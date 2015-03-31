/*
 *  Universal power supply monitor class
 *
 *  Copyright © 2007  Anton Vorontsov <cbou@mail.ru>
 *  Copyright © 2004  Szabolcs Gyurko
 *  Copyright © 2003  Ian Molton <spyro@f2s.com>
 *  Copyright (C) 2015 XiaoMi, Inc.
 *
 *  Modified: 2004, Oct     Szabolcs Gyurko
 *
 *  You may use this code as per GPL version 2
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/notifier.h>
#include "power_supply.h"

/* exported for the APM Power driver, APM emulation */
struct class *power_supply_class;
EXPORT_SYMBOL_GPL(power_supply_class);

static struct device_type power_supply_dev_type;

static BLOCKING_NOTIFIER_HEAD(power_supply_chain);

int register_power_supply_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&power_supply_chain, nb);
}
EXPORT_SYMBOL(register_power_supply_notifier);

int unregister_power_supply_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&power_supply_chain, nb);
}
EXPORT_SYMBOL(unregister_power_supply_notifier);

/**
 * power_supply_set_voltage_limit - set current limit
 * @psy:	the power supply to control
 * @limit:	current limit in uV from the power supply.
 *		0 will disable the power supply.
 *
 * This function will set a maximum supply current from a source
 * and it will disable the charger when limit is 0.
 */
int power_supply_set_voltage_limit(struct power_supply *psy, int limit)
{
	const union power_supply_propval ret = {limit,};

	if (psy->set_property)
		return psy->set_property(psy, POWER_SUPPLY_PROP_VOLTAGE_MAX,
								&ret);

	return -ENXIO;
}
EXPORT_SYMBOL(power_supply_set_voltage_limit);


/**
 * power_supply_set_current_limit - set current limit
 * @psy:	the power supply to control
 * @limit:	current limit in uA from the power supply.
 *		0 will disable the power supply.
 *
 * This function will set a maximum supply current from a source
 * and it will disable the charger when limit is 0.
 */
int power_supply_set_current_limit(struct power_supply *psy, int limit)
{
	const union power_supply_propval ret = {limit,};

	if (psy->set_property)
		return psy->set_property(psy, POWER_SUPPLY_PROP_CURRENT_MAX,
								&ret);

	return -ENXIO;
}
EXPORT_SYMBOL_GPL(power_supply_set_current_limit);

/**
 * power_supply_set_charging_enabled - enable or disable charging
 * @psy:	the power supply to control
 * @enable:	sets enable property of power supply
 */
int power_supply_set_charging_enabled(struct power_supply *psy, bool enable)
{
	const union power_supply_propval ret = {enable,};

	if (psy->set_property)
		return psy->set_property(psy,
				POWER_SUPPLY_PROP_CHARGING_ENABLED,
				&ret);

	return -ENXIO;
}
EXPORT_SYMBOL_GPL(power_supply_set_charging_enabled);

/**
 * power_supply_set_present - set present state of the power supply
 * @psy:	the power supply to control
 * @enable:	sets present property of power supply
 */
int power_supply_set_present(struct power_supply *psy, bool enable)
{
	const union power_supply_propval ret = {enable,};

	if (psy->set_property)
		return psy->set_property(psy, POWER_SUPPLY_PROP_PRESENT,
								&ret);

	return -ENXIO;
}
EXPORT_SYMBOL_GPL(power_supply_set_present);

/**
 * power_supply_set_online - set online state of the power supply
 * @psy:	the power supply to control
 * @enable:	sets online property of power supply
 */
int power_supply_set_online(struct power_supply *psy, bool enable)
{
	const union power_supply_propval ret = {enable,};

	if (psy->set_property)
		return psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE,
								&ret);

	return -ENXIO;
}
EXPORT_SYMBOL_GPL(power_supply_set_online);


/** power_supply_set_health_state - set health state of the power supply
 * @psy:       the power supply to control
 * @health:    sets health property of power supply
 */
int power_supply_set_health_state(struct power_supply *psy, int health)
{
	const union power_supply_propval ret = {health,};

	if (psy->set_property)
		return psy->set_property(psy, POWER_SUPPLY_PROP_HEALTH,
		&ret);
	return -ENXIO;
}
EXPORT_SYMBOL(power_supply_set_health_state);


