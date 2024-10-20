// logger.cpp

#include "logger.h"

#include <linux/input-event-codes.h>
#include <nlohmann/json.hpp>
#include <sys/stat.h>
#include <sys/types.h>

#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <utility>

Logger& Logger::get_instance() {
	static Logger instance;
	return instance;
}

Logger::Logger()
: fd_(-1)
, initialized_(false)
, running_(false)
, keymap_(load_keymap("keymap.json")) {
	return;
}

Logger::~Logger() {
	if (fd_ != -1) {
		close(fd_);
	}
}

auto Logger::init(std::string const& event_ID) -> bool {
	if (check_init()) {
		std::cerr << "logger (init): logger process ID {" << ev_init_ << "}." << std::endl;
		return true;
	}

	std::string device = event_ID;
	if (device.empty()) {
		device = find_kbd();
		if (device.empty()) {
			std::cerr << "logger (init): no viable keyboard device." << std::endl;
			return false;
		}
	}

	fd_ = open(device.c_str(), O_RDONLY | O_NONBLOCK);
	if (fd_ == -1) {
		std::cerr << "logger (init) open error: " << device << ": " << strerror(errno) << std::endl;
		return false;
	}

	initialized_ = true;
	ev_init_     = device;
	return true;
}

auto Logger::check_init() const -> bool { return initialized_; }

auto Logger::start() -> void {
	if (!check_init()) {
		std::cerr << "logger (start): not initialized." << std::endl;
		return;
	}

	if (running_) {
		std::cerr << "logger (start): already running on ID {" << ev_init_ << "}." << std::endl;
		return;
	}

	running_ = true;
	ev_reader();
}

auto Logger::kill() -> void {
	running_ = false;
	if (fd_ != -1) {
		close(fd_);
		fd_ = -1;
	}
	initialized_ = false;
	ev_init_.clear();
}

auto Logger::
  load_keymap(const std::string& config) -> std::unordered_map<unsigned int, std::pair<std::string, std::string>> {
	std::unordered_map<unsigned int, std::pair<std::string, std::string>> tmp;
	std::ifstream                                                         conf(config);

	if (!conf.is_open()) {
		std::cerr << "logger (constructor): keymap config error" << std::endl;
	}

	nlohmann::json keyconfig;
	conf >> keyconfig;

	for (auto& [key, val] : keyconfig.items()) {
		auto code = std::stoi(key);
		auto name = std::string{val[0]};
		auto ch   = std::string{val[1]};
		tmp.emplace(code, std::make_pair(name, ch));
	}

	return tmp;
}

template<class _Rep, class _Period>
auto async_timer(std::chrono::duration<_Rep, _Period> duration, std::function<void()> callback) -> std::future<void> {
	return std::async(std::launch::async, [duration, callback]() {
		std::this_thread::sleep_for(duration);
		callback();
	});
}

auto Logger::fd_monitor(signed int fd, fd_set fds) -> signed int {
	struct timeval timeout;
	timeout.tv_sec  = 1;
	timeout.tv_usec = 0;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	return select(fd + 1, &fds, nullptr, nullptr, &timeout);
}

auto Logger::datetime(time_t tv_sec) -> std::pair<std::string, std::string> {
	char date[11], time[13];

	std::tm* tv = std::localtime(&tv_sec);
	std::strftime(date, sizeof(date), "%Y-%m-%d", tv);
	std::strftime(time, sizeof(time), "%H:%M:%S", tv);

	return std::make_pair(std::string{date}, std::string{time});
}

auto Logger::find_kbd() -> std::string {
	std::string       device_path;
	const std::string hardware_path = "/dev/input/by-id";
	const std::string kbd_id        = "-event-kbd";

	std::filesystem::path const input_dir(hardware_path);

	if (!std::filesystem::exists(input_dir) || !std::filesystem::is_directory(input_dir)) {
		std::cerr << "logger (kbd) bad input directory: " << input_dir << std::endl;
		return device_path;
	}

	for (auto const& file : std::filesystem::directory_iterator(input_dir)) {
		if (file.is_symlink() || file.is_character_file()) {
			auto const& path = file.path();
			if (path.filename().string().find(kbd_id) != std::string::npos) {
				std::error_code       ec;
				std::filesystem::path resolved_path = std::filesystem::canonical(path, ec);
				if (ec) {
					std::cerr << "logger (kbd) symlink resolution error: " << ec.message() << std::endl;
					continue;
				}

				auto fd = open(resolved_path.c_str(), O_RDONLY | O_NONBLOCK);
				if (fd == -1) {
					std::cerr << "logger (kbd) open error: " << resolved_path << ": " << strerror(errno) << std::endl;
					continue;
				}

				fd_set fds;
				auto   retval = fd_monitor(fd, fds);

				if (retval == -1) {
					std::cerr << "logger (kbd) select error: " << strerror(errno) << std::endl;
					close(fd);
					continue;
				}
				else if (retval == 0) {
					std::cerr << "logger (kbd) timeout error: " << resolved_path << std::endl;
					close(fd);
					continue;
				}

				if (FD_ISSET(fd, &fds)) {
					input_event ev;
					ssize_t     n = read(fd, &ev, sizeof(ev));

					if (n == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
						std::cerr << "logger (kbd) read error: " << resolved_path << ": " << strerror(errno) << std::endl;
						close(fd);
						continue;
					}

					close(fd);
					device_path = resolved_path.string();
					std::cout << "logger (kbd) keyboard found: " << device_path << std::endl;
					break;
				}
			}
		}
	}
	if (device_path.empty()) {
		std::cerr << "logger (kbd): no keyboard found." << std::endl;
	}

	return device_path;
}

auto Logger::ev_reader() -> void {
	struct input_event                  ev;
	ssize_t                             n;
	std::pair<std::string, std::string> dtg;

	while (running_) {
		n = read(fd_, &ev, sizeof(ev));
		if (n == (ssize_t)sizeof(ev)) {
			if (ev.type == EV_KEY) {
				std::string key_name = get_keychar(ev.code);
				dtg                  = datetime(ev.time.tv_sec);
				std::cout
				  << dtg.first << " " << dtg.second << " logger (ev): " << key_name << " (" << ev.code << ") "
				  << (ev.value ? "pressed" : "released") << std::endl;
			}
		}
		else if (n == -1) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				std::cerr << "logger (ev) read erorr: " << strerror(errno) << std::endl;
				break;
			}

			fd_set fds;
			auto   retval = fd_monitor(fd_, fds);
			if (retval == -1) {
				std::cerr << "logger (ev) select error: " << strerror(errno) << std::endl;
				break;
			}
		}
	}
}

auto Logger::get_keychar(unsigned int code) -> std::string {
	auto it = keymap_.find(code);
	if (it != keymap_.end()) {
		return it->second.second.data();
	}
	else {
		return "unknown";
	}
}
