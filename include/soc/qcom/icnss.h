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
#ifndef _ICNSS_WLAN_H_
#define _ICNSS_WLAN_H_

#include <linux/interrupt.h>

#define ICNSS_MAX_IRQ_REGISTRATIONS    12

struct icnss_driver_ops {
	char *name;
	int (*probe)(struct device *dev);
	void (*remove)(struct device *dev);
	void (*shutdown)(struct device *dev);
	int (*reinit)(struct device *dev);
	void (*crash_shutdown)(void *pdev);
	int (*suspend)(struct device *dev, pm_message_t state);
	int (*resume)(struct device *dev);
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
	u32 version;
};

extern int icnss_register_driver(struct icnss_driver_ops *driver);
extern int icnss_unregister_driver(struct icnss_driver_ops *driver);
extern int icnss_register_ce_irq(unsigned int ce_id,
	irqreturn_t (*handler)(int, void *),
		unsigned long flags, const char *name);
extern int icnss_unregister_ce_irq(unsigned int ce_id);
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
extern int icnss_set_fw_debug_mode(bool enablefwlog);

#endif /* _ICNSS_WLAN_H_ */
