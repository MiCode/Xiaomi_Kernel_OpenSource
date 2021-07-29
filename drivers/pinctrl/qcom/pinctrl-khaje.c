// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>

#include "pinctrl-msm.h"

#define FUNCTION(fname)					\
	[msm_mux_##fname] = {				\
		.name = #fname,				\
		.groups = fname##_groups,		\
		.ngroups = ARRAY_SIZE(fname##_groups),	\
	}

#define SOUTH		0x00500000
#define WEST		0x00100000
#define EAST		0x00900000
#define DUMMY		0x0
#define REG_SIZE	0x1000
#define PINGROUP(id, base, f1, f2, f3, f4, f5, f6, f7, f8, f9, wake_off, bit)	\
	{						\
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
		},					\
		.nfuncs = 10,				\
		.ctl_reg = base + REG_SIZE * id,		\
		.io_reg = base + 0x4 + REG_SIZE * id,		\
		.intr_cfg_reg = base + 0x8 + REG_SIZE * id,	\
		.intr_status_reg = base + 0xc + REG_SIZE * id,	\
		.intr_target_reg = base + 0x8 + REG_SIZE * id,	\
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
		.wake_reg = base + wake_off,	\
		.wake_bit = bit,		\
	}

#define SDC_QDSD_PINGROUP(pg_name, ctl, pull, drv)	\
	{						\
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
	{						\
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

static const struct pinctrl_pin_desc khaje_pins[] = {
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
	PINCTRL_PIN(113, "SDC1_RCLK"),
	PINCTRL_PIN(114, "SDC1_CLK"),
	PINCTRL_PIN(115, "SDC1_CMD"),
	PINCTRL_PIN(116, "SDC1_DATA"),
	PINCTRL_PIN(117, "SDC2_CLK"),
	PINCTRL_PIN(118, "SDC2_CMD"),
	PINCTRL_PIN(119, "SDC2_DATA"),
	PINCTRL_PIN(120, "UFS_RESET"),
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

static const unsigned int sdc1_rclk_pins[] = { 113 };
static const unsigned int sdc1_clk_pins[] = { 114 };
static const unsigned int sdc1_cmd_pins[] = { 115 };
static const unsigned int sdc1_data_pins[] = { 116 };
static const unsigned int sdc2_clk_pins[] = { 117 };
static const unsigned int sdc2_cmd_pins[] = { 118 };
static const unsigned int sdc2_data_pins[] = { 119 };
static const unsigned int ufs_reset_pins[] = { 120 };

enum khaje_functions {
	msm_mux_gpio,
	msm_mux_qup0,
	msm_mux_usb2phy_ac,
	msm_mux_ddr_bist,
	msm_mux_m_voc,
	msm_mux_phase_flag0,
	msm_mux_qdss_gpio8,
	msm_mux_atest_tsens,
	msm_mux_mpm_pwr,
	msm_mux_phase_flag1,
	msm_mux_qdss_gpio9,
	msm_mux_atest_tsens2,
	msm_mux_phase_flag2,
	msm_mux_qdss_gpio10,
	msm_mux_dac_calib0,
	msm_mux_atest_usb10,
	msm_mux_phase_flag3,
	msm_mux_qdss_gpio11,
	msm_mux_dac_calib1,
	msm_mux_atest_usb11,
	msm_mux_qup1,
	msm_mux_CRI_TRNG0,
	msm_mux_phase_flag4,
	msm_mux_dac_calib2,
	msm_mux_atest_usb12,
	msm_mux_CRI_TRNG1,
	msm_mux_phase_flag5,
	msm_mux_dac_calib3,
	msm_mux_atest_usb13,
	msm_mux_qup2,
	msm_mux_phase_flag6,
	msm_mux_dac_calib4,
	msm_mux_atest_usb1,
	msm_mux_qup3,
	msm_mux_pbs_out,
	msm_mux_PLL_BIST,
	msm_mux_qdss_gpio,
	msm_mux_tsense_pwm,
	msm_mux_AGERA_PLL,
	msm_mux_pbs0,
	msm_mux_qdss_gpio0,
	msm_mux_pbs1,
	msm_mux_qdss_gpio1,
	msm_mux_qup4,
	msm_mux_tgu_ch0,
	msm_mux_tgu_ch1,
	msm_mux_qup5,
	msm_mux_tgu_ch2,
	msm_mux_phase_flag7,
	msm_mux_qdss_gpio4,
	msm_mux_dac_calib5,
	msm_mux_tgu_ch3,
	msm_mux_phase_flag8,
	msm_mux_qdss_gpio5,
	msm_mux_dac_calib6,
	msm_mux_phase_flag9,
	msm_mux_qdss_gpio6,
	msm_mux_dac_calib7,
	msm_mux_phase_flag10,
	msm_mux_qdss_gpio7,
	msm_mux_dac_calib8,
	msm_mux_SDC2_TB,
	msm_mux_CRI_TRNG,
	msm_mux_pbs2,
	msm_mux_qdss_gpio2,
	msm_mux_SDC1_TB,
	msm_mux_pbs3,
	msm_mux_qdss_gpio3,
	msm_mux_cam_mclk,
	msm_mux_pbs4,
	msm_mux_adsp_ext,
	msm_mux_pbs5,
	msm_mux_cci_i2c,
	msm_mux_prng_rosc,
	msm_mux_pbs6,
	msm_mux_phase_flag11,
	msm_mux_dac_calib9,
	msm_mux_atest_usb20,
	msm_mux_pbs7,
	msm_mux_phase_flag12,
	msm_mux_dac_calib10,
	msm_mux_atest_usb21,
	msm_mux_CCI_TIMER1,
	msm_mux_GCC_GP1,
	msm_mux_pbs8,
	msm_mux_phase_flag13,
	msm_mux_dac_calib11,
	msm_mux_atest_usb22,
	msm_mux_cci_async,
	msm_mux_CCI_TIMER0,
	msm_mux_pbs9,
	msm_mux_phase_flag14,
	msm_mux_dac_calib12,
	msm_mux_atest_usb23,
	msm_mux_pbs10,
	msm_mux_phase_flag15,
	msm_mux_dac_calib13,
	msm_mux_atest_usb2,
	msm_mux_vsense_trigger,
	msm_mux_qdss_cti,
	msm_mux_CCI_TIMER2,
	msm_mux_phase_flag16,
	msm_mux_dac_calib14,
	msm_mux_atest_char,
	msm_mux_phase_flag17,
	msm_mux_dac_calib15,
	msm_mux_atest_char0,
	msm_mux_GP_PDM0,
	msm_mux_phase_flag18,
	msm_mux_dac_calib16,
	msm_mux_atest_char1,
	msm_mux_CCI_TIMER3,
	msm_mux_GP_PDM1,
	msm_mux_phase_flag19,
	msm_mux_dac_calib17,
	msm_mux_atest_char2,
	msm_mux_GP_PDM2,
	msm_mux_phase_flag20,
	msm_mux_dac_calib18,
	msm_mux_atest_char3,
	msm_mux_phase_flag21,
	msm_mux_qdss_gpio14,
	msm_mux_phase_flag22,
	msm_mux_qdss_gpio15,
	msm_mux_NAV_GPIO,
	msm_mux_phase_flag23,
	msm_mux_phase_flag24,
	msm_mux_phase_flag25,
	msm_mux_pbs14,
	msm_mux_vfr_1,
	msm_mux_pbs15,
	msm_mux_PA_INDICATOR,
	msm_mux_gsm1_tx,
	msm_mux_SSBI_WTR1,
	msm_mux_pll_clk,
	msm_mux_pll_bypassnl,
	msm_mux_pll_reset,
	msm_mux_phase_flag26,
	msm_mux_ddr_pxi0,
	msm_mux_gsm0_tx,
	msm_mux_phase_flag27,
	msm_mux_GCC_GP2,
	msm_mux_qdss_gpio12,
	msm_mux_ddr_pxi1,
	msm_mux_GCC_GP3,
	msm_mux_qdss_gpio13,
	msm_mux_dbg_out,
	msm_mux_uim2_data,
	msm_mux_uim2_clk,
	msm_mux_uim2_reset,
	msm_mux_uim2_present,
	msm_mux_uim1_data,
	msm_mux_uim1_clk,
	msm_mux_uim1_reset,
	msm_mux_uim1_present,
	msm_mux_dac_calib19,
	msm_mux_mdp_vsync,
	msm_mux_mdp_vsync_out_0,
	msm_mux_mdp_vsync_out_1,
	msm_mux_dac_calib20,
	msm_mux_dac_calib21,
	msm_mux_atest_bbrx1,
	msm_mux_pbs11,
	msm_mux_usb_phy,
	msm_mux_atest_bbrx0,
	msm_mux_mss_lte,
	msm_mux_pbs12,
	msm_mux_pbs13,
	msm_mux_wlan1_adc0,
	msm_mux_wlan1_adc1,
	msm_mux_sd_write,
	msm_mux_JITTER_BIST,
	msm_mux_atest_gpsadc_dtest0_native,
	msm_mux_atest_gpsadc_dtest1_native,
	msm_mux_phase_flag28,
	msm_mux_dac_calib22,
	msm_mux_ddr_pxi2,
	msm_mux_phase_flag29,
	msm_mux_dac_calib23,
	msm_mux_phase_flag30,
	msm_mux_dac_calib24,
	msm_mux_ddr_pxi3,
	msm_mux_phase_flag31,
	msm_mux_dac_calib25,
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
	"gpio111", "gpio112",
};
static const char * const qup0_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3", "gpio82", "gpio86",
};
static const char * const usb2phy_ac_groups[] = {
	"gpio0",
};
static const char * const ddr_bist_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3",
};
static const char * const m_voc_groups[] = {
	"gpio0",
};
static const char * const phase_flag0_groups[] = {
	"gpio0",
};
static const char * const qdss_gpio8_groups[] = {
	"gpio0", "gpio24",
};
static const char * const atest_tsens_groups[] = {
	"gpio0",
};
static const char * const mpm_pwr_groups[] = {
	"gpio1",
};
static const char * const phase_flag1_groups[] = {
	"gpio1",
};
static const char * const qdss_gpio9_groups[] = {
	"gpio1", "gpio25",
};
static const char * const atest_tsens2_groups[] = {
	"gpio1",
};
static const char * const phase_flag2_groups[] = {
	"gpio2",
};
static const char * const qdss_gpio10_groups[] = {
	"gpio2", "gpio26",
};
static const char * const dac_calib0_groups[] = {
	"gpio2",
};
static const char * const atest_usb10_groups[] = {
	"gpio2",
};
static const char * const phase_flag3_groups[] = {
	"gpio3",
};
static const char * const qdss_gpio11_groups[] = {
	"gpio3", "gpio87",
};
static const char * const dac_calib1_groups[] = {
	"gpio3",
};
static const char * const atest_usb11_groups[] = {
	"gpio3",
};
static const char * const qup1_groups[] = {
	"gpio4", "gpio5", "gpio69", "gpio70",
};
static const char * const CRI_TRNG0_groups[] = {
	"gpio4",
};
static const char * const phase_flag4_groups[] = {
	"gpio4",
};
static const char * const dac_calib2_groups[] = {
	"gpio4",
};
static const char * const atest_usb12_groups[] = {
	"gpio4",
};
static const char * const CRI_TRNG1_groups[] = {
	"gpio5",
};
static const char * const phase_flag5_groups[] = {
	"gpio5",
};
static const char * const dac_calib3_groups[] = {
	"gpio5",
};
static const char * const atest_usb13_groups[] = {
	"gpio5",
};
static const char * const qup2_groups[] = {
	"gpio6", "gpio7", "gpio71", "gpio80",
};
static const char * const phase_flag6_groups[] = {
	"gpio6",
};
static const char * const dac_calib4_groups[] = {
	"gpio6",
};
static const char * const atest_usb1_groups[] = {
	"gpio6",
};
static const char * const qup3_groups[] = {
	"gpio8", "gpio9", "gpio10", "gpio11",
};
static const char * const pbs_out_groups[] = {
	"gpio8", "gpio9", "gpio52",
};
static const char * const PLL_BIST_groups[] = {
	"gpio8", "gpio9",
};
static const char * const qdss_gpio_groups[] = {
	"gpio8", "gpio9", "gpio105", "gpio106",
};
static const char * const tsense_pwm_groups[] = {
	"gpio8",
};
static const char * const AGERA_PLL_groups[] = {
	"gpio10", "gpio11",
};
static const char * const pbs0_groups[] = {
	"gpio10",
};
static const char * const qdss_gpio0_groups[] = {
	"gpio10", "gpio107",
};
static const char * const pbs1_groups[] = {
	"gpio11",
};
static const char * const qdss_gpio1_groups[] = {
	"gpio11", "gpio104",
};
static const char * const qup4_groups[] = {
	"gpio12", "gpio13", "gpio96", "gpio97",
};
static const char * const tgu_ch0_groups[] = {
	"gpio12",
};
static const char * const tgu_ch1_groups[] = {
	"gpio13",
};
static const char * const qup5_groups[] = {
	"gpio14", "gpio15", "gpio16", "gpio17",
};
static const char * const tgu_ch2_groups[] = {
	"gpio14",
};
static const char * const phase_flag7_groups[] = {
	"gpio14",
};
static const char * const qdss_gpio4_groups[] = {
	"gpio14", "gpio20",
};
static const char * const dac_calib5_groups[] = {
	"gpio14",
};
static const char * const tgu_ch3_groups[] = {
	"gpio15",
};
static const char * const phase_flag8_groups[] = {
	"gpio15",
};
static const char * const qdss_gpio5_groups[] = {
	"gpio15", "gpio21",
};
static const char * const dac_calib6_groups[] = {
	"gpio15",
};
static const char * const phase_flag9_groups[] = {
	"gpio16",
};
static const char * const qdss_gpio6_groups[] = {
	"gpio16", "gpio22",
};
static const char * const dac_calib7_groups[] = {
	"gpio16",
};
static const char * const phase_flag10_groups[] = {
	"gpio17",
};
static const char * const qdss_gpio7_groups[] = {
	"gpio17", "gpio23",
};
static const char * const dac_calib8_groups[] = {
	"gpio17",
};
static const char * const SDC2_TB_groups[] = {
	"gpio18",
};
static const char * const CRI_TRNG_groups[] = {
	"gpio18",
};
static const char * const pbs2_groups[] = {
	"gpio18",
};
static const char * const qdss_gpio2_groups[] = {
	"gpio18", "gpio109",
};
static const char * const SDC1_TB_groups[] = {
	"gpio19",
};
static const char * const pbs3_groups[] = {
	"gpio19",
};
static const char * const qdss_gpio3_groups[] = {
	"gpio19", "gpio110",
};
static const char * const cam_mclk_groups[] = {
	"gpio20", "gpio21", "gpio27", "gpio28",
};
static const char * const pbs4_groups[] = {
	"gpio20",
};
static const char * const adsp_ext_groups[] = {
	"gpio21",
};
static const char * const pbs5_groups[] = {
	"gpio21",
};
static const char * const cci_i2c_groups[] = {
	"gpio22", "gpio23", "gpio29", "gpio30",
};
static const char * const prng_rosc_groups[] = {
	"gpio22", "gpio23",
};
static const char * const pbs6_groups[] = {
	"gpio22",
};
static const char * const phase_flag11_groups[] = {
	"gpio22",
};
static const char * const dac_calib9_groups[] = {
	"gpio22",
};
static const char * const atest_usb20_groups[] = {
	"gpio22",
};
static const char * const pbs7_groups[] = {
	"gpio23",
};
static const char * const phase_flag12_groups[] = {
	"gpio23",
};
static const char * const dac_calib10_groups[] = {
	"gpio23",
};
static const char * const atest_usb21_groups[] = {
	"gpio23",
};
static const char * const CCI_TIMER1_groups[] = {
	"gpio24",
};
static const char * const GCC_GP1_groups[] = {
	"gpio24", "gpio86",
};
static const char * const pbs8_groups[] = {
	"gpio24",
};
static const char * const phase_flag13_groups[] = {
	"gpio24",
};
static const char * const dac_calib11_groups[] = {
	"gpio24",
};
static const char * const atest_usb22_groups[] = {
	"gpio24",
};
static const char * const cci_async_groups[] = {
	"gpio25",
};
static const char * const CCI_TIMER0_groups[] = {
	"gpio25",
};
static const char * const pbs9_groups[] = {
	"gpio25",
};
static const char * const phase_flag14_groups[] = {
	"gpio25",
};
static const char * const dac_calib12_groups[] = {
	"gpio25",
};
static const char * const atest_usb23_groups[] = {
	"gpio25",
};
static const char * const pbs10_groups[] = {
	"gpio26",
};
static const char * const phase_flag15_groups[] = {
	"gpio26",
};
static const char * const dac_calib13_groups[] = {
	"gpio26",
};
static const char * const atest_usb2_groups[] = {
	"gpio26",
};
static const char * const vsense_trigger_groups[] = {
	"gpio26",
};
static const char * const qdss_cti_groups[] = {
	"gpio27", "gpio28", "gpio72", "gpio73", "gpio96", "gpio97",
};
static const char * const CCI_TIMER2_groups[] = {
	"gpio28",
};
static const char * const phase_flag16_groups[] = {
	"gpio29",
};
static const char * const dac_calib14_groups[] = {
	"gpio29",
};
static const char * const atest_char_groups[] = {
	"gpio29",
};
static const char * const phase_flag17_groups[] = {
	"gpio30",
};
static const char * const dac_calib15_groups[] = {
	"gpio30",
};
static const char * const atest_char0_groups[] = {
	"gpio30",
};
static const char * const GP_PDM0_groups[] = {
	"gpio31", "gpio95",
};
static const char * const phase_flag18_groups[] = {
	"gpio31",
};
static const char * const dac_calib16_groups[] = {
	"gpio31",
};
static const char * const atest_char1_groups[] = {
	"gpio31",
};
static const char * const CCI_TIMER3_groups[] = {
	"gpio32",
};
static const char * const GP_PDM1_groups[] = {
	"gpio32", "gpio96",
};
static const char * const phase_flag19_groups[] = {
	"gpio32",
};
static const char * const dac_calib17_groups[] = {
	"gpio32",
};
static const char * const atest_char2_groups[] = {
	"gpio32",
};
static const char * const GP_PDM2_groups[] = {
	"gpio33", "gpio97",
};
static const char * const phase_flag20_groups[] = {
	"gpio33",
};
static const char * const dac_calib18_groups[] = {
	"gpio33",
};
static const char * const atest_char3_groups[] = {
	"gpio33",
};
static const char * const phase_flag21_groups[] = {
	"gpio35",
};
static const char * const qdss_gpio14_groups[] = {
	"gpio35", "gpio94",
};
static const char * const phase_flag22_groups[] = {
	"gpio36",
};
static const char * const qdss_gpio15_groups[] = {
	"gpio36", "gpio95",
};
static const char * const NAV_GPIO_groups[] = {
	"gpio42", "gpio47", "gpio52", "gpio95", "gpio96", "gpio97", "gpio106",
	"gpio107", "gpio108",
};
static const char * const phase_flag23_groups[] = {
	"gpio43",
};
static const char * const phase_flag24_groups[] = {
	"gpio44",
};
static const char * const phase_flag25_groups[] = {
	"gpio45",
};
static const char * const pbs14_groups[] = {
	"gpio47",
};
static const char * const vfr_1_groups[] = {
	"gpio48",
};
static const char * const pbs15_groups[] = {
	"gpio48",
};
static const char * const PA_INDICATOR_groups[] = {
	"gpio49",
};
static const char * const gsm1_tx_groups[] = {
	"gpio53",
};
static const char * const SSBI_WTR1_groups[] = {
	"gpio59", "gpio60",
};
static const char * const pll_clk_groups[] = {
	"gpio61",
};
static const char * const pll_bypassnl_groups[] = {
	"gpio62",
};
static const char * const pll_reset_groups[] = {
	"gpio63",
};
static const char * const phase_flag26_groups[] = {
	"gpio63",
};
static const char * const ddr_pxi0_groups[] = {
	"gpio63", "gpio64",
};
static const char * const gsm0_tx_groups[] = {
	"gpio64",
};
static const char * const phase_flag27_groups[] = {
	"gpio64",
};
static const char * const GCC_GP2_groups[] = {
	"gpio69", "gpio107",
};
static const char * const qdss_gpio12_groups[] = {
	"gpio69", "gpio90",
};
static const char * const ddr_pxi1_groups[] = {
	"gpio69", "gpio70",
};
static const char * const GCC_GP3_groups[] = {
	"gpio70", "gpio106",
};
static const char * const qdss_gpio13_groups[] = {
	"gpio70", "gpio91",
};
static const char * const dbg_out_groups[] = {
	"gpio71",
};
static const char * const uim2_data_groups[] = {
	"gpio72",
};
static const char * const uim2_clk_groups[] = {
	"gpio73",
};
static const char * const uim2_reset_groups[] = {
	"gpio74",
};
static const char * const uim2_present_groups[] = {
	"gpio75",
};
static const char * const uim1_data_groups[] = {
	"gpio76",
};
static const char * const uim1_clk_groups[] = {
	"gpio77",
};
static const char * const uim1_reset_groups[] = {
	"gpio78",
};
static const char * const uim1_present_groups[] = {
	"gpio79",
};
static const char * const dac_calib19_groups[] = {
	"gpio80",
};
static const char * const mdp_vsync_groups[] = {
	"gpio81", "gpio96", "gpio97",
};
static const char * const mdp_vsync_out_0_groups[] = {
	"gpio81",
};
static const char * const mdp_vsync_out_1_groups[] = {
	"gpio81",
};
static const char * const dac_calib20_groups[] = {
	"gpio81",
};
static const char * const dac_calib21_groups[] = {
	"gpio82",
};
static const char * const atest_bbrx1_groups[] = {
	"gpio86",
};
static const char * const pbs11_groups[] = {
	"gpio87",
};
static const char * const usb_phy_groups[] = {
	"gpio89",
};
static const char * const atest_bbrx0_groups[] = {
	"gpio89",
};
static const char * const mss_lte_groups[] = {
	"gpio90", "gpio91",
};
static const char * const pbs12_groups[] = {
	"gpio90",
};
static const char * const pbs13_groups[] = {
	"gpio91",
};
static const char * const wlan1_adc0_groups[] = {
	"gpio94",
};
static const char * const wlan1_adc1_groups[] = {
	"gpio95",
};
static const char * const sd_write_groups[] = {
	"gpio96",
};
static const char * const JITTER_BIST_groups[] = {
	"gpio96", "gpio97",
};
static const char * const atest_gpsadc_dtest0_native_groups[] = {
	"gpio100",
};
static const char * const atest_gpsadc_dtest1_native_groups[] = {
	"gpio101",
};
static const char * const phase_flag28_groups[] = {
	"gpio102",
};
static const char * const dac_calib22_groups[] = {
	"gpio102",
};
static const char * const ddr_pxi2_groups[] = {
	"gpio102", "gpio103",
};
static const char * const phase_flag29_groups[] = {
	"gpio103",
};
static const char * const dac_calib23_groups[] = {
	"gpio103",
};
static const char * const phase_flag30_groups[] = {
	"gpio104",
};
static const char * const dac_calib24_groups[] = {
	"gpio104",
};
static const char * const ddr_pxi3_groups[] = {
	"gpio104", "gpio105",
};
static const char * const phase_flag31_groups[] = {
	"gpio105",
};
static const char * const dac_calib25_groups[] = {
	"gpio105",
};

static const struct msm_function khaje_functions[] = {
	FUNCTION(gpio),
	FUNCTION(qup0),
	FUNCTION(usb2phy_ac),
	FUNCTION(ddr_bist),
	FUNCTION(m_voc),
	FUNCTION(phase_flag0),
	FUNCTION(qdss_gpio8),
	FUNCTION(atest_tsens),
	FUNCTION(mpm_pwr),
	FUNCTION(phase_flag1),
	FUNCTION(qdss_gpio9),
	FUNCTION(atest_tsens2),
	FUNCTION(phase_flag2),
	FUNCTION(qdss_gpio10),
	FUNCTION(dac_calib0),
	FUNCTION(atest_usb10),
	FUNCTION(phase_flag3),
	FUNCTION(qdss_gpio11),
	FUNCTION(dac_calib1),
	FUNCTION(atest_usb11),
	FUNCTION(qup1),
	FUNCTION(CRI_TRNG0),
	FUNCTION(phase_flag4),
	FUNCTION(dac_calib2),
	FUNCTION(atest_usb12),
	FUNCTION(CRI_TRNG1),
	FUNCTION(phase_flag5),
	FUNCTION(dac_calib3),
	FUNCTION(atest_usb13),
	FUNCTION(qup2),
	FUNCTION(phase_flag6),
	FUNCTION(dac_calib4),
	FUNCTION(atest_usb1),
	FUNCTION(qup3),
	FUNCTION(pbs_out),
	FUNCTION(PLL_BIST),
	FUNCTION(qdss_gpio),
	FUNCTION(tsense_pwm),
	FUNCTION(AGERA_PLL),
	FUNCTION(pbs0),
	FUNCTION(qdss_gpio0),
	FUNCTION(pbs1),
	FUNCTION(qdss_gpio1),
	FUNCTION(qup4),
	FUNCTION(tgu_ch0),
	FUNCTION(tgu_ch1),
	FUNCTION(qup5),
	FUNCTION(tgu_ch2),
	FUNCTION(phase_flag7),
	FUNCTION(qdss_gpio4),
	FUNCTION(dac_calib5),
	FUNCTION(tgu_ch3),
	FUNCTION(phase_flag8),
	FUNCTION(qdss_gpio5),
	FUNCTION(dac_calib6),
	FUNCTION(phase_flag9),
	FUNCTION(qdss_gpio6),
	FUNCTION(dac_calib7),
	FUNCTION(phase_flag10),
	FUNCTION(qdss_gpio7),
	FUNCTION(dac_calib8),
	FUNCTION(SDC2_TB),
	FUNCTION(CRI_TRNG),
	FUNCTION(pbs2),
	FUNCTION(qdss_gpio2),
	FUNCTION(SDC1_TB),
	FUNCTION(pbs3),
	FUNCTION(qdss_gpio3),
	FUNCTION(cam_mclk),
	FUNCTION(pbs4),
	FUNCTION(adsp_ext),
	FUNCTION(pbs5),
	FUNCTION(cci_i2c),
	FUNCTION(prng_rosc),
	FUNCTION(pbs6),
	FUNCTION(phase_flag11),
	FUNCTION(dac_calib9),
	FUNCTION(atest_usb20),
	FUNCTION(pbs7),
	FUNCTION(phase_flag12),
	FUNCTION(dac_calib10),
	FUNCTION(atest_usb21),
	FUNCTION(CCI_TIMER1),
	FUNCTION(GCC_GP1),
	FUNCTION(pbs8),
	FUNCTION(phase_flag13),
	FUNCTION(dac_calib11),
	FUNCTION(atest_usb22),
	FUNCTION(cci_async),
	FUNCTION(CCI_TIMER0),
	FUNCTION(pbs9),
	FUNCTION(phase_flag14),
	FUNCTION(dac_calib12),
	FUNCTION(atest_usb23),
	FUNCTION(pbs10),
	FUNCTION(phase_flag15),
	FUNCTION(dac_calib13),
	FUNCTION(atest_usb2),
	FUNCTION(vsense_trigger),
	FUNCTION(qdss_cti),
	FUNCTION(CCI_TIMER2),
	FUNCTION(phase_flag16),
	FUNCTION(dac_calib14),
	FUNCTION(atest_char),
	FUNCTION(phase_flag17),
	FUNCTION(dac_calib15),
	FUNCTION(atest_char0),
	FUNCTION(GP_PDM0),
	FUNCTION(phase_flag18),
	FUNCTION(dac_calib16),
	FUNCTION(atest_char1),
	FUNCTION(CCI_TIMER3),
	FUNCTION(GP_PDM1),
	FUNCTION(phase_flag19),
	FUNCTION(dac_calib17),
	FUNCTION(atest_char2),
	FUNCTION(GP_PDM2),
	FUNCTION(phase_flag20),
	FUNCTION(dac_calib18),
	FUNCTION(atest_char3),
	FUNCTION(phase_flag21),
	FUNCTION(qdss_gpio14),
	FUNCTION(phase_flag22),
	FUNCTION(qdss_gpio15),
	FUNCTION(NAV_GPIO),
	FUNCTION(phase_flag23),
	FUNCTION(phase_flag24),
	FUNCTION(phase_flag25),
	FUNCTION(pbs14),
	FUNCTION(vfr_1),
	FUNCTION(pbs15),
	FUNCTION(PA_INDICATOR),
	FUNCTION(gsm1_tx),
	FUNCTION(SSBI_WTR1),
	FUNCTION(pll_clk),
	FUNCTION(pll_bypassnl),
	FUNCTION(pll_reset),
	FUNCTION(phase_flag26),
	FUNCTION(ddr_pxi0),
	FUNCTION(gsm0_tx),
	FUNCTION(phase_flag27),
	FUNCTION(GCC_GP2),
	FUNCTION(qdss_gpio12),
	FUNCTION(ddr_pxi1),
	FUNCTION(GCC_GP3),
	FUNCTION(qdss_gpio13),
	FUNCTION(dbg_out),
	FUNCTION(uim2_data),
	FUNCTION(uim2_clk),
	FUNCTION(uim2_reset),
	FUNCTION(uim2_present),
	FUNCTION(uim1_data),
	FUNCTION(uim1_clk),
	FUNCTION(uim1_reset),
	FUNCTION(uim1_present),
	FUNCTION(dac_calib19),
	FUNCTION(mdp_vsync),
	FUNCTION(mdp_vsync_out_0),
	FUNCTION(mdp_vsync_out_1),
	FUNCTION(dac_calib20),
	FUNCTION(dac_calib21),
	FUNCTION(atest_bbrx1),
	FUNCTION(pbs11),
	FUNCTION(usb_phy),
	FUNCTION(atest_bbrx0),
	FUNCTION(mss_lte),
	FUNCTION(pbs12),
	FUNCTION(pbs13),
	FUNCTION(wlan1_adc0),
	FUNCTION(wlan1_adc1),
	FUNCTION(sd_write),
	FUNCTION(JITTER_BIST),
	FUNCTION(atest_gpsadc_dtest0_native),
	FUNCTION(atest_gpsadc_dtest1_native),
	FUNCTION(phase_flag28),
	FUNCTION(dac_calib22),
	FUNCTION(ddr_pxi2),
	FUNCTION(phase_flag29),
	FUNCTION(dac_calib23),
	FUNCTION(phase_flag30),
	FUNCTION(dac_calib24),
	FUNCTION(ddr_pxi3),
	FUNCTION(phase_flag31),
	FUNCTION(dac_calib25),
};

/* Every pin is maintained as a single group, and missing or non-existing pin
 * would be maintained as dummy group to synchronize pin group index with
 * pin descriptor registered with pinctrl core.
 * Clients would not be able to request these dummy pin groups.
 */
static const struct msm_pingroup khaje_groups[] = {
	[0] = PINGROUP(0, WEST, qup0, usb2phy_ac, m_voc, ddr_bist, NA,
		       phase_flag0, qdss_gpio8, atest_tsens, NA, 0x71000, 1),
	[1] = PINGROUP(1, WEST, qup0, mpm_pwr, ddr_bist, NA, phase_flag1,
		       qdss_gpio9, atest_tsens2, NA, NA, 0, -1),
	[2] = PINGROUP(2, WEST, qup0, ddr_bist, NA, phase_flag2, qdss_gpio10,
		       dac_calib0, atest_usb10, NA, NA, 0, -1),
	[3] = PINGROUP(3, WEST, qup0, ddr_bist, NA, phase_flag3, qdss_gpio11,
		       dac_calib1, atest_usb11, NA, NA, 0x71000, 2),
	[4] = PINGROUP(4, WEST, qup1, CRI_TRNG0, NA, phase_flag4, dac_calib2,
		       atest_usb12, NA, NA, NA, 0x71000, 3),
	[5] = PINGROUP(5, WEST, qup1, CRI_TRNG1, NA, phase_flag5, dac_calib3,
		       atest_usb13, NA, NA, NA, 0, -1),
	[6] = PINGROUP(6, WEST, qup2, NA, phase_flag6, dac_calib4, atest_usb1,
		       NA, NA, NA, NA, 0x71000, 4),
	[7] = PINGROUP(7, WEST, qup2, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[8] = PINGROUP(8, EAST, qup3, pbs_out, PLL_BIST, NA, qdss_gpio, NA,
		       tsense_pwm, NA, NA, 0x71000, 0),
	[9] = PINGROUP(9, EAST, qup3, pbs_out, PLL_BIST, NA, qdss_gpio, NA, NA,
		       NA, NA, 0, -1),
	[10] = PINGROUP(10, EAST, qup3, AGERA_PLL, NA, pbs0, qdss_gpio0, NA,
			NA, NA, NA, 0, -1),
	[11] = PINGROUP(11, EAST, qup3, AGERA_PLL, NA, pbs1, qdss_gpio1, NA,
			NA, NA, NA, 0x71000, 1),
	[12] = PINGROUP(12, WEST, qup4, tgu_ch0, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[13] = PINGROUP(13, WEST, qup4, tgu_ch1, NA, NA, NA, NA, NA, NA, NA,
			0x71000, 5),
	[14] = PINGROUP(14, WEST, qup5, tgu_ch2, NA, phase_flag7, qdss_gpio4,
			dac_calib5, NA, NA, NA, 0x71000, 6),
	[15] = PINGROUP(15, WEST, qup5, tgu_ch3, NA, phase_flag8, qdss_gpio5,
			dac_calib6, NA, NA, NA, 0, -1),
	[16] = PINGROUP(16, WEST, qup5, NA, phase_flag9, qdss_gpio6,
			dac_calib7, NA, NA, NA, NA, 0, -1),
	[17] = PINGROUP(17, WEST, qup5, NA, phase_flag10, qdss_gpio7,
			dac_calib8, NA, NA, NA, NA, 0x71000, 7),
	[18] = PINGROUP(18, EAST, SDC2_TB, CRI_TRNG, pbs2, qdss_gpio2, NA, NA,
			NA, NA, NA, 0x71000, 2),
	[19] = PINGROUP(19, EAST, SDC1_TB, pbs3, qdss_gpio3, NA, NA, NA, NA,
			NA, NA, 0x71000, 3),
	[20] = PINGROUP(20, EAST, cam_mclk, pbs4, qdss_gpio4, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[21] = PINGROUP(21, EAST, cam_mclk, adsp_ext, pbs5, qdss_gpio5, NA, NA,
			NA, NA, NA, 0, -1),
	[22] = PINGROUP(22, EAST, cci_i2c, prng_rosc, NA, pbs6, phase_flag11,
			qdss_gpio6, dac_calib9, atest_usb20, NA, 0, -1),
	[23] = PINGROUP(23, EAST, cci_i2c, prng_rosc, NA, pbs7, phase_flag12,
			qdss_gpio7, dac_calib10, atest_usb21, NA, 0, -1),
	[24] = PINGROUP(24, EAST, CCI_TIMER1, GCC_GP1, NA, pbs8, phase_flag13,
			qdss_gpio8, dac_calib11, atest_usb22, NA, 0x71000, 4),
	[25] = PINGROUP(25, EAST, cci_async, CCI_TIMER0, NA, pbs9,
			phase_flag14, qdss_gpio9, dac_calib12, atest_usb23, NA,
			0x71000, 5),
	[26] = PINGROUP(26, EAST, NA, pbs10, phase_flag15, qdss_gpio10,
			dac_calib13, atest_usb2, vsense_trigger, NA, NA, 0, -1),
	[27] = PINGROUP(27, EAST, cam_mclk, qdss_cti, NA, NA, NA, NA, NA, NA,
			NA, 0x71000, 6),
	[28] = PINGROUP(28, EAST, cam_mclk, CCI_TIMER2, qdss_cti, NA, NA, NA,
			NA, NA, NA, 0x71000, 7),
	[29] = PINGROUP(29, EAST, cci_i2c, NA, phase_flag16, dac_calib14,
			atest_char, NA, NA, NA, NA, 0, -1),
	[30] = PINGROUP(30, EAST, cci_i2c, NA, phase_flag17, dac_calib15,
			atest_char0, NA, NA, NA, NA, 0, -1),
	[31] = PINGROUP(31, EAST, GP_PDM0, NA, phase_flag18, dac_calib16,
			atest_char1, NA, NA, NA, NA, 0x71000, 8),
	[32] = PINGROUP(32, EAST, CCI_TIMER3, GP_PDM1, NA, phase_flag19,
			dac_calib17, atest_char2, NA, NA, NA, 0x71000, 9),
	[33] = PINGROUP(33, EAST, GP_PDM2, NA, phase_flag20, dac_calib18,
			atest_char3, NA, NA, NA, NA, 0x71000, 10),
	[34] = PINGROUP(34, EAST, NA, NA, NA, NA, NA, NA, NA, NA, NA,
			0x71000, 11),
	[35] = PINGROUP(35, EAST, NA, phase_flag21, qdss_gpio14, NA, NA, NA,
			NA, NA, NA, 0x71000, 12),
	[36] = PINGROUP(36, EAST, NA, phase_flag22, qdss_gpio15, NA, NA, NA,
			NA, NA, NA, 0x71000, 13),
	[37] = PINGROUP(37, EAST, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[38] = PINGROUP(38, EAST, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[39] = PINGROUP(39, EAST, NA, NA, NA, NA, NA, NA, NA, NA, NA,
			0x71000, 14),
	[40] = PINGROUP(40, EAST, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[41] = PINGROUP(41, EAST, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[42] = PINGROUP(42, EAST, NA, NAV_GPIO, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[43] = PINGROUP(43, EAST, NA, phase_flag23, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[44] = PINGROUP(44, EAST, NA, NA, phase_flag24, NA, NA, NA, NA, NA, NA,
			0, -1),
	[45] = PINGROUP(45, EAST, NA, NA, phase_flag25, NA, NA, NA, NA, NA, NA,
			0, -1),
	[46] = PINGROUP(46, EAST, NA, NA, NA, NA, NA, NA, NA, NA, NA,
			0x71000, 15),
	[47] = PINGROUP(47, EAST, NA, NAV_GPIO, NA, pbs14, NA, NA, NA, NA, NA,
			0, -1),
	[48] = PINGROUP(48, EAST, NA, vfr_1, NA, pbs15, NA, NA, NA, NA, NA,
			0, -1),
	[49] = PINGROUP(49, EAST, NA, PA_INDICATOR, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[50] = PINGROUP(50, EAST, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[51] = PINGROUP(51, EAST, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[52] = PINGROUP(52, EAST, NA, NAV_GPIO, pbs_out, NA, NA, NA, NA, NA,
			NA, 0, -1),
	[53] = PINGROUP(53, EAST, NA, gsm1_tx, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[54] = PINGROUP(54, EAST, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[55] = PINGROUP(55, EAST, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[56] = PINGROUP(56, EAST, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[57] = PINGROUP(57, EAST, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[58] = PINGROUP(58, EAST, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[59] = PINGROUP(59, EAST, NA, SSBI_WTR1, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[60] = PINGROUP(60, EAST, NA, SSBI_WTR1, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[61] = PINGROUP(61, EAST, NA, pll_clk, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[62] = PINGROUP(62, EAST, NA, pll_bypassnl, NA, NA, NA, NA, NA, NA, NA,
			0x71000, 16),
	[63] = PINGROUP(63, EAST, pll_reset, NA, phase_flag26, ddr_pxi0, NA,
			NA, NA, NA, NA, 0x71000, 17),
	[64] = PINGROUP(64, EAST, gsm0_tx, NA, phase_flag27, ddr_pxi0, NA, NA,
			NA, NA, NA, 0x71000, 18),
	[65] = PINGROUP(65, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA,
			0x71000, 8),
	[66] = PINGROUP(66, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA,
			0x71000, 9),
	[67] = PINGROUP(67, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA,
			0x71000, 10),
	[68] = PINGROUP(68, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[69] = PINGROUP(69, WEST, qup1, GCC_GP2, qdss_gpio12, ddr_pxi1, NA, NA,
			NA, NA, NA, 0x71000, 11),
	[70] = PINGROUP(70, WEST, qup1, GCC_GP3, qdss_gpio13, ddr_pxi1, NA, NA,
			NA, NA, NA, 0x71000, 12),
	[71] = PINGROUP(71, WEST, qup2, dbg_out, NA, NA, NA, NA, NA, NA, NA,
			0x71000, 18),
	[72] = PINGROUP(72, SOUTH, uim2_data, qdss_cti, NA, NA, NA, NA, NA, NA,
			NA, 0x71000, 3),
	[73] = PINGROUP(73, SOUTH, uim2_clk, qdss_cti, NA, NA, NA, NA, NA, NA,
			NA, 0, -1),
	[74] = PINGROUP(74, SOUTH, uim2_reset, NA, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[75] = PINGROUP(75, SOUTH, uim2_present, NA, NA, NA, NA, NA, NA, NA,
			NA, 0x71000, 4),
	[76] = PINGROUP(76, SOUTH, uim1_data, NA, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[77] = PINGROUP(77, SOUTH, uim1_clk, NA, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[78] = PINGROUP(78, SOUTH, uim1_reset, NA, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[79] = PINGROUP(79, SOUTH, uim1_present, NA, NA, NA, NA, NA, NA, NA,
			NA, 0x71000, 5),
	[80] = PINGROUP(80, WEST, qup2, dac_calib19, NA, NA, NA, NA, NA, NA,
			NA, 0x71000, 13),
	[81] = PINGROUP(81, WEST, mdp_vsync_out_0, mdp_vsync_out_1, mdp_vsync,
			dac_calib20, NA, NA, NA, NA, NA, 0x71000, 14),
	[82] = PINGROUP(82, WEST, qup0, dac_calib21, NA, NA, NA, NA, NA, NA,
			NA, 0, -1),
	[83] = PINGROUP(83, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA,
			0x71000, 15),
	[84] = PINGROUP(84, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA,
			0x71000, 16),
	[85] = PINGROUP(85, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA,
			0x71000, 17),
	[86] = PINGROUP(86, WEST, qup0, GCC_GP1, NA, atest_bbrx1, NA, NA, NA,
			NA, NA, 0, -1),
	[87] = PINGROUP(87, EAST, pbs11, qdss_gpio11, NA, NA, NA, NA, NA, NA,
			NA, 0x71000, 19),
	[88] = PINGROUP(88, EAST, NA, NA, NA, NA, NA, NA, NA, NA, NA,
			0x71000, 20),
	[89] = PINGROUP(89, WEST, usb_phy, atest_bbrx0, NA, NA, NA, NA, NA, NA,
			NA, 0x71000, 19),
	[90] = PINGROUP(90, EAST, mss_lte, pbs12, qdss_gpio12, NA, NA, NA, NA,
			NA, NA, 0, -1),
	[91] = PINGROUP(91, EAST, mss_lte, pbs13, qdss_gpio13, NA, NA, NA, NA,
			NA, NA, 0x71000, 21),
	[92] = PINGROUP(92, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[93] = PINGROUP(93, WEST, NA, NA, NA, NA, NA, NA, NA, NA, NA,
			0x71000, 20),
	[94] = PINGROUP(94, WEST, NA, NA, qdss_gpio14, wlan1_adc0, NA, NA, NA,
			NA, NA, 0x71000, 21),
	[95] = PINGROUP(95, WEST, NAV_GPIO, GP_PDM0, NA, qdss_gpio15,
			wlan1_adc1, NA, NA, NA, NA, 0x71000, 22),
	[96] = PINGROUP(96, WEST, qup4, NAV_GPIO, mdp_vsync, GP_PDM1, sd_write,
			JITTER_BIST, NA, qdss_cti, qdss_cti, 0x71000, 23),
	[97] = PINGROUP(97, WEST, qup4, NAV_GPIO, mdp_vsync, GP_PDM2,
			JITTER_BIST, NA, qdss_cti, qdss_cti, NA, 0x71000, 24),
	[98] = PINGROUP(98, SOUTH, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[99] = PINGROUP(99, SOUTH, NA, NA, NA, NA, NA, NA, NA, NA, NA,
			0x71000, 6),
	[100] = PINGROUP(100, SOUTH, atest_gpsadc_dtest0_native, NA, NA, NA,
			 NA, NA, NA, NA, NA, 0, -1),
	[101] = PINGROUP(101, SOUTH, atest_gpsadc_dtest1_native, NA, NA, NA,
			 NA, NA, NA, NA, NA, 0, -1),
	[102] = PINGROUP(102, SOUTH, phase_flag28, dac_calib22, ddr_pxi2, NA,
			 NA, NA, NA, NA, NA, 0x71000, 7),
	[103] = PINGROUP(103, SOUTH, phase_flag29, dac_calib23, ddr_pxi2, NA,
			 NA, NA, NA, NA, NA, 0x71000, 8),
	[104] = PINGROUP(104, SOUTH, phase_flag30, qdss_gpio1, dac_calib24,
			 ddr_pxi3, NA, NA, NA, NA, NA, 0x71000, 9),
	[105] = PINGROUP(105, SOUTH, phase_flag31, qdss_gpio, dac_calib25,
			 ddr_pxi3, NA, NA, NA, NA, NA, 0x71000, 10),
	[106] = PINGROUP(106, SOUTH, NAV_GPIO, GCC_GP3, qdss_gpio, NA, NA, NA,
			 NA, NA, NA, 0x71000, 11),
	[107] = PINGROUP(107, SOUTH, NAV_GPIO, GCC_GP2, qdss_gpio0, NA, NA, NA,
			 NA, NA, NA, 0x71000, 12),
	[108] = PINGROUP(108, SOUTH, NAV_GPIO, NA, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[109] = PINGROUP(109, SOUTH, qdss_gpio2, NA, NA, NA, NA, NA, NA, NA,
			 NA, 0x71000, 13),
	[110] = PINGROUP(110, SOUTH, qdss_gpio3, NA, NA, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[111] = PINGROUP(111, SOUTH, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[112] = PINGROUP(112, SOUTH, NA, NA, NA, NA, NA, NA, NA, NA, NA,
			 0x71000, 14),
	[113] = SDC_QDSD_PINGROUP(sdc1_rclk, 0x175000, 15, 0),
	[114] = SDC_QDSD_PINGROUP(sdc1_clk, 0x175000, 13, 6),
	[115] = SDC_QDSD_PINGROUP(sdc1_cmd, 0x175000, 11, 3),
	[116] = SDC_QDSD_PINGROUP(sdc1_data, 0x175000, 9, 0),
	[117] = SDC_QDSD_PINGROUP(sdc2_clk, 0x573000, 14, 6),
	[118] = SDC_QDSD_PINGROUP(sdc2_cmd, 0x573000, 11, 3),
	[119] = SDC_QDSD_PINGROUP(sdc2_data, 0x573000, 9, 0),
	[120] = UFS_RESET(ufs_reset, 0x178000),
};

static const int khaje_reserved_gpios[] = {
	0, 1, 2, 3, -1
};

static const struct msm_pinctrl_soc_data khaje_pinctrl = {
	.pins = khaje_pins,
	.npins = ARRAY_SIZE(khaje_pins),
	.functions = khaje_functions,
	.nfunctions = ARRAY_SIZE(khaje_functions),
	.groups = khaje_groups,
	.ngroups = ARRAY_SIZE(khaje_groups),
	.reserved_gpios = khaje_reserved_gpios,
	.ngpios = 113,
};

static int khaje_pinctrl_probe(struct platform_device *pdev)
{
	return msm_pinctrl_probe(pdev, &khaje_pinctrl);
}

static const struct of_device_id khaje_pinctrl_of_match[] = {
	{ .compatible = "qcom,khaje-pinctrl", },
	{ },
};

static struct platform_driver khaje_pinctrl_driver = {
	.driver = {
		.name = "khaje-pinctrl",
		.of_match_table = khaje_pinctrl_of_match,
	},
	.probe = khaje_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init khaje_pinctrl_init(void)
{
	return platform_driver_register(&khaje_pinctrl_driver);
}
arch_initcall(khaje_pinctrl_init);

static void __exit khaje_pinctrl_exit(void)
{
	platform_driver_unregister(&khaje_pinctrl_driver);
}
module_exit(khaje_pinctrl_exit);

MODULE_DESCRIPTION("QTI khaje pinctrl driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, khaje_pinctrl_of_match);
