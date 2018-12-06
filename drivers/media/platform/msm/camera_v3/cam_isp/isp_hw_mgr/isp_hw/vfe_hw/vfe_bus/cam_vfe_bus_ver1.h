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

#ifndef _CAM_VFE_BUS_VER1_H_
#define _CAM_VFE_BUS_VER1_H_

enum cam_vfe_bus_ver1_pingpong_id {
	CAM_VFE_BUS_VER1_PING,
	CAM_VFE_BUS_VER1_PONG,
	CAM_VFE_BUS_VER1_PINGPONG_MAX,
};

enum cam_vfe_bus_ver1_wm_type {
	CAM_VFE_BUS_WM_TYPE_IMAGE,
	CAM_VFE_BUS_WM_TYPE_STATS,
	CAM_VFE_BUS_WM_TYPE_MAX,
};

enum cam_vfe_bus_ver1_comp_grp_type {
	CAM_VFE_BUS_VER1_COMP_GRP_IMG0,
	CAM_VFE_BUS_VER1_COMP_GRP_IMG1,
	CAM_VFE_BUS_VER1_COMP_GRP_IMG2,
	CAM_VFE_BUS_VER1_COMP_GRP_IMG3,
	CAM_VFE_BUS_VER1_COMP_GRP_STATS0,
	CAM_VFE_BUS_VER1_COMP_GRP_STATS1,
	CAM_VFE_BUS_VER1_COMP_GRP_MAX,
};

struct cam_vfe_bus_ver1_common_reg {
	uint32_t cmd_offset;
	uint32_t cfg_offset;
	uint32_t io_fmt_offset;
	uint32_t argb_cfg_offset;
	uint32_t xbar_cfg0_offset;
	uint32_t xbar_cfg1_offset;
	uint32_t xbar_cfg2_offset;
	uint32_t xbar_cfg3_offset;
	uint32_t ping_pong_status_reg;
};

struct cam_vfe_bus_ver1_wm_reg {
	uint32_t wm_cfg_offset;
	uint32_t ping_addr_offset;
	uint32_t ping_max_addr_offset;
	uint32_t pong_addr_offset;
	uint32_t pong_max_addr_offset;
	uint32_t addr_cfg_offset;
	uint32_t ub_cfg_offset;
	uint32_t image_size_offset;
	uint32_t buffer_cfg_offset;
	uint32_t framedrop_pattern_offset;
	uint32_t irq_subsample_pattern_offset;
	uint32_t ping_pong_status_bit; /* 0 - 31 */
	uint32_t composite_bit; /* 0 -31 */
};

struct cam_vfe_bus_ver1_wm_resource_data {
	uint32_t             index;
	uint32_t             wm_type;
	uint32_t             res_type;

	uint32_t             offset;
	uint32_t             width;
	uint32_t             height;
	uint32_t             stride;
	uint32_t             scanline;

	uint32_t             burst_len;

	uint32_t             framedrop_period;
	uint32_t             framedrop_pattern;

	uint32_t             buf_valid[CAM_VFE_BUS_VER1_PINGPONG_MAX];
	uint32_t             ub_size;
	uint32_t             ub_offset;

	struct cam_vfe_bus_ver1_wm_reg  hw_regs;
};

struct cam_vfe_bus_ver1_comp_grp_reg {
	enum cam_vfe_bus_ver1_comp_grp_type comp_grp_type;
	uint32_t             comp_grp_offset;
};

struct cam_vfe_bus_ver1_comp_grp {
	struct cam_vfe_bus_ver1_comp_grp_reg reg_info;
	struct list_head     wm_list;
	uint32_t             cur_bit_mask;
};

/*
 * cam_vfe_bus_ver1_init()
 *
 * @Brief:                   Initialize Bus layer
 *
 * @mem_base:                Mapped base address of register space
 * @hw_intf:                 HW Interface of HW to which this resource belongs
 * @bus_hw_info:             BUS HW info that contains details of BUS registers
 * @vfe_irq_controller:      VFE IRQ Controller to use for subscribing to Top
 *                           level IRQs
 * @vfe_bus:                 Pointer to vfe_bus structure which will be filled
 *                           and returned on successful initialize
 */
int cam_vfe_bus_ver1_init(
	void __iomem                         *mem_base,
	struct cam_hw_intf                   *hw_intf,
	void                                 *bus_hw_info,
	void                                 *vfe_irq_controller,
	struct cam_vfe_bus                  **vfe_bus);

#endif /* _CAM_VFE_BUS_VER1_H_ */
