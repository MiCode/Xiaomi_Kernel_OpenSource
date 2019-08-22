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
#include <linux/msm_pcie.h>

#include "ipa_eth_i.h"

struct ipa_eth_pci_driver {
	struct list_head driver_list;

	struct ipa_eth_net_driver *nd;

	int (*probe_real)(struct pci_dev *dev, const struct pci_device_id *id);
	void (*remove_real)(struct pci_dev *dev);

	const struct dev_pm_ops *pm_ops_real;
};

struct ipa_eth_pci_device {
	struct ipa_eth_device *eth_dev;
	struct ipa_eth_pci_driver *epci_drv;
	struct msm_pcie_register_event pcie_event;
};

#define eth_dev_pm_ops(edev) \
	(((struct ipa_eth_pci_device *)edev->bus_priv)->epci_drv->pm_ops_real)

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

static struct ipa_eth_pci_driver *__lookup_epci_driver(
					struct pci_driver *pci_drv)
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
	epci_drv = __lookup_epci_driver(pci_drv);
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

static void ipa_eth_pcie_event_cb(struct msm_pcie_notify *notify)
{
	struct pci_dev *pdev = notify->user;
	struct ipa_eth_device *eth_dev;

	eth_dev = __lookup_eth_dev(pdev);
	if (!eth_dev) {
		ipa_eth_bug("Failed to lookup eth device");
		return;
	}

	ipa_eth_dev_log(eth_dev, "Received PCIe event %d", notify->event);

	switch (notify->event) {
	case MSM_PCIE_EVENT_WAKEUP:
		/* Just set the flag here. ipa_eth_pm_notifier_cb() will later
		 * schedule global refresh.
		 */
		if (eth_dev->start_on_wakeup)
			eth_dev->start = true;
		break;
	default:
		break;
	}
}

static int ipa_eth_pci_probe_handler(struct pci_dev *pdev,
				     const struct pci_device_id *id)
{
	int rc = 0;
	struct device *dev = &pdev->dev;
	struct ipa_eth_device *eth_dev;
	struct ipa_eth_pci_device *epci_dev;
	struct ipa_eth_pci_driver *epci_drv;

	ipa_eth_dbg("PCI probe called for %s driver with devfn %u",
		    pdev->driver->name, pdev->devfn);

	if (!ipa_eth_pci_is_ready) {
		ipa_eth_err("Offload sub-system PCI module is not initialized");
		ipa_eth_err("PCI probe for device is deferred");
		return -EPROBE_DEFER;
	}

	epci_drv = lookup_epci_driver(pdev->driver);
	if (!epci_drv) {
		ipa_eth_bug("Failed to lookup epci driver");
		return -EFAULT;
	}

	rc = epci_drv->probe_real(pdev, id);
	if (rc) {
		ipa_eth_err("Failed real PCI probe of devfn=%u");
		goto err_probe;
	}

	eth_dev = devm_kzalloc(dev, sizeof(*eth_dev), GFP_KERNEL);
	if (!eth_dev) {
		rc = -ENOMEM;
		goto err_alloc_edev;
	}

	eth_dev->dev = dev;
	eth_dev->nd = epci_drv->nd;

	epci_dev = devm_kzalloc(dev, sizeof(*epci_dev), GFP_KERNEL);
	if (!epci_dev) {
		rc = -ENOMEM;
		goto err_alloc_epdev;
	}

	eth_dev->bus_priv = epci_dev;

	epci_dev->eth_dev = eth_dev;
	epci_dev->epci_drv = epci_drv;

	epci_dev->pcie_event.events = MSM_PCIE_EVENT_WAKEUP;
	epci_dev->pcie_event.user = pdev;
	epci_dev->pcie_event.mode = MSM_PCIE_TRIGGER_CALLBACK;
	epci_dev->pcie_event.callback = ipa_eth_pcie_event_cb;

	rc = msm_pcie_register_event(&epci_dev->pcie_event);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to register for PCIe event");
		goto err_register_pcie;
	}

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
	msm_pcie_deregister_event(&epci_dev->pcie_event);
err_register_pcie:
	devm_kfree(dev, epci_dev);
err_alloc_epdev:
	devm_kfree(dev, eth_dev);
err_alloc_edev:
	epci_drv->remove_real(pdev);
err_probe:
	return rc;
}

