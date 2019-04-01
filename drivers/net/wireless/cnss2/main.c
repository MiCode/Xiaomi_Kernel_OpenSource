// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved. */

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
#include "bus.h"
#include "debug.h"
#include "genl.h"

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

#define CNSS_QUIRKS_DEFAULT		0
#ifdef CONFIG_CNSS_EMULATION
#define CNSS_MHI_TIMEOUT_DEFAULT	90000
#else
#define CNSS_MHI_TIMEOUT_DEFAULT	0
#endif
#define CNSS_QMI_TIMEOUT_DEFAULT	10000
#define CNSS_BDF_TYPE_DEFAULT		CNSS_BDF_ELF

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

struct cnss_driver_event {
	struct list_head list;
	enum cnss_driver_event_type type;
	bool sync;
	struct completion complete;
	int ret;
	void *data;
};

static void cnss_set_plat_priv(struct platform_device *plat_dev,
			       struct cnss_plat_data *plat_priv)
{
	plat_env = plat_priv;
}

struct cnss_plat_data *cnss_get_plat_priv(struct platform_device *plat_dev)
{
	return plat_env;
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
		ret = msm_bus_scale_client_update_request
			(bus_bw_info->bus_client, bandwidth);
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
	int ret = 0;

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		return 0;

	if (test_bit(QMI_BYPASS, &plat_priv->ctrl_params.quirks))
		return 0;

	if (!config || !host_version) {
		cnss_pr_err("Invalid config or host_version pointer\n");
		return -EINVAL;
	}

	cnss_pr_dbg("Mode: %d, config: %pK, host_version: %s\n",
		    mode, config, host_version);

	if (mode == CNSS_WALTEST || mode == CNSS_CCPM)
		goto skip_cfg;

	ret = cnss_wlfw_wlan_cfg_send_sync(plat_priv, config, host_version);
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

	if (test_bit(QMI_BYPASS, &plat_priv->ctrl_params.quirks))
		return 0;

	return cnss_wlfw_wlan_mode_send_sync(plat_priv, CNSS_OFF);
}
EXPORT_SYMBOL(cnss_wlan_disable);

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

int cnss_set_fw_log_mode(struct device *dev, u8 fw_log_mode)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		return 0;

	return cnss_wlfw_ini_send_sync(plat_priv, fw_log_mode);
}
EXPORT_SYMBOL(cnss_set_fw_log_mode);

static int cnss_fw_mem_ready_hdlr(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	set_bit(CNSS_FW_MEM_READY, &plat_priv->driver_state);

	ret = cnss_wlfw_tgt_cap_send_sync(plat_priv);
	if (ret)
		goto out;

	cnss_wlfw_bdf_dnld_send_sync(plat_priv, CNSS_BDF_REGDB);

	ret = cnss_wlfw_bdf_dnld_send_sync(plat_priv,
					   plat_priv->ctrl_params.bdf_type);
	if (ret)
		goto out;

	ret = cnss_bus_load_m3(plat_priv);
	if (ret)
		goto out;

	ret = cnss_wlfw_m3_dnld_send_sync(plat_priv);
	if (ret)
		goto out;

	return 0;
out:
	return ret;
}

static int cnss_request_antenna_sharing(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (!plat_priv->antenna) {
		ret = cnss_wlfw_antenna_switch_send_sync(plat_priv);
		if (ret)
			goto out;
	}

	if (test_bit(CNSS_COEX_CONNECTED, &plat_priv->driver_state)) {
		ret = coex_antenna_switch_to_wlan_send_sync_msg(plat_priv);
		if (ret)
			goto out;
	}

	ret = cnss_wlfw_antenna_grant_send_sync(plat_priv);
	if (ret)
		goto out;

	return 0;

out:
	return ret;
}

static void cnss_release_antenna_sharing(struct cnss_plat_data *plat_priv)
{
	if (test_bit(CNSS_COEX_CONNECTED, &plat_priv->driver_state))
		coex_antenna_switch_to_mdm_send_sync_msg(plat_priv);
}

