/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#define FUNCTION(fname)					\
	[msm_mux_##fname] = {				\
		.name = #fname,				\
		.groups = fname##_groups,		\
		.ngroups = ARRAY_SIZE(fname##_groups),	\
	}

#define NORTH	0x00500000
#define SOUTH	0x00900000
#define REG_SIZE 0x1000
#define PINGROUP(id, base, f1, f2, f3, f4, f5, f6, f7, f8, f9)	\
	{						\
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
		},					\
		.nfuncs = 10,				\
		.ctl_reg = base + REG_SIZE * id,		\
		.io_reg = base + 0x4 + REG_SIZE * id,		\
		.intr_cfg_reg = base + 0x8 + REG_SIZE * id,	\
		.intr_status_reg = base + 0xc + REG_SIZE * id,	\
		.intr_target_reg = base + 0x8 + REG_SIZE * id,	\
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
	{						\
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
	{						\
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
static const struct pinctrl_pin_desc sdm845_pins[] = {
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
	PINCTRL_PIN(113, "GPIO_113"),
	PINCTRL_PIN(114, "GPIO_114"),
	PINCTRL_PIN(115, "GPIO_115"),
	PINCTRL_PIN(116, "GPIO_116"),
	PINCTRL_PIN(117, "GPIO_117"),
	PINCTRL_PIN(118, "GPIO_118"),
	PINCTRL_PIN(119, "GPIO_119"),
	PINCTRL_PIN(120, "GPIO_120"),
	PINCTRL_PIN(121, "GPIO_121"),
	PINCTRL_PIN(122, "GPIO_122"),
	PINCTRL_PIN(123, "GPIO_123"),
	PINCTRL_PIN(124, "GPIO_124"),
	PINCTRL_PIN(125, "GPIO_125"),
	PINCTRL_PIN(126, "GPIO_126"),
	PINCTRL_PIN(127, "GPIO_127"),
	PINCTRL_PIN(128, "GPIO_128"),
	PINCTRL_PIN(129, "GPIO_129"),
	PINCTRL_PIN(130, "GPIO_130"),
	PINCTRL_PIN(131, "GPIO_131"),
	PINCTRL_PIN(132, "GPIO_132"),
	PINCTRL_PIN(133, "GPIO_133"),
	PINCTRL_PIN(134, "GPIO_134"),
	PINCTRL_PIN(135, "GPIO_135"),
	PINCTRL_PIN(136, "GPIO_136"),
	PINCTRL_PIN(137, "GPIO_137"),
	PINCTRL_PIN(138, "GPIO_138"),
	PINCTRL_PIN(139, "GPIO_139"),
	PINCTRL_PIN(140, "GPIO_140"),
	PINCTRL_PIN(141, "GPIO_141"),
	PINCTRL_PIN(142, "GPIO_142"),
	PINCTRL_PIN(143, "GPIO_143"),
	PINCTRL_PIN(144, "GPIO_144"),
	PINCTRL_PIN(145, "GPIO_145"),
	PINCTRL_PIN(146, "GPIO_146"),
	PINCTRL_PIN(147, "GPIO_147"),
	PINCTRL_PIN(148, "GPIO_148"),
	PINCTRL_PIN(149, "GPIO_149"),
	PINCTRL_PIN(150, "SDC2_CLK"),
	PINCTRL_PIN(151, "SDC2_CMD"),
	PINCTRL_PIN(152, "SDC2_DATA"),
	PINCTRL_PIN(153, "UFS_RESET"),
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
DECLARE_MSM_GPIO_PINS(113);
DECLARE_MSM_GPIO_PINS(114);
DECLARE_MSM_GPIO_PINS(115);
DECLARE_MSM_GPIO_PINS(116);
DECLARE_MSM_GPIO_PINS(117);
DECLARE_MSM_GPIO_PINS(118);
DECLARE_MSM_GPIO_PINS(119);
DECLARE_MSM_GPIO_PINS(120);
DECLARE_MSM_GPIO_PINS(121);
DECLARE_MSM_GPIO_PINS(122);
DECLARE_MSM_GPIO_PINS(123);
DECLARE_MSM_GPIO_PINS(124);
DECLARE_MSM_GPIO_PINS(125);
DECLARE_MSM_GPIO_PINS(126);
DECLARE_MSM_GPIO_PINS(127);
DECLARE_MSM_GPIO_PINS(128);
DECLARE_MSM_GPIO_PINS(129);
DECLARE_MSM_GPIO_PINS(130);
DECLARE_MSM_GPIO_PINS(131);
DECLARE_MSM_GPIO_PINS(132);
DECLARE_MSM_GPIO_PINS(133);
DECLARE_MSM_GPIO_PINS(134);
DECLARE_MSM_GPIO_PINS(135);
DECLARE_MSM_GPIO_PINS(136);
DECLARE_MSM_GPIO_PINS(137);
DECLARE_MSM_GPIO_PINS(138);
DECLARE_MSM_GPIO_PINS(139);
DECLARE_MSM_GPIO_PINS(140);
DECLARE_MSM_GPIO_PINS(141);
DECLARE_MSM_GPIO_PINS(142);
DECLARE_MSM_GPIO_PINS(143);
DECLARE_MSM_GPIO_PINS(144);
DECLARE_MSM_GPIO_PINS(145);
DECLARE_MSM_GPIO_PINS(146);
DECLARE_MSM_GPIO_PINS(147);
DECLARE_MSM_GPIO_PINS(148);
DECLARE_MSM_GPIO_PINS(149);

static const unsigned int sdc2_clk_pins[] = { 150 };
static const unsigned int sdc2_cmd_pins[] = { 151 };
static const unsigned int sdc2_data_pins[] = { 152 };
static const unsigned int ufs_reset_pins[] = { 153 };

enum sdm845_functions {
	msm_mux_gpio,
	msm_mux_qup0,
	msm_mux_reserved0,
	msm_mux_reserved1,
	msm_mux_reserved2,
	msm_mux_reserved3,
	msm_mux_qup9,
	msm_mux_qdss_cti,
	msm_mux_reserved4,
	msm_mux_reserved5,
	msm_mux_ddr_pxi0,
	msm_mux_reserved6,
	msm_mux_ddr_bist,
	msm_mux_atest_tsens2,
	msm_mux_vsense_trigger,
	msm_mux_atest_usb1,
	msm_mux_reserved7,
	msm_mux_qup_l4,
	msm_mux_wlan1_adc1,
	msm_mux_atest_usb13,
	msm_mux_ddr_pxi1,
	msm_mux_reserved8,
	msm_mux_qup_l5,
	msm_mux_wlan1_adc0,
	msm_mux_atest_usb12,
	msm_mux_reserved9,
	msm_mux_mdp_vsync,
	msm_mux_qup_l6,
	msm_mux_wlan2_adc1,
	msm_mux_atest_usb11,
	msm_mux_ddr_pxi2,
	msm_mux_reserved10,
	msm_mux_edp_lcd,
	msm_mux_dbg_out,
	msm_mux_wlan2_adc0,
	msm_mux_atest_usb10,
	msm_mux_reserved11,
	msm_mux_m_voc,
	msm_mux_tsif1_sync,
	msm_mux_ddr_pxi3,
	msm_mux_reserved12,
	msm_mux_cam_mclk,
	msm_mux_pll_bypassnl,
	msm_mux_qdss_gpio0,
	msm_mux_reserved13,
	msm_mux_pll_reset,
	msm_mux_qdss_gpio1,
	msm_mux_reserved14,
	msm_mux_qdss_gpio2,
	msm_mux_reserved15,
	msm_mux_qdss_gpio3,
	msm_mux_reserved16,
	msm_mux_cci_i2c,
	msm_mux_qup1,
	msm_mux_qdss_gpio4,
	msm_mux_reserved17,
	msm_mux_qdss_gpio5,
	msm_mux_reserved18,
	msm_mux_qdss_gpio6,
	msm_mux_reserved19,
	msm_mux_qdss_gpio7,
	msm_mux_reserved20,
	msm_mux_cci_timer0,
	msm_mux_gcc_gp2,
	msm_mux_qdss_gpio8,
	msm_mux_reserved21,
	msm_mux_cci_timer1,
	msm_mux_gcc_gp3,
	msm_mux_qdss_gpio,
	msm_mux_reserved22,
	msm_mux_cci_timer2,
	msm_mux_qdss_gpio9,
	msm_mux_reserved23,
	msm_mux_cci_timer3,
	msm_mux_cci_async,
	msm_mux_qdss_gpio10,
	msm_mux_reserved24,
	msm_mux_cci_timer4,
	msm_mux_qdss_gpio11,
	msm_mux_reserved25,
	msm_mux_qdss_gpio12,
	msm_mux_reserved26,
	msm_mux_qup2,
	msm_mux_qdss_gpio13,
	msm_mux_reserved27,
	msm_mux_qdss_gpio14,
	msm_mux_reserved28,
	msm_mux_phase_flag1,
	msm_mux_qdss_gpio15,
	msm_mux_reserved29,
	msm_mux_phase_flag2,
	msm_mux_reserved30,
	msm_mux_qup11,
	msm_mux_qup14,
	msm_mux_phase_flag3,
	msm_mux_reserved96,
	msm_mux_ldo_en,
	msm_mux_reserved97,
	msm_mux_ldo_update,
	msm_mux_reserved98,
	msm_mux_phase_flag14,
	msm_mux_reserved99,
	msm_mux_phase_flag15,
	msm_mux_reserved100,
	msm_mux_reserved101,
	msm_mux_pci_e1,
	msm_mux_prng_rosc,
	msm_mux_reserved102,
	msm_mux_phase_flag5,
	msm_mux_reserved103,
	msm_mux_reserved104,
	msm_mux_pcie1_forceon,
	msm_mux_uim2_data,
	msm_mux_qup13,
	msm_mux_reserved105,
	msm_mux_pcie1_pwren,
	msm_mux_uim2_clk,
	msm_mux_reserved106,
	msm_mux_pcie1_auxen,
	msm_mux_uim2_reset,
	msm_mux_reserved107,
	msm_mux_pcie1_button,
	msm_mux_uim2_present,
	msm_mux_reserved108,
	msm_mux_uim1_data,
	msm_mux_reserved109,
	msm_mux_uim1_clk,
	msm_mux_reserved110,
	msm_mux_uim1_reset,
	msm_mux_reserved111,
	msm_mux_uim1_present,
	msm_mux_reserved112,
	msm_mux_pcie1_prsnt2,
	msm_mux_uim_batt,
	msm_mux_edp_hot,
	msm_mux_reserved113,
	msm_mux_nav_pps,
	msm_mux_reserved114,
	msm_mux_reserved115,
	msm_mux_reserved116,
	msm_mux_atest_char,
	msm_mux_reserved117,
	msm_mux_adsp_ext,
	msm_mux_atest_char3,
	msm_mux_reserved118,
	msm_mux_atest_char2,
	msm_mux_reserved119,
	msm_mux_atest_char1,
	msm_mux_reserved120,
	msm_mux_atest_char0,
	msm_mux_reserved121,
	msm_mux_reserved122,
	msm_mux_reserved123,
	msm_mux_reserved124,
	msm_mux_reserved125,
	msm_mux_sd_card,
	msm_mux_reserved126,
	msm_mux_reserved127,
	msm_mux_reserved128,
	msm_mux_reserved129,
	msm_mux_qlink_request,
	msm_mux_reserved130,
	msm_mux_qlink_enable,
	msm_mux_reserved131,
	msm_mux_reserved132,
	msm_mux_reserved133,
	msm_mux_reserved134,
	msm_mux_pa_indicator,
	msm_mux_reserved135,
	msm_mux_reserved136,
	msm_mux_phase_flag26,
	msm_mux_reserved137,
	msm_mux_phase_flag27,
	msm_mux_reserved138,
	msm_mux_phase_flag28,
	msm_mux_reserved139,
	msm_mux_phase_flag6,
	msm_mux_reserved140,
	msm_mux_phase_flag29,
	msm_mux_reserved141,
	msm_mux_phase_flag30,
	msm_mux_reserved142,
	msm_mux_phase_flag31,
	msm_mux_reserved143,
	msm_mux_mss_lte,
	msm_mux_reserved144,
	msm_mux_reserved145,
	msm_mux_reserved146,
	msm_mux_reserved147,
	msm_mux_reserved148,
	msm_mux_reserved149,
	msm_mux_reserved31,
	msm_mux_reserved32,
	msm_mux_reserved33,
	msm_mux_reserved34,
	msm_mux_pci_e0,
	msm_mux_jitter_bist,
	msm_mux_reserved35,
	msm_mux_pll_bist,
	msm_mux_atest_tsens,
	msm_mux_reserved36,
	msm_mux_agera_pll,
	msm_mux_reserved37,
	msm_mux_usb_phy,
	msm_mux_reserved38,
	msm_mux_lpass_slimbus,
	msm_mux_reserved39,
	msm_mux_sd_write,
	msm_mux_tsif1_error,
	msm_mux_reserved40,
	msm_mux_qup3,
	msm_mux_reserved41,
	msm_mux_reserved42,
	msm_mux_reserved43,
	msm_mux_reserved44,
	msm_mux_bt_reset,
	msm_mux_qup6,
	msm_mux_reserved45,
	msm_mux_reserved46,
	msm_mux_reserved47,
	msm_mux_reserved48,
	msm_mux_qup12,
	msm_mux_reserved49,
	msm_mux_reserved50,
	msm_mux_reserved51,
	msm_mux_phase_flag16,
	msm_mux_reserved52,
	msm_mux_qup10,
	msm_mux_phase_flag11,
	msm_mux_reserved53,
	msm_mux_phase_flag12,
	msm_mux_reserved54,
	msm_mux_phase_flag13,
	msm_mux_reserved55,
	msm_mux_phase_flag17,
	msm_mux_reserved56,
	msm_mux_qua_mi2s,
	msm_mux_gcc_gp1,
	msm_mux_phase_flag18,
	msm_mux_reserved57,
	msm_mux_ssc_irq,
	msm_mux_phase_flag19,
	msm_mux_reserved58,
	msm_mux_phase_flag20,
	msm_mux_reserved59,
	msm_mux_cri_trng0,
	msm_mux_phase_flag21,
	msm_mux_reserved60,
	msm_mux_cri_trng1,
	msm_mux_phase_flag22,
	msm_mux_reserved61,
	msm_mux_cri_trng,
	msm_mux_phase_flag23,
	msm_mux_reserved62,
	msm_mux_phase_flag24,
	msm_mux_reserved63,
	msm_mux_pri_mi2s,
	msm_mux_sp_cmu,
	msm_mux_phase_flag25,
	msm_mux_reserved64,
	msm_mux_qup8,
	msm_mux_reserved65,
	msm_mux_pri_mi2s_ws,
	msm_mux_reserved66,
	msm_mux_reserved67,
	msm_mux_reserved68,
	msm_mux_spkr_i2s,
	msm_mux_audio_ref,
	msm_mux_reserved69,
	msm_mux_reserved70,
	msm_mux_tsense_pwm1,
	msm_mux_tsense_pwm2,
	msm_mux_reserved71,
	msm_mux_reserved72,
	msm_mux_btfm_slimbus,
	msm_mux_atest_usb2,
	msm_mux_reserved73,
	msm_mux_ter_mi2s,
	msm_mux_phase_flag7,
	msm_mux_atest_usb23,
	msm_mux_reserved74,
	msm_mux_phase_flag8,
	msm_mux_atest_usb22,
	msm_mux_reserved75,
	msm_mux_phase_flag9,
	msm_mux_atest_usb21,
	msm_mux_reserved76,
	msm_mux_phase_flag4,
	msm_mux_atest_usb20,
	msm_mux_reserved77,
	msm_mux_reserved78,
	msm_mux_sec_mi2s,
	msm_mux_reserved79,
	msm_mux_reserved80,
	msm_mux_qup15,
	msm_mux_reserved81,
	msm_mux_reserved82,
	msm_mux_reserved83,
	msm_mux_reserved84,
	msm_mux_pcie1_pwrfault,
	msm_mux_qup5,
	msm_mux_reserved85,
	msm_mux_pcie1_mrl,
	msm_mux_reserved86,
	msm_mux_reserved87,
	msm_mux_reserved88,
	msm_mux_tsif1_clk,
	msm_mux_qup4,
	msm_mux_tgu_ch3,
	msm_mux_phase_flag10,
	msm_mux_reserved89,
	msm_mux_tsif1_en,
	msm_mux_mdp_vsync0,
	msm_mux_mdp_vsync1,
	msm_mux_mdp_vsync2,
	msm_mux_mdp_vsync3,
	msm_mux_tgu_ch0,
	msm_mux_phase_flag0,
	msm_mux_reserved90,
	msm_mux_tsif1_data,
	msm_mux_sdc4_cmd,
	msm_mux_tgu_ch1,
	msm_mux_reserved91,
	msm_mux_tsif2_error,
	msm_mux_sdc43,
	msm_mux_vfr_1,
	msm_mux_tgu_ch2,
	msm_mux_reserved92,
	msm_mux_tsif2_clk,
	msm_mux_sdc4_clk,
	msm_mux_qup7,
	msm_mux_reserved93,
	msm_mux_tsif2_en,
	msm_mux_sdc42,
	msm_mux_reserved94,
	msm_mux_tsif2_data,
	msm_mux_sdc41,
	msm_mux_reserved95,
	msm_mux_tsif2_sync,
	msm_mux_sdc40,
	msm_mux_NA,
};

static const char * const gpio_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3", "gpio4", "gpio5", "gpio6", "gpio7",
	"gpio8", "gpio9", "gpio10", "gpio11", "gpio12", "gpio13", "gpio14",
	"gpio15", "gpio16", "gpio17", "gpio18", "gpio19", "gpio20", "gpio21",
	"gpio22", "gpio23", "gpio24", "gpio25", "gpio26", "gpio27", "gpio28",
	"gpio29", "gpio30", "gpio31", "gpio32", "gpio33", "gpio34", "gpio35",
	"gpio36", "gpio38", "gpio39", "gpio40", "gpio41", "gpio42", "gpio43",
	"gpio44", "gpio46", "gpio47", "gpio48", "gpio49", "gpio50", "gpio51",
	"gpio52", "gpio53", "gpio54", "gpio55", "gpio56", "gpio57", "gpio64",
	"gpio65", "gpio66", "gpio67", "gpio68", "gpio69", "gpio70", "gpio71",
	"gpio72", "gpio73", "gpio74", "gpio75", "gpio76", "gpio77", "gpio81",
	"gpio82", "gpio83", "gpio84", "gpio87", "gpio88", "gpio89", "gpio90",
	"gpio91", "gpio92", "gpio93", "gpio94", "gpio95", "gpio96", "gpio97",
	"gpio98", "gpio99", "gpio100", "gpio101", "gpio102", "gpio103",
	"gpio109", "gpio110", "gpio111", "gpio112", "gpio114", "gpio115",
	"gpio116", "gpio127", "gpio128", "gpio129", "gpio130", "gpio131",
	"gpio132", "gpio133", "gpio134", "gpio135", "gpio136", "gpio137",
	"gpio138", "gpio139", "gpio140", "gpio141", "gpio142", "gpio143",
	"gpio144", "gpio145", "gpio146", "gpio147", "gpio148", "gpio149",
};
static const char * const qup0_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3",
};
static const char * const reserved0_groups[] = {
	"gpio0",
};
static const char * const reserved1_groups[] = {
	"gpio1",
};
static const char * const reserved2_groups[] = {
	"gpio2",
};
static const char * const reserved3_groups[] = {
	"gpio3",
};
static const char * const qup9_groups[] = {
	"gpio4", "gpio5", "gpio6", "gpio7",
};
static const char * const qdss_cti_groups[] = {
	"gpio4", "gpio5", "gpio51", "gpio52", "gpio62", "gpio63", "gpio90",
	"gpio91",
};
static const char * const reserved4_groups[] = {
	"gpio4",
};
static const char * const reserved5_groups[] = {
	"gpio5",
};
static const char * const ddr_pxi0_groups[] = {
	"gpio6", "gpio7",
};
static const char * const reserved6_groups[] = {
	"gpio6",
};
static const char * const ddr_bist_groups[] = {
	"gpio7", "gpio8", "gpio9", "gpio10",
};
static const char * const atest_tsens2_groups[] = {
	"gpio7",
};
static const char * const vsense_trigger_groups[] = {
	"gpio7",
};
static const char * const atest_usb1_groups[] = {
	"gpio7",
};
static const char * const reserved7_groups[] = {
	"gpio7",
};
static const char * const qup_l4_groups[] = {
	"gpio8", "gpio35", "gpio105", "gpio123",
};
static const char * const wlan1_adc1_groups[] = {
	"gpio8",
};
static const char * const atest_usb13_groups[] = {
	"gpio8",
};
static const char * const ddr_pxi1_groups[] = {
	"gpio8", "gpio9",
};
static const char * const reserved8_groups[] = {
	"gpio8",
};
static const char * const qup_l5_groups[] = {
	"gpio9", "gpio36", "gpio106", "gpio124",
};
static const char * const wlan1_adc0_groups[] = {
	"gpio9",
};
static const char * const atest_usb12_groups[] = {
	"gpio9",
};
static const char * const reserved9_groups[] = {
	"gpio9",
};
static const char * const mdp_vsync_groups[] = {
	"gpio10", "gpio11", "gpio12", "gpio97", "gpio98",
};
static const char * const qup_l6_groups[] = {
	"gpio10", "gpio37", "gpio107", "gpio125",
};
static const char * const wlan2_adc1_groups[] = {
	"gpio10",
};
static const char * const atest_usb11_groups[] = {
	"gpio10",
};
static const char * const ddr_pxi2_groups[] = {
	"gpio10", "gpio11",
};
static const char * const reserved10_groups[] = {
	"gpio10",
};
static const char * const edp_lcd_groups[] = {
	"gpio11",
};
static const char * const dbg_out_groups[] = {
	"gpio11",
};
static const char * const wlan2_adc0_groups[] = {
	"gpio11",
};
static const char * const atest_usb10_groups[] = {
	"gpio11",
};
static const char * const reserved11_groups[] = {
	"gpio11",
};
static const char * const m_voc_groups[] = {
	"gpio12",
};
static const char * const tsif1_sync_groups[] = {
	"gpio12",
};
static const char * const ddr_pxi3_groups[] = {
	"gpio12", "gpio13",
};
static const char * const reserved12_groups[] = {
	"gpio12",
};
static const char * const cam_mclk_groups[] = {
	"gpio13", "gpio14", "gpio15", "gpio16",
};
static const char * const pll_bypassnl_groups[] = {
	"gpio13",
};
static const char * const qdss_gpio0_groups[] = {
	"gpio13", "gpio117",
};
static const char * const reserved13_groups[] = {
	"gpio13",
};
static const char * const pll_reset_groups[] = {
	"gpio14",
};
static const char * const qdss_gpio1_groups[] = {
	"gpio14", "gpio118",
};
static const char * const reserved14_groups[] = {
	"gpio14",
};
static const char * const qdss_gpio2_groups[] = {
	"gpio15", "gpio119",
};
static const char * const reserved15_groups[] = {
	"gpio15",
};
static const char * const qdss_gpio3_groups[] = {
	"gpio16", "gpio120",
};
static const char * const reserved16_groups[] = {
	"gpio16",
};
static const char * const cci_i2c_groups[] = {
	"gpio17", "gpio18", "gpio19", "gpio20",
};
static const char * const qup1_groups[] = {
	"gpio17", "gpio18", "gpio19", "gpio20",
};
static const char * const qdss_gpio4_groups[] = {
	"gpio17", "gpio121",
};
static const char * const reserved17_groups[] = {
	"gpio17",
};
static const char * const qdss_gpio5_groups[] = {
	"gpio18", "gpio122",
};
static const char * const reserved18_groups[] = {
	"gpio18",
};
static const char * const qdss_gpio6_groups[] = {
	"gpio19", "gpio41",
};
static const char * const reserved19_groups[] = {
	"gpio19",
};
static const char * const qdss_gpio7_groups[] = {
	"gpio20", "gpio42",
};
static const char * const reserved20_groups[] = {
	"gpio20",
};
static const char * const cci_timer0_groups[] = {
	"gpio21",
};
static const char * const gcc_gp2_groups[] = {
	"gpio21", "gpio58",
};
static const char * const qdss_gpio8_groups[] = {
	"gpio21", "gpio75",
};
static const char * const reserved21_groups[] = {
	"gpio21",
};
static const char * const cci_timer1_groups[] = {
	"gpio22",
};
static const char * const gcc_gp3_groups[] = {
	"gpio22", "gpio59",
};
static const char * const qdss_gpio_groups[] = {
	"gpio22", "gpio30", "gpio123", "gpio124",
};
static const char * const reserved22_groups[] = {
	"gpio22",
};
static const char * const cci_timer2_groups[] = {
	"gpio23",
};
static const char * const qdss_gpio9_groups[] = {
	"gpio23", "gpio76",
};
static const char * const reserved23_groups[] = {
	"gpio23",
};
static const char * const cci_timer3_groups[] = {
	"gpio24",
};
static const char * const cci_async_groups[] = {
	"gpio24", "gpio25", "gpio26",
};
static const char * const qdss_gpio10_groups[] = {
	"gpio24", "gpio77",
};
static const char * const reserved24_groups[] = {
	"gpio24",
};
static const char * const cci_timer4_groups[] = {
	"gpio25",
};
static const char * const qdss_gpio11_groups[] = {
	"gpio25", "gpio79",
};
static const char * const reserved25_groups[] = {
	"gpio25",
};
static const char * const qdss_gpio12_groups[] = {
	"gpio26", "gpio80",
};
static const char * const reserved26_groups[] = {
	"gpio26",
};
static const char * const qup2_groups[] = {
	"gpio27", "gpio28", "gpio29", "gpio30",
};
static const char * const qdss_gpio13_groups[] = {
	"gpio27", "gpio93",
};
static const char * const reserved27_groups[] = {
	"gpio27",
};
static const char * const qdss_gpio14_groups[] = {
	"gpio28", "gpio43",
};
static const char * const reserved28_groups[] = {
	"gpio28",
};
static const char * const phase_flag1_groups[] = {
	"gpio29",
};
static const char * const qdss_gpio15_groups[] = {
	"gpio29", "gpio44",
};
static const char * const reserved29_groups[] = {
	"gpio29",
};
static const char * const phase_flag2_groups[] = {
	"gpio30",
};
static const char * const reserved30_groups[] = {
	"gpio30",
};
static const char * const qup11_groups[] = {
	"gpio31", "gpio32", "gpio33", "gpio34",
};
static const char * const qup14_groups[] = {
	"gpio31", "gpio32", "gpio33", "gpio34",
};
static const char * const phase_flag3_groups[] = {
	"gpio96",
};
static const char * const reserved96_groups[] = {
	"gpio96",
};
static const char * const ldo_en_groups[] = {
	"gpio97",
};
static const char * const reserved97_groups[] = {
	"gpio97",
};
static const char * const ldo_update_groups[] = {
	"gpio98",
};
static const char * const reserved98_groups[] = {
	"gpio98",
};
static const char * const phase_flag14_groups[] = {
	"gpio99",
};
static const char * const reserved99_groups[] = {
	"gpio99",
};
static const char * const phase_flag15_groups[] = {
	"gpio100",
};
static const char * const reserved100_groups[] = {
	"gpio100",
};
static const char * const reserved101_groups[] = {
	"gpio101",
};
static const char * const pci_e1_groups[] = {
	"gpio102", "gpio103", "gpio104",
};
static const char * const prng_rosc_groups[] = {
	"gpio102",
};
static const char * const reserved102_groups[] = {
	"gpio102",
};
static const char * const phase_flag5_groups[] = {
	"gpio103",
};
static const char * const reserved103_groups[] = {
	"gpio103",
};
static const char * const reserved104_groups[] = {
	"gpio104",
};
static const char * const pcie1_forceon_groups[] = {
	"gpio105",
};
static const char * const uim2_data_groups[] = {
	"gpio105",
};
static const char * const qup13_groups[] = {
	"gpio105", "gpio106", "gpio107", "gpio108",
};
static const char * const reserved105_groups[] = {
	"gpio105",
};
static const char * const pcie1_pwren_groups[] = {
	"gpio106",
};
static const char * const uim2_clk_groups[] = {
	"gpio106",
};
static const char * const reserved106_groups[] = {
	"gpio106",
};
static const char * const pcie1_auxen_groups[] = {
	"gpio107",
};
static const char * const uim2_reset_groups[] = {
	"gpio107",
};
static const char * const reserved107_groups[] = {
	"gpio107",
};
static const char * const pcie1_button_groups[] = {
	"gpio108",
};
static const char * const uim2_present_groups[] = {
	"gpio108",
};
static const char * const reserved108_groups[] = {
	"gpio108",
};
static const char * const uim1_data_groups[] = {
	"gpio109",
};
static const char * const reserved109_groups[] = {
	"gpio109",
};
static const char * const uim1_clk_groups[] = {
	"gpio110",
};
static const char * const reserved110_groups[] = {
	"gpio110",
};
static const char * const uim1_reset_groups[] = {
	"gpio111",
};
static const char * const reserved111_groups[] = {
	"gpio111",
};
static const char * const uim1_present_groups[] = {
	"gpio112",
};
static const char * const reserved112_groups[] = {
	"gpio112",
};
static const char * const pcie1_prsnt2_groups[] = {
	"gpio113",
};
static const char * const uim_batt_groups[] = {
	"gpio113",
};
static const char * const edp_hot_groups[] = {
	"gpio113",
};
static const char * const reserved113_groups[] = {
	"gpio113",
};
static const char * const nav_pps_groups[] = {
	"gpio114", "gpio114", "gpio115", "gpio115", "gpio128", "gpio128",
	"gpio129", "gpio129", "gpio143", "gpio143",
};
static const char * const reserved114_groups[] = {
	"gpio114",
};
static const char * const reserved115_groups[] = {
	"gpio115",
};
static const char * const reserved116_groups[] = {
	"gpio116",
};
static const char * const atest_char_groups[] = {
	"gpio117",
};
static const char * const reserved117_groups[] = {
	"gpio117",
};
static const char * const adsp_ext_groups[] = {
	"gpio118",
};
static const char * const atest_char3_groups[] = {
	"gpio118",
};
static const char * const reserved118_groups[] = {
	"gpio118",
};
static const char * const atest_char2_groups[] = {
	"gpio119",
};
static const char * const reserved119_groups[] = {
	"gpio119",
};
static const char * const atest_char1_groups[] = {
	"gpio120",
};
static const char * const reserved120_groups[] = {
	"gpio120",
};
static const char * const atest_char0_groups[] = {
	"gpio121",
};
static const char * const reserved121_groups[] = {
	"gpio121",
};
static const char * const reserved122_groups[] = {
	"gpio122",
};
static const char * const reserved123_groups[] = {
	"gpio123",
};
static const char * const reserved124_groups[] = {
	"gpio124",
};
static const char * const reserved125_groups[] = {
	"gpio125",
};
static const char * const sd_card_groups[] = {
	"gpio126",
};
static const char * const reserved126_groups[] = {
	"gpio126",
};
static const char * const reserved127_groups[] = {
	"gpio127",
};
static const char * const reserved128_groups[] = {
	"gpio128",
};
static const char * const reserved129_groups[] = {
	"gpio129",
};
static const char * const qlink_request_groups[] = {
	"gpio130",
};
static const char * const reserved130_groups[] = {
	"gpio130",
};
static const char * const qlink_enable_groups[] = {
	"gpio131",
};
static const char * const reserved131_groups[] = {
	"gpio131",
};
static const char * const reserved132_groups[] = {
	"gpio132",
};
static const char * const reserved133_groups[] = {
	"gpio133",
};
static const char * const reserved134_groups[] = {
	"gpio134",
};
static const char * const pa_indicator_groups[] = {
	"gpio135",
};
static const char * const reserved135_groups[] = {
	"gpio135",
};
static const char * const reserved136_groups[] = {
	"gpio136",
};
static const char * const phase_flag26_groups[] = {
	"gpio137",
};
static const char * const reserved137_groups[] = {
	"gpio137",
};
static const char * const phase_flag27_groups[] = {
	"gpio138",
};
static const char * const reserved138_groups[] = {
	"gpio138",
};
static const char * const phase_flag28_groups[] = {
	"gpio139",
};
static const char * const reserved139_groups[] = {
	"gpio139",
};
static const char * const phase_flag6_groups[] = {
	"gpio140",
};
static const char * const reserved140_groups[] = {
	"gpio140",
};
static const char * const phase_flag29_groups[] = {
	"gpio141",
};
static const char * const reserved141_groups[] = {
	"gpio141",
};
static const char * const phase_flag30_groups[] = {
	"gpio142",
};
static const char * const reserved142_groups[] = {
	"gpio142",
};
static const char * const phase_flag31_groups[] = {
	"gpio143",
};
static const char * const reserved143_groups[] = {
	"gpio143",
};
static const char * const mss_lte_groups[] = {
	"gpio144", "gpio145",
};
static const char * const reserved144_groups[] = {
	"gpio144",
};
static const char * const reserved145_groups[] = {
	"gpio145",
};
static const char * const reserved146_groups[] = {
	"gpio146",
};
static const char * const reserved147_groups[] = {
	"gpio147",
};
static const char * const reserved148_groups[] = {
	"gpio148",
};
static const char * const reserved149_groups[] = {
	"gpio149", "gpio149",
};
static const char * const reserved31_groups[] = {
	"gpio31",
};
static const char * const reserved32_groups[] = {
	"gpio32",
};
static const char * const reserved33_groups[] = {
	"gpio33",
};
static const char * const reserved34_groups[] = {
	"gpio34",
};
static const char * const pci_e0_groups[] = {
	"gpio35", "gpio36", "gpio37",
};
static const char * const jitter_bist_groups[] = {
	"gpio35",
};
static const char * const reserved35_groups[] = {
	"gpio35",
};
static const char * const pll_bist_groups[] = {
	"gpio36",
};
static const char * const atest_tsens_groups[] = {
	"gpio36",
};
static const char * const reserved36_groups[] = {
	"gpio36",
};
static const char * const agera_pll_groups[] = {
	"gpio37",
};
static const char * const reserved37_groups[] = {
	"gpio37",
};
static const char * const usb_phy_groups[] = {
	"gpio38",
};
static const char * const reserved38_groups[] = {
	"gpio38",
};
static const char * const lpass_slimbus_groups[] = {
	"gpio39", "gpio70", "gpio71", "gpio72",
};
static const char * const reserved39_groups[] = {
	"gpio39",
};
static const char * const sd_write_groups[] = {
	"gpio40",
};
static const char * const tsif1_error_groups[] = {
	"gpio40",
};
static const char * const reserved40_groups[] = {
	"gpio40",
};
static const char * const qup3_groups[] = {
	"gpio41", "gpio42", "gpio43", "gpio44",
};
static const char * const reserved41_groups[] = {
	"gpio41",
};
static const char * const reserved42_groups[] = {
	"gpio42",
};
static const char * const reserved43_groups[] = {
	"gpio43",
};
static const char * const reserved44_groups[] = {
	"gpio44",
};
static const char * const bt_reset_groups[] = {
	"gpio45",
};
static const char * const qup6_groups[] = {
	"gpio45", "gpio46", "gpio47", "gpio48",
};
static const char * const reserved45_groups[] = {
	"gpio45",
};
static const char * const reserved46_groups[] = {
	"gpio46",
};
static const char * const reserved47_groups[] = {
	"gpio47",
};
static const char * const reserved48_groups[] = {
	"gpio48",
};
static const char * const qup12_groups[] = {
	"gpio49", "gpio50", "gpio51", "gpio52",
};
static const char * const reserved49_groups[] = {
	"gpio49",
};
static const char * const reserved50_groups[] = {
	"gpio50",
};
static const char * const reserved51_groups[] = {
	"gpio51",
};
static const char * const phase_flag16_groups[] = {
	"gpio52",
};
static const char * const reserved52_groups[] = {
	"gpio52",
};
static const char * const qup10_groups[] = {
	"gpio53", "gpio54", "gpio55", "gpio56",
};
static const char * const phase_flag11_groups[] = {
	"gpio53",
};
static const char * const reserved53_groups[] = {
	"gpio53",
};
static const char * const phase_flag12_groups[] = {
	"gpio54",
};
static const char * const reserved54_groups[] = {
	"gpio54",
};
static const char * const phase_flag13_groups[] = {
	"gpio55",
};
static const char * const reserved55_groups[] = {
	"gpio55",
};
static const char * const phase_flag17_groups[] = {
	"gpio56",
};
static const char * const reserved56_groups[] = {
	"gpio56",
};
static const char * const qua_mi2s_groups[] = {
	"gpio57", "gpio58", "gpio59", "gpio60", "gpio61", "gpio62", "gpio63",
};
static const char * const gcc_gp1_groups[] = {
	"gpio57", "gpio78",
};
static const char * const phase_flag18_groups[] = {
	"gpio57",
};
static const char * const reserved57_groups[] = {
	"gpio57",
};
static const char * const ssc_irq_groups[] = {
	"gpio58", "gpio59", "gpio60", "gpio61", "gpio62", "gpio63", "gpio78",
	"gpio79", "gpio80", "gpio117", "gpio118", "gpio119", "gpio120",
	"gpio121", "gpio122", "gpio123", "gpio124", "gpio125",
};
static const char * const phase_flag19_groups[] = {
	"gpio58",
};
static const char * const reserved58_groups[] = {
	"gpio58",
};
static const char * const phase_flag20_groups[] = {
	"gpio59",
};
static const char * const reserved59_groups[] = {
	"gpio59",
};
static const char * const cri_trng0_groups[] = {
	"gpio60",
};
static const char * const phase_flag21_groups[] = {
	"gpio60",
};
static const char * const reserved60_groups[] = {
	"gpio60",
};
static const char * const cri_trng1_groups[] = {
	"gpio61",
};
static const char * const phase_flag22_groups[] = {
	"gpio61",
};
static const char * const reserved61_groups[] = {
	"gpio61",
};
static const char * const cri_trng_groups[] = {
	"gpio62",
};
static const char * const phase_flag23_groups[] = {
	"gpio62",
};
static const char * const reserved62_groups[] = {
	"gpio62",
};
static const char * const phase_flag24_groups[] = {
	"gpio63",
};
static const char * const reserved63_groups[] = {
	"gpio63",
};
static const char * const pri_mi2s_groups[] = {
	"gpio64", "gpio65", "gpio67", "gpio68",
};
static const char * const sp_cmu_groups[] = {
	"gpio64",
};
static const char * const phase_flag25_groups[] = {
	"gpio64",
};
static const char * const reserved64_groups[] = {
	"gpio64",
};
static const char * const qup8_groups[] = {
	"gpio65", "gpio66", "gpio67", "gpio68",
};
static const char * const reserved65_groups[] = {
	"gpio65",
};
static const char * const pri_mi2s_ws_groups[] = {
	"gpio66",
};
static const char * const reserved66_groups[] = {
	"gpio66",
};
static const char * const reserved67_groups[] = {
	"gpio67",
};
static const char * const reserved68_groups[] = {
	"gpio68",
};
static const char * const spkr_i2s_groups[] = {
	"gpio69", "gpio70", "gpio71", "gpio72",
};
static const char * const audio_ref_groups[] = {
	"gpio69",
};
static const char * const reserved69_groups[] = {
	"gpio69",
};
static const char * const reserved70_groups[] = {
	"gpio70",
};
static const char * const tsense_pwm1_groups[] = {
	"gpio71",
};
static const char * const tsense_pwm2_groups[] = {
	"gpio71",
};
static const char * const reserved71_groups[] = {
	"gpio71",
};
static const char * const reserved72_groups[] = {
	"gpio72",
};
static const char * const btfm_slimbus_groups[] = {
	"gpio73", "gpio74",
};
static const char * const atest_usb2_groups[] = {
	"gpio73",
};
static const char * const reserved73_groups[] = {
	"gpio73",
};
static const char * const ter_mi2s_groups[] = {
	"gpio74", "gpio75", "gpio76", "gpio77", "gpio78",
};
static const char * const phase_flag7_groups[] = {
	"gpio74",
};
static const char * const atest_usb23_groups[] = {
	"gpio74",
};
static const char * const reserved74_groups[] = {
	"gpio74",
};
static const char * const phase_flag8_groups[] = {
	"gpio75",
};
static const char * const atest_usb22_groups[] = {
	"gpio75",
};
static const char * const reserved75_groups[] = {
	"gpio75",
};
static const char * const phase_flag9_groups[] = {
	"gpio76",
};
static const char * const atest_usb21_groups[] = {
	"gpio76",
};
static const char * const reserved76_groups[] = {
	"gpio76",
};
static const char * const phase_flag4_groups[] = {
	"gpio77",
};
static const char * const atest_usb20_groups[] = {
	"gpio77",
};
static const char * const reserved77_groups[] = {
	"gpio77",
};
static const char * const reserved78_groups[] = {
	"gpio78",
};
static const char * const sec_mi2s_groups[] = {
	"gpio79", "gpio80", "gpio81", "gpio82", "gpio83",
};
static const char * const reserved79_groups[] = {
	"gpio79",
};
static const char * const reserved80_groups[] = {
	"gpio80",
};
static const char * const qup15_groups[] = {
	"gpio81", "gpio82", "gpio83", "gpio84",
};
static const char * const reserved81_groups[] = {
	"gpio81",
};
static const char * const reserved82_groups[] = {
	"gpio82",
};
static const char * const reserved83_groups[] = {
	"gpio83",
};
static const char * const reserved84_groups[] = {
	"gpio84",
};
static const char * const pcie1_pwrfault_groups[] = {
	"gpio85",
};
static const char * const qup5_groups[] = {
	"gpio85", "gpio86", "gpio87", "gpio88",
};
static const char * const reserved85_groups[] = {
	"gpio85",
};
static const char * const pcie1_mrl_groups[] = {
	"gpio86",
};
static const char * const reserved86_groups[] = {
	"gpio86",
};
static const char * const reserved87_groups[] = {
	"gpio87",
};
static const char * const reserved88_groups[] = {
	"gpio88",
};
static const char * const tsif1_clk_groups[] = {
	"gpio89",
};
static const char * const qup4_groups[] = {
	"gpio89", "gpio90", "gpio91", "gpio92",
};
static const char * const tgu_ch3_groups[] = {
	"gpio89",
};
static const char * const phase_flag10_groups[] = {
	"gpio89",
};
static const char * const reserved89_groups[] = {
	"gpio89",
};
static const char * const tsif1_en_groups[] = {
	"gpio90",
};
static const char * const mdp_vsync0_groups[] = {
	"gpio90",
};
static const char * const mdp_vsync1_groups[] = {
	"gpio90",
};
static const char * const mdp_vsync2_groups[] = {
	"gpio90",
};
static const char * const mdp_vsync3_groups[] = {
	"gpio90",
};
static const char * const tgu_ch0_groups[] = {
	"gpio90",
};
static const char * const phase_flag0_groups[] = {
	"gpio90",
};
static const char * const reserved90_groups[] = {
	"gpio90",
};
static const char * const tsif1_data_groups[] = {
	"gpio91",
};
static const char * const sdc4_cmd_groups[] = {
	"gpio91",
};
static const char * const tgu_ch1_groups[] = {
	"gpio91",
};
static const char * const reserved91_groups[] = {
	"gpio91",
};
static const char * const tsif2_error_groups[] = {
	"gpio92",
};
static const char * const sdc43_groups[] = {
	"gpio92",
};
static const char * const vfr_1_groups[] = {
	"gpio92",
};
static const char * const tgu_ch2_groups[] = {
	"gpio92",
};
static const char * const reserved92_groups[] = {
	"gpio92",
};
static const char * const tsif2_clk_groups[] = {
	"gpio93",
};
static const char * const sdc4_clk_groups[] = {
	"gpio93",
};
static const char * const qup7_groups[] = {
	"gpio93", "gpio94", "gpio95", "gpio96",
};
static const char * const reserved93_groups[] = {
	"gpio93",
};
static const char * const tsif2_en_groups[] = {
	"gpio94",
};
static const char * const sdc42_groups[] = {
	"gpio94",
};
static const char * const reserved94_groups[] = {
	"gpio94",
};
static const char * const tsif2_data_groups[] = {
	"gpio95",
};
static const char * const sdc41_groups[] = {
	"gpio95",
};
static const char * const reserved95_groups[] = {
	"gpio95",
};
static const char * const tsif2_sync_groups[] = {
	"gpio96",
};
static const char * const sdc40_groups[] = {
	"gpio96",
};

static const struct msm_function sdm845_functions[] = {
	FUNCTION(gpio),
	FUNCTION(qup0),
	FUNCTION(reserved0),
	FUNCTION(reserved1),
	FUNCTION(reserved2),
	FUNCTION(reserved3),
	FUNCTION(qup9),
	FUNCTION(qdss_cti),
	FUNCTION(reserved4),
	FUNCTION(reserved5),
	FUNCTION(ddr_pxi0),
	FUNCTION(reserved6),
	FUNCTION(ddr_bist),
	FUNCTION(atest_tsens2),
	FUNCTION(vsense_trigger),
	FUNCTION(atest_usb1),
	FUNCTION(reserved7),
	FUNCTION(qup_l4),
	FUNCTION(wlan1_adc1),
	FUNCTION(atest_usb13),
	FUNCTION(ddr_pxi1),
	FUNCTION(reserved8),
	FUNCTION(qup_l5),
	FUNCTION(wlan1_adc0),
	FUNCTION(atest_usb12),
	FUNCTION(reserved9),
	FUNCTION(mdp_vsync),
	FUNCTION(qup_l6),
	FUNCTION(wlan2_adc1),
	FUNCTION(atest_usb11),
	FUNCTION(ddr_pxi2),
	FUNCTION(reserved10),
	FUNCTION(edp_lcd),
	FUNCTION(dbg_out),
	FUNCTION(wlan2_adc0),
	FUNCTION(atest_usb10),
	FUNCTION(reserved11),
	FUNCTION(m_voc),
	FUNCTION(tsif1_sync),
	FUNCTION(ddr_pxi3),
	FUNCTION(reserved12),
	FUNCTION(cam_mclk),
	FUNCTION(pll_bypassnl),
	FUNCTION(qdss_gpio0),
	FUNCTION(reserved13),
	FUNCTION(pll_reset),
	FUNCTION(qdss_gpio1),
	FUNCTION(reserved14),
	FUNCTION(qdss_gpio2),
	FUNCTION(reserved15),
	FUNCTION(qdss_gpio3),
	FUNCTION(reserved16),
	FUNCTION(cci_i2c),
	FUNCTION(qup1),
	FUNCTION(qdss_gpio4),
	FUNCTION(reserved17),
	FUNCTION(qdss_gpio5),
	FUNCTION(reserved18),
	FUNCTION(qdss_gpio6),
	FUNCTION(reserved19),
	FUNCTION(qdss_gpio7),
	FUNCTION(reserved20),
	FUNCTION(cci_timer0),
	FUNCTION(gcc_gp2),
	FUNCTION(qdss_gpio8),
	FUNCTION(reserved21),
	FUNCTION(cci_timer1),
	FUNCTION(gcc_gp3),
	FUNCTION(qdss_gpio),
	FUNCTION(reserved22),
	FUNCTION(cci_timer2),
	FUNCTION(qdss_gpio9),
	FUNCTION(reserved23),
	FUNCTION(cci_timer3),
	FUNCTION(cci_async),
	FUNCTION(qdss_gpio10),
	FUNCTION(reserved24),
	FUNCTION(cci_timer4),
	FUNCTION(qdss_gpio11),
	FUNCTION(reserved25),
	FUNCTION(qdss_gpio12),
	FUNCTION(reserved26),
	FUNCTION(qup2),
	FUNCTION(qdss_gpio13),
	FUNCTION(reserved27),
	FUNCTION(qdss_gpio14),
	FUNCTION(reserved28),
	FUNCTION(phase_flag1),
	FUNCTION(qdss_gpio15),
	FUNCTION(reserved29),
	FUNCTION(phase_flag2),
	FUNCTION(reserved30),
	FUNCTION(qup11),
	FUNCTION(qup14),
	FUNCTION(phase_flag3),
	FUNCTION(reserved96),
	FUNCTION(ldo_en),
	FUNCTION(reserved97),
	FUNCTION(ldo_update),
	FUNCTION(reserved98),
	FUNCTION(phase_flag14),
	FUNCTION(reserved99),
	FUNCTION(phase_flag15),
	FUNCTION(reserved100),
	FUNCTION(reserved101),
	FUNCTION(pci_e1),
	FUNCTION(prng_rosc),
	FUNCTION(reserved102),
	FUNCTION(phase_flag5),
	FUNCTION(reserved103),
	FUNCTION(reserved104),
	FUNCTION(pcie1_forceon),
	FUNCTION(uim2_data),
	FUNCTION(qup13),
	FUNCTION(reserved105),
	FUNCTION(pcie1_pwren),
	FUNCTION(uim2_clk),
	FUNCTION(reserved106),
	FUNCTION(pcie1_auxen),
	FUNCTION(uim2_reset),
	FUNCTION(reserved107),
	FUNCTION(pcie1_button),
	FUNCTION(uim2_present),
	FUNCTION(reserved108),
	FUNCTION(uim1_data),
	FUNCTION(reserved109),
	FUNCTION(uim1_clk),
	FUNCTION(reserved110),
	FUNCTION(uim1_reset),
	FUNCTION(reserved111),
	FUNCTION(uim1_present),
	FUNCTION(reserved112),
	FUNCTION(pcie1_prsnt2),
	FUNCTION(uim_batt),
	FUNCTION(edp_hot),
	FUNCTION(reserved113),
	FUNCTION(nav_pps),
	FUNCTION(reserved114),
	FUNCTION(reserved115),
	FUNCTION(reserved116),
	FUNCTION(atest_char),
	FUNCTION(reserved117),
	FUNCTION(adsp_ext),
	FUNCTION(atest_char3),
	FUNCTION(reserved118),
	FUNCTION(atest_char2),
	FUNCTION(reserved119),
	FUNCTION(atest_char1),
	FUNCTION(reserved120),
	FUNCTION(atest_char0),
	FUNCTION(reserved121),
	FUNCTION(reserved122),
	FUNCTION(reserved123),
	FUNCTION(reserved124),
	FUNCTION(reserved125),
	FUNCTION(sd_card),
	FUNCTION(reserved126),
	FUNCTION(reserved127),
	FUNCTION(reserved128),
	FUNCTION(reserved129),
	FUNCTION(qlink_request),
	FUNCTION(reserved130),
	FUNCTION(qlink_enable),
	FUNCTION(reserved131),
	FUNCTION(reserved132),
	FUNCTION(reserved133),
	FUNCTION(reserved134),
	FUNCTION(pa_indicator),
	FUNCTION(reserved135),
	FUNCTION(reserved136),
	FUNCTION(phase_flag26),
	FUNCTION(reserved137),
	FUNCTION(phase_flag27),
	FUNCTION(reserved138),
	FUNCTION(phase_flag28),
	FUNCTION(reserved139),
	FUNCTION(phase_flag6),
	FUNCTION(reserved140),
	FUNCTION(phase_flag29),
	FUNCTION(reserved141),
	FUNCTION(phase_flag30),
	FUNCTION(reserved142),
	FUNCTION(phase_flag31),
	FUNCTION(reserved143),
	FUNCTION(mss_lte),
	FUNCTION(reserved144),
	FUNCTION(reserved145),
	FUNCTION(reserved146),
	FUNCTION(reserved147),
	FUNCTION(reserved148),
	FUNCTION(reserved149),
	FUNCTION(reserved31),
	FUNCTION(reserved32),
	FUNCTION(reserved33),
	FUNCTION(reserved34),
	FUNCTION(pci_e0),
	FUNCTION(jitter_bist),
	FUNCTION(reserved35),
	FUNCTION(pll_bist),
	FUNCTION(atest_tsens),
	FUNCTION(reserved36),
	FUNCTION(agera_pll),
	FUNCTION(reserved37),
	FUNCTION(usb_phy),
	FUNCTION(reserved38),
	FUNCTION(lpass_slimbus),
	FUNCTION(reserved39),
	FUNCTION(sd_write),
	FUNCTION(tsif1_error),
	FUNCTION(reserved40),
	FUNCTION(qup3),
	FUNCTION(reserved41),
	FUNCTION(reserved42),
	FUNCTION(reserved43),
	FUNCTION(reserved44),
	FUNCTION(bt_reset),
	FUNCTION(qup6),
	FUNCTION(reserved45),
	FUNCTION(reserved46),
	FUNCTION(reserved47),
	FUNCTION(reserved48),
	FUNCTION(qup12),
	FUNCTION(reserved49),
	FUNCTION(reserved50),
	FUNCTION(reserved51),
	FUNCTION(phase_flag16),
	FUNCTION(reserved52),
	FUNCTION(qup10),
	FUNCTION(phase_flag11),
	FUNCTION(reserved53),
	FUNCTION(phase_flag12),
	FUNCTION(reserved54),
	FUNCTION(phase_flag13),
	FUNCTION(reserved55),
	FUNCTION(phase_flag17),
	FUNCTION(reserved56),
	FUNCTION(qua_mi2s),
	FUNCTION(gcc_gp1),
	FUNCTION(phase_flag18),
	FUNCTION(reserved57),
	FUNCTION(ssc_irq),
	FUNCTION(phase_flag19),
	FUNCTION(reserved58),
	FUNCTION(phase_flag20),
	FUNCTION(reserved59),
	FUNCTION(cri_trng0),
	FUNCTION(phase_flag21),
	FUNCTION(reserved60),
	FUNCTION(cri_trng1),
	FUNCTION(phase_flag22),
	FUNCTION(reserved61),
	FUNCTION(cri_trng),
	FUNCTION(phase_flag23),
	FUNCTION(reserved62),
	FUNCTION(phase_flag24),
	FUNCTION(reserved63),
	FUNCTION(pri_mi2s),
	FUNCTION(sp_cmu),
	FUNCTION(phase_flag25),
	FUNCTION(reserved64),
	FUNCTION(qup8),
	FUNCTION(reserved65),
	FUNCTION(pri_mi2s_ws),
	FUNCTION(reserved66),
	FUNCTION(reserved67),
	FUNCTION(reserved68),
	FUNCTION(spkr_i2s),
	FUNCTION(audio_ref),
	FUNCTION(reserved69),
	FUNCTION(reserved70),
	FUNCTION(tsense_pwm1),
	FUNCTION(tsense_pwm2),
	FUNCTION(reserved71),
	FUNCTION(reserved72),
	FUNCTION(btfm_slimbus),
	FUNCTION(atest_usb2),
	FUNCTION(reserved73),
	FUNCTION(ter_mi2s),
	FUNCTION(phase_flag7),
	FUNCTION(atest_usb23),
	FUNCTION(reserved74),
	FUNCTION(phase_flag8),
	FUNCTION(atest_usb22),
	FUNCTION(reserved75),
	FUNCTION(phase_flag9),
	FUNCTION(atest_usb21),
	FUNCTION(reserved76),
	FUNCTION(phase_flag4),
	FUNCTION(atest_usb20),
	FUNCTION(reserved77),
	FUNCTION(reserved78),
	FUNCTION(sec_mi2s),
	FUNCTION(reserved79),
	FUNCTION(reserved80),
	FUNCTION(qup15),
	FUNCTION(reserved81),
	FUNCTION(reserved82),
	FUNCTION(reserved83),
	FUNCTION(reserved84),
	FUNCTION(pcie1_pwrfault),
	FUNCTION(qup5),
	FUNCTION(reserved85),
	FUNCTION(pcie1_mrl),
	FUNCTION(reserved86),
	FUNCTION(reserved87),
	FUNCTION(reserved88),
	FUNCTION(tsif1_clk),
	FUNCTION(qup4),
	FUNCTION(tgu_ch3),
	FUNCTION(phase_flag10),
	FUNCTION(reserved89),
	FUNCTION(tsif1_en),
	FUNCTION(mdp_vsync0),
	FUNCTION(mdp_vsync1),
	FUNCTION(mdp_vsync2),
	FUNCTION(mdp_vsync3),
	FUNCTION(tgu_ch0),
	FUNCTION(phase_flag0),
	FUNCTION(reserved90),
	FUNCTION(tsif1_data),
	FUNCTION(sdc4_cmd),
	FUNCTION(tgu_ch1),
	FUNCTION(reserved91),
	FUNCTION(tsif2_error),
	FUNCTION(sdc43),
	FUNCTION(vfr_1),
	FUNCTION(tgu_ch2),
	FUNCTION(reserved92),
	FUNCTION(tsif2_clk),
	FUNCTION(sdc4_clk),
	FUNCTION(qup7),
	FUNCTION(reserved93),
	FUNCTION(tsif2_en),
	FUNCTION(sdc42),
	FUNCTION(reserved94),
	FUNCTION(tsif2_data),
	FUNCTION(sdc41),
	FUNCTION(reserved95),
	FUNCTION(tsif2_sync),
	FUNCTION(sdc40),
};

static const struct msm_pingroup sdm845_groups[] = {
	PINGROUP(0, NORTH, qup0, NA, reserved0, NA, NA, NA, NA, NA, NA),
	PINGROUP(1, NORTH, qup0, NA, reserved1, NA, NA, NA, NA, NA, NA),
	PINGROUP(2, NORTH, qup0, NA, reserved2, NA, NA, NA, NA, NA, NA),
	PINGROUP(3, NORTH, qup0, NA, reserved3, NA, NA, NA, NA, NA, NA),
	PINGROUP(4, NORTH, qup9, qdss_cti, reserved4, NA, NA, NA, NA, NA, NA),
	PINGROUP(5, NORTH, qup9, qdss_cti, reserved5, NA, NA, NA, NA, NA, NA),
	PINGROUP(6, NORTH, qup9, NA, ddr_pxi0, reserved6, NA, NA, NA, NA, NA),
	PINGROUP(7, NORTH, qup9, ddr_bist, NA, atest_tsens2, vsense_trigger,
		 atest_usb1, ddr_pxi0, reserved7, NA),
	PINGROUP(8, NORTH, qup_l4, NA, ddr_bist, NA, NA, wlan1_adc1,
		 atest_usb13, ddr_pxi1, reserved8),
	PINGROUP(9, NORTH, qup_l5, ddr_bist, NA, wlan1_adc0, atest_usb12,
		 ddr_pxi1, reserved9, NA, NA),
	PINGROUP(10, NORTH, mdp_vsync, qup_l6, ddr_bist, wlan2_adc1,
		 atest_usb11, ddr_pxi2, reserved10, NA, NA),
	PINGROUP(11, NORTH, mdp_vsync, edp_lcd, dbg_out, wlan2_adc0,
		 atest_usb10, ddr_pxi2, reserved11, NA, NA),
	PINGROUP(12, SOUTH, mdp_vsync, m_voc, tsif1_sync, ddr_pxi3, reserved12,
		 NA, NA, NA, NA),
	PINGROUP(13, SOUTH, cam_mclk, pll_bypassnl, qdss_gpio0, ddr_pxi3,
		 reserved13, NA, NA, NA, NA),
	PINGROUP(14, SOUTH, cam_mclk, pll_reset, qdss_gpio1, reserved14, NA,
		 NA, NA, NA, NA),
	PINGROUP(15, SOUTH, cam_mclk, qdss_gpio2, reserved15, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(16, SOUTH, cam_mclk, qdss_gpio3, reserved16, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(17, SOUTH, cci_i2c, qup1, qdss_gpio4, reserved17, NA, NA, NA,
		 NA, NA),
	PINGROUP(18, SOUTH, cci_i2c, qup1, NA, qdss_gpio5, reserved18, NA, NA,
		 NA, NA),
	PINGROUP(19, SOUTH, cci_i2c, qup1, NA, qdss_gpio6, reserved19, NA, NA,
		 NA, NA),
	PINGROUP(20, SOUTH, cci_i2c, qup1, NA, qdss_gpio7, reserved20, NA, NA,
		 NA, NA),
	PINGROUP(21, SOUTH, cci_timer0, gcc_gp2, qdss_gpio8, reserved21, NA,
		 NA, NA, NA, NA),
	PINGROUP(22, SOUTH, cci_timer1, gcc_gp3, qdss_gpio, reserved22, NA, NA,
		 NA, NA, NA),
	PINGROUP(23, SOUTH, cci_timer2, qdss_gpio9, reserved23, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(24, SOUTH, cci_timer3, cci_async, qdss_gpio10, reserved24, NA,
		 NA, NA, NA, NA),
	PINGROUP(25, SOUTH, cci_timer4, cci_async, qdss_gpio11, reserved25, NA,
		 NA, NA, NA, NA),
	PINGROUP(26, SOUTH, cci_async, qdss_gpio12, reserved26, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(27, NORTH, qup2, qdss_gpio13, reserved27, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(28, NORTH, qup2, qdss_gpio14, reserved28, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(29, NORTH, qup2, NA, phase_flag1, qdss_gpio15, reserved29, NA,
		 NA, NA, NA),
	PINGROUP(30, NORTH, qup2, phase_flag2, qdss_gpio, reserved30, NA, NA,
		 NA, NA, NA),
	PINGROUP(31, NORTH, qup11, qup14, reserved31, NA, NA, NA, NA, NA, NA),
	PINGROUP(32, NORTH, qup11, qup14, NA, reserved32, NA, NA, NA, NA, NA),
	PINGROUP(33, NORTH, qup11, qup14, NA, reserved33, NA, NA, NA, NA, NA),
	PINGROUP(34, NORTH, qup11, qup14, NA, reserved34, NA, NA, NA, NA, NA),
	PINGROUP(35, SOUTH, pci_e0, qup_l4, jitter_bist, NA, reserved35, NA,
		 NA, NA, NA),
	PINGROUP(36, SOUTH, pci_e0, qup_l5, pll_bist, NA, atest_tsens,
		 reserved36, NA, NA, NA),
	PINGROUP(37, SOUTH, qup_l6, agera_pll, NA, reserved37, NA, NA, NA, NA,
		 NA),
	PINGROUP(38, NORTH, usb_phy, NA, reserved38, NA, NA, NA, NA, NA, NA),
	PINGROUP(39, NORTH, lpass_slimbus, NA, reserved39, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(40, SOUTH, sd_write, tsif1_error, NA, reserved40, NA, NA, NA,
		 NA, NA),
	PINGROUP(41, SOUTH, qup3, NA, qdss_gpio6, reserved41, NA, NA, NA, NA,
		 NA),
	PINGROUP(42, SOUTH, qup3, NA, qdss_gpio7, reserved42, NA, NA, NA, NA,
		 NA),
	PINGROUP(43, SOUTH, qup3, NA, qdss_gpio14, reserved43, NA, NA, NA, NA,
		 NA),
	PINGROUP(44, SOUTH, qup3, NA, qdss_gpio15, reserved44, NA, NA, NA, NA,
		 NA),
	PINGROUP(45, NORTH, qup6, NA, reserved45, NA, NA, NA, NA, NA, NA),
	PINGROUP(46, NORTH, qup6, NA, reserved46, NA, NA, NA, NA, NA, NA),
	PINGROUP(47, NORTH, qup6, reserved47, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(48, NORTH, qup6, reserved48, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(49, NORTH, qup12, reserved49, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(50, NORTH, qup12, reserved50, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(51, NORTH, qup12, qdss_cti, reserved51, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(52, NORTH, qup12, phase_flag16, qdss_cti, reserved52, NA, NA,
		 NA, NA, NA),
	PINGROUP(53, NORTH, qup10, phase_flag11, reserved53, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(54, NORTH, qup10, NA, phase_flag12, reserved54, NA, NA, NA,
		 NA, NA),
	PINGROUP(55, NORTH, qup10, phase_flag13, reserved55, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(56, NORTH, qup10, phase_flag17, reserved56, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(57, NORTH, qua_mi2s, gcc_gp1, phase_flag18, reserved57, NA,
		 NA, NA, NA, NA),
	PINGROUP(58, NORTH, qua_mi2s, gcc_gp2, phase_flag19, reserved58, NA,
		 NA, NA, NA, NA),
	PINGROUP(59, NORTH, qua_mi2s, gcc_gp3, phase_flag20, reserved59, NA,
		 NA, NA, NA, NA),
	PINGROUP(60, NORTH, qua_mi2s, cri_trng0, phase_flag21, reserved60, NA,
		 NA, NA, NA, NA),
	PINGROUP(61, NORTH, qua_mi2s, cri_trng1, phase_flag22, reserved61, NA,
		 NA, NA, NA, NA),
	PINGROUP(62, NORTH, qua_mi2s, cri_trng, phase_flag23, qdss_cti,
		 reserved62, NA, NA, NA, NA),
	PINGROUP(63, NORTH, qua_mi2s, NA, phase_flag24, qdss_cti, reserved63,
		 NA, NA, NA, NA),
	PINGROUP(64, NORTH, pri_mi2s, sp_cmu, phase_flag25, reserved64, NA, NA,
		 NA, NA, NA),
	PINGROUP(65, NORTH, pri_mi2s, qup8, reserved65, NA, NA, NA, NA, NA, NA),
	PINGROUP(66, NORTH, pri_mi2s_ws, qup8, reserved66, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(67, NORTH, pri_mi2s, qup8, reserved67, NA, NA, NA, NA, NA, NA),
	PINGROUP(68, NORTH, pri_mi2s, qup8, reserved68, NA, NA, NA, NA, NA, NA),
	PINGROUP(69, NORTH, spkr_i2s, audio_ref, reserved69, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(70, NORTH, lpass_slimbus, spkr_i2s, reserved70, NA, NA, NA,
		 NA, NA, NA),
	PINGROUP(71, NORTH, lpass_slimbus, spkr_i2s, tsense_pwm1, tsense_pwm2,
		 reserved71, NA, NA, NA, NA),
	PINGROUP(72, NORTH, lpass_slimbus, spkr_i2s, reserved72, NA, NA, NA,
		 NA, NA, NA),
	PINGROUP(73, NORTH, btfm_slimbus, atest_usb2, reserved73, NA, NA, NA,
		 NA, NA, NA),
	PINGROUP(74, NORTH, btfm_slimbus, ter_mi2s, phase_flag7, atest_usb23,
		 reserved74, NA, NA, NA, NA),
	PINGROUP(75, NORTH, ter_mi2s, phase_flag8, qdss_gpio8, atest_usb22,
		 reserved75, NA, NA, NA, NA),
	PINGROUP(76, NORTH, ter_mi2s, phase_flag9, qdss_gpio9, atest_usb21,
		 reserved76, NA, NA, NA, NA),
	PINGROUP(77, NORTH, ter_mi2s, phase_flag4, qdss_gpio10, atest_usb20,
		 reserved77, NA, NA, NA, NA),
	PINGROUP(78, NORTH, ter_mi2s, gcc_gp1, reserved78, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(79, NORTH, sec_mi2s, NA, NA, qdss_gpio11, reserved79, NA, NA,
		 NA, NA),
	PINGROUP(80, NORTH, sec_mi2s, NA, qdss_gpio12, reserved80, NA, NA, NA,
		 NA, NA),
	PINGROUP(81, NORTH, sec_mi2s, qup15, NA, reserved81, NA, NA, NA, NA,
		 NA),
	PINGROUP(82, NORTH, sec_mi2s, qup15, NA, reserved82, NA, NA, NA, NA,
		 NA),
	PINGROUP(83, NORTH, sec_mi2s, qup15, NA, reserved83, NA, NA, NA, NA,
		 NA),
	PINGROUP(84, NORTH, qup15, NA, reserved84, NA, NA, NA, NA, NA, NA),
	PINGROUP(85, SOUTH, qup5, NA, reserved85, NA, NA, NA, NA, NA, NA),
	PINGROUP(86, SOUTH, qup5, NA, NA, reserved86, NA, NA, NA, NA, NA),
	PINGROUP(87, SOUTH, qup5, NA, reserved87, NA, NA, NA, NA, NA, NA),
	PINGROUP(88, SOUTH, qup5, NA, reserved88, NA, NA, NA, NA, NA, NA),
	PINGROUP(89, SOUTH, tsif1_clk, qup4, tgu_ch3, phase_flag10, reserved89,
		 NA, NA, NA, NA),
	PINGROUP(90, SOUTH, tsif1_en, mdp_vsync0, qup4, mdp_vsync1, mdp_vsync2,
		 mdp_vsync3, tgu_ch0, phase_flag0, qdss_cti),
	PINGROUP(91, SOUTH, tsif1_data, sdc4_cmd, qup4, tgu_ch1, NA, qdss_cti,
		 reserved91, NA, NA),
	PINGROUP(92, SOUTH, tsif2_error, sdc43, qup4, vfr_1, tgu_ch2, NA,
		 reserved92, NA, NA),
	PINGROUP(93, SOUTH, tsif2_clk, sdc4_clk, qup7, NA, qdss_gpio13,
		 reserved93, NA, NA, NA),
	PINGROUP(94, SOUTH, tsif2_en, sdc42, qup7, NA, reserved94, NA, NA, NA,
		 NA),
	PINGROUP(95, SOUTH, tsif2_data, sdc41, qup7, NA, NA, reserved95, NA,
		 NA, NA),
	PINGROUP(96, SOUTH, tsif2_sync, sdc40, qup7, phase_flag3, reserved96,
		 NA, NA, NA, NA),
	PINGROUP(97, NORTH, NA, NA, mdp_vsync, ldo_en, reserved97, NA, NA, NA,
		 NA),
	PINGROUP(98, NORTH, NA, mdp_vsync, ldo_update, reserved98, NA, NA, NA,
		 NA, NA),
	PINGROUP(99, NORTH, phase_flag14, reserved99, NA, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(100, NORTH, phase_flag15, reserved100, NA, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(101, NORTH, NA, reserved101, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(102, NORTH, pci_e1, prng_rosc, reserved102, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(103, NORTH, pci_e1, phase_flag5, reserved103, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(104, NORTH, NA, reserved104, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(105, NORTH, uim2_data, qup13, qup_l4, NA, reserved105, NA, NA,
		 NA, NA),
	PINGROUP(106, NORTH, uim2_clk, qup13, qup_l5, NA, reserved106, NA, NA,
		 NA, NA),
	PINGROUP(107, NORTH, uim2_reset, qup13, qup_l6, reserved107, NA, NA,
		 NA, NA, NA),
	PINGROUP(108, NORTH, uim2_present, qup13, reserved108, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(109, NORTH, uim1_data, reserved109, NA, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(110, NORTH, uim1_clk, reserved110, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(111, NORTH, uim1_reset, reserved111, NA, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(112, NORTH, uim1_present, reserved112, NA, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(113, NORTH, uim_batt, edp_hot, reserved113, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(114, NORTH, NA, nav_pps, nav_pps, NA, NA, reserved114, NA, NA,
		 NA),
	PINGROUP(115, NORTH, NA, nav_pps, nav_pps, NA, NA, reserved115, NA, NA,
		 NA),
	PINGROUP(116, NORTH, NA, reserved116, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(117, NORTH, NA, qdss_gpio0, atest_char, reserved117, NA, NA,
		 NA, NA, NA),
	PINGROUP(118, NORTH, adsp_ext, NA, qdss_gpio1, atest_char3,
		 reserved118, NA, NA, NA, NA),
	PINGROUP(119, NORTH, NA, qdss_gpio2, atest_char2, reserved119, NA, NA,
		 NA, NA, NA),
	PINGROUP(120, NORTH, NA, qdss_gpio3, atest_char1, reserved120, NA, NA,
		 NA, NA, NA),
	PINGROUP(121, NORTH, NA, qdss_gpio4, atest_char0, reserved121, NA, NA,
		 NA, NA, NA),
	PINGROUP(122, NORTH, NA, qdss_gpio5, reserved122, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(123, NORTH, qup_l4, NA, qdss_gpio, reserved123, NA, NA, NA,
		 NA, NA),
	PINGROUP(124, NORTH, qup_l5, NA, qdss_gpio, reserved124, NA, NA, NA,
		 NA, NA),
	PINGROUP(125, NORTH, qup_l6, NA, reserved125, NA, NA, NA, NA, NA, NA),
	PINGROUP(126, NORTH, NA, reserved126, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(127, NORTH, NA, reserved127, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(128, NORTH, nav_pps, nav_pps, NA, NA, reserved128, NA, NA, NA,
		 NA),
	PINGROUP(129, NORTH, nav_pps, nav_pps, NA, NA, reserved129, NA, NA, NA,
		 NA),
	PINGROUP(130, NORTH, qlink_request, NA, reserved130, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(131, NORTH, qlink_enable, NA, reserved131, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(132, NORTH, NA, NA, reserved132, NA, NA, NA, NA, NA, NA),
	PINGROUP(133, NORTH, NA, reserved133, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(134, NORTH, NA, reserved134, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(135, NORTH, NA, pa_indicator, NA, reserved135, NA, NA, NA, NA,
		 NA),
	PINGROUP(136, NORTH, NA, reserved136, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(137, NORTH, NA, NA, phase_flag26, reserved137, NA, NA, NA, NA,
		 NA),
	PINGROUP(138, NORTH, NA, NA, phase_flag27, reserved138, NA, NA, NA, NA,
		 NA),
	PINGROUP(139, NORTH, NA, phase_flag28, reserved139, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(140, NORTH, NA, NA, phase_flag6, reserved140, NA, NA, NA, NA,
		 NA),
	PINGROUP(141, NORTH, NA, phase_flag29, reserved141, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(142, NORTH, NA, phase_flag30, reserved142, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(143, NORTH, NA, nav_pps, nav_pps, NA, phase_flag31,
		 reserved143, NA, NA, NA),
	PINGROUP(144, NORTH, mss_lte, reserved144, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(145, NORTH, mss_lte, NA, reserved145, NA, NA, NA, NA, NA, NA),
	PINGROUP(146, NORTH, NA, NA, reserved146, NA, NA, NA, NA, NA, NA),
	PINGROUP(147, NORTH, NA, NA, reserved147, NA, NA, NA, NA, NA, NA),
	PINGROUP(148, NORTH, NA, reserved148, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(149, NORTH, NA, reserved149, NA, NA, NA, NA, NA, NA, NA),
	SDC_QDSD_PINGROUP(sdc2_clk, 0x99a000, 14, 6),
	SDC_QDSD_PINGROUP(sdc2_cmd, 0x99a000, 11, 3),
	SDC_QDSD_PINGROUP(sdc2_data, 0x99a000, 9, 0),
	UFS_RESET(ufs_reset, 0x99f000),
};

static const struct msm_pinctrl_soc_data sdm845_pinctrl = {
	.pins = sdm845_pins,
	.npins = ARRAY_SIZE(sdm845_pins),
	.functions = sdm845_functions,
	.nfunctions = ARRAY_SIZE(sdm845_functions),
	.groups = sdm845_groups,
	.ngroups = ARRAY_SIZE(sdm845_groups),
	.ngpios = 150,
};

static int sdm845_pinctrl_probe(struct platform_device *pdev)
{
	return msm_pinctrl_probe(pdev, &sdm845_pinctrl);
}

static const struct of_device_id sdm845_pinctrl_of_match[] = {
	{ .compatible = "qcom,sdm845-pinctrl", },
	{ },
};

static struct platform_driver sdm845_pinctrl_driver = {
	.driver = {
		.name = "sdm845-pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = sdm845_pinctrl_of_match,
	},
	.probe = sdm845_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init sdm845_pinctrl_init(void)
{
	return platform_driver_register(&sdm845_pinctrl_driver);
}
arch_initcall(sdm845_pinctrl_init);

static void __exit sdm845_pinctrl_exit(void)
{
	platform_driver_unregister(&sdm845_pinctrl_driver);
}
module_exit(sdm845_pinctrl_exit);

MODULE_DESCRIPTION("QTI sdm845 pinctrl driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, sdm845_pinctrl_of_match);
