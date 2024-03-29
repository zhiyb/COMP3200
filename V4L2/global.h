#ifndef GLOBAL_H
#define GLOBAL_H

#include <mutex>
#include <condition_variable>
#include "yavta.h"
#include "ov5647_v4l2.h"

// Preview
#define ENABLE_PV	0
// OpenCV
#define ENABLE_CV	1

#define MAX_W		OV5647_MAX_W
#define MAX_H		OV5647_MAX_H
#define BUFFER_NUM	(3 + ENABLE_PV + ENABLE_CV)

enum {	REQUEST_NONE = 0x00,
	REQUEST_CAPTURE,
	REQUEST_QUIT,
	REQUEST_SWAP,
	REQUEST_FPS,
};

extern struct status_t {
	unsigned int width, height, pixelformat;
	volatile unsigned int request;
	volatile bool swap, pvUpdate, cvShow;
	// Video, Preview, CV FPS
	volatile float vFPS, pvFPS, cvFPS_GPU, cvFPS_CPU, cvFPS_disp;
	volatile float fps_request;
} status;

// V4L2
extern struct device dev;
extern volatile unsigned int bufidx;
extern volatile int64_t timestamps[BUFFER_NUM];
extern struct timeval timevals[BUFFER_NUM];

class semaphore_t
{
public:
	std::unique_lock<std::mutex> lock()
	{
		return std::unique_lock<std::mutex>(mtx);
	}
	void unlock(std::unique_lock<std::mutex> &locker)
	{
		if (locker.owns_lock()) {
			locker.unlock();
			locker.release();
		}
	}
	void notify()
	{
		//std::unique_lock<std::mutex> lock(mtx);
		//ready = true;
		cvar.notify_all();
	}
	void wait(std::unique_lock<std::mutex> &locker)
	{
		//while (!ready)
			cvar.wait(locker);
		//ready = false;
	}

private:
	std::mutex mtx;
	std::condition_variable cvar;
	//bool ready;
};

struct thread_t {
	std::mutex mtx;
	volatile int bufidx;
	volatile int err;
	semaphore_t smpr;

	bool ready;
	void notify()
	{
		std::unique_lock<std::mutex> locker = smpr.lock();
		ready = true;
		smpr.notify();
		smpr.unlock(locker);
	}
	void wait()
	{
		std::unique_lock<std::mutex> locker = smpr.lock();
		while (!ready)
			smpr.wait(locker);
		ready = false;
		smpr.unlock(locker);
	}
};
#if ENABLE_PV
extern struct thread_t pvData;
#endif
#if ENABLE_CV
extern struct thread_t cvData;
#endif

void inputThread();
void cvThread();
void pvThread();
void writeRegister(const bool word, const unsigned int addr, const unsigned int value);
unsigned int readRegister(const bool word, const unsigned int addr, const bool print = false);
float getFPS(unsigned int lines = 0);
unsigned int setFPS(float fps, bool limit = true);
void fixed(const bool e);

#endif
