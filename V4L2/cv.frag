#version 400

uniform sampler2D sampler, drawing;
out vec4 FragColor;

in VERTEX {
	smooth vec2 tex;
} fragment;

void main()
{
	float r = 0.0, g = 0.0, b = 0.0;

	vec4 drawing = texture(drawing, fragment.tex);
	if (drawing != vec4(0.0, 0.0, 0.0, 1.0)) {
		FragColor = drawing;
		return;
	}

	vec4 img = texture(sampler, fragment.tex);
	FragColor = img;
}