static void ipa_eth_pci_remove_handler(struct pci_dev *pdev)
{
	struct device *dev = &pdev->dev;
	struct ipa_eth_device *eth_dev = NULL;
	struct ipa_eth_pci_device *epci_dev = NULL;

	ipa_eth_dbg("PCI remove called for %s driver with devfn %u",
		    pdev->driver->name, pdev->devfn);

	eth_dev = lookup_eth_dev(pdev);
	if (!eth_dev) {
		ipa_eth_bug("Failed to lookup pci_dev -> eth_dev");
		return;
	}

	mutex_lock(&pci_devices_mutex);
	list_del(&eth_dev->bus_device_list);
	mutex_unlock(&pci_devices_mutex);

	epci_dev = eth_dev->bus_priv;

	ipa_eth_unregister_device(eth_dev);
	msm_pcie_deregister_event(&epci_dev->pcie_event);

	epci_dev->epci_drv->remove_real(pdev);

	memset(epci_dev, 0, sizeof(*epci_dev));
	devm_kfree(dev, epci_dev);

	memset(eth_dev, 0, sizeof(*eth_dev));
	devm_kfree(dev, eth_dev);
}

static int ipa_eth_pci_suspend_handler(struct device *dev)
{
	int rc = 0;
	struct ipa_eth_device *eth_dev;
	struct pci_dev *pci_dev = to_pci_dev(dev);

	eth_dev = lookup_eth_dev(pci_dev);
	if (!eth_dev) {
		ipa_eth_bug("Failed to lookup pci_dev -> eth_dev");
		return -EFAULT;
	}

	if (work_pending(&eth_dev->refresh))
		return -EAGAIN;

	/* When offload is started, PCI power collapse is already disabled by
	 * the ipa_eth_pci_disable_pc() api. Nonetheless, we still need to do
	 * a dummy PCI config space save so that the PCIe framework will not by
	 * itself perform a config space save-restore.
	 */
	if (eth_dev->of_state == IPA_ETH_OF_ST_STARTED) {
		ipa_eth_dev_log(eth_dev,
			"Device suspend performing dummy config space save");
		rc = pci_save_state(pci_dev);
	} else {
		ipa_eth_dev_log(eth_dev,
			"Device suspend delegated to net driver");
		rc = eth_dev_pm_ops(eth_dev)->suspend(dev);
	}

	if (rc)
		ipa_eth_dev_log(eth_dev, "Device suspend failed");
	else
		ipa_eth_dev_log(eth_dev, "Device suspend complete");

	return rc;
}

static int ipa_eth_pci_resume_handler(struct device *dev)
{
	int rc = 0;
	struct ipa_eth_device *eth_dev;
	struct pci_dev *pci_dev = to_pci_dev(dev);

	eth_dev = lookup_eth_dev(pci_dev);
	if (!eth_dev) {
		ipa_eth_bug("Failed to lookup pci_dev -> eth_dev");
		return -EFAULT;
	}

	/* Just set the flag here. ipa_eth_pm_notifier_cb() will later schedule
	 * global refresh.
	 */
	if (eth_dev->start_on_resume)
		eth_dev->start = true;

	/* During suspend, RC power collapse would not have happened if offload
	 * was started. Ignore resume callback since the device does not need
	 * to be re-initialized.
	 */
	if (eth_dev->of_state == IPA_ETH_OF_ST_STARTED) {
		ipa_eth_dev_log(eth_dev,
			"Device resume performing nop");
		rc = 0;
	} else {
		ipa_eth_dev_log(eth_dev,
			"Device resume delegated to net driver");
		rc = eth_dev_pm_ops(eth_dev)->resume(dev);
	}

	if (rc)
		ipa_eth_dev_log(eth_dev, "Device resume failed");
	else
		ipa_eth_dev_log(eth_dev, "Device resume complete");

	return 0;
}

/* MSM PCIe driver invokes only suspend and resume callbacks, other operations
 * can be ignored unless we see a client requiring the feature.
 */
static const struct dev_pm_ops ipa_eth_pci_pm_ops = {
	.suspend = ipa_eth_pci_suspend_handler,
	.resume = ipa_eth_pci_resume_handler,
};

