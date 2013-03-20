/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/spmi.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/qpnp-misc.h>

#define QPNP_MISC_DEV_NAME "qcom,qpnp-misc"

#define REVID_REVISION2	0x1

static DEFINE_MUTEX(qpnp_misc_dev_list_mutex);
static LIST_HEAD(qpnp_misc_dev_list);

/**
 * struct qpnp_misc_dev - holds controller device specific information
 * @list:			Doubly-linked list parameter linking to other
 *				qpnp_misc devices.
 * @mutex:			Mutex lock that is used to ensure mutual
 *				exclusion between probing and accessing misc
 *				driver information
 * @dev:			Device pointer to the misc device
 * @resource:			Resource pointer that holds base address
 * @spmi:			Spmi pointer which holds spmi information
 */
struct qpnp_misc_dev {
	struct list_head		list;
	struct mutex			mutex;
	struct device			*dev;
	struct resource			*resource;
	struct spmi_device		*spmi;
};

static struct of_device_id qpnp_misc_match_table[] = {
	{ .compatible = QPNP_MISC_DEV_NAME },
	{}
};

static u8 qpnp_read_byte(struct spmi_device *spmi, u16 addr)
{
	int rc;
	u8 val;

	rc = spmi_ext_register_readl(spmi->ctrl, spmi->sid, addr, &val, 1);
	if (rc) {
		pr_err("SPMI read failed rc=%d\n", rc);
		return 0;
	}
	return val;
}

#define REV2_IRQ_AVAILABLE_VERSION	2
static bool __misc_irqs_available(struct qpnp_misc_dev *dev)
{
	u8 rev2;

	rev2 = qpnp_read_byte(dev->spmi,
		dev->resource->start + REVID_REVISION2);
	pr_debug("rev2 0x%x\n", rev2);

	if (rev2 >= REV2_IRQ_AVAILABLE_VERSION)
		return 1;

	return 0;
}

int qpnp_misc_irqs_available(struct device *consumer_dev)
{
	struct device_node *misc_node = NULL;
	struct qpnp_misc_dev *mdev = NULL;
	struct qpnp_misc_dev *mdev_found = NULL;

	misc_node = of_parse_phandle(consumer_dev->of_node, "qcom,misc-ref", 0);
	if (!misc_node) {
		pr_debug("Could not find qcom,misc-ref property in %s\n",
			consumer_dev->of_node->full_name);
		return 0;
	}

	mutex_lock(&qpnp_misc_dev_list_mutex);
	list_for_each_entry(mdev, &qpnp_misc_dev_list, list) {
		if (mdev->dev->of_node == misc_node) {
			mdev_found = mdev;
			break;
		}
	}
	mutex_unlock(&qpnp_misc_dev_list_mutex);

	if (!mdev_found) {
		/* No MISC device was found. This API should only
		 * be called by drivers which have specified the
		 * misc phandle in their device tree node */
		pr_err("no probed misc device found\n");
		return -EPROBE_DEFER;
	}

	return __misc_irqs_available(mdev_found);
}

static int __devinit qpnp_misc_probe(struct spmi_device *spmi)
{
	struct resource *resource;
	struct qpnp_misc_dev *mdev = ERR_PTR(-EINVAL);

	resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!resource) {
		pr_err("Unable to get spmi resource for MISC\n");
		return -EINVAL;
	}

	mdev = kzalloc(sizeof(*mdev), GFP_KERNEL);
	if (!mdev) {
		pr_err("allocation failed\n");
		return -ENOMEM;
	}

	mdev->spmi = spmi;
	mdev->dev = &(spmi->dev);
	mdev->resource = resource;

	mutex_lock(&qpnp_misc_dev_list_mutex);
	list_add_tail(&mdev->list, &qpnp_misc_dev_list);
	mutex_unlock(&qpnp_misc_dev_list_mutex);

	pr_debug("probed successfully\n");
	return 0;
}

static struct spmi_driver qpnp_misc_driver = {
	.probe	= qpnp_misc_probe,
	.driver	= {
		.name		= QPNP_MISC_DEV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= qpnp_misc_match_table,
	},
};

static int __init qpnp_misc_init(void)
{
	return spmi_driver_register(&qpnp_misc_driver);
}

static void __exit qpnp_misc_exit(void)
{
	return spmi_driver_unregister(&qpnp_misc_driver);
}

module_init(qpnp_misc_init);
module_exit(qpnp_misc_exit);

MODULE_DESCRIPTION(QPNP_MISC_DEV_NAME);
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" QPNP_MISC_DEV_NAME);
