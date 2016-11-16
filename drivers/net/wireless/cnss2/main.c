/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_wakeup.h>
#include <linux/rwsem.h>
#include <linux/suspend.h>
#include <soc/qcom/ramdump.h>
#include <soc/qcom/subsystem_notif.h>

#include "debug.h"
#include "main.h"
#include "pci.h"

#define CNSS_DUMP_FORMAT_VER		0x11
#define CNSS_DUMP_MAGIC_VER_V2		0x42445953
#define CNSS_DUMP_NAME			"CNSS_WLAN"
#define WLAN_RECOVERY_DELAY		1000

static struct cnss_plat_data *plat_env;

static DECLARE_RWSEM(cnss_pm_sem);

static struct cnss_fw_files FW_FILES_QCA6174_FW_3_0 = {
	"qwlan30.bin", "bdwlan30.bin", "otp30.bin", "utf30.bin",
	"utfbd30.bin", "epping30.bin", "evicted30.bin"
};

static struct cnss_fw_files FW_FILES_DEFAULT = {
	"qwlan.bin", "bdwlan.bin", "otp.bin", "utf.bin",
	"utfbd.bin", "epping.bin", "evicted.bin"
};

static enum cnss_dev_bus_type cnss_get_dev_bus_type(struct device *dev)
{
	if (!dev)
		return CNSS_BUS_NONE;

	if (!dev->bus)
		return CNSS_BUS_NONE;

	if (memcmp(dev->bus->name, "pci", 3) == 0)
		return CNSS_BUS_PCI;
	else
		return CNSS_BUS_NONE;
}

static void cnss_set_plat_priv(struct platform_device *plat_dev,
			       struct cnss_plat_data *plat_priv)
{
	plat_env = plat_priv;
}

static struct cnss_plat_data *cnss_get_plat_priv(struct platform_device
						 *plat_dev)
{
	return plat_env;
}

void *cnss_bus_dev_to_bus_priv(struct device *dev)
{
	if (!dev)
		return NULL;

	switch (cnss_get_dev_bus_type(dev)) {
	case CNSS_BUS_PCI:
		return cnss_get_pci_priv(to_pci_dev(dev));
	default:
		return NULL;
	}
}

struct cnss_plat_data *cnss_bus_dev_to_plat_priv(struct device *dev)
{
	void *bus_priv;

	if (!dev)
		return cnss_get_plat_priv(NULL);

	bus_priv = cnss_bus_dev_to_bus_priv(dev);
	if (!bus_priv)
		return NULL;

	switch (cnss_get_dev_bus_type(dev)) {
	case CNSS_BUS_PCI:
		return cnss_pci_priv_to_plat_priv(bus_priv);
	default:
		return NULL;
	}
}

static int cnss_pm_notify(struct notifier_block *b,
			  unsigned long event, void *p)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		down_write(&cnss_pm_sem);
		break;
	case PM_POST_SUSPEND:
		up_write(&cnss_pm_sem);
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block cnss_pm_notifier = {
	.notifier_call = cnss_pm_notify,
};

void cnss_lock_pm_sem(void)
{
	down_read(&cnss_pm_sem);
}
EXPORT_SYMBOL(cnss_lock_pm_sem);

void cnss_release_pm_sem(void)
{
	up_read(&cnss_pm_sem);
}
EXPORT_SYMBOL(cnss_release_pm_sem);

int cnss_get_fw_files_for_target(struct cnss_fw_files *pfw_files,
				 u32 target_type, u32 target_version)
{
	if (!pfw_files)
		return -ENODEV;

	switch (target_version) {
	case QCA6174_REV3_VERSION:
	case QCA6174_REV3_2_VERSION:
		memcpy(pfw_files, &FW_FILES_QCA6174_FW_3_0, sizeof(*pfw_files));
		break;
	default:
		memcpy(pfw_files, &FW_FILES_DEFAULT, sizeof(*pfw_files));
		cnss_pr_err("Unknown target version, type: 0x%X, version: 0x%X",
			    target_type, target_version);
		break;
	}

	return 0;
}
EXPORT_SYMBOL(cnss_get_fw_files_for_target);

