/*
This file is part of BGSLibrary.

BGSLibrary is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

BGSLibrary is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with BGSLibrary.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <iostream>
#include <iomanip>
#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>
#include "opencv2/nonfree/gpu.hpp"

//#define IMGDATASET	"../bgslibrary/frames/"
#define VIDDATASET	"../bgslibrary/dataset/"

using namespace std;
using namespace cv;
using namespace cv::gpu;

int main(int argc, char **argv)
{
	std::cout << "Using OpenCV " << CV_MAJOR_VERSION << "." << CV_MINOR_VERSION << "." << CV_SUBMINOR_VERSION << std::endl;

	if (gpu::getCudaEnabledDeviceCount() == 0)
		return -1;

	gpu::setDevice(0);
	GpuMat img, fgmask;
	VIBE_GPU vibe;

#ifdef VIDDATASET
	VideoCapture cap;
	std::cout << "Openning: " VIDDATASET "video.avi" << std::endl;
	cap.open(VIDDATASET "video.avi");
	if(!cap.isOpened()) {
		std::cerr << "Cannot initialize video!" << std::endl;
		return -1;
	}
#endif

	int frameNumber = 1;
	int key = 0;
	Mat img_input;
	while (key != 'q') {
#ifdef IMGDATASET
		std::stringstream ss;
		ss << frameNumber;
		std::string fileName = IMGDATASET + ss.str() + ".png";
		std::cout << "reading " << fileName << std::endl;

		img_input = imread(fileName, CV_LOAD_IMAGE_COLOR);
#endif
#ifdef VIDDATASET
		cap >> img_input;
#endif

		if (img_input.empty())
			break;

		imshow("input", img_input);

		img.upload(img_input);
#if 1
		gpu::GaussianBlur(img, img, Size(3, 3), 1.5);
#endif
		if (frameNumber == 0)
			vibe.initialize(img);
		else {
			vibe(img, fgmask);

			Mat mask;
			fgmask.download(mask);

			imshow("mask", mask);
#ifdef IMGDATASET
			fileName = IMGDATASET + ss.str() + ".png";
			//cv::imwrite(fileName, mask);
			if (frameNumber == 51) {
				//cv::imwrite(IMGDATASET "input.png", img_input);
				cv::imwrite(IMGDATASET "mask.png", mask);
				//cv::imwrite(IMGDATASET "bkgmodel.png", img_bkgmodel);
			}
#endif
#ifdef VIDDATASET
			if (frameNumber == 126) {
				cv::imwrite(VIDDATASET "input.png", img_input);
				cv::imwrite(VIDDATASET "mask.png", mask);
				//cv::imwrite(VIDDATASET "bkgmodel.png", img_bkgmodel);
				break;
			}
#endif
		}

		key = cvWaitKey(33);
		frameNumber++;
	}
	cvWaitKey(0);

	cvDestroyAllWindows();

	return 0;
/******************************************************************************/

#if 0
		//if (frame_count == 0)
		//	cv::imwrite("dataset/1.png", img_input);

		if (frame_count == 0) {
			//previous = img_input;
			trace = Mat(img_input.size(), CV_8UC3, Scalar(0));
		}

		bgs->process(img_input, img_mask, img_bkgmodel); // by default, it shows automatically the foreground mask image

		//if(!img_mask.empty())
		//  cv::imshow("Foreground", img_mask);
		//  do something
		medianBlur(img_mask, img_mask, 5);

#if 1
#define BLOB_SIZE	10
		// find blobs
		std::vector<std::vector<cv::Point> > v;
		std::vector<cv::Vec4i> hierarchy;
		cv::findContours(img_mask, v, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
		img_mask = cv::Scalar(0, 0, 0);
		for (size_t i=0; i < v.size(); ++i)
		{
			// drop smaller blobs
			if (cv::contourArea(v[i]) < BLOB_SIZE * BLOB_SIZE)
				continue;
			// draw filled blob
			cv::drawContours(img_mask, v, i, cv::Scalar(255,0,0), CV_FILLED, 8, hierarchy, 0, cv::Point() );
		}
		imshow("Mask", img_mask);

		// morphological closure
		cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(BLOB_SIZE, BLOB_SIZE));
		cv::morphologyEx(img_mask, img_mask, cv::MORPH_CLOSE, element);
		imshow("Mask", img_mask);
