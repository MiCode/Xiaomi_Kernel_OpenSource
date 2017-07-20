/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
