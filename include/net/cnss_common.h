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

#ifndef _NET_CNSS_COMMON_H_
#define _NET_CNSS_COMMON_H_

#ifdef CONFIG_CNSS

#define MAX_FIRMWARE_SIZE (1 * 1024 * 1024)
/* max 20mhz channel count */
#define CNSS_MAX_CH_NUM		45

extern int cnss_set_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 ch_count);
extern int cnss_get_wlan_unsafe_channel(u16 *unsafe_ch_list,
			u16 *ch_count, u16 buf_len);

extern int cnss_wlan_set_dfs_nol(const void *info, u16 info_len);
extern int cnss_wlan_get_dfs_nol(void *info, u16 info_len);

extern void cnss_init_work(struct work_struct *work, work_func_t func);
extern void cnss_flush_work(void *work);
extern void cnss_flush_delayed_work(void *dwork);
extern void cnss_pm_wake_lock_timeout(struct wakeup_source *ws, ulong msec);
extern void cnss_pm_wake_lock_release(struct wakeup_source *ws);
extern void cnss_pm_wake_lock_destroy(struct wakeup_source *ws);
extern void cnss_get_monotonic_boottime(struct timespec *ts);
extern void cnss_get_boottime(struct timespec *ts);
extern void cnss_init_delayed_work(struct delayed_work *work, work_func_t func);
extern int cnss_vendor_cmd_reply(struct sk_buff *skb);
extern int cnss_set_cpus_allowed_ptr(struct task_struct *task, ulong cpu);
extern void cnss_dump_stack(struct task_struct *task);

int cnss_pci_request_bus_bandwidth(int bandwidth);
int cnss_sdio_request_bus_bandwidth(int bandwidth);
extern int cnss_common_request_bus_bandwidth(struct device *dev,
				int bandwidth);

void cnss_sdio_device_crashed(void);
void cnss_pci_device_crashed(void);
extern void cnss_common_device_crashed(struct device *dev);

void cnss_pci_device_self_recovery(void);
void cnss_sdio_device_self_recovery(void);
extern void cnss_common_device_self_recovery(struct device *dev);

void *cnss_pci_get_virt_ramdump_mem(unsigned long *size);
void *cnss_sdio_get_virt_ramdump_mem(unsigned long *size);
extern void *cnss_common_get_virt_ramdump_mem(struct device *dev,
				unsigned long *size);

void cnss_sdio_schedule_recovery_work(void);
void cnss_pci_schedule_recovery_work(void);
extern void cnss_common_schedule_recovery_work(struct device *dev);

extern int cnss_pcie_set_wlan_mac_address(const u8 *in, uint32_t len);
extern int cnss_sdio_set_wlan_mac_address(const u8 *in, uint32_t len);
extern int cnss_common_set_wlan_mac_address(struct device *dev,
				const u8 *in, uint32_t len);

u8 *cnss_pci_get_wlan_mac_address(uint32_t *num);
u8 *cnss_sdio_get_wlan_mac_address(uint32_t *num);
extern u8 *cnss_common_get_wlan_mac_address(struct device *dev, uint32_t *num);
#endif
#endif /* _NET_CNSS_COMMON_H_ */
