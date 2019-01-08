/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef __LINUX_MSMB_CAMERA_H
#define __LINUX_MSMB_CAMERA_H

#include <uapi/media/msmb_camera.h>

#ifdef CONFIG_COMPAT
#define MSM_CAM_V4L2_IOCTL_NOTIFY32 \
	_IOW('V', BASE_VIDIOC_PRIVATE + 30, struct v4l2_event32)

#define MSM_CAM_V4L2_IOCTL_NOTIFY_META32 \
	_IOW('V', BASE_VIDIOC_PRIVATE + 31, struct v4l2_event32)

#define MSM_CAM_V4L2_IOCTL_CMD_ACK32 \
	_IOW('V', BASE_VIDIOC_PRIVATE + 32, struct v4l2_event32)

#define MSM_CAM_V4L2_IOCTL_NOTIFY_ERROR32 \
	_IOW('V', BASE_VIDIOC_PRIVATE + 33, struct v4l2_event32)

#define MSM_CAM_V4L2_IOCTL_NOTIFY_DEBUG32 \
	_IOW('V', BASE_VIDIOC_PRIVATE + 34, struct v4l2_event32)

#endif

#endif
