/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_wakeup.h>
#include <linux/rwsem.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <soc/qcom/ramdump.h>
#include <soc/qcom/subsystem_notif.h>

#include "main.h"
#include "debug.h"
#include "pci.h"

#define CNSS_DUMP_FORMAT_VER		0x11
#define CNSS_DUMP_FORMAT_VER_V2		0x22
#define CNSS_DUMP_MAGIC_VER_V2		0x42445953
#define CNSS_DUMP_NAME			"CNSS_WLAN"
#define CNSS_DUMP_DESC_SIZE		0x1000
#define CNSS_DUMP_SEG_VER		0x1
#define WLAN_RECOVERY_DELAY		1000
#define FILE_SYSTEM_READY		1
#define FW_READY_TIMEOUT		20000
#define FW_ASSERT_TIMEOUT		5000
#define CNSS_EVENT_PENDING		2989
#define WAKE_MSI_NAME			"WAKE"

static struct cnss_plat_data *plat_env;

static DECLARE_RWSEM(cnss_pm_sem);

static bool qmi_bypass;
#ifdef CONFIG_CNSS2_DEBUG
module_param(qmi_bypass, bool, 0600);
MODULE_PARM_DESC(qmi_bypass, "Bypass QMI from platform driver");
#endif

static bool enable_waltest;
#ifdef CONFIG_CNSS2_DEBUG
module_param(enable_waltest, bool, 0600);
MODULE_PARM_DESC(enable_waltest, "Enable to handle firmware waltest");
#endif

enum cnss_debug_quirks {
	LINK_DOWN_SELF_RECOVERY,
	SKIP_DEVICE_BOOT,
	USE_CORE_ONLY_FW,
};

unsigned long quirks;
#ifdef CONFIG_CNSS2_DEBUG
module_param(quirks, ulong, 0600);
MODULE_PARM_DESC(quirks, "Debug quirks for the driver");
#endif

static struct cnss_fw_files FW_FILES_QCA6174_FW_3_0 = {
	"qwlan30.bin", "bdwlan30.bin", "otp30.bin", "utf30.bin",
	"utfbd30.bin", "epping30.bin", "evicted30.bin"
};

static struct cnss_fw_files FW_FILES_DEFAULT = {
	"qwlan.bin", "bdwlan.bin", "otp.bin", "utf.bin",
	"utfbd.bin", "epping.bin", "evicted.bin"
};

struct cnss_driver_event {
	struct list_head list;
	enum cnss_driver_event_type type;
	bool sync;
	struct completion complete;
	int ret;
	void *data;
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

static void cnss_pm_stay_awake(struct cnss_plat_data *plat_priv)
{
	if (atomic_inc_return(&plat_priv->pm_count) != 1)
		return;

	cnss_pr_dbg("PM stay awake, state: 0x%lx, count: %d\n",
		    plat_priv->driver_state,
		    atomic_read(&plat_priv->pm_count));
	pm_stay_awake(&plat_priv->plat_dev->dev);
}

static void cnss_pm_relax(struct cnss_plat_data *plat_priv)
{
	int r = atomic_dec_return(&plat_priv->pm_count);

	WARN_ON(r < 0);

	if (r != 0)
		return;

	cnss_pr_dbg("PM relax, state: 0x%lx, count: %d\n",
		    plat_priv->driver_state,
		    atomic_read(&plat_priv->pm_count));
	pm_relax(&plat_priv->plat_dev->dev);
}

void cnss_lock_pm_sem(struct device *dev)
{
	down_read(&cnss_pm_sem);
}
EXPORT_SYMBOL(cnss_lock_pm_sem);

void cnss_release_pm_sem(struct device *dev)
{
	up_read(&cnss_pm_sem);
}
EXPORT_SYMBOL(cnss_release_pm_sem);

int cnss_get_fw_files_for_target(struct device *dev,
				 struct cnss_fw_files *pfw_files,
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

int cnss_request_bus_bandwidth(struct device *dev, int bandwidth)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct cnss_bus_bw_info *bus_bw_info;

	if (!plat_priv)
		return -ENODEV;

	bus_bw_info = &plat_priv->bus_bw_info;
	if (!bus_bw_info->bus_client)
		return -EINVAL;

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

int cnss_get_platform_cap(struct device *dev, struct cnss_platform_cap *cap)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return -ENODEV;

	if (cap)
		*cap = plat_priv->cap;

	return 0;
}
EXPORT_SYMBOL(cnss_get_platform_cap);

int cnss_get_soc_info(struct device *dev, struct cnss_soc_info *info)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	void *bus_priv = cnss_bus_dev_to_bus_priv(dev);

	if (!plat_priv)
		return -ENODEV;

	ret = cnss_pci_get_bar_info(bus_priv, &info->va, &info->pa);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL(cnss_get_soc_info);

void cnss_request_pm_qos(struct device *dev, u32 qos_val)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return;

	pm_qos_add_request(&plat_priv->qos_request, PM_QOS_CPU_DMA_LATENCY,
			   qos_val);
}
EXPORT_SYMBOL(cnss_request_pm_qos);

void cnss_remove_pm_qos(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return;

	pm_qos_remove_request(&plat_priv->qos_request);
}
EXPORT_SYMBOL(cnss_remove_pm_qos);

