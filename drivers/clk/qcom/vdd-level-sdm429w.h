/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

#ifndef __DRIVERS_CLK_QCOM_VDD_LEVEL_SDM429W_H
#define __DRIVERS_CLK_QCOM_VDD_LEVEL_SDM429W_H

#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/regulator/consumer.h>

enum vdd_levels {
	VDD_NONE,
	VDD_LOW,
	VDD_LOW_L1,
	VDD_NOMINAL,
	VDD_NOMINAL_L1,
	VDD_HIGH,
	VDD_NUM
};

static int vdd_corner[] = {
	RPM_REGULATOR_LEVEL_NONE,		/* VDD_NONE */
	RPM_REGULATOR_LEVEL_SVS,		/* VDD_SVS */
	RPM_REGULATOR_LEVEL_SVS_PLUS,		/* VDD_SVS_PLUS */
	RPM_REGULATOR_LEVEL_NOM,		/* VDD_NOM */
	RPM_REGULATOR_LEVEL_NOM_PLUS,		/* VDD_NOM_PLUS */
	RPM_REGULATOR_LEVEL_TURBO,		/* VDD_TURBO */
};

#endif
