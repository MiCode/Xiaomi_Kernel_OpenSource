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

#define REG_BASE 0x0
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

#define QUP_I3C(qup_mode, qup_offset)                  \
	{                                               \
		.mode = qup_mode,                       \
		.offset = qup_offset,                   \
	}


#define QUP_I3C_5_MODE_OFFSET	0x86000
#define QUP_I3C_6_MODE_OFFSET	0x87000

static const struct pinctrl_pin_desc monaco_pins[] = {
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
	PINCTRL_PIN(112, "SDC1_RCLK"),
	PINCTRL_PIN(113, "SDC1_CLK"),
	PINCTRL_PIN(114, "SDC1_CMD"),
	PINCTRL_PIN(115, "SDC1_DATA"),
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

static const unsigned int sdc1_rclk_pins[] = { 112 };
static const unsigned int sdc1_clk_pins[] = { 113 };
static const unsigned int sdc1_cmd_pins[] = { 114 };
static const unsigned int sdc1_data_pins[] = { 115 };

enum monaco_functions {
	msm_mux_gpio,
	msm_mux_AGERA_PLL,
	msm_mux_CCI_TIMER0,
	msm_mux_CCI_TIMER1,
	msm_mux_CCI_TIMER2,
	msm_mux_CCI_TIMER3,
	msm_mux_CRI_TRNG,
	msm_mux_CRI_TRNG0,
	msm_mux_CRI_TRNG1,
	msm_mux_GCC_GP1,
	msm_mux_GCC_GP2,
	msm_mux_GCC_GP3,
	msm_mux_GP_PDM0,
	msm_mux_GP_PDM1,
	msm_mux_GP_PDM2,
	msm_mux_JITTER_BIST,
	msm_mux_PA_INDICATOR,
	msm_mux_PLL_BIST,
	msm_mux_QUP0_L0,
	msm_mux_QUP0_L1,
	msm_mux_QUP0_L2,
	msm_mux_QUP0_L3,
	msm_mux_SDC1_TB,
	msm_mux_SDC2_TB,
	msm_mux_SSBI_WTR1,
	msm_mux_adsp_ext,
	msm_mux_atest_bbrx0,
	msm_mux_atest_bbrx1,
	msm_mux_atest_char,
	msm_mux_atest_char0,
	msm_mux_atest_char1,
	msm_mux_atest_char2,
	msm_mux_atest_char3,
	msm_mux_atest_gpsadc_dtest0_native,
	msm_mux_atest_gpsadc_dtest1_native,
	msm_mux_atest_tsens,
	msm_mux_atest_tsens2,
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
	msm_mux_cam_mclk,
	msm_mux_cci_async,
	msm_mux_cci_i2c,
	msm_mux_dac_calib0,
	msm_mux_dac_calib1,
	msm_mux_dac_calib10,
	msm_mux_dac_calib11,
	msm_mux_dac_calib12,
	msm_mux_dac_calib13,
	msm_mux_dac_calib14,
	msm_mux_dac_calib15,
	msm_mux_dac_calib16,
	msm_mux_dac_calib17,
	msm_mux_dac_calib18,
	msm_mux_dac_calib19,
	msm_mux_dac_calib2,
	msm_mux_dac_calib20,
	msm_mux_dac_calib21,
	msm_mux_dac_calib22,
	msm_mux_dac_calib23,
	msm_mux_dac_calib24,
	msm_mux_dac_calib25,
	msm_mux_dac_calib3,
	msm_mux_dac_calib4,
	msm_mux_dac_calib5,
	msm_mux_dac_calib6,
	msm_mux_dac_calib7,
	msm_mux_dac_calib8,
	msm_mux_dac_calib9,
	msm_mux_dbg_out,
	msm_mux_ddr_bist,
	msm_mux_ddr_pxi0,
	msm_mux_ddr_pxi1,
	msm_mux_ddr_pxi2,
	msm_mux_ddr_pxi3,
	msm_mux_gsm0_tx,
	msm_mux_gsm1_tx,
	msm_mux_m_voc,
	msm_mux_mdp_vsync,
	msm_mux_mpm_pwr,
	msm_mux_nav_gpio0,
	msm_mux_nav_gpio1,
	msm_mux_nav_gpio2,
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
	msm_mux_pbs_out,
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
	msm_mux_pll_bypassnl,
	msm_mux_pll_clk,
	msm_mux_pll_reset,
	msm_mux_prng_rosc0,
	msm_mux_prng_rosc1,
	msm_mux_prng_rosc2,
	msm_mux_prng_rosc3,
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
	msm_mux_qup00,
	msm_mux_qup01,
	msm_mux_qup02,
	msm_mux_qup03,
	msm_mux_qup04,
	msm_mux_qup05,
	msm_mux_qup06,
	msm_mux_sdc3_clk,
	msm_mux_sdc3_cmd,
	msm_mux_sdc3_data,
	msm_mux_tgu_ch0,
	msm_mux_tgu_ch1,
	msm_mux_tgu_ch2,
	msm_mux_tgu_ch3,
	msm_mux_tsense_pwm,
	msm_mux_uim0_clk,
	msm_mux_uim0_data,
	msm_mux_uim0_present,
	msm_mux_uim0_reset,
	msm_mux_usb2phy_ac,
	msm_mux_vfr_1,
	msm_mux_vsense_trigger,
	msm_mux_wci_uart,
	msm_mux_wlan1_adc0,
	msm_mux_wlan1_adc1,
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
	"gpio111",
};
static const char * const AGERA_PLL_groups[] = {
	"gpio10", "gpio11",
};
static const char * const CCI_TIMER0_groups[] = {
	"gpio7",
};
static const char * const CCI_TIMER1_groups[] = {
	"gpio36",
};
static const char * const CCI_TIMER2_groups[] = {
	"gpio34",
};
static const char * const CCI_TIMER3_groups[] = {
	"gpio41",
};
static const char * const CRI_TRNG_groups[] = {
	"gpio35",
};
static const char * const CRI_TRNG0_groups[] = {
	"gpio4",
};
static const char * const CRI_TRNG1_groups[] = {
	"gpio5",
};
static const char * const GCC_GP1_groups[] = {
	"gpio8", "gpio36",
};
static const char * const GCC_GP2_groups[] = {
	"gpio6", "gpio64",
};
static const char * const GCC_GP3_groups[] = {
	"gpio7", "gpio65",
};
static const char * const GP_PDM0_groups[] = {
	"gpio9", "gpio76",
};
static const char * const GP_PDM1_groups[] = {
	"gpio13", "gpio77",
};
static const char * const GP_PDM2_groups[] = {
	"gpio19", "gpio33",
};
static const char * const JITTER_BIST_groups[] = {
	"gpio81", "gpio82",
};
static const char * const PA_INDICATOR_groups[] = {
	"gpio55",
};
static const char * const PLL_BIST_groups[] = {
	"gpio14", "gpio15",
};
static const char * const QUP0_L0_groups[] = {
	"gpio101", "gpio104",
};
static const char * const QUP0_L1_groups[] = {
	"gpio102", "gpio105",
};
static const char * const QUP0_L2_groups[] = {
	"gpio101", "gpio104",
};
static const char * const QUP0_L3_groups[] = {
	"gpio102", "gpio105",
};
static const char * const SDC1_TB_groups[] = {
	"gpio36",
};
static const char * const SDC2_TB_groups[] = {
	"gpio35",
};
static const char * const SSBI_WTR1_groups[] = {
	"gpio63", "gpio64",
};
static const char * const adsp_ext_groups[] = {
	"gpio33",
};
static const char * const atest_bbrx0_groups[] = {
	"gpio84",
};
static const char * const atest_bbrx1_groups[] = {
	"gpio83",
};
static const char * const atest_char_groups[] = {
	"gpio43",
};
static const char * const atest_char0_groups[] = {
	"gpio44",
};
static const char * const atest_char1_groups[] = {
	"gpio45",
};
static const char * const atest_char2_groups[] = {
	"gpio46",
};
static const char * const atest_char3_groups[] = {
	"gpio50",
};
static const char * const atest_gpsadc_dtest0_native_groups[] = {
	"gpio81",
};
static const char * const atest_gpsadc_dtest1_native_groups[] = {
	"gpio82",
};
static const char * const atest_tsens_groups[] = {
	"gpio4",
};
static const char * const atest_tsens2_groups[] = {
	"gpio5",
};
static const char * const atest_usb1_groups[] = {
	"gpio9",
};
static const char * const atest_usb10_groups[] = {
	"gpio0",
};
static const char * const atest_usb11_groups[] = {
	"gpio1",
};
static const char * const atest_usb12_groups[] = {
	"gpio2",
};
static const char * const atest_usb13_groups[] = {
	"gpio8",
};
static const char * const atest_usb2_groups[] = {
	"gpio24",
};
static const char * const atest_usb20_groups[] = {
	"gpio20",
};
static const char * const atest_usb21_groups[] = {
	"gpio21",
};
static const char * const atest_usb22_groups[] = {
	"gpio22",
};
static const char * const atest_usb23_groups[] = {
	"gpio23",
};
static const char * const cam_mclk_groups[] = {
	"gpio32", "gpio33", "gpio34",
};
static const char * const cci_async_groups[] = {
	"gpio7",
};
static const char * const cci_i2c_groups[] = {
	"gpio37", "gpio38", "gpio39", "gpio40",
};
static const char * const dac_calib0_groups[] = {
	"gpio0",
};
static const char * const dac_calib1_groups[] = {
	"gpio35",
};
static const char * const dac_calib10_groups[] = {
	"gpio20",
};
static const char * const dac_calib11_groups[] = {
	"gpio21",
};
static const char * const dac_calib12_groups[] = {
	"gpio22",
};
static const char * const dac_calib13_groups[] = {
	"gpio23",
};
static const char * const dac_calib14_groups[] = {
	"gpio37",
};
static const char * const dac_calib15_groups[] = {
	"gpio25",
};
static const char * const dac_calib16_groups[] = {
	"gpio26",
};
static const char * const dac_calib17_groups[] = {
	"gpio27",
};
static const char * const dac_calib18_groups[] = {
	"gpio28",
};
static const char * const dac_calib19_groups[] = {
	"gpio29",
};
static const char * const dac_calib2_groups[] = {
	"gpio2",
};
static const char * const dac_calib20_groups[] = {
	"gpio30",
};
static const char * const dac_calib21_groups[] = {
	"gpio31",
};
static const char * const dac_calib22_groups[] = {
	"gpio80",
};
static const char * const dac_calib23_groups[] = {
	"gpio41",
};
static const char * const dac_calib24_groups[] = {
	"gpio42",
};
static const char * const dac_calib25_groups[] = {
	"gpio43",
};
static const char * const dac_calib3_groups[] = {
	"gpio3",
};
static const char * const dac_calib4_groups[] = {
	"gpio10",
};
static const char * const dac_calib5_groups[] = {
	"gpio13",
};
static const char * const dac_calib6_groups[] = {
	"gpio40",
};
static const char * const dac_calib7_groups[] = {
	"gpio16",
};
static const char * const dac_calib8_groups[] = {
	"gpio17",
};
static const char * const dac_calib9_groups[] = {
	"gpio33",
};
static const char * const dbg_out_groups[] = {
	"gpio81",
};
static const char * const ddr_bist_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3",
};
static const char * const ddr_pxi0_groups[] = {
	"gpio1", "gpio2",
};
static const char * const ddr_pxi1_groups[] = {
	"gpio8", "gpio9",
};
static const char * const ddr_pxi2_groups[] = {
	"gpio14", "gpio15",
};
static const char * const ddr_pxi3_groups[] = {
	"gpio16", "gpio17",
};
static const char * const gsm0_tx_groups[] = {
	"gpio68",
};
static const char * const gsm1_tx_groups[] = {
	"gpio57",
};
static const char * const m_voc_groups[] = {
	"gpio0",
};
static const char * const mdp_vsync_groups[] = {
	"gpio6", "gpio19", "gpio73", "gpio73", "gpio73",
};
static const char * const mpm_pwr_groups[] = {
	"gpio1",
};
static const char * const nav_gpio0_groups[] = {
	"gpio47",
};
static const char * const nav_gpio1_groups[] = {
	"gpio48",
};
static const char * const nav_gpio2_groups[] = {
	"gpio49",
};
static const char * const pbs0_groups[] = {
	"gpio20",
};
static const char * const pbs1_groups[] = {
	"gpio21",
};
static const char * const pbs10_groups[] = {
	"gpio30",
};
static const char * const pbs11_groups[] = {
	"gpio31",
};
static const char * const pbs12_groups[] = {
	"gpio32",
};
static const char * const pbs13_groups[] = {
	"gpio33",
};
static const char * const pbs14_groups[] = {
	"gpio34",
};
static const char * const pbs15_groups[] = {
	"gpio35",
};
static const char * const pbs2_groups[] = {
	"gpio22",
};
static const char * const pbs3_groups[] = {
	"gpio23",
};
static const char * const pbs4_groups[] = {
	"gpio24",
};
static const char * const pbs5_groups[] = {
	"gpio25",
};
static const char * const pbs6_groups[] = {
	"gpio26",
};
static const char * const pbs7_groups[] = {
	"gpio27",
};
static const char * const pbs8_groups[] = {
	"gpio28",
};
static const char * const pbs9_groups[] = {
	"gpio29",
};
static const char * const pbs_out_groups[] = {
	"gpio36", "gpio37", "gpio38",
};
static const char * const phase_flag0_groups[] = {
	"gpio0",
};
static const char * const phase_flag1_groups[] = {
	"gpio1",
};
static const char * const phase_flag10_groups[] = {
	"gpio20",
};
static const char * const phase_flag11_groups[] = {
	"gpio21",
};
static const char * const phase_flag12_groups[] = {
	"gpio22",
};
static const char * const phase_flag13_groups[] = {
	"gpio23",
};
static const char * const phase_flag14_groups[] = {
	"gpio24",
};
static const char * const phase_flag15_groups[] = {
	"gpio25",
};
static const char * const phase_flag16_groups[] = {
	"gpio26",
};
static const char * const phase_flag17_groups[] = {
	"gpio27",
};
static const char * const phase_flag18_groups[] = {
	"gpio28",
};
static const char * const phase_flag19_groups[] = {
	"gpio29",
};
static const char * const phase_flag2_groups[] = {
	"gpio2",
};
static const char * const phase_flag20_groups[] = {
	"gpio30",
};
static const char * const phase_flag21_groups[] = {
	"gpio31",
};
static const char * const phase_flag22_groups[] = {
	"gpio80",
};
static const char * const phase_flag23_groups[] = {
	"gpio41",
};
static const char * const phase_flag24_groups[] = {
	"gpio42",
};
static const char * const phase_flag25_groups[] = {
	"gpio43",
};
static const char * const phase_flag26_groups[] = {
	"gpio46",
};
static const char * const phase_flag27_groups[] = {
	"gpio50",
};
static const char * const phase_flag28_groups[] = {
	"gpio53",
};
static const char * const phase_flag29_groups[] = {
	"gpio54",
};
static const char * const phase_flag3_groups[] = {
	"gpio8",
};
static const char * const phase_flag30_groups[] = {
	"gpio55",
};
static const char * const phase_flag31_groups[] = {
	"gpio56",
};
static const char * const phase_flag4_groups[] = {
	"gpio9",
};
static const char * const phase_flag5_groups[] = {
	"gpio14",
};
static const char * const phase_flag6_groups[] = {
	"gpio15",
};
static const char * const phase_flag7_groups[] = {
	"gpio16",
};
static const char * const phase_flag8_groups[] = {
	"gpio17",
};
static const char * const phase_flag9_groups[] = {
	"gpio18",
};
static const char * const pll_bypassnl_groups[] = {
	"gpio66",
};
static const char * const pll_clk_groups[] = {
	"gpio10",
};
static const char * const pll_reset_groups[] = {
	"gpio67",
};
static const char * const prng_rosc0_groups[] = {
	"gpio57",
};
static const char * const prng_rosc1_groups[] = {
	"gpio59",
};
static const char * const prng_rosc2_groups[] = {
	"gpio61",
};
static const char * const prng_rosc3_groups[] = {
	"gpio63",
};
static const char * const pwm_0_groups[] = {
	"gpio35",
};
static const char * const pwm_1_groups[] = {
	"gpio34",
};
static const char * const pwm_2_groups[] = {
	"gpio41",
};
static const char * const pwm_3_groups[] = {
	"gpio24",
};
static const char * const pwm_4_groups[] = {
	"gpio25",
};
static const char * const pwm_5_groups[] = {
	"gpio18",
};
static const char * const pwm_6_groups[] = {
	"gpio12",
};
static const char * const pwm_7_groups[] = {
	"gpio45",
};
static const char * const pwm_8_groups[] = {
	"gpio42",
};
static const char * const pwm_9_groups[] = {
	"gpio46",
};
static const char * const qdss_cti_groups[] = {
	"gpio18", "gpio19", "gpio78", "gpio79", "gpio95", "gpio96", "gpio97",
	"gpio98", "gpio101", "gpio102", "gpio109", "gpio111",
};
static const char * const qdss_gpio_groups[] = {
	"gpio35", "gpio36", "gpio91", "gpio92",
};
static const char * const qdss_gpio0_groups[] = {
	"gpio0", "gpio3",
};
static const char * const qdss_gpio1_groups[] = {
	"gpio1", "gpio4",
};
static const char * const qdss_gpio10_groups[] = {
	"gpio20", "gpio31",
};
static const char * const qdss_gpio11_groups[] = {
	"gpio21", "gpio74",
};
static const char * const qdss_gpio12_groups[] = {
	"gpio22", "gpio75",
};
static const char * const qdss_gpio13_groups[] = {
	"gpio23", "gpio76",
};
static const char * const qdss_gpio14_groups[] = {
	"gpio24", "gpio77",
};
static const char * const qdss_gpio15_groups[] = {
	"gpio25", "gpio93",
};
static const char * const qdss_gpio2_groups[] = {
	"gpio2", "gpio5",
};
static const char * const qdss_gpio3_groups[] = {
	"gpio6", "gpio8",
};
static const char * const qdss_gpio4_groups[] = {
	"gpio7", "gpio9",
};
static const char * const qdss_gpio5_groups[] = {
	"gpio14", "gpio94",
};
static const char * const qdss_gpio6_groups[] = {
	"gpio15", "gpio27",
};
static const char * const qdss_gpio7_groups[] = {
	"gpio16", "gpio28",
};
static const char * const qdss_gpio8_groups[] = {
	"gpio17", "gpio29",
};
static const char * const qdss_gpio9_groups[] = {
	"gpio26", "gpio30",
};
static const char * const qup00_groups[] = {
	"gpio4", "gpio5", "gpio6", "gpio7",
};
static const char * const qup01_groups[] = {
	"gpio10", "gpio11", "gpio12", "gpio13",
};
static const char * const qup02_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3", "gpio8", "gpio9",
};
static const char * const qup03_groups[] = {
	"gpio14", "gpio15", "gpio16", "gpio17", "gpio18", "gpio19", "gpio80",
};
static const char * const qup04_groups[] = {
	"gpio20", "gpio21", "gpio22", "gpio23",
};
static const char * const qup05_groups[] = {
	"gpio26", "gpio27", "gpio28", "gpio29", "gpio35", "gpio36",
};
static const char * const qup06_groups[] = {
	"gpio24", "gpio25", "gpio30", "gpio31",
};
static const char * const sdc3_clk_groups[] = {
	"gpio79",
};
static const char * const sdc3_cmd_groups[] = {
	"gpio78",
};
static const char * const sdc3_data_groups[] = {
	"gpio74", "gpio75", "gpio76", "gpio77",
};
static const char * const tgu_ch0_groups[] = {
	"gpio30",
};
static const char * const tgu_ch1_groups[] = {
	"gpio31",
};
static const char * const tgu_ch2_groups[] = {
	"gpio14",
};
static const char * const tgu_ch3_groups[] = {
	"gpio40",
};
static const char * const tsense_pwm_groups[] = {
	"gpio3",
};
static const char * const uim0_clk_groups[] = {
	"gpio70",
};
static const char * const uim0_data_groups[] = {
	"gpio69",
};
static const char * const uim0_present_groups[] = {
	"gpio72",
};
static const char * const uim0_reset_groups[] = {
	"gpio71",
};
static const char * const usb2phy_ac_groups[] = {
	"gpio42",
};
static const char * const vfr_1_groups[] = {
	"gpio54",
};
static const char * const vsense_trigger_groups[] = {
	"gpio18",
};
static const char * const wci_uart_groups[] = {
	"gpio81", "gpio82",
};
static const char * const wlan1_adc0_groups[] = {
	"gpio81",
};
static const char * const wlan1_adc1_groups[] = {
	"gpio82",
};

