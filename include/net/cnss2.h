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

#ifndef _NET_CNSS2_H
#define _NET_CNSS2_H

#include <linux/pci.h>

#define CNSS_MAX_FILE_NAME		20

enum cnss_bus_width_type {
	CNSS_BUS_WIDTH_NONE,
	CNSS_BUS_WIDTH_LOW,
	CNSS_BUS_WIDTH_MEDIUM,
	CNSS_BUS_WIDTH_HIGH
};

enum cnss_platform_cap_flag {
	CNSS_HAS_EXTERNAL_SWREG = 0x01,
	CNSS_HAS_UART_ACCESS = 0x02,
};

struct cnss_platform_cap {
	u32 cap_flag;
};

struct cnss_fw_files {
	char image_file[CNSS_MAX_FILE_NAME];
	char board_data[CNSS_MAX_FILE_NAME];
	char otp_data[CNSS_MAX_FILE_NAME];
	char utf_file[CNSS_MAX_FILE_NAME];
	char utf_board_data[CNSS_MAX_FILE_NAME];
	char epping_file[CNSS_MAX_FILE_NAME];
	char evicted_data[CNSS_MAX_FILE_NAME];
};

struct cnss_wlan_runtime_ops {
	int (*runtime_suspend)(struct pci_dev *pdev);
	int (*runtime_resume)(struct pci_dev *pdev);
};

struct cnss_wlan_driver {
	char *name;
	int  (*probe)(struct pci_dev *pdev, const struct pci_device_id *id);
	void (*remove)(struct pci_dev *pdev);
	int  (*reinit)(struct pci_dev *pdev, const struct pci_device_id *id);
	void (*shutdown)(struct pci_dev *pdev);
	void (*crash_shutdown)(struct pci_dev *pdev);
	int  (*suspend)(struct pci_dev *pdev, pm_message_t state);
	int  (*resume)(struct pci_dev *pdev);
	void (*modem_status)(struct pci_dev *, int state);
	struct cnss_wlan_runtime_ops *runtime_ops;
	const struct pci_device_id *id_table;
};

enum cnss_driver_status {
	CNSS_UNINITIALIZED,
	CNSS_INITIALIZED,
	CNSS_LOAD_UNLOAD
};

extern int cnss_wlan_register_driver(struct cnss_wlan_driver *driver);
extern void cnss_wlan_unregister_driver(struct cnss_wlan_driver *driver);
extern void cnss_wlan_pci_link_down(void);
extern void cnss_schedule_recovery_work(void);
extern void cnss_device_crashed(void);
extern void cnss_device_self_recovery(void);
extern void *cnss_get_virt_ramdump_mem(unsigned long *size);
extern int cnss_get_fw_files_for_target(struct cnss_fw_files *pfw_files,
					u32 target_type, u32 target_version);
extern int cnss_get_platform_cap(struct cnss_platform_cap *cap);
extern void cnss_set_driver_status(enum cnss_driver_status driver_status);
extern int cnss_request_bus_bandwidth(int bandwidth);
extern int cnss_set_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 ch_count);
extern int cnss_get_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 *ch_count,
					u16 buf_len);
extern int cnss_wlan_set_dfs_nol(const void *info, u16 info_len);
extern int cnss_wlan_get_dfs_nol(void *info, u16 info_len);
extern int cnss_power_up(struct device *dev);
extern int cnss_power_down(struct device *dev);
extern u8 *cnss_common_get_wlan_mac_address(struct device *dev, uint32_t *num);
extern void cnss_request_pm_qos(u32 qos_val);
extern void cnss_remove_pm_qos(void);
extern void cnss_lock_pm_sem(void);
extern void cnss_release_pm_sem(void);
extern int cnss_wlan_pm_control(bool vote);
extern int cnss_auto_suspend(void);
extern int cnss_auto_resume(void);

#endif /* _NET_CNSS2_H */
