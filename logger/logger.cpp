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

logger::~logger() {
	if (fd_ != -1) {
		close(fd_);
	}
}

auto logger::init(const std::string& event_ID) -> bool {
	assert(!check_init() && "logger (init): logger already initialized.");

	auto const device = std::string{event_ID.empty() ? find_kbd() : event_ID};
	assert(!device.empty() && "logger (init): no viable keyboard device found.");

	fd_ = open(device.c_str(), O_RDONLY | O_NONBLOCK);
	assert(fd_ != -1 && "logger (init): failed to open device.");

	initialized_ = true;
	ev_init_     = device;
	return true;
}

auto logger::check_init() const -> bool { return initialized_; }

auto logger::start() -> void {
	assert(check_init() && "logger (start): logger not initialized.");
	assert(!running_ && "logger (start): logger already running.");

	running_ = true;

	if (work_.joinable()) {
		work_.join();
	}

	work_ = std::thread(&logger::ev_reader, this);
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
}

auto logger::
  load_keymap(const std::string& config) -> std::unordered_map<unsigned int, std::pair<std::string, std::string>> {
	auto tmp  = std::unordered_map<unsigned int, std::pair<std::string, std::string>>{};
	auto conf = std::ifstream(config);

	assert(conf.is_open() && "logger (load_keymap): keymap config error.");

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
		std::cerr << "logger (fd_monitor): select error: " << strerror(errno) << std::endl;
		assert(retval != -1 && "logger (fd_monitor): select error.");
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
	assert(std::filesystem::exists(dev_input) && "logger (find_kbd): /dev/input directory not found.");
	assert(std::filesystem::is_directory(dev_input) && "logger (find_kbd): /dev/input is not a directory.");

	for (const auto& event_num : std::filesystem::directory_iterator(dev_input)) {
		auto const& path = event_num.path();
		if (path.filename().string().find("event13") == 0) {
			auto const resolved_path = path.string();
			std::cerr << "logger (find_kbd): attempting device " << resolved_path << std::endl;

			auto fd = open(resolved_path.c_str(), O_RDONLY | O_NONBLOCK);
			if (fd == -1) {
				std::cerr << "logger (find_kbd): fd = -1" << std::endl;
				continue;
			}

			fd_set fds;
			FD_ZERO(&fds);
			auto retval = fd_monitor(fd, fds);
			if (retval > 0 && FD_ISSET(fd, &fds)) {
				close(fd);
				std::cerr << "logger (find_kbd): found working input device at " << resolved_path << std::endl;
				return resolved_path;
			}
			std::cerr << "logger (find_kbd): device did not respond as expected: " << resolved_path << std::endl;
			close(fd);
		}
	}
	assert(false && "logger (find_kbd): no viable keyboard device found.");
	return "";
}

auto logger::ev_reader() -> void {
	auto ev = input_event{};
	while (running_) {
		fd_set fds;
		auto   retval = fd_monitor(fd_, fds);
		assert(retval != -1 && "logger (ev_reader): select error.");

		if (retval > 0 && FD_ISSET(fd_, &fds)) {
			auto n = read(fd_, &ev, sizeof(ev));

			if (n == -1) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					continue;
				}
				else {
					assert(false && "logger (ev_reader): read error.");
				}
			}

			if (n == static_cast<ssize_t>(sizeof(ev)) && ev.type == EV_KEY) {
				auto  dtg = datetime(ev.time.tv_sec);
				event e{dtg.first, dtg.second, get_keychar(ev.code), ev.value ? true : false};
				if (ev.value == true) {
					q_.push(e);
				}
			}
		}
	}
}

auto logger::get_keychar(unsigned int code) -> std::string {
	auto it = keymap_.find(code);
	assert(it != keymap_.end() && "logger (get_keychar): unknown key code.");
	return it->second.second;
}