/**
 * power_supply_set_scope - set scope of the power supply
 * @psy:	the power supply to control
 * @scope:	value to set the scope property to, should be from
 *		the SCOPE enum in power_supply.h
 */
int power_supply_set_scope(struct power_supply *psy, int scope)
{
	const union power_supply_propval ret = {scope, };

	if (psy->set_property)
		return psy->set_property(psy, POWER_SUPPLY_PROP_SCOPE,
								&ret);
	return -ENXIO;
}
EXPORT_SYMBOL_GPL(power_supply_set_scope);

/**
 * power_supply_set_supply_type - set type of the power supply
 * @psy:	the power supply to control
 * @supply_type:	sets type property of power supply
 */
int power_supply_set_supply_type(struct power_supply *psy,
				enum power_supply_type supply_type)
{
	const union power_supply_propval ret = {supply_type,};

	if (psy->set_property)
		return psy->set_property(psy, POWER_SUPPLY_PROP_TYPE,
								&ret);

	return -ENXIO;
}
EXPORT_SYMBOL_GPL(power_supply_set_supply_type);

/**
 * power_supply_set_charge_type - set charge type of the power supply
 * @psy:	the power supply to control
 * @enable:	sets charge type property of power supply
 */
int power_supply_set_charge_type(struct power_supply *psy, int charge_type)
{
	const union power_supply_propval ret = {charge_type,};

	if (psy->set_property)
		return psy->set_property(psy, POWER_SUPPLY_PROP_CHARGE_TYPE,
								&ret);

	return -ENXIO;
}
EXPORT_SYMBOL_GPL(power_supply_set_charge_type);

static int __power_supply_changed_work(struct device *dev, void *data)
{
	struct power_supply *psy = (struct power_supply *)data;
	struct power_supply *pst = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < psy->num_supplicants; i++)
		if (!strcmp(psy->supplied_to[i], pst->name)) {
			if (pst->external_power_changed)
				pst->external_power_changed(pst);
		}
	return 0;
}

static void power_supply_changed_work(struct work_struct *work)
{
	unsigned long flags;
	struct power_supply *psy = container_of(work, struct power_supply,
						changed_work);

	dev_dbg(psy->dev, "%s\n", __func__);

	spin_lock_irqsave(&psy->changed_lock, flags);
	if (psy->changed) {
		psy->changed = false;
		spin_unlock_irqrestore(&psy->changed_lock, flags);

		class_for_each_device(power_supply_class, NULL, psy,
				      __power_supply_changed_work);

		power_supply_update_leds(psy);
		blocking_notifier_call_chain(&power_supply_chain, 0, NULL);

		kobject_uevent(&psy->dev->kobj, KOBJ_CHANGE);
		spin_lock_irqsave(&psy->changed_lock, flags);
	}
	if (!psy->changed)
		wake_unlock(&psy->work_wake_lock);
	spin_unlock_irqrestore(&psy->changed_lock, flags);
}

void power_supply_changed(struct power_supply *psy)
{
	unsigned long flags;

	dev_dbg(psy->dev, "%s\n", __func__);

	spin_lock_irqsave(&psy->changed_lock, flags);
	psy->changed = true;
	wake_lock(&psy->work_wake_lock);
	spin_unlock_irqrestore(&psy->changed_lock, flags);
	schedule_work(&psy->changed_work);
}
EXPORT_SYMBOL_GPL(power_supply_changed);

static int __power_supply_am_i_supplied(struct device *dev, void *data)
{
	union power_supply_propval ret = {0,};
	struct power_supply *psy = (struct power_supply *)data;
	struct power_supply *epsy = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < epsy->num_supplicants; i++) {
		if (!strcmp(epsy->supplied_to[i], psy->name)) {
			if (epsy->get_property(epsy,
				  POWER_SUPPLY_PROP_ONLINE, &ret))
				continue;
			if (ret.intval)
				return ret.intval;
		}
	}
	return 0;
}

int power_supply_am_i_supplied(struct power_supply *psy)
{
	int error;

	error = class_for_each_device(power_supply_class, NULL, psy,
				      __power_supply_am_i_supplied);

	dev_dbg(psy->dev, "%s %d\n", __func__, error);

	return error;
}
EXPORT_SYMBOL_GPL(power_supply_am_i_supplied);

