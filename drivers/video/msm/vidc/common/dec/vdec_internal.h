/* Copyright (c) 2010, 2012, Code Aurora Forum. All rights reserved.
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

#ifndef VDEC_INTERNAL_H
#define VDEC_INTERNAL_H

#include <linux/msm_vidc_dec.h>
#include <linux/cdev.h>
#include <media/msm/vidc_init.h>

#define NUM_OF_DRIVER_NODES 2

struct vid_dec_msg {
	struct list_head list;
	struct vdec_msginfo vdec_msg_info;
};

struct vid_dec_dev {
	struct cdev cdev[NUM_OF_DRIVER_NODES];
	struct device *device[NUM_OF_DRIVER_NODES];
	resource_size_t phys_base;
	void __iomem *virt_base;
	unsigned int irq;
	struct clk *hclk;
	struct clk *hclk_div2;
	struct clk *pclk;
	unsigned long hclk_rate;
	struct mutex lock;
	s32 device_handle;
	struct video_client_ctx vdec_clients[VIDC_MAX_NUM_CLIENTS];
	u32 num_clients;
	void(*timer_handler)(void *);
};

#endif
