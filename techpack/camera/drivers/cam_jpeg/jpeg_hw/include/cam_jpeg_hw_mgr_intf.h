/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_JPEG_HW_MGR_INTF_H
#define CAM_JPEG_HW_MGR_INTF_H

#include <linux/of.h>
#include <media/cam_jpeg.h>
#include <media/cam_defs.h>

int cam_jpeg_hw_mgr_init(struct device_node *of_node,
	uint64_t *hw_mgr_hdl, int *iommu_hdl);

#endif /* CAM_JPEG_HW_MGR_INTF_H */
