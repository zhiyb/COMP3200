#include <stdint.h>
#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>
#include "opencv2/nonfree/gpu.hpp"
#include "global.h"

#if 0
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
#include "package_bgs/AdaptiveBackgroundLearning.h"
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
#include "package_bgs/AdaptiveBackgroundLearning.h"
#include "package_bgs/ae/KDE.h"
#include "package_bgs/db/IndependentMultimodalBGS.h"
#include "package_bgs/sjn/SJN_MultiCueBGS.h"
#include "package_bgs/bl/SigmaDeltaBGS.h"

#include "package_bgs/pl/SuBSENSE.h"
#include "package_bgs/pl/LOBSTER.h"
#endif

using namespace std;
using namespace cv;

struct thread_t cvData;

//void bayer10toRGB(unsigned int width, unsigned int height, void *input, void *output);

void cvThread()
{
	// OpenCV GPU
	//cout << __func__ << ": Cuda devices: " << gpu::getCudaEnabledDeviceCount() << endl;
	if (gpu::getCudaEnabledDeviceCount() == 0) {
		status.request = REQUEST_QUIT;
		return;
	}
	gpu::setDevice(0);

#if 0
	/* Background Subtraction Methods */
	IBGS *bgs;

	/*** Default Package ***/
	//bgs = new FrameDifferenceBGS;
	//bgs = new StaticFrameDifferenceBGS;
	//bgs = new WeightedMovingMeanBGS;
	//bgs = new WeightedMovingVarianceBGS;
	//bgs = new MixtureOfGaussianV1BGS;
	bgs = new MixtureOfGaussianV2BGS;
	//bgs = new AdaptiveBackgroundLearning;
	//bgs = new AdaptiveSelectiveBackgroundLearning;
	//bgs = new GMG;

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
#endif

	Mat img_input(status.height, status.width, CV_8UC3);
	Mat img_mask;
	Mat img_bkgmodel;
#if 1
	gpu::GpuMat *raw = new gpu::GpuMat(status.height, status.width, CV_16UC1);
	gpu::GpuMat *rawu8 = new gpu::GpuMat(status.height, status.width, CV_8UC1);
	gpu::GpuMat *img = new gpu::GpuMat(status.height, status.width, CV_8UC3);
#if 0
	gpu::GpuMat *img_s = new gpu::GpuMat(status.height / 2, status.width / 2, CV_8UC3);
#endif
	gpu::GpuMat *fgmask = new gpu::GpuMat;
#endif

#if 1
	gpu::VIBE_GPU *vibe = new gpu::VIBE_GPU;
#endif

	// Waiting for ready start
	cvData.mtx.lock();
	cvData.mtx.unlock();

	int64_t past = getTickCount(), count = 0;
	unsigned long frameCount = 0;
	while (status.request != REQUEST_QUIT) {
		cvData.mtx.lock();
		cvData.bufidx = bufidx;
		cvData.mtx.unlock();
#if 1
		if (raw->empty())
			break;
		raw->upload(Mat(status.height, status.width, CV_16UC1, dev.buffers[cvData.bufidx].mem));
#if 1
		cvData.mtx.lock();
		cvData.bufidx = -1;
		cvData.mtx.unlock();
#endif

		raw->convertTo(*rawu8, CV_8UC1, 1.f / 4.f);
		gpu::cvtColor(*rawu8, *img, CV_BayerBG2RGB);
#if 1
		gpu::GaussianBlur(*img, *img, Size(5, 5), 1.5);
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
#if 1Enable
		img->download(img_input);
#else
		img_s->download(img_input);
#endif
		// by default, it shows automatically the foreground mask image
		bgs->process(img_input, img_mask, img_bkgmodel);
#else
		if (frameCount == 10)
			vibe->initialize(*img);
		else if (frameCount > 10) {
			(*vibe)(*img, *fgmask);
#if 0
			gpu::blur(*fgmask, *fgmask, Size(5, 5));
#endif
#if 1
			fgmask->download(img_mask);
#endif
#if 0
			medianBlur(img_mask, img_mask, 5);
#endif
#if 1
			if (status.cvShow)
				imshow("mask", img_mask);
#endif
		}
#endif

		count++;
		frameCount++;
		int64_t now = getTickCount();
		if (now - past > 3 * getTickFrequency()) {
			float fps = (float)count / (now - past) * getTickFrequency();
			status.cvFPS = fps;
			count = 0;
			past = now;
		}

#if 1
		if (status.cvShow && waitKey(1) >= 0)
			status.request = REQUEST_QUIT;
#endif
		cvData.wait();
	}
	status.request = REQUEST_QUIT;
#if 1
	if (frameCount >= 10)
		vibe->release();
	delete vibe;
	delete fgmask;
#if 0
	delete img_s;
#endif
	delete img;
	delete rawu8;
	delete raw;
#endif
}
