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


#include "package_bgs/FrameDifferenceBGS.h"
#include "package_bgs/StaticFrameDifferenceBGS.h"
#include "package_bgs/WeightedMovingMeanBGS.h"
#include "package_bgs/WeightedMovingVarianceBGS.h"
#include "package_bgs/MixtureOfGaussianV1BGS.h"
#include "package_bgs/MixtureOfGaussianV2BGS.h"
#include "package_bgs/AdaptiveBackgroundLearning.h"
#include "package_bgs/AdaptiveSelectiveBackgroundLearning.h"

#if CV_MAJOR_VERSION >= 2 && CV_MINOR_VERSION >= 4 && CV_SUBMINOR_VERSION >= 3
#include "package_bgs/GMG.h"
#endif

#include "package_bgs/dp/DPAdaptiveMedianBGS.h"
#include "package_bgs/dp/DPGrimsonGMMBGS.h"
#include "package_bgs/dp/DPZivkovicAGMMBGS.h"
#include "package_bgs/dp/DPMeanBGS.h"
#include "package_bgs/dp/DPWrenGABGS.h"
#include "package_bgs/dp/DPPratiMediodBGS.h"
#include "package_bgs/dp/DPEigenbackgroundBGS.h"
#include "package_bgs/dp/DPTextureBGS.h"

#include "package_bgs/tb/T2FGMM_UM.h"
#include "package_bgs/tb/T2FGMM_UV.h"
#include "package_bgs/tb/T2FMRF_UM.h"
#include "package_bgs/tb/T2FMRF_UV.h"
#include "package_bgs/tb/FuzzySugenoIntegral.h"
#include "package_bgs/tb/FuzzyChoquetIntegral.h"

#include "package_bgs/lb/LBSimpleGaussian.h"
#include "package_bgs/lb/LBFuzzyGaussian.h"
#include "package_bgs/lb/LBMixtureOfGaussians.h"
#include "package_bgs/lb/LBAdaptiveSOM.h"
#include "package_bgs/lb/LBFuzzyAdaptiveSOM.h"

#include "package_bgs/ck/LbpMrf.h"
#include "package_bgs/jmo/MultiLayerBGS.h"
// The PBAS algorithm was removed from BGSLibrary because it is
// based on patented algorithm ViBE
// http://www2.ulg.ac.be/telecom/research/vibe/
//#include "package_bgs/pt/PixelBasedAdaptiveSegmenter.h"
#include "package_bgs/av/VuMeter.h"
#include "package_bgs/ae/KDE.h"
#include "package_bgs/db/IndependentMultimodalBGS.h"
#include "package_bgs/sjn/SJN_MultiCueBGS.h"
#include "package_bgs/bl/SigmaDeltaBGS.h"

#include "package_bgs/pl/SuBSENSE.h"
#include "package_bgs/pl/LOBSTER.h"

#define DATASET	"D:\\dataset\\dataset\\baseline\\highway"

static RNG rng(12345);

using namespace std;

