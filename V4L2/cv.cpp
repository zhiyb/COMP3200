#include <stdint.h>
#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>
#include "global.h"

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

using namespace std;
using namespace cv;

struct thread_t cvData;

void cvThread()
{
	// OpenCV GPU
	cout << __func__ << ": Cuda devices: " << gpu::getCudaEnabledDeviceCount() << endl;
	if (gpu::getCudaEnabledDeviceCount() == 0) {
		status.request = REQUEST_QUIT;
		return;
	}
	gpu::setDevice(0);

	/* Background Subtraction Methods */
	IBGS *bgs;
	/*** Default Package ***/
	//bgs = new FrameDifferenceBGS;
	/*** JMO Package (thanks to Jean-Marc Odobez) ***/
	bgs = new MultiLayerBGS;

	Mat img_input(status.height, status.width, CV_8UC3);
	Mat img_mask;
	Mat img_bkgmodel;
	gpu::GpuMat *raw;
	gpu::GpuMat *rawu8;
	gpu::GpuMat *img;

	// Waiting for ready start
	cvData.mtx.lock();
	cvData.mtx.unlock();

	int64_t past = getTickCount(), count = 0;
	while (status.request != REQUEST_QUIT) {
		raw = new gpu::GpuMat();
		cvData.mtx.lock();
		cvData.bufidx = bufidx;
		cvData.mtx.unlock();
		raw->upload(Mat(status.height, status.width, CV_16UC1, dev.buffers[cvData.bufidx].mem));
		if (raw->empty())
			break;
		cvData.mtx.lock();
		cvData.bufidx = -1;
		cvData.mtx.unlock();

		rawu8 = new gpu::GpuMat();
		raw->convertTo(*rawu8, CV_8UC1, 1.f / 4.f);
		delete raw;

		img = new gpu::GpuMat();
		gpu::cvtColor(*rawu8, *img, CV_BayerBG2RGB);
		delete rawu8;

		img->download(img_input);
		delete img;
		imshow("cv_input", img_input);

		// by default, it shows automatically the foreground mask image
		bgs->process(img_input, img_mask, img_bkgmodel);

		count++;
		int64_t now = getTickCount();
		if (now - past > 3 * getTickFrequency()) {
			float fps = (float)count / (now - past) * getTickFrequency();
			status.cvFPS = fps;
			count = 0;
			past = now;
		}

		if (waitKey(1) >= 0)
			status.request = REQUEST_QUIT;
	}
	status.request = REQUEST_QUIT;
}
