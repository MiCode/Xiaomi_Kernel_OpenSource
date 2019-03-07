/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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

#ifndef _CAM_ISP_DEV_H_
#define _CAM_ISP_DEV_H_

#include "cam_subdev.h"
#include "cam_hw_mgr_intf.h"
#include "cam_context.h"
#include "cam_isp_context.h"

/**
 * struct cam_isp_dev - Camera ISP V4l2 device node
 *
 * @sd:                    Commone camera subdevice node
 * @ctx:                   Isp base context storage
 * @ctx_isp:               Isp private context storage
 *
 */
struct cam_isp_dev {
	struct cam_subdev          sd;
	struct cam_context         ctx[CAM_CTX_MAX];
	struct cam_isp_context     ctx_isp[CAM_CTX_MAX];
};

#endif /* __CAM_ISP_DEV_H__ */
