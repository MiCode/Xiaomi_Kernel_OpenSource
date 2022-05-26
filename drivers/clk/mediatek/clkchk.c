// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#define pr_fmt(fmt) "[clkchk] " fmt

#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/module.h>

#include "clk-mtk.h"
#include "clkchk.h"
#include "mt-plat/aee.h"

#define MAX_CLK_NUM			1024
#define PLL_LEN				20
#define INV_MSK				0xFFFFFFFF

void __attribute__((weak)) clkchk_set_cfg(void)
{
}

static const struct clkchk_ops *clkchk_ops;
static struct notifier_block mtk_clkchk_notifier;

void set_clkchk_ops(const struct clkchk_ops *ops)
{
	clkchk_ops = ops;
}
EXPORT_SYMBOL(set_clkchk_ops);

/*
 * for mtcmos debug
 */
bool is_valid_reg(void __iomem *addr)
{
#if IS_ENABLED(CONFIG_64BIT)
	return ((u64)addr & 0xf0000000) != 0UL ||
			(((u64)addr >> 32U) & 0xf0000000) != 0UL;
#else
	return ((u32)addr & 0xf0000000) != 0U;
#endif
}
EXPORT_SYMBOL(is_valid_reg);

const struct regname *get_all_regnames(void)
{
	if (clkchk_ops == NULL || clkchk_ops->get_all_regnames == NULL)
		return NULL;

	return clkchk_ops->get_all_regnames();
}
EXPORT_SYMBOL(get_all_regnames);

void clkchk_devapc_dump(void)
{
	if (clkchk_ops == NULL || clkchk_ops->devapc_dump == NULL)
		return;

	clkchk_ops->devapc_dump();
}
EXPORT_SYMBOL(clkchk_devapc_dump);

static const char *get_provider_name(struct device_node *node, u32 *cells)
{
	const char *name;
	const char *p;
	u32 cc = 0;

	if (of_property_read_u32(node, "#clock-cells", &cc) != 0)
		cc = 0;

	if (cells != NULL)
		*cells = cc;

	if (cc == 0U) {
		if (of_property_read_string(node,
				"clock-output-names", &name) < 0)
			name = node->name;

		return name;
	}

	if (of_property_read_string(node, "compatible", &name) < 0)
		name = node->name;

	p = strchr(name, (int)'-');

	if (p != NULL)
		return p + 1;
	else
		return name;
}

static void setup_provider_clk(struct provider_clk *pvdck)
{
	const char *pvdname = pvdck->provider_name;
	struct pvd_msk *pm;

	if (!pvdname)
		return;

	if (clkchk_ops == NULL || clkchk_ops->get_pvd_pwr_mask == NULL)
		return;

	pm = clkchk_ops->get_pvd_pwr_mask();

	for (; pm->pvdname != NULL; pm++) {
		if (!strcmp(pvdname, pm->pvdname)) {
			pvdck->pwr_mask = pm->pwr_mask;
			pvdck->sta_type = pm->sta_type;
			return;
		}
	}

	pvdck->pwr_mask = INV_MSK;
}

struct provider_clk *get_all_provider_clks(void)
{
	static struct provider_clk provider_clks[MAX_CLK_NUM];
	struct device_node *node = NULL;
	unsigned int n = 0;

	if (provider_clks[0].ck != NULL)
		return provider_clks;

	do {
		const char *node_name;
		u32 cells;

		node = of_find_node_with_property(node, "#clock-cells");

		if (node == NULL)
			break;

		node_name = get_provider_name(node, &cells);

		if (cells != 0U)  {
			unsigned int i;

			for (i = 0; i < MAX_CLK_NUM; i++) {
				struct of_phandle_args pa;
				struct clk *ck;

				pa.np = node;
				pa.args[0] = i;
				pa.args_count = 1;
				ck = of_clk_get_from_provider(&pa);

				if (PTR_ERR(ck) == -EINVAL)
					break;
				else if (IS_ERR_OR_NULL(ck))
					continue;

				provider_clks[n].ck = ck;
				provider_clks[n].ck_name = __clk_get_name(ck);
				provider_clks[n].idx = i;
				provider_clks[n].provider_name = node_name;
				setup_provider_clk(&provider_clks[n]);

				++n;
			}
		}
	} while (node != NULL && n < MAX_CLK_NUM);

	return provider_clks;
}
EXPORT_SYMBOL(get_all_provider_clks);

static struct provider_clk *__clk_chk_lookup_pvdck(const char *name)
{
	struct provider_clk *pvdck = get_all_provider_clks();

