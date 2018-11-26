/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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
#include <linux/sde_rsc.h>

#include "msm_prop.h"

#include "sde_kms.h"
#include "sde_trace.h"
#include "sde_crtc.h"
#include "sde_core_perf.h"

#define SDE_PERF_MODE_STRING_SIZE	128

static DEFINE_MUTEX(sde_core_perf_lock);

/**
 * enum sde_perf_mode - performance tuning mode
 * @SDE_PERF_MODE_NORMAL: performance controlled by user mode client
 * @SDE_PERF_MODE_MINIMUM: performance bounded by minimum setting
 * @SDE_PERF_MODE_FIXED: performance bounded by fixed setting
 */
enum sde_perf_mode {
	SDE_PERF_MODE_NORMAL,
	SDE_PERF_MODE_MINIMUM,
	SDE_PERF_MODE_FIXED,
	SDE_PERF_MODE_MAX
};

/**
 * enum sde_perf_vote_mode: perf vote mode.
 * @APPS_RSC_MODE:	It combines the vote for all displays and votes it
 *                      through APPS rsc. This is default mode when display
 *                      rsc is not available.
 * @DISP_RSC_MODE:	It combines the vote for all displays and votes it
 *                      through display rsc. This is default configuration
 *                      when display rsc is available.
 * @DISP_RSC_PRIMARY_MODE:	The primary display votes through display rsc
 *                      while all other displays votes through apps rsc.
 */
enum sde_perf_vote_mode {
	APPS_RSC_MODE,
	DISP_RSC_MODE,
	DISP_RSC_PRIMARY_MODE,
};

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

static void _sde_core_perf_calc_crtc(struct sde_kms *kms,
		struct drm_crtc *crtc,
		struct drm_crtc_state *state,
		struct sde_core_perf_params *perf)
{
	struct sde_crtc_state *sde_cstate;
	int i;

	if (!kms || !kms->catalog || !crtc || !state || !perf) {
		SDE_ERROR("invalid parameters\n");
		return;
	}

	sde_cstate = to_sde_crtc_state(state);
	memset(perf, 0, sizeof(struct sde_core_perf_params));

