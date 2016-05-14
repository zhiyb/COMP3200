#version 400

in vec2 position;

out VERTEX {
	smooth vec2 tex;
} vertex;

void main()
{
	gl_Position = vec4(position, 0.0, 1.0);
	vertex.tex = (position * vec2(1.0, -1.0) + 1.0) / 2.0;
}
