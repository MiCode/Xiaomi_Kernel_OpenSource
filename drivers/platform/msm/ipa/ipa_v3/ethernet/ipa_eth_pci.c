/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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

#include <linux/pci.h>
#include <linux/msm_pcie.h>

#include "ipa_eth_i.h"

struct ipa_eth_pci_driver {
	struct ipa_eth_net_driver *nd;

	int (*probe_real)(struct pci_dev *dev, const struct pci_device_id *id);
	void (*remove_real)(struct pci_dev *dev);

	const struct dev_pm_ops *pm_ops_real;
};

struct ipa_eth_pci_device {
	struct ipa_eth_device *eth_dev;
	struct ipa_eth_pci_driver *eth_pdrv;
	struct msm_pcie_register_event pcie_event;
};

#define eth_dev_to_pdev(e) \
	((struct ipa_eth_pci_device *)e->bus_priv)

#define eth_dev_pm_ops(e) \
	(eth_dev_to_pdev(e)->eth_pdrv->pm_ops_real)

static LIST_HEAD(ipa_eth_pci_drivers);
static DEFINE_MUTEX(ipa_eth_pci_drivers_mutex);

static LIST_HEAD(ipa_eth_pci_devices);
static DEFINE_MUTEX(ipa_eth_pci_devices_mutex);

static bool ipa_eth_pci_is_ready;

static struct ipa_eth_pci_driver *__lookup_eth_pdrv(
					struct pci_driver *pdrv)
{
	struct ipa_eth_net_driver *nd;

	list_for_each_entry(nd, &ipa_eth_pci_drivers, bus_driver_list) {
		if (nd->driver == &pdrv->driver)
			return nd->bus_priv;
	}

	return NULL;
}

static struct ipa_eth_pci_driver *lookup_eth_pdrv(struct pci_driver *pdrv)
{
	struct ipa_eth_pci_driver *eth_pdrv;

	mutex_lock(&ipa_eth_pci_drivers_mutex);
	eth_pdrv = __lookup_eth_pdrv(pdrv);
	mutex_unlock(&ipa_eth_pci_drivers_mutex);

	return eth_pdrv;
}

static struct ipa_eth_device *__lookup_eth_dev(struct pci_dev *pdev)
{
	struct device *dev = &pdev->dev;
	struct ipa_eth_device *eth_dev;

	list_for_each_entry(eth_dev, &ipa_eth_pci_devices, bus_device_list) {
		if (eth_dev->dev == dev)
			return eth_dev;
	}

	return NULL;
}

static struct ipa_eth_device *lookup_eth_dev(struct pci_dev *pdev)
{
	struct ipa_eth_device *eth_dev;

	mutex_lock(&ipa_eth_pci_devices_mutex);
	eth_dev = __lookup_eth_dev(pdev);
	mutex_unlock(&ipa_eth_pci_devices_mutex);

	return eth_dev;
}

static struct ipa_eth_device *dev_to_eth_dev(struct device *dev)
{
	return lookup_eth_dev(to_pci_dev(dev));
}

static bool is_driver_used(struct ipa_eth_pci_driver *eth_pdrv)
{
	bool in_use = false;
	struct ipa_eth_device *eth_dev;

	list_for_each_entry(eth_dev, &ipa_eth_pci_devices, bus_device_list) {
		if (eth_dev_to_pdev(eth_dev)->eth_pdrv == eth_pdrv) {
			in_use = true;
			break;
		}
	}

	return in_use;
}

static void ipa_eth_pcie_event_wakeup(struct pci_dev *pdev)
{
	struct ipa_eth_device *eth_dev;

	/* We are not expected to be re-entrant while processing wake up
	 * event.
	 */
	eth_dev = __lookup_eth_dev(pdev);
	if (!eth_dev) {
		ipa_eth_bug("Failed to lookup eth device");
		return;
	}

	if (eth_dev->start_on_wakeup && !eth_dev->start) {
		eth_dev->start = true;
		ipa_eth_device_refresh_sched(eth_dev);
	}
}

static void ipa_eth_pcie_event_cb(struct msm_pcie_notify *notify)
{
	struct pci_dev *pdev = notify->user;

	ipa_eth_log("Received PCIe event %d", notify->event);

	switch (notify->event) {
	case MSM_PCIE_EVENT_WAKEUP:
		ipa_eth_pcie_event_wakeup(pdev);
		break;
	default:
		break;
	}
}

