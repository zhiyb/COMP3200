#version 400

uniform usampler2D sampler;
out vec4 FragColor;

in VERTEX {
	smooth vec2 texCoord;
} vertex;

void main(void)
{
	uint texel = texture(sampler, vertex.texCoord).r;
	float colour = float(texel) / 1024.0;
	FragColor = vec4(colour, colour, colour, 1.0);
}
