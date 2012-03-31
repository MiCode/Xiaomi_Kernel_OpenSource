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
VREG_CONSUMERS(S1B) = {
	REGULATOR_SUPPLY("8841_s1",		NULL),
};
VREG_CONSUMERS(S2B) = {
	REGULATOR_SUPPLY("8841_s2",		NULL),
	REGULATOR_SUPPLY("HSUSB_VDDCX",		"msm_otg"),
};
VREG_CONSUMERS(S3B) = {
	REGULATOR_SUPPLY("8841_s3",		NULL),
};
VREG_CONSUMERS(S4B) = {
	REGULATOR_SUPPLY("8841_s4",		NULL),
};
VREG_CONSUMERS(S5B) = {
	REGULATOR_SUPPLY("8841_s5",		NULL),
};
VREG_CONSUMERS(S6B) = {
	REGULATOR_SUPPLY("8841_s6",		NULL),
};
VREG_CONSUMERS(S7B) = {
	REGULATOR_SUPPLY("8841_s7",		NULL),
};
VREG_CONSUMERS(S8B) = {
	REGULATOR_SUPPLY("8841_s8",		NULL),
};
VREG_CONSUMERS(S1) = {
	REGULATOR_SUPPLY("8941_s1",		NULL),
};
VREG_CONSUMERS(S2) = {
	REGULATOR_SUPPLY("8941_s2",		NULL),
};
VREG_CONSUMERS(S3) = {
	REGULATOR_SUPPLY("8941_s3",		NULL),
};
VREG_CONSUMERS(L1) = {
	REGULATOR_SUPPLY("8941_l1",		NULL),
};
VREG_CONSUMERS(L2) = {
	REGULATOR_SUPPLY("8941_l2",		NULL),
};
VREG_CONSUMERS(L3) = {
	REGULATOR_SUPPLY("8941_l3",		NULL),
};
VREG_CONSUMERS(L4) = {
	REGULATOR_SUPPLY("8941_l4",		NULL),
};
VREG_CONSUMERS(L5) = {
	REGULATOR_SUPPLY("8941_l5",		NULL),
};
VREG_CONSUMERS(L6) = {
	REGULATOR_SUPPLY("8941_l6",		NULL),
	REGULATOR_SUPPLY("HSUSB_1p8",		"msm_otg"),
};
VREG_CONSUMERS(L7) = {
	REGULATOR_SUPPLY("8941_l7",		NULL),
};
VREG_CONSUMERS(L8) = {
	REGULATOR_SUPPLY("8941_l8",		NULL),
};
VREG_CONSUMERS(L9) = {
	REGULATOR_SUPPLY("8941_l9",		NULL),
};
VREG_CONSUMERS(L10) = {
	REGULATOR_SUPPLY("8941_l10",		NULL),
};
VREG_CONSUMERS(L11) = {
	REGULATOR_SUPPLY("8941_l11",		NULL),
};
VREG_CONSUMERS(L12) = {
	REGULATOR_SUPPLY("8941_l12",		NULL),
};
VREG_CONSUMERS(L13) = {
	REGULATOR_SUPPLY("8941_l13",		NULL),
};
VREG_CONSUMERS(L14) = {
	REGULATOR_SUPPLY("8941_l14",		NULL),
};
VREG_CONSUMERS(L15) = {
	REGULATOR_SUPPLY("8941_l15",		NULL),
};
VREG_CONSUMERS(L16) = {
	REGULATOR_SUPPLY("8941_l16",		NULL),
};
VREG_CONSUMERS(L17) = {
	REGULATOR_SUPPLY("8941_l17",		NULL),
};
VREG_CONSUMERS(L18) = {
	REGULATOR_SUPPLY("8941_l18",		NULL),
};
VREG_CONSUMERS(L19) = {
	REGULATOR_SUPPLY("8941_l19",		NULL),
};
VREG_CONSUMERS(L20) = {
	REGULATOR_SUPPLY("8941_l20",		NULL),
};
VREG_CONSUMERS(L21) = {
	REGULATOR_SUPPLY("8941_l21",		NULL),
};
VREG_CONSUMERS(L22) = {
	REGULATOR_SUPPLY("8941_l22",		NULL),
};
VREG_CONSUMERS(L23) = {
	REGULATOR_SUPPLY("8941_l23",		NULL),
};
VREG_CONSUMERS(L24) = {
	REGULATOR_SUPPLY("8941_l24",		NULL),
	REGULATOR_SUPPLY("HSUSB_3p3",		"msm_otg"),
};
VREG_CONSUMERS(LVS1) = {
	REGULATOR_SUPPLY("8941_lvs1",		NULL),
};
VREG_CONSUMERS(LVS2) = {
	REGULATOR_SUPPLY("8941_lvs2",		NULL),
};
VREG_CONSUMERS(LVS3) = {
	REGULATOR_SUPPLY("8941_lvs3",		NULL),
};
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

