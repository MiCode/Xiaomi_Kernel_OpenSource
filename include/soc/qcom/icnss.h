/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
#ifndef _ICNSS_WLAN_H_
#define _ICNSS_WLAN_H_

#include <linux/interrupt.h>

#define ICNSS_MAX_IRQ_REGISTRATIONS    12
#define ICNSS_MAX_TIMESTAMP_LEN        32

enum icnss_uevent {
	ICNSS_UEVENT_FW_READY,
	ICNSS_UEVENT_FW_CRASHED,
	ICNSS_UEVENT_FW_DOWN,
};

struct icnss_uevent_fw_down_data {
	bool crashed;
};

struct icnss_uevent_data {
	enum icnss_uevent uevent;
	void *data;
};

struct icnss_driver_ops {
	char *name;
	int (*probe)(struct device *dev);
	void (*remove)(struct device *dev);
	void (*shutdown)(struct device *dev);
	int (*reinit)(struct device *dev);
	void (*crash_shutdown)(void *pdev);
	int (*pm_suspend)(struct device *dev);
	int (*pm_resume)(struct device *dev);
	int (*suspend_noirq)(struct device *dev);
	int (*resume_noirq)(struct device *dev);
	int (*uevent)(struct device *dev, struct icnss_uevent_data *uevent);
};


struct ce_tgt_pipe_cfg {
	u32 pipe_num;
	u32 pipe_dir;
	u32 nentries;
	u32 nbytes_max;
	u32 flags;
	u32 reserved;
};

struct ce_svc_pipe_cfg {
	u32 service_id;
	u32 pipe_dir;
	u32 pipe_num;
};

struct icnss_shadow_reg_cfg {
	u16 ce_id;
	u16 reg_offset;
};

/* CE configuration to target */
struct icnss_wlan_enable_cfg {
	u32 num_ce_tgt_cfg;
	struct ce_tgt_pipe_cfg *ce_tgt_cfg;
	u32 num_ce_svc_pipe_cfg;
	struct ce_svc_pipe_cfg *ce_svc_cfg;
	u32 num_shadow_reg_cfg;
	struct icnss_shadow_reg_cfg *shadow_reg_cfg;
};

/* MSA Memory Regions Information */
struct icnss_mem_region_info {
	uint64_t reg_addr;
	uint32_t size;
	uint8_t secure_flag;
};

/* driver modes */
enum icnss_driver_mode {
	ICNSS_MISSION,
	ICNSS_FTM,
	ICNSS_EPPING,
	ICNSS_WALTEST,
	ICNSS_OFF,
	ICNSS_CCPM,
	ICNSS_QVIT,
};

struct icnss_soc_info {
	void __iomem *v_addr;
	phys_addr_t p_addr;
	uint32_t chip_id;
	uint32_t chip_family;
	uint32_t board_id;
	uint32_t soc_id;
	uint32_t fw_version;
	char fw_build_timestamp[ICNSS_MAX_TIMESTAMP_LEN + 1];
};

extern int icnss_register_driver(struct icnss_driver_ops *driver);
extern int icnss_unregister_driver(struct icnss_driver_ops *driver);
extern int icnss_wlan_enable(struct icnss_wlan_enable_cfg *config,
			     enum icnss_driver_mode mode,
			     const char *host_version);
extern int icnss_wlan_disable(enum icnss_driver_mode mode);
extern void icnss_enable_irq(unsigned int ce_id);
extern void icnss_disable_irq(unsigned int ce_id);
extern int icnss_get_soc_info(struct icnss_soc_info *info);
extern int icnss_ce_free_irq(unsigned int ce_id, void *ctx);
extern int icnss_ce_request_irq(unsigned int ce_id,
	irqreturn_t (*handler)(int, void *),
	unsigned long flags, const char *name, void *ctx);
extern int icnss_get_ce_id(int irq);
extern int icnss_set_fw_log_mode(uint8_t fw_log_mode);
extern int icnss_athdiag_read(struct device *dev, uint32_t offset,
			      uint32_t mem_type, uint32_t data_len,
			      uint8_t *output);
extern int icnss_athdiag_write(struct device *dev, uint32_t offset,
			       uint32_t mem_type, uint32_t data_len,
			       uint8_t *input);
extern int icnss_get_irq(int ce_id);
extern int icnss_power_on(struct device *dev);
extern int icnss_power_off(struct device *dev);
extern struct dma_iommu_mapping *icnss_smmu_get_mapping(struct device *dev);
extern int icnss_smmu_map(struct device *dev, phys_addr_t paddr,
			  uint32_t *iova_addr, size_t size);
extern unsigned int icnss_socinfo_get_serial_number(struct device *dev);
extern int icnss_set_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 ch_count);
extern int icnss_get_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 *ch_count,
					 u16 buf_len);
extern int icnss_wlan_set_dfs_nol(const void *info, u16 info_len);
extern int icnss_wlan_get_dfs_nol(void *info, u16 info_len);
extern bool icnss_is_qmi_disable(void);
extern bool icnss_is_fw_ready(void);
extern int icnss_set_wlan_mac_address(const u8 *in, const uint32_t len);
extern u8 *icnss_get_wlan_mac_address(struct device *dev, uint32_t *num);
extern int icnss_trigger_recovery(struct device *dev);
extern int icnss_get_driver_load_cnt(void);
extern void icnss_increment_driver_load_cnt(void);
#endif /* _ICNSS_WLAN_H_ */
