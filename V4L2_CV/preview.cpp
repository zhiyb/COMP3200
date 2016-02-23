#include <map>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "helper.h"
#include "global.h"

#define FILE_VERTEX_SHADER	"basic.vert"
#define FILE_FRAGMENT_SHADER	"bayer.frag"

using namespace std;
using namespace glm;

struct thread_t pvThreadInfo;

// OpenGL
static unsigned int texture;
static map<string, GLint> location;
static float fps;

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

void pvThread()
{
	int err;

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

	/* Initialize the library */
	if (!glfwInit()) {
		err = -1;
		return;
	}

	/* Create a windowed mode window and its OpenGL context */
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	window = glfwCreateWindow(status.width, status.height, "Hello World", NULL, NULL);
	glfwSetWindowPos(window, status.width, 0);
	if (!window) {
		glfwTerminate();
		err = -1;
		return;
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
		return;
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

	// Waiting for start
	;

	past = glfwGetTime();
	count = 0;

	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window) && status.request != REQUEST_QUIT) {
		//setLED(1);
		//printf("%s: buffer %u\n", __func__, buf[bufidx].index);
		//dev.buffers[buf.index].mem, buf.bytesused;
		glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, status.width * status.height * 2, dev.buffers[buf[bufidx].index].mem);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, status.width, status.height, GL_RED_INTEGER,
				GL_UNSIGNED_SHORT, 0);
		//setLED(0);

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

		/* Poll for and process events */
		glfwPollEvents();
	}
}
