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
#include "sde_ad4.h"
#include "sde_hw_interrupts.h"
#include "sde_core_irq.h"

struct sde_cp_node {
	u32 property_id;
	u32 prop_flags;
	u32 feature;
	void *blob_ptr;
	uint64_t prop_val;
	const struct sde_pp_blk *pp_blk;
	struct list_head feature_list;
	struct list_head active_list;
	struct list_head dirty_list;
	bool is_dspp_feature;
	u32 prop_blob_sz;
	struct sde_irq_callback *irq;
};

struct sde_cp_prop_attach {
	struct drm_crtc *crtc;
	struct drm_property *prop;
	struct sde_cp_node *prop_node;
	u32 feature;
	uint64_t val;
};

static void dspp_pcc_install_property(struct drm_crtc *crtc);

static void dspp_hsic_install_property(struct drm_crtc *crtc);

static void dspp_ad_install_property(struct drm_crtc *crtc);

static void dspp_vlut_install_property(struct drm_crtc *crtc);

static void dspp_gamut_install_property(struct drm_crtc *crtc);

static void dspp_gc_install_property(struct drm_crtc *crtc);

typedef void (*dspp_prop_install_func_t)(struct drm_crtc *crtc);

static dspp_prop_install_func_t dspp_prop_install_func[SDE_DSPP_MAX];

static void sde_cp_update_list(struct sde_cp_node *prop_node,
		struct sde_crtc *crtc, bool dirty_list);

static int sde_cp_ad_validate_prop(struct sde_cp_node *prop_node,
		struct sde_crtc *crtc);

#define setup_dspp_prop_install_funcs(func) \
do { \
	func[SDE_DSPP_PCC] = dspp_pcc_install_property; \
	func[SDE_DSPP_HSIC] = dspp_hsic_install_property; \
	func[SDE_DSPP_AD] = dspp_ad_install_property; \
	func[SDE_DSPP_VLUT] = dspp_vlut_install_property; \
	func[SDE_DSPP_GAMUT] = dspp_gamut_install_property; \
	func[SDE_DSPP_GC] = dspp_gc_install_property; \
} while (0)

typedef void (*lm_prop_install_func_t)(struct drm_crtc *crtc);

static lm_prop_install_func_t lm_prop_install_func[SDE_MIXER_MAX];

static void lm_gc_install_property(struct drm_crtc *crtc);

#define setup_lm_prop_install_funcs(func) \
	(func[SDE_MIXER_GC] = lm_gc_install_property)

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
	SDE_CP_CRTC_DSPP_VLUT,
	SDE_CP_CRTC_DSPP_AD_MODE,
	SDE_CP_CRTC_DSPP_AD_INIT,
	SDE_CP_CRTC_DSPP_AD_CFG,
	SDE_CP_CRTC_DSPP_AD_INPUT,
	SDE_CP_CRTC_DSPP_AD_ASSERTIVENESS,
	SDE_CP_CRTC_DSPP_AD_BACKLIGHT,
	SDE_CP_CRTC_DSPP_MAX,
	/* DSPP features end */

	/* Append new LM features before SDE_CP_CRTC_MAX_FEATURES */
	/* LM feature start*/
	SDE_CP_CRTC_LM_GC,
	/* LM feature end*/

	SDE_CP_CRTC_MAX_FEATURES,
};

#define INIT_PROP_ATTACH(p, crtc, prop, node, feature, val) \
	do { \
		(p)->crtc = crtc; \
		(p)->prop = prop; \
		(p)->prop_node = node; \
		(p)->feature = feature; \
		(p)->val = val; \
	} while (0)

static void sde_cp_get_hw_payload(struct sde_cp_node *prop_node,
				  struct sde_hw_cp_cfg *hw_cfg,
				  bool *feature_enabled)
{

	struct drm_property_blob *blob = NULL;

	memset(hw_cfg, 0, sizeof(*hw_cfg));
	*feature_enabled = false;

	blob = prop_node->blob_ptr;
	if (prop_node->prop_flags & DRM_MODE_PROP_BLOB) {
		if (blob) {
			hw_cfg->len = blob->length;
			hw_cfg->payload = blob->data;
			*feature_enabled = true;
		}
	} else if (prop_node->prop_flags & DRM_MODE_PROP_RANGE) {
		/* Check if local blob is Set */
		if (!blob) {
			if (prop_node->prop_val) {
				hw_cfg->len = sizeof(prop_node->prop_val);
				hw_cfg->payload = &prop_node->prop_val;
			}
		} else {
			hw_cfg->len = (prop_node->prop_val) ? blob->length :
					0;
			hw_cfg->payload = (prop_node->prop_val) ? blob->data
						: NULL;
		}
		if (prop_node->prop_val)
			*feature_enabled = true;
	} else if (prop_node->prop_flags & DRM_MODE_PROP_ENUM) {
		*feature_enabled = (prop_node->prop_val != 0);
		hw_cfg->len = sizeof(prop_node->prop_val);
		hw_cfg->payload = &prop_node->prop_val;
	} else {
		DRM_ERROR("property type is not supported\n");
	}
}

