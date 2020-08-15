// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "icnss: " fmt

#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/iommu.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/ipc_logging.h>
#include <linux/thread_info.h>
#include <linux/uaccess.h>
#include <linux/adc-tm-clients.h>
#include <linux/iio/consumer.h>
#include <dt-bindings/iio/qcom,spmi-vadc.h>
#include <linux/etherdevice.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/soc/qcom/qmi.h>
#include <linux/sysfs.h>
#include <soc/qcom/memory_dump.h>
#include <soc/qcom/icnss.h>
#include <soc/qcom/secure_buffer.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/service-locator.h>
#include <soc/qcom/service-notifier.h>
#include <soc/qcom/socinfo.h>
#include <soc/qcom/ramdump.h>
#include "icnss_private.h"
#include "icnss_qmi.h"

#define MAX_PROP_SIZE			32
#define NUM_LOG_PAGES			10
#define NUM_LOG_LONG_PAGES		4
#define ICNSS_MAGIC			0x5abc5abc

#define ICNSS_SERVICE_LOCATION_CLIENT_NAME			"ICNSS-WLAN"
#define ICNSS_WLAN_SERVICE_NAME					"wlan/fw"
#define ICNSS_THRESHOLD_HIGH		3600000
#define ICNSS_THRESHOLD_LOW		3450000
#define ICNSS_THRESHOLD_GUARD		20000
#define ICNSS_DEFAULT_FEATURE_MASK 0x01

#define ICNSS_QUIRKS_DEFAULT		BIT(FW_REJUVENATE_ENABLE)
#define ICNSS_MAX_PROBE_CNT		2

#define PROBE_TIMEOUT                 15000

static struct icnss_priv *penv;

unsigned long quirks = ICNSS_QUIRKS_DEFAULT;

uint64_t dynamic_feature_mask = ICNSS_DEFAULT_FEATURE_MASK;

void *icnss_ipc_log_context;
void *icnss_ipc_log_long_context;

#define ICNSS_EVENT_PENDING			2989

#define ICNSS_EVENT_SYNC			BIT(0)
#define ICNSS_EVENT_UNINTERRUPTIBLE		BIT(1)
#define ICNSS_EVENT_SYNC_UNINTERRUPTIBLE	(ICNSS_EVENT_UNINTERRUPTIBLE | \
						 ICNSS_EVENT_SYNC)

struct icnss_msa_perm_list_t msa_perm_secure_list[ICNSS_MSA_PERM_MAX] = {
	[ICNSS_MSA_PERM_HLOS_ALL] = {
		.vmids = {VMID_HLOS},
		.perms = {PERM_READ | PERM_WRITE | PERM_EXEC},
		.nelems = 1,
	},

	[ICNSS_MSA_PERM_WLAN_HW_RW] = {
		.vmids = {VMID_MSS_MSA, VMID_WLAN},
		.perms = {PERM_READ | PERM_WRITE,
			PERM_READ | PERM_WRITE},
		.nelems = 2,
	},

};

struct icnss_msa_perm_list_t msa_perm_list[ICNSS_MSA_PERM_MAX] = {
	[ICNSS_MSA_PERM_HLOS_ALL] = {
		.vmids = {VMID_HLOS},
		.perms = {PERM_READ | PERM_WRITE | PERM_EXEC},
		.nelems = 1,
	},

	[ICNSS_MSA_PERM_WLAN_HW_RW] = {
		.vmids = {VMID_MSS_MSA, VMID_WLAN, VMID_WLAN_CE},
		.perms = {PERM_READ | PERM_WRITE,
			PERM_READ | PERM_WRITE,
			PERM_READ | PERM_WRITE},
		.nelems = 3,
	},

};

static struct icnss_vreg_info icnss_vreg_info[] = {
	{NULL, "vdd-cx-mx", 752000, 752000, 0, 0, false},
	{NULL, "vdd-1.8-xo", 1800000, 1800000, 0, 0, false},
	{NULL, "vdd-1.3-rfa", 1304000, 1304000, 0, 0, false},
	{NULL, "vdd-3.3-ch1", 3312000, 3312000, 0, 0, false},
	{NULL, "vdd-3.3-ch0", 3312000, 3312000, 0, 0, false},
};

#define ICNSS_VREG_INFO_SIZE		ARRAY_SIZE(icnss_vreg_info)

static struct icnss_clk_info icnss_clk_info[] = {
	{NULL, "cxo_ref_clk_pin", 0, false},
};

#define ICNSS_CLK_INFO_SIZE		ARRAY_SIZE(icnss_clk_info)

static struct icnss_battery_level icnss_battery_level[] = {
	{70, 3300000},
	{60, 3200000},
	{50, 3100000},
	{25, 3000000},
	{0, 2850000},
};

#define ICNSS_BATTERY_LEVEL_COUNT	ARRAY_SIZE(icnss_battery_level)
#define ICNSS_MAX_BATTERY_LEVEL		100

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

static ssize_t icnss_sysfs_store(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	atomic_set(&penv->is_shutdown, true);
	icnss_pr_dbg("Received shutdown indication");
	return count;
}

static struct kobj_attribute icnss_sysfs_attribute =
__ATTR(shutdown, 0660, NULL, icnss_sysfs_store);

static int icnss_assign_msa_perm(struct icnss_mem_region_info
				 *mem_region, enum icnss_msa_perm new_perm)
{
	int ret = 0;
	phys_addr_t addr;
	u32 size;
	u32 i = 0;
	u32 source_vmids[ICNSS_MAX_VMIDS] = {0};
	u32 source_nelems;
	u32 dest_vmids[ICNSS_MAX_VMIDS] = {0};
	u32 dest_perms[ICNSS_MAX_VMIDS] = {0};
	u32 dest_nelems;
	enum icnss_msa_perm cur_perm = mem_region->perm;
	struct icnss_msa_perm_list_t *new_perm_list, *old_perm_list;

	if (penv && !penv->is_hyp_enabled) {
		icnss_pr_err("hyperviser disabled");
		return 0;
	}

	addr = mem_region->reg_addr;
	size = mem_region->size;

	if (mem_region->secure_flag) {
		new_perm_list = &msa_perm_secure_list[new_perm];
		old_perm_list = &msa_perm_secure_list[cur_perm];
	} else {
		new_perm_list = &msa_perm_list[new_perm];
		old_perm_list = &msa_perm_list[cur_perm];
	}

	source_nelems = old_perm_list->nelems;
	dest_nelems = new_perm_list->nelems;

	for (i = 0; i < source_nelems; ++i)
		source_vmids[i] = old_perm_list->vmids[i];

	for (i = 0; i < dest_nelems; ++i) {
		dest_vmids[i] = new_perm_list->vmids[i];
		dest_perms[i] = new_perm_list->perms[i];
	}

	ret = hyp_assign_phys(addr, size, source_vmids, source_nelems,
			      dest_vmids, dest_perms, dest_nelems);
	if (ret) {
		icnss_pr_err("Hyperviser map failed for PA=%pa size=%u err=%d\n",
			     &addr, size, ret);
		goto out;
	}

	icnss_pr_dbg("Hypervisor map for source_nelems=%d, source[0]=%x, source[1]=%x, source[2]=%x, source[3]=%x, dest_nelems=%d, dest[0]=%x, dest[1]=%x, dest[2]=%x, dest[3]=%x\n",
		     source_nelems, source_vmids[0], source_vmids[1],
		     source_vmids[2], source_vmids[3], dest_nelems,
		     dest_vmids[0], dest_vmids[1], dest_vmids[2],
		     dest_vmids[3]);
out:
	return ret;
}

static int icnss_assign_msa_perm_all(struct icnss_priv *priv,
				     enum icnss_msa_perm new_perm)
{
	int ret;
	int i;
	enum icnss_msa_perm old_perm;

	if (priv->nr_mem_region > WLFW_MAX_NUM_MEMORY_REGIONS) {
		icnss_pr_err("Invalid memory region len %d\n",
			     priv->nr_mem_region);
		return -EINVAL;
	}

	for (i = 0; i < priv->nr_mem_region; i++) {
		old_perm = priv->mem_region[i].perm;
		ret = icnss_assign_msa_perm(&priv->mem_region[i], new_perm);
		if (ret)
			goto err_unmap;
		priv->mem_region[i].perm = new_perm;
	}
	return 0;

err_unmap:
	for (i--; i >= 0; i--)
		icnss_assign_msa_perm(&priv->mem_region[i], old_perm);

	return ret;
}

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

static char *icnss_driver_event_to_str(enum icnss_driver_event_type type)
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
	case ICNSS_DRIVER_EVENT_MAX:
		return "EVENT_MAX";
	}

	return "UNKNOWN";
};

int icnss_driver_event_post(enum icnss_driver_event_type type,
				   u32 flags, void *data)
{
	struct icnss_driver_event *event;
	unsigned long irq_flags;
	int gfp = GFP_KERNEL;
	int ret = 0;

	if (!penv)
		return -ENODEV;

	icnss_pr_dbg("Posting event: %s(%d), %s, flags: 0x%x, state: 0x%lx\n",
		     icnss_driver_event_to_str(type), type, current->comm,
		     flags, penv->state);

	if (type >= ICNSS_DRIVER_EVENT_MAX) {
		icnss_pr_err("Invalid Event type: %d, can't post", type);
		return -EINVAL;
	}

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	event = kzalloc(sizeof(*event), gfp);
	if (event == NULL)
		return -ENOMEM;

	icnss_pm_stay_awake(penv);

	event->type = type;
	event->data = data;
	init_completion(&event->complete);
	event->ret = ICNSS_EVENT_PENDING;
	event->sync = !!(flags & ICNSS_EVENT_SYNC);

	spin_lock_irqsave(&penv->event_lock, irq_flags);
	list_add_tail(&event->list, &penv->event_list);
	spin_unlock_irqrestore(&penv->event_lock, irq_flags);

	penv->stats.events[type].posted++;
	queue_work(penv->event_wq, &penv->event_work);

	if (!(flags & ICNSS_EVENT_SYNC))
		goto out;

	if (flags & ICNSS_EVENT_UNINTERRUPTIBLE)
		wait_for_completion(&event->complete);
	else
		ret = wait_for_completion_interruptible(&event->complete);

	icnss_pr_dbg("Completed event: %s(%d), state: 0x%lx, ret: %d/%d\n",
		     icnss_driver_event_to_str(type), type, penv->state, ret,
		     event->ret);

	spin_lock_irqsave(&penv->event_lock, irq_flags);
	if (ret == -ERESTARTSYS && event->ret == ICNSS_EVENT_PENDING) {
		event->sync = false;
		spin_unlock_irqrestore(&penv->event_lock, irq_flags);
		ret = -EINTR;
		goto out;
	}
	spin_unlock_irqrestore(&penv->event_lock, irq_flags);

	ret = event->ret;
	kfree(event);

out:
	icnss_pm_relax(penv);
	return ret;
}

