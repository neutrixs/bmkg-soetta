cmake_minimum_required(VERSION 3.9)
project(discord VERSION 0.1 DESCRIPTION "discord bot for the radar")

set (
    SOURCES
    "main.cpp"
    "command_manager.cpp"
    "commands/render.cpp"
)

include_directories("../include")

add_executable(Discord ${SOURCES})

include(FindPkgConfig)

pkg_check_modules(DPP REQUIRED dpp)
include_directories("${DPP_INCLUDE_DIRS}")
target_link_libraries(Discord "${DPP_LIBRARIES}")

pkg_check_modules(radarworker REQUIRED radarworker)
include_directories("${radarworker_INCLUDE_DIRS}")
target_link_libraries(Discord "${radarworker_LIBRARIES}")