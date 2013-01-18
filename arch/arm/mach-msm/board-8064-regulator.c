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

#include <linux/regulator/pm8xxx-regulator.h>

#include "board-8064.h"

#define VREG_CONSUMERS(_id) \
	static struct regulator_consumer_supply vreg_consumers_##_id[]

/* Regulators that are present when using either PM8921 or PM8917 */
/*
 * Consumer specific regulator names:
 *			 regulator name		consumer dev_name
 */
VREG_CONSUMERS(L1) = {
	REGULATOR_SUPPLY("8921_l1",		NULL),
};
VREG_CONSUMERS(L2) = {
	REGULATOR_SUPPLY("8921_l2",		NULL),
	REGULATOR_SUPPLY("mipi_csi_vdd",	"msm_csid.0"),
	REGULATOR_SUPPLY("mipi_csi_vdd",	"msm_csid.1"),
	REGULATOR_SUPPLY("mipi_csi_vdd",	"msm_csid.2"),
	REGULATOR_SUPPLY("lvds_pll_vdda",	"lvds.0"),
	REGULATOR_SUPPLY("dsi1_pll_vdda",	"mipi_dsi.1"),
	REGULATOR_SUPPLY("HRD_VDDD_CDC_D",		"tabla2x-slim"),
	REGULATOR_SUPPLY("HRD_CDC_VDDA_A_1P2V",	"tabla2x-slim"),
	REGULATOR_SUPPLY("dsi_pll_vdda",	"mdp.0"),
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
	REGULATOR_SUPPLY("iris_vddxo",		"wcnss_wlan.0"),
	REGULATOR_SUPPLY("bha_vddxo",		"bt_power"),
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
	REGULATOR_SUPPLY("sdc_vdd_io",		"msm_sdcc.3"),
};
VREG_CONSUMERS(L8) = {
	REGULATOR_SUPPLY("8921_l8",		NULL),
	REGULATOR_SUPPLY("cam_vana",		"4-001a"),
	REGULATOR_SUPPLY("cam_vana",		"4-0010"),
	REGULATOR_SUPPLY("cam_vana",		"4-0048"),
	REGULATOR_SUPPLY("cam_vana",		"4-006c"),
	REGULATOR_SUPPLY("cam_vana",		"4-0034"),
	REGULATOR_SUPPLY("cam_vana",		"4-0020"),
};
VREG_CONSUMERS(L9) = {
	REGULATOR_SUPPLY("8921_l9",		NULL),
	REGULATOR_SUPPLY("vdd",			"3-0024"),
};
VREG_CONSUMERS(L10) = {
	REGULATOR_SUPPLY("8921_l10",		NULL),
	REGULATOR_SUPPLY("iris_vddpa",		"wcnss_wlan.0"),
	REGULATOR_SUPPLY("bha_vddpa",		"bt_power"),
};
VREG_CONSUMERS(L11) = {
	REGULATOR_SUPPLY("8921_l11",		NULL),
	REGULATOR_SUPPLY("dsi1_avdd",		"mipi_dsi.1"),
};
VREG_CONSUMERS(L12) = {
	REGULATOR_SUPPLY("cam_vdig",		"4-001a"),
	REGULATOR_SUPPLY("cam_vdig",		"4-0010"),
	REGULATOR_SUPPLY("cam_vdig",		"4-0048"),
	REGULATOR_SUPPLY("cam_vdig",		"4-006c"),
	REGULATOR_SUPPLY("cam_vdig",		"4-0034"),
	REGULATOR_SUPPLY("cam_vdig",		"4-0020"),
	REGULATOR_SUPPLY("8921_l12",		NULL),
};
VREG_CONSUMERS(L13) = {
	REGULATOR_SUPPLY("8921_l13",		NULL),
};
VREG_CONSUMERS(L14) = {
	REGULATOR_SUPPLY("8921_l14",		NULL),
	REGULATOR_SUPPLY("vreg_xoadc",		"pm8921-charger"),
};
VREG_CONSUMERS(L15) = {
	REGULATOR_SUPPLY("8921_l15",		NULL),
};
VREG_CONSUMERS(L16) = {
	REGULATOR_SUPPLY("8921_l16",		NULL),
	REGULATOR_SUPPLY("cam_vaf",		"4-001a"),
	REGULATOR_SUPPLY("cam_vaf",		"4-0010"),
	REGULATOR_SUPPLY("cam_vaf",		"4-0048"),
	REGULATOR_SUPPLY("cam_vaf",		"4-006c"),
	REGULATOR_SUPPLY("cam_vaf",		"4-0034"),
	REGULATOR_SUPPLY("cam_vaf",		"4-0020"),
};
VREG_CONSUMERS(L17) = {
	REGULATOR_SUPPLY("8921_l17",		NULL),
};
VREG_CONSUMERS(L18) = {
	REGULATOR_SUPPLY("8921_l18",		NULL),
};
VREG_CONSUMERS(L21) = {
	REGULATOR_SUPPLY("8921_l21",		NULL),
};
VREG_CONSUMERS(L22) = {
	REGULATOR_SUPPLY("8921_l22",		NULL),
};
VREG_CONSUMERS(L23) = {
	REGULATOR_SUPPLY("8921_l23",		NULL),
	REGULATOR_SUPPLY("HSUSB_1p8",		"msm_ehci_host.0"),
	REGULATOR_SUPPLY("HSUSB_1p8",		"msm_ehci_host.1"),
};
VREG_CONSUMERS(L24) = {
	REGULATOR_SUPPLY("8921_l24",		NULL),
	REGULATOR_SUPPLY("riva_vddmx",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(L25) = {
	REGULATOR_SUPPLY("8921_l25",		NULL),
	REGULATOR_SUPPLY("VDDD_CDC_D",		"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_A_1P2V",	"tabla-slim"),
	REGULATOR_SUPPLY("VDDD_CDC_D",		"tabla2x-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_A_1P2V",	"tabla2x-slim"),
	REGULATOR_SUPPLY("VDDD_CDC_D",		"0-000d"),
	REGULATOR_SUPPLY("CDC_VDDA_A_1P2V",	"0-000d"),
};
VREG_CONSUMERS(L26) = {
	REGULATOR_SUPPLY("8921_l26",		NULL),
	REGULATOR_SUPPLY("core_vdd",		"pil-q6v4-lpass"),
};
VREG_CONSUMERS(L27) = {
	REGULATOR_SUPPLY("8921_l27",		NULL),
};
VREG_CONSUMERS(L28) = {
	REGULATOR_SUPPLY("8921_l28",		NULL),
};
VREG_CONSUMERS(L29) = {
	REGULATOR_SUPPLY("8921_l29",		NULL),
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
	REGULATOR_SUPPLY("HSIC_VDDCX",		"msm_hsic_host"),
	REGULATOR_SUPPLY("riva_vddcx",		"wcnss_wlan.0"),
	REGULATOR_SUPPLY("vp_pcie",             "msm_pcie"),
	REGULATOR_SUPPLY("vptx_pcie",           "msm_pcie"),
};
VREG_CONSUMERS(S4) = {
	REGULATOR_SUPPLY("8921_s4",		NULL),
	REGULATOR_SUPPLY("sdc_vdd_io",		"msm_sdcc.1"),
	REGULATOR_SUPPLY("VDDIO_CDC",		"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDD_CP",		"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_TX",		"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_RX",		"tabla-slim"),
	REGULATOR_SUPPLY("VDDIO_CDC",		"tabla2x-slim"),
	REGULATOR_SUPPLY("CDC_VDD_CP",		"tabla2x-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_TX",		"tabla2x-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_RX",		"tabla2x-slim"),
	REGULATOR_SUPPLY("VDDIO_CDC",		"0-000d"),
	REGULATOR_SUPPLY("CDC_VDD_CP",		"0-000d"),
	REGULATOR_SUPPLY("CDC_VDDA_TX",		"0-000d"),
	REGULATOR_SUPPLY("CDC_VDDA_RX",		"0-000d"),
	REGULATOR_SUPPLY("riva_vddpx",		"wcnss_wlan.0"),
	REGULATOR_SUPPLY("bha_vddpx",		"bt_power"),
	REGULATOR_SUPPLY("vcc_i2c",		"3-005b"),
	REGULATOR_SUPPLY("vcc_i2c",		"3-0024"),
	REGULATOR_SUPPLY("vddp",		"0-0048"),
	REGULATOR_SUPPLY("hdmi_lvl_tsl",	"hdmi_msm.0"),
	REGULATOR_SUPPLY("vdd-io",		"spi0.2"),
	REGULATOR_SUPPLY("sata_pmp_pwr",	"msm_sata.0"),
};
VREG_CONSUMERS(S5) = {
	REGULATOR_SUPPLY("8921_s5",		NULL),
	REGULATOR_SUPPLY("krait0",		"acpuclk-8064"),
};
VREG_CONSUMERS(S6) = {
	REGULATOR_SUPPLY("8921_s6",		NULL),
	REGULATOR_SUPPLY("krait1",		"acpuclk-8064"),
};
VREG_CONSUMERS(S7) = {
	REGULATOR_SUPPLY("8921_s7",		NULL),
};
VREG_CONSUMERS(S8) = {
	REGULATOR_SUPPLY("8921_s8",		NULL),
};
VREG_CONSUMERS(LVS1) = {
	REGULATOR_SUPPLY("8921_lvs1",		NULL),
	REGULATOR_SUPPLY("iris_vddio",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(LVS3) = {
	REGULATOR_SUPPLY("8921_lvs3",		NULL),
};
VREG_CONSUMERS(LVS4) = {
	REGULATOR_SUPPLY("8921_lvs4",		NULL),
};
VREG_CONSUMERS(LVS5) = {
	REGULATOR_SUPPLY("8921_lvs5",		NULL),
	REGULATOR_SUPPLY("cam_vio",		"4-001a"),
	REGULATOR_SUPPLY("cam_vio",		"4-0010"),
	REGULATOR_SUPPLY("cam_vio",		"4-0048"),
	REGULATOR_SUPPLY("cam_vio",		"4-006c"),
	REGULATOR_SUPPLY("cam_vio",		"4-0034"),
	REGULATOR_SUPPLY("cam_vio",		"4-0020"),
};
VREG_CONSUMERS(LVS6) = {
	REGULATOR_SUPPLY("8921_lvs6",		NULL),
	REGULATOR_SUPPLY("vdd_pcie_vph",        "msm_pcie"),
};
VREG_CONSUMERS(LVS7) = {
	REGULATOR_SUPPLY("8921_lvs7",		NULL),
	REGULATOR_SUPPLY("pll_vdd",		"pil_riva"),
	REGULATOR_SUPPLY("lvds_vdda",		"lvds.0"),
	REGULATOR_SUPPLY("dsi1_vddio",		"mipi_dsi.1"),
	REGULATOR_SUPPLY("dsi_pll_vddio",	"mdp.0"),
	REGULATOR_SUPPLY("hdmi_vdda",		"hdmi_msm.0"),
};
VREG_CONSUMERS(USB_OTG) = {
	REGULATOR_SUPPLY("8921_usb_otg",	NULL),
	REGULATOR_SUPPLY("vbus_otg",		"msm_otg"),
};
VREG_CONSUMERS(8821_S0) = {
	REGULATOR_SUPPLY("8821_s0",		NULL),
	REGULATOR_SUPPLY("krait2",		"acpuclk-8064"),
};
VREG_CONSUMERS(8821_S1) = {
	REGULATOR_SUPPLY("8821_s1",		NULL),
	REGULATOR_SUPPLY("krait3",		"acpuclk-8064"),
};
VREG_CONSUMERS(EXT_MPP8) = {
	REGULATOR_SUPPLY("ext_mpp8",		NULL),
	REGULATOR_SUPPLY("vbus",		"msm_ehci_host.1"),
};
VREG_CONSUMERS(EXT_3P3V) = {
	REGULATOR_SUPPLY("ext_3p3v",		NULL),
	REGULATOR_SUPPLY("vdd-phy",		"spi0.2"),
	REGULATOR_SUPPLY("mhl_usb_hs_switch",	"msm_otg"),
	REGULATOR_SUPPLY("lvds_vccs_3p3v",      "lvds.0"),
	REGULATOR_SUPPLY("dsi1_vccs_3p3v",      "mipi_dsi.1"),
	REGULATOR_SUPPLY("hdmi_mux_vdd",        "hdmi_msm.0"),
	REGULATOR_SUPPLY("pcie_ext_3p3v",       "msm_pcie"),
};
VREG_CONSUMERS(EXT_TS_SW) = {
	REGULATOR_SUPPLY("ext_ts_sw",		NULL),
	REGULATOR_SUPPLY("vdd_ana",		"3-005b"),
};
VREG_CONSUMERS(EXT_SATA_PWR) = {
	REGULATOR_SUPPLY("ext_sata_pwr",	NULL),
	REGULATOR_SUPPLY("sata_ext_3p3v",       "msm_sata.0"),
};
VREG_CONSUMERS(AVC_1P2V) = {
	REGULATOR_SUPPLY("avc_1p2v",	NULL),
};
VREG_CONSUMERS(AVC_1P8V) = {
	REGULATOR_SUPPLY("avc_1p8v",	NULL),
};
VREG_CONSUMERS(AVC_2P2V) = {
	REGULATOR_SUPPLY("avc_2p2v",	NULL),
};
VREG_CONSUMERS(AVC_5V) = {
	REGULATOR_SUPPLY("avc_5v",	NULL),
};
VREG_CONSUMERS(AVC_3P3V) = {
	REGULATOR_SUPPLY("avc_3p3v",	NULL),
};

/* Regulators that are only present when using PM8921 */
VREG_CONSUMERS(S1) = {
	REGULATOR_SUPPLY("8921_s1",		NULL),
};
VREG_CONSUMERS(LVS2) = {
	REGULATOR_SUPPLY("8921_lvs2",		NULL),
	REGULATOR_SUPPLY("iris_vdddig",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(HDMI_MVS) = {
	REGULATOR_SUPPLY("8921_hdmi_mvs",	NULL),
	REGULATOR_SUPPLY("hdmi_mvs",		"hdmi_msm.0"),
};
VREG_CONSUMERS(NCP) = {
	REGULATOR_SUPPLY("8921_ncp",		NULL),
};
VREG_CONSUMERS(EXT_5V) = {
	REGULATOR_SUPPLY("ext_5v",		NULL),
	REGULATOR_SUPPLY("vbus",		"msm_ehci_host.0"),
};

/* Regulators that are only present when using PM8917 */
VREG_CONSUMERS(8917_S1) = {
	REGULATOR_SUPPLY("8921_s1",		NULL),
	REGULATOR_SUPPLY("iris_vdddig",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(L30) = {
	REGULATOR_SUPPLY("8917_l30",		NULL),
};
VREG_CONSUMERS(L31) = {
	REGULATOR_SUPPLY("8917_l31",		NULL),
};
VREG_CONSUMERS(L32) = {
	REGULATOR_SUPPLY("8917_l32",		NULL),
};
VREG_CONSUMERS(L33) = {
	REGULATOR_SUPPLY("8917_l33",		NULL),
};
VREG_CONSUMERS(L34) = {
	REGULATOR_SUPPLY("8917_l34",		NULL),
};
VREG_CONSUMERS(L35) = {
	REGULATOR_SUPPLY("8917_l35",		NULL),
};
VREG_CONSUMERS(L36) = {
	REGULATOR_SUPPLY("8917_l36",		NULL),
};
VREG_CONSUMERS(BOOST) = {
	REGULATOR_SUPPLY("8917_boost",		NULL),
	REGULATOR_SUPPLY("ext_ddr3",		NULL),
	REGULATOR_SUPPLY("vbus",		"msm_ehci_host.0"),
	REGULATOR_SUPPLY("hdmi_mvs",		"hdmi_msm.0"),
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

#define PM8XXX_BOOST(_id, _name, _always_on, _min_uV, _max_uV, _enable_time, \
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

#define GPIO_VREG(_id, _reg_name, _gpio_label, _gpio, _supply_regulator, \
		_active_low) \
	[GPIO_VREG_ID_##_id] = { \
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
		.active_low	= _active_low, \
	}

#define FIXED_VREG_INIT(_id, _supply_regulator) \
	{ \
		.constraints = { \
			.valid_ops_mask	= REGULATOR_CHANGE_STATUS, \
		}, \
		.num_consumer_supplies	= ARRAY_SIZE(vreg_consumers_##_id), \
		.consumer_supplies	= vreg_consumers_##_id, \
		.supply_regulator	= _supply_regulator, \
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
		.id			= RPM_VREG_ID_PM8921_##_id, \
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
		 RPM_VREG_PIN_CTRL_NONE, NONE, RPM_VREG_PIN_FN_8960_NONE, \
		 RPM_VREG_FORCE_MODE_8960_NONE, \
		 RPM_VREG_FORCE_MODE_8960_NONE, RPM_VREG_POWER_MODE_8960_PWM, \
		 RPM_VREG_STATE_OFF, _sleep_selectable, _always_on, \
		 _supply_regulator, _system_uA)

#define RPM_SMPS(_id, _always_on, _pd, _sleep_selectable, _min_uV, _max_uV, \
		 _supply_regulator, _system_uA, _freq, _force_mode, \
		 _sleep_set_force_mode) \
	RPM_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		 | REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE \
		 | REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE \
		 | REGULATOR_CHANGE_DRMS, 0, _max_uV, _system_uA, 0, _pd, \
		 RPM_VREG_PIN_CTRL_NONE, _freq, RPM_VREG_PIN_FN_8960_NONE, \
		 RPM_VREG_FORCE_MODE_8960_##_force_mode, \
		 RPM_VREG_FORCE_MODE_8960_##_sleep_set_force_mode, \
		 RPM_VREG_POWER_MODE_8960_PWM, RPM_VREG_STATE_OFF, \
		 _sleep_selectable, _always_on, _supply_regulator, _system_uA)

#define RPM_VS(_id, _always_on, _pd, _sleep_selectable, _supply_regulator) \
	RPM_INIT(_id, 0, 0, 0, REGULATOR_CHANGE_STATUS, 0, 0, 1000, 1000, _pd, \
		 RPM_VREG_PIN_CTRL_NONE, NONE, RPM_VREG_PIN_FN_8960_NONE, \
		 RPM_VREG_FORCE_MODE_8960_NONE, \
		 RPM_VREG_FORCE_MODE_8960_NONE, RPM_VREG_POWER_MODE_8960_PWM, \
		 RPM_VREG_STATE_OFF, _sleep_selectable, _always_on, \
		 _supply_regulator, 0)

#define RPM_NCP(_id, _always_on, _sleep_selectable, _min_uV, _max_uV, \
		_supply_regulator, _freq) \
	RPM_INIT(_id, _min_uV, _max_uV, 0, REGULATOR_CHANGE_VOLTAGE \
		 | REGULATOR_CHANGE_STATUS, 0, _max_uV, 1000, 1000, 0, \
		 RPM_VREG_PIN_CTRL_NONE, _freq, RPM_VREG_PIN_FN_8960_NONE, \
		 RPM_VREG_FORCE_MODE_8960_NONE, \
		 RPM_VREG_FORCE_MODE_8960_NONE, RPM_VREG_POWER_MODE_8960_PWM, \
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
		.id	  = RPM_VREG_ID_PM8921_##_id##_PC, \
		.pin_fn	  = RPM_VREG_PIN_FN_8960_##_pin_fn, \
		.pin_ctrl = _pin_ctrl, \
	}

/* GPIO regulator constraints */
struct gpio_regulator_platform_data
apq8064_gpio_regulator_pdata[] __devinitdata = {
	/*        ID      vreg_name gpio_label   gpio   supply   active_low */
	GPIO_VREG(EXT_5V, "ext_5v", "ext_5v_en",
			PM8921_MPP_PM_TO_SYS(7), NULL, 0),
	GPIO_VREG(EXT_3P3V, "ext_3p3v", "ext_3p3v_en",
		  APQ8064_EXT_3P3V_REG_EN_GPIO, NULL, 0),
	GPIO_VREG(EXT_TS_SW, "ext_ts_sw", "ext_ts_sw_en",
		  PM8921_GPIO_PM_TO_SYS(23), "ext_3p3v", 0),
	GPIO_VREG(EXT_MPP8, "ext_mpp8", "ext_mpp8_en",
			PM8921_MPP_PM_TO_SYS(8), NULL, 0),
	GPIO_VREG(EXT_SATA_PWR, "ext_sata_pwr", "ext_sata_pwr_en",
			PM8921_MPP_PM_TO_SYS(4), "ext_3p3v", 1),
};

struct gpio_regulator_platform_data
mpq8064_gpio_regulator_pdata[] __devinitdata = {
	GPIO_VREG(AVC_1P2V, "avc_1p2v", "avc_1p2v_en",
			SX150X_GPIO(4, 2), NULL, 0),
	GPIO_VREG(AVC_1P8V, "avc_1p8v", "avc_1p8v_en",
			SX150X_GPIO(4, 4), NULL, 0),
	GPIO_VREG(AVC_2P2V, "avc_2p2v", "avc_2p2v_en",
						 SX150X_GPIO(4, 14), NULL, 0),
	GPIO_VREG(AVC_5V, "avc_5v", "avc_5v_en", SX150X_GPIO(4, 3), NULL, 0),
	GPIO_VREG(AVC_3P3V, "avc_3p3v", "avc_3p3v_en",
					SX150X_GPIO(4, 15), "avc_5v", 0),
};

/* Fixed regulator constraints */
static struct regulator_init_data mpq8064_3p3_regulator_init =
	/*              ID        supply */
	FIXED_VREG_INIT(EXT_3P3V, NULL);

struct fixed_voltage_config mpq8064_3p3_regulator_pdata = {
	.supply_name = "ext_3p3v",
	.gpio = -EINVAL,
	.init_data = &mpq8064_3p3_regulator_init,
};

/* SAW regulator constraints */
struct regulator_init_data msm8064_saw_regulator_pdata_8921_s5 =
	/*	      ID  vreg_name	       min_uV   max_uV */
	SAW_VREG_INIT(S5, "8921_s5",	       850000, 1300000);
struct regulator_init_data msm8064_saw_regulator_pdata_8921_s6 =
	SAW_VREG_INIT(S6, "8921_s6",	       850000, 1300000);

struct regulator_init_data msm8064_saw_regulator_pdata_8821_s0 =
	/*	      ID       vreg_name	min_uV  max_uV */
	SAW_VREG_INIT(8821_S0, "8821_s0",       850000, 1300000);
struct regulator_init_data msm8064_saw_regulator_pdata_8821_s1 =
	SAW_VREG_INIT(8821_S1, "8821_s1",       850000, 1300000);

/* PM8921 regulator constraints */
struct pm8xxx_regulator_platform_data
msm8064_pm8921_regulator_pdata[] __devinitdata = {
	/*
	 *		ID   name always_on pd min_uV   max_uV   en_t supply
	 *	system_uA reg_ID
	 */
	PM8XXX_NLDO1200(L26, "8921_l26", 0, 1, 375000, 1050000, 200, "8921_s7",
		0, 1),

	/*           ID        name     always_on pd       en_t supply reg_ID */
	PM8XXX_VS300(USB_OTG,  "8921_usb_otg",  0, 0,         0, "ext_5v", 2),
	PM8XXX_VS300(HDMI_MVS, "8921_hdmi_mvs", 0, 1,         0, "ext_5v", 3),
};

/* PM8917 regulator constraints */
struct pm8xxx_regulator_platform_data
msm8064_pm8917_regulator_pdata[] __devinitdata = {
	/*
	 *		ID   name always_on pd min_uV   max_uV   en_t supply
	 *	system_uA reg_ID
	 */
	PM8XXX_NLDO1200(L26, "8921_l26", 0, 1, 375000, 1050000, 200, "8921_s7",
		0, 1),
	PM8XXX_LDO(L30,      "8917_l30", 0, 1, 1800000, 1800000, 200, NULL,
		0, 2),
	PM8XXX_LDO(L31,      "8917_l31", 0, 1, 1800000, 1800000, 200, NULL,
		0, 3),
	PM8XXX_LDO(L32,      "8917_l32", 0, 1, 2800000, 2800000, 200, NULL,
		0, 4),
	PM8XXX_LDO(L33,      "8917_l33", 0, 1, 2800000, 2800000, 200, NULL,
		0, 5),
	PM8XXX_LDO(L34,      "8917_l34", 0, 1, 1800000, 1800000, 200, NULL,
		0, 6),
	PM8XXX_LDO(L35,      "8917_l35", 0, 1, 3000000, 3000000, 200, NULL,
		0, 7),
	PM8XXX_LDO(L36,      "8917_l36", 0, 1, 1800000, 1800000, 200, NULL,
		0, 8),

	/*
	 *           ID     name   always_on  min_uV   max_uV en_t supply reg_ID
	 */
	PM8XXX_BOOST(BOOST, "8917_boost", 0,  5000000, 5000000, 500, NULL, 9),

	/*	     ID        name      always_on pd en_t supply    reg_ID */
	PM8XXX_VS300(USB_OTG,  "8921_usb_otg",  0, 1, 0,   "8917_boost", 10),
};

static struct rpm_regulator_init_data
apq8064_rpm_regulator_init_data[] __devinitdata = {
	/*	ID a_on pd ss min_uV   max_uV  supply sys_uA  freq  fm  ss_fm */
	RPM_SMPS(S1, 1, 1, 0, 1225000, 1225000, NULL, 100000, 3p20, NONE, NONE),
	RPM_SMPS(S2, 0, 1, 0, 1300000, 1300000, NULL,      0, 1p60, NONE, NONE),
	RPM_SMPS(S3, 0, 1, 1,  500000, 1150000, NULL, 100000, 4p80, NONE, NONE),
	RPM_SMPS(S4, 1, 1, 0, 1800000, 1800000, NULL, 100000, 1p60, AUTO, AUTO),
	RPM_SMPS(S7, 0, 0, 0, 1300000, 1300000, NULL, 100000, 3p20, NONE, NONE),
	RPM_SMPS(S8, 0, 1, 0, 2200000, 2200000, NULL,      0, 1p60, NONE, NONE),

	/*	ID a_on pd ss min_uV   max_uV   supply    sys_uA init_ip */
	RPM_LDO(L1,  1, 1, 0, 1100000, 1100000, "8921_s4",     0,  1000),
	RPM_LDO(L2,  0, 1, 0, 1200000, 1200000, "8921_s4",     0,     0),
	RPM_LDO(L3,  0, 1, 0, 3075000, 3075000, NULL,          0,     0),
	RPM_LDO(L4,  1, 1, 0, 1800000, 1800000, NULL,          0, 10000),
	RPM_LDO(L5,  0, 1, 0, 2950000, 2950000, NULL,          0,     0),
	RPM_LDO(L6,  0, 1, 0, 2950000, 2950000, NULL,          0,     0),
	RPM_LDO(L7,  0, 1, 0, 1850000, 2950000, NULL,          0,     0),
	RPM_LDO(L8,  0, 1, 0, 2800000, 2800000, NULL,          0,     0),
	RPM_LDO(L9,  0, 1, 0, 3000000, 3000000, NULL,          0,     0),
	RPM_LDO(L10, 0, 1, 0, 2900000, 2900000, NULL,          0,     0),
	RPM_LDO(L11, 0, 1, 0, 3000000, 3000000, NULL,          0,     0),
	RPM_LDO(L12, 0, 1, 0, 1200000, 1200000, "8921_s4",     0,     0),
	RPM_LDO(L13, 0, 0, 0, 2220000, 2220000, NULL,          0,     0),
	RPM_LDO(L14, 0, 1, 0, 1800000, 1800000, NULL,          0,     0),
	RPM_LDO(L15, 0, 1, 0, 1800000, 2950000, NULL,          0,     0),
	RPM_LDO(L16, 0, 1, 0, 2800000, 2800000, NULL,          0,     0),
	RPM_LDO(L17, 0, 1, 0, 2000000, 2000000, NULL,          0,     0),
	RPM_LDO(L18, 0, 1, 0, 1300000, 1800000, "8921_s4",     0,     0),
	RPM_LDO(L21, 0, 1, 0, 1050000, 1050000, NULL,          0,     0),
	RPM_LDO(L22, 0, 1, 0, 2600000, 2600000, NULL,          0,     0),
	RPM_LDO(L23, 0, 1, 0, 1800000, 1800000, NULL,          0,     0),
	RPM_LDO(L24, 0, 1, 1,  750000, 1150000, "8921_s1", 10000, 10000),
	RPM_LDO(L25, 1, 1, 0, 1250000, 1250000, "8921_s1", 10000, 10000),
	RPM_LDO(L27, 0, 0, 0, 1100000, 1100000, "8921_s7",     0,     0),
	RPM_LDO(L28, 0, 1, 0, 1050000, 1050000, "8921_s7",     0,     0),
	RPM_LDO(L29, 0, 1, 0, 2000000, 2000000, NULL,          0,     0),

	/*     ID  a_on pd ss                   supply */
	RPM_VS(LVS1, 0, 1, 0,                   "8921_s4"),
	RPM_VS(LVS3, 0, 1, 0,                   "8921_s4"),
	RPM_VS(LVS4, 0, 1, 0,                   "8921_s4"),
	RPM_VS(LVS5, 0, 1, 0,                   "8921_s4"),
	RPM_VS(LVS6, 0, 1, 0,                   "8921_s4"),
	RPM_VS(LVS7, 0, 1, 1,                   "8921_s4"),
};

static struct rpm_regulator_init_data
apq8064_rpm_regulator_pm8921_init_data[] __devinitdata = {
	/*     ID  a_on pd ss                   supply */
	RPM_VS(LVS2, 0, 1, 0,                   "8921_s1"),

	/*	ID a_on    ss min_uV   max_uV   supply     freq */
	RPM_NCP(NCP, 0,    0, 1800000, 1800000, "8921_l6", 1p60),
};

int msm8064_pm8921_regulator_pdata_len __devinitdata =
	ARRAY_SIZE(msm8064_pm8921_regulator_pdata);
int msm8064_pm8917_regulator_pdata_len __devinitdata =
	ARRAY_SIZE(msm8064_pm8917_regulator_pdata);

#define RPM_REG_MAP(_id, _sleep_also, _voter, _supply, _dev_name) \
	{ \
		.vreg_id = RPM_VREG_ID_PM8921_##_id, \
		.sleep_also = _sleep_also, \
		.voter = _voter, \
		.supply = _supply, \
		.dev_name = _dev_name, \
	}
static struct rpm_regulator_consumer_mapping
	      msm_rpm_regulator_consumer_mapping[] __devinitdata = {
	RPM_REG_MAP(LVS7, 0, 1, "krait0_hfpll", "acpuclk-8064"),
	RPM_REG_MAP(LVS7, 0, 2, "krait1_hfpll", "acpuclk-8064"),
	RPM_REG_MAP(LVS7, 0, 4, "krait2_hfpll", "acpuclk-8064"),
	RPM_REG_MAP(LVS7, 0, 5, "krait3_hfpll", "acpuclk-8064"),
	RPM_REG_MAP(LVS7, 0, 6, "l2_hfpll",     "acpuclk-8064"),
	RPM_REG_MAP(L24,  0, 1, "krait0_mem",   "acpuclk-8064"),
	RPM_REG_MAP(L24,  0, 2, "krait1_mem",   "acpuclk-8064"),
	RPM_REG_MAP(L24,  0, 4, "krait2_mem",   "acpuclk-8064"),
	RPM_REG_MAP(L24,  0, 5, "krait3_mem",   "acpuclk-8064"),
	RPM_REG_MAP(S3,   0, 1, "krait0_dig",   "acpuclk-8064"),
	RPM_REG_MAP(S3,   0, 2, "krait1_dig",   "acpuclk-8064"),
	RPM_REG_MAP(S3,   0, 4, "krait2_dig",   "acpuclk-8064"),
	RPM_REG_MAP(S3,   0, 5, "krait3_dig",   "acpuclk-8064"),
};

struct rpm_regulator_platform_data apq8064_rpm_regulator_pdata __devinitdata = {
	.init_data		  = apq8064_rpm_regulator_init_data,
	.num_regulators		  = ARRAY_SIZE(apq8064_rpm_regulator_init_data),
	.version		  = RPM_VREG_VERSION_8960,
	.vreg_id_vdd_mem	  = RPM_VREG_ID_PM8921_L24,
	.vreg_id_vdd_dig	  = RPM_VREG_ID_PM8921_S3,
	.requires_tcxo_workaround = true,
	.consumer_map		  = msm_rpm_regulator_consumer_mapping,
	.consumer_map_len = ARRAY_SIZE(msm_rpm_regulator_consumer_mapping),
};

/* Regulators that are only present when using PM8921 */
struct rpm_regulator_platform_data
apq8064_rpm_regulator_pm8921_pdata __devinitdata = {
	.init_data		  = apq8064_rpm_regulator_pm8921_init_data,
	.num_regulators	= ARRAY_SIZE(apq8064_rpm_regulator_pm8921_init_data),
	.version		  = RPM_VREG_VERSION_8960,
	.vreg_id_vdd_mem	  = RPM_VREG_ID_PM8921_L24,
	.vreg_id_vdd_dig	  = RPM_VREG_ID_PM8921_S3,
	.requires_tcxo_workaround = true,
};

/*
 * Fix up regulator consumer data that moves to a different regulator when
 * PM8917 is used.
 */
void __init configure_apq8064_pm8917_power_grid(void)
{
	static struct rpm_regulator_init_data *rpm_data;
	int i;

	for (i = 0; i < ARRAY_SIZE(apq8064_rpm_regulator_init_data); i++) {
		rpm_data = &apq8064_rpm_regulator_init_data[i];
		if (rpm_data->id == RPM_VREG_ID_PM8921_S1) {
			rpm_data->init_data.consumer_supplies
				= vreg_consumers_8917_S1;
			rpm_data->init_data.num_consumer_supplies
				= ARRAY_SIZE(vreg_consumers_8917_S1);
		}
	}

	/*
	 * Switch to 8960_PM8917 rpm-regulator version so that TCXO workaround
	 * is applied to PM8917 regulators L25, L26, L27, and L28.
	 */
	apq8064_rpm_regulator_pdata.version = RPM_VREG_VERSION_8960_PM8917;
}
