/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_JPEG_DEV_H_
#define _CAM_JPEG_DEV_H_

#include "cam_subdev.h"
#include "cam_hw_mgr_intf.h"
#include "cam_context.h"
#include "cam_jpeg_context.h"

/**
 * struct cam_jpeg_dev - Camera JPEG V4l2 device node
 *
 * @sd: Commone camera subdevice node
 * @node: Pointer to jpeg subdevice
 * @ctx: JPEG base context storage
 * @ctx_jpeg: JPEG private context storage
 * @jpeg_mutex: Jpeg dev mutex
 * @open_cnt: Open device count
 */
struct cam_jpeg_dev {
	struct cam_subdev sd;
	struct cam_node *node;
	struct cam_context ctx[CAM_CTX_MAX];
	struct cam_jpeg_context ctx_jpeg[CAM_CTX_MAX];
	struct mutex jpeg_mutex;
	int32_t open_cnt;
};
#endif /* __CAM_JPEG_DEV_H__ */
