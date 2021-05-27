// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/slab.h>

#include "mtk-pd-chk.h"

#define TAG			"[pdchk] "
#define MAX_PD_NUM		64
#define POWER_ON_STA		1
#define POWER_OFF_STA		0

static struct platform_device *pd_pdev[MAX_PD_NUM];
static struct notifier_block mtk_pd_notifier[MAX_PD_NUM];
static bool pd_sta[MAX_PD_NUM];

static const struct pdchk_ops *pdchk_ops;

static struct pd_check_swcg *get_subsys_cg(unsigned int id)
{
	if (pdchk_ops == NULL || pdchk_ops->get_subsys_cg == NULL)
		return NULL;

	return pdchk_ops->get_subsys_cg(id);
}

static void dump_subsys_reg(unsigned int id)
{
	if (pdchk_ops == NULL || pdchk_ops->dump_subsys_reg == NULL)
		return;

	return pdchk_ops->dump_subsys_reg(id);
}


static bool is_in_pd_list(unsigned int id)
{
	if (pdchk_ops == NULL || pdchk_ops->is_in_pd_list == NULL)
		return false;

	return pdchk_ops->is_in_pd_list(id);
}

static void pd_debug_dump(unsigned int id, unsigned int pwr_sta)
{
	if (pdchk_ops == NULL || pdchk_ops->debug_dump == NULL)
		return;

	pdchk_ops->debug_dump(id, pwr_sta);
}

static unsigned int check_cg_state(struct pd_check_swcg *swcg, unsigned int id)
{
	int enable_count = 0;

	if (!swcg)
		return 0;

	while (swcg->name) {
		if (!IS_ERR_OR_NULL(swcg->c)) {
			if (__clk_is_enabled(swcg->c) > 0) {
				dump_subsys_reg(id);
				pr_notice("%s[%-17s]\n", __func__,
					__clk_get_name(swcg->c));
				enable_count++;
			}
		}
		swcg++;
	}

	return enable_count;
}

static void mtk_check_subsys_swcg(unsigned int id)
{
	struct pd_check_swcg *swcg = get_subsys_cg(id);
	int ret;

	if (!swcg)
		return;

	/* check if Subsys CGs are still on */
	ret = check_cg_state(swcg, id);
	if (ret) {
		pr_err("%s(%d): %d\n", __func__, id, ret);
	}
}

/*
 * pm_domain event receive
 */

static int mtk_pd_dbg_dump(struct notifier_block *nb,
			unsigned long flags, void *data)
{
	switch (flags) {
	case GENPD_NOTIFY_PRE_ON:
		pd_sta[nb->priority] = POWER_ON_STA;
		break;
	case GENPD_NOTIFY_PRE_OFF:
		pd_sta[nb->priority] = POWER_OFF_STA;
		mtk_check_subsys_swcg(nb->priority);
		break;
	case GENPD_NOTIFY_ON:
		if (pd_sta[nb->priority] == POWER_OFF_STA)
			pd_debug_dump(nb->priority, PD_PWR_ON);

		break;
	case GENPD_NOTIFY_OFF:
		if (pd_sta[nb->priority] == POWER_ON_STA)
			pd_debug_dump(nb->priority, PD_PWR_OFF);

		break;
	default:
		pr_notice("cannot get flags identify\n");
		break;
	}

	return NOTIFY_OK;
}

static int set_genpd_notify(void)
{
	struct generic_pm_domain *pd;
	struct device_node *node;
	int r = 0;
	int i;

	node = of_find_node_with_property(NULL, "#power-domain-cells");

	if (node == NULL) {
		pr_err("no power domain defined at dts node\n");
		return -ENODEV;
	}

	for (i = 0; i < MAX_PD_NUM; i++) {
		struct of_phandle_args pa;

		if(!is_in_pd_list(i))
			continue;

		pa.np = node;
		pa.args[0] = i;
		pa.args_count = 1;

		pd_pdev[i] = platform_device_alloc("traverse", 0);
		if (!pd_pdev[i]) {
			pr_err("create pd-%d device fail\n", i);
			return -ENOMEM;
		}

		r = of_genpd_add_device(&pa, &pd_pdev[i]->dev);
		if (r == -EINVAL) {
			pr_err("add pd device fail\n");
			continue;
		} else if (r != 0)
			pr_warn("%s(): of_genpd_add_device(%d)\n", __func__, r);

		pd = pd_to_genpd(pd_pdev[i]->dev.pm_domain);
		if (IS_ERR(pd)) {
			pr_err("pd-%d is err\n", i);
			break;
		}

		mtk_pd_notifier[i].notifier_call = mtk_pd_dbg_dump;
		mtk_pd_notifier[i].priority = i,
		r = dev_pm_genpd_add_notifier(&pd_pdev[i]->dev, &mtk_pd_notifier[i]);
		if (r) {
			pr_err("pd-%s notifier err\n", pd->name);
			break;
		}

		pr_notice("pd-%s add to notifier\n", pd->name);

		pm_runtime_enable(&pd_pdev[i]->dev);
		pm_runtime_get(&pd_pdev[i]->dev);
		pm_runtime_put(&pd_pdev[i]->dev);
	}

	return r;
}

static void pd_check_swcg_init_common(struct pd_check_swcg *swcg)
{
	if (!swcg)
		return;

	while (swcg->name) {
		struct clk *c = clk_chk_lookup(swcg->name);

		if (IS_ERR_OR_NULL(c))
			pr_notice("[%17s: NULL]\n", swcg->name);
		else
			swcg->c = c;
		swcg++;
	}
}

void pd_check_common_init(const struct pdchk_ops *ops)
{
	int i;

	pdchk_ops = ops;

	for (i = 0; i < MAX_PD_NUM; i++) {
		struct pd_check_swcg *swcg = get_subsys_cg(i);

		if (!swcg)
			continue;
		/* fill the 'struct clk *' ptr of every CGs*/
		pd_check_swcg_init_common(swcg);
	}

	set_genpd_notify();
}
EXPORT_SYMBOL(pd_check_common_init);

