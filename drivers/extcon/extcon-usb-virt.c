// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for Spreadtrum virt USB extcon
 *
 * Copyright (C) 2020 Spreadtrum Communications Inc.
 */

#include <linux/extcon-provider.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/pinctrl/consumer.h>


struct usb_virt_extcon_info {
	struct device *dev;
	struct extcon_dev *edev;
	u32 id_virt;
	u32 vbus_virt;

	struct work_struct wq_detcable;
};

static const unsigned int usb_virt_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

/*
 * "USB" = VBUS and "USB-HOST" = !ID, so we have:
 * Both "USB" and "USB-HOST" can't be set as active at the
 * same time so if "USB-HOST" is active (i.e. ID is 0)  we keep "USB" inactive
 * even if VBUS is on.
 *
 *  State              |    ID   |   VBUS
 * ----------------------------------------
 *  [1] USB            |    H    |    H
 *  [2] none           |    H    |    L
 *  [3] USB-HOST       |    L    |    H
 *  [4] USB-HOST       |    L    |    L
 *
 * In case we have only one of these signals:
 * - VBUS only - we want to distinguish between [1] and [2], so ID is always 1.
 * - ID only - we want to distinguish between [1] and [4], so VBUS = ID.
 */
static void usb_virt_extcon_detect_cable(struct work_struct *work)
{
	struct usb_virt_extcon_info *info = container_of(work,
						    struct usb_virt_extcon_info,
						    wq_detcable);

	dev_info(info->dev, "id-virt=%d, vbus-virt=%d\n",
						    info->id_virt, info->vbus_virt);

	/* at first we clean states which are no longer active */
	if (info->id_virt)
		extcon_set_state_sync(info->edev, EXTCON_USB_HOST, false);
	if (!info->vbus_virt)
		extcon_set_state_sync(info->edev, EXTCON_USB, false);

	if (!info->id_virt) {
		extcon_set_state_sync(info->edev, EXTCON_USB_HOST, true);
	} else {
		if (info->vbus_virt)
			extcon_set_state_sync(info->edev, EXTCON_USB, true);
	}
}

static ssize_t id_virt_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct usb_virt_extcon_info *info = dev_get_drvdata(dev);

	if (!info)
		return -EINVAL;

	return sprintf(buf, "%d\n", info->id_virt);
}

static ssize_t id_virt_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct usb_virt_extcon_info *info = dev_get_drvdata(dev);
	u32 id_virt;

	if (!info)
		return -EINVAL;

	if (kstrtouint(buf, 0, &id_virt))
		return -EINVAL;

	if (id_virt > 1)
		return -EINVAL;

	info->id_virt = id_virt;

	dev_info(dev, "set id-virt->%d\n", id_virt);

	schedule_work(&info->wq_detcable);
	flush_work(&info->wq_detcable);

	return size;
}
static DEVICE_ATTR_RW(id_virt);

static ssize_t vbus_virt_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct usb_virt_extcon_info *info = dev_get_drvdata(dev);

	if (!info)
		return -EINVAL;

	return sprintf(buf, "%d\n", info->vbus_virt);
}

static ssize_t vbus_virt_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct usb_virt_extcon_info *info = dev_get_drvdata(dev);
	u32 vbus_virt;

	if (!info)
		return -EINVAL;

	if (kstrtouint(buf, 0, &vbus_virt))
		return -EINVAL;

	if (vbus_virt > 1)
		return -EINVAL;

	info->vbus_virt = vbus_virt;

	dev_info(dev, "set vbus-virt->%d\n", vbus_virt);

	schedule_work(&info->wq_detcable);
	flush_work(&info->wq_detcable);

	return size;
}
static DEVICE_ATTR_RW(vbus_virt);

static struct attribute *usb_virt_extcon_attrs[] = {
	&dev_attr_id_virt.attr,
	&dev_attr_vbus_virt.attr,
	NULL
};
ATTRIBUTE_GROUPS(usb_virt_extcon);


static int usb_virt_extcon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct usb_virt_extcon_info *info;
	int ret;
	u32 val;

	if (!np)
		return -EINVAL;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;

	info->edev = devm_extcon_dev_allocate(dev, usb_virt_extcon_cable);
	if (IS_ERR(info->edev)) {
		dev_err(dev, "failed to allocate extcon device\n");
		return -ENOMEM;
	}

	ret = devm_extcon_dev_register(dev, info->edev);
	if (ret < 0) {
		dev_err(dev, "failed to register extcon device\n");
		return ret;
	}

	if (!of_property_read_u32(np, "id-virt", &val))
		info->id_virt = val;
	else
		info->id_virt = 1;

	if (!of_property_read_u32(np, "vbus-virt", &val))
		info->vbus_virt = val;
	else
		info->vbus_virt = info->id_virt;

	dev_info(dev, "default id-virt=%d, vbus-virt=%d\n", info->id_virt, info->vbus_virt);

	INIT_WORK(&info->wq_detcable, usb_virt_extcon_detect_cable);

	platform_set_drvdata(pdev, info);

	ret = sysfs_create_groups(&dev->kobj, usb_virt_extcon_groups);
	if (ret) {
		dev_err(dev, "failed to create usb_virt_extcon attributes\n");
		return ret;
	}

	/* Perform initial detection */
	usb_virt_extcon_detect_cable(&info->wq_detcable);

	return 0;
}

static int usb_virt_extcon_remove(struct platform_device *pdev)
{
	struct usb_virt_extcon_info *info = platform_get_drvdata(pdev);

	sysfs_remove_groups(&info->dev->kobj, usb_virt_extcon_groups);

	cancel_work_sync(&info->wq_detcable);

	return 0;
}

static const struct of_device_id usb_virt_extcon_dt_match[] = {
	{ .compatible = "sprd,extcon-usb-virt", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, usb_virt_extcon_dt_match);

static const struct platform_device_id usb_virt_extcon_platform_ids[] = {
	{ .name = "extcon-usb-virt", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(platform, usb_virt_extcon_platform_ids);

static struct platform_driver usb_virt_extcon_driver = {
	.probe		= usb_virt_extcon_probe,
	.remove		= usb_virt_extcon_remove,
	.driver		= {
		.name	= "extcon-usb-virt",
		.of_match_table = usb_virt_extcon_dt_match,
	},
	.id_table = usb_virt_extcon_platform_ids,
};

module_platform_driver(usb_virt_extcon_driver);

MODULE_DESCRIPTION("USB Virt extcon driver");
MODULE_LICENSE("GPL v2");
