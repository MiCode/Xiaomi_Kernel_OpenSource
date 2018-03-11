/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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
		.intr_target_kpss_val = 4,       \
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
static const struct pinctrl_pin_desc msm8909_pins[] = {
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
	PINCTRL_PIN(108, "GPIO_108"),
	PINCTRL_PIN(109, "GPIO_109"),
	PINCTRL_PIN(110, "GPIO_110"),
	PINCTRL_PIN(111, "GPIO_111"),
	PINCTRL_PIN(112, "GPIO_112"),
	PINCTRL_PIN(113, "SDC1_CLK"),
	PINCTRL_PIN(114, "SDC1_CMD"),
	PINCTRL_PIN(115, "SDC1_DATA"),
	PINCTRL_PIN(116, "SDC2_CLK"),
	PINCTRL_PIN(117, "SDC2_CMD"),
	PINCTRL_PIN(118, "SDC2_DATA"),
	PINCTRL_PIN(119, "QDSD_CLK"),
	PINCTRL_PIN(120, "QDSD_CMD"),
	PINCTRL_PIN(121, "QDSD_DATA0"),
	PINCTRL_PIN(122, "QDSD_DATA1"),
	PINCTRL_PIN(123, "QDSD_DATA2"),
	PINCTRL_PIN(124, "QDSD_DATA3"),
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
DECLARE_MSM_GPIO_PINS(108);
DECLARE_MSM_GPIO_PINS(109);
DECLARE_MSM_GPIO_PINS(110);
DECLARE_MSM_GPIO_PINS(111);
DECLARE_MSM_GPIO_PINS(112);

static const unsigned int sdc1_clk_pins[] = { 113 };
static const unsigned int sdc1_cmd_pins[] = { 114 };
static const unsigned int sdc1_data_pins[] = { 115 };
static const unsigned int sdc2_clk_pins[] = { 116 };
static const unsigned int sdc2_cmd_pins[] = { 117 };
static const unsigned int sdc2_data_pins[] = { 118 };
static const unsigned int qdsd_clk_pins[] = { 119 };
static const unsigned int qdsd_cmd_pins[] = { 120 };
static const unsigned int qdsd_data0_pins[] = { 121 };
static const unsigned int qdsd_data1_pins[] = { 122 };
static const unsigned int qdsd_data2_pins[] = { 123 };
static const unsigned int qdsd_data3_pins[] = { 124 };

enum msm8909_functions {
	msm_mux_blsp_spi3,
	msm_mux_gpio,
	msm_mux_sec_mi2s,
	msm_mux_blsp_spi1,
	msm_mux_blsp_uart1,
	msm_mux_blsp_uim1,
	msm_mux_blsp3_spi,
	msm_mux_dmic0_clk,
	msm_mux_qdss_tracectl_b,
	msm_mux_blsp2_spi,
	msm_mux_dmic0_data,
	msm_mux_qdss_traceclk_b,
	msm_mux_blsp_i2c1,
	msm_mux_qdss_tracedata_a,
	msm_mux_bimc_dte0,
	msm_mux_bimc_dte1,
	msm_mux_blsp_spi6,
	msm_mux_m_voc,
	msm_mux_blsp_i2c6,
	msm_mux_dbg_out,
	msm_mux_blsp_spi4,
	msm_mux_gcc_gp2_clk_b,
	msm_mux_gcc_gp3_clk_b,
	msm_mux_blsp_i2c4,
	msm_mux_gcc_gp1_clk_b,
	msm_mux_qdss_tracedata_b,
	msm_mux_blsp_spi5,
	msm_mux_blsp_i2c5,
	msm_mux_uim3_clk,
	msm_mux_qdss_cti_trig_out_a0,
	msm_mux_mdp_vsync,
	msm_mux_ebi2_lcd,
	msm_mux_dsi_rst,
	msm_mux_cam_mclk,
	msm_mux_uim3_data,
	msm_mux_blsp_spi2,
	msm_mux_blsp_uart2,
	msm_mux_blsp_uim2,
	msm_mux_qdss_cti_trig_in_a0,
	msm_mux_uim3_present,
	msm_mux_qdss_cti_trig_in_b0,
	msm_mux_uim3_reset,
	msm_mux_qdss_cti_trig_out_b0,
	msm_mux_webcam1_rst,
	msm_mux_pwr_modem_enabled_a,
	msm_mux_blsp_i2c3,
	msm_mux_flash_strobe,
	msm_mux_cci_timer0,
	msm_mux_cci_timer1,
	msm_mux_atest_combodac_to_gpio_native,
	msm_mux_cci_async,
	msm_mux_cam1_standby,
	msm_mux_pwr_nav_enabled_a,
	msm_mux_cam1_rst,
	msm_mux_pwr_crypto_enabled_a,
	msm_mux_atest_bbrx1,
	msm_mux_backlight_en,
	msm_mux_blsp1_spi,
	msm_mux_atest_bbrx0,
	msm_mux_sd_card,
	msm_mux_cci_timer2,
	msm_mux_adsp_ext,
	msm_mux_wcss_bt,
	msm_mux_wcss_wlan2,
	msm_mux_wcss_wlan1,
	msm_mux_wcss_wlan0,
	msm_mux_wcss_wlan,
	msm_mux_prng_rosc,
	msm_mux_wcss_fm,
	msm_mux_ext_lpass,
	msm_mux_qdss_tracectl_a,
	msm_mux_qdss_traceclk_a,
	msm_mux_uim2_data,
	msm_mux_gcc_gp1_clk_a,
	msm_mux_qdss_cti_trig_in_a1,
	msm_mux_uim2_clk,
	msm_mux_gcc_gp2_clk_a,
	msm_mux_qdss_cti_trig_in_b1,
	msm_mux_uim2_reset,
	msm_mux_gcc_gp3_clk_a,
	msm_mux_qdss_cti_trig_out_b1,
	msm_mux_uim2_present,
	msm_mux_qdss_cti_trig_out_a1,
	msm_mux_uim1_data,
	msm_mux_uim1_clk,
	msm_mux_uim1_reset,
	msm_mux_uim1_present,
	msm_mux_uim_batt,
	msm_mux_smb_int,
	msm_mux_cdc_pdm0,
	msm_mux_pri_mi2s_mclk_a,
	msm_mux_atest_char3,
	msm_mux_pri_mi2s_sck_a,
	msm_mux_atest_char2,
	msm_mux_pri_mi2s_ws_a,
	msm_mux_atest_char1,
	msm_mux_pri_mi2s_data0_a,
	msm_mux_atest_char0,
	msm_mux_atest_gpsadc_dtest0_native,
	msm_mux_gcc_plltest,
	msm_mux_pri_mi2s_data1_a,
	msm_mux_atest_char,
	msm_mux_atest_tsens,
	msm_mux_ebi0_wrcdc,
	msm_mux_mag_int,
	msm_mux_atest_gpsadc_dtest1_native,
	msm_mux_pa_indicator,
	msm_mux_modem_tsync,
	msm_mux_nav_tsync,
	msm_mux_nav_pps,
	msm_mux_gsm0_tx,
	msm_mux_ssbi0,
	msm_mux_ssbi1,
	msm_mux_kpsns0,
	msm_mux_pbs0,
	msm_mux_kpsns1,
	msm_mux_pbs1,
	msm_mux_kpsns2,
	msm_mux_pbs2,
	msm_mux_ext_buck,
	msm_mux_alsp_int,
	msm_mux_pri_mi2s_sck_b,
	msm_mux_pwr_modem_enabled_b,
	msm_mux_pri_mi2s_data0_b,
	msm_mux_pwr_nav_enabled_b,
	msm_mux_gyro_accl,
	msm_mux_pri_mi2s_data1_b,
	msm_mux_pwr_crypto_enabled_b,
	msm_mux_atest_wlan0,
	msm_mux_euro_us,
	msm_mux_atest_wlan1,
	msm_mux_pri_mi2s_mclk_b,
	msm_mux_ldo_update,
	msm_mux_gcc_tlmm,
	msm_mux_ebi2_a,
	msm_mux_sd_write,
	msm_mux_ldo_en,
	msm_mux_msim_int,
	msm_mux_pri_mi2s_ws_b,
	msm_mux_blsp_i2c2,
	msm_mux_NA,
};

static const char * const blsp_spi3_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3",
};
static const char * const gpio_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3", "gpio4", "gpio5", "gpio6", "gpio7",
	"gpio8", "gpio9", "gpio10", "gpio11", "gpio12", "gpio13", "gpio14",
	"gpio15", "gpio16", "gpio17", "gpio18", "gpio19", "gpio20", "gpio21",
	"gpio22", "gpio23", "gpio24", "gpio25", "gpio26", "gpio27", "gpio28",
	"gpio29", "gpio30", "gpio31", "gpio32", "gpio33", "gpio34", "gpio35",
	"gpio36", "gpio37", "gpio38", "gpio39", "gpio40", "gpio41", "gpio42",
	"gpio43", "gpio44", "gpio45", "gpio46", "gpio47", "gpio48", "gpio49",
	"gpio50",  "gpio51", "gpio52", "gpio53", "gpio54", "gpio55", "gpio56",
	"gpio57", "gpio58", "gpio59", "gpio60", "gpio61", "gpio62", "gpio63",
	"gpio64",  "gpio65", "gpio66", "gpio67", "gpio68", "gpio69", "gpio70",
	"gpio71", "gpio72", "gpio73", "gpio74", "gpio75", "gpio76", "gpio77",
	"gpio78", "gpio79", "gpio80", "gpio81", "gpio82", "gpio83", "gpio84",
	"gpio85", "gpio86", "gpio87", "gpio88", "gpio89", "gpio90", "gpio91",
	"gpio92", "gpio93", "gpio94", "gpio95", "gpio96", "gpio97", "gpio98",
	"gpio99", "gpio100", "gpio101", "gpio102", "gpio103", "gpio104",
	"gpio105", "gpio106", "gpio107", "gpio108", "gpio109", "gpio110",
	"gpio111", "gpio112",
};
static const char * const sec_mi2s_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3", "gpio98",
};
static const char * const blsp_spi1_groups[] = {
	"gpio4", "gpio5", "gpio6", "gpio7",
};
static const char * const blsp_uart1_groups[] = {
	"gpio4", "gpio5", "gpio6", "gpio7",
};
static const char * const blsp_uim1_groups[] = {
	"gpio4", "gpio5",
};
static const char * const blsp3_spi_groups[] = {
	"gpio4", "gpio65", "gpio95",
};
static const char * const dmic0_clk_groups[] = {
	"gpio4",
};
static const char * const qdss_tracectl_b_groups[] = {
	"gpio4",
};
static const char * const blsp2_spi_groups[] = {
	"gpio5", "gpio17", "gpio98",
};
static const char * const dmic0_data_groups[] = {
	"gpio5",
};
static const char * const qdss_traceclk_b_groups[] = {
	"gpio5",
};
static const char * const blsp_i2c1_groups[] = {
	"gpio6", "gpio7",
};
static const char * const qdss_tracedata_a_groups[] = {
	"gpio6", "gpio8", "gpio9", "gpio10", "gpio39", "gpio40", "gpio41",
	"gpio42", "gpio43", "gpio47", "gpio48", "gpio58", "gpio65", "gpio94",
	"gpio96", "gpio97",
};
static const char * const bimc_dte0_groups[] = {
	"gpio6", "gpio59",
};
static const char * const bimc_dte1_groups[] = {
	"gpio7", "gpio60",
};
static const char * const blsp_spi6_groups[] = {
	"gpio8", "gpio9", "gpio10", "gpio11",
};
static const char * const m_voc_groups[] = {
	"gpio8", "gpio95",
};
static const char * const blsp_i2c6_groups[] = {
	"gpio10", "gpio11",
};
static const char * const dbg_out_groups[] = {
	"gpio10",
};
static const char * const blsp_spi4_groups[] = {
	"gpio12", "gpio13", "gpio14", "gpio15",
};
static const char * const gcc_gp2_clk_b_groups[] = {
	"gpio12",
};
static const char * const gcc_gp3_clk_b_groups[] = {
	"gpio13",
};
static const char * const blsp_i2c4_groups[] = {
	"gpio14", "gpio15",
};
static const char * const gcc_gp1_clk_b_groups[] = {
	"gpio14",
};
static const char * const qdss_tracedata_b_groups[] = {
	"gpio14", "gpio16", "gpio17", "gpio26", "gpio27", "gpio28", "gpio29",
	"gpio30", "gpio31", "gpio32", "gpio33", "gpio34", "gpio35", "gpio36",
	"gpio37", "gpio93",
};
static const char * const blsp_spi5_groups[] = {
	"gpio16", "gpio17", "gpio18", "gpio19",
};
static const char * const blsp_i2c5_groups[] = {
	"gpio18", "gpio19",
};
static const char * const uim3_clk_groups[] = {
	"gpio23",
};
static const char * const qdss_cti_trig_out_a0_groups[] = {
	"gpio23",
};
static const char * const mdp_vsync_groups[] = {
	"gpio24", "gpio25",
};
static const char * const ebi2_lcd_groups[] = {
	"gpio24", "gpio24", "gpio25", "gpio95",
};
static const char * const dsi_rst_groups[] = {
	"gpio25",
};
static const char * const cam_mclk_groups[] = {
	"gpio26", "gpio27",
};
static const char * const uim3_data_groups[] = {
	"gpio20",
};
static const char * const blsp_spi2_groups[] = {
	"gpio20", "gpio21", "gpio111", "gpio112",
};
static const char * const blsp_uart2_groups[] = {
	"gpio20", "gpio21", "gpio111", "gpio112",
};
static const char * const blsp_uim2_groups[] = {
	"gpio20", "gpio21",
};
static const char * const qdss_cti_trig_in_a0_groups[] = {
	"gpio20",
};
static const char * const uim3_present_groups[] = {
	"gpio21",
};
static const char * const qdss_cti_trig_in_b0_groups[] = {
	"gpio21",
};
static const char * const uim3_reset_groups[] = {
	"gpio22",
};
static const char * const qdss_cti_trig_out_b0_groups[] = {
	"gpio22",
};
static const char * const webcam1_rst_groups[] = {
	"gpio28",
};
static const char * const pwr_modem_enabled_a_groups[] = {
	"gpio28",
};
static const char * const blsp_i2c3_groups[] = {
	"gpio29", "gpio30",
};
static const char * const flash_strobe_groups[] = {
	"gpio31", "gpio32",
};
static const char * const cci_timer0_groups[] = {
	"gpio31",
};
static const char * const cci_timer1_groups[] = {
	"gpio32",
};
static const char * const atest_combodac_to_gpio_native_groups[] = {
	"gpio32", "gpio38", "gpio39", "gpio40", "gpio41", "gpio42", "gpio43",
	"gpio44", "gpio45", "gpio47", "gpio48", "gpio66", "gpio81", "gpio83",
	"gpio84", "gpio85", "gpio86", "gpio94", "gpio95", "gpio110",
};
static const char * const cci_async_groups[] = {
	"gpio33",
};
static const char * const cam1_standby_groups[] = {
	"gpio34",
};
static const char * const pwr_nav_enabled_a_groups[] = {
	"gpio34",
};
static const char * const cam1_rst_groups[] = {
	"gpio35",
};
static const char * const pwr_crypto_enabled_a_groups[] = {
	"gpio35",
};
static const char * const atest_bbrx1_groups[] = {
	"gpio36",
};
static const char * const backlight_en_groups[] = {
	"gpio37",
};
static const char * const blsp1_spi_groups[] = {
	"gpio37", "gpio65", "gpio97",
};
static const char * const atest_bbrx0_groups[] = {
	"gpio37",
};
static const char * const sd_card_groups[] = {
	"gpio38",
};
static const char * const cci_timer2_groups[] = {
	"gpio38",
};
static const char * const adsp_ext_groups[] = {
	"gpio38",
};
static const char * const wcss_bt_groups[] = {
	"gpio39", "gpio47", "gpio48",
};
static const char * const wcss_wlan2_groups[] = {
	"gpio40",
};
static const char * const wcss_wlan1_groups[] = {
	"gpio41",
};
static const char * const wcss_wlan0_groups[] = {
	"gpio42",
};
static const char * const wcss_wlan_groups[] = {
	"gpio43", "gpio44",
};
static const char * const prng_rosc_groups[] = {
	"gpio43",
};
static const char * const wcss_fm_groups[] = {
	"gpio45", "gpio46",
};
static const char * const ext_lpass_groups[] = {
	"gpio45",
};
static const char * const qdss_tracectl_a_groups[] = {
	"gpio45",
};
static const char * const qdss_traceclk_a_groups[] = {
	"gpio46",
};
static const char * const uim2_data_groups[] = {
	"gpio49",
};
static const char * const gcc_gp1_clk_a_groups[] = {
	"gpio49",
};
static const char * const qdss_cti_trig_in_a1_groups[] = {
	"gpio49",
};
static const char * const uim2_clk_groups[] = {
	"gpio50",
};
static const char * const gcc_gp2_clk_a_groups[] = {
	"gpio50",
};
static const char * const qdss_cti_trig_in_b1_groups[] = {
	"gpio50",
};
static const char * const uim2_reset_groups[] = {
	"gpio51",
};
static const char * const gcc_gp3_clk_a_groups[] = {
	"gpio51",
};
static const char * const qdss_cti_trig_out_b1_groups[] = {
	"gpio51",
};
static const char * const uim2_present_groups[] = {
	"gpio52",
};
static const char * const qdss_cti_trig_out_a1_groups[] = {
	"gpio52",
};
static const char * const uim1_data_groups[] = {
	"gpio53",
};
static const char * const uim1_clk_groups[] = {
	"gpio54",
};
static const char * const uim1_reset_groups[] = {
	"gpio55",
};
static const char * const uim1_present_groups[] = {
	"gpio56",
};
static const char * const uim_batt_groups[] = {
	"gpio57",
};
static const char * const smb_int_groups[] = {
	"gpio58",
};
static const char * const cdc_pdm0_groups[] = {
	"gpio59", "gpio60", "gpio61", "gpio62", "gpio63", "gpio64",
};
static const char * const pri_mi2s_mclk_a_groups[] = {
	"gpio59",
};
static const char * const atest_char3_groups[] = {
	"gpio59",
};
static const char * const pri_mi2s_sck_a_groups[] = {
	"gpio60",
};
static const char * const atest_char2_groups[] = {
	"gpio60",
};
static const char * const pri_mi2s_ws_a_groups[] = {
	"gpio61",
};
static const char * const atest_char1_groups[] = {
	"gpio61",
};
static const char * const pri_mi2s_data0_a_groups[] = {
	"gpio62",
};
static const char * const atest_char0_groups[] = {
	"gpio62",
};
static const char * const atest_gpsadc_dtest0_native_groups[] = {
	"gpio65",
};
static const char * const gcc_plltest_groups[] = {
	"gpio66", "gpio67",
};
static const char * const pri_mi2s_data1_a_groups[] = {
	"gpio63",
};
static const char * const atest_char_groups[] = {
	"gpio63",
};
static const char * const atest_tsens_groups[] = {
	"gpio63",
};
static const char * const ebi0_wrcdc_groups[] = {
	"gpio64",
};
static const char * const mag_int_groups[] = {
	"gpio65",
};
static const char * const atest_gpsadc_dtest1_native_groups[] = {
	"gpio79",
};
static const char * const pa_indicator_groups[] = {
	"gpio82",
};
static const char * const modem_tsync_groups[] = {
	"gpio83",
};
static const char * const nav_tsync_groups[] = {
	"gpio83",
};
static const char * const nav_pps_groups[] = {
	"gpio83",
};
static const char * const gsm0_tx_groups[] = {
	"gpio85",
};
static const char * const ssbi0_groups[] = {
	"gpio88",
};
static const char * const ssbi1_groups[] = {
	"gpio89",
};
static const char * const kpsns0_groups[] = {
	"gpio90",
};
static const char * const pbs0_groups[] = {
	"gpio90",
};
static const char * const kpsns1_groups[] = {
	"gpio91",
};
static const char * const pbs1_groups[] = {
	"gpio91",
};
static const char * const kpsns2_groups[] = {
	"gpio92",
};
static const char * const pbs2_groups[] = {
	"gpio92",
};
static const char * const ext_buck_groups[] = {
	"gpio93",
};
static const char * const alsp_int_groups[] = {
	"gpio94",
};
static const char * const pri_mi2s_sck_b_groups[] = {
	"gpio94",
};
static const char * const pwr_modem_enabled_b_groups[] = {
	"gpio94",
};
static const char * const pri_mi2s_data0_b_groups[] = {
	"gpio95",
};
static const char * const pwr_nav_enabled_b_groups[] = {
	"gpio95",
};
static const char * const gyro_accl_groups[] = {
	"gpio96",
};
static const char * const pri_mi2s_data1_b_groups[] = {
	"gpio96",
};
static const char * const pwr_crypto_enabled_b_groups[] = {
	"gpio96",
};
static const char * const atest_wlan0_groups[] = {
	"gpio96",
};
static const char * const euro_us_groups[] = {
	"gpio97",
};
static const char * const atest_wlan1_groups[] = {
	"gpio97",
};
static const char * const pri_mi2s_mclk_b_groups[] = {
	"gpio98",
};
static const char * const ldo_update_groups[] = {
	"gpio98",
};
static const char * const gcc_tlmm_groups[] = {
	"gpio98",
};
static const char * const ebi2_a_groups[] = {
	"gpio99",
};
static const char * const sd_write_groups[] = {
	"gpio99",
};
static const char * const ldo_en_groups[] = {
	"gpio99",
};
static const char * const msim_int_groups[] = {
	"gpio110",
};
static const char * const pri_mi2s_ws_b_groups[] = {
	"gpio110",
};
static const char * const blsp_i2c2_groups[] = {
	"gpio111", "gpio112",
};

