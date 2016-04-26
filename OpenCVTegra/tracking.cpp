#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>
#include "opencv2/nonfree/gpu.hpp"

//#define IMGDATASET	"../bgslibrary/frames/"
//#define VIDDATASET	"../bgslibrary/dataset/"
#define DATASET	"dataset/baseline/highway/"

#define SHOW
#define BLOB_SIZE	8

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
	vector<vector<Point> > *prev_contours = 0;
	ofstream ofmaxd("maxd.log");

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
#if 0
		if (frameNumber % 8) {
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
		//imshow("input", img);
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
		//imshow("mask", mask);
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
		//imshow("mask2", mask);
#endif

		// Extract contours
		img.copyTo(drawing);
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
#ifdef SHOW
		//imshow("drawing", drawing);
#endif

		// Optical flow tracking between frames
		Mat img_grey, img_grey_tmp;
		img_grey_gpu.download(img_grey_tmp);
		//img_grey = img_grey_tmp;
		img_grey_tmp.copyTo(img_grey, mask2_bak);

		vector<Point2f> prevPts, nextPts;
		vector<uchar> ofStatus;
		if (!prev_grey.empty() && prev_contours != 0) {
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
				vector<Point2f> pts, bkpPts = prevPts;
				vector<float> err;
				//TermCriteria termcrit(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.03);
				Size winSize(32, 32);
				//calcOpticalFlowPyrLK(prev_grey, img_grey, bkpPts, pts, ofStatus, err, winSize, 2);
				calcOpticalFlowPyrLK(prev_grey, img_grey, prevPts, nextPts, ofStatus, err, winSize, 2);
#if 0
				prevPts.clear();
				nextPts.clear();
				for (unsigned int i = 0; i < pts.size(); i++)
					if (ofStatus[i]) {
						prevPts.push_back(bkpPts[i]);
						nextPts.push_back(pts[i]);
					}
#endif
			}
		}
		prev_grey = img_grey;

		if (prev_contours)
			delete prev_contours;
		prev_contours = contours;

		Point2f dmax;
		if (prevPts.size() > 0) {
			Mat d;
			absdiff(Mat(prevPts), Mat(nextPts), d);
			vector<Point2f> diff;
			d.copyTo(diff);
			for (size_t i = 0; i < prevPts.size(); i++) {
				if (!ofStatus[i])
					continue;
				dmax.x = max(dmax.x, diff[i].x);
				dmax.y = max(dmax.y, diff[i].y);
				//line(drawing, center[i], next_points[i], colour, 5);
				line(drawing, prevPts[i], nextPts[i], Scalar(0.f, 255.f, 0.f), 1, 8);
				//circle(drawing, prevPts[i], 3, Scalar(0.f, 0.f, 255.f), 1, 8);
				circle(drawing, nextPts[i], 3, Scalar(0.f, 0.f, 255.f), 1, 8);
			}
		}
		cout << "Max distance: " << dmax << endl;
		ofmaxd << dmax << endl;
#ifdef SHOW
		imshow("input OF", img_grey);
		imshow("drawing OF", drawing);
#endif

		// Relate feature points to contours
		{
			;
		}

		// Write images
#if 1
		stringstream file;
#if 0
		file << "output/" << setw(6) << setfill('0') << frameNumber << "_mask.png";
		imwrite(file.str(), mask_bak);
		file.str("");
		file.clear();
#endif
		file << "output/" << setw(6) << setfill('0') << frameNumber << "_mask2.png";
		imwrite(file.str(), mask2_bak);
		file.str("");
		file.clear();
		file << "output/" << setw(6) << setfill('0') << frameNumber << "_drawing.png";
		imwrite(file.str(), drawing);
		file.str("");
		file.clear();
		file << "output/" << setw(6) << setfill('0') << frameNumber << "_input_of.png";
		imwrite(file.str(), img_grey);
#else
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
#endif
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
