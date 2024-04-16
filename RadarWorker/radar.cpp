#include "radar.hpp"

#include <cmath>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <iostream>
#include <nlohmann/json.hpp>

#include "map.hpp"
#include "png.hpp"

using json = nlohmann::json;

const std::string BMKG_RADAR_BASE_URL = "https://api-apps.bmkg.go.id/storage/radar/radar-dev.json";

bool radar::is_overlapping(std::array<double, 4> x, std::array<double, 4> y) {
    double &latx1 = x[2];
    double &latx2 = x[0];
    double &laty1 = y[2];
    double &laty2 = y[0];

    double &lonx1 = x[1];
    double &lonx2 = x[3];
    double &lony1 = y[1];
    double &lony2 = y[3];

    bool lat_overlap = std::max(latx1, laty1) < std::min(latx2, laty2);
    bool lon_overlap = std::max(lonx1, lony1) < std::min(lonx2, lony2);

    return lat_overlap && lon_overlap;
}

cv::Mat radar::Imagery::render(int width, int height) {
    std::vector<radar::RadarAPIDataVec *> radars = get_radars_in_range();
    cv::Mat container = cv::Mat::zeros(height, width, CV_8UC4);

    for (int i = 0; i < radars.size(); i++) {
        auto radar_data = radars.at(i);

        curlpp::initialize();
        std::stringstream response;

        try {
            curlpp::Easy req;
            req.setOpt(new curlpp::options::Url(radar_data->CMAX.file));

            curlpp::options::WriteStream write(&response);
            req.setOpt(write);

            req.perform();
        } catch (curlpp::RuntimeError &e) {
            std::cerr << e.what() << std::endl;
        } catch (curlpp::LogicError &e) {
            std::cerr << e.what() << std::endl;
        }

        std::string content = response.str();
        std::vector<uchar> buffer(content.begin(), content.end());

        auto resolution = png::get_resolution(content);
        int radar_width = resolution[0];
        int radar_height = resolution[1];

        cv::Mat original_image = cv::imdecode(buffer, cv::IMREAD_UNCHANGED);

        double original_cropn = (boundaries[0] - radar_data->boundaries[0]) /
            (radar_data->boundaries[2] - radar_data->boundaries[0]) * radar_height;
        int original_cropn_floor = floor(original_cropn);

        double original_cropw =
            (boundaries[1] - radar_data->boundaries[1]) / (radar_data->boundaries[3] - radar_data->boundaries[1]) * radar_width;
        int original_cropw_floor = floor(original_cropw);

        double original_crops = (boundaries[2] - radar_data->boundaries[2]) /
            (radar_data->boundaries[0] - radar_data->boundaries[2]) * radar_height;
        int original_crops_floor = floor(original_crops);

        double original_crope =
            (boundaries[3] - radar_data->boundaries[3]) / (radar_data->boundaries[1] - radar_data->boundaries[3]) * radar_width;
        int original_crope_floor = floor(original_crope);

        int temp_width = radar_width - original_cropw_floor - original_crope_floor;
        int temp_height = radar_height - original_cropn_floor - original_crops_floor;

        cv::Mat temp_cropped = cv::Mat::zeros(temp_height, temp_width, CV_8UC4);

        auto inset_m_x_pos = abs(std::min(0, original_cropw_floor));
        auto inset_m_y_pos = abs(std::min(0, original_cropn_floor));
        auto inset_mr_x_width = original_cropw_floor >= 0 ? std::min(temp_width, temp_width + original_crope_floor)
                                                          : std::min(radar_width, radar_width - original_crope_floor);
        auto inset_mr_y_width = original_cropn_floor >= 0 ? std::min(temp_height, temp_height + original_crops_floor)
                                                          : std::min(radar_height, radar_height - original_crops_floor);

        auto inset_r_x_pos = std::max(0, original_cropw_floor);
        auto inset_r_y_pos = std::max(0, original_cropn_floor);

        cv::Mat inset_m(temp_cropped, cv::Rect(inset_m_x_pos, inset_m_y_pos, inset_mr_x_width, inset_mr_y_width));
        cv::Mat inset_r(original_image, cv::Rect(inset_r_x_pos, inset_r_y_pos, inset_mr_x_width, inset_mr_y_width));

        inset_r.copyTo(inset_m);

        int resized_uncropped_width =
            width + width * ((original_cropw - original_cropw_floor + original_crope - original_crope_floor) / temp_width);
        int resized_uncropped_height =
            height + height * ((original_cropn - original_cropn_floor + original_crops - original_crops_floor) / temp_height);

        cv::Mat resized_uncropped;
        cv::resize(temp_cropped, resized_uncropped, cv::Size(resized_uncropped_width, resized_uncropped_height), 0, 0,
            cv::INTER_NEAREST);

        cv::Mat resized_cropped = cv::Mat::zeros(height, width, CV_8UC4);
        int trim_left = (original_cropw - original_cropw_floor) / temp_width * width;
        int trim_top = (original_cropn - original_cropn_floor) / temp_height * height;

        cv::Mat resized_cropped_inset(resized_uncropped, cv::Rect(trim_left, trim_top, width, height));
        resized_cropped_inset.copyTo(resized_cropped);

        if (i == 0) {
            // first time, just copy
            resized_cropped.copyTo(container);
        } else {
            map::overlayImage(&container, &resized_cropped, cv::Point(0, 0));
        }
    }

    return container;
}

