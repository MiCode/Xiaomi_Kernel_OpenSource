/* Copyright (c) 2015-2016, 2018 The Linux Foundation. All rights reserved.
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

/*
 * The default input fence timeout is 2 seconds while max allowed
 * range is 10 seconds. Any value above 10 seconds adds glitches beyond
 * tolerance limit.
 */
#define SDE_CRTC_MAX_INPUT_FENCE_TIMEOUT 10000

static struct sde_kms *get_kms(struct drm_crtc *crtc)
{
	struct msm_drm_private *priv = crtc->dev->dev_private;
	return to_sde_kms(priv->kms);
}

static inline int sde_crtc_mixer_width(struct sde_crtc *sde_crtc,
	struct drm_display_mode *mode)
{
	if (!sde_crtc || !mode)
		return 0;

	return  sde_crtc->num_mixers == CRTC_DUAL_MIXERS ?
		mode->hdisplay / CRTC_DUAL_MIXERS : mode->hdisplay;
}

static void sde_crtc_destroy(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);

	DBG("");

	if (!crtc)
		return;

	msm_property_destroy(&sde_crtc->property_info);
	debugfs_remove_recursive(sde_crtc->debugfs_root);
	sde_fence_deinit(&sde_crtc->output_fence);

	drm_crtc_cleanup(crtc);
	kfree(sde_crtc);
}

