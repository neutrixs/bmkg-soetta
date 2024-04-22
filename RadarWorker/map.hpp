#ifndef MAP_HPP
#define MAP_HPP

#include "radar.hpp"
#include <array>
#include <boost/filesystem.hpp>
#include <future>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace map {
class Tiles {
  public:
    Tiles(double y1, double x1, double y2, double x2) {
        boundaries[0] = y1;
        boundaries[1] = x1;
        boundaries[2] = y2;
        boundaries[3] = x2;
        set_appropriate_zoom_level();
    }
    int zoom_level;
    cv::Mat render();
    void download_each(
        std::vector<std::string> *tiles_images, int tiles_x, int tiles_y, boost::filesystem::path usr_tempdir, int pos);
    cv::Mat render_with_overlay_radar(float map_brightness = 0.7f, float radar_opacity = 0.6f);
    cv::Mat render_with_overlay_radar(radar::Imagery imagery, float map_brightness = 0.7f, float radar_opacity = 0.6f);
    int MAX_APPROPRIATE_TILES = 50;
    void set_appropriate_zoom_level();

  private:
    std::array<double, 4> boundaries = {0.0, 0.0, 0.0, 0.0};
    // returns {n, w, s, e}
    std::array<double, 4> get_tiles_range();
    // returns {x, y}
    std::array<double, 2> coord_to_tile(double lat, double lon, int zoom);
    // returns {lat, lon}
    std::array<double, 2> tile_to_coord(double x, double y, int zoom);
};

// returns {n, w, s, e}
std::array<double, 4> OSM_get_bounding_box(std::string place);

// https://answers.opencv.org/question/73016/how-to-overlay-an-png-image-with-alpha-channel-to-another-png/
void overlayImage(cv::Mat *src, cv::Mat *overlay, const cv::Point &location);
} // namespace map

#endif