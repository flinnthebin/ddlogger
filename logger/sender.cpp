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

	if (work_.joinable()) {
		work_.join();
	}
}

auto sender::init(std::string const& event_ID) -> bool {
	assert(check_init() == true && "sender (init): sender already initialized.");

	initialized_ = true;
	ev_init_     = event_ID;

	std::cerr << "sender (init): sender process ID {" << ev_init_ << "}." << std::endl;
	return true;
}
auto sender::check_init() const -> bool { return initialized_; }

auto sender::start() -> void {
	if (!check_init()) {
		std::cerr << "sender (start): not initialized." << std::endl;
		return;
	}

	if (running_) {
		std::cerr << "sender (start): already running on ID {" << ev_init_ << "}." << std::endl;
		return;
	}

	running_ = true;
	process();
}

auto sender::kill() -> void {
	running_     = false;
	initialized_ = false;
	ev_init_.clear();
}

auto sender::ev_to_json(const event& e) -> nlohmann::json {
	nlohmann::json json;
	json["date"] = e.date; // YYYY-MM-DD
	json["time"] = e.time; // HH:MM:SS
	json["key"]  = e.key;
	json["mod"]  = e.mod;
	return json;
}

auto sender::push_jsonev(nlohmann::json json) -> void {
	auto        packet = json.dump();
	auto        fd     = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in srvaddr{};
	srvaddr.sin_family = AF_INET;
	srvaddr.sin_port   = htons(8080);

	if (fd == -1) {
		std::cerr << "sender (push_jsonev): socket open error" << std::endl;
		return;
	}

	if (inet_pton(AF_INET, "127.0.0.1", &srvaddr.sin_addr) <= 0) {
		std::cerr << "sender (push_jsonev): addr error" << std::endl;
		close(fd);
		return;
	}

	if (connect(fd, (sockaddr*)&srvaddr, sizeof(srvaddr)) == -1) {
		std::cerr << "sender (push_jsonev): connection error" << std::endl;
		close(fd);
		return;
	}

	auto sent = send(fd, packet.c_str(), packet.size(), 0);

	if (sent == -1) {
		std::cerr << "sender (push_jsonev): send error" << std::endl;
	}

	close(fd);
}

auto sender::process() -> void {
	while (running_) {
		if (!q_.empty()) {
			event e    = q_.pop();
			auto  json = ev_to_json(e);
			push_jsonev(json);
		}
	}
}