#define PM8X41_LDO(_id, _name, _always_on, _min_uV, _max_uV, \
		_supply_regulator, _hpm_min, _system_uA) \
	PM8X41_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		| REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE | \
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		REGULATOR_CHANGE_DRMS, _always_on, \
		_supply_regulator, _hpm_min, _system_uA)

#define PM8X41_SMPS(_id, _name, _always_on, _min_uV, _max_uV, \
		_supply_regulator, _hpm_min, _system_uA) \
	PM8X41_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		| REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE | \
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		REGULATOR_CHANGE_DRMS, _always_on, \
		_supply_regulator, _hpm_min, _system_uA)

#define PM8X41_FTSMPS(_id, _name, _always_on, _min_uV, _max_uV, \
		_supply_regulator, _hpm_min, _system_uA) \
	PM8X41_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL, \
		REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS \
		| REGULATOR_CHANGE_MODE, _always_on, \
		_supply_regulator, _hpm_min, _system_uA)

#define PM8X41_VS(_id, _name, _always_on, _supply_regulator) \
	PM8X41_VREG_INIT(_id, _name, 0, 0, 0, REGULATOR_CHANGE_STATUS, \
		 _always_on, _supply_regulator, 0, 0)

#define KRAIT_PWR(_id, _name, _always_on, _min_uV, _max_uV, \
		_supply_regulator, _hpm_min, _system_uA) \
	PM8X41_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		| REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE | \
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		REGULATOR_CHANGE_DRMS, _always_on, \
		_supply_regulator, _hpm_min, _system_uA)

/* PM8x41 regulator constraints */

/*	    ID      name     a_on  min_uV   max_uV  supply    hpm_min sys_uA  */
PM8X41_SMPS(S1B,   "8841_s1", 0, 900000,  1150000,  NULL,       100000, 0);
PM8X41_FTSMPS(S2B, "8841_s2", 0, 900000,  1150000,  NULL,       100000, 0);
PM8X41_SMPS(S3B,   "8841_s3", 0, 1150000, 1150000, NULL,       100000, 0);
PM8X41_FTSMPS(S4B, "8841_s4", 0, 900000,  900000,  NULL,       100000, 0);
PM8X41_FTSMPS(S5B, "8841_s5", 0, 850000,  1100000, NULL,       100000, 0);
PM8X41_FTSMPS(S6B, "8841_s6", 0, 850000,  1100000, NULL,       100000, 0);
PM8X41_FTSMPS(S7B, "8841_s7", 0, 850000,  1100000, NULL,       100000, 0);
PM8X41_FTSMPS(S8B, "8841_s8", 0, 850000,  1100000, NULL,       100000, 0);
PM8X41_SMPS(S1,   "8941_s1", 0, 1300000, 1300000, NULL,       100000, 0);
PM8X41_SMPS(S2,   "8941_s2", 0, 2150000, 2150000, NULL,       100000, 0);
PM8X41_SMPS(S3,   "8941_s3", 0, 1800000, 1800000, NULL,       100000, 0);
PM8X41_LDO(L1,     "8941_l1",  0, 1225000, 1225000, "8941_s1", 100000, 0);
PM8X41_LDO(L2,     "8941_l2",  0, 1200000, 1200000, "8941_s3", 5000,   0);
PM8X41_LDO(L3,     "8941_l3",  0, 1200000, 1200000, "8941_s1", 10000,  0);
PM8X41_LDO(L4,     "8941_l4",  0, 1150000, 1150000, "8941_s1", 10000,  0);
PM8X41_LDO(L5,     "8941_l5",  0, 1800000, 1800000, "8941_s2", 1000,   0);
PM8X41_LDO(L6,     "8941_l6",  0, 1800000, 1800000, "8941_s2", 10000,  0);
PM8X41_LDO(L7,     "8941_l7",  0, 1800000, 1800000, "8941_s2", 1000,   0);
PM8X41_LDO(L8,     "8941_l8",  0, 1800000, 1800000, NULL,       1000,   0);
PM8X41_LDO(L9,     "8941_l9",  0, 1800000, 2950000, NULL,       10000,  0);
PM8X41_LDO(L10,    "8941_l10", 0, 1800000, 2950000, NULL,       10000,  0);
PM8X41_LDO(L11,    "8941_l11", 0, 1250000, 1250000, "8941_s1", 10000,  0);
PM8X41_LDO(L12,    "8941_l12", 0, 1800000, 1800000, "8941_s2", 10000,  0);
PM8X41_LDO(L13,    "8941_l13", 0, 2950000, 2950000, NULL,       10000,  0);
PM8X41_LDO(L14,    "8941_l14", 0, 1800000, 1800000, "8941_s2", 10000,  0);
PM8X41_LDO(L15,    "8941_l15", 0, 2050000, 2050000, "8941_s2", 10000,  0);
PM8X41_LDO(L16,    "8941_l16", 0, 2700000, 2700000, NULL,       10000,  0);
PM8X41_LDO(L17,    "8941_l17", 0, 2850000, 2850000, NULL,       10000,  0);
PM8X41_LDO(L18,    "8941_l18", 0, 2850000, 2850000, NULL,       10000,  0);
PM8X41_LDO(L19,    "8941_l19", 0, 2900000, 2900000, NULL,       10000,  0);
PM8X41_LDO(L20,    "8941_l20", 0, 2950000, 2950000, NULL,       10000,  0);
PM8X41_LDO(L21,    "8941_l21", 0, 2950000, 2950000, NULL,       10000,  0);
PM8X41_LDO(L22,    "8941_l22", 0, 3000000, 3000000, NULL,       10000,  0);
PM8X41_LDO(L23,    "8941_l23", 0, 3000000, 3000000, NULL,       10000,  0);
PM8X41_LDO(L24,    "8941_l24", 0, 3075000, 3075000, NULL,       5000,   0);

