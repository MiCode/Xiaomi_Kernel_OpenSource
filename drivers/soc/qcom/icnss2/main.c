// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2020, 2021, The Linux Foundation.
 * All rights reserved.
 */

#define pr_fmt(fmt) "icnss2: " fmt

#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/iommu.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/thread_info.h>
#include <linux/uaccess.h>
#include <linux/adc-tm-clients.h>
#include <linux/iio/consumer.h>
#include <linux/etherdevice.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/pm_runtime.h>
#include <linux/soc/qcom/qmi.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <soc/qcom/memory_dump.h>
#include <soc/qcom/secure_buffer.h>
#include <soc/qcom/socinfo.h>
#include <soc/qcom/ramdump.h>
#include <linux/soc/qcom/smem.h>
#include <linux/soc/qcom/smem_state.h>
#include <linux/remoteproc.h>
#include <linux/remoteproc/qcom_rproc.h>
#include <linux/soc/qcom/pdr.h>
#include "main.h"
#include "qmi.h"
#include "debug.h"
#include "power.h"
#include "genl.h"

#define MAX_PROP_SIZE			32
#define NUM_LOG_PAGES			10
#define NUM_LOG_LONG_PAGES		4
#define ICNSS_MAGIC			0x5abc5abc

#define ICNSS_WLAN_SERVICE_NAME					"wlan/fw"
#define ICNSS_WLANPD_NAME					"msm/modem/wlan_pd"
#define ICNSS_DEFAULT_FEATURE_MASK 0x01

#define ICNSS_M3_SEGMENT(segment)		"wcnss_"segment
#define ICNSS_M3_SEGMENT_PHYAREG		"phyareg"
#define ICNSS_M3_SEGMENT_PHYA			"phydbg"
#define ICNSS_M3_SEGMENT_WMACREG		"wmac0reg"
#define ICNSS_M3_SEGMENT_WCSSDBG		"WCSSDBG"
#define ICNSS_M3_SEGMENT_PHYAM3			"PHYAPDMEM"

#define ICNSS_QUIRKS_DEFAULT		BIT(FW_REJUVENATE_ENABLE)
#define ICNSS_MAX_PROBE_CNT		2

#define ICNSS_BDF_TYPE_DEFAULT         ICNSS_BDF_ELF

#define PROBE_TIMEOUT                 15000

#ifdef CONFIG_ICNSS2_DEBUG
static unsigned long qmi_timeout = 3000;
module_param(qmi_timeout, ulong, 0600);
#define WLFW_TIMEOUT                    msecs_to_jiffies(qmi_timeout)
#else
#define WLFW_TIMEOUT                    msecs_to_jiffies(3000)
#endif

static struct icnss_priv *penv;
static struct work_struct wpss_loader;
uint64_t dynamic_feature_mask = ICNSS_DEFAULT_FEATURE_MASK;

#define ICNSS_EVENT_PENDING			2989

#define ICNSS_EVENT_SYNC			BIT(0)
#define ICNSS_EVENT_UNINTERRUPTIBLE		BIT(1)
#define ICNSS_EVENT_SYNC_UNINTERRUPTIBLE	(ICNSS_EVENT_UNINTERRUPTIBLE | \
						 ICNSS_EVENT_SYNC)
#define ICNSS_DMS_QMI_CONNECTION_WAIT_MS 50
#define ICNSS_DMS_QMI_CONNECTION_WAIT_RETRY 200

enum icnss_pdr_cause_index {
	ICNSS_FW_CRASH,
	ICNSS_ROOT_PD_CRASH,
	ICNSS_ROOT_PD_SHUTDOWN,
	ICNSS_HOST_ERROR,
};

static const char * const icnss_pdr_cause[] = {
	[ICNSS_FW_CRASH] = "FW crash",
	[ICNSS_ROOT_PD_CRASH] = "Root PD crashed",
	[ICNSS_ROOT_PD_SHUTDOWN] = "Root PD shutdown",
	[ICNSS_HOST_ERROR] = "Host error",
};

static void icnss_set_plat_priv(struct icnss_priv *priv)
{
	penv = priv;
}

static struct icnss_priv *icnss_get_plat_priv()
{
	return penv;
}

static ssize_t icnss_sysfs_store(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	struct icnss_priv *priv = icnss_get_plat_priv();

	atomic_set(&priv->is_shutdown, true);
	icnss_pr_dbg("Received shutdown indication");
	return count;
}

static struct kobj_attribute icnss_sysfs_attribute =
__ATTR(shutdown, 0660, NULL, icnss_sysfs_store);

static void icnss_pm_stay_awake(struct icnss_priv *priv)
{
	if (atomic_inc_return(&priv->pm_count) != 1)
		return;

	icnss_pr_vdbg("PM stay awake, state: 0x%lx, count: %d\n", priv->state,
		     atomic_read(&priv->pm_count));

	pm_stay_awake(&priv->pdev->dev);

	priv->stats.pm_stay_awake++;
}

static void icnss_pm_relax(struct icnss_priv *priv)
{
	int r = atomic_dec_return(&priv->pm_count);

	WARN_ON(r < 0);

	if (r != 0)
		return;

	icnss_pr_vdbg("PM relax, state: 0x%lx, count: %d\n", priv->state,
		     atomic_read(&priv->pm_count));

	pm_relax(&priv->pdev->dev);
	priv->stats.pm_relax++;
}

char *icnss_driver_event_to_str(enum icnss_driver_event_type type)
{
	switch (type) {
	case ICNSS_DRIVER_EVENT_SERVER_ARRIVE:
		return "SERVER_ARRIVE";
	case ICNSS_DRIVER_EVENT_SERVER_EXIT:
		return "SERVER_EXIT";
	case ICNSS_DRIVER_EVENT_FW_READY_IND:
		return "FW_READY";
	case ICNSS_DRIVER_EVENT_REGISTER_DRIVER:
		return "REGISTER_DRIVER";
	case ICNSS_DRIVER_EVENT_UNREGISTER_DRIVER:
		return "UNREGISTER_DRIVER";
	case ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN:
		return "PD_SERVICE_DOWN";
	case ICNSS_DRIVER_EVENT_FW_EARLY_CRASH_IND:
		return "FW_EARLY_CRASH_IND";
	case ICNSS_DRIVER_EVENT_IDLE_SHUTDOWN:
		return "IDLE_SHUTDOWN";
	case ICNSS_DRIVER_EVENT_IDLE_RESTART:
		return "IDLE_RESTART";
	case ICNSS_DRIVER_EVENT_FW_INIT_DONE_IND:
		return "FW_INIT_DONE";
	case ICNSS_DRIVER_EVENT_QDSS_TRACE_REQ_MEM:
		return "QDSS_TRACE_REQ_MEM";
	case ICNSS_DRIVER_EVENT_QDSS_TRACE_SAVE:
		return "QDSS_TRACE_SAVE";
	case ICNSS_DRIVER_EVENT_QDSS_TRACE_FREE:
		return "QDSS_TRACE_FREE";
	case ICNSS_DRIVER_EVENT_M3_DUMP_UPLOAD_REQ:
		return "M3_DUMP_UPLOAD";
	case ICNSS_DRIVER_EVENT_QDSS_TRACE_REQ_DATA:
		return "QDSS_TRACE_REQ_DATA";
	case ICNSS_DRIVER_EVENT_MAX:
		return "EVENT_MAX";
	}

	return "UNKNOWN";
};

char *icnss_soc_wake_event_to_str(enum icnss_soc_wake_event_type type)
{
	switch (type) {
	case ICNSS_SOC_WAKE_REQUEST_EVENT:
		return "SOC_WAKE_REQUEST";
	case ICNSS_SOC_WAKE_RELEASE_EVENT:
		return "SOC_WAKE_RELEASE";
	case ICNSS_SOC_WAKE_EVENT_MAX:
		return "SOC_EVENT_MAX";
	}

	return "UNKNOWN";
};

int icnss_driver_event_post(struct icnss_priv *priv,
			    enum icnss_driver_event_type type,
			    u32 flags, void *data)
{
	struct icnss_driver_event *event;
	unsigned long irq_flags;
	int gfp = GFP_KERNEL;
	int ret = 0;

	if (!priv)
		return -ENODEV;

	icnss_pr_dbg("Posting event: %s(%d), %s, flags: 0x%x, state: 0x%lx\n",
		     icnss_driver_event_to_str(type), type, current->comm,
		     flags, priv->state);

	if (type >= ICNSS_DRIVER_EVENT_MAX) {
		icnss_pr_err("Invalid Event type: %d, can't post", type);
		return -EINVAL;
	}

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	event = kzalloc(sizeof(*event), gfp);
	if (event == NULL)
		return -ENOMEM;

	icnss_pm_stay_awake(priv);

	event->type = type;
	event->data = data;
	init_completion(&event->complete);
	event->ret = ICNSS_EVENT_PENDING;
	event->sync = !!(flags & ICNSS_EVENT_SYNC);

	spin_lock_irqsave(&priv->event_lock, irq_flags);
	list_add_tail(&event->list, &priv->event_list);
	spin_unlock_irqrestore(&priv->event_lock, irq_flags);

	priv->stats.events[type].posted++;
	queue_work(priv->event_wq, &priv->event_work);

	if (!(flags & ICNSS_EVENT_SYNC))
		goto out;

	if (flags & ICNSS_EVENT_UNINTERRUPTIBLE)
		wait_for_completion(&event->complete);
	else
		ret = wait_for_completion_interruptible(&event->complete);

	icnss_pr_dbg("Completed event: %s(%d), state: 0x%lx, ret: %d/%d\n",
		     icnss_driver_event_to_str(type), type, priv->state, ret,
		     event->ret);

	spin_lock_irqsave(&priv->event_lock, irq_flags);
	if (ret == -ERESTARTSYS && event->ret == ICNSS_EVENT_PENDING) {
		event->sync = false;
		spin_unlock_irqrestore(&priv->event_lock, irq_flags);
		ret = -EINTR;
		goto out;
	}
	spin_unlock_irqrestore(&priv->event_lock, irq_flags);

	ret = event->ret;
	kfree(event);

out:
	icnss_pm_relax(priv);
	return ret;
}

int icnss_soc_wake_event_post(struct icnss_priv *priv,
			      enum icnss_soc_wake_event_type type,
			      u32 flags, void *data)
{
	struct icnss_soc_wake_event *event;
	unsigned long irq_flags;
	int gfp = GFP_KERNEL;
	int ret = 0;

	if (!priv)
		return -ENODEV;

	icnss_pr_soc_wake("Posting event: %s(%d), %s, flags: 0x%x, state: 0x%lx\n",
			  icnss_soc_wake_event_to_str(type),
			  type, current->comm, flags, priv->state);

	if (type >= ICNSS_SOC_WAKE_EVENT_MAX) {
		icnss_pr_err("Invalid Event type: %d, can't post", type);
		return -EINVAL;
	}

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	event = kzalloc(sizeof(*event), gfp);
	if (!event)
		return -ENOMEM;

	icnss_pm_stay_awake(priv);

	event->type = type;
	event->data = data;
	init_completion(&event->complete);
	event->ret = ICNSS_EVENT_PENDING;
	event->sync = !!(flags & ICNSS_EVENT_SYNC);

	spin_lock_irqsave(&priv->soc_wake_msg_lock, irq_flags);
	list_add_tail(&event->list, &priv->soc_wake_msg_list);
	spin_unlock_irqrestore(&priv->soc_wake_msg_lock, irq_flags);

	priv->stats.soc_wake_events[type].posted++;
	queue_work(priv->soc_wake_wq, &priv->soc_wake_msg_work);

	if (!(flags & ICNSS_EVENT_SYNC))
		goto out;

	if (flags & ICNSS_EVENT_UNINTERRUPTIBLE)
		wait_for_completion(&event->complete);
	else
		ret = wait_for_completion_interruptible(&event->complete);

	icnss_pr_soc_wake("Completed event: %s(%d), state: 0x%lx, ret: %d/%d\n",
			  icnss_soc_wake_event_to_str(type),
			  type, priv->state, ret, event->ret);

	spin_lock_irqsave(&priv->soc_wake_msg_lock, irq_flags);
	if (ret == -ERESTARTSYS && event->ret == ICNSS_EVENT_PENDING) {
		event->sync = false;
		spin_unlock_irqrestore(&priv->soc_wake_msg_lock, irq_flags);
		ret = -EINTR;
		goto out;
	}
	spin_unlock_irqrestore(&priv->soc_wake_msg_lock, irq_flags);

	ret = event->ret;
	kfree(event);

out:
	icnss_pm_relax(priv);
	return ret;
}

bool icnss_is_fw_ready(void)
{
	if (!penv)
		return false;
	else
		return test_bit(ICNSS_FW_READY, &penv->state);
}
EXPORT_SYMBOL(icnss_is_fw_ready);

void icnss_block_shutdown(bool status)
{
	if (!penv)
		return;

	if (status) {
		set_bit(ICNSS_BLOCK_SHUTDOWN, &penv->state);
		reinit_completion(&penv->unblock_shutdown);
	} else {
		clear_bit(ICNSS_BLOCK_SHUTDOWN, &penv->state);
		complete(&penv->unblock_shutdown);
	}
}
EXPORT_SYMBOL(icnss_block_shutdown);

bool icnss_is_fw_down(void)
{

	struct icnss_priv *priv = icnss_get_plat_priv();

	if (!priv)
		return false;

	return test_bit(ICNSS_FW_DOWN, &priv->state) ||
		test_bit(ICNSS_PD_RESTART, &priv->state) ||
		test_bit(ICNSS_REJUVENATE, &priv->state);
}
EXPORT_SYMBOL(icnss_is_fw_down);

bool icnss_is_rejuvenate(void)
{
	if (!penv)
		return false;
	else
		return test_bit(ICNSS_REJUVENATE, &penv->state);
}
EXPORT_SYMBOL(icnss_is_rejuvenate);

bool icnss_is_pdr(void)
{
	if (!penv)
		return false;
	else
		return test_bit(ICNSS_PDR, &penv->state);
}
EXPORT_SYMBOL(icnss_is_pdr);

static irqreturn_t fw_error_fatal_handler(int irq, void *ctx)
{
	struct icnss_priv *priv = ctx;

	if (priv)
		priv->force_err_fatal = true;

	icnss_pr_err("Received force error fatal request from FW\n");

	return IRQ_HANDLED;
}

static irqreturn_t fw_crash_indication_handler(int irq, void *ctx)
{
	struct icnss_priv *priv = ctx;
	struct icnss_uevent_fw_down_data fw_down_data = {0};

	icnss_pr_err("Received early crash indication from FW\n");

	if (priv) {
		set_bit(ICNSS_FW_DOWN, &priv->state);
		icnss_ignore_fw_timeout(true);

		if (test_bit(ICNSS_FW_READY, &priv->state)) {
			clear_bit(ICNSS_FW_READY, &priv->state);
			fw_down_data.crashed = true;
			icnss_call_driver_uevent(priv, ICNSS_UEVENT_FW_DOWN,
						 &fw_down_data);
		}
	}

	icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_FW_EARLY_CRASH_IND,
				0, NULL);

	return IRQ_HANDLED;
}

static void register_fw_error_notifications(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	struct device_node *dev_node;
	int irq = 0, ret = 0;

	if (!priv)
		return;

	dev_node = of_find_node_by_name(NULL, "qcom,smp2p_map_wlan_1_in");
	if (!dev_node) {
		icnss_pr_err("Failed to get smp2p node for force-fatal-error\n");
		return;
	}

	icnss_pr_dbg("smp2p node->name=%s\n", dev_node->name);

	if (strcmp("qcom,smp2p_map_wlan_1_in", dev_node->name) == 0) {
		ret = irq = of_irq_get_byname(dev_node,
					      "qcom,smp2p-force-fatal-error");
		if (ret < 0) {
			icnss_pr_err("Unable to get force-fatal-error irq %d\n",
				     irq);
			return;
		}
	}

	ret = devm_request_threaded_irq(dev, irq, NULL, fw_error_fatal_handler,
					IRQF_ONESHOT | IRQF_TRIGGER_RISING,
					"wlanfw-err", priv);
	if (ret < 0) {
		icnss_pr_err("Unable to register for error fatal IRQ handler %d ret = %d",
			     irq, ret);
		return;
	}
	icnss_pr_dbg("FW force error fatal handler registered irq = %d\n", irq);
	priv->fw_error_fatal_irq = irq;
}

