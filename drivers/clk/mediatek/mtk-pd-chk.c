// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

#include "mtk-pd-chk.h"

#define TAG			"[pdchk] "
#define MAX_PD_NUM		64
#define MAX_BUF_LEN		512
#define POWER_ON_STA		1
#define POWER_OFF_STA		0
#define ENABLE_PD_CHK_CG	0

static struct platform_device *pd_pdev[MAX_PD_NUM];
static struct generic_pm_domain *pds[MAX_PD_NUM];
static struct notifier_block mtk_pd_notifier[MAX_PD_NUM];
static bool pd_sta[MAX_PD_NUM];

static const struct pdchk_ops *pdchk_ops;

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

static void pd_log_dump(unsigned int id, unsigned int pwr_sta)
{
	if (pdchk_ops == NULL || pdchk_ops->log_dump == NULL)
		return;

	pdchk_ops->log_dump(id, pwr_sta);
}

static struct pd_check_swcg *get_subsys_cg(unsigned int id)
{
	if (pdchk_ops == NULL || pdchk_ops->get_subsys_cg == NULL)
		return NULL;

	return pdchk_ops->get_subsys_cg(id);
}

#if ENABLE_PD_CHK_CG
static void dump_subsys_reg(unsigned int id)
{
	if (pdchk_ops == NULL || pdchk_ops->dump_subsys_reg == NULL)
		return;

	return pdchk_ops->dump_subsys_reg(id);
}

static unsigned int check_cg_state(struct pd_check_swcg *swcg, unsigned int id)
{
	char *buf;
	int enable_count = 0;
	int len = 0;

	if (!swcg)
		return 0;

	buf = kzalloc(MAX_BUF_LEN, GFP_KERNEL);

	while (swcg->name) {
		if (!IS_ERR_OR_NULL(swcg->c)) {
			if (__clk_is_enabled(swcg->c) > 0) {
				len += snprintf(buf + len, PAGE_SIZE - len,
					"%s[%-20s]: en\n", __func__,
					__clk_get_name(swcg->c));
				enable_count++;
			}
		}
		swcg++;
	}

	if (enable_count > 0)
		pr_notice("%s\n", buf);

	kfree(buf);

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
	if (ret)
		dump_subsys_reg(id);
}
#endif

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
#if ENABLE_PD_CHK_CG
		mtk_check_subsys_swcg(nb->priority);
#endif
		break;
	case GENPD_NOTIFY_ON:
		if (pd_sta[nb->priority] == POWER_OFF_STA)
			pd_debug_dump(nb->priority, PD_PWR_ON);
		if (pd_sta[nb->priority] == POWER_ON_STA)
			pd_log_dump(nb->priority, PD_PWR_ON);

		break;
	case GENPD_NOTIFY_OFF:
		if (pd_sta[nb->priority] == POWER_ON_STA)
			pd_debug_dump(nb->priority, PD_PWR_OFF);
		if (pd_sta[nb->priority] == POWER_OFF_STA)
			pd_log_dump(nb->priority, PD_PWR_OFF);

		break;
	default:
		pr_notice("cannot get flags identify\n");
		break;
	}

	return NOTIFY_OK;
}

static int set_genpd_notify(void)
{
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

		pd_pdev[i] = platform_device_alloc("power-domain-chk", 0);
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

		pds[i] = pd_to_genpd(pd_pdev[i]->dev.pm_domain);
		if (IS_ERR(pds[i])) {
			pr_err("pd-%d is err\n", i);
			break;
		}

		mtk_pd_notifier[i].notifier_call = mtk_pd_dbg_dump;
		mtk_pd_notifier[i].priority = i,
		r = dev_pm_genpd_add_notifier(&pd_pdev[i]->dev, &mtk_pd_notifier[i]);
		if (r) {
			pr_err("pd-%s notifier err\n", pds[i]->name);
			break;
		}

		pr_notice("pd-%s add to notifier\n", pds[i]->name);

		pm_runtime_enable(&pd_pdev[i]->dev);
		pm_runtime_get_noresume(&pd_pdev[i]->dev);
		pm_runtime_put_noidle(&pd_pdev[i]->dev);
	}

	return r;
}

