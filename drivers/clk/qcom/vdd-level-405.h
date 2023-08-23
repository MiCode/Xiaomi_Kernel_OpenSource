/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#ifndef __DRIVERS_CLK_QCOM_VDD_LEVEL_405_H
#define __DRIVERS_CLK_QCOM_VDD_LEVEL_405_H

#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/regulator/consumer.h>

enum vdd_dig_levels {
	VDD_NONE,
	VDD_MIN,		/* MIN SVS */
	VDD_LOWER,		/* SVS2 */
	VDD_LOW,		/* SVS */
	VDD_LOW_L1,		/* SVSL1 */
	VDD_NOMINAL,	/* NOM */
	VDD_NOMINAL_L1,	/* NOM */
	VDD_HIGH,		/* TURBO */
	VDD_NUM,
};

static int vdd_corner[] = {
	RPM_REGULATOR_LEVEL_NONE,		/* VDD_NONE */
	RPM_REGULATOR_LEVEL_MIN_SVS,		/* VDD_MIN */
	RPM_REGULATOR_LEVEL_LOW_SVS,		/* VDD_LOWER */
	RPM_REGULATOR_LEVEL_SVS,		/* VDD_LOW */
	RPM_REGULATOR_LEVEL_SVS_PLUS,		/* VDD_LOW_L1 */
	RPM_REGULATOR_LEVEL_NOM,		/* VDD_NOMINAL */
	RPM_REGULATOR_LEVEL_NOM_PLUS,		/* VDD_NOMINAL */
	RPM_REGULATOR_LEVEL_TURBO,		/* VDD_HIGH */
};

enum vdd_hf_pll_levels {
	VDD_HF_PLL_OFF,
	VDD_HF_PLL_SVS,
	VDD_HF_PLL_NOM,
	VDD_HF_PLL_TUR,
	VDD_HF_PLL_NUM,
};

static int vdd_hf_levels[] = {
	0,       RPM_REGULATOR_LEVEL_NONE,	/* VDD_HF_PLL_OFF */
	1800000, RPM_REGULATOR_LEVEL_SVS,	/* VDD_HF_PLL_SVS */
	1800000, RPM_REGULATOR_LEVEL_NOM,	/* VDD_HF_PLL_NOM */
	1800000, RPM_REGULATOR_LEVEL_TURBO,	/* VDD_HF_PLL_TUR */
};

enum vdd_sr_pll_levels {
	VDD_SR_PLL_OFF,
	VDD_SR_PLL_SVS,
	VDD_SR_PLL_NOM,
	VDD_SR_PLL_TUR,
	VDD_SR_PLL_NUM,
};

static int vdd_sr_levels[] = {
	0,	/* VDD_SR_PLL_OFF */
	976000,	/* VDD_SR_PLL_SVS */
	976000,	/* VDD_SR_PLL_NOM */
	976000,	/* VDD_SR_PLL_TUR */
};

#endif
