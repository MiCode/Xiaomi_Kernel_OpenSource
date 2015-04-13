/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#ifndef MSM_CSIPHY_3_5_HWREG_H
#define MSM_CSIPHY_3_5_HWREG_H

#include <sensor/csiphy/msm_csiphy.h>

struct csiphy_reg_parms_t csiphy_v3_5 = {
	.mipi_csiphy_interrupt_status0_addr = 0x8B0,
	.mipi_csiphy_interrupt_clear0_addr = 0x858,
	.mipi_csiphy_glbl_irq_cmd_addr = 0x828,
};

struct csiphy_reg_3ph_parms_t csiphy_v3_5_3ph = {
	/*MIPI CSI PHY registers*/
	0x814,
	0x818,
	0x188,
	0x18C,
	0x190,
	0x108,
	0x10c,
	0x118,
	0x11c,
	0x120,
	0x124,
	0x128,
	0x12c,
	0x144,
	0x160,
	0x1cc,
	0x164,
	0x81c,
	0x82c,
	0x830,
	0x834,
	0x838,
	0x83c,
	0x840,
	0x844,
	0x848,
	0x84c,
	0x850,
	0x854,
	0x28,
	0x800,
	0x0,
	0x4,
	0x8,
	0xC,
	0x10,
	0x2C,
	0x30,
	0x34,
	0x38,
	0x3C,
	0x1C,
	0x14,
};
#endif
