/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef __DRIVERS_CLK_QCOM_VDD_LEVEL_SDMMAGPIE_H
#define __DRIVERS_CLK_QCOM_VDD_LEVEL_SDMMAGPIE_H

#include <linux/regulator/consumer.h>
#include <dt-bindings/regulator/qcom,rpmh-regulator.h>

enum vdd_levels {
	VDD_NONE,
	VDD_MIN,		/* MIN SVS */
	VDD_LOWER,		/* SVS2 */
	VDD_LOW,		/* SVS */
	VDD_LOW_L1,		/* SVSL1 */
	VDD_NOMINAL,		/* NOM */
	VDD_HIGH,		/* TURBO */
	VDD_NUM,
};

static int vdd_corner[] = {
	RPMH_REGULATOR_LEVEL_OFF,		/* VDD_NONE */
	RPMH_REGULATOR_LEVEL_MIN_SVS,		/* VDD_MIN */
	RPMH_REGULATOR_LEVEL_LOW_SVS,		/* VDD_LOWER */
	RPMH_REGULATOR_LEVEL_SVS,		/* VDD_LOW */
	RPMH_REGULATOR_LEVEL_SVS_L1,		/* VDD_LOW_L1 */
	RPMH_REGULATOR_LEVEL_NOM,		/* VDD_NOMINAL */
	RPMH_REGULATOR_LEVEL_TURBO,		/* VDD_HIGH */
};

#endif
