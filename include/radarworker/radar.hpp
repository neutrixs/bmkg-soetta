#ifndef RADAR_HPP
#define RADAR_HPP

// converts the kilometer to longitude degree at equator
#define KM / 40075 * 360

#include <array>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace radar {
constexpr char RADAR_LIST_API_URL[] = "https://radar.bmkg.go.id:8090/radarlist";
constexpr char RADAR_IMAGE_PUBLIC_API_URL[] = "https://api-apps.bmkg.go.id/api/radar-image";
constexpr char RADAR_IMAGE_API_URL[] = "https://radar.bmkg.go.id:8090/sidarmaimage";

struct Color {
    int r;
    int g;
    int b;
};

constexpr std::array<Color, 14> ColorScheme = {{
    {173, 216, 230}, // 5-10 dBZ (Light Blue)
    {0, 0, 255},     // 10-15 dBZ (Medium Blue)
    {0, 0, 139},     // 15-20 dBZ (Dark Blue)
    {0, 255, 0},     // 20-25 dBZ (Green)
    {50, 205, 50},   // 25-30 dBZ (Lime Green)
    {255, 255, 0},   // 30-35 dBZ (Yellow)
    {255, 215, 0},   // 35-40 dBZ (Gold)
    {255, 165, 0},   // 40-45 dBZ (Orange)
    {255, 140, 0},   // 45-50 dBZ (Dark Orange)
    {255, 0, 0},     // 50-55 dBZ (Red)
    {139, 0, 0},     // 55-60 dBZ (Dark Red)
    {255, 0, 255},   // 60-65 dBZ (Magenta)
    {128, 0, 128},   // 65-70 dBZ (Purple)
}};

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
    std::vector<Color> colors;
};

bool is_overlapping(std::array<double, 4> x, std::array<double, 4> y);
Color parseHexColor(const std::string &hexColor);

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
    int max_concurrent_threads = 7;

    std::vector<RadarImage *> used_radars;

    int zoom_level = 13;
    int check_radar_dist_every_px = 10;
    std::vector<std::string> exclude_radar;
    std::map<std::string, double> radarRangeOverride;
    std::map<std::string, int> radarPriority;

    cv::Mat render(int width, int height);

    Imagery() {
        radarRangeOverride = {{"PWK", 110.0 KM}, {"CGK", 90.0 KM}, {"JAK", 200.0 KM}};
        radarPriority = {{"PWK", 1}, {"CGK", 2}, {"JAK", 0}};
    }

  private:
    void render_loop(int width, int height, std::vector<radar::RadarImage> &radars, std::vector<std::string> &raw_images,
        cv::Mat &container, int i, std::mutex &mtx, bool *is_done);
    std::array<double, 4> boundaries;
    std::vector<RadarImage> radar_datas;
    std::vector<RadarImage> &get_radar_datas();
    void fetch_detailed_data(std::string code, std::mutex &mtx, std::string &runtime_error, int index);
};

} // namespace radar

#endif