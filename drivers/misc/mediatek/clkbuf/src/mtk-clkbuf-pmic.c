// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: owen.chen <owen.chen@mediatek.com>
 */

/*
 * @file    mtk-clk-buf-hw.c
 * @brief   Driver for clock buffer control of each platform
 *
 */
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/list.h>

#include "mtk_clkbuf_pmic.h"
#include <mtk-clkbuf-bridge.h>

struct list_head dts_head;

static const char *PMIC_NAME[PMIC_NUM] = {
	"mediatek,mt6357",
	"mediatek,mt6359",
};

static const char *pmic_clkbuf_prop[PMIC_HW_DTS_NUM] = {
	[PMIC_DRV_CURR] = "pmic-drvcurr",
	[PMIC_HW_BBLPM] = "pmic-bblplm-hw",
	[PMIC_HW_BBLPM_SEL] = "pmic-bblpm-sel",
	[PMIC_LDO_VRFCK] = "pmic-ldo-vrfck",
	[PMIC_LDO_VBBCK] = "pmic-ldo-vbbck",
	[PMIC_AUXOUT_SEL] = "pmic-auxout-sel",
	[PMIC_AUXOUT_XO] = "pmic-auxout-xo",
	[PMIC_AUXOUT_DRV_CURR] = "pmic-auxout-drvcurr",
	[PMIC_AUXOUT_BBLPM_EN] = "pmic-auxout-bblpm-en",
	[PMIC_AUXOUT_BBLPM_O] = "pmic-auxout-bblpm-o",
	[PMIC_COFST_FPM] = "pmic-cofst-fpm",
	[PMIC_CDAC_FPM] = "pmic-cdac-fpm",
	[PMIC_CORE_IDAC_FPM] = "pmic-core-idac-fpm",
	[PMIC_AAC_FPM_SWEN] = "pmic-aac-fpm-swen",
};

static const u32 PMIC_CLKBUF_MASK[PMIC_HW_DTS_NUM] = {
	[PMIC_DRV_CURR] = 0x3,
	[PMIC_HW_BBLPM] = 0x1,
	[PMIC_HW_BBLPM_SEL] = 0x1,
	[PMIC_LDO_VRFCK] = 0x1,
	[PMIC_LDO_VBBCK] = 0x1,
	[PMIC_AUXOUT_SEL] = 0x2f,
	[PMIC_AUXOUT_XO] = 0x1,
	[PMIC_AUXOUT_DRV_CURR] = 0x3,
	[PMIC_AUXOUT_BBLPM_EN] = 0x1,
	[PMIC_AUXOUT_BBLPM_O] = 0x1,
	[PMIC_COFST_FPM] = 0x3,
	[PMIC_CDAC_FPM] = 0xff,
	[PMIC_CORE_IDAC_FPM] = 0x3,
	[PMIC_AAC_FPM_SWEN] = 0x1,
};

static inline bool _is_pmic_clk_buf_debug_enable(void)
{
#ifdef CLKBUF_DEBUG
	return 1;
#else
	return 0;
#endif
}

static inline struct pmic_clkbuf_dts *find_pmic_dts_node(u32 dts)
{
	struct pmic_clkbuf_dts *node = NULL;

	list_for_each_entry(node, &dts_head, dts_list) {
		if (!strcmp(pmic_clkbuf_prop[dts], node->name))
			return node;
	}

	pr_info("[%s]: no %s property, function not support or something wrong\n",
			__func__, pmic_clkbuf_prop[dts]);

	return NULL;
}

static inline void pmic_clkbuf_read(u32 dts, u32 id, u32 *val)
{
	struct pmic_clkbuf_dts *node = NULL;
	u32 regval = 0;

	node = find_pmic_dts_node(dts);
	if (!node) {
		pr_info("%s: pmic node property not stored, something wrong!\n",
				__func__);
		return;
	}

	regmap_read(node->cfg.regmap, node->cfg.ofs[id], &regval);
	*val = (regval >> node->cfg.bit[id] & node->mask);
}

static inline void pmic_clkbuf_write(u32 dts, u32 id, u32 val)
{
	struct pmic_clkbuf_dts *node = NULL;

	node = find_pmic_dts_node(dts);
	if (!node) {
		pr_info("%s: pmic node property not stored, something wrong!\n",
				__func__);
		return;
	}

	val <<= node->cfg.bit[id];
	pr_info("offset: 0x%x\n", node->cfg.ofs[id]);
	regmap_write(node->cfg.regmap, node->cfg.ofs[id], val);
}

