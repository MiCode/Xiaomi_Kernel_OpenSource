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
#include <linux/types.h>

#include "clk-mtk.h"
#include "mtk-pd-chk.h"
#include "mt-plat/aee.h"

#define TAG			"[pdchk] "
#define MAX_PD_NUM		64
#define MAX_BUF_LEN		512
#define POWER_ON_STA		1
#define POWER_OFF_STA		0
#define ENABLE_PD_CHK_CG	0
#define DEVN_LEN			20

static struct platform_device *pd_pdev[MAX_PD_NUM];
static struct generic_pm_domain *pds[MAX_PD_NUM];
static struct notifier_block mtk_pd_notifier[MAX_PD_NUM];
static struct notifier_block mtk_pdchk_notifier;
static bool pd_sta[MAX_PD_NUM];

static const struct pdchk_ops *pdchk_ops;
static bool bug_on_stat;
static atomic_t check_enabled;

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

static void pdchk_trace_power_event(unsigned int id, unsigned int pwr_sta)
{
	if (pdchk_ops == NULL || pdchk_ops->trace_power_event == NULL)
		return;

	pdchk_ops->trace_power_event(id, pwr_sta);
}

static struct pd_check_swcg *get_subsys_cg(unsigned int id)
{
	if (pdchk_ops == NULL || pdchk_ops->get_subsys_cg == NULL)
		return NULL;

	return pdchk_ops->get_subsys_cg(id);
}

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

static int pdchk_suspend_is_in_usage(struct generic_pm_domain *pd)
{
	struct pm_domain_data *pdd;
	int valid = 0;

	if (IS_ERR_OR_NULL(pd))
		return 0;

	list_for_each_entry(pdd, &pd->dev_list, list_node) {
		struct device *d = pdd->dev;

		if (atomic_read(&d->power.usage_count) > 1)
			valid = 1;
	}

	return valid;
}

static void pdchk_dump_enabled_power_domain(struct generic_pm_domain *pd)
{
	struct pm_domain_data *pdd;

	if (IS_ERR_OR_NULL(pd))
		return;

	list_for_each_entry(pdd, &pd->dev_list, list_node) {
		struct device *d = pdd->dev;
		struct platform_device *p = to_platform_device(d);

		if (!p || !d)
			continue;

		pr_notice("\t%c (%-80s %3d %3d %3d %3d %3d:  %10s)\n",
				pm_runtime_active(d) ? '+' : '-',
				p->name,
				atomic_read(&d->power.usage_count),
				d->power.is_noirq_suspended,
				d->power.syscore,
				d->power.direct_complete,
				d->power.must_resume,
				//d->power.disable_depth ? "unsupport" :
				d->power.runtime_error ? "error" :
				d->power.runtime_status < ARRAY_SIZE(prm_status_name) ?
				prm_status_name[d->power.runtime_status] : "UFO");
	}

	return;
}

