/*
 * Copyright (c) 2014-2017 The Linux Foundation. All rights reserved.
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__
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
#include "sde_plane.h"
#include "sde_color_processing.h"
#include "sde_encoder.h"
#include "sde_connector.h"
#include "sde_power_handle.h"
#include "sde_core_perf.h"
#include "sde_trace.h"

/* default input fence timeout, in ms */
#define SDE_CRTC_INPUT_FENCE_TIMEOUT    2000

/*
 * The default input fence timeout is 2 seconds while max allowed
 * range is 10 seconds. Any value above 10 seconds adds glitches beyond
 * tolerance limit.
 */
#define SDE_CRTC_MAX_INPUT_FENCE_TIMEOUT 10000

/* layer mixer index on sde_crtc */
#define LEFT_MIXER 0
#define RIGHT_MIXER 1

/* indicates pending page flip events */
#define PENDING_FLIP   0x2

static inline struct sde_kms *_sde_crtc_get_kms(struct drm_crtc *crtc)
{
	struct msm_drm_private *priv;

	if (!crtc || !crtc->dev || !crtc->dev->dev_private) {
		SDE_ERROR("invalid crtc\n");
		return NULL;
	}
	priv = crtc->dev->dev_private;
	if (!priv || !priv->kms) {
		SDE_ERROR("invalid kms\n");
		return NULL;
	}

	return to_sde_kms(priv->kms);
}

static void sde_crtc_destroy(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);

	SDE_DEBUG("\n");

	if (!crtc)
		return;

	if (sde_crtc->blob_info)
		drm_property_unreference_blob(sde_crtc->blob_info);
	msm_property_destroy(&sde_crtc->property_info);
	sde_cp_crtc_destroy_properties(crtc);

	debugfs_remove_recursive(sde_crtc->debugfs_root);
	sde_fence_deinit(&sde_crtc->output_fence);

	drm_crtc_cleanup(crtc);
	mutex_destroy(&sde_crtc->crtc_lock);
	kfree(sde_crtc);
}

static bool sde_crtc_mode_fixup(struct drm_crtc *crtc,
		const struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	SDE_DEBUG("\n");

	if (msm_is_mode_seamless(adjusted_mode) &&
		(!crtc->enabled || crtc->state->active_changed)) {
		SDE_ERROR("crtc state prevents seamless transition\n");
		return false;
	}

	return true;
}

static void _sde_crtc_setup_blend_cfg(struct sde_crtc_mixer *mixer,
	struct sde_plane_state *pstate, struct sde_format *format)
{
	uint32_t blend_op, fg_alpha, bg_alpha;
	uint32_t blend_type;
	struct sde_hw_mixer *lm = mixer->hw_lm;

	/* default to opaque blending */
	fg_alpha = sde_plane_get_property(pstate, PLANE_PROP_ALPHA);
	bg_alpha = 0xFF - fg_alpha;
	blend_op = SDE_BLEND_FG_ALPHA_FG_CONST | SDE_BLEND_BG_ALPHA_BG_CONST;
	blend_type = sde_plane_get_property(pstate, PLANE_PROP_BLEND_OP);

	SDE_DEBUG("blend type:0x%x blend alpha:0x%x\n", blend_type, fg_alpha);

	switch (blend_type) {

	case SDE_DRM_BLEND_OP_OPAQUE:
		blend_op = SDE_BLEND_FG_ALPHA_FG_CONST |
			SDE_BLEND_BG_ALPHA_BG_CONST;
		break;

	case SDE_DRM_BLEND_OP_PREMULTIPLIED:
		if (format->alpha_enable) {
			blend_op = SDE_BLEND_FG_ALPHA_FG_CONST |
				SDE_BLEND_BG_ALPHA_FG_PIXEL;
			if (fg_alpha != 0xff) {
				bg_alpha = fg_alpha;
				blend_op |= SDE_BLEND_BG_MOD_ALPHA |
					SDE_BLEND_BG_INV_MOD_ALPHA;
			} else {
				blend_op |= SDE_BLEND_BG_INV_ALPHA;
			}
		}
		break;

	case SDE_DRM_BLEND_OP_COVERAGE:
		if (format->alpha_enable) {
			blend_op = SDE_BLEND_FG_ALPHA_FG_PIXEL |
				SDE_BLEND_BG_ALPHA_FG_PIXEL;
			if (fg_alpha != 0xff) {
				bg_alpha = fg_alpha;
				blend_op |= SDE_BLEND_FG_MOD_ALPHA |
					SDE_BLEND_FG_INV_MOD_ALPHA |
					SDE_BLEND_BG_MOD_ALPHA |
					SDE_BLEND_BG_INV_MOD_ALPHA;
			} else {
				blend_op |= SDE_BLEND_BG_INV_ALPHA;
			}
		}
		break;
	default:
		/* do nothing */
		break;
	}

	lm->ops.setup_blend_config(lm, pstate->stage, fg_alpha,
						bg_alpha, blend_op);
	SDE_DEBUG("format 0x%x, alpha_enable %u fg alpha:0x%x bg alpha:0x%x \"\
		 blend_op:0x%x\n", format->base.pixel_format,
		format->alpha_enable, fg_alpha, bg_alpha, blend_op);
}

static void _sde_crtc_blend_setup_mixer(struct drm_crtc *crtc,
	struct sde_crtc *sde_crtc, struct sde_crtc_mixer *mixer)
{
	struct drm_plane *plane;

	struct sde_plane_state *pstate = NULL;
	struct sde_format *format;
	struct sde_hw_ctl *ctl = mixer->hw_ctl;
	struct sde_hw_stage_cfg *stage_cfg = &sde_crtc->stage_cfg;

	u32 flush_mask = 0, crtc_split_width;
	uint32_t lm_idx = LEFT_MIXER, idx;
	bool bg_alpha_enable[CRTC_DUAL_MIXERS] = {false};
	bool lm_right = false;
	int left_crtc_zpos_cnt[SDE_STAGE_MAX + 1] = {0};
	int right_crtc_zpos_cnt[SDE_STAGE_MAX + 1] = {0};

	crtc_split_width = get_crtc_split_width(crtc);

