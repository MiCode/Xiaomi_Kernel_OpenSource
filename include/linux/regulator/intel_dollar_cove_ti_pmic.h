/*
 * intel_dollar_cove_ti_pmic.h - Support for dollar cove TI pmic
 * Copyright (c) 2015, Intel Corporation.
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
#ifndef __INTEL_DCOVETI_REGULATOR_H_
#define __INTEL_DCOVETI_REGULATOR_H_

#include <linux/regulator/driver.h>

enum {
	DCOVETI_ID_BUCK3 = 1,
	DCOVETI_ID_BUCK4,
	DCOVETI_ID_BUCK5,
	DCOVETI_ID_BUCK6,

	DCOVETI_ID_LDO1,
	DCOVETI_ID_LDO2,
	DCOVETI_ID_LDO3,
	DCOVETI_ID_LDO5,
	DCOVETI_ID_LDO6,
	DCOVETI_ID_LDO7,
	DCOVETI_ID_LDO8,
	DCOVETI_ID_LDO9,
	DCOVETI_ID_LDO10,
	DCOVETI_ID_LDO11,
	DCOVETI_ID_LDO12,
	DCOVETI_ID_LDO13,
	DCOVETI_ID_LDO14,

	DCOVETI_ID_MAX,
};

struct dcoveti_regulator_info {
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
