#ifndef CAPTURE_H
#define CAPTURE_H

#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>

int captureInit(const char *devfile, int width, int height);
cv::Mat captureQuery();
cv::gpu::GpuMat captureQueryGPU();
void captureClose();

#endif
