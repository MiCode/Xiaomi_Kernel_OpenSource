/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CSID_PPI_DEV_H_
#define _CAM_CSID_PPI_DEV_H_

#include "cam_isp_hw.h"

irqreturn_t cam_csid_ppi_irq(int irq_num, void *data);
int cam_csid_ppi_probe(struct platform_device *pdev);
int cam_csid_ppi_remove(struct platform_device *pdev);

#endif /*_CAM_CSID_PPI_DEV_H_ */
