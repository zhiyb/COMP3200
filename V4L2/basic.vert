#version 400

in vec2 position;
uniform mat4 projection;

out VERTEX {
	smooth vec2 texCoord;
} vertex;

void main()
{
	gl_Position = projection * vec4(position, 0.0, 1.0);
	vertex.texCoord = (position * vec2(1.0, -1.0) + 1.0) / 2.0;
}
