/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#define QUP_I3C_0_SE0_MODE_OFFSET	0xA4000
#define QUP_I3C_0_SE1_MODE_OFFSET	0xA5000
#define QUP_I3C_1_SE0_MODE_OFFSET	0xA6000
#define QUP_I3C_1_SE1_MODE_OFFSET	0xA7000

static const struct pinctrl_pin_desc parrot_pins[] = {
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
	PINCTRL_PIN(141, "UFS_RESET"),
	PINCTRL_PIN(142, "SDC1_RCLK"),
	PINCTRL_PIN(143, "SDC1_CLK"),
	PINCTRL_PIN(144, "SDC1_CMD"),
	PINCTRL_PIN(145, "SDC1_DATA"),
	PINCTRL_PIN(146, "SDC2_CLK"),
	PINCTRL_PIN(147, "SDC2_CMD"),
	PINCTRL_PIN(148, "SDC2_DATA"),
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

static const unsigned int ufs_reset_pins[] = { 141 };
static const unsigned int sdc1_rclk_pins[] = { 142 };
static const unsigned int sdc1_clk_pins[] = { 143 };
static const unsigned int sdc1_cmd_pins[] = { 144 };
static const unsigned int sdc1_data_pins[] = { 145 };
static const unsigned int sdc2_clk_pins[] = { 146 };
static const unsigned int sdc2_cmd_pins[] = { 147 };
static const unsigned int sdc2_data_pins[] = { 148 };

enum parrot_functions {
	msm_mux_gpio,
	msm_mux_PCIE0_CLK_REQ,
	msm_mux_QSPI0,
	msm_mux_QSPI1,
	msm_mux_QSPI2,
	msm_mux_QSPI3,
	msm_mux_QSPI_CLK,
	msm_mux_QSPI_CS_N,
	msm_mux_atest_char_start,
	msm_mux_atest_char_status0,
	msm_mux_atest_char_status1,
	msm_mux_atest_char_status2,
	msm_mux_atest_char_status3,
	msm_mux_atest_usb0_atereset,
	msm_mux_atest_usb0_testdataout00,
	msm_mux_atest_usb0_testdataout01,
	msm_mux_atest_usb0_testdataout02,
	msm_mux_atest_usb0_testdataout03,
	msm_mux_audio_ref_clk,
	msm_mux_cam_mclk,
	msm_mux_cci_async_in0,
	msm_mux_cci_i2c_scl0,
	msm_mux_cci_i2c_scl1,
	msm_mux_cci_i2c_scl2,
	msm_mux_cci_i2c_scl3,
	msm_mux_cci_i2c_sda0,
	msm_mux_cci_i2c_sda1,
	msm_mux_cci_i2c_sda2,
	msm_mux_cci_i2c_sda3,
	msm_mux_cci_timer0,
	msm_mux_cci_timer1,
	msm_mux_cci_timer2,
	msm_mux_cci_timer3,
	msm_mux_cmu_rng_entropy0,
	msm_mux_cmu_rng_entropy1,
	msm_mux_cmu_rng_entropy2,
	msm_mux_cmu_rng_entropy3,
	msm_mux_coex_uart1_rx,
	msm_mux_coex_uart1_tx,
	msm_mux_cri_trng_rosc,
	msm_mux_cri_trng_rosc0,
	msm_mux_cri_trng_rosc1,
	msm_mux_dbg_out_clk,
	msm_mux_ddr_bist_complete,
	msm_mux_ddr_bist_fail,
	msm_mux_ddr_bist_start,
	msm_mux_ddr_bist_stop,
	msm_mux_ddr_pxi0_test,
	msm_mux_ddr_pxi1_test,
	msm_mux_dp0_hot_plug,
	msm_mux_gcc_gp1_clk,
	msm_mux_gcc_gp2_clk,
	msm_mux_gcc_gp3_clk,
	msm_mux_host2wlan_sol_mira,
	msm_mux_host2wlan_sol_mirb,
	msm_mux_i2s0_data0,
	msm_mux_i2s0_data1,
	msm_mux_i2s0_sck,
	msm_mux_i2s0_ws,
	msm_mux_ibi_i3c_qup0,
	msm_mux_ibi_i3c_qup1,
	msm_mux_jitter_bist_ref,
	msm_mux_mdp_vsync0_out,
	msm_mux_mdp_vsync1_out,
	msm_mux_mdp_vsync2_out,
	msm_mux_mdp_vsync3_out,
	msm_mux_mdp_vsync_e,
	msm_mux_mdp_vsync_p,
	msm_mux_mdp_vsync_s,
	msm_mux_nav_gpio0,
	msm_mux_nav_gpio1,
	msm_mux_nav_gpio2,
	msm_mux_phase_flag_status0,
	msm_mux_phase_flag_status1,
	msm_mux_phase_flag_status10,
	msm_mux_phase_flag_status11,
	msm_mux_phase_flag_status12,
	msm_mux_phase_flag_status13,
	msm_mux_phase_flag_status14,
	msm_mux_phase_flag_status15,
	msm_mux_phase_flag_status16,
	msm_mux_phase_flag_status17,
	msm_mux_phase_flag_status18,
	msm_mux_phase_flag_status19,
	msm_mux_phase_flag_status2,
	msm_mux_phase_flag_status20,
	msm_mux_phase_flag_status21,
	msm_mux_phase_flag_status22,
	msm_mux_phase_flag_status23,
	msm_mux_phase_flag_status24,
	msm_mux_phase_flag_status25,
	msm_mux_phase_flag_status26,
	msm_mux_phase_flag_status27,
	msm_mux_phase_flag_status28,
	msm_mux_phase_flag_status29,
	msm_mux_phase_flag_status3,
	msm_mux_phase_flag_status30,
	msm_mux_phase_flag_status31,
	msm_mux_phase_flag_status4,
	msm_mux_phase_flag_status5,
	msm_mux_phase_flag_status6,
	msm_mux_phase_flag_status7,
	msm_mux_phase_flag_status8,
	msm_mux_phase_flag_status9,
	msm_mux_pll_bist_sync,
	msm_mux_pll_clk_aux,
	msm_mux_prng_rosc_test0,
	msm_mux_prng_rosc_test1,
	msm_mux_prng_rosc_test2,
	msm_mux_prng_rosc_test3,
	msm_mux_qdss_cti_trig0,
	msm_mux_qdss_cti_trig1,
	msm_mux_qdss_gpio_traceclk,
	msm_mux_qdss_gpio_tracectl,
	msm_mux_qdss_gpio_tracedata0,
	msm_mux_qdss_gpio_tracedata1,
	msm_mux_qdss_gpio_tracedata10,
	msm_mux_qdss_gpio_tracedata11,
	msm_mux_qdss_gpio_tracedata12,
	msm_mux_qdss_gpio_tracedata13,
	msm_mux_qdss_gpio_tracedata14,
	msm_mux_qdss_gpio_tracedata15,
	msm_mux_qdss_gpio_tracedata2,
	msm_mux_qdss_gpio_tracedata3,
	msm_mux_qdss_gpio_tracedata4,
	msm_mux_qdss_gpio_tracedata5,
	msm_mux_qdss_gpio_tracedata6,
	msm_mux_qdss_gpio_tracedata7,
	msm_mux_qdss_gpio_tracedata8,
	msm_mux_qdss_gpio_tracedata9,
	msm_mux_qlink0_enable,
	msm_mux_qlink0_request,
	msm_mux_qlink0_wmss_reset,
	msm_mux_qlink1_enable,
	msm_mux_qlink1_request,
	msm_mux_qlink1_wmss_reset,
	msm_mux_qup0_se0_l0,
	msm_mux_qup0_se0_l1,
	msm_mux_qup0_se0_l2,
	msm_mux_qup0_se0_l3,
	msm_mux_qup0_se1_l0,
	msm_mux_qup0_se1_l1,
	msm_mux_qup0_se1_l2,
	msm_mux_qup0_se1_l3,
	msm_mux_qup0_se1_l4,
	msm_mux_qup0_se2_l0,
	msm_mux_qup0_se2_l1,
	msm_mux_qup0_se2_l2,
	msm_mux_qup0_se2_l3,
	msm_mux_qup0_se3_l0,
	msm_mux_qup0_se3_l1,
	msm_mux_qup0_se3_l2,
	msm_mux_qup0_se3_l3,
	msm_mux_qup0_se4_l0,
	msm_mux_qup0_se4_l1,
	msm_mux_qup0_se4_l2,
	msm_mux_qup0_se4_l3,
	msm_mux_qup0_se5_l0,
	msm_mux_qup0_se5_l1,
	msm_mux_qup0_se5_l2,
	msm_mux_qup0_se5_l3,
	msm_mux_qup0_se5_l4,
	msm_mux_qup0_se5_l5,
	msm_mux_qup0_se5_l6,
	msm_mux_qup1_se0_l0,
	msm_mux_qup1_se0_l1,
	msm_mux_qup1_se0_l2,
	msm_mux_qup1_se0_l3,
	msm_mux_qup1_se1_l0,
	msm_mux_qup1_se1_l1,
	msm_mux_qup1_se1_l2,
	msm_mux_qup1_se1_l3,
	msm_mux_qup1_se2_l0,
	msm_mux_qup1_se2_l1,
	msm_mux_qup1_se2_l2,
	msm_mux_qup1_se2_l3,
	msm_mux_qup1_se2_l4,
	msm_mux_qup1_se3_l0,
	msm_mux_qup1_se3_l1,
	msm_mux_qup1_se3_l2,
	msm_mux_qup1_se3_l3,
	msm_mux_qup1_se5_l0,
	msm_mux_qup1_se5_l1,
	msm_mux_qup1_se5_l2,
	msm_mux_qup1_se5_l3,
	msm_mux_sd_write_protect,
	msm_mux_tb_trig_sdc1,
	msm_mux_tb_trig_sdc2,
	msm_mux_tgu_ch0_trigout,
	msm_mux_tgu_ch1_trigout,
	msm_mux_tgu_ch2_trigout,
	msm_mux_tgu_ch3_trigout,
	msm_mux_tmess_prng_rosc0,
	msm_mux_tmess_prng_rosc1,
	msm_mux_tmess_prng_rosc2,
	msm_mux_tmess_prng_rosc3,
	msm_mux_tsense_pwm1_out,
	msm_mux_tsense_pwm2_out,
	msm_mux_uim0_clk,
	msm_mux_uim0_data,
	msm_mux_uim0_present,
	msm_mux_uim0_reset,
	msm_mux_uim1_clk,
	msm_mux_uim1_data,
	msm_mux_uim1_present,
	msm_mux_uim1_reset,
	msm_mux_usb0_hs_ac,
	msm_mux_usb0_phy_ps,
	msm_mux_vfr_0_mira,
	msm_mux_vfr_0_mirb,
	msm_mux_vfr_1,
	msm_mux_vsense_trigger_mirnat,
	msm_mux_wlan1_adc_dtest0,
	msm_mux_wlan1_adc_dtest1,
	msm_mux_wlan2_adc_dtest0,
	msm_mux_wlan2_adc_dtest1,
	msm_mux_NA,
};

static const char * const gpio_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3", "gpio4", "gpio5", "gpio6", "gpio7",
	"gpio8", "gpio9", "gpio10", "gpio11", "gpio12", "gpio13", "gpio14",
	"gpio15", "gpio16", "gpio17", "gpio18", "gpio19", "gpio20", "gpio21",
	"gpio22", "gpio23", "gpio24", "gpio25", "gpio26", "gpio26", "gpio27",
	"gpio27", "gpio28", "gpio29", "gpio30", "gpio31", "gpio32", "gpio33",
	"gpio34", "gpio35", "gpio36", "gpio37", "gpio38", "gpio39", "gpio40",
	"gpio41", "gpio42", "gpio43", "gpio44", "gpio45", "gpio46", "gpio47",
	"gpio48", "gpio49", "gpio50", "gpio51", "gpio52", "gpio53", "gpio54",
	"gpio55", "gpio56", "gpio57", "gpio58", "gpio59", "gpio60", "gpio61",
	"gpio62", "gpio63", "gpio64", "gpio65", "gpio66", "gpio67", "gpio68",
	"gpio69", "gpio70", "gpio71", "gpio72", "gpio73", "gpio74", "gpio75",
	"gpio76", "gpio77", "gpio78", "gpio79", "gpio80", "gpio81", "gpio82",
	"gpio83", "gpio84", "gpio85", "gpio86", "gpio87", "gpio88", "gpio89",
	"gpio90", "gpio91", "gpio92", "gpio93", "gpio94", "gpio95", "gpio96",
	"gpio97", "gpio98", "gpio99", "gpio100", "gpio101", "gpio102",
	"gpio103", "gpio104", "gpio105", "gpio106", "gpio107", "gpio108",
	"gpio109", "gpio110", "gpio111", "gpio112", "gpio113", "gpio114",
	"gpio115", "gpio116", "gpio117", "gpio118", "gpio119", "gpio120",
	"gpio121", "gpio122", "gpio123", "gpio124", "gpio125", "gpio126",
	"gpio127", "gpio128", "gpio129", "gpio130", "gpio131", "gpio132",
	"gpio133", "gpio134", "gpio135", "gpio136", "gpio137", "gpio138",
	"gpio139", "gpio140",
};
static const char * const PCIE0_CLK_REQ_groups[] = {
	"gpio54", "gpio113",
};
static const char * const QSPI0_groups[] = {
	"gpio32",
};
static const char * const QSPI1_groups[] = {
	"gpio33",
};
static const char * const QSPI2_groups[] = {
	"gpio36",
};
static const char * const QSPI3_groups[] = {
	"gpio37",
};
static const char * const QSPI_CLK_groups[] = {
	"gpio34",
};
static const char * const QSPI_CS_N_groups[] = {
	"gpio35", "gpio38",
};
static const char * const atest_char_start_groups[] = {
	"gpio71",
};
static const char * const atest_char_status0_groups[] = {
	"gpio47",
};
static const char * const atest_char_status1_groups[] = {
	"gpio48",
};
static const char * const atest_char_status2_groups[] = {
	"gpio57",
};
static const char * const atest_char_status3_groups[] = {
	"gpio58",
};
static const char * const atest_usb0_atereset_groups[] = {
	"gpio63",
};
static const char * const atest_usb0_testdataout00_groups[] = {
	"gpio66",
};
static const char * const atest_usb0_testdataout01_groups[] = {
	"gpio3",
};
static const char * const atest_usb0_testdataout02_groups[] = {
	"gpio31",
};
static const char * const atest_usb0_testdataout03_groups[] = {
	"gpio62",
};
static const char * const audio_ref_clk_groups[] = {
	"gpio76",
};
static const char * const cam_mclk_groups[] = {
	"gpio39", "gpio40", "gpio41", "gpio42", "gpio43",
};
static const char * const cci_async_in0_groups[] = {
	"gpio44",
};
static const char * const cci_i2c_scl0_groups[] = {
	"gpio50",
};
static const char * const cci_i2c_scl1_groups[] = {
	"gpio52",
};
static const char * const cci_i2c_scl2_groups[] = {
	"gpio54",
};
static const char * const cci_i2c_scl3_groups[] = {
	"gpio56",
};
static const char * const cci_i2c_sda0_groups[] = {
	"gpio49",
};
static const char * const cci_i2c_sda1_groups[] = {
	"gpio51",
};
static const char * const cci_i2c_sda2_groups[] = {
	"gpio53",
};
static const char * const cci_i2c_sda3_groups[] = {
	"gpio55",
};
static const char * const cci_timer0_groups[] = {
	"gpio44",
};
static const char * const cci_timer1_groups[] = {
	"gpio45",
};
static const char * const cci_timer2_groups[] = {
	"gpio46",
};
static const char * const cci_timer3_groups[] = {
	"gpio47",
};
static const char * const cmu_rng_entropy0_groups[] = {
	"gpio29",
};
static const char * const cmu_rng_entropy1_groups[] = {
	"gpio3",
};
static const char * const cmu_rng_entropy2_groups[] = {
	"gpio1",
};
static const char * const cmu_rng_entropy3_groups[] = {
	"gpio0",
};
static const char * const coex_uart1_rx_groups[] = {
	"gpio59",
};
static const char * const coex_uart1_tx_groups[] = {
	"gpio60",
};
static const char * const cri_trng_rosc_groups[] = {
	"gpio46",
};
static const char * const cri_trng_rosc0_groups[] = {
	"gpio44",
};
static const char * const cri_trng_rosc1_groups[] = {
	"gpio45",
};
static const char * const dbg_out_clk_groups[] = {
	"gpio46",
};
static const char * const ddr_bist_complete_groups[] = {
	"gpio33",
};
static const char * const ddr_bist_fail_groups[] = {
	"gpio30",
};
static const char * const ddr_bist_start_groups[] = {
	"gpio31",
};
static const char * const ddr_bist_stop_groups[] = {
	"gpio32",
};
static const char * const ddr_pxi0_test_groups[] = {
	"gpio15", "gpio17",
};
static const char * const ddr_pxi1_test_groups[] = {
	"gpio64", "gpio65",
};
static const char * const dp0_hot_plug_groups[] = {
	"gpio31", "gpio112",
};
static const char * const gcc_gp1_clk_groups[] = {
	"gpio45", "gpio53",
};
static const char * const gcc_gp2_clk_groups[] = {
	"gpio31", "gpio54",
};
static const char * const gcc_gp3_clk_groups[] = {
	"gpio3", "gpio55",
};
static const char * const host2wlan_sol_mira_groups[] = {
	"gpio42",
};
static const char * const host2wlan_sol_mirb_groups[] = {
	"gpio112",
};
static const char * const i2s0_data0_groups[] = {
	"gpio101",
};
static const char * const i2s0_data1_groups[] = {
	"gpio111",
};
static const char * const i2s0_sck_groups[] = {
	"gpio98",
};
static const char * const i2s0_ws_groups[] = {
	"gpio99",
};
static const char * const ibi_i3c_qup0_groups[] = {
	"gpio0", "gpio1", "gpio28", "gpio29",
};
static const char * const ibi_i3c_qup1_groups[] = {
	"gpio4", "gpio5", "gpio8", "gpio9",
};
static const char * const jitter_bist_ref_groups[] = {
	"gpio65",
};
static const char * const mdp_vsync0_out_groups[] = {
	"gpio99",
};
static const char * const mdp_vsync1_out_groups[] = {
	"gpio99",
};
static const char * const mdp_vsync2_out_groups[] = {
	"gpio48",
};
static const char * const mdp_vsync3_out_groups[] = {
	"gpio48",
};
static const char * const mdp_vsync_e_groups[] = {
	"gpio26", "gpio37",
};
static const char * const mdp_vsync_p_groups[] = {
	"gpio99", "gpio103",
};
static const char * const mdp_vsync_s_groups[] = {
	"gpio35", "gpio48",
};
static const char * const nav_gpio0_groups[] = {
	"gpio88",
};
static const char * const nav_gpio1_groups[] = {
	"gpio89",
};
static const char * const nav_gpio2_groups[] = {
	"gpio86",
};
static const char * const phase_flag_status0_groups[] = {
	"gpio83",
};
static const char * const phase_flag_status1_groups[] = {
	"gpio86",
};
static const char * const phase_flag_status10_groups[] = {
	"gpio84",
};
static const char * const phase_flag_status11_groups[] = {
	"gpio85",
};
static const char * const phase_flag_status12_groups[] = {
	"gpio13",
};
static const char * const phase_flag_status13_groups[] = {
	"gpio137",
};
static const char * const phase_flag_status14_groups[] = {
	"gpio4",
};
static const char * const phase_flag_status15_groups[] = {
	"gpio108",
};
static const char * const phase_flag_status16_groups[] = {
	"gpio21",
};
static const char * const phase_flag_status17_groups[] = {
	"gpio47",
};
static const char * const phase_flag_status18_groups[] = {
	"gpio75",
};
static const char * const phase_flag_status19_groups[] = {
	"gpio107",
};
static const char * const phase_flag_status2_groups[] = {
	"gpio5",
};
static const char * const phase_flag_status20_groups[] = {
	"gpio101",
};
static const char * const phase_flag_status21_groups[] = {
	"gpio9",
};
static const char * const phase_flag_status22_groups[] = {
	"gpio138",
};
static const char * const phase_flag_status23_groups[] = {
	"gpio7",
};
static const char * const phase_flag_status24_groups[] = {
	"gpio71",
};
static const char * const phase_flag_status25_groups[] = {
	"gpio44",
};
static const char * const phase_flag_status26_groups[] = {
	"gpio25",
};
static const char * const phase_flag_status27_groups[] = {
	"gpio57",
};
static const char * const phase_flag_status28_groups[] = {
	"gpio58",
};
static const char * const phase_flag_status29_groups[] = {
	"gpio48",
};
static const char * const phase_flag_status3_groups[] = {
	"gpio18",
};
static const char * const phase_flag_status30_groups[] = {
	"gpio24",
};
static const char * const phase_flag_status31_groups[] = {
	"gpio102",
};
static const char * const phase_flag_status4_groups[] = {
	"gpio11",
};
static const char * const phase_flag_status5_groups[] = {
	"gpio14",
};
static const char * const phase_flag_status6_groups[] = {
	"gpio10",
};
static const char * const phase_flag_status7_groups[] = {
	"gpio19",
};
static const char * const phase_flag_status8_groups[] = {
	"gpio81",
};
static const char * const phase_flag_status9_groups[] = {
	"gpio82",
};
static const char * const pll_bist_sync_groups[] = {
	"gpio78",
};
static const char * const pll_clk_aux_groups[] = {
	"gpio77",
};
static const char * const prng_rosc_test0_groups[] = {
	"gpio40",
};
static const char * const prng_rosc_test1_groups[] = {
	"gpio41",
};
static const char * const prng_rosc_test2_groups[] = {
	"gpio42",
};
static const char * const prng_rosc_test3_groups[] = {
	"gpio43",
};
static const char * const qdss_cti_trig0_groups[] = {
	"gpio0", "gpio26", "gpio118", "gpio119",
};
static const char * const qdss_cti_trig1_groups[] = {
	"gpio3", "gpio27", "gpio62", "gpio63",
};
static const char * const qdss_gpio_traceclk_groups[] = {
	"gpio52", "gpio123",
};
static const char * const qdss_gpio_tracectl_groups[] = {
	"gpio53", "gpio122",
};
static const char * const qdss_gpio_tracedata0_groups[] = {
	"gpio4", "gpio39",
};
static const char * const qdss_gpio_tracedata1_groups[] = {
	"gpio5", "gpio40",
};
static const char * const qdss_gpio_tracedata10_groups[] = {
	"gpio56", "gpio132",
};
static const char * const qdss_gpio_tracedata11_groups[] = {
	"gpio44", "gpio133",
};
static const char * const qdss_gpio_tracedata12_groups[] = {
	"gpio45", "gpio134",
};
static const char * const qdss_gpio_tracedata13_groups[] = {
	"gpio46", "gpio135",
};
static const char * const qdss_gpio_tracedata14_groups[] = {
	"gpio57", "gpio136",
};
static const char * const qdss_gpio_tracedata15_groups[] = {
	"gpio58", "gpio138",
};
static const char * const qdss_gpio_tracedata2_groups[] = {
	"gpio7", "gpio41",
};
static const char * const qdss_gpio_tracedata3_groups[] = {
	"gpio8", "gpio42",
};
static const char * const qdss_gpio_tracedata4_groups[] = {
	"gpio9", "gpio43",
};
static const char * const qdss_gpio_tracedata5_groups[] = {
	"gpio49", "gpio120",
};
static const char * const qdss_gpio_tracedata6_groups[] = {
	"gpio50", "gpio121",
};
static const char * const qdss_gpio_tracedata7_groups[] = {
	"gpio51", "gpio126",
};
static const char * const qdss_gpio_tracedata8_groups[] = {
	"gpio54", "gpio127",
};
static const char * const qdss_gpio_tracedata9_groups[] = {
	"gpio55", "gpio131",
};
static const char * const qlink0_enable_groups[] = {
	"gpio93",
};
static const char * const qlink0_request_groups[] = {
	"gpio92",
};
static const char * const qlink0_wmss_reset_groups[] = {
	"gpio94",
};
static const char * const qlink1_enable_groups[] = {
	"gpio96",
};
static const char * const qlink1_request_groups[] = {
	"gpio95",
};
static const char * const qlink1_wmss_reset_groups[] = {
	"gpio97",
};
static const char * const qup0_se0_l0_groups[] = {
	"gpio28",
};
static const char * const qup0_se0_l1_groups[] = {
	"gpio29",
};
static const char * const qup0_se0_l2_groups[] = {
	"gpio30",
};
static const char * const qup0_se0_l3_groups[] = {
	"gpio31",
};
static const char * const qup0_se1_l0_groups[] = {
	"gpio0",
};
static const char * const qup0_se1_l1_groups[] = {
	"gpio1",
};
static const char * const qup0_se1_l2_groups[] = {
	"gpio2",
};
static const char * const qup0_se1_l3_groups[] = {
	"gpio3",
};
static const char * const qup0_se1_l4_groups[] = {
	"gpio38",
};
static const char * const qup0_se2_l0_groups[] = {
	"gpio24",
};
static const char * const qup0_se2_l1_groups[] = {
	"gpio25",
};
static const char * const qup0_se2_l2_groups[] = {
	"gpio34",
};
static const char * const qup0_se2_l3_groups[] = {
	"gpio35",
};
static const char * const qup0_se3_l0_groups[] = {
	"gpio37",
};
static const char * const qup0_se3_l1_groups[] = {
	"gpio36",
};
static const char * const qup0_se3_l2_groups[] = {
	"gpio22",
};
static const char * const qup0_se3_l3_groups[] = {
	"gpio23",
};
static const char * const qup0_se4_l0_groups[] = {
	"gpio26",
};
static const char * const qup0_se4_l1_groups[] = {
	"gpio27",
};
static const char * const qup0_se4_l2_groups[] = {
	"gpio34",
};
static const char * const qup0_se4_l3_groups[] = {
	"gpio35",
};
static const char * const qup0_se5_l0_groups[] = {
	"gpio32",
};
static const char * const qup0_se5_l1_groups[] = {
	"gpio33",
};
static const char * const qup0_se5_l2_groups[] = {
	"gpio34",
};
static const char * const qup0_se5_l3_groups[] = {
	"gpio35",
};
static const char * const qup0_se5_l4_groups[] = {
	"gpio36",
};
static const char * const qup0_se5_l5_groups[] = {
	"gpio37",
};
static const char * const qup0_se5_l6_groups[] = {
	"gpio38",
};
static const char * const qup1_se0_l0_groups[] = {
	"gpio4",
};
static const char * const qup1_se0_l1_groups[] = {
	"gpio5",
};
static const char * const qup1_se0_l2_groups[] = {
	"gpio6",
};
static const char * const qup1_se0_l3_groups[] = {
	"gpio7",
};
static const char * const qup1_se1_l0_groups[] = {
	"gpio8",
};
static const char * const qup1_se1_l1_groups[] = {
	"gpio9",
};
static const char * const qup1_se1_l2_groups[] = {
	"gpio6",
};
static const char * const qup1_se1_l3_groups[] = {
	"gpio7",
};
static const char * const qup1_se2_l0_groups[] = {
	"gpio18",
};
static const char * const qup1_se2_l1_groups[] = {
	"gpio19",
};
static const char * const qup1_se2_l2_groups[] = {
	"gpio20",
};
static const char * const qup1_se2_l3_groups[] = {
	"gpio21",
};
static const char * const qup1_se2_l4_groups[] = {
	"gpio98",
};
static const char * const qup1_se3_l0_groups[] = {
	"gpio10",
};
static const char * const qup1_se3_l1_groups[] = {
	"gpio11",
};
static const char * const qup1_se3_l2_groups[] = {
	"gpio12",
};
static const char * const qup1_se3_l3_groups[] = {
	"gpio13",
};
static const char * const qup1_se5_l0_groups[] = {
	"gpio14",
};
static const char * const qup1_se5_l1_groups[] = {
	"gpio15",
};
static const char * const qup1_se5_l2_groups[] = {
	"gpio16",
};
static const char * const qup1_se5_l3_groups[] = {
	"gpio17",
};
static const char * const sd_write_protect_groups[] = {
	"gpio108",
};
static const char * const tb_trig_sdc1_groups[] = {
	"gpio36",
};
static const char * const tb_trig_sdc2_groups[] = {
	"gpio75",
};
static const char * const tgu_ch0_trigout_groups[] = {
	"gpio20",
};
static const char * const tgu_ch1_trigout_groups[] = {
	"gpio21",
};
static const char * const tgu_ch2_trigout_groups[] = {
	"gpio22",
};
static const char * const tgu_ch3_trigout_groups[] = {
	"gpio23",
};
static const char * const tmess_prng_rosc0_groups[] = {
	"gpio62",
};
static const char * const tmess_prng_rosc1_groups[] = {
	"gpio63",
};
static const char * const tmess_prng_rosc2_groups[] = {
	"gpio64",
};
static const char * const tmess_prng_rosc3_groups[] = {
	"gpio65",
};
static const char * const tsense_pwm1_out_groups[] = {
	"gpio98",
};
static const char * const tsense_pwm2_out_groups[] = {
	"gpio98",
};
static const char * const uim0_clk_groups[] = {
	"gpio69",
};
static const char * const uim0_data_groups[] = {
	"gpio68",
};
static const char * const uim0_present_groups[] = {
	"gpio71",
};
static const char * const uim0_reset_groups[] = {
	"gpio70",
};
static const char * const uim1_clk_groups[] = {
	"gpio73",
};
static const char * const uim1_data_groups[] = {
	"gpio72",
};
static const char * const uim1_present_groups[] = {
	"gpio75",
};
static const char * const uim1_reset_groups[] = {
	"gpio74",
};
static const char * const usb0_hs_ac_groups[] = {
	"gpio105",
};
static const char * const usb0_phy_ps_groups[] = {
	"gpio100",
};
static const char * const vfr_0_mira_groups[] = {
	"gpio19",
};
static const char * const vfr_0_mirb_groups[] = {
	"gpio106",
};
static const char * const vfr_1_groups[] = {
	"gpio89",
};
static const char * const vsense_trigger_mirnat_groups[] = {
	"gpio63",
};
static const char * const wlan1_adc_dtest0_groups[] = {
	"gpio3",
};
static const char * const wlan1_adc_dtest1_groups[] = {
	"gpio62",
};
static const char * const wlan2_adc_dtest0_groups[] = {
	"gpio31",
};
static const char * const wlan2_adc_dtest1_groups[] = {
	"gpio66",
};

