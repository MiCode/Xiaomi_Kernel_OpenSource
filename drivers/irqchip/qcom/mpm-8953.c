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

const struct mpm_pin mpm_msm8953_gic_chip_data[] = {
	{2, 216}, /* tsens_upper_lower_int */
	{37, 252}, /* qmp_usb3_lfps_rxterm_irq -> ss_phy_irq */
	{49, 168}, /* qusb2phy_dpse_hv -> hs_phy_irq*/
	{53, 104}, /* mdss_irq */
	{58, 168}, /* qusb2phy_dmse_hv -> hs_phy_irq*/
	{88, 222}, /* ee0_krait_hlos_spmi_periph_irq */
	{-1},
};

const struct mpm_pin mpm_msm8953_gpio_chip_data[] = {
	{3, 38},
	{4, 1},
	{5, 5},
	{6, 9},
	{8, 37},
	{9, 36},
	{10, 13},
	{11, 35},
	{12, 17},
	{13, 21},
	{14, 54},
	{15, 34},
	{16, 31},
	{17, 58},
	{18, 28},
	{19, 42},
	{20, 25},
	{21, 12},
	{22, 43},
	{23, 44},
	{24, 45},
	{25, 46},
	{26, 48},
	{27, 65},
	{28, 93},
	{29, 97},
	{30, 63},
	{31, 70},
	{32, 71},
	{33, 72},
	{34, 81},
	{35, 85},
	{36, 90},
	{50, 67},
	{51, 73},
	{52, 74},
	{53, 62},
	{59, 59},
	{60, 60},
	{61, 61},
	{62, 86},
	{63, 87},
	{64, 91},
	{65, 129},
	{66, 130},
	{67, 131},
	{68, 132},
	{69, 133},
	{70, 137},
	{71, 138},
	{72, 139},
	{73, 140},
	{74, 141},
	{-1},
};
