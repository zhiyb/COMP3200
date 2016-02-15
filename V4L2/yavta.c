/*
 * yavta --  Yet Another V4L2 Test Application
 *
 * Copyright (C) 2005-2010 Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/time.h>

#include <linux/videodev2.h>
#include "yavta.h"

#ifndef V4L2_BUF_FLAG_ERROR
#define V4L2_BUF_FLAG_ERROR	0x0040
#endif

#define ARRAY_SIZE(a)	(sizeof(a)/sizeof((a)[0]))

static int video_set_format(struct device *dev, unsigned int w, unsigned int h, unsigned int format)
{
	struct v4l2_format fmt;
	int ret;

	memset(&fmt, 0, sizeof fmt);
	fmt.type = dev->type;
	fmt.fmt.pix.width = w;
	fmt.fmt.pix.height = h;
	fmt.fmt.pix.pixelformat = format;
	fmt.fmt.pix.field = V4L2_FIELD_ANY;

	ret = ioctl(dev->fd, VIDIOC_S_FMT, &fmt);
	if (ret < 0) {
		printf("Unable to set format: %s (%d).\n", strerror(errno),
			errno);
		return ret;
	}

	printf("Video format set: width: %u height: %u buffer size: %u\n",
		fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.sizeimage);
	return 0;
}

static int video_open(struct device *dev, const char *devname, int no_query)
{
	struct v4l2_capability cap;
	int ret;

	memset(dev, 0, sizeof *dev);
	dev->fd = -1;
	dev->memtype = V4L2_MEMORY_MMAP;
	dev->buffers = NULL;

	dev->fd = open(devname, O_RDWR);
	if (dev->fd < 0) {
		printf("Error opening device %s: %d.\n", devname, errno);
		return dev->fd;
	}

	if (!no_query) {
		memset(&cap, 0, sizeof cap);
		ret = ioctl(dev->fd, VIDIOC_QUERYCAP, &cap);
		if (ret < 0) {
			printf("Error opening device %s: unable to query "
				"device.\n", devname);
			close(dev->fd);
			return ret;
		}

		if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
			printf("V4L2_BUF_TYPE_VIDEO_CAPTURE\n");
			dev->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		} else if (cap.capabilities & V4L2_CAP_VIDEO_OUTPUT) {
			printf("V4L2_BUF_TYPE_VIDEO_OUTPUT\n");
			dev->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		} else {
			printf("Error opening device %s: neither video capture "
				"nor video output supported.\n", devname);
			close(dev->fd);
			return -EINVAL;
		}

		printf("Device %s opened: %s (%s).\n", devname, cap.card, cap.bus_info);
	} else {
		dev->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		printf("Device %s opened.\n", devname);
	}

	return 0;
}

static int video_free_buffers(struct device *dev)
{
	struct v4l2_requestbuffers rb;
	unsigned int i;
	int ret;

	if (dev->nbufs == 0)
		return 0;

	if (dev->memtype == V4L2_MEMORY_MMAP) {
		for (i = 0; i < dev->nbufs; ++i) {
			ret = munmap(dev->buffers[i].mem, dev->buffers[i].size);
			if (ret < 0) {
				printf("Unable to unmap buffer %u (%d)\n", i, errno);
				return ret;
			}
		}
	}

	memset(&rb, 0, sizeof rb);
	rb.count = 0;
	rb.type = dev->type;
	rb.memory = dev->memtype;

	ret = ioctl(dev->fd, VIDIOC_REQBUFS, &rb);
	if (ret < 0) {
		printf("Unable to release buffers: %d.\n", errno);
		return ret;
	}

	printf("%u buffers released.\n", dev->nbufs);

	free(dev->buffers);
	dev->nbufs = 0;
	dev->buffers = NULL;

	return 0;
}

void video_close(struct device *dev)
{
	video_free_buffers(dev);

	free(dev->pattern);
	free(dev->buffers);
	close(dev->fd);
}

static int video_alloc_buffers(struct device *dev, int nbufs, unsigned int offset)
{
	struct v4l2_requestbuffers rb;
	struct v4l2_buffer buf;
	int page_size;
	struct buffer *buffers;
	unsigned int i;
	int ret;

	memset(&rb, 0, sizeof rb);
	rb.count = nbufs;
	rb.type = dev->type;
	rb.memory = dev->memtype;

	ret = ioctl(dev->fd, VIDIOC_REQBUFS, &rb);
	if (ret < 0) {
		printf("Unable to request buffers: %d.\n", errno);
		return ret;
	}

	printf("%u buffers requested.\n", rb.count);

	buffers = malloc(rb.count * sizeof buffers[0]);
	if (buffers == NULL)
		return -ENOMEM;

	page_size = getpagesize();

	/* Map the buffers. */
	for (i = 0; i < rb.count; ++i) {
		memset(&buf, 0, sizeof buf);
		buf.index = i;
		buf.type = dev->type;
		buf.memory = dev->memtype;
		ret = ioctl(dev->fd, VIDIOC_QUERYBUF, &buf);
		if (ret < 0) {
			printf("Unable to query buffer %u (%d).\n", i, errno);
			return ret;
		}
		printf("length: %u offset: %u\n", buf.length, buf.m.offset);

		switch (dev->memtype) {
		case V4L2_MEMORY_MMAP:
			buffers[i].mem = mmap(0, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, buf.m.offset);
			if (buffers[i].mem == MAP_FAILED) {
				printf("Unable to map buffer %u (%d)\n", i, errno);
				return ret;
			}
			buffers[i].size = buf.length;
			printf("Buffer %u mapped at address %p.\n", i, buffers[i].mem);
			break;

		case V4L2_MEMORY_USERPTR:
			ret = posix_memalign(&buffers[i].mem, page_size, buf.length + offset);
			if (ret < 0) {
				printf("Unable to allocate buffer %u (%d)\n", i, ret);
				return -ENOMEM;
			}
			buffers[i].mem += offset;
			buffers[i].size = buf.length;
			printf("Buffer %u allocated at address %p.\n", i, buffers[i].mem);
			break;

		default:
			break;
		}
	}

	dev->buffers = buffers;
	dev->nbufs = rb.count;
	return 0;
}

