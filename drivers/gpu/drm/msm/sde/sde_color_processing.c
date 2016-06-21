/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <drm/msm_drm_pp.h>
#include "sde_color_processing.h"
#include "sde_kms.h"
#include "sde_crtc.h"
#include "sde_hw_dspp.h"
#include "sde_hw_lm.h"

struct sde_color_process_node {
	u32 property_id;
	u32 prop_flags;
	u32 feature;
	void *blob_ptr;
	uint64_t prop_val;
	const struct sde_pp_blk *pp_blk;
	struct list_head feature_list;
	struct list_head active_list;
	struct list_head dirty_list;
	void (*dspp_feature_op)(struct sde_hw_dspp *ctx, void *cfg);
	void (*lm_feature_op)(struct sde_hw_mixer *mixer, void *cfg);
};

struct sde_cp_prop_attach {
	struct drm_crtc *crtc;
	struct drm_property *prop;
	struct sde_color_process_node *prop_node;
	const struct sde_pp_blk *pp_blk;
	u32 feature;
	void *ops;
	uint64_t val;
};

enum {
	/* Append new DSPP features before SDE_CP_CRTC_DSPP_MAX */
	/* DSPP Features start */
	SDE_CP_CRTC_DSPP_IGC,
	SDE_CP_CRTC_DSPP_PCC,
	SDE_CP_CRTC_DSPP_GC,
	SDE_CP_CRTC_DSPP_HUE,
	SDE_CP_CRTC_DSPP_SAT,
	SDE_CP_CRTC_DSPP_VAL,
	SDE_CP_CRTC_DSPP_CONT,
	SDE_CP_CRTC_DSPP_MEMCOLOR,
	SDE_CP_CRTC_DSPP_SIXZONE,
	SDE_CP_CRTC_DSPP_GAMUT,
	SDE_CP_CRTC_DSPP_DITHER,
	SDE_CP_CRTC_DSPP_HIST,
	SDE_CP_CRTC_DSPP_AD,
	SDE_CP_CRTC_DSPP_MAX,
	/* DSPP features end */

	/* Append new LM features before SDE_CP_CRTC_MAX_FEATURES */
	/* LM feature start*/
	SDE_CP_CRTC_LM_GC,
	/* LM feature end*/

	SDE_CP_CRTC_MAX_FEATURES,
};

#define INIT_PROP_ATTACH(p, crtc, prop, node, blk, feature, func, val) \
	do { \
		(p)->crtc = crtc; \
		(p)->prop = prop; \
		(p)->prop_node = node; \
		(p)->pp_blk = blk; \
		(p)->feature = feature; \
		(p)->ops = func; \
		(p)->val = val; \
	} while (0)

static int sde_cp_disable_crtc_blob_property(
				struct sde_color_process_node *prop_node)
{
	struct drm_property_blob *blob = prop_node->blob_ptr;

	if (!blob)
		return -EINVAL;
	drm_property_unreference_blob(blob);
	prop_node->blob_ptr = NULL;
	return 0;
}

static int sde_cp_disable_crtc_property(struct drm_crtc *crtc,
				 struct drm_property *property,
				 struct sde_color_process_node *prop_node)
{
	int ret = 0;

	if (property->flags & DRM_MODE_PROP_BLOB) {
		ret = sde_cp_disable_crtc_blob_property(prop_node);
	} else if (property->flags & DRM_MODE_PROP_RANGE) {
		prop_node->prop_val = 0;
		ret = 0;
	}
	return ret;
}

static int sde_cp_enable_crtc_blob_property(struct drm_crtc *crtc,
				       struct sde_color_process_node *prop_node,
				       uint64_t val)
{
	struct drm_property_blob *blob = NULL;

	/**
	 * For non-blob based properties add support to create a blob
	 * using the val and store the blob_ptr in prop_node.
	 */
	blob = drm_property_lookup_blob(crtc->dev, val);
	if (!blob) {
		DRM_ERROR("invalid blob id %lld\n", val);
		return -EINVAL;
	}
	/* Release refernce to existing payload of the property */
	if (prop_node->blob_ptr)
		drm_property_unreference_blob(prop_node->blob_ptr);

	prop_node->blob_ptr = blob;
	return 0;
}

