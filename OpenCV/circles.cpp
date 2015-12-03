#include <cv.h>
#include <highgui.h>
#include <math.h>

using namespace cv;

int main(int argc, char** argv)
{
	VideoCapture cap(0);
	if(!cap.isOpened()) return -1;

	namedWindow( "circles", 1 );
	for (;;) {
		Mat img, gray;
		cap >> img;
		flip(img, img, 1);
		cvtColor(img, gray, CV_BGR2GRAY);
		// smooth it, otherwise a lot of false circles may be detected
		GaussianBlur( gray, gray, Size(3, 3), 2, 2 );
		vector<Vec3f> circles;
		HoughCircles(gray, circles, CV_HOUGH_GRADIENT,
			     2, gray.rows/4, 200, 100 );
		for( size_t i = 0; i < circles.size(); i++ )
		{
			Point center(cvRound(circles[i][0]), cvRound(circles[i][1]));
			int radius = cvRound(circles[i][2]);
			// draw the circle center
			circle( img, center, 3, Scalar(0,255,0), -1, 8, 0 );
			// draw the circle outline
			circle( img, center, radius, Scalar(0,0,255), 3, 8, 0 );
		}
		imshow( "circles", img );
		if(waitKey(30) >= 0) break;
	}
	return 0;
}