int cnss_request_bus_bandwidth(int bandwidth)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);
	struct cnss_bus_bw_info *bus_bw_info;

	if (!plat_priv)
		return -ENODEV;

	bus_bw_info = &plat_priv->bus_bw_info;
	if (!bus_bw_info->bus_client)
		return -ENOSYS;

	switch (bandwidth) {
	case CNSS_BUS_WIDTH_NONE:
	case CNSS_BUS_WIDTH_LOW:
	case CNSS_BUS_WIDTH_MEDIUM:
	case CNSS_BUS_WIDTH_HIGH:
		ret = msm_bus_scale_client_update_request(
			bus_bw_info->bus_client, bandwidth);
		if (!ret)
			bus_bw_info->current_bw_vote = bandwidth;
		else
			cnss_pr_err("Could not set bus bandwidth: %d, err = %d\n",
				    bandwidth, ret);
		break;
	default:
		cnss_pr_err("Invalid bus bandwidth: %d", bandwidth);
		ret = -EINVAL;
	}

	return ret;
}
EXPORT_SYMBOL(cnss_request_bus_bandwidth);

int cnss_get_platform_cap(struct cnss_platform_cap *cap)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);

	if (!plat_priv)
		return -ENODEV;

	if (cap)
		*cap = plat_priv->cap;

	return 0;
}
EXPORT_SYMBOL(cnss_get_platform_cap);

void cnss_set_driver_status(enum cnss_driver_status driver_status)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);

	if (!plat_priv)
		return;

	plat_priv->driver_status = driver_status;
}
EXPORT_SYMBOL(cnss_set_driver_status);

void cnss_request_pm_qos(u32 qos_val)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);

	if (!plat_priv)
		return;

	pm_qos_add_request(&plat_priv->qos_request, PM_QOS_CPU_DMA_LATENCY,
			   qos_val);
}
EXPORT_SYMBOL(cnss_request_pm_qos);

void cnss_remove_pm_qos(void)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);

	if (!plat_priv)
		return;

	pm_qos_remove_request(&plat_priv->qos_request);
}
EXPORT_SYMBOL(cnss_remove_pm_qos);

u8 *cnss_common_get_wlan_mac_address(struct device *dev, uint32_t *num)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct cnss_wlan_mac_info *wlan_mac_info;
	struct cnss_wlan_mac_addr *addr;

	if (!plat_priv)
		goto out;

	wlan_mac_info = &plat_priv->wlan_mac_info;
	if (!wlan_mac_info->is_wlan_mac_set) {
		cnss_pr_info("Platform driver doesn't have any MAC address!\n");
		goto out;
	}

	addr = &wlan_mac_info->wlan_mac_addr;
	*num = addr->no_of_mac_addr_set;

	return &addr->mac_addr[0][0];
out:
	*num = 0;
	return NULL;
}
EXPORT_SYMBOL(cnss_common_get_wlan_mac_address);

int cnss_power_up(struct device *dev)
{
	int ret = 0;
	void *bus_priv = cnss_bus_dev_to_bus_priv(dev);
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	ret = cnss_power_on_device(plat_priv);
	if (ret) {
		cnss_pr_err("Failed to power on device, err = %d\n", ret);
		goto err_power_on;
	}

	ret = cnss_resume_pci_link(bus_priv);
	if (ret) {
		cnss_pr_err("Failed to resume PCI link, err = %d\n", ret);
		goto err_resume_link;
	}

	return 0;
err_resume_link:
	cnss_power_off_device(plat_priv);
err_power_on:
	return ret;
}
EXPORT_SYMBOL(cnss_power_up);

int cnss_power_down(struct device *dev)
{
	int ret = 0;
	void *bus_priv = cnss_bus_dev_to_bus_priv(dev);
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!bus_priv || !plat_priv)
		return -ENODEV;

	cnss_request_bus_bandwidth(CNSS_BUS_WIDTH_NONE);
	cnss_pci_set_monitor_wake_intr(bus_priv, false);
	cnss_pci_set_auto_suspended(bus_priv, 0);

	ret = cnss_suspend_pci_link(bus_priv);
	if (ret)
		cnss_pr_err("Failed to suspend PCI link, err = %d\n", ret);

	cnss_power_off_device(plat_priv);

	return 0;
}
EXPORT_SYMBOL(cnss_power_down);

