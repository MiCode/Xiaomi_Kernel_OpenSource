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

#ifndef _CAM_VFE_TOP_H_
#define _CAM_VFE_TOP_H_

#include "cam_hw_intf.h"
#include "cam_isp_hw.h"

#define CAM_VFE_TOP_VER_1_0 0x100000
#define CAM_VFE_TOP_VER_2_0 0x200000

#define CAM_VFE_CAMIF_VER_1_0 0x10
#define CAM_VFE_CAMIF_VER_2_0 0x20

#define CAM_VFE_RDI_VER_1_0 0x1000

struct cam_vfe_top {
	void                   *top_priv;
	struct cam_hw_ops       hw_ops;
};

int cam_vfe_top_init(uint32_t          top_version,
	struct cam_hw_soc_info        *soc_info,
	struct cam_hw_intf            *hw_intf,
	void                          *top_hw_info,
	struct cam_vfe_top            **vfe_top);

int cam_vfe_top_deinit(uint32_t        top_version,
	struct cam_vfe_top           **vfe_top);

#endif /* _CAM_VFE_TOP_H_*/
