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
 */

#ifndef MSM_MERCURY_SYNC_H
#define MSM_MERCURY_SYNC_H

#include <linux/fs.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include "msm_mercury_core.h"

struct msm_mercury_q {
		char const  *name;
		struct list_head  q;
		spinlock_t  lck;
		wait_queue_head_t wait;
		int        unblck;
};

struct msm_mercury_q_entry {
		struct list_head list;
		void   *data;
};

struct msm_mercury_device {
		struct platform_device *pdev;
		struct resource        *mem;
		int                     irq;
		void                   *base;
		struct clk *mercury_clk[2];
		struct device *device;
		struct cdev   cdev;
		struct mutex  lock;
		char      open_count;
		uint8_t       op_mode;

		/* event queue including frame done & err indications*/
		struct msm_mercury_q evt_q;
		struct v4l2_subdev subdev;

};

int __msm_mercury_open(struct msm_mercury_device *pmcry_dev);
int __msm_mercury_release(struct msm_mercury_device *pmcry_dev);

long __msm_mercury_ioctl(struct msm_mercury_device *pmcry_dev,
	unsigned int cmd, unsigned long arg);

struct msm_mercury_device *__msm_mercury_init(struct platform_device *pdev);
int __msm_mercury_exit(struct msm_mercury_device *pmcry_dev);
int msm_mercury_ioctl_hw_cmds(struct msm_mercury_device *pmcry_dev,
	void * __user arg);
int msm_mercury_ioctl_hw_cmds_wo(struct msm_mercury_device *pmcry_dev,
	void * __user arg);
#endif /* MSM_MERCURY_SYNC_H */
