#include <stdint.h>
#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>
#include "opencv2/nonfree/gpu.hpp"
#include "global.h"
#include "camera.h"
#include "cv_private.h"

#define ADAPTIVE
#define MOVE_MAX	64
#define BLOB_SIZE	7
#define OF_SIZE		32

using namespace std;
using namespace cv;

void maskEnhance(Mat mask)
{
#if 1
	//Mat img_mask(mask.size(), mask.type(), Scalar(0.f));
#if 1
	// find blobs
	std::vector<std::vector<cv::Point> > v;
	std::vector<cv::Vec4i> hierarchy;
	cv::findContours(mask, v, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
	mask = Scalar(0);
	for (size_t i=0; i < v.size(); ++i)
	{
		// drop smaller blobs
		if (cv::contourArea(v[i]) < BLOB_SIZE * BLOB_SIZE)
			continue;
		// draw filled blob
		cv::drawContours(mask, v, i, cv::Scalar(255,0,0), CV_FILLED, 8, hierarchy, 0, cv::Point());
	}
#endif

	// morphological closure
	cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(BLOB_SIZE, BLOB_SIZE));
	cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, element);
#endif
}

// OpenCV CPU post processing
void cvThread_CPU()
{
	std::unique_lock<std::mutex> locker;

	Mat grey_prev, drawing;
	vector<vector<Point> > *prev_contours = 0;

	float fps = FPS_MAX;
	int64_t past = getTickCount(), count = 0;
	uint64_t ts, ts_prev = 0;
	unsigned long frameCount = 0;
	while (1) {
		locker = cv_gpu.smpr.lock();
		cv_gpu.smpr.wait(locker);
		ts = cv_gpu.ts;
		Mat input(cv_gpu.input);
		Mat mask(cv_gpu.mask);
		Mat grey(cv_gpu.grey);
		cv_gpu.smpr.unlock(locker);
		if (status.request & REQUEST_QUIT)
			break;
		if (input.empty())
			continue;

		float itvl = (float)(ts - ts_prev) / (float)cv::getTickFrequency();
		ts_prev = ts;
#if 0
		if (status.cvShow) {
			imshow("input", input);
			//imshow("grey", grey);
			//imshow("mask", mask);
		}
#endif

		// Enhance foreground mask
		maskEnhance(mask);

		Mat grey_masked;
		grey.copyTo(grey_masked, mask);

		if (status.cvShow) {
			imshow("input OF", grey_masked);
		}

		// Extract contours
		input.copyTo(drawing);
		vector<vector<Point> > *contours = new vector<vector<Point> >;
#if 1
		{
			/// Find contours
			std::vector<cv::Vec4i> hierarchy;
			//std::vector<cv::Vec4i> hierarchy;
			cv::findContours(mask, *contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

			/// Approximate contours to polygons + get bounding rects and circles
			vector<vector<Point> > contours_poly(contours->size());

			/// Draw polygonal contour + bonding rects + circles
			for (size_t i = 0; i < contours->size(); i++) {
				// drop smaller blobs
				if (cv::contourArea((*contours)[i]) < BLOB_SIZE)
					continue;

				//Scalar color = Scalar(rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255));
				Scalar color(255.f, 0.f, 0.f);
				cv::drawContours(drawing, *contours, i, color, 2, 8, hierarchy, 0, Point());
			}
		}
#endif

		// Optical flow tracking between frames
		vector<Point2f> prevPts, nextPts;
		vector<uchar> ofStatus;
		if (!grey_prev.empty() && prev_contours != 0) {
#if 0
			goodFeaturesToTrack(prev_grey, prevPts, 32, 0.1, 3, mask);
#else
			for (vector<Point> &points: *prev_contours) {
				if (cv::contourArea(points) < BLOB_SIZE * BLOB_SIZE)
					continue;
				for (Point &point: points)
					prevPts.push_back(point);
			}
#endif
			if (prevPts.size() > 0) {
				vector<float> err;
				//TermCriteria termcrit(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.03);
				Size winSize(OF_SIZE, OF_SIZE);
				calcOpticalFlowPyrLK(grey_prev, grey_masked, prevPts, nextPts, ofStatus, err, winSize, 1);
			}
		}
		grey_prev = grey_masked;

		if (prev_contours)
			delete prev_contours;
		prev_contours = contours;

		double dismax = 0.f;
		Point2f dismaxp;
		if (prevPts.size() > 0) {
			Mat d;
			absdiff(Mat(prevPts), Mat(nextPts), d);
			vector<Point2f> diff;
			d.copyTo(diff);
			for (size_t i = 0; i < prevPts.size(); i++) {
				if (!ofStatus[i])
					continue;
				double dis = norm(diff[i]);
#ifdef ADAPTIVE
				if (dis > fps * MOVE_MAX)
					continue;
#endif
				if (dis > dismax) {
					dismax = dis;
					dismaxp = diff[i];
				}
				//line(drawing, center[i], next_points[i], colour, 5);
				line(drawing, prevPts[i], nextPts[i], Scalar(0.f, 255.f, 0.f), 1, 8);
				//circle(drawing, prevPts[i], 3, Scalar(0.f, 0.f, 255.f), 1, 8);
				circle(drawing, nextPts[i], 3, Scalar(0.f, 0.f, 255.f), 1, 8);
			}
		}

		if (status.cvShow) {
			imshow("output OF", drawing);
		}

#ifdef ADAPTIVE
		// Adaptive FPS calculation
		fps = dismax / itvl / (float)OF_SIZE;
		fps = max(fps, (float)FPS_MIN);
		fps = min(fps, (float)FPS_MAX);
		//cout << fps << endl;
		setFPS(fps);
#endif

		count++;
		frameCount++;
		int64_t now = getTickCount();
		if (now - past > 3 * getTickFrequency()) {
			float fps = (float)count / (now - past) * getTickFrequency();
			status.cvFPS_disp = status.cvFPS_CPU = fps;
			count = 0;
			past = now;
		}

		if (status.cvShow && waitKey(1) >= 0)
			break;
	}
	status.request = REQUEST_QUIT;
	delete prev_contours;
}
