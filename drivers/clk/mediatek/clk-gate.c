// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: James Liao <jamesjj.liao@mediatek.com>
 */

#include <linux/of.h>
#include <linux/of_address.h>

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clkdev.h>
#include <linux/module.h>
#include <linux/sched/clock.h>

#include "clk-mtk.h"
#include "clk-gate.h"

static unsigned long long profile_time[4];
static bool is_registered;

static int mtk_cg_bit_is_cleared(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val;

	if (!is_registered)
		return 0;

	regmap_read(cg->regmap, cg->sta_ofs, &val);

	val &= BIT(cg->bit);

	return val == 0;
}

static int mtk_cg_bit_is_set(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val;

	if (!is_registered)
		return 0;

	regmap_read(cg->regmap, cg->sta_ofs, &val);

	val &= BIT(cg->bit);

	return val != 0;
}

static void mtk_cg_set_bit(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);

	regmap_write(cg->regmap, cg->set_ofs, BIT(cg->bit));
}

static void mtk_cg_clr_bit(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);

	regmap_write(cg->regmap, cg->clr_ofs, BIT(cg->bit));
}

static void mtk_cg_set_bit_no_setclr(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 cgbit = BIT(cg->bit);

	regmap_update_bits(cg->regmap, cg->sta_ofs, cgbit, cgbit);
}

static void mtk_cg_clr_bit_no_setclr(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 cgbit = BIT(cg->bit);

	regmap_update_bits(cg->regmap, cg->sta_ofs, cgbit, 0);
}

static int mtk_cg_enable(struct clk_hw *hw)
{
	mtk_cg_clr_bit(hw);

	return 0;
}

static void mtk_cg_disable(struct clk_hw *hw)
{
	mtk_cg_set_bit(hw);
}

static void mtk_cg_disable_unused(struct clk_hw *hw)
{
	const char *c_n = clk_hw_get_name(hw);

	pr_notice("disable_unused - %s\n", c_n);
	mtk_cg_set_bit(hw);
}

static int mtk_cg_enable_inv(struct clk_hw *hw)
{
	mtk_cg_set_bit(hw);

	return 0;
}

static void mtk_cg_disable_inv(struct clk_hw *hw)
{
	mtk_cg_clr_bit(hw);
}

static void mtk_cg_disable_unused_inv(struct clk_hw *hw)
{
	const char *c_n = clk_hw_get_name(hw);

	pr_notice("disable_unused - %s\n", c_n);
	mtk_cg_clr_bit(hw);
}

static int mtk_cg_is_set_hwv(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val;

	if (!is_registered)
		return 0;

	regmap_read(cg->hwv_regmap, cg->hwv_set_ofs, &val);

	val &= BIT(cg->bit);

	return val != 0;
}

static int mtk_cg_is_done_hwv(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val;

	regmap_read(cg->hwv_regmap, cg->hwv_sta_ofs, &val);

	val &= BIT(cg->bit);

	return val != 0;
}

static int __cg_enable_hwv(struct clk_hw *hw, bool inv)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val, val2;
	int i = 0, j = 0;

	profile_time[2] = 0;
	profile_time[3] = 0;

	/* dummy read to clr idle signal of hw voter bus */
	regmap_read(cg->hwv_regmap, cg->hwv_set_ofs, &val);

	regmap_write(cg->hwv_regmap, cg->hwv_set_ofs,
			BIT(cg->bit));
	profile_time[0] = sched_clock();

	while (!mtk_cg_is_set_hwv(hw)) {
		if (i < MTK_WAIT_HWV_PREPARE_CNT)
			udelay(MTK_WAIT_HWV_PREPARE_US);
		else
			goto hwv_prepare_fail;
		i++;
	}

	profile_time[1] = sched_clock();
	i = 0;

	while (1) {
		regmap_read(cg->hwv_regmap, cg->hwv_sta_ofs, &val);

		if ((profile_time[2] == 0) && (val & BIT(cg->bit)) != 0)
			profile_time[2] = sched_clock();
		else {
			regmap_read(cg->regmap, cg->sta_ofs, &val);
			if ((inv && (val & BIT(cg->bit)) != 0) ||
					(!inv && (val & BIT(cg->bit)) == 0)) {
				profile_time[3] = sched_clock();
				break;
			} else if (j  > MTK_WAIT_HWV_STA_CNT)
				goto hwv_sta_fail;
			else
				j++;
		}

		if (i < MTK_WAIT_HWV_DONE_CNT && j < MTK_WAIT_HWV_STA_CNT)
			udelay(MTK_WAIT_HWV_DONE_US);
		else
			goto hwv_done_fail;

		i++;
	}

	mtk_clk_notify(cg->regmap, cg->hwv_regmap, clk_hw_get_name(hw),
			cg->sta_ofs, (cg->hwv_set_ofs / MTK_HWV_ID_OFS),
			cg->bit, CLK_EVT_HWV_CG_CHK_PWR);

	return 0;

