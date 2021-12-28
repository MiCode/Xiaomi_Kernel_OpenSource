/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_VFE_BUS_R_VER1_H_
#define _CAM_VFE_BUS_R_VER1_H_

#include "cam_irq_controller.h"
#include "cam_vfe_bus.h"

#define CAM_VFE_BUS_RD_VER1_MAX_CLIENTS 1

enum cam_vfe_bus_rd_ver1_vfe_core_id {
	CAM_VFE_BUS_RD_VER1_VFE_CORE_0,
	CAM_VFE_BUS_RD_VER1_VFE_CORE_1,
	CAM_VFE_BUS_RD_VER1_VFE_CORE_MAX,
};

enum cam_vfe_bus_rd_ver1_comp_grp_type {
	CAM_VFE_BUS_RD_VER1_COMP_GRP_0,
	CAM_VFE_BUS_RD_VER1_COMP_GRP_MAX,
};


enum cam_vfe_bus_rd_ver1_vfe_bus_rd_type {
	CAM_VFE_BUS_RD_VER1_VFE_BUSRD_RDI0,
	CAM_VFE_BUS_RD_VER1_VFE_BUSRD_MAX,
};

/*
 * struct cam_vfe_bus_rd_ver1_reg_offset_common:
 *
 * @Brief:        Common registers across all BUS Clients
 */
struct cam_vfe_bus_rd_ver1_reg_offset_common {
	uint32_t hw_version;
	uint32_t hw_capability;
	uint32_t sw_reset;
	uint32_t cgc_ovd;
	uint32_t pwr_iso_cfg;
	uint32_t input_if_cmd;
	uint32_t test_bus_ctrl;
	struct cam_irq_controller_reg_info irq_reg_info;
};

/*
 * struct cam_vfe_bus_rd_ver1_reg_offset_bus_client:
 *
 * @Brief:        Register offsets for BUS Clients
 */
struct cam_vfe_bus_rd_ver1_reg_offset_bus_client {
	uint32_t cfg;
	uint32_t image_addr;
	uint32_t buf_size;
	uint32_t stride;
	uint32_t unpacker_cfg;
	uint32_t latency_buf_allocation;
	uint32_t burst_limit;
};

/*
 * struct cam_vfe_bus_rd_ver1_vfe_bus_hw_info:
 *
 * @Brief:        HW capability of VFE Bus Client
 */
struct cam_vfe_bus_rd_ver1_vfe_bus_hw_info {
	enum cam_vfe_bus_rd_ver1_vfe_bus_rd_type  vfe_bus_rd_type;
	uint32_t                            max_width;
	uint32_t                            max_height;
};

/*
 * struct cam_vfe_bus_rd_ver1_hw_info:
 *
 * @Brief:            HW register info for entire Bus
 *
 * @common_reg:       Common register details
 * @bus_client_reg:   Bus client register info
 * @comp_reg_grp:     Composite group register info
 * @vfe_out_hw_info:  VFE output capability
 */
struct cam_vfe_bus_rd_ver1_hw_info {
	struct cam_vfe_bus_rd_ver1_reg_offset_common common_reg;
	uint32_t num_client;
	struct cam_vfe_bus_rd_ver1_reg_offset_bus_client
		bus_client_reg[CAM_VFE_BUS_RD_VER1_MAX_CLIENTS];
	uint32_t num_bus_rd_resc;
	struct cam_vfe_bus_rd_ver1_vfe_bus_hw_info
		vfe_bus_rd_hw_info[CAM_VFE_BUS_RD_VER1_VFE_BUSRD_MAX];
	uint32_t top_irq_shift;
};

/*
 * cam_vfe_bus_rd_ver1_init()
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
int cam_vfe_bus_rd_ver1_init(
	struct cam_hw_soc_info               *soc_info,
	struct cam_hw_intf                   *hw_intf,
	void                                 *bus_hw_info,
	void                                 *vfe_irq_controller,
	struct cam_vfe_bus                  **vfe_bus);

/*
 * cam_vfe_bus_rd_bus_ver1_deinit()
 *
 * @Brief:                   Deinitialize Bus layer
 *
 * @vfe_bus:                 Pointer to vfe_bus structure to deinitialize
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_vfe_bus_rd_bus_ver1_deinit(struct cam_vfe_bus     **vfe_bus);

#endif /* _CAM_VFE_BUS_R_VER1_H_ */
