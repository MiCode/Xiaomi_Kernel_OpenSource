/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#ifndef _NET_CNSS_H_
#define _NET_CNSS_H_

#include <linux/device.h>
#include <linux/pci.h>

/* max 20mhz channel count */
#define CNSS_MAX_CH_NUM       45

#define CNSS_MAX_FILE_NAME	  20

/* FW image files */
struct cnss_fw_files {
	char image_file[CNSS_MAX_FILE_NAME];
	char board_data[CNSS_MAX_FILE_NAME];
	char otp_data[CNSS_MAX_FILE_NAME];
	char utf_file[CNSS_MAX_FILE_NAME];
};

struct cnss_wlan_driver {
	char *name;
	int  (*probe)(struct pci_dev *, const struct pci_device_id *);
	void (*remove)(struct pci_dev *);
	int  (*reinit)(struct pci_dev *, const struct pci_device_id *);
	void (*shutdown)(struct pci_dev *);
	int  (*suspend)(struct pci_dev *, pm_message_t state);
	int  (*resume)(struct pci_dev *);
	const struct pci_device_id *id_table;
};

extern void cnss_device_crashed(void);
extern int cnss_get_ramdump_mem(unsigned long *address, unsigned long *size);
extern int cnss_set_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 ch_count);
extern int cnss_get_wlan_unsafe_channel(u16 *unsafe_ch_list,
						u16 *ch_count, u16 buf_len);
extern int cnss_wlan_register_driver(struct cnss_wlan_driver *driver);
extern void cnss_wlan_unregister_driver(struct cnss_wlan_driver *driver);
extern int cnss_get_fw_files(struct cnss_fw_files *pfw_files);
extern void cnss_flush_work(void *work);
extern void cnss_flush_delayed_work(void *dwork);

extern void cnss_pm_wake_lock_init(struct wakeup_source *ws, const char *name);
extern void cnss_pm_wake_lock(struct wakeup_source *ws);
extern void cnss_pm_wake_lock_timeout(struct wakeup_source *ws, ulong msec);
extern void cnss_pm_wake_lock_release(struct wakeup_source *ws);
extern void cnss_pm_wake_lock_destroy(struct wakeup_source *ws);

extern int cnss_set_cpus_allowed_ptr(struct task_struct *task, ulong cpu);

#endif /* _NET_CNSS_H_ */
