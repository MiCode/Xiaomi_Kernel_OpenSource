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

struct sde_crtc_irq_info {
	struct sde_irq_callback irq;
	u32 event;
	int (*func)(struct drm_crtc *crtc, bool en,
			struct sde_irq_callback *irq);
	struct list_head list;
};

struct sde_crtc_custom_events {
	u32 event;
	int (*func)(struct drm_crtc *crtc, bool en,
			struct sde_irq_callback *irq);
};

static int sde_crtc_power_interrupt_handler(struct drm_crtc *crtc_drm,
	bool en, struct sde_irq_callback *ad_irq);

static struct sde_crtc_custom_events custom_events[] = {
	{DRM_EVENT_AD_BACKLIGHT, sde_cp_ad_interrupt},
	{DRM_EVENT_CRTC_POWER, sde_crtc_power_interrupt_handler}
};

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

#define MISR_BUFF_SIZE			256

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

static inline int _sde_crtc_power_enable(struct sde_crtc *sde_crtc, bool enable)
{
	struct drm_crtc *crtc;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;

	if (!sde_crtc) {
		SDE_ERROR("invalid sde crtc\n");
		return -EINVAL;
	}

	crtc = &sde_crtc->base;
	if (!crtc->dev || !crtc->dev->dev_private) {
		SDE_ERROR("invalid drm device\n");
		return -EINVAL;
	}

	priv = crtc->dev->dev_private;
	if (!priv->kms) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}

	sde_kms = to_sde_kms(priv->kms);

	return sde_power_resource_enable(&priv->phandle, sde_kms->core_client,
									enable);
}

/**
 * _sde_crtc_rp_to_crtc - get crtc from resource pool object
 * @rp: Pointer to resource pool
 * return: Pointer to drm crtc if success; null otherwise
 */
static struct drm_crtc *_sde_crtc_rp_to_crtc(struct sde_crtc_respool *rp)
{
	if (!rp)
		return NULL;

	return container_of(rp, struct sde_crtc_state, rp)->base.crtc;
}

/**
 * _sde_crtc_rp_reclaim - reclaim unused, or all if forced, resources in pool
 * @rp: Pointer to resource pool
 * @force: True to reclaim all resources; otherwise, reclaim only unused ones
 * return: None
 */
static void _sde_crtc_rp_reclaim(struct sde_crtc_respool *rp, bool force)
{
	struct sde_crtc_res *res, *next;
	struct drm_crtc *crtc;

	crtc = _sde_crtc_rp_to_crtc(rp);
	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	SDE_DEBUG("crtc%d.%u %s\n", crtc->base.id, rp->sequence_id,
			force ? "destroy" : "free_unused");

	list_for_each_entry_safe(res, next, &rp->res_list, list) {
		if (!force && !(res->flags & SDE_CRTC_RES_FLAG_FREE))
			continue;
		SDE_DEBUG("crtc%d.%u reclaim res:0x%x/0x%llx/%pK/%d\n",
				crtc->base.id, rp->sequence_id,
				res->type, res->tag, res->val,
				atomic_read(&res->refcount));
		list_del(&res->list);
		if (res->ops.put)
			res->ops.put(res->val);
		kfree(res);
	}
}

/**
 * _sde_crtc_rp_free_unused - free unused resource in pool
 * @rp: Pointer to resource pool
 * return: none
 */
static void _sde_crtc_rp_free_unused(struct sde_crtc_respool *rp)
{
	_sde_crtc_rp_reclaim(rp, false);
}

/**
 * _sde_crtc_rp_destroy - destroy resource pool
 * @rp: Pointer to resource pool
 * return: None
 */
static void _sde_crtc_rp_destroy(struct sde_crtc_respool *rp)
{
	_sde_crtc_rp_reclaim(rp, true);
}

/**
 * _sde_crtc_hw_blk_get - get callback for hardware block
 * @val: Resource handle
 * @type: Resource type
 * @tag: Search tag for given resource
 * return: Resource handle
 */
static void *_sde_crtc_hw_blk_get(void *val, u32 type, u64 tag)
{
	SDE_DEBUG("res:%d/0x%llx/%pK\n", type, tag, val);
	return sde_hw_blk_get(val, type, tag);
}

/**
 * _sde_crtc_hw_blk_put - put callback for hardware block
 * @val: Resource handle
 * return: None
 */
static void _sde_crtc_hw_blk_put(void *val)
{
	SDE_DEBUG("res://%pK\n", val);
	sde_hw_blk_put(val);
}

/**
 * _sde_crtc_rp_duplicate - duplicate resource pool and reset reference count
 * @rp: Pointer to original resource pool
 * @dup_rp: Pointer to duplicated resource pool
 * return: None
 */
static void _sde_crtc_rp_duplicate(struct sde_crtc_respool *rp,
		struct sde_crtc_respool *dup_rp)
{
	struct sde_crtc_res *res, *dup_res;
	struct drm_crtc *crtc;

	if (!rp || !dup_rp) {
		SDE_ERROR("invalid resource pool\n");
		return;
	}

	crtc = _sde_crtc_rp_to_crtc(rp);
	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	SDE_DEBUG("crtc%d.%u duplicate\n", crtc->base.id, rp->sequence_id);

	dup_rp->sequence_id = rp->sequence_id + 1;
	INIT_LIST_HEAD(&dup_rp->res_list);
	dup_rp->ops = rp->ops;
	list_for_each_entry(res, &rp->res_list, list) {
		dup_res = kzalloc(sizeof(struct sde_crtc_res), GFP_KERNEL);
		if (!dup_res)
			return;
		INIT_LIST_HEAD(&dup_res->list);
		atomic_set(&dup_res->refcount, 0);
		dup_res->type = res->type;
		dup_res->tag = res->tag;
		dup_res->val = res->val;
		dup_res->ops = res->ops;
		dup_res->flags = SDE_CRTC_RES_FLAG_FREE;
		SDE_DEBUG("crtc%d.%u dup res:0x%x/0x%llx/%pK/%d\n",
				crtc->base.id, dup_rp->sequence_id,
				dup_res->type, dup_res->tag, dup_res->val,
				atomic_read(&dup_res->refcount));
		list_add_tail(&dup_res->list, &dup_rp->res_list);
		if (dup_res->ops.get)
			dup_res->ops.get(dup_res->val, 0, -1);
	}
}

/**
 * _sde_crtc_rp_reset - reset resource pool after allocation
 * @rp: Pointer to original resource pool
 * return: None
 */
static void _sde_crtc_rp_reset(struct sde_crtc_respool *rp)
{
	if (!rp) {
		SDE_ERROR("invalid resource pool\n");
		return;
	}

	rp->sequence_id = 0;
	INIT_LIST_HEAD(&rp->res_list);
	rp->ops.get = _sde_crtc_hw_blk_get;
	rp->ops.put = _sde_crtc_hw_blk_put;
}

/**
 * _sde_crtc_rp_add - add given resource to resource pool
 * @rp: Pointer to original resource pool
 * @type: Resource type
 * @tag: Search tag for given resource
 * @val: Resource handle
 * @ops: Resource callback operations
 * return: 0 if success; error code otherwise
 */
static int _sde_crtc_rp_add(struct sde_crtc_respool *rp, u32 type, u64 tag,
		void *val, struct sde_crtc_res_ops *ops)
{
	struct sde_crtc_res *res;
	struct drm_crtc *crtc;

	if (!rp || !ops) {
		SDE_ERROR("invalid resource pool/ops\n");
		return -EINVAL;
	}

	crtc = _sde_crtc_rp_to_crtc(rp);
	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return -EINVAL;
	}

	list_for_each_entry(res, &rp->res_list, list) {
		if (res->type != type || res->tag != tag)
			continue;
		SDE_ERROR("crtc%d.%u already exist res:0x%x/0x%llx/%pK/%d\n",
				crtc->base.id, rp->sequence_id,
				res->type, res->tag, res->val,
				atomic_read(&res->refcount));
		return -EEXIST;
	}
	res = kzalloc(sizeof(struct sde_crtc_res), GFP_KERNEL);
	if (!res)
		return -ENOMEM;
	INIT_LIST_HEAD(&res->list);
	atomic_set(&res->refcount, 1);
	res->type = type;
	res->tag = tag;
	res->val = val;
	res->ops = *ops;
	list_add_tail(&res->list, &rp->res_list);
	SDE_DEBUG("crtc%d.%u added res:0x%x/0x%llx\n",
			crtc->base.id, rp->sequence_id, type, tag);
	return 0;
}

/**
 * _sde_crtc_rp_get - lookup the resource from given resource pool and obtain
 *	if available; otherwise, obtain resource from global pool
 * @rp: Pointer to original resource pool
 * @type: Resource type
 * @tag:  Search tag for given resource
 * return: Resource handle if success; pointer error or null otherwise
 */
static void *_sde_crtc_rp_get(struct sde_crtc_respool *rp, u32 type, u64 tag)
{
	struct sde_crtc_res *res;
	void *val = NULL;
	int rc;
	struct drm_crtc *crtc;

	if (!rp) {
		SDE_ERROR("invalid resource pool\n");
		return NULL;
	}

	crtc = _sde_crtc_rp_to_crtc(rp);
	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return NULL;
	}

	list_for_each_entry(res, &rp->res_list, list) {
		if (res->type != type || res->tag != tag)
			continue;
		SDE_DEBUG("crtc%d.%u found res:0x%x/0x%llx/%pK/%d\n",
				crtc->base.id, rp->sequence_id,
				res->type, res->tag, res->val,
				atomic_read(&res->refcount));
		atomic_inc(&res->refcount);
		res->flags &= ~SDE_CRTC_RES_FLAG_FREE;
		return res->val;
	}
	list_for_each_entry(res, &rp->res_list, list) {
		if (res->type != type || !(res->flags & SDE_CRTC_RES_FLAG_FREE))
			continue;
		SDE_DEBUG("crtc%d.%u retag res:0x%x/0x%llx/%pK/%d\n",
				crtc->base.id, rp->sequence_id,
				res->type, res->tag, res->val,
				atomic_read(&res->refcount));
		atomic_inc(&res->refcount);
		res->tag = tag;
		res->flags &= ~SDE_CRTC_RES_FLAG_FREE;
		return res->val;
	}
	if (rp->ops.get)
		val = rp->ops.get(NULL, type, -1);
	if (IS_ERR_OR_NULL(val)) {
		SDE_DEBUG("crtc%d.%u failed to get res:0x%x//\n",
				crtc->base.id, rp->sequence_id, type);
		return NULL;
	}
	rc = _sde_crtc_rp_add(rp, type, tag, val, &rp->ops);
	if (rc) {
		SDE_ERROR("crtc%d.%u failed to add res:0x%x/0x%llx\n",
				crtc->base.id, rp->sequence_id, type, tag);
		if (rp->ops.put)
			rp->ops.put(val);
		val = NULL;
	}
	return val;
}

/**
 * _sde_crtc_rp_put - return given resource to resource pool
 * @rp: Pointer to original resource pool
 * @type: Resource type
 * @tag: Search tag for given resource
 * return: None
 */
static void _sde_crtc_rp_put(struct sde_crtc_respool *rp, u32 type, u64 tag)
{
	struct sde_crtc_res *res, *next;
	struct drm_crtc *crtc;

	if (!rp) {
		SDE_ERROR("invalid resource pool\n");
		return;
	}

	crtc = _sde_crtc_rp_to_crtc(rp);
	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	list_for_each_entry_safe(res, next, &rp->res_list, list) {
		if (res->type != type || res->tag != tag)
			continue;
		SDE_DEBUG("crtc%d.%u found res:0x%x/0x%llx/%pK/%d\n",
				crtc->base.id, rp->sequence_id,
				res->type, res->tag, res->val,
				atomic_read(&res->refcount));
		if (res->flags & SDE_CRTC_RES_FLAG_FREE)
			SDE_ERROR(
				"crtc%d.%u already free res:0x%x/0x%llx/%pK/%d\n",
					crtc->base.id, rp->sequence_id,
					res->type, res->tag, res->val,
					atomic_read(&res->refcount));
		else if (atomic_dec_return(&res->refcount) == 0)
			res->flags |= SDE_CRTC_RES_FLAG_FREE;

		return;
	}
	SDE_ERROR("crtc%d.%u not found res:0x%x/0x%llx\n",
			crtc->base.id, rp->sequence_id, type, tag);
}

