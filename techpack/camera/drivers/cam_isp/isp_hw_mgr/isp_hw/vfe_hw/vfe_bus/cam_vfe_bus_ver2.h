/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_VFE_BUS_VER2_H_
#define _CAM_VFE_BUS_VER2_H_

#include "cam_irq_controller.h"
#include "cam_vfe_bus.h"

#define CAM_VFE_BUS_VER2_MAX_CLIENTS 24

enum cam_vfe_bus_ver2_vfe_core_id {
	CAM_VFE_BUS_VER2_VFE_CORE_0,
	CAM_VFE_BUS_VER2_VFE_CORE_1,
	CAM_VFE_BUS_VER2_VFE_CORE_2,
	CAM_VFE_BUS_VER2_VFE_CORE_MAX,
};

enum cam_vfe_bus_ver2_comp_grp_type {
	CAM_VFE_BUS_VER2_COMP_GRP_0,
	CAM_VFE_BUS_VER2_COMP_GRP_1,
	CAM_VFE_BUS_VER2_COMP_GRP_2,
	CAM_VFE_BUS_VER2_COMP_GRP_3,
	CAM_VFE_BUS_VER2_COMP_GRP_4,
	CAM_VFE_BUS_VER2_COMP_GRP_5,
	CAM_VFE_BUS_VER2_COMP_GRP_DUAL_0,
	CAM_VFE_BUS_VER2_COMP_GRP_DUAL_1,
	CAM_VFE_BUS_VER2_COMP_GRP_DUAL_2,
	CAM_VFE_BUS_VER2_COMP_GRP_DUAL_3,
	CAM_VFE_BUS_VER2_COMP_GRP_DUAL_4,
	CAM_VFE_BUS_VER2_COMP_GRP_DUAL_5,
	CAM_VFE_BUS_VER2_COMP_GRP_MAX,
};

enum cam_vfe_bus_ver2_vfe_out_type {
	CAM_VFE_BUS_VER2_VFE_OUT_RDI0,
	CAM_VFE_BUS_VER2_VFE_OUT_RDI1,
	CAM_VFE_BUS_VER2_VFE_OUT_RDI2,
	CAM_VFE_BUS_VER2_VFE_OUT_RDI3,
	CAM_VFE_BUS_VER2_VFE_OUT_FULL,
	CAM_VFE_BUS_VER2_VFE_OUT_DS4,
	CAM_VFE_BUS_VER2_VFE_OUT_DS16,
	CAM_VFE_BUS_VER2_VFE_OUT_RAW_DUMP,
	CAM_VFE_BUS_VER2_VFE_OUT_FD,
	CAM_VFE_BUS_VER2_VFE_OUT_PDAF,
	CAM_VFE_BUS_VER2_VFE_OUT_STATS_HDR_BE,
	CAM_VFE_BUS_VER2_VFE_OUT_STATS_HDR_BHIST,
	CAM_VFE_BUS_VER2_VFE_OUT_STATS_TL_BG,
	CAM_VFE_BUS_VER2_VFE_OUT_STATS_BF,
	CAM_VFE_BUS_VER2_VFE_OUT_STATS_AWB_BG,
	CAM_VFE_BUS_VER2_VFE_OUT_STATS_BHIST,
	CAM_VFE_BUS_VER2_VFE_OUT_STATS_RS,
	CAM_VFE_BUS_VER2_VFE_OUT_STATS_CS,
	CAM_VFE_BUS_VER2_VFE_OUT_STATS_IHIST,
	CAM_VFE_BUS_VER2_VFE_OUT_FULL_DISP,
	CAM_VFE_BUS_VER2_VFE_OUT_DS4_DISP,
	CAM_VFE_BUS_VER2_VFE_OUT_DS16_DISP,
	CAM_VFE_BUS_VER2_VFE_OUT_2PD,
	CAM_VFE_BUS_VER2_VFE_OUT_MAX,
};

struct cam_vfe_bus_ver2_dmi_lut_bank_info {
	uint32_t size;
	uint32_t bank_0;
	uint32_t bank_1;
};

struct cam_vfe_bus_ver2_stats_cfg_offset {
	uint32_t res_index;
	uint32_t cfg_offset;
	uint32_t num_cfg;
	uint32_t cfg_size;
	uint32_t is_lut;
	struct cam_vfe_bus_ver2_dmi_lut_bank_info lut;
};

struct cam_vfe_bus_ver2_dmi_offset_common {
	uint32_t auto_increment;
	uint32_t cfg_offset;
	uint32_t addr_offset;
	uint32_t data_hi_offset;
	uint32_t data_lo_offset;
};

struct cam_vfe_bus_ver2_stats_cfg_info {
	struct cam_vfe_bus_ver2_dmi_offset_common
		dmi_offset_info;
	struct cam_vfe_bus_ver2_stats_cfg_offset
		stats_cfg_offset[CAM_VFE_BUS_VER2_VFE_OUT_MAX];
};

/*
 * struct cam_vfe_bus_ver2_reg_offset_common:
 *
 * @Brief:        Common registers across all BUS Clients
 */
