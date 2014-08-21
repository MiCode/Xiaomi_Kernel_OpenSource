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

#ifdef CONFIG_WCNSS_MEM_PRE_ALLOC
#define WCNSS_PRE_ALLOC_GET_THRESHOLD (4*1024)
#endif

/* max 20mhz channel count */
#define CNSS_MAX_CH_NUM       45

#define CNSS_MAX_FILE_NAME	  20

#define MAX_FIRMWARE_SIZE (512 * 1024)

enum cnss_bus_width_type {
	CNSS_BUS_WIDTH_NONE,
	CNSS_BUS_WIDTH_LOW,
	CNSS_BUS_WIDTH_MEDIUM,
	CNSS_BUS_WIDTH_HIGH
};

/* FW image files */
struct cnss_fw_files {
	char image_file[CNSS_MAX_FILE_NAME];
	char board_data[CNSS_MAX_FILE_NAME];
	char otp_data[CNSS_MAX_FILE_NAME];
	char utf_file[CNSS_MAX_FILE_NAME];
	char utf_board_data[CNSS_MAX_FILE_NAME];
	char epping_file[CNSS_MAX_FILE_NAME];
	char evicted_data[CNSS_MAX_FILE_NAME];
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
	const struct pci_device_id *id_table;
};

/*
 * codeseg_total_bytes: Total bytes across all the codesegment blocks
 * num_codesegs: No of Pages used
 * codeseg_size: Size of each segment. Should be power of 2 and multiple of 4K
 * codeseg_size_log2: log2(codeseg_size)
 * codeseg_busaddr: Physical address of the DMAble memory;4K aligned
 */

#define CODESWAP_MAX_CODESEGS 16
struct codeswap_codeseg_info {
	u32   codeseg_total_bytes;
	u32   num_codesegs;
	u32   codeseg_size;
	u32   codeseg_size_log2;
	void *codeseg_busaddr[CODESWAP_MAX_CODESEGS];
};

/* platform capabilities */
enum cnss_platform_cap_flag {
	CNSS_HAS_EXTERNAL_SWREG = 0x01,
	CNSS_HAS_UART_ACCESS = 0x02,
};

struct cnss_platform_cap {
	u32 cap_flag;
};

/* WLAN driver status */
enum cnss_driver_status {
	CNSS_UNINITIALIZED,
	CNSS_INITIALIZED,
	CNSS_LOAD_UNLOAD
};

extern int cnss_get_fw_image(dma_addr_t *fw_image, u32 *image_size);
extern void cnss_device_crashed(void);
extern void cnss_device_self_recovery(void);
extern int cnss_get_ramdump_mem(unsigned long *address, unsigned long *size);
extern int cnss_set_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 ch_count);
extern int cnss_get_wlan_unsafe_channel(u16 *unsafe_ch_list,
		u16 *ch_count, u16 buf_len);
extern int cnss_wlan_set_dfs_nol(void *info, u16 info_len);
extern int cnss_wlan_get_dfs_nol(void *info, u16 info_len);
extern void cnss_schedule_recovery_work(void);
extern void cnss_wlan_pci_link_down(void);
extern int cnss_pcie_shadow_control(struct pci_dev *dev, bool enable);
extern int cnss_wlan_register_driver(struct cnss_wlan_driver *driver);
extern void cnss_wlan_unregister_driver(struct cnss_wlan_driver *driver);
extern int cnss_get_fw_files(struct cnss_fw_files *pfw_files);
extern int cnss_get_fw_files_for_target(struct cnss_fw_files *pfw_files,
					u32 target_type, u32 target_version);
extern void cnss_flush_work(void *work);
extern void cnss_flush_delayed_work(void *dwork);
extern void cnss_get_monotonic_boottime(struct timespec *ts);
extern void cnss_get_boottime(struct timespec *ts);
extern void cnss_init_work(struct work_struct *work, work_func_t func);
extern void cnss_init_delayed_work(struct delayed_work *work, work_func_t func);
extern int cnss_request_bus_bandwidth(int bandwidth);

extern int cnss_get_sha_hash(const u8 *data, u32 data_len,
					u8 *hash_idx, u8 *out);
extern void *cnss_get_fw_ptr(void);

extern int cnss_get_codeswap_struct(struct codeswap_codeseg_info *swap_seg);

extern void cnss_pm_wake_lock_init(struct wakeup_source *ws, const char *name);
extern void cnss_pm_wake_lock(struct wakeup_source *ws);
extern void cnss_pm_wake_lock_timeout(struct wakeup_source *ws, ulong msec);
extern void cnss_pm_wake_lock_release(struct wakeup_source *ws);
extern void cnss_pm_wake_lock_destroy(struct wakeup_source *ws);
#ifdef CONFIG_PCI_MSM
extern int cnss_wlan_pm_control(bool vote);
#endif
extern void cnss_lock_pm_sem(void);
extern void cnss_release_pm_sem(void);

extern int cnss_set_cpus_allowed_ptr(struct task_struct *task, ulong cpu);
extern void cnss_request_pm_qos(u32 qos_val);
extern void cnss_remove_pm_qos(void);
extern int cnss_get_platform_cap(struct cnss_platform_cap *cap);
extern void cnss_set_driver_status(enum cnss_driver_status driver_status);

#ifdef CONFIG_WCNSS_MEM_PRE_ALLOC
extern void *wcnss_prealloc_get(unsigned int size);
extern int wcnss_prealloc_put(void *ptr);
#endif

extern int msm_pcie_enumerate(u32 rc_idx);
#endif /* _NET_CNSS_H_ */
