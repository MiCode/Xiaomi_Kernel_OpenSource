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

const struct mpm_pin mpm_msm8909_gic_chip_data[] = {
	{2, 216}, /* tsens_upper_lower_int */
	{48, 168}, /* usb_phy_id_irq */
	{49, 172}, /* usb1_hs_async_wakeup_irq */
	{58, 166}, /* usb_hs_irq */
	{53, 104}, /* mdss_irq */
	{62, 222}, /* ee0_krait_hlos_spmi_periph_irq */
	{-1},
};

const struct mpm_pin mpm_msm8909_gpio_chip_data[] = {
	{3, 65 },
	{4, 5},
	{5, 11},
	{6, 12},
	{7, 64},
	{8, 58},
	{9, 50},
	{10, 13},
	{11, 49},
	{12, 20},
	{13, 21},
	{14, 25},
	{15, 46},
	{16, 45},
	{17, 28},
	{18, 44},
	{19, 31},
	{20, 43},
	{21, 42},
	{22, 34},
	{23, 35},
	{24, 36},
	{25, 37},
	{26, 38},
	{27, 39},
	{28, 40},
	{29, 41},
	{30, 90},
	{32, 91},
	{33, 92},
	{34, 94},
	{35, 95},
	{36, 96},
	{37, 97},
	{38, 98},
	{39, 110},
	{40, 111},
	{41, 112},
	{42, 105},
	{43, 107},
	{50, 47},
	{51, 48},
	{-1},
};