int cnss_wlan_register_driver(struct cnss_wlan_driver *driver_ops)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = cnss_get_plat_priv(NULL);
	struct cnss_subsys_info *subsys_info;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		ret = -ENODEV;
		goto out;
	}

	if (plat_priv->driver_ops) {
		cnss_pr_err("Driver has already registered!\n");
		ret = -EEXIST;
		goto out;
	}

	plat_priv->driver_status = CNSS_LOAD_UNLOAD;
	plat_priv->driver_ops = driver_ops;
	subsys_info = &plat_priv->subsys_info;

	subsys_info->subsys_handle =
		subsystem_get(subsys_info->subsys_desc.name);
	if (!subsys_info->subsys_handle) {
		ret = -EINVAL;
		goto reset_ctx;
	} else if (IS_ERR(subsys_info->subsys_handle)) {
		ret = PTR_ERR(subsys_info->subsys_handle);
		goto reset_ctx;
	}

	plat_priv->driver_status = CNSS_INITIALIZED;

	return 0;
reset_ctx:
	cnss_pr_err("Failed to get subsystem, err = %d\n", ret);
	plat_priv->driver_status = CNSS_UNINITIALIZED;
	plat_priv->driver_ops = NULL;
out:
	return ret;
}
EXPORT_SYMBOL(cnss_wlan_register_driver);

void cnss_wlan_unregister_driver(struct cnss_wlan_driver *driver_ops)
{
	struct cnss_plat_data *plat_priv = cnss_get_plat_priv(NULL);
	struct cnss_subsys_info *subsys_info;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return;
	}

	plat_priv->driver_status = CNSS_LOAD_UNLOAD;
	subsys_info = &plat_priv->subsys_info;
	subsystem_put(subsys_info->subsys_handle);
	subsys_info->subsys_handle = NULL;
	plat_priv->driver_ops = NULL;
	plat_priv->driver_status = CNSS_UNINITIALIZED;
}
EXPORT_SYMBOL(cnss_wlan_unregister_driver);

static int cnss_get_resources(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	ret = cnss_get_vreg(plat_priv);
	if (ret) {
		cnss_pr_err("Failed to get vreg, err = %d\n", ret);
		goto out;
	}

	ret = cnss_get_pinctrl(plat_priv);
	if (ret) {
		cnss_pr_err("Failed to get pinctrl, err = %d\n", ret);
		goto out;
	}

	return 0;
out:
	return ret;
}

static void cnss_put_resources(struct cnss_plat_data *plat_priv)
{
}

static int cnss_modem_notifier_nb(struct notifier_block *nb,
				  unsigned long code,
				  void *ss_handle)
{
	struct cnss_plat_data *plat_priv =
		container_of(nb, struct cnss_plat_data, modem_nb);
	struct cnss_pci_data *pci_priv = plat_priv->bus_priv;
	struct cnss_esoc_info *esoc_info;
	struct cnss_wlan_driver *driver_ops;

	cnss_pr_dbg("Modem notifier: event %lu\n", code);

	if (!pci_priv)
		return NOTIFY_DONE;

	esoc_info = &plat_priv->esoc_info;

	if (SUBSYS_AFTER_POWERUP == code)
		esoc_info->modem_current_status = 1;
	else if (SUBSYS_BEFORE_SHUTDOWN == code)
		esoc_info->modem_current_status = 0;
	else
		return NOTIFY_DONE;

	driver_ops = plat_priv->driver_ops;
	if (!driver_ops || !driver_ops->modem_status)
		return NOTIFY_DONE;

	driver_ops->modem_status(pci_priv->pci_dev,
				 esoc_info->modem_current_status);

	return NOTIFY_OK;
}

