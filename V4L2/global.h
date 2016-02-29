#ifndef GLOBAL_H
#define GLOBAL_H

#include <mutex>
#include "yavta.h"
#include "ov5647_v4l2.h"

// Preview
#define ENABLE_PV	1
// OpenCV
#define ENABLE_CV	1

#define MAX_W		OV5647_MAX_W
#define MAX_H		OV5647_MAX_H
#define BUFFER_NUM	(3 + ENABLE_PV + ENABLE_CV)

enum {	REQUEST_NONE = 0x00,
	REQUEST_CAPTURE,
	REQUEST_QUIT,
	REQUEST_SWAP,
};

extern struct status_t {
	unsigned int width, height, pixelformat;
	volatile unsigned int request;
	volatile bool swap;
	// Video, CV, Preview FPS
	volatile float vFPS, cvFPS, pvFPS;
} status;

// V4L2
extern struct device dev;
extern volatile unsigned int bufidx;

struct thread_t {
	std::mutex mtx;
	volatile int bufidx;
	volatile int err;
};
#if ENABLE_PV
extern struct thread_t pvData;
#endif
#if ENABLE_CV
extern struct thread_t cvData;
#endif

void inputThread();
void cvThread();
void pvThread();

#endif
