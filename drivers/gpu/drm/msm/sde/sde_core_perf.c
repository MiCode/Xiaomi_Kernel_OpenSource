/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#include "sde_trace.h"
#include "sde_crtc.h"
#include "sde_rsc.h"
#include "sde_core_perf.h"

static struct sde_kms *_sde_crtc_get_kms(struct drm_crtc *crtc)
{
	struct msm_drm_private *priv;

	if (!crtc->dev || !crtc->dev->dev_private) {
		SDE_ERROR("invalid device\n");
		return NULL;
	}

	priv = crtc->dev->dev_private;
	if (!priv || !priv->kms) {
		SDE_ERROR("invalid kms\n");
		return NULL;
	}

	return to_sde_kms(priv->kms);
}

static bool _sde_core_perf_crtc_is_power_on(struct drm_crtc *crtc)
{
	return sde_crtc_is_enabled(crtc);
}

static bool _sde_core_video_mode_intf_connected(struct drm_crtc *crtc)
{
	struct drm_crtc *tmp_crtc;
	bool intf_connected = false;

	if (!crtc)
		goto end;

	drm_for_each_crtc(tmp_crtc, crtc->dev) {
		if ((sde_crtc_get_intf_mode(tmp_crtc) == INTF_MODE_VIDEO) &&
				_sde_core_perf_crtc_is_power_on(tmp_crtc)) {
			SDE_DEBUG("video interface connected crtc:%d\n",
				tmp_crtc->base.id);
			intf_connected = true;
			goto end;
		}
	}

end:
	return intf_connected;
}

int sde_core_perf_crtc_check(struct drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	u32 bw, threshold;
	u64 bw_sum_of_intfs = 0;
	enum sde_crtc_client_type curr_client_type;
	bool is_video_mode;
	struct sde_crtc_state *sde_cstate;
	struct drm_crtc *tmp_crtc;
	struct sde_kms *kms;

	if (!crtc || !state) {
		SDE_ERROR("invalid crtc\n");
		return -EINVAL;
	}

	kms = _sde_crtc_get_kms(crtc);
	if (!kms || !kms->catalog) {
		SDE_ERROR("invalid parameters\n");
		return 0;
	}

	/* we only need bandwidth check on real-time clients (interfaces) */
	if (sde_crtc_get_client_type(crtc) == NRT_CLIENT)
		return 0;

	sde_cstate = to_sde_crtc_state(state);

	bw_sum_of_intfs = sde_crtc_get_property(sde_cstate, CRTC_PROP_CORE_AB);
	curr_client_type = sde_crtc_get_client_type(crtc);

	drm_for_each_crtc(tmp_crtc, crtc->dev) {
		if (_sde_core_perf_crtc_is_power_on(tmp_crtc) &&
		    (sde_crtc_get_client_type(tmp_crtc) == curr_client_type) &&
		    (tmp_crtc != crtc)) {
			struct sde_crtc_state *tmp_cstate =
					to_sde_crtc_state(tmp_crtc->state);

			bw_sum_of_intfs += tmp_cstate->cur_perf.bw_ctl;
		}
	}

	/* convert bandwidth to kb */
	bw = DIV_ROUND_UP_ULL(bw_sum_of_intfs, 1000);
	SDE_DEBUG("calculated bandwidth=%uk\n", bw);

	is_video_mode = sde_crtc_get_intf_mode(crtc) == INTF_MODE_VIDEO;
	threshold = (is_video_mode ||
		_sde_core_video_mode_intf_connected(crtc)) ?
		kms->catalog->perf.max_bw_low : kms->catalog->perf.max_bw_high;

	SDE_DEBUG("final threshold bw limit = %d\n", threshold);

	if (!threshold) {
		sde_cstate->cur_perf.bw_ctl = 0;
		SDE_ERROR("no bandwidth limits specified\n");
		return -E2BIG;
	} else if (bw > threshold) {
		sde_cstate->cur_perf.bw_ctl = 0;
		SDE_ERROR("exceeds bandwidth: %ukb > %ukb\n", bw, threshold);
		return -E2BIG;
	}

	return 0;
}

