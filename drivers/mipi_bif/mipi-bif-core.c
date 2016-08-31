/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/idr.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/hardirq.h>
#include <linux/irqflags.h>
#include <linux/pm_runtime.h>
#include <linux/mipi-bif.h>

static DEFINE_MUTEX(core_lock);
static DEFINE_IDR(mipi_bif_adapter_idr);

int __mipi_bif_first_dynamic_bus_num;

#ifdef CONFIG_MIPI_BIF_COMPAT
static struct class_compat *mipi_bif_adapter_compat_class;
#endif

static struct device_type mipi_bif_client_type;

static ssize_t
show_name(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", dev->type == &mipi_bif_client_type ?
		to_mipi_bif_client(dev)->name : to_mipi_bif_adapter(dev)->name);
}

static DEVICE_ATTR(name, S_IRUGO, show_name, NULL);

static struct attribute *mipi_bif_adapter_attrs[] = {
	&dev_attr_name.attr,
	NULL
};

static struct attribute_group mipi_bif_adapter_attr_group = {
	.attrs		= mipi_bif_adapter_attrs,
};

static const struct attribute_group *mipi_bif_adapter_attr_groups[] = {
	&mipi_bif_adapter_attr_group,
	NULL
};

static void mipi_bif_adapter_dev_release(struct device *dev)
{
	struct mipi_bif_adapter *adap = to_mipi_bif_adapter(dev);
	complete(&adap->dev_released);
}

struct device_type mipi_bif_adapter_type = {
	.groups		= mipi_bif_adapter_attr_groups,
	.release	= mipi_bif_adapter_dev_release,
};

static struct attribute *mipi_bif_client_attrs[] = {
	&dev_attr_name.attr,
	NULL
};

static struct attribute_group mipi_bif_client_attr_group = {
	.attrs		= mipi_bif_client_attrs,
};

static const struct attribute_group *mipi_bif_client_attr_groups[] = {
	&mipi_bif_client_attr_group,
	NULL
};

static void mipi_bif_client_dev_release(struct device *dev)
{
	kfree(to_mipi_bif_client(dev));
}

static struct device_type mipi_bif_client_type = {
	.groups		= mipi_bif_client_attr_groups,
	.release	= mipi_bif_client_dev_release,
};

static const struct mipi_bif_device_id *mipi_bif_match_id(
	const struct mipi_bif_device_id *id,
	const struct mipi_bif_client *client)
{
	while (id->name[0]) {
		if (strcmp(client->name, id->name) == 0)
			return id;
		id++;
	}
	return NULL;
}

static int mipi_bif_device_match(struct device *dev, struct device_driver *drv)
{
	if (dev->type == &mipi_bif_client_type) {

		struct mipi_bif_client *client = to_mipi_bif_client(dev);
		struct mipi_bif_driver *driver = to_mipi_bif_driver(drv);
		if (driver->id_table)
			return mipi_bif_match_id(driver->id_table,
							client) != NULL;
	}
	return 0;
}

static int mipi_bif_device_probe(struct device *dev)
{
	int status = 0;
	if (dev->type == &mipi_bif_client_type) {

		struct mipi_bif_client *client = to_mipi_bif_client(dev);
		struct mipi_bif_driver *driver;

		if (!client)
			return 0;

		driver = to_mipi_bif_driver(dev->driver);
		if (!driver->probe || !driver->id_table)
			return -ENODEV;
		client->driver = driver;

		dev_dbg(dev, "probe\n");

		status = driver->probe(client,
				mipi_bif_match_id(driver->id_table, client));
		if (status) {
			client->driver = NULL;
			mipi_bif_set_clientdata(client, NULL);
		}
	}
	return status;
}

static int mipi_bif_device_remove(struct device *dev)
{
	int status = 0;
	if (dev->type == &mipi_bif_client_type) {
		struct mipi_bif_client	*client = to_mipi_bif_client(dev);
		struct mipi_bif_driver	*driver;

		if (!client || !dev->driver)
			return 0;

		driver = to_mipi_bif_driver(dev->driver);
		if (driver->remove) {
			dev_dbg(dev, "remove\n");
			status = driver->remove(client);
		} else {
			dev->driver = NULL;
			status = 0;
		}
		if (status == 0) {
			client->driver = NULL;
			mipi_bif_set_clientdata(client, NULL);
		}
	}
	return status;
}

