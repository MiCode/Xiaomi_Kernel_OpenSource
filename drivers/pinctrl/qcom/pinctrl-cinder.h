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
		.ctl_reg = REG_BASE + ctl,				\
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
static const struct pinctrl_pin_desc cinder_pins[] = {
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
	PINCTRL_PIN(151, "SDC1_RCLK"),
	PINCTRL_PIN(152, "SDC1_CLK"),
	PINCTRL_PIN(153, "SDC1_CMD"),
	PINCTRL_PIN(154, "SDC1_DATA"),
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

static const unsigned int sdc1_rclk_pins[] = { 151 };
static const unsigned int sdc1_clk_pins[] = { 152 };
static const unsigned int sdc1_cmd_pins[] = { 153 };
static const unsigned int sdc1_data_pins[] = { 154 };

enum cinder_functions {
	msm_mux_gpio,
	msm_mux_qup20,
	msm_mux_qup21,
	msm_mux_ddr_bist,
	msm_mux_tsense_pwm1,
	msm_mux_tsense_pwm2,
	msm_mux_qup22,
	msm_mux_qup00,
	msm_mux_tgu_ch0,
	msm_mux_phase_flag0,
	msm_mux_qdss_gpio0,
	msm_mux_tgu_ch1,
	msm_mux_phase_flag1,
	msm_mux_qdss_gpio1,
	msm_mux_tgu_ch2,
	msm_mux_phase_flag2,
	msm_mux_qdss_gpio2,
	msm_mux_tgu_ch3,
	msm_mux_phase_flag3,
	msm_mux_qdss_gpio3,
	msm_mux_qup01,
	msm_mux_qup02,
	msm_mux_qup17,
	msm_mux_qup03,
	msm_mux_qup04,
	msm_mux_phase_flag4,
	msm_mux_qdss_gpio4,
	msm_mux_phase_flag5,
	msm_mux_qdss_gpio5,
	msm_mux_qup10,
	msm_mux_prng_rosc0,
	msm_mux_phase_flag6,
	msm_mux_prng_rosc1,
	msm_mux_phase_flag7,
	msm_mux_prng_rosc2,
	msm_mux_pll_bist,
	msm_mux_phase_flag8,
	msm_mux_prng_rosc3,
	msm_mux_phase_flag9,
	msm_mux_qup11,
	msm_mux_qup12,
	msm_mux_phase_flag10,
	msm_mux_phase_flag11,
	msm_mux_phase_flag12,
	msm_mux_qdss_gpio6,
	msm_mux_phase_flag13,
	msm_mux_qdss_gpio7,
	msm_mux_qup13,
	msm_mux_qup14,
	msm_mux_intr_c0,
	msm_mux_phase_flag14,
	msm_mux_qdss_gpio8,
	msm_mux_intr_c1,
	msm_mux_phase_flag15,
	msm_mux_qdss_gpio9,
	msm_mux_intr_c2,
	msm_mux_phase_flag16,
	msm_mux_qdss_gpio10,
	msm_mux_qup16,
	msm_mux_phase_flag17,
	msm_mux_qdss_gpio11,
	msm_mux_qup15,
	msm_mux_tmess_prng3,
	msm_mux_phase_flag18,
	msm_mux_qdss_gpio12,
	msm_mux_tmess_prng2,
	msm_mux_phase_flag19,
	msm_mux_qdss_gpio13,
	msm_mux_tmess_prng1,
	msm_mux_phase_flag20,
	msm_mux_qdss_gpio14,
	msm_mux_tmess_prng0,
	msm_mux_phase_flag21,
	msm_mux_qdss_gpio15,
	msm_mux_qdss_gpio,
	msm_mux_qdss_cti,
	msm_mux_usb_con,
	msm_mux_phase_flag22,
	msm_mux_usb_dfp,
	msm_mux_phase_flag23,
	msm_mux_SI5518_INT,
	msm_mux_tgu_ch4,
	msm_mux_tgu_ch5,
	msm_mux_tgu_ch6,
	msm_mux_hardsync_pps,
	msm_mux_tgu_ch7,
	msm_mux_tod_pps,
	msm_mux_qlink6_wmss,
	msm_mux_qlink7_wmss,
	msm_mux_qlink0_request,
	msm_mux_qlink0_enable,
	msm_mux_qlink1_request,
	msm_mux_qlink1_enable,
	msm_mux_qlink2_request,
	msm_mux_qlink2_enable,
	msm_mux_qlink3_request,
	msm_mux_qlink3_enable,
	msm_mux_qlink4_request,
	msm_mux_qlink4_enable,
	msm_mux_qlink5_request,
	msm_mux_qlink5_enable,
	msm_mux_qlink6_request,
	msm_mux_qlink6_enable,
	msm_mux_qlink7_request,
	msm_mux_qlink7_enable,
	msm_mux_qlink0_wmss,
	msm_mux_qlink1_wmss,
	msm_mux_qlink4_wmss,
	msm_mux_qlink5_wmss,
	msm_mux_eth012_int,
	msm_mux_gcc_gp1,
	msm_mux_eth345_int,
	msm_mux_gcc_gp2,
	msm_mux_eth6_int,
	msm_mux_smb_alert,
	msm_mux_gcc_gp3,
	msm_mux_phase_flag24,
	msm_mux_cmu_rng3,
	msm_mux_atest_char3,
	msm_mux_usb2phy_ac,
	msm_mux_phase_flag25,
	msm_mux_cmu_rng2,
	msm_mux_atest_char2,
	msm_mux_usb_phy,
	msm_mux_phase_flag26,
	msm_mux_cmu_rng1,
	msm_mux_atest_char1,
	msm_mux_phase_flag27,
	msm_mux_cmu_rng0,
	msm_mux_atest_char0,
	msm_mux_vfr_0,
	msm_mux_vfr_1,
	msm_mux_phase_flag28,
	msm_mux_atest_char,
	msm_mux_phase_flag29,
	msm_mux_phase_flag30,
	msm_mux_pll_clk,
	msm_mux_pcie0_clkreqn,
	msm_mux_char_exec,
	msm_mux_phase_flag31,
	msm_mux_CMO_PRI,
	msm_mux_qspi_data,
	msm_mux_tb_trig,
	msm_mux_atest_usb0,
	msm_mux_ddr_pxi0,
	msm_mux_atest_usb00,
	msm_mux_atest_usb01,
	msm_mux_ddr_pxi1,
	msm_mux_atest_usb02,
	msm_mux_atest_usb03,
	msm_mux_ddr_pxi2,
	msm_mux_ddr_pxi3,
	msm_mux_ddr_pxi4,
	msm_mux_ddr_pxi5,
	msm_mux_qspi_cs,
	msm_mux_qspi_clk,
	msm_mux_ddr_pxi6,
	msm_mux_ddr_pxi7,
	msm_mux_qup05,
	msm_mux_qup06,
	msm_mux_jitter_bist,
	msm_mux_smb_dat,
	msm_mux_smb_clk,
	msm_mux_qup07,
	msm_mux_vsense_trigger,
	msm_mux_dbg_out,
	msm_mux_qlink2_wmss,
	msm_mux_qlink3_wmss,
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
	"gpio147", "gpio148", "gpio149", "gpio150",
};
static const char * const qup20_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3",
};
static const char * const qup21_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3",
};
static const char * const ddr_bist_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3",
};
static const char * const tsense_pwm1_groups[] = {
	"gpio2",
};
static const char * const tsense_pwm2_groups[] = {
	"gpio3",
};
static const char * const qup22_groups[] = {
	"gpio4", "gpio5", "gpio128", "gpio129",
};
static const char * const qup00_groups[] = {
	"gpio6", "gpio7", "gpio8", "gpio9",
};
static const char * const tgu_ch0_groups[] = {
	"gpio6",
};
static const char * const phase_flag0_groups[] = {
	"gpio6",
};
static const char * const qdss_gpio0_groups[] = {
	"gpio6", "gpio18",
};
static const char * const tgu_ch1_groups[] = {
	"gpio7",
};
static const char * const phase_flag1_groups[] = {
	"gpio7",
};
static const char * const qdss_gpio1_groups[] = {
	"gpio7", "gpio19",
};
static const char * const tgu_ch2_groups[] = {
	"gpio8",
};
static const char * const phase_flag2_groups[] = {
	"gpio8",
};
static const char * const qdss_gpio2_groups[] = {
	"gpio8", "gpio20",
};
static const char * const tgu_ch3_groups[] = {
	"gpio9",
};
static const char * const phase_flag3_groups[] = {
	"gpio9",
};
static const char * const qdss_gpio3_groups[] = {
	"gpio9", "gpio21",
};
static const char * const qup01_groups[] = {
	"gpio10", "gpio11", "gpio12", "gpio13",
};
static const char * const qup02_groups[] = {
	"gpio10", "gpio11", "gpio12", "gpio13",
};
static const char * const qup17_groups[] = {
	"gpio12", "gpio13", "gpio14", "gpio30", "gpio31", "gpio40", "gpio41",
};
static const char * const qup03_groups[] = {
	"gpio14", "gpio15", "gpio16", "gpio17",
};
static const char * const qup04_groups[] = {
	"gpio14", "gpio15", "gpio16", "gpio17",
};
static const char * const phase_flag4_groups[] = {
	"gpio16",
};
static const char * const qdss_gpio4_groups[] = {
	"gpio16", "gpio22",
};
static const char * const phase_flag5_groups[] = {
	"gpio17",
};
static const char * const qdss_gpio5_groups[] = {
	"gpio17", "gpio23",
};
static const char * const qup10_groups[] = {
	"gpio18", "gpio19", "gpio20", "gpio21",
};
static const char * const prng_rosc0_groups[] = {
	"gpio18",
};
static const char * const phase_flag6_groups[] = {
	"gpio18",
};
static const char * const prng_rosc1_groups[] = {
	"gpio19",
};
static const char * const phase_flag7_groups[] = {
	"gpio19",
};
static const char * const prng_rosc2_groups[] = {
	"gpio20",
};
static const char * const pll_bist_groups[] = {
	"gpio20",
};
static const char * const phase_flag8_groups[] = {
	"gpio20",
};
static const char * const prng_rosc3_groups[] = {
	"gpio21",
};
static const char * const phase_flag9_groups[] = {
	"gpio21",
};
static const char * const qup11_groups[] = {
	"gpio22", "gpio23", "gpio24", "gpio25",
};
static const char * const qup12_groups[] = {
	"gpio22", "gpio23", "gpio24", "gpio25",
};
static const char * const phase_flag10_groups[] = {
	"gpio22",
};
static const char * const phase_flag11_groups[] = {
	"gpio23",
};
static const char * const phase_flag12_groups[] = {
	"gpio24",
};
static const char * const qdss_gpio6_groups[] = {
	"gpio24", "gpio42",
};
static const char * const phase_flag13_groups[] = {
	"gpio25",
};
static const char * const qdss_gpio7_groups[] = {
	"gpio25", "gpio43",
};
static const char * const qup13_groups[] = {
	"gpio26", "gpio27", "gpio28", "gpio29",
};
static const char * const qup14_groups[] = {
	"gpio26", "gpio27", "gpio28", "gpio29",
};
static const char * const intr_c0_groups[] = {
	"gpio26", "gpio141",
};
static const char * const phase_flag14_groups[] = {
	"gpio26",
};
static const char * const qdss_gpio8_groups[] = {
	"gpio26", "gpio89",
};
static const char * const intr_c1_groups[] = {
	"gpio27", "gpio142",
};
static const char * const phase_flag15_groups[] = {
	"gpio27",
};
static const char * const qdss_gpio9_groups[] = {
	"gpio27", "gpio90",
};
static const char * const intr_c2_groups[] = {
	"gpio28", "gpio143",
};
static const char * const phase_flag16_groups[] = {
	"gpio28",
};
static const char * const qdss_gpio10_groups[] = {
	"gpio28", "gpio91",
};
static const char * const qup16_groups[] = {
	"gpio29", "gpio34", "gpio35", "gpio36", "gpio37", "gpio38", "gpio39",
};
static const char * const phase_flag17_groups[] = {
	"gpio29",
};
static const char * const qdss_gpio11_groups[] = {
	"gpio29", "gpio92",
};
static const char * const qup15_groups[] = {
	"gpio30", "gpio31", "gpio32", "gpio33",
};
static const char * const tmess_prng3_groups[] = {
	"gpio30",
};
static const char * const phase_flag18_groups[] = {
	"gpio30",
};
static const char * const qdss_gpio12_groups[] = {
	"gpio30", "gpio95",
};
static const char * const tmess_prng2_groups[] = {
	"gpio31",
};
static const char * const phase_flag19_groups[] = {
	"gpio31",
};
static const char * const qdss_gpio13_groups[] = {
	"gpio31", "gpio96",
};
static const char * const tmess_prng1_groups[] = {
	"gpio32",
};
static const char * const phase_flag20_groups[] = {
	"gpio32",
};
static const char * const qdss_gpio14_groups[] = {
	"gpio32", "gpio97",
};
static const char * const tmess_prng0_groups[] = {
	"gpio33",
};
static const char * const phase_flag21_groups[] = {
	"gpio33",
};
static const char * const qdss_gpio15_groups[] = {
	"gpio33", "gpio102",
};
static const char * const qdss_gpio_groups[] = {
	"gpio34", "gpio35", "gpio88", "gpio103",
};
static const char * const qdss_cti_groups[] = {
	"gpio36", "gpio37", "gpio38", "gpio39", "gpio40", "gpio41", "gpio48",
	"gpio49", "gpio86", "gpio87", "gpio93", "gpio94", "gpio130", "gpio131",
	"gpio132", "gpio133", "gpio134", "gpio135", "gpio144", "gpio145",
};
static const char * const usb_con_groups[] = {
	"gpio42",
};
static const char * const phase_flag22_groups[] = {
	"gpio42",
};
static const char * const usb_dfp_groups[] = {
	"gpio43",
};
static const char * const phase_flag23_groups[] = {
	"gpio43",
};
static const char * const SI5518_INT_groups[] = {
	"gpio44",
};
static const char * const tgu_ch4_groups[] = {
	"gpio44",
};
static const char * const tgu_ch5_groups[] = {
	"gpio45",
};
static const char * const tgu_ch6_groups[] = {
	"gpio46",
};
static const char * const hardsync_pps_groups[] = {
	"gpio47",
};
static const char * const tgu_ch7_groups[] = {
	"gpio47",
};
static const char * const tod_pps_groups[] = {
	"gpio48",
};
static const char * const qlink6_wmss_groups[] = {
	"gpio56",
};
static const char * const qlink7_wmss_groups[] = {
	"gpio57",
};
static const char * const qlink0_request_groups[] = {
	"gpio66",
};
static const char * const qlink0_enable_groups[] = {
	"gpio67",
};
static const char * const qlink1_request_groups[] = {
	"gpio68",
};
static const char * const qlink1_enable_groups[] = {
	"gpio69",
};
static const char * const qlink2_request_groups[] = {
	"gpio70",
};
static const char * const qlink2_enable_groups[] = {
	"gpio71",
};
static const char * const qlink3_request_groups[] = {
	"gpio72",
};
static const char * const qlink3_enable_groups[] = {
	"gpio73",
};
static const char * const qlink4_request_groups[] = {
	"gpio74",
};
static const char * const qlink4_enable_groups[] = {
	"gpio75",
};
static const char * const qlink5_request_groups[] = {
	"gpio76",
};
static const char * const qlink5_enable_groups[] = {
	"gpio77",
};
static const char * const qlink6_request_groups[] = {
	"gpio78",
};
static const char * const qlink6_enable_groups[] = {
	"gpio79",
};
static const char * const qlink7_request_groups[] = {
	"gpio80",
};
static const char * const qlink7_enable_groups[] = {
	"gpio81",
};
static const char * const qlink0_wmss_groups[] = {
	"gpio82",
};
static const char * const qlink1_wmss_groups[] = {
	"gpio83",
};
static const char * const qlink4_wmss_groups[] = {
	"gpio84",
};
static const char * const qlink5_wmss_groups[] = {
	"gpio85",
};
static const char * const eth012_int_groups[] = {
	"gpio86",
};
static const char * const gcc_gp1_groups[] = {
	"gpio86", "gpio134",
};
static const char * const eth345_int_groups[] = {
	"gpio87",
};
static const char * const gcc_gp2_groups[] = {
	"gpio87", "gpio135",
};
static const char * const eth6_int_groups[] = {
	"gpio88",
};
static const char * const smb_alert_groups[] = {
	"gpio88", "gpio101",
};
static const char * const gcc_gp3_groups[] = {
	"gpio88", "gpio136",
};
static const char * const phase_flag24_groups[] = {
	"gpio89",
};
static const char * const cmu_rng3_groups[] = {
	"gpio89",
};
static const char * const atest_char3_groups[] = {
	"gpio89",
};
static const char * const usb2phy_ac_groups[] = {
	"gpio90",
};
static const char * const phase_flag25_groups[] = {
	"gpio90",
};
static const char * const cmu_rng2_groups[] = {
	"gpio90",
};
static const char * const atest_char2_groups[] = {
	"gpio90",
};
static const char * const usb_phy_groups[] = {
	"gpio91",
};
static const char * const phase_flag26_groups[] = {
	"gpio91",
};
static const char * const cmu_rng1_groups[] = {
	"gpio91",
};
static const char * const atest_char1_groups[] = {
	"gpio91",
};
static const char * const phase_flag27_groups[] = {
	"gpio92",
};
static const char * const cmu_rng0_groups[] = {
	"gpio92",
};
static const char * const atest_char0_groups[] = {
	"gpio92",
};
static const char * const vfr_0_groups[] = {
	"gpio93",
};
static const char * const vfr_1_groups[] = {
	"gpio94",
};
static const char * const phase_flag28_groups[] = {
	"gpio95",
};
static const char * const atest_char_groups[] = {
	"gpio95",
};
static const char * const phase_flag29_groups[] = {
	"gpio96",
};
static const char * const phase_flag30_groups[] = {
	"gpio97",
};
static const char * const pll_clk_groups[] = {
	"gpio98",
};
static const char * const pcie0_clkreqn_groups[] = {
	"gpio99",
};
static const char * const char_exec_groups[] = {
	"gpio99", "gpio100",
};
static const char * const phase_flag31_groups[] = {
	"gpio102",
};
static const char * const CMO_PRI_groups[] = {
	"gpio103",
};
static const char * const qspi_data_groups[] = {
	"gpio114", "gpio115", "gpio116", "gpio117",
};
static const char * const tb_trig_groups[] = {
	"gpio114",
};
static const char * const atest_usb0_groups[] = {
	"gpio114",
};
static const char * const ddr_pxi0_groups[] = {
	"gpio114", "gpio115",
};
static const char * const atest_usb00_groups[] = {
	"gpio115",
};
static const char * const atest_usb01_groups[] = {
	"gpio116",
};
static const char * const ddr_pxi1_groups[] = {
	"gpio116", "gpio117",
};
static const char * const atest_usb02_groups[] = {
	"gpio117",
};
static const char * const atest_usb03_groups[] = {
	"gpio118",
};
static const char * const ddr_pxi2_groups[] = {
	"gpio118", "gpio119",
};
static const char * const ddr_pxi3_groups[] = {
	"gpio120", "gpio121",
};
static const char * const ddr_pxi4_groups[] = {
	"gpio122", "gpio123",
};
static const char * const ddr_pxi5_groups[] = {
	"gpio124", "gpio125",
};
static const char * const qspi_cs_groups[] = {
	"gpio125",
};
static const char * const qspi_clk_groups[] = {
	"gpio126",
};
static const char * const ddr_pxi6_groups[] = {
	"gpio126", "gpio127",
};
static const char * const ddr_pxi7_groups[] = {
	"gpio128", "gpio129",
};
static const char * const qup05_groups[] = {
	"gpio130", "gpio131", "gpio132", "gpio133",
};
static const char * const qup06_groups[] = {
	"gpio130", "gpio131", "gpio132", "gpio133",
};
static const char * const jitter_bist_groups[] = {
	"gpio130",
};
static const char * const smb_dat_groups[] = {
	"gpio132",
};
static const char * const smb_clk_groups[] = {
	"gpio133",
};
static const char * const qup07_groups[] = {
	"gpio134", "gpio134", "gpio135", "gpio135",
};
static const char * const vsense_trigger_groups[] = {
	"gpio135",
};
static const char * const dbg_out_groups[] = {
	"gpio136",
};
static const char * const qlink2_wmss_groups[] = {
	"gpio138",
};
static const char * const qlink3_wmss_groups[] = {
	"gpio139",
};