hwv_sta_fail:
	mtk_clk_notify(cg->regmap, cg->hwv_regmap, NULL,
			cg->sta_ofs, (cg->hwv_set_ofs / MTK_HWV_ID_OFS),
			cg->bit, CLK_EVT_LONG_BUS_LATENCY);
hwv_done_fail:
	regmap_read(cg->regmap, cg->sta_ofs, &val);
	regmap_read(cg->hwv_regmap, cg->hwv_sta_ofs, &val2);
	pr_err("%s cg enable timeout(%x %x)\n", clk_hw_get_name(hw), val, val2);
hwv_prepare_fail:
	regmap_read(cg->regmap, cg->hwv_sta_ofs, &val);
	pr_err("%s cg prepare timeout(%x)\n", clk_hw_get_name(hw), val);

	for (i = 0; i < 4; i++)
		pr_err("[%d]%lld us", i, profile_time[i]);

	mtk_clk_notify(cg->regmap, cg->hwv_regmap, clk_hw_get_name(hw),
			cg->sta_ofs, (cg->hwv_set_ofs / MTK_HWV_ID_OFS),
			cg->bit, CLK_EVT_HWV_CG_TIMEOUT);

	return -EBUSY;
}

static int mtk_cg_enable_hwv(struct clk_hw *hw)
{
	return __cg_enable_hwv(hw, false);
}

static int mtk_cg_enable_hwv_inv(struct clk_hw *hw)
{
	return __cg_enable_hwv(hw, true);
}

static void mtk_cg_disable_hwv(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val;
	int i = 0;

	/* dummy read to clr idle signal of hw voter bus */
	regmap_read(cg->hwv_regmap, cg->hwv_clr_ofs, &val);

	regmap_write(cg->hwv_regmap, cg->hwv_clr_ofs, BIT(cg->bit));

	while (mtk_cg_is_set_hwv(hw)) {
		if (i < MTK_WAIT_HWV_PREPARE_CNT)
			udelay(MTK_WAIT_HWV_PREPARE_US);
		else
			goto hwv_prepare_fail;
		i++;
	}

	i = 0;

	while (!mtk_cg_is_done_hwv(hw)) {
		if (i < MTK_WAIT_HWV_DONE_CNT)
			udelay(MTK_WAIT_HWV_DONE_US);
		else
			goto hwv_done_fail;
		i++;
	}

	mtk_clk_notify(cg->regmap, cg->hwv_regmap, clk_hw_get_name(hw),
			cg->sta_ofs, (cg->hwv_set_ofs / MTK_HWV_ID_OFS),
			cg->bit, CLK_EVT_HWV_CG_CHK_PWR);

	return;

hwv_done_fail:
	pr_err("%s cg disable timeout(%dus)\n", clk_hw_get_name(hw),
			i * MTK_WAIT_HWV_DONE_US);
hwv_prepare_fail:
	regmap_read(cg->regmap, cg->sta_ofs, &val);
	pr_err("%s cg unprepare timeout(%dus)(0x%x 0x%x)\n", clk_hw_get_name(hw),
			i * MTK_WAIT_HWV_PREPARE_US, val);

	mtk_clk_notify(cg->regmap, cg->hwv_regmap, clk_hw_get_name(hw),
			cg->sta_ofs, (cg->hwv_set_ofs / MTK_HWV_ID_OFS),
			cg->bit, CLK_EVT_HWV_CG_TIMEOUT);
}