static int sde_cp_enable_crtc_property(struct drm_crtc *crtc,
				       struct drm_property *property,
				       struct sde_color_process_node *prop_node,
				       uint64_t val)
{
	int ret = -EINVAL;

	if (property->flags & DRM_MODE_PROP_BLOB)
		ret = sde_cp_enable_crtc_blob_property(crtc, prop_node, val);
	else if (property->flags & DRM_MODE_PROP_RANGE) {
		ret = 0;
		prop_node->prop_val = val;
	}
	return ret;
}

static int sde_cp_crtc_get_mixer_idx(struct sde_crtc *sde_crtc)
{
	if (sde_crtc->num_mixers)
		return sde_crtc->mixers[0].hw_lm->idx;
	else
		return -EINVAL;
}

static struct sde_kms *get_kms(struct drm_crtc *crtc)
{
	struct msm_drm_private *priv = crtc->dev->dev_private;

	return to_sde_kms(priv->kms);
}

static void sde_cp_crtc_prop_attach(struct sde_cp_prop_attach *prop_attach)
{

	struct sde_crtc *sde_crtc = to_sde_crtc(prop_attach->crtc);

	drm_object_attach_property(&prop_attach->crtc->base,
				   prop_attach->prop, prop_attach->val);

	INIT_LIST_HEAD(&prop_attach->prop_node->active_list);
	INIT_LIST_HEAD(&prop_attach->prop_node->dirty_list);

	prop_attach->prop_node->property_id = prop_attach->prop->base.id;
	prop_attach->prop_node->prop_flags = prop_attach->prop->flags;
	prop_attach->prop_node->feature = prop_attach->feature;
	prop_attach->prop_node->pp_blk = prop_attach->pp_blk;

	if (prop_attach->feature < SDE_CP_CRTC_DSPP_MAX)
		prop_attach->prop_node->dspp_feature_op = prop_attach->ops;
	else
		prop_attach->prop_node->lm_feature_op = prop_attach->ops;

	list_add(&prop_attach->prop_node->feature_list,
		 &sde_crtc->feature_list);
}

