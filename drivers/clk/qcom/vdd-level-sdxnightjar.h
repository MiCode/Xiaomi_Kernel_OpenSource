/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef __DRIVERS_CLK_QCOM_VDD_LEVEL_SDXNIGHTJAR_H
#define __DRIVERS_CLK_QCOM_VDD_LEVEL_SDXNIGHTJAR_H

#include <dt-bindings/regulator/qcom,rpm-smd-regulator.h>

enum vdd_dig_levels {
	VDD_NONE,
	VDD_MIN,		/* MIN SVS */
	VDD_LOWER,		/* SVS2 */
	VDD_LOW,		/* SVS */
	VDD_NOMINAL,		/* NOM */
	VDD_HIGH,		/* TURBO */
	VDD_NUM
};

static int vdd_corner[] = {
	RPM_SMD_REGULATOR_LEVEL_NONE,		/* VDD_DIG_NONE */
	RPM_SMD_REGULATOR_LEVEL_MIN_SVS,	/* VDD_DIG_MIN */
	RPM_SMD_REGULATOR_LEVEL_LOW_SVS,	/* VDD_DIG_LOWER */
	RPM_SMD_REGULATOR_LEVEL_SVS,		/* VDD_DIG_LOW */
	RPM_SMD_REGULATOR_LEVEL_NOM,		/* VDD_DIG_NOMINAL */
	RPM_SMD_REGULATOR_LEVEL_TURBO,		/* VDD_DIG_HIGH */
};

#endif

