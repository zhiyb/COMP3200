#include "opencv/cv.h"
#include "opencv/highgui.h"
#include "capture.h"

using namespace cv;
using namespace std;

int main(int argc, char *argv[])
{
#if 0
	VideoCapture cap(0);
	if(!cap.isOpened()) return -1;
#else
	int width, height;
	if (argc == 4) {
		width = atoi(argv[2]);
		height = atoi(argv[3]);
	} else if (argc == 1) {
		cerr << "Please specify the device." << endl;
		return 1;
	} else {
		width = 1280;
		height = 960;
	}
	if (captureInit(argv[1], width, height))
		return -1;
#endif

	Mat frame, edges;
	namedWindow("edges",1);
	for (;;) {
		Mat frame = captureQuery();
		//cap >> frame;
		cvtColor(frame, edges, CV_BGR2GRAY);
		GaussianBlur(edges, edges, Size(7,7), 1.5, 1.5);
		Canny(edges, edges, 0, 30, 3);
		imshow("edges", edges);
		if(waitKey(30) >= 0) break;
	}
	captureClose();
	return 0;
}
