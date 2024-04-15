#include "radar.hpp"

#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <iostream>
#include <nlohmann/json.hpp>

#include "png.hpp"

using json = nlohmann::json;

const std::string BMKG_RADAR_BASE_URL = "https://api-apps.bmkg.go.id/storage/radar/radar-dev.json";

cv::Mat radar::Imagery::render(int width, int height) {
    radar::RadarAPIDataVec closest = get_closest();
    std::string URL = closest.CMAX.file;

    curlpp::initialize();
    std::stringstream response;

    try {
        curlpp::Easy req;
        req.setOpt(new curlpp::options::Url(URL));

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

    double original_cropn =
        (boundaries[0] - closest.overlay_tlc[0]) / (closest.overlay_brc[0] - closest.overlay_tlc[0]) * radar_height;
    int original_cropn_floor = floor(original_cropn);

    double original_cropw =
        (boundaries[1] - closest.overlay_tlc[1]) / (closest.overlay_brc[1] - closest.overlay_tlc[1]) * radar_width;
    int original_cropw_floor = floor(original_cropw);

    double original_crops =
        (boundaries[2] - closest.overlay_brc[0]) / (closest.overlay_tlc[0] - closest.overlay_brc[0]) * radar_height;
    int original_crops_floor = floor(original_crops);

    double original_crope =
        (boundaries[3] - closest.overlay_brc[1]) / (closest.overlay_tlc[1] - closest.overlay_brc[1]) * radar_width;
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

    // will return exceptions on requesting area not overlapping with any radars
    // in that case, just return a blank image

    try {
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

        return resized_cropped;
    } catch (cv::Exception &e) {
        std::cerr << "Error: requesting area outside radar bounds" << std::endl;

        return cv::Mat::zeros(height, width, CV_8UC4);
    }
}

std::vector<radar::RadarAPIDataVec> &radar::Imagery::get_radar_datas() {
    if (radar_datas.size() != 0) return radar_datas;

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
        radar_data_struct.overlay_tlc = {north, west};
        radar_data_struct.overlay_brc = {south, east};
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

        if (dist < prev_distance) prev_distance = dist, closest_index = i;
    }

    return radar_datas.at(closest_index);
}