#ifndef PROCESSOR_HPP
#define PROCESSOR_HPP

#include <opencv2/opencv.hpp>
#include <string>

namespace processor {
std::string render(double north, double west, double south, double east, int zoom);
cv::Mat renderRadar(double north, double west, double south, double east, int zoom, int width, int height);
void overlayImage(cv::Mat* src, cv::Mat* overlay, const cv::Point& location);
}  // namespace processor

#endif