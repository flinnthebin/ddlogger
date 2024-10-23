// eventstruct.h

#ifndef eventstruct_h
#define eventstruct_h

#include <string>

struct event {
	std::string date; // YYYY-MM-DD
	std::string time; // HH:MM:SS
	std::string key;
	std::string press;
	bool        mod;
};

#endif // eventstruct_h
