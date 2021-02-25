/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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
 * @isp_device_type        ISP device type
 * @max_context            maximum contexts for TFE is 4 and for IFE is 8
 */
struct cam_isp_dev {
	struct cam_subdev          sd;
	struct cam_context         *ctx;
	struct cam_isp_context     *ctx_isp;
	struct mutex               isp_mutex;
	int32_t                    open_cnt;
	uint32_t                   isp_device_type;
	int32_t                    max_context;
};

/**
 * @brief : API to register ISP Dev to platform framework.
 * @return struct platform_device pointer on on success, or ERR_PTR() on error.
 */
int cam_isp_dev_init_module(void);

/**
 * @brief : API to remove ISP Dev from platform framework.
 */
void cam_isp_dev_exit_module(void);
#endif /* __CAM_ISP_DEV_H__ */
