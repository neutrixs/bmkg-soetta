#include "processor.hpp"

#include <boost/filesystem.hpp>
#include <cmath>
#include <fstream>
#include <ios>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <tuple>
#include <vector>

#include "img.hpp"
#include "request.hpp"

namespace fs = boost::filesystem;

std::string processor::render(double north, double west, double south, double east, int zoom) {
    auto tilesRange = request::getTilesRange(north, west, south, east, zoom);

    int trnorth = static_cast<int>(floor(std::get<0>(tilesRange)));
    int trwest = static_cast<int>(floor(std::get<1>(tilesRange)));
    int trsouth = static_cast<int>(ceil(std::get<2>(tilesRange)));
    int treast = static_cast<int>(ceil(std::get<3>(tilesRange)));

    double ncoordtile;
    double wcoordtile;
    double scoordtile;
    double ecoordtile;

    auto coordtilestart = request::coordToTile(north, west, zoom);
    auto coordtileend = request::coordToTile(south, east, zoom);

    wcoordtile = std::get<0>(coordtilestart);
    ncoordtile = std::get<1>(coordtilestart);
    ecoordtile = std::get<0>(coordtileend);
    scoordtile = std::get<1>(coordtileend);

    int rows = trsouth - trnorth;
    int cols = treast - trwest;

    std::vector<std::string> maptiles;

    const std::string BASE_URL = "https://tile.openstreetmap.org/";

    auto tmp_dir = fs::temp_directory_path();
    tmp_dir.append("bmkg-soetta");
    fs::create_directories(tmp_dir);

    for (int i = trnorth, count = 1; i < trsouth; i++) {
        for (int j = trwest; j < treast; j++, count++) {
            printf("\rDownloading tiles... %d/%d", count, (trsouth - trnorth) * (treast - trwest));

            std::string URL_SCHEME =
                BASE_URL + std::to_string(zoom) + "/" + std::to_string(j) + "/" + std::to_string(i) + ".png";

            std::hash<std::string> hasher;
            size_t hash_value = hasher(URL_SCHEME);

            std::stringstream ss;
            ss << std::hex << hash_value;

            fs::path PATH_SCHEME = tmp_dir / ss.str();
            bool cache_exists = fs::exists(PATH_SCHEME);

            std::string data;
            if (cache_exists) {
                std::ifstream file(PATH_SCHEME, std::ios::binary);
                if (!file.is_open()) {
                    goto redownload;
                }

                std::vector<char> buffer(std::istreambuf_iterator<char>(file), {});
                data = std::string(buffer.begin(), buffer.end());

                file.close();
            } else {
            redownload:
                data = request::fetch(URL_SCHEME, true).str();

                std::ofstream file(PATH_SCHEME, std::ios::binary);
                file.write(data.c_str(), data.size());
                file.close();
            }

            maptiles.push_back(data);
        }
    }
    printf("\n");

    // get resolution of the first one as a reference
    // it means the other pictures will have the same resolutions
    auto resolution = png::get_resolution(maptiles[0]);
    int width = std::get<0>(resolution);
    int height = std::get<1>(resolution);

    int canvas_width = width * cols;
    int canvas_height = height * rows;

    cv::Mat canvas = cv::Mat::zeros(canvas_height, canvas_width, CV_8UC3);

    for (int row = 0, count = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++, count++) {
            auto current = maptiles.at(count);
            std::vector<uchar> buffer(current.begin(), current.end());

            cv::Mat img = cv::imdecode(buffer, cv::IMREAD_COLOR);

            cv::Mat insetImage(canvas, cv::Rect(width * col, height * row, width, height));
            img.copyTo(insetImage);
        }
    }

    int croptop = int((ncoordtile - floor(ncoordtile)) * height);
    int cropleft = int((wcoordtile - floor(wcoordtile)) * width);
    int cropbot = int((ceil(scoordtile) - scoordtile) * height);
    int cropright = int((ceil(ecoordtile) - ecoordtile) * width);

    int cropped_width = canvas_width - cropleft - cropright;
    int cropped_height = canvas_height - croptop - cropbot;

    cv::Mat cropped_canvas;

    cv::Mat insetImage(canvas, cv::Rect(cropleft, croptop, cropped_width, cropped_height));
    insetImage.copyTo(cropped_canvas);

    // turn cropped_canvas to an image with alpha
    cv::Mat alpha(cropped_canvas.size(), CV_8UC1, cv::Scalar(255));
    cv::Mat base;
    std::vector<cv::Mat> channels = {cropped_canvas, alpha};
    cv::merge(channels, base);
    // set brightness to just 70%;
    for (int i = 0; i < base.rows; i++) {
        for (int j = 0; j < base.cols; j++) {
            cv::Vec4b& v = base.at<cv::Vec4b>(i, j);
            v[0] *= 0.7;
            v[1] *= 0.7;
            v[2] *= 0.7;
        }
    }

    // we don't need the original canvas anymore
    canvas.release();

    cv::Mat radarImage = renderRadar(north, west, south, east, zoom, cropped_width, cropped_height);
    // make the radar 60% opacity?
    for (int i = 0; i < radarImage.rows; i++) {
        for (int j = 0; j < radarImage.cols; j++) {
            cv::Vec4b& v = radarImage.at<cv::Vec4b>(i, j);
            v[3] *= 0.6;
        }
    }

    overlayImage(&base, &radarImage, cv::Point(0, 0));

    cv::Mat no_transparency;
    cv::cvtColor(base, no_transparency, cv::COLOR_BGRA2BGR);

    std::vector<uchar> buffer;
    cv::imencode(".png", no_transparency, buffer);

    std::string output(buffer.begin(), buffer.end());
    return output;
}  // namespace boost::filesystem

