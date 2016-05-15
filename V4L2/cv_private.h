#ifndef CV_PRIVATE_H
#define CV_PRIVATE_H

#include "global.h"

extern struct cv_gpu_t {
	semaphore_t smpr;
	//uint64_t ts;
	struct timeval ts;
	cv::Mat input, grey, mask;
	void *raw;
} cv_gpu;

void cvThread_CPU();

#endif
