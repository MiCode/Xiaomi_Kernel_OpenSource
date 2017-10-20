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
#include "sde_vbif.h"
#include "sde_power_handle.h"
#include "sde_core_perf.h"
#include "sde_trace.h"
#include <soc/qcom/scm.h>
#include "soc/qcom/secure_buffer.h"

/* defines for secure channel call */
#define SEC_SID_CNT               2
#define SEC_SID_MASK_0            0x80881
#define SEC_SID_MASK_1            0x80C81
#define MEM_PROTECT_SD_CTRL_SWITCH 0x18
#define MDP_DEVICE_ID            0x1A

struct sde_crtc_custom_events {
	u32 event;
	int (*func)(struct drm_crtc *crtc, bool en,
			struct sde_irq_callback *irq);
};

static int sde_crtc_power_interrupt_handler(struct drm_crtc *crtc_drm,
	bool en, struct sde_irq_callback *ad_irq);
static int sde_crtc_idle_interrupt_handler(struct drm_crtc *crtc_drm,
	bool en, struct sde_irq_callback *idle_irq);

static struct sde_crtc_custom_events custom_events[] = {
	{DRM_EVENT_AD_BACKLIGHT, sde_cp_ad_interrupt},
	{DRM_EVENT_CRTC_POWER, sde_crtc_power_interrupt_handler},
	{DRM_EVENT_IDLE_NOTIFY, sde_crtc_idle_interrupt_handler},
	{DRM_EVENT_HISTOGRAM, sde_cp_hist_interrupt},
};

/* default input fence timeout, in ms */
#define SDE_CRTC_INPUT_FENCE_TIMEOUT    10000

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
	mutex_lock(rp->rp_lock);
	_sde_crtc_rp_reclaim(rp, false);
	mutex_unlock(rp->rp_lock);
}

/**
 * _sde_crtc_rp_destroy - destroy resource pool
 * @rp: Pointer to resource pool
 * return: None
 */
static void _sde_crtc_rp_destroy(struct sde_crtc_respool *rp)
{
	mutex_lock(rp->rp_lock);
	list_del_init(&rp->rp_list);
	_sde_crtc_rp_reclaim(rp, true);
	mutex_unlock(rp->rp_lock);
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

	if (!rp || !dup_rp || !rp->rp_head) {
		SDE_ERROR("invalid resource pool\n");
		return;
	}

	crtc = _sde_crtc_rp_to_crtc(rp);
	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	SDE_DEBUG("crtc%d.%u duplicate\n", crtc->base.id, rp->sequence_id);

	mutex_lock(rp->rp_lock);
	dup_rp->sequence_id = rp->sequence_id + 1;
	INIT_LIST_HEAD(&dup_rp->res_list);
	dup_rp->ops = rp->ops;
	list_for_each_entry(res, &rp->res_list, list) {
		dup_res = kzalloc(sizeof(struct sde_crtc_res), GFP_KERNEL);
		if (!dup_res) {
			mutex_unlock(rp->rp_lock);
			return;
		}
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

	dup_rp->rp_lock = rp->rp_lock;
	dup_rp->rp_head = rp->rp_head;
	INIT_LIST_HEAD(&dup_rp->rp_list);
	list_add_tail(&dup_rp->rp_list, rp->rp_head);
	mutex_unlock(rp->rp_lock);
}

/**
 * _sde_crtc_rp_reset - reset resource pool after allocation
 * @rp: Pointer to original resource pool
 * @rp_lock: Pointer to serialization resource pool lock
 * @rp_head: Pointer to crtc resource pool head
 * return: None
 */
static void _sde_crtc_rp_reset(struct sde_crtc_respool *rp,
		struct mutex *rp_lock, struct list_head *rp_head)
{
	if (!rp || !rp_lock || !rp_head) {
		SDE_ERROR("invalid resource pool\n");
		return;
	}

	mutex_lock(rp_lock);
	rp->rp_lock = rp_lock;
	rp->rp_head = rp_head;
	INIT_LIST_HEAD(&rp->rp_list);
	rp->sequence_id = 0;
	INIT_LIST_HEAD(&rp->res_list);
	rp->ops.get = _sde_crtc_hw_blk_get;
	rp->ops.put = _sde_crtc_hw_blk_put;
	list_add_tail(&rp->rp_list, rp->rp_head);
	mutex_unlock(rp_lock);
}

/**
 * _sde_crtc_rp_add_no_lock - add given resource to resource pool without lock
 * @rp: Pointer to original resource pool
 * @type: Resource type
 * @tag: Search tag for given resource
 * @val: Resource handle
 * @ops: Resource callback operations
 * return: 0 if success; error code otherwise
 */
static int _sde_crtc_rp_add_no_lock(struct sde_crtc_respool *rp, u32 type,
		u64 tag, void *val, struct sde_crtc_res_ops *ops)
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
	int rc;

	if (!rp) {
		SDE_ERROR("invalid resource pool\n");
		return -EINVAL;
	}

	mutex_lock(rp->rp_lock);
	rc = _sde_crtc_rp_add_no_lock(rp, type, tag, val, ops);
	mutex_unlock(rp->rp_lock);
	return rc;
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
	struct sde_crtc_respool *old_rp;
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

	mutex_lock(rp->rp_lock);
	list_for_each_entry(res, &rp->res_list, list) {
		if (res->type != type || res->tag != tag)
			continue;
		SDE_DEBUG("crtc%d.%u found res:0x%x/0x%llx/%pK/%d\n",
				crtc->base.id, rp->sequence_id,
				res->type, res->tag, res->val,
				atomic_read(&res->refcount));
		atomic_inc(&res->refcount);
		res->flags &= ~SDE_CRTC_RES_FLAG_FREE;
		mutex_unlock(rp->rp_lock);
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
		mutex_unlock(rp->rp_lock);
		return res->val;
	}
	/* not in this rp, try to grab from global pool */
	if (rp->ops.get)
		val = rp->ops.get(NULL, type, -1);
	if (!IS_ERR_OR_NULL(val))
		goto add_res;
	/*
	 * Search older resource pools for hw blk with matching type,
	 * necessary when resource is being used by this object,
	 * but in previous states not yet cleaned up.
	 *
	 * This enables searching of all resources currently owned
	 * by this crtc even though the resource might not be used
	 * in the current atomic state. This allows those resources
	 * to be re-acquired by the new atomic state immediately
	 * without waiting for the resources to be fully released.
	 */
	else if (IS_ERR_OR_NULL(val) && (type < SDE_HW_BLK_MAX)) {
		list_for_each_entry(old_rp, rp->rp_head, rp_list) {
			if (old_rp == rp)
				continue;

			list_for_each_entry(res, &old_rp->res_list, list) {
				if (res->type != type)
					continue;
				SDE_DEBUG(
					"crtc%d.%u found res:0x%x//%pK/ in crtc%d.%d\n",
						crtc->base.id,
						rp->sequence_id,
						res->type, res->val,
						crtc->base.id,
						old_rp->sequence_id);
				SDE_EVT32_VERBOSE(crtc->base.id,
						rp->sequence_id,
						res->type, res->val,
						crtc->base.id,
						old_rp->sequence_id);
				if (res->ops.get)
					res->ops.get(res->val, 0, -1);
				val = res->val;
				break;
			}

			if (!IS_ERR_OR_NULL(val))
				break;
		}
	}
	if (IS_ERR_OR_NULL(val)) {
		SDE_DEBUG("crtc%d.%u failed to get res:0x%x//\n",
				crtc->base.id, rp->sequence_id, type);
		mutex_unlock(rp->rp_lock);
		return NULL;
	}
add_res:
	rc = _sde_crtc_rp_add_no_lock(rp, type, tag, val, &rp->ops);
	if (rc) {
		SDE_ERROR("crtc%d.%u failed to add res:0x%x/0x%llx\n",
				crtc->base.id, rp->sequence_id, type, tag);
		if (rp->ops.put)
			rp->ops.put(val);
		val = NULL;
	}
	mutex_unlock(rp->rp_lock);
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

	mutex_lock(rp->rp_lock);
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

		mutex_unlock(rp->rp_lock);
		return;
	}
	SDE_ERROR("crtc%d.%u not found res:0x%x/0x%llx\n",
			crtc->base.id, rp->sequence_id, type, tag);
	mutex_unlock(rp->rp_lock);
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
}

/**
 * sde_crtc_destroy_dest_scaler - free memory allocated for scaler lut
 * @sde_crtc: Pointer to sde crtc
 */
static void _sde_crtc_destroy_dest_scaler(struct sde_crtc *sde_crtc)
{
	if (!sde_crtc)
		return;

	kfree(sde_crtc->scl3_lut_cfg);
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
	_sde_crtc_destroy_dest_scaler(sde_crtc);

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

	if ((msm_is_mode_seamless(adjusted_mode) ||
			msm_is_mode_seamless_vrr(adjusted_mode)) &&
		(!crtc->enabled)) {
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
		void __user *usr_ptr)
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

	/*
	 * partial update is not supported with 3dmux dsc or dest scaler.
	 * hence, crtc roi must match the mixer dimensions.
	 */
	if (crtc_state->num_ds_enabled ||
		_sde_crtc_setup_is_3dmux_dsc(state)) {
		if (memcmp(lm_roi, lm_bounds, sizeof(struct sde_rect))) {
			SDE_ERROR("Unsupported: Dest scaler/3d mux DSC + PU\n");
			return -EINVAL;
		}
	}

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
	if (sde_crtc->num_mixers < 2)
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
	const struct drm_plane_state *pstate;
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

	drm_atomic_crtc_state_for_each_plane_state(plane, pstate, state) {
		struct sde_rect plane_roi, intersection;

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

	u32 flush_mask, flush_sbuf;
	uint32_t stage_idx, lm_idx;
	int zpos_cnt[SDE_STAGE_MAX + 1] = { 0 };
	int i;
	bool bg_alpha_enable = false;
	u32 prefill = 0;

	if (!sde_crtc || !mixer) {
		SDE_ERROR("invalid sde_crtc or mixer\n");
		return;
	}

	ctl = mixer->hw_ctl;
	lm = mixer->hw_lm;
	stage_cfg = &sde_crtc->stage_cfg;
	cstate = to_sde_crtc_state(crtc->state);

	cstate->sbuf_prefill_line = 0;
	sde_crtc->sbuf_flush_mask = 0x0;

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

		prefill = sde_plane_rot_calc_prefill(plane);
		if (prefill > cstate->sbuf_prefill_line)
			cstate->sbuf_prefill_line = prefill;

		sde_plane_get_ctl_flush(plane, ctl, &flush_mask, &flush_sbuf);

		/* save sbuf flush value for later */
		sde_crtc->sbuf_flush_mask |= flush_sbuf;

		SDE_DEBUG("crtc %d stage:%d - plane %d sspp %d fb %d\n",
				crtc->base.id,
				pstate->stage,
				plane->base.id,
				sde_plane_pipe(plane) - SSPP_VIG0,
				state->fb ? state->fb->base.id : -1);

		format = to_sde_format(msm_framebuffer_format(pstate->base.fb));
		if (!format) {
			SDE_ERROR("invalid format\n");
			return;
		}

		if (pstate->stage == SDE_STAGE_BASE && format->alpha_enable)
			bg_alpha_enable = true;

		SDE_EVT32(DRMID(crtc), DRMID(plane),
				state->fb ? state->fb->base.id : -1,
				state->src_x >> 16, state->src_y >> 16,
				state->src_w >> 16, state->src_h >> 16,
				state->crtc_x, state->crtc_y,
				state->crtc_w, state->crtc_h,
				flush_sbuf != 0);

		stage_idx = zpos_cnt[pstate->stage]++;
		stage_cfg->stage[pstate->stage][stage_idx] =
					sde_plane_pipe(plane);
		stage_cfg->multirect_index[pstate->stage][stage_idx] =
					pstate->multirect_index;

		SDE_EVT32(DRMID(crtc), DRMID(plane), stage_idx,
			sde_plane_pipe(plane) - SSPP_VIG0, pstate->stage,
			pstate->multirect_index, pstate->multirect_mode,
			format->base.pixel_format, fb ? fb->modifier[0] : 0);

		/* blend config update */
		for (lm_idx = 0; lm_idx < sde_crtc->num_mixers; lm_idx++) {
			_sde_crtc_setup_blend_cfg(mixer + lm_idx, pstate,
								format);
			mixer[lm_idx].flush_mask |= flush_mask;

			if (bg_alpha_enable && !format->alpha_enable)
				mixer[lm_idx].mixer_op_mode = 0;
			else
				mixer[lm_idx].mixer_op_mode |=
						1 << pstate->stage;
		}
	}

	if (lm && lm->ops.setup_dim_layer) {
		cstate = to_sde_crtc_state(crtc->state);
		for (i = 0; i < cstate->num_dim_layers; i++)
			_sde_crtc_setup_dim_layer_cfg(crtc, sde_crtc,
					mixer, &cstate->dim_layer[i]);
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

		mixer[i].pipe_mask = mixer[i].flush_mask;
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
			&sde_crtc->stage_cfg);
	}

	_sde_crtc_program_lm_output_roi(crtc);
}