int cnss_wlan_enable(struct device *dev,
		     struct cnss_wlan_enable_cfg *config,
		     enum cnss_driver_mode mode,
		     const char *host_version)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct wlfw_wlan_cfg_req_msg_v01 req;
	u32 i;
	int ret = 0;

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		return 0;

	if (qmi_bypass)
		return 0;

	if (!config || !host_version) {
		cnss_pr_err("Invalid config or host_version pointer\n");
		return -EINVAL;
	}

	cnss_pr_dbg("Mode: %d, config: %pK, host_version: %s\n",
		    mode, config, host_version);

	if (mode == CNSS_WALTEST || mode == CNSS_CCPM)
		goto skip_cfg;

	memset(&req, 0, sizeof(req));

	req.host_version_valid = 1;
	strlcpy(req.host_version, host_version,
		QMI_WLFW_MAX_STR_LEN_V01 + 1);

	req.tgt_cfg_valid = 1;
	if (config->num_ce_tgt_cfg > QMI_WLFW_MAX_NUM_CE_V01)
		req.tgt_cfg_len = QMI_WLFW_MAX_NUM_CE_V01;
	else
		req.tgt_cfg_len = config->num_ce_tgt_cfg;
	for (i = 0; i < req.tgt_cfg_len; i++) {
		req.tgt_cfg[i].pipe_num = config->ce_tgt_cfg[i].pipe_num;
		req.tgt_cfg[i].pipe_dir = config->ce_tgt_cfg[i].pipe_dir;
		req.tgt_cfg[i].nentries = config->ce_tgt_cfg[i].nentries;
		req.tgt_cfg[i].nbytes_max = config->ce_tgt_cfg[i].nbytes_max;
		req.tgt_cfg[i].flags = config->ce_tgt_cfg[i].flags;
	}

	req.svc_cfg_valid = 1;
	if (config->num_ce_svc_pipe_cfg > QMI_WLFW_MAX_NUM_SVC_V01)
		req.svc_cfg_len = QMI_WLFW_MAX_NUM_SVC_V01;
	else
		req.svc_cfg_len = config->num_ce_svc_pipe_cfg;
	for (i = 0; i < req.svc_cfg_len; i++) {
		req.svc_cfg[i].service_id = config->ce_svc_cfg[i].service_id;
		req.svc_cfg[i].pipe_dir = config->ce_svc_cfg[i].pipe_dir;
		req.svc_cfg[i].pipe_num = config->ce_svc_cfg[i].pipe_num;
	}

	req.shadow_reg_v2_valid = 1;
	if (config->num_shadow_reg_v2_cfg >
	    QMI_WLFW_MAX_NUM_SHADOW_REG_V2_V01)
		req.shadow_reg_v2_len = QMI_WLFW_MAX_NUM_SHADOW_REG_V2_V01;
	else
		req.shadow_reg_v2_len = config->num_shadow_reg_v2_cfg;

	memcpy(req.shadow_reg_v2, config->shadow_reg_v2_cfg,
	       sizeof(struct wlfw_shadow_reg_v2_cfg_s_v01)
	       * req.shadow_reg_v2_len);

	ret = cnss_wlfw_wlan_cfg_send_sync(plat_priv, &req);
	if (ret)
		goto out;

skip_cfg:
	ret = cnss_wlfw_wlan_mode_send_sync(plat_priv, mode);
out:
	return ret;
}
EXPORT_SYMBOL(cnss_wlan_enable);

int cnss_wlan_disable(struct device *dev, enum cnss_driver_mode mode)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		return 0;

	if (qmi_bypass)
		return 0;

	return cnss_wlfw_wlan_mode_send_sync(plat_priv, QMI_WLFW_OFF_V01);
}
EXPORT_SYMBOL(cnss_wlan_disable);

#ifdef CONFIG_CNSS2_DEBUG
int cnss_athdiag_read(struct device *dev, u32 offset, u32 mem_type,
		      u32 data_len, u8 *output)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	int ret = 0;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return -EINVAL;
	}

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		return 0;

	if (!output || data_len == 0 || data_len > QMI_WLFW_MAX_DATA_SIZE_V01) {
		cnss_pr_err("Invalid parameters for athdiag read: output %p, data_len %u\n",
			    output, data_len);
		ret = -EINVAL;
		goto out;
	}

	if (!test_bit(CNSS_FW_READY, &plat_priv->driver_state)) {
		cnss_pr_err("Invalid state for athdiag read: 0x%lx\n",
			    plat_priv->driver_state);
		ret = -EINVAL;
		goto out;
	}

	ret = cnss_wlfw_athdiag_read_send_sync(plat_priv, offset, mem_type,
					       data_len, output);

out:
	return ret;
}
EXPORT_SYMBOL(cnss_athdiag_read);

int cnss_athdiag_write(struct device *dev, u32 offset, u32 mem_type,
		       u32 data_len, u8 *input)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	int ret = 0;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return -EINVAL;
	}

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		return 0;

	if (!input || data_len == 0 || data_len > QMI_WLFW_MAX_DATA_SIZE_V01) {
		cnss_pr_err("Invalid parameters for athdiag write: input %p, data_len %u\n",
			    input, data_len);
		ret = -EINVAL;
		goto out;
	}

	if (!test_bit(CNSS_FW_READY, &plat_priv->driver_state)) {
		cnss_pr_err("Invalid state for athdiag write: 0x%lx\n",
			    plat_priv->driver_state);
		ret = -EINVAL;
		goto out;
	}

	ret = cnss_wlfw_athdiag_write_send_sync(plat_priv, offset, mem_type,
						data_len, input);

out:
	return ret;
}
EXPORT_SYMBOL(cnss_athdiag_write);
#else
int cnss_athdiag_read(struct device *dev, u32 offset, u32 mem_type,
		      u32 data_len, u8 *output)
{
	return -EPERM;
}
EXPORT_SYMBOL(cnss_athdiag_read);

int cnss_athdiag_write(struct device *dev, u32 offset, u32 mem_type,
		       u32 data_len, u8 *input)
{
	return -EPERM;
}
EXPORT_SYMBOL(cnss_athdiag_write);
#endif

int cnss_set_fw_log_mode(struct device *dev, u8 fw_log_mode)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		return 0;

	return cnss_wlfw_ini_send_sync(plat_priv, fw_log_mode);
}
EXPORT_SYMBOL(cnss_set_fw_log_mode);

u32 cnss_get_wake_msi(struct cnss_plat_data *plat_priv)
{
	struct cnss_pci_data *pci_priv = plat_priv->bus_priv;
	int ret, num_vectors;
	u32 user_base_data, base_vector;

	ret = cnss_get_user_msi_assignment(&pci_priv->pci_dev->dev,
					   WAKE_MSI_NAME, &num_vectors,
					   &user_base_data, &base_vector);

	if (ret) {
		cnss_pr_err("WAKE MSI is not valid\n");
		return 0;
	}

	return user_base_data;
}

static int cnss_fw_mem_ready_hdlr(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	set_bit(CNSS_FW_MEM_READY, &plat_priv->driver_state);

	ret = cnss_wlfw_tgt_cap_send_sync(plat_priv);
	if (ret)
		goto out;

	ret = cnss_wlfw_bdf_dnld_send_sync(plat_priv);
	if (ret)
		goto out;

	ret = cnss_pci_load_m3(plat_priv->bus_priv);
	if (ret)
		goto out;

	ret = cnss_wlfw_m3_dnld_send_sync(plat_priv);
	if (ret)
		goto out;

	return 0;
out:
	return ret;
}

static int cnss_driver_call_probe(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct cnss_pci_data *pci_priv = plat_priv->bus_priv;

	if (test_bit(CNSS_DRIVER_DEBUG, &plat_priv->driver_state)) {
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		cnss_pr_dbg("Skip driver probe\n");
		goto out;
	}

	if (!plat_priv->driver_ops) {
		cnss_pr_err("driver_ops is NULL\n");
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state) &&
	    test_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state)) {
		ret = plat_priv->driver_ops->reinit(pci_priv->pci_dev,
						    pci_priv->pci_device_id);
		if (ret) {
			cnss_pr_err("Failed to reinit host driver, err = %d\n",
				    ret);
			goto out;
		}
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
	} else if (test_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state)) {
		ret = plat_priv->driver_ops->probe(pci_priv->pci_dev,
						   pci_priv->pci_device_id);
		if (ret) {
			cnss_pr_err("Failed to probe host driver, err = %d\n",
				    ret);
			goto out;
		}
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		clear_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state);
		set_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state);
	}

	return 0;

