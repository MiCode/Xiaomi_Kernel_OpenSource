/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef __DRIVERS_CLK_QCOM_VDD_LEVEL_660_H
#define __DRIVERS_CLK_QCOM_VDD_LEVEL_660_H

#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/regulator/consumer.h>

#define VDD_DIG_FMAX_MAP1(l1, f1) \
	.vdd_class = &vdd_dig,			\
	.rate_max = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
	},					\
	.num_rate_max = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP2(l1, f1, l2, f2) \
	.vdd_class = &vdd_dig,			\
	.rate_max = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
	},					\
	.num_rate_max = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP3(l1, f1, l2, f2, l3, f3) \
	.vdd_class = &vdd_dig,			\
	.rate_max = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
		[VDD_DIG_##l3] = (f3),		\
	},					\
	.num_rate_max = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP4(l1, f1, l2, f2, l3, f3, l4, f4) \
	.vdd_class = &vdd_dig,			\
	.rate_max = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
		[VDD_DIG_##l3] = (f3),		\
		[VDD_DIG_##l4] = (f4),		\
	},					\
	.num_rate_max = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP5(l1, f1, l2, f2, l3, f3, l4, f4, l5, f5) \
	.vdd_class = &vdd_dig,			\
	.rate_max = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
		[VDD_DIG_##l3] = (f3),		\
		[VDD_DIG_##l4] = (f4),		\
		[VDD_DIG_##l5] = (f5),		\
	},					\
	.num_rate_max = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP6(l1, f1, l2, f2, l3, f3, l4, f4, l5, f5, l6, f6) \
	.vdd_class = &vdd_dig,			\
	.rate_max = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
		[VDD_DIG_##l3] = (f3),		\
		[VDD_DIG_##l4] = (f4),		\
		[VDD_DIG_##l5] = (f5),		\
		[VDD_DIG_##l6] = (f6),		\
	},					\
	.num_rate_max = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP7(l1, f1, l2, f2, l3, f3, l4, f4, l5, f5, l6, f6, \
				l7, f7)		\
	.vdd_class = &vdd_dig,			\
	.rate_max = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
		[VDD_DIG_##l3] = (f3),		\
		[VDD_DIG_##l4] = (f4),		\
		[VDD_DIG_##l5] = (f5),		\
		[VDD_DIG_##l6] = (f6),		\
		[VDD_DIG_##l7] = (f7),		\
	},					\
	.num_rate_max = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP1_AO(l1, f1)		 \
	.vdd_class = &vdd_dig_ao,		\
	.rate_max = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
	},					\
	.num_rate_max = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP3_AO(l1, f1, l2, f2, l3, f3) \
	.vdd_class = &vdd_dig_ao,			\
	.rate_max = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
		[VDD_DIG_##l3] = (f3),		\
	},					\
	.num_rate_max = VDD_DIG_NUM

#define VDD_GPU_PLL_FMAX_MAP1(l1, f1)			 \
	.vdd_class = &vdd_mx,				\
	.rate_max = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),			\
	},						\
	.num_rate_max = VDD_DIG_NUM

#define VDD_MMSS_PLL_DIG_FMAX_MAP1(l1, f1) \
	.vdd_class = &vdd_mx,			\
	.rate_max = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
	},					\
	.num_rate_max = VDD_DIG_NUM

#define VDD_MMSS_PLL_DIG_FMAX_MAP2(l1, f1, l2, f2) \
	.vdd_class = &vdd_mx,			\
	.rate_max = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
	},					\
	.num_rate_max = VDD_DIG_NUM

enum vdd_dig_levels {
	VDD_DIG_NONE,
	VDD_DIG_MIN,		/* MIN SVS */
	VDD_DIG_LOWER,		/* SVS2 */
	VDD_DIG_LOW,		/* SVS */
	VDD_DIG_LOW_L1,		/* SVSL1 */
	VDD_DIG_NOMINAL,	/* NOM */
	VDD_DIG_NOMINAL_L1,	/* NOM */
	VDD_DIG_HIGH,		/* TURBO */
	VDD_DIG_NUM
};

static int vdd_corner[] = {
	RPM_REGULATOR_LEVEL_NONE,		/* VDD_DIG_NONE */
	RPM_REGULATOR_LEVEL_MIN_SVS,		/* VDD_DIG_MIN */
	RPM_REGULATOR_LEVEL_LOW_SVS,		/* VDD_DIG_LOWER */
	RPM_REGULATOR_LEVEL_SVS,		/* VDD_DIG_LOW */
	RPM_REGULATOR_LEVEL_SVS_PLUS,		/* VDD_DIG_LOW_L1 */
	RPM_REGULATOR_LEVEL_NOM,		/* VDD_DIG_NOMINAL */
	RPM_REGULATOR_LEVEL_NOM_PLUS,		/* VDD_DIG_NOMINAL */
	RPM_REGULATOR_LEVEL_TURBO,		/* VDD_DIG_HIGH */
};

#endif
