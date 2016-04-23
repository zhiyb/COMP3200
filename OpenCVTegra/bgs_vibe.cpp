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

#ifdef SHOW
		Mat mask;
		fgmask.download(mask);

		imshow("mask", mask);
#endif

#if defined(IMGDATASET)
		if (frameNumber == 51) {
#elif defined(VIDDATASET)
		if (frameNumber == 126) {
#elif defined(DATASET)
		if (frameNumber == 1666) {
#endif
#ifndef SHOW
			Mat mask;
			fgmask.download(mask);
#endif
			cv::imwrite("input.png", img_input);
			cv::imwrite("mask.png", mask);
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
