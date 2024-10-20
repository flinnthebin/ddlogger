// sender.cpp

#include "sender.h"

#include <thread>
#include <string>
#include <iostream>

sender& sender::get_instance() {
	static sender instance;
	return instance;
}

sender::sender()
: q_()
, initialized_(false)
, running_(false) {
	work_ = std::thread(&sender::send, this);
}

sender::~sender() {
	 q_.shutdown();

    if (work_.joinable()) {
        work_.join();
    }
}

auto sender::init(std::string const& event_ID) -> bool {
	if (check_init()) {
		std::cerr << "sender (init): sender process ID {" << ev_init_ << "}." << std::endl;
        return false;
	}

	initialized_ = true;
    ev_init_ = event_ID;
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
	send();
}

auto sender::kill() -> void {
    	running_ = false;
	initialized_ = false;
	ev_init_.clear();
}

auto send() -> void;