static inline void pmic_clkbuf_set(u32 dts, u32 id, u32 val)
{
	struct pmic_clkbuf_dts *node = NULL;

	node = find_pmic_dts_node(dts);
	if (!node) {
		pr_info("%s: pmic node property not stored, something wrong!\n",
				__func__);
		return;
	}

	val <<= node->cfg.bit[id];
	regmap_write(node->cfg.regmap, node->cfg.ofs[id] + 0x2, val);
}

static inline void pmic_clkbuf_clr(u32 dts, u32 id, u32 val)
{
	struct pmic_clkbuf_dts *node = NULL;

	node = find_pmic_dts_node(dts);
	if (!node) {
		pr_info("%s: pmic node property not stored, something wrong!\n",
				__func__);
		return;
	}

	val <<= node->cfg.bit[id];
	regmap_write(node->cfg.regmap, node->cfg.ofs[id] + 0x4, val);
}

static inline void pmic_clkbuf_update(u32 dts, u32 id, u32 val)
{
	struct pmic_clkbuf_dts *node = NULL;
	u32 mask = 0;
	u32 out = 0;

	node = find_pmic_dts_node(dts);
	if (!node) {
		pr_info("%s: pmic node property not stored, something wrong!\n",
				__func__);
		return;
	}

	val <<= node->cfg.bit[id];
	mask = node->mask << node->cfg.bit[id];

	regmap_update_bits(node->cfg.regmap,
			node->cfg.ofs[id],
			mask,
			val);
	regmap_read(node->cfg.regmap, node->cfg.ofs[id], &out);

	if (_is_pmic_clk_buf_debug_enable()) {
		pr_info("[%s]: val: 0x%x, shift val: 0x%x\n",
				__func__, node->cfg.bit[id], val);
		pr_info("[%s]: mask: 0x%x, shift mask: 0x%x\n",
				__func__, node->mask, mask);
		pr_info("%s: update value: 0x%x\n", __func__, out);
	}
}

static void _dummy_clk_buf_set_bblpm_hw_msk(enum clk_buf_id id, bool onoff)
{
	if (_is_pmic_clk_buf_debug_enable())
		pr_info("%s: HW BBLPM not support\n", __func__);
}

static int _dummy_clk_buf_bblpm_hw_en(bool on)
{
	if (_is_pmic_clk_buf_debug_enable())
		pr_info("%s: HW BBLPM not support\n", __func__);
	return 0;
}

static void _dummy_clk_buf_get_drv_curr(u32 *drvcurr)
{
	if (_is_pmic_clk_buf_debug_enable())
		pr_info("%s: Driving current not support\n", __func__);
}

static void _dummy_clk_buf_set_drv_curr(u32 *drvcurr)
{
	if (_is_pmic_clk_buf_debug_enable())
		pr_info("%s: Driving current not support\n", __func__);
}

static int _dummy_clk_buf_dump_misc_log(char *buf)
{
	if (_is_pmic_clk_buf_debug_enable())
		pr_info("%s: Dump misc log for clkbuf not support\n", __func__);
	return 0;
}

static void _pmic_clk_buf_set_bblpm_hw_msk(enum clk_buf_id id, bool onoff)
{
	pmic_clkbuf_update(PMIC_HW_BBLPM, id, onoff);
}

static int _pmic_clk_buf_bblpm_hw_en(bool on)
{
	u32 val = 0;

	pmic_clkbuf_update(PMIC_HW_BBLPM_SEL, 0, on);
	pmic_clkbuf_read(PMIC_HW_BBLPM_SEL, 0, &val);

	pr_debug("%s(%u): bblpm_hw=0x%x\n",
			__func__, (on ? 1 : 0), val);
	return 0;
}