void sde_cp_crtc_init(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = NULL;

	if (!crtc) {
		DRM_ERROR("invalid crtc %pK\n", crtc);
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
	if (!sde_crtc) {
		DRM_ERROR("invalid sde_crtc %pK\n", sde_crtc);
		return;
	}

	INIT_LIST_HEAD(&sde_crtc->active_list);
	INIT_LIST_HEAD(&sde_crtc->dirty_list);
	INIT_LIST_HEAD(&sde_crtc->feature_list);
}

static void sde_cp_crtc_install_immutable_property(struct drm_crtc *crtc,
						   char *name,
						   u32 feature)
{
	struct drm_property *prop;
	struct sde_color_process_node *prop_node = NULL;
	struct msm_drm_private *priv;
	struct sde_cp_prop_attach prop_attach;
	uint64_t val = 0;

	if (feature >=  SDE_CP_CRTC_MAX_FEATURES) {
		DRM_ERROR("invalid feature %d max %d\n", feature,
		       SDE_CP_CRTC_MAX_FEATURES);
		return;
	}

	prop_node = kzalloc(sizeof(*prop_node), GFP_KERNEL);
	if (!prop_node)
		return;

	priv = crtc->dev->dev_private;
	prop = priv->cp_property[feature];

	if (!prop) {
		prop = drm_property_create(crtc->dev, DRM_MODE_PROP_IMMUTABLE,
					   name, 0);
		if (!prop) {
			DRM_ERROR("property create failed: %s\n", name);
			kfree(prop_node);
			return;
		}
		priv->cp_property[feature] = prop;
	}

	INIT_PROP_ATTACH(&prop_attach, crtc, prop, prop_node, NULL,
				feature, NULL, val);
	sde_cp_crtc_prop_attach(&prop_attach);
}

static void sde_cp_crtc_install_range_property(struct drm_crtc *crtc,
					     char *name,
					     const struct sde_pp_blk *pp_blk,
					     u32 feature, void *ops,
					     uint64_t min, uint64_t max,
					     uint64_t val)
{
	struct drm_property *prop;
	struct sde_color_process_node *prop_node = NULL;
	struct msm_drm_private *priv;
	struct sde_cp_prop_attach prop_attach;

	if (feature >=  SDE_CP_CRTC_MAX_FEATURES) {
		DRM_ERROR("invalid feature %d max %d\n", feature,
			  SDE_CP_CRTC_MAX_FEATURES);
		return;
	}

	prop_node = kzalloc(sizeof(*prop_node), GFP_KERNEL);
	if (!prop_node)
		return;

	priv = crtc->dev->dev_private;
	prop = priv->cp_property[feature];

	if (!prop) {
		prop = drm_property_create_range(crtc->dev, 0, name, min, max);
		if (!prop) {
			DRM_ERROR("property create failed: %s\n", name);
			kfree(prop_node);
			return;
		}
		priv->cp_property[feature] = prop;
	}

	INIT_PROP_ATTACH(&prop_attach, crtc, prop, prop_node, pp_blk,
				feature, ops, val);

	sde_cp_crtc_prop_attach(&prop_attach);
}

static void sde_cp_crtc_create_blob_property(struct drm_crtc *crtc, char *name,
					     const struct sde_pp_blk *pp_blk,
					     u32 feature, void *ops)
{
	struct drm_property *prop;
	struct sde_color_process_node *prop_node = NULL;
	struct msm_drm_private *priv;
	uint64_t val = 0;
	struct sde_cp_prop_attach prop_attach;

	if (feature >=  SDE_CP_CRTC_MAX_FEATURES) {
		DRM_ERROR("invalid feature %d max %d\n", feature,
		       SDE_CP_CRTC_MAX_FEATURES);
		return;
	}

	prop_node = kzalloc(sizeof(*prop_node), GFP_KERNEL);
	if (!prop_node)
		return;

	priv = crtc->dev->dev_private;
	prop = priv->cp_property[feature];

	if (!prop) {
		prop = drm_property_create(crtc->dev,
					   DRM_MODE_PROP_BLOB, name, 0);
		if (!prop) {
			DRM_ERROR("property create failed: %s\n", name);
			kfree(prop_node);
			return;
		}
		priv->cp_property[feature] = prop;
	}

	INIT_PROP_ATTACH(&prop_attach, crtc, prop, prop_node, pp_blk,
				feature, ops, val);

	sde_cp_crtc_prop_attach(&prop_attach);
}


static void sde_cp_crtc_setfeature(struct sde_color_process_node *prop_node,
				   struct sde_crtc *sde_crtc)
{
	u32 num_mixers = sde_crtc->num_mixers;
	uint64_t val = 0;
	struct drm_property_blob *blob = NULL;
	struct sde_hw_cp_cfg hw_cfg;
	int i = 0;
	bool is_dspp = true;

	if (!prop_node->dspp_feature_op && !prop_node->lm_feature_op) {
		DRM_ERROR("ops not set for dspp/lm\n");
		return;
	}

	is_dspp = !prop_node->lm_feature_op;
	memset(&hw_cfg, 0, sizeof(hw_cfg));
	if (prop_node->prop_flags & DRM_MODE_PROP_BLOB) {
		blob = prop_node->blob_ptr;
		if (blob) {
			hw_cfg.len = blob->length;
			hw_cfg.payload = blob->data;
		}
	} else if (prop_node->prop_flags & DRM_MODE_PROP_RANGE) {
		val = prop_node->prop_val;
		hw_cfg.len = sizeof(prop_node->prop_val);
		hw_cfg.payload = &prop_node->prop_val;
	} else {
		DRM_ERROR("property type is not supported\n");
		return;
	}

	for (i = 0; i < num_mixers; i++) {
		if (is_dspp) {
			if (!sde_crtc->mixers[i].hw_dspp)
				continue;
			prop_node->dspp_feature_op(sde_crtc->mixers[i].hw_dspp,
						   &hw_cfg);
		} else {
			if (!sde_crtc->mixers[i].hw_lm)
				continue;
			prop_node->lm_feature_op(sde_crtc->mixers[i].hw_lm,
						 &hw_cfg);
		}
	}

	if (blob || val) {
		DRM_DEBUG_DRIVER("Add feature to active list %d\n",
				 prop_node->property_id);
		list_add_tail(&prop_node->active_list, &sde_crtc->active_list);
	} else {
		DRM_DEBUG_DRIVER("remove feature from active list %d\n",
			 prop_node->property_id);
		list_del_init(&prop_node->active_list);
	}
	/* Programming of feature done remove from dirty list */
	list_del_init(&prop_node->dirty_list);
}

void sde_cp_crtc_apply_properties(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = NULL;
	bool set_dspp_flush = false, set_lm_flush = false;
	struct sde_color_process_node *prop_node = NULL, *n = NULL;
	struct sde_hw_ctl *ctl;
	uint32_t flush_mask = 0;
	u32 num_mixers = 0, i = 0;

	if (!crtc || !crtc->dev) {
		DRM_ERROR("invalid crtc %pK dev %pK\n", crtc,
			  (crtc ? crtc->dev : NULL));
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
	if (!sde_crtc) {
		DRM_ERROR("invalid sde_crtc %pK\n", sde_crtc);
		return;
	}

	num_mixers = sde_crtc->num_mixers;
	if (!num_mixers) {
		DRM_DEBUG_DRIVER("no mixers for this crtc\n");
		return;
	}

	/* Check if dirty list is empty for early return */
	if (list_empty(&sde_crtc->dirty_list)) {
		DRM_DEBUG_DRIVER("Dirty list is empty\n");
		return;
	}

	list_for_each_entry_safe(prop_node, n, &sde_crtc->dirty_list,
							dirty_list) {
		sde_cp_crtc_setfeature(prop_node, sde_crtc);
		/* Set the flush flag to true */
		if (prop_node->dspp_feature_op)
			set_dspp_flush = true;
		else
			set_lm_flush = true;
	}

	for (i = 0; i < num_mixers; i++) {
		ctl = sde_crtc->mixers[i].hw_ctl;
		if (!ctl)
			continue;
		if (set_dspp_flush && ctl->ops.get_bitmask_dspp
				&& sde_crtc->mixers[i].hw_dspp)
			ctl->ops.get_bitmask_dspp(ctl,
					&flush_mask,
					sde_crtc->mixers[i].hw_dspp->idx);
			ctl->ops.update_pending_flush(ctl, flush_mask);
		if (set_lm_flush && ctl->ops.get_bitmask_mixer
				&& sde_crtc->mixers[i].hw_lm)
			flush_mask = ctl->ops.get_bitmask_mixer(ctl,
					sde_crtc->mixers[i].hw_lm->idx);
			ctl->ops.update_pending_flush(ctl, flush_mask);
	}
}

void sde_cp_crtc_install_properties(struct drm_crtc *crtc)
{
	struct sde_kms *kms = NULL;
	struct sde_crtc *sde_crtc = NULL;
	struct sde_mdss_cfg *catalog = NULL;
	unsigned long features = 0;
	int idx = 0, i = 0;
	char feature_name[256];
	struct msm_drm_private *priv;
	struct sde_hw_dspp *hw_dspp = NULL;
	struct sde_hw_mixer *hw_mixer = NULL;

	if (!crtc || !crtc->dev || !crtc->dev->dev_private) {
		DRM_ERROR("invalid crtc %pK dev %pK\n",
		       crtc, ((crtc) ? crtc->dev : NULL));
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
	if (!sde_crtc) {
		DRM_ERROR("sde_crtc %pK\n", sde_crtc);
		return;
	}

	kms = get_kms(crtc);
	if (!kms || !kms->catalog || !sde_crtc) {
		DRM_ERROR("invalid sde kms %pK catalog %pK sde_crtc %pK\n",
		 kms, ((kms) ? kms->catalog : NULL), sde_crtc);
		return;
	}

	/**
	 * Function can be called during the atomic_check with test_only flag
	 * and actual commit. Allocate properties only if feature list is
	 * empty during the atomic_check with test_only flag.
	 */
	if (!list_empty(&sde_crtc->feature_list))
		return;

	catalog = kms->catalog;
	idx = sde_cp_crtc_get_mixer_idx(sde_crtc);
	if (idx < 0 || idx >= catalog->mixer_count) {
		DRM_ERROR("invalid idx %d\n", idx);
		return;
	}

	priv = crtc->dev->dev_private;
	/**
	 * DSPP/LM properties are global to all the CRTCS.
	 * Properties are created for first CRTC and re-used for later
	 * crtcs.
	 */
	if (!priv->cp_property)
		priv->cp_property = kzalloc((sizeof(priv->cp_property) *
				SDE_CP_CRTC_MAX_FEATURES), GFP_KERNEL);
	if (!priv->cp_property)
		return;
	memset(feature_name, 0, sizeof(feature_name));

	if (idx >= catalog->dspp_count)
		goto lm_property;

	/* Check for all the DSPP properties and attach it to CRTC */
	hw_dspp = sde_crtc->mixers[0].hw_dspp;
	features = (hw_dspp) ? hw_dspp->cap->features : 0;

	if (!hw_dspp || !hw_dspp->cap->sblk)
		goto lm_property;

	for (i = 0; i < SDE_DSPP_MAX; i++) {
		if (!test_bit(i, &features))
			continue;
		switch (i) {
		case SDE_DSPP_PCC:
			snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
				"SDE_DSPP_PCC_V",
				(hw_dspp->cap->sblk->pcc.version >> 16));
			sde_cp_crtc_create_blob_property(crtc, feature_name,
				&hw_dspp->cap->sblk->pcc,
				SDE_CP_CRTC_DSPP_PCC,
				hw_dspp->ops.setup_pcc);
			break;
		case SDE_DSPP_HSIC:
			snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
				"SDE_DSPP_HUE_V",
				(hw_dspp->cap->sblk->hsic.version >> 16));
			sde_cp_crtc_install_range_property(crtc, feature_name,
				&hw_dspp->cap->sblk->hsic,
				SDE_CP_CRTC_DSPP_HUE, hw_dspp->ops.setup_hue,
				0, U32_MAX, 0);
			break;
		case SDE_DSPP_AD:
			snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
				"SDE_DSPP_AD_V",
				(hw_dspp->cap->sblk->ad.version >> 16));
			sde_cp_crtc_install_immutable_property(crtc,
					feature_name, SDE_CP_CRTC_DSPP_AD);
			break;
		default:
			break;
		}
	}

lm_property:
	/* Check for all the LM properties and attach it to CRTC */
	hw_mixer = sde_crtc->mixers[0].hw_lm;
	features = (hw_mixer) ? hw_mixer->cap->features : 0;

	if (!hw_mixer || !hw_mixer->cap->sblk)
		return;

	for (i = 0; i < SDE_MIXER_MAX; i++) {
		if (!test_bit(i, &features))
			continue;
		switch (i) {
		case SDE_MIXER_GC:
			snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
				 "SDE_LM_GC_V",
				 (hw_mixer->cap->sblk->gc.version >> 16));
			sde_cp_crtc_create_blob_property(crtc, feature_name,
				&hw_mixer->cap->sblk->gc,
				SDE_CP_CRTC_LM_GC,
				hw_mixer->ops.setup_gc);
			break;
		default:
			break;
		}
	}
}