static int _sde_crtc_find_plane_fb_modes(struct drm_crtc_state *state,
		uint32_t *fb_ns,
		uint32_t *fb_sec,
		uint32_t *fb_sec_dir)
{
	struct drm_plane *plane;
	const struct drm_plane_state *pstate;
	struct sde_plane_state *sde_pstate;
	uint32_t mode = 0;
	int rc;

	if (!state) {
		SDE_ERROR("invalid state\n");
		return -EINVAL;
	}

	*fb_ns = 0;
	*fb_sec = 0;
	*fb_sec_dir = 0;
	drm_atomic_crtc_state_for_each_plane_state(plane, pstate, state) {
		if (IS_ERR_OR_NULL(pstate)) {
			rc = PTR_ERR(pstate);
			SDE_ERROR("crtc%d failed to get plane%d state%d\n",
					state->crtc->base.id,
					plane->base.id, rc);
			return rc;
		}
		sde_pstate = to_sde_plane_state(pstate);
		mode = sde_plane_get_property(sde_pstate,
				PLANE_PROP_FB_TRANSLATION_MODE);
		switch (mode) {
		case SDE_DRM_FB_NON_SEC:
			(*fb_ns)++;
			break;
		case SDE_DRM_FB_SEC:
			(*fb_sec)++;
			break;
		case SDE_DRM_FB_SEC_DIR_TRANS:
			(*fb_sec_dir)++;
			break;
		default:
			SDE_ERROR("Error: Plane[%d], fb_trans_mode:%d",
					plane->base.id, mode);
			return -EINVAL;
		}
	}
	return 0;
}

/**
 * sde_crtc_get_secure_transition_ops - determines the operations that
 * need to be performed before transitioning to secure state
 * This function should be called after swapping the new state
 * @crtc: Pointer to drm crtc structure
 * Returns the bitmask of operations need to be performed, -Error in
 * case of error cases
 */
int sde_crtc_get_secure_transition_ops(struct drm_crtc *crtc,
		struct drm_crtc_state *old_crtc_state,
		bool old_valid_fb)
{
	struct drm_plane *plane;
	struct drm_encoder *encoder;
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate;
	struct sde_crtc_smmu_state_data *smmu_state;
	uint32_t translation_mode = 0, secure_level;
	int ops  = 0;
	bool post_commit = false;

	if (!crtc || !crtc->state) {
		SDE_ERROR("invalid crtc\n");
		return -EINVAL;
	}

	sde_crtc = to_sde_crtc(crtc);
	cstate = to_sde_crtc_state(crtc->state);
	smmu_state = &sde_crtc->smmu_state;
	secure_level = sde_crtc_get_secure_level(crtc, crtc->state);

	SDE_DEBUG("crtc%d, secure_level%d old_valid_fb%d\n",
			crtc->base.id, secure_level, old_valid_fb);

	SDE_EVT32_VERBOSE(DRMID(crtc), secure_level, smmu_state->state,
			old_valid_fb, SDE_EVTLOG_FUNC_ENTRY);
	/**
	 * SMMU operations need to be delayed in case of
	 * video mode panels when switching back to non_secure
	 * mode
	 */
	drm_for_each_encoder(encoder, crtc->dev) {
		if (encoder->crtc != crtc)
			continue;

		post_commit |= sde_encoder_check_mode(encoder,
						MSM_DISPLAY_CAP_VID_MODE);
	}

	drm_atomic_crtc_for_each_plane(plane, crtc) {
		if (!plane->state)
			continue;

		translation_mode = sde_plane_get_property(
				to_sde_plane_state(plane->state),
				PLANE_PROP_FB_TRANSLATION_MODE);
		if (translation_mode > SDE_DRM_FB_SEC_DIR_TRANS) {
			SDE_ERROR("crtc%d, invalid translation_mode%d\n",
					crtc->base.id, translation_mode);
			return -EINVAL;
		}

		/**
		 * we can break if we find sec_fir or non_sec_dir
		 * plane
		 */
		if (translation_mode == SDE_DRM_FB_SEC_DIR_TRANS)
			break;
	}

	switch (translation_mode) {
	case SDE_DRM_FB_SEC_DIR_TRANS:
		/* secure display usecase */
		if ((smmu_state->state == ATTACHED) &&
				(secure_level == SDE_DRM_SEC_ONLY)) {
			smmu_state->state = DETACH_ALL_REQ;
			smmu_state->transition_type = PRE_COMMIT;
			ops |= SDE_KMS_OPS_CRTC_SECURE_STATE_CHANGE;
			if (old_valid_fb) {
				ops |= (SDE_KMS_OPS_WAIT_FOR_TX_DONE  |
					SDE_KMS_OPS_CLEANUP_PLANE_FB);
			}
		/* secure camera usecase */
		} else if (smmu_state->state == ATTACHED) {
			smmu_state->state = DETACH_SEC_REQ;
			smmu_state->transition_type = PRE_COMMIT;
			ops |= SDE_KMS_OPS_CRTC_SECURE_STATE_CHANGE;
		}
		break;
	case SDE_DRM_FB_SEC:
	case SDE_DRM_FB_NON_SEC:
		if ((smmu_state->state == DETACHED_SEC) ||
			(smmu_state->state == DETACH_SEC_REQ)) {
			smmu_state->state = ATTACH_SEC_REQ;
			smmu_state->transition_type = post_commit ?
				POST_COMMIT : PRE_COMMIT;
			ops |= SDE_KMS_OPS_CRTC_SECURE_STATE_CHANGE;
			if (old_valid_fb)
				ops |= SDE_KMS_OPS_WAIT_FOR_TX_DONE;
		} else if ((smmu_state->state == DETACHED) ||
				(smmu_state->state == DETACH_ALL_REQ)) {
			smmu_state->state = ATTACH_ALL_REQ;
			smmu_state->transition_type = post_commit ?
				POST_COMMIT : PRE_COMMIT;
			ops |= SDE_KMS_OPS_CRTC_SECURE_STATE_CHANGE;
			if (old_valid_fb)
				ops |= (SDE_KMS_OPS_WAIT_FOR_TX_DONE |
				 SDE_KMS_OPS_CLEANUP_PLANE_FB);
		}
		break;
	default:
		SDE_ERROR("invalid plane fb_mode:%d\n", translation_mode);
		ops = 0;
		return -EINVAL;
	}

	SDE_DEBUG("SMMU State:%d, type:%d ops:%x\n", smmu_state->state,
			smmu_state->transition_type, ops);
	/* log only during actual transition times */
	if (ops)
		SDE_EVT32(DRMID(crtc), secure_level, translation_mode,
				smmu_state->state, smmu_state->transition_type,
				ops, old_valid_fb, SDE_EVTLOG_FUNC_EXIT);
	return ops;
}

/**
 * _sde_crtc_scm_call - makes secure channel call to switch the VMIDs
 * @vimd: switch the stage 2 translation to this VMID.
 */
static int _sde_crtc_scm_call(int vmid)
{
	struct scm_desc desc = {0};
	uint32_t num_sids;
	uint32_t *sec_sid;
	uint32_t mem_protect_sd_ctrl_id = MEM_PROTECT_SD_CTRL_SWITCH;
	int ret = 0;

	/* This info should be queried from catalog */
	num_sids = SEC_SID_CNT;
	sec_sid = kcalloc(num_sids, sizeof(uint32_t), GFP_KERNEL);
	if (!sec_sid)
		return -ENOMEM;

	/**
	 * derive this info from device tree/catalog, this is combination of
	 * smr mask and SID for secure
	 */
	sec_sid[0] = SEC_SID_MASK_0;
	sec_sid[1] = SEC_SID_MASK_1;
	dmac_flush_range(sec_sid, sec_sid + num_sids);

	SDE_DEBUG("calling scm_call for vmid %d", vmid);

	desc.arginfo = SCM_ARGS(4, SCM_VAL, SCM_RW, SCM_VAL, SCM_VAL);
	desc.args[0] = MDP_DEVICE_ID;
	desc.args[1] = SCM_BUFFER_PHYS(sec_sid);
	desc.args[2] = sizeof(uint32_t) * num_sids;
	desc.args[3] =  vmid;

	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_MP,
				mem_protect_sd_ctrl_id), &desc);
	if (ret) {
		SDE_ERROR("Error:scm_call2, vmid (%lld): ret%d\n",
				desc.args[3], ret);
	}
	SDE_EVT32(mem_protect_sd_ctrl_id,
			desc.args[0], desc.args[3], num_sids,
			sec_sid[0], sec_sid[1], ret);

	kfree(sec_sid);
	return ret;
}

/**
 * _sde_crtc_setup_scaler3_lut - Set up scaler lut
 * LUTs are configured only once during boot
 * @sde_crtc: Pointer to sde crtc
 * @cstate: Pointer to sde crtc state
 */
static int _sde_crtc_set_dest_scaler_lut(struct sde_crtc *sde_crtc,
		struct sde_crtc_state *cstate, uint32_t lut_idx)
{
	struct sde_hw_scaler3_lut_cfg *cfg;
	u32 *lut_data = NULL;
	size_t len = 0;
	int ret = 0;

	if (!sde_crtc || !cstate || !sde_crtc->scl3_lut_cfg) {
		SDE_ERROR("invalid args\n");
		return -EINVAL;
	}

	if (sde_crtc->scl3_lut_cfg->is_configured) {
		SDE_DEBUG("lut already configured\n");
		return 0;
	}

	lut_data = msm_property_get_blob(&sde_crtc->property_info,
			&cstate->property_state, &len, lut_idx);
	if (!lut_data || !len) {
		SDE_ERROR("lut(%d): no data, len(%zu)\n", lut_idx, len);
		return -ENODATA;
	}

	cfg = sde_crtc->scl3_lut_cfg;

	switch (lut_idx) {
	case CRTC_PROP_DEST_SCALER_LUT_ED:
		cfg->dir_lut = lut_data;
		cfg->dir_len = len;
		break;
	case CRTC_PROP_DEST_SCALER_LUT_CIR:
		cfg->cir_lut = lut_data;
		cfg->cir_len = len;
		break;
	case CRTC_PROP_DEST_SCALER_LUT_SEP:
		cfg->sep_lut = lut_data;
		cfg->sep_len = len;
		break;
	default:
		ret = -EINVAL;
		SDE_ERROR("invalid LUT index = %d", lut_idx);
		break;
	}