#endif

#if 0
		// find blobs again
		//std::vector<std::vector<cv::Point> > v;
		//std::vector<cv::Vec4i> hierarchy;
		cv::findContours(img_mask, v, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
		img_mask = cv::Scalar(0, 0, 0);
		for (size_t i=0; i < v.size(); ++i)
		{
			// drop smaller blobs
			if (cv::contourArea(v[i]) < BLOB_SIZE * BLOB_SIZE)
				continue;
			// draw filled blob
			cv::drawContours(img_mask, v, i, cv::Scalar(255,0,0), CV_FILLED, 8, hierarchy, 0, cv::Point());
		}
		imshow("Mask", img_mask);
#endif
#if 0		// http://docs.opencv.org/2.4.10/doc/tutorials/imgproc/shapedescriptors/bounding_rects_circles/bounding_rects_circles.html

		/// Find contours
		std::vector<std::vector<cv::Point> > contours;
		//std::vector<cv::Vec4i> hierarchy;
		cv::findContours(img_mask, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

		/// Approximate contours to polygons + get bounding rects and circles
		vector<vector<Point> > contours_poly(contours.size());
		vector<Rect> boundRect(contours.size());
		//vector<Point2f>center(contours.size());
		//vector<float>radius(contours.size());

		for (size_t i = 0; i < contours.size(); i++) {
			approxPolyDP(Mat(contours[i]), contours_poly[i], 3, true);
			boundRect[i] = boundingRect(Mat(contours_poly[i]));
			//minEnclosingCircle((Mat)contours_poly[i], center[i], radius[i]);
		}

		/// Draw polygonal contour + bonding rects + circles
		//Mat drawing = Mat::zeros(img_mask.size(), CV_8UC3);
		Mat drawing;
		img_input.copyTo(drawing);
		for (size_t i = 0; i < contours.size(); i++) {
			Scalar color = Scalar(rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255));
			drawContours(drawing, contours_poly, i, color, 1, 8, vector<Vec4i>(), 0, Point());
			//rectangle(drawing, boundRect[i].tl(), boundRect[i].br(), color, 2, 8, 0);
			//circle(drawing, center[i], (int)radius[i], color, 2, 8, 0);
		}

		imshow("Drawing", drawing);
#endif
#if 0
		Mat masked;
		img_input.copyTo(masked, img_mask);
		imshow("Masked", masked);
#endif
#if 0
		Mat prev_grey;
		Mat next_grey;
		cvtColor(previous, prev_grey, CV_RGB2GRAY);
		cvtColor(img_input, next_grey, CV_RGB2GRAY);
		//imshow("GREY", next_grey);
#endif
#if 0
		Mat flow;
		calcOpticalFlowFarneback(prev_grey, next_grey, flow, 0.5, 3, 15, 3, 5, 1.2, 0);
		//imshow("Flow", flow);
#endif
#if 0
		Mat flow;
		// find blobs again
		std::vector<std::vector<cv::Point> > contours;
		//std::vector<std::vector<cv::Point> > v;
		//std::vector<cv::Vec4i> hierarchy;
		cv::findContours(img_mask, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
		//img_mask = cv::Scalar(0, 0, 0);

		if (contours.size() != 0) {
#if 1
			/// Approximate contours to polygons + get bounding rects and circles
			vector<vector<Point> > contours_poly(contours.size());
			vector<Point2f> center(contours.size());
			vector<float> radius(contours.size());
			for (size_t i = 0; i < contours.size(); i++) {
				approxPolyDP(Mat(contours[i]), contours_poly[i], 3, true);
				//boundRect[i] = boundingRect(Mat(contours_poly[i]));
				minEnclosingCircle((Mat)contours_poly[i], center[i], radius[i]);
			}
#endif

#if 1
			vector<Point2f> next_points;
			//vector<unsigned char> status;
			vector<uchar> status;
			vector<float> err;
			TermCriteria termcrit(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.03);
			Size winSize(31, 31);
			// Reversing it?
			calcOpticalFlowPyrLK(next_grey, prev_grey, center, next_points, \
					     status, err, winSize, 3, termcrit, 0, 0.001);
#endif
			//Mat drawing;
			//img_input.copyTo(drawing);

			for (size_t i = 0; i < next_points.size(); i++) {
				Scalar colour = Scalar(rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255));
				//line(drawing, center[i], next_points[i], colour, 5);
				line(trace, center[i], next_points[i], colour, 3, CV_AA);
				//circle(drawing, next_points[i], 3, colour, -1, 8);
			}

			//imshow("Drawing", drawing);
			imshow("Trace", trace);
		}

		//imshow("Mask", img_mask);
#endif
#if 1
		Mat next_grey;
		cvtColor(img_input, next_grey, CV_RGB2GRAY);
		ofUpdate(next_grey, img_mask);

		if (prevPts.size() > 0) {
			for (size_t i = 0; i < prevPts.size(); i++) {
				//line(drawing, center[i], next_points[i], colour, 5);
				line(trace, prevPts[i], nextPts[i], colour[i], 1, 8);
				//circle(drawing, next_points[i], 3, colour, -1, 8);
			}
			imshow("Trace", trace);
		}
#else
		vector<Point2f> corners;
		goodFeaturesToTrack(prev_grey, corners, 25, 0.3, 10, img_mask);
#if 0
		for (Point p: corners) {
			Scalar color = Scalar(rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255));
			circle(trace, p, 2, color, -1);
		}
