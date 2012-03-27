/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/regulator/stub-regulator.h>

#define VREG_CONSUMERS(_name) \
	static struct regulator_consumer_supply vreg_consumers_##_name[]

/*
 * Consumer specific regulator names:
 *			 regulator name		consumer dev_name
 */
VREG_CONSUMERS(K0) = {
	REGULATOR_SUPPLY("krait0",		NULL),
};
VREG_CONSUMERS(K1) = {
	REGULATOR_SUPPLY("krait1",		NULL),
};
VREG_CONSUMERS(K2) = {
	REGULATOR_SUPPLY("krait2",		NULL),
};
VREG_CONSUMERS(K3) = {
	REGULATOR_SUPPLY("krait3",		NULL),
};

#define PM8X41_VREG_INIT(_id, _name, _min_uV, _max_uV, _modes, _ops, \
			 _always_on, _supply_regulator, _hpm_min, _system_uA)  \
	struct stub_regulator_pdata vreg_dev_##_id##_pdata __devinitdata = { \
		.init_data = { \
			.constraints = { \
				.valid_modes_mask	= _modes, \
				.valid_ops_mask		= _ops, \
				.min_uV			= _min_uV, \
				.max_uV			= _max_uV, \
				.input_uV		= _max_uV, \
				.apply_uV		= 0,	\
				.always_on		= _always_on, \
				.name			= _name, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id), \
			.consumer_supplies	= vreg_consumers_##_id, \
			.supply_regulator	= _supply_regulator, \
		}, \
		.hpm_min_load		= _hpm_min, \
		.system_uA		= _system_uA, \
	}

#define KRAIT_PWR(_id, _name, _always_on, _min_uV, _max_uV, \
		_supply_regulator, _hpm_min, _system_uA) \
	PM8X41_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		| REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE | \
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		REGULATOR_CHANGE_DRMS, _always_on, \
		_supply_regulator, _hpm_min, _system_uA)

/*	 ID      name     a_on  min_uV   max_uV  supply  hpm_min sys_uA  */
KRAIT_PWR(K0, "krait0", 0, 850000,  1100000, NULL,     100000, 0);
KRAIT_PWR(K1, "krait1", 0, 850000,  1100000, NULL,     100000, 0);
KRAIT_PWR(K2, "krait2", 0, 850000,  1100000, NULL,     100000, 0);
KRAIT_PWR(K3, "krait3", 0, 850000,  1100000, NULL,     100000, 0);

#define VREG_DEVICE(_name, _devid)					       \
		vreg_device_##_name __devinitdata =			       \
		{							       \
			.name = STUB_REGULATOR_DRIVER_NAME,		       \
			.id = _devid,					       \
			.dev = { .platform_data = &vreg_dev_##_name##_pdata }, \
		}

static struct platform_device VREG_DEVICE(K0, 0);
static struct platform_device VREG_DEVICE(K1, 1);
static struct platform_device VREG_DEVICE(K2, 2);
static struct platform_device VREG_DEVICE(K3, 3);

struct platform_device *msm_copper_stub_regulator_devices[] __devinitdata = {
	&vreg_device_K0,
	&vreg_device_K1,
	&vreg_device_K2,
	&vreg_device_K3,
};

int msm_copper_stub_regulator_devices_len __devinitdata =
			ARRAY_SIZE(msm_copper_stub_regulator_devices);