	drm_atomic_crtc_for_each_plane(plane, crtc) {

		pstate = to_sde_plane_state(plane->state);

		/* always stage plane on either left or right lm */
		if (plane->state->crtc_x >= crtc_split_width) {
			lm_idx = RIGHT_MIXER;
			idx = right_crtc_zpos_cnt[pstate->stage]++;
		} else {
			lm_idx = LEFT_MIXER;
			idx = left_crtc_zpos_cnt[pstate->stage]++;
		}

		/*
		 * program each mixer with two hw pipes in dual mixer mode,
		 */
		if (sde_crtc->num_mixers == CRTC_DUAL_MIXERS) {
			stage_cfg->stage[LEFT_MIXER][pstate->stage][1] =
				sde_plane_pipe(plane, 1);

			flush_mask = ctl->ops.get_bitmask_sspp(ctl,
				sde_plane_pipe(plane, 1));
		}

		flush_mask |= ctl->ops.get_bitmask_sspp(ctl,
				sde_plane_pipe(plane, lm_idx ? 1 : 0));

		/* stage plane on right LM if it crosses the boundary */
		lm_right = (lm_idx == LEFT_MIXER) &&
		   (plane->state->crtc_x + plane->state->crtc_w >
							crtc_split_width);

		stage_cfg->stage[lm_idx][pstate->stage][idx] =
					sde_plane_pipe(plane, lm_idx ? 1 : 0);

		mixer[lm_idx].flush_mask |= flush_mask;

		SDE_DEBUG("crtc %d stage:%d - plane %d sspp %d fb %d\n",
				crtc->base.id,
				pstate->stage,
				plane->base.id,
				sde_plane_pipe(plane,
					lm_idx ? 1 : 0) - SSPP_VIG0,
				plane->state->fb ?
				plane->state->fb->base.id : -1);

		format = to_sde_format(msm_framebuffer_format(pstate->base.fb));

		/* blend config update */
		if (pstate->stage != SDE_STAGE_BASE) {
			_sde_crtc_setup_blend_cfg(mixer + lm_idx, pstate,
								format);

			if (bg_alpha_enable[lm_idx] && !format->alpha_enable)
				mixer[lm_idx].mixer_op_mode = 0;
			else
				mixer[lm_idx].mixer_op_mode |=
					1 << pstate->stage;
		} else if (format->alpha_enable) {
			bg_alpha_enable[lm_idx] = true;
		}

		if (lm_right) {
			idx = right_crtc_zpos_cnt[pstate->stage]++;

			/*
			 * program each mixer with two hw pipes
			   in dual mixer mode,
			 */
			if (sde_crtc->num_mixers == CRTC_DUAL_MIXERS) {
				stage_cfg->stage[RIGHT_MIXER][pstate->stage][1]
					= sde_plane_pipe(plane, 0);
			}

			stage_cfg->stage[RIGHT_MIXER][pstate->stage][idx]
				= sde_plane_pipe(plane, 1);

			mixer[RIGHT_MIXER].flush_mask |= flush_mask;

			/* blend config update */
			if (pstate->stage != SDE_STAGE_BASE) {
				_sde_crtc_setup_blend_cfg(mixer + RIGHT_MIXER,
							pstate, format);

				if (bg_alpha_enable[RIGHT_MIXER] &&
						!format->alpha_enable)
					mixer[RIGHT_MIXER].mixer_op_mode = 0;
				else
					mixer[RIGHT_MIXER].mixer_op_mode |=
						1 << pstate->stage;
			} else if (format->alpha_enable) {
				bg_alpha_enable[RIGHT_MIXER] = true;
			}
		}
	}
}

/**
 * _sde_crtc_blend_setup - configure crtc mixers
 * @crtc: Pointer to drm crtc structure
 */
static void _sde_crtc_blend_setup(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	struct sde_crtc_mixer *mixer = sde_crtc->mixers;
	struct sde_hw_ctl *ctl;
	struct sde_hw_mixer *lm;

	int i;

	SDE_DEBUG("%s\n", sde_crtc->name);

	if (sde_crtc->num_mixers > CRTC_DUAL_MIXERS) {
		SDE_ERROR("invalid number mixers: %d\n", sde_crtc->num_mixers);
		return;
	}

	for (i = 0; i < sde_crtc->num_mixers; i++) {
		if (!mixer[i].hw_lm || !mixer[i].hw_ctl) {
			SDE_ERROR("invalid lm or ctl assigned to mixer\n");
			return;
		}
		mixer[i].mixer_op_mode = 0;
		mixer[i].flush_mask = 0;
		if (mixer[i].hw_ctl->ops.clear_all_blendstages)
			mixer[i].hw_ctl->ops.clear_all_blendstages(
					mixer[i].hw_ctl);
	}

	/* initialize stage cfg */
	memset(&sde_crtc->stage_cfg, 0, sizeof(struct sde_hw_stage_cfg));

	_sde_crtc_blend_setup_mixer(crtc, sde_crtc, mixer);

	for (i = 0; i < sde_crtc->num_mixers; i++) {
		ctl = mixer[i].hw_ctl;
		lm = mixer[i].hw_lm;

		lm->ops.setup_alpha_out(lm, mixer[i].mixer_op_mode);

		mixer[i].flush_mask |= ctl->ops.get_bitmask_mixer(ctl,
			mixer[i].hw_lm->idx);

		/* stage config flush mask */
		ctl->ops.update_pending_flush(ctl, mixer[i].flush_mask);

		SDE_DEBUG("lm %d, op_mode 0x%X, ctl %d, flush mask 0x%x\n",
			mixer[i].hw_lm->idx - LM_0,
			mixer[i].mixer_op_mode,
			ctl->idx - CTL_0,
			mixer[i].flush_mask);

		ctl->ops.setup_blendstage(ctl, mixer[i].hw_lm->idx,
			&sde_crtc->stage_cfg, i);
	}
}

void sde_crtc_prepare_commit(struct drm_crtc *crtc,
		struct drm_crtc_state *old_state)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate;
	struct drm_connector *conn;

	if (!crtc || !crtc->state) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
	cstate = to_sde_crtc_state(crtc->state);
	SDE_EVT32(DRMID(crtc));

	/* identify connectors attached to this crtc */
	cstate->is_rt = false;
	cstate->num_connectors = 0;

	drm_for_each_connector(conn, crtc->dev)
		if (conn->state && conn->state->crtc == crtc &&
				cstate->num_connectors < MAX_CONNECTORS) {
			cstate->connectors[cstate->num_connectors++] = conn;
			sde_connector_prepare_fence(conn);

			if (conn->connector_type != DRM_MODE_CONNECTOR_VIRTUAL)
				cstate->is_rt = true;
		}

	/* prepare main output fence */
	sde_fence_prepare(&sde_crtc->output_fence);
}

bool sde_crtc_is_rt(struct drm_crtc *crtc)
{
	if (!crtc || !crtc->state) {
		SDE_ERROR("invalid crtc or state\n");
		return true;
	}
	return to_sde_crtc_state(crtc->state)->is_rt;
}

/**
 *  _sde_crtc_complete_flip - signal pending page_flip events
 * Any pending vblank events are added to the vblank_event_list
 * so that the next vblank interrupt shall signal them.
 * However PAGE_FLIP events are not handled through the vblank_event_list.
 * This API signals any pending PAGE_FLIP events requested through
 * DRM_IOCTL_MODE_PAGE_FLIP and are cached in the sde_crtc->event.
 * if file!=NULL, this is preclose potential cancel-flip path
 * @crtc: Pointer to drm crtc structure
 * @file: Pointer to drm file
 */
static void _sde_crtc_complete_flip(struct drm_crtc *crtc,
		struct drm_file *file)
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
			DRM_DEBUG_VBL("%s: send event: %pK\n",
						sde_crtc->name, event);
			SDE_EVT32(DRMID(crtc));
			drm_crtc_send_vblank_event(crtc, event);
		}
	}
	spin_unlock_irqrestore(&dev->event_lock, flags);
}

enum sde_intf_mode sde_crtc_get_intf_mode(struct drm_crtc *crtc)
{
	struct drm_encoder *encoder;

	if (!crtc || !crtc->dev) {
		SDE_ERROR("invalid crtc\n");
		return INTF_MODE_NONE;
	}

	drm_for_each_encoder(encoder, crtc->dev)
		if (encoder->crtc == crtc)
			return sde_encoder_get_intf_mode(encoder);

	return INTF_MODE_NONE;
}

static void sde_crtc_vblank_cb(void *data)
{
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	unsigned pending;

	pending = atomic_xchg(&sde_crtc->pending, 0);
	/* keep statistics on vblank callback - with auto reset via debugfs */
	if (ktime_equal(sde_crtc->vblank_cb_time, ktime_set(0, 0)))
		sde_crtc->vblank_cb_time = ktime_get();
	else
		sde_crtc->vblank_cb_count++;

	if (pending & PENDING_FLIP)
		_sde_crtc_complete_flip(crtc, NULL);

	drm_crtc_handle_vblank(crtc);
	DRM_DEBUG_VBL("crtc%d\n", crtc->base.id);
	SDE_EVT32_IRQ(DRMID(crtc));
}

