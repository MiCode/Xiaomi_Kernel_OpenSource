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

#include "hq_adapter_class.h"

static struct class *adapter_class;
static struct device_type adapter_dev_type;

#define ADAPTER_ATTR(_name)                     \
{                                               \
	.attr = {                                   \
		.name = #_name ,                        \
		.mode = 0660,                           \
	},                                          \
	.show = adapter_show_property,              \
	.store = adapter_store_property,            \
}

static struct device_attribute adapter_attrs[];

int adapter_get_softreset(struct adapter_dev *adapter)
{
	if (!adapter || !adapter->ops)
		return -EINVAL;
	if (adapter->ops->handshake == NULL)
		return -EOPNOTSUPP;
	return adapter->ops->get_softreset(adapter);
}
EXPORT_SYMBOL(adapter_get_softreset);

int adapter_set_softreset(struct adapter_dev *adapter, bool val)
{
	if (!adapter || !adapter->ops)
		return -EINVAL;
	if (adapter->ops->handshake == NULL)
		return -EOPNOTSUPP;
	return adapter->ops->set_softreset(adapter, val);
}
EXPORT_SYMBOL(adapter_set_softreset);

int adapter_handshake(struct adapter_dev *adapter)
{
	if (!adapter || !adapter->ops)
		return -EINVAL;
	if (adapter->ops->handshake == NULL)
		return -EOPNOTSUPP;
	return adapter->ops->handshake(adapter);
}
EXPORT_SYMBOL(adapter_handshake);

int adapter_get_cap(struct adapter_dev *adapter, struct adapter_cap *cap)
{
	if (!adapter || !adapter->ops)
		return -EINVAL;
	if (adapter->ops->get_cap == NULL)
		return -EOPNOTSUPP;
	return adapter->ops->get_cap(adapter, cap);
}
EXPORT_SYMBOL(adapter_get_cap);

int adapter_set_cap(struct adapter_dev *adapter, uint8_t nr,
											uint32_t mv, uint32_t ma)
{
	if (!adapter || !adapter->ops)
		return -EINVAL;
	if (adapter->ops->set_cap == NULL)
		return -EOPNOTSUPP;
	return adapter->ops->set_cap(adapter, nr, mv, ma);
}
EXPORT_SYMBOL(adapter_set_cap);

int adapter_get_temp(struct adapter_dev *adapter, uint8_t *temp)
{
	if (!adapter || !adapter->ops)
		return -EINVAL;
	if (adapter->ops->get_temp == NULL)
		return -EOPNOTSUPP;
	return adapter->ops->get_temp(adapter, temp);
}
EXPORT_SYMBOL(adapter_get_temp);

int adapter_set_wdt(struct adapter_dev *adapter, uint32_t ms)
{
	if (!adapter || !adapter->ops)
		return -EINVAL;
	if (adapter->ops->set_wdt == NULL)
		return -EOPNOTSUPP;
	return adapter->ops->set_wdt(adapter, ms);
}
EXPORT_SYMBOL(adapter_set_wdt);

int adapter_reset(struct adapter_dev *adapter)
{
	if (!adapter || !adapter->ops)
		return -EINVAL;
	if (adapter->ops->reset == NULL)
		return -EOPNOTSUPP;
	return adapter->ops->reset(adapter);
}
EXPORT_SYMBOL(adapter_reset);

static int adapter_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	struct adapter_dev *adapter = dev_get_drvdata(dev);

	return strcmp(adapter->name, name) == 0;
}

struct adapter_dev *adapter_find_dev_by_name(const char *name)
{
	struct adapter_dev *adapter = NULL;
	struct device *dev = class_find_device(adapter_class, NULL, name,
					adapter_match_device_by_name);

	if (dev) {
		adapter = dev_get_drvdata(dev);
	}

	return adapter;
}
EXPORT_SYMBOL(adapter_find_dev_by_name);

struct adapter_dev * adapter_register(char *name, struct device *parent,
							struct adapter_ops *ops, void *private)
{
	struct adapter_dev *adapter;
	struct device *dev;
	int ret;

	if (!parent)
		pr_warn("%s: Expected proper parent device\n", __func__);

	if (!ops || !name)
		return ERR_PTR(-EINVAL);

	adapter = kzalloc(sizeof(*adapter), GFP_KERNEL);
	if (!adapter)
		return ERR_PTR(-ENOMEM);

