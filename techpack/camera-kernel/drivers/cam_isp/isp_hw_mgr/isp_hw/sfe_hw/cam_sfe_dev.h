/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_SFE_DEV_H_
#define _CAM_SFE_DEV_H_

/**
 * @brief : API to register SFE hw to platform framework.
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_sfe_init_module(void);

/**
 * @brief : API to remove SFE  Hw from platform framework.
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
void cam_sfe_exit_module(void);

#endif /* _CAM_SFE_DEV_H_ */
