/* Copyright (c) 2018, 2019 The Linux Foundation. All rights reserved.
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

const struct mpm_pin mpm_mdm9607_gic_chip_data[] = {
	{2, 216}, /* tsens_upper_lower_int */
	{49, 172}, /* usb1_hs_async_wakeup_irq */
	{51, 174}, /* usb2_hs_async_wakeup_irq */
	{53, 104}, /* mdss_irq */
	{58, 166}, /* usb_hs_irq */
	{62, 222}, /* ee0_apps_hlos_spmi_periph_irq */
	{-1},
};

const struct mpm_pin mpm_mdm9607_gpio_chip_data[] = {
	{3, 16},
	{4, 5},
	{5, 11},
	{6, 12},
	{7, 3},
	{8, 17},
	{9, 9},
	{10, 13},
	{11, 1},
	{12, 20},
	{13, 21},
	{14, 22},
	{15, 75},
	{16, 74},
	{17, 28},
	{18, 44},
	{19, 26},
	{20, 43},
	{21, 42},
	{22, 29},
	{23, 69},
	{24, 30},
	{25, 37},
	{26, 25},
	{27, 71},
	{28, 34},
	{29, 55},
	{30, 8},
	{31, 40},
	{32, 48},
	{33, 52},
	{34, 57},
	{35, 62},
	{36, 66},
	{37, 59},
	{38, 79},
	{39, 38},
	{40, 63},
	{41, 76},
	{-1},
};
