/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CUSTOM_SUB_MOD_DEV_H_
#define _CAM_CUSTOM_SUB_MOD_DEV_H_

#include "cam_custom_hw_mgr_intf.h"

#define CAM_CUSTOM_SUB_MOD_NAME "cam_custom_sub_mod"

#define CAM_CUSTOM_SUB_MOD_MAX_INSTANCES                   2

/**
 * @brief : API to register Custom submodule to platform framework.
 * @return struct platform_device pointer on on success, or ERR_PTR() on error.
 */
int cam_custom_hw_sub_module_init(void);

/**
 * @brief : API to remove Custom submodule interface from platform framework.
 */
void cam_custom_hw_sub_module_exit(void);
#endif /* _CAM_CUSTOM_SUB_MOD_DEV_H_ */