static void mtk_cg_disable_unused_hwv(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	const char *c_n = clk_hw_get_name(hw);

	pr_notice("disable_unused - %s\n", c_n);
	regmap_write(cg->hwv_regmap, cg->hwv_clr_ofs, BIT(cg->bit));
}

static int mtk_cg_enable_no_setclr(struct clk_hw *hw)
{
	mtk_cg_clr_bit_no_setclr(hw);

	return 0;
}

static void mtk_cg_disable_no_setclr(struct clk_hw *hw)
{
	mtk_cg_set_bit_no_setclr(hw);
}

static void mtk_cg_disable_unused_no_setclr(struct clk_hw *hw)
{
	const char *c_n = clk_hw_get_name(hw);

	pr_notice("disable_unused - %s\n", c_n);
	mtk_cg_set_bit_no_setclr(hw);
}


static int mtk_cg_enable_inv_no_setclr(struct clk_hw *hw)
{
	mtk_cg_set_bit_no_setclr(hw);

	return 0;
}

static void mtk_cg_disable_inv_no_setclr(struct clk_hw *hw)
{
	mtk_cg_clr_bit_no_setclr(hw);
}

static void mtk_cg_disable_unused_inv_no_setclr(struct clk_hw *hw)
{
	const char *c_n = clk_hw_get_name(hw);

	pr_notice("disable_unused - %s\n", c_n);
	mtk_cg_clr_bit_no_setclr(hw);
}

const struct clk_ops mtk_clk_gate_ops_setclr_dummy = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.enable		= mtk_cg_enable,
	.disable_unused = mtk_cg_disable_unused,
};
EXPORT_SYMBOL(mtk_clk_gate_ops_setclr_dummy);

const struct clk_ops mtk_clk_gate_ops_setclr_dummys = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.disable_unused = mtk_cg_disable_unused,
};
EXPORT_SYMBOL(mtk_clk_gate_ops_setclr_dummys);

const struct clk_ops mtk_clk_gate_ops_hwv_dummy = {
	.is_enabled	= mtk_cg_is_set_hwv,
	.enable		= mtk_cg_enable_hwv,
	.disable_unused = mtk_cg_disable_unused_hwv,
};
EXPORT_SYMBOL(mtk_clk_gate_ops_hwv_dummy);

const struct clk_ops mtk_clk_gate_ops_setclr_inv_dummy = {
	.is_enabled	= mtk_cg_bit_is_set,
	.enable		= mtk_cg_enable_inv,
	.disable_unused = mtk_cg_disable_unused_inv,
};
EXPORT_SYMBOL(mtk_clk_gate_ops_setclr_inv_dummy);

const struct clk_ops mtk_clk_gate_ops_setclr = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.enable		= mtk_cg_enable,
	.disable	= mtk_cg_disable,
	.disable_unused = mtk_cg_disable_unused,
};
EXPORT_SYMBOL(mtk_clk_gate_ops_setclr);

const struct clk_ops mtk_clk_gate_ops_setclr_inv = {
	.is_enabled	= mtk_cg_bit_is_set,
	.enable		= mtk_cg_enable_inv,
	.disable	= mtk_cg_disable_inv,
	.disable_unused = mtk_cg_disable_unused_inv,
};
EXPORT_SYMBOL(mtk_clk_gate_ops_setclr_inv);

const struct clk_ops mtk_clk_gate_ops_hwv = {
	.is_enabled	= mtk_cg_is_set_hwv,
	.enable		= mtk_cg_enable_hwv,
	.disable	= mtk_cg_disable_hwv,
	.disable_unused = mtk_cg_disable_unused_hwv,
};
EXPORT_SYMBOL(mtk_clk_gate_ops_hwv);

