// sender.h

#ifndef sender_h
#define sender_h

#include "eventstruct.h"
#include "tsq.h"
#include <nlohmann/json.hpp>

#include <atomic>
#include <thread>

class sender {
public:
	static sender& get_instance(tsq& queue);

	sender(sender const&)            = delete;
	sender& operator=(sender const&) = delete;

	auto init(std::string const& event_ID = "") -> bool;
	auto check_init() const -> bool;

	auto start() -> void;
	auto kill() -> void;

private:
	sender(tsq& queue);
	~sender();

	auto ev_to_json(const event& e) -> nlohmann::json;
	auto push_jsonev(nlohmann::json json) -> void;
	auto process() -> void;

	int               fd_;
	bool              initialized_;
	std::atomic<bool> running_;
	std::string       ev_init_;
	tsq&              q_;
	std::thread       work_;
};

#endif // sender_h