struct cam_vfe_bus_ver2_reg_offset_common {
	uint32_t hw_version;
	uint32_t hw_capability;
	uint32_t sw_reset;
	uint32_t cgc_ovd;
	uint32_t pwr_iso_cfg;
	uint32_t dual_master_comp_cfg;
	struct cam_irq_controller_reg_info irq_reg_info;
	uint32_t comp_error_status;
	uint32_t comp_ovrwr_status;
	uint32_t dual_comp_error_status;
	uint32_t dual_comp_ovrwr_status;
	uint32_t addr_sync_cfg;
	uint32_t addr_sync_frame_hdr;
	uint32_t addr_sync_no_sync;
	uint32_t debug_status_cfg;
	uint32_t debug_status_0;
};

/*
 * struct cam_vfe_bus_ver2_reg_offset_ubwc_client:
 *
 * @Brief:        UBWC register offsets for BUS Clients
 */
struct cam_vfe_bus_ver2_reg_offset_ubwc_client {
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
 * struct cam_vfe_bus_ver2_reg_offset_ubwc_client:
 *
 * @Brief:        UBWC register offsets for BUS Clients
 */
struct cam_vfe_bus_ver2_reg_offset_ubwc_3_client {
	uint32_t tile_cfg;
	uint32_t h_init;
	uint32_t v_init;
	uint32_t meta_addr;
	uint32_t meta_offset;
	uint32_t meta_stride;
	uint32_t mode_cfg_0;
	uint32_t mode_cfg_1;
	uint32_t bw_limit;
};


/*
 * struct cam_vfe_bus_ver2_reg_offset_bus_client:
 *
 * @Brief:        Register offsets for BUS Clients
 */
struct cam_vfe_bus_ver2_reg_offset_bus_client {
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
 * struct cam_vfe_bus_ver2_reg_offset_comp_grp:
 *
 * @Brief:        Register offsets for Composite Group registers
 * comp_mask:     Comp group register address
 * addr_sync_mask:Address sync group register address
 */
struct cam_vfe_bus_ver2_reg_offset_comp_grp {
	uint32_t                            comp_mask;
	uint32_t                            addr_sync_mask;
};

/*
 * struct cam_vfe_bus_ver2_vfe_out_hw_info:
 *
 * @Brief:        HW capability of VFE Bus Client
 */
struct cam_vfe_bus_ver2_vfe_out_hw_info {
	enum cam_vfe_bus_ver2_vfe_out_type  vfe_out_type;
	uint32_t                            max_width;
	uint32_t                            max_height;
};

/*
 * struct cam_vfe_bus_ver2_reg_data:
 *
 * @Brief:        Holds the bus register data
 */

struct cam_vfe_bus_ver2_reg_data {
	uint32_t      ubwc_10bit_threshold_lossy_0;
	uint32_t      ubwc_10bit_threshold_lossy_1;
	uint32_t      ubwc_8bit_threshold_lossy_0;
	uint32_t      ubwc_8bit_threshold_lossy_1;
};

/*
 * struct cam_vfe_bus_ver2_hw_info:
 *
 * @Brief:            HW register info for entire Bus
 *
 * @common_reg:       Common register details
 * @bus_client_reg:   Bus client register info
 * @comp_reg_grp:     Composite group register info
 * @vfe_out_hw_info:  VFE output capability
 */
struct cam_vfe_bus_ver2_hw_info {
	struct cam_vfe_bus_ver2_reg_offset_common common_reg;
	uint32_t num_client;
	struct cam_vfe_bus_ver2_reg_offset_bus_client
		bus_client_reg[CAM_VFE_BUS_VER2_MAX_CLIENTS];
	struct cam_vfe_bus_ver2_reg_offset_comp_grp
		comp_grp_reg[CAM_VFE_BUS_VER2_COMP_GRP_MAX];
	uint32_t num_out;
	struct cam_vfe_bus_ver2_vfe_out_hw_info
		vfe_out_hw_info[CAM_VFE_BUS_VER2_VFE_OUT_MAX];
	struct cam_vfe_bus_ver2_reg_data  reg_data;
	struct cam_vfe_bus_ver2_stats_cfg_info *stats_data;
};

/*
 * cam_vfe_bus_ver2_init()
 *
 * @Brief:                   Initialize Bus layer
 *
 * @soc_info:                Soc Information for the associated HW
 * @hw_intf:                 HW Interface of HW to which this resource belongs
 * @bus_hw_info:             BUS HW info that contains details of BUS registers
 * @vfe_irq_controller:      VFE IRQ Controller to use for subscribing to Top
 *                           level IRQs
 * @vfe_bus:                 Pointer to vfe_bus structure which will be filled
 *                           and returned on successful initialize
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_vfe_bus_ver2_init(
	struct cam_hw_soc_info               *soc_info,
	struct cam_hw_intf                   *hw_intf,
	void                                 *bus_hw_info,
	void                                 *vfe_irq_controller,
	struct cam_vfe_bus                  **vfe_bus);

/*
 * cam_vfe_bus_ver2_deinit()
 *
 * @Brief:                   Deinitialize Bus layer
 *
 * @vfe_bus:                 Pointer to vfe_bus structure to deinitialize
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_vfe_bus_ver2_deinit(struct cam_vfe_bus     **vfe_bus);

#endif /* _CAM_VFE_BUS_VER2_H_ */
