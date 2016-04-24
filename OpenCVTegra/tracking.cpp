#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>
#include "opencv2/nonfree/gpu.hpp"

//#define IMGDATASET	"../bgslibrary/frames/"
//#define VIDDATASET	"../bgslibrary/dataset/"
#define DATASET	"dataset/baseline/highway/"

#define SHOW
#define BLOB_SIZE	12

using namespace std;
using namespace cv;
using namespace cv::gpu;

static RNG rng(12345);

int main(int argc, char **argv)
{
	std::cout << "Using OpenCV " << CV_MAJOR_VERSION << "." << CV_MINOR_VERSION << "." << CV_SUBMINOR_VERSION << std::endl;

	if (gpu::getCudaEnabledDeviceCount() == 0)
		return -1;
	gpu::setDevice(0);
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
#ifdef DATASET
	int start, total;

	{
		clog << "Using dataset " DATASET << endl;
		ifstream ifs(DATASET "/temporalROI.txt");
		string str;
		getline(ifs, str);
		istringstream iss(str);
		iss >> start >> total;
		ifs.close();
	}

	clog << "Starts at frame " << start << ", " << total << " frames in total." << endl;
#endif

	GpuMat img_gpu, img_grey_gpu, mask_gpu;
	Mat img, mask, drawing, prev_grey;

	int frameNumber = 1;
	int64_t past = getTickCount();
	uint64_t count = 0;
	do {
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
#ifdef DATASET
		// Frame control
		if (frameNumber > total)
			break;

		// Frame rate control
#if 1
		if (frameNumber % 10) {
			frameNumber++;
			continue;
		}
#endif

		// Read frame image
		ostringstream imgfile;
		imgfile << DATASET "/input/in" << setw(6) << setfill('0') << frameNumber << ".jpg";
		//clog << "Reading file " << imgfile.str() << endl;
		img = imread(imgfile.str().c_str());
#endif
		if (img.empty())
			break;

#ifdef SHOW
		imshow("input", img);
#endif
		img_gpu.upload(img);
		gpu::cvtColor(img_gpu, img_grey_gpu, CV_RGB2GRAY);

		// ViBE foreground mask
#if 1
		gpu::GaussianBlur(img_gpu, img_gpu, Size(3, 3), 1.5);
#endif
		vibe(img_gpu, mask_gpu);

		mask_gpu.download(mask);
		Mat mask_bak = mask.clone();

#ifdef SHOW
		imshow("mask", mask);
#endif

		// Enhance foreground mask
#if 1
		//Mat img_mask(mask.size(), mask.type(), Scalar(0.f));
		{
			// find blobs
			std::vector<std::vector<cv::Point> > v;
			std::vector<cv::Vec4i> hierarchy;
			cv::findContours(mask, v, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
			mask = Scalar(0);
			for (size_t i=0; i < v.size(); ++i)
			{
				// drop smaller blobs
				if (cv::contourArea(v[i]) < BLOB_SIZE)
					continue;
				// draw filled blob
				cv::drawContours(mask, v, i, cv::Scalar(255,0,0), CV_FILLED, 8, hierarchy, 0, cv::Point());
			}

			// morphological closure
			cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(BLOB_SIZE, BLOB_SIZE));
			cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, element);
		}
#endif
		Mat mask2_bak = mask.clone();
#ifdef SHOW
		imshow("mask2", mask);
#endif

		// Extract contours
		img.copyTo(drawing);
#if 1
		{
			/// Find contours
			std::vector<cv::Vec4i> hierarchy;
			std::vector<std::vector<cv::Point> > contours;
			//std::vector<cv::Vec4i> hierarchy;
			cv::findContours(mask, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

			/// Approximate contours to polygons + get bounding rects and circles
			vector<vector<Point> > contours_poly(contours.size());

			/// Draw polygonal contour + bonding rects + circles
			for (size_t i = 0; i < contours.size(); i++) {
				// drop smaller blobs
				if (cv::contourArea(contours[i]) < BLOB_SIZE)
					continue;

				//Scalar color = Scalar(rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255));
				Scalar color(255.f, 0.f, 0.f);
				cv::drawContours(drawing, contours, i, color, 2, 8, hierarchy, 0, Point());
			}
		}
#endif
#ifdef SHOW
		imshow("drawing", drawing);
#endif

		// Optical flow tracking between frames
		Mat img_grey;
		img_grey_gpu.download(img_grey);

		vector<Point2f> prevPts, nextPts;
		if (!prev_grey.empty()) {
			goodFeaturesToTrack(prev_grey, prevPts, 25, 0.3, 10, mask);
			if (prevPts.size() > 0) {
				vector<Point2f> pts, bkpPts = prevPts;
				vector<uchar> ofStatus;
				vector<float> err;
				//TermCriteria termcrit(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.03);
				Size winSize(20, 20);
				calcOpticalFlowPyrLK(prev_grey, img_grey, bkpPts, pts,
						ofStatus, err, winSize);

				prevPts.clear();
				nextPts.clear();
				for (unsigned int i = 0; i < pts.size(); i++)
					if (ofStatus[i]) {
						prevPts.push_back(bkpPts[i]);
						nextPts.push_back(pts[i]);
					}
			}
		}
		prev_grey = img_grey;

		if (prevPts.size() > 0) {
			for (size_t i = 0; i < prevPts.size(); i++) {
				//line(drawing, center[i], next_points[i], colour, 5);
				line(drawing, prevPts[i], nextPts[i], Scalar(0.f, 255.f, 0.f), 2, 8);
				//circle(drawing, next_points[i], 3, colour, -1, 8);
			}
		}
#ifdef SHOW
		imshow("drawing OF", drawing);
#endif

		// Write images
#if defined(IMGDATASET)
		if (frameNumber == 51) {
#elif defined(VIDDATASET)
		if (frameNumber == 126) {
#elif defined(DATASET)
		if (frameNumber == 1666) {
#endif
			Mat tmp;
			imwrite("input.png", img);
			imwrite("mask.png", mask_bak);
			imwrite("img_mask.png", mask2_bak);
			imwrite("drawing.png", drawing);
			//cv::imwrite("bkgmodel.png", img_bkgmodel);
		}
		frameNumber++;

		// FPS calculation
		int64_t now = getTickCount();
		if (now - past > 3 * getTickFrequency()) {
			float fps = (float)count / (now - past) * getTickFrequency();
			count = 0;
			past = now;
			printf("FPS: %g\n", fps);
		}
		count++;
#ifdef SHOW
	} while (cvWaitKey(1) != 'q');
#else
	} while (1);
#endif

	//cvWaitKey(0);
	cvDestroyAllWindows();
	return 0;
}