	perf->bw_ctl[SDE_POWER_HANDLE_DBUS_ID_MNOC] =
		sde_crtc_get_property(sde_cstate, CRTC_PROP_CORE_AB);
	perf->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_MNOC] =
		sde_crtc_get_property(sde_cstate, CRTC_PROP_CORE_IB);

	if (sde_cstate->bw_split_vote) {
		perf->bw_ctl[SDE_POWER_HANDLE_DBUS_ID_LLCC] =
			sde_crtc_get_property(sde_cstate, CRTC_PROP_LLCC_AB);
		perf->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_LLCC] =
			sde_crtc_get_property(sde_cstate, CRTC_PROP_LLCC_IB);
		perf->bw_ctl[SDE_POWER_HANDLE_DBUS_ID_EBI] =
			sde_crtc_get_property(sde_cstate, CRTC_PROP_DRAM_AB);
		perf->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_EBI] =
			sde_crtc_get_property(sde_cstate, CRTC_PROP_DRAM_IB);
	} else {
		perf->bw_ctl[SDE_POWER_HANDLE_DBUS_ID_LLCC] =
			sde_crtc_get_property(sde_cstate, CRTC_PROP_CORE_AB);
		perf->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_LLCC] =
			sde_crtc_get_property(sde_cstate, CRTC_PROP_CORE_IB);
		perf->bw_ctl[SDE_POWER_HANDLE_DBUS_ID_EBI] =
			sde_crtc_get_property(sde_cstate, CRTC_PROP_CORE_AB);
		perf->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_EBI] =
			sde_crtc_get_property(sde_cstate, CRTC_PROP_CORE_IB);
	}

	perf->core_clk_rate =
			sde_crtc_get_property(sde_cstate, CRTC_PROP_CORE_CLK);

	if (!sde_cstate->bw_control) {
		for (i = 0; i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++) {
			perf->bw_ctl[i] = kms->catalog->perf.max_bw_high *
					1000ULL;
			perf->max_per_pipe_ib[i] = perf->bw_ctl[i];
		}
		perf->core_clk_rate = kms->perf.max_core_clk_rate;
	} else if (kms->perf.perf_tune.mode == SDE_PERF_MODE_MINIMUM) {
		for (i = 0; i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++) {
			perf->bw_ctl[i] = 0;
			perf->max_per_pipe_ib[i] = 0;
		}
		perf->core_clk_rate = 0;
	} else if (kms->perf.perf_tune.mode == SDE_PERF_MODE_FIXED) {
		for (i = 0; i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++) {
			perf->bw_ctl[i] = kms->perf.fix_core_ab_vote;
			perf->max_per_pipe_ib[i] = kms->perf.fix_core_ib_vote;
		}
		perf->core_clk_rate = kms->perf.fix_core_clk_rate;
	}

	trace_sde_perf_calc_crtc(crtc->base.id,
			perf->bw_ctl[SDE_POWER_HANDLE_DBUS_ID_MNOC],
			perf->bw_ctl[SDE_POWER_HANDLE_DBUS_ID_LLCC],
			perf->bw_ctl[SDE_POWER_HANDLE_DBUS_ID_EBI],
			perf->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_MNOC],
			perf->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_LLCC],
			perf->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_EBI],
			perf->core_clk_rate);

	SDE_EVT32(crtc->base.id, perf->core_clk_rate);
	trace_sde_perf_calc_crtc(crtc->base.id,
			perf->bw_ctl[SDE_POWER_HANDLE_DBUS_ID_MNOC],
			perf->bw_ctl[SDE_POWER_HANDLE_DBUS_ID_LLCC],
			perf->bw_ctl[SDE_POWER_HANDLE_DBUS_ID_EBI],
			perf->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_MNOC],
			perf->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_LLCC],
			perf->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_EBI],
			perf->core_clk_rate);

	SDE_DEBUG(
		"crtc=%d clk_rate=%llu core_ib=%llu core_ab=%llu llcc_ib=%llu llcc_ab=%llu mem_ib=%llu mem_ab=%llu\n",
			crtc->base.id, perf->core_clk_rate,
			perf->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_MNOC],
			perf->bw_ctl[SDE_POWER_HANDLE_DBUS_ID_MNOC],
			perf->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_LLCC],
			perf->bw_ctl[SDE_POWER_HANDLE_DBUS_ID_LLCC],
			perf->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_EBI],
			perf->bw_ctl[SDE_POWER_HANDLE_DBUS_ID_EBI]);
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
	int i;

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

	/* obtain new values */
	_sde_core_perf_calc_crtc(kms, crtc, state, &sde_cstate->new_perf);

	for (i = SDE_POWER_HANDLE_DBUS_ID_MNOC;
			i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++) {
		bw_sum_of_intfs = sde_cstate->new_perf.bw_ctl[i];
		curr_client_type = sde_crtc_get_client_type(crtc);

		drm_for_each_crtc(tmp_crtc, crtc->dev) {
			if (_sde_core_perf_crtc_is_power_on(tmp_crtc) &&
			    (sde_crtc_get_client_type(tmp_crtc) ==
					    curr_client_type) &&
			    (tmp_crtc != crtc)) {
				struct sde_crtc_state *tmp_cstate =
					to_sde_crtc_state(tmp_crtc->state);

				SDE_DEBUG("crtc:%d bw:%llu ctrl:%d\n",
					tmp_crtc->base.id,
					tmp_cstate->new_perf.bw_ctl[i],
					tmp_cstate->bw_control);
				/*
				 * For bw check only use the bw if the
				 * atomic property has been already set
				 */
				if (tmp_cstate->bw_control)
					bw_sum_of_intfs +=
						tmp_cstate->new_perf.bw_ctl[i];
			}
		}

		/* convert bandwidth to kb */
		bw = DIV_ROUND_UP_ULL(bw_sum_of_intfs, 1000);
		SDE_DEBUG("calculated bandwidth=%uk\n", bw);

		is_video_mode = sde_crtc_get_intf_mode(crtc) == INTF_MODE_VIDEO;
		threshold = (is_video_mode ||
			_sde_core_video_mode_intf_connected(crtc)) ?
			kms->catalog->perf.max_bw_low :
			kms->catalog->perf.max_bw_high;

		SDE_DEBUG("final threshold bw limit = %d\n", threshold);

		if (!sde_cstate->bw_control) {
			SDE_DEBUG("bypass bandwidth check\n");
		} else if (!threshold) {
			SDE_ERROR("no bandwidth limits specified\n");
			return -E2BIG;
		} else if (bw > threshold) {
			SDE_ERROR("exceeds bandwidth: %ukb > %ukb\n", bw,
					threshold);
			return -E2BIG;
		}
	}

	return 0;
}

