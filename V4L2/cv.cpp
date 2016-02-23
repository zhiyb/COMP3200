#include <stdint.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include "global.h"

using namespace std;
using namespace cv;

struct thread_t cvData;

void cvThread()
{
	namedWindow("cv", 1);

	// Waiting for ready start
	cvData.mtx.lock();
	cvData.mtx.unlock();

	int64_t past = getTickCount(), count = 0;
	while (status.request != REQUEST_QUIT) {
		cvData.mtx.lock();
		cvData.bufidx = bufidx;
		cvData.mtx.unlock();
		Mat raw(status.height, status.width, CV_16UC1, dev.buffers[cvData.bufidx].mem);
		Mat rawu8;
		raw.convertTo(rawu8, CV_8UC1, 1.f / 4.f);
		Mat img;
		cvtColor(rawu8, img, CV_BayerBG2RGB);
		imshow("cv", img);

		count++;
		int64_t now = getTickCount();
		if (now - past > 3 * getTickFrequency()) {
			float fps = (float)count / (now - past) * getTickFrequency();
			status.cvFPS = fps;
			count = 0;
			past = now;
		}

		if (waitKey(1) >= 0)
			status.request = REQUEST_QUIT;
	}
}
