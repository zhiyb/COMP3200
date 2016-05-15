#version 430

in vec2 data;
uniform float last;

out VERTEX {
	smooth vec4 clr;
} vertex;

void main()
{
	float fps = data.x;
	float time = data.y;
	float x = 1.0 - (last - time) / 30.0;
	float y = fps / 60.0;
	gl_Position = vec4(-1.0 + x * 2.0, -1.0 + y * 2.0, 0.0, 1.0);
	float c = y * 2.0;
	vertex.clr = vec4(0.0, c, 1.0, 1.0);
}
