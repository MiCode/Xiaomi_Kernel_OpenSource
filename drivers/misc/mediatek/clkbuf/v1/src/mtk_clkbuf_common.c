// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: ren-ting.wang <ren-ting.wang@mediatek.com>
 */
#include <linux/of.h>

#include "mtk_clkbuf_common.h"

static const char *chip_name[CLKBUF_CHIP_ID_MAX] __initconst = {
	[MT6765] = "mediatek,mt6765",
	[MT6768] = "mediatek,mt6768",
	[MT6789] = "mediatek,mt6789",
	[MT6833] = "mediatek,mt6833",
	[MT6855] = "mediatek,mt6855",
	[MT6873] = "mediatek,mt6873",
	[MT6879] = "mediatek,mt6879",
	[MT6893] = "mediatek,mt6893",
	[MT6895] = "mediatek,mt6895",
	[MT6983] = "mediatek,mt6983",
};

static const char *pmic_name[CLKBUF_PMIC_ID_MAX] __initconst = {
	[MT6357] = "mediatek,mt6357",
	[MT6358] = "mediatek,mt6358",
	[MT6359P] = "mediatek,mt6359p",
	[MT6366] = "mediatek,mt6366",
	[MT6685] = "mediatek,mt6685",
};

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

enum CLKBUF_PMIC_ID clk_buf_get_pmic_id(struct platform_device *clkbuf_pdev)
{
	u32 i;
	struct device_node *pmic_node = clkbuf_pdev->dev.of_node->parent;

	for (i = 0; i < CLKBUF_PMIC_ID_MAX; i++)
		if (of_device_is_compatible(pmic_node, pmic_name[i]))
			return i;

	return -ECHIP_NOT_FOUND;
}
