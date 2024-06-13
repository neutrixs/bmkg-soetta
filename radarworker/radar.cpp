#include <cmath>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <date/date.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <radar_debug/debug.h>
#include <radarworker/fetch.hpp>
#include <radarworker/radar.hpp>
#include <thread>

using json = nlohmann::json;

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
    std::vector<radar::RadarImage> &radars = get_radar_datas();
    cv::Mat container = cv::Mat::zeros(height, width, CV_8UC4);

    std::vector<std::thread> jobs;
    std::vector<std::string> raw_images(radars.size(), std::string());
    std::mutex mtx;

    std::string runtime_error("");

    for (int i = 0; i < radars.size(); i++) {
        auto &d = radars.at(i);

        std::thread job([this, &raw_images, &d, i, &mtx, &runtime_error] {
            mtx.lock();
            std::string url = d.data.file.back();
            mtx.unlock();
            try {
                std::string content = fetch::get(url);
                mtx.lock();
                raw_images.at(i) = content;
                mtx.unlock();
            } catch (std::runtime_error &e) {
                runtime_error = e.what();
            }
        });

        jobs.push_back(std::move(job));
    }

    for (auto &job : jobs) {
        if (job.joinable()) {
            job.join();
        }
    }

    if (runtime_error != "") {
        throw std::runtime_error(runtime_error);
    }

    used_radars.clear();

    for (int i = 0; i < raw_images.size(); i++) {
        auto d = radars.at(i);
        auto content = raw_images.at(i);

        std::vector<uchar> buffer(content.begin(), content.end());

        cv::Mat image;
        try {
            image = cv::imdecode(buffer, cv::IMREAD_UNCHANGED);
        } catch (cv::Exception &e) {
            std::string err = e.what();
            throw std::runtime_error("OpenCV error: " + err);
        }

        int radar_width = image.cols, radar_height = image.rows;

        // crop if necessary to fit the map (approximate, obviously larger than the accurate size)
        // we will crop it later
        // but, image_cropx is accurate
        // and note the new boundaries of the radar
        double image_cropleft = (boundaries[1] - d.boundaries[1]) / (d.boundaries[3] - d.boundaries[1]) * radar_width;
        image_cropleft = std::max(0., image_cropleft);
        int image_cropleft_floor = floor(image_cropleft);

        double image_cropright = (d.boundaries[3] - boundaries[3]) / (d.boundaries[3] - d.boundaries[1]) * radar_width;
        image_cropright = std::max(0., image_cropright);
        int image_cropright_floor = floor(image_cropright);

        double image_croptop = (d.boundaries[0] - boundaries[0]) / (d.boundaries[0] - d.boundaries[2]) * radar_height;
        image_croptop = std::max(0., image_croptop);
        int image_croptop_floor = floor(image_croptop);

        double image_cropbottom = (boundaries[2] - d.boundaries[2]) / (d.boundaries[0] - d.boundaries[2]) * radar_height;
        image_cropbottom = std::max(0., image_cropbottom);
        int image_cropbottom_floor = floor(image_cropbottom);

        int image_cropwidth = radar_width - image_cropleft_floor - image_cropright_floor;
        int image_cropheight = radar_height - image_croptop_floor - image_cropbottom_floor;

        // crop the image, not creating a copy
        cv::Mat image_crop = image(cv::Rect(image_cropleft_floor, image_croptop_floor, image_cropwidth, image_cropheight));
        image_crop.copyTo(image);

        std::array<double, 4> image_cropbounds = {
            d.boundaries[0] - (d.boundaries[0] - d.boundaries[2]) * image_croptop / radar_height,
            d.boundaries[1] + (d.boundaries[3] - d.boundaries[1]) * image_cropleft / radar_width,
            d.boundaries[2] + (d.boundaries[0] - d.boundaries[2]) * image_cropbottom / radar_height,
            d.boundaries[3] - (d.boundaries[3] - d.boundaries[1]) * image_cropright / radar_width //
        };

        // bounds for the approximated cropped image
        std::array<double, 4> image_cropbounds_floor = {
            d.boundaries[0] - (d.boundaries[0] - d.boundaries[2]) * image_croptop_floor / radar_height,
            d.boundaries[1] + (d.boundaries[3] - d.boundaries[1]) * image_cropleft_floor / radar_width,
            d.boundaries[2] + (d.boundaries[0] - d.boundaries[2]) * image_cropbottom_floor / radar_height,
            d.boundaries[3] - (d.boundaries[3] - d.boundaries[1]) * image_cropright_floor / radar_width //
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

        auto seconds_to_now = std::time(nullptr) - std::chrono::system_clock::to_time_t(d.data.time.back());

        if (seconds_to_now > (declare_old_after_mins * 60) && stripe_on_old_radars) {
            for (int y = 0; y < roi_height; y += STRIPE_EVERY_PX * 2) {
                int current_height = std::min(STRIPE_EVERY_PX, roi_height - y);
                cv::Mat empty_mask_roi = empty_mask(cv::Rect(0, 0, roi_width, current_height));
                cv::Mat image_roi_roi = image_roi(cv::Rect(0, y, roi_width, current_height));
                empty_mask_roi.copyTo(image_roi_roi);
            }
        }

        bool radar_used_atleast_once = false;

        for (int y = 0; y < roi_height; y += check_radar_dist_every_px) {
            for (int x = 0; x < roi_width; x += check_radar_dist_every_px) {
                int width_current = std::min(check_radar_dist_every_px, roi_width - x);
                int height_current = std::min(check_radar_dist_every_px, roi_height - y);

                double cen_x = roi_x_start + (2 * x + width_current) / 2.0;
                double cen_y = roi_y_start + (2 * y + height_current) / 2.0;

                double lat = boundaries[0] - (boundaries[0] - boundaries[2]) * cen_y / height;
                double lon = boundaries[1] + (boundaries[3] - boundaries[1]) * cen_x / width;

                unsigned int closest_index = 0;
                double prev_distance = UINT_MAX;
                double current_distance = 0;

                for (int i = 0; i < radars.size(); i++) {
                    auto data = radars.at(i);

                    double lat_dist = abs(data.lat - lat);
                    double lon_dist = abs(data.lon - lon);
                    double dist = lat_dist * lat_dist + lon_dist * lon_dist;

                    if (radars.at(i).kode == d.kode) {
                        current_distance = dist;
                    }

                    if (dist < prev_distance)
                        prev_distance = dist, closest_index = i;
                }

                // if the distance difference between the closest and current radar
                // is the same or less than check_radar_dist_every_px
                // we can just ignore it
                // why? you see, we loop through the distance via the radar
                // bounds. not the map bounds, so there might be places
                // where it'll be always empty (not good)

                // in this case, we'll use the width as the reference
                double dist = width * abs(current_distance - prev_distance) / (boundaries[3] - boundaries[1]);

                if (radars.at(closest_index).kode == d.kode || dist <= check_radar_dist_every_px) {
                    cv::Mat image_roi_current = image_roi(cv::Rect(x, y, width_current, height_current));
                    cv::Mat container_roi_current = container_roi(cv::Rect(x, y, width_current, height_current));

                    image_roi_current.copyTo(container_roi_current);
                    radar_used_atleast_once = true;
                }
            }
        }

        if (radar_used_atleast_once) {
            // lesson learned: do not create a pointer to a reference, it's gonna be... weird
            used_radars.push_back(&(radars[i]));
        }
    }

    // color scheme replace
    for (int row = 0; row < container.rows; row++) {
        for (int col = 0; col < container.cols; col++) {
            cv::Vec4b &pixelValue = container.at<cv::Vec4b>(row, col);

            uchar &blue = pixelValue[0];
            uchar &green = pixelValue[1];
            uchar &red = pixelValue[2];

            for (int i = 0; i < radars.at(0).colors.size(); i++) {
                auto c = radars.at(0).colors.at(i);
                if (red == c.r && green == c.g && blue == c.b) {
                    if (radar::ColorScheme.size() <= i)
                        break;
                    auto current = radar::ColorScheme.at(i);
                    red = current.r;
                    green = current.g;
                    blue = current.b;
                }
            }
        }
    }

    return container;
}

