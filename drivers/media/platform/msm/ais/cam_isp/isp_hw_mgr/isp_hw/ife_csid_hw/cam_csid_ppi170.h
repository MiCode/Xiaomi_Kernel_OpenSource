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

#ifndef _CAM_CSID_PPI_170_H_
#define _CAM_CSID_PPI_170_H_

#include "cam_csid_ppi_core.h"

static struct cam_csid_ppi_reg_offset cam_csid_ppi_170_reg_offset = {
	.ppi_hw_version_addr    = 0,
	.ppi_module_cfg_addr    = 0x60,
	.ppi_irq_status_addr    = 0x68,
	.ppi_irq_mask_addr      = 0x6c,
	.ppi_irq_set_addr       = 0x70,
	.ppi_irq_clear_addr     = 0x74,
	.ppi_irq_cmd_addr       = 0x78,
	.ppi_rst_cmd_addr       = 0x7c,
	.ppi_test_bus_ctrl_addr = 0x1f4,
	.ppi_debug_addr         = 0x1f8,
	.ppi_spare_addr         = 0x1fc,
};

#endif /*_CAM_CSID_PPI_170_H_ */