static inline bool _is_crtc_client_type_matches(struct drm_crtc *tmp_crtc,
	enum sde_crtc_client_type curr_client_type,
	struct sde_core_perf *perf)
{
	if (!tmp_crtc)
		return false;
	else if (perf->bw_vote_mode == DISP_RSC_PRIMARY_MODE &&
							perf->sde_rsc_available)
		return curr_client_type == sde_crtc_get_client_type(tmp_crtc);
	else
		return true;
}

static inline enum sde_crtc_client_type _get_sde_client_type(
	enum sde_crtc_client_type curr_client_type,
	struct sde_core_perf *perf)
{
	if (perf->bw_vote_mode == DISP_RSC_PRIMARY_MODE &&
						perf->sde_rsc_available)
		return curr_client_type;
	else if (perf->bw_vote_mode != APPS_RSC_MODE && perf->sde_rsc_available)
		return RT_RSC_CLIENT;
	else
		return RT_CLIENT;
}

static void _sde_core_perf_crtc_update_bus(struct sde_kms *kms,
		struct drm_crtc *crtc, u32 bus_id)
{
	u64 bw_sum_of_intfs = 0, bus_ab_quota, bus_ib_quota;
	struct sde_core_perf_params perf = { { 0 } };
	enum sde_crtc_client_type client_vote, curr_client_type
					= sde_crtc_get_client_type(crtc);
	struct drm_crtc *tmp_crtc;
	struct sde_crtc_state *sde_cstate;
	struct msm_drm_private *priv = kms->dev->dev_private;
	struct sde_crtc *sde_crtc;

	u64 tmp_max_per_pipe_ib;
	u64 tmp_bw_ctl;

	drm_for_each_crtc(tmp_crtc, crtc->dev) {
		if (_sde_core_perf_crtc_is_power_on(tmp_crtc) &&
		    _is_crtc_client_type_matches(tmp_crtc, curr_client_type,
								&kms->perf)) {

			/* use current perf, which are the values voted */
			sde_crtc = to_sde_crtc(tmp_crtc);
			tmp_max_per_pipe_ib =
			  sde_crtc->cur_perf.max_per_pipe_ib[bus_id];
			tmp_bw_ctl =
			  sde_crtc->cur_perf.bw_ctl[bus_id];

			perf.max_per_pipe_ib[bus_id] =
				max(perf.max_per_pipe_ib[bus_id],
				tmp_max_per_pipe_ib);

			bw_sum_of_intfs += tmp_bw_ctl;

			SDE_DEBUG("crtc=%d bus_id=%d bw=%llu perf_pipe:%llu\n",
				tmp_crtc->base.id, bus_id,
				tmp_bw_ctl, tmp_max_per_pipe_ib);
		}
	}

	bus_ab_quota = max(bw_sum_of_intfs, kms->perf.perf_tune.min_bus_vote);
	bus_ib_quota = perf.max_per_pipe_ib[bus_id];

	if (kms->perf.perf_tune.mode == SDE_PERF_MODE_FIXED) {
		bus_ab_quota = kms->perf.fix_core_ab_vote;
		bus_ib_quota = kms->perf.fix_core_ib_vote;
	}

	client_vote = _get_sde_client_type(curr_client_type, &kms->perf);
	switch (client_vote) {
	case NRT_CLIENT:
		sde_power_data_bus_set_quota(&priv->phandle, kms->core_client,
				SDE_POWER_HANDLE_DATA_BUS_CLIENT_NRT,
				bus_id, bus_ab_quota, bus_ib_quota);
		SDE_DEBUG("client:%s bus_id=%d ab=%llu ib=%llu\n", "nrt",
				bus_id, bus_ab_quota, bus_ib_quota);
		break;

	case RT_CLIENT:
		sde_power_data_bus_set_quota(&priv->phandle, kms->core_client,
				SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT,
				bus_id, bus_ab_quota, bus_ib_quota);
		SDE_DEBUG("client:%s bus_id=%d ab=%llu ib=%llu\n", "rt",
				bus_id, bus_ab_quota, bus_ib_quota);
		break;

	case RT_RSC_CLIENT:
		sde_cstate = to_sde_crtc_state(crtc->state);
		sde_rsc_client_vote(sde_cstate->rsc_client,
				bus_id, bus_ab_quota, bus_ib_quota);
		SDE_DEBUG("client:%s bus_id=%d ab=%llu ib=%llu\n", "rt_rsc",
				bus_id, bus_ab_quota, bus_ib_quota);
		break;

	default:
		SDE_ERROR("invalid client type:%d\n", curr_client_type);
		break;
	}

