/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#include <linux/sort.h>
#include <linux/debugfs.h>
#include <linux/ktime.h>
#include <uapi/drm/sde_drm.h>
#include <drm/drm_mode.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_flip_work.h>

#include "sde_kms.h"
#include "sde_hw_lm.h"
#include "sde_hw_ctl.h"
#include "sde_crtc.h"

#define CTL(i)       (CTL_0 + (i))
#define LM(i)        (LM_0  + (i))
#define INTF(i)      (INTF_0 + (i))

/* uncomment to enable higher level IRQ msg's */
/*#define DBG_IRQ      DBG*/
#define DBG_IRQ(fmt, ...)

/* default input fence timeout, in ms */
#define SDE_CRTC_INPUT_FENCE_TIMEOUT    2000

static struct sde_kms *get_kms(struct drm_crtc *crtc)
{
	struct msm_drm_private *priv = crtc->dev->dev_private;
	return to_sde_kms(priv->kms);
}

static int sde_crtc_reserve_hw_resources(struct drm_crtc *crtc,
		struct drm_encoder *encoder)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	struct sde_kms *sde_kms = get_kms(crtc);
	struct sde_encoder_hw_resources enc_hw_res;
	const struct sde_hw_res_map *plat_hw_res_map;
	enum sde_lm unused_lm_id[CRTC_DUAL_MIXERS] = {0};
	enum sde_lm lm_idx;
	int i, unused_lm_count = 0;

	if (!sde_kms) {
		DBG("[%s] invalid kms", __func__);
		return -EINVAL;
	}

	if (!sde_kms->mmio)
		return -EINVAL;

	/* Get unused LMs */
	for (i = sde_kms->catalog->mixer_count - 1; i >= 0; --i) {
		if (!sde_rm_get_mixer(sde_kms, LM(i))) {
			unused_lm_id[unused_lm_count++] = LM(i);
			if (unused_lm_count == CRTC_DUAL_MIXERS)
				break;
		}
	}

	/* query encoder resources */
	sde_encoder_get_hw_resources(sde_crtc->encoder, &enc_hw_res);

	/* parse encoder hw resources, find CTL paths */
	for (i = CTL_0; i <= sde_kms->catalog->ctl_count; i++) {
		WARN_ON(sde_crtc->num_ctls > CRTC_DUAL_MIXERS);
		if (enc_hw_res.ctls[i]) {
			struct sde_crtc_mixer *mixer  =
				&sde_crtc->mixer[sde_crtc->num_ctls];
			mixer->hw_ctl = sde_rm_get_ctl_path(sde_kms, i);
			if (IS_ERR_OR_NULL(mixer->hw_ctl)) {
				DBG("[%s], Invalid ctl_path", __func__);
				return -EACCES;
			}
			sde_crtc->num_ctls++;
		}
	}

	/* shortcut this process if encoder has no ctl paths */
	if (!sde_crtc->num_ctls)
		return 0;

	/*
	 * Get default LMs if specified in platform config
	 * other wise acquire the free LMs
	 */
	for (i = INTF_0; i <= sde_kms->catalog->intf_count; i++) {
		if (enc_hw_res.intfs[i]) {
			struct sde_crtc_mixer *mixer  =
				&sde_crtc->mixer[sde_crtc->num_mixers];
			plat_hw_res_map = sde_rm_get_res_map(sde_kms, i);

			lm_idx = plat_hw_res_map->lm;
			if (!lm_idx && unused_lm_count)
				lm_idx = unused_lm_id[--unused_lm_count];

			DBG("Acquiring LM %d", lm_idx);
			mixer->hw_lm = sde_rm_acquire_mixer(sde_kms, lm_idx);
			if (IS_ERR_OR_NULL(mixer->hw_lm)) {
				DRM_ERROR("Invalid mixer\n");
				return -EACCES;
			}
			/* interface info */
			mixer->intf_idx = i;
			mixer->mode = enc_hw_res.intfs[i];
			sde_crtc->num_mixers++;
		}
	}

	DBG("control paths %d, num_mixers %d, lm[0] %d, ctl[0] %d ",
			sde_crtc->num_ctls, sde_crtc->num_mixers,
			sde_crtc->mixer[0].hw_lm->idx,
			sde_crtc->mixer[0].hw_ctl->idx);
	if (sde_crtc->num_mixers > 1)
		DBG("lm[1] %d, ctl[1], %d",
			sde_crtc->mixer[1].hw_lm->idx,
			sde_crtc->mixer[1].hw_ctl->idx);
	return 0;
}

