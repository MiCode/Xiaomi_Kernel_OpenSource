/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CUSTOM_CSID_DEV_H_
#define _CAM_CUSTOM_CSID_DEV_H_

#include "cam_debug_util.h"
#include "cam_custom_hw_mgr_intf.h"

/**
 * @brief : API to register Custom CSID hw to platform framework.
 * @return struct platform_device pointer on on success, or ERR_PTR() on error.
 */
int cam_custom_csid_driver_init(void);

/**
 * @brief : API to remove Custom CSID hw interface from platform framework.
 */
void cam_custom_csid_driver_exit(void);

#endif /*_CAM_CUSTOM_CSID_DEV_H_ */
