// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: ren-ting.wang <ren-ting.wang@mediatek.com>
 */
#include <linux/of.h>
#include "mtk_clkbuf_common.h"

int clk_buf_read_with_ofs(struct base_hw *hw, struct reg_t *reg,
	u32 *val, u32 ofs)
{
	int ret = 0;

	if (!reg)
		return -EREG_NOT_SUPPORT;

	if (!reg->mask)
		return -EREG_NOT_SUPPORT;
	if (!(hw->enable))
		return -EHW_NOT_SUPPORT;

	if (hw->is_pmic) {
		ret = regmap_read(hw->base.map, reg->ofs + ofs, val);
		if (ret) {
			pr_notice("clkbuf read failed: %d\n", ret);
			return ret;
		}
		(*val) = ((*val) & (reg->mask << reg->shift)) >> reg->shift;
	} else {
		(*val) = (readl(hw->base.addr + reg->ofs + ofs)
			&(reg->mask << reg->shift)) >> reg->shift;
	}

	return ret;
}
EXPORT_SYMBOL(clk_buf_read_with_ofs);

int clk_buf_write_with_ofs(struct base_hw *hw, struct reg_t *reg,
	u32 val, u32 ofs)
{
	int ret = 0;
	u32 mask;

	if (!reg->mask)
		return -EREG_NOT_SUPPORT;
	if (!(hw->enable))
		return -EHW_NOT_SUPPORT;

	if (hw->is_pmic) {
		mask = reg->mask << reg->shift;
		val <<= reg->shift;
		ret = regmap_update_bits(hw->base.map, reg->ofs + ofs,
			mask, val);
		if (ret) {
			pr_notice("clkbuf write failed: %d\n", ret);
			return ret;
		}
	} else {
		writel((readl(hw->base.addr + reg->ofs + ofs)
			&(~(reg->mask << reg->shift))) | (val << reg->shift),
			hw->base.addr + reg->ofs + ofs);
	}

	return ret;
}
EXPORT_SYMBOL(clk_buf_write_with_ofs);
