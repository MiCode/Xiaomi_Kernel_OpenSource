// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
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

#define REG_BASE 0x100000
#define REG_SIZE 0x1000
#define PINGROUP(id, f1, f2, f3, f4, f5, f6, f7, f8, f9, wake_off, bit)	\
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
		.wake_reg = REG_BASE + wake_off,	\
		.wake_bit = bit,		\
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

#define QUP_I3C(qup_mode, qup_offset)			\
	{						\
		.mode = qup_mode,			\
		.offset = qup_offset,			\
	}


#define QUP_I3C_0_MODE_OFFSET	0xC8000
#define QUP_I3C_1_MODE_OFFSET	0xC9000
#define QUP_I3C_8_MODE_OFFSET	0xCA000
#define QUP_I3C_9_MODE_OFFSET	0xCB000

static const struct pinctrl_pin_desc yupik_pins[] = {
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
	PINCTRL_PIN(175, "UFS_RESET"),
	PINCTRL_PIN(176, "SDC1_RCLK"),
	PINCTRL_PIN(177, "SDC1_CLK"),
	PINCTRL_PIN(178, "SDC1_CMD"),
	PINCTRL_PIN(179, "SDC1_DATA"),
	PINCTRL_PIN(180, "SDC2_CLK"),
	PINCTRL_PIN(181, "SDC2_CMD"),
	PINCTRL_PIN(182, "SDC2_DATA"),
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

static const unsigned int ufs_reset_pins[] = { 175 };
static const unsigned int sdc1_rclk_pins[] = { 176 };
static const unsigned int sdc1_clk_pins[] = { 177 };
static const unsigned int sdc1_cmd_pins[] = { 178 };
static const unsigned int sdc1_data_pins[] = { 179 };
static const unsigned int sdc2_clk_pins[] = { 180 };
static const unsigned int sdc2_cmd_pins[] = { 181 };
static const unsigned int sdc2_data_pins[] = { 182 };

enum yupik_functions {
	msm_mux_gpio,
	msm_mux_atest_char,
	msm_mux_atest_char0,
	msm_mux_atest_char1,
	msm_mux_atest_char2,
	msm_mux_atest_char3,
	msm_mux_atest_usb0,
	msm_mux_atest_usb00,
	msm_mux_atest_usb01,
	msm_mux_atest_usb02,
	msm_mux_atest_usb03,
	msm_mux_atest_usb1,
	msm_mux_atest_usb10,
	msm_mux_atest_usb11,
	msm_mux_atest_usb12,
	msm_mux_atest_usb13,
	msm_mux_audio_ref,
	msm_mux_cam_mclk,
	msm_mux_cci_async,
	msm_mux_cci_i2c,
	msm_mux_cci_timer0,
	msm_mux_cci_timer1,
	msm_mux_cci_timer2,
	msm_mux_cci_timer3,
	msm_mux_cci_timer4,
	msm_mux_cmu_rng0,
	msm_mux_cmu_rng1,
	msm_mux_cmu_rng2,
	msm_mux_cmu_rng3,
	msm_mux_coex_uart1,
	msm_mux_cri_trng,
	msm_mux_cri_trng0,
	msm_mux_cri_trng1,
	msm_mux_dbg_out,
	msm_mux_ddr_bist,
	msm_mux_ddr_pxi0,
	msm_mux_ddr_pxi1,
	msm_mux_dp_hot,
	msm_mux_dp_lcd,
	msm_mux_edp_hot,
	msm_mux_edp_lcd,
	msm_mux_gcc_gp1,
	msm_mux_gcc_gp2,
	msm_mux_gcc_gp3,
	msm_mux_host2wlan_sol,
	msm_mux_ibi_i3c,
	msm_mux_jitter_bist,
	msm_mux_lpass_slimbus,
	msm_mux_mdp_vsync,
	msm_mux_mdp_vsync0,
	msm_mux_mdp_vsync1,
	msm_mux_mdp_vsync2,
	msm_mux_mdp_vsync3,
	msm_mux_mdp_vsync4,
	msm_mux_mdp_vsync5,
	msm_mux_mi2s0_data0,
	msm_mux_mi2s0_data1,
	msm_mux_mi2s0_sck,
	msm_mux_mi2s0_ws,
	msm_mux_mi2s1_data0,
	msm_mux_mi2s1_data1,
	msm_mux_mi2s1_sck,
	msm_mux_mi2s1_ws,
	msm_mux_mi2s2_data0,
	msm_mux_mi2s2_data1,
	msm_mux_mi2s2_sck,
	msm_mux_mi2s2_ws,
	msm_mux_mss_grfc0,
	msm_mux_mss_grfc1,
	msm_mux_mss_grfc10,
	msm_mux_mss_grfc11,
	msm_mux_mss_grfc12,
	msm_mux_mss_grfc2,
	msm_mux_mss_grfc3,
	msm_mux_mss_grfc4,
	msm_mux_mss_grfc5,
	msm_mux_mss_grfc6,
	msm_mux_mss_grfc7,
	msm_mux_mss_grfc8,
	msm_mux_mss_grfc9,
	msm_mux_nav_gpio0,
	msm_mux_nav_gpio1,
	msm_mux_nav_gpio2,
	msm_mux_pa_indicator,
	msm_mux_pcie0_clkreqn,
	msm_mux_pcie1_clkreqn,
	msm_mux_phase_flag0,
	msm_mux_phase_flag1,
	msm_mux_phase_flag10,
	msm_mux_phase_flag11,
	msm_mux_phase_flag12,
	msm_mux_phase_flag13,
	msm_mux_phase_flag14,
	msm_mux_phase_flag15,
	msm_mux_phase_flag16,
	msm_mux_phase_flag17,
	msm_mux_phase_flag18,
	msm_mux_phase_flag19,
	msm_mux_phase_flag2,
	msm_mux_phase_flag20,
	msm_mux_phase_flag21,
	msm_mux_phase_flag22,
	msm_mux_phase_flag23,
	msm_mux_phase_flag24,
	msm_mux_phase_flag25,
	msm_mux_phase_flag26,
	msm_mux_phase_flag27,
	msm_mux_phase_flag28,
	msm_mux_phase_flag29,
	msm_mux_phase_flag3,
	msm_mux_phase_flag30,
	msm_mux_phase_flag31,
	msm_mux_phase_flag4,
	msm_mux_phase_flag5,
	msm_mux_phase_flag6,
	msm_mux_phase_flag7,
	msm_mux_phase_flag8,
	msm_mux_phase_flag9,
	msm_mux_pll_bist,
	msm_mux_pll_bypassnl,
	msm_mux_pll_clk,
	msm_mux_pll_reset,
	msm_mux_pri_mi2s,
	msm_mux_prng_rosc0,
	msm_mux_prng_rosc1,
	msm_mux_prng_rosc2,
	msm_mux_prng_rosc3,
	msm_mux_qdss_cti,
	msm_mux_qdss_gpio,
	msm_mux_qdss_gpio0,
	msm_mux_qdss_gpio1,
	msm_mux_qdss_gpio10,
	msm_mux_qdss_gpio11,
	msm_mux_qdss_gpio12,
	msm_mux_qdss_gpio13,
	msm_mux_qdss_gpio14,
	msm_mux_qdss_gpio15,
	msm_mux_qdss_gpio2,
	msm_mux_qdss_gpio3,
	msm_mux_qdss_gpio4,
	msm_mux_qdss_gpio5,
	msm_mux_qdss_gpio6,
	msm_mux_qdss_gpio7,
	msm_mux_qdss_gpio8,
	msm_mux_qdss_gpio9,
	msm_mux_qlink0_enable,
	msm_mux_qlink0_request,
	msm_mux_qlink0_wmss,
	msm_mux_qlink1_enable,
	msm_mux_qlink1_request,
	msm_mux_qlink1_wmss,
	msm_mux_qspi_clk,
	msm_mux_qspi_cs,
	msm_mux_qspi_data,
	msm_mux_qup00,
	msm_mux_qup01,
	msm_mux_qup02,
	msm_mux_qup03,
	msm_mux_qup04,
	msm_mux_qup05,
	msm_mux_qup06,
	msm_mux_qup07,
	msm_mux_qup10,
	msm_mux_qup11,
	msm_mux_qup12,
	msm_mux_qup13,
	msm_mux_qup14,
	msm_mux_qup15,
	msm_mux_qup16,
	msm_mux_qup17,
	msm_mux_sd_write,
	msm_mux_sdc40,
	msm_mux_sdc41,
	msm_mux_sdc42,
	msm_mux_sdc43,
	msm_mux_sdc4_clk,
	msm_mux_sdc4_cmd,
	msm_mux_sec_mi2s,
	msm_mux_tb_trig,
	msm_mux_tgu_ch0,
	msm_mux_tgu_ch1,
	msm_mux_tsense_pwm1,
	msm_mux_tsense_pwm2,
	msm_mux_uim0_clk,
	msm_mux_uim0_data,
	msm_mux_uim0_present,
	msm_mux_uim0_reset,
	msm_mux_uim1_clk,
	msm_mux_uim1_data,
	msm_mux_uim1_present,
	msm_mux_uim1_reset,
	msm_mux_usb2phy_ac,
	msm_mux_usb_phy,
	msm_mux_vfr_0,
	msm_mux_vfr_1,
	msm_mux_vsense_trigger,
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
static const char * const atest_char_groups[] = {
	"gpio81",
};
static const char * const atest_char0_groups[] = {
	"gpio77",
};
static const char * const atest_char1_groups[] = {
	"gpio78",
};
static const char * const atest_char2_groups[] = {
	"gpio79",
};
static const char * const atest_char3_groups[] = {
	"gpio80",
};
static const char * const atest_usb0_groups[] = {
	"gpio107",
};
static const char * const atest_usb00_groups[] = {
	"gpio106",
};
static const char * const atest_usb01_groups[] = {
	"gpio105",
};
static const char * const atest_usb02_groups[] = {
	"gpio104",
};
static const char * const atest_usb03_groups[] = {
	"gpio103",
};
static const char * const atest_usb1_groups[] = {
	"gpio81",
};
static const char * const atest_usb10_groups[] = {
	"gpio80",
};
static const char * const atest_usb11_groups[] = {
	"gpio79",
};
static const char * const atest_usb12_groups[] = {
	"gpio78",
};
static const char * const atest_usb13_groups[] = {
	"gpio77",
};
static const char * const audio_ref_groups[] = {
	"gpio105",
};
static const char * const cam_mclk_groups[] = {
	"gpio64", "gpio65", "gpio66", "gpio67", "gpio68", "gpio93",
};
static const char * const cci_async_groups[] = {
	"gpio78", "gpio79", "gpio93",
};
static const char * const cci_i2c_groups[] = {
	"gpio69", "gpio70", "gpio71", "gpio72", "gpio73", "gpio74", "gpio75",
	"gpio76",
};
static const char * const cci_timer0_groups[] = {
	"gpio20",
};
static const char * const cci_timer1_groups[] = {
	"gpio21",
};
static const char * const cci_timer2_groups[] = {
	"gpio77",
};
static const char * const cci_timer3_groups[] = {
	"gpio78",
};
static const char * const cci_timer4_groups[] = {
	"gpio79",
};
static const char * const cmu_rng0_groups[] = {
	"gpio120",
};
static const char * const cmu_rng1_groups[] = {
	"gpio119",
};
static const char * const cmu_rng2_groups[] = {
	"gpio118",
};
static const char * const cmu_rng3_groups[] = {
	"gpio117",
};
static const char * const coex_uart1_groups[] = {
	"gpio127", "gpio128",
};
static const char * const cri_trng_groups[] = {
	"gpio124",
};
static const char * const cri_trng0_groups[] = {
	"gpio121",
};
static const char * const cri_trng1_groups[] = {
	"gpio122",
};
static const char * const dbg_out_groups[] = {
	"gpio38",
};
static const char * const ddr_bist_groups[] = {
	"gpio56", "gpio57", "gpio58", "gpio59",
};
static const char * const ddr_pxi0_groups[] = {
	"gpio14", "gpio15",
};
static const char * const ddr_pxi1_groups[] = {
	"gpio12", "gpio13",
};
static const char * const dp_hot_groups[] = {
	"gpio47",
};
static const char * const dp_lcd_groups[] = {
	"gpio81",
};
static const char * const edp_hot_groups[] = {
	"gpio60",
};
static const char * const edp_lcd_groups[] = {
	"gpio46",
};
static const char * const gcc_gp1_groups[] = {
	"gpio76", "gpio105",
};
static const char * const gcc_gp2_groups[] = {
	"gpio77", "gpio106",
};
static const char * const gcc_gp3_groups[] = {
	"gpio78", "gpio107",
};
static const char * const host2wlan_sol_groups[] = {
	"gpio26",
};
static const char * const ibi_i3c_groups[] = {
	"gpio0", "gpio1", "gpio4", "gpio5", "gpio36", "gpio37",
};
static const char * const jitter_bist_groups[] = {
	"gpio79",
};
static const char * const lpass_slimbus_groups[] = {
	"gpio94", "gpio95",
};
static const char * const mdp_vsync_groups[] = {
	"gpio14", "gpio16", "gpio79", "gpio80", "gpio81",
};
static const char * const mdp_vsync0_groups[] = {
	"gpio80",
};
static const char * const mdp_vsync1_groups[] = {
	"gpio80",
};
static const char * const mdp_vsync2_groups[] = {
	"gpio81",
};
static const char * const mdp_vsync3_groups[] = {
	"gpio81",
};
static const char * const mdp_vsync4_groups[] = {
	"gpio80",
};
static const char * const mdp_vsync5_groups[] = {
	"gpio81",
};
static const char * const mi2s0_data0_groups[] = {
	"gpio98",
};
static const char * const mi2s0_data1_groups[] = {
	"gpio99",
};
static const char * const mi2s0_sck_groups[] = {
	"gpio97",
};
static const char * const mi2s0_ws_groups[] = {
	"gpio100",
};
static const char * const mi2s1_data0_groups[] = {
	"gpio107",
};
static const char * const mi2s1_data1_groups[] = {
	"gpio105",
};
static const char * const mi2s1_sck_groups[] = {
	"gpio106",
};
static const char * const mi2s1_ws_groups[] = {
	"gpio108",
};
static const char * const mi2s2_data0_groups[] = {
	"gpio102",
};
static const char * const mi2s2_data1_groups[] = {
	"gpio104",
};
static const char * const mi2s2_sck_groups[] = {
	"gpio101",
};
static const char * const mi2s2_ws_groups[] = {
	"gpio103",
};
static const char * const mss_grfc0_groups[] = {
	"gpio117", "gpio132",
};
static const char * const mss_grfc1_groups[] = {
	"gpio118",
};
static const char * const mss_grfc10_groups[] = {
	"gpio127",
};
static const char * const mss_grfc11_groups[] = {
	"gpio128",
};
static const char * const mss_grfc12_groups[] = {
	"gpio131",
};
static const char * const mss_grfc2_groups[] = {
	"gpio119",
};
static const char * const mss_grfc3_groups[] = {
	"gpio120",
};
static const char * const mss_grfc4_groups[] = {
	"gpio121",
};
static const char * const mss_grfc5_groups[] = {
	"gpio122",
};
static const char * const mss_grfc6_groups[] = {
	"gpio123",
};
static const char * const mss_grfc7_groups[] = {
	"gpio124",
};
static const char * const mss_grfc8_groups[] = {
	"gpio125",
};
static const char * const mss_grfc9_groups[] = {
	"gpio126",
};
static const char * const nav_gpio0_groups[] = {
	"gpio129",
};
static const char * const nav_gpio1_groups[] = {
	"gpio130",
};
static const char * const nav_gpio2_groups[] = {
	"gpio131",
};
static const char * const pa_indicator_groups[] = {
	"gpio131",
};
static const char * const pcie0_clkreqn_groups[] = {
	"gpio88",
};
static const char * const pcie1_clkreqn_groups[] = {
	"gpio79",
};
static const char * const phase_flag0_groups[] = {
	"gpio132",
};
static const char * const phase_flag1_groups[] = {
	"gpio131",
};
static const char * const phase_flag10_groups[] = {
	"gpio122",
};
static const char * const phase_flag11_groups[] = {
	"gpio121",
};
static const char * const phase_flag12_groups[] = {
	"gpio120",
};
static const char * const phase_flag13_groups[] = {
	"gpio119",
};
static const char * const phase_flag14_groups[] = {
	"gpio118",
};
static const char * const phase_flag15_groups[] = {
	"gpio117",
};
static const char * const phase_flag16_groups[] = {
	"gpio63",
};
static const char * const phase_flag17_groups[] = {
	"gpio62",
};
static const char * const phase_flag18_groups[] = {
	"gpio61",
};
static const char * const phase_flag19_groups[] = {
	"gpio60",
};
static const char * const phase_flag2_groups[] = {
	"gpio130",
};
static const char * const phase_flag20_groups[] = {
	"gpio59",
};
static const char * const phase_flag21_groups[] = {
	"gpio58",
};
static const char * const phase_flag22_groups[] = {
	"gpio57",
};
static const char * const phase_flag23_groups[] = {
	"gpio56",
};
static const char * const phase_flag24_groups[] = {
	"gpio19",
};
static const char * const phase_flag25_groups[] = {
	"gpio18",
};
static const char * const phase_flag26_groups[] = {
	"gpio17",
};
static const char * const phase_flag27_groups[] = {
	"gpio16",
};
static const char * const phase_flag28_groups[] = {
	"gpio15",
};
static const char * const phase_flag29_groups[] = {
	"gpio14",
};
static const char * const phase_flag3_groups[] = {
	"gpio129",
};
static const char * const phase_flag30_groups[] = {
	"gpio13",
};
static const char * const phase_flag31_groups[] = {
	"gpio12",
};
static const char * const phase_flag4_groups[] = {
	"gpio128",
};
static const char * const phase_flag5_groups[] = {
	"gpio127",
};
static const char * const phase_flag6_groups[] = {
	"gpio126",
};
static const char * const phase_flag7_groups[] = {
	"gpio125",
};
static const char * const phase_flag8_groups[] = {
	"gpio124",
};
static const char * const phase_flag9_groups[] = {
	"gpio123",
};
static const char * const pll_bist_groups[] = {
	"gpio80",
};
static const char * const pll_bypassnl_groups[] = {
	"gpio66",
};
static const char * const pll_clk_groups[] = {
	"gpio140",
};
static const char * const pll_reset_groups[] = {
	"gpio67",
};
static const char * const pri_mi2s_groups[] = {
	"gpio96",
};
static const char * const prng_rosc0_groups[] = {
	"gpio123",
};
static const char * const prng_rosc1_groups[] = {
	"gpio120",
};
static const char * const prng_rosc2_groups[] = {
	"gpio121",
};
static const char * const prng_rosc3_groups[] = {
	"gpio122",
};
static const char * const qdss_cti_groups[] = {
	"gpio15", "gpio16", "gpio18", "gpio19", "gpio156", "gpio157",
	"gpio165", "gpio166",
};
static const char * const qdss_gpio_groups[] = {
	"gpio12", "gpio13", "gpio58", "gpio59",
};
static const char * const qdss_gpio0_groups[] = {
	"gpio2", "gpio101",
};
static const char * const qdss_gpio1_groups[] = {
	"gpio3", "gpio102",
};
static const char * const qdss_gpio10_groups[] = {
	"gpio24", "gpio152",
};
static const char * const qdss_gpio11_groups[] = {
	"gpio25", "gpio153",
};
static const char * const qdss_gpio12_groups[] = {
	"gpio26", "gpio171",
};
static const char * const qdss_gpio13_groups[] = {
	"gpio27", "gpio172",
};
static const char * const qdss_gpio14_groups[] = {
	"gpio28", "gpio173",
};
static const char * const qdss_gpio15_groups[] = {
	"gpio29", "gpio174",
};
static const char * const qdss_gpio2_groups[] = {
	"gpio8", "gpio103",
};
static const char * const qdss_gpio3_groups[] = {
	"gpio9", "gpio104",
};
static const char * const qdss_gpio4_groups[] = {
	"gpio10", "gpio105",
};
static const char * const qdss_gpio5_groups[] = {
	"gpio11", "gpio106",
};
static const char * const qdss_gpio6_groups[] = {
	"gpio20", "gpio107",
};
static const char * const qdss_gpio7_groups[] = {
	"gpio21", "gpio108",
};
static const char * const qdss_gpio8_groups[] = {
	"gpio22", "gpio150",
};
static const char * const qdss_gpio9_groups[] = {
	"gpio23", "gpio151",
};
static const char * const qlink0_enable_groups[] = {
	"gpio134",
};
static const char * const qlink0_request_groups[] = {
	"gpio133",
};
static const char * const qlink0_wmss_groups[] = {
	"gpio135",
};
static const char * const qlink1_enable_groups[] = {
	"gpio137",
};
static const char * const qlink1_request_groups[] = {
	"gpio136",
};
static const char * const qlink1_wmss_groups[] = {
	"gpio138",
};
static const char * const qspi_clk_groups[] = {
	"gpio14",
};
static const char * const qspi_cs_groups[] = {
	"gpio15", "gpio19",
};
static const char * const qspi_data_groups[] = {
	"gpio12", "gpio13", "gpio16", "gpio17",
};
static const char * const qup00_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3",
};
static const char * const qup01_groups[] = {
	"gpio4", "gpio5", "gpio6", "gpio7",
};
static const char * const qup02_groups[] = {
	"gpio8", "gpio9", "gpio10", "gpio11",
};
static const char * const qup03_groups[] = {
	"gpio12", "gpio13", "gpio14", "gpio15",
};
static const char * const qup04_groups[] = {
	"gpio16", "gpio17", "gpio18", "gpio19",
};
static const char * const qup05_groups[] = {
	"gpio20", "gpio21", "gpio22", "gpio23",
};
static const char * const qup06_groups[] = {
	"gpio24", "gpio25", "gpio26", "gpio27",
};
static const char * const qup07_groups[] = {
	"gpio2", "gpio3", "gpio6", "gpio28", "gpio29", "gpio30", "gpio31",
};
static const char * const qup10_groups[] = {
	"gpio32", "gpio33", "gpio34", "gpio35",
};
static const char * const qup11_groups[] = {
	"gpio36", "gpio37", "gpio38", "gpio39",
};
static const char * const qup12_groups[] = {
	"gpio40", "gpio41", "gpio42", "gpio43",
};
static const char * const qup13_groups[] = {
	"gpio44", "gpio45", "gpio46", "gpio47",
};
static const char * const qup14_groups[] = {
	"gpio38", "gpio48", "gpio49", "gpio50", "gpio51", "gpio54", "gpio55",
};
static const char * const qup15_groups[] = {
	"gpio52", "gpio53", "gpio54", "gpio55",
};
static const char * const qup16_groups[] = {
	"gpio50", "gpio56", "gpio57", "gpio58", "gpio59", "gpio62", "gpio63",
};
static const char * const qup17_groups[] = {
	"gpio60", "gpio61", "gpio62", "gpio63",
};
static const char * const sd_write_groups[] = {
	"gpio61",
};
static const char * const sdc40_groups[] = {
	"gpio12",
};
static const char * const sdc41_groups[] = {
	"gpio13",
};
static const char * const sdc42_groups[] = {
	"gpio16",
};
static const char * const sdc43_groups[] = {
	"gpio17",
};
static const char * const sdc4_clk_groups[] = {
	"gpio14",
};
static const char * const sdc4_cmd_groups[] = {
	"gpio19",
};
static const char * const sec_mi2s_groups[] = {
	"gpio105",
};
static const char * const tb_trig_groups[] = {
	"gpio12", "gpio13", "gpio15",
};
static const char * const tgu_ch0_groups[] = {
	"gpio65",
};
static const char * const tgu_ch1_groups[] = {
	"gpio66",
};
static const char * const tsense_pwm1_groups[] = {
	"gpio61",
};
static const char * const tsense_pwm2_groups[] = {
	"gpio61",
};
static const char * const uim0_clk_groups[] = {
	"gpio114",
};
static const char * const uim0_data_groups[] = {
	"gpio113",
};
static const char * const uim0_present_groups[] = {
	"gpio116",
};
static const char * const uim0_reset_groups[] = {
	"gpio115",
};
static const char * const uim1_clk_groups[] = {
	"gpio110",
};
static const char * const uim1_data_groups[] = {
	"gpio109",
};
static const char * const uim1_present_groups[] = {
	"gpio112",
};
static const char * const uim1_reset_groups[] = {
	"gpio111",
};
static const char * const usb2phy_ac_groups[] = {
	"gpio84", "gpio85",
};
static const char * const usb_phy_groups[] = {
	"gpio140",
};
static const char * const vfr_0_groups[] = {
	"gpio80",
};
static const char * const vfr_1_groups[] = {
	"gpio103",
};
static const char * const vsense_trigger_groups[] = {
	"gpio100",
};

static const struct msm_function yupik_functions[] = {
	FUNCTION(ibi_i3c),
	FUNCTION(gpio),
	FUNCTION(qup00),
	FUNCTION(qup07),
	FUNCTION(qdss_gpio0),
	FUNCTION(qdss_gpio1),
	FUNCTION(qup01),
	FUNCTION(qup02),
	FUNCTION(qdss_gpio2),
	FUNCTION(qdss_gpio3),
	FUNCTION(qdss_gpio4),
	FUNCTION(qdss_gpio5),
	FUNCTION(qup03),
	FUNCTION(qspi_data),
	FUNCTION(sdc40),
	FUNCTION(tb_trig),
	FUNCTION(phase_flag31),
	FUNCTION(qdss_gpio),
	FUNCTION(ddr_pxi1),
	FUNCTION(sdc41),
	FUNCTION(phase_flag30),
	FUNCTION(qspi_clk),
	FUNCTION(sdc4_clk),
	FUNCTION(mdp_vsync),
	FUNCTION(phase_flag29),
	FUNCTION(ddr_pxi0),
	FUNCTION(qspi_cs),
	FUNCTION(phase_flag28),
	FUNCTION(qdss_cti),
	FUNCTION(qup04),
	FUNCTION(sdc42),
	FUNCTION(phase_flag27),
	FUNCTION(sdc43),
	FUNCTION(phase_flag26),
	FUNCTION(phase_flag25),
	FUNCTION(sdc4_cmd),
	FUNCTION(phase_flag24),
	FUNCTION(qup05),
	FUNCTION(cci_timer0),
	FUNCTION(qdss_gpio6),
	FUNCTION(cci_timer1),
	FUNCTION(qdss_gpio7),
	FUNCTION(qdss_gpio8),
	FUNCTION(qdss_gpio9),
	FUNCTION(qup06),
	FUNCTION(qdss_gpio10),
	FUNCTION(qdss_gpio11),
	FUNCTION(host2wlan_sol),
	FUNCTION(qdss_gpio12),
	FUNCTION(qdss_gpio13),
	FUNCTION(qdss_gpio14),
	FUNCTION(qdss_gpio15),
	FUNCTION(qup10),
	FUNCTION(qup11),
	FUNCTION(qup14),
	FUNCTION(dbg_out),
	FUNCTION(qup12),
	FUNCTION(qup13),
	FUNCTION(edp_lcd),
	FUNCTION(dp_hot),
	FUNCTION(qup16),
	FUNCTION(qup15),
	FUNCTION(ddr_bist),
	FUNCTION(phase_flag23),
	FUNCTION(phase_flag22),
	FUNCTION(phase_flag21),
	FUNCTION(phase_flag20),
	FUNCTION(qup17),
	FUNCTION(edp_hot),
	FUNCTION(phase_flag19),
	FUNCTION(sd_write),
	FUNCTION(phase_flag18),
	FUNCTION(tsense_pwm1),
	FUNCTION(tsense_pwm2),
	FUNCTION(phase_flag17),
	FUNCTION(phase_flag16),
	FUNCTION(cam_mclk),
	FUNCTION(tgu_ch0),
	FUNCTION(pll_bypassnl),
	FUNCTION(tgu_ch1),
	FUNCTION(pll_reset),
	FUNCTION(cci_i2c),
	FUNCTION(gcc_gp1),
	FUNCTION(cci_timer2),
	FUNCTION(gcc_gp2),
	FUNCTION(atest_usb13),
	FUNCTION(atest_char0),
	FUNCTION(cci_timer3),
	FUNCTION(cci_async),
	FUNCTION(gcc_gp3),
	FUNCTION(atest_usb12),
	FUNCTION(atest_char1),
	FUNCTION(cci_timer4),
	FUNCTION(pcie1_clkreqn),
	FUNCTION(jitter_bist),
	FUNCTION(atest_usb11),
	FUNCTION(atest_char2),
	FUNCTION(vfr_0),
	FUNCTION(mdp_vsync0),
	FUNCTION(mdp_vsync1),
	FUNCTION(mdp_vsync4),
	FUNCTION(pll_bist),
	FUNCTION(atest_usb10),
	FUNCTION(atest_char3),
	FUNCTION(dp_lcd),
	FUNCTION(mdp_vsync2),
	FUNCTION(mdp_vsync3),
	FUNCTION(mdp_vsync5),
	FUNCTION(atest_usb1),
	FUNCTION(atest_char),
	FUNCTION(usb2phy_ac),
	FUNCTION(pcie0_clkreqn),
	FUNCTION(lpass_slimbus),
	FUNCTION(pri_mi2s),
	FUNCTION(mi2s0_sck),
	FUNCTION(mi2s0_data0),
	FUNCTION(mi2s0_data1),
	FUNCTION(mi2s0_ws),
	FUNCTION(vsense_trigger),
	FUNCTION(mi2s2_sck),
	FUNCTION(mi2s2_data0),
	FUNCTION(mi2s2_ws),
	FUNCTION(vfr_1),
	FUNCTION(atest_usb03),
	FUNCTION(mi2s2_data1),
	FUNCTION(atest_usb02),
	FUNCTION(sec_mi2s),
	FUNCTION(mi2s1_data1),
	FUNCTION(audio_ref),
	FUNCTION(atest_usb01),
	FUNCTION(mi2s1_sck),
	FUNCTION(atest_usb00),
	FUNCTION(mi2s1_data0),
	FUNCTION(atest_usb0),
	FUNCTION(mi2s1_ws),
	FUNCTION(uim1_data),
	FUNCTION(uim1_clk),
	FUNCTION(uim1_reset),
	FUNCTION(uim1_present),
	FUNCTION(uim0_data),
	FUNCTION(uim0_clk),
	FUNCTION(uim0_reset),
	FUNCTION(uim0_present),
	FUNCTION(mss_grfc0),
	FUNCTION(cmu_rng3),
	FUNCTION(phase_flag15),
	FUNCTION(mss_grfc1),
	FUNCTION(cmu_rng2),
	FUNCTION(phase_flag14),
	FUNCTION(mss_grfc2),
	FUNCTION(cmu_rng1),
	FUNCTION(phase_flag13),
	FUNCTION(mss_grfc3),
	FUNCTION(cmu_rng0),
	FUNCTION(phase_flag12),
	FUNCTION(prng_rosc1),
	FUNCTION(mss_grfc4),
	FUNCTION(cri_trng0),
	FUNCTION(phase_flag11),
	FUNCTION(prng_rosc2),
	FUNCTION(mss_grfc5),
	FUNCTION(cri_trng1),
	FUNCTION(phase_flag10),
	FUNCTION(prng_rosc3),
	FUNCTION(mss_grfc6),
	FUNCTION(prng_rosc0),
	FUNCTION(phase_flag9),
	FUNCTION(mss_grfc7),
	FUNCTION(cri_trng),
	FUNCTION(phase_flag8),
	FUNCTION(mss_grfc8),
	FUNCTION(phase_flag7),
	FUNCTION(mss_grfc9),
	FUNCTION(phase_flag6),
	FUNCTION(coex_uart1),
	FUNCTION(mss_grfc10),
	FUNCTION(phase_flag5),
	FUNCTION(mss_grfc11),
	FUNCTION(phase_flag4),
	FUNCTION(nav_gpio0),
	FUNCTION(phase_flag3),
	FUNCTION(nav_gpio1),
	FUNCTION(phase_flag2),
	FUNCTION(mss_grfc12),
	FUNCTION(nav_gpio2),
	FUNCTION(pa_indicator),
	FUNCTION(phase_flag1),
	FUNCTION(phase_flag0),
	FUNCTION(qlink0_request),
	FUNCTION(qlink0_enable),
	FUNCTION(qlink0_wmss),
	FUNCTION(qlink1_request),
	FUNCTION(qlink1_enable),
	FUNCTION(qlink1_wmss),
	FUNCTION(usb_phy),
	FUNCTION(pll_clk),
};

/* Every pin is maintained as a single group, and missing or non-existing pin
 * would be maintained as dummy group to synchronize pin group index with
 * pin descriptor registered with pinctrl core.
 * Clients would not be able to request these dummy pin groups.
 */
static const struct msm_pingroup yupik_groups[] = {
	[0] = PINGROUP(0, qup00, ibi_i3c, NA, NA, NA, NA, NA, NA, NA,
		       0xAF00C, 1),
	[1] = PINGROUP(1, qup00, ibi_i3c, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[2] = PINGROUP(2, qup00, qup07, NA, qdss_gpio0, NA, NA, NA, NA, NA,
		       0, -1),
	[3] = PINGROUP(3, qup00, qup07, NA, qdss_gpio1, NA, NA, NA, NA, NA,
		       0xAF00C, 2),
	[4] = PINGROUP(4, qup01, ibi_i3c, NA, NA, NA, NA, NA, NA, NA,
		       0xAF00C, 3),
	[5] = PINGROUP(5, qup01, ibi_i3c, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[6] = PINGROUP(6, qup01, qup07, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[7] = PINGROUP(7, qup01, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF00C, 4),
	[8] = PINGROUP(8, qup02, NA, qdss_gpio2, NA, NA, NA, NA, NA, NA,
		       0xAF00C, 5),
	[9] = PINGROUP(9, qup02, NA, qdss_gpio3, NA, NA, NA, NA, NA, NA, 0, -1),
	[10] = PINGROUP(10, qup02, NA, qdss_gpio4, NA, NA, NA, NA, NA, NA,
			0, -1),
	[11] = PINGROUP(11, qup02, NA, qdss_gpio5, NA, NA, NA, NA, NA, NA,
			0xAF00C, 6),
	[12] = PINGROUP(12, qup03, qspi_data, sdc40, tb_trig, phase_flag31,
			qdss_gpio, ddr_pxi1, NA, NA, 0xAF00C, 7),
	[13] = PINGROUP(13, qup03, qspi_data, sdc41, tb_trig, phase_flag30,
			qdss_gpio, ddr_pxi1, NA, NA, 0, -1),
	[14] = PINGROUP(14, qup03, qspi_clk, sdc4_clk, mdp_vsync, phase_flag29,
			ddr_pxi0, NA, NA, NA, 0, -1),
	[15] = PINGROUP(15, qup03, qspi_cs, tb_trig, phase_flag28, qdss_cti,
			ddr_pxi0, NA, NA, NA, 0xAF00C, 8),
	[16] = PINGROUP(16, qup04, qspi_data, sdc42, mdp_vsync, phase_flag27,
			qdss_cti, NA, NA, NA, 0xAF00C, 9),
	[17] = PINGROUP(17, qup04, qspi_data, sdc43, NA, phase_flag26, NA, NA,
			NA, NA, 0, -1),
	[18] = PINGROUP(18, qup04, NA, phase_flag25, qdss_cti, NA, NA, NA, NA,
			NA, 0xAF00C, 10),
	[19] = PINGROUP(19, qup04, qspi_cs, sdc4_cmd, NA, phase_flag24,
			qdss_cti, NA, NA, NA, 0xAF00C, 11),
	[20] = PINGROUP(20, qup05, cci_timer0, NA, qdss_gpio6, NA, NA, NA, NA,
			NA, 0xAF00C, 12),
	[21] = PINGROUP(21, qup05, cci_timer1, NA, qdss_gpio7, NA, NA, NA, NA,
			NA, 0xAF00C, 13),
	[22] = PINGROUP(22, qup05, NA, qdss_gpio8, NA, NA, NA, NA, NA, NA,
			0, -1),
	[23] = PINGROUP(23, qup05, NA, qdss_gpio9, NA, NA, NA, NA, NA, NA,
			0xAF00C, 14),
	[24] = PINGROUP(24, qup06, NA, qdss_gpio10, NA, NA, NA, NA, NA, NA,
			0xAF00C, 15),
	[25] = PINGROUP(25, qup06, NA, qdss_gpio11, NA, NA, NA, NA, NA, NA,
			0xAF010, 0),
	[26] = PINGROUP(26, qup06, host2wlan_sol, NA, qdss_gpio12, NA, NA, NA,
			NA, NA, 0, -1),
	[27] = PINGROUP(27, qup06, NA, qdss_gpio13, NA, NA, NA, NA, NA, NA,
			0xAF010, 1),
	[28] = PINGROUP(28, qup07, NA, qdss_gpio14, NA, NA, NA, NA, NA, NA,
			0xAF010, 2),
	[29] = PINGROUP(29, qup07, qdss_gpio15, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[30] = PINGROUP(30, qup07, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[31] = PINGROUP(31, qup07, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF010, 3),
	[32] = PINGROUP(32, qup10, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF000, 3),
	[33] = PINGROUP(33, qup10, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[34] = PINGROUP(34, qup10, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF000, 4),
	[35] = PINGROUP(35, qup10, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF000, 5),
	[36] = PINGROUP(36, qup11, ibi_i3c, NA, NA, NA, NA, NA, NA, NA,
			0xAF000, 6),
	[37] = PINGROUP(37, qup11, ibi_i3c, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[38] = PINGROUP(38, qup11, qup14, dbg_out, NA, NA, NA, NA, NA, NA,
			0, -1),
	[39] = PINGROUP(39, qup11, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF000, 7),
	[40] = PINGROUP(40, qup12, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF000, 8),
	[41] = PINGROUP(41, qup12, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF000, 9),
	[42] = PINGROUP(42, qup12, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[43] = PINGROUP(43, qup12, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF000, 10),
	[44] = PINGROUP(44, qup13, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF000, 11),
	[45] = PINGROUP(45, qup13, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF000, 12),
	[46] = PINGROUP(46, qup13, edp_lcd, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[47] = PINGROUP(47, qup13, dp_hot, NA, NA, NA, NA, NA, NA, NA,
			0xAF000, 13),
	[48] = PINGROUP(48, qup14, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF000, 14),
	[49] = PINGROUP(49, qup14, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[50] = PINGROUP(50, qup14, qup16, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[51] = PINGROUP(51, qup14, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF000, 15),
	[52] = PINGROUP(52, qup15, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF004, 0),
	[53] = PINGROUP(53, qup15, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[54] = PINGROUP(54, qup15, qup14, NA, NA, NA, NA, NA, NA, NA,
			0xAF004, 1),
	[55] = PINGROUP(55, qup15, qup14, NA, NA, NA, NA, NA, NA, NA,
			0xAF004, 2),
	[56] = PINGROUP(56, qup16, ddr_bist, phase_flag23, NA, NA, NA, NA, NA,
			NA, 0xAF004, 3),
	[57] = PINGROUP(57, qup16, ddr_bist, phase_flag22, NA, NA, NA, NA, NA,
			NA, 0, -1),
	[58] = PINGROUP(58, qup16, ddr_bist, phase_flag21, qdss_gpio, NA, NA,
			NA, NA, NA, 0, -1),
	[59] = PINGROUP(59, qup16, ddr_bist, phase_flag20, qdss_gpio, NA, NA,
			NA, NA, NA, 0xAF004, 4),
	[60] = PINGROUP(60, qup17, edp_hot, NA, phase_flag19, NA, NA, NA, NA,
			NA, 0xAF018, 0),
	[61] = PINGROUP(61, qup17, sd_write, phase_flag18, tsense_pwm1,
			tsense_pwm2, NA, NA, NA, NA, 0xAF018, 1),
	[62] = PINGROUP(62, qup17, qup16, phase_flag17, NA, NA, NA, NA, NA, NA,
			0, -1),
	[63] = PINGROUP(63, qup17, qup16, phase_flag16, NA, NA, NA, NA, NA, NA,
			0xAF018, 2),
	[64] = PINGROUP(64, cam_mclk, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[65] = PINGROUP(65, cam_mclk, tgu_ch0, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[66] = PINGROUP(66, cam_mclk, pll_bypassnl, tgu_ch1, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[67] = PINGROUP(67, cam_mclk, pll_reset, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[68] = PINGROUP(68, cam_mclk, NA, NA, NA, NA, NA, NA, NA, NA,
			0xAF010, 4),
	[69] = PINGROUP(69, cci_i2c, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[70] = PINGROUP(70, cci_i2c, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[71] = PINGROUP(71, cci_i2c, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[72] = PINGROUP(72, cci_i2c, NA, NA, NA, NA, NA, NA, NA, NA,
			0xAF010, 5),
	[73] = PINGROUP(73, cci_i2c, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[74] = PINGROUP(74, cci_i2c, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[75] = PINGROUP(75, cci_i2c, NA, NA, NA, NA, NA, NA, NA, NA,
			0xAF010, 6),
	[76] = PINGROUP(76, cci_i2c, gcc_gp1, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[77] = PINGROUP(77, cci_timer2, gcc_gp2, NA, atest_usb13, atest_char0,
			NA, NA, NA, NA, 0xAF010, 7),
	[78] = PINGROUP(78, cci_timer3, cci_async, gcc_gp3, NA, atest_usb12,
			atest_char1, NA, NA, NA, 0xAF010, 8),
	[79] = PINGROUP(79, cci_timer4, cci_async, pcie1_clkreqn, mdp_vsync,
			jitter_bist, atest_usb11, atest_char2, NA, NA,
			0xAF010, 9),
	[80] = PINGROUP(80, mdp_vsync, vfr_0, mdp_vsync0, mdp_vsync1,
			mdp_vsync4, pll_bist, atest_usb10, atest_char3, NA,
			0xAF004, 5),
	[81] = PINGROUP(81, mdp_vsync, dp_lcd, mdp_vsync2, mdp_vsync3,
			mdp_vsync5, atest_usb1, atest_char, NA, NA, 0xAF004, 6),
	[82] = PINGROUP(82, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF018, 3),
	[83] = PINGROUP(83, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF010, 10),
	[84] = PINGROUP(84, usb2phy_ac, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[85] = PINGROUP(85, usb2phy_ac, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[86] = PINGROUP(86, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF010, 11),
	[87] = PINGROUP(87, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[88] = PINGROUP(88, pcie0_clkreqn, NA, NA, NA, NA, NA, NA, NA, NA,
			0xAF010, 12),
	[89] = PINGROUP(89, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF010, 13),
	[90] = PINGROUP(90, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF010, 14),
	[91] = PINGROUP(91, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF010, 15),
	[92] = PINGROUP(92, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF018, 4),
	[93] = PINGROUP(93, cam_mclk, cci_async, NA, NA, NA, NA, NA, NA, NA,
			0xAF014, 0),
	[94] = PINGROUP(94, lpass_slimbus, NA, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[95] = PINGROUP(95, lpass_slimbus, NA, NA, NA, NA, NA, NA, NA, NA,
			0xAF004, 7),
	[96] = PINGROUP(96, pri_mi2s, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[97] = PINGROUP(97, mi2s0_sck, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[98] = PINGROUP(98, mi2s0_data0, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[99] = PINGROUP(99, mi2s0_data1, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[100] = PINGROUP(100, mi2s0_ws, NA, vsense_trigger, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[101] = PINGROUP(101, mi2s2_sck, NA, qdss_gpio0, NA, NA, NA, NA, NA,
			 NA, 0xAF004, 8),
	[102] = PINGROUP(102, mi2s2_data0, NA, NA, qdss_gpio1, NA, NA, NA, NA,
			 NA, 0xAF004, 9),
	[103] = PINGROUP(103, mi2s2_ws, vfr_1, NA, NA, qdss_gpio2, NA,
			 atest_usb03, NA, NA, 0xAF004, 10),
	[104] = PINGROUP(104, mi2s2_data1, NA, NA, qdss_gpio3, NA, atest_usb02,
			 NA, NA, NA, 0xAF004, 11),
	[105] = PINGROUP(105, sec_mi2s, mi2s1_data1, audio_ref, gcc_gp1, NA,
			 qdss_gpio4, atest_usb01, NA, NA, 0, -1),
	[106] = PINGROUP(106, mi2s1_sck, gcc_gp2, NA, qdss_gpio5, atest_usb00,
			 NA, NA, NA, NA, 0, -1),
	[107] = PINGROUP(107, mi2s1_data0, gcc_gp3, NA, qdss_gpio6, atest_usb0,
			 NA, NA, NA, NA, 0, -1),
	[108] = PINGROUP(108, mi2s1_ws, NA, qdss_gpio7, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[109] = PINGROUP(109, uim1_data, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[110] = PINGROUP(110, uim1_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[111] = PINGROUP(111, uim1_reset, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[112] = PINGROUP(112, uim1_present, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xAF018, 5),
	[113] = PINGROUP(113, uim0_data, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[114] = PINGROUP(114, uim0_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[115] = PINGROUP(115, uim0_reset, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[116] = PINGROUP(116, uim0_present, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xAF018, 6),
	[117] = PINGROUP(117, NA, mss_grfc0, cmu_rng3, phase_flag15, NA, NA,
			 NA, NA, NA, 0xAF014, 1),
	[118] = PINGROUP(118, NA, mss_grfc1, cmu_rng2, phase_flag14, NA, NA,
			 NA, NA, NA, 0, -1),
	[119] = PINGROUP(119, NA, mss_grfc2, cmu_rng1, phase_flag13, NA, NA,
			 NA, NA, NA, 0xAF014, 2),
	[120] = PINGROUP(120, NA, mss_grfc3, cmu_rng0, phase_flag12, NA,
			 prng_rosc1, NA, NA, NA, 0, -1),
	[121] = PINGROUP(121, NA, mss_grfc4, cri_trng0, phase_flag11, NA,
			 prng_rosc2, NA, NA, NA, 0xAF014, 3),
	[122] = PINGROUP(122, NA, mss_grfc5, cri_trng1, phase_flag10, NA,
			 prng_rosc3, NA, NA, NA, 0, -1),
	[123] = PINGROUP(123, NA, mss_grfc6, prng_rosc0, phase_flag9, NA, NA,
			 NA, NA, NA, 0xAF014, 4),
	[124] = PINGROUP(124, NA, mss_grfc7, cri_trng, phase_flag8, NA, NA, NA,
			 NA, NA, 0, -1),
	[125] = PINGROUP(125, NA, mss_grfc8, phase_flag7, NA, NA, NA, NA, NA,
			 NA, 0xAF014, 5),
	[126] = PINGROUP(126, NA, mss_grfc9, phase_flag6, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[127] = PINGROUP(127, coex_uart1, mss_grfc10, phase_flag5, NA, NA, NA,
			 NA, NA, NA, 0xAF014, 6),
	[128] = PINGROUP(128, coex_uart1, mss_grfc11, phase_flag4, NA, NA, NA,
			 NA, NA, NA, 0xAF014, 7),
	[129] = PINGROUP(129, nav_gpio0, phase_flag3, NA, NA, NA, NA, NA, NA,
			 NA, 0xAF014, 8),
	[130] = PINGROUP(130, nav_gpio1, phase_flag2, NA, NA, NA, NA, NA, NA,
			 NA, 0xAF014, 9),
	[131] = PINGROUP(131, mss_grfc12, nav_gpio2, pa_indicator, phase_flag1,
			 NA, NA, NA, NA, NA, 0xAF014, 10),
	[132] = PINGROUP(132, mss_grfc0, phase_flag0, NA, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[133] = PINGROUP(133, qlink0_request, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xAF014, 11),
	[134] = PINGROUP(134, qlink0_enable, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[135] = PINGROUP(135, qlink0_wmss, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[136] = PINGROUP(136, qlink1_request, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xAF014, 12),
	[137] = PINGROUP(137, qlink1_enable, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[138] = PINGROUP(138, qlink1_wmss, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[139] = PINGROUP(139, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[140] = PINGROUP(140, usb_phy, pll_clk, NA, NA, NA, NA, NA, NA, NA,
			 0xAF018, 7),
	[141] = PINGROUP(141, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF018, 8),
	[142] = PINGROUP(142, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF018, 9),
	[143] = PINGROUP(143, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF018, 10),
	[144] = PINGROUP(144, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[145] = PINGROUP(145, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF004, 12),
	[146] = PINGROUP(146, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[147] = PINGROUP(147, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[148] = PINGROUP(148, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF004, 13),
	[149] = PINGROUP(149, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[150] = PINGROUP(150, qdss_gpio8, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xAF004, 14),
	[151] = PINGROUP(151, qdss_gpio9, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xAF004, 15),
	[152] = PINGROUP(152, qdss_gpio10, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[153] = PINGROUP(153, qdss_gpio11, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xAF008, 0),
	[154] = PINGROUP(154, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[155] = PINGROUP(155, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF008, 1),
	[156] = PINGROUP(156, qdss_cti, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xAF008, 2),
	[157] = PINGROUP(157, qdss_cti, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xAF008, 3),
	[158] = PINGROUP(158, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF008, 4),
	[159] = PINGROUP(159, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF008, 5),
	[160] = PINGROUP(160, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[161] = PINGROUP(161, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF008, 6),
	[162] = PINGROUP(162, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[163] = PINGROUP(163, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAF008, 7),
	[164] = PINGROUP(164, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[165] = PINGROUP(165, qdss_cti, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[166] = PINGROUP(166, qdss_cti, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xAF008, 8),
	[167] = PINGROUP(167, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[168] = PINGROUP(168, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[169] = PINGROUP(169, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[170] = PINGROUP(170, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[171] = PINGROUP(171, qdss_gpio12, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[172] = PINGROUP(172, qdss_gpio13, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xAF008, 9),
	[173] = PINGROUP(173, qdss_gpio14, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[174] = PINGROUP(174, qdss_gpio15, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xAF008, 10),
	[175] = UFS_RESET(ufs_reset, 0x1be000),
	[176] = SDC_QDSD_PINGROUP(sdc1_rclk, 0x1b3004, 0, 0),
	[177] = SDC_QDSD_PINGROUP(sdc1_clk, 0x1b3000, 13, 6),
	[178] = SDC_QDSD_PINGROUP(sdc1_cmd, 0x1b3000, 11, 3),
	[179] = SDC_QDSD_PINGROUP(sdc1_data, 0x1b3000, 9, 0),
	[180] = SDC_QDSD_PINGROUP(sdc2_clk, 0x1b4000, 14, 6),
	[181] = SDC_QDSD_PINGROUP(sdc2_cmd, 0x1b4000, 11, 3),
	[182] = SDC_QDSD_PINGROUP(sdc2_data, 0x1b4000, 9, 0),
};

static const int yupik_reserved_gpios[] = {
	32, 33, 48, 49, 50, 51, 56, 57, 58, 59, -1
};
static struct pinctrl_qup yupik_qup_regs[] = {
	QUP_I3C(0, QUP_I3C_0_MODE_OFFSET),
	QUP_I3C(1, QUP_I3C_1_MODE_OFFSET),
	QUP_I3C(8, QUP_I3C_8_MODE_OFFSET),
	QUP_I3C(9, QUP_I3C_9_MODE_OFFSET),
};

static const struct msm_gpio_wakeirq_map yupik_pdc_map[] = {
	{ 0, 134 }, { 3, 131 }, { 4, 121 }, { 7, 103 }, { 8, 155 },
	{ 11, 93 }, { 12, 78 }, { 15, 79 }, { 16, 80 }, { 18, 81 },
	{ 19, 107 }, { 20, 82 }, { 21, 83 }, { 23, 99 }, { 24, 86 },
	{ 25, 95 }, { 27, 158 }, { 28, 159 }, { 31, 90 }, { 32, 144 },
	{ 34, 77 }, { 35, 92 }, { 36, 157 }, { 39, 73 }, { 40, 97 },
	{ 41, 98 }, { 43, 85 }, { 44, 100 }, { 45, 101 }, { 47, 102 },
	{ 48, 74 }, { 51, 112 }, { 52, 156 }, { 54, 117 }, { 55, 84 },
	{ 56, 108 }, { 59, 110 }, { 60, 111 }, { 61, 123 }, { 63, 104 },
	{ 68, 127 }, { 72, 150 }, { 75, 133 }, { 77, 125 }, { 78, 105 },
	{ 79, 106 }, { 80, 118 }, { 81, 119 }, { 82, 162 }, { 83, 122 },
	{ 86, 75 }, { 88, 154 }, { 89, 124 }, { 90, 149 }, { 91, 76 },
	{ 93, 128 }, { 95, 160 }, { 101, 126 }, { 102, 96 }, { 103, 116 },
	{ 104, 114 }, { 112, 72 }, { 116, 135 }, { 117, 163 }, { 119, 137 },
	{ 121, 138 }, { 123, 139 }, { 125, 140 }, { 127, 141 }, { 128, 165 },
	{ 129, 143 }, { 130, 94 }, { 131, 145 }, { 133, 146 }, { 136, 147 },
	{ 140, 148 }, { 141, 115 }, { 142, 113 }, { 145, 130 }, { 148, 132 },
	{ 150, 87 }, { 151, 88 }, { 153, 89 }, { 155, 164 }, { 156, 129 },
	{ 157, 161 }, { 158, 120 }, { 161, 136 }, { 163, 142 }, { 172, 166 },
	{ 174, 167 },
};

static const struct msm_pinctrl_soc_data yupik_pinctrl = {
	.pins = yupik_pins,
	.npins = ARRAY_SIZE(yupik_pins),
	.functions = yupik_functions,
	.nfunctions = ARRAY_SIZE(yupik_functions),
	.groups = yupik_groups,
	.ngroups = ARRAY_SIZE(yupik_groups),
	.reserved_gpios = yupik_reserved_gpios,
	.ngpios = 176,
	.qup_regs = yupik_qup_regs,
	.nqup_regs = ARRAY_SIZE(yupik_qup_regs),
	.wakeirq_map = yupik_pdc_map,
	.nwakeirq_map = ARRAY_SIZE(yupik_pdc_map),
};

/* By default, all the gpios that are mpm wake capable are enabled.
 * The following list disables the gpios explicitly
 */
static const unsigned int config_mpm_wake_disable_gpios[] = { 127 };

static void yupik_pinctrl_config_mpm_wake_disable_gpios(void)
{
	unsigned int i;
	unsigned int n_gpios = ARRAY_SIZE(config_mpm_wake_disable_gpios);

	for (i = 0; i < n_gpios; i++)
		msm_gpio_mpm_wake_set(config_mpm_wake_disable_gpios[i], false);
}

static int yupik_pinctrl_probe(struct platform_device *pdev)
{
	int ret;

	ret = msm_pinctrl_probe(pdev, &yupik_pinctrl);
	if (ret)
		return ret;

	yupik_pinctrl_config_mpm_wake_disable_gpios();

	return 0;
}

static const struct of_device_id yupik_pinctrl_of_match[] = {
	{ .compatible = "qcom,yupik-pinctrl", },
	{ },
};

static struct platform_driver yupik_pinctrl_driver = {
	.driver = {
		.name = "yupik-pinctrl",
		.of_match_table = yupik_pinctrl_of_match,
	},
	.probe = yupik_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init yupik_pinctrl_init(void)
{
	return platform_driver_register(&yupik_pinctrl_driver);
}
arch_initcall(yupik_pinctrl_init);

static void __exit yupik_pinctrl_exit(void)
{
	platform_driver_unregister(&yupik_pinctrl_driver);
}
module_exit(yupik_pinctrl_exit);

MODULE_DESCRIPTION("QTI yupik pinctrl driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, yupik_pinctrl_of_match);
