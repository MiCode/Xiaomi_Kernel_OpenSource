/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/export.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <soc/qcom/memory_dump.h>
#include <soc/qcom/icnss.h>

struct ce_irq_list {
	int irq;
	irqreturn_t (*handler)(int, void *);
};

static struct {
	struct platform_device *pdev;
	struct icnss_driver_ops *ops;
	struct ce_irq_list ce_irq_list[ICNSS_MAX_IRQ_REGISTRATIONS];
	u32 ce_irqs[ICNSS_MAX_IRQ_REGISTRATIONS];
	phys_addr_t mem_base_pa;
	void __iomem *mem_base_va;
} *penv;

int icnss_register_driver(struct icnss_driver_ops *ops)
{
	struct platform_device *pdev;
	int ret = 0;

	if (!penv || !penv->pdev) {
		ret = -ENODEV;
		goto out;
	}

	pdev = penv->pdev;
	if (!pdev) {
		ret = -ENODEV;
		goto out;
	}

	if (penv->ops) {
		pr_err("icnss: driver already registered\n");
		ret = -EEXIST;
		goto out;
	}
	penv->ops = ops;

	/* check for all conditions before invoking probe */
	if (penv->ops->probe)
		ret = penv->ops->probe(&pdev->dev);

out:
	return ret;
}
EXPORT_SYMBOL(icnss_register_driver);

int icnss_unregister_driver(struct icnss_driver_ops *ops)
{
	int ret = 0;
	struct platform_device *pdev;

	if (!penv || !penv->pdev) {
		ret = -ENODEV;
		goto out;
	}

	pdev = penv->pdev;
	if (!pdev) {
		ret = -ENODEV;
		goto out;
	}
	if (!penv->ops) {
		pr_err("icnss: driver not registered\n");
		ret = -ENOENT;
		goto out;
	}
	if (penv->ops->remove)
		penv->ops->remove(&pdev->dev);

	penv->ops = NULL;
out:
	return ret;
}
EXPORT_SYMBOL(icnss_unregister_driver);

int icnss_register_ce_irq(unsigned int ce_id,
	irqreturn_t (*handler)(int, void *),
		unsigned long flags, const char *name)
{
	int ret = 0;
	unsigned int irq;
	struct ce_irq_list *irq_entry;

	if (!penv || !penv->pdev) {
		ret = -ENODEV;
		goto out;
	}
	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS) {
		pr_err("icnss: Invalid CE ID %d\n", ce_id);
		ret = -EINVAL;
		goto out;
	}
	irq = penv->ce_irqs[ce_id];
	irq_entry = &penv->ce_irq_list[ce_id];

	if (irq_entry->handler || irq_entry->irq) {
		pr_err("icnss: handler already registered %d\n", irq);
		ret = -EEXIST;
		goto out;
	}

	ret = request_irq(irq, handler, IRQF_SHARED, name, &penv->pdev->dev);
	if (ret) {
		pr_err("icnss: IRQ not registered %d\n", irq);
		ret = -EINVAL;
		goto out;
	}
	irq_entry->irq = irq;
	irq_entry->handler = handler;
	pr_debug("icnss: IRQ registered %d\n", irq);
out:
	return ret;

}
EXPORT_SYMBOL(icnss_register_ce_irq);

int icnss_unregister_ce_irq(unsigned int ce_id)
{
	int ret = 0;
	unsigned int irq;
	struct ce_irq_list *irq_entry;

	if (!penv || !penv->pdev) {
		ret = -ENODEV;
		goto out;
	}
	irq = penv->ce_irqs[ce_id];
	irq_entry = &penv->ce_irq_list[ce_id];
	if (!irq_entry->handler || !irq_entry->irq) {
		pr_err("icnss: handler not registered %d\n", irq);
		ret = -EEXIST;
		goto out;
	}
	free_irq(irq, &penv->pdev->dev);
	irq_entry->irq = 0;
	irq_entry->handler = NULL;
out:
	return ret;
}
EXPORT_SYMBOL(icnss_unregister_ce_irq);

int icnss_ce_request_irq(unsigned int ce_id,
	irqreturn_t (*handler)(int, void *),
		unsigned long flags, const char *name, void *ctx)
{
	int ret = 0;
	unsigned int irq;
	struct ce_irq_list *irq_entry;

	if (!penv || !penv->pdev) {
		ret = -ENODEV;
		goto out;
	}
	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS) {
		pr_err("icnss: Invalid CE ID %d\n", ce_id);
		ret = -EINVAL;
		goto out;
	}
	irq = penv->ce_irqs[ce_id];
	irq_entry = &penv->ce_irq_list[ce_id];

	if (irq_entry->handler || irq_entry->irq) {
		pr_err("icnss: handler already registered %d\n", irq);
		ret = -EEXIST;
		goto out;
	}

	ret = request_irq(irq, handler, flags, name, ctx);
	if (ret) {
		pr_err("icnss: IRQ not registered %d\n", irq);
		ret = -EINVAL;
		goto out;
	}
	irq_entry->irq = irq;
	irq_entry->handler = handler;
	pr_debug("icnss: IRQ registered %d\n", irq);