static void mipi_bif_device_shutdown(struct device *dev)
{
	if (dev->type == &mipi_bif_client_type) {

		struct mipi_bif_client *client = to_mipi_bif_client(dev);
		struct mipi_bif_driver *driver;

		if (!client || !dev->driver)
			return;
		driver = to_mipi_bif_driver(dev->driver);
		if (driver->shutdown)
			driver->shutdown(client);

	}
}
#ifdef CONFIG_PM_SLEEP
static int mipi_bif_legacy_suspend(struct device *dev, pm_message_t mesg)
{
	if (dev->type == &mipi_bif_client_type) {

		struct mipi_bif_client *client = to_mipi_bif_client(dev);

		struct mipi_bif_driver *driver;

		if (!client || !dev->driver)
			return 0;
		driver = to_mipi_bif_driver(dev->driver);
		if (!driver->suspend)
			return 0;
		return driver->suspend(client, mesg);

	}
	return 0;
}

static int mipi_bif_legacy_resume(struct device *dev)
{
	if (dev->type == &mipi_bif_client_type) {

		struct mipi_bif_client *client = to_mipi_bif_client(dev);
		struct mipi_bif_driver *driver;

		if (!client || !dev->driver)
			return 0;
		driver = to_mipi_bif_driver(dev->driver);
		if (!driver->resume)
			return 0;
		return driver->resume(client);
	}
	return 0;
}

static int mipi_bif_device_pm_suspend(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm)
		return pm_generic_suspend(dev);
	else
		return mipi_bif_legacy_suspend(dev, PMSG_SUSPEND);
}

static int mipi_bif_device_pm_resume(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm)
		return pm_generic_resume(dev);
	else
		return mipi_bif_legacy_resume(dev);
}

static int mipi_bif_device_pm_freeze(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm)
		return pm_generic_freeze(dev);
	else
		return mipi_bif_legacy_suspend(dev, PMSG_FREEZE);
}

static int mipi_bif_device_pm_thaw(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm)
		return pm_generic_thaw(dev);
	else
		return mipi_bif_legacy_resume(dev);
}

static int mipi_bif_device_pm_poweroff(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm)
		return pm_generic_poweroff(dev);
	else
		return mipi_bif_legacy_suspend(dev, PMSG_HIBERNATE);
}

static int mipi_bif_device_pm_restore(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm)
		return pm_generic_restore(dev);
	else
		return mipi_bif_legacy_resume(dev);
}
#else /* !CONFIG_PM_SLEEP */
#define mipi_bif_device_pm_suspend	NULL
#define mipi_bif_device_pm_resume	NULL
#define mipi_bif_device_pm_freeze	NULL
#define mipi_bif_device_pm_thaw	NULL
#define mipi_bif_device_pm_poweroff	NULL
#define mipi_bif_device_pm_restore	NULL
#endif /* !CONFIG_PM_SLEEP */

static const struct dev_pm_ops mipi_bif_device_pm_ops = {
	.suspend = mipi_bif_device_pm_suspend,
	.resume = mipi_bif_device_pm_resume,
	.freeze = mipi_bif_device_pm_freeze,
	.thaw = mipi_bif_device_pm_thaw,
	.poweroff = mipi_bif_device_pm_poweroff,
	.restore = mipi_bif_device_pm_restore,
	SET_RUNTIME_PM_OPS(
		pm_generic_runtime_suspend,
		pm_generic_runtime_resume,
		pm_generic_runtime_idle
	)
};

struct bus_type mipi_bif_bus_type = {
	.name		= "mipi_bif",
	.match		= mipi_bif_device_match,
	.probe		= mipi_bif_device_probe,
	.remove		= mipi_bif_device_remove,
	.shutdown	= mipi_bif_device_shutdown,
	.pm		= &mipi_bif_device_pm_ops,
};
EXPORT_SYMBOL_GPL(mipi_bif_bus_type);

void mipi_bif_lock_adapter(struct mipi_bif_adapter *adapter)
{
	struct mipi_bif_adapter *parent =
				mipi_bif_parent_is_mipi_bif_adapter(adapter);

	if (parent)
		mipi_bif_lock_adapter(parent);
	else
		rt_mutex_lock(&adapter->bus_lock);
}
EXPORT_SYMBOL_GPL(mipi_bif_lock_adapter);