cv::Mat processor::renderRadar(double north, double west, double south, double east, int zoom, int width, int height) {
    std::string raw_img = radar::getLatest().str();
    std::vector<uchar> buffer(raw_img.begin(), raw_img.end());

    auto resolution = png::get_resolution(raw_img);
    int radar_width = std::get<0>(resolution);
    int radar_height = std::get<1>(resolution);

    cv::Mat img = cv::imdecode(buffer, cv::IMREAD_UNCHANGED);

    double croptop = (north - radar::bounds::north) / (radar::bounds::south - radar::bounds::north) * radar_height;
    int croptopfloor = floor(croptop);

    double cropleft = (west - radar::bounds::west) / (radar::bounds::east - radar::bounds::west) * radar_width;
    int cropleftfloor = floor(cropleft);

    double cropbot = (radar::bounds::south - south) / (radar::bounds::south - radar::bounds::north) * radar_height;
    int cropbotceil = ceil(cropbot);

    double cropright = (radar::bounds::east - east) / (radar::bounds::east - radar::bounds::west) * radar_width;
    int croprightceil = ceil(cropright);

    int cropped_width = radar_width - cropleftfloor - croprightceil;
    int cropped_height = radar_height - croptopfloor - cropbotceil;

    cv::Mat cropped_canvas = cv::Mat::zeros(cropped_height, cropped_width, CV_8UC4);
    cv::Mat inset(img, cv::Rect(cropleftfloor, croptopfloor, cropped_width, cropped_height));

    inset.copyTo(cropped_canvas);

    int placeholder_width = width + width * (((cropleft - cropleftfloor) + (croprightceil - cropright)) / cropped_width);
    int placeholder_height = height + height * (((croptop - croptopfloor) + (cropbotceil - cropbot)) / cropped_height);

    cv::Mat resized_unprocessed;
    cv::resize(cropped_canvas, resized_unprocessed, cv::Size(placeholder_width, placeholder_height), 0, 0, cv::INTER_NEAREST);

    cv::Mat resized_processed = cv::Mat::zeros(height, width, CV_8UC4);
    int trimleft = (cropleft - cropleftfloor) / cropped_width * width;
    int trimtop = (croptop - croptopfloor) / cropped_height * height;

    cv::Mat resized_inset(resized_unprocessed, cv::Rect(trimleft, trimtop, width, height));
    resized_inset.copyTo(resized_processed);

    return resized_processed;
}

// https://answers.opencv.org/question/73016/how-to-overlay-an-png-image-with-alpha-channel-to-another-png/
void processor::overlayImage(cv::Mat* src, cv::Mat* overlay, const cv::Point& location) {
    for (int y = (location.y, 0); y < src->rows; ++y) {
        int fY = y - location.y;

        if (fY >= overlay->rows) break;

        for (int x = std::max(location.x, 0); x < src->cols; ++x) {
            int fX = x - location.x;

            if (fX >= overlay->cols) break;

            double opacity = ((double)overlay->data[fY * overlay->step + fX * overlay->channels() + 3]) / 255;

            for (int c = 0; opacity > 0 && c < src->channels(); ++c) {
                unsigned char overlayPx = overlay->data[fY * overlay->step + fX * overlay->channels() + c];
                unsigned char srcPx = src->data[y * src->step + x * src->channels() + c];
                src->data[y * src->step + src->channels() * x + c] = srcPx * (1. - opacity) + overlayPx * opacity;
            }
        }
    }
}