static void register_early_crash_notifications(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	struct device_node *dev_node;
	int irq = 0, ret = 0;

	if (!priv)
		return;

	dev_node = of_find_node_by_name(NULL, "qcom,smp2p_map_wlan_1_in");
	if (!dev_node) {
		icnss_pr_err("Failed to get smp2p node for early-crash-ind\n");
		return;
	}

	icnss_pr_dbg("smp2p node->name=%s\n", dev_node->name);

	if (strcmp("qcom,smp2p_map_wlan_1_in", dev_node->name) == 0) {
		ret = irq = of_irq_get_byname(dev_node,
					      "qcom,smp2p-early-crash-ind");
		if (ret < 0) {
			icnss_pr_err("Unable to get early-crash-ind irq %d\n",
				     irq);
			return;
		}
	}

	ret = devm_request_threaded_irq(dev, irq, NULL,
					fw_crash_indication_handler,
					IRQF_ONESHOT | IRQF_TRIGGER_RISING,
					"wlanfw-early-crash-ind", priv);
	if (ret < 0) {
		icnss_pr_err("Unable to register for early crash indication IRQ handler %d ret = %d",
			     irq, ret);
		return;
	}
	icnss_pr_dbg("FW crash indication handler registered irq = %d\n", irq);
	priv->fw_early_crash_irq = irq;
}

int icnss_call_driver_uevent(struct icnss_priv *priv,
				    enum icnss_uevent uevent, void *data)
{
	struct icnss_uevent_data uevent_data;

	if (!priv->ops || !priv->ops->uevent)
		return 0;

	icnss_pr_dbg("Calling driver uevent state: 0x%lx, uevent: %d\n",
		     priv->state, uevent);

	uevent_data.uevent = uevent;
	uevent_data.data = data;

	return priv->ops->uevent(&priv->pdev->dev, &uevent_data);
}

static int icnss_setup_dms_mac(struct icnss_priv *priv)
{
	int i;
	int ret = 0;

	ret = icnss_qmi_get_dms_mac(priv);
	if (ret == 0 && priv->dms.mac_valid)
		goto qmi_send;

	/* DTSI property use-nv-mac is used to force DMS MAC address for WLAN.
	 * Thus assert on failure to get MAC from DMS even after retries
	 */
	if (priv->use_nv_mac) {
		for (i = 0; i < ICNSS_DMS_QMI_CONNECTION_WAIT_RETRY; i++) {
			if (priv->dms.mac_valid)
				break;

			ret = icnss_qmi_get_dms_mac(priv);
			if (ret != -EAGAIN)
				break;
			msleep(ICNSS_DMS_QMI_CONNECTION_WAIT_MS);
		}
		if (!priv->dms.nv_mac_not_prov && !priv->dms.mac_valid) {
			icnss_pr_err("Unable to get MAC from DMS after retries\n");
			ICNSS_ASSERT(0);
			return -EINVAL;
		}
	}
qmi_send:
	if (priv->dms.mac_valid)
		ret =
		icnss_wlfw_wlan_mac_req_send_sync(priv, priv->dms.mac,
						  ARRAY_SIZE(priv->dms.mac));
	return ret;
}

static int icnss_driver_event_server_arrive(struct icnss_priv *priv,
						 void *data)
{
	int ret = 0;
	bool ignore_assert = false;

	if (!priv)
		return -ENODEV;

	set_bit(ICNSS_WLFW_EXISTS, &priv->state);
	clear_bit(ICNSS_FW_DOWN, &priv->state);
	clear_bit(ICNSS_FW_READY, &priv->state);

	icnss_ignore_fw_timeout(false);

	if (test_bit(ICNSS_WLFW_CONNECTED, &priv->state)) {
		icnss_pr_err("QMI Server already in Connected State\n");
		ICNSS_ASSERT(0);
	}

	ret = icnss_connect_to_fw_server(priv, data);
	if (ret)
		goto fail;

	set_bit(ICNSS_WLFW_CONNECTED, &priv->state);

	ret = wlfw_ind_register_send_sync_msg(priv);
	if (ret < 0) {
		if (ret == -EALREADY) {
			ret = 0;
			goto qmi_registered;
		}
		ignore_assert = true;
		goto fail;
	}

	if (priv->device_id == WCN6750_DEVICE_ID) {
		ret = wlfw_host_cap_send_sync(priv);
		if (ret < 0)
			goto fail;
	}

	if (priv->device_id == ADRASTEA_DEVICE_ID) {
		if (!priv->msa_va) {
			icnss_pr_err("Invalid MSA address\n");
			ret = -EINVAL;
			goto fail;
		}

		ret = wlfw_msa_mem_info_send_sync_msg(priv);
		if (ret < 0) {
			ignore_assert = true;
			goto fail;
		}

		ret = wlfw_msa_ready_send_sync_msg(priv);
		if (ret < 0) {
			ignore_assert = true;
			goto fail;
		}
	}

	ret = wlfw_cap_send_sync_msg(priv);
	if (ret < 0) {
		ignore_assert = true;
		goto fail;
	}

	ret = icnss_hw_power_on(priv);
	if (ret)
		goto fail;

	if (priv->device_id == WCN6750_DEVICE_ID) {
		ret = wlfw_device_info_send_msg(priv);
		if (ret < 0) {
			ignore_assert = true;
			goto  device_info_failure;
		}

		priv->mem_base_va = devm_ioremap(&priv->pdev->dev,
						 priv->mem_base_pa,
						 priv->mem_base_size);
		if (!priv->mem_base_va) {
			icnss_pr_err("Ioremap failed for bar address\n");
			goto device_info_failure;
		}

		icnss_pr_dbg("Non-Secured Bar Address pa: %pa, va: 0x%pK\n",
			     &priv->mem_base_pa,
			     priv->mem_base_va);

		if (priv->mhi_state_info_pa)
			priv->mhi_state_info_va = devm_ioremap(&priv->pdev->dev,
						priv->mhi_state_info_pa,
						PAGE_SIZE);
		if (!priv->mhi_state_info_va)
			icnss_pr_err("Ioremap failed for MHI info address\n");

		icnss_pr_dbg("MHI state info Address pa: %pa, va: 0x%pK\n",
			     &priv->mhi_state_info_pa,
			     priv->mhi_state_info_va);

		icnss_wlfw_bdf_dnld_send_sync(priv, ICNSS_BDF_REGDB);

		ret = icnss_wlfw_bdf_dnld_send_sync(priv,
						    priv->ctrl_params.bdf_type);
	}

	if (priv->device_id == ADRASTEA_DEVICE_ID) {
		wlfw_dynamic_feature_mask_send_sync_msg(priv,
							dynamic_feature_mask);
	}

	if (!priv->fw_error_fatal_irq)
		register_fw_error_notifications(&priv->pdev->dev);

	if (!priv->fw_early_crash_irq)
		register_early_crash_notifications(&priv->pdev->dev);

	if (priv->vbatt_supported)
		icnss_init_vph_monitor(priv);

	return ret;

device_info_failure:
	icnss_hw_power_off(priv);
fail:
	ICNSS_ASSERT(ignore_assert);
qmi_registered:
	return ret;
}

static int icnss_driver_event_server_exit(struct icnss_priv *priv)
{
	if (!priv)
		return -ENODEV;

	icnss_pr_info("WLAN FW Service Disconnected: 0x%lx\n", priv->state);

	icnss_clear_server(priv);

	if (priv->adc_tm_dev && priv->vbatt_supported)
		adc_tm_disable_chan_meas(priv->adc_tm_dev,
					  &priv->vph_monitor_params);

	return 0;
}

