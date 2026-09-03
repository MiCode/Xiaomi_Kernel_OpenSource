/*
 * Copyright (C) 2011 Unisoc Co., Ltd.
 * Youxu.Zeng <Youxu.Zeng@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _SPRD_FCHG_EXTCON_H
#define _SPRD_FCHG_EXTCON_H

#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/usb/phy.h>
#include <linux/usb/sprd_tcpm.h>
#include <linux/usb/sprd_pd.h>

/* Fast charging device name */
#define SPRD_FCHG_TCPM_PD_NAME			"sprd-tcpm-source-psy-sc27xx-pd"
#define SPRD_FCHG_SFCP_NAME			"sc27xx_fast_charger"

#define SPRD_PD_DEFAULT_POWER_UW		10000000

#define SPRD_FCHG_VOLTAGE_5V			5000000
#define SPRD_FCHG_5V_FIXED_PROG_MAX		5900000

#define SPRD_FCHG_NON_5V_FIXED_PROG_MIN_UW	15000000

#define SPRD_FCHG_CURRENT_2A			2000000

#define SPRD_ENABLE_PPS				2
#define SPRD_DISABLE_PPS			1

#define SPRD_FIXED_FCHG_DETECT_MS		msecs_to_jiffies(1000)

struct sprd_fchg_info {
	struct device *dev;
	struct power_supply *psy;
	struct notifier_block fchg_notify;
	struct power_supply *psy_fchg;
	struct delayed_work fixed_fchg_handshake_work;
	struct work_struct pd_online_work;
	struct work_struct fchg_work;
	struct sprd_fchg_ops *ops;
	struct mutex lock;
	struct adapter_power_cap pd_source_cap;
	const char *customized_fchg_psy;
	u32 fchg_type;
	int pd_fixed_max_uw;
	bool chg_online;
	bool pd_extcon;
	bool sfcp_extcon;
	bool customized_fchg_extcon;
	bool detected;
	bool pd_enable;
	bool sfcp_enable;
	bool customized_fchg_enable;
	bool pps_enable;
	bool pps_active;
	bool support_fchg;
	bool support_sfcp;
	bool support_pd_pps;
	bool shutdown_flag;
};

struct sprd_fchg_ops {
	int (*extcon_init)(struct sprd_fchg_info *info, struct power_supply *psy);
	void (*fchg_detect)(struct sprd_fchg_info *info);
	int (*get_fchg_type)(struct sprd_fchg_info *info, u32 *type);
	void (*force_set_fixed_fchg_type)(struct sprd_fchg_info *info);
	int (*get_fchg_vol_max)(struct sprd_fchg_info *info, int *voltage_max);
	int (*get_fchg_cur_max)(struct sprd_fchg_info *info, int input_vol, int *current_max);
	void (*enable_fixed_fchg)(struct sprd_fchg_info *info, bool enable);
	int (*enable_dynamic_fchg)(struct sprd_fchg_info *info, bool enable);
	int (*adj_fchg_vol)(struct sprd_fchg_info *info, u32 input_vol);
	int (*adj_fchg_cur)(struct sprd_fchg_info *info, u32 input_current);
	int (*update_src_cap)(struct sprd_fchg_info *info, const u32 *pdo, unsigned int nr_pdo);
	void (*suspend)(struct sprd_fchg_info *info);
	void (*resume)(struct sprd_fchg_info *info);
	void (*remove)(struct sprd_fchg_info *info);
	void (*shutdown)(struct sprd_fchg_info *info);
};

struct sprd_fchg_info *sprd_fchg_info_register(struct device *dev);

#endif /* _SPRD_VCHG_DETECT_H */
