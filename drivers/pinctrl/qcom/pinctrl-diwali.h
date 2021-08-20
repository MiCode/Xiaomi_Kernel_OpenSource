/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

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


#define QUP_I3C_0_SE0_MODE_OFFSET	0xBE000
#define QUP_I3C_0_SE1_MODE_OFFSET	0xBF000
#define QUP_I3C_1_SE0_MODE_OFFSET	0xC0000
#define QUP_I3C_1_SE1_MODE_OFFSET	0xC1000

static const struct pinctrl_pin_desc diwali_pins[] = {
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
	PINCTRL_PIN(170, "UFS_RESET"),
	PINCTRL_PIN(171, "SDC2_CLK"),
	PINCTRL_PIN(172, "SDC2_CMD"),
	PINCTRL_PIN(173, "SDC2_DATA"),
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

static const unsigned int sdc2_clk_pins[] = { 171 };
static const unsigned int sdc2_cmd_pins[] = { 172 };
static const unsigned int sdc2_data_pins[] = { 173 };
static const unsigned int ufs_reset_pins[] = { 170 };

enum diwali_functions {
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
	msm_mux_dp0_hot,
	msm_mux_gcc_gp1,
	msm_mux_gcc_gp2,
	msm_mux_gcc_gp3,
	msm_mux_host2wlan_sol,
	msm_mux_ibi_i3c,
	msm_mux_jitter_bist,
	msm_mux_mdp_vsync,
	msm_mux_mdp_vsync0,
	msm_mux_mdp_vsync1,
	msm_mux_mdp_vsync2,
	msm_mux_mdp_vsync3,
	msm_mux_mi2s0_data0,
	msm_mux_mi2s0_data1,
	msm_mux_mi2s0_sck,
	msm_mux_mi2s0_ws,
	msm_mux_mi2s2_data0,
	msm_mux_mi2s2_data1,
	msm_mux_mi2s2_sck,
	msm_mux_mi2s2_ws,
	msm_mux_mi2s_mclk0,
	msm_mux_mi2s_mclk1,
	msm_mux_nav_gpio0,
	msm_mux_nav_gpio1,
	msm_mux_nav_gpio2,
	msm_mux_pcie0_clk,
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
	msm_mux_qlink0_enable,
	msm_mux_qlink0_request,
	msm_mux_qlink0_wmss,
	msm_mux_qlink1_enable,
	msm_mux_qlink1_request,
	msm_mux_qlink1_wmss,
	msm_mux_qlink2_enable,
	msm_mux_qlink2_request,
	msm_mux_qlink2_wmss,
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
	msm_mux_sd_write,
	msm_mux_tb_trig,
	msm_mux_tgu_ch0,
	msm_mux_tgu_ch1,
	msm_mux_tgu_ch2,
	msm_mux_tgu_ch3,
	msm_mux_tmess_prng0,
	msm_mux_tmess_prng1,
	msm_mux_tmess_prng2,
	msm_mux_tmess_prng3,
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
	msm_mux_usb0_hs,
	msm_mux_usb0_phy,
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
	"gpio165", "gpio166", "gpio167", "gpio168", "gpio169",
};
static const char * const atest_char_groups[] = {
	"gpio98",
};
static const char * const atest_char0_groups[] = {
	"gpio100",
};
static const char * const atest_char1_groups[] = {
	"gpio97",
};
static const char * const atest_char2_groups[] = {
	"gpio95",
};
static const char * const atest_char3_groups[] = {
	"gpio94",
};
static const char * const atest_usb0_groups[] = {
	"gpio22",
};
static const char * const atest_usb00_groups[] = {
	"gpio23",
};
static const char * const atest_usb01_groups[] = {
	"gpio24",
};
static const char * const atest_usb02_groups[] = {
	"gpio25",
};
static const char * const atest_usb03_groups[] = {
	"gpio26",
};
static const char * const audio_ref_groups[] = {
	"gpio125",
};
static const char * const cam_mclk_groups[] = {
	"gpio64", "gpio65", "gpio66", "gpio67", "gpio68", "gpio69",
};
static const char * const cci_async_groups[] = {
	"gpio79", "gpio80", "gpio81",
};
static const char * const cci_i2c_groups[] = {
	"gpio70", "gpio71", "gpio72", "gpio73", "gpio74", "gpio75", "gpio76",
	"gpio77",
};
static const char * const cci_timer0_groups[] = {
	"gpio20",
};
static const char * const cci_timer1_groups[] = {
	"gpio21",
};
static const char * const cci_timer2_groups[] = {
	"gpio78",
};
static const char * const cci_timer3_groups[] = {
	"gpio79",
};
static const char * const cci_timer4_groups[] = {
	"gpio80",
};
static const char * const cmu_rng0_groups[] = {
	"gpio11",
};
static const char * const cmu_rng1_groups[] = {
	"gpio10",
};
static const char * const cmu_rng2_groups[] = {
	"gpio9",
};
static const char * const cmu_rng3_groups[] = {
	"gpio8",
};
static const char * const coex_uart1_groups[] = {
	"gpio111", "gpio112",
};
static const char * const cri_trng_groups[] = {
	"gpio77",
};
static const char * const cri_trng0_groups[] = {
	"gpio64",
};
static const char * const cri_trng1_groups[] = {
	"gpio65",
};
static const char * const dbg_out_groups[] = {
	"gpio95",
};
static const char * const ddr_bist_groups[] = {
	"gpio48", "gpio49", "gpio51", "gpio52",
};
static const char * const ddr_pxi0_groups[] = {
	"gpio22", "gpio23",
};
static const char * const ddr_pxi1_groups[] = {
	"gpio24", "gpio25",
};
static const char * const dp0_hot_groups[] = {
	"gpio6",
};
static const char * const gcc_gp1_groups[] = {
	"gpio16", "gpio40",
};
static const char * const gcc_gp2_groups[] = {
	"gpio17", "gpio41",
};
static const char * const gcc_gp3_groups[] = {
	"gpio15", "gpio42",
};
static const char * const host2wlan_sol_groups[] = {
	"gpio124",
};
static const char * const ibi_i3c_groups[] = {
	"gpio0", "gpio1", "gpio4", "gpio5", "gpio32", "gpio33", "gpio36",
	"gpio37",
};
static const char * const jitter_bist_groups[] = {
	"gpio94",
};
static const char * const mdp_vsync_groups[] = {
	"gpio45", "gpio48", "gpio49", "gpio81", "gpio82",
};
static const char * const mdp_vsync0_groups[] = {
	"gpio82",
};
static const char * const mdp_vsync1_groups[] = {
	"gpio82",
};
static const char * const mdp_vsync2_groups[] = {
	"gpio81",
};
static const char * const mdp_vsync3_groups[] = {
	"gpio81",
};
static const char * const mi2s0_data0_groups[] = {
	"gpio128",
};
static const char * const mi2s0_data1_groups[] = {
	"gpio129",
};
static const char * const mi2s0_sck_groups[] = {
	"gpio127",
};
static const char * const mi2s0_ws_groups[] = {
	"gpio130",
};
static const char * const mi2s2_data0_groups[] = {
	"gpio61",
};
static const char * const mi2s2_data1_groups[] = {
	"gpio63",
};
static const char * const mi2s2_sck_groups[] = {
	"gpio60",
};
static const char * const mi2s2_ws_groups[] = {
	"gpio62",
};
static const char * const mi2s_mclk0_groups[] = {
	"gpio126",
};
static const char * const mi2s_mclk1_groups[] = {
	"gpio125",
};
static const char * const nav_gpio0_groups[] = {
	"gpio113",
};
static const char * const nav_gpio1_groups[] = {
	"gpio114",
};
static const char * const nav_gpio2_groups[] = {
	"gpio115",
};
static const char * const pcie0_clk_groups[] = {
	"gpio118",
};
static const char * const phase_flag0_groups[] = {
	"gpio59",
};
static const char * const phase_flag1_groups[] = {
	"gpio58",
};
static const char * const phase_flag10_groups[] = {
	"gpio49",
};
static const char * const phase_flag11_groups[] = {
	"gpio48",
};
static const char * const phase_flag12_groups[] = {
	"gpio46",
};
static const char * const phase_flag13_groups[] = {
	"gpio44",
};
static const char * const phase_flag14_groups[] = {
	"gpio43",
};
static const char * const phase_flag15_groups[] = {
	"gpio42",
};
static const char * const phase_flag16_groups[] = {
	"gpio41",
};
static const char * const phase_flag17_groups[] = {
	"gpio40",
};
static const char * const phase_flag18_groups[] = {
	"gpio28",
};
static const char * const phase_flag19_groups[] = {
	"gpio20",
};
static const char * const phase_flag2_groups[] = {
	"gpio57",
};
static const char * const phase_flag20_groups[] = {
	"gpio19",
};
static const char * const phase_flag21_groups[] = {
	"gpio18",
};
static const char * const phase_flag22_groups[] = {
	"gpio17",
};
static const char * const phase_flag23_groups[] = {
	"gpio16",
};
static const char * const phase_flag24_groups[] = {
	"gpio15",
};
static const char * const phase_flag25_groups[] = {
	"gpio14",
};
static const char * const phase_flag26_groups[] = {
	"gpio13",
};
static const char * const phase_flag27_groups[] = {
	"gpio12",
};
static const char * const phase_flag28_groups[] = {
	"gpio11",
};
static const char * const phase_flag29_groups[] = {
	"gpio10",
};
static const char * const phase_flag3_groups[] = {
	"gpio56",
};
static const char * const phase_flag30_groups[] = {
	"gpio9",
};
static const char * const phase_flag31_groups[] = {
	"gpio8",
};
static const char * const phase_flag4_groups[] = {
	"gpio55",
};
static const char * const phase_flag5_groups[] = {
	"gpio54",
};
static const char * const phase_flag6_groups[] = {
	"gpio53",
};
static const char * const phase_flag7_groups[] = {
	"gpio52",
};
static const char * const phase_flag8_groups[] = {
	"gpio51",
};
static const char * const phase_flag9_groups[] = {
	"gpio50",
};
static const char * const pll_bist_groups[] = {
	"gpio98",
};
static const char * const pll_clk_groups[] = {
	"gpio92",
};
static const char * const prng_rosc0_groups[] = {
	"gpio66",
};
static const char * const prng_rosc1_groups[] = {
	"gpio68",
};
static const char * const prng_rosc2_groups[] = {
	"gpio76",
};
static const char * const prng_rosc3_groups[] = {
	"gpio74",
};
static const char * const qdss_cti_groups[] = {
	"gpio38", "gpio39", "gpio52", "gpio53", "gpio127", "gpio128",
	"gpio129", "gpio130",
};
static const char * const qdss_gpio_groups[] = {
	"gpio72", "gpio73", "gpio152", "gpio153",
};
static const char * const qdss_gpio0_groups[] = {
	"gpio64", "gpio137",
};
static const char * const qdss_gpio1_groups[] = {
	"gpio65", "gpio138",
};
static const char * const qdss_gpio10_groups[] = {
	"gpio76", "gpio162",
};
static const char * const qdss_gpio11_groups[] = {
	"gpio77", "gpio163",
};
static const char * const qdss_gpio12_groups[] = {
	"gpio78", "gpio165",
};
static const char * const qdss_gpio13_groups[] = {
	"gpio79", "gpio167",
};
static const char * const qdss_gpio14_groups[] = {
	"gpio80", "gpio169",
};
static const char * const qdss_gpio15_groups[] = {
	"gpio35", "gpio61",
};
static const char * const qdss_gpio2_groups[] = {
	"gpio66", "gpio139",
};
static const char * const qdss_gpio3_groups[] = {
	"gpio67", "gpio140",
};
static const char * const qdss_gpio4_groups[] = {
	"gpio68", "gpio143",
};
static const char * const qdss_gpio5_groups[] = {
	"gpio69", "gpio144",
};
static const char * const qdss_gpio6_groups[] = {
	"gpio70", "gpio148",
};
static const char * const qdss_gpio7_groups[] = {
	"gpio71", "gpio149",
};
static const char * const qdss_gpio8_groups[] = {
	"gpio74", "gpio160",
};
static const char * const qdss_gpio9_groups[] = {
	"gpio75", "gpio161",
};
static const char * const qlink0_enable_groups[] = {
	"gpio93",
};
static const char * const qlink0_request_groups[] = {
	"gpio92",
};
static const char * const qlink0_wmss_groups[] = {
	"gpio94",
};
static const char * const qlink1_enable_groups[] = {
	"gpio96",
};
static const char * const qlink1_request_groups[] = {
	"gpio95",
};
static const char * const qlink1_wmss_groups[] = {
	"gpio97",
};
static const char * const qlink2_enable_groups[] = {
	"gpio99",
};
static const char * const qlink2_request_groups[] = {
	"gpio98",
};
static const char * const qlink2_wmss_groups[] = {
	"gpio100",
};
static const char * const qup0_se0_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3",
};
static const char * const qup0_se1_groups[] = {
	"gpio4", "gpio5", "gpio6", "gpio7",
};
static const char * const qup0_se2_groups[] = {
	"gpio6", "gpio7", "gpio8", "gpio9", "gpio10", "gpio11",
};
static const char * const qup0_se3_groups[] = {
	"gpio12", "gpio13", "gpio14", "gpio15",
};
static const char * const qup0_se4_groups[] = {
	"gpio16", "gpio17", "gpio18", "gpio19", "gpio26", "gpio27",
};
static const char * const qup0_se5_groups[] = {
	"gpio20", "gpio21", "gpio22", "gpio23",
};
static const char * const qup0_se6_groups[] = {
	"gpio24", "gpio25", "gpio26", "gpio27",
};
static const char * const qup0_se7_groups[] = {
	"gpio28", "gpio29", "gpio30", "gpio31",
};
static const char * const qup1_se0_groups[] = {
	"gpio32", "gpio33", "gpio34", "gpio35",
};
static const char * const qup1_se1_groups[] = {
	"gpio36", "gpio37", "gpio38", "gpio39",
};
static const char * const qup1_se2_groups[] = {
	"gpio38", "gpio39", "gpio40", "gpio41", "gpio42", "gpio43",
};
static const char * const qup1_se3_groups[] = {
	"gpio44", "gpio45", "gpio46", "gpio47",
};
static const char * const qup1_se4_groups[] = {
	"gpio48", "gpio49", "gpio50", "gpio51", "gpio54", "gpio55",
};
static const char * const qup1_se5_groups[] = {
	"gpio52", "gpio53", "gpio54", "gpio55",
};
static const char * const qup1_se6_groups[] = {
	"gpio56", "gpio57", "gpio58", "gpio59",
};
static const char * const sd_write_groups[] = {
	"gpio46",
};
static const char * const tb_trig_groups[] = {
	"gpio91",
};
static const char * const tgu_ch0_groups[] = {
	"gpio24",
};
static const char * const tgu_ch1_groups[] = {
	"gpio25",
};
static const char * const tgu_ch2_groups[] = {
	"gpio26",
};
static const char * const tgu_ch3_groups[] = {
	"gpio27",
};
static const char * const tmess_prng0_groups[] = {
	"gpio73",
};
static const char * const tmess_prng1_groups[] = {
	"gpio72",
};
static const char * const tmess_prng2_groups[] = {
	"gpio70",
};
static const char * const tmess_prng3_groups[] = {
	"gpio69",
};
static const char * const tsense_pwm1_groups[] = {
	"gpio45",
};
static const char * const tsense_pwm2_groups[] = {
	"gpio45",
};
static const char * const uim0_clk_groups[] = {
	"gpio85",
};
static const char * const uim0_data_groups[] = {
	"gpio84",
};
static const char * const uim0_present_groups[] = {
	"gpio87",
};
static const char * const uim0_reset_groups[] = {
	"gpio86",
};
static const char * const uim1_clk_groups[] = {
	"gpio89",
};
static const char * const uim1_data_groups[] = {
	"gpio88",
};
static const char * const uim1_present_groups[] = {
	"gpio91",
};
static const char * const uim1_reset_groups[] = {
	"gpio90",
};
static const char * const usb0_hs_groups[] = {
	"gpio123",
};
static const char * const usb0_phy_groups[] = {
	"gpio122",
};
static const char * const vfr_0_groups[] = {
	"gpio57",
};
static const char * const vfr_1_groups[] = {
	"gpio114",
};
static const char * const vsense_trigger_groups[] = {
	"gpio22",
};

