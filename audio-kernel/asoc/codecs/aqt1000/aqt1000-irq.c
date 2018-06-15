/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/irqdomain.h>
#include <linux/regmap.h>

#include "aqt1000-registers.h"
#include "aqt1000-irq.h"


static int aqt_irq_init(struct aqt1000_irq *aqt_irq)
{
	int i, ret;

	if (aqt_irq == NULL) {
		pr_err("%s: aqt_irq is NULL\n", __func__);
		return -EINVAL;
	}
	mutex_init(&aqt_irq->irq_lock);
	mutex_init(&aqt_irq->nested_irq_lock);

	aqt_irq->irq = aqt_irq_get_upstream_irq(aqt_irq);
	if (!aqt_irq->irq) {
		pr_warn("%s: irq driver is not yet initialized\n", __func__);
		mutex_destroy(&aqt_irq->irq_lock);
		mutex_destroy(&aqt_irq->nested_irq_lock);
		return -EPROBE_DEFER;
	}
	pr_debug("%s: probed irq %d\n", __func__, aqt_irq->irq);

	/* Setup downstream IRQs */
	ret = aqt_irq_setup_downstream_irq(aqt_irq);
	if (ret) {
		pr_err("%s: Failed to setup downstream IRQ\n", __func__);
		goto fail_irq_init;
	}

	/* mask all the interrupts */
	for (i = 0; i < aqt_irq->num_irqs; i++) {
		aqt_irq->irq_masks_cur |= BYTE_BIT_MASK(i);
		aqt_irq->irq_masks_cache |= BYTE_BIT_MASK(i);
	}

	ret = request_threaded_irq(aqt_irq->irq, NULL, aqt_irq_thread,
				   IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				   "aqt", aqt_irq);
	if (ret != 0) {
		dev_err(aqt_irq->dev, "Failed to request IRQ %d: %d\n",
			aqt_irq->irq, ret);
	} else {
		ret = enable_irq_wake(aqt_irq->irq);
		if (ret) {
			dev_err(aqt_irq->dev,
				"Failed to set wake interrupt on IRQ %d: %d\n",
				aqt_irq->irq, ret);
			free_irq(aqt_irq->irq, aqt_irq);
		}
	}

	if (ret)
		goto fail_irq_init;

	return ret;

fail_irq_init:
	dev_err(aqt_irq->dev,
			"%s: Failed to init aqt irq\n", __func__);
	aqt_irq_put_upstream_irq(aqt_irq);
	mutex_destroy(&aqt_irq->irq_lock);
	mutex_destroy(&aqt_irq->nested_irq_lock);
	return ret;
}

static int aqt_irq_probe(struct platform_device *pdev)
{
	int irq;
	struct aqt1000_irq *aqt_irq = NULL;
	int ret = -EINVAL;

	irq = platform_get_irq_byname(pdev, "aqt-int");
	if (irq < 0) {
		dev_err(&pdev->dev, "%s: Couldn't find aqt-int node(%d)\n",
			__func__, irq);
		return -EINVAL;
	}
	aqt_irq = kzalloc(sizeof(*aqt_irq), GFP_KERNEL);
	if (!aqt_irq)
		return -ENOMEM;
	/*
	 * AQT interrupt controller supports N to N irq mapping with
	 * single cell binding with irq numbers(offsets) only.
	 * Use irq_domain_simple_ops that has irq_domain_simple_map and
	 * irq_domain_xlate_onetwocell.
	 */
	aqt_irq->dev = &pdev->dev;
	aqt_irq->domain = irq_domain_add_linear(aqt_irq->dev->of_node,
				WSA_NUM_IRQS, &irq_domain_simple_ops,
				aqt_irq);
	if (!aqt_irq->domain) {
		dev_err(&pdev->dev, "%s: domain is NULL\n", __func__);
		ret = -ENOMEM;
		goto err;
	}
	aqt_irq->dev = &pdev->dev;

	dev_dbg(&pdev->dev, "%s: virq = %d\n", __func__, irq);
	aqt_irq->irq = irq;
	aqt_irq->num_irq_regs = 2;
	aqt_irq->num_irqs = WSA_NUM_IRQS;
	ret = aqt_irq_init(aqt_irq);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: failed to do irq init %d\n",
				__func__, ret);
		goto err;
	}

	return ret;
err:
	kfree(aqt_irq);
	return ret;
}

static int aqt_irq_remove(struct platform_device *pdev)
{
	struct irq_domain *domain;
	struct aqt1000_irq *data;

	domain = irq_find_host(pdev->dev.of_node);
	if (unlikely(!domain)) {
		pr_err("%s: domain is NULL\n", __func__);
		return -EINVAL;
	}
	data = (struct aqt_irq *)domain->host_data;
	data->irq = 0;

	return 0;
}

static const struct of_device_id of_match[] = {
	{ .compatible = "qcom,aqt-irq" },
	{ }
};

static struct platform_driver aqt_irq_driver = {
	.probe = aqt_irq_probe,
	.remove = aqt_irq_remove,
	.driver = {
		.name = "aqt_intc",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_match),
	},
};

static int aqt_irq_drv_init(void)
{
	return platform_driver_register(&aqt_irq_driver);
}
subsys_initcall(aqt_irq_drv_init);

static void aqt_irq_drv_exit(void)
{
	platform_driver_unregister(&aqt_irq_driver);
}
module_exit(aqt_irq_drv_exit);

MODULE_DESCRIPTION("AQT1000 IRQ driver");
MODULE_LICENSE("GPL v2");
