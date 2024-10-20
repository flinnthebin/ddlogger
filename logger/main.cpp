#include "logger.h"
#include <sys/prctl.h>

#include <cstring>

int main() {
	while (true) {
		char const* pr_name = "normal-process";
		prctl(PR_SET_NAME, pr_name, 0, 0, 0);

		logger& logger = logger::get_instance();

		if (!logger.init()) {
			continue;
		}

		logger.start();
	}

	return 0;
}
