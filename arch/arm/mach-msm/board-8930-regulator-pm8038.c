/*
 * Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

/*
 * This file contains regulator configuration and mappings for targets
 * consisting of MSM8930 and PM8038.
 */

#include <linux/regulator/pm8xxx-regulator.h>

#include "board-8930.h"

#define VREG_CONSUMERS(_id) \
	static struct regulator_consumer_supply vreg_consumers_##_id[]

/*
 * Consumer specific regulator names:
 *			 regulator name		consumer dev_name
 */
VREG_CONSUMERS(L1) = {
	REGULATOR_SUPPLY("8038_l1",		NULL),
	REGULATOR_SUPPLY("iris_vddrfa",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(L2) = {
	REGULATOR_SUPPLY("8038_l2",		NULL),
	REGULATOR_SUPPLY("iris_vdddig",		"wcnss_wlan.0"),
	REGULATOR_SUPPLY("dsi_vdda",		"mipi_dsi.1"),
	REGULATOR_SUPPLY("dsi_pll_vdda",	"mdp.0"),
	REGULATOR_SUPPLY("mipi_csi_vdd",	"msm_csid.0"),
	REGULATOR_SUPPLY("mipi_csi_vdd",	"msm_csid.1"),
	REGULATOR_SUPPLY("mipi_csi_vdd",	"msm_csid.2"),
};
VREG_CONSUMERS(L3) = {
	REGULATOR_SUPPLY("8038_l3",		NULL),
	REGULATOR_SUPPLY("HSUSB_3p3",		"msm_otg"),
};
VREG_CONSUMERS(L4) = {
	REGULATOR_SUPPLY("8038_l4",		NULL),
	REGULATOR_SUPPLY("HSUSB_1p8",		"msm_otg"),
	REGULATOR_SUPPLY("iris_vddxo",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(L5) = {
	REGULATOR_SUPPLY("8038_l5",		NULL),
	REGULATOR_SUPPLY("sdc_vdd",		"msm_sdcc.1"),
};
VREG_CONSUMERS(L6) = {
	REGULATOR_SUPPLY("8038_l6",		NULL),
	REGULATOR_SUPPLY("sdc_vdd",		"msm_sdcc.3"),
};
VREG_CONSUMERS(L7) = {
	REGULATOR_SUPPLY("8038_l7",		NULL),
};
VREG_CONSUMERS(L8) = {
	REGULATOR_SUPPLY("8038_l8",		NULL),
	REGULATOR_SUPPLY("dsi_vdc",		"mipi_dsi.1"),
};
VREG_CONSUMERS(L9) = {
	REGULATOR_SUPPLY("8038_l9",		NULL),
	REGULATOR_SUPPLY("vdd_ana",		"3-004a"),
	REGULATOR_SUPPLY("vdd",			"3-0024"),
	REGULATOR_SUPPLY("cam_vana",		"4-001a"),
	REGULATOR_SUPPLY("cam_vana",		"4-006c"),
	REGULATOR_SUPPLY("cam_vana",		"4-0048"),
	REGULATOR_SUPPLY("cam_vaf",		"4-001a"),
	REGULATOR_SUPPLY("cam_vaf",		"4-006c"),
	REGULATOR_SUPPLY("cam_vaf",		"4-0048"),
	REGULATOR_SUPPLY("cam_vana",            "4-0020"),
	REGULATOR_SUPPLY("cam_vaf",             "4-0020"),
	REGULATOR_SUPPLY("vdd",			"12-0018"),
	REGULATOR_SUPPLY("vdd",			"12-0068"),
};
VREG_CONSUMERS(L10) = {
	REGULATOR_SUPPLY("8038_l10",		NULL),
	REGULATOR_SUPPLY("iris_vddpa",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(L11) = {
	REGULATOR_SUPPLY("8038_l11",		NULL),
	REGULATOR_SUPPLY("vdd_dig",		"3-004a"),
	REGULATOR_SUPPLY("iris_vddio",		"wcnss_wlan.0"),
	REGULATOR_SUPPLY("riva_vddpx",		"wcnss_wlan.0"),
	REGULATOR_SUPPLY("sdc_vdd_io",		"msm_sdcc.1"),
	REGULATOR_SUPPLY("VDDIO_CDC",		"sitar-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_TX",		"sitar-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_RX",		"sitar-slim"),
	REGULATOR_SUPPLY("VDDIO_CDC",		"sitar1p1-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_TX",		"sitar1p1-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_RX",		"sitar1p1-slim"),
	REGULATOR_SUPPLY("vcc_i2c",		"0-0048"),
	REGULATOR_SUPPLY("mhl_iovcc18",		"0-0039"),
	REGULATOR_SUPPLY("vdd-io",		"spi0.0"),
	REGULATOR_SUPPLY("vdd-phy",		"spi0.0"),
	REGULATOR_SUPPLY("sdc_vdd",		"msm_sdcc.2"),
};
VREG_CONSUMERS(L12) = {
	REGULATOR_SUPPLY("8038_l12",		NULL),
	REGULATOR_SUPPLY("cam_vdig",		"4-001a"),
	REGULATOR_SUPPLY("cam_vdig",		"4-006c"),
	REGULATOR_SUPPLY("cam_vdig",		"4-0048"),
	REGULATOR_SUPPLY("cam_vdig",            "4-0020"),
};
VREG_CONSUMERS(L13) = {
	REGULATOR_SUPPLY("8038_l13",		NULL),
};
VREG_CONSUMERS(L14) = {
	REGULATOR_SUPPLY("8038_l14",		NULL),
	REGULATOR_SUPPLY("pa_therm",		"pm8xxx-adc"),
};
VREG_CONSUMERS(L15) = {
	REGULATOR_SUPPLY("8038_l15",		NULL),
};
VREG_CONSUMERS(L16) = {
	REGULATOR_SUPPLY("8038_l16",		NULL),
	REGULATOR_SUPPLY("sw_core_vdd",		"pil-q6v4-modem"),
};
VREG_CONSUMERS(L17) = {
	REGULATOR_SUPPLY("8038_l17",		NULL),
};
VREG_CONSUMERS(L18) = {
	REGULATOR_SUPPLY("8038_l18",		NULL),
};
VREG_CONSUMERS(L19) = {
	REGULATOR_SUPPLY("8038_l19",		NULL),
	REGULATOR_SUPPLY("fw_core_vdd",		"pil-q6v4-modem"),
};
VREG_CONSUMERS(L20) = {
	REGULATOR_SUPPLY("8038_l20",		NULL),
	REGULATOR_SUPPLY("VDDD_CDC_D",		"sitar-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_A_1P2V",	"sitar-slim"),
	REGULATOR_SUPPLY("VDDD_CDC_D",		"sitar1p1-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_A_1P2V",	"sitar1p1-slim"),
	REGULATOR_SUPPLY("mhl_avcc12",		"0-0039"),
};
VREG_CONSUMERS(L21) = {
	REGULATOR_SUPPLY("8038_l21",		NULL),
};
VREG_CONSUMERS(L22) = {
	REGULATOR_SUPPLY("8038_l22",		NULL),
	REGULATOR_SUPPLY("sdc_vdd_io",		"msm_sdcc.3"),
};
VREG_CONSUMERS(L23) = {
	REGULATOR_SUPPLY("8038_l23",		NULL),
	REGULATOR_SUPPLY("dsi_vddio",		"mipi_dsi.1"),
	REGULATOR_SUPPLY("dsi_pll_vddio",	"mdp.0"),
	REGULATOR_SUPPLY("hdmi_avdd",		"hdmi_msm.0"),
	REGULATOR_SUPPLY("hdmi_vcc",		"hdmi_msm.0"),
	REGULATOR_SUPPLY("pll_vdd",		"pil_riva"),
	REGULATOR_SUPPLY("pll_vdd",		"pil-q6v4-modem"),
};
VREG_CONSUMERS(L24) = {
	REGULATOR_SUPPLY("8038_l24",		NULL),
	REGULATOR_SUPPLY("riva_vddmx",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(L25) = {
	REGULATOR_SUPPLY("8038_l25",		NULL),
};
VREG_CONSUMERS(L26) = {
	REGULATOR_SUPPLY("8038_l26",		NULL),
};
VREG_CONSUMERS(L27) = {
	REGULATOR_SUPPLY("8038_l27",		NULL),
	REGULATOR_SUPPLY("core_vdd",		"pil-q6v4-lpass"),
};
VREG_CONSUMERS(S1) = {
	REGULATOR_SUPPLY("8038_s1",		NULL),
	REGULATOR_SUPPLY("riva_vddcx",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(S2) = {
	REGULATOR_SUPPLY("8038_s2",		NULL),
};
VREG_CONSUMERS(S3) = {
	REGULATOR_SUPPLY("8038_s3",		NULL),
};
VREG_CONSUMERS(S4) = {
	REGULATOR_SUPPLY("8038_s4",		NULL),
	REGULATOR_SUPPLY("CDC_VDD_CP",		"sitar-slim"),
	REGULATOR_SUPPLY("CDC_VDD_CP",		"sitar1p1-slim"),
};
VREG_CONSUMERS(S5) = {
	REGULATOR_SUPPLY("8038_s5",		NULL),
	REGULATOR_SUPPLY("krait0",		"acpuclk-8627"),
	REGULATOR_SUPPLY("krait0",		"acpuclk-8930"),
	REGULATOR_SUPPLY("krait0",		"acpuclk-8930aa"),
	REGULATOR_SUPPLY("krait0",		"acpuclk-8930ab"),
};
VREG_CONSUMERS(S6) = {
	REGULATOR_SUPPLY("8038_s6",		NULL),
	REGULATOR_SUPPLY("krait1",		"acpuclk-8627"),
	REGULATOR_SUPPLY("krait1",		"acpuclk-8930"),
	REGULATOR_SUPPLY("krait1",		"acpuclk-8930aa"),
	REGULATOR_SUPPLY("krait1",		"acpuclk-8930ab"),
};
VREG_CONSUMERS(LVS1) = {
	REGULATOR_SUPPLY("8038_lvs1",		NULL),
	REGULATOR_SUPPLY("cam_vio",		"4-001a"),
	REGULATOR_SUPPLY("cam_vio",		"4-006c"),
	REGULATOR_SUPPLY("cam_vio",		"4-0048"),
	REGULATOR_SUPPLY("cam_vio",             "4-0020"),
};
VREG_CONSUMERS(LVS2) = {
	REGULATOR_SUPPLY("8038_lvs2",		NULL),
	REGULATOR_SUPPLY("vcc_i2c",		"3-004a"),
	REGULATOR_SUPPLY("vcc_i2c",		"3-0024"),
	REGULATOR_SUPPLY("vddio",		"12-0018"),
	REGULATOR_SUPPLY("vlogic",		"12-0068"),
};
VREG_CONSUMERS(EXT_5V) = {
	REGULATOR_SUPPLY("ext_5v",		NULL),
	REGULATOR_SUPPLY("hdmi_mvs",		"hdmi_msm.0"),
	REGULATOR_SUPPLY("mhl_usb_hs_switch",	"msm_otg"),
};
VREG_CONSUMERS(EXT_OTG_SW) = {
	REGULATOR_SUPPLY("ext_otg_sw",		NULL),
	REGULATOR_SUPPLY("vbus_otg",		"msm_otg"),
};
VREG_CONSUMERS(VDD_DIG_CORNER) = {
	REGULATOR_SUPPLY("vdd_dig_corner",	NULL),
	REGULATOR_SUPPLY("hsusb_vdd_dig",	"msm_otg"),
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

#define RPM_INIT(_id, _min_uV, _max_uV, _modes, _ops, _apply_uV, _default_uV, \
		 _peak_uA, _avg_uA, _pull_down, _pin_ctrl, _freq, _pin_fn, \
		 _force_mode, _sleep_set_force_mode, _power_mode, _state, \
		 _sleep_selectable, _always_on, _supply_regulator, _system_uA) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_modes_mask	= _modes, \
				.valid_ops_mask		= _ops, \
				.min_uV			= _min_uV, \
				.max_uV			= _max_uV, \
				.input_uV		= _min_uV, \
				.apply_uV		= _apply_uV, \
				.always_on		= _always_on, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id), \
			.consumer_supplies	= vreg_consumers_##_id, \
			.supply_regulator	= _supply_regulator, \
		}, \
		.id			= RPM_VREG_ID_PM8038_##_id, \
		.default_uV		= _default_uV, \
		.peak_uA		= _peak_uA, \
		.avg_uA			= _avg_uA, \
		.pull_down_enable	= _pull_down, \
		.pin_ctrl		= _pin_ctrl, \
		.freq			= RPM_VREG_FREQ_##_freq, \
		.pin_fn			= _pin_fn, \
		.force_mode		= _force_mode, \
		.sleep_set_force_mode	= _sleep_set_force_mode, \
		.power_mode		= _power_mode, \
		.state			= _state, \
		.sleep_selectable	= _sleep_selectable, \
		.system_uA		= _system_uA, \
	}

#define RPM_LDO(_id, _always_on, _pd, _sleep_selectable, _min_uV, _max_uV, \
		_supply_regulator, _system_uA, _init_peak_uA) \
	RPM_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		 | REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE \
		 | REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE \
		 | REGULATOR_CHANGE_DRMS, 0, _max_uV, _init_peak_uA, 0, _pd, \
		 RPM_VREG_PIN_CTRL_NONE, NONE, RPM_VREG_PIN_FN_8930_NONE, \
		 RPM_VREG_FORCE_MODE_8930_NONE, \
		 RPM_VREG_FORCE_MODE_8930_NONE, RPM_VREG_POWER_MODE_8930_PWM, \
		 RPM_VREG_STATE_OFF, _sleep_selectable, _always_on, \
		 _supply_regulator, _system_uA)

#define RPM_SMPS(_id, _always_on, _pd, _sleep_selectable, _min_uV, _max_uV, \
		 _supply_regulator, _system_uA, _freq, _force_mode, \
		 _sleep_set_force_mode) \
	RPM_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		 | REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE \
		 | REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE \
		 | REGULATOR_CHANGE_DRMS, 0, _min_uV, _system_uA, 0, _pd, \
		 RPM_VREG_PIN_CTRL_NONE, _freq, RPM_VREG_PIN_FN_8930_NONE, \
		 RPM_VREG_FORCE_MODE_8930_##_force_mode, \
		 RPM_VREG_FORCE_MODE_8930_##_sleep_set_force_mode, \
		 RPM_VREG_POWER_MODE_8930_PWM, RPM_VREG_STATE_OFF, \
		 _sleep_selectable, _always_on, _supply_regulator, _system_uA)

#define RPM_VS(_id, _always_on, _pd, _sleep_selectable, _supply_regulator) \
	RPM_INIT(_id, 0, 0, 0, REGULATOR_CHANGE_STATUS, 0, 0, 1000, 1000, _pd, \
		 RPM_VREG_PIN_CTRL_NONE, NONE, RPM_VREG_PIN_FN_8930_NONE, \
		 RPM_VREG_FORCE_MODE_8930_NONE, \
		 RPM_VREG_FORCE_MODE_8930_NONE, RPM_VREG_POWER_MODE_8930_PWM, \
		 RPM_VREG_STATE_OFF, _sleep_selectable, _always_on, \
		 _supply_regulator, 0)

#define RPM_NCP(_id, _always_on, _sleep_selectable, _min_uV, _max_uV, \
		_supply_regulator, _freq) \
	RPM_INIT(_id, _min_uV, _max_uV, 0, REGULATOR_CHANGE_VOLTAGE \
		 | REGULATOR_CHANGE_STATUS, 0, _max_uV, 1000, 1000, 0, \
		 RPM_VREG_PIN_CTRL_NONE, _freq, RPM_VREG_PIN_FN_8930_NONE, \
		 RPM_VREG_FORCE_MODE_8930_NONE, \
		 RPM_VREG_FORCE_MODE_8930_NONE, RPM_VREG_POWER_MODE_8930_PWM, \
		 RPM_VREG_STATE_OFF, _sleep_selectable, _always_on, \
		 _supply_regulator, 0)

#define RPM_CORNER(_id, _always_on, _sleep_selectable, _min_uV, _max_uV, \
		_supply_regulator) \
	RPM_INIT(_id, _min_uV, _max_uV, 0, REGULATOR_CHANGE_VOLTAGE \
		 | REGULATOR_CHANGE_STATUS, 0, _max_uV, 0, 0, 0, \
		 RPM_VREG_PIN_CTRL_NONE, NONE, RPM_VREG_PIN_FN_8930_NONE, \
		 RPM_VREG_FORCE_MODE_8930_NONE, \
		 RPM_VREG_FORCE_MODE_8930_NONE, RPM_VREG_POWER_MODE_8930_PWM, \
		 RPM_VREG_STATE_OFF, _sleep_selectable, _always_on, \
		 _supply_regulator, 0)

/* Pin control initialization */
#define RPM_PC_INIT(_id, _always_on, _pin_fn, _pin_ctrl, _supply_regulator) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_ops_mask	= REGULATOR_CHANGE_STATUS, \
				.always_on	= _always_on, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id##_PC), \
			.consumer_supplies	= vreg_consumers_##_id##_PC, \
			.supply_regulator	= _supply_regulator, \
		}, \
		.id	  = RPM_VREG_ID_PM8038_##_id##_PC, \
		.pin_fn	  = RPM_VREG_PIN_FN_8930_##_pin_fn, \
		.pin_ctrl = _pin_ctrl, \
	}