static const struct msm_function diwali_functions[] = {
	FUNCTION(gpio),
	FUNCTION(qup0_se0),
	FUNCTION(ibi_i3c),
	FUNCTION(qup0_se1),
	FUNCTION(qup0_se2),
	FUNCTION(dp0_hot),
	FUNCTION(cmu_rng3),
	FUNCTION(phase_flag31),
	FUNCTION(cmu_rng2),
	FUNCTION(phase_flag30),
	FUNCTION(cmu_rng1),
	FUNCTION(phase_flag29),
	FUNCTION(cmu_rng0),
	FUNCTION(phase_flag28),
	FUNCTION(qup0_se3),
	FUNCTION(phase_flag27),
	FUNCTION(phase_flag26),
	FUNCTION(phase_flag25),
	FUNCTION(gcc_gp3),
	FUNCTION(phase_flag24),
	FUNCTION(qup0_se4),
	FUNCTION(gcc_gp1),
	FUNCTION(phase_flag23),
	FUNCTION(gcc_gp2),
	FUNCTION(phase_flag22),
	FUNCTION(phase_flag21),
	FUNCTION(phase_flag20),
	FUNCTION(qup0_se5),
	FUNCTION(cci_timer0),
	FUNCTION(phase_flag19),
	FUNCTION(cci_timer1),
	FUNCTION(vsense_trigger),
	FUNCTION(atest_usb0),
	FUNCTION(ddr_pxi0),
	FUNCTION(atest_usb00),
	FUNCTION(qup0_se6),
	FUNCTION(tgu_ch0),
	FUNCTION(atest_usb01),
	FUNCTION(ddr_pxi1),
	FUNCTION(tgu_ch1),
	FUNCTION(atest_usb02),
	FUNCTION(tgu_ch2),
	FUNCTION(atest_usb03),
	FUNCTION(tgu_ch3),
	FUNCTION(qup0_se7),
	FUNCTION(phase_flag18),
	FUNCTION(qup1_se0),
	FUNCTION(qdss_gpio15),
	FUNCTION(qup1_se1),
	FUNCTION(qup1_se2),
	FUNCTION(qdss_cti),
	FUNCTION(phase_flag17),
	FUNCTION(phase_flag16),
	FUNCTION(phase_flag15),
	FUNCTION(phase_flag14),
	FUNCTION(qup1_se3),
	FUNCTION(phase_flag13),
	FUNCTION(mdp_vsync),
	FUNCTION(tsense_pwm1),
	FUNCTION(tsense_pwm2),
	FUNCTION(sd_write),
	FUNCTION(phase_flag12),
	FUNCTION(qup1_se4),
	FUNCTION(ddr_bist),
	FUNCTION(phase_flag11),
	FUNCTION(phase_flag10),
	FUNCTION(phase_flag9),
	FUNCTION(phase_flag8),
	FUNCTION(qup1_se5),
	FUNCTION(phase_flag7),
	FUNCTION(phase_flag6),
	FUNCTION(phase_flag5),
	FUNCTION(phase_flag4),
	FUNCTION(qup1_se6),
	FUNCTION(phase_flag3),
	FUNCTION(vfr_0),
	FUNCTION(phase_flag2),
	FUNCTION(phase_flag1),
	FUNCTION(phase_flag0),
	FUNCTION(mi2s2_sck),
	FUNCTION(mi2s2_data0),
	FUNCTION(mi2s2_ws),
	FUNCTION(mi2s2_data1),
	FUNCTION(cam_mclk),
	FUNCTION(cri_trng0),
	FUNCTION(qdss_gpio0),
	FUNCTION(cri_trng1),
	FUNCTION(qdss_gpio1),
	FUNCTION(prng_rosc0),
	FUNCTION(qdss_gpio2),
	FUNCTION(qdss_gpio3),
	FUNCTION(prng_rosc1),
	FUNCTION(qdss_gpio4),
	FUNCTION(tmess_prng3),
	FUNCTION(qdss_gpio5),
	FUNCTION(cci_i2c),
	FUNCTION(tmess_prng2),
	FUNCTION(qdss_gpio6),
	FUNCTION(qdss_gpio7),
	FUNCTION(tmess_prng1),
	FUNCTION(qdss_gpio),
	FUNCTION(tmess_prng0),
	FUNCTION(prng_rosc3),
	FUNCTION(qdss_gpio8),
	FUNCTION(qdss_gpio9),
	FUNCTION(prng_rosc2),
	FUNCTION(qdss_gpio10),
	FUNCTION(cri_trng),
	FUNCTION(qdss_gpio11),
	FUNCTION(cci_timer2),
	FUNCTION(qdss_gpio12),
	FUNCTION(cci_timer3),
	FUNCTION(cci_async),
	FUNCTION(qdss_gpio13),
	FUNCTION(cci_timer4),
	FUNCTION(qdss_gpio14),
	FUNCTION(mdp_vsync2),
	FUNCTION(mdp_vsync3),
	FUNCTION(mdp_vsync0),
	FUNCTION(mdp_vsync1),
	FUNCTION(uim0_data),
	FUNCTION(uim0_clk),
	FUNCTION(uim0_reset),
	FUNCTION(uim0_present),
	FUNCTION(uim1_data),
	FUNCTION(uim1_clk),
	FUNCTION(uim1_reset),
	FUNCTION(uim1_present),
	FUNCTION(tb_trig),
	FUNCTION(qlink0_request),
	FUNCTION(pll_clk),
	FUNCTION(qlink0_enable),
	FUNCTION(qlink0_wmss),
	FUNCTION(jitter_bist),
	FUNCTION(atest_char3),
	FUNCTION(qlink1_request),
	FUNCTION(dbg_out),
	FUNCTION(atest_char2),
	FUNCTION(qlink1_enable),
	FUNCTION(qlink1_wmss),
	FUNCTION(atest_char1),
	FUNCTION(qlink2_request),
	FUNCTION(pll_bist),
	FUNCTION(atest_char),
	FUNCTION(qlink2_enable),
	FUNCTION(qlink2_wmss),
	FUNCTION(atest_char0),
	FUNCTION(coex_uart1),
	FUNCTION(nav_gpio0),
	FUNCTION(nav_gpio1),
	FUNCTION(vfr_1),
	FUNCTION(nav_gpio2),
	FUNCTION(pcie0_clk),
	FUNCTION(usb0_phy),
	FUNCTION(usb0_hs),
	FUNCTION(host2wlan_sol),
	FUNCTION(mi2s_mclk1),
	FUNCTION(audio_ref),
	FUNCTION(mi2s_mclk0),
	FUNCTION(mi2s0_sck),
	FUNCTION(mi2s0_data0),
	FUNCTION(mi2s0_data1),
	FUNCTION(mi2s0_ws),
};

