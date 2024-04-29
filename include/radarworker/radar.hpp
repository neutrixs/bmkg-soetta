#ifndef RADAR_HPP
#define RADAR_HPP

#include <array>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace radar {
constexpr char RADAR_LIST_API_URL[] = "https://radar.bmkg.go.id:8090/radarlist";
constexpr char RADAR_IMAGE_PUBLIC_API_URL[] = "https://api-apps.bmkg.go.id/api/radar-image";
constexpr char RADAR_IMAGE_API_URL[] = "https://radar.bmkg.go.id:8090/sidarmaimage";

struct RadarList {
    std::array<double, 4> boundaries{};
    std::string kota;
    std::string stasiun;
    std::string kode;
    double lat;
    double lon;
};

struct RadarImageData {
    std::vector<std::chrono::system_clock::time_point> time;
    std::vector<std::string> file;
};

struct RadarImage {
    RadarImageData data;
    std::array<double, 4> boundaries{};
    std::string kota;
    std::string stasiun;
    std::string kode;
    double lat;
    double lon;
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

    int declare_old_after_mins = 20;
    bool ignore_old_radars = false;
    bool stripe_on_old_radars = true;

    std::vector<RadarImage *> used_radars;

    int zoom_level = 13;
    int check_radar_dist_every_px = 50;
    std::vector<std::string> exclude_radar;

    cv::Mat render(int width, int height);

  private:
    std::array<double, 4> boundaries;
    std::vector<RadarImage> radar_datas;
    std::vector<RadarImage> &get_radar_datas();
    void fetch_detailed_data(std::string code, std::mutex &mtx);
    void download(std::vector<std::string> *raw_images, RadarImage &d, int pos, std::mutex &mtx);
};

} // namespace radar

#endif