std::vector<radar::RadarAPIDataVec> &radar::Imagery::get_radar_datas() {
    if (radar_datas.size() != 0)
        return radar_datas;

    curlpp ::initialize();
    std::stringstream response;

    try {
        curlpp::Easy req;
        req.setOpt(new curlpp::options::Url(BMKG_RADAR_BASE_URL));

        curlpp::options::WriteStream write(&response);
        req.setOpt(write);

        req.perform();
    } catch (curlpp::RuntimeError &e) {
        std::cerr << e.what() << std::endl;
    } catch (curlpp::LogicError &e) {
        std::cerr << e.what() << std::endl;
    }

    std::string content = response.str();

    std::vector<radar::RadarAPIDataVec> output;

    json API_data;
    try {
        API_data = json::parse(content);
    } catch (const json::parse_error &e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
    }

    std::vector<radar::RadarAPIDataVec> data;

    for (auto radar_data : API_data) {
        auto tlc_raw = radar_data["overlayTLC"];
        auto brc_raw = radar_data["overlayBRC"];

        double north = std::stod(static_cast<std::string>(tlc_raw[0]));
        double west = std::stod(static_cast<std::string>(tlc_raw[1]));
        double south = std::stod(static_cast<std::string>(brc_raw[0]));
        double east = std::stod(static_cast<std::string>(brc_raw[1]));

        std::string kota = static_cast<std::string>(radar_data["Kota"]);
        std::string stasiun = static_cast<std::string>(radar_data["Stasiun"]);
        std::string kode = static_cast<std::string>(radar_data["kode"]);
        double lat = radar_data["lat"];
        double lon = radar_data["lon"];

        std::string file = static_cast<std::string>(radar_data["CMAX"]["file"]);
        std::string time_string = static_cast<std::string>(radar_data["CMAX"]["timeUTC"]);

        std::stringstream ss(time_string);
        std::tm tm = {};
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M %Z");
        auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

        radar::RadarAPICMAX cmax;
        cmax.file = file;
        cmax.time = tp;

        radar::RadarAPIDataVec radar_data_struct;
        radar_data_struct.boundaries = {north, west, south, east};
        radar_data_struct.kota = kota;
        radar_data_struct.stasiun = stasiun;
        radar_data_struct.kode = kode;
        radar_data_struct.lat = lat;
        radar_data_struct.lon = lon;
        radar_data_struct.CMAX = cmax;

        data.push_back(radar_data_struct);
    }

    radar_datas = data;

    return radar_datas;
}

std::vector<radar::RadarAPIDataVec *> radar::Imagery::get_radars_in_range() {
    get_radar_datas();
    std::vector<radar::RadarAPIDataVec *> output;

    for (auto &radar_data : radar_datas) {
        if (std::find(exclude_radar.begin(), exclude_radar.end(), radar_data.kode) != exclude_radar.end())
            continue;

        // eliminates those with no time data (i think)
        if (std::chrono::system_clock::to_time_t(radar_data.CMAX.time) <= 0)
            continue;

        bool in_range = radar::is_overlapping(boundaries, radar_data.boundaries);
        if (!in_range)
            continue;

        output.push_back(&radar_data);
    }

    double cen_lat = (boundaries[0] + boundaries[2]) / 2;
    double cen_lon = (boundaries[1] + boundaries[3]) / 2;

    // sort by distance descending
    std::sort(output.begin(), output.end(), [cen_lat, cen_lon](radar::RadarAPIDataVec *&a, radar::RadarAPIDataVec *&b) {
        double dist_a_x = abs(a->lon - cen_lon);
        double dist_a_y = abs(a->lat - cen_lat);
        double dist_a = sqrt(pow(dist_a_x, 2) + pow(dist_a_y, 2));

        double dist_b_x = abs(b->lon - cen_lon);
        double dist_b_y = abs(b->lat - cen_lat);
        double dist_b = sqrt(pow(dist_b_x, 2) + pow(dist_b_y, 2));

        return dist_a > dist_b;
    });

    return output;
}

radar::RadarAPIDataVec &radar::Imagery::get_closest() {
    get_radar_datas();

    unsigned int closest_index = 0;
    double prev_distance = UINT_MAX;
    double cen_lat = (boundaries[0] + boundaries[2]) / 2;
    double cen_lon = (boundaries[1] + boundaries[3]) / 2;

    for (int i = 0; i < radar_datas.size(); i++) {
        const radar::RadarAPIDataVec &data = radar_datas.at(i);

        double lat_dist = abs(data.lat - cen_lat);
        double lon_dist = abs(data.lon - cen_lon);
        double dist = sqrt(pow(lat_dist, 2.0) + pow(lon_dist, 2.0));

        // eliminates those with no time data (i think)
        if (std::chrono::system_clock::to_time_t(data.CMAX.time) <= 0) {
            continue;
        }

        if (dist < prev_distance)
            prev_distance = dist, closest_index = i;
    }

    return radar_datas.at(closest_index);
}