/* Every pin is maintained as a single group, and missing or non-existing pin
 * would be maintained as dummy group to synchronize pin group index with
 * pin descriptor registered with pinctrl core.
 * Clients would not be able to request these dummy pin groups.
 */
static const struct msm_pingroup diwali_groups[] = {
	[0] = PINGROUP(0, qup0_se0, ibi_i3c, NA, NA, NA, NA, NA, NA, NA,
		       0xAA000, 3),
	[1] = PINGROUP(1, qup0_se0, ibi_i3c, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[2] = PINGROUP(2, qup0_se0, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[3] = PINGROUP(3, qup0_se0, NA, NA, NA, NA, NA, NA, NA, NA, 0xAA000, 4),
	[4] = PINGROUP(4, qup0_se1, ibi_i3c, NA, NA, NA, NA, NA, NA, NA,
		       0xAA000, 5),
	[5] = PINGROUP(5, qup0_se1, ibi_i3c, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[6] = PINGROUP(6, qup0_se1, qup0_se2, dp0_hot, NA, NA, NA, NA, NA, NA,
		       0, -1),
	[7] = PINGROUP(7, qup0_se1, qup0_se2, NA, NA, NA, NA, NA, NA, NA,
		       0xAA000, 6),
	[8] = PINGROUP(8, qup0_se2, cmu_rng3, NA, phase_flag31, NA, NA, NA, NA,
		       NA, 0xAA000, 7),
	[9] = PINGROUP(9, qup0_se2, cmu_rng2, NA, phase_flag30, NA, NA, NA, NA,
		       NA, 0, -1),
	[10] = PINGROUP(10, qup0_se2, cmu_rng1, NA, phase_flag29, NA, NA, NA,
			NA, NA, 0xAA000, 8),
	[11] = PINGROUP(11, qup0_se2, cmu_rng0, NA, phase_flag28, NA, NA, NA,
			NA, NA, 0xAA000, 9),
	[12] = PINGROUP(12, qup0_se3, NA, phase_flag27, NA, NA, NA, NA, NA, NA,
			0xAA000, 10),
	[13] = PINGROUP(13, qup0_se3, NA, phase_flag26, NA, NA, NA, NA, NA, NA,
			0, -1),
	[14] = PINGROUP(14, qup0_se3, NA, phase_flag25, NA, NA, NA, NA, NA, NA,
			0, -1),
	[15] = PINGROUP(15, qup0_se3, gcc_gp3, NA, phase_flag24, NA, NA, NA,
			NA, NA, 0xAA000, 11),
	[16] = PINGROUP(16, qup0_se4, gcc_gp1, NA, phase_flag23, NA, NA, NA,
			NA, NA, 0xAA000, 12),
	[17] = PINGROUP(17, qup0_se4, gcc_gp2, NA, phase_flag22, NA, NA, NA,
			NA, NA, 0, -1),
	[18] = PINGROUP(18, qup0_se4, NA, phase_flag21, NA, NA, NA, NA, NA, NA,
			0xAA000, 13),
	[19] = PINGROUP(19, qup0_se4, NA, NA, phase_flag20, NA, NA, NA, NA, NA,
			0xAA000, 14),
	[20] = PINGROUP(20, qup0_se5, cci_timer0, NA, NA, phase_flag19, NA, NA,
			NA, NA, 0xAA000, 15),
	[21] = PINGROUP(21, qup0_se5, cci_timer1, NA, NA, NA, NA, NA, NA, NA,
			0xAA004, 0),
	[22] = PINGROUP(22, qup0_se5, NA, NA, NA, vsense_trigger, atest_usb0,
			ddr_pxi0, NA, NA, 0, -1),
	[23] = PINGROUP(23, qup0_se5, NA, NA, atest_usb00, ddr_pxi0, NA, NA,
			NA, NA, 0xAA004, 1),
	[24] = PINGROUP(24, qup0_se6, tgu_ch0, NA, atest_usb01, ddr_pxi1, NA,
			NA, NA, NA, 0xAA004, 2),
	[25] = PINGROUP(25, qup0_se6, tgu_ch1, NA, atest_usb02, ddr_pxi1, NA,
			NA, NA, NA, 0xAA004, 3),
	[26] = PINGROUP(26, qup0_se6, qup0_se4, tgu_ch2, NA, atest_usb03, NA,
			NA, NA, NA, 0xAA004, 4),
	[27] = PINGROUP(27, qup0_se6, qup0_se4, tgu_ch3, NA, NA, NA, NA, NA,
			NA, 0xAA004, 5),
	[28] = PINGROUP(28, qup0_se7, NA, phase_flag18, NA, NA, NA, NA, NA, NA,
			0xAA004, 6),
	[29] = PINGROUP(29, qup0_se7, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[30] = PINGROUP(30, qup0_se7, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[31] = PINGROUP(31, qup0_se7, NA, NA, NA, NA, NA, NA, NA, NA,
			0xAA004, 7),
	[32] = PINGROUP(32, qup1_se0, ibi_i3c, NA, NA, NA, NA, NA, NA, NA,
			0xAA014, 0),
	[33] = PINGROUP(33, qup1_se0, ibi_i3c, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[34] = PINGROUP(34, qup1_se0, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[35] = PINGROUP(35, qup1_se0, qdss_gpio15, NA, NA, NA, NA, NA, NA, NA,
			0xAA014, 1),
	[36] = PINGROUP(36, qup1_se1, ibi_i3c, NA, NA, NA, NA, NA, NA, NA,
			0xAA014, 2),
	[37] = PINGROUP(37, qup1_se1, ibi_i3c, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[38] = PINGROUP(38, qup1_se1, qup1_se2, qdss_cti, NA, NA, NA, NA, NA,
			NA, 0xAA014, 3),
	[39] = PINGROUP(39, qup1_se1, qup1_se2, qdss_cti, NA, NA, NA, NA, NA,
			NA, 0xAA014, 4),
	[40] = PINGROUP(40, qup1_se2, gcc_gp1, NA, phase_flag17, NA, NA, NA,
			NA, NA, 0xAA014, 5),
	[41] = PINGROUP(41, qup1_se2, gcc_gp2, NA, phase_flag16, NA, NA, NA,
			NA, NA, 0xAA014, 6),
	[42] = PINGROUP(42, qup1_se2, gcc_gp3, NA, phase_flag15, NA, NA, NA,
			NA, NA, 0, -1),
	[43] = PINGROUP(43, qup1_se2, NA, NA, phase_flag14, NA, NA, NA, NA, NA,
			0xAA014, 7),
	[44] = PINGROUP(44, qup1_se3, NA, NA, phase_flag13, NA, NA, NA, NA, NA,
			0xAA014, 8),
	[45] = PINGROUP(45, qup1_se3, mdp_vsync, NA, tsense_pwm1, tsense_pwm2,
			NA, NA, NA, NA, 0xAA014, 9),
	[46] = PINGROUP(46, qup1_se3, sd_write, NA, phase_flag12, NA, NA, NA,
			NA, NA, 0, -1),
	[47] = PINGROUP(47, qup1_se3, NA, NA, NA, NA, NA, NA, NA, NA,
			0xAA014, 10),
	[48] = PINGROUP(48, qup1_se4, mdp_vsync, ddr_bist, NA, phase_flag11,
			NA, NA, NA, NA, 0xAA014, 11),
	[49] = PINGROUP(49, qup1_se4, mdp_vsync, ddr_bist, NA, phase_flag10,
			NA, NA, NA, NA, 0, -1),
	[50] = PINGROUP(50, qup1_se4, NA, phase_flag9, NA, NA, NA, NA, NA, NA,
			0xAA014, 12),
	[51] = PINGROUP(51, qup1_se4, ddr_bist, NA, phase_flag8, NA, NA, NA,
			NA, NA, 0xAA014, 13),
	[52] = PINGROUP(52, qup1_se5, ddr_bist, NA, phase_flag7, qdss_cti, NA,
			NA, NA, NA, 0xAA014, 14),
	[53] = PINGROUP(53, qup1_se5, NA, phase_flag6, qdss_cti, NA, NA, NA,
			NA, NA, 0xAA014, 15),
	[54] = PINGROUP(54, qup1_se5, qup1_se4, NA, phase_flag5, NA, NA, NA,
			NA, NA, 0xAA018, 0),
	[55] = PINGROUP(55, qup1_se5, qup1_se4, NA, phase_flag4, NA, NA, NA,
			NA, NA, 0xAA018, 1),
	[56] = PINGROUP(56, qup1_se6, NA, phase_flag3, NA, NA, NA, NA, NA, NA,
			0xAA018, 2),
	[57] = PINGROUP(57, qup1_se6, vfr_0, NA, phase_flag2, NA, NA, NA, NA,
			NA, 0, -1),
	[58] = PINGROUP(58, qup1_se6, NA, phase_flag1, NA, NA, NA, NA, NA, NA,
			0, -1),
	[59] = PINGROUP(59, qup1_se6, NA, phase_flag0, NA, NA, NA, NA, NA, NA,
			0xAA018, 3),
	[60] = PINGROUP(60, mi2s2_sck, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[61] = PINGROUP(61, mi2s2_data0, qdss_gpio15, NA, NA, NA, NA, NA, NA,
			NA, 0xAA00C, 0),
	[62] = PINGROUP(62, mi2s2_ws, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[63] = PINGROUP(63, mi2s2_data1, NA, NA, NA, NA, NA, NA, NA, NA,
			0xAA00C, 1),
	[64] = PINGROUP(64, cam_mclk, cri_trng0, qdss_gpio0, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[65] = PINGROUP(65, cam_mclk, cri_trng1, qdss_gpio1, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[66] = PINGROUP(66, cam_mclk, prng_rosc0, qdss_gpio2, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[67] = PINGROUP(67, cam_mclk, qdss_gpio3, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[68] = PINGROUP(68, cam_mclk, prng_rosc1, qdss_gpio4, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[69] = PINGROUP(69, cam_mclk, tmess_prng3, qdss_gpio5, NA, NA, NA, NA,
			NA, NA, 0xAA018, 4),
	[70] = PINGROUP(70, cci_i2c, tmess_prng2, qdss_gpio6, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[71] = PINGROUP(71, cci_i2c, qdss_gpio7, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[72] = PINGROUP(72, cci_i2c, tmess_prng1, qdss_gpio, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[73] = PINGROUP(73, cci_i2c, tmess_prng0, qdss_gpio, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[74] = PINGROUP(74, cci_i2c, prng_rosc3, qdss_gpio8, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[75] = PINGROUP(75, cci_i2c, qdss_gpio9, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[76] = PINGROUP(76, cci_i2c, prng_rosc2, qdss_gpio10, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[77] = PINGROUP(77, cci_i2c, cri_trng, qdss_gpio11, NA, NA, NA, NA, NA,
			NA, 0xAA018, 5),
	[78] = PINGROUP(78, cci_timer2, qdss_gpio12, NA, NA, NA, NA, NA, NA,
			NA, 0, -1),
	[79] = PINGROUP(79, cci_timer3, cci_async, qdss_gpio13, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[80] = PINGROUP(80, cci_timer4, cci_async, qdss_gpio14, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[81] = PINGROUP(81, cci_async, mdp_vsync, mdp_vsync2, mdp_vsync3, NA,
			NA, NA, NA, NA, 0xAA018, 6),
	[82] = PINGROUP(82, mdp_vsync, mdp_vsync0, mdp_vsync1, NA, NA, NA, NA,
			NA, NA, 0xAA004, 8),
	[83] = PINGROUP(83, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAA004, 9),
	[84] = PINGROUP(84, uim0_data, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[85] = PINGROUP(85, uim0_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[86] = PINGROUP(86, uim0_reset, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[87] = PINGROUP(87, uim0_present, NA, NA, NA, NA, NA, NA, NA, NA,
			0xAA004, 10),
	[88] = PINGROUP(88, uim1_data, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[89] = PINGROUP(89, uim1_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[90] = PINGROUP(90, uim1_reset, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[91] = PINGROUP(91, uim1_present, tb_trig, NA, NA, NA, NA, NA, NA, NA,
			0xAA004, 11),
	[92] = PINGROUP(92, qlink0_request, pll_clk, NA, NA, NA, NA, NA, NA,
			NA, 0xAA004, 12),
	[93] = PINGROUP(93, qlink0_enable, NA, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[94] = PINGROUP(94, qlink0_wmss, jitter_bist, atest_char3, NA, NA, NA,
			NA, NA, NA, 0, -1),
	[95] = PINGROUP(95, qlink1_request, dbg_out, atest_char2, NA, NA, NA,
			NA, NA, NA, 0xAA004, 13),
	[96] = PINGROUP(96, qlink1_enable, NA, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[97] = PINGROUP(97, qlink1_wmss, NA, atest_char1, NA, NA, NA, NA, NA,
			NA, 0, -1),
	[98] = PINGROUP(98, qlink2_request, pll_bist, atest_char, NA, NA, NA,
			NA, NA, NA, 0xAA004, 14),
	[99] = PINGROUP(99, qlink2_enable, NA, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[100] = PINGROUP(100, qlink2_wmss, NA, atest_char0, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[101] = PINGROUP(101, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[102] = PINGROUP(102, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[103] = PINGROUP(103, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[104] = PINGROUP(104, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[105] = PINGROUP(105, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[106] = PINGROUP(106, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[107] = PINGROUP(107, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[108] = PINGROUP(108, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[109] = PINGROUP(109, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[110] = PINGROUP(110, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[111] = PINGROUP(111, coex_uart1, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[112] = PINGROUP(112, coex_uart1, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[113] = PINGROUP(113, nav_gpio0, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xAA004, 15),
	[114] = PINGROUP(114, nav_gpio1, vfr_1, NA, NA, NA, NA, NA, NA, NA,
			 0xAA008, 0),
	[115] = PINGROUP(115, nav_gpio2, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xAA008, 1),
	[116] = PINGROUP(116, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[117] = PINGROUP(117, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[118] = PINGROUP(118, pcie0_clk, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xAA008, 2),
	[119] = PINGROUP(119, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAA008, 3),
	[120] = PINGROUP(120, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[121] = PINGROUP(121, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAA008, 4),
	[122] = PINGROUP(122, usb0_phy, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xAA008, 5),
	[123] = PINGROUP(123, usb0_hs, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xAA008, 6),
	[124] = PINGROUP(124, host2wlan_sol, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[125] = PINGROUP(125, mi2s_mclk1, audio_ref, NA, NA, NA, NA, NA, NA,
			 NA, 0xAA00C, 2),
	[126] = PINGROUP(126, mi2s_mclk0, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[127] = PINGROUP(127, mi2s0_sck, qdss_cti, NA, NA, NA, NA, NA, NA, NA,
			 0xAA00C, 3),
	[128] = PINGROUP(128, mi2s0_data0, qdss_cti, NA, NA, NA, NA, NA, NA,
			 NA, 0xAA00C, 4),
	[129] = PINGROUP(129, mi2s0_data1, qdss_cti, NA, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[130] = PINGROUP(130, mi2s0_ws, qdss_cti, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[131] = PINGROUP(131, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[132] = PINGROUP(132, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAA00C, 5),
	[133] = PINGROUP(133, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[134] = PINGROUP(134, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[135] = PINGROUP(135, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAA00C, 6),
	[136] = PINGROUP(136, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[137] = PINGROUP(137, NA, qdss_gpio0, NA, NA, NA, NA, NA, NA, NA,
			 0xAA00C, 7),
	[138] = PINGROUP(138, NA, qdss_gpio1, NA, NA, NA, NA, NA, NA, NA,
			 0xAA00C, 8),
	[139] = PINGROUP(139, NA, qdss_gpio2, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[140] = PINGROUP(140, NA, qdss_gpio3, NA, NA, NA, NA, NA, NA, NA,
			 0xAA00C, 9),
	[141] = PINGROUP(141, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[142] = PINGROUP(142, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAA00C, 10),
	[143] = PINGROUP(143, NA, qdss_gpio4, NA, NA, NA, NA, NA, NA, NA,
			 0xAA00C, 11),
	[144] = PINGROUP(144, NA, qdss_gpio5, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[145] = PINGROUP(145, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[146] = PINGROUP(146, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[147] = PINGROUP(147, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAA00C, 12),
	[148] = PINGROUP(148, NA, qdss_gpio6, NA, NA, NA, NA, NA, NA, NA,
			 0xAA00C, 13),
	[149] = PINGROUP(149, NA, qdss_gpio7, NA, NA, NA, NA, NA, NA, NA,
			 0xAA00C, 14),
	[150] = PINGROUP(150, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[151] = PINGROUP(151, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAA00C, 15),
	[152] = PINGROUP(152, NA, qdss_gpio, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[153] = PINGROUP(153, NA, qdss_gpio, NA, NA, NA, NA, NA, NA, NA,
			 0xAA010, 0),
	[154] = PINGROUP(154, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAA010, 1),
	[155] = PINGROUP(155, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[156] = PINGROUP(156, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAA010, 2),
	[157] = PINGROUP(157, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAA010, 3),
	[158] = PINGROUP(158, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAA010, 4),
	[159] = PINGROUP(159, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAA010, 5),
	[160] = PINGROUP(160, NA, qdss_gpio8, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[161] = PINGROUP(161, NA, qdss_gpio9, NA, NA, NA, NA, NA, NA, NA,
			 0xAA010, 6),
	[162] = PINGROUP(162, NA, qdss_gpio10, NA, NA, NA, NA, NA, NA, NA,
			 0xAA010, 7),
	[163] = PINGROUP(163, NA, qdss_gpio11, NA, NA, NA, NA, NA, NA, NA,
			 0xAA010, 8),
	[164] = PINGROUP(164, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xAA010, 9),
	[165] = PINGROUP(165, NA, qdss_gpio12, NA, NA, NA, NA, NA, NA, NA,
			 0xAA010, 10),
	[166] = PINGROUP(166, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[167] = PINGROUP(167, NA, qdss_gpio13, NA, NA, NA, NA, NA, NA, NA,
			 0xAA010, 11),
	[168] = PINGROUP(168, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[169] = PINGROUP(169, NA, qdss_gpio14, NA, NA, NA, NA, NA, NA, NA,
			 0xAA010, 12),
	[170] = UFS_RESET(ufs_reset, 0x1b6000),
	[171] = SDC_QDSD_PINGROUP(sdc2_clk, 0x1ae000, 14, 6),
	[172] = SDC_QDSD_PINGROUP(sdc2_cmd, 0x1ae000, 11, 3),
	[173] = SDC_QDSD_PINGROUP(sdc2_data, 0x1ae000, 9, 0),
};

/*TODO:
 *	static struct pinctrl_qup diwali_qup_regs[] = {
 *	QUP_I3C(0_SE0, QUP_I3C_0_SE0_MODE_OFFSET),
 *	QUP_I3C(0_SE1, QUP_I3C_0_SE1_MODE_OFFSET),
 *	QUP_I3C(1_SE0, QUP_I3C_1_SE0_MODE_OFFSET),
 *	QUP_I3C(1_SE1, QUP_I3C_1_SE1_MODE_OFFSET),
 * };
 */

static const struct msm_gpio_wakeirq_map diwali_pdc_map[] = {
	{ 0, 47 }, { 3, 48 }, { 4, 49 }, { 7, 63 }, { 8, 53 },
	{ 10, 85 }, { 11, 83 }, { 12, 50 }, { 15, 51 }, { 16, 44 },
	{ 18, 57 }, { 19, 56 }, { 20, 58 }, { 21, 59 }, { 23, 60 },
	{ 24, 66 }, { 25, 96 }, { 26, 76 }, { 27, 62 }, { 28, 61 },
	{ 31, 67 }, { 32, 68 }, { 35, 82 }, { 36, 69 }, { 38, 70 },
	{ 39, 43 }, { 40, 71 }, { 41, 86 }, { 43, 55 }, { 44, 73 },
	{ 45, 93 }, { 47, 46 }, { 48, 52 }, { 50, 92 }, { 51, 54 },
	{ 52, 65 }, { 53, 72 }, { 54, 77 }, { 55, 78 }, { 56, 79 },
	{ 59, 80 }, { 61, 81 }, { 63, 87 }, { 69, 75 }, { 77, 88 },
	{ 81, 95 }, { 82, 89 }, { 83, 45 }, { 87, 90 }, { 91, 42 },
	{ 92, 91 }, { 95, 97 }, { 98, 98 }, { 113, 64 }, { 114, 99 },
	{ 115, 100 }, { 118, 94 }, { 119, 101 }, { 121, 102 }, { 122, 103 },
	{ 123, 104 }, { 125, 74 }, { 127, 105 }, { 128, 106 }, { 132, 107 },
	{ 135, 108 }, { 137, 109 }, { 138, 110 }, { 140, 111 }, { 142, 112 },
	{ 143, 113 }, { 147, 114 }, { 148, 115 }, { 149, 116 }, { 151, 117 },
	{ 153, 118 }, { 154, 119 }, { 156, 120 }, { 157, 121 }, { 158, 122 },
	{ 159, 123 }, { 161, 124 }, { 162, 84 }, { 163, 128 }, { 164, 129 },
	{ 165, 130 }, { 167, 131 }, { 169, 132 },
};
