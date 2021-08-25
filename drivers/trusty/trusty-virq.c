/*
 * Copyright 2018 GoldenRiver Technologies Co., Ltd. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/arm-gic-v3.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sort.h>

#include "trusty-virq.h"

#define MAX_VIRQ_DEV 4
#define LESS	(-1)
#define EQUAL	(0)
#define GREATER	(1)

static struct virq_device *virqdev[MAX_VIRQ_DEV];
static u32 virqdev_num;

static irqreturn_t virq_handler(int irq, void *data)
{
	struct virq_device *dev = data;
	ulong payload = okl4_get_virq_payload(irq);

	dev->virq.payload |= payload;

	/* Ensure all PEs see the latest payload. */
	smp_wmb();
	dev->virq.raised = true;

	wake_up_interruptible(&dev->virq.wq);
	return IRQ_HANDLED;
}

struct virq_device *trusty_get_virq_device(u32 virqidx)
{
	if (virqidx >= MAX_VIRQ_DEV)
		return NULL;

	return virqdev[virqidx];
}

s32 trusty_virq_recv(u32 virqidx, ulong *out)
{
	ulong payload;
	int ret;
	struct virq_device *dev = trusty_get_virq_device(virqidx);

	if (!dev)
		return -EINVAL;

	do {
		ret = wait_event_interruptible(dev->virq.wq, dev->virq.raised);
		if (ret < 0)
			return ret;
	} while (!xchg(&dev->virq.raised, 0));

	/* Ensure all PEs see the latest payload. */
	smp_rmb();
	payload = xchg(&dev->virq.payload, 0);

	if (out)
		*out = payload;

	dev_dbg(dev->dev, "%s: irq %d hwirq %d payload 0x%lx\n",
			__func__, dev->virq.irqno, dev->virq.hwirq, payload);

	return 0;
}

s32 trusty_virq_send(u32 virqidx, ulong payload)
{
	okl4_error_t err;
	struct virq_device *dev = trusty_get_virq_device(virqidx);

	if (!dev) {
		dev_err(dev->dev, "virqidx %u invalid, virqdev_num = %u\n",
				virqidx, virqdev_num);
		return -EINVAL;
	}

	err = _okl4_sys_vinterrupt_raise(dev->source.kcap, payload);
	if (err != OKL4_OK) {
		dev_err(dev->dev, "failed to raise virq %u err %u payload 0x%lx\n",
				dev->source.kcap, err, payload);
		return  -EIO;
	}

	dev_dbg(dev->dev, "%s: irq %d hwirq %d payload 0x%lx\n",
			__func__, dev->virq.irqno, dev->virq.hwirq, payload);

	return 0;
}

s32 trusty_virq_smc(u32 virqidx, ulong r0, ulong r1, ulong r2, ulong r3)
{
	unsigned long ret = 0;
	struct virq_device *dev = trusty_get_virq_device(virqidx);

	trusty_virq_send(virqidx, r0);
	trusty_virq_recv(virqidx, NULL);
	trusty_virq_send(virqidx, r1);
	trusty_virq_recv(virqidx, NULL);
	trusty_virq_send(virqidx, r2);
	trusty_virq_recv(virqidx, NULL);
	trusty_virq_send(virqidx, r3);
	trusty_virq_recv(virqidx, &ret);

	dev_dbg(dev->dev, "%s(%u): 0x%lx 0x%lx 0x%lx 0x%lx done (0x%lx)\n",
			__func__, virqidx, r0, r1, r2, r3, ret);

	return ret;
}

static int virqdev_compare(const void *lhs, const void *rhs)
{
	const struct virq_device *l = lhs;
	const struct virq_device *r = rhs;

	if (l->virq.hwirq < r->virq.hwirq)
		return LESS;
	if (l->virq.hwirq > r->virq.hwirq)
		return GREATER;
	return EQUAL;
}

static int trusty_virq_probe(struct platform_device *pdev)
{
	int ret, i;
	struct device_node *node = pdev->dev.of_node;
	struct virq_device *vdev = NULL;
	struct irq_desc *desc;

	if (!node) {
		dev_err(&pdev->dev, "of_node required\n");
		return -ENODEV;
	}

	if (virqdev_num >= MAX_VIRQ_DEV) {
		dev_err(&pdev->dev, "virq devices > MAX_VIRQ_DEV\n");
		return -ENOMEM;
	}

	vdev = kzalloc(sizeof(*vdev), GFP_KERNEL);
	if (!vdev)
		return -ENOMEM;

	vdev->dev = &pdev->dev;
	vdev->virq.raised = false;
	vdev->virq.payload = 0;
	init_waitqueue_head(&vdev->virq.wq);

	if (!of_device_is_compatible(node, "okl,microvisor-interrupt-line")) {
		ret = -EINVAL;
		goto err_virq_register;
	}
	of_property_read_u32(node, "reg", &vdev->source.kcap);

	vdev->virq.irqno = platform_get_irq(pdev, 0);
	if (vdev->virq.irqno < 0) {
		ret = -EINVAL;
		goto err_virq_register;
	}

	ret = devm_request_irq(&pdev->dev, vdev->virq.irqno,
			virq_handler, 0, "virq", vdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request_irq: %d\n", ret);
		goto err_virq_register;
	}

	desc = irq_to_desc(vdev->virq.irqno);
	if (desc)
		vdev->virq.hwirq = desc->irq_data.hwirq;

	virqdev[virqdev_num++] = vdev;
	sort(virqdev, virqdev_num, sizeof(u64), &virqdev_compare, NULL);

	dev_info(&pdev->dev, "current %d virq devices registered.\n",
			virqdev_num);

	for (i = 0; i < virqdev_num; i++) {
		dev_info(&pdev->dev, "  virqdev[%d] irqno %d hwirq %d kcap %d\n",
				i, virqdev[i]->virq.irqno,
				virqdev[i]->virq.hwirq,
				virqdev[i]->source.kcap);
	}

	return 0;

err_virq_register:
	kfree(vdev);
	return ret;
}

static const struct of_device_id trusty_virq_of_match[] = {
	{ .compatible = "okl,user-virq", },
	{},
};

static struct platform_driver trusty_virq_driver = {
	.probe = trusty_virq_probe,
	.driver	= {
		.name = "trusty-virq",
		.owner = THIS_MODULE,
		.of_match_table = trusty_virq_of_match,
	},
};

static int __init trusty_virq_init(void)
{
	return platform_driver_register(&trusty_virq_driver);
}

static void __exit trusty_virq_exit(void)
{
	platform_driver_unregister(&trusty_virq_driver);
}

arch_initcall(trusty_virq_init);
module_exit(trusty_virq_exit);
