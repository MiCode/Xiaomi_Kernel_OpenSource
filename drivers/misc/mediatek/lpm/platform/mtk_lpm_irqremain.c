// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/list.h>
#include <linux/slab.h>

#if defined(CONFIG_MTK_GIC_V3_EXT)
#include <linux/irqchip/mtk-gic-extend.h>
#endif

#include <mtk_lpm_platform.h>
#include <mtk_lpm_module.h>
#include <mtk_lpm_internal.h>

#include "mtk_lpm_irqremain.h"

#define MTK_LPM_IRQREMAIN_NODE		"irq-remain"
#define MTK_LPM_IRQREMAIN_LISTNODE	"irq-remain-list"

#define MTK_LPM_IRQREMAIN_LIST_METHOD	"method"
#define MTK_LPM_IRQREMAIN_TARGET	"target"
#define MTK_LPM_IRQREMAIN_PROP		"value"

#define FOR_EACH_IRQ_REMAIN(edges)\
		list_for_each_entry(edges,\
				&mtk_irqremain, list)

#define IRQ_REMAIN_FREE(_irq_remain) ({\
	if (_irq_remain) {\
		kfree(_irq_remain->irqs);\
		kfree(_irq_remain->wakeup_src_cat);\
		kfree(_irq_remain->wakeup_src);\
		kfree(_irq_remain);\
	} })


enum MTK_LPM_IRQREMAIN_TYPE {
	MTK_LPM_IRQREMAIN_LIST,
	MTK_LPM_IRQREMAIN_SMC,
};

struct mtk_lpm_irqremain_node {
	unsigned int irq;
	unsigned int wakeup_src_cat;
	unsigned int wakeup_src;
	struct list_head list;
};

static LIST_HEAD(mtk_irqremain);

#define PLAT_COVERT_IRQ_NUM(irq)	virq_to_hwirq(irq)

#if !defined(CONFIG_MTK_GIC_V3_EXT)
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

void mtk_lpm_irqremain_list_release(void)
{
	unsigned long flag;
	struct mtk_lpm_irqremain_node *cur;
	struct mtk_lpm_irqremain_node *next;

	if (list_empty(&mtk_irqremain))
		return;

	mtk_lpm_system_lock(flag);
	cur = list_first_entry(&mtk_irqremain,
				struct mtk_lpm_irqremain_node,
				list);
	do {
		if (list_is_last(&cur->list, &mtk_irqremain))
			next = NULL;
		else
			next = list_next_entry(cur, list);

		list_del(&cur->list);
		kfree(cur);
		cur = next;
	} while (cur);

	INIT_LIST_HEAD(&mtk_irqremain);
	mtk_lpm_system_unlock(flag);
}
EXPORT_SYMBOL(mtk_lpm_irqremain_list_release);

int mtk_lpm_irqremain_get(struct mtk_lpm_irqremain **irq)
{
	unsigned long flag;
	size_t count = 0;
	struct mtk_lpm_irqremain_node *irqnode;
	struct mtk_lpm_irqremain *tar;

	if (!irq)
		return -EINVAL;

	if (list_empty(&mtk_irqremain))
		return -EPERM;

	mtk_lpm_system_lock(flag);

	FOR_EACH_IRQ_REMAIN(irqnode)
		count++;

	*irq = NULL;

	tar = kcalloc(1, sizeof(**irq), GFP_KERNEL);

	if (!tar)
		goto mtk_lpm_irqremain_release;

	tar->irqs = tar->wakeup_src_cat = tar->wakeup_src = NULL;

	tar->irqs = kcalloc(count,
				sizeof(*tar->irqs), GFP_KERNEL);

	if (!tar->irqs)
		goto mtk_lpm_irqremain_release;

	tar->wakeup_src_cat = kcalloc(count,
		sizeof(*tar->wakeup_src_cat), GFP_KERNEL);

	if (!tar->wakeup_src_cat)
		goto mtk_lpm_irqremain_release;

	tar->wakeup_src = kcalloc(count,
				sizeof(*tar->wakeup_src), GFP_KERNEL);

	if (!tar->wakeup_src)
		goto mtk_lpm_irqremain_release;

	tar->count = count;
	count = 0;

	FOR_EACH_IRQ_REMAIN(irqnode) {
		tar->irqs[count] = irqnode->irq;
		tar->wakeup_src_cat[count] = irqnode->wakeup_src_cat;
		tar->wakeup_src[count] = irqnode->wakeup_src;
		count++;
	}

	*irq = tar;
	mtk_lpm_system_unlock(flag);

	return 0;

mtk_lpm_irqremain_release:
	IRQ_REMAIN_FREE(tar);
	mtk_lpm_system_unlock(flag);

	return -ENOMEM;
}
EXPORT_SYMBOL(mtk_lpm_irqremain_get);