	if (cfg->dir_lut && cfg->cir_lut && cfg->sep_lut)
		cfg->is_configured = true;

	return ret;
}

/**
 * sde_crtc_secure_ctrl - Initiates the operations to swtich  between secure
 *                       and non-secure mode
 * @crtc: Pointer to crtc
 * @post_commit: if this operation is triggered after commit
 */
int sde_crtc_secure_ctrl(struct drm_crtc *crtc, bool post_commit)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate;
	struct sde_kms *sde_kms;
	struct sde_crtc_smmu_state_data *smmu_state;
	int ret = 0;
	int old_smmu_state;

	if (!crtc || !crtc->state) {
		SDE_ERROR("invalid crtc\n");
		return -EINVAL;
	}

	sde_kms = _sde_crtc_get_kms(crtc);
	if (!sde_kms) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}

	sde_crtc = to_sde_crtc(crtc);
	cstate = to_sde_crtc_state(crtc->state);
	smmu_state = &sde_crtc->smmu_state;
	old_smmu_state = smmu_state->state;

	SDE_EVT32(DRMID(crtc), smmu_state->state, smmu_state->transition_type,
			post_commit, SDE_EVTLOG_FUNC_ENTRY);

	if ((!smmu_state->transition_type) ||
	    ((smmu_state->transition_type == POST_COMMIT) && !post_commit))
		/* Bail out */
		return 0;

	/* Secure UI use case enable */
	switch (smmu_state->state) {
	case DETACH_ALL_REQ:
		/* detach_all_contexts */
		ret = sde_kms_mmu_detach(sde_kms, false);
		if (ret) {
			SDE_ERROR("crtc: %d, failed to detach %d\n",
					crtc->base.id, ret);
			goto error;
		}

		ret = _sde_crtc_scm_call(VMID_CP_SEC_DISPLAY);
		if (ret)
			goto error;

		smmu_state->state = DETACHED;
		break;
	/* Secure UI use case disable */
	case ATTACH_ALL_REQ:
		ret = _sde_crtc_scm_call(VMID_CP_PIXEL);
		if (ret)
			goto error;

		/* attach_all_contexts */
		ret = sde_kms_mmu_attach(sde_kms, false);
		if (ret) {
			SDE_ERROR("crtc: %d, failed to attach %d\n",
					crtc->base.id,
					ret);
			goto error;
		}

		smmu_state->state = ATTACHED;

		break;
	/* Secure preview enable */
	case DETACH_SEC_REQ:
		/* detach secure_context */
		ret = sde_kms_mmu_detach(sde_kms, true);
		if (ret) {
			SDE_ERROR("crtc: %d, failed to detach %d\n",
					crtc->base.id,
					ret);
			goto error;
		}

		smmu_state->state = DETACHED_SEC;
		ret = _sde_crtc_scm_call(VMID_CP_CAMERA_PREVIEW);
		if (ret)
			goto error;

		break;

	/* Secure preview disable */
	case ATTACH_SEC_REQ:
		ret = _sde_crtc_scm_call(VMID_CP_PIXEL);
		if (ret)
			goto error;

		ret = sde_kms_mmu_attach(sde_kms, true);
		if (ret) {
			SDE_ERROR("crtc: %d, failed to attach %d\n",
					crtc->base.id,
					ret);
			goto error;
		}
		smmu_state->state = ATTACHED;
		break;
	default:
		break;
	}

	SDE_DEBUG("crtc: %d, old_state %d new_state %d\n", crtc->base.id,
			old_smmu_state,
			smmu_state->state);
	smmu_state->transition_type = NONE;

error:
	smmu_state->transition_error = ret ? true : false;
	SDE_EVT32(DRMID(crtc), smmu_state->state, smmu_state->transition_type,
			smmu_state->transition_error, ret,
			SDE_EVTLOG_FUNC_EXIT);
	return ret;
}

/**
 * _sde_crtc_dest_scaler_setup - Set up dest scaler block
 * @crtc: Pointer to drm crtc
 */
static void _sde_crtc_dest_scaler_setup(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate;
	struct sde_hw_mixer *hw_lm;
	struct sde_hw_ctl *hw_ctl;
	struct sde_hw_ds *hw_ds;
	struct sde_hw_ds_cfg *cfg;
	struct sde_kms *kms;
	u32 flush_mask = 0, op_mode = 0;
	u32 lm_idx = 0, num_mixers = 0;
	int i, count = 0;

	if (!crtc)
		return;

	sde_crtc   = to_sde_crtc(crtc);
	cstate = to_sde_crtc_state(crtc->state);
	kms    = _sde_crtc_get_kms(crtc);
	num_mixers = sde_crtc->num_mixers;

	SDE_DEBUG("crtc%d\n", crtc->base.id);

	if (!cstate->ds_dirty) {
		SDE_DEBUG("no change in settings, skip commit\n");
	} else if (!kms || !kms->catalog) {
		SDE_ERROR("invalid parameters\n");
	} else if (!kms->catalog->mdp[0].has_dest_scaler) {
		SDE_DEBUG("dest scaler feature not supported\n");
	} else if (num_mixers > CRTC_DUAL_MIXERS) {
		SDE_ERROR("invalid number mixers: %d\n", num_mixers);
	} else if (!sde_crtc->scl3_lut_cfg->is_configured) {
		SDE_DEBUG("no LUT data available\n");
	} else {
		count = cstate->num_ds_enabled ? cstate->num_ds : num_mixers;

		for (i = 0; i < count; i++) {
			cfg = &cstate->ds_cfg[i];

			if (!cfg->flags)
				continue;

			lm_idx = cfg->ndx;
			hw_lm  = sde_crtc->mixers[lm_idx].hw_lm;
			hw_ctl = sde_crtc->mixers[lm_idx].hw_ctl;
			hw_ds  = sde_crtc->mixers[lm_idx].hw_ds;

			/* Setup op mode - Dual/single */
			if (cfg->flags & SDE_DRM_DESTSCALER_ENABLE)
				op_mode |= BIT(hw_ds->idx - DS_0);

			if ((i == count-1) && hw_ds->ops.setup_opmode) {
				op_mode |= (cstate->num_ds_enabled ==
					CRTC_DUAL_MIXERS) ?
					SDE_DS_OP_MODE_DUAL : 0;
				hw_ds->ops.setup_opmode(hw_ds, op_mode);
				SDE_EVT32(DRMID(crtc), op_mode);
			}

			/* Setup scaler */
			if ((cfg->flags & SDE_DRM_DESTSCALER_SCALE_UPDATE) ||
				(cfg->flags &
					SDE_DRM_DESTSCALER_ENHANCER_UPDATE)) {
				if (hw_ds->ops.setup_scaler)
					hw_ds->ops.setup_scaler(hw_ds,
							cfg->scl3_cfg,
							sde_crtc->scl3_lut_cfg);

				/**
				 * Clear the flags as the block doesn't have to
				 * be programmed in each commit if no updates
				 */
				cfg->flags &= ~SDE_DRM_DESTSCALER_SCALE_UPDATE;
				cfg->flags &=
					~SDE_DRM_DESTSCALER_ENHANCER_UPDATE;
			}

			/*
			 * Dest scaler shares the flush bit of the LM in control
			 */
			if (cfg->set_lm_flush && hw_lm && hw_ctl &&
				hw_ctl->ops.get_bitmask_mixer) {
				flush_mask = hw_ctl->ops.get_bitmask_mixer(
						hw_ctl, hw_lm->idx);
				SDE_DEBUG("Set lm[%d] flush = %d",
					hw_lm->idx, flush_mask);
				hw_ctl->ops.update_pending_flush(hw_ctl,
								flush_mask);
			}
			cfg->set_lm_flush = false;
		}
		cstate->ds_dirty = false;
	}
}