out:
	return ret;
}
EXPORT_SYMBOL(icnss_ce_request_irq);

int icnss_ce_free_irq(unsigned int ce_id, void *ctx)
{
	int ret = 0;
	unsigned int irq;
	struct ce_irq_list *irq_entry;

	if (!penv || !penv->pdev) {
		ret = -ENODEV;
		goto out;
	}
	irq = penv->ce_irqs[ce_id];
	irq_entry = &penv->ce_irq_list[ce_id];
	if (!irq_entry->handler || !irq_entry->irq) {
		pr_err("icnss: handler not registered %d\n", irq);
		ret = -EEXIST;
		goto out;
	}
	free_irq(irq, ctx);
	irq_entry->irq = 0;
	irq_entry->handler = NULL;
out:
	return ret;
}
EXPORT_SYMBOL(icnss_ce_free_irq);

void icnss_enable_irq(unsigned int ce_id)
{
	unsigned int irq;

	if (!penv || !penv->pdev) {
		pr_err("icnss: platform driver not initialized\n");
		return;
	}
	irq = penv->ce_irqs[ce_id];
	enable_irq(irq);
}
EXPORT_SYMBOL(icnss_enable_irq);

void icnss_disable_irq(unsigned int ce_id)
{
	unsigned int irq;

	if (!penv || !penv->pdev) {
		pr_err("icnss: platform driver not initialized\n");
		return;
	}
	irq = penv->ce_irqs[ce_id];
	disable_irq(irq);
}
EXPORT_SYMBOL(icnss_disable_irq);

int icnss_get_soc_info(struct icnss_soc_info *info)
{
	if (!penv) {
		pr_err("icnss: platform driver not initialized\n");
		return -EINVAL;
	}

	info->v_addr = penv->mem_base_va;
	info->p_addr = penv->mem_base_pa;

	return 0;
}
EXPORT_SYMBOL(icnss_get_soc_info);

int icnss_wlan_enable(struct icnss_wlan_enable_cfg *config,
		enum icnss_driver_mode mode)
{
	return 0;
}
EXPORT_SYMBOL(icnss_wlan_enable);

int icnss_wlan_disable(enum icnss_driver_mode mode)
{
	return 0;
}
EXPORT_SYMBOL(icnss_wlan_disable);

int icnss_get_ce_id(int irq)
{
	int i;

	for (i = 0; i < ICNSS_MAX_IRQ_REGISTRATIONS; i++) {
		if (penv->ce_irqs[i] == irq)
			return i;
	}
	pr_err("icnss: No matching CE id for irq %d\n", irq);
	return -EINVAL;
}
EXPORT_SYMBOL(icnss_get_ce_id);

static int icnss_probe(struct platform_device *pdev)
{
	int ret = 0;
	int len = 0;
	struct resource *res;
	int i;

	if (penv)
		return -EEXIST;

	penv = devm_kzalloc(&pdev->dev, sizeof(*penv), GFP_KERNEL);
	if (!penv)
		return -ENOMEM;

	penv->pdev = pdev;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "membase");
	if (!res) {
		pr_err("icnss: Memory base not found\n");
		ret = -EINVAL;
		goto out;
	}
	penv->mem_base_pa = res->start;
	penv->mem_base_va = ioremap(penv->mem_base_pa, resource_size(res));
	if (!penv->mem_base_va) {
		pr_err("icnss: ioremap failed\n");
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < ICNSS_MAX_IRQ_REGISTRATIONS; i++) {
		res = platform_get_resource(pdev, IORESOURCE_IRQ, i);
		if (!res) {
			pr_err("icnss: Fail to get IRQ-%d\n", i);
			ret = -ENODEV;
			goto out;
		} else {
			penv->ce_irqs[i] = res->start;
		}
	}

	pr_debug("icnss: Platform driver probed successfully\n");
out:
	return ret;
}

static int icnss_remove(struct platform_device *pdev)
{
	return 0;
}


static const struct of_device_id icnss_dt_match[] = {
	{.compatible = "qcom,icnss"},
	{}
};

MODULE_DEVICE_TABLE(of, icnss_dt_match);

static struct platform_driver icnss_driver = {
	.probe  = icnss_probe,
	.remove = icnss_remove,
	.driver = {
		.name = "icnss",
		.owner = THIS_MODULE,
		.of_match_table = icnss_dt_match,
	},
};

static int __init icnss_initialize(void)
{
	return platform_driver_register(&icnss_driver);
}

static void __exit icnss_exit(void)
{
	platform_driver_unregister(&icnss_driver);
}


module_init(icnss_initialize);
module_exit(icnss_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DEVICE "iCNSS CORE platform driver");
