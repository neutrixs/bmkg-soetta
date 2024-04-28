#include "../command.hpp"
#include "../command_manager.hpp"
#include <dpp/dpp.h>
#include <iostream>

namespace commands {
namespace render {
void run(dpp::cluster &bot, const dpp::slashcommand_t &event) {
    event.thinking(false);
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
}