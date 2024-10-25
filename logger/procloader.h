// procloader.h

#ifndef procloader_h
#define procloader_h

class procloader {
public:
	static procloader& get_instance();
	
	procloader(procloader const&) = delete;
	procloader& operator=(procloader const&) = delete;

	auto start() -> void;
	auto kill() -> void;

private:
	procloader();
	~procloader();

	// member vars?

	auto grpriv() -> void;

};

#endif // procloader.h
