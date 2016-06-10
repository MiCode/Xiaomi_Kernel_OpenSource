/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/sched.h>
#include <linux/suspend.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <net/cnss.h>
#include "cnss_common.h"
#include <net/cfg80211.h>

enum cnss_dev_bus_type {
	CNSS_BUS_NONE = -1,
	CNSS_BUS_PCI,
	CNSS_BUS_SDIO
};

static DEFINE_MUTEX(unsafe_channel_list_lock);
static DEFINE_MUTEX(dfs_nol_info_lock);

static struct cnss_unsafe_channel_list {
	u16 unsafe_ch_count;
	u16 unsafe_ch_list[CNSS_MAX_CH_NUM];
} unsafe_channel_list;

static struct cnss_dfs_nol_info {
	void *dfs_nol_info;
	u16 dfs_nol_info_len;
} dfs_nol_info;

int cnss_set_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 ch_count)
{
	mutex_lock(&unsafe_channel_list_lock);
	if ((!unsafe_ch_list) || (ch_count > CNSS_MAX_CH_NUM)) {
		mutex_unlock(&unsafe_channel_list_lock);
		return -EINVAL;
	}

	unsafe_channel_list.unsafe_ch_count = ch_count;

	if (ch_count != 0) {
		memcpy(
			(char *)unsafe_channel_list.unsafe_ch_list,
			(char *)unsafe_ch_list, ch_count * sizeof(u16));
	}
	mutex_unlock(&unsafe_channel_list_lock);

	return 0;
}
EXPORT_SYMBOL(cnss_set_wlan_unsafe_channel);

int cnss_get_wlan_unsafe_channel(
			u16 *unsafe_ch_list,
			u16 *ch_count, u16 buf_len)
{
	mutex_lock(&unsafe_channel_list_lock);
	if (!unsafe_ch_list || !ch_count) {
		mutex_unlock(&unsafe_channel_list_lock);
		return -EINVAL;
	}

	if (buf_len < (unsafe_channel_list.unsafe_ch_count * sizeof(u16))) {
		mutex_unlock(&unsafe_channel_list_lock);
		return -ENOMEM;
	}

	*ch_count = unsafe_channel_list.unsafe_ch_count;
	memcpy(
		(char *)unsafe_ch_list,
		(char *)unsafe_channel_list.unsafe_ch_list,
		unsafe_channel_list.unsafe_ch_count * sizeof(u16));
	mutex_unlock(&unsafe_channel_list_lock);

	return 0;
}
EXPORT_SYMBOL(cnss_get_wlan_unsafe_channel);

int cnss_wlan_set_dfs_nol(const void *info, u16 info_len)
{
	void *temp;
	struct cnss_dfs_nol_info *dfs_info;

	mutex_lock(&dfs_nol_info_lock);
	if (!info || !info_len) {
		mutex_unlock(&dfs_nol_info_lock);
		return -EINVAL;
	}

	temp = kmalloc(info_len, GFP_KERNEL);
	if (!temp) {
		mutex_unlock(&dfs_nol_info_lock);
		return -ENOMEM;
	}

	memcpy(temp, info, info_len);
	dfs_info = &dfs_nol_info;
	kfree(dfs_info->dfs_nol_info);

	dfs_info->dfs_nol_info = temp;
	dfs_info->dfs_nol_info_len = info_len;
	mutex_unlock(&dfs_nol_info_lock);

	return 0;
}
EXPORT_SYMBOL(cnss_wlan_set_dfs_nol);

int cnss_wlan_get_dfs_nol(void *info, u16 info_len)
{
	int len;
	struct cnss_dfs_nol_info *dfs_info;

	mutex_lock(&dfs_nol_info_lock);
	if (!info || !info_len) {
		mutex_unlock(&dfs_nol_info_lock);
		return -EINVAL;
	}

	dfs_info = &dfs_nol_info;

	if (dfs_info->dfs_nol_info == NULL || dfs_info->dfs_nol_info_len == 0) {
		mutex_unlock(&dfs_nol_info_lock);
		return -ENOENT;
	}

	len = min(info_len, dfs_info->dfs_nol_info_len);

	memcpy(info, dfs_info->dfs_nol_info, len);
	mutex_unlock(&dfs_nol_info_lock);

	return len;
}
EXPORT_SYMBOL(cnss_wlan_get_dfs_nol);

