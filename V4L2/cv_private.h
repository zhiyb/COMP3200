#ifndef CV_PRIVATE_H
#define CV_PRIVATE_H

#include "global.h"

extern struct cv_gpu_t {
	semaphore_t smpr;
	cv::Mat input, mask;
} cv_gpu;

void cvThread_CPU();

#endif