	for (; pvdck->ck != NULL; pvdck++) {
		if (!strcmp(pvdck->ck_name, name))
			return pvdck;
	}

	return NULL;
}

static struct clk *__clk_chk_lookup(const char *name)
{
	struct provider_clk *pvdck = __clk_chk_lookup_pvdck(name);

	if (pvdck)
		return pvdck->ck;

	return NULL;
}

struct clk *clk_chk_lookup(const char *name)
{
	return __clk_chk_lookup(name);
}
EXPORT_SYMBOL(clk_chk_lookup);

static s32 *read_spm_pwr_status_array(void)
{
	if (clkchk_ops == NULL || clkchk_ops->get_spm_pwr_status_array  == NULL)
		return  ERR_PTR(-EINVAL);

	return clkchk_ops->get_spm_pwr_status_array();
}

int pwr_hw_is_on(enum PWR_STA_TYPE type, u32 mask)
{
	u32 *pval = read_spm_pwr_status_array();

	if (type == PWR_STA) {
		if ((pval[PWR_STA] & mask) != mask &&
				(pval[PWR_STA2] & mask) != mask)
			return 0;
		else if ((pval[PWR_STA] & mask) == mask &&
				(pval[PWR_STA2] & mask) == mask)
			return 1;
		else
			return -1;
	}  else {
		if ((pval[type] & mask) == mask)
			return 1;
		else if ((pval[type] & mask) != 0)
			return -1;

		else
			return 0;
	}
}
EXPORT_SYMBOL(pwr_hw_is_on);

int clkchk_pvdck_is_on(struct provider_clk *pvdck)
{
	if (!pvdck)
		return -1;

	if (clkchk_ops == NULL || clkchk_ops->is_pwr_on == NULL) {
		if (clkchk_ops == NULL || clkchk_ops->get_pvd_pwr_mask == NULL)
			return -1;

		if (pvdck->pwr_mask == INV_MSK) {
			return 0;
		}

		if (!pvdck->pwr_mask) {
			return 1;
		}

		return pwr_hw_is_on(pvdck->sta_type, pvdck->pwr_mask);
	}

	return clkchk_ops->is_pwr_on(pvdck);
}
EXPORT_SYMBOL(clkchk_pvdck_is_on);

bool clkchk_pvdck_is_prepared(struct provider_clk *pvdck)
{
	struct clk_hw *hw;

	if (clkchk_pvdck_is_on(pvdck) == 1) {
		hw = __clk_get_hw(pvdck->ck);

		if (IS_ERR_OR_NULL(hw))
			return false;

		return clk_hw_is_prepared(hw);
	}

	return false;
}
EXPORT_SYMBOL(clkchk_pvdck_is_prepared);

bool clkchk_pvdck_is_enabled(struct provider_clk *pvdck)
{
	struct clk_hw *hw;

	if (clkchk_pvdck_is_on(pvdck) == 1) {
		hw = __clk_get_hw(pvdck->ck);

		if (IS_ERR_OR_NULL(hw))
			return false;

		if (IS_ERR_OR_NULL(clk_hw_get_parent(hw)))
			return false;

		if (!clk_hw_is_enabled(clk_hw_get_parent(hw)))
			return false;

		return clk_hw_is_enabled(hw);
	}

	return false;
}
EXPORT_SYMBOL(clkchk_pvdck_is_enabled);

static const char *ccf_state(struct provider_clk *pvdck)
{
	if (clkchk_pvdck_is_enabled(pvdck))
		return "enabled";

	if (clkchk_pvdck_is_prepared(pvdck))
		return "prepared";

	return "disabled";
}