static const struct msm_function msm8909_functions[] = {
	FUNCTION(blsp_spi3),
	FUNCTION(gpio),
	FUNCTION(sec_mi2s),
	FUNCTION(blsp_spi1),
	FUNCTION(blsp_uart1),
	FUNCTION(blsp_uim1),
	FUNCTION(blsp3_spi),
	FUNCTION(dmic0_clk),
	FUNCTION(qdss_tracectl_b),
	FUNCTION(blsp2_spi),
	FUNCTION(dmic0_data),
	FUNCTION(qdss_traceclk_b),
	FUNCTION(blsp_i2c1),
	FUNCTION(qdss_tracedata_a),
	FUNCTION(bimc_dte0),
	FUNCTION(bimc_dte1),
	FUNCTION(blsp_spi6),
	FUNCTION(m_voc),
	FUNCTION(blsp_i2c6),
	FUNCTION(dbg_out),
	FUNCTION(blsp_spi4),
	FUNCTION(gcc_gp2_clk_b),
	FUNCTION(gcc_gp3_clk_b),
	FUNCTION(blsp_i2c4),
	FUNCTION(gcc_gp1_clk_b),
	FUNCTION(qdss_tracedata_b),
	FUNCTION(blsp_spi5),
	FUNCTION(blsp_i2c5),
	FUNCTION(uim3_clk),
	FUNCTION(qdss_cti_trig_out_a0),
	FUNCTION(mdp_vsync),
	FUNCTION(ebi2_lcd),
	FUNCTION(dsi_rst),
	FUNCTION(cam_mclk),
	FUNCTION(uim3_data),
	FUNCTION(blsp_spi2),
	FUNCTION(blsp_uart2),
	FUNCTION(blsp_uim2),
	FUNCTION(qdss_cti_trig_in_a0),
	FUNCTION(uim3_present),
	FUNCTION(qdss_cti_trig_in_b0),
	FUNCTION(uim3_reset),
	FUNCTION(qdss_cti_trig_out_b0),
	FUNCTION(webcam1_rst),
	FUNCTION(pwr_modem_enabled_a),
	FUNCTION(blsp_i2c3),
	FUNCTION(flash_strobe),
	FUNCTION(cci_timer0),
	FUNCTION(cci_timer1),
	FUNCTION(atest_combodac_to_gpio_native),
	FUNCTION(cci_async),
	FUNCTION(cam1_standby),
	FUNCTION(pwr_nav_enabled_a),
	FUNCTION(cam1_rst),
	FUNCTION(pwr_crypto_enabled_a),
	FUNCTION(atest_bbrx1),
	FUNCTION(backlight_en),
	FUNCTION(blsp1_spi),
	FUNCTION(atest_bbrx0),
	FUNCTION(sd_card),
	FUNCTION(cci_timer2),
	FUNCTION(adsp_ext),
	FUNCTION(wcss_bt),
	FUNCTION(wcss_wlan2),
	FUNCTION(wcss_wlan1),
	FUNCTION(wcss_wlan0),
	FUNCTION(wcss_wlan),
	FUNCTION(prng_rosc),
	FUNCTION(wcss_fm),
	FUNCTION(ext_lpass),
	FUNCTION(qdss_tracectl_a),
	FUNCTION(qdss_traceclk_a),
	FUNCTION(uim2_data),
	FUNCTION(gcc_gp1_clk_a),
	FUNCTION(qdss_cti_trig_in_a1),
	FUNCTION(uim2_clk),
	FUNCTION(gcc_gp2_clk_a),
	FUNCTION(qdss_cti_trig_in_b1),
	FUNCTION(uim2_reset),
	FUNCTION(gcc_gp3_clk_a),
	FUNCTION(qdss_cti_trig_out_b1),
	FUNCTION(uim2_present),
	FUNCTION(qdss_cti_trig_out_a1),
	FUNCTION(uim1_data),
	FUNCTION(uim1_clk),
	FUNCTION(uim1_reset),
	FUNCTION(uim1_present),
	FUNCTION(uim_batt),
	FUNCTION(smb_int),
	FUNCTION(cdc_pdm0),
	FUNCTION(pri_mi2s_mclk_a),
	FUNCTION(atest_char3),
	FUNCTION(pri_mi2s_sck_a),
	FUNCTION(atest_char2),
	FUNCTION(pri_mi2s_ws_a),
	FUNCTION(atest_char1),
	FUNCTION(pri_mi2s_data0_a),
	FUNCTION(atest_char0),
	FUNCTION(atest_gpsadc_dtest0_native),
	FUNCTION(gcc_plltest),
	FUNCTION(pri_mi2s_data1_a),
	FUNCTION(atest_char),
	FUNCTION(atest_tsens),
	FUNCTION(ebi0_wrcdc),
	FUNCTION(mag_int),
	FUNCTION(atest_gpsadc_dtest1_native),
	FUNCTION(pa_indicator),
	FUNCTION(modem_tsync),
	FUNCTION(nav_tsync),
	FUNCTION(nav_pps),
	FUNCTION(gsm0_tx),
	FUNCTION(ssbi0),
	FUNCTION(ssbi1),
	FUNCTION(kpsns0),
	FUNCTION(pbs0),
	FUNCTION(kpsns1),
	FUNCTION(pbs1),
	FUNCTION(kpsns2),
	FUNCTION(pbs2),
	FUNCTION(ext_buck),
	FUNCTION(alsp_int),
	FUNCTION(pri_mi2s_sck_b),
	FUNCTION(pwr_modem_enabled_b),
	FUNCTION(pri_mi2s_data0_b),
	FUNCTION(pwr_nav_enabled_b),
	FUNCTION(gyro_accl),
	FUNCTION(pri_mi2s_data1_b),
	FUNCTION(pwr_crypto_enabled_b),
	FUNCTION(atest_wlan0),
	FUNCTION(euro_us),
	FUNCTION(atest_wlan1),
	FUNCTION(pri_mi2s_mclk_b),
	FUNCTION(ldo_update),
	FUNCTION(gcc_tlmm),
	FUNCTION(ebi2_a),
	FUNCTION(sd_write),
	FUNCTION(ldo_en),
	FUNCTION(msim_int),
	FUNCTION(pri_mi2s_ws_b),
	FUNCTION(blsp_i2c2),
};

