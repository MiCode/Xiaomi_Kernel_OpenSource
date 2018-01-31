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

#ifndef _CAM_FD_HW_SOC_H_
#define _CAM_FD_HW_SOC_H_

#include "cam_soc_util.h"

/**
 * enum cam_fd_reg_base - Enum for FD register sets
 *
 * @CAM_FD_REG_CORE    : Indicates FD Core register space
 * @CAM_FD_REG_WRAPPER : Indicates FD Wrapper register space
 * @CAM_FD_REG_MAX     : Max number of register sets supported
 */
enum cam_fd_reg_base {
	CAM_FD_REG_CORE,
	CAM_FD_REG_WRAPPER,
	CAM_FD_REG_MAX
};

/**
 * struct cam_fd_soc_private : FD private SOC information
 *
 * @regbase_index : Mapping between Register base enum to register index in SOC
 * @cpas_handle   : CPAS handle
 *
 */
struct cam_fd_soc_private {
	int32_t        regbase_index[CAM_FD_REG_MAX];
	uint32_t       cpas_handle;
};

int cam_fd_soc_init_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t irq_handler, void *private_data);
int cam_fd_soc_deinit_resources(struct cam_hw_soc_info *soc_info);
int cam_fd_soc_enable_resources(struct cam_hw_soc_info *soc_info);
int cam_fd_soc_disable_resources(struct cam_hw_soc_info *soc_info);
uint32_t cam_fd_soc_register_read(struct cam_hw_soc_info *soc_info,
	enum cam_fd_reg_base reg_base, uint32_t reg_offset);
void cam_fd_soc_register_write(struct cam_hw_soc_info *soc_info,
	enum cam_fd_reg_base reg_base, uint32_t reg_offset, uint32_t reg_value);

#endif /* _CAM_FD_HW_SOC_H_ */