int sde_crtc_res_add(struct drm_crtc_state *state, u32 type, u64 tag,
		void *val, struct sde_crtc_res_ops *ops)
{
	struct sde_crtc_respool *rp;

	if (!state) {
		SDE_ERROR("invalid parameters\n");
		return -EINVAL;
	}

	rp = &to_sde_crtc_state(state)->rp;
	return _sde_crtc_rp_add(rp, type, tag, val, ops);
}

void *sde_crtc_res_get(struct drm_crtc_state *state, u32 type, u64 tag)
{
	struct sde_crtc_respool *rp;
	void *val;

	if (!state) {
		SDE_ERROR("invalid parameters\n");
		return NULL;
	}

	rp = &to_sde_crtc_state(state)->rp;
	val = _sde_crtc_rp_get(rp, type, tag);
	if (IS_ERR(val)) {
		SDE_ERROR("failed to get res type:0x%x:0x%llx\n",
				type, tag);
		return NULL;
	}

	return val;
}

void sde_crtc_res_put(struct drm_crtc_state *state, u32 type, u64 tag)
{
	struct sde_crtc_respool *rp;

	if (!state) {
		SDE_ERROR("invalid parameters\n");
		return;
	}

	rp = &to_sde_crtc_state(state)->rp;
	_sde_crtc_rp_put(rp, type, tag);
}

static void _sde_crtc_deinit_events(struct sde_crtc *sde_crtc)
{
	if (!sde_crtc)
		return;

	if (sde_crtc->event_thread) {
		kthread_flush_worker(&sde_crtc->event_worker);
		kthread_stop(sde_crtc->event_thread);
		sde_crtc->event_thread = NULL;
	}
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

	sde_fence_deinit(&sde_crtc->output_fence);
	_sde_crtc_deinit_events(sde_crtc);

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
	SDE_DEBUG(
		"format: %4.4s, alpha_enable %u fg alpha:0x%x bg alpha:0x%x blend_op:0x%x\n",
		(char *) &format->base.pixel_format,
		format->alpha_enable, fg_alpha, bg_alpha, blend_op);
}

static void _sde_crtc_setup_dim_layer_cfg(struct drm_crtc *crtc,
		struct sde_crtc *sde_crtc, struct sde_crtc_mixer *mixer,
		struct sde_hw_dim_layer *dim_layer)
{
	struct sde_crtc_state *cstate;
	struct sde_hw_mixer *lm;
	struct sde_hw_dim_layer split_dim_layer;
	int i;

	if (!dim_layer->rect.w || !dim_layer->rect.h) {
		SDE_DEBUG("empty dim_layer\n");
		return;
	}

	cstate = to_sde_crtc_state(crtc->state);

	SDE_DEBUG("dim_layer - flags:%d, stage:%d\n",
			dim_layer->flags, dim_layer->stage);

	split_dim_layer.stage = dim_layer->stage;
	split_dim_layer.color_fill = dim_layer->color_fill;

	/*
	 * traverse through the layer mixers attached to crtc and find the
	 * intersecting dim layer rect in each LM and program accordingly.
	 */
	for (i = 0; i < sde_crtc->num_mixers; i++) {
		split_dim_layer.flags = dim_layer->flags;

		sde_kms_rect_intersect(&cstate->lm_bounds[i], &dim_layer->rect,
					&split_dim_layer.rect);
		if (sde_kms_rect_is_null(&split_dim_layer.rect)) {
			/*
			 * no extra programming required for non-intersecting
			 * layer mixers with INCLUSIVE dim layer
			 */
			if (split_dim_layer.flags & SDE_DRM_DIM_LAYER_INCLUSIVE)
				continue;

			/*
			 * program the other non-intersecting layer mixers with
			 * INCLUSIVE dim layer of full size for uniformity
			 * with EXCLUSIVE dim layer config.
			 */
			split_dim_layer.flags &= ~SDE_DRM_DIM_LAYER_EXCLUSIVE;
			split_dim_layer.flags |= SDE_DRM_DIM_LAYER_INCLUSIVE;
			memcpy(&split_dim_layer.rect, &cstate->lm_bounds[i],
					sizeof(split_dim_layer.rect));

		} else {
			split_dim_layer.rect.x =
					split_dim_layer.rect.x -
						cstate->lm_bounds[i].x;
		}

		SDE_DEBUG("split_dim_layer - LM:%d, rect:{%d,%d,%d,%d}}\n",
			i, split_dim_layer.rect.x, split_dim_layer.rect.y,
			split_dim_layer.rect.w, split_dim_layer.rect.h);

		lm = mixer[i].hw_lm;
		mixer[i].mixer_op_mode |= 1 << split_dim_layer.stage;
		lm->ops.setup_dim_layer(lm, &split_dim_layer);
	}
}

void sde_crtc_get_crtc_roi(struct drm_crtc_state *state,
		const struct sde_rect **crtc_roi)
{
	struct sde_crtc_state *crtc_state;

	if (!state || !crtc_roi)
		return;

	crtc_state = to_sde_crtc_state(state);
	*crtc_roi = &crtc_state->crtc_roi;
}

static int _sde_crtc_set_roi_v1(struct drm_crtc_state *state,
		void *usr_ptr)
{
	struct drm_crtc *crtc;
	struct sde_crtc_state *cstate;
	struct sde_drm_roi_v1 roi_v1;
	int i;

	if (!state) {
		SDE_ERROR("invalid args\n");
		return -EINVAL;
	}

	cstate = to_sde_crtc_state(state);
	crtc = cstate->base.crtc;

	memset(&cstate->user_roi_list, 0, sizeof(cstate->user_roi_list));

	if (!usr_ptr) {
		SDE_DEBUG("crtc%d: rois cleared\n", DRMID(crtc));
		return 0;
	}

	if (copy_from_user(&roi_v1, usr_ptr, sizeof(roi_v1))) {
		SDE_ERROR("crtc%d: failed to copy roi_v1 data\n", DRMID(crtc));
		return -EINVAL;
	}

	SDE_DEBUG("crtc%d: num_rects %d\n", DRMID(crtc), roi_v1.num_rects);

	if (roi_v1.num_rects == 0) {
		SDE_DEBUG("crtc%d: rois cleared\n", DRMID(crtc));
		return 0;
	}

	if (roi_v1.num_rects > SDE_MAX_ROI_V1) {
		SDE_ERROR("crtc%d: too many rects specified: %d\n", DRMID(crtc),
				roi_v1.num_rects);
		return -EINVAL;
	}

	cstate->user_roi_list.num_rects = roi_v1.num_rects;
	for (i = 0; i < roi_v1.num_rects; ++i) {
		cstate->user_roi_list.roi[i] = roi_v1.roi[i];
		SDE_DEBUG("crtc%d: roi%d: roi (%d,%d) (%d,%d)\n",
				DRMID(crtc), i,
				cstate->user_roi_list.roi[i].x1,
				cstate->user_roi_list.roi[i].y1,
				cstate->user_roi_list.roi[i].x2,
				cstate->user_roi_list.roi[i].y2);
	}

	return 0;
}

static bool _sde_crtc_setup_is_3dmux_dsc(struct drm_crtc_state *state)
{
	int i;
	struct sde_crtc_state *cstate;
	bool is_3dmux_dsc = false;

	cstate = to_sde_crtc_state(state);

	for (i = 0; i < cstate->num_connectors; i++) {
		struct drm_connector *conn = cstate->connectors[i];

		if (sde_connector_get_topology_name(conn) ==
				SDE_RM_TOPOLOGY_DUALPIPE_3DMERGE_DSC)
			is_3dmux_dsc = true;
	}

	return is_3dmux_dsc;
}

static int _sde_crtc_set_crtc_roi(struct drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	struct drm_connector *conn;
	struct drm_connector_state *conn_state;
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *crtc_state;
	struct sde_rect *crtc_roi;
	int i, num_attached_conns = 0;

	if (!crtc || !state)
		return -EINVAL;

	sde_crtc = to_sde_crtc(crtc);
	crtc_state = to_sde_crtc_state(state);
	crtc_roi = &crtc_state->crtc_roi;

	for_each_connector_in_state(state->state, conn, conn_state, i) {
		struct sde_connector_state *sde_conn_state;

		if (!conn_state || conn_state->crtc != crtc)
			continue;

		if (num_attached_conns) {
			SDE_ERROR(
				"crtc%d: unsupported: roi on crtc w/ >1 connectors\n",
					DRMID(crtc));
			return -EINVAL;
		}
		++num_attached_conns;

		sde_conn_state = to_sde_connector_state(conn_state);

		/*
		 * current driver only supports same connector and crtc size,
		 * but if support for different sizes is added, driver needs
		 * to check the connector roi here to make sure is full screen
		 * for dsc 3d-mux topology that doesn't support partial update.
		 */
		if (memcmp(&sde_conn_state->rois, &crtc_state->user_roi_list,
				sizeof(crtc_state->user_roi_list))) {
			SDE_ERROR("%s: crtc -> conn roi scaling unsupported\n",
					sde_crtc->name);
			return -EINVAL;
		}
	}

	sde_kms_rect_merge_rectangles(&crtc_state->user_roi_list, crtc_roi);

	/*
	 * for 3dmux dsc, make sure is full ROI, since current driver doesn't
	 * support partial update for this configuration.
	 */
	if (!sde_kms_rect_is_null(crtc_roi) &&
		_sde_crtc_setup_is_3dmux_dsc(state)) {
		struct drm_display_mode *adj_mode = &state->adjusted_mode;

		if (crtc_roi->w != adj_mode->hdisplay ||
			crtc_roi->h != adj_mode->vdisplay) {
			SDE_ERROR("%s: unsupported top roi[%d %d] wxh[%d %d]\n",
				sde_crtc->name, crtc_roi->w, crtc_roi->h,
				adj_mode->hdisplay, adj_mode->vdisplay);
			return -EINVAL;
		}
	}

	SDE_DEBUG("%s: crtc roi (%d,%d,%d,%d)\n", sde_crtc->name,
			crtc_roi->x, crtc_roi->y, crtc_roi->w, crtc_roi->h);

	return 0;
}

static int _sde_crtc_check_autorefresh(struct drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *crtc_state;
	struct drm_connector *conn;
	struct drm_connector_state *conn_state;
	int i;

	if (!crtc || !state)
		return -EINVAL;

	sde_crtc = to_sde_crtc(crtc);
	crtc_state = to_sde_crtc_state(state);

	if (sde_kms_rect_is_null(&crtc_state->crtc_roi))
		return 0;

	/* partial update active, check if autorefresh is also requested */
	for_each_connector_in_state(state->state, conn, conn_state, i) {
		uint64_t autorefresh;

		if (!conn_state || conn_state->crtc != crtc)
			continue;

		autorefresh = sde_connector_get_property(conn_state,
				CONNECTOR_PROP_AUTOREFRESH);
		if (autorefresh) {
			SDE_ERROR(
				"%s: autorefresh & partial crtc roi incompatible %llu\n",
					sde_crtc->name, autorefresh);
			return -EINVAL;
		}
	}

	return 0;
}

static int _sde_crtc_set_lm_roi(struct drm_crtc *crtc,
		struct drm_crtc_state *state, int lm_idx)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *crtc_state;
	const struct sde_rect *crtc_roi;
	const struct sde_rect *lm_bounds;
	struct sde_rect *lm_roi;

	if (!crtc || !state || lm_idx >= ARRAY_SIZE(crtc_state->lm_bounds))
		return -EINVAL;

	sde_crtc = to_sde_crtc(crtc);
	crtc_state = to_sde_crtc_state(state);
	crtc_roi = &crtc_state->crtc_roi;
	lm_bounds = &crtc_state->lm_bounds[lm_idx];
	lm_roi = &crtc_state->lm_roi[lm_idx];

	if (sde_kms_rect_is_null(crtc_roi))
		memcpy(lm_roi, lm_bounds, sizeof(*lm_roi));
	else
		sde_kms_rect_intersect(crtc_roi, lm_bounds, lm_roi);

	SDE_DEBUG("%s: lm%d roi (%d,%d,%d,%d)\n", sde_crtc->name, lm_idx,
			lm_roi->x, lm_roi->y, lm_roi->w, lm_roi->h);

	/* if any dimension is zero, clear all dimensions for clarity */
	if (sde_kms_rect_is_null(lm_roi))
		memset(lm_roi, 0, sizeof(*lm_roi));

	return 0;
}

