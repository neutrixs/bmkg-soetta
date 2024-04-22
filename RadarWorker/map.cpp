#include "map.hpp"

#include <boost/filesystem.hpp>
#include <cmath>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>
#include <sstream>
#include <vector>

#include "png.hpp"
#include "radar.hpp"

namespace fs = boost::filesystem;
using json = nlohmann::json;

const std::string OSM_TILES_BASE_URL = "https://tile.openstreetmap.org/";
const std::string OSM_NOMINATIM_SEARCH_BASE_URL = "https://nominatim.openstreetmap.org/search";

cv::Mat map::Tiles::render() {
    std::array<double, 4> tiles_range = get_tiles_range();

    int range_north_approx = static_cast<int>(floor(tiles_range[0]));
    int range_west_approx = static_cast<int>(floor(tiles_range[1]));
    int range_south_approx = static_cast<int>(ceil(tiles_range[2]));
    int range_east_approx = static_cast<int>(ceil(tiles_range[3]));

    std::array<double, 2> tstart = coord_to_tile(boundaries[0], boundaries[1], zoom_level);
    std::array<double, 2> tend = coord_to_tile(boundaries[2], boundaries[3], zoom_level);

    double tnorth = tstart[1];
    double twest = tstart[0];
    double tsouth = tend[1];
    double teast = tend[0];

    int rows = range_south_approx - range_north_approx;
    int cols = range_east_approx - range_west_approx;

    std::vector<std::string> tiles_images(rows * cols, std::string());

    fs::path usr_tempdir = fs::current_path() / ".cache";
    fs::create_directories(usr_tempdir);

    std::vector<std::thread> jobs;

    for (int tiles_y = range_north_approx, dl_count = 0; tiles_y < range_south_approx; tiles_y++) {
        for (int tiles_x = range_west_approx; tiles_x < range_east_approx; tiles_x++, dl_count++) {
            std::thread job([this, &tiles_images, tiles_x, tiles_y, usr_tempdir, dl_count] {
                this->download_each(&tiles_images, tiles_x, tiles_y, usr_tempdir, dl_count);
            });

            jobs.push_back(std::move(job));
        }
    }

    for (auto &job : jobs) {
        job.join();
    }

    // get resolution of the first one as a reference
    auto resolution = png::get_resolution(tiles_images.at(0));
    int width = resolution[0], height = resolution[1];

    int uncropped_canvas_width = width * cols, uncropped_canvas_height = height * rows;

    cv::Mat uncropped_canvas = cv::Mat::zeros(uncropped_canvas_height, uncropped_canvas_width, CV_8UC3);

    for (int row = 0, count = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++, count++) {
            std::string current = tiles_images.at(count);
            std::vector<uchar> buffer(current.begin(), current.end());

            cv::Mat tiles_image = cv::imdecode(buffer, cv::IMREAD_COLOR);
            cv::Mat inset(uncropped_canvas, cv::Rect(width * col, height * row, width, height));
            tiles_image.copyTo(inset);
        }
    }

    // tiles_images no longer used
    tiles_images.clear();

    int crop_top = int(height * (tnorth - floor(tnorth)));
    int crop_left = int(width * (twest - floor(twest)));
    int crop_bottom = int(height * (ceil(tsouth) - tsouth));
    int crop_right = int(width * (ceil(teast) - teast));

    int cropped_width = uncropped_canvas_width - crop_left - crop_right;
    int cropped_height = uncropped_canvas_height - crop_top - crop_bottom;

    cv::Mat cropped_canvas(uncropped_canvas, cv::Rect(crop_left, crop_top, cropped_width, cropped_height));

    // turn cropped_canvas to 4 channels
    cv::Mat alpha(cropped_canvas.size(), CV_8UC1, cv::Scalar(255));
    cv::Mat cropped_canvas_alpha;
    std::vector<cv::Mat> channels = {cropped_canvas, alpha};

    // cropped_canvas and alpha no longer used
    cropped_canvas.release();
    alpha.release();

    cv::merge(channels, cropped_canvas_alpha);

    return cropped_canvas_alpha;
}

