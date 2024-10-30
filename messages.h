// messages.h

#ifndef messages_h
#define messages_h

#include <iostream>
#include <string>

enum class messagetype { none, error, warning, info, debug };

extern messagetype messages;

#define CODE(level)                                                                                                    \
	((level) == messagetype::error     ? "\033[31m"                                                                      \
	 : (level) == messagetype::warning ? "\033[33m"                                                                      \
	 : (level) == messagetype::info    ? "\033[32m"                                                                      \
	 : (level) == messagetype::debug                                                                                     \
	   ? "\033[35m"                                                                                                      \
	   : "")

#define MSG(level, message)                                                                                            \
	do {                                                                                                                 \
		if (level <= messages) {                                                                                           \
			std::cerr << CODE(level) << message << "\033[0m" << std::endl;                                                   \
		}                                                                                                                  \
	}                                                                                                                    \
	while (0)

#endif // messages_h