	dev = &(adapter->dev);

	device_initialize(dev);

	dev->class = adapter_class;
	dev->type = &adapter_dev_type;
	dev->parent = parent;
	dev_set_drvdata(dev, adapter);

	adapter->private = private;

	ret = dev_set_name(dev, "%s", name);
	if (ret)
		goto dev_set_name_failed;

	ret = device_add(dev);
	if (ret)
		goto device_add_failed;

	adapter->name = name;
	adapter->ops = ops;

	return adapter;

device_add_failed:
dev_set_name_failed:
	put_device(dev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(adapter_register);

void * adapter_get_private(struct adapter_dev *adapter)
{
	if (!adapter)
		return ERR_PTR(-EINVAL);
	return adapter->private;
}
EXPORT_SYMBOL(adapter_get_private);

int adapter_unregister(struct adapter_dev *adapter)
{
	device_unregister(&adapter->dev);
	kfree(adapter);
	return 0;
}
EXPORT_SYMBOL(adapter_unregister);

/**
 * sysfs
 */
static ssize_t adapter_show_property(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	ssize_t ret = 0;
	int i = 0;
	struct adapter_dev *adapter = dev_get_drvdata(dev);
	struct adapter_cap cap;
	enum ADAPTER_ATTR_NUM num = attr - adapter_attrs;
	switch (num) {
	case ADAPTER_ATTR_GET_CAP:
		ret = adapter_get_cap(adapter, &cap);
		if (ret < 0)
			return ret;
		ret = 0;
		for (i = 0; i < cap.cnt; i++) {
			ret += sprintf(buf + ret, "%d { volt %d ~ %d , curr %d ~ %d }\n",
							i,
							cap.volt_min[i], cap.volt_max[i],
							cap.curr_min[i], cap.curr_max[i]);
		}
		break;
	default:
		ret = sprintf(buf, "no support read\n");
	}

	return ret;
}

static ssize_t adapter_store_property(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	size_t ret = 0;
	int arg_1 = 0, arg_2 = 0, arg_3 = 0;
	struct adapter_dev *adapter = dev_get_drvdata(dev);
	enum ADAPTER_ATTR_NUM num = attr - adapter_attrs;
	switch (num) {
	case ADAPTER_ATTR_HANDSHAKE:
		ret = adapter_handshake(adapter);
		break;
	case ADAPTER_ATTR_SET_CAP:
		ret = sscanf(buf, "%d %d %d", &arg_1, &arg_2, &arg_3);
		ret = adapter_set_cap(adapter, arg_1, arg_2, arg_3);
		break;
	case ADAPTER_ATTR_SET_WDT:
		ret = sscanf(buf, "%d", &arg_1);
		ret = adapter_set_wdt(adapter, arg_1);
		break;
	case ADAPTER_ATTR_RESET:
		ret = adapter_reset(adapter);
		break;
	default:
		return -EINVAL;
	}

	if (ret < 0)
		return ret;

	return count;
}

static struct device_attribute adapter_attrs[] = {
	ADAPTER_ATTR(handshake),
	ADAPTER_ATTR(get_cap),
	ADAPTER_ATTR(set_cap),
	ADAPTER_ATTR(set_wdt),
	ADAPTER_ATTR(reset),
};

static struct attribute *_adapter_attrs[ARRAY_SIZE(adapter_attrs) + 1];

static struct attribute_group adapter_attr_group = {
	.attrs = _adapter_attrs,
};

static const struct attribute_group *adapter_attr_groups[] = {
	&adapter_attr_group,
	NULL,
};

static int __init adapter_class_init(void)
{
	int i = 0;
	pr_info("%s\n", __func__);
	adapter_class = class_create(THIS_MODULE, "adapter_class");
	if (IS_ERR(adapter_class)) {
		return PTR_ERR(adapter_class);
	}

	adapter_class->dev_uevent = NULL;
	adapter_dev_type.groups = adapter_attr_groups;
	for (i = 0; i < ARRAY_SIZE(adapter_attrs); i++) {
		_adapter_attrs[i] = &(adapter_attrs[i].attr);
	}

	return 0;
}

static void __exit adapter_class_exit(void)
{
	class_destroy(adapter_class);
}

subsys_initcall(adapter_class_init);
module_exit(adapter_class_exit);

MODULE_DESCRIPTION("Huaqin Adapter Class Core");
MODULE_LICENSE("GPL v2");