static int icnss_vreg_on(struct icnss_priv *priv)
{
	int ret = 0;
	struct icnss_vreg_info *vreg_info;
	int i;

	for (i = 0; i < ICNSS_VREG_INFO_SIZE; i++) {
		vreg_info = &priv->vreg_info[i];

		if (!vreg_info->reg)
			continue;

		if (vreg_info->min_v || vreg_info->max_v) {
			icnss_pr_vdbg("Set voltage for regulator %s\n",
							vreg_info->name);
			ret = regulator_set_voltage(vreg_info->reg,
						    vreg_info->min_v,
						    vreg_info->max_v);
			if (ret) {
				icnss_pr_err("Regulator %s, can't set voltage: min_v: %u, max_v: %u, ret: %d\n",
					     vreg_info->name, vreg_info->min_v,
					     vreg_info->max_v, ret);
				break;
			}
		}

		if (vreg_info->load_ua) {
			icnss_pr_vdbg("Set load for regulator %s\n",
							vreg_info->name);
			ret = regulator_set_load(vreg_info->reg,
						 vreg_info->load_ua);
			if (ret < 0) {
				icnss_pr_err("Regulator %s, can't set load: %u, ret: %d\n",
					     vreg_info->name,
					     vreg_info->load_ua, ret);
				break;
			}
		}

		icnss_pr_vdbg("Regulator %s being enabled\n", vreg_info->name);

		ret = regulator_enable(vreg_info->reg);
		if (ret) {
			icnss_pr_err("Regulator %s, can't enable: %d\n",
				     vreg_info->name, ret);
			break;
		}

		if (vreg_info->settle_delay)
			udelay(vreg_info->settle_delay);
	}

	if (!ret)
		return 0;

	for (; i >= 0; i--) {
		vreg_info = &priv->vreg_info[i];

		if (!vreg_info->reg)
			continue;

		regulator_disable(vreg_info->reg);

		if (vreg_info->load_ua)
			regulator_set_load(vreg_info->reg, 0);

		if (vreg_info->min_v || vreg_info->max_v)
			regulator_set_voltage(vreg_info->reg, 0,
					      vreg_info->max_v);
	}

	return ret;
}

static int icnss_vreg_off(struct icnss_priv *priv)
{
	int ret = 0;
	struct icnss_vreg_info *vreg_info;
	int i;

	for (i = ICNSS_VREG_INFO_SIZE - 1; i >= 0; i--) {
		vreg_info = &priv->vreg_info[i];

		if (!vreg_info->reg)
			continue;

		icnss_pr_vdbg("Regulator %s being disabled\n", vreg_info->name);

		ret = regulator_disable(vreg_info->reg);
		if (ret)
			icnss_pr_err("Regulator %s, can't disable: %d\n",
				     vreg_info->name, ret);

		if (vreg_info->load_ua) {
			ret = regulator_set_load(vreg_info->reg, 0);
			if (ret < 0)
				icnss_pr_err("Regulator %s, can't set load: %d\n",
					     vreg_info->name, ret);
		}

		if (vreg_info->min_v || vreg_info->max_v) {
			ret = regulator_set_voltage(vreg_info->reg, 0,
						    vreg_info->max_v);
			if (ret)
				icnss_pr_err("Regulator %s, can't set voltage: %d\n",
					     vreg_info->name, ret);
		}
	}

	return ret;
}

static int icnss_clk_init(struct icnss_priv *priv)
{
	struct icnss_clk_info *clk_info;
	int i;
	int ret = 0;

	for (i = 0; i < ICNSS_CLK_INFO_SIZE; i++) {
		clk_info = &priv->clk_info[i];

		if (!clk_info->handle)
			continue;

		icnss_pr_vdbg("Clock %s being enabled\n", clk_info->name);

		if (clk_info->freq) {
			ret = clk_set_rate(clk_info->handle, clk_info->freq);

			if (ret) {
				icnss_pr_err("Clock %s, can't set frequency: %u, ret: %d\n",
					     clk_info->name, clk_info->freq,
					     ret);
				break;
			}
		}

		ret = clk_prepare_enable(clk_info->handle);
		if (ret) {
			icnss_pr_err("Clock %s, can't enable: %d\n",
				     clk_info->name, ret);
			break;
		}
	}

	if (ret == 0)
		return 0;

	for (; i >= 0; i--) {
		clk_info = &priv->clk_info[i];

		if (!clk_info->handle)
			continue;

		clk_disable_unprepare(clk_info->handle);
	}

	return ret;
}

static int icnss_clk_deinit(struct icnss_priv *priv)
{
	struct icnss_clk_info *clk_info;
	int i;

	for (i = 0; i < ICNSS_CLK_INFO_SIZE; i++) {
		clk_info = &priv->clk_info[i];

		if (!clk_info->handle)
			continue;

		icnss_pr_vdbg("Clock %s being disabled\n", clk_info->name);

		clk_disable_unprepare(clk_info->handle);
	}

	return 0;
}

static int icnss_hw_power_on(struct icnss_priv *priv)
{
	int ret = 0;

	icnss_pr_dbg("HW Power on: state: 0x%lx\n", priv->state);

	spin_lock(&priv->on_off_lock);
	if (test_bit(ICNSS_POWER_ON, &priv->state)) {
		spin_unlock(&priv->on_off_lock);
		return ret;
	}
	set_bit(ICNSS_POWER_ON, &priv->state);
	spin_unlock(&priv->on_off_lock);

	ret = icnss_vreg_on(priv);
	if (ret)
		goto out;

	ret = icnss_clk_init(priv);
	if (ret)
		goto vreg_off;

	return ret;

vreg_off:
	icnss_vreg_off(priv);
out:
	clear_bit(ICNSS_POWER_ON, &priv->state);
	return ret;
}

static int icnss_hw_power_off(struct icnss_priv *priv)
{
	int ret = 0;

	if (test_bit(HW_ALWAYS_ON, &quirks))
		return 0;

	if (test_bit(ICNSS_FW_DOWN, &priv->state))
		return 0;

	icnss_pr_dbg("HW Power off: 0x%lx\n", priv->state);

	spin_lock(&priv->on_off_lock);
	if (!test_bit(ICNSS_POWER_ON, &priv->state)) {
		spin_unlock(&priv->on_off_lock);
		return ret;
	}
	clear_bit(ICNSS_POWER_ON, &priv->state);
	spin_unlock(&priv->on_off_lock);

	icnss_clk_deinit(priv);

	ret = icnss_vreg_off(priv);

	return ret;
}

int icnss_power_on(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Invalid drvdata: dev %pK, data %pK\n",
			     dev, priv);
		return -EINVAL;
	}

	icnss_pr_dbg("Power On: 0x%lx\n", priv->state);

	return icnss_hw_power_on(priv);
}
EXPORT_SYMBOL(icnss_power_on);

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
	if (!penv)
		return false;

	return test_bit(ICNSS_FW_DOWN, &penv->state) ||
		test_bit(ICNSS_PD_RESTART, &penv->state) ||
		test_bit(ICNSS_REJUVENATE, &penv->state);
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

