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

pkg_check_modules(OPENCV4 REQUIRED opencv4)
include_directories("${OPENCV4_INCLUDE_DIRS}")
target_link_libraries(Discord "${OPENCV4_LIBRARIES}")

pkg_check_modules(CURLPP REQUIRED curlpp)
include_directories("${CURLPP_INCLUDE_DIRS}")
target_link_libraries(Discord ${CURLPP_LIBRARIES})

find_package(Boost REQUIRED COMPONENTS filesystem)
include_directories("${Boost_INCLUDE_DIRS}")
target_link_libraries(Discord ${Boost_LIBRARIES})