static void _sde_core_perf_calc_crtc(struct sde_kms *kms,
		struct drm_crtc *crtc,
		struct sde_core_perf_params *perf)
{
	struct sde_crtc_state *sde_cstate;

	sde_cstate = to_sde_crtc_state(crtc->state);
	memset(perf, 0, sizeof(struct sde_core_perf_params));

	perf->bw_ctl = sde_crtc_get_property(sde_cstate, CRTC_PROP_CORE_AB);
	perf->max_per_pipe_ib =
			sde_crtc_get_property(sde_cstate, CRTC_PROP_CORE_IB);
	perf->core_clk_rate =
			sde_crtc_get_property(sde_cstate, CRTC_PROP_CORE_CLK);

	SDE_DEBUG("crtc=%d clk_rate=%u ib=%llu ab=%llu\n",
			crtc->base.id, perf->core_clk_rate,
			perf->max_per_pipe_ib, perf->bw_ctl);
}

static void _sde_core_perf_crtc_update_bus(struct sde_kms *kms,
		struct drm_crtc *crtc)
{
	u64 bw_sum_of_intfs = 0, bus_ab_quota, bus_ib_quota;
	struct sde_core_perf_params perf = {0};
	enum sde_crtc_client_type curr_client_type
					= sde_crtc_get_client_type(crtc);
	struct drm_crtc *tmp_crtc;
	struct sde_crtc_state *sde_cstate;
	struct msm_drm_private *priv = kms->dev->dev_private;

	drm_for_each_crtc(tmp_crtc, crtc->dev) {
		if (_sde_core_perf_crtc_is_power_on(tmp_crtc) &&
		    (curr_client_type == sde_crtc_get_client_type(tmp_crtc))) {
			sde_cstate = to_sde_crtc_state(tmp_crtc->state);

			perf.max_per_pipe_ib = max(perf.max_per_pipe_ib,
				sde_cstate->cur_perf.max_per_pipe_ib);

			bw_sum_of_intfs += sde_cstate->cur_perf.bw_ctl;

			SDE_DEBUG("crtc=%d bw=%llu\n",
				tmp_crtc->base.id,
				sde_cstate->cur_perf.bw_ctl);
		}
	}

	bus_ab_quota = max(bw_sum_of_intfs, kms->perf.perf_tune.min_bus_vote);
	bus_ib_quota = perf.max_per_pipe_ib;

	switch (curr_client_type) {
	case NRT_CLIENT:
		sde_power_data_bus_set_quota(&priv->phandle, kms->core_client,
				SDE_POWER_HANDLE_DATA_BUS_CLIENT_NRT,
				bus_ab_quota, bus_ib_quota);
		SDE_DEBUG("client:%s ab=%llu ib=%llu\n", "nrt",
				bus_ab_quota, bus_ib_quota);
		break;

	case RT_CLIENT:
		sde_power_data_bus_set_quota(&priv->phandle, kms->core_client,
				SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT,
				bus_ab_quota, bus_ib_quota);
		SDE_DEBUG("client:%s ab=%llu ib=%llu\n", "rt",
				bus_ab_quota, bus_ib_quota);
		break;

	case RT_RSC_CLIENT:
		sde_cstate = to_sde_crtc_state(crtc->state);
		sde_rsc_client_vote(sde_cstate->rsc_client, bus_ab_quota,
					bus_ib_quota);
		SDE_DEBUG("client:%s ab=%llu ib=%llu\n", "rt_rsc",
				bus_ab_quota, bus_ib_quota);
		break;

	default:
		SDE_ERROR("invalid client type:%d\n", curr_client_type);
		break;
	}
}

/**
 * @sde_core_perf_crtc_release_bw() - request zero bandwidth
 * @crtc - pointer to a crtc
 *
 * Function checks a state variable for the crtc, if all pending commit
 * requests are done, meaning no more bandwidth is needed, release
 * bandwidth request.
 */