static void _pmic_clk_buf_get_drv_curr(u32 *drvcurr)
{
	int i = 0;	/* to write different AUX function */
	int j = 0;	/* for reading different AUX value */
	int offset = 0;	/* for ignoring XO_BUF5 */
	const u32 auxout_soc_wcn_curr = 0;
	const u32 auxout_pd_ext_curr = 2;
	int max = auxout_pd_ext_curr - auxout_soc_wcn_curr + 1;
	struct pmic_clkbuf_dts *node = NULL;
	struct pmic_clkbuf_dts *out_node = NULL;

	node = find_pmic_dts_node(PMIC_AUXOUT_SEL);
	if (!node) {
		pr_info("%s: pmic node property not stored, something wrong!\n",
				__func__);
		return;
	}

	out_node = find_pmic_dts_node(PMIC_AUXOUT_DRV_CURR);
	if (!out_node) {
		pr_info("%s: pmic node property not stored, something wrong!\n",
				__func__);
		return;
	}

	for (i = 0; i < max; i++) {
		if (_is_pmic_clk_buf_debug_enable())
			pr_info("%s: AUXOUT drv curr idx: %d, value: %d\n",
					__func__,
					i,
					node->cfg.bit[i + 1]);

		pmic_clkbuf_write(PMIC_AUXOUT_SEL, 0,
					node->cfg.bit[i + 1]);

		if (i == 2) {
			offset = 1;
			j++;
		}

		pmic_clkbuf_read(PMIC_AUXOUT_DRV_CURR,
				j,
				&(drvcurr[i * 2 + offset]));
		j++;
		pmic_clkbuf_read(PMIC_AUXOUT_DRV_CURR,
				j,
				&(drvcurr[i * 2 + 1 + offset]));
		j++;
	}
}

static void _pmic_clk_buf_set_drv_curr(u32 *drvcurr)
{
	u32 i = 0;
	struct pmic_clkbuf_dts *node = NULL;

	node = find_pmic_dts_node(PMIC_AUXOUT_DRV_CURR);
	if (!node) {
		pr_info("%s: pmic node property not stored, something wrong!\n",
				__func__);
		return;
	}

	for (i = 0; i < XO_NUMBER; i++)
		if (node->cfg.ofs[i] != NOT_VALID)
			pmic_clkbuf_update(PMIC_DRV_CURR, i, drvcurr[i] % 4);

}

static void mt6357_clk_buf_get_xo_en(u32 *stat)
{
	struct pmic_clkbuf_dts *node = NULL;

	node = find_pmic_dts_node(PMIC_AUXOUT_SEL);
	if (!node) {
		pr_info("%s: pmic node property not stored, something wrong!\n",
				__func__);
		return;
	}

	if (_is_pmic_clk_buf_debug_enable())
		pr_info("[%s]: idx: %d, AUXOUT write: %u\n",
			__func__, 1, node->cfg.bit[1]);
	pmic_clkbuf_write(PMIC_AUXOUT_SEL, 0, node->cfg.bit[1]);

	pmic_clkbuf_read(PMIC_AUXOUT_XO, 0, &(stat[0]));
	pmic_clkbuf_read(PMIC_AUXOUT_XO, 1, &(stat[1]));

	if (_is_pmic_clk_buf_debug_enable())
		pr_info("[%s]: idx: %d, AUXOUT write: %u\n",
			__func__, 2, node->cfg.bit[2]);
	pmic_clkbuf_write(PMIC_AUXOUT_SEL, 0, node->cfg.bit[2]);

	pmic_clkbuf_read(PMIC_AUXOUT_XO, 2, &(stat[2]));
	pmic_clkbuf_read(PMIC_AUXOUT_XO, 3, &(stat[3]));

	if (_is_pmic_clk_buf_debug_enable())
		pr_info("[%s]: idx: %d, AUXOUT write: %u\n",
			__func__, 3, node->cfg.bit[3]);
	pmic_clkbuf_write(PMIC_AUXOUT_SEL, 0, node->cfg.bit[3]);

	pmic_clkbuf_read(PMIC_AUXOUT_XO, 5, &(stat[5]));

	if (_is_pmic_clk_buf_debug_enable())
		pr_info("[%s]: idx: %d, AUXOUT write: %u\n",
			__func__, 4, node->cfg.bit[4]);
	pmic_clkbuf_write(PMIC_AUXOUT_SEL, 0, node->cfg.bit[4]);

	pmic_clkbuf_read(PMIC_AUXOUT_XO, 6, &(stat[6]));
}