	if (kms->perf.bw_vote_mode_updated) {
		switch (kms->perf.bw_vote_mode) {
		case DISP_RSC_MODE:
			sde_power_data_bus_set_quota(&priv->phandle,
				kms->core_client,
				SDE_POWER_HANDLE_DATA_BUS_CLIENT_NRT,
				bus_id, 0, 0);
			sde_power_data_bus_set_quota(&priv->phandle,
				kms->core_client,
				SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT,
				bus_id, 0, 0);
			kms->perf.bw_vote_mode_updated = false;
			break;

		case APPS_RSC_MODE:
			sde_cstate = to_sde_crtc_state(crtc->state);
			if (sde_cstate->rsc_client) {
				sde_rsc_client_vote(sde_cstate->rsc_client,
								bus_id, 0, 0);
				kms->perf.bw_vote_mode_updated = false;
			}
			break;

		default:
			break;
		}
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
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *sde_cstate;
	struct sde_kms *kms;
	int i;

	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	kms = _sde_crtc_get_kms(crtc);
	if (!kms || !kms->catalog) {
		SDE_ERROR("invalid kms\n");
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
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
		SDE_DEBUG("Release BW crtc=%d\n", crtc->base.id);
		for (i = 0; i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++) {
			sde_crtc->cur_perf.bw_ctl[i] = 0;
			_sde_core_perf_crtc_update_bus(kms, crtc, i);
		}
	}
}

static u64 _sde_core_perf_get_core_clk_rate(struct sde_kms *kms)
{
	u64 clk_rate = kms->perf.perf_tune.min_core_clk;
	struct drm_crtc *tmp_crtc;
	struct sde_crtc *sde_crtc;
	u64 tmp_rate;

	drm_for_each_crtc(tmp_crtc, kms->dev) {
		if (_sde_core_perf_crtc_is_power_on(tmp_crtc)) {

			/* use current perf, which are the values voted */
			sde_crtc = to_sde_crtc(tmp_crtc);
			tmp_rate = sde_crtc->cur_perf.core_clk_rate;

			clk_rate = max(tmp_rate, clk_rate);

			clk_rate = clk_round_rate(kms->perf.core_clk, clk_rate);
		}
	}

	if (kms->perf.perf_tune.mode == SDE_PERF_MODE_FIXED)
		clk_rate = kms->perf.fix_core_clk_rate;

	SDE_DEBUG("clk:%llu\n", clk_rate);

	return clk_rate;
}

void sde_core_perf_crtc_update(struct drm_crtc *crtc,
		int params_changed, bool stop_req)
{
	struct sde_core_perf_params *new, *old;
	int update_bus = 0, update_clk = 0;
	u64 clk_rate = 0;
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *sde_cstate;
	int ret, i;
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

	SDE_DEBUG("crtc:%d stop_req:%d core_clk:%llu\n",
			crtc->base.id, stop_req, kms->perf.core_clk_rate);

	mutex_lock(&sde_core_perf_lock);

	/*
	 * cache the performance numbers in the crtc prior to the
	 * crtc kickoff, so the same numbers are used during the
	 * perf update that happens post kickoff.
	 */
	if (params_changed)
		memcpy(&sde_crtc->new_perf, &sde_cstate->new_perf,
			sizeof(struct sde_core_perf_params));

	old = &sde_crtc->cur_perf;
	new = &sde_crtc->new_perf;

	if (_sde_core_perf_crtc_is_power_on(crtc) && !stop_req) {
		for (i = 0; i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++) {
			/*
			 * cases for bus bandwidth update.
			 * 1. new bandwidth vote - "ab or ib vote" is higher
			 *    than current vote for update request.
			 * 2. new bandwidth vote - "ab or ib vote" is lower
			 *    than current vote at end of commit or stop.
			 */

			if ((params_changed &&
				(new->bw_ctl[i] > old->bw_ctl[i])) ||
			    (!params_changed &&
				(new->bw_ctl[i] < old->bw_ctl[i]))) {

				SDE_DEBUG(
					"crtc=%d p=%d new_bw=%llu,old_bw=%llu\n",
					crtc->base.id, params_changed,
					new->bw_ctl[i], old->bw_ctl[i]);
				old->bw_ctl[i] = new->bw_ctl[i];
				update_bus |= BIT(i);
			}

			if ((params_changed &&
				(new->max_per_pipe_ib[i] >
				 old->max_per_pipe_ib[i])) ||
			    (!params_changed &&
				(new->max_per_pipe_ib[i] <
				old->max_per_pipe_ib[i]))) {

				SDE_DEBUG(
					"crtc=%d p=%d new_ib=%llu,old_ib=%llu\n",
					crtc->base.id, params_changed,
					new->max_per_pipe_ib[i],
					old->max_per_pipe_ib[i]);
				old->max_per_pipe_ib[i] =
						new->max_per_pipe_ib[i];
				update_bus |= BIT(i);
			}

			/* display rsc override during solver mode */
			if (kms->perf.bw_vote_mode == DISP_RSC_MODE &&
				get_sde_rsc_current_state(SDE_RSC_INDEX) ==
						SDE_RSC_CMD_STATE) {
				/* update new bandwidth in all cases */
				if (params_changed && ((new->bw_ctl[i] !=
						old->bw_ctl[i]) ||
				      (new->max_per_pipe_ib[i] !=
						old->max_per_pipe_ib[i]))) {
					old->bw_ctl[i] = new->bw_ctl[i];
					old->max_per_pipe_ib[i] =
							new->max_per_pipe_ib[i];
					update_bus |= BIT(i);
				/*
				 * reduce bw vote is not required in solver
				 * mode
				 */
				} else if (!params_changed) {
					update_bus &= ~BIT(i);
				}
			}
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
		update_bus = ~0;
		update_clk = 1;
	}
	trace_sde_perf_crtc_update(crtc->base.id,
		new->bw_ctl[SDE_POWER_HANDLE_DBUS_ID_MNOC],
		new->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_MNOC],
		new->bw_ctl[SDE_POWER_HANDLE_DBUS_ID_LLCC],
		new->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_LLCC],
		new->bw_ctl[SDE_POWER_HANDLE_DBUS_ID_EBI],
		new->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_EBI],
		new->core_clk_rate, stop_req,
		update_bus, update_clk, params_changed);

	trace_printk("crtc:%d ib_mnoc:%llu ib_llc:%llu ib_ebi:%llu\n",
		crtc->base.id,
		new->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_MNOC],
		new->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_LLCC],
		new->max_per_pipe_ib[SDE_POWER_HANDLE_DBUS_ID_EBI]);

	for (i = 0; i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++) {
		if (update_bus & BIT(i))
			_sde_core_perf_crtc_update_bus(kms, crtc, i);
	}

	/*
	 * Update the clock after bandwidth vote to ensure
	 * bandwidth is available before clock rate is increased.
	 */
	if (update_clk) {
		clk_rate = _sde_core_perf_get_core_clk_rate(kms);

		SDE_EVT32(kms->dev, stop_req, clk_rate, params_changed,
			old->core_clk_rate, new->core_clk_rate);

		ret = sde_power_clk_set_rate(&priv->phandle,
				kms->perf.clk_name, clk_rate);
		if (ret) {
			SDE_ERROR("failed to set %s clock rate %llu\n",
					kms->perf.clk_name, clk_rate);
			mutex_unlock(&sde_core_perf_lock);
			return;
		}

		kms->perf.core_clk_rate = clk_rate;
		SDE_DEBUG("update clk rate = %lld HZ\n", clk_rate);
	}
	mutex_unlock(&sde_core_perf_lock);

}

