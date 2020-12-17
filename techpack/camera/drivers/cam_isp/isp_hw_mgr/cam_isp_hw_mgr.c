// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#include "cam_isp_hw_mgr_intf.h"
#include "cam_ife_hw_mgr.h"
#include "cam_debug_util.h"
#include "cam_tfe_hw_mgr.h"


int cam_isp_hw_mgr_init(const char   *device_name_str,
	struct cam_hw_mgr_intf *hw_mgr, int *iommu_hdl)
{
	int rc = 0;

	if (strnstr(device_name_str, "ife", strlen(device_name_str)))
		rc = cam_ife_hw_mgr_init(hw_mgr, iommu_hdl);
	else if (strnstr(device_name_str, "tfe", strlen(device_name_str)))
		rc = cam_tfe_hw_mgr_init(hw_mgr, iommu_hdl);
	else {
		CAM_ERR(CAM_ISP, "Invalid ISP hw type :%s", device_name_str);
		rc = -EINVAL;
	}

	return rc;
}