static const struct msm_function parrot_functions[] = {
	FUNCTION(gpio),
	FUNCTION(PCIE0_CLK_REQ),
	FUNCTION(QSPI0),
	FUNCTION(QSPI1),
	FUNCTION(QSPI2),
	FUNCTION(QSPI3),
	FUNCTION(QSPI_CLK),
	FUNCTION(QSPI_CS_N),
	FUNCTION(atest_char_start),
	FUNCTION(atest_char_status0),
	FUNCTION(atest_char_status1),
	FUNCTION(atest_char_status2),
	FUNCTION(atest_char_status3),
	FUNCTION(atest_usb0_atereset),
	FUNCTION(atest_usb0_testdataout00),
	FUNCTION(atest_usb0_testdataout01),
	FUNCTION(atest_usb0_testdataout02),
	FUNCTION(atest_usb0_testdataout03),
	FUNCTION(audio_ref_clk),
	FUNCTION(cam_mclk),
	FUNCTION(cci_async_in0),
	FUNCTION(cci_i2c_scl0),
	FUNCTION(cci_i2c_scl1),
	FUNCTION(cci_i2c_scl2),
	FUNCTION(cci_i2c_scl3),
	FUNCTION(cci_i2c_sda0),
	FUNCTION(cci_i2c_sda1),
	FUNCTION(cci_i2c_sda2),
	FUNCTION(cci_i2c_sda3),
	FUNCTION(cci_timer0),
	FUNCTION(cci_timer1),
	FUNCTION(cci_timer2),
	FUNCTION(cci_timer3),
	FUNCTION(cmu_rng_entropy0),
	FUNCTION(cmu_rng_entropy1),
	FUNCTION(cmu_rng_entropy2),
	FUNCTION(cmu_rng_entropy3),
	FUNCTION(coex_uart1_rx),
	FUNCTION(coex_uart1_tx),
	FUNCTION(cri_trng_rosc),
	FUNCTION(cri_trng_rosc0),
	FUNCTION(cri_trng_rosc1),
	FUNCTION(dbg_out_clk),
	FUNCTION(ddr_bist_complete),
	FUNCTION(ddr_bist_fail),
	FUNCTION(ddr_bist_start),
	FUNCTION(ddr_bist_stop),
	FUNCTION(ddr_pxi0_test),
	FUNCTION(ddr_pxi1_test),
	FUNCTION(dp0_hot_plug),
	FUNCTION(gcc_gp1_clk),
	FUNCTION(gcc_gp2_clk),
	FUNCTION(gcc_gp3_clk),
	FUNCTION(host2wlan_sol_mira),
	FUNCTION(host2wlan_sol_mirb),
	FUNCTION(i2s0_data0),
	FUNCTION(i2s0_data1),
	FUNCTION(i2s0_sck),
	FUNCTION(i2s0_ws),
	FUNCTION(ibi_i3c_qup0),
	FUNCTION(ibi_i3c_qup1),
	FUNCTION(jitter_bist_ref),
	FUNCTION(mdp_vsync0_out),
	FUNCTION(mdp_vsync1_out),
	FUNCTION(mdp_vsync2_out),
	FUNCTION(mdp_vsync3_out),
	FUNCTION(mdp_vsync_e),
	FUNCTION(mdp_vsync_p),
	FUNCTION(mdp_vsync_s),
	FUNCTION(nav_gpio0),
	FUNCTION(nav_gpio1),
	FUNCTION(nav_gpio2),
	FUNCTION(phase_flag_status0),
	FUNCTION(phase_flag_status1),
	FUNCTION(phase_flag_status10),
	FUNCTION(phase_flag_status11),
	FUNCTION(phase_flag_status12),
	FUNCTION(phase_flag_status13),
	FUNCTION(phase_flag_status14),
	FUNCTION(phase_flag_status15),
	FUNCTION(phase_flag_status16),
	FUNCTION(phase_flag_status17),
	FUNCTION(phase_flag_status18),
	FUNCTION(phase_flag_status19),
	FUNCTION(phase_flag_status2),
	FUNCTION(phase_flag_status20),
	FUNCTION(phase_flag_status21),
	FUNCTION(phase_flag_status22),
	FUNCTION(phase_flag_status23),
	FUNCTION(phase_flag_status24),
	FUNCTION(phase_flag_status25),
	FUNCTION(phase_flag_status26),
	FUNCTION(phase_flag_status27),
	FUNCTION(phase_flag_status28),
	FUNCTION(phase_flag_status29),
	FUNCTION(phase_flag_status3),
	FUNCTION(phase_flag_status30),
	FUNCTION(phase_flag_status31),
	FUNCTION(phase_flag_status4),
	FUNCTION(phase_flag_status5),
	FUNCTION(phase_flag_status6),
	FUNCTION(phase_flag_status7),
	FUNCTION(phase_flag_status8),
	FUNCTION(phase_flag_status9),
	FUNCTION(pll_bist_sync),
	FUNCTION(pll_clk_aux),
	FUNCTION(prng_rosc_test0),
	FUNCTION(prng_rosc_test1),
	FUNCTION(prng_rosc_test2),
	FUNCTION(prng_rosc_test3),
	FUNCTION(qdss_cti_trig0),
	FUNCTION(qdss_cti_trig1),
	FUNCTION(qdss_gpio_traceclk),
	FUNCTION(qdss_gpio_tracectl),
	FUNCTION(qdss_gpio_tracedata0),
	FUNCTION(qdss_gpio_tracedata1),
	FUNCTION(qdss_gpio_tracedata10),
	FUNCTION(qdss_gpio_tracedata11),
	FUNCTION(qdss_gpio_tracedata12),
	FUNCTION(qdss_gpio_tracedata13),
	FUNCTION(qdss_gpio_tracedata14),
	FUNCTION(qdss_gpio_tracedata15),
	FUNCTION(qdss_gpio_tracedata2),
	FUNCTION(qdss_gpio_tracedata3),
	FUNCTION(qdss_gpio_tracedata4),
	FUNCTION(qdss_gpio_tracedata5),
	FUNCTION(qdss_gpio_tracedata6),
	FUNCTION(qdss_gpio_tracedata7),
	FUNCTION(qdss_gpio_tracedata8),
	FUNCTION(qdss_gpio_tracedata9),
	FUNCTION(qlink0_enable),
	FUNCTION(qlink0_request),
	FUNCTION(qlink0_wmss_reset),
	FUNCTION(qlink1_enable),
	FUNCTION(qlink1_request),
	FUNCTION(qlink1_wmss_reset),
	FUNCTION(qup0_se0_l0),
	FUNCTION(qup0_se0_l1),
	FUNCTION(qup0_se0_l2),
	FUNCTION(qup0_se0_l3),
	FUNCTION(qup0_se1_l0),
	FUNCTION(qup0_se1_l1),
	FUNCTION(qup0_se1_l2),
	FUNCTION(qup0_se1_l3),
	FUNCTION(qup0_se1_l4),
	FUNCTION(qup0_se2_l0),
	FUNCTION(qup0_se2_l1),
	FUNCTION(qup0_se2_l2),
	FUNCTION(qup0_se2_l3),
	FUNCTION(qup0_se3_l0),
	FUNCTION(qup0_se3_l1),
	FUNCTION(qup0_se3_l2),
	FUNCTION(qup0_se3_l3),
	FUNCTION(qup0_se4_l0),
	FUNCTION(qup0_se4_l1),
	FUNCTION(qup0_se4_l2),
	FUNCTION(qup0_se4_l3),
	FUNCTION(qup0_se5_l0),
	FUNCTION(qup0_se5_l1),
	FUNCTION(qup0_se5_l2),
	FUNCTION(qup0_se5_l3),
	FUNCTION(qup0_se5_l4),
	FUNCTION(qup0_se5_l5),
	FUNCTION(qup0_se5_l6),
	FUNCTION(qup1_se0_l0),
	FUNCTION(qup1_se0_l1),
	FUNCTION(qup1_se0_l2),
	FUNCTION(qup1_se0_l3),
	FUNCTION(qup1_se1_l0),
	FUNCTION(qup1_se1_l1),
	FUNCTION(qup1_se1_l2),
	FUNCTION(qup1_se1_l3),
	FUNCTION(qup1_se2_l0),
	FUNCTION(qup1_se2_l1),
	FUNCTION(qup1_se2_l2),
	FUNCTION(qup1_se2_l3),
	FUNCTION(qup1_se2_l4),
	FUNCTION(qup1_se3_l0),
	FUNCTION(qup1_se3_l1),
	FUNCTION(qup1_se3_l2),
	FUNCTION(qup1_se3_l3),
	FUNCTION(qup1_se5_l0),
	FUNCTION(qup1_se5_l1),
	FUNCTION(qup1_se5_l2),
	FUNCTION(qup1_se5_l3),
	FUNCTION(sd_write_protect),
	FUNCTION(tb_trig_sdc1),
	FUNCTION(tb_trig_sdc2),
	FUNCTION(tgu_ch0_trigout),
	FUNCTION(tgu_ch1_trigout),
	FUNCTION(tgu_ch2_trigout),
	FUNCTION(tgu_ch3_trigout),
	FUNCTION(tmess_prng_rosc0),
	FUNCTION(tmess_prng_rosc1),
	FUNCTION(tmess_prng_rosc2),
	FUNCTION(tmess_prng_rosc3),
	FUNCTION(tsense_pwm1_out),
	FUNCTION(tsense_pwm2_out),
	FUNCTION(uim0_clk),
	FUNCTION(uim0_data),
	FUNCTION(uim0_present),
	FUNCTION(uim0_reset),
	FUNCTION(uim1_clk),
	FUNCTION(uim1_data),
	FUNCTION(uim1_present),
	FUNCTION(uim1_reset),
	FUNCTION(usb0_hs_ac),
	FUNCTION(usb0_phy_ps),
	FUNCTION(vfr_0_mira),
	FUNCTION(vfr_0_mirb),
	FUNCTION(vfr_1),
	FUNCTION(vsense_trigger_mirnat),
	FUNCTION(wlan1_adc_dtest0),
	FUNCTION(wlan1_adc_dtest1),
	FUNCTION(wlan2_adc_dtest0),
	FUNCTION(wlan2_adc_dtest1),
};

