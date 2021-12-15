/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_TFE_DEV_H_
#define _CAM_TFE_DEV_H_

#include <linux/platform_device.h>

/*
 * cam_tfe_probe()
 *
 * @brief:                   Driver probe function called on Boot
 *
 * @pdev:                    Platform Device pointer
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_tfe_probe(struct platform_device *pdev);

/*
 * cam_tfe_remove()
 *
 * @brief:                   Driver remove function
 *
 * @pdev:                    Platform Device pointer
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_tfe_remove(struct platform_device *pdev);

int cam_tfe_init_module(void);
void cam_tfe_exit_module(void);

#endif /* _CAM_TFE_DEV_H_ */
