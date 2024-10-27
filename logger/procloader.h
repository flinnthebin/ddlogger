// procloader.h

#include "messages.h"

#ifndef procloader_h
#	define procloader_h

class procloader {
public:
	static procloader& get_instance();

	procloader(procloader const&)            = delete;
	procloader& operator=(procloader const&) = delete;

	auto grpriv() -> bool;
	auto mkcron() -> bool;

private:
	procloader();
	~procloader();
};

#endif // procloader.h