static bool __check_mtcmos_off(int *pd_id, bool dump_en)
{
	int valid = 0;

	for (; *pd_id != PD_NULL; pd_id++) {
		if (!pdchk_pd_is_on(*pd_id))
			continue;

		if (dump_en) {
			/* dump devicelist belongs to current power domain */
			if (pdchk_suspend_is_in_usage(pds[*pd_id]) > 0) {
				pr_notice("suspend warning[0m: %s is on\n", pds[*pd_id]->name);

				pdchk_dump_enabled_power_domain(pds[*pd_id]);
				valid++;
			}
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

static void pdchk_set_bug_on_stat(bool stat)
{
	bug_on_stat = stat;
}

bool pdchk_get_bug_on_stat(void)
{
	return bug_on_stat;
}
EXPORT_SYMBOL(pdchk_get_bug_on_stat);

static int pdchk_dev_pm_suspend(struct device *dev)
{
	atomic_inc(&check_enabled);
	if (check_mtcmos_off()) {
		if (is_mtcmos_chk_bug_on())
			pdchk_set_bug_on_stat(true);

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		aee_kernel_warning_api(__FILE__, __LINE__,
				DB_OPT_DEFAULT, "pd-chk",
				"fail to disable clk/pd in suspend\n");
#endif
	}

	return 0;
}

static int pdchk_dev_pm_resume(struct device *dev)
{
	atomic_dec(&check_enabled);

	return 0;
}

const struct dev_pm_ops pdchk_dev_pm_ops = {
	.suspend_noirq = pdchk_dev_pm_suspend,
	.resume_noirq = pdchk_dev_pm_resume,
};
EXPORT_SYMBOL(pdchk_dev_pm_ops);

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
		if (atomic_read(&check_enabled)
				&& pdchk_suspend_is_in_usage(pds[nb->priority])) {
			dump_stack();
			pdchk_dump_enabled_power_domain(pds[nb->priority]);
		}
		break;
	case GENPD_NOTIFY_PRE_OFF:
		pd_sta[nb->priority] = POWER_OFF_STA;
#if ENABLE_PD_CHK_CG
		mtk_check_subsys_swcg(nb->priority);
#endif
		if (atomic_read(&check_enabled)
				&& pdchk_suspend_is_in_usage(pds[nb->priority])) {
			dump_stack();
			pdchk_dump_enabled_power_domain(pds[nb->priority]);
		}
		break;
	case GENPD_NOTIFY_ON:
		if (pd_sta[nb->priority] == POWER_OFF_STA) {
			/* dump devicelist belongs to current power domain */
			pdchk_dump_enabled_power_domain(pds[nb->priority]);
			pd_debug_dump(nb->priority, PD_PWR_ON);
		}
		if (pd_sta[nb->priority] == POWER_ON_STA)
			pd_log_dump(nb->priority, PD_PWR_ON);

		pdchk_trace_power_event(nb->priority, POWER_ON_STA);

		break;
	case GENPD_NOTIFY_OFF:
		if (pd_sta[nb->priority] == POWER_ON_STA) {
			/* dump devicelist belongs to current power domain */
			pdchk_dump_enabled_power_domain(pds[nb->priority]);
			pd_debug_dump(nb->priority, PD_PWR_OFF);
		}
		if (pd_sta[nb->priority] == POWER_OFF_STA)
			pd_log_dump(nb->priority, PD_PWR_OFF);

		pdchk_trace_power_event(nb->priority, POWER_OFF_STA);

		break;
	default:
		pr_notice("cannot get flags identify\n");
		break;
	}

	return NOTIFY_OK;
}

static bool pdchk_suspend_allow(unsigned int id)
{
	int *pd_id;

	if (pdchk_ops == NULL || pdchk_ops->get_suspend_allow_id == NULL)
		return false;

	pd_id = pdchk_ops->get_suspend_allow_id();
	for (; *pd_id != PD_NULL; pd_id++) {
		if (*pd_id == id)
			return true;
	}

	return false;
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
		char pd_dev_name[DEVN_LEN];

		if(!is_in_pd_list(i))
			continue;

		pa.np = node;
		pa.args[0] = i;
		pa.args_count = 1;

		if (pdchk_suspend_allow(i)) {
			snprintf(pd_dev_name, DEVN_LEN, "power-domain-chk-%d", i);
			pd_pdev[i] = platform_device_register_simple(pd_dev_name, -1, NULL, 0);
		} else
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

	atomic_set(&check_enabled, 0);
	set_genpd_notify();
}
EXPORT_SYMBOL(pdchk_common_init);

static void pdchk_suspend_cg_dump(const char *name)
{
	if (!atomic_read(&check_enabled))
		return;

	if (pdchk_ops == NULL || pdchk_ops->suspend_cg_dump == NULL)
		return;

	pdchk_ops->suspend_cg_dump(name);
}

static int pdchk_evt_handling(struct notifier_block *nb,
			unsigned long flags, void *data)
{
	struct clk_event_data *clkd = (struct clk_event_data *)data;

	switch (clkd->event_type) {
	case CLK_EVT_SUSPEND_CG_DUMP:
		pdchk_suspend_cg_dump(clkd->name);
		break;
	default:
		pr_notice("cannot get flags identify\n");
		break;
	}

	return NOTIFY_OK;
}

int set_pdchk_notify(void)
{
	int r = 0;

	mtk_pdchk_notifier.notifier_call = pdchk_evt_handling;
	r = register_mtk_clk_notifier(&mtk_pdchk_notifier);
	if (r)
		pr_err("pd-chk notifier register err(%d)\n", r);

	return r;
}
EXPORT_SYMBOL(set_pdchk_notify);

