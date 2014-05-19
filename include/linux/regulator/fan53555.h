/*
 * fan53555.h - Fairchild Regulator FAN53555 Driver
 *
 * Copyright (C) 2012 Marvell Technology Ltd.
 * Yunfan Zhang <yfzhang@marvell.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __FAN53555_H__

/* VSEL ID */
enum {
	FAN53555_VSEL_ID_0 = 0,
	FAN53555_VSEL_ID_1,
};

/* Transition slew rate limiting from a low to high voltage.
 * The slew rate values listed in this table are based on
 * 10 mV voltage scaling step. The actual slew rate has a
 * relationship between the slew rate based on 10 mV as its
 * voltage scaling step size and the actual voltage step size.
 * The formula for calculating the actual slew rate is:
 * actual_slew_rate = 10mv_based_slew_rate * scaling_step_size_mv / 10mV.
 * -----------------------
 *   Bin |Slew Rate(mV/uS)
 * ------|----------------
 *   000 |    64.00
 * ------|----------------
 *   001 |    32.00
 * ------|----------------
 *   010 |    16.00
 * ------|----------------
 *   011 |     8.00
 * ------|----------------
 *   100 |     4.00
 * ------|----------------
 *   101 |     2.00
 * ------|----------------
 *   110 |     1.00
 * ------|----------------
 *   111 |     0.50
 * -----------------------
 */
enum {
	FAN53555_SLEW_RATE_64MV = 0,
	FAN53555_SLEW_RATE_32MV,
	FAN53555_SLEW_RATE_16MV,
	FAN53555_SLEW_RATE_8MV,
	FAN53555_SLEW_RATE_4MV,
	FAN53555_SLEW_RATE_2MV,
	FAN53555_SLEW_RATE_1MV,
	FAN53555_SLEW_RATE_0_5MV,
};

struct fan53555_platform_data {
	struct regulator_init_data *regulator;
	unsigned int slew_rate;
	/* Sleep VSEL ID */
	unsigned int sleep_vsel_id;
};

#ifdef CONFIG_REGULATOR_FAN53555
int __init fan53555_regulator_init(void);
#else
static inline int __init fan53555_regulator_init(void) { return 0; }
#endif

#endif /* __FAN53555_H__ */
