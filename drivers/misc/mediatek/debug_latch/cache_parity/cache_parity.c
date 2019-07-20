/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Mars.Cheng <mars.cheng@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/printk.h>
#include <linux/bug.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/irqreturn.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <mt-plat/aee.h>
#include <cache_parity.h>

static DEFINE_SPINLOCK(parity_isr_lock);
void __iomem *parity_debug_base;
static unsigned int err_level;
static unsigned int irq_count;
static unsigned int version;
static struct parity_irq_record_t *parity_irq_record;

static irqreturn_t (*custom_parity_isr)(int irq, void *dev_id);
static int cache_parity_probe(struct platform_device *pdev);

static const struct of_device_id cache_parity_of_ids[] = {
	{   .compatible = "mediatek,cache_parity", },
	{}
};

static struct platform_driver cache_parity_drv = {
	.driver = {
		.name = "cache_parity",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = cache_parity_of_ids,
	},
	.probe = cache_parity_probe,
};

static irqreturn_t default_parity_isr_v1(int irq, void *dev_id)
{
	struct parity_record_t *parity_record;
	unsigned int status;
	unsigned int offset;
	unsigned int irq_idx;
	unsigned int i;

	for (i = 0, parity_record = NULL; i < irq_count; i++) {
		if (parity_irq_record[i].irq == irq) {
			irq_idx = i;
			parity_record = &(parity_irq_record[i].parity_record);
			pr_info("parity isr for %d\n", i);
			break;
		}
	}

	if (parity_record == NULL) {
		pr_info("no matched irq %d\n", irq);
		return IRQ_HANDLED;
	}

	status = readl(parity_debug_base + parity_record->check_offset);
	pr_info("status 0x%x\n", status);

	if (status & parity_record->check_mask)
		pr_info("detect cache parity error\n");
	else
		pr_info("no cache parity error\n");

	for (i = 0; i < parity_record->dump_length; i += 4) {
		offset = parity_record->dump_offset + i;
		pr_info("offset 0x%x, val 0x%x\n", offset,
			readl(parity_debug_base + offset));
	}

#ifdef CONFIG_MTK_ENG_BUILD
	WARN_ON(1);
#else
	if (err_level) {
		aee_kernel_exception("cache parity",
			"cache parity error,%s:%d,%s:0x%x\n\n%s\n",
			"irq_index", irq_idx,
			"status", status,
			"CRDISPATCH_KEY:Cache Parity Issue");
	} else
		WARN_ON(1);
#endif

	spin_lock(&parity_isr_lock);

	if (parity_record->clear_mask) {
		writel(parity_record->clear_mask,
			parity_debug_base + parity_record->clear_offset);
		dsb(sy);
		writel(0x0,
			parity_debug_base + parity_record->clear_offset);
		dsb(sy);

		while (readl(parity_debug_base + parity_record->check_offset) &
			parity_record->check_mask) {
			udelay(1);
		}
	}

	spin_unlock(&parity_isr_lock);

	return IRQ_HANDLED;
}

void __attribute__((weak)) cache_parity_init_platform(void)
{
	pr_info("[%s] adopt default flow\n", __func__);
}

static int cache_parity_probe_v1(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct parity_irq_config_t *parity_irq_config;
	size_t size;
	unsigned int i;
	unsigned int target_cpu;
	int ret;
	int irq;

	cache_parity_init_platform();

	ret = of_property_read_u32(node, "err_level", &err_level);
	if (ret)
		return ret;

	irq_count = of_irq_count(node);
	pr_info("irq_count: %d, err_level: %d\n", irq_count, err_level);

	size = sizeof(struct parity_irq_record_t) * irq_count;
	parity_irq_record = kmalloc(size, GFP_KERNEL);
	if (!parity_irq_record)
		return -ENOMEM;

	size = sizeof(struct parity_irq_config_t) * irq_count;
	parity_irq_config = kmalloc(size, GFP_KERNEL);
	if (!parity_irq_config)
		return -ENOMEM;

	size = size >> 2;
	of_property_read_variable_u32_array(node, "irq_config",
		(u32 *)parity_irq_config, size, size);

	for (i = 0; i < irq_count; i++) {
		memcpy(
			&(parity_irq_record[i].parity_record),
			&(parity_irq_config[i].parity_record),
			sizeof(struct parity_record_t));

		irq = irq_of_parse_and_map(node, i);
		parity_irq_record[i].irq = irq;
		pr_info("get %d for %d\n", irq, i);

		target_cpu = parity_irq_config[i].target_cpu;
		if (target_cpu != 1024) {
			ret = irq_set_affinity(irq, cpumask_of(target_cpu));
			if (ret)
				pr_info("target_cpu(%d) fail\n", i);
		}

		if (custom_parity_isr)
			ret = request_irq(irq, custom_parity_isr,
				IRQF_TRIGGER_NONE, "cache_parity",
				&cache_parity_drv);
		else
			ret = request_irq(irq, default_parity_isr_v1,
				IRQF_TRIGGER_NONE, "cache_parity",
				&cache_parity_drv);
		if (ret != 0)
			pr_info("request_irq(%d) fail\n", i);
	}

	kfree(parity_irq_config);

	return 0;
}

static int cache_parity_probe(struct platform_device *pdev)
{
	int ret;

	parity_debug_base = of_iomap(pdev->dev.of_node, 0);
	if (!parity_debug_base)
		return -ENOMEM;

	ret = of_property_read_u32(pdev->dev.of_node, "version", &version);
	if (ret)
		return ret;

	switch (version) {
	case 1:
		return cache_parity_probe_v1(pdev);
	default:
		pr_info("unsupported version\n");
		return 0;
	}
}

static int __init cache_parity_init(void)
{
	return platform_driver_register(&cache_parity_drv);
}

module_init(cache_parity_init);
