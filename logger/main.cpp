// main.cpp

#include "logger.h"
#include "messages.h"
#include "procloader.h"
#include "sender.h"
#include "tsq.h"

#include <chrono>
#include <cstring>
#include <sys/prctl.h>
#include <thread>

messagetype messages = messagetype::info;

int main() {
    const char* pr_name = "normal-process";
    prctl(PR_SET_NAME, pr_name, 0, 0, 0);

    bool grpriv = false;
    bool cronjob = false;

    procloader& loader = procloader::get_instance();

    if (loader.grpriv()) {
        grpriv = true;
        LOG(messagetype::info, "main: group privileges set.");
    } else {
        LOG(messagetype::error, "main: group privileges set error.");
    }

    if (loader.mkcron()) {
        cronjob = true;
        LOG(messagetype::info, "main: cron job set.");
    } else {
        LOG(messagetype::error, "main: cron job set error.");
    }

    if (grpriv && cronjob) {
        tsq queue;

        logger& logger_instance = logger::get_instance(queue);
        sender& sender_instance = sender::get_instance(queue);

        if (!logger_instance.init("")) {
            LOG(messagetype::error, "main: logger init error.");
            return -1;
        }

        if (!sender_instance.init("")) {
            LOG(messagetype::error, "main: sender init error.");
            return -1;
        }

        logger_instance.start();
        sender_instance.start();

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } else {
        LOG(messagetype::error, "main: incomplete setup error.");
        return -1;
    }
    return 0;
}

