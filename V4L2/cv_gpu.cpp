#include <thread>
#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>
#include "opencv2/nonfree/gpu.hpp"
#include "global.h"
#include "cv_private.h"

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
	gpu::GpuMat raw(status.height, status.width, CV_16UC1);
	gpu::GpuMat rawu8(status.height, status.width, CV_8UC1);
	gpu::GpuMat img_orig(status.height, status.width, CV_8UC3);
	gpu::GpuMat img(status.height * sizef, status.width * sizef, CV_8UC3);
	gpu::GpuMat img_grey(status.height * sizef, status.width * sizef, CV_8UC1);
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

	int64_t past = getTickCount(), count = 0;
	unsigned long frameCount = 0;
	for (;;) {
		cvData.mtx.lock();
		cvData.bufidx = bufidx;
		cvData.mtx.unlock();
		cv_gpu.ts = timestamps[bufidx];
#if 1
		if (++frameCount >= 10) {
			raw.upload(Mat(status.height, status.width, CV_16UC1, dev.buffers[cvData.bufidx].mem));
			if (raw.empty())
				break;
		}
#if 1
		cvData.mtx.lock();
		cvData.bufidx = -1;
		cvData.mtx.unlock();
#endif
		if (status.request & REQUEST_QUIT)
			break;

		if (frameCount < 10)
			continue;

		raw.convertTo(rawu8, CV_8UC1, 1.f / 4.f);
		gpu::cvtColor(rawu8, img_orig, CV_BayerBG2RGB);
		gpu::resize(img_orig, img, Size(), sizef, sizef);
		gpu::cvtColor(img, img_grey, CV_RGB2GRAY);
#if 1
		gpu::GaussianBlur(img, img, Size(3, 3), 1.5);
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
		vibe(img, fgmask);
#if 0
		gpu::blur(*fgmask, *fgmask, Size(5, 5));
#endif
#if 1
		Mat input, grey, mask;
		fgmask.download(mask);
		img.download(input);
		img_grey.download(grey);
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
