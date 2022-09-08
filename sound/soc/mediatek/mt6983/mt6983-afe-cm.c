// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 * Author: yiwen chiou<yiwen.chiou@mediatek.com
 */
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <sound/soc.h>

#include "../common/mtk-afe-fe-dai.h"
#include "../common/mtk-base-afe.h"

#include "mt6983-afe-cm.h"
#include "mt6983-afe-common.h"

static int mtk_convert_cm_rate(unsigned int rate)
{
	switch (rate) {
	case MTK_AFE_RATE_8K:
	case MTK_AFE_RATE_11K:
	case MTK_AFE_RATE_12K:
	case MTK_AFE_RATE_384K:
	case MTK_AFE_RATE_16K:
	case MTK_AFE_RATE_22K:
	case MTK_AFE_RATE_24K:
	case MTK_AFE_RATE_32K:
	case MTK_AFE_RATE_44K:
	case MTK_AFE_RATE_48K:
	case MTK_AFE_RATE_88K:
	case MTK_AFE_RATE_96K:
	case MTK_AFE_RATE_176K:
	case MTK_AFE_RATE_192K:
		return rate;
	case MTK_AFE_RATE_352K:
	case MTK_AFE_RATE_260K:
	default:
		return 0;
	}
	return 0;
}

static int mtk_convert_cm_ch(unsigned int ch)
{
	return ch - 1;
}

int mtk_set_cm(struct mtk_base_afe *afe, int id, unsigned int rate,
	       unsigned int update, bool swap, unsigned int ch)
{
	int cm_reg = 0;
	int cm_rate_mask = 0;
	int cm_update_mask = 0;
	int cm_swap_mask = 0;
	int cm_ch_mask = 0;
	int cm_rate_shift = 0;
	int cm_update_shift = 0;
	int cm_swap_shift = 0;
	int cm_ch_shift = 0;

	pr_info("%s()-0, CM%d, rate %d, update %d, swap %d, ch %d\n",
		__func__, id+1, rate, update, swap, ch);

	cm_rate_mask = AFE_CM_1X_EN_SEL_FS_MASK;
	cm_rate_shift = AFE_CM_1X_EN_SEL_FS;
	cm_update_mask = AFE_CM_UPDATE_CNT_MASK;
	cm_update_shift = AFE_CM_UPDATE_CNT;
	cm_swap_mask = AFE_CM_BYTE_SWAP_MASK;
	cm_swap_shift = AFE_CM_BYTE_SWAP;
	cm_ch_mask = AFE_CM_CH_NUM_MASK;
	cm_ch_shift = AFE_CM_CH_NUM;

	switch (id) {
	case CM1:
		cm_reg = AFE_CM1_CON0;
		break;
	case CM2:
		cm_reg = AFE_CM2_CON0;
		break;
	default:
		pr_info("%s(), CM%d not found\n", __func__, id+1);
		return 0;
	}

	/* update cnt */
	mtk_regmap_update_bits(afe->regmap, cm_reg,
			       cm_update_mask, update, cm_update_shift);

	/* rate */
	rate = mtk_convert_cm_rate(rate);
	mtk_regmap_update_bits(afe->regmap, cm_reg,
			       cm_rate_mask, rate, cm_rate_shift);

	/* ch num */
	ch = mtk_convert_cm_ch(ch);
	mtk_regmap_update_bits(afe->regmap, cm_reg,
			       cm_ch_mask, ch, cm_ch_shift);

	/* swap */
	mtk_regmap_update_bits(afe->regmap, cm_reg,
			       cm_swap_mask, swap, cm_swap_shift);

	pr_info("%s()-1, CM%d, rate %d, update %d, swap %d, ch %d\n",
		__func__, id+1, rate, update, swap, ch);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_set_cm);

