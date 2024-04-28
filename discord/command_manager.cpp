#include "command_manager.hpp"

void CommandManager::register_command(const std::string &name, command cmd) {
    commands[name] = cmd;
}

command CommandManager::get_command(const std::string &name) {
    return commands[name];
}

CommandManager GlobalCommandManager;