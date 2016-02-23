#include <iostream>
#include <thread>

#include <opencv/highgui.h>

#include "ov5647_v4l2.h"
#include "global.h"

using namespace std;
using namespace cv;

// V4L2
struct device dev;
//struct v4l2_buffer buf;
volatile unsigned int bufidx = 0;

struct status_t status;
enum {Queued = 0, Captured, Used, Free} bufstatus[BUFFER_NUM] = {Queued};

static inline int setLED(uint32_t enable)
{
	return video_set_control(&dev, OV5647_CID_LED, &enable);
}

int saveCapture()
{
	int ret;
	if ((ret = video_enable(&dev, 0)) != 0)
		return ret;

	// Set maximum resolution
	printf("%s: Saveing capture...\n", __func__);
	if ((ret = video_set_format(&dev, MAX_W, MAX_H, status.pixelformat)) < 0)
		return ret;
	if ((ret = video_queue_buffer(&dev, 0)) < 0)
		return ret;
	if ((ret = video_enable(&dev, 1)) != 0)
		return ret;

	// Capture
	struct v4l2_buffer buf;
	setLED(1);
	if ((ret = video_capture(&dev, &buf)) != 0)
		return ret;
	setLED(0);

	// Restore state
	if ((ret = video_enable(&dev, 0)) != 0)
		return ret;
	if ((ret = video_set_format(&dev, status.width, status.height, status.pixelformat)) < 0)
		return ret;
	/* Queue the buffers. */
	unsigned int i;
	for (i = 0; i < dev.nbufs; ++i) {
		int ret = video_queue_buffer(&dev, i);
		if (ret < 0)
			return ret;
	}
	return video_enable(&dev, 1);
}

int main(int argc, char *argv[])
{
	// Arguments
	if (argc == 4) {
		status.width = atoi(argv[2]);
		status.height = atoi(argv[3]);
	} else if (argc == 1) {
		cerr << "Please specify the device." << endl;
		return 1;
	} else {
#if 0
		status.width = 1280;
		status.height = 960;
#elif 1
		status.width = 1920;
		status.height = 1080;
#else
		status.width = 2592;
		status.height = 1944;
#endif
	}
	status.pixelformat = V4L2_PIX_FMT_SBGGR10;
	printf("Requested video size: %ux%u\n", status.width, status.height);

	// Status
	status.request = REQUEST_NONE;
	status.swap = true;
	int err;

	// Initialise V4L2
	struct v4l2_buffer buf;
	memset(&buf, sizeof(buf), 0);
	if ((err = video_init(&dev, argv[1], status.pixelformat, MAX_W, MAX_H, BUFFER_NUM)) != 0)
		return err;
	if ((err = video_set_format(&dev, status.width, status.height, status.pixelformat)) < 0) {
		video_close(&dev);
		return err;
	}

	// Setup threads
	cvData.mtx.lock();
	pvData.mtx.lock();
	bool locked = true;
	thread tInput(inputThread);
	thread tCV(cvThread);
	thread tPV(pvThread);
	cvData.bufidx = -1;
	pvData.bufidx = -1;

	/* Start streaming. */
	int64_t past = getTickCount(), count = 0;
	if ((err = video_enable(&dev, 1)) != 0)
		goto failed;

	unsigned int bufidxN;
restart:
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
		bufstatus[bufidx] = Captured;

		// Refresh buffer status
		for (unsigned int i = 0; i < BUFFER_NUM; i++)
			if (i != bufidx && bufstatus[i] != Queued)
				bufstatus[i] = Free;
		if (!locked) {
			pvData.mtx.lock();
			cvData.mtx.lock();
		}
		if (pvData.bufidx != -1)
			bufstatus[pvData.bufidx] = Used;
		if (cvData.bufidx != -1)
			bufstatus[cvData.bufidx] = Used;
		pvData.mtx.unlock();
		cvData.mtx.unlock();
		for (unsigned int i = 0; i < BUFFER_NUM; i++)
			if (bufstatus[i] == Free) {
				buf.index = i;
				if ((err = video_buffer_requeue(&dev, &buf)) != 0)
					goto captureFailed;
				bufstatus[i] = Queued;
			}
		locked = false;

		int64_t now = getTickCount();
		if (now - past > 3 * getTickFrequency()) {
			float fps = (float)count / (now - past) * getTickFrequency();
			status.vFPS = fps;
			count = 0;
			past = now;
		}

		// Better use a mutex or queue?
		switch (status.request) {
		case REQUEST_CAPTURE:
			printf("saveCapture(): %d\n", saveCapture());
			status.request = REQUEST_NONE;
			goto restart;
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
	}

captureFailed:
	video_enable(&dev, 0);
failed:
	status.request = REQUEST_QUIT;
	printf("%s: quitting\n", __func__);
	if (locked) {
		pvData.mtx.unlock();
		cvData.mtx.unlock();
	}
	tPV.join();
	tCV.join();
	tInput.detach();
	video_close(&dev);
	return err;
}
