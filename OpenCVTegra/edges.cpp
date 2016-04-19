#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
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

	GpuMat frame, edges;
	namedWindow("edges",1);
	int64_t past = getTickCount();
	uint64_t count = 0;
	for (;;) {
		frame = captureQueryGPU();
		if (frame.empty()) {
			printf("%s: Empty frame received, quitting...\n", __func__);
			break;
		}

		gpu::cvtColor(frame, edges, CV_BGR2GRAY);
		gpu::GaussianBlur(edges, edges, Size(7,7), 1.5, 1.5);
		gpu::Canny(edges, edges, 0, 30, 3);

		Mat edgesCPU;
		edges.download(edgesCPU);
		imshow("edges", edgesCPU);

		int64_t now = getTickCount();
		if (now - past > 3 * getTickFrequency()) {
			float fps = (float)count / (now - past) * getTickFrequency();
			count = 0;
			past = now;
			printf("\nCapture FPS: %g\n", captureFPS());
			printf("Window FPS: %g\n", fps);
		}
		count++;

		if(waitKey(1) >= 0) break;
	}
	captureClose();
	destroyAllWindows();
	return 0;
}