static int sde_cp_disable_crtc_blob_property(struct sde_cp_node *prop_node)
{
	struct drm_property_blob *blob = prop_node->blob_ptr;

	if (!blob)
		return -EINVAL;
	drm_property_unreference_blob(blob);
	prop_node->blob_ptr = NULL;
	return 0;
}

static int sde_cp_create_local_blob(struct drm_crtc *crtc, u32 feature, int len)
{
	int ret = -EINVAL;
	bool found = false;
	struct sde_cp_node *prop_node = NULL;
	struct drm_property_blob *blob_ptr;
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);

	list_for_each_entry(prop_node, &sde_crtc->feature_list, feature_list) {
		if (prop_node->feature == feature) {
			found = true;
			break;
		}
	}

	if (!found || !(prop_node->prop_flags & DRM_MODE_PROP_RANGE)) {
		DRM_ERROR("local blob create failed prop found %d flags %d\n",
		       found, prop_node->prop_flags);
		return ret;
	}

	blob_ptr = drm_property_create_blob(crtc->dev, len, NULL);
	ret = (IS_ERR_OR_NULL(blob_ptr)) ? PTR_ERR(blob_ptr) : 0;
	if (!ret)
		prop_node->blob_ptr = blob_ptr;

	return ret;
}

static void sde_cp_destroy_local_blob(struct sde_cp_node *prop_node)
{
	if (!(prop_node->prop_flags & DRM_MODE_PROP_BLOB) &&
		prop_node->blob_ptr)
		drm_property_unreference_blob(prop_node->blob_ptr);
}

static int sde_cp_handle_range_property(struct sde_cp_node *prop_node,
					uint64_t val)
{
	int ret = 0;
	struct drm_property_blob *blob_ptr = prop_node->blob_ptr;

	if (!blob_ptr) {
		prop_node->prop_val = val;
		return 0;
	}

	if (!val) {
		prop_node->prop_val = 0;
		return 0;
	}

	ret = copy_from_user(blob_ptr->data, (void *)val, blob_ptr->length);
	if (ret) {
		DRM_ERROR("failed to get the property info ret %d", ret);
		ret = -EFAULT;
	} else {
		prop_node->prop_val = val;
	}

	return ret;
}

static int sde_cp_disable_crtc_property(struct drm_crtc *crtc,
					 struct drm_property *property,
					 struct sde_cp_node *prop_node)
{
	int ret = -EINVAL;

	if (property->flags & DRM_MODE_PROP_BLOB) {
		ret = sde_cp_disable_crtc_blob_property(prop_node);
	} else if (property->flags & DRM_MODE_PROP_RANGE) {
		ret = sde_cp_handle_range_property(prop_node, 0);
	} else if (property->flags & DRM_MODE_PROP_ENUM) {
		ret = 0;
		prop_node->prop_val = 0;
	}
	return ret;
}

static int sde_cp_enable_crtc_blob_property(struct drm_crtc *crtc,
					       struct sde_cp_node *prop_node,
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
	if (blob->length != prop_node->prop_blob_sz) {
		DRM_ERROR("invalid blob len %zd exp %d feature %d\n",
		    blob->length, prop_node->prop_blob_sz, prop_node->feature);
		drm_property_unreference_blob(blob);
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
				       struct sde_cp_node *prop_node,
				       uint64_t val)
{
	int ret = -EINVAL;

	if (property->flags & DRM_MODE_PROP_BLOB) {
		ret = sde_cp_enable_crtc_blob_property(crtc, prop_node, val);
	} else if (property->flags & DRM_MODE_PROP_RANGE) {
		ret = sde_cp_handle_range_property(prop_node, val);
	} else if (property->flags & DRM_MODE_PROP_ENUM) {
		ret = 0;
		prop_node->prop_val = val;
	}
	return ret;
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

	if (prop_attach->feature < SDE_CP_CRTC_DSPP_MAX)
		prop_attach->prop_node->is_dspp_feature = true;
	else
		prop_attach->prop_node->is_dspp_feature = false;

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
	INIT_LIST_HEAD(&sde_crtc->ad_dirty);
	INIT_LIST_HEAD(&sde_crtc->ad_active);
}

static void sde_cp_crtc_install_immutable_property(struct drm_crtc *crtc,
						   char *name,
						   u32 feature)
{
	struct drm_property *prop;
	struct sde_cp_node *prop_node = NULL;
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
		prop = drm_property_create_range(crtc->dev,
				DRM_MODE_PROP_IMMUTABLE, name, 0, 1);
		if (!prop) {
			DRM_ERROR("property create failed: %s\n", name);
			kfree(prop_node);
			return;
		}
		priv->cp_property[feature] = prop;
	}

	INIT_PROP_ATTACH(&prop_attach, crtc, prop, prop_node,
				feature, val);
	sde_cp_crtc_prop_attach(&prop_attach);
}