static int cnss_register_esoc(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct device *dev;
	struct cnss_esoc_info *esoc_info;
	struct esoc_desc *esoc_desc;
	const char *client_desc;

	dev = &plat_priv->plat_dev->dev;
	esoc_info = &plat_priv->esoc_info;

	esoc_info->notify_modem_status =
		of_property_read_bool(dev->of_node,
				      "qcom,notify-modem-status");

	if (esoc_info->notify_modem_status)
		goto out;

	ret = of_property_read_string_index(dev->of_node, "esoc-names", 0,
					    &client_desc);
	if (ret) {
		cnss_pr_dbg("esoc-names is not defined in DT, skip!\n");
	} else {
		esoc_desc = devm_register_esoc_client(dev, client_desc);
		if (IS_ERR_OR_NULL(esoc_desc)) {
			ret = PTR_RET(esoc_desc);
			cnss_pr_err("Failed to register esoc_desc, err = %d\n",
				    ret);
			goto out;
		}
		esoc_info->esoc_desc = esoc_desc;
	}

	plat_priv->modem_nb.notifier_call = cnss_modem_notifier_nb;
	esoc_info->modem_current_status = 0;
	esoc_info->modem_notify_handler =
		subsys_notif_register_notifier(esoc_info->esoc_desc ?
					       esoc_info->esoc_desc->name :
					       "modem", &plat_priv->modem_nb);
	if (IS_ERR(esoc_info->modem_notify_handler)) {
		ret = PTR_ERR(esoc_info->modem_notify_handler);
		cnss_pr_err("Failed to register esoc notifier, err = %d\n",
			    ret);
		goto unreg_esoc;
	}

	return 0;
unreg_esoc:
	if (esoc_info->esoc_desc)
		devm_unregister_esoc_client(dev, esoc_info->esoc_desc);
out:
	return ret;
}

static void cnss_unregister_esoc(struct cnss_plat_data *plat_priv)
{
	struct device *dev;
	struct cnss_esoc_info *esoc_info;

	dev = &plat_priv->plat_dev->dev;
	esoc_info = &plat_priv->esoc_info;

	if (esoc_info->notify_modem_status)
		subsys_notif_unregister_notifier(esoc_info->
						 modem_notify_handler,
						 &plat_priv->modem_nb);
	if (esoc_info->esoc_desc)
		devm_unregister_esoc_client(dev, esoc_info->esoc_desc);
}

static int cnss_qca6174_powerup(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct cnss_pci_data *pci_priv = plat_priv->bus_priv;

	if (!pci_priv)
		return -ENODEV;

	if (!plat_priv->driver_ops)
		return -EINVAL;

	ret = cnss_power_on_device(plat_priv);
	if (ret) {
		cnss_pr_err("Failed to power on device, err = %d\n", ret);
		goto out;
	}

	ret = cnss_resume_pci_link(pci_priv);
	if (ret) {
		cnss_pr_err("Failed to resume PCI link, err = %d\n", ret);
		goto power_off;
	}

	if (plat_priv->driver_status == CNSS_LOAD_UNLOAD) {
		ret = plat_priv->driver_ops->probe(pci_priv->pci_dev,
						   pci_priv->pci_device_id);
		if (ret) {
			cnss_pr_err("Failed to probe host driver, err = %d\n",
				    ret);
			goto suspend_link;
		}
	} else if (plat_priv->recovery_in_progress) {
		ret = plat_priv->driver_ops->reinit(pci_priv->pci_dev,
						    pci_priv->pci_device_id);
		if (ret) {
			cnss_pr_err("Failed to reinit host driver, err = %d\n",
				    ret);
			goto suspend_link;
		}
		plat_priv->recovery_in_progress = false;
	} else {
		cnss_pr_err("Driver state is not correct to power up!\n");
		ret = -EINVAL;
		goto suspend_link;
	}

	return 0;
suspend_link:
	cnss_suspend_pci_link(pci_priv);
power_off:
	cnss_power_off_device(plat_priv);
out:
	return ret;
}