static void sde_crtc_destroy(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);

	DBG("");

	if (!crtc)
		return;

	msm_property_destroy(&sde_crtc->property_info);
	debugfs_remove_recursive(sde_crtc->debugfs_root);

	drm_crtc_cleanup(crtc);
	kfree(sde_crtc);
}

static void update_crtc_vsync_count(struct sde_crtc *sde_crtc)
{
	struct vsync_info vsync;

	/* request vsync info, cache the current frame count */
	sde_encoder_get_vblank_status(sde_crtc->encoder, &vsync);
	sde_crtc->vsync_count = vsync.frame_count;
}

static bool sde_crtc_mode_fixup(struct drm_crtc *crtc,
		const struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);

	DBG("");

	/* Update vsync counter incase wait for vsync needed before mode_set */
	update_crtc_vsync_count(sde_crtc);

	if (msm_is_mode_seamless(adjusted_mode)) {
		DBG("Seamless mode set requested");
		if (!crtc->enabled || crtc->state->active_changed) {
			DRM_ERROR("crtc state prevents seamless transition");
			return false;
		}
	}

	return true;
}

static void sde_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	DBG("");
}

static void sde_crtc_get_blend_cfg(struct sde_hw_blend_cfg *cfg,
		struct sde_plane_state *pstate)
{
	struct drm_plane *plane;
	const struct sde_format *format;
	uint32_t blend_op;

	format = to_sde_format(
			msm_framebuffer_format(pstate->base.fb));
	plane = pstate->base.plane;

	memset(cfg, 0, sizeof(*cfg));

	/* default to opaque blending */
	cfg->fg.alpha_sel = ALPHA_FG_CONST;
	cfg->bg.alpha_sel = ALPHA_BG_CONST;
	cfg->fg.const_alpha =
		sde_plane_get_property32(pstate, PLANE_PROP_ALPHA);
	cfg->bg.const_alpha = 0xFF - cfg->fg.const_alpha;

	blend_op = sde_plane_get_property32(pstate, PLANE_PROP_BLEND_OP);

	if (format->alpha_enable) {
		switch (blend_op) {
		case SDE_DRM_BLEND_OP_PREMULTIPLIED:
			cfg->fg.alpha_sel = ALPHA_FG_CONST;
			cfg->bg.alpha_sel = ALPHA_FG_PIXEL;
			if (cfg->fg.const_alpha != 0xff) {
				cfg->bg.const_alpha = cfg->fg.const_alpha;
				cfg->bg.mod_alpha = 1;
				cfg->bg.inv_alpha_sel = 1;
			} else {
				cfg->bg.inv_mode_alpha = 1;
			}
			break;
		case SDE_DRM_BLEND_OP_COVERAGE:
			cfg->fg.alpha_sel = ALPHA_FG_PIXEL;
			cfg->bg.alpha_sel = ALPHA_FG_PIXEL;
			if (cfg->fg.const_alpha != 0xff) {
				cfg->bg.const_alpha = cfg->fg.const_alpha;
				cfg->fg.mod_alpha = 1;
				cfg->bg.inv_alpha_sel = 1;
				cfg->bg.mod_alpha = 1;
				cfg->bg.inv_mode_alpha = 1;
			} else {
				cfg->bg.inv_mode_alpha = 1;
			}
			break;
		default:
			/* do nothing */
			break;
		}
	} else {
		cfg->bg.inv_alpha_sel = 1;
		/* force 100% alpha */
		cfg->fg.const_alpha = 0xFF;
		cfg->bg.const_alpha = 0x00;
	}

	DBG("format 0x%x, alpha_enable %u blend_op %u",
			format->base.pixel_format, format->alpha_enable,
			blend_op);
	DBG("fg alpha config %d %d %d %d %d",
		cfg->fg.alpha_sel, cfg->fg.const_alpha, cfg->fg.mod_alpha,
		cfg->fg.inv_alpha_sel, cfg->fg.inv_mode_alpha);
	DBG("bg alpha config %d %d %d %d %d",
		cfg->bg.alpha_sel, cfg->bg.const_alpha, cfg->bg.mod_alpha,
		cfg->bg.inv_alpha_sel, cfg->bg.inv_mode_alpha);
}

