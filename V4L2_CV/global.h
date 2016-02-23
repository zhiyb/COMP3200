#ifndef GLOBAL_H
#define GLOBAL_H

#include "yavta.h"

enum {	REQUEST_NONE = 0x00,
	REQUEST_CAPTURE,
	REQUEST_QUIT,
	REQUEST_SWAP,
};

extern struct status_t {
	unsigned int width, height, pixelformat;
	volatile unsigned int request;
	volatile bool swap;
} status;

// V4L2
extern struct device dev;
extern struct v4l2_buffer buf[2];
extern volatile unsigned int bufidx;

void inputThread();
void cvThread();

#endif
