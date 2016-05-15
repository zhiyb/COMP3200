#include <stdint.h>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>
#include "opencv2/nonfree/gpu.hpp"
#include "global.h"
#include "camera.h"
#include "tslog.h"
#include "cv_private.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "helper.h"

#define ADAPTIVE
#define MOVE_MAX	32
#define BLOB_SIZE	7
#define OF_SIZE		32

using namespace std;
using namespace glm;
using namespace cv;

static GLuint vao[2], bData;

static map<string, GLint> location;

static void refreshCB(GLFWwindow *window)
{
	int width, height;
	glfwGetWindowSize(window, &width, &height);
	glViewport(0, 0, width, height);
}

static void setupVertices()
{
	static const vec2 vertices[4] = {{1.f, 1.f}, {1.f, -1.f}, {-1.f, -1.f}, {-1.f, 1.f}};

	glGenVertexArrays(2, vao);
	glBindVertexArray(vao[0]);

	GLuint bVertex;
	glGenBuffers(1, &bVertex);
	glBindBuffer(GL_ARRAY_BUFFER, bVertex);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), (void *)vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(location["position"]);
	glVertexAttribPointer(location["position"], 2, GL_FLOAT, GL_FALSE, 0, 0);

	glBindVertexArray(vao[1]);
	glGenBuffers(1, &bData);
	glBindBuffer(GL_ARRAY_BUFFER, bData);
	glEnableVertexAttribArray(location["data"]);
	glVertexAttribPointer(location["data"], 2, GL_FLOAT, GL_FALSE, 0, 0);
}

static void getUniforms(GLuint program, const char **uniforms)
{
	if (!program)
		return;
	while (*uniforms) {
		location[*uniforms] = glGetUniformLocation(program, *uniforms);
		uniforms++;
	}
}

void maskEnhance(Mat mask)
{
#if 1
	//Mat img_mask(mask.size(), mask.type(), Scalar(0.f));
#if 1
	// find blobs
	std::vector<std::vector<cv::Point> > v;
	std::vector<cv::Vec4i> hierarchy;
	cv::findContours(mask, v, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
	mask = Scalar(0);
	for (size_t i=0; i < v.size(); ++i)
	{
		// drop smaller blobs
		if (cv::contourArea(v[i]) < BLOB_SIZE * BLOB_SIZE)
			continue;
		// draw filled blob
		cv::drawContours(mask, v, i, cv::Scalar(255,0,0), CV_FILLED, 8, hierarchy, 0, cv::Point());
	}
#endif

	// morphological closure
	cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(BLOB_SIZE, BLOB_SIZE));
	cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, element);
#endif
}

// OpenCV CPU post processing
void cvThread_CPU()
{
	TSLog tsActual;

	GLFWwindow *window;
	/* Initialize the library */
	if (!glfwInit()) {
		return;
	}

	/* Create a windowed mode window and its OpenGL context */
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	window = glfwCreateWindow(status.width, status.height, "Hello World", NULL, NULL);
	glfwSetWindowPos(window, 0, 0);
	if (!window) {
		glfwTerminate();
		return;
	}

	/* Make the window's context current */
	glfwMakeContextCurrent(window);
	glfwSwapInterval(2);
	glewExperimental = GL_TRUE;
	glewInit();
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_LINE_SMOOTH);
	glLineWidth(3);

	GLuint programFPS;
	shader_t shadersFPS[] = {
		{GL_VERTEX_SHADER, "cv_fps.vert"},
		{GL_FRAGMENT_SHADER, "cv_fps.frag"},
		{0, NULL}
	};

	/* Setup render program */
	programFPS = setupProgramFromFiles(shadersFPS);
	if (programFPS == 0) {
		glfwTerminate();
		return;
	}
	glUseProgram(programFPS);
	location["data"] = glGetAttribLocation(programFPS, "data");
	location["last"] = glGetUniformLocation(programFPS, "last");
	//static const char *uniformsFPS[] = {"last", 0};
	//getUniforms(programFPS, uniformsFPS);

	GLuint program;
	shader_t shaders[] = {
		{GL_VERTEX_SHADER, "cv.vert"},
		{GL_FRAGMENT_SHADER, "cv.frag"},
		{0, NULL}
	};

	/* Setup render program */
	program = setupProgramFromFiles(shaders);
	if (program == 0) {
		glfwTerminate();
		return;
	}
	glUseProgram(program);
	location["position"] = glGetAttribLocation(program, "position");
	static const char *uniforms[] = {"sampler", "drawing", 0};
	getUniforms(program, uniforms);

	glUniform1i(location["sampler"], 0);
	glUniform1i(location["drawing"], 1);

	// Texture