out:
	return ret;
}

static int cnss_driver_call_remove(struct cnss_plat_data *plat_priv)
{
	struct cnss_pci_data *pci_priv = plat_priv->bus_priv;

	if (test_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state) ||
	    test_bit(CNSS_FW_BOOT_RECOVERY, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_DEBUG, &plat_priv->driver_state)) {
		cnss_pr_dbg("Skip driver remove\n");
		return 0;
	}

	if (!plat_priv->driver_ops) {
		cnss_pr_err("driver_ops is NULL\n");
		return -EINVAL;
	}

	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state) &&
	    test_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state)) {
		plat_priv->driver_ops->shutdown(pci_priv->pci_dev);
	} else if (test_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state)) {
		plat_priv->driver_ops->remove(pci_priv->pci_dev);
		clear_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state);
	}

	return 0;
}

static int cnss_fw_ready_hdlr(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	del_timer(&plat_priv->fw_boot_timer);
	set_bit(CNSS_FW_READY, &plat_priv->driver_state);

	if (test_bit(CNSS_FW_BOOT_RECOVERY, &plat_priv->driver_state)) {
		clear_bit(CNSS_FW_BOOT_RECOVERY, &plat_priv->driver_state);
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
	}

	if (enable_waltest) {
		ret = cnss_wlfw_wlan_mode_send_sync(plat_priv,
						    QMI_WLFW_WALTEST_V01);
	} else if (test_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state)) {
		ret = cnss_wlfw_wlan_mode_send_sync(plat_priv,
						    QMI_WLFW_CALIBRATION_V01);
	} else if (test_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state) ||
		   test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		ret = cnss_driver_call_probe(plat_priv);
	} else {
		complete(&plat_priv->power_up_complete);
	}

	if (ret && test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state))
		goto out;
	else if (ret)
		goto shutdown;

	return 0;

shutdown:
	cnss_pci_stop_mhi(plat_priv->bus_priv);
	cnss_suspend_pci_link(plat_priv->bus_priv);
	cnss_power_off_device(plat_priv);

	clear_bit(CNSS_FW_READY, &plat_priv->driver_state);
	clear_bit(CNSS_FW_MEM_READY, &plat_priv->driver_state);

out:
	return ret;
}

static char *cnss_driver_event_to_str(enum cnss_driver_event_type type)
{
	switch (type) {
	case CNSS_DRIVER_EVENT_SERVER_ARRIVE:
		return "SERVER_ARRIVE";
	case CNSS_DRIVER_EVENT_SERVER_EXIT:
		return "SERVER_EXIT";
	case CNSS_DRIVER_EVENT_REQUEST_MEM:
		return "REQUEST_MEM";
	case CNSS_DRIVER_EVENT_FW_MEM_READY:
		return "FW_MEM_READY";
	case CNSS_DRIVER_EVENT_FW_READY:
		return "FW_READY";
	case CNSS_DRIVER_EVENT_COLD_BOOT_CAL_START:
		return "COLD_BOOT_CAL_START";
	case CNSS_DRIVER_EVENT_COLD_BOOT_CAL_DONE:
		return "COLD_BOOT_CAL_DONE";
	case CNSS_DRIVER_EVENT_REGISTER_DRIVER:
		return "REGISTER_DRIVER";
	case CNSS_DRIVER_EVENT_UNREGISTER_DRIVER:
		return "UNREGISTER_DRIVER";
	case CNSS_DRIVER_EVENT_RECOVERY:
		return "RECOVERY";
	case CNSS_DRIVER_EVENT_FORCE_FW_ASSERT:
		return "FORCE_FW_ASSERT";
	case CNSS_DRIVER_EVENT_POWER_UP:
		return "POWER_UP";
	case CNSS_DRIVER_EVENT_POWER_DOWN:
		return "POWER_DOWN";
	case CNSS_DRIVER_EVENT_MAX:
		return "EVENT_MAX";
	}

	return "UNKNOWN";
};

int cnss_driver_event_post(struct cnss_plat_data *plat_priv,
			   enum cnss_driver_event_type type,
			   bool sync, void *data)
{
	struct cnss_driver_event *event;
	unsigned long flags;
	int gfp = GFP_KERNEL;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Posting event: %s(%d)%s, state: 0x%lx\n",
		    cnss_driver_event_to_str(type), type,
		    sync ? "-sync" : "", plat_priv->driver_state);

	if (type >= CNSS_DRIVER_EVENT_MAX) {
		cnss_pr_err("Invalid Event type: %d, can't post", type);
		return -EINVAL;
	}

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	event = kzalloc(sizeof(*event), gfp);
	if (!event)
		return -ENOMEM;

	cnss_pm_stay_awake(plat_priv);

	event->type = type;
	event->data = data;
	init_completion(&event->complete);
	event->ret = CNSS_EVENT_PENDING;
	event->sync = sync;

	spin_lock_irqsave(&plat_priv->event_lock, flags);
	list_add_tail(&event->list, &plat_priv->event_list);
	spin_unlock_irqrestore(&plat_priv->event_lock, flags);

	queue_work(plat_priv->event_wq, &plat_priv->event_work);

	if (!sync)
		goto out;

	ret = wait_for_completion_interruptible(&event->complete);

	cnss_pr_dbg("Completed event: %s(%d), state: 0x%lx, ret: %d/%d\n",
		    cnss_driver_event_to_str(type), type,
		    plat_priv->driver_state, ret, event->ret);

	spin_lock_irqsave(&plat_priv->event_lock, flags);
	if (ret == -ERESTARTSYS && event->ret == CNSS_EVENT_PENDING) {
		event->sync = false;
		spin_unlock_irqrestore(&plat_priv->event_lock, flags);
		ret = -EINTR;
		goto out;
	}
	spin_unlock_irqrestore(&plat_priv->event_lock, flags);

	ret = event->ret;
	kfree(event);

out:
	cnss_pm_relax(plat_priv);
	return ret;
}

int cnss_power_up(struct device *dev)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	unsigned int timeout;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	cnss_pr_dbg("Powering up device\n");

	ret = cnss_driver_event_post(plat_priv,
				     CNSS_DRIVER_EVENT_POWER_UP,
				     true, NULL);
	if (ret)
		goto out;

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		goto out;

	timeout = cnss_get_qmi_timeout();

	reinit_completion(&plat_priv->power_up_complete);
	ret = wait_for_completion_timeout(&plat_priv->power_up_complete,
					  msecs_to_jiffies(timeout) << 2);
	if (!ret) {
		cnss_pr_err("Timeout waiting for power up to complete\n");
		ret = -EAGAIN;
		goto out;
	}

	return 0;

