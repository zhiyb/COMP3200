#ifndef CV_PRIVATE_H
#define CV_PRIVATE_H

#include "global.h"

extern struct cv_gpu_t {
	semaphore_t smpr;
	uint64_t ts;
	cv::Mat input, grey, mask;
} cv_gpu;

void cvThread_CPU();

#endif