void cnss_init_work(struct work_struct *work, work_func_t func)
{
	INIT_WORK(work, func);
}
EXPORT_SYMBOL(cnss_init_work);

void cnss_flush_work(void *work)
{
	struct work_struct *cnss_work = work;

	cancel_work_sync(cnss_work);
}
EXPORT_SYMBOL(cnss_flush_work);

void cnss_flush_delayed_work(void *dwork)
{
	struct delayed_work *cnss_dwork = dwork;

	cancel_delayed_work_sync(cnss_dwork);
}
EXPORT_SYMBOL(cnss_flush_delayed_work);

void cnss_pm_wake_lock_init(struct wakeup_source *ws, const char *name)
{
	wakeup_source_init(ws, name);
}
EXPORT_SYMBOL(cnss_pm_wake_lock_init);

void cnss_pm_wake_lock(struct wakeup_source *ws)
{
	__pm_stay_awake(ws);
}
EXPORT_SYMBOL(cnss_pm_wake_lock);

void cnss_pm_wake_lock_timeout(struct wakeup_source *ws, ulong msec)
{
	__pm_wakeup_event(ws, msec);
}
EXPORT_SYMBOL(cnss_pm_wake_lock_timeout);

void cnss_pm_wake_lock_release(struct wakeup_source *ws)
{
	__pm_relax(ws);
}
EXPORT_SYMBOL(cnss_pm_wake_lock_release);

void cnss_pm_wake_lock_destroy(struct wakeup_source *ws)
{
	wakeup_source_trash(ws);
}
EXPORT_SYMBOL(cnss_pm_wake_lock_destroy);

void cnss_get_monotonic_boottime(struct timespec *ts)
{
	get_monotonic_boottime(ts);
}
EXPORT_SYMBOL(cnss_get_monotonic_boottime);

void cnss_get_boottime(struct timespec *ts)
{
	ktime_get_ts(ts);
}
EXPORT_SYMBOL(cnss_get_boottime);

void cnss_init_delayed_work(struct delayed_work *work, work_func_t func)
{
	INIT_DELAYED_WORK(work, func);
}
EXPORT_SYMBOL(cnss_init_delayed_work);

int cnss_vendor_cmd_reply(struct sk_buff *skb)
{
	return cfg80211_vendor_cmd_reply(skb);
}
EXPORT_SYMBOL(cnss_vendor_cmd_reply);

int cnss_set_cpus_allowed_ptr(struct task_struct *task, ulong cpu)
{
	return set_cpus_allowed_ptr(task, cpumask_of(cpu));
}
EXPORT_SYMBOL(cnss_set_cpus_allowed_ptr);

/* wlan prop driver cannot invoke show_stack
 * function directly, so to invoke this function it
 * call wcnss_dump_stack function
 */
void cnss_dump_stack(struct task_struct *task)
{
	show_stack(task, NULL);
}
EXPORT_SYMBOL(cnss_dump_stack);

enum cnss_dev_bus_type cnss_get_dev_bus_type(struct device *dev)
{
	if (!dev)
		return CNSS_BUS_NONE;

	if (!dev->bus)
		return CNSS_BUS_NONE;

	if (memcmp(dev->bus->name, "sdio", 4) == 0)
		return CNSS_BUS_SDIO;
	else if (memcmp(dev->bus->name, "pci", 3) == 0)
		return CNSS_BUS_PCI;
	else
		return CNSS_BUS_NONE;
}

