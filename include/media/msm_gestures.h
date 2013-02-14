/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __LINUX_MSM_GESTURES_H
#define __LINUX_MSM_GESTURES_H

#include <linux/types.h>
#include <linux/ioctl.h>
#include <media/msm_camera.h>

#define MSM_GES_IOCTL_CTRL_COMMAND \
	_IOW('V', BASE_VIDIOC_PRIVATE + 20, struct v4l2_control)

#define VIDIOC_MSM_GESTURE_EVT \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 21, struct v4l2_event)

#define MSM_GES_GET_EVT_PAYLOAD \
	_IOW('V', BASE_VIDIOC_PRIVATE + 22, struct msm_ges_evt)

#define VIDIOC_MSM_GESTURE_CAM_EVT \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 23, int)

#define MSM_GES_RESP_V4L2  MSM_CAM_RESP_MAX
#define MSM_GES_RESP_MAX  (MSM_GES_RESP_V4L2 + 1)

#define MSM_SVR_RESP_MAX  MSM_GES_RESP_MAX


#define MSM_V4L2_GES_BASE                             100
#define MSM_V4L2_GES_OPEN         (MSM_V4L2_GES_BASE + 0)
#define MSM_V4L2_GES_CLOSE        (MSM_V4L2_GES_BASE + 1)
#define MSM_V4L2_GES_CAM_OPEN     (MSM_V4L2_GES_BASE + 2)
#define MSM_V4L2_GES_CAM_CLOSE    (MSM_V4L2_GES_BASE + 3)

#define MSM_GES_APP_EVT_MIN     (V4L2_EVENT_PRIVATE_START + 0x14)
#define MSM_GES_APP_NOTIFY_EVENT        (MSM_GES_APP_EVT_MIN + 0)
#define MSM_GES_APP_NOTIFY_ERROR_EVENT  (MSM_GES_APP_EVT_MIN + 1)
#define MSM_GES_APP_EVT_MAX             (MSM_GES_APP_EVT_MIN + 2)

#define MSM_GESTURE_CID_CTRL_CMD V4L2_CID_BRIGHTNESS

#define MAX_GES_EVENTS 25

struct msm_ges_ctrl_cmd {
	int type;
	void *value;
	int len;
	int fd;
	uint32_t cookie;
};

struct msm_ges_evt {
	void *evt_data;
	int evt_len;
};

#endif /*__LINUX_MSM_GESTURES_H*/
