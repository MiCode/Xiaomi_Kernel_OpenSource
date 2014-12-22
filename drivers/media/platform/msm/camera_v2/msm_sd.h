/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_SD_H
#define _MSM_SD_H

#include <media/v4l2-subdev.h>
#include <media/msmb_camera.h>

/* NOTE: this header file should ONLY be included by subdev drivers */

struct msm_sd_close_ioctl {
	unsigned int session;
	unsigned int stream;
};

#define MSM_SD_CLOSE_STREAM \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 26, struct msm_sd_close_ioctl)

#define MSM_SD_CLOSE_SESSION \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 27, struct msm_sd_close_ioctl)

#define MSM_SD_CLOSE_SESSION_AND_STREAM \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 28, struct msm_sd_close_ioctl)

#define MSM_SD_SHUTDOWN \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 29, struct msm_sd_close_ioctl)

#define MSM_SD_NOTIFY_FREEZE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 30, struct msm_sd_close_ioctl)
/*
 * This is used to install Sequence in msm_sd_register.
 * During msm_close, proper close sequence will be triggered.
 * For example:
 *
 * close_sequence = 0x00100001 (ISP)
 * close_sequence = 0x00100002 (ISP)
 * close_sequence = 0x00100003 (ISP)
 * close_sequence = 0x00200001 (sensor)
 * close_sequence = 0x00200002 (sensor)
 * close_sequence = 0x00200003 (sensor)
 */
#define MSM_SD_CLOSE_1ST_CATEGORY  0x00010000
#define MSM_SD_CLOSE_2ND_CATEGORY  0x00020000
#define MSM_SD_CLOSE_3RD_CATEGORY  0x00030000
#define MSM_SD_CLOSE_4TH_CATEGORY  0x00040000

struct msm_sd_subdev {
	struct v4l2_subdev sd;
	int close_seq;
	struct list_head list;
};

struct msm_sd_req_sd {
	char *name;
	struct v4l2_subdev *subdev;
};

struct msm_sd_req_vb2_q {
	struct vb2_buffer *(*get_buf)(int session_id, unsigned int stream_id);
	struct vb2_queue *(*get_vb2_queue)(int session_id,
		unsigned int stream_id);
	int (*put_buf)(struct vb2_buffer *vb2_buf, int session_id,
		unsigned int stream_id);
	int (*buf_done)(struct vb2_buffer *vb2_buf, int session_id,
		unsigned int stream_id);
};

#define MSM_SD_NOTIFY_GET_SD 0x00000001
#define MSM_SD_NOTIFY_PUT_SD 0x00000002
#define MSM_SD_NOTIFY_REQ_CB 0x00000003

int msm_sd_register(struct msm_sd_subdev *msm_subdev);
int msm_sd_unregister(struct msm_sd_subdev *sd);
struct v4l2_subdev *msm_sd_get_subdev(struct v4l2_subdev *sd,
	const char *get_name);
void msm_sd_put_subdev(struct v4l2_subdev *sd, struct v4l2_subdev *put);

#endif /*_MSM_SD_H */
