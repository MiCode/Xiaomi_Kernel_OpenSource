/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef MSM_V4L2_VIDEO_H
#define MSM_V4L2_VIDEO_H

#include <linux/mm.h>
#include <linux/msm_mdp.h>
#include <linux/videodev2.h>


struct msm_v4l2_overlay_buffer {
	int mapped;
	int queued;
	int offset;
	int bufsize;
};

struct msm_v4l2_overlay_device {
	struct device dev;

	int ref_count;
	int id;

	int screen_width;
	int screen_height;
	int streaming;

	struct v4l2_pix_format pix;
	struct v4l2_window win;
	struct v4l2_rect crop_rect;
	struct v4l2_framebuffer fb;
	struct msm_v4l2_overlay_buffer *bufs;
	int numbufs;
	struct mdp_overlay req;
	void *par;

	struct mutex update_lock;
};

struct msm_v4l2_overlay_fh {
	struct msm_v4l2_overlay_device *vout;
	enum v4l2_buf_type type;
};

struct msm_v4l2_overlay_userptr_buffer {
	uint base[3];
	size_t length[3];
};

#endif