#ifdef CONFIG_DEBUG_FS

static ssize_t _sde_core_perf_mode_write(struct file *file,
		    const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct sde_core_perf *perf = file->private_data;
	struct sde_perf_cfg *cfg = &perf->catalog->perf;
	u32 perf_mode = 0;
	char buf[10];

	if (!perf)
		return -ENODEV;

	if (count >= sizeof(buf))
		return -EFAULT;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = 0;	/* end of string */

	if (kstrtouint(buf, 0, &perf_mode))
		return -EFAULT;

	if (perf_mode >= SDE_PERF_MODE_MAX)
		return -EFAULT;

	if (perf_mode == SDE_PERF_MODE_FIXED) {
		DRM_INFO("fix performance mode\n");
	} else if (perf_mode == SDE_PERF_MODE_MINIMUM) {
		/* run the driver with max clk and BW vote */
		perf->perf_tune.min_core_clk = perf->max_core_clk_rate;
		perf->perf_tune.min_bus_vote =
				(u64) cfg->max_bw_high * 1000;
		DRM_INFO("minimum performance mode\n");
	} else if (perf_mode == SDE_PERF_MODE_NORMAL) {
		/* reset the perf tune params to 0 */
		perf->perf_tune.min_core_clk = 0;
		perf->perf_tune.min_bus_vote = 0;
		DRM_INFO("normal performance mode\n");
	}
	perf->perf_tune.mode = perf_mode;

	return count;
}

