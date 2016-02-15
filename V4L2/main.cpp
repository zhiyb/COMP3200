#include <iostream>
#include <vector>
#include <fstream>
#include <map>
#include <cstdlib>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "helper.h"
#include "yavta.h"

//#define TEST_PATTERN

#define FILE_VERTEX_SHADER	"basic.vert"
#define FILE_FRAGMENT_SHADER	"bayer.frag"

using namespace std;
using namespace glm;

unsigned int width, height, texture;
#ifndef TEST_PATTERN
struct device dev;
struct v4l2_buffer buf;
#endif
map<string, GLint> location;

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

void render()
{
#if 1
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
#endif

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void refreshCB(GLFWwindow *window)
{
	int width, height;
	glfwGetWindowSize(window, &width, &height);
	glViewport(0, 0, width, height);
	GLfloat aspi = (GLfloat)height / (GLfloat)width;
	mat4 projection = ortho<GLfloat>(-1.f, 1.f, -aspi, aspi, -1, 1);
	glUniformMatrix4fv(location["projection"], 1, GL_FALSE, (GLfloat *)&projection);
}

void keyCB(GLFWwindow * /*window*/, int key, int /*scancode*/, int action, int /*mods*/)
{
	if (action != GLFW_PRESS && action != GLFW_REPEAT)
		return;

	switch (key) {
	case GLFW_KEY_ESCAPE:
	case GLFW_KEY_Q:
#ifndef TEST_PATTERN
		video_enable(&dev, 0);
		video_close(&dev);
#endif
		glfwTerminate();
		exit(0);
		return;
	}
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

int main(int /*argc*/, char * /*argv*/[])
{
	width = 1280;
	height = 960;

#ifndef TEST_PATTERN
	int ret;
	if ((ret = video_init(&dev, "/dev/video0", V4L2_PIX_FMT_SBGGR10, width, height)) != 0)
		return ret;
#endif

	// OpenGL variables
	GLFWwindow *window;
	GLuint program;
	shader_t shaders[] = {
		{GL_VERTEX_SHADER, FILE_VERTEX_SHADER},
		{GL_FRAGMENT_SHADER, FILE_FRAGMENT_SHADER},
		{0, NULL}
	};
	const char *uniforms[] = {
		"projection", 0
	};

	unsigned short test[width * height];
	unsigned short i = 0, h, w, *ptr = test;
	for (w = 0; w != width; w++)
		for (h = 0; h != height; h++)
			*ptr++ = i++;


	/* Initialize the library */
	if (!glfwInit())
		goto failed;

	/* Create a windowed mode window and its OpenGL context */
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	window = glfwCreateWindow(1280, 960, "Hello World", NULL, NULL);
	if (!window) {
		glfwTerminate();
		goto failed;
	}

	/* Make the window's context current */
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	glewExperimental = GL_TRUE;
	glewInit();

	/* Setup render program */
	program = setupProgramFromFiles(shaders);
	if (program == 0) {
		glfwTerminate();
		goto failed;
	}
	glUseProgram(program);
	location["position"] = glGetAttribLocation(program, "position");
	getUniforms(program, uniforms);

	//glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	//glTexStorage2D(GL_TEXTURE_2D, 1, GL_R16UI, width, height);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, test);

	setupVertices();

	glfwSetWindowRefreshCallback(window, refreshCB);
	glfwSetKeyCallback(window, keyCB);
	refreshCB(window);

#ifndef TEST_PATTERN
	/* Start streaming. */
	if (video_enable(&dev, 1) != 0)
		goto failed;
#endif

	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window)) {
#ifndef TEST_PATTERN
		if (video_capture(&dev, &buf) != 0)
			goto captureFailed;
#endif

#ifndef TEST_PATTERN
		//dev.buffers[buf.index].mem, buf.bytesused;
		//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_SHORT, dev.buffers[buf.index].mem);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, dev.buffers[buf.index].mem);
#else
		//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_SHORT, test);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, test);
#endif

		/* Render here */
		render();

#ifndef TEST_PATTERN
		if (video_buffer_requeue(&dev, &buf) != 0)
			goto captureFailed;
#endif

		/* Swap front and back buffers */
		glfwSwapBuffers(window);

		/* Poll for and process events */
		glfwPollEvents();
	}

#ifndef TEST_PATTERN
	/* Stop streaming. */
	if (video_enable(&dev, 0) != 0)
		goto failed;
#endif

#ifndef TEST_PATTERN
	video_close(&dev);
#endif
	glfwTerminate();
	return 0;

captureFailed:
#ifndef TEST_PATTERN
	video_enable(&dev, 0);
#endif
failed:
#ifndef TEST_PATTERN
	video_close(&dev);
#endif
	glfwTerminate();
	return -1;
}