static void sde_crtc_frame_event_work(struct kthread_work *work)
{
	struct msm_drm_private *priv;
	struct sde_crtc_frame_event *fevent;
	struct drm_crtc *crtc;
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate;
	struct sde_kms *sde_kms;
	unsigned long flags;

	if (!work) {
		SDE_ERROR("invalid work handle\n");
		return;
	}

	fevent = container_of(work, struct sde_crtc_frame_event, work);
	if (!fevent->crtc || !fevent->crtc->state) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	crtc = fevent->crtc;
	sde_crtc = to_sde_crtc(crtc);
	cstate = to_sde_crtc_state(crtc->state);

	sde_kms = _sde_crtc_get_kms(crtc);
	if (!sde_kms) {
		SDE_ERROR("invalid kms handle\n");
		return;
	}
	priv = sde_kms->dev->dev_private;

	SDE_DEBUG("crtc%d event:%u ts:%lld\n", crtc->base.id, fevent->event,
			ktime_to_ns(fevent->ts));

	if (fevent->event == SDE_ENCODER_FRAME_EVENT_DONE ||
			fevent->event == SDE_ENCODER_FRAME_EVENT_ERROR) {

		if (atomic_read(&sde_crtc->frame_pending) < 1) {
			/* this should not happen */
			SDE_ERROR("crtc%d ts:%lld invalid frame_pending:%d\n",
					crtc->base.id,
					ktime_to_ns(fevent->ts),
					atomic_read(&sde_crtc->frame_pending));
			SDE_EVT32(DRMID(crtc), fevent->event, 0);
		} else if (atomic_dec_return(&sde_crtc->frame_pending) == 0) {
			/* release bandwidth and other resources */
			SDE_DEBUG("crtc%d ts:%lld last pending\n",
					crtc->base.id,
					ktime_to_ns(fevent->ts));
			SDE_EVT32(DRMID(crtc), fevent->event, 1);
			sde_power_data_bus_bandwidth_ctrl(&priv->phandle,
					sde_kms->core_client, false);
			sde_core_perf_crtc_release_bw(crtc);
		} else {
			SDE_EVT32(DRMID(crtc), fevent->event, 2);
		}

		if (fevent->event == SDE_ENCODER_FRAME_EVENT_DONE)
			sde_core_perf_crtc_update(crtc, 0, false);
	} else {
		SDE_ERROR("crtc%d ts:%lld unknown event %u\n", crtc->base.id,
				ktime_to_ns(fevent->ts),
				fevent->event);
		SDE_EVT32(DRMID(crtc), fevent->event, 3);
	}

	spin_lock_irqsave(&sde_crtc->spin_lock, flags);
	list_add_tail(&fevent->list, &sde_crtc->frame_event_list);
	spin_unlock_irqrestore(&sde_crtc->spin_lock, flags);
}

static void sde_crtc_frame_event_cb(void *data, u32 event)
{
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct sde_crtc *sde_crtc;
	struct msm_drm_private *priv;
	struct sde_crtc_frame_event *fevent;
	unsigned long flags;
	int pipe_id;

	if (!crtc || !crtc->dev || !crtc->dev->dev_private) {
		SDE_ERROR("invalid parameters\n");
		return;
	}
	sde_crtc = to_sde_crtc(crtc);
	priv = crtc->dev->dev_private;
	pipe_id = drm_crtc_index(crtc);

	SDE_DEBUG("crtc%d\n", crtc->base.id);

	SDE_EVT32(DRMID(crtc), event);

	spin_lock_irqsave(&sde_crtc->spin_lock, flags);
	fevent = list_first_entry_or_null(&sde_crtc->frame_event_list,
			struct sde_crtc_frame_event, list);
	if (fevent)
		list_del_init(&fevent->list);
	spin_unlock_irqrestore(&sde_crtc->spin_lock, flags);

	if (!fevent) {
		SDE_ERROR("crtc%d event %d overflow\n",
				crtc->base.id, event);
		SDE_EVT32(DRMID(crtc), event);
		return;
	}

	fevent->event = event;
	fevent->crtc = crtc;
	fevent->ts = ktime_get();
	queue_kthread_work(&priv->disp_thread[pipe_id].worker, &fevent->work);
}

/**
 *  sde_crtc_request_flip_cb - callback to request page_flip events
 * Once the HW flush is complete , userspace must be notified of
 * PAGE_FLIP completed event in the next vblank event.
 * Using this callback, a hint is set to signal any callers waiting
 * for a PAGE_FLIP complete event.
 * This is called within the enc_spinlock.
 * @data: Pointer to cb private data
 */
static void sde_crtc_request_flip_cb(void *data)
{
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct sde_crtc *sde_crtc;

	if (!crtc) {
		SDE_ERROR("invalid parameters\n");
		return;
	}
	sde_crtc = to_sde_crtc(crtc);
	atomic_or(PENDING_FLIP, &sde_crtc->pending);
}

void sde_crtc_complete_commit(struct drm_crtc *crtc,
		struct drm_crtc_state *old_state)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate;
	struct drm_connector *conn;
	struct sde_connector *c_conn;
	struct drm_device *dev;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	int i;

	if (!crtc || !crtc->state || !crtc->dev) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	dev = crtc->dev;
	priv = dev->dev_private;

	sde_crtc = to_sde_crtc(crtc);
	sde_kms = _sde_crtc_get_kms(crtc);
	cstate = to_sde_crtc_state(crtc->state);
	SDE_EVT32(DRMID(crtc));

	/* signal output fence(s) at end of commit */
	sde_fence_signal(&sde_crtc->output_fence, 0);

	for (i = 0; i < cstate->num_connectors; ++i)
		sde_connector_complete_commit(cstate->connectors[i]);

	if (sde_splash_get_lk_complete_status(&sde_kms->splash_info)) {
		mutex_lock(&dev->mode_config.mutex);
		drm_for_each_connector(conn, crtc->dev) {
			if (conn->state->crtc != crtc)
				continue;

			c_conn = to_sde_connector(conn);

			sde_splash_clean_up_free_resource(priv->kms,
					&priv->phandle,
					c_conn->connector_type,
					c_conn->display);
		}
		mutex_unlock(&dev->mode_config.mutex);
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
	ktime_t kt_end, kt_wait;

	SDE_DEBUG("\n");

	if (!crtc || !crtc->state) {
		SDE_ERROR("invalid crtc/state %pK\n", crtc);
		return;
	}

	/* use monotonic timer to limit total fence wait time */
	kt_end = ktime_add_ns(ktime_get(),
		to_sde_crtc_state(crtc->state)->input_fence_timeout_ns);

	/*
	 * Wait for fences sequentially, as all of them need to be signalled
	 * before we can proceed.
	 *
	 * Limit total wait time to INPUT_FENCE_TIMEOUT, but still call
	 * sde_plane_wait_input_fence with wait_ms == 0 after the timeout so
	 * that each plane can check its fence status and react appropriately
	 * if its fence has timed out.
	 */
	SDE_ATRACE_BEGIN("plane_wait_input_fence");
	drm_atomic_crtc_for_each_plane(plane, crtc) {
		if (wait_ms) {
			/* determine updated wait time */
			kt_wait = ktime_sub(kt_end, ktime_get());
			if (ktime_compare(kt_wait, ktime_set(0, 0)) >= 0)
				wait_ms = ktime_to_ms(kt_wait);
			else
				wait_ms = 0;
		}
		sde_plane_wait_input_fence(plane, wait_ms);
	}
	SDE_ATRACE_END("plane_wait_input_fence");
}

