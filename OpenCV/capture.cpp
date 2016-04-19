#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <opencv/cv.h>
#include <opencv/highgui.h>

#include "yavta.h"
#include "ov5647_v4l2.h"

using namespace std;
using namespace cv;

#define MAX_W		OV5647_MAX_W
#define MAX_H		OV5647_MAX_H
#define BUFFER_NUM	(3 + 1)

enum {	REQUEST_NONE = 0x00,
	REQUEST_QUIT,
};

// V4L2
static struct device dev;
//struct v4l2_buffer buf;
static volatile unsigned int bufidx = 0;

static struct status_t {
	unsigned int width, height, pixelformat;
	volatile unsigned int request;
	volatile bool swap;
	// Video FPS
	volatile float vFPS;
} status;
static enum {Queued = 0, Captured, Used, Free} bufstatus[BUFFER_NUM] = {Queued};

static thread tCapture;

static struct sync_t {
	volatile int bufidx;
	mutex lockmtx, mtx;
	condition_variable cvar;
	bool ready;

	void lock() {lockmtx.lock();}
	void unlock() {lockmtx.unlock();}
	void notify()
	{
		std::unique_lock<std::mutex> locker(mtx);
		ready = true;
		cvar.notify_all();
	}
	void wait()
	{
		std::unique_lock<std::mutex> locker(mtx);
		while (!ready)
			cvar.wait(locker);
		ready = false;
	}
} sync;

static inline int setLED(uint32_t enable)
{
	return video_set_control(&dev, OV5647_CID_LED, &enable);
}

void captureThread()
{
	unsigned int bufidxN = 0;
	int err = 0;

	struct v4l2_buffer buf;
	//memset(&buf, sizeof(buf), 0);

	/* Start streaming. */
	int64_t past = getTickCount(), count = 0;
	if ((err = video_enable(&dev, 1)) != 0)
		goto failed;

//restart:
	if (status.swap) {
		setLED(1);
		if ((err = video_capture(&dev, &buf)) != 0)
			goto captureFailed;
		setLED(0);
		count++;
		bufidxN = buf.index;
	}

	while (status.request != REQUEST_QUIT) {
		setLED(1);
		if ((err = video_capture(&dev, &buf)) != 0)
			goto captureFailed;
		setLED(0);
		count++;
		if (status.swap) {
			bufidx = bufidxN;
			bufidxN = buf.index;
		} else
			bufidx = buf.index;

		sync.notify();

		// Refresh buffer status
		for (unsigned int i = 0; i < BUFFER_NUM; i++)
			if (bufstatus[i] != Queued)
				bufstatus[i] = Free;
		bufstatus[bufidx] = Captured;

		sync.lock();
		if (sync.bufidx != -1)
			bufstatus[sync.bufidx] = Used;
		sync.unlock();

		for (unsigned int i = 0; i < BUFFER_NUM; i++)
			if (bufstatus[i] == Free) {
				buf.index = i;
				if ((err = video_buffer_requeue(&dev, &buf)) != 0)
					goto captureFailed;
				bufstatus[i] = Queued;
				break;
			}

		int64_t now = getTickCount();
		if (now - past > 3 * getTickFrequency()) {
			float fps = (float)count / (now - past) * getTickFrequency();
			status.vFPS = fps;
			count = 0;
			past = now;
		}

#if 0
		// Better use a mutex or queue?
		switch (status.request) {
		case REQUEST_SWAP:
			status.swap = !status.swap;
			status.request = REQUEST_NONE;
			if (status.swap) {
				bufidxN = bufidx;
				continue;
			}
			bufidx = bufidxN;
			bufstatus[bufidx] = Captured;
			break;
		}
#endif
	}

captureFailed:
	video_enable(&dev, 0);
failed:
	video_close(&dev);
	status.request = REQUEST_QUIT;
	printf("%s: quitting\n", __func__);
}

Mat captureQuery()
{
	sync.wait();
	sync.lock();
	sync.bufidx = bufidx;
	Mat raw(status.height, status.width, CV_16UC1, dev.buffers[sync.bufidx].mem);
	sync.unlock();
	Mat rawu8, frame;
	raw.convertTo(rawu8, CV_8UC1, 1.f / 4.f);
	sync.lock();
	sync.bufidx = -1;
	sync.unlock();
	cvtColor(rawu8, frame, CV_BayerBG2RGB);
	return frame;
}

int captureInit(const char *devfile, int width, int height)
{
	status.width = width;
	status.height = height;
	status.pixelformat = V4L2_PIX_FMT_SBGGR10;
	printf("Requested video size: %ux%u\n", status.width, status.height);

	// Status
	status.request = REQUEST_NONE;
	status.swap = true;
	sync.bufidx = -1;

	// Initialise V4L2
	int err = 0;
	if ((err = video_init(&dev, devfile, status.pixelformat, MAX_W, MAX_H, BUFFER_NUM)) != 0)
		return err;
	if ((err = video_set_format(&dev, status.width, status.height, status.pixelformat)) < 0) {
		video_close(&dev);
		return err;
	}

	// Setup handling thread
	tCapture = thread(captureThread);
	return err;
}

void captureClose()
{
	status.request = REQUEST_QUIT;
	tCapture.join();
}