std::vector<radar::RadarImage> &radar::Imagery::get_radar_datas() {
    if (radar_datas.size() != 0) {
        return radar_datas;
    }

    std::string content = fetch::get(RADAR_LIST_API_URL);

    std::vector<radar::RadarList> list;

    json list_data;
    try {
        list_data = json::parse(content);
    } catch (const json::parse_error &e) {
        std::string err(e.what());
        throw std::runtime_error("Error parsing JSON: " + err);
    }

    for (auto &radar : list_data) {
        auto tlc_raw = radar["overlayTLC"];
        auto brc_raw = radar["overlayBRC"];

        double north = std::stod(static_cast<std::string>(tlc_raw[0]));
        double west = std::stod(static_cast<std::string>(tlc_raw[1]));
        double south = std::stod(static_cast<std::string>(brc_raw[0]));
        double east = std::stod(static_cast<std::string>(brc_raw[1]));

        std::string kota = static_cast<std::string>(radar["Kota"]);
        std::string stasiun = static_cast<std::string>(radar["Stasiun"]);
        std::string kode = static_cast<std::string>(radar["kode"]);
        double lat = radar["lat"];
        double lon = radar["lon"];

        radar::RadarList radar_data;
        radar_data.boundaries = {north, west, south, east};
        radar_data.kota = kota;
        radar_data.stasiun = stasiun;
        radar_data.kode = kode;
        radar_data.lat = lat;
        radar_data.lon = lon;

        list.push_back(radar_data);
    }

    std::string runtime_error("");
    std::vector<std::thread> jobs;
    std::mutex mtx;

    int count = 0;
    for (auto &radar : list) {
        // excluded radar
        if (std::find(exclude_radar.begin(), exclude_radar.end(), radar.kode) != exclude_radar.end())
            continue;

        bool in_range = radar::is_overlapping(boundaries, radar.boundaries);
        if (!in_range)
            continue;

        std::string code = radar.kode;

        std::thread job(
            [this, code, &mtx, &runtime_error, count] { this->fetch_detailed_data(code, mtx, runtime_error, count); });
        jobs.push_back(std::move(job));

        count++;
    }

    for (auto &job : jobs) {
        if (job.joinable()) {
            job.join();
        }
    }

    return radar_datas;
}

