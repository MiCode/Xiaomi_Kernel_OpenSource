/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2021, The Linux Foundation. All rights reserved. */

#ifndef __DRIVERS_CLK_QCOM_VDD_LEVEL_404_H
#define __DRIVERS_CLK_QCOM_VDD_LEVEL_404_H

#include <linux/regulator/consumer.h>
#include <dt-bindings/regulator/qcom,rpm-smd-regulator.h>

enum vdd_dig_levels {
	VDD_NONE,
	VDD_MIN,		/* MIN SVS */
	VDD_LOWER,		/* SVS2 */
	VDD_LOW,		/* SVS */
	VDD_LOW_L1,		/* SVSL1 */
	VDD_NOMINAL,		/* NOM */
	VDD_NOMINAL_L1,		/* NOM */
	VDD_HIGH,		/* TURBO */
	VDD_NUM,
};

static int vdd_corner[] = {
	[VDD_NONE]       = 0,					/* VDD_NONE */
	[VDD_MIN]        = RPM_SMD_REGULATOR_LEVEL_MIN_SVS,	/* VDD_MIN */
	[VDD_LOWER]      = RPM_SMD_REGULATOR_LEVEL_LOW_SVS,	/* VDD_LOWER */
	[VDD_LOW]        = RPM_SMD_REGULATOR_LEVEL_SVS,		/* VDD_LOW */
	[VDD_LOW_L1]     = RPM_SMD_REGULATOR_LEVEL_SVS_PLUS,	/* VDD_LOW_L1 */
	[VDD_NOMINAL]    = RPM_SMD_REGULATOR_LEVEL_NOM,		/* VDD_NOMINAL */
	[VDD_NOMINAL_L1] = RPM_SMD_REGULATOR_LEVEL_NOM_PLUS,	/* VDD_NOMINAL */
	[VDD_HIGH]       = RPM_SMD_REGULATOR_LEVEL_TURBO,	/* VDD_HIGH */
};

enum vdd_sr_pll_levels {
	VDD_SR_PLL_OFF,
	VDD_SR_PLL_SVS,
	VDD_SR_PLL_NOM,
	VDD_SR_PLL_TUR,
	VDD_SR_PLL_NUM,
};

static int vdd_sr_levels[] = {
	[VDD_SR_PLL_OFF] = 0,		/* VDD_SR_PLL_OFF */
	[VDD_SR_PLL_SVS] = 976000,	/* VDD_SR_PLL_SVS */
	[VDD_SR_PLL_NOM] = 976000,	/* VDD_SR_PLL_NOM */
	[VDD_SR_PLL_TUR] = 976000,	/* VDD_SR_PLL_TUR */
};

#endif
