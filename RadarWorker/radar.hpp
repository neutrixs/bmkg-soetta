#ifndef RADAR_HPP
#define RADAR_HPP

#include <array>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace radar {
struct RadarAPICMAX {
    std::chrono::system_clock::time_point time;
    std::string file;

    RadarAPICMAX() {
        time = std::chrono::system_clock::from_time_t(0);
        file = "";
    }
};

struct RadarAPIDataVec {
    // north, west, south, east
    std::array<double, 4> boundaries;
    std::string kota;
    std::string stasiun;
    std::string kode;
    double lat;
    double lon;
    RadarAPICMAX CMAX;

    RadarAPIDataVec() {
        boundaries = {0.0, 0.0, 0.0, 0.0};
        kota = "";
        stasiun = "";
        kode = "";
        lat = 0.0;
        lon = 0.0;
    }
};

bool is_overlapping(std::array<double, 4> x, std::array<double, 4> y);

class Imagery {
  public:
    void set_boundaries(double y1, double x1, double y2, double x2) {
        boundaries[0] = y1;
        boundaries[1] = x1;
        boundaries[2] = y2;
        boundaries[3] = x2;
    }
    int zoom_level = 13;
    // terrible quality, lots of interverence
    // there is CGK nearby anyway
    std::vector<std::string> exclude_radar = {"JAK"};
    cv::Mat render(int width, int height);
    std::vector<radar::RadarAPIDataVec> &get_radar_datas();
    std::vector<radar::RadarAPIDataVec *> get_radars_in_range();
    radar::RadarAPIDataVec &get_closest();

  private:
    std::array<double, 4> boundaries = {0.0, 0.0, 0.0, 0.0};
    std::vector<radar::RadarAPIDataVec> radar_datas;
};
} // namespace radar

#endif