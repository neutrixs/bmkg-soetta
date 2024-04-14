#include "request.hpp"

#include <cmath>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <iostream>
#include <list>

std::stringstream request::fetch(std::string url, bool osm_fake_headers) {
    curlpp::initialize();
    std::stringstream response;

    // fake headers to bypass the limitation
    // this is enough
    std::list<std::string> headers;
    if (osm_fake_headers) {
        headers.push_back(
            "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
            "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/123.0.0.0 "
            "Safari/537.36");
        headers.push_back("Referer: https://www.openstreetmap.org/");
        headers.push_back(
            "Accept: "
            "image/avif,image/webp,image/apng, text/html,image/svg+xml,image/*,*/*;q=0.8");
        headers.push_back("Accept-Encoding: gzip, deflate, br, zstd");
        headers.push_back(
            "Sec-Ch-Ua: \"Google Chrome\";v=\"123\", \"Not:A-Brand\";v=\"8\", "
            "\"Chromium\";v=\"123\"");
    }

    try {
        curlpp::Easy req;

        req.setOpt(new curlpp::options::Url(url));
        req.setOpt(new curlpp::options::HttpHeader(headers));

        curlpp::options::WriteStream write(&response);
        req.setOpt(write);

        req.perform();
    } catch (curlpp::RuntimeError &e) {
        std::cerr << e.what() << std::endl;
    } catch (curlpp::LogicError &e) {
        std::cerr << e.what() << std::endl;
    }

    return response;
}

std::tuple<double, double> request::coordToTile(double lat, double lon, int zoom) {
    double n = powf64(2, zoom);
    double x = n * ((lon + 180) / 360);
    double y = n * (1 - (log(tan(lat * M_PI / 180.0) + 1 / cos(lat * M_PI / 180.0)) / M_PI)) / 2.0;

    return std::tuple<double, double>(x, y);
}

std::tuple<double, double> request::tileToCoord(double x, double y, int zoom) {
    double n = powf64(2, zoom);
    double lon_deg = x / n * 360.0 - 180.0;
    double lat_rad = atan(sinh(M_PI * (1 - 2 * y / n)));
    double lat_deg = lat_rad * 180.0 / M_PI;

    return std::tuple<double, double>(lat_deg, lon_deg);
}

std::tuple<double, double, double, double> request::getTilesRange(double n, double w, double s, double e, int zoom) {
    auto start = coordToTile(n, w, zoom);
    auto wtile = std::get<0>(start);
    auto ntile = std::get<1>(start);

    auto end = coordToTile(s, e, zoom);
    auto etile = std::get<0>(end);
    auto stile = std::get<1>(end);

    return std::tuple<double, double, double, double>(ntile, wtile, stile, etile);
}

std::stringstream radar::getLatest() {
    const std::string API_URL = "https://meteosoetta.com/radaronline/datradcgkapi_x1.php?lop=1";
    const std::string IMAGE_BASE_URL = "https://meteosoetta.com/gambar/radarimage/";
    const std::string REFERER = "https://meteosoetta.com/";

    auto urlData = request::fetch(API_URL, false);
    auto imgURL = urlData.str();

    auto pos = imgURL.find(";");
    imgURL = imgURL.substr(0, pos);

    return request::fetch(IMAGE_BASE_URL + imgURL, false);
}