static int cnss_qca6174_shutdown(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct cnss_pci_data *pci_priv = plat_priv->bus_priv;

	if (!pci_priv)
		return -ENODEV;

	if (!plat_priv->driver_ops)
		return -EINVAL;

	if (plat_priv->driver_status == CNSS_LOAD_UNLOAD) {
		cnss_request_bus_bandwidth(CNSS_BUS_WIDTH_NONE);
		plat_priv->driver_ops->remove(pci_priv->pci_dev);
		cnss_pci_set_monitor_wake_intr(pci_priv, false);
		cnss_pci_set_auto_suspended(pci_priv, 0);
	} else {
		plat_priv->recovery_in_progress = true;
		plat_priv->driver_ops->shutdown(pci_priv->pci_dev);
	}

	ret = cnss_suspend_pci_link(pci_priv);
	if (ret)
		cnss_pr_err("Failed to suspend PCI link, err = %d\n", ret);

	cnss_power_off_device(plat_priv);

	return ret;
}

static void cnss_qca6174_crash_shutdown(struct cnss_plat_data *plat_priv)
{
	struct cnss_pci_data *pci_priv = plat_priv->bus_priv;

	if (!plat_priv->driver_ops)
		return;

	plat_priv->driver_ops->crash_shutdown(pci_priv->pci_dev);
}

static int cnss_powerup(const struct subsys_desc *subsys_desc)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = dev_get_drvdata(subsys_desc->dev);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return -ENODEV;
	}

	switch (plat_priv->device_id) {
	case QCA6174_DEVICE_ID:
		ret = cnss_qca6174_powerup(plat_priv);
		break;
	case QCA6290_DEVICE_ID:
		break;
	default:
		cnss_pr_err("Unknown device_id found: 0x%lx\n",
			    plat_priv->device_id);
		ret = -ENODEV;
	}

	return ret;
}

static int cnss_shutdown(const struct subsys_desc *subsys_desc, bool force_stop)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = dev_get_drvdata(subsys_desc->dev);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return -ENODEV;
	}

	switch (plat_priv->device_id) {
	case QCA6174_DEVICE_ID:
		ret = cnss_qca6174_shutdown(plat_priv);
		break;
	case QCA6290_DEVICE_ID:
		break;
	default:
		cnss_pr_err("Unknown device_id found: 0x%lx\n",
			    plat_priv->device_id);
		ret = -ENODEV;
	}

	return ret;
}

static int cnss_ramdump(int enable, const struct subsys_desc *subsys_desc)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = dev_get_drvdata(subsys_desc->dev);
	struct cnss_ramdump_info *ramdump_info;
	struct ramdump_segment segment;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return -ENODEV;
	}

	ramdump_info = &plat_priv->ramdump_info;
	if (!ramdump_info->ramdump_size)
		return -EINVAL;

	if (!enable)
		return 0;

	switch (plat_priv->device_id) {
	case QCA6174_DEVICE_ID:
		memset(&segment, 0, sizeof(segment));
		segment.v_address = ramdump_info->ramdump_va;
		segment.size = ramdump_info->ramdump_size;
		ret = do_ramdump(ramdump_info->ramdump_dev, &segment, 1);
		break;
	case QCA6290_DEVICE_ID:
		break;
	default:
		cnss_pr_err("Unknown device_id found: 0x%lx\n",
			    plat_priv->device_id);
		ret = -ENODEV;
	}

	return ret;
}

void *cnss_get_virt_ramdump_mem(unsigned long *size)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);
	struct cnss_ramdump_info *ramdump_info;

	if (!plat_priv)
		return NULL;

	ramdump_info = &plat_priv->ramdump_info;
	*size = ramdump_info->ramdump_size;

	return ramdump_info->ramdump_va;
}
EXPORT_SYMBOL(cnss_get_virt_ramdump_mem);

void cnss_device_crashed(void)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);
	struct cnss_subsys_info *subsys_info;

	if (!plat_priv)
		return;

	subsys_info = &plat_priv->subsys_info;
	if (subsys_info->subsys_device) {
		subsys_set_crash_status(subsys_info->subsys_device, true);
		subsystem_restart_dev(subsys_info->subsys_device);
	}
}
EXPORT_SYMBOL(cnss_device_crashed);

