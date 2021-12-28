/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_ICP_SUBDEV_H_
#define _CAM_ICP_SUBDEV_H_

/**
 * @brief : API to register ICP subdev to platform framework.
 * @return struct platform_device pointer on on success, or ERR_PTR() on error.
 */
int cam_icp_init_module(void);

/**
 * @brief : API to remove ICP subdev from platform framework.
 */
void cam_icp_exit_module(void);

#endif /* _CAM_ICP_SUBDEV_H_ */