static const struct msm_function monaco_functions[] = {
	FUNCTION(gpio),
	FUNCTION(qup02),
	FUNCTION(m_voc),
	FUNCTION(phase_flag0),
	FUNCTION(ddr_bist),
	FUNCTION(qdss_gpio0),
	FUNCTION(dac_calib0),
	FUNCTION(atest_usb10),
	FUNCTION(mpm_pwr),
	FUNCTION(phase_flag1),
	FUNCTION(qdss_gpio1),
	FUNCTION(atest_usb11),
	FUNCTION(ddr_pxi0),
	FUNCTION(phase_flag2),
	FUNCTION(qdss_gpio2),
	FUNCTION(dac_calib2),
	FUNCTION(atest_usb12),
	FUNCTION(dac_calib3),
	FUNCTION(tsense_pwm),
	FUNCTION(qup00),
	FUNCTION(CRI_TRNG0),
	FUNCTION(atest_tsens),
	FUNCTION(CRI_TRNG1),
	FUNCTION(atest_tsens2),
	FUNCTION(mdp_vsync),
	FUNCTION(GCC_GP2),
	FUNCTION(qdss_gpio3),
	FUNCTION(CCI_TIMER0),
	FUNCTION(cci_async),
	FUNCTION(GCC_GP3),
	FUNCTION(qdss_gpio4),
	FUNCTION(GCC_GP1),
	FUNCTION(phase_flag3),
	FUNCTION(atest_usb13),
	FUNCTION(ddr_pxi1),
	FUNCTION(GP_PDM0),
	FUNCTION(phase_flag4),
	FUNCTION(atest_usb1),
	FUNCTION(qup01),
	FUNCTION(pll_clk),
	FUNCTION(AGERA_PLL),
	FUNCTION(dac_calib4),
	FUNCTION(pwm_6),
	FUNCTION(GP_PDM1),
	FUNCTION(dac_calib5),
	FUNCTION(qup03),
	FUNCTION(tgu_ch2),
	FUNCTION(PLL_BIST),
	FUNCTION(phase_flag5),
	FUNCTION(qdss_gpio5),
	FUNCTION(ddr_pxi2),
	FUNCTION(phase_flag6),
	FUNCTION(qdss_gpio6),
	FUNCTION(phase_flag7),
	FUNCTION(qdss_gpio7),
	FUNCTION(dac_calib7),
	FUNCTION(ddr_pxi3),
	FUNCTION(phase_flag8),
	FUNCTION(qdss_gpio8),
	FUNCTION(dac_calib8),
	FUNCTION(pwm_5),
	FUNCTION(phase_flag9),
	FUNCTION(qdss_cti),
	FUNCTION(vsense_trigger),
	FUNCTION(GP_PDM2),
	FUNCTION(qup04),
	FUNCTION(pbs0),
	FUNCTION(phase_flag10),
	FUNCTION(qdss_gpio10),
	FUNCTION(dac_calib10),
	FUNCTION(atest_usb20),
	FUNCTION(pbs1),
	FUNCTION(phase_flag11),
	FUNCTION(qdss_gpio11),
	FUNCTION(dac_calib11),
	FUNCTION(atest_usb21),
	FUNCTION(pbs2),
	FUNCTION(phase_flag12),
	FUNCTION(qdss_gpio12),
	FUNCTION(dac_calib12),
	FUNCTION(atest_usb22),
	FUNCTION(pbs3),
	FUNCTION(phase_flag13),
	FUNCTION(qdss_gpio13),
	FUNCTION(dac_calib13),
	FUNCTION(atest_usb23),
	FUNCTION(qup06),
	FUNCTION(pwm_3),
	FUNCTION(pbs4),
	FUNCTION(phase_flag14),
	FUNCTION(qdss_gpio14),
	FUNCTION(atest_usb2),
	FUNCTION(pwm_4),
	FUNCTION(pbs5),
	FUNCTION(phase_flag15),
	FUNCTION(qdss_gpio15),
	FUNCTION(dac_calib15),
	FUNCTION(qup05),
	FUNCTION(pbs6),
	FUNCTION(phase_flag16),
	FUNCTION(qdss_gpio9),
	FUNCTION(dac_calib16),
	FUNCTION(pbs7),
	FUNCTION(phase_flag17),
	FUNCTION(dac_calib17),
	FUNCTION(pbs8),
	FUNCTION(phase_flag18),
	FUNCTION(dac_calib18),
	FUNCTION(pbs9),
	FUNCTION(phase_flag19),
	FUNCTION(dac_calib19),
	FUNCTION(tgu_ch0),
	FUNCTION(pbs10),
	FUNCTION(phase_flag20),
	FUNCTION(dac_calib20),
	FUNCTION(tgu_ch1),
	FUNCTION(pbs11),
	FUNCTION(phase_flag21),
	FUNCTION(dac_calib21),
	FUNCTION(cam_mclk),
	FUNCTION(pbs12),
	FUNCTION(adsp_ext),
	FUNCTION(pbs13),
	FUNCTION(dac_calib9),
	FUNCTION(CCI_TIMER2),
	FUNCTION(pwm_1),
	FUNCTION(pbs14),
	FUNCTION(SDC2_TB),
	FUNCTION(pwm_0),
	FUNCTION(CRI_TRNG),
	FUNCTION(pbs15),
	FUNCTION(qdss_gpio),
	FUNCTION(dac_calib1),
	FUNCTION(CCI_TIMER1),
	FUNCTION(SDC1_TB),
	FUNCTION(pbs_out),
	FUNCTION(cci_i2c),
	FUNCTION(dac_calib14),
	FUNCTION(tgu_ch3),
	FUNCTION(dac_calib6),
	FUNCTION(CCI_TIMER3),
	FUNCTION(pwm_2),
	FUNCTION(phase_flag23),
	FUNCTION(dac_calib23),
	FUNCTION(usb2phy_ac),
	FUNCTION(pwm_8),
	FUNCTION(phase_flag24),
	FUNCTION(dac_calib24),
	FUNCTION(phase_flag25),
	FUNCTION(dac_calib25),
	FUNCTION(atest_char),
	FUNCTION(atest_char0),
	FUNCTION(pwm_7),
	FUNCTION(atest_char1),
	FUNCTION(pwm_9),
	FUNCTION(phase_flag26),
	FUNCTION(atest_char2),
	FUNCTION(nav_gpio0),
	FUNCTION(nav_gpio1),
	FUNCTION(nav_gpio2),
	FUNCTION(phase_flag27),
	FUNCTION(atest_char3),
	FUNCTION(phase_flag28),
	FUNCTION(vfr_1),
	FUNCTION(phase_flag29),
	FUNCTION(PA_INDICATOR),
	FUNCTION(phase_flag30),
	FUNCTION(phase_flag31),
	FUNCTION(gsm1_tx),
	FUNCTION(prng_rosc0),
	FUNCTION(prng_rosc1),
	FUNCTION(prng_rosc2),
	FUNCTION(SSBI_WTR1),
	FUNCTION(prng_rosc3),
	FUNCTION(pll_bypassnl),
	FUNCTION(pll_reset),
	FUNCTION(gsm0_tx),
	FUNCTION(uim0_data),
	FUNCTION(uim0_clk),
	FUNCTION(uim0_reset),
	FUNCTION(uim0_present),
	FUNCTION(sdc3_data),
	FUNCTION(sdc3_cmd),
	FUNCTION(sdc3_clk),
	FUNCTION(phase_flag22),
	FUNCTION(dac_calib22),
	FUNCTION(wci_uart),
	FUNCTION(JITTER_BIST),
	FUNCTION(dbg_out),
	FUNCTION(atest_gpsadc_dtest0_native),
	FUNCTION(wlan1_adc0),
	FUNCTION(atest_gpsadc_dtest1_native),
	FUNCTION(wlan1_adc1),
	FUNCTION(atest_bbrx1),
	FUNCTION(atest_bbrx0),
	FUNCTION(QUP0_L0),
	FUNCTION(QUP0_L2),
	FUNCTION(QUP0_L1),
	FUNCTION(QUP0_L3),
};

