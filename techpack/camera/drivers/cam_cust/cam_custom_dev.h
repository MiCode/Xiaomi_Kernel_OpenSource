/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CUSTOM_DEV_H_
#define _CAM_CUSTOM_DEV_H_

#include "cam_subdev.h"
#include "cam_hw_mgr_intf.h"
#include "cam_custom_hw_mgr.h"
#include "cam_context.h"
#include "cam_custom_context.h"

#define CAM_CUSTOM_HW_MAX_INSTANCES 3

/**
 * struct cam_custom_dev - Camera Custom V4l2 device node
 *
 * @sd:                    Common camera subdevice node
 * @ctx:                   Custom base context storage
 * @ctx_custom:            Custom private context storage
 * @custom_dev_mutex:      Custom dev mutex
 * @open_cnt:              Open device count
 */
struct cam_custom_dev {
	struct cam_subdev          sd;
	struct cam_context         ctx[CAM_CUSTOM_HW_MAX_INSTANCES];
	struct cam_custom_context  ctx_custom[CAM_CUSTOM_HW_MAX_INSTANCES];
	struct mutex               custom_dev_mutex;
	int32_t                    open_cnt;
};

#endif /* _CAM_CUSTOM_DEV_H_ */
