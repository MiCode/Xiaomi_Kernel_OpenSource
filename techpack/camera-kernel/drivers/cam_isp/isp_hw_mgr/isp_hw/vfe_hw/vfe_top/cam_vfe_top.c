// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include "cam_vfe_top.h"
#include "cam_vfe_top_ver2.h"
#include "cam_vfe_top_ver3.h"
#include "cam_vfe_top_ver4.h"
#include "cam_debug_util.h"

int cam_vfe_top_init(uint32_t          top_version,
	struct cam_hw_soc_info        *soc_info,
	struct cam_hw_intf            *hw_intf,
	void                          *top_hw_info,
	void                          *vfe_irq_controller,
	struct cam_vfe_top           **vfe_top)
{
	int rc = -EINVAL;

	switch (top_version) {
	case CAM_VFE_TOP_VER_2_0:
		rc = cam_vfe_top_ver2_init(soc_info, hw_intf, top_hw_info,
			vfe_irq_controller, vfe_top);
		break;
	case CAM_VFE_TOP_VER_3_0:
		rc = cam_vfe_top_ver3_init(soc_info, hw_intf, top_hw_info,
			vfe_irq_controller, vfe_top);
		break;
	case CAM_VFE_TOP_VER_4_0:
		rc = cam_vfe_top_ver4_init(soc_info, hw_intf, top_hw_info,
			vfe_irq_controller, vfe_top);
		break;
	default:
		CAM_ERR(CAM_ISP, "Error! Unsupported Version %x", top_version);
		break;
	}

	return rc;
}

int cam_vfe_top_deinit(uint32_t        top_version,
	struct cam_vfe_top           **vfe_top)
{
	int rc = -EINVAL;

	switch (top_version) {
	case CAM_VFE_TOP_VER_2_0:
		rc = cam_vfe_top_ver2_deinit(vfe_top);
		break;
	case CAM_VFE_TOP_VER_3_0:
		rc = cam_vfe_top_ver3_deinit(vfe_top);
		break;
	case CAM_VFE_TOP_VER_4_0:
		rc = cam_vfe_top_ver4_deinit(vfe_top);
		break;
	default:
		CAM_ERR(CAM_ISP, "Error! Unsupported Version %x", top_version);
		break;
	}

	return rc;
}
