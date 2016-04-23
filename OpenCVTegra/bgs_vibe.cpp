#include <iostream>
#include <fstream>
#include <iomanip>
#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>
#include "opencv2/nonfree/gpu.hpp"

//#define IMGDATASET	"../bgslibrary/frames/"
//#define VIDDATASET	"../bgslibrary/dataset/"
#define DATASET	"dataset/baseline/highway/"

//#define SHOW
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
	GpuMat img, fgmask;
	VIBE_GPU vibe;

	Mat img_input;

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
		if (frameNumber > total)
			break;

		// Frame rate control
#if 0
		if (frameNumber % 3)
			 continue;
#endif

		// Read frame image
		ostringstream imgfile;
		imgfile << DATASET "/input/in" << setw(6) << setfill('0') << frameNumber << ".jpg";
		//clog << "Reading file " << imgfile.str() << endl;
		img_input = imread(imgfile.str().c_str());
#endif
		if (img_input.empty())
			break;

#ifdef SHOW
		imshow("input", img_input);
#endif
		img.upload(img_input);
#if 1
		gpu::GaussianBlur(img, img, Size(3, 3), 1.5);
#endif
		vibe(img, fgmask);

		Mat mask;
		fgmask.download(mask);
		Mat mask_bak = mask.clone();

#ifdef SHOW
		imshow("mask", mask);
#endif

#if 1
		Mat img_mask(mask.size(), mask.type(), Scalar(0.f));
		{
			// find blobs
			std::vector<std::vector<cv::Point> > v;
			std::vector<cv::Vec4i> hierarchy;
			cv::findContours(mask, v, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
			for (size_t i=0; i < v.size(); ++i)
			{
				// drop smaller blobs
				if (cv::contourArea(v[i]) < BLOB_SIZE)
					continue;
				// draw filled blob
				cv::drawContours(img_mask, v, i, cv::Scalar(255,0,0), CV_FILLED, 8, hierarchy, 0, cv::Point());
			}

			// morphological closure
			cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(BLOB_SIZE, BLOB_SIZE));
			cv::morphologyEx(img_mask, img_mask, cv::MORPH_CLOSE, element);
		}
#else
		Mat img_mask(mask);
#endif
		Mat img_mask_bak = img_mask.clone();
#ifdef SHOW
		imshow("img_mask", img_mask);
#endif

		Mat drawing;
		img_input.copyTo(drawing);
#if 1
		{
			/// Find contours
			std::vector<cv::Vec4i> hierarchy;
			std::vector<std::vector<cv::Point> > contours;
			//std::vector<cv::Vec4i> hierarchy;
			cv::findContours(img_mask, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

			/// Approximate contours to polygons + get bounding rects and circles
			vector<vector<Point> > contours_poly(contours.size());
			//vector<Rect> boundRect(contours.size());
			//vector<Point2f>center(contours.size());
			//vector<float>radius(contours.size());

#if 0
			for (size_t i = 0; i < contours.size(); i++) {
				cv::approxPolyDP(Mat(contours[i]), contours_poly[i], 3, true);
				//boundRect[i] = boundingRect(Mat(contours_poly[i]));
				//minEnclosingCircle((Mat)contours_poly[i], center[i], radius[i]);
			}
#endif

			/// Draw polygonal contour + bonding rects + circles
			//Mat drawing = Mat::zeros(img_mask.size(), CV_8UC3);
			for (size_t i = 0; i < contours.size(); i++) {
				// drop smaller blobs
				if (cv::contourArea(contours[i]) < BLOB_SIZE)
					continue;

				//Scalar color = Scalar(rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255));
				Scalar color(255.f, 0.f, 0.f);
				//cv::drawContours(drawing, contours_poly, i, color, 1, 8, vector<Vec4i>(), 0, Point());
				cv::drawContours(drawing, contours, i, color, 2, 8, hierarchy, 0, Point());
				//rectangle(drawing, boundRect[i].tl(), boundRect[i].br(), color, 2, 8, 0);
				//circle(drawing, center[i], (int)radius[i], color, 2, 8, 0);
			}
		}
#endif
#ifdef SHOW
		imshow("drawing", drawing);
#endif

#if defined(IMGDATASET)
		if (frameNumber == 51) {
#elif defined(VIDDATASET)
		if (frameNumber == 126) {
#elif defined(DATASET)
		if (frameNumber == 1666) {
#endif
			Mat tmp;
			imwrite("input.png", img_input);
			imwrite("mask.png", mask_bak);
			imwrite("img_mask.png", img_mask_bak);
			imwrite("drawing.png", drawing);
			//cv::imwrite("bkgmodel.png", img_bkgmodel);
		}
		frameNumber++;

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
