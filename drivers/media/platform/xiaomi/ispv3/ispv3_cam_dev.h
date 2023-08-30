/*
 * Copyright (c) 2020, Xiaomi, Inc. All rights reserved.
 */
#ifndef _ISPV3_CAM_DEV_H_
#define _ISPV3_CAM_DEV_H_

#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/ispv3_defs.h>
#include <linux/mfd/ispv3_dev.h>

#define DRV_NAME			"ispv3-v4l2"

struct ispv3_v4l2_dev {
	struct v4l2_device *v4l2_dev;
	struct video_device *video;
	struct ispv3_data *pdata;
	wait_queue_head_t wait;
	struct mutex isp_lock;
	struct device *dev;
	atomic_t int_sof;
	atomic_t int_eof;
	int open_cnt;
};

#endif