static int icnss_call_driver_probe(struct icnss_priv *priv)
{
	int ret = 0;
	int probe_cnt = 0;

	if (!priv->ops || !priv->ops->probe)
		return 0;

	if (test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		return -EINVAL;

	icnss_pr_dbg("Calling driver probe state: 0x%lx\n", priv->state);

	icnss_hw_power_on(priv);

	icnss_block_shutdown(true);
	while (probe_cnt < ICNSS_MAX_PROBE_CNT) {
		ret = priv->ops->probe(&priv->pdev->dev);
		probe_cnt++;
		if (ret != -EPROBE_DEFER)
			break;
	}
	if (ret < 0) {
		icnss_pr_err("Driver probe failed: %d, state: 0x%lx, probe_cnt: %d\n",
			     ret, priv->state, probe_cnt);
		icnss_block_shutdown(false);
		goto out;
	}

	icnss_block_shutdown(false);
	set_bit(ICNSS_DRIVER_PROBED, &priv->state);

	return 0;

out:
	icnss_hw_power_off(priv);
	return ret;
}

static int icnss_call_driver_shutdown(struct icnss_priv *priv)
{
	if (!test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		goto out;

	if (!priv->ops || !priv->ops->shutdown)
		goto out;

	if (test_bit(ICNSS_SHUTDOWN_DONE, &priv->state))
		goto out;

	icnss_pr_dbg("Calling driver shutdown state: 0x%lx\n", priv->state);

	priv->ops->shutdown(&priv->pdev->dev);
	set_bit(ICNSS_SHUTDOWN_DONE, &priv->state);

out:
	return 0;
}

static int icnss_pd_restart_complete(struct icnss_priv *priv)
{
	int ret = 0;

	icnss_pm_relax(priv);

	icnss_call_driver_shutdown(priv);

	clear_bit(ICNSS_PDR, &priv->state);
	clear_bit(ICNSS_REJUVENATE, &priv->state);
	clear_bit(ICNSS_PD_RESTART, &priv->state);
	priv->early_crash_ind = false;
	priv->is_ssr = false;

	if (!priv->ops || !priv->ops->reinit)
		goto out;

	if (test_bit(ICNSS_FW_DOWN, &priv->state)) {
		icnss_pr_err("FW is in bad state, state: 0x%lx\n",
			     priv->state);
		goto out;
	}

	if (!test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		goto call_probe;

	icnss_pr_dbg("Calling driver reinit state: 0x%lx\n", priv->state);

	icnss_hw_power_on(priv);

	icnss_block_shutdown(true);

	ret = priv->ops->reinit(&priv->pdev->dev);
	if (ret < 0) {
		icnss_fatal_err("Driver reinit failed: %d, state: 0x%lx\n",
				ret, priv->state);
		if (!priv->allow_recursive_recovery)
			ICNSS_ASSERT(false);
		icnss_block_shutdown(false);
		goto out_power_off;
	}

	icnss_block_shutdown(false);
	clear_bit(ICNSS_SHUTDOWN_DONE, &priv->state);
	return 0;

call_probe:
	return icnss_call_driver_probe(priv);

out_power_off:
	icnss_hw_power_off(priv);

out:
	return ret;
}


static int icnss_driver_event_fw_ready_ind(struct icnss_priv *priv, void *data)
{
	int ret = 0;

	if (!priv)
		return -ENODEV;

	set_bit(ICNSS_FW_READY, &priv->state);
	clear_bit(ICNSS_MODE_ON, &priv->state);
	atomic_set(&priv->soc_wake_ref_count, 0);

	if (priv->device_id == WCN6750_DEVICE_ID)
		icnss_free_qdss_mem(priv);

	icnss_pr_info("WLAN FW is ready: 0x%lx\n", priv->state);

	icnss_hw_power_off(priv);

	if (!priv->pdev) {
		icnss_pr_err("Device is not ready\n");
		ret = -ENODEV;
		goto out;
	}

	if (test_bit(ICNSS_PD_RESTART, &priv->state)) {
		ret = icnss_pd_restart_complete(priv);
	} else {
		if (priv->device_id == WCN6750_DEVICE_ID)
			icnss_setup_dms_mac(priv);
		ret = icnss_call_driver_probe(priv);
	}

	icnss_vreg_unvote(priv);

out:
	return ret;
}

static int icnss_driver_event_fw_init_done(struct icnss_priv *priv, void *data)
{
	int ret = 0;

	if (!priv)
		return -ENODEV;

	icnss_pr_info("WLAN FW Initialization done: 0x%lx\n", priv->state);

	if (icnss_wlfw_qdss_dnld_send_sync(priv))
		icnss_pr_info("Failed to download qdss configuration file");

	if (test_bit(ICNSS_COLD_BOOT_CAL, &priv->state))
		ret = wlfw_wlan_mode_send_sync_msg(priv,
			(enum wlfw_driver_mode_enum_v01)ICNSS_CALIBRATION);
	else
		icnss_driver_event_fw_ready_ind(priv, NULL);

	return ret;
}

int icnss_alloc_qdss_mem(struct icnss_priv *priv)
{
	struct platform_device *pdev = priv->pdev;
	struct icnss_fw_mem *qdss_mem = priv->qdss_mem;
	int i, j;

	for (i = 0; i < priv->qdss_mem_seg_len; i++) {
		if (!qdss_mem[i].va && qdss_mem[i].size) {
			qdss_mem[i].va =
				dma_alloc_coherent(&pdev->dev,
						   qdss_mem[i].size,
						   &qdss_mem[i].pa,
						   GFP_KERNEL);
			if (!qdss_mem[i].va) {
				icnss_pr_err("Failed to allocate QDSS memory for FW, size: 0x%zx, type: %u, chuck-ID: %d\n",
					     qdss_mem[i].size,
					     qdss_mem[i].type, i);
				break;
			}
		}
	}

	/* Best-effort allocation for QDSS trace */
	if (i < priv->qdss_mem_seg_len) {
		for (j = i; j < priv->qdss_mem_seg_len; j++) {
			qdss_mem[j].type = 0;
			qdss_mem[j].size = 0;
		}
		priv->qdss_mem_seg_len = i;
	}

	return 0;
}

void icnss_free_qdss_mem(struct icnss_priv *priv)
{
	struct platform_device *pdev = priv->pdev;
	struct icnss_fw_mem *qdss_mem = priv->qdss_mem;
	int i;

	for (i = 0; i < priv->qdss_mem_seg_len; i++) {
		if (qdss_mem[i].va && qdss_mem[i].size) {
			icnss_pr_dbg("Freeing memory for QDSS: pa: %pa, size: 0x%zx, type: %u\n",
				     &qdss_mem[i].pa, qdss_mem[i].size,
				     qdss_mem[i].type);
			dma_free_coherent(&pdev->dev,
					  qdss_mem[i].size, qdss_mem[i].va,
					  qdss_mem[i].pa);
			qdss_mem[i].va = NULL;
			qdss_mem[i].pa = 0;
			qdss_mem[i].size = 0;
			qdss_mem[i].type = 0;
		}
	}
	priv->qdss_mem_seg_len = 0;
}

static int icnss_qdss_trace_req_mem_hdlr(struct icnss_priv *priv)
{
	int ret = 0;

	ret = icnss_alloc_qdss_mem(priv);
	if (ret < 0)
		return ret;

	return wlfw_qdss_trace_mem_info_send_sync(priv);
}

static void *icnss_qdss_trace_pa_to_va(struct icnss_priv *priv,
				       u64 pa, u32 size, int *seg_id)
{
	int i = 0;
	struct icnss_fw_mem *qdss_mem = priv->qdss_mem;
	u64 offset = 0;
	void *va = NULL;
	u64 local_pa;
	u32 local_size;

	for (i = 0; i < priv->qdss_mem_seg_len; i++) {
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

static int icnss_qdss_trace_save_hdlr(struct icnss_priv *priv,
				      void *data)
{
	struct icnss_qmi_event_qdss_trace_save_data *event_data = data;
	struct icnss_fw_mem *qdss_mem = priv->qdss_mem;
	int ret = 0;
	int i;
	void *va = NULL;
	u64 pa;
	u32 size;
	int seg_id = 0;

	if (!priv->qdss_mem_seg_len) {
		icnss_pr_err("Memory for QDSS trace is not available\n");
		return -ENOMEM;
	}

	if (event_data->mem_seg_len == 0) {
		for (i = 0; i < priv->qdss_mem_seg_len; i++) {
			ret = icnss_genl_send_msg(qdss_mem[i].va,
						  ICNSS_GENL_MSG_TYPE_QDSS,
						  event_data->file_name,
						  qdss_mem[i].size);
			if (ret < 0) {
				icnss_pr_err("Fail to save QDSS data: %d\n",
					     ret);
				break;
			}
		}
	} else {
		for (i = 0; i < event_data->mem_seg_len; i++) {
			pa = event_data->mem_seg[i].addr;
			size = event_data->mem_seg[i].size;
			va = icnss_qdss_trace_pa_to_va(priv, pa,
						       size, &seg_id);
			if (!va) {
				icnss_pr_err("Fail to find matching va for pa %pa\n",
					     &pa);
				ret = -EINVAL;
				break;
			}
			ret = icnss_genl_send_msg(va, ICNSS_GENL_MSG_TYPE_QDSS,
						  event_data->file_name, size);
			if (ret < 0) {
				icnss_pr_err("Fail to save QDSS data: %d\n",
					     ret);
				break;
			}
		}
	}

	kfree(data);
	return ret;
}

static inline int icnss_atomic_dec_if_greater_one(atomic_t *v)
{
	int dec, c = atomic_read(v);

	do {
		dec = c - 1;
		if (unlikely(dec < 1))
			break;
	} while (!atomic_try_cmpxchg(v, &c, dec));

	return dec;
}

static int icnss_qdss_trace_req_data_hdlr(struct icnss_priv *priv,
					  void *data)
{
	int ret = 0;
	struct icnss_qmi_event_qdss_trace_save_data *event_data = data;

	if (!priv)
		return -ENODEV;

	if (!data)
		return -EINVAL;

	ret = icnss_wlfw_qdss_data_send_sync(priv, event_data->file_name,
					     event_data->total_size);

	kfree(data);
	return ret;
}

static int icnss_event_soc_wake_request(struct icnss_priv *priv, void *data)
{
	int ret = 0;

	if (!priv)
		return -ENODEV;

	if (atomic_inc_not_zero(&priv->soc_wake_ref_count)) {
		icnss_pr_soc_wake("SOC awake after posting work, Ref count: %d",
				  atomic_read(&priv->soc_wake_ref_count));
		return 0;
	}

	ret = wlfw_send_soc_wake_msg(priv, QMI_WLFW_WAKE_REQUEST_V01);
	if (!ret)
		atomic_inc(&priv->soc_wake_ref_count);

	return ret;
}

static int icnss_event_soc_wake_release(struct icnss_priv *priv, void *data)
{
	int ret = 0;

	if (!priv)
		return -ENODEV;

	if (atomic_dec_if_positive(&priv->soc_wake_ref_count)) {
		icnss_pr_soc_wake("Wake release not called. Ref count: %d",
				  priv->soc_wake_ref_count);
		return 0;
	}

	ret = wlfw_send_soc_wake_msg(priv, QMI_WLFW_WAKE_RELEASE_V01);

	return ret;
}

static int icnss_driver_event_register_driver(struct icnss_priv *priv,
							 void *data)
{
	int ret = 0;
	int probe_cnt = 0;

	if (priv->ops)
		return -EEXIST;

	priv->ops = data;

	if (test_bit(SKIP_QMI, &priv->ctrl_params.quirks))
		set_bit(ICNSS_FW_READY, &priv->state);

	if (test_bit(ICNSS_FW_DOWN, &priv->state)) {
		icnss_pr_err("FW is in bad state, state: 0x%lx\n",
			     priv->state);
		return -ENODEV;
	}

	if (!test_bit(ICNSS_FW_READY, &priv->state)) {
		icnss_pr_dbg("FW is not ready yet, state: 0x%lx\n",
			     priv->state);
		goto out;
	}

	ret = icnss_hw_power_on(priv);
	if (ret)
		goto out;

	icnss_block_shutdown(true);
	while (probe_cnt < ICNSS_MAX_PROBE_CNT) {
		ret = priv->ops->probe(&priv->pdev->dev);
		probe_cnt++;
		if (ret != -EPROBE_DEFER)
			break;
	}
	if (ret) {
		icnss_pr_err("Driver probe failed: %d, state: 0x%lx, probe_cnt: %d\n",
			     ret, priv->state, probe_cnt);
		icnss_block_shutdown(false);
		goto power_off;
	}

	icnss_block_shutdown(false);
	set_bit(ICNSS_DRIVER_PROBED, &priv->state);

	return 0;

power_off:
	icnss_hw_power_off(priv);
out:
	return ret;
}

static int icnss_driver_event_unregister_driver(struct icnss_priv *priv,
							 void *data)
{
	if (!test_bit(ICNSS_DRIVER_PROBED, &priv->state)) {
		priv->ops = NULL;
		goto out;
	}

	set_bit(ICNSS_DRIVER_UNLOADING, &priv->state);

	icnss_block_shutdown(true);

	if (priv->ops)
		priv->ops->remove(&priv->pdev->dev);

	icnss_block_shutdown(false);

	clear_bit(ICNSS_DRIVER_UNLOADING, &priv->state);
	clear_bit(ICNSS_DRIVER_PROBED, &priv->state);

	priv->ops = NULL;

	icnss_hw_power_off(priv);

out:
	return 0;
}

static int icnss_fw_crashed(struct icnss_priv *priv,
			    struct icnss_event_pd_service_down_data *event_data)
{
	icnss_pr_dbg("FW crashed, state: 0x%lx\n", priv->state);

	set_bit(ICNSS_PD_RESTART, &priv->state);
	clear_bit(ICNSS_FW_READY, &priv->state);

	icnss_pm_stay_awake(priv);

	if (test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		icnss_call_driver_uevent(priv, ICNSS_UEVENT_FW_CRASHED, NULL);

	if (event_data && event_data->fw_rejuvenate)
		wlfw_rejuvenate_ack_send_sync_msg(priv);

	return 0;
}

int icnss_update_hang_event_data(struct icnss_priv *priv,
				 struct icnss_uevent_hang_data *hang_data)
{
	if (!priv->hang_event_data_va)
		return -EINVAL;

	priv->hang_event_data = kmemdup(priv->hang_event_data_va,
					priv->hang_event_data_len,
					GFP_ATOMIC);
	if (!priv->hang_event_data)
		return -ENOMEM;

	// Update the hang event params
	hang_data->hang_event_data = priv->hang_event_data;
	hang_data->hang_event_data_len = priv->hang_event_data_len;

	return 0;
}

int icnss_send_hang_event_data(struct icnss_priv *priv)
{
	struct icnss_uevent_hang_data hang_data = {0};
	int ret = 0xFF;

	if (priv->early_crash_ind) {
		ret = icnss_update_hang_event_data(priv, &hang_data);
		if (ret)
			icnss_pr_err("Unable to allocate memory for Hang event data\n");
	}
	icnss_call_driver_uevent(priv, ICNSS_UEVENT_HANG_DATA,
				 &hang_data);

	if (!ret) {
		kfree(priv->hang_event_data);
		priv->hang_event_data = NULL;
	}

	return 0;
}

static int icnss_driver_event_pd_service_down(struct icnss_priv *priv,
					      void *data)
{
	struct icnss_event_pd_service_down_data *event_data = data;

	if (!test_bit(ICNSS_WLFW_EXISTS, &priv->state)) {
		icnss_ignore_fw_timeout(false);
		goto out;
	}

	if (priv->force_err_fatal)
		ICNSS_ASSERT(0);

	if (priv->device_id == WCN6750_DEVICE_ID) {
		priv->smp2p_info.seq = 0;
		if (qcom_smem_state_update_bits(
				priv->smp2p_info.smem_state,
				ICNSS_SMEM_VALUE_MASK,
				0))
			icnss_pr_dbg("Error in SMP2P sent ret: %d\n");
	}

	icnss_send_hang_event_data(priv);

	if (priv->early_crash_ind) {
		icnss_pr_dbg("PD Down ignored as early indication is processed: %d, state: 0x%lx\n",
			     event_data->crashed, priv->state);
		goto out;
	}

	if (test_bit(ICNSS_PD_RESTART, &priv->state) && event_data->crashed) {
		icnss_fatal_err("PD Down while recovery inprogress, crashed: %d, state: 0x%lx\n",
				event_data->crashed, priv->state);
		if (!priv->allow_recursive_recovery)
			ICNSS_ASSERT(0);
		goto out;
	}

	if (!test_bit(ICNSS_PD_RESTART, &priv->state))
		icnss_fw_crashed(priv, event_data);

out:
	kfree(data);

	return 0;
}

static int icnss_driver_event_early_crash_ind(struct icnss_priv *priv,
					      void *data)
{
	if (!test_bit(ICNSS_WLFW_EXISTS, &priv->state)) {
		icnss_ignore_fw_timeout(false);
		goto out;
	}

	priv->early_crash_ind = true;
	icnss_fw_crashed(priv, NULL);

out:
	kfree(data);

	return 0;
}

static int icnss_driver_event_idle_shutdown(struct icnss_priv *priv,
					    void *data)
{
	int ret = 0;

	if (!priv->ops || !priv->ops->idle_shutdown)
		return 0;

	if (priv->is_ssr || test_bit(ICNSS_PDR, &priv->state) ||
	    test_bit(ICNSS_REJUVENATE, &priv->state)) {
		icnss_pr_err("SSR/PDR is already in-progress during idle shutdown callback\n");
		ret = -EBUSY;
	} else {
		icnss_pr_dbg("Calling driver idle shutdown, state: 0x%lx\n",
								priv->state);
		icnss_block_shutdown(true);
		ret = priv->ops->idle_shutdown(&priv->pdev->dev);
		icnss_block_shutdown(false);
	}

	return ret;
}

static int icnss_driver_event_idle_restart(struct icnss_priv *priv,
					   void *data)
{
	int ret = 0;

	if (!priv->ops || !priv->ops->idle_restart)
		return 0;

	if (priv->is_ssr || test_bit(ICNSS_PDR, &priv->state) ||
	    test_bit(ICNSS_REJUVENATE, &priv->state)) {
		icnss_pr_err("SSR/PDR is already in-progress during idle restart callback\n");
		ret = -EBUSY;
	} else {
		icnss_pr_dbg("Calling driver idle restart, state: 0x%lx\n",
								priv->state);
		icnss_block_shutdown(true);
		ret = priv->ops->idle_restart(&priv->pdev->dev);
		icnss_block_shutdown(false);
	}

	return ret;
}

static int icnss_qdss_trace_free_hdlr(struct icnss_priv *priv)
{
	icnss_free_qdss_mem(priv);

	return 0;
}

static int icnss_m3_dump_upload_req_hdlr(struct icnss_priv *priv,
					 void *data)
{
	struct icnss_m3_upload_segments_req_data *event_data = data;
	struct ramdump_segment segment;
	int i, status = 0, ret = 0;

	for (i = 0; i < event_data->no_of_valid_segments; i++) {
		memset(&segment, 0, sizeof(segment));
		segment.v_address = devm_ioremap(&priv->pdev->dev,
						event_data->m3_segment[i].addr,
						event_data->m3_segment[i].size);
		if (!segment.v_address) {
			icnss_pr_err("Failed to ioremap M3 Dump region");
			ret = -ENOMEM;
			goto send_resp;
		}

		segment.size = event_data->m3_segment[i].size;
		segment.name = event_data->m3_segment[i].name;

		switch (event_data->m3_segment[i].type) {
		case QMI_M3_SEGMENT_PHYAREG_V01:
			ret = do_ramdump(priv->m3_dump_dev_seg1, &segment, 1);
			break;
		case QMI_M3_SEGMENT_PHYDBG_V01:
			ret = do_ramdump(priv->m3_dump_dev_seg2, &segment, 1);
			break;
		case QMI_M3_SEGMENT_WMAC0_REG_V01:
			ret = do_ramdump(priv->m3_dump_dev_seg3, &segment, 1);
			break;
		case QMI_M3_SEGMENT_WCSSDBG_V01:
			ret = do_ramdump(priv->m3_dump_dev_seg4, &segment, 1);
			break;
		case QMI_M3_SEGMENT_PHYAPDMEM_V01:
			ret = do_ramdump(priv->m3_dump_dev_seg5, &segment, 1);
			break;
		default:
			icnss_pr_err("Invalid Segment type: %d",
				     event_data->m3_segment[i].type);
		}

		if (ret) {
			status = ret;
			icnss_pr_err("Failed to dump m3 %s segment, err = %d\n",
				     event_data->m3_segment[i].name, ret);
		}
	}
send_resp:
	icnss_wlfw_m3_dump_upload_done_send_sync(priv, event_data->pdev_id,
						 status);

	return ret;
}

static void icnss_driver_event_work(struct work_struct *work)
{
	struct icnss_priv *priv =
		container_of(work, struct icnss_priv, event_work);
	struct icnss_driver_event *event;
	unsigned long flags;
	int ret;

	icnss_pm_stay_awake(priv);

	spin_lock_irqsave(&priv->event_lock, flags);

	while (!list_empty(&priv->event_list)) {
		event = list_first_entry(&priv->event_list,
					 struct icnss_driver_event, list);
		list_del(&event->list);
		spin_unlock_irqrestore(&priv->event_lock, flags);

		icnss_pr_dbg("Processing event: %s%s(%d), state: 0x%lx\n",
			     icnss_driver_event_to_str(event->type),
			     event->sync ? "-sync" : "", event->type,
			     priv->state);

		switch (event->type) {
		case ICNSS_DRIVER_EVENT_SERVER_ARRIVE:
			ret = icnss_driver_event_server_arrive(priv,
								 event->data);
			break;
		case ICNSS_DRIVER_EVENT_SERVER_EXIT:
			ret = icnss_driver_event_server_exit(priv);
			break;
		case ICNSS_DRIVER_EVENT_FW_READY_IND:
			ret = icnss_driver_event_fw_ready_ind(priv,
								 event->data);
			break;
		case ICNSS_DRIVER_EVENT_REGISTER_DRIVER:
			ret = icnss_driver_event_register_driver(priv,
								 event->data);
			break;
		case ICNSS_DRIVER_EVENT_UNREGISTER_DRIVER:
			ret = icnss_driver_event_unregister_driver(priv,
								   event->data);
			break;
		case ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN:
			ret = icnss_driver_event_pd_service_down(priv,
								 event->data);
			break;
		case ICNSS_DRIVER_EVENT_FW_EARLY_CRASH_IND:
			ret = icnss_driver_event_early_crash_ind(priv,
								 event->data);
			break;
		case ICNSS_DRIVER_EVENT_IDLE_SHUTDOWN:
			ret = icnss_driver_event_idle_shutdown(priv,
							       event->data);
			break;
		case ICNSS_DRIVER_EVENT_IDLE_RESTART:
			ret = icnss_driver_event_idle_restart(priv,
							      event->data);
			break;
		case ICNSS_DRIVER_EVENT_FW_INIT_DONE_IND:
			ret = icnss_driver_event_fw_init_done(priv,
							      event->data);
			break;
		case ICNSS_DRIVER_EVENT_QDSS_TRACE_REQ_MEM:
			ret = icnss_qdss_trace_req_mem_hdlr(priv);
			break;
		case ICNSS_DRIVER_EVENT_QDSS_TRACE_SAVE:
			ret = icnss_qdss_trace_save_hdlr(priv,
							 event->data);
			break;
		case ICNSS_DRIVER_EVENT_QDSS_TRACE_FREE:
			ret = icnss_qdss_trace_free_hdlr(priv);
			break;
		case ICNSS_DRIVER_EVENT_M3_DUMP_UPLOAD_REQ:
			ret = icnss_m3_dump_upload_req_hdlr(priv, event->data);
			break;
		case ICNSS_DRIVER_EVENT_QDSS_TRACE_REQ_DATA:
			ret = icnss_qdss_trace_req_data_hdlr(priv,
							     event->data);
			break;
		default:
			icnss_pr_err("Invalid Event type: %d", event->type);
			kfree(event);
			continue;
		}

		priv->stats.events[event->type].processed++;

		icnss_pr_dbg("Event Processed: %s%s(%d), ret: %d, state: 0x%lx\n",
			     icnss_driver_event_to_str(event->type),
			     event->sync ? "-sync" : "", event->type, ret,
			     priv->state);

		spin_lock_irqsave(&priv->event_lock, flags);
		if (event->sync) {
			event->ret = ret;
			complete(&event->complete);
			continue;
		}
		spin_unlock_irqrestore(&priv->event_lock, flags);

		kfree(event);

		spin_lock_irqsave(&priv->event_lock, flags);
	}
	spin_unlock_irqrestore(&priv->event_lock, flags);

	icnss_pm_relax(priv);
}

static void icnss_soc_wake_msg_work(struct work_struct *work)
{
	struct icnss_priv *priv =
		container_of(work, struct icnss_priv, soc_wake_msg_work);
	struct icnss_soc_wake_event *event;
	unsigned long flags;
	int ret;

	icnss_pm_stay_awake(priv);

	spin_lock_irqsave(&priv->soc_wake_msg_lock, flags);

	while (!list_empty(&priv->soc_wake_msg_list)) {
		event = list_first_entry(&priv->soc_wake_msg_list,
					 struct icnss_soc_wake_event, list);
		list_del(&event->list);
		spin_unlock_irqrestore(&priv->soc_wake_msg_lock, flags);

		icnss_pr_soc_wake("Processing event: %s%s(%d), state: 0x%lx\n",
				  icnss_soc_wake_event_to_str(event->type),
				  event->sync ? "-sync" : "", event->type,
				  priv->state);

		switch (event->type) {
		case ICNSS_SOC_WAKE_REQUEST_EVENT:
			ret = icnss_event_soc_wake_request(priv,
							   event->data);
			break;
		case ICNSS_SOC_WAKE_RELEASE_EVENT:
			ret = icnss_event_soc_wake_release(priv,
							   event->data);
			break;
		default:
			icnss_pr_err("Invalid Event type: %d", event->type);
			kfree(event);
			continue;
		}

		priv->stats.soc_wake_events[event->type].processed++;

		icnss_pr_soc_wake("Event Processed: %s%s(%d), ret: %d, state: 0x%lx\n",
				  icnss_soc_wake_event_to_str(event->type),
				  event->sync ? "-sync" : "", event->type, ret,
				  priv->state);

		spin_lock_irqsave(&priv->soc_wake_msg_lock, flags);
		if (event->sync) {
			event->ret = ret;
			complete(&event->complete);
			continue;
		}
		spin_unlock_irqrestore(&priv->soc_wake_msg_lock, flags);

		kfree(event);

		spin_lock_irqsave(&priv->soc_wake_msg_lock, flags);
	}
	spin_unlock_irqrestore(&priv->soc_wake_msg_lock, flags);

	icnss_pm_relax(priv);
}

static int icnss_msa0_ramdump(struct icnss_priv *priv)
{
	struct ramdump_segment segment;

	memset(&segment, 0, sizeof(segment));
	segment.v_address = priv->msa_va;
	segment.size = priv->msa_mem_size;
	return do_ramdump(priv->msa0_dump_dev, &segment, 1);
}

static void icnss_update_state_send_modem_shutdown(struct icnss_priv *priv,
							void *data)
{
	struct qcom_ssr_notify_data *notif = data;
	int ret = 0;

	if (!notif->crashed) {
		if (atomic_read(&priv->is_shutdown)) {
			atomic_set(&priv->is_shutdown, false);
			if (!test_bit(ICNSS_PD_RESTART, &priv->state) &&
				!test_bit(ICNSS_SHUTDOWN_DONE, &priv->state) &&
				!test_bit(ICNSS_BLOCK_SHUTDOWN, &priv->state)) {
				clear_bit(ICNSS_FW_READY, &priv->state);
				icnss_driver_event_post(priv,
					  ICNSS_DRIVER_EVENT_UNREGISTER_DRIVER,
					  ICNSS_EVENT_SYNC_UNINTERRUPTIBLE,
					  NULL);
			}
		}

		if (test_bit(ICNSS_BLOCK_SHUTDOWN, &priv->state)) {
			if (!wait_for_completion_timeout(
					&priv->unblock_shutdown,
					msecs_to_jiffies(PROBE_TIMEOUT)))
				icnss_pr_err("modem block shutdown timeout\n");
		}

		ret = wlfw_send_modem_shutdown_msg(priv);
		if (ret < 0)
			icnss_pr_err("Fail to send modem shutdown Indication %d\n",
				     ret);
	}
}

static char *icnss_qcom_ssr_notify_state_to_str(enum qcom_ssr_notify_type code)
{
	switch (code) {
	case QCOM_SSR_BEFORE_POWERUP:
		return "BEFORE_POWERUP";
	case QCOM_SSR_AFTER_POWERUP:
		return "AFTER_POWERUP";
	case QCOM_SSR_BEFORE_SHUTDOWN:
		return "BEFORE_SHUTDOWN";
	case QCOM_SSR_AFTER_SHUTDOWN:
		return "AFTER_SHUTDOWN";
	default:
		return "UNKNOWN";
	}
};

static int icnss_wpss_notifier_nb(struct notifier_block *nb,
				  unsigned long code,
				  void *data)
{
	struct icnss_event_pd_service_down_data *event_data;
	struct qcom_ssr_notify_data *notif = data;
	struct icnss_priv *priv = container_of(nb, struct icnss_priv,
					       wpss_ssr_nb);
	struct icnss_uevent_fw_down_data fw_down_data = {0};

	icnss_pr_vdbg("WPSS-Notify: event %s(%lu)\n",
		      icnss_qcom_ssr_notify_state_to_str(code), code);

	if (code == QCOM_SSR_AFTER_SHUTDOWN) {
		icnss_pr_info("Collecting msa0 segment dump\n");
		icnss_msa0_ramdump(priv);
		goto out;
	}

	if (code != QCOM_SSR_BEFORE_SHUTDOWN)
		goto out;

	priv->is_ssr = true;

	icnss_pr_info("WPSS went down, state: 0x%lx, crashed: %d\n",
		      priv->state, notif->crashed);

	set_bit(ICNSS_FW_DOWN, &priv->state);

	if (notif->crashed)
		priv->stats.recovery.root_pd_crash++;
	else
		priv->stats.recovery.root_pd_shutdown++;

	icnss_ignore_fw_timeout(true);

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);

	if (event_data == NULL)
		return notifier_from_errno(-ENOMEM);

	event_data->crashed = notif->crashed;

	fw_down_data.crashed = !!notif->crashed;
	if (test_bit(ICNSS_FW_READY, &priv->state)) {
		clear_bit(ICNSS_FW_READY, &priv->state);
		fw_down_data.crashed = !!notif->crashed;
		icnss_call_driver_uevent(priv,
					 ICNSS_UEVENT_FW_DOWN,
					 &fw_down_data);
	}
	icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN,
				ICNSS_EVENT_SYNC, event_data);
out:
	icnss_pr_vdbg("Exit %s,state: 0x%lx\n", __func__, priv->state);
	return NOTIFY_OK;
}

static int icnss_modem_notifier_nb(struct notifier_block *nb,
				  unsigned long code,
				  void *data)
{
	struct icnss_event_pd_service_down_data *event_data;
	struct qcom_ssr_notify_data *notif = data;
	struct icnss_priv *priv = container_of(nb, struct icnss_priv,
					       modem_ssr_nb);
	struct icnss_uevent_fw_down_data fw_down_data = {0};

	icnss_pr_vdbg("Modem-Notify: event %s(%lu)\n",
		      icnss_qcom_ssr_notify_state_to_str(code), code);

	if (code == QCOM_SSR_AFTER_SHUTDOWN) {
		icnss_pr_info("Collecting msa0 segment dump\n");
		icnss_msa0_ramdump(priv);
		goto out;
	}

	if (code != QCOM_SSR_BEFORE_SHUTDOWN)
		goto out;

	priv->is_ssr = true;

	if (notif->crashed) {
		priv->stats.recovery.root_pd_crash++;
		priv->root_pd_shutdown = false;
	} else {
		priv->stats.recovery.root_pd_shutdown++;
		priv->root_pd_shutdown = true;
	}

	icnss_update_state_send_modem_shutdown(priv, data);

	if (test_bit(ICNSS_PDR_REGISTERED, &priv->state)) {
		set_bit(ICNSS_FW_DOWN, &priv->state);
		icnss_ignore_fw_timeout(true);

		if (test_bit(ICNSS_FW_READY, &priv->state)) {
			clear_bit(ICNSS_FW_READY, &priv->state);
			fw_down_data.crashed = !!notif->crashed;
			icnss_call_driver_uevent(priv,
						 ICNSS_UEVENT_FW_DOWN,
						 &fw_down_data);
		}
		goto out;
	}

	icnss_pr_info("Modem went down, state: 0x%lx, crashed: %d\n",
		      priv->state, notif->crashed);

	set_bit(ICNSS_FW_DOWN, &priv->state);

	icnss_ignore_fw_timeout(true);

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);

	if (event_data == NULL)
		return notifier_from_errno(-ENOMEM);

	event_data->crashed = notif->crashed;

	fw_down_data.crashed = !!notif->crashed;
	if (test_bit(ICNSS_FW_READY, &priv->state)) {
		clear_bit(ICNSS_FW_READY, &priv->state);
		fw_down_data.crashed = !!notif->crashed;
		icnss_call_driver_uevent(priv,
					 ICNSS_UEVENT_FW_DOWN,
					 &fw_down_data);
	}
	icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN,
				ICNSS_EVENT_SYNC, event_data);
out:
	icnss_pr_vdbg("Exit %s,state: 0x%lx\n", __func__, priv->state);
	return NOTIFY_OK;
}

static int icnss_wpss_ssr_register_notifier(struct icnss_priv *priv)
{
	int ret = 0;

	priv->wpss_ssr_nb.notifier_call = icnss_wpss_notifier_nb;
	/*
	 * Assign priority of icnss wpss notifier callback over IPA
	 * modem notifier callback which is 0
	 */
	priv->wpss_ssr_nb.priority = 1;

	priv->wpss_notify_handler =
		qcom_register_ssr_notifier("wpss", &priv->wpss_ssr_nb);

	if (IS_ERR(priv->wpss_notify_handler)) {
		ret = PTR_ERR(priv->wpss_notify_handler);
		icnss_pr_err("WPSS register notifier failed: %d\n", ret);
	}

	set_bit(ICNSS_SSR_REGISTERED, &priv->state);

	return ret;
}

static int icnss_modem_ssr_register_notifier(struct icnss_priv *priv)
{
	int ret = 0;

	priv->modem_ssr_nb.notifier_call = icnss_modem_notifier_nb;
	/*
	 * Assign priority of icnss modem notifier callback over IPA
	 * modem notifier callback which is 0
	 */
	priv->modem_ssr_nb.priority = 1;

	priv->modem_notify_handler =
		qcom_register_ssr_notifier("modem", &priv->modem_ssr_nb);

	if (IS_ERR(priv->modem_notify_handler)) {
		ret = PTR_ERR(priv->modem_notify_handler);
		icnss_pr_err("Modem register notifier failed: %d\n", ret);
	}

	set_bit(ICNSS_SSR_REGISTERED, &priv->state);

	return ret;
}

static int icnss_wpss_ssr_unregister_notifier(struct icnss_priv *priv)
{
	if (!test_and_clear_bit(ICNSS_SSR_REGISTERED, &priv->state))
		return 0;

	qcom_unregister_ssr_notifier(priv->wpss_notify_handler,
				     &priv->wpss_ssr_nb);
	priv->wpss_notify_handler = NULL;

	return 0;
}

static int icnss_modem_ssr_unregister_notifier(struct icnss_priv *priv)
{
	if (!test_and_clear_bit(ICNSS_SSR_REGISTERED, &priv->state))
		return 0;

	qcom_unregister_ssr_notifier(priv->modem_notify_handler,
				     &priv->modem_ssr_nb);
	priv->modem_notify_handler = NULL;

	return 0;
}

static void icnss_pdr_notifier_cb(int state, char *service_path, void *priv_cb)
{
	struct icnss_priv *priv = priv_cb;
	struct icnss_event_pd_service_down_data *event_data;
	struct icnss_uevent_fw_down_data fw_down_data = {0};
	enum icnss_pdr_cause_index cause = ICNSS_ROOT_PD_CRASH;

	icnss_pr_dbg("PD service notification: 0x%lx state: 0x%lx\n",
		     state, priv->state);

	switch (state) {
	case SERVREG_SERVICE_STATE_DOWN:
		event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);

		if (!event_data)
			return;

		event_data->crashed = true;

		if (!priv->is_ssr) {
			set_bit(ICNSS_PDR, &penv->state);
			if (test_bit(ICNSS_HOST_TRIGGERED_PDR, &priv->state)) {
				cause = ICNSS_HOST_ERROR;
				priv->stats.recovery.pdr_host_error++;
			} else {
				cause = ICNSS_FW_CRASH;
				priv->stats.recovery.pdr_fw_crash++;
			}
		} else if (priv->root_pd_shutdown) {
			cause = ICNSS_ROOT_PD_SHUTDOWN;
			event_data->crashed = false;
		}

		icnss_pr_info("PD service down, state: 0x%lx: cause: %s\n",
			      priv->state, icnss_pdr_cause[cause]);

		if (!test_bit(ICNSS_FW_DOWN, &priv->state)) {
			set_bit(ICNSS_FW_DOWN, &priv->state);
			icnss_ignore_fw_timeout(true);

			if (test_bit(ICNSS_FW_READY, &priv->state)) {
				clear_bit(ICNSS_FW_READY, &priv->state);
				fw_down_data.crashed = event_data->crashed;
				icnss_call_driver_uevent(priv,
							 ICNSS_UEVENT_FW_DOWN,
							 &fw_down_data);
			}
		}
		clear_bit(ICNSS_HOST_TRIGGERED_PDR, &priv->state);
		icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN,
					ICNSS_EVENT_SYNC, event_data);
		break;
	case SERVREG_SERVICE_STATE_UP:
		clear_bit(ICNSS_FW_DOWN, &priv->state);
		break;
	default:
		break;
	}
	return;
}

