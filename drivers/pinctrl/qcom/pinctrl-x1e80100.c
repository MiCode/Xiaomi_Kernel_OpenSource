// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>

#include "pinctrl-msm.h"

#define REG_BASE 0x100000
#define REG_SIZE 0x1000
#define PINGROUP(id, f1, f2, f3, f4, f5, f6, f7, f8, f9, wake_off, bit)	\
	{					        \
		.grp = PINCTRL_PINGROUP("gpio" #id,	\
			gpio##id##_pins,		\
			ARRAY_SIZE(gpio##id##_pins)),	\
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
		.i2c_pull_bit = 13,		\
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
		.grp = PINCTRL_PINGROUP(#pg_name,	\
			pg_name##_pins,			\
			ARRAY_SIZE(pg_name##_pins)),	\
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
		.grp = PINCTRL_PINGROUP(#pg_name,	\
			pg_name##_pins,			\
			ARRAY_SIZE(pg_name##_pins)),	\
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

static const struct pinctrl_pin_desc x1e80100_pins[] = {
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
	PINCTRL_PIN(175, "GPIO_175"),
	PINCTRL_PIN(176, "GPIO_176"),
	PINCTRL_PIN(177, "GPIO_177"),
	PINCTRL_PIN(178, "GPIO_178"),
	PINCTRL_PIN(179, "GPIO_179"),
	PINCTRL_PIN(180, "GPIO_180"),
	PINCTRL_PIN(181, "GPIO_181"),
	PINCTRL_PIN(182, "GPIO_182"),
	PINCTRL_PIN(183, "GPIO_183"),
	PINCTRL_PIN(184, "GPIO_184"),
	PINCTRL_PIN(185, "GPIO_185"),
	PINCTRL_PIN(186, "GPIO_186"),
	PINCTRL_PIN(187, "GPIO_187"),
	PINCTRL_PIN(188, "GPIO_188"),
	PINCTRL_PIN(189, "GPIO_189"),
	PINCTRL_PIN(190, "GPIO_190"),
	PINCTRL_PIN(191, "GPIO_191"),
	PINCTRL_PIN(192, "GPIO_192"),
	PINCTRL_PIN(193, "GPIO_193"),
	PINCTRL_PIN(194, "GPIO_194"),
	PINCTRL_PIN(195, "GPIO_195"),
	PINCTRL_PIN(196, "GPIO_196"),
	PINCTRL_PIN(197, "GPIO_197"),
	PINCTRL_PIN(198, "GPIO_198"),
	PINCTRL_PIN(199, "GPIO_199"),
	PINCTRL_PIN(200, "GPIO_200"),
	PINCTRL_PIN(201, "GPIO_201"),
	PINCTRL_PIN(202, "GPIO_202"),
	PINCTRL_PIN(203, "GPIO_203"),
	PINCTRL_PIN(204, "GPIO_204"),
	PINCTRL_PIN(205, "GPIO_205"),
	PINCTRL_PIN(206, "GPIO_206"),
	PINCTRL_PIN(207, "GPIO_207"),
	PINCTRL_PIN(208, "GPIO_208"),
	PINCTRL_PIN(209, "GPIO_209"),
	PINCTRL_PIN(210, "GPIO_210"),
	PINCTRL_PIN(211, "GPIO_211"),
	PINCTRL_PIN(212, "GPIO_212"),
	PINCTRL_PIN(213, "GPIO_213"),
	PINCTRL_PIN(214, "GPIO_214"),
	PINCTRL_PIN(215, "GPIO_215"),
	PINCTRL_PIN(216, "GPIO_216"),
	PINCTRL_PIN(217, "GPIO_217"),
	PINCTRL_PIN(218, "GPIO_218"),
	PINCTRL_PIN(219, "GPIO_219"),
	PINCTRL_PIN(220, "GPIO_220"),
	PINCTRL_PIN(221, "GPIO_221"),
	PINCTRL_PIN(222, "GPIO_222"),
	PINCTRL_PIN(223, "GPIO_223"),
	PINCTRL_PIN(224, "GPIO_224"),
	PINCTRL_PIN(225, "GPIO_225"),
	PINCTRL_PIN(226, "GPIO_226"),
	PINCTRL_PIN(227, "GPIO_227"),
	PINCTRL_PIN(228, "GPIO_228"),
	PINCTRL_PIN(229, "GPIO_229"),
	PINCTRL_PIN(230, "GPIO_230"),
	PINCTRL_PIN(231, "GPIO_231"),
	PINCTRL_PIN(232, "GPIO_232"),
	PINCTRL_PIN(233, "GPIO_233"),
	PINCTRL_PIN(234, "GPIO_234"),
	PINCTRL_PIN(235, "GPIO_235"),
	PINCTRL_PIN(236, "GPIO_236"),
	PINCTRL_PIN(237, "GPIO_237"),
	PINCTRL_PIN(238, "UFS_RESET"),
	PINCTRL_PIN(239, "SDC2_CLK"),
	PINCTRL_PIN(240, "SDC2_CMD"),
	PINCTRL_PIN(241, "SDC2_DATA"),
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
DECLARE_MSM_GPIO_PINS(175);
DECLARE_MSM_GPIO_PINS(176);
DECLARE_MSM_GPIO_PINS(177);
DECLARE_MSM_GPIO_PINS(178);
DECLARE_MSM_GPIO_PINS(179);
DECLARE_MSM_GPIO_PINS(180);
DECLARE_MSM_GPIO_PINS(181);
DECLARE_MSM_GPIO_PINS(182);
DECLARE_MSM_GPIO_PINS(183);
DECLARE_MSM_GPIO_PINS(184);
DECLARE_MSM_GPIO_PINS(185);
DECLARE_MSM_GPIO_PINS(186);
DECLARE_MSM_GPIO_PINS(187);
DECLARE_MSM_GPIO_PINS(188);
DECLARE_MSM_GPIO_PINS(189);
DECLARE_MSM_GPIO_PINS(190);
DECLARE_MSM_GPIO_PINS(191);
DECLARE_MSM_GPIO_PINS(192);
DECLARE_MSM_GPIO_PINS(193);
DECLARE_MSM_GPIO_PINS(194);
DECLARE_MSM_GPIO_PINS(195);
DECLARE_MSM_GPIO_PINS(196);
DECLARE_MSM_GPIO_PINS(197);
DECLARE_MSM_GPIO_PINS(198);
DECLARE_MSM_GPIO_PINS(199);
DECLARE_MSM_GPIO_PINS(200);
DECLARE_MSM_GPIO_PINS(201);
DECLARE_MSM_GPIO_PINS(202);
DECLARE_MSM_GPIO_PINS(203);
DECLARE_MSM_GPIO_PINS(204);
DECLARE_MSM_GPIO_PINS(205);
DECLARE_MSM_GPIO_PINS(206);
DECLARE_MSM_GPIO_PINS(207);
DECLARE_MSM_GPIO_PINS(208);
DECLARE_MSM_GPIO_PINS(209);
DECLARE_MSM_GPIO_PINS(210);
DECLARE_MSM_GPIO_PINS(211);
DECLARE_MSM_GPIO_PINS(212);
DECLARE_MSM_GPIO_PINS(213);
DECLARE_MSM_GPIO_PINS(214);
DECLARE_MSM_GPIO_PINS(215);
DECLARE_MSM_GPIO_PINS(216);
DECLARE_MSM_GPIO_PINS(217);
DECLARE_MSM_GPIO_PINS(218);
DECLARE_MSM_GPIO_PINS(219);
DECLARE_MSM_GPIO_PINS(220);
DECLARE_MSM_GPIO_PINS(221);
DECLARE_MSM_GPIO_PINS(222);
DECLARE_MSM_GPIO_PINS(223);
DECLARE_MSM_GPIO_PINS(224);
DECLARE_MSM_GPIO_PINS(225);
DECLARE_MSM_GPIO_PINS(226);
DECLARE_MSM_GPIO_PINS(227);
DECLARE_MSM_GPIO_PINS(228);
DECLARE_MSM_GPIO_PINS(229);
DECLARE_MSM_GPIO_PINS(230);
DECLARE_MSM_GPIO_PINS(231);
DECLARE_MSM_GPIO_PINS(232);
DECLARE_MSM_GPIO_PINS(233);
DECLARE_MSM_GPIO_PINS(234);
DECLARE_MSM_GPIO_PINS(235);
DECLARE_MSM_GPIO_PINS(236);
DECLARE_MSM_GPIO_PINS(237);

static const unsigned int ufs_reset_pins[] = { 238 };
static const unsigned int sdc2_clk_pins[] = { 239 };
static const unsigned int sdc2_cmd_pins[] = { 240 };
static const unsigned int sdc2_data_pins[] = { 241 };

enum x1e80100_functions {
	msm_mux_gpio,
	msm_mux_RESOUT_GPIO,
	msm_mux_aon_cci,
	msm_mux_aoss_cti,
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
	msm_mux_atest_usb2,
	msm_mux_atest_usb20,
	msm_mux_atest_usb21,
	msm_mux_atest_usb22,
	msm_mux_atest_usb23,
	msm_mux_atest_usb3,
	msm_mux_atest_usb30,
	msm_mux_atest_usb31,
	msm_mux_atest_usb32,
	msm_mux_atest_usb33,
	msm_mux_atest_usb4,
	msm_mux_atest_usb40,
	msm_mux_atest_usb41,
	msm_mux_atest_usb42,
	msm_mux_atest_usb43,
	msm_mux_atest_usb5,
	msm_mux_atest_usb50,
	msm_mux_atest_usb51,
	msm_mux_atest_usb52,
	msm_mux_atest_usb53,
	msm_mux_audio_ext,
	msm_mux_audio_ref,
	msm_mux_cam_aon,
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
	msm_mux_cri_trng,
	msm_mux_dbg_out,
	msm_mux_ddr_bist,
	msm_mux_ddr_pxi0,
	msm_mux_ddr_pxi1,
	msm_mux_ddr_pxi2,
	msm_mux_ddr_pxi3,
	msm_mux_ddr_pxi4,
	msm_mux_ddr_pxi5,
	msm_mux_ddr_pxi6,
	msm_mux_ddr_pxi7,
	msm_mux_edp0_hot,
	msm_mux_edp0_lcd,
	msm_mux_edp1_hot,
	msm_mux_edp1_lcd,
	msm_mux_eusb0_ac,
	msm_mux_eusb1_ac,
	msm_mux_eusb2_ac,
	msm_mux_eusb3_ac,
	msm_mux_eusb5_ac,
	msm_mux_eusb6_ac,
	msm_mux_gcc_gp1,
	msm_mux_gcc_gp2,
	msm_mux_gcc_gp3,
	msm_mux_i2s0_data0,
	msm_mux_i2s0_data1,
	msm_mux_i2s0_sck,
	msm_mux_i2s0_ws,
	msm_mux_i2s1_data0,
	msm_mux_i2s1_data1,
	msm_mux_i2s1_sck,
	msm_mux_i2s1_ws,
	msm_mux_ibi_i3c,
	msm_mux_jitter_bist,
	msm_mux_mdp_vsync0,
	msm_mux_mdp_vsync1,
	msm_mux_mdp_vsync2,
	msm_mux_mdp_vsync3,
	msm_mux_mdp_vsync4,
	msm_mux_mdp_vsync5,
	msm_mux_mdp_vsync6,
	msm_mux_mdp_vsync7,
	msm_mux_mdp_vsync8,
	msm_mux_pcie3_clk,
	msm_mux_pcie4_clk,
	msm_mux_pcie5_clk,
	msm_mux_pcie6a_clk,
	msm_mux_pcie6b_clk,
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
	msm_mux_pll_clk,
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
	msm_mux_qspi00,
	msm_mux_qspi01,
	msm_mux_qspi02,
	msm_mux_qspi03,
	msm_mux_qspi0_clk,
	msm_mux_qspi0_cs0,
	msm_mux_qspi0_cs1,
	msm_mux_qup0_se0,
	msm_mux_qup0_se1,
	msm_mux_qup0_se2,
	msm_mux_qup0_se3,
	msm_mux_qup0_se4,
	msm_mux_qup0_se5,
	msm_mux_qup0_se6,
	msm_mux_qup0_se7,
	msm_mux_qup1_se0,
	msm_mux_qup1_se1,
	msm_mux_qup1_se2,
	msm_mux_qup1_se3,
	msm_mux_qup1_se4,
	msm_mux_qup1_se5,
	msm_mux_qup1_se6,
	msm_mux_qup1_se7,
	msm_mux_qup2_se0,
	msm_mux_qup2_se1,
	msm_mux_qup2_se2,
	msm_mux_qup2_se3,
	msm_mux_qup2_se4,
	msm_mux_qup2_se5,
	msm_mux_qup2_se6,
	msm_mux_qup2_se7,
	msm_mux_sd_write,
	msm_mux_sdc4_clk,
	msm_mux_sdc4_cmd,
	msm_mux_sdc4_data0,
	msm_mux_sdc4_data1,
	msm_mux_sdc4_data2,
	msm_mux_sdc4_data3,
	msm_mux_sys_throttle,
	msm_mux_tb_trig,
	msm_mux_tgu_ch0,
	msm_mux_tgu_ch1,
	msm_mux_tgu_ch2,
	msm_mux_tgu_ch3,
	msm_mux_tgu_ch4,
	msm_mux_tgu_ch5,
	msm_mux_tgu_ch6,
	msm_mux_tgu_ch7,
	msm_mux_tmess_prng0,
	msm_mux_tmess_prng1,
	msm_mux_tmess_prng2,
	msm_mux_tmess_prng3,
	msm_mux_tsense_pwm1,
	msm_mux_tsense_pwm2,
	msm_mux_tsense_pwm3,
	msm_mux_tsense_pwm4,
	msm_mux_usb0_dp,
	msm_mux_usb0_phy,
	msm_mux_usb0_sbrx,
	msm_mux_usb0_sbtx,
	msm_mux_usb1_dp,
	msm_mux_usb1_phy,
	msm_mux_usb1_sbrx,
	msm_mux_usb1_sbtx,
	msm_mux_usb2_dp,
	msm_mux_usb2_phy,
	msm_mux_usb2_sbrx,
	msm_mux_usb2_sbtx,
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
	"gpio171", "gpio172", "gpio173", "gpio174", "gpio175", "gpio176",
	"gpio177", "gpio178", "gpio179", "gpio180", "gpio181", "gpio182",
	"gpio183", "gpio184", "gpio185", "gpio186", "gpio187", "gpio188",
	"gpio189", "gpio190", "gpio191", "gpio192", "gpio193", "gpio194",
	"gpio195", "gpio196", "gpio197", "gpio198", "gpio199", "gpio200",
	"gpio201", "gpio202", "gpio203", "gpio204", "gpio205", "gpio206",
	"gpio207", "gpio208", "gpio209", "gpio210", "gpio211", "gpio212",
	"gpio213", "gpio214", "gpio215", "gpio216", "gpio217", "gpio218",
	"gpio219", "gpio220", "gpio221", "gpio222", "gpio223", "gpio224",
	"gpio225", "gpio226", "gpio227", "gpio228", "gpio229", "gpio230",
	"gpio231", "gpio232", "gpio233", "gpio234", "gpio235", "gpio236",
	"gpio237",
};

static const char * const RESOUT_GPIO_groups[] = {
	"gpio160",
};

static const char * const aon_cci_groups[] = {
	"gpio235", "gpio236",
};

static const char * const aoss_cti_groups[] = {
	"gpio60", "gpio61", "gpio62", "gpio63",
};

static const char * const atest_char_groups[] = {
	"gpio181",
};

static const char * const atest_char0_groups[] = {
	"gpio185",
};

static const char * const atest_char1_groups[] = {
	"gpio184",
};

static const char * const atest_char2_groups[] = {
	"gpio188",
};

static const char * const atest_char3_groups[] = {
	"gpio182",
};

static const char * const atest_usb0_groups[] = {
	"gpio38",
};

static const char * const atest_usb00_groups[] = {
	"gpio9",
};

static const char * const atest_usb01_groups[] = {
	"gpio10",
};

static const char * const atest_usb02_groups[] = {
	"gpio41",
};

static const char * const atest_usb03_groups[] = {
	"gpio42",
};

static const char * const atest_usb1_groups[] = {
	"gpio43",
};

static const char * const atest_usb10_groups[] = {
	"gpio44",
};

static const char * const atest_usb11_groups[] = {
	"gpio45",
};

static const char * const atest_usb12_groups[] = {
	"gpio46",
};

static const char * const atest_usb13_groups[] = {
	"gpio47",
};

static const char * const atest_usb2_groups[] = {
	"gpio48",
};

static const char * const atest_usb20_groups[] = {
	"gpio49",
};

static const char * const atest_usb21_groups[] = {
	"gpio50",
};

static const char * const atest_usb22_groups[] = {
	"gpio51",
};

static const char * const atest_usb23_groups[] = {
	"gpio52",
};

static const char * const atest_usb3_groups[] = {
	"gpio53",
};

static const char * const atest_usb30_groups[] = {
	"gpio54",
};

static const char * const atest_usb31_groups[] = {
	"gpio35",
};

static const char * const atest_usb32_groups[] = {
	"gpio58",
};

static const char * const atest_usb33_groups[] = {
	"gpio59",
};

static const char * const atest_usb4_groups[] = {
	"gpio81",
};

static const char * const atest_usb40_groups[] = {
	"gpio65",
};

static const char * const atest_usb41_groups[] = {
	"gpio66",
};

static const char * const atest_usb42_groups[] = {
	"gpio67",
};

static const char * const atest_usb43_groups[] = {
	"gpio83",
};

static const char * const atest_usb5_groups[] = {
	"gpio80",
};

static const char * const atest_usb50_groups[] = {
	"gpio72",
};

static const char * const atest_usb51_groups[] = {
	"gpio73",
};

static const char * const atest_usb52_groups[] = {
	"gpio74",
};

static const char * const atest_usb53_groups[] = {
	"gpio75",
};

static const char * const audio_ext_groups[] = {
	"gpio134", "gpio142",
};

static const char * const audio_ref_groups[] = {
	"gpio142",
};

static const char * const cam_aon_groups[] = {
	"gpio100",
};

static const char * const cam_mclk_groups[] = {
	"gpio96", "gpio97", "gpio98", "gpio99",
};

static const char * const cci_async_groups[] = {
	"gpio111", "gpio112", "gpio113",
};

static const char * const cci_i2c_groups[] = {
	"gpio101", "gpio102", "gpio103", "gpio104", "gpio105", "gpio106",
};

static const char * const cci_timer0_groups[] = {
	"gpio109",
};

static const char * const cci_timer1_groups[] = {
	"gpio110",
};

static const char * const cci_timer2_groups[] = {
	"gpio111",
};

static const char * const cci_timer3_groups[] = {
	"gpio112",
};

static const char * const cci_timer4_groups[] = {
	"gpio113",
};

static const char * const cmu_rng0_groups[] = {
	"gpio48",
};

static const char * const cmu_rng1_groups[] = {
	"gpio47",
};

static const char * const cmu_rng2_groups[] = {
	"gpio46",
};

static const char * const cmu_rng3_groups[] = {
	"gpio45",
};

static const char * const cri_trng_groups[] = {
	"gpio187",
};

static const char * const dbg_out_groups[] = {
	"gpio51",
};

static const char * const ddr_bist_groups[] = {
	"gpio54", "gpio55", "gpio56", "gpio57",
};

static const char * const ddr_pxi0_groups[] = {
	"gpio9", "gpio38",
};

static const char * const ddr_pxi1_groups[] = {
	"gpio10", "gpio41",
};

static const char * const ddr_pxi2_groups[] = {
	"gpio42", "gpio43",
};

static const char * const ddr_pxi3_groups[] = {
	"gpio44", "gpio45",
};

static const char * const ddr_pxi4_groups[] = {
	"gpio46", "gpio47",
};

static const char * const ddr_pxi5_groups[] = {
	"gpio48", "gpio49",
};

static const char * const ddr_pxi6_groups[] = {
	"gpio50", "gpio51",
};

static const char * const ddr_pxi7_groups[] = {
	"gpio52", "gpio53",
};

static const char * const edp0_hot_groups[] = {
	"gpio119",
};

static const char * const edp0_lcd_groups[] = {
	"gpio120",
};

static const char * const edp1_hot_groups[] = {
	"gpio120",
};

static const char * const edp1_lcd_groups[] = {
	"gpio115", "gpio119",
};

static const char * const eusb0_ac_groups[] = {
	"gpio168",
};

static const char * const eusb1_ac_groups[] = {
	"gpio177",
};

static const char * const eusb2_ac_groups[] = {
	"gpio186",
};

static const char * const eusb3_ac_groups[] = {
	"gpio169",
};

static const char * const eusb5_ac_groups[] = {
	"gpio187",
};

static const char * const eusb6_ac_groups[] = {
	"gpio178",
};

static const char * const gcc_gp1_groups[] = {
	"gpio71", "gpio72",
};

static const char * const gcc_gp2_groups[] = {
	"gpio64", "gpio73",
};

static const char * const gcc_gp3_groups[] = {
	"gpio74", "gpio82",
};

static const char * const i2s0_data0_groups[] = {
	"gpio136",
};

static const char * const i2s0_data1_groups[] = {
	"gpio137",
};

static const char * const i2s0_sck_groups[] = {
	"gpio135",
};

static const char * const i2s0_ws_groups[] = {
	"gpio138",
};

static const char * const i2s1_data0_groups[] = {
	"gpio140",
};

static const char * const i2s1_data1_groups[] = {
	"gpio142",
};

static const char * const i2s1_sck_groups[] = {
	"gpio139",
};

static const char * const i2s1_ws_groups[] = {
	"gpio141",
};

static const char * const ibi_i3c_groups[] = {
	"gpio0", "gpio1", "gpio32", "gpio33", "gpio36", "gpio37", "gpio68",
	"gpio69",
};

static const char * const jitter_bist_groups[] = {
	"gpio42",
};

static const char * const mdp_vsync0_groups[] = {
	"gpio114",
};

static const char * const mdp_vsync1_groups[] = {
	"gpio114",
};

static const char * const mdp_vsync2_groups[] = {
	"gpio115",
};

static const char * const mdp_vsync3_groups[] = {
	"gpio115",
};

static const char * const mdp_vsync4_groups[] = {
	"gpio109",
};

static const char * const mdp_vsync5_groups[] = {
	"gpio110",
};

static const char * const mdp_vsync6_groups[] = {
	"gpio111",
};

static const char * const mdp_vsync7_groups[] = {
	"gpio112",
};

static const char * const mdp_vsync8_groups[] = {
	"gpio113",
};

static const char * const pcie3_clk_groups[] = {
	"gpio144",
};

static const char * const pcie4_clk_groups[] = {
	"gpio147",
};

static const char * const pcie5_clk_groups[] = {
	"gpio150",
};

static const char * const pcie6a_clk_groups[] = {
	"gpio153",
};

static const char * const pcie6b_clk_groups[] = {
	"gpio156",
};

static const char * const phase_flag0_groups[] = {
	"gpio6",
};

static const char * const phase_flag1_groups[] = {
	"gpio7",
};

static const char * const phase_flag10_groups[] = {
	"gpio16",
};

static const char * const phase_flag11_groups[] = {
	"gpio17",
};

static const char * const phase_flag12_groups[] = {
	"gpio18",
};

static const char * const phase_flag13_groups[] = {
	"gpio19",
};

static const char * const phase_flag14_groups[] = {
	"gpio20",
};

static const char * const phase_flag15_groups[] = {
	"gpio21",
};

static const char * const phase_flag16_groups[] = {
	"gpio22",
};

static const char * const phase_flag17_groups[] = {
	"gpio23",
};

static const char * const phase_flag18_groups[] = {
	"gpio24",
};

static const char * const phase_flag19_groups[] = {
	"gpio25",
};

static const char * const phase_flag2_groups[] = {
	"gpio8",
};

static const char * const phase_flag20_groups[] = {
	"gpio26",
};

static const char * const phase_flag21_groups[] = {
	"gpio27",
};

static const char * const phase_flag22_groups[] = {
	"gpio181",
};

static const char * const phase_flag23_groups[] = {
	"gpio182",
};

static const char * const phase_flag24_groups[] = {
	"gpio188",
};

static const char * const phase_flag25_groups[] = {
	"gpio184",
};

static const char * const phase_flag26_groups[] = {
	"gpio185",
};

static const char * const phase_flag27_groups[] = {
	"gpio186",
};

static const char * const phase_flag28_groups[] = {
	"gpio187",
};

static const char * const phase_flag29_groups[] = {
	"gpio76",
};

static const char * const phase_flag3_groups[] = {
	"gpio39",
};

static const char * const phase_flag30_groups[] = {
	"gpio77",
};

static const char * const phase_flag31_groups[] = {
	"gpio78",
};

static const char * const phase_flag4_groups[] = {
	"gpio40",
};

static const char * const phase_flag5_groups[] = {
	"gpio11",
};

static const char * const phase_flag6_groups[] = {
	"gpio12",
};

static const char * const phase_flag7_groups[] = {
	"gpio13",
};

static const char * const phase_flag8_groups[] = {
	"gpio14",
};

static const char * const phase_flag9_groups[] = {
	"gpio15",
};

static const char * const pll_bist_groups[] = {
	"gpio28",
};

static const char * const pll_clk_groups[] = {
	"gpio35",
};

static const char * const prng_rosc0_groups[] = {
	"gpio186",
};

static const char * const prng_rosc1_groups[] = {
	"gpio188",
};

static const char * const prng_rosc2_groups[] = {
	"gpio182",
};

static const char * const prng_rosc3_groups[] = {
	"gpio181",
};

static const char * const qdss_cti_groups[] = {
	"gpio18", "gpio19", "gpio23", "gpio27", "gpio161", "gpio162",
	"gpio215", "gpio217",
};

static const char * const qdss_gpio_groups[] = {
	"gpio104", "gpio105", "gpio227", "gpio228",
};

static const char * const qdss_gpio0_groups[] = {
	"gpio96", "gpio219",
};

static const char * const qdss_gpio1_groups[] = {
	"gpio97", "gpio220",
};

static const char * const qdss_gpio10_groups[] = {
	"gpio108", "gpio231",
};

static const char * const qdss_gpio11_groups[] = {
	"gpio109", "gpio232",
};

static const char * const qdss_gpio12_groups[] = {
	"gpio110", "gpio233",
};

static const char * const qdss_gpio13_groups[] = {
	"gpio111", "gpio234",
};

static const char * const qdss_gpio14_groups[] = {
	"gpio112", "gpio235",
};

static const char * const qdss_gpio15_groups[] = {
	"gpio113", "gpio236",
};

static const char * const qdss_gpio2_groups[] = {
	"gpio98", "gpio221",
};

static const char * const qdss_gpio3_groups[] = {
	"gpio99", "gpio222",
};

static const char * const qdss_gpio4_groups[] = {
	"gpio100", "gpio223",
};

static const char * const qdss_gpio5_groups[] = {
	"gpio101", "gpio224",
};

static const char * const qdss_gpio6_groups[] = {
	"gpio102", "gpio225",
};

static const char * const qdss_gpio7_groups[] = {
	"gpio103", "gpio226",
};

static const char * const qdss_gpio8_groups[] = {
	"gpio106", "gpio229",
};

static const char * const qdss_gpio9_groups[] = {
	"gpio107", "gpio230",
};
static const char * const qspi00_groups[] = {
	"gpio128",
};

static const char * const qspi01_groups[] = {
	"gpio129",
};

static const char * const qspi02_groups[] = {
	"gpio130",
};

static const char * const qspi03_groups[] = {
	"gpio131",
};

static const char * const qspi0_clk_groups[] = {
	"gpio127",
};

static const char * const qspi0_cs0_groups[] = {
	"gpio132",
};

static const char * const qspi0_cs1_groups[] = {
	"gpio133",
};

static const char * const qup0_se0_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3",
};

static const char * const qup0_se1_groups[] = {
	"gpio4", "gpio5", "gpio6", "gpio7",
};

static const char * const qup0_se2_groups[] = {
	"gpio8", "gpio9", "gpio10", "gpio11", "gpio17", "gpio18", "gpio19",
};

static const char * const qup0_se3_groups[] = {
	"gpio12", "gpio13", "gpio14", "gpio15", "gpio21", "gpio22", "gpio23",
};

static const char * const qup0_se4_groups[] = {
	"gpio16", "gpio17", "gpio18", "gpio19",
};

static const char * const qup0_se5_groups[] = {
	"gpio20", "gpio21", "gpio22", "gpio23",
};

static const char * const qup0_se6_groups[] = {
	"gpio24", "gpio25", "gpio26", "gpio27",
};

static const char * const qup0_se7_groups[] = {
	"gpio12", "gpio13", "gpio14", "gpio15",
};

static const char * const qup1_se0_groups[] = {
	"gpio32", "gpio33", "gpio34", "gpio35",
};

static const char * const qup1_se1_groups[] = {
	"gpio36", "gpio37", "gpio38", "gpio39",
};

static const char * const qup1_se2_groups[] = {
	"gpio40", "gpio41", "gpio42", "gpio43", "gpio49", "gpio50", "gpio51",
};

static const char * const qup1_se3_groups[] = {
	"gpio33", "gpio34", "gpio35", "gpio44", "gpio45", "gpio46", "gpio47",
};

static const char * const qup1_se4_groups[] = {
	"gpio48", "gpio49", "gpio50", "gpio51",
};

static const char * const qup1_se5_groups[] = {
	"gpio52", "gpio53", "gpio54", "gpio55",
};

static const char * const qup1_se6_groups[] = {
	"gpio56", "gpio57", "gpio58", "gpio59",
};

static const char * const qup1_se7_groups[] = {
	"gpio52", "gpio53", "gpio54", "gpio55",
};

static const char * const qup2_se0_groups[] = {
	"gpio64", "gpio65", "gpio66", "gpio67",
};

static const char * const qup2_se1_groups[] = {
	"gpio68", "gpio69", "gpio70", "gpio71",
};

static const char * const qup2_se2_groups[] = {
	"gpio72", "gpio73", "gpio74", "gpio75", "gpio81", "gpio82", "gpio83",
};

static const char * const qup2_se3_groups[] = {
	"gpio65", "gpio66", "gpio67", "gpio76", "gpio77", "gpio78", "gpio79",
};

static const char * const qup2_se4_groups[] = {
	"gpio80", "gpio81", "gpio82", "gpio83",
};

static const char * const qup2_se5_groups[] = {
	"gpio84", "gpio85", "gpio86", "gpio87",
};

static const char * const qup2_se6_groups[] = {
	"gpio88", "gpio89", "gpio90", "gpio91",
};

static const char * const qup2_se7_groups[] = {
	"gpio84", "gpio85", "gpio86", "gpio87",
};

static const char * const sd_write_groups[] = {
	"gpio162",
};

static const char * const sdc4_clk_groups[] = {
	"gpio127",
};

static const char * const sdc4_cmd_groups[] = {
	"gpio132",
};

static const char * const sdc4_data0_groups[] = {
	"gpio128",
};

static const char * const sdc4_data1_groups[] = {
	"gpio129",
};

static const char * const sdc4_data2_groups[] = {
	"gpio130",
};

static const char * const sdc4_data3_groups[] = {
	"gpio131",
};

static const char * const sys_throttle_groups[] = {
	"gpio39", "gpio94",
};

static const char * const tb_trig_groups[] = {
	"gpio133", "gpio137",
};

static const char * const tgu_ch0_groups[] = {
	"gpio81",
};

static const char * const tgu_ch1_groups[] = {
	"gpio65",
};

static const char * const tgu_ch2_groups[] = {
	"gpio66",
};

static const char * const tgu_ch3_groups[] = {
	"gpio67",
};

static const char * const tgu_ch4_groups[] = {
	"gpio68",
};

static const char * const tgu_ch5_groups[] = {
	"gpio69",
};

static const char * const tgu_ch6_groups[] = {
	"gpio83",
};

static const char * const tgu_ch7_groups[] = {
	"gpio80",
};

static const char * const tmess_prng0_groups[] = {
	"gpio92",
};

static const char * const tmess_prng1_groups[] = {
	"gpio93",
};

static const char * const tmess_prng2_groups[] = {
	"gpio94",
};

static const char * const tmess_prng3_groups[] = {
	"gpio95",
};

static const char * const tsense_pwm1_groups[] = {
	"gpio34",
};

static const char * const tsense_pwm2_groups[] = {
	"gpio34",
};

static const char * const tsense_pwm3_groups[] = {
	"gpio34",
};

static const char * const tsense_pwm4_groups[] = {
	"gpio34",
};

static const char * const usb0_dp_groups[] = {
	"gpio122",
};

static const char * const usb0_phy_groups[] = {
	"gpio121",
};

static const char * const usb0_sbrx_groups[] = {
	"gpio163",
};

static const char * const usb0_sbtx_groups[] = {
	"gpio164", "gpio165",
};

static const char * const usb1_dp_groups[] = {
	"gpio124",
};

static const char * const usb1_phy_groups[] = {
	"gpio123",
};

static const char * const usb1_sbrx_groups[] = {
	"gpio172",
};

static const char * const usb1_sbtx_groups[] = {
	"gpio173", "gpio174",
};

static const char * const usb2_dp_groups[] = {
	"gpio126",
};

static const char * const usb2_phy_groups[] = {
	"gpio125",
};

static const char * const usb2_sbrx_groups[] = {
	"gpio181",
};

static const char * const usb2_sbtx_groups[] = {
	"gpio182", "gpio183",
};

static const char * const vsense_trigger_groups[] = {
	"gpio38",
};

static const struct pinfunction x1e80100_functions[] = {
	MSM_PIN_FUNCTION(gpio),
	MSM_PIN_FUNCTION(RESOUT_GPIO),
	MSM_PIN_FUNCTION(aon_cci),
	MSM_PIN_FUNCTION(aoss_cti),
	MSM_PIN_FUNCTION(atest_char),
	MSM_PIN_FUNCTION(atest_char0),
	MSM_PIN_FUNCTION(atest_char1),
	MSM_PIN_FUNCTION(atest_char2),
	MSM_PIN_FUNCTION(atest_char3),
	MSM_PIN_FUNCTION(atest_usb0),
	MSM_PIN_FUNCTION(atest_usb00),
	MSM_PIN_FUNCTION(atest_usb01),
	MSM_PIN_FUNCTION(atest_usb02),
	MSM_PIN_FUNCTION(atest_usb03),
	MSM_PIN_FUNCTION(atest_usb1),
	MSM_PIN_FUNCTION(atest_usb10),
	MSM_PIN_FUNCTION(atest_usb11),
	MSM_PIN_FUNCTION(atest_usb12),
	MSM_PIN_FUNCTION(atest_usb13),
	MSM_PIN_FUNCTION(atest_usb2),
	MSM_PIN_FUNCTION(atest_usb20),
	MSM_PIN_FUNCTION(atest_usb21),
	MSM_PIN_FUNCTION(atest_usb22),
	MSM_PIN_FUNCTION(atest_usb23),
	MSM_PIN_FUNCTION(atest_usb3),
	MSM_PIN_FUNCTION(atest_usb30),
	MSM_PIN_FUNCTION(atest_usb31),
	MSM_PIN_FUNCTION(atest_usb32),
	MSM_PIN_FUNCTION(atest_usb33),
	MSM_PIN_FUNCTION(atest_usb4),
	MSM_PIN_FUNCTION(atest_usb40),
	MSM_PIN_FUNCTION(atest_usb41),
	MSM_PIN_FUNCTION(atest_usb42),
	MSM_PIN_FUNCTION(atest_usb43),
	MSM_PIN_FUNCTION(atest_usb5),
	MSM_PIN_FUNCTION(atest_usb50),
	MSM_PIN_FUNCTION(atest_usb51),
	MSM_PIN_FUNCTION(atest_usb52),
	MSM_PIN_FUNCTION(atest_usb53),
	MSM_PIN_FUNCTION(audio_ext),
	MSM_PIN_FUNCTION(audio_ref),
	MSM_PIN_FUNCTION(cam_aon),
	MSM_PIN_FUNCTION(cam_mclk),
	MSM_PIN_FUNCTION(cci_async),
	MSM_PIN_FUNCTION(cci_i2c),
	MSM_PIN_FUNCTION(cci_timer0),
	MSM_PIN_FUNCTION(cci_timer1),
	MSM_PIN_FUNCTION(cci_timer2),
	MSM_PIN_FUNCTION(cci_timer3),
	MSM_PIN_FUNCTION(cci_timer4),
	MSM_PIN_FUNCTION(cmu_rng0),
	MSM_PIN_FUNCTION(cmu_rng1),
	MSM_PIN_FUNCTION(cmu_rng2),
	MSM_PIN_FUNCTION(cmu_rng3),
	MSM_PIN_FUNCTION(cri_trng),
	MSM_PIN_FUNCTION(dbg_out),
	MSM_PIN_FUNCTION(ddr_bist),
	MSM_PIN_FUNCTION(ddr_pxi0),
	MSM_PIN_FUNCTION(ddr_pxi1),
	MSM_PIN_FUNCTION(ddr_pxi2),
	MSM_PIN_FUNCTION(ddr_pxi3),
	MSM_PIN_FUNCTION(ddr_pxi4),
	MSM_PIN_FUNCTION(ddr_pxi5),
	MSM_PIN_FUNCTION(ddr_pxi6),
	MSM_PIN_FUNCTION(ddr_pxi7),
	MSM_PIN_FUNCTION(edp0_hot),
	MSM_PIN_FUNCTION(edp0_lcd),
	MSM_PIN_FUNCTION(edp1_hot),
	MSM_PIN_FUNCTION(edp1_lcd),
	MSM_PIN_FUNCTION(eusb0_ac),
	MSM_PIN_FUNCTION(eusb1_ac),
	MSM_PIN_FUNCTION(eusb2_ac),
	MSM_PIN_FUNCTION(eusb3_ac),
	MSM_PIN_FUNCTION(eusb5_ac),
	MSM_PIN_FUNCTION(eusb6_ac),
	MSM_PIN_FUNCTION(gcc_gp1),
	MSM_PIN_FUNCTION(gcc_gp2),
	MSM_PIN_FUNCTION(gcc_gp3),
	MSM_PIN_FUNCTION(i2s0_data0),
	MSM_PIN_FUNCTION(i2s0_data1),
	MSM_PIN_FUNCTION(i2s0_sck),
	MSM_PIN_FUNCTION(i2s0_ws),
	MSM_PIN_FUNCTION(i2s1_data0),
	MSM_PIN_FUNCTION(i2s1_data1),
	MSM_PIN_FUNCTION(i2s1_sck),
	MSM_PIN_FUNCTION(i2s1_ws),
	MSM_PIN_FUNCTION(ibi_i3c),
	MSM_PIN_FUNCTION(jitter_bist),
	MSM_PIN_FUNCTION(mdp_vsync0),
	MSM_PIN_FUNCTION(mdp_vsync1),
	MSM_PIN_FUNCTION(mdp_vsync2),
	MSM_PIN_FUNCTION(mdp_vsync3),
	MSM_PIN_FUNCTION(mdp_vsync4),
	MSM_PIN_FUNCTION(mdp_vsync5),
	MSM_PIN_FUNCTION(mdp_vsync6),
	MSM_PIN_FUNCTION(mdp_vsync7),
	MSM_PIN_FUNCTION(mdp_vsync8),
	MSM_PIN_FUNCTION(pcie3_clk),
	MSM_PIN_FUNCTION(pcie4_clk),
	MSM_PIN_FUNCTION(pcie5_clk),
	MSM_PIN_FUNCTION(pcie6a_clk),
	MSM_PIN_FUNCTION(pcie6b_clk),
	MSM_PIN_FUNCTION(phase_flag0),
	MSM_PIN_FUNCTION(phase_flag1),
	MSM_PIN_FUNCTION(phase_flag10),
	MSM_PIN_FUNCTION(phase_flag11),
	MSM_PIN_FUNCTION(phase_flag12),
	MSM_PIN_FUNCTION(phase_flag13),
	MSM_PIN_FUNCTION(phase_flag14),
	MSM_PIN_FUNCTION(phase_flag15),
	MSM_PIN_FUNCTION(phase_flag16),
	MSM_PIN_FUNCTION(phase_flag17),
	MSM_PIN_FUNCTION(phase_flag18),
	MSM_PIN_FUNCTION(phase_flag19),
	MSM_PIN_FUNCTION(phase_flag2),
	MSM_PIN_FUNCTION(phase_flag20),
	MSM_PIN_FUNCTION(phase_flag21),
	MSM_PIN_FUNCTION(phase_flag22),
	MSM_PIN_FUNCTION(phase_flag23),
	MSM_PIN_FUNCTION(phase_flag24),
	MSM_PIN_FUNCTION(phase_flag25),
	MSM_PIN_FUNCTION(phase_flag26),
	MSM_PIN_FUNCTION(phase_flag27),
	MSM_PIN_FUNCTION(phase_flag28),
	MSM_PIN_FUNCTION(phase_flag29),
	MSM_PIN_FUNCTION(phase_flag3),
	MSM_PIN_FUNCTION(phase_flag30),
	MSM_PIN_FUNCTION(phase_flag31),
	MSM_PIN_FUNCTION(phase_flag4),
	MSM_PIN_FUNCTION(phase_flag5),
	MSM_PIN_FUNCTION(phase_flag6),
	MSM_PIN_FUNCTION(phase_flag7),
	MSM_PIN_FUNCTION(phase_flag8),
	MSM_PIN_FUNCTION(phase_flag9),
	MSM_PIN_FUNCTION(pll_bist),
	MSM_PIN_FUNCTION(pll_clk),
	MSM_PIN_FUNCTION(prng_rosc0),
	MSM_PIN_FUNCTION(prng_rosc1),
	MSM_PIN_FUNCTION(prng_rosc2),
	MSM_PIN_FUNCTION(prng_rosc3),
	MSM_PIN_FUNCTION(qdss_cti),
	MSM_PIN_FUNCTION(qdss_gpio),
	MSM_PIN_FUNCTION(qdss_gpio0),
	MSM_PIN_FUNCTION(qdss_gpio1),
	MSM_PIN_FUNCTION(qdss_gpio10),
	MSM_PIN_FUNCTION(qdss_gpio11),
	MSM_PIN_FUNCTION(qdss_gpio12),
	MSM_PIN_FUNCTION(qdss_gpio13),
	MSM_PIN_FUNCTION(qdss_gpio14),
	MSM_PIN_FUNCTION(qdss_gpio15),
	MSM_PIN_FUNCTION(qdss_gpio2),
	MSM_PIN_FUNCTION(qdss_gpio3),
	MSM_PIN_FUNCTION(qdss_gpio4),
	MSM_PIN_FUNCTION(qdss_gpio5),
	MSM_PIN_FUNCTION(qdss_gpio6),
	MSM_PIN_FUNCTION(qdss_gpio7),
	MSM_PIN_FUNCTION(qdss_gpio8),
	MSM_PIN_FUNCTION(qdss_gpio9),
	MSM_PIN_FUNCTION(qspi00),
	MSM_PIN_FUNCTION(qspi01),
	MSM_PIN_FUNCTION(qspi02),
	MSM_PIN_FUNCTION(qspi03),
	MSM_PIN_FUNCTION(qspi0_clk),
	MSM_PIN_FUNCTION(qspi0_cs0),
	MSM_PIN_FUNCTION(qspi0_cs1),
	MSM_PIN_FUNCTION(qup0_se0),
	MSM_PIN_FUNCTION(qup0_se1),
	MSM_PIN_FUNCTION(qup0_se2),
	MSM_PIN_FUNCTION(qup0_se3),
	MSM_PIN_FUNCTION(qup0_se4),
	MSM_PIN_FUNCTION(qup0_se5),
	MSM_PIN_FUNCTION(qup0_se6),
	MSM_PIN_FUNCTION(qup0_se7),
	MSM_PIN_FUNCTION(qup1_se0),
	MSM_PIN_FUNCTION(qup1_se1),
	MSM_PIN_FUNCTION(qup1_se2),
	MSM_PIN_FUNCTION(qup1_se3),
	MSM_PIN_FUNCTION(qup1_se4),
	MSM_PIN_FUNCTION(qup1_se5),
	MSM_PIN_FUNCTION(qup1_se6),
	MSM_PIN_FUNCTION(qup1_se7),
	MSM_PIN_FUNCTION(qup2_se0),
	MSM_PIN_FUNCTION(qup2_se1),
	MSM_PIN_FUNCTION(qup2_se2),
	MSM_PIN_FUNCTION(qup2_se3),
	MSM_PIN_FUNCTION(qup2_se4),
	MSM_PIN_FUNCTION(qup2_se5),
	MSM_PIN_FUNCTION(qup2_se6),
	MSM_PIN_FUNCTION(qup2_se7),
	MSM_PIN_FUNCTION(sd_write),
	MSM_PIN_FUNCTION(sdc4_clk),
	MSM_PIN_FUNCTION(sdc4_cmd),
	MSM_PIN_FUNCTION(sdc4_data0),
	MSM_PIN_FUNCTION(sdc4_data1),
	MSM_PIN_FUNCTION(sdc4_data2),
	MSM_PIN_FUNCTION(sdc4_data3),
	MSM_PIN_FUNCTION(sys_throttle),
	MSM_PIN_FUNCTION(tb_trig),
	MSM_PIN_FUNCTION(tgu_ch0),
	MSM_PIN_FUNCTION(tgu_ch1),
	MSM_PIN_FUNCTION(tgu_ch2),
	MSM_PIN_FUNCTION(tgu_ch3),
	MSM_PIN_FUNCTION(tgu_ch4),
	MSM_PIN_FUNCTION(tgu_ch5),
	MSM_PIN_FUNCTION(tgu_ch6),
	MSM_PIN_FUNCTION(tgu_ch7),
	MSM_PIN_FUNCTION(tmess_prng0),
	MSM_PIN_FUNCTION(tmess_prng1),
	MSM_PIN_FUNCTION(tmess_prng2),
	MSM_PIN_FUNCTION(tmess_prng3),
	MSM_PIN_FUNCTION(tsense_pwm1),
	MSM_PIN_FUNCTION(tsense_pwm2),
	MSM_PIN_FUNCTION(tsense_pwm3),
	MSM_PIN_FUNCTION(tsense_pwm4),
	MSM_PIN_FUNCTION(usb0_dp),
	MSM_PIN_FUNCTION(usb0_phy),
	MSM_PIN_FUNCTION(usb0_sbrx),
	MSM_PIN_FUNCTION(usb0_sbtx),
	MSM_PIN_FUNCTION(usb1_dp),
	MSM_PIN_FUNCTION(usb1_phy),
	MSM_PIN_FUNCTION(usb1_sbrx),
	MSM_PIN_FUNCTION(usb1_sbtx),
	MSM_PIN_FUNCTION(usb2_dp),
	MSM_PIN_FUNCTION(usb2_phy),
	MSM_PIN_FUNCTION(usb2_sbrx),
	MSM_PIN_FUNCTION(usb2_sbtx),
	MSM_PIN_FUNCTION(vsense_trigger),
};

/*
 * Every pin is maintained as a single group, and missing or non-existing pin
 * would be maintained as dummy group to synchronize pin group index with
 * pin descriptor registered with pinctrl core.
 * Clients would not be able to request these dummy pin groups.
 */
static const struct msm_pingroup x1e80100_groups[] = {
	[0] = PINGROUP(0, qup0_se0, ibi_i3c, NA, NA, NA, NA, NA, NA, NA, 0xEE010, 3),
	[1] = PINGROUP(1, qup0_se0, ibi_i3c, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[2] = PINGROUP(2, qup0_se0, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE010, 4),
	[3] = PINGROUP(3, qup0_se0, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE010, 5),
	[4] = PINGROUP(4, qup0_se1, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[5] = PINGROUP(5, qup0_se1, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[6] = PINGROUP(6, qup0_se1, phase_flag0, NA, NA, NA, NA, NA, NA, NA, 0xEE010, 6),
	[7] = PINGROUP(7, qup0_se1, phase_flag1, NA, NA, NA, NA, NA, NA, NA, 0xEE010, 7),
	[8] = PINGROUP(8, qup0_se2, phase_flag2, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[9] = PINGROUP(9, qup0_se2, NA, atest_usb00, ddr_pxi0, NA, NA, NA, NA,
		       NA, 0, -1),
	[10] = PINGROUP(10, qup0_se2, NA, atest_usb01, ddr_pxi1, NA, NA, NA,
			NA, NA, 0, -1),
	[11] = PINGROUP(11, qup0_se2, phase_flag5, NA, NA, NA, NA, NA, NA, NA, 0xEE010, 8),
	[12] = PINGROUP(12, qup0_se3, qup0_se7, phase_flag6, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[13] = PINGROUP(13, qup0_se3, qup0_se7, phase_flag7, NA, NA, NA, NA,
			NA, NA, 0xEE010, 9),
	[14] = PINGROUP(14, qup0_se3, qup0_se7, phase_flag8, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[15] = PINGROUP(15, qup0_se3, qup0_se7, phase_flag9, NA, NA, NA, NA,
			NA, NA, 0xEE010, 10),
	[16] = PINGROUP(16, qup0_se4, phase_flag10, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[17] = PINGROUP(17, qup0_se4, qup0_se2, phase_flag11, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[18] = PINGROUP(18, qup0_se4, qup0_se2, phase_flag12, NA, qdss_cti, NA,
			NA, NA, NA, 0xEE010, 11),
	[19] = PINGROUP(19, qup0_se4, qup0_se2, phase_flag13, NA, qdss_cti, NA,
			NA, NA, NA, 0xEE010, 12),
	[20] = PINGROUP(20, qup0_se5, NA, phase_flag14, NA, NA, NA, NA, NA, NA, 0, -1),
	[21] = PINGROUP(21, qup0_se5, qup0_se3, NA, phase_flag15, NA, NA, NA,
			NA, NA, 0xEE010, 13),
	[22] = PINGROUP(22, qup0_se5, qup0_se3, NA, phase_flag16, NA, NA, NA,
			NA, NA, 0, -1),
	[23] = PINGROUP(23, qup0_se5, qup0_se3, phase_flag17, NA, qdss_cti, NA,
			NA, NA, NA, 0xEE010, 14),
	[24] = PINGROUP(24, qup0_se6, phase_flag18, NA, NA, NA, NA, NA, NA, NA, 0xEE010, 15),
	[25] = PINGROUP(25, qup0_se6, phase_flag19, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[26] = PINGROUP(26, qup0_se6, phase_flag20, NA, NA, NA, NA, NA, NA, NA, 0xEE014, 0),
	[27] = PINGROUP(27, qup0_se6, phase_flag21, NA, qdss_cti, NA, NA, NA,
			NA, NA, 0xEE014, 1),
	[28] = PINGROUP(28, pll_bist, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE014, 2),
	[29] = PINGROUP(29, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE014, 3),
	[30] = PINGROUP(30, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE014, 4),
	[31] = PINGROUP(31, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE014, 5),
	[32] = PINGROUP(32, qup1_se0, ibi_i3c, NA, NA, NA, NA, NA, NA, NA, 0xEE000, 0),
	[33] = PINGROUP(33, qup1_se0, ibi_i3c, qup1_se3, NA, NA, NA, NA, NA,
			NA, 0xEE000, 1),
	[34] = PINGROUP(34, qup1_se0, qup1_se3, tsense_pwm1, tsense_pwm2,
			tsense_pwm3, tsense_pwm4, NA, NA, NA, 0xEE000, 2),
	[35] = PINGROUP(35, qup1_se0, qup1_se3, pll_clk, atest_usb31, NA, NA,
			NA, NA, NA, 0xEE000, 3),
	[36] = PINGROUP(36, qup1_se1, ibi_i3c, NA, NA, NA, NA, NA, NA, NA, 0xEE000, 4),
	[37] = PINGROUP(37, qup1_se1, ibi_i3c, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[38] = PINGROUP(38, qup1_se1, vsense_trigger, atest_usb0, ddr_pxi0, NA,
			NA, NA, NA, NA, 0xEE000, 5),
	[39] = PINGROUP(39, qup1_se1, sys_throttle, phase_flag3, NA, NA, NA,
			NA, NA, NA, 0xEE000, 6),
	[40] = PINGROUP(40, qup1_se2, phase_flag4, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[41] = PINGROUP(41, qup1_se2, atest_usb02, ddr_pxi1, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[42] = PINGROUP(42, qup1_se2, jitter_bist, atest_usb03, ddr_pxi2, NA,
			NA, NA, NA, NA, 0, -1),
	[43] = PINGROUP(43, qup1_se2, NA, atest_usb1, ddr_pxi2, NA, NA, NA, NA,
			NA, 0xEE000, 7),
	[44] = PINGROUP(44, qup1_se3, NA, atest_usb10, ddr_pxi3, NA, NA, NA,
			NA, NA, 0, -1),
	[45] = PINGROUP(45, qup1_se3, cmu_rng3, NA, atest_usb11, ddr_pxi3, NA,
			NA, NA, NA, 0, -1),
	[46] = PINGROUP(46, qup1_se3, cmu_rng2, NA, atest_usb12, ddr_pxi4, NA,
			NA, NA, NA, 0, -1),
	[47] = PINGROUP(47, qup1_se3, cmu_rng1, NA, atest_usb13, ddr_pxi4, NA,
			NA, NA, NA, 0xEE000, 8),
	[48] = PINGROUP(48, qup1_se4, cmu_rng0, NA, atest_usb2, ddr_pxi5, NA,
			NA, NA, NA, 0, -1),
	[49] = PINGROUP(49, qup1_se4, qup1_se2, NA, atest_usb20, ddr_pxi5, NA,
			NA, NA, NA, 0, -1),
	[50] = PINGROUP(50, qup1_se4, qup1_se2, NA, atest_usb21, ddr_pxi6, NA,
			NA, NA, NA, 0, -1),
	[51] = PINGROUP(51, qup1_se4, qup1_se2, dbg_out, atest_usb22, ddr_pxi6,
			NA, NA, NA, NA, 0xEE000, 9),
	[52] = PINGROUP(52, qup1_se5, qup1_se7, atest_usb23, ddr_pxi7, NA, NA,
			NA, NA, NA, 0, -1),
	[53] = PINGROUP(53, qup1_se5, qup1_se7, NA, atest_usb3, ddr_pxi7, NA,
			NA, NA, NA, 0xEE000, 10),
	[54] = PINGROUP(54, qup1_se5, qup1_se7, ddr_bist, atest_usb30, NA, NA,
			NA, NA, NA, 0, -1),
	[55] = PINGROUP(55, qup1_se5, qup1_se7, ddr_bist, NA, NA, NA, NA, NA,
			NA, 0xEE000, 11),
	[56] = PINGROUP(56, qup1_se6, ddr_bist, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[57] = PINGROUP(57, qup1_se6, ddr_bist, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[58] = PINGROUP(58, qup1_se6, atest_usb32, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[59] = PINGROUP(59, qup1_se6, atest_usb33, NA, NA, NA, NA, NA, NA, NA, 0xEE000, 12),
	[60] = PINGROUP(60, aoss_cti, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[61] = PINGROUP(61, aoss_cti, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[62] = PINGROUP(62, aoss_cti, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[63] = PINGROUP(63, aoss_cti, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[64] = PINGROUP(64, qup2_se0, gcc_gp2, NA, NA, NA, NA, NA, NA, NA, 0xEE000, 13),
	[65] = PINGROUP(65, qup2_se0, qup2_se3, tgu_ch1, atest_usb40, NA, NA,
			NA, NA, NA, 0xEE000, 14),
	[66] = PINGROUP(66, qup2_se0, qup2_se3, tgu_ch2, atest_usb41, NA, NA,
			NA, NA, NA, 0xEE000, 15),
	[67] = PINGROUP(67, qup2_se0, qup2_se3, tgu_ch3, atest_usb42, NA, NA,
			NA, NA, NA, 0xEE004, 0),
	[68] = PINGROUP(68, qup2_se1, ibi_i3c, tgu_ch4, NA, NA, NA, NA, NA, NA, 0xEE004, 1),
	[69] = PINGROUP(69, qup2_se1, ibi_i3c, tgu_ch5, NA, NA, NA, NA, NA, NA, 0, -1),
	[70] = PINGROUP(70, qup2_se1, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[71] = PINGROUP(71, qup2_se1, gcc_gp1, NA, NA, NA, NA, NA, NA, NA, 0xEE004, 2),
	[72] = PINGROUP(72, qup2_se2, gcc_gp1, atest_usb50, NA, NA, NA, NA, NA,
			NA, 0, -1),
	[73] = PINGROUP(73, qup2_se2, gcc_gp2, atest_usb51, NA, NA, NA, NA, NA,
			NA, 0, -1),
	[74] = PINGROUP(74, qup2_se2, gcc_gp3, atest_usb52, NA, NA, NA, NA, NA,
			NA, 0, -1),
	[75] = PINGROUP(75, qup2_se2, atest_usb53, NA, NA, NA, NA, NA, NA, NA, 0xEE004, 3),
	[76] = PINGROUP(76, qup2_se3, phase_flag29, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[77] = PINGROUP(77, qup2_se3, phase_flag30, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[78] = PINGROUP(78, qup2_se3, phase_flag31, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[79] = PINGROUP(79, qup2_se3, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE004, 4),
	[80] = PINGROUP(80, qup2_se4, tgu_ch7, atest_usb5, NA, NA, NA, NA, NA,
			NA, 0xEE004, 5),
	[81] = PINGROUP(81, qup2_se4, qup2_se2, tgu_ch0, atest_usb4, NA, NA,
			NA, NA, NA, 0xEE004, 6),
	[82] = PINGROUP(82, qup2_se4, qup2_se2, gcc_gp3, NA, NA, NA, NA, NA,
			NA, 0, -1),
	[83] = PINGROUP(83, qup2_se4, qup2_se2, tgu_ch6, atest_usb43, NA, NA,
			NA, NA, NA, 0xEE004, 7),
	[84] = PINGROUP(84, qup2_se5, qup2_se7, NA, NA, NA, NA, NA, NA, NA, 0xEE004, 8),
	[85] = PINGROUP(85, qup2_se5, qup2_se7, NA, NA, NA, NA, NA, NA, NA, 0xEE004, 9),
	[86] = PINGROUP(86, qup2_se5, qup2_se7, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[87] = PINGROUP(87, qup2_se5, qup2_se7, NA, NA, NA, NA, NA, NA, NA, 0xEE004, 10),
	[88] = PINGROUP(88, qup2_se6, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[89] = PINGROUP(89, qup2_se6, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[90] = PINGROUP(90, qup2_se6, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[91] = PINGROUP(91, qup2_se6, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE004, 11),
	[92] = PINGROUP(92, tmess_prng0, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE014, 6),
	[93] = PINGROUP(93, tmess_prng1, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE014, 7),
	[94] = PINGROUP(94, sys_throttle, tmess_prng2, NA, NA, NA, NA, NA, NA,
			NA, 0xEE014, 8),
	[95] = PINGROUP(95, tmess_prng3, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE014, 9),
	[96] = PINGROUP(96, cam_mclk, qdss_gpio0, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[97] = PINGROUP(97, cam_mclk, qdss_gpio1, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[98] = PINGROUP(98, cam_mclk, qdss_gpio2, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[99] = PINGROUP(99, cam_mclk, qdss_gpio3, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[100] = PINGROUP(100, cam_aon, qdss_gpio4, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[101] = PINGROUP(101, cci_i2c, qdss_gpio5, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[102] = PINGROUP(102, cci_i2c, qdss_gpio6, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[103] = PINGROUP(103, cci_i2c, qdss_gpio7, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[104] = PINGROUP(104, cci_i2c, qdss_gpio, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[105] = PINGROUP(105, cci_i2c, qdss_gpio, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[106] = PINGROUP(106, cci_i2c, qdss_gpio8, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[107] = PINGROUP(107, qdss_gpio9, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[108] = PINGROUP(108, qdss_gpio10, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[109] = PINGROUP(109, cci_timer0, mdp_vsync4, qdss_gpio11, NA, NA, NA,
			 NA, NA, NA, 0, -1),
	[110] = PINGROUP(110, cci_timer1, mdp_vsync5, qdss_gpio12, NA, NA, NA,
			 NA, NA, NA, 0, -1),
	[111] = PINGROUP(111, cci_timer2, cci_async, mdp_vsync6, qdss_gpio13,
			 NA, NA, NA, NA, NA, 0, -1),
	[112] = PINGROUP(112, cci_timer3, cci_async, mdp_vsync7, qdss_gpio14,
			 NA, NA, NA, NA, NA, 0, -1),
	[113] = PINGROUP(113, cci_timer4, cci_async, mdp_vsync8, qdss_gpio15,
			 NA, NA, NA, NA, NA, 0xEE014, 10),
	[114] = PINGROUP(114, mdp_vsync0, mdp_vsync1, NA, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[115] = PINGROUP(115, mdp_vsync3, mdp_vsync2, edp1_lcd, NA, NA, NA, NA,
			 NA, NA, 0, -1),
	[116] = PINGROUP(116, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[117] = PINGROUP(117, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[118] = PINGROUP(118, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[119] = PINGROUP(119, edp0_hot, edp1_lcd, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[120] = PINGROUP(120, edp1_hot, edp0_lcd, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[121] = PINGROUP(121, usb0_phy, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE014, 11),
	[122] = PINGROUP(122, usb0_dp, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[123] = PINGROUP(123, usb1_phy, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE014, 12),
	[124] = PINGROUP(124, usb1_dp, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[125] = PINGROUP(125, usb2_phy, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE014, 13),
	[126] = PINGROUP(126, usb2_dp, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[127] = PINGROUP(127, qspi0_clk, sdc4_clk, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[128] = PINGROUP(128, qspi00, sdc4_data0, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[129] = PINGROUP(129, qspi01, sdc4_data1, NA, NA, NA, NA, NA, NA, NA, 0xEE008, 0),
	[130] = PINGROUP(130, qspi02, sdc4_data2, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[131] = PINGROUP(131, qspi03, sdc4_data3, NA, NA, NA, NA, NA, NA, NA, 0xEE008, 1),
	[132] = PINGROUP(132, qspi0_cs0, sdc4_cmd, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[133] = PINGROUP(133, qspi0_cs1, tb_trig, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[134] = PINGROUP(134, audio_ext, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE008, 2),
	[135] = PINGROUP(135, i2s0_sck, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[136] = PINGROUP(136, i2s0_data0, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[137] = PINGROUP(137, i2s0_data1, tb_trig, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[138] = PINGROUP(138, i2s0_ws, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[139] = PINGROUP(139, i2s1_sck, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[140] = PINGROUP(140, i2s1_data0, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[141] = PINGROUP(141, i2s1_ws, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE008, 3),
	[142] = PINGROUP(142, i2s1_data1, audio_ext, audio_ref, NA, NA, NA, NA,
			 NA, NA, 0, -1),
	[143] = PINGROUP(143, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[144] = PINGROUP(144, pcie3_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE008, 4),
	[145] = PINGROUP(145, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE008, 5),
	[146] = PINGROUP(146, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[147] = PINGROUP(147, pcie4_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE008, 6),
	[148] = PINGROUP(148, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE008, 7),
	[149] = PINGROUP(149, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[150] = PINGROUP(150, pcie5_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE008, 8),
	[151] = PINGROUP(151, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE008, 9),
	[152] = PINGROUP(152, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[153] = PINGROUP(153, pcie6a_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE014, 14),
	[154] = PINGROUP(154, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE014, 15),
	[155] = PINGROUP(155, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[156] = PINGROUP(156, pcie6b_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE018, 0),
	[157] = PINGROUP(157, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE018, 1),
	[158] = PINGROUP(158, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[159] = PINGROUP(159, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[160] = PINGROUP(160, RESOUT_GPIO, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[161] = PINGROUP(161, qdss_cti, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[162] = PINGROUP(162, sd_write, qdss_cti, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[163] = PINGROUP(163, usb0_sbrx, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE018, 2),
	[164] = PINGROUP(164, usb0_sbtx, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[165] = PINGROUP(165, usb0_sbtx, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[166] = PINGROUP(166, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE018, 3),
	[167] = PINGROUP(167, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[168] = PINGROUP(168, eusb0_ac, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[169] = PINGROUP(169, eusb3_ac, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[170] = PINGROUP(170, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[171] = PINGROUP(171, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[172] = PINGROUP(172, usb1_sbrx, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE018, 4),
	[173] = PINGROUP(173, usb1_sbtx, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[174] = PINGROUP(174, usb1_sbtx, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[175] = PINGROUP(175, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE018, 5),
	[176] = PINGROUP(176, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[177] = PINGROUP(177, eusb1_ac, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[178] = PINGROUP(178, eusb6_ac, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[179] = PINGROUP(179, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[180] = PINGROUP(180, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[181] = PINGROUP(181, usb2_sbrx, prng_rosc3, phase_flag22, NA,
			 atest_char, NA, NA, NA, NA, 0xEE018, 6),
	[182] = PINGROUP(182, usb2_sbtx, prng_rosc2, phase_flag23, NA,
			 atest_char3, NA, NA, NA, NA, 0, -1),
	[183] = PINGROUP(183, usb2_sbtx, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[184] = PINGROUP(184, phase_flag25, NA, atest_char1, NA, NA, NA, NA,
			 NA, NA, 0xEE018, 7),
	[185] = PINGROUP(185, phase_flag26, NA, atest_char0, NA, NA, NA, NA,
			 NA, NA, 0, -1),
	[186] = PINGROUP(186, eusb2_ac, prng_rosc0, phase_flag27, NA, NA, NA,
			 NA, NA, NA, 0, -1),
	[187] = PINGROUP(187, eusb5_ac, cri_trng, phase_flag28, NA, NA, NA, NA,
			 NA, NA, 0, -1),
	[188] = PINGROUP(188, prng_rosc1, phase_flag24, NA, atest_char2, NA,
			 NA, NA, NA, NA, 0, -1),
	[189] = PINGROUP(189, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[190] = PINGROUP(190, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[191] = PINGROUP(191, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[192] = PINGROUP(192, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[193] = PINGROUP(193, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE008, 10),
	[194] = PINGROUP(194, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[195] = PINGROUP(195, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[196] = PINGROUP(196, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE008, 11),
	[197] = PINGROUP(197, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[198] = PINGROUP(198, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[199] = PINGROUP(199, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[200] = PINGROUP(200, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[201] = PINGROUP(201, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[202] = PINGROUP(202, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[203] = PINGROUP(203, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE008, 12),
	[204] = PINGROUP(204, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[205] = PINGROUP(205, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[206] = PINGROUP(206, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[207] = PINGROUP(207, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[208] = PINGROUP(208, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE008, 13),
	[209] = PINGROUP(209, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[210] = PINGROUP(210, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[211] = PINGROUP(211, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[212] = PINGROUP(212, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE008, 14),
	[213] = PINGROUP(213, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE008, 15),
	[214] = PINGROUP(214, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE00C, 0),
	[215] = PINGROUP(215, NA, qdss_cti, NA, NA, NA, NA, NA, NA, NA, 0xEE00C, 1),
	[216] = PINGROUP(216, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[217] = PINGROUP(217, NA, qdss_cti, NA, NA, NA, NA, NA, NA, NA, 0xEE00C, 2),
	[218] = PINGROUP(218, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[219] = PINGROUP(219, NA, qdss_gpio0, NA, NA, NA, NA, NA, NA, NA, 0xEE00C, 3),
	[220] = PINGROUP(220, NA, qdss_gpio1, NA, NA, NA, NA, NA, NA, NA, 0xEE00C, 4),
	[221] = PINGROUP(221, NA, qdss_gpio2, NA, NA, NA, NA, NA, NA, NA, 0xEE00C, 5),
	[222] = PINGROUP(222, NA, qdss_gpio3, NA, NA, NA, NA, NA, NA, NA, 0xEE00C, 6),
	[223] = PINGROUP(223, NA, qdss_gpio4, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[224] = PINGROUP(224, NA, qdss_gpio5, NA, NA, NA, NA, NA, NA, NA, 0xEE00C, 7),
	[225] = PINGROUP(225, NA, qdss_gpio6, NA, NA, NA, NA, NA, NA, NA, 0xEE00C, 8),
	[226] = PINGROUP(226, NA, qdss_gpio7, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[227] = PINGROUP(227, NA, qdss_gpio, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[228] = PINGROUP(228, NA, qdss_gpio, NA, NA, NA, NA, NA, NA, NA, 0xEE00C, 9),
	[229] = PINGROUP(229, qdss_gpio8, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[230] = PINGROUP(230, qdss_gpio9, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE00C, 10),
	[231] = PINGROUP(231, qdss_gpio10, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[232] = PINGROUP(232, qdss_gpio11, NA, NA, NA, NA, NA, NA, NA, NA, 0xEE00C, 11),
	[233] = PINGROUP(233, qdss_gpio12, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[234] = PINGROUP(234, qdss_gpio13, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[235] = PINGROUP(235, aon_cci, qdss_gpio14, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[236] = PINGROUP(236, aon_cci, qdss_gpio15, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[237] = PINGROUP(237, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[238] = UFS_RESET(ufs_reset, 0x1f9000),
	[239] = SDC_QDSD_PINGROUP(sdc2_clk, 0x1f2000, 14, 6),
	[240] = SDC_QDSD_PINGROUP(sdc2_cmd, 0x1f2000, 11, 3),
	[241] = SDC_QDSD_PINGROUP(sdc2_data, 0x1f2000, 9, 0),
};

static const struct msm_gpio_wakeirq_map x1e80100_pdc_map[] = {
	{ 0, 72 }, { 2, 70 }, { 3, 71 }, { 6, 123 }, { 7, 67 }, { 11, 85 },
	{ 15, 68 }, { 18, 122 }, { 19, 69 }, { 21, 158 }, { 23, 143 }, { 26, 129 },
	{ 27, 144 }, { 28, 77 }, { 29, 78 }, { 30, 92 }, { 32, 145 }, { 33, 115 },
	{ 34, 130 }, { 35, 146 }, { 36, 147 }, { 39, 80 }, { 43, 148 }, { 47, 149 },
	{ 51, 79 }, { 53, 89 }, { 59, 87 }, { 64, 90 }, { 65, 106 }, { 66, 142 },
	{ 67, 88 }, { 71, 91 }, { 75, 152 }, { 79, 153 }, { 80, 125 }, { 81, 128 },
	{ 84, 137 }, { 85, 155 }, { 87, 156 }, { 91, 157 }, { 92, 138 }, { 94, 140 },
	{ 95, 141 }, { 113, 84 }, { 121, 73 }, { 123, 74 }, { 129, 76 }, { 131, 82 },
	{ 134, 83 }, { 141, 93 }, { 144, 94 }, { 147, 96 }, { 148, 97 }, { 150, 102 },
	{ 151, 103 }, { 153, 104 }, { 156, 105 }, { 157, 107 }, { 163, 98 }, { 166, 112 },
	{ 172, 99 }, { 181, 101 }, { 184, 116 }, { 193, 40 }, { 193, 117 }, { 196, 108 },
	{ 203, 133 }, { 212, 120 }, { 213, 150 }, { 214, 121 }, { 215, 118 }, { 217, 109 },
	{ 220, 110 }, { 221, 111 }, { 222, 124 }, { 224, 131 }, { 225, 132 },
};

static const struct msm_pinctrl_soc_data x1e80100_pinctrl = {
	.pins = x1e80100_pins,
	.npins = ARRAY_SIZE(x1e80100_pins),
	.functions = x1e80100_functions,
	.nfunctions = ARRAY_SIZE(x1e80100_functions),
	.groups = x1e80100_groups,
	.ngroups = ARRAY_SIZE(x1e80100_groups),
	.ngpios = 239,
	.wakeirq_map = x1e80100_pdc_map,
	.nwakeirq_map = ARRAY_SIZE(x1e80100_pdc_map),
	.egpio_func = 9,
};

static int x1e80100_pinctrl_probe(struct platform_device *pdev)
{
	return msm_pinctrl_probe(pdev, &x1e80100_pinctrl);
}

static const struct of_device_id x1e80100_pinctrl_of_match[] = {
	{ .compatible = "qcom,x1e80100-tlmm", },
	{ },
};

static struct platform_driver x1e80100_pinctrl_driver = {
	.driver = {
		.name = "x1e80100-tlmm",
		.of_match_table = x1e80100_pinctrl_of_match,
	},
	.probe = x1e80100_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init x1e80100_pinctrl_init(void)
{
	return platform_driver_register(&x1e80100_pinctrl_driver);
}
arch_initcall(x1e80100_pinctrl_init);

static void __exit x1e80100_pinctrl_exit(void)
{
	platform_driver_unregister(&x1e80100_pinctrl_driver);
}
module_exit(x1e80100_pinctrl_exit);

MODULE_DESCRIPTION("QTI X1E80100 TLMM pinctrl driver");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(of, x1e80100_pinctrl_of_match);