static u32 _sde_crtc_get_displays_affected(struct drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *crtc_state;
	u32 disp_bitmask = 0;
	int i;

	sde_crtc = to_sde_crtc(crtc);
	crtc_state = to_sde_crtc_state(state);

	/* pingpong split: one ROI, one LM, two physical displays */
	if (crtc_state->is_ppsplit) {
		u32 lm_split_width = crtc_state->lm_bounds[0].w / 2;
		struct sde_rect *roi = &crtc_state->lm_roi[0];

		if (sde_kms_rect_is_null(roi))
			disp_bitmask = 0;
		else if ((u32)roi->x + (u32)roi->w <= lm_split_width)
			disp_bitmask = BIT(0);		/* left only */
		else if (roi->x >= lm_split_width)
			disp_bitmask = BIT(1);		/* right only */
		else
			disp_bitmask = BIT(0) | BIT(1); /* left and right */
	} else {
		for (i = 0; i < sde_crtc->num_mixers; i++) {
			if (!sde_kms_rect_is_null(&crtc_state->lm_roi[i]))
				disp_bitmask |= BIT(i);
		}
	}

	SDE_DEBUG("affected displays 0x%x\n", disp_bitmask);

	return disp_bitmask;
}

static int _sde_crtc_check_rois_centered_and_symmetric(struct drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *crtc_state;
	const struct sde_rect *roi[CRTC_DUAL_MIXERS];

	if (!crtc || !state)
		return -EINVAL;

	sde_crtc = to_sde_crtc(crtc);
	crtc_state = to_sde_crtc_state(state);

	if (sde_crtc->num_mixers > CRTC_DUAL_MIXERS) {
		SDE_ERROR("%s: unsupported number of mixers: %d\n",
				sde_crtc->name, sde_crtc->num_mixers);
		return -EINVAL;
	}

	/*
	 * If using pingpong split: one ROI, one LM, two physical displays
	 * then the ROI must be centered on the panel split boundary and
	 * be of equal width across the split.
	 */
	if (crtc_state->is_ppsplit) {
		u16 panel_split_width;
		u32 display_mask;

		roi[0] = &crtc_state->lm_roi[0];

		if (sde_kms_rect_is_null(roi[0]))
			return 0;

		display_mask = _sde_crtc_get_displays_affected(crtc, state);
		if (display_mask != (BIT(0) | BIT(1)))
			return 0;

		panel_split_width = crtc_state->lm_bounds[0].w / 2;
		if (roi[0]->x + roi[0]->w / 2 != panel_split_width) {
			SDE_ERROR("%s: roi x %d w %d split %d\n",
					sde_crtc->name, roi[0]->x, roi[0]->w,
					panel_split_width);
			return -EINVAL;
		}

		return 0;
	}

	/*
	 * On certain HW, if using 2 LM, ROIs must be split evenly between the
	 * LMs and be of equal width.
	 */
	if (sde_crtc->num_mixers == 1)
		return 0;

	roi[0] = &crtc_state->lm_roi[0];
	roi[1] = &crtc_state->lm_roi[1];

	/* if one of the roi is null it's a left/right-only update */
	if (sde_kms_rect_is_null(roi[0]) || sde_kms_rect_is_null(roi[1]))
		return 0;

	/* check lm rois are equal width & first roi ends at 2nd roi */
	if (roi[0]->x + roi[0]->w != roi[1]->x || roi[0]->w != roi[1]->w) {
		SDE_ERROR(
			"%s: rois not centered and symmetric: roi0 x %d w %d roi1 x %d w %d\n",
				sde_crtc->name, roi[0]->x, roi[0]->w,
				roi[1]->x, roi[1]->w);
		return -EINVAL;
	}

	return 0;
}

static int _sde_crtc_check_planes_within_crtc_roi(struct drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *crtc_state;
	const struct sde_rect *crtc_roi;
	struct drm_plane_state *pstate;
	struct drm_plane *plane;

	if (!crtc || !state)
		return -EINVAL;

	/*
	 * Reject commit if a Plane CRTC destination coordinates fall outside
	 * the partial CRTC ROI. LM output is determined via connector ROIs,
	 * if they are specified, not Plane CRTC ROIs.
	 */

	sde_crtc = to_sde_crtc(crtc);
	crtc_state = to_sde_crtc_state(state);
	crtc_roi = &crtc_state->crtc_roi;

	if (sde_kms_rect_is_null(crtc_roi))
		return 0;

	drm_atomic_crtc_state_for_each_plane(plane, state) {
		struct sde_rect plane_roi, intersection;

		pstate = drm_atomic_get_plane_state(state->state, plane);
		if (IS_ERR_OR_NULL(pstate)) {
			int rc = PTR_ERR(pstate);

			SDE_ERROR("%s: failed to get plane%d state, %d\n",
					sde_crtc->name, plane->base.id, rc);
			return rc;
		}

		plane_roi.x = pstate->crtc_x;
		plane_roi.y = pstate->crtc_y;
		plane_roi.w = pstate->crtc_w;
		plane_roi.h = pstate->crtc_h;
		sde_kms_rect_intersect(crtc_roi, &plane_roi, &intersection);
		if (!sde_kms_rect_is_equal(&plane_roi, &intersection)) {
			SDE_ERROR(
				"%s: plane%d crtc roi (%d,%d,%d,%d) outside crtc roi (%d,%d,%d,%d)\n",
					sde_crtc->name, plane->base.id,
					plane_roi.x, plane_roi.y,
					plane_roi.w, plane_roi.h,
					crtc_roi->x, crtc_roi->y,
					crtc_roi->w, crtc_roi->h);
			return -E2BIG;
		}
	}

	return 0;
}

static int _sde_crtc_check_rois(struct drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	struct sde_crtc *sde_crtc;
	int lm_idx;
	int rc;

	if (!crtc || !state)
		return -EINVAL;

	sde_crtc = to_sde_crtc(crtc);

	rc = _sde_crtc_set_crtc_roi(crtc, state);
	if (rc)
		return rc;

	rc = _sde_crtc_check_autorefresh(crtc, state);
	if (rc)
		return rc;

	for (lm_idx = 0; lm_idx < sde_crtc->num_mixers; lm_idx++) {
		rc = _sde_crtc_set_lm_roi(crtc, state, lm_idx);
		if (rc)
			return rc;
	}

	rc = _sde_crtc_check_rois_centered_and_symmetric(crtc, state);
	if (rc)
		return rc;

	rc = _sde_crtc_check_planes_within_crtc_roi(crtc, state);
	if (rc)
		return rc;

	return 0;
}

static void _sde_crtc_program_lm_output_roi(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *crtc_state;
	const struct sde_rect *lm_roi;
	struct sde_hw_mixer *hw_lm;
	int lm_idx, lm_horiz_position;

	if (!crtc)
		return;

	sde_crtc = to_sde_crtc(crtc);
	crtc_state = to_sde_crtc_state(crtc->state);

	lm_horiz_position = 0;
	for (lm_idx = 0; lm_idx < sde_crtc->num_mixers; lm_idx++) {
		struct sde_hw_mixer_cfg cfg;

		lm_roi = &crtc_state->lm_roi[lm_idx];
		hw_lm = sde_crtc->mixers[lm_idx].hw_lm;

		SDE_EVT32(DRMID(crtc_state->base.crtc), lm_idx,
			lm_roi->x, lm_roi->y, lm_roi->w, lm_roi->h);

		if (sde_kms_rect_is_null(lm_roi))
			continue;

		hw_lm->cfg.out_width = lm_roi->w;
		hw_lm->cfg.out_height = lm_roi->h;
		hw_lm->cfg.right_mixer = lm_horiz_position;

		cfg.out_width = lm_roi->w;
		cfg.out_height = lm_roi->h;
		cfg.right_mixer = lm_horiz_position++;
		cfg.flags = 0;
		hw_lm->ops.setup_mixer_out(hw_lm, &cfg);
	}
}

static void _sde_crtc_blend_setup_mixer(struct drm_crtc *crtc,
	struct sde_crtc *sde_crtc, struct sde_crtc_mixer *mixer)
{
	struct drm_plane *plane;
	struct drm_framebuffer *fb;
	struct drm_plane_state *state;
	struct sde_crtc_state *cstate;
	struct sde_plane_state *pstate = NULL;
	struct sde_format *format;
	struct sde_hw_ctl *ctl;
	struct sde_hw_mixer *lm;
	struct sde_hw_stage_cfg *stage_cfg;
	struct sde_rect plane_crtc_roi;

	u32 flush_mask = 0;
	uint32_t lm_idx = LEFT_MIXER, stage_idx;
	bool bg_alpha_enable[CRTC_DUAL_MIXERS] = {false};
	int zpos_cnt[CRTC_DUAL_MIXERS][SDE_STAGE_MAX + 1] = { {0} };
	int i;
	bool sbuf_mode = false;
	u32 prefill = 0;

	if (!sde_crtc || !mixer) {
		SDE_ERROR("invalid sde_crtc or mixer\n");
		return;
	}

	ctl = mixer->hw_ctl;
	lm = mixer->hw_lm;
	stage_cfg = &sde_crtc->stage_cfg;
	cstate = to_sde_crtc_state(crtc->state);

	drm_atomic_crtc_for_each_plane(plane, crtc) {
		state = plane->state;
		if (!state)
			continue;

		plane_crtc_roi.x = state->crtc_x;
		plane_crtc_roi.y = state->crtc_y;
		plane_crtc_roi.w = state->crtc_w;
		plane_crtc_roi.h = state->crtc_h;

		pstate = to_sde_plane_state(state);
		fb = state->fb;

		if (sde_plane_is_sbuf_mode(plane, &prefill))
			sbuf_mode = true;

		sde_plane_get_ctl_flush(plane, ctl, &flush_mask);


		SDE_DEBUG("crtc %d stage:%d - plane %d sspp %d fb %d\n",
				crtc->base.id,
				pstate->stage,
				plane->base.id,
				sde_plane_pipe(plane) - SSPP_VIG0,
				state->fb ? state->fb->base.id : -1);

		format = to_sde_format(msm_framebuffer_format(pstate->base.fb));

		SDE_EVT32(DRMID(crtc), DRMID(plane),
				state->fb ? state->fb->base.id : -1,
				state->src_x >> 16, state->src_y >> 16,
				state->src_w >> 16, state->src_h >> 16,
				state->crtc_x, state->crtc_y,
				state->crtc_w, state->crtc_h);

		for (lm_idx = 0; lm_idx < sde_crtc->num_mixers; lm_idx++) {
			struct sde_rect intersect;

			/* skip if the roi doesn't fall within LM's bounds */
			sde_kms_rect_intersect(&plane_crtc_roi,
					&cstate->lm_bounds[lm_idx],
					&intersect);
			if (sde_kms_rect_is_null(&intersect))
				continue;

			stage_idx = zpos_cnt[lm_idx][pstate->stage]++;
			stage_cfg->stage[lm_idx][pstate->stage][stage_idx] =
					sde_plane_pipe(plane);
			stage_cfg->multirect_index
					[lm_idx][pstate->stage][stage_idx] =
					pstate->multirect_index;

			mixer[lm_idx].flush_mask |= flush_mask;


			SDE_EVT32(DRMID(plane), DRMID(crtc), lm_idx, stage_idx,
				pstate->stage, pstate->multirect_index,
				pstate->multirect_mode,
				format->base.pixel_format,
				fb ? fb->modifier[0] : 0);

			/* blend config update */
			if (pstate->stage != SDE_STAGE_BASE) {
				_sde_crtc_setup_blend_cfg(mixer + lm_idx,
						pstate, format);

				if (bg_alpha_enable[lm_idx] &&
						!format->alpha_enable)
					mixer[lm_idx].mixer_op_mode = 0;
				else
					mixer[lm_idx].mixer_op_mode |=
						1 << pstate->stage;
			} else if (format->alpha_enable) {
				bg_alpha_enable[lm_idx] = true;
			}
		}
	}