static void _sde_crtc_setup_mixer_for_encoder(
		struct drm_crtc *crtc,
		struct drm_encoder *enc)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	struct sde_kms *sde_kms = _sde_crtc_get_kms(crtc);
	struct sde_rm *rm = &sde_kms->rm;
	struct sde_crtc_mixer *mixer;
	struct sde_hw_ctl *last_valid_ctl = NULL;
	int i;
	struct sde_rm_hw_iter lm_iter, ctl_iter, dspp_iter;

	sde_rm_init_hw_iter(&lm_iter, enc->base.id, SDE_HW_BLK_LM);
	sde_rm_init_hw_iter(&ctl_iter, enc->base.id, SDE_HW_BLK_CTL);
	sde_rm_init_hw_iter(&dspp_iter, enc->base.id, SDE_HW_BLK_DSPP);

	/* Set up all the mixers and ctls reserved by this encoder */
	for (i = sde_crtc->num_mixers; i < ARRAY_SIZE(sde_crtc->mixers); i++) {
		mixer = &sde_crtc->mixers[i];

		if (!sde_rm_get_hw(rm, &lm_iter))
			break;
		mixer->hw_lm = (struct sde_hw_mixer *)lm_iter.hw;

		/* CTL may be <= LMs, if <, multiple LMs controlled by 1 CTL */
		if (!sde_rm_get_hw(rm, &ctl_iter)) {
			SDE_DEBUG("no ctl assigned to lm %d, using previous\n",
					mixer->hw_lm->idx - LM_0);
			mixer->hw_ctl = last_valid_ctl;
		} else {
			mixer->hw_ctl = (struct sde_hw_ctl *)ctl_iter.hw;
			last_valid_ctl = mixer->hw_ctl;
		}

		/* Shouldn't happen, mixers are always >= ctls */
		if (!mixer->hw_ctl) {
			SDE_ERROR("no valid ctls found for lm %d\n",
					mixer->hw_lm->idx - LM_0);
			return;
		}

		/* Dspp may be null */
		(void) sde_rm_get_hw(rm, &dspp_iter);
		mixer->hw_dspp = (struct sde_hw_dspp *)dspp_iter.hw;

		mixer->encoder = enc;

		sde_crtc->num_mixers++;
		SDE_DEBUG("setup mixer %d: lm %d\n",
				i, mixer->hw_lm->idx - LM_0);
		SDE_DEBUG("setup mixer %d: ctl %d\n",
				i, mixer->hw_ctl->idx - CTL_0);
	}
}

static void _sde_crtc_setup_mixers(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	struct drm_encoder *enc;

	sde_crtc->num_mixers = 0;
	memset(sde_crtc->mixers, 0, sizeof(sde_crtc->mixers));

	mutex_lock(&sde_crtc->crtc_lock);
	/* Check for mixers on all encoders attached to this crtc */
	list_for_each_entry(enc, &crtc->dev->mode_config.encoder_list, head) {
		if (enc->crtc != crtc)
			continue;

		_sde_crtc_setup_mixer_for_encoder(crtc, enc);
	}
	mutex_unlock(&sde_crtc->crtc_lock);
}

static void sde_crtc_atomic_begin(struct drm_crtc *crtc,
		struct drm_crtc_state *old_state)
{
	struct sde_crtc *sde_crtc;
	struct drm_device *dev;
	u32 i;

	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	if (!crtc->state->enable) {
		SDE_DEBUG("crtc%d -> enable %d, skip atomic_begin\n",
				crtc->base.id, crtc->state->enable);
		return;
	}

	SDE_DEBUG("crtc%d\n", crtc->base.id);

	sde_crtc = to_sde_crtc(crtc);
	dev = crtc->dev;

	if (!sde_crtc->num_mixers)
		_sde_crtc_setup_mixers(crtc);

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

	_sde_crtc_blend_setup(crtc);
	sde_cp_crtc_apply_properties(crtc);

	/*
	 * PP_DONE irq is only used by command mode for now.
	 * It is better to request pending before FLUSH and START trigger
	 * to make sure no pp_done irq missed.
	 * This is safe because no pp_done will happen before SW trigger
	 * in command mode.
	 */
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

	if (!crtc->state->enable) {
		SDE_DEBUG("crtc%d -> enable %d, skip atomic_flush\n",
				crtc->base.id, crtc->state->enable);
		return;
	}

	SDE_DEBUG("crtc%d\n", crtc->base.id);

	sde_crtc = to_sde_crtc(crtc);

	dev = crtc->dev;

	if (sde_crtc->event) {
		SDE_ERROR("%s already received sde_crtc->event\n",
				  sde_crtc->name);
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

	/* update performance setting before crtc kickoff */
	sde_core_perf_crtc_update(crtc, 1, false);

	/*
	 * Final plane updates: Give each plane a chance to complete all
	 *                      required writes/flushing before crtc's "flush
	 *                      everything" call below.
	 */
	drm_atomic_crtc_for_each_plane(plane, crtc)
		sde_plane_flush(plane);

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

	SDE_DEBUG("crtc%d\n", crtc->base.id);

	__drm_atomic_helper_crtc_destroy_state(crtc, state);

	/* destroy value helper */
	msm_property_destroy_state(&sde_crtc->property_info, cstate,
			cstate->property_values, cstate->property_blobs);
}

void sde_crtc_commit_kickoff(struct drm_crtc *crtc)
{
	struct drm_encoder *encoder;
	struct drm_device *dev;
	struct sde_crtc *sde_crtc;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;

	if (!crtc) {
		SDE_ERROR("invalid argument\n");
		return;
	}
	dev = crtc->dev;
	sde_crtc = to_sde_crtc(crtc);
	sde_kms = _sde_crtc_get_kms(crtc);
	priv = sde_kms->dev->dev_private;

	/*
	 * If no mixers has been allocated in sde_crtc_atomic_check(),
	 * it means we are trying to start a CRTC whose state is disabled:
	 * nothing else needs to be done.
	 */
	if (unlikely(!sde_crtc->num_mixers))
		return;


	SDE_ATRACE_BEGIN("crtc_commit");
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (encoder->crtc != crtc)
			continue;

		/*
		 * Encoder will flush/start now, unless it has a tx pending.
		 * If so, it may delay and flush at an irq event (e.g. ppdone)
		 */
		sde_encoder_prepare_for_kickoff(encoder);
	}

	if (atomic_read(&sde_crtc->frame_pending) > 2) {
		/* framework allows only 1 outstanding + current */
		SDE_ERROR("crtc%d invalid frame pending\n",
				crtc->base.id);
		SDE_EVT32(DRMID(crtc), 0);
		goto end;
	} else if (atomic_inc_return(&sde_crtc->frame_pending) == 1) {
		/* acquire bandwidth and other resources */
		SDE_DEBUG("crtc%d first commit\n", crtc->base.id);
		SDE_EVT32(DRMID(crtc), 1);
		sde_power_data_bus_bandwidth_ctrl(&priv->phandle,
				sde_kms->core_client, true);
	} else {
		SDE_DEBUG("crtc%d commit\n", crtc->base.id);
		SDE_EVT32(DRMID(crtc), 2);
	}

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (encoder->crtc != crtc)
			continue;

		sde_encoder_kickoff(encoder);
	}
end:
	SDE_ATRACE_END("crtc_commit");
	return;
}

