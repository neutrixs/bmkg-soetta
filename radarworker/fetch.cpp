
#include <chrono>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <radarworker/fetch.hpp>
#include <sstream>
#include <thread>

std::string fetch::get(std::string url, std::list<std::string> headers) {
    curlpp::initialize();
    std::stringstream response;
    std::stringstream *rp = &response;

    std::string runtime_error("");
    std::string *re = &runtime_error;

    bool done = false;
    bool *dp = &done;

    std::thread job([url, rp, re, dp, headers] {
        try {
            curlpp::Easy req;
            curlpp::options::Url url_options(url);
            curlpp::options::SslVerifyPeer ssl_verify_peer(false);
            curlpp::options::SslVerifyHost ssl_verify_host(false);
            curlpp::options::HttpHeader http_header(headers);

            req.setOpt(url_options);

            // well, it seems like it doesn't recognize ssl certificate on non-443 port
            // whatever, i don't care
            req.setOpt(ssl_verify_peer);
            req.setOpt(ssl_verify_host);
            req.setOpt(http_header);

            curlpp::options::WriteStream write(rp);
            req.setOpt(write);

            req.perform();
        } catch (curlpp::RuntimeError &e) {
            *re = e.what();
        } catch (curlpp::LogicError &e) {
            *re = e.what();
        }
        *dp = true;
    });

    auto start_time = std::chrono::high_resolution_clock::now();
    while (true) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        if (elapsed.count() > 20000 && !done) {
            job.detach();
            runtime_error = "HTTP Request timeout";
            throw runtime_error;
        }

        if (done)
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    job.join();

    if (runtime_error != "") {
        throw std::runtime_error(runtime_error);
    }

    return response.str();
}