void map::Tiles::download_each(
    std::vector<std::string> *tiles_images, int tiles_x, int tiles_y, fs::path usr_tempdir, int pos) {

    std::string URL_SCHEME = OSM_TILES_BASE_URL + std::to_string(zoom_level) + "/" + std::to_string(tiles_x) + "/" +
        std::to_string(tiles_y) + ".png";

    std::hash<std::string> hasher;
    size_t hash_value = hasher(URL_SCHEME);

    std::stringstream ss;
    ss << std::hex << hash_value;

    fs::path PATH_SCHEME = usr_tempdir / ss.str();
    bool cache_exists = fs::exists(PATH_SCHEME);

    std::string data;
    if (cache_exists) {
        std::ifstream cache_file(PATH_SCHEME, std::ios::binary);
        if (!cache_file.is_open()) {
            goto redownload;
        }

        std::vector<char> buffer(std::istreambuf_iterator<char>(cache_file), {});
        data = std::string(buffer.begin(), buffer.end());

        cache_file.close();
    } else {
    redownload:
        // TODO: handle exception
        curlpp::initialize();
        std::stringstream response;

        // fake headers to bypass the limitation
        // this is enough
        std::list<std::string> headers;
        headers.push_back("User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                          "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/123.0.0.0 "
                          "Safari/537.36");
        headers.push_back("Referer: https://www.openstreetmap.org/");
        headers.push_back("Accept: "
                          "image/avif,image/webp,image/apng, text/html,image/svg+xml,image/*,*/*;q=0.8");
        headers.push_back("Accept-Encoding: gzip, deflate, br, zstd");
        headers.push_back("Sec-Ch-Ua: \"Google Chrome\";v=\"123\", \"Not:A-Brand\";v=\"8\", "
                          "\"Chromium\";v=\"123\"");

        try {
            curlpp::Easy req;

            req.setOpt(new curlpp::options::Url(URL_SCHEME));
            req.setOpt(new curlpp::options::HttpHeader(headers));

            curlpp::options::WriteStream write(&response);
            req.setOpt(write);

            req.perform();
        } catch (curlpp::RuntimeError &e) {
            std::cerr << e.what() << std::endl;
        } catch (curlpp::LogicError &e) {
            std::cerr << e.what() << std::endl;
        }

        data = response.str();

        std::ofstream output_file(PATH_SCHEME, std::ios::binary);
        output_file.write(data.c_str(), data.size());
        output_file.close();
    }

    (*tiles_images).at(pos) = data;
}

cv::Mat map::Tiles::render_with_overlay_radar(radar::Imagery imagery, float map_brightness, float radar_opacity) {
    cv::Mat base_map = render();
    for (int row = 0; row < base_map.rows; row++) {
        for (int col = 0; col < base_map.cols; col++) {
            cv::Vec4b &v = base_map.at<cv::Vec4b>(row, col);
            v[0] *= map_brightness;
            v[1] *= map_brightness;
            v[2] *= map_brightness;
        }
    }

    imagery.set_boundaries(boundaries[0], boundaries[1], boundaries[2], boundaries[3]);
    cv::Mat radar_imagery = imagery.render(base_map.cols, base_map.rows);
    for (int i = 0; i < radar_imagery.rows; i++) {
        for (int j = 0; j < radar_imagery.cols; j++) {
            cv::Vec4b &v = radar_imagery.at<cv::Vec4b>(i, j);
            v[3] *= radar_opacity;
        }
    }

    map::overlayImage(&base_map, &radar_imagery, cv::Point(0, 0));
    cv::cvtColor(base_map, base_map, cv::COLOR_BGRA2BGR);

    return base_map;
}

cv::Mat map::Tiles::render_with_overlay_radar(float map_brightness, float radar_opacity) {
    radar::Imagery imagery;
    return render_with_overlay_radar(imagery, map_brightness, radar_opacity);
}

void map::Tiles::set_appropriate_zoom_level() {
    for (int zl = 14; zl > 0; zl--) {
        zoom_level = zl;

        std::array<double, 4> tiles_range = get_tiles_range();

        int range_north_approx = static_cast<int>(floor(tiles_range[0]));
        int range_west_approx = static_cast<int>(floor(tiles_range[1]));
        int range_south_approx = static_cast<int>(ceil(tiles_range[2]));
        int range_east_approx = static_cast<int>(ceil(tiles_range[3]));

        int rows = range_south_approx - range_north_approx;
        int cols = range_east_approx - range_west_approx;

        if (rows * cols < MAX_APPROPRIATE_TILES)
            break;
    }
}

