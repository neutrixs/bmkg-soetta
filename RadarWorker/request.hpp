#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <sstream>

namespace request {
    std::stringstream fetch(std::string url);
}

#endif