int icnss_power_off(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Invalid drvdata: dev %pK, data %pK\n",
			     dev, priv);
		return -EINVAL;
	}

	icnss_pr_dbg("Power Off: 0x%lx\n", priv->state);

	return icnss_hw_power_off(priv);
}
EXPORT_SYMBOL(icnss_power_off);


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
			fw_down_data.crashed = true;
			icnss_call_driver_uevent(priv, ICNSS_UEVENT_FW_DOWN,
						 &fw_down_data);
		}
	}

	icnss_driver_event_post(ICNSS_DRIVER_EVENT_FW_EARLY_CRASH_IND,
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


static int icnss_get_phone_power(struct icnss_priv *priv, uint64_t *result_uv)
{
	int ret = 0;
	int result;

	if (!priv->channel) {
		icnss_pr_err("Channel doesn't exists\n");
		ret = -EINVAL;
		goto out;
	}

	ret = iio_read_channel_processed(penv->channel, &result);
	if (ret < 0) {
		icnss_pr_err("Error reading channel, ret = %d\n", ret);
		goto out;
	}

	*result_uv = (uint64_t) result;
out:
	return ret;
}

static void icnss_vph_notify(enum adc_tm_state state, void *ctx)
{
	struct icnss_priv *priv = ctx;
	uint64_t vph_pwr = 0;
	uint64_t vph_pwr_prev;
	int ret = 0;
	bool update = true;

	if (!priv) {
		icnss_pr_err("Priv pointer is NULL\n");
		return;
	}

	vph_pwr_prev = priv->vph_pwr;

	ret = icnss_get_phone_power(priv, &vph_pwr);
	if (ret < 0)
		return;

	if (vph_pwr < ICNSS_THRESHOLD_LOW) {
		if (vph_pwr_prev < ICNSS_THRESHOLD_LOW)
			update = false;
		priv->vph_monitor_params.state_request =
			ADC_TM_HIGH_THR_ENABLE;
		priv->vph_monitor_params.high_thr = ICNSS_THRESHOLD_LOW +
			ICNSS_THRESHOLD_GUARD;
		priv->vph_monitor_params.low_thr = 0;
	} else if (vph_pwr > ICNSS_THRESHOLD_HIGH) {
		if (vph_pwr_prev > ICNSS_THRESHOLD_HIGH)
			update = false;
		priv->vph_monitor_params.state_request =
			ADC_TM_LOW_THR_ENABLE;
		priv->vph_monitor_params.low_thr = ICNSS_THRESHOLD_HIGH -
			ICNSS_THRESHOLD_GUARD;
		priv->vph_monitor_params.high_thr = 0;
	} else {
		if (vph_pwr_prev > ICNSS_THRESHOLD_LOW &&
		    vph_pwr_prev < ICNSS_THRESHOLD_HIGH)
			update = false;
		priv->vph_monitor_params.state_request =
			ADC_TM_HIGH_LOW_THR_ENABLE;
		priv->vph_monitor_params.low_thr = ICNSS_THRESHOLD_LOW;
		priv->vph_monitor_params.high_thr = ICNSS_THRESHOLD_HIGH;
	}

	priv->vph_pwr = vph_pwr;

	if (update) {
		icnss_send_vbatt_update(priv, vph_pwr);
		icnss_pr_dbg("set low threshold to %d, high threshold to %d Phone power=%llu\n",
			     priv->vph_monitor_params.low_thr,
			     priv->vph_monitor_params.high_thr, vph_pwr);
	}

	ret = adc_tm5_channel_measure(priv->adc_tm_dev,
				      &priv->vph_monitor_params);
	if (ret)
		icnss_pr_err("TM channel setup failed %d\n", ret);
}

static int icnss_setup_vph_monitor(struct icnss_priv *priv)
{
	int ret = 0;

	if (!priv->adc_tm_dev) {
		icnss_pr_err("ADC TM handler is NULL\n");
		ret = -EINVAL;
		goto out;
	}

	priv->vph_monitor_params.low_thr = ICNSS_THRESHOLD_LOW;
	priv->vph_monitor_params.high_thr = ICNSS_THRESHOLD_HIGH;
	priv->vph_monitor_params.state_request = ADC_TM_HIGH_LOW_THR_ENABLE;
	priv->vph_monitor_params.channel = ADC_VBAT_SNS;
	priv->vph_monitor_params.btm_ctx = priv;
	priv->vph_monitor_params.threshold_notification = &icnss_vph_notify;
	icnss_pr_dbg("Set low threshold to %d, high threshold to %d\n",
		     priv->vph_monitor_params.low_thr,
		     priv->vph_monitor_params.high_thr);

	ret = adc_tm5_channel_measure(priv->adc_tm_dev,
				      &priv->vph_monitor_params);
	if (ret)
		icnss_pr_err("TM channel setup failed %d\n", ret);
out:
	return ret;
}

static int icnss_init_vph_monitor(struct icnss_priv *priv)
{
	int ret = 0;

	ret = icnss_get_phone_power(priv, &priv->vph_pwr);
	if (ret < 0)
		goto out;

	icnss_pr_dbg("Phone power=%llu\n", priv->vph_pwr);

	icnss_send_vbatt_update(priv, priv->vph_pwr);

	ret = icnss_setup_vph_monitor(priv);
	if (ret)
		goto out;
out:
	return ret;
}

static int icnss_get_battery_level(struct icnss_priv *priv)
{
	int err = 0, battery_percentage = 0;
	union power_supply_propval psp = {0,};

	if (!priv->batt_psy)
		priv->batt_psy = power_supply_get_by_name("battery");

	if (priv->batt_psy) {
		err = power_supply_get_property(priv->batt_psy,
						POWER_SUPPLY_PROP_CAPACITY,
						&psp);
		if (err) {
			icnss_pr_err("battery percentage read error:%d\n", err);
			goto out;
		}
		battery_percentage = psp.intval;
	}

	icnss_pr_info("Battery Percentage: %d\n", battery_percentage);
out:
	return battery_percentage;
}

static void icnss_update_soc_level(struct work_struct *work)
{
	int battery_percentage = 0, current_updated_voltage = 0, err = 0;
	int level_count;

	battery_percentage = icnss_get_battery_level(penv);
	if (!battery_percentage ||
	    battery_percentage > ICNSS_MAX_BATTERY_LEVEL) {
		icnss_pr_err("Battery percentage read failure\n");
		return;
	}

	for (level_count = 0; level_count < ICNSS_BATTERY_LEVEL_COUNT;
	     level_count++) {
		if (battery_percentage >=
		    icnss_battery_level[level_count].lower_battery_threshold) {
			current_updated_voltage =
				icnss_battery_level[level_count].ldo_voltage;
			break;
		}
	}

	if (level_count != ICNSS_BATTERY_LEVEL_COUNT &&
	    penv->last_updated_voltage != current_updated_voltage) {
		err = icnss_send_vbatt_update(penv, current_updated_voltage);
		if (err < 0) {
			icnss_pr_err("Unable to update ldo voltage");
			return;
		}
		penv->last_updated_voltage = current_updated_voltage;
	}
}

static int icnss_battery_supply_callback(struct notifier_block *nb,
					 unsigned long event, void *data)
{
	struct power_supply *psy = data;

	if (strcmp(psy->desc->name, "battery"))
		return NOTIFY_OK;

	if (test_bit(ICNSS_WLFW_CONNECTED, &penv->state) &&
	    !test_bit(ICNSS_FW_DOWN, &penv->state))
		queue_work(penv->soc_update_wq, &penv->soc_update_work);

	return NOTIFY_OK;
}

static int icnss_driver_event_server_arrive(void *data)
{
	int ret = 0;
	bool ignore_assert = false;

	if (!penv)
		return -ENODEV;

	set_bit(ICNSS_WLFW_EXISTS, &penv->state);
	clear_bit(ICNSS_FW_DOWN, &penv->state);
	clear_bit(ICNSS_FW_READY, &penv->state);

	icnss_ignore_fw_timeout(false);

	if (test_bit(ICNSS_WLFW_CONNECTED, &penv->state)) {
		icnss_pr_err("QMI Server already in Connected State\n");
		ICNSS_ASSERT(0);
	}

	ret = icnss_connect_to_fw_server(penv, data);
	if (ret)
		goto fail;

	set_bit(ICNSS_WLFW_CONNECTED, &penv->state);

	ret = icnss_hw_power_on(penv);
	if (ret)
		goto clear_server;

	ret = wlfw_ind_register_send_sync_msg(penv);
	if (ret < 0) {
		if (ret == -EALREADY) {
			ret = 0;
			goto qmi_registered;
		}
		ignore_assert = true;
		goto err_power_on;
	}

	if (!penv->msa_va) {
		icnss_pr_err("Invalid MSA address\n");
		ret = -EINVAL;
		goto err_power_on;
	}

	ret = wlfw_msa_mem_info_send_sync_msg(penv);
	if (ret < 0) {
		ignore_assert = true;
		goto err_power_on;
	}

	if (!test_bit(ICNSS_MSA0_ASSIGNED, &penv->state)) {
		ret = icnss_assign_msa_perm_all(penv,
						ICNSS_MSA_PERM_WLAN_HW_RW);
		if (ret < 0)
			goto err_power_on;
		set_bit(ICNSS_MSA0_ASSIGNED, &penv->state);
	}

	ret = wlfw_msa_ready_send_sync_msg(penv);
	if (ret < 0) {
		ignore_assert = true;
		goto err_setup_msa;
	}

	ret = wlfw_cap_send_sync_msg(penv);
	if (ret < 0) {
		ignore_assert = true;
		goto err_setup_msa;
	}

	wlfw_dynamic_feature_mask_send_sync_msg(penv,
						dynamic_feature_mask);

	if (!penv->fw_error_fatal_irq)
		register_fw_error_notifications(&penv->pdev->dev);

	if (!penv->fw_early_crash_irq)
		register_early_crash_notifications(&penv->pdev->dev);

	if (penv->vbatt_supported)
		icnss_init_vph_monitor(penv);

	if (penv->psf_supported)
		queue_work(penv->soc_update_wq, &penv->soc_update_work);

	return ret;

err_setup_msa:
	icnss_assign_msa_perm_all(penv, ICNSS_MSA_PERM_HLOS_ALL);
	clear_bit(ICNSS_MSA0_ASSIGNED, &penv->state);
err_power_on:
	icnss_hw_power_off(penv);
clear_server:
	icnss_clear_server(penv);
fail:
	ICNSS_ASSERT(ignore_assert);
qmi_registered:
	return ret;
}

static int icnss_driver_event_server_exit(void *data)
{
	if (!penv)
		return -ENODEV;

	icnss_pr_info("WLAN FW Service Disconnected: 0x%lx\n", penv->state);

	icnss_clear_server(penv);

	if (penv->adc_tm_dev && penv->vbatt_supported)
		adc_tm5_disable_chan_meas(penv->adc_tm_dev,
					  &penv->vph_monitor_params);

	if (penv->psf_supported)
		penv->last_updated_voltage = 0;

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
	if (!test_bit(ICNSS_DRIVER_PROBED, &penv->state))
		goto out;

	if (!priv->ops || !priv->ops->shutdown)
		goto out;

	if (test_bit(ICNSS_SHUTDOWN_DONE, &penv->state))
		goto out;

	icnss_pr_dbg("Calling driver shutdown state: 0x%lx\n", priv->state);

	priv->ops->shutdown(&priv->pdev->dev);
	set_bit(ICNSS_SHUTDOWN_DONE, &penv->state);

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
	clear_bit(ICNSS_SHUTDOWN_DONE, &penv->state);
	return 0;

call_probe:
	return icnss_call_driver_probe(priv);

out_power_off:
	icnss_hw_power_off(priv);

out:
	return ret;
}


static int icnss_driver_event_fw_ready_ind(void *data)
{
	int ret = 0;

	if (!penv)
		return -ENODEV;

	set_bit(ICNSS_FW_READY, &penv->state);
	clear_bit(ICNSS_MODE_ON, &penv->state);

	icnss_pr_info("WLAN FW is ready: 0x%lx\n", penv->state);

	icnss_hw_power_off(penv);

	if (!penv->pdev) {
		icnss_pr_err("Device is not ready\n");
		ret = -ENODEV;
		goto out;
	}

	if (test_bit(ICNSS_PD_RESTART, &penv->state))
		ret = icnss_pd_restart_complete(penv);
	else
		ret = icnss_call_driver_probe(penv);

out:
	return ret;
}

static int icnss_driver_event_register_driver(void *data)
{
	int ret = 0;
	int probe_cnt = 0;

	if (penv->ops)
		return -EEXIST;

	penv->ops = data;

	if (test_bit(SKIP_QMI, &quirks))
		set_bit(ICNSS_FW_READY, &penv->state);

	if (test_bit(ICNSS_FW_DOWN, &penv->state)) {
		icnss_pr_err("FW is in bad state, state: 0x%lx\n",
			     penv->state);
		return -ENODEV;
	}

	if (!test_bit(ICNSS_FW_READY, &penv->state)) {
		icnss_pr_dbg("FW is not ready yet, state: 0x%lx\n",
			     penv->state);
		goto out;
	}

	ret = icnss_hw_power_on(penv);
	if (ret)
		goto out;

	icnss_block_shutdown(true);
	while (probe_cnt < ICNSS_MAX_PROBE_CNT) {
		ret = penv->ops->probe(&penv->pdev->dev);
		probe_cnt++;
		if (ret != -EPROBE_DEFER)
			break;
	}
	if (ret) {
		icnss_pr_err("Driver probe failed: %d, state: 0x%lx, probe_cnt: %d\n",
			     ret, penv->state, probe_cnt);
		icnss_block_shutdown(false);
		goto power_off;
	}

	icnss_block_shutdown(false);
	set_bit(ICNSS_DRIVER_PROBED, &penv->state);

	return 0;

power_off:
	icnss_hw_power_off(penv);
out:
	return ret;
}

static int icnss_driver_event_unregister_driver(void *data)
{
	if (!test_bit(ICNSS_DRIVER_PROBED, &penv->state)) {
		penv->ops = NULL;
		goto out;
	}

	set_bit(ICNSS_DRIVER_UNLOADING, &penv->state);

	icnss_block_shutdown(true);

	if (penv->ops)
		penv->ops->remove(&penv->pdev->dev);

	icnss_block_shutdown(false);

	clear_bit(ICNSS_DRIVER_UNLOADING, &penv->state);
	clear_bit(ICNSS_DRIVER_PROBED, &penv->state);

	penv->ops = NULL;

	icnss_hw_power_off(penv);

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

static int icnss_driver_event_idle_shutdown(void *data)
{
	int ret = 0;

	if (!penv->ops || !penv->ops->idle_shutdown)
		return 0;

	if (penv->is_ssr || test_bit(ICNSS_PDR, &penv->state) ||
	    test_bit(ICNSS_REJUVENATE, &penv->state)) {
		icnss_pr_err("SSR/PDR is already in-progress during idle shutdown callback\n");
		ret = -EBUSY;
	} else {
		icnss_pr_dbg("Calling driver idle shutdown, state: 0x%lx\n",
								penv->state);
		icnss_block_shutdown(true);
		ret = penv->ops->idle_shutdown(&penv->pdev->dev);
		icnss_block_shutdown(false);
	}

	return ret;
}

static int icnss_driver_event_idle_restart(void *data)
{
	int ret = 0;

	if (!penv->ops || !penv->ops->idle_restart)
		return 0;

	if (penv->is_ssr || test_bit(ICNSS_PDR, &penv->state) ||
	    test_bit(ICNSS_REJUVENATE, &penv->state)) {
		icnss_pr_err("SSR/PDR is already in-progress during idle restart callback\n");
		ret = -EBUSY;
	} else {
		icnss_pr_dbg("Calling driver idle restart, state: 0x%lx\n",
								penv->state);
		icnss_block_shutdown(true);
		ret = penv->ops->idle_restart(&penv->pdev->dev);
		icnss_block_shutdown(false);
	}

	return ret;
}

static void icnss_driver_event_work(struct work_struct *work)
{
	struct icnss_driver_event *event;
	unsigned long flags;
	int ret;

	icnss_pm_stay_awake(penv);

	spin_lock_irqsave(&penv->event_lock, flags);

	while (!list_empty(&penv->event_list)) {
		event = list_first_entry(&penv->event_list,
					 struct icnss_driver_event, list);
		list_del(&event->list);
		spin_unlock_irqrestore(&penv->event_lock, flags);

		icnss_pr_dbg("Processing event: %s%s(%d), state: 0x%lx\n",
			     icnss_driver_event_to_str(event->type),
			     event->sync ? "-sync" : "", event->type,
			     penv->state);

		switch (event->type) {
		case ICNSS_DRIVER_EVENT_SERVER_ARRIVE:
			ret = icnss_driver_event_server_arrive(event->data);
			break;
		case ICNSS_DRIVER_EVENT_SERVER_EXIT:
			ret = icnss_driver_event_server_exit(event->data);
			break;
		case ICNSS_DRIVER_EVENT_FW_READY_IND:
			ret = icnss_driver_event_fw_ready_ind(event->data);
			break;
		case ICNSS_DRIVER_EVENT_REGISTER_DRIVER:
			ret = icnss_driver_event_register_driver(event->data);
			break;
		case ICNSS_DRIVER_EVENT_UNREGISTER_DRIVER:
			ret = icnss_driver_event_unregister_driver(event->data);
			break;
		case ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN:
			ret = icnss_driver_event_pd_service_down(penv,
								 event->data);
			break;
		case ICNSS_DRIVER_EVENT_FW_EARLY_CRASH_IND:
			ret = icnss_driver_event_early_crash_ind(penv,
								 event->data);
			break;
		case ICNSS_DRIVER_EVENT_IDLE_SHUTDOWN:
			ret = icnss_driver_event_idle_shutdown(event->data);
			break;
		case ICNSS_DRIVER_EVENT_IDLE_RESTART:
			ret = icnss_driver_event_idle_restart(event->data);
			break;
		default:
			icnss_pr_err("Invalid Event type: %d", event->type);
			kfree(event);
			continue;
		}

		penv->stats.events[event->type].processed++;

		icnss_pr_dbg("Event Processed: %s%s(%d), ret: %d, state: 0x%lx\n",
			     icnss_driver_event_to_str(event->type),
			     event->sync ? "-sync" : "", event->type, ret,
			     penv->state);

		spin_lock_irqsave(&penv->event_lock, flags);
		if (event->sync) {
			event->ret = ret;
			complete(&event->complete);
			continue;
		}
		spin_unlock_irqrestore(&penv->event_lock, flags);

		kfree(event);

		spin_lock_irqsave(&penv->event_lock, flags);
	}
	spin_unlock_irqrestore(&penv->event_lock, flags);

	icnss_pm_relax(penv);
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
	struct notif_data *notif = data;
	int ret = 0;

	if (!notif->crashed) {
		if (atomic_read(&priv->is_shutdown)) {
			atomic_set(&priv->is_shutdown, false);
			if (!test_bit(ICNSS_PD_RESTART, &priv->state) &&
				!test_bit(ICNSS_SHUTDOWN_DONE, &priv->state) &&
				!test_bit(ICNSS_BLOCK_SHUTDOWN, &priv->state)) {
				clear_bit(ICNSS_FW_READY, &priv->state);
				icnss_driver_event_post(
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

static int icnss_modem_notifier_nb(struct notifier_block *nb,
				  unsigned long code,
				  void *data)
{
	struct icnss_event_pd_service_down_data *event_data;
	struct notif_data *notif = data;
	struct icnss_priv *priv = container_of(nb, struct icnss_priv,
					       modem_ssr_nb);
	struct icnss_uevent_fw_down_data fw_down_data = {0};
	int ret;

	icnss_pr_vdbg("Modem-Notify: event %lu\n", code);

	if (code == SUBSYS_AFTER_SHUTDOWN &&
	    notif->crashed == CRASH_STATUS_ERR_FATAL) {
		ret = icnss_assign_msa_perm_all(priv,
						ICNSS_MSA_PERM_HLOS_ALL);
		if (!ret) {
			icnss_pr_info("Collecting msa0 segment dump\n");
			icnss_msa0_ramdump(priv);
			icnss_assign_msa_perm_all(priv,
						  ICNSS_MSA_PERM_WLAN_HW_RW);
		} else {
			icnss_pr_err("Not able to Collect msa0 segment dump, Apps permissions not assigned %d\n",
				     ret);
		}
		goto out;
	}

	if (code != SUBSYS_BEFORE_SHUTDOWN)
		goto out;

	priv->is_ssr = true;

	icnss_update_state_send_modem_shutdown(priv, data);

	if (test_bit(ICNSS_PDR_REGISTERED, &priv->state)) {
		set_bit(ICNSS_FW_DOWN, &priv->state);
		icnss_ignore_fw_timeout(true);

		if (test_bit(ICNSS_FW_READY, &priv->state)) {
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

	if (notif->crashed)
		priv->stats.recovery.root_pd_crash++;
	else
		priv->stats.recovery.root_pd_shutdown++;

	icnss_ignore_fw_timeout(true);

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);

	if (event_data == NULL) {
		icnss_pr_vdbg("Exit %s, event_data is NULL\n", __func__);
		return notifier_from_errno(-ENOMEM);
	}

	event_data->crashed = notif->crashed;

	if (test_bit(ICNSS_FW_READY, &priv->state)) {
		fw_down_data.crashed = !!notif->crashed;
		icnss_call_driver_uevent(priv,
					 ICNSS_UEVENT_FW_DOWN,
					 &fw_down_data);
	}
	icnss_driver_event_post(ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN,
				ICNSS_EVENT_SYNC, event_data);
out:
	icnss_pr_vdbg("Exit %s,state: 0x%lx\n", __func__, priv->state);
	return NOTIFY_OK;
}

static int icnss_modem_ssr_register_notifier(struct icnss_priv *priv)
{
	int ret = 0;

	priv->modem_ssr_nb.notifier_call = icnss_modem_notifier_nb;

	priv->modem_notify_handler =
		subsys_notif_register_notifier("modem", &priv->modem_ssr_nb);

	if (IS_ERR(priv->modem_notify_handler)) {
		ret = PTR_ERR(priv->modem_notify_handler);
		icnss_pr_err("Modem register notifier failed: %d\n", ret);
	}

	set_bit(ICNSS_SSR_REGISTERED, &priv->state);

	return ret;
}

static int icnss_modem_ssr_unregister_notifier(struct icnss_priv *priv)
{
	if (!test_and_clear_bit(ICNSS_SSR_REGISTERED, &priv->state))
		return 0;

	subsys_notif_unregister_notifier(priv->modem_notify_handler,
					 &priv->modem_ssr_nb);
	priv->modem_notify_handler = NULL;

	return 0;
}

static int icnss_pdr_unregister_notifier(struct icnss_priv *priv)
{
	int i;

	if (!test_and_clear_bit(ICNSS_PDR_REGISTERED, &priv->state))
		return 0;

	for (i = 0; i < priv->total_domains; i++)
		service_notif_unregister_notifier(
				priv->service_notifier[i].handle,
				&priv->service_notifier_nb);

	kfree(priv->service_notifier);

	priv->service_notifier = NULL;

	return 0;
}

static int icnss_service_notifier_notify(struct notifier_block *nb,
					 unsigned long notification, void *data)
{
	struct icnss_priv *priv = container_of(nb, struct icnss_priv,
					       service_notifier_nb);
	enum pd_subsys_state *state = data;
	struct icnss_event_pd_service_down_data *event_data;
	struct icnss_uevent_fw_down_data fw_down_data = {0};
	enum icnss_pdr_cause_index cause = ICNSS_ROOT_PD_CRASH;

	icnss_pr_dbg("PD service notification: 0x%lx state: 0x%lx\n",
		     notification, priv->state);

	if (notification != SERVREG_NOTIF_SERVICE_STATE_DOWN_V01)
		goto done;

	if (!priv->is_ssr)
		set_bit(ICNSS_PDR, &priv->state);

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);

	if (event_data == NULL)
		return notifier_from_errno(-ENOMEM);

	event_data->crashed = true;

	if (state == NULL) {
		priv->stats.recovery.root_pd_crash++;
		goto event_post;
	}

	switch (*state) {
	case ROOT_PD_WDOG_BITE:
		priv->stats.recovery.root_pd_crash++;
		break;
	case ROOT_PD_SHUTDOWN:
		cause = ICNSS_ROOT_PD_SHUTDOWN;
		priv->stats.recovery.root_pd_shutdown++;
		event_data->crashed = false;
		break;
	case USER_PD_STATE_CHANGE:
		if (test_bit(ICNSS_HOST_TRIGGERED_PDR, &priv->state)) {
			cause = ICNSS_HOST_ERROR;
			priv->stats.recovery.pdr_host_error++;
		} else {
			cause = ICNSS_FW_CRASH;
			priv->stats.recovery.pdr_fw_crash++;
		}
		break;
	default:
		priv->stats.recovery.root_pd_crash++;
		break;
	}
	icnss_pr_info("PD service down, pd_state: %d, state: 0x%lx: cause: %s\n",
		      *state, priv->state, icnss_pdr_cause[cause]);
event_post:
	if (!test_bit(ICNSS_FW_DOWN, &priv->state)) {
		set_bit(ICNSS_FW_DOWN, &priv->state);
		icnss_ignore_fw_timeout(true);

		if (test_bit(ICNSS_FW_READY, &priv->state)) {
			fw_down_data.crashed = event_data->crashed;
			icnss_call_driver_uevent(priv,
						 ICNSS_UEVENT_FW_DOWN,
						 &fw_down_data);
		}
	}

	clear_bit(ICNSS_HOST_TRIGGERED_PDR, &priv->state);
	icnss_driver_event_post(ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN,
				ICNSS_EVENT_SYNC, event_data);
done:
	if (notification == SERVREG_NOTIF_SERVICE_STATE_UP_V01)
		clear_bit(ICNSS_FW_DOWN, &priv->state);
	return NOTIFY_OK;
}

static int icnss_get_service_location_notify(struct notifier_block *nb,
					     unsigned long opcode, void *data)
{
	struct icnss_priv *priv = container_of(nb, struct icnss_priv,
					       get_service_nb);
	struct pd_qmi_client_data *pd = data;
	int curr_state;
	int ret;
	int i;
	int j;
	bool duplicate;
	struct service_notifier_context *notifier;

	icnss_pr_dbg("Get service notify opcode: %lu, state: 0x%lx\n", opcode,
		     priv->state);

	if (opcode != LOCATOR_UP)
		return NOTIFY_DONE;

	if (pd->total_domains == 0) {
		icnss_pr_err("Did not find any domains\n");
		ret = -ENOENT;
		goto out;
	}

	notifier = kcalloc(pd->total_domains,
				sizeof(struct service_notifier_context),
				GFP_KERNEL);
	if (!notifier) {
		ret = -ENOMEM;
		goto out;
	}

	priv->service_notifier_nb.notifier_call = icnss_service_notifier_notify;

	for (i = 0; i < pd->total_domains; i++) {
		duplicate = false;
		for (j = i + 1; j < pd->total_domains; j++) {
			if (!strcmp(pd->domain_list[i].name,
			    pd->domain_list[j].name))
				duplicate = true;
		}

		if (duplicate)
			continue;

		icnss_pr_dbg("%d: domain_name: %s, instance_id: %d\n", i,
			     pd->domain_list[i].name,
			     pd->domain_list[i].instance_id);

		notifier[i].handle =
			service_notif_register_notifier(pd->domain_list[i].name,
				pd->domain_list[i].instance_id,
				&priv->service_notifier_nb, &curr_state);
		notifier[i].instance_id = pd->domain_list[i].instance_id;
		strlcpy(notifier[i].name, pd->domain_list[i].name,
			QMI_SERVREG_LOC_NAME_LENGTH_V01 + 1);

		if (IS_ERR(notifier[i].handle)) {
			icnss_pr_err("%d: Unable to register notifier for %s(0x%x)\n",
				     i, pd->domain_list->name,
				     pd->domain_list->instance_id);
			ret = PTR_ERR(notifier[i].handle);
			goto free_handle;
		}
	}

	priv->service_notifier = notifier;
	priv->total_domains = pd->total_domains;

	set_bit(ICNSS_PDR_REGISTERED, &priv->state);

	icnss_pr_dbg("PD notification registration happened, state: 0x%lx\n",
		     priv->state);

	return NOTIFY_OK;

free_handle:
	for (i = 0; i < pd->total_domains; i++) {
		if (notifier[i].handle)
			service_notif_unregister_notifier(notifier[i].handle,
					&priv->service_notifier_nb);
	}
	kfree(notifier);

out:
	icnss_pr_err("PD restart not enabled: %d, state: 0x%lx\n", ret,
		     priv->state);

	return NOTIFY_OK;
}


static int icnss_pd_restart_enable(struct icnss_priv *priv)
{
	int ret;

	if (test_bit(SSR_ONLY, &quirks)) {
		icnss_pr_dbg("PDR disabled through module parameter\n");
		return 0;
	}

	icnss_pr_dbg("Get service location, state: 0x%lx\n", priv->state);

	priv->get_service_nb.notifier_call = icnss_get_service_location_notify;
	ret = get_service_location(ICNSS_SERVICE_LOCATION_CLIENT_NAME,
				   ICNSS_WLAN_SERVICE_NAME,
				   &priv->get_service_nb);
	if (ret) {
		icnss_pr_err("Get service location failed: %d\n", ret);
		goto out;
	}

	return 0;
out:
	icnss_pr_err("Failed to enable PD restart: %d\n", ret);
	return ret;

}


static int icnss_enable_recovery(struct icnss_priv *priv)
{
	int ret;

	if (test_bit(RECOVERY_DISABLE, &quirks)) {
		icnss_pr_dbg("Recovery disabled through module parameter\n");
		return 0;
	}

	if (test_bit(PDR_ONLY, &quirks)) {
		icnss_pr_dbg("SSR disabled through module parameter\n");
		goto enable_pdr;
	}

	priv->msa0_dump_dev = create_ramdump_device("wcss_msa0",
						    &priv->pdev->dev);
	if (!priv->msa0_dump_dev)
		return -ENOMEM;

	icnss_modem_ssr_register_notifier(priv);
	if (test_bit(SSR_ONLY, &quirks)) {
		icnss_pr_dbg("PDR disabled through module parameter\n");
		return 0;
	}

enable_pdr:
	ret = icnss_pd_restart_enable(priv);

	if (ret)
		return ret;

	return 0;
}

int __icnss_register_driver(struct icnss_driver_ops *ops,
			    struct module *owner, const char *mod_name)
{
	int ret = 0;

	if (!penv || !penv->pdev) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("Registering driver, state: 0x%lx\n", penv->state);

	if (penv->ops) {
		icnss_pr_err("Driver already registered\n");
		ret = -EEXIST;
		goto out;
	}

	if (!ops->probe || !ops->remove) {
		ret = -EINVAL;
		goto out;
	}

	ret = icnss_driver_event_post(ICNSS_DRIVER_EVENT_REGISTER_DRIVER,
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

	if (!penv || !penv->pdev) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("Unregistering driver, state: 0x%lx\n", penv->state);

	if (!penv->ops) {
		icnss_pr_err("Driver not registered\n");
		ret = -ENOENT;
		goto out;
	}

	ret = icnss_driver_event_post(ICNSS_DRIVER_EVENT_UNREGISTER_DRIVER,
				      ICNSS_EVENT_SYNC_UNINTERRUPTIBLE, NULL);
out:
	return ret;
}
EXPORT_SYMBOL(icnss_unregister_driver);

int icnss_ce_request_irq(struct device *dev, unsigned int ce_id,
	irqreturn_t (*handler)(int, void *),
		unsigned long flags, const char *name, void *ctx)
{
	int ret = 0;
	unsigned int irq;
	struct ce_irq_list *irq_entry;

	if (!penv || !penv->pdev || !dev) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_vdbg("CE request IRQ: %d, state: 0x%lx\n", ce_id, penv->state);

	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS) {
		icnss_pr_err("Invalid CE ID, ce_id: %d\n", ce_id);
		ret = -EINVAL;
		goto out;
	}
	irq = penv->ce_irqs[ce_id];
	irq_entry = &penv->ce_irq_list[ce_id];

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

	if (!penv || !dev) {
		icnss_pr_err("Platform driver not initialized\n");
		return -EINVAL;
	}

	info->v_addr = penv->mem_base_va;
	info->p_addr = penv->mem_base_pa;
	info->chip_id = penv->chip_info.chip_id;
	info->chip_family = penv->chip_info.chip_family;
	info->board_id = penv->board_id;
	info->soc_id = penv->soc_id;
	info->fw_version = penv->fw_version_info.fw_version;
	fw_build_timestamp = penv->fw_version_info.fw_build_timestamp;
	fw_build_timestamp[WLFW_MAX_TIMESTAMP_LEN] = '\0';
	strlcpy(info->fw_build_timestamp,
		penv->fw_version_info.fw_build_timestamp,
		WLFW_MAX_TIMESTAMP_LEN + 1);

	return 0;
}
EXPORT_SYMBOL(icnss_get_soc_info);

int icnss_set_fw_log_mode(struct device *dev, uint8_t fw_log_mode)
{
	int ret;

	if (!dev)
		return -ENODEV;

	if (test_bit(ICNSS_FW_DOWN, &penv->state) ||
	    !test_bit(ICNSS_FW_READY, &penv->state)) {
		icnss_pr_err("FW down, ignoring fw_log_mode state: 0x%lx\n",
			     penv->state);
		return -EINVAL;
	}

	icnss_pr_dbg("FW log mode: %u\n", fw_log_mode);

	ret = wlfw_ini_send_sync_msg(penv, fw_log_mode);
	if (ret)
		icnss_pr_err("Fail to send ini, ret = %d, fw_log_mode: %u\n",
			     ret, fw_log_mode);
	return ret;
}
EXPORT_SYMBOL(icnss_set_fw_log_mode);

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
	if (test_bit(ICNSS_FW_DOWN, &penv->state) ||
	    !test_bit(ICNSS_FW_READY, &penv->state)) {
		icnss_pr_err("FW down, ignoring wlan_enable state: 0x%lx\n",
			     penv->state);
		return -EINVAL;
	}

	if (test_bit(ICNSS_MODE_ON, &penv->state)) {
		icnss_pr_err("Already Mode on, ignoring wlan_enable state: 0x%lx\n",
			     penv->state);
		return -EINVAL;
	}

	return icnss_send_wlan_enable_to_fw(penv, config, mode, host_version);
}
EXPORT_SYMBOL(icnss_wlan_enable);

int icnss_wlan_disable(struct device *dev, enum icnss_driver_mode mode)
{
	if (test_bit(ICNSS_FW_DOWN, &penv->state)) {
		icnss_pr_dbg("FW down, ignoring wlan_disable state: 0x%lx\n",
			     penv->state);
		return 0;
	}

	return icnss_send_wlan_disable_to_fw(penv);
}
EXPORT_SYMBOL(icnss_wlan_disable);

bool icnss_is_qmi_disable(struct device *dev)
{
	return test_bit(SKIP_QMI, &quirks) ? true : false;
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
	iova = roundup(penv->smmu_iova_ipa_current, PAGE_SIZE);

	if (of_get_property(dev->of_node, "qcom,iommu-geometry", &prop_len) &&
	    iova >= priv->smmu_iova_ipa_start + priv->smmu_iova_ipa_len) {
		icnss_pr_err("No IOVA space to map, iova %lx, smmu_iova_ipa_start %pad, smmu_iova_ipa_len %zu\n",
			     iova,
			     &priv->smmu_iova_ipa_start,
			     priv->smmu_iova_ipa_len);
		return -ENOMEM;
	}

	icnss_pr_dbg("IOMMU Map: iova %lx, len %zu\n", iova, len);

	ret = iommu_map(priv->iommu_domain, iova,
			rounddown(paddr, PAGE_SIZE), len,
			IOMMU_READ | IOMMU_WRITE);
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

	if (!test_bit(ICNSS_PDR_REGISTERED, &priv->state)) {
		icnss_pr_err("PD restart not enabled to trigger recovery: state: 0x%lx\n",
			     priv->state);
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (!priv->service_notifier || !priv->service_notifier[0].handle) {
		icnss_pr_err("Invalid handle during recovery, state: 0x%lx\n",
			     priv->state);
		ret = -EINVAL;
		goto out;
	}

	icnss_pr_warn("Initiate PD restart at WLAN FW, state: 0x%lx\n",
		      priv->state);

	/*
	 * Initiate PDR, required only for the first instance
	 */
	ret = service_notif_pd_restart(priv->service_notifier[0].name,
		priv->service_notifier[0].instance_id);

	if (!ret)
		set_bit(ICNSS_HOST_TRIGGERED_PDR, &priv->state);

	icnss_pr_warn("PD restart request completed, ret: %d state: 0x%lx\n",
		      ret, priv->state);

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
	    test_bit(ICNSS_REJUVENATE, &penv->state)) {
		icnss_pr_err("SSR/PDR is already in-progress during idle shutdown\n");
		return -EBUSY;
	}

	return icnss_driver_event_post(ICNSS_DRIVER_EVENT_IDLE_SHUTDOWN,
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
	    test_bit(ICNSS_REJUVENATE, &penv->state)) {
		icnss_pr_err("SSR/PDR is already in-progress during idle restart\n");
		return -EBUSY;
	}

	return icnss_driver_event_post(ICNSS_DRIVER_EVENT_IDLE_RESTART,
					ICNSS_EVENT_SYNC_UNINTERRUPTIBLE, NULL);
}
EXPORT_SYMBOL(icnss_idle_restart);

static int icnss_get_vreg_info(struct device *dev,
			       struct icnss_vreg_info *vreg_info)
{
	int ret = 0;
	char prop_name[MAX_PROP_SIZE];
	struct regulator *reg;
	const __be32 *prop;
	int len = 0;
	int i;

	reg = devm_regulator_get_optional(dev, vreg_info->name);
	if (PTR_ERR(reg) == -EPROBE_DEFER) {
		icnss_pr_err("EPROBE_DEFER for regulator: %s\n",
			     vreg_info->name);
		ret = PTR_ERR(reg);
		goto out;
	}

	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);

		if (vreg_info->required) {
			icnss_pr_err("Regulator %s doesn't exist: %d\n",
				     vreg_info->name, ret);
			goto out;
		} else {
			icnss_pr_dbg("Optional regulator %s doesn't exist: %d\n",
				     vreg_info->name, ret);
			goto done;
		}
	}

	vreg_info->reg = reg;

	snprintf(prop_name, MAX_PROP_SIZE,
		 "qcom,%s-config", vreg_info->name);

	prop = of_get_property(dev->of_node, prop_name, &len);

	icnss_pr_dbg("Got regulator config, prop: %s, len: %d\n",
		     prop_name, len);

	if (!prop || len < (2 * sizeof(__be32))) {
		icnss_pr_dbg("Property %s %s\n", prop_name,
			     prop ? "invalid format" : "doesn't exist");
		goto done;
	}

	for (i = 0; (i * sizeof(__be32)) < len; i++) {
		switch (i) {
		case 0:
			vreg_info->min_v = be32_to_cpup(&prop[0]);
			break;
		case 1:
			vreg_info->max_v = be32_to_cpup(&prop[1]);
			break;
		case 2:
			vreg_info->load_ua = be32_to_cpup(&prop[2]);
			break;
		case 3:
			vreg_info->settle_delay = be32_to_cpup(&prop[3]);
			break;
		default:
			icnss_pr_dbg("Property %s, ignoring value at %d\n",
				     prop_name, i);
			break;
		}
	}

done:
	icnss_pr_dbg("Regulator: %s, min_v: %u, max_v: %u, load: %u, delay: %lu\n",
		     vreg_info->name, vreg_info->min_v, vreg_info->max_v,
		     vreg_info->load_ua, vreg_info->settle_delay);

	return 0;

out:
	return ret;
}

static int icnss_get_clk_info(struct device *dev,
			      struct icnss_clk_info *clk_info)
{
	struct clk *handle;
	int ret = 0;

	handle = devm_clk_get(dev, clk_info->name);
	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		if (clk_info->required) {
			icnss_pr_err("Clock %s isn't available: %d\n",
				     clk_info->name, ret);
			goto out;
		} else {
			icnss_pr_dbg("Ignoring clock %s: %d\n", clk_info->name,
				     ret);
			ret = 0;
			goto out;
		}
	}

	icnss_pr_dbg("Clock: %s, freq: %u\n", clk_info->name, clk_info->freq);

	clk_info->handle = handle;
out:
	return ret;
}

static int icnss_fw_debug_show(struct seq_file *s, void *data)
{
	struct icnss_priv *priv = s->private;

	seq_puts(s, "\nUsage: echo <CMD> <VAL> > <DEBUGFS>/icnss/fw_debug\n");

	seq_puts(s, "\nCMD: test_mode\n");
	seq_puts(s, "  VAL: 0 (Test mode disable)\n");
	seq_puts(s, "  VAL: 1 (WLAN FW test)\n");
	seq_puts(s, "  VAL: 2 (CCPM test)\n");
	seq_puts(s, "  VAL: 3 (Trigger Recovery)\n");
	seq_puts(s, "  VAL: 4 (Allow Recursive Recovery)\n");
	seq_puts(s, "  VAL: 5 (Disallow Recursive Recovery)\n");
	seq_puts(s, "  VAL: 6 (Trigger power supply callback)\n");

	seq_puts(s, "\nCMD: dynamic_feature_mask\n");
	seq_puts(s, "  VAL: (64 bit feature mask)\n");

	if (!test_bit(ICNSS_FW_READY, &priv->state)) {
		seq_puts(s, "Firmware is not ready yet, can't run test_mode!\n");
		goto out;
	}

	if (test_bit(ICNSS_DRIVER_PROBED, &priv->state)) {
		seq_puts(s, "Machine mode is running, can't run test_mode!\n");
		goto out;
	}

	if (test_bit(ICNSS_FW_TEST_MODE, &priv->state)) {
		seq_puts(s, "test_mode is running, can't run test_mode!\n");
		goto out;
	}

out:
	seq_puts(s, "\n");
	return 0;
}

static int icnss_test_mode_fw_test_off(struct icnss_priv *priv)
{
	int ret;

	if (!test_bit(ICNSS_FW_READY, &priv->state)) {
		icnss_pr_err("Firmware is not ready yet!, wait for FW READY: state: 0x%lx\n",
			     priv->state);
		ret = -ENODEV;
		goto out;
	}

	if (test_bit(ICNSS_DRIVER_PROBED, &priv->state)) {
		icnss_pr_err("Machine mode is running, can't run test mode: state: 0x%lx\n",
			     priv->state);
		ret = -EINVAL;
		goto out;
	}

	if (!test_bit(ICNSS_FW_TEST_MODE, &priv->state)) {
		icnss_pr_err("Test mode not started, state: 0x%lx\n",
			     priv->state);
		ret = -EINVAL;
		goto out;
	}

	icnss_wlan_disable(&priv->pdev->dev, ICNSS_OFF);

	ret = icnss_hw_power_off(priv);

	clear_bit(ICNSS_FW_TEST_MODE, &priv->state);

out:
	return ret;
}
static int icnss_test_mode_fw_test(struct icnss_priv *priv,
				   enum icnss_driver_mode mode)
{
	int ret;

	if (!test_bit(ICNSS_FW_READY, &priv->state)) {
		icnss_pr_err("Firmware is not ready yet!, wait for FW READY, state: 0x%lx\n",
			     priv->state);
		ret = -ENODEV;
		goto out;
	}

	if (test_bit(ICNSS_DRIVER_PROBED, &priv->state)) {
		icnss_pr_err("Machine mode is running, can't run test mode, state: 0x%lx\n",
			     priv->state);
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(ICNSS_FW_TEST_MODE, &priv->state)) {
		icnss_pr_err("Test mode already started, state: 0x%lx\n",
			     priv->state);
		ret = -EBUSY;
		goto out;
	}

	ret = icnss_hw_power_on(priv);
	if (ret)
		goto out;

	set_bit(ICNSS_FW_TEST_MODE, &priv->state);

	ret = icnss_wlan_enable(&priv->pdev->dev, NULL, mode, NULL);
	if (ret)
		goto power_off;

	return 0;

power_off:
	icnss_hw_power_off(priv);
	clear_bit(ICNSS_FW_TEST_MODE, &priv->state);

out:
	return ret;
}

static void icnss_allow_recursive_recovery(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	priv->allow_recursive_recovery = true;

	icnss_pr_info("Recursive recovery allowed for WLAN\n");
}

static void icnss_disallow_recursive_recovery(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	priv->allow_recursive_recovery = false;

	icnss_pr_info("Recursive recovery disallowed for WLAN\n");
}

static ssize_t icnss_fw_debug_write(struct file *fp,
				    const char __user *user_buf,
				    size_t count, loff_t *off)
{
	struct icnss_priv *priv =
		((struct seq_file *)fp->private_data)->private;
	char buf[64];
	char *sptr, *token;
	unsigned int len = 0;
	char *cmd;
	uint64_t val;
	const char *delim = " ";
	int ret = 0;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EINVAL;

	buf[len] = '\0';
	sptr = buf;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;
	if (!sptr)
		return -EINVAL;
	cmd = token;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;
	if (kstrtou64(token, 0, &val))
		return -EINVAL;

	if (strcmp(cmd, "test_mode") == 0) {
		switch (val) {
		case 0:
			ret = icnss_test_mode_fw_test_off(priv);
			break;
		case 1:
			ret = icnss_test_mode_fw_test(priv, ICNSS_WALTEST);
			break;
		case 2:
			ret = icnss_test_mode_fw_test(priv, ICNSS_CCPM);
			break;
		case 3:
			ret = icnss_trigger_recovery(&priv->pdev->dev);
			break;
		case 4:
			icnss_allow_recursive_recovery(&priv->pdev->dev);
			break;
		case 5:
			icnss_disallow_recursive_recovery(&priv->pdev->dev);
			break;
		case 6:
			power_supply_changed(priv->batt_psy);
			break;
		default:
			return -EINVAL;
		}
	} else if (strcmp(cmd, "dynamic_feature_mask") == 0) {
		ret = wlfw_dynamic_feature_mask_send_sync_msg(priv, val);
	} else {
		return -EINVAL;
	}

	if (ret)
		return ret;

	return count;
}

static int icnss_fw_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, icnss_fw_debug_show, inode->i_private);
}

static const struct file_operations icnss_fw_debug_fops = {
	.read		= seq_read,
	.write		= icnss_fw_debug_write,
	.release	= single_release,
	.open		= icnss_fw_debug_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static ssize_t icnss_stats_write(struct file *fp, const char __user *buf,
				    size_t count, loff_t *off)
{
	struct icnss_priv *priv =
		((struct seq_file *)fp->private_data)->private;
	int ret;
	u32 val;

	ret = kstrtou32_from_user(buf, count, 0, &val);
	if (ret)
		return ret;

	if (ret == 0)
		memset(&priv->stats, 0, sizeof(priv->stats));

	return count;
}

static int icnss_stats_show_state(struct seq_file *s, struct icnss_priv *priv)
{
	enum icnss_driver_state i;
	int skip = 0;
	unsigned long state;

	seq_printf(s, "\nState: 0x%lx(", priv->state);
	for (i = 0, state = priv->state; state != 0; state >>= 1, i++) {

		if (!(state & 0x1))
			continue;

		if (skip++)
			seq_puts(s, " | ");

		switch (i) {
		case ICNSS_WLFW_CONNECTED:
			seq_puts(s, "FW CONN");
			continue;
		case ICNSS_POWER_ON:
			seq_puts(s, "POWER ON");
			continue;
		case ICNSS_FW_READY:
			seq_puts(s, "FW READY");
			continue;
		case ICNSS_DRIVER_PROBED:
			seq_puts(s, "DRIVER PROBED");
			continue;
		case ICNSS_FW_TEST_MODE:
			seq_puts(s, "FW TEST MODE");
			continue;
		case ICNSS_PM_SUSPEND:
			seq_puts(s, "PM SUSPEND");
			continue;
		case ICNSS_PM_SUSPEND_NOIRQ:
			seq_puts(s, "PM SUSPEND NOIRQ");
			continue;
		case ICNSS_SSR_REGISTERED:
			seq_puts(s, "SSR REGISTERED");
			continue;
		case ICNSS_PDR_REGISTERED:
			seq_puts(s, "PDR REGISTERED");
			continue;
		case ICNSS_PD_RESTART:
			seq_puts(s, "PD RESTART");
			continue;
		case ICNSS_MSA0_ASSIGNED:
			seq_puts(s, "MSA0 ASSIGNED");
			continue;
		case ICNSS_WLFW_EXISTS:
			seq_puts(s, "WLAN FW EXISTS");
			continue;
		case ICNSS_SHUTDOWN_DONE:
			seq_puts(s, "SHUTDOWN DONE");
			continue;
		case ICNSS_HOST_TRIGGERED_PDR:
			seq_puts(s, "HOST TRIGGERED PDR");
			continue;
		case ICNSS_FW_DOWN:
			seq_puts(s, "FW DOWN");
			continue;
		case ICNSS_DRIVER_UNLOADING:
			seq_puts(s, "DRIVER UNLOADING");
			continue;
		case ICNSS_REJUVENATE:
			seq_puts(s, "FW REJUVENATE");
			continue;
		case ICNSS_MODE_ON:
			seq_puts(s, "MODE ON DONE");
			continue;
		case ICNSS_BLOCK_SHUTDOWN:
			seq_puts(s, "BLOCK SHUTDOWN");
			continue;
		case ICNSS_PDR:
			seq_puts(s, "PDR TRIGGERED");
			continue;
		case ICNSS_DEL_SERVER:
			seq_puts(s, "DEL SERVER");
		}

		seq_printf(s, "UNKNOWN-%d", i);
	}
	seq_puts(s, ")\n");

	return 0;
}

static int icnss_stats_show_capability(struct seq_file *s,
				       struct icnss_priv *priv)
{
	if (test_bit(ICNSS_FW_READY, &priv->state)) {
		seq_puts(s, "\n<---------------- FW Capability ----------------->\n");
		seq_printf(s, "Chip ID: 0x%x\n", priv->chip_info.chip_id);
		seq_printf(s, "Chip family: 0x%x\n",
			  priv->chip_info.chip_family);
		seq_printf(s, "Board ID: 0x%x\n", priv->board_id);
		seq_printf(s, "SOC Info: 0x%x\n", priv->soc_id);
		seq_printf(s, "Firmware Version: 0x%x\n",
			   priv->fw_version_info.fw_version);
		seq_printf(s, "Firmware Build Timestamp: %s\n",
			   priv->fw_version_info.fw_build_timestamp);
		seq_printf(s, "Firmware Build ID: %s\n",
			   priv->fw_build_id);
	}

	return 0;
}

static int icnss_stats_show_rejuvenate_info(struct seq_file *s,
					    struct icnss_priv *priv)
{
	if (priv->stats.rejuvenate_ind)  {
		seq_puts(s, "\n<---------------- Rejuvenate Info ----------------->\n");
		seq_printf(s, "Number of Rejuvenations: %u\n",
			   priv->stats.rejuvenate_ind);
		seq_printf(s, "Cause for Rejuvenation: 0x%x\n",
			   priv->cause_for_rejuvenation);
		seq_printf(s, "Requesting Sub-System: 0x%x\n",
			   priv->requesting_sub_system);
		seq_printf(s, "Line Number: %u\n",
			   priv->line_number);
		seq_printf(s, "Function Name: %s\n",
			   priv->function_name);
	}

	return 0;
}

static int icnss_stats_show_events(struct seq_file *s, struct icnss_priv *priv)
{
	int i;

	seq_puts(s, "\n<----------------- Events stats ------------------->\n");
	seq_printf(s, "%24s %16s %16s\n", "Events", "Posted", "Processed");
	for (i = 0; i < ICNSS_DRIVER_EVENT_MAX; i++)
		seq_printf(s, "%24s %16u %16u\n",
			   icnss_driver_event_to_str(i),
			   priv->stats.events[i].posted,
			   priv->stats.events[i].processed);

	return 0;
}

static int icnss_stats_show_irqs(struct seq_file *s, struct icnss_priv *priv)
{
	int i;

	seq_puts(s, "\n<------------------ IRQ stats ------------------->\n");
	seq_printf(s, "%4s %4s %8s %8s %8s %8s\n", "CE_ID", "IRQ", "Request",
		   "Free", "Enable", "Disable");
	for (i = 0; i < ICNSS_MAX_IRQ_REGISTRATIONS; i++)
		seq_printf(s, "%4d: %4u %8u %8u %8u %8u\n", i,
			   priv->ce_irqs[i], priv->stats.ce_irqs[i].request,
			   priv->stats.ce_irqs[i].free,
			   priv->stats.ce_irqs[i].enable,
			   priv->stats.ce_irqs[i].disable);

	return 0;
}

static int icnss_stats_show(struct seq_file *s, void *data)
{
#define ICNSS_STATS_DUMP(_s, _priv, _x) \
	seq_printf(_s, "%24s: %u\n", #_x, _priv->stats._x)

	struct icnss_priv *priv = s->private;

	ICNSS_STATS_DUMP(s, priv, ind_register_req);
	ICNSS_STATS_DUMP(s, priv, ind_register_resp);
	ICNSS_STATS_DUMP(s, priv, ind_register_err);
	ICNSS_STATS_DUMP(s, priv, msa_info_req);
	ICNSS_STATS_DUMP(s, priv, msa_info_resp);
	ICNSS_STATS_DUMP(s, priv, msa_info_err);
	ICNSS_STATS_DUMP(s, priv, msa_ready_req);
	ICNSS_STATS_DUMP(s, priv, msa_ready_resp);
	ICNSS_STATS_DUMP(s, priv, msa_ready_err);
	ICNSS_STATS_DUMP(s, priv, msa_ready_ind);
	ICNSS_STATS_DUMP(s, priv, cap_req);
	ICNSS_STATS_DUMP(s, priv, cap_resp);
	ICNSS_STATS_DUMP(s, priv, cap_err);
	ICNSS_STATS_DUMP(s, priv, pin_connect_result);
	ICNSS_STATS_DUMP(s, priv, cfg_req);
	ICNSS_STATS_DUMP(s, priv, cfg_resp);
	ICNSS_STATS_DUMP(s, priv, cfg_req_err);
	ICNSS_STATS_DUMP(s, priv, mode_req);
	ICNSS_STATS_DUMP(s, priv, mode_resp);
	ICNSS_STATS_DUMP(s, priv, mode_req_err);
	ICNSS_STATS_DUMP(s, priv, ini_req);
	ICNSS_STATS_DUMP(s, priv, ini_resp);
	ICNSS_STATS_DUMP(s, priv, ini_req_err);
	ICNSS_STATS_DUMP(s, priv, rejuvenate_ind);
	ICNSS_STATS_DUMP(s, priv, rejuvenate_ack_req);
	ICNSS_STATS_DUMP(s, priv, rejuvenate_ack_resp);
	ICNSS_STATS_DUMP(s, priv, rejuvenate_ack_err);
	ICNSS_STATS_DUMP(s, priv, recovery.pdr_fw_crash);
	ICNSS_STATS_DUMP(s, priv, recovery.pdr_host_error);
	ICNSS_STATS_DUMP(s, priv, recovery.root_pd_crash);
	ICNSS_STATS_DUMP(s, priv, recovery.root_pd_shutdown);

	seq_puts(s, "\n<------------------ PM stats ------------------->\n");
	ICNSS_STATS_DUMP(s, priv, pm_suspend);
	ICNSS_STATS_DUMP(s, priv, pm_suspend_err);
	ICNSS_STATS_DUMP(s, priv, pm_resume);
	ICNSS_STATS_DUMP(s, priv, pm_resume_err);
	ICNSS_STATS_DUMP(s, priv, pm_suspend_noirq);
	ICNSS_STATS_DUMP(s, priv, pm_suspend_noirq_err);
	ICNSS_STATS_DUMP(s, priv, pm_resume_noirq);
	ICNSS_STATS_DUMP(s, priv, pm_resume_noirq_err);
	ICNSS_STATS_DUMP(s, priv, pm_stay_awake);
	ICNSS_STATS_DUMP(s, priv, pm_relax);

	icnss_stats_show_irqs(s, priv);

	icnss_stats_show_capability(s, priv);

	icnss_stats_show_rejuvenate_info(s, priv);

	icnss_stats_show_events(s, priv);

	icnss_stats_show_state(s, priv);

	return 0;
#undef ICNSS_STATS_DUMP
}

static int icnss_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, icnss_stats_show, inode->i_private);
}

static const struct file_operations icnss_stats_fops = {
	.read		= seq_read,
	.write		= icnss_stats_write,
	.release	= single_release,
	.open		= icnss_stats_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static int icnss_regwrite_show(struct seq_file *s, void *data)
{
	struct icnss_priv *priv = s->private;

	seq_puts(s, "\nUsage: echo <mem_type> <offset> <reg_val> > <debugfs>/icnss/reg_write\n");

	if (!test_bit(ICNSS_FW_READY, &priv->state))
		seq_puts(s, "Firmware is not ready yet!, wait for FW READY\n");

	return 0;
}

static ssize_t icnss_regwrite_write(struct file *fp,
				    const char __user *user_buf,
				    size_t count, loff_t *off)
{
	struct icnss_priv *priv =
		((struct seq_file *)fp->private_data)->private;
	char buf[64];
	char *sptr, *token;
	unsigned int len = 0;
	uint32_t reg_offset, mem_type, reg_val;
	const char *delim = " ";
	int ret = 0;

	if (!test_bit(ICNSS_FW_READY, &priv->state) ||
	    !test_bit(ICNSS_POWER_ON, &priv->state))
		return -EINVAL;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	sptr = buf;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &mem_type))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_offset))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_val))
		return -EINVAL;

	ret = wlfw_athdiag_write_send_sync_msg(priv, reg_offset, mem_type,
					       sizeof(uint32_t),
					       (uint8_t *)&reg_val);
	if (ret)
		return ret;

	return count;
}