int cnss_common_request_bus_bandwidth(struct device *dev, int bandwidth)
{
	int ret;

	switch (cnss_get_dev_bus_type(dev)) {
	case CNSS_BUS_SDIO:
		ret = cnss_sdio_request_bus_bandwidth(bandwidth);
		break;
	case CNSS_BUS_PCI:
		ret = cnss_pci_request_bus_bandwidth(bandwidth);
		break;
	default:
		pr_debug("%s: Invalid device type\n", __func__);
		ret = -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(cnss_common_request_bus_bandwidth);

void *cnss_common_get_virt_ramdump_mem(struct device *dev, unsigned long *size)
{
	switch (cnss_get_dev_bus_type(dev)) {
	case CNSS_BUS_SDIO:
		return cnss_sdio_get_virt_ramdump_mem(size);
	case CNSS_BUS_PCI:
		return cnss_pci_get_virt_ramdump_mem(size);
	default:
		pr_debug("%s: Invalid device type\n", __func__);
		return NULL;
	}
}
EXPORT_SYMBOL(cnss_common_get_virt_ramdump_mem);

void cnss_common_device_self_recovery(struct device *dev)
{
	switch (cnss_get_dev_bus_type(dev)) {
	case CNSS_BUS_SDIO:
		cnss_sdio_device_self_recovery();
		break;
	case CNSS_BUS_PCI:
		cnss_pci_device_self_recovery();
		break;
	default:
		pr_debug("%s: Invalid device type\n", __func__);
		break;
	}
}
EXPORT_SYMBOL(cnss_common_device_self_recovery);

void cnss_common_schedule_recovery_work(struct device *dev)
{
	switch (cnss_get_dev_bus_type(dev)) {
	case CNSS_BUS_SDIO:
		cnss_sdio_schedule_recovery_work();
		break;
	case CNSS_BUS_PCI:
		cnss_pci_schedule_recovery_work();
		break;
	default:
		pr_debug("%s: Invalid device type\n", __func__);
		break;
	}
}
EXPORT_SYMBOL(cnss_common_schedule_recovery_work);

void cnss_common_device_crashed(struct device *dev)
{
	switch (cnss_get_dev_bus_type(dev)) {
	case CNSS_BUS_SDIO:
		cnss_sdio_device_crashed();
		break;
	case CNSS_BUS_PCI:
		cnss_pci_device_crashed();
		break;
	default:
		pr_debug("%s: Invalid device type\n", __func__);
		break;
	}
}
EXPORT_SYMBOL(cnss_common_device_crashed);

u8 *cnss_common_get_wlan_mac_address(struct device *dev, uint32_t *num)
{
	u8 *ret;

	switch (cnss_get_dev_bus_type(dev)) {
	case CNSS_BUS_SDIO:
		ret = cnss_sdio_get_wlan_mac_address(num);
		break;
	case CNSS_BUS_PCI:
		ret = cnss_pci_get_wlan_mac_address(num);
		break;
	default:
		pr_debug("%s: Invalid device type\n", __func__);
		ret = NULL;
		break;
	}
	return ret;
}
EXPORT_SYMBOL(cnss_common_get_wlan_mac_address);

int cnss_common_set_wlan_mac_address(
		struct device *dev, const u8 *in, uint32_t len)
{
	int ret;

	switch (cnss_get_dev_bus_type(dev)) {
	case CNSS_BUS_SDIO:
		ret = cnss_sdio_set_wlan_mac_address(in, len);
		break;
	case CNSS_BUS_PCI:
		ret = cnss_pcie_set_wlan_mac_address(in, len);
		break;
	default:
		pr_debug("%s: Invalid device type\n", __func__);
		ret = -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(cnss_common_set_wlan_mac_address);

int cnss_power_up(struct device *dev)
{
	int ret;

	switch (cnss_get_dev_bus_type(dev)) {
	case CNSS_BUS_PCI:
		ret = cnss_pcie_power_up(dev);
		break;
	case CNSS_BUS_SDIO:
		ret = cnss_sdio_power_up(dev);
		break;
	default:
		pr_err("%s: Invalid Bus Type\n", __func__);
		ret = -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(cnss_power_up);

int cnss_power_down(struct device *dev)
{
	int ret;

	switch (cnss_get_dev_bus_type(dev)) {
	case CNSS_BUS_PCI:
		ret = cnss_pcie_power_down(dev);
		break;
	case CNSS_BUS_SDIO:
		ret = cnss_sdio_power_down(dev);
		break;
	default:
		pr_err("%s: Invalid Bus Type\n", __func__);
		ret = -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(cnss_power_down);
