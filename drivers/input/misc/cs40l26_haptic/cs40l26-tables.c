// SPDX-License-Identifier: GPL-2.0
//
// cs40l26-tables.c -- CS40L26 Boosted Haptic Driver with Integrated DSP and
// Waveform Memory with Advanced Closed Loop Algorithms and LRA protection
//
// Copyright 2022 Cirrus Logic, Inc.
//
// Author: Fred Treven <fred.treven@cirrus.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include <linux/mfd/cs40l26.h>

const struct regmap_config cs40l26_regmap = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
	.val_format_endian = REGMAP_ENDIAN_BIG,
	.max_register = CS40L26_LASTREG,
	.num_reg_defaults = 0,
	.precious_reg = cs40l26_precious_reg,
	.readable_reg = cs40l26_readable_reg,
	.volatile_reg = cs40l26_volatile_reg,
	.cache_type = REGCACHE_NONE,
};
EXPORT_SYMBOL_GPL(cs40l26_regmap);

const struct cs40l26_ls_cal_param cs40l26_ls_cal_params[CS40L26_LS_CAL_NUM_REGS] = {
	{
		.calib_name = "STATE_OL_RESULTS_REDC",
		.runtime_name = "TA_TEMP_EST_INITIAL_REDC",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_RESULTS_LE",
		.runtime_name = NULL,
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_RESULTS_F0",
		.runtime_name = NULL,
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_RESULTS_RES",
		.runtime_name = NULL,
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_RESULTS_Q",
		.runtime_name = NULL,
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_RESULTS_REDC",
		.runtime_name = NULL,
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_RESULTS_LE",
		.runtime_name = NULL,
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_RESULTS_F0",
		.runtime_name = NULL,
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_RESULTS_RES",
		.runtime_name = NULL,
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_RESULTS_Q",
		.runtime_name = NULL,
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_ZPK_BQ1Z1_REAL",
		.runtime_name = "FF_SVC_OL_ZP1_REAL",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_ZPK_BQ1Z1_IMAG",
		.runtime_name = "FF_SVC_OL_ZP1_IMAG",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_ZPK_BQ1Z2_REAL",
		.runtime_name = "FF_SVC_OL_ZP2_REAL",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_ZPK_BQ1Z2_IMAG",
		.runtime_name = "FF_SVC_OL_ZP2_IMAG",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_ZPK_BQ1P1_REAL",
		.runtime_name = "FF_SVC_OL_ZP3_REAL",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_ZPK_BQ1P1_IMAG",
		.runtime_name = "FF_SVC_OL_ZP3_IMAG",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_ZPK_BQ1P2_REAL",
		.runtime_name = "FF_SVC_OL_ZP4_REAL",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_ZPK_BQ1P2_IMAG",
		.runtime_name = "FF_SVC_OL_ZP4_IMAG",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_ZPK_BQ2Z1_REAL",
		.runtime_name = "FF_SVC_OL_ZP5_REAL",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_ZPK_BQ2Z1_IMAG",
		.runtime_name = "FF_SVC_OL_ZP5_IMAG",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_ZPK_BQ2Z2_REAL",
		.runtime_name = "FF_SVC_OL_ZP6_REAL",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_ZPK_BQ2Z2_IMAG",
		.runtime_name = "FF_SVC_OL_ZP6_IMAG",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_ZPK_BQ2P1_REAL",
		.runtime_name = "FF_SVC_OL_ZP7_REAL",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_ZPK_BQ2P1_IMAG",
		.runtime_name = "FF_SVC_OL_ZP7_IMAG",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_ZPK_BQ2P2_REAL",
		.runtime_name = "FF_SVC_OL_ZP8_REAL",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_ZPK_BQ2P2_IMAG",
		.runtime_name = "FF_SVC_OL_ZP8_IMAG",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_ZPK_GAIN",
		.runtime_name = "FF_SVC_OL_K",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_OL_ZPK_GAIN",
		.runtime_name = "FF_SVC_OL_K",
		.word_num = 2,
	},
	{
		.calib_name = "STATE_CL_ZPK_BQ1Z1_REAL",
		.runtime_name = "FF_SVC_CL_ZP1_REAL",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_ZPK_BQ1Z1_IMAG",
		.runtime_name = "FF_SVC_CL_ZP1_IMAG",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_ZPK_BQ1Z2_REAL",
		.runtime_name = "FF_SVC_CL_ZP2_REAL",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_ZPK_BQ1Z2_IMAG",
		.runtime_name = "FF_SVC_CL_ZP2_IMAG",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_ZPK_BQ1P1_REAL",
		.runtime_name = "FF_SVC_CL_ZP3_REAL",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_ZPK_BQ1P1_IMAG",
		.runtime_name = "FF_SVC_CL_ZP3_IMAG",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_ZPK_BQ1P2_REAL",
		.runtime_name = "FF_SVC_CL_ZP4_REAL",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_ZPK_BQ1P2_IMAG",
		.runtime_name = "FF_SVC_CL_ZP4_IMAG",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_ZPK_BQ2Z1_REAL",
		.runtime_name = "FF_SVC_CL_ZP5_REAL",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_ZPK_BQ2Z1_IMAG",
		.runtime_name = "FF_SVC_CL_ZP5_IMAG",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_ZPK_BQ2Z2_REAL",
		.runtime_name = "FF_SVC_CL_ZP6_REAL",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_ZPK_BQ2Z2_IMAG",
		.runtime_name = "FF_SVC_CL_ZP6_IMAG",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_ZPK_BQ2P1_REAL",
		.runtime_name = "FF_SVC_CL_ZP7_REAL",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_ZPK_BQ2P1_IMAG",
		.runtime_name = "FF_SVC_CL_ZP7_IMAG",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_ZPK_BQ2P2_REAL",
		.runtime_name = "FF_SVC_CL_ZP8_REAL",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_ZPK_BQ2P2_IMAG",
		.runtime_name = "FF_SVC_CL_ZP8_IMAG",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_ZPK_GAIN",
		.runtime_name = "FF_SVC_CL_K",
		.word_num = 1,
	},
	{
		.calib_name = "STATE_CL_ZPK_GAIN",
		.runtime_name = "FF_SVC_CL_K",
		.word_num = 2,
	},
	{
		.calib_name = "STATE_TEMPERATURE",
		.runtime_name = "TA_TEMP_EST_INITIAL_TEMP",
		.word_num = 1,
	},
};
EXPORT_SYMBOL_GPL(cs40l26_ls_cal_params);

