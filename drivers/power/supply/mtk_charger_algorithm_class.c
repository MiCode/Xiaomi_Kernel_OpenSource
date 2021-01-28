// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#include <linux/module.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/slab.h>
#include "mtk_charger_algorithm_class.h"


static struct class *charger_algorithm_class;

static void chg_alg_device_release(struct device *dev)
{
	struct chg_alg_device *chg_dev = to_chg_alg_dev(dev);

	kfree(chg_dev);
}

int chg_alg_init_algo(struct chg_alg_device *alg_dev)
{
	if (alg_dev != NULL && alg_dev->ops != NULL &&
	    alg_dev->ops->init_algo)
		return alg_dev->ops->init_algo(alg_dev);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(chg_alg_init_algo);

int chg_alg_is_algo_ready(struct chg_alg_device *alg_dev)
{
	if (alg_dev != NULL && alg_dev->ops != NULL &&
	    alg_dev->ops->is_algo_ready)
		return alg_dev->ops->is_algo_ready(alg_dev);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(chg_alg_is_algo_ready);

int chg_alg_start_algo(struct chg_alg_device *alg_dev)
{
	if (alg_dev != NULL && alg_dev->ops != NULL &&
	    alg_dev->ops->start_algo)
		return alg_dev->ops->start_algo(alg_dev);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(chg_alg_start_algo);

int chg_alg_get_prop(struct chg_alg_device *alg_dev,
	enum chg_alg_props s, int *value)
{
	if (alg_dev != NULL && alg_dev->ops != NULL &&
	    alg_dev->ops->get_prop)
		return alg_dev->ops->get_prop(alg_dev, s, value);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(chg_alg_get_prop);

int chg_alg_set_prop(struct chg_alg_device *alg_dev,
	enum chg_alg_props s, int value)
{
	if (alg_dev != NULL && alg_dev->ops != NULL &&
	    alg_dev->ops->set_prop)
		return alg_dev->ops->set_prop(alg_dev, s, value);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(chg_alg_set_prop);

int chg_alg_stop_algo(struct chg_alg_device *alg_dev)
{
	if (alg_dev != NULL && alg_dev->ops != NULL &&
	    alg_dev->ops->stop_algo)
		return alg_dev->ops->stop_algo(alg_dev);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(chg_alg_stop_algo);

int chg_alg_notifier_call(struct chg_alg_device *alg_dev,
	struct chg_alg_notify *notify)
{
	if (alg_dev != NULL && alg_dev->ops != NULL &&
	    alg_dev->ops->notifier_call)
		return alg_dev->ops->notifier_call(alg_dev, notify);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(chg_alg_notifier_call);

int chg_alg_set_current_limit(struct chg_alg_device *alg_dev,
	struct chg_limit_setting *setting)
{
	pr_notice("%s\n", __func__);
	if (alg_dev != NULL && alg_dev->ops != NULL &&
	    alg_dev->ops->set_current_limit)
		return alg_dev->ops->set_current_limit(alg_dev, setting);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(chg_alg_set_current_limit);

char *chg_alg_state_to_str(int state)
{
	switch (state) {
	case ALG_INIT_FAIL:
		return "ALG_INIT_FAIL";
	case ALG_TA_CHECKING:
		return "ALG_TA_CHECKING";
	case ALG_TA_NOT_SUPPORT:
		return "ALG_TA_NOT_SUPPORT";
	case ALG_NOT_READY:
		return "ALG_NOT_READY";
	case ALG_READY:
		return "ALG_READY";
	case ALG_RUNNING:
		return "ALG_RUNNING";
	case ALG_DONE:
		return "ALG_DONE";
	default:
		break;
	}
	pr_notice("%s unknown state:%d\n", __func__
		, state);
	return "chg_alg_state_UNKNOWN";
}
EXPORT_SYMBOL(chg_alg_state_to_str);

int register_chg_alg_notifier(struct chg_alg_device *alg_dev,
				struct notifier_block *nb)
{
	int ret;

	ret = srcu_notifier_chain_register(&alg_dev->evt_nh, nb);
	return ret;
}
EXPORT_SYMBOL(register_chg_alg_notifier);

int unregister_chg_alg_notifier(struct chg_alg_device *alg_dev,
				struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&alg_dev->evt_nh, nb);
}
EXPORT_SYMBOL(unregister_chg_alg_notifier);

/**
 * chg_alg_device_register - create and register a new object of
 *   charger_device class.
 * @name: the name of the new object
 * @parent: a pointer to the parent device
 * @devdata: an optional pointer to be stored for private driver use.
 * The methods may retrieve it by using charger_get_data(charger_dev).
 * @ops: the charger operations structure.
 *
 * Creates and registers new charger device. Returns either an
 * ERR_PTR() or a pointer to the newly allocated device.
 */
struct chg_alg_device *chg_alg_device_register(const char *name,
		struct device *parent, void *devdata,
		const struct chg_alg_ops *ops,
		const struct chg_alg_properties *props)
{
	struct chg_alg_device *chg_dev;
	static struct lock_class_key key;
	struct srcu_notifier_head *head;
	int rc;
	char *algo_name = NULL;

	pr_debug("%s: name=%s\n", __func__, name);
	chg_dev = kzalloc(sizeof(*chg_dev), GFP_KERNEL);
	if (!chg_dev)
		return ERR_PTR(-ENOMEM);

	head = &chg_dev->evt_nh;
	srcu_init_notifier_head(head);
	/* Rename srcu's lock to avoid LockProve warning */
	lockdep_init_map(&(&head->srcu)->dep_map, name, &key, 0);
	mutex_init(&chg_dev->ops_lock);
	chg_dev->dev.class = charger_algorithm_class;
	chg_dev->dev.parent = parent;
	chg_dev->dev.release = chg_alg_device_release;
	algo_name = kasprintf(GFP_KERNEL, "%s", name);
	dev_set_name(&chg_dev->dev, algo_name);
	dev_set_drvdata(&chg_dev->dev, devdata);
	kfree(algo_name);

	/* Copy properties */
	if (props) {
		memcpy(&chg_dev->props, props,
		       sizeof(struct chg_alg_properties));
	}
	rc = device_register(&chg_dev->dev);
	if (rc) {
		kfree(chg_dev);
		return ERR_PTR(rc);
	}
	chg_dev->ops = ops;
	return chg_dev;
}
EXPORT_SYMBOL(chg_alg_device_register);

/**
 * chg_alg_device_unregister - unregisters a switching charger device
 * object.
 * @charger_dev: the switching charger device object to be unregistered
 * and freed.
 *
 * Unregisters a previously registered via charger_device_register object.
 */
void chg_alg_device_unregister(struct chg_alg_device *chg_dev)
{
	if (!chg_dev)
		return;

	mutex_lock(&chg_dev->ops_lock);
	chg_dev->ops = NULL;
	mutex_unlock(&chg_dev->ops_lock);
	device_unregister(&chg_dev->dev);
}
EXPORT_SYMBOL(chg_alg_device_unregister);

static int chg_alg_match_device_by_name(struct device *dev,
	const void *data)
{
	const char *name = data;

	return strcmp(dev_name(dev), name) == 0;
}

struct chg_alg_device *get_chg_alg_by_name(const char *name)
{
	struct device *dev;

	if (!name)
		return (struct chg_alg_device *)NULL;
	dev = class_find_device(charger_algorithm_class, NULL, name,
				chg_alg_match_device_by_name);

	return dev ? to_chg_alg_dev(dev) : NULL;

}
EXPORT_SYMBOL(get_chg_alg_by_name);

static void __exit charger_algorithm_class_exit(void)
{
	class_destroy(charger_algorithm_class);
}

static int __init charger_algorithm_class_init(void)
{
	charger_algorithm_class =
		class_create(THIS_MODULE, "Charger Algorithm");
	if (IS_ERR(charger_algorithm_class)) {
		pr_notice("Unable to create charger algorithm class; errno = %ld\n",
			PTR_ERR(charger_algorithm_class));
		return PTR_ERR(charger_algorithm_class);
	}
	//charger_algorithm_class->dev_groups = adapter_groups;
	//charger_algorithm_class->suspend = charger_algorithm_suspend;
	//charger_algorithm_class->resume = charger_algorithm_resume;
	return 0;
}

subsys_initcall(charger_algorithm_class_init);
module_exit(charger_algorithm_class_exit);

MODULE_DESCRIPTION("Charger Algorithm Class Device");
MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_VERSION("1.0.0");
MODULE_LICENSE("GPL");
