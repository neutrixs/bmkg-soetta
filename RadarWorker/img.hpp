#ifndef IMG_HPP
#define IMG_HPP

#include <string>
#include <tuple>

namespace png {
std::tuple<unsigned int, unsigned int> get_resolution(std::string& data);
}

#endif