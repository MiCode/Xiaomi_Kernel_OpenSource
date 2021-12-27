/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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
 * @isp_mutex:             ISP dev mutex
 * @open_cnt:              Open device count
 */
struct cam_isp_dev {
	struct cam_subdev          sd;
	struct cam_context         ctx[CAM_CTX_MAX];
	struct cam_isp_context     ctx_isp[CAM_CTX_MAX];
	struct mutex               isp_mutex;
	int32_t                    open_cnt;
};

#endif /* __CAM_ISP_DEV_H__ */
