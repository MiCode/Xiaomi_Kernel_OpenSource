// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>

#include "pinctrl-msm.h"

#define FUNCTION(fname)			                \
	[msm_mux_##fname] = {		                \
		.name = #fname,				\
		.groups = fname##_groups,               \
		.ngroups = ARRAY_SIZE(fname##_groups),	\
	}

#define REG_BASE 0x0
#define REG_SIZE 0x1000
#define PINGROUP(id, f1, f2, f3, f4, f5, f6, f7, f8, f9)	\
	{					        \
		.name = "gpio" #id,			\
		.pins = gpio##id##_pins,		\
		.npins = (unsigned int)ARRAY_SIZE(gpio##id##_pins),	\
		.funcs = (int[]){			\
			msm_mux_gpio, /* gpio mode */	\
			msm_mux_##f1,			\
			msm_mux_##f2,			\
			msm_mux_##f3,			\
			msm_mux_##f4,			\
			msm_mux_##f5,			\
			msm_mux_##f6,			\
			msm_mux_##f7,			\
			msm_mux_##f8,			\
			msm_mux_##f9			\
		},				        \
		.nfuncs = 10,				\
		.ctl_reg = REG_BASE + REG_SIZE * id,			\
		.io_reg = REG_BASE + 0x4 + REG_SIZE * id,		\
		.intr_cfg_reg = REG_BASE + 0x8 + REG_SIZE * id,		\
		.intr_status_reg = REG_BASE + 0xc + REG_SIZE * id,	\
		.intr_target_reg = REG_BASE + 0x8 + REG_SIZE * id,	\
		.mux_bit = 2,			\
		.pull_bit = 0,			\
		.drv_bit = 6,			\
		.oe_bit = 9,			\
		.in_bit = 0,			\
		.out_bit = 1,			\
		.intr_enable_bit = 0,		\
		.intr_status_bit = 0,		\
		.intr_target_bit = 5,		\
		.intr_target_kpss_val = 3,	\
		.intr_raw_status_bit = 4,	\
		.intr_polarity_bit = 1,		\
		.intr_detection_bit = 2,	\
		.intr_detection_width = 2,	\
	}

#define SDC_QDSD_PINGROUP(pg_name, ctl, pull, drv)	\
	{					        \
		.name = #pg_name,			\
		.pins = pg_name##_pins,			\
		.npins = (unsigned int)ARRAY_SIZE(pg_name##_pins),	\
		.ctl_reg = ctl,				\
		.io_reg = 0,				\
		.intr_cfg_reg = 0,			\
		.intr_status_reg = 0,			\
		.intr_target_reg = 0,			\
		.mux_bit = -1,				\
		.pull_bit = pull,			\
		.drv_bit = drv,				\
		.oe_bit = -1,				\
		.in_bit = -1,				\
		.out_bit = -1,				\
		.intr_enable_bit = -1,			\
		.intr_status_bit = -1,			\
		.intr_target_bit = -1,			\
		.intr_raw_status_bit = -1,		\
		.intr_polarity_bit = -1,		\
		.intr_detection_bit = -1,		\
		.intr_detection_width = -1,		\
	}

#define UFS_RESET(pg_name, offset)				\
	{					        \
		.name = #pg_name,			\
		.pins = pg_name##_pins,			\
		.npins = (unsigned int)ARRAY_SIZE(pg_name##_pins),	\
		.ctl_reg = offset,			\
		.io_reg = offset + 0x4,			\
		.intr_cfg_reg = 0,			\
		.intr_status_reg = 0,			\
		.intr_target_reg = 0,			\
		.mux_bit = -1,				\
		.pull_bit = 3,				\
		.drv_bit = 0,				\
		.oe_bit = -1,				\
		.in_bit = -1,				\
		.out_bit = 0,				\
		.intr_enable_bit = -1,			\
		.intr_status_bit = -1,			\
		.intr_target_bit = -1,			\
		.intr_raw_status_bit = -1,		\
		.intr_polarity_bit = -1,		\
		.intr_detection_bit = -1,		\
		.intr_detection_width = -1,		\
	}
static const struct pinctrl_pin_desc sdxlemur_pins[] = {
	PINCTRL_PIN(0, "GPIO_0"),
	PINCTRL_PIN(1, "GPIO_1"),
	PINCTRL_PIN(2, "GPIO_2"),
	PINCTRL_PIN(3, "GPIO_3"),
	PINCTRL_PIN(4, "GPIO_4"),
	PINCTRL_PIN(5, "GPIO_5"),
	PINCTRL_PIN(6, "GPIO_6"),
	PINCTRL_PIN(7, "GPIO_7"),
	PINCTRL_PIN(8, "GPIO_8"),
	PINCTRL_PIN(9, "GPIO_9"),
	PINCTRL_PIN(10, "GPIO_10"),
	PINCTRL_PIN(11, "GPIO_11"),
	PINCTRL_PIN(12, "GPIO_12"),
	PINCTRL_PIN(13, "GPIO_13"),
	PINCTRL_PIN(14, "GPIO_14"),
	PINCTRL_PIN(15, "GPIO_15"),
	PINCTRL_PIN(16, "GPIO_16"),
	PINCTRL_PIN(17, "GPIO_17"),
	PINCTRL_PIN(18, "GPIO_18"),
	PINCTRL_PIN(19, "GPIO_19"),
	PINCTRL_PIN(20, "GPIO_20"),
	PINCTRL_PIN(21, "GPIO_21"),
	PINCTRL_PIN(22, "GPIO_22"),
	PINCTRL_PIN(23, "GPIO_23"),
	PINCTRL_PIN(24, "GPIO_24"),
	PINCTRL_PIN(25, "GPIO_25"),
	PINCTRL_PIN(26, "GPIO_26"),
	PINCTRL_PIN(27, "GPIO_27"),
	PINCTRL_PIN(28, "GPIO_28"),
	PINCTRL_PIN(29, "GPIO_29"),
	PINCTRL_PIN(30, "GPIO_30"),
	PINCTRL_PIN(31, "GPIO_31"),
	PINCTRL_PIN(32, "GPIO_32"),
	PINCTRL_PIN(33, "GPIO_33"),
	PINCTRL_PIN(34, "GPIO_34"),
	PINCTRL_PIN(35, "GPIO_35"),
	PINCTRL_PIN(36, "GPIO_36"),
	PINCTRL_PIN(37, "GPIO_37"),
	PINCTRL_PIN(38, "GPIO_38"),
	PINCTRL_PIN(39, "GPIO_39"),
	PINCTRL_PIN(40, "GPIO_40"),
	PINCTRL_PIN(41, "GPIO_41"),
	PINCTRL_PIN(42, "GPIO_42"),
	PINCTRL_PIN(43, "GPIO_43"),
	PINCTRL_PIN(44, "GPIO_44"),
	PINCTRL_PIN(45, "GPIO_45"),
	PINCTRL_PIN(46, "GPIO_46"),
	PINCTRL_PIN(47, "GPIO_47"),
	PINCTRL_PIN(48, "GPIO_48"),
	PINCTRL_PIN(49, "GPIO_49"),
	PINCTRL_PIN(50, "GPIO_50"),
	PINCTRL_PIN(51, "GPIO_51"),
	PINCTRL_PIN(52, "GPIO_52"),
	PINCTRL_PIN(53, "GPIO_53"),
	PINCTRL_PIN(54, "GPIO_54"),
	PINCTRL_PIN(55, "GPIO_55"),
	PINCTRL_PIN(56, "GPIO_56"),
	PINCTRL_PIN(57, "GPIO_57"),
	PINCTRL_PIN(58, "GPIO_58"),
	PINCTRL_PIN(59, "GPIO_59"),
	PINCTRL_PIN(60, "GPIO_60"),
	PINCTRL_PIN(61, "GPIO_61"),
	PINCTRL_PIN(62, "GPIO_62"),
	PINCTRL_PIN(63, "GPIO_63"),
	PINCTRL_PIN(64, "GPIO_64"),
	PINCTRL_PIN(65, "GPIO_65"),
	PINCTRL_PIN(66, "GPIO_66"),
	PINCTRL_PIN(67, "GPIO_67"),
	PINCTRL_PIN(68, "GPIO_68"),
	PINCTRL_PIN(69, "GPIO_69"),
	PINCTRL_PIN(70, "GPIO_70"),
	PINCTRL_PIN(71, "GPIO_71"),
	PINCTRL_PIN(72, "GPIO_72"),
	PINCTRL_PIN(73, "GPIO_73"),
	PINCTRL_PIN(74, "GPIO_74"),
	PINCTRL_PIN(75, "GPIO_75"),
	PINCTRL_PIN(76, "GPIO_76"),
	PINCTRL_PIN(77, "GPIO_77"),
	PINCTRL_PIN(78, "GPIO_78"),
	PINCTRL_PIN(79, "GPIO_79"),
	PINCTRL_PIN(80, "GPIO_80"),
	PINCTRL_PIN(81, "GPIO_81"),
	PINCTRL_PIN(82, "GPIO_82"),
	PINCTRL_PIN(83, "GPIO_83"),
	PINCTRL_PIN(84, "GPIO_84"),
	PINCTRL_PIN(85, "GPIO_85"),
	PINCTRL_PIN(86, "GPIO_86"),
	PINCTRL_PIN(87, "GPIO_87"),
	PINCTRL_PIN(88, "GPIO_88"),
	PINCTRL_PIN(89, "GPIO_89"),
	PINCTRL_PIN(90, "GPIO_90"),
	PINCTRL_PIN(91, "GPIO_91"),
	PINCTRL_PIN(92, "GPIO_92"),
	PINCTRL_PIN(93, "GPIO_93"),
	PINCTRL_PIN(94, "GPIO_94"),
	PINCTRL_PIN(95, "GPIO_95"),
	PINCTRL_PIN(96, "GPIO_96"),
	PINCTRL_PIN(97, "GPIO_97"),
	PINCTRL_PIN(98, "GPIO_98"),
	PINCTRL_PIN(99, "GPIO_99"),
	PINCTRL_PIN(100, "GPIO_100"),
	PINCTRL_PIN(101, "GPIO_101"),
	PINCTRL_PIN(102, "GPIO_102"),
	PINCTRL_PIN(103, "GPIO_103"),
	PINCTRL_PIN(104, "GPIO_104"),
	PINCTRL_PIN(105, "GPIO_105"),
	PINCTRL_PIN(106, "GPIO_106"),
	PINCTRL_PIN(107, "GPIO_107"),
	PINCTRL_PIN(108, "SDC1_RCLK"),
	PINCTRL_PIN(109, "SDC1_CLK"),
	PINCTRL_PIN(110, "SDC1_CMD"),
	PINCTRL_PIN(111, "SDC1_DATA"),
	PINCTRL_PIN(112, "UFS_RESET"),
};

#define DECLARE_MSM_GPIO_PINS(pin) \
	static const unsigned int gpio##pin##_pins[] = { pin }
DECLARE_MSM_GPIO_PINS(0);
DECLARE_MSM_GPIO_PINS(1);
DECLARE_MSM_GPIO_PINS(2);
DECLARE_MSM_GPIO_PINS(3);
DECLARE_MSM_GPIO_PINS(4);
DECLARE_MSM_GPIO_PINS(5);
DECLARE_MSM_GPIO_PINS(6);
DECLARE_MSM_GPIO_PINS(7);
DECLARE_MSM_GPIO_PINS(8);
DECLARE_MSM_GPIO_PINS(9);
DECLARE_MSM_GPIO_PINS(10);
DECLARE_MSM_GPIO_PINS(11);
DECLARE_MSM_GPIO_PINS(12);
DECLARE_MSM_GPIO_PINS(13);
DECLARE_MSM_GPIO_PINS(14);
DECLARE_MSM_GPIO_PINS(15);
DECLARE_MSM_GPIO_PINS(16);
DECLARE_MSM_GPIO_PINS(17);
DECLARE_MSM_GPIO_PINS(18);
DECLARE_MSM_GPIO_PINS(19);
DECLARE_MSM_GPIO_PINS(20);
DECLARE_MSM_GPIO_PINS(21);
DECLARE_MSM_GPIO_PINS(22);
DECLARE_MSM_GPIO_PINS(23);
DECLARE_MSM_GPIO_PINS(24);
DECLARE_MSM_GPIO_PINS(25);
DECLARE_MSM_GPIO_PINS(26);
DECLARE_MSM_GPIO_PINS(27);
DECLARE_MSM_GPIO_PINS(28);
DECLARE_MSM_GPIO_PINS(29);
DECLARE_MSM_GPIO_PINS(30);
DECLARE_MSM_GPIO_PINS(31);
DECLARE_MSM_GPIO_PINS(32);
DECLARE_MSM_GPIO_PINS(33);
DECLARE_MSM_GPIO_PINS(34);
DECLARE_MSM_GPIO_PINS(35);
DECLARE_MSM_GPIO_PINS(36);
DECLARE_MSM_GPIO_PINS(37);
DECLARE_MSM_GPIO_PINS(38);
DECLARE_MSM_GPIO_PINS(39);
DECLARE_MSM_GPIO_PINS(40);
DECLARE_MSM_GPIO_PINS(41);
DECLARE_MSM_GPIO_PINS(42);
DECLARE_MSM_GPIO_PINS(43);
DECLARE_MSM_GPIO_PINS(44);
DECLARE_MSM_GPIO_PINS(45);
DECLARE_MSM_GPIO_PINS(46);
DECLARE_MSM_GPIO_PINS(47);
DECLARE_MSM_GPIO_PINS(48);
DECLARE_MSM_GPIO_PINS(49);
DECLARE_MSM_GPIO_PINS(50);
DECLARE_MSM_GPIO_PINS(51);
DECLARE_MSM_GPIO_PINS(52);
DECLARE_MSM_GPIO_PINS(53);
DECLARE_MSM_GPIO_PINS(54);
DECLARE_MSM_GPIO_PINS(55);
DECLARE_MSM_GPIO_PINS(56);
DECLARE_MSM_GPIO_PINS(57);
DECLARE_MSM_GPIO_PINS(58);
DECLARE_MSM_GPIO_PINS(59);
DECLARE_MSM_GPIO_PINS(60);
DECLARE_MSM_GPIO_PINS(61);
DECLARE_MSM_GPIO_PINS(62);
DECLARE_MSM_GPIO_PINS(63);
DECLARE_MSM_GPIO_PINS(64);
DECLARE_MSM_GPIO_PINS(65);
DECLARE_MSM_GPIO_PINS(66);
DECLARE_MSM_GPIO_PINS(67);
DECLARE_MSM_GPIO_PINS(68);
DECLARE_MSM_GPIO_PINS(69);
DECLARE_MSM_GPIO_PINS(70);
DECLARE_MSM_GPIO_PINS(71);
DECLARE_MSM_GPIO_PINS(72);
DECLARE_MSM_GPIO_PINS(73);
DECLARE_MSM_GPIO_PINS(74);
DECLARE_MSM_GPIO_PINS(75);
DECLARE_MSM_GPIO_PINS(76);
DECLARE_MSM_GPIO_PINS(77);
DECLARE_MSM_GPIO_PINS(78);
DECLARE_MSM_GPIO_PINS(79);
DECLARE_MSM_GPIO_PINS(80);
DECLARE_MSM_GPIO_PINS(81);
DECLARE_MSM_GPIO_PINS(82);
DECLARE_MSM_GPIO_PINS(83);
DECLARE_MSM_GPIO_PINS(84);
DECLARE_MSM_GPIO_PINS(85);
DECLARE_MSM_GPIO_PINS(86);
DECLARE_MSM_GPIO_PINS(87);
DECLARE_MSM_GPIO_PINS(88);
DECLARE_MSM_GPIO_PINS(89);
DECLARE_MSM_GPIO_PINS(90);
DECLARE_MSM_GPIO_PINS(91);
DECLARE_MSM_GPIO_PINS(92);
DECLARE_MSM_GPIO_PINS(93);
DECLARE_MSM_GPIO_PINS(94);
DECLARE_MSM_GPIO_PINS(95);
DECLARE_MSM_GPIO_PINS(96);
DECLARE_MSM_GPIO_PINS(97);
DECLARE_MSM_GPIO_PINS(98);
DECLARE_MSM_GPIO_PINS(99);
DECLARE_MSM_GPIO_PINS(100);
DECLARE_MSM_GPIO_PINS(101);
DECLARE_MSM_GPIO_PINS(102);
DECLARE_MSM_GPIO_PINS(103);
DECLARE_MSM_GPIO_PINS(104);
DECLARE_MSM_GPIO_PINS(105);
DECLARE_MSM_GPIO_PINS(106);
DECLARE_MSM_GPIO_PINS(107);

static const unsigned int sdc1_rclk_pins[] = { 108 };
static const unsigned int sdc1_clk_pins[] = { 109 };
static const unsigned int sdc1_cmd_pins[] = { 110 };
static const unsigned int sdc1_data_pins[] = { 111 };
static const unsigned int ufs_reset_pins[] = { 112 };

enum sdxlemur_functions {
	msm_mux_gpio,
	msm_mux_uim2_data,
	msm_mux_blsp_uart1,
	msm_mux_ebi0_wrcdc,
	msm_mux_uim2_present,
	msm_mux_uim2_reset,
	msm_mux_blsp_i2c1,
	msm_mux_uim2_clk,
	msm_mux_blsp_spi2,
	msm_mux_blsp_uart2,
	msm_mux_qdss_gpio3,
	msm_mux_qdss_gpio2,
	msm_mux_blsp_i2c2,
	msm_mux_char_exec,
	msm_mux_qdss_gpio1,
	msm_mux_qdss_gpio0,
	msm_mux_blsp_spi3,
	msm_mux_blsp_uart3,
	msm_mux_ext_dbg,
	msm_mux_ldo_en,
	msm_mux_blsp_i2c3,
	msm_mux_gcc_gp3,
	msm_mux_pri_mi2s_ws,
	msm_mux_qdss_gpio12,
	msm_mux_pri_mi2s,
	msm_mux_qdss_gpio13,
	msm_mux_vsense_trigger,
	msm_mux_qdss_gpio14,
	msm_mux_native_tsens,
	msm_mux_bimc_dte0,
	msm_mux_qdss_gpio15,
	msm_mux_bimc_dte1,
	msm_mux_sec_mi2s,
	msm_mux_blsp_spi4,
	msm_mux_blsp_uart4,
	msm_mux_qdss_cti,
	msm_mux_qdss_gpio8,
	msm_mux_qdss_gpio9,
	msm_mux_blsp_i2c4,
	msm_mux_gcc_gp1,
	msm_mux_qdss_gpio10,
	msm_mux_jitter_bist,
	msm_mux_gcc_gp2,
	msm_mux_qdss_gpio11,
	msm_mux_pll_bist,
	msm_mux_blsp_spi1,
	msm_mux_adsp_ext,
	msm_mux_native_char3,
	msm_mux_QLINK0_WMSS,
	msm_mux_native_tsense,
	msm_mux_native_char2,
	msm_mux_nav_gpio,
	msm_mux_pll_ref,
	msm_mux_pa_indicator,
	msm_mux_qdss_gpio,
	msm_mux_native_char0,
	msm_mux_qlink0_en,
	msm_mux_qlink0_req,
	msm_mux_dbg_out,
	msm_mux_cri_trng,
	msm_mux_prng_rosc,
	msm_mux_cri_trng0,
	msm_mux_cri_trng1,
	msm_mux_native_char1,
	msm_mux_coex_uart,
	msm_mux_ddr_pxi0,
	msm_mux_m_voc,
	msm_mux_ddr_bist,
	msm_mux_pci_e,
	msm_mux_tgu_ch0,
	msm_mux_pcie_clkreq,
	msm_mux_native_char,
	msm_mux_mgpi_clk,
	msm_mux_qlink2_wmss,
	msm_mux_i2s_mclk,
	msm_mux_audio_ref,
	msm_mux_ldo_update,
	msm_mux_qdss_gpio4,
	msm_mux_atest_char,
	msm_mux_qdss_gpio5,
	msm_mux_atest_char3,
	msm_mux_qdss_gpio6,
	msm_mux_atest_char2,
	msm_mux_qdss_gpio7,
	msm_mux_atest_char1,
	msm_mux_uim1_data,
	msm_mux_atest_char0,
	msm_mux_uim1_present,
	msm_mux_uim1_reset,
	msm_mux_uim1_clk,
	msm_mux_qlink2_en,
	msm_mux_qlink1_en,
	msm_mux_qlink1_req,
	msm_mux_qlink1_wmss,
	msm_mux_coex_uart2,
	msm_mux_spmi_coex,
	msm_mux_qlink2_req,
	msm_mux_spmi_vgi,
	msm_mux_gcc_plltest,
	msm_mux_ebi2_lcd,
	msm_mux_usb2phy_ac,
	msm_mux_sdc1_tb,
	msm_mux_NA,
};

static const char * const gpio_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3", "gpio4", "gpio5", "gpio6", "gpio7",
	"gpio8", "gpio9", "gpio10", "gpio11", "gpio12", "gpio13", "gpio14",
	"gpio15", "gpio16", "gpio17", "gpio18", "gpio19", "gpio20", "gpio21",
	"gpio22", "gpio23", "gpio24", "gpio25", "gpio26", "gpio27", "gpio28",
	"gpio29", "gpio30", "gpio31", "gpio32", "gpio33", "gpio34", "gpio35",
	"gpio36", "gpio37", "gpio38", "gpio39", "gpio40", "gpio41", "gpio42",
	"gpio43", "gpio44", "gpio45", "gpio46", "gpio47", "gpio48", "gpio49",
	"gpio50", "gpio51", "gpio52", "gpio53", "gpio54", "gpio55", "gpio56",
	"gpio57", "gpio58", "gpio59", "gpio60", "gpio61", "gpio62", "gpio63",
	"gpio64", "gpio65", "gpio66", "gpio67", "gpio68", "gpio69", "gpio70",
	"gpio71", "gpio72", "gpio73", "gpio74", "gpio75", "gpio76", "gpio77",
	"gpio78", "gpio79", "gpio80", "gpio81", "gpio82", "gpio83", "gpio84",
	"gpio85", "gpio86", "gpio87", "gpio88", "gpio89", "gpio90", "gpio91",
	"gpio92", "gpio93", "gpio94", "gpio95", "gpio96", "gpio97", "gpio98",
	"gpio99", "gpio100", "gpio101", "gpio102", "gpio103", "gpio104",
	"gpio105", "gpio106", "gpio107",
};
static const char * const uim2_data_groups[] = {
	"gpio0",
};
static const char * const blsp_uart1_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3", "gpio48", "gpio49", "gpio80",
	"gpio81",
};
static const char * const ebi0_wrcdc_groups[] = {
	"gpio0", "gpio2",
};
static const char * const uim2_present_groups[] = {
	"gpio1",
};
static const char * const uim2_reset_groups[] = {
	"gpio2",
};
static const char * const blsp_i2c1_groups[] = {
	"gpio2", "gpio3", "gpio82", "gpio83",
};
static const char * const uim2_clk_groups[] = {
	"gpio3",
};
static const char * const blsp_spi2_groups[] = {
	"gpio4", "gpio5", "gpio6", "gpio7", "gpio23", "gpio47", "gpio62",
};
static const char * const blsp_uart2_groups[] = {
	"gpio4", "gpio5", "gpio6", "gpio7", "gpio63", "gpio64", "gpio65",
	"gpio66",
};
static const char * const qdss_gpio3_groups[] = {
	"gpio4",
};
static const char * const qdss_gpio2_groups[] = {
	"gpio5",
};
static const char * const blsp_i2c2_groups[] = {
	"gpio6", "gpio7", "gpio65", "gpio66",
};
static const char * const char_exec_groups[] = {
	"gpio6", "gpio7",
};
static const char * const qdss_gpio1_groups[] = {
	"gpio6",
};
static const char * const qdss_gpio0_groups[] = {
	"gpio7",
};
static const char * const blsp_spi3_groups[] = {
	"gpio8", "gpio9", "gpio10", "gpio11", "gpio23", "gpio47", "gpio62",
};
static const char * const blsp_uart3_groups[] = {
	"gpio8", "gpio9", "gpio10", "gpio11",
};
static const char * const ext_dbg_groups[] = {
	"gpio8", "gpio9", "gpio10", "gpio11",
};
static const char * const ldo_en_groups[] = {
	"gpio8",
};
static const char * const blsp_i2c3_groups[] = {
	"gpio10", "gpio11",
};
static const char * const gcc_gp3_groups[] = {
	"gpio11",
};
static const char * const pri_mi2s_ws_groups[] = {
	"gpio12",
};
static const char * const qdss_gpio12_groups[] = {
	"gpio12",
};
static const char * const pri_mi2s_groups[] = {
	"gpio13", "gpio14", "gpio15",
};
static const char * const qdss_gpio13_groups[] = {
	"gpio13",
};
static const char * const vsense_trigger_groups[] = {
	"gpio13",
};
static const char * const qdss_gpio14_groups[] = {
	"gpio14",
};
static const char * const native_tsens_groups[] = {
	"gpio14",
};
static const char * const bimc_dte0_groups[] = {
	"gpio14", "gpio59",
};
static const char * const qdss_gpio15_groups[] = {
	"gpio15",
};
static const char * const bimc_dte1_groups[] = {
	"gpio15", "gpio61",
};
static const char * const sec_mi2s_groups[] = {
	"gpio16", "gpio17", "gpio18", "gpio19",
};
static const char * const blsp_spi4_groups[] = {
	"gpio16", "gpio17", "gpio18", "gpio19", "gpio23", "gpio47", "gpio62",
};
static const char * const blsp_uart4_groups[] = {
	"gpio16", "gpio17", "gpio18", "gpio19", "gpio22", "gpio23", "gpio48",
	"gpio49",
};
static const char * const qdss_cti_groups[] = {
	"gpio16", "gpio16", "gpio17", "gpio17", "gpio54", "gpio54", "gpio55",
	"gpio55", "gpio59", "gpio60", "gpio65", "gpio65", "gpio66", "gpio66",
	"gpio94", "gpio94", "gpio95", "gpio95",
};
static const char * const qdss_gpio8_groups[] = {
	"gpio16",
};
static const char * const qdss_gpio9_groups[] = {
	"gpio17",
};
static const char * const blsp_i2c4_groups[] = {
	"gpio18", "gpio19", "gpio84", "gpio85",
};
static const char * const gcc_gp1_groups[] = {
	"gpio18",
};
static const char * const qdss_gpio10_groups[] = {
	"gpio18",
};
static const char * const jitter_bist_groups[] = {
	"gpio19",
};
static const char * const gcc_gp2_groups[] = {
	"gpio19",
};
static const char * const qdss_gpio11_groups[] = {
	"gpio19",
};
static const char * const pll_bist_groups[] = {
	"gpio22",
};
static const char * const blsp_spi1_groups[] = {
	"gpio23", "gpio47", "gpio62", "gpio80", "gpio81", "gpio82", "gpio83",
};
static const char * const adsp_ext_groups[] = {
	"gpio24", "gpio25",
};
static const char * const native_char3_groups[] = {
	"gpio26",
};
static const char * const QLINK0_WMSS_groups[] = {
	"gpio28",
};
static const char * const native_tsense_groups[] = {
	"gpio29", "gpio72",
};
static const char * const native_char2_groups[] = {
	"gpio29",
};
static const char * const nav_gpio_groups[] = {
	"gpio31", "gpio32",
};
static const char * const pll_ref_groups[] = {
	"gpio32",
};
static const char * const pa_indicator_groups[] = {
	"gpio33",
};
static const char * const qdss_gpio_groups[] = {
	"gpio33", "gpio42",
};
static const char * const native_char0_groups[] = {
	"gpio33",
};
static const char * const qlink0_en_groups[] = {
	"gpio34",
};
static const char * const qlink0_req_groups[] = {
	"gpio35",
};
static const char * const dbg_out_groups[] = {
	"gpio35",
};
static const char * const cri_trng_groups[] = {
	"gpio36",
};
static const char * const prng_rosc_groups[] = {
	"gpio38",
};
static const char * const cri_trng0_groups[] = {
	"gpio40",
};
static const char * const cri_trng1_groups[] = {
	"gpio41",
};
static const char * const native_char1_groups[] = {
	"gpio42",
};
static const char * const coex_uart_groups[] = {
	"gpio44", "gpio45",
};
static const char * const ddr_pxi0_groups[] = {
	"gpio45", "gpio46",
};
static const char * const m_voc_groups[] = {
	"gpio46", "gpio48", "gpio49", "gpio59", "gpio60",
};
static const char * const ddr_bist_groups[] = {
	"gpio46", "gpio47", "gpio48", "gpio49",
};
static const char * const pci_e_groups[] = {
	"gpio53",
};
static const char * const tgu_ch0_groups[] = {
	"gpio55",
};
static const char * const pcie_clkreq_groups[] = {
	"gpio56",
};
static const char * const native_char_groups[] = {
	"gpio57",
};
static const char * const mgpi_clk_groups[] = {
	"gpio61", "gpio71",
};
static const char * const qlink2_wmss_groups[] = {
	"gpio61",
};
static const char * const i2s_mclk_groups[] = {
	"gpio62",
};
static const char * const audio_ref_groups[] = {
	"gpio62",
};
static const char * const ldo_update_groups[] = {
	"gpio62",
};
static const char * const qdss_gpio4_groups[] = {
	"gpio63",
};
static const char * const atest_char_groups[] = {
	"gpio63",
};
static const char * const qdss_gpio5_groups[] = {
	"gpio64",
};
static const char * const atest_char3_groups[] = {
	"gpio64",
};
static const char * const qdss_gpio6_groups[] = {
	"gpio65",
};
static const char * const atest_char2_groups[] = {
	"gpio65",
};
static const char * const qdss_gpio7_groups[] = {
	"gpio66",
};
static const char * const atest_char1_groups[] = {
	"gpio66",
};
static const char * const uim1_data_groups[] = {
	"gpio67",
};
static const char * const atest_char0_groups[] = {
	"gpio67",
};
static const char * const uim1_present_groups[] = {
	"gpio68",
};
static const char * const uim1_reset_groups[] = {
	"gpio69",
};
static const char * const uim1_clk_groups[] = {
	"gpio70",
};
static const char * const qlink2_en_groups[] = {
	"gpio71",
};
static const char * const qlink1_en_groups[] = {
	"gpio72",
};
static const char * const qlink1_req_groups[] = {
	"gpio73",
};
static const char * const qlink1_wmss_groups[] = {
	"gpio74",
};
static const char * const coex_uart2_groups[] = {
	"gpio75", "gpio76", "gpio102", "gpio103",
};
static const char * const spmi_coex_groups[] = {
	"gpio75", "gpio76",
};
static const char * const qlink2_req_groups[] = {
	"gpio77",
};
static const char * const spmi_vgi_groups[] = {
	"gpio78", "gpio79",
};
static const char * const gcc_plltest_groups[] = {
	"gpio81", "gpio82",
};
static const char * const ebi2_lcd_groups[] = {
	"gpio84", "gpio85",
};
static const char * const usb2phy_ac_groups[] = {
	"gpio93",
};
static const char * const sdc1_tb_groups[] = {
	"gpio106",
};

static const struct msm_function sdxlemur_functions[] = {
	FUNCTION(gpio),
	FUNCTION(uim2_data),
	FUNCTION(blsp_uart1),
	FUNCTION(ebi0_wrcdc),
	FUNCTION(uim2_present),
	FUNCTION(uim2_reset),
	FUNCTION(blsp_i2c1),
	FUNCTION(uim2_clk),
	FUNCTION(blsp_spi2),
	FUNCTION(blsp_uart2),
	FUNCTION(qdss_gpio3),
	FUNCTION(qdss_gpio2),
	FUNCTION(blsp_i2c2),
	FUNCTION(char_exec),
	FUNCTION(qdss_gpio1),
	FUNCTION(qdss_gpio0),
	FUNCTION(blsp_spi3),
	FUNCTION(blsp_uart3),
	FUNCTION(ext_dbg),
	FUNCTION(ldo_en),
	FUNCTION(blsp_i2c3),
	FUNCTION(gcc_gp3),
	FUNCTION(pri_mi2s_ws),
	FUNCTION(qdss_gpio12),
	FUNCTION(pri_mi2s),
	FUNCTION(qdss_gpio13),
	FUNCTION(vsense_trigger),
	FUNCTION(qdss_gpio14),
	FUNCTION(native_tsens),
	FUNCTION(bimc_dte0),
	FUNCTION(qdss_gpio15),
	FUNCTION(bimc_dte1),
	FUNCTION(sec_mi2s),
	FUNCTION(blsp_spi4),
	FUNCTION(blsp_uart4),
	FUNCTION(qdss_cti),
	FUNCTION(qdss_gpio8),
	FUNCTION(qdss_gpio9),
	FUNCTION(blsp_i2c4),
	FUNCTION(gcc_gp1),
	FUNCTION(qdss_gpio10),
	FUNCTION(jitter_bist),
	FUNCTION(gcc_gp2),
	FUNCTION(qdss_gpio11),
	FUNCTION(pll_bist),
	FUNCTION(blsp_spi1),
	FUNCTION(adsp_ext),
	FUNCTION(native_char3),
	FUNCTION(QLINK0_WMSS),
	FUNCTION(native_tsense),
	FUNCTION(native_char2),
	FUNCTION(nav_gpio),
	FUNCTION(pll_ref),
	FUNCTION(pa_indicator),
	FUNCTION(qdss_gpio),
	FUNCTION(native_char0),
	FUNCTION(qlink0_en),
	FUNCTION(qlink0_req),
	FUNCTION(dbg_out),
	FUNCTION(cri_trng),
	FUNCTION(prng_rosc),
	FUNCTION(cri_trng0),
	FUNCTION(cri_trng1),
	FUNCTION(native_char1),
	FUNCTION(coex_uart),
	FUNCTION(ddr_pxi0),
	FUNCTION(m_voc),
	FUNCTION(ddr_bist),
	FUNCTION(pci_e),
	FUNCTION(tgu_ch0),
	FUNCTION(pcie_clkreq),
	FUNCTION(native_char),
	FUNCTION(mgpi_clk),
	FUNCTION(qlink2_wmss),
	FUNCTION(i2s_mclk),
	FUNCTION(audio_ref),
	FUNCTION(ldo_update),
	FUNCTION(qdss_gpio4),
	FUNCTION(atest_char),
	FUNCTION(qdss_gpio5),
	FUNCTION(atest_char3),
	FUNCTION(qdss_gpio6),
	FUNCTION(atest_char2),
	FUNCTION(qdss_gpio7),
	FUNCTION(atest_char1),
	FUNCTION(uim1_data),
	FUNCTION(atest_char0),
	FUNCTION(uim1_present),
	FUNCTION(uim1_reset),
	FUNCTION(uim1_clk),
	FUNCTION(qlink2_en),
	FUNCTION(qlink1_en),
	FUNCTION(qlink1_req),
	FUNCTION(qlink1_wmss),
	FUNCTION(coex_uart2),
	FUNCTION(spmi_coex),
	FUNCTION(qlink2_req),
	FUNCTION(spmi_vgi),
	FUNCTION(gcc_plltest),
	FUNCTION(ebi2_lcd),
	FUNCTION(usb2phy_ac),
	FUNCTION(sdc1_tb),
};

/* Every pin is maintained as a single group, and missing or non-existing pin
 * would be maintained as dummy group to synchronize pin group index with
 * pin descriptor registered with pinctrl core.
 * Clients would not be able to request these dummy pin groups.
 */
static const struct msm_pingroup sdxlemur_groups[] = {
	[0] = PINGROUP(0, uim2_data, blsp_uart1, ebi0_wrcdc, NA, NA, NA, NA,
		       NA, NA),
	[1] = PINGROUP(1, uim2_present, blsp_uart1, NA, NA, NA, NA, NA, NA, NA),
	[2] = PINGROUP(2, uim2_reset, blsp_uart1, blsp_i2c1, ebi0_wrcdc, NA,
		       NA, NA, NA, NA),
	[3] = PINGROUP(3, uim2_clk, blsp_uart1, blsp_i2c1, NA, NA, NA, NA, NA,
		       NA),
	[4] = PINGROUP(4, blsp_spi2, blsp_uart2, NA, qdss_gpio3, NA, NA, NA,
		       NA, NA),
	[5] = PINGROUP(5, blsp_spi2, blsp_uart2, NA, qdss_gpio2, NA, NA, NA,
		       NA, NA),
	[6] = PINGROUP(6, blsp_spi2, blsp_uart2, blsp_i2c2, char_exec, NA,
		       qdss_gpio1, NA, NA, NA),
	[7] = PINGROUP(7, blsp_spi2, blsp_uart2, blsp_i2c2, char_exec, NA,
		       qdss_gpio0, NA, NA, NA),
	[8] = PINGROUP(8, blsp_spi3, blsp_uart3, ext_dbg, ldo_en, NA, NA, NA,
		       NA, NA),
	[9] = PINGROUP(9, blsp_spi3, blsp_uart3, ext_dbg, NA, NA, NA, NA, NA,
		       NA),
	[10] = PINGROUP(10, blsp_spi3, blsp_uart3, blsp_i2c3, ext_dbg, NA, NA,
			NA, NA, NA),
	[11] = PINGROUP(11, blsp_spi3, blsp_uart3, blsp_i2c3, ext_dbg, gcc_gp3,
			NA, NA, NA, NA),
	[12] = PINGROUP(12, pri_mi2s_ws, NA, qdss_gpio12, NA, NA, NA, NA, NA,
			NA),
	[13] = PINGROUP(13, pri_mi2s, NA, qdss_gpio13, vsense_trigger, NA, NA,
			NA, NA, NA),
	[14] = PINGROUP(14, pri_mi2s, NA, NA, qdss_gpio14, native_tsens,
			bimc_dte0, NA, NA, NA),
	[15] = PINGROUP(15, pri_mi2s, NA, NA, qdss_gpio15, bimc_dte1, NA, NA,
			NA, NA),
	[16] = PINGROUP(16, sec_mi2s, blsp_spi4, blsp_uart4, qdss_cti,
			qdss_cti, NA, NA, qdss_gpio8, NA),
	[17] = PINGROUP(17, sec_mi2s, blsp_spi4, blsp_uart4, qdss_cti,
			qdss_cti, NA, qdss_gpio9, NA, NA),
	[18] = PINGROUP(18, sec_mi2s, blsp_spi4, blsp_uart4, blsp_i2c4,
			gcc_gp1, qdss_gpio10, NA, NA, NA),
	[19] = PINGROUP(19, sec_mi2s, blsp_spi4, blsp_uart4, blsp_i2c4,
			jitter_bist, gcc_gp2, NA, qdss_gpio11, NA),
	[20] = PINGROUP(20, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[21] = PINGROUP(21, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[22] = PINGROUP(22, blsp_uart4, pll_bist, NA, NA, NA, NA, NA, NA, NA),
	[23] = PINGROUP(23, blsp_uart4, blsp_spi2, blsp_spi1, blsp_spi3,
			blsp_spi4, NA, NA, NA, NA),
	[24] = PINGROUP(24, adsp_ext, NA, NA, NA, NA, NA, NA, NA, NA),
	[25] = PINGROUP(25, adsp_ext, NA, NA, NA, NA, NA, NA, NA, NA),
	[26] = PINGROUP(26, NA, NA, NA, native_char3, NA, NA, NA, NA, NA),
	[27] = PINGROUP(27, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[28] = PINGROUP(28, QLINK0_WMSS, NA, NA, NA, NA, NA, NA, NA, NA),
	[29] = PINGROUP(29, NA, NA, NA, native_tsense, native_char2, NA, NA,
			NA, NA),
	[30] = PINGROUP(30, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[31] = PINGROUP(31, nav_gpio, NA, NA, NA, NA, NA, NA, NA, NA),
	[32] = PINGROUP(32, nav_gpio, pll_ref, NA, NA, NA, NA, NA, NA, NA),
	[33] = PINGROUP(33, NA, pa_indicator, qdss_gpio, native_char0, NA, NA,
			NA, NA, NA),
	[34] = PINGROUP(34, qlink0_en, NA, NA, NA, NA, NA, NA, NA, NA),
	[35] = PINGROUP(35, qlink0_req, dbg_out, NA, NA, NA, NA, NA, NA, NA),
	[36] = PINGROUP(36, NA, NA, cri_trng, NA, NA, NA, NA, NA, NA),
	[37] = PINGROUP(37, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[38] = PINGROUP(38, NA, NA, prng_rosc, NA, NA, NA, NA, NA, NA),
	[39] = PINGROUP(39, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[40] = PINGROUP(40, NA, NA, cri_trng0, NA, NA, NA, NA, NA, NA),
	[41] = PINGROUP(41, NA, NA, cri_trng1, NA, NA, NA, NA, NA, NA),
	[42] = PINGROUP(42, NA, qdss_gpio, native_char1, NA, NA, NA, NA, NA,
			NA),
	[43] = PINGROUP(43, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[44] = PINGROUP(44, coex_uart, NA, NA, NA, NA, NA, NA, NA, NA),
	[45] = PINGROUP(45, coex_uart, ddr_pxi0, NA, NA, NA, NA, NA, NA, NA),
	[46] = PINGROUP(46, m_voc, ddr_bist, ddr_pxi0, NA, NA, NA, NA, NA, NA),
	[47] = PINGROUP(47, ddr_bist, blsp_spi1, blsp_spi2, blsp_spi3,
			blsp_spi4, NA, NA, NA, NA),
	[48] = PINGROUP(48, m_voc, blsp_uart1, blsp_uart4, ddr_bist, NA, NA,
			NA, NA, NA),
	[49] = PINGROUP(49, m_voc, blsp_uart1, blsp_uart4, ddr_bist, NA, NA,
			NA, NA, NA),
	[50] = PINGROUP(50, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[51] = PINGROUP(51, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[52] = PINGROUP(52, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[53] = PINGROUP(53, pci_e, NA, NA, NA, NA, NA, NA, NA, NA),
	[54] = PINGROUP(54, qdss_cti, qdss_cti, NA, NA, NA, NA, NA, NA, NA),
	[55] = PINGROUP(55, qdss_cti, qdss_cti, tgu_ch0, NA, NA, NA, NA, NA,
			NA),
	[56] = PINGROUP(56, pcie_clkreq, NA, NA, NA, NA, NA, NA, NA, NA),
	[57] = PINGROUP(57, NA, native_char, NA, NA, NA, NA, NA, NA, NA),
	[58] = PINGROUP(58, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[59] = PINGROUP(59, qdss_cti, m_voc, bimc_dte0, NA, NA, NA, NA, NA, NA),
	[60] = PINGROUP(60, qdss_cti, NA, m_voc, NA, NA, NA, NA, NA, NA),
	[61] = PINGROUP(61, mgpi_clk, qlink2_wmss, bimc_dte1, NA, NA, NA, NA,
			NA, NA),
	[62] = PINGROUP(62, i2s_mclk, audio_ref, blsp_spi1, blsp_spi2,
			blsp_spi3, blsp_spi4, ldo_update, NA, NA),
	[63] = PINGROUP(63, blsp_uart2, NA, qdss_gpio4, atest_char, NA, NA, NA,
			NA, NA),
	[64] = PINGROUP(64, blsp_uart2, qdss_gpio5, atest_char3, NA, NA, NA,
			NA, NA, NA),
	[65] = PINGROUP(65, blsp_uart2, blsp_i2c2, qdss_cti, qdss_cti, NA,
			qdss_gpio6, atest_char2, NA, NA),
	[66] = PINGROUP(66, blsp_uart2, blsp_i2c2, qdss_cti, qdss_cti,
			qdss_gpio7, atest_char1, NA, NA, NA),
	[67] = PINGROUP(67, uim1_data, atest_char0, NA, NA, NA, NA, NA, NA, NA),
	[68] = PINGROUP(68, uim1_present, NA, NA, NA, NA, NA, NA, NA, NA),
	[69] = PINGROUP(69, uim1_reset, NA, NA, NA, NA, NA, NA, NA, NA),
	[70] = PINGROUP(70, uim1_clk, NA, NA, NA, NA, NA, NA, NA, NA),
	[71] = PINGROUP(71, mgpi_clk, qlink2_en, NA, NA, NA, NA, NA, NA, NA),
	[72] = PINGROUP(72, qlink1_en, NA, native_tsense, NA, NA, NA, NA, NA,
			NA),
	[73] = PINGROUP(73, qlink1_req, NA, NA, NA, NA, NA, NA, NA, NA),
	[74] = PINGROUP(74, qlink1_wmss, NA, NA, NA, NA, NA, NA, NA, NA),
	[75] = PINGROUP(75, coex_uart2, spmi_coex, NA, NA, NA, NA, NA, NA, NA),
	[76] = PINGROUP(76, coex_uart2, spmi_coex, NA, NA, NA, NA, NA, NA, NA),
	[77] = PINGROUP(77, NA, qlink2_req, NA, NA, NA, NA, NA, NA, NA),
	[78] = PINGROUP(78, spmi_vgi, NA, NA, NA, NA, NA, NA, NA, NA),
	[79] = PINGROUP(79, spmi_vgi, NA, NA, NA, NA, NA, NA, NA, NA),
	[80] = PINGROUP(80, NA, blsp_spi1, NA, blsp_uart1, NA, NA, NA, NA, NA),
	[81] = PINGROUP(81, NA, blsp_spi1, NA, blsp_uart1, gcc_plltest, NA, NA,
			NA, NA),
	[82] = PINGROUP(82, NA, blsp_spi1, NA, blsp_i2c1, gcc_plltest, NA, NA,
			NA, NA),
	[83] = PINGROUP(83, NA, blsp_spi1, NA, blsp_i2c1, NA, NA, NA, NA, NA),
	[84] = PINGROUP(84, NA, ebi2_lcd, NA, blsp_i2c4, NA, NA, NA, NA, NA),
	[85] = PINGROUP(85, NA, ebi2_lcd, NA, blsp_i2c4, NA, NA, NA, NA, NA),
	[86] = PINGROUP(86, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[87] = PINGROUP(87, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[88] = PINGROUP(88, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[89] = PINGROUP(89, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[90] = PINGROUP(90, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[91] = PINGROUP(91, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[92] = PINGROUP(92, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[93] = PINGROUP(93, NA, NA, usb2phy_ac, NA, NA, NA, NA, NA, NA),
	[94] = PINGROUP(94, qdss_cti, qdss_cti, NA, NA, NA, NA, NA, NA, NA),
	[95] = PINGROUP(95, qdss_cti, qdss_cti, NA, NA, NA, NA, NA, NA, NA),
	[96] = PINGROUP(96, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[97] = PINGROUP(97, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[98] = PINGROUP(98, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[99] = PINGROUP(99, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[100] = PINGROUP(100, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[101] = PINGROUP(101, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[102] = PINGROUP(102, NA, NA, coex_uart2, NA, NA, NA, NA, NA, NA),
	[103] = PINGROUP(103, NA, NA, coex_uart2, NA, NA, NA, NA, NA, NA),
	[104] = PINGROUP(104, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[105] = PINGROUP(105, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[106] = PINGROUP(106, sdc1_tb, NA, NA, NA, NA, NA, NA, NA, NA),
	[107] = PINGROUP(107, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[108] = SDC_QDSD_PINGROUP(sdc1_rclk, 0x9a000, 15, 0),
	[109] = SDC_QDSD_PINGROUP(sdc1_clk, 0x9a000, 13, 6),
	[110] = SDC_QDSD_PINGROUP(sdc1_cmd, 0x9a000, 11, 3),
	[111] = SDC_QDSD_PINGROUP(sdc1_data, 0x9a000, 9, 0),
	[112] = UFS_RESET(ufs_reset, 0x0),
};

static const struct msm_gpio_wakeirq_map sdxlemur_pdc_map[] = {
	{1, 20}, {2, 21}, {5, 22}, {6, 23}, {9, 24}, {10, 25},
	{11, 26}, {12, 27}, {13, 28}, {14, 29}, {15, 30}, {16, 31},
	{17, 32}, {18, 33}, {19, 34}, {21, 35}, {22, 36}, {23, 70},
	{24, 37}, {25, 38}, {35, 40}, {43, 41}, {46, 44}, {48, 45},
	{49, 57}, {50, 46}, {52, 47}, {54, 49}, {55, 50}, {60, 53},
	{61, 54}, {64, 55}, {65, 81}, {68, 56}, {71, 58}, {73, 59},
	{77, 77}, {81, 65}, {83, 63}, {84, 64}, {86, 66}, {88, 67},
	{89, 68}, {90, 69}, {93, 71}, {94, 72}, {95, 73}, {96, 74},
	{99, 75}, {103, 78}, {104, 79}
};

static const struct msm_pinctrl_soc_data sdxlemur_pinctrl = {
	.pins = sdxlemur_pins,
	.npins = ARRAY_SIZE(sdxlemur_pins),
	.functions = sdxlemur_functions,
	.nfunctions = ARRAY_SIZE(sdxlemur_functions),
	.groups = sdxlemur_groups,
	.ngroups = ARRAY_SIZE(sdxlemur_groups),
	.ngpios = 108,
	.wakeirq_map = sdxlemur_pdc_map,
	.nwakeirq_map = ARRAY_SIZE(sdxlemur_pdc_map),
};

static int sdxlemur_pinctrl_probe(struct platform_device *pdev)
{
	return msm_pinctrl_probe(pdev, &sdxlemur_pinctrl);
}

static const struct of_device_id sdxlemur_pinctrl_of_match[] = {
	{ .compatible = "qcom,sdxlemur-pinctrl", },
	{ },
};

static struct platform_driver sdxlemur_pinctrl_driver = {
	.driver = {
		.name = "sdxlemur-pinctrl",
		.of_match_table = sdxlemur_pinctrl_of_match,
	},
	.probe = sdxlemur_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init sdxlemur_pinctrl_init(void)
{
	return platform_driver_register(&sdxlemur_pinctrl_driver);
}
arch_initcall(sdxlemur_pinctrl_init);

static void __exit sdxlemur_pinctrl_exit(void)
{
	platform_driver_unregister(&sdxlemur_pinctrl_driver);
}
module_exit(sdxlemur_pinctrl_exit);

MODULE_DESCRIPTION("QTI sdxlemur pinctrl driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, sdxlemur_pinctrl_of_match);