static const struct msm_function cinder_functions[] = {
	FUNCTION(gpio),
	FUNCTION(qup20),
	FUNCTION(qup21),
	FUNCTION(ddr_bist),
	FUNCTION(tsense_pwm1),
	FUNCTION(tsense_pwm2),
	FUNCTION(qup22),
	FUNCTION(qup00),
	FUNCTION(tgu_ch0),
	FUNCTION(phase_flag0),
	FUNCTION(qdss_gpio0),
	FUNCTION(tgu_ch1),
	FUNCTION(phase_flag1),
	FUNCTION(qdss_gpio1),
	FUNCTION(tgu_ch2),
	FUNCTION(phase_flag2),
	FUNCTION(qdss_gpio2),
	FUNCTION(tgu_ch3),
	FUNCTION(phase_flag3),
	FUNCTION(qdss_gpio3),
	FUNCTION(qup01),
	FUNCTION(qup02),
	FUNCTION(qup17),
	FUNCTION(qup03),
	FUNCTION(qup04),
	FUNCTION(phase_flag4),
	FUNCTION(qdss_gpio4),
	FUNCTION(phase_flag5),
	FUNCTION(qdss_gpio5),
	FUNCTION(qup10),
	FUNCTION(prng_rosc0),
	FUNCTION(phase_flag6),
	FUNCTION(prng_rosc1),
	FUNCTION(phase_flag7),
	FUNCTION(prng_rosc2),
	FUNCTION(pll_bist),
	FUNCTION(phase_flag8),
	FUNCTION(prng_rosc3),
	FUNCTION(phase_flag9),
	FUNCTION(qup11),
	FUNCTION(qup12),
	FUNCTION(phase_flag10),
	FUNCTION(phase_flag11),
	FUNCTION(phase_flag12),
	FUNCTION(qdss_gpio6),
	FUNCTION(phase_flag13),
	FUNCTION(qdss_gpio7),
	FUNCTION(qup13),
	FUNCTION(qup14),
	FUNCTION(intr_c0),
	FUNCTION(phase_flag14),
	FUNCTION(qdss_gpio8),
	FUNCTION(intr_c1),
	FUNCTION(phase_flag15),
	FUNCTION(qdss_gpio9),
	FUNCTION(intr_c2),
	FUNCTION(phase_flag16),
	FUNCTION(qdss_gpio10),
	FUNCTION(qup16),
	FUNCTION(phase_flag17),
	FUNCTION(qdss_gpio11),
	FUNCTION(qup15),
	FUNCTION(tmess_prng3),
	FUNCTION(phase_flag18),
	FUNCTION(qdss_gpio12),
	FUNCTION(tmess_prng2),
	FUNCTION(phase_flag19),
	FUNCTION(qdss_gpio13),
	FUNCTION(tmess_prng1),
	FUNCTION(phase_flag20),
	FUNCTION(qdss_gpio14),
	FUNCTION(tmess_prng0),
	FUNCTION(phase_flag21),
	FUNCTION(qdss_gpio15),
	FUNCTION(qdss_gpio),
	FUNCTION(qdss_cti),
	FUNCTION(usb_con),
	FUNCTION(phase_flag22),
	FUNCTION(usb_dfp),
	FUNCTION(phase_flag23),
	FUNCTION(SI5518_INT),
	FUNCTION(tgu_ch4),
	FUNCTION(tgu_ch5),
	FUNCTION(tgu_ch6),
	FUNCTION(hardsync_pps),
	FUNCTION(tgu_ch7),
	FUNCTION(tod_pps),
	FUNCTION(qlink6_wmss),
	FUNCTION(qlink7_wmss),
	FUNCTION(qlink0_request),
	FUNCTION(qlink0_enable),
	FUNCTION(qlink1_request),
	FUNCTION(qlink1_enable),
	FUNCTION(qlink2_request),
	FUNCTION(qlink2_enable),
	FUNCTION(qlink3_request),
	FUNCTION(qlink3_enable),
	FUNCTION(qlink4_request),
	FUNCTION(qlink4_enable),
	FUNCTION(qlink5_request),
	FUNCTION(qlink5_enable),
	FUNCTION(qlink6_request),
	FUNCTION(qlink6_enable),
	FUNCTION(qlink7_request),
	FUNCTION(qlink7_enable),
	FUNCTION(qlink0_wmss),
	FUNCTION(qlink1_wmss),
	FUNCTION(qlink4_wmss),
	FUNCTION(qlink5_wmss),
	FUNCTION(eth012_int),
	FUNCTION(gcc_gp1),
	FUNCTION(eth345_int),
	FUNCTION(gcc_gp2),
	FUNCTION(eth6_int),
	FUNCTION(smb_alert),
	FUNCTION(gcc_gp3),
	FUNCTION(phase_flag24),
	FUNCTION(cmu_rng3),
	FUNCTION(atest_char3),
	FUNCTION(usb2phy_ac),
	FUNCTION(phase_flag25),
	FUNCTION(cmu_rng2),
	FUNCTION(atest_char2),
	FUNCTION(usb_phy),
	FUNCTION(phase_flag26),
	FUNCTION(cmu_rng1),
	FUNCTION(atest_char1),
	FUNCTION(phase_flag27),
	FUNCTION(cmu_rng0),
	FUNCTION(atest_char0),
	FUNCTION(vfr_0),
	FUNCTION(vfr_1),
	FUNCTION(phase_flag28),
	FUNCTION(atest_char),
	FUNCTION(phase_flag29),
	FUNCTION(phase_flag30),
	FUNCTION(pll_clk),
	FUNCTION(pcie0_clkreqn),
	FUNCTION(char_exec),
	FUNCTION(phase_flag31),
	FUNCTION(CMO_PRI),
	FUNCTION(qspi_data),
	FUNCTION(tb_trig),
	FUNCTION(atest_usb0),
	FUNCTION(ddr_pxi0),
	FUNCTION(atest_usb00),
	FUNCTION(atest_usb01),
	FUNCTION(ddr_pxi1),
	FUNCTION(atest_usb02),
	FUNCTION(atest_usb03),
	FUNCTION(ddr_pxi2),
	FUNCTION(ddr_pxi3),
	FUNCTION(ddr_pxi4),
	FUNCTION(ddr_pxi5),
	FUNCTION(qspi_cs),
	FUNCTION(qspi_clk),
	FUNCTION(ddr_pxi6),
	FUNCTION(ddr_pxi7),
	FUNCTION(qup05),
	FUNCTION(qup06),
	FUNCTION(jitter_bist),
	FUNCTION(smb_dat),
	FUNCTION(smb_clk),
	FUNCTION(qup07),
	FUNCTION(vsense_trigger),
	FUNCTION(dbg_out),
	FUNCTION(qlink2_wmss),
	FUNCTION(qlink3_wmss),
};

