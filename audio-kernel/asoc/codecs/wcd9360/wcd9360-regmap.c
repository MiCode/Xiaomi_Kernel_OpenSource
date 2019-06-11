/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#include <linux/regmap.h>
#include <linux/device.h>
#include <asoc/wcd9360-registers.h>
#include "../core.h"
#include "../wcd9xxx-regmap.h"
#include "wcd9360-defaults.h"

static bool wcd9360_is_readable_register(struct device *dev, unsigned int reg)
{
	u8 pg_num, reg_offset;
	const u8 *reg_tbl = NULL;

	/*
	 * Get the page number from MSB of codec register. If its 0x80, assign
	 * the corresponding page index PAGE_0x80.
	 */
	pg_num = reg >> 8;
	if (pg_num == 128)
		pg_num = WCD9360_PAGE_128;
	else if (pg_num == 80)
		pg_num = WCD9360_PAGE_80;
	else if (pg_num > 15)
		return false;

	reg_tbl = wcd9360_reg[pg_num];
	reg_offset = reg & 0xFF;

	if (reg_tbl && reg_tbl[reg_offset])
		return true;
	else
		return false;
}

static bool wcd9360_is_volatile_register(struct device *dev, unsigned int reg)
{
	u8 pg_num, reg_offset;
	const u8 *reg_tbl = NULL;

	pg_num = reg >> 8;

	if (pg_num == 1 || pg_num == 2 ||
		pg_num == 6 || pg_num == 7)
		return true;
	else if (pg_num == 128)
		pg_num = WCD9360_PAGE_128;
	else if (pg_num == 80)
		pg_num = WCD9360_PAGE_80;
	else if (pg_num > 15)
		return false;

	reg_tbl = wcd9360_reg[pg_num];
	reg_offset = reg & 0xFF;

	if (reg_tbl && reg_tbl[reg_offset] == WCD9360_RO)
		return true;

	if ((reg >= WCD9360_CODEC_RPM_RST_CTL) &&
		(reg <= WCD9360_CHIP_TIER_CTRL_ALT_FUNC_EN))
		return true;

	if ((reg >= WCD9360_CDC_ANC0_IIR_COEFF_1_CTL) &&
	    (reg <= WCD9360_CDC_ANC0_FB_GAIN_CTL))
		return true;

	if ((reg >= WCD9360_CODEC_CPR_WR_DATA_0) &&
	    (reg <= WCD9360_CODEC_CPR_RD_DATA_3))
		return true;

	/*
	 * Need to mark volatile for registers that are writable but
	 * only few bits are read-only
	 */
	switch (reg) {
	case WCD9360_CODEC_RPM_CLK_BYPASS:
	case WCD9360_CODEC_RPM_CLK_GATE:
	case WCD9360_CODEC_RPM_CLK_MCLK_CFG:
	case WCD9360_CODEC_CPR_SVS_CX_VDD:
	case WCD9360_CODEC_CPR_SVS2_CX_VDD:
	case WCD9360_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL:
	case WCD9360_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL:
		return true;
	}

	return false;
}

struct regmap_config wcd9360_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = wcd9360_defaults,
	.num_reg_defaults = ARRAY_SIZE(wcd9360_defaults),
	.max_register = WCD9360_MAX_REGISTER,
	.volatile_reg = wcd9360_is_volatile_register,
	.readable_reg = wcd9360_is_readable_register,
	.can_multi_write = true,
};
