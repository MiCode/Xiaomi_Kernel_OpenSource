/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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
#include "aqt1000-registers.h"
#include "aqt1000-reg-defaults.h"
#include "aqt1000-internal.h"

static bool aqt1000_is_readable_register(struct device *dev, unsigned int reg)
{
	u8 pg_num, reg_offset;
	const u8 *reg_tbl = NULL;

	/*
	 * Get the page number from MSB of codec register. If its 0x80, assign
	 * the corresponding page index PAGE_0x80.
	 */
	pg_num = reg >> 0x8;
	if (pg_num == 0x80)
		pg_num = AQT1000_PAGE_128;
	else if (pg_num > 15)
		return false;

	reg_tbl = aqt1000_reg[pg_num];
	reg_offset = reg & 0xFF;

	if (reg_tbl && reg_tbl[reg_offset])
		return true;
	else
		return false;
}

static bool aqt1000_is_volatile_register(struct device *dev, unsigned int reg)
{
	u8 pg_num, reg_offset;
	const u8 *reg_tbl = NULL;

	pg_num = reg >> 0x8;
	if (pg_num == 0x80)
		pg_num = AQT1000_PAGE_128;
	else if (pg_num > 15)
		return false;

	reg_tbl = aqt1000_reg[pg_num];
	reg_offset = reg & 0xFF;

	if (reg_tbl && reg_tbl[reg_offset] == AQT1000_RO)
		return true;

	/* IIR Coeff registers are not cacheable */
	if ((reg >= AQT1000_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL) &&
	    (reg <= AQT1000_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL))
		return true;

	if ((reg >= AQT1000_CDC_ANC0_IIR_COEFF_1_CTL) &&
	    (reg <= AQT1000_CDC_ANC0_FB_GAIN_CTL))
		return true;

	if ((reg >= AQT1000_CDC_ANC1_IIR_COEFF_1_CTL) &&
	    (reg <= AQT1000_CDC_ANC1_FB_GAIN_CTL))
		return true;

	/*
	 * Need to mark volatile for registers that are writable but
	 * only few bits are read-only
	 */
	switch (reg) {
	case AQT1000_BUCK_5V_CTRL_CCL_1:
	case AQT1000_BIAS_CCOMP_FINE_ADJ:
	case AQT1000_ANA_BIAS:
	case AQT1000_BUCK_5V_IBIAS_CTL_4:
	case AQT1000_BUCK_5V_CTRL_CCL_2:
	case AQT1000_CHIP_CFG0_RST_CTL:
	case AQT1000_CHIP_CFG0_CLK_CTL_CDC_DIG:
	case AQT1000_CHIP_CFG0_CLK_CFG_MCLK:
	case AQT1000_CHIP_CFG0_EFUSE_CTL:
	case AQT1000_CDC_CLK_RST_CTRL_FS_CNT_CONTROL:
	case AQT1000_CDC_CLK_RST_CTRL_MCLK_CONTROL:
	case AQT1000_ANA_RX_SUPPLIES:
	case AQT1000_ANA_MBHC_MECH:
	case AQT1000_ANA_MBHC_ELECT:
	case AQT1000_ANA_MBHC_ZDET:
	case AQT1000_ANA_MICB1:
	case AQT1000_BUCK_5V_EN_CTL:
		return true;
	}

	return false;
}

struct regmap_config aqt1000_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = aqt1000_defaults,
	.num_reg_defaults = ARRAY_SIZE(aqt1000_defaults),
	.max_register = AQT1000_MAX_REGISTER,
	.volatile_reg = aqt1000_is_volatile_register,
	.readable_reg = aqt1000_is_readable_register,
};
