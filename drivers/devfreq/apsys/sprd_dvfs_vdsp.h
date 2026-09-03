// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef __SPRD_DVFS_VDSP_H__
#define __SPRD_DVFS_VDSP_H__

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
	VOLT80, //0.80v
} voltage_level;

typedef enum {
	EDAP_DIV_0 = 0,
	EDAP_DIV_1,
	EDAP_DIV_2,
	EDAP_DIV_3,
} edap_div_level;

typedef enum {
	M0_DIV_0 = 0,
	M0_DIV_1,
	M0_DIV_2,
	M0_DIV_3,
} m0_div_level;

typedef enum {
	VDSP_CLK_INDEX_192M = 0,
	VDSP_CLK_INDEX_307M2,
	VDSP_CLK_INDEX_468M,
	VDSP_CLK_INDEX_614M4,
	VDSP_CLK_INDEX_702M,
	VDSP_CLK_INDEX_768M,
} clock_level;

typedef enum {
	VDSP_CLK192M = 192000000,
	VDSP_CLK307M2 = 307200000,
	VDSP_CLK468M = 468000000,
	VDSP_CLK614M4 = 614400000,
	VDSP_CLK702M = 702000000,
	VDSP_CLK768M = 768000000,
} clock_rate;

struct vdsp_dvfs_map_cfg {
	u32 map_index;
	u32 volt_level;
	u32 clk_level;
	u32 clk_rate;
	u32 edap_div;
	u32 m0_div;
};

struct vdsp_dvfs {
	int dvfs_enable;
	struct device dev;

	struct devfreq *devfreq;
	struct opp_table *opp_table;
	struct devfreq_event_dev *edev;
	struct notifier_block vdsp_dvfs_nb;

	u32 work_freq;
	u32 idle_freq;
	set_freq_type freq_type;

	struct ip_dvfs_coffe dvfs_coffe;
	struct ip_dvfs_status dvfs_status;
	const struct vdsp_dvfs_ops *dvfs_ops;

	struct apsys_dev *apsys;
};

struct vdsp_dvfs_ops {
	/* initialization interface */
	int (*parse_dt)(struct vdsp_dvfs *vdsp, struct device_node *np);
	int (*parse_pll)(struct vdsp_dvfs *vdsp, struct device *dev);
	int (*dvfs_init)(struct vdsp_dvfs *vdsp);
	void (*hw_dfs_en)(struct vdsp_dvfs *vdsp, bool dfs_en);

	/* work-idle dvfs index ops */
	int  (*set_work_index)(struct vdsp_dvfs *vdsp, int index);
	int  (*get_work_index)(struct vdsp_dvfs *vdsp);
	void  (*set_idle_index)(struct vdsp_dvfs *vdsp, int index);
	int  (*get_idle_index)(struct vdsp_dvfs *vdsp);

	/* work-idle dvfs freq ops */
	int (*set_work_freq)(struct vdsp_dvfs *vdsp, u32 freq);
	u32 (*get_work_freq)(struct vdsp_dvfs *vdsp);
	void (*set_idle_freq)(struct vdsp_dvfs *vdsp, u32 freq);
	u32 (*get_idle_freq)(struct vdsp_dvfs *vdsp);

	/* work-idle dvfs map ops */
	int  (*get_dvfs_table)(struct ip_dvfs_map_cfg *dvfs_table);
	void (*set_dvfs_table)(struct ip_dvfs_map_cfg *dvfs_table);
	void (*get_dvfs_coffe)(struct ip_dvfs_coffe *dvfs_coffe);
	void (*set_dvfs_coffe)(struct ip_dvfs_coffe *dvfs_coffe);
	void (*get_dvfs_status)(struct vdsp_dvfs *vdsp, struct ip_dvfs_status *dvfs_status);

	/* coffe setting ops */
	void (*set_gfree_wait_delay)(struct vdsp_dvfs *vdsp, u32 para);
	void (*set_freq_upd_en_byp)(struct vdsp_dvfs *vdsp, bool enable);
	void (*set_freq_upd_delay_en)(struct vdsp_dvfs *vdsp, bool enable);
	void (*set_freq_upd_hdsk_en)(struct vdsp_dvfs *vdsp, bool enable);
	void (*set_dvfs_swtrig_en)(struct vdsp_dvfs *vdsp, bool enable);
};

struct sprd_vdsp_dvfs_ops {
	const struct vdsp_dvfs_ops *dvfs_ops;
};

extern const struct vdsp_dvfs_ops sharkl5pro_vdsp_dvfs_ops;

#endif /* __SPRD_DVFS_VDSP_H__ */
