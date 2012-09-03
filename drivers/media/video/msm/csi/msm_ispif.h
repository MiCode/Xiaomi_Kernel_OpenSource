/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#ifndef MSM_ISPIF_H
#define MSM_ISPIF_H

#include <linux/clk.h>
#include <linux/io.h>
#include <media/v4l2-subdev.h>

struct ispif_irq_status {
	uint32_t ispifIrqStatus0;
	uint32_t ispifIrqStatus1;
	uint32_t ispifIrqStatus2;
};

enum msm_ispif_state_t {
	ISPIF_POWER_UP,
	ISPIF_POWER_DOWN,
};

struct ispif_device {
	struct platform_device *pdev;
	struct v4l2_subdev subdev;
	struct resource *mem;
	struct resource *irq;
	struct resource *io;
	void __iomem *base;
	struct mutex mutex;
	uint8_t start_ack_pending;
	struct completion reset_complete;
	uint32_t csid_version;
	struct clk *ispif_clk[5];
	uint32_t pix_sof_count;
	uint32_t rdi0_sof_count;
	uint32_t rdi1_sof_count;
	uint32_t rdi2_sof_count;
	uint32_t global_intf_cmd_mask;
	struct tasklet_struct ispif_tasklet;
	enum msm_ispif_state_t ispif_state;
};

struct ispif_isr_queue_cmd {
	struct list_head list;
	uint32_t    ispifInterruptStatus0;
	uint32_t    ispifInterruptStatus1;
	uint32_t    ispifInterruptStatus2;
};

#define VIDIOC_MSM_ISPIF_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 18, struct ispif_cfg_data*)

#endif
