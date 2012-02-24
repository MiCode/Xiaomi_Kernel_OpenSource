/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef __STUB_REGULATOR_H__
#define __STUB_REGULATOR_H__

#include <linux/regulator/machine.h>

#define STUB_REGULATOR_DRIVER_NAME "stub-regulator"

/**
 * struct stub_regulator_pdata - stub regulator device data
 * @init_data:		regulator constraints
 * @hpm_min_load:	minimum load in uA that will result in the regulator
 *			being set to high power mode
 * @system_uA:		current drawn from regulator not accounted for by any
 *			regulator framework consumer
 */
struct stub_regulator_pdata {
	struct regulator_init_data	init_data;
	int				hpm_min_load;
	int				system_uA;
};

int __init regulator_stub_init(void);
#endif
