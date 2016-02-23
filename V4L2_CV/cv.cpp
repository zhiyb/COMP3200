//#include <pthread.h>
#include "opencv/cv.h"
#include "opencv/highgui.h"
#include "global.h"

using namespace std;
using namespace cv;

void cvThread()
{
	//pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	namedWindow("cv", 1);
loop:
	Mat raw(status.height, status.width, CV_16UC1, dev.buffers[buf[bufidx].index].mem);
	Mat img;
	cvtColor(raw, img, CV_BayerBG2RGB);
	imshow("cv", img);
	if(waitKey(10) >= 0 || status.request == REQUEST_QUIT)
		return;
	goto loop;
}