int sde_cp_crtc_set_property(struct drm_crtc *crtc,
				struct drm_property *property,
				uint64_t val)
{
	struct sde_color_process_node *prop_node = NULL;
	struct sde_crtc *sde_crtc = NULL;
	int ret = 0;
	u8 found = 0;

	if (!crtc || !property) {
		DRM_ERROR("invalid crtc %pK property %pK\n", crtc, property);
		return -EINVAL;
	}

	sde_crtc = to_sde_crtc(crtc);
	if (!sde_crtc) {
		DRM_ERROR("invalid sde_crtc %pK\n", sde_crtc);
		return -EINVAL;
	}

	list_for_each_entry(prop_node, &sde_crtc->feature_list, feature_list) {
		if (property->base.id == prop_node->property_id) {
			found = 1;
			break;
		}
	}

	if (!found)
		return 0;

	/* remove the property from dirty list */
	list_del_init(&prop_node->dirty_list);

	if (!val)
		ret = sde_cp_disable_crtc_property(crtc, property, prop_node);
	else
		ret = sde_cp_enable_crtc_property(crtc, property,
						  prop_node, val);

	if (!ret) {
		/* remove the property from active list */
		list_del_init(&prop_node->active_list);
		/* Mark the feature as dirty */
		list_add_tail(&prop_node->dirty_list, &sde_crtc->dirty_list);
	}
	return ret;
}

