#version 400

uniform usampler2D sampler;
out vec4 FragColor;

in VERTEX {
	smooth vec2 texCoord;
} vertex;

// 10-bit resolution
#define MAX_RES	1023.0

float texelCorner()
{
	uint texel = 0;
	texel += textureOffset(sampler, vertex.texCoord, ivec2(-1, -1));
	texel += textureOffset(sampler, vertex.texCoord, ivec2(-1, 1));
	texel += textureOffset(sampler, vertex.texCoord, ivec2(1, -1));
	texel += textureOffset(sampler, vertex.texCoord, ivec2(1, 1));
	return float(texel / 4) / MAX_RES;
}

float texelNear()
{
	uint texel = 0;
	texel += textureOffset(sampler, vertex.texCoord, ivec2(-1, 0));
	texel += textureOffset(sampler, vertex.texCoord, ivec2(1, 0));
	texel += textureOffset(sampler, vertex.texCoord, ivec2(0, -1));
	texel += textureOffset(sampler, vertex.texCoord, ivec2(0, 1));
	return float(texel / 4) / MAX_RES;
}

float texelHorizontal()
{
	uint texel = 0;
	texel += textureOffset(sampler, vertex.texCoord, ivec2(-1, 0));
	texel += textureOffset(sampler, vertex.texCoord, ivec2(1, 0));
	return float(texel / 2) / MAX_RES;
}

float texelVertical()
{
	uint texel = 0;
	texel += textureOffset(sampler, vertex.texCoord, ivec2(0, -1));
	texel += textureOffset(sampler, vertex.texCoord, ivec2(0, 1));
	return float(texel / 2) / MAX_RES;
}

float texelCenter()
{
	uint texel = texture(sampler, vertex.texCoord, 0).r;
	return float(texel) / MAX_RES;
}

void main()
{
	float r = 0, g = 0, b = 0;

	uvec2 pos = uvec2(vertex.texCoord * textureSize(sampler, 0));
	// B,G/G,R
	if (pos.y % 2 == 0) {
		if (pos.x % 2 == 0) {
			// RGR/GBG/RGR
			r = texelCorner();
			g = texelNear();
			b = texelCenter();
		} else {
			// GRG/BGB/GRG
			r = texelVertical();
			g = texelCenter();
			b = texelHorizontal();
		}
	} else {
		if (pos.x % 2 == 0) {
			// GBG/RGR/GBG
			r = texelHorizontal();
			g = texelCenter();
			b = texelVertical();
		} else {
			// BGB/GRG/BGB
			r = texelCenter();
			g = texelNear();
			b = texelCorner();
		}
	}

	FragColor = vec4(r, g, b, 1.0);
}
