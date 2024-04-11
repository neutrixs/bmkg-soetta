set (
    SOURCES
    "main.cpp"
)

find_package(OpenCV REQUIRED)

add_executable(RadarWorker ${SOURCES})

include_directories("${OpenCV_INCLUDE_DIRS}")
target_link_libraries(RadarWorker "${OpenCV_LIBS}")

message("${OpenCV_INCLUDE_DIRS}")