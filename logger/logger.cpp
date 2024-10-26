// logger.cpp

#include "logger.h"

#include <linux/input-event-codes.h>
#include <nlohmann/json.hpp>
#include <sys/stat.h>
#include <sys/types.h>

#include <cassert>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <utility>

logger& logger::get_instance() {
    static logger instance;
    return instance;
}

logger::logger()
    : fd_(-1), initialized_(false), running_(false), keymap_(load_keymap("keymap.json")) {}

logger::~logger() {
    if (fd_ != -1) {
        close(fd_);
    }
}

auto logger::init(const std::string& event_ID) -> bool {
    assert(!check_init() && "logger (init): logger already initialized.");

    auto device = std::string{event_ID.empty() ? find_kbd() : event_ID};
    assert(!device.empty() && "logger (init): no viable keyboard device found.");

    fd_ = open(device.c_str(), O_RDONLY | O_NONBLOCK);
    assert(fd_ != -1 && "logger (init): failed to open device.");

    initialized_ = true;
    ev_init_ = device;
    return true;
}

auto logger::check_init() const -> bool {
    return initialized_;
}

auto logger::start() -> void {
    assert(check_init() && "logger (start): logger not initialized.");
    assert(!running_ && "logger (start): logger already running.");

    running_ = true;
    ev_reader();
}

auto logger::kill() -> void {
    running_ = false;
    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }
    initialized_ = false;
    ev_init_.clear();
}

auto logger::load_keymap(const std::string& config) -> std::unordered_map<unsigned int, std::pair<std::string, std::string>> {
    std::unordered_map<unsigned int, std::pair<std::string, std::string>> tmp;
    std::ifstream conf(config);

    assert(conf.is_open() && "logger (load_keymap): keymap config error.");

    nlohmann::json keyconfig;
    conf >> keyconfig;

    for (auto& [key, val] : keyconfig.items()) {
        auto code = std::stoi(key);
        auto name = std::string{val[0]};
        auto ch = std::string{val[1]};
        tmp.emplace(code, std::make_pair(name, ch));
    }

    return tmp;
}

auto fd_monitor(signed int fd, fd_set fds) -> signed int {
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    auto retval = select(fd + 1, &fds, nullptr, nullptr, &timeout);
    assert(retval != -1 && "logger (fd_monitor): select error.");

    return retval;
}

auto datetime(time_t tv_sec) -> std::pair<std::string, std::string> {
    char date[11], time[13];

    std::tm* tv = std::localtime(&tv_sec);
    std::strftime(date, sizeof(date), "%Y-%m-%d", tv);
    std::strftime(time, sizeof(time), "%H:%M:%S", tv);

    return std::make_pair(std::string{date}, std::string{time});
}

auto find_kbd() -> std::string {
    const std::string hardware_path = "/dev/input/by-id";
    const std::string kbd_id = "-event-kbd";
    std::filesystem::path const input_dir(hardware_path);

    assert(std::filesystem::exists(input_dir) && std::filesystem::is_directory(input_dir) && "logger (find_kbd): invalid input directory.");

    for (auto const& file : std::filesystem::directory_iterator(input_dir)) {
        if (file.is_symlink() || file.is_character_file()) {
            auto const& path = file.path();
            if (path.filename().string().find(kbd_id) != std::string::npos) {
                std::error_code ec;
                auto resolved_path = std::filesystem::canonical(path, ec);
                assert(!ec && "logger (find_kbd): symlink resolution error.");

                auto fd = open(resolved_path.c_str(), O_RDONLY | O_NONBLOCK);
                assert(fd != -1 && "logger (find_kbd): open error.");

                fd_set fds;
                auto retval = fd_monitor(fd, fds);

                if (retval > 0 && FD_ISSET(fd, &fds)) {
                    close(fd);
                    return resolved_path.string();
                }
                close(fd);
            }
        }
    }
    assert(false && "logger (find_kbd): no keyboard found.");
    return "";
}

auto logger::ev_reader() -> void {
    struct input_event ev;
    ssize_t n;
    while (running_) {
        n = read(fd_, &ev, sizeof(ev));
        assert(n != -1 && "logger (ev_reader): read error.");

        if (n == (ssize_t)sizeof(ev) && ev.type == EV_KEY) {
            auto dtg = datetime(ev.time.tv_sec);
            event e{dtg.first, dtg.second, get_keychar(ev.code), ev.value ? "press" : "release", false};
            // TODO: send event e to tsq using dependency injection
        }

        fd_set fds;
        auto retval = fd_monitor(fd_, fds);
        assert(retval != -1 && "logger (ev_reader): select error.");
    }
}

auto logger::get_keychar(unsigned int code) -> std::string {
    auto it = keymap_.find(code);
    assert(it != keymap_.end() && "logger (get_keychar): unknown key code.");
    return it->second.second;
}