static void blend_setup(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	struct sde_crtc_mixer *mixer = sde_crtc->mixer;
	struct drm_plane *plane;
	struct sde_plane_state *pstate;
	struct sde_hw_blend_cfg blend;
	struct sde_hw_ctl *ctl;
	struct sde_hw_mixer *lm;
	struct sde_hw_color3_cfg alpha_out;
	u32 flush_mask = 0;
	unsigned long flags;
	int i, plane_cnt = 0;

	DBG("");
	spin_lock_irqsave(&sde_crtc->lm_lock, flags);

	/* ctl could be reserved already */
	if (!sde_crtc->num_ctls)
		goto out;

	/* initialize stage cfg */
	memset(&sde_crtc->stage_cfg, 0, sizeof(struct sde_hw_stage_cfg));

	for (i = 0; i < sde_crtc->num_mixers; i++) {
		if ((!mixer[i].hw_lm) || (!mixer[i].hw_ctl))
			continue;

		ctl = mixer[i].hw_ctl;
		lm = mixer[i].hw_lm;
		memset(&alpha_out, 0, sizeof(alpha_out));

		drm_atomic_crtc_for_each_plane(plane, crtc) {
			pstate = to_sde_plane_state(plane->state);
			sde_crtc->stage_cfg.stage[pstate->stage][i] =
				sde_plane_pipe(plane);
			DBG("crtc_id %d - mixer %d pipe %d at stage %d",
					i,
					sde_crtc->id,
					sde_plane_pipe(plane),
					pstate->stage);
			plane_cnt++;

			/* Cache the flushmask for this layer
			 * sourcesplit is always enabled, so this layer will
			 * be staged on both the mixers
			 */
			ctl = mixer[i].hw_ctl;
			ctl->ops.get_bitmask_sspp(ctl, &flush_mask,
					sde_plane_pipe(plane));

			/* blend config */
			sde_crtc_get_blend_cfg(&blend, pstate);
			lm->ops.setup_blend_config(lm, pstate->stage, &blend);
			alpha_out.keep_fg[pstate->stage] = 1;
		}
		lm->ops.setup_alpha_out(lm, &alpha_out);

		/* stage config flush mask */
		mixer[i].flush_mask = flush_mask;
		/* get the flush mask for mixer */
		ctl->ops.get_bitmask_mixer(ctl, &mixer[i].flush_mask,
			mixer[i].hw_lm->idx);
	}

	/*
	 * If there is no base layer, enable border color.
	 * currently border color is always black
	 */
	if ((sde_crtc->stage_cfg.stage[SDE_STAGE_BASE][0] == SSPP_NONE) &&
			plane_cnt) {
		sde_crtc->stage_cfg.border_enable = 1;
		DBG("Border Color is enabled");
	}

	/* Program ctl_paths */
	for (i = 0; i < sde_crtc->num_ctls; i++) {
		if ((!mixer[i].hw_lm) || (!mixer[i].hw_ctl))
			continue;

		ctl = mixer[i].hw_ctl;
		lm = mixer[i].hw_lm;

		/* same stage config to all mixers */
		ctl->ops.setup_blendstage(ctl, mixer[i].hw_lm->idx,
			&sde_crtc->stage_cfg);
	}
out:
	spin_unlock_irqrestore(&sde_crtc->lm_lock, flags);
}

/* if file!=NULL, this is preclose potential cancel-flip path */
static void complete_flip(struct drm_crtc *crtc, struct drm_file *file)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	struct drm_device *dev = crtc->dev;
	struct drm_pending_vblank_event *event;
	unsigned long flags;

	spin_lock_irqsave(&dev->event_lock, flags);
	event = sde_crtc->event;
	if (event) {
		/* if regular vblank case (!file) or if cancel-flip from
		 * preclose on file that requested flip, then send the
		 * event:
		 */
		if (!file || (event->base.file_priv == file)) {
			sde_crtc->event = NULL;
			DBG("%s: send event: %pK", sde_crtc->name, event);
			drm_send_vblank_event(dev, sde_crtc->id, event);
		}
	}
	spin_unlock_irqrestore(&dev->event_lock, flags);
}

static void sde_crtc_vblank_cb(void *data)
{
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	struct sde_kms *sde_kms = get_kms(crtc);
	struct drm_device *dev = sde_kms->dev;
	unsigned pending;

	pending = atomic_xchg(&sde_crtc->pending, 0);

	if (pending & PENDING_FLIP)
		complete_flip(crtc, NULL);

	if (sde_crtc->drm_requested_vblank) {
		drm_handle_vblank(dev, sde_crtc->id);
		DBG_IRQ("");
		MSM_EVT(crtc->dev, sde_crtc->id, 0);
	}
}

static bool frame_flushed(struct sde_crtc *sde_crtc)
{
	struct vsync_info vsync;

	/*
	 * encoder get vsync_info
	 * if frame_count does not match
	 * frame is flushed
	 */
	sde_encoder_get_vblank_status(sde_crtc->encoder, &vsync);

	return (vsync.frame_count != sde_crtc->vsync_count) ? true : false;
}

