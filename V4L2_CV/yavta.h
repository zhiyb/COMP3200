#ifndef YAVTA_H
#define YAVTA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
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

int video_init(struct device *dev, const char *devfile, unsigned int pixelformat,
		unsigned int width, unsigned int height, unsigned int nbufs);
int video_enable(struct device *dev, int enable);
int video_capture(struct device *dev, struct v4l2_buffer *buf);
void video_close(struct device *dev);

int video_alloc_buffers(struct device *dev, int nbufs, unsigned int offset);
int video_queue_buffer(struct device *dev, int index);
int video_buffer_requeue(struct device *dev, struct v4l2_buffer *buf);

int video_set_format(struct device *dev, unsigned int w, unsigned int h, unsigned int format);
int video_set_control(struct device *dev, uint32_t id, uint32_t *value);
int video_get_control(struct device *dev, uint32_t id, uint32_t *value);

#ifdef __cplusplus
}
#endif

#endif