const struct reg_sequence cs40l26_a1_errata[CS40L26_ERRATA_A1_NUM_WRITES] = {
	{ CS40L26_PLL_REFCLK_DETECT_0, 0x00000000 },
	{ CS40L26_TEST_KEY_CTRL, CS40L26_TEST_KEY_UNLOCK_CODE1 },
	{ CS40L26_TEST_KEY_CTRL, CS40L26_TEST_KEY_UNLOCK_CODE2 },
	{ CS40L26_TEST_LBST, CS40L26_DISABLE_EXPL_MODE },
	{ CS40L26_TEST_KEY_CTRL, CS40L26_TEST_KEY_LOCK_CODE },
};

struct regulator_bulk_data cs40l26_supplies[CS40L26_NUM_SUPPLIES] = {
	{ .supply = CS40L26_VP_SUPPLY_NAME },
	{ .supply = CS40L26_VA_SUPPLY_NAME },
};

const struct mfd_cell cs40l26_devs[CS40L26_NUM_MFD_DEVS] = {
	{ .name = "cs40l26-codec" },
};

const u32 cs40l26_attn_q21_2_vals[CS40L26_NUM_PCT_MAP_VALUES] = {
	400, /* MUTE */
	160, /* 1% */
	136,
	122,
	112,
	104,
	98,
	92,
	88,
	84,
	80,
	77,
	74,
	71,
	68,
	66,
	64,
	62,
	60,
	58,
	56,
	54,
	53,
	51,
	50,
	48, /* 25% */
	47,
	45,
	44,
	43,
	42,
	41,
	40,
	39,
	37,
	36,
	35,
	35,
	34,
	33,
	32,
	31,
	30,
	29,
	29,
	28,
	27,
	26,
	26,
	25,
	24, /* 50 % */
	23,
	23,
	22,
	21,
	21,
	20,
	20,
	19,
	18,
	18,
	17,
	17,
	16,
	16,
	15,
	14,
	14,
	13,
	13,
	12,
	12,
	11,
	11,
	10,
	10, /* 75% */
	10,
	9,
	9,
	8,
	8,
	7,
	7,
	6,
	6,
	6,
	5,
	5,
	4,
	4,
	4,
	3,
	3,
	3,
	2,
	2,
	1,
	1,
	1,
	0,
	0, /* 100% */
};

