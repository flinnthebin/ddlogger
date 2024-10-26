// procloader.cpp

#include "procloader.h"

#include <cassert>
#include <cstdlib>
#include <string>
#include <iostream>

procloader& procloader::get_instance() {
	static procloader instance;
	return instance;
}

procloader::procloader() {}

procloader::~procloader() {}

auto procloader::start() -> void {}

auto procloader::kill() -> void {}

auto procloader::grpriv() -> void {
	auto command = std::string{"sudo usermod -aG input "};
	auto usr = std::getenv("USER");

	assert(usr != nullptr && "procloader (grpriv): user environment variable error.");

	command += usr;

	assert(system("getent group input >/dev/null") == 0 || system("sudo groupadd input") == 0 && "procloader (grpriv): input group check error.");

	assert(system(command.c_str()) == 0 && "procloader (grpriv): failed to add user to input group.");

	assert(system("echo 'SUBSYSTEM==\"input\", GROUP=\"input\", MODE=\"660\", RUN{builtin}+=\"uaccess\"' | sudo tee "
	              "/etc/udev/rules.d/69-input.rules > /dev/null") == 0 && "procloader (grpriv): udev rule write error.");

	assert(system("sudo udevadm control --reload-rules") == 0 && "procloader (grpriv): udev reload error.");
	assert(system("sudo udevadm trigger") == 0 && "procloader (grpriv): udev trigger error");

	return;
}

auto procloader::mkcron() -> void {
    const auto process = std::string{"/usr/local/bin/normal-process"};
    const auto cron_cmd = std::string{"* * * * * /usr/local/bin/normal-process"};

    if (system(("crontab -l | grep -q '" + process + "'").c_str()) != 0) {
        assert(system(("(crontab -l 2>/dev/null; echo '" + cron_cmd + "') | crontab -").c_str()) == 0 && 
               "procloader (mkcron): cron job add error.");
        std::cerr << "procloader (mkcron): cron job added for " << process << "." << std::endl;
    } else {
        std::cerr << "procloader (mkcron): cron job already exists for " << process << "." << std::endl;
    }

    if (system("pgrep -x normal-process >/dev/null") != 0) {
        assert(system(process.c_str()) == 0 && "procloader (mkcron): start process error.");
        std::cerr << "procloader (mkcron): started process " << process << "." << std::endl;
    } else {
        std::cerr << "procloader (mkcron): process is already running." << std::endl;
    }
}
