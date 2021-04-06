/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __DRIVERS_CLK_QCOM_VDD_LEVEL_H
#define __DRIVERS_CLK_QCOM_VDD_LEVEL_H

#include <linux/regulator/consumer.h>
#include <dt-bindings/regulator/qcom,rpm-smd-regulator.h>

enum vdd_levels {
	VDD_NONE,
	VDD_LOWER_D1,		/* MIN SVS */
	VDD_LOWER,		/* SVS2 */
	VDD_LOW,		/* SVS */
	VDD_LOW_L1,		/* SVSL1 */
	VDD_NOMINAL,		/* NOM */
	VDD_NOMINAL_L1,		/* NOM L1 */
	VDD_HIGH,
	VDD_HIGH_L1,		/* TURBO L1*/
	VDD_NUM,
};

static int vdd_corner[] = {
	[VDD_NONE]    = 0,
	[VDD_LOWER_D1]     = RPM_SMD_REGULATOR_LEVEL_MIN_SVS,
	[VDD_LOWER]   = RPM_SMD_REGULATOR_LEVEL_LOW_SVS,
	[VDD_LOW]     = RPM_SMD_REGULATOR_LEVEL_SVS,
	[VDD_LOW_L1]  = RPM_SMD_REGULATOR_LEVEL_SVS_PLUS,
	[VDD_NOMINAL] = RPM_SMD_REGULATOR_LEVEL_NOM,
	[VDD_NOMINAL_L1] = RPM_SMD_REGULATOR_LEVEL_NOM_PLUS,
	[VDD_HIGH]    = RPM_SMD_REGULATOR_LEVEL_TURBO,
	[VDD_HIGH_L1]    = RPM_SMD_REGULATOR_LEVEL_TURBO_NO_CPR,
};

#endif
