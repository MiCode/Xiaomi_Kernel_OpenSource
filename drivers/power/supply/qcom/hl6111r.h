/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
 */

#ifndef __HL6111R_H__
#define __HL6111R_H__

/* Register definitions */

#define LATCHED_STATUS_REG	0x00
#define OUT_EN_L_BIT		BIT(0)

#define VRECT_REG		0x01

#define IRECT_REG		0x02

#define DIE_TEMP_REG		0x03

#define VOUT_TARGET_REG		0x0E

#define ID_REG			0xA

#define IOUT_LIM_SEL_REG	0x28
#define IOUT_LIM_SEL_MASK	GENMASK(7, 3)
#define IOUT_LIM_SHIFT		3

#define VOUT_RANGE_SEL_REG	0x30
#define VOUT_RANGE_SEL_MASK	GENMASK(7, 6)
#define VOUT_RANGE_SEL_SHIFT	6

#define IOUT_NOW_REG		0x82
#define IOUT_NOW_STEP_UA	9180

#define VOUT_STEP_UV		93750

#define VOUT_NOW_REG		0x83

#define VOUT_AVG_REG		0x8E

#define IOUT_AVG_REG		0x8F
#define IOUT_AVG_STEP_UA	9171

/* Macros for internal use */

#define VRECT_SCALED_UV(raw)	(raw * VOUT_STEP_UV)

#define IRECT_SCALED_UA(raw)	(raw * 1000 * 13)

/*
 * die_temp in deg C = (220.09 - raw) / 0.6316
 *		     = (2200900 - (raw * 10000)) / 6316
 */
#define DIE_TEMP_SCALED_DEG_C(raw)	((2200900 - (raw * 10000)) / 6316)

#endif
