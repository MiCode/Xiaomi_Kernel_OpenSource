// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Owen Chen <Owen.Chen@mediatek.com>
 */
#include <linux/ktime.h>
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/soc/mediatek/scpsys-ext.h>

#define MTK_POLL_DELAY_US   10
#define MTK_POLL_TIMEOUT    USEC_PER_SEC

static int enable_way_en(struct regmap *map, struct regmap *ack, u32 mask,
				u32 ack_mask, u32 reg_set, u32 reg_sta,
				u32 reg_en)
{
	u32 val;

	if (reg_set)
		regmap_write(map, reg_set, mask);
	else
		regmap_update_bits(map, reg_en, mask, mask);

	return regmap_read_poll_timeout(ack, reg_sta,
			val, (val & ack_mask) == ack_mask,
			MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
}

static int disable_way_en(struct regmap *map, struct regmap *ack, u32 mask,
				u32 ack_mask, u32 reg_clr, u32 reg_sta,
				u32 reg_en)
{
	u32 val;

	if (reg_clr)
		regmap_write(map, reg_clr, mask);
	else
		regmap_update_bits(map, reg_en, mask, 0);

	return regmap_read_poll_timeout(ack, reg_sta,
			val, (val & ack_mask) == ack_mask,
			MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
}

static int set_bus_protection(struct regmap *map, u32 mask, u32 ack_mask,
				u32 reg_set, u32 reg_sta, u32 reg_en)
{
	u32 val;

	if (reg_set)
		regmap_write(map, reg_set, mask);
	else
		regmap_update_bits(map, reg_en, mask, mask);

	return regmap_read_poll_timeout(map, reg_sta,
			val, (val & ack_mask) == ack_mask,
			MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
}

static int clear_bus_protection(struct regmap *map, u32 mask, u32 ack_mask,
				u32 reg_clr, u32 reg_sta, u32 reg_en)
{
	u32 val;

	if (reg_clr)
		regmap_write(map, reg_clr, mask);
	else
		regmap_update_bits(map, reg_en, mask, 0);

	return regmap_read_poll_timeout(map, reg_sta,
			val, !(val & ack_mask),
			MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
}

int mtk_scpsys_ext_set_bus_protection(const struct bus_prot *bp_table,
	struct regmap *infracfg, struct regmap *smi_common,
	struct regmap *infracfg_nao)
{
	int i;

	for (i = 0; i < MAX_STEPS && bp_table[i].mask; i++) {
		int ret;

		switch (bp_table[i].type) {
		case IFR_TYPE:
			ret = set_bus_protection(infracfg, bp_table[i].mask,
					bp_table[i].mask, bp_table[i].set_ofs,
					bp_table[i].sta_ofs,
					bp_table[i].en_ofs);
			break;
		case SMI_TYPE:
			ret = set_bus_protection(smi_common, bp_table[i].mask,
					bp_table[i].mask, bp_table[i].set_ofs,
					bp_table[i].sta_ofs,
					bp_table[i].en_ofs);
			break;
		case IFR_WAYEN_TYPE:
			ret = disable_way_en(infracfg, infracfg_nao,
					bp_table[i].mask,
					bp_table[i].clr_ack_mask,
					bp_table[i].clr_ofs,
					bp_table[i].sta_ofs,
					bp_table[i].en_ofs);
			break;
		default:
			return -EINVAL;
		}

		if (ret)
			return ret;
	}

	return 0;
}

int mtk_scpsys_ext_clear_bus_protection(const struct bus_prot *bp_table,
	struct regmap *infracfg, struct regmap *smi_common,
	struct regmap *infracfg_nao)
{
	int i;

	for (i = MAX_STEPS - 1; i >= 0; i--) {
		int ret;

		switch (bp_table[i].type) {
		case IFR_TYPE:
			ret = clear_bus_protection(infracfg, bp_table[i].mask,
						bp_table[i].clr_ack_mask,
						bp_table[i].clr_ofs,
						bp_table[i].sta_ofs,
						bp_table[i].en_ofs);
			break;
		case SMI_TYPE:
			ret = clear_bus_protection(smi_common,
						bp_table[i].mask,
						bp_table[i].clr_ack_mask,
						bp_table[i].clr_ofs,
						bp_table[i].sta_ofs,
						bp_table[i].en_ofs);
			break;
		case IFR_WAYEN_TYPE:
			ret = enable_way_en(infracfg, infracfg_nao,
						bp_table[i].mask,
						bp_table[i].set_ack_mask,
						bp_table[i].set_ofs,
						bp_table[i].sta_ofs,
						bp_table[i].en_ofs);
			break;
		default:
			return -EINVAL;
		}

		if (ret)
			return ret;
	}

	return 0;
}
