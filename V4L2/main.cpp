#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "helper.h"
#include "yavta.h"
#include "ov5647_v4l2.h"
#include "escape.h"

#define MAX_W	OV5647_MAX_W
#define MAX_H	OV5647_MAX_H
//#define CAPTURE_SWAP_BUFFER

#define FILE_VERTEX_SHADER	"basic.vert"
#define FILE_FRAGMENT_SHADER	"bayer.frag"

using namespace std;
using namespace glm;

// V4L2
static struct device dev;
static struct v4l2_buffer buf[2];
static unsigned int bufidx = 0;
// OpenGL
static unsigned int width, height, texture, pixelformat;
static map<string, GLint> location;
static float fps;

enum {	REQUEST_NONE = 0x00,
	REQUEST_CAPTURE,
	REQUEST_QUIT,
	REQUEST_DEBUG,
};
static struct status_t {
	volatile unsigned int request;
	volatile int debug;
} status;

static inline int setLED(uint32_t enable)
{
	return video_set_control(&dev, OV5647_CID_LED, &enable);
}

int saveCapture()
{
	int ret;
	if ((ret = video_enable(&dev, 0)) != 0)
		return ret;

	// Set maximum resolution
	printf("%s: Saveing capture...\n", __func__);
	if ((ret = video_set_format(&dev, MAX_W, MAX_H, pixelformat)) < 0)
		return ret;
	if ((ret = video_queue_buffer(&dev, 0)) < 0)
		return ret;
	if ((ret = video_enable(&dev, 1)) != 0)
		return ret;

	// Capture
	setLED(1);
	if ((ret = video_capture(&dev, &buf[bufidx])) != 0)
		return ret;
	setLED(0);

	// Restore state
	if ((ret = video_enable(&dev, 0)) != 0)
		return ret;
	if ((ret = video_set_format(&dev, width, height, pixelformat)) < 0)
		return ret;
	/* Queue the buffers. */
	unsigned int i;
	for (i = 0; i < dev.nbufs; ++i) {
		int ret = video_queue_buffer(&dev, i);
		if (ret < 0)
			return ret;
	}
	return video_enable(&dev, 1);
}

#define FUNC_DBG	ESC_GREY << __func__ << ": "

void writeRegister(const bool word, const unsigned int addr, const unsigned int value)
{
	cout << FUNC_DBG << ESC_BLUE;
	uint32_t val = (word ? OV5647_CID_REG_WMASK : 0) | ((uint32_t)addr << 16) | value;
	int ret = video_set_control(&dev, OV5647_CID_REG_W, &val);
	if (ret)
		cerr << ESC_RED << strerror(ret) << endl;
	else if (word)
		printf("0x%04x", val & 0xffff);
	else
		printf("0x%02x", val & 0xff);
	cout << endl;
}

void readRegister(const bool word, const unsigned int addr)
{
	cout << FUNC_DBG << ESC_BLUE;
	uint32_t val = (word ? OV5647_CID_REG_WMASK : 0) | ((uint32_t)addr << 16);
	int ret = video_set_control(&dev, OV5647_CID_REG_R, &val);
	if (ret)
		cerr << ESC_RED << strerror(ret) << endl;
	else if (word)
		printf("0x%04x", val & 0xffff);
	else
		printf("0x%02x", val & 0xff);
	cout << endl;
}

void resolution()
{
}

void *inputThread(void *param)
{
	string str;
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	cin >> hex;
	cout << hex;
loop:
	cout << ESC_GREY << __func__ << ": " << ESC_GREEN << "> " << ESC_DEFAULT;
	getline(cin, str);
	istringstream sstr(str);
	sstr >> hex;
	string cmd;
	if (!(sstr >> cmd))
		goto loop;
	bool word = false;
	if (cmd == "fps") {
		cout << "FPS: " << fps << endl;
	} else if (cmd == "rb" || (word = (cmd == "rw"))) {
		unsigned int addr;
		if (!(sstr >> addr))
			goto loop;
		readRegister(word, addr);
	} else if (cmd == "wb" || (word = (cmd == "ww"))) {
		unsigned int addr, value;
		if (!(sstr >> addr >> value))
			goto loop;
		writeRegister(word, addr, value);
	} else if (cmd == "quit") {
		status.request = REQUEST_QUIT;
		return 0;
	} else if (cmd == "res") {
		resolution();
	} else if (cmd == "cap") {
		status.request = REQUEST_CAPTURE;
	}
	goto loop;
}

void refreshCB(GLFWwindow *window)
{
	int width, height;
	glfwGetWindowSize(window, &width, &height);
	glViewport(0, 0, width, height);
	//GLfloat aspi = (GLfloat)height / (GLfloat)width;
	//mat4 projection;// = ortho<GLfloat>(-1.f, 1.f, -aspi, aspi, -1, 1);
	//glUniformMatrix4fv(location["projection"], 1, GL_FALSE, (GLfloat *)&projection);
}

void keyCB(GLFWwindow * /*window*/, int key, int /*scancode*/, int action, int /*mods*/)
{
	if (action != GLFW_PRESS && action != GLFW_REPEAT)
		return;

	switch (key) {
	case GLFW_KEY_ESCAPE:
	case GLFW_KEY_Q:
		status.request = REQUEST_QUIT;
		return;
	case GLFW_KEY_S:
		status.request = REQUEST_CAPTURE;
		return;
	case GLFW_KEY_D:
		status.debug = 0;
		return;
	}
}

