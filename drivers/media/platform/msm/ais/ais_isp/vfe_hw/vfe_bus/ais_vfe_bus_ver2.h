/* Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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

#ifndef _AIS_VFE_BUS_VER2_H_
#define _AIS_VFE_BUS_VER2_H_

#include "ais_vfe_bus.h"

#define AIS_VFE_BUS_VER2_MAX_CLIENTS 24

#define AIS_VFE_BUS_ENABLE_DMI_DUMP                     BIT(0)
#define AIS_VFE_BUS_ENABLE_STATS_REG_DUMP               BIT(1)

enum ais_vfe_bus_ver2_vfe_core_id {
	AIS_VFE_BUS_VER2_VFE_CORE_0,
	AIS_VFE_BUS_VER2_VFE_CORE_1,
	AIS_VFE_BUS_VER2_VFE_CORE_MAX,
};

enum ais_vfe_bus_ver2_comp_grp_type {
	AIS_VFE_BUS_VER2_COMP_GRP_0,
	AIS_VFE_BUS_VER2_COMP_GRP_1,
	AIS_VFE_BUS_VER2_COMP_GRP_2,
	AIS_VFE_BUS_VER2_COMP_GRP_3,
	AIS_VFE_BUS_VER2_COMP_GRP_4,
	AIS_VFE_BUS_VER2_COMP_GRP_5,
	AIS_VFE_BUS_VER2_COMP_GRP_DUAL_0,
	AIS_VFE_BUS_VER2_COMP_GRP_DUAL_1,
	AIS_VFE_BUS_VER2_COMP_GRP_DUAL_2,
	AIS_VFE_BUS_VER2_COMP_GRP_DUAL_3,
	AIS_VFE_BUS_VER2_COMP_GRP_DUAL_4,
	AIS_VFE_BUS_VER2_COMP_GRP_DUAL_5,
	AIS_VFE_BUS_VER2_COMP_GRP_MAX,
};

enum ais_vfe_bus_ver2_vfe_out_type {
	AIS_VFE_BUS_VER2_VFE_OUT_RDI0,
	AIS_VFE_BUS_VER2_VFE_OUT_RDI1,
	AIS_VFE_BUS_VER2_VFE_OUT_RDI2,
	AIS_VFE_BUS_VER2_VFE_OUT_RDI3,
	AIS_VFE_BUS_VER2_VFE_OUT_FULL,
	AIS_VFE_BUS_VER2_VFE_OUT_DS4,
	AIS_VFE_BUS_VER2_VFE_OUT_DS16,
	AIS_VFE_BUS_VER2_VFE_OUT_RAW_DUMP,
	AIS_VFE_BUS_VER2_VFE_OUT_FD,
	AIS_VFE_BUS_VER2_VFE_OUT_PDAF,
	AIS_VFE_BUS_VER2_VFE_OUT_STATS_HDR_BE,
	AIS_VFE_BUS_VER2_VFE_OUT_STATS_HDR_BHIST,
	AIS_VFE_BUS_VER2_VFE_OUT_STATS_TL_BG,
	AIS_VFE_BUS_VER2_VFE_OUT_STATS_BF,
	AIS_VFE_BUS_VER2_VFE_OUT_STATS_AWB_BG,
	AIS_VFE_BUS_VER2_VFE_OUT_STATS_BHIST,
	AIS_VFE_BUS_VER2_VFE_OUT_STATS_RS,
	AIS_VFE_BUS_VER2_VFE_OUT_STATS_CS,
	AIS_VFE_BUS_VER2_VFE_OUT_STATS_IHIST,
	AIS_VFE_BUS_VER2_VFE_OUT_FULL_DISP,
	AIS_VFE_BUS_VER2_VFE_OUT_DS4_DISP,
	AIS_VFE_BUS_VER2_VFE_OUT_DS16_DISP,
	AIS_VFE_BUS_VER2_VFE_OUT_2PD,
	AIS_VFE_BUS_VER2_VFE_OUT_MAX,
};

struct ais_vfe_bus_ver2_dmi_lut_bank_info {
	uint32_t size;
	uint32_t bank_0;
	uint32_t bank_1;
};

struct ais_vfe_bus_ver2_stats_cfg_offset {
	uint32_t res_index;
	uint32_t cfg_offset;
	uint32_t num_cfg;
	uint32_t cfg_size;
	uint32_t is_lut;
	struct ais_vfe_bus_ver2_dmi_lut_bank_info lut;
};

struct ais_vfe_bus_ver2_dmi_offset_common {
	uint32_t auto_increment;
	uint32_t cfg_offset;
	uint32_t addr_offset;
	uint32_t data_hi_offset;
	uint32_t data_lo_offset;
};

struct ais_vfe_bus_ver2_stats_cfg_info {
	struct ais_vfe_bus_ver2_dmi_offset_common
		dmi_offset_info;
	struct ais_vfe_bus_ver2_stats_cfg_offset
		stats_cfg_offset[AIS_VFE_BUS_VER2_VFE_OUT_MAX];
};

/*
 * struct ais_vfe_bus_ver2_reg_offset_common:
 *
 * @Brief:        Common registers across all BUS Clients
 */