static int icnss_pd_restart_enable(struct icnss_priv *priv)
{
	struct pdr_handle *handle = NULL;
	struct pdr_service *service = NULL;
	int err = 0;

	handle = pdr_handle_alloc(icnss_pdr_notifier_cb, priv);
	if (IS_ERR_OR_NULL(handle)) {
		err = PTR_ERR(handle);
		icnss_pr_err("Failed to alloc pdr handle, err %d", err);
		goto out;
	}
	service = pdr_add_lookup(handle, ICNSS_WLAN_SERVICE_NAME, ICNSS_WLANPD_NAME);
	if (IS_ERR_OR_NULL(service)) {
		err = PTR_ERR(service);
		icnss_pr_err("Failed to add lookup, err %d", err);
		goto out;
	}
	priv->pdr_handle = handle;
	priv->pdr_service = service;
	set_bit(ICNSS_PDR_REGISTERED, &priv->state);

	icnss_pr_info("PDR registration happened");
out:
	return err;
}

static void icnss_pdr_unregister_notifier(struct icnss_priv *priv)
{
	if (!test_and_clear_bit(ICNSS_PDR_REGISTERED, &priv->state))
		return;

	pdr_handle_release(priv->pdr_handle);
}

static int icnss_create_ramdump_devices(struct icnss_priv *priv)
{

	if (!priv || !priv->pdev) {
		icnss_pr_err("Platform priv or pdev is NULL\n");
		return -EINVAL;
	}

	priv->msa0_dump_dev = create_ramdump_device("wcss_msa0",
						    &priv->pdev->dev);
	if (!priv->msa0_dump_dev)
		return -ENOMEM;

	if (priv->device_id == WCN6750_DEVICE_ID) {
		priv->m3_dump_dev_seg1 = create_ramdump_device(
					    ICNSS_M3_SEGMENT(
						ICNSS_M3_SEGMENT_PHYAREG),
					    &priv->pdev->dev);
		if (!priv->m3_dump_dev_seg1)
			return -ENOMEM;

		priv->m3_dump_dev_seg2 = create_ramdump_device(
					    ICNSS_M3_SEGMENT(
						ICNSS_M3_SEGMENT_PHYA),
					    &priv->pdev->dev);
		if (!priv->m3_dump_dev_seg2)
			return -ENOMEM;

		priv->m3_dump_dev_seg3 = create_ramdump_device(
					    ICNSS_M3_SEGMENT(
						ICNSS_M3_SEGMENT_WMACREG),
					    &priv->pdev->dev);
		if (!priv->m3_dump_dev_seg3)
			return -ENOMEM;

		priv->m3_dump_dev_seg4 = create_ramdump_device(
					    ICNSS_M3_SEGMENT(
						ICNSS_M3_SEGMENT_WCSSDBG),
					    &priv->pdev->dev);
		if (!priv->m3_dump_dev_seg4)
			return -ENOMEM;

		priv->m3_dump_dev_seg5 = create_ramdump_device(
					     ICNSS_M3_SEGMENT(
						ICNSS_M3_SEGMENT_PHYAM3),
					     &priv->pdev->dev);
		if (!priv->m3_dump_dev_seg5)
			return -ENOMEM;
	}

	return 0;
}

