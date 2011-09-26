/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <mach/gpio.h>
#include <mach/board.h>
#include <mach/camera.h>
#include <mach/vreg.h>
#include <mach/camera.h>
#include <mach/clk.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <media/v4l2-subdev.h>


struct ispif_irq_status {
	uint32_t ispifIrqStatus0;
	uint32_t ispifIrqStatus1;
};

struct ispif_device {
	struct v4l2_subdev subdev;
	struct resource *mem;
	struct resource *irq;
	struct resource *io;
	void __iomem *base;
	struct mutex mutex;
	uint8_t start_ack_pending;
	struct completion reset_complete;
};

#define VIDIOC_MSM_ISPSF_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 1, struct msm_ispif_params)

#define ISPIF_STREAM(intf, action) (((intf)<<ISPIF_S_STREAM_SHIFT)+(action))
#define ISPIF_ON_FRAME_BOUNDARY	(0x01 << 0)
#define ISPIF_OFF_FRAME_BOUNDARY    (0x01 << 1)
#define ISPIF_OFF_IMMEDIATELY       (0x01 << 2)
#define ISPIF_S_STREAM_SHIFT	4

int msm_ispif_init(struct v4l2_subdev **sd, struct platform_device *pdev);
void msm_ispif_release(struct v4l2_subdev *sd);
void msm_ispif_vfe_get_cid(uint8_t intftype, char *cids, int *num);

#endif
