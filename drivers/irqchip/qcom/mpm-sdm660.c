/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

const struct mpm_pin mpm_sdm660_gic_chip_data[] = {
	{2, 216}, /* tsens1_tsens_upper_lower_int */
	{52, 275}, /* qmp_usb3_lfps_rxterm_irq_cx */
	{61, 209}, /* lpi_dir_conn_irq_apps[1] */
	{79, 379}, /* qusb2phy_intr for Dm */
	{80, 380}, /* qusb2phy_intr for Dm for secondary PHY */
	{81, 379}, /* qusb2phy_intr for Dp */
	{82, 380}, /* qusb2phy_intr for Dp for secondary PHY */
	{87, 358}, /* ee0_apps_hlos_spmi_periph_irq */
	{91, 519}, /* lpass_pmu_tmr_timeout_irq_cx */
	{-1},
};

const struct mpm_pin mpm_sdm660_gpio_chip_data[] = {
	{3, 1},
	{4, 5},
	{5, 9},
	{6, 10},
	{7, 66},
	{8, 22},
	{9, 25},
	{10, 28},
	{11, 58},
	{13, 41},
	{14, 43},
	{15, 40},
	{16, 42},
	{17, 46},
	{18, 50},
	{19, 44},
	{21, 56},
	{22, 45},
	{23, 68},
	{24, 69},
	{25, 70},
	{26, 71},
	{27, 72},
	{28, 73},
	{29, 64},
	{30, 2},
	{31, 13},
	{32, 111},
	{33, 74},
	{34, 75},
	{35, 76},
	{36, 82},
	{37, 17},
	{38, 77},
	{39, 47},
	{40, 54},
	{41, 48},
	{42, 101},
	{43, 49},
	{44, 51},
	{45, 86},
	{46, 90},
	{47, 91},
	{48, 52},
	{50, 55},
	{51, 6},
	{53, 65},
	{55, 67},
	{56, 83},
	{57, 84},
	{58, 85},
	{59, 87},
	{63, 21},
	{64, 78},
	{65, 113},
	{66, 60},
	{67, 98},
	{68, 30},
	{70, 31},
	{71, 29},
	{76, 107},
	{83, 109},
	{84, 103},
	{85, 105},
	{-1},
};
