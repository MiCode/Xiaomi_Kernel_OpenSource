/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef __DRIVERS_CLK_QCOM_VDD_LEVEL_LITO_H
#define __DRIVERS_CLK_QCOM_VDD_LEVEL_LITO_H

#include <linux/regulator/consumer.h>
#include <dt-bindings/regulator/qcom,rpmh-regulator-levels.h>

enum vdd_levels {
	VDD_NONE,
	VDD_MIN,		/* MIN SVS */
	VDD_LOWER,		/* SVS2 */
	VDD_LOW,		/* SVS */
	VDD_LOW_L1,		/* SVSL1 */
	VDD_NOMINAL,		/* NOM */
	VDD_HIGH,		/* TURBO */
	VDD_HIGH_L1,		/* TURBO_L1 */
	VDD_NUM,
};

static int vdd_corner[] = {
	[VDD_NONE]    = 0,
	[VDD_MIN]     = RPMH_REGULATOR_LEVEL_MIN_SVS,
	[VDD_LOWER]   = RPMH_REGULATOR_LEVEL_LOW_SVS,
	[VDD_LOW]     = RPMH_REGULATOR_LEVEL_SVS,
	[VDD_LOW_L1]  = RPMH_REGULATOR_LEVEL_SVS_L1,
	[VDD_NOMINAL] = RPMH_REGULATOR_LEVEL_NOM,
	[VDD_HIGH]    = RPMH_REGULATOR_LEVEL_TURBO,
	[VDD_HIGH_L1] = RPMH_REGULATOR_LEVEL_TURBO_L1,
};

enum vdd_gx_levels {
	VDD_GX_NONE,
	VDD_GX_MIN,		/* MIN SVS */
	VDD_GX_LOWER,		/* SVS2 */
	VDD_GX_LOW,		/* SVS */
	VDD_GX_LOW_L1,		/* SVSL1 */
	VDD_GX_NOMINAL,		/* NOM */
	VDD_GX_NOMINAL_L1,	/* NOM1 */
	VDD_GX_HIGH,		/* TURBO */
	VDD_GX_HIGH_L1,		/* TURBO1 */
	VDD_GX_NUM,
};

static int vdd_gx_corner[] = {
	[VDD_GX_NONE]    = 0,
	[VDD_GX_MIN]     = RPMH_REGULATOR_LEVEL_MIN_SVS,
	[VDD_GX_LOWER]   = RPMH_REGULATOR_LEVEL_LOW_SVS,
	[VDD_GX_LOW]     = RPMH_REGULATOR_LEVEL_SVS,
	[VDD_GX_LOW_L1]  = RPMH_REGULATOR_LEVEL_SVS_L1,
	[VDD_GX_NOMINAL] = RPMH_REGULATOR_LEVEL_NOM,
	[VDD_GX_NOMINAL_L1] = RPMH_REGULATOR_LEVEL_NOM_L1,
	[VDD_GX_HIGH]    = RPMH_REGULATOR_LEVEL_TURBO,
	[VDD_GX_HIGH_L1]    = RPMH_REGULATOR_LEVEL_TURBO_L1,
};

#endif