static int icnss_regwrite_open(struct inode *inode, struct file *file)
{
	return single_open(file, icnss_regwrite_show, inode->i_private);
}

static const struct file_operations icnss_regwrite_fops = {
	.read		= seq_read,
	.write          = icnss_regwrite_write,
	.open           = icnss_regwrite_open,
	.owner          = THIS_MODULE,
	.llseek		= seq_lseek,
};

static int icnss_regread_show(struct seq_file *s, void *data)
{
	struct icnss_priv *priv = s->private;

	mutex_lock(&priv->dev_lock);
	if (!priv->diag_reg_read_buf) {
		seq_puts(s, "Usage: echo <mem_type> <offset> <data_len> > <debugfs>/icnss/reg_read\n");

		if (!test_bit(ICNSS_FW_READY, &priv->state))
			seq_puts(s, "Firmware is not ready yet!, wait for FW READY\n");

		mutex_unlock(&priv->dev_lock);
		return 0;
	}

	seq_printf(s, "REGREAD: Addr 0x%x Type 0x%x Length 0x%x\n",
		   priv->diag_reg_read_addr, priv->diag_reg_read_mem_type,
		   priv->diag_reg_read_len);

	seq_hex_dump(s, "", DUMP_PREFIX_OFFSET, 32, 4, priv->diag_reg_read_buf,
		     priv->diag_reg_read_len, false);

	priv->diag_reg_read_len = 0;
	kfree(priv->diag_reg_read_buf);
	priv->diag_reg_read_buf = NULL;
	mutex_unlock(&priv->dev_lock);

	return 0;
}