static int mipi_bif_trylock_adapter(struct mipi_bif_adapter *adapter)
{
	struct mipi_bif_adapter *parent =
				mipi_bif_parent_is_mipi_bif_adapter(adapter);

	if (parent)
		return mipi_bif_trylock_adapter(parent);
	else
		return rt_mutex_trylock(&adapter->bus_lock);
}

void mipi_bif_unlock_adapter(struct mipi_bif_adapter *adapter)
{
	struct mipi_bif_adapter *parent =
				mipi_bif_parent_is_mipi_bif_adapter(adapter);

	if (parent)
		mipi_bif_unlock_adapter(parent);
	else
		rt_mutex_unlock(&adapter->bus_lock);
}
EXPORT_SYMBOL_GPL(mipi_bif_unlock_adapter);

static int mipi_bif_register_adapter(struct mipi_bif_adapter *adap)
{
	int res = 0;

	/* Can't register until after driver model init */
	if (unlikely(WARN_ON(!mipi_bif_bus_type.p))) {
		res = -EAGAIN;
		goto out_list;
	}

	if (unlikely(adap->name[0] == '\0')) {
		pr_err("mipi-bif-core:Attempt to add adapter without name\n");
		return -EINVAL;
	}
	if (unlikely(!adap->algo)) {
		pr_err("mipi-bif-core:Attempt to add adapter %s without algo\n",
								 adap->name);
		return -EINVAL;
	}

	rt_mutex_init(&adap->bus_lock);

	if (adap->timeout == 0)
		adap->timeout = HZ;

	dev_set_name(&adap->dev, "mipi-bif.%d", adap->nr);
	adap->dev.bus = &mipi_bif_bus_type;
	adap->dev.type = &mipi_bif_adapter_type;
	res = device_register(&adap->dev);
	if (res)
		goto out_list;

	dev_dbg(&adap->dev, "adapter %s registered\n", adap->name);

#ifdef CONFIG_MIPI_BIF_COMPAT
	res = class_compat_create_link(mipi_bif_adapter_compat_class,
				&adap->dev, adap->dev.parent);
	if (res)
		dev_warn(&adap->dev,
			 "Failed to create compatibility class link\n");
#endif

	return 0;

out_list:
	mutex_lock(&core_lock);
	idr_remove(&mipi_bif_adapter_idr, adap->nr);
	mutex_unlock(&core_lock);
	return res;
}

/**
 * mipi_bif_add_adapter - declare mipi_bif adapter, use dynamic bus number
 * @adapter: the adapter to add
 * Context: can sleep
 *
 * When this returns zero, a new bus number was allocated and stored
 * in adap->nr, and the specified adapter became available for clients.
 * Otherwise, a negative errno value is returned.
 */
int mipi_bif_add_adapter(struct mipi_bif_adapter *adapter)
{
	int	id, res = 0;

retry:
	if (idr_pre_get(&mipi_bif_adapter_idr, GFP_KERNEL) == 0)
		return -ENOMEM;

	mutex_lock(&core_lock);
	/* "above" here means "above or equal to", sigh */
	res = idr_get_new_above(&mipi_bif_adapter_idr, adapter,
				__mipi_bif_first_dynamic_bus_num, &id);
	mutex_unlock(&core_lock);

	if (res < 0) {
		if (res == -EAGAIN)
			goto retry;
		return res;
	}

	adapter->nr = id;
	return mipi_bif_register_adapter(adapter);
}
EXPORT_SYMBOL_GPL(mipi_bif_add_adapter);

/*
 * mipi_bif_add_numbered_adapter - declare mipi_bif adapter with static bus num
 * @adap: the adapter to register (with adap->nr initialized)
 * Context: can sleep
 */
int mipi_bif_add_numbered_adapter(struct mipi_bif_adapter *adap)
{
	int	id;
	int	status;

	if (adap->nr == -1) /* -1 means dynamically assign bus id */
		return mipi_bif_add_adapter(adap);
	if (adap->nr & ~MAX_IDR_MASK)
		return -EINVAL;

retry:
	if (idr_pre_get(&mipi_bif_adapter_idr, GFP_KERNEL) == 0)
		return -ENOMEM;

	mutex_lock(&core_lock);

	status = idr_get_new_above(&mipi_bif_adapter_idr, adap, adap->nr, &id);
	if (status == 0 && id != adap->nr) {
		status = -EBUSY;
		idr_remove(&mipi_bif_adapter_idr, id);
	}
	mutex_unlock(&core_lock);
	if (status == -EAGAIN)
		goto retry;

	if (status == 0)
		status = mipi_bif_register_adapter(adap);

	return status;
}
EXPORT_SYMBOL_GPL(mipi_bif_add_numbered_adapter);