static void cnss_crash_shutdown(const struct subsys_desc *subsys_desc)
{
	struct cnss_plat_data *plat_priv = dev_get_drvdata(subsys_desc->dev);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return;
	}

	switch (plat_priv->device_id) {
	case QCA6174_DEVICE_ID:
		cnss_qca6174_crash_shutdown(plat_priv);
		break;
	case QCA6290_DEVICE_ID:
		break;
	default:
		cnss_pr_err("Unknown device_id found: 0x%lx\n",
			    plat_priv->device_id);
	}
}

void cnss_device_self_recovery(void)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);
	struct cnss_subsys_info *subsys_info;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return;
	}

	if (!plat_priv->plat_dev) {
		cnss_pr_err("plat_dev is NULL!\n");
		return;
	}

	if (!plat_priv->driver_ops) {
		cnss_pr_err("Driver is not registered yet!\n");
		return;
	}

	if (plat_priv->recovery_in_progress) {
		cnss_pr_err("Recovery is already in progress!\n");
		return;
	}

	if (plat_priv->driver_status == CNSS_LOAD_UNLOAD) {
		cnss_pr_err("Driver load or unload is in progress!\n");
		return;
	}

	subsys_info = &plat_priv->subsys_info;
	plat_priv->recovery_count++;
	plat_priv->recovery_in_progress = true;
	pm_stay_awake(&plat_priv->plat_dev->dev);
	cnss_shutdown(&subsys_info->subsys_desc, false);
	udelay(WLAN_RECOVERY_DELAY);
	cnss_powerup(&subsys_info->subsys_desc);
	pm_relax(&plat_priv->plat_dev->dev);
	plat_priv->recovery_in_progress = false;
}
EXPORT_SYMBOL(cnss_device_self_recovery);

void cnss_recovery_work_handler(struct work_struct *recovery)
{
	cnss_device_self_recovery();
}

DECLARE_WORK(cnss_recovery_work, cnss_recovery_work_handler);

void cnss_schedule_recovery_work(void)
{
	schedule_work(&cnss_recovery_work);
}
EXPORT_SYMBOL(cnss_schedule_recovery_work);

static int cnss_register_subsys(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct cnss_subsys_info *subsys_info;

	subsys_info = &plat_priv->subsys_info;

	switch (plat_priv->device_id) {
	case QCA6174_DEVICE_ID:
		subsys_info->subsys_desc.name = "AR6320";
		break;
	case QCA6290_DEVICE_ID:
		subsys_info->subsys_desc.name = "QCA6290";
		break;
	default:
		cnss_pr_err("Unknown device ID: 0x%lx\n", plat_priv->device_id);
		ret = -ENODEV;
		goto out;
	}

	subsys_info->subsys_desc.owner = THIS_MODULE;
	subsys_info->subsys_desc.powerup = cnss_powerup;
	subsys_info->subsys_desc.shutdown = cnss_shutdown;
	subsys_info->subsys_desc.ramdump = cnss_ramdump;
	subsys_info->subsys_desc.crash_shutdown = cnss_crash_shutdown;
	subsys_info->subsys_desc.dev = &plat_priv->plat_dev->dev;

	subsys_info->subsys_device = subsys_register(&subsys_info->subsys_desc);
	if (IS_ERR(subsys_info->subsys_device)) {
		ret = PTR_ERR(subsys_info->subsys_device);
		cnss_pr_err("Failed to register subsys, err = %d\n", ret);
		goto out;
	}

	return 0;
out:
	return ret;
}

static void cnss_unregister_subsys(struct cnss_plat_data *plat_priv)
{
	struct cnss_subsys_info *subsys_info;

	subsys_info = &plat_priv->subsys_info;
	subsys_unregister(subsys_info->subsys_device);
}

static int cnss_init_dump_entry(struct cnss_plat_data *plat_priv)
{
	struct cnss_ramdump_info *ramdump_info;
	struct msm_dump_entry dump_entry;

	ramdump_info = &plat_priv->ramdump_info;
	ramdump_info->dump_data.addr = ramdump_info->ramdump_pa;
	ramdump_info->dump_data.len = ramdump_info->ramdump_size;
	ramdump_info->dump_data.version = CNSS_DUMP_FORMAT_VER;
	ramdump_info->dump_data.magic = CNSS_DUMP_MAGIC_VER_V2;
	strlcpy(ramdump_info->dump_data.name, CNSS_DUMP_NAME,
		sizeof(ramdump_info->dump_data.name));
	dump_entry.id = MSM_DUMP_DATA_CNSS_WLAN;
	dump_entry.addr = virt_to_phys(&ramdump_info->dump_data);

	return msm_dump_data_register(MSM_DUMP_TABLE_APPS, &dump_entry);
}