void mtk_lpm_irqremain_put(struct mtk_lpm_irqremain *irqs)
{
	IRQ_REMAIN_FREE(irqs);
}
EXPORT_SYMBOL(mtk_lpm_irqremain_put);

int __init mtk_lpm_irqremain_parsing(struct device_node *parent)
{
	int ret = 0, idx = 0, irqnum;
	u32 irqidx = 0, irqtype, irq_do = 0;
	u32 wakeup_src = 0, wakeup_src_cat = 0;
	int remain_count = 0;
	struct mtk_lpm_irqremain_node *irqnode;
	const __be32 *next = NULL;
	struct property *prop;
	struct device_node *np = NULL;
	struct device_node *tar_np = NULL;

	while ((np = of_parse_phandle(parent, MTK_LPM_IRQREMAIN_NODE, idx))) {
		idx++;
		next = NULL;

		tar_np = of_parse_phandle(np, MTK_LPM_IRQREMAIN_TARGET, 0);
		prop = of_find_property(np, MTK_LPM_IRQREMAIN_PROP, NULL);

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

				if (irqnode) {
					irqnode->irq =
						PLAT_COVERT_IRQ_NUM(irqnum);
					irqnode->wakeup_src_cat =
							wakeup_src_cat;
					irqnode->wakeup_src =
							wakeup_src;
					list_add(&irqnode->list,
						 &mtk_irqremain);

					pr_info("[name:mtk_lpm][P] - irq_%u, wakeup-src=0x%x (%pOF) (%s:%d)\n",
						irqnode->irq,
						irqnode->wakeup_src,
						tar_np,
						__func__, __LINE__);

					remain_count++;
				}
			} else
				pr_info("[name:mtk_lpm][P] - invalid irq, erro=%d (%s:%d)\n",
						irqnum, __func__, __LINE__);

		} while (1);

		if (tar_np)
			of_node_put(tar_np);
		of_node_put(np);
	}

	mtk_lpm_smc_cpu_pm(IRQ_REMAIN_LIST_ALLOC, remain_count, 0, 0);
	mtk_lpm_smc_cpu_pm_lp(IRQS_REMAIN_ALLOC, MT_LPM_SMC_ACT_SET,
			      remain_count, 0);
	remain_count = 0;
	FOR_EACH_IRQ_REMAIN(irqnode) {
		mtk_lpm_smc_cpu_pm(IRQ_REMAIN_IRQ_ADD,
				   irqnode->irq,
				   irqnode->wakeup_src_cat,
				   irqnode->wakeup_src);

		mtk_lpm_smc_cpu_pm_lp(IRQS_REMAIN_IRQ,
				   MT_LPM_SMC_ACT_SET,
				   irqnode->irq, 0);
		mtk_lpm_smc_cpu_pm_lp(IRQS_REMAIN_WAKEUP_CAT,
				   MT_LPM_SMC_ACT_SET,
				   irqnode->wakeup_src_cat, 0);
		mtk_lpm_smc_cpu_pm_lp(IRQS_REMAIN_WAKEUP_SRC,
				   MT_LPM_SMC_ACT_SET,
				   irqnode->wakeup_src, 0);
		mtk_lpm_smc_cpu_pm_lp(IRQS_REMAIN_CTRL,
				   MT_LPM_SMC_ACT_PUSH, 0, 0);
		remain_count++;
	}
	mtk_lpm_smc_cpu_pm(IRQ_REMAIN_IRQ_SUBMIT, 0, 0, 0);
	mtk_lpm_smc_cpu_pm_lp(IRQS_REMAIN_CTRL,
			      MT_LPM_SMC_ACT_SUBMIT, 0, 0);
	return ret;
}

