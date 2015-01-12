/*
 * intel_dcovex_regulator.h - Support for dollar cove XB pmic
 * Copyright (c) 2013, Intel Corporation.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __INTEL_DCOVEX_REGULATOR_H_
#define __INTEL_DCOVEX_REGULATOR_H_

#include <linux/regulator/driver.h>

enum {
	DCOVEX_ID_BUCK1 = 1,
	DCOVEX_ID_BUCK2,
	DCOVEX_ID_BUCK3,
	DCOVEX_ID_BUCK4,
	DCOVEX_ID_BUCK5,
	DCOVEX_ID_BUCK6,

	DCOVEX_ID_LDO1,
	DCOVEX_ID_LDO2,
	DCOVEX_ID_LDO3,
	DCOVEX_ID_LDO4,
	DCOVEX_ID_LDO5,	/* ELDO1 */
	DCOVEX_ID_LDO6,
	DCOVEX_ID_LDO7,
	DCOVEX_ID_LDO8,	/* FLDO1 */
	DCOVEX_ID_LDO9,
	DCOVEX_ID_LDO10,
	DCOVEX_ID_LDO11,/* ALDO1 */
	DCOVEX_ID_LDO12,
	DCOVEX_ID_LDO13,

	DCOVEX_ID_GPIO1,

	DCOVEX_ID_MAX,
};

struct dcovex_regulator_info {
	struct regulator_desc	desc;
	struct regulator_dev	*regulator;
	struct regulator_init_data *init_data;
	int vol_reg;
	int vol_nbits;
	int vol_shift;
	int enable_reg;		/* enable register base  */
	int enable_bit;
};

#endif