static void sde_cp_crtc_install_range_property(struct drm_crtc *crtc,
					     char *name,
					     u32 feature,
					     uint64_t min, uint64_t max,
					     uint64_t val)
{
	struct drm_property *prop;
	struct sde_cp_node *prop_node = NULL;
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

	INIT_PROP_ATTACH(&prop_attach, crtc, prop, prop_node,
				feature, val);

	sde_cp_crtc_prop_attach(&prop_attach);
}

static void sde_cp_crtc_install_blob_property(struct drm_crtc *crtc, char *name,
			u32 feature, u32 blob_sz)
{
	struct drm_property *prop;
	struct sde_cp_node *prop_node = NULL;
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

	INIT_PROP_ATTACH(&prop_attach, crtc, prop, prop_node,
				feature, val);
	prop_node->prop_blob_sz = blob_sz;

	sde_cp_crtc_prop_attach(&prop_attach);
}

static void sde_cp_crtc_install_enum_property(struct drm_crtc *crtc,
	u32 feature, const struct drm_prop_enum_list *list, u32 enum_sz,
	char *name)
{
	struct drm_property *prop;
	struct sde_cp_node *prop_node = NULL;
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
		prop = drm_property_create_enum(crtc->dev, 0, name,
			list, enum_sz);
		if (!prop) {
			DRM_ERROR("property create failed: %s\n", name);
			kfree(prop_node);
			return;
		}
		priv->cp_property[feature] = prop;
	}

	INIT_PROP_ATTACH(&prop_attach, crtc, prop, prop_node,
				feature, val);

	sde_cp_crtc_prop_attach(&prop_attach);
}

static void sde_cp_crtc_setfeature(struct sde_cp_node *prop_node,
				   struct sde_crtc *sde_crtc, u32 last_feature)
{
	struct sde_hw_cp_cfg hw_cfg;
	struct sde_hw_mixer *hw_lm;
	struct sde_hw_dspp *hw_dspp;
	u32 num_mixers = sde_crtc->num_mixers;
	int i = 0;
	bool feature_enabled = false;
	int ret = 0;
	struct sde_ad_hw_cfg ad_cfg;

	sde_cp_get_hw_payload(prop_node, &hw_cfg, &feature_enabled);
	hw_cfg.num_of_mixers = sde_crtc->num_mixers;
	hw_cfg.displayh = sde_crtc->base.mode.hdisplay;
	hw_cfg.displayv = sde_crtc->base.mode.vdisplay;

