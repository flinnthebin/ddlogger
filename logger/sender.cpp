// sender.cpp

#include "sender.h"

#include <arpa/inet.h>
#include <sys/socket.h>

#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

sender& sender::get_instance() {
	static sender instance;
	return instance;
}

sender::sender()
: q_()
, initialized_(false)
, running_(false) {
	work_ = std::thread(&sender::process, this);
}

sender::~sender() {
	q_.shutdown();
    assert(work_.joinable() && "sender (destructor): work thread not joinable.");
	if (work_.joinable()) {
		work_.join();
	}
}

auto sender::init(std::string const& event_ID) -> bool {
	assert(!check_init() && "sender (init): sender already initialized.");

	initialized_ = true;
	ev_init_     = event_ID;

	std::cerr << "sender (init): sender process ID {" << ev_init_ << "}." << std::endl;
	return true;
}

auto sender::check_init() const -> bool { return initialized_; }

auto sender::start() -> void {
    assert(check_init() && "sender (start): not initialized.");
    assert(!running_ && "sender (start): already running.");

	running_ = true;
	process();
}

auto sender::kill() -> void {
	running_     = false;
	initialized_ = false;
	ev_init_.clear();
}

auto sender::ev_to_json(const event& e) -> nlohmann::json {
    assert(!e.date.empty() && "sender (ev_to_json): date is empty.");
    assert(!e.time.empty() && "sender (ev_to_json): time is empty.");
    assert(!e.key.empty() && "sender (ev_to_json): key is empty.");
	
    nlohmann::json json;
	json["date"] = e.date; // YYYY-MM-DD
	json["time"] = e.time; // HH:MM:SS
	json["key"]  = e.key;
	json["mod"]  = e.mod;
	return json;
}

auto sender::push_jsonev(nlohmann::json json) -> void {
    assert(check_init() && "sender (push_jsonev): not initialized.");

	auto        packet = json.dump();
	auto        fd     = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in srvaddr{};
	srvaddr.sin_family = AF_INET;
	srvaddr.sin_port   = htons(8080);

    assert(fd != -1 && "sender (push_jsonev): socket open error.");
    if (fd == -1) return;

    auto addr = inet_pton(AF_INET, "127.0.0.1", &srvaddr.sin_addr);
    auto conn = connect(fd, (sockaddr*)&srvaddr, sizeof(srvaddr));
    auto sent = send(fd, packet.c_str(), packet.size(), 0);

    assert(addr > 0 && "sender (push_jsonev): addr error.");
    if (addr <= 0) {
        close(fd);
        return;
    }

    assert(conn != -1 && "sender (push_jsonev): connection error.");
    if (conn == -1) {
        close(fd);
        return;
    }

    assert(sent != -1 && "sender (push_jsonev): send error.");
    close(fd);
}

auto sender::process() -> void {
    assert(running_ && "sender (process): sender not running.");
    assert(check_init() && "sender (process): sender not initialized.");
	while (running_) {
		if (!q_.empty()) {
			event e    = q_.pop();
			auto  json = ev_to_json(e);
			push_jsonev(json);
		}
	}
}
