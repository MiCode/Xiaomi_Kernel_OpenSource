// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _VSP_DVFS_H
#define _VSP_DVFS_H

#include <linux/apsys_dvfs.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/devfreq.h>
#include <linux/devfreq-event.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "../governor.h"
#include "sprd_dvfs_apsys.h"

struct vsp_dvfs {
	struct device dev;
	unsigned long freq, target_freq;
	unsigned long volt, target_volt;
	unsigned long user_freq;
	u32 work_freq;
	u32 idle_freq;
	struct devfreq *devfreq;
	struct devfreq_event_dev *edev;
	const struct ip_dvfs_ops  *dvfs_ops;
	struct ip_dvfs_status ip_status;
	struct ip_dvfs_coffe ip_coeff;
	set_freq_type freq_type;
	struct notifier_block vsp_dvfs_nb;
	u32 max_freq_level;
	struct apsys_dev *apsys;
};

struct ip_dvfs_ops {
	/* userspace interface */
	void (*parse_dt)(struct vsp_dvfs *vsp, struct device_node *np);
	int (*dvfs_init)(struct vsp_dvfs *vsp);
	void (*hw_dvfs_en)(struct vsp_dvfs *vsp, u32 dvfs_eb);

	void (*set_work_freq)(struct vsp_dvfs *vsp, u32 work_freq);
	u32 (*get_work_freq)(struct vsp_dvfs *vsp);
	void (*set_idle_freq)(struct vsp_dvfs *vsp, u32 idle_freq);
	u32 (*get_idle_freq)(struct vsp_dvfs *vsp);

	/* work-idle dvfs map ops */
	void (*get_dvfs_table)(struct ip_dvfs_map_cfg *dvfs_table);
	void (*get_dvfs_status)(struct vsp_dvfs *vsp, struct ip_dvfs_status *ip_status);

	/* coffe setting ops */
	void (*set_gfree_wait_delay)(struct vsp_dvfs *vsp, u32 wind_para);
	void (*set_freq_upd_en_byp)(struct vsp_dvfs *vsp, u32 on);
	void (*set_freq_upd_delay_en)(struct vsp_dvfs *vsp, u32 on);
	void (*set_freq_upd_hdsk_en)(struct vsp_dvfs *vsp, u32 on);
	void (*set_dvfs_swtrig_en)(struct vsp_dvfs *vsp, u32 en);

	/* work-idle dvfs index ops */
	void (*set_work_index)(struct vsp_dvfs *vsp, u32 index);
	u32 (*get_work_index)(struct vsp_dvfs *vsp);
	void (*set_idle_index)(struct vsp_dvfs *vsp, u32 index);
	u32 (*get_idle_index)(struct vsp_dvfs *vsp);
	void (*updata_target_freq)(struct vsp_dvfs *vsp, u32 freq, set_freq_type freq_type);
};

typedef enum {
	VOLT70 = 0, //0.7
	VOLT75, //0.75
	VOLT80, //0.8
} voltage_level;

typedef enum {
	VSP_CLK256 = 256000000, //256
	VSP_CLK307 = 307200000, //307
	VSP_CLK384 = 384000000, //384
	VSP_CLK512 = 512000000, //512
	VSP_CLK614 = 614400000, //614
	VSP_CLK680 = 668250000, //680
} vsp_clock_freq;

typedef enum {
	VSP_CLK_INDEX_256 = 0,
	VSP_CLK_INDEX_307 = 1,
	VSP_CLK_INDEX_384 = 2,
	VSP_CLK_INDEX_512 = 3,
	VSP_CLK_INDEX_614 = 4,
	VSP_CLK_INDEX_680 = 5,//4?
} vsp_clock_level;

#define MAX_FREQ_LEVEL 8

extern const struct ip_dvfs_ops qogirn6lite_vpudec_vsp_dvfs_ops;
extern const struct ip_dvfs_ops qogirn6lite_vpuenc_vsp_dvfs_ops;
extern const struct ip_dvfs_ops qogirn6pro_vpudec_vsp_dvfs_ops;
extern const struct ip_dvfs_ops qogirn6pro_vpuenc_vsp_dvfs_ops;
extern const struct ip_dvfs_ops qogirl6_vsp_dvfs_ops;
extern const struct ip_dvfs_ops sharkl5pro_vsp_dvfs_ops;
extern const struct ip_dvfs_ops sharkl5_vsp_dvfs_ops;
extern const struct ip_dvfs_ops roc1_vsp_dvfs_ops;
#endif