static int video_queue_buffer(struct device *dev, int index)
{
	struct v4l2_buffer buf;
	int ret;

	memset(&buf, 0, sizeof buf);
	buf.index = index;
	buf.type = dev->type;
	buf.memory = dev->memtype;
	buf.length = dev->buffers[index].size;
	if (dev->memtype == V4L2_MEMORY_USERPTR)
		buf.m.userptr = (unsigned long)dev->buffers[index].mem;

	if (dev->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		buf.bytesused = buf.length;
		memcpy(dev->buffers[buf.index].mem, dev->pattern, buf.bytesused);
	} else
		memset(dev->buffers[buf.index].mem, 0x55, buf.length);

	ret = ioctl(dev->fd, VIDIOC_QBUF, &buf);
	if (ret < 0)
		printf("Unable to queue buffer (%d).\n", errno);

	return ret;
}

int video_enable(struct device *dev, int enable)
{
	int type = dev->type;
	int ret;

	ret = ioctl(dev->fd, enable ? VIDIOC_STREAMON : VIDIOC_STREAMOFF, &type);
	if (ret < 0) {
		printf("Unable to %s streaming: %d.\n", enable ? "start" : "stop",
			errno);
		return ret;
	}

	return 0;
}

static int video_prepare_capture(struct device *dev, int nbufs)
{
	unsigned int i;
	int ret;

	/* Allocate and map buffers. */
	if ((ret = video_alloc_buffers(dev, nbufs, 0)) < 0)
		return ret;

	/* Queue the buffers. */
	for (i = 0; i < dev->nbufs; ++i) {
		ret = video_queue_buffer(dev, i);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int video_capture(struct device *dev, struct v4l2_buffer *buf)
{
	int ret;

	/* Dequeue a buffer. */
	memset(buf, 0, sizeof(struct v4l2_buffer));
	buf->type = dev->type;
	buf->memory = dev->memtype;
	ret = ioctl(dev->fd, VIDIOC_DQBUF, buf);
	if (ret < 0) {
		if (errno != EIO) {
			printf("Unable to dequeue buffer (%d).\n", errno);
			return errno;
		}
		buf->type = dev->type;
		buf->memory = dev->memtype;
		//if (dev->memtype == V4L2_MEMORY_USERPTR)
		//	buf.m.userptr = (unsigned long)dev->buffers[i].mem;
	}

	if (dev->type == V4L2_BUF_TYPE_VIDEO_CAPTURE &&
	    dev->imagesize != 0	&& buf->bytesused != dev->imagesize)
		printf("Warning: bytes used %u != image size %u\n",
		       buf->bytesused, dev->imagesize);

#if 1
	printf("(%u) [%c] %u %u bytes %ld.%06ld\n", buf->index,
		(buf->flags & V4L2_BUF_FLAG_ERROR) ? 'E' : '-',
		buf->sequence, buf->bytesused, buf->timestamp.tv_sec,
		buf->timestamp.tv_usec);
#endif

#if 0
	/* Save the image. */
	if (dev->type == V4L2_BUF_TYPE_VIDEO_CAPTURE && filename_prefix && !skip) {
		sprintf(filename, "%s-%06u.bin", filename_prefix, i);
		file = fopen(filename, "wb");
		//file = fopen(filename_prefix, "wb");
		if (file != NULL) {
			ret = fwrite(dev->buffers[buf.index].mem, buf.bytesused, 1, file);
			fclose(file);
		}
	}
#endif
	return 0;
}

int video_buffer_requeue(struct device *dev, struct v4l2_buffer *buf)
{
	int ret = video_queue_buffer(dev, buf->index);
	if (ret < 0) {
		printf("Unable to requeue buffer (%d).\n", errno);
		return errno;
	}

	return 0;
}

#define V4L_BUFFERS_DEFAULT	8
#define V4L_BUFFERS_MAX	32

#if 1
int video_init(struct device *dev, const char *devfile, unsigned int pixelformat, unsigned int width, unsigned int height)
#else
int main()
#endif
{
	int ret;

	/* Video buffers */
	enum v4l2_memory memtype = V4L2_MEMORY_MMAP;
	unsigned int nbufs = V4L_BUFFERS_DEFAULT;

	/* Capture loop */
	if (nbufs > V4L_BUFFERS_MAX)
		nbufs = V4L_BUFFERS_MAX;

	/* Open the video device. */
	ret = video_open(dev, devfile, 0/*no_query*/);
	if (ret < 0)
		return ret;

	dev->memtype = memtype;

	if ((ret = video_set_format(dev, width, height, pixelformat)) < 0) {
		video_close(dev);
		return ret;
	}

	if ((ret = video_prepare_capture(dev, nbufs)) != 0) {
		video_close(dev);
		return ret;
	}

	return 0;
}

#if 0
int main()
{
	int ret;
	struct device dev;
	struct v4l2_buffer buf;

	if ((ret = video_init(&dev, "/dev/video0", V4L2_PIX_FMT_SBGGR10, 1920, 1080)) != 0)
		return ret;

	/* Start streaming. */
	if ((ret = video_enable(&dev, 1)) != 0)
		return ret;

	if ((ret = video_capture(&dev, &buf)) < 0) {
		video_enable(&dev, 0);
		video_close(&dev);
		return ret;
	}

	if ((ret = video_buffer_requeue(&dev, &buf)) != 0) {
		video_enable(&dev, 0);
		video_close(&dev);
		return ret;
	}

	/* Stop streaming. */
	if ((ret = video_enable(&dev, 0)) != 0) {
		video_close(&dev);
		return ret;
	}

	video_close(&dev);
	return 0;
}
#endif
