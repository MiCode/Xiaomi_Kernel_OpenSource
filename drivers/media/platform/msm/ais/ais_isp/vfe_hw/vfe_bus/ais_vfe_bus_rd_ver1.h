/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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

#ifndef _AIS_VFE_BUS_R_VER1_H_
#define _AIS_VFE_BUS_R_VER1_H_

#include "ais_vfe_bus.h"

#define AIS_VFE_BUS_RD_VER1_MAX_CLIENTS 1

enum ais_vfe_bus_rd_ver1_vfe_core_id {
	AIS_VFE_BUS_RD_VER1_VFE_CORE_0,
	AIS_VFE_BUS_RD_VER1_VFE_CORE_1,
	AIS_VFE_BUS_RD_VER1_VFE_CORE_MAX,
};

enum ais_vfe_bus_rd_ver1_comp_grp_type {
	AIS_VFE_BUS_RD_VER1_COMP_GRP_0,
	AIS_VFE_BUS_RD_VER1_COMP_GRP_MAX,
};


enum ais_vfe_bus_rd_ver1_vfe_bus_rd_type {
	AIS_VFE_BUS_RD_VER1_VFE_BUSRD_RDI0,
	AIS_VFE_BUS_RD_VER1_VFE_BUSRD_MAX,
};

/*
 * struct ais_vfe_bus_rd_ver1_reg_offset_common:
 *
 * @Brief:        Common registers across all BUS Clients
 */
struct ais_vfe_bus_rd_ver1_reg_offset_common {
	uint32_t hw_version;
	uint32_t hw_capability;
	uint32_t sw_reset;
	uint32_t cgc_ovd;
	uint32_t pwr_iso_cfg;
	uint32_t input_if_cmd;
	uint32_t test_bus_ctrl;
	struct ais_irq_controller_reg_info irq_reg_info;
};

/*
 * struct ais_vfe_bus_rd_ver1_reg_offset_bus_client:
 *
 * @Brief:        Register offsets for BUS Clients
 */
struct ais_vfe_bus_rd_ver1_reg_offset_bus_client {
	uint32_t status0;
	uint32_t status1;
	uint32_t cfg;
	uint32_t header_addr;
	uint32_t header_cfg;
	uint32_t image_addr;
	uint32_t image_addr_offset;
	uint32_t buffer_width_cfg;
	uint32_t buffer_height_cfg;
	uint32_t unpacker_cfg;
	uint32_t stride;
	void    *ubwc_regs;
	uint32_t burst_limit;
	uint32_t latency_buf_allocation;
	uint32_t buf_size;
};

/*
 * struct ais_vfe_bus_rd_ver1_vfe_bus_hw_info:
 *
 * @Brief:        HW capability of VFE Bus Client
 */
struct ais_vfe_bus_rd_ver1_vfe_bus_hw_info {
	enum ais_vfe_bus_rd_ver1_vfe_bus_rd_type  vfe_bus_rd_type;
	uint32_t                            max_width;
	uint32_t                            max_height;
};

/*
 * struct ais_vfe_bus_rd_ver1_hw_info:
 *
 * @Brief:            HW register info for entire Bus
 *
 * @common_reg:       Common register details
 * @bus_client_reg:   Bus client register info
 * @comp_reg_grp:     Composite group register info
 * @vfe_out_hw_info:  VFE output capability
 */
struct ais_vfe_bus_rd_ver1_hw_info {
	struct ais_vfe_bus_rd_ver1_reg_offset_common common_reg;
	uint32_t num_client;
	struct ais_vfe_bus_rd_ver1_reg_offset_bus_client
		bus_client_reg[AIS_VFE_BUS_RD_VER1_MAX_CLIENTS];
	uint32_t num_bus_rd_resc;
	struct ais_vfe_bus_rd_ver1_vfe_bus_hw_info
		vfe_bus_rd_hw_info[AIS_VFE_BUS_RD_VER1_VFE_BUSRD_MAX];
};

#endif /* _AIS_VFE_BUS_R_VER1_H_ */