static void pdchk_swcg_init_common(struct pd_check_swcg *swcg)
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

void pdchk_common_init(const struct pdchk_ops *ops)
{
	int i;

	pdchk_ops = ops;

	for (i = 0; i < MAX_PD_NUM; i++) {
		struct pd_check_swcg *swcg = get_subsys_cg(i);

		if (!swcg)
			continue;
		/* fill the 'struct clk *' ptr of every CGs*/
		pdchk_swcg_init_common(swcg);
	}

	set_genpd_notify();
}
EXPORT_SYMBOL(pdchk_common_init);

/*
 * pm_domain status check
 */

static int pdchk_pd_is_on(int pd_id)
{
	struct pd_sta *ps;

	if (pdchk_ops == NULL || pdchk_ops->get_pd_pwr_msk == NULL)
		return false;

	ps = pdchk_ops->get_pd_pwr_msk(pd_id);

	return pwr_hw_is_on(ps->sta_type, ps->pwr_mask);
}

static const char * const prm_status_name[] = {
	"active",
	"resuming",
	"suspended",
	"suspending",
};

static void pdchk_dump_enabled_power_domain(struct generic_pm_domain *pd)
{
	struct pm_domain_data *pdd;

	if (IS_ERR_OR_NULL(pd))
		return;

	list_for_each_entry(pdd, &pd->dev_list, list_node) {
		struct device *d = pdd->dev;
		struct platform_device *p = to_platform_device(d);

		pr_notice("\t%c (%-19s %3d :  %10s)\n",
				pm_runtime_active(d) ? '+' : '-',
				p->name,
				atomic_read(&d->power.usage_count),
				d->power.disable_depth ? "unsupport" :
				d->power.runtime_error ? "error" :
				d->power.runtime_status < ARRAY_SIZE(prm_status_name) ?
				prm_status_name[d->power.runtime_status] : "UFO");
	}
}

static bool __check_mtcmos_off(int *pd_id, bool dump_en)
{
	int valid = 0;

	for (; *pd_id != PD_NULL; pd_id++) {
		if (!pdchk_pd_is_on(*pd_id))
			continue;

		pr_notice("suspend warning[0m: %s is on\n", pds[*pd_id]->name);

		if (dump_en) {
			/* dump devicelist belongs to current power domain */
			pdchk_dump_enabled_power_domain(pds[*pd_id]);
			valid++;
		}
	}

	if (valid)
		return true;

	return false;
}

static bool check_mtcmos_off(void)
{
	int *pd_id;
	int ret = 0;

	if (pdchk_ops == NULL || pdchk_ops->get_off_mtcmos_id == NULL)
		goto OUT;

	pd_id = pdchk_ops->get_off_mtcmos_id();

	ret = __check_mtcmos_off(pd_id, true);

	if (pdchk_ops == NULL || pdchk_ops->get_notice_mtcmos_id == NULL)
		goto OUT;

	pd_id = pdchk_ops->get_notice_mtcmos_id();

	__check_mtcmos_off(pd_id, false);

	if (ret)
		return true;
OUT:
	return false;
}

static bool is_mtcmos_chk_bug_on(void)
{
	if (pdchk_ops == NULL || pdchk_ops->is_mtcmos_chk_bug_on == NULL)
		return false;

	return pdchk_ops->is_mtcmos_chk_bug_on();
}

static int pdchk_dev_pm_suspend(struct device *dev)
{
	if (check_mtcmos_off()) {
		if (is_mtcmos_chk_bug_on())
			BUG_ON(1);

		WARN_ON(1);
	}

	return 0;
}

const struct dev_pm_ops pdchk_dev_pm_ops = {
	.suspend = pdchk_dev_pm_suspend,
	.resume = NULL,
};
EXPORT_SYMBOL(pdchk_dev_pm_ops);