static int cnss_fw_ready_hdlr(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	del_timer(&plat_priv->fw_boot_timer);
	set_bit(CNSS_FW_READY, &plat_priv->driver_state);
	clear_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state);

	if (test_bit(CNSS_FW_BOOT_RECOVERY, &plat_priv->driver_state)) {
		clear_bit(CNSS_FW_BOOT_RECOVERY, &plat_priv->driver_state);
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
	}

	if (test_bit(ENABLE_WALTEST, &plat_priv->ctrl_params.quirks)) {
		ret = cnss_wlfw_wlan_mode_send_sync(plat_priv,
						    CNSS_WALTEST);
	} else if (test_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state)) {
		cnss_request_antenna_sharing(plat_priv);
		ret = cnss_wlfw_wlan_mode_send_sync(plat_priv,
						    CNSS_CALIBRATION);
	} else if (test_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state) ||
		   test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		ret = cnss_bus_call_driver_probe(plat_priv);
	} else {
		complete(&plat_priv->power_up_complete);
	}

	if (ret && test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state))
		goto out;
	else if (ret)
		goto shutdown;

	return 0;

shutdown:
	cnss_bus_dev_shutdown(plat_priv);

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
	case CNSS_DRIVER_EVENT_QDSS_TRACE_REQ_MEM:
		return "QDSS_TRACE_REQ_MEM";
	case CNSS_DRIVER_EVENT_QDSS_TRACE_SAVE:
		return "QDSS_TRACE_SAVE";
	case CNSS_DRIVER_EVENT_QDSS_TRACE_FREE:
		return "QDSS_TRACE_FREE";
	case CNSS_DRIVER_EVENT_MAX:
		return "EVENT_MAX";
	}

	return "UNKNOWN";
};

int cnss_driver_event_post(struct cnss_plat_data *plat_priv,
			   enum cnss_driver_event_type type,
			   u32 flags, void *data)
{
	struct cnss_driver_event *event;
	unsigned long irq_flags;
	int gfp = GFP_KERNEL;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Posting event: %s(%d)%s, state: 0x%lx flags: 0x%0x\n",
		    cnss_driver_event_to_str(type), type,
		    flags ? "-sync" : "", plat_priv->driver_state, flags);

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
	event->sync = !!(flags & CNSS_EVENT_SYNC);

	spin_lock_irqsave(&plat_priv->event_lock, irq_flags);
	list_add_tail(&event->list, &plat_priv->event_list);
	spin_unlock_irqrestore(&plat_priv->event_lock, irq_flags);

	queue_work(plat_priv->event_wq, &plat_priv->event_work);

	if (!(flags & CNSS_EVENT_SYNC))
		goto out;

	if (flags & CNSS_EVENT_UNINTERRUPTIBLE)
		wait_for_completion(&event->complete);
	else
		ret = wait_for_completion_interruptible(&event->complete);

	cnss_pr_dbg("Completed event: %s(%d), state: 0x%lx, ret: %d/%d\n",
		    cnss_driver_event_to_str(type), type,
		    plat_priv->driver_state, ret, event->ret);
	spin_lock_irqsave(&plat_priv->event_lock, irq_flags);
	if (ret == -ERESTARTSYS && event->ret == CNSS_EVENT_PENDING) {
		event->sync = false;
		spin_unlock_irqrestore(&plat_priv->event_lock, irq_flags);
		ret = -EINTR;
		goto out;
	}
	spin_unlock_irqrestore(&plat_priv->event_lock, irq_flags);

	ret = event->ret;
	kfree(event);

out:
	cnss_pm_relax(plat_priv);
	return ret;
}

unsigned int cnss_get_boot_timeout(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return 0;
	}

	return cnss_get_qmi_timeout(plat_priv);
}
EXPORT_SYMBOL(cnss_get_boot_timeout);

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
				     CNSS_EVENT_SYNC, NULL);
	if (ret)
		goto out;

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		goto out;

	timeout = cnss_get_boot_timeout(dev);

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
				      CNSS_EVENT_SYNC, NULL);
}
EXPORT_SYMBOL(cnss_power_down);