	if (lm && lm->ops.setup_dim_layer) {
		cstate = to_sde_crtc_state(crtc->state);
		for (i = 0; i < cstate->num_dim_layers; i++)
			_sde_crtc_setup_dim_layer_cfg(crtc, sde_crtc,
					mixer, &cstate->dim_layer[i]);
	}

	if (ctl->ops.setup_sbuf_cfg) {
		cstate = to_sde_crtc_state(crtc->state);
		if (!sbuf_mode) {
			cstate->sbuf_cfg.rot_op_mode =
					SDE_CTL_ROT_OP_MODE_OFFLINE;
			cstate->sbuf_prefill_line = 0;
		} else {
			cstate->sbuf_cfg.rot_op_mode =
					SDE_CTL_ROT_OP_MODE_INLINE_SYNC;
			cstate->sbuf_prefill_line = prefill;
		}

		ctl->ops.setup_sbuf_cfg(ctl, &cstate->sbuf_cfg);
	}

	_sde_crtc_program_lm_output_roi(crtc);
}

static void _sde_crtc_swap_mixers_for_right_partial_update(
		struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate;
	struct drm_encoder *drm_enc;
	bool is_right_only;
	bool encoder_in_dsc_merge = false;

	if (!crtc || !crtc->state)
		return;

	sde_crtc = to_sde_crtc(crtc);
	cstate = to_sde_crtc_state(crtc->state);

	if (sde_crtc->num_mixers != CRTC_DUAL_MIXERS)
		return;

	drm_for_each_encoder(drm_enc, crtc->dev) {
		if (drm_enc->crtc == crtc &&
				sde_encoder_is_dsc_merge(drm_enc)) {
			encoder_in_dsc_merge = true;
			break;
		}
	}

	/**
	 * For right-only partial update with DSC merge, we swap LM0 & LM1.
	 * This is due to two reasons:
	 * - On 8996, there is a DSC HW requirement that in DSC Merge Mode,
	 *   the left DSC must be used, right DSC cannot be used alone.
	 *   For right-only partial update, this means swap layer mixers to map
	 *   Left LM to Right INTF. On later HW this was relaxed.
	 * - In DSC Merge mode, the physical encoder has already registered
	 *   PP0 as the master, to switch to right-only we would have to
	 *   reprogram to be driven by PP1 instead.
	 * To support both cases, we prefer to support the mixer swap solution.
	 */
	if (!encoder_in_dsc_merge)
		return;

	is_right_only = sde_kms_rect_is_null(&cstate->lm_roi[0]) &&
			!sde_kms_rect_is_null(&cstate->lm_roi[1]);

	if (is_right_only && !sde_crtc->mixers_swapped) {
		/* right-only update swap mixers */
		swap(sde_crtc->mixers[0], sde_crtc->mixers[1]);
		sde_crtc->mixers_swapped = true;
	} else if (!is_right_only && sde_crtc->mixers_swapped) {
		/* left-only or full update, swap back */
		swap(sde_crtc->mixers[0], sde_crtc->mixers[1]);
		sde_crtc->mixers_swapped = false;
	}

	SDE_DEBUG("%s: right_only %d swapped %d, mix0->lm%d, mix1->lm%d\n",
			sde_crtc->name, is_right_only, sde_crtc->mixers_swapped,
			sde_crtc->mixers[0].hw_lm->idx - LM_0,
			sde_crtc->mixers[1].hw_lm->idx - LM_0);
	SDE_EVT32(DRMID(crtc), is_right_only, sde_crtc->mixers_swapped,
			sde_crtc->mixers[0].hw_lm->idx - LM_0,
			sde_crtc->mixers[1].hw_lm->idx - LM_0);
}

/**
 * _sde_crtc_blend_setup - configure crtc mixers
 * @crtc: Pointer to drm crtc structure
 */
static void _sde_crtc_blend_setup(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *sde_crtc_state;
	struct sde_crtc_mixer *mixer;
	struct sde_hw_ctl *ctl;
	struct sde_hw_mixer *lm;

	int i;

	if (!crtc)
		return;

	sde_crtc = to_sde_crtc(crtc);
	sde_crtc_state = to_sde_crtc_state(crtc->state);
	mixer = sde_crtc->mixers;

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

		/* clear dim_layer settings */
		lm = mixer[i].hw_lm;
		if (lm->ops.clear_dim_layer)
			lm->ops.clear_dim_layer(lm);
	}

	_sde_crtc_swap_mixers_for_right_partial_update(crtc);

	/* initialize stage cfg */
	memset(&sde_crtc->stage_cfg, 0, sizeof(struct sde_hw_stage_cfg));

	_sde_crtc_blend_setup_mixer(crtc, sde_crtc, mixer);

	for (i = 0; i < sde_crtc->num_mixers; i++) {
		const struct sde_rect *lm_roi = &sde_crtc_state->lm_roi[i];

		ctl = mixer[i].hw_ctl;
		lm = mixer[i].hw_lm;

		if (sde_kms_rect_is_null(lm_roi)) {
			SDE_DEBUG(
				"%s: lm%d leave ctl%d mask 0 since null roi\n",
					sde_crtc->name, lm->idx - LM_0,
					ctl->idx - CTL_0);
			continue;
		}

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

	_sde_crtc_program_lm_output_roi(crtc);
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
	SDE_EVT32_VERBOSE(DRMID(crtc));

	/* identify connectors attached to this crtc */
	cstate->num_connectors = 0;

	drm_for_each_connector(conn, crtc->dev)
		if (conn->state && conn->state->crtc == crtc &&
				cstate->num_connectors < MAX_CONNECTORS) {
			cstate->connectors[cstate->num_connectors++] = conn;
			sde_connector_prepare_fence(conn);
		}

	/* prepare main output fence */
	sde_fence_prepare(&sde_crtc->output_fence);
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

	/* keep statistics on vblank callback - with auto reset via debugfs */
	if (ktime_equal(sde_crtc->vblank_cb_time, ktime_set(0, 0)))
		sde_crtc->vblank_cb_time = ktime_get();
	else
		sde_crtc->vblank_cb_count++;
	_sde_crtc_complete_flip(crtc, NULL);
	drm_crtc_handle_vblank(crtc);
	DRM_DEBUG_VBL("crtc%d\n", crtc->base.id);
	SDE_EVT32_VERBOSE(DRMID(crtc));
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
	bool disable_inprogress = false;

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
	disable_inprogress = fevent->event &
					SDE_ENCODER_FRAME_EVENT_DURING_DISABLE;
	fevent->event &= ~SDE_ENCODER_FRAME_EVENT_DURING_DISABLE;

	if (fevent->event == SDE_ENCODER_FRAME_EVENT_DONE ||
			(fevent->event & SDE_ENCODER_FRAME_EVENT_ERROR) ||
			(fevent->event & SDE_ENCODER_FRAME_EVENT_PANEL_DEAD)) {

		if (atomic_read(&sde_crtc->frame_pending) < 1) {
			/* this should not happen */
			SDE_ERROR("crtc%d ts:%lld invalid frame_pending:%d\n",
					crtc->base.id,
					ktime_to_ns(fevent->ts),
					atomic_read(&sde_crtc->frame_pending));
			SDE_EVT32(DRMID(crtc), fevent->event,
							SDE_EVTLOG_FUNC_CASE1);
		} else if (atomic_dec_return(&sde_crtc->frame_pending) == 0) {
			/* release bandwidth and other resources */
			SDE_DEBUG("crtc%d ts:%lld last pending\n",
					crtc->base.id,
					ktime_to_ns(fevent->ts));
			SDE_EVT32(DRMID(crtc), fevent->event,
							SDE_EVTLOG_FUNC_CASE2);
			if (!disable_inprogress)
				sde_core_perf_crtc_release_bw(crtc);
		} else {
			SDE_EVT32_VERBOSE(DRMID(crtc), fevent->event,
							SDE_EVTLOG_FUNC_CASE3);
		}

		if (fevent->event == SDE_ENCODER_FRAME_EVENT_DONE &&
							!disable_inprogress)
			sde_core_perf_crtc_update(crtc, 0, false);
	} else {
		SDE_ERROR("crtc%d ts:%lld unknown event %u\n", crtc->base.id,
				ktime_to_ns(fevent->ts),
				fevent->event);
		SDE_EVT32(DRMID(crtc), fevent->event, SDE_EVTLOG_FUNC_CASE4);
	}

	if (fevent->event & SDE_ENCODER_FRAME_EVENT_PANEL_DEAD)
		SDE_ERROR("crtc%d ts:%lld received panel dead event\n",
				crtc->base.id, ktime_to_ns(fevent->ts));

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
	SDE_EVT32_VERBOSE(DRMID(crtc), event);

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
	if (event & SDE_ENCODER_FRAME_EVENT_DURING_DISABLE)
		sde_crtc_frame_event_work(&fevent->work);
	else
		kthread_queue_work(&priv->disp_thread[pipe_id].worker,
								&fevent->work);
}

void sde_crtc_complete_commit(struct drm_crtc *crtc,
		struct drm_crtc_state *old_state)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate;
	int i;

	if (!crtc || !crtc->state) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
	cstate = to_sde_crtc_state(crtc->state);
	SDE_EVT32_VERBOSE(DRMID(crtc));

	/* signal output fence(s) at end of commit */
	sde_fence_signal(&sde_crtc->output_fence, 0);

	for (i = 0; i < cstate->num_connectors; ++i)
		sde_connector_complete_commit(cstate->connectors[i]);
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
 * _sde_crtc_set_dim_layer_v1 - copy dim layer settings from userspace
 * @cstate:      Pointer to sde crtc state
 * @user_ptr:    User ptr for sde_drm_dim_layer_v1 struct
 */
