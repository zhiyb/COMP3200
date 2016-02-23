#ifndef GLOBAL_H
#define GLOBAL_H

#include <mutex>
#include "yavta.h"
#include "ov5647_v4l2.h"

#define MAX_W	OV5647_MAX_W
#define MAX_H	OV5647_MAX_H

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
extern struct v4l2_buffer buf[2];
extern volatile unsigned int bufidx;

extern struct thread_t {
} pvThreadInfo;

void inputThread();
void cvThread();
void pvThread();

#endif