void setupVertices()
{
	static const vec2 vertices[4] = {{1.f, 1.f}, {1.f, -1.f}, {-1.f, -1.f}, {-1.f, 1.f}};

	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	GLuint bVertex;
	glGenBuffers(1, &bVertex);
	glBindBuffer(GL_ARRAY_BUFFER, bVertex);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), (void *)vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(location["position"]);
	glVertexAttribPointer(location["position"], 2, GL_FLOAT, GL_FALSE, 0, 0);
}

void getUniforms(GLuint program, const char **uniforms)
{
	if (!program)
		return;
	while (*uniforms) {
		location[*uniforms] = glGetUniformLocation(program, *uniforms);
		uniforms++;
	}
}

int main(int argc, char *argv[])
{
	if (argc == 3) {
		width = atoi(argv[1]);
		height = atoi(argv[2]);
	} else {
#if 0
		width = 1280;
		height = 960;
#elif 1
		width = 1920;
		height = 1080;
#else
		width = 2592;
		height = 1944;
#endif
	}
	pixelformat = V4L2_PIX_FMT_SBGGR10;
	printf("Requested video size: %ux%u\n", width, height);

	// OpenGL variables
	GLFWwindow *window;
	GLuint program;
	GLuint pbo;
	shader_t shaders[] = {
		{GL_VERTEX_SHADER, FILE_VERTEX_SHADER},
		{GL_FRAGMENT_SHADER, FILE_FRAGMENT_SHADER},
		{0, NULL}
	};
	const char *uniforms[] = {/*"projection",*/ 0};
	// FPS counter
	double past;
	unsigned int count;
	// Status
	status.request = REQUEST_NONE;
	status.debug = -1;
	int err;
	// Threads
	pthread_t pid_input;

	/* Initialize the library */
	if (!glfwInit()) {
		err = -1;
		goto failed;
	}

	// Initialise V4L2
	if ((err = video_init(&dev, "/dev/video0", pixelformat, MAX_W, MAX_H, 4)) != 0)
		goto initFailed;
	if ((err = video_set_format(&dev, width, height, pixelformat)) < 0)
		goto failed;

	/* Create a windowed mode window and its OpenGL context */
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	window = glfwCreateWindow(width, height, "Hello World", NULL, NULL);
	if (!window) {
		glfwTerminate();
		err = -1;
		goto failed;
	}

	/* Make the window's context current */
	glfwMakeContextCurrent(window);
	//glfwSwapInterval(1);
	glewExperimental = GL_TRUE;
	glewInit();
	glEnable(GL_TEXTURE_2D);

	/* Setup render program */
	program = setupProgramFromFiles(shaders);
	if (program == 0) {
		glfwTerminate();
		err = -1;
		goto failed;
	}
	glUseProgram(program);
	location["position"] = glGetAttribLocation(program, "position");
	getUniforms(program, uniforms);

	// Texture
#if 1
	glGenBuffers(1, &pbo);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * 2, 0, GL_DYNAMIC_READ);
#endif
	//glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
	//glTexStorage2D(GL_TEXTURE_2D, 1, GL_R16UI, width, height);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, test);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, NULL);

	setupVertices();

	glfwSetWindowRefreshCallback(window, refreshCB);
	glfwSetKeyCallback(window, keyCB);
	refreshCB(window);

	// Setup input handler thread
	pthread_create(&pid_input, NULL, inputThread, NULL);

	/* Start streaming. */
	if ((err = video_enable(&dev, 1)) != 0)
		goto failed;

	past = glfwGetTime();
	//capture = glfwGet/Time();
	count = 0;

restart:
#ifdef CAPTURE_SWAP_BUFFER
	setLED(1);
	if ((err = video_capture(&dev, &buf[bufidx])) != 0)
		goto captureFailed;
	setLED(0);
	bufidx = !bufidx;
#endif

	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window)) {
		setLED(1);
		if ((err = video_capture(&dev, &buf[bufidx])) != 0)
			goto captureFailed;
		setLED(0);
#ifdef CAPTURE_SWAP_BUFFER
		bufidx = !bufidx;
#endif

		//setLED(1);
		//printf("%s: buffer %u\n", __func__, buf[bufidx].index);
		//dev.buffers[buf.index].mem, buf.bytesused;
		glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, width * height * 2, dev.buffers[buf[bufidx].index].mem);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, width, height, 0, GL_RED_INTEGER,
				GL_UNSIGNED_SHORT, 0);
		//glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, width, height, 0, GL_RED_INTEGER,
		//		GL_UNSIGNED_SHORT, dev.buffers[buf[bufidx].index].mem);
		//glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, NULL);
		//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED_INTEGER,
		//		GL_UNSIGNED_SHORT, dev.buffers[buf[bufidx].index].mem);
		//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED_INTEGER,
		//		GL_UNSIGNED_SHORT, 0);
		//setLED(0);

		if ((err = video_buffer_requeue(&dev, &buf[bufidx])) != 0)
			goto captureFailed;

		/* Render here */
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		/* Swap front and back buffers */
		glfwSwapBuffers(window);

		count++;
		double now = glfwGetTime();
		if (now - past > 3) {
			fps = (float)count / (now - past);
			char buf[32];
			sprintf(buf, "%g FPS", fps);
			glfwSetWindowTitle(window, buf);
			count = 0;
			past = now;
		}

		unsigned int req = status.request;
		status.request = REQUEST_NONE;
		switch (req) {
		case REQUEST_CAPTURE:
			printf("saveCapture(): %d\n", saveCapture());
			goto restart;
		case REQUEST_QUIT:
			goto quit;
		}
		/* Poll for and process events */
		glfwPollEvents();
	}

quit:
captureFailed:
	pthread_cancel(pid_input);
	pthread_join(pid_input, NULL);
	video_enable(&dev, 0);
failed:
	video_close(&dev);
initFailed:
	glfwTerminate();
	return -1;
}
