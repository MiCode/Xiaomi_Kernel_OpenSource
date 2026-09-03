// SPDX-License-Identifier: GPL-2.0

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

#include "lc_charger_class.h"

static struct class *charger_class;

int charger_get_chg_present(struct charger_dev *charger, int *state)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->is_present == NULL)
		return -EOPNOTSUPP;
	return charger->ops->is_present(charger, state);
}
EXPORT_SYMBOL(charger_get_chg_present);

int charger_get_vindpm_state(struct charger_dev *charger, bool *state)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_vindpm_state == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_vindpm_state(charger, state);
}
EXPORT_SYMBOL(charger_get_vindpm_state);

int charger_first_bc1p2(struct charger_dev *charger, int *bc1p2_result)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->first_bc1p2 == NULL)
		return -EOPNOTSUPP;
	return charger->ops->first_bc1p2(charger, bc1p2_result);
}
EXPORT_SYMBOL(charger_first_bc1p2);

int charger_retry_bc1p2(struct charger_dev *charger, int *bc1p2_result)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->retry_bc1p2 == NULL)
		return -EOPNOTSUPP;
	return charger->ops->retry_bc1p2(charger, bc1p2_result);
}
EXPORT_SYMBOL(charger_retry_bc1p2);

int charger_set_shipmode(struct charger_dev *charger, bool en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_shipmode == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_shipmode(charger, en);
}
EXPORT_SYMBOL(charger_set_shipmode);

int charger_get_vbus_type(struct charger_dev *charger, int *type)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_shipmode == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_vbus_type(charger, type);
}
EXPORT_SYMBOL(charger_get_vbus_type);

int charger_set_vindpm(struct charger_dev *charger, int vindpm)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_vindpm == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_vindpm(charger, vindpm);
}
EXPORT_SYMBOL(charger_set_vindpm);

int charger_get_iindpm(struct charger_dev *charger, int *iindpm)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_iindpm == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_iindpm(charger, iindpm);
}
EXPORT_SYMBOL(charger_get_iindpm);

int charger_set_iterm(struct charger_dev *charger, int iterm)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_iterm == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_iterm(charger, iterm);
}
EXPORT_SYMBOL(charger_set_iterm);

int charger_set_pfm(struct charger_dev *charger, bool en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_pfm == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_pfm(charger, en);
}
EXPORT_SYMBOL(charger_set_pfm);

static int charger_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	struct charger_dev *charger = dev_get_drvdata(dev);

	return strcmp(charger->name, name) == 0;
}

struct charger_dev *charger_find_dev_by_name(const char *name)
{
	struct charger_dev *charger = NULL;
	struct device *dev = class_find_device(charger_class, NULL, name,
					charger_match_device_by_name);

	if (dev) {
		charger = dev_get_drvdata(dev);
	}

	return charger;
}
EXPORT_SYMBOL(charger_find_dev_by_name);

struct charger_dev *charger_register(char *name, struct device *parent,
				struct charger_ops *ops, void *private)
{
	struct charger_dev *charger;
	struct device *dev;
	int ret;

	if (!parent)
		pr_err("Expected proper parent device\n");

	if (!ops || !name)
		return ERR_PTR(-EINVAL);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return ERR_PTR(-ENOMEM);

	dev = &(charger->dev);
	charger->name = name;
	charger->ops = ops;

	device_initialize(dev);

	dev->class = charger_class;
	dev->parent = parent;
	dev_set_drvdata(dev, charger);

	charger->private = private;

	ret = dev_set_name(dev, "%s", name);
	if (ret)
		goto dev_set_name_failed;

	ret = device_add(dev);
	if (ret)
		goto device_add_failed;

	return charger;

device_add_failed:
dev_set_name_failed:
	put_device(dev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(charger_register);

void *charger_get_private(struct charger_dev *charger)
{
	if (!charger)
		return ERR_PTR(-EINVAL);
	return charger->private;
}
EXPORT_SYMBOL(charger_get_private);

int charger_unregister(struct charger_dev *charger)
{
	device_unregister(&charger->dev);
	kfree(charger);
	return 0;
}
EXPORT_SYMBOL(charger_unregister);


static int __init charger_class_init(void)
{
	charger_class = class_create(THIS_MODULE, "charger_class");
	if (IS_ERR(charger_class)) {
		return PTR_ERR(charger_class);
	}

	charger_class->dev_uevent = NULL;

	pr_info("charger class initialize success\n");

	return 0;
}

static void __exit charger_class_exit(void)
{
	class_destroy(charger_class);
}

subsys_initcall(charger_class_init);
module_exit(charger_class_exit);

MODULE_DESCRIPTION("Lc Charger Class Core");
MODULE_LICENSE("GPL v2");