static void _sde_crtc_set_dim_layer_v1(struct sde_crtc_state *cstate,
		void *usr_ptr)
{
	struct sde_drm_dim_layer_v1 dim_layer_v1;
	struct sde_drm_dim_layer_cfg *user_cfg;
	struct sde_hw_dim_layer *dim_layer;
	u32 count, i;

	if (!cstate) {
		SDE_ERROR("invalid cstate\n");
		return;
	}
	dim_layer = cstate->dim_layer;

	if (!usr_ptr) {
		SDE_DEBUG("dim_layer data removed\n");
		return;
	}

	if (copy_from_user(&dim_layer_v1, usr_ptr, sizeof(dim_layer_v1))) {
		SDE_ERROR("failed to copy dim_layer data\n");
		return;
	}

	count = dim_layer_v1.num_layers;
	if (count > SDE_MAX_DIM_LAYERS) {
		SDE_ERROR("invalid number of dim_layers:%d", count);
		return;
	}

	/* populate from user space */
	cstate->num_dim_layers = count;
	for (i = 0; i < count; i++) {
		user_cfg = &dim_layer_v1.layer_cfg[i];

		dim_layer[i].flags = user_cfg->flags;
		dim_layer[i].stage = user_cfg->stage + SDE_STAGE_0;

		dim_layer[i].rect.x = user_cfg->rect.x1;
		dim_layer[i].rect.y = user_cfg->rect.y1;
		dim_layer[i].rect.w = user_cfg->rect.x2 - user_cfg->rect.x1;
		dim_layer[i].rect.h = user_cfg->rect.y2 - user_cfg->rect.y1;

		dim_layer[i].color_fill = (struct sde_mdss_color) {
				user_cfg->color_fill.color_0,
				user_cfg->color_fill.color_1,
				user_cfg->color_fill.color_2,
				user_cfg->color_fill.color_3,
		};

		SDE_DEBUG("dim_layer[%d] - flags:%d, stage:%d\n",
				i, dim_layer[i].flags, dim_layer[i].stage);
		SDE_DEBUG(" rect:{%d,%d,%d,%d}, color:{%d,%d,%d,%d}\n",
				dim_layer[i].rect.x, dim_layer[i].rect.y,
				dim_layer[i].rect.w, dim_layer[i].rect.h,
				dim_layer[i].color_fill.color_0,
				dim_layer[i].color_fill.color_1,
				dim_layer[i].color_fill.color_2,
				dim_layer[i].color_fill.color_3);
	}
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
	int rc = 0;

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
	 * if its fence has timed out. Call input fence wait multiple times if
	 * fence wait is interrupted due to interrupt call.
	 */
	SDE_ATRACE_BEGIN("plane_wait_input_fence");
	drm_atomic_crtc_for_each_plane(plane, crtc) {
		do {
			kt_wait = ktime_sub(kt_end, ktime_get());
			if (ktime_compare(kt_wait, ktime_set(0, 0)) >= 0)
				wait_ms = ktime_to_ms(kt_wait);
			else
				wait_ms = 0;

			rc = sde_plane_wait_input_fence(plane, wait_ms);
		} while (wait_ms && rc == -ERESTARTSYS);
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

static void _sde_crtc_setup_is_ppsplit(struct drm_crtc_state *state)
{
	int i;
	struct sde_crtc_state *cstate;

	cstate = to_sde_crtc_state(state);

	cstate->is_ppsplit = false;
	for (i = 0; i < cstate->num_connectors; i++) {
		struct drm_connector *conn = cstate->connectors[i];

		if (sde_connector_get_topology_name(conn) ==
				SDE_RM_TOPOLOGY_PPSPLIT)
			cstate->is_ppsplit = true;
	}
}

static void _sde_crtc_setup_lm_bounds(struct drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate;
	struct drm_display_mode *adj_mode;
	u32 crtc_split_width;
	int i;

	if (!crtc || !state) {
		SDE_ERROR("invalid args\n");
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
	cstate = to_sde_crtc_state(state);

	adj_mode = &state->adjusted_mode;
	crtc_split_width = sde_crtc_mixer_width(sde_crtc, adj_mode);

	for (i = 0; i < sde_crtc->num_mixers; i++) {
		cstate->lm_bounds[i].x = crtc_split_width * i;
		cstate->lm_bounds[i].y = 0;
		cstate->lm_bounds[i].w = crtc_split_width;
		cstate->lm_bounds[i].h = adj_mode->vdisplay;
		memcpy(&cstate->lm_roi[i], &cstate->lm_bounds[i],
				sizeof(cstate->lm_roi[i]));
		SDE_EVT32(DRMID(crtc), i,
				cstate->lm_bounds[i].x, cstate->lm_bounds[i].y,
				cstate->lm_bounds[i].w, cstate->lm_bounds[i].h);
		SDE_DEBUG("%s: lm%d bnd&roi (%d,%d,%d,%d)\n", sde_crtc->name, i,
				cstate->lm_roi[i].x, cstate->lm_roi[i].y,
				cstate->lm_roi[i].w, cstate->lm_roi[i].h);
	}

	drm_mode_debug_printmodeline(adj_mode);
}

static void sde_crtc_atomic_begin(struct drm_crtc *crtc,
		struct drm_crtc_state *old_state)
{
	struct sde_crtc *sde_crtc;
	struct drm_encoder *encoder;
	struct drm_device *dev;
	unsigned long flags;

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

	if (!sde_crtc->num_mixers) {
		_sde_crtc_setup_mixers(crtc);
		_sde_crtc_setup_is_ppsplit(crtc->state);
		_sde_crtc_setup_lm_bounds(crtc, crtc->state);
	}

	if (sde_crtc->event) {
		WARN_ON(sde_crtc->event);
	} else {
		spin_lock_irqsave(&dev->event_lock, flags);
		sde_crtc->event = crtc->state->event;
		spin_unlock_irqrestore(&dev->event_lock, flags);
	}

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (encoder->crtc != crtc)
			continue;

		/* encoder will trigger pending mask now */
		sde_encoder_trigger_kickoff_pending(encoder);
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
	struct drm_encoder *encoder;
	struct sde_crtc *sde_crtc;
	struct drm_device *dev;
	struct drm_plane *plane;
	unsigned long flags;
	struct sde_crtc_state *cstate;

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
	cstate = to_sde_crtc_state(crtc->state);
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

	if (!cstate->rsc_update) {
		drm_for_each_encoder(encoder, dev) {
			if (encoder->crtc != crtc)
				continue;

			cstate->rsc_client =
				sde_encoder_get_rsc_client(encoder);
		}
		cstate->rsc_update = true;
	}

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

	_sde_crtc_rp_destroy(&cstate->rp);

	__drm_atomic_helper_crtc_destroy_state(state);

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
	struct sde_crtc_state *cstate;

	if (!crtc) {
		SDE_ERROR("invalid argument\n");
		return;
	}
	dev = crtc->dev;
	sde_crtc = to_sde_crtc(crtc);
	sde_kms = _sde_crtc_get_kms(crtc);
	priv = sde_kms->dev->dev_private;
	cstate = to_sde_crtc_state(crtc->state);

	/*
	 * If no mixers has been allocated in sde_crtc_atomic_check(),
	 * it means we are trying to start a CRTC whose state is disabled:
	 * nothing else needs to be done.
	 */
	if (unlikely(!sde_crtc->num_mixers))
		return;

	SDE_ATRACE_BEGIN("crtc_commit");
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		struct sde_encoder_kickoff_params params = { 0 };

		if (encoder->crtc != crtc)
			continue;

		/*
		 * Encoder will flush/start now, unless it has a tx pending.
		 * If so, it may delay and flush at an irq event (e.g. ppdone)
		 */
		params.inline_rotate_prefill = cstate->sbuf_prefill_line;
		params.affected_displays = _sde_crtc_get_displays_affected(crtc,
				crtc->state);
		sde_encoder_prepare_for_kickoff(encoder, &params);
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
 */
static void _sde_crtc_vblank_enable_nolock(
		struct sde_crtc *sde_crtc, bool enable)
{
	struct drm_device *dev;
	struct drm_crtc *crtc;
	struct drm_encoder *enc;

	if (!sde_crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	crtc = &sde_crtc->base;
	dev = crtc->dev;

	if (enable) {
		int ret;

		/* drop lock since power crtc cb may try to re-acquire lock */
		mutex_unlock(&sde_crtc->crtc_lock);
		ret = _sde_crtc_power_enable(sde_crtc, true);
		mutex_lock(&sde_crtc->crtc_lock);
		if (ret)
			return;

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

		/* drop lock since power crtc cb may try to re-acquire lock */
		mutex_unlock(&sde_crtc->crtc_lock);
		_sde_crtc_power_enable(sde_crtc, false);
		mutex_lock(&sde_crtc->crtc_lock);
	}
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
	struct drm_event event;
	u32 power_on;

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

	event.type = DRM_EVENT_CRTC_POWER;
	event.length = sizeof(u32);
	/*
	 * Update CP on suspend/resume transitions
	 */
	if (enable && !sde_crtc->suspend) {
		sde_cp_crtc_suspend(crtc);
		power_on = 0;
	} else if (!enable && sde_crtc->suspend) {
		sde_cp_crtc_resume(crtc);
		power_on = 1;
	}

	/*
	 * If the vblank refcount != 0, release a power reference on suspend
	 * and take it back during resume (if it is still != 0).
	 */
	if (sde_crtc->suspend == enable)
		SDE_DEBUG("crtc%d suspend already set to %d, ignoring update\n",
				crtc->base.id, enable);
	else if (atomic_read(&sde_crtc->vblank_refcount) != 0)
		_sde_crtc_vblank_enable_nolock(sde_crtc, !enable);

	sde_crtc->suspend = enable;
	msm_mode_object_event_nofity(&crtc->base, crtc->dev, &event,
			(u8 *)&power_on);
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

	_sde_crtc_rp_duplicate(&old_cstate->rp, &cstate->rp);

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

	_sde_crtc_rp_reset(&cstate->rp);

	cstate->base.crtc = crtc;
	crtc->state = &cstate->base;
}

static int _sde_crtc_vblank_no_lock(struct sde_crtc *sde_crtc, bool en)
{
	if (!sde_crtc) {
		SDE_ERROR("invalid crtc\n");
		return -EINVAL;
	} else if (en && atomic_inc_return(&sde_crtc->vblank_refcount) == 1) {
		SDE_DEBUG("crtc%d vblank enable\n", sde_crtc->base.base.id);
		if (!sde_crtc->suspend)
			_sde_crtc_vblank_enable_nolock(sde_crtc, true);
	} else if (!en && atomic_read(&sde_crtc->vblank_refcount) < 1) {
		SDE_ERROR("crtc%d invalid vblank disable\n",
				sde_crtc->base.base.id);
		return -EINVAL;
	} else if (!en && atomic_dec_return(&sde_crtc->vblank_refcount) == 0) {
		SDE_DEBUG("crtc%d vblank disable\n", sde_crtc->base.base.id);
		if (!sde_crtc->suspend)
			_sde_crtc_vblank_enable_nolock(sde_crtc, false);
	} else {
		SDE_DEBUG("crtc%d vblank %s refcount:%d\n",
				sde_crtc->base.base.id,
				en ? "enable" : "disable",
				atomic_read(&sde_crtc->vblank_refcount));
	}

	return 0;
}

static void sde_crtc_handle_power_event(u32 event_type, void *arg)
{
	struct drm_crtc *crtc = arg;
	struct sde_crtc *sde_crtc;
	struct drm_encoder *encoder;

	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}
	sde_crtc = to_sde_crtc(crtc);

	mutex_lock(&sde_crtc->crtc_lock);

	SDE_EVT32(DRMID(crtc), event_type);

	if (event_type == SDE_POWER_EVENT_POST_ENABLE) {
		/* restore encoder; crtc will be programmed during commit */
		drm_for_each_encoder(encoder, crtc->dev) {
			if (encoder->crtc != crtc)
				continue;

			sde_encoder_virt_restore(encoder);
		}

	} else if (event_type == SDE_POWER_EVENT_POST_DISABLE) {
		struct drm_plane *plane;

		/*
		 * set revalidate flag in planes, so it will be re-programmed
		 * in the next frame update
		 */
		drm_atomic_crtc_for_each_plane(plane, crtc)
			sde_plane_set_revalidate(plane, true);
	}

	mutex_unlock(&sde_crtc->crtc_lock);
}

static void sde_crtc_disable(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate;
	struct drm_encoder *encoder;
	struct msm_drm_private *priv;
	unsigned long flags;
	struct sde_crtc_irq_info *node = NULL;
	int ret;

	if (!crtc || !crtc->dev || !crtc->dev->dev_private || !crtc->state) {
		SDE_ERROR("invalid crtc\n");
		return;
	}
	sde_crtc = to_sde_crtc(crtc);
	cstate = to_sde_crtc_state(crtc->state);
	priv = crtc->dev->dev_private;

	SDE_DEBUG("crtc%d\n", crtc->base.id);

	if (msm_is_suspend_state(crtc->dev))
		_sde_crtc_set_suspend(crtc, true);

	mutex_lock(&sde_crtc->crtc_lock);
	SDE_EVT32(DRMID(crtc));

	if (atomic_read(&sde_crtc->vblank_refcount) && !sde_crtc->suspend) {
		SDE_ERROR("crtc%d invalid vblank refcount\n",
				crtc->base.id);
		SDE_EVT32(DRMID(crtc), atomic_read(&sde_crtc->vblank_refcount),
							SDE_EVTLOG_FUNC_CASE1);
		while (atomic_read(&sde_crtc->vblank_refcount))
			if (_sde_crtc_vblank_no_lock(sde_crtc, false))
				break;
	}

	if (atomic_read(&sde_crtc->frame_pending)) {
		/* release bandwidth and other resources */
		SDE_ERROR("crtc%d invalid frame pending\n", crtc->base.id);
		SDE_EVT32(DRMID(crtc), atomic_read(&sde_crtc->frame_pending),
							SDE_EVTLOG_FUNC_CASE2);
		sde_core_perf_crtc_release_bw(crtc);
		atomic_set(&sde_crtc->frame_pending, 0);
	}

	sde_core_perf_crtc_update(crtc, 0, true);

	drm_for_each_encoder(encoder, crtc->dev) {
		if (encoder->crtc != crtc)
			continue;
		sde_encoder_register_frame_event_callback(encoder, NULL, NULL);
		cstate->rsc_client = NULL;
		cstate->rsc_update = false;
	}

	if (sde_crtc->power_event)
		sde_power_handle_unregister_event(&priv->phandle,
				sde_crtc->power_event);

	memset(sde_crtc->mixers, 0, sizeof(sde_crtc->mixers));
	sde_crtc->num_mixers = 0;

	spin_lock_irqsave(&sde_crtc->spin_lock, flags);
	list_for_each_entry(node, &sde_crtc->user_event_list, list) {
		ret = 0;
		if (node->func)
			ret = node->func(crtc, false, &node->irq);
		if (ret)
			SDE_ERROR("%s failed to disable event %x\n",
					sde_crtc->name, node->event);
	}
	spin_unlock_irqrestore(&sde_crtc->spin_lock, flags);

	mutex_unlock(&sde_crtc->crtc_lock);
}

static void sde_crtc_enable(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;
	struct drm_encoder *encoder;
	struct msm_drm_private *priv;
	unsigned long flags;
	struct sde_crtc_irq_info *node = NULL;
	int ret;

	if (!crtc || !crtc->dev || !crtc->dev->dev_private) {
		SDE_ERROR("invalid crtc\n");
		return;
	}
	priv = crtc->dev->dev_private;

	SDE_DEBUG("crtc%d\n", crtc->base.id);
	SDE_EVT32(DRMID(crtc));
	sde_crtc = to_sde_crtc(crtc);

	drm_for_each_encoder(encoder, crtc->dev) {
		if (encoder->crtc != crtc)
			continue;
		sde_encoder_register_frame_event_callback(encoder,
				sde_crtc_frame_event_cb, (void *)crtc);
	}

	spin_lock_irqsave(&sde_crtc->spin_lock, flags);
	list_for_each_entry(node, &sde_crtc->user_event_list, list) {
		ret = 0;
		if (node->func)
			ret = node->func(crtc, true, &node->irq);
		if (ret)
			SDE_ERROR("%s failed to enable event %x\n",
				sde_crtc->name, node->event);
	}
	spin_unlock_irqrestore(&sde_crtc->spin_lock, flags);

	sde_crtc->power_event = sde_power_handle_register_event(
		&priv->phandle,
		SDE_POWER_EVENT_POST_ENABLE | SDE_POWER_EVENT_POST_DISABLE,
		sde_crtc_handle_power_event, crtc, sde_crtc->name);
}

struct plane_state {
	struct sde_plane_state *sde_pstate;
	const struct drm_plane_state *drm_pstate;
	int stage;
	u32 pipe_id;
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

static int _sde_crtc_excl_rect_overlap_check(struct plane_state pstates[],
	int cnt, int curr_cnt, struct sde_rect *excl_rect, int z_pos)
{
	struct sde_rect dst_rect, intersect;
	int i, rc = -EINVAL;
	const struct drm_plane_state *pstate;

	/* start checking from next plane */
	for (i = curr_cnt; i < cnt; i++) {
		pstate = pstates[i].drm_pstate;
		POPULATE_RECT(&dst_rect, pstate->crtc_x, pstate->crtc_y,
				pstate->crtc_w, pstate->crtc_h, true);
		sde_kms_rect_intersect(&dst_rect, excl_rect, &intersect);

		if (intersect.w == excl_rect->w && intersect.h == excl_rect->h
				/* next plane may be on same z-order */
				&& z_pos != pstates[i].stage) {
			rc = 0;
			goto end;
		}
	}

	SDE_ERROR("excl rect does not find top overlapping rect\n");
end:
	return rc;
}

/* no input validation - caller API has all the checks */
static int _sde_crtc_excl_dim_layer_check(struct drm_crtc_state *state,
		struct plane_state pstates[], int cnt)
{
	struct sde_crtc_state *cstate = to_sde_crtc_state(state);
	struct drm_display_mode *mode = &state->adjusted_mode;
	const struct drm_plane_state *pstate;
	struct sde_plane_state *sde_pstate;
	int rc = 0, i;

	/* Check dim layer rect bounds and stage */
	for (i = 0; i < cstate->num_dim_layers; i++) {
		if ((CHECK_LAYER_BOUNDS(cstate->dim_layer[i].rect.y,
			cstate->dim_layer[i].rect.h, mode->vdisplay)) ||
		    (CHECK_LAYER_BOUNDS(cstate->dim_layer[i].rect.x,
			cstate->dim_layer[i].rect.w, mode->hdisplay)) ||
		    (cstate->dim_layer[i].stage >= SDE_STAGE_MAX) ||
		    (!cstate->dim_layer[i].rect.w) ||
		    (!cstate->dim_layer[i].rect.h)) {
			SDE_ERROR("invalid dim_layer:{%d,%d,%d,%d}, stage:%d\n",
					cstate->dim_layer[i].rect.x,
					cstate->dim_layer[i].rect.y,
					cstate->dim_layer[i].rect.w,
					cstate->dim_layer[i].rect.h,
					cstate->dim_layer[i].stage);
			SDE_ERROR("display: %dx%d\n", mode->hdisplay,
					mode->vdisplay);
			rc = -E2BIG;
			goto end;
		}
	}

	/* this is traversing on sorted z-order pstates */
	for (i = 0; i < cnt; i++) {
		pstate = pstates[i].drm_pstate;
		sde_pstate = to_sde_plane_state(pstate);
		if (sde_pstate->excl_rect.w && sde_pstate->excl_rect.h) {
			/* check overlap on all top z-order */
			rc = _sde_crtc_excl_rect_overlap_check(pstates, cnt,
			     i + 1, &sde_pstate->excl_rect, pstates[i].stage);
			if (rc)
				goto end;
		}
	}

end:
	return rc;
}

static int sde_crtc_atomic_check(struct drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	struct sde_crtc *sde_crtc;
	struct plane_state pstates[SDE_STAGE_MAX * 4];
	struct sde_crtc_state *cstate;

	const struct drm_plane_state *pstate;
	struct drm_plane *plane;
	struct drm_display_mode *mode;

	int cnt = 0, rc = 0, mixer_width, i, z_pos;

	struct sde_multirect_plane_states multirect_plane[SDE_STAGE_MAX * 2];
	int multirect_count = 0;
	const struct drm_plane_state *pipe_staged[SSPP_MAX];
	int left_zpos_cnt = 0, right_zpos_cnt = 0;

	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return -EINVAL;
	}

	sde_crtc = to_sde_crtc(crtc);
	cstate = to_sde_crtc_state(state);

	if (!state->enable || !state->active) {
		SDE_DEBUG("crtc%d -> enable %d, active %d, skip atomic_check\n",
				crtc->base.id, state->enable, state->active);
		goto end;
	}

	mode = &state->adjusted_mode;
	SDE_DEBUG("%s: check", sde_crtc->name);

	/* force a full mode set if active state changed */
	if (state->active_changed)
		state->mode_changed = true;

	memset(pipe_staged, 0, sizeof(pipe_staged));

	mixer_width = sde_crtc_mixer_width(sde_crtc, mode);

	_sde_crtc_setup_is_ppsplit(state);
	_sde_crtc_setup_lm_bounds(crtc, state);

	 /* get plane state for all drm planes associated with crtc state */
	drm_atomic_crtc_state_for_each_plane_state(plane, pstate, state) {
		if (IS_ERR_OR_NULL(pstate)) {
			rc = PTR_ERR(pstate);
			SDE_ERROR("%s: failed to get plane%d state, %d\n",
					sde_crtc->name, plane->base.id, rc);
			goto end;
		}
		if (cnt >= ARRAY_SIZE(pstates))
			continue;

		pstates[cnt].sde_pstate = to_sde_plane_state(pstate);
		pstates[cnt].drm_pstate = pstate;
		pstates[cnt].stage = sde_plane_get_property(
				pstates[cnt].sde_pstate, PLANE_PROP_ZPOS);
		pstates[cnt].pipe_id = sde_plane_pipe(plane);

		/* check dim layer stage with every plane */
		for (i = 0; i < cstate->num_dim_layers; i++) {
			if (pstates[cnt].stage == cstate->dim_layer[i].stage) {
				SDE_ERROR(
					"plane:%d/dim_layer:%i-same stage:%d\n",
					plane->base.id, i,
					cstate->dim_layer[i].stage);
				rc = -EINVAL;
				goto end;
			}
		}

		if (pipe_staged[pstates[cnt].pipe_id]) {
			multirect_plane[multirect_count].r0 =
				pipe_staged[pstates[cnt].pipe_id];
			multirect_plane[multirect_count].r1 = pstate;
			multirect_count++;

			pipe_staged[pstates[cnt].pipe_id] = NULL;
		} else {
			pipe_staged[pstates[cnt].pipe_id] = pstate;
		}

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

	for (i = 1; i < SSPP_MAX; i++) {
		if (pipe_staged[i]) {
			sde_plane_clear_multirect(pipe_staged[i]);

			if (is_sde_plane_virtual(pipe_staged[i]->plane)) {
				SDE_ERROR("invalid use of virtual plane: %d\n",
					pipe_staged[i]->plane->base.id);
				goto end;
			}
		}
	}

	/* assign mixer stages based on sorted zpos property */
	sort(pstates, cnt, sizeof(pstates[0]), pstate_cmp, NULL);

	rc = _sde_crtc_excl_dim_layer_check(state, pstates, cnt);
	if (rc)
		goto end;

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

	for (i = 0; i < multirect_count; i++) {
		if (sde_plane_validate_multirect_v2(&multirect_plane[i])) {
			SDE_ERROR(
			"multirect validation failed for planes (%d - %d)\n",
					multirect_plane[i].r0->plane->base.id,
					multirect_plane[i].r1->plane->base.id);
			rc = -EINVAL;
			goto end;
		}
	}

	rc = sde_core_perf_crtc_check(crtc, state);
	if (rc) {
		SDE_ERROR("crtc%d failed performance check %d\n",
				crtc->base.id, rc);
		goto end;
	}

	/*
	 * enforce pipe priority restrictions
	 * use pstates sorted by stage to check planes on same stage
	 * we assume that all pipes are in source split so its valid to compare
	 * without taking into account left/right mixer placement
	 */
	for (i = 1; i < cnt; i++) {
		struct plane_state *prv_pstate, *cur_pstate;
		int32_t prv_x, cur_x, prv_id, cur_id;

		prv_pstate = &pstates[i - 1];
		cur_pstate = &pstates[i];
		if (prv_pstate->stage != cur_pstate->stage)
			continue;

		prv_x = prv_pstate->drm_pstate->crtc_x;
		cur_x = cur_pstate->drm_pstate->crtc_x;
		prv_id = prv_pstate->sde_pstate->base.plane->base.id;
		cur_id = cur_pstate->sde_pstate->base.plane->base.id;

		/*
		 * Planes are enumerated in pipe-priority order such that planes
		 * with lower drm_id must be left-most in a shared blend-stage
		 * when using source split.
		 */
		if (cur_x > prv_x && cur_id < prv_id) {
			SDE_ERROR(
				"shared z_pos %d lower id plane%d @ x%d should be left of plane%d @ x %d\n",
				cur_pstate->stage, cur_id, cur_x,
				prv_id, prv_x);
			rc = -EINVAL;
			goto end;
		} else if (cur_x < prv_x && cur_id > prv_id) {
			SDE_ERROR(
				"shared z_pos %d lower id plane%d @ x%d should be left of plane%d @ x %d\n",
				cur_pstate->stage, prv_id, prv_x,
				cur_id, cur_x);
			rc = -EINVAL;
			goto end;
		}
	}

	rc = _sde_crtc_check_rois(crtc, state);
	if (rc) {
		SDE_ERROR("crtc%d failed roi check %d\n", crtc->base.id, rc);
		goto end;
	}

end:
	_sde_crtc_rp_free_unused(&cstate->rp);
	return rc;
}

int sde_crtc_vblank(struct drm_crtc *crtc, bool en)
{
	struct sde_crtc *sde_crtc;
	int rc;

	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return -EINVAL;
	}
	sde_crtc = to_sde_crtc(crtc);

	mutex_lock(&sde_crtc->crtc_lock);
	rc = _sde_crtc_vblank_no_lock(sde_crtc, en);
	mutex_unlock(&sde_crtc->crtc_lock);

	return rc;
}

void sde_crtc_cancel_pending_flip(struct drm_crtc *crtc, struct drm_file *file)
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
			catalog->perf.max_bw_high * 1000ULL,
			CRTC_PROP_CORE_AB);
	msm_property_install_range(&sde_crtc->property_info,
			"core_ib", 0x0, 0, U64_MAX,
			catalog->perf.max_bw_high * 1000ULL,
			CRTC_PROP_CORE_IB);
	msm_property_install_range(&sde_crtc->property_info,
			"mem_ab", 0x0, 0, U64_MAX,
			catalog->perf.max_bw_high * 1000ULL,
			CRTC_PROP_MEM_AB);
	msm_property_install_range(&sde_crtc->property_info,
			"mem_ib", 0x0, 0, U64_MAX,
			catalog->perf.max_bw_high * 1000ULL,
			CRTC_PROP_MEM_IB);
	msm_property_install_range(&sde_crtc->property_info,
			"rot_prefill_bw", 0, 0, U64_MAX,
			catalog->perf.max_bw_high * 1000ULL,
			CRTC_PROP_ROT_PREFILL_BW);
	msm_property_install_range(&sde_crtc->property_info,
			"rot_clk", 0, 0, U64_MAX,
			sde_kms->perf.max_core_clk_rate,
			CRTC_PROP_ROT_CLK);

	msm_property_install_blob(&sde_crtc->property_info, "capabilities",
		DRM_MODE_PROP_IMMUTABLE, CRTC_PROP_INFO);

	msm_property_install_volatile_range(&sde_crtc->property_info,
		"sde_drm_roi_v1", 0x0, 0, ~0, 0, CRTC_PROP_ROI_V1);

	sde_kms_info_reset(info);

	if (catalog->has_dim_layer) {
		msm_property_install_volatile_range(&sde_crtc->property_info,
			"dim_layer_v1", 0x0, 0, ~0, 0, CRTC_PROP_DIM_LAYER_V1);
		sde_kms_info_add_keyint(info, "dim_layer_v1_max_layers",
				SDE_MAX_DIM_LAYERS);
	}

	sde_kms_info_add_keyint(info, "hw_version", catalog->hwversion);
	sde_kms_info_add_keyint(info, "max_linewidth",
			catalog->max_mixer_width);
	sde_kms_info_add_keyint(info, "max_blendstages",
			catalog->max_mixer_blendstages);
	if (catalog->qseed_type == SDE_SSPP_SCALER_QSEED2)
		sde_kms_info_add_keystr(info, "qseed_type", "qseed2");
	if (catalog->qseed_type == SDE_SSPP_SCALER_QSEED3)
		sde_kms_info_add_keystr(info, "qseed_type", "qseed3");

	if (sde_is_custom_client()) {
		if (catalog->smart_dma_rev == SDE_SSPP_SMART_DMA_V1)
			sde_kms_info_add_keystr(info,
					"smart_dma_rev", "smart_dma_v1");
		if (catalog->smart_dma_rev == SDE_SSPP_SMART_DMA_V2)
			sde_kms_info_add_keystr(info,
					"smart_dma_rev", "smart_dma_v2");
	}

	sde_kms_info_add_keyint(info, "has_src_split", catalog->has_src_split);
	if (catalog->perf.max_bw_low)
		sde_kms_info_add_keyint(info, "max_bandwidth_low",
				catalog->perf.max_bw_low * 1000LL);
	if (catalog->perf.max_bw_high)
		sde_kms_info_add_keyint(info, "max_bandwidth_high",
				catalog->perf.max_bw_high * 1000LL);
	if (sde_kms->perf.max_core_clk_rate)
		sde_kms_info_add_keyint(info, "max_mdp_clk",
				sde_kms->perf.max_core_clk_rate);
	sde_kms_info_add_keystr(info, "core_ib_ff",
			catalog->perf.core_ib_ff);
	sde_kms_info_add_keystr(info, "core_clk_ff",
			catalog->perf.core_clk_ff);
	sde_kms_info_add_keystr(info, "comp_ratio_rt",
			catalog->perf.comp_ratio_rt);
	sde_kms_info_add_keystr(info, "comp_ratio_nrt",
			catalog->perf.comp_ratio_nrt);
	sde_kms_info_add_keyint(info, "dest_scale_prefill_lines",
			catalog->perf.dest_scale_prefill_lines);
	sde_kms_info_add_keyint(info, "undersized_prefill_lines",
			catalog->perf.undersized_prefill_lines);
	sde_kms_info_add_keyint(info, "macrotile_prefill_lines",
			catalog->perf.macrotile_prefill_lines);
	sde_kms_info_add_keyint(info, "yuv_nv12_prefill_lines",
			catalog->perf.yuv_nv12_prefill_lines);
	sde_kms_info_add_keyint(info, "linear_prefill_lines",
			catalog->perf.linear_prefill_lines);
	sde_kms_info_add_keyint(info, "downscaling_prefill_lines",
			catalog->perf.downscaling_prefill_lines);
	sde_kms_info_add_keyint(info, "xtra_prefill_lines",
			catalog->perf.xtra_prefill_lines);
	sde_kms_info_add_keyint(info, "amortizable_threshold",
			catalog->perf.amortizable_threshold);
	sde_kms_info_add_keyint(info, "min_prefill_lines",
			catalog->perf.min_prefill_lines);

	msm_property_set_blob(&sde_crtc->property_info, &sde_crtc->blob_info,
			info->data, info->len, CRTC_PROP_INFO);

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
			switch (idx) {
			case CRTC_PROP_INPUT_FENCE_TIMEOUT:
				_sde_crtc_set_input_fence_timeout(cstate);
				break;
			case CRTC_PROP_DIM_LAYER_V1:
				_sde_crtc_set_dim_layer_v1(cstate, (void *)val);
				break;
			case CRTC_PROP_ROI_V1:
				ret = _sde_crtc_set_roi_v1(state, (void *)val);
				break;
			case CRTC_PROP_CORE_AB:
			case CRTC_PROP_CORE_IB:
			case CRTC_PROP_MEM_AB:
			case CRTC_PROP_MEM_IB:
				cstate->bw_control = true;
				break;
			default:
				/* nothing to do */
				break;
			}
		} else {
			ret = sde_cp_crtc_set_property(crtc,
					property, val);
		}
		if (ret)
			DRM_ERROR("failed to set the property\n");

		SDE_DEBUG("crtc%d %s[%d] <= 0x%llx ret=%d\n", crtc->base.id,
				property->name, property->base.id, val, ret);
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
	bool conn_offset = 0;

	if (!crtc || !state) {
		SDE_ERROR("invalid argument(s)\n");
	} else {
		sde_crtc = to_sde_crtc(crtc);
		cstate = to_sde_crtc_state(state);

		for (i = 0; i < cstate->num_connectors; ++i) {
			conn_offset = sde_connector_needs_offset(
						cstate->connectors[i]);
			if (conn_offset)
				break;
		}

		i = msm_property_index(&sde_crtc->property_info, property);
		if (i == CRTC_PROP_OUTPUT_FENCE) {
			uint32_t offset = sde_crtc_get_property(cstate,
					CRTC_PROP_OUTPUT_FENCE_OFFSET);

			ret = sde_fence_create(&sde_crtc->output_fence, val,
							offset + conn_offset);
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
	struct sde_crtc_state *cstate;

	int i, out_width;

	if (!s || !s->private)
		return -EINVAL;

	sde_crtc = s->private;
	crtc = &sde_crtc->base;
	cstate = to_sde_crtc_state(crtc->state);

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

	for (i = 0; i < cstate->num_dim_layers; i++) {
		struct sde_hw_dim_layer *dim_layer = &cstate->dim_layer[i];

		seq_printf(s, "\tdim_layer:%d] stage:%d flags:%d\n",
				i, dim_layer->stage, dim_layer->flags);
		seq_printf(s, "\tdst_x:%d dst_y:%d dst_w:%d dst_h:%d\n",
				dim_layer->rect.x, dim_layer->rect.y,
				dim_layer->rect.w, dim_layer->rect.h);
		seq_printf(s,
			"\tcolor_0:%d color_1:%d color_2:%d color_3:%d\n",
				dim_layer->color_fill.color_0,
				dim_layer->color_fill.color_1,
				dim_layer->color_fill.color_2,
				dim_layer->color_fill.color_3);
		seq_puts(s, "\n");
	}

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
		seq_printf(s, "\tmultirect: mode: %d index: %d\n",
			pstate->multirect_mode, pstate->multirect_index);

		seq_printf(s, "\texcl_rect: x:%4d y:%4d w:%4d h:%4d\n",
			pstate->excl_rect.x, pstate->excl_rect.y,
			pstate->excl_rect.w, pstate->excl_rect.h);

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

	seq_printf(s, "vblank_refcount:%d\n",
			atomic_read(&sde_crtc->vblank_refcount));

	mutex_unlock(&sde_crtc->crtc_lock);

	return 0;
}

static int _sde_debugfs_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, _sde_debugfs_status_show, inode->i_private);
}

static ssize_t _sde_crtc_misr_setup(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_mixer *m;
	int i = 0, rc;
	char buf[MISR_BUFF_SIZE + 1];
	u32 frame_count, enable;
	size_t buff_copy;

	if (!file || !file->private_data)
		return -EINVAL;

	sde_crtc = file->private_data;
	buff_copy = min_t(size_t, count, MISR_BUFF_SIZE);
	if (copy_from_user(buf, user_buf, buff_copy)) {
		SDE_ERROR("buffer copy failed\n");
		return -EINVAL;
	}

	buf[buff_copy] = 0; /* end of string */

	if (sscanf(buf, "%u %u", &enable, &frame_count) != 2)
		return -EINVAL;

	rc = _sde_crtc_power_enable(sde_crtc, true);
	if (rc)
		return rc;

	mutex_lock(&sde_crtc->crtc_lock);
	sde_crtc->misr_enable = enable;
	for (i = 0; i < sde_crtc->num_mixers; ++i) {
		m = &sde_crtc->mixers[i];
		if (!m->hw_lm)
			continue;

		m->hw_lm->ops.setup_misr(m->hw_lm, enable, frame_count);
	}
	mutex_unlock(&sde_crtc->crtc_lock);
	_sde_crtc_power_enable(sde_crtc, false);

	return count;
}

static ssize_t _sde_crtc_misr_read(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_mixer *m;
	int i = 0, rc;
	ssize_t len = 0;
	char buf[MISR_BUFF_SIZE + 1] = {'\0'};

	if (*ppos)
		return 0;

	if (!file || !file->private_data)
		return -EINVAL;

	sde_crtc = file->private_data;
	rc = _sde_crtc_power_enable(sde_crtc, true);
	if (rc)
		return rc;

	mutex_lock(&sde_crtc->crtc_lock);
	if (!sde_crtc->misr_enable) {
		len += snprintf(buf + len, MISR_BUFF_SIZE - len,
			"disabled\n");
		goto buff_check;
	}

	for (i = 0; i < sde_crtc->num_mixers; ++i) {
		m = &sde_crtc->mixers[i];
		if (!m->hw_lm)
			continue;

		len += snprintf(buf + len, MISR_BUFF_SIZE - len, "lm idx:%d\n",
					m->hw_lm->idx - LM_0);
		len += snprintf(buf + len, MISR_BUFF_SIZE - len, "0x%x\n",
				m->hw_lm->ops.collect_misr(m->hw_lm));
	}

buff_check:
	if (count <= len) {
		len = 0;
		goto end;
	}

	if (copy_to_user(user_buff, buf, len)) {
		len = -EFAULT;
		goto end;
	}

	*ppos += len;   /* increase offset */

end:
	mutex_unlock(&sde_crtc->crtc_lock);
	_sde_crtc_power_enable(sde_crtc, false);
	return len;
}

#define DEFINE_SDE_DEBUGFS_SEQ_FOPS(__prefix)                          \
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
	struct sde_crtc_state *cstate = to_sde_crtc_state(crtc->state);
	struct sde_crtc_res *res;

	seq_printf(s, "num_connectors: %d\n", cstate->num_connectors);
	seq_printf(s, "client type: %d\n", sde_crtc_get_client_type(crtc));
	seq_printf(s, "intf_mode: %d\n", sde_crtc_get_intf_mode(crtc));
	seq_printf(s, "bw_ctl: %llu\n", cstate->cur_perf.bw_ctl);
	seq_printf(s, "core_clk_rate: %llu\n", cstate->cur_perf.core_clk_rate);
	seq_printf(s, "max_per_pipe_ib: %llu\n",
			cstate->cur_perf.max_per_pipe_ib);

	seq_printf(s, "rp.%d: ", cstate->rp.sequence_id);
	list_for_each_entry(res, &cstate->rp.res_list, list)
		seq_printf(s, "0x%x/0x%llx/%pK/%d ",
				res->type, res->tag, res->val,
				atomic_read(&res->refcount));
	seq_puts(s, "\n");

	return 0;
}
DEFINE_SDE_DEBUGFS_SEQ_FOPS(sde_crtc_debugfs_state);