static void dump_enabled_clks(struct provider_clk *pvdck)
{
	const char * const *pll_names;
	const char *c_name;
	const char *p_name;
	const char *comp_name;
	struct clk_hw *c_hw = __clk_get_hw(pvdck->ck);
	struct clk_hw *p_hw;

	if (!clkchk_pvdck_is_prepared(pvdck) && !clkchk_pvdck_is_enabled(pvdck))
		return;

	if (clkchk_ops == NULL || clkchk_ops->get_off_pll_names == NULL)
		return;

	if (IS_ERR_OR_NULL(c_hw))
		return;

	c_name = clk_hw_get_name(c_hw);

	p_hw = clk_hw_get_parent(c_hw);
	if (IS_ERR_OR_NULL(p_hw))
		return;

	p_name = clk_hw_get_name(c_hw);
	comp_name = clk_hw_get_name(p_hw);
	while (strcmp(comp_name, "clk26m")) {
		p_name = p_hw ? clk_hw_get_name(p_hw) : NULL;
		p_hw = clk_hw_get_parent(p_hw);
		if (IS_ERR_OR_NULL(p_hw))
			return;

		comp_name = clk_hw_get_name(p_hw);
	}

	pll_names = clkchk_ops->get_off_pll_names();
	for (; *pll_names != NULL && p_name != NULL; pll_names++) {
		if (!strncmp(p_name, *pll_names, PLL_LEN)) {
			p_hw = clk_hw_get_parent(c_hw);
			pr_notice("[%-21s: %8s, %3d, %3d, %10ld, %21s]\n",
					c_name,
					ccf_state(pvdck),
					clkchk_pvdck_is_prepared(pvdck),
					clkchk_pvdck_is_enabled(pvdck),
					clk_hw_get_rate(c_hw),
					p_hw ? clk_hw_get_name(p_hw) : "None");
			break;
		}
	}
}

static bool __check_pll_off(const char * const *name)
{
	int valid = 0;

	for (; *name != NULL; name++) {
		struct provider_clk *pvdck = __clk_chk_lookup_pvdck(*name);

		if (!clkchk_pvdck_is_enabled(pvdck))
			continue;

		pr_notice("suspend warning[0m: %s is on\n", *name);

		valid++;
	}

	if (valid)
		return true;

	return false;
}

static bool check_pll_off(void)
{
	const char * const *name;
	int ret = 0;

	if (clkchk_ops == NULL || clkchk_ops->get_off_pll_names == NULL)
		return false;

	name = clkchk_ops->get_off_pll_names();

	ret = __check_pll_off(name);

	if (clkchk_ops == NULL || clkchk_ops->get_notice_pll_names == NULL)
		return false;

	name = clkchk_ops->get_notice_pll_names();

	__check_pll_off(name);

	if (ret)
		return true;

	return false;
}

static bool is_pll_chk_bug_on(void)
{
	if (clkchk_ops == NULL || clkchk_ops->is_pll_chk_bug_on == NULL)
		return false;

	return clkchk_ops->is_pll_chk_bug_on();
}

/*
 * clkchk vf table checking
 */

static struct mtk_vf *clkchk_get_vf_table(void)
{
	if (clkchk_ops == NULL || clkchk_ops->get_vf_table == NULL)
		return NULL;

	return clkchk_ops->get_vf_table();
}

static int get_vcore_opp(void)
{
	if (clkchk_ops == NULL || clkchk_ops->get_vcore_opp == NULL)
		return VCORE_NULL;

	return clkchk_ops->get_vcore_opp();
}

void warn_vcore(int opp, const char *clk_name, int rate, int freq)
{
	if ((opp >= 0) && (freq > 0) && ((rate/1000) > freq)) {
		pr_notice("%s Choose %d FAIL!!!![MAX(%d): %d]\r\n",
				clk_name, rate/1000, opp, freq);

		BUG_ON(1);
	}
}

static int mtk_mux2opp(const char *mux_name, int opp)
{
	struct mtk_vf *vf_table = clkchk_get_vf_table();
	int i = 0;

	if (!vf_table || opp < 0)
		return -EINVAL;

	for (; vf_table != NULL && vf_table->name != NULL; vf_table++, i++) {
		const char *name = vf_table->name;

		if (!name)
			continue;

		if (strcmp(mux_name, name) == 0)
			return vf_table->freq_table[opp];
	}

	return -EINVAL;
}

/* The clocks have a mechanism for synchronizing rate changes. */
static int mtk_clk_rate_change(struct notifier_block *nb,
					  unsigned long flags, void *data)
{
	struct clk_notifier_data *ndata = data;
	struct clk_hw *hw = __clk_get_hw(ndata->clk);
	const char *clk_name = __clk_get_name(hw->clk);
	int vcore_opp = get_vcore_opp();

	if (vcore_opp == VCORE_NULL)
		return -EINVAL;

	if (flags == PRE_RATE_CHANGE && clk_name) {
		warn_vcore(vcore_opp, clk_name,
			ndata->new_rate, mtk_mux2opp(clk_name, vcore_opp));
	}

	return NOTIFY_OK;
}

static struct notifier_block mtk_clk_notifier = {
	.notifier_call = mtk_clk_rate_change,
};

