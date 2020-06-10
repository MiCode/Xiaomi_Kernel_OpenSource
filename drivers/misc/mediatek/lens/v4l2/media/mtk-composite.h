/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_COMPOSITE_H
#define _MTK_COMPOSITE_H

#include <linux/list.h>

//#include <linux/leds.h>
//#include <linux/led-class-flash.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/of.h>

#define MISC_MAX_SUBDEVS		8

struct mtk_composite_async_subdev {
	struct v4l2_subdev *sd;
	struct v4l2_async_subdev asd;
};


struct mtk_composite_v4l2_device {
	struct v4l2_device v4l2_dev;
	struct v4l2_async_notifier notifier;
	struct media_device media_dev;
	struct video_device *vdev;
	struct device *dev;
	u32 revision;

	struct v4l2_subdev **sd;
	struct v4l2_async_subdev *asd[MISC_MAX_SUBDEVS];
	struct v4l2_subdev *subdevs[MISC_MAX_SUBDEVS];
};

#endif /* _MTK_COMPOSITE_H */