out:
	return ret;
}
EXPORT_SYMBOL(cnss_power_up);

int cnss_power_down(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	cnss_pr_dbg("Powering down device\n");

	return cnss_driver_event_post(plat_priv,
				      CNSS_DRIVER_EVENT_POWER_DOWN,
				      true, NULL);
}
EXPORT_SYMBOL(cnss_power_down);

int cnss_wlan_register_driver(struct cnss_wlan_driver *driver_ops)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = cnss_get_plat_priv(NULL);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return -ENODEV;
	}

	if (plat_priv->driver_ops) {
		cnss_pr_err("Driver has already registered!\n");
		return -EEXIST;
	}

	ret = cnss_driver_event_post(plat_priv,
				     CNSS_DRIVER_EVENT_REGISTER_DRIVER,
				     true, driver_ops);
	return ret;
}
EXPORT_SYMBOL(cnss_wlan_register_driver);

void cnss_wlan_unregister_driver(struct cnss_wlan_driver *driver_ops)
{
	struct cnss_plat_data *plat_priv = cnss_get_plat_priv(NULL);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return;
	}

	cnss_driver_event_post(plat_priv,
			       CNSS_DRIVER_EVENT_UNREGISTER_DRIVER,
			       true, NULL);
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

	if (code == SUBSYS_AFTER_POWERUP)
		esoc_info->modem_current_status = 1;
	else if (code == SUBSYS_BEFORE_SHUTDOWN)
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

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL!\n");
		return -ENODEV;
	}

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

	ret = cnss_driver_call_probe(plat_priv);
	if (ret)
		goto suspend_link;

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

	cnss_pm_request_resume(pci_priv);

	cnss_driver_call_remove(plat_priv);

	cnss_request_bus_bandwidth(&plat_priv->plat_dev->dev,
				   CNSS_BUS_WIDTH_NONE);
	cnss_pci_set_monitor_wake_intr(pci_priv, false);
	cnss_pci_set_auto_suspended(pci_priv, 0);

	ret = cnss_suspend_pci_link(pci_priv);
	if (ret)
		cnss_pr_err("Failed to suspend PCI link, err = %d\n", ret);

	cnss_power_off_device(plat_priv);

	clear_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state);

	return ret;
}

static void cnss_qca6174_crash_shutdown(struct cnss_plat_data *plat_priv)
{
	struct cnss_pci_data *pci_priv = plat_priv->bus_priv;

	if (!plat_priv->driver_ops)
		return;

	plat_priv->driver_ops->crash_shutdown(pci_priv->pci_dev);
}

static int cnss_qca6290_powerup(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct cnss_pci_data *pci_priv = plat_priv->bus_priv;
	unsigned int timeout;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL!\n");
		return -ENODEV;
	}

	if (plat_priv->ramdump_info_v2.dump_data_valid ||
	    test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_DEINIT);
		cnss_pci_clear_dump_info(pci_priv);
	}

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

	timeout = cnss_get_qmi_timeout();

	ret = cnss_pci_start_mhi(pci_priv);
	if (ret) {
		cnss_pr_err("Failed to start MHI, err = %d\n", ret);
		if (!test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state) &&
		    !pci_priv->pci_link_down_ind && timeout)
			mod_timer(&plat_priv->fw_boot_timer,
				  jiffies + msecs_to_jiffies(timeout >> 1));
		return 0;
	}

	if (test_bit(USE_CORE_ONLY_FW, &quirks)) {
		clear_bit(CNSS_FW_BOOT_RECOVERY, &plat_priv->driver_state);
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		return 0;
	}

	cnss_set_pin_connect_status(plat_priv);

	if (qmi_bypass) {
		ret = cnss_driver_call_probe(plat_priv);
		if (ret)
			goto stop_mhi;
	} else if (timeout) {
		mod_timer(&plat_priv->fw_boot_timer,
			  jiffies + msecs_to_jiffies(timeout << 1));
	}

	return 0;

stop_mhi:
	cnss_pci_stop_mhi(pci_priv);
	cnss_suspend_pci_link(pci_priv);
power_off:
	cnss_power_off_device(plat_priv);
out:
	return ret;
}

static int cnss_qca6290_shutdown(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct cnss_pci_data *pci_priv = plat_priv->bus_priv;

	if (!pci_priv)
		return -ENODEV;

	cnss_pm_request_resume(pci_priv);

	cnss_driver_call_remove(plat_priv);

	cnss_request_bus_bandwidth(&plat_priv->plat_dev->dev,
				   CNSS_BUS_WIDTH_NONE);
	cnss_pci_set_monitor_wake_intr(pci_priv, false);
	cnss_pci_set_auto_suspended(pci_priv, 0);

	cnss_pci_stop_mhi(pci_priv);

	ret = cnss_suspend_pci_link(pci_priv);
	if (ret)
		cnss_pr_err("Failed to suspend PCI link, err = %d\n", ret);

	cnss_power_off_device(plat_priv);

	clear_bit(CNSS_FW_READY, &plat_priv->driver_state);
	clear_bit(CNSS_FW_MEM_READY, &plat_priv->driver_state);
	clear_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state);

	return ret;
}

static void cnss_qca6290_crash_shutdown(struct cnss_plat_data *plat_priv)
{
	struct cnss_pci_data *pci_priv = plat_priv->bus_priv;
	int ret = 0;

	cnss_pr_dbg("Crash shutdown with driver_state 0x%lx\n",
		    plat_priv->driver_state);

	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state)) {
		cnss_pr_dbg("Ignore crash shutdown\n");
		return;
	}

	ret = cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_RDDM_KERNEL_PANIC);
	if (ret) {
		cnss_pr_err("Fail to complete RDDM, err = %d\n", ret);
		return;
	}

	cnss_pci_collect_dump_info(pci_priv);
}

static int cnss_powerup(struct cnss_plat_data *plat_priv)
{
	int ret;

	switch (plat_priv->device_id) {
	case QCA6174_DEVICE_ID:
		ret = cnss_qca6174_powerup(plat_priv);
		break;
	case QCA6290_EMULATION_DEVICE_ID:
	case QCA6290_DEVICE_ID:
		ret = cnss_qca6290_powerup(plat_priv);
		break;
	default:
		cnss_pr_err("Unknown device_id found: 0x%lx\n",
			    plat_priv->device_id);
		ret = -ENODEV;
	}

	return ret;
}

