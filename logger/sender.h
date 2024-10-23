// sender.h

#ifndef sender_h
#define sender_h

#include "eventstruct.h"
#include "tsq.h"
#include <nlohmann/json.hpp>

#include <thread>

class sender {
public:
	static sender& get_instance();

	sender(sender const&)            = delete;
	sender& operator=(sender const&) = delete;

	auto init(std::string const& event_ID = "") -> bool;
	auto check_init() const -> bool;

	auto start() -> void;
	auto kill() -> void;

private:
	sender();
	~sender();

	auto ev_to_json(const event& e) -> nlohmann::json;
	auto push_jsonev(nlohmann::json json) -> void;
	auto process() -> void;

	tsq         q_;
	std::thread work_;
	bool        initialized_;
	bool        running_;
	std::string ev_init_;
};

#endif // sender_h