void sde_crtc_prepare_commit(struct drm_crtc *crtc,
		struct drm_crtc_state *old_state)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate;
	struct drm_connector *conn;
	struct sde_crtc_retire_event *retire_event = NULL;
	unsigned long flags;
	int i;

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

	for (i = 0; i < SDE_CRTC_FRAME_EVENT_SIZE; i++) {
		retire_event = &sde_crtc->retire_events[i];
		if (list_empty(&retire_event->list))
			break;
		retire_event = NULL;
	}

	if (retire_event) {
		retire_event->num_connectors = cstate->num_connectors;
		for (i = 0; i < cstate->num_connectors; i++)
			retire_event->connectors[i] = cstate->connectors[i];

		spin_lock_irqsave(&sde_crtc->spin_lock, flags);
		list_add_tail(&retire_event->list,
						&sde_crtc->retire_event_list);
		spin_unlock_irqrestore(&sde_crtc->spin_lock, flags);
	} else {
		SDE_ERROR("crtc%d retire event overflow\n", crtc->base.id);
		SDE_EVT32(DRMID(crtc), SDE_EVTLOG_ERROR);
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
			SDE_EVT32_VERBOSE(DRMID(crtc));
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

static void _sde_crtc_retire_event(struct drm_crtc *crtc, ktime_t ts)
{
	struct sde_crtc_retire_event *retire_event;
	struct sde_crtc *sde_crtc;
	unsigned long flags;
	int i;

	if (!crtc) {
		SDE_ERROR("invalid param\n");
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
	spin_lock_irqsave(&sde_crtc->spin_lock, flags);
	retire_event = list_first_entry_or_null(&sde_crtc->retire_event_list,
				struct sde_crtc_retire_event, list);
	if (retire_event)
		list_del_init(&retire_event->list);
	spin_unlock_irqrestore(&sde_crtc->spin_lock, flags);

	if (!retire_event) {
		SDE_ERROR("crtc%d retire event without kickoff\n",
								crtc->base.id);
		SDE_EVT32(DRMID(crtc), SDE_EVTLOG_ERROR);
		return;
	}

	SDE_ATRACE_BEGIN("signal_retire_fence");
	for (i = 0; (i < retire_event->num_connectors) &&
					retire_event->connectors[i]; ++i)
		sde_connector_complete_commit(
					retire_event->connectors[i], ts);
	SDE_ATRACE_END("signal_retire_fence");
}

/* _sde_crtc_idle_notify - signal idle timeout to client */
static void _sde_crtc_idle_notify(struct sde_crtc *sde_crtc)
{
	struct drm_crtc *crtc;
	struct drm_event event;
	int ret = 0;

	if (!sde_crtc) {
		SDE_ERROR("invalid sde crtc\n");
		return;
	}

	crtc = &sde_crtc->base;
	event.type = DRM_EVENT_IDLE_NOTIFY;
	event.length = sizeof(u32);
	msm_mode_object_event_notify(&crtc->base, crtc->dev, &event,
								(u8 *)&ret);

	SDE_DEBUG("crtc:%d idle timeout notified\n", crtc->base.id);
}

/*
 * sde_crtc_handle_event - crtc frame event handle.
 * This API must manage only non-IRQ context events.
 */
static bool _sde_crtc_handle_event(struct sde_crtc *sde_crtc, u32 event)
{
	bool event_processed = false;

	/**
	 * idle events are originated from commit thread and can be processed
	 * in same context
	 */
	if (event & SDE_ENCODER_FRAME_EVENT_IDLE) {
		_sde_crtc_idle_notify(sde_crtc);
		event_processed = true;
	}

	return event_processed;
}

static void sde_crtc_frame_event_work(struct kthread_work *work)
{
	struct msm_drm_private *priv;
	struct sde_crtc_frame_event *fevent;
	struct drm_crtc *crtc;
	struct sde_crtc *sde_crtc;
	struct sde_kms *sde_kms;
	unsigned long flags;
	bool frame_done = false;

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

	sde_kms = _sde_crtc_get_kms(crtc);
	if (!sde_kms) {
		SDE_ERROR("invalid kms handle\n");
		return;
	}
	priv = sde_kms->dev->dev_private;
	SDE_ATRACE_BEGIN("crtc_frame_event");

	SDE_DEBUG("crtc%d event:%u ts:%lld\n", crtc->base.id, fevent->event,
			ktime_to_ns(fevent->ts));

	SDE_EVT32_VERBOSE(DRMID(crtc), fevent->event, SDE_EVTLOG_FUNC_ENTRY);

	if (fevent->event & (SDE_ENCODER_FRAME_EVENT_DONE
				| SDE_ENCODER_FRAME_EVENT_ERROR
				| SDE_ENCODER_FRAME_EVENT_PANEL_DEAD)) {

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
			sde_core_perf_crtc_release_bw(crtc);
		} else {
			SDE_EVT32_VERBOSE(DRMID(crtc), fevent->event,
							SDE_EVTLOG_FUNC_CASE3);
		}

		if (fevent->event & SDE_ENCODER_FRAME_EVENT_DONE)
			sde_core_perf_crtc_update(crtc, 0, false);

		if (fevent->event & (SDE_ENCODER_FRAME_EVENT_DONE
					| SDE_ENCODER_FRAME_EVENT_ERROR))
			frame_done = true;
	}

	if (fevent->event & SDE_ENCODER_FRAME_EVENT_SIGNAL_RELEASE_FENCE) {
		SDE_ATRACE_BEGIN("signal_release_fence");
		sde_fence_signal(&sde_crtc->output_fence, fevent->ts, false);
		SDE_ATRACE_END("signal_release_fence");
	}

	if (fevent->event & SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE)
		/* this api should be called without spin_lock */
		_sde_crtc_retire_event(crtc, fevent->ts);

	if (fevent->event & SDE_ENCODER_FRAME_EVENT_PANEL_DEAD)
		SDE_ERROR("crtc%d ts:%lld received panel dead event\n",
				crtc->base.id, ktime_to_ns(fevent->ts));

	if (frame_done)
		complete_all(&sde_crtc->frame_done_comp);

	spin_lock_irqsave(&sde_crtc->spin_lock, flags);
	list_add_tail(&fevent->list, &sde_crtc->frame_event_list);
	spin_unlock_irqrestore(&sde_crtc->spin_lock, flags);
	SDE_ATRACE_END("crtc_frame_event");
}

/*
 * sde_crtc_frame_event_cb - crtc frame event callback API. CRTC module
 * registers this API to encoder for all frame event callbacks like
 * release_fence, retire_fence, frame_error, frame_done, idle_timeout,
 * etc. Encoder may call different events from different context - IRQ,
 * user thread, commit_thread, etc. Each event should be carefully
 * reviewed and should be processed in proper task context to avoid scheduling
 * delay or properly manage the irq context's bottom half processing.
 */
static void sde_crtc_frame_event_cb(void *data, u32 event)
{
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct sde_crtc *sde_crtc;
	struct msm_drm_private *priv;
	struct sde_crtc_frame_event *fevent;
	unsigned long flags;
	u32 crtc_id;
	bool event_processed = false;

	if (!crtc || !crtc->dev || !crtc->dev->dev_private) {
		SDE_ERROR("invalid parameters\n");
		return;
	}
	sde_crtc = to_sde_crtc(crtc);
	priv = crtc->dev->dev_private;
	crtc_id = drm_crtc_index(crtc);

	SDE_DEBUG("crtc%d\n", crtc->base.id);
	SDE_EVT32_VERBOSE(DRMID(crtc), event);

	/* try to process the event in caller context */
	event_processed = _sde_crtc_handle_event(sde_crtc, event);
	if (event_processed)
		return;

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
	kthread_queue_work(&priv->event_thread[crtc_id].worker, &fevent->work);
}

void sde_crtc_complete_commit(struct drm_crtc *crtc,
		struct drm_crtc_state *old_state)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_smmu_state_data *smmu_state;

	if (!crtc || !crtc->state) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
	SDE_EVT32_VERBOSE(DRMID(crtc));
	smmu_state = &sde_crtc->smmu_state;

	/* complete secure transitions if any */
	if (smmu_state->transition_type == POST_COMMIT)
		sde_crtc_secure_ctrl(crtc, true);
}