static struct ipa_eth_device *ipa_eth_pci_alloc_device(
	struct pci_dev *pdev,
	struct ipa_eth_pci_driver *eth_pdrv)
{
	struct device *dev = &pdev->dev;
	struct ipa_eth_device *eth_dev;
	struct ipa_eth_pci_device *eth_pdev;

	eth_pdev = devm_kzalloc(dev, sizeof(*eth_pdev), GFP_KERNEL);
	if (!eth_pdev)
		return NULL;

	eth_dev = ipa_eth_alloc_device(dev, eth_pdrv->nd);
	if (!eth_dev) {
		ipa_eth_err("Failed to alloc eth device");
		devm_kfree(dev, eth_pdev);
		return NULL;
	}

	eth_pdev->eth_dev = eth_dev;
	eth_pdev->eth_pdrv = eth_pdrv;

	eth_pdev->pcie_event.events = MSM_PCIE_EVENT_WAKEUP;
	eth_pdev->pcie_event.user = pdev;
	eth_pdev->pcie_event.mode = MSM_PCIE_TRIGGER_CALLBACK;
	eth_pdev->pcie_event.callback = ipa_eth_pcie_event_cb;

	eth_dev->bus_priv = eth_pdev;

	return eth_dev;
}

static void ipa_eth_pci_free_device(struct ipa_eth_device *eth_dev)
{
	struct device *dev = eth_dev->dev;
	struct ipa_eth_pci_device *eth_pdev = eth_dev->bus_priv;

	if (eth_pdev) {
		memset(eth_pdev, 0, sizeof(*eth_pdev));
		devm_kfree(dev, eth_pdev);
	}

	ipa_eth_free_device(eth_dev);
}

static int ipa_eth_pci_register_device(
	struct pci_dev *pdev,
	struct ipa_eth_pci_driver *eth_pdrv)
{
	int rc;
	struct ipa_eth_device *eth_dev;

	eth_dev = ipa_eth_pci_alloc_device(pdev, eth_pdrv);
	if (!eth_dev) {
		rc = -ENOMEM;
		ipa_eth_err("Failed to alloc eth pci device");
		goto err_alloc;
	}

	rc = ipa_eth_register_device(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to register eth device");
		goto err_register;
	}

	mutex_lock(&ipa_eth_pci_devices_mutex);
	list_add(&eth_dev->bus_device_list, &ipa_eth_pci_devices);
	mutex_unlock(&ipa_eth_pci_devices_mutex);

	rc = msm_pcie_register_event(&eth_dev_to_pdev(eth_dev)->pcie_event);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to register for PCIe event");
		goto err_pcie_register;
	}

	return 0;

err_pcie_register:
	ipa_eth_unregister_device(eth_dev);
err_register:
	ipa_eth_pci_free_device(eth_dev);
err_alloc:
	return rc;
}

static void ipa_eth_pci_unregister_device(struct ipa_eth_device *eth_dev)
{
	/* Deregister event handler first to prevent possible race with
	 * __lookup_eth_dev() while remove the device from devices list.
	 */
	msm_pcie_deregister_event(&eth_dev_to_pdev(eth_dev)->pcie_event);

	mutex_lock(&ipa_eth_pci_devices_mutex);
	list_del(&eth_dev->bus_device_list);
	mutex_unlock(&ipa_eth_pci_devices_mutex);

	ipa_eth_unregister_device(eth_dev);
	ipa_eth_pci_free_device(eth_dev);
}

static int ipa_eth_pci_probe_handler(struct pci_dev *pdev,
				     const struct pci_device_id *id)
{
	int rc = 0;
	struct ipa_eth_pci_driver *eth_pdrv;

	ipa_eth_dbg("PCI probe called for %s driver with devfn %u",
		    pdev->driver->name, pdev->devfn);

	if (!ipa_eth_is_ready()) {
		ipa_eth_log(
			"Offload sub-system not initialized, deferring probe");
		return -EPROBE_DEFER;
	}

	eth_pdrv = lookup_eth_pdrv(pdev->driver);
	if (!eth_pdrv) {
		ipa_eth_bug("Failed to lookup epci driver");
		return -EFAULT;
	}

	rc = eth_pdrv->probe_real(pdev, id);
	if (rc) {
		ipa_eth_err("Failed real PCI probe of devfn=%u");
		goto err_probe;
	}

	rc = ipa_eth_pci_register_device(pdev, eth_pdrv);
	if (rc) {
		ipa_eth_err("Failed to register PCI eth device");
		goto err_register;
	}

	return 0;

err_register:
	eth_pdrv->remove_real(pdev);
err_probe:
	return rc;
}

