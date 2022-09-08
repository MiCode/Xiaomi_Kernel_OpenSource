// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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


static const struct pinctrl_pin_desc sdxbaagha_pins[] = {
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


enum sdxbaagha_functions {
	msm_mux_gpio,
	msm_mux_adsp_ext_vfr,
	msm_mux_atest_bbrx_or0,
	msm_mux_atest_bbrx_or1,
	msm_mux_atest_char_start,
	msm_mux_atest_char_status0,
	msm_mux_atest_char_status1,
	msm_mux_atest_char_status2,
	msm_mux_atest_char_status3,
	msm_mux_atest_gpsadc_dtestout00,
	msm_mux_atest_gpsadc_dtestout01,
	msm_mux_atest_gpsadc_dtestout10,
	msm_mux_atest_gpsadc_dtestout11,
	msm_mux_atest_usb2_atereset,
	msm_mux_atest_usb2_testdataout0,
	msm_mux_atest_usb2_testdataout1,
	msm_mux_atest_usb2_testdataout2,
	msm_mux_atest_usb2_testdataout3,
	msm_mux_audio_ref_clk,
	msm_mux_char_exec_pending,
	msm_mux_char_exec_release,
	msm_mux_clk_dac_gsm,
	msm_mux_coex_uart_rx,
	msm_mux_coex_uart_tx,
	msm_mux_cri_trng_rosc,
	msm_mux_cri_trng_rosc0,
	msm_mux_cri_trng_rosc1,
	msm_mux_dbg_out_clk,
	msm_mux_ddr_bist_complete,
	msm_mux_ddr_bist_fail,
	msm_mux_ddr_bist_start,
	msm_mux_ddr_bist_stop,
	msm_mux_ddr_pxi0_test,
	msm_mux_ebi2_lcd_a,
	msm_mux_ebi2_lcd_cs,
	msm_mux_ebi2_lcd_reset,
	msm_mux_ebi2_lcd_te,
	msm_mux_emac_mdc,
	msm_mux_emac_mdio,
	msm_mux_emac_pps_in,
	msm_mux_emac_ptp_aux,
	msm_mux_emac_ptp_pps,
	msm_mux_etdac_calib_data0,
	msm_mux_etdac_calib_data1,
	msm_mux_etdac_calib_data10,
	msm_mux_etdac_calib_data11,
	msm_mux_etdac_calib_data12,
	msm_mux_etdac_calib_data13,
	msm_mux_etdac_calib_data14,
	msm_mux_etdac_calib_data15,
	msm_mux_etdac_calib_data16,
	msm_mux_etdac_calib_data17,
	msm_mux_etdac_calib_data18,
	msm_mux_etdac_calib_data19,
	msm_mux_etdac_calib_data2,
	msm_mux_etdac_calib_data20,
	msm_mux_etdac_calib_data21,
	msm_mux_etdac_calib_data22,
	msm_mux_etdac_calib_data23,
	msm_mux_etdac_calib_data24,
	msm_mux_etdac_calib_data25,
	msm_mux_etdac_calib_data3,
	msm_mux_etdac_calib_data4,
	msm_mux_etdac_calib_data5,
	msm_mux_etdac_calib_data6,
	msm_mux_etdac_calib_data7,
	msm_mux_etdac_calib_data8,
	msm_mux_etdac_calib_data9,
	msm_mux_gcc_gp1_clk,
	msm_mux_gcc_gp2_clk,
	msm_mux_gcc_gp3_clk,
	msm_mux_gcc_plltest_bypassnl,
	msm_mux_gcc_plltest_resetn,
	msm_mux_gsm_tx_phase,
	msm_mux_jitter_bist_ref,
	msm_mux_m_voc_ext,
	msm_mux_mgpi_clk_req,
	msm_mux_mi2s0_data0,
	msm_mux_mi2s0_data1,
	msm_mux_mi2s0_sck,
	msm_mux_mi2s0_ws,
	msm_mux_mi2s1_data0,
	msm_mux_mi2s1_data1,
	msm_mux_mi2s1_sck,
	msm_mux_mi2s1_ws,
	msm_mux_mi2s_mclk,
	msm_mux_native_tsense_pwm1,
	msm_mux_nav_gpio_0,
	msm_mux_nav_gpio_1,
	msm_mux_nav_gpio_2,
	msm_mux_pa_indicator_1,
	msm_mux_pci_e_rst,
	msm_mux_pcie_clkreq_n,
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
	msm_mux_qup0_se0_l0,
	msm_mux_qup0_se0_l1,
	msm_mux_qup0_se0_l2,
	msm_mux_qup0_se0_l3,
	msm_mux_qup0_se0_l4,
	msm_mux_qup0_se0_l5,
	msm_mux_qup0_se0_l6,
	msm_mux_qup0_se1_l0,
	msm_mux_qup0_se1_l1,
	msm_mux_qup0_se1_l2,
	msm_mux_qup0_se1_l3,
	msm_mux_qup0_se1_l4,
	msm_mux_qup0_se1_l5,
	msm_mux_qup0_se1_l6,
	msm_mux_qup0_se2_l0,
	msm_mux_qup0_se2_l1,
	msm_mux_qup0_se2_l2,
	msm_mux_qup0_se2_l3,
	msm_mux_qup0_se2_l4,
	msm_mux_qup0_se2_l5,
	msm_mux_qup0_se2_l6,
	msm_mux_qup0_se3_l0_mira,
	msm_mux_qup0_se3_l0_mirb,
	msm_mux_qup0_se3_l1_mira,
	msm_mux_qup0_se3_l1_mirb,
	msm_mux_qup0_se3_l2_mira,
	msm_mux_qup0_se3_l2_mirb,
	msm_mux_qup0_se3_l3_mira,
	msm_mux_qup0_se3_l3_mirb,
	msm_mux_qup0_se3_l4,
	msm_mux_qup0_se3_l5,
	msm_mux_qup0_se3_l6,
	msm_mux_qup0_se4_l0,
	msm_mux_qup0_se4_l1,
	msm_mux_qup0_se4_l2,
	msm_mux_qup0_se4_l3,
	msm_mux_qup0_se4_l4,
	msm_mux_qup0_se4_l5,
	msm_mux_qup0_se4_l6,
	msm_mux_sdc40,
	msm_mux_sdc41,
	msm_mux_sdc42,
	msm_mux_sdc43,
	msm_mux_sdc4_clk,
	msm_mux_sdc4_cmd,
	msm_mux_sdc4_tb_trig,
	msm_mux_sgmii_phy_intr,
	msm_mux_spmi_coex_clk,
	msm_mux_spmi_coex_data,
	msm_mux_spmi_vgi_hwevent,
	msm_mux_tgu_ch0_trigout,
	msm_mux_tmess_prng_rosc0,
	msm_mux_tmess_prng_rosc1,
	msm_mux_tmess_prng_rosc2,
	msm_mux_tmess_prng_rosc3,
	msm_mux_txdac_calib_data0,
	msm_mux_txdac_calib_data1,
	msm_mux_txdac_calib_data10,
	msm_mux_txdac_calib_data11,
	msm_mux_txdac_calib_data12,
	msm_mux_txdac_calib_data13,
	msm_mux_txdac_calib_data14,
	msm_mux_txdac_calib_data15,
	msm_mux_txdac_calib_data16,
	msm_mux_txdac_calib_data17,
	msm_mux_txdac_calib_data18,
	msm_mux_txdac_calib_data19,
	msm_mux_txdac_calib_data2,
	msm_mux_txdac_calib_data20,
	msm_mux_txdac_calib_data21,
	msm_mux_txdac_calib_data22,
	msm_mux_txdac_calib_data23,
	msm_mux_txdac_calib_data24,
	msm_mux_txdac_calib_data25,
	msm_mux_txdac_calib_data3,
	msm_mux_txdac_calib_data4,
	msm_mux_txdac_calib_data5,
	msm_mux_txdac_calib_data6,
	msm_mux_txdac_calib_data7,
	msm_mux_txdac_calib_data8,
	msm_mux_txdac_calib_data9,
	msm_mux_uim0_clk,
	msm_mux_uim0_data,
	msm_mux_uim0_present,
	msm_mux_uim0_reset,
	msm_mux_uim1_clk,
	msm_mux_uim1_data,
	msm_mux_uim1_present,
	msm_mux_uim1_reset,
	msm_mux_usb2phy_ac_en,
	msm_mux_vsense_trigger_mirnat,
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
	"gpio105", "gpio106", "gpio107", "gpio108", "gpio109",
};
static const char * const adsp_ext_vfr_groups[] = {
	"gpio24", "gpio25",
};
static const char * const atest_bbrx_or0_groups[] = {
	"gpio18",
};
static const char * const atest_bbrx_or1_groups[] = {
	"gpio19",
};
static const char * const atest_char_start_groups[] = {
	"gpio43",
};
static const char * const atest_char_status0_groups[] = {
	"gpio72",
};
static const char * const atest_char_status1_groups[] = {
	"gpio73",
};
static const char * const atest_char_status2_groups[] = {
	"gpio74",
};
static const char * const atest_char_status3_groups[] = {
	"gpio77",
};
static const char * const atest_gpsadc_dtestout00_groups[] = {
	"gpio52",
};
static const char * const atest_gpsadc_dtestout01_groups[] = {
	"gpio54",
};
static const char * const atest_gpsadc_dtestout10_groups[] = {
	"gpio55",
};
static const char * const atest_gpsadc_dtestout11_groups[] = {
	"gpio60",
};
static const char * const atest_usb2_atereset_groups[] = {
	"gpio9",
};
static const char * const atest_usb2_testdataout0_groups[] = {
	"gpio18",
};
static const char * const atest_usb2_testdataout1_groups[] = {
	"gpio19",
};
static const char * const atest_usb2_testdataout2_groups[] = {
	"gpio48",
};
static const char * const atest_usb2_testdataout3_groups[] = {
	"gpio49",
};
static const char * const audio_ref_clk_groups[] = {
	"gpio62",
};
static const char * const char_exec_pending_groups[] = {
	"gpio6",
};
static const char * const char_exec_release_groups[] = {
	"gpio7",
};
static const char * const clk_dac_gsm_groups[] = {
	"gpio35",
};
static const char * const coex_uart_rx_groups[] = {
	"gpio45",
};
static const char * const coex_uart_tx_groups[] = {
	"gpio44",
};
static const char * const cri_trng_rosc_groups[] = {
	"gpio25",
};
static const char * const cri_trng_rosc0_groups[] = {
	"gpio26",
};
static const char * const cri_trng_rosc1_groups[] = {
	"gpio27",
};
static const char * const dbg_out_clk_groups[] = {
	"gpio60",
};
static const char * const ddr_bist_complete_groups[] = {
	"gpio25",
};
static const char * const ddr_bist_fail_groups[] = {
	"gpio26",
};
static const char * const ddr_bist_start_groups[] = {
	"gpio27",
};
static const char * const ddr_bist_stop_groups[] = {
	"gpio28",
};
static const char * const ddr_pxi0_test_groups[] = {
	"gpio63", "gpio66",
};
static const char * const ebi2_lcd_a_groups[] = {
	"gpio93",
};
static const char * const ebi2_lcd_cs_groups[] = {
	"gpio94",
};
static const char * const ebi2_lcd_reset_groups[] = {
	"gpio89",
};
static const char * const ebi2_lcd_te_groups[] = {
	"gpio88",
};
static const char * const emac_mdc_groups[] = {
	"gpio98",
};
static const char * const emac_mdio_groups[] = {
	"gpio99",
};
static const char * const emac_pps_in_groups[] = {
	"gpio88",
};
static const char * const emac_ptp_aux_groups[] = {
	"gpio93", "gpio94",
};
static const char * const emac_ptp_pps_groups[] = {
	"gpio93", "gpio94",
};
static const char * const etdac_calib_data0_groups[] = {
	"gpio0",
};
static const char * const etdac_calib_data1_groups[] = {
	"gpio1",
};
static const char * const etdac_calib_data10_groups[] = {
	"gpio10",
};
static const char * const etdac_calib_data11_groups[] = {
	"gpio11",
};
static const char * const etdac_calib_data12_groups[] = {
	"gpio12",
};
static const char * const etdac_calib_data13_groups[] = {
	"gpio13",
};
static const char * const etdac_calib_data14_groups[] = {
	"gpio14",
};
static const char * const etdac_calib_data15_groups[] = {
	"gpio15",
};
static const char * const etdac_calib_data16_groups[] = {
	"gpio16",
};
static const char * const etdac_calib_data17_groups[] = {
	"gpio17",
};
static const char * const etdac_calib_data18_groups[] = {
	"gpio22",
};
static const char * const etdac_calib_data19_groups[] = {
	"gpio23",
};
static const char * const etdac_calib_data2_groups[] = {
	"gpio2",
};
static const char * const etdac_calib_data20_groups[] = {
	"gpio24",
};
static const char * const etdac_calib_data21_groups[] = {
	"gpio25",
};
static const char * const etdac_calib_data22_groups[] = {
	"gpio26",
};
static const char * const etdac_calib_data23_groups[] = {
	"gpio27",
};
static const char * const etdac_calib_data24_groups[] = {
	"gpio28",
};
static const char * const etdac_calib_data25_groups[] = {
	"gpio34",
};
static const char * const etdac_calib_data3_groups[] = {
	"gpio3",
};
static const char * const etdac_calib_data4_groups[] = {
	"gpio45",
};
static const char * const etdac_calib_data5_groups[] = {
	"gpio5",
};
static const char * const etdac_calib_data6_groups[] = {
	"gpio6",
};
static const char * const etdac_calib_data7_groups[] = {
	"gpio7",
};
static const char * const etdac_calib_data8_groups[] = {
	"gpio8",
};
static const char * const etdac_calib_data9_groups[] = {
	"gpio9",
};
static const char * const gcc_gp1_clk_groups[] = {
	"gpio34",
};
static const char * const gcc_gp2_clk_groups[] = {
	"gpio46",
};
static const char * const gcc_gp3_clk_groups[] = {
	"gpio35",
};
static const char * const gcc_plltest_bypassnl_groups[] = {
	"gpio68",
};
static const char * const gcc_plltest_resetn_groups[] = {
	"gpio69",
};
static const char * const gsm_tx_phase_groups[] = {
	"gpio46",
};
static const char * const jitter_bist_ref_groups[] = {
	"gpio71",
};
static const char * const m_voc_ext_groups[] = {
	"gpio46", "gpio48", "gpio49", "gpio60", "gpio84",
};
static const char * const mgpi_clk_req_groups[] = {
	"gpio61", "gpio71",
};
static const char * const mi2s0_data0_groups[] = {
	"gpio13",
};
static const char * const mi2s0_data1_groups[] = {
	"gpio14",
};
static const char * const mi2s0_sck_groups[] = {
	"gpio15",
};
static const char * const mi2s0_ws_groups[] = {
	"gpio12",
};
static const char * const mi2s1_data0_groups[] = {
	"gpio17",
};
static const char * const mi2s1_data1_groups[] = {
	"gpio18",
};
static const char * const mi2s1_sck_groups[] = {
	"gpio19",
};
static const char * const mi2s1_ws_groups[] = {
	"gpio16",
};
static const char * const mi2s_mclk_groups[] = {
	"gpio62",
};
static const char * const native_tsense_pwm1_groups[] = {
	"gpio31", "gpio32",
};
static const char * const nav_gpio_0_groups[] = {
	"gpio32",
};
static const char * const nav_gpio_1_groups[] = {
	"gpio31",
};
static const char * const nav_gpio_2_groups[] = {
	"gpio33",
};
static const char * const pa_indicator_1_groups[] = {
	"gpio61",
};
static const char * const pci_e_rst_groups[] = {
	"gpio57",
};
static const char * const pcie_clkreq_n_groups[] = {
	"gpio56",
};
static const char * const pll_bist_sync_groups[] = {
	"gpio47",
};
static const char * const pll_clk_aux_groups[] = {
	"gpio54",
};
static const char * const prng_rosc_test0_groups[] = {
	"gpio37",
};
static const char * const prng_rosc_test1_groups[] = {
	"gpio38",
};
static const char * const prng_rosc_test2_groups[] = {
	"gpio39",
};
static const char * const prng_rosc_test3_groups[] = {
	"gpio40",
};
static const char * const qdss_cti_trig0_groups[] = {
	"gpio16", "gpio17", "gpio54", "gpio55", "gpio60", "gpio84", "gpio101",
	"gpio102",
};
static const char * const qdss_cti_trig1_groups[] = {
	"gpio16", "gpio17", "gpio54", "gpio55", "gpio65", "gpio65", "gpio66",
	"gpio66", "gpio101", "gpio102",
};
static const char * const qdss_gpio_traceclk_groups[] = {
	"gpio8",
};
static const char * const qdss_gpio_tracectl_groups[] = {
	"gpio9",
};
static const char * const qdss_gpio_tracedata0_groups[] = {
	"gpio7",
};
static const char * const qdss_gpio_tracedata1_groups[] = {
	"gpio6",
};
static const char * const qdss_gpio_tracedata10_groups[] = {
	"gpio18",
};
static const char * const qdss_gpio_tracedata11_groups[] = {
	"gpio19",
};
static const char * const qdss_gpio_tracedata12_groups[] = {
	"gpio12",
};
static const char * const qdss_gpio_tracedata13_groups[] = {
	"gpio13",
};
static const char * const qdss_gpio_tracedata14_groups[] = {
	"gpio14",
};
static const char * const qdss_gpio_tracedata15_groups[] = {
	"gpio15",
};
static const char * const qdss_gpio_tracedata2_groups[] = {
	"gpio5",
};
static const char * const qdss_gpio_tracedata3_groups[] = {
	"gpio4",
};
static const char * const qdss_gpio_tracedata4_groups[] = {
	"gpio63",
};
static const char * const qdss_gpio_tracedata5_groups[] = {
	"gpio64",
};
static const char * const qdss_gpio_tracedata6_groups[] = {
	"gpio65",
};
static const char * const qdss_gpio_tracedata7_groups[] = {
	"gpio66",
};
static const char * const qdss_gpio_tracedata8_groups[] = {
	"gpio16",
};
static const char * const qdss_gpio_tracedata9_groups[] = {
	"gpio17",
};
static const char * const qup0_se0_l0_groups[] = {
	"gpio18",
};
static const char * const qup0_se0_l1_groups[] = {
	"gpio19",
};
static const char * const qup0_se0_l2_groups[] = {
	"gpio48",
};
static const char * const qup0_se0_l3_groups[] = {
	"gpio49",
};
static const char * const qup0_se0_l4_groups[] = {
	"gpio92",
};
static const char * const qup0_se0_l5_groups[] = {
	"gpio101",
};
static const char * const qup0_se0_l6_groups[] = {
	"gpio14",
};
static const char * const qup0_se1_l0_groups[] = {
	"gpio65",
};
static const char * const qup0_se1_l1_groups[] = {
	"gpio66",
};
static const char * const qup0_se1_l2_groups[] = {
	"gpio63",
};
static const char * const qup0_se1_l3_groups[] = {
	"gpio64",
};
static const char * const qup0_se1_l4_groups[] = {
	"gpio92",
};
static const char * const qup0_se1_l5_groups[] = {
	"gpio101",
};
static const char * const qup0_se1_l6_groups[] = {
	"gpio14",
};
static const char * const qup0_se2_l0_groups[] = {
	"gpio5",
};
static const char * const qup0_se2_l1_groups[] = {
	"gpio4",
};
static const char * const qup0_se2_l2_groups[] = {
	"gpio7",
};
static const char * const qup0_se2_l3_groups[] = {
	"gpio6",
};
static const char * const qup0_se2_l4_groups[] = {
	"gpio92",
};
static const char * const qup0_se2_l5_groups[] = {
	"gpio101",
};
static const char * const qup0_se2_l6_groups[] = {
	"gpio14",
};
static const char * const qup0_se3_l0_mira_groups[] = {
	"gpio16",
};
static const char * const qup0_se3_l0_mirb_groups[] = {
	"gpio14",
};
static const char * const qup0_se3_l1_mira_groups[] = {
	"gpio17",
};
static const char * const qup0_se3_l1_mirb_groups[] = {
	"gpio15",
};
static const char * const qup0_se3_l2_mira_groups[] = {
	"gpio8",
};
static const char * const qup0_se3_l2_mirb_groups[] = {
	"gpio12",
};
static const char * const qup0_se3_l3_mira_groups[] = {
	"gpio9",
};
static const char * const qup0_se3_l3_mirb_groups[] = {
	"gpio13",
};
static const char * const qup0_se3_l4_groups[] = {
	"gpio92",
};
static const char * const qup0_se3_l5_groups[] = {
	"gpio101",
};
static const char * const qup0_se3_l6_groups[] = {
	"gpio14",
};
static const char * const qup0_se4_l0_groups[] = {
	"gpio10",
};
static const char * const qup0_se4_l1_groups[] = {
	"gpio11",
};
static const char * const qup0_se4_l2_groups[] = {
	"gpio80",
};
static const char * const qup0_se4_l3_groups[] = {
	"gpio81",
};
static const char * const qup0_se4_l4_groups[] = {
	"gpio92",
};
static const char * const qup0_se4_l5_groups[] = {
	"gpio101",
};
static const char * const qup0_se4_l6_groups[] = {
	"gpio14",
};
static const char * const sdc40_groups[] = {
	"gpio104",
};
static const char * const sdc41_groups[] = {
	"gpio105",
};
static const char * const sdc42_groups[] = {
	"gpio106",
};
static const char * const sdc43_groups[] = {
	"gpio107",
};
static const char * const sdc4_clk_groups[] = {
	"gpio103",
};
static const char * const sdc4_cmd_groups[] = {
	"gpio102",
};
static const char * const sdc4_tb_trig_groups[] = {
	"gpio101",
};
static const char * const sgmii_phy_intr_groups[] = {
	"gpio91",
};
static const char * const spmi_coex_clk_groups[] = {
	"gpio76",
};
static const char * const spmi_coex_data_groups[] = {
	"gpio75",
};
static const char * const spmi_vgi_hwevent_groups[] = {
	"gpio78", "gpio79",
};
static const char * const tgu_ch0_trigout_groups[] = {
	"gpio24",
};
static const char * const tmess_prng_rosc0_groups[] = {
	"gpio22",
};
static const char * const tmess_prng_rosc1_groups[] = {
	"gpio23",
};
static const char * const tmess_prng_rosc2_groups[] = {
	"gpio48",
};
static const char * const tmess_prng_rosc3_groups[] = {
	"gpio49",
};
static const char * const txdac_calib_data0_groups[] = {
	"gpio20",
};
static const char * const txdac_calib_data1_groups[] = {
	"gpio21",
};
static const char * const txdac_calib_data10_groups[] = {
	"gpio50",
};
static const char * const txdac_calib_data11_groups[] = {
	"gpio51",
};
static const char * const txdac_calib_data12_groups[] = {
	"gpio58",
};
static const char * const txdac_calib_data13_groups[] = {
	"gpio59",
};
static const char * const txdac_calib_data14_groups[] = {
	"gpio72",
};
static const char * const txdac_calib_data15_groups[] = {
	"gpio73",
};
static const char * const txdac_calib_data16_groups[] = {
	"gpio80",
};
static const char * const txdac_calib_data17_groups[] = {
	"gpio81",
};
static const char * const txdac_calib_data18_groups[] = {
	"gpio83",
};
static const char * const txdac_calib_data19_groups[] = {
	"gpio84",
};
static const char * const txdac_calib_data2_groups[] = {
	"gpio29",
};
static const char * const txdac_calib_data20_groups[] = {
	"gpio85",
};
static const char * const txdac_calib_data21_groups[] = {
	"gpio86",
};
static const char * const txdac_calib_data22_groups[] = {
	"gpio88",
};
static const char * const txdac_calib_data23_groups[] = {
	"gpio89",
};
static const char * const txdac_calib_data24_groups[] = {
	"gpio93",
};
static const char * const txdac_calib_data25_groups[] = {
	"gpio94",
};
static const char * const txdac_calib_data3_groups[] = {
	"gpio30",
};
static const char * const txdac_calib_data4_groups[] = {
	"gpio36",
};
static const char * const txdac_calib_data5_groups[] = {
	"gpio37",
};
static const char * const txdac_calib_data6_groups[] = {
	"gpio38",
};
static const char * const txdac_calib_data7_groups[] = {
	"gpio39",
};
static const char * const txdac_calib_data8_groups[] = {
	"gpio40",
};
static const char * const txdac_calib_data9_groups[] = {
	"gpio41",
};
static const char * const uim0_clk_groups[] = {
	"gpio70",
};
static const char * const uim0_data_groups[] = {
	"gpio67",
};
static const char * const uim0_present_groups[] = {
	"gpio68",
};
static const char * const uim0_reset_groups[] = {
	"gpio69",
};
static const char * const uim1_clk_groups[] = {
	"gpio3",
};
static const char * const uim1_data_groups[] = {
	"gpio0",
};
static const char * const uim1_present_groups[] = {
	"gpio1",
};
static const char * const uim1_reset_groups[] = {
	"gpio2",
};
static const char * const usb2phy_ac_en_groups[] = {
	"gpio90",
};
static const char * const vsense_trigger_mirnat_groups[] = {
	"gpio52",
};

static const struct msm_function sdxbaagha_functions[] = {
	FUNCTION(gpio),
	FUNCTION(adsp_ext_vfr),
	FUNCTION(atest_bbrx_or0),
	FUNCTION(atest_bbrx_or1),
	FUNCTION(atest_char_start),
	FUNCTION(atest_char_status0),
	FUNCTION(atest_char_status1),
	FUNCTION(atest_char_status2),
	FUNCTION(atest_char_status3),
	FUNCTION(atest_gpsadc_dtestout00),
	FUNCTION(atest_gpsadc_dtestout01),
	FUNCTION(atest_gpsadc_dtestout10),
	FUNCTION(atest_gpsadc_dtestout11),
	FUNCTION(atest_usb2_atereset),
	FUNCTION(atest_usb2_testdataout0),
	FUNCTION(atest_usb2_testdataout1),
	FUNCTION(atest_usb2_testdataout2),
	FUNCTION(atest_usb2_testdataout3),
	FUNCTION(audio_ref_clk),
	FUNCTION(char_exec_pending),
	FUNCTION(char_exec_release),
	FUNCTION(clk_dac_gsm),
	FUNCTION(coex_uart_rx),
	FUNCTION(coex_uart_tx),
	FUNCTION(cri_trng_rosc),
	FUNCTION(cri_trng_rosc0),
	FUNCTION(cri_trng_rosc1),
	FUNCTION(dbg_out_clk),
	FUNCTION(ddr_bist_complete),
	FUNCTION(ddr_bist_fail),
	FUNCTION(ddr_bist_start),
	FUNCTION(ddr_bist_stop),
	FUNCTION(ddr_pxi0_test),
	FUNCTION(ebi2_lcd_a),
	FUNCTION(ebi2_lcd_cs),
	FUNCTION(ebi2_lcd_reset),
	FUNCTION(ebi2_lcd_te),
	FUNCTION(emac_mdc),
	FUNCTION(emac_mdio),
	FUNCTION(emac_pps_in),
	FUNCTION(emac_ptp_aux),
	FUNCTION(emac_ptp_pps),
	FUNCTION(etdac_calib_data0),
	FUNCTION(etdac_calib_data1),
	FUNCTION(etdac_calib_data10),
	FUNCTION(etdac_calib_data11),
	FUNCTION(etdac_calib_data12),
	FUNCTION(etdac_calib_data13),
	FUNCTION(etdac_calib_data14),
	FUNCTION(etdac_calib_data15),
	FUNCTION(etdac_calib_data16),
	FUNCTION(etdac_calib_data17),
	FUNCTION(etdac_calib_data18),
	FUNCTION(etdac_calib_data19),
	FUNCTION(etdac_calib_data2),
	FUNCTION(etdac_calib_data20),
	FUNCTION(etdac_calib_data21),
	FUNCTION(etdac_calib_data22),
	FUNCTION(etdac_calib_data23),
	FUNCTION(etdac_calib_data24),
	FUNCTION(etdac_calib_data25),
	FUNCTION(etdac_calib_data3),
	FUNCTION(etdac_calib_data4),
	FUNCTION(etdac_calib_data5),
	FUNCTION(etdac_calib_data6),
	FUNCTION(etdac_calib_data7),
	FUNCTION(etdac_calib_data8),
	FUNCTION(etdac_calib_data9),
	FUNCTION(gcc_gp1_clk),
	FUNCTION(gcc_gp2_clk),
	FUNCTION(gcc_gp3_clk),
	FUNCTION(gcc_plltest_bypassnl),
	FUNCTION(gcc_plltest_resetn),
	FUNCTION(gsm_tx_phase),
	FUNCTION(jitter_bist_ref),
	FUNCTION(m_voc_ext),
	FUNCTION(mgpi_clk_req),
	FUNCTION(mi2s0_data0),
	FUNCTION(mi2s0_data1),
	FUNCTION(mi2s0_sck),
	FUNCTION(mi2s0_ws),
	FUNCTION(mi2s1_data0),
	FUNCTION(mi2s1_data1),
	FUNCTION(mi2s1_sck),
	FUNCTION(mi2s1_ws),
	FUNCTION(mi2s_mclk),
	FUNCTION(native_tsense_pwm1),
	FUNCTION(nav_gpio_0),
	FUNCTION(nav_gpio_1),
	FUNCTION(nav_gpio_2),
	FUNCTION(pa_indicator_1),
	FUNCTION(pci_e_rst),
	FUNCTION(pcie_clkreq_n),
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
	FUNCTION(qup0_se0_l0),
	FUNCTION(qup0_se0_l1),
	FUNCTION(qup0_se0_l2),
	FUNCTION(qup0_se0_l3),
	FUNCTION(qup0_se0_l4),
	FUNCTION(qup0_se0_l5),
	FUNCTION(qup0_se0_l6),
	FUNCTION(qup0_se1_l0),
	FUNCTION(qup0_se1_l1),
	FUNCTION(qup0_se1_l2),
	FUNCTION(qup0_se1_l3),
	FUNCTION(qup0_se1_l4),
	FUNCTION(qup0_se1_l5),
	FUNCTION(qup0_se1_l6),
	FUNCTION(qup0_se2_l0),
	FUNCTION(qup0_se2_l1),
	FUNCTION(qup0_se2_l2),
	FUNCTION(qup0_se2_l3),
	FUNCTION(qup0_se2_l4),
	FUNCTION(qup0_se2_l5),
	FUNCTION(qup0_se2_l6),
	FUNCTION(qup0_se3_l0_mira),
	FUNCTION(qup0_se3_l0_mirb),
	FUNCTION(qup0_se3_l1_mira),
	FUNCTION(qup0_se3_l1_mirb),
	FUNCTION(qup0_se3_l2_mira),
	FUNCTION(qup0_se3_l2_mirb),
	FUNCTION(qup0_se3_l3_mira),
	FUNCTION(qup0_se3_l3_mirb),
	FUNCTION(qup0_se3_l4),
	FUNCTION(qup0_se3_l5),
	FUNCTION(qup0_se3_l6),
	FUNCTION(qup0_se4_l0),
	FUNCTION(qup0_se4_l1),
	FUNCTION(qup0_se4_l2),
	FUNCTION(qup0_se4_l3),
	FUNCTION(qup0_se4_l4),
	FUNCTION(qup0_se4_l5),
	FUNCTION(qup0_se4_l6),
	FUNCTION(sdc40),
	FUNCTION(sdc41),
	FUNCTION(sdc42),
	FUNCTION(sdc43),
	FUNCTION(sdc4_clk),
	FUNCTION(sdc4_cmd),
	FUNCTION(sdc4_tb_trig),
	FUNCTION(sgmii_phy_intr),
	FUNCTION(spmi_coex_clk),
	FUNCTION(spmi_coex_data),
	FUNCTION(spmi_vgi_hwevent),
	FUNCTION(tgu_ch0_trigout),
	FUNCTION(tmess_prng_rosc0),
	FUNCTION(tmess_prng_rosc1),
	FUNCTION(tmess_prng_rosc2),
	FUNCTION(tmess_prng_rosc3),
	FUNCTION(txdac_calib_data0),
	FUNCTION(txdac_calib_data1),
	FUNCTION(txdac_calib_data10),
	FUNCTION(txdac_calib_data11),
	FUNCTION(txdac_calib_data12),
	FUNCTION(txdac_calib_data13),
	FUNCTION(txdac_calib_data14),
	FUNCTION(txdac_calib_data15),
	FUNCTION(txdac_calib_data16),
	FUNCTION(txdac_calib_data17),
	FUNCTION(txdac_calib_data18),
	FUNCTION(txdac_calib_data19),
	FUNCTION(txdac_calib_data2),
	FUNCTION(txdac_calib_data20),
	FUNCTION(txdac_calib_data21),
	FUNCTION(txdac_calib_data22),
	FUNCTION(txdac_calib_data23),
	FUNCTION(txdac_calib_data24),
	FUNCTION(txdac_calib_data25),
	FUNCTION(txdac_calib_data3),
	FUNCTION(txdac_calib_data4),
	FUNCTION(txdac_calib_data5),
	FUNCTION(txdac_calib_data6),
	FUNCTION(txdac_calib_data7),
	FUNCTION(txdac_calib_data8),
	FUNCTION(txdac_calib_data9),
	FUNCTION(uim0_clk),
	FUNCTION(uim0_data),
	FUNCTION(uim0_present),
	FUNCTION(uim0_reset),
	FUNCTION(uim1_clk),
	FUNCTION(uim1_data),
	FUNCTION(uim1_present),
	FUNCTION(uim1_reset),
	FUNCTION(usb2phy_ac_en),
	FUNCTION(vsense_trigger_mirnat),
};

/* Every pin is maintained as a single group, and missing or non-existing pin
 * would be maintained as dummy group to synchronize pin group index with
 * pin descriptor registered with pinctrl core.
 * Clients would not be able to request these dummy pin groups.
 */
static const struct msm_pingroup sdxbaagha_groups[] = {
	[0] = PINGROUP(0, uim1_data, etdac_calib_data0, NA, NA, NA, NA, NA, NA,
		       NA, 0, -1),
	[1] = PINGROUP(1, uim1_present, NA, etdac_calib_data1, NA, NA, NA, NA,
		       NA, NA, 0x6E000, 0),
	[2] = PINGROUP(2, uim1_reset, NA, etdac_calib_data2, NA, NA, NA, NA,
		       NA, NA, 0x6E000, 1),
	[3] = PINGROUP(3, uim1_clk, NA, etdac_calib_data3, NA, NA, NA, NA, NA,
		       NA, 0, -1),
	[4] = PINGROUP(4, qup0_se2_l1, NA, qdss_gpio_tracedata3, NA, NA, NA,
		       NA, NA, NA, 0x6E000, 2),
	[5] = PINGROUP(5, qup0_se2_l0, NA, qdss_gpio_tracedata2,
		       etdac_calib_data5, NA, NA, NA, NA, NA, 0x6E000, 3),
	[6] = PINGROUP(6, qup0_se2_l3, char_exec_pending, NA,
		       qdss_gpio_tracedata1, etdac_calib_data6, NA, NA, NA, NA, 0x6E000, 4),
	[7] = PINGROUP(7, qup0_se2_l2, char_exec_release, NA,
		       qdss_gpio_tracedata0, etdac_calib_data7, NA, NA, NA, NA, 0x6E000, 5),
	[8] = PINGROUP(8, qup0_se3_l2_mira, NA, qdss_gpio_traceclk,
		       etdac_calib_data8, NA, NA, NA, NA, NA, 0x6E000, 6),
	[9] = PINGROUP(9, qup0_se3_l3_mira, NA, qdss_gpio_tracectl,
		       etdac_calib_data9, atest_usb2_atereset, NA, NA, NA, NA, 0x6E000, 7),
	[10] = PINGROUP(10, qup0_se4_l0, NA, etdac_calib_data10, NA, NA, NA,
			NA, NA, NA, 0x6E000, 8),
	[11] = PINGROUP(11, qup0_se4_l1, NA, etdac_calib_data11, NA, NA, NA,
			NA, NA, NA, 0x6E000, 9),
	[12] = PINGROUP(12, mi2s0_ws, qup0_se3_l2_mirb, NA, qdss_gpio_tracedata12,
			etdac_calib_data12, NA, NA, NA, NA, 0x6E000, 10),
	[13] = PINGROUP(13, mi2s0_data0, qup0_se3_l3_mirb, NA,
			qdss_gpio_tracedata13, etdac_calib_data13, NA, NA, NA,
			NA, 0x6E000, 11),
	[14] = PINGROUP(14, mi2s0_data1, qup0_se3_l0_mirb, qup0_se0_l6, qup0_se1_l6,
			qup0_se2_l6, qup0_se3_l6, qup0_se4_l6, NA,
			qdss_gpio_tracedata14, 0x6E000, 12),
	[15] = PINGROUP(15, mi2s0_sck, qup0_se3_l1_mirb, NA, qdss_gpio_tracedata15,
			etdac_calib_data15, NA, NA, NA, NA, 0x6E000, 13),
	[16] = PINGROUP(16, mi2s1_ws, qup0_se3_l0_mira, qdss_cti_trig1,
			qdss_cti_trig0, NA, qdss_gpio_tracedata8,
			etdac_calib_data16, NA, NA, 0x6E000, 14),
	[17] = PINGROUP(17, mi2s1_data0, qup0_se3_l1_mira, qdss_cti_trig1,
			qdss_cti_trig0, NA, qdss_gpio_tracedata9,
			etdac_calib_data17, NA, NA, 0x6E000, 15),
	[18] = PINGROUP(18, mi2s1_data1, qup0_se0_l0, NA, NA,
			qdss_gpio_tracedata10, atest_bbrx_or0,
			atest_usb2_testdataout0, NA, NA, 0x6E000, 16),
	[19] = PINGROUP(19, mi2s1_sck, qup0_se0_l1, NA, NA,
			qdss_gpio_tracedata11, atest_bbrx_or1,
			atest_usb2_testdataout1, NA, NA, 0x6E000, 17),
	[20] = PINGROUP(20, NA, NA, NA, txdac_calib_data0, NA, NA, NA, NA, NA, 0, -1),
	[21] = PINGROUP(21, NA, NA, txdac_calib_data1, NA, NA, NA, NA, NA, NA, 0x6E000, 18),
	[22] = PINGROUP(22, NA, tmess_prng_rosc0, NA, etdac_calib_data18, NA,
			NA, NA, NA, NA, 0x6E000, 19),
	[23] = PINGROUP(23, NA, tmess_prng_rosc1, NA, etdac_calib_data19, NA,
			NA, NA, NA, NA, 0x6E000, 20),
	[24] = PINGROUP(24, NA, adsp_ext_vfr, tgu_ch0_trigout, NA,
			etdac_calib_data20, NA, NA, NA, NA, 0x6E000, 21),
	[25] = PINGROUP(25, NA, adsp_ext_vfr, cri_trng_rosc, ddr_bist_complete,
			NA, etdac_calib_data21, NA, NA, NA, 0x6E000, 22),
	[26] = PINGROUP(26, NA, cri_trng_rosc0, ddr_bist_fail, NA,
			etdac_calib_data22, NA, NA, NA, NA, 0x6E000, 23),
	[27] = PINGROUP(27, NA, cri_trng_rosc1, ddr_bist_start, NA,
			etdac_calib_data23, NA, NA, NA, NA, 0x6E000, 24),
	[28] = PINGROUP(28, NA, ddr_bist_stop, NA, etdac_calib_data24, NA, NA,
			NA, NA, NA, 0x6E000, 25),
	[29] = PINGROUP(29, NA, NA, NA, txdac_calib_data2, NA, NA, NA, NA, NA, 0, -1),
	[30] = PINGROUP(30, NA, NA, NA, txdac_calib_data3, NA, NA, NA, NA, NA, 0, -1),
	[31] = PINGROUP(31, nav_gpio_1, native_tsense_pwm1, NA, NA, NA, NA, NA,
			NA, NA, 0x6E000, 26),
	[32] = PINGROUP(32, nav_gpio_0, native_tsense_pwm1, NA, NA, NA, NA, NA,
			NA, NA, 0x6E000, 27),
	[33] = PINGROUP(33, nav_gpio_2, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E000, 28),
	[34] = PINGROUP(34, NA, gcc_gp1_clk, NA, etdac_calib_data25, NA, NA,
			NA, NA, NA, 0, -1),
	[35] = PINGROUP(35, NA, clk_dac_gsm, gcc_gp3_clk, NA, NA, NA, NA, NA,
			NA, 0x6E000, 29),
	[36] = PINGROUP(36, NA, NA, txdac_calib_data4, NA, NA, NA, NA, NA, NA, 0, -1),
	[37] = PINGROUP(37, NA, prng_rosc_test0, NA, txdac_calib_data5, NA, NA,
			NA, NA, NA, 0, -1),
	[38] = PINGROUP(38, NA, prng_rosc_test1, NA, txdac_calib_data6, NA, NA,
			NA, NA, NA, 0, -1),
	[39] = PINGROUP(39, NA, prng_rosc_test2, NA, txdac_calib_data7, NA, NA,
			NA, NA, NA, 0, -1),
	[40] = PINGROUP(40, NA, prng_rosc_test3, NA, txdac_calib_data8, NA, NA,
			NA, NA, NA, 0, -1),
	[41] = PINGROUP(41, NA, NA, txdac_calib_data9, NA, NA, NA, NA, NA, NA, 0, -1),
	[42] = PINGROUP(42, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[43] = PINGROUP(43, NA, atest_char_start, NA, NA, NA, NA, NA, NA, NA, 0x6E000, 30),
	[44] = PINGROUP(44, coex_uart_tx, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E000, 31),
	[45] = PINGROUP(45, coex_uart_rx, etdac_calib_data4, NA, NA, NA, NA,
			NA, NA, NA, 0x6E004, 0),
	[46] = PINGROUP(46, NA, gsm_tx_phase, m_voc_ext, gcc_gp2_clk, NA, NA,
			NA, NA, NA, 0x6E004, 1),
	[47] = PINGROUP(47, NA, pll_bist_sync, NA, NA, NA, NA, NA, NA, NA, 0x6E004, 2),
	[48] = PINGROUP(48, qup0_se0_l2, m_voc_ext, tmess_prng_rosc2, NA,
			atest_usb2_testdataout2, NA, NA, NA, NA, 0x6E004, 3),
	[49] = PINGROUP(49, qup0_se0_l3, m_voc_ext, tmess_prng_rosc3, NA,
			atest_usb2_testdataout3, NA, NA, NA, NA, 0x6E004, 4),
	[50] = PINGROUP(50, NA, NA, NA, txdac_calib_data10, NA, NA, NA, NA, NA, 0x6E004, 5),
	[51] = PINGROUP(51, NA, NA, NA, txdac_calib_data11, NA, NA, NA, NA, NA, 0, -1),
	[52] = PINGROUP(52, NA, atest_gpsadc_dtestout00, vsense_trigger_mirnat,
			NA, NA, NA, NA, NA, NA, 0x6E004, 6),
	[53] = PINGROUP(53, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E004, 7),
	[54] = PINGROUP(54, qdss_cti_trig1, qdss_cti_trig0, pll_clk_aux,
			atest_gpsadc_dtestout01, NA, NA, NA, NA, NA, 0x6E004, 8),
	[55] = PINGROUP(55, qdss_cti_trig1, qdss_cti_trig0, NA,
			atest_gpsadc_dtestout10, NA, NA, NA, NA, NA, 0x6E004, 9),
	[56] = PINGROUP(56, pcie_clkreq_n, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E004, 10),
	[57] = PINGROUP(57, pci_e_rst, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E004, 11),
	[58] = PINGROUP(58, NA, NA, NA, txdac_calib_data12, NA, NA, NA, NA, NA, 0, -1),
	[59] = PINGROUP(59, NA, NA, NA, txdac_calib_data13, NA, NA, NA, NA, NA, 0, -1),
	[60] = PINGROUP(60, qdss_cti_trig0, m_voc_ext, dbg_out_clk,
			atest_gpsadc_dtestout11, NA, NA, NA, NA, NA, 0x6E004, 12),
	[61] = PINGROUP(61, NA, mgpi_clk_req, pa_indicator_1, NA, NA, NA, NA,
			NA, NA, 0x6E004, 13),
	[62] = PINGROUP(62, mi2s_mclk, audio_ref_clk, NA, NA, NA, NA, NA, NA,
			NA, 0, -1),
	[63] = PINGROUP(63, qup0_se1_l2, NA, NA, qdss_gpio_tracedata4,
			ddr_pxi0_test, NA, NA, NA, NA, 0x6E004, 14),
	[64] = PINGROUP(64, qup0_se1_l3, NA, qdss_gpio_tracedata5, NA, NA, NA,
			NA, NA, NA, 0x6E004, 15),
	[65] = PINGROUP(65, qup0_se1_l0, qdss_cti_trig1, qdss_cti_trig1, NA,
			qdss_gpio_tracedata6, NA, NA, NA, NA, 0x6E004, 16),
	[66] = PINGROUP(66, qup0_se1_l1, qdss_cti_trig1, qdss_cti_trig1, NA,
			NA, qdss_gpio_tracedata7, ddr_pxi0_test, NA, NA, 0x6E004, 17),
	[67] = PINGROUP(67, uim0_data, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[68] = PINGROUP(68, uim0_present, gcc_plltest_bypassnl, NA, NA, NA, NA,
			NA, NA, NA, 0x6E004, 18),
	[69] = PINGROUP(69, uim0_reset, gcc_plltest_resetn, NA, NA, NA, NA, NA,
			NA, NA, 0x6E004, 19),
	[70] = PINGROUP(70, uim0_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[71] = PINGROUP(71, NA, mgpi_clk_req, jitter_bist_ref, NA, NA, NA, NA,
			NA, NA, 0x6E004, 20),
	[72] = PINGROUP(72, txdac_calib_data14, atest_char_status0, NA, NA, NA,
			NA, NA, NA, NA, 0, -1),
	[73] = PINGROUP(73, NA, txdac_calib_data15, atest_char_status1, NA, NA,
			NA, NA, NA, NA, 0x6E004, 21),
	[74] = PINGROUP(74, NA, atest_char_status2, NA, NA, NA, NA, NA, NA, NA, 0x6E004, 22),
	[75] = PINGROUP(75, spmi_coex_data, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[76] = PINGROUP(76, spmi_coex_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E004, 23),
	[77] = PINGROUP(77, NA, atest_char_status3, NA, NA, NA, NA, NA, NA, NA, 0x6E004, 24),
	[78] = PINGROUP(78, spmi_vgi_hwevent, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E004, 25),
	[79] = PINGROUP(79, spmi_vgi_hwevent, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E004, 26),
	[80] = PINGROUP(80, qup0_se4_l2, NA, txdac_calib_data16, NA, NA, NA,
			NA, NA, NA, 0x6E004, 27),
	[81] = PINGROUP(81, qup0_se4_l3, NA, txdac_calib_data17, NA, NA, NA,
			NA, NA, NA, 0x6E004, 28),
	[82] = PINGROUP(82, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E004, 29),
	[83] = PINGROUP(83, NA, txdac_calib_data18, NA, NA, NA, NA, NA, NA, NA, 0x6E004, 30),
	[84] = PINGROUP(84, qdss_cti_trig0, m_voc_ext, NA, txdac_calib_data19,
			NA, NA, NA, NA, NA, 0x6E004, 31),
	[85] = PINGROUP(85, NA, txdac_calib_data20, NA, NA, NA, NA, NA, NA, NA, 0x6E008, 0),
	[86] = PINGROUP(86, NA, txdac_calib_data21, NA, NA, NA, NA, NA, NA, NA, 0x6E008, 1),
	[87] = PINGROUP(87, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E008, 2),
	[88] = PINGROUP(88, ebi2_lcd_te, emac_pps_in, txdac_calib_data22, NA,
			NA, NA, NA, NA, NA, 0x6E008, 3),
	[89] = PINGROUP(89, ebi2_lcd_reset, txdac_calib_data23, NA, NA, NA, NA,
			NA, NA, NA, 0, -1),
	[90] = PINGROUP(90, usb2phy_ac_en, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E008, 4),
	[91] = PINGROUP(91, sgmii_phy_intr, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E008, 5),
	[92] = PINGROUP(92, qup0_se0_l4, qup0_se1_l4, qup0_se2_l4, qup0_se3_l4,
			qup0_se4_l4, NA, NA, NA, NA, 0x6E008, 6),
	[93] = PINGROUP(93, ebi2_lcd_a, emac_ptp_pps, emac_ptp_aux,
			txdac_calib_data24, NA, NA, NA, NA, NA, 0x6E008, 7),
	[94] = PINGROUP(94, ebi2_lcd_cs, emac_ptp_pps, emac_ptp_aux,
			txdac_calib_data25, NA, NA, NA, NA, NA, 0x6E008, 8),
	[95] = PINGROUP(95, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[96] = PINGROUP(96, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E008, 9),
	[97] = PINGROUP(97, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E008, 10),
	[98] = PINGROUP(98, emac_mdc, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E008, 11),
	[99] = PINGROUP(99, emac_mdio, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E008, 12),
	[100] = PINGROUP(100, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E008, 13),
	[101] = PINGROUP(101, sdc4_tb_trig, qdss_cti_trig1, qdss_cti_trig0,
			 qup0_se0_l5, qup0_se1_l5, qup0_se2_l5, qup0_se3_l5,
			 qup0_se4_l5, NA, 0x6E008, 14),
	[102] = PINGROUP(102, sdc4_cmd, qdss_cti_trig1, qdss_cti_trig0, NA, NA,
			 NA, NA, NA, NA, 0x6E008, 15),
	[103] = PINGROUP(103, sdc4_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E008, 16),
	[104] = PINGROUP(104, sdc40, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E008, 17),
	[105] = PINGROUP(105, sdc41, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E008, 18),
	[106] = PINGROUP(106, sdc42, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E008, 19),
	[107] = PINGROUP(107, sdc43, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E008, 20),
	[108] = PINGROUP(108, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[109] = PINGROUP(109, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x6E008, 21),
};
static struct pinctrl_qup sdxbaagha_qup_regs[] = {
};

static const struct msm_gpio_wakeirq_map sdxbaagha_pdc_map[] = {
	{ 2, 83 }, { 4, 59 }, { 5, 107 }, { 6, 112 }, { 7, 119 },
	{ 10, 52 }, { 11, 73 }, { 12, 74 }, { 13, 75 }, { 14, 76 },
	{ 16, 81 }, { 17, 82 }, { 18, 80 }, { 19, 100 }, { 21, 88 },
	{ 23, 84 }, { 24, 85 }, { 25, 97 }, { 26, 86 }, { 27, 102 },
	{ 35, 103 }, { 43, 90 }, { 46, 104 }, { 47, 89 }, { 48, 105 },
	{ 50, 101 }, { 52, 109 }, { 54, 110 }, { 55, 111 }, { 60, 113 },
	{ 63, 115 }, { 64, 116 }, { 65, 57 }, { 66, 117 }, { 69, 118 },
	{ 73, 120 }, { 74, 121 }, { 77, 122 }, { 80, 124 }, { 81, 125 },
	{ 83, 127 }, { 84, 93 }, { 85, 128 }, { 86, 129 }, { 87, 70 },
	{ 96, 68 }, { 98, 50 }, { 99, 67 }, { 100, 51 }, { 101, 53 },
	{ 103, 98 }, { 104, 91 }, { 105, 69 }, { 106, 55 }, { 107, 56 },
};

static const struct msm_pinctrl_soc_data sdxbaagha_pinctrl = {
	.pins = sdxbaagha_pins,
	.npins = ARRAY_SIZE(sdxbaagha_pins),
	.functions = sdxbaagha_functions,
	.nfunctions = ARRAY_SIZE(sdxbaagha_functions),
	.groups = sdxbaagha_groups,
	.ngroups = ARRAY_SIZE(sdxbaagha_groups),
	.ngpios = 110,
	.qup_regs = sdxbaagha_qup_regs,
	.nqup_regs = ARRAY_SIZE(sdxbaagha_qup_regs),
	.wakeirq_map = sdxbaagha_pdc_map,
	.nwakeirq_map = ARRAY_SIZE(sdxbaagha_pdc_map),
};

static int sdxbaagha_pinctrl_probe(struct platform_device *pdev)
{
	return msm_pinctrl_probe(pdev, &sdxbaagha_pinctrl);
}

static const struct of_device_id sdxbaagha_pinctrl_of_match[] = {
	{ .compatible = "qcom,sdxbaagha-pinctrl", },
	{ },
};

static struct platform_driver sdxbaagha_pinctrl_driver = {
	.driver = {
		.name = "sdxbaagha-pinctrl",
		.of_match_table = sdxbaagha_pinctrl_of_match,
	},
	.probe = sdxbaagha_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init sdxbaagha_pinctrl_init(void)
{
	return platform_driver_register(&sdxbaagha_pinctrl_driver);
}
arch_initcall(sdxbaagha_pinctrl_init);

static void __exit sdxbaagha_pinctrl_exit(void)
{
	platform_driver_unregister(&sdxbaagha_pinctrl_driver);
}
module_exit(sdxbaagha_pinctrl_exit);

MODULE_DESCRIPTION("QTI sdxbaagha pinctrl driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, sdxbaagha_pinctrl_of_match);