static bool sde_crtc_mode_fixup(struct drm_crtc *crtc,
		const struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	DBG("");

	if (msm_is_mode_seamless(adjusted_mode)) {
		SDE_DEBUG("seamless mode set requested\n");
		if (!crtc->enabled || crtc->state->active_changed) {
			SDE_ERROR("crtc state prevents seamless transition\n");
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
		/* force 100% alpha */
		cfg->fg.const_alpha = 0xFF;
		cfg->bg.const_alpha = 0x00;
	}

	SDE_DEBUG("format 0x%x, alpha_enable %u blend_op %u\n",
			format->base.pixel_format, format->alpha_enable,
			blend_op);
	SDE_DEBUG("fg alpha config %d %d %d %d %d\n",
		cfg->fg.alpha_sel, cfg->fg.const_alpha, cfg->fg.mod_alpha,
		cfg->fg.inv_alpha_sel, cfg->fg.inv_mode_alpha);
	SDE_DEBUG("bg alpha config %d %d %d %d %d\n",
		cfg->bg.alpha_sel, cfg->bg.const_alpha, cfg->bg.mod_alpha,
		cfg->bg.inv_alpha_sel, cfg->bg.inv_mode_alpha);
}

static u32 blend_config_per_mixer(struct drm_crtc *crtc,
	struct sde_crtc *sde_crtc, struct sde_crtc_mixer *mixer,
	struct sde_hw_color3_cfg *alpha_out)
{
	struct drm_plane *plane;
	struct drm_display_mode *mode;

	struct sde_plane_state *pstate;
	struct sde_hw_blend_cfg blend;
	struct sde_hw_ctl *ctl = mixer->hw_ctl;
	struct sde_hw_mixer *lm = mixer->hw_lm;

	u32 flush_mask = 0, crtc_split_width;
	bool dual_pipe = false;

	mode = &crtc->state->adjusted_mode;
	crtc_split_width = sde_crtc_mixer_width(sde_crtc, mode);

	drm_atomic_crtc_for_each_plane(plane, crtc) {
		pstate = to_sde_plane_state(plane->state);
		/*
		 * Always program right lm first if in dual mixer mode,
		 * it could be overwrote later.
		 */
		dual_pipe = (sde_crtc->num_mixers == CRTC_DUAL_MIXERS) ||
				(sde_plane_num_of_phy_pipe(plane) > 1);
		if (dual_pipe)
			sde_crtc->stage_cfg.stage[pstate->stage][1] =
				sde_plane_pipe(plane, 1);
		sde_crtc->stage_cfg.stage[pstate->stage][0] =
			sde_plane_pipe(plane, 0);

		SDE_DEBUG("crtc_id %d pipe %d at stage %d\n",
			crtc->base.id,
			sde_plane_pipe(plane, 0),
			pstate->stage);

		/**
		 * cache the flushmask for this layer
		 * sourcesplit is always enabled, so this layer will
		 * be staged on both the mixers
		 */
		if (dual_pipe)
			ctl->ops.get_bitmask_sspp(ctl, &flush_mask,
					sde_plane_pipe(plane, 1));
		ctl->ops.get_bitmask_sspp(ctl, &flush_mask,
				sde_plane_pipe(plane, 0));

		/* blend config */
		sde_crtc_get_blend_cfg(&blend, pstate);
		lm->ops.setup_blend_config(lm, pstate->stage, &blend);
		alpha_out->keep_fg[pstate->stage] = 1;
	}

	return flush_mask;
}

static void sde_crtc_blend_setup(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	struct sde_crtc_mixer *mixer = sde_crtc->mixers;
	struct sde_hw_ctl *ctl;
	struct sde_hw_mixer *lm;
	struct sde_hw_color3_cfg alpha_out;

	int i;

	SDE_DEBUG("%s\n", sde_crtc->name);

	/* initialize stage cfg */
	memset(&sde_crtc->stage_cfg, 0, sizeof(struct sde_hw_stage_cfg));

	for (i = 0; i < sde_crtc->num_mixers; i++) {
		uint32_t flush_mask = 0;

		if ((!mixer[i].hw_lm) || (!mixer[i].hw_ctl)) {
			sde_crtc->stage_cfg.border_enable[i] = true;
			continue;
		}

		ctl = mixer[i].hw_ctl;
		lm = mixer[i].hw_lm;
		memset(&alpha_out, 0, sizeof(alpha_out));

		flush_mask = blend_config_per_mixer(crtc, sde_crtc,
				mixer + i, &alpha_out);

		if (sde_crtc->stage_cfg.stage[SDE_STAGE_BASE][i] == SSPP_NONE)
			sde_crtc->stage_cfg.border_enable[i] = true;

		lm->ops.setup_alpha_out(lm, &alpha_out);

		/* get the flush mask for mixer */
		ctl->ops.get_bitmask_mixer(ctl, &flush_mask,
			mixer[i].hw_lm->idx);

		/* stage config flush mask */
		ctl->ops.update_pending_flush(ctl, flush_mask);
		SDE_DEBUG("lm %d ctl %d add mask 0x%x to pending flush\n",
				mixer->hw_lm->idx, ctl->idx, flush_mask);
	}

	/* Program ctl_paths */
	for (i = 0; i < ARRAY_SIZE(sde_crtc->mixers); i++) {
		if ((!mixer[i].hw_lm) || (!mixer[i].hw_ctl))
			continue;

		ctl = mixer[i].hw_ctl;
		lm = mixer[i].hw_lm;

		/* same stage config to all mixers */
		ctl->ops.setup_blendstage(ctl, mixer[i].hw_lm->idx,
			&sde_crtc->stage_cfg, i);
	}
}

void sde_crtc_prepare_fence(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;

	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	sde_crtc = to_sde_crtc(crtc);

	MSM_EVT(crtc->dev, crtc->base.id, 0);

	sde_fence_prepare(&sde_crtc->output_fence);
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
			SDE_DEBUG("%s: send event: %pK\n",
						sde_crtc->name, event);
			drm_send_vblank_event(dev, sde_crtc->drm_crtc_id,
					event);
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

	if (pending & PENDING_FLIP) {
		complete_flip(crtc, NULL);
		/* free ref count paired with the atomic_flush */
		drm_crtc_vblank_put(crtc);
	}

	if (atomic_read(&sde_crtc->drm_requested_vblank)) {
		drm_handle_vblank(dev, sde_crtc->drm_crtc_id);
		DBG_IRQ("");
		MSM_EVT(crtc->dev, crtc->base.id, 0);
	}
}

void sde_crtc_complete_commit(struct drm_crtc *crtc)
{
	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	/* signal out fence at end of commit */
	sde_fence_signal(&to_sde_crtc(crtc)->output_fence, 0);
}

/**
 * _sde_crtc_trigger_kickoff - Iterate through the control paths and trigger
 *	the hw_ctl object to flush any pending flush mask, and trigger
 *	control start if the interface types require it.
 *
 *	This is currently designed to be called only once per crtc, per flush.
 *	It should be called from the encoder, through the
 *	sde_encoder_schedule_kickoff callflow, after all the encoders are ready
 *	to have CTL_START triggered.
 *
 *	It is called from the commit thread context.
 * @data: crtc pointer
 */
static void _sde_crtc_trigger_kickoff(void *data)
{
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	struct sde_crtc_mixer *mixer;
	struct sde_hw_ctl *ctl;
	int i;

	if (!data) {
		SDE_ERROR("invalid argument\n");
		return;
	}

	MSM_EVT(crtc->dev, crtc->base.id, 0);

	/* Commit all pending flush masks to hardware */
	for (i = 0; i < ARRAY_SIZE(sde_crtc->mixers); i++) {
		ctl = sde_crtc->mixers[i].hw_ctl;
		if (ctl) {
			ctl->ops.trigger_flush(ctl);
			MSM_EVT(crtc->dev, crtc->base.id, ctl->idx);
		}
	}

	/* Signal start to any interface types that require it */
	for (i = 0; i < ARRAY_SIZE(sde_crtc->mixers); i++) {
		mixer = &sde_crtc->mixers[i];
		ctl = mixer->hw_ctl;
		if (ctl && sde_encoder_needs_ctl_start(mixer->encoder)) {
			ctl->ops.trigger_start(ctl);
			MSM_EVT(crtc->dev, crtc->base.id, ctl->idx);
		}
	}
}

/**
 * _sde_crtc_set_input_fence_timeout - update ns version of in fence timeout
 * @cstate: Pointer to sde crtc state
 */
static void _sde_crtc_set_input_fence_timeout(struct sde_crtc_state *cstate)
{
	if (!cstate) {
		SDE_ERROR("invalid cstate\n");
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
		SDE_ERROR("invalid crtc/state %pK\n", crtc);
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

static void _sde_crtc_setup_mixer_for_encoder(
		struct drm_crtc *crtc,
		struct drm_encoder *enc)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	struct sde_kms *sde_kms = get_kms(crtc);
	struct sde_rm *rm = &sde_kms->rm;
	struct sde_crtc_mixer *mixer;
	struct sde_hw_ctl *last_valid_ctl = NULL;
	int i;
	struct sde_rm_hw_iter lm_iter, ctl_iter;

	DBG("");
	sde_rm_init_hw_iter(&lm_iter, enc->base.id, SDE_HW_BLK_LM);
	sde_rm_init_hw_iter(&ctl_iter, enc->base.id, SDE_HW_BLK_CTL);

	/* Set up all the mixers and ctls reserved by this encoder */
	for (i = sde_crtc->num_mixers; i < ARRAY_SIZE(sde_crtc->mixers); i++) {
		mixer = &sde_crtc->mixers[i];

		if (!sde_rm_get_hw(rm, &lm_iter))
			break;
		mixer->hw_lm = (struct sde_hw_mixer *)lm_iter.hw;

		/* CTL may be <= LMs, if <, multiple LMs controlled by 1 CTL */
		if (!sde_rm_get_hw(rm, &ctl_iter)) {
			SDE_DEBUG("no ctl assigned to lm %d, using previous\n",
					mixer->hw_lm->idx);
			mixer->hw_ctl = last_valid_ctl;
		} else {
			mixer->hw_ctl = (struct sde_hw_ctl *)ctl_iter.hw;
			last_valid_ctl = mixer->hw_ctl;
		}

		/* Shouldn't happen, mixers are always >= ctls */
		if (!mixer->hw_ctl) {
			SDE_ERROR("no valid ctls found for lm %d\n",
					mixer->hw_lm->idx);
			return;
		}

		mixer->encoder = enc;

		sde_crtc->num_mixers++;
		SDE_DEBUG("setup mixer %d: lm %d\n", i, mixer->hw_lm->idx);
		SDE_DEBUG("setup mixer %d: ctl %d\n", i, mixer->hw_ctl->idx);
	}
}

static void _sde_crtc_setup_mixers(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	struct drm_encoder *enc;

	sde_crtc->num_mixers = 0;
	memset(sde_crtc->mixers, 0, sizeof(sde_crtc->mixers));

	/* Check for mixers on all encoders attached to this crtc */
	list_for_each_entry(enc, &crtc->dev->mode_config.encoder_list, head) {
		if (enc->crtc != crtc)
			continue;

		_sde_crtc_setup_mixer_for_encoder(crtc, enc);
	}
}

static void sde_crtc_atomic_begin(struct drm_crtc *crtc,
		struct drm_crtc_state *old_crtc_state)
{
	struct sde_crtc *sde_crtc;
	struct drm_device *dev;
	unsigned long flags;
	u32 i;

	DBG("");

	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
	dev = crtc->dev;

	DBG("crtc:%d num_mixers=%d\n", sde_crtc->drm_crtc_id,
		sde_crtc->num_mixers);
	if (!sde_crtc->num_mixers)
		_sde_crtc_setup_mixers(crtc);

	if (sde_crtc->event) {
		WARN_ON(sde_crtc->event);
	} else {
		spin_lock_irqsave(&dev->event_lock, flags);
		sde_crtc->event = crtc->state->event;
		spin_unlock_irqrestore(&dev->event_lock, flags);
	}

	/* Reset flush mask from previous commit */
	for (i = 0; i < ARRAY_SIZE(sde_crtc->mixers); i++) {
		struct sde_hw_ctl *ctl = sde_crtc->mixers[i].hw_ctl;

		if (ctl)
			ctl->ops.clear_pending_flush(ctl);
	}

	/*
	 * If no mixers have been allocated in sde_crtc_atomic_check(),
	 * it means we are trying to flush a CRTC whose state is disabled:
	 * nothing else needs to be done.
	 */
	if (unlikely(!sde_crtc->num_mixers))
		return;

	sde_crtc_blend_setup(crtc);

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

	atomic_or(pending, &sde_crtc->pending);

	/* ref count the vblank event and interrupts over the atomic commit */
	if (drm_crtc_vblank_get(crtc))
		return;
}

static void sde_crtc_atomic_flush(struct drm_crtc *crtc,
		struct drm_crtc_state *old_crtc_state)
{
	struct sde_crtc *sde_crtc;
	struct drm_device *dev;
	struct drm_plane *plane;
	unsigned long flags;

	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	DBG("");

	sde_crtc = to_sde_crtc(crtc);

	dev = crtc->dev;

	if (sde_crtc->event) {
		SDE_DEBUG("already received sde_crtc->event\n");
	} else {
		spin_lock_irqsave(&dev->event_lock, flags);
		sde_crtc->event = crtc->state->event;
		spin_unlock_irqrestore(&dev->event_lock, flags);
	}

	/*
	 * If no mixers has been allocated in sde_crtc_atomic_check(),
	 * it means we are trying to flush a CRTC whose state is disabled:
	 * nothing else needs to be done.
	 */
	if (unlikely(!sde_crtc->num_mixers))
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

	request_pending(crtc, PENDING_FLIP);

	/* Kickoff will be scheduled by outer layer */
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
		SDE_ERROR("invalid argument(s)\n");
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

void sde_crtc_commit_kickoff(struct drm_crtc *crtc)
{
	struct drm_encoder *encoder;
	struct drm_device *dev;

	if (!crtc) {
		SDE_ERROR("invalid argument\n");
		return;
	}
	dev = crtc->dev;

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (encoder->crtc != crtc)
			continue;

		/*
		 * Encoder will flush/start now, unless it has a tx pending.
		 * If so, it may delay and flush at an irq event (e.g. ppdone)
		 */
		sde_encoder_schedule_kickoff(encoder, _sde_crtc_trigger_kickoff,
				crtc);
	}
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
		SDE_ERROR("invalid argument(s)\n");
		return NULL;
	}

	sde_crtc = to_sde_crtc(crtc);
	old_cstate = to_sde_crtc_state(crtc->state);
	cstate = msm_property_alloc_state(&sde_crtc->property_info);
	if (!cstate) {
		SDE_ERROR("failed to allocate state\n");
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
		SDE_ERROR("invalid crtc\n");
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
		SDE_ERROR("failed to allocate state\n");
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
	struct sde_crtc *sde_crtc;

	if (!crtc) {
		DRM_ERROR("invalid crtc\n");
		return;
	}
	sde_crtc = to_sde_crtc(crtc);

	DBG("");

	memset(sde_crtc->mixers, 0, sizeof(sde_crtc->mixers));
	sde_crtc->num_mixers = 0;
}

static void sde_crtc_enable(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_mixer *mixer;
	struct sde_hw_mixer *lm;
	struct drm_display_mode *mode;
	struct sde_hw_mixer_cfg cfg;
	int i;

	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	DBG("");

	sde_crtc = to_sde_crtc(crtc);
	mixer = sde_crtc->mixers;

	if (WARN_ON(!crtc->state))
		return;

	mode = &crtc->state->adjusted_mode;

	drm_mode_debug_printmodeline(mode);

	for (i = 0; i < sde_crtc->num_mixers; i++) {
		lm = mixer[i].hw_lm;
		cfg.out_width = sde_crtc_mixer_width(sde_crtc, mode);
		cfg.out_height = mode->vdisplay;
		cfg.right_mixer = (i == 0) ? false : true;
		cfg.flags = 0;
		lm->ops.setup_mixer_out(lm, &cfg);
	}
}

struct plane_state {
	struct sde_plane_state *sde_pstate;
	struct drm_plane_state *drm_pstate;
};

static int pstate_cmp(const void *a, const void *b)
{
	struct plane_state *pa = (struct plane_state *)a;
	struct plane_state *pb = (struct plane_state *)b;
	int rc = 0;
	int pa_zpos, pb_zpos;

	pa_zpos = sde_plane_get_property(pa->sde_pstate, PLANE_PROP_ZPOS);
	pb_zpos = sde_plane_get_property(pb->sde_pstate, PLANE_PROP_ZPOS);

	if (pa_zpos != pb_zpos)
		rc = pa_zpos - pb_zpos;
	else
		rc = pa->drm_pstate->crtc_x - pb->drm_pstate->crtc_x;

	return rc;
}

static int sde_crtc_atomic_check(struct drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	struct sde_crtc *sde_crtc;
	struct plane_state pstates[SDE_STAGE_MAX * 2];

	struct drm_plane_state *pstate;
	struct drm_plane *plane;
	struct drm_display_mode *mode;

	int cnt = 0, rc = 0, mixer_width, i, z_pos_cur, z_pos_prev = 0;
	int z_pos = 0;
	int left_crtc_zpos_cnt[SDE_STAGE_MAX] = {0};
	int right_crtc_zpos_cnt[SDE_STAGE_MAX] = {0};

	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return -EINVAL;
	}

	sde_crtc = to_sde_crtc(crtc);
	mode = &state->adjusted_mode;
	SDE_DEBUG("%s: check", sde_crtc->name);

	mixer_width = sde_crtc_mixer_width(sde_crtc, mode);

	 /* get plane state for all drm planes associated with crtc state */
	drm_atomic_crtc_state_for_each_plane(plane, state) {
		pstate = state->state->plane_states[drm_plane_index(plane)];

		/* plane might not have changed, in which case take
		 * current state:
		 */
		if (!pstate)
			pstate = plane->state;

		pstates[cnt].sde_pstate = to_sde_plane_state(pstate);
		pstates[cnt].drm_pstate = pstate;
		cnt++;

		if (CHECK_LAYER_BOUNDS(pstate->crtc_y, pstate->crtc_h,
				mode->vdisplay) ||
		    CHECK_LAYER_BOUNDS(pstate->crtc_x, pstate->crtc_w,
				mode->hdisplay)) {
			SDE_ERROR("invalid vertical/horizontal destination\n");
			SDE_ERROR("y:%d h:%d vdisp:%d x:%d w:%d hdisp:%d\n",
				pstate->crtc_y, pstate->crtc_h, mode->vdisplay,
				pstate->crtc_x, pstate->crtc_w, mode->hdisplay);
			rc = -E2BIG;
			goto end;
		}
	}

	/* sort planes based on sorted zpos property */
	sort(pstates, cnt, sizeof(pstates[0]), pstate_cmp, NULL);

	for (i = 0; i < cnt; i++) {
		z_pos_cur = sde_plane_get_property(pstates[i].sde_pstate,
			PLANE_PROP_ZPOS);
		if (z_pos_cur != z_pos_prev)
			z_pos++;
		z_pos_prev = z_pos_cur;

		if (pstates[i].drm_pstate->crtc_x < mixer_width) {
			if (left_crtc_zpos_cnt[z_pos] == 2) {
				SDE_ERROR("> 2 plane @ stage%d on left\n",
					z_pos);
				rc = -EINVAL;
				goto end;
			}
			left_crtc_zpos_cnt[z_pos]++;
		} else {
			if (right_crtc_zpos_cnt[z_pos] == 2) {
				SDE_ERROR("> 2 plane @ stage%d on right\n",
					z_pos);
				rc = -EINVAL;
				goto end;
			}
			right_crtc_zpos_cnt[z_pos]++;
		}
		pstates[i].sde_pstate->stage = z_pos;
		/*
		 * Hardware crossbar still limits the cursor layer always to be
		 * topmost layer even though mixer could support any stage for
		 * cursor.
		 * The topmost blend stage needs to be moved to catalog or dtsi
		 * as well.
		 */
		if ((sde_plane_pipe(pstates[i].drm_pstate->plane, 0)
				== SSPP_CURSOR0) ||
			(sde_plane_pipe(pstates[i].drm_pstate->plane, 0)
				== SSPP_CURSOR1))
			pstates[i].sde_pstate->stage = SDE_STAGE_6;
		SDE_DEBUG("%s: zpos %d", sde_crtc->name,
				pstates[i].sde_pstate->stage);
	}

end:
	return rc;
}

int sde_crtc_vblank(struct drm_crtc *crtc, bool en)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	struct drm_encoder *encoder;
	struct drm_device *dev = crtc->dev;

	SDE_DEBUG("%d", en);

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (encoder->crtc != crtc)
			continue;
		/*
		 * Mark that framework requested vblank,
		 * as opposed to enabling vblank only for our internal purposes
		 * Currently this variable isn't required, but may be useful for
		 * future features
		 */
		atomic_set(&sde_crtc->drm_requested_vblank, en);
		MSM_EVT(crtc->dev, crtc->base.id, en);

		if (en)
			sde_encoder_register_vblank_callback(encoder,
					sde_crtc_vblank_cb, (void *)crtc);
		else
			sde_encoder_register_vblank_callback(encoder, NULL,
					NULL);
	}

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
		SDE_ERROR("invalid crtc\n");
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
	dev = crtc->dev;

	/* range properties */
	msm_property_install_range(&sde_crtc->property_info,
		"input_fence_timeout", 0x0, 0, SDE_CRTC_MAX_INPUT_FENCE_TIMEOUT,
		SDE_CRTC_INPUT_FENCE_TIMEOUT, CRTC_PROP_INPUT_FENCE_TIMEOUT);

	msm_property_install_range(&sde_crtc->property_info, "output_fence",
			0x0, 0, INR_OPEN_MAX, 0x0, CRTC_PROP_OUTPUT_FENCE);
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
		SDE_ERROR("invalid argument(s)\n");
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
	int i, ret = -EINVAL;

	if (!crtc || !state) {
		SDE_ERROR("invalid argument(s)\n");
	} else {
		sde_crtc = to_sde_crtc(crtc);
		cstate = to_sde_crtc_state(state);
		i = msm_property_index(&sde_crtc->property_info, property);
		if (i == CRTC_PROP_OUTPUT_FENCE) {
			ret = sde_fence_create(&sde_crtc->output_fence, val);
		} else {
			ret = msm_property_atomic_get(&sde_crtc->property_info,
					cstate->property_values,
					cstate->property_blobs, property, val);
		}
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
		m = &sde_crtc->mixers[i];
		if (!m->hw_lm) {
			seq_printf(s, "Mixer[%d] has no LM\n", i);
		} else if (!m->hw_ctl) {
			seq_printf(s, "Mixer[%d] has no CTL\n", i);
		} else {
			seq_printf(s, "LM_%d/CTL_%d\n",
					m->hw_lm->idx - LM_0,
					m->hw_ctl->idx - CTL_0);
		}
		seq_printf(s, "Border: %d\n",
				sde_crtc->stage_cfg.border_enable[i]);
	}
	for (i = 0; i < SDE_STAGE_MAX; ++i) {
		if (i == SDE_STAGE_BASE)
			seq_puts(s, "Base Stage:");
		else
			seq_printf(s, "Stage %d:", i - SDE_STAGE_0);

		for (j = 0; j < SDE_MAX_PIPES_PER_STAGE; ++j)
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
			debugfs_create_file("mixers", S_IRUGO,
					sde_crtc->debugfs_root,
					sde_crtc, &debugfs_mixer_fops);
		}
	}
}

