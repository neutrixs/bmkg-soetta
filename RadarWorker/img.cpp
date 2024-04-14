#include "img.hpp"

#include <bitset>
#include <iostream>

#include "string"

std::tuple<unsigned int, unsigned int> png::get_resolution(std::string& data) {
    std::string ihdr_start = data.substr(16);

    std::string width_part = ihdr_start.substr(0, 4);
    std::string height_part = ihdr_start.substr(4, 4);

    unsigned int width = 0;
    unsigned int height = 0;

    for (int pos = 0, shift = 3; pos < 4; pos++, shift--) {
        width |= static_cast<u_char>(width_part[pos]) << (shift * 8);
        height |= static_cast<u_char>(height_part[pos]) << (shift * 8);
    }

    return std::tuple<unsigned int, unsigned int>(width, height);
}