/* Every pin is maintained as a single group, and missing or non-existing pin
 * would be maintained as dummy group to synchronize pin group index with
 * pin descriptor registered with pinctrl core.
 * Clients would not be able to request these dummy pin groups.
 */
static const struct msm_pingroup cinder_groups[] = {
	[0] = PINGROUP(0, qup20, qup21, ddr_bist, NA, NA, NA, NA, NA, NA),
	[1] = PINGROUP(1, qup20, qup21, ddr_bist, NA, NA, NA, NA, NA, NA),
	[2] = PINGROUP(2, qup21, qup20, ddr_bist, NA, tsense_pwm1, NA, NA, NA,
		       NA),
	[3] = PINGROUP(3, qup21, qup20, ddr_bist, NA, tsense_pwm2, NA, NA, NA,
		       NA),
	[4] = PINGROUP(4, qup22, NA, NA, NA, NA, NA, NA, NA, NA),
	[5] = PINGROUP(5, qup22, NA, NA, NA, NA, NA, NA, NA, NA),
	[6] = PINGROUP(6, qup00, tgu_ch0, phase_flag0, NA, qdss_gpio0, NA, NA,
		       NA, NA),
	[7] = PINGROUP(7, qup00, tgu_ch1, phase_flag1, NA, qdss_gpio1, NA, NA,
		       NA, NA),
	[8] = PINGROUP(8, qup00, tgu_ch2, phase_flag2, NA, qdss_gpio2, NA, NA,
		       NA, NA),
	[9] = PINGROUP(9, qup00, tgu_ch3, phase_flag3, NA, qdss_gpio3, NA, NA,
		       NA, NA),
	[10] = PINGROUP(10, qup01, qup02, NA, NA, NA, NA, NA, NA, NA),
	[11] = PINGROUP(11, qup01, qup02, NA, NA, NA, NA, NA, NA, NA),
	[12] = PINGROUP(12, qup02, qup01, qup17, NA, NA, NA, NA, NA, NA),
	[13] = PINGROUP(13, qup02, qup01, qup17, NA, NA, NA, NA, NA, NA),
	[14] = PINGROUP(14, qup03, qup04, qup17, NA, NA, NA, NA, NA, NA),
	[15] = PINGROUP(15, qup03, qup04, NA, NA, NA, NA, NA, NA, NA),
	[16] = PINGROUP(16, qup04, qup03, phase_flag4, NA, qdss_gpio4, NA, NA,
			NA, NA),
	[17] = PINGROUP(17, qup04, qup03, phase_flag5, NA, qdss_gpio5, NA, NA,
			NA, NA),
	[18] = PINGROUP(18, qup10, prng_rosc0, phase_flag6, NA, qdss_gpio0, NA,
			NA, NA, NA),
	[19] = PINGROUP(19, qup10, prng_rosc1, phase_flag7, NA, qdss_gpio1, NA,
			NA, NA, NA),
	[20] = PINGROUP(20, qup10, prng_rosc2, pll_bist, phase_flag8, NA,
			qdss_gpio2, NA, NA, NA),
	[21] = PINGROUP(21, qup10, prng_rosc3, phase_flag9, NA, qdss_gpio3, NA,
			NA, NA, NA),
	[22] = PINGROUP(22, qup11, qup12, phase_flag10, NA, qdss_gpio4, NA, NA,
			NA, NA),
	[23] = PINGROUP(23, qup11, qup12, phase_flag11, NA, qdss_gpio5, NA, NA,
			NA, NA),
	[24] = PINGROUP(24, qup12, qup11, phase_flag12, NA, qdss_gpio6, NA, NA,
			NA, NA),
	[25] = PINGROUP(25, qup12, qup11, phase_flag13, NA, qdss_gpio7, NA, NA,
			NA, NA),
	[26] = PINGROUP(26, qup13, qup14, intr_c0, phase_flag14, NA,
			qdss_gpio8, NA, NA, NA),
	[27] = PINGROUP(27, qup13, qup14, intr_c1, phase_flag15, NA,
			qdss_gpio9, NA, NA, NA),
	[28] = PINGROUP(28, qup14, qup13, intr_c2, phase_flag16, NA,
			qdss_gpio10, NA, NA, NA),
	[29] = PINGROUP(29, qup14, qup13, qup16, phase_flag17, NA, qdss_gpio11,
			NA, NA, NA),
	[30] = PINGROUP(30, qup17, qup15, tmess_prng3, phase_flag18, NA,
			qdss_gpio12, NA, NA, NA),
	[31] = PINGROUP(31, qup17, qup15, tmess_prng2, phase_flag19, NA,
			qdss_gpio13, NA, NA, NA),
	[32] = PINGROUP(32, qup15, tmess_prng1, phase_flag20, NA, qdss_gpio14,
			NA, NA, NA, NA),
	[33] = PINGROUP(33, qup15, tmess_prng0, phase_flag21, NA, qdss_gpio15,
			NA, NA, NA, NA),
	[34] = PINGROUP(34, qup16, qdss_gpio, NA, NA, NA, NA, NA, NA, NA),
	[35] = PINGROUP(35, qup16, qdss_gpio, NA, NA, NA, NA, NA, NA, NA),
	[36] = PINGROUP(36, qup16, qdss_cti, NA, NA, NA, NA, NA, NA, NA),
	[37] = PINGROUP(37, qup16, qdss_cti, NA, NA, NA, NA, NA, NA, NA),
	[38] = PINGROUP(38, qup16, qdss_cti, NA, NA, NA, NA, NA, NA, NA),
	[39] = PINGROUP(39, qup16, qdss_cti, NA, NA, NA, NA, NA, NA, NA),
	[40] = PINGROUP(40, qup17, qdss_cti, NA, NA, NA, NA, NA, NA, NA),
	[41] = PINGROUP(41, qup17, qdss_cti, NA, NA, NA, NA, NA, NA, NA),
	[42] = PINGROUP(42, usb_con, phase_flag22, NA, qdss_gpio6, NA, NA, NA,
			NA, NA),
	[43] = PINGROUP(43, usb_dfp, phase_flag23, NA, qdss_gpio7, NA, NA, NA,
			NA, NA),
	[44] = PINGROUP(44, SI5518_INT, tgu_ch4, NA, NA, NA, NA, NA, NA, NA),
	[45] = PINGROUP(45, tgu_ch5, NA, NA, NA, NA, NA, NA, NA, NA),
	[46] = PINGROUP(46, tgu_ch6, NA, NA, NA, NA, NA, NA, NA, NA),
	[47] = PINGROUP(47, hardsync_pps, tgu_ch7, NA, NA, NA, NA, NA, NA, NA),
	[48] = PINGROUP(48, tod_pps, qdss_cti, NA, NA, NA, NA, NA, NA, NA),
	[49] = PINGROUP(49, NA, qdss_cti, NA, NA, NA, NA, NA, NA, NA),
	[50] = PINGROUP(50, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[51] = PINGROUP(51, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[52] = PINGROUP(52, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[53] = PINGROUP(53, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[54] = PINGROUP(54, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[55] = PINGROUP(55, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[56] = PINGROUP(56, NA, qlink6_wmss, NA, NA, NA, NA, NA, NA, NA),
	[57] = PINGROUP(57, NA, qlink7_wmss, NA, NA, NA, NA, NA, NA, NA),
	[58] = PINGROUP(58, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[59] = PINGROUP(59, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[60] = PINGROUP(60, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[61] = PINGROUP(61, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[62] = PINGROUP(62, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[63] = PINGROUP(63, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[64] = PINGROUP(64, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[65] = PINGROUP(65, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[66] = PINGROUP(66, qlink0_request, NA, NA, NA, NA, NA, NA, NA, NA),
	[67] = PINGROUP(67, qlink0_enable, NA, NA, NA, NA, NA, NA, NA, NA),
	[68] = PINGROUP(68, qlink1_request, NA, NA, NA, NA, NA, NA, NA, NA),
	[69] = PINGROUP(69, qlink1_enable, NA, NA, NA, NA, NA, NA, NA, NA),
	[70] = PINGROUP(70, qlink2_request, NA, NA, NA, NA, NA, NA, NA, NA),
	[71] = PINGROUP(71, qlink2_enable, NA, NA, NA, NA, NA, NA, NA, NA),
	[72] = PINGROUP(72, qlink3_request, NA, NA, NA, NA, NA, NA, NA, NA),
	[73] = PINGROUP(73, qlink3_enable, NA, NA, NA, NA, NA, NA, NA, NA),
	[74] = PINGROUP(74, qlink4_request, NA, NA, NA, NA, NA, NA, NA, NA),
	[75] = PINGROUP(75, qlink4_enable, NA, NA, NA, NA, NA, NA, NA, NA),
	[76] = PINGROUP(76, qlink5_request, NA, NA, NA, NA, NA, NA, NA, NA),
	[77] = PINGROUP(77, qlink5_enable, NA, NA, NA, NA, NA, NA, NA, NA),
	[78] = PINGROUP(78, qlink6_request, NA, NA, NA, NA, NA, NA, NA, NA),
	[79] = PINGROUP(79, qlink6_enable, NA, NA, NA, NA, NA, NA, NA, NA),
	[80] = PINGROUP(80, qlink7_request, NA, NA, NA, NA, NA, NA, NA, NA),
	[81] = PINGROUP(81, qlink7_enable, NA, NA, NA, NA, NA, NA, NA, NA),
	[82] = PINGROUP(82, qlink0_wmss, NA, NA, NA, NA, NA, NA, NA, NA),
	[83] = PINGROUP(83, qlink1_wmss, NA, NA, NA, NA, NA, NA, NA, NA),
	[84] = PINGROUP(84, qlink4_wmss, NA, NA, NA, NA, NA, NA, NA, NA),
	[85] = PINGROUP(85, qlink5_wmss, NA, NA, NA, NA, NA, NA, NA, NA),
	[86] = PINGROUP(86, eth012_int, gcc_gp1, NA, qdss_cti, NA, NA, NA, NA,
			NA),
	[87] = PINGROUP(87, eth345_int, gcc_gp2, NA, qdss_cti, NA, NA, NA, NA,
			NA),
	[88] = PINGROUP(88, eth6_int, smb_alert, gcc_gp3, NA, qdss_gpio, NA,
			NA, NA, NA),
	[89] = PINGROUP(89, phase_flag24, cmu_rng3, NA, qdss_gpio8,
			atest_char3, NA, NA, NA, NA),
	[90] = PINGROUP(90, usb2phy_ac, phase_flag25, cmu_rng2, NA, qdss_gpio9,
			atest_char2, NA, NA, NA),
	[91] = PINGROUP(91, usb_phy, phase_flag26, cmu_rng1, NA, qdss_gpio10,
			atest_char1, NA, NA, NA),
	[92] = PINGROUP(92, phase_flag27, cmu_rng0, NA, qdss_gpio11,
			atest_char0, NA, NA, NA, NA),
	[93] = PINGROUP(93, vfr_0, qdss_cti, NA, NA, NA, NA, NA, NA, NA),
	[94] = PINGROUP(94, vfr_1, qdss_cti, NA, NA, NA, NA, NA, NA, NA),
	[95] = PINGROUP(95, phase_flag28, NA, qdss_gpio12, atest_char, NA, NA,
			NA, NA, NA),
	[96] = PINGROUP(96, phase_flag29, NA, qdss_gpio13, NA, NA, NA, NA, NA,
			NA),
	[97] = PINGROUP(97, phase_flag30, NA, qdss_gpio14, NA, NA, NA, NA, NA,
			NA),
	[98] = PINGROUP(98, pll_clk, NA, NA, NA, NA, NA, NA, NA, NA),
	[99] = PINGROUP(99, pcie0_clkreqn, char_exec, NA, NA, NA, NA, NA, NA,
			NA),
	[100] = PINGROUP(100, char_exec, NA, NA, NA, NA, NA, NA, NA, NA),
	[101] = PINGROUP(101, smb_alert, NA, NA, NA, NA, NA, NA, NA, NA),
	[102] = PINGROUP(102, phase_flag31, NA, qdss_gpio15, NA, NA, NA, NA,
			 NA, NA),
	[103] = PINGROUP(103, CMO_PRI, qdss_gpio, NA, NA, NA, NA, NA, NA, NA),
	[104] = PINGROUP(104, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[105] = PINGROUP(105, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[106] = PINGROUP(106, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[107] = PINGROUP(107, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[108] = PINGROUP(108, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[109] = PINGROUP(109, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[110] = PINGROUP(110, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[111] = PINGROUP(111, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[112] = PINGROUP(112, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[113] = PINGROUP(113, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[114] = PINGROUP(114, qspi_data, tb_trig, NA, atest_usb0, ddr_pxi0, NA,
			 NA, NA, NA),
	[115] = PINGROUP(115, qspi_data, NA, atest_usb00, ddr_pxi0, NA, NA, NA,
			 NA, NA),
	[116] = PINGROUP(116, qspi_data, NA, atest_usb01, ddr_pxi1, NA, NA, NA,
			 NA, NA),
	[117] = PINGROUP(117, qspi_data, NA, atest_usb02, ddr_pxi1, NA, NA, NA,
			 NA, NA),
	[118] = PINGROUP(118, NA, atest_usb03, ddr_pxi2, NA, NA, NA, NA, NA,
			 NA),
	[119] = PINGROUP(119, NA, NA, ddr_pxi2, NA, NA, NA, NA, NA, NA),
	[120] = PINGROUP(120, NA, NA, ddr_pxi3, NA, NA, NA, NA, NA, NA),
	[121] = PINGROUP(121, NA, ddr_pxi3, NA, NA, NA, NA, NA, NA, NA),
	[122] = PINGROUP(122, NA, ddr_pxi4, NA, NA, NA, NA, NA, NA, NA),
	[123] = PINGROUP(123, NA, ddr_pxi4, NA, NA, NA, NA, NA, NA, NA),
	[124] = PINGROUP(124, NA, ddr_pxi5, NA, NA, NA, NA, NA, NA, NA),
	[125] = PINGROUP(125, qspi_cs, NA, ddr_pxi5, NA, NA, NA, NA, NA, NA),
	[126] = PINGROUP(126, qspi_clk, NA, ddr_pxi6, NA, NA, NA, NA, NA, NA),
	[127] = PINGROUP(127, NA, ddr_pxi6, NA, NA, NA, NA, NA, NA, NA),
	[128] = PINGROUP(128, qup22, NA, ddr_pxi7, NA, NA, NA, NA, NA, NA),
	[129] = PINGROUP(129, qup22, ddr_pxi7, NA, NA, NA, NA, NA, NA, NA),
	[130] = PINGROUP(130, qup05, qup06, jitter_bist, qdss_cti, NA, NA, NA,
			 NA, NA),
	[131] = PINGROUP(131, qup05, qup06, qdss_cti, NA, NA, NA, NA, NA, NA),
	[132] = PINGROUP(132, qup06, qup05, smb_dat, qdss_cti, NA, NA, NA, NA,
			 NA),
	[133] = PINGROUP(133, qup06, qup05, smb_clk, qdss_cti, NA, NA, NA, NA,
			 NA),
	[134] = PINGROUP(134, qup07, qup07, gcc_gp1, NA, qdss_cti, NA, NA, NA,
			 NA),
	[135] = PINGROUP(135, qup07, qup07, gcc_gp2, NA, qdss_cti,
			 vsense_trigger, NA, NA, NA),
	[136] = PINGROUP(136, gcc_gp3, dbg_out, NA, NA, NA, NA, NA, NA, NA),
	[137] = PINGROUP(137, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[138] = PINGROUP(138, qlink2_wmss, NA, NA, NA, NA, NA, NA, NA, NA),
	[139] = PINGROUP(139, qlink3_wmss, NA, NA, NA, NA, NA, NA, NA, NA),
	[140] = PINGROUP(140, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[141] = PINGROUP(141, intr_c0, NA, NA, NA, NA, NA, NA, NA, NA),
	[142] = PINGROUP(142, intr_c1, NA, NA, NA, NA, NA, NA, NA, NA),
	[143] = PINGROUP(143, intr_c2, NA, NA, NA, NA, NA, NA, NA, NA),
	[144] = PINGROUP(144, qdss_cti, NA, NA, NA, NA, NA, NA, NA, NA),
	[145] = PINGROUP(145, qdss_cti, NA, NA, NA, NA, NA, NA, NA, NA),
	[146] = PINGROUP(146, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[147] = PINGROUP(147, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[148] = PINGROUP(148, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[149] = PINGROUP(149, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[150] = PINGROUP(150, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	[151] = SDC_QDSD_PINGROUP(sdc1_rclk, 0x9e000, 0, 0),
	[152] = SDC_QDSD_PINGROUP(sdc1_clk, 0x9d000, 13, 6),
	[153] = SDC_QDSD_PINGROUP(sdc1_cmd, 0x9d000, 11, 3),
	[154] = SDC_QDSD_PINGROUP(sdc1_data, 0x9d000, 9, 0),
};