static int icnss_enable_recovery(struct icnss_priv *priv)
{
	int ret;

	if (test_bit(RECOVERY_DISABLE, &priv->ctrl_params.quirks)) {
		icnss_pr_dbg("Recovery disabled through module parameter\n");
		return 0;
	}

	if (test_bit(PDR_ONLY, &priv->ctrl_params.quirks)) {
		icnss_pr_dbg("SSR disabled through module parameter\n");
		goto enable_pdr;
	}

	ret = icnss_create_ramdump_devices(priv);
	if (ret)
		return ret;

	if (priv->device_id == WCN6750_DEVICE_ID) {
		icnss_wpss_ssr_register_notifier(priv);
		return 0;
	}

	icnss_modem_ssr_register_notifier(priv);
	if (test_bit(SSR_ONLY, &priv->ctrl_params.quirks)) {
		icnss_pr_dbg("PDR disabled through module parameter\n");
		return 0;
	}

enable_pdr:
	ret = icnss_pd_restart_enable(priv);

	if (ret)
		return ret;

	return 0;
}

static int icnss_send_smp2p(struct icnss_priv *priv,
			    enum icnss_smp2p_msg_id msg_id)
{
	unsigned int value = 0;
	int ret;

	if (IS_ERR(priv->smp2p_info.smem_state))
		return -EINVAL;

	if (test_bit(ICNSS_FW_DOWN, &priv->state))
		return -ENODEV;

	value |= priv->smp2p_info.seq++;
	value <<= ICNSS_SMEM_SEQ_NO_POS;
	value |= msg_id;

	icnss_pr_smp2p("Sending SMP2P value: 0x%X\n", value);

	ret = qcom_smem_state_update_bits(
			priv->smp2p_info.smem_state,
			ICNSS_SMEM_VALUE_MASK,
			value);
	if (ret)
		icnss_pr_smp2p("Error in SMP2P send ret: %d\n", ret);

	return ret;
}

static int icnss_tcdev_get_max_state(struct thermal_cooling_device *tcdev,
					unsigned long *thermal_state)
{
	struct icnss_thermal_cdev *icnss_tcdev = tcdev->devdata;

	*thermal_state = icnss_tcdev->max_thermal_state;

	return 0;
}

static int icnss_tcdev_get_cur_state(struct thermal_cooling_device *tcdev,
					unsigned long *thermal_state)
{
	struct icnss_thermal_cdev *icnss_tcdev = tcdev->devdata;

	*thermal_state = icnss_tcdev->curr_thermal_state;

	return 0;
}

static int icnss_tcdev_set_cur_state(struct thermal_cooling_device *tcdev,
					unsigned long thermal_state)
{
	struct icnss_thermal_cdev *icnss_tcdev = tcdev->devdata;
	struct device *dev = &penv->pdev->dev;
	int ret = 0;


	if (!penv->ops || !penv->ops->set_therm_cdev_state)
		return 0;

	if (thermal_state > icnss_tcdev->max_thermal_state)
		return -EINVAL;

	icnss_pr_vdbg("Cooling device set current state: %ld,for cdev id %d",
		      thermal_state, icnss_tcdev->tcdev_id);

	mutex_lock(&penv->tcdev_lock);
	ret = penv->ops->set_therm_cdev_state(dev, thermal_state,
					      icnss_tcdev->tcdev_id);
	if (!ret)
		icnss_tcdev->curr_thermal_state = thermal_state;
	mutex_unlock(&penv->tcdev_lock);
	if (ret) {
		icnss_pr_err("Setting Current Thermal State Failed: %d,for cdev id %d",
			     ret, icnss_tcdev->tcdev_id);
		return ret;
	}

	return 0;
}

static struct thermal_cooling_device_ops icnss_cooling_ops = {
	.get_max_state = icnss_tcdev_get_max_state,
	.get_cur_state = icnss_tcdev_get_cur_state,
	.set_cur_state = icnss_tcdev_set_cur_state,
};

int icnss_thermal_cdev_register(struct device *dev, unsigned long max_state,
			   int tcdev_id)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	struct icnss_thermal_cdev *icnss_tcdev = NULL;
	char cdev_node_name[THERMAL_NAME_LENGTH] = "";
	struct device_node *dev_node;
	int ret = 0;

	icnss_tcdev = kzalloc(sizeof(*icnss_tcdev), GFP_KERNEL);
	if (!icnss_tcdev)
		return -ENOMEM;

	icnss_tcdev->tcdev_id = tcdev_id;
	icnss_tcdev->max_thermal_state = max_state;

	snprintf(cdev_node_name, THERMAL_NAME_LENGTH,
		 "qcom,icnss_cdev%d", tcdev_id);

	dev_node = of_find_node_by_name(NULL, cdev_node_name);
	if (!dev_node) {
		icnss_pr_err("Failed to get cooling device node\n");
		return -EINVAL;
	}

	icnss_pr_dbg("tcdev node->name=%s\n", dev_node->name);

	if (of_find_property(dev_node, "#cooling-cells", NULL)) {
		icnss_tcdev->tcdev = thermal_of_cooling_device_register(
						dev_node,
						cdev_node_name, icnss_tcdev,
						&icnss_cooling_ops);
		if (IS_ERR_OR_NULL(icnss_tcdev->tcdev)) {
			ret = PTR_ERR(icnss_tcdev->tcdev);
			icnss_pr_err("Cooling device register failed: %d, for cdev id %d\n",
				     ret, icnss_tcdev->tcdev_id);
		} else {
			icnss_pr_dbg("Cooling device registered for cdev id %d",
				     icnss_tcdev->tcdev_id);
			list_add(&icnss_tcdev->tcdev_list,
				 &priv->icnss_tcdev_list);
		}
	} else {
		icnss_pr_dbg("Cooling device registration not supported");
		ret = -EOPNOTSUPP;
	}

	return ret;
}
EXPORT_SYMBOL(icnss_thermal_cdev_register);

void icnss_thermal_cdev_unregister(struct device *dev, int tcdev_id)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	struct icnss_thermal_cdev *icnss_tcdev = NULL;

	while (!list_empty(&priv->icnss_tcdev_list)) {
		icnss_tcdev = list_first_entry(&priv->icnss_tcdev_list,
					       struct icnss_thermal_cdev,
					       tcdev_list);
		thermal_cooling_device_unregister(icnss_tcdev->tcdev);
		list_del(&icnss_tcdev->tcdev_list);
		kfree(icnss_tcdev);
	}
}
EXPORT_SYMBOL(icnss_thermal_cdev_unregister);

int icnss_get_curr_therm_cdev_state(struct device *dev,
				    unsigned long *thermal_state,
				    int tcdev_id)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	struct icnss_thermal_cdev *icnss_tcdev = NULL;

	mutex_lock(&priv->tcdev_lock);
	list_for_each_entry(icnss_tcdev, &priv->icnss_tcdev_list, tcdev_list) {
		if (icnss_tcdev->tcdev_id != tcdev_id)
			continue;

		*thermal_state = icnss_tcdev->curr_thermal_state;
		mutex_unlock(&priv->tcdev_lock);
		icnss_pr_dbg("Cooling device current state: %ld, for cdev id %d",
			     icnss_tcdev->curr_thermal_state, tcdev_id);
		return 0;
	}
	mutex_unlock(&priv->tcdev_lock);
	icnss_pr_dbg("Cooling device ID not found: %d", tcdev_id);
	return -EINVAL;
}
EXPORT_SYMBOL(icnss_get_curr_therm_cdev_state);

int icnss_qmi_send(struct device *dev, int type, void *cmd,
		  int cmd_len, void *cb_ctx,
		  int (*cb)(void *ctx, void *event, int event_len))
{
	struct icnss_priv *priv = icnss_get_plat_priv();
	int ret;

	if (!priv)
		return -ENODEV;

	if (!test_bit(ICNSS_WLFW_CONNECTED, &priv->state))
		return -EINVAL;

	priv->get_info_cb = cb;
	priv->get_info_cb_ctx = cb_ctx;

	ret = icnss_wlfw_get_info_send_sync(priv, type, cmd, cmd_len);
	if (ret) {
		priv->get_info_cb = NULL;
		priv->get_info_cb_ctx = NULL;
	}

	return ret;
}
EXPORT_SYMBOL(icnss_qmi_send);

int __icnss_register_driver(struct icnss_driver_ops *ops,
			    struct module *owner, const char *mod_name)
{
	int ret = 0;
	struct icnss_priv *priv = icnss_get_plat_priv();

	if (!priv || !priv->pdev) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("Registering driver, state: 0x%lx\n", priv->state);

	if (priv->ops) {
		icnss_pr_err("Driver already registered\n");
		ret = -EEXIST;
		goto out;
	}

	if (!ops->probe || !ops->remove) {
		ret = -EINVAL;
		goto out;
	}

	ret = icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_REGISTER_DRIVER,
				      0, ops);

	if (ret == -EINTR)
		ret = 0;

out:
	return ret;
}
EXPORT_SYMBOL(__icnss_register_driver);

int icnss_unregister_driver(struct icnss_driver_ops *ops)
{
	int ret;
	struct icnss_priv *priv = icnss_get_plat_priv();

	if (!priv || !priv->pdev) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("Unregistering driver, state: 0x%lx\n", priv->state);

	if (!priv->ops) {
		icnss_pr_err("Driver not registered\n");
		ret = -ENOENT;
		goto out;
	}

	ret = icnss_driver_event_post(priv,
					 ICNSS_DRIVER_EVENT_UNREGISTER_DRIVER,
				      ICNSS_EVENT_SYNC_UNINTERRUPTIBLE, NULL);
out:
	return ret;
}
EXPORT_SYMBOL(icnss_unregister_driver);

static struct icnss_msi_config msi_config = {
	.total_vectors = 28,
	.total_users = 2,
	.users = (struct icnss_msi_user[]) {
		{ .name = "CE", .num_vectors = 10, .base_vector = 0 },
		{ .name = "DP", .num_vectors = 18, .base_vector = 10 },
	},
};

static int icnss_get_msi_assignment(struct icnss_priv *priv)
{
	priv->msi_config = &msi_config;

	return 0;
}

int icnss_get_user_msi_assignment(struct device *dev, char *user_name,
				 int *num_vectors, u32 *user_base_data,
				 u32 *base_vector)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	struct icnss_msi_config *msi_config;
	int idx;

	if (!priv)
		return -ENODEV;

	msi_config = priv->msi_config;
	if (!msi_config) {
		icnss_pr_err("MSI is not supported.\n");
		return -EINVAL;
	}

	for (idx = 0; idx < msi_config->total_users; idx++) {
		if (strcmp(user_name, msi_config->users[idx].name) == 0) {
			*num_vectors = msi_config->users[idx].num_vectors;
			*user_base_data = msi_config->users[idx].base_vector
				+ priv->msi_base_data;
			*base_vector = msi_config->users[idx].base_vector;

			icnss_pr_dbg("Assign MSI to user: %s, num_vectors: %d, user_base_data: %u, base_vector: %u\n",
				    user_name, *num_vectors, *user_base_data,
				    *base_vector);

			return 0;
		}
	}

	icnss_pr_err("Failed to find MSI assignment for %s!\n", user_name);

	return -EINVAL;
}
EXPORT_SYMBOL(icnss_get_user_msi_assignment);

int icnss_get_msi_irq(struct device *dev, unsigned int vector)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int irq_num;

	irq_num = priv->srng_irqs[vector];
	icnss_pr_dbg("Get IRQ number %d for vector index %d\n",
		     irq_num, vector);

	return irq_num;
}
EXPORT_SYMBOL(icnss_get_msi_irq);

void icnss_get_msi_address(struct device *dev, u32 *msi_addr_low,
			   u32 *msi_addr_high)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	*msi_addr_low = lower_32_bits(priv->msi_addr_iova);
	*msi_addr_high = upper_32_bits(priv->msi_addr_iova);

}
EXPORT_SYMBOL(icnss_get_msi_address);

int icnss_ce_request_irq(struct device *dev, unsigned int ce_id,
	irqreturn_t (*handler)(int, void *),
		unsigned long flags, const char *name, void *ctx)
{
	int ret = 0;
	unsigned int irq;
	struct ce_irq_list *irq_entry;
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv || !priv->pdev) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_vdbg("CE request IRQ: %d, state: 0x%lx\n", ce_id, priv->state);

	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS) {
		icnss_pr_err("Invalid CE ID, ce_id: %d\n", ce_id);
		ret = -EINVAL;
		goto out;
	}
	irq = priv->ce_irqs[ce_id];
	irq_entry = &priv->ce_irq_list[ce_id];

	if (irq_entry->handler || irq_entry->irq) {
		icnss_pr_err("IRQ already requested: %d, ce_id: %d\n",
			     irq, ce_id);
		ret = -EEXIST;
		goto out;
	}

	ret = request_irq(irq, handler, flags, name, ctx);
	if (ret) {
		icnss_pr_err("IRQ request failed: %d, ce_id: %d, ret: %d\n",
			     irq, ce_id, ret);
		goto out;
	}
	irq_entry->irq = irq;
	irq_entry->handler = handler;

	icnss_pr_vdbg("IRQ requested: %d, ce_id: %d\n", irq, ce_id);

	penv->stats.ce_irqs[ce_id].request++;
out:
	return ret;
}
EXPORT_SYMBOL(icnss_ce_request_irq);

int icnss_ce_free_irq(struct device *dev, unsigned int ce_id, void *ctx)
{
	int ret = 0;
	unsigned int irq;
	struct ce_irq_list *irq_entry;

	if (!penv || !penv->pdev || !dev) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_vdbg("CE free IRQ: %d, state: 0x%lx\n", ce_id, penv->state);

	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS) {
		icnss_pr_err("Invalid CE ID to free, ce_id: %d\n", ce_id);
		ret = -EINVAL;
		goto out;
	}

	irq = penv->ce_irqs[ce_id];
	irq_entry = &penv->ce_irq_list[ce_id];
	if (!irq_entry->handler || !irq_entry->irq) {
		icnss_pr_err("IRQ not requested: %d, ce_id: %d\n", irq, ce_id);
		ret = -EEXIST;
		goto out;
	}
	free_irq(irq, ctx);
	irq_entry->irq = 0;
	irq_entry->handler = NULL;

	penv->stats.ce_irqs[ce_id].free++;
out:
	return ret;
}
EXPORT_SYMBOL(icnss_ce_free_irq);

void icnss_enable_irq(struct device *dev, unsigned int ce_id)
{
	unsigned int irq;

	if (!penv || !penv->pdev || !dev) {
		icnss_pr_err("Platform driver not initialized\n");
		return;
	}

	icnss_pr_vdbg("Enable IRQ: ce_id: %d, state: 0x%lx\n", ce_id,
		     penv->state);

	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS) {
		icnss_pr_err("Invalid CE ID to enable IRQ, ce_id: %d\n", ce_id);
		return;
	}

	penv->stats.ce_irqs[ce_id].enable++;

	irq = penv->ce_irqs[ce_id];
	enable_irq(irq);
}
EXPORT_SYMBOL(icnss_enable_irq);

