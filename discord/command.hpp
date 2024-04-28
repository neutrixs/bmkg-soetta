#ifndef COMMAND_HPP
#define COMMAND_HPP
#include <dpp/dpp.h>
#include <string>
#include <vector>

struct command {
    std::string name;
    std::string description;
    std::vector<dpp::command_option> options;
    std::function<void(dpp::cluster &bot, const dpp::slashcommand_t &event)> run;
};

#endif