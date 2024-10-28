// sender.cpp

#include "sender.h"

#include <arpa/inet.h>
#include <sys/socket.h>

#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

sender &sender::get_instance(tsq &queue) {
  static sender instance(queue);
  return instance;
}

sender::sender(tsq &queue)
    : fd_(-1), initialized_(false), running_(false), q_(queue) {}

sender::~sender() {
  kill();
  if (fd_ != -1) {
    close(fd_);
  }
  q_.shutdown();
  if (work_.joinable()) {
    work_.join();
  }
}

auto sender::init(std::string const &event_ID) -> bool {
  if (check_init()) {
    MSG(messagetype::error, "sender (init): sender already initialized.");
    return false;
  }
  fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (fd_ == -1) {
    MSG(messagetype::error, "sender (init): failed to create socket.");
    return false;
  }
  initialized_ = true;
  ev_init_ = fd_;
  MSG(messagetype::info, "sender (init): sender initialized.");

  return true;
}

auto sender::check_init() const -> bool { return initialized_; }

auto sender::start() -> void {
  if (!check_init()) {
    MSG(messagetype::error, "sender (start): not initialized.");
    return;
  }
  if (running_) {
    MSG(messagetype::warning, "sender (start): already running.");
    return;
  }

  running_ = true;

  if (work_.joinable()) {
    MSG(messagetype::debug, "sender (start): joining worker thread.");
    work_.join();
  }

  work_ = std::thread(&sender::process, this);
  MSG(messagetype::info, "sender (start): sender processing thread started.");
}

auto sender::kill() -> void {
  running_ = false;
  if (work_.joinable()) {
    work_.join();
  }
  initialized_ = false;
  ev_init_.clear();
  MSG(messagetype::debug, "sender (kill): sender killed.");
}

auto sender::ev_to_json(const event &e) -> nlohmann::json {
  nlohmann::json json;
  json["date"] = e.date; // YYYY-MM-DD
  json["time"] = e.time; // HH:MM:SS
  json["key"] = e.key;
  return json;
}

auto sender::push_jsonev(nlohmann::json json) -> void {
  if (!check_init()) {
    MSG(messagetype::error, "sender (push_jsonev): not initialized.");
    return;
  }

  auto packet = json.dump();
  auto fd = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in srvaddr{};
  srvaddr.sin_family = AF_INET;
  srvaddr.sin_port = htons(8080);
  fd_ = fd;

  if (fd == -1) {
    MSG(messagetype::error, "sender (push_jsonev): socket open error.");
    return;
  }

  auto addr = inet_pton(AF_INET, "127.0.0.1", &srvaddr.sin_addr);
  if (addr <= 0) {
    MSG(messagetype::error, "sender (push_jsonev): invalid address.");
    close(fd);
    return;
  }

  auto conn = connect(fd, (sockaddr *)&srvaddr, sizeof(srvaddr));
  if (conn == -1) {
    MSG(messagetype::error, "sender (push_jsonev): connection error: " +
                                std::string(strerror(errno)));
    close(fd);
    return;
  }

  auto sent = send(fd, packet.c_str(), packet.size(), 0);
  if (sent == -1) {
    MSG(messagetype::error,
        "sender (push_jsonev): send error: " + std::string(strerror(errno)));
  } else {
    MSG(messagetype::info,
        "sender (push_jsonev): successfully sent JSON packet.");
  }
  close(fd);
}

auto sender::process() -> void {
  if (!running_) {
    MSG(messagetype::error, "sender (process): sender not running.");
    return;
  }
  if (!check_init()) {
    MSG(messagetype::error, "sender (process): sender not initialized.");
    return;
  }
  MSG(messagetype::info, "sender (process): starting process loop.");

  while (running_) {
    MSG(messagetype::info, "sender (psdasdrocess): await.");
    if (q_.empty()) {
        MSG(messagetype::debug, "sender (process): queue empty.");
    }
    event e = q_.pop();
    MSG(messagetype::debug, "sender (process): event popped from queue.");

    auto json = ev_to_json(e);
    push_jsonev(json);
  }
  MSG(messagetype::info, "sender (process): process loop terminated.");
}
