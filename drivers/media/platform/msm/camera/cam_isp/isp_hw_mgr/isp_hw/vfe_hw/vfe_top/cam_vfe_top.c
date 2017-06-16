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

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include "cam_vfe_top.h"
#include "cam_vfe_top_ver2.h"

int cam_vfe_top_init(uint32_t          top_version,
	struct cam_hw_soc_info        *soc_info,
	struct cam_hw_intf            *hw_intf,
	void                          *top_hw_info,
	struct cam_vfe_top            **vfe_top)
{
	int rc = -EINVAL;

	switch (top_version) {
	case CAM_VFE_TOP_VER_2_0:
		rc = cam_vfe_top_ver2_init(soc_info, hw_intf, top_hw_info,
			vfe_top);
		break;
	default:
		pr_err("Error! Unsupported Version %x\n", top_version);
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
	default:
		pr_err("Error! Unsupported Version %x\n", top_version);
		break;
	}

	return rc;
}