void icnss_disable_irq(struct device *dev, unsigned int ce_id)
{
	unsigned int irq;

	if (!penv || !penv->pdev || !dev) {
		icnss_pr_err("Platform driver not initialized\n");
		return;
	}

	icnss_pr_vdbg("Disable IRQ: ce_id: %d, state: 0x%lx\n", ce_id,
		     penv->state);

	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS) {
		icnss_pr_err("Invalid CE ID to disable IRQ, ce_id: %d\n",
			     ce_id);
		return;
	}

	irq = penv->ce_irqs[ce_id];
	disable_irq(irq);

	penv->stats.ce_irqs[ce_id].disable++;
}
EXPORT_SYMBOL(icnss_disable_irq);

int icnss_get_soc_info(struct device *dev, struct icnss_soc_info *info)
{
	char *fw_build_timestamp = NULL;
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Platform driver not initialized\n");
		return -EINVAL;
	}

	info->v_addr = priv->mem_base_va;
	info->p_addr = priv->mem_base_pa;
	info->chip_id = priv->chip_info.chip_id;
	info->chip_family = priv->chip_info.chip_family;
	info->board_id = priv->board_id;
	info->soc_id = priv->soc_id;
	info->fw_version = priv->fw_version_info.fw_version;
	fw_build_timestamp = priv->fw_version_info.fw_build_timestamp;
	fw_build_timestamp[WLFW_MAX_TIMESTAMP_LEN] = '\0';
	strlcpy(info->fw_build_timestamp,
		priv->fw_version_info.fw_build_timestamp,
		WLFW_MAX_TIMESTAMP_LEN + 1);

	return 0;
}
EXPORT_SYMBOL(icnss_get_soc_info);

int icnss_get_mhi_state(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Platform driver not initialized\n");
		return -EINVAL;
	}

	if (!priv->mhi_state_info_va)
		return -ENOMEM;

	return ioread32(priv->mhi_state_info_va);
}
EXPORT_SYMBOL(icnss_get_mhi_state);

int icnss_set_fw_log_mode(struct device *dev, uint8_t fw_log_mode)
{
	int ret;
	struct icnss_priv *priv;

	if (!dev)
		return -ENODEV;

	priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Platform driver not initialized\n");
		return -EINVAL;
	}

	if (test_bit(ICNSS_FW_DOWN, &penv->state) ||
	    !test_bit(ICNSS_FW_READY, &penv->state)) {
		icnss_pr_err("FW down, ignoring fw_log_mode state: 0x%lx\n",
			     priv->state);
		return -EINVAL;
	}

	icnss_pr_dbg("FW log mode: %u\n", fw_log_mode);

	ret = wlfw_ini_send_sync_msg(priv, fw_log_mode);
	if (ret)
		icnss_pr_err("Fail to send ini, ret = %d, fw_log_mode: %u\n",
			     ret, fw_log_mode);
	return ret;
}
EXPORT_SYMBOL(icnss_set_fw_log_mode);

int icnss_force_wake_request(struct device *dev)
{
	struct icnss_priv *priv;

	if (!dev)
		return -ENODEV;

	priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Platform driver not initialized\n");
		return -EINVAL;
	}

	if (atomic_inc_not_zero(&priv->soc_wake_ref_count)) {
		icnss_pr_soc_wake("SOC already awake, Ref count: %d",
				  atomic_read(&priv->soc_wake_ref_count));
		return 0;
	}

	icnss_pr_soc_wake("Calling SOC Wake request");

	icnss_soc_wake_event_post(priv, ICNSS_SOC_WAKE_REQUEST_EVENT,
				  0, NULL);

	return 0;
}
EXPORT_SYMBOL(icnss_force_wake_request);

int icnss_force_wake_release(struct device *dev)
{
	struct icnss_priv *priv;

	if (!dev)
		return -ENODEV;

	priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Platform driver not initialized\n");
		return -EINVAL;
	}

	icnss_pr_soc_wake("Calling SOC Wake response");

	if (atomic_read(&priv->soc_wake_ref_count) &&
	    icnss_atomic_dec_if_greater_one(&priv->soc_wake_ref_count)) {
		icnss_pr_soc_wake("SOC previous release pending, Ref count: %d",
				  atomic_read(&priv->soc_wake_ref_count));
		return 0;
	}

	icnss_soc_wake_event_post(priv, ICNSS_SOC_WAKE_RELEASE_EVENT,
				  0, NULL);

	return 0;
}
EXPORT_SYMBOL(icnss_force_wake_release);

int icnss_is_device_awake(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Platform driver not initialized\n");
		return -EINVAL;
	}

	return atomic_read(&priv->soc_wake_ref_count);
}
EXPORT_SYMBOL(icnss_is_device_awake);

int icnss_is_pci_ep_awake(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Platform driver not initialized\n");
		return -EINVAL;
	}

	if (!priv->mhi_state_info_va)
		return -ENOMEM;

	return ioread32(priv->mhi_state_info_va + ICNSS_PCI_EP_WAKE_OFFSET);
}
EXPORT_SYMBOL(icnss_is_pci_ep_awake);

int icnss_athdiag_read(struct device *dev, uint32_t offset,
		       uint32_t mem_type, uint32_t data_len,
		       uint8_t *output)
{
	int ret = 0;
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for diag read: dev %pK, data %pK, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	if (!output || data_len == 0
	    || data_len > WLFW_MAX_DATA_SIZE) {
		icnss_pr_err("Invalid parameters for diag read: output %pK, data_len %u\n",
			     output, data_len);
		ret = -EINVAL;
		goto out;
	}

	if (!test_bit(ICNSS_FW_READY, &priv->state) ||
	    !test_bit(ICNSS_POWER_ON, &priv->state)) {
		icnss_pr_err("Invalid state for diag read: 0x%lx\n",
			     priv->state);
		ret = -EINVAL;
		goto out;
	}

	ret = wlfw_athdiag_read_send_sync_msg(priv, offset, mem_type,
					      data_len, output);
out:
	return ret;
}
EXPORT_SYMBOL(icnss_athdiag_read);

int icnss_athdiag_write(struct device *dev, uint32_t offset,
			uint32_t mem_type, uint32_t data_len,
			uint8_t *input)
{
	int ret = 0;
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for diag write: dev %pK, data %pK, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	if (!input || data_len == 0
	    || data_len > WLFW_MAX_DATA_SIZE) {
		icnss_pr_err("Invalid parameters for diag write: input %pK, data_len %u\n",
			     input, data_len);
		ret = -EINVAL;
		goto out;
	}

	if (!test_bit(ICNSS_FW_READY, &priv->state) ||
	    !test_bit(ICNSS_POWER_ON, &priv->state)) {
		icnss_pr_err("Invalid state for diag write: 0x%lx\n",
			     priv->state);
		ret = -EINVAL;
		goto out;
	}

	ret = wlfw_athdiag_write_send_sync_msg(priv, offset, mem_type,
					       data_len, input);
out:
	return ret;
}
EXPORT_SYMBOL(icnss_athdiag_write);

int icnss_wlan_enable(struct device *dev, struct icnss_wlan_enable_cfg *config,
		      enum icnss_driver_mode mode,
		      const char *host_version)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (test_bit(ICNSS_FW_DOWN, &priv->state) ||
	    !test_bit(ICNSS_FW_READY, &priv->state)) {
		icnss_pr_err("FW down, ignoring wlan_enable state: 0x%lx\n",
			     priv->state);
		return -EINVAL;
	}

	if (test_bit(ICNSS_MODE_ON, &priv->state)) {
		icnss_pr_err("Already Mode on, ignoring wlan_enable state: 0x%lx\n",
			     priv->state);
		return -EINVAL;
	}

	if (priv->device_id == WCN6750_DEVICE_ID &&
	    !priv->dms.nv_mac_not_prov && !priv->dms.mac_valid)
		icnss_setup_dms_mac(priv);

	return icnss_send_wlan_enable_to_fw(priv, config, mode, host_version);
}
EXPORT_SYMBOL(icnss_wlan_enable);

int icnss_wlan_disable(struct device *dev, enum icnss_driver_mode mode)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (test_bit(ICNSS_FW_DOWN, &priv->state)) {
		icnss_pr_dbg("FW down, ignoring wlan_disable state: 0x%lx\n",
			     priv->state);
		return 0;
	}

	return icnss_send_wlan_disable_to_fw(priv);
}
EXPORT_SYMBOL(icnss_wlan_disable);

bool icnss_is_qmi_disable(struct device *dev)
{
	return test_bit(SKIP_QMI, &penv->ctrl_params.quirks) ? true : false;
}
EXPORT_SYMBOL(icnss_is_qmi_disable);

int icnss_get_ce_id(struct device *dev, int irq)
{
	int i;

	if (!penv || !penv->pdev || !dev)
		return -ENODEV;

	for (i = 0; i < ICNSS_MAX_IRQ_REGISTRATIONS; i++) {
		if (penv->ce_irqs[i] == irq)
			return i;
	}

	icnss_pr_err("No matching CE id for irq %d\n", irq);

	return -EINVAL;
}
EXPORT_SYMBOL(icnss_get_ce_id);

int icnss_get_irq(struct device *dev, int ce_id)
{
	int irq;

	if (!penv || !penv->pdev || !dev)
		return -ENODEV;

	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS)
		return -EINVAL;

	irq = penv->ce_irqs[ce_id];

	return irq;
}
EXPORT_SYMBOL(icnss_get_irq);

struct iommu_domain *icnss_smmu_get_domain(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Invalid drvdata: dev %pK\n", dev);
		return NULL;
	}
	return priv->iommu_domain;
}
EXPORT_SYMBOL(icnss_smmu_get_domain);

int icnss_smmu_map(struct device *dev,
		   phys_addr_t paddr, uint32_t *iova_addr, size_t size)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int flag = IOMMU_READ | IOMMU_WRITE;
	bool dma_coherent = false;
	unsigned long iova;
	int prop_len = 0;
	size_t len;
	int ret = 0;

	if (!priv) {
		icnss_pr_err("Invalid drvdata: dev %pK, data %pK\n",
			     dev, priv);
		return -EINVAL;
	}

	if (!iova_addr) {
		icnss_pr_err("iova_addr is NULL, paddr %pa, size %zu\n",
			     &paddr, size);
		return -EINVAL;
	}

	len = roundup(size + paddr - rounddown(paddr, PAGE_SIZE), PAGE_SIZE);
	iova = roundup(priv->smmu_iova_ipa_current, PAGE_SIZE);

	if (of_get_property(dev->of_node, "qcom,iommu-geometry", &prop_len) &&
	    iova >= priv->smmu_iova_ipa_start + priv->smmu_iova_ipa_len) {
		icnss_pr_err("No IOVA space to map, iova %lx, smmu_iova_ipa_start %pad, smmu_iova_ipa_len %zu\n",
			     iova,
			     &priv->smmu_iova_ipa_start,
			     priv->smmu_iova_ipa_len);
		return -ENOMEM;
	}

	dma_coherent = of_property_read_bool(dev->of_node, "dma-coherent");
	icnss_pr_dbg("dma-coherent is %s\n",
		     dma_coherent ? "enabled" : "disabled");
	if (dma_coherent)
		flag |= IOMMU_CACHE;

	icnss_pr_dbg("IOMMU Map: iova %lx, len %zu\n", iova, len);

	ret = iommu_map(priv->iommu_domain, iova,
			rounddown(paddr, PAGE_SIZE), len,
			flag);
	if (ret) {
		icnss_pr_err("PA to IOVA mapping failed, ret %d\n", ret);
		return ret;
	}

	priv->smmu_iova_ipa_current = iova + len;
	*iova_addr = (uint32_t)(iova + paddr - rounddown(paddr, PAGE_SIZE));

	icnss_pr_dbg("IOVA addr mapped to physical addr %lx\n", *iova_addr);
	return 0;
}
EXPORT_SYMBOL(icnss_smmu_map);

int icnss_smmu_unmap(struct device *dev,
		     uint32_t iova_addr, size_t size)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	unsigned long iova;
	size_t len, unmapped_len;

	if (!priv) {
		icnss_pr_err("Invalid drvdata: dev %pK, data %pK\n",
			     dev, priv);
		return -EINVAL;
	}

	if (!iova_addr) {
		icnss_pr_err("iova_addr is NULL, size %zu\n",
			     size);
		return -EINVAL;
	}

	len = roundup(size + iova_addr - rounddown(iova_addr, PAGE_SIZE),
		      PAGE_SIZE);
	iova = rounddown(iova_addr, PAGE_SIZE);

	if (iova >= priv->smmu_iova_ipa_start + priv->smmu_iova_ipa_len) {
		icnss_pr_err("Out of IOVA space during unmap, iova %lx, smmu_iova_ipa_start %pad, smmu_iova_ipa_len %zu\n",
			     iova,
			     &priv->smmu_iova_ipa_start,
			     priv->smmu_iova_ipa_len);
		return -ENOMEM;
	}

	icnss_pr_dbg("IOMMU Unmap: iova %lx, len %zu\n",
		     iova, len);

	unmapped_len = iommu_unmap(priv->iommu_domain, iova, len);
	if (unmapped_len != len) {
		icnss_pr_err("Failed to unmap, %zu\n", unmapped_len);
		return -EINVAL;
	}

	priv->smmu_iova_ipa_current = iova;
	return 0;
}
EXPORT_SYMBOL(icnss_smmu_unmap);

unsigned int icnss_socinfo_get_serial_number(struct device *dev)
{
	return socinfo_get_serial_number();
}
EXPORT_SYMBOL(icnss_socinfo_get_serial_number);

int icnss_trigger_recovery(struct device *dev)
{
	int ret = 0;
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata: magic 0x%x\n", priv->magic);
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(ICNSS_PD_RESTART, &priv->state)) {
		icnss_pr_err("PD recovery already in progress: state: 0x%lx\n",
			     priv->state);
		ret = -EPERM;
		goto out;
	}

	if (priv->device_id == WCN6750_DEVICE_ID) {
		icnss_pr_vdbg("Initiate Root PD restart");
		ret = icnss_send_smp2p(priv, ICNSS_TRIGGER_SSR);
		if (!ret)
			set_bit(ICNSS_HOST_TRIGGERED_PDR, &priv->state);
		return ret;
	}

	if (!test_bit(ICNSS_PDR_REGISTERED, &priv->state)) {
		icnss_pr_err("PD restart not enabled to trigger recovery: state: 0x%lx\n",
			     priv->state);
		ret = -EOPNOTSUPP;
		goto out;
	}

	icnss_pr_warn("Initiate PD restart at WLAN FW, state: 0x%lx\n",
		      priv->state);

	ret = pdr_restart_pd(priv->pdr_handle, priv->pdr_service);

	if (!ret)
		set_bit(ICNSS_HOST_TRIGGERED_PDR, &priv->state);

out:
	return ret;
}
EXPORT_SYMBOL(icnss_trigger_recovery);

int icnss_idle_shutdown(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Invalid drvdata: dev %pK", dev);
		return -EINVAL;
	}

	if (priv->is_ssr || test_bit(ICNSS_PDR, &priv->state) ||
	    test_bit(ICNSS_REJUVENATE, &priv->state)) {
		icnss_pr_err("SSR/PDR is already in-progress during idle shutdown\n");
		return -EBUSY;
	}

	return icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_IDLE_SHUTDOWN,
					ICNSS_EVENT_SYNC_UNINTERRUPTIBLE, NULL);
}
EXPORT_SYMBOL(icnss_idle_shutdown);

int icnss_idle_restart(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Invalid drvdata: dev %pK", dev);
		return -EINVAL;
	}

	if (priv->is_ssr || test_bit(ICNSS_PDR, &priv->state) ||
	    test_bit(ICNSS_REJUVENATE, &priv->state)) {
		icnss_pr_err("SSR/PDR is already in-progress during idle restart\n");
		return -EBUSY;
	}

	return icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_IDLE_RESTART,
					ICNSS_EVENT_SYNC_UNINTERRUPTIBLE, NULL);
}
EXPORT_SYMBOL(icnss_idle_restart);