/*
 * Every pin is maintained as a single group, and missing or non-existing pin
 * would be maintained as dummy group to synchronize pin group index with
 * pin descriptor registered with pinctrl core.
 * Clients would not be able to request these dummy pin groups.
 */
static const struct msm_pingroup monaco_groups[] = {
	[0] = PINGROUP(0, qup02, m_voc, ddr_bist, NA, phase_flag0, qdss_gpio0,
		       dac_calib0, atest_usb10, NA, 0x70000, 0),
	[1] = PINGROUP(1, qup02, mpm_pwr, ddr_bist, NA, phase_flag1,
		       qdss_gpio1, atest_usb11, ddr_pxi0, NA, 0, -1),
	[2] = PINGROUP(2, qup02, ddr_bist, NA, phase_flag2, qdss_gpio2,
		       dac_calib2, atest_usb12, ddr_pxi0, NA, 0, -1),
	[3] = PINGROUP(3, qup02, ddr_bist, qdss_gpio0, dac_calib3, NA, NA,
		       tsense_pwm, NA, NA, 0x70000, 1),
	[4] = PINGROUP(4, qup00, CRI_TRNG0, NA, qdss_gpio1, atest_tsens, NA,
		       NA, NA, NA, 0x70000, 2),
	[5] = PINGROUP(5, qup00, CRI_TRNG1, NA, qdss_gpio2, atest_tsens2, NA,
		       NA, NA, NA, 0, -1),
	[6] = PINGROUP(6, qup00, mdp_vsync, GCC_GP2, NA, qdss_gpio3, NA, NA,
		       NA, NA, 0x70000, 3),
	[7] = PINGROUP(7, qup00, CCI_TIMER0, cci_async, GCC_GP3, NA,
		       qdss_gpio4, NA, NA, NA, 0x70000, 4),
	[8] = PINGROUP(8, qup02, GCC_GP1, NA, phase_flag3, qdss_gpio3,
		       atest_usb13, ddr_pxi1, NA, NA, 0x70000, 5),
	[9] = PINGROUP(9, qup02, GP_PDM0, NA, phase_flag4, qdss_gpio4,
		       atest_usb1, ddr_pxi1, NA, NA, 0x70000, 6),
	[10] = PINGROUP(10, qup01, pll_clk, AGERA_PLL, dac_calib4, NA, NA, NA,
			NA, NA, 0x70000, 7),
	[11] = PINGROUP(11, qup01, AGERA_PLL, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[12] = PINGROUP(12, qup01, pwm_6, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[13] = PINGROUP(13, qup01, GP_PDM1, dac_calib5, NA, NA, NA, NA, NA, NA,
			0x70000, 8),
	[14] = PINGROUP(14, qup03, tgu_ch2, PLL_BIST, NA, phase_flag5,
			qdss_gpio5, ddr_pxi2, NA, NA, 0x70000, 9),
	[15] = PINGROUP(15, qup03, PLL_BIST, NA, phase_flag6, qdss_gpio6,
			ddr_pxi2, NA, NA, NA, 0, -1),
	[16] = PINGROUP(16, qup03, NA, phase_flag7, qdss_gpio7, dac_calib7,
			ddr_pxi3, NA, NA, NA, 0x70000, 10),
	[17] = PINGROUP(17, qup03, NA, phase_flag8, qdss_gpio8, dac_calib8,
			ddr_pxi3, NA, NA, NA, 0x70000, 11),
	[18] = PINGROUP(18, qup03, NA, pwm_5, NA, phase_flag9, qdss_cti,
			vsense_trigger, NA, NA, 0x70000, 12),
	[19] = PINGROUP(19, qup03, mdp_vsync, GP_PDM2, qdss_cti, NA, NA, NA,
			NA, NA, 0x70000, 13),
	[20] = PINGROUP(20, qup04, NA, pbs0, phase_flag10, qdss_gpio10,
			dac_calib10, atest_usb20, NA, NA, 0x70010, 3),
	[21] = PINGROUP(21, qup04, NA, pbs1, phase_flag11, qdss_gpio11,
			dac_calib11, atest_usb21, NA, NA, 0x70010, 4),
	[22] = PINGROUP(22, qup04, NA, pbs2, phase_flag12, qdss_gpio12,
			dac_calib12, atest_usb22, NA, NA, 0x70010, 5),
	[23] = PINGROUP(23, qup04, NA, pbs3, phase_flag13, qdss_gpio13,
			dac_calib13, atest_usb23, NA, NA, 0x70010, 6),
	[24] = PINGROUP(24, qup06, pwm_3, NA, pbs4, phase_flag14, qdss_gpio14,
			atest_usb2, NA, NA, 0x70010, 7),
	[25] = PINGROUP(25, qup06, pwm_4, NA, pbs5, phase_flag15, qdss_gpio15,
			dac_calib15, NA, NA, 0x70010, 8),
	[26] = PINGROUP(26, qup05, NA, pbs6, phase_flag16, qdss_gpio9,
			dac_calib16, NA, NA, NA, 0x70010, 9),
	[27] = PINGROUP(27, qup05, NA, pbs7, phase_flag17, qdss_gpio6,
			dac_calib17, NA, NA, NA, 0, -1),
	[28] = PINGROUP(28, qup05, NA, pbs8, phase_flag18, qdss_gpio7,
			dac_calib18, NA, NA, NA, 0, -1),
	[29] = PINGROUP(29, qup05, NA, pbs9, phase_flag19, qdss_gpio8,
			dac_calib19, NA, NA, NA, 0x70010, 10),
	[30] = PINGROUP(30, qup06, tgu_ch0, NA, pbs10, phase_flag20,
			qdss_gpio9, dac_calib20, NA, NA, 0, -1),
	[31] = PINGROUP(31, qup06, tgu_ch1, NA, pbs11, phase_flag21,
			qdss_gpio10, dac_calib21, NA, NA, 0x70010, 11),
	[32] = PINGROUP(32, cam_mclk, NA, pbs12, NA, NA, NA, NA, NA, NA, 0, -1),
	[33] = PINGROUP(33, cam_mclk, GP_PDM2, adsp_ext, NA, pbs13, dac_calib9,
			NA, NA, NA, 0x70010, 12),
	[34] = PINGROUP(34, cam_mclk, CCI_TIMER2, pwm_1, NA, pbs14, NA, NA, NA,
			NA, 0x70010, 13),
	[35] = PINGROUP(35, qup05, SDC2_TB, pwm_0, CRI_TRNG, NA, pbs15,
			qdss_gpio, dac_calib1, NA, 0, -1),
	[36] = PINGROUP(36, qup05, CCI_TIMER1, SDC1_TB, GCC_GP1, NA, pbs_out,
			qdss_gpio, NA, NA, 0x70010, 14),
	[37] = PINGROUP(37, cci_i2c, NA, pbs_out, dac_calib14, NA, NA, NA, NA,
			NA, 0, -1),
	[38] = PINGROUP(38, cci_i2c, NA, pbs_out, NA, NA, NA, NA, NA, NA,
			0, -1),
	[39] = PINGROUP(39, cci_i2c, NA, NA, NA, NA, NA, NA, NA, NA,
			0x70010, 15),
	[40] = PINGROUP(40, cci_i2c, tgu_ch3, NA, dac_calib6, NA, NA, NA, NA,
			NA, 0x70014, 0),
	[41] = PINGROUP(41, NA, CCI_TIMER3, pwm_2, NA, phase_flag23,
			dac_calib23, NA, NA, NA, 0x7000C, 0),
	[42] = PINGROUP(42, NA, usb2phy_ac, NA, pwm_8, NA, phase_flag24,
			dac_calib24, NA, NA, 0x7000C, 1),
	[43] = PINGROUP(43, NA, NA, phase_flag25, dac_calib25, atest_char, NA,
			NA, NA, NA, 0, -1),
	[44] = PINGROUP(44, NA, atest_char0, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[45] = PINGROUP(45, NA, pwm_7, atest_char1, NA, NA, NA, NA, NA, NA,
			0x7000C, 2),
	[46] = PINGROUP(46, NA, pwm_9, NA, phase_flag26, atest_char2, NA, NA,
			NA, NA, 0, -1),
	[47] = PINGROUP(47, nav_gpio0, NA, NA, NA, NA, NA, NA, NA, NA,
			0x7000C, 3),
	[48] = PINGROUP(48, nav_gpio1, NA, NA, NA, NA, NA, NA, NA, NA,
			0x7000C, 4),
	[49] = PINGROUP(49, nav_gpio2, NA, NA, NA, NA, NA, NA, NA, NA,
			0x7000C, 5),
	[50] = PINGROUP(50, NA, NA, phase_flag27, atest_char3, NA, NA, NA, NA,
			NA, 0, -1),
	[51] = PINGROUP(51, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[52] = PINGROUP(52, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x7000C, 6),
	[53] = PINGROUP(53, NA, NA, phase_flag28, NA, NA, NA, NA, NA, NA,
			0, -1),
	[54] = PINGROUP(54, NA, vfr_1, NA, phase_flag29, NA, NA, NA, NA, NA,
			0, -1),
	[55] = PINGROUP(55, NA, PA_INDICATOR, NA, phase_flag30, NA, NA, NA, NA,
			NA, 0x7000C, 7),
	[56] = PINGROUP(56, NA, NA, phase_flag31, NA, NA, NA, NA, NA, NA,
			0x7000C, 8),
	[57] = PINGROUP(57, NA, gsm1_tx, prng_rosc0, NA, NA, NA, NA, NA, NA,
			0, -1),
	[58] = PINGROUP(58, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[59] = PINGROUP(59, NA, NA, prng_rosc1, NA, NA, NA, NA, NA, NA, 0, -1),
	[60] = PINGROUP(60, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[61] = PINGROUP(61, NA, prng_rosc2, NA, NA, NA, NA, NA, NA, NA,
			0x7000C, 9),
	[62] = PINGROUP(62, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x7000C, 10),
	[63] = PINGROUP(63, NA, NA, SSBI_WTR1, prng_rosc3, NA, NA, NA, NA, NA,
			0x7000C, 11),
	[64] = PINGROUP(64, NA, NA, SSBI_WTR1, GCC_GP2, NA, NA, NA, NA, NA,
			0x7000C, 12),
	[65] = PINGROUP(65, NA, GCC_GP3, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[66] = PINGROUP(66, NA, pll_bypassnl, NA, NA, NA, NA, NA, NA, NA,
			0x7000C, 13),
	[67] = PINGROUP(67, pll_reset, NA, NA, NA, NA, NA, NA, NA, NA,
			0x7000C, 14),
	[68] = PINGROUP(68, gsm0_tx, NA, NA, NA, NA, NA, NA, NA, NA,
			0x7000C, 15),
	[69] = PINGROUP(69, uim0_data, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[70] = PINGROUP(70, uim0_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[71] = PINGROUP(71, uim0_reset, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[72] = PINGROUP(72, uim0_present, NA, NA, NA, NA, NA, NA, NA, NA,
			0x70000, 14),
	[73] = PINGROUP(73, mdp_vsync, mdp_vsync, mdp_vsync, NA, NA, NA, NA,
			NA, NA, 0x70000, 15),
	[74] = PINGROUP(74, sdc3_data, qdss_gpio11, NA, NA, NA, NA, NA, NA, NA,
			0x70004, 0),
	[75] = PINGROUP(75, sdc3_data, qdss_gpio12, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[76] = PINGROUP(76, sdc3_data, GP_PDM0, qdss_gpio13, NA, NA, NA, NA,
			NA, NA, 0x70004, 1),
	[77] = PINGROUP(77, sdc3_data, GP_PDM1, qdss_gpio14, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[78] = PINGROUP(78, sdc3_cmd, qdss_cti, NA, NA, NA, NA, NA, NA, NA,
			0x70004, 2),
	[79] = PINGROUP(79, sdc3_clk, qdss_cti, NA, NA, NA, NA, NA, NA, NA,
			0x70004, 3),
	[80] = PINGROUP(80, qup03, NA, phase_flag22, dac_calib22, NA, NA, NA,
			NA, NA, 0x70004, 4),
	[81] = PINGROUP(81, wci_uart, JITTER_BIST, dbg_out,
			atest_gpsadc_dtest0_native, wlan1_adc0, NA, NA, NA, NA,
			0, -1),
	[82] = PINGROUP(82, wci_uart, JITTER_BIST, NA,
			atest_gpsadc_dtest1_native, wlan1_adc1, NA, NA, NA, NA,
			0x70004, 5),
	[83] = PINGROUP(83, NA, atest_bbrx1, NA, NA, NA, NA, NA, NA, NA,
			0x70014, 1),
	[84] = PINGROUP(84, NA, atest_bbrx0, NA, NA, NA, NA, NA, NA, NA,
			0x70014, 2),
	[85] = PINGROUP(85, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[86] = PINGROUP(86, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x70004, 6),
	[87] = PINGROUP(87, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x70004, 7),
	[88] = PINGROUP(88, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[89] = PINGROUP(89, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x70004, 8),
	[90] = PINGROUP(90, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x70004, 9),
	[91] = PINGROUP(91, qdss_gpio, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[92] = PINGROUP(92, qdss_gpio, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[93] = PINGROUP(93, qdss_gpio15, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[94] = PINGROUP(94, qdss_gpio5, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[95] = PINGROUP(95, qdss_cti, NA, NA, NA, NA, NA, NA, NA, NA,
			0x70004, 10),
	[96] = PINGROUP(96, qdss_cti, NA, NA, NA, NA, NA, NA, NA, NA,
			0x70004, 11),
	[97] = PINGROUP(97, qdss_cti, NA, NA, NA, NA, NA, NA, NA, NA,
			0x70004, 12),
	[98] = PINGROUP(98, qdss_cti, NA, NA, NA, NA, NA, NA, NA, NA,
			0x70004, 13),
	[99] = PINGROUP(99, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[100] = PINGROUP(100, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x70004, 14),
	[101] = PINGROUP(101, QUP0_L0, QUP0_L2, qdss_cti, NA, NA, NA, NA, NA,
			 NA, 0x70004, 15),
	[102] = PINGROUP(102, QUP0_L1, QUP0_L3, qdss_cti, NA, NA, NA, NA, NA,
			 NA, 0x70008, 0),
	[103] = PINGROUP(103, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[104] = PINGROUP(104, QUP0_L2, QUP0_L0, NA, NA, NA, NA, NA, NA, NA,
			 0x70008, 1),
	[105] = PINGROUP(105, QUP0_L3, QUP0_L1, NA, NA, NA, NA, NA, NA, NA,
			 0x70008, 2),
	[106] = PINGROUP(106, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0x70008, 3),
	[107] = PINGROUP(107, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[108] = PINGROUP(108, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[109] = PINGROUP(109, qdss_cti, NA, NA, NA, NA, NA, NA, NA, NA,
			 0x70008, 4),
	[110] = PINGROUP(110, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[111] = PINGROUP(111, qdss_cti, NA, NA, NA, NA, NA, NA, NA, NA,
			 0x70008, 5),
	[112] = SDC_QDSD_PINGROUP(sdc1_rclk, 0x75004, 0, 0),
	[113] = SDC_QDSD_PINGROUP(sdc1_clk, 0x75000, 13, 6),
	[114] = SDC_QDSD_PINGROUP(sdc1_cmd, 0x75000, 11, 3),
	[115] = SDC_QDSD_PINGROUP(sdc1_data, 0x75000, 9, 0),
};
static struct pinctrl_qup monaco_qup_regs[] = {
	QUP_I3C(5, QUP_I3C_5_MODE_OFFSET),
	QUP_I3C(6, QUP_I3C_6_MODE_OFFSET),
};

static const struct msm_gpio_wakeirq_map monaco_mpm_map[] = {
	{0, 84},
	{3, 13},
	{4, 14},
	{6, 16},
	{7, 17},
	{8, 18},
	{9, 19},
	{10, 20},
	{13, 21},
	{14, 23},
	{16, 24},
	{17, 25},
	{18, 26},
	{19, 27},
	{20, 28},
	{23, 29},
	{24, 30},
	{25, 31},
	{26, 32},
	{29, 33},
	{31, 34},
	{33, 37},
	{34, 38},
	{36, 39},
	{39, 40},
	{40, 41},
	{41, 42},
	{42, 43},
	{45, 44},
	{47, 45},
	{48, 46},
	{49, 47},
	{52, 48},
	{55, 49},
	{56, 50},
	{61, 51},
	{62, 52},
	{63, 53},
	{64, 54},
	{66, 55},
	{67, 56},
	{68, 57},
	{72, 58},
	{73, 59},
	{74, 81},
	{76, 82},
	{78, 60},
	{79, 61},
	{80, 62},
	{82, 63},
	{83, 64},
	{84, 65},
};

static const struct msm_pinctrl_soc_data monaco_pinctrl = {
	.pins = monaco_pins,
	.npins = ARRAY_SIZE(monaco_pins),
	.functions = monaco_functions,
	.nfunctions = ARRAY_SIZE(monaco_functions),
	.groups = monaco_groups,
	.ngroups = ARRAY_SIZE(monaco_groups),
	.ngpios = 112,
	.qup_regs = monaco_qup_regs,
	.nqup_regs = ARRAY_SIZE(monaco_qup_regs),
	.wakeirq_map = monaco_mpm_map,
	.nwakeirq_map = ARRAY_SIZE(monaco_mpm_map),
};

static int monaco_pinctrl_probe(struct platform_device *pdev)
{
	return msm_pinctrl_probe(pdev, &monaco_pinctrl);
}

static const struct of_device_id monaco_pinctrl_of_match[] = {
	{ .compatible = "qcom,monaco-pinctrl", },
	{ },
};

static struct platform_driver monaco_pinctrl_driver = {
	.driver = {
		.name = "monaco-pinctrl",
		.of_match_table = monaco_pinctrl_of_match,
	},
	.probe = monaco_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init monaco_pinctrl_init(void)
{
	return platform_driver_register(&monaco_pinctrl_driver);
}
arch_initcall(monaco_pinctrl_init);

static void __exit monaco_pinctrl_exit(void)
{
	platform_driver_unregister(&monaco_pinctrl_driver);
}
module_exit(monaco_pinctrl_exit);

MODULE_DESCRIPTION("QTI monaco pinctrl driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, monaco_pinctrl_of_match);
