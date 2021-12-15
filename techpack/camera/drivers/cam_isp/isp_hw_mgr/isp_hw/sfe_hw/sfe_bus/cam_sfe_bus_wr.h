/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */


#ifndef _CAM_SFE_BUS_WR_H_
#define _CAM_SFE_BUS_WR_H_

#include "cam_sfe_bus.h"

#define CAM_SFE_BUS_WR_MAX_CLIENTS     13
#define CAM_SFE_BUS_WR_MAX_SUB_GRPS    6

enum cam_sfe_bus_wr_src_grp {
	CAM_SFE_BUS_WR_SRC_GRP_0,
	CAM_SFE_BUS_WR_SRC_GRP_1,
	CAM_SFE_BUS_WR_SRC_GRP_2,
	CAM_SFE_BUS_WR_SRC_GRP_3,
	CAM_SFE_BUS_WR_SRC_GRP_4,
	CAM_SFE_BUS_WR_SRC_GRP_5,
	CAM_SFE_BUS_WR_SRC_GRP_MAX,
};

enum cam_sfe_bus_wr_comp_grp_type {
	CAM_SFE_BUS_WR_COMP_GRP_0,
	CAM_SFE_BUS_WR_COMP_GRP_1,
	CAM_SFE_BUS_WR_COMP_GRP_2,
	CAM_SFE_BUS_WR_COMP_GRP_3,
	CAM_SFE_BUS_WR_COMP_GRP_4,
	CAM_SFE_BUS_WR_COMP_GRP_5,
	CAM_SFE_BUS_WR_COMP_GRP_6,
	CAM_SFE_BUS_WR_COMP_GRP_7,
	CAM_SFE_BUS_WR_COMP_GRP_8,
	CAM_SFE_BUS_WR_COMP_GRP_9,
	CAM_SFE_BUS_WR_COMP_GRP_MAX,
};

enum cam_sfe_bus_sfe_out_type {
	CAM_SFE_BUS_SFE_OUT_RDI0,
	CAM_SFE_BUS_SFE_OUT_RDI1,
	CAM_SFE_BUS_SFE_OUT_RDI2,
	CAM_SFE_BUS_SFE_OUT_RDI3,
	CAM_SFE_BUS_SFE_OUT_RDI4,
	CAM_SFE_BUS_SFE_OUT_RAW_DUMP,
	CAM_SFE_BUS_SFE_OUT_LCR,
	CAM_SFE_BUS_SFE_OUT_BE_0,
	CAM_SFE_BUS_SFE_OUT_BHIST_0,
	CAM_SFE_BUS_SFE_OUT_BE_1,
	CAM_SFE_BUS_SFE_OUT_BHIST_1,
	CAM_SFE_BUS_SFE_OUT_BE_2,
	CAM_SFE_BUS_SFE_OUT_BHIST_2,
	CAM_SFE_BUS_SFE_OUT_MAX,
};

/*
 * struct cam_sfe_bus_reg_offset_common:
 *
 * @Brief:        Common registers across all BUS Clients
 */
struct cam_sfe_bus_reg_offset_common {
	uint32_t hw_version;
	uint32_t cgc_ovd;
	uint32_t if_frameheader_cfg[CAM_SFE_BUS_WR_MAX_SUB_GRPS];
	uint32_t pwr_iso_cfg;
	uint32_t overflow_status_clear;
	uint32_t ccif_violation_status;
	uint32_t overflow_status;
	uint32_t image_size_violation_status;
	uint32_t debug_status_top_cfg;
	uint32_t debug_status_top;
	uint32_t test_bus_ctrl;
	uint32_t top_irq_mask_0;
	struct cam_irq_controller_reg_info irq_reg_info;
};

/*
 * struct cam_sfe_bus_reg_offset_bus_client:
 *
 * @Brief:        Register offsets for BUS Clients
 */
struct cam_sfe_bus_reg_offset_bus_client {
	uint32_t cfg;
	uint32_t image_addr;
	uint32_t frame_incr;
	uint32_t image_cfg_0;
	uint32_t image_cfg_1;
	uint32_t image_cfg_2;
	uint32_t packer_cfg;
	uint32_t frame_header_addr;
	uint32_t frame_header_incr;
	uint32_t frame_header_cfg;
	uint32_t line_done_cfg;
	uint32_t irq_subsample_period;
	uint32_t irq_subsample_pattern;
	uint32_t framedrop_period;
	uint32_t framedrop_pattern;
	uint32_t system_cache_cfg;
	uint32_t addr_status_0;
	uint32_t addr_status_1;
	uint32_t addr_status_2;
	uint32_t addr_status_3;
	uint32_t debug_status_cfg;
	uint32_t debug_status_0;
	uint32_t debug_status_1;
	uint32_t comp_group;
};

/*
 * struct cam_sfe_bus_sfe_out_hw_info:
 *
 * @Brief:        HW capability of SFE Bus Client
 */
struct cam_sfe_bus_sfe_out_hw_info {
	enum cam_sfe_bus_sfe_out_type       sfe_out_type;
	uint32_t                            max_width;
	uint32_t                            max_height;
	uint32_t                            source_group;
	uint32_t                            num_wm;
	uint32_t                            wm_idx;
};

/*
 * struct cam_sfe_bus_wr_hw_info:
 *
 * @Brief:            HW register info for entire Bus
 *
 * @common_reg:       Common register details
 * @num_client:       Total number of write clients
 * @bus_client_reg:   Bus client register info
 * @sfe_out_hw_info:  SFE output capability
 * @num_comp_grp:     Number of composite groups
 * @comp_done_shift:  Mask shift for comp done mask
 * @top_irq_shift:    Mask shift for top level BUS WR irq
 */
struct cam_sfe_bus_wr_hw_info {
	struct cam_sfe_bus_reg_offset_common common_reg;
	uint32_t num_client;
	struct cam_sfe_bus_reg_offset_bus_client
		bus_client_reg[CAM_SFE_BUS_WR_MAX_CLIENTS];
	uint32_t num_out;
	struct cam_sfe_bus_sfe_out_hw_info
		sfe_out_hw_info[CAM_SFE_BUS_SFE_OUT_MAX];
	uint32_t num_comp_grp;
	uint32_t comp_done_shift;
	uint32_t top_irq_shift;
};

/*
 * cam_sfe_bus_wr_init()
 *
 * @Brief:                   Initialize Bus layer
 *
 * @soc_info:                Soc Information for the associated HW
 * @hw_intf:                 HW Interface of HW to which this resource belongs
 * @bus_hw_info:             BUS HW info that contains details of BUS registers
 * @sfe_irq_controller:      SFE irq controller
 * @sfe_bus:                 Pointer to sfe_bus structure which will be filled
 *                           and returned on successful initialize
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_sfe_bus_wr_init(
	struct cam_hw_soc_info               *soc_info,
	struct cam_hw_intf                   *hw_intf,
	void                                 *bus_hw_info,
	void                                 *sfe_irq_controller,
	struct cam_sfe_bus                  **sfe_bus);

/*
 * cam_sfe_bus_wr_deinit()
 *
 * @Brief:                   Deinitialize Bus layer
 *
 * @sfe_bus:                 Pointer to sfe_bus structure to deinitialize
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_sfe_bus_wr_deinit(struct cam_sfe_bus     **sfe_bus);

#endif /* _CAM_SFE_BUS_WR_H_ */
