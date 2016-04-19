#include <math.h>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "capture.h"

using namespace std;
using namespace cv;
using namespace cv::gpu;

int main(int argc, char** argv)
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

	namedWindow( "circles", 1 );
	int64_t past = getTickCount();
	uint64_t count = 0;
	for (;;) {
		GpuMat img = captureQueryGPU();
		if (img.empty()) {
			printf("%s: Empty frame received, quitting...\n", __func__);
			break;
		}

		Mat imgCPU;
		img.download(imgCPU);
		GpuMat gray;
		gpu::cvtColor(img, gray, CV_BGR2GRAY);
		// smooth it, otherwise a lot of false circles may be detected
		gpu::GaussianBlur( gray, gray, Size(3, 3), 2, 2 );
		vector<Vec3f> circles;
		GpuMat circlesGPU;
		gpu::HoughCircles(gray, circlesGPU, CV_HOUGH_GRADIENT,
			     2, gray.rows/4, 200, 100, 5, 100);
		gpu::HoughCirclesDownload(circlesGPU, circles);
		for( size_t i = 0; i < circles.size(); i++ )
		{
			Point center(cvRound(circles[i][0]), cvRound(circles[i][1]));
			int radius = cvRound(circles[i][2]);
			// draw the circle center
			circle(imgCPU, center, 3, Scalar(0,255,0), -1, 8, 0);
			// draw the circle outline
			circle(imgCPU, center, radius, Scalar(0,0,255), 3, 8, 0);
		}
		imshow( "circles", imgCPU );

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