int main(int argc, char **argv)
{
	std::cout << "Using OpenCV " << CV_MAJOR_VERSION << "." << CV_MINOR_VERSION << "." << CV_SUBMINOR_VERSION << std::endl;

#ifdef DATASET
	Mat frame;
	int start, total;

	clog << "Using dataset " DATASET << endl;
	ifstream ifs(DATASET "/temporalROI.txt");
	string str;
	getline(ifs, str);
	istringstream iss(str);
	iss >> start >> total;

	clog << "Starts at frame " << start << ", " << total << " frames in total." << endl;
#else
	//CvCapture *capture = 0;
	VideoCapture cap;
	int resize_factor = 100;

	if(argc > 1)
	{
		std::cout << "Openning: " << argv[1] << std::endl;
		//capture = cvCaptureFromAVI(argv[1]);
		cap.open(argv[1]);
	}
	else
	{
		//capture = cvCaptureFromCAM(0);
		cap.open(0);
		//resize_factor = 50; // set size = 50% of original image
	}

	//if(!capture)
	if(!cap.isOpened())
	{
		std::cerr << "Cannot initialize video!" << std::endl;
		return -1;
	}

#if 0
	IplImage *frame_aux = cvQueryFrame(capture);
	IplImage *frame = cvCreateImage(cvSize((int)((frame_aux->width*resize_factor)/100) , (int)((frame_aux->height*resize_factor)/100)), frame_aux->depth, frame_aux->nChannels);
	cvResize(frame_aux, frame);
#else
	//cvNamedWindow("Empty");
	//cvWaitKey(100);
	Mat frame;
	do {
		cap >> frame;
		if (cvWaitKey(1) == 'q')
			return 0;
		std::cerr << "Capture: " << frame.rows << ", " << frame.cols << std::endl;
	} while (!frame.rows || !frame.cols);
#endif
#endif

	/* Background Subtraction Methods */
	IBGS *bgs;

	/*** Default Package ***/
	//bgs = new FrameDifferenceBGS;
	//bgs = new StaticFrameDifferenceBGS;
	//bgs = new WeightedMovingMeanBGS;
	//bgs = new WeightedMovingVarianceBGS;
	//bgs = new MixtureOfGaussianV1BGS;
	//bgs = new MixtureOfGaussianV2BGS;
	//bgs = new AdaptiveBackgroundLearning;
	//bgs = new AdaptiveSelectiveBackgroundLearning;
	bgs = new GMG;

	/*** DP Package (thanks to Donovan Parks) ***/
	//bgs = new DPAdaptiveMedianBGS;
	//bgs = new DPGrimsonGMMBGS;
	//bgs = new DPZivkovicAGMMBGS;
	//bgs = new DPMeanBGS;
	//bgs = new DPWrenGABGS;
	//bgs = new DPPratiMediodBGS;
	//bgs = new DPEigenbackgroundBGS;
	//bgs = new DPTextureBGS;

	/*** TB Package (thanks to Thierry Bouwmans, Fida EL BAF and Zhenjie Zhao) ***/
	//bgs = new T2FGMM_UM;
	//bgs = new T2FGMM_UV;
	//bgs = new T2FMRF_UM;
	//bgs = new T2FMRF_UV;
	//bgs = new FuzzySugenoIntegral;
	//bgs = new FuzzyChoquetIntegral;

	/*** JMO Package (thanks to Jean-Marc Odobez) ***/
	//bgs = new MultiLayerBGS;

	/*** PT Package (thanks to Martin Hofmann, Philipp Tiefenbacher and Gerhard Rigoll) ***/
	//bgs = new PixelBasedAdaptiveSegmenter;

	/*** LB Package (thanks to Laurence Bender) ***/
	//bgs = new LBSimpleGaussian;
	//bgs = new LBFuzzyGaussian;
	//bgs = new LBMixtureOfGaussians;
	//bgs = new LBAdaptiveSOM;
	//bgs = new LBFuzzyAdaptiveSOM;

	/*** LBP-MRF Package (thanks to Csaba Kertész) ***/
	//bgs = new LbpMrf;

	/*** AV Package (thanks to Lionel Robinault and Antoine Vacavant) ***/
	//bgs = new VuMeter;

	/*** EG Package (thanks to Ahmed Elgammal) ***/
	//bgs = new KDE;

	/*** DB Package (thanks to Domenico Daniele Bloisi) ***/
	//bgs = new IndependentMultimodalBGS;

	/*** SJN Package (thanks to SeungJong Noh) ***/
	//bgs = new SJN_MultiCueBGS;

	/*** BL Package (thanks to Benjamin Laugraud) ***/
	//bgs = new SigmaDeltaBGS;

	/*** PL Package (thanks to Pierre-Luc) ***/
	//bgs = new SuBSENSEBGS();
	//bgs = new LOBSTERBGS();

	Mat previous;//, previous_mask;
	Mat trace;
	cv::Mat img_mask;
	cv::Mat img_bkgmodel;
	//std::vector<std::vector<cv::Point> > previous_contours;
	int frame_count = 0;
	for (int ds = 1; ds != total;) {
		//frame_aux = cvQueryFrame(capture);
		//if(!frame_aux) break;

#ifdef DATASET
		ostringstream imgfile;
		imgfile << DATASET "/input/in" << setw(6) << setfill('0') << ds++ << ".jpg";
		//clog << "Reading file " << imgfile.str() << endl;
		frame = imread(imgfile.str().c_str());
		if (!frame.rows || !frame.cols) {
			clog << "Cannot read file " << imgfile.str() << endl;
			return 1;
		}
#else
		//cvResize(frame_aux, frame);
		cap >> frame;
#endif

		Mat img_input(frame);
		imshow("input", img_input);
		//if (frame_count == 0)
		//	cv::imwrite("dataset/1.png", img_input);

		if (frame_count == 0) {
			previous = img_input;
			trace = Mat(img_input.size(), CV_8UC3);
		}

		bgs->process(img_input, img_mask, img_bkgmodel); // by default, it shows automatically the foreground mask image

		//if(!img_mask.empty())
		//  cv::imshow("Foreground", img_mask);
		//  do something
		medianBlur(img_mask, img_mask, 5);

#if 0
#define BLOB_SIZE	50
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
			if (cv::contourArea(v[i]) < 100)
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
}