static void ipa_eth_pci_remove_handler(struct pci_dev *pdev)
{
	struct ipa_eth_device *eth_dev = NULL;
	struct ipa_eth_pci_driver *eth_pdrv = NULL;

	ipa_eth_dbg("PCI remove called for %s driver with devfn %u",
		    pdev->driver->name, pdev->devfn);

	eth_dev = lookup_eth_dev(pdev);
	if (!eth_dev) {
		ipa_eth_bug("Failed to lookup pci_dev -> eth_dev");
		return;
	}

	eth_pdrv = eth_dev_to_pdev(eth_dev)->eth_pdrv;

	ipa_eth_pci_unregister_device(eth_dev);

	eth_pdrv->remove_real(pdev);
}

static int ipa_eth_pci_suspend_handler(struct device *dev)
{
	int rc = 0;
	struct ipa_eth_device *eth_dev;

	eth_dev = dev_to_eth_dev(dev);
	if (!eth_dev) {
		ipa_eth_bug("Failed to lookup pci_dev -> eth_dev");
		return -EFAULT;
	}

	if (work_pending(&eth_dev->refresh)) {
		ipa_eth_dev_log(eth_dev,
			"Refresh work is pending, aborting suspend");

		/* Just abort suspend. Since the wq is freezable, the work item
		 * would get flushed before we get called again.
		 */
		return -EAGAIN;
	}

	/* When offload is started, PCI power collapse is already disabled by
	 * the ipa_eth_pci_disable_pc() api. Nonetheless, we still need to do
	 * a dummy PCI config space save so that the PCIe framework will not by
	 * itself perform a config space save-restore.
	 */
	if (eth_dev->of_state == IPA_ETH_OF_ST_STARTED) {
		ipa_eth_dev_dbg(eth_dev,
			"Device suspend performing dummy config space save");
		rc = pci_save_state(to_pci_dev(dev));
	} else {
		ipa_eth_dev_log(eth_dev,
			"Device suspend delegated to net driver");
		rc = eth_dev_pm_ops(eth_dev)->suspend(dev);
	}

	if (rc)
		ipa_eth_dev_err(eth_dev, "Device suspend failed");
	else
		ipa_eth_dev_dbg(eth_dev, "Device suspend complete");

	return rc;
}

static int ipa_eth_pci_suspend_late_handler(struct device *dev)
{
	struct ipa_eth_device *eth_dev;

	eth_dev = dev_to_eth_dev(dev);
	if (!eth_dev) {
		ipa_eth_bug("Failed to lookup pci_dev -> eth_dev");
		return -EFAULT;
	}

	/* In rare case where we detect some interface activity between the
	 * time PM_SUSPEND_PREPARE event was processed and the device was
	 * actually frozen, abort the suspend operation.
	 */
	if (ipa_eth_net_check_active(eth_dev)) {
		pr_info("%s: %s shows late stage activity, preventing suspend",
				IPA_ETH_SUBSYS, eth_dev->net_dev->name);

		/* Have PM_SUSPEND_PREPARE give us one wakeup time quanta */
		eth_dev_priv(eth_dev)->assume_active++;

		return -EAGAIN;
	}

	return 0;
}

static int ipa_eth_pci_resume_early_handler(struct device *dev)
{
	struct ipa_eth_device *eth_dev;

	eth_dev = dev_to_eth_dev(dev);
	if (!eth_dev) {
		ipa_eth_bug("Failed to lookup pci_dev -> eth_dev");
		return -EFAULT;
	}

	/* We cannot check start_on_resume in the resume handler as it can get
	 * invoked also if .suspend_late() aborts due to interface activity.
	 */
	if (eth_dev->start_on_resume && !eth_dev->start) {
		eth_dev->start = true;
		ipa_eth_device_refresh_sched(eth_dev);
	}

	return 0;
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

	/* During suspend, RC power collapse would not have happened if offload
	 * was started. Ignore resume callback since the device does not need
	 * to be re-initialized.
	 */
	if (eth_dev->of_state == IPA_ETH_OF_ST_STARTED) {
		ipa_eth_dev_dbg(eth_dev,
			"Device resume performing nop");
		rc = 0;
	} else {
		ipa_eth_dev_log(eth_dev,
			"Device resume delegated to net driver");
		rc = eth_dev_pm_ops(eth_dev)->resume(dev);

		/* Give some time after a resume for the device to settle */
		eth_dev_priv(eth_dev)->assume_active++;
	}

	if (rc)
		ipa_eth_dev_err(eth_dev, "Device resume failed");
	else
		ipa_eth_dev_dbg(eth_dev, "Device resume complete");

	return 0;
}

