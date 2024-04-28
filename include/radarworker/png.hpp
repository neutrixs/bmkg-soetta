#ifndef PNG_HPP
#define PNG_HPP

#include <array>
#include <string>

namespace png {
std::array<unsigned int, 2> get_resolution(std::string& data);
}

#endif