	for (i = 0; i < num_mixers && !ret; i++) {
		hw_lm = sde_crtc->mixers[i].hw_lm;
		hw_dspp = sde_crtc->mixers[i].hw_dspp;
		hw_cfg.ctl = sde_crtc->mixers[i].hw_ctl;
		hw_cfg.mixer_info = hw_lm;
		if (i == num_mixers - 1)
			hw_cfg.last_feature = last_feature;
		else
			hw_cfg.last_feature = 0;
		switch (prop_node->feature) {
		case SDE_CP_CRTC_DSPP_VLUT:
			if (!hw_dspp || !hw_dspp->ops.setup_vlut) {
				ret = -EINVAL;
				continue;
			}
			hw_dspp->ops.setup_vlut(hw_dspp, &hw_cfg);
			break;
		case SDE_CP_CRTC_DSPP_PCC:
			if (!hw_dspp || !hw_dspp->ops.setup_pcc) {
				ret = -EINVAL;
				continue;
			}
			hw_dspp->ops.setup_pcc(hw_dspp, &hw_cfg);
			break;
		case SDE_CP_CRTC_DSPP_IGC:
			if (!hw_dspp || !hw_dspp->ops.setup_igc) {
				ret = -EINVAL;
				continue;
			}
			hw_dspp->ops.setup_igc(hw_dspp, &hw_cfg);
			break;
		case SDE_CP_CRTC_DSPP_GC:
			if (!hw_dspp || !hw_dspp->ops.setup_gc) {
				ret = -EINVAL;
				continue;
			}
			hw_dspp->ops.setup_gc(hw_dspp, &hw_cfg);
			break;
		case SDE_CP_CRTC_DSPP_HUE:
			if (!hw_dspp || !hw_dspp->ops.setup_hue) {
				ret = -EINVAL;
				continue;
			}
			hw_dspp->ops.setup_hue(hw_dspp, &hw_cfg);
			break;
		case SDE_CP_CRTC_DSPP_SAT:
			if (!hw_dspp || !hw_dspp->ops.setup_sat) {
				ret = -EINVAL;
				continue;
			}
			hw_dspp->ops.setup_sat(hw_dspp, &hw_cfg);
			break;
		case SDE_CP_CRTC_DSPP_VAL:
			if (!hw_dspp || !hw_dspp->ops.setup_val) {
				ret = -EINVAL;
				continue;
			}
			hw_dspp->ops.setup_val(hw_dspp, &hw_cfg);
			break;
		case SDE_CP_CRTC_DSPP_CONT:
			if (!hw_dspp || !hw_dspp->ops.setup_cont) {
				ret = -EINVAL;
				continue;
			}
			hw_dspp->ops.setup_cont(hw_dspp, &hw_cfg);
			break;
		case SDE_CP_CRTC_DSPP_MEMCOLOR:
			if (!hw_dspp || !hw_dspp->ops.setup_pa_memcolor) {
				ret = -EINVAL;
				continue;
			}
			hw_dspp->ops.setup_pa_memcolor(hw_dspp, &hw_cfg);
			break;
		case SDE_CP_CRTC_DSPP_SIXZONE:
			if (!hw_dspp || !hw_dspp->ops.setup_sixzone) {
				ret = -EINVAL;
				continue;
			}
			hw_dspp->ops.setup_sixzone(hw_dspp, &hw_cfg);
			break;
		case SDE_CP_CRTC_DSPP_GAMUT:
			if (!hw_dspp || !hw_dspp->ops.setup_gamut) {
				ret = -EINVAL;
				continue;
			}
			hw_dspp->ops.setup_gamut(hw_dspp, &hw_cfg);
			break;
		case SDE_CP_CRTC_LM_GC:
			if (!hw_lm || !hw_lm->ops.setup_gc) {
				ret = -EINVAL;
				continue;
			}
			hw_lm->ops.setup_gc(hw_lm, &hw_cfg);
			break;
		case SDE_CP_CRTC_DSPP_AD_MODE:
			if (!hw_dspp || !hw_dspp->ops.setup_ad) {
				ret = -EINVAL;
				continue;
			}
			ad_cfg.prop = AD_MODE;
			ad_cfg.hw_cfg = &hw_cfg;
			hw_dspp->ops.setup_ad(hw_dspp, &ad_cfg);
			break;
		case SDE_CP_CRTC_DSPP_AD_INIT:
			if (!hw_dspp || !hw_dspp->ops.setup_ad) {
				ret = -EINVAL;
				continue;
			}
			ad_cfg.prop = AD_INIT;
			ad_cfg.hw_cfg = &hw_cfg;
			hw_dspp->ops.setup_ad(hw_dspp, &ad_cfg);
			break;
		case SDE_CP_CRTC_DSPP_AD_CFG:
			if (!hw_dspp || !hw_dspp->ops.setup_ad) {
				ret = -EINVAL;
				continue;
			}
			ad_cfg.prop = AD_CFG;
			ad_cfg.hw_cfg = &hw_cfg;
			hw_dspp->ops.setup_ad(hw_dspp, &ad_cfg);
			break;
		case SDE_CP_CRTC_DSPP_AD_INPUT:
			if (!hw_dspp || !hw_dspp->ops.setup_ad) {
				ret = -EINVAL;
				continue;
			}
			ad_cfg.prop = AD_INPUT;
			ad_cfg.hw_cfg = &hw_cfg;
			hw_dspp->ops.setup_ad(hw_dspp, &ad_cfg);
			break;
		case SDE_CP_CRTC_DSPP_AD_ASSERTIVENESS:
			if (!hw_dspp || !hw_dspp->ops.setup_ad) {
				ret = -EINVAL;
				continue;
			}
			ad_cfg.prop = AD_ASSERTIVE;
			ad_cfg.hw_cfg = &hw_cfg;
			hw_dspp->ops.setup_ad(hw_dspp, &ad_cfg);
			break;
		case SDE_CP_CRTC_DSPP_AD_BACKLIGHT:
			if (!hw_dspp || !hw_dspp->ops.setup_ad) {
				ret = -EINVAL;
				continue;
			}
			ad_cfg.prop = AD_BACKLIGHT;
			ad_cfg.hw_cfg = &hw_cfg;
			hw_dspp->ops.setup_ad(hw_dspp, &ad_cfg);
			break;
		default:
			ret = -EINVAL;
			break;
		}
	}

	if (ret) {
		DRM_ERROR("failed to %s feature %d\n",
			((feature_enabled) ? "enable" : "disable"),
			prop_node->feature);
		return;
	}