void sde_crtc_wait_for_commit_done(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	struct drm_device *dev = crtc->dev;
	int i, ret, wait_ret_val;

	if (!sde_crtc->num_ctls)
		return;

	/* ref count the vblank event */
	ret = drm_crtc_vblank_get(crtc);
	if (ret)
		return;

	/* wait */
	wait_ret_val = wait_event_timeout(
			dev->vblank[drm_crtc_index(crtc)].queue,
			frame_flushed(sde_crtc),
			msecs_to_jiffies(50));
	if (wait_ret_val <= 1)
		dev_warn(dev->dev, "vblank time out, crtc=%d, ret %u\n",
				sde_crtc->id, ret);

	for (i = 0; i < sde_crtc->num_ctls; i++)
		sde_crtc->mixer[i].flush_mask = 0;

	/* release */
	drm_crtc_vblank_put(crtc);
}

/**
 * Flush the CTL PATH
 */
u32 crtc_flush_all(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	struct sde_hw_ctl *ctl;
	int i;

	DBG("");

	for (i = 0; i < sde_crtc->num_ctls; i++) {
		ctl = sde_crtc->mixer[i].hw_ctl;
		ctl->ops.get_bitmask_intf(ctl,
				&(sde_crtc->mixer[i].flush_mask),
				sde_crtc->mixer[i].intf_idx);
		DBG("Flushing CTL_ID %d, flush_mask %x", ctl->idx,
				sde_crtc->mixer[i].flush_mask);
		ctl->ops.setup_flush(ctl,
				sde_crtc->mixer[i].flush_mask);
	}

	return 0;
}

/**
 * _sde_crtc_set_input_fence_timeout - update ns version of in fence timeout
 * @cstate: Pointer to sde crtc state
 */
static void _sde_crtc_set_input_fence_timeout(struct sde_crtc_state *cstate)
{
	if (!cstate) {
		DRM_ERROR("invalid cstate\n");
		return;
	}
	cstate->input_fence_timeout_ns =
		sde_crtc_get_property(cstate, CRTC_PROP_INPUT_FENCE_TIMEOUT);
	cstate->input_fence_timeout_ns *= NSEC_PER_MSEC;
}

/**
 * _sde_crtc_wait_for_fences - wait for incoming framebuffer sync fences
 * @crtc: Pointer to CRTC object
 */
static void _sde_crtc_wait_for_fences(struct drm_crtc *crtc)
{
	struct drm_plane *plane = NULL;
	uint32_t wait_ms = 1;
	u64 ktime_end;
	s64 ktime_wait; /* need signed 64-bit type */

	DBG("");

	if (!crtc || !crtc->state) {
		DRM_ERROR("invalid crtc/state %pK\n", crtc);
		return;
	}

	/* use monotonic timer to limit total fence wait time */
	ktime_end = ktime_get_ns() +
		to_sde_crtc_state(crtc->state)->input_fence_timeout_ns;

	/*
	 * Wait for fences sequentially, as all of them need to be signalled
	 * before we can proceed.
	 *
	 * Limit total wait time to INPUT_FENCE_TIMEOUT, but still call
	 * sde_plane_wait_input_fence with wait_ms == 0 after the timeout so
	 * that each plane can check its fence status and react appropriately
	 * if its fence has timed out.
	 */
	drm_atomic_crtc_for_each_plane(plane, crtc) {
		if (wait_ms) {
			/* determine updated wait time */
			ktime_wait = ktime_end - ktime_get_ns();
			if (ktime_wait >= 0)
				wait_ms = ktime_wait / NSEC_PER_MSEC;
			else
				wait_ms = 0;
		}
		sde_plane_wait_input_fence(plane, wait_ms);
	}
}

static void sde_crtc_atomic_begin(struct drm_crtc *crtc,
		struct drm_crtc_state *old_crtc_state)
{
	struct sde_crtc *sde_crtc;
	struct drm_device *dev;
	unsigned long flags;

	DBG("");

	if (!crtc) {
		DRM_ERROR("invalid crtc\n");
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
	dev = crtc->dev;

	if (sde_crtc->event) {
		WARN_ON(sde_crtc->event);
	} else {
		spin_lock_irqsave(&dev->event_lock, flags);
		sde_crtc->event = crtc->state->event;
		spin_unlock_irqrestore(&dev->event_lock, flags);
	}

	/*
	 * If no CTL has been allocated in sde_crtc_atomic_check(),
	 * it means we are trying to flush a CRTC whose state is disabled:
	 * nothing else needs to be done.
	 */
	if (unlikely(!sde_crtc->num_ctls))
		return;

	blend_setup(crtc);

	/*
	 * PP_DONE irq is only used by command mode for now.
	 * It is better to request pending before FLUSH and START trigger
	 * to make sure no pp_done irq missed.
	 * This is safe because no pp_done will happen before SW trigger
	 * in command mode.
	 */
}

static void request_pending(struct drm_crtc *crtc, u32 pending)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);

	update_crtc_vsync_count(sde_crtc);
	atomic_or(pending, &sde_crtc->pending);
}