static int cnss_get_resources(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	ret = cnss_get_vreg_type(plat_priv, CNSS_VREG_PRIM);
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
	struct cnss_esoc_info *esoc_info;

	cnss_pr_dbg("Modem notifier: event %lu\n", code);

	if (!plat_priv)
		return NOTIFY_DONE;

	esoc_info = &plat_priv->esoc_info;

	if (code == SUBSYS_AFTER_POWERUP)
		esoc_info->modem_current_status = 1;
	else if (code == SUBSYS_BEFORE_SHUTDOWN)
		esoc_info->modem_current_status = 0;
	else
		return NOTIFY_DONE;

	if (!cnss_bus_call_driver_modem_status(plat_priv,
					       esoc_info->modem_current_status))
		return NOTIFY_DONE;

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

	if (!esoc_info->notify_modem_status)
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
		subsys_notif_unregister_notifier
		(esoc_info->modem_notify_handler,
		 &plat_priv->modem_nb);
	if (esoc_info->esoc_desc)
		devm_unregister_esoc_client(dev, esoc_info->esoc_desc);
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

	return cnss_bus_dev_powerup(plat_priv);
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

	return cnss_bus_dev_shutdown(plat_priv);
}

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
		cnss_pr_err("plat_priv is NULL\n");
		return;
	}

	cnss_bus_dev_crash_shutdown(plat_priv);
}

static int cnss_subsys_ramdump(int enable,
			       const struct subsys_desc *subsys_desc)
{
	struct cnss_plat_data *plat_priv = dev_get_drvdata(subsys_desc->dev);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	if (!enable)
		return 0;

	return cnss_bus_dev_ramdump(plat_priv);
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
	struct cnss_subsys_info *subsys_info =
		&plat_priv->subsys_info;

	plat_priv->recovery_count++;

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		goto self_recovery;

	if (test_bit(SKIP_RECOVERY, &plat_priv->ctrl_params.quirks)) {
		cnss_pr_dbg("Skip device recovery\n");
		return 0;
	}

	switch (reason) {
	case CNSS_REASON_LINK_DOWN:
		if (test_bit(LINK_DOWN_SELF_RECOVERY,
			     &plat_priv->ctrl_params.quirks))
			goto self_recovery;
		break;
	case CNSS_REASON_RDDM:
		cnss_bus_collect_dump_info(plat_priv, false);
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
	cnss_bus_dev_shutdown(plat_priv);
	cnss_bus_dev_powerup(plat_priv);

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

	if (!plat_priv->driver_state) {
		cnss_pr_err("Improper driver state, ignore recovery\n");
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		cnss_pr_err("Recovery is already in progress\n");
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

	cnss_bus_update_status(plat_priv, CNSS_FW_DOWN);

	if (test_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state)) {
		cnss_pr_dbg("Driver unload is in progress, ignore schedule recovery\n");
		return;
	}

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	data = kzalloc(sizeof(*data), gfp);
	if (!data)
		return;

	data->reason = reason;
	cnss_driver_event_post(plat_priv,
			       CNSS_DRIVER_EVENT_RECOVERY,
			       0, data);
}
EXPORT_SYMBOL(cnss_schedule_recovery);

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

	if (cnss_pci_is_device_down(dev)) {
		cnss_pr_info("Device is already in bad state, ignore force assert\n");
		return 0;
	}

	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		cnss_pr_info("Recovery is already in progress, ignore forced FW assert\n");
		return 0;
	}

	cnss_driver_event_post(plat_priv,
			       CNSS_DRIVER_EVENT_FORCE_FW_ASSERT,
			       0, NULL);

	return 0;
}
EXPORT_SYMBOL(cnss_force_fw_assert);

int cnss_force_collect_rddm(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	int ret = 0;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	if (plat_priv->device_id == QCA6174_DEVICE_ID) {
		cnss_pr_info("Force collect rddm is not supported\n");
		return -EOPNOTSUPP;
	}

	if (cnss_pci_is_device_down(dev)) {
		cnss_pr_info("Device is already in bad state, ignore force collect rddm\n");
		return 0;
	}

	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		cnss_pr_info("Recovery is already in progress, ignore forced collect rddm\n");
		return 0;
	}

	if (test_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state)) {
		cnss_pr_info("Loading/Unloading is in progress, ignore forced collect rddm\n");
		return 0;
	}

	ret = cnss_bus_force_fw_assert_hdlr(plat_priv);
	if (ret)
		return ret;

	reinit_completion(&plat_priv->rddm_complete);
	ret = wait_for_completion_timeout
		(&plat_priv->rddm_complete,
		 msecs_to_jiffies(CNSS_RDDM_TIMEOUT_MS));
	if (!ret)
		ret = -ETIMEDOUT;

	return ret;
}
EXPORT_SYMBOL(cnss_force_collect_rddm);

