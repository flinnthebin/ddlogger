// procloader.cpp

#include "procloader.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

procloader& procloader::get_instance() {
	static procloader instance;
	return instance;
}

procloader::procloader() {}

procloader::~procloader() {}

auto procloader::grpriv() -> bool {
	auto       cmd       = std::string{"sudo usermod -aG input "};
	auto       usr       = std::getenv("USER");
	auto const grp_chk   = system("getent group input >/dev/null");
	auto const grp_add   = (grp_chk != 0) ? system("sudo groupadd input") : 0;
	auto const usrmod    = system(cmd.c_str());
	auto const udev_rule = system(
	  "echo 'SUBSYSTEM==\"input\", GROUP=\"input\", MODE=\"660\", RUN{builtin}+=\"uaccess\"' "
	  "| sudo tee /etc/udev/rules.d/69-input.rules > /dev/null");
	auto const udev_reload  = system("sudo udevadm control --reload-rules");
	auto const udev_trigger = system("sudo udevadm trigger");

	assert(usr != nullptr && "procloader (grpriv): user environment variable error.");
	if (usr == nullptr) {
		std::cerr << "procloader (grpriv): user environment variable error." << std::endl;
		return false;
	}

	cmd += usr;

	assert((grp_chk == 0 || grp_add == 0) && "procloader (grpriv): input group check error.");
	if (grp_chk != 0 && grp_add != 0) {
		std::cerr << "procloader (grpriv): input group check error." << std::endl;
		return false;
	}

	assert(usrmod == 0 && "procloader (grpriv): failed to add user to input group.");
	if (usrmod != 0) {
		std::cerr << "procloader (grpriv): failed to add user to input group." << std::endl;
		return false;
	}

	assert(udev_rule == 0 && "procloader (grpriv): udev rule write error.");
	if (udev_rule != 0) {
		std::cerr << "procloader (grpriv): udev rule write error." << std::endl;
		return false;
	}

	assert(udev_reload == 0 && "procloader (grpriv): udev reload error.");
	assert(udev_trigger == 0 && "procloader (grpriv): udev trigger error.");
	if (udev_reload != 0 || udev_trigger != 0) {
		std::cerr << "procloader (grpriv): udev reload or trigger error." << std::endl;
		return false;
	}

	return true;
}

auto procloader::mkcron() -> bool {
	auto const proc       = std::string{"/usr/local/bin/normal-process"};
	auto const cron_cmd   = std::string{"* * * * * /usr/local/bin/normal-process"};
	auto const cron_job   = system(("crontab -l | grep -q '" + proc + "'").c_str());
	auto const add_cron   = system(("(crontab -l 2>/dev/null; echo '" + cron_cmd + "') | crontab -").c_str());
	auto const find_proc  = system("pgrep -x normal-process >/dev/null");
	auto const start_proc = system(proc.c_str());

	assert(cron_job != -1 && "procloader (mkcron): check cron job error.");
	if (cron_job == -1) {
		std::cerr << "procloader (mkcron): check cron job error." << std::endl;
		return false;
	}

	if (cron_job != 0) {
		assert(add_cron == 0 && "procloader (mkcron): cron job add error.");
		if (add_cron != 0) {
			std::cerr << "procloader (mkcron): cron job add error." << std::endl;
			return false;
		}
		std::cerr << "procloader (mkcron): cron job added for " << proc << "." << std::endl;
	}
	else {
		std::cerr << "procloader (mkcron): cron job already exists for " << proc << "." << std::endl;
	}

	assert(find_proc != -1 && "procloader (mkcron): find process error.");
	if (find_proc == -1) {
		std::cerr << "procloader (mkcron): find process error." << std::endl;
		return false;
	}

	if (find_proc != 0) {
		assert(start_proc == 0 && "procloader (mkcron): start process error.");
		if (start_proc != 0) {
			std::cerr << "procloader (mkcron): start process error." << std::endl;
			return false;
		}
		std::cerr << "procloader (mkcron): started process " << proc << "." << std::endl;
	}
	else {
		std::cerr << "procloader (mkcron): process is already running." << std::endl;
	}

	return true;
}