static int cnss_register_ramdump(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct device *dev;
	struct cnss_subsys_info *subsys_info;
	struct cnss_ramdump_info *ramdump_info;
	u32 ramdump_size = 0;

	dev = &plat_priv->plat_dev->dev;
	subsys_info = &plat_priv->subsys_info;
	ramdump_info = &plat_priv->ramdump_info;

	if (of_property_read_u32(dev->of_node, "qcom,wlan-ramdump-dynamic",
				 &ramdump_size) == 0) {
		ramdump_info->ramdump_va = dma_alloc_coherent(dev, ramdump_size,
			&ramdump_info->ramdump_pa, GFP_KERNEL);

		if (ramdump_info->ramdump_va)
			ramdump_info->ramdump_size = ramdump_size;
	}

	cnss_pr_dbg("ramdump va: %p, pa: %pa\n",
		    ramdump_info->ramdump_va, &ramdump_info->ramdump_pa);

	if (ramdump_info->ramdump_size == 0) {
		cnss_pr_info("Ramdump will not be collected");
		goto out;
	}

	ret = cnss_init_dump_entry(plat_priv);
	if (ret) {
		cnss_pr_err("Failed to setup dump table, err = %d\n", ret);
		goto free_ramdump;
	}

	ramdump_info->ramdump_dev = create_ramdump_device(
		subsys_info->subsys_desc.name, subsys_info->subsys_desc.dev);
	if (!ramdump_info->ramdump_dev) {
		cnss_pr_err("Failed to create ramdump device!");
		ret = -ENOMEM;
		goto free_ramdump;
	}

	return 0;
free_ramdump:
	dma_free_coherent(dev, ramdump_info->ramdump_size,
			  ramdump_info->ramdump_va, ramdump_info->ramdump_pa);
out:
	return ret;
}

static void cnss_unregister_ramdump(struct cnss_plat_data *plat_priv)
{
	struct device *dev;
	struct cnss_ramdump_info *ramdump_info;

	dev = &plat_priv->plat_dev->dev;
	ramdump_info = &plat_priv->ramdump_info;

	if (ramdump_info->ramdump_dev)
		destroy_ramdump_device(ramdump_info->ramdump_dev);

	if (ramdump_info->ramdump_va)
		dma_free_coherent(dev, ramdump_info->ramdump_size,
				  ramdump_info->ramdump_va,
				  ramdump_info->ramdump_pa);
}

static int cnss_register_bus_scale(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct cnss_bus_bw_info *bus_bw_info;

	bus_bw_info = &plat_priv->bus_bw_info;

	bus_bw_info->bus_scale_table =
		msm_bus_cl_get_pdata(plat_priv->plat_dev);
	if (bus_bw_info->bus_scale_table)  {
		bus_bw_info->bus_client =
			msm_bus_scale_register_client(
				bus_bw_info->bus_scale_table);
		if (!bus_bw_info->bus_client) {
			cnss_pr_err("Failed to register bus scale client!\n");
			ret = -EINVAL;
			goto out;
		}
	}

	return 0;
out:
	return ret;
}

static void cnss_unregister_bus_scale(struct cnss_plat_data *plat_priv)
{
	struct cnss_bus_bw_info *bus_bw_info;

	bus_bw_info = &plat_priv->bus_bw_info;

	if (bus_bw_info->bus_client)
		msm_bus_scale_unregister_client(bus_bw_info->bus_client);
}

static const struct platform_device_id cnss_platform_id_table[] = {
	{ .name = "qca6174", .driver_data = QCA6174_DEVICE_ID, },
	{ .name = "qca6290", .driver_data = QCA6290_DEVICE_ID, },
};