#define GPIO_VREG(_id, _reg_name, _gpio_label, _gpio, _supply_regulator) \
	[MSM8930_GPIO_VREG_ID_##_id] = { \
		.init_data = { \
			.constraints = { \
				.valid_ops_mask	= REGULATOR_CHANGE_STATUS, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id), \
			.consumer_supplies	= vreg_consumers_##_id, \
			.supply_regulator	= _supply_regulator, \
		}, \
		.regulator_name = _reg_name, \
		.gpio_label	= _gpio_label, \
		.gpio		= _gpio, \
	}

#define SAW_VREG_INIT(_id, _name, _min_uV, _max_uV) \
	{ \
		.constraints = { \
			.name		= _name, \
			.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE | \
					  REGULATOR_CHANGE_STATUS, \
			.min_uV		= _min_uV, \
			.max_uV		= _max_uV, \
		}, \
		.num_consumer_supplies	= ARRAY_SIZE(vreg_consumers_##_id), \
		.consumer_supplies	= vreg_consumers_##_id, \
	}

/* GPIO regulator constraints */
struct gpio_regulator_platform_data
msm8930_pm8038_gpio_regulator_pdata[] __devinitdata = {
	/*        ID          vreg_name     gpio_label     gpio  supply */
	GPIO_VREG(EXT_5V,     "ext_5v",     "ext_5v_en",     63, NULL),
	GPIO_VREG(EXT_OTG_SW, "ext_otg_sw", "ext_otg_sw_en", 97, "ext_5v"),
};

/* SAW regulator constraints */
struct regulator_init_data msm8930_pm8038_saw_regulator_core0_pdata =
	/*	      ID  vreg_name	       min_uV   max_uV */
	SAW_VREG_INIT(S5, "8038_s5",	       850000, 1300000);
struct regulator_init_data msm8930_pm8038_saw_regulator_core1_pdata =
	SAW_VREG_INIT(S6, "8038_s6",	       850000, 1300000);

/* PM8038 regulator constraints */
struct pm8xxx_regulator_platform_data
msm8930_pm8038_regulator_pdata[] __devinitdata = {
	/*
	 *	    ID  name always_on pd min_uV   max_uV   en_t supply
	 *	system_uA reg_ID
	 */
	PM8XXX_NLDO1200(L16, "8038_l16", 0, 1, 375000, 1050000, 200, "8038_s3",
		0, 0),
	PM8XXX_NLDO1200(L19, "8038_l19", 0, 1, 375000, 1050000, 200, "8038_s3",
		0, 1),
	PM8XXX_NLDO1200(L27, "8038_l27", 0, 1, 375000, 1050000, 200, "8038_s3",
		0, 2),
};

static struct rpm_regulator_init_data
msm8930_rpm_regulator_init_data[] __devinitdata = {
	/*	ID a_on pd ss min_uV   max_uV  supply sys_uA  freq  fm  ss_fm */
	RPM_SMPS(S1, 0, 1, 1,  500000, 1150000, NULL, 100000, 4p80, AUTO, LPM),
	RPM_SMPS(S2, 1, 1, 1, 1400000, 1400000, NULL, 100000, 1p60, AUTO, LPM),
	RPM_SMPS(S3, 0, 1, 1, 1150000, 1150000, NULL, 100000, 3p20, AUTO, AUTO),
	RPM_SMPS(S4, 1, 1, 1, 1950000, 2200000, NULL, 100000, 1p60, AUTO, LPM),

	/*	ID     a_on pd ss min_uV   max_uV  supply  sys_uA init_ip */
	RPM_LDO(L1,	 0, 1, 0, 1300000, 1300000, "8038_s2", 0, 0),
	RPM_LDO(L2,	 0, 1, 0, 1200000, 1200000, "8038_s2", 0, 0),
	RPM_LDO(L3,	 0, 1, 0, 3075000, 3075000, NULL,      0, 0),
	RPM_LDO(L4,	 1, 1, 0, 1800000, 1800000, NULL,      10000, 10000),
	RPM_LDO(L5,	 0, 1, 0, 2950000, 2950000, NULL,      0, 0),
	RPM_LDO(L6,	 0, 1, 0, 2950000, 2950000, NULL,      0, 0),
	RPM_LDO(L7,	 0, 1, 0, 2050000, 2050000, "8038_s4", 0, 0),
	RPM_LDO(L8,	 0, 1, 0, 2800000, 2800000, NULL,      0, 0),
	RPM_LDO(L9,	 0, 1, 0, 2850000, 2850000, NULL,      0, 0),
	RPM_LDO(L10,	 0, 1, 0, 2900000, 2900000, NULL,      0, 0),
	RPM_LDO(L11,	 1, 1, 0, 1800000, 1800000, "8038_s4", 10000, 10000),
	RPM_LDO(L12,	 0, 1, 0, 1200000, 1200000, "8038_s2", 0, 0),
	RPM_LDO(L13,	 0, 0, 0, 2220000, 2220000, NULL,      0, 0),
	RPM_LDO(L14,	 0, 1, 0, 1800000, 1800000, NULL,      0, 0),
	RPM_LDO(L15,	 0, 1, 0, 1800000, 2950000, NULL,      0, 0),
	RPM_LDO(L17,	 0, 1, 0, 1800000, 2950000, NULL,      0, 0),
	RPM_LDO(L18,	 0, 1, 0, 1800000, 1800000, NULL,      0, 0),
	RPM_LDO(L20,	 1, 1, 0, 1250000, 1250000, "8038_s2", 10000, 10000),
	RPM_LDO(L21,	 0, 1, 0, 1900000, 1900000, "8038_s4", 0, 0),
	RPM_LDO(L22,	 1, 1, 0, 1850000, 2950000, NULL,      10000, 10000),
	RPM_LDO(L23,	 1, 1, 1, 1800000, 1800000, "8038_s4", 0, 0),
	RPM_LDO(L24,	 0, 1, 1,  500000, 1150000, "8038_s2", 10000, 10000),
	RPM_LDO(L25,	 0, 0, 0, 1740000, 1740000, "8038_l13", 0, 0),
	RPM_LDO(L26,     1, 1, 0, 1050000, 1050000, "8038_s2", 10000, 10000),

	/*	ID     a_on pd ss		    supply */
	RPM_VS(LVS1,	 0, 1, 0,		    "8038_l11"),
	RPM_VS(LVS2,	 0, 1, 0,		    "8038_l11"),

	/*	   ID            a_on ss min_corner  max_corner  supply */
	RPM_CORNER(VDD_DIG_CORNER, 0, 1, RPM_VREG_CORNER_NONE,
		RPM_VREG_CORNER_HIGH, NULL),
};

int msm8930_pm8038_regulator_pdata_len __devinitdata =
	ARRAY_SIZE(msm8930_pm8038_regulator_pdata);

#define RPM_REG_MAP(_id, _sleep_also, _voter, _supply, _dev_name) \
	{ \
		.vreg_id = RPM_VREG_ID_PM8038_##_id, \
		.sleep_also = _sleep_also, \
		.voter = _voter, \
		.supply = _supply, \
		.dev_name = _dev_name, \
	}
static struct rpm_regulator_consumer_mapping
	      msm_rpm_regulator_consumer_mapping[] __devinitdata = {
	RPM_REG_MAP(L23,            0, 1, "krait0_hfpll", "acpuclk-8930"),
	RPM_REG_MAP(L23,            0, 2, "krait1_hfpll", "acpuclk-8930"),
	RPM_REG_MAP(L23,            0, 6, "l2_hfpll",     "acpuclk-8930"),
	RPM_REG_MAP(L24,            0, 1, "krait0_mem",   "acpuclk-8930"),
	RPM_REG_MAP(L24,            0, 2, "krait1_mem",   "acpuclk-8930"),
	RPM_REG_MAP(VDD_DIG_CORNER, 0, 1, "krait0_dig",   "acpuclk-8930"),
	RPM_REG_MAP(VDD_DIG_CORNER, 0, 2, "krait1_dig",   "acpuclk-8930"),

	RPM_REG_MAP(L23,            0, 1, "krait0_hfpll", "acpuclk-8627"),
	RPM_REG_MAP(L23,            0, 2, "krait1_hfpll", "acpuclk-8627"),
	RPM_REG_MAP(L23,            0, 6, "l2_hfpll",     "acpuclk-8627"),
	RPM_REG_MAP(L24,            0, 1, "krait0_mem",   "acpuclk-8627"),
	RPM_REG_MAP(L24,            0, 2, "krait1_mem",   "acpuclk-8627"),
	RPM_REG_MAP(VDD_DIG_CORNER, 0, 1, "krait0_dig",   "acpuclk-8627"),
	RPM_REG_MAP(VDD_DIG_CORNER, 0, 2, "krait1_dig",   "acpuclk-8627"),

	RPM_REG_MAP(L23,            0, 1, "krait0_hfpll", "acpuclk-8930aa"),
	RPM_REG_MAP(L23,            0, 2, "krait1_hfpll", "acpuclk-8930aa"),
	RPM_REG_MAP(L23,            0, 6, "l2_hfpll",     "acpuclk-8930aa"),
	RPM_REG_MAP(L24,            0, 1, "krait0_mem",   "acpuclk-8930aa"),
	RPM_REG_MAP(L24,            0, 2, "krait1_mem",   "acpuclk-8930aa"),
	RPM_REG_MAP(VDD_DIG_CORNER, 0, 1, "krait0_dig",   "acpuclk-8930aa"),
	RPM_REG_MAP(VDD_DIG_CORNER, 0, 2, "krait1_dig",   "acpuclk-8930aa"),

	RPM_REG_MAP(L23,            0, 1, "krait0_hfpll", "acpuclk-8930ab"),
	RPM_REG_MAP(L23,            0, 2, "krait1_hfpll", "acpuclk-8930ab"),
	RPM_REG_MAP(L23,            0, 6, "l2_hfpll",     "acpuclk-8930ab"),
	RPM_REG_MAP(L24,            0, 1, "krait0_mem",   "acpuclk-8930ab"),
	RPM_REG_MAP(L24,            0, 2, "krait1_mem",   "acpuclk-8930ab"),
	RPM_REG_MAP(VDD_DIG_CORNER, 0, 1, "krait0_dig",   "acpuclk-8930ab"),
	RPM_REG_MAP(VDD_DIG_CORNER, 0, 2, "krait1_dig",   "acpuclk-8930ab"),
};

struct rpm_regulator_platform_data
msm8930_pm8038_rpm_regulator_pdata __devinitdata = {
	.init_data		= msm8930_rpm_regulator_init_data,
	.num_regulators		= ARRAY_SIZE(msm8930_rpm_regulator_init_data),
	.version		= RPM_VREG_VERSION_8930,
	.vreg_id_vdd_mem	= RPM_VREG_ID_PM8038_L24,
	.vreg_id_vdd_dig	= RPM_VREG_ID_PM8038_VDD_DIG_CORNER,
	.consumer_map		= msm_rpm_regulator_consumer_mapping,
	.consumer_map_len = ARRAY_SIZE(msm_rpm_regulator_consumer_mapping),
};
