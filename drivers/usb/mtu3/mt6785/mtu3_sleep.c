/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>

#include "mtu3.h"
#include "mtu3_priv.h"

static int ipsleep_irqnum;
static int ipsleep_init;

static irqreturn_t ipsleep_eint_isr(int irqnum, void *data)
{
	disable_irq_nosync(irqnum);
	pr_info("ipsleep_eint\n");
	return IRQ_HANDLED;
}

static int ipsleep_eint_irq_en(struct device *dev)
{
	int ret = 0;

	ret = devm_request_irq(dev, ipsleep_irqnum,
		ipsleep_eint_isr, 0, "usbcd_eint", NULL);

	if (ret != 0) {
		dev_info(dev, "usbcd irq fail, ret %d, irqnum %d!!!\n",
			ret, ipsleep_irqnum);
	} else
		enable_irq_wake(ipsleep_irqnum);

	return ret;
}

static int mt_usb_ipsleep_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;

	ipsleep_irqnum = irq_of_parse_and_map(node, 0);
	if (ipsleep_irqnum < 0) {
		dev_info(dev, "get eint fail\n");
		return -ENODEV;
	}

	ret = ipsleep_eint_irq_en(dev);
	if (ret != 0)
		goto irqfail;

	ipsleep_init = 1;

irqfail:
	return ret;
}

static int mt_usb_ipsleep_remove(struct platform_device *pdev)
{
	free_irq(ipsleep_irqnum, NULL);
	return 0;
}

static const struct of_device_id usb_ipsleep_of_match[] = {
	{.compatible = "mediatek,usb_ipsleep"},
	{},
};

static struct platform_driver usb_ipsleep_driver = {
	.remove = mt_usb_ipsleep_remove,
	.probe = mt_usb_ipsleep_probe,
	.driver = {
		.name = "usb_ipsleep",
		.owner = THIS_MODULE,
		.of_match_table = usb_ipsleep_of_match,
	},
};

void ssusb_wakeup_mode_enable(struct ssusb_mtk *ssusb)
{
	if (ipsleep_init)
		enable_irq(ipsleep_irqnum);
}

void ssusb_wakeup_mode_disable(struct ssusb_mtk *ssusb)
{
	if (ipsleep_init)
		disable_irq(ipsleep_irqnum);
}

module_platform_driver(usb_ipsleep_driver);