static int ipa_eth_pci_register_net_driver(struct ipa_eth_net_driver *nd)
{
	struct pci_driver *pci_drv;
	struct ipa_eth_pci_driver *epci_drv;

	if (!nd) {
		ipa_eth_err("Network driver is NULL");
		return -EINVAL;
	}

	pci_drv = to_pci_driver(nd->driver);

	if (WARN_ON(!pci_drv->probe || !pci_drv->remove)) {
		ipa_eth_err("PCI net driver %s lacking probe/remove callbacks",
			nd->name);
		return -EFAULT;
	}

	if (!pci_drv->driver.pm || !pci_drv->driver.pm->suspend ||
			!pci_drv->driver.pm->resume) {
		ipa_eth_err("PCI net driver %s does not support PM ops",
			nd->name);
		return -EFAULT;
	}

	epci_drv = kzalloc(sizeof(*epci_drv), GFP_KERNEL);
	if (!epci_drv)
		return -ENOMEM;

	epci_drv->probe_real = pci_drv->probe;
	pci_drv->probe = ipa_eth_pci_probe_handler;

	epci_drv->remove_real = pci_drv->remove;
	pci_drv->remove = ipa_eth_pci_remove_handler;

	epci_drv->pm_ops_real = pci_drv->driver.pm;
	pci_drv->driver.pm = &ipa_eth_pci_pm_ops;

	epci_drv->nd = nd;

	mutex_lock(&pci_drivers_mutex);
	list_add(&epci_drv->driver_list, &pci_drivers);
	mutex_unlock(&pci_drivers_mutex);

	return 0;

}

static void ipa_eth_pci_unregister_net_driver(struct ipa_eth_net_driver *nd)
{
	struct pci_driver *pci_drv = to_pci_driver(nd->driver);
	struct ipa_eth_pci_driver *epci_drv = lookup_epci_driver(pci_drv);

	if (!epci_drv) {
		ipa_eth_bug("Failed to lookup epci driver");
		return;
	}

	mutex_lock(&pci_drivers_mutex);
	list_del(&epci_drv->driver_list);
	mutex_unlock(&pci_drivers_mutex);

	pci_drv->probe = epci_drv->probe_real;
	pci_drv->remove = epci_drv->remove_real;

	pci_drv->driver.pm = epci_drv->pm_ops_real;

	memset(epci_drv, 0, sizeof(*epci_drv));
	kfree(epci_drv);
}

/**
 * ipa_eth_pci_enable_pc() - Permit power collapse of the PCI root port
 * @eth_dev: Device attached to the PCI bus
 *
 * This function instructs the MSM PCIe bus driver to permit power collapse
 * of the root complex when Linux goes to suspend state.
 *
 * Return: 0 on success, non-zero otherwise
 */
static int ipa_eth_pci_enable_pc(struct ipa_eth_device *eth_dev)
{
	int rc;
	struct pci_dev *pci_dev = to_pci_dev(eth_dev->dev);

	rc = msm_pcie_pm_control(MSM_PCIE_ENABLE_PC,
		pci_dev->bus->number, pci_dev, NULL, MSM_PCIE_CONFIG_INVALID);
	if (rc) {
		ipa_eth_dev_err(eth_dev,
			"Failed to enable MSM PCIe power collapse");
	} else {
		ipa_eth_dev_log(eth_dev,
			"Enabled MSM PCIe power collapse");
	}

	return rc;
}

/**
 * ipa_eth_pci_disable_pc() - Prevent power collapse of the PCI root port
 * @eth_dev: Device attached to the PCI bus
 *
 * This function instructs the MSM PCIe bus driver to prevent power collapse
 * of the root complex and connected EPs when Linux goes to suspend state.
 *
 * Return: 0 on success, non-zero otherwise
 */
static int ipa_eth_pci_disable_pc(struct ipa_eth_device *eth_dev)
{
	int rc;
	struct pci_dev *pci_dev = to_pci_dev(eth_dev->dev);

	rc = msm_pcie_pm_control(MSM_PCIE_DISABLE_PC,
		pci_dev->bus->number, pci_dev, NULL, MSM_PCIE_CONFIG_INVALID);
	if (rc) {
		ipa_eth_dev_err(eth_dev,
			"Failed to disable MSM PCIe power collapse");
	} else {
		ipa_eth_dev_log(eth_dev,
			"Disabled MSM PCIe power collapse");
	}

	return rc;
}

struct ipa_eth_bus ipa_eth_pci_bus = {
	.bus = &pci_bus_type,
	.register_net_driver = ipa_eth_pci_register_net_driver,
	.unregister_net_driver = ipa_eth_pci_unregister_net_driver,
	.enable_pc = ipa_eth_pci_enable_pc,
	.disable_pc = ipa_eth_pci_disable_pc,
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

	ipa_eth_log("Offload sub-system pci bus module init is complete");

	return 0;
}

void ipa_eth_pci_modexit(void)
{
	ipa_eth_log("De-initing offload sub-system pci bus module");

	if (!ipa_eth_pci_is_ready)
		return;

	ipa_eth_pci_is_ready = false;

	ipa_eth_pci_debugfs_cleanup();
}