struct ais_vfe_bus_ver2_reg_offset_common {
	uint32_t hw_version;
	uint32_t hw_capability;
	uint32_t sw_reset;
	uint32_t cgc_ovd;
	uint32_t pwr_iso_cfg;
	uint32_t dual_master_comp_cfg;
	struct ais_irq_controller_reg_info irq_reg_info;
	uint32_t comp_error_status;
	uint32_t comp_ovrwr_status;
	uint32_t dual_comp_error_status;
	uint32_t dual_comp_ovrwr_status;
	uint32_t addr_sync_cfg;
	uint32_t addr_sync_frame_hdr;
	uint32_t addr_sync_no_sync;
	uint32_t addr_fifo_status;
	uint32_t debug_status_cfg;
	uint32_t debug_status_0;
};

/*
 * struct ais_vfe_bus_ver2_reg_offset_ubwc_client:
 *
 * @Brief:        UBWC register offsets for BUS Clients
 */
struct ais_vfe_bus_ver2_reg_offset_ubwc_client {
	uint32_t tile_cfg;
	uint32_t h_init;
	uint32_t v_init;
	uint32_t meta_addr;
	uint32_t meta_offset;
	uint32_t meta_stride;
	uint32_t mode_cfg_0;
	uint32_t bw_limit;
};

/*
 * struct ais_vfe_bus_ver2_reg_offset_ubwc_client:
 *
 * @Brief:        UBWC register offsets for BUS Clients
 */
struct ais_vfe_bus_ver2_reg_offset_ubwc_3_client {
	uint32_t tile_cfg;
	uint32_t h_init;
	uint32_t v_init;
	uint32_t meta_addr;
	uint32_t meta_offset;
	uint32_t meta_stride;
	uint32_t mode_cfg_0;
	uint32_t mode_cfg_1;
	uint32_t bw_limit;
	uint32_t threshlod_lossy_0;
	uint32_t threshlod_lossy_1;
};


/*
 * struct ais_vfe_bus_ver2_reg_offset_bus_client:
 *
 * @Brief:        Register offsets for BUS Clients
 */
struct ais_vfe_bus_ver2_reg_offset_bus_client {
	uint32_t status0;
	uint32_t status1;
	uint32_t cfg;
	uint32_t header_addr;
	uint32_t header_cfg;
	uint32_t image_addr;
	uint32_t image_addr_offset;
	uint32_t buffer_width_cfg;
	uint32_t buffer_height_cfg;
	uint32_t packer_cfg;
	uint32_t stride;
	uint32_t irq_subsample_period;
	uint32_t irq_subsample_pattern;
	uint32_t framedrop_period;
	uint32_t framedrop_pattern;
	uint32_t frame_inc;
	uint32_t burst_limit;
	void    *ubwc_regs;
};

/*
 * struct ais_vfe_bus_ver2_reg_offset_comp_grp:
 *
 * @Brief:        Register offsets for Composite Group registers
 * comp_mask:     Comp group register address
 * addr_sync_mask:Address sync group register address
 */
struct ais_vfe_bus_ver2_reg_offset_comp_grp {
	uint32_t                            comp_mask;
	uint32_t                            addr_sync_mask;
};

/*
 * struct ais_vfe_bus_ver2_vfe_out_hw_info:
 *
 * @Brief:        HW capability of VFE Bus Client
 */
struct ais_vfe_bus_ver2_vfe_out_hw_info {
	enum ais_vfe_bus_ver2_vfe_out_type  vfe_out_type;
	uint32_t                            max_width;
	uint32_t                            max_height;
};

/*
 * struct ais_vfe_bus_ver2_reg_data:
 *
 * @Brief:        Holds the bus register data
 */

struct ais_vfe_bus_ver2_reg_data {
	uint32_t      ubwc_10bit_threshold_lossy_0;
	uint32_t      ubwc_10bit_threshold_lossy_1;
	uint32_t      ubwc_8bit_threshold_lossy_0;
	uint32_t      ubwc_8bit_threshold_lossy_1;
};

/*
 * struct ais_vfe_bus_ver2_hw_info:
 *
 * @Brief:            HW register info for entire Bus
 *
 * @common_reg:       Common register details
 * @bus_client_reg:   Bus client register info
 * @comp_reg_grp:     Composite group register info
 * @vfe_out_hw_info:  VFE output capability
 * @reg_data:         bus register data;
 */
struct ais_vfe_bus_ver2_hw_info {
	struct ais_vfe_bus_ver2_reg_offset_common common_reg;
	uint32_t num_client;
	uint32_t is_lite;
	struct ais_vfe_bus_ver2_reg_offset_bus_client
		bus_client_reg[AIS_VFE_BUS_VER2_MAX_CLIENTS];
	struct ais_vfe_bus_ver2_reg_offset_comp_grp
		comp_grp_reg[AIS_VFE_BUS_VER2_COMP_GRP_MAX];
	uint32_t num_out;
	struct ais_vfe_bus_ver2_vfe_out_hw_info
		vfe_out_hw_info[AIS_VFE_BUS_VER2_VFE_OUT_MAX];
	struct ais_vfe_bus_ver2_reg_data  reg_data;
	struct ais_vfe_bus_ver2_stats_cfg_info *stats_data;
};

#endif /* _AIS_VFE_BUS_VER2_H_ */
