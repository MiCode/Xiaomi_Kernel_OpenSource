/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#define NORTH	0x00900000
#define SOUTH	0x00D00000
#define WEST	0x00100000
#define EAST	0x00500000
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
		.dir_conn_reg = (base == EAST) ? base + 0xb7000 : \
			((base == WEST) ? base + 0xbb000 : \
			((base == NORTH) ? base + 0xbc000 : base + 0xbe000)), \
		.mux_bit = 2,			\
		.pull_bit = 0,			\
		.drv_bit = 6,			\
		.egpio_enable = 12,		\
		.egpio_present = 11,		\
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
		.dir_conn_en_bit = 8,		\
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
static const struct pinctrl_pin_desc sm8150_pins[] = {
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
	PINCTRL_PIN(150, "GPIO_150"),
	PINCTRL_PIN(151, "GPIO_151"),
	PINCTRL_PIN(152, "GPIO_152"),
	PINCTRL_PIN(153, "GPIO_153"),
	PINCTRL_PIN(154, "GPIO_154"),
	PINCTRL_PIN(155, "GPIO_155"),
	PINCTRL_PIN(156, "GPIO_156"),
	PINCTRL_PIN(157, "GPIO_157"),
	PINCTRL_PIN(158, "GPIO_158"),
	PINCTRL_PIN(159, "GPIO_159"),
	PINCTRL_PIN(160, "GPIO_160"),
	PINCTRL_PIN(161, "GPIO_161"),
	PINCTRL_PIN(162, "GPIO_162"),
	PINCTRL_PIN(163, "GPIO_163"),
	PINCTRL_PIN(164, "GPIO_164"),
	PINCTRL_PIN(165, "GPIO_165"),
	PINCTRL_PIN(166, "GPIO_166"),
	PINCTRL_PIN(167, "GPIO_167"),
	PINCTRL_PIN(168, "GPIO_168"),
	PINCTRL_PIN(169, "GPIO_169"),
	PINCTRL_PIN(170, "GPIO_170"),
	PINCTRL_PIN(171, "GPIO_171"),
	PINCTRL_PIN(172, "GPIO_172"),
	PINCTRL_PIN(173, "GPIO_173"),
	PINCTRL_PIN(174, "GPIO_174"),
	PINCTRL_PIN(175, "SDC2_CLK"),
	PINCTRL_PIN(176, "SDC2_CMD"),
	PINCTRL_PIN(177, "SDC2_DATA"),
	PINCTRL_PIN(178, "UFS_RESET"),
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
DECLARE_MSM_GPIO_PINS(150);
DECLARE_MSM_GPIO_PINS(151);
DECLARE_MSM_GPIO_PINS(152);
DECLARE_MSM_GPIO_PINS(153);
DECLARE_MSM_GPIO_PINS(154);
DECLARE_MSM_GPIO_PINS(155);
DECLARE_MSM_GPIO_PINS(156);
DECLARE_MSM_GPIO_PINS(157);
DECLARE_MSM_GPIO_PINS(158);
DECLARE_MSM_GPIO_PINS(159);
DECLARE_MSM_GPIO_PINS(160);
DECLARE_MSM_GPIO_PINS(161);
DECLARE_MSM_GPIO_PINS(162);
DECLARE_MSM_GPIO_PINS(163);
DECLARE_MSM_GPIO_PINS(164);
DECLARE_MSM_GPIO_PINS(165);
DECLARE_MSM_GPIO_PINS(166);
DECLARE_MSM_GPIO_PINS(167);
DECLARE_MSM_GPIO_PINS(168);
DECLARE_MSM_GPIO_PINS(169);
DECLARE_MSM_GPIO_PINS(170);
DECLARE_MSM_GPIO_PINS(171);
DECLARE_MSM_GPIO_PINS(172);
DECLARE_MSM_GPIO_PINS(173);
DECLARE_MSM_GPIO_PINS(174);

static const unsigned int sdc2_clk_pins[] = { 175 };
static const unsigned int sdc2_cmd_pins[] = { 176 };
static const unsigned int sdc2_data_pins[] = { 177 };
static const unsigned int ufs_reset_pins[] = { 178 };

enum sm8150_functions {
	msm_mux_phase_flag8,
	msm_mux_phase_flag7,
	msm_mux_emac_pps,
	msm_mux_qup12,
	msm_mux_qup16,
	msm_mux_tsif1_clk,
	msm_mux_qup8,
	msm_mux_qspi_cs,
	msm_mux_tgu_ch3,
	msm_mux_tsif1_en,
	msm_mux_qspi0,
	msm_mux_mdp_vsync0,
	msm_mux_mdp_vsync1,
	msm_mux_mdp_vsync2,
	msm_mux_mdp_vsync3,
	msm_mux_tgu_ch0,
	msm_mux_tsif1_data,
	msm_mux_qspi1,
	msm_mux_sdc4_cmd,
	msm_mux_phase_flag31,
	msm_mux_tgu_ch1,
	msm_mux_wlan1_adc1,
	msm_mux_tsif1_sync,
	msm_mux_qspi2,
	msm_mux_sdc43,
	msm_mux_vfr_1,
	msm_mux_phase_flag30,
	msm_mux_tgu_ch2,
	msm_mux_wlan1_adc0,
	msm_mux_tsif2_clk,
	msm_mux_qup11,
	msm_mux_qspi_clk,
	msm_mux_sdc4_clk,
	msm_mux_phase_flag27,
	msm_mux_wlan2_adc1,
	msm_mux_tsif2_en,
	msm_mux_qspi3,
	msm_mux_sdc42,
	msm_mux_phase_flag26,
	msm_mux_wlan2_adc0,
	msm_mux_tsif2_data,
	msm_mux_sdc41,
	msm_mux_phase_flag25,
	msm_mux_tsif2_sync,
	msm_mux_sdc40,
	msm_mux_tsif2_error,
	msm_mux_phase_flag11,
	msm_mux_sd_write,
	msm_mux_tsif1_error,
	msm_mux_qup7,
	msm_mux_ddr_bist,
	msm_mux_ddr_pxi3,
	msm_mux_atest_usb13,
	msm_mux_ddr_pxi1,
	msm_mux_pll_bypassnl,
	msm_mux_atest_usb12,
	msm_mux_pll_reset,
	msm_mux_pci_e1,
	msm_mux_uim2_data,
	msm_mux_uim2_clk,
	msm_mux_uim2_reset,
	msm_mux_uim2_present,
	msm_mux_uim1_data,
	msm_mux_uim1_clk,
	msm_mux_uim1_reset,
	msm_mux_uim1_present,
	msm_mux_uim_batt,
	msm_mux_usb2phy_ac,
	msm_mux_aoss_cti,
	msm_mux_qup1,
	msm_mux_rgmii_txc,
	msm_mux_phase_flag20,
	msm_mux_rgmii_rxc,
	msm_mux_phase_flag19,
	msm_mux_adsp_ext,
	msm_mux_rgmii_rx,
	msm_mux_phase_flag18,
	msm_mux_rgmii_rxd0,
	msm_mux_phase_flag17,
	msm_mux_rgmii_rxd1,
	msm_mux_phase_flag16,
	msm_mux_qup5,
	msm_mux_rgmii_rxd2,
	msm_mux_phase_flag15,
	msm_mux_rgmii_rxd3,
	msm_mux_phase_flag14,
	msm_mux_rgmii_tx,
	msm_mux_phase_flag13,
	msm_mux_rgmii_txd0,
	msm_mux_phase_flag12,
	msm_mux_atest_usb22,
	msm_mux_emac_phy,
	msm_mux_hs3_mi2s,
	msm_mux_sec_mi2s,
	msm_mux_qup2,
	msm_mux_phase_flag9,
	msm_mux_phase_flag4,
	msm_mux_phase_flag21,
	msm_mux_jitter_bist,
	msm_mux_atest_usb21,
	msm_mux_pll_bist,
	msm_mux_atest_usb20,
	msm_mux_atest_char0,
	msm_mux_ter_mi2s,
	msm_mux_gcc_gp1,
	msm_mux_atest_char1,
	msm_mux_atest_char2,
	msm_mux_atest_char3,
	msm_mux_qua_mi2s,
	msm_mux_pri_mi2s,
	msm_mux_qup3,
	msm_mux_phase_flag29,
	msm_mux_ddr_pxi0,
	msm_mux_pri_mi2s_ws,
	msm_mux_phase_flag28,
	msm_mux_vsense_trigger,
	msm_mux_atest_usb1,
	msm_mux_atest_usb11,
	msm_mux_ddr_pxi2,
	msm_mux_dbg_out,
	msm_mux_atest_usb10,
	msm_mux_spkr_i2s,
	msm_mux_audio_ref,
	msm_mux_lpass_slimbus,
	msm_mux_tsense_pwm1,
	msm_mux_tsense_pwm2,
	msm_mux_btfm_slimbus,
	msm_mux_hs1_mi2s,
	msm_mux_cri_trng0,
	msm_mux_hs2_mi2s,
	msm_mux_cri_trng1,
	msm_mux_cri_trng,
	msm_mux_sp_cmu,
	msm_mux_prng_rosc,
	msm_mux_qup0,
	msm_mux_gpio,
	msm_mux_qup6,
	msm_mux_rgmii_txd1,
	msm_mux_rgmii_txd2,
	msm_mux_rgmii_txd3,
	msm_mux_qup_l6,
	msm_mux_rgmii_mdc,
	msm_mux_qup_l5,
	msm_mux_mdp_vsync,
	msm_mux_edp_lcd,
	msm_mux_qup10,
	msm_mux_m_voc,
	msm_mux_edp_hot,
	msm_mux_cam_mclk,
	msm_mux_qdss_gpio0,
	msm_mux_qdss_gpio1,
	msm_mux_qdss_gpio2,
	msm_mux_qdss_gpio3,
	msm_mux_cci_i2c,
	msm_mux_qdss_gpio4,
	msm_mux_phase_flag3,
	msm_mux_qdss_gpio5,
	msm_mux_phase_flag2,
	msm_mux_qdss_gpio6,
	msm_mux_phase_flag1,
	msm_mux_qdss_gpio7,
	msm_mux_cci_timer0,
	msm_mux_gcc_gp2,
	msm_mux_qdss_gpio8,
	msm_mux_cci_timer1,
	msm_mux_gcc_gp3,
	msm_mux_qdss_gpio,
	msm_mux_cci_timer2,
	msm_mux_qup18,
	msm_mux_qdss_gpio9,
	msm_mux_cci_timer3,
	msm_mux_cci_async,
	msm_mux_qdss_gpio10,
	msm_mux_cci_timer4,
	msm_mux_qdss_gpio11,
	msm_mux_qdss_gpio12,
	msm_mux_qup15,
	msm_mux_qdss_gpio15,
	msm_mux_qdss_gpio13,
	msm_mux_qdss_gpio14,
	msm_mux_pci_e0,
	msm_mux_qup_l4,
	msm_mux_agera_pll,
	msm_mux_usb_phy,
	msm_mux_qup9,
	msm_mux_qup13,
	msm_mux_qdss_cti,
	msm_mux_qup14,
	msm_mux_qup4,
	msm_mux_qup17,
	msm_mux_qup19,
	msm_mux_phase_flag5,
	msm_mux_phase_flag0,
	msm_mux_phase_flag22,
	msm_mux_rgmii_mdio,
	msm_mux_phase_flag10,
	msm_mux_atest_char,
	msm_mux_nav_pps,
	msm_mux_atest_usb2,
	msm_mux_qlink_request,
	msm_mux_qlink_enable,
	msm_mux_wmss_reset,
	msm_mux_atest_usb23,
	msm_mux_phase_flag6,
	msm_mux_pa_indicator,
	msm_mux_phase_flag23,
	msm_mux_mss_lte,
	msm_mux_phase_flag24,
	msm_mux_NA,
};

static const char * const phase_flag8_groups[] = {
	"gpio79",
};
static const char * const phase_flag7_groups[] = {
	"gpio80",
};
static const char * const emac_pps_groups[] = {
	"gpio81",
};
static const char * const qup12_groups[] = {
	"gpio83", "gpio84", "gpio85", "gpio86",
};
static const char * const qup16_groups[] = {
	"gpio83", "gpio84", "gpio85", "gpio86",
};
static const char * const tsif1_clk_groups[] = {
	"gpio88",
};
static const char * const qup8_groups[] = {
	"gpio88", "gpio89", "gpio90", "gpio91",
};
static const char * const qspi_cs_groups[] = {
	"gpio88", "gpio94",
};
static const char * const tgu_ch3_groups[] = {
	"gpio88",
};
static const char * const tsif1_en_groups[] = {
	"gpio89",
};
static const char * const qspi0_groups[] = {
	"gpio89",
};
static const char * const mdp_vsync0_groups[] = {
	"gpio89",
};
static const char * const mdp_vsync1_groups[] = {
	"gpio89",
};
static const char * const mdp_vsync2_groups[] = {
	"gpio89",
};
static const char * const mdp_vsync3_groups[] = {
	"gpio89",
};
static const char * const tgu_ch0_groups[] = {
	"gpio89",
};
static const char * const tsif1_data_groups[] = {
	"gpio90",
};
static const char * const qspi1_groups[] = {
	"gpio90",
};
static const char * const sdc4_cmd_groups[] = {
	"gpio90",
};
static const char * const phase_flag31_groups[] = {
	"gpio90",
};
static const char * const tgu_ch1_groups[] = {
	"gpio90",
};
static const char * const wlan1_adc1_groups[] = {
	"gpio90",
};
static const char * const tsif1_sync_groups[] = {
	"gpio91",
};
static const char * const qspi2_groups[] = {
	"gpio91",
};
static const char * const sdc43_groups[] = {
	"gpio91",
};
static const char * const vfr_1_groups[] = {
	"gpio91",
};
static const char * const phase_flag30_groups[] = {
	"gpio91",
};
static const char * const tgu_ch2_groups[] = {
	"gpio91",
};
static const char * const wlan1_adc0_groups[] = {
	"gpio91",
};
static const char * const tsif2_clk_groups[] = {
	"gpio92",
};
static const char * const qup11_groups[] = {
	"gpio92", "gpio93", "gpio94", "gpio95",
};
static const char * const qspi_clk_groups[] = {
	"gpio92",
};
static const char * const sdc4_clk_groups[] = {
	"gpio92",
};
static const char * const phase_flag27_groups[] = {
	"gpio92",
};
static const char * const wlan2_adc1_groups[] = {
	"gpio92",
};
static const char * const tsif2_en_groups[] = {
	"gpio93",
};
static const char * const qspi3_groups[] = {
	"gpio93",
};
static const char * const sdc42_groups[] = {
	"gpio93",
};
static const char * const phase_flag26_groups[] = {
	"gpio93",
};
static const char * const wlan2_adc0_groups[] = {
	"gpio93",
};
static const char * const tsif2_data_groups[] = {
	"gpio94",
};
static const char * const sdc41_groups[] = {
	"gpio94",
};
static const char * const phase_flag25_groups[] = {
	"gpio94",
};
static const char * const tsif2_sync_groups[] = {
	"gpio95",
};
static const char * const sdc40_groups[] = {
	"gpio95",
};
static const char * const tsif2_error_groups[] = {
	"gpio96",
};
static const char * const phase_flag11_groups[] = {
	"gpio96",
};
static const char * const sd_write_groups[] = {
	"gpio97",
};
static const char * const tsif1_error_groups[] = {
	"gpio97",
};
static const char * const qup7_groups[] = {
	"gpio98", "gpio99", "gpio100", "gpio101",
};
static const char * const ddr_bist_groups[] = {
	"gpio98", "gpio99", "gpio145", "gpio146",
};
static const char * const ddr_pxi3_groups[] = {
	"gpio98", "gpio101",
};
static const char * const atest_usb13_groups[] = {
	"gpio99",
};
static const char * const ddr_pxi1_groups[] = {
	"gpio99", "gpio100",
};
static const char * const pll_bypassnl_groups[] = {
	"gpio100",
};
static const char * const atest_usb12_groups[] = {
	"gpio100",
};
static const char * const pll_reset_groups[] = {
	"gpio101",
};
static const char * const pci_e1_groups[] = {
	"gpio102", "gpio103",
};
static const char * const uim2_data_groups[] = {
	"gpio105",
};
static const char * const uim2_clk_groups[] = {
	"gpio106",
};
static const char * const uim2_reset_groups[] = {
	"gpio107",
};
static const char * const uim2_present_groups[] = {
	"gpio108",
};
static const char * const uim1_data_groups[] = {
	"gpio109",
};
static const char * const uim1_clk_groups[] = {
	"gpio110",
};
static const char * const uim1_reset_groups[] = {
	"gpio111",
};
static const char * const uim1_present_groups[] = {
	"gpio112",
};
static const char * const uim_batt_groups[] = {
	"gpio113",
};
static const char * const usb2phy_ac_groups[] = {
	"gpio113", "gpio123",
};
static const char * const aoss_cti_groups[] = {
	"gpio113",
};
static const char * const qup1_groups[] = {
	"gpio114", "gpio115", "gpio116", "gpio117",
};
static const char * const rgmii_txc_groups[] = {
	"gpio114",
};
static const char * const phase_flag20_groups[] = {
	"gpio114",
};
static const char * const rgmii_rxc_groups[] = {
	"gpio115",
};
static const char * const phase_flag19_groups[] = {
	"gpio115",
};
static const char * const adsp_ext_groups[] = {
	"gpio115",
};
static const char * const rgmii_rx_groups[] = {
	"gpio116",
};
static const char * const phase_flag18_groups[] = {
	"gpio116",
};
static const char * const rgmii_rxd0_groups[] = {
	"gpio117",
};
static const char * const phase_flag17_groups[] = {
	"gpio117",
};
static const char * const rgmii_rxd1_groups[] = {
	"gpio118",
};
static const char * const phase_flag16_groups[] = {
	"gpio118",
};
static const char * const qup5_groups[] = {
	"gpio119", "gpio120", "gpio121", "gpio122",
};
static const char * const rgmii_rxd2_groups[] = {
	"gpio119",
};
static const char * const phase_flag15_groups[] = {
	"gpio119",
};
static const char * const rgmii_rxd3_groups[] = {
	"gpio120",
};
static const char * const phase_flag14_groups[] = {
	"gpio120",
};
static const char * const rgmii_tx_groups[] = {
	"gpio121",
};
static const char * const phase_flag13_groups[] = {
	"gpio121",
};
static const char * const rgmii_txd0_groups[] = {
	"gpio122",
};
static const char * const phase_flag12_groups[] = {
	"gpio122",
};
static const char * const atest_usb22_groups[] = {
	"gpio123",
};
static const char * const emac_phy_groups[] = {
	"gpio124",
};
static const char * const hs3_mi2s_groups[] = {
	"gpio125", "gpio165", "gpio166", "gpio167", "gpio168",
};
static const char * const sec_mi2s_groups[] = {
	"gpio126", "gpio127", "gpio128", "gpio129", "gpio130",
};
static const char * const qup2_groups[] = {
	"gpio126", "gpio127", "gpio128", "gpio129",
};
static const char * const phase_flag9_groups[] = {
	"gpio126",
};
static const char * const phase_flag4_groups[] = {
	"gpio127",
};
static const char * const phase_flag21_groups[] = {
	"gpio128",
};
static const char * const jitter_bist_groups[] = {
	"gpio129",
};
static const char * const atest_usb21_groups[] = {
	"gpio129",
};
static const char * const pll_bist_groups[] = {
	"gpio130",
};
static const char * const atest_usb20_groups[] = {
	"gpio130",
};
static const char * const atest_char0_groups[] = {
	"gpio130",
};
static const char * const ter_mi2s_groups[] = {
	"gpio131", "gpio132", "gpio133", "gpio134", "gpio135",
};
static const char * const gcc_gp1_groups[] = {
	"gpio131", "gpio136",
};
static const char * const atest_char1_groups[] = {
	"gpio133",
};
static const char * const atest_char2_groups[] = {
	"gpio134",
};
static const char * const atest_char3_groups[] = {
	"gpio135",
};
static const char * const qua_mi2s_groups[] = {
	"gpio136", "gpio137", "gpio138", "gpio139", "gpio140", "gpio141",
	"gpio142",
};
static const char * const pri_mi2s_groups[] = {
	"gpio143", "gpio144", "gpio146", "gpio147",
};
static const char * const qup3_groups[] = {
	"gpio144", "gpio145", "gpio146", "gpio147",
};
static const char * const phase_flag29_groups[] = {
	"gpio144",
};
static const char * const ddr_pxi0_groups[] = {
	"gpio144", "gpio145",
};
static const char * const pri_mi2s_ws_groups[] = {
	"gpio145",
};
static const char * const phase_flag28_groups[] = {
	"gpio145",
};
static const char * const vsense_trigger_groups[] = {
	"gpio145",
};
static const char * const atest_usb1_groups[] = {
	"gpio145",
};
static const char * const atest_usb11_groups[] = {
	"gpio146",
};
static const char * const ddr_pxi2_groups[] = {
	"gpio146", "gpio147",
};
static const char * const dbg_out_groups[] = {
	"gpio147",
};
static const char * const atest_usb10_groups[] = {
	"gpio147",
};
static const char * const spkr_i2s_groups[] = {
	"gpio148", "gpio149", "gpio150", "gpio151", "gpio152",
};
static const char * const audio_ref_groups[] = {
	"gpio148",
};
static const char * const lpass_slimbus_groups[] = {
	"gpio149", "gpio150", "gpio151", "gpio152",
};
static const char * const tsense_pwm1_groups[] = {
	"gpio150",
};
static const char * const tsense_pwm2_groups[] = {
	"gpio150",
};
static const char * const btfm_slimbus_groups[] = {
	"gpio153", "gpio154",
};
static const char * const hs1_mi2s_groups[] = {
	"gpio155", "gpio156", "gpio157", "gpio158", "gpio159",
};
static const char * const cri_trng0_groups[] = {
	"gpio159",
};
static const char * const hs2_mi2s_groups[] = {
	"gpio160", "gpio161", "gpio162", "gpio163", "gpio164",
};
static const char * const cri_trng1_groups[] = {
	"gpio160",
};
static const char * const cri_trng_groups[] = {
	"gpio161",
};
static const char * const sp_cmu_groups[] = {
	"gpio162",
};
static const char * const prng_rosc_groups[] = {
	"gpio163",
};
static const char * const qup0_groups[] = {
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
	"gpio50", "gpio51", "gpio52", "gpio53", "gpio54", "gpio55", "gpio56",
	"gpio57", "gpio58", "gpio59", "gpio60", "gpio61", "gpio62", "gpio63",
	"gpio64", "gpio65", "gpio66", "gpio67", "gpio68", "gpio69", "gpio70",
	"gpio71", "gpio72", "gpio73", "gpio74", "gpio75", "gpio76", "gpio77",
	"gpio78", "gpio79", "gpio80", "gpio81", "gpio82", "gpio83", "gpio84",
	"gpio85", "gpio86", "gpio87", "gpio88", "gpio89", "gpio90", "gpio91",
	"gpio92", "gpio93", "gpio94", "gpio95", "gpio96", "gpio97", "gpio98",
	"gpio99", "gpio100", "gpio101", "gpio102", "gpio103", "gpio104",
	"gpio105", "gpio106", "gpio107", "gpio108", "gpio109", "gpio110",
	"gpio111", "gpio112", "gpio113", "gpio114", "gpio115", "gpio116",
	"gpio117", "gpio118", "gpio119", "gpio120", "gpio121", "gpio122",
	"gpio123", "gpio124", "gpio125", "gpio126", "gpio127", "gpio128",
	"gpio129", "gpio130", "gpio131", "gpio132", "gpio133", "gpio134",
	"gpio135", "gpio136", "gpio137", "gpio138", "gpio139", "gpio140",
	"gpio141", "gpio142", "gpio143", "gpio144", "gpio145", "gpio146",
	"gpio147", "gpio148", "gpio149", "gpio150", "gpio151", "gpio152",
	"gpio153", "gpio154", "gpio155", "gpio156", "gpio157", "gpio158",
	"gpio159", "gpio160", "gpio161", "gpio162", "gpio163", "gpio164",
	"gpio165", "gpio166", "gpio167", "gpio168", "gpio169", "gpio170",
	"gpio171", "gpio172", "gpio173", "gpio174",
};
static const char * const qup6_groups[] = {
	"gpio4", "gpio5", "gpio6", "gpio7",
};
static const char * const rgmii_txd1_groups[] = {
	"gpio4",
};
static const char * const rgmii_txd2_groups[] = {
	"gpio5",
};
static const char * const rgmii_txd3_groups[] = {
	"gpio6",
};
static const char * const qup_l6_groups[] = {
	"gpio6", "gpio34", "gpio97", "gpio123",
};
static const char * const rgmii_mdc_groups[] = {
	"gpio7",
};
static const char * const qup_l5_groups[] = {
	"gpio7", "gpio33", "gpio82", "gpio96",
};
static const char * const mdp_vsync_groups[] = {
	"gpio8", "gpio9", "gpio10", "gpio81", "gpio82",
};
static const char * const edp_lcd_groups[] = {
	"gpio9",
};
static const char * const qup10_groups[] = {
	"gpio9", "gpio10", "gpio11", "gpio12",
};
static const char * const m_voc_groups[] = {
	"gpio10",
};
static const char * const edp_hot_groups[] = {
	"gpio10",
};
static const char * const cam_mclk_groups[] = {
	"gpio13", "gpio14", "gpio15", "gpio16",
};
static const char * const qdss_gpio0_groups[] = {
	"gpio13", "gpio32",
};
static const char * const qdss_gpio1_groups[] = {
	"gpio14", "gpio33",
};
static const char * const qdss_gpio2_groups[] = {
	"gpio15", "gpio83",
};
static const char * const qdss_gpio3_groups[] = {
	"gpio16", "gpio117",
};
static const char * const cci_i2c_groups[] = {
	"gpio17", "gpio18", "gpio19", "gpio20", "gpio31", "gpio32", "gpio33",
	"gpio34",
};
static const char * const qdss_gpio4_groups[] = {
	"gpio17", "gpio118",
};
static const char * const phase_flag3_groups[] = {
	"gpio18",
};
static const char * const qdss_gpio5_groups[] = {
	"gpio18", "gpio119",
};
static const char * const phase_flag2_groups[] = {
	"gpio19",
};
static const char * const qdss_gpio6_groups[] = {
	"gpio19", "gpio39",
};
static const char * const phase_flag1_groups[] = {
	"gpio20",
};
static const char * const qdss_gpio7_groups[] = {
	"gpio20", "gpio40",
};
static const char * const cci_timer0_groups[] = {
	"gpio21",
};
static const char * const gcc_gp2_groups[] = {
	"gpio21", "gpio137",
};
static const char * const qdss_gpio8_groups[] = {
	"gpio21", "gpio133",
};
static const char * const cci_timer1_groups[] = {
	"gpio22",
};
static const char * const gcc_gp3_groups[] = {
	"gpio22", "gpio138",
};
static const char * const qdss_gpio_groups[] = {
	"gpio22", "gpio28", "gpio120", "gpio121",
};
static const char * const cci_timer2_groups[] = {
	"gpio23",
};
static const char * const qup18_groups[] = {
	"gpio23", "gpio24", "gpio25", "gpio26",
};
static const char * const qdss_gpio9_groups[] = {
	"gpio23", "gpio134",
};
static const char * const cci_timer3_groups[] = {
	"gpio24",
};
static const char * const cci_async_groups[] = {
	"gpio24", "gpio25", "gpio26",
};
static const char * const qdss_gpio10_groups[] = {
	"gpio24", "gpio48",
};
static const char * const cci_timer4_groups[] = {
	"gpio25",
};
static const char * const qdss_gpio11_groups[] = {
	"gpio25", "gpio132",
};
static const char * const qdss_gpio12_groups[] = {
	"gpio26", "gpio47",
};
static const char * const qup15_groups[] = {
	"gpio27", "gpio28", "gpio29", "gpio30",
};
static const char * const qdss_gpio15_groups[] = {
	"gpio27", "gpio42",
};
static const char * const qdss_gpio13_groups[] = {
	"gpio29", "gpio31",
};
static const char * const qdss_gpio14_groups[] = {
	"gpio30", "gpio41",
};
static const char * const pci_e0_groups[] = {
	"gpio35", "gpio36",
};
static const char * const qup_l4_groups[] = {
	"gpio37", "gpio59", "gpio81", "gpio95",
};
static const char * const agera_pll_groups[] = {
	"gpio37",
};
static const char * const usb_phy_groups[] = {
	"gpio38",
};
static const char * const qup9_groups[] = {
	"gpio39", "gpio40", "gpio41", "gpio42",
};
static const char * const qup13_groups[] = {
	"gpio43", "gpio44", "gpio45", "gpio46",
};
static const char * const qdss_cti_groups[] = {
	"gpio45", "gpio46", "gpio49", "gpio50", "gpio56", "gpio57", "gpio58",
	"gpio58",
};
static const char * const qup14_groups[] = {
	"gpio47", "gpio48", "gpio49", "gpio50",
};
static const char * const qup4_groups[] = {
	"gpio51", "gpio52", "gpio53", "gpio54",
};
static const char * const qup17_groups[] = {
	"gpio55", "gpio56", "gpio57", "gpio58",
};
static const char * const qup19_groups[] = {
	"gpio55", "gpio56", "gpio57", "gpio58",
};
static const char * const phase_flag5_groups[] = {
	"gpio55",
};
static const char * const phase_flag0_groups[] = {
	"gpio56",
};
static const char * const phase_flag22_groups[] = {
	"gpio57",
};
static const char * const rgmii_mdio_groups[] = {
	"gpio59",
};
static const char * const phase_flag10_groups[] = {
	"gpio59",
};
static const char * const atest_char_groups[] = {
	"gpio59",
};
static const char * const nav_pps_groups[] = {
	"gpio60", "gpio60", "gpio76", "gpio76", "gpio77", "gpio77", "gpio81",
	"gpio81", "gpio82", "gpio82",
};
static const char * const atest_usb2_groups[] = {
	"gpio60",
};
static const char * const qlink_request_groups[] = {
	"gpio61",
};
static const char * const qlink_enable_groups[] = {
	"gpio62",
};
static const char * const wmss_reset_groups[] = {
	"gpio63",
};
static const char * const atest_usb23_groups[] = {
	"gpio63",
};
static const char * const phase_flag6_groups[] = {
	"gpio64",
};
static const char * const pa_indicator_groups[] = {
	"gpio68",
};
static const char * const phase_flag23_groups[] = {
	"gpio68",
};
static const char * const mss_lte_groups[] = {
	"gpio69", "gpio70",
};
static const char * const phase_flag24_groups[] = {
	"gpio76",
};

static const struct msm_function sm8150_functions[] = {
	FUNCTION(phase_flag8),
	FUNCTION(phase_flag7),
	FUNCTION(emac_pps),
	FUNCTION(qup12),
	FUNCTION(qup16),
	FUNCTION(tsif1_clk),
	FUNCTION(qup8),
	FUNCTION(qspi_cs),
	FUNCTION(tgu_ch3),
	FUNCTION(tsif1_en),
	FUNCTION(qspi0),
	FUNCTION(mdp_vsync0),
	FUNCTION(mdp_vsync1),
	FUNCTION(mdp_vsync2),
	FUNCTION(mdp_vsync3),
	FUNCTION(tgu_ch0),
	FUNCTION(tsif1_data),
	FUNCTION(qspi1),
	FUNCTION(sdc4_cmd),
	FUNCTION(phase_flag31),
	FUNCTION(tgu_ch1),
	FUNCTION(wlan1_adc1),
	FUNCTION(tsif1_sync),
	FUNCTION(qspi2),
	FUNCTION(sdc43),
	FUNCTION(vfr_1),
	FUNCTION(phase_flag30),
	FUNCTION(tgu_ch2),
	FUNCTION(wlan1_adc0),
	FUNCTION(tsif2_clk),
	FUNCTION(qup11),
	FUNCTION(qspi_clk),
	FUNCTION(sdc4_clk),
	FUNCTION(phase_flag27),
	FUNCTION(wlan2_adc1),
	FUNCTION(tsif2_en),
	FUNCTION(qspi3),
	FUNCTION(sdc42),
	FUNCTION(phase_flag26),
	FUNCTION(wlan2_adc0),
	FUNCTION(tsif2_data),
	FUNCTION(sdc41),
	FUNCTION(phase_flag25),
	FUNCTION(tsif2_sync),
	FUNCTION(sdc40),
	FUNCTION(tsif2_error),
	FUNCTION(phase_flag11),
	FUNCTION(sd_write),
	FUNCTION(tsif1_error),
	FUNCTION(qup7),
	FUNCTION(ddr_bist),
	FUNCTION(ddr_pxi3),
	FUNCTION(atest_usb13),
	FUNCTION(ddr_pxi1),
	FUNCTION(pll_bypassnl),
	FUNCTION(atest_usb12),
	FUNCTION(pll_reset),
	FUNCTION(pci_e1),
	FUNCTION(uim2_data),
	FUNCTION(uim2_clk),
	FUNCTION(uim2_reset),
	FUNCTION(uim2_present),
	FUNCTION(uim1_data),
	FUNCTION(uim1_clk),
	FUNCTION(uim1_reset),
	FUNCTION(uim1_present),
	FUNCTION(uim_batt),
	FUNCTION(usb2phy_ac),
	FUNCTION(aoss_cti),
	FUNCTION(qup1),
	FUNCTION(rgmii_txc),
	FUNCTION(phase_flag20),
	FUNCTION(rgmii_rxc),
	FUNCTION(phase_flag19),
	FUNCTION(adsp_ext),
	FUNCTION(rgmii_rx),
	FUNCTION(phase_flag18),
	FUNCTION(rgmii_rxd0),
	FUNCTION(phase_flag17),
	FUNCTION(rgmii_rxd1),
	FUNCTION(phase_flag16),
	FUNCTION(qup5),
	FUNCTION(rgmii_rxd2),
	FUNCTION(phase_flag15),
	FUNCTION(rgmii_rxd3),
	FUNCTION(phase_flag14),
	FUNCTION(rgmii_tx),
	FUNCTION(phase_flag13),
	FUNCTION(rgmii_txd0),
	FUNCTION(phase_flag12),
	FUNCTION(atest_usb22),
	FUNCTION(emac_phy),
	FUNCTION(hs3_mi2s),
	FUNCTION(sec_mi2s),
	FUNCTION(qup2),
	FUNCTION(phase_flag9),
	FUNCTION(phase_flag4),
	FUNCTION(phase_flag21),
	FUNCTION(jitter_bist),
	FUNCTION(atest_usb21),
	FUNCTION(pll_bist),
	FUNCTION(atest_usb20),
	FUNCTION(atest_char0),
	FUNCTION(ter_mi2s),
	FUNCTION(gcc_gp1),
	FUNCTION(atest_char1),
	FUNCTION(atest_char2),
	FUNCTION(atest_char3),
	FUNCTION(qua_mi2s),
	FUNCTION(pri_mi2s),
	FUNCTION(qup3),
	FUNCTION(phase_flag29),
	FUNCTION(ddr_pxi0),
	FUNCTION(pri_mi2s_ws),
	FUNCTION(phase_flag28),
	FUNCTION(vsense_trigger),
	FUNCTION(atest_usb1),
	FUNCTION(atest_usb11),
	FUNCTION(ddr_pxi2),
	FUNCTION(dbg_out),
	FUNCTION(atest_usb10),
	FUNCTION(spkr_i2s),
	FUNCTION(audio_ref),
	FUNCTION(lpass_slimbus),
	FUNCTION(tsense_pwm1),
	FUNCTION(tsense_pwm2),
	FUNCTION(btfm_slimbus),
	FUNCTION(hs1_mi2s),
	FUNCTION(cri_trng0),
	FUNCTION(hs2_mi2s),
	FUNCTION(cri_trng1),
	FUNCTION(cri_trng),
	FUNCTION(sp_cmu),
	FUNCTION(prng_rosc),
	FUNCTION(qup0),
	FUNCTION(gpio),
	FUNCTION(qup6),
	FUNCTION(rgmii_txd1),
	FUNCTION(rgmii_txd2),
	FUNCTION(rgmii_txd3),
	FUNCTION(qup_l6),
	FUNCTION(rgmii_mdc),
	FUNCTION(qup_l5),
	FUNCTION(mdp_vsync),
	FUNCTION(edp_lcd),
	FUNCTION(qup10),
	FUNCTION(m_voc),
	FUNCTION(edp_hot),
	FUNCTION(cam_mclk),
	FUNCTION(qdss_gpio0),
	FUNCTION(qdss_gpio1),
	FUNCTION(qdss_gpio2),
	FUNCTION(qdss_gpio3),
	FUNCTION(cci_i2c),
	FUNCTION(qdss_gpio4),
	FUNCTION(phase_flag3),
	FUNCTION(qdss_gpio5),
	FUNCTION(phase_flag2),
	FUNCTION(qdss_gpio6),
	FUNCTION(phase_flag1),
	FUNCTION(qdss_gpio7),
	FUNCTION(cci_timer0),
	FUNCTION(gcc_gp2),
	FUNCTION(qdss_gpio8),
	FUNCTION(cci_timer1),
	FUNCTION(gcc_gp3),
	FUNCTION(qdss_gpio),
	FUNCTION(cci_timer2),
	FUNCTION(qup18),
	FUNCTION(qdss_gpio9),
	FUNCTION(cci_timer3),
	FUNCTION(cci_async),
	FUNCTION(qdss_gpio10),
	FUNCTION(cci_timer4),
	FUNCTION(qdss_gpio11),
	FUNCTION(qdss_gpio12),
	FUNCTION(qup15),
	FUNCTION(qdss_gpio15),
	FUNCTION(qdss_gpio13),
	FUNCTION(qdss_gpio14),
	FUNCTION(pci_e0),
	FUNCTION(qup_l4),
	FUNCTION(agera_pll),
	FUNCTION(usb_phy),
	FUNCTION(qup9),
	FUNCTION(qup13),
	FUNCTION(qdss_cti),
	FUNCTION(qup14),
	FUNCTION(qup4),
	FUNCTION(qup17),
	FUNCTION(qup19),
	FUNCTION(phase_flag5),
	FUNCTION(phase_flag0),
	FUNCTION(phase_flag22),
	FUNCTION(rgmii_mdio),
	FUNCTION(phase_flag10),
	FUNCTION(atest_char),
	FUNCTION(nav_pps),
	FUNCTION(atest_usb2),
	FUNCTION(qlink_request),
	FUNCTION(qlink_enable),
	FUNCTION(wmss_reset),
	FUNCTION(atest_usb23),
	FUNCTION(phase_flag6),
	FUNCTION(pa_indicator),
	FUNCTION(phase_flag23),
	FUNCTION(mss_lte),
	FUNCTION(phase_flag24),
};

/* Every pin is maintained as a single group, and missing or non-existing pin
 * would be maintained as dummy group to synchronize pin group index with
 * pin descriptor registered with pinctrl core.
 * Clients would not be able to request these dummy pin groups.
 */
static const struct msm_pingroup sm8150_groups[] = {
	[0] = PINGROUP(0, SOUTH, qup0, NA, NA, NA, NA, NA, NA, NA, NA),
	[1] = PINGROUP(1, SOUTH, qup0, NA, NA, NA, NA, NA, NA, NA, NA),
	[2] = PINGROUP(2, SOUTH, qup0, NA, NA, NA, NA, NA, NA, NA, NA),
	[3] = PINGROUP(3, SOUTH, qup0, NA, NA, NA, NA, NA, NA, NA, NA),
	[4] = PINGROUP(4, SOUTH, qup6, rgmii_txd1, NA, NA, NA, NA, NA, NA, NA),
	[5] = PINGROUP(5, SOUTH, qup6, rgmii_txd2, NA, NA, NA, NA, NA, NA, NA),
	[6] = PINGROUP(6, SOUTH, qup6, rgmii_txd3, qup_l6, NA, NA, NA, NA, NA,
		       NA),
	[7] = PINGROUP(7, SOUTH, qup6, rgmii_mdc, qup_l5, NA, NA, NA, NA, NA,
		       NA),
	[8] = PINGROUP(8, NORTH, mdp_vsync, NA, NA, NA, NA, NA, NA, NA, NA),
	[9] = PINGROUP(9, NORTH, mdp_vsync, edp_lcd, qup10, NA, NA, NA, NA, NA,
		       NA),
	[10] = PINGROUP(10, NORTH, mdp_vsync, m_voc, edp_hot, qup10, NA, NA,
			NA, NA, NA),
	[11] = PINGROUP(11, NORTH, qup10, NA, NA, NA, NA, NA, NA, NA, NA),
	[12] = PINGROUP(12, NORTH, qup10, NA, NA, NA, NA, NA, NA, NA, NA),
	[13] = PINGROUP(13, NORTH, cam_mclk, qdss_gpio0, NA, NA, NA, NA, NA,
			NA, NA),
	[14] = PINGROUP(14, NORTH, cam_mclk, qdss_gpio1, NA, NA, NA, NA, NA,
			NA, NA),
	[15] = PINGROUP(15, NORTH, cam_mclk, qdss_gpio2, NA, NA, NA, NA, NA,
			NA, NA),
	[16] = PINGROUP(16, NORTH, cam_mclk, qdss_gpio3, NA, NA, NA, NA, NA,
			NA, NA),
	[17] = PINGROUP(17, NORTH, cci_i2c, qdss_gpio4, NA, NA, NA, NA, NA, NA,
			NA),
	[18] = PINGROUP(18, NORTH, cci_i2c, phase_flag3, NA, qdss_gpio5, NA,
			NA, NA, NA, NA),
	[19] = PINGROUP(19, NORTH, cci_i2c, phase_flag2, NA, qdss_gpio6, NA,
			NA, NA, NA, NA),
	[20] = PINGROUP(20, NORTH, cci_i2c, phase_flag1, NA, qdss_gpio7, NA,
			NA, NA, NA, NA),
	[21] = PINGROUP(21, EAST, cci_timer0, gcc_gp2, qdss_gpio8, NA, NA, NA,
			NA, NA, NA),
	[22] = PINGROUP(22, EAST, cci_timer1, gcc_gp3, qdss_gpio, NA, NA, NA,
			NA, NA, NA),
	[23] = PINGROUP(23, EAST, cci_timer2, qup18, qdss_gpio9, NA, NA, NA,
			NA, NA, NA),
	[24] = PINGROUP(24, EAST, cci_timer3, cci_async, qup18, qdss_gpio10,
			NA, NA, NA, NA, NA),
	[25] = PINGROUP(25, EAST, cci_timer4, cci_async, qup18, qdss_gpio11,
			NA, NA, NA, NA, NA),
	[26] = PINGROUP(26, EAST, cci_async, qup18, qdss_gpio12, NA, NA, NA,
			NA, NA, NA),
	[27] = PINGROUP(27, EAST, qup15, NA, qdss_gpio15, NA, NA, NA, NA, NA,
			NA),
	[28] = PINGROUP(28, EAST, qup15, qdss_gpio, NA, NA, NA, NA, NA, NA, NA),
	[29] = PINGROUP(29, EAST, qup15, qdss_gpio13, NA, NA, NA, NA, NA, NA,
			NA),
	[30] = PINGROUP(30, EAST, qup15, qdss_gpio14, NA, NA, NA, NA, NA, NA,
			NA),
	[31] = PINGROUP(31, NORTH, cci_i2c, qdss_gpio13, NA, NA, NA, NA, NA,
			NA, NA),
	[32] = PINGROUP(32, NORTH, cci_i2c, qdss_gpio0, NA, NA, NA, NA, NA, NA,
			NA),
	[33] = PINGROUP(33, NORTH, cci_i2c, qup_l5, qdss_gpio1, NA, NA, NA, NA,
			NA, NA),
	[34] = PINGROUP(34, NORTH, cci_i2c, qup_l6, NA, NA, NA, NA, NA, NA, NA),
	[35] = PINGROUP(35, NORTH, pci_e0, NA, NA, NA, NA, NA, NA, NA, NA),
	[36] = PINGROUP(36, NORTH, pci_e0, NA, NA, NA, NA, NA, NA, NA, NA),
	[37] = PINGROUP(37, NORTH, qup_l4, agera_pll, NA, NA, NA, NA, NA, NA,
			NA),
	[38] = PINGROUP(38, SOUTH, usb_phy, NA, NA, NA, NA, NA, NA, NA, NA),
	[39] = PINGROUP(39, NORTH, qup9, qdss_gpio6, NA, NA, NA, NA, NA, NA,
			NA),
	[40] = PINGROUP(40, NORTH, qup9, qdss_gpio7, NA, NA, NA, NA, NA, NA,
			NA),
	[41] = PINGROUP(41, NORTH, qup9, qdss_gpio14, NA, NA, NA, NA, NA, NA,
			NA),
	[42] = PINGROUP(42, NORTH, qup9, qdss_gpio15, NA, NA, NA, NA, NA, NA,
			NA),
	[43] = PINGROUP(43, EAST, qup13, NA, NA, NA, NA, NA, NA, NA, NA),
	[44] = PINGROUP(44, EAST, qup13, NA, NA, NA, NA, NA, NA, NA, NA),
	[45] = PINGROUP(45, EAST, qup13, qdss_cti, NA, NA, NA, NA, NA, NA, NA),
	[46] = PINGROUP(46, EAST, qup13, qdss_cti, NA, NA, NA, NA, NA, NA, NA),
	[47] = PINGROUP(47, EAST, qup14, qdss_gpio12, NA, NA, NA, NA, NA, NA,
			NA),
	[48] = PINGROUP(48, EAST, qup14, qdss_gpio10, NA, NA, NA, NA, NA, NA,
			NA),
	[49] = PINGROUP(49, EAST, qup14, NA, qdss_cti, NA, NA, NA, NA, NA, NA),
	[50] = PINGROUP(50, EAST, qup14, qdss_cti, NA, NA, NA, NA, NA, NA, NA),
	[51] = PINGROUP(51, SOUTH, qup4, NA, NA, NA, NA, NA, NA, NA, NA),
	[52] = PINGROUP(52, SOUTH, qup4, NA, NA, NA, NA, NA, NA, NA, NA),
	[53] = PINGROUP(53, SOUTH, qup4, NA, NA, NA, NA, NA, NA, NA, NA),
	[54] = PINGROUP(54, SOUTH, qup4, NA, NA, NA, NA, NA, NA, NA, NA),
	[55] = PINGROUP(55, SOUTH, qup17, qup19, phase_flag5, NA, NA, NA, NA,
			NA, NA),
	[56] = PINGROUP(56, SOUTH, qup17, qup19, qdss_cti, phase_flag0, NA, NA,
			NA, NA, NA),
	[57] = PINGROUP(57, SOUTH, qup17, qup19, qdss_cti, phase_flag22, NA,
			NA, NA, NA, NA),
	[58] = PINGROUP(58, SOUTH, qup17, qup19, qdss_cti, qdss_cti, NA, NA,
			NA, NA, NA),
	[59] = PINGROUP(59, SOUTH, rgmii_mdio, qup_l4, phase_flag10, NA,
			atest_char, NA, NA, NA, NA),
	[60] = PINGROUP(60, SOUTH, NA, nav_pps, nav_pps, atest_usb2, NA, NA,
			NA, NA, NA),
	[61] = PINGROUP(61, SOUTH, qlink_request, NA, NA, NA, NA, NA, NA, NA,
			NA),
	[62] = PINGROUP(62, SOUTH, qlink_enable, NA, NA, NA, NA, NA, NA, NA,
			NA),
	[63] = PINGROUP(63, SOUTH, wmss_reset, atest_usb23, NA, NA, NA, NA, NA,
			NA, NA),
	[64] = PINGROUP(64, SOUTH, NA, phase_flag6, NA, NA, NA, NA, NA, NA, NA),
	[65] = PINGROUP(65, SOUTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[66] = PINGROUP(66, SOUTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[67] = PINGROUP(67, SOUTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[68] = PINGROUP(68, SOUTH, NA, pa_indicator, phase_flag23, NA, NA, NA,
			NA, NA, NA),
	[69] = PINGROUP(69, SOUTH, mss_lte, NA, NA, NA, NA, NA, NA, NA, NA),
	[70] = PINGROUP(70, SOUTH, mss_lte, NA, NA, NA, NA, NA, NA, NA, NA),
	[71] = PINGROUP(71, SOUTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[72] = PINGROUP(72, SOUTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[73] = PINGROUP(73, SOUTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[74] = PINGROUP(74, SOUTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[75] = PINGROUP(75, SOUTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[76] = PINGROUP(76, SOUTH, NA, NA, NA, nav_pps, nav_pps, phase_flag24,
			NA, NA, NA),
	[77] = PINGROUP(77, SOUTH, NA, NA, NA, nav_pps, nav_pps, NA, NA, NA,
			NA),
	[78] = PINGROUP(78, SOUTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[79] = PINGROUP(79, SOUTH, NA, NA, phase_flag8, NA, NA, NA, NA, NA, NA),
	[80] = PINGROUP(80, SOUTH, NA, NA, phase_flag7, NA, NA, NA, NA, NA, NA),
	[81] = PINGROUP(81, SOUTH, NA, NA, NA, nav_pps, nav_pps, qup_l4,
			mdp_vsync, emac_pps, NA),
	[82] = PINGROUP(82, SOUTH, NA, NA, NA, nav_pps, nav_pps, qup_l5,
			mdp_vsync, NA, NA),
	[83] = PINGROUP(83, NORTH, qup12, qup16, NA, qdss_gpio2, NA, NA, NA,
			NA, NA),
	[84] = PINGROUP(84, NORTH, qup12, qup16, NA, NA, NA, NA, NA, NA, NA),
	[85] = PINGROUP(85, NORTH, qup12, qup16, NA, NA, NA, NA, NA, NA, NA),
	[86] = PINGROUP(86, NORTH, qup12, qup16, NA, NA, NA, NA, NA, NA, NA),
	[87] = PINGROUP(87, EAST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[88] = PINGROUP(88, NORTH, tsif1_clk, qup8, qspi_cs, tgu_ch3, NA, NA,
			NA, NA, NA),
	[89] = PINGROUP(89, NORTH, tsif1_en, qup8, qspi0, mdp_vsync0,
			mdp_vsync1, mdp_vsync2, mdp_vsync3, tgu_ch0, NA),
	[90] = PINGROUP(90, NORTH, tsif1_data, qup8, qspi1, sdc4_cmd,
			phase_flag31, tgu_ch1, NA, NA, wlan1_adc1),
	[91] = PINGROUP(91, NORTH, tsif1_sync, qup8, qspi2, sdc43, vfr_1,
			phase_flag30, tgu_ch2, NA, NA),
	[92] = PINGROUP(92, NORTH, tsif2_clk, qup11, qspi_clk, sdc4_clk,
			phase_flag27, NA, wlan2_adc1, NA, NA),
	[93] = PINGROUP(93, NORTH, tsif2_en, qup11, qspi3, sdc42, phase_flag26,
			NA, wlan2_adc0, NA, NA),
	[94] = PINGROUP(94, NORTH, tsif2_data, qup11, qspi_cs, sdc41,
			phase_flag25, NA, NA, NA, NA),
	[95] = PINGROUP(95, NORTH, tsif2_sync, qup11, sdc40, qup_l4, NA, NA,
			NA, NA, NA),
	[96] = PINGROUP(96, NORTH, tsif2_error, qup_l5, phase_flag11, NA, NA,
			NA, NA, NA, NA),
	[97] = PINGROUP(97, NORTH, sd_write, tsif1_error, qup_l6, NA, NA, NA,
			NA, NA, NA),
	[98] = PINGROUP(98, SOUTH, qup7, ddr_bist, ddr_pxi3, NA, NA, NA, NA,
			NA, NA),
	[99] = PINGROUP(99, SOUTH, qup7, ddr_bist, atest_usb13, ddr_pxi1, NA,
			NA, NA, NA, NA),
	[100] = PINGROUP(100, SOUTH, qup7, pll_bypassnl, atest_usb12, ddr_pxi1,
			 NA, NA, NA, NA, NA),
	[101] = PINGROUP(101, SOUTH, qup7, pll_reset, ddr_pxi3, NA, NA, NA, NA,
			 NA, NA),
	[102] = PINGROUP(102, NORTH, pci_e1, NA, NA, NA, NA, NA, NA, NA, NA),
	[103] = PINGROUP(103, NORTH, pci_e1, NA, NA, NA, NA, NA, NA, NA, NA),
	[104] = PINGROUP(104, NORTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[105] = PINGROUP(105, WEST, uim2_data, NA, NA, NA, NA, NA, NA, NA, NA),
	[106] = PINGROUP(106, WEST, uim2_clk, NA, NA, NA, NA, NA, NA, NA, NA),
	[107] = PINGROUP(107, WEST, uim2_reset, NA, NA, NA, NA, NA, NA, NA, NA),
	[108] = PINGROUP(108, WEST, uim2_present, NA, NA, NA, NA, NA, NA, NA,
			 NA),
	[109] = PINGROUP(109, WEST, uim1_data, NA, NA, NA, NA, NA, NA, NA, NA),
	[110] = PINGROUP(110, WEST, uim1_clk, NA, NA, NA, NA, NA, NA, NA, NA),
	[111] = PINGROUP(111, WEST, uim1_reset, NA, NA, NA, NA, NA, NA, NA, NA),
	[112] = PINGROUP(112, WEST, uim1_present, NA, NA, NA, NA, NA, NA, NA,
			 NA),
	[113] = PINGROUP(113, WEST, uim_batt, usb2phy_ac, aoss_cti, NA, NA, NA,
			 NA, NA, NA),
	[114] = PINGROUP(114, SOUTH, qup1, rgmii_txc, phase_flag20, NA, NA, NA,
			 NA, NA, NA),
	[115] = PINGROUP(115, SOUTH, qup1, rgmii_rxc, phase_flag19, adsp_ext,
			 NA, NA, NA, NA, NA),
	[116] = PINGROUP(116, SOUTH, qup1, rgmii_rx, phase_flag18, NA, NA, NA,
			 NA, NA, NA),
	[117] = PINGROUP(117, SOUTH, qup1, rgmii_rxd0, phase_flag17, NA,
			 qdss_gpio3, NA, NA, NA, NA),
	[118] = PINGROUP(118, SOUTH, rgmii_rxd1, phase_flag16, NA, qdss_gpio4,
			 NA, NA, NA, NA, NA),
	[119] = PINGROUP(119, SOUTH, qup5, rgmii_rxd2, phase_flag15, NA,
			 qdss_gpio5, NA, NA, NA, NA),
	[120] = PINGROUP(120, SOUTH, qup5, rgmii_rxd3, phase_flag14, NA,
			 qdss_gpio, NA, NA, NA, NA),
	[121] = PINGROUP(121, SOUTH, qup5, rgmii_tx, phase_flag13, NA,
			 qdss_gpio, NA, NA, NA, NA),
	[122] = PINGROUP(122, SOUTH, qup5, rgmii_txd0, phase_flag12, NA, NA,
			 NA, NA, NA, NA),
	[123] = PINGROUP(123, SOUTH, usb2phy_ac, qup_l6, atest_usb22, NA, NA,
			 NA, NA, NA, NA),
	[124] = PINGROUP(124, SOUTH, emac_phy, NA, NA, NA, NA, NA, NA, NA, NA),
	[125] = PINGROUP(125, WEST, hs3_mi2s, NA, NA, NA, NA, NA, NA, NA, NA),
	[126] = PINGROUP(126, SOUTH, sec_mi2s, qup2, phase_flag9, NA, NA, NA,
			 NA, NA, NA),
	[127] = PINGROUP(127, SOUTH, sec_mi2s, qup2, phase_flag4, NA, NA, NA,
			 NA, NA, NA),
	[128] = PINGROUP(128, SOUTH, sec_mi2s, qup2, phase_flag21, NA, NA, NA,
			 NA, NA, NA),
	[129] = PINGROUP(129, SOUTH, sec_mi2s, qup2, jitter_bist, atest_usb21,
			 NA, NA, NA, NA, NA),
	[130] = PINGROUP(130, SOUTH, sec_mi2s, pll_bist, atest_usb20,
			 atest_char0, NA, NA, NA, NA, NA),
	[131] = PINGROUP(131, SOUTH, ter_mi2s, gcc_gp1, NA, NA, NA, NA, NA, NA,
			 NA),
	[132] = PINGROUP(132, SOUTH, ter_mi2s, NA, qdss_gpio11, NA, NA, NA, NA,
			 NA, NA),
	[133] = PINGROUP(133, SOUTH, ter_mi2s, qdss_gpio8, atest_char1, NA, NA,
			 NA, NA, NA, NA),
	[134] = PINGROUP(134, SOUTH, ter_mi2s, qdss_gpio9, atest_char2, NA, NA,
			 NA, NA, NA, NA),
	[135] = PINGROUP(135, SOUTH, ter_mi2s, atest_char3, NA, NA, NA, NA, NA,
			 NA, NA),
	[136] = PINGROUP(136, SOUTH, qua_mi2s, gcc_gp1, NA, NA, NA, NA, NA, NA,
			 NA),
	[137] = PINGROUP(137, SOUTH, qua_mi2s, gcc_gp2, NA, NA, NA, NA, NA, NA,
			 NA),
	[138] = PINGROUP(138, SOUTH, qua_mi2s, gcc_gp3, NA, NA, NA, NA, NA, NA,
			 NA),
	[139] = PINGROUP(139, SOUTH, qua_mi2s, NA, NA, NA, NA, NA, NA, NA, NA),
	[140] = PINGROUP(140, SOUTH, qua_mi2s, NA, NA, NA, NA, NA, NA, NA, NA),
	[141] = PINGROUP(141, SOUTH, qua_mi2s, NA, NA, NA, NA, NA, NA, NA, NA),
	[142] = PINGROUP(142, SOUTH, qua_mi2s, NA, NA, NA, NA, NA, NA, NA, NA),
	[143] = PINGROUP(143, SOUTH, pri_mi2s, NA, NA, NA, NA, NA, NA, NA, NA),
	[144] = PINGROUP(144, SOUTH, pri_mi2s, qup3, phase_flag29, NA,
			 ddr_pxi0, NA, NA, NA, NA),
	[145] = PINGROUP(145, SOUTH, pri_mi2s_ws, qup3, phase_flag28, ddr_bist,
			 NA, vsense_trigger, atest_usb1, ddr_pxi0, NA),
	[146] = PINGROUP(146, SOUTH, pri_mi2s, qup3, ddr_bist, atest_usb11,
			 ddr_pxi2, NA, NA, NA, NA),
	[147] = PINGROUP(147, SOUTH, pri_mi2s, qup3, dbg_out, atest_usb10,
			 ddr_pxi2, NA, NA, NA, NA),
	[148] = PINGROUP(148, SOUTH, spkr_i2s, audio_ref, NA, NA, NA, NA, NA,
			 NA, NA),
	[149] = PINGROUP(149, SOUTH, lpass_slimbus, spkr_i2s, NA, NA, NA, NA,
			 NA, NA, NA),
	[150] = PINGROUP(150, SOUTH, lpass_slimbus, spkr_i2s, tsense_pwm1,
			 tsense_pwm2, NA, NA, NA, NA, NA),
	[151] = PINGROUP(151, SOUTH, lpass_slimbus, spkr_i2s, NA, NA, NA, NA,
			 NA, NA, NA),
	[152] = PINGROUP(152, SOUTH, lpass_slimbus, spkr_i2s, NA, NA, NA, NA,
			 NA, NA, NA),
	[153] = PINGROUP(153, SOUTH, btfm_slimbus, NA, NA, NA, NA, NA, NA, NA,
			 NA),
	[154] = PINGROUP(154, SOUTH, btfm_slimbus, NA, NA, NA, NA, NA, NA, NA,
			 NA),
	[155] = PINGROUP(155, WEST, hs1_mi2s, NA, NA, NA, NA, NA, NA, NA, NA),
	[156] = PINGROUP(156, WEST, hs1_mi2s, NA, NA, NA, NA, NA, NA, NA, NA),
	[157] = PINGROUP(157, WEST, hs1_mi2s, NA, NA, NA, NA, NA, NA, NA, NA),
	[158] = PINGROUP(158, WEST, hs1_mi2s, NA, NA, NA, NA, NA, NA, NA, NA),
	[159] = PINGROUP(159, WEST, hs1_mi2s, cri_trng0, NA, NA, NA, NA, NA,
			 NA, NA),
	[160] = PINGROUP(160, WEST, hs2_mi2s, cri_trng1, NA, NA, NA, NA, NA,
			 NA, NA),
	[161] = PINGROUP(161, WEST, hs2_mi2s, cri_trng, NA, NA, NA, NA, NA, NA,
			 NA),
	[162] = PINGROUP(162, WEST, hs2_mi2s, sp_cmu, NA, NA, NA, NA, NA, NA,
			 NA),
	[163] = PINGROUP(163, WEST, hs2_mi2s, prng_rosc, NA, NA, NA, NA, NA,
			 NA, NA),
	[164] = PINGROUP(164, WEST, hs2_mi2s, NA, NA, NA, NA, NA, NA, NA, NA),
	[165] = PINGROUP(165, WEST, hs3_mi2s, NA, NA, NA, NA, NA, NA, NA, NA),
	[166] = PINGROUP(166, WEST, hs3_mi2s, NA, NA, NA, NA, NA, NA, NA, NA),
	[167] = PINGROUP(167, WEST, hs3_mi2s, NA, NA, NA, NA, NA, NA, NA, NA),
	[168] = PINGROUP(168, WEST, hs3_mi2s, NA, NA, NA, NA, NA, NA, NA, NA),
	[169] = PINGROUP(169, NORTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[170] = PINGROUP(170, NORTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[171] = PINGROUP(171, NORTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[172] = PINGROUP(172, NORTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[173] = PINGROUP(173, NORTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[174] = PINGROUP(174, NORTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[175] = SDC_QDSD_PINGROUP(sdc2_clk, 0x9b2000, 14, 6),
	[176] = SDC_QDSD_PINGROUP(sdc2_cmd, 0x9b2000, 11, 3),
	[177] = SDC_QDSD_PINGROUP(sdc2_data, 0x9b2000, 9, 0),
	[178] = UFS_RESET(ufs_reset, 0xdb6000),
};

static struct msm_dir_conn sm8150_dir_conn[] = {
	{3, 511},
	{5, 512},
	{8, 513},
	{9, 514},
	{10, 615},
	{12, 619},
	{24, 517},
	{26, 518},
	{27, 521},
	{28, 522},
	{30, 519},
	{36, 523},
	{37, 524},
	{38, 510},
	{39, 633}, /* GPIO 39 mapped to 640 SPI as well */
	{41, 527},
	{42, 528},
	{46, 530},
	{47, 529},
	{48, 531},
	{49, 533},
	{50, 532},
	{51, 631}, /* GPIO 51 mapped to SPI 638 as well */
	{53, 534},
	{54, 535},
	{55, 536},
	{56, 537},
	{58, 538},
	{60, 540},
	{61, 541},
	{68, 542},
	{70, 543},
	{76, 551},
	{77, 546},
	{81, 544},
	{83, 545},
	{86, 547},
	{87, 564},
	{88, 632}, /* GPIO 88 mapped to SPI 639 as well */
	{90, 549},
	{91, 550},
	{93, 555},
	{95, 552},
	{96, 553},
	{97, 554},
	{101, 520},
	{103, 557},
	{104, 558},
	{108, 559},
	{112, 560},
	{113, 561},
	{114, 562},
	{117, 565},
	{118, 616},
	{119, 567},
	{120, 568},
	{121, 569},
	{122, 570},
	{123, 571},
	{124, 572},
	{125, 573},
	{129, 609},
	{132, 620},
	{133, 563},
	{134, 516},
	{136, 612},
	{142, 618},
	{144, 630}, /* GPIO 144 mapped to SPI 637 as well */
	{147, 617},
	{150, 622},
	{152, 623},
	{153, 624},
	{-1, 216},
	{-1, 215},
	{-1, 214},
	{-1, 213},
	{-1, 212},
	{-1, 211},
	{-1, 210},
	{-1, 209},
};

static const struct msm_pinctrl_soc_data sm8150_pinctrl = {
	.pins = sm8150_pins,
	.npins = ARRAY_SIZE(sm8150_pins),
	.functions = sm8150_functions,
	.nfunctions = ARRAY_SIZE(sm8150_functions),
	.groups = sm8150_groups,
	.ngroups = ARRAY_SIZE(sm8150_groups),
	.ngpios = 175,
	.dir_conn = sm8150_dir_conn,
	.n_dir_conns = ARRAY_SIZE(sm8150_dir_conn),
	.dir_conn_irq_base = 216,
};

static int sm8150_pinctrl_probe(struct platform_device *pdev)
{
	return msm_pinctrl_probe(pdev, &sm8150_pinctrl);
}

static const struct of_device_id sm8150_pinctrl_of_match[] = {
	{ .compatible = "qcom,sm8150-pinctrl", },
	{ },
};

static struct platform_driver sm8150_pinctrl_driver = {
	.driver = {
		.name = "sm8150-pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = sm8150_pinctrl_of_match,
	},
	.probe = sm8150_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init sm8150_pinctrl_init(void)
{
	return platform_driver_register(&sm8150_pinctrl_driver);
}
arch_initcall(sm8150_pinctrl_init);

static void __exit sm8150_pinctrl_exit(void)
{
	platform_driver_unregister(&sm8150_pinctrl_driver);
}
module_exit(sm8150_pinctrl_exit);

MODULE_DESCRIPTION("QTI sm8150 pinctrl driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, sm8150_pinctrl_of_match);
