#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <thread>
#include <map>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include "global.h"
#include "escape.h"
#include "camera.h"

using namespace std;

#define FUNC_DBG	ESC_GREY << __func__ << ": "

void writeRegister(const bool word, const unsigned int addr, const unsigned int value)
{
	//cout << FUNC_DBG << ESC_BLUE;
	uint32_t val = (word ? OV5647_CID_REG_WMASK : 0) | ((uint32_t)addr << 16) | value;
	int ret = video_set_control(&dev, OV5647_CID_REG_W, &val);
	if (ret)
		cerr << ESC_RED << strerror(ret) << endl;
#if 0
	else if (word)
		printf("0x%04x", val & 0xffff);
	else
		printf("0x%02x", val & 0xff);
	cout << endl;
#endif
}

unsigned int readRegister(const bool word, const unsigned int addr, const bool print)
{
	cout << FUNC_DBG << ESC_BLUE;
	uint32_t val = (word ? OV5647_CID_REG_WMASK : 0) | ((uint32_t)addr << 16);
	int ret = video_set_control(&dev, OV5647_CID_REG_R, &val);
	if (ret)
		cerr << ESC_RED << strerror(ret) << endl;
	ret = video_get_control(&dev, OV5647_CID_REG_R, &val);
	if (ret)
		cerr << ESC_RED << strerror(ret) << endl;
	else if (!print)
		return val;
	else if (word)
		printf("0x%04x", val & 0xffff);
	else
		printf("0x%02x", val & 0xff);
	cout << endl;
	return val;
}

unsigned int setFPS(float fps)
{
	static unsigned int lines_prev = CAM_LINES;
	fps = max(fps, FPS_MIN);
	fps = min(fps, FPS_MAX);
	unsigned int lines = round((1.f / fps * 1000.f - CAM_ITVL) / CAM_LINEITVL * 1000.f + CAM_LINES);
	if (abs(lines - lines_prev) > CAM_CHANGE)
		lines = lines > lines_prev ? lines_prev + CAM_CHANGE : lines_prev - CAM_CHANGE;
	lines_prev = lines;
	// Total lines
	writeRegister(true, 0x380e, lines);
	return lines;
}

void fixed(const bool e)
{
	if (e) {
		// AGC AEC manual
		writeRegister(false, 0x3503, 3);
		// AWB auto
		writeRegister(false, 0x5001, 0);
		cout << FUNC_DBG << ESC_CYAN << "Disabled auto AGC, AEC, AWD" << endl;
	} else {
		writeRegister(false, 0x3503, 0);
		writeRegister(false, 0x5001, 1);
		cout << FUNC_DBG << ESC_CYAN << "Enabled auto AGC, AEC, AWD" << endl;
	}
}

static void resolution()
{
}

void inputThread()
{
	writeRegister(false, 0, 0);
	string str;
	cin >> hex;
	cout << hex;
loop:
	cout << ESC_GREY << __func__ << ESC_GREEN << "> " << ESC_DEFAULT;
	getline(cin, str);
	istringstream sstr(str);
	sstr >> hex;
	string cmd;
	if (!(sstr >> cmd))
		goto loop;
	bool word = false;
	if (cmd == "fps") {
		float e;
		if (!(sstr >> e)) {
			cout << "  Video: " << status.vFPS << endl;
			cout << "Preview: " << status.pvFPS << endl;
			cout << " CV GPU: " << status.cvFPS_GPU << endl;
			cout << " CV CPU: " << status.cvFPS_CPU << endl;
			cout << "CV disp: " << status.cvFPS_disp << endl;
		} else {
			unsigned int l = setFPS(e);
			cout << "Set camera FPS: " << e << endl;
			cout << " Output liness: " << l << endl;
		}
	} else if (cmd == "rb" || (word = (cmd == "rw"))) {
		unsigned int addr;
		if (!(sstr >> addr))
			goto loop;
		readRegister(word, addr, true);
	} else if (cmd == "wb" || (word = (cmd == "ww"))) {
		unsigned int addr, value;
		if (!(sstr >> addr >> value))
			goto loop;
		writeRegister(word, addr, value);
	} else if (cmd == "quit") {
		status.request = REQUEST_QUIT;
		return;
	} else if (cmd == "swap") {
		cout << "Change to " << (status.swap ? "non " : "") << "swapping capture" << endl;
		status.request = REQUEST_SWAP;
	} else if (cmd == "fixed") {
		unsigned int e;
		if (!(sstr >> e))
			goto loop;
		fixed(e);
	} else if (cmd == "pv") {
		unsigned int e;
		if (!(sstr >> e))
			goto loop;
		status.pvUpdate = e;
	} else if (cmd == "cv") {
		unsigned int e;
		if (!(sstr >> e))
			goto loop;
		status.cvShow = e;
	} else if (cmd == "res") {
		resolution();
	} else if (cmd == "cap") {
		status.request = REQUEST_CAPTURE;
	}
	goto loop;
}