/* MSM PCIe driver invokes only suspend and resume callbacks, other operations
 * can be ignored unless we see a client requiring the feature.
 */
static const struct dev_pm_ops ipa_eth_pci_pm_ops = {
	.suspend = ipa_eth_pci_suspend_handler,
	.suspend_late = ipa_eth_pci_suspend_late_handler,
	.resume_early = ipa_eth_pci_resume_early_handler,
	.resume = ipa_eth_pci_resume_handler,
};

static bool ipa_eth_pci_check_driver(struct pci_driver *pdrv)
{
	return
		pdrv->name &&
		pdrv->probe &&
		pdrv->remove &&
		pdrv->driver.pm &&
		pdrv->driver.pm->suspend &&
		pdrv->driver.pm->resume;
}

static struct ipa_eth_pci_driver *ipa_eth_pci_setup_driver(
	struct pci_driver *pdrv,
	struct ipa_eth_net_driver *nd)
{
	struct ipa_eth_pci_driver *eth_pdrv;

	eth_pdrv = kzalloc(sizeof(*eth_pdrv), GFP_KERNEL);
	if (!eth_pdrv)
		return NULL;

	eth_pdrv->probe_real = pdrv->probe;
	pdrv->probe = ipa_eth_pci_probe_handler;

	eth_pdrv->remove_real = pdrv->remove;
	pdrv->remove = ipa_eth_pci_remove_handler;

	eth_pdrv->pm_ops_real = pdrv->driver.pm;
	pdrv->driver.pm = &ipa_eth_pci_pm_ops;

	eth_pdrv->nd = nd;
	nd->bus_priv = eth_pdrv;

	return eth_pdrv;
}

static void ipa_eth_pci_reset_driver(struct ipa_eth_net_driver *nd)
{
	struct pci_driver *pdrv = to_pci_driver(nd->driver);
	struct ipa_eth_pci_driver *eth_pdrv = nd->bus_priv;

	if (is_driver_used(eth_pdrv))
		ipa_eth_bug("Driver is still being used by a device");

	nd->bus_priv = NULL;

	pdrv->probe = eth_pdrv->probe_real;
	pdrv->remove = eth_pdrv->remove_real;
	pdrv->driver.pm = eth_pdrv->pm_ops_real;

	memset(eth_pdrv, 0, sizeof(*eth_pdrv));
	kfree(eth_pdrv);
}

static int ipa_eth_pci_register_driver(struct ipa_eth_net_driver *nd)
{
	struct pci_driver *pdrv;
	struct ipa_eth_pci_driver *eth_pdrv;

	pdrv = to_pci_driver(nd->driver);

	if (!ipa_eth_pci_check_driver(pdrv)) {
		ipa_eth_err("PCI driver validation failed for %s", nd->name);
		return -EINVAL;
	}

	eth_pdrv = ipa_eth_pci_setup_driver(pdrv, nd);
	if (!eth_pdrv)
		return -ENOMEM;

	mutex_lock(&ipa_eth_pci_drivers_mutex);
	list_add(&nd->bus_driver_list, &ipa_eth_pci_drivers);
	mutex_unlock(&ipa_eth_pci_drivers_mutex);

	return 0;
}

static void ipa_eth_pci_unregister_driver(struct ipa_eth_net_driver *nd)
{

	mutex_lock(&ipa_eth_pci_drivers_mutex);
	list_del(&nd->bus_driver_list);
	mutex_unlock(&ipa_eth_pci_drivers_mutex);

	ipa_eth_pci_reset_driver(nd);
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
	.register_driver = ipa_eth_pci_register_driver,
	.unregister_driver = ipa_eth_pci_unregister_driver,
	.enable_pc = ipa_eth_pci_enable_pc,
	.disable_pc = ipa_eth_pci_disable_pc,
};

int ipa_eth_pci_modinit(void)
{
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
}
