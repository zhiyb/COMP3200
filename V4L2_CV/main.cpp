#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/select.h>

#include "ov5647_v4l2.h"
#include "global.h"

using namespace std;

// V4L2
struct device dev;
struct v4l2_buffer buf[2];
volatile unsigned int bufidx = 0;

struct status_t status;

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
	setLED(1);
	if ((ret = video_capture(&dev, &buf[bufidx])) != 0)
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
	// Status
	status.request = REQUEST_NONE;
	status.swap = true;
	int err;
	// Threads
	thread *tInput, *tCV, *tPV;

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

	// Initialise V4L2
	if ((err = video_init(&dev, argv[1], status.pixelformat, MAX_W, MAX_H, 4)) != 0)
		goto initFailed;
	if ((err = video_set_format(&dev, status.width, status.height, status.pixelformat)) < 0)
		goto failed;

	// Setup threads
	tInput = new thread(inputThread);
	tCV = new thread(cvThread);
	tPV = new thread(pvThread);

	/* Start streaming. */
	if ((err = video_enable(&dev, 1)) != 0)
		goto failed;

restart:
	if (status.swap) {
		setLED(1);
		if ((err = video_capture(&dev, &buf[bufidx])) != 0)
			goto captureFailed;
		setLED(0);
		bufidx = !bufidx;
	}

	/* Loop until the user closes the window */
	while (1) {
		setLED(1);
		if ((err = video_capture(&dev, &buf[bufidx])) != 0)
			goto captureFailed;
		setLED(0);
		if (status.swap)
			bufidx = !bufidx;

		if ((err = video_buffer_requeue(&dev, &buf[bufidx])) != 0)
			goto captureFailed;

		// Better use a mutex or queue?
		switch (status.request) {
		case REQUEST_CAPTURE:
			printf("saveCapture(): %d\n", saveCapture());
			status.request = REQUEST_NONE;
			goto restart;
		case REQUEST_SWAP:
			status.swap = !status.swap;
			status.request = REQUEST_NONE;
			if (status.swap)
				goto restart;
			bufidx = !bufidx;
			if ((err = video_buffer_requeue(&dev, &buf[bufidx])) != 0)
				goto captureFailed;
			continue;
		case REQUEST_QUIT:
			goto quit;
		}
	}

quit:
captureFailed:
	video_enable(&dev, 0);
	tPV->join();
	delete tPV;
	tCV->join();
	delete tCV;
	tInput->detach();
	delete tInput;
failed:
	video_close(&dev);
initFailed:
	//glfwTerminate();
	return err;
}
