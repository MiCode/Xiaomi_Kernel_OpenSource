/*
 * Copyright (c) 2011 Synaptics Incorporated
 * Copyright (c) 2011 Unixphere
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/rmi.h>

static struct rmi_function_list {
	struct list_head list;
	struct rmi_function_handler *fh;
} rmi_supported_functions;

static int rmi_bus_match(struct device *dev, struct device_driver *driver)
{
	struct rmi_driver *rmi_driver;
	struct rmi_device *rmi_dev;
	struct rmi_device_platform_data *pdata;

	pr_info("in function ____%s____  \n", __func__);
	rmi_driver = to_rmi_driver(driver);
	rmi_dev = to_rmi_device(dev);
	pdata = to_rmi_platform_data(rmi_dev);

	pr_info("rmi_driver->driver.name =  %s\n", rmi_driver->driver.name);
	pr_info("device:rmi_device:rmi_device_platform_data:driver_name = %s\n",
		pdata->driver_name);

	if (!strcmp(pdata->driver_name, rmi_driver->driver.name)) {
		rmi_dev->driver = rmi_driver;
		return 1;
	}
	pr_info("names DO NOT match, so return nothing\n");

	return 0;
}

#ifdef CONFIG_PM
static int rmi_bus_suspend(struct device *dev)
{
#ifdef GENERIC_SUBSYS_PM_OPS
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm && pm->suspend)
		return pm->suspend(dev);
#endif

	return 0;
}

static int rmi_bus_resume(struct device *dev)
{
#ifdef GENERIC_SUBSYS_PM_OPS
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;
	pr_info("in function ____%s____  \n", __func__);

	if (pm && pm->resume)
		return pm->resume(dev);
#endif

	return 0;
}
#endif

static int rmi_bus_probe(struct device *dev)
{
	struct rmi_driver *driver;
	struct rmi_device *rmi_dev = to_rmi_device(dev);

	pr_info("in function ____%s____  \n", __func__);
	driver = rmi_dev->driver;
	if (driver && driver->probe)
		return driver->probe(rmi_dev);

	return 0;
}

static int rmi_bus_remove(struct device *dev)
{
	struct rmi_driver *driver;
	struct rmi_device *rmi_dev = to_rmi_device(dev);

	pr_info("in function ____%s____  \n", __func__);
	driver = rmi_dev->driver;
	if (driver && driver->remove)
		return driver->remove(rmi_dev);

	return 0;
}

static void rmi_bus_shutdown(struct device *dev)
{
	struct rmi_driver *driver;
	struct rmi_device *rmi_dev = to_rmi_device(dev);

	driver = rmi_dev->driver;
	if (driver && driver->shutdown)
		driver->shutdown(rmi_dev);
}

static SIMPLE_DEV_PM_OPS(rmi_bus_pm_ops,
			 rmi_bus_suspend, rmi_bus_resume);

struct bus_type rmi_bus_type = {
	.name		= "rmi",
	.match		= rmi_bus_match,
	.probe		= rmi_bus_probe,
	.remove		= rmi_bus_remove,
	.shutdown	= rmi_bus_shutdown,
	.pm		= &rmi_bus_pm_ops
};

int rmi_register_phys_device(struct rmi_phys_device *phys)
{
	static int phys_device_num;
	struct rmi_device_platform_data *pdata = phys->dev->platform_data;
	struct rmi_device *rmi_dev;

	pr_info("in function ____%s____  \n", __func__);

	if (!pdata) {
		dev_err(phys->dev, "no platform data!\n");
		return -EINVAL;
	}

	rmi_dev = kzalloc(sizeof(struct rmi_device), GFP_KERNEL);
	if (!rmi_dev)
		return -ENOMEM;

	rmi_dev->phys = phys;
	rmi_dev->dev.bus = &rmi_bus_type;
	dev_set_name(&rmi_dev->dev, "sensor%02d", phys_device_num++);

	phys->rmi_dev = rmi_dev;
	pr_info("registering physical device:\n");
	pr_info("dev.init_name = %s\n", rmi_dev->dev.init_name);
	pr_info("dev.bus->name = %s\n", rmi_dev->dev.bus->name);
	return device_register(&rmi_dev->dev);
}
EXPORT_SYMBOL(rmi_register_phys_device);

void rmi_unregister_phys_device(struct rmi_phys_device *phys)
{
	struct rmi_device *rmi_dev = phys->rmi_dev;
	pr_info("in function ____%s____  \n", __func__);

	device_unregister(&rmi_dev->dev);
	kfree(rmi_dev);
}
EXPORT_SYMBOL(rmi_unregister_phys_device);

int rmi_register_driver(struct rmi_driver *driver)
{
	pr_info("in function ____%s____  \n", __func__);
	driver->driver.bus = &rmi_bus_type;
	return driver_register(&driver->driver);
}
EXPORT_SYMBOL(rmi_register_driver);

static int __rmi_driver_remove(struct device *dev, void *data)
{
	struct rmi_driver *driver = data;
	struct rmi_device *rmi_dev = to_rmi_device(dev);

	if (rmi_dev->driver == driver)
		rmi_dev->driver = NULL;

	return 0;
}

void rmi_unregister_driver(struct rmi_driver *driver)
{
	bus_for_each_dev(&rmi_bus_type, NULL, driver, __rmi_driver_remove);
	driver_unregister(&driver->driver);
}
EXPORT_SYMBOL(rmi_unregister_driver);

static int __rmi_bus_fh_add(struct device *dev, void *data)
{
	struct rmi_driver *driver;
	struct rmi_device *rmi_dev = to_rmi_device(dev);
	pr_info("in function ____%s____  \n", __func__);

	driver = rmi_dev->driver;
	if (driver && driver->fh_add)
		driver->fh_add(rmi_dev, data);

	return 0;
}

int rmi_register_function_driver(struct rmi_function_handler *fh)
{
	struct rmi_function_list *entry;
	struct rmi_function_handler *fh_dup;

	fh_dup = rmi_get_function_handler(fh->func);
	if (fh_dup) {
		pr_err("%s: function f%.2x already registered!\n", __func__,
			fh->func);
		return -EINVAL;
	}

	entry = kzalloc(sizeof(struct rmi_function_list), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	entry->fh = fh;
	list_add_tail(&entry->list, &rmi_supported_functions.list);

	/* notify devices of the new function handler */
	bus_for_each_dev(&rmi_bus_type, NULL, fh, __rmi_bus_fh_add);

	return 0;
}
EXPORT_SYMBOL(rmi_register_function_driver);

