#ifndef COMMAND_MANAGER_HPP
#define COMMAND_MANAGER_HPP
#include "command.hpp"
#include <map>

class CommandManager {
  public:
    void register_command(const std::string &name, command cmd);
    command get_command(const std::string &name);
    std::map<std::string, command> commands;
};

extern CommandManager GlobalCommandManager;

#endif