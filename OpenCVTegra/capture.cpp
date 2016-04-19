#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <signal.h>

#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "yavta.h"
#include "ov5647_v4l2.h"
#include "capture.h"

using namespace std;
using namespace cv;
using namespace cv::gpu;

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

static thread *tInput, *tCapture;

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

void inputThread()
{
	getchar();
	printf("%s: Request for quit...\n", __func__);
	status.request = REQUEST_QUIT;
	//captureClose();
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
			status.vFPS = (float)count / (now - past) * getTickFrequency();
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
	printf("%s: quitting: 0\n", __func__);
	return;
	//sync.notify();
	video_enable(&dev, 0);
failed:
	printf("%s: quitting: 1\n", __func__);
	video_close(&dev);
	printf("%s: quitting: 2\n", __func__);
	status.request = REQUEST_QUIT;
}

Mat captureQuery()
{
	sync.wait();
	sync.lock();
	if (status.request == REQUEST_QUIT) {
		sync.unlock();
		return Mat();
	}
	sync.bufidx = bufidx;
	sync.unlock();
	Mat raw(status.height, status.width, CV_16UC1, dev.buffers[sync.bufidx].mem);
	Mat rawu8, frame;
	raw.convertTo(rawu8, CV_8UC1, 1.f / 4.f);
	sync.lock();
	sync.bufidx = -1;
	sync.unlock();
	if (status.request == REQUEST_QUIT)
		return Mat();
	cvtColor(rawu8, frame, CV_BayerBG2RGB);
	return frame;
}

static GpuMat raw, rawu8, frame;
GpuMat captureQueryGPU()
{
	sync.wait();
	sync.lock();
	if (status.request == REQUEST_QUIT) {
		sync.unlock();
		return GpuMat();
	}
	sync.bufidx = bufidx;
	sync.unlock();
	raw.upload(Mat(status.height, status.width, CV_16UC1, dev.buffers[sync.bufidx].mem));
	sync.lock();
	sync.bufidx = -1;
	sync.unlock();
	if (status.request == REQUEST_QUIT)
		return GpuMat();
	raw.convertTo(rawu8, CV_8UC1, 1.f / 4.f);
	gpu::cvtColor(rawu8, frame, CV_BayerBG2RGB);
	return frame;
}

#if 0
static void sigintHandler(int signo)
{
	clog << "Signal INT received, closing camera..." << endl;
	captureClose();
}
#endif

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
	if ((err = video_init(&dev, devfile, status.pixelformat, status.width, status.height, BUFFER_NUM)) != 0)
		return err;
#if 0
	if ((err = video_set_format(&dev, status.width, status.height, status.pixelformat)) < 0) {
		video_close(&dev);
		return err;
	}
#endif

	// Setup interrupt signal handler
#if 0
	struct sigaction newact;
	newact.sa_handler = sigintHandler;
	sigemptyset(&newact.sa_mask);
	newact.sa_flags = 0;
	sigaction(SIGINT, &newact, NULL);
#endif

	// Setup handling thread
	tInput = new thread(inputThread);
	tCapture = new thread(captureThread);
	return err;
}

void captureClose()
{
	//if (status.request == REQUEST_QUIT)
	//	return;
	status.request = REQUEST_QUIT;
	printf("%s: Request for quit...\n", __func__);
	tCapture->join();
	delete tCapture;
	video_enable(&dev, 0);
	video_close(&dev);
	printf("%s: Request for quit: tInput\n", __func__);
	tInput->detach();
	//tInput->join();
	delete tInput;
}

float captureFPS()
{
	return status.vFPS;
}