static ssize_t icnss_reg_parse(const char __user *user_buf, size_t count,
			       struct icnss_reg_info *reg_info_ptr)
{
	char buf[64] = {0};
	char *sptr = NULL, *token = NULL;
	const char *delim = " ";
	unsigned int len = 0;

	if (user_buf == NULL)
		return -EFAULT;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	sptr = buf;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_info_ptr->mem_type))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_info_ptr->reg_offset))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_info_ptr->data_len))
		return -EINVAL;

	if (reg_info_ptr->data_len == 0 ||
	    reg_info_ptr->data_len > WLFW_MAX_DATA_SIZE)
		return -EINVAL;

	return 0;
}

static ssize_t icnss_regread_write(struct file *fp, const char __user *user_buf,
				   size_t count, loff_t *off)
{
	struct icnss_priv *priv =
		((struct seq_file *)fp->private_data)->private;
	uint8_t *reg_buf = NULL;
	int ret = 0;
	struct icnss_reg_info reg_info;

	if (!test_bit(ICNSS_FW_READY, &priv->state) ||
	    !test_bit(ICNSS_POWER_ON, &priv->state))
		return -EINVAL;

	ret = icnss_reg_parse(user_buf, count, &reg_info);
	if (ret)
		return ret;

	mutex_lock(&priv->dev_lock);
	kfree(priv->diag_reg_read_buf);
	priv->diag_reg_read_buf = NULL;

	reg_buf = kzalloc(reg_info.data_len, GFP_KERNEL);
	if (!reg_buf) {
		mutex_unlock(&priv->dev_lock);
		return -ENOMEM;
	}

	ret = wlfw_athdiag_read_send_sync_msg(priv, reg_info.reg_offset,
					      reg_info.mem_type,
					      reg_info.data_len,
					      reg_buf);
	if (ret) {
		kfree(reg_buf);
		mutex_unlock(&priv->dev_lock);
		return ret;
	}

	priv->diag_reg_read_addr = reg_info.reg_offset;
	priv->diag_reg_read_mem_type = reg_info.mem_type;
	priv->diag_reg_read_len = reg_info.data_len;
	priv->diag_reg_read_buf = reg_buf;
	mutex_unlock(&priv->dev_lock);

	return count;
}

