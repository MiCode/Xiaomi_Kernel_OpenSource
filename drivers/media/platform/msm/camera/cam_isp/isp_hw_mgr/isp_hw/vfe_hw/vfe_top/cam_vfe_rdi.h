/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_VFE_RDI_H_
#define _CAM_VFE_RDI_H_

#include "cam_isp_hw.h"
#include "cam_vfe_top.h"

#define CAM_VFE_RDI_VER2_MAX  4

struct cam_vfe_rdi_ver2_reg {
	uint32_t     reg_update_cmd;
};

struct cam_vfe_rdi_reg_data {
	uint32_t     reg_update_cmd_data;
	uint32_t     sof_irq_mask;
	uint32_t     reg_update_irq_mask;
};

struct cam_vfe_rdi_ver2_hw_info {
	struct cam_vfe_top_ver2_reg_offset_common  *common_reg;
	struct cam_vfe_rdi_ver2_reg                *rdi_reg;
	struct cam_vfe_rdi_reg_data  *reg_data[CAM_VFE_RDI_VER2_MAX];
};

int cam_vfe_rdi_ver2_acquire_resource(
	struct cam_isp_resource_node  *rdi_res,
	void                          *acquire_param);

int cam_vfe_rdi_ver2_init(
	struct cam_hw_intf            *hw_intf,
	struct cam_hw_soc_info        *soc_info,
	void                          *rdi_hw_info,
	struct cam_isp_resource_node  *rdi_node);

int cam_vfe_rdi_ver2_deinit(
	struct cam_isp_resource_node  *rdi_node);

#endif /* _CAM_VFE_RDI_H_ */
