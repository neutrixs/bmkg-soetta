set (
    SOURCES
    "main.cpp"
    "request.cpp"
)

add_executable(RadarWorker ${SOURCES})

find_package(OpenCV REQUIRED)
include_directories("${OpenCV_INCLUDE_DIRS}")
target_link_libraries(RadarWorker "${OpenCV_LIBS}")

include(FindPkgConfig)
pkg_check_modules(CURLPP REQUIRED curlpp)
include_directories("${CURLPP_INCLUDE_DIRS}")
target_link_libraries(RadarWorker ${CURLPP_LIBRARIES})

message("${CURLPP_LIBRARIES}")