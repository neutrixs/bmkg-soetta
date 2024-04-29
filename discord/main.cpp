#include "command.hpp"
#include "command_manager.hpp"
#include "commands/render.hpp"
#include <dpp/dpp.h>

void loader() {
    commands::render::init();
}

void register_commands(dpp::cluster &bot) {
    std::vector<dpp::slashcommand> commands_list;

    for (auto &data : GlobalCommandManager.commands) {
        std::string name = data.first;
        command cmd = data.second;

        dpp::slashcommand cmd_data(name, cmd.description, bot.me.id);
        for (auto &option : cmd.options) {
            cmd_data.add_option(option);
        }

        commands_list.push_back(cmd_data);
    }

    // delete command if it doesn't exist locally
    auto existing_commands = bot.global_commands_get_sync();
    for (auto &existing_command : existing_commands) {
        std::string name = existing_command.second.name;
        try {
            GlobalCommandManager.get_command(name);
        } catch (std::runtime_error &e) {
            bot.global_command_delete_sync(existing_command.first);
        }
    }

    for (auto &cmd : commands_list) {
        bot.global_command_create(cmd);
    }
}

int main() {
    char *raw_env = std::getenv("discord_token");
    std::string token(raw_env == NULL ? "" : raw_env);

    if (token == "") {
        std::cerr << "discord_token env required" << std::endl;
        return 1;
    }

    loader();

    dpp::cluster bot(token);
    bot.on_log(dpp::utility::cout_logger());
    bot.on_ready([&bot](const dpp::ready_t &event) {
        if (dpp::run_once<struct reg_commands>()) {
            register_commands(bot);
        }
    });
    bot.on_slashcommand([&bot](const dpp::slashcommand_t &event) {
        command current_cmd;
        try {
            current_cmd = GlobalCommandManager.get_command(event.command.get_command_name());
        } catch (std::runtime_error &e) {
            event.reply(dpp::message(e.what()).set_flags(dpp::m_ephemeral));
            return;
        }

        current_cmd.run(bot, event);
    });

    bot.start(dpp::st_wait);

    return 0;
}