int mtk_clk_check_muxes(void)
{
	struct mtk_vf *vf_table;
	struct clk *clk;

	if (clkchk_ops == NULL || clkchk_ops->get_vf_table == NULL
			|| clkchk_ops->get_vcore_opp == NULL)
		return -EINVAL;

	vf_table = clkchk_ops->get_vf_table();
	if (vf_table == NULL)
		return -EINVAL;

	for (; vf_table->name != NULL; vf_table++) {
		const char *name = vf_table->name;

		if (!name)
			continue;
		pr_notice("name: %s\n", name);

		clk = __clk_chk_lookup(name);
		clk_notifier_register(clk, &mtk_clk_notifier);
	}

	return 0;
}

static int clk_chk_dev_pm_suspend(struct device *dev)
{
	struct provider_clk *pvdck = get_all_provider_clks();

	if (check_pll_off()) {
		for (; pvdck->ck != NULL; pvdck++)
			dump_enabled_clks(pvdck);

		if (is_pll_chk_bug_on() || pdchk_get_bug_on_stat())
			BUG_ON(1);

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		aee_kernel_warning_api(__FILE__, __LINE__,
				DB_OPT_DEFAULT, "clk-chk",
				"fail to disable clk/pd in suspend\n");
#endif
	}

	return 0;
}

const struct dev_pm_ops clk_chk_dev_pm_ops = {
	.suspend_noirq = clk_chk_dev_pm_suspend,
	.resume_noirq = NULL,
};
EXPORT_SYMBOL(clk_chk_dev_pm_ops);

/*
 * for clock exception event handling
 */

static void clkchk_dump_hwv_history(struct regmap *regmap, u32 id)
{
	if (clkchk_ops == NULL || clkchk_ops->dump_hwv_history == NULL)
		return;

	clkchk_ops->dump_hwv_history(regmap, id);
}

static void clkchk_get_bus_reg(void)
{
	if (clkchk_ops == NULL || clkchk_ops->get_bus_reg == NULL)
		return;

	clkchk_ops->get_bus_reg();
}

static void clkchk_dump_bus_reg(struct regmap *regmap, u32 ofs)
{
	if (clkchk_ops == NULL || clkchk_ops->dump_bus_reg == NULL)
		return;

	clkchk_ops->dump_bus_reg(regmap, ofs);
}

static void clkchk_dump_hwv_pll_reg(struct regmap *regmap, u32 shift)
{
	if (clkchk_ops == NULL || clkchk_ops->dump_hwv_pll_reg == NULL)
		return;

	clkchk_ops->dump_hwv_pll_reg(regmap, shift);
}

static bool clkchk_is_cg_chk_pwr_on(void)
{
	if (clkchk_ops == NULL || clkchk_ops->is_cg_chk_pwr_on == NULL)
		return false;

	return clkchk_ops->is_cg_chk_pwr_on();
}

static void clkchk_cg_chk(const char *name)
{
	struct provider_clk *pvdck;

	pvdck = __clk_chk_lookup_pvdck(name);
	if (!clkchk_pvdck_is_on(pvdck))
		pr_notice("clk %s access without power on\n", name);
}

static int clkchk_evt_handling(struct notifier_block *nb,
			unsigned long flags, void *data)
{
	struct clk_event_data *clkd = (struct clk_event_data *)data;

	switch (clkd->event_type) {
	case CLK_EVT_HWV_CG_TIMEOUT:
		clkchk_dump_hwv_history(clkd->hwv_regmap, clkd->id);
		clkchk_dump_bus_reg(clkd->regmap, clkd->ofs);
		break;
	case CLK_EVT_HWV_CG_CHK_PWR:
		if (clkchk_is_cg_chk_pwr_on())
			clkchk_cg_chk(clkd->name);
		break;
	case CLK_EVT_LONG_BUS_LATENCY:
		clkchk_get_bus_reg();
		break;
	case CLK_EVT_HWV_PLL_TIMEOUT:
		clkchk_dump_hwv_pll_reg(clkd->hwv_regmap, clkd->shift);
		break;
	case CLK_EVT_SUSPEND_CG_DUMP:
		break;
	default:
		pr_notice("cannot get flags identify\n");
		break;
	}

	return NOTIFY_OK;
}

int set_clkchk_notify(void)
{
	int r = 0;

	mtk_clkchk_notifier.notifier_call = clkchk_evt_handling;
	r = register_mtk_clk_notifier(&mtk_clkchk_notifier);
	if (r)
		pr_err("clk-chk notifier register err(%d)\n", r);

	return r;
}
EXPORT_SYMBOL(set_clkchk_notify);
