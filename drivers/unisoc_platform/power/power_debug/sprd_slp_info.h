/* SPDX-License-Identifier: GPL-2.0 */
//
// UNISOC APCPU POWER STAT driver
// Copyright (C) 2020 Unisoc, Inc.

#ifndef __SPRD_SLP_INFO_H__
#define __SPRD_SLP_INFO_H__

#include <linux/hwspinlock.h>
#include <linux/soc/sprd/sprd_pdbg.h>

struct subsys_slp_info_data;
typedef void (*slp_info_update_func)(struct subsys_slp_info_data *subsys_data, u32 stage);

struct slp_info_reg {
	struct regmap *map;
	u32 reg;
	u32 offset;
	u32 mask;
	u32 last_val;
};

struct slp_lock {
	struct mutex mtx;
	struct hwspinlock *hwlock;
};

struct subsys_slp_info_data {
	struct subsys_slp_info *slp_info;
	struct subsys_slp_info slp_info_get;
	struct slp_info_reg slp_cnt;
	struct slp_info_reg slp_state;
	struct slp_info_reg slp_time;
	slp_info_update_func info_update;
	slp_info_update_func info_update_ext;
	struct slp_lock *lock;
	u32 index;
	bool update_ext;
};

struct subsys_data_var {
	const char *name;
	slp_info_update_func info_update;
	slp_info_update_func info_update_ext;
};

struct slp_info_data {
	struct subsys_slp_info_data *subsys_datas;
	u32 subsys_datas_cnt;
	pdbg_notify_cb notify_cb;
	struct slp_lock lock;
	void *virt_base;
};

int sprd_slp_info_init(struct device *dev, struct proc_dir_entry *dir, struct slp_info_data **data);
#endif /* __SPRD_SLP_INFO_H__ */