void sde_core_perf_crtc_release_bw(struct drm_crtc *crtc)
{
	struct drm_crtc *tmp_crtc;
	struct sde_crtc_state *sde_cstate;
	struct sde_kms *kms;

	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	kms = _sde_crtc_get_kms(crtc);
	if (!kms || !kms->catalog) {
		SDE_ERROR("invalid kms\n");
		return;
	}

	sde_cstate = to_sde_crtc_state(crtc->state);

	/* only do this for command mode rt client (non-rsc client) */
	if ((sde_crtc_get_intf_mode(crtc) != INTF_MODE_CMD) &&
		(sde_crtc_get_client_type(crtc) != RT_RSC_CLIENT))
		return;

	/*
	 * If video interface present, cmd panel bandwidth cannot be
	 * released.
	 */
	if (sde_crtc_get_intf_mode(crtc) == INTF_MODE_CMD)
		drm_for_each_crtc(tmp_crtc, crtc->dev) {
			if (_sde_core_perf_crtc_is_power_on(tmp_crtc) &&
				sde_crtc_get_intf_mode(tmp_crtc) ==
						INTF_MODE_VIDEO)
				return;
		}

	/* Release the bandwidth */
	if (kms->perf.enable_bw_release) {
		trace_sde_cmd_release_bw(crtc->base.id);
		sde_cstate->cur_perf.bw_ctl = 0;
		sde_cstate->new_perf.bw_ctl = 0;
		SDE_DEBUG("Release BW crtc=%d\n", crtc->base.id);
		_sde_core_perf_crtc_update_bus(kms, crtc);
	}
}

static u32 _sde_core_perf_get_core_clk_rate(struct sde_kms *kms)
{
	u32 clk_rate = 0;
	struct drm_crtc *crtc;
	struct sde_crtc_state *sde_cstate;

	drm_for_each_crtc(crtc, kms->dev) {
		if (_sde_core_perf_crtc_is_power_on(crtc)) {
			sde_cstate = to_sde_crtc_state(crtc->state);
			clk_rate = max(sde_cstate->cur_perf.core_clk_rate,
							clk_rate);
			clk_rate = clk_round_rate(kms->perf.core_clk, clk_rate);
		}
	}

	SDE_DEBUG("clk:%u\n", clk_rate);

	return clk_rate;
}

void sde_core_perf_crtc_update(struct drm_crtc *crtc,
		int params_changed, bool stop_req)
{
	struct sde_core_perf_params *new, *old;
	int update_bus = 0, update_clk = 0;
	u32 clk_rate = 0;
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *sde_cstate;
	int ret;
	struct msm_drm_private *priv;
	struct sde_kms *kms;

	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	kms = _sde_crtc_get_kms(crtc);
	if (!kms || !kms->catalog) {
		SDE_ERROR("invalid kms\n");
		return;
	}
	priv = kms->dev->dev_private;

	sde_crtc = to_sde_crtc(crtc);
	sde_cstate = to_sde_crtc_state(crtc->state);

	SDE_DEBUG("crtc:%d stop_req:%d core_clk:%u\n",
			crtc->base.id, stop_req, kms->perf.core_clk_rate);

	old = &sde_cstate->cur_perf;
	new = &sde_cstate->new_perf;

	if (_sde_core_perf_crtc_is_power_on(crtc) && !stop_req) {
		if (params_changed)
			_sde_core_perf_calc_crtc(kms, crtc, new);

		/*
		 * cases for bus bandwidth update.
		 * 1. new bandwidth vote or writeback output vote
		 *    are higher than current vote for update request.
		 * 2. new bandwidth vote or writeback output vote are
		 *    lower than current vote at end of commit or stop.
		 */
		if ((params_changed && ((new->bw_ctl > old->bw_ctl))) ||
		    (!params_changed && ((new->bw_ctl < old->bw_ctl)))) {
			SDE_DEBUG("crtc=%d p=%d new_bw=%llu,old_bw=%llu\n",
				crtc->base.id, params_changed, new->bw_ctl,
				old->bw_ctl);
			old->bw_ctl = new->bw_ctl;
			old->max_per_pipe_ib = new->max_per_pipe_ib;
			update_bus = 1;
		}

		if ((params_changed &&
				(new->core_clk_rate > old->core_clk_rate)) ||
				(!params_changed &&
				(new->core_clk_rate < old->core_clk_rate))) {
			old->core_clk_rate = new->core_clk_rate;
			update_clk = 1;
		}
	} else {
		SDE_DEBUG("crtc=%d disable\n", crtc->base.id);
		memset(old, 0, sizeof(*old));
		memset(new, 0, sizeof(*new));
		update_bus = 1;
		update_clk = 1;
	}

	if (update_bus)
		_sde_core_perf_crtc_update_bus(kms, crtc);

	/*
	 * Update the clock after bandwidth vote to ensure
	 * bandwidth is available before clock rate is increased.
	 */
	if (update_clk) {
		clk_rate = _sde_core_perf_get_core_clk_rate(kms);

		SDE_EVT32(kms->dev, stop_req, clk_rate);
		ret = sde_power_clk_set_rate(&priv->phandle,
				kms->perf.clk_name, clk_rate);
		if (ret) {
			SDE_ERROR("failed to set %s clock rate %u\n",
					kms->perf.clk_name, clk_rate);
			return;
		}

		kms->perf.core_clk_rate = clk_rate;
		SDE_DEBUG("update clk rate = %d HZ\n", clk_rate);
	}
}

