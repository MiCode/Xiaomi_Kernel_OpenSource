/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
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

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>

#include <linux/pci.h>

#include "ipa_eth_i.h"

struct ipa_eth_pci_driver {
	struct list_head driver_list;

	struct ipa_eth_net_driver *nd;

	int (*probe_real)(struct pci_dev *dev, const struct pci_device_id *id);
	void (*remove_real)(struct pci_dev *dev);
};

static LIST_HEAD(pci_drivers);
static DEFINE_MUTEX(pci_drivers_mutex);

static LIST_HEAD(pci_devices);
static DEFINE_MUTEX(pci_devices_mutex);

static struct dentry *ipa_eth_pci_debugfs;

static bool ipa_eth_pci_is_ready;

static void ipa_eth_pci_debugfs_cleanup(void)
{
	debugfs_remove_recursive(ipa_eth_pci_debugfs);
}

static int ipa_eth_pci_debugfs_init(struct dentry *dbgfs_root)
{
	if (!dbgfs_root)
		return 0;

	ipa_eth_pci_debugfs = debugfs_create_dir("pci", dbgfs_root);
	if (IS_ERR_OR_NULL(ipa_eth_pci_debugfs)) {
		int rc = ipa_eth_pci_debugfs ?
			PTR_ERR(ipa_eth_pci_debugfs) : -EFAULT;

		ipa_eth_pci_debugfs = NULL;
		return rc;
	}

	return 0;
}

static struct ipa_eth_pci_driver *__lookup_driver(struct pci_driver *pci_drv)
{
	struct ipa_eth_pci_driver *epci_drv;

	list_for_each_entry(epci_drv, &pci_drivers, driver_list) {
		if (epci_drv->nd->driver == &pci_drv->driver)
			return epci_drv;
	}

	return NULL;
}

static struct ipa_eth_pci_driver *lookup_epci_driver(struct pci_driver *pci_drv)
{
	struct ipa_eth_pci_driver *epci_drv;

	mutex_lock(&pci_drivers_mutex);
	epci_drv = __lookup_driver(pci_drv);
	mutex_unlock(&pci_drivers_mutex);

	return epci_drv;
}

static struct ipa_eth_device *__lookup_eth_dev(struct pci_dev *pdev)
{
	struct device *dev = &pdev->dev;
	struct ipa_eth_device *eth_dev;

	list_for_each_entry(eth_dev, &pci_devices, bus_device_list) {
		if (eth_dev->dev == dev)
			return eth_dev;
	}

	return NULL;
}

static struct ipa_eth_device *lookup_eth_dev(struct pci_dev *pdev)
{
	struct ipa_eth_device *eth_dev;

	mutex_lock(&pci_devices_mutex);
	eth_dev = __lookup_eth_dev(pdev);
	mutex_unlock(&pci_devices_mutex);

	return eth_dev;
}

static int ipa_eth_pci_probe_handler(struct pci_dev *pdev,
				     const struct pci_device_id *id)
{
	int rc = 0;
	struct device *dev = &pdev->dev;
	struct ipa_eth_device *eth_dev;
	struct ipa_eth_pci_driver *epci_drv;

	ipa_eth_dbg("PCI probe called for %s driver with devfn %u",
		    pdev->driver->name, pdev->devfn);

	epci_drv = lookup_epci_driver(pdev->driver);

	rc = epci_drv->probe_real(pdev, id);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed real PCI probe of devfn=%u");
		goto err_probe;
	}

	eth_dev = devm_kzalloc(dev, sizeof(*eth_dev), GFP_KERNEL);
	if (!eth_dev) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	eth_dev->dev = dev;
	eth_dev->nd = epci_drv->nd;
	eth_dev->bus_priv = epci_drv;

	rc = ipa_eth_register_device(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to register PCI devfn=%u");
		goto err_register;
	}

	mutex_lock(&pci_devices_mutex);
	list_add(&eth_dev->bus_device_list, &pci_devices);
	mutex_unlock(&pci_devices_mutex);

	return 0;

err_register:
	devm_kfree(dev, eth_dev);
err_alloc:
	epci_drv->remove_real(pdev);
err_probe:
	return rc;
}

static void ipa_eth_pci_remove_handler(struct pci_dev *pdev)
{
	struct device *dev = &pdev->dev;
	struct ipa_eth_device *eth_dev = NULL;
	struct ipa_eth_pci_driver *epci_drv = NULL;

	ipa_eth_dbg("PCI remove called for %s driver with devfn %u",
		    pdev->driver->name, pdev->devfn);

	eth_dev = lookup_eth_dev(pdev);

	mutex_lock(&pci_devices_mutex);
	list_del(&eth_dev->bus_device_list);
	mutex_unlock(&pci_devices_mutex);

	ipa_eth_unregister_device(eth_dev);

	epci_drv = eth_dev->bus_priv;
	epci_drv->remove_real(pdev);

	devm_kfree(dev, eth_dev);
}

static int ipa_eth_pci_register_net_driver(struct ipa_eth_net_driver *nd)
{
	struct ipa_eth_pci_driver *epci_drv = NULL;
	struct pci_driver *pci_drv = container_of(nd->driver,
		struct pci_driver, driver);

	if (WARN_ON(!pci_drv->probe || !pci_drv->remove)) {
		ipa_eth_err("PCI driver lacking probe/remove callbacks");
		return -EFAULT;
	}

	epci_drv = kzalloc(sizeof(*epci_drv), GFP_KERNEL);
	if (!epci_drv)
		return -ENOMEM;

	epci_drv->probe_real = pci_drv->probe;
	pci_drv->probe = ipa_eth_pci_probe_handler;

	epci_drv->remove_real = pci_drv->remove;
	pci_drv->remove = ipa_eth_pci_remove_handler;

	epci_drv->nd = nd;

	mutex_lock(&pci_drivers_mutex);
	list_add(&epci_drv->driver_list, &pci_drivers);
	mutex_unlock(&pci_drivers_mutex);

	return 0;

}

static void ipa_eth_pci_unregister_net_driver(struct ipa_eth_net_driver *nd)
{
	struct pci_driver *pci_drv = container_of(nd->driver,
		struct pci_driver, driver);
	struct ipa_eth_pci_driver *epci_drv = lookup_epci_driver(pci_drv);

	mutex_lock(&pci_drivers_mutex);
	list_del(&epci_drv->driver_list);
	mutex_unlock(&pci_drivers_mutex);

	pci_drv->probe = epci_drv->probe_real;
	pci_drv->remove = epci_drv->remove_real;

	kfree(epci_drv);
}

struct ipa_eth_bus ipa_eth_pci_bus = {
	.bus = &pci_bus_type,
	.register_net_driver = ipa_eth_pci_register_net_driver,
	.unregister_net_driver = ipa_eth_pci_unregister_net_driver,
};

int ipa_eth_pci_modinit(struct dentry *dbgfs_root)
{
	int rc;

	rc = ipa_eth_pci_debugfs_init(dbgfs_root);
	if (rc) {
		ipa_eth_err("Unable to create debugfs root for pci bus");
		return rc;
	}

	ipa_eth_pci_is_ready = true;

	return 0;
}

void ipa_eth_pci_modexit(void)
{
	if (!ipa_eth_pci_is_ready)
		return;

	ipa_eth_pci_is_ready = false;

	ipa_eth_pci_debugfs_cleanup();
}
