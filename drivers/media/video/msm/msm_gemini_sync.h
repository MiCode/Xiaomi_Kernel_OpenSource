/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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

#ifndef MSM_GEMINI_SYNC_H
#define MSM_GEMINI_SYNC_H

#include <linux/fs.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include "msm_gemini_core.h"

struct msm_gemini_q {
	char const	*name;
	struct list_head  q;
	spinlock_t	lck;
	wait_queue_head_t wait;
	int	       unblck;
};

struct msm_gemini_q_entry {
	struct list_head list;
	void   *data;
};

struct msm_gemini_device {
	struct platform_device *pdev;
	struct resource        *mem;
	int                     irq;
	void                   *base;

	struct device *device;
	struct cdev   cdev;
	struct mutex  lock;
	char	  open_count;
	uint8_t       op_mode;

	/* event queue including frame done & err indications
	 */
	struct msm_gemini_q evt_q;

	/* output return queue
	 */
	struct msm_gemini_q output_rtn_q;

	/* output buf queue
	 */
	struct msm_gemini_q output_buf_q;

	/* input return queue
	 */
	struct msm_gemini_q input_rtn_q;

	/* input buf queue
	 */
	struct msm_gemini_q input_buf_q;
};

int __msm_gemini_open(struct msm_gemini_device *pgmn_dev);
int __msm_gemini_release(struct msm_gemini_device *pgmn_dev);

long __msm_gemini_ioctl(struct msm_gemini_device *pgmn_dev,
	unsigned int cmd, unsigned long arg);

struct msm_gemini_device *__msm_gemini_init(struct platform_device *pdev);
int __msm_gemini_exit(struct msm_gemini_device *pgmn_dev);

#endif /* MSM_GEMINI_SYNC_H */