	if (feature_enabled) {
		DRM_DEBUG_DRIVER("Add feature to active list %d\n",
				 prop_node->property_id);
		sde_cp_update_list(prop_node, sde_crtc, false);
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
	struct sde_cp_node *prop_node = NULL, *n = NULL;
	struct sde_hw_ctl *ctl;
	uint32_t flush_mask = 0;
	u32 num_mixers = 0, i = 0;
	u32 num_of_features;

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

	/* Check if dirty lists are empty and ad features are disabled for
	 * early return. If ad properties are active then we need to issue
	 * dspp flush.
	 **/
	if (list_empty(&sde_crtc->dirty_list) &&
		list_empty(&sde_crtc->ad_dirty)) {
		if (list_empty(&sde_crtc->ad_active)) {
			DRM_DEBUG_DRIVER("Dirty list is empty\n");
			return;
		}
		set_dspp_flush = true;
	}

	num_of_features = 0;
	list_for_each_entry(prop_node, &sde_crtc->dirty_list, dirty_list)
		num_of_features++;

	list_for_each_entry_safe(prop_node, n, &sde_crtc->dirty_list,
				dirty_list) {
		num_of_features--;
		sde_cp_crtc_setfeature(prop_node, sde_crtc,
				(num_of_features == 0));
		/* Set the flush flag to true */
		if (prop_node->is_dspp_feature)
			set_dspp_flush = true;
		else
			set_lm_flush = true;
	}

	num_of_features = 0;
	list_for_each_entry(prop_node, &sde_crtc->ad_dirty, dirty_list)
		num_of_features++;

	list_for_each_entry_safe(prop_node, n, &sde_crtc->ad_dirty,
				dirty_list) {
		num_of_features--;
		set_dspp_flush = true;
		sde_cp_crtc_setfeature(prop_node, sde_crtc,
				(num_of_features == 0));
	}

	for (i = 0; i < num_mixers; i++) {
		ctl = sde_crtc->mixers[i].hw_ctl;
		if (!ctl)
			continue;
		if (set_dspp_flush && ctl->ops.get_bitmask_dspp
				&& sde_crtc->mixers[i].hw_dspp) {
			ctl->ops.get_bitmask_dspp(ctl,
					&flush_mask,
					sde_crtc->mixers[i].hw_dspp->idx);
			ctl->ops.update_pending_flush(ctl, flush_mask);
		}
		if (set_lm_flush && ctl->ops.get_bitmask_mixer
				&& sde_crtc->mixers[i].hw_lm) {
			flush_mask = ctl->ops.get_bitmask_mixer(ctl,
					sde_crtc->mixers[i].hw_lm->idx);
			ctl->ops.update_pending_flush(ctl, flush_mask);
		}
	}
}

void sde_cp_crtc_install_properties(struct drm_crtc *crtc)
{
	struct sde_kms *kms = NULL;
	struct sde_crtc *sde_crtc = NULL;
	struct sde_mdss_cfg *catalog = NULL;
	unsigned long features = 0;
	int i = 0;
	struct msm_drm_private *priv;

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
	if (!kms || !kms->catalog) {
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
	priv = crtc->dev->dev_private;
	/**
	 * DSPP/LM properties are global to all the CRTCS.
	 * Properties are created for first CRTC and re-used for later
	 * crtcs.
	 */
	if (!priv->cp_property) {
		priv->cp_property = kzalloc((sizeof(priv->cp_property) *
				SDE_CP_CRTC_MAX_FEATURES), GFP_KERNEL);
		setup_dspp_prop_install_funcs(dspp_prop_install_func);
		setup_lm_prop_install_funcs(lm_prop_install_func);
	}
	if (!priv->cp_property)
		return;

	if (!catalog->dspp_count)
		goto lm_property;

	/* Check for all the DSPP properties and attach it to CRTC */
	features = catalog->dspp[0].features;
	for (i = 0; i < SDE_DSPP_MAX; i++) {
		if (!test_bit(i, &features))
			continue;
		if (dspp_prop_install_func[i])
			dspp_prop_install_func[i](crtc);
	}

lm_property:
	if (!catalog->mixer_count)
		return;

	/* Check for all the LM properties and attach it to CRTC */
	features = catalog->mixer[0].features;
	for (i = 0; i < SDE_MIXER_MAX; i++) {
		if (!test_bit(i, &features))
			continue;
		if (lm_prop_install_func[i])
			lm_prop_install_func[i](crtc);
	}
}

int sde_cp_crtc_set_property(struct drm_crtc *crtc,
				struct drm_property *property,
				uint64_t val)
{
	struct sde_cp_node *prop_node = NULL;
	struct sde_crtc *sde_crtc = NULL;
	int ret = 0, i = 0, dspp_cnt, lm_cnt;
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
	/**
	 * sde_crtc is virtual ensure that hardware has been attached to the
	 * crtc. Check LM and dspp counts based on whether feature is a
	 * dspp/lm feature.
	 */
	if (!sde_crtc->num_mixers ||
	    sde_crtc->num_mixers > ARRAY_SIZE(sde_crtc->mixers)) {
		DRM_ERROR("Invalid mixer config act cnt %d max cnt %ld\n",
			sde_crtc->num_mixers, ARRAY_SIZE(sde_crtc->mixers));
		return -EINVAL;
	}

	dspp_cnt = 0;
	lm_cnt = 0;
	for (i = 0; i < sde_crtc->num_mixers; i++) {
		if (sde_crtc->mixers[i].hw_dspp)
			dspp_cnt++;
		if (sde_crtc->mixers[i].hw_lm)
			lm_cnt++;
	}

	if (prop_node->is_dspp_feature && dspp_cnt < sde_crtc->num_mixers) {
		DRM_ERROR("invalid dspp cnt %d mixer cnt %d\n", dspp_cnt,
			sde_crtc->num_mixers);
		return -EINVAL;
	} else if (lm_cnt < sde_crtc->num_mixers) {
		DRM_ERROR("invalid lm cnt %d mixer cnt %d\n", lm_cnt,
			sde_crtc->num_mixers);
		return -EINVAL;
	}

	ret = sde_cp_ad_validate_prop(prop_node, sde_crtc);
	if (ret) {
		DRM_ERROR("ad property validation failed ret %d\n", ret);
		return ret;
	}

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
		sde_cp_update_list(prop_node, sde_crtc, true);
	}
	return ret;
}

int sde_cp_crtc_get_property(struct drm_crtc *crtc,
			     struct drm_property *property, uint64_t *val)
{
	struct sde_cp_node *prop_node = NULL;
	struct sde_crtc *sde_crtc = NULL;

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
	/* Return 0 if property is not supported */
	*val = 0;
	list_for_each_entry(prop_node, &sde_crtc->feature_list, feature_list) {
		if (property->base.id == prop_node->property_id) {
			*val = prop_node->prop_val;
			break;
		}
	}
	return 0;
}

void sde_cp_crtc_destroy_properties(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = NULL;
	struct sde_cp_node *prop_node = NULL, *n = NULL;

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
		sde_cp_destroy_local_blob(prop_node);
		kfree(prop_node);
	}