static void sde_crtc_atomic_flush(struct drm_crtc *crtc,
		struct drm_crtc_state *old_crtc_state)
{
	struct sde_crtc *sde_crtc;
	struct drm_device *dev;
	struct drm_plane *plane;
	unsigned long flags;

	if (!crtc) {
		DRM_ERROR("invalid crtc\n");
		return;
	}

	DBG("");

	sde_crtc = to_sde_crtc(crtc);

	dev = crtc->dev;

	if (sde_crtc->event) {
		DBG("already received sde_crtc->event");
	} else {
		DBG("%s: event: %pK", sde_crtc->name, crtc->state->event);
		spin_lock_irqsave(&dev->event_lock, flags);
		sde_crtc->event = crtc->state->event;
		spin_unlock_irqrestore(&dev->event_lock, flags);
	}

	/*
	 * If no CTL has been allocated in sde_crtc_atomic_check(),
	 * it means we are trying to flush a CRTC whose state is disabled:
	 * nothing else needs to be done.
	 */
	if (unlikely(!sde_crtc->num_ctls))
		return;

	/* wait for acquire fences before anything else is done */
	_sde_crtc_wait_for_fences(crtc);

	/*
	 * Final plane updates: Give each plane a chance to complete all
	 *                      required writes/flushing before crtc's "flush
	 *                      everything" call below.
	 */
	drm_atomic_crtc_for_each_plane(plane, crtc)
		sde_plane_flush(plane);

	crtc_flush_all(crtc);

	request_pending(crtc, PENDING_FLIP);
}

/**
 * sde_crtc_destroy_state - state destroy hook
 * @crtc: drm CRTC
 * @state: CRTC state object to release
 */
