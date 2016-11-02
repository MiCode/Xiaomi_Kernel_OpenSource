/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef __SDE_CORE_PERF_H__
#define __SDE_CORE_PERF_H__

#include <linux/types.h>
#include <linux/dcache.h>
#include <linux/mutex.h>

#include "sde_hw_catalog.h"
#include "sde_power_handle.h"

/**
 * struct sde_core_perf - definition of core performance context
 * @dev: Pointer to drm device
 * @debugfs_root: top level debug folder
 * @perf_lock: serialization lock for this context
 * @catalog: Pointer to catalog configuration
 * @phandle: Pointer to power handler
 * @pclient: Pointer to power client
 * @clk_name: core clock name
 * @core_clk: Pointer to core clock structure
 * @core_clk_rate: current core clock rate
 * @max_core_clk_rate: maximum allowable core clock rate
 */
struct sde_core_perf {
	struct drm_device *dev;
	struct dentry *debugfs_root;
	struct mutex perf_lock;
	struct sde_mdss_cfg *catalog;
	struct sde_power_handle *phandle;
	struct sde_power_client *pclient;
	char *clk_name;
	struct clk *core_clk;
	u32 core_clk_rate;
	u64 max_core_clk_rate;
};

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
 * @pclient: Pointer to power client
 * @clk_name: core clock name
 * @debugfs_parent: Pointer to parent debugfs
 */
int sde_core_perf_init(struct sde_core_perf *perf,
		struct drm_device *dev,
		struct sde_mdss_cfg *catalog,
		struct sde_power_handle *phandle,
		struct sde_power_client *pclient,
		char *clk_name,
		struct dentry *debugfs_parent);

#endif /* __SDE_CORE_PERF_H__ */