#if 0
	GLuint pbo;
	glGenBuffers(1, &pbo);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, status.width * status.height * 2, 0, GL_DYNAMIC_READ);
#endif
	const int texcnt = 2;
	GLuint texture[texcnt];
	//glActiveTexture(GL_TEXTURE0);
	glGenTextures(texcnt, texture);

	glBindTexture(GL_TEXTURE_2D, texture[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
	//glTexStorage2D(GL_TEXTURE_2D, 1, GL_R16UI, status.width, status.height);

	glBindTexture(GL_TEXTURE_2D, texture[1]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTexStorage2D(GL_TEXTURE_2D, 1, GL_R16UI, status.width, status.height);

	setupVertices();

	glfwSetWindowRefreshCallback(window, refreshCB);

	// OpenCV
	std::unique_lock<std::mutex> locker;

	Mat grey_prev;//, drawing;
	float sizef = 0.5f;
	Mat drawing(status.height * sizef, status.width * sizef, CV_8UC3);
	vector<vector<Point> > *prev_contours = 0;

#ifdef ADAPTIVE
	float fps = FPS_MAX;
#endif
	int64_t past = getTickCount(), count = 0;
	struct timeval ts, ts_prev, ts_init;
	gettimeofday(&ts_init, 0);
	ts_prev.tv_sec = 0;
	ts_prev.tv_usec = 0;
	unsigned long frameCount = 0;
	while (1) {
		glUseProgram(program);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture[0]);
		locker = cv_gpu.smpr.lock();
		cv_gpu.smpr.wait(locker);
		ts.tv_sec = cv_gpu.ts.tv_sec - ts_init.tv_sec;
		ts.tv_usec = cv_gpu.ts.tv_usec - ts_init.tv_usec;
		Mat input(cv_gpu.input);
		Mat mask(cv_gpu.mask);
		Mat grey(cv_gpu.grey);
		//glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, status.width * status.height * 2, cv_gpu.raw);
		//glBufferData(GL_PIXEL_UNPACK_BUFFER, status.width * status.height * 2, cv_gpu.raw, GL_DYNAMIC_READ);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, status.width, status.height, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, cv_gpu.raw);
		//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, status.width, status.height, GL_RED_INTEGER,
		//		GL_UNSIGNED_SHORT, 0);
		cv_gpu.smpr.unlock(locker);
		if (glfwWindowShouldClose(window) || status.request == REQUEST_QUIT)
			break;
		if (input.empty())
			continue;

		//clog << "TS add" << endl;
#ifdef ADAPTIVE
		double dts = (double)ts.tv_sec + (double)ts.tv_usec / 1000000.f;
		double dts_prev = (double)ts_prev.tv_sec + (double)ts_prev.tv_usec / 1000000.f;
		double itvl = dts - dts_prev;
#endif
		ts_prev = ts;
		tsActual.add(dts);
#if 0
		if (status.cvShow) {d
			imshow("input", input);
			//imshow("grey", grey);
			//imshow("mask", mask);
		}
#endif

		// Enhance foreground mask
		maskEnhance(mask);

		Mat grey_masked;
		//grey.copyTo(grey_masked, mask);
		grey.copyTo(grey_masked);

#if 0
		if (status.cvShow) {
			imshow("input OF", grey_masked);
		}
#endif

		// Extract contours
		//input.copyTo(drawing);
		drawing = Scalar(0);
		vector<vector<Point> > *contours = new vector<vector<Point> >;
#if 1
		{
			/// Find contours
			std::vector<cv::Vec4i> hierarchy;
			//std::vector<cv::Vec4i> hierarchy;
			cv::findContours(mask, *contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

			/// Approximate contours to polygons + get bounding rects and circles
			vector<vector<Point> > contours_poly(contours->size());

			/// Draw polygonal contour + bonding rects +d circles
			for (size_t i = 0; i < contours->size(); i++) {
				// drop smaller blobs
				if (cv::contourArea((*contours)[i]) < BLOB_SIZE)
					continue;

				//Scalar color = Scalar(rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255));
				Scalar color(127.f, 0.f, 0.f);
				cv::drawContours(drawing, *contours, i, color, 1, 8, hierarchy, 0, Point());
			}
		}
#endif

		// Optical flow tracking between frames
		vector<Point2f> prevPts, nextPts;
		vector<uchar> ofStatus;
		if (!grey_prev.empty() && prev_contours != 0) {
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
				vector<float> err;
				//TermCriteria termcrit(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.03);
				Size winSize(OF_SIZE, OF_SIZE);
				calcOpticalFlowPyrLK(grey_prev, grey_masked, prevPts, nextPts, ofStatus, err, winSize, 1);
			}
		}
		grey_prev = grey_masked;

		if (prev_contours)
			delete prev_contours;
		prev_contours = contours;

		double dismax = 0.f;
		Point2f dismaxp;
		if (prevPts.size() > 0) {
			Mat d;
			absdiff(Mat(prevPts), Mat(nextPts), d);
			vector<Point2f> diff;
			d.copyTo(diff);
			for (size_t i = 0; i < prevPts.size(); i++) {
				if (!ofStatus[i])
					continue;
				double dis = norm(diff[i]);
#ifdef ADAPTIVE
				if (dis > fps * MOVE_MAX)
					continue;
#endif
				if (dis > dismax) {
					dismax = dis;
					dismaxp = diff[i];
				}
				//line(drawing, center[i], next_points[i], colour, 5);
				line(drawing, prevPts[i], nextPts[i], Scalar(0.f, 127.f, 0.f), 1, 8);
				//circle(drawing, prevPts[i], 3, Scalar(0.f, 0.f, 255.f), 1, 8);
				//circle(drawing, nextPts[i], 2, Scalar(0.f, 0.f, 127.f), 1, 8);
			}
		}

#if 0
		if (status.cvShow) {
			imshow("output OF", drawing);
		}
#endif
#ifdef ADAPTIVE
		// Adaptive FPS calculation
		fps = dismax / itvl / (float)OF_SIZE;
		fps = std::fmax(fps, (float)FPS_MIN);
		fps = std::fmin(fps, (float)FPS_MAX);
		//cout << fps << ", " << getFPS() << endl;
		status.fps_request = fps;
		//setFPS(fps);
#endif

		//clog << "Render OF" << endl;
		glBindVertexArray(vao[0]);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, texture[1]);
		//clog << "Render OF texture" << endl;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, drawing.cols, drawing.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, drawing.data);
		/*glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, status.width * status.height * 2, drawing.data);
		glActiveTexture(GL_TEXTURE1);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, status.width, status.height, GL_RED_INTEGER,
				GL_UNSIGNED_SHORT, 0);*/

		//clog << "Render OF draw" << endl;
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		//clog << "Render FPS" << endl;
		glUseProgram(programFPS);
		glBindVertexArray(vao[1]);
		glBindBuffer(GL_ARRAY_BUFFER, bData);
		glBufferData(GL_ARRAY_BUFFER, tsActual.size(), tsActual.data(), GL_DYNAMIC_DRAW);
		glUniform1f(location["last"], tsActual.last());
		//glVertexAttribPointer(location["data"], 2, GL_FLOAT, GL_FALSE, 0, 0);
		//clog << tsActual.size() << ", " << tsActual.last() << endl;
		//clog << location["data"] << ", " << location["last"] << endl;
		glDrawArrays(GL_LINE_STRIP, 0, tsActual.count());

		glfwSwapBuffers(window);
		count++;
		frameCount++;
		int64_t now = getTickCount();
		if (now - past > 3 * getTickFrequency()) {
			float fps = (float)count / (now - past) * getTickFrequency();
			status.cvFPS_disp = status.cvFPS_CPU = fps;
			count = 0;
			past = now;
			char buf[32];
			sprintf(buf, "%g FPS", fps);
			glfwSetWindowTitle(window, buf);
		}

#if 0
		if (status.cvShow && waitKey(1) >= 0)
			break;
#endif
		glfwPollEvents();
	}
	status.request = REQUEST_QUIT;
	delete prev_contours;
}
