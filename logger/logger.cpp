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
					// If select returns and the fd is set, read the event
					// change to send struct to datastore
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
	struct input_event ev;
	ssize_t            n;

	while (running_) {
		n = read(fd_, &ev, sizeof(ev));
		if (n == (ssize_t)sizeof(ev)) {
			if (ev.type == EV_KEY) {
				std::string key_name = get_keychar(ev.code);
				std::cout
				  << "logger (ev): " << key_name << " (" << ev.code << ") " << (ev.value ? "pressed" : "released") << std::endl;
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
	std::cout << "Looking for key code: " << code << std::endl;
	auto it = keymap_.find(code);
	if (it != keymap_.end()) {
		return it->second.first;
	}
	else {
		return "unknown";
	}
}

const std::unordered_map<unsigned int, std::pair<std::string, std::string>> keymap_ = {
  {0,  {"KEY_RESERVED", ""}           },
  {1,  {"KEY_ESC", "ESC"}             },
  {2,  {"KEY_1", "1"}                 },
  {3,  {"KEY_2", "2"}                 },
  {4,  {"KEY_3", "3"}                 },
  {5,  {"KEY_4", "4"}                 },
  {6,  {"KEY_5", "5"}                 },
  {7,  {"KEY_6", "6"}                 },
  {8,  {"KEY_7", "7"}                 },
  {9,  {"KEY_8", "8"}                 },
  {10, {"KEY_9", "9"}                 },
  {11, {"KEY_0", "0"}                 },
  {12, {"KEY_MINUS", "-"}             },
  {13, {"KEY_EQUALS", "="}            },
  {14, {"KEY_BACKSPACE", "BACKSPACE"} },
  {15, {"KEY_TAB", "TAB"}             },
  {16, {"KEY_Q", "Q"}                 },
  {17, {"KEY_W", "W"}                 },
  {18, {"KEY_E", "E"}                 },
  {19, {"KEY_R", "R"}                 },
  {20, {"KEY_T", "T"}                 },
  {21, {"KEY_Y", "Y"}                 },
  {22, {"KEY_U", "U"}                 },
  {23, {"KEY_I", "I"}                 },
  {24, {"KEY_O", "O"}                 },
  {25, {"KEY_P", "P"}                 },
  {26, {"KEY_LEFT", "["}              },
  {27, {"KEY_RIGHT", "]"}             },
  {28, {"KEY_ENTER", "ENTER"}         },
  {29, {"KEY_LEFT", "CTRL"}           },
  {30, {"KEY_A", "A"}                 },
  {31, {"KEY_S", "S"}                 },
  {32, {"KEY_D", "D"}                 },
  {33, {"KEY_F", "F"}                 },
  {34, {"KEY_G", "G"}                 },
  {35, {"KEY_H", "H"}                 },
  {36, {"KEY_J", "J"}                 },
  {37, {"KEY_K", "K"}                 },
  {38, {"KEY_L", "L"}                 },
  {39, {"KEY_SEMICOLON", ";"}         },
  {40, {"KEY_APOSTROPHE", "'"}        },
  {41, {"KEY_GRAVE", "`"}             },
  {42, {"KEY_LEFTSHIFT", "SHIFT"}     },
  {43, {"KEY_BACKSPACE", "\\"}        },
  {44, {"KEY_Z", "Z"}                 },
  {45, {"KEY_X", "X"}                 },
  {46, {"KEY_C", "C"}                 },
  {47, {"KEY_V", "V"}                 },
  {48, {"KEY_B", "B"}                 },
  {49, {"KEY_N", "N"}                 },
  {50, {"KEY_M", "M"}                 },
  {51, {"KEY_COMMA", ","}             },
  {52, {"KEY_DOT", "."}               },
  {53, {"KEY_SLASH", "/"}             },
  {54, {"KEY_RIGHTSHIFT", "SHIFT"}    },
  {55, {"KEY_KPASTERISK", "*"}        },
  {56, {"KEY_LEFTALT", "ALT"}         },
  {57, {"KEY_SPACE", "SPACE"}         },
  {58, {"KEY_CAPSLOCK", "CAPSLOCK"}   },
  {59, {"KEY_F1", "F1"}               },
  {60, {"KEY_F2", "F2"}               },
  {61, {"KEY_F3", "F3"}               },
  {62, {"KEY_F4", "F4"}               },
  {63, {"KEY_F5", "F5"}               },
  {64, {"KEY_F6", "F6"}               },
  {65, {"KEY_F7", "F7"}               },
  {66, {"KEY_F8", "F8"}               },
  {67, {"KEY_F9", "F9"}               },
  {68, {"KEY_F10", "F10"}             },
  {69, {"KEY_NUMLOCK", "NUMLOCK"}     },
  {70, {"KEY_SCROLLOCK", "SCROLLLOCK"}},
  {71, {"KEY_KP7", "KP7"}             },
  {72, {"KEY_KP8", "KP8"}             },
  {73, {"KEY_KP9", "KP9"}             },
  {74, {"KEY_KPMINUS", "-"}           },
  {75, {"KEY_KP4", "KP4"}             },
  {76, {"KEY_KP5", "KP5"}             },
  {77, {"KEY_KP6", "KP6"}             },
  {78, {"KEY_KPPLUS", "+"}            },
  {79, {"KEY_KP1", "KP1"}             },
  {80, {"KEY_KP2", "KP2"}             },
  {81, {"KEY_KP3", "KP3"}             },
  {82, {"KEY_KP0", "KP0"}             },
  {83, {"KEY_KPDOT", "."}             },
};