void mipi_bif_unregister_device(struct mipi_bif_client *client)
{
	device_unregister(&client->dev);
}
EXPORT_SYMBOL_GPL(mipi_bif_unregister_device);

static int mipi_bif_do_del_adapter(struct mipi_bif_driver *driver,
			      struct mipi_bif_adapter *adapter)
{
	int res;
	if (!driver->detach_adapter)
		return 0;
	dev_warn(&adapter->dev, "%s: detach_adapter method is deprecated\n",
		 driver->driver.name);

	res = driver->detach_adapter(adapter);
	if (res)
		dev_err(&adapter->dev, "detach_adapter failed (%d)"\
		"for driver %s\n", res, driver->driver.name);
	return res;
}

static int __process_removed_adapter(struct device_driver *d, void *data)
{
	return mipi_bif_do_del_adapter(to_mipi_bif_driver(d), data);
}

int mipi_bif_del_adapter(struct mipi_bif_adapter *adap)
{
	int res = 0;
	struct mipi_bif_adapter *found;

	/* First make sure that this adapter was ever added */
	mutex_lock(&core_lock);
	found = idr_find(&mipi_bif_adapter_idr, adap->nr);
	mutex_unlock(&core_lock);
	if (found != adap) {
		pr_debug("mipi-bif-core: attempting to delete unregistered "\
		"adapter %s\n", adap->name);
		return -EINVAL;
	}

	/* Tell drivers about this removal */
	mutex_lock(&core_lock);
	res = bus_for_each_drv(&mipi_bif_bus_type, NULL, adap,
			       __process_removed_adapter);
	mutex_unlock(&core_lock);
	if (res)
		return res;

#ifdef CONFIG_MIPI_BIF_COMPAT
	class_compat_remove_link(mipi_bif_adapter_compat_class, &adap->dev,
				 adap->dev.parent);
#endif
	dev_dbg(&adap->dev, "adapter %s unregistered\n", adap->name);

	init_completion(&adap->dev_released);
	device_unregister(&adap->dev);
	/* wait for sysfs to drop all references */
	wait_for_completion(&adap->dev_released);

	/* free bus id */
	mutex_lock(&core_lock);
	idr_remove(&mipi_bif_adapter_idr, adap->nr);
	mutex_unlock(&core_lock);

	memset(&adap->dev, 0, sizeof(adap->dev));

	return 0;
}
EXPORT_SYMBOL_GPL(mipi_bif_del_adapter);

struct mipi_bif_client *
	mipi_bif_new_device(struct mipi_bif_adapter *adap,
	struct mipi_bif_board_info const *info)
{
	struct mipi_bif_client	*client;
	int			status;

	client = kzalloc(sizeof *client, GFP_KERNEL);
	if (!client)
		return NULL;

	client->adapter = adap;
	client->addr = info->addr;
	client->dev.platform_data = info->platform_data;

	if (info->archdata)
		client->dev.archdata = *info->archdata;

	strlcpy(client->name, info->type, sizeof(client->name));

	client->dev.parent = &client->adapter->dev;
	client->dev.bus = &mipi_bif_bus_type;
	client->dev.type = &mipi_bif_client_type;

	dev_set_name(&client->dev, "%d-%04x",
				mipi_bif_adapter_id(adap), client->addr);

	status = device_register(&client->dev);
	if (status)
		goto out_err;

	dev_dbg(&adap->dev, "client %s registered with bus id %s\n",
		client->name, dev_name(&client->dev));

	return client;

out_err:
	dev_err(&adap->dev, "Failed to register mipi_bif client %s at 0x%02x "\
		"(%d)\n", client->name, client->addr, status);
	kfree(client);
	return NULL;
}
EXPORT_SYMBOL_GPL(mipi_bif_new_device);

int mipi_bif_register_driver(struct module *owner,
	struct mipi_bif_driver *driver)
{
	int res;

	/* Can't register until after driver model init */
	if (unlikely(WARN_ON(!mipi_bif_bus_type.p)))
		return -EAGAIN;

	/* add the driver to the list of mipi_bif drivers in the driver core */
	driver->driver.owner = owner;
	driver->driver.bus = &mipi_bif_bus_type;

