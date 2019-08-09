/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_CSID_PPI_HW_H_
#define _CAM_CSID_PPI_HW_H_

#include "cam_hw.h"
#include "cam_hw_intf.h"

#define CAM_CSID_PPI_HW_MAX      4
#define CAM_CSID_PPI_LANES_MAX   3

#define PPI_IRQ_RST_DONE                   BIT(0)
#define PPI_IRQ_FIFO0_OVERFLOW             BIT(1)
#define PPI_IRQ_FIFO1_OVERFLOW             BIT(2)
#define PPI_IRQ_FIFO2_OVERFLOW             BIT(3)

#define PPI_IRQ_CMD_SET                    BIT(0)

#define PPI_IRQ_CMD_CLEAR                  BIT(0)

#define PPI_RST_CONTROL                    BIT(0)
/*
 * Select the PHY (CPHY set '1' or DPHY set '0')
 */
#define PPI_CFG_CPHY_DLX_SEL(X)            ((X < 2) ? BIT(X) : 0)

#define PPI_CFG_CPHY_DLX_EN(X)             BIT(4+X)

struct cam_csid_ppi_reg_offset {
	uint32_t ppi_hw_version_addr;
	uint32_t ppi_module_cfg_addr;

	uint32_t ppi_irq_status_addr;
	uint32_t ppi_irq_mask_addr;
	uint32_t ppi_irq_set_addr;
	uint32_t ppi_irq_clear_addr;
	uint32_t ppi_irq_cmd_addr;
	uint32_t ppi_rst_cmd_addr;
	uint32_t ppi_test_bus_ctrl_addr;
	uint32_t ppi_debug_addr;
	uint32_t ppi_spare_addr;
};

/**
 * struct cam_csid_ppi_hw_info- ppi HW info
 *
 * @ppi_reg:         ppi register offsets
 *
 */
struct cam_csid_ppi_hw_info {
	const struct cam_csid_ppi_reg_offset *ppi_reg;
};

/**
 * struct cam_csid_ppi_hw- ppi hw device resources data
 *
 * @hw_intf:                  contain the ppi hw interface information
 * @hw_info:                  ppi hw device information
 * @ppi_info:                 ppi hw specific information
 * @device_enabled            Device enabled will set once ppi powered on and
 *                            initial configuration are done.
 * @lock_state                ppi spin lock
 *
 */
struct cam_csid_ppi_hw {
	struct cam_hw_intf              *hw_intf;
	struct cam_hw_info              *hw_info;
	struct cam_csid_ppi_hw_info     *ppi_info;
	uint32_t                         device_enabled;
};

/**
 * struct cam_csid_ppi_cfg - ppi lane configuration data
 * @lane_type:   lane type: c-phy or d-phy
 * @lane_num :   active lane number
 * @lane_cfg:    lane configurations: 4 bits per lane
 *
 */
struct cam_csid_ppi_cfg {
	uint32_t lane_type;
	uint32_t lane_num;
	uint32_t lane_cfg;
};

int cam_csid_ppi_hw_probe_init(struct cam_hw_intf  *ppi_hw_intf,
	uint32_t ppi_idx);
int cam_csid_ppi_hw_deinit(struct cam_csid_ppi_hw *csid_ppi_hw);
int cam_csid_ppi_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t ppi_irq_handler, void *irq_data);
int cam_csid_ppi_deinit_soc_resources(struct cam_hw_soc_info *soc_info);
int cam_csid_ppi_hw_init(struct cam_hw_intf **csid_ppi_hw,
	uint32_t hw_idx);
#endif /* _CAM_CSID_PPI_HW_H_ */