static int cnss_cold_boot_cal_start_hdlr(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (test_bit(CNSS_FW_READY, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state)) {
		cnss_pr_dbg("Device is already active, ignore calibration\n");
		goto out;
	}

	set_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state);
	reinit_completion(&plat_priv->cal_complete);
	ret = cnss_bus_dev_powerup(plat_priv);
	if (ret) {
		complete(&plat_priv->cal_complete);
		clear_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state);
	}

out:
	return ret;
}

static int cnss_cold_boot_cal_done_hdlr(struct cnss_plat_data *plat_priv)
{
	if (!test_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state))
		return 0;

	plat_priv->cal_done = true;
	cnss_wlfw_wlan_mode_send_sync(plat_priv, CNSS_OFF);
	cnss_release_antenna_sharing(plat_priv);
	cnss_bus_dev_shutdown(plat_priv);
	complete(&plat_priv->cal_complete);
	clear_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state);

	return 0;
}

static int cnss_power_up_hdlr(struct cnss_plat_data *plat_priv)
{
	return cnss_bus_dev_powerup(plat_priv);
}

static int cnss_power_down_hdlr(struct cnss_plat_data *plat_priv)
{
	cnss_bus_dev_shutdown(plat_priv);

	return 0;
}

static int cnss_qdss_trace_req_mem_hdlr(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	ret = cnss_bus_alloc_qdss_mem(plat_priv);
	if (ret < 0)
		return ret;

	return cnss_wlfw_qdss_trace_mem_info_send_sync(plat_priv);
}

static void *cnss_qdss_trace_pa_to_va(struct cnss_plat_data *plat_priv,
				      u64 pa, u32 size, int *seg_id)
{
	int i = 0;
	struct cnss_fw_mem *qdss_mem = plat_priv->qdss_mem;
	u64 offset = 0;
	void *va = NULL;
	u64 local_pa;
	u32 local_size;

	for (i = 0; i < plat_priv->qdss_mem_seg_len; i++) {
		local_pa = (u64)qdss_mem[i].pa;
		local_size = (u32)qdss_mem[i].size;
		if (pa == local_pa && size <= local_size) {
			va = qdss_mem[i].va;
			break;
		}
		if (pa > local_pa &&
		    pa < local_pa + local_size &&
		    pa + size <= local_pa + local_size) {
			offset = pa - local_pa;
			va = qdss_mem[i].va + offset;
			break;
		}
	}

	*seg_id = i;
	return va;
}

static int cnss_qdss_trace_save_hdlr(struct cnss_plat_data *plat_priv,
				     void *data)
{
	struct cnss_qmi_event_qdss_trace_save_data *event_data = data;
	struct cnss_fw_mem *qdss_mem = plat_priv->qdss_mem;
	int ret = 0;
	int i;
	void *va = NULL;
	u64 pa;
	u32 size;
	int seg_id = 0;

	if (!plat_priv->qdss_mem_seg_len) {
		cnss_pr_err("Memory for QDSS trace is not available\n");
		return -ENOMEM;
	}

	if (event_data->mem_seg_len == 0) {
		for (i = 0; i < plat_priv->qdss_mem_seg_len; i++) {
			ret = cnss_genl_send_msg(qdss_mem[i].va,
						 CNSS_GENL_MSG_TYPE_QDSS,
						 event_data->file_name,
						 qdss_mem[i].size);
			if (ret < 0) {
				cnss_pr_err("Fail to save QDSS data: %d\n",
					    ret);
				break;
			}
		}
	} else {
		for (i = 0; i < event_data->mem_seg_len; i++) {
			pa = event_data->mem_seg[i].addr;
			size = event_data->mem_seg[i].size;
			va = cnss_qdss_trace_pa_to_va(plat_priv, pa,
						      size, &seg_id);
			if (!va) {
				cnss_pr_err("Fail to find matching va for pa %pa\n",
					    &pa);
				ret = -EINVAL;
				break;
			}
			ret = cnss_genl_send_msg(va, CNSS_GENL_MSG_TYPE_QDSS,
						 event_data->file_name, size);
			if (ret < 0) {
				cnss_pr_err("Fail to save QDSS data: %d\n",
					    ret);
				break;
			}
		}
	}

	kfree(data);
	return ret;
}

