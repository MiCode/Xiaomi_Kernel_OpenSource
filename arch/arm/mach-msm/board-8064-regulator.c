/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/regulator/pm8xxx-regulator.h>

#include "board-8064.h"

#define VREG_CONSUMERS(_id) \
	static struct regulator_consumer_supply vreg_consumers_##_id[]

/*
 * Consumer specific regulator names:
 *			 regulator name		consumer dev_name
 */
VREG_CONSUMERS(L1) = {
	REGULATOR_SUPPLY("8921_l1",		NULL),
};
VREG_CONSUMERS(L2) = {
	REGULATOR_SUPPLY("8921_l2",		NULL),
};
VREG_CONSUMERS(L3) = {
	REGULATOR_SUPPLY("8921_l3",		NULL),
	REGULATOR_SUPPLY("HSUSB_3p3",		"msm_otg"),
	REGULATOR_SUPPLY("HSUSB_3p3",		"msm_ehci_host.0"),
	REGULATOR_SUPPLY("HSUSB_3p3",		"msm_ehci_host.1"),
};
VREG_CONSUMERS(L4) = {
	REGULATOR_SUPPLY("8921_l4",		NULL),
	REGULATOR_SUPPLY("HSUSB_1p8",		"msm_otg"),
	REGULATOR_SUPPLY("HSUSB_1p8",		"msm_ehci_host.0"),
	REGULATOR_SUPPLY("HSUSB_1p8",		"msm_ehci_host.1"),
	REGULATOR_SUPPLY("iris_vddxo",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(L5) = {
	REGULATOR_SUPPLY("8921_l5",		NULL),
	REGULATOR_SUPPLY("sdc_vdd",		"msm_sdcc.1"),
};
VREG_CONSUMERS(L6) = {
	REGULATOR_SUPPLY("8921_l6",		NULL),
	REGULATOR_SUPPLY("sdc_vdd",		"msm_sdcc.3"),
};
VREG_CONSUMERS(L7) = {
	REGULATOR_SUPPLY("8921_l7",		NULL),
	REGULATOR_SUPPLY("sdc_vddp",		"msm_sdcc.3"),
};
VREG_CONSUMERS(L8) = {
	REGULATOR_SUPPLY("8921_l8",		NULL),
};
VREG_CONSUMERS(L9) = {
	REGULATOR_SUPPLY("8921_l9",		NULL),
};
VREG_CONSUMERS(L10) = {
	REGULATOR_SUPPLY("8921_l10",		NULL),
	REGULATOR_SUPPLY("iris_vddpa",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(L11) = {
	REGULATOR_SUPPLY("8921_l11",		NULL),
};
VREG_CONSUMERS(L12) = {
	REGULATOR_SUPPLY("8921_l12",		NULL),
};
VREG_CONSUMERS(L14) = {
	REGULATOR_SUPPLY("8921_l14",		NULL),
};
VREG_CONSUMERS(L15) = {
	REGULATOR_SUPPLY("8921_l15",		NULL),
};
VREG_CONSUMERS(L16) = {
	REGULATOR_SUPPLY("8921_l16",		NULL),
};
VREG_CONSUMERS(L17) = {
	REGULATOR_SUPPLY("8921_l17",		NULL),
};
VREG_CONSUMERS(L18) = {
	REGULATOR_SUPPLY("8921_l18",		NULL),
};
VREG_CONSUMERS(L22) = {
	REGULATOR_SUPPLY("8921_l22",		NULL),
};
VREG_CONSUMERS(L23) = {
	REGULATOR_SUPPLY("8921_l23",		NULL),
};
VREG_CONSUMERS(L24) = {
	REGULATOR_SUPPLY("8921_l24",		NULL),
	REGULATOR_SUPPLY("riva_vddmx",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(L25) = {
	REGULATOR_SUPPLY("8921_l25",		NULL),
	REGULATOR_SUPPLY("VDDD_CDC_D",		"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_A_1P2V",	"tabla-slim"),
};
VREG_CONSUMERS(L26) = {
	REGULATOR_SUPPLY("8921_l26",		NULL),
};
VREG_CONSUMERS(L27) = {
	REGULATOR_SUPPLY("8921_l27",		NULL),
};
VREG_CONSUMERS(L28) = {
	REGULATOR_SUPPLY("8921_l28",		NULL),
};
VREG_CONSUMERS(S1) = {
	REGULATOR_SUPPLY("8921_s1",		NULL),
};
VREG_CONSUMERS(S2) = {
	REGULATOR_SUPPLY("8921_s2",		NULL),
	REGULATOR_SUPPLY("iris_vddrfa",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(S3) = {
	REGULATOR_SUPPLY("8921_s3",		NULL),
	REGULATOR_SUPPLY("HSUSB_VDDCX",		"msm_otg"),
	REGULATOR_SUPPLY("HSUSB_VDDCX",		"msm_ehci_host.0"),
	REGULATOR_SUPPLY("HSUSB_VDDCX",		"msm_ehci_host.1"),
	REGULATOR_SUPPLY("riva_vddcx",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(S4) = {
	REGULATOR_SUPPLY("8921_s4",		NULL),
	REGULATOR_SUPPLY("sdc_vccq",		"msm_sdcc.1"),
	REGULATOR_SUPPLY("VDDIO_CDC",		"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDD_CP",		"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_TX",		"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_RX",		"tabla-slim"),
	REGULATOR_SUPPLY("riva_vddpx",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(S5) = {
	REGULATOR_SUPPLY("8921_s5",		NULL),
};
VREG_CONSUMERS(S6) = {
	REGULATOR_SUPPLY("8921_s6",		NULL),
};
VREG_CONSUMERS(S7) = {
	REGULATOR_SUPPLY("8921_s7",		NULL),
};
VREG_CONSUMERS(LVS1) = {
	REGULATOR_SUPPLY("8921_lvs1",		NULL),
	REGULATOR_SUPPLY("iris_vddio",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(LVS2) = {
	REGULATOR_SUPPLY("8921_lvs2",		NULL),
	REGULATOR_SUPPLY("iris_vdddig",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(LVS3) = {
	REGULATOR_SUPPLY("8921_lvs3",		NULL),
};
VREG_CONSUMERS(LVS4) = {
	REGULATOR_SUPPLY("8921_lvs4",		NULL),
};
VREG_CONSUMERS(LVS5) = {
	REGULATOR_SUPPLY("8921_lvs5",		NULL),
};
VREG_CONSUMERS(LVS6) = {
	REGULATOR_SUPPLY("8921_lvs6",		NULL),
};
VREG_CONSUMERS(LVS7) = {
	REGULATOR_SUPPLY("8921_lvs7",		NULL),
};
VREG_CONSUMERS(USB_OTG) = {
	REGULATOR_SUPPLY("8921_usb_otg",	NULL),
};
VREG_CONSUMERS(HDMI_MVS) = {
	REGULATOR_SUPPLY("8921_hdmi_mvs",	NULL),
};
VREG_CONSUMERS(NCP) = {
	REGULATOR_SUPPLY("8921_ncp",		NULL),
};
VREG_CONSUMERS(8821_S0) = {
	REGULATOR_SUPPLY("8821_s0",		NULL),
};
VREG_CONSUMERS(8821_S1) = {
	REGULATOR_SUPPLY("8821_s1",		NULL),
};

#define PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, _modes, _ops, \
			 _apply_uV, _pull_down, _always_on, _supply_regulator, \
			 _system_uA, _enable_time, _reg_id) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_modes_mask	= _modes, \
				.valid_ops_mask		= _ops, \
				.min_uV			= _min_uV, \
				.max_uV			= _max_uV, \
				.input_uV		= _max_uV, \
				.apply_uV		= _apply_uV, \
				.always_on		= _always_on, \
				.name			= _name, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id), \
			.consumer_supplies	= vreg_consumers_##_id, \
			.supply_regulator	= _supply_regulator, \
		}, \
		.id			= _reg_id, \
		.pull_down_enable	= _pull_down, \
		.system_uA		= _system_uA, \
		.enable_time		= _enable_time, \
	}

#define PM8XXX_LDO(_id, _name, _always_on, _pull_down, _min_uV, _max_uV, \
		_enable_time, _supply_regulator, _system_uA, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		| REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE | \
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		REGULATOR_CHANGE_DRMS, 0, _pull_down, _always_on, \
		_supply_regulator, _system_uA, _enable_time, _reg_id)

#define PM8XXX_NLDO1200(_id, _name, _always_on, _pull_down, _min_uV, \
		_max_uV, _enable_time, _supply_regulator, _system_uA, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		| REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE | \
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		REGULATOR_CHANGE_DRMS, 0, _pull_down, _always_on, \
		_supply_regulator, _system_uA, _enable_time, _reg_id)

#define PM8XXX_SMPS(_id, _name, _always_on, _pull_down, _min_uV, _max_uV, \
		_enable_time, _supply_regulator, _system_uA, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		| REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE | \
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		REGULATOR_CHANGE_DRMS, 0, _pull_down, _always_on, \
		_supply_regulator, _system_uA, _enable_time, _reg_id)

#define PM8XXX_FTSMPS(_id, _name, _always_on, _pull_down, _min_uV, _max_uV, \
		_enable_time, _supply_regulator, _system_uA, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL, \
		REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS \
		| REGULATOR_CHANGE_MODE, 0, _pull_down, _always_on, \
		_supply_regulator, _system_uA, _enable_time, _reg_id)

#define PM8XXX_VS(_id, _name, _always_on, _pull_down, _enable_time, \
		_supply_regulator, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, 0, 0, 0, REGULATOR_CHANGE_STATUS, 0, \
		_pull_down, _always_on, _supply_regulator, 0, _enable_time, \
		_reg_id)

#define PM8XXX_VS300(_id, _name, _always_on, _pull_down, _enable_time, \
		_supply_regulator, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, 0, 0, 0, REGULATOR_CHANGE_STATUS, 0, \
		_pull_down, _always_on, _supply_regulator, 0, _enable_time, \
		_reg_id)

#define PM8XXX_NCP(_id, _name, _always_on, _min_uV, _max_uV, _enable_time, \
		_supply_regulator, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, 0, \
		REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS, 0, 0, \
		_always_on, _supply_regulator, 0, _enable_time, _reg_id)

/* Pin control initialization */
#define PM8XXX_PC(_id, _name, _always_on, _pin_fn, _pin_ctrl, \
		  _supply_regulator, _reg_id) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_ops_mask	= REGULATOR_CHANGE_STATUS, \
				.always_on	= _always_on, \
				.name		= _name, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id##_PC), \
			.consumer_supplies	= vreg_consumers_##_id##_PC, \
			.supply_regulator  = _supply_regulator, \
		}, \
		.id		= _reg_id, \
		.pin_fn		= PM8XXX_VREG_PIN_FN_##_pin_fn, \
		.pin_ctrl	= _pin_ctrl, \
	}

#define SAW_VREG_INIT(_id, _name, _min_uV, _max_uV) \
	{ \
		.constraints = { \
			.name		= _name, \
			.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE, \
			.min_uV		= _min_uV, \
			.max_uV		= _max_uV, \
		}, \
		.num_consumer_supplies	= ARRAY_SIZE(vreg_consumers_##_id), \
		.consumer_supplies	= vreg_consumers_##_id, \
	}

/* SAW regulator constraints */
struct regulator_init_data msm8064_saw_regulator_pdata_8921_s5 =
	/*	      ID  vreg_name	       min_uV   max_uV */
	SAW_VREG_INIT(S5, "8921_s5",	       950000, 1150000);
struct regulator_init_data msm8064_saw_regulator_pdata_8921_s6 =
	SAW_VREG_INIT(S6, "8921_s6",	       950000, 1150000);

struct regulator_init_data msm8064_saw_regulator_pdata_8821_s0 =
	/*	      ID       vreg_name	min_uV  max_uV */
	SAW_VREG_INIT(8821_S0, "8821_s0",       950000, 1150000);
struct regulator_init_data msm8064_saw_regulator_pdata_8821_s1 =
	SAW_VREG_INIT(8821_S1, "8821_s1",       950000, 1150000);

/* PM8921 regulator constraints */
struct pm8xxx_regulator_platform_data
msm8064_pm8921_regulator_pdata[] __devinitdata = {
	/*
	 *	   ID   name always_on pd min_uV   max_uV   en_t supply
	 *	system_uA reg_ID
	 */
	PM8XXX_SMPS(S1, "8921_s1",  1, 1, 1225000, 1225000, 500, NULL, 100000,
		1),
	PM8XXX_SMPS(S2, "8921_s2",  0, 1, 1300000, 1300000, 500, NULL, 0, 2),
	PM8XXX_SMPS(S3, "8921_s3",  1, 1, 1150000, 1150000, 500, NULL, 100000,
		3),
	PM8XXX_SMPS(S4, "8921_s4",  1, 1, 1800000, 1800000, 500, NULL, 100000,
		4),
	PM8XXX_SMPS(S7, "8921_s7",  0, 1, 1200000, 1200000, 500, NULL, 100000,
		5),

	PM8XXX_LDO(L1,  "8921_l1",  1, 1, 1100000, 1100000, 200, "8921_s4", 0,
		6),
	PM8XXX_LDO(L2,  "8921_l2",  0, 1, 1200000, 1200000, 200, "8921_s4", 0,
		7),
	PM8XXX_LDO(L3,  "8921_l3",  0, 1, 3075000, 3075000, 200, NULL, 0, 8),
	PM8XXX_LDO(L4,  "8921_l4",  1, 1, 1800000, 1800000, 200, NULL, 0, 9),
	PM8XXX_LDO(L5,  "8921_l5",  0, 1, 2950000, 2950000, 200, NULL, 0, 10),
	PM8XXX_LDO(L6,  "8921_l6",  0, 1, 2950000, 2950000, 200, NULL, 0, 11),
	PM8XXX_LDO(L7,  "8921_l7",  1, 1, 1850000, 2950000, 200, NULL, 0, 12),
	PM8XXX_LDO(L8,  "8921_l8",  0, 1, 2800000, 2800000, 200, NULL, 0, 13),
	PM8XXX_LDO(L9,  "8921_l9",  0, 1, 2850000, 2850000, 200, NULL, 0, 14),
	PM8XXX_LDO(L10, "8921_l10", 0, 1, 2900000, 2900000, 200, NULL, 0, 15),
	PM8XXX_LDO(L11, "8921_l11", 0, 1, 3000000, 3000000, 200, NULL, 0, 16),
	PM8XXX_LDO(L12, "8921_l12", 0, 1, 1200000, 1200000, 200, "8921_s4", 0,
		17),
	PM8XXX_LDO(L14, "8921_l14", 0, 1, 1800000, 1800000, 200, NULL, 0, 18),
	PM8XXX_LDO(L15, "8921_l15", 0, 1, 1800000, 2950000, 200, NULL, 0, 19),
	PM8XXX_LDO(L16, "8921_l16", 0, 1, 2800000, 2800000, 200, NULL, 0, 20),
	PM8XXX_LDO(L17, "8921_l17", 0, 1, 2000000, 2000000, 200, NULL, 0, 21),
	PM8XXX_LDO(L18, "8921_l18", 0, 1, 1300000, 1800000, 200, "8921_s4", 0,
		22),
	PM8XXX_LDO(L22, "8921_l22", 0, 1, 2600000, 2600000, 200, NULL, 0, 23),
	PM8XXX_LDO(L23, "8921_l23", 0, 1, 1800000, 1800000, 200, NULL, 0, 24),
	PM8XXX_NLDO1200(L24, "8921_l24", 1, 1, 1150000, 1150000, 200, "8921_s1",
		10000, 25),
	PM8XXX_NLDO1200(L25, "8921_l25", 1, 1, 1225000, 1225000, 200, "8921_s1",
		0, 26),
	PM8XXX_NLDO1200(L26, "8921_l26", 0, 1, 1050000, 1050000, 200, "8921_s7",
		0, 27),
	PM8XXX_NLDO1200(L27, "8921_l27", 0, 1, 1000000, 1000000, 200, "8921_s7",
		0, 28),
	PM8XXX_NLDO1200(L28, "8921_l28", 0, 1, 1050000, 1050000, 200, "8921_s7",
		0, 29),

	/*        ID       name  always_on pd              en_t supply reg_ID */
	PM8XXX_VS(LVS1,	   "8921_lvs1", 0, 1,                 0, "8921_s4", 30),
	PM8XXX_VS300(LVS2, "8921_lvs2", 0, 1,                 0, "8921_s1", 31),
	PM8XXX_VS(LVS3,    "8921_lvs3", 0, 1,                 0, "8921_s4", 32),
	PM8XXX_VS(LVS4,    "8921_lvs4", 0, 1,                 0, "8921_s4", 33),
	PM8XXX_VS(LVS5,    "8921_lvs5", 0, 1,                 0, "8921_s4", 34),
	PM8XXX_VS(LVS6,    "8921_lvs6", 0, 1,                 0, "8921_s4", 35),
	PM8XXX_VS(LVS7,    "8921_lvs7", 1, 1,                 0, "8921_s4", 36),

	PM8XXX_VS300(USB_OTG,  "8921_usb_otg",  0, 1,         0, NULL, 37),
	PM8XXX_VS300(HDMI_MVS, "8921_hdmi_mvs", 0, 1,         0, NULL, 38),

	/*         ID   name  always_on   min_uV   max_uV  en_t supply reg_ID */
	PM8XXX_NCP(NCP,	"8921_ncp", 0,    1800000, 1800000, 200, "8921_l6", 39),
};

int msm8064_pm8921_regulator_pdata_len __devinitdata =
	ARRAY_SIZE(msm8064_pm8921_regulator_pdata);
