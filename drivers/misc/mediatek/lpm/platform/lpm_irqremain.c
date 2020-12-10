// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/list.h>
#include <linux/slab.h>

#if IS_ENABLED(CONFIG_MTK_GIC_V3_EXT)
#include <linux/irqchip/mtk-gic-extend.h>
#endif

#include <lpm_plat_common.h>
#include <lpm_module.h>
#include <lpm_internal.h>

#include "lpm_irqremain.h"

#define LPM_IRQREMAIN_NODE		"irq-remain"
#define LPM_IRQREMAIN_LISTNODE		"irq-remain-list"

#define LPM_IRQREMAIN_LIST_METHOD	"method"
#define LPM_IRQREMAIN_TARGET		"target"
#define LPM_IRQREMAIN_PROP		"value"

#define FOR_EACH_IRQ_REMAIN(edges)\
		list_for_each_entry(edges,\
				&lpm_irqremain, list)


enum LPM_IRQREMAIN_TYPE {
	LPM_IRQREMAIN_LIST,
	LPM_IRQREMAIN_SMC,
};

struct lpm_irqremain_node {
	unsigned int irq;
	unsigned int wakeup_src_cat;
	unsigned int wakeup_src;
	struct list_head list;
};

static LIST_HEAD(lpm_irqremain);

#define PLAT_COVERT_IRQ_NUM(irq)	virq_to_hwirq(irq)

#if !IS_ENABLED(CONFIG_MTK_GIC_V3_EXT)
static inline unsigned int virq_to_hwirq(unsigned int virq)
{
	struct irq_desc *desc;
	unsigned int hwirq;

	desc = irq_to_desc(virq);
	WARN_ON(!desc);
	hwirq = desc ? desc->irq_data.hwirq : 0;
	return hwirq;
}
#endif

void lpm_irqremain_list_release(void)
{
	unsigned long flag;
	struct lpm_irqremain_node *cur;
	struct lpm_irqremain_node *next;

	if (list_empty(&lpm_irqremain))
		return;

	lpm_system_lock(flag);
	cur = list_first_entry(&lpm_irqremain,
				struct lpm_irqremain_node,
				list);
	do {
		if (list_is_last(&cur->list, &lpm_irqremain))
			next = NULL;
		else
			next = list_next_entry(cur, list);

		list_del(&cur->list);
		kfree(cur);
		cur = next;
	} while (cur);

	INIT_LIST_HEAD(&lpm_irqremain);
	lpm_system_unlock(flag);
}
EXPORT_SYMBOL(lpm_irqremain_list_release);

int lpm_irqremain_get(struct lpm_irqremain **irq)
{
	unsigned long flag;
	size_t count = 0;
	struct lpm_irqremain_node *irqnode;
	struct lpm_irqremain *tar;

	if (!irq)
		return -EINVAL;

	if (list_empty(&lpm_irqremain))
		return -EPERM;

	lpm_system_lock(flag);

	FOR_EACH_IRQ_REMAIN(irqnode)
		count++;

	*irq = NULL;

	tar = kcalloc(1, sizeof(**irq), GFP_KERNEL);

	if (!tar)
		goto lpm_irqremain_release;

	tar->irqs = kcalloc(count,
				sizeof(*tar->irqs), GFP_KERNEL);

	if (!tar->irqs)
		goto lpm_irqremain_release;

	tar->wakeup_src_cat = kcalloc(count,
		sizeof(*tar->wakeup_src_cat), GFP_KERNEL);

	if (!tar->wakeup_src_cat)
		goto lpm_irqremain_release;

	tar->wakeup_src = kcalloc(count,
				sizeof(*tar->irqs), GFP_KERNEL);

	if (!tar->wakeup_src)
		goto lpm_irqremain_release;

	tar->count = count;
	count = 0;

	FOR_EACH_IRQ_REMAIN(irqnode) {
		tar->irqs[count] = irqnode->irq;
		tar->wakeup_src_cat[count] = irqnode->wakeup_src_cat;
		tar->wakeup_src[count] = irqnode->wakeup_src;
		count++;
	}

	*irq = tar;
	lpm_system_unlock(flag);

	return 0;

lpm_irqremain_release:
	if (tar) {
		kfree(tar->irqs);
		kfree(tar->wakeup_src);
		kfree(tar);
	}
	lpm_system_unlock(flag);

	return -ENOMEM;
}
EXPORT_SYMBOL(lpm_irqremain_get);

void lpm_irqremain_put(struct lpm_irqremain *irqs)
{
	if (irqs) {
		kfree(irqs->irqs);
		kfree(irqs->wakeup_src);
		kfree(irqs);
	}
}
EXPORT_SYMBOL(lpm_irqremain_put);

