#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

static unsigned int w = 0, h = 0;
static struct {
	uint16_t dummy;
	uint8_t signature[2];
	uint32_t file_size;
	uint16_t reserved[2];
	uint32_t offset;
	struct {
		uint32_t header_size;
		uint32_t w, h;
		uint16_t planes, bpp;
		uint32_t compression;
		uint32_t image_size;
		uint32_t xppm, yppm;
		uint32_t color_table;
		uint32_t imp_color_count;
	} dib;
} header = {
	0,
	{0x42, 0x4d,},
	0x00000000,
	{0x0000, 0x0000,},
	0x00000036,
	{
		0x00000028,
		0x00000000, 0x00000000,
		0x0001, 0x0018,
		0x00000000,
		0x00000000,
		0x00000000, 0x00000000,
		0x00000000,
		0x00000000,
	}
};

void renderRAW(unsigned char *in, unsigned char *out)
{
	int x, y;
	for (y = h - 1; y >= 0; y--)
		for (x = 0; x < w; x++) {
			size_t offset = (y * w + x) * 2;
			uint16_t pix = (uint16_t)*(in + offset) | ((uint16_t)*(in + offset + 1)) << 8;
			uint8_t r = 0, g = 0, b = 0;
			switch ((y % 2) * 2 + x % 2) {
			case 0:	// B
				b = pix >> 2;
				break;
			case 1:	// G
			case 2:	// G
				g = pix >> 2;
				break;
			case 3:	// R
				r = pix >> 2;
			}
			*out++ = b;
			*out++ = g;
			*out++ = r;
		}
}

// Manage edge wrap
static uint16_t pixel(uint16_t *pix, int x, int y)
{
	if (x < 0)
		x = -x;
	else if (x >= w)
		x = w - (x - w + 2);
	if (y < 0)
		y = -y;
	else if (y >= h)
		y = h - (y - h + 2);
	return *(pix + y * w + x);
}

void renderDemosaic(unsigned char *in, unsigned char *out)
{
	int x, y;
	for (y = h - 1; y >= 0; y--)
		for (x = 0; x < w; x++) {
			uint16_t *pix = (uint16_t *)in;// + (y * w + x) * 2);
#if 0
			size_t offset[4] = {
				of, of + 2,
				of + w * 2, of + w * 2 + 2,
			};
			uint16_t pix[4];
			int i;
			for (i = 0; i < 4; i++)
				pix[i] = (uint16_t)*(in + offset[i]) | ((uint16_t)*(in + offset[i] + 1)) << 8;
#endif
			uint16_t h, v, c, p;
			h = pixel(pix, x - 1, y) + pixel(pix, x + 1, y);
			v = pixel(pix, x, y - 1) + pixel(pix, x, y + 1);
			c = (pixel(pix, x - 1, y - 1) + pixel(pix, x + 1, y - 1) +
			     pixel(pix, x - 1, y + 1) + pixel(pix, x + 1, y + 1)) / 4;
			p = *(pix + y * w + x);

			// B,G/G,R
			uint8_t r = 0, g = 0, b = 0;
			switch ((y % 2) * 2 + x % 2) {
			case 0:	// BGGR
				r = c >> 2;
				g = ((h + v) / 4) >> 2;
				b = p >> 2;
				break;
			case 1:	// GBRG
				r = (v / 2) >> 2;
				g = p >> 2;
				b = (h / 2) >> 2;
				break;
			case 2:	// GRBG
				r = (h / 2) >> 2;
				g = p >> 2;
				b = (v / 2) >> 2;
				break;
			case 3:	// RGGB
				r = p >> 2;
				g = ((h + v) / 4) >> 2;
				b = c >> 2;
			}
#if 0
			uint16_t red = (uint16_t)*(in + offsetR) | ((uint16_t)*(in + offsetR + 1)) << 8;
			uint16_t green = (uint16_t)*(in + offsetG) | ((uint16_t)*(in + offsetG + 1)) << 8;
			uint16_t green2 = (uint16_t)*(in + offsetG2) | ((uint16_t)*(in + offsetG2 + 1)) << 8;
			uint16_t blue = (uint16_t)*(in + offsetB) | ((uint16_t)*(in + offsetB + 1)) << 8;

			if (y == h - 1 || x == w - 1)
				red = green = blue = 0;

			*out++ = blue >> 2;
			*out++ = ((green + green2) / 2)>> 2;
			*out++ = red >> 2;
#endif
			*out++ = b;
			*out++ = g;
			*out++ = r;
		}
}

int main(int argc, char *argv[])
{
	int raw = 0;
	int infd, outfd;
	unsigned char *in, *out;

	if (argc < 5)
		return 1;

	if (argc >= 6)
		raw = strcmp(argv[5], "raw") == 0;

	w = atoi(argv[1]);
	h = atoi(argv[2]);

	infd = open(argv[3], O_RDONLY);
	if (!infd)
		return 2;

	struct stat st;
	fstat(infd, &st);
	if (st.st_size != h * w * 2) {
		close(infd);
		return 3;
	}

	in = mmap(NULL, h * w * 2, PROT_READ, MAP_SHARED, infd, 0);
	if (!in) {
		close(infd);
		return 4;
	}

	header.file_size = sizeof(header) - 2 + w * h * 3;
	header.dib.w = w;
	header.dib.h = h;
	header.dib.image_size = w * h * 3;

	outfd = open(argv[4], O_RDWR | O_CREAT | O_TRUNC);
	if (!outfd) {
		close(infd);
		return 5;
	}

	if (ftruncate(outfd, header.file_size)) {
		close(infd);
		close(outfd);
		return 6;
	}

	out = mmap(NULL, header.file_size, PROT_WRITE, MAP_SHARED, outfd, 0);
	if (!out) {
		close(infd);
		close(outfd);
		return 7;
	}

	memcpy(out, (char *)&header + 2, sizeof(header) - 2);

	if (raw)
		renderRAW(in, out + sizeof(header) - 2);
	else
		renderDemosaic(in, out + sizeof(header) - 2);

	munmap(in, h * w * 2);
	munmap(out, header.file_size);
	close(infd);
	close(outfd);
	return 0;
}