static int _sde_crtc_init_debugfs(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;
	struct sde_kms *sde_kms;

	static const struct file_operations debugfs_status_fops = {
		.open =		_sde_debugfs_status_open,
		.read =		seq_read,
		.llseek =	seq_lseek,
		.release =	single_release,
	};
	static const struct file_operations debugfs_misr_fops = {
		.open =		simple_open,
		.read =		_sde_crtc_misr_read,
		.write =	_sde_crtc_misr_setup,
	};

	if (!crtc)
		return -EINVAL;
	sde_crtc = to_sde_crtc(crtc);

	sde_kms = _sde_crtc_get_kms(crtc);
	if (!sde_kms)
		return -EINVAL;

	sde_crtc->debugfs_root = debugfs_create_dir(sde_crtc->name,
			crtc->dev->primary->debugfs_root);
	if (!sde_crtc->debugfs_root)
		return -ENOMEM;

	/* don't error check these */
	debugfs_create_file("status", 0444,
			sde_crtc->debugfs_root,
			sde_crtc, &debugfs_status_fops);
	debugfs_create_file("state", 0644,
			sde_crtc->debugfs_root,
			&sde_crtc->base,
			&sde_crtc_debugfs_state_fops);
	debugfs_create_file("misr_data", 0644, sde_crtc->debugfs_root,
					sde_crtc, &debugfs_misr_fops);

	return 0;
}