bool cs40l26_precious_reg(struct device *dev, unsigned int reg)
{
	return false;
}
EXPORT_SYMBOL_GPL(cs40l26_precious_reg);

bool cs40l26_volatile_reg(struct device *dev, unsigned int reg)
{
	return false;
}
EXPORT_SYMBOL_GPL(cs40l26_volatile_reg);

bool cs40l26_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CS40L26_DEVID:
	case CS40L26_REVID:
	case CS40L26_TEST_KEY_CTRL:
	case CS40L26_GLOBAL_ENABLES:
	case CS40L26_BLOCK_ENABLES2:
	case CS40L26_ERROR_RELEASE:
	case CS40L26_GPIO_PAD_CONTROL:
	case CS40L26_PWRMGT_CTL:
	case CS40L26_PWRMGT_STS:
	case CS40L26_REFCLK_INPUT:
	case CS40L26_GLOBAL_SAMPLE_RATE:
	case CS40L26_PLL_REFCLK_DETECT_0:
	case CS40L26_VBST_CTL_1:
	case CS40L26_VBST_CTL_2:
	case CS40L26_BST_IPK_CTL:
	case CS40L26_BST_DCM_CTL:
	case CS40L26_TEST_LBST:
	case CS40L26_MONITOR_FILT:
	case CS40L26_SPKMON_VMON_DEC_OUT_DATA:
	case CS40L26_ENABLES_AND_CODES_DIG:
	case CS40L26_ASP_ENABLES1:
	case CS40L26_ASP_CONTROL2:
	case CS40L26_ASP_FRAME_CONTROL5:
	case CS40L26_ASP_DATA_CONTROL5:
	case CS40L26_DACPCM1_INPUT:
	case CS40L26_ASPTX1_INPUT:
	case CS40L26_DSP1RX1_INPUT:
	case CS40L26_DSP1RX5_INPUT:
	case CS40L26_NGATE1_INPUT:
	case CS40L26_VPBR_CONFIG:
	case CS40L26_VBBR_CONFIG:
	case CS40L26_VPBR_STATUS:
	case CS40L26_VBBR_STATUS:
	case CS40L26_NG_CONFIG:
	case CS40L26_DIGPWM_CONFIG2:
	case CS40L26_TST_DAC_MSM_CONFIG:
	case CS40L26_ALIVE_DCIN_WD:
	case CS40L26_IRQ1_CFG:
	case CS40L26_IRQ1_STATUS:
	case CS40L26_IRQ1_EINT_1:
	case CS40L26_IRQ1_EINT_2:
	case CS40L26_IRQ1_STS_1:
	case CS40L26_IRQ1_STS_2:
	case CS40L26_IRQ1_MASK_1:
	case CS40L26_IRQ1_MASK_2:
	case CS40L26_GPIO1_CTRL1:
	case CS40L26_MIXER_NGATE_CH1_CFG:
	case CS40L26_DSP_MBOX_1 ... CS40L26_DSP_VIRTUAL1_MBOX_1:
	case CS40L26_DSP1_XMEM_PACKED_0 ... CS40L26_DSP1_XMEM_PACKED_6143:
	case CS40L26_DSP1_XROM_PACKED_0 ... CS40L26_DSP1_XROM_PACKED_4604:
	case CS40L26_DSP1_XMEM_UNPACKED32_0 ... CS40L26_DSP1_XROM_UNPACKED32_3070:
	case CS40L26_DSP1_XMEM_UNPACKED24_0 ... CS40L26_DSP1_XMEM_UNPACKED24_8191:
	case CS40L26_DSP1_XROM_UNPACKED24_0 ... CS40L26_DSP1_XROM_UNPACKED24_6141:
	case CS40L26_DSP1_CCM_CORE_CONTROL:
	case CS40L26_DSP1_YMEM_PACKED_0 ... CS40L26_DSP1_YMEM_PACKED_1532:
	case CS40L26_DSP1_YMEM_UNPACKED32_0 ... CS40L26_DSP1_YMEM_UNPACKED32_1022:
	case CS40L26_DSP1_YMEM_UNPACKED24_0 ... CS40L26_DSP1_YMEM_UNPACKED24_2045:
	case CS40L26_DSP1_PMEM_0 ... CS40L26_DSP1_PMEM_5114:
	case CS40L26_DSP1_PROM_0 ... CS40L26_DSP1_PROM_30714:
		return true;
	default:
		return false;
	}
}
EXPORT_SYMBOL_GPL(cs40l26_readable_reg);