/*	  ID	      name     a_on   supply  */
PM8X41_VS(LVS1,    "8941_lvs1", 0, "8941_s3");
PM8X41_VS(LVS2,    "8941_lvs2", 0, "8941_s3");
PM8X41_VS(LVS3,    "8941_lvs3", 0, "8941_s3");

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

static struct platform_device VREG_DEVICE(S1B, 0);
static struct platform_device VREG_DEVICE(S2B, 1);
static struct platform_device VREG_DEVICE(S3B, 3);
static struct platform_device VREG_DEVICE(S4B, 4);
static struct platform_device VREG_DEVICE(S5B, 5);
static struct platform_device VREG_DEVICE(S6B, 6);
static struct platform_device VREG_DEVICE(S7B, 7);
static struct platform_device VREG_DEVICE(S8B, 8);
static struct platform_device VREG_DEVICE(S1, 9);
static struct platform_device VREG_DEVICE(S2, 10);
static struct platform_device VREG_DEVICE(S3, 11);
static struct platform_device VREG_DEVICE(L1, 12);
static struct platform_device VREG_DEVICE(L2, 13);
static struct platform_device VREG_DEVICE(L3, 14);
static struct platform_device VREG_DEVICE(L4, 15);
static struct platform_device VREG_DEVICE(L5, 16);
static struct platform_device VREG_DEVICE(L6, 17);
static struct platform_device VREG_DEVICE(L7, 18);
static struct platform_device VREG_DEVICE(L8, 19);
static struct platform_device VREG_DEVICE(L9, 20);
static struct platform_device VREG_DEVICE(L10, 21);
static struct platform_device VREG_DEVICE(L11, 22);
static struct platform_device VREG_DEVICE(L12, 23);
static struct platform_device VREG_DEVICE(L13, 24);
static struct platform_device VREG_DEVICE(L14, 25);
static struct platform_device VREG_DEVICE(L15, 26);
static struct platform_device VREG_DEVICE(L16, 27);
static struct platform_device VREG_DEVICE(L17, 28);
static struct platform_device VREG_DEVICE(L18, 29);
static struct platform_device VREG_DEVICE(L19, 30);
static struct platform_device VREG_DEVICE(L20, 31);
static struct platform_device VREG_DEVICE(L21, 32);
static struct platform_device VREG_DEVICE(L22, 33);
static struct platform_device VREG_DEVICE(L23, 34);
static struct platform_device VREG_DEVICE(L24, 35);
static struct platform_device VREG_DEVICE(LVS1, 36);
static struct platform_device VREG_DEVICE(LVS2, 37);
static struct platform_device VREG_DEVICE(LVS3, 38);
static struct platform_device VREG_DEVICE(K0, 39);
static struct platform_device VREG_DEVICE(K1, 40);
static struct platform_device VREG_DEVICE(K2, 41);
static struct platform_device VREG_DEVICE(K3, 42);

struct platform_device *msm_copper_stub_regulator_devices[] __devinitdata = {
	&vreg_device_S1B,
	&vreg_device_S2B,
	&vreg_device_S3B,
	&vreg_device_S4B,
	&vreg_device_S5B,
	&vreg_device_S6B,
	&vreg_device_S7B,
	&vreg_device_S8B,
	&vreg_device_S1,
	&vreg_device_S2,
	&vreg_device_S3,
	&vreg_device_L1,
	&vreg_device_L2,
	&vreg_device_L3,
	&vreg_device_L4,
	&vreg_device_L5,
	&vreg_device_L6,
	&vreg_device_L7,
	&vreg_device_L8,
	&vreg_device_L9,
	&vreg_device_L10,
	&vreg_device_L11,
	&vreg_device_L12,
	&vreg_device_L13,
	&vreg_device_L14,
	&vreg_device_L15,
	&vreg_device_L16,
	&vreg_device_L17,
	&vreg_device_L18,
	&vreg_device_L19,
	&vreg_device_L20,
	&vreg_device_L21,
	&vreg_device_L22,
	&vreg_device_L23,
	&vreg_device_L24,
	&vreg_device_LVS1,
	&vreg_device_LVS2,
	&vreg_device_LVS3,
	&vreg_device_K0,
	&vreg_device_K1,
	&vreg_device_K2,
	&vreg_device_K3,
};

int msm_copper_stub_regulator_devices_len __devinitdata =
			ARRAY_SIZE(msm_copper_stub_regulator_devices);
