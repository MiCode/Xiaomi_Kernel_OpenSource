/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#define FUNCTION(fname)	(	                    \
	[msm_mux_##fname] = {		                \
		.name = #fname,		\
		.groups = fname##_groups,               \
		.ngroups = ARRAY_SIZE(fname##_groups),	\
	})

#define NORTH	0x500000
#define WEST	0x100000
#define EAST	0x900000
#define REG_SIZE 0x1000
#define PINGROUP(id, base, f1, f2, f3, f4, f5, f6, f7, f8, f9)	\
	{					        \
		.name = "gpio" #id,			\
		.pins = gpio##id##_pins,		\
		.npins = (unsigned)ARRAY_SIZE(gpio##id##_pins),	\
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
		.ctl_reg = base + REG_SIZE * id,	\
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
	{					        \
		.name = #pg_name,			\
		.pins = pg_name##_pins,			\
		.npins = (unsigned)ARRAY_SIZE(pg_name##_pins),	\
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
static const struct pinctrl_pin_desc msmhamster_pins[] = {
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

enum msmhamster_functions {
	msm_mux_blsp_spi1,
	msm_mux_blsp_uim1_a,
	msm_mux_blsp_uart1_a,
	msm_mux_blsp_i2c1,
	msm_mux_blsp_spi8,
	msm_mux_blsp_uart8_a,
	msm_mux_blsp_uim8_a,
	msm_mux_qdss_cti0_b,
	msm_mux_blsp_i2c8,
	msm_mux_ddr_bist,
	msm_mux_atest_tsens2,
	msm_mux_atest_usb1,
	msm_mux_blsp_spi4,
	msm_mux_blsp_uart1_b,
	msm_mux_blsp_uim1_b,
	msm_mux_wlan1_adc1,
	msm_mux_atest_usb13,
	msm_mux_bimc_dte1,
	msm_mux_tsif1_sync,
	msm_mux_wlan1_adc0,
	msm_mux_atest_usb12,
	msm_mux_bimc_dte0,
	msm_mux_mdp_vsync_a,
	msm_mux_blsp_i2c4,
	msm_mux_atest_gpsadc1,
	msm_mux_wlan2_adc1,
	msm_mux_atest_usb11,
	msm_mux_edp_lcd,
	msm_mux_dbg_out,
	msm_mux_atest_gpsadc0,
	msm_mux_wlan2_adc0,
	msm_mux_atest_usb10,
	msm_mux_mdp_vsync,
	msm_mux_m_voc,
	msm_mux_cam_mclk,
	msm_mux_pll_bypassnl,
	msm_mux_qdss_gpio0,
	msm_mux_pll_reset,
	msm_mux_qdss_gpio1,
	msm_mux_qdss_gpio2,
	msm_mux_qdss_gpio3,
	msm_mux_cci_i2c,
	msm_mux_qdss_gpio4,
	msm_mux_phase_flag14,
	msm_mux_qdss_gpio5,
	msm_mux_phase_flag15,
	msm_mux_qdss_gpio6,
	msm_mux_qdss_gpio7,
	msm_mux_cci_timer4,
	msm_mux_blsp2_spi,
	msm_mux_qdss_gpio11,
	msm_mux_qdss_gpio12,
	msm_mux_qdss_gpio13,
	msm_mux_qdss_gpio14,
	msm_mux_qdss_gpio15,
	msm_mux_cci_timer0,
	msm_mux_qdss_gpio8,
	msm_mux_vsense_data0,
	msm_mux_cci_timer1,
	msm_mux_qdss_gpio,
	msm_mux_vsense_data1,
	msm_mux_cci_timer2,
	msm_mux_blsp1_spi_b,
	msm_mux_qdss_gpio9,
	msm_mux_vsense_mode,
	msm_mux_cci_timer3,
	msm_mux_cci_async,
	msm_mux_blsp1_spi_a,
	msm_mux_qdss_gpio10,
	msm_mux_vsense_clkout,
	msm_mux_hdmi_rcv,
	msm_mux_hdmi_cec,
	msm_mux_blsp_spi2,
	msm_mux_blsp_uart2_a,
	msm_mux_blsp_uim2_a,
	msm_mux_pwr_modem,
	msm_mux_hdmi_ddc,
	msm_mux_blsp_i2c2,
	msm_mux_pwr_nav,
	msm_mux_pwr_crypto,
	msm_mux_hdmi_hot,
	msm_mux_edp_hot,
	msm_mux_pci_e0,
	msm_mux_jitter_bist,
	msm_mux_agera_pll,
	msm_mux_atest_tsens,
	msm_mux_usb_phy,
	msm_mux_lpass_slimbus,
	msm_mux_sd_write,
	msm_mux_tsif1_error,
	msm_mux_blsp_spi6,
	msm_mux_blsp_uart3_b,
	msm_mux_blsp_uim3_b,
	msm_mux_blsp_i2c6,
	msm_mux_bt_reset,
	msm_mux_blsp_spi3,
	msm_mux_blsp_uart3_a,
	msm_mux_blsp_uim3_a,
	msm_mux_blsp_i2c3,
	msm_mux_blsp_spi9,
	msm_mux_blsp_uart9_a,
	msm_mux_blsp_uim9_a,
	msm_mux_blsp10_spi_b,
	msm_mux_qdss_cti0_a,
	msm_mux_blsp_i2c9,
	msm_mux_blsp10_spi_a,
	msm_mux_blsp_spi7,
	msm_mux_blsp_uart7_a,
	msm_mux_blsp_uim7_a,
	msm_mux_blsp_i2c7,
	msm_mux_qua_mi2s,
	msm_mux_blsp10_spi,
	msm_mux_gcc_gp1_a,
	msm_mux_ssc_irq,
	msm_mux_blsp_spi11,
	msm_mux_blsp_uart8_b,
	msm_mux_blsp_uim8_b,
	msm_mux_gcc_gp2_a,
	msm_mux_qdss_cti1_a,
	msm_mux_gcc_gp3_a,
	msm_mux_blsp_i2c11,
	msm_mux_cri_trng0,
	msm_mux_cri_trng1,
	msm_mux_cri_trng,
	msm_mux_pri_mi2s,
	msm_mux_sp_cmu,
	msm_mux_blsp_spi10,
	msm_mux_blsp_uart7_b,
	msm_mux_blsp_uim7_b,
	msm_mux_pri_mi2s_ws,
	msm_mux_blsp_i2c10,
	msm_mux_spkr_i2s,
	msm_mux_audio_ref,
	msm_mux_blsp9_spi,
	msm_mux_tsense_pwm1,
	msm_mux_tsense_pwm2,
	msm_mux_btfm_slimbus,
	msm_mux_phase_flag0,
	msm_mux_ter_mi2s,
	msm_mux_phase_flag7,
	msm_mux_phase_flag8,
	msm_mux_phase_flag9,
	msm_mux_phase_flag4,
	msm_mux_gcc_gp1_b,
	msm_mux_sec_mi2s,
	msm_mux_blsp_spi12,
	msm_mux_blsp_uart9_b,
	msm_mux_blsp_uim9_b,
	msm_mux_gcc_gp2_b,
	msm_mux_gcc_gp3_b,
	msm_mux_blsp_i2c12,
	msm_mux_blsp_spi5,
	msm_mux_blsp_uart2_b,
	msm_mux_blsp_uim2_b,
	msm_mux_blsp_i2c5,
	msm_mux_tsif1_clk,
	msm_mux_phase_flag10,
	msm_mux_tsif1_en,
	msm_mux_mdp_vsync0,
	msm_mux_mdp_vsync1,
	msm_mux_mdp_vsync2,
	msm_mux_mdp_vsync3,
	msm_mux_blsp1_spi,
	msm_mux_tgu_ch0,
	msm_mux_qdss_cti1_b,
	msm_mux_tsif1_data,
	msm_mux_sdc4_cmd,
	msm_mux_tgu_ch1,
	msm_mux_phase_flag1,
	msm_mux_tsif2_error,
	msm_mux_sdc43,
	msm_mux_vfr_1,
	msm_mux_phase_flag2,
	msm_mux_tsif2_clk,
	msm_mux_sdc4_clk,
	msm_mux_tsif2_en,
	msm_mux_sdc42,
	msm_mux_sd_card,
	msm_mux_tsif2_data,
	msm_mux_sdc41,
	msm_mux_tsif2_sync,
	msm_mux_sdc40,
	msm_mux_phase_flag3,
	msm_mux_mdp_vsync_b,
	msm_mux_ldo_en,
	msm_mux_ldo_update,
	msm_mux_blsp_uart8,
	msm_mux_blsp11_i2c,
	msm_mux_prng_rosc,
	msm_mux_phase_flag5,
	msm_mux_uim2_data,
	msm_mux_uim2_clk,
	msm_mux_uim2_reset,
	msm_mux_uim2_present,
	msm_mux_uim1_data,
	msm_mux_uim1_clk,
	msm_mux_uim1_reset,
	msm_mux_uim1_present,
	msm_mux_uim_batt,
	msm_mux_phase_flag16,
	msm_mux_nav_dr,
	msm_mux_phase_flag11,
	msm_mux_phase_flag12,
	msm_mux_phase_flag13,
	msm_mux_atest_char,
	msm_mux_adsp_ext,
	msm_mux_phase_flag17,
	msm_mux_atest_char3,
	msm_mux_phase_flag18,
	msm_mux_atest_char2,
	msm_mux_phase_flag19,
	msm_mux_atest_char1,
	msm_mux_phase_flag20,
	msm_mux_atest_char0,
	msm_mux_phase_flag21,
	msm_mux_phase_flag22,
	msm_mux_phase_flag23,
	msm_mux_phase_flag24,
	msm_mux_phase_flag25,
	msm_mux_modem_tsync,
	msm_mux_nav_pps,
	msm_mux_phase_flag26,
	msm_mux_phase_flag27,
	msm_mux_qlink_request,
	msm_mux_phase_flag28,
	msm_mux_qlink_enable,
	msm_mux_phase_flag6,
	msm_mux_phase_flag29,
	msm_mux_phase_flag30,
	msm_mux_phase_flag31,
	msm_mux_pa_indicator,
	msm_mux_ssbi1,
	msm_mux_isense_dbg,
	msm_mux_mss_lte,
	msm_mux_gpio,
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
	"gpio99", "gpio100", "gpio101",	"gpio102", "gpio103", "gpio104",
	"gpio105", "gpio106", "gpio107", "gpio108", "gpio109", "gpio110",
	"gpio111", "gpio112", "gpio113", "gpio114", "gpio115", "gpio116",
	"gpio117", "gpio118", "gpio119", "gpio120", "gpio121", "gpio122",
	"gpio123", "gpio124", "gpio125", "gpio126", "gpio127", "gpio128",
	"gpio129", "gpio130", "gpio131", "gpio132", "gpio133", "gpio134",
	"gpio135", "gpio136", "gpio137", "gpio138", "gpio139", "gpio140",
	"gpio141", "gpio142", "gpio143", "gpio144", "gpio145", "gpio146",
	"gpio147", "gpio148", "gpio149",
};
static const char * const blsp_spi1_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3",
};
static const char * const blsp_uim1_a_groups[] = {
	"gpio0", "gpio1",
};
static const char * const blsp_uart1_a_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3",
};
static const char * const blsp_i2c1_groups[] = {
	"gpio2", "gpio3",
};
static const char * const blsp_spi8_groups[] = {
	"gpio4", "gpio5", "gpio6", "gpio7",
};
static const char * const blsp_uart8_a_groups[] = {
	"gpio4", "gpio5", "gpio6", "gpio7",
};
static const char * const blsp_uim8_a_groups[] = {
	"gpio4", "gpio5",
};
static const char * const qdss_cti0_b_groups[] = {
	"gpio4", "gpio5",
};
static const char * const blsp_i2c8_groups[] = {
	"gpio6", "gpio7",
};
static const char * const ddr_bist_groups[] = {
	"gpio7", "gpio8", "gpio9", "gpio10",
};
static const char * const atest_tsens2_groups[] = {
	"gpio7",
};
static const char * const atest_usb1_groups[] = {
	"gpio7",
};
static const char * const blsp_spi4_groups[] = {
	"gpio8", "gpio9", "gpio10", "gpio11",
};
static const char * const blsp_uart1_b_groups[] = {
	"gpio8", "gpio9", "gpio10", "gpio11",
};
static const char * const blsp_uim1_b_groups[] = {
	"gpio8", "gpio9",
};
static const char * const wlan1_adc1_groups[] = {
	"gpio8",
};
static const char * const atest_usb13_groups[] = {
	"gpio8",
};
static const char * const bimc_dte1_groups[] = {
	"gpio8", "gpio10",
};
static const char * const tsif1_sync_groups[] = {
	"gpio9",
};
static const char * const wlan1_adc0_groups[] = {
	"gpio9",
};
static const char * const atest_usb12_groups[] = {
	"gpio9",
};
static const char * const bimc_dte0_groups[] = {
	"gpio9", "gpio11",
};
static const char * const mdp_vsync_a_groups[] = {
	"gpio10", "gpio11",
};
static const char * const blsp_i2c4_groups[] = {
	"gpio10", "gpio11",
};
static const char * const atest_gpsadc1_groups[] = {
	"gpio10",
};
static const char * const wlan2_adc1_groups[] = {
	"gpio10",
};
static const char * const atest_usb11_groups[] = {
	"gpio10",
};
static const char * const edp_lcd_groups[] = {
	"gpio11",
};
static const char * const dbg_out_groups[] = {
	"gpio11",
};
static const char * const atest_gpsadc0_groups[] = {
	"gpio11",
};
static const char * const wlan2_adc0_groups[] = {
	"gpio11",
};
static const char * const atest_usb10_groups[] = {
	"gpio11",
};
static const char * const mdp_vsync_groups[] = {
	"gpio12",
};
static const char * const m_voc_groups[] = {
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
static const char * const qdss_gpio4_groups[] = {
	"gpio17", "gpio121",
};
static const char * const phase_flag14_groups[] = {
	"gpio18",
};
static const char * const qdss_gpio5_groups[] = {
	"gpio18", "gpio122",
};
static const char * const phase_flag15_groups[] = {
	"gpio19",
};
static const char * const qdss_gpio6_groups[] = {
	"gpio19", "gpio41",
};
static const char * const qdss_gpio7_groups[] = {
	"gpio20", "gpio42",
};
static const char * const cci_timer4_groups[] = {
	"gpio25",
};
static const char * const blsp2_spi_groups[] = {
	"gpio25", "gpio29", "gpio30",
};
static const char * const qdss_gpio11_groups[] = {
	"gpio25", "gpio79",
};
static const char * const qdss_gpio12_groups[] = {
	"gpio26", "gpio80",
};
static const char * const qdss_gpio13_groups[] = {
	"gpio27", "gpio93",
};
static const char * const qdss_gpio14_groups[] = {
	"gpio28", "gpio43",
};
static const char * const qdss_gpio15_groups[] = {
	"gpio29", "gpio44",
};
static const char * const cci_timer0_groups[] = {
	"gpio21",
};
static const char * const qdss_gpio8_groups[] = {
	"gpio21", "gpio75",
};
static const char * const vsense_data0_groups[] = {
	"gpio21",
};
static const char * const cci_timer1_groups[] = {
	"gpio22",
};
static const char * const qdss_gpio_groups[] = {
	"gpio22", "gpio30", "gpio123", "gpio124",
};
static const char * const vsense_data1_groups[] = {
	"gpio22",
};
static const char * const cci_timer2_groups[] = {
	"gpio23",
};
static const char * const blsp1_spi_b_groups[] = {
	"gpio23", "gpio28",
};
static const char * const qdss_gpio9_groups[] = {
	"gpio23", "gpio76",
};
static const char * const vsense_mode_groups[] = {
	"gpio23",
};
static const char * const cci_timer3_groups[] = {
	"gpio24",
};
static const char * const cci_async_groups[] = {
	"gpio24", "gpio25", "gpio26",
};
static const char * const blsp1_spi_a_groups[] = {
	"gpio24", "gpio27",
};
static const char * const qdss_gpio10_groups[] = {
	"gpio24", "gpio77",
};
static const char * const vsense_clkout_groups[] = {
	"gpio24",
};
static const char * const hdmi_rcv_groups[] = {
	"gpio30",
};
static const char * const hdmi_cec_groups[] = {
	"gpio31",
};
static const char * const blsp_spi2_groups[] = {
	"gpio31", "gpio32", "gpio33", "gpio34",
};
static const char * const blsp_uart2_a_groups[] = {
	"gpio31", "gpio32", "gpio33", "gpio34",
};
static const char * const blsp_uim2_a_groups[] = {
	"gpio31", "gpio34",
};
static const char * const pwr_modem_groups[] = {
	"gpio31",
};
static const char * const hdmi_ddc_groups[] = {
	"gpio32", "gpio33",
};
static const char * const blsp_i2c2_groups[] = {
	"gpio32", "gpio33",
};
static const char * const pwr_nav_groups[] = {
	"gpio32",
};
static const char * const pwr_crypto_groups[] = {
	"gpio33",
};
static const char * const hdmi_hot_groups[] = {
	"gpio34",
};
static const char * const edp_hot_groups[] = {
	"gpio34",
};
static const char * const pci_e0_groups[] = {
	"gpio35", "gpio36", "gpio37",
};
static const char * const jitter_bist_groups[] = {
	"gpio35",
};
static const char * const agera_pll_groups[] = {
	"gpio36", "gpio37",
};
static const char * const atest_tsens_groups[] = {
	"gpio36",
};
static const char * const usb_phy_groups[] = {
	"gpio38",
};
static const char * const lpass_slimbus_groups[] = {
	"gpio39", "gpio70", "gpio71", "gpio72",
};
static const char * const sd_write_groups[] = {
	"gpio40",
};
static const char * const tsif1_error_groups[] = {
	"gpio40",
};
static const char * const blsp_spi6_groups[] = {
	"gpio41", "gpio42", "gpio43", "gpio44",
};
static const char * const blsp_uart3_b_groups[] = {
	"gpio41", "gpio42", "gpio43", "gpio44",
};
static const char * const blsp_uim3_b_groups[] = {
	"gpio41", "gpio42",
};
static const char * const blsp_i2c6_groups[] = {
	"gpio43", "gpio44",
};
static const char * const bt_reset_groups[] = {
	"gpio45",
};
static const char * const blsp_spi3_groups[] = {
	"gpio45", "gpio46", "gpio47", "gpio48",
};
static const char * const blsp_uart3_a_groups[] = {
	"gpio45", "gpio46", "gpio47", "gpio48",
};
static const char * const blsp_uim3_a_groups[] = {
	"gpio45", "gpio46",
};
static const char * const blsp_i2c3_groups[] = {
	"gpio47", "gpio48",
};
static const char * const blsp_spi9_groups[] = {
	"gpio49", "gpio50", "gpio51", "gpio52",
};
static const char * const blsp_uart9_a_groups[] = {
	"gpio49", "gpio50", "gpio51", "gpio52",
};
static const char * const blsp_uim9_a_groups[] = {
	"gpio49", "gpio50",
};
static const char * const blsp10_spi_b_groups[] = {
	"gpio49", "gpio50",
};
static const char * const qdss_cti0_a_groups[] = {
	"gpio49", "gpio50",
};
static const char * const blsp_i2c9_groups[] = {
	"gpio51", "gpio52",
};
static const char * const blsp10_spi_a_groups[] = {
	"gpio51", "gpio52",
};
static const char * const blsp_spi7_groups[] = {
	"gpio53", "gpio54", "gpio55", "gpio56",
};
static const char * const blsp_uart7_a_groups[] = {
	"gpio53", "gpio54", "gpio55", "gpio56",
};
static const char * const blsp_uim7_a_groups[] = {
	"gpio53", "gpio54",
};
static const char * const blsp_i2c7_groups[] = {
	"gpio55", "gpio56",
};
static const char * const qua_mi2s_groups[] = {
	"gpio57", "gpio58", "gpio59", "gpio60", "gpio61", "gpio62", "gpio63",
};
static const char * const blsp10_spi_groups[] = {
	"gpio57",
};
static const char * const gcc_gp1_a_groups[] = {
	"gpio57",
};
static const char * const ssc_irq_groups[] = {
	"gpio58", "gpio59", "gpio60", "gpio61", "gpio62", "gpio63", "gpio78",
	"gpio79", "gpio80", "gpio117", "gpio118", "gpio119", "gpio120",
	"gpio121", "gpio122", "gpio123", "gpio124", "gpio125",
};
static const char * const blsp_spi11_groups[] = {
	"gpio58", "gpio59", "gpio60", "gpio61",
};
static const char * const blsp_uart8_b_groups[] = {
	"gpio58", "gpio59", "gpio60", "gpio61",
};
static const char * const blsp_uim8_b_groups[] = {
	"gpio58", "gpio59",
};
static const char * const gcc_gp2_a_groups[] = {
	"gpio58",
};
static const char * const qdss_cti1_a_groups[] = {
	"gpio58", "gpio59",
};
static const char * const gcc_gp3_a_groups[] = {
	"gpio59",
};
static const char * const blsp_i2c11_groups[] = {
	"gpio60", "gpio61",
};
static const char * const cri_trng0_groups[] = {
	"gpio60",
};
static const char * const cri_trng1_groups[] = {
	"gpio61",
};
static const char * const cri_trng_groups[] = {
	"gpio62",
};
static const char * const pri_mi2s_groups[] = {
	"gpio64", "gpio65", "gpio67", "gpio68",
};
static const char * const sp_cmu_groups[] = {
	"gpio64",
};
static const char * const blsp_spi10_groups[] = {
	"gpio65", "gpio66", "gpio67", "gpio68",
};
static const char * const blsp_uart7_b_groups[] = {
	"gpio65", "gpio66", "gpio67", "gpio68",
};
static const char * const blsp_uim7_b_groups[] = {
	"gpio65", "gpio66",
};
static const char * const pri_mi2s_ws_groups[] = {
	"gpio66",
};
static const char * const blsp_i2c10_groups[] = {
	"gpio67", "gpio68",
};
static const char * const spkr_i2s_groups[] = {
	"gpio69", "gpio70", "gpio71", "gpio72",
};
static const char * const audio_ref_groups[] = {
	"gpio69",
};
static const char * const blsp9_spi_groups[] = {
	"gpio70", "gpio71", "gpio72",
};
static const char * const tsense_pwm1_groups[] = {
	"gpio71",
};
static const char * const tsense_pwm2_groups[] = {
	"gpio71",
};
static const char * const btfm_slimbus_groups[] = {
	"gpio73", "gpio74",
};
static const char * const phase_flag0_groups[] = {
	"gpio73",
};
static const char * const ter_mi2s_groups[] = {
	"gpio74", "gpio75", "gpio76", "gpio77", "gpio78",
};
static const char * const phase_flag7_groups[] = {
	"gpio74",
};
static const char * const phase_flag8_groups[] = {
	"gpio75",
};
static const char * const phase_flag9_groups[] = {
	"gpio76",
};
static const char * const phase_flag4_groups[] = {
	"gpio77",
};
static const char * const gcc_gp1_b_groups[] = {
	"gpio78",
};
static const char * const sec_mi2s_groups[] = {
	"gpio79", "gpio80", "gpio81", "gpio82", "gpio83",
};
static const char * const blsp_spi12_groups[] = {
	"gpio81", "gpio82", "gpio83", "gpio84",
};
static const char * const blsp_uart9_b_groups[] = {
	"gpio81", "gpio82", "gpio83", "gpio84",
};
static const char * const blsp_uim9_b_groups[] = {
	"gpio81", "gpio82",
};
static const char * const gcc_gp2_b_groups[] = {
	"gpio81",
};
static const char * const gcc_gp3_b_groups[] = {
	"gpio82",
};
static const char * const blsp_i2c12_groups[] = {
	"gpio83", "gpio84",
};
static const char * const blsp_spi5_groups[] = {
	"gpio85", "gpio86", "gpio87", "gpio88",
};
static const char * const blsp_uart2_b_groups[] = {
	"gpio85", "gpio86", "gpio87", "gpio88",
};
static const char * const blsp_uim2_b_groups[] = {
	"gpio85", "gpio86",
};
static const char * const blsp_i2c5_groups[] = {
	"gpio87", "gpio88",
};
static const char * const tsif1_clk_groups[] = {
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
static const char * const blsp1_spi_groups[] = {
	"gpio90",
};
static const char * const tgu_ch0_groups[] = {
	"gpio90",
};
static const char * const qdss_cti1_b_groups[] = {
	"gpio90", "gpio91",
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
static const char * const phase_flag1_groups[] = {
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
static const char * const phase_flag2_groups[] = {
	"gpio92",
};
static const char * const tsif2_clk_groups[] = {
	"gpio93",
};
static const char * const sdc4_clk_groups[] = {
	"gpio93",
};
static const char * const tsif2_en_groups[] = {
	"gpio94",
};
static const char * const sdc42_groups[] = {
	"gpio94",
};
static const char * const sd_card_groups[] = {
	"gpio95",
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
static const char * const mdp_vsync_b_groups[] = {
	"gpio97", "gpio98",
};
static const char * const ldo_en_groups[] = {
	"gpio97",
};
static const char * const ldo_update_groups[] = {
	"gpio98",
};
static const char * const blsp_uart8_groups[] = {
	"gpio100", "gpio101",
};
static const char * const blsp11_i2c_groups[] = {
	"gpio102", "gpio103",
};
static const char * const prng_rosc_groups[] = {
	"gpio102",
};
static const char * const phase_flag5_groups[] = {
	"gpio103",
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
static const char * const phase_flag16_groups[] = {
	"gpio114",
};
static const char * const nav_dr_groups[] = {
	"gpio115",
};
static const char * const phase_flag11_groups[] = {
	"gpio115",
};
static const char * const phase_flag12_groups[] = {
	"gpio116",
};
static const char * const phase_flag13_groups[] = {
	"gpio117",
};
static const char * const atest_char_groups[] = {
	"gpio117",
};
static const char * const adsp_ext_groups[] = {
	"gpio118",
};
static const char * const phase_flag17_groups[] = {
	"gpio118",
};
static const char * const atest_char3_groups[] = {
	"gpio118",
};
static const char * const phase_flag18_groups[] = {
	"gpio119",
};
static const char * const atest_char2_groups[] = {
	"gpio119",
};
static const char * const phase_flag19_groups[] = {
	"gpio120",
};
static const char * const atest_char1_groups[] = {
	"gpio120",
};
static const char * const phase_flag20_groups[] = {
	"gpio121",
};
static const char * const atest_char0_groups[] = {
	"gpio121",
};
static const char * const phase_flag21_groups[] = {
	"gpio122",
};
static const char * const phase_flag22_groups[] = {
	"gpio123",
};
static const char * const phase_flag23_groups[] = {
	"gpio124",
};
static const char * const phase_flag24_groups[] = {
	"gpio125",
};
static const char * const phase_flag25_groups[] = {
	"gpio126",
};
static const char * const modem_tsync_groups[] = {
	"gpio128",
};
static const char * const nav_pps_groups[] = {
	"gpio128",
};
static const char * const phase_flag26_groups[] = {
	"gpio128",
};
static const char * const phase_flag27_groups[] = {
	"gpio129",
};
static const char * const qlink_request_groups[] = {
	"gpio130",
};
static const char * const phase_flag28_groups[] = {
	"gpio130",
};
static const char * const qlink_enable_groups[] = {
	"gpio131",
};
static const char * const phase_flag6_groups[] = {
	"gpio131",
};
static const char * const phase_flag29_groups[] = {
	"gpio132",
};
static const char * const phase_flag30_groups[] = {
	"gpio133",
};
static const char * const phase_flag31_groups[] = {
	"gpio134",
};
static const char * const pa_indicator_groups[] = {
	"gpio135",
};
static const char * const ssbi1_groups[] = {
	"gpio142",
};
static const char * const isense_dbg_groups[] = {
	"gpio143",
};
static const char * const mss_lte_groups[] = {
	"gpio144", "gpio145",
};

static const struct msm_function msmhamster_functions[] = {
	FUNCTION(blsp_spi1),
	FUNCTION(gpio),
	FUNCTION(blsp_uim1_a),
	FUNCTION(blsp_uart1_a),
	FUNCTION(blsp_i2c1),
	FUNCTION(blsp_spi8),
	FUNCTION(blsp_uart8_a),
	FUNCTION(blsp_uim8_a),
	FUNCTION(qdss_cti0_b),
	FUNCTION(blsp_i2c8),
	FUNCTION(ddr_bist),
	FUNCTION(atest_tsens2),
	FUNCTION(atest_usb1),
	FUNCTION(blsp_spi4),
	FUNCTION(blsp_uart1_b),
	FUNCTION(blsp_uim1_b),
	FUNCTION(wlan1_adc1),
	FUNCTION(atest_usb13),
	FUNCTION(bimc_dte1),
	FUNCTION(tsif1_sync),
	FUNCTION(wlan1_adc0),
	FUNCTION(atest_usb12),
	FUNCTION(bimc_dte0),
	FUNCTION(mdp_vsync_a),
	FUNCTION(blsp_i2c4),
	FUNCTION(atest_gpsadc1),
	FUNCTION(wlan2_adc1),
	FUNCTION(atest_usb11),
	FUNCTION(edp_lcd),
	FUNCTION(dbg_out),
	FUNCTION(atest_gpsadc0),
	FUNCTION(wlan2_adc0),
	FUNCTION(atest_usb10),
	FUNCTION(mdp_vsync),
	FUNCTION(m_voc),
	FUNCTION(cam_mclk),
	FUNCTION(pll_bypassnl),
	FUNCTION(qdss_gpio0),
	FUNCTION(pll_reset),
	FUNCTION(qdss_gpio1),
	FUNCTION(qdss_gpio2),
	FUNCTION(qdss_gpio3),
	FUNCTION(cci_i2c),
	FUNCTION(qdss_gpio4),
	FUNCTION(phase_flag14),
	FUNCTION(qdss_gpio5),
	FUNCTION(phase_flag15),
	FUNCTION(qdss_gpio6),
	FUNCTION(qdss_gpio7),
	FUNCTION(cci_timer4),
	FUNCTION(blsp2_spi),
	FUNCTION(qdss_gpio11),
	FUNCTION(qdss_gpio12),
	FUNCTION(qdss_gpio13),
	FUNCTION(qdss_gpio14),
	FUNCTION(qdss_gpio15),
	FUNCTION(cci_timer0),
	FUNCTION(qdss_gpio8),
	FUNCTION(vsense_data0),
	FUNCTION(cci_timer1),
	FUNCTION(qdss_gpio),
	FUNCTION(vsense_data1),
	FUNCTION(cci_timer2),
	FUNCTION(blsp1_spi_b),
	FUNCTION(qdss_gpio9),
	FUNCTION(vsense_mode),
	FUNCTION(cci_timer3),
	FUNCTION(cci_async),
	FUNCTION(blsp1_spi_a),
	FUNCTION(qdss_gpio10),
	FUNCTION(vsense_clkout),
	FUNCTION(hdmi_rcv),
	FUNCTION(hdmi_cec),
	FUNCTION(blsp_spi2),
	FUNCTION(blsp_uart2_a),
	FUNCTION(blsp_uim2_a),
	FUNCTION(pwr_modem),
	FUNCTION(hdmi_ddc),
	FUNCTION(blsp_i2c2),
	FUNCTION(pwr_nav),
	FUNCTION(pwr_crypto),
	FUNCTION(hdmi_hot),
	FUNCTION(edp_hot),
	FUNCTION(pci_e0),
	FUNCTION(jitter_bist),
	FUNCTION(agera_pll),
	FUNCTION(atest_tsens),
	FUNCTION(usb_phy),
	FUNCTION(lpass_slimbus),
	FUNCTION(sd_write),
	FUNCTION(tsif1_error),
	FUNCTION(blsp_spi6),
	FUNCTION(blsp_uart3_b),
	FUNCTION(blsp_uim3_b),
	FUNCTION(blsp_i2c6),
	FUNCTION(bt_reset),
	FUNCTION(blsp_spi3),
	FUNCTION(blsp_uart3_a),
	FUNCTION(blsp_uim3_a),
	FUNCTION(blsp_i2c3),
	FUNCTION(blsp_spi9),
	FUNCTION(blsp_uart9_a),
	FUNCTION(blsp_uim9_a),
	FUNCTION(blsp10_spi_b),
	FUNCTION(qdss_cti0_a),
	FUNCTION(blsp_i2c9),
	FUNCTION(blsp10_spi_a),
	FUNCTION(blsp_spi7),
	FUNCTION(blsp_uart7_a),
	FUNCTION(blsp_uim7_a),
	FUNCTION(blsp_i2c7),
	FUNCTION(qua_mi2s),
	FUNCTION(blsp10_spi),
	FUNCTION(gcc_gp1_a),
	FUNCTION(ssc_irq),
	FUNCTION(blsp_spi11),
	FUNCTION(blsp_uart8_b),
	FUNCTION(blsp_uim8_b),
	FUNCTION(gcc_gp2_a),
	FUNCTION(qdss_cti1_a),
	FUNCTION(gcc_gp3_a),
	FUNCTION(blsp_i2c11),
	FUNCTION(cri_trng0),
	FUNCTION(cri_trng1),
	FUNCTION(cri_trng),
	FUNCTION(pri_mi2s),
	FUNCTION(sp_cmu),
	FUNCTION(blsp_spi10),
	FUNCTION(blsp_uart7_b),
	FUNCTION(blsp_uim7_b),
	FUNCTION(pri_mi2s_ws),
	FUNCTION(blsp_i2c10),
	FUNCTION(spkr_i2s),
	FUNCTION(audio_ref),
	FUNCTION(blsp9_spi),
	FUNCTION(tsense_pwm1),
	FUNCTION(tsense_pwm2),
	FUNCTION(btfm_slimbus),
	FUNCTION(phase_flag0),
	FUNCTION(ter_mi2s),
	FUNCTION(phase_flag7),
	FUNCTION(phase_flag8),
	FUNCTION(phase_flag9),
	FUNCTION(phase_flag4),
	FUNCTION(gcc_gp1_b),
	FUNCTION(sec_mi2s),
	FUNCTION(blsp_spi12),
	FUNCTION(blsp_uart9_b),
	FUNCTION(blsp_uim9_b),
	FUNCTION(gcc_gp2_b),
	FUNCTION(gcc_gp3_b),
	FUNCTION(blsp_i2c12),
	FUNCTION(blsp_spi5),
	FUNCTION(blsp_uart2_b),
	FUNCTION(blsp_uim2_b),
	FUNCTION(blsp_i2c5),
	FUNCTION(tsif1_clk),
	FUNCTION(phase_flag10),
	FUNCTION(tsif1_en),
	FUNCTION(mdp_vsync0),
	FUNCTION(mdp_vsync1),
	FUNCTION(mdp_vsync2),
	FUNCTION(mdp_vsync3),
	FUNCTION(blsp1_spi),
	FUNCTION(tgu_ch0),
	FUNCTION(qdss_cti1_b),
	FUNCTION(tsif1_data),
	FUNCTION(sdc4_cmd),
	FUNCTION(tgu_ch1),
	FUNCTION(phase_flag1),
	FUNCTION(tsif2_error),
	FUNCTION(sdc43),
	FUNCTION(vfr_1),
	FUNCTION(phase_flag2),
	FUNCTION(tsif2_clk),
	FUNCTION(sdc4_clk),
	FUNCTION(tsif2_en),
	FUNCTION(sdc42),
	FUNCTION(sd_card),
	FUNCTION(tsif2_data),
	FUNCTION(sdc41),
	FUNCTION(tsif2_sync),
	FUNCTION(sdc40),
	FUNCTION(phase_flag3),
	FUNCTION(mdp_vsync_b),
	FUNCTION(ldo_en),
	FUNCTION(ldo_update),
	FUNCTION(blsp_uart8),
	FUNCTION(blsp11_i2c),
	FUNCTION(prng_rosc),
	FUNCTION(phase_flag5),
	FUNCTION(uim2_data),
	FUNCTION(uim2_clk),
	FUNCTION(uim2_reset),
	FUNCTION(uim2_present),
	FUNCTION(uim1_data),
	FUNCTION(uim1_clk),
	FUNCTION(uim1_reset),
	FUNCTION(uim1_present),
	FUNCTION(uim_batt),
	FUNCTION(phase_flag16),
	FUNCTION(nav_dr),
	FUNCTION(phase_flag11),
	FUNCTION(phase_flag12),
	FUNCTION(phase_flag13),
	FUNCTION(atest_char),
	FUNCTION(adsp_ext),
	FUNCTION(phase_flag17),
	FUNCTION(atest_char3),
	FUNCTION(phase_flag18),
	FUNCTION(atest_char2),
	FUNCTION(phase_flag19),
	FUNCTION(atest_char1),
	FUNCTION(phase_flag20),
	FUNCTION(atest_char0),
	FUNCTION(phase_flag21),
	FUNCTION(phase_flag22),
	FUNCTION(phase_flag23),
	FUNCTION(phase_flag24),
	FUNCTION(phase_flag25),
	FUNCTION(modem_tsync),
	FUNCTION(nav_pps),
	FUNCTION(phase_flag26),
	FUNCTION(phase_flag27),
	FUNCTION(qlink_request),
	FUNCTION(phase_flag28),
	FUNCTION(qlink_enable),
	FUNCTION(phase_flag6),
	FUNCTION(phase_flag29),
	FUNCTION(phase_flag30),
	FUNCTION(phase_flag31),
	FUNCTION(pa_indicator),
	FUNCTION(ssbi1),
	FUNCTION(isense_dbg),
	FUNCTION(mss_lte),
};

static const struct msm_pingroup msmhamster_groups[] = {
	PINGROUP(0, EAST, blsp_spi1, blsp_uart1_a, blsp_uim1_a, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(1, EAST, blsp_spi1, blsp_uart1_a, blsp_uim1_a, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(2, EAST, blsp_spi1, blsp_uart1_a, blsp_i2c1, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(3, EAST, blsp_spi1, blsp_uart1_a, blsp_i2c1, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(4, WEST, blsp_spi8, blsp_uart8_a, blsp_uim8_a, NA,
		 qdss_cti0_b, NA, NA, NA, NA),
	PINGROUP(5, WEST, blsp_spi8, blsp_uart8_a, blsp_uim8_a, NA,
		 qdss_cti0_b, NA, NA, NA, NA),
	PINGROUP(6, WEST, blsp_spi8, blsp_uart8_a, blsp_i2c8, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(7, WEST, blsp_spi8, blsp_uart8_a, blsp_i2c8, ddr_bist, NA,
		 atest_tsens2, atest_usb1, NA, NA),
	PINGROUP(8, EAST, blsp_spi4, blsp_uart1_b, blsp_uim1_b, NA, ddr_bist,
		 NA, wlan1_adc1, atest_usb13, bimc_dte1),
	PINGROUP(9, EAST, blsp_spi4, blsp_uart1_b, blsp_uim1_b, tsif1_sync,
		 ddr_bist, NA, wlan1_adc0, atest_usb12, bimc_dte0),
	PINGROUP(10, EAST, mdp_vsync_a, blsp_spi4, blsp_uart1_b, blsp_i2c4,
		 ddr_bist, atest_gpsadc1, wlan2_adc1, atest_usb11, bimc_dte1),
	PINGROUP(11, EAST, mdp_vsync_a, edp_lcd, blsp_spi4, blsp_uart1_b,
		 blsp_i2c4, dbg_out, atest_gpsadc0, wlan2_adc0, atest_usb10),
	PINGROUP(12, EAST, mdp_vsync, m_voc, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(13, EAST, cam_mclk, pll_bypassnl, qdss_gpio0, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(14, EAST, cam_mclk, pll_reset, qdss_gpio1, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(15, EAST, cam_mclk, qdss_gpio2, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(16, EAST, cam_mclk, qdss_gpio3, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(17, EAST, cci_i2c, qdss_gpio4, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(18, EAST, cci_i2c, phase_flag14, qdss_gpio5, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(19, EAST, cci_i2c, phase_flag15, qdss_gpio6, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(20, EAST, cci_i2c, qdss_gpio7, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(21, EAST, cci_timer0, NA, qdss_gpio8, vsense_data0, NA, NA,
		 NA, NA, NA),
	PINGROUP(22, EAST, cci_timer1, NA, qdss_gpio, vsense_data1, NA, NA, NA,
		 NA, NA),
	PINGROUP(23, EAST, cci_timer2, blsp1_spi_b, qdss_gpio9, vsense_mode,
		 NA, NA, NA, NA, NA),
	PINGROUP(24, EAST, cci_timer3, cci_async, blsp1_spi_a, NA, qdss_gpio10,
		 vsense_clkout, NA, NA, NA),
	PINGROUP(25, EAST, cci_timer4, cci_async, blsp2_spi, NA, qdss_gpio11,
		 NA, NA, NA, NA),
	PINGROUP(26, EAST, cci_async, qdss_gpio12, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(27, EAST, blsp1_spi_a, qdss_gpio13, NA, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(28, EAST, blsp1_spi_b, qdss_gpio14, NA, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(29, EAST, blsp2_spi, NA, qdss_gpio15, NA, NA, NA, NA, NA, NA),
	PINGROUP(30, EAST, hdmi_rcv, blsp2_spi, qdss_gpio, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(31, EAST, hdmi_cec, blsp_spi2, blsp_uart2_a, blsp_uim2_a,
		 pwr_modem, NA, NA, NA, NA),
	PINGROUP(32, EAST, hdmi_ddc, blsp_spi2, blsp_uart2_a, blsp_i2c2,
		 pwr_nav, NA, NA, NA, NA),
	PINGROUP(33, EAST, hdmi_ddc, blsp_spi2, blsp_uart2_a, blsp_i2c2,
		 pwr_crypto, NA, NA, NA, NA),
	PINGROUP(34, EAST, hdmi_hot, edp_hot, blsp_spi2, blsp_uart2_a,
		 blsp_uim2_a, NA, NA, NA, NA),
	PINGROUP(35, WEST, pci_e0, jitter_bist, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(36, WEST, pci_e0, agera_pll, NA, atest_tsens, NA, NA, NA, NA,
		 NA),
	PINGROUP(37, WEST, agera_pll, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(38, WEST, usb_phy, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(39, WEST, lpass_slimbus, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(40, EAST, sd_write, tsif1_error, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(41, EAST, blsp_spi6, blsp_uart3_b, blsp_uim3_b, NA,
		 qdss_gpio6, NA, NA, NA, NA),
	PINGROUP(42, EAST, blsp_spi6, blsp_uart3_b, blsp_uim3_b, NA,
		 qdss_gpio7, NA, NA, NA, NA),
	PINGROUP(43, EAST, blsp_spi6, blsp_uart3_b, blsp_i2c6, NA, qdss_gpio14,
		 NA, NA, NA, NA),
	PINGROUP(44, EAST, blsp_spi6, blsp_uart3_b, blsp_i2c6, NA, qdss_gpio15,
		 NA, NA, NA, NA),
	PINGROUP(45, EAST, blsp_spi3, blsp_uart3_a, blsp_uim3_a, NA, NA, NA,
		 NA, NA, NA),
	PINGROUP(46, EAST, blsp_spi3, blsp_uart3_a, blsp_uim3_a, NA, NA, NA,
		 NA, NA, NA),
	PINGROUP(47, EAST, blsp_spi3, blsp_uart3_a, blsp_i2c3, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(48, EAST, blsp_spi3, blsp_uart3_a, blsp_i2c3, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(49, NORTH, blsp_spi9, blsp_uart9_a, blsp_uim9_a, blsp10_spi_b,
		 qdss_cti0_a, NA, NA, NA, NA),
	PINGROUP(50, NORTH, blsp_spi9, blsp_uart9_a, blsp_uim9_a, blsp10_spi_b,
		 qdss_cti0_a, NA, NA, NA, NA),
	PINGROUP(51, NORTH, blsp_spi9, blsp_uart9_a, blsp_i2c9, blsp10_spi_a,
		 NA, NA, NA, NA, NA),
	PINGROUP(52, NORTH, blsp_spi9, blsp_uart9_a, blsp_i2c9, blsp10_spi_a,
		 NA, NA, NA, NA, NA),
	PINGROUP(53, WEST, blsp_spi7, blsp_uart7_a, blsp_uim7_a, NA, NA, NA,
		 NA, NA, NA),
	PINGROUP(54, WEST, blsp_spi7, blsp_uart7_a, blsp_uim7_a, NA, NA, NA,
		 NA, NA, NA),
	PINGROUP(55, WEST, blsp_spi7, blsp_uart7_a, blsp_i2c7, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(56, WEST, blsp_spi7, blsp_uart7_a, blsp_i2c7, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(57, WEST, qua_mi2s, blsp10_spi, gcc_gp1_a, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(58, NORTH, qua_mi2s, blsp_spi11, blsp_uart8_b, blsp_uim8_b,
		 gcc_gp2_a, NA, qdss_cti1_a, NA, NA),
	PINGROUP(59, NORTH, qua_mi2s, blsp_spi11, blsp_uart8_b, blsp_uim8_b,
		 gcc_gp3_a, NA, qdss_cti1_a, NA, NA),
	PINGROUP(60, NORTH, qua_mi2s, blsp_spi11, blsp_uart8_b, blsp_i2c11,
		 cri_trng0, NA, NA, NA, NA),
	PINGROUP(61, NORTH, qua_mi2s, blsp_spi11, blsp_uart8_b, blsp_i2c11,
		 cri_trng1, NA, NA, NA, NA),
	PINGROUP(62, WEST, qua_mi2s, cri_trng, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(63, WEST, qua_mi2s, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(64, WEST, pri_mi2s, sp_cmu, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(65, WEST, pri_mi2s, blsp_spi10, blsp_uart7_b, blsp_uim7_b, NA,
		 NA, NA, NA, NA),
	PINGROUP(66, WEST, pri_mi2s_ws, blsp_spi10, blsp_uart7_b, blsp_uim7_b,
		 NA, NA, NA, NA, NA),
	PINGROUP(67, WEST, pri_mi2s, blsp_spi10, blsp_uart7_b, blsp_i2c10, NA,
		 NA, NA, NA, NA),
	PINGROUP(68, WEST, pri_mi2s, blsp_spi10, blsp_uart7_b, blsp_i2c10, NA,
		 NA, NA, NA, NA),
	PINGROUP(69, WEST, spkr_i2s, audio_ref, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(70, WEST, lpass_slimbus, spkr_i2s, blsp9_spi, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(71, WEST, lpass_slimbus, spkr_i2s, blsp9_spi, tsense_pwm1,
		 tsense_pwm2, NA, NA, NA, NA),
	PINGROUP(72, WEST, lpass_slimbus, spkr_i2s, blsp9_spi, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(73, WEST, btfm_slimbus, phase_flag0, NA, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(74, WEST, btfm_slimbus, ter_mi2s, phase_flag7, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(75, WEST, ter_mi2s, phase_flag8, qdss_gpio8, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(76, WEST, ter_mi2s, phase_flag9, qdss_gpio9, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(77, WEST, ter_mi2s, phase_flag4, qdss_gpio10, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(78, WEST, ter_mi2s, gcc_gp1_b, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(79, WEST, sec_mi2s, NA, qdss_gpio11, NA, NA, NA, NA, NA, NA),
	PINGROUP(80, WEST, sec_mi2s, NA, qdss_gpio12, NA, NA, NA, NA, NA, NA),
	PINGROUP(81, WEST, sec_mi2s, blsp_spi12, blsp_uart9_b, blsp_uim9_b,
		 gcc_gp2_b, NA, NA, NA, NA),
	PINGROUP(82, WEST, sec_mi2s, blsp_spi12, blsp_uart9_b, blsp_uim9_b,
		 gcc_gp3_b, NA, NA, NA, NA),
	PINGROUP(83, WEST, sec_mi2s, blsp_spi12, blsp_uart9_b, blsp_i2c12, NA,
		 NA, NA, NA, NA),
	PINGROUP(84, WEST, blsp_spi12, blsp_uart9_b, blsp_i2c12, NA, NA, NA,
		 NA, NA, NA),
	PINGROUP(85, EAST, blsp_spi5, blsp_uart2_b, blsp_uim2_b, NA, NA, NA,
		 NA, NA, NA),
	PINGROUP(86, EAST, blsp_spi5, blsp_uart2_b, blsp_uim2_b, NA, NA, NA,
		 NA, NA, NA),
	PINGROUP(87, EAST, blsp_spi5, blsp_uart2_b, blsp_i2c5, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(88, EAST, blsp_spi5, blsp_uart2_b, blsp_i2c5, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(89, EAST, tsif1_clk, phase_flag10, NA, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(90, EAST, tsif1_en, mdp_vsync0, mdp_vsync1, mdp_vsync2,
		 mdp_vsync3, blsp1_spi, tgu_ch0, qdss_cti1_b, NA),
	PINGROUP(91, EAST, tsif1_data, sdc4_cmd, tgu_ch1, phase_flag1,
		 qdss_cti1_b, NA, NA, NA, NA),
	PINGROUP(92, EAST, tsif2_error, sdc43, vfr_1, phase_flag2, NA, NA, NA,
		 NA, NA),
	PINGROUP(93, EAST, tsif2_clk, sdc4_clk, NA, qdss_gpio13, NA, NA, NA,
		 NA, NA),
	PINGROUP(94, EAST, tsif2_en, sdc42, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(95, EAST, tsif2_data, sdc41, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(96, EAST, tsif2_sync, sdc40, phase_flag3, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(97, WEST, NA, mdp_vsync_b, ldo_en, NA, NA, NA, NA, NA, NA),
	PINGROUP(98, WEST, NA, mdp_vsync_b, ldo_update, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(99, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(100, WEST, NA, NA, blsp_uart8, NA, NA, NA, NA, NA, NA),
	PINGROUP(101, WEST, NA, blsp_uart8, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(102, WEST, NA, blsp11_i2c, prng_rosc, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(103, WEST, NA, blsp11_i2c, phase_flag5, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(104, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(105, NORTH, uim2_data, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(106, NORTH, uim2_clk, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(107, NORTH, uim2_reset, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(108, NORTH, uim2_present, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(109, NORTH, uim1_data, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(110, NORTH, uim1_clk, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(111, NORTH, uim1_reset, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(112, NORTH, uim1_present, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(113, NORTH, uim_batt, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(114, WEST, NA, NA, phase_flag16, NA, NA, NA, NA, NA, NA),
	PINGROUP(115, WEST, NA, nav_dr, phase_flag11, NA, NA, NA, NA, NA, NA),
	PINGROUP(116, WEST, phase_flag12, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(117, EAST, phase_flag13, qdss_gpio0, atest_char, NA, NA, NA,
		 NA, NA, NA),
	PINGROUP(118, EAST, adsp_ext, phase_flag17, qdss_gpio1, atest_char3,
		 NA, NA, NA, NA, NA),
	PINGROUP(119, EAST, phase_flag18, qdss_gpio2, atest_char2, NA, NA, NA,
		 NA, NA, NA),
	PINGROUP(120, EAST, phase_flag19, qdss_gpio3, atest_char1, NA, NA, NA,
		 NA, NA, NA),
	PINGROUP(121, EAST, phase_flag20, qdss_gpio4, atest_char0, NA, NA, NA,
		 NA, NA, NA),
	PINGROUP(122, EAST, phase_flag21, qdss_gpio5, NA, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(123, EAST, phase_flag22, qdss_gpio, NA, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(124, EAST, phase_flag23, qdss_gpio, NA, NA, NA, NA, NA, NA,
		 NA),
	PINGROUP(125, EAST, phase_flag24, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(126, EAST, phase_flag25, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(127, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(128, WEST, modem_tsync, nav_pps, phase_flag26, NA, NA, NA,
		 NA, NA, NA),
	PINGROUP(129, WEST, phase_flag27, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(130, NORTH, qlink_request, phase_flag28, NA, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(131, NORTH, qlink_enable, phase_flag6, NA, NA, NA, NA, NA,
		 NA, NA),
	PINGROUP(132, WEST, NA, phase_flag29, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(133, WEST, phase_flag30, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(134, WEST, phase_flag31, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(135, WEST, NA, pa_indicator, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(136, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(137, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(138, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(139, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(140, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(141, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(142, WEST, NA, ssbi1, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(143, WEST, isense_dbg, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(144, WEST, mss_lte, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(145, WEST, mss_lte, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(146, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(147, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(148, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(149, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	SDC_QDSD_PINGROUP(sdc2_clk, 0x999000, 14, 6),
	SDC_QDSD_PINGROUP(sdc2_cmd, 0x999000, 11, 3),
	SDC_QDSD_PINGROUP(sdc2_data, 0x999000, 9, 0),
};

static const struct msm_pinctrl_soc_data msmhamster_pinctrl = {
	.pins = msmhamster_pins,
	.npins = ARRAY_SIZE(msmhamster_pins),
	.functions = msmhamster_functions,
	.nfunctions = ARRAY_SIZE(msmhamster_functions),
	.groups = msmhamster_groups,
	.ngroups = ARRAY_SIZE(msmhamster_groups),
	.ngpios = 153,
};

static int msmhamster_pinctrl_probe(struct platform_device *pdev)
{
	return msm_pinctrl_probe(pdev, &msmhamster_pinctrl);
}

static const struct of_device_id msmhamster_pinctrl_of_match[] = {
	{ .compatible = "qcom,msmhamster-pinctrl", },
	{ },
};

static struct platform_driver msmhamster_pinctrl_driver = {
	.driver = {
		.name = "msmhamster-pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = msmhamster_pinctrl_of_match,
	},
	.probe = msmhamster_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init msmhamster_pinctrl_init(void)
{
	return platform_driver_register(&msmhamster_pinctrl_driver);
}
arch_initcall(msmhamster_pinctrl_init);

static void __exit msmhamster_pinctrl_exit(void)
{
	platform_driver_unregister(&msmhamster_pinctrl_driver);
}
module_exit(msmhamster_pinctrl_exit);

MODULE_DESCRIPTION("QTI msmhamster pinctrl driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, msmhamster_pinctrl_of_match);
