// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: ren-ting.wang <ren-ting.wang@mediatek.com>
 */
#include <linux/of.h>

#include "mtk_clkbuf_common.h"

static const char *chip_name[CLKBUF_CHIP_ID_MAX] __initconst = {
	[MT6893] = "mediatek,mt6893",
	[MT6873] = "mediatek,mt6873",
};

int clk_buf_read_with_ofs(struct base_hw *hw, struct reg_t *reg,
	u32 *val, u32 ofs)
{
	int ret = 0;

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

enum CLKBUF_CHIP_ID clk_buf_get_chip_id(void)
{
	u32 i;

	for (i = 0; i < CLKBUF_CHIP_ID_MAX; i++)
		if (of_machine_is_compatible(chip_name[i]))
			return i;

	return -ECHIP_NOT_FOUND;
}
