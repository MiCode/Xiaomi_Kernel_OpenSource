/* Copyright (c) 2010,2013, The Linux Foundation. All rights reserved.
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
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include "msm_gemini_core.h"

#define GEMINI_7X 0x1
#define GEMINI_8X60 (0x1 << 1)
#define GEMINI_8960 (0x1 << 2)

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
	struct clk *gemini_clk[3];
	struct regulator *gemini_fs;
	uint32_t hw_version;

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

	struct v4l2_subdev subdev;
	enum msm_gmn_out_mode out_mode;

	/* single out mode parameters */
	struct msm_gemini_hw_buf out_buf;
	int out_offset;
	int out_buf_set;
	int max_out_size;
	int out_frag_cnt;

	uint32_t bus_perf_client;
};

int __msm_gemini_open(struct msm_gemini_device *pgmn_dev);
int __msm_gemini_release(struct msm_gemini_device *pgmn_dev);

long __msm_gemini_ioctl(struct msm_gemini_device *pgmn_dev,
	unsigned int cmd, unsigned long arg);

struct msm_gemini_device *__msm_gemini_init(struct platform_device *pdev);
int __msm_gemini_exit(struct msm_gemini_device *pgmn_dev);

#endif /* MSM_GEMINI_SYNC_H */