int icnss_exit_power_save(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	icnss_pr_vdbg("Calling Exit Power Save\n");

	if (test_bit(ICNSS_PD_RESTART, &priv->state) ||
	    !test_bit(ICNSS_MODE_ON, &priv->state))
		return 0;

	return icnss_send_smp2p(priv, ICNSS_POWER_SAVE_EXIT);
}
EXPORT_SYMBOL(icnss_exit_power_save);

int icnss_prevent_l1(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (test_bit(ICNSS_PD_RESTART, &priv->state) ||
	    !test_bit(ICNSS_MODE_ON, &priv->state))
		return 0;

	return icnss_send_smp2p(priv, ICNSS_PCI_EP_POWER_SAVE_EXIT);
}
EXPORT_SYMBOL(icnss_prevent_l1);

void icnss_allow_l1(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (test_bit(ICNSS_PD_RESTART, &priv->state) ||
	    !test_bit(ICNSS_MODE_ON, &priv->state))
		return;

	icnss_send_smp2p(priv, ICNSS_PCI_EP_POWER_SAVE_ENTER);
}
EXPORT_SYMBOL(icnss_allow_l1);

void icnss_allow_recursive_recovery(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	priv->allow_recursive_recovery = true;

	icnss_pr_info("Recursive recovery allowed for WLAN\n");
}

void icnss_disallow_recursive_recovery(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	priv->allow_recursive_recovery = false;

	icnss_pr_info("Recursive recovery disallowed for WLAN\n");
}

static int icnss_create_shutdown_sysfs(struct icnss_priv *priv)
{
	struct kobject *icnss_kobject;
	int ret = 0;

	atomic_set(&priv->is_shutdown, false);

	icnss_kobject = kobject_create_and_add("shutdown_wlan", kernel_kobj);
	if (!icnss_kobject) {
		icnss_pr_err("Unable to create shutdown_wlan kernel object");
		return -EINVAL;
	}

	priv->icnss_kobject = icnss_kobject;

	ret = sysfs_create_file(icnss_kobject, &icnss_sysfs_attribute.attr);
	if (ret) {
		icnss_pr_err("Unable to create icnss sysfs file err:%d", ret);
		return ret;
	}

	return ret;
}

static void icnss_destroy_shutdown_sysfs(struct icnss_priv *priv)
{
	struct kobject *icnss_kobject;

	icnss_kobject = priv->icnss_kobject;
	if (icnss_kobject)
		kobject_put(icnss_kobject);
}

static ssize_t qdss_tr_start_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	wlfw_qdss_trace_start(priv);
	icnss_pr_dbg("Received QDSS start command\n");
	return count;
}

static ssize_t qdss_tr_stop_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *user_buf, size_t count)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	u32 option = 0;

	if (sscanf(user_buf, "%du", &option) != 1)
		return -EINVAL;

	wlfw_qdss_trace_stop(priv, option);
	icnss_pr_dbg("Received QDSS stop command\n");
	return count;
}

static ssize_t qdss_conf_download_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	icnss_wlfw_qdss_dnld_send_sync(priv);
	icnss_pr_dbg("Received QDSS download config command\n");
	return count;
}

static ssize_t hw_trc_override_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int tmp = 0;

	if (sscanf(buf, "%du", &tmp) != 1)
		return -EINVAL;

	priv->hw_trc_override = tmp;
	icnss_pr_dbg("Received QDSS hw_trc_override indication\n");
	return count;
}

static void icnss_wpss_load(struct work_struct *wpss_load_work)
{
	struct icnss_priv *priv = icnss_get_plat_priv();
	phandle rproc_phandle;
	int ret;

	if (of_property_read_u32(priv->pdev->dev.of_node, "qcom,rproc-handle",
				 &rproc_phandle)) {
		icnss_pr_err("error reading rproc phandle\n");
		return;
	}

	priv->rproc = rproc_get_by_phandle(rproc_phandle);
	if (IS_ERR_OR_NULL(priv->rproc)) {
		icnss_pr_err("rproc not found");
		return;
	}

	ret = rproc_boot(priv->rproc);
	if (ret) {
		icnss_pr_err("Failed to boot wpss rproc, ret: %d", ret);
		rproc_put(priv->rproc);
	}
}

static inline void icnss_wpss_unload(struct icnss_priv *priv)
{
	if (priv && priv->rproc) {
		rproc_shutdown(priv->rproc);
		rproc_put(priv->rproc);
		priv->rproc = NULL;
	}
}

static ssize_t wpss_boot_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int wpss_rproc = 0;

	if (priv->device_id != WCN6750_DEVICE_ID)
		return count;

	if (sscanf(buf, "%du", &wpss_rproc) != 1) {
		icnss_pr_err("Failed to read wpss rproc info");
		return -EINVAL;
	}

	icnss_pr_dbg("WPSS Remote Processor: %s", wpss_rproc ? "GET" : "PUT");

	if (wpss_rproc == 1)
		schedule_work(&wpss_loader);
	else if (wpss_rproc == 0)
		icnss_wpss_unload(priv);

	return count;
}

static DEVICE_ATTR_WO(qdss_tr_start);
static DEVICE_ATTR_WO(qdss_tr_stop);
static DEVICE_ATTR_WO(qdss_conf_download);
static DEVICE_ATTR_WO(hw_trc_override);
static DEVICE_ATTR_WO(wpss_boot);

static struct attribute *icnss_attrs[] = {
	&dev_attr_qdss_tr_start.attr,
	&dev_attr_qdss_tr_stop.attr,
	&dev_attr_qdss_conf_download.attr,
	&dev_attr_hw_trc_override.attr,
	&dev_attr_wpss_boot.attr,
	NULL,
};

static struct attribute_group icnss_attr_group = {
	.attrs = icnss_attrs,
};

static int icnss_create_sysfs_link(struct icnss_priv *priv)
{
	struct device *dev = &priv->pdev->dev;
	int ret;

	ret = sysfs_create_link(kernel_kobj, &dev->kobj, "icnss");
	if (ret) {
		icnss_pr_err("Failed to create icnss link, err = %d\n",
			     ret);
		goto out;
	}

	return 0;
out:
	return ret;
}

static void icnss_remove_sysfs_link(struct icnss_priv *priv)
{
	sysfs_remove_link(kernel_kobj, "icnss");
}

static int icnss_sysfs_create(struct icnss_priv *priv)
{
	int ret = 0;

	ret = devm_device_add_group(&priv->pdev->dev,
				    &icnss_attr_group);
	if (ret) {
		icnss_pr_err("Failed to create icnss device group, err = %d\n",
			     ret);
		goto out;
	}

	icnss_create_sysfs_link(priv);

	ret = icnss_create_shutdown_sysfs(priv);
	if (ret)
		goto remove_icnss_group;

	return 0;
remove_icnss_group:
	devm_device_remove_group(&priv->pdev->dev, &icnss_attr_group);
out:
	return ret;
}

static void icnss_sysfs_destroy(struct icnss_priv *priv)
{
	icnss_destroy_shutdown_sysfs(priv);
	icnss_remove_sysfs_link(priv);
	devm_device_remove_group(&priv->pdev->dev, &icnss_attr_group);
}

static int icnss_get_vbatt_info(struct icnss_priv *priv)
{
	struct adc_tm_chip *adc_tm_dev = NULL;
	struct iio_channel *channel = NULL;
	int ret = 0;

	adc_tm_dev = get_adc_tm(&priv->pdev->dev, "icnss");
	if (PTR_ERR(adc_tm_dev) == -EPROBE_DEFER) {
		icnss_pr_err("adc_tm_dev probe defer\n");
		return -EPROBE_DEFER;
	}

	if (IS_ERR(adc_tm_dev)) {
		ret = PTR_ERR(adc_tm_dev);
		icnss_pr_err("Not able to get ADC dev, VBATT monitoring is disabled: %d\n",
			     ret);
		return ret;
	}

	channel = devm_iio_channel_get(&priv->pdev->dev, "icnss");
	if (PTR_ERR(channel) == -EPROBE_DEFER) {
		icnss_pr_err("channel probe defer\n");
		return -EPROBE_DEFER;
	}

	if (IS_ERR(channel)) {
		ret = PTR_ERR(channel);
		icnss_pr_err("Not able to get VADC dev, VBATT monitoring is disabled: %d\n",
			     ret);
		return ret;
	}

	priv->adc_tm_dev = adc_tm_dev;
	priv->channel = channel;

	return 0;
}

static int icnss_resource_parse(struct icnss_priv *priv)
{
	int ret = 0, i = 0;
	struct platform_device *pdev = priv->pdev;
	struct device *dev = &pdev->dev;
	struct resource *res;
	u32 int_prop;

	if (of_property_read_bool(pdev->dev.of_node, "qcom,icnss-adc_tm")) {
		ret = icnss_get_vbatt_info(priv);
		if (ret == -EPROBE_DEFER)
			goto out;
		priv->vbatt_supported = true;
	}

	ret = icnss_get_vreg(priv);
	if (ret) {
		icnss_pr_err("Failed to get vreg, err = %d\n", ret);
		goto out;
	}

	ret = icnss_get_clk(priv);
	if (ret) {
		icnss_pr_err("Failed to get clocks, err = %d\n", ret);
		goto put_vreg;
	}

	if (priv->device_id == ADRASTEA_DEVICE_ID) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   "membase");
		if (!res) {
			icnss_pr_err("Memory base not found in DT\n");
			ret = -EINVAL;
			goto put_clk;
		}

		priv->mem_base_pa = res->start;
		priv->mem_base_va = devm_ioremap(dev, priv->mem_base_pa,
						 resource_size(res));
		if (!priv->mem_base_va) {
			icnss_pr_err("Memory base ioremap failed: phy addr: %pa\n",
				     &priv->mem_base_pa);
			ret = -EINVAL;
			goto put_clk;
		}
		icnss_pr_dbg("MEM_BASE pa: %pa, va: 0x%pK\n",
			     &priv->mem_base_pa,
			     priv->mem_base_va);

		for (i = 0; i < ICNSS_MAX_IRQ_REGISTRATIONS; i++) {
			res = platform_get_resource(priv->pdev,
						    IORESOURCE_IRQ, i);
			if (!res) {
				icnss_pr_err("Fail to get IRQ-%d\n", i);
				ret = -ENODEV;
				goto put_clk;
			} else {
				priv->ce_irqs[i] = res->start;
			}
		}
	} else if (priv->device_id == WCN6750_DEVICE_ID) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   "msi_addr");
		if (!res) {
			icnss_pr_err("MSI address not found in DT\n");
			ret = -EINVAL;
			goto put_clk;
		}

		priv->msi_addr_pa = res->start;
		priv->msi_addr_iova = dma_map_resource(dev, priv->msi_addr_pa,
						       PAGE_SIZE,
						       DMA_FROM_DEVICE, 0);
		if (dma_mapping_error(dev, priv->msi_addr_iova)) {
			icnss_pr_err("MSI: failed to map msi address\n");
			priv->msi_addr_iova = 0;
			ret = -ENOMEM;
			goto put_clk;
		}
		icnss_pr_dbg("MSI Addr pa: %pa, iova: 0x%pK\n",
			     &priv->msi_addr_pa,
			     priv->msi_addr_iova);

		ret = of_property_read_u32_index(dev->of_node,
						 "interrupts",
						 1,
						 &int_prop);
		if (ret) {
			icnss_pr_dbg("Read interrupt prop failed");
			goto put_clk;
		}

		priv->msi_base_data = int_prop + 32;
		icnss_pr_dbg(" MSI Base Data: %d, IRQ Index: %d\n",
			     priv->msi_base_data, int_prop);

		icnss_get_msi_assignment(priv);
		for (i = 0; i < msi_config.total_vectors; i++) {
			res = platform_get_resource(priv->pdev,
						    IORESOURCE_IRQ, i);
			if (!res) {
				icnss_pr_err("Fail to get IRQ-%d\n", i);
				ret = -ENODEV;
				goto put_clk;
			} else {
				priv->srng_irqs[i] = res->start;
			}
		}
	}

	return 0;

put_clk:
	icnss_put_clk(priv);
put_vreg:
	icnss_put_vreg(priv);
out:
	return ret;
}

static int icnss_msa_dt_parse(struct icnss_priv *priv)
{
	int ret = 0;
	struct platform_device *pdev = priv->pdev;
	struct device *dev = &pdev->dev;
	struct device_node *np = NULL;
	u64 prop_size = 0;
	const __be32 *addrp = NULL;

	np = of_parse_phandle(dev->of_node,
			      "qcom,wlan-msa-fixed-region", 0);
	if (np) {
		addrp = of_get_address(np, 0, &prop_size, NULL);
		if (!addrp) {
			icnss_pr_err("Failed to get assigned-addresses or property\n");
			ret = -EINVAL;
			of_node_put(np);
			goto out;
		}

		priv->msa_pa = of_translate_address(np, addrp);
		if (priv->msa_pa == OF_BAD_ADDR) {
			icnss_pr_err("Failed to translate MSA PA from device-tree\n");
			ret = -EINVAL;
			of_node_put(np);
			goto out;
		}

		of_node_put(np);

		priv->msa_va = memremap(priv->msa_pa,
					(unsigned long)prop_size, MEMREMAP_WT);
		if (!priv->msa_va) {
			icnss_pr_err("MSA PA ioremap failed: phy addr: %pa\n",
				     &priv->msa_pa);
			ret = -EINVAL;
			goto out;
		}
		priv->msa_mem_size = prop_size;
	} else {
		ret = of_property_read_u32(dev->of_node, "qcom,wlan-msa-memory",
					   &priv->msa_mem_size);
		if (ret || priv->msa_mem_size == 0) {
			icnss_pr_err("Fail to get MSA Memory Size: %u ret: %d\n",
				     priv->msa_mem_size, ret);
			goto out;
		}

		priv->msa_va = dmam_alloc_coherent(&pdev->dev,
				priv->msa_mem_size, &priv->msa_pa, GFP_KERNEL);

		if (!priv->msa_va) {
			icnss_pr_err("DMA alloc failed for MSA\n");
			ret = -ENOMEM;
			goto out;
		}
	}

	icnss_pr_dbg("MSA pa: %pa, MSA va: 0x%pK MSA Memory Size: 0x%x\n",
		     &priv->msa_pa, (void *)priv->msa_va, priv->msa_mem_size);

	priv->use_prefix_path = of_property_read_bool(priv->pdev->dev.of_node,
						      "qcom,fw-prefix");
	return 0;

out:
	return ret;
}

static int icnss_smmu_fault_handler(struct iommu_domain *domain,
				    struct device *dev, unsigned long iova,
				    int flags, void *handler_token)
{
	struct icnss_priv *priv = handler_token;
	struct icnss_uevent_fw_down_data fw_down_data = {0};

	icnss_fatal_err("SMMU fault happened with IOVA 0x%lx\n", iova);

	if (!priv) {
		icnss_pr_err("priv is NULL\n");
		return -ENODEV;
	}

	if (test_bit(ICNSS_FW_READY, &priv->state)) {
		fw_down_data.crashed = true;
		icnss_call_driver_uevent(priv, ICNSS_UEVENT_FW_DOWN,
					 &fw_down_data);
	}

	icnss_trigger_recovery(&priv->pdev->dev);

	/* IOMMU driver requires non-zero return value to print debug info. */
	return -EINVAL;
}