#endif

		if (corners.size() > 0) {
			//TermCriteria termcrit(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.03);
			Size winSize(20, 20);
			calcOpticalFlowPyrLK(prev_grey, next_grey, corners, nextPts,
					     status, err, winSize);

			for (size_t i = 0; i < nextPts.size(); i++) {
				if (!status[i])
					continue;
				Scalar colour = Scalar(rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255));
				//line(drawing, center[i], next_points[i], colour, 5);
				line(trace, corners[i], nextPts[i], colour, 1, 8);
				//circle(drawing, next_points[i], 3, colour, -1, 8);
			}
			imshow("Trace", trace);
		}
#endif
#if 0
		for (size_t i = 0; i < previous_contours.size(); i++) {
			//Scalar color = Scalar(rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255));
			//drawContours(drawing, contours_poly, i, color, 1, 8, vector<Vec4i>(), 0, Point());
			//rectangle(drawing, boundRect[i].tl(), boundRect[i].br(), color, 2, 8, 0);
			//circle(drawing, center[i], (int)radius[i], color, 2, 8, 0);
		}
#endif

		//masked.copyTo(previous);
		//img_input.copyTo(previous);
		//img_mask.copyTo(previous_mask);
		//previous_contours = contours;
		//previous = img_input;
		frame_count++;
		int key = cvWaitKey(1);
		/*if (frame_count == 126) {
			cv::imwrite("dataset/input.png", img_input);
			cv::imwrite("dataset/mask.png", img_mask);
			cv::imwrite("dataset/bkgmodel.png", img_bkgmodel);
			break;
		} else*/ if (key == 's')
			std::cout << "Frame: " << frame_count << std::endl;
		else if (key == 'q')
			break;
	}

	delete bgs;

	cvDestroyAllWindows();
	//cvReleaseCapture(&capture);

	return 0;
#endif
}
