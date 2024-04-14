set (
    SOURCES
    "main.cpp"
    "request.cpp"
    "processor.cpp"
    "img.cpp"
)

add_executable(RadarWorker ${SOURCES})

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