static int __rmi_bus_fh_remove(struct device *dev, void *data)
{
	struct rmi_driver *driver;
	struct rmi_device *rmi_dev = to_rmi_device(dev);

	pr_info("in function ____%s____  \n", __func__);
	driver = rmi_dev->driver;
	if (driver && driver->fh_remove)
		driver->fh_remove(rmi_dev, data);

	return 0;
}

void rmi_unregister_function_driver(struct rmi_function_handler *fh)
{
	struct rmi_function_list *entry, *n;
	pr_info("in function ____%s____  \n", __func__);

	/* notify devices of the removal of the function handler */
	bus_for_each_dev(&rmi_bus_type, NULL, fh, __rmi_bus_fh_remove);

	list_for_each_entry_safe(entry, n, &rmi_supported_functions.list, list)
		if (entry->fh->func == fh->func) {
			list_del(&entry->list);
			kfree(entry);
		}
}
EXPORT_SYMBOL(rmi_unregister_function_driver);

struct rmi_function_handler *rmi_get_function_handler(int id)
{
	struct rmi_function_list *entry;
	pr_info("in function ____%s____  \n", __func__);

	list_for_each_entry(entry, &rmi_supported_functions.list, list)
		if (entry->fh->func == id)
			return entry->fh;

	return NULL;
}
EXPORT_SYMBOL(rmi_get_function_handler);

static int __init rmi_bus_init(void)
{
	int error;

	pr_info("in function ____%s____  \n", __func__);
	INIT_LIST_HEAD(&rmi_supported_functions.list);

	error = bus_register(&rmi_bus_type);
	if (error < 0) {
		pr_err("%s: error registering the RMI bus: %d\n", __func__,
		       error);
		return error;
	}
	pr_info("%s: successfully registered RMI bus.\n", __func__);

	return 0;
}

static void __exit rmi_bus_exit(void)
{
	struct rmi_function_list *entry, *n;
	pr_info("in function ____%s____  \n", __func__);

	list_for_each_entry_safe(entry, n, &rmi_supported_functions.list,
				 list) {
		list_del(&entry->list);
		kfree(entry);
	}

	bus_unregister(&rmi_bus_type);
}

module_init(rmi_bus_init);
module_exit(rmi_bus_exit);

MODULE_AUTHOR("Eric Andersson <eric.andersson@unixphere.com>");
MODULE_DESCRIPTION("RMI bus");
MODULE_LICENSE("GPL");