static ssize_t _sde_core_perf_mode_read(struct file *file,
			char __user *buff, size_t count, loff_t *ppos)
{
	struct sde_core_perf *perf = file->private_data;
	int len = 0;
	char buf[SDE_PERF_MODE_STRING_SIZE] = {'\0'};

	if (!perf)
		return -ENODEV;

	if (*ppos)
		return 0;	/* the end */

	len = snprintf(buf, sizeof(buf),
			"mode %d min_mdp_clk %llu min_bus_vote %llu\n",
			perf->perf_tune.mode,
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

static void sde_core_perf_debugfs_destroy(struct sde_core_perf *perf)
{
	debugfs_remove_recursive(perf->debugfs_root);
	perf->debugfs_root = NULL;
}

int sde_core_perf_debugfs_init(struct sde_core_perf *perf,
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

	debugfs_create_u64("max_core_clk_rate", 0600, perf->debugfs_root,
			&perf->max_core_clk_rate);
	debugfs_create_u64("core_clk_rate", 0600, perf->debugfs_root,
			&perf->core_clk_rate);
	debugfs_create_u32("enable_bw_release", 0600, perf->debugfs_root,
			(u32 *)&perf->enable_bw_release);
	debugfs_create_u32("threshold_low", 0600, perf->debugfs_root,
			(u32 *)&catalog->perf.max_bw_low);
	debugfs_create_u32("threshold_high", 0600, perf->debugfs_root,
			(u32 *)&catalog->perf.max_bw_high);
	debugfs_create_u32("min_core_ib", 0600, perf->debugfs_root,
			(u32 *)&catalog->perf.min_core_ib);
	debugfs_create_u32("min_llcc_ib", 0600, perf->debugfs_root,
			(u32 *)&catalog->perf.min_llcc_ib);
	debugfs_create_u32("min_dram_ib", 0600, perf->debugfs_root,
			(u32 *)&catalog->perf.min_dram_ib);
	debugfs_create_file("perf_mode", 0600, perf->debugfs_root,
			(u32 *)perf, &sde_core_perf_mode_fops);
	debugfs_create_u32("bw_vote_mode", 0600, perf->debugfs_root,
			&perf->bw_vote_mode);
	debugfs_create_bool("bw_vote_mode_updated", 0600, perf->debugfs_root,
			&perf->bw_vote_mode_updated);
	debugfs_create_u64("fix_core_clk_rate", 0600, perf->debugfs_root,
			&perf->fix_core_clk_rate);
	debugfs_create_u64("fix_core_ib_vote", 0600, perf->debugfs_root,
			&perf->fix_core_ib_vote);
	debugfs_create_u64("fix_core_ab_vote", 0600, perf->debugfs_root,
			&perf->fix_core_ab_vote);

	return 0;
}
#else
static void sde_core_perf_debugfs_destroy(struct sde_core_perf *perf)
{
}

int sde_core_perf_debugfs_init(struct sde_core_perf *perf,
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

	sde_core_perf_debugfs_destroy(perf);
	perf->max_core_clk_rate = 0;
	perf->core_clk = NULL;
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
		char *clk_name)
{
	if (!perf || !dev || !catalog || !phandle || !pclient || !clk_name) {
		SDE_ERROR("invalid parameters\n");
		return -EINVAL;
	}

	perf->dev = dev;
	perf->catalog = catalog;
	perf->phandle = phandle;
	perf->pclient = pclient;
	perf->clk_name = clk_name;
	perf->sde_rsc_available = is_sde_rsc_available(SDE_RSC_INDEX);
	/* set default mode */
	if (perf->sde_rsc_available)
		perf->bw_vote_mode = DISP_RSC_MODE;
	else
		perf->bw_vote_mode = APPS_RSC_MODE;

	perf->core_clk = sde_power_clk_get_clk(phandle, clk_name);
	if (!perf->core_clk) {
		SDE_ERROR("invalid core clk\n");
		goto err;
	}

	perf->max_core_clk_rate = sde_power_clk_get_max_rate(phandle, clk_name);
	if (!perf->max_core_clk_rate) {
		SDE_DEBUG("optional max core clk rate, use default\n");
		perf->max_core_clk_rate = SDE_PERF_DEFAULT_MAX_CORE_CLK_RATE;
	}

	return 0;

err:
	sde_core_perf_destroy(perf);
	return -ENODEV;
}
