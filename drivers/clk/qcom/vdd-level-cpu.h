/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __DRIVERS_CLK_QCOM_VDD_LEVEL_CPU_H
#define __DRIVERS_CLK_QCOM_VDD_LEVEL_CPU_H

#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/regulator/consumer.h>

enum vdd_hf_pll_levels {
	VDD_HF_PLL_OFF,
	VDD_HF_PLL_SVS,
	VDD_HF_PLL_NOM,
	VDD_HF_PLL_TUR,
	VDD_HF_PLL_NUM,
};

static int vdd_hf_levels[] = {
	0,	 RPM_REGULATOR_LEVEL_NONE,	/* VDD_HF_PLL_OFF */
	1800000, RPM_REGULATOR_LEVEL_SVS,	/* VDD_HF_PLL_SVS */
	1800000, RPM_REGULATOR_LEVEL_NOM,	/* VDD_HF_PLL_NOM */
	1800000, RPM_REGULATOR_LEVEL_TURBO,	/* VDD_HF_PLL_TUR */
};

#endif