	/* When registration returns, the driver core
	 * will have called probe() for all matching-but-unbound devices.
	 */
	res = driver_register(&driver->driver);
	if (res)
		return res;

	/* Drivers should switch to dev_pm_ops instead. */
	if (driver->suspend)
		pr_warn("mipi-bif-core: driver %s using legacy suspend\n",
							driver->driver.name);
	if (driver->resume)
		pr_warn("mipi-bif-core: driver %s using legacy resume\n",
							driver->driver.name);

	pr_debug("mipi-bif-core: driver %s registered\n", driver->driver.name);

	INIT_LIST_HEAD(&driver->clients);
	return 0;
}
EXPORT_SYMBOL_GPL(mipi_bif_register_driver);

int mipi_bif_for_each_dev(void *data, int (*fn)(struct device *, void *))
{
	int res;

	mutex_lock(&core_lock);
	res = bus_for_each_dev(&mipi_bif_bus_type, NULL, data, fn);
	mutex_unlock(&core_lock);

	return res;
}

static int __process_removed_driver(struct device *dev, void *data)
{
	if (dev->type != &mipi_bif_adapter_type)
		return 0;
	return mipi_bif_do_del_adapter(data, to_mipi_bif_adapter(dev));
}

void mipi_bif_del_driver(struct mipi_bif_driver *driver)
{
	mipi_bif_for_each_dev(driver, __process_removed_driver);

	driver_unregister(&driver->driver);
	pr_debug("mipi-bif-core:driver %s unregistered\n", driver->driver.name);
}
EXPORT_SYMBOL_GPL(mipi_bif_del_driver);

struct mipi_bif_adapter *mipi_bif_get_adapter(int nr)
{
	struct mipi_bif_adapter *adapter;

	mutex_lock(&core_lock);
	adapter = idr_find(&mipi_bif_adapter_idr, nr);
	if (adapter && !try_module_get(adapter->owner))
		adapter = NULL;

	mutex_unlock(&core_lock);
	return adapter;
}
EXPORT_SYMBOL_GPL(mipi_bif_get_adapter);

int mipi_bif_transfer(struct mipi_bif_adapter *adap, struct mipi_bif_msg *msg)
{
	unsigned long orig_jiffies;
	int ret, try;

	if (adap->algo->master_xfer) {

		if (in_atomic() || irqs_disabled()) {
			ret = mipi_bif_trylock_adapter(adap);
			if (!ret)
				/* mipi_bif activity is ongoing. */
				return -EAGAIN;
		} else {
			mipi_bif_lock_adapter(adap);
		}

		/* Retry automatically on arbitration loss */
		orig_jiffies = jiffies;
		for (ret = 0, try = 0; try <= adap->retries; try++) {
			ret = adap->algo->master_xfer(adap, msg);
			if (ret != -EAGAIN)
				break;
			if (time_after(jiffies, orig_jiffies + adap->timeout))
				break;
		}
		mipi_bif_unlock_adapter(adap);

		return ret;
	} else {
		dev_dbg(&adap->dev, "mipi_bif level transfers not supported\n");
		return -EOPNOTSUPP;
	}
}
EXPORT_SYMBOL_GPL(mipi_bif_transfer);

static int __init mipi_bif_init(void)
{
	int retval;

	retval = bus_register(&mipi_bif_bus_type);
	if (retval)
		return retval;
#ifdef CONFIG_MIPI_BIF_COMPAT
	mipi_bif_adapter_compat_class =
				class_compat_register("mipi_bif_adapter");
	if (!mipi_bif_adapter_compat_class) {
		retval = -ENOMEM;
		goto bus_err;
	}
#endif
	return 0;

#ifdef CONFIG_MIPI_BIF_COMPAT
	class_compat_unregister(mipi_bif_adapter_compat_class);
bus_err:
#endif
	bus_unregister(&mipi_bif_bus_type);
	return retval;
}

static void __exit mipi_bif_exit(void)
{
#ifdef CONFIG_MIPI_BIF_COMPAT
	class_compat_unregister(mipi_bif_adapter_compat_class);
#endif
	bus_unregister(&mipi_bif_bus_type);
}

postcore_initcall(mipi_bif_init);
module_exit(mipi_bif_exit);

MODULE_AUTHOR("Chaitanya Bandi<bandik@nvidia.com>");
MODULE_DESCRIPTION("MIPI BIF core driver module");
MODULE_LICENSE("GPL");