/**
 * _sde_crtc_vblank_enable_nolock - update power resource and vblank request
 * @sde_crtc: Pointer to sde crtc structure
 * @enable: Whether to enable/disable vblanks
 *
 * @Return: error code
 */
static int _sde_crtc_vblank_enable_no_lock(
		struct sde_crtc *sde_crtc, bool enable)
{
	struct drm_device *dev;
	struct drm_crtc *crtc;
	struct drm_encoder *enc;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	int ret = 0;

	if (!sde_crtc) {
		SDE_ERROR("invalid crtc\n");
		return -EINVAL;
	}

	crtc = &sde_crtc->base;
	dev = crtc->dev;
	priv = dev->dev_private;

	if (!priv->kms) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}
	sde_kms = to_sde_kms(priv->kms);

	if (enable) {
		ret = sde_power_resource_enable(&priv->phandle,
				sde_kms->core_client, true);
		if (ret)
			return ret;

		list_for_each_entry(enc, &dev->mode_config.encoder_list, head) {
			if (enc->crtc != crtc)
				continue;

			SDE_EVT32(DRMID(crtc), DRMID(enc), enable);

			sde_encoder_register_vblank_callback(enc,
					sde_crtc_vblank_cb, (void *)crtc);
		}
	} else {
		list_for_each_entry(enc, &dev->mode_config.encoder_list, head) {
			if (enc->crtc != crtc)
				continue;

			SDE_EVT32(DRMID(crtc), DRMID(enc), enable);

			sde_encoder_register_vblank_callback(enc, NULL, NULL);
		}
		ret = sde_power_resource_enable(&priv->phandle,
				sde_kms->core_client, false);
	}

	return ret;
}

/**
 * _sde_crtc_set_suspend - notify crtc of suspend enable/disable
 * @crtc: Pointer to drm crtc object
 * @enable: true to enable suspend, false to indicate resume
 */
