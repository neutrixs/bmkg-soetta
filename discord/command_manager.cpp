#include "command_manager.hpp"

void CommandManager::register_command(const std::string &name, command cmd) {
    commands[name] = cmd;
}

command CommandManager::get_command(const std::string &name) {
    if (commands.find(name) == commands.end()) {
        std::string message = "Command " + name + " not found";
        throw std::runtime_error(message);
    } else {
        return commands[name];
    }
}

CommandManager GlobalCommandManager;