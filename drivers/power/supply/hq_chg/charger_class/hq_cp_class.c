// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 Huaqin Technology(Shanghai) Co., Ltd.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/err.h>
#include <linux/of.h>

#include "hq_cp_class.h"

static struct class *chargerpump_class;

int chargerpump_set_chip_init(struct chargerpump_dev *chargerpump)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->set_chip_init == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->set_chip_init(chargerpump);
}

int chargerpump_set_enable(struct chargerpump_dev *chargerpump, bool enable)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->set_enable == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->set_enable(chargerpump, enable);
}
EXPORT_SYMBOL(chargerpump_set_enable);

int chargerpump_set_vbus_ovp(struct chargerpump_dev *chargerpump, int mv)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->set_vbus_ovp == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->set_vbus_ovp(chargerpump, mv);
}

int chargerpump_set_ibus_ocp(struct chargerpump_dev *chargerpump, int ma)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->set_ibus_ocp == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->set_ibus_ocp(chargerpump, ma);
}
EXPORT_SYMBOL(chargerpump_set_ibus_ocp);

int chargerpump_set_vbat_ovp(struct chargerpump_dev *chargerpump, int mv)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->set_vbat_ovp == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->set_vbat_ovp(chargerpump, mv);
}
EXPORT_SYMBOL(chargerpump_set_vbat_ovp);

int chargerpump_set_ibat_ocp(struct chargerpump_dev *chargerpump, int ma)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->set_ibat_ocp == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->set_ibat_ocp(chargerpump, ma);
}
EXPORT_SYMBOL(chargerpump_set_ibat_ocp);

int chargerpump_set_enable_adc(struct chargerpump_dev *chargerpump, bool enable)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->set_enable_adc == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->set_enable_adc(chargerpump, enable);
}
EXPORT_SYMBOL(chargerpump_set_enable_adc);

int chargerpump_get_is_enable(struct chargerpump_dev *chargerpump, bool *enable)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->get_is_enable == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->get_is_enable(chargerpump, enable);
}
EXPORT_SYMBOL(chargerpump_get_is_enable);

int chargerpump_get_status(struct chargerpump_dev *chargerpump, uint32_t *status)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->get_status == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->get_status(chargerpump, status);
}
EXPORT_SYMBOL(chargerpump_get_status);

int chargerpump_get_adc_value(struct chargerpump_dev *chargerpump, enum sc_adc_channel ch, int *value)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->get_adc_value == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->get_adc_value(chargerpump, ch, value);
}
EXPORT_SYMBOL(chargerpump_get_adc_value);

int chargerpump_get_chip_id(struct chargerpump_dev *chargerpump, int *value)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->get_chip_id == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->get_chip_id(chargerpump, value);
}
EXPORT_SYMBOL(chargerpump_get_chip_id);

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
int chargerpump_set_cp_workmode(struct chargerpump_dev *chargerpump, int workmode)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->set_cp_workmode == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->set_cp_workmode(chargerpump, workmode);
}
EXPORT_SYMBOL_GPL(chargerpump_set_cp_workmode);
int chargerpump_get_cp_workmode(struct chargerpump_dev *chargerpump, int *workmode)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->get_cp_workmode == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->get_cp_workmode(chargerpump, workmode);
}
EXPORT_SYMBOL_GPL(chargerpump_get_cp_workmode);
#endif

static int chargerpump_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	struct chargerpump_dev *chargerpump = dev_get_drvdata(dev);

	return strcmp(chargerpump->name, name) == 0;
}

struct chargerpump_dev *chargerpump_find_dev_by_name(const char *name)
{
	struct chargerpump_dev *chargerpump = NULL;
	struct device *dev = class_find_device(chargerpump_class, NULL, name,
					chargerpump_match_device_by_name);

	if (dev) {
		chargerpump = dev_get_drvdata(dev);
	}

	return chargerpump;
}
EXPORT_SYMBOL(chargerpump_find_dev_by_name);

struct chargerpump_dev *chargerpump_register(char *name, struct device *parent,
							struct chargerpump_ops *ops, void *private)
{
	struct chargerpump_dev *chargerpump;
	struct device *dev;
	int ret;

	if (!parent)
		pr_warn("%s: Expected proper parent device\n", __func__);

	if (!ops || !name)
		return ERR_PTR(-EINVAL);

	chargerpump = kzalloc(sizeof(*chargerpump), GFP_KERNEL);
	if (!chargerpump)
		return ERR_PTR(-ENOMEM);

	dev = &(chargerpump->dev);

	device_initialize(dev);

	dev->class = chargerpump_class;
	dev->parent = parent;
	dev_set_drvdata(dev, chargerpump);

	chargerpump->private = private;

	ret = dev_set_name(dev, "%s", name);
	if (ret)
		goto dev_set_name_failed;

	ret = device_add(dev);
	if (ret)
		goto device_add_failed;

	chargerpump->name = name;
	chargerpump->ops = ops;

	return chargerpump;

device_add_failed:
dev_set_name_failed:
	put_device(dev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(chargerpump_register);

void * chargerpump_get_private(struct chargerpump_dev *chargerpump)
{
	if (!chargerpump)
		return ERR_PTR(-EINVAL);
	return chargerpump->private;
}
EXPORT_SYMBOL(chargerpump_get_private);

int chargerpump_unregister(struct chargerpump_dev *chargerpump)
{
	device_unregister(&chargerpump->dev);
	kfree(chargerpump);
	return 0;
}

static int __init chargerpump_class_init(void)
{
	chargerpump_class = class_create(THIS_MODULE, "chargerpump_class");
	if (IS_ERR(chargerpump_class)) {
		return PTR_ERR(chargerpump_class);
	}

	chargerpump_class->dev_uevent = NULL;

	return 0;
}

static void __exit chargerpump_class_exit(void)
{
	class_destroy(chargerpump_class);
}

subsys_initcall(chargerpump_class_init);
module_exit(chargerpump_class_exit);

MODULE_DESCRIPTION("Huaqin Charger Pump Class Core");
MODULE_LICENSE("GPL v2");