int sde_cp_crtc_get_property(struct drm_crtc *crtc,
			     struct drm_property *property, uint64_t *val)
{
	struct sde_color_process_node *prop_node = NULL;
	struct sde_crtc *sde_crtc = NULL;
	int ret = -EINVAL;

	if (!crtc || !property || !val) {
		DRM_ERROR("invalid crtc %pK property %pK val %pK\n",
			  crtc, property, val);
		return -EINVAL;
	}

	sde_crtc = to_sde_crtc(crtc);
	if (!sde_crtc) {
		DRM_ERROR("invalid sde_crtc %pK\n", sde_crtc);
		return -EINVAL;
	}

	list_for_each_entry(prop_node, &sde_crtc->feature_list, feature_list) {
		if (property->base.id == prop_node->property_id) {
			*val = prop_node->prop_val;
			ret = 0;
			break;
		}
	}
	return ret;
}

void sde_cp_crtc_destroy_properties(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = NULL;
	struct sde_color_process_node *prop_node = NULL, *n = NULL;

	if (!crtc) {
		DRM_ERROR("invalid crtc %pK\n", crtc);
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
	if (!sde_crtc) {
		DRM_ERROR("invalid sde_crtc %pK\n", sde_crtc);
		return;
	}

	list_for_each_entry_safe(prop_node, n, &sde_crtc->feature_list,
				 feature_list) {
		if (prop_node->prop_flags & DRM_MODE_PROP_BLOB
		    && prop_node->blob_ptr)
			drm_property_unreference_blob(prop_node->blob_ptr);

		list_del_init(&prop_node->active_list);
		list_del_init(&prop_node->dirty_list);
		list_del_init(&prop_node->feature_list);
		kfree(prop_node);
	}

	INIT_LIST_HEAD(&sde_crtc->active_list);
	INIT_LIST_HEAD(&sde_crtc->dirty_list);
	INIT_LIST_HEAD(&sde_crtc->feature_list);
}

void sde_cp_crtc_suspend(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = NULL;
	struct sde_color_process_node *prop_node = NULL, *n = NULL;

	if (!crtc) {
		DRM_ERROR("crtc %pK\n", crtc);
		return;
	}
	sde_crtc = to_sde_crtc(crtc);
	if (!sde_crtc) {
		DRM_ERROR("sde_crtc %pK\n", sde_crtc);
		return;
	}

	list_for_each_entry_safe(prop_node, n, &sde_crtc->active_list,
				 active_list) {
		list_add_tail(&prop_node->dirty_list, &sde_crtc->dirty_list);
		list_del_init(&prop_node->active_list);
	}
}

void sde_cp_crtc_resume(struct drm_crtc *crtc)
{
	/* placeholder for operations needed during resume */
}