static int cnss_qdss_trace_free_hdlr(struct cnss_plat_data *plat_priv)
{
	cnss_bus_free_qdss_mem(plat_priv);

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
			ret = cnss_wlfw_server_arrive(plat_priv, event->data);
			break;
		case CNSS_DRIVER_EVENT_SERVER_EXIT:
			ret = cnss_wlfw_server_exit(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_REQUEST_MEM:
			ret = cnss_bus_alloc_fw_mem(plat_priv);
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
			ret = cnss_bus_register_driver_hdlr(plat_priv,
							    event->data);
			break;
		case CNSS_DRIVER_EVENT_UNREGISTER_DRIVER:
			ret = cnss_bus_unregister_driver_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_RECOVERY:
			ret = cnss_driver_recovery_hdlr(plat_priv,
							event->data);
			break;
		case CNSS_DRIVER_EVENT_FORCE_FW_ASSERT:
			ret = cnss_bus_force_fw_assert_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_POWER_UP:
			ret = cnss_power_up_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_POWER_DOWN:
			ret = cnss_power_down_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_QDSS_TRACE_REQ_MEM:
			ret = cnss_qdss_trace_req_mem_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_QDSS_TRACE_SAVE:
			ret = cnss_qdss_trace_save_hdlr(plat_priv,
							event->data);
			break;
		case CNSS_DRIVER_EVENT_QDSS_TRACE_FREE:
			ret = cnss_qdss_trace_free_hdlr(plat_priv);
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

	subsys_info->subsys_desc.name = "wlan";
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

static int cnss_register_ramdump_v1(struct cnss_plat_data *plat_priv)
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
		ramdump_info->ramdump_va =
			dma_alloc_coherent(dev, ramdump_size,
					   &ramdump_info->ramdump_pa,
					   GFP_KERNEL);

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

	ramdump_info->ramdump_dev =
		create_ramdump_device(subsys_info->subsys_desc.name,
				      subsys_info->subsys_desc.dev);
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

static void cnss_unregister_ramdump_v1(struct cnss_plat_data *plat_priv)
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

static int cnss_register_ramdump_v2(struct cnss_plat_data *plat_priv)
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

static void cnss_unregister_ramdump_v2(struct cnss_plat_data *plat_priv)
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
		ret = cnss_register_ramdump_v1(plat_priv);
		break;
	case QCA6290_DEVICE_ID:
	case QCA6390_DEVICE_ID:
		ret = cnss_register_ramdump_v2(plat_priv);
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
		cnss_unregister_ramdump_v1(plat_priv);
		break;
	case QCA6290_DEVICE_ID:
	case QCA6390_DEVICE_ID:
		cnss_unregister_ramdump_v2(plat_priv);
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
			msm_bus_scale_register_client
			(bus_bw_info->bus_scale_table);
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

static ssize_t fs_ready_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int fs_ready = 0;
	struct cnss_plat_data *plat_priv = dev_get_drvdata(dev);

	if (sscanf(buf, "%du", &fs_ready) != 1)
		return -EINVAL;

	cnss_pr_dbg("File system is ready, fs_ready is %d, count is %zu\n",
		    fs_ready, count);

	if (test_bit(QMI_BYPASS, &plat_priv->ctrl_params.quirks)) {
		cnss_pr_dbg("QMI is bypassed.\n");
		return count;
	}

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return count;
	}

	switch (plat_priv->device_id) {
	case QCA6290_DEVICE_ID:
	case QCA6390_DEVICE_ID:
		break;
	default:
		cnss_pr_err("Not supported for device ID 0x%lx\n",
			    plat_priv->device_id);
		return count;
	}

	if (fs_ready == FILE_SYSTEM_READY) {
		cnss_driver_event_post(plat_priv,
				       CNSS_DRIVER_EVENT_COLD_BOOT_CAL_START,
				       CNSS_EVENT_SYNC, NULL);
	}

	return count;
}

static DEVICE_ATTR_WO(fs_ready);

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

static int cnss_misc_init(struct cnss_plat_data *plat_priv)
{
	int ret;

	timer_setup(&plat_priv->fw_boot_timer,
		    cnss_bus_fw_boot_timeout_hdlr, 0);

	register_pm_notifier(&cnss_pm_notifier);

	ret = device_init_wakeup(&plat_priv->plat_dev->dev, true);
	if (ret)
		cnss_pr_err("Failed to init platform device wakeup source, err = %d\n",
			    ret);

	init_completion(&plat_priv->power_up_complete);
	init_completion(&plat_priv->cal_complete);
	init_completion(&plat_priv->rddm_complete);
	init_completion(&plat_priv->recovery_complete);
	mutex_init(&plat_priv->dev_lock);

	return 0;
}