static void _sde_crtc_destroy_debugfs(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;

	if (!crtc)
		return;
	sde_crtc = to_sde_crtc(crtc);
	debugfs_remove_recursive(sde_crtc->debugfs_root);
}
#else
static int _sde_crtc_init_debugfs(struct drm_crtc *crtc)
{
	return 0;
}

static void _sde_crtc_destroy_debugfs(struct drm_crtc *crtc)
{
}
#endif /* CONFIG_DEBUG_FS */

static int sde_crtc_late_register(struct drm_crtc *crtc)
{
	return _sde_crtc_init_debugfs(crtc);
}

static void sde_crtc_early_unregister(struct drm_crtc *crtc)
{
	_sde_crtc_destroy_debugfs(crtc);
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
	.late_register = sde_crtc_late_register,
	.early_unregister = sde_crtc_early_unregister,
};

static const struct drm_crtc_helper_funcs sde_crtc_helper_funcs = {
	.mode_fixup = sde_crtc_mode_fixup,
	.disable = sde_crtc_disable,
	.enable = sde_crtc_enable,
	.atomic_check = sde_crtc_atomic_check,
	.atomic_begin = sde_crtc_atomic_begin,
	.atomic_flush = sde_crtc_atomic_flush,
};

static void _sde_crtc_event_cb(struct kthread_work *work)
{
	struct sde_crtc_event *event;
	struct sde_crtc *sde_crtc;
	unsigned long irq_flags;

	if (!work) {
		SDE_ERROR("invalid work item\n");
		return;
	}

	event = container_of(work, struct sde_crtc_event, kt_work);

	/* set sde_crtc to NULL for static work structures */
	sde_crtc = event->sde_crtc;
	if (!sde_crtc)
		return;

	if (event->cb_func)
		event->cb_func(&sde_crtc->base, event->usr);

	spin_lock_irqsave(&sde_crtc->event_lock, irq_flags);
	list_add_tail(&event->list, &sde_crtc->event_free_list);
	spin_unlock_irqrestore(&sde_crtc->event_lock, irq_flags);
}