/* _sde_crtc_set_idle_timeout - update idle timeout wait duration */
static void _sde_crtc_set_idle_timeout(struct drm_crtc *crtc, u64 val)
{
	struct drm_encoder *encoder;

	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	drm_for_each_encoder(encoder, crtc->dev) {
		if (encoder->crtc != crtc)
			continue;

		sde_encoder_set_idle_timeout(encoder, (u32) val);
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
 * _sde_crtc_set_dim_layer_v1 - copy dim layer settings from userspace
 * @cstate:      Pointer to sde crtc state
 * @user_ptr:    User ptr for sde_drm_dim_layer_v1 struct
 */
static void _sde_crtc_set_dim_layer_v1(struct sde_crtc_state *cstate,
		void __user *usr_ptr)
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
 * _sde_crtc_dest_scaler_init - allocate memory for scaler lut
 * @sde_crtc    :  Pointer to sde crtc
 * @catalog :  Pointer to mdss catalog info
 */
static void _sde_crtc_dest_scaler_init(struct sde_crtc *sde_crtc,
				struct sde_mdss_cfg *catalog)
{
	if (!sde_crtc || !catalog)
		return;

	if (!catalog->mdp[0].has_dest_scaler) {
		SDE_DEBUG("dest scaler feature not supported\n");
		return;
	}

	sde_crtc->scl3_lut_cfg = kzalloc(sizeof(struct sde_hw_scaler3_lut_cfg),
				GFP_KERNEL);
	if (!sde_crtc->scl3_lut_cfg)
		SDE_ERROR("failed to create scale LUT for dest scaler");
}

/**
 * _sde_crtc_set_dest_scaler - copy dest scaler settings from userspace
 * @sde_crtc   :  Pointer to sde crtc
 * @cstate :  Pointer to sde crtc state
 * @usr_ptr:  User ptr for sde_drm_dest_scaler_data struct
 */
static int _sde_crtc_set_dest_scaler(struct sde_crtc *sde_crtc,
				struct sde_crtc_state *cstate,
				void __user *usr_ptr)
{
	struct sde_drm_dest_scaler_data ds_data;
	struct sde_drm_dest_scaler_cfg *ds_cfg_usr;
	struct sde_drm_scaler_v2 scaler_v2;
	void __user *scaler_v2_usr;
	int i, count, ret = 0;

	if (!sde_crtc || !cstate) {
		SDE_ERROR("invalid sde_crtc/state\n");
		return -EINVAL;
	}

	SDE_DEBUG("crtc %s\n", sde_crtc->name);

	cstate->num_ds = 0;
	cstate->ds_dirty = false;
	if (!usr_ptr) {
		SDE_DEBUG("ds data removed\n");
		return 0;
	}

	if (copy_from_user(&ds_data, usr_ptr, sizeof(ds_data))) {
		SDE_ERROR("failed to copy dest scaler data from user\n");
		return -EINVAL;
	}

	count = ds_data.num_dest_scaler;
	if (!count) {
		SDE_DEBUG("no ds data available\n");
		return 0;
	}

	if (!sde_crtc->num_mixers || count > sde_crtc->num_mixers ||
		(count && (count != sde_crtc->num_mixers) &&
		!(ds_data.ds_cfg[0].flags & SDE_DRM_DESTSCALER_PU_ENABLE))) {
		SDE_ERROR("invalid config:num ds(%d), mixers(%d),flags(%d)\n",
			count, sde_crtc->num_mixers, ds_data.ds_cfg[0].flags);
		return -EINVAL;
	}

	/* Populate from user space */
	for (i = 0; i < count; i++) {
		ds_cfg_usr = &ds_data.ds_cfg[i];

		cstate->ds_cfg[i].ndx = ds_cfg_usr->index;
		cstate->ds_cfg[i].flags = ds_cfg_usr->flags;
		cstate->ds_cfg[i].lm_width = ds_cfg_usr->lm_width;
		cstate->ds_cfg[i].lm_height = ds_cfg_usr->lm_height;
		cstate->ds_cfg[i].scl3_cfg = NULL;

		if (ds_cfg_usr->scaler_cfg) {
			scaler_v2_usr =
			(void __user *)((uintptr_t)ds_cfg_usr->scaler_cfg);

			memset(&scaler_v2, 0, sizeof(scaler_v2));

			cstate->ds_cfg[i].scl3_cfg =
				kzalloc(sizeof(struct sde_hw_scaler3_cfg),
					GFP_KERNEL);

			if (!cstate->ds_cfg[i].scl3_cfg) {
				ret = -ENOMEM;
				goto err;
			}

			if (copy_from_user(&scaler_v2, scaler_v2_usr,
					sizeof(scaler_v2))) {
				SDE_ERROR("scale data:copy from user failed\n");
				ret = -EINVAL;
				goto err;
			}

			sde_set_scaler_v2(cstate->ds_cfg[i].scl3_cfg,
					&scaler_v2);

			SDE_DEBUG("en(%d)dir(%d)de(%d) src(%dx%d) dst(%dx%d)\n",
				scaler_v2.enable, scaler_v2.dir_en,
				scaler_v2.de.enable, scaler_v2.src_width[0],
				scaler_v2.src_height[0], scaler_v2.dst_width,
				scaler_v2.dst_height);
			SDE_EVT32_VERBOSE(DRMID(&sde_crtc->base),
				scaler_v2.enable, scaler_v2.dir_en,
				scaler_v2.de.enable, scaler_v2.src_width[0],
				scaler_v2.src_height[0], scaler_v2.dst_width,
				scaler_v2.dst_height);
		}

		SDE_DEBUG("ds cfg[%d]-ndx(%d) flags(%d) lm(%dx%d)\n",
			i, ds_cfg_usr->index, ds_cfg_usr->flags,
			ds_cfg_usr->lm_width, ds_cfg_usr->lm_height);
		SDE_EVT32_VERBOSE(DRMID(&sde_crtc->base), i, ds_cfg_usr->index,
			ds_cfg_usr->flags, ds_cfg_usr->lm_width,
			ds_cfg_usr->lm_height);
	}

	cstate->num_ds = count;
	cstate->ds_dirty = true;
	return 0;

err:
	for (; i >= 0; i--)
		kfree(cstate->ds_cfg[i].scl3_cfg);

	return ret;
}

/**
 * _sde_crtc_check_dest_scaler_data - validate the dest scaler data
 * @crtc  :  Pointer to drm crtc
 * @state :  Pointer to drm crtc state
 */
static int _sde_crtc_check_dest_scaler_data(struct drm_crtc *crtc,
				struct drm_crtc_state *state)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate;
	struct drm_display_mode *mode;
	struct sde_kms *kms;
	struct sde_hw_ds *hw_ds;
	struct sde_hw_ds_cfg *cfg;
	u32 i, ret = 0, lm_idx;
	u32 num_ds_enable = 0;
	u32 max_in_width = 0, max_out_width = 0;
	u32 prev_lm_width = 0, prev_lm_height = 0;

	if (!crtc || !state)
		return -EINVAL;

	sde_crtc = to_sde_crtc(crtc);
	cstate = to_sde_crtc_state(state);
	kms = _sde_crtc_get_kms(crtc);
	mode = &state->adjusted_mode;

	SDE_DEBUG("crtc%d\n", crtc->base.id);

	if (!cstate->ds_dirty && !cstate->num_ds_enabled) {
		SDE_DEBUG("dest scaler property not set, skip validation\n");
		return 0;
	}

	if (!kms || !kms->catalog) {
		SDE_ERROR("invalid parameters\n");
		return -EINVAL;
	}

	if (!kms->catalog->mdp[0].has_dest_scaler) {
		SDE_DEBUG("dest scaler feature not supported\n");
		return 0;
	}

	if (!sde_crtc->num_mixers) {
		SDE_ERROR("mixers not allocated\n");
		return -EINVAL;
	}

	/**
	 * Check if sufficient hw resources are
	 * available as per target caps & topology
	 */
	if (sde_crtc->num_mixers > CRTC_DUAL_MIXERS) {
		SDE_ERROR("invalid config: mixers(%d) max(%d)\n",
			sde_crtc->num_mixers, CRTC_DUAL_MIXERS);
		ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < sde_crtc->num_mixers; i++) {
		if (!sde_crtc->mixers[i].hw_lm || !sde_crtc->mixers[i].hw_ds) {
			SDE_ERROR("insufficient HW resources allocated\n");
			ret = -EINVAL;
			goto err;
		}
	}

	/**
	 * Check if DS needs to be enabled or disabled
	 * In case of enable, validate the data
	 */
	if (!cstate->ds_dirty || !cstate->num_ds ||
		!(cstate->ds_cfg[0].flags & SDE_DRM_DESTSCALER_ENABLE)) {
		SDE_DEBUG("disable dest scaler,dirty(%d)num(%d)flags(%d)\n",
			cstate->ds_dirty, cstate->num_ds,
			cstate->ds_cfg[0].flags);
		goto disable;
	}

	/**
	 * No of dest scalers shouldn't exceed hw ds block count and
	 * also, match the num of mixers unless it is partial update
	 * left only/right only use case - currently PU + DS is not supported
	 */
	if (cstate->num_ds > kms->catalog->ds_count ||
		((cstate->num_ds != sde_crtc->num_mixers) &&
		!(cstate->ds_cfg[0].flags & SDE_DRM_DESTSCALER_PU_ENABLE))) {
		SDE_ERROR("invalid cfg: num_ds(%d), hw_ds_cnt(%d) flags(%d)\n",
			cstate->num_ds, kms->catalog->ds_count,
			cstate->ds_cfg[0].flags);
		ret = -EINVAL;
		goto err;
	}

	/* Validate the DS data */
	for (i = 0; i < cstate->num_ds; i++) {
		cfg = &cstate->ds_cfg[i];
		lm_idx = cfg->ndx;

		/**
		 * Validate against topology
		 * No of dest scalers should match the num of mixers
		 * unless it is partial update left only/right only use case
		 */
		if (lm_idx >= sde_crtc->num_mixers || (i != lm_idx &&
			!(cfg->flags & SDE_DRM_DESTSCALER_PU_ENABLE))) {
			SDE_ERROR("invalid user data(%d):idx(%d), flags(%d)\n",
				i, lm_idx, cfg->flags);
			ret = -EINVAL;
			goto err;
		}

		hw_ds = sde_crtc->mixers[lm_idx].hw_ds;

		if (!max_in_width && !max_out_width) {
			max_in_width = hw_ds->scl->top->maxinputwidth;
			max_out_width = hw_ds->scl->top->maxoutputwidth;

			if (cstate->num_ds == CRTC_DUAL_MIXERS)
				max_in_width -= SDE_DS_OVERFETCH_SIZE;

			SDE_DEBUG("max DS width [%d,%d] for num_ds = %d\n",
				max_in_width, max_out_width, cstate->num_ds);
		}

		/* Check LM width and height */
		if (cfg->lm_width > (mode->hdisplay/sde_crtc->num_mixers) ||
			cfg->lm_height > mode->vdisplay ||
			!cfg->lm_width || !cfg->lm_height) {
			SDE_ERROR("invalid lm size[%d,%d] display [%d,%d]\n",
				cfg->lm_width,
				cfg->lm_height,
				mode->hdisplay/sde_crtc->num_mixers,
				mode->vdisplay);
			ret = -E2BIG;
			goto err;
		}

		if (!prev_lm_width && !prev_lm_height) {
			prev_lm_width = cfg->lm_width;
			prev_lm_height = cfg->lm_height;
		} else {
			if (cfg->lm_width != prev_lm_width ||
				cfg->lm_height != prev_lm_height) {
				SDE_ERROR("lm size:left[%d,%d], right[%d %d]\n",
					cfg->lm_width, cfg->lm_height,
					prev_lm_width, prev_lm_height);
				ret = -EINVAL;
				goto err;
			}
		}

		/* Check scaler data */
		if (cfg->flags & SDE_DRM_DESTSCALER_SCALE_UPDATE ||
			cfg->flags & SDE_DRM_DESTSCALER_ENHANCER_UPDATE) {
			if (!cfg->scl3_cfg) {
				ret = -EINVAL;
				SDE_ERROR("null scale data\n");
				goto err;
			}
			if (cfg->scl3_cfg->src_width[0] > max_in_width ||
				cfg->scl3_cfg->dst_width > max_out_width ||
				!cfg->scl3_cfg->src_width[0] ||
				!cfg->scl3_cfg->dst_width) {
				SDE_ERROR("scale width(%d %d) for ds-%d:\n",
					cfg->scl3_cfg->src_width[0],
					cfg->scl3_cfg->dst_width,
					hw_ds->idx - DS_0);
				SDE_ERROR("scale_en = %d, DE_en =%d\n",
					cfg->scl3_cfg->enable,
					cfg->scl3_cfg->de.enable);

				cfg->flags &=
					~SDE_DRM_DESTSCALER_SCALE_UPDATE;
				cfg->flags &=
					~SDE_DRM_DESTSCALER_ENHANCER_UPDATE;

				ret = -EINVAL;
				goto err;
			}
		}

		if (cfg->flags & SDE_DRM_DESTSCALER_ENABLE)
			num_ds_enable++;

		/**
		 * Validation successful, indicator for flush to be issued
		 */
		cfg->set_lm_flush = true;

		SDE_DEBUG("ds[%d]: flags = 0x%X\n",
			hw_ds->idx - DS_0, cfg->flags);
	}

disable:
	SDE_DEBUG("dest scaler enable status, old = %d, new = %d",
		cstate->num_ds_enabled, num_ds_enable);
	SDE_EVT32(DRMID(crtc), cstate->num_ds_enabled, num_ds_enable,
		cstate->ds_dirty);

	if (cstate->num_ds_enabled != num_ds_enable) {
		/* Disabling destination scaler */
		if (!num_ds_enable) {
			for (i = 0; i < sde_crtc->num_mixers; i++) {
				cfg = &cstate->ds_cfg[i];
				cfg->ndx = i;
				/* Update scaler settings in disable case */
				cfg->flags = SDE_DRM_DESTSCALER_SCALE_UPDATE;
				cfg->scl3_cfg->enable = 0;
				cfg->scl3_cfg->de.enable = 0;
				cfg->set_lm_flush = true;
			}
		}
		cstate->num_ds_enabled = num_ds_enable;
		cstate->ds_dirty = true;
	}

	return 0;

err:
	cstate->ds_dirty = false;
	return ret;
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
	struct sde_rm_hw_iter lm_iter, ctl_iter, dspp_iter, ds_iter;

	sde_rm_init_hw_iter(&lm_iter, enc->base.id, SDE_HW_BLK_LM);
	sde_rm_init_hw_iter(&ctl_iter, enc->base.id, SDE_HW_BLK_CTL);
	sde_rm_init_hw_iter(&dspp_iter, enc->base.id, SDE_HW_BLK_DSPP);
	sde_rm_init_hw_iter(&ds_iter, enc->base.id, SDE_HW_BLK_DS);

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

		/* DS may be null */
		(void) sde_rm_get_hw(rm, &ds_iter);
		mixer->hw_ds = (struct sde_hw_ds *)ds_iter.hw;

		mixer->encoder = enc;

		sde_crtc->num_mixers++;
		SDE_DEBUG("setup mixer %d: lm %d\n",
				i, mixer->hw_lm->idx - LM_0);
		SDE_DEBUG("setup mixer %d: ctl %d\n",
				i, mixer->hw_ctl->idx - CTL_0);
		if (mixer->hw_ds)
			SDE_DEBUG("setup mixer %d: ds %d\n",
				i, mixer->hw_ds->idx - DS_0);
	}
}