static void _sde_crtc_set_suspend(struct drm_crtc *crtc, bool enable)
{
	struct sde_crtc *sde_crtc;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;

	if (!crtc || !crtc->dev || !crtc->dev->dev_private) {
		SDE_ERROR("invalid crtc\n");
		return;
	}
	sde_crtc = to_sde_crtc(crtc);
	priv = crtc->dev->dev_private;

	if (!priv->kms) {
		SDE_ERROR("invalid crtc kms\n");
		return;
	}
	sde_kms = to_sde_kms(priv->kms);

	SDE_DEBUG("crtc%d suspend = %d\n", crtc->base.id, enable);

	mutex_lock(&sde_crtc->crtc_lock);

	/*
	 * Update CP on suspend/resume transitions
	 */
	if (enable && !sde_crtc->suspend)
		sde_cp_crtc_suspend(crtc);
	else if (!enable && sde_crtc->suspend)
		sde_cp_crtc_resume(crtc);

	/*
	 * If the vblank refcount != 0, release a power reference on suspend
	 * and take it back during resume (if it is still != 0).
	 */
	if (sde_crtc->suspend == enable)
		SDE_DEBUG("crtc%d suspend already set to %d, ignoring update\n",
				crtc->base.id, enable);
	else if (sde_crtc->enabled && sde_crtc->vblank_requested)
		_sde_crtc_vblank_enable_no_lock(sde_crtc, !enable);

	sde_crtc->suspend = enable;

	mutex_unlock(&sde_crtc->crtc_lock);
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

	/* revert suspend actions, if necessary */
	if (msm_is_suspend_state(crtc->dev))
		_sde_crtc_set_suspend(crtc, false);

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

static void sde_crtc_disable(struct drm_crtc *crtc)
{
	struct drm_encoder *encoder;
	struct sde_crtc *sde_crtc;
	struct sde_kms *sde_kms;
	struct msm_drm_private *priv;
	int ret = 0;

	if (!crtc || !crtc->dev || !crtc->state) {
		SDE_ERROR("invalid crtc\n");
		return;
	}
	sde_crtc = to_sde_crtc(crtc);
	sde_kms = _sde_crtc_get_kms(crtc);
	if (!sde_kms || !sde_kms->dev || !sde_kms->dev->dev_private) {
		SDE_ERROR("invalid kms handle\n");
		return;
	}
	priv = sde_kms->dev->dev_private;

	SDE_DEBUG("crtc%d\n", crtc->base.id);

	if (msm_is_suspend_state(crtc->dev))
		_sde_crtc_set_suspend(crtc, true);

	mutex_lock(&sde_crtc->crtc_lock);
	SDE_EVT32(DRMID(crtc), sde_crtc->enabled, sde_crtc->suspend,
			sde_crtc->vblank_requested);

	if (sde_crtc->enabled && !sde_crtc->suspend &&
			sde_crtc->vblank_requested) {
		ret = _sde_crtc_vblank_enable_no_lock(sde_crtc, false);
		if (ret)
			SDE_ERROR("%s vblank enable failed: %d\n",
				sde_crtc->name, ret);
	}

	sde_crtc->enabled = false;

	if (atomic_read(&sde_crtc->frame_pending)) {
		/* release bandwidth and other resources */
		SDE_ERROR("crtc%d invalid frame pending\n",
				crtc->base.id);
		SDE_EVT32(DRMID(crtc));
		sde_power_data_bus_bandwidth_ctrl(&priv->phandle,
				sde_kms->core_client, false);
		sde_core_perf_crtc_release_bw(crtc);
		atomic_set(&sde_crtc->frame_pending, 0);
	}

	sde_core_perf_crtc_update(crtc, 0, true);

	drm_for_each_encoder(encoder, crtc->dev) {
		if (encoder->crtc != crtc)
			continue;
		sde_encoder_register_frame_event_callback(encoder, NULL, NULL);
		sde_encoder_register_request_flip_callback(encoder, NULL, NULL);
	}

	memset(sde_crtc->mixers, 0, sizeof(sde_crtc->mixers));
	sde_crtc->num_mixers = 0;
	mutex_unlock(&sde_crtc->crtc_lock);
}

static void sde_crtc_enable(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_mixer *mixer;
	struct sde_hw_mixer *lm;
	struct drm_display_mode *mode;
	struct sde_hw_mixer_cfg cfg;
	struct drm_encoder *encoder;
	int i;
	int ret = 0;

	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	SDE_DEBUG("crtc%d\n", crtc->base.id);
	SDE_EVT32(DRMID(crtc));

	sde_crtc = to_sde_crtc(crtc);
	mixer = sde_crtc->mixers;

	if (WARN_ON(!crtc->state))
		return;

	mode = &crtc->state->adjusted_mode;

	drm_mode_debug_printmodeline(mode);

	drm_for_each_encoder(encoder, crtc->dev) {
		if (encoder->crtc != crtc)
			continue;
		sde_encoder_register_frame_event_callback(encoder,
				sde_crtc_frame_event_cb, (void *)crtc);
		sde_encoder_register_request_flip_callback(encoder,
				sde_crtc_request_flip_cb, (void *)crtc);
	}

	mutex_lock(&sde_crtc->crtc_lock);
	SDE_EVT32(DRMID(crtc), sde_crtc->enabled, sde_crtc->suspend,
			sde_crtc->vblank_requested);
	if (!sde_crtc->enabled && !sde_crtc->suspend &&
			sde_crtc->vblank_requested) {
		ret = _sde_crtc_vblank_enable_no_lock(sde_crtc, true);
		if (ret)
			SDE_ERROR("%s vblank enable failed: %d\n",
				sde_crtc->name, ret);
	}
	sde_crtc->enabled = true;
	mutex_unlock(&sde_crtc->crtc_lock);

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

	int stage;
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

	int cnt = 0, rc = 0, mixer_width, i, z_pos;
	int left_zpos_cnt = 0, right_zpos_cnt = 0;

	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return -EINVAL;
	}

	if (!state->enable || !state->active) {
		SDE_DEBUG("crtc%d -> enable %d, active %d, skip atomic_check\n",
				crtc->base.id, state->enable, state->active);
		return 0;
	}

	sde_crtc = to_sde_crtc(crtc);
	mode = &state->adjusted_mode;
	SDE_DEBUG("%s: check", sde_crtc->name);

	/* force a full mode set if active state changed */
	if (state->active_changed)
		state->mode_changed = true;

	mixer_width = sde_crtc_mixer_width(sde_crtc, mode);

	 /* get plane state for all drm planes associated with crtc state */
	drm_atomic_crtc_state_for_each_plane(plane, state) {
		pstate = drm_atomic_get_existing_plane_state(
				state->state, plane);
		if (IS_ERR_OR_NULL(pstate)) {
			SDE_DEBUG("%s: failed to get plane%d state, %d\n",
					sde_crtc->name, plane->base.id, rc);
			continue;
		}
		if (cnt >= ARRAY_SIZE(pstates))
			continue;

		pstates[cnt].sde_pstate = to_sde_plane_state(pstate);
		pstates[cnt].drm_pstate = pstate;
		pstates[cnt].stage = sde_plane_get_property(
				pstates[cnt].sde_pstate, PLANE_PROP_ZPOS);
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

	/* assign mixer stages based on sorted zpos property */
	sort(pstates, cnt, sizeof(pstates[0]), pstate_cmp, NULL);

	if (!sde_is_custom_client()) {
		int stage_old = pstates[0].stage;

		z_pos = 0;
		for (i = 0; i < cnt; i++) {
			if (stage_old != pstates[i].stage)
				++z_pos;
			stage_old = pstates[i].stage;
			pstates[i].stage = z_pos;
		}
	}

	z_pos = -1;
	for (i = 0; i < cnt; i++) {
		/* reset counts at every new blend stage */
		if (pstates[i].stage != z_pos) {
			left_zpos_cnt = 0;
			right_zpos_cnt = 0;
			z_pos = pstates[i].stage;
		}

		/* verify z_pos setting before using it */
		if (z_pos >= SDE_STAGE_MAX - SDE_STAGE_0) {
			SDE_ERROR("> %d plane stages assigned\n",
					SDE_STAGE_MAX - SDE_STAGE_0);
			rc = -EINVAL;
			goto end;
		} else if (pstates[i].drm_pstate->crtc_x < mixer_width) {
			if (left_zpos_cnt == 2) {
				SDE_ERROR("> 2 planes @ stage %d on left\n",
					z_pos);
				rc = -EINVAL;
				goto end;
			}
			left_zpos_cnt++;

		} else {
			if (right_zpos_cnt == 2) {
				SDE_ERROR("> 2 planes @ stage %d on right\n",
					z_pos);
				rc = -EINVAL;
				goto end;
			}
			right_zpos_cnt++;
		}

		pstates[i].sde_pstate->stage = z_pos + SDE_STAGE_0;
		SDE_DEBUG("%s: zpos %d", sde_crtc->name, z_pos);
	}

	rc = sde_core_perf_crtc_check(crtc, state);
	if (rc) {
		SDE_ERROR("crtc%d failed performance check %d\n",
				crtc->base.id, rc);
		goto end;
	}

	/* validate source split:
	 * use pstates sorted by stage to check planes on same stage
	 * we assume that all pipes are in source split so its valid to compare
	 * without taking into account left/right mixer placement
	 */
	for (i = 1; i < cnt; i++) {
		struct plane_state *prv_pstate, *cur_pstate;
		struct sde_rect left_rect, right_rect;
		int32_t left_pid, right_pid;
		int32_t stage;

		prv_pstate = &pstates[i - 1];
		cur_pstate = &pstates[i];
		if (prv_pstate->stage != cur_pstate->stage)
			continue;

		stage = cur_pstate->stage;

		left_pid = prv_pstate->sde_pstate->base.plane->base.id;
		POPULATE_RECT(&left_rect, prv_pstate->drm_pstate->crtc_x,
			prv_pstate->drm_pstate->crtc_y,
			prv_pstate->drm_pstate->crtc_w,
			prv_pstate->drm_pstate->crtc_h, false);

		right_pid = cur_pstate->sde_pstate->base.plane->base.id;
		POPULATE_RECT(&right_rect, cur_pstate->drm_pstate->crtc_x,
			cur_pstate->drm_pstate->crtc_y,
			cur_pstate->drm_pstate->crtc_w,
			cur_pstate->drm_pstate->crtc_h, false);

		if (right_rect.x < left_rect.x) {
			swap(left_pid, right_pid);
			swap(left_rect, right_rect);
		}

		/**
		 * - planes are enumerated in pipe-priority order such that
		 *   planes with lower drm_id must be left-most in a shared
		 *   blend-stage when using source split.
		 * - planes in source split must be contiguous in width
		 * - planes in source split must have same dest yoff and height
		 */
		if (right_pid < left_pid) {
			SDE_ERROR(
				"invalid src split cfg. priority mismatch. stage: %d left: %d right: %d\n",
				stage, left_pid, right_pid);
			rc = -EINVAL;
			goto end;
		} else if (right_rect.x != (left_rect.x + left_rect.w)) {
			SDE_ERROR(
				"non-contiguous coordinates for src split. stage: %d left: %d - %d right: %d - %d\n",
				stage, left_rect.x, left_rect.w,
				right_rect.x, right_rect.w);
			rc = -EINVAL;
			goto end;
		} else if ((left_rect.y != right_rect.y) ||
				(left_rect.h != right_rect.h)) {
			SDE_ERROR(
				"source split at stage: %d. invalid yoff/height: l_y: %d r_y: %d l_h: %d r_h: %d\n",
				stage, left_rect.y, right_rect.y,
				left_rect.h, right_rect.h);
			rc = -EINVAL;
			goto end;
		}
	}


end:
	return rc;
}

int sde_crtc_vblank(struct drm_crtc *crtc, bool en)
{
	struct sde_crtc *sde_crtc;
	int ret;

	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return -EINVAL;
	}
	sde_crtc = to_sde_crtc(crtc);

	mutex_lock(&sde_crtc->crtc_lock);
	SDE_EVT32(DRMID(&sde_crtc->base), en, sde_crtc->enabled,
			sde_crtc->suspend, sde_crtc->vblank_requested);
	if (sde_crtc->enabled && !sde_crtc->suspend) {
		ret = _sde_crtc_vblank_enable_no_lock(sde_crtc, en);
		if (ret)
			SDE_ERROR("%s vblank enable failed: %d\n",
					sde_crtc->name, ret);
	}

	sde_crtc->vblank_requested = en;
	mutex_unlock(&sde_crtc->crtc_lock);

	return 0;
}

void sde_crtc_cancel_pending_flip(struct drm_crtc *crtc,
	struct drm_file *file)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);

	SDE_DEBUG("%s: cancel: %p\n", sde_crtc->name, file);
	_sde_crtc_complete_flip(crtc, file);
}

/**
 * sde_crtc_install_properties - install all drm properties for crtc
 * @crtc: Pointer to drm crtc structure
 */