int __init lpm_irqremain_parsing(struct device_node *parent)
{
	int ret = 0, idx = 0, irqnum;
	u32 irqidx = 0, irqtype, irq_do = 0;
	u32 wakeup_src = 0, wakeup_src_cat = 0;
	int remain_count = 0;
	struct lpm_irqremain_node *irqnode;
	const __be32 *next = NULL;
	struct property *prop;
	struct device_node *np = NULL;
	struct device_node *tar_np = NULL;

	while ((np = of_parse_phandle(parent, LPM_IRQREMAIN_NODE, idx))) {
		idx++;
		next = NULL;

		tar_np = of_parse_phandle(np, LPM_IRQREMAIN_TARGET, 0);
		prop = of_find_property(np, LPM_IRQREMAIN_PROP, NULL);

		do {

			if (!prop || !tar_np)
				break;

			next = of_prop_next_u32(prop, next, &irq_do);

			if (!next)
				break;

			next = of_prop_next_u32(prop, next, &irqidx);

			if (!next)
				break;

			next = of_prop_next_u32(prop, next, &wakeup_src_cat);

			if (!next)
				break;

			next = of_prop_next_u32(prop, next, &wakeup_src);

			if (!next)
				break;

			irqnum = of_irq_get(tar_np, irqidx);
			irqtype = irq_get_trigger_type(irqnum);

			if (!(irqtype & IRQ_TYPE_EDGE_BOTH))
				pr_info("[name:mtk_lpm][P] - %pOF irq(%d) type = 0x%x ?\n",
						tar_np, irqidx, irqtype);
			if (irq_do) {
				irq_set_irq_type(irqnum, irqtype);
				pr_info("[name:mtk_lpm][P] - %pOF set irq(%d) as type = 0x%x\n",
						tar_np, irqidx, irqtype);
			}

			if (irqnum >= 0) {
				irqnode = kcalloc(1, sizeof(*irqnode),
						  GFP_KERNEL);
				if (!irqnode)
					return -ENOMEM;

				irqnode->irq = irqnum;
				irqnode->wakeup_src_cat =
						wakeup_src_cat;
				irqnode->wakeup_src =
						wakeup_src;
				list_add(&irqnode->list,
					 &lpm_irqremain);

				pr_info("[name:mtk_lpm][P] - irq_%u, wakeup-src=0x%x (%pOF) (%s:%d)\n",
					irqnode->irq,
					irqnode->wakeup_src,
					tar_np,
					__func__, __LINE__);

				remain_count++;

			} else
				pr_info("[name:mtk_lpm][P] - invalid irq, erro=%d (%s:%d)\n",
						irqnum, __func__, __LINE__);

		} while (1);

		if (tar_np)
			of_node_put(tar_np);
		of_node_put(np);
	}

	lpm_smc_cpu_pm(IRQ_REMAIN_LIST_ALLOC, remain_count, 0, 0);
	lpm_smc_cpu_pm_lp(IRQS_REMAIN_ALLOC, MT_LPM_SMC_ACT_SET,
			      remain_count, 0);
	remain_count = 0;
	FOR_EACH_IRQ_REMAIN(irqnode) {
		lpm_smc_cpu_pm(IRQ_REMAIN_IRQ_ADD,
				   PLAT_COVERT_IRQ_NUM(irqnode->irq),
				   irqnode->wakeup_src_cat,
				   irqnode->wakeup_src);

		lpm_smc_cpu_pm_lp(IRQS_REMAIN_IRQ,
				   MT_LPM_SMC_ACT_SET,
				   PLAT_COVERT_IRQ_NUM(irqnode->irq), 0);
		lpm_smc_cpu_pm_lp(IRQS_REMAIN_WAKEUP_CAT,
				   MT_LPM_SMC_ACT_SET,
				   irqnode->wakeup_src_cat, 0);
		lpm_smc_cpu_pm_lp(IRQS_REMAIN_WAKEUP_SRC,
				   MT_LPM_SMC_ACT_SET,
				   irqnode->wakeup_src, 0);
		lpm_smc_cpu_pm_lp(IRQS_REMAIN_CTRL,
				   MT_LPM_SMC_ACT_PUSH, 0, 0);
		remain_count++;
	}
	lpm_smc_cpu_pm(IRQ_REMAIN_IRQ_SUBMIT, 0, 0, 0);
	lpm_smc_cpu_pm_lp(IRQS_REMAIN_CTRL,
			      MT_LPM_SMC_ACT_SUBMIT, 0, 0);
	return ret;
}

