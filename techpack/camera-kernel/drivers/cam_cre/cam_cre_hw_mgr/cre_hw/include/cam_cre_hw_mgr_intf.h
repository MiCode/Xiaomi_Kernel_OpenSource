/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_CRE_HW_MGR_INTF_H
#define CAM_CRE_HW_MGR_INTF_H

#include <linux/of.h>
#include <media/cam_cre.h>
#include <media/cam_defs.h>

#define CAM_CRE_CTX_MAX        16

int cam_cre_hw_mgr_init(struct device_node *of_node,
	void *hw_mgr, int *iommu_hdl);

#endif /* CAM_CRE_HW_MGR_INTF_H */