static int icnss_regread_open(struct inode *inode, struct file *file)
{
	return single_open(file, icnss_regread_show, inode->i_private);
}

static const struct file_operations icnss_regread_fops = {
	.read           = seq_read,
	.write          = icnss_regread_write,
	.open           = icnss_regread_open,
	.owner          = THIS_MODULE,
	.llseek         = seq_lseek,
};

#ifdef CONFIG_ICNSS_DEBUG
static int icnss_debugfs_create(struct icnss_priv *priv)
{
	int ret = 0;
	struct dentry *root_dentry;

	root_dentry = debugfs_create_dir("icnss", NULL);

	if (IS_ERR(root_dentry)) {
		ret = PTR_ERR(root_dentry);
		icnss_pr_err("Unable to create debugfs %d\n", ret);
		goto out;
	}

	priv->root_dentry = root_dentry;

	debugfs_create_file("fw_debug", 0600, root_dentry, priv,
			    &icnss_fw_debug_fops);

	debugfs_create_file("stats", 0600, root_dentry, priv,
			    &icnss_stats_fops);
	debugfs_create_file("reg_read", 0600, root_dentry, priv,
			    &icnss_regread_fops);
	debugfs_create_file("reg_write", 0600, root_dentry, priv,
			    &icnss_regwrite_fops);

out:
	return ret;
}
#else
static int icnss_debugfs_create(struct icnss_priv *priv)
{
	int ret = 0;
	struct dentry *root_dentry;

	root_dentry = debugfs_create_dir("icnss", NULL);

	if (IS_ERR(root_dentry)) {
		ret = PTR_ERR(root_dentry);
		icnss_pr_err("Unable to create debugfs %d\n", ret);
		return ret;
	}

	priv->root_dentry = root_dentry;

	debugfs_create_file("stats", 0600, root_dentry, priv,
			    &icnss_stats_fops);
	return 0;
}
#endif