int sde_crtc_event_queue(struct drm_crtc *crtc,
		void (*func)(struct drm_crtc *crtc, void *usr), void *usr)
{
	unsigned long irq_flags;
	struct sde_crtc *sde_crtc;
	struct sde_crtc_event *event = NULL;

	if (!crtc || !func)
		return -EINVAL;
	sde_crtc = to_sde_crtc(crtc);

	if (!sde_crtc->event_thread)
		return -EINVAL;
	/*
	 * Obtain an event struct from the private cache. This event
	 * queue may be called from ISR contexts, so use a private
	 * cache to avoid calling any memory allocation functions.
	 */
	spin_lock_irqsave(&sde_crtc->event_lock, irq_flags);
	if (!list_empty(&sde_crtc->event_free_list)) {
		event = list_first_entry(&sde_crtc->event_free_list,
				struct sde_crtc_event, list);
		list_del_init(&event->list);
	}
	spin_unlock_irqrestore(&sde_crtc->event_lock, irq_flags);

	if (!event)
		return -ENOMEM;

	/* populate event node */
	event->sde_crtc = sde_crtc;
	event->cb_func = func;
	event->usr = usr;

	/* queue new event request */
	kthread_init_work(&event->kt_work, _sde_crtc_event_cb);
	kthread_queue_work(&sde_crtc->event_worker, &event->kt_work);

	return 0;
}

static int _sde_crtc_init_events(struct sde_crtc *sde_crtc)
{
	int i, rc = 0;

	if (!sde_crtc) {
		SDE_ERROR("invalid crtc\n");
		return -EINVAL;
	}

	spin_lock_init(&sde_crtc->event_lock);

	INIT_LIST_HEAD(&sde_crtc->event_free_list);
	for (i = 0; i < SDE_CRTC_MAX_EVENT_COUNT; ++i)
		list_add_tail(&sde_crtc->event_cache[i].list,
				&sde_crtc->event_free_list);

	kthread_init_worker(&sde_crtc->event_worker);
	sde_crtc->event_thread = kthread_run(kthread_worker_fn,
			&sde_crtc->event_worker, "crtc_event:%d",
			sde_crtc->base.base.id);

	if (IS_ERR_OR_NULL(sde_crtc->event_thread)) {
		SDE_ERROR("failed to create event thread\n");
		rc = PTR_ERR(sde_crtc->event_thread);
		sde_crtc->event_thread = NULL;
	}

	return rc;
}

/* initialize crtc */
struct drm_crtc *sde_crtc_init(struct drm_device *dev, struct drm_plane *plane)
{
	struct drm_crtc *crtc = NULL;
	struct sde_crtc *sde_crtc = NULL;
	struct msm_drm_private *priv = NULL;
	struct sde_kms *kms = NULL;
	int i, rc;

	priv = dev->dev_private;
	kms = to_sde_kms(priv->kms);

	sde_crtc = kzalloc(sizeof(*sde_crtc), GFP_KERNEL);
	if (!sde_crtc)
		return ERR_PTR(-ENOMEM);

	crtc = &sde_crtc->base;
	crtc->dev = dev;
	atomic_set(&sde_crtc->vblank_refcount, 0);

	mutex_init(&sde_crtc->crtc_lock);
	spin_lock_init(&sde_crtc->spin_lock);
	atomic_set(&sde_crtc->frame_pending, 0);

	INIT_LIST_HEAD(&sde_crtc->frame_event_list);
	INIT_LIST_HEAD(&sde_crtc->user_event_list);
	for (i = 0; i < ARRAY_SIZE(sde_crtc->frame_events); i++) {
		INIT_LIST_HEAD(&sde_crtc->frame_events[i].list);
		list_add(&sde_crtc->frame_events[i].list,
				&sde_crtc->frame_event_list);
		kthread_init_work(&sde_crtc->frame_events[i].work,
				sde_crtc_frame_event_work);
	}

	drm_crtc_init_with_planes(dev, crtc, plane, NULL, &sde_crtc_funcs,
				NULL);

	drm_crtc_helper_add(crtc, &sde_crtc_helper_funcs);
	plane->crtc = crtc;

	/* save user friendly CRTC name for later */
	snprintf(sde_crtc->name, SDE_CRTC_NAME_SIZE, "crtc%u", crtc->base.id);

	/* initialize event handling */
	rc = _sde_crtc_init_events(sde_crtc);
	if (rc) {
		drm_crtc_cleanup(crtc);
		kfree(sde_crtc);
		return ERR_PTR(rc);
	}

	/* initialize output fence support */
	sde_fence_init(&sde_crtc->output_fence, sde_crtc->name, crtc->base.id);

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

static int _sde_crtc_event_enable(struct sde_kms *kms,
		struct drm_crtc *crtc_drm, u32 event)
{
	struct sde_crtc *crtc = NULL;
	struct sde_crtc_irq_info *node;
	struct msm_drm_private *priv;
	unsigned long flags;
	bool found = false;
	int ret, i = 0;

	crtc = to_sde_crtc(crtc_drm);
	spin_lock_irqsave(&crtc->spin_lock, flags);
	list_for_each_entry(node, &crtc->user_event_list, list) {
		if (node->event == event) {
			found = true;
			break;
		}
	}
	spin_unlock_irqrestore(&crtc->spin_lock, flags);

	/* event already enabled */
	if (found)
		return 0;

	node = NULL;
	for (i = 0; i < ARRAY_SIZE(custom_events); i++) {
		if (custom_events[i].event == event &&
			custom_events[i].func) {
			node = kzalloc(sizeof(*node), GFP_KERNEL);
			if (!node)
				return -ENOMEM;
			node->event = event;
			INIT_LIST_HEAD(&node->list);
			node->func = custom_events[i].func;
			node->event = event;
			break;
		}
	}

	if (!node) {
		SDE_ERROR("unsupported event %x\n", event);
		return -EINVAL;
	}

	priv = kms->dev->dev_private;
	ret = 0;
	if (crtc_drm->enabled) {
		sde_power_resource_enable(&priv->phandle, kms->core_client,
				true);
		ret = node->func(crtc_drm, true, &node->irq);
		sde_power_resource_enable(&priv->phandle, kms->core_client,
				false);
	}

	if (!ret) {
		spin_lock_irqsave(&crtc->spin_lock, flags);
		list_add_tail(&node->list, &crtc->user_event_list);
		spin_unlock_irqrestore(&crtc->spin_lock, flags);
	} else {
		kfree(node);
	}

	return ret;
}

static int _sde_crtc_event_disable(struct sde_kms *kms,
		struct drm_crtc *crtc_drm, u32 event)
{
	struct sde_crtc *crtc = NULL;
	struct sde_crtc_irq_info *node = NULL;
	struct msm_drm_private *priv;
	unsigned long flags;
	bool found = false;
	int ret;

	crtc = to_sde_crtc(crtc_drm);
	spin_lock_irqsave(&crtc->spin_lock, flags);
	list_for_each_entry(node, &crtc->user_event_list, list) {
		if (node->event == event) {
			list_del(&node->list);
			found = true;
			break;
		}
	}
	spin_unlock_irqrestore(&crtc->spin_lock, flags);

	/* event already disabled */
	if (!found)
		return 0;

	/**
	 * crtc is disabled interrupts are cleared remove from the list,
	 * no need to disable/de-register.
	 */
	if (!crtc_drm->enabled) {
		kfree(node);
		return 0;
	}
	priv = kms->dev->dev_private;
	sde_power_resource_enable(&priv->phandle, kms->core_client, true);
	ret = node->func(crtc_drm, false, &node->irq);
	sde_power_resource_enable(&priv->phandle, kms->core_client, false);
	return ret;
}

int sde_crtc_register_custom_event(struct sde_kms *kms,
		struct drm_crtc *crtc_drm, u32 event, bool en)
{
	struct sde_crtc *crtc = NULL;
	int ret;

	crtc = to_sde_crtc(crtc_drm);
	if (!crtc || !kms || !kms->dev) {
		DRM_ERROR("invalid sde_crtc %pK kms %pK dev %pK\n", crtc,
			kms, ((kms) ? (kms->dev) : NULL));
		return -EINVAL;
	}

	if (en)
		ret = _sde_crtc_event_enable(kms, crtc_drm, event);
	else
		ret = _sde_crtc_event_disable(kms, crtc_drm, event);

	return ret;
}

static int sde_crtc_power_interrupt_handler(struct drm_crtc *crtc_drm,
	bool en, struct sde_irq_callback *irq)
{
	return 0;
}