/* initialize crtc */
struct drm_crtc *sde_crtc_init(struct drm_device *dev,
		struct drm_plane *primary_plane,
		struct drm_plane *cursor_plane,
		int drm_crtc_id)
{
	struct drm_crtc *crtc = NULL;
	struct sde_crtc *sde_crtc = NULL;
	struct msm_drm_private *priv = NULL;
	struct sde_kms *kms = NULL;

	priv = dev->dev_private;
	kms = to_sde_kms(priv->kms);

	sde_crtc = kzalloc(sizeof(*sde_crtc), GFP_KERNEL);
	if (!sde_crtc)
		return ERR_PTR(-ENOMEM);

	crtc = &sde_crtc->base;

	sde_crtc->drm_crtc_id = drm_crtc_id;
	atomic_set(&sde_crtc->drm_requested_vblank, 0);

	drm_crtc_init_with_planes(dev, crtc, primary_plane, cursor_plane,
					&sde_crtc_funcs);

	drm_crtc_helper_add(crtc, &sde_crtc_helper_funcs);
	if (primary_plane)
		primary_plane->crtc = crtc;
	if (cursor_plane)
		cursor_plane->crtc = crtc;

	/* save user friendly CRTC name for later */
	snprintf(sde_crtc->name, SDE_CRTC_NAME_SIZE, "crtc%u", crtc->base.id);

	/*
	 * Initialize output fence support. Set output fence offset to zero
	 * so that fences returned during a commit will signal at the end of
	 * the same commit.
	 */
	sde_fence_init(dev, &sde_crtc->output_fence, sde_crtc->name, 0);

	/* initialize debugfs support */
	_sde_crtc_init_debugfs(sde_crtc, kms);

	/* create CRTC properties */
	msm_property_init(&sde_crtc->property_info, &crtc->base, dev,
			priv->crtc_property, sde_crtc->property_data,
			CRTC_PROP_COUNT, CRTC_PROP_BLOBCOUNT,
			sizeof(struct sde_crtc_state));

	sde_crtc_install_properties(crtc);

	SDE_DEBUG("%s: successfully initialized crtc=%pK\n",
			sde_crtc->name, crtc);
	return crtc;
}