static void icnss_debugfs_destroy(struct icnss_priv *priv)
{
	debugfs_remove_recursive(priv->root_dentry);
}

static void icnss_sysfs_create(struct icnss_priv *priv)
{
	struct kobject *icnss_kobject;
	int error = 0;

	atomic_set(&priv->is_shutdown, false);

	icnss_kobject = kobject_create_and_add("shutdown_wlan", kernel_kobj);
	if (!icnss_kobject) {
		icnss_pr_err("Unable to create kernel object");
		return;
	}

	priv->icnss_kobject = icnss_kobject;

	error = sysfs_create_file(icnss_kobject, &icnss_sysfs_attribute.attr);
	if (error)
		icnss_pr_err("Unable to create icnss sysfs file");
}

static void icnss_sysfs_destroy(struct icnss_priv *priv)
{
	struct kobject *icnss_kobject;

	icnss_kobject = priv->icnss_kobject;
	if (icnss_kobject)
		kobject_put(icnss_kobject);
}

static void icnss_unregister_power_supply_notifier(struct icnss_priv *priv)
{
	if (priv->batt_psy)
		power_supply_put(penv->batt_psy);

	if (priv->psf_supported) {
		flush_workqueue(priv->soc_update_wq);
		destroy_workqueue(priv->soc_update_wq);
		power_supply_unreg_notifier(&priv->psf_nb);
	}
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

	channel = iio_channel_get(&priv->pdev->dev, "icnss");
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

static int icnss_get_psf_info(struct icnss_priv *priv)
{
	int ret = 0;

	priv->soc_update_wq = alloc_workqueue("icnss_soc_update",
					      WQ_UNBOUND, 1);
	if (!priv->soc_update_wq) {
		icnss_pr_err("Workqueue creation failed for soc update\n");
		ret = -EFAULT;
		goto out;
	}

	priv->psf_nb.notifier_call = icnss_battery_supply_callback;
	ret = power_supply_reg_notifier(&priv->psf_nb);
	if (ret < 0) {
		icnss_pr_err("Power supply framework registration err: %d\n",
			     ret);
		goto err_psf_registration;
	}

	INIT_WORK(&priv->soc_update_work, icnss_update_soc_level);

	return 0;

err_psf_registration:
	destroy_workqueue(priv->soc_update_wq);
out:
	return ret;
}

static int icnss_resource_parse(struct icnss_priv *priv)
{
	int ret = 0, i = 0;
	struct platform_device *pdev = priv->pdev;
	struct device *dev = &pdev->dev;
	struct resource *res;

	if (of_property_read_bool(pdev->dev.of_node, "qcom,icnss-adc_tm")) {
		ret = icnss_get_vbatt_info(priv);
		if (ret == -EPROBE_DEFER)
			goto out;
		priv->vbatt_supported = true;
	}

	if (of_property_read_bool(pdev->dev.of_node, "qcom,psf-supported")) {
		ret = icnss_get_psf_info(priv);
		if (ret < 0)
			goto out;
		priv->psf_supported = true;
	}

	for (i = 0; i < ICNSS_VREG_INFO_SIZE; i++) {
		ret = icnss_get_vreg_info(dev, &priv->vreg_info[i]);

		if (ret)
			goto out;
	}

	priv->clk_info = icnss_clk_info;
	for (i = 0; i < ICNSS_CLK_INFO_SIZE; i++) {
		ret = icnss_get_clk_info(dev, &priv->clk_info[i]);
		if (ret)
			goto out;
	}

	if (of_property_read_bool(pdev->dev.of_node, "qcom,hyp_enabled"))
		priv->is_hyp_enabled = true;

	icnss_pr_dbg("Hypervisor enabled = %d\n", priv->is_hyp_enabled);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "membase");
	if (!res) {
		icnss_pr_err("Memory base not found in DT\n");
		ret = -EINVAL;
		goto out;
	}

	priv->mem_base_pa = res->start;
	priv->mem_base_va = devm_ioremap(dev, priv->mem_base_pa,
					 resource_size(res));
	if (!priv->mem_base_va) {
		icnss_pr_err("Memory base ioremap failed: phy addr: %pa\n",
			     &priv->mem_base_pa);
		ret = -EINVAL;
		goto out;
	}
	icnss_pr_dbg("MEM_BASE pa: %pa, va: 0x%pK\n", &priv->mem_base_pa,
		     priv->mem_base_va);

	for (i = 0; i < ICNSS_MAX_IRQ_REGISTRATIONS; i++) {
		res = platform_get_resource(priv->pdev, IORESOURCE_IRQ, i);
		if (!res) {
			icnss_pr_err("Fail to get IRQ-%d\n", i);
			ret = -ENODEV;
			goto out;
		} else {
			priv->ce_irqs[i] = res->start;
		}
	}

	return 0;

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
			goto out;
		}

		priv->msa_pa = of_translate_address(np, addrp);
		if (priv->msa_pa == OF_BAD_ADDR) {
			icnss_pr_err("Failed to translate MSA PA from device-tree\n");
			ret = -EINVAL;
			goto out;
		}

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

	return 0;