static void _sde_crtc_setup_mixers(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	struct drm_encoder *enc;

	sde_crtc->num_mixers = 0;
	sde_crtc->mixers_swapped = false;
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
	crtc_split_width = sde_crtc_get_mixer_width(sde_crtc, cstate, adj_mode);

	for (i = 0; i < sde_crtc->num_mixers; i++) {
		cstate->lm_bounds[i].x = crtc_split_width * i;
		cstate->lm_bounds[i].y = 0;
		cstate->lm_bounds[i].w = crtc_split_width;
		cstate->lm_bounds[i].h =
			sde_crtc_get_mixer_height(sde_crtc, cstate, adj_mode);
		memcpy(&cstate->lm_roi[i], &cstate->lm_bounds[i],
				sizeof(cstate->lm_roi[i]));
		SDE_EVT32_VERBOSE(DRMID(crtc), i,
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
	struct sde_crtc_smmu_state_data *smmu_state;

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
	smmu_state = &sde_crtc->smmu_state;

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
	_sde_crtc_dest_scaler_setup(crtc);

	/*
	 * Since CP properties use AXI buffer to program the
	 * HW, check if context bank is in attached
	 * state,
	 * apply color processing properties only if
	 * smmu state is attached,
	 */
	if ((smmu_state->state != DETACHED) &&
			(smmu_state->state != DETACH_ALL_REQ))
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
	struct msm_drm_private *priv;
	struct msm_drm_thread *event_thread;
	unsigned long flags;
	struct sde_crtc_state *cstate;

	if (!crtc || !crtc->dev || !crtc->dev->dev_private) {
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
	priv = dev->dev_private;

	if (crtc->index >= ARRAY_SIZE(priv->event_thread)) {
		SDE_ERROR("invalid crtc index[%d]\n", crtc->index);
		return;
	}

	event_thread = &priv->event_thread[crtc->index];

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

	/*
	 * For planes without commit update, drm framework will not add
	 * those planes to current state since hardware update is not
	 * required. However, if those planes were power collapsed since
	 * last commit cycle, driver has to restore the hardware state
	 * of those planes explicitly here prior to plane flush.
	 */
	drm_atomic_crtc_for_each_plane(plane, crtc)
		sde_plane_restore(plane);

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
	drm_atomic_crtc_for_each_plane(plane, crtc) {
		if (sde_crtc->smmu_state.transition_error)
			sde_plane_set_error(plane, true);
		sde_plane_flush(plane);
	}

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
			&cstate->property_state);
}

static int _sde_crtc_wait_for_frame_done(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;
	int ret, rc = 0, i;

	if (!crtc) {
		SDE_ERROR("invalid argument\n");
		return -EINVAL;
	}
	sde_crtc = to_sde_crtc(crtc);

	if (!atomic_read(&sde_crtc->frame_pending)) {
		SDE_DEBUG("no frames pending\n");
		return 0;
	}

	SDE_EVT32(DRMID(crtc), SDE_EVTLOG_FUNC_ENTRY);

	/*
	 * flush all the event thread work to make sure all the
	 * FRAME_EVENTS from encoder are propagated to crtc
	 */
	for (i = 0; i < ARRAY_SIZE(sde_crtc->frame_events); i++) {
		if (list_empty(&sde_crtc->frame_events[i].list))
			kthread_flush_work(&sde_crtc->frame_events[i].work);
	}

	ret = wait_for_completion_timeout(&sde_crtc->frame_done_comp,
			msecs_to_jiffies(SDE_FRAME_DONE_TIMEOUT));
	if (!ret) {
		SDE_ERROR("frame done completion wait timed out, ret:%d\n",
				ret);
		SDE_EVT32(DRMID(crtc), SDE_EVTLOG_FATAL);
		rc = -ETIMEDOUT;
	}
	SDE_EVT32_VERBOSE(DRMID(crtc), SDE_EVTLOG_FUNC_EXIT);

	return rc;
}

static int _sde_crtc_commit_kickoff_rot(struct drm_crtc *crtc,
		struct sde_crtc_state *cstate)
{
	struct drm_plane *plane;
	struct sde_crtc *sde_crtc;
	struct sde_hw_ctl *ctl, *master_ctl;
	u32 flush_mask;
	int i, rc = 0;

	if (!crtc || !cstate)
		return -EINVAL;

	sde_crtc = to_sde_crtc(crtc);

	/*
	 * Update sbuf configuration and flush bits if a flush
	 * mask has been defined for either the current or
	 * previous commit.
	 *
	 * Updates are also required for the first commit after
	 * sbuf_flush_mask becomes 0x0, to properly transition
	 * the hardware out of sbuf mode.
	 */
	if (!sde_crtc->sbuf_flush_mask_old && !sde_crtc->sbuf_flush_mask)
		return 0;

	flush_mask = sde_crtc->sbuf_flush_mask_old | sde_crtc->sbuf_flush_mask;
	sde_crtc->sbuf_flush_mask_old = sde_crtc->sbuf_flush_mask;

	SDE_ATRACE_BEGIN("crtc_kickoff_rot");

	if (cstate->sbuf_cfg.rot_op_mode != SDE_CTL_ROT_OP_MODE_OFFLINE) {
		drm_atomic_crtc_for_each_plane(plane, crtc) {
			rc = sde_plane_kickoff_rot(plane);
			if (rc) {
				SDE_ERROR("crtc%d cancelling inline rotation\n",
						crtc->base.id);
				SDE_EVT32(DRMID(crtc), SDE_EVTLOG_ERROR);

				/* revert to offline on errors */
				cstate->sbuf_cfg.rot_op_mode =
					SDE_CTL_ROT_OP_MODE_OFFLINE;
				break;
			}
		}
	}

	master_ctl = NULL;
	for (i = 0; i < sde_crtc->num_mixers; i++) {
		ctl = sde_crtc->mixers[i].hw_ctl;
		if (!ctl)
			continue;

		if (!master_ctl || master_ctl->idx > ctl->idx)
			master_ctl = ctl;
	}

	/* only update sbuf_cfg and flush for master ctl */
	if (master_ctl && master_ctl->ops.setup_sbuf_cfg &&
			master_ctl->ops.update_pending_flush) {
		master_ctl->ops.setup_sbuf_cfg(master_ctl, &cstate->sbuf_cfg);
		master_ctl->ops.update_pending_flush(master_ctl, flush_mask);

		/* explicitly trigger rotator for async modes */
		if (cstate->sbuf_cfg.rot_op_mode ==
				SDE_CTL_ROT_OP_MODE_INLINE_ASYNC &&
				master_ctl->ops.trigger_rot_start) {
			master_ctl->ops.trigger_rot_start(master_ctl);
			SDE_EVT32(DRMID(crtc), master_ctl->idx - CTL_0);
		}
	}

	SDE_ATRACE_END("crtc_kickoff_rot");
	return rc;
}

/**
 * _sde_crtc_remove_pipe_flush - remove staged pipes from flush mask
 * @sde_crtc: Pointer to sde crtc structure
 */
static void _sde_crtc_remove_pipe_flush(struct sde_crtc *sde_crtc)
{
	struct sde_crtc_mixer *mixer;
	struct sde_hw_ctl *ctl;
	u32 i, flush_mask;

	if (!sde_crtc)
		return;

	mixer = sde_crtc->mixers;
	for (i = 0; i < sde_crtc->num_mixers; i++) {
		ctl = mixer[i].hw_ctl;
		if (!ctl || !ctl->ops.get_pending_flush ||
				!ctl->ops.clear_pending_flush ||
				!ctl->ops.update_pending_flush)
			continue;

		flush_mask = ctl->ops.get_pending_flush(ctl);
		flush_mask &= ~mixer[i].pipe_mask;
		ctl->ops.clear_pending_flush(ctl);
		ctl->ops.update_pending_flush(ctl, flush_mask);
	}
}

void sde_crtc_commit_kickoff(struct drm_crtc *crtc)
{
	struct drm_encoder *encoder;
	struct drm_device *dev;
	struct sde_crtc *sde_crtc;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct sde_crtc_state *cstate;
	bool is_error;
	int ret;

	if (!crtc) {
		SDE_ERROR("invalid argument\n");
		return;
	}
	dev = crtc->dev;
	sde_crtc = to_sde_crtc(crtc);
	sde_kms = _sde_crtc_get_kms(crtc);
	is_error = false;

	if (!sde_kms || !sde_kms->dev || !sde_kms->dev->dev_private) {
		SDE_ERROR("invalid argument\n");
		return;
	}

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

	/* default to ASYNC mode for inline rotation */
	cstate->sbuf_cfg.rot_op_mode = sde_crtc->sbuf_flush_mask ?
		SDE_CTL_ROT_OP_MODE_INLINE_ASYNC : SDE_CTL_ROT_OP_MODE_OFFLINE;

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

		/*
		 * For inline ASYNC modes, the flush bits are not written
		 * to hardware atomically, so avoid using it if a video
		 * mode encoder is active on this CRTC.
		 */
		if (cstate->sbuf_cfg.rot_op_mode ==
				SDE_CTL_ROT_OP_MODE_INLINE_ASYNC &&
				sde_encoder_get_intf_mode(encoder) ==
				INTF_MODE_VIDEO)
			cstate->sbuf_cfg.rot_op_mode =
				SDE_CTL_ROT_OP_MODE_INLINE_SYNC;
	}

	/*
	 * For ASYNC inline modes, kick off the rotator now so that the H/W
	 * can start as soon as it's ready.
	 */
	if (cstate->sbuf_cfg.rot_op_mode == SDE_CTL_ROT_OP_MODE_INLINE_ASYNC)
		if (_sde_crtc_commit_kickoff_rot(crtc, cstate))
			is_error = true;

	/* wait for frame_event_done completion */
	SDE_ATRACE_BEGIN("wait_for_frame_done_event");
	ret = _sde_crtc_wait_for_frame_done(crtc);
	SDE_ATRACE_END("wait_for_frame_done_event");
	if (ret) {
		SDE_ERROR("crtc%d wait for frame done failed;frame_pending%d\n",
				crtc->base.id,
				atomic_read(&sde_crtc->frame_pending));

		is_error = true;

		/* force offline rotation mode since the commit has no pipes */
		cstate->sbuf_cfg.rot_op_mode = SDE_CTL_ROT_OP_MODE_OFFLINE;
	}

	if (atomic_inc_return(&sde_crtc->frame_pending) == 1) {
		/* acquire bandwidth and other resources */
		SDE_DEBUG("crtc%d first commit\n", crtc->base.id);
		SDE_EVT32(DRMID(crtc), cstate->sbuf_cfg.rot_op_mode,
				SDE_EVTLOG_FUNC_CASE1);
	} else {
		SDE_DEBUG("crtc%d commit\n", crtc->base.id);
		SDE_EVT32(DRMID(crtc), cstate->sbuf_cfg.rot_op_mode,
				SDE_EVTLOG_FUNC_CASE2);
	}
	sde_crtc->play_count++;

	/*
	 * For SYNC inline modes, delay the kick off until after the
	 * wait for frame done in case the wait times out.
	 *
	 * Also perform a final kickoff when transitioning back to
	 * offline mode.
	 */
	if (cstate->sbuf_cfg.rot_op_mode != SDE_CTL_ROT_OP_MODE_INLINE_ASYNC)
		if (_sde_crtc_commit_kickoff_rot(crtc, cstate))
			is_error = true;

	sde_vbif_clear_errors(sde_kms);

	if (is_error)
		_sde_crtc_remove_pipe_flush(sde_crtc);

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (encoder->crtc != crtc)
			continue;

		sde_encoder_kickoff(encoder, is_error);
	}

	reinit_completion(&sde_crtc->frame_done_comp);
	SDE_ATRACE_END("crtc_commit");
	return;
}

/**
 * _sde_crtc_vblank_enable_no_lock - update power resource and vblank request
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

	if (!sde_crtc) {
		SDE_ERROR("invalid crtc\n");
		return -EINVAL;
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
			return ret;

		list_for_each_entry(enc, &dev->mode_config.encoder_list, head) {
			if (enc->crtc != crtc)
				continue;

			SDE_EVT32(DRMID(&sde_crtc->base), DRMID(enc), enable,
					sde_crtc->enabled,
					sde_crtc->suspend,
					sde_crtc->vblank_requested);

			sde_encoder_register_vblank_callback(enc,
					sde_crtc_vblank_cb, (void *)crtc);
		}
	} else {
		list_for_each_entry(enc, &dev->mode_config.encoder_list, head) {
			if (enc->crtc != crtc)
				continue;

			SDE_EVT32(DRMID(&sde_crtc->base), DRMID(enc), enable,
					sde_crtc->enabled,
					sde_crtc->suspend,
					sde_crtc->vblank_requested);

			sde_encoder_register_vblank_callback(enc, NULL, NULL);
		}

		/* drop lock since power crtc cb may try to re-acquire lock */
		mutex_unlock(&sde_crtc->crtc_lock);
		_sde_crtc_power_enable(sde_crtc, false);
		mutex_lock(&sde_crtc->crtc_lock);
	}

	return 0;
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
	int ret = 0;

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
	SDE_EVT32_VERBOSE(DRMID(crtc), enable);

	mutex_lock(&sde_crtc->crtc_lock);

	/*
	 * If the vblank is enabled, release a power reference on suspend
	 * and take it back during resume (if it is still enabled).
	 */
	SDE_EVT32(DRMID(&sde_crtc->base), enable, sde_crtc->enabled,
			sde_crtc->suspend, sde_crtc->vblank_requested);
	if (sde_crtc->suspend == enable)
		SDE_DEBUG("crtc%d suspend already set to %d, ignoring update\n",
				crtc->base.id, enable);
	else if (sde_crtc->enabled && sde_crtc->vblank_requested) {
		ret = _sde_crtc_vblank_enable_no_lock(sde_crtc, !enable);
		if (ret)
			SDE_ERROR("%s vblank enable failed: %d\n",
					sde_crtc->name, ret);
	}

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
			&cstate->property_state, cstate->property_values);

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
	if (sde_kms_is_suspend_state(crtc->dev)) {
		_sde_crtc_set_suspend(crtc, false);

		if (!sde_crtc_is_reset_required(crtc)) {
			SDE_DEBUG("avoiding reset for crtc:%d\n",
					crtc->base.id);
			return;
		}
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
			&cstate->property_state,
			cstate->property_values);

	_sde_crtc_set_input_fence_timeout(cstate);

	_sde_crtc_rp_reset(&cstate->rp, &sde_crtc->rp_lock,
			&sde_crtc->rp_head);

	cstate->base.crtc = crtc;
	crtc->state = &cstate->base;
}

