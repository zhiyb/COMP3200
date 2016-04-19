#include "opencv/cv.h"
#include "opencv/highgui.h"

using namespace cv;

int main(int, char**)
{
	VideoCapture cap("samples/original.jpg");
	if(!cap.isOpened()) return -1;

	Mat frame, edges;
	namedWindow("edges",1);

	cap >> frame;
	cvtColor(frame, edges, CV_BGR2GRAY);
	GaussianBlur(edges, edges, Size(9, 9), 1, 1);
	//GaussianBlur(edges, edges, Size(7,7), 1.5, 1.5);
	Canny(edges, edges, 0, 30, 3);
	imwrite("samples/edges.jpg", edges);
	imshow("edges", edges);

	waitKey();
	return 0;
}
