// main.cpp

#include "tsq.h"
#include "messages.h"
#include "logger.h"
#include "sender.h"
#include <sys/prctl.h>

#include <cstring>

messagetype messages = messagetype::debug;

int main() {
    messages = messagetype::none;

    char const* pr_name = "normal-process";
    prctl(PR_SET_NAME, pr_name, 0, 0, 0);

    tsq queue;

    logger& logger = logger::get_instance(queue);
    sender& sender = sender::get_instance(queue);

    if (!logger.init("")) {
        return -1;
    }

    if (!sender.init("")) {
        return -1;
    }

    logger.start();
    sender.start();

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
	return 0;
}