static int cnss_shutdown(struct cnss_plat_data *plat_priv)
{
	int ret;

	switch (plat_priv->device_id) {
	case QCA6174_DEVICE_ID:
		ret = cnss_qca6174_shutdown(plat_priv);
		break;
	case QCA6290_EMULATION_DEVICE_ID:
	case QCA6290_DEVICE_ID:
		ret = cnss_qca6290_shutdown(plat_priv);
		break;
	default:
		cnss_pr_err("Unknown device_id found: 0x%lx\n",
			    plat_priv->device_id);
		ret = -ENODEV;
	}

	return ret;
}

static int cnss_subsys_powerup(const struct subsys_desc *subsys_desc)
{
	struct cnss_plat_data *plat_priv;

	if (!subsys_desc->dev) {
		cnss_pr_err("dev from subsys_desc is NULL\n");
		return -ENODEV;
	}

	plat_priv = dev_get_drvdata(subsys_desc->dev);
	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	if (!plat_priv->driver_state) {
		cnss_pr_dbg("Powerup is ignored\n");
		return 0;
	}

	return cnss_powerup(plat_priv);
}

static int cnss_subsys_shutdown(const struct subsys_desc *subsys_desc,
				bool force_stop)
{
	struct cnss_plat_data *plat_priv;

	if (!subsys_desc->dev) {
		cnss_pr_err("dev from subsys_desc is NULL\n");
		return -ENODEV;
	}

	plat_priv = dev_get_drvdata(subsys_desc->dev);
	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	if (!plat_priv->driver_state) {
		cnss_pr_dbg("shutdown is ignored\n");
		return 0;
	}

	return cnss_shutdown(plat_priv);
}

static int cnss_qca6290_ramdump(struct cnss_plat_data *plat_priv)
{
	struct cnss_ramdump_info_v2 *info_v2 = &plat_priv->ramdump_info_v2;
	struct cnss_dump_data *dump_data = &info_v2->dump_data;
	struct cnss_dump_seg *dump_seg = info_v2->dump_data_vaddr;
	struct ramdump_segment *ramdump_segs, *s;
	int i, ret = 0;

	if (!info_v2->dump_data_valid ||
	    dump_data->nentries == 0)
		return 0;

	ramdump_segs = kcalloc(dump_data->nentries,
			       sizeof(*ramdump_segs),
			       GFP_KERNEL);
	if (!ramdump_segs)
		return -ENOMEM;

	s = ramdump_segs;
	for (i = 0; i < dump_data->nentries; i++) {
		s->address = dump_seg->address;
		s->v_address = dump_seg->v_address;
		s->size = dump_seg->size;
		s++;
		dump_seg++;
	}

	ret = do_elf_ramdump(info_v2->ramdump_dev, ramdump_segs,
			     dump_data->nentries);
	kfree(ramdump_segs);

	cnss_pci_set_mhi_state(plat_priv->bus_priv, CNSS_MHI_DEINIT);
	cnss_pci_clear_dump_info(plat_priv->bus_priv);

	return ret;
}

static int cnss_qca6174_ramdump(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct cnss_ramdump_info *ramdump_info;
	struct ramdump_segment segment;

	ramdump_info = &plat_priv->ramdump_info;
	if (!ramdump_info->ramdump_size)
		return -EINVAL;

	memset(&segment, 0, sizeof(segment));
	segment.v_address = ramdump_info->ramdump_va;
	segment.size = ramdump_info->ramdump_size;
	ret = do_ramdump(ramdump_info->ramdump_dev, &segment, 1);

	return ret;
}

static int cnss_subsys_ramdump(int enable,
			       const struct subsys_desc *subsys_desc)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = dev_get_drvdata(subsys_desc->dev);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return -ENODEV;
	}

	if (!enable)
		return 0;

	switch (plat_priv->device_id) {
	case QCA6174_DEVICE_ID:
		ret = cnss_qca6174_ramdump(plat_priv);
		break;
	case QCA6290_EMULATION_DEVICE_ID:
	case QCA6290_DEVICE_ID:
		ret = cnss_qca6290_ramdump(plat_priv);
		break;
	default:
		cnss_pr_err("Unknown device_id found: 0x%lx\n",
			    plat_priv->device_id);
		ret = -ENODEV;
	}

	return ret;
}

void *cnss_get_virt_ramdump_mem(struct device *dev, unsigned long *size)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct cnss_ramdump_info *ramdump_info;

	if (!plat_priv)
		return NULL;

	ramdump_info = &plat_priv->ramdump_info;
	*size = ramdump_info->ramdump_size;

	return ramdump_info->ramdump_va;
}
EXPORT_SYMBOL(cnss_get_virt_ramdump_mem);

void cnss_device_crashed(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct cnss_subsys_info *subsys_info;

	if (!plat_priv)
		return;

	subsys_info = &plat_priv->subsys_info;
	if (subsys_info->subsys_device) {
		set_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		subsys_set_crash_status(subsys_info->subsys_device, true);
		subsystem_restart_dev(subsys_info->subsys_device);
	}
}
EXPORT_SYMBOL(cnss_device_crashed);

static void cnss_subsys_crash_shutdown(const struct subsys_desc *subsys_desc)
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
	case QCA6290_EMULATION_DEVICE_ID:
	case QCA6290_DEVICE_ID:
		cnss_qca6290_crash_shutdown(plat_priv);
		break;
	default:
		cnss_pr_err("Unknown device_id found: 0x%lx\n",
			    plat_priv->device_id);
	}
}

static const char *cnss_recovery_reason_to_str(enum cnss_recovery_reason reason)
{
	switch (reason) {
	case CNSS_REASON_DEFAULT:
		return "DEFAULT";
	case CNSS_REASON_LINK_DOWN:
		return "LINK_DOWN";
	case CNSS_REASON_RDDM:
		return "RDDM";
	case CNSS_REASON_TIMEOUT:
		return "TIMEOUT";
	}

	return "UNKNOWN";
};

static int cnss_do_recovery(struct cnss_plat_data *plat_priv,
			    enum cnss_recovery_reason reason)
{
	struct cnss_pci_data *pci_priv = plat_priv->bus_priv;
	struct cnss_subsys_info *subsys_info =
		&plat_priv->subsys_info;
	int ret = 0;

	plat_priv->recovery_count++;

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		goto self_recovery;

