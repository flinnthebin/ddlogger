// keymap.h

#ifndef KEYMAP_H
#define KEYMAP_H

#include <unordered_map>
#include <string>

struct key_info {
	std::string  key_name;
	unsigned int key_code;
	std::string  key_char;
};
using keymap_t = std::unordered_map<std::string, key_info>;

extern keymap_t const keymap;

#endif // KEYMAP_H
