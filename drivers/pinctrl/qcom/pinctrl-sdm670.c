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

#define NORTH	0x00500000
#define SOUTH	0x00900000
#define WEST	0x00100000

#define DUMMY	0x0
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
		.dir_conn_reg = (base == NORTH) ? base + 0xa3000 : \
			((base == SOUTH) ? base + 0xa6000 : base + 0xa4000), \
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
		.dir_conn_en_bit = 8,	        \
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
static const struct pinctrl_pin_desc sdm670_pins[] = {
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
	PINCTRL_PIN(65, "GPIO_65"),
	PINCTRL_PIN(66, "GPIO_66"),
	PINCTRL_PIN(67, "GPIO_67"),
	PINCTRL_PIN(68, "GPIO_68"),
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
	PINCTRL_PIN(150, "SDC1_RCLK"),
	PINCTRL_PIN(151, "SDC1_CLK"),
	PINCTRL_PIN(152, "SDC1_CMD"),
	PINCTRL_PIN(153, "SDC1_DATA"),
	PINCTRL_PIN(154, "SDC2_CLK"),
	PINCTRL_PIN(155, "SDC2_CMD"),
	PINCTRL_PIN(156, "SDC2_DATA"),
	PINCTRL_PIN(157, "UFS_RESET"),
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

static const unsigned int sdc1_rclk_pins[] = { 150 };
static const unsigned int sdc1_clk_pins[] = { 151 };
static const unsigned int sdc1_cmd_pins[] = { 152 };
static const unsigned int sdc1_data_pins[] = { 153 };
static const unsigned int sdc2_clk_pins[] = { 154 };
static const unsigned int sdc2_cmd_pins[] = { 155 };
static const unsigned int sdc2_data_pins[] = { 156 };
static const unsigned int ufs_reset_pins[] = { 157 };

enum sdm670_functions {
	msm_mux_qup0,
	msm_mux_gpio,
	msm_mux_qup9,
	msm_mux_qdss_cti,
	msm_mux_ddr_pxi0,
	msm_mux_ddr_bist,
	msm_mux_atest_tsens2,
	msm_mux_vsense_trigger,
	msm_mux_atest_usb1,
	msm_mux_qup_l4,
	msm_mux_GP_PDM1,
	msm_mux_qup_l5,
	msm_mux_mdp_vsync,
	msm_mux_qup_l6,
	msm_mux_wlan2_adc1,
	msm_mux_atest_usb11,
	msm_mux_ddr_pxi2,
	msm_mux_edp_lcd,
	msm_mux_dbg_out,
	msm_mux_wlan2_adc0,
	msm_mux_atest_usb10,
	msm_mux_m_voc,
	msm_mux_tsif1_sync,
	msm_mux_ddr_pxi3,
	msm_mux_cam_mclk,
	msm_mux_pll_bypassnl,
	msm_mux_qdss_gpio0,
	msm_mux_pll_reset,
	msm_mux_qdss_gpio1,
	msm_mux_qdss_gpio2,
	msm_mux_qdss_gpio3,
	msm_mux_cci_i2c,
	msm_mux_qup1,
	msm_mux_qdss_gpio4,
	msm_mux_qdss_gpio5,
	msm_mux_qdss_gpio6,
	msm_mux_qdss_gpio7,
	msm_mux_cci_timer0,
	msm_mux_gcc_gp2,
	msm_mux_qdss_gpio8,
	msm_mux_cci_timer1,
	msm_mux_gcc_gp3,
	msm_mux_qdss_gpio,
	msm_mux_cci_timer2,
	msm_mux_qdss_gpio9,
	msm_mux_cci_timer3,
	msm_mux_cci_async,
	msm_mux_qdss_gpio10,
	msm_mux_cci_timer4,
	msm_mux_qdss_gpio11,
	msm_mux_qdss_gpio12,
	msm_mux_JITTER_BIST,
	msm_mux_qup2,
	msm_mux_qdss_gpio13,
	msm_mux_PLL_BIST,
	msm_mux_qdss_gpio14,
	msm_mux_AGERA_PLL,
	msm_mux_phase_flag1,
	msm_mux_qdss_gpio15,
	msm_mux_atest_tsens,
	msm_mux_phase_flag2,
	msm_mux_qup11,
	msm_mux_qup14,
	msm_mux_pci_e0,
	msm_mux_QUP_L4,
	msm_mux_QUP_L5,
	msm_mux_QUP_L6,
	msm_mux_usb_phy,
	msm_mux_lpass_slimbus,
	msm_mux_sd_write,
	msm_mux_tsif1_error,
	msm_mux_qup3,
	msm_mux_qup6,
	msm_mux_qup12,
	msm_mux_phase_flag16,
	msm_mux_qup10,
	msm_mux_phase_flag11,
	msm_mux_GP_PDM0,
	msm_mux_phase_flag12,
	msm_mux_wlan1_adc1,
	msm_mux_atest_usb13,
	msm_mux_ddr_pxi1,
	msm_mux_phase_flag13,
	msm_mux_wlan1_adc0,
	msm_mux_atest_usb12,
	msm_mux_phase_flag17,
	msm_mux_qua_mi2s,
	msm_mux_gcc_gp1,
	msm_mux_phase_flag18,
	msm_mux_pri_mi2s,
	msm_mux_qup8,
	msm_mux_wsa_clk,
	msm_mux_pri_mi2s_ws,
	msm_mux_wsa_data,
	msm_mux_atest_usb2,
	msm_mux_atest_usb23,
	msm_mux_ter_mi2s,
	msm_mux_phase_flag8,
	msm_mux_atest_usb22,
	msm_mux_phase_flag9,
	msm_mux_atest_usb21,
	msm_mux_phase_flag4,
	msm_mux_atest_usb20,
	msm_mux_sec_mi2s,
	msm_mux_GP_PDM2,
	msm_mux_qup15,
	msm_mux_qup5,
	msm_mux_copy_gp,
	msm_mux_tsif1_clk,
	msm_mux_qup4,
	msm_mux_tgu_ch3,
	msm_mux_phase_flag10,
	msm_mux_tsif1_en,
	msm_mux_mdp_vsync0,
	msm_mux_mdp_vsync1,
	msm_mux_mdp_vsync2,
	msm_mux_mdp_vsync3,
	msm_mux_tgu_ch0,
	msm_mux_phase_flag0,
	msm_mux_tsif1_data,
	msm_mux_sdc4_cmd,
	msm_mux_tgu_ch1,
	msm_mux_tsif2_error,
	msm_mux_sdc43,
	msm_mux_vfr_1,
	msm_mux_tgu_ch2,
	msm_mux_tsif2_clk,
	msm_mux_sdc4_clk,
	msm_mux_qup7,
	msm_mux_tsif2_en,
	msm_mux_sdc42,
	msm_mux_tsif2_data,
	msm_mux_sdc41,
	msm_mux_tsif2_sync,
	msm_mux_sdc40,
	msm_mux_phase_flag3,
	msm_mux_ldo_en,
	msm_mux_ldo_update,
	msm_mux_phase_flag14,
	msm_mux_prng_rosc,
	msm_mux_phase_flag15,
	msm_mux_phase_flag5,
	msm_mux_pci_e1,
	msm_mux_COPY_PHASE,
	msm_mux_uim2_data,
	msm_mux_qup13,
	msm_mux_uim2_clk,
	msm_mux_uim2_reset,
	msm_mux_uim2_present,
	msm_mux_uim1_data,
	msm_mux_uim1_clk,
	msm_mux_uim1_reset,
	msm_mux_uim1_present,
	msm_mux_uim_batt,
	msm_mux_edp_hot,
	msm_mux_NAV_PPS,
	msm_mux_GPS_TX,
	msm_mux_atest_char,
	msm_mux_adsp_ext,
	msm_mux_atest_char3,
	msm_mux_atest_char2,
	msm_mux_atest_char1,
	msm_mux_atest_char0,
	msm_mux_qlink_request,
	msm_mux_qlink_enable,
	msm_mux_pa_indicator,
	msm_mux_phase_flag26,
	msm_mux_phase_flag27,
	msm_mux_phase_flag28,
	msm_mux_phase_flag6,
	msm_mux_phase_flag29,
	msm_mux_phase_flag30,
	msm_mux_phase_flag31,
	msm_mux_mss_lte,
	msm_mux_NA,
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
	"gpio57", "gpio65", "gpio66", "gpio67", "gpio68", "gpio75", "gpio76",
	"gpio77", "gpio78", "gpio79", "gpio80", "gpio81", "gpio82", "gpio83",
	"gpio84", "gpio85", "gpio86", "gpio87", "gpio88", "gpio89", "gpio90",
	"gpio91", "gpio92", "gpio93", "gpio94", "gpio95", "gpio96", "gpio97",
	"gpio98", "gpio99", "gpio100", "gpio101", "gpio102", "gpio103",
	"gpio105", "gpio106", "gpio107", "gpio108", "gpio109", "gpio110",
	"gpio111", "gpio112", "gpio113", "gpio114", "gpio115", "gpio116",
	"gpio117", "gpio118", "gpio119", "gpio120", "gpio121", "gpio122",
	"gpio123", "gpio124", "gpio125", "gpio126", "gpio127", "gpio128",
	"gpio129", "gpio130", "gpio131", "gpio132", "gpio133", "gpio134",
	"gpio135", "gpio136", "gpio137", "gpio138", "gpio139", "gpio140",
	"gpio141", "gpio142", "gpio143", "gpio144", "gpio145", "gpio146",
	"gpio147", "gpio148", "gpio149",
};
static const char * const qup9_groups[] = {
	"gpio4", "gpio5", "gpio6", "gpio7",
};
static const char * const qdss_cti_groups[] = {
	"gpio4", "gpio5", "gpio51", "gpio52", "gpio90", "gpio91",
};
static const char * const ddr_pxi0_groups[] = {
	"gpio6", "gpio7",
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
static const char * const qup_l4_groups[] = {
	"gpio8", "gpio105", "gpio123",
};
static const char * const GP_PDM1_groups[] = {
	"gpio8", "gpio66",
};
static const char * const qup_l5_groups[] = {
	"gpio9", "gpio106", "gpio124",
};
static const char * const mdp_vsync_groups[] = {
	"gpio10", "gpio11", "gpio12", "gpio97", "gpio98",
};
static const char * const qup_l6_groups[] = {
	"gpio10", "gpio107", "gpio125",
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
static const char * const m_voc_groups[] = {
	"gpio12",
};
static const char * const tsif1_sync_groups[] = {
	"gpio12",
};
static const char * const ddr_pxi3_groups[] = {
	"gpio12", "gpio13",
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
static const char * const pll_reset_groups[] = {
	"gpio14",
};
static const char * const qdss_gpio1_groups[] = {
	"gpio14", "gpio118",
};
static const char * const qdss_gpio2_groups[] = {
	"gpio15", "gpio119",
};
static const char * const qdss_gpio3_groups[] = {
	"gpio16", "gpio120",
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
static const char * const qdss_gpio5_groups[] = {
	"gpio18", "gpio122",
};
static const char * const qdss_gpio6_groups[] = {
	"gpio19", "gpio41",
};
static const char * const qdss_gpio7_groups[] = {
	"gpio20", "gpio42",
};
static const char * const cci_timer0_groups[] = {
	"gpio21",
};
static const char * const gcc_gp2_groups[] = {
	"gpio21",
};
static const char * const qdss_gpio8_groups[] = {
	"gpio21", "gpio75",
};
static const char * const cci_timer1_groups[] = {
	"gpio22",
};
static const char * const gcc_gp3_groups[] = {
	"gpio22",
};
static const char * const qdss_gpio_groups[] = {
	"gpio22", "gpio30", "gpio123", "gpio124",
};
static const char * const cci_timer2_groups[] = {
	"gpio23",
};
static const char * const qdss_gpio9_groups[] = {
	"gpio23", "gpio76",
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
static const char * const cci_timer4_groups[] = {
	"gpio25",
};
static const char * const qdss_gpio11_groups[] = {
	"gpio25", "gpio79",
};
static const char * const qdss_gpio12_groups[] = {
	"gpio26", "gpio80",
};
static const char * const JITTER_BIST_groups[] = {
	"gpio26", "gpio35",
};
static const char * const qup2_groups[] = {
	"gpio27", "gpio28", "gpio29", "gpio30",
};
static const char * const qdss_gpio13_groups[] = {
	"gpio27", "gpio93",
};
static const char * const PLL_BIST_groups[] = {
	"gpio27", "gpio36",
};
static const char * const qdss_gpio14_groups[] = {
	"gpio28", "gpio43",
};
static const char * const AGERA_PLL_groups[] = {
	"gpio28", "gpio37",
};
static const char * const phase_flag1_groups[] = {
	"gpio29",
};
static const char * const qdss_gpio15_groups[] = {
	"gpio29", "gpio44",
};
static const char * const atest_tsens_groups[] = {
	"gpio29",
};
static const char * const phase_flag2_groups[] = {
	"gpio30",
};
static const char * const qup11_groups[] = {
	"gpio31", "gpio32", "gpio33", "gpio34",
};
static const char * const qup14_groups[] = {
	"gpio31", "gpio32", "gpio33", "gpio34",
};
static const char * const pci_e0_groups[] = {
	"gpio35", "gpio36",
};
static const char * const QUP_L4_groups[] = {
	"gpio35", "gpio75",
};
static const char * const QUP_L5_groups[] = {
	"gpio36", "gpio76",
};
static const char * const QUP_L6_groups[] = {
	"gpio37", "gpio77",
};
static const char * const usb_phy_groups[] = {
	"gpio38",
};
static const char * const lpass_slimbus_groups[] = {
	"gpio39",
};
static const char * const sd_write_groups[] = {
	"gpio40",
};
static const char * const tsif1_error_groups[] = {
	"gpio40",
};
static const char * const qup3_groups[] = {
	"gpio41", "gpio42", "gpio43", "gpio44",
};
static const char * const qup6_groups[] = {
	"gpio45", "gpio46", "gpio47", "gpio48",
};
static const char * const qup12_groups[] = {
	"gpio49", "gpio50", "gpio51", "gpio52",
};
static const char * const phase_flag16_groups[] = {
	"gpio52",
};
static const char * const qup10_groups[] = {
	"gpio53", "gpio54", "gpio55", "gpio56",
};
static const char * const phase_flag11_groups[] = {
	"gpio53",
};
static const char * const GP_PDM0_groups[] = {
	"gpio54", "gpio95",
};
static const char * const phase_flag12_groups[] = {
	"gpio54",
};
static const char * const wlan1_adc1_groups[] = {
	"gpio54",
};
static const char * const atest_usb13_groups[] = {
	"gpio54",
};
static const char * const ddr_pxi1_groups[] = {
	"gpio54", "gpio55",
};
static const char * const phase_flag13_groups[] = {
	"gpio55",
};
static const char * const wlan1_adc0_groups[] = {
	"gpio55",
};
static const char * const atest_usb12_groups[] = {
	"gpio55",
};
static const char * const phase_flag17_groups[] = {
	"gpio56",
};
static const char * const qua_mi2s_groups[] = {
	"gpio57",
};
static const char * const gcc_gp1_groups[] = {
	"gpio57", "gpio78",
};
static const char * const phase_flag18_groups[] = {
	"gpio57",
};
static const char * const pri_mi2s_groups[] = {
	"gpio65", "gpio67", "gpio68",
};
static const char * const qup8_groups[] = {
	"gpio65", "gpio66", "gpio67", "gpio68",
};
static const char * const wsa_clk_groups[] = {
	"gpio65",
};
static const char * const pri_mi2s_ws_groups[] = {
	"gpio66",
};
static const char * const wsa_data_groups[] = {
	"gpio66",
};
static const char * const atest_usb2_groups[] = {
	"gpio67",
};
static const char * const atest_usb23_groups[] = {
	"gpio68",
};
static const char * const ter_mi2s_groups[] = {
	"gpio75", "gpio76", "gpio77", "gpio78",
};
static const char * const phase_flag8_groups[] = {
	"gpio75",
};
static const char * const atest_usb22_groups[] = {
	"gpio75",
};
static const char * const phase_flag9_groups[] = {
	"gpio76",
};
static const char * const atest_usb21_groups[] = {
	"gpio76",
};
static const char * const phase_flag4_groups[] = {
	"gpio77",
};
static const char * const atest_usb20_groups[] = {
	"gpio77",
};
static const char * const sec_mi2s_groups[] = {
	"gpio79", "gpio80", "gpio81", "gpio82", "gpio83",
};
static const char * const GP_PDM2_groups[] = {
	"gpio79",
};
static const char * const qup15_groups[] = {
	"gpio81", "gpio82", "gpio83", "gpio84",
};
static const char * const qup5_groups[] = {
	"gpio85", "gpio86", "gpio87", "gpio88",
};
static const char * const copy_gp_groups[] = {
	"gpio86",
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
static const char * const tsif1_data_groups[] = {
	"gpio91",
};
static const char * const sdc4_cmd_groups[] = {
	"gpio91",
};
static const char * const tgu_ch1_groups[] = {
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
static const char * const tsif2_clk_groups[] = {
	"gpio93",
};
static const char * const sdc4_clk_groups[] = {
	"gpio93",
};
static const char * const qup7_groups[] = {
	"gpio93", "gpio94", "gpio95", "gpio96",
};
static const char * const tsif2_en_groups[] = {
	"gpio94",
};
static const char * const sdc42_groups[] = {
	"gpio94",
};
static const char * const tsif2_data_groups[] = {
	"gpio95",
};
static const char * const sdc41_groups[] = {
	"gpio95",
};
static const char * const tsif2_sync_groups[] = {
	"gpio96",
};
static const char * const sdc40_groups[] = {
	"gpio96",
};
static const char * const phase_flag3_groups[] = {
	"gpio96",
};
static const char * const ldo_en_groups[] = {
	"gpio97",
};
static const char * const ldo_update_groups[] = {
	"gpio98",
};
static const char * const phase_flag14_groups[] = {
	"gpio99",
};
static const char * const prng_rosc_groups[] = {
	"gpio99", "gpio102",
};
static const char * const phase_flag15_groups[] = {
	"gpio100",
};
static const char * const phase_flag5_groups[] = {
	"gpio101",
};
static const char * const pci_e1_groups[] = {
	"gpio102", "gpio103",
};
static const char * const COPY_PHASE_groups[] = {
	"gpio103",
};
static const char * const uim2_data_groups[] = {
	"gpio105",
};
static const char * const qup13_groups[] = {
	"gpio105", "gpio106", "gpio107", "gpio108",
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
static const char * const edp_hot_groups[] = {
	"gpio113",
};
static const char * const NAV_PPS_groups[] = {
	"gpio114", "gpio114", "gpio115", "gpio115", "gpio128", "gpio128",
	"gpio129", "gpio129", "gpio143", "gpio143",
};
static const char * const GPS_TX_groups[] = {
	"gpio114", "gpio115", "gpio128", "gpio129", "gpio143", "gpio145",
};
static const char * const atest_char_groups[] = {
	"gpio117",
};
static const char * const adsp_ext_groups[] = {
	"gpio118",
};
static const char * const atest_char3_groups[] = {
	"gpio118",
};
static const char * const atest_char2_groups[] = {
	"gpio119",
};
static const char * const atest_char1_groups[] = {
	"gpio120",
};
static const char * const atest_char0_groups[] = {
	"gpio121",
};
static const char * const qlink_request_groups[] = {
	"gpio130",
};
static const char * const qlink_enable_groups[] = {
	"gpio131",
};
static const char * const pa_indicator_groups[] = {
	"gpio135",
};
static const char * const phase_flag26_groups[] = {
	"gpio137",
};
static const char * const phase_flag27_groups[] = {
	"gpio138",
};
static const char * const phase_flag28_groups[] = {
	"gpio139",
};
static const char * const phase_flag6_groups[] = {
	"gpio140",
};
static const char * const phase_flag29_groups[] = {
	"gpio141",
};
static const char * const phase_flag30_groups[] = {
	"gpio142",
};
static const char * const phase_flag31_groups[] = {
	"gpio143",
};
static const char * const mss_lte_groups[] = {
	"gpio144", "gpio145",
};

static const struct msm_function sdm670_functions[] = {
	FUNCTION(qup0),
	FUNCTION(gpio),
	FUNCTION(qup9),
	FUNCTION(qdss_cti),
	FUNCTION(ddr_pxi0),
	FUNCTION(ddr_bist),
	FUNCTION(atest_tsens2),
	FUNCTION(vsense_trigger),
	FUNCTION(atest_usb1),
	FUNCTION(qup_l4),
	FUNCTION(GP_PDM1),
	FUNCTION(qup_l5),
	FUNCTION(mdp_vsync),
	FUNCTION(qup_l6),
	FUNCTION(wlan2_adc1),
	FUNCTION(atest_usb11),
	FUNCTION(ddr_pxi2),
	FUNCTION(edp_lcd),
	FUNCTION(dbg_out),
	FUNCTION(wlan2_adc0),
	FUNCTION(atest_usb10),
	FUNCTION(m_voc),
	FUNCTION(tsif1_sync),
	FUNCTION(ddr_pxi3),
	FUNCTION(cam_mclk),
	FUNCTION(pll_bypassnl),
	FUNCTION(qdss_gpio0),
	FUNCTION(pll_reset),
	FUNCTION(qdss_gpio1),
	FUNCTION(qdss_gpio2),
	FUNCTION(qdss_gpio3),
	FUNCTION(cci_i2c),
	FUNCTION(qup1),
	FUNCTION(qdss_gpio4),
	FUNCTION(qdss_gpio5),
	FUNCTION(qdss_gpio6),
	FUNCTION(qdss_gpio7),
	FUNCTION(cci_timer0),
	FUNCTION(gcc_gp2),
	FUNCTION(qdss_gpio8),
	FUNCTION(cci_timer1),
	FUNCTION(gcc_gp3),
	FUNCTION(qdss_gpio),
	FUNCTION(cci_timer2),
	FUNCTION(qdss_gpio9),
	FUNCTION(cci_timer3),
	FUNCTION(cci_async),
	FUNCTION(qdss_gpio10),
	FUNCTION(cci_timer4),
	FUNCTION(qdss_gpio11),
	FUNCTION(qdss_gpio12),
	FUNCTION(JITTER_BIST),
	FUNCTION(qup2),
	FUNCTION(qdss_gpio13),
	FUNCTION(PLL_BIST),
	FUNCTION(qdss_gpio14),
	FUNCTION(AGERA_PLL),
	FUNCTION(phase_flag1),
	FUNCTION(qdss_gpio15),
	FUNCTION(atest_tsens),
	FUNCTION(phase_flag2),
	FUNCTION(qup11),
	FUNCTION(qup14),
	FUNCTION(pci_e0),
	FUNCTION(QUP_L4),
	FUNCTION(QUP_L5),
	FUNCTION(QUP_L6),
	FUNCTION(usb_phy),
	FUNCTION(lpass_slimbus),
	FUNCTION(sd_write),
	FUNCTION(tsif1_error),
	FUNCTION(qup3),
	FUNCTION(qup6),
	FUNCTION(qup12),
	FUNCTION(phase_flag16),
	FUNCTION(qup10),
	FUNCTION(phase_flag11),
	FUNCTION(GP_PDM0),
	FUNCTION(phase_flag12),
	FUNCTION(wlan1_adc1),
	FUNCTION(atest_usb13),
	FUNCTION(ddr_pxi1),
	FUNCTION(phase_flag13),
	FUNCTION(wlan1_adc0),
	FUNCTION(atest_usb12),
	FUNCTION(phase_flag17),
	FUNCTION(qua_mi2s),
	FUNCTION(gcc_gp1),
	FUNCTION(phase_flag18),
	FUNCTION(pri_mi2s),
	FUNCTION(qup8),
	FUNCTION(wsa_clk),
	FUNCTION(pri_mi2s_ws),
	FUNCTION(wsa_data),
	FUNCTION(atest_usb2),
	FUNCTION(atest_usb23),
	FUNCTION(ter_mi2s),
	FUNCTION(phase_flag8),
	FUNCTION(atest_usb22),
	FUNCTION(phase_flag9),
	FUNCTION(atest_usb21),
	FUNCTION(phase_flag4),
	FUNCTION(atest_usb20),
	FUNCTION(sec_mi2s),
	FUNCTION(GP_PDM2),
	FUNCTION(qup15),
	FUNCTION(qup5),
	FUNCTION(copy_gp),
	FUNCTION(tsif1_clk),
	FUNCTION(qup4),
	FUNCTION(tgu_ch3),
	FUNCTION(phase_flag10),
	FUNCTION(tsif1_en),
	FUNCTION(mdp_vsync0),
	FUNCTION(mdp_vsync1),
	FUNCTION(mdp_vsync2),
	FUNCTION(mdp_vsync3),
	FUNCTION(tgu_ch0),
	FUNCTION(phase_flag0),
	FUNCTION(tsif1_data),
	FUNCTION(sdc4_cmd),
	FUNCTION(tgu_ch1),
	FUNCTION(tsif2_error),
	FUNCTION(sdc43),
	FUNCTION(vfr_1),
	FUNCTION(tgu_ch2),
	FUNCTION(tsif2_clk),
	FUNCTION(sdc4_clk),
	FUNCTION(qup7),
	FUNCTION(tsif2_en),
	FUNCTION(sdc42),
	FUNCTION(tsif2_data),
	FUNCTION(sdc41),
	FUNCTION(tsif2_sync),
	FUNCTION(sdc40),
	FUNCTION(phase_flag3),
	FUNCTION(ldo_en),
	FUNCTION(ldo_update),
	FUNCTION(phase_flag14),
	FUNCTION(prng_rosc),
	FUNCTION(phase_flag15),
	FUNCTION(phase_flag5),
	FUNCTION(pci_e1),
	FUNCTION(COPY_PHASE),
	FUNCTION(uim2_data),
	FUNCTION(qup13),
	FUNCTION(uim2_clk),
	FUNCTION(uim2_reset),
	FUNCTION(uim2_present),
	FUNCTION(uim1_data),
	FUNCTION(uim1_clk),
	FUNCTION(uim1_reset),
	FUNCTION(uim1_present),
	FUNCTION(uim_batt),
	FUNCTION(edp_hot),
	FUNCTION(NAV_PPS),
	FUNCTION(GPS_TX),
	FUNCTION(atest_char),
	FUNCTION(adsp_ext),
	FUNCTION(atest_char3),
	FUNCTION(atest_char2),
	FUNCTION(atest_char1),
	FUNCTION(atest_char0),
	FUNCTION(qlink_request),
	FUNCTION(qlink_enable),
	FUNCTION(pa_indicator),
	FUNCTION(phase_flag26),
	FUNCTION(phase_flag27),
	FUNCTION(phase_flag28),
	FUNCTION(phase_flag6),
	FUNCTION(phase_flag29),
	FUNCTION(phase_flag30),
	FUNCTION(phase_flag31),
	FUNCTION(mss_lte),
};

/* Every pin is maintained as a single group, and missing or non-existing pin
 * would be maintained as dummy group to synchronize pin group index with
 * pin descriptor registered with pinctrl core.
 * Clients would not be able to request these dummy pin groups.
 */
static const struct msm_pingroup sdm670_groups[] = {
	[0] = PINGROUP(0, SOUTH, qup0, NA, NA, NA, NA, NA, NA, NA, NA),
	[1] = PINGROUP(1, SOUTH, qup0, NA, NA, NA, NA, NA, NA, NA, NA),
	[2] = PINGROUP(2, SOUTH, qup0, NA, NA, NA, NA, NA, NA, NA, NA),
	[3] = PINGROUP(3, SOUTH, qup0, NA, NA, NA, NA, NA, NA, NA, NA),
	[4] = PINGROUP(4, NORTH, qup9, qdss_cti, NA, NA, NA, NA, NA, NA, NA),
	[5] = PINGROUP(5, NORTH, qup9, qdss_cti, NA, NA, NA, NA, NA, NA, NA),
	[6] = PINGROUP(6, NORTH, qup9, NA, ddr_pxi0, NA, NA, NA, NA, NA, NA),
	[7] = PINGROUP(7, NORTH, qup9, ddr_bist, NA, atest_tsens2,
		       vsense_trigger, atest_usb1, ddr_pxi0, NA, NA),
	[8] = PINGROUP(8, WEST, qup_l4, GP_PDM1, ddr_bist, NA, NA, NA, NA, NA,
		       NA),
	[9] = PINGROUP(9, WEST, qup_l5, ddr_bist, NA, NA, NA, NA, NA, NA, NA),
	[10] = PINGROUP(10, NORTH, mdp_vsync, qup_l6, ddr_bist, wlan2_adc1,
			atest_usb11, ddr_pxi2, NA, NA, NA),
	[11] = PINGROUP(11, NORTH, mdp_vsync, edp_lcd, dbg_out, wlan2_adc0,
			atest_usb10, ddr_pxi2, NA, NA, NA),
	[12] = PINGROUP(12, SOUTH, mdp_vsync, m_voc, tsif1_sync, ddr_pxi3, NA,
			NA, NA, NA, NA),
	[13] = PINGROUP(13, WEST, cam_mclk, pll_bypassnl, qdss_gpio0, ddr_pxi3,
			NA, NA, NA, NA, NA),
	[14] = PINGROUP(14, WEST, cam_mclk, pll_reset, qdss_gpio1, NA, NA, NA,
			NA, NA, NA),
	[15] = PINGROUP(15, WEST, cam_mclk, qdss_gpio2, NA, NA, NA, NA, NA, NA,
			NA),
	[16] = PINGROUP(16, WEST, cam_mclk, qdss_gpio3, NA, NA, NA, NA, NA, NA,
			NA),
	[17] = PINGROUP(17, WEST, cci_i2c, qup1, qdss_gpio4, NA, NA, NA, NA,
			NA, NA),
	[18] = PINGROUP(18, WEST, cci_i2c, qup1, NA, qdss_gpio5, NA, NA, NA,
			NA, NA),
	[19] = PINGROUP(19, WEST, cci_i2c, qup1, NA, qdss_gpio6, NA, NA, NA,
			NA, NA),
	[20] = PINGROUP(20, WEST, cci_i2c, qup1, NA, qdss_gpio7, NA, NA, NA,
			NA, NA),
	[21] = PINGROUP(21, WEST, cci_timer0, gcc_gp2, qdss_gpio8, NA, NA, NA,
			NA, NA, NA),
	[22] = PINGROUP(22, WEST, cci_timer1, gcc_gp3, qdss_gpio, NA, NA, NA,
			NA, NA, NA),
	[23] = PINGROUP(23, WEST, cci_timer2, qdss_gpio9, NA, NA, NA, NA, NA,
			NA, NA),
	[24] = PINGROUP(24, WEST, cci_timer3, cci_async, qdss_gpio10, NA, NA,
			NA, NA, NA, NA),
	[25] = PINGROUP(25, WEST, cci_timer4, cci_async, qdss_gpio11, NA, NA,
			NA, NA, NA, NA),
	[26] = PINGROUP(26, WEST, cci_async, qdss_gpio12, JITTER_BIST, NA, NA,
			NA, NA, NA, NA),
	[27] = PINGROUP(27, WEST, qup2, qdss_gpio13, PLL_BIST, NA, NA, NA, NA,
			NA, NA),
	[28] = PINGROUP(28, WEST, qup2, qdss_gpio14, AGERA_PLL, NA, NA, NA, NA,
			NA, NA),
	[29] = PINGROUP(29, WEST, qup2, NA, phase_flag1, qdss_gpio15,
			atest_tsens, NA, NA, NA, NA),
	[30] = PINGROUP(30, WEST, qup2, phase_flag2, qdss_gpio, NA, NA, NA, NA,
			NA, NA),
	[31] = PINGROUP(31, WEST, qup11, qup14, NA, NA, NA, NA, NA, NA, NA),
	[32] = PINGROUP(32, WEST, qup11, qup14, NA, NA, NA, NA, NA, NA, NA),
	[33] = PINGROUP(33, WEST, qup11, qup14, NA, NA, NA, NA, NA, NA, NA),
	[34] = PINGROUP(34, WEST, qup11, qup14, NA, NA, NA, NA, NA, NA, NA),
	[35] = PINGROUP(35, NORTH, pci_e0, QUP_L4, JITTER_BIST, NA, NA, NA, NA,
			NA, NA),
	[36] = PINGROUP(36, NORTH, pci_e0, QUP_L5, PLL_BIST, NA, NA, NA, NA,
			NA, NA),
	[37] = PINGROUP(37, NORTH, QUP_L6, AGERA_PLL, NA, NA, NA, NA, NA, NA,
			NA),
	[38] = PINGROUP(38, NORTH, usb_phy, NA, NA, NA, NA, NA, NA, NA, NA),
	[39] = PINGROUP(39, NORTH, lpass_slimbus, NA, NA, NA, NA, NA, NA, NA,
			NA),
	[40] = PINGROUP(40, NORTH, sd_write, tsif1_error, NA, NA, NA, NA, NA,
			NA, NA),
	[41] = PINGROUP(41, SOUTH, qup3, NA, qdss_gpio6, NA, NA, NA, NA, NA,
			NA),
	[42] = PINGROUP(42, SOUTH, qup3, NA, qdss_gpio7, NA, NA, NA, NA, NA,
			NA),
	[43] = PINGROUP(43, SOUTH, qup3, NA, qdss_gpio14, NA, NA, NA, NA, NA,
			NA),
	[44] = PINGROUP(44, SOUTH, qup3, NA, qdss_gpio15, NA, NA, NA, NA, NA,
			NA),
	[45] = PINGROUP(45, SOUTH, qup6, NA, NA, NA, NA, NA, NA, NA, NA),
	[46] = PINGROUP(46, SOUTH, qup6, NA, NA, NA, NA, NA, NA, NA, NA),
	[47] = PINGROUP(47, SOUTH, qup6, NA, NA, NA, NA, NA, NA, NA, NA),
	[48] = PINGROUP(48, SOUTH, qup6, NA, NA, NA, NA, NA, NA, NA, NA),
	[49] = PINGROUP(49, NORTH, qup12, NA, NA, NA, NA, NA, NA, NA, NA),
	[50] = PINGROUP(50, NORTH, qup12, NA, NA, NA, NA, NA, NA, NA, NA),
	[51] = PINGROUP(51, NORTH, qup12, qdss_cti, NA, NA, NA, NA, NA, NA, NA),
	[52] = PINGROUP(52, NORTH, qup12, phase_flag16, qdss_cti, NA, NA, NA,
			NA, NA, NA),
	[53] = PINGROUP(53, NORTH, qup10, phase_flag11, NA, NA, NA, NA, NA, NA,
			NA),
	[54] = PINGROUP(54, NORTH, qup10, GP_PDM0, phase_flag12, NA,
			wlan1_adc1, atest_usb13, ddr_pxi1, NA, NA),
	[55] = PINGROUP(55, NORTH, qup10, phase_flag13, NA, wlan1_adc0,
			atest_usb12, ddr_pxi1, NA, NA, NA),
	[56] = PINGROUP(56, NORTH, qup10, phase_flag17, NA, NA, NA, NA, NA, NA,
			NA),
	[57] = PINGROUP(57, NORTH, qua_mi2s, gcc_gp1, phase_flag18, NA, NA, NA,
			NA, NA, NA),
	[58] = PINGROUP(58, DUMMY, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[59] = PINGROUP(59, DUMMY, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[60] = PINGROUP(60, DUMMY, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[61] = PINGROUP(61, DUMMY, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[62] = PINGROUP(62, DUMMY, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[63] = PINGROUP(63, DUMMY, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[64] = PINGROUP(64, DUMMY, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[65] = PINGROUP(65, NORTH, pri_mi2s, qup8, wsa_clk, NA, NA, NA, NA, NA,
			NA),
	[66] = PINGROUP(66, NORTH, pri_mi2s_ws, qup8, wsa_data, GP_PDM1, NA,
			NA, NA, NA, NA),
	[67] = PINGROUP(67, NORTH, pri_mi2s, qup8, NA, atest_usb2, NA, NA, NA,
			NA, NA),
	[68] = PINGROUP(68, NORTH, pri_mi2s, qup8, NA, atest_usb23, NA, NA, NA,
			NA, NA),
	[69] = PINGROUP(69, DUMMY, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[70] = PINGROUP(70, DUMMY, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[71] = PINGROUP(71, DUMMY, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[72] = PINGROUP(72, DUMMY, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[73] = PINGROUP(73, DUMMY, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[74] = PINGROUP(74, DUMMY, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[75] = PINGROUP(75, NORTH, ter_mi2s, phase_flag8, qdss_gpio8,
			atest_usb22, QUP_L4, NA, NA, NA, NA),
	[76] = PINGROUP(76, NORTH, ter_mi2s, phase_flag9, qdss_gpio9,
			atest_usb21, QUP_L5, NA, NA, NA, NA),
	[77] = PINGROUP(77, NORTH, ter_mi2s, phase_flag4, qdss_gpio10,
			atest_usb20, QUP_L6, NA, NA, NA, NA),
	[78] = PINGROUP(78, NORTH, ter_mi2s, gcc_gp1, NA, NA, NA, NA, NA, NA,
			NA),
	[79] = PINGROUP(79, NORTH, sec_mi2s, GP_PDM2, NA, qdss_gpio11, NA, NA,
			NA, NA, NA),
	[80] = PINGROUP(80, NORTH, sec_mi2s, NA, qdss_gpio12, NA, NA, NA, NA,
			NA, NA),
	[81] = PINGROUP(81, NORTH, sec_mi2s, qup15, NA, NA, NA, NA, NA, NA, NA),
	[82] = PINGROUP(82, NORTH, sec_mi2s, qup15, NA, NA, NA, NA, NA, NA, NA),
	[83] = PINGROUP(83, NORTH, sec_mi2s, qup15, NA, NA, NA, NA, NA, NA, NA),
	[84] = PINGROUP(84, NORTH, qup15, NA, NA, NA, NA, NA, NA, NA, NA),
	[85] = PINGROUP(85, SOUTH, qup5, NA, NA, NA, NA, NA, NA, NA, NA),
	[86] = PINGROUP(86, SOUTH, qup5, copy_gp, NA, NA, NA, NA, NA, NA, NA),
	[87] = PINGROUP(87, SOUTH, qup5, NA, NA, NA, NA, NA, NA, NA, NA),
	[88] = PINGROUP(88, SOUTH, qup5, NA, NA, NA, NA, NA, NA, NA, NA),
	[89] = PINGROUP(89, SOUTH, tsif1_clk, qup4, tgu_ch3, phase_flag10, NA,
			NA, NA, NA, NA),
	[90] = PINGROUP(90, SOUTH, tsif1_en, mdp_vsync0, qup4, mdp_vsync1,
			mdp_vsync2, mdp_vsync3, tgu_ch0, phase_flag0, qdss_cti),
	[91] = PINGROUP(91, SOUTH, tsif1_data, sdc4_cmd, qup4, tgu_ch1, NA,
			qdss_cti, NA, NA, NA),
	[92] = PINGROUP(92, SOUTH, tsif2_error, sdc43, qup4, vfr_1, tgu_ch2,
			NA, NA, NA, NA),
	[93] = PINGROUP(93, SOUTH, tsif2_clk, sdc4_clk, qup7, NA, qdss_gpio13,
			NA, NA, NA, NA),
	[94] = PINGROUP(94, SOUTH, tsif2_en, sdc42, qup7, NA, NA, NA, NA, NA,
			NA),
	[95] = PINGROUP(95, SOUTH, tsif2_data, sdc41, qup7, GP_PDM0, NA, NA,
			NA, NA, NA),
	[96] = PINGROUP(96, SOUTH, tsif2_sync, sdc40, qup7, phase_flag3, NA,
			NA, NA, NA, NA),
	[97] = PINGROUP(97, WEST, NA, NA, mdp_vsync, ldo_en, NA, NA, NA, NA,
			NA),
	[98] = PINGROUP(98, WEST, NA, mdp_vsync, ldo_update, NA, NA, NA, NA,
			NA, NA),
	[99] = PINGROUP(99, NORTH, phase_flag14, prng_rosc, NA, NA, NA, NA, NA,
			NA, NA),
	[100] = PINGROUP(100, WEST, phase_flag15, NA, NA, NA, NA, NA, NA, NA,
			 NA),
	[101] = PINGROUP(101, WEST, NA, phase_flag5, NA, NA, NA, NA, NA, NA,
			 NA),
	[102] = PINGROUP(102, WEST, pci_e1, prng_rosc, NA, NA, NA, NA, NA, NA,
			 NA),
	[103] = PINGROUP(103, WEST, pci_e1, COPY_PHASE, NA, NA, NA, NA, NA, NA,
			 NA),
	[104] = PINGROUP(104, DUMMY, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[105] = PINGROUP(105, NORTH, uim2_data, qup13, qup_l4, NA, NA, NA, NA,
			 NA, NA),
	[106] = PINGROUP(106, NORTH, uim2_clk, qup13, qup_l5, NA, NA, NA, NA,
			 NA, NA),
	[107] = PINGROUP(107, NORTH, uim2_reset, qup13, qup_l6, NA, NA, NA, NA,
			 NA, NA),
	[108] = PINGROUP(108, NORTH, uim2_present, qup13, NA, NA, NA, NA, NA,
			 NA, NA),
	[109] = PINGROUP(109, NORTH, uim1_data, NA, NA, NA, NA, NA, NA, NA, NA),
	[110] = PINGROUP(110, NORTH, uim1_clk, NA, NA, NA, NA, NA, NA, NA, NA),
	[111] = PINGROUP(111, NORTH, uim1_reset, NA, NA, NA, NA, NA, NA, NA,
			 NA),
	[112] = PINGROUP(112, NORTH, uim1_present, NA, NA, NA, NA, NA, NA, NA,
			 NA),
	[113] = PINGROUP(113, NORTH, uim_batt, edp_hot, NA, NA, NA, NA, NA, NA,
			 NA),
	[114] = PINGROUP(114, WEST, NA, NAV_PPS, NAV_PPS, GPS_TX, NA, NA, NA,
			 NA, NA),
	[115] = PINGROUP(115, WEST, NA, NAV_PPS, NAV_PPS, GPS_TX, NA, NA, NA,
			 NA, NA),
	[116] = PINGROUP(116, SOUTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[117] = PINGROUP(117, NORTH, NA, qdss_gpio0, atest_char, NA, NA, NA,
			 NA, NA, NA),
	[118] = PINGROUP(118, NORTH, adsp_ext, NA, qdss_gpio1, atest_char3, NA,
			 NA, NA, NA, NA),
	[119] = PINGROUP(119, NORTH, NA, qdss_gpio2, atest_char2, NA, NA, NA,
			 NA, NA, NA),
	[120] = PINGROUP(120, NORTH, NA, qdss_gpio3, atest_char1, NA, NA, NA,
			 NA, NA, NA),
	[121] = PINGROUP(121, NORTH, NA, qdss_gpio4, atest_char0, NA, NA, NA,
			 NA, NA, NA),
	[122] = PINGROUP(122, NORTH, NA, qdss_gpio5, NA, NA, NA, NA, NA, NA,
			 NA),
	[123] = PINGROUP(123, NORTH, qup_l4, NA, qdss_gpio, NA, NA, NA, NA, NA,
			 NA),
	[124] = PINGROUP(124, NORTH, qup_l5, NA, qdss_gpio, NA, NA, NA, NA, NA,
			 NA),
	[125] = PINGROUP(125, NORTH, qup_l6, NA, NA, NA, NA, NA, NA, NA, NA),
	[126] = PINGROUP(126, NORTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[127] = PINGROUP(127, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[128] = PINGROUP(128, WEST, NAV_PPS, NAV_PPS, GPS_TX, NA, NA, NA, NA,
			 NA, NA),
	[129] = PINGROUP(129, WEST, NAV_PPS, NAV_PPS, GPS_TX, NA, NA, NA, NA,
			 NA, NA),
	[130] = PINGROUP(130, WEST, qlink_request, NA, NA, NA, NA, NA, NA, NA,
			 NA),
	[131] = PINGROUP(131, WEST, qlink_enable, NA, NA, NA, NA, NA, NA, NA,
			 NA),
	[132] = PINGROUP(132, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[133] = PINGROUP(133, NORTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[134] = PINGROUP(134, NORTH, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[135] = PINGROUP(135, WEST, NA, pa_indicator, NA, NA, NA, NA, NA, NA,
			 NA),
	[136] = PINGROUP(136, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[137] = PINGROUP(137, WEST, NA, NA, phase_flag26, NA, NA, NA, NA, NA,
			 NA),
	[138] = PINGROUP(138, WEST, NA, NA, phase_flag27, NA, NA, NA, NA, NA,
			 NA),
	[139] = PINGROUP(139, WEST, NA, phase_flag28, NA, NA, NA, NA, NA, NA,
			 NA),
	[140] = PINGROUP(140, WEST, NA, NA, phase_flag6, NA, NA, NA, NA, NA,
			 NA),
	[141] = PINGROUP(141, WEST, NA, phase_flag29, NA, NA, NA, NA, NA, NA,
			 NA),
	[142] = PINGROUP(142, WEST, NA, phase_flag30, NA, NA, NA, NA, NA, NA,
			 NA),
	[143] = PINGROUP(143, WEST, NA, NAV_PPS, NAV_PPS, GPS_TX, phase_flag31,
			 NA, NA, NA, NA),
	[144] = PINGROUP(144, SOUTH, mss_lte, NA, NA, NA, NA, NA, NA, NA, NA),
	[145] = PINGROUP(145, SOUTH, mss_lte, GPS_TX, NA, NA, NA, NA, NA, NA,
			 NA),
	[146] = PINGROUP(146, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[147] = PINGROUP(147, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[148] = PINGROUP(148, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[149] = PINGROUP(149, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[150] = SDC_QDSD_PINGROUP(sdc1_rclk, 0x599000, 15, 0),
	[151] = SDC_QDSD_PINGROUP(sdc1_clk, 0x599000, 13, 6),
	[152] = SDC_QDSD_PINGROUP(sdc1_cmd, 0x599000, 11, 3),
	[153] = SDC_QDSD_PINGROUP(sdc1_data, 0x599000, 9, 0),
	[154] = SDC_QDSD_PINGROUP(sdc2_clk, 0x99a000, 14, 6),
	[155] = SDC_QDSD_PINGROUP(sdc2_cmd, 0x99a000, 11, 3),
	[156] = SDC_QDSD_PINGROUP(sdc2_data, 0x99a000, 9, 0),
	[157] = UFS_RESET(ufs_reset, 0x99d000),
};
static struct msm_dir_conn sdm670_dir_conn[] = {
	{1, 510},
	{3, 511},
	{5, 512},
	{10, 513},
	{11, 514},
	{20, 515},
	{22, 516},
	{24, 517},
	{26, 518},
	{30, 519},
	{31, 632},
	{32, 521},
	{34, 522},
	{36, 523},
	{37, 524},
	{38, 525},
	{39, 526},
	{40, 527},
	{41, 630},
	{43, 529},
	{44, 530},
	{46, 531},
	{48, 532},
	{49, 633},
	{52, 534},
	{53, 535},
	{54, 536},
	{56, 537},
	{57, 538},
	{66, 546},
	{68, 547},
	{77, 550},
	{78, 551},
	{79, 552},
	{80, 553},
	{84, 554},
	{85, 555},
	{86, 556},
	{88, 557},
	{89, 631},
	{91, 559},
	{92, 560},
	{95, 561},
	{96, 562},
	{97, 563},
	{101, 564},
	{103, 565},
	{108, 567},
	{112, 568},
	{113, 569},
	{115, 570},
	{116, 571},
	{117, 572},
	{118, 573},
	{119, 609},
	{120, 610},
	{121, 611},
	{122, 612},
	{123, 613},
	{124, 614},
	{125, 615},
	{126, 616},
	{127, 617},
	{128, 618},
	{129, 619},
	{130, 620},
	{132, 621},
	{133, 622},
	{145, 623},
	{0, 216},
	{0, 215},
	{0, 214},
	{0, 213},
	{0, 212},
	{0, 211},
	{0, 210},
	{0, 209},
};

static const struct msm_pinctrl_soc_data sdm670_pinctrl = {
	.pins = sdm670_pins,
	.npins = ARRAY_SIZE(sdm670_pins),
	.functions = sdm670_functions,
	.nfunctions = ARRAY_SIZE(sdm670_functions),
	.groups = sdm670_groups,
	.ngroups = ARRAY_SIZE(sdm670_groups),
	.ngpios = 150,
	.dir_conn = sdm670_dir_conn,
	.n_dir_conns = ARRAY_SIZE(sdm670_dir_conn),
	.dir_conn_irq_base = 216,
};

static int sdm670_pinctrl_probe(struct platform_device *pdev)
{
	return msm_pinctrl_probe(pdev, &sdm670_pinctrl);
}

static const struct of_device_id sdm670_pinctrl_of_match[] = {
	{ .compatible = "qcom,sdm670-pinctrl", },
	{ },
};

static struct platform_driver sdm670_pinctrl_driver = {
	.driver = {
		.name = "sdm670-pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = sdm670_pinctrl_of_match,
	},
	.probe = sdm670_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init sdm670_pinctrl_init(void)
{
	return platform_driver_register(&sdm670_pinctrl_driver);
}
arch_initcall(sdm670_pinctrl_init);

static void __exit sdm670_pinctrl_exit(void)
{
	platform_driver_unregister(&sdm670_pinctrl_driver);
}
module_exit(sdm670_pinctrl_exit);

MODULE_DESCRIPTION("QTI sdm670 pinctrl driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, sdm670_pinctrl_of_match);
