#include "../command.hpp"
#include "../command_manager.hpp"
#include <array>
#include <dpp/dpp.h>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <radarworker/radarworker.hpp>

namespace commands {
namespace render {
static std::string write_info(radar::Imagery &imagery) {
    std::string output;
    size_t count = imagery.used_radars.size();
    switch (count) {
    case 0:
        output += "No radars used. Likely out of bounds or outdated datas.";
        break;
    case 1:
        output += "Used radar:\n";
        break;
    default:
        output += "Used radars:\n";
        break;
    }

    for (auto &radar : imagery.used_radars) {
        std::string name = radar->stasiun;
        auto time = radar->data.time.back().time_since_epoch();
        long epoch = std::chrono::duration_cast<std::chrono::seconds>(time).count();

        std::string not_enough = "\netc...";
        std::string format = name + ": <t:" + std::to_string(epoch) + ":R>\n";

        // discord character limit
        if (output.size() + format.size() + not_enough.size() <= 2000) {
            output += format;
        } else {
            output += not_enough;
            break;
        }
    }

    return output;
}

void run(dpp::cluster &bot, const dpp::slashcommand_t &event) {
    event.thinking(false, [event](const dpp::confirmation_callback_t &callback) {
        if (callback.is_error()) {
            std::cerr << callback.get_error().human_readable << std::endl;
            return;
        }

        std::string place;
        bool ignore_old;
        try {
            place = std::get<std::string>(event.get_parameter("place"));
            ignore_old = std::get<bool>(event.get_parameter("ignore_old"));
        } catch (std::bad_variant_access &e) {
            ignore_old = false;
        }

        std::array<double, 4> bounding;
        try {
            bounding = map::OSM_get_bounding_box(place);
        } catch (std::runtime_error &e) {
            event.edit_original_response(dpp::message(e.what()));
            return;
        }

        map::Tiles tiles(bounding[0], bounding[1], bounding[2], bounding[3]);
        radar::Imagery imagery;
        imagery.exclude_radar = {"CGK"};
        imagery.ignore_old_radars = ignore_old;

        cv::Mat image;

        try {
            image = tiles.render_with_overlay_radar(imagery);
        } catch (std::runtime_error &e) {
            dpp::message msg(e.what());
            event.edit_original_response(msg);
            return;
        }

        std::vector<uchar> buf;
        cv::imencode(".png", image, buf);

        dpp::message msg(write_info(imagery));
        msg.set_file_content(std::string(buf.begin(), buf.end()));
        msg.set_filename("radar.png");

        event.edit_original_response(msg);
    });
}

void init() {
    command cmd;
    cmd.name = "render";
    cmd.description = "render the weather radar for a specific area";
    cmd.options = {
        dpp::command_option(dpp::co_string, "place", "Set the area of the radar", true),
        dpp::command_option(dpp::co_boolean, "ignore_old", "Do not use old radar datas", false) //
    };
    cmd.run = run;

    GlobalCommandManager.register_command(cmd.name, cmd);
}
} // namespace render
} // namespace commands