const struct clk_ops mtk_clk_gate_ops_hwv_inv = {
	.is_enabled	= mtk_cg_is_set_hwv,
	.enable		= mtk_cg_enable_hwv_inv,
	.disable	= mtk_cg_disable_hwv,
	.disable_unused = mtk_cg_disable_unused_hwv,
};
EXPORT_SYMBOL(mtk_clk_gate_ops_hwv_inv);

const struct clk_ops mtk_clk_gate_ops_no_setclr = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.enable		= mtk_cg_enable_no_setclr,
	.disable	= mtk_cg_disable_no_setclr,
	.disable_unused = mtk_cg_disable_unused_no_setclr,
};
EXPORT_SYMBOL(mtk_clk_gate_ops_no_setclr);

const struct clk_ops mtk_clk_gate_ops_no_setclr_inv = {
	.is_enabled	= mtk_cg_bit_is_set,
	.enable		= mtk_cg_enable_inv_no_setclr,
	.disable	= mtk_cg_disable_inv_no_setclr,
	.disable_unused = mtk_cg_disable_unused_inv_no_setclr,
};
EXPORT_SYMBOL(mtk_clk_gate_ops_no_setclr_inv);

struct clk *mtk_clk_register_gate_hwv(
		const char *name,
		const char *parent_name,
		struct regmap *regmap,
		struct regmap *hwv_regmap,
		int set_ofs,
		int clr_ofs,
		int sta_ofs,
		int hwv_set_ofs,
		int hwv_clr_ofs,
		int hwv_sta_ofs,
		u8 bit,
		const struct clk_ops *ops,
		unsigned long flags,
		struct device *dev)
{
	struct mtk_clk_gate *cg;
	struct clk *clk;
	struct clk_init_data init = {};

	is_registered = false;

	cg = kzalloc(sizeof(*cg), GFP_KERNEL);
	if (!cg)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.flags = flags | CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE;
	init.parent_names = parent_name ? &parent_name : NULL;
	init.num_parents = parent_name ? 1 : 0;
	init.ops = ops;

	cg->regmap = regmap;
	cg->hwv_regmap = hwv_regmap;
	cg->set_ofs = set_ofs;
	cg->clr_ofs = clr_ofs;
	cg->sta_ofs = sta_ofs;
	cg->hwv_set_ofs = hwv_set_ofs;
	cg->hwv_clr_ofs = hwv_clr_ofs;
	cg->hwv_sta_ofs = hwv_sta_ofs;
	cg->bit = bit;

	cg->hw.init = &init;

	clk = clk_register(dev, &cg->hw);
	if (IS_ERR(clk))
		kfree(cg);

	is_registered = true;

	return clk;
}
EXPORT_SYMBOL(mtk_clk_register_gate_hwv);

struct clk *mtk_clk_register_gate(
		const char *name,
		const char *parent_name,
		struct regmap *regmap,
		int set_ofs,
		int clr_ofs,
		int sta_ofs,
		u8 bit,
		const struct clk_ops *ops,
		unsigned long flags,
		struct device *dev)
{
	struct mtk_clk_gate *cg;
	struct clk *clk;
	struct clk_init_data init = {};

	is_registered = false;

	cg = kzalloc(sizeof(*cg), GFP_KERNEL);
	if (!cg)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.flags = flags | CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE;
	init.parent_names = parent_name ? &parent_name : NULL;
	init.num_parents = parent_name ? 1 : 0;
	init.ops = ops;

	cg->regmap = regmap;
	cg->set_ofs = set_ofs;
	cg->clr_ofs = clr_ofs;
	cg->sta_ofs = sta_ofs;
	cg->bit = bit;

	cg->hw.init = &init;

	clk = clk_register(dev, &cg->hw);
	if (IS_ERR(clk))
		kfree(cg);

	is_registered = true;

	return clk;
}
EXPORT_SYMBOL(mtk_clk_register_gate);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek GATE");
MODULE_AUTHOR("MediaTek Inc.");
