/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/list.h>
#include <media/v4l2-subdev.h>

#define MAX_ACTIVE_CPP_INSTANCE 8
#define MAX_CPP_PROCESSING_FRAME 2
#define MAX_CPP_V4l2_EVENTS 30

#define MSM_CPP_MICRO_BASE          0x4000
#define MSM_CPP_MICRO_HW_VERSION    0x0000
#define MSM_CPP_MICRO_IRQGEN_STAT   0x0004
#define MSM_CPP_MICRO_IRQGEN_CLR    0x0008
#define MSM_CPP_MICRO_IRQGEN_MASK   0x000C
#define MSM_CPP_MICRO_FIFO_TX_DATA  0x0010
#define MSM_CPP_MICRO_FIFO_TX_STAT  0x0014
#define MSM_CPP_MICRO_FIFO_RX_DATA  0x0018
#define MSM_CPP_MICRO_FIFO_RX_STAT  0x001C
#define MSM_CPP_MICRO_BOOT_START    0x0020
#define MSM_CPP_MICRO_BOOT_LDORG    0x0024
#define MSM_CPP_MICRO_CLKEN_CTL     0x0030

struct cpp_subscribe_info {
	struct v4l2_fh *vfh;
	uint32_t active;
};

struct cpp_device {
	struct platform_device *pdev;
	struct v4l2_subdev subdev;
	struct resource *mem;
	struct resource *irq;
	struct resource *io;
	void __iomem *base;
	struct clk *cpp_clk[2];
	struct mutex mutex;

	struct cpp_subscribe_info cpp_subscribe_list[MAX_ACTIVE_CPP_INSTANCE];
	uint32_t cpp_open_cnt;

	struct msm_device_queue eventData_q; /*V4L2 Event Payload Queue*/

	/*Offline Frame Queue
	  process when realtime queue is empty*/
	struct msm_device_queue offline_q;
	/*Realtime Frame Queue
	  process with highest priority*/
	struct msm_device_queue realtime_q;
	/*Processing Queue
	  store frame info for frames sent to microcontroller*/
	struct msm_device_queue processing_q;
};

