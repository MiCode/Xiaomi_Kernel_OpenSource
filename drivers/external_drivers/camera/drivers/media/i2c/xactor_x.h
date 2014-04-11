/*
 * Support for mipi CSI_data generator.
 *
 * Copyright (c) 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __CSI_XACTOR_H__
#define __CSI_XACTOR_H__
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#define	CSI_XACTOR_A_NAME	"xactor_a"
#define	CSI_XACTOR_B_NAME	"xactor_b"
#define	CSI_XACTOR_C_NAME	"xactor_c"
#define	CSI_XACTOR_NAME		"xactor_x"

enum xactor_contexts {
	CONTEXT_SNAPSHOT = 0,
	CONTEXT_PREVIEW,
	CONTEXT_VIDEO,
	CONTEXT_NUM
};

struct xactor_context_config {
	unsigned int width;
	unsigned int height;
	unsigned int lines_per_frame;
	unsigned int mbus_fmt;
};

/* csi_xactor device structure */
struct csi_xactor_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;

	struct camera_sensor_platform_data *platform_data;
	struct xactor_context_config cntx_config[3];
	struct mutex input_lock; /* serialize sensor's ioctl */
	enum xactor_contexts cur_context;

	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *run_mode;
};

#endif