#ifdef CONFIG_DEBUG_FS

static ssize_t _sde_core_perf_mode_write(struct file *file,
		    const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct sde_core_perf *perf = file->private_data;
	struct sde_perf_cfg *cfg = &perf->catalog->perf;
	int perf_mode = 0;
	char buf[10];

	if (!perf)
		return -ENODEV;

	if (count >= sizeof(buf))
		return -EFAULT;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = 0;	/* end of string */

	if (kstrtoint(buf, 0, &perf_mode))
		return -EFAULT;

	if (perf_mode) {
		/* run the driver with max clk and BW vote */
		perf->perf_tune.min_core_clk = perf->max_core_clk_rate;
		perf->perf_tune.min_bus_vote =
				(u64) cfg->max_bw_high * 1000;
	} else {
		/* reset the perf tune params to 0 */
		perf->perf_tune.min_core_clk = 0;
		perf->perf_tune.min_bus_vote = 0;
	}
	return count;
}

static ssize_t _sde_core_perf_mode_read(struct file *file,
			char __user *buff, size_t count, loff_t *ppos)
{
	struct sde_core_perf *perf = file->private_data;
	int len = 0;
	char buf[40] = {'\0'};

	if (!perf)
		return -ENODEV;

	if (*ppos)
		return 0;	/* the end */

	len = snprintf(buf, sizeof(buf), "min_mdp_clk %lu min_bus_vote %llu\n",
			perf->perf_tune.min_core_clk,
			perf->perf_tune.min_bus_vote);
	if (len < 0 || len >= sizeof(buf))
		return 0;

	if ((count < sizeof(buf)) || copy_to_user(buff, buf, len))
		return -EFAULT;

	*ppos += len;   /* increase offset */

	return len;
}

static const struct file_operations sde_core_perf_mode_fops = {
	.open = simple_open,
	.read = _sde_core_perf_mode_read,
	.write = _sde_core_perf_mode_write,
};

static void sde_debugfs_core_perf_destroy(struct sde_core_perf *perf)
{
	debugfs_remove_recursive(perf->debugfs_root);
	perf->debugfs_root = NULL;
}

static int sde_debugfs_core_perf_init(struct sde_core_perf *perf,
		struct dentry *parent)
{
	struct sde_mdss_cfg *catalog = perf->catalog;
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
	debugfs_create_u32("enable_bw_release", 0644, perf->debugfs_root,
			(u32 *)&perf->enable_bw_release);
	debugfs_create_u32("threshold_low", 0644, perf->debugfs_root,
			(u32 *)&catalog->perf.max_bw_low);
	debugfs_create_u32("threshold_high", 0644, perf->debugfs_root,
			(u32 *)&catalog->perf.max_bw_high);
	debugfs_create_file("perf_mode", 0644, perf->debugfs_root,
			(u32 *)perf, &sde_core_perf_mode_fops);

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
