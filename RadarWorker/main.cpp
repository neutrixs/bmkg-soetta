#include <fstream>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <tuple>

#include "map.hpp"

int main(int argc, char** argv) {
    std::string desc = "Usage: app place name";
    if (argc < 2) {
        std::cout << desc << std::endl;
        return 1;
    }

    std::string input = "";
    for (int i = 1; i < argc; i++) {
        if (i != 1) input += " ";
        input += argv[i];
    }

    auto bounding_box = map::OSM_get_bounding_box(input);
    map::Tiles tiles(bounding_box[0], bounding_box[1], bounding_box[2], bounding_box[3]);
    cv::Mat rendered = tiles.render_with_overlay_radar(0.8f);

    std::vector<uchar> buf;
    cv::imencode(".png", rendered, buf);

    std::string b(buf.begin(), buf.end());

    std::ofstream fileb("map.png", std::ios::binary);
    fileb.write(b.c_str(), b.size());
    fileb.close();

    return 0;
}