	INIT_LIST_HEAD(&sde_crtc->active_list);
	INIT_LIST_HEAD(&sde_crtc->dirty_list);
	INIT_LIST_HEAD(&sde_crtc->feature_list);
}

void sde_cp_crtc_suspend(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = NULL;
	struct sde_cp_node *prop_node = NULL, *n = NULL;

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
		sde_cp_update_list(prop_node, sde_crtc, true);
		list_del_init(&prop_node->active_list);
	}
}

void sde_cp_crtc_resume(struct drm_crtc *crtc)
{
	/* placeholder for operations needed during resume */
}

static void dspp_pcc_install_property(struct drm_crtc *crtc)
{
	char feature_name[256];
	struct sde_kms *kms = NULL;
	struct sde_mdss_cfg *catalog = NULL;
	u32 version;

	kms = get_kms(crtc);
	catalog = kms->catalog;

	version = catalog->dspp[0].sblk->pcc.version >> 16;
	snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
		"SDE_DSPP_PCC_V", version);
	switch (version) {
	case 1:
		sde_cp_crtc_install_blob_property(crtc, feature_name,
			SDE_CP_CRTC_DSPP_PCC, sizeof(struct drm_msm_pcc));
		break;
	default:
		DRM_ERROR("version %d not supported\n", version);
		break;
	}
}

static void dspp_hsic_install_property(struct drm_crtc *crtc)
{
	char feature_name[256];
	struct sde_kms *kms = NULL;
	struct sde_mdss_cfg *catalog = NULL;
	u32 version;

	kms = get_kms(crtc);
	catalog = kms->catalog;
	version = catalog->dspp[0].sblk->hsic.version >> 16;
	switch (version) {
	case 1:
		snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
			"SDE_DSPP_HUE_V", version);
		sde_cp_crtc_install_range_property(crtc, feature_name,
			SDE_CP_CRTC_DSPP_HUE, 0, U32_MAX, 0);
		break;
	default:
		DRM_ERROR("version %d not supported\n", version);
		break;
	}
}

static void dspp_vlut_install_property(struct drm_crtc *crtc)
{
	char feature_name[256];
	struct sde_kms *kms = NULL;
	struct sde_mdss_cfg *catalog = NULL;
	u32 version;

	kms = get_kms(crtc);
	catalog = kms->catalog;
	version = catalog->dspp[0].sblk->vlut.version >> 16;
	snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
		"SDE_DSPP_VLUT_V", version);
	switch (version) {
	case 1:
		sde_cp_crtc_install_range_property(crtc, feature_name,
			SDE_CP_CRTC_DSPP_VLUT, 0, U64_MAX, 0);
		sde_cp_create_local_blob(crtc,
			SDE_CP_CRTC_DSPP_VLUT,
			sizeof(struct drm_msm_pa_vlut));
		break;
	default:
		DRM_ERROR("version %d not supported\n", version);
		break;
	}
}