/* Every pin is maintained as a single group, and missing or non-existing pin
 * would be maintained as dummy group to synchronize pin group index with
 * pin descriptor registered with pinctrl core.
 * Clients would not be able to request these dummy pin groups.
 */
static const struct msm_pingroup parrot_groups[] = {
	[0] = PINGROUP(0, qup0_se1_l0, ibi_i3c_qup0, cmu_rng_entropy3,
		       qdss_cti_trig0, NA, NA, NA, NA, NA, 0x8D00C, 1),
	[1] = PINGROUP(1, qup0_se1_l1, ibi_i3c_qup0, cmu_rng_entropy2, NA, NA,
		       NA, NA, NA, NA, 0, -1),
	[2] = PINGROUP(2, qup0_se1_l2, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[3] = PINGROUP(3, qup0_se1_l3, gcc_gp3_clk, cmu_rng_entropy1,
		       qdss_cti_trig1, wlan1_adc_dtest0,
		       atest_usb0_testdataout01, NA, NA, NA, 0x8D00C, 2),
	[4] = PINGROUP(4, qup1_se0_l0, ibi_i3c_qup1, NA, phase_flag_status14,
		       qdss_gpio_tracedata0, NA, NA, NA, NA, 0x8D000, 3),
	[5] = PINGROUP(5, qup1_se0_l1, ibi_i3c_qup1, NA, phase_flag_status2,
		       qdss_gpio_tracedata1, NA, NA, NA, NA, 0x8D000, 4),
	[6] = PINGROUP(6, qup1_se0_l2, qup1_se1_l2, NA, NA, NA, NA, NA, NA, NA, 0x8D000, 5),
	[7] = PINGROUP(7, qup1_se0_l3, qup1_se1_l3, NA, phase_flag_status23,
		       qdss_gpio_tracedata2, NA, NA, NA, NA, 0x8D000, 6),
	[8] = PINGROUP(8, qup1_se1_l0, ibi_i3c_qup1, qdss_gpio_tracedata3, NA,
		       NA, NA, NA, NA, NA, 0x8D000, 7),
	[9] = PINGROUP(9, qup1_se1_l1, ibi_i3c_qup1, NA, phase_flag_status21,
		       qdss_gpio_tracedata4, NA, NA, NA, NA, 0x8D000, 8),
	[10] = PINGROUP(10, qup1_se3_l0, NA, phase_flag_status6, NA, NA, NA,
			NA, NA, NA, 0x8D000, 9),
	[11] = PINGROUP(11, qup1_se3_l1, NA, phase_flag_status4, NA, NA, NA,
			NA, NA, NA, 0x8D000, 10),
	[12] = PINGROUP(12, qup1_se3_l2, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D000, 11),
	[13] = PINGROUP(13, qup1_se3_l3, NA, phase_flag_status12, NA, NA, NA,
			NA, NA, NA, 0x8D000, 12),
	[14] = PINGROUP(14, qup1_se5_l0, NA, phase_flag_status5, NA, NA, NA,
			NA, NA, NA, 0x8D000, 13),
	[15] = PINGROUP(15, qup1_se5_l1, NA, NA, NA, ddr_pxi0_test, NA, NA, NA,
			NA, 0, -1),
	[16] = PINGROUP(16, qup1_se5_l2, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[17] = PINGROUP(17, qup1_se5_l3, NA, NA, NA, ddr_pxi0_test, NA, NA, NA,
			NA, 0x8D000, 14),
	[18] = PINGROUP(18, qup1_se2_l0, NA, phase_flag_status3, NA, NA, NA,
			NA, NA, NA, 0x8D000, 15),
	[19] = PINGROUP(19, qup1_se2_l1, vfr_0_mira, NA, phase_flag_status7,
			NA, NA, NA, NA, NA, 0, -1),
	[20] = PINGROUP(20, qup1_se2_l2, tgu_ch0_trigout, NA, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[21] = PINGROUP(21, qup1_se2_l3, NA, phase_flag_status16,
			tgu_ch1_trigout, NA, NA, NA, NA, NA, 0x8D004, 0),
	[22] = PINGROUP(22, qup0_se3_l2, tgu_ch2_trigout, NA, NA, NA, NA, NA,
			NA, NA, 0x8D00C, 3),
	[23] = PINGROUP(23, qup0_se3_l3, NA, tgu_ch3_trigout, NA, NA, NA, NA,
			NA, NA, 0x8D00C, 4),
	[24] = PINGROUP(24, qup0_se2_l0, NA, phase_flag_status30, NA, NA, NA,
			NA, NA, NA, 0x8D00C, 5),
	[25] = PINGROUP(25, qup0_se2_l1, NA, phase_flag_status26, NA, NA, NA,
			NA, NA, NA, 0x8D00C, 6),
	[26] = PINGROUP(26, qup0_se4_l0, mdp_vsync_e, qdss_cti_trig0, NA, NA,
			NA, NA, NA, NA, 0x8D00C, 7),
	[27] = PINGROUP(27, qup0_se4_l1, qdss_cti_trig1, NA, NA, NA, NA, NA,
			NA, NA, 0x8D00C, 8),
	[28] = PINGROUP(28, qup0_se0_l0, ibi_i3c_qup0, NA, NA, NA, NA, NA, NA,
			NA, 0x8D00C, 9),
	[29] = PINGROUP(29, qup0_se0_l1, ibi_i3c_qup0, NA, cmu_rng_entropy0,
			NA, NA, NA, NA, NA, 0, -1),
	[30] = PINGROUP(30, qup0_se0_l2, ddr_bist_fail, NA, NA, NA, NA, NA, NA,
			NA, 0, -1),
	[31] = PINGROUP(31, qup0_se0_l3, dp0_hot_plug, NA, gcc_gp2_clk,
			ddr_bist_start, wlan2_adc_dtest0,
			atest_usb0_testdataout02, NA, NA, 0x8D00C, 10),
	[32] = PINGROUP(32, qup0_se5_l0, QSPI0, ddr_bist_stop, NA, NA, NA, NA,
			NA, NA, 0x8D00C, 11),
	[33] = PINGROUP(33, qup0_se5_l1, QSPI1, ddr_bist_complete, NA, NA, NA,
			NA, NA, NA, 0, -1),
	[34] = PINGROUP(34, qup0_se5_l2, QSPI_CLK, qup0_se2_l2, qup0_se4_l2,
			NA, NA, NA, NA, NA, 0, -1),
	[35] = PINGROUP(35, qup0_se5_l3, QSPI_CS_N, qup0_se2_l3, mdp_vsync_s,
			qup0_se4_l3, NA, NA, NA, NA, 0x8D00C, 12),
	[36] = PINGROUP(36, qup0_se5_l4, QSPI2, qup0_se3_l1, tb_trig_sdc1, NA,
			NA, NA, NA, NA, 0, -1),
	[37] = PINGROUP(37, qup0_se5_l5, QSPI3, qup0_se3_l0, mdp_vsync_e, NA,
			NA, NA, NA, NA, 0x8D00C, 13),
	[38] = PINGROUP(38, qup0_se5_l6, QSPI_CS_N, qup0_se1_l4, NA, NA, NA,
			NA, NA, NA, 0x8D00C, 14),
	[39] = PINGROUP(39, cam_mclk, qdss_gpio_tracedata0, NA, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[40] = PINGROUP(40, cam_mclk, prng_rosc_test0, qdss_gpio_tracedata1,
			NA, NA, NA, NA, NA, NA, 0, -1),
	[41] = PINGROUP(41, cam_mclk, prng_rosc_test1, qdss_gpio_tracedata2,
			NA, NA, NA, NA, NA, NA, 0, -1),
	[42] = PINGROUP(42, cam_mclk, host2wlan_sol_mira, prng_rosc_test2,
			qdss_gpio_tracedata3, NA, NA, NA, NA, NA, 0, -1),
	[43] = PINGROUP(43, cam_mclk, prng_rosc_test3, qdss_gpio_tracedata4,
			NA, NA, NA, NA, NA, NA, 0x8D00C, 15),
	[44] = PINGROUP(44, cci_timer0, cci_async_in0, NA, phase_flag_status25,
			cri_trng_rosc0, qdss_gpio_tracedata11, NA, NA, NA, 0x8D010, 0),
	[45] = PINGROUP(45, cci_timer1, NA, gcc_gp1_clk, cri_trng_rosc1,
			qdss_gpio_tracedata12, NA, NA, NA, NA, 0x8D010, 1),
	[46] = PINGROUP(46, cci_timer2, dbg_out_clk, cri_trng_rosc,
			qdss_gpio_tracedata13, NA, NA, NA, NA, NA, 0x8D010, 2),
	[47] = PINGROUP(47, cci_timer3, NA, phase_flag_status17,
			atest_char_status0, NA, NA, NA, NA, NA, 0x8D010, 3),
	[48] = PINGROUP(48, mdp_vsync_s, mdp_vsync2_out, mdp_vsync3_out, NA,
			phase_flag_status29, atest_char_status1, NA, NA, NA, 0x8D010, 4),
	[49] = PINGROUP(49, cci_i2c_sda0, qdss_gpio_tracedata5, NA, NA, NA, NA,
			NA, NA, NA, 0, -1),
	[50] = PINGROUP(50, cci_i2c_scl0, qdss_gpio_tracedata6, NA, NA, NA, NA,
			NA, NA, NA, 0, -1),
	[51] = PINGROUP(51, cci_i2c_sda1, qdss_gpio_tracedata7, NA, NA, NA, NA,
			NA, NA, NA, 0, -1),
	[52] = PINGROUP(52, cci_i2c_scl1, qdss_gpio_traceclk, NA, NA, NA, NA,
			NA, NA, NA, 0x8D010, 5),
	[53] = PINGROUP(53, cci_i2c_sda2, gcc_gp1_clk, qdss_gpio_tracectl, NA,
			NA, NA, NA, NA, NA, 0, -1),
	[54] = PINGROUP(54, cci_i2c_scl2, PCIE0_CLK_REQ, gcc_gp2_clk,
			qdss_gpio_tracedata8, NA, NA, NA, NA, NA, 0x8D010, 6),
	[55] = PINGROUP(55, cci_i2c_sda3, gcc_gp3_clk, qdss_gpio_tracedata9,
			NA, NA, NA, NA, NA, NA, 0x8D010, 7),
	[56] = PINGROUP(56, cci_i2c_scl3, qdss_gpio_tracedata10, NA, NA, NA,
			NA, NA, NA, NA, 0, -1),
	[57] = PINGROUP(57, NA, phase_flag_status27, qdss_gpio_tracedata14,
			atest_char_status2, NA, NA, NA, NA, NA, 0x8D010, 8),
	[58] = PINGROUP(58, NA, phase_flag_status28, qdss_gpio_tracedata15,
			atest_char_status3, NA, NA, NA, NA, NA, 0, -1),
	[59] = PINGROUP(59, coex_uart1_rx, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D018, 0),
	[60] = PINGROUP(60, coex_uart1_tx, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D018, 1),
	[61] = PINGROUP(61, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D018, 2),
	[62] = PINGROUP(62, NA, tmess_prng_rosc0, qdss_cti_trig1,
			wlan1_adc_dtest1, atest_usb0_testdataout03, NA, NA, NA,
			NA, 0x8D010, 9),
	[63] = PINGROUP(63, NA, tmess_prng_rosc1, qdss_cti_trig1,
			vsense_trigger_mirnat, atest_usb0_atereset, NA, NA, NA,
			NA, 0x8D010, 10),
	[64] = PINGROUP(64, NA, tmess_prng_rosc2, NA, ddr_pxi1_test, NA, NA,
			NA, NA, NA, 0x8D010, 11),
	[65] = PINGROUP(65, jitter_bist_ref, tmess_prng_rosc3, NA,
			ddr_pxi1_test, NA, NA, NA, NA, NA, 0x8D010, 12),
	[66] = PINGROUP(66, wlan2_adc_dtest1, atest_usb0_testdataout00, NA, NA,
			NA, NA, NA, NA, NA, 0x8D010, 13),
	[67] = PINGROUP(67, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D010, 14),
	[68] = PINGROUP(68, uim0_data, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[69] = PINGROUP(69, uim0_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[70] = PINGROUP(70, uim0_reset, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[71] = PINGROUP(71, uim0_present, NA, phase_flag_status24,
			atest_char_start, NA, NA, NA, NA, NA, 0x8D004, 1),
	[72] = PINGROUP(72, uim1_data, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D004, 2),
	[73] = PINGROUP(73, uim1_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[74] = PINGROUP(74, uim1_reset, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[75] = PINGROUP(75, uim1_present, NA, phase_flag_status18,
			tb_trig_sdc2, NA, NA, NA, NA, NA, 0x8D004, 3),
	[76] = PINGROUP(76, NA, NA, NA, audio_ref_clk, NA, NA, NA, NA, NA, 0x8D004, 4),
	[77] = PINGROUP(77, NA, NA, pll_clk_aux, NA, NA, NA, NA, NA, NA, 0, -1),
	[78] = PINGROUP(78, NA, NA, NA, pll_bist_sync, NA, NA, NA, NA, NA, 0x8D004, 5),
	[79] = PINGROUP(79, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[80] = PINGROUP(80, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D004, 6),
	[81] = PINGROUP(81, NA, NA, NA, phase_flag_status8, NA, NA, NA, NA, NA, 0, -1),
	[82] = PINGROUP(82, NA, NA, NA, phase_flag_status9, NA, NA, NA, NA, NA, 0x8D004, 7),
	[83] = PINGROUP(83, NA, NA, NA, phase_flag_status0, NA, NA, NA, NA, NA, 0, -1),
	[84] = PINGROUP(84, NA, NA, NA, phase_flag_status10, NA, NA, NA, NA,
			NA, 0x8D004, 8),
	[85] = PINGROUP(85, NA, NA, NA, phase_flag_status11, NA, NA, NA, NA,
			NA, 0, -1),
	[86] = PINGROUP(86, NA, nav_gpio2, NA, phase_flag_status1, NA, NA, NA,
			NA, NA, 0x8D004, 9),
	[87] = PINGROUP(87, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[88] = PINGROUP(88, nav_gpio0, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D018, 3),
	[89] = PINGROUP(89, nav_gpio1, vfr_1, NA, NA, NA, NA, NA, NA, NA, 0x8D018, 4),
	[90] = PINGROUP(90, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[91] = PINGROUP(91, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D018, 5),
	[92] = PINGROUP(92, qlink0_request, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D018, 6),
	[93] = PINGROUP(93, qlink0_enable, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[94] = PINGROUP(94, qlink0_wmss_reset, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[95] = PINGROUP(95, qlink1_request, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D018, 7),
	[96] = PINGROUP(96, qlink1_enable, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[97] = PINGROUP(97, qlink1_wmss_reset, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[98] = PINGROUP(98, i2s0_sck, qup1_se2_l4, tsense_pwm1_out,
			tsense_pwm2_out, NA, NA, NA, NA, NA, 0x8D004, 10),
	[99] = PINGROUP(99, mdp_vsync_p, i2s0_ws, mdp_vsync0_out,
			mdp_vsync1_out, NA, NA, NA, NA, NA, 0x8D004, 11),
	[100] = PINGROUP(100, usb0_phy_ps, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D010, 15),
	[101] = PINGROUP(101, i2s0_data0, NA, phase_flag_status20, NA, NA, NA,
			 NA, NA, NA, 0x8D004, 12),
	[102] = PINGROUP(102, NA, phase_flag_status31, NA, NA, NA, NA, NA, NA,
			 NA, 0x8D014, 0),
	[103] = PINGROUP(103, mdp_vsync_p, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D014, 1),
	[104] = PINGROUP(104, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D014, 2),
	[105] = PINGROUP(105, usb0_hs_ac, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D014, 3),
	[106] = PINGROUP(106, vfr_0_mirb, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[107] = PINGROUP(107, NA, phase_flag_status19, NA, NA, NA, NA, NA, NA,
			 NA, 0x8D004, 13),
	[108] = PINGROUP(108, sd_write_protect, NA, phase_flag_status15, NA,
			 NA, NA, NA, NA, NA, 0, -1),
	[109] = PINGROUP(109, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D014, 4),
	[110] = PINGROUP(110, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[111] = PINGROUP(111, i2s0_data1, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D004, 14),
	[112] = PINGROUP(112, host2wlan_sol_mirb, dp0_hot_plug, NA, NA, NA, NA,
			 NA, NA, NA, 0x8D018, 8),
	[113] = PINGROUP(113, PCIE0_CLK_REQ, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D018, 9),
	[114] = PINGROUP(114, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[115] = PINGROUP(115, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D004, 15),
	[116] = PINGROUP(116, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[117] = PINGROUP(117, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[118] = PINGROUP(118, NA, qdss_cti_trig0, NA, NA, NA, NA, NA, NA, NA, 0x8D008, 0),
	[119] = PINGROUP(119, qdss_cti_trig0, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[120] = PINGROUP(120, qdss_gpio_tracedata5, NA, NA, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[121] = PINGROUP(121, qdss_gpio_tracedata6, NA, NA, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[122] = PINGROUP(122, qdss_gpio_tracectl, NA, NA, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[123] = PINGROUP(123, qdss_gpio_traceclk, NA, NA, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[124] = PINGROUP(124, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[125] = PINGROUP(125, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D008, 1),
	[126] = PINGROUP(126, qdss_gpio_tracedata7, NA, NA, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[127] = PINGROUP(127, qdss_gpio_tracedata8, NA, NA, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[128] = PINGROUP(128, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D008, 2),
	[129] = PINGROUP(129, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[130] = PINGROUP(130, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D008, 3),
	[131] = PINGROUP(131, NA, qdss_gpio_tracedata9, NA, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[132] = PINGROUP(132, qdss_gpio_tracedata10, NA, NA, NA, NA, NA, NA,
			 NA, NA, 0, -1),
	[133] = PINGROUP(133, NA, qdss_gpio_tracedata11, NA, NA, NA, NA, NA,
			 NA, NA, 0x8D008, 4),
	[134] = PINGROUP(134, NA, qdss_gpio_tracedata12, NA, NA, NA, NA, NA,
			 NA, NA, 0, -1),
	[135] = PINGROUP(135, NA, qdss_gpio_tracedata13, NA, NA, NA, NA, NA,
			 NA, NA, 0x8D008, 5),
	[136] = PINGROUP(136, NA, qdss_gpio_tracedata14, NA, NA, NA, NA, NA,
			 NA, NA, 0, -1),
	[137] = PINGROUP(137, NA, phase_flag_status13, NA, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[138] = PINGROUP(138, NA, phase_flag_status22, qdss_gpio_tracedata15,
			 NA, NA, NA, NA, NA, NA, 0x8D008, 6),
	[139] = PINGROUP(139, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[140] = PINGROUP(140, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x8D008, 7),
	[141] = UFS_RESET(ufs_reset, 0x19c000),
	[142] = SDC_QDSD_PINGROUP(sdc1_rclk, 0x191004, 0, 0),
	[143] = SDC_QDSD_PINGROUP(sdc1_clk, 0x191000, 13, 6),
	[144] = SDC_QDSD_PINGROUP(sdc1_cmd, 0x191000, 11, 3),
	[145] = SDC_QDSD_PINGROUP(sdc1_data, 0x191000, 9, 0),
	[146] = SDC_QDSD_PINGROUP(sdc2_clk, 0x194000, 14, 6),
	[147] = SDC_QDSD_PINGROUP(sdc2_cmd, 0x194000, 11, 3),
	[148] = SDC_QDSD_PINGROUP(sdc2_data, 0x194000, 9, 0),
};

static const struct msm_gpio_wakeirq_map parrot_pdc_map[] = {
	{ 0, 67 }, { 3, 82 }, { 4, 69 }, { 5, 70 }, { 6, 44 }, {7, 43 },
	{ 8, 71 }, { 9, 86 }, { 10, 48 }, { 11, 77 }, { 12, 90 }, {13, 54 },
	{ 14, 91 }, { 17, 97 }, { 18, 102 }, { 21, 103 }, { 22, 104 },
	{ 23, 105 }, { 24, 53 }, { 25, 106 }, { 26, 65 }, { 27, 55 },
	{ 28, 89 }, { 31, 109 }, { 32, 110 }, { 35, 59 }, { 37, 98 },
	{ 38, 111 }, { 43, 75 }, { 44, 114 }, { 45, 115 }, { 46, 116 },
	{ 47, 118 }, { 48, 95 }, { 52, 121 }, { 54, 94 }, { 55, 99 },
	{ 57, 52 }, { 59, 100 }, { 60, 113 }, { 61, 45 }, { 62, 85 },
	{ 63, 56 }, { 64, 84 }, { 65, 83 }, {66, 96 }, {67, 93 }, { 71, 122 },
	{ 72, 123 }, { 75, 42 }, { 76, 124 }, { 78, 125 }, { 80, 126 },
	{ 82, 127 }, { 84, 128 }, { 86, 129 }, { 88, 64 }, { 89, 130 },
	{ 91, 63 }, { 92, 101 }, { 95, 57 }, { 98, 66 }, { 99, 133 },
	{ 100, 76 }, { 101, 62 }, { 102, 134 }, {103, 49 }, { 104, 73 },
	{ 105, 92 }, { 107, 46 }, { 109, 135 }, { 111, 136 }, { 112, 137 },
	{ 113, 58 }, { 115, 107 }, { 118, 108 }, { 125, 112 }, { 128, 72 },
	{ 130, 117 }, { 133, 119 }, { 135, 120 }, { 138, 131 }, { 140, 132 },
};