static int icnss_smmu_dt_parse(struct icnss_priv *priv)
{
	int ret = 0;
	struct platform_device *pdev = priv->pdev;
	struct device *dev = &pdev->dev;
	const char *iommu_dma_type;
	struct resource *res;
	u32 addr_win[2];

	ret = of_property_read_u32_array(dev->of_node,
					 "qcom,iommu-dma-addr-pool",
					 addr_win,
					 ARRAY_SIZE(addr_win));

	if (ret) {
		icnss_pr_err("SMMU IOVA base not found\n");
	} else {
		priv->smmu_iova_start = addr_win[0];
		priv->smmu_iova_len = addr_win[1];
		icnss_pr_dbg("SMMU IOVA start: %pa, len: %zx\n",
			     &priv->smmu_iova_start,
			     priv->smmu_iova_len);

		priv->iommu_domain =
			iommu_get_domain_for_dev(&pdev->dev);

		ret = of_property_read_string(dev->of_node, "qcom,iommu-dma",
					      &iommu_dma_type);
		if (!ret && !strcmp("fastmap", iommu_dma_type)) {
			icnss_pr_dbg("SMMU S1 stage enabled\n");
			priv->smmu_s1_enable = true;
			if (priv->device_id == WCN6750_DEVICE_ID)
				iommu_set_fault_handler(priv->iommu_domain,
						icnss_smmu_fault_handler,
						priv);
		}

		res = platform_get_resource_byname(pdev,
						   IORESOURCE_MEM,
						   "smmu_iova_ipa");
		if (!res) {
			icnss_pr_err("SMMU IOVA IPA not found\n");
		} else {
			priv->smmu_iova_ipa_start = res->start;
			priv->smmu_iova_ipa_current = res->start;
			priv->smmu_iova_ipa_len = resource_size(res);
			icnss_pr_dbg("SMMU IOVA IPA start: %pa, len: %zx\n",
				     &priv->smmu_iova_ipa_start,
				     priv->smmu_iova_ipa_len);
		}
	}

	return 0;
}

int icnss_get_iova(struct icnss_priv *priv, u64 *addr, u64 *size)
{
	if (!priv)
		return -ENODEV;

	if (!priv->smmu_iova_len)
		return -EINVAL;

	*addr = priv->smmu_iova_start;
	*size = priv->smmu_iova_len;

	return 0;
}

int icnss_get_iova_ipa(struct icnss_priv *priv, u64 *addr, u64 *size)
{
	if (!priv)
		return -ENODEV;

	if (!priv->smmu_iova_ipa_len)
		return -EINVAL;

	*addr = priv->smmu_iova_ipa_start;
	*size = priv->smmu_iova_ipa_len;

	return 0;
}

void icnss_add_fw_prefix_name(struct icnss_priv *priv, char *prefix_name,
			      char *name)
{
	if (!priv)
		return;

	if (!priv->use_prefix_path) {
		scnprintf(prefix_name, ICNSS_MAX_FILE_NAME, "%s", name);
		return;
	}

	scnprintf(prefix_name, ICNSS_MAX_FILE_NAME,
		  QCA6750_PATH_PREFIX "%s", name);

	icnss_pr_dbg("File added with prefix: %s\n", prefix_name);
}

static const struct platform_device_id icnss_platform_id_table[] = {
	{ .name = "wcn6750", .driver_data = WCN6750_DEVICE_ID, },
	{ .name = "adrastea", .driver_data = ADRASTEA_DEVICE_ID, },
	{ },
};

static const struct of_device_id icnss_dt_match[] = {
	{
		.compatible = "qcom,wcn6750",
		.data = (void *)&icnss_platform_id_table[0]},
	{
		.compatible = "qcom,icnss",
		.data = (void *)&icnss_platform_id_table[1]},
	{ },
};

MODULE_DEVICE_TABLE(of, icnss_dt_match);

static void icnss_init_control_params(struct icnss_priv *priv)
{
	priv->ctrl_params.qmi_timeout = WLFW_TIMEOUT;
	priv->ctrl_params.quirks = ICNSS_QUIRKS_DEFAULT;
	priv->ctrl_params.bdf_type = ICNSS_BDF_TYPE_DEFAULT;

	if (of_property_read_bool(priv->pdev->dev.of_node,
				  "cnss-daemon-support")) {
		priv->ctrl_params.quirks |= BIT(ENABLE_DAEMON_SUPPORT);
	}
}

static inline void  icnss_get_smp2p_info(struct icnss_priv *priv)
{

	priv->smp2p_info.smem_state =
			qcom_smem_state_get(&priv->pdev->dev,
					    "wlan-smp2p-out",
					    &priv->smp2p_info.smem_bit);
	if (IS_ERR(priv->smp2p_info.smem_state)) {
		icnss_pr_smp2p("Failed to get smem state %d",
			     PTR_ERR(priv->smp2p_info.smem_state));
	}

}

static inline void icnss_runtime_pm_init(struct icnss_priv *priv)
{
	pm_runtime_get_sync(&priv->pdev->dev);
	pm_runtime_forbid(&priv->pdev->dev);
	pm_runtime_set_active(&priv->pdev->dev);
	pm_runtime_enable(&priv->pdev->dev);
}

static inline void icnss_runtime_pm_deinit(struct icnss_priv *priv)
{
	pm_runtime_disable(&priv->pdev->dev);
	pm_runtime_allow(&priv->pdev->dev);
	pm_runtime_put_sync(&priv->pdev->dev);
}

static inline bool icnss_use_nv_mac(struct icnss_priv *priv)
{
	return of_property_read_bool(priv->pdev->dev.of_node,
				     "use-nv-mac");
}

static int icnss_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct icnss_priv *priv;
	const struct of_device_id *of_id;
	const struct platform_device_id *device_id;

	if (dev_get_drvdata(dev)) {
		icnss_pr_err("Driver is already initialized\n");
		return -EEXIST;
	}

	of_id = of_match_device(icnss_dt_match, &pdev->dev);
	if (!of_id || !of_id->data) {
		icnss_pr_err("Failed to find of match device!\n");
		ret = -ENODEV;
		goto out_reset_drvdata;
	}

	device_id = of_id->data;

	icnss_pr_dbg("Platform driver probe\n");

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->magic = ICNSS_MAGIC;
	dev_set_drvdata(dev, priv);

	priv->pdev = pdev;
	priv->device_id = device_id->driver_data;
	priv->is_chain1_supported = true;
	INIT_LIST_HEAD(&priv->vreg_list);
	INIT_LIST_HEAD(&priv->clk_list);
	icnss_allow_recursive_recovery(dev);

	icnss_init_control_params(priv);

	ret = icnss_resource_parse(priv);
	if (ret)
		goto out_reset_drvdata;

	ret = icnss_msa_dt_parse(priv);
	if (ret)
		goto out_free_resources;

	ret = icnss_smmu_dt_parse(priv);
	if (ret)
		goto out_free_resources;

	spin_lock_init(&priv->event_lock);
	spin_lock_init(&priv->on_off_lock);
	spin_lock_init(&priv->soc_wake_msg_lock);
	mutex_init(&priv->dev_lock);
	mutex_init(&priv->tcdev_lock);

	priv->event_wq = alloc_workqueue("icnss_driver_event", WQ_UNBOUND, 1);
	if (!priv->event_wq) {
		icnss_pr_err("Workqueue creation failed\n");
		ret = -EFAULT;
		goto smmu_cleanup;
	}

	INIT_WORK(&priv->event_work, icnss_driver_event_work);
	INIT_LIST_HEAD(&priv->event_list);

	priv->soc_wake_wq = alloc_workqueue("icnss_soc_wake_event",
					    WQ_UNBOUND|WQ_HIGHPRI, 1);
	if (!priv->soc_wake_wq) {
		icnss_pr_err("Soc wake Workqueue creation failed\n");
		ret = -EFAULT;
		goto out_destroy_wq;
	}

	INIT_WORK(&priv->soc_wake_msg_work, icnss_soc_wake_msg_work);
	INIT_LIST_HEAD(&priv->soc_wake_msg_list);

	ret = icnss_register_fw_service(priv);
	if (ret < 0) {
		icnss_pr_err("fw service registration failed: %d\n", ret);
		goto out_destroy_soc_wq;
	}

	icnss_enable_recovery(priv);

	icnss_debugfs_create(priv);

	icnss_sysfs_create(priv);

	ret = device_init_wakeup(&priv->pdev->dev, true);
	if (ret)
		icnss_pr_err("Failed to init platform device wakeup source, err = %d\n",
			     ret);

	icnss_set_plat_priv(priv);

	init_completion(&priv->unblock_shutdown);

	if (priv->device_id == WCN6750_DEVICE_ID) {
		ret = icnss_dms_init(priv);
		if (ret)
			icnss_pr_err("ICNSS DMS init failed %d\n", ret);
		ret = icnss_genl_init();
		if (ret < 0)
			icnss_pr_err("ICNSS genl init failed %d\n", ret);

		icnss_runtime_pm_init(priv);
		icnss_get_cpr_info(priv);
		icnss_get_smp2p_info(priv);
		set_bit(ICNSS_COLD_BOOT_CAL, &priv->state);
		priv->use_nv_mac = icnss_use_nv_mac(priv);
		icnss_pr_dbg("NV MAC feature is %s\n",
			     priv->use_nv_mac ? "Mandatory":"Not Mandatory");
		INIT_WORK(&wpss_loader, icnss_wpss_load);
	}

	INIT_LIST_HEAD(&priv->icnss_tcdev_list);

	icnss_pr_info("Platform driver probed successfully\n");

	return 0;

out_destroy_soc_wq:
	destroy_workqueue(priv->soc_wake_wq);
out_destroy_wq:
	destroy_workqueue(priv->event_wq);
smmu_cleanup:
	priv->iommu_domain = NULL;
out_free_resources:
	icnss_put_resources(priv);
out_reset_drvdata:
	dev_set_drvdata(dev, NULL);
	return ret;
}

static int icnss_remove(struct platform_device *pdev)
{
	struct icnss_priv *priv = dev_get_drvdata(&pdev->dev);

	icnss_pr_info("Removing driver: state: 0x%lx\n", priv->state);

	if (priv->device_id == WCN6750_DEVICE_ID) {
		icnss_dms_deinit(priv);
		icnss_genl_exit();
		icnss_runtime_pm_deinit(priv);
	}

	device_init_wakeup(&priv->pdev->dev, false);

	icnss_debugfs_destroy(priv);

	icnss_sysfs_destroy(priv);

	complete_all(&priv->unblock_shutdown);

	destroy_ramdump_device(priv->msa0_dump_dev);

	if (priv->device_id == WCN6750_DEVICE_ID) {
		icnss_wpss_ssr_unregister_notifier(priv);
		rproc_put(priv->rproc);
		destroy_ramdump_device(priv->m3_dump_dev_seg1);
		destroy_ramdump_device(priv->m3_dump_dev_seg2);
		destroy_ramdump_device(priv->m3_dump_dev_seg3);
		destroy_ramdump_device(priv->m3_dump_dev_seg4);
		destroy_ramdump_device(priv->m3_dump_dev_seg5);
	} else {
		icnss_modem_ssr_unregister_notifier(priv);
		icnss_pdr_unregister_notifier(priv);
	}

	icnss_unregister_fw_service(priv);
	if (priv->event_wq)
		destroy_workqueue(priv->event_wq);

	if (priv->soc_wake_wq)
		destroy_workqueue(priv->soc_wake_wq);

	priv->iommu_domain = NULL;

	icnss_hw_power_off(priv);

	icnss_put_resources(priv);

	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int icnss_pm_suspend(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for pm suspend: dev %pK, data %pK, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	icnss_pr_vdbg("PM Suspend, state: 0x%lx\n", priv->state);

	if (!priv->ops || !priv->ops->pm_suspend ||
	    IS_ERR(priv->smp2p_info.smem_state) ||
	    !test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		return 0;

	ret = priv->ops->pm_suspend(dev);

	if (ret == 0) {
		if (priv->device_id == WCN6750_DEVICE_ID) {
			if (test_bit(ICNSS_PD_RESTART, &priv->state) ||
			    !test_bit(ICNSS_MODE_ON, &priv->state))
				return 0;

			ret = icnss_send_smp2p(priv, ICNSS_POWER_SAVE_ENTER);
		}
		priv->stats.pm_suspend++;
		set_bit(ICNSS_PM_SUSPEND, &priv->state);
	} else {
		priv->stats.pm_suspend_err++;
	}
	return ret;
}

static int icnss_pm_resume(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for pm resume: dev %pK, data %pK, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	icnss_pr_vdbg("PM resume, state: 0x%lx\n", priv->state);

	if (!priv->ops || !priv->ops->pm_resume ||
	    IS_ERR(priv->smp2p_info.smem_state) ||
	    !test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		goto out;

	ret = priv->ops->pm_resume(dev);

out:
	if (ret == 0) {
		priv->stats.pm_resume++;
		clear_bit(ICNSS_PM_SUSPEND, &priv->state);
	} else {
		priv->stats.pm_resume_err++;
	}
	return ret;
}

static int icnss_pm_suspend_noirq(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for pm suspend_noirq: dev %pK, data %pK, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	icnss_pr_vdbg("PM suspend_noirq, state: 0x%lx\n", priv->state);

	if (!priv->ops || !priv->ops->suspend_noirq ||
	    !test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		goto out;

	ret = priv->ops->suspend_noirq(dev);

out:
	if (ret == 0) {
		priv->stats.pm_suspend_noirq++;
		set_bit(ICNSS_PM_SUSPEND_NOIRQ, &priv->state);
	} else {
		priv->stats.pm_suspend_noirq_err++;
	}
	return ret;
}

static int icnss_pm_resume_noirq(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for pm resume_noirq: dev %pK, data %pK, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	icnss_pr_vdbg("PM resume_noirq, state: 0x%lx\n", priv->state);

	if (!priv->ops || !priv->ops->resume_noirq ||
	    !test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		goto out;

	ret = priv->ops->resume_noirq(dev);

out:
	if (ret == 0) {
		priv->stats.pm_resume_noirq++;
		clear_bit(ICNSS_PM_SUSPEND_NOIRQ, &priv->state);
	} else {
		priv->stats.pm_resume_noirq_err++;
	}
	return ret;
}

static int icnss_pm_runtime_suspend(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

	if (priv->device_id != WCN6750_DEVICE_ID) {
		icnss_pr_err("Ignore runtime suspend:\n");
		goto out;
	}

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for runtime suspend: dev %pK, data %pK, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	if (!priv->ops || !priv->ops->runtime_suspend ||
	    IS_ERR(priv->smp2p_info.smem_state))
		goto out;

	icnss_pr_vdbg("Runtime suspend\n");
	ret = priv->ops->runtime_suspend(dev);
	if (!ret) {
		if (test_bit(ICNSS_PD_RESTART, &priv->state) ||
		    !test_bit(ICNSS_MODE_ON, &priv->state))
			return 0;

		ret = icnss_send_smp2p(priv, ICNSS_POWER_SAVE_ENTER);
	}
out:
	return ret;
}

static int icnss_pm_runtime_resume(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

	if (priv->device_id != WCN6750_DEVICE_ID) {
		icnss_pr_err("Ignore runtime resume:\n");
		goto out;
	}

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for runtime resume: dev %pK, data %pK, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	if (!priv->ops || !priv->ops->runtime_resume ||
	    IS_ERR(priv->smp2p_info.smem_state))
		goto out;

	icnss_pr_vdbg("Runtime resume, state: 0x%lx\n", priv->state);

	ret = priv->ops->runtime_resume(dev);

out:
	return ret;
}

static int icnss_pm_runtime_idle(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (priv->device_id != WCN6750_DEVICE_ID) {
		icnss_pr_err("Ignore runtime idle:\n");
		goto out;
	}

	icnss_pr_vdbg("Runtime idle\n");

	pm_request_autosuspend(dev);

out:
	return -EBUSY;
}
#endif

static const struct dev_pm_ops icnss_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(icnss_pm_suspend,
				icnss_pm_resume)
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(icnss_pm_suspend_noirq,
				      icnss_pm_resume_noirq)
	SET_RUNTIME_PM_OPS(icnss_pm_runtime_suspend, icnss_pm_runtime_resume,
			   icnss_pm_runtime_idle)
};

static struct platform_driver icnss_driver = {
	.probe  = icnss_probe,
	.remove = icnss_remove,
	.driver = {
		.name = "icnss2",
		.pm = &icnss_pm_ops,
		.of_match_table = icnss_dt_match,
	},
};

static int __init icnss_initialize(void)
{
	icnss_debug_init();
	return platform_driver_register(&icnss_driver);
}

static void __exit icnss_exit(void)
{
	platform_driver_unregister(&icnss_driver);
	icnss_debug_deinit();
}


module_init(icnss_initialize);
module_exit(icnss_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("iWCN CORE platform driver");
