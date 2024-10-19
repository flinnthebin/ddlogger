// keymap.h

#ifndef KEYMAP_H
#define KEYMAP_H

#include <unordered_map>

#include <string>

using keymap_t = std::unordered_map<unsigned int, std::pair<std::string, std::string>>;

extern keymap_t const keymap;

#endif // KEYMAP_H
