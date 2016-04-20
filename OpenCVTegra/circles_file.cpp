#include <cv.h>
#include <highgui.h>
#include <math.h>

using namespace cv;

int main(int argc, char** argv)
{
	VideoCapture cap("samples/original.jpg");
	if(!cap.isOpened()) return -1;

	namedWindow( "circles", 1 );
	Mat img, gray;
	cap >> img;
	cvtColor(img, gray, CV_BGR2GRAY);
	imwrite("samples/gray.jpg", gray);
	imshow( "gray", gray );
	// smooth it, otherwise a lot of false circles may be detected
	GaussianBlur( gray, gray, Size(9, 9), 1, 1 );
	imwrite("samples/blur.jpg", gray);
	imshow( "blur", gray );
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
	imwrite("samples/circles.jpg", img);
	imshow( "circles", img );
	waitKey();
}
