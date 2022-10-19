// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>

#include "pinctrl-msm.h"

static const char * const sa410m_tiles[] = {
	"normal",
	"extended"
};

enum {
	NORMAL,
	EXTENDED
};

#define FUNCTION(fname)			                \
	[msm_mux_##fname] = {		                \
		.name = #fname,				\
		.groups = fname##_groups,               \
		.ngroups = ARRAY_SIZE(fname##_groups),	\
	}

#define REG_BASE 0x0
#define REG_SIZE 0x1000
#define REG_BASE1 0x0
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
		.tile = NORMAL,				\
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

#define PINGROUP1(id, f1, f2, f3, f4, f5, f6, f7, f8, f9, wake_off, bit)	\
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
		.tile = EXTENDED,                      \
		.ctl_reg = REG_BASE1 + REG_SIZE * (id-127),			\
		.io_reg = REG_BASE1 + 0x4 + REG_SIZE * (id-127),		\
		.intr_cfg_reg = REG_BASE1 + 0x8 + REG_SIZE * (id-127),		\
		.intr_status_reg = REG_BASE1 + 0xc + REG_SIZE * (id-127),	\
		.intr_target_reg = REG_BASE1 + 0x8 + REG_SIZE * (id-127),	\
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


static const struct pinctrl_pin_desc sa410m_pins[] = {
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
	PINCTRL_PIN(65, "GPIO_69"),
	PINCTRL_PIN(66, "GPIO_70"),
	PINCTRL_PIN(67, "GPIO_71"),
	PINCTRL_PIN(68, "GPIO_72"),
	PINCTRL_PIN(69, "GPIO_73"),
	PINCTRL_PIN(70, "GPIO_74"),
	PINCTRL_PIN(71, "GPIO_75"),
	PINCTRL_PIN(72, "GPIO_76"),
	PINCTRL_PIN(73, "GPIO_77"),
	PINCTRL_PIN(74, "GPIO_78"),
	PINCTRL_PIN(75, "GPIO_79"),
	PINCTRL_PIN(76, "GPIO_80"),
	PINCTRL_PIN(77, "GPIO_81"),
	PINCTRL_PIN(78, "GPIO_82"),
	PINCTRL_PIN(79, "GPIO_86"),
	PINCTRL_PIN(80, "GPIO_87"),
	PINCTRL_PIN(81, "GPIO_88"),
	PINCTRL_PIN(82, "GPIO_89"),
	PINCTRL_PIN(83, "GPIO_90"),
	PINCTRL_PIN(84, "GPIO_91"),
	PINCTRL_PIN(85, "GPIO_94"),
	PINCTRL_PIN(86, "GPIO_95"),
	PINCTRL_PIN(87, "GPIO_96"),
	PINCTRL_PIN(88, "GPIO_97"),
	PINCTRL_PIN(89, "GPIO_98"),
	PINCTRL_PIN(90, "GPIO_99"),
	PINCTRL_PIN(91, "GPIO_100"),
	PINCTRL_PIN(92, "GPIO_101"),
	PINCTRL_PIN(93, "GPIO_102"),
	PINCTRL_PIN(94, "GPIO_103"),
	PINCTRL_PIN(95, "GPIO_104"),
	PINCTRL_PIN(96, "GPIO_105"),
	PINCTRL_PIN(97, "GPIO_106"),
	PINCTRL_PIN(98, "GPIO_107"),
	PINCTRL_PIN(99, "GPIO_108"),
	PINCTRL_PIN(100, "GPIO_109"),
	PINCTRL_PIN(101, "GPIO_110"),
	PINCTRL_PIN(102, "GPIO_111"),
	PINCTRL_PIN(103, "GPIO_112"),
	PINCTRL_PIN(104, "GPIO_113"),
	PINCTRL_PIN(105, "GPIO_114"),
	PINCTRL_PIN(106, "GPIO_115"),
	PINCTRL_PIN(107, "GPIO_116"),
	PINCTRL_PIN(108, "GPIO_117"),
	PINCTRL_PIN(109, "GPIO_118"),
	PINCTRL_PIN(110, "GPIO_119"),
	PINCTRL_PIN(111, "GPIO_120"),
	PINCTRL_PIN(112, "GPIO_121"),
	PINCTRL_PIN(113, "GPIO_122"),
	PINCTRL_PIN(114, "GPIO_123"),
	PINCTRL_PIN(115, "GPIO_124"),
	PINCTRL_PIN(116, "GPIO_125"),
	PINCTRL_PIN(117, "GPIO_126"),
	PINCTRL_PIN(118, "GPIO_127"),
	PINCTRL_PIN(119, "GPIO_128"),
	PINCTRL_PIN(120, "GPIO_129"),
	PINCTRL_PIN(121, "GPIO_130"),
	PINCTRL_PIN(122, "GPIO_131"),
	PINCTRL_PIN(123, "GPIO_132"),
	PINCTRL_PIN(124, "GPIO_133"),
	PINCTRL_PIN(125, "GPIO_134"),
	PINCTRL_PIN(126, "GPIO_135"),
	PINCTRL_PIN(127, "GPIO_136"),
	PINCTRL_PIN(128, "GPIO_137"),
	PINCTRL_PIN(129, "GPIO_138"),
	PINCTRL_PIN(130, "GPIO_139"),
	PINCTRL_PIN(131, "SDC1_RCLK"),
	PINCTRL_PIN(132, "SDC1_CLK"),
	PINCTRL_PIN(133, "SDC1_CMD"),
	PINCTRL_PIN(134, "SDC1_DATA"),
	PINCTRL_PIN(135, "SDC2_CLK"),
	PINCTRL_PIN(136, "SDC2_CMD"),
	PINCTRL_PIN(137, "SDC2_DATA"),
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

static const unsigned int sdc1_rclk_pins[] = { 131 };
static const unsigned int sdc1_clk_pins[] = { 132 };
static const unsigned int sdc1_cmd_pins[] = { 133 };
static const unsigned int sdc1_data_pins[] = { 134 };
static const unsigned int sdc2_clk_pins[] = { 135 };
static const unsigned int sdc2_cmd_pins[] = { 136 };
static const unsigned int sdc2_data_pins[] = { 137 };

enum sa410m_functions {
	msm_mux_gpio,
	msm_mux_AGERA_PLL_REF,
	msm_mux_CRI_TRNG_ROSC,
	msm_mux_CRI_TRNG_ROSC0,
	msm_mux_CRI_TRNG_ROSC1,
	msm_mux_GCC_GP1_CLK,
	msm_mux_GCC_GP2_CLK,
	msm_mux_GCC_GP3_CLK,
	msm_mux_GP0,
	msm_mux_GP1,
	msm_mux_GP2,
	msm_mux_JITTER_BIST_REF,
	msm_mux_PA_INDICATOR_OR,
	msm_mux_PCIE0_CLK_REQ,
	msm_mux_PLL_BIST_SYNC,
	msm_mux_RGMII0_MDC,
	msm_mux_RGMII0_MDIO,
	msm_mux_RGMII0_RXC,
	msm_mux_RGMII0_RXD0,
	msm_mux_RGMII0_RXD1,
	msm_mux_RGMII0_RXD2,
	msm_mux_RGMII0_RXD3,
	msm_mux_RGMII0_RX_CTL,
	msm_mux_RGMII0_TXC,
	msm_mux_RGMII0_TXD0,
	msm_mux_RGMII0_TXD1,
	msm_mux_RGMII0_TXD2,
	msm_mux_RGMII0_TXD3,
	msm_mux_RGMII0_TX_CTL,
	msm_mux_SDC1_TB_TRIG,
	msm_mux_SDC2_TB_TRIG,
	msm_mux_SSBI_WTR1_RX,
	msm_mux_SSBI_WTR1_TX,
	msm_mux_adsp_ext_vfr,
	msm_mux_atest_bbrx_dtestout0,
	msm_mux_atest_bbrx_dtestout1,
	msm_mux_atest_char_start,
	msm_mux_atest_char_status0,
	msm_mux_atest_char_status1,
	msm_mux_atest_char_status2,
	msm_mux_atest_char_status3,
	msm_mux_atest_gpsadc0_dtest0,
	msm_mux_atest_gpsadc0_dtest1,
	msm_mux_atest_gpsadc1_dtest0,
	msm_mux_atest_gpsadc1_dtest1,
	msm_mux_atest_tsens2_osc,
	msm_mux_atest_tsens_osc,
	msm_mux_atest_usb1_atereset,
	msm_mux_atest_usb1_testdataout00,
	msm_mux_atest_usb1_testdataout01,
	msm_mux_atest_usb1_testdataout02,
	msm_mux_atest_usb1_testdataout03,
	msm_mux_atest_usb2_atereset,
	msm_mux_atest_usb2_testdataout00,
	msm_mux_atest_usb2_testdataout01,
	msm_mux_atest_usb2_testdataout02,
	msm_mux_atest_usb2_testdataout03,
	msm_mux_char_exec_pending,
	msm_mux_char_exec_release,
	msm_mux_dac_calib_data0,
	msm_mux_dac_calib_data1,
	msm_mux_dac_calib_data10,
	msm_mux_dac_calib_data11,
	msm_mux_dac_calib_data12,
	msm_mux_dac_calib_data13,
	msm_mux_dac_calib_data14,
	msm_mux_dac_calib_data15,
	msm_mux_dac_calib_data16,
	msm_mux_dac_calib_data17,
	msm_mux_dac_calib_data18,
	msm_mux_dac_calib_data19,
	msm_mux_dac_calib_data2,
	msm_mux_dac_calib_data20,
	msm_mux_dac_calib_data21,
	msm_mux_dac_calib_data22,
	msm_mux_dac_calib_data23,
	msm_mux_dac_calib_data24,
	msm_mux_dac_calib_data25,
	msm_mux_dac_calib_data3,
	msm_mux_dac_calib_data4,
	msm_mux_dac_calib_data5,
	msm_mux_dac_calib_data6,
	msm_mux_dac_calib_data7,
	msm_mux_dac_calib_data8,
	msm_mux_dac_calib_data9,
	msm_mux_dbg_out_clk,
	msm_mux_ddr_bist_complete,
	msm_mux_ddr_bist_fail,
	msm_mux_ddr_bist_start,
	msm_mux_ddr_bist_stop,
	msm_mux_ddr_pxi0_test,
	msm_mux_ddr_pxi1_test,
	msm_mux_ddr_pxi2_test,
	msm_mux_ddr_pxi3_test,
	msm_mux_emac0_dll_sdc4,
	msm_mux_emac0_mcg_pst0,
	msm_mux_emac0_mcg_pst1,
	msm_mux_emac0_mcg_pst2,
	msm_mux_emac0_mcg_pst3,
	msm_mux_emac0_phy_intr,
	msm_mux_emac0_ptp_aux,
	msm_mux_emac0_ptp_pps,
	msm_mux_gsm0_tx_phase,
	msm_mux_gsm1_tx_phase,
	msm_mux_m_voc_ext,
	msm_mux_mpm_pwr_cllps,
	msm_mux_mss_lte_coxm,
	msm_mux_nav_gpio0_mira,
	msm_mux_nav_gpio0_mirb,
	msm_mux_nav_gpio0_mirc,
	msm_mux_nav_gpio1_mira,
	msm_mux_nav_gpio1_mirb,
	msm_mux_nav_gpio1_mirc,
	msm_mux_nav_gpio2_mira,
	msm_mux_nav_gpio2_mirb,
	msm_mux_nav_gpio2_mirc,
	msm_mux_pbs0,
	msm_mux_pbs1,
	msm_mux_pbs10,
	msm_mux_pbs11,
	msm_mux_pbs12,
	msm_mux_pbs13,
	msm_mux_pbs14,
	msm_mux_pbs15,
	msm_mux_pbs2,
	msm_mux_pbs3,
	msm_mux_pbs4,
	msm_mux_pbs5,
	msm_mux_pbs6,
	msm_mux_pbs7,
	msm_mux_pbs8,
	msm_mux_pbs9,
	msm_mux_pbs_out_0,
	msm_mux_pbs_out_1,
	msm_mux_pbs_out_2,
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
	msm_mux_pll_bypassnl,
	msm_mux_pll_reset_n,
	msm_mux_prng_rosc_test0,
	msm_mux_prng_rosc_test1,
	msm_mux_prng_rosc_test2,
	msm_mux_prng_rosc_test3,
	msm_mux_pwm_0,
	msm_mux_pwm_1,
	msm_mux_pwm_2,
	msm_mux_pwm_3,
	msm_mux_pwm_4,
	msm_mux_pwm_5,
	msm_mux_pwm_6,
	msm_mux_pwm_7,
	msm_mux_pwm_8,
	msm_mux_pwm_9,
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
	msm_mux_qup0_se1_l0,
	msm_mux_qup0_se1_l1,
	msm_mux_qup0_se1_l2,
	msm_mux_qup0_se1_l3,
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
	msm_mux_sd_write_protect,
	msm_mux_tgu_ch0_trigout,
	msm_mux_tgu_ch1_trigout,
	msm_mux_tgu_ch2_trigout,
	msm_mux_tgu_ch3_trigout,
	msm_mux_tsense_pwm_out,
	msm_mux_uim1_clk,
	msm_mux_uim1_data,
	msm_mux_uim1_present,
	msm_mux_uim1_reset,
	msm_mux_uim2_clk,
	msm_mux_uim2_data,
	msm_mux_uim2_present,
	msm_mux_uim2_reset,
	msm_mux_usb0_phy_ps,
	msm_mux_vfr_1,
	msm_mux_vsense_trigger_mirnat,
	msm_mux_wlan1_adc_dtest0,
	msm_mux_wlan1_adc_dtest1,
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
	"gpio64", "gpio69", "gpio70", "gpio71", "gpio72", "gpio73", "gpio74",
	"gpio75", "gpio76", "gpio77", "gpio78", "gpio79", "gpio80", "gpio81",
	"gpio82", "gpio86", "gpio87", "gpio88", "gpio89", "gpio90", "gpio91",
	"gpio94", "gpio95", "gpio96", "gpio97", "gpio98", "gpio99", "gpio100",
	"gpio101", "gpio102", "gpio103", "gpio104", "gpio105", "gpio106",
	"gpio107", "gpio108", "gpio109", "gpio110", "gpio111", "gpio112",
	"gpio113", "gpio114", "gpio115", "gpio116", "gpio117", "gpio118",
	"gpio119", "gpio120", "gpio121", "gpio122", "gpio123", "gpio124",
	"gpio125", "gpio126", "gpio127", "gpio128", "gpio129", "gpio130",
	"gpio131", "gpio132", "gpio133", "gpio134", "gpio135", "gpio136",
	"gpio137", "gpio138", "gpio139",
};
static const char * const AGERA_PLL_REF_groups[] = {
	"gpio40", "gpio41",
};
static const char * const CRI_TRNG_ROSC_groups[] = {
	"gpio10",
};
static const char * const CRI_TRNG_ROSC0_groups[] = {
	"gpio4",
};
static const char * const CRI_TRNG_ROSC1_groups[] = {
	"gpio5",
};
static const char * const GCC_GP1_CLK_groups[] = {
	"gpio24", "gpio86",
};
static const char * const GCC_GP2_CLK_groups[] = {
	"gpio69", "gpio107",
};
static const char * const GCC_GP3_CLK_groups[] = {
	"gpio70", "gpio106",
};
static const char * const GP0_groups[] = {
	"gpio31", "gpio95",
};
static const char * const GP1_groups[] = {
	"gpio32", "gpio96",
};
static const char * const GP2_groups[] = {
	"gpio33", "gpio97",
};
static const char * const JITTER_BIST_REF_groups[] = {
	"gpio96", "gpio97",
};
static const char * const PA_INDICATOR_OR_groups[] = {
	"gpio49",
};
static const char * const PCIE0_CLK_REQ_groups[] = {
	"gpio24",
};
static const char * const PLL_BIST_SYNC_groups[] = {
	"gpio35", "gpio47",
};
static const char * const RGMII0_MDC_groups[] = {
	"gpio136",
};
static const char * const RGMII0_MDIO_groups[] = {
	"gpio135",
};
static const char * const RGMII0_RXC_groups[] = {
	"gpio20",
};
static const char * const RGMII0_RXD0_groups[] = {
	"gpio21",
};
static const char * const RGMII0_RXD1_groups[] = {
	"gpio27",
};
static const char * const RGMII0_RXD2_groups[] = {
	"gpio28",
};
static const char * const RGMII0_RXD3_groups[] = {
	"gpio87",
};
static const char * const RGMII0_RX_CTL_groups[] = {
	"gpio18",
};
static const char * const RGMII0_TXC_groups[] = {
	"gpio91",
};
static const char * const RGMII0_TXD0_groups[] = {
	"gpio131",
};
static const char * const RGMII0_TXD1_groups[] = {
	"gpio132",
};
static const char * const RGMII0_TXD2_groups[] = {
	"gpio133",
};
static const char * const RGMII0_TXD3_groups[] = {
	"gpio134",
};
static const char * const RGMII0_TX_CTL_groups[] = {
	"gpio90",
};
static const char * const SDC1_TB_TRIG_groups[] = {
	"gpio23",
};
static const char * const SDC2_TB_TRIG_groups[] = {
	"gpio22",
};
static const char * const SSBI_WTR1_RX_groups[] = {
	"gpio60",
};
static const char * const SSBI_WTR1_TX_groups[] = {
	"gpio59",
};
static const char * const adsp_ext_vfr_groups[] = {
	"gpio26",
};
static const char * const atest_bbrx_dtestout0_groups[] = {
	"gpio89",
};
static const char * const atest_bbrx_dtestout1_groups[] = {
	"gpio86",
};
static const char * const atest_char_start_groups[] = {
	"gpio29",
};
static const char * const atest_char_status0_groups[] = {
	"gpio30",
};
static const char * const atest_char_status1_groups[] = {
	"gpio31",
};
static const char * const atest_char_status2_groups[] = {
	"gpio32",
};
static const char * const atest_char_status3_groups[] = {
	"gpio33",
};
static const char * const atest_gpsadc0_dtest0_groups[] = {
	"gpio100",
};
static const char * const atest_gpsadc0_dtest1_groups[] = {
	"gpio101",
};
static const char * const atest_gpsadc1_dtest0_groups[] = {
	"gpio53",
};
static const char * const atest_gpsadc1_dtest1_groups[] = {
	"gpio54",
};
static const char * const atest_tsens2_osc_groups[] = {
	"gpio1",
};
static const char * const atest_tsens_osc_groups[] = {
	"gpio0",
};
static const char * const atest_usb1_atereset_groups[] = {
	"gpio6",
};
static const char * const atest_usb1_testdataout00_groups[] = {
	"gpio2",
};
static const char * const atest_usb1_testdataout01_groups[] = {
	"gpio3",
};
static const char * const atest_usb1_testdataout02_groups[] = {
	"gpio4",
};
static const char * const atest_usb1_testdataout03_groups[] = {
	"gpio5",
};
static const char * const atest_usb2_atereset_groups[] = {
	"gpio26",
};
static const char * const atest_usb2_testdataout00_groups[] = {
	"gpio22",
};
static const char * const atest_usb2_testdataout01_groups[] = {
	"gpio23",
};
static const char * const atest_usb2_testdataout02_groups[] = {
	"gpio24",
};
static const char * const atest_usb2_testdataout03_groups[] = {
	"gpio25",
};
static const char * const char_exec_pending_groups[] = {
	"gpio38",
};
static const char * const char_exec_release_groups[] = {
	"gpio37",
};
static const char * const dac_calib_data0_groups[] = {
	"gpio2",
};
static const char * const dac_calib_data1_groups[] = {
	"gpio3",
};
static const char * const dac_calib_data10_groups[] = {
	"gpio23",
};
static const char * const dac_calib_data11_groups[] = {
	"gpio24",
};
static const char * const dac_calib_data12_groups[] = {
	"gpio25",
};
static const char * const dac_calib_data13_groups[] = {
	"gpio26",
};
static const char * const dac_calib_data14_groups[] = {
	"gpio29",
};
static const char * const dac_calib_data15_groups[] = {
	"gpio30",
};
static const char * const dac_calib_data16_groups[] = {
	"gpio31",
};
static const char * const dac_calib_data17_groups[] = {
	"gpio32",
};
static const char * const dac_calib_data18_groups[] = {
	"gpio33",
};
static const char * const dac_calib_data19_groups[] = {
	"gpio42",
};
static const char * const dac_calib_data2_groups[] = {
	"gpio4",
};
static const char * const dac_calib_data20_groups[] = {
	"gpio43",
};
static const char * const dac_calib_data21_groups[] = {
	"gpio11",
};
static const char * const dac_calib_data22_groups[] = {
	"gpio102",
};
static const char * const dac_calib_data23_groups[] = {
	"gpio103",
};
static const char * const dac_calib_data24_groups[] = {
	"gpio104",
};
static const char * const dac_calib_data25_groups[] = {
	"gpio105",
};
static const char * const dac_calib_data3_groups[] = {
	"gpio5",
};
static const char * const dac_calib_data4_groups[] = {
	"gpio6",
};
static const char * const dac_calib_data5_groups[] = {
	"gpio14",
};
static const char * const dac_calib_data6_groups[] = {
	"gpio15",
};
static const char * const dac_calib_data7_groups[] = {
	"gpio16",
};
static const char * const dac_calib_data8_groups[] = {
	"gpio17",
};
static const char * const dac_calib_data9_groups[] = {
	"gpio22",
};
static const char * const dbg_out_clk_groups[] = {
	"gpio71",
};
static const char * const ddr_bist_complete_groups[] = {
	"gpio0",
};
static const char * const ddr_bist_fail_groups[] = {
	"gpio1",
};
static const char * const ddr_bist_start_groups[] = {
	"gpio2",
};
static const char * const ddr_bist_stop_groups[] = {
	"gpio3",
};
static const char * const ddr_pxi0_test_groups[] = {
	"gpio63", "gpio64",
};
static const char * const ddr_pxi1_test_groups[] = {
	"gpio69", "gpio70",
};
static const char * const ddr_pxi2_test_groups[] = {
	"gpio102", "gpio103",
};
static const char * const ddr_pxi3_test_groups[] = {
	"gpio104", "gpio105",
};
static const char * const emac0_dll_sdc4_groups[] = {
	"gpio35", "gpio36",
};
static const char * const emac0_mcg_pst0_groups[] = {
	"gpio138",
};
static const char * const emac0_mcg_pst1_groups[] = {
	"gpio10",
};
static const char * const emac0_mcg_pst2_groups[] = {
	"gpio9",
};
static const char * const emac0_mcg_pst3_groups[] = {
	"gpio8",
};
static const char * const emac0_phy_intr_groups[] = {
	"gpio137",
};
static const char * const emac0_ptp_aux_groups[] = {
	"gpio8", "gpio9", "gpio10", "gpio138",
};
static const char * const emac0_ptp_pps_groups[] = {
	"gpio139", "gpio139", "gpio139", "gpio139",
};
static const char * const gsm0_tx_phase_groups[] = {
	"gpio64",
};
static const char * const gsm1_tx_phase_groups[] = {
	"gpio53",
};
static const char * const m_voc_ext_groups[] = {
	"gpio0",
};
static const char * const mpm_pwr_cllps_groups[] = {
	"gpio1",
};
static const char * const mss_lte_coxm_groups[] = {
	"gpio35", "gpio36",
};
static const char * const nav_gpio0_mira_groups[] = {
	"gpio47",
};
static const char * const nav_gpio0_mirb_groups[] = {
	"gpio98",
};
static const char * const nav_gpio0_mirc_groups[] = {
	"gpio95",
};
static const char * const nav_gpio1_mira_groups[] = {
	"gpio42",
};
static const char * const nav_gpio1_mirb_groups[] = {
	"gpio99",
};
static const char * const nav_gpio1_mirc_groups[] = {
	"gpio96",
};
static const char * const nav_gpio2_mira_groups[] = {
	"gpio52",
};
static const char * const nav_gpio2_mirb_groups[] = {
	"gpio108",
};
static const char * const nav_gpio2_mirc_groups[] = {
	"gpio97",
};
static const char * const pbs0_groups[] = {
	"gpio39",
};
static const char * const pbs1_groups[] = {
	"gpio34",
};
static const char * const pbs10_groups[] = {
	"gpio26",
};
static const char * const pbs11_groups[] = {
	"gpio45",
};
static const char * const pbs12_groups[] = {
	"gpio37",
};
static const char * const pbs13_groups[] = {
	"gpio38",
};
static const char * const pbs14_groups[] = {
	"gpio47",
};
static const char * const pbs15_groups[] = {
	"gpio48",
};
static const char * const pbs2_groups[] = {
	"gpio30",
};
static const char * const pbs3_groups[] = {
	"gpio29",
};
static const char * const pbs4_groups[] = {
	"gpio31",
};
static const char * const pbs5_groups[] = {
	"gpio32",
};
static const char * const pbs6_groups[] = {
	"gpio22",
};
static const char * const pbs7_groups[] = {
	"gpio23",
};
static const char * const pbs8_groups[] = {
	"gpio24",
};
static const char * const pbs9_groups[] = {
	"gpio25",
};
static const char * const pbs_out_0_groups[] = {
	"gpio19",
};
static const char * const pbs_out_1_groups[] = {
	"gpio43",
};
static const char * const pbs_out_2_groups[] = {
	"gpio52",
};
static const char * const phase_flag_status0_groups[] = {
	"gpio0",
};
static const char * const phase_flag_status1_groups[] = {
	"gpio1",
};
static const char * const phase_flag_status10_groups[] = {
	"gpio17",
};
static const char * const phase_flag_status11_groups[] = {
	"gpio22",
};
static const char * const phase_flag_status12_groups[] = {
	"gpio23",
};
static const char * const phase_flag_status13_groups[] = {
	"gpio24",
};
static const char * const phase_flag_status14_groups[] = {
	"gpio25",
};
static const char * const phase_flag_status15_groups[] = {
	"gpio26",
};
static const char * const phase_flag_status16_groups[] = {
	"gpio29",
};
static const char * const phase_flag_status17_groups[] = {
	"gpio30",
};
static const char * const phase_flag_status18_groups[] = {
	"gpio31",
};
static const char * const phase_flag_status19_groups[] = {
	"gpio32",
};
static const char * const phase_flag_status2_groups[] = {
	"gpio2",
};
static const char * const phase_flag_status20_groups[] = {
	"gpio33",
};
static const char * const phase_flag_status21_groups[] = {
	"gpio45",
};
static const char * const phase_flag_status22_groups[] = {
	"gpio46",
};
static const char * const phase_flag_status23_groups[] = {
	"gpio7",
};
static const char * const phase_flag_status24_groups[] = {
	"gpio47",
};
static const char * const phase_flag_status25_groups[] = {
	"gpio81",
};
static const char * const phase_flag_status26_groups[] = {
	"gpio63",
};
static const char * const phase_flag_status27_groups[] = {
	"gpio64",
};
static const char * const phase_flag_status28_groups[] = {
	"gpio102",
};
static const char * const phase_flag_status29_groups[] = {
	"gpio103",
};
static const char * const phase_flag_status3_groups[] = {
	"gpio3",
};
static const char * const phase_flag_status30_groups[] = {
	"gpio104",
};
static const char * const phase_flag_status31_groups[] = {
	"gpio105",
};
static const char * const phase_flag_status4_groups[] = {
	"gpio4",
};
static const char * const phase_flag_status5_groups[] = {
	"gpio5",
};
static const char * const phase_flag_status6_groups[] = {
	"gpio6",
};
static const char * const phase_flag_status7_groups[] = {
	"gpio14",
};
static const char * const phase_flag_status8_groups[] = {
	"gpio15",
};
static const char * const phase_flag_status9_groups[] = {
	"gpio16",
};
static const char * const pll_bypassnl_groups[] = {
	"gpio62",
};
static const char * const pll_reset_n_groups[] = {
	"gpio63",
};
static const char * const prng_rosc_test0_groups[] = {
	"gpio22",
};
static const char * const prng_rosc_test1_groups[] = {
	"gpio23",
};
static const char * const prng_rosc_test2_groups[] = {
	"gpio25",
};
static const char * const prng_rosc_test3_groups[] = {
	"gpio29",
};
static const char * const pwm_0_groups[] = {
	"gpio10",
};
static const char * const pwm_1_groups[] = {
	"gpio29",
};
static const char * const pwm_2_groups[] = {
	"gpio51",
};
static const char * const pwm_3_groups[] = {
	"gpio72",
};
static const char * const pwm_4_groups[] = {
	"gpio74",
};
static const char * const pwm_5_groups[] = {
	"gpio75",
};
static const char * const pwm_6_groups[] = {
	"gpio82",
};
static const char * const pwm_7_groups[] = {
	"gpio89",
};
static const char * const pwm_8_groups[] = {
	"gpio104",
};
static const char * const pwm_9_groups[] = {
	"gpio115",
};
static const char * const qdss_cti_trig0_groups[] = {
	"gpio27", "gpio28", "gpio96", "gpio97",
};
static const char * const qdss_cti_trig1_groups[] = {
	"gpio72", "gpio73", "gpio96", "gpio97",
};
static const char * const qdss_gpio_traceclk_groups[] = {
	"gpio19", "gpio105",
};
static const char * const qdss_gpio_tracectl_groups[] = {
	"gpio33", "gpio106",
};
static const char * const qdss_gpio_tracedata0_groups[] = {
	"gpio45", "gpio107",
};
static const char * const qdss_gpio_tracedata1_groups[] = {
	"gpio37", "gpio104",
};
static const char * const qdss_gpio_tracedata10_groups[] = {
	"gpio2", "gpio26",
};
static const char * const qdss_gpio_tracedata11_groups[] = {
	"gpio3", "gpio31",
};
static const char * const qdss_gpio_tracedata12_groups[] = {
	"gpio38", "gpio86",
};
static const char * const qdss_gpio_tracedata13_groups[] = {
	"gpio39", "gpio69",
};
static const char * const qdss_gpio_tracedata14_groups[] = {
	"gpio47", "gpio94",
};
static const char * const qdss_gpio_tracedata15_groups[] = {
	"gpio48", "gpio70",
};
static const char * const qdss_gpio_tracedata2_groups[] = {
	"gpio43", "gpio112",
};
static const char * const qdss_gpio_tracedata3_groups[] = {
	"gpio32", "gpio113",
};
static const char * const qdss_gpio_tracedata4_groups[] = {
	"gpio4", "gpio29",
};
static const char * const qdss_gpio_tracedata5_groups[] = {
	"gpio5", "gpio30",
};
static const char * const qdss_gpio_tracedata6_groups[] = {
	"gpio6", "gpio22",
};
static const char * const qdss_gpio_tracedata7_groups[] = {
	"gpio7", "gpio23",
};
static const char * const qdss_gpio_tracedata8_groups[] = {
	"gpio0", "gpio24",
};
static const char * const qdss_gpio_tracedata9_groups[] = {
	"gpio1", "gpio25",
};
static const char * const qup0_se0_l0_groups[] = {
	"gpio0",
};
static const char * const qup0_se0_l1_groups[] = {
	"gpio1",
};
static const char * const qup0_se0_l2_groups[] = {
	"gpio2",
};
static const char * const qup0_se0_l3_groups[] = {
	"gpio3",
};
static const char * const qup0_se0_l4_groups[] = {
	"gpio82",
};
static const char * const qup0_se0_l5_groups[] = {
	"gpio86",
};
static const char * const qup0_se1_l0_groups[] = {
	"gpio4",
};
static const char * const qup0_se1_l1_groups[] = {
	"gpio5",
};
static const char * const qup0_se1_l2_groups[] = {
	"gpio69",
};
static const char * const qup0_se1_l3_groups[] = {
	"gpio70",
};
static const char * const qup0_se2_l0_groups[] = {
	"gpio6",
};
static const char * const qup0_se2_l1_groups[] = {
	"gpio7",
};
static const char * const qup0_se2_l2_groups[] = {
	"gpio71",
};
static const char * const qup0_se2_l3_groups[] = {
	"gpio80",
};
static const char * const qup0_se3_l0_groups[] = {
	"gpio8",
};
static const char * const qup0_se3_l1_groups[] = {
	"gpio9",
};
static const char * const qup0_se3_l2_groups[] = {
	"gpio10",
};
static const char * const qup0_se3_l3_groups[] = {
	"gpio11",
};
static const char * const qup0_se4_l0_groups[] = {
	"gpio96",
};
static const char * const qup0_se4_l1_groups[] = {
	"gpio97",
};
static const char * const qup0_se4_l2_groups[] = {
	"gpio12",
};
static const char * const qup0_se4_l3_groups[] = {
	"gpio13",
};
static const char * const qup0_se5_l0_groups[] = {
	"gpio14",
};
static const char * const qup0_se5_l1_groups[] = {
	"gpio15",
};
static const char * const qup0_se5_l2_groups[] = {
	"gpio16",
};
static const char * const qup0_se5_l3_groups[] = {
	"gpio17",
};
static const char * const sd_write_protect_groups[] = {
	"gpio96",
};
static const char * const tgu_ch0_trigout_groups[] = {
	"gpio44",
};
static const char * const tgu_ch1_trigout_groups[] = {
	"gpio45",
};
static const char * const tgu_ch2_trigout_groups[] = {
	"gpio14",
};
static const char * const tgu_ch3_trigout_groups[] = {
	"gpio15",
};
static const char * const tsense_pwm_out_groups[] = {
	"gpio8",
};
static const char * const uim1_clk_groups[] = {
	"gpio77",
};
static const char * const uim1_data_groups[] = {
	"gpio76",
};
static const char * const uim1_present_groups[] = {
	"gpio79",
};
static const char * const uim1_reset_groups[] = {
	"gpio78",
};
static const char * const uim2_clk_groups[] = {
	"gpio73",
};
static const char * const uim2_data_groups[] = {
	"gpio72",
};
static const char * const uim2_present_groups[] = {
	"gpio75",
};
static const char * const uim2_reset_groups[] = {
	"gpio74",
};
static const char * const usb0_phy_ps_groups[] = {
	"gpio89",
};
static const char * const vfr_1_groups[] = {
	"gpio48",
};
static const char * const vsense_trigger_mirnat_groups[] = {
	"gpio26",
};
static const char * const wlan1_adc_dtest0_groups[] = {
	"gpio94",
};
static const char * const wlan1_adc_dtest1_groups[] = {
	"gpio95",
};

static const struct msm_function sa410m_functions[] = {
	FUNCTION(gpio),
	FUNCTION(AGERA_PLL_REF),
	FUNCTION(CRI_TRNG_ROSC),
	FUNCTION(CRI_TRNG_ROSC0),
	FUNCTION(CRI_TRNG_ROSC1),
	FUNCTION(GCC_GP1_CLK),
	FUNCTION(GCC_GP2_CLK),
	FUNCTION(GCC_GP3_CLK),
	FUNCTION(GP0),
	FUNCTION(GP1),
	FUNCTION(GP2),
	FUNCTION(JITTER_BIST_REF),
	FUNCTION(PA_INDICATOR_OR),
	FUNCTION(PCIE0_CLK_REQ),
	FUNCTION(PLL_BIST_SYNC),
	FUNCTION(RGMII0_MDC),
	FUNCTION(RGMII0_MDIO),
	FUNCTION(RGMII0_RXC),
	FUNCTION(RGMII0_RXD0),
	FUNCTION(RGMII0_RXD1),
	FUNCTION(RGMII0_RXD2),
	FUNCTION(RGMII0_RXD3),
	FUNCTION(RGMII0_RX_CTL),
	FUNCTION(RGMII0_TXC),
	FUNCTION(RGMII0_TXD0),
	FUNCTION(RGMII0_TXD1),
	FUNCTION(RGMII0_TXD2),
	FUNCTION(RGMII0_TXD3),
	FUNCTION(RGMII0_TX_CTL),
	FUNCTION(SDC1_TB_TRIG),
	FUNCTION(SDC2_TB_TRIG),
	FUNCTION(SSBI_WTR1_RX),
	FUNCTION(SSBI_WTR1_TX),
	FUNCTION(adsp_ext_vfr),
	FUNCTION(atest_bbrx_dtestout0),
	FUNCTION(atest_bbrx_dtestout1),
	FUNCTION(atest_char_start),
	FUNCTION(atest_char_status0),
	FUNCTION(atest_char_status1),
	FUNCTION(atest_char_status2),
	FUNCTION(atest_char_status3),
	FUNCTION(atest_gpsadc0_dtest0),
	FUNCTION(atest_gpsadc0_dtest1),
	FUNCTION(atest_gpsadc1_dtest0),
	FUNCTION(atest_gpsadc1_dtest1),
	FUNCTION(atest_tsens2_osc),
	FUNCTION(atest_tsens_osc),
	FUNCTION(atest_usb1_atereset),
	FUNCTION(atest_usb1_testdataout00),
	FUNCTION(atest_usb1_testdataout01),
	FUNCTION(atest_usb1_testdataout02),
	FUNCTION(atest_usb1_testdataout03),
	FUNCTION(atest_usb2_atereset),
	FUNCTION(atest_usb2_testdataout00),
	FUNCTION(atest_usb2_testdataout01),
	FUNCTION(atest_usb2_testdataout02),
	FUNCTION(atest_usb2_testdataout03),
	FUNCTION(char_exec_pending),
	FUNCTION(char_exec_release),
	FUNCTION(dac_calib_data0),
	FUNCTION(dac_calib_data1),
	FUNCTION(dac_calib_data10),
	FUNCTION(dac_calib_data11),
	FUNCTION(dac_calib_data12),
	FUNCTION(dac_calib_data13),
	FUNCTION(dac_calib_data14),
	FUNCTION(dac_calib_data15),
	FUNCTION(dac_calib_data16),
	FUNCTION(dac_calib_data17),
	FUNCTION(dac_calib_data18),
	FUNCTION(dac_calib_data19),
	FUNCTION(dac_calib_data2),
	FUNCTION(dac_calib_data20),
	FUNCTION(dac_calib_data21),
	FUNCTION(dac_calib_data22),
	FUNCTION(dac_calib_data23),
	FUNCTION(dac_calib_data24),
	FUNCTION(dac_calib_data25),
	FUNCTION(dac_calib_data3),
	FUNCTION(dac_calib_data4),
	FUNCTION(dac_calib_data5),
	FUNCTION(dac_calib_data6),
	FUNCTION(dac_calib_data7),
	FUNCTION(dac_calib_data8),
	FUNCTION(dac_calib_data9),
	FUNCTION(dbg_out_clk),
	FUNCTION(ddr_bist_complete),
	FUNCTION(ddr_bist_fail),
	FUNCTION(ddr_bist_start),
	FUNCTION(ddr_bist_stop),
	FUNCTION(ddr_pxi0_test),
	FUNCTION(ddr_pxi1_test),
	FUNCTION(ddr_pxi2_test),
	FUNCTION(ddr_pxi3_test),
	FUNCTION(emac0_dll_sdc4),
	FUNCTION(emac0_mcg_pst0),
	FUNCTION(emac0_mcg_pst1),
	FUNCTION(emac0_mcg_pst2),
	FUNCTION(emac0_mcg_pst3),
	FUNCTION(emac0_phy_intr),
	FUNCTION(emac0_ptp_aux),
	FUNCTION(emac0_ptp_pps),
	FUNCTION(gsm0_tx_phase),
	FUNCTION(gsm1_tx_phase),
	FUNCTION(m_voc_ext),
	FUNCTION(mpm_pwr_cllps),
	FUNCTION(mss_lte_coxm),
	FUNCTION(nav_gpio0_mira),
	FUNCTION(nav_gpio0_mirb),
	FUNCTION(nav_gpio0_mirc),
	FUNCTION(nav_gpio1_mira),
	FUNCTION(nav_gpio1_mirb),
	FUNCTION(nav_gpio1_mirc),
	FUNCTION(nav_gpio2_mira),
	FUNCTION(nav_gpio2_mirb),
	FUNCTION(nav_gpio2_mirc),
	FUNCTION(pbs0),
	FUNCTION(pbs1),
	FUNCTION(pbs10),
	FUNCTION(pbs11),
	FUNCTION(pbs12),
	FUNCTION(pbs13),
	FUNCTION(pbs14),
	FUNCTION(pbs15),
	FUNCTION(pbs2),
	FUNCTION(pbs3),
	FUNCTION(pbs4),
	FUNCTION(pbs5),
	FUNCTION(pbs6),
	FUNCTION(pbs7),
	FUNCTION(pbs8),
	FUNCTION(pbs9),
	FUNCTION(pbs_out_0),
	FUNCTION(pbs_out_1),
	FUNCTION(pbs_out_2),
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
	FUNCTION(pll_bypassnl),
	FUNCTION(pll_reset_n),
	FUNCTION(prng_rosc_test0),
	FUNCTION(prng_rosc_test1),
	FUNCTION(prng_rosc_test2),
	FUNCTION(prng_rosc_test3),
	FUNCTION(pwm_0),
	FUNCTION(pwm_1),
	FUNCTION(pwm_2),
	FUNCTION(pwm_3),
	FUNCTION(pwm_4),
	FUNCTION(pwm_5),
	FUNCTION(pwm_6),
	FUNCTION(pwm_7),
	FUNCTION(pwm_8),
	FUNCTION(pwm_9),
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
	FUNCTION(qup0_se1_l0),
	FUNCTION(qup0_se1_l1),
	FUNCTION(qup0_se1_l2),
	FUNCTION(qup0_se1_l3),
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
	FUNCTION(sd_write_protect),
	FUNCTION(tgu_ch0_trigout),
	FUNCTION(tgu_ch1_trigout),
	FUNCTION(tgu_ch2_trigout),
	FUNCTION(tgu_ch3_trigout),
	FUNCTION(tsense_pwm_out),
	FUNCTION(uim1_clk),
	FUNCTION(uim1_data),
	FUNCTION(uim1_present),
	FUNCTION(uim1_reset),
	FUNCTION(uim2_clk),
	FUNCTION(uim2_data),
	FUNCTION(uim2_present),
	FUNCTION(uim2_reset),
	FUNCTION(usb0_phy_ps),
	FUNCTION(vfr_1),
	FUNCTION(vsense_trigger_mirnat),
	FUNCTION(wlan1_adc_dtest0),
	FUNCTION(wlan1_adc_dtest1),
};

/* Every pin is maintained as a single group, and missing or non-existing pin
 * would be maintained as dummy group to synchronize pin group index with
 * pin descriptor registered with pinctrl core.
 * Clients would not be able to request these dummy pin groups.
 */
static const struct msm_pingroup sa410m_groups[] = {
	[0] = PINGROUP(0, qup0_se0_l0, m_voc_ext, ddr_bist_complete, NA,
		       phase_flag_status0, qdss_gpio_tracedata8,
		       atest_tsens_osc, NA, NA, 0x7F000, 0),
	[1] = PINGROUP(1, qup0_se0_l1, mpm_pwr_cllps, ddr_bist_fail, NA,
		       phase_flag_status1, qdss_gpio_tracedata9,
		       atest_tsens2_osc, NA, NA, 0, -1),
	[2] = PINGROUP(2, qup0_se0_l2, ddr_bist_start, NA, phase_flag_status2,
		       qdss_gpio_tracedata10, dac_calib_data0,
		       atest_usb1_testdataout00, NA, NA, 0, -1),
	[3] = PINGROUP(3, qup0_se0_l3, ddr_bist_stop, NA, phase_flag_status3,
		       qdss_gpio_tracedata11, dac_calib_data1,
		       atest_usb1_testdataout01, NA, NA, 0x7F000, 1),
	[4] = PINGROUP(4, qup0_se1_l0, CRI_TRNG_ROSC0, NA, phase_flag_status4,
		       qdss_gpio_tracedata4, dac_calib_data2,
		       atest_usb1_testdataout02, NA, NA, 0x7F000, 2),
	[5] = PINGROUP(5, qup0_se1_l1, CRI_TRNG_ROSC1, NA, phase_flag_status5,
		       qdss_gpio_tracedata5, dac_calib_data3,
		       atest_usb1_testdataout03, NA, NA, 0, -1),
	[6] = PINGROUP(6, qup0_se2_l0, NA, phase_flag_status6,
		       qdss_gpio_tracedata6, dac_calib_data4,
		       atest_usb1_atereset, NA, NA, NA, 0x7F000, 3),
	[7] = PINGROUP(7, qup0_se2_l1, NA, phase_flag_status23,
		       qdss_gpio_tracedata7, NA, NA, NA, NA, NA, 0, -1),
	[8] = PINGROUP(8, qup0_se3_l0, emac0_ptp_aux, emac0_mcg_pst3,
		       tsense_pwm_out, NA, NA, NA, NA, NA, 0x7F008, 0),
	[9] = PINGROUP(9, qup0_se3_l1, emac0_ptp_aux, emac0_mcg_pst2, NA, NA,
		       NA, NA, NA, NA, 0, -1),
	[10] = PINGROUP(10, qup0_se3_l2, emac0_ptp_aux, emac0_mcg_pst1, pwm_0,
			CRI_TRNG_ROSC, NA, NA, NA, NA, 0, -1),
	[11] = PINGROUP(11, qup0_se3_l3, dac_calib_data21, NA, NA, NA, NA, NA,
			NA, NA, 0x7F008, 1),
	[12] = PINGROUP(12, qup0_se4_l2, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[13] = PINGROUP(13, qup0_se4_l3, NA, NA, NA, NA, NA, NA, NA, NA, 0x7F000, 4),
	[14] = PINGROUP(14, qup0_se5_l0, tgu_ch2_trigout, NA,
			phase_flag_status7, dac_calib_data5, NA, NA, NA, NA, 0x7F000, 5),
	[15] = PINGROUP(15, qup0_se5_l1, tgu_ch3_trigout, NA,
			phase_flag_status8, dac_calib_data6, NA, NA, NA, NA, 0, -1),
	[16] = PINGROUP(16, qup0_se5_l2, NA, phase_flag_status9,
			dac_calib_data7, NA, NA, NA, NA, NA, 0, -1),
	[17] = PINGROUP(17, qup0_se5_l3, NA, phase_flag_status10,
			dac_calib_data8, NA, NA, NA, NA, NA, 0x7F000, 6),
	[18] = PINGROUP(18, RGMII0_RX_CTL, NA, NA, NA, NA, NA, NA, NA, NA, 0x7F008, 2),
	[19] = PINGROUP(19, pbs_out_0, qdss_gpio_traceclk, NA, NA, NA, NA, NA,
			NA, NA, 0x7F008, 3),
	[20] = PINGROUP(20, RGMII0_RXC, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[21] = PINGROUP(21, RGMII0_RXD0, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[22] = PINGROUP(22, SDC2_TB_TRIG, prng_rosc_test0, NA, pbs6,
			phase_flag_status11, qdss_gpio_tracedata6,
			dac_calib_data9, atest_usb2_testdataout00, NA, 0, -1),
	[23] = PINGROUP(23, SDC1_TB_TRIG, prng_rosc_test1, NA, pbs7,
			phase_flag_status12, qdss_gpio_tracedata7,
			dac_calib_data10, atest_usb2_testdataout01, NA, 0, -1),
	[24] = PINGROUP(24, PCIE0_CLK_REQ, GCC_GP1_CLK, NA, pbs8,
			phase_flag_status13, qdss_gpio_tracedata8,
			dac_calib_data11, atest_usb2_testdataout02, NA, 0x7F008, 4),
	[25] = PINGROUP(25, prng_rosc_test2, NA, pbs9, phase_flag_status14,
			qdss_gpio_tracedata9, dac_calib_data12,
			atest_usb2_testdataout03, NA, NA, 0x7F008, 5),
	[26] = PINGROUP(26, adsp_ext_vfr, NA, pbs10, phase_flag_status15,
			qdss_gpio_tracedata10, dac_calib_data13,
			atest_usb2_atereset, vsense_trigger_mirnat, NA, 0, -1),
	[27] = PINGROUP(27, RGMII0_RXD1, qdss_cti_trig0, NA, NA, NA, NA, NA,
			NA, NA, 0x7F008, 6),
	[28] = PINGROUP(28, RGMII0_RXD2, qdss_cti_trig0, NA, NA, NA, NA, NA,
			NA, NA, 0x7F008, 7),
	[29] = PINGROUP(29, pwm_1, prng_rosc_test3, NA, pbs3,
			phase_flag_status16, qdss_gpio_tracedata4,
			dac_calib_data14, atest_char_start, NA, 0, -1),
	[30] = PINGROUP(30, NA, pbs2, phase_flag_status17,
			qdss_gpio_tracedata5, dac_calib_data15,
			atest_char_status0, NA, NA, NA, 0, -1),
	[31] = PINGROUP(31, GP0, NA, pbs4, phase_flag_status18,
			qdss_gpio_tracedata11, dac_calib_data16,
			atest_char_status1, NA, NA, 0x7F008, 8),
	[32] = PINGROUP(32, GP1, NA, pbs5, phase_flag_status19,
			qdss_gpio_tracedata3, dac_calib_data17,
			atest_char_status2, NA, NA, 0x7F008, 9),
	[33] = PINGROUP(33, GP2, NA, phase_flag_status20, qdss_gpio_tracectl,
			dac_calib_data18, atest_char_status3, NA, NA, NA, 0x7F008, 10),
	[34] = PINGROUP(34, pbs1, NA, NA, NA, NA, NA, NA, NA, NA, 0x7F008, 11),
	[35] = PINGROUP(35, mss_lte_coxm, PLL_BIST_SYNC, NA, NA,
			emac0_dll_sdc4, NA, NA, NA, NA, 0x7F008, 12),
	[36] = PINGROUP(36, mss_lte_coxm, NA, NA, emac0_dll_sdc4, NA, NA, NA,
			NA, NA, 0x7F008, 13),
	[37] = PINGROUP(37, NA, char_exec_release, pbs12, qdss_gpio_tracedata1,
			NA, NA, NA, NA, NA, 0, -1),
	[38] = PINGROUP(38, NA, char_exec_pending, pbs13,
			qdss_gpio_tracedata12, NA, NA, NA, NA, NA, 0, -1),
	[39] = PINGROUP(39, NA, pbs0, qdss_gpio_tracedata13, NA, NA, NA, NA,
			NA, NA, 0x7F008, 14),
	[40] = PINGROUP(40, NA, AGERA_PLL_REF, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[41] = PINGROUP(41, NA, AGERA_PLL_REF, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[42] = PINGROUP(42, NA, nav_gpio1_mira, dac_calib_data19, NA, NA, NA,
			NA, NA, NA, 0, -1),
	[43] = PINGROUP(43, NA, pbs_out_1, qdss_gpio_tracedata2,
			dac_calib_data20, NA, NA, NA, NA, NA, 0, -1),
	[44] = PINGROUP(44, NA, tgu_ch0_trigout, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[45] = PINGROUP(45, NA, tgu_ch1_trigout, NA, pbs11,
			phase_flag_status21, qdss_gpio_tracedata0, NA, NA, NA, 0, -1),
	[46] = PINGROUP(46, NA, NA, phase_flag_status22, NA, NA, NA, NA, NA,
			NA, 0x7F008, 15),
	[47] = PINGROUP(47, NA, nav_gpio0_mira, PLL_BIST_SYNC, NA, pbs14,
			phase_flag_status24, qdss_gpio_tracedata14, NA, NA, 0, -1),
	[48] = PINGROUP(48, NA, vfr_1, NA, pbs15, qdss_gpio_tracedata15, NA,
			NA, NA, NA, 0, -1),
	[49] = PINGROUP(49, NA, PA_INDICATOR_OR, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[50] = PINGROUP(50, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[51] = PINGROUP(51, NA, pwm_2, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[52] = PINGROUP(52, NA, nav_gpio2_mira, pbs_out_2, NA, NA, NA, NA, NA,
			NA, 0, -1),
	[53] = PINGROUP(53, NA, gsm1_tx_phase, NA, atest_gpsadc1_dtest0, NA,
			NA, NA, NA, NA, 0, -1),
	[54] = PINGROUP(54, NA, NA, atest_gpsadc1_dtest1, NA, NA, NA, NA, NA,
			NA, 0, -1),
	[55] = PINGROUP(55, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[56] = PINGROUP(56, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[57] = PINGROUP(57, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[58] = PINGROUP(58, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[59] = PINGROUP(59, SSBI_WTR1_TX, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[60] = PINGROUP(60, SSBI_WTR1_RX, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[61] = PINGROUP(61, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[62] = PINGROUP(62, NA, pll_bypassnl, NA, NA, NA, NA, NA, NA, NA, 0x7F00C, 0),
	[63] = PINGROUP(63, pll_reset_n, NA, phase_flag_status26,
			ddr_pxi0_test, NA, NA, NA, NA, NA, 0x7F00C, 1),
	[64] = PINGROUP(64, gsm0_tx_phase, NA, phase_flag_status27,
			ddr_pxi0_test, NA, NA, NA, NA, NA, 0x7F00C, 2),
	[65] = PINGROUP(65, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[66] = PINGROUP(66, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[67] = PINGROUP(67, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[68] = PINGROUP(68, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[69] = PINGROUP(69, qup0_se1_l2, GCC_GP2_CLK, qdss_gpio_tracedata13,
			ddr_pxi1_test, NA, NA, NA, NA, NA, 0x7F000, 10),
	[70] = PINGROUP(70, qup0_se1_l3, GCC_GP3_CLK, qdss_gpio_tracedata15,
			ddr_pxi1_test, NA, NA, NA, NA, NA, 0x7F000, 11),
	[71] = PINGROUP(71, qup0_se2_l2, dbg_out_clk, NA, NA, NA, NA, NA, NA,
			NA, 0, -1),
	[72] = PINGROUP(72, uim2_data, pwm_3, qdss_cti_trig1, NA, NA, NA, NA,
			NA, NA, 0x7F010, 3),
	[73] = PINGROUP(73, uim2_clk, NA, qdss_cti_trig1, NA, NA, NA, NA, NA,
			NA, 0, -1),
	[74] = PINGROUP(74, uim2_reset, pwm_4, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[75] = PINGROUP(75, uim2_present, pwm_5, NA, NA, NA, NA, NA, NA, NA, 0x7F010, 4),
	[76] = PINGROUP(76, uim1_data, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[77] = PINGROUP(77, uim1_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[78] = PINGROUP(78, uim1_reset, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[79] = PINGROUP(79, uim1_present, NA, NA, NA, NA, NA, NA, NA, NA, 0x7F010, 5),
	[80] = PINGROUP(80, qup0_se2_l3, NA, NA, NA, NA, NA, NA, NA, NA, 0x7F000, 12),
	[81] = PINGROUP(81, NA, phase_flag_status25, NA, NA, NA, NA, NA, NA,
			NA, 0x7F000, 13),
	[82] = PINGROUP(82, qup0_se0_l4, pwm_6, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[83] = PINGROUP(83, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[84] = PINGROUP(84, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[85] = PINGROUP(85, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[86] = PINGROUP(86, qup0_se0_l5, GCC_GP1_CLK, NA,
			qdss_gpio_tracedata12, atest_bbrx_dtestout1, NA, NA,
			NA, NA, 0x7F004, 1),
	[87] = PINGROUP(87, RGMII0_RXD3, NA, NA, NA, NA, NA, NA, NA, NA, 0x7F00C, 3),
	[88] = PINGROUP(88, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x7F00C, 4),
	[89] = PINGROUP(89, usb0_phy_ps, pwm_7, NA, atest_bbrx_dtestout0, NA,
			NA, NA, NA, NA, 0x7F004, 2),
	[90] = PINGROUP(90, RGMII0_TX_CTL, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[91] = PINGROUP(91, RGMII0_TXC, NA, NA, NA, NA, NA, NA, NA, NA, 0x7F00C, 5),
	[92] = PINGROUP(92, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[93] = PINGROUP(93, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[94] = PINGROUP(94, NA, qdss_gpio_tracedata14, wlan1_adc_dtest0, NA,
			NA, NA, NA, NA, NA, 0x7F004, 4),
	[95] = PINGROUP(95, nav_gpio0_mirc, GP0, NA, wlan1_adc_dtest1, NA, NA,
			NA, NA, NA, 0x7F004, 5),
	[96] = PINGROUP(96, qup0_se4_l0, nav_gpio1_mirc, GP1, sd_write_protect,
			JITTER_BIST_REF, qdss_cti_trig0, qdss_cti_trig1, NA,
			NA, 0x7F004, 6),
	[97] = PINGROUP(97, qup0_se4_l1, nav_gpio2_mirc, GP2, JITTER_BIST_REF,
			qdss_cti_trig0, qdss_cti_trig1, NA, NA, NA, 0x7F004, 7),
	[98] = PINGROUP(98, nav_gpio0_mirb, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[99] = PINGROUP(99, nav_gpio1_mirb, NA, NA, NA, NA, NA, NA, NA, NA, 0x7F010, 6),
	[100] = PINGROUP(100, atest_gpsadc0_dtest0, NA, NA, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[101] = PINGROUP(101, atest_gpsadc0_dtest1, NA, NA, NA, NA, NA, NA, NA,
			 NA, 0x7F010, 7),
	[102] = PINGROUP(102, NA, phase_flag_status28, dac_calib_data22,
			 ddr_pxi2_test, NA, NA, NA, NA, NA, 0x7F010, 8),
	[103] = PINGROUP(103, NA, phase_flag_status29, dac_calib_data23,
			 ddr_pxi2_test, NA, NA, NA, NA, NA, 0x7F010, 9),
	[104] = PINGROUP(104, pwm_8, NA, phase_flag_status30,
			 qdss_gpio_tracedata1, dac_calib_data24, ddr_pxi3_test,
			 NA, NA, NA, 0x7F010, 10),
	[105] = PINGROUP(105, NA, phase_flag_status31, qdss_gpio_traceclk,
			 dac_calib_data25, ddr_pxi3_test, NA, NA, NA, NA, 0x7F010, 11),
	[106] = PINGROUP(106, GCC_GP3_CLK, qdss_gpio_tracectl, NA, NA, NA, NA,
			 NA, NA, NA, 0x7F010, 12),
	[107] = PINGROUP(107, GCC_GP2_CLK, qdss_gpio_tracedata0, NA, NA, NA,
			 NA, NA, NA, NA, 0x7F010, 13),
	[108] = PINGROUP(108, nav_gpio2_mirb, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[109] = PINGROUP(109, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x7F010, 14),
	[110] = PINGROUP(110, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[111] = PINGROUP(111, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[112] = PINGROUP(112, NA, qdss_gpio_tracedata2, NA, NA, NA, NA, NA, NA,
			 NA, 0x7F010, 15),
	[113] = PINGROUP(113, qdss_gpio_tracedata3, NA, NA, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[114] = PINGROUP(114, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[115] = PINGROUP(115, pwm_9, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[116] = PINGROUP(116, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[117] = PINGROUP(117, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[118] = PINGROUP(118, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[119] = PINGROUP(119, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[120] = PINGROUP(120, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x7F004, 8),
	[121] = PINGROUP(121, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[122] = PINGROUP(122, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x7F004, 9),
	[123] = PINGROUP(123, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[124] = PINGROUP(124, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[125] = PINGROUP(125, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[126] = PINGROUP(126, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[127] = PINGROUP1(127, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[128] = PINGROUP1(128, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[129] = PINGROUP1(129, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[130] = PINGROUP1(130, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[131] = PINGROUP1(131, RGMII0_TXD0, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[132] = PINGROUP1(132, RGMII0_TXD1, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[133] = PINGROUP1(133, RGMII0_TXD2, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[134] = PINGROUP1(134, RGMII0_TXD3, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[135] = PINGROUP1(135, RGMII0_MDIO, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[136] = PINGROUP1(136, RGMII0_MDC, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[137] = PINGROUP1(137, emac0_phy_intr, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[138] = PINGROUP1(138, emac0_ptp_aux, emac0_mcg_pst0, NA, NA, NA, NA,
			 NA, NA, NA, 0, -1),
	[139] = PINGROUP1(139, emac0_ptp_pps, emac0_ptp_pps, emac0_ptp_pps,
			 emac0_ptp_pps, NA, NA, NA, NA, NA, 0, -1),
	[140] = SDC_QDSD_PINGROUP(sdc1_rclk, 0x399a000, 15, 0),
	[141] = SDC_QDSD_PINGROUP(sdc1_clk, 0x399a000, 13, 6),
	[142] = SDC_QDSD_PINGROUP(sdc1_cmd, 0x399a000, 11, 3),
	[143] = SDC_QDSD_PINGROUP(sdc1_data, 0x399a000, 9, 0),
	[144] = SDC_QDSD_PINGROUP(sdc2_clk, 0x186000, 14, 6),
	[145] = SDC_QDSD_PINGROUP(sdc2_cmd, 0x186000, 11, 3),
	[146] = SDC_QDSD_PINGROUP(sdc2_data, 0x186000, 9, 0),
};
static struct pinctrl_qup sa410m_qup_regs[] = {
};

static const struct msm_gpio_wakeirq_map sa410m_mpm_map[] = {
	{0, 84},
	{3, 75},
	{4, 16},
	{6, 59},
	{8, 63},
	{11, 17},
	{13, 18},
	{14, 51},
	{17, 20},
	{18, 52},
	{19, 53},
	{24, 6},
	{25, 71},
	{27, 73},
	{28, 41},
	{31, 27},
	{32, 54},
	{33, 55},
	{34, 56},
	{35, 57},
	{36, 58},
	{39, 28},
	{46, 29},
	{62, 60},
	{63, 61},
	{64, 62},
	{69, 33},
	{70, 34},
	{72, 72},
	{75, 35},
	{79, 36},
	{80, 21},
	{81, 38},
	{86, 19},
	{87, 42},
	{88, 43},
	{89, 45},
	{91, 74},
	{94, 47},
	{95, 48},
	{96, 49},
	{97, 50},
};

static const struct msm_pinctrl_soc_data sa410m_pinctrl = {
	.pins = sa410m_pins,
	.npins = ARRAY_SIZE(sa410m_pins),
	.functions = sa410m_functions,
	.nfunctions = ARRAY_SIZE(sa410m_functions),
	.groups = sa410m_groups,
	.ngroups = ARRAY_SIZE(sa410m_groups),
	.ngpios = 140,
	.tiles = sa410m_tiles,
	.ntiles = ARRAY_SIZE(sa410m_tiles),
	.qup_regs = sa410m_qup_regs,
	.nqup_regs = ARRAY_SIZE(sa410m_qup_regs),
	.wakeirq_map = sa410m_mpm_map,
	.nwakeirq_map = ARRAY_SIZE(sa410m_mpm_map),
};

static int sa410m_pinctrl_probe(struct platform_device *pdev)
{
	return msm_pinctrl_probe(pdev, &sa410m_pinctrl);
}

static const struct of_device_id sa410m_pinctrl_of_match[] = {
	{ .compatible = "qcom,sa410m-pinctrl", },
	{ },
};

static struct platform_driver sa410m_pinctrl_driver = {
	.driver = {
		.name = "sa410m-pinctrl",
		.of_match_table = sa410m_pinctrl_of_match,
	},
	.probe = sa410m_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init sa410m_pinctrl_init(void)
{
	return platform_driver_register(&sa410m_pinctrl_driver);
}
arch_initcall(sa410m_pinctrl_init);

static void __exit sa410m_pinctrl_exit(void)
{
	platform_driver_unregister(&sa410m_pinctrl_driver);
}
module_exit(sa410m_pinctrl_exit);

MODULE_DESCRIPTION("QTI sa410m pinctrl driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, sa410m_pinctrl_of_match);
