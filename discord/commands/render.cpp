#include "../command.hpp"
#include "../command_manager.hpp"
#include <array>
#include <dpp/dpp.h>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <radarworker/radarworker.hpp>

namespace commands {
namespace render {
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
        imagery.ignore_old_radars = ignore_old;

        cv::Mat image = tiles.render_with_overlay_radar(imagery);

        std::vector<uchar> buf;
        cv::imencode(".png", image, buf);

        dpp::message msg("");
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