static int __power_supply_is_system_supplied(struct device *dev, void *data)
{
	union power_supply_propval ret = {0,};
	struct power_supply *psy = dev_get_drvdata(dev);
	unsigned int *count = data;

	(*count)++;
	if (psy->type != POWER_SUPPLY_TYPE_BATTERY) {
		if (psy->get_property(psy, POWER_SUPPLY_PROP_ONLINE, &ret))
			return 0;
		if (ret.intval)
			return ret.intval;
	}
	return 0;
}

int power_supply_is_system_supplied(void)
{
	int error;
	unsigned int count = 0;

	error = class_for_each_device(power_supply_class, NULL, &count,
				      __power_supply_is_system_supplied);

	/*
	 * If no power class device was found at all, most probably we are
	 * running on a desktop system, so assume we are on mains power.
	 */
	if (count == 0)
		return 1;

	return error;
}
EXPORT_SYMBOL_GPL(power_supply_is_system_supplied);

int power_supply_set_battery_charged(struct power_supply *psy)
{
	if (psy->type == POWER_SUPPLY_TYPE_BATTERY && psy->set_charged) {
		psy->set_charged(psy);
		return 0;
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(power_supply_set_battery_charged);

static int power_supply_match_device_by_name(struct device *dev, void *data)
{
	const char *name = data;
	struct power_supply *psy = dev_get_drvdata(dev);

	return strcmp(psy->name, name) == 0;
}

struct power_supply *power_supply_get_by_name(char *name)
{
	struct device *dev = class_find_device(power_supply_class, NULL, name,
					power_supply_match_device_by_name);

	return dev ? dev_get_drvdata(dev) : NULL;
}
EXPORT_SYMBOL_GPL(power_supply_get_by_name);

int power_supply_powers(struct power_supply *psy, struct device *dev)
{
	return sysfs_create_link(&psy->dev->kobj, &dev->kobj, "powers");
}
EXPORT_SYMBOL_GPL(power_supply_powers);

static void power_supply_dev_release(struct device *dev)
{
	pr_debug("device: '%s': %s\n", dev_name(dev), __func__);
	kfree(dev);
}

int power_supply_register(struct device *parent, struct power_supply *psy)
{
	struct device *dev;
	int rc;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	device_initialize(dev);

	dev->class = power_supply_class;
	dev->type = &power_supply_dev_type;
	dev->parent = parent;
	dev->release = power_supply_dev_release;
	dev_set_drvdata(dev, psy);
	psy->dev = dev;

	INIT_WORK(&psy->changed_work, power_supply_changed_work);

	rc = kobject_set_name(&dev->kobj, "%s", psy->name);
	if (rc)
		goto kobject_set_name_failed;

	rc = device_add(dev);
	if (rc)
		goto device_add_failed;

	spin_lock_init(&psy->changed_lock);
	wake_lock_init(&psy->work_wake_lock, WAKE_LOCK_SUSPEND, "power-supply");

	rc = power_supply_create_triggers(psy);
	if (rc)
		goto create_triggers_failed;

	power_supply_changed(psy);

	goto success;

create_triggers_failed:
	wake_lock_destroy(&psy->work_wake_lock);
	device_del(dev);
kobject_set_name_failed:
device_add_failed:
	put_device(dev);
success:
	return rc;
}
EXPORT_SYMBOL_GPL(power_supply_register);

void power_supply_unregister(struct power_supply *psy)
{
	cancel_work_sync(&psy->changed_work);
	sysfs_remove_link(&psy->dev->kobj, "powers");
	power_supply_remove_triggers(psy);
	wake_lock_destroy(&psy->work_wake_lock);
	device_unregister(psy->dev);
}
EXPORT_SYMBOL_GPL(power_supply_unregister);

static int __init power_supply_class_init(void)
{
	power_supply_class = class_create(THIS_MODULE, "power_supply");

	if (IS_ERR(power_supply_class))
		return PTR_ERR(power_supply_class);

	power_supply_class->dev_uevent = power_supply_uevent;
	power_supply_init_attrs(&power_supply_dev_type);

	return 0;
}

static void __exit power_supply_class_exit(void)
{
	class_destroy(power_supply_class);
}

subsys_initcall(power_supply_class_init);
module_exit(power_supply_class_exit);

MODULE_DESCRIPTION("Universal power supply monitor class");
MODULE_AUTHOR("Ian Molton <spyro@f2s.com>, "
	      "Szabolcs Gyurko, "
	      "Anton Vorontsov <cbou@mail.ru>");
MODULE_LICENSE("GPL");
