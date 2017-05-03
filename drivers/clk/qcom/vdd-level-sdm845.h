/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef __DRIVERS_CLK_QCOM_VDD_LEVEL_COBALT_H
#define __DRIVERS_CLK_QCOM_VDD_LEVEL_COBALT_H

#include <linux/regulator/consumer.h>
#include <dt-bindings/regulator/qcom,rpmh-regulator.h>

#define VDD_CX_FMAX_MAP1(l1, f1) \
	.vdd_class = &vdd_cx,			\
	.rate_max = (unsigned long[VDD_CX_NUM]) {	\
		[VDD_CX_##l1] = (f1),		\
	},					\
	.num_rate_max = VDD_CX_NUM

#define VDD_CX_FMAX_MAP2(l1, f1, l2, f2) \
	.vdd_class = &vdd_cx,			\
	.rate_max = (unsigned long[VDD_CX_NUM]) {	\
		[VDD_CX_##l1] = (f1),		\
		[VDD_CX_##l2] = (f2),		\
	},					\
	.num_rate_max = VDD_CX_NUM

#define VDD_CX_FMAX_MAP3(l1, f1, l2, f2, l3, f3) \
	.vdd_class = &vdd_cx,			\
	.rate_max = (unsigned long[VDD_CX_NUM]) {	\
		[VDD_CX_##l1] = (f1),		\
		[VDD_CX_##l2] = (f2),		\
		[VDD_CX_##l3] = (f3),		\
	},					\
	.num_rate_max = VDD_CX_NUM

#define VDD_CX_FMAX_MAP4(l1, f1, l2, f2, l3, f3, l4, f4) \
	.vdd_class = &vdd_cx,			\
	.rate_max = (unsigned long[VDD_CX_NUM]) {	\
		[VDD_CX_##l1] = (f1),		\
		[VDD_CX_##l2] = (f2),		\
		[VDD_CX_##l3] = (f3),		\
		[VDD_CX_##l4] = (f4),		\
	},					\
	.num_rate_max = VDD_CX_NUM

#define VDD_CX_FMAX_MAP5(l1, f1, l2, f2, l3, f3, l4, f4, l5, f5) \
	.vdd_class = &vdd_cx,			\
	.rate_max = (unsigned long[VDD_CX_NUM]) {	\
		[VDD_CX_##l1] = (f1),		\
		[VDD_CX_##l2] = (f2),		\
		[VDD_CX_##l3] = (f3),		\
		[VDD_CX_##l4] = (f4),		\
		[VDD_CX_##l5] = (f5),		\
	},					\
	.num_rate_max = VDD_CX_NUM

#define VDD_CX_FMAX_MAP6(l1, f1, l2, f2, l3, f3, l4, f4, l5, f5, l6, f6) \
	.vdd_class = &vdd_cx,			\
	.rate_max = (unsigned long[VDD_CX_NUM]) {	\
		[VDD_CX_##l1] = (f1),		\
		[VDD_CX_##l2] = (f2),		\
		[VDD_CX_##l3] = (f3),		\
		[VDD_CX_##l4] = (f4),		\
		[VDD_CX_##l5] = (f5),		\
		[VDD_CX_##l6] = (f6),		\
	},					\
	.num_rate_max = VDD_CX_NUM

#define VDD_CX_FMAX_MAP1_AO(l1, f1)		 \
	.vdd_class = &vdd_cx_ao,		\
	.rate_max = (unsigned long[VDD_CX_NUM]) {	\
		[VDD_CX_##l1] = (f1),		\
	},					\
	.num_rate_max = VDD_CX_NUM

#define VDD_CX_FMAX_MAP3_AO(l1, f1, l2, f2, l3, f3) \
	.vdd_class = &vdd_cx_ao,			\
	.rate_max = (unsigned long[VDD_CX_NUM]) {	\
		[VDD_CX_##l1] = (f1),		\
		[VDD_CX_##l2] = (f2),		\
		[VDD_CX_##l3] = (f3),		\
	},					\
	.num_rate_max = VDD_CX_NUM

#define VDD_MX_FMAX_MAP4(l1, f1, l2, f2, l3, f3, l4, f4) \
	.vdd_class = &vdd_mx,			\
	.rate_max = (unsigned long[VDD_CX_NUM]) {	\
		[VDD_CX_##l1] = (f1),		\
		[VDD_CX_##l2] = (f2),		\
		[VDD_CX_##l3] = (f3),		\
		[VDD_CX_##l4] = (f4),		\
	},					\
	.num_rate_max = VDD_CX_NUM

#define VDD_GX_FMAX_MAP8(l1, f1, l2, f2, l3, f3, l4, f4, l5, f5, l6, f6, \
				l7, f7, l8, f8) \
	.vdd_class = &vdd_gfx,			\
	.rate_max = (unsigned long[VDD_GX_NUM]) {	\
		[VDD_GX_##l1] = (f1),		\
		[VDD_GX_##l2] = (f2),		\
		[VDD_GX_##l3] = (f3),		\
		[VDD_GX_##l4] = (f4),		\
		[VDD_GX_##l5] = (f5),		\
		[VDD_GX_##l6] = (f6),		\
		[VDD_GX_##l7] = (f7),		\
		[VDD_GX_##l8] = (f8),		\
	},					\
	.num_rate_max = VDD_GX_NUM

enum vdd_cx_levels {
	VDD_CX_NONE,
	VDD_CX_MIN,		/* MIN SVS */
	VDD_CX_LOWER,		/* SVS2 */
	VDD_CX_LOW,		/* SVS */
	VDD_CX_LOW_L1,		/* SVSL1 */
	VDD_CX_NOMINAL,		/* NOM */
	VDD_CX_HIGH,		/* TURBO */
	VDD_CX_NUM,
};

enum vdd_gx_levels {
	VDD_GX_NONE,
	VDD_GX_MIN,		/* MIN SVS */
	VDD_GX_LOWER,		/* SVS2 */
	VDD_GX_LOW,		/* SVS */
	VDD_GX_LOW_L1,		/* SVSL1 */
	VDD_GX_NOMINAL,		/* NOM */
	VDD_GX_NOMINAL_L1,		/* NOM1 */
	VDD_GX_HIGH,		/* TURBO */
	VDD_GX_HIGH_L1,		/* TURBO1 */
	VDD_GX_NUM,
};

/* Need to use the correct VI/VL mappings */
static int vdd_corner[] = {
	RPMH_REGULATOR_LEVEL_OFF,		/* VDD_CX_NONE */
	RPMH_REGULATOR_LEVEL_MIN_SVS,		/* VDD_CX_MIN */
	RPMH_REGULATOR_LEVEL_LOW_SVS,		/* VDD_CX_LOWER */
	RPMH_REGULATOR_LEVEL_SVS,		/* VDD_CX_LOW */
	RPMH_REGULATOR_LEVEL_SVS_L1,		/* VDD_CX_LOW_L1 */
	RPMH_REGULATOR_LEVEL_NOM,		/* VDD_CX_NOMINAL */
	RPMH_REGULATOR_LEVEL_TURBO,		/* VDD_CX_HIGH */
};

#endif
