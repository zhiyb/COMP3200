#ifndef YAVTA_H
#define YAVTA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/videodev2.h>

struct buffer
{
	unsigned int size;
	void *mem;
};

struct device
{
	int fd;

	enum v4l2_buf_type type;
	enum v4l2_memory memtype;
	unsigned int nbufs;
	struct buffer *buffers;

	unsigned int width;
	unsigned int height;
	unsigned int bytesperline;
	unsigned int imagesize;

	void *pattern;
};

int video_init(struct device *dev, const char *devfile, unsigned int pixelformat, unsigned int width, unsigned int height);
int video_enable(struct device *dev, int enable);
int video_capture(struct device *dev, struct v4l2_buffer *buf);
int video_buffer_requeue(struct device *dev, struct v4l2_buffer *buf);
void video_close(struct device *dev);

#ifdef __cplusplus
}
#endif

#endif
