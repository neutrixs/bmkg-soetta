cmake_minimum_required(VERSION 3.9)
project(radarworker VERSION 0.1 DESCRIPTION "worker for radar")

set (
    SOURCES
    "map.cpp"
    "png.cpp"
    "radar.cpp"
    "fetch.cpp"
)

include_directories("../include")

add_executable(RadarWorker main.cpp ${SOURCES})
add_library(radarworker STATIC ${SOURCES})

include(GNUInstallDirs)
install(TARGETS radarworker LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY ../include/radarworker
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_file(radarworker.pc.in radarworker.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/radarworker.pc DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)

include(FindPkgConfig)

pkg_check_modules(OPENCV4 REQUIRED opencv4)
include_directories("${OPENCV4_INCLUDE_DIRS}")
target_link_libraries(RadarWorker "${OPENCV4_LIBRARIES}")

pkg_check_modules(CURLPP REQUIRED curlpp)
include_directories("${CURLPP_INCLUDE_DIRS}")
target_link_libraries(RadarWorker ${CURLPP_LIBRARIES})

find_package(Boost REQUIRED COMPONENTS filesystem)
include_directories("${Boost_INCLUDE_DIRS}")
target_link_libraries(RadarWorker ${Boost_LIBRARIES})