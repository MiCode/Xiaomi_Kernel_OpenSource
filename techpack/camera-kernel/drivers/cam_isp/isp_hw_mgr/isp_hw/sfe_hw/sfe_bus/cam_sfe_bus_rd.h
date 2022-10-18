/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_SFE_BUS_RD_H_
#define _CAM_SFE_BUS_RD_H_

#include "cam_sfe_bus.h"

#define CAM_SFE_BUS_RD_MAX_CLIENTS 3

enum cam_sfe_bus_rd_type {
	CAM_SFE_BUS_RD_RDI0,
	CAM_SFE_BUS_RD_RDI1,
	CAM_SFE_BUS_RD_RDI2,
	CAM_SFE_BUS_RD_MAX,
};

/*
 * struct cam_sfe_bus_rd_reg_offset_common:
 *
 * @Brief:        Common registers across BUS RD Clients
 */
struct cam_sfe_bus_rd_reg_offset_common {
	uint32_t hw_version;
	uint32_t misr_reset;
	uint32_t pwr_iso_cfg;
	uint32_t input_if_cmd;
	uint32_t test_bus_ctrl;
	uint32_t security_cfg;
	uint32_t cons_violation_status;
	struct cam_irq_controller_reg_info irq_reg_info;
};

/*
 * struct cam_sfe_bus_rd_reg_offset_bus_client:
 *
 * @Brief:        Register offsets for BUS RD Clients
 */
struct cam_sfe_bus_rd_reg_offset_bus_client {
	uint32_t cfg;
	uint32_t image_addr;
	uint32_t buf_width;
	uint32_t buf_height;
	uint32_t stride;
	uint32_t unpacker_cfg;
	uint32_t latency_buf_allocation;
	uint32_t system_cache_cfg;
	uint32_t addr_cfg;
};

/*
 * struct cam_sfe_bus_rd_hw_info:
 *
 * @Brief:        HW capability of SFE Bus RD Client
 */
struct cam_sfe_bus_rd_info {
	enum cam_sfe_bus_rd_type  sfe_bus_rd_type;
	uint32_t                  mid[CAM_SFE_BUS_MAX_MID_PER_PORT];
	uint32_t                  max_width;
	uint32_t                  max_height;
};

/*
 * struct cam_sfe_bus_rd_hw_info:
 *
 * @Brief:            HW register info for entire Bus
 *
 * @common_reg:             Common register details
 * @num_client:             Number of bus rd clients
 * @bus_client_reg:         Bus client register info
 * @num_bus_rd_resc:        Number of SFE BUS RD masters
 * @sfe_bus_rd_info:        SFE bus rd client info
 * @top_irq_shift:          Top irq shift val
 * @latency_buf_allocation: latency buf allocation
 */
struct cam_sfe_bus_rd_hw_info {
	struct cam_sfe_bus_rd_reg_offset_common common_reg;
	uint32_t num_client;
	struct cam_sfe_bus_rd_reg_offset_bus_client
		bus_client_reg[CAM_SFE_BUS_RD_MAX_CLIENTS];
	uint32_t num_bus_rd_resc;
	struct cam_sfe_bus_rd_info
		sfe_bus_rd_info[CAM_SFE_BUS_RD_MAX];
	uint32_t top_irq_shift;
	uint32_t latency_buf_allocation;
};

/*
 * cam_sfe_bus_rd_init()
 *
 * @Brief:                   Initialize Bus layer
 *
 * @soc_info:                Soc Information for the associated HW
 * @hw_intf:                 HW Interface of HW to which this resource belongs
 * @bus_hw_info:             BUS HW info that contains details of BUS registers
 * @sfe_bus:                 Pointer to sfe_bus structure which will be filled
 *                           and returned on successful initialize
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_sfe_bus_rd_init(
	struct cam_hw_soc_info               *soc_info,
	struct cam_hw_intf                   *hw_intf,
	void                                 *bus_hw_info,
	void                                 *sfe_irq_controller,
	struct cam_sfe_bus                  **sfe_bus);

/*
 * cam_sfe_bus_rd_deinit()
 *
 * @Brief:                   Deinitialize Bus layer
 *
 * @sfe_bus:                 Pointer to sfe_bus structure to deinitialize
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_sfe_bus_rd_deinit(struct cam_sfe_bus       **sfe_bus);

#endif /* _CAM_SFE_BUS_RD_H_ */