static void mt6359_clk_buf_get_xo_en(u32 *stat)
{
	u32 i = 0;
	struct pmic_clkbuf_dts *node = NULL;

	node = find_pmic_dts_node(PMIC_AUXOUT_SEL);
	if (!node) {
		pr_info("%s: pmic node property not stored, something wrong!\n",
				__func__);
		return;
	}

	pmic_clkbuf_write(PMIC_AUXOUT_SEL, 0, node->cfg.bit[1]);

	for (i = 0; i < XO_NUMBER; i++)
		if (node->cfg.ofs[i] != NOT_VALID)
			pmic_clkbuf_read(PMIC_AUXOUT_XO, i, &(stat[i]));

	pr_info("[%s]: EN_STAT=%u %u %u %u %u %u\n",
		__func__,
		stat[XO_SOC],
		stat[XO_WCN],
		stat[XO_NFC],
		stat[XO_CEL],
		stat[XO_PD],
		stat[XO_EXT]);
}

static void mt6357_clk_buf_get_bblpm_en(u32 *stat)
{
	struct pmic_clkbuf_dts *node = NULL;

	node = find_pmic_dts_node(PMIC_AUXOUT_SEL);
	if (!node) {
		pr_info("%s: pmic node property not stored, something wrong!\n",
				__func__);
		return;
	}

	if (_is_pmic_clk_buf_debug_enable())
		pr_info("[%s]: auxout bblpm idx: %u\n",
			__func__, node->cfg.bit[5]);
	pmic_clkbuf_write(PMIC_AUXOUT_SEL, 0, node->cfg.bit[5]);
	pmic_clkbuf_read(PMIC_AUXOUT_BBLPM_EN, 0, &(stat[0]));
}

static void mt6357_clk_buf_set_cap_id(u32 capid)
{
	pmic_clkbuf_update(PMIC_COFST_FPM, 0, 0);
	pmic_clkbuf_update(PMIC_CDAC_FPM, 0, capid);
	pmic_clkbuf_update(PMIC_CORE_IDAC_FPM, 0, 2);
	pmic_clkbuf_update(PMIC_AAC_FPM_SWEN, 0, 0);
	mdelay(1);
	pmic_clkbuf_update(PMIC_AAC_FPM_SWEN, 0, 1);
	mdelay(5);
}

static void mt6357_clk_buf_get_cap_id(u32 *capid)
{
	pmic_clkbuf_read(PMIC_CDAC_FPM, 0, capid);
}

static void mt6359_clk_buf_get_bblpm_en(u32 *stat)
{
	struct pmic_clkbuf_dts *node = NULL;

	node = find_pmic_dts_node(PMIC_AUXOUT_SEL);
	if (!node) {
		pr_info("%s: pmic node property not stored, something wrong!\n",
				__func__);
		return;
	}

	if (_is_pmic_clk_buf_debug_enable())
		pr_info("[%s]: auxout bblpm idx: %u\n",
			__func__, node->cfg.bit[2]);
	pmic_clkbuf_write(PMIC_AUXOUT_SEL, 0, node->cfg.bit[2]);
	pmic_clkbuf_read(PMIC_AUXOUT_BBLPM_EN, 0, &(stat[0]));
}

static int mt6359_clk_buf_dump_misc_log(char *buf)
{
	u32 len = strlen(buf);
	u32 val = 0;
	u32 i = 0;
	u32 node_idx[2] = {PMIC_LDO_VRFCK, PMIC_LDO_VBBCK};
	struct pmic_clkbuf_dts *node = NULL;

	for (i = 0; i < 2; i++) {
		node = find_pmic_dts_node(node_idx[i]);
		if (!node) {
			pr_info("%s: pmic node property not stored, something wrong!\n",
					__func__);
			return 0;
		}
		pmic_clkbuf_read(PMIC_LDO_VRFCK, 0, &val);
		len += snprintf(buf+len, PAGE_SIZE-len, "%s(%s)=0x%x\n",
				node->name, "en", val);
		pmic_clkbuf_read(PMIC_LDO_VRFCK, 1, &val);
		len += snprintf(buf+len, PAGE_SIZE-len, "%s(%s)=0x%x\n",
				node->name, "op_mode", val);
	}
	return len;
}

static inline int _pmic_clk_buf_find_mask(const char *pmic_prop,
					struct pmic_clkbuf_dts *pmic_dts_node)
{
	u32 idx = 0;

