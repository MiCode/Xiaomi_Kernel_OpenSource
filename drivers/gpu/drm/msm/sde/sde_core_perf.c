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

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__

#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/sort.h>
#include <linux/clk.h>
#include <linux/bitmap.h>

#include "msm_prop.h"

#include "sde_kms.h"
#include "sde_fence.h"
#include "sde_formats.h"
#include "sde_hw_sspp.h"
#include "sde_trace.h"
#include "sde_crtc.h"
#include "sde_plane.h"
#include "sde_encoder.h"
#include "sde_wb.h"
#include "sde_core_perf.h"
#include "sde_trace.h"

#ifdef CONFIG_DEBUG_FS

static void sde_debugfs_core_perf_destroy(struct sde_core_perf *perf)
{
	debugfs_remove_recursive(perf->debugfs_root);
	perf->debugfs_root = NULL;
}

static int sde_debugfs_core_perf_init(struct sde_core_perf *perf,
		struct dentry *parent)
{
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;

	priv = perf->dev->dev_private;
	if (!priv || !priv->kms) {
		SDE_ERROR("invalid KMS reference\n");
		return -EINVAL;
	}

	sde_kms = to_sde_kms(priv->kms);

	perf->debugfs_root = debugfs_create_dir("core_perf", parent);
	if (!perf->debugfs_root) {
		SDE_ERROR("failed to create core perf debugfs\n");
		return -EINVAL;
	}

	debugfs_create_u64("max_core_clk_rate", 0644, perf->debugfs_root,
			&perf->max_core_clk_rate);
	debugfs_create_u32("core_clk_rate", 0644, perf->debugfs_root,
			&perf->core_clk_rate);

	return 0;
}
#else
static void sde_debugfs_core_perf_destroy(struct sde_core_perf *perf)
{
}

static int sde_debugfs_core_perf_init(struct sde_core_perf *perf,
		struct dentry *parent)
{
	return 0;
}
#endif

void sde_core_perf_destroy(struct sde_core_perf *perf)
{
	if (!perf) {
		SDE_ERROR("invalid parameters\n");
		return;
	}

	sde_debugfs_core_perf_destroy(perf);
	perf->max_core_clk_rate = 0;
	perf->core_clk = NULL;
	mutex_destroy(&perf->perf_lock);
	perf->clk_name = NULL;
	perf->phandle = NULL;
	perf->catalog = NULL;
	perf->dev = NULL;
}

int sde_core_perf_init(struct sde_core_perf *perf,
		struct drm_device *dev,
		struct sde_mdss_cfg *catalog,
		struct sde_power_handle *phandle,
		struct sde_power_client *pclient,
		char *clk_name,
		struct dentry *debugfs_parent)
{
	if (!perf || !catalog || !phandle || !pclient ||
			!clk_name || !debugfs_parent) {
		SDE_ERROR("invalid parameters\n");
		return -EINVAL;
	}

	perf->dev = dev;
	perf->catalog = catalog;
	perf->phandle = phandle;
	perf->pclient = pclient;
	perf->clk_name = clk_name;
	mutex_init(&perf->perf_lock);

	perf->core_clk = sde_power_clk_get_clk(phandle, clk_name);
	if (!perf->core_clk) {
		SDE_ERROR("invalid core clk\n");
		goto err;
	}

	perf->max_core_clk_rate = sde_power_clk_get_max_rate(phandle, clk_name);
	if (!perf->max_core_clk_rate) {
		SDE_ERROR("invalid max core clk rate\n");
		goto err;
	}

	sde_debugfs_core_perf_init(perf, debugfs_parent);

	return 0;

err:
	sde_core_perf_destroy(perf);
	return -ENODEV;
}
