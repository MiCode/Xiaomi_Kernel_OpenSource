/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_IFE_CSID_DEV_H_
#define _CAM_IFE_CSID_DEV_H_

#include "cam_isp_hw.h"

irqreturn_t cam_ife_csid_irq(int irq_num, void *data);

int cam_ife_csid_probe(struct platform_device *pdev);
int cam_ife_csid_remove(struct platform_device *pdev);

/**
 * @brief : API to register CSID hw to platform framework.
 * @return struct platform_device pointer on on success, or ERR_PTR() on error.
 */
int cam_ife_csid17x_init_module(void);

/**
 * @brief : API to register CSID Lite hw to platform framework.
 * @return struct platform_device pointer on on success, or ERR_PTR() on error.
 */
int cam_ife_csid_lite_init_module(void);

/**
 * @brief : API to remove CSID Hw from platform framework.
 */
void cam_ife_csid17x_exit_module(void);

/**
 * @brief : API to remove CSID Lite Hw from platform framework.
 */
void cam_ife_csid_lite_exit_module(void);

#endif /*_CAM_IFE_CSID_DEV_H_ */