std::array<double, 4> map::Tiles::get_tiles_range() {
    std::array<double, 2> range_start = coord_to_tile(boundaries[0], boundaries[1], zoom_level);
    double west_tile = range_start[0];
    double north_tile = range_start[1];

    std::array<double, 2> range_end = coord_to_tile(boundaries[2], boundaries[3], zoom_level);
    double east_tile = range_end[0];
    double south_tile = range_end[1];

    return std::array<double, 4>{north_tile, west_tile, south_tile, east_tile};
}

std::array<double, 2> map::Tiles::coord_to_tile(double lat, double lon, int zoom) {
    double n = powf64(2, zoom);
    double x = n * ((lon + 180) / 360);
    double y = n * (1 - (log(tan(lat * M_PI / 180.0) + 1 / cos(lat * M_PI / 180.0)) / M_PI)) / 2.0;

    return std::array<double, 2>{x, y};
}

std::array<double, 2> map::Tiles::tile_to_coord(double x, double y, int zoom) {
    double n = powf64(2, zoom);
    double lon_deg = x / n * 360.0 - 180.0;
    double lat_rad = atan(sinh(M_PI * (1 - 2 * y / n)));
    double lat_deg = lat_rad * 180.0 / M_PI;

    return std::array<double, 2>{lat_deg, lon_deg};
}

std::array<double, 4> map::OSM_get_bounding_box(std::string place) {
    std::string URL = OSM_NOMINATIM_SEARCH_BASE_URL + "?q=" + cURLpp::escape(place) + "&format=json";

    curlpp::initialize();
    std::stringstream response;

    std::list<std::string> 👺👺👺👺👺;
    👺👺👺👺👺.push_back("User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                         "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/123.0.0.0 "
                         "Safari/537.36");
    👺👺👺👺👺.push_back("Referer: https://www.openstreetmap.org/");
    👺👺👺👺👺.push_back("Accept: "
                         "application/json,text/html,image/svg+xml,image/*,*/*;q=0.8");
    👺👺👺👺👺.push_back("Accept-Encoding: gzip, deflate, br, zstd");
    👺👺👺👺👺.push_back("Sec-Ch-Ua: \"Google Chrome\";v=\"123\", \"Not:A-Brand\";v=\"8\", "
                         "\"Chromium\";v=\"123\"");

    try {
        curlpp::Easy req;
        req.setOpt(new curlpp::options::Url(URL));
        req.setOpt(new curlpp::options::HttpHeader(👺👺👺👺👺));

        curlpp::options::WriteStream write(&response);
        req.setOpt(write);

        req.perform();
    } catch (curlpp::RuntimeError &e) {
        std::cerr << e.what() << std::endl;
    } catch (curlpp::LogicError &e) {
        std::cerr << e.what() << std::endl;
    }

    std::string content = response.str();
    json API_data;

    try {
        API_data = json::parse(content);
    } catch (const json::parse_error &e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
    }

    std::array<double, 4> bounding_box{0.0, 0.0, 0.0, 0.0};
    if (API_data.size() == 0) {
        std::cerr << "Not found" << std::endl;
        return bounding_box;
    }

    for (int i = 0; i < API_data.size(); i++) {
        if (API_data[i]["addresstype"] == "railway")
            continue;
        auto box = API_data[i]["boundingbox"];
        bounding_box[0] = stod(static_cast<std::string>(box[1]));
        bounding_box[1] = stod(static_cast<std::string>(box[2]));
        bounding_box[2] = stod(static_cast<std::string>(box[0]));
        bounding_box[3] = stod(static_cast<std::string>(box[3]));
        break;
    }

    return bounding_box;
}

void map::overlayImage(cv::Mat *src, cv::Mat *overlay, const cv::Point &location) {
    for (int y = (location.y, 0); y < src->rows; ++y) {
        int fY = y - location.y;

        if (fY >= overlay->rows)
            break;

        for (int x = std::max(location.x, 0); x < src->cols; ++x) {
            int fX = x - location.x;

            if (fX >= overlay->cols)
                break;

            double opacity = ((double)overlay->data[fY * overlay->step + fX * overlay->channels() + 3]) / 255;

            for (int c = 0; opacity > 0 && c < src->channels(); ++c) {
                unsigned char overlayPx = overlay->data[fY * overlay->step + fX * overlay->channels() + c];
                unsigned char srcPx = src->data[y * src->step + x * src->channels() + c];
                src->data[y * src->step + src->channels() * x + c] = srcPx * (1. - opacity) + overlayPx * opacity;
            }
        }
    }
}