radar::Color radar::parseHexColor(const std::string &hexColor) {
    radar::Color color;
    if (hexColor[0] == '#') {
        unsigned int rgb;
        std::stringstream ss;
        ss << std::hex << hexColor.substr(1);
        ss >> rgb;

        color.r = (rgb >> 16) & 0xFF;
        color.g = (rgb >> 8) & 0xFF;
        color.b = rgb & 0xFF;
    }
    return color;
}

void radar::Imagery::fetch_detailed_data(std::string code, std::mutex &mtx, std::string &runtime_error, int index) {
    char *token_get = std::getenv("token");
    std::string token = std::string(token_get == NULL ? "" : token_get);

    std::string URL = token == "" ? radar::RADAR_IMAGE_PUBLIC_API_URL : radar::RADAR_IMAGE_API_URL;
    URL += "?radar=" + curlpp::escape(code);

    if (token != "") {
        URL += "&token=" + curlpp::escape(token);
    }

    std::string content;

    try {
        content = fetch::get(URL);
    } catch (std::runtime_error &e) {
        runtime_error = e.what();
        return;
    }

    json parsed_data;
    try {
        parsed_data = json::parse(content);
    } catch (const json::parse_error &e) {
        std::string err(e.what());
        runtime_error = ("Error parsing JSON: " + err);
        return;
    }

    if (parsed_data.is_null()) {
        std::string URL = token == "" ? radar::RADAR_IMAGE_PUBLIC_API_URL : radar::RADAR_IMAGE_API_URL;
        throw std::runtime_error("API " + URL + " returned NULL");
    }

    radar::RadarImage radar_data;
    if (parsed_data["Latest"]["timeUTC"] == "No Data") {
        return;
    }

    auto tlc_raw = parsed_data["bounds"]["overlayTLC"];
    auto brc_raw = parsed_data["bounds"]["overlayBRC"];

    double north = std::stod(static_cast<std::string>(tlc_raw[0]));
    double west = std::stod(static_cast<std::string>(tlc_raw[1]));
    double south = std::stod(static_cast<std::string>(brc_raw[0]));
    double east = std::stod(static_cast<std::string>(brc_raw[1]));

    radar_data.boundaries = {north, west, south, east};
    radar_data.kode = parsed_data["bounds"]["kode"];
    radar_data.kota = parsed_data["bounds"]["Kota"];
    radar_data.stasiun = parsed_data["bounds"]["Stasiun"];
    radar_data.lat = parsed_data["bounds"]["lat"];
    radar_data.lon = parsed_data["bounds"]["lon"];

    auto last_1h = parsed_data["LastOneHour"];
    std::vector<std::chrono::system_clock::time_point> time;
    std::vector<std::string> file;

    for (auto &color : parsed_data["legends"]["colors"]) {
        radar_data.colors.push_back(parseHexColor(color));
    }

    for (int i = 0; i < last_1h["file"].size(); i++) {
        std::istringstream in{static_cast<std::string>(last_1h["timeUTC"][i])};
        std::chrono::system_clock::time_point tp;
        in >> date::parse("%Y-%m-%d %H:%M %Z", tp);
        std::string filename = last_1h["file"][i];

        time.push_back(tp);
        file.push_back(filename);
    }

    // ignore if the data is old, if the user specifies so
    auto seconds_to_now = std::time(nullptr) - std::chrono::system_clock::to_time_t(time.back());
    if (seconds_to_now > (declare_old_after_mins * 60) && ignore_old_radars)
        return;

    radar_data.data.file = file;
    radar_data.data.time = time;
    mtx.lock();
    radar_datas.push_back(radar_data);
    mtx.unlock();
}