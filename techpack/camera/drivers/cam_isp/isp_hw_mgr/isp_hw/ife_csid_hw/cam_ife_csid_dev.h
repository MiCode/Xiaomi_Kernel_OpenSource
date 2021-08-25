/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_IFE_CSID_DEV_H_
#define _CAM_IFE_CSID_DEV_H_

#include "cam_isp_hw.h"

irqreturn_t cam_ife_csid_irq(int irq_num, void *data);

int cam_ife_csid_probe(struct platform_device *pdev);
int cam_ife_csid_remove(struct platform_device *pdev);

#endif /*_CAM_IFE_CSID_DEV_H_ */