static const struct of_device_id cnss_of_match_table[] = {
	{
		.compatible = "qcom,cnss",
		.data = (void *)&cnss_platform_id_table[0]},
	{
		.compatible = "qcom,cnss-qca6290",
		.data = (void *)&cnss_platform_id_table[1]},
	{ },
};
MODULE_DEVICE_TABLE(of, cnss_of_match_table);

static int cnss_probe(struct platform_device *plat_dev)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv;
	const struct of_device_id *of_id;
	const struct platform_device_id *device_id;

	if (cnss_get_plat_priv(plat_dev)) {
		cnss_pr_err("Driver is already initialized!\n");
		ret = -EEXIST;
		goto out;
	}

	of_id = of_match_device(cnss_of_match_table, &plat_dev->dev);
	if (!of_id || !of_id->data) {
		cnss_pr_err("Failed to find of match device!\n");
		ret = -ENODEV;
		goto out;
	}

	device_id = of_id->data;

	plat_priv = devm_kzalloc(&plat_dev->dev, sizeof(*plat_priv),
				 GFP_KERNEL);
	if (!plat_priv) {
		ret = -ENOMEM;
		goto out;
	}

	plat_priv->plat_dev = plat_dev;
	plat_priv->device_id = device_id->driver_data;
	cnss_set_plat_priv(plat_dev, plat_priv);
	platform_set_drvdata(plat_dev, plat_priv);

	ret = cnss_get_resources(plat_priv);
	if (ret)
		goto reset_ctx;

	ret = cnss_power_on_device(plat_priv);
	if (ret)
		goto free_res;

	ret = cnss_pci_init(plat_priv);
	if (ret)
		goto power_off;

	ret = cnss_register_esoc(plat_priv);
	if (ret)
		goto deinit_pci;

	ret = cnss_register_subsys(plat_priv);
	if (ret)
		goto unreg_esoc;

	ret = cnss_register_ramdump(plat_priv);
	if (ret)
		goto unreg_subsys;

	ret = cnss_register_bus_scale(plat_priv);
	if (ret)
		goto unreg_ramdump;

	register_pm_notifier(&cnss_pm_notifier);

	cnss_pr_info("Platform driver probed successfully.\n");

	return 0;

unreg_ramdump:
	cnss_unregister_ramdump(plat_priv);
unreg_subsys:
	cnss_unregister_subsys(plat_priv);
unreg_esoc:
	cnss_unregister_esoc(plat_priv);
deinit_pci:
	cnss_pci_deinit(plat_priv);
power_off:
	cnss_power_off_device(plat_priv);
free_res:
	cnss_put_resources(plat_priv);
reset_ctx:
	platform_set_drvdata(plat_dev, NULL);
	cnss_set_plat_priv(plat_dev, NULL);
out:
	return ret;
}

static int cnss_remove(struct platform_device *plat_dev)
{
	struct cnss_plat_data *plat_priv = platform_get_drvdata(plat_dev);

	unregister_pm_notifier(&cnss_pm_notifier);
	cnss_unregister_bus_scale(plat_priv);
	cnss_unregister_ramdump(plat_priv);
	cnss_unregister_subsys(plat_priv);
	cnss_unregister_esoc(plat_priv);
	cnss_put_resources(plat_priv);
	cnss_pci_deinit(plat_priv);
	platform_set_drvdata(plat_dev, NULL);
	plat_env = NULL;

	return 0;
}

static struct platform_driver cnss_platform_driver = {
	.probe  = cnss_probe,
	.remove = cnss_remove,
	.driver = {
		.name = "cnss2",
		.owner = THIS_MODULE,
		.of_match_table = cnss_of_match_table,
	},
};

static int __init cnss_initialize(void)
{
	int ret = 0;

	cnss_debug_init();
	ret = platform_driver_register(&cnss_platform_driver);
	if (ret)
		cnss_debug_deinit();

	return ret;
}

static void __exit cnss_exit(void)
{
	platform_driver_unregister(&cnss_platform_driver);
	cnss_debug_deinit();
}

module_init(cnss_initialize);
module_exit(cnss_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CNSS2 Platform Driver");
