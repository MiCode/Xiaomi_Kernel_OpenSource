/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _SDE_CORE_PERF_H_
#define _SDE_CORE_PERF_H_

#include <linux/types.h>
#include <linux/dcache.h>
#include <linux/mutex.h>
#include <drm/drm_crtc.h>

#include "sde_hw_catalog.h"
#include "sde_power_handle.h"

#define	SDE_PERF_DEFAULT_MAX_CORE_CLK_RATE	320000000

/**
 *  uidle performance counters mode
 * @SDE_PERF_UIDLE_DISABLE: Disable logging (default)
 * @SDE_PERF_UIDLE_CNT: Enable logging of uidle performance counters
 * @SDE_PERF_UIDLE_STATUS: Enable logging of uidle status
 * @SDE_PERF_UIDLE_MAX: Max available mode
 */
#define SDE_PERF_UIDLE_DISABLE 0x0
#define SDE_PERF_UIDLE_CNT BIT(0)
#define SDE_PERF_UIDLE_STATUS BIT(1)
#define SDE_PERF_UIDLE_MAX BIT(2)

/**
 * struct sde_core_perf_params - definition of performance parameters
 * @max_per_pipe_ib: maximum instantaneous bandwidth request
 * @bw_ctl: arbitrated bandwidth request
 * @core_clk_rate: core clock rate request
 * @llcc_active: request to activate/deactivate the llcc
 */
struct sde_core_perf_params {
	u64 max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_MAX];
	u64 bw_ctl[SDE_POWER_HANDLE_DBUS_ID_MAX];
	u64 core_clk_rate;
	bool llcc_active;
};

/**
 * struct sde_core_perf_tune - definition of performance tuning control
 * @mode: performance mode
 * @min_core_clk: minimum core clock
 * @min_bus_vote: minimum bus vote
 * @mode_changed: indicate if clock tuning strategy changed
 */
struct sde_core_perf_tune {
	u32 mode;
	u64 min_core_clk;
	u64 min_bus_vote;
	bool mode_changed;
};

/**
 * struct sde_core_perf - definition of core performance context
 * @dev: Pointer to drm device
 * @debugfs_root: top level debug folder
 * @catalog: Pointer to catalog configuration
 * @phandle: Pointer to power handler
 * @clk_name: core clock name
 * @core_clk: Pointer to core clock structure
 * @core_clk_rate: current core clock rate
 * @max_core_clk_rate: maximum allowable core clock rate
 * @perf_tune: debug control for performance tuning
 * @enable_bw_release: debug control for bandwidth release
 * @fix_core_clk_rate: fixed core clock request in Hz used in mode 2
 * @fix_core_ib_vote: fixed core ib vote in bps used in mode 2
 * @fix_core_ab_vote: fixed core ab vote in bps used in mode 2
 * @bw_vote_mode: apps rsc vs display rsc bandwidth vote mode
 * @sde_rsc_available: is display rsc available
 * @bw_vote_mode_updated: bandwidth vote mode update
 * @llcc_active: status of the llcc, true if active.
 * @uidle_enabled: indicates if uidle is already enabled
 */
struct sde_core_perf {
	struct drm_device *dev;
	struct dentry *debugfs_root;
	struct sde_mdss_cfg *catalog;
	struct sde_power_handle *phandle;
	char *clk_name;
	struct clk *core_clk;
	u64 core_clk_rate;
	u64 max_core_clk_rate;
	struct sde_core_perf_tune perf_tune;
	u32 enable_bw_release;
	u64 fix_core_clk_rate;
	u64 fix_core_ib_vote;
	u64 fix_core_ab_vote;
	u32 bw_vote_mode;
	bool sde_rsc_available;
	bool bw_vote_mode_updated;
	bool llcc_active;
	bool uidle_enabled;
};

/**
 * sde_core_perf_crtc_check - validate performance of the given crtc state
 * @crtc: Pointer to crtc
 * @state: Pointer to new crtc state
 * return: zero if success, or error code otherwise
 */
int sde_core_perf_crtc_check(struct drm_crtc *crtc,
		struct drm_crtc_state *state);

/**
 * sde_core_perf_crtc_update - update performance of the given crtc
 * @crtc: Pointer to crtc
 * @params_changed: true if crtc parameters are modified
 * @stop_req: true if this is a stop request
 */
void sde_core_perf_crtc_update(struct drm_crtc *crtc,
		int params_changed, bool stop_req);

/**
 * sde_core_perf_crtc_release_bw - release bandwidth of the given crtc
 * @crtc: Pointer to crtc
 */
void sde_core_perf_crtc_release_bw(struct drm_crtc *crtc);

/**
 * sde_core_perf_crtc_update_uidle - attempts to enable uidle of the given crtc
 * @crtc: Pointer to crtc
 * @enable: enable/disable uidle
 */
void sde_core_perf_crtc_update_uidle(struct drm_crtc *crtc, bool enable);

/**
 * sde_core_perf_destroy - destroy the given core performance context
 * @perf: Pointer to core performance context
 */
void sde_core_perf_destroy(struct sde_core_perf *perf);

/**
 * sde_core_perf_init - initialize the given core performance context
 * @perf: Pointer to core performance context
 * @dev: Pointer to drm device
 * @catalog: Pointer to catalog
 * @phandle: Pointer to power handle
 * @clk_name: core clock name
 */
int sde_core_perf_init(struct sde_core_perf *perf,
		struct drm_device *dev,
		struct sde_mdss_cfg *catalog,
		struct sde_power_handle *phandle,
		char *clk_name);

/**
 * sde_core_perf_debugfs_init - initialize debugfs for core performance context
 * @perf: Pointer to core performance context
 * @debugfs_parent: Pointer to parent debugfs
 */
int sde_core_perf_debugfs_init(struct sde_core_perf *perf,
		struct dentry *parent);

#endif /* _SDE_CORE_PERF_H_ */