static void sde_crtc_handle_power_event(u32 event_type, void *arg)
{
	struct drm_crtc *crtc = arg;
	struct sde_crtc *sde_crtc;
	struct drm_plane *plane;
	struct drm_encoder *encoder;
	struct sde_crtc_mixer *m;
	u32 i, misr_status;
	unsigned long flags;
	struct sde_crtc_irq_info *node = NULL;
	int ret = 0;

	if (!crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}
	sde_crtc = to_sde_crtc(crtc);

	mutex_lock(&sde_crtc->crtc_lock);

	SDE_EVT32(DRMID(crtc), event_type);

	switch (event_type) {
	case SDE_POWER_EVENT_POST_ENABLE:
		/* restore encoder; crtc will be programmed during commit */
		drm_for_each_encoder(encoder, crtc->dev) {
			if (encoder->crtc != crtc)
				continue;

			sde_encoder_virt_restore(encoder);
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

		sde_cp_crtc_post_ipc(crtc);

		for (i = 0; i < sde_crtc->num_mixers; ++i) {
			m = &sde_crtc->mixers[i];
			if (!m->hw_lm || !m->hw_lm->ops.setup_misr ||
					!sde_crtc->misr_enable)
				continue;

			m->hw_lm->ops.setup_misr(m->hw_lm, true,
					sde_crtc->misr_frame_count);
		}
		break;
	case SDE_POWER_EVENT_PRE_DISABLE:
		for (i = 0; i < sde_crtc->num_mixers; ++i) {
			m = &sde_crtc->mixers[i];
			if (!m->hw_lm || !m->hw_lm->ops.collect_misr ||
					!sde_crtc->misr_enable)
				continue;

			misr_status = m->hw_lm->ops.collect_misr(m->hw_lm);
			sde_crtc->misr_data[i] = misr_status ? misr_status :
							sde_crtc->misr_data[i];
		}

		spin_lock_irqsave(&sde_crtc->spin_lock, flags);
		node = NULL;
		list_for_each_entry(node, &sde_crtc->user_event_list, list) {
			ret = 0;
			if (node->func)
				ret = node->func(crtc, false, &node->irq);
			if (ret)
				SDE_ERROR("%s failed to disable event %x\n",
						sde_crtc->name, node->event);
		}
		spin_unlock_irqrestore(&sde_crtc->spin_lock, flags);

		sde_cp_crtc_pre_ipc(crtc);
		break;
	case SDE_POWER_EVENT_POST_DISABLE:
		/*
		 * set revalidate flag in planes, so it will be re-programmed
		 * in the next frame update
		 */
		drm_atomic_crtc_for_each_plane(plane, crtc)
			sde_plane_set_revalidate(plane, true);

		sde_cp_crtc_suspend(crtc);
		break;
	default:
		SDE_DEBUG("event:%d not handled\n", event_type);
		break;
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
	struct drm_event event;
	u32 power_on;
	int ret, i;

	if (!crtc || !crtc->dev || !crtc->dev->dev_private || !crtc->state) {
		SDE_ERROR("invalid crtc\n");
		return;
	}
	sde_crtc = to_sde_crtc(crtc);
	cstate = to_sde_crtc_state(crtc->state);
	priv = crtc->dev->dev_private;

	SDE_DEBUG("crtc%d\n", crtc->base.id);

	for (i = 0; i < cstate->num_connectors; i++)
		sde_connector_schedule_status_work(cstate->connectors[i],
							false);

	if (sde_kms_is_suspend_state(crtc->dev))
		_sde_crtc_set_suspend(crtc, true);

	mutex_lock(&sde_crtc->crtc_lock);
	SDE_EVT32_VERBOSE(DRMID(crtc));

	/* update color processing on suspend */
	event.type = DRM_EVENT_CRTC_POWER;
	event.length = sizeof(u32);
	sde_cp_crtc_suspend(crtc);
	power_on = 0;
	msm_mode_object_event_notify(&crtc->base, crtc->dev, &event,
			(u8 *)&power_on);

	/* wait for frame_event_done completion */
	if (_sde_crtc_wait_for_frame_done(crtc))
		SDE_ERROR("crtc%d wait for frame done failed;frame_pending%d\n",
				crtc->base.id,
				atomic_read(&sde_crtc->frame_pending));

	SDE_EVT32(DRMID(crtc), sde_crtc->enabled, sde_crtc->suspend,
			sde_crtc->vblank_requested,
			crtc->state->active, crtc->state->enable);
	if (sde_crtc->enabled && !sde_crtc->suspend &&
			sde_crtc->vblank_requested) {
		ret = _sde_crtc_vblank_enable_no_lock(sde_crtc, false);
		if (ret)
			SDE_ERROR("%s vblank enable failed: %d\n",
					sde_crtc->name, ret);
	}
	sde_crtc->enabled = false;

	if (atomic_read(&sde_crtc->frame_pending)) {
		SDE_EVT32(DRMID(crtc), atomic_read(&sde_crtc->frame_pending),
							SDE_EVTLOG_FUNC_CASE2);
		sde_core_perf_crtc_release_bw(crtc);
		atomic_set(&sde_crtc->frame_pending, 0);
	}

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

	/**
	 * All callbacks are unregistered and frame done waits are complete
	 * at this point. No buffers are accessed by hardware.
	 * reset the fence timeline if crtc will not be enabled for this commit
	 */
	if (!crtc->state->active || !crtc->state->enable) {
		sde_fence_signal(&sde_crtc->output_fence, ktime_get(), true);
		for (i = 0; i < cstate->num_connectors; ++i)
			sde_connector_commit_reset(cstate->connectors[i],
					ktime_get());
	}

	memset(sde_crtc->mixers, 0, sizeof(sde_crtc->mixers));
	sde_crtc->num_mixers = 0;
	sde_crtc->mixers_swapped = false;

	/* disable clk & bw control until clk & bw properties are set */
	cstate->bw_control = false;
	cstate->bw_split_vote = false;

	mutex_unlock(&sde_crtc->crtc_lock);
}

static void sde_crtc_enable(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;
	struct drm_encoder *encoder;
	struct msm_drm_private *priv;
	unsigned long flags;
	struct sde_crtc_irq_info *node = NULL;
	struct drm_event event;
	u32 power_on;
	int ret, i;
	struct sde_crtc_state *cstate;

	if (!crtc || !crtc->dev || !crtc->dev->dev_private) {
		SDE_ERROR("invalid crtc\n");
		return;
	}
	priv = crtc->dev->dev_private;
	cstate = to_sde_crtc_state(crtc->state);

	SDE_DEBUG("crtc%d\n", crtc->base.id);
	SDE_EVT32_VERBOSE(DRMID(crtc));
	sde_crtc = to_sde_crtc(crtc);

	mutex_lock(&sde_crtc->crtc_lock);
	SDE_EVT32(DRMID(crtc), sde_crtc->enabled, sde_crtc->suspend,
			sde_crtc->vblank_requested);

	/* return early if crtc is already enabled */
	if (sde_crtc->enabled) {
		if (msm_is_mode_seamless_dms(&crtc->state->adjusted_mode))
			SDE_DEBUG("%s extra crtc enable expected during DMS\n",
					sde_crtc->name);
		else
			WARN(1, "%s unexpected crtc enable\n", sde_crtc->name);

		mutex_unlock(&sde_crtc->crtc_lock);
		return;
	}

	drm_for_each_encoder(encoder, crtc->dev) {
		if (encoder->crtc != crtc)
			continue;
		sde_encoder_register_frame_event_callback(encoder,
				sde_crtc_frame_event_cb, (void *)crtc);
	}

	if (!sde_crtc->enabled && !sde_crtc->suspend &&
			sde_crtc->vblank_requested) {
		ret = _sde_crtc_vblank_enable_no_lock(sde_crtc, true);
		if (ret)
			SDE_ERROR("%s vblank enable failed: %d\n",
					sde_crtc->name, ret);
	}
	sde_crtc->enabled = true;

	/* update color processing on resume */
	event.type = DRM_EVENT_CRTC_POWER;
	event.length = sizeof(u32);
	sde_cp_crtc_resume(crtc);
	power_on = 1;
	msm_mode_object_event_notify(&crtc->base, crtc->dev, &event,
			(u8 *)&power_on);

	mutex_unlock(&sde_crtc->crtc_lock);

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
		SDE_POWER_EVENT_POST_ENABLE | SDE_POWER_EVENT_POST_DISABLE |
		SDE_POWER_EVENT_PRE_DISABLE,
		sde_crtc_handle_power_event, crtc, sde_crtc->name);

	for (i = 0; i < cstate->num_connectors; i++)
		sde_connector_schedule_status_work(cstate->connectors[i], true);
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
				pstate->crtc_w, pstate->crtc_h, false);
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

static int _sde_crtc_check_secure_state(struct drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	struct drm_encoder *encoder;
	struct sde_crtc_state *cstate;
	uint32_t secure;
	uint32_t fb_ns = 0, fb_sec = 0, fb_sec_dir = 0;
	int encoder_cnt = 0;
	int rc;

	if (!crtc || !state) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	cstate = to_sde_crtc_state(state);

	secure = sde_crtc_get_property(cstate,
			CRTC_PROP_SECURITY_LEVEL);

	rc = _sde_crtc_find_plane_fb_modes(state,
			&fb_ns,
			&fb_sec,
			&fb_sec_dir);
	if (rc)
		return rc;

	/**
	 * validate planes
	 * fb_sec_dir is for secure camera preview and secure display  use case,
	 * fb_sec is for secure video playback,
	 * fb_ns is for normal non secure use cases.
	 */
	if ((secure == SDE_DRM_SEC_ONLY) &&
			(fb_ns || fb_sec || (fb_sec && fb_sec_dir))) {
		SDE_ERROR(
		"crtc%d: invalid planes fb_modes Sec:%d, NS:%d, Sec_Dir:%d\n",
				crtc->base.id, fb_sec, fb_ns, fb_sec_dir);
		return -EINVAL;
	}

	/**
	 * secure_crtc is not allowed in a shared toppolgy
	 * across different encoders.
	 */
	if (fb_sec_dir) {
		drm_for_each_encoder(encoder, crtc->dev)
			if (encoder->crtc ==  crtc)
				encoder_cnt++;

		if (encoder_cnt >
			MAX_ALLOWED_ENCODER_CNT_PER_SECURE_CRTC) {
			SDE_ERROR(
				"crtc%d, invalid virtual encoder crtc%d\n",
				crtc->base.id,
				encoder_cnt);
			return -EINVAL;

		}
	}
	SDE_DEBUG("crtc:%d Secure validation successful\n", crtc->base.id);
	return 0;
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

	rc = _sde_crtc_check_dest_scaler_data(crtc, state);
	if (rc) {
		SDE_ERROR("crtc%d failed dest scaler check %d\n",
			crtc->base.id, rc);
		goto end;
	}

	mixer_width = sde_crtc_get_mixer_width(sde_crtc, cstate, mode);

	_sde_crtc_setup_is_ppsplit(state);
	_sde_crtc_setup_lm_bounds(crtc, state);

	rc = _sde_crtc_check_secure_state(crtc, state);
	if (rc)
		return rc;

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
			if (cstate->dim_layer[i].stage
					== (pstates[cnt].stage + SDE_STAGE_0)) {
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
				SDE_ERROR(
					"r1 only virt plane:%d not supported\n",
					pipe_staged[i]->plane->base.id);
				rc  = -EINVAL;
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

	if (!sde_kms) {
		SDE_ERROR("invalid argument\n");
		return;
	}

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
			"llcc_ab", 0x0, 0, U64_MAX,
			catalog->perf.max_bw_high * 1000ULL,
			CRTC_PROP_LLCC_AB);
	msm_property_install_range(&sde_crtc->property_info,
			"llcc_ib", 0x0, 0, U64_MAX,
			catalog->perf.max_bw_high * 1000ULL,
			CRTC_PROP_LLCC_IB);
	msm_property_install_range(&sde_crtc->property_info,
			"dram_ab", 0x0, 0, U64_MAX,
			catalog->perf.max_bw_high * 1000ULL,
			CRTC_PROP_DRAM_AB);
	msm_property_install_range(&sde_crtc->property_info,
			"dram_ib", 0x0, 0, U64_MAX,
			catalog->perf.max_bw_high * 1000ULL,
			CRTC_PROP_DRAM_IB);
	msm_property_install_range(&sde_crtc->property_info,
			"rot_prefill_bw", 0, 0, U64_MAX,
			catalog->perf.max_bw_high * 1000ULL,
			CRTC_PROP_ROT_PREFILL_BW);
	msm_property_install_range(&sde_crtc->property_info,
			"rot_clk", 0, 0, U64_MAX,
			sde_kms->perf.max_core_clk_rate,
			CRTC_PROP_ROT_CLK);

	msm_property_install_range(&sde_crtc->property_info,
		"idle_time", IDLE_TIMEOUT, 0, U64_MAX, 0,
		CRTC_PROP_IDLE_TIMEOUT);

	msm_property_install_blob(&sde_crtc->property_info, "capabilities",
		DRM_MODE_PROP_IMMUTABLE, CRTC_PROP_INFO);

	msm_property_install_volatile_range(&sde_crtc->property_info,
		"sde_drm_roi_v1", 0x0, 0, ~0, 0, CRTC_PROP_ROI_V1);

	msm_property_install_enum(&sde_crtc->property_info, "security_level",
			0x0, 0, e_secure_level,
			ARRAY_SIZE(e_secure_level),
			CRTC_PROP_SECURITY_LEVEL);

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

	if (catalog->mdp[0].has_dest_scaler) {
		sde_kms_info_add_keyint(info, "has_dest_scaler",
				catalog->mdp[0].has_dest_scaler);
		sde_kms_info_add_keyint(info, "dest_scaler_count",
					catalog->ds_count);

		if (catalog->ds[0].top) {
			sde_kms_info_add_keyint(info,
					"max_dest_scaler_input_width",
					catalog->ds[0].top->maxinputwidth);
			sde_kms_info_add_keyint(info,
					"max_dest_scaler_output_width",
					catalog->ds[0].top->maxinputwidth);
			sde_kms_info_add_keyint(info, "max_dest_scale_up",
					catalog->ds[0].top->maxupscale);
		}

		if (catalog->ds[0].features & BIT(SDE_SSPP_SCALER_QSEED3)) {
			msm_property_install_volatile_range(
					&sde_crtc->property_info, "dest_scaler",
					0x0, 0, ~0, 0, CRTC_PROP_DEST_SCALER);
			msm_property_install_blob(&sde_crtc->property_info,
					"ds_lut_ed", 0,
					CRTC_PROP_DEST_SCALER_LUT_ED);
			msm_property_install_blob(&sde_crtc->property_info,
					"ds_lut_cir", 0,
					CRTC_PROP_DEST_SCALER_LUT_CIR);
			msm_property_install_blob(&sde_crtc->property_info,
					"ds_lut_sep", 0,
					CRTC_PROP_DEST_SCALER_LUT_SEP);
		}
	}

	sde_kms_info_add_keyint(info, "has_src_split", catalog->has_src_split);
	if (catalog->perf.max_bw_low)
		sde_kms_info_add_keyint(info, "max_bandwidth_low",
				catalog->perf.max_bw_low * 1000LL);
	if (catalog->perf.max_bw_high)
		sde_kms_info_add_keyint(info, "max_bandwidth_high",
				catalog->perf.max_bw_high * 1000LL);
	if (catalog->perf.min_core_ib)
		sde_kms_info_add_keyint(info, "min_core_ib",
				catalog->perf.min_core_ib * 1000LL);
	if (catalog->perf.min_llcc_ib)
		sde_kms_info_add_keyint(info, "min_llcc_ib",
				catalog->perf.min_llcc_ib * 1000LL);
	if (catalog->perf.min_dram_ib)
		sde_kms_info_add_keyint(info, "min_dram_ib",
				catalog->perf.min_dram_ib * 1000LL);
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
				&cstate->property_state, property, val);
		if (!ret) {
			idx = msm_property_index(&sde_crtc->property_info,
					property);
			switch (idx) {
			case CRTC_PROP_INPUT_FENCE_TIMEOUT:
				_sde_crtc_set_input_fence_timeout(cstate);
				break;
			case CRTC_PROP_DIM_LAYER_V1:
				_sde_crtc_set_dim_layer_v1(cstate,
							(void __user *)val);
				break;
			case CRTC_PROP_ROI_V1:
				ret = _sde_crtc_set_roi_v1(state,
							(void __user *)val);
				break;
			case CRTC_PROP_DEST_SCALER:
				ret = _sde_crtc_set_dest_scaler(sde_crtc,
						cstate, (void __user *)val);
				break;
			case CRTC_PROP_DEST_SCALER_LUT_ED:
			case CRTC_PROP_DEST_SCALER_LUT_CIR:
			case CRTC_PROP_DEST_SCALER_LUT_SEP:
				ret = _sde_crtc_set_dest_scaler_lut(sde_crtc,
								cstate, idx);
				break;
			case CRTC_PROP_CORE_CLK:
			case CRTC_PROP_CORE_AB:
			case CRTC_PROP_CORE_IB:
				cstate->bw_control = true;
				break;
			case CRTC_PROP_LLCC_AB:
			case CRTC_PROP_LLCC_IB:
			case CRTC_PROP_DRAM_AB:
			case CRTC_PROP_DRAM_IB:
				cstate->bw_control = true;
				cstate->bw_split_vote = true;
				break;
			case CRTC_PROP_IDLE_TIMEOUT:
				_sde_crtc_set_idle_timeout(crtc, val);
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
	struct drm_encoder *encoder;
	int i, ret = -EINVAL;
	bool conn_offset = 0;
	bool is_cmd = true;

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

		/**
		 * set the cmd flag only when all the encoders attached
		 * to the crtc are in cmd mode. Consider all other cases
		 * as video mode.
		 */
		drm_for_each_encoder(encoder, crtc->dev) {
			if (encoder->crtc == crtc)
				is_cmd = sde_encoder_check_mode(encoder,
						MSM_DISPLAY_CAP_CMD_MODE);
		}

		i = msm_property_index(&sde_crtc->property_info, property);
		if (i == CRTC_PROP_OUTPUT_FENCE) {
			uint32_t offset = sde_crtc_get_property(cstate,
					CRTC_PROP_OUTPUT_FENCE_OFFSET);

			/**
			 * set the offset to 0 only for cmd mode panels, so
			 * the release fence for the current frame can be
			 * triggered right after PP_DONE interrupt.
			 */
			offset = is_cmd ? 0 : (offset + conn_offset);

			ret = sde_fence_create(&sde_crtc->output_fence, val,
								offset);
			if (ret)
				SDE_ERROR("fence create failed\n");
		} else {
			ret = msm_property_atomic_get(&sde_crtc->property_info,
					&cstate->property_state,
					property, val);
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
	out_width = sde_crtc_get_mixer_width(sde_crtc, cstate, mode);

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
			"vblank fps:%lld count:%u total:%llums total_framecount:%llu\n",
				fps, sde_crtc->vblank_cb_count,
				ktime_to_ms(diff), sde_crtc->play_count);

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
	sde_crtc->misr_frame_count = frame_count;
	for (i = 0; i < sde_crtc->num_mixers; ++i) {
		sde_crtc->misr_data[i] = 0;
		m = &sde_crtc->mixers[i];
		if (!m->hw_lm || !m->hw_lm->ops.setup_misr)
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
	u32 misr_status;
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
		if (!m->hw_lm || !m->hw_lm->ops.collect_misr)
			continue;

		misr_status = m->hw_lm->ops.collect_misr(m->hw_lm);
		sde_crtc->misr_data[i] = misr_status ? misr_status :
							sde_crtc->misr_data[i];
		len += snprintf(buf + len, MISR_BUFF_SIZE - len, "lm idx:%d\n",
					m->hw_lm->idx - LM_0);
		len += snprintf(buf + len, MISR_BUFF_SIZE - len, "0x%x\n",
							sde_crtc->misr_data[i]);
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
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	struct sde_crtc_state *cstate = to_sde_crtc_state(crtc->state);
	struct sde_crtc_res *res;
	struct sde_crtc_respool *rp;
	int i;

	seq_printf(s, "num_connectors: %d\n", cstate->num_connectors);
	seq_printf(s, "client type: %d\n", sde_crtc_get_client_type(crtc));
	seq_printf(s, "intf_mode: %d\n", sde_crtc_get_intf_mode(crtc));
	seq_printf(s, "core_clk_rate: %llu\n",
			sde_crtc->cur_perf.core_clk_rate);
	for (i = SDE_POWER_HANDLE_DBUS_ID_MNOC;
			i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++) {
		seq_printf(s, "bw_ctl[%s]: %llu\n",
				sde_power_handle_get_dbus_name(i),
				sde_crtc->cur_perf.bw_ctl[i]);
		seq_printf(s, "max_per_pipe_ib[%s]: %llu\n",
				sde_power_handle_get_dbus_name(i),
				sde_crtc->cur_perf.max_per_pipe_ib[i]);
	}

	mutex_lock(&sde_crtc->rp_lock);
	list_for_each_entry(rp, &sde_crtc->rp_head, rp_list) {
		seq_printf(s, "rp.%d: ", rp->sequence_id);
		list_for_each_entry(res, &rp->res_list, list)
			seq_printf(s, "0x%x/0x%llx/%pK/%d ",
					res->type, res->tag, res->val,
					atomic_read(&res->refcount));
		seq_puts(s, "\n");
	}
	mutex_unlock(&sde_crtc->rp_lock);

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
	debugfs_create_file("status", 0400,
			sde_crtc->debugfs_root,
			sde_crtc, &debugfs_status_fops);
	debugfs_create_file("state", 0600,
			sde_crtc->debugfs_root,
			&sde_crtc->base,
			&sde_crtc_debugfs_state_fops);
	debugfs_create_file("misr_data", 0600, sde_crtc->debugfs_root,
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
	struct msm_drm_private *priv;
	struct sde_crtc_event *event = NULL;
	u32 crtc_id;

	if (!crtc || !crtc->dev || !crtc->dev->dev_private || !func) {
		SDE_ERROR("invalid parameters\n");
		return -EINVAL;
	}
	sde_crtc = to_sde_crtc(crtc);
	priv = crtc->dev->dev_private;
	crtc_id = drm_crtc_index(crtc);

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
	kthread_queue_work(&priv->event_thread[crtc_id].worker,
			&event->kt_work);

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

	INIT_LIST_HEAD(&sde_crtc->retire_event_list);
	for (i = 0; i < ARRAY_SIZE(sde_crtc->retire_events); i++)
		INIT_LIST_HEAD(&sde_crtc->retire_events[i].list);

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

	mutex_init(&sde_crtc->crtc_lock);
	spin_lock_init(&sde_crtc->spin_lock);
	atomic_set(&sde_crtc->frame_pending, 0);

	mutex_init(&sde_crtc->rp_lock);
	INIT_LIST_HEAD(&sde_crtc->rp_head);

	init_completion(&sde_crtc->frame_done_comp);
	sde_crtc->enabled = false;

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

	/* Init dest scaler */
	_sde_crtc_dest_scaler_init(sde_crtc, kms->catalog);

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
		INIT_LIST_HEAD(&node->irq.list);
		ret = node->func(crtc_drm, true, &node->irq);
		sde_power_resource_enable(&priv->phandle, kms->core_client,
				false);
	}

	if (!ret) {
		spin_lock_irqsave(&crtc->spin_lock, flags);
		/* irq is regiestered and enabled and set the state */
		node->state = IRQ_ENABLED;
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
		list_del(&node->list);
		kfree(node);
		return 0;
	}
	priv = kms->dev->dev_private;
	sde_power_resource_enable(&priv->phandle, kms->core_client, true);
	ret = node->func(crtc_drm, false, &node->irq);
	list_del(&node->list);
	kfree(node);
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

static int sde_crtc_idle_interrupt_handler(struct drm_crtc *crtc_drm,
	bool en, struct sde_irq_callback *irq)
{
	return 0;
}
