/* Copyright (c) 2013-2014,2016, The Linux Foundation. All rights reserved.
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

#define REG_DIG_MAJOR_REV	0x01
#define REG_SUBTYPE		0x05
#define REG_PWM_SEL		0x49
#define REG_GP_DRIVER_EN	0x4C

#define PWM_SEL_MAX		0x03
#define GP_DRIVER_EN_BIT	BIT(0)

static DEFINE_MUTEX(qpnp_misc_dev_list_mutex);
static LIST_HEAD(qpnp_misc_dev_list);

struct qpnp_misc_version {
	u8	subtype;
	u8	dig_major_rev;
};

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
 * @version:			struct that holds the subtype and dig_major_rev
 *				of the chip.
 */
struct qpnp_misc_dev {
	struct list_head		list;
	struct mutex			mutex;
	struct device			*dev;
	struct resource			*resource;
	struct spmi_device		*spmi;
	struct qpnp_misc_version	version;

	u8				pwm_sel;
	bool				enable_gp_driver;
};

static struct of_device_id qpnp_misc_match_table[] = {
	{ .compatible = QPNP_MISC_DEV_NAME },
	{}
};

enum qpnp_misc_version_name {
	INVALID,
	PM8941,
	PM8226,
	PMA8084,
	PMDCALIFORNIUM,
};

static struct qpnp_misc_version irq_support_version[] = {
	{0x00, 0x00}, /* INVALID */
	{0x01, 0x02}, /* PM8941 */
	{0x07, 0x00}, /* PM8226 */
	{0x09, 0x00}, /* PMA8084 */
	{0x16, 0x00}, /* PMDCALIFORNIUM */
};

static int qpnp_write_byte(struct spmi_device *spmi, u16 addr, u8 val)
{
	int rc;

	rc = spmi_ext_register_writel(spmi->ctrl, spmi->sid, addr, &val, 1);
	if (rc)
		pr_err("SPMI write failed rc=%d\n", rc);

	return rc;
}

static int qpnp_read_byte(struct spmi_device *spmi, u16 addr, u8 *val)
{
	int rc;

	rc = spmi_ext_register_readl(spmi->ctrl, spmi->sid, addr, val, 1);
	if (rc) {
		pr_err("SPMI read failed rc=%d\n", rc);
		return rc;
	}
	return rc;
}

static int get_qpnp_misc_version_name(struct qpnp_misc_dev *dev)
{
	int i;

	for (i = 1; i < ARRAY_SIZE(irq_support_version); i++)
		if (dev->version.subtype == irq_support_version[i].subtype &&
		    dev->version.dig_major_rev >=
					irq_support_version[i].dig_major_rev)
			return i;

	return INVALID;
}

static bool __misc_irqs_available(struct qpnp_misc_dev *dev)
{
	int version_name = get_qpnp_misc_version_name(dev);

	if (version_name == INVALID)
		return 0;
	return 1;
}

int qpnp_misc_irqs_available(struct device *consumer_dev)
{
	struct device_node *misc_node = NULL;
	struct qpnp_misc_dev *mdev = NULL;
	struct qpnp_misc_dev *mdev_found = NULL;

	if (IS_ERR_OR_NULL(consumer_dev)) {
		pr_err("Invalid consumer device pointer\n");
		return -EINVAL;
	}

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

static int qpnp_misc_dt_init(struct qpnp_misc_dev *mdev)
{
	u32 val;

	if (!of_property_read_u32(mdev->dev->of_node, "qcom,pwm-sel", &val)) {
		if (val > PWM_SEL_MAX) {
			dev_err(mdev->dev, "Invalid value for pwm-sel\n");
			return -EINVAL;
		}
		mdev->pwm_sel = (u8)val;
	}
	mdev->enable_gp_driver = of_property_read_bool(mdev->dev->of_node,
						"qcom,enable-gp-driver");

	WARN((mdev->pwm_sel > 0 && !mdev->enable_gp_driver),
			"Setting PWM source without enabling gp driver\n");
	WARN((mdev->pwm_sel == 0 && mdev->enable_gp_driver),
			"Enabling gp driver without setting PWM source\n");

	return 0;
}

static int qpnp_misc_config(struct qpnp_misc_dev *mdev)
{
	int rc, version_name;

	version_name = get_qpnp_misc_version_name(mdev);

	switch (version_name) {
	case PMDCALIFORNIUM:
		if (mdev->pwm_sel > 0 && mdev->enable_gp_driver) {
			rc = qpnp_write_byte(mdev->spmi,
				mdev->resource->start + REG_PWM_SEL,
				mdev->pwm_sel);
			if (rc < 0) {
				dev_err(mdev->dev,
					"Failed to write PWM_SEL reg\n");
				return rc;
			}

			rc = qpnp_write_byte(mdev->spmi,
				mdev->resource->start + REG_GP_DRIVER_EN,
				GP_DRIVER_EN_BIT);
			if (rc < 0) {
				dev_err(mdev->dev,
					"Failed to write GP_DRIVER_EN reg\n");
				return rc;
			}
		}
		break;
	default:
		break;
	}

	return 0;
}

static int qpnp_misc_probe(struct spmi_device *spmi)
{
	struct resource *resource;
	struct qpnp_misc_dev *mdev = ERR_PTR(-EINVAL);
	int rc;

	resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!resource) {
		pr_err("Unable to get spmi resource for MISC\n");
		return -EINVAL;
	}

	mdev = kzalloc(sizeof(*mdev), GFP_KERNEL);
	if (!mdev)
		return -ENOMEM;

	mdev->spmi = spmi;
	mdev->dev = &(spmi->dev);
	mdev->resource = resource;

	rc = qpnp_read_byte(spmi, resource->start + REG_SUBTYPE,
				&mdev->version.subtype);
	if (rc < 0) {
		dev_err(mdev->dev, "Failed to read subtype, rc=%d\n", rc);
		return rc;
	}

	rc = qpnp_read_byte(spmi, resource->start + REG_DIG_MAJOR_REV,
				&mdev->version.dig_major_rev);
	if (rc < 0) {
		dev_err(mdev->dev, "Failed to read dig_major_rev, rc=%d\n", rc);
		return rc;
	}

	mutex_lock(&qpnp_misc_dev_list_mutex);
	list_add_tail(&mdev->list, &qpnp_misc_dev_list);
	mutex_unlock(&qpnp_misc_dev_list_mutex);

	rc = qpnp_misc_dt_init(mdev);
	if (rc < 0) {
		dev_err(mdev->dev,
			"Error reading device tree properties, rc=%d\n", rc);
		return rc;
	}

	rc = qpnp_misc_config(mdev);
	if (rc < 0) {
		dev_err(mdev->dev,
			"Error configuring module registers, rc=%d\n", rc);
		return rc;
	}

	dev_info(mdev->dev, "probe successful\n");
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