	if (plat_priv->driver_ops &&
	    test_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state))
		plat_priv->driver_ops->update_status(pci_priv->pci_dev,
						     CNSS_RECOVERY);

	switch (reason) {
	case CNSS_REASON_LINK_DOWN:
		if (test_bit(LINK_DOWN_SELF_RECOVERY, &quirks))
			goto self_recovery;
		break;
	case CNSS_REASON_RDDM:
		clear_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state);
		ret = cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_RDDM);
		if (ret) {
			cnss_pr_err("Failed to complete RDDM, err = %d\n", ret);
			break;
		}
		cnss_pci_collect_dump_info(pci_priv);
		break;
	case CNSS_REASON_DEFAULT:
	case CNSS_REASON_TIMEOUT:
		break;
	default:
		cnss_pr_err("Unsupported recovery reason: %s(%d)\n",
			    cnss_recovery_reason_to_str(reason), reason);
		break;
	}

	if (!subsys_info->subsys_device)
		return 0;

	subsys_set_crash_status(subsys_info->subsys_device, true);
	subsystem_restart_dev(subsys_info->subsys_device);

	return 0;

self_recovery:
	cnss_shutdown(plat_priv);
	cnss_powerup(plat_priv);

	return 0;
}

static int cnss_driver_recovery_hdlr(struct cnss_plat_data *plat_priv,
				     void *data)
{
	struct cnss_recovery_data *recovery_data = data;
	int ret = 0;

	cnss_pr_dbg("Driver recovery is triggered with reason: %s(%d)\n",
		    cnss_recovery_reason_to_str(recovery_data->reason),
		    recovery_data->reason);

	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		cnss_pr_err("Recovery is already in progress!\n");
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state)) {
		cnss_pr_err("Driver unload is in progress, ignore recovery\n");
		ret = -EINVAL;
		goto out;
	}

	switch (plat_priv->device_id) {
	case QCA6174_DEVICE_ID:
		if (test_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state)) {
			cnss_pr_err("Driver load is in progress, ignore recovery\n");
			ret = -EINVAL;
			goto out;
		}
		break;
	default:
		if (!test_bit(CNSS_FW_READY, &plat_priv->driver_state)) {
			set_bit(CNSS_FW_BOOT_RECOVERY,
				&plat_priv->driver_state);
		}
		break;
	}

	set_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
	ret = cnss_do_recovery(plat_priv, recovery_data->reason);

out:
	kfree(data);
	return ret;
}

int cnss_self_recovery(struct device *dev,
		       enum cnss_recovery_reason reason)
{
	cnss_schedule_recovery(dev, reason);
	return 0;
}
EXPORT_SYMBOL(cnss_self_recovery);

void cnss_schedule_recovery(struct device *dev,
			    enum cnss_recovery_reason reason)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct cnss_recovery_data *data;
	int gfp = GFP_KERNEL;

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	data = kzalloc(sizeof(*data), gfp);
	if (!data)
		return;

	data->reason = reason;
	cnss_driver_event_post(plat_priv,
			       CNSS_DRIVER_EVENT_RECOVERY,
			       false, data);
}
EXPORT_SYMBOL(cnss_schedule_recovery);

static int cnss_force_fw_assert_hdlr(struct cnss_plat_data *plat_priv)
{
	struct cnss_pci_data *pci_priv = plat_priv->bus_priv;
	int ret;

	ret = cnss_pci_set_mhi_state(plat_priv->bus_priv,
				     CNSS_MHI_TRIGGER_RDDM);
	if (ret) {
		cnss_pr_err("Failed to trigger RDDM, err = %d\n", ret);
		cnss_schedule_recovery(&pci_priv->pci_dev->dev,
				       CNSS_REASON_DEFAULT);
		return 0;
	}

	if (!test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state)) {
		mod_timer(&plat_priv->fw_boot_timer,
			  jiffies + msecs_to_jiffies(FW_ASSERT_TIMEOUT));
	}

	return 0;
}

int cnss_force_fw_assert(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	if (plat_priv->device_id == QCA6174_DEVICE_ID) {
		cnss_pr_info("Forced FW assert is not supported\n");
		return -EOPNOTSUPP;
	}

	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		cnss_pr_info("Recovery is already in progress, ignore forced FW assert\n");
		return 0;
	}

	cnss_driver_event_post(plat_priv,
			       CNSS_DRIVER_EVENT_FORCE_FW_ASSERT,
			       false, NULL);

	return 0;
}
EXPORT_SYMBOL(cnss_force_fw_assert);

void fw_boot_timeout(unsigned long data)
{
	struct cnss_plat_data *plat_priv = (struct cnss_plat_data *)data;
	struct cnss_pci_data *pci_priv = plat_priv->bus_priv;

	cnss_pr_err("Timeout waiting for FW ready indication!\n");

	cnss_schedule_recovery(&pci_priv->pci_dev->dev,
			       CNSS_REASON_TIMEOUT);
}

static int cnss_register_driver_hdlr(struct cnss_plat_data *plat_priv,
				     void *data)
{
	int ret = 0;

	set_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state);
	plat_priv->driver_ops = data;

	ret = cnss_powerup(plat_priv);
	if (ret) {
		clear_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state);
		plat_priv->driver_ops = NULL;
	}

	return ret;
}

static int cnss_unregister_driver_hdlr(struct cnss_plat_data *plat_priv)
{
	set_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state);
	cnss_shutdown(plat_priv);
	plat_priv->driver_ops = NULL;

	return 0;
}

static int cnss_cold_boot_cal_start_hdlr(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	set_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state);
	ret = cnss_powerup(plat_priv);
	if (ret)
		clear_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state);

	return ret;
}

static int cnss_cold_boot_cal_done_hdlr(struct cnss_plat_data *plat_priv)
{
	cnss_wlfw_wlan_mode_send_sync(plat_priv, QMI_WLFW_OFF_V01);
	cnss_shutdown(plat_priv);
	clear_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state);

	return 0;
}

static int cnss_power_up_hdlr(struct cnss_plat_data *plat_priv)
{
	return cnss_powerup(plat_priv);
}

static int cnss_power_down_hdlr(struct cnss_plat_data *plat_priv)
{
	cnss_shutdown(plat_priv);

	return 0;
}

