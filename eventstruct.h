// eventstruct.h

#ifndef eventstruct_h
#define eventstruct_h

#include <string>

struct event {
  std::string date; // YYYY-MM-DD
  std::string time; // HH:MM:SS
  std::string key;
  bool press;
};

#endif // eventstruct_h