static void sde_crtc_install_properties(struct drm_crtc *crtc,
				struct sde_mdss_cfg *catalog)
{
	struct sde_crtc *sde_crtc;
	struct drm_device *dev;
	struct sde_kms_info *info;
	struct sde_kms *sde_kms;
	static const struct drm_prop_enum_list e_secure_level[] = {
		{SDE_DRM_SEC_NON_SEC, "sec_and_non_sec"},
		{SDE_DRM_SEC_ONLY, "sec_only"},
	};

	SDE_DEBUG("\n");

	if (!crtc || !catalog) {
		SDE_ERROR("invalid crtc or catalog\n");
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
	dev = crtc->dev;
	sde_kms = _sde_crtc_get_kms(crtc);

	info = kzalloc(sizeof(struct sde_kms_info), GFP_KERNEL);
	if (!info) {
		SDE_ERROR("failed to allocate info memory\n");
		return;
	}

	/* range properties */
	msm_property_install_range(&sde_crtc->property_info,
		"input_fence_timeout", 0x0, 0, SDE_CRTC_MAX_INPUT_FENCE_TIMEOUT,
		SDE_CRTC_INPUT_FENCE_TIMEOUT, CRTC_PROP_INPUT_FENCE_TIMEOUT);

	msm_property_install_range(&sde_crtc->property_info, "output_fence",
			0x0, 0, INR_OPEN_MAX, 0x0, CRTC_PROP_OUTPUT_FENCE);

	msm_property_install_range(&sde_crtc->property_info,
			"output_fence_offset", 0x0, 0, 1, 0,
			CRTC_PROP_OUTPUT_FENCE_OFFSET);

	msm_property_install_range(&sde_crtc->property_info,
			"core_clk", 0x0, 0, U64_MAX,
			sde_kms->perf.max_core_clk_rate,
			CRTC_PROP_CORE_CLK);
	msm_property_install_range(&sde_crtc->property_info,
			"core_ab", 0x0, 0, U64_MAX,
			SDE_POWER_HANDLE_DATA_BUS_AB_QUOTA,
			CRTC_PROP_CORE_AB);
	msm_property_install_range(&sde_crtc->property_info,
			"core_ib", 0x0, 0, U64_MAX,
			SDE_POWER_HANDLE_DATA_BUS_IB_QUOTA,
			CRTC_PROP_CORE_IB);

	msm_property_install_blob(&sde_crtc->property_info, "capabilities",
		DRM_MODE_PROP_IMMUTABLE, CRTC_PROP_INFO);

	msm_property_install_enum(&sde_crtc->property_info, "security_level",
			0x0, 0, e_secure_level,
			ARRAY_SIZE(e_secure_level),
			CRTC_PROP_SECURITY_LEVEL, SDE_DRM_SEC_NON_SEC);

	sde_kms_info_reset(info);

	sde_kms_info_add_keyint(info, "hw_version", catalog->hwversion);
	sde_kms_info_add_keyint(info, "max_linewidth",
			catalog->max_mixer_width);
	sde_kms_info_add_keyint(info, "max_blendstages",
			catalog->max_mixer_blendstages);
	if (catalog->qseed_type == SDE_SSPP_SCALER_QSEED2)
		sde_kms_info_add_keystr(info, "qseed_type", "qseed2");
	if (catalog->qseed_type == SDE_SSPP_SCALER_QSEED3)
		sde_kms_info_add_keystr(info, "qseed_type", "qseed3");
	sde_kms_info_add_keyint(info, "has_src_split", catalog->has_src_split);
	sde_kms_info_add_keyint(info, "has_hdr", catalog->has_hdr);
	if (catalog->perf.max_bw_low)
		sde_kms_info_add_keyint(info, "max_bandwidth_low",
				catalog->perf.max_bw_low);
	if (catalog->perf.max_bw_high)
		sde_kms_info_add_keyint(info, "max_bandwidth_high",
				catalog->perf.max_bw_high);
	if (sde_kms->perf.max_core_clk_rate)
		sde_kms_info_add_keyint(info, "max_mdp_clk",
				sde_kms->perf.max_core_clk_rate);
	msm_property_set_blob(&sde_crtc->property_info, &sde_crtc->blob_info,
			info->data, SDE_KMS_INFO_DATALEN(info), CRTC_PROP_INFO);

	kfree(info);
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
		} else {
			ret = sde_cp_crtc_set_property(crtc,
					property, val);
		}
		if (ret)
			DRM_ERROR("failed to set the property\n");
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
	SDE_DEBUG("\n");

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
			int offset = sde_crtc_get_property(cstate,
					CRTC_PROP_OUTPUT_FENCE_OFFSET);

			ret = sde_fence_create(
					&sde_crtc->output_fence, val, offset);
			if (ret)
				SDE_ERROR("fence create failed\n");
		} else {
			ret = msm_property_atomic_get(&sde_crtc->property_info,
					cstate->property_values,
					cstate->property_blobs, property, val);
			if (ret)
				ret = sde_cp_crtc_get_property(crtc,
					property, val);
		}
		if (ret)
			DRM_ERROR("get property failed\n");
	}
	return ret;
}

#ifdef CONFIG_DEBUG_FS
static int _sde_debugfs_status_show(struct seq_file *s, void *data)
{
	struct sde_crtc *sde_crtc;
	struct sde_plane_state *pstate = NULL;
	struct sde_crtc_mixer *m;

	struct drm_crtc *crtc;
	struct drm_plane *plane;
	struct drm_display_mode *mode;
	struct drm_framebuffer *fb;
	struct drm_plane_state *state;

	int i, out_width;

	if (!s || !s->private)
		return -EINVAL;

	sde_crtc = s->private;
	crtc = &sde_crtc->base;

	mutex_lock(&sde_crtc->crtc_lock);
	mode = &crtc->state->adjusted_mode;
	out_width = sde_crtc_mixer_width(sde_crtc, mode);

	seq_printf(s, "crtc:%d width:%d height:%d\n", crtc->base.id,
				mode->hdisplay, mode->vdisplay);

	seq_puts(s, "\n");

	for (i = 0; i < sde_crtc->num_mixers; ++i) {
		m = &sde_crtc->mixers[i];
		if (!m->hw_lm)
			seq_printf(s, "\tmixer[%d] has no lm\n", i);
		else if (!m->hw_ctl)
			seq_printf(s, "\tmixer[%d] has no ctl\n", i);
		else
			seq_printf(s, "\tmixer:%d ctl:%d width:%d height:%d\n",
				m->hw_lm->idx - LM_0, m->hw_ctl->idx - CTL_0,
				out_width, mode->vdisplay);
	}

	seq_puts(s, "\n");

	drm_atomic_crtc_for_each_plane(plane, crtc) {
		pstate = to_sde_plane_state(plane->state);
		state = plane->state;

		if (!pstate || !state)
			continue;

		seq_printf(s, "\tplane:%u stage:%d\n", plane->base.id,
			pstate->stage);

		if (plane->state->fb) {
			fb = plane->state->fb;

			seq_printf(s, "\tfb:%d image format:%4.4s wxh:%ux%u bpp:%d\n",
				fb->base.id, (char *) &fb->pixel_format,
				fb->width, fb->height, fb->bits_per_pixel);

			seq_puts(s, "\t");
			for (i = 0; i < ARRAY_SIZE(fb->modifier); i++)
				seq_printf(s, "modifier[%d]:%8llu ", i,
							fb->modifier[i]);
			seq_puts(s, "\n");

			seq_puts(s, "\t");
			for (i = 0; i < ARRAY_SIZE(fb->pitches); i++)
				seq_printf(s, "pitches[%d]:%8u ", i,
							fb->pitches[i]);
			seq_puts(s, "\n");

			seq_puts(s, "\t");
			for (i = 0; i < ARRAY_SIZE(fb->offsets); i++)
				seq_printf(s, "offsets[%d]:%8u ", i,
							fb->offsets[i]);
			seq_puts(s, "\n");
		}

		seq_printf(s, "\tsrc_x:%4d src_y:%4d src_w:%4d src_h:%4d\n",
			state->src_x, state->src_y, state->src_w, state->src_h);

		seq_printf(s, "\tdst x:%4d dst_y:%4d dst_w:%4d dst_h:%4d\n",
			state->crtc_x, state->crtc_y, state->crtc_w,
			state->crtc_h);
		seq_puts(s, "\n");
	}

	if (sde_crtc->vblank_cb_count) {
		ktime_t diff = ktime_sub(ktime_get(), sde_crtc->vblank_cb_time);
		s64 diff_ms = ktime_to_ms(diff);
		s64 fps = diff_ms ? DIV_ROUND_CLOSEST(
				sde_crtc->vblank_cb_count * 1000, diff_ms) : 0;

		seq_printf(s,
			"vblank fps:%lld count:%u total:%llums\n",
				fps,
				sde_crtc->vblank_cb_count,
				ktime_to_ms(diff));

		/* reset time & count for next measurement */
		sde_crtc->vblank_cb_count = 0;
		sde_crtc->vblank_cb_time = ktime_set(0, 0);
	}

	seq_printf(s, "vblank_enable:%d\n", sde_crtc->vblank_requested);

	mutex_unlock(&sde_crtc->crtc_lock);

	return 0;
}

