/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MTK_COMPOSITE_H
#define _MTK_COMPOSITE_H

#include <linux/list.h>

#include <linux/leds.h>
#include <linux/led-class-flash.h>
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

