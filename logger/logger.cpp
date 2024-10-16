// logger.cpp

#include "logger.h"
#include <linux/input-event-codes.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <unistd.h>

Logger& Logger::get_instance() {
	static Logger instance;
	return instance;
}

Logger::Logger()
: fd_(-1)
, initialized_(false)
, running_(false) {
	return;
}

Logger::~Logger() {
	if (fd_ != -1) {
		close(fd_);
	}
}

auto Logger::init(std::string const& event_ID) -> bool {
	if (check_init()) {
		std::cerr << "logger (init): logger is already initialized to ID {" << ev_init_ << "}." << std::endl;
		return true;
	}

	std::string device = event_ID;
	if (device.empty()) {
		device = find_kbd();
		if (device.empty()) {
			std::cerr << "logger (init): failed to find keyboard." << std::endl;
			return false;
		}
	}

	fd_ = open(device.c_str(), O_RDONLY | O_NONBLOCK);
	if (fd_ == -1) {
		std::cerr << "logger (init) handshake error: " << device << ": " << strerror(errno) << std::endl;
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

auto Logger::find_kbd() -> std::string {
	std::string device_path;

	std::filesystem::path const input_dir("/dev/input/by-id");

	if (!std::filesystem::exists(input_dir) || !std::filesystem::is_directory(input_dir)) {
		std::cerr << "logger (kbd) bad input directory: " << input_dir << std::endl;
		return device_path;
	}

	for (auto const& file : std::filesystem::directory_iterator(input_dir)) {
		if (file.is_symlink() || file.is_character_file()) {
			auto const& path = file.path();
			if (path.filename().string().find("-event-kbd") != std::string::npos) {
				std::error_code       ec;
				std::filesystem::path resolved_path = std::filesystem::canonical(path, ec);
				if (ec) {
					std::cerr << "logger (kbd) failed to resolve symlink: " << ec.message() << std::endl;
					continue;
				}

				int fd = open(resolved_path.c_str(), O_RDONLY | O_NONBLOCK);
				if (fd == -1) {
					std::cerr << "logger (kbd) failed to open device: " << resolved_path << ": " << strerror(errno)
					          << std::endl;
					continue;
				}

				fd_set fds;
				FD_ZERO(&fds);
				FD_SET(fd, &fds);

				// Set a timeout for select
				struct timeval timeout;
				timeout.tv_sec  = 1; // 1 second timeout
				timeout.tv_usec = 0;

				int ret = select(fd + 1, &fds, nullptr, nullptr, &timeout);
				if (ret == -1) {
					std::cerr << "logger (kbd) select error: " << strerror(errno) << std::endl;
					close(fd);
					continue;
				}
				else if (ret == 0) {
					// Timeout occurred, no input detected
					std::cerr << "logger (kbd) no input detected for device: " << resolved_path << std::endl;
					close(fd);
					continue;
				}

				if (FD_ISSET(fd, &fds)) {
					// If select returns and the fd is set, read the event
					input_event ev;
					ssize_t     n = read(fd, &ev, sizeof(ev));

					if (n == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
						std::cerr << "logger (kbd) error reading from device: " << resolved_path << ": "
						          << strerror(errno) << std::endl;
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
	struct input_event ev;
	ssize_t            n;

	while (running_) {
		n = read(fd_, &ev, sizeof(ev));
		if (n == (ssize_t)sizeof(ev)) {
			if (ev.type == EV_KEY) {
				char const* key_name = get_keychar(ev.code);
				std::cout << "logger (ev): " << key_name << " (" << ev.code << ") "
				          << (ev.value ? "pressed" : "released") << std::endl;
			}
		}
		else if (n == -1) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				std::cerr << "logger (ev) input event erorr: " << strerror(errno) << std::endl;
				break;
			}
			// If no data available, wait for input using select()
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(fd_, &fds);

			int ret = select(fd_ + 1, &fds, nullptr, nullptr, nullptr);
			if (ret == -1) {
				std::cerr << "logger (ev) select error: " << strerror(errno) << std::endl;
				break;
			}
		}
	}
}

auto Logger::get_keychar(unsigned int code) -> char const* {
	auto it = keymap_.find(code);
	if (it != keymap_.end()) {
		return it->second.c_str();
	}
	else {
		return "unknown";
	}
}
