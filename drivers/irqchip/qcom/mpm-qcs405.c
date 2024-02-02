/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "mpm.h"

const struct mpm_pin mpm_qcs405_gic_chip_data[] = {
	{2, 216},
	{35, 350}, /* dmse_hv, usb20 -> hs_phy_irq */
	{36, 350}, /* dpse_hv, usb20 -> hs_phy_irq */
	{38, 351}, /* dmse_hv, usb30 -> hs_phy_irq */
	{39, 351}, /* dpse_hv, usb30 -> hs_phy_irq */
	{62, 222}, /* mpm_wake,spmi_m */
	{-1},
};

const struct mpm_pin mpm_qcs405_gpio_chip_data[] = {
	{3, 4},
	{4, 6},
	{5, 14},
	{6, 18},
	{7, 117},
	{8, 19},
	{9, 20},
	{10, 21},
	{11, 22},
	{12, 23},
	{13, 24},
	{14, 27},
	{15, 28},
	{16, 31},
	{17, 32},
	{18, 34},
	{19, 35},
	{20, 37},
	{21, 38},
	{22, 59},
	{23, 61},
	{24, 62},
	{25, 77},
	{26, 78},
	{27, 79},
	{28, 80},
	{29, 81},
	{30, 83},
	{31, 84},
	{32, 88},
	{33, 89},
	{34, 99},
	{42, 100},
	{43, 104},
	{47, 105},
	{48, 106},
	{49, 107},
	{52, 109},
	{53, 110},
	{54, 111},
	{55, 112},
	{56, 113},
	{57, 114},
	{58, 115},
	{67, 53},
	{68, 54},
	{-1},
};
