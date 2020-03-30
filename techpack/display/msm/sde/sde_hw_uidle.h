/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 *
 */

#ifndef _SDE_HW_UIDLE_H
#define _SDE_HW_UIDLE_H

#include "sde_hw_catalog.h"
#include "sde_hw_mdss.h"
#include "sde_hw_util.h"

struct sde_hw_uidle;

#define FAL10_DANGER_SHFT 0
#define FAL10_EXIT_DANGER_SHFT 4
#define FAL10_EXIT_CNT_SHFT 16

#define FAL10_DANGER_MSK GENMASK(2, FAL10_DANGER_SHFT)
#define FAL10_EXIT_DANGER_MSK GENMASK(6, FAL10_EXIT_DANGER_SHFT)
#define FAL10_EXIT_CNT_MSK GENMASK(23, FAL10_EXIT_CNT_SHFT)

#define SDE_UIDLE_WD_GRANULARITY 1
#define SDE_UIDLE_WD_HEART_BEAT 0
#define SDE_UIDLE_WD_LOAD_VAL 3

struct sde_uidle_ctl_cfg {
	u32 fal10_exit_cnt;
	u32 fal10_exit_danger;
	u32 fal10_danger;
	bool uidle_enable;
};

struct sde_uidle_wd_cfg {
	u32 granularity;
	u32 heart_beat;
	u32 load_value;
	bool clear;
	bool enable;
};

struct sde_uidle_cntr {
	u32 fal1_gate_cntr;
	u32 fal10_gate_cntr;
	u32 fal_wait_gate_cntr;
	u32 fal1_num_transitions_cntr;
	u32 fal10_num_transitions_cntr;
	u32 min_gate_cntr;
	u32 max_gate_cntr;
};

struct sde_uidle_status {
	u32 uidle_danger_status_0;
	u32 uidle_danger_status_1;
	u32 uidle_safe_status_0;
	u32 uidle_safe_status_1;
	u32 uidle_idle_status_0;
	u32 uidle_idle_status_1;
	u32 uidle_fal_status_0;
	u32 uidle_fal_status_1;
	u32 uidle_status;
	u32 uidle_en_fal10;
};

struct sde_hw_uidle_ops {
	/**
	 * set_uidle_ctl - set uidle global config
	 * @uidle: uidle context driver
	 * @cfg: uidle global config
	 */
	void (*set_uidle_ctl)(struct sde_hw_uidle *uidle,
		struct sde_uidle_ctl_cfg *cfg);

	/**
	 * setup_wd_timer - set uidle watchdog timer
	 * @uidle: uidle context driver
	 * @cfg: uidle wd timer config
	 */
	void (*setup_wd_timer)(struct sde_hw_uidle *uidle,
		struct sde_uidle_wd_cfg *cfg);

	/**
	 * uidle_setup_cntr - set uidle perf counters
	 * @uidle: uidle context driver
	 * @enable: true to enable the counters
	 */
	void (*uidle_setup_cntr)(struct sde_hw_uidle *uidle,
		bool enable);

	/**
	 * uidle_get_cntr - get uidle perf counters
	 * @uidle: uidle context driver
	 * @cntr: pointer to return the counters
	 */
	void (*uidle_get_cntr)(struct sde_hw_uidle *uidle,
		struct sde_uidle_cntr *cntr);

	/**
	 * uidle_get_status - get uidle status
	 * @uidle: uidle context driver
	 * @status: pointer to return the status of uidle
	 */
	void (*uidle_get_status)(struct sde_hw_uidle *uidle,
		struct sde_uidle_status *status);

};

struct sde_hw_uidle {
	/* base */
	struct sde_hw_blk_reg_map hw;

	/* uidle */
	const struct sde_uidle_cfg *cap;

	/* ops */
	struct sde_hw_uidle_ops ops;

	/*
	 * uidle is common across all displays, lock to serialize access.
	 * must be taken by client before using any ops
	 */
	struct mutex uidle_lock;

	enum sde_uidle idx;
};

struct sde_hw_uidle *sde_hw_uidle_init(enum sde_uidle idx,
		void __iomem *addr, unsigned long len,
		struct sde_mdss_cfg *m);

#endif /*_SDE_HW_UIDLE_H */
