#include <thread>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>
#include "opencv2/nonfree/gpu.hpp"
#include "global.h"
#include "cv_private.h"

//#define PROOFING
//#define PROOFING_MEM
//#define CPUIMP

using namespace std;
using namespace cv;

struct thread_t cvData;
struct cv_gpu_t cv_gpu;

void cvThread()
{
	// OpenCV GPU
	//cout << __func__ << ": Cuda devices: " << gpu::getCudaEnabledDeviceCount() << endl;
	if (gpu::getCudaEnabledDeviceCount() == 0) {
		status.request = REQUEST_QUIT;
		return;
	}
	gpu::setDevice(0);

#if 1
	float sizef = 0.5f;
#ifdef CPUIMP
	Mat raw, rawu8, img_orig;
	gpu::GpuMat img_gpu;
#else
	gpu::GpuMat raw(status.height, status.width, CV_16UC1);
	gpu::GpuMat rawu8(status.height, status.width, CV_8UC1);
	gpu::GpuMat img_orig(status.height, status.width, CV_8UC3);
	gpu::GpuMat img(status.height * sizef, status.width * sizef, CV_8UC3);
	gpu::GpuMat img_grey(status.height * sizef, status.width * sizef, CV_8UC1);
#endif
#if 0
	gpu::GpuMat *img_s = new gpu::GpuMat(status.height / 2, status.width / 2, CV_8UC3);
#endif
	gpu::GpuMat fgmask;
#endif

#if 1
	gpu::VIBE_GPU vibe;
#endif

	thread cpuThread(cvThread_CPU);
	
	// Waiting for ready start
	std::unique_lock<std::mutex> locker;
	cvData.mtx.lock();
	cvData.mtx.unlock();

#ifdef PROOFING
	ofstream oflog("logging.log");
#endif
	int64_t past = getTickCount(), count = 0;
#ifdef PROOFING
	uint64_t start, stop, elapsed;
#endif
	unsigned long frameCount = 0;
	for (;;) {
#ifdef PROOFING
		elapsed = 0;
#endif
		cvData.mtx.lock();
		cvData.bufidx = bufidx;
		cvData.mtx.unlock();
		cv_gpu.ts = timestamps[bufidx];
#ifdef PROOFING
		start = getTickCount();
#endif
#if 1
		if (++frameCount >= 10) {
#ifdef CPUIMP
			raw = Mat(status.height, status.width, CV_16UC1, dev.buffers[cvData.bufidx].mem);
			raw.convertTo(rawu8, CV_8UC1, 1.f / 4.f);
#else
			raw.upload(Mat(status.height, status.width, CV_16UC1, dev.buffers[cvData.bufidx].mem));
#endif
			if (raw.empty())
				break;
		}
#ifdef PROOFING
#if defined(PROOFING_MEM) || defined(CPUIMP)
		stop = getTickCount();
		elapsed += stop - start;
#endif
#endif
#if 1
		cvData.mtx.lock();
		cvData.bufidx = -1;
		cvData.mtx.unlock();
#endif
		if (status.request & REQUEST_QUIT)
			break;

		if (frameCount < 10)
			continue;

#ifdef PROOFING
		start = getTickCount();
#endif
#ifdef CPUIMP
		Mat img, img_grey;
		cvtColor(rawu8, img_orig, CV_BayerBG2RGB);
		resize(img_orig, img, Size(), sizef, sizef);
		cvtColor(img, img_grey, CV_RGB2GRAY);
		GaussianBlur(img, img, Size(3, 3), 1.5);
#else
		raw.convertTo(rawu8, CV_8UC1, 1.f / 4.f);
		gpu::cvtColor(rawu8, img_orig, CV_BayerBG2RGB);
		gpu::resize(img_orig, img, Size(), sizef, sizef);
		gpu::cvtColor(img, img_grey, CV_RGB2GRAY);
		gpu::GaussianBlur(img, img, Size(3, 3), 1.5);
#endif
#ifdef PROOFING
		stop = getTickCount();
		elapsed += stop - start;
#endif
#if 0
		gpu::resize(*img, *img_s, Size(), 0.5, 0.5);
#endif
#else
		bayer10toRGB(status.width, status.height, dev.buffers[cvData.bufidx].mem, img_input.data);
		cvData.mtx.lock();
		cvData.bufidx = -1;
		cvData.mtx.unlock();
#endif
		//imshow("cv_input", img_input);
#if 0
#if 1
		img->download(img_input);
#else
		img_s->download(img_input);
#endif
		// by default, it shows automatically the foreground mask image
		bgs->process(img_input, img_mask, img_bkgmodel);
#else
#ifdef CPUIMP
#ifdef PROOFING
		start = getTickCount();
#endif
		img_gpu.upload(img);
#ifdef PROOFING
#if defined(PROOFING_MEM) || defined(CPUIMP)
		stop = getTickCount();
		elapsed += stop - start;
#endif
#endif
		vibe(img_gpu, fgmask);
#else
		vibe(img, fgmask);
#endif
#if 0
		gpu::blur(*fgmask, *fgmask, Size(5, 5));
#endif
#if 1
		Mat input, grey, mask;
		fgmask.download(mask);
#ifdef PROOFING
		start = getTickCount();
#endif
#ifdef CPUIMP
		input = img;
		grey = img_grey;
#else
		img.download(input);
		img_grey.download(grey);
#endif
#ifdef PROOFING
#if defined(PROOFING_MEM) || defined(CPUIMP)
		stop = getTickCount();
		elapsed += stop - start;
#endif
#endif
#if 1
		locker = cv_gpu.smpr.lock();
		cv_gpu.input = input;
		cv_gpu.grey = grey;
		cv_gpu.mask = mask;
		cv_gpu.smpr.notify();
		cv_gpu.smpr.unlock(locker);
#endif
#endif
#if 0
		medianBlur(img_mask, img_mask, 5);
#endif
#if 0
		if (status.cvShow)
			imshow("mask", img_mask);
#endif
#endif

#ifdef PROOFING
		oflog << ((float)elapsed / (float)getTickFrequency()) << endl;
#endif

		count++;
		int64_t now = getTickCount();
		if (now - past > 3 * getTickFrequency()) {
			float fps = (float)count / (now - past) * getTickFrequency();
			status.cvFPS_GPU = fps;
			count = 0;
			past = now;
		}

#if 0
		if (status.cvShow && waitKey(1) >= 0)
			status.request = REQUEST_QUIT;
#endif
		cvData.wait();
	}
	status.request = REQUEST_QUIT;

	locker = cv_gpu.smpr.lock();
	cv_gpu.smpr.notify();
	cv_gpu.smpr.unlock(locker);
	cpuThread.join();

#if 1
	if (frameCount >= 10)
		vibe.release();
#endif
}
