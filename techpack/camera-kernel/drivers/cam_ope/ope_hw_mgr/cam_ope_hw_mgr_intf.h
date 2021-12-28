/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_OPE_HW_MGR_INTF_H
#define CAM_OPE_HW_MGR_INTF_H

#include <linux/types.h>
#include <linux/completion.h>
#include <media/cam_ope.h>
#include "cam_ope_hw_mgr.h"

int cam_ope_hw_mgr_init(struct device_node *of_node, uint64_t *hw_mgr_hdl,
	int *iommu_hdl);

int cam_ope_hw_init(struct cam_ope_hw_intf_data **ope_hw_intf_data,
	uint32_t hw_idx);

#endif /* CAM_OPE_HW_MGR_INTF_H */