out:
	return ret;
}

static int icnss_smmu_dt_parse(struct icnss_priv *priv)
{
	int ret = 0;
	struct platform_device *pdev = priv->pdev;
	struct device *dev = &pdev->dev;
	struct resource *res;
	u32 addr_win[2];

	ret = of_property_read_u32_array(dev->of_node,
					 "qcom,iommu-dma-addr-pool",
					 addr_win,
					 ARRAY_SIZE(addr_win));

	if (ret) {
		icnss_pr_err("SMMU IOVA base not found\n");
	} else {
		priv->iommu_domain =
			iommu_get_domain_for_dev(&pdev->dev);

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

static int icnss_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct icnss_priv *priv;

	if (penv) {
		icnss_pr_err("Driver is already initialized\n");
		return -EEXIST;
	}

	icnss_pr_dbg("Platform driver probe\n");

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->magic = ICNSS_MAGIC;
	dev_set_drvdata(dev, priv);

	priv->pdev = pdev;

	priv->vreg_info = icnss_vreg_info;

	icnss_allow_recursive_recovery(dev);

	ret = icnss_resource_parse(priv);
	if (ret)
		goto out;

	ret = icnss_msa_dt_parse(priv);
	if (ret)
		goto out;

	ret = icnss_smmu_dt_parse(priv);
	if (ret)
		goto out;

	spin_lock_init(&priv->event_lock);
	spin_lock_init(&priv->on_off_lock);
	mutex_init(&priv->dev_lock);

	priv->event_wq = alloc_workqueue("icnss_driver_event", WQ_UNBOUND, 1);
	if (!priv->event_wq) {
		icnss_pr_err("Workqueue creation failed\n");
		ret = -EFAULT;
		goto smmu_cleanup;
	}

	INIT_WORK(&priv->event_work, icnss_driver_event_work);
	INIT_LIST_HEAD(&priv->event_list);

	ret = icnss_register_fw_service(priv);
	if (ret < 0) {
		icnss_pr_err("fw service registration failed: %d\n", ret);
		goto out_destroy_wq;
	}

	icnss_enable_recovery(priv);

	icnss_debugfs_create(priv);

	icnss_sysfs_create(priv);

	ret = device_init_wakeup(&priv->pdev->dev, true);
	if (ret)
		icnss_pr_err("Failed to init platform device wakeup source, err = %d\n",
			     ret);

	penv = priv;

	init_completion(&priv->unblock_shutdown);

	icnss_pr_info("Platform driver probed successfully\n");

	return 0;

out_destroy_wq:
	destroy_workqueue(priv->event_wq);
smmu_cleanup:
	priv->iommu_domain = NULL;
out:
	dev_set_drvdata(dev, NULL);

	return ret;
}

static int icnss_remove(struct platform_device *pdev)
{
	icnss_pr_info("Removing driver: state: 0x%lx\n", penv->state);

	device_init_wakeup(&penv->pdev->dev, false);

	icnss_unregister_power_supply_notifier(penv);

	icnss_debugfs_destroy(penv);

	icnss_sysfs_destroy(penv);

	complete_all(&penv->unblock_shutdown);

	icnss_modem_ssr_unregister_notifier(penv);

	destroy_ramdump_device(penv->msa0_dump_dev);

	icnss_pdr_unregister_notifier(penv);

	icnss_unregister_fw_service(penv);
	if (penv->event_wq)
		destroy_workqueue(penv->event_wq);

	penv->iommu_domain = NULL;

	icnss_hw_power_off(penv);

	icnss_assign_msa_perm_all(penv, ICNSS_MSA_PERM_HLOS_ALL);
	clear_bit(ICNSS_MSA0_ASSIGNED, &penv->state);

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
	    !test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		goto out;

	ret = priv->ops->pm_suspend(dev);

out:
	if (ret == 0) {
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
#endif

static const struct dev_pm_ops icnss_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(icnss_pm_suspend,
				icnss_pm_resume)
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(icnss_pm_suspend_noirq,
				      icnss_pm_resume_noirq)
};

static const struct of_device_id icnss_dt_match[] = {
	{.compatible = "qcom,icnss"},
	{}
};

MODULE_DEVICE_TABLE(of, icnss_dt_match);

static struct platform_driver icnss_driver = {
	.probe  = icnss_probe,
	.remove = icnss_remove,
	.driver = {
		.name = "icnss",
		.pm = &icnss_pm_ops,
		.of_match_table = icnss_dt_match,
	},
};

static int __init icnss_initialize(void)
{
	icnss_ipc_log_context = ipc_log_context_create(NUM_LOG_PAGES,
						       "icnss", 0);
	if (!icnss_ipc_log_context)
		icnss_pr_err("Unable to create log context\n");

	icnss_ipc_log_long_context = ipc_log_context_create(NUM_LOG_LONG_PAGES,
						       "icnss_long", 0);
	if (!icnss_ipc_log_long_context)
		icnss_pr_err("Unable to create log long context\n");

	return platform_driver_register(&icnss_driver);
}

static void __exit icnss_exit(void)
{
	platform_driver_unregister(&icnss_driver);
	ipc_log_context_destroy(icnss_ipc_log_context);
	icnss_ipc_log_context = NULL;
	ipc_log_context_destroy(icnss_ipc_log_long_context);
	icnss_ipc_log_long_context = NULL;
}


module_init(icnss_initialize);
module_exit(icnss_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DEVICE "iCNSS CORE platform driver");
