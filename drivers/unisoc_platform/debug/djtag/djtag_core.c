/* Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include "djtag.h"

static inline struct djtag_device *to_djtag_device(struct device *dev)
{
	return dev ? container_of(dev, struct djtag_device, dev) : NULL;
}

static inline void *djtag_master_get_devdata(struct djtag_master *master)
{
	return dev_get_drvdata(&master->dev);
}

static inline struct djtag_master *djtag_master_get(struct djtag_master *master)
{
	if (!master || !get_device(&master->dev))
		return NULL;
	return master;
}

static inline void djtag_master_put(struct djtag_master *master)
{
	if (master)
		put_device(&master->dev);
}

static int djtag_match_device(struct device *dev, struct device_driver *drv)
{
	return of_driver_match_device(dev, drv);
}

static void djtag_master_release(struct device *dev)
{
	struct djtag_master *master;

	master = container_of(dev, struct djtag_master, dev);
	kfree(master);
}

static void djtag_dev_release(struct device *dev)
{
	struct djtag_device *dap = to_djtag_device(dev);

	djtag_master_put(dap->master);
	kfree(dap);
}

struct bus_type djtag_bus_type = {
	.name		= "djtag",
	.match		= djtag_match_device,
};

static struct class djtag_master_class = {
	.name		= "djtag",
	.owner		= THIS_MODULE,
	.dev_release	= djtag_master_release,
};

struct djtag_device *djtag_alloc_device(struct djtag_master *master)
{
	struct djtag_device *dap;

	if (!djtag_master_get(master))
		return NULL;

	dap = kzalloc(sizeof(*dap), GFP_KERNEL);
	if (!dap) {
		djtag_master_put(master);
		return NULL;
	}

	dap->master = master;
	dap->dev.parent = &master->dev;
	dap->dev.bus = &djtag_bus_type;
	dap->dev.release = djtag_dev_release;

	device_initialize(&dap->dev);
	return dap;
}

static struct djtag_device *
of_register_djtag_device(struct djtag_master *master, struct device_node *nc)
{
	struct djtag_device *dap;
	u32 reg;
	int rc;

	dap = djtag_alloc_device(master);
	if (!dap) {
		dev_err(&master->dev, "djtag alloc device fail\n");
		return NULL;
	}

	rc = of_modalias_node(nc, dap->modalias,
				sizeof(dap->modalias));
	if (rc < 0) {
		dev_err(&master->dev, "cannot find modalias for %s\n",
			nc->full_name);
		goto err_out;
	}

	rc = of_property_read_u32(nc, "reg", &reg);
	if (rc) {
		dev_err(&master->dev, "%s has no valid 'reg' property (%d)\n",
			nc->full_name, rc);
		goto err_out;
	}
	dap->sys = reg;
	dap->dev.of_node = nc;
	dev_set_name(&dap->dev, "%s", nc->name);
	rc = device_add(&dap->dev);
	if (rc < 0) {
		dev_err(&master->dev, "can't add %s, status %d\n",
			dev_name(&dap->dev), rc);
		goto err_out;
	}

	dev_dbg(&master->dev, "registered child %s\n", dev_name(&dap->dev));
	return dap;

err_out:
	djtag_dev_release(&dap->dev);
	return NULL;
}

static void of_register_djtag_devices(struct djtag_master *master)
{
	struct djtag_device *dap;
	struct device_node *nc;

	if (!master->dev.of_node)
		return;

	for_each_available_child_of_node(master->dev.of_node, nc) {
		dap = of_register_djtag_device(master, nc);
		if (!dap)
			dev_warn(&master->dev, "Failed to create djtag device for %s\n",
				nc->full_name);
	}

}

static int djtag_drv_probe(struct device *dev)
{
	struct djtag_driver *ddrv = to_djtag_driver(dev->driver);
	struct djtag_device *ddev = to_djtag_device(dev);

	return ddrv->probe(ddev);
}

static int djtag_drv_remove(struct device *dev)
{
	struct djtag_driver *ddrv = to_djtag_driver(dev->driver);
	struct djtag_device *ddev = to_djtag_device(dev);

	return ddrv->remove(ddev);
}

static void djtag_drv_shutdown(struct device *dev)
{
	struct djtag_driver *ddrv = to_djtag_driver(dev->driver);
	struct djtag_device *ddev = to_djtag_device(dev);

	ddrv->shutdown(ddev);
}

int djtag_register_real_driver(struct module *owner, struct djtag_driver *ddrv)
{
	ddrv->driver.owner = owner;
	ddrv->driver.bus = &djtag_bus_type;

	if (ddrv->probe)
		ddrv->driver.probe = djtag_drv_probe;
	if (ddrv->remove)
		ddrv->driver.remove = djtag_drv_remove;
	if (ddrv->shutdown)
		ddrv->driver.shutdown = djtag_drv_shutdown;

	return driver_register(&ddrv->driver);
}
EXPORT_SYMBOL_GPL(djtag_register_real_driver);

int register_djtag_master(struct djtag_master *master)
{
	int status;

	master->dev.class = &djtag_master_class;
	device_initialize(&master->dev);
	dev_set_name(&master->dev, "djtag");
	status = device_add(&master->dev);
	if (status)
		return status;
	of_register_djtag_devices(master);

	return 0;
}
EXPORT_SYMBOL_GPL(register_djtag_master);

static void djtag_unregister_device(struct djtag_device *dd)
{
	if (dd)
		device_unregister(&dd->dev);
}

static int djtag_unregister(struct device *dev, void *data)
{
	djtag_unregister_device(to_djtag_device(dev));
	return 0;
}

void djtag_unregister_master(struct djtag_master *master)
{
	device_for_each_child(&master->dev, NULL, djtag_unregister);
	device_unregister(&master->dev);
}
EXPORT_SYMBOL_GPL(djtag_unregister_master);

static int __init djtag_init(void)
{
	int status;

	status = bus_register(&djtag_bus_type);
	if (status < 0)
		return status;

	status = class_register(&djtag_master_class);
	if (status < 0)
		goto class_err;

	return 0;

class_err:
	bus_unregister(&djtag_bus_type);
	return status;
}

postcore_initcall(djtag_init);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Lanqing Liu<lanqing.liu@spreadtrum.com>");
MODULE_DESCRIPTION("spreadtrum platform DJTAG driver");
