/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#ifndef __DRIVERS_CLK_QCOM_VDD_LEVEL_TRINKET_H
#define __DRIVERS_CLK_QCOM_VDD_LEVEL_TRINKET_H

#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/regulator/consumer.h>

enum vdd_mx_levels {
	VDD_MX_NONE,
	VDD_MX_MIN,		/* MIN SVS */
	VDD_MX_LOWER,		/* SVS2 */
	VDD_MX_LOW,		/* SVS */
	VDD_MX_LOW_L1,		/* SVSL1 */
	VDD_MX_NOMINAL,		/* NOM */
	VDD_MX_HIGH,		/* TURBO */
	VDD_MX_HIGH_L1,		/* TURBO_L1 */
	VDD_MX_NUM,
};

static int vdd_mx_corner[] = {
	RPM_REGULATOR_LEVEL_NONE,		/* VDD_NONE */
	RPM_REGULATOR_LEVEL_MIN_SVS,		/* VDD_MIN */
	RPM_REGULATOR_LEVEL_LOW_SVS,		/* VDD_LOWER */
	RPM_REGULATOR_LEVEL_SVS,		/* VDD_LOW */
	RPM_REGULATOR_LEVEL_SVS_PLUS,		/* VDD_LOW_L1 */
	RPM_REGULATOR_LEVEL_NOM,		/* VDD_NOMINAL */
	RPM_REGULATOR_LEVEL_TURBO,		/* VDD_HIGH */
	RPM_REGULATOR_LEVEL_TURBO_NO_CPR,		/* VDD_HIGH_L1 */
	RPM_REGULATOR_LEVEL_MAX
};

enum vdd_dig_levels {
	VDD_NONE,
	VDD_MIN,		/* MIN SVS */
	VDD_LOWER,		/* SVS2 */
	VDD_LOW,		/* SVS */
	VDD_LOW_L1,		/* SVSL1 */
	VDD_NOMINAL,		/* NOM */
	VDD_NOMINAL_L1,		/* NOM */
	VDD_HIGH,		/* TURBO */
	VDD_HIGH_L1,		/* TURBO_L1 */
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
	RPM_REGULATOR_LEVEL_TURBO_NO_CPR,		/* VDD_HIGH_L1 */
	RPM_REGULATOR_LEVEL_MAX
};

#endif