	if (!pmic_dts_node) {
		pr_info("[%s]: no pmic clkbuf dts node, find mask failed!\n",
				__func__);
		return -1;
	}

	for (idx = 0; idx < PMIC_HW_DTS_NUM; idx++) {
		if (!strcmp(pmic_prop, pmic_clkbuf_prop[idx])) {
			pmic_dts_node->mask = PMIC_CLKBUF_MASK[idx];
			if (_is_pmic_clk_buf_debug_enable())
				pr_info("[%s]: find %s mask: 0x%x\n",
						__func__,
						pmic_prop,
						pmic_dts_node->mask);
			return 0;
		}
	}
	pr_info("[%s]: find %s mask failed\n", __func__, pmic_prop);
	return -1;
}

static void _free_pmic_dts_list(void)
{
	struct pmic_clkbuf_dts *node = NULL;
	struct pmic_clkbuf_dts *next = NULL;

	list_for_each_entry_safe(node, next, &dts_head, dts_list) {
		list_del(&(node->dts_list));
		kfree(node->cfg.ofs);
		kfree(node->cfg.bit);
		kfree(node);
	}
}

static int _pmic_clk_buf_dts_get_property(struct device_node *node,
					struct regmap *regmap,
					const char *pmic_prop)
{
	u32 n_prop = 0;
	u32 idx = 0;
	int ret = 0;
	char n_pmic_prop[32] = "n-"; /* n-xxx property */
	struct pmic_clkbuf_dts *pmic_dts_node = NULL;

	strncat(n_pmic_prop, pmic_prop, sizeof(n_pmic_prop) - 1);

	ret = of_property_read_u32(node, n_pmic_prop, &n_prop);
	if (ret) {
		pr_info("[%s]: find %s failed\n", __func__, n_pmic_prop);
		goto no_property;
	}

	pmic_dts_node = kzalloc(sizeof(struct pmic_clkbuf_dts), GFP_KERNEL);
	if (!pmic_dts_node)
		goto no_mem;

	pmic_dts_node->cfg.ofs = kcalloc(n_prop, sizeof(u32), GFP_KERNEL);
	if (!(pmic_dts_node->cfg.ofs)) {
		pr_info("[%s]: allocate cfg offset memory failed\n",
				__func__);
		goto no_cfg_offset_mem;
	}

	pmic_dts_node->cfg.bit = kcalloc(n_prop, sizeof(u32), GFP_KERNEL);
	if (!(pmic_dts_node->cfg.bit)) {
		pr_info("[%s]: allocate cfg offset memory failed\n",
				__func__);
		goto no_cfg_bit_mem;
	}

	strncpy(pmic_dts_node->name, pmic_prop,
			sizeof(pmic_dts_node->name) - 1);

	if (_is_pmic_clk_buf_debug_enable())
		pr_info("[%s]: node name: %s\n",
			__func__, pmic_dts_node->name);

	pmic_dts_node->cfg.regmap = regmap;
	ret = _pmic_clk_buf_find_mask(pmic_prop, pmic_dts_node);

	if (ret)
		goto mask_not_found;

	for (idx = 0; idx < n_prop; idx++) {
		ret = of_property_read_u32_index(node,
					pmic_prop,
					(idx * 2),
					&(pmic_dts_node->cfg.ofs[idx]));
		if (ret) {
			pr_info("[%s]: find %s cfg offset index %u failed\n",
				__func__, pmic_prop, idx);
			goto offset_not_found;
		}

		if (_is_pmic_clk_buf_debug_enable())
			pr_info("[%s]: find %s cfg offset index %u: %u\n",
					__func__,
					pmic_prop,
					idx,
					pmic_dts_node->cfg.ofs[idx]);

		ret = of_property_read_u32_index(node,
					pmic_prop,
					(idx * 2 + 1),
					&(pmic_dts_node->cfg.bit[idx]));
		if (ret) {
			pr_info("[%s]: find %s cfg bit index %u failed\n",
				__func__, pmic_prop, idx);
			goto bit_not_found;
		}

		if (_is_pmic_clk_buf_debug_enable())
			pr_info("[%s]: find %s cfg bit index %u: %u\n",
					__func__,
					pmic_prop,
					idx,
					pmic_dts_node->cfg.bit[idx]);
	}

	INIT_LIST_HEAD(&(pmic_dts_node->dts_list));
	list_add_tail(&(pmic_dts_node->dts_list), &dts_head);

	return 0;

bit_not_found:
offset_not_found:
mask_not_found:
	kfree(pmic_dts_node->cfg.bit);
no_cfg_bit_mem:
	kfree(pmic_dts_node->cfg.ofs);
no_cfg_offset_mem:
	kfree(pmic_dts_node);
no_property:
no_mem:
	return -1;
}

