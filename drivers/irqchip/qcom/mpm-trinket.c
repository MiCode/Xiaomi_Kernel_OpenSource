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
 *
 */

#include "mpm.h"

const struct mpm_pin mpm_trinket_gic_chip_data[] = {
	{2, 222},
	{12, 454}, /* b3_lfps_rxterm_irq */
	{86, 215}, /* mpm_wake,spmi_m */
	{90, 292}, /* eud_p0_dpse_int_mx */
	{91, 292}, /* eud_p0_dmse_int_mx */
	{-1},
};

const struct mpm_pin mpm_trinket_gpio_chip_data[] = {
	{5, 43},
	{6, 45},
	{7, 59},
	{8, 72},
	{9, 83},
	{13, 124},
	{14, 1},
	{15, 3},
	{16, 4},
	{17, 9},
	{18, 13},
	{19, 15},
	{20, 17},
	{21, 19},
	{22, 21},
	{23, 14},
	{24, 25},
	{25, 26},
	{26, 27},
	{27, 29},
	{28, 33},
	{29, 36},
	{30, 42},
	{31, 44},
	{32, 47},
	{33, 50},
	{34, 70},
	{35, 75},
	{36, 79},
	{37, 80},
	{38, 81},
	{39, 82},
	{40, 85},
	{41, 86},
	{42, 88},
	{43, 89},
	{44, 91},
	{45, 92},
	{46, 93},
	{47, 94},
	{48, 95},
	{49, 96},
	{50, 98},
	{51, 99},
	{52, 101},
	{53, 102},
	{54, 105},
	{55, 107},
	{56, 110},
	{57, 111},
	{58, 112},
	{59, 118},
	{60, 122},
	{61, 123},
	{62, 126},
	{63, 128},
	{64, 100},
	{65, 130},
	{66, 131},
	{67, 132},
	{70, 97},
	{71, 120},
	{84, 22},
	{-1},
};
