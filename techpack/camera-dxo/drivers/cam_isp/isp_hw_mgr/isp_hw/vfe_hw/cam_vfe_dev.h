/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_VFE_DEV_H_
#define _CAM_VFE_DEV_H_

#include <linux/platform_device.h>

/*
 * cam_vfe_probe()
 *
 * @brief:                   Driver probe function called on Boot
 *
 * @pdev:                    Platform Device pointer
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_vfe_probe(struct platform_device *pdev);

/*
 * cam_vfe_remove()
 *
 * @brief:                   Driver remove function
 *
 * @pdev:                    Platform Device pointer
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_vfe_remove(struct platform_device *pdev);

#endif /* _CAM_VFE_DEV_H_ */
