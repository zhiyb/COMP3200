#include <opencv/cv.h>
#include <opencv/highgui.h>
#include "global.h"

using namespace std;
using namespace cv;

void cvThread()
{
	namedWindow("cv", 1);
loop:
	Mat raw(status.height, status.width, CV_16UC1, dev.buffers[buf[bufidx].index].mem);
	Mat rawu8;
	raw.convertTo(rawu8, CV_8UC1, 1.f / 4.f);
#if 1
	Mat img;
	cvtColor(rawu8, img, CV_BayerBG2RGB);
	imshow("cv", img);
#else
	imshow("cv", raw);
#endif
	if (waitKey(1) >= 0)
		status.request = REQUEST_QUIT;
	if(status.request == REQUEST_QUIT)
		return;
	goto loop;
}