static int _pmic_clk_buf_dts_init(struct device_node *node,
				struct regmap *regmap)
{
	u32 n_prop = 0;
	u32 idx = 0;
	const char *prop;
	int ret = 0;

	INIT_LIST_HEAD(&dts_head);

	ret = of_property_read_u32(node,
			"n-clkbuf-pmic-dependent",
			&n_prop);
	if (ret) {
		pr_info("[%s]: read number of pmic clkbuf dependent failed\n",
				__func__);
		goto no_property;
	}

	for (idx = 0; idx < n_prop; idx++) {
		ret = of_property_read_string_index(node,
				"clkbuf-pmic-dependent",
				idx,
				&prop);

		if (_is_pmic_clk_buf_debug_enable())
			pr_info("[%s]: find property %s\n", __func__, prop);
		if (ret) {
			pr_info("[%s]: read pmic clkbuf dependent failed\n",
					__func__);
			goto dependent_property_failed;
		}

		ret = _pmic_clk_buf_dts_get_property(node, regmap, prop);

		if (ret) {
			pr_info("[%s]: find property %s failed\n",
				__func__,
				prop);
			goto find_property_internal_failed;
		}
	}
	pr_info("[%s]: pmic dts init done\n", __func__);

	return 0;

find_property_internal_failed:
dependent_property_failed:
	_free_pmic_dts_list();
no_property:
	return -1;
}

static struct pmic_clkbuf_op pmic_clkbuf[PMIC_NUM] = {
	[PMIC_6357] = {
		.pmic_name = "mt6357",
		.pmic_clk_buf_set_bblpm_hw_msk =
					_dummy_clk_buf_set_bblpm_hw_msk,
		.pmic_clk_buf_bblpm_hw_en = _dummy_clk_buf_bblpm_hw_en,
		.pmic_clk_buf_get_drv_curr = _pmic_clk_buf_get_drv_curr,
		.pmic_clk_buf_set_drv_curr = _pmic_clk_buf_set_drv_curr,
		.pmic_clk_buf_dts_init = _pmic_clk_buf_dts_init,
		.pmic_clk_buf_get_xo_en = mt6357_clk_buf_get_xo_en,
		.pmic_clk_buf_get_bblpm_en = mt6357_clk_buf_get_bblpm_en,
		.pmic_clk_buf_dump_misc_log = _dummy_clk_buf_dump_misc_log,
		.pmic_clk_buf_set_cap_id = mt6357_clk_buf_set_cap_id,
		.pmic_clk_buf_get_cap_id = mt6357_clk_buf_get_cap_id,
	},
	[PMIC_6359] = {
		.pmic_name = "mt6359",
		.pmic_clk_buf_set_bblpm_hw_msk =
					_pmic_clk_buf_set_bblpm_hw_msk,
		.pmic_clk_buf_bblpm_hw_en = _pmic_clk_buf_bblpm_hw_en,
		.pmic_clk_buf_get_drv_curr = _dummy_clk_buf_get_drv_curr,
		.pmic_clk_buf_set_drv_curr = _dummy_clk_buf_set_drv_curr,
		.pmic_clk_buf_dts_init = _pmic_clk_buf_dts_init,
		.pmic_clk_buf_get_xo_en = mt6359_clk_buf_get_xo_en,
		.pmic_clk_buf_get_bblpm_en = mt6359_clk_buf_get_bblpm_en,
		.pmic_clk_buf_dump_misc_log = mt6359_clk_buf_dump_misc_log,
	},
};

int get_pmic_clkbuf(struct device_node *node,
				struct pmic_clkbuf_op **pmic_op)
{
	u32 idx = 0;
	struct device_node *parent_node = node->parent;

	for (idx = 0; idx < PMIC_NUM; idx++) {
		if (of_device_is_compatible(parent_node, PMIC_NAME[idx])) {
			*pmic_op = &pmic_clkbuf[idx];
			return 0;
		}
	}
	return -1;
}
