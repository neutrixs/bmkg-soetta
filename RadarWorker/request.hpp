#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <sstream>
#include <tuple>

namespace request {
std::stringstream fetch(std::string url, bool osm_fake_headers);
std::tuple<double, double> coordToTile(double lat, double lon, int zoom);
std::tuple<double, double> tileToCoord(double x, double y, int zoom);
std::tuple<double, double, double, double> getTilesRange(double n, double w, double s, double e, int zoom);

}  // namespace request

namespace radar {
namespace bounds {
constexpr double north = -4.371433;
constexpr double west = 104.846622;
constexpr double south = -7.971433;
constexpr double east = 108.446622;
}  // namespace bounds

std::stringstream getLatest();
}  // namespace radar

#endif