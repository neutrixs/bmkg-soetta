#include "radar.hpp"

#include <cmath>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <date/date.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <radar_debug/debug.h>
#include <thread>

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

    std::vector<std::thread> jobs;
    std::vector<std::string> raw_images(radars.size(), std::string());

    for (int i = 0; i < radars.size(); i++) {
        auto d = radars.at(i);
        std::thread job([this, &raw_images, d, i] { this->download_each(&raw_images, d, i); });

        jobs.push_back(std::move(job));
    }

    for (auto &job : jobs) {
        job.join();
    }

    for (int i = 0; i < raw_images.size(); i++) {
        auto d = radars.at(i);
        auto content = raw_images.at(i);

        std::vector<uchar> buffer(content.begin(), content.end());

        cv::Mat image = cv::imdecode(buffer, cv::IMREAD_UNCHANGED);

        int radar_width = image.cols, radar_height = image.rows;

        // crop if necessary to fit the map (approximate, obviously larger than the accurate size)
        // we will crop it later
        // but, image_cropx is accurate
        // and note the new boundaries of the radar
        double image_cropleft = (boundaries[1] - d->boundaries[1]) / (d->boundaries[3] - d->boundaries[1]) * radar_width;
        image_cropleft = std::max(0., image_cropleft);
        int image_cropleft_floor = floor(image_cropleft);

        double image_cropright = (d->boundaries[3] - boundaries[3]) / (d->boundaries[3] - d->boundaries[1]) * radar_width;
        image_cropright = std::max(0., image_cropright);
        int image_cropright_floor = floor(image_cropright);

        double image_croptop = (d->boundaries[0] - boundaries[0]) / (d->boundaries[0] - d->boundaries[2]) * radar_height;
        image_croptop = std::max(0., image_croptop);
        int image_croptop_floor = floor(image_croptop);

        double image_cropbottom = (boundaries[2] - d->boundaries[2]) / (d->boundaries[0] - d->boundaries[2]) * radar_height;
        image_cropbottom = std::max(0., image_cropbottom);
        int image_cropbottom_floor = floor(image_cropbottom);

        int image_cropwidth = radar_width - image_cropleft_floor - image_cropright_floor;
        int image_cropheight = radar_height - image_croptop_floor - image_cropbottom_floor;

        // crop the image, not creating a copy
        cv::Mat image_crop = image(cv::Rect(image_cropleft_floor, image_croptop_floor, image_cropwidth, image_cropheight));
        image_crop.copyTo(image);

        std::array<double, 4> image_cropbounds = {
            d->boundaries[0] - (d->boundaries[0] - d->boundaries[2]) * image_croptop / radar_height,
            d->boundaries[1] + (d->boundaries[3] - d->boundaries[1]) * image_cropleft / radar_width,
            d->boundaries[2] + (d->boundaries[0] - d->boundaries[2]) * image_cropbottom / radar_height,
            d->boundaries[3] - (d->boundaries[3] - d->boundaries[1]) * image_cropright / radar_width //
        };

        // bounds for the approximated cropped image
        std::array<double, 4> image_cropbounds_floor = {
            d->boundaries[0] - (d->boundaries[0] - d->boundaries[2]) * image_croptop_floor / radar_height,
            d->boundaries[1] + (d->boundaries[3] - d->boundaries[1]) * image_cropleft_floor / radar_width,
            d->boundaries[2] + (d->boundaries[0] - d->boundaries[2]) * image_cropbottom_floor / radar_height,
            d->boundaries[3] - (d->boundaries[3] - d->boundaries[1]) * image_cropright_floor / radar_width //
        };

        // this is where to put the image on the container
        // points: x,y
        std::array<int, 2> image_croppoints = {
            static_cast<int>(width * (image_cropbounds[1] - boundaries[1]) / (boundaries[3] - boundaries[1])),
            static_cast<int>(height * (boundaries[0] - image_cropbounds[0]) / (boundaries[0] - boundaries[2])) //
        };

        // scale (resize) the image according to width, height and boundaries
        int scaled_width =
            round(width * (image_cropbounds_floor[3] - image_cropbounds_floor[1]) / (boundaries[3] - boundaries[1]));
        int scaled_height =
            round(height * (image_cropbounds_floor[0] - image_cropbounds_floor[2]) / (boundaries[0] - boundaries[2]));

        cv::resize(image, image, cv::Size(scaled_width, scaled_height), 0, 0, cv::INTER_NEAREST);

        // create image roi of the more accurate version (in some cases it will be more accurate)
        int trim_left = round(scaled_width * (image_cropbounds[1] - image_cropbounds_floor[1]) /
            (image_cropbounds_floor[3] - image_cropbounds_floor[1]));
        int trim_right = round(scaled_width * (image_cropbounds_floor[3] - image_cropbounds[3]) /
            (image_cropbounds_floor[3] - image_cropbounds_floor[1]));

        int trim_top = round(scaled_height * (image_cropbounds_floor[0] - image_cropbounds[0]) /
            (image_cropbounds_floor[0] - image_cropbounds_floor[2]));
        int trim_bottom = round(scaled_height * (image_cropbounds[2] - image_cropbounds_floor[2]) /
            (image_cropbounds_floor[0] - image_cropbounds_floor[2]));

        // using std::min just in case slightly inaccurate calculation made it few pixels larger than what
        // the container can hold
        int trim_width = scaled_width - trim_left - trim_right;
        trim_width = std::min(width - image_croppoints[0], trim_width);

        int trim_height = scaled_height - trim_top - trim_bottom;
        trim_height = std::min(height - image_croppoints[1], trim_height);

        cv::Mat image_roi = image(cv::Rect(trim_left, trim_top, trim_width, trim_height));
        cv::Mat container_roi = container(cv::Rect(image_croppoints[0], image_croppoints[1], trim_width, trim_height));

        int &roi_width = trim_width;
        int &roi_height = trim_height;
        int &roi_x_start = image_croppoints[0];
        int &roi_y_start = image_croppoints[1];

        // check if the radar is outdated, if it is, create striped pattern, every some px
        const int STRIPE_EVERY_PX = 2;
        cv::Mat empty_mask = cv::Mat::zeros(STRIPE_EVERY_PX, roi_width, CV_8UC4);
        if (std::time(nullptr) - std::chrono::system_clock::to_time_t(d->CMAX.time) > (ignore_after_mins * 60)) {
            for (int y = 0; y < roi_height; y += STRIPE_EVERY_PX * 2) {
                int current_height = std::min(STRIPE_EVERY_PX, roi_height - y);
                cv::Mat empty_mask_roi = empty_mask(cv::Rect(0, 0, roi_width, current_height));
                cv::Mat image_roi_roi = image_roi(cv::Rect(0, y, roi_width, current_height));
                empty_mask_roi.copyTo(image_roi_roi);
            }
        }

        for (int y = 0; y < roi_height; y += check_radar_dist_every_px) {
            for (int x = 0; x < roi_width; x += check_radar_dist_every_px) {
                int width_current = std::min(check_radar_dist_every_px, roi_width - x);
                int height_current = std::min(check_radar_dist_every_px, roi_height - y);

                double cen_x = roi_x_start + (x + width_current) / 2.0;
                double cen_y = roi_y_start + (y + height_current) / 2.0;

                double lat = boundaries[0] - (boundaries[0] - boundaries[2]) * cen_y / height;
                double lon = boundaries[1] + (boundaries[3] - boundaries[1]) * cen_x / width;

                unsigned int closest_index = 0;
                double prev_distance = UINT_MAX;

                for (int i = 0; i < radars.size(); i++) {
                    auto data = *(radars.at(i));

                    double lat_dist = abs(data.lat - lat);
                    double lon_dist = abs(data.lon - lon);
                    double dist = sqrt(pow(lat_dist, 2.0) + pow(lon_dist, 2.0));

                    if (dist < prev_distance)
                        prev_distance = dist, closest_index = i;
                }

                if (radars.at(closest_index) == d) {
                    cv::Mat image_roi_current = image_roi(cv::Rect(x, y, width_current, height_current));
                    cv::Mat container_roi_current = container_roi(cv::Rect(x, y, width_current, height_current));

                    image_roi_current.copyTo(container_roi_current);
                }
            }
        }
    }

    return container;
}

void radar::Imagery::download_each(std::vector<std::string> *raw_images, RadarAPIDataVec *d, int pos) {
    curlpp::initialize();
    std::stringstream response;

    try {
        curlpp::Easy req;
        req.setOpt(new curlpp::options::Url(d->CMAX.file));

        curlpp::options::WriteStream write(&response);
        req.setOpt(write);

        req.perform();
    } catch (curlpp::RuntimeError &e) {
        std::cerr << e.what() << std::endl;
    } catch (curlpp::LogicError &e) {
        std::cerr << e.what() << std::endl;
    }

    std::string content = response.str();
    (*raw_images).at(pos) = content;
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

        std::istringstream in{time_string};
        std::chrono::system_clock::time_point tp;
        in >> date::parse("%Y-%m-%d %H:%M %Z", tp);

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