static void sde_crtc_destroy_state(struct drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate;

	if (!crtc || !state) {
		DRM_ERROR("invalid argument(s)\n");
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
	cstate = to_sde_crtc_state(state);

	DBG("");

	__drm_atomic_helper_crtc_destroy_state(crtc, state);

	/* destroy value helper */
	msm_property_destroy_state(&sde_crtc->property_info, cstate,
			cstate->property_values, cstate->property_blobs);
}

/**
 * sde_crtc_duplicate_state - state duplicate hook
 * @crtc: Pointer to drm crtc structure
 * @Returns: Pointer to new drm_crtc_state structure
 */
static struct drm_crtc_state *sde_crtc_duplicate_state(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate, *old_cstate;

	if (!crtc || !crtc->state) {
		DRM_ERROR("invalid argument(s)\n");
		return NULL;
	}

	sde_crtc = to_sde_crtc(crtc);
	old_cstate = to_sde_crtc_state(crtc->state);
	cstate = msm_property_alloc_state(&sde_crtc->property_info);
	if (!cstate) {
		DRM_ERROR("failed to allocate state\n");
		return NULL;
	}

	/* duplicate value helper */
	msm_property_duplicate_state(&sde_crtc->property_info,
			old_cstate, cstate,
			cstate->property_values, cstate->property_blobs);

	/* duplicate base helper */
	__drm_atomic_helper_crtc_duplicate_state(crtc, &cstate->base);

	return &cstate->base;
}

/**
 * sde_crtc_reset - reset hook for CRTCs
 * Resets the atomic state for @crtc by freeing the state pointer (which might
 * be NULL, e.g. at driver load time) and allocating a new empty state object.
 * @crtc: Pointer to drm crtc structure
 */
static void sde_crtc_reset(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate;

	if (!crtc) {
		DRM_ERROR("invalid crtc\n");
		return;
	}

	/* remove previous state, if present */
	if (crtc->state) {
		sde_crtc_destroy_state(crtc, crtc->state);
		crtc->state = 0;
	}

	sde_crtc = to_sde_crtc(crtc);
	cstate = msm_property_alloc_state(&sde_crtc->property_info);
	if (!cstate) {
		DRM_ERROR("failed to allocate state\n");
		return;
	}

	/* reset value helper */
	msm_property_reset_state(&sde_crtc->property_info, cstate,
			cstate->property_values, cstate->property_blobs);

	_sde_crtc_set_input_fence_timeout(cstate);

	cstate->base.crtc = crtc;
	crtc->state = &cstate->base;
}

static int sde_crtc_cursor_set(struct drm_crtc *crtc,
		struct drm_file *file, uint32_t handle,
		uint32_t width, uint32_t height)
{
	return 0;
}

static int sde_crtc_cursor_move(struct drm_crtc *crtc, int x, int y)
{
	return 0;
}

static void sde_crtc_disable(struct drm_crtc *crtc)
{
	DBG("");
}

static void sde_crtc_enable(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_mixer *mixer;
	struct sde_hw_mixer *lm;
	unsigned long flags;
	struct drm_display_mode *mode;
	struct sde_hw_mixer_cfg cfg;
	u32 mixer_width;
	int i;
	int rc;

	if (!crtc) {
		DRM_ERROR("invalid crtc\n");
		return;
	}

	DBG("");

	sde_crtc = to_sde_crtc(crtc);
	mixer = sde_crtc->mixer;

	if (WARN_ON(!crtc->state))
		return;

	mode = &crtc->state->adjusted_mode;

	drm_mode_debug_printmodeline(mode);

	/*
	 * reserve mixer(s) if not already avaialable
	 * if dual mode, mixer_width = half mode width
	 * program mode configuration on mixer(s)
	 */
	if ((sde_crtc->num_ctls == 0) ||
		(sde_crtc->num_mixers == 0)) {
		rc = sde_crtc_reserve_hw_resources(crtc, sde_crtc->encoder);
		if (rc) {
			DRM_ERROR("error reserving HW resource for CRTC\n");
			return;
		}
	}

	if (sde_crtc->num_mixers == CRTC_DUAL_MIXERS)
		mixer_width = mode->hdisplay >> 1;
	 else
		mixer_width = mode->hdisplay;

	spin_lock_irqsave(&sde_crtc->lm_lock, flags);

	for (i = 0; i < sde_crtc->num_mixers; i++) {
		lm = mixer[i].hw_lm;
		cfg.out_width = mixer_width;
		cfg.out_height = mode->vdisplay;
		cfg.right_mixer = (i == 0) ? false : true;
		cfg.flags = 0;
		lm->ops.setup_mixer_out(lm, &cfg);
	}

	spin_unlock_irqrestore(&sde_crtc->lm_lock, flags);
}

struct plane_state {
	struct drm_plane *plane;
	struct sde_plane_state *state;
};

static int pstate_cmp(const void *a, const void *b)
{
	struct plane_state *pa = (struct plane_state *)a;
	struct plane_state *pb = (struct plane_state *)b;

	return (int)sde_plane_get_property(pa->state, PLANE_PROP_ZPOS) -
		(int)sde_plane_get_property(pb->state, PLANE_PROP_ZPOS);
}

static int sde_crtc_atomic_check(struct drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	struct sde_crtc *sde_crtc;
	struct sde_kms *sde_kms;
	struct drm_plane *plane;
	struct plane_state pstates[SDE_STAGE_MAX];
	int max_stages;
	int cnt = 0, i;

	if (!crtc) {
		DRM_ERROR("invalid crtc\n");
		return -EINVAL;
	}

	sde_crtc = to_sde_crtc(crtc);
	sde_kms = get_kms(crtc);
	if (!sde_kms) {
		DRM_ERROR("invalid kms\n");
		return -EINVAL;
	}
	max_stages = CRTC_HW_MIXER_MAXSTAGES(sde_kms->catalog, 0);

	DBG("%s: check", sde_crtc->name);

	/* verify that there are not too many planes attached to crtc
	 * and that we don't have conflicting mixer stages:
	 */
	drm_atomic_crtc_state_for_each_plane(plane, state) {
		struct drm_plane_state *pstate;

		if (cnt >= (max_stages)) {
			DRM_ERROR("too many planes!\n");
			return -EINVAL;
		}

		pstate = state->state->plane_states[drm_plane_index(plane)];

		/* plane might not have changed, in which case take
		 * current state:
		 */
		if (!pstate)
			pstate = plane->state;
		pstates[cnt].plane = plane;
		pstates[cnt].state = to_sde_plane_state(pstate);

		cnt++;
	}

	/* assign a stage based on sorted zpos property */
	sort(pstates, cnt, sizeof(pstates[0]), pstate_cmp, NULL);

	for (i = 0; i < cnt; i++) {
		pstates[i].state->stage = SDE_STAGE_0 + i;
		DBG("%s: assign pipe %d on stage=%d zpos %d", sde_crtc->name,
				sde_plane_pipe(pstates[i].plane),
				pstates[i].state->stage,
				sde_plane_get_property32(pstates[i].state,
						PLANE_PROP_ZPOS));
	}

	return 0;
}

int sde_crtc_vblank(struct drm_crtc *crtc, bool en)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);

	DBG("%d", en);

	/*
	 * Mark that framework requested vblank,
	 * as opposed to enabling vblank only for our internal purposes
	 * Currently this variable isn't required, but may be useful for future
	 * features
	 */
	sde_crtc->drm_requested_vblank = en;

	if (en)
		sde_encoder_register_vblank_callback(sde_crtc->encoder,
				sde_crtc_vblank_cb, (void *)crtc);
	else
		sde_encoder_register_vblank_callback(sde_crtc->encoder,
				NULL, NULL);

	return 0;
}

void sde_crtc_cancel_pending_flip(struct drm_crtc *crtc, struct drm_file *file)
{
}

/**
 * sde_crtc_install_properties - install all drm properties for crtc
 * @crtc: Pointer to drm crtc structure
 */
static void sde_crtc_install_properties(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;
	struct drm_device *dev;

	DBG("");

	if (!crtc) {
		DRM_ERROR("invalid crtc\n");
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
	dev = crtc->dev;

	/* range properties */
	msm_property_install_range(&sde_crtc->property_info,
			"input_fence_timeout",
			0, ~0, SDE_CRTC_INPUT_FENCE_TIMEOUT,
			CRTC_PROP_INPUT_FENCE_TIMEOUT);
}

/**
 * sde_crtc_atomic_set_property - atomically set a crtc drm property
 * @crtc: Pointer to drm crtc structure
 * @state: Pointer to drm crtc state structure
 * @property: Pointer to targeted drm property
 * @val: Updated property value
 * @Returns: Zero on success
 */
static int sde_crtc_atomic_set_property(struct drm_crtc *crtc,
		struct drm_crtc_state *state,
		struct drm_property *property,
		uint64_t val)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate;
	int idx, ret = -EINVAL;

	if (!crtc || !state || !property) {
		DRM_ERROR("invalid argument(s)\n");
	} else {
		sde_crtc = to_sde_crtc(crtc);
		cstate = to_sde_crtc_state(state);
		ret = msm_property_atomic_set(&sde_crtc->property_info,
				cstate->property_values, cstate->property_blobs,
				property, val);
		if (!ret) {
			idx = msm_property_index(&sde_crtc->property_info,
					property);
			if (idx == CRTC_PROP_INPUT_FENCE_TIMEOUT)
				_sde_crtc_set_input_fence_timeout(cstate);
		}
	}

	return ret;
}

/**
 * sde_crtc_set_property - set a crtc drm property
 * @crtc: Pointer to drm crtc structure
 * @property: Pointer to targeted drm property
 * @val: Updated property value
 * @Returns: Zero on success
 */
static int sde_crtc_set_property(struct drm_crtc *crtc,
		struct drm_property *property, uint64_t val)
{
	DBG("");

	return sde_crtc_atomic_set_property(crtc, crtc->state, property, val);
}

/**
 * sde_crtc_atomic_get_property - retrieve a crtc drm property
 * @crtc: Pointer to drm crtc structure
 * @state: Pointer to drm crtc state structure
 * @property: Pointer to targeted drm property
 * @val: Pointer to variable for receiving property value
 * @Returns: Zero on success
 */
static int sde_crtc_atomic_get_property(struct drm_crtc *crtc,
		const struct drm_crtc_state *state,
		struct drm_property *property,
		uint64_t *val)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate;
	int ret = -EINVAL;

	if (!crtc || !state) {
		DRM_ERROR("invalid argument(s)\n");
	} else {
		sde_crtc = to_sde_crtc(crtc);
		cstate = to_sde_crtc_state(state);
		ret = msm_property_atomic_get(&sde_crtc->property_info,
				cstate->property_values, cstate->property_blobs,
				property, val);
	}

	return ret;
}

static int _sde_debugfs_mixer_read(struct seq_file *s, void *data)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_mixer *m;
	int i, j;

	if (!s || !s->private)
		return -EINVAL;

	sde_crtc = s->private;
	for (i = 0; i < sde_crtc->num_mixers; ++i) {
		m = &sde_crtc->mixer[i];
		if (!m->hw_lm) {
			seq_printf(s, "Mixer[%d] has no LM\n", i);
		} else if (!m->hw_ctl) {
			seq_printf(s, "Mixer[%d] has no CTL\n", i);
		} else {
			seq_printf(s, "LM_%d/CTL_%d -> INTF_%d\n",
					m->hw_lm->idx - LM_0,
					m->hw_ctl->idx - CTL_0,
					m->intf_idx - INTF_0);
		}
	}
	seq_printf(s, "Border: %d\n", sde_crtc->stage_cfg.border_enable);
	for (i = 0; i < SDE_STAGE_MAX; ++i) {
		if (i == SDE_STAGE_BASE)
			seq_puts(s, "Base Stage:");
		else
			seq_printf(s, "Stage %d:", i - SDE_STAGE_0);
		for (j = 0; j < PIPES_PER_STAGE; ++j)
			seq_printf(s, " % 2d", sde_crtc->stage_cfg.stage[i][j]);
		seq_puts(s, "\n");
	}
	return 0;
}

static int _sde_debugfs_mixer_open(struct inode *inode, struct file *file)
{
	return single_open(file, _sde_debugfs_mixer_read, inode->i_private);
}

static const struct drm_crtc_funcs sde_crtc_funcs = {
	.set_config = drm_atomic_helper_set_config,
	.destroy = sde_crtc_destroy,
	.page_flip = drm_atomic_helper_page_flip,
	.set_property = sde_crtc_set_property,
	.atomic_set_property = sde_crtc_atomic_set_property,
	.atomic_get_property = sde_crtc_atomic_get_property,
	.reset = sde_crtc_reset,
	.atomic_duplicate_state = sde_crtc_duplicate_state,
	.atomic_destroy_state = sde_crtc_destroy_state,
	.cursor_set = sde_crtc_cursor_set,
	.cursor_move = sde_crtc_cursor_move,
};

static const struct drm_crtc_helper_funcs sde_crtc_helper_funcs = {
	.mode_fixup = sde_crtc_mode_fixup,
	.mode_set_nofb = sde_crtc_mode_set_nofb,
	.disable = sde_crtc_disable,
	.enable = sde_crtc_enable,
	.atomic_check = sde_crtc_atomic_check,
	.atomic_begin = sde_crtc_atomic_begin,
	.atomic_flush = sde_crtc_atomic_flush,
};

static void _sde_crtc_init_debugfs(struct sde_crtc *sde_crtc,
		struct sde_kms *sde_kms)
{
	static const struct file_operations debugfs_mixer_fops = {
		.open =		_sde_debugfs_mixer_open,
		.read =		seq_read,
		.llseek =	seq_lseek,
		.release =	single_release,
	};
	if (sde_crtc && sde_kms) {
		sde_crtc->debugfs_root = debugfs_create_dir(sde_crtc->name,
				sde_debugfs_get_root(sde_kms));
		if (sde_crtc->debugfs_root) {
			/* don't error check these */
			debugfs_create_u32("vsync_count", S_IRUGO,
					sde_crtc->debugfs_root,
					&sde_crtc->vsync_count);
			debugfs_create_file("mixers", S_IRUGO,
					sde_crtc->debugfs_root,
					sde_crtc, &debugfs_mixer_fops);
		}
	}
}

/* initialize crtc */
struct drm_crtc *sde_crtc_init(struct drm_device *dev,
		struct drm_encoder *encoder,
		struct drm_plane *plane, int id)
{
	struct drm_crtc *crtc = NULL;
	struct sde_crtc *sde_crtc = NULL;
	struct msm_drm_private *priv = NULL;
	struct sde_kms *kms = NULL;
	int rc;

	priv = dev->dev_private;
	kms = to_sde_kms(priv->kms);

	sde_crtc = kzalloc(sizeof(*sde_crtc), GFP_KERNEL);
	if (!sde_crtc)
		return ERR_PTR(-ENOMEM);

	crtc = &sde_crtc->base;

	sde_crtc->id = id;
	sde_crtc->encoder = encoder;
	spin_lock_init(&sde_crtc->lm_lock);

	drm_crtc_init_with_planes(dev, crtc, plane, NULL, &sde_crtc_funcs);

	drm_crtc_helper_add(crtc, &sde_crtc_helper_funcs);
	plane->crtc = crtc;

	rc = sde_crtc_reserve_hw_resources(crtc, encoder);
	 if (rc) {
		DRM_ERROR(" error reserving HW resource for this CRTC\n");
		return ERR_PTR(-EINVAL);
	 }

	/* save user friendly CRTC name for later */
	snprintf(sde_crtc->name, SDE_CRTC_NAME_SIZE, "crtc%u", crtc->base.id);

	_sde_crtc_init_debugfs(sde_crtc, kms);

	/* create CRTC properties */
	msm_property_init(&sde_crtc->property_info, &crtc->base, dev,
			priv->crtc_property, sde_crtc->property_data,
			CRTC_PROP_COUNT, CRTC_PROP_BLOBCOUNT,
			sizeof(struct sde_crtc_state));

	sde_crtc_install_properties(crtc);

	DBG("%s: Successfully initialized crtc", sde_crtc->name);
	return crtc;
}
