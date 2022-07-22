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


static const struct pinctrl_pin_desc sdxpinn_pins[] = {
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
	PINCTRL_PIN(133, "SDC1_RCLK"),
	PINCTRL_PIN(134, "SDC1_CLK"),
	PINCTRL_PIN(135, "SDC1_CMD"),
	PINCTRL_PIN(136, "SDC1_DATA"),
	PINCTRL_PIN(137, "SDC2_CLK"),
	PINCTRL_PIN(138, "SDC2_CMD"),
	PINCTRL_PIN(139, "SDC2_DATA"),
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

static const unsigned int sdc1_rclk_pins[] = { 133 };
static const unsigned int sdc1_clk_pins[] = { 134 };
static const unsigned int sdc1_cmd_pins[] = { 135 };
static const unsigned int sdc1_data_pins[] = { 136 };
static const unsigned int sdc2_clk_pins[] = { 137 };
static const unsigned int sdc2_cmd_pins[] = { 138 };
static const unsigned int sdc2_data_pins[] = { 139 };

enum sdxpinn_functions {
	msm_mux_gpio,
	msm_mux_ETH0_MDC,
	msm_mux_ETH0_MDIO,
	msm_mux_ETH1_MDC,
	msm_mux_ETH1_MDIO,
	msm_mux_QLINK0_WMSS_RESET,
	msm_mux_QLINK1_WMSS_RESET,
	msm_mux_RGMII_RXC,
	msm_mux_RGMII_RXD0,
	msm_mux_RGMII_RXD1,
	msm_mux_RGMII_RXD2,
	msm_mux_RGMII_RXD3,
	msm_mux_RGMII_RX_CTL,
	msm_mux_RGMII_TXC,
	msm_mux_RGMII_TXD0,
	msm_mux_RGMII_TXD1,
	msm_mux_RGMII_TXD2,
	msm_mux_RGMII_TXD3,
	msm_mux_RGMII_TX_CTL,
	msm_mux_adsp_ext_vfr,
	msm_mux_atest_char_start,
	msm_mux_atest_char_status0,
	msm_mux_atest_char_status1,
	msm_mux_atest_char_status2,
	msm_mux_atest_char_status3,
	msm_mux_audio_ref_clk,
	msm_mux_bimc_dte_test0,
	msm_mux_bimc_dte_test1,
	msm_mux_char_exec_pending,
	msm_mux_char_exec_release,
	msm_mux_coex_uart2_rx,
	msm_mux_coex_uart2_tx,
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
	msm_mux_ebi0_wrcdc_dq2,
	msm_mux_ebi0_wrcdc_dq3,
	msm_mux_ebi2_a_d,
	msm_mux_ebi2_lcd_cs,
	msm_mux_ebi2_lcd_reset,
	msm_mux_ebi2_lcd_te,
	msm_mux_emac0_mcg_pst0,
	msm_mux_emac0_mcg_pst1,
	msm_mux_emac0_mcg_pst2,
	msm_mux_emac0_mcg_pst3,
	msm_mux_emac0_ptp_aux,
	msm_mux_emac0_ptp_pps,
	msm_mux_emac1_mcg_pst0,
	msm_mux_emac1_mcg_pst1,
	msm_mux_emac1_mcg_pst2,
	msm_mux_emac1_mcg_pst3,
	msm_mux_emac1_ptp_aux0,
	msm_mux_emac1_ptp_aux1,
	msm_mux_emac1_ptp_aux2,
	msm_mux_emac1_ptp_aux3,
	msm_mux_emac1_ptp_pps0,
	msm_mux_emac1_ptp_pps1,
	msm_mux_emac1_ptp_pps2,
	msm_mux_emac1_ptp_pps3,
	msm_mux_emac_cdc_dtest0,
	msm_mux_emac_cdc_dtest1,
	msm_mux_emac_pps_in,
	msm_mux_ext_dbg_uart,
	msm_mux_gcc_125_clk,
	msm_mux_gcc_gp1_clk,
	msm_mux_gcc_gp2_clk,
	msm_mux_gcc_gp3_clk,
	msm_mux_gcc_plltest_bypassnl,
	msm_mux_gcc_plltest_resetn,
	msm_mux_i2s_mclk,
	msm_mux_jitter_bist_ref,
	msm_mux_ldo_en,
	msm_mux_ldo_update,
	msm_mux_m_voc_ext,
	msm_mux_mgpi_clk_req,
	msm_mux_native0,
	msm_mux_native1,
	msm_mux_native2,
	msm_mux_native3,
	msm_mux_native_char_start,
	msm_mux_native_tsens_osc,
	msm_mux_native_tsense_pwm1,
	msm_mux_nav_dr_sync,
	msm_mux_nav_gpio_0,
	msm_mux_nav_gpio_1,
	msm_mux_nav_gpio_2,
	msm_mux_nav_gpio_3,
	msm_mux_pa_indicator_1,
	msm_mux_pci_e_rst,
	msm_mux_pcie0_clkreq_n,
	msm_mux_pcie1_clkreq_n,
	msm_mux_pcie2_clkreq_n,
	msm_mux_pll_bist_sync,
	msm_mux_pll_clk_aux,
	msm_mux_pll_ref_clk,
	msm_mux_pri_mi2s_data0,
	msm_mux_pri_mi2s_data1,
	msm_mux_pri_mi2s_sck,
	msm_mux_pri_mi2s_ws,
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
	msm_mux_qlink0_b_en,
	msm_mux_qlink0_b_req,
	msm_mux_qlink0_l_en,
	msm_mux_qlink0_l_req,
	msm_mux_qlink1_l_en,
	msm_mux_qlink1_l_req,
	msm_mux_qup_se0_l0,
	msm_mux_qup_se0_l1,
	msm_mux_qup_se0_l2,
	msm_mux_qup_se0_l3,
	msm_mux_qup_se1_l2,
	msm_mux_qup_se1_l3,
	msm_mux_qup_se2_l0,
	msm_mux_qup_se2_l1,
	msm_mux_qup_se2_l2,
	msm_mux_qup_se2_l3,
	msm_mux_qup_se3_l0,
	msm_mux_qup_se3_l1,
	msm_mux_qup_se3_l2,
	msm_mux_qup_se3_l3,
	msm_mux_qup_se4_l2,
	msm_mux_qup_se4_l3,
	msm_mux_qup_se5_l0,
	msm_mux_qup_se5_l1,
	msm_mux_qup_se6_l0,
	msm_mux_qup_se6_l1,
	msm_mux_qup_se6_l2,
	msm_mux_qup_se6_l3,
	msm_mux_qup_se7_l0,
	msm_mux_qup_se7_l1,
	msm_mux_qup_se7_l2,
	msm_mux_qup_se7_l3,
	msm_mux_qup_se8_l2,
	msm_mux_qup_se8_l3,
	msm_mux_sdc1_tb_trig,
	msm_mux_sdc2_tb_trig,
	msm_mux_sec_mi2s_data0,
	msm_mux_sec_mi2s_data1,
	msm_mux_sec_mi2s_sck,
	msm_mux_sec_mi2s_ws,
	msm_mux_sgmii_phy_intr0,
	msm_mux_sgmii_phy_intr1,
	msm_mux_spmi_coex_clk,
	msm_mux_spmi_coex_data,
	msm_mux_spmi_vgi_hwevent,
	msm_mux_tgu_ch0_trigout,
	msm_mux_tri_mi2s_data0,
	msm_mux_tri_mi2s_data1,
	msm_mux_tri_mi2s_sck,
	msm_mux_tri_mi2s_ws,
	msm_mux_uim1_clk,
	msm_mux_uim1_data,
	msm_mux_uim1_present,
	msm_mux_uim1_reset,
	msm_mux_uim2_clk,
	msm_mux_uim2_data,
	msm_mux_uim2_present,
	msm_mux_uim2_reset,
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
	"gpio105", "gpio106", "gpio107", "gpio108", "gpio109", "gpio110",
	"gpio111", "gpio112", "gpio113", "gpio114", "gpio115", "gpio116",
	"gpio117", "gpio118", "gpio119", "gpio120", "gpio121", "gpio122",
	"gpio123", "gpio124", "gpio125", "gpio126", "gpio127", "gpio128",
	"gpio129", "gpio130", "gpio131", "gpio132",
};
static const char * const ETH0_MDC_groups[] = {
	"gpio94",
};
static const char * const ETH0_MDIO_groups[] = {
	"gpio95",
};
static const char * const ETH1_MDC_groups[] = {
	"gpio106",
};
static const char * const ETH1_MDIO_groups[] = {
	"gpio107",
};
static const char * const QLINK0_WMSS_RESET_groups[] = {
	"gpio39",
};
static const char * const QLINK1_WMSS_RESET_groups[] = {
	"gpio28",
};
static const char * const RGMII_RXC_groups[] = {
	"gpio88",
};
static const char * const RGMII_RXD0_groups[] = {
	"gpio89",
};
static const char * const RGMII_RXD1_groups[] = {
	"gpio90",
};
static const char * const RGMII_RXD2_groups[] = {
	"gpio91",
};
static const char * const RGMII_RXD3_groups[] = {
	"gpio92",
};
static const char * const RGMII_RX_CTL_groups[] = {
	"gpio93",
};
static const char * const RGMII_TXC_groups[] = {
	"gpio82",
};
static const char * const RGMII_TXD0_groups[] = {
	"gpio83",
};
static const char * const RGMII_TXD1_groups[] = {
	"gpio84",
};
static const char * const RGMII_TXD2_groups[] = {
	"gpio85",
};
static const char * const RGMII_TXD3_groups[] = {
	"gpio86",
};
static const char * const RGMII_TX_CTL_groups[] = {
	"gpio87",
};
static const char * const adsp_ext_vfr_groups[] = {
	"gpio59", "gpio68",
};
static const char * const atest_char_start_groups[] = {
	"gpio63",
};
static const char * const atest_char_status0_groups[] = {
	"gpio67",
};
static const char * const atest_char_status1_groups[] = {
	"gpio66",
};
static const char * const atest_char_status2_groups[] = {
	"gpio25",
};
static const char * const atest_char_status3_groups[] = {
	"gpio24",
};
static const char * const audio_ref_clk_groups[] = {
	"gpio126",
};
static const char * const bimc_dte_test0_groups[] = {
	"gpio14", "gpio59",
};
static const char * const bimc_dte_test1_groups[] = {
	"gpio15", "gpio61",
};
static const char * const char_exec_pending_groups[] = {
	"gpio6",
};
static const char * const char_exec_release_groups[] = {
	"gpio7",
};
static const char * const coex_uart2_rx_groups[] = {
	"gpio49", "gpio91",
};
static const char * const coex_uart2_tx_groups[] = {
	"gpio48", "gpio90",
};
static const char * const coex_uart_rx_groups[] = {
	"gpio47",
};
static const char * const coex_uart_tx_groups[] = {
	"gpio46",
};
static const char * const cri_trng_rosc_groups[] = {
	"gpio36",
};
static const char * const cri_trng_rosc0_groups[] = {
	"gpio31",
};
static const char * const cri_trng_rosc1_groups[] = {
	"gpio32",
};
static const char * const dbg_out_clk_groups[] = {
	"gpio26",
};
static const char * const ddr_bist_complete_groups[] = {
	"gpio46",
};
static const char * const ddr_bist_fail_groups[] = {
	"gpio47",
};
static const char * const ddr_bist_start_groups[] = {
	"gpio48",
};
static const char * const ddr_bist_stop_groups[] = {
	"gpio49",
};
static const char * const ddr_pxi0_test_groups[] = {
	"gpio45", "gpio46",
};
static const char * const ebi0_wrcdc_dq2_groups[] = {
	"gpio2",
};
static const char * const ebi0_wrcdc_dq3_groups[] = {
	"gpio0",
};
static const char * const ebi2_a_d_groups[] = {
	"gpio100",
};
static const char * const ebi2_lcd_cs_groups[] = {
	"gpio101",
};
static const char * const ebi2_lcd_reset_groups[] = {
	"gpio99",
};
static const char * const ebi2_lcd_te_groups[] = {
	"gpio98",
};
static const char * const emac0_mcg_pst0_groups[] = {
	"gpio83",
};
static const char * const emac0_mcg_pst1_groups[] = {
	"gpio89",
};
static const char * const emac0_mcg_pst2_groups[] = {
	"gpio84",
};
static const char * const emac0_mcg_pst3_groups[] = {
	"gpio85",
};
static const char * const emac0_ptp_aux_groups[] = {
	"gpio35", "gpio83", "gpio84", "gpio85", "gpio89", "gpio119",
};
static const char * const emac0_ptp_pps_groups[] = {
	"gpio35", "gpio83", "gpio89", "gpio123", "gpio123", "gpio123",
	"gpio123",
};
static const char * const emac1_mcg_pst0_groups[] = {
	"gpio90",
};
static const char * const emac1_mcg_pst1_groups[] = {
	"gpio93",
};
static const char * const emac1_mcg_pst2_groups[] = {
	"gpio122",
};
static const char * const emac1_mcg_pst3_groups[] = {
	"gpio92",
};
static const char * const emac1_ptp_aux0_groups[] = {
	"gpio112",
};
static const char * const emac1_ptp_aux1_groups[] = {
	"gpio113",
};
static const char * const emac1_ptp_aux2_groups[] = {
	"gpio114",
};
static const char * const emac1_ptp_aux3_groups[] = {
	"gpio115",
};
static const char * const emac1_ptp_pps0_groups[] = {
	"gpio112",
};
static const char * const emac1_ptp_pps1_groups[] = {
	"gpio113",
};
static const char * const emac1_ptp_pps2_groups[] = {
	"gpio114",
};
static const char * const emac1_ptp_pps3_groups[] = {
	"gpio115",
};
static const char * const emac_cdc_dtest0_groups[] = {
	"gpio39",
};
static const char * const emac_cdc_dtest1_groups[] = {
	"gpio38",
};
static const char * const emac_pps_in_groups[] = {
	"gpio127",
};
static const char * const ext_dbg_uart_groups[] = {
	"gpio12", "gpio13", "gpio14", "gpio15",
};
static const char * const gcc_125_clk_groups[] = {
	"gpio25",
};
static const char * const gcc_gp1_clk_groups[] = {
	"gpio39",
};
static const char * const gcc_gp2_clk_groups[] = {
	"gpio40",
};
static const char * const gcc_gp3_clk_groups[] = {
	"gpio41",
};
static const char * const gcc_plltest_bypassnl_groups[] = {
	"gpio81",
};
static const char * const gcc_plltest_resetn_groups[] = {
	"gpio82",
};
static const char * const i2s_mclk_groups[] = {
	"gpio74",
};
static const char * const jitter_bist_ref_groups[] = {
	"gpio41",
};
static const char * const ldo_en_groups[] = {
	"gpio8",
};
static const char * const ldo_update_groups[] = {
	"gpio62",
};
static const char * const m_voc_ext_groups[] = {
	"gpio62", "gpio63", "gpio64", "gpio65", "gpio71",
};
static const char * const mgpi_clk_req_groups[] = {
	"gpio39", "gpio40",
};
static const char * const native0_groups[] = {
	"gpio33",
};
static const char * const native1_groups[] = {
	"gpio41",
};
static const char * const native2_groups[] = {
	"gpio29",
};
static const char * const native3_groups[] = {
	"gpio26",
};
static const char * const native_char_start_groups[] = {
	"gpio57",
};
static const char * const native_tsens_osc_groups[] = {
	"gpio38",
};
static const char * const native_tsense_pwm1_groups[] = {
	"gpio64", "gpio76",
};
static const char * const nav_dr_sync_groups[] = {
	"gpio36",
};
static const char * const nav_gpio_0_groups[] = {
	"gpio36",
};
static const char * const nav_gpio_1_groups[] = {
	"gpio35",
};
static const char * const nav_gpio_2_groups[] = {
	"gpio104",
};
static const char * const nav_gpio_3_groups[] = {
	"gpio36",
};
static const char * const pa_indicator_1_groups[] = {
	"gpio58",
};
static const char * const pci_e_rst_groups[] = {
	"gpio42",
};
static const char * const pcie0_clkreq_n_groups[] = {
	"gpio43",
};
static const char * const pcie1_clkreq_n_groups[] = {
	"gpio124",
};
static const char * const pcie2_clkreq_n_groups[] = {
	"gpio121",
};
static const char * const pll_bist_sync_groups[] = {
	"gpio38",
};
static const char * const pll_clk_aux_groups[] = {
	"gpio40",
};
static const char * const pll_ref_clk_groups[] = {
	"gpio37",
};
static const char * const pri_mi2s_data0_groups[] = {
	"gpio17",
};
static const char * const pri_mi2s_data1_groups[] = {
	"gpio18",
};
static const char * const pri_mi2s_sck_groups[] = {
	"gpio19",
};
static const char * const pri_mi2s_ws_groups[] = {
	"gpio16",
};
static const char * const prng_rosc_test0_groups[] = {
	"gpio27",
};
static const char * const prng_rosc_test1_groups[] = {
	"gpio36",
};
static const char * const prng_rosc_test2_groups[] = {
	"gpio37",
};
static const char * const prng_rosc_test3_groups[] = {
	"gpio38",
};
static const char * const qdss_cti_trig0_groups[] = {
	"gpio16", "gpio17", "gpio56", "gpio57", "gpio59", "gpio60", "gpio78",
	"gpio79",
};
static const char * const qdss_cti_trig1_groups[] = {
	"gpio16", "gpio17", "gpio52", "gpio52", "gpio53", "gpio53", "gpio56",
	"gpio57", "gpio78", "gpio79",
};
static const char * const qdss_gpio_traceclk_groups[] = {
	"gpio116",
};
static const char * const qdss_gpio_tracectl_groups[] = {
	"gpio117",
};
static const char * const qdss_gpio_tracedata0_groups[] = {
	"gpio118",
};
static const char * const qdss_gpio_tracedata1_groups[] = {
	"gpio119",
};
static const char * const qdss_gpio_tracedata10_groups[] = {
	"gpio114",
};
static const char * const qdss_gpio_tracedata11_groups[] = {
	"gpio115",
};
static const char * const qdss_gpio_tracedata12_groups[] = {
	"gpio83",
};
static const char * const qdss_gpio_tracedata13_groups[] = {
	"gpio82",
};
static const char * const qdss_gpio_tracedata14_groups[] = {
	"gpio84",
};
static const char * const qdss_gpio_tracedata15_groups[] = {
	"gpio85",
};
static const char * const qdss_gpio_tracedata2_groups[] = {
	"gpio94",
};
static const char * const qdss_gpio_tracedata3_groups[] = {
	"gpio95",
};
static const char * const qdss_gpio_tracedata4_groups[] = {
	"gpio96",
};
static const char * const qdss_gpio_tracedata5_groups[] = {
	"gpio97",
};
static const char * const qdss_gpio_tracedata6_groups[] = {
	"gpio110",
};
static const char * const qdss_gpio_tracedata7_groups[] = {
	"gpio111",
};
static const char * const qdss_gpio_tracedata8_groups[] = {
	"gpio112",
};
static const char * const qdss_gpio_tracedata9_groups[] = {
	"gpio113",
};
static const char * const qlink0_b_en_groups[] = {
	"gpio40",
};
static const char * const qlink0_b_req_groups[] = {
	"gpio41",
};
static const char * const qlink0_l_en_groups[] = {
	"gpio37",
};
static const char * const qlink0_l_req_groups[] = {
	"gpio38",
};
static const char * const qlink1_l_en_groups[] = {
	"gpio26",
};
static const char * const qlink1_l_req_groups[] = {
	"gpio27",
};
static const char * const qup_se0_l0_groups[] = {
	"gpio8",
};
static const char * const qup_se0_l1_groups[] = {
	"gpio9",
};
static const char * const qup_se0_l2_groups[] = {
	"gpio10",
};
static const char * const qup_se0_l3_groups[] = {
	"gpio11",
};
static const char * const qup_se1_l2_groups[] = {
	"gpio12", "gpio16",
};
static const char * const qup_se1_l3_groups[] = {
	"gpio13", "gpio17",
};
static const char * const qup_se2_l0_groups[] = {
	"gpio14",
};
static const char * const qup_se2_l1_groups[] = {
	"gpio15",
};
static const char * const qup_se2_l2_groups[] = {
	"gpio16",
};
static const char * const qup_se2_l3_groups[] = {
	"gpio17",
};
static const char * const qup_se3_l0_groups[] = {
	"gpio52",
};
static const char * const qup_se3_l1_groups[] = {
	"gpio53",
};
static const char * const qup_se3_l2_groups[] = {
	"gpio54",
};
static const char * const qup_se3_l3_groups[] = {
	"gpio55",
};
static const char * const qup_se4_l2_groups[] = {
	"gpio64",
};
static const char * const qup_se4_l3_groups[] = {
	"gpio65",
};
static const char * const qup_se5_l0_groups[] = {
	"gpio110",
};
static const char * const qup_se5_l1_groups[] = {
	"gpio111",
};
static const char * const qup_se6_l0_groups[] = {
	"gpio112",
};
static const char * const qup_se6_l1_groups[] = {
	"gpio113",
};
static const char * const qup_se6_l2_groups[] = {
	"gpio114",
};
static const char * const qup_se6_l3_groups[] = {
	"gpio115",
};
static const char * const qup_se7_l0_groups[] = {
	"gpio116",
};
static const char * const qup_se7_l1_groups[] = {
	"gpio117",
};
static const char * const qup_se7_l2_groups[] = {
	"gpio118",
};
static const char * const qup_se7_l3_groups[] = {
	"gpio119",
};
static const char * const qup_se8_l2_groups[] = {
	"gpio124",
};
static const char * const qup_se8_l3_groups[] = {
	"gpio125",
};
static const char * const sdc1_tb_trig_groups[] = {
	"gpio130",
};
static const char * const sdc2_tb_trig_groups[] = {
	"gpio129",
};
static const char * const sec_mi2s_data0_groups[] = {
	"gpio21",
};
static const char * const sec_mi2s_data1_groups[] = {
	"gpio22",
};
static const char * const sec_mi2s_sck_groups[] = {
	"gpio23",
};
static const char * const sec_mi2s_ws_groups[] = {
	"gpio20",
};
static const char * const sgmii_phy_intr0_groups[] = {
	"gpio97",
};
static const char * const sgmii_phy_intr1_groups[] = {
	"gpio109",
};
static const char * const spmi_coex_clk_groups[] = {
	"gpio49",
};
static const char * const spmi_coex_data_groups[] = {
	"gpio48",
};
static const char * const spmi_vgi_hwevent_groups[] = {
	"gpio50", "gpio51",
};
static const char * const tgu_ch0_trigout_groups[] = {
	"gpio55",
};
static const char * const tri_mi2s_data0_groups[] = {
	"gpio99",
};
static const char * const tri_mi2s_data1_groups[] = {
	"gpio100",
};
static const char * const tri_mi2s_sck_groups[] = {
	"gpio101",
};
static const char * const tri_mi2s_ws_groups[] = {
	"gpio98",
};
static const char * const uim1_clk_groups[] = {
	"gpio7",
};
static const char * const uim1_data_groups[] = {
	"gpio4",
};
static const char * const uim1_present_groups[] = {
	"gpio5",
};
static const char * const uim1_reset_groups[] = {
	"gpio6",
};
static const char * const uim2_clk_groups[] = {
	"gpio3",
};
static const char * const uim2_data_groups[] = {
	"gpio0",
};
static const char * const uim2_present_groups[] = {
	"gpio1",
};
static const char * const uim2_reset_groups[] = {
	"gpio2",
};
static const char * const usb2phy_ac_en_groups[] = {
	"gpio80",
};
static const char * const vsense_trigger_mirnat_groups[] = {
	"gpio37",
};

static const struct msm_function sdxpinn_functions[] = {
	FUNCTION(gpio),
	FUNCTION(ETH0_MDC),
	FUNCTION(ETH0_MDIO),
	FUNCTION(ETH1_MDC),
	FUNCTION(ETH1_MDIO),
	FUNCTION(QLINK0_WMSS_RESET),
	FUNCTION(QLINK1_WMSS_RESET),
	FUNCTION(RGMII_RXC),
	FUNCTION(RGMII_RXD0),
	FUNCTION(RGMII_RXD1),
	FUNCTION(RGMII_RXD2),
	FUNCTION(RGMII_RXD3),
	FUNCTION(RGMII_RX_CTL),
	FUNCTION(RGMII_TXC),
	FUNCTION(RGMII_TXD0),
	FUNCTION(RGMII_TXD1),
	FUNCTION(RGMII_TXD2),
	FUNCTION(RGMII_TXD3),
	FUNCTION(RGMII_TX_CTL),
	FUNCTION(adsp_ext_vfr),
	FUNCTION(atest_char_start),
	FUNCTION(atest_char_status0),
	FUNCTION(atest_char_status1),
	FUNCTION(atest_char_status2),
	FUNCTION(atest_char_status3),
	FUNCTION(audio_ref_clk),
	FUNCTION(bimc_dte_test0),
	FUNCTION(bimc_dte_test1),
	FUNCTION(char_exec_pending),
	FUNCTION(char_exec_release),
	FUNCTION(coex_uart2_rx),
	FUNCTION(coex_uart2_tx),
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
	FUNCTION(ebi0_wrcdc_dq2),
	FUNCTION(ebi0_wrcdc_dq3),
	FUNCTION(ebi2_a_d),
	FUNCTION(ebi2_lcd_cs),
	FUNCTION(ebi2_lcd_reset),
	FUNCTION(ebi2_lcd_te),
	FUNCTION(emac0_mcg_pst0),
	FUNCTION(emac0_mcg_pst1),
	FUNCTION(emac0_mcg_pst2),
	FUNCTION(emac0_mcg_pst3),
	FUNCTION(emac0_ptp_aux),
	FUNCTION(emac0_ptp_pps),
	FUNCTION(emac1_mcg_pst0),
	FUNCTION(emac1_mcg_pst1),
	FUNCTION(emac1_mcg_pst2),
	FUNCTION(emac1_mcg_pst3),
	FUNCTION(emac1_ptp_aux0),
	FUNCTION(emac1_ptp_aux1),
	FUNCTION(emac1_ptp_aux2),
	FUNCTION(emac1_ptp_aux3),
	FUNCTION(emac1_ptp_pps0),
	FUNCTION(emac1_ptp_pps1),
	FUNCTION(emac1_ptp_pps2),
	FUNCTION(emac1_ptp_pps3),
	FUNCTION(emac_cdc_dtest0),
	FUNCTION(emac_cdc_dtest1),
	FUNCTION(emac_pps_in),
	FUNCTION(ext_dbg_uart),
	FUNCTION(gcc_125_clk),
	FUNCTION(gcc_gp1_clk),
	FUNCTION(gcc_gp2_clk),
	FUNCTION(gcc_gp3_clk),
	FUNCTION(gcc_plltest_bypassnl),
	FUNCTION(gcc_plltest_resetn),
	FUNCTION(i2s_mclk),
	FUNCTION(jitter_bist_ref),
	FUNCTION(ldo_en),
	FUNCTION(ldo_update),
	FUNCTION(m_voc_ext),
	FUNCTION(mgpi_clk_req),
	FUNCTION(native0),
	FUNCTION(native1),
	FUNCTION(native2),
	FUNCTION(native3),
	FUNCTION(native_char_start),
	FUNCTION(native_tsens_osc),
	FUNCTION(native_tsense_pwm1),
	FUNCTION(nav_dr_sync),
	FUNCTION(nav_gpio_0),
	FUNCTION(nav_gpio_1),
	FUNCTION(nav_gpio_2),
	FUNCTION(nav_gpio_3),
	FUNCTION(pa_indicator_1),
	FUNCTION(pci_e_rst),
	FUNCTION(pcie0_clkreq_n),
	FUNCTION(pcie1_clkreq_n),
	FUNCTION(pcie2_clkreq_n),
	FUNCTION(pll_bist_sync),
	FUNCTION(pll_clk_aux),
	FUNCTION(pll_ref_clk),
	FUNCTION(pri_mi2s_data0),
	FUNCTION(pri_mi2s_data1),
	FUNCTION(pri_mi2s_sck),
	FUNCTION(pri_mi2s_ws),
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
	FUNCTION(qlink0_b_en),
	FUNCTION(qlink0_b_req),
	FUNCTION(qlink0_l_en),
	FUNCTION(qlink0_l_req),
	FUNCTION(qlink1_l_en),
	FUNCTION(qlink1_l_req),
	FUNCTION(qup_se0_l0),
	FUNCTION(qup_se0_l1),
	FUNCTION(qup_se0_l2),
	FUNCTION(qup_se0_l3),
	FUNCTION(qup_se1_l2),
	FUNCTION(qup_se1_l3),
	FUNCTION(qup_se2_l0),
	FUNCTION(qup_se2_l1),
	FUNCTION(qup_se2_l2),
	FUNCTION(qup_se2_l3),
	FUNCTION(qup_se3_l0),
	FUNCTION(qup_se3_l1),
	FUNCTION(qup_se3_l2),
	FUNCTION(qup_se3_l3),
	FUNCTION(qup_se4_l2),
	FUNCTION(qup_se4_l3),
	FUNCTION(qup_se5_l0),
	FUNCTION(qup_se5_l1),
	FUNCTION(qup_se6_l0),
	FUNCTION(qup_se6_l1),
	FUNCTION(qup_se6_l2),
	FUNCTION(qup_se6_l3),
	FUNCTION(qup_se7_l0),
	FUNCTION(qup_se7_l1),
	FUNCTION(qup_se7_l2),
	FUNCTION(qup_se7_l3),
	FUNCTION(qup_se8_l2),
	FUNCTION(qup_se8_l3),
	FUNCTION(sdc1_tb_trig),
	FUNCTION(sdc2_tb_trig),
	FUNCTION(sec_mi2s_data0),
	FUNCTION(sec_mi2s_data1),
	FUNCTION(sec_mi2s_sck),
	FUNCTION(sec_mi2s_ws),
	FUNCTION(sgmii_phy_intr0),
	FUNCTION(sgmii_phy_intr1),
	FUNCTION(spmi_coex_clk),
	FUNCTION(spmi_coex_data),
	FUNCTION(spmi_vgi_hwevent),
	FUNCTION(tgu_ch0_trigout),
	FUNCTION(tri_mi2s_data0),
	FUNCTION(tri_mi2s_data1),
	FUNCTION(tri_mi2s_sck),
	FUNCTION(tri_mi2s_ws),
	FUNCTION(uim1_clk),
	FUNCTION(uim1_data),
	FUNCTION(uim1_present),
	FUNCTION(uim1_reset),
	FUNCTION(uim2_clk),
	FUNCTION(uim2_data),
	FUNCTION(uim2_present),
	FUNCTION(uim2_reset),
	FUNCTION(usb2phy_ac_en),
	FUNCTION(vsense_trigger_mirnat),
};

/* Every pin is maintained as a single group, and missing or non-existing pin
 * would be maintained as dummy group to synchronize pin group index with
 * pin descriptor registered with pinctrl core.
 * Clients would not be able to request these dummy pin groups.
 */
static const struct msm_pingroup sdxpinn_groups[] = {
	[0] = PINGROUP(0, uim2_data, ebi0_wrcdc_dq3, NA, NA, NA, NA, NA, NA,
		       NA, 0, -1),
	[1] = PINGROUP(1, uim2_present, NA, NA, NA, NA, NA, NA, NA, NA, 0x96000, 11),
	[2] = PINGROUP(2, uim2_reset, ebi0_wrcdc_dq2, NA, NA, NA, NA, NA, NA,
		       NA, 0x96000, 12),
	[3] = PINGROUP(3, uim2_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[4] = PINGROUP(4, uim1_data, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[5] = PINGROUP(5, uim1_present, NA, NA, NA, NA, NA, NA, NA, NA, 0x96000, 13),
	[6] = PINGROUP(6, uim1_reset, char_exec_pending, NA, NA, NA, NA, NA,
		       NA, NA, 0x96000, 14),
	[7] = PINGROUP(7, uim1_clk, char_exec_release, NA, NA, NA, NA, NA, NA,
		       NA, 0, -1),
	[8] = PINGROUP(8, qup_se0_l0, ldo_en, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[9] = PINGROUP(9, qup_se0_l1, NA, NA, NA, NA, NA, NA, NA, NA, 0x96000, 15),
	[10] = PINGROUP(10, qup_se0_l2, NA, NA, NA, NA, NA, NA, NA, NA, 0x96000, 16),
	[11] = PINGROUP(11, qup_se0_l3, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[12] = PINGROUP(12, qup_se1_l2, ext_dbg_uart, NA, NA, NA, NA, NA, NA,
			NA, 0, -1),
	[13] = PINGROUP(13, qup_se1_l3, ext_dbg_uart, NA, NA, NA, NA, NA, NA,
			NA, 0x96000, 17),
	[14] = PINGROUP(14, qup_se2_l0, ext_dbg_uart, bimc_dte_test0, NA, NA,
			NA, NA, NA, NA, 0, -1),
	[15] = PINGROUP(15, qup_se2_l1, ext_dbg_uart, bimc_dte_test1, NA, NA,
			NA, NA, NA, NA, 0x96000, 18),
	[16] = PINGROUP(16, pri_mi2s_ws, qup_se2_l2, qup_se1_l2,
			qdss_cti_trig1, qdss_cti_trig0, NA, NA, NA, NA, 0x96000, 19),
	[17] = PINGROUP(17, pri_mi2s_data0, qup_se2_l3, qup_se1_l3,
			qdss_cti_trig1, qdss_cti_trig0, NA, NA, NA, NA, 0, -1),
	[18] = PINGROUP(18, pri_mi2s_data1, NA, NA, NA, NA, NA, NA, NA, NA, 0x96000, 20),
	[19] = PINGROUP(19, pri_mi2s_sck, NA, NA, NA, NA, NA, NA, NA, NA, 0x96000, 21),
	[20] = PINGROUP(20, sec_mi2s_ws, NA, NA, NA, NA, NA, NA, NA, NA, 0x96000, 22),
	[21] = PINGROUP(21, sec_mi2s_data0, NA, NA, NA, NA, NA, NA, NA, NA, 0x96000, 23),
	[22] = PINGROUP(22, sec_mi2s_data1, NA, NA, NA, NA, NA, NA, NA, NA, 0x96000, 24),
	[23] = PINGROUP(23, sec_mi2s_sck, NA, NA, NA, NA, NA, NA, NA, NA, 0x96000, 25),
	[24] = PINGROUP(24, NA, atest_char_status3, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[25] = PINGROUP(25, gcc_125_clk, NA, atest_char_status2, NA, NA, NA,
			NA, NA, NA, 0x96000, 26),
	[26] = PINGROUP(26, NA, NA, qlink1_l_en, dbg_out_clk, native3, NA, NA,
			NA, NA, 0, -1),
	[27] = PINGROUP(27, NA, NA, qlink1_l_req, prng_rosc_test0, NA, NA, NA,
			NA, NA, 0x96000, 27),
	[28] = PINGROUP(28, NA, QLINK1_WMSS_RESET, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[29] = PINGROUP(29, NA, NA, NA, native2, NA, NA, NA, NA, NA, 0, -1),
	[30] = PINGROUP(30, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[31] = PINGROUP(31, NA, NA, cri_trng_rosc0, NA, NA, NA, NA, NA, NA, 0, -1),
	[32] = PINGROUP(32, NA, NA, cri_trng_rosc1, NA, NA, NA, NA, NA, NA, 0, -1),
	[33] = PINGROUP(33, NA, NA, native0, NA, NA, NA, NA, NA, NA, 0, -1),
	[34] = PINGROUP(34, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[35] = PINGROUP(35, nav_gpio_1, emac0_ptp_aux, emac0_ptp_pps, NA, NA,
			NA, NA, NA, NA, 0x96000, 28),
	[36] = PINGROUP(36, nav_gpio_3, nav_dr_sync, nav_gpio_0, cri_trng_rosc,
			prng_rosc_test1, NA, NA, NA, NA, 0x96000, 29),
	[37] = PINGROUP(37, qlink0_l_en, NA, pll_ref_clk, prng_rosc_test2,
			vsense_trigger_mirnat, NA, NA, NA, NA, 0, -1),
	[38] = PINGROUP(38, qlink0_l_req, NA, pll_bist_sync, prng_rosc_test3,
			NA, emac_cdc_dtest1, NA, native_tsens_osc, NA, 0x96000, 30),
	[39] = PINGROUP(39, QLINK0_WMSS_RESET, NA, mgpi_clk_req, gcc_gp1_clk,
			NA, emac_cdc_dtest0, NA, NA, NA, 0x96000, 31),
	[40] = PINGROUP(40, qlink0_b_en, NA, mgpi_clk_req, pll_clk_aux,
			gcc_gp2_clk, NA, NA, NA, NA, 0x96004, 0),
	[41] = PINGROUP(41, qlink0_b_req, NA, jitter_bist_ref, gcc_gp3_clk, NA,
			NA, native1, NA, NA, 0x96004, 1),
	[42] = PINGROUP(42, pci_e_rst, NA, NA, NA, NA, NA, NA, NA, NA, 0x96004, 2),
	[43] = PINGROUP(43, pcie0_clkreq_n, NA, NA, NA, NA, NA, NA, NA, NA, 0x96004, 3),
	[44] = PINGROUP(44, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x96004, 4),
	[45] = PINGROUP(45, ddr_pxi0_test, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[46] = PINGROUP(46, coex_uart_tx, ddr_bist_complete, ddr_pxi0_test, NA,
			NA, NA, NA, NA, NA, 0x96004, 5),
	[47] = PINGROUP(47, coex_uart_rx, ddr_bist_fail, NA, NA, NA, NA, NA,
			NA, NA, 0x96004, 6),
	[48] = PINGROUP(48, coex_uart2_tx, spmi_coex_data, ddr_bist_start, NA,
			NA, NA, NA, NA, NA, 0, -1),
	[49] = PINGROUP(49, coex_uart2_rx, spmi_coex_clk, ddr_bist_stop, NA,
			NA, NA, NA, NA, NA, 0x96004, 7),
	[50] = PINGROUP(50, spmi_vgi_hwevent, NA, NA, NA, NA, NA, NA, NA, NA, 0x96004, 8),
	[51] = PINGROUP(51, spmi_vgi_hwevent, NA, NA, NA, NA, NA, NA, NA, NA, 0x96004, 9),
	[52] = PINGROUP(52, qup_se3_l0, qdss_cti_trig1, qdss_cti_trig1, NA, NA,
			NA, NA, NA, NA, 0x96004, 10),
	[53] = PINGROUP(53, qup_se3_l1, qdss_cti_trig1, qdss_cti_trig1, NA, NA,
			NA, NA, NA, NA, 0x96004, 11),
	[54] = PINGROUP(54, qup_se3_l2, NA, NA, NA, NA, NA, NA, NA, NA, 0x96004, 12),
	[55] = PINGROUP(55, qup_se3_l3, tgu_ch0_trigout, NA, NA, NA, NA, NA,
			NA, NA, 0x96004, 13),
	[56] = PINGROUP(56, qdss_cti_trig1, qdss_cti_trig0, NA, NA, NA, NA, NA,
			NA, NA, 0x96004, 14),
	[57] = PINGROUP(57, qdss_cti_trig1, qdss_cti_trig0, NA,
			native_char_start, NA, NA, NA, NA, NA, 0x96004, 15),
	[58] = PINGROUP(58, NA, pa_indicator_1, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[59] = PINGROUP(59, adsp_ext_vfr, qdss_cti_trig0, NA, bimc_dte_test0,
			NA, NA, NA, NA, NA, 0x96004, 16),
	[60] = PINGROUP(60, qdss_cti_trig0, NA, NA, NA, NA, NA, NA, NA, NA, 0x96004, 17),
	[61] = PINGROUP(61, NA, bimc_dte_test1, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[62] = PINGROUP(62, m_voc_ext, ldo_update, NA, NA, NA, NA, NA, NA, NA, 0x96004, 18),
	[63] = PINGROUP(63, m_voc_ext, NA, atest_char_start, NA, NA, NA, NA,
			NA, NA, 0x96004, 19),
	[64] = PINGROUP(64, qup_se4_l2, m_voc_ext, NA, native_tsense_pwm1, NA,
			NA, NA, NA, NA, 0x96004, 20),
	[65] = PINGROUP(65, qup_se4_l3, m_voc_ext, NA, NA, NA, NA, NA, NA, NA, 0x96004, 21),
	[66] = PINGROUP(66, NA, atest_char_status1, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[67] = PINGROUP(67, NA, atest_char_status0, NA, NA, NA, NA, NA, NA, NA, 0x96004, 22),
	[68] = PINGROUP(68, adsp_ext_vfr, NA, NA, NA, NA, NA, NA, NA, NA, 0x96004, 23),
	[69] = PINGROUP(69, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x96004, 24),
	[70] = PINGROUP(70, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x96004, 25),
	[71] = PINGROUP(71, m_voc_ext, NA, NA, NA, NA, NA, NA, NA, NA, 0x96004, 26),
	[72] = PINGROUP(72, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x96004, 27),
	[73] = PINGROUP(73, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[74] = PINGROUP(74, i2s_mclk, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[75] = PINGROUP(75, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x96004, 28),
	[76] = PINGROUP(76, native_tsense_pwm1, NA, NA, NA, NA, NA, NA, NA, NA, 0x96004, 29),
	[77] = PINGROUP(77, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[78] = PINGROUP(78, qdss_cti_trig1, qdss_cti_trig0, NA, NA, NA, NA, NA,
			NA, NA, 0x96004, 30),
	[79] = PINGROUP(79, qdss_cti_trig1, qdss_cti_trig0, NA, NA, NA, NA, NA,
			NA, NA, 0x96004, 31),
	[80] = PINGROUP(80, usb2phy_ac_en, NA, NA, NA, NA, NA, NA, NA, NA, 0x96008, 0),
	[81] = PINGROUP(81, gcc_plltest_bypassnl, NA, NA, NA, NA, NA, NA, NA,
			NA, 0x96008, 1),
	[82] = PINGROUP(82, RGMII_TXC, gcc_plltest_resetn,
			qdss_gpio_tracedata13, NA, NA, NA, NA, NA, NA, 0, -1),
	[83] = PINGROUP(83, RGMII_TXD0, emac0_ptp_aux, emac0_ptp_pps,
			emac0_mcg_pst0, qdss_gpio_tracedata12, NA, NA, NA, NA, 0, -1),
	[84] = PINGROUP(84, RGMII_TXD1, emac0_ptp_aux, emac0_mcg_pst2,
			qdss_gpio_tracedata14, NA, NA, NA, NA, NA, 0, -1),
	[85] = PINGROUP(85, RGMII_TXD2, emac0_ptp_aux, emac0_mcg_pst3,
			qdss_gpio_tracedata15, NA, NA, NA, NA, NA, 0x96008, 2),
	[86] = PINGROUP(86, RGMII_TXD3, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[87] = PINGROUP(87, RGMII_TX_CTL, NA, NA, NA, NA, NA, NA, NA, NA, 0x96008, 3),
	[88] = PINGROUP(88, RGMII_RXC, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[89] = PINGROUP(89, RGMII_RXD0, emac0_ptp_aux, emac0_ptp_pps,
			emac0_mcg_pst1, NA, NA, NA, NA, NA, 0, -1),
	[90] = PINGROUP(90, RGMII_RXD1, coex_uart2_tx, emac1_mcg_pst0, NA, NA,
			NA, NA, NA, NA, 0, -1),
	[91] = PINGROUP(91, RGMII_RXD2, coex_uart2_rx, NA, NA, NA, NA, NA, NA,
			NA, 0x96008, 4),
	[92] = PINGROUP(92, RGMII_RXD3, emac1_mcg_pst3, NA, NA, NA, NA, NA, NA,
			NA, 0x96008, 5),
	[93] = PINGROUP(93, RGMII_RX_CTL, emac1_mcg_pst1, NA, NA, NA, NA, NA,
			NA, NA, 0x96008, 6),
	[94] = PINGROUP(94, ETH0_MDC, qdss_gpio_tracedata2, NA, NA, NA, NA, NA,
			NA, NA, 0x96008, 7),
	[95] = PINGROUP(95, ETH0_MDIO, qdss_gpio_tracedata3, NA, NA, NA, NA,
			NA, NA, NA, 0x96008, 8),
	[96] = PINGROUP(96, qdss_gpio_tracedata4, NA, NA, NA, NA, NA, NA, NA,
			NA, 0, -1),
	[97] = PINGROUP(97, sgmii_phy_intr0, NA, qdss_gpio_tracedata5, NA, NA,
			NA, NA, NA, NA, 0x96008, 9),
	[98] = PINGROUP(98, tri_mi2s_ws, ebi2_lcd_te, NA, NA, NA, NA, NA, NA,
			NA, 0x96008, 10),
	[99] = PINGROUP(99, tri_mi2s_data0, ebi2_lcd_reset, NA, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[100] = PINGROUP(100, tri_mi2s_data1, ebi2_a_d, NA, NA, NA, NA, NA, NA,
			 NA, 0x96008, 11),
	[101] = PINGROUP(101, tri_mi2s_sck, ebi2_lcd_cs, NA, NA, NA, NA, NA,
			 NA, NA, 0x96008, 12),
	[102] = PINGROUP(102, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[103] = PINGROUP(103, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x96008, 13),
	[104] = PINGROUP(104, nav_gpio_2, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[105] = PINGROUP(105, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x96008, 14),
	[106] = PINGROUP(106, ETH1_MDC, NA, NA, NA, NA, NA, NA, NA, NA, 0x96008, 15),
	[107] = PINGROUP(107, ETH1_MDIO, NA, NA, NA, NA, NA, NA, NA, NA, 0x96008, 16),
	[108] = PINGROUP(108, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x96008, 17),
	[109] = PINGROUP(109, sgmii_phy_intr1, NA, NA, NA, NA, NA, NA, NA, NA, 0x96008, 18),
	[110] = PINGROUP(110, qup_se5_l0, qdss_gpio_tracedata6, NA, NA, NA, NA,
			 NA, NA, NA, 0, -1),
	[111] = PINGROUP(111, qup_se5_l1, qdss_gpio_tracedata7, NA, NA, NA, NA,
			 NA, NA, NA, 0x96008, 19),
	[112] = PINGROUP(112, qup_se6_l0, emac1_ptp_aux0, emac1_ptp_pps0,
			 qdss_gpio_tracedata8, NA, NA, NA, NA, NA, 0, -1),
	[113] = PINGROUP(113, qup_se6_l1, emac1_ptp_aux1, emac1_ptp_pps1,
			 qdss_gpio_tracedata9, NA, NA, NA, NA, NA, 0x96008, 20),
	[114] = PINGROUP(114, qup_se6_l2, emac1_ptp_aux2, emac1_ptp_pps2,
			 qdss_gpio_tracedata10, NA, NA, NA, NA, NA, 0, -1),
	[115] = PINGROUP(115, qup_se6_l3, emac1_ptp_aux3, emac1_ptp_pps3,
			 qdss_gpio_tracedata11, NA, NA, NA, NA, NA, 0x96008, 21),
	[116] = PINGROUP(116, qup_se7_l0, qdss_gpio_traceclk, NA, NA, NA, NA,
			 NA, NA, NA, 0x96008, 22),
	[117] = PINGROUP(117, qup_se7_l1, qdss_gpio_tracectl, NA, NA, NA, NA,
			 NA, NA, NA, 0x96008, 23),
	[118] = PINGROUP(118, qup_se7_l2, qdss_gpio_tracedata0, NA, NA, NA, NA,
			 NA, NA, NA, 0x96008, 24),
	[119] = PINGROUP(119, qup_se7_l3, emac0_ptp_aux, qdss_gpio_tracedata1,
			 NA, NA, NA, NA, NA, NA, 0x96008, 25),
	[120] = PINGROUP(120, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x96008, 26),
	[121] = PINGROUP(121, pcie2_clkreq_n, NA, NA, NA, NA, NA, NA, NA, NA, 0x96008, 27),
	[122] = PINGROUP(122, emac1_mcg_pst2, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[123] = PINGROUP(123, emac0_ptp_pps, emac0_ptp_pps, emac0_ptp_pps,
			 emac0_ptp_pps, NA, NA, NA, NA, NA, 0x96008, 28),
	[124] = PINGROUP(124, pcie1_clkreq_n, qup_se8_l2, NA, NA, NA, NA, NA,
			 NA, NA, 0x96008, 29),
	[125] = PINGROUP(125, qup_se8_l3, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[126] = PINGROUP(126, audio_ref_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[127] = PINGROUP(127, emac_pps_in, NA, NA, NA, NA, NA, NA, NA, NA, 0x96008, 30),
	[128] = PINGROUP(128, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x96008, 31),
	[129] = PINGROUP(129, sdc2_tb_trig, NA, NA, NA, NA, NA, NA, NA, NA, 0x9600C, 0),
	[130] = PINGROUP(130, sdc1_tb_trig, NA, NA, NA, NA, NA, NA, NA, NA, 0x9600C, 1),
	[131] = PINGROUP(131, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[132] = PINGROUP(132, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x9600C, 2),
	[133] = SDC_QDSD_PINGROUP(sdc1_rclk, 0x19a000, 15, 0),
	[134] = SDC_QDSD_PINGROUP(sdc1_clk, 0x19a000, 13, 6),
	[135] = SDC_QDSD_PINGROUP(sdc1_cmd, 0x19a000, 11, 3),
	[136] = SDC_QDSD_PINGROUP(sdc1_data, 0x19a000, 9, 0),
	[137] = SDC_QDSD_PINGROUP(sdc2_clk, 0x19b000, 14, 6),
	[138] = SDC_QDSD_PINGROUP(sdc2_cmd, 0x19b000, 11, 3),
	[139] = SDC_QDSD_PINGROUP(sdc2_data, 0x19b000, 9, 0),
};

static const struct msm_gpio_wakeirq_map sdxpinn_pdc_map[] = {
	{ 2, 91 }, { 6, 109 }, { 9, 129 }, { 11, 62 }, { 13, 84 }, { 15, 87 },
	{ 17, 88 }, { 18, 89 }, { 19, 90 }, { 20, 92 }, { 21, 93 }, { 22, 94 },
	{ 23, 95 }, { 25, 96 }, { 27, 97 }, { 38, 98 }, { 39, 99 }, { 40, 100 },
	{ 41, 101 }, { 52, 102 }, { 53, 141 }, { 54, 104 }, { 55, 105 }, { 56, 106 },
	{ 57, 107 }, { 59, 108 }, { 60, 110 }, { 62, 111 }, { 63, 112 }, { 64, 113 },
	{ 65, 114 }, { 67, 115 }, { 68, 116 }, { 69, 117 }, { 70, 118 }, { 71, 119 },
	{ 72, 120 }, { 75, 121 }, { 76, 122 }, { 78, 123 }, { 79, 124 }, { 80, 125 },
	{ 81, 50 }, { 85, 127 }, { 87, 128 }, { 91, 130 }, { 92, 131 }, { 93, 132 },
	{ 94, 133 }, { 95, 134 }, { 97, 135 }, { 98, 136 }, { 101, 64 }, { 105, 65 },
	{ 106, 66 }, { 107, 67 }, { 108, 68 }, { 109, 69 }, { 111, 70 }, { 113, 59 },
	{ 115, 72 }, { 116, 73 }, { 117, 74 }, { 118, 75 }, { 119, 76 }, { 120, 77 },
	{ 121, 78 }, { 123, 79 }, { 124, 80 }, { 125, 63 }, { 127, 81 }, { 128, 82 },
	{ 129, 83 }, { 130, 85 }, { 132, 86 },
};

static const struct msm_pinctrl_soc_data sdxpinn_pinctrl = {
	.pins = sdxpinn_pins,
	.npins = ARRAY_SIZE(sdxpinn_pins),
	.functions = sdxpinn_functions,
	.nfunctions = ARRAY_SIZE(sdxpinn_functions),
	.groups = sdxpinn_groups,
	.ngroups = ARRAY_SIZE(sdxpinn_groups),
	.ngpios = 133,
	.wakeirq_map = sdxpinn_pdc_map,
	.nwakeirq_map = ARRAY_SIZE(sdxpinn_pdc_map),
};

static int sdxpinn_pinctrl_probe(struct platform_device *pdev)
{
	return msm_pinctrl_probe(pdev, &sdxpinn_pinctrl);
}

static const struct of_device_id sdxpinn_pinctrl_of_match[] = {
	{ .compatible = "qcom,sdxpinn-pinctrl", },
	{ },
};

static struct platform_driver sdxpinn_pinctrl_driver = {
	.driver = {
		.name = "sdxpinn-pinctrl",
		.of_match_table = sdxpinn_pinctrl_of_match,
	},
	.probe = sdxpinn_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init sdxpinn_pinctrl_init(void)
{
	return platform_driver_register(&sdxpinn_pinctrl_driver);
}
arch_initcall(sdxpinn_pinctrl_init);

static void __exit sdxpinn_pinctrl_exit(void)
{
	platform_driver_unregister(&sdxpinn_pinctrl_driver);
}
module_exit(sdxpinn_pinctrl_exit);

MODULE_DESCRIPTION("QTI sdxpinn pinctrl driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, sdxpinn_pinctrl_of_match);