static void cnss_misc_deinit(struct cnss_plat_data *plat_priv)
{
	complete_all(&plat_priv->recovery_complete);
	complete_all(&plat_priv->rddm_complete);
	complete_all(&plat_priv->cal_complete);
	complete_all(&plat_priv->power_up_complete);
	device_init_wakeup(&plat_priv->plat_dev->dev, false);
	unregister_pm_notifier(&cnss_pm_notifier);
	del_timer(&plat_priv->fw_boot_timer);
}

static void cnss_init_control_params(struct cnss_plat_data *plat_priv)
{
	plat_priv->ctrl_params.quirks = CNSS_QUIRKS_DEFAULT;
	plat_priv->ctrl_params.mhi_timeout = CNSS_MHI_TIMEOUT_DEFAULT;
	plat_priv->ctrl_params.qmi_timeout = CNSS_QMI_TIMEOUT_DEFAULT;
	plat_priv->ctrl_params.bdf_type = CNSS_BDF_TYPE_DEFAULT;
}

static const struct platform_device_id cnss_platform_id_table[] = {
	{ .name = "qca6174", .driver_data = QCA6174_DEVICE_ID, },
	{ .name = "qca6290", .driver_data = QCA6290_DEVICE_ID, },
	{ .name = "qca6390", .driver_data = QCA6390_DEVICE_ID, },
	{ },
};

static const struct of_device_id cnss_of_match_table[] = {
	{
		.compatible = "qcom,cnss",
		.data = (void *)&cnss_platform_id_table[0]},
	{
		.compatible = "qcom,cnss-qca6290",
		.data = (void *)&cnss_platform_id_table[1]},
	{
		.compatible = "qcom,cnss-qca6390",
		.data = (void *)&cnss_platform_id_table[2]},
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
	plat_priv->bus_type = cnss_get_bus_type(plat_priv->device_id);
	cnss_set_plat_priv(plat_dev, plat_priv);
	platform_set_drvdata(plat_dev, plat_priv);
	INIT_LIST_HEAD(&plat_priv->vreg_list);

	cnss_init_control_params(plat_priv);

	ret = cnss_get_resources(plat_priv);
	if (ret)
		goto reset_ctx;

	if (!test_bit(SKIP_DEVICE_BOOT, &plat_priv->ctrl_params.quirks)) {
		ret = cnss_power_on_device(plat_priv);
		if (ret)
			goto free_res;

		ret = cnss_bus_init(plat_priv);
		if (ret)
			goto power_off;
	}

	ret = cnss_register_esoc(plat_priv);
	if (ret)
		goto deinit_bus;

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

	ret = cnss_misc_init(plat_priv);
	if (ret)
		goto destroy_debugfs;

	cnss_register_coex_service(plat_priv);

	ret = cnss_genl_init();
	if (ret < 0)
		cnss_pr_err("CNSS genl init failed %d\n", ret);

	cnss_pr_info("Platform driver probed successfully.\n");

	return 0;

destroy_debugfs:
	cnss_debugfs_destroy(plat_priv);
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
deinit_bus:
	if (!test_bit(SKIP_DEVICE_BOOT, &plat_priv->ctrl_params.quirks))
		cnss_bus_deinit(plat_priv);
power_off:
	if (!test_bit(SKIP_DEVICE_BOOT, &plat_priv->ctrl_params.quirks))
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

	cnss_genl_exit();
	cnss_unregister_coex_service(plat_priv);
	cnss_misc_deinit(plat_priv);
	cnss_debugfs_destroy(plat_priv);
	cnss_qmi_deinit(plat_priv);
	cnss_event_work_deinit(plat_priv);
	cnss_remove_sysfs(plat_priv);
	cnss_unregister_bus_scale(plat_priv);
	cnss_unregister_esoc(plat_priv);
	cnss_bus_deinit(plat_priv);
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
		.of_match_table = cnss_of_match_table,
#ifdef CONFIG_CNSS_ASYNC
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
#endif
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
