#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <thread>
#include <map>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/select.h>

#include "global.h"
#include "ov5647_v4l2.h"
#include "escape.h"

#define MAX_W	OV5647_MAX_W
#define MAX_H	OV5647_MAX_H

#define FILE_VERTEX_SHADER	"basic.vert"
#define FILE_FRAGMENT_SHADER	"bayer.frag"

using namespace std;
using namespace glm;

// V4L2
struct device dev;
struct v4l2_buffer buf[2];
volatile unsigned int bufidx = 0;
// OpenGL
static unsigned int texture;
static map<string, GLint> location;
static float fps;

struct status_t status;

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
	if ((ret = video_set_format(&dev, MAX_W, MAX_H, status.pixelformat)) < 0)
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
	if ((ret = video_set_format(&dev, status.width, status.height, status.pixelformat)) < 0)
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

void readRegister(const bool word, const unsigned int addr)
{
	cout << FUNC_DBG << ESC_BLUE;
	uint32_t val = (word ? OV5647_CID_REG_WMASK : 0) | ((uint32_t)addr << 16);
	int ret = video_set_control(&dev, OV5647_CID_REG_R, &val);
	if (ret)
		cerr << ESC_RED << strerror(ret) << endl;
	ret = video_get_control(&dev, OV5647_CID_REG_R, &val);
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

void inputThread()
{
	string str;
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
		return;
	} else if (cmd == "swap") {
		cout << "Change to " << (status.swap ? "non " : "") << "swapping capture" << endl;
		status.request = REQUEST_SWAP;
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
	status.swap = true;
	int err;
	// Threads
	thread *tInput, *tCV;

	// Arguments
	if (argc == 4) {
		status.width = atoi(argv[2]);
		status.height = atoi(argv[3]);
	} else if (argc == 1) {
		cerr << "Please specify the device." << endl;
		return 1;
	} else {
#if 0
		status.width = 1280;
		status.height = 960;
#elif 1
		status.width = 1920;
		status.height = 1080;
#else
		status.width = 2592;
		status.height = 1944;
#endif
	}
	status.pixelformat = V4L2_PIX_FMT_SBGGR10;
	printf("Requested video size: %ux%u\n", status.width, status.height);

	/* Initialize the library */
	if (!glfwInit()) {
		err = -1;
		goto failed;
	}

	// Initialise V4L2
	if ((err = video_init(&dev, argv[1], status.pixelformat, MAX_W, MAX_H, 4)) != 0)
		goto initFailed;
	if ((err = video_set_format(&dev, status.width, status.height, status.pixelformat)) < 0)
		goto failed;

	/* Create a windowed mode window and its OpenGL context */
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	window = glfwCreateWindow(status.width, status.height, "Hello World", NULL, NULL);
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
	glBufferData(GL_PIXEL_UNPACK_BUFFER, status.width * status.height * 2, 0, GL_DYNAMIC_READ);
#endif
	//glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_R16UI, status.width, status.height);

	setupVertices();

	glfwSetWindowRefreshCallback(window, refreshCB);
	glfwSetKeyCallback(window, keyCB);
	refreshCB(window);

	// Setup threads
	tInput = new thread(inputThread);
	tCV = new thread(cvThread);

	/* Start streaming. */
	if ((err = video_enable(&dev, 1)) != 0)
		goto failed;

	past = glfwGetTime();
	count = 0;

restart:
	if (status.swap) {
		setLED(1);
		if ((err = video_capture(&dev, &buf[bufidx])) != 0)
			goto captureFailed;
		setLED(0);
		bufidx = !bufidx;
	}

	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window)) {
		setLED(1);
		if ((err = video_capture(&dev, &buf[bufidx])) != 0)
			goto captureFailed;
		setLED(0);
		if (status.swap)
			bufidx = !bufidx;

#if 0
		//setLED(1);
		//printf("%s: buffer %u\n", __func__, buf[bufidx].index);
		//dev.buffers[buf.index].mem, buf.bytesused;
		glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, status.width * status.height * 2, dev.buffers[buf[bufidx].index].mem);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, status.width, status.height, GL_RED_INTEGER,
				GL_UNSIGNED_SHORT, 0);
		//setLED(0);
#endif

		if ((err = video_buffer_requeue(&dev, &buf[bufidx])) != 0)
			goto captureFailed;

#if 0
		/* Render here */
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		/* Swap front and back buffers */
		glfwSwapBuffers(window);
#endif

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

		// Better use a mutex or queue?
		switch (status.request) {
		case REQUEST_CAPTURE:
			printf("saveCapture(): %d\n", saveCapture());
			status.request = REQUEST_NONE;
			goto restart;
		case REQUEST_SWAP:
			status.swap = !status.swap;
			status.request = REQUEST_NONE;
			if (status.swap)
				goto restart;
			bufidx = !bufidx;
			if ((err = video_buffer_requeue(&dev, &buf[bufidx])) != 0)
				goto captureFailed;
			continue;
		case REQUEST_QUIT:
			goto quit;
		}
		/* Poll for and process events */
		glfwPollEvents();
	}

quit:
captureFailed:
	video_enable(&dev, 0);
	tInput->detach();
	delete tInput;
	tCV->join();
	delete tCV;
failed:
	video_close(&dev);
initFailed:
	glfwTerminate();
	return err;
}