static const struct msm_pingroup msm8909_groups[] = {
	PINGROUP(0, blsp_spi3, sec_mi2s, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(1, blsp_spi3, sec_mi2s, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(2, blsp_spi3, sec_mi2s, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(3, blsp_spi3, sec_mi2s, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(4, blsp_spi1, blsp_uart1, blsp_uim1, blsp3_spi, dmic0_clk, NA,
		 NA, NA, NA),
	PINGROUP(5, blsp_spi1, blsp_uart1, blsp_uim1, blsp2_spi, dmic0_data,
		 NA, NA, NA, NA),
	PINGROUP(6, blsp_spi1, blsp_uart1, blsp_i2c1, NA, NA, NA, NA, NA, NA),
	PINGROUP(7, blsp_spi1, blsp_uart1, blsp_i2c1, NA, NA, NA, NA, NA,
		 bimc_dte1),
	PINGROUP(8, blsp_spi6, m_voc, NA, NA, NA, NA, NA, qdss_tracedata_a, NA),
	PINGROUP(9, blsp_spi6, NA, NA, NA, NA, NA, qdss_tracedata_a, NA, NA),
	PINGROUP(10, blsp_spi6, blsp_i2c6, dbg_out, qdss_tracedata_a, NA, NA,
		 NA, NA, NA),
	PINGROUP(11, blsp_spi6, blsp_i2c6, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(12, blsp_spi4, gcc_gp2_clk_b, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(13, blsp_spi4, gcc_gp3_clk_b, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(14, blsp_spi4, blsp_i2c4, gcc_gp1_clk_b, NA, NA, NA, NA, NA,
		 qdss_tracedata_b),
	PINGROUP(15, blsp_spi4, blsp_i2c4, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(16, blsp_spi5, NA, NA, NA, NA, NA, qdss_tracedata_b, NA, NA),
	PINGROUP(17, blsp_spi5, blsp2_spi, NA, NA, NA, NA, NA,
		 qdss_tracedata_b, NA),
	PINGROUP(18, blsp_spi5, blsp_i2c5, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(19, blsp_spi5, blsp_i2c5, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(20, uim3_data, blsp_spi2, blsp_uart2, blsp_uim2, NA,
		 qdss_cti_trig_in_a0, NA, NA, NA),
	PINGROUP(21, uim3_present, blsp_spi2, blsp_uart2, blsp_uim2, NA,
		 qdss_cti_trig_in_b0, NA, NA, NA),
	PINGROUP(22, uim3_reset, NA, qdss_cti_trig_out_b0, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(23, uim3_clk, qdss_cti_trig_out_a0, NA, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(24, mdp_vsync, ebi2_lcd, ebi2_lcd, NA, NA, NA, NA, NA, NA),
	PINGROUP(25, mdp_vsync, ebi2_lcd, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(26, cam_mclk, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(27, cam_mclk, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(28, NA, pwr_modem_enabled_a, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(29, blsp_i2c3, NA, NA, NA, NA, NA, qdss_tracedata_b, NA, NA),
	PINGROUP(30, blsp_i2c3, NA, NA, NA, NA, NA, qdss_tracedata_b, NA, NA),
	PINGROUP(31, cci_timer0, NA, NA, NA, NA, NA, NA, qdss_tracedata_b, NA),
	PINGROUP(32, cci_timer1, NA, qdss_tracedata_b, NA,
		 atest_combodac_to_gpio_native, NA, NA, NA, NA),
	PINGROUP(33, cci_async, qdss_tracedata_b, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(34, pwr_nav_enabled_a, qdss_tracedata_b, NA, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(35, pwr_crypto_enabled_a, qdss_tracedata_b, NA, NA, NA, NA,
		 NA, NA, NA),
	PINGROUP(36, qdss_tracedata_b, NA, atest_bbrx1, NA, NA, NA, NA, NA, NA),
	PINGROUP(37, blsp1_spi, qdss_tracedata_b, NA, atest_bbrx0, NA, NA, NA,
		 NA, NA),
	PINGROUP(38, cci_timer2, adsp_ext, NA, atest_combodac_to_gpio_native,
		 NA, NA, NA, NA, NA),
	PINGROUP(39, wcss_bt, qdss_tracedata_a, NA,
		 atest_combodac_to_gpio_native, NA, NA, NA, NA, NA),
	PINGROUP(40, wcss_wlan2, qdss_tracedata_a, NA,
		 atest_combodac_to_gpio_native, NA, NA, NA, NA, NA),
	PINGROUP(41, wcss_wlan1, qdss_tracedata_a, NA,
		 atest_combodac_to_gpio_native, NA, NA, NA, NA, NA),
	PINGROUP(42, wcss_wlan0, qdss_tracedata_a, NA,
		 atest_combodac_to_gpio_native, NA, NA, NA, NA, NA),
	PINGROUP(43, wcss_wlan, prng_rosc, qdss_tracedata_a, NA,
		 atest_combodac_to_gpio_native, NA, NA, NA, NA),
	PINGROUP(44, wcss_wlan, NA, atest_combodac_to_gpio_native, NA, NA, NA,
		 NA, NA, NA),
	PINGROUP(45, wcss_fm, ext_lpass, qdss_tracectl_a, NA,
		 atest_combodac_to_gpio_native, NA, NA, NA, NA),
	PINGROUP(46, wcss_fm, qdss_traceclk_a, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(47, wcss_bt, qdss_tracedata_a, NA,
		 atest_combodac_to_gpio_native, NA, NA, NA, NA, NA),
	PINGROUP(48, wcss_bt, qdss_tracedata_a, NA,
		 atest_combodac_to_gpio_native, NA, NA, NA, NA, NA),
	PINGROUP(49, uim2_data, gcc_gp1_clk_a, qdss_cti_trig_in_a1, NA, NA, NA,
		 NA, NA, NA),
	PINGROUP(50, uim2_clk, gcc_gp2_clk_a, qdss_cti_trig_in_b1, NA, NA, NA,
		 NA, NA, NA),
	PINGROUP(51, uim2_reset, gcc_gp3_clk_a, qdss_cti_trig_out_b1, NA, NA,
		 NA, NA, NA, NA),
	PINGROUP(52, uim2_present, qdss_cti_trig_out_a1, NA, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(53, uim1_data, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(54, uim1_clk, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(55, uim1_reset, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(56, uim1_present, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(57, uim_batt, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(58, qdss_tracedata_a, smb_int, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(59, cdc_pdm0, pri_mi2s_mclk_a, atest_char3, NA, NA, NA, NA,
		 NA, bimc_dte0),
	PINGROUP(60, cdc_pdm0, pri_mi2s_sck_a, atest_char2, NA, NA, NA, NA, NA,
		 bimc_dte1),
	PINGROUP(61, cdc_pdm0, pri_mi2s_ws_a, atest_char1, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(62, cdc_pdm0, pri_mi2s_data0_a, atest_char0, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(63, cdc_pdm0, pri_mi2s_data1_a, atest_char, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(64, cdc_pdm0, NA, NA, NA, NA, NA, ebi0_wrcdc, NA, NA),
	PINGROUP(65, blsp3_spi, blsp1_spi, qdss_tracedata_a, NA,
		 atest_gpsadc_dtest0_native, NA, NA, NA, NA),
	PINGROUP(66, NA, gcc_plltest, NA, atest_combodac_to_gpio_native, NA,
		 NA, NA, NA, NA),
	PINGROUP(67, NA, gcc_plltest, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(68, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(69, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(70, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(71, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(72, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(73, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(74, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(75, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(76, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(77, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(78, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(79, NA, NA, atest_gpsadc_dtest1_native, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(80, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(81, NA, NA, NA, atest_combodac_to_gpio_native, NA, NA, NA, NA,
		 NA),
	PINGROUP(82, NA, pa_indicator, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(83, NA, modem_tsync, nav_tsync, nav_pps, NA,
		 atest_combodac_to_gpio_native, NA, NA, NA),
	PINGROUP(84, NA, NA, atest_combodac_to_gpio_native, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(85, gsm0_tx, NA, NA, atest_combodac_to_gpio_native, NA, NA,
		 NA, NA, NA),
	PINGROUP(86, NA, NA, atest_combodac_to_gpio_native, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(87, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(88, NA, ssbi0, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(89, NA, ssbi1, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(90, pbs0, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(91, pbs1, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(92, pbs2, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(93, qdss_tracedata_b, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(94, pri_mi2s_sck_b, pwr_modem_enabled_b, qdss_tracedata_a, NA,
		 atest_combodac_to_gpio_native, NA, NA, NA, NA),
	PINGROUP(95, blsp3_spi, pri_mi2s_data0_b, ebi2_lcd, m_voc,
		 pwr_nav_enabled_b, NA, atest_combodac_to_gpio_native, NA, NA),
	PINGROUP(96, pri_mi2s_data1_b, NA, pwr_crypto_enabled_b,
		 qdss_tracedata_a, NA, atest_wlan0, NA, NA, NA),
	PINGROUP(97, blsp1_spi, qdss_tracedata_a, NA, atest_wlan1, NA, NA, NA,
		 NA, NA),
	PINGROUP(98, sec_mi2s, pri_mi2s_mclk_b, blsp2_spi, ldo_update, NA, NA,
		 NA, NA, NA),
	PINGROUP(99, ebi2_a, sd_write, ldo_en, NA, NA, NA, NA, NA, NA),
	PINGROUP(100, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(101, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(102, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(103, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(104, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(105, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(106, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(107, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(108, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(109, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(110, pri_mi2s_ws_b, NA, atest_combodac_to_gpio_native, NA, NA,
		 NA, NA, NA, NA),
	PINGROUP(111, blsp_spi2, blsp_uart2, blsp_i2c2, NA, NA, NA, NA, NA, NA),
	PINGROUP(112, blsp_spi2, blsp_uart2, blsp_i2c2, NA, NA, NA, NA, NA, NA),
	SDC_QDSD_PINGROUP(sdc1_clk, 0x10a000, 13, 6),
	SDC_QDSD_PINGROUP(sdc1_cmd, 0x10a000, 11, 3),
	SDC_QDSD_PINGROUP(sdc1_data, 0x10a000, 9, 0),
	SDC_QDSD_PINGROUP(sdc2_clk, 0x109000, 14, 6),
	SDC_QDSD_PINGROUP(sdc2_cmd, 0x109000, 11, 3),
	SDC_QDSD_PINGROUP(sdc2_data, 0x109000, 9, 0),
	SDC_QDSD_PINGROUP(qdsd_clk, 0x19c000, 3, 0),
	SDC_QDSD_PINGROUP(qdsd_cmd, 0x19c000, 8, 5),
	SDC_QDSD_PINGROUP(qdsd_data0, 0x19c000, 13, 10),
	SDC_QDSD_PINGROUP(qdsd_data1, 0x19c000, 18, 15),
	SDC_QDSD_PINGROUP(qdsd_data2, 0x19c000, 23, 20),
	SDC_QDSD_PINGROUP(qdsd_data3, 0x19c000, 28, 25),
};

static const struct msm_pinctrl_soc_data msm8909_pinctrl = {
	.pins = msm8909_pins,
	.npins = ARRAY_SIZE(msm8909_pins),
	.functions = msm8909_functions,
	.nfunctions = ARRAY_SIZE(msm8909_functions),
	.groups = msm8909_groups,
	.ngroups = ARRAY_SIZE(msm8909_groups),
	.ngpios = 113,
};

static int msm8909_pinctrl_probe(struct platform_device *pdev)
{
	return msm_pinctrl_probe(pdev, &msm8909_pinctrl);
}

static const struct of_device_id msm8909_pinctrl_of_match[] = {
	{ .compatible = "qcom,msm8909-pinctrl", },
	{ },
};

static struct platform_driver msm8909_pinctrl_driver = {
	.driver = {
		.name = "msm8909-pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = msm8909_pinctrl_of_match,
	},
	.probe = msm8909_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init msm8909_pinctrl_init(void)
{
	return platform_driver_register(&msm8909_pinctrl_driver);
}
arch_initcall(msm8909_pinctrl_init);

static void __exit msm8909_pinctrl_exit(void)
{
	platform_driver_unregister(&msm8909_pinctrl_driver);
}
module_exit(msm8909_pinctrl_exit);

MODULE_DESCRIPTION("QTI msm8909 pinctrl driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, msm8909_pinctrl_of_match);