static void dspp_ad_install_property(struct drm_crtc *crtc)
{
	char feature_name[256];
	struct sde_kms *kms = NULL;
	struct sde_mdss_cfg *catalog = NULL;
	u32 version;

	kms = get_kms(crtc);
	catalog = kms->catalog;
	version = catalog->dspp[0].sblk->ad.version >> 16;
	snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
		"SDE_DSPP_AD_V", version);
	switch (version) {
	case 3:
		sde_cp_crtc_install_immutable_property(crtc,
			feature_name, SDE_CP_CRTC_DSPP_AD);
		break;
	case 4:
		sde_cp_crtc_install_immutable_property(crtc,
			feature_name, SDE_CP_CRTC_DSPP_AD);

		sde_cp_crtc_install_enum_property(crtc,
			SDE_CP_CRTC_DSPP_AD_MODE, ad4_modes,
			ARRAY_SIZE(ad4_modes), "SDE_DSPP_AD_V4_MODE");

		sde_cp_crtc_install_range_property(crtc, "SDE_DSPP_AD_V4_INIT",
			SDE_CP_CRTC_DSPP_AD_INIT, 0, U64_MAX, 0);
		sde_cp_create_local_blob(crtc, SDE_CP_CRTC_DSPP_AD_INIT,
			sizeof(struct drm_msm_ad4_init));

		sde_cp_crtc_install_range_property(crtc, "SDE_DSPP_AD_V4_CFG",
			SDE_CP_CRTC_DSPP_AD_CFG, 0, U64_MAX, 0);
		sde_cp_create_local_blob(crtc, SDE_CP_CRTC_DSPP_AD_CFG,
			sizeof(struct drm_msm_ad4_cfg));
		sde_cp_crtc_install_range_property(crtc,
			"SDE_DSPP_AD_V4_ASSERTIVNESS",
			SDE_CP_CRTC_DSPP_AD_ASSERTIVENESS, 0, (BIT(8) - 1), 0);
		sde_cp_crtc_install_range_property(crtc, "SDE_DSPP_AD_V4_INPUT",
			SDE_CP_CRTC_DSPP_AD_INPUT, 0, U16_MAX, 0);
		sde_cp_crtc_install_range_property(crtc,
				"SDE_DSPP_AD_V4_BACKLIGHT",
			SDE_CP_CRTC_DSPP_AD_BACKLIGHT, 0, (BIT(16) - 1),
			0);
		break;
	default:
		DRM_ERROR("version %d not supported\n", version);
		break;
	}
}

static void lm_gc_install_property(struct drm_crtc *crtc)
{
	char feature_name[256];
	struct sde_kms *kms = NULL;
	struct sde_mdss_cfg *catalog = NULL;
	u32 version;

	kms = get_kms(crtc);
	catalog = kms->catalog;
	version = catalog->mixer[0].sblk->gc.version >> 16;
	snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
		 "SDE_LM_GC_V", version);
	switch (version) {
	case 1:
		sde_cp_crtc_install_blob_property(crtc, feature_name,
			SDE_CP_CRTC_LM_GC, sizeof(struct drm_msm_pgc_lut));
		break;
	default:
		DRM_ERROR("version %d not supported\n", version);
		break;
	}
}

static void dspp_gamut_install_property(struct drm_crtc *crtc)
{
	char feature_name[256];
	struct sde_kms *kms = NULL;
	struct sde_mdss_cfg *catalog = NULL;
	u32 version;

	kms = get_kms(crtc);
	catalog = kms->catalog;

	version = catalog->dspp[0].sblk->gamut.version >> 16;
	snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
		"SDE_DSPP_GAMUT_V", version);
	switch (version) {
	case 4:
		sde_cp_crtc_install_blob_property(crtc, feature_name,
			SDE_CP_CRTC_DSPP_GAMUT,
			sizeof(struct drm_msm_3d_gamut));
		break;
	default:
		DRM_ERROR("version %d not supported\n", version);
		break;
	}
}

static void dspp_gc_install_property(struct drm_crtc *crtc)
{
	char feature_name[256];
	struct sde_kms *kms = NULL;
	struct sde_mdss_cfg *catalog = NULL;
	u32 version;

	kms = get_kms(crtc);
	catalog = kms->catalog;

	version = catalog->dspp[0].sblk->gc.version >> 16;
	snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
		"SDE_DSPP_GC_V", version);
	switch (version) {
	case 1:
		sde_cp_crtc_install_blob_property(crtc, feature_name,
			SDE_CP_CRTC_DSPP_GC, sizeof(struct drm_msm_pgc_lut));
		break;
	default:
		DRM_ERROR("version %d not supported\n", version);
		break;
	}
}

static void sde_cp_update_list(struct sde_cp_node *prop_node,
		struct sde_crtc *crtc, bool dirty_list)
{
	switch (prop_node->feature) {
	case SDE_CP_CRTC_DSPP_AD_MODE:
	case SDE_CP_CRTC_DSPP_AD_INIT:
	case SDE_CP_CRTC_DSPP_AD_CFG:
	case SDE_CP_CRTC_DSPP_AD_INPUT:
	case SDE_CP_CRTC_DSPP_AD_ASSERTIVENESS:
	case SDE_CP_CRTC_DSPP_AD_BACKLIGHT:
		if (dirty_list)
			list_add_tail(&prop_node->dirty_list, &crtc->ad_dirty);
		else
			list_add_tail(&prop_node->active_list,
					&crtc->ad_active);
		break;
	default:
		/* color processing properties handle here */
		if (dirty_list)
			list_add_tail(&prop_node->dirty_list,
					&crtc->dirty_list);
		else
			list_add_tail(&prop_node->active_list,
					&crtc->active_list);
		break;
	};
}

static int sde_cp_ad_validate_prop(struct sde_cp_node *prop_node,
		struct sde_crtc *crtc)
{
	int i = 0, ret = 0;
	u32 ad_prop;

