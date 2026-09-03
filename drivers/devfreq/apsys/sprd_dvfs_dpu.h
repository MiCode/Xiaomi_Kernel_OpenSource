// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef __SPRD_DVFS_DPU_H__
#define __SPRD_DVFS_DPU_H__

#include <linux/apsys_dvfs.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/types.h>

#include "sprd_dvfs_apsys.h"

typedef enum {
	VOLT70 = 0, //0.7v
	VOLT75, //0.75v
	VOLT80, //0.8v
} voltage_level;

typedef enum {
	DPU_CLK_INDEX_153M6 = 0,
	DPU_CLK_INDEX_192M,
	DPU_CLK_INDEX_256M,
	DPU_CLK_INDEX_307M2,
	DPU_CLK_INDEX_384M,
	DPU_CLK_INDEX_409M6,
	DPU_CLK_INDEX_512M,
	DPU_CLK_INDEX_468M,
	DPU_CLK_INDEX_614M4,
} clock_level;

typedef enum {
	DPU_CLK153M6 = 153600000,
	DPU_CLK192M = 192000000,
	DPU_CLK256M = 256000000,
	DPU_CLK307M2 = 307200000,
	DPU_CLK384M = 384000000,
	DPU_CLK409M6 = 409600000,
	DPU_CLK468M = 468000000,
	DPU_CLK512M = 512000000,
	DPU_CLK614M4 = 614400000,
} clock_rate;

struct dpu_dvfs {
	int dvfs_enable;
	struct device dev;

	struct devfreq *devfreq;
	struct opp_table *opp_table;
	struct devfreq_event_dev *edev;
	struct notifier_block dpu_dvfs_nb;

	u32 work_freq;
	u32 idle_freq;
	set_freq_type freq_type;

	struct ip_dvfs_coffe dvfs_coffe;
	struct ip_dvfs_status dvfs_status;
	const struct dpu_dvfs_ops *dvfs_ops;

	struct apsys_dev *apsys;
};

struct dpu_dvfs_ops {
	/* initialization interface */
	int (*parse_dt)(struct dpu_dvfs *dpu, struct device_node *np);
	int (*dvfs_init)(struct dpu_dvfs *dpu);
	void (*hw_dfs_en)(struct dpu_dvfs *dpu, bool dfs_en);

	/* work-idle dvfs index ops */
	void  (*set_work_index)(struct dpu_dvfs *dpu, int index);
	int  (*get_work_index)(struct dpu_dvfs *dpu);
	void  (*set_idle_index)(struct dpu_dvfs *dpu, int index);
	int  (*get_idle_index)(struct dpu_dvfs *dpu);

	/* work-idle dvfs freq ops */
	void (*set_work_freq)(struct dpu_dvfs *dpu, u32 freq);
	u32 (*get_work_freq)(struct dpu_dvfs *dpu);
	void (*set_idle_freq)(struct dpu_dvfs *dpu, u32 freq);
	u32 (*get_idle_freq)(struct dpu_dvfs *dpu);

	/* work-idle dvfs map ops */
	int  (*get_dvfs_table)(struct ip_dvfs_map_cfg *dvfs_table);
	void (*set_dvfs_table)(struct ip_dvfs_map_cfg *dvfs_table);
	void (*get_dvfs_coffe)(struct ip_dvfs_coffe *dvfs_coffe);
	void (*set_dvfs_coffe)(struct ip_dvfs_coffe *dvfs_coffe);
	void (*get_dvfs_status)(struct dpu_dvfs *dpu, struct ip_dvfs_status *dvfs_status);

	/* coffe setting ops */
	void (*set_gfree_wait_delay)(struct dpu_dvfs *dpu, u32 para);
	void (*set_freq_upd_en_byp)(struct dpu_dvfs *dpu, bool enable);
	void (*set_freq_upd_delay_en)(struct dpu_dvfs *dpu, bool enable);
	void (*set_freq_upd_hdsk_en)(struct dpu_dvfs *dpu, bool enable);
	void (*set_dvfs_swtrig_en)(struct dpu_dvfs *dpu, bool enable);
};

struct sprd_dpu_dvfs_ops {
	const struct dpu_dvfs_ops *core;
};

extern const struct dpu_dvfs_ops sharkl5pro_dpu_dvfs_ops;
extern const struct dpu_dvfs_ops sharkl5_dpu_dvfs_ops;
extern const struct dpu_dvfs_ops qogirl6_dpu_dvfs_ops;
extern const struct dpu_dvfs_ops qogirn6pro_dpu_dvfs_ops;
extern const struct dpu_dvfs_ops qogirn6lite_dpu_dvfs_ops;
extern const struct dpu_dvfs_ops roc1_dpu_dvfs_ops;

#endif /* __SPRD_DVFS_DPU_H__ */