static int _sde_debugfs_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, _sde_debugfs_status_show, inode->i_private);
}
#endif

static void sde_crtc_suspend(struct drm_crtc *crtc)
{
	sde_cp_crtc_suspend(crtc);
}

static void sde_crtc_resume(struct drm_crtc *crtc)
{
	sde_cp_crtc_resume(crtc);
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
	.save = sde_crtc_suspend,
	.restore = sde_crtc_resume,
};

static const struct drm_crtc_helper_funcs sde_crtc_helper_funcs = {
	.mode_fixup = sde_crtc_mode_fixup,
	.disable = sde_crtc_disable,
	.enable = sde_crtc_enable,
	.atomic_check = sde_crtc_atomic_check,
	.atomic_begin = sde_crtc_atomic_begin,
	.atomic_flush = sde_crtc_atomic_flush,
};

#ifdef CONFIG_DEBUG_FS
#define DEFINE_SDE_DEBUGFS_SEQ_FOPS(__prefix)				\
static int __prefix ## _open(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, __prefix ## _show, inode->i_private);	\
}									\
static const struct file_operations __prefix ## _fops = {		\
	.owner = THIS_MODULE,						\
	.open = __prefix ## _open,					\
	.release = single_release,					\
	.read = seq_read,						\
	.llseek = seq_lseek,						\
}

static int sde_crtc_debugfs_state_show(struct seq_file *s, void *v)
{
	struct drm_crtc *crtc = (struct drm_crtc *) s->private;
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	struct sde_crtc_state *cstate = to_sde_crtc_state(crtc->state);

	seq_printf(s, "num_connectors: %d\n", cstate->num_connectors);
	seq_printf(s, "is_rt: %d\n", cstate->is_rt);
	seq_printf(s, "intf_mode: %d\n", sde_crtc_get_intf_mode(crtc));

	seq_printf(s, "bw_ctl: %llu\n", sde_crtc->cur_perf.bw_ctl);
	seq_printf(s, "core_clk_rate: %u\n",
			sde_crtc->cur_perf.core_clk_rate);
	seq_printf(s, "max_per_pipe_ib: %llu\n",
			sde_crtc->cur_perf.max_per_pipe_ib);

	return 0;
}
DEFINE_SDE_DEBUGFS_SEQ_FOPS(sde_crtc_debugfs_state);

static void _sde_crtc_init_debugfs(struct sde_crtc *sde_crtc,
		struct sde_kms *sde_kms)
{
	static const struct file_operations debugfs_status_fops = {
		.open =		_sde_debugfs_status_open,
		.read =		seq_read,
		.llseek =	seq_lseek,
		.release =	single_release,
	};

	if (sde_crtc && sde_kms) {
		sde_crtc->debugfs_root = debugfs_create_dir(sde_crtc->name,
				sde_debugfs_get_root(sde_kms));
		if (sde_crtc->debugfs_root) {
			/* don't error check these */
			debugfs_create_file("status", S_IRUGO,
					sde_crtc->debugfs_root,
					sde_crtc, &debugfs_status_fops);
			debugfs_create_file("state", S_IRUGO | S_IWUSR,
					sde_crtc->debugfs_root,
					&sde_crtc->base,
					&sde_crtc_debugfs_state_fops);
		}
	}
}
#else
static void _sde_crtc_init_debugfs(struct sde_crtc *sde_crtc,
		struct sde_kms *sde_kms)
{
}
#endif

/* initialize crtc */
struct drm_crtc *sde_crtc_init(struct drm_device *dev,
	struct drm_plane *plane)
{
	struct drm_crtc *crtc = NULL;
	struct sde_crtc *sde_crtc = NULL;
	struct msm_drm_private *priv = NULL;
	struct sde_kms *kms = NULL;
	int i;

	priv = dev->dev_private;
	kms = to_sde_kms(priv->kms);

	sde_crtc = kzalloc(sizeof(*sde_crtc), GFP_KERNEL);
	if (!sde_crtc)
		return ERR_PTR(-ENOMEM);

	crtc = &sde_crtc->base;
	crtc->dev = dev;

	mutex_init(&sde_crtc->crtc_lock);
	spin_lock_init(&sde_crtc->spin_lock);
	atomic_set(&sde_crtc->frame_pending, 0);

	INIT_LIST_HEAD(&sde_crtc->frame_event_list);
	for (i = 0; i < ARRAY_SIZE(sde_crtc->frame_events); i++) {
		INIT_LIST_HEAD(&sde_crtc->frame_events[i].list);
		list_add(&sde_crtc->frame_events[i].list,
				&sde_crtc->frame_event_list);
		init_kthread_work(&sde_crtc->frame_events[i].work,
				sde_crtc_frame_event_work);
	}

	drm_crtc_init_with_planes(dev, crtc, plane, NULL, &sde_crtc_funcs);

	drm_crtc_helper_add(crtc, &sde_crtc_helper_funcs);
	plane->crtc = crtc;

	/* save user friendly CRTC name for later */
	snprintf(sde_crtc->name, SDE_CRTC_NAME_SIZE, "crtc%u", crtc->base.id);

	/* initialize output fence support */
	sde_fence_init(&sde_crtc->output_fence, sde_crtc->name, crtc->base.id);

	/* initialize debugfs support */
	_sde_crtc_init_debugfs(sde_crtc, kms);

	/* create CRTC properties */
	msm_property_init(&sde_crtc->property_info, &crtc->base, dev,
			priv->crtc_property, sde_crtc->property_data,
			CRTC_PROP_COUNT, CRTC_PROP_BLOBCOUNT,
			sizeof(struct sde_crtc_state));

	sde_crtc_install_properties(crtc, kms->catalog);

	/* Install color processing properties */
	sde_cp_crtc_init(crtc);
	sde_cp_crtc_install_properties(crtc);

	SDE_DEBUG("%s: successfully initialized crtc\n", sde_crtc->name);
	return crtc;
}