	for (i = 0; i < crtc->num_mixers && !ret; i++) {
		if (!crtc->mixers[i].hw_dspp) {
			ret = -EINVAL;
			continue;
		}
		switch (prop_node->feature) {
		case SDE_CP_CRTC_DSPP_AD_MODE:
			ad_prop = AD_MODE;
			break;
		case SDE_CP_CRTC_DSPP_AD_INIT:
			ad_prop = AD_INIT;
			break;
		case SDE_CP_CRTC_DSPP_AD_CFG:
			ad_prop = AD_CFG;
			break;
		case SDE_CP_CRTC_DSPP_AD_INPUT:
			ad_prop = AD_INPUT;
			break;
		case SDE_CP_CRTC_DSPP_AD_ASSERTIVENESS:
			ad_prop = AD_ASSERTIVE;
			break;
		case SDE_CP_CRTC_DSPP_AD_BACKLIGHT:
			ad_prop = AD_BACKLIGHT;
			break;
		default:
			/* Not an AD property */
			return 0;
		}
		if (!crtc->mixers[i].hw_dspp->ops.validate_ad)
			ret = -EINVAL;
		else
			ret = crtc->mixers[i].hw_dspp->ops.validate_ad(
				crtc->mixers[i].hw_dspp, &ad_prop);
	}
	return ret;
}

static void sde_cp_ad_interrupt_cb(void *arg, int irq_idx)
{
	struct sde_crtc *crtc = arg;
	struct drm_event event;
	uint32_t bl = 0;
	u32 num_mixers = crtc->num_mixers;
	struct sde_hw_mixer *hw_lm = NULL;
	struct sde_hw_dspp *hw_dspp = NULL;
	int i;

	event.type = DRM_EVENT_AD_BACKLIGHT;
	event.length = sizeof(bl);
	for (i = 0; i < num_mixers; i++) {
		hw_lm = crtc->mixers[i].hw_lm;
		hw_dspp = crtc->mixers[i].hw_dspp;
		if (!hw_lm->cfg.right_mixer)
			break;
	}

	if (!hw_dspp)
		return;

	hw_dspp->ops.ad_read_intr_resp(hw_dspp, AD4_BACKLIGHT, &bl);
	msm_send_crtc_notification(&crtc->base, &event, (u8 *)&bl);
}

int sde_cp_ad_interrupt(struct drm_crtc *crtc_drm, bool en,
	struct sde_irq_callback *ad_irq)
{
	struct sde_kms *kms = NULL;
	u32 num_mixers;
	struct sde_hw_mixer *hw_lm;
	struct sde_hw_dspp *hw_dspp = NULL;
	struct sde_crtc *crtc;
	int i;
	int irq_idx, ret;
	struct sde_cp_node prop_node;

	if (!crtc_drm || !ad_irq) {
		DRM_ERROR("invalid crtc %pK irq %pK\n", crtc_drm, ad_irq);
		return -EINVAL;
	}

	crtc = to_sde_crtc(crtc_drm);
	if (!crtc) {
		DRM_ERROR("invalid sde_crtc %pK\n", crtc);
		return -EINVAL;
	}

	mutex_lock(&crtc->crtc_lock);
	kms = get_kms(crtc_drm);
	num_mixers = crtc->num_mixers;

	memset(&prop_node, 0, sizeof(prop_node));
	prop_node.feature = SDE_CP_CRTC_DSPP_AD_BACKLIGHT;
	ret = sde_cp_ad_validate_prop(&prop_node, crtc);
	if (ret) {
		DRM_ERROR("Ad not supported ret %d\n", ret);
		goto exit;
	}

	for (i = 0; i < num_mixers; i++) {
		hw_lm = crtc->mixers[i].hw_lm;
		hw_dspp = crtc->mixers[i].hw_dspp;
		if (!hw_lm->cfg.right_mixer)
			break;
	}

	if (!hw_dspp) {
		DRM_ERROR("invalid dspp\n");
		ret = -EINVAL;
		goto exit;
	}

	irq_idx = sde_core_irq_idx_lookup(kms, SDE_IRQ_TYPE_AD4_BL_DONE,
			hw_dspp->idx);
	if (irq_idx < 0) {
		DRM_ERROR("failed to get the irq idx ret %d\n", irq_idx);
		ret = irq_idx;
		goto exit;
	}

	if (!en) {
		sde_core_irq_disable(kms, &irq_idx, 1);
		sde_core_irq_unregister_callback(kms, irq_idx, ad_irq);
		ret = 0;
		goto exit;
	}

	INIT_LIST_HEAD(&ad_irq->list);
	ad_irq->arg = crtc;
	ad_irq->func = sde_cp_ad_interrupt_cb;
	ret = sde_core_irq_register_callback(kms, irq_idx, ad_irq);
	if (ret) {
		DRM_ERROR("failed to register the callback ret %d\n", ret);
		goto exit;
	}
	ret = sde_core_irq_enable(kms, &irq_idx, 1);
	if (ret) {
		DRM_ERROR("failed to enable irq ret %d\n", ret);
		sde_core_irq_unregister_callback(kms, irq_idx, ad_irq);
	}
exit:
	mutex_unlock(&crtc->crtc_lock);
	return ret;
}
