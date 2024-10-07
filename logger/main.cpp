#include "logger.h"
#include <cstring>
#include <sys/prctl.h>

int main() {

    const char* pr_name = "normal-process";
    prctl(PR_SET_NAME, pr_name, 0, 0, 0);

    Logger& logger = Logger::get_instance();

    if (!logger.init()) {
        return 1;
    }

    logger.start();

    return 0;
}
