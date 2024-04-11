#include <curlpp/cURLpp.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Easy.hpp>
#include <list>
#include "request.hpp"

std::stringstream request::fetch(std::string url) {
    curlpp::initialize();
    std::stringstream response;

    // fake headers to bypass the limitation
    // this is enough
    std::list<std::string> headers;
    headers.push_back("User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/123.0.0.0 Safari/537.36");
    headers.push_back("Referer: https://www.openstreetmap.org/");
    headers.push_back("Accept: image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8");
    headers.push_back("Accept-Encoding: gzip, deflate, br, zstd");
    headers.push_back("Sec-Ch-Ua: \"Google Chrome\";v=\"123\", \"Not:A-Brand\";v=\"8\", \"Chromium\";v=\"123\"");

    try {
        curlpp::Easy req;

        req.setOpt(new curlpp::options::Url(url));
        req.setOpt(new curlpp::options::HttpHeader(headers));

        curlpp::options::WriteStream write(&response);
        req.setOpt(write);

        req.perform();
    }
    catch (curlpp::RuntimeError &e) {
        std::cerr << e.what() << std::endl;
    }
    catch (curlpp::LogicError &e) {
        std::cerr << e.what() << std::endl;
    }

    return response;
}