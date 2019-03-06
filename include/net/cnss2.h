/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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
#include <linux/usb.h>

#define CNSS_MAX_FILE_NAME		20
#define CNSS_MAX_TIMESTAMP_LEN		32

/*
 * Temporary change for compilation, will be removed
 * after WLAN host driver switched to use new APIs
 */
#define CNSS_API_WITH_DEV

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

struct cnss_soc_info {
	void __iomem *va;
	phys_addr_t pa;
	uint32_t chip_id;
	uint32_t chip_family;
	uint32_t board_id;
	uint32_t soc_id;
	uint32_t fw_version;
	char fw_build_timestamp[CNSS_MAX_TIMESTAMP_LEN + 1];
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
	int  (*suspend_noirq)(struct pci_dev *pdev);
	int  (*resume_noirq)(struct pci_dev *pdev);
	void (*modem_status)(struct pci_dev *, int state);
	void (*update_status)(struct pci_dev *pdev, uint32_t status);
	struct cnss_wlan_runtime_ops *runtime_ops;
	const struct pci_device_id *id_table;
};

struct cnss_usb_wlan_driver {
	char *name;
	int  (*probe)(struct usb_interface *pintf, const struct usb_device_id
		      *id);
	void (*remove)(struct usb_interface *pintf);
	int  (*reinit)(struct usb_interface *pintf, const struct usb_device_id
		       *id);
	void (*shutdown)(struct usb_interface *pintf);
	void (*crash_shutdown)(struct usb_interface *pintf);
	int  (*suspend)(struct usb_interface *pintf, pm_message_t state);
	int  (*resume)(struct usb_interface *pintf);
	int  (*reset_resume)(struct usb_interface *pintf);
	const struct usb_device_id *id_table;
};

enum cnss_driver_status {
	CNSS_UNINITIALIZED,
	CNSS_INITIALIZED,
	CNSS_LOAD_UNLOAD,
	CNSS_RECOVERY,
	CNSS_FW_DOWN,
};

struct cnss_ce_tgt_pipe_cfg {
	u32 pipe_num;
	u32 pipe_dir;
	u32 nentries;
	u32 nbytes_max;
	u32 flags;
	u32 reserved;
};

struct cnss_ce_svc_pipe_cfg {
	u32 service_id;
	u32 pipe_dir;
	u32 pipe_num;
};

struct cnss_shadow_reg_cfg {
	u16 ce_id;
	u16 reg_offset;
};

struct cnss_shadow_reg_v2_cfg {
	u32 addr;
};

struct cnss_rri_over_ddr_cfg {
	u32 base_addr_low;
	u32 base_addr_high;
};

struct cnss_wlan_enable_cfg {
	u32 num_ce_tgt_cfg;
	struct cnss_ce_tgt_pipe_cfg *ce_tgt_cfg;
	u32 num_ce_svc_pipe_cfg;
	struct cnss_ce_svc_pipe_cfg *ce_svc_cfg;
	u32 num_shadow_reg_cfg;
	struct cnss_shadow_reg_cfg *shadow_reg_cfg;
	u32 num_shadow_reg_v2_cfg;
	struct cnss_shadow_reg_v2_cfg *shadow_reg_v2_cfg;
	bool rri_over_ddr_cfg_valid;
	struct cnss_rri_over_ddr_cfg rri_over_ddr_cfg;
};

enum cnss_driver_mode {
	CNSS_MISSION,
	CNSS_FTM,
	CNSS_EPPING,
	CNSS_WALTEST,
	CNSS_OFF,
	CNSS_CCPM,
	CNSS_QVIT,
	CNSS_CALIBRATION,
};

enum cnss_recovery_reason {
	CNSS_REASON_DEFAULT,
	CNSS_REASON_LINK_DOWN,
	CNSS_REASON_RDDM,
	CNSS_REASON_TIMEOUT,
};

extern int cnss_wlan_register_driver(struct cnss_wlan_driver *driver);
extern void cnss_wlan_unregister_driver(struct cnss_wlan_driver *driver);
extern void cnss_device_crashed(struct device *dev);
extern int cnss_pci_link_down(struct device *dev);
extern int cnss_pci_is_device_down(struct device *dev);
extern void cnss_schedule_recovery(struct device *dev,
				   enum cnss_recovery_reason reason);
extern int cnss_self_recovery(struct device *dev,
			      enum cnss_recovery_reason reason);
extern int cnss_force_fw_assert(struct device *dev);
extern int cnss_force_collect_rddm(struct device *dev);
extern void *cnss_get_virt_ramdump_mem(struct device *dev, unsigned long *size);
extern int cnss_get_fw_files_for_target(struct device *dev,
					struct cnss_fw_files *pfw_files,
					u32 target_type, u32 target_version);
extern int cnss_get_platform_cap(struct device *dev,
				 struct cnss_platform_cap *cap);
extern struct dma_iommu_mapping *cnss_smmu_get_mapping(struct device *dev);
extern int cnss_smmu_map(struct device *dev,
			 phys_addr_t paddr, uint32_t *iova_addr, size_t size);
extern int cnss_get_soc_info(struct device *dev, struct cnss_soc_info *info);
extern int cnss_request_bus_bandwidth(struct device *dev, int bandwidth);
extern int cnss_power_up(struct device *dev);
extern int cnss_power_down(struct device *dev);
extern void cnss_request_pm_qos(struct device *dev, u32 qos_val);
extern void cnss_remove_pm_qos(struct device *dev);
extern void cnss_lock_pm_sem(struct device *dev);
extern void cnss_release_pm_sem(struct device *dev);
extern int cnss_wlan_pm_control(struct device *dev, bool vote);
extern int cnss_auto_suspend(struct device *dev);
extern int cnss_auto_resume(struct device *dev);
extern int cnss_pci_force_wake_request(struct device *dev);
extern int cnss_pci_is_device_awake(struct device *dev);
extern int cnss_pci_force_wake_release(struct device *dev);
extern int cnss_get_user_msi_assignment(struct device *dev, char *user_name,
					int *num_vectors,
					uint32_t *user_base_data,
					uint32_t *base_vector);
extern int cnss_get_msi_irq(struct device *dev, unsigned int vector);
extern void cnss_get_msi_address(struct device *dev, uint32_t *msi_addr_low,
				 uint32_t *msi_addr_high);
extern int cnss_wlan_enable(struct device *dev,
			    struct cnss_wlan_enable_cfg *config,
			    enum cnss_driver_mode mode,
			    const char *host_version);
extern int cnss_wlan_disable(struct device *dev, enum cnss_driver_mode mode);
extern unsigned int cnss_get_boot_timeout(struct device *dev);
extern int cnss_athdiag_read(struct device *dev, uint32_t offset,
			     uint32_t mem_type, uint32_t data_len,
			     uint8_t *output);
extern int cnss_athdiag_write(struct device *dev, uint32_t offset,
			      uint32_t mem_type, uint32_t data_len,
			      uint8_t *input);
extern int cnss_set_fw_log_mode(struct device *dev, uint8_t fw_log_mode);
extern int cnss_usb_wlan_register_driver(struct cnss_usb_wlan_driver *driver);
extern void cnss_usb_wlan_unregister_driver(struct cnss_usb_wlan_driver *
					    driver);
#endif /* _NET_CNSS2_H */
