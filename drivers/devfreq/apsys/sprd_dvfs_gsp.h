// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef __SPRD_DVFS_GSP_H__
#define __SPRD_DVFS_GSP_H__

#include <linux/apsys_dvfs.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/types.h>

#include "sprd_dvfs_apsys.h"

typedef enum {
	VOLT55 = 0, //0.55v
	VOLT60 = 1, //0.60v
	VOLT65 = 2, //0.65v
	VOLT70 = 3, //0.70v
	VOLT75 = 4, //0.75v
} voltage_level;

typedef enum {
	GSP_CLK_INDEX_256M = 0,
	GSP_CLK_INDEX_307M2,
	GSP_CLK_INDEX_384M,
	GSP_CLK_INDEX_512M,
	GSP_CLK_INDEX_614M4,
} clock_level;

typedef enum {
	GSP_CLK256M = 256000000,
	GSP_CLK307M2 = 307200000,
	GSP_CLK384M = 384000000,
	GSP_CLK512M = 512000000,
	GSP_CLK614M4 = 614400000,
} clock_rate;

struct gsp_dvfs {
	int dvfs_enable;
	struct device dev;

	struct devfreq *devfreq;
	struct opp_table *opp_table;
	struct devfreq_event_dev *edev;
	struct notifier_block gsp_dvfs_nb;

	u32 work_freq;
	u32 idle_freq;
	set_freq_type freq_type;

	struct ip_dvfs_coffe dvfs_coffe;
	struct ip_dvfs_status dvfs_status;
	const struct gsp_dvfs_ops *dvfs_ops;

	struct apsys_dev *apsys;
};

struct gsp_dvfs_ops {
	/* initialization interface */
	int (*parse_dt)(struct gsp_dvfs *gsp, struct device_node *np);
	int (*dvfs_init)(struct gsp_dvfs *gsp);
	void (*hw_dfs_en)(struct gsp_dvfs *gsp, bool dfs_en);

	/* work-idle dvfs index ops */
	void  (*set_work_index)(struct gsp_dvfs *gsp, int index);
	int  (*get_work_index)(struct gsp_dvfs *gsp);
	void  (*set_idle_index)(struct gsp_dvfs *gsp, int index);
	int  (*get_idle_index)(struct gsp_dvfs *gsp);

	/* work-idle dvfs freq ops */
	void (*set_work_freq)(struct gsp_dvfs *gsp, u32 freq);
	u32 (*get_work_freq)(struct gsp_dvfs *gsp);
	void (*set_idle_freq)(struct gsp_dvfs *gsp, u32 freq);
	u32 (*get_idle_freq)(struct gsp_dvfs *gsp);

	/* work-idle dvfs map ops */
	int  (*get_dvfs_table)(struct ip_dvfs_map_cfg *dvfs_table);
	void (*set_dvfs_table)(struct ip_dvfs_map_cfg *dvfs_table);
	void (*get_dvfs_coffe)(struct ip_dvfs_coffe *dvfs_coffe);
	void (*set_dvfs_coffe)(struct ip_dvfs_coffe *dvfs_coffe);
	void (*get_dvfs_status)(struct gsp_dvfs *gsp, struct ip_dvfs_status *dvfs_status);

	/* coffe setting ops */
	void (*set_gfree_wait_delay)(struct gsp_dvfs *gsp, u32 para);
	void (*set_freq_upd_en_byp)(struct gsp_dvfs *gsp, bool enable);
	void (*set_freq_upd_delay_en)(struct gsp_dvfs *gsp, bool enable);
	void (*set_freq_upd_hdsk_en)(struct gsp_dvfs *gsp, bool enable);
	void (*set_dvfs_swtrig_en)(struct gsp_dvfs *gsp, bool enable);
};

struct sprd_gsp_dvfs_ops {
	const struct gsp_dvfs_ops *dvfs_ops;
};

extern const struct gsp_dvfs_ops qogirn6pro_gsp_dvfs_ops;
extern const struct gsp_dvfs_ops qogirn6lite_gsp_dvfs_ops;

#endif /* __SPRD_DVFS_GSP_H__ */
