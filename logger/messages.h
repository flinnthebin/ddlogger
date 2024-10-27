//  messages.h

#ifndef messages_h
#define messages_h

#include <iostream>
#include <string>

enum class messagetype { none, error, warning, info, debug };

extern messagetype messages;

#define LOG(level, message)                                                                                            \
	do {                                                                                                                 \
		if (level <= messages) {                                                                                           \
			std::cerr << message << std::endl;                                                                               \
		}                                                                                                                  \
	}                                                                                                                    \
	while (0)

#endif // messages_h
