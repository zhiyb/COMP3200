#include "opencv/cv.h"
#include "opencv/highgui.h"
#include "capture.h"

using namespace std;
using namespace cv;
using namespace cv::gpu;

int main(int argc, char *argv[])
{
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
	//gpu::setDevice(0);
	if (captureInit(argv[1], width, height))
		return -1;

	Mat frame, edges;
	GpuMat frameGPU;
	namedWindow("edges",1);
	for (;;) {
		frameGPU = captureQueryGPU();
		if (frameGPU.empty())
			break;
		frameGPU.download(frame);
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
