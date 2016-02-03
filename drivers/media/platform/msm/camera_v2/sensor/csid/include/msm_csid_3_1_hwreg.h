 /* Copyright (c) 2014-2015  The Linux Foundation. All rights reserved.
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

#ifndef MSM_CSID_3_1_HWREG_H
#define MSM_CSID_3_1_HWREG_H

#include <sensor/csid/msm_csid.h>

uint8_t csid_lane_assign_v3_1[PHY_LANE_MAX] = {0, 1, 2, 3, 4};

struct csid_reg_parms_t csid_v3_1 = {
	/* MIPI	CSID registers */
	0x0,
	0x4,
	0x8,
	0xC,
	0x10,
	0x14,
	0x18,
	0x1C,
	0x20,
	0x60,
	0x64,
	0x68,
	0x6C,
	0x70,
	0x74,
	0x78,
	0x7C,
	0x80,
	0x84,
	0x88,
	0x8C,
	0x90,
	0x94,
	0x98,
	0xA0,
	0xA4,
	0xAC,
	0xB0,
	0xB4,
	11,
	0x7FFF,
	0x4,
	17,
	0x30010000,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0x7f010800,
	20,
	0xFFFFFFFF,
	0xFFFFFFFF,
};
#endif