static void cnss_driver_event_work(struct work_struct *work)
{
	struct cnss_plat_data *plat_priv =
		container_of(work, struct cnss_plat_data, event_work);
	struct cnss_driver_event *event;
	unsigned long flags;
	int ret = 0;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return;
	}

	cnss_pm_stay_awake(plat_priv);

	spin_lock_irqsave(&plat_priv->event_lock, flags);

	while (!list_empty(&plat_priv->event_list)) {
		event = list_first_entry(&plat_priv->event_list,
					 struct cnss_driver_event, list);
		list_del(&event->list);
		spin_unlock_irqrestore(&plat_priv->event_lock, flags);

		cnss_pr_dbg("Processing driver event: %s%s(%d), state: 0x%lx\n",
			    cnss_driver_event_to_str(event->type),
			    event->sync ? "-sync" : "", event->type,
			    plat_priv->driver_state);

		switch (event->type) {
		case CNSS_DRIVER_EVENT_SERVER_ARRIVE:
			ret = cnss_wlfw_server_arrive(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_SERVER_EXIT:
			ret = cnss_wlfw_server_exit(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_REQUEST_MEM:
			ret = cnss_pci_alloc_fw_mem(plat_priv->bus_priv);
			if (ret)
				break;
			ret = cnss_wlfw_respond_mem_send_sync(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_FW_MEM_READY:
			ret = cnss_fw_mem_ready_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_FW_READY:
			ret = cnss_fw_ready_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_COLD_BOOT_CAL_START:
			ret = cnss_cold_boot_cal_start_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_COLD_BOOT_CAL_DONE:
			ret = cnss_cold_boot_cal_done_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_REGISTER_DRIVER:
			ret = cnss_register_driver_hdlr(plat_priv,
							event->data);
			break;
		case CNSS_DRIVER_EVENT_UNREGISTER_DRIVER:
			ret = cnss_unregister_driver_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_RECOVERY:
			ret = cnss_driver_recovery_hdlr(plat_priv,
							event->data);
			break;
		case CNSS_DRIVER_EVENT_FORCE_FW_ASSERT:
			ret = cnss_force_fw_assert_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_POWER_UP:
			ret = cnss_power_up_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_POWER_DOWN:
			ret = cnss_power_down_hdlr(plat_priv);
			break;
		default:
			cnss_pr_err("Invalid driver event type: %d",
				    event->type);
			kfree(event);
			spin_lock_irqsave(&plat_priv->event_lock, flags);
			continue;
		}

		spin_lock_irqsave(&plat_priv->event_lock, flags);
		if (event->sync) {
			event->ret = ret;
			complete(&event->complete);
			continue;
		}
		spin_unlock_irqrestore(&plat_priv->event_lock, flags);

		kfree(event);

		spin_lock_irqsave(&plat_priv->event_lock, flags);
	}
	spin_unlock_irqrestore(&plat_priv->event_lock, flags);

	cnss_pm_relax(plat_priv);
}

int cnss_register_subsys(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct cnss_subsys_info *subsys_info;

	subsys_info = &plat_priv->subsys_info;

	switch (plat_priv->device_id) {
	case QCA6174_DEVICE_ID:
		subsys_info->subsys_desc.name = "AR6320";
		break;
	case QCA6290_EMULATION_DEVICE_ID:
	case QCA6290_DEVICE_ID:
		subsys_info->subsys_desc.name = "QCA6290";
		break;
	default:
		cnss_pr_err("Unknown device ID: 0x%lx\n", plat_priv->device_id);
		ret = -ENODEV;
		goto out;
	}

	subsys_info->subsys_desc.owner = THIS_MODULE;
	subsys_info->subsys_desc.powerup = cnss_subsys_powerup;
	subsys_info->subsys_desc.shutdown = cnss_subsys_shutdown;
	subsys_info->subsys_desc.ramdump = cnss_subsys_ramdump;
	subsys_info->subsys_desc.crash_shutdown = cnss_subsys_crash_shutdown;
	subsys_info->subsys_desc.dev = &plat_priv->plat_dev->dev;

	subsys_info->subsys_device = subsys_register(&subsys_info->subsys_desc);
	if (IS_ERR(subsys_info->subsys_device)) {
		ret = PTR_ERR(subsys_info->subsys_device);
		cnss_pr_err("Failed to register subsys, err = %d\n", ret);
		goto out;
	}

	subsys_info->subsys_handle =
		subsystem_get(subsys_info->subsys_desc.name);
	if (!subsys_info->subsys_handle) {
		cnss_pr_err("Failed to get subsys_handle!\n");
		ret = -EINVAL;
		goto unregister_subsys;
	} else if (IS_ERR(subsys_info->subsys_handle)) {
		ret = PTR_ERR(subsys_info->subsys_handle);
		cnss_pr_err("Failed to do subsystem_get, err = %d\n", ret);
		goto unregister_subsys;
	}

	return 0;

unregister_subsys:
	subsys_unregister(subsys_info->subsys_device);
out:
	return ret;
}

void cnss_unregister_subsys(struct cnss_plat_data *plat_priv)
{
	struct cnss_subsys_info *subsys_info;

	subsys_info = &plat_priv->subsys_info;
	subsystem_put(subsys_info->subsys_handle);
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

static int cnss_qca6174_register_ramdump(struct cnss_plat_data *plat_priv)
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

	cnss_pr_dbg("ramdump va: %pK, pa: %pa\n",
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

static void cnss_qca6174_unregister_ramdump(struct cnss_plat_data *plat_priv)
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

static int cnss_qca6290_register_ramdump(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct cnss_subsys_info *subsys_info;
	struct cnss_ramdump_info_v2 *info_v2;
	struct cnss_dump_data *dump_data;
	struct msm_dump_entry dump_entry;
	struct device *dev = &plat_priv->plat_dev->dev;
	u32 ramdump_size = 0;

	subsys_info = &plat_priv->subsys_info;
	info_v2 = &plat_priv->ramdump_info_v2;
	dump_data = &info_v2->dump_data;

	if (of_property_read_u32(dev->of_node, "qcom,wlan-ramdump-dynamic",
				 &ramdump_size) == 0)
		info_v2->ramdump_size = ramdump_size;

	cnss_pr_dbg("Ramdump size 0x%lx\n", info_v2->ramdump_size);

	info_v2->dump_data_vaddr = kzalloc(CNSS_DUMP_DESC_SIZE, GFP_KERNEL);
	if (!info_v2->dump_data_vaddr)
		return -ENOMEM;

	dump_data->paddr = virt_to_phys(info_v2->dump_data_vaddr);
	dump_data->version = CNSS_DUMP_FORMAT_VER_V2;
	dump_data->magic = CNSS_DUMP_MAGIC_VER_V2;
	dump_data->seg_version = CNSS_DUMP_SEG_VER;
	strlcpy(dump_data->name, CNSS_DUMP_NAME,
		sizeof(dump_data->name));
	dump_entry.id = MSM_DUMP_DATA_CNSS_WLAN;
	dump_entry.addr = virt_to_phys(dump_data);

	ret = msm_dump_data_register(MSM_DUMP_TABLE_APPS, &dump_entry);
	if (ret) {
		cnss_pr_err("Failed to setup dump table, err = %d\n", ret);
		goto free_ramdump;
	}

	info_v2->ramdump_dev =
		create_ramdump_device(subsys_info->subsys_desc.name,
				      subsys_info->subsys_desc.dev);
	if (!info_v2->ramdump_dev) {
		cnss_pr_err("Failed to create ramdump device!\n");
		ret = -ENOMEM;
		goto free_ramdump;
	}

	return 0;

free_ramdump:
	kfree(info_v2->dump_data_vaddr);
	info_v2->dump_data_vaddr = NULL;
	return ret;
}

static void cnss_qca6290_unregister_ramdump(struct cnss_plat_data *plat_priv)
{
	struct cnss_ramdump_info_v2 *info_v2;

	info_v2 = &plat_priv->ramdump_info_v2;

	if (info_v2->ramdump_dev)
		destroy_ramdump_device(info_v2->ramdump_dev);

	kfree(info_v2->dump_data_vaddr);
	info_v2->dump_data_vaddr = NULL;
	info_v2->dump_data_valid = false;
}

int cnss_register_ramdump(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	switch (plat_priv->device_id) {
	case QCA6174_DEVICE_ID:
		ret = cnss_qca6174_register_ramdump(plat_priv);
		break;
	case QCA6290_EMULATION_DEVICE_ID:
	case QCA6290_DEVICE_ID:
		ret = cnss_qca6290_register_ramdump(plat_priv);
		break;
	default:
		cnss_pr_err("Unknown device ID: 0x%lx\n", plat_priv->device_id);
		ret = -ENODEV;
		break;
	}
	return ret;
}

void cnss_unregister_ramdump(struct cnss_plat_data *plat_priv)
{
	switch (plat_priv->device_id) {
	case QCA6174_DEVICE_ID:
		cnss_qca6174_unregister_ramdump(plat_priv);
		break;
	case QCA6290_EMULATION_DEVICE_ID:
	case QCA6290_DEVICE_ID:
		cnss_qca6290_unregister_ramdump(plat_priv);
		break;
	default:
		cnss_pr_err("Unknown device ID: 0x%lx\n", plat_priv->device_id);
		break;
	}
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

static ssize_t cnss_fs_ready_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf,
				   size_t count)
{
	int fs_ready = 0;
	struct cnss_plat_data *plat_priv = dev_get_drvdata(dev);

	if (sscanf(buf, "%du", &fs_ready) != 1)
		return -EINVAL;

	cnss_pr_dbg("File system is ready, fs_ready is %d, count is %zu\n",
		    fs_ready, count);

	if (qmi_bypass) {
		cnss_pr_dbg("QMI is bypassed.\n");
		return count;
	}

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return count;
	}

	switch (plat_priv->device_id) {
	case QCA6290_EMULATION_DEVICE_ID:
	case QCA6290_DEVICE_ID:
		break;
	default:
		cnss_pr_err("Not supported for device ID 0x%lx\n",
			    plat_priv->device_id);
		return count;
	}

	if (fs_ready == FILE_SYSTEM_READY) {
		cnss_driver_event_post(plat_priv,
				       CNSS_DRIVER_EVENT_COLD_BOOT_CAL_START,
				       true, NULL);
	}

	return count;
}

static DEVICE_ATTR(fs_ready, 0220, NULL, cnss_fs_ready_store);

static int cnss_create_sysfs(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	ret = device_create_file(&plat_priv->plat_dev->dev, &dev_attr_fs_ready);
	if (ret) {
		cnss_pr_err("Failed to create device file, err = %d\n", ret);
		goto out;
	}

	return 0;
out:
	return ret;
}

static void cnss_remove_sysfs(struct cnss_plat_data *plat_priv)
{
	device_remove_file(&plat_priv->plat_dev->dev, &dev_attr_fs_ready);
}

static int cnss_event_work_init(struct cnss_plat_data *plat_priv)
{
	spin_lock_init(&plat_priv->event_lock);
	plat_priv->event_wq = alloc_workqueue("cnss_driver_event",
					      WQ_UNBOUND, 1);
	if (!plat_priv->event_wq) {
		cnss_pr_err("Failed to create event workqueue!\n");
		return -EFAULT;
	}

	INIT_WORK(&plat_priv->event_work, cnss_driver_event_work);
	INIT_LIST_HEAD(&plat_priv->event_list);

	return 0;
}

static void cnss_event_work_deinit(struct cnss_plat_data *plat_priv)
{
	destroy_workqueue(plat_priv->event_wq);
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

	if (!test_bit(SKIP_DEVICE_BOOT, &quirks)) {
		ret = cnss_power_on_device(plat_priv);
		if (ret)
			goto free_res;

		ret = cnss_pci_init(plat_priv);
		if (ret)
			goto power_off;
	}

	ret = cnss_register_esoc(plat_priv);
	if (ret)
		goto deinit_pci;

	ret = cnss_register_bus_scale(plat_priv);
	if (ret)
		goto unreg_esoc;

	ret = cnss_create_sysfs(plat_priv);
	if (ret)
		goto unreg_bus_scale;

	ret = cnss_event_work_init(plat_priv);
	if (ret)
		goto remove_sysfs;

	ret = cnss_qmi_init(plat_priv);
	if (ret)
		goto deinit_event_work;

	ret = cnss_debugfs_create(plat_priv);
	if (ret)
		goto deinit_qmi;

	setup_timer(&plat_priv->fw_boot_timer,
		    fw_boot_timeout, (unsigned long)plat_priv);

	register_pm_notifier(&cnss_pm_notifier);

	ret = device_init_wakeup(&plat_dev->dev, true);
	if (ret)
		cnss_pr_err("Failed to init platform device wakeup source, err = %d\n",
			    ret);

	init_completion(&plat_priv->power_up_complete);

	cnss_pr_info("Platform driver probed successfully.\n");

	return 0;

deinit_qmi:
	cnss_qmi_deinit(plat_priv);
deinit_event_work:
	cnss_event_work_deinit(plat_priv);
remove_sysfs:
	cnss_remove_sysfs(plat_priv);
unreg_bus_scale:
	cnss_unregister_bus_scale(plat_priv);
unreg_esoc:
	cnss_unregister_esoc(plat_priv);
deinit_pci:
	if (!test_bit(SKIP_DEVICE_BOOT, &quirks))
		cnss_pci_deinit(plat_priv);
power_off:
	if (!test_bit(SKIP_DEVICE_BOOT, &quirks))
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

	complete_all(&plat_priv->power_up_complete);
	device_init_wakeup(&plat_dev->dev, false);
	unregister_pm_notifier(&cnss_pm_notifier);
	del_timer(&plat_priv->fw_boot_timer);
	cnss_debugfs_destroy(plat_priv);
	cnss_qmi_deinit(plat_priv);
	cnss_event_work_deinit(plat_priv);
	cnss_remove_sysfs(plat_priv);
	cnss_unregister_bus_scale(plat_priv);
	cnss_unregister_esoc(plat_priv);
	cnss_pci_deinit(plat_priv);
	cnss_put_resources(plat_priv);
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
