#version 430

out vec4 FragColor;

in VERTEX {
	smooth vec4 clr;
} fragment;

void main()
{
	FragColor = fragment.clr;
}