mtk_enable_cm_bypass(struct mtk_base_afe *afe, int id, bool en1, bool en2)
{
	int cm_reg = 0;
	int cm_on1_mask = 0, cm_on2_mask = 0;
	int cm_on1_shift = 0, cm_on2_shift = 0;

	pr_info("%s, CM%d, en %d/%d\n", __func__, id+1, en1, en2);

	switch (id) {
	case CM1:
		cm_reg = AFE_CM1_CON0;
		cm_on1_mask = AFE_CM1_VUL8_BYPASS_CM_MASK;
		cm_on1_shift = AFE_CM1_VUL8_BYPASS_CM;
		cm_on2_mask = AFE_CM1_VUL9_BYPASS_CM_MASK;
		cm_on2_shift = AFE_CM1_VUL9_BYPASS_CM;
		break;
	case CM2:
		cm_reg = AFE_CM2_CON0;
		cm_on1_mask = -1;
		cm_on1_shift = -1;
		cm_on2_mask = AFE_CM2_AWB2_TINY_BYPASS_CM_MASK;
		cm_on2_shift = AFE_CM2_AWB2_TINY_BYPASS_CM;
		break;
	default:
		pr_info("%s(), CM%d not found\n", __func__, id+1);
		return 0;
	}

	if (cm_on1_shift > 0)
		mtk_regmap_update_bits(afe->regmap, cm_reg, cm_on1_mask,
				       en1, cm_on1_shift);
	mtk_regmap_update_bits(afe->regmap, cm_reg, cm_on2_mask,
			       en2, cm_on2_shift);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_enable_cm_bypass);

int mtk_enable_cm(struct mtk_base_afe *afe, int id, bool en)
{
	int cm_reg = 0;
	int cm_on_mask = 0;
	int cm_on_shift = 0;

	cm_on_mask = AFE_CM_ON_MASK;
	cm_on_shift = AFE_CM_ON;

	switch (id) {
	case CM1:
		cm_reg = AFE_CM1_CON0;
		break;
	case CM2:
		cm_reg = AFE_CM2_CON0;
		break;
	default:
		dev_info(afe->dev, "%s(), CM%d not found\n",
			 __func__, id+1);
		return 0;
	}
	mtk_regmap_update_bits(afe->regmap, cm_reg, cm_on_mask,
			       en, cm_on_shift);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_enable_cm);

int mt6983_is_need_enable_cm(struct mtk_base_afe *afe, int id)
{
	int cm_reg = 0;
	int cm_on_mask = 0;
	int cm_on_shift = 0;
	unsigned int value = 0;

	switch (id) {
	case CM1:
		cm_reg = AFE_CM1_CON0;
		cm_on_mask = AFE_CM1_VUL9_BYPASS_CM_MASK_SFT |
			     AFE_CM1_VUL8_BYPASS_CM_MASK_SFT;
		cm_on_shift = AFE_CM1_VUL8_BYPASS_CM;

		regmap_read(afe->regmap, cm_reg, &value);
		value &= cm_on_mask;
		value >>= cm_on_shift;

		pr_info("%s(), CM%d value %d\n", __func__, id+1, value);
		if (value != 0x3)
			return true;

		break;
	case CM2:
		cm_reg = AFE_CM2_CON0;
		cm_on_mask = AFE_CM2_AWB2_TINY_BYPASS_CM_MASK_SFT;
		cm_on_shift = AFE_CM2_AWB2_TINY_BYPASS_CM;

		regmap_read(afe->regmap, cm_reg, &value);
		value &= cm_on_mask;
		value >>= cm_on_shift;

		pr_info("%s(), CM%d value %d\n", __func__, id+1, value);
		if (value != 0x1)
			return true;

		break;
	default:
		pr_info("%s(), CM%d not found\n", __func__, id+1);
		return 0;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mt6983_is_need_enable_cm);

MODULE_DESCRIPTION("Mediatek afe cm");
MODULE_AUTHOR("yiwen chiou<yiwen.chiou@mediatek.com>");
MODULE_LICENSE("GPL v2");
