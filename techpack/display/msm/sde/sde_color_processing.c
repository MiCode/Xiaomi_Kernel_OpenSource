// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/dma-buf.h>
#include <linux/string.h>
#include <drm/msm_drm_pp.h>
#include "sde_color_processing.h"
#include "sde_kms.h"
#include "sde_crtc.h"
#include "sde_hw_dspp.h"
#include "sde_hw_lm.h"
#include "sde_ad4.h"
#include "sde_hw_interrupts.h"
#include "sde_core_irq.h"
#include "dsi_panel.h"
#include "sde_hw_color_proc_common_v4.h"

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
	bool lm_flush_override;
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

#define ALIGNED_OFFSET (U32_MAX & ~(LTM_GUARD_BYTES))

static void dspp_pcc_install_property(struct drm_crtc *crtc);

static void dspp_hsic_install_property(struct drm_crtc *crtc);

static void dspp_memcolor_install_property(struct drm_crtc *crtc);

static void dspp_sixzone_install_property(struct drm_crtc *crtc);

static void dspp_ad_install_property(struct drm_crtc *crtc);

static void dspp_ltm_install_property(struct drm_crtc *crtc);

static void dspp_rc_install_property(struct drm_crtc *crtc);

static void dspp_spr_install_property(struct drm_crtc *crtc);

static void dspp_vlut_install_property(struct drm_crtc *crtc);

static void dspp_gamut_install_property(struct drm_crtc *crtc);

static void dspp_gc_install_property(struct drm_crtc *crtc);

static void dspp_igc_install_property(struct drm_crtc *crtc);

static void dspp_hist_install_property(struct drm_crtc *crtc);

static void dspp_dither_install_property(struct drm_crtc *crtc);

static void dspp_demura_install_property(struct drm_crtc *crtc);

typedef void (*dspp_prop_install_func_t)(struct drm_crtc *crtc);

static dspp_prop_install_func_t dspp_prop_install_func[SDE_DSPP_MAX];

static void sde_cp_update_list(struct sde_cp_node *prop_node,
		struct sde_crtc *crtc, bool dirty_list);
static int sde_cp_ad_validate_prop(struct sde_cp_node *prop_node,
		struct sde_crtc *crtc);

static void sde_cp_notify_ad_event(struct drm_crtc *crtc_drm, void *arg);

static void sde_cp_ad_set_prop(struct sde_crtc *sde_crtc,
		enum ad_property ad_prop);

static void sde_cp_notify_hist_event(struct drm_crtc *crtc_drm, void *arg);

static void _sde_cp_crtc_set_ltm_buffer(struct sde_crtc *sde_crtc, void *cfg);
static void _sde_cp_crtc_free_ltm_buffer(struct sde_crtc *sde_crtc, void *cfg);
static void _sde_cp_crtc_queue_ltm_buffer(struct sde_crtc *sde_crtc, void *cfg);
static int _sde_cp_crtc_get_ltm_buffer(struct sde_crtc *sde_crtc, u64 *addr);
static void _sde_cp_crtc_enable_ltm_hist(struct sde_crtc *sde_crtc,
		struct sde_hw_dspp *hw_dspp, struct sde_hw_cp_cfg *hw_cfg);
static void _sde_cp_crtc_disable_ltm_hist(struct sde_crtc *sde_crtc,
		struct sde_hw_dspp *hw_dspp, struct sde_hw_cp_cfg *hw_cfg);
static void sde_cp_notify_ltm_hist(struct drm_crtc *crtc_drm, void *arg);
static void sde_cp_notify_ltm_wb_pb(struct drm_crtc *crtc_drm, void *arg);
static void _sde_cp_crtc_update_ltm_roi(struct sde_crtc *sde_crtc,
		struct sde_hw_cp_cfg *hw_cfg);

#define setup_dspp_prop_install_funcs(func) \
do { \
	func[SDE_DSPP_PCC] = dspp_pcc_install_property; \
	func[SDE_DSPP_HSIC] = dspp_hsic_install_property; \
	func[SDE_DSPP_MEMCOLOR] = dspp_memcolor_install_property; \
	func[SDE_DSPP_SIXZONE] = dspp_sixzone_install_property; \
	func[SDE_DSPP_AD] = dspp_ad_install_property; \
	func[SDE_DSPP_LTM] = dspp_ltm_install_property; \
	func[SDE_DSPP_SPR] = dspp_spr_install_property; \
	func[SDE_DSPP_VLUT] = dspp_vlut_install_property; \
	func[SDE_DSPP_GAMUT] = dspp_gamut_install_property; \
	func[SDE_DSPP_GC] = dspp_gc_install_property; \
	func[SDE_DSPP_IGC] = dspp_igc_install_property; \
	func[SDE_DSPP_HIST] = dspp_hist_install_property; \
	func[SDE_DSPP_DITHER] = dspp_dither_install_property; \
	func[SDE_DSPP_RC] = dspp_rc_install_property; \
	func[SDE_DSPP_DEMURA] = dspp_demura_install_property; \
} while (0)

typedef void (*lm_prop_install_func_t)(struct drm_crtc *crtc);

static lm_prop_install_func_t lm_prop_install_func[SDE_MIXER_MAX];

static void lm_gc_install_property(struct drm_crtc *crtc);

#define setup_lm_prop_install_funcs(func) \
	(func[SDE_MIXER_GC] = lm_gc_install_property)

enum sde_cp_crtc_features {
	/* Append new DSPP features before SDE_CP_CRTC_DSPP_MAX */
	/* DSPP Features start */
	SDE_CP_CRTC_DSPP_IGC,
	SDE_CP_CRTC_DSPP_PCC,
	SDE_CP_CRTC_DSPP_GC,
	SDE_CP_CRTC_DSPP_HSIC,
	SDE_CP_CRTC_DSPP_MEMCOL_SKIN,
	SDE_CP_CRTC_DSPP_MEMCOL_SKY,
	SDE_CP_CRTC_DSPP_MEMCOL_FOLIAGE,
	SDE_CP_CRTC_DSPP_MEMCOL_PROT,
	SDE_CP_CRTC_DSPP_SIXZONE,
	SDE_CP_CRTC_DSPP_GAMUT,
	SDE_CP_CRTC_DSPP_DITHER,
	SDE_CP_CRTC_DSPP_HIST_CTRL,
	SDE_CP_CRTC_DSPP_HIST_IRQ,
	SDE_CP_CRTC_DSPP_AD,
	SDE_CP_CRTC_DSPP_VLUT,
	SDE_CP_CRTC_DSPP_AD_MODE,
	SDE_CP_CRTC_DSPP_AD_INIT,
	SDE_CP_CRTC_DSPP_AD_CFG,
	SDE_CP_CRTC_DSPP_AD_INPUT,
	SDE_CP_CRTC_DSPP_AD_ASSERTIVENESS,
	SDE_CP_CRTC_DSPP_AD_BACKLIGHT,
	SDE_CP_CRTC_DSPP_AD_STRENGTH,
	SDE_CP_CRTC_DSPP_AD_ROI,
	SDE_CP_CRTC_DSPP_LTM,
	SDE_CP_CRTC_DSPP_LTM_INIT,
	SDE_CP_CRTC_DSPP_LTM_ROI,
	SDE_CP_CRTC_DSPP_LTM_HIST_CTL,
	SDE_CP_CRTC_DSPP_LTM_HIST_THRESH,
	SDE_CP_CRTC_DSPP_LTM_SET_BUF,
	SDE_CP_CRTC_DSPP_LTM_QUEUE_BUF,
	SDE_CP_CRTC_DSPP_LTM_QUEUE_BUF2,
	SDE_CP_CRTC_DSPP_LTM_QUEUE_BUF3,
	SDE_CP_CRTC_DSPP_LTM_VLUT,
	SDE_CP_CRTC_DSPP_SB,
	SDE_CP_CRTC_DSPP_RC_MASK,
	SDE_CP_CRTC_DSPP_SPR_INIT,
	SDE_CP_CRTC_DSPP_DEMURA_INIT,
	SDE_CP_CRTC_DSPP_DEMURA_BACKLIGHT,
	SDE_CP_CRTC_DSPP_MAX,
	/* DSPP features end */

	/* Append new LM features before SDE_CP_CRTC_MAX_FEATURES */
	/* LM feature start*/
	SDE_CP_CRTC_LM_GC,
	/* LM feature end*/

	SDE_CP_CRTC_MAX_FEATURES,
};

enum sde_cp_crtc_pu_features {
	SDE_CP_CRTC_DSPP_RC_PU,
	SDE_CP_CRTC_DSPP_SPR_PU,
	SDE_CP_CRTC_MAX_PU_FEATURES,
};

static enum sde_cp_crtc_pu_features
		sde_cp_crtc_pu_to_feature[SDE_CP_CRTC_MAX_PU_FEATURES] = {
	[SDE_CP_CRTC_DSPP_RC_PU] =
		(enum sde_cp_crtc_pu_features) SDE_CP_CRTC_DSPP_RC_MASK,
};

/* explicitly set the features that needs to be treated during handoff */
static bool feature_handoff_mask[SDE_CP_CRTC_MAX_FEATURES] = {
	[SDE_CP_CRTC_DSPP_IGC] = 1,
	[SDE_CP_CRTC_DSPP_PCC] = 1,
	[SDE_CP_CRTC_DSPP_GC] = 1,
	[SDE_CP_CRTC_DSPP_HSIC] = 1,
	[SDE_CP_CRTC_DSPP_MEMCOL_SKIN] = 1,
	[SDE_CP_CRTC_DSPP_MEMCOL_SKY] = 1,
	[SDE_CP_CRTC_DSPP_MEMCOL_FOLIAGE] = 1,
	[SDE_CP_CRTC_DSPP_MEMCOL_PROT] = 1,
	[SDE_CP_CRTC_DSPP_SIXZONE] = 1,
	[SDE_CP_CRTC_DSPP_GAMUT] = 1,
	[SDE_CP_CRTC_DSPP_DITHER] = 1,
};

typedef void (*dspp_cap_update_func_t)(struct sde_crtc *crtc,
		struct sde_kms_info *info);
enum sde_dspp_caps_features {
	SDE_CP_DSPP_CAPS,
	SDE_CP_RC_CAPS,
	SDE_CP_DEMRUA_CAPS,
	SDE_CP_SPR_CAPS,
	SDE_CP_LTM_CAPS,
	SDE_CP_CAPS_MAX,
};

static void dspp_idx_caps_update(struct sde_crtc *crtc,
				struct sde_kms_info *info);
static void rc_caps_update(struct sde_crtc *crtc, struct sde_kms_info *info);
static void demura_caps_update(struct sde_crtc *crtc,
				struct sde_kms_info *info);
static void spr_caps_update(struct sde_crtc *crtc, struct sde_kms_info *info);
static void ltm_caps_update(struct sde_crtc *crtc, struct sde_kms_info *info);
static dspp_cap_update_func_t dspp_cap_update_func[SDE_CP_CAPS_MAX];

#define setup_dspp_caps_funcs(func) \
do { \
	func[SDE_CP_DSPP_CAPS] = dspp_idx_caps_update; \
	func[SDE_CP_RC_CAPS] = rc_caps_update; \
	func[SDE_CP_DEMRUA_CAPS] = demura_caps_update; \
	func[SDE_CP_SPR_CAPS] = spr_caps_update; \
	func[SDE_CP_LTM_CAPS] = ltm_caps_update; \
} while (0)

static void _sde_cp_crtc_enable_hist_irq(struct sde_crtc *sde_crtc);

typedef int (*feature_wrapper)(struct sde_hw_dspp *hw_dspp,
				   struct sde_hw_cp_cfg *hw_cfg,
				   struct sde_crtc *hw_crtc);


static struct sde_kms *get_kms(struct drm_crtc *crtc)
{
	struct msm_drm_private *priv = crtc->dev->dev_private;

	return to_sde_kms(priv->kms);
}

static void update_pu_feature_enable(struct sde_crtc *sde_crtc,
		u32 feature, bool enable)
{
	if (!sde_crtc || feature > SDE_CP_CRTC_MAX_PU_FEATURES) {
		DRM_ERROR("invalid args feature %d\n", feature);
		return;
	}

	if (enable)
		sde_crtc->cp_pu_feature_mask |= BIT(feature);
	else
		sde_crtc->cp_pu_feature_mask &= ~BIT(feature);
}

static int set_dspp_vlut_feature(struct sde_hw_dspp *hw_dspp,
				 struct sde_hw_cp_cfg *hw_cfg,
				 struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_vlut)
		ret = -EINVAL;
	else
		hw_dspp->ops.setup_vlut(hw_dspp, hw_cfg);
	return ret;
}

static int set_dspp_pcc_feature(struct sde_hw_dspp *hw_dspp,
				struct sde_hw_cp_cfg *hw_cfg,
				struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_pcc)
		ret = -EINVAL;
	else
		hw_dspp->ops.setup_pcc(hw_dspp, hw_cfg);
	return ret;
}

static int set_dspp_igc_feature(struct sde_hw_dspp *hw_dspp,
				struct sde_hw_cp_cfg *hw_cfg,
				struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_igc)
		ret = -EINVAL;
	else
		hw_dspp->ops.setup_igc(hw_dspp, hw_cfg);
	return ret;
}

static int set_dspp_gc_feature(struct sde_hw_dspp *hw_dspp,
			       struct sde_hw_cp_cfg *hw_cfg,
			       struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_gc)
		ret = -EINVAL;
	else
		hw_dspp->ops.setup_gc(hw_dspp, hw_cfg);
	return ret;
}

static int set_dspp_hsic_feature(struct sde_hw_dspp *hw_dspp,
				 struct sde_hw_cp_cfg *hw_cfg,
				 struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_pa_hsic)
		ret = -EINVAL;
	else
		hw_dspp->ops.setup_pa_hsic(hw_dspp, hw_cfg);

	return ret;
}


static int set_dspp_memcol_skin_feature(struct sde_hw_dspp *hw_dspp,
					struct sde_hw_cp_cfg *hw_cfg,
					struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_pa_memcol_skin)
		ret = -EINVAL;
	else
		hw_dspp->ops.setup_pa_memcol_skin(hw_dspp, hw_cfg);
	return ret;
}

static int set_dspp_memcol_sky_feature(struct sde_hw_dspp *hw_dspp,
				       struct sde_hw_cp_cfg *hw_cfg,
				       struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_pa_memcol_sky)
		ret = -EINVAL;
	else
		hw_dspp->ops.setup_pa_memcol_sky(hw_dspp, hw_cfg);
	return ret;
}

static int set_dspp_memcol_foliage_feature(struct sde_hw_dspp *hw_dspp,
					   struct sde_hw_cp_cfg *hw_cfg,
					   struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_pa_memcol_foliage)
		ret = -EINVAL;
	else
		hw_dspp->ops.setup_pa_memcol_foliage(hw_dspp, hw_cfg);
	return ret;
}


static int set_dspp_memcol_prot_feature(struct sde_hw_dspp *hw_dspp,
					struct sde_hw_cp_cfg *hw_cfg,
					struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_pa_memcol_prot)
		ret = -EINVAL;
	else
		hw_dspp->ops.setup_pa_memcol_prot(hw_dspp, hw_cfg);
	return ret;
}

static int set_dspp_sixzone_feature(struct sde_hw_dspp *hw_dspp,
				    struct sde_hw_cp_cfg *hw_cfg,
				    struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_sixzone)
		ret = -EINVAL;
	else
		hw_dspp->ops.setup_sixzone(hw_dspp, hw_cfg);
	return ret;
}

static int set_dspp_gamut_feature(struct sde_hw_dspp *hw_dspp,
				  struct sde_hw_cp_cfg *hw_cfg,
				  struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_gamut)
		ret = -EINVAL;
	else
		hw_dspp->ops.setup_gamut(hw_dspp, hw_cfg);
	return ret;
}

static int set_dspp_dither_feature(struct sde_hw_dspp *hw_dspp,
				   struct sde_hw_cp_cfg *hw_cfg,
				   struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_pa_dither)
		ret = -EINVAL;
	else
		hw_dspp->ops.setup_pa_dither(hw_dspp, hw_cfg);
	return ret;
}

static int set_dspp_hist_ctrl_feature(struct sde_hw_dspp *hw_dspp,
				      struct sde_hw_cp_cfg *hw_cfg,
				      struct sde_crtc *hw_crtc)
{
	int ret = 0;
	bool feature_enabled;

	if (!hw_dspp || !hw_dspp->ops.setup_histogram) {
		ret = -EINVAL;
	} else {
		feature_enabled = hw_cfg->payload &&
			*((u64 *)hw_cfg->payload) != 0;
		hw_dspp->ops.setup_histogram(hw_dspp, &feature_enabled);
	}
	return ret;
}

static int set_dspp_hist_irq_feature(struct sde_hw_dspp *hw_dspp,
				     struct sde_hw_cp_cfg *hw_cfg,
				     struct sde_crtc *hw_crtc)
{
	int ret = 0;
	struct sde_hw_mixer *hw_lm = hw_cfg->mixer_info;

	if (!hw_dspp)
		ret = -EINVAL;
	else if (!hw_lm->cfg.right_mixer)
		_sde_cp_crtc_enable_hist_irq(hw_crtc);
	return ret;
}


static int set_dspp_ad_mode_feature(struct sde_hw_dspp *hw_dspp,
				    struct sde_hw_cp_cfg *hw_cfg,
				    struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_ad) {
		ret = -EINVAL;
	} else {
		struct sde_ad_hw_cfg ad_cfg;

		ad_cfg.prop = AD_MODE;
		ad_cfg.hw_cfg = hw_cfg;
		hw_dspp->ops.setup_ad(hw_dspp, &ad_cfg);
	}
	return ret;
}

static int set_dspp_ad_init_feature(struct sde_hw_dspp *hw_dspp,
				    struct sde_hw_cp_cfg *hw_cfg,
				    struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_ad) {
		ret = -EINVAL;
	} else {
		struct sde_ad_hw_cfg ad_cfg;

		ad_cfg.prop = AD_INIT;
		ad_cfg.hw_cfg = hw_cfg;
		hw_dspp->ops.setup_ad(hw_dspp, &ad_cfg);
	}
	return ret;
}

static int set_dspp_ad_cfg_feature(struct sde_hw_dspp *hw_dspp,
				   struct sde_hw_cp_cfg *hw_cfg,
				   struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_ad) {
		ret = -EINVAL;
	} else {
		struct sde_ad_hw_cfg ad_cfg;

		ad_cfg.prop = AD_CFG;
		ad_cfg.hw_cfg = hw_cfg;
		hw_dspp->ops.setup_ad(hw_dspp, &ad_cfg);
	}
	return ret;
}

static int set_dspp_ad_input_feature(struct sde_hw_dspp *hw_dspp,
				     struct sde_hw_cp_cfg *hw_cfg,
				     struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_ad) {
		ret = -EINVAL;
	} else {
		struct sde_ad_hw_cfg ad_cfg;

		ad_cfg.prop = AD_INPUT;
		ad_cfg.hw_cfg = hw_cfg;
		hw_dspp->ops.setup_ad(hw_dspp, &ad_cfg);
	}
	return ret;
}

static int set_dspp_ad_assertive_feature(struct sde_hw_dspp *hw_dspp,
					 struct sde_hw_cp_cfg *hw_cfg,
					 struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_ad) {
		ret = -EINVAL;
	} else {
		struct sde_ad_hw_cfg ad_cfg;

		ad_cfg.prop = AD_ASSERTIVE;
		ad_cfg.hw_cfg = hw_cfg;
		hw_dspp->ops.setup_ad(hw_dspp, &ad_cfg);
	}
	return ret;
}

static int set_dspp_ad_backlight_feature(struct sde_hw_dspp *hw_dspp,
					 struct sde_hw_cp_cfg *hw_cfg,
					 struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_ad) {
		ret = -EINVAL;
	} else {
		struct sde_ad_hw_cfg ad_cfg;

		ad_cfg.prop = AD_BACKLIGHT;
		ad_cfg.hw_cfg = hw_cfg;
		hw_dspp->ops.setup_ad(hw_dspp, &ad_cfg);
	}
	return ret;
}

static int set_dspp_ad_strength_feature(struct sde_hw_dspp *hw_dspp,
					struct sde_hw_cp_cfg *hw_cfg,
					struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_ad) {
		ret = -EINVAL;
	} else {
		struct sde_ad_hw_cfg ad_cfg;

		ad_cfg.prop = AD_STRENGTH;
		ad_cfg.hw_cfg = hw_cfg;
		hw_dspp->ops.setup_ad(hw_dspp, &ad_cfg);
	}
	return ret;
}

static int set_dspp_ad_roi_feature(struct sde_hw_dspp *hw_dspp,
				   struct sde_hw_cp_cfg *hw_cfg,
				   struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_ad) {
		ret = -EINVAL;
	} else {
		struct sde_ad_hw_cfg ad_cfg;

		ad_cfg.prop = AD_ROI;
		ad_cfg.hw_cfg = hw_cfg;
		hw_dspp->ops.setup_ad(hw_dspp, &ad_cfg);
	}
	return ret;
}

static int set_lm_gc_feature(struct sde_hw_dspp *hw_dspp,
			     struct sde_hw_cp_cfg *hw_cfg,
			     struct sde_crtc *hw_crtc)
{
	int ret = 0;
	struct sde_hw_mixer *hw_lm = (struct sde_hw_mixer *)hw_cfg->mixer_info;

	if (!hw_lm->ops.setup_gc)
		ret = -EINVAL;
	else
		hw_lm->ops.setup_gc(hw_lm, hw_cfg);
	return ret;
}

static int set_ltm_init_feature(struct sde_hw_dspp *hw_dspp,
				   struct sde_hw_cp_cfg *hw_cfg,
				   struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_ltm_init)
		ret = -EINVAL;
	else
		hw_dspp->ops.setup_ltm_init(hw_dspp, hw_cfg);

	return ret;
}

static int set_ltm_roi_feature(struct sde_hw_dspp *hw_dspp,
				   struct sde_hw_cp_cfg *hw_cfg,
				   struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_ltm_roi) {
		ret = -EINVAL;
	} else {
		hw_dspp->ops.setup_ltm_roi(hw_dspp, hw_cfg);
		_sde_cp_crtc_update_ltm_roi(hw_crtc, hw_cfg);
	}

	return ret;
}

static int set_ltm_vlut_feature(struct sde_hw_dspp *hw_dspp,
				   struct sde_hw_cp_cfg *hw_cfg,
				   struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_ltm_vlut)
		ret = -EINVAL;
	else
		hw_dspp->ops.setup_ltm_vlut(hw_dspp, hw_cfg);

	return ret;
}

static int set_ltm_thresh_feature(struct sde_hw_dspp *hw_dspp,
				   struct sde_hw_cp_cfg *hw_cfg,
				   struct sde_crtc *sde_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_ltm_thresh)
		ret = -EINVAL;
	else
		hw_dspp->ops.setup_ltm_thresh(hw_dspp, hw_cfg);

	return ret;
}

static int set_ltm_buffers_feature(struct sde_hw_dspp *hw_dspp,
				   struct sde_hw_cp_cfg *hw_cfg,
				   struct sde_crtc *sde_crtc)
{
	int ret = 0;
	struct sde_hw_mixer *hw_lm;
	struct drm_msm_ltm_buffers_ctrl *payload;

	if (!sde_crtc || !hw_dspp) {
		ret = -EINVAL;
	} else {
		hw_lm = hw_cfg->mixer_info;
		/* in merge mode, both LTM cores use the same buffer */
		if (!hw_lm->cfg.right_mixer) {
			payload = hw_cfg->payload;
			mutex_lock(&sde_crtc->ltm_buffer_lock);
			if (payload)
				_sde_cp_crtc_set_ltm_buffer(sde_crtc, hw_cfg);
			else
				_sde_cp_crtc_free_ltm_buffer(sde_crtc, hw_cfg);
			mutex_unlock(&sde_crtc->ltm_buffer_lock);
		}
	}

	return ret;
}

static int set_ltm_queue_buf_feature(struct sde_hw_dspp *hw_dspp,
				   struct sde_hw_cp_cfg *hw_cfg,
				   struct sde_crtc *sde_crtc)
{
	int ret = 0;
	struct sde_hw_mixer *hw_lm;

	if (!sde_crtc || !hw_dspp) {
		ret = -EINVAL;
	} else {
		hw_lm = hw_cfg->mixer_info;
		/* in merge mode, both LTM cores use the same buffer */
		if (!hw_lm->cfg.right_mixer) {
			mutex_lock(&sde_crtc->ltm_buffer_lock);
			_sde_cp_crtc_queue_ltm_buffer(sde_crtc, hw_cfg);
			mutex_unlock(&sde_crtc->ltm_buffer_lock);
		}
	}

	return ret;
}

static int set_ltm_hist_crtl_feature(struct sde_hw_dspp *hw_dspp,
				   struct sde_hw_cp_cfg *hw_cfg,
				   struct sde_crtc *sde_crtc)
{
	int ret = 0;
	bool feature_enabled = false;

	if (!sde_crtc || !hw_dspp || !hw_dspp->ops.setup_ltm_hist_ctrl) {
		ret = -EINVAL;
	} else {
		mutex_lock(&sde_crtc->ltm_buffer_lock);
		feature_enabled = hw_cfg->payload &&
			(*((u64 *)hw_cfg->payload) != 0);
		if (feature_enabled)
			_sde_cp_crtc_enable_ltm_hist(sde_crtc, hw_dspp, hw_cfg);
		else
			_sde_cp_crtc_disable_ltm_hist(sde_crtc, hw_dspp,
					hw_cfg);
		mutex_unlock(&sde_crtc->ltm_buffer_lock);
	}

	return ret;
}

static int check_rc_mask_feature(struct sde_hw_dspp *hw_dspp,
				 struct sde_hw_cp_cfg *hw_cfg,
				 struct sde_crtc *sde_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_cfg || !sde_crtc) {
		DRM_ERROR("invalid arguments");
		return -EINVAL;
	}

	if (!hw_dspp->ops.validate_rc_mask) {
		DRM_ERROR("invalid rc ops");
		return -EINVAL;
	}

	ret = hw_dspp->ops.validate_rc_mask(hw_dspp, hw_cfg);
	if (ret)
		DRM_ERROR("failed to validate rc mask %d", ret);

	return ret;
}

static int set_rc_mask_feature(struct sde_hw_dspp *hw_dspp,
			       struct sde_hw_cp_cfg *hw_cfg,
			       struct sde_crtc *sde_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_cfg || !sde_crtc) {
		DRM_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	if (!hw_dspp->ops.setup_rc_mask || !hw_dspp->ops.setup_rc_data) {
		DRM_ERROR("invalid rc ops\n");
		return -EINVAL;
	}

	DRM_DEBUG_DRIVER("dspp %d setup mask for rc instance %u\n",
			hw_dspp->idx, hw_dspp->cap->sblk->rc.idx);

	ret = hw_dspp->ops.setup_rc_mask(hw_dspp, hw_cfg);
	if (ret) {
		DRM_ERROR("failed to setup rc mask, ret %d\n", ret);
		goto exit;
	}

	/* rc data should be programmed once if dspp are in multi-pipe mode */
	if (hw_dspp->cap->sblk->rc.idx % hw_cfg->num_of_mixers == 0) {
		ret = hw_dspp->ops.setup_rc_data(hw_dspp, hw_cfg);
		if (ret) {
			DRM_ERROR("failed to setup rc data, ret %d\n", ret);
			goto exit;
		}
	}

	update_pu_feature_enable(sde_crtc, SDE_CP_CRTC_DSPP_RC_PU,
			hw_cfg->payload != NULL);
exit:
	return ret;
}

static int set_rc_pu_feature(struct sde_hw_dspp *hw_dspp,
			     struct sde_hw_cp_cfg *hw_cfg,
			     struct sde_crtc *sde_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_cfg || !sde_crtc) {
		DRM_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	if (!hw_dspp->ops.setup_rc_pu_roi) {
		DRM_ERROR("invalid rc ops\n");
		return -EINVAL;
	}

	DRM_DEBUG_DRIVER("dspp %d setup pu roi for rc instance %u\n",
			hw_dspp->idx, hw_dspp->cap->sblk->rc.idx);

	ret = hw_dspp->ops.setup_rc_pu_roi(hw_dspp, hw_cfg);
	if (ret < 0)
		DRM_ERROR("failed to setup rc pu roi, ret %d\n", ret);

	return ret;
}

static int check_rc_pu_feature(struct sde_hw_dspp *hw_dspp,
			       struct sde_hw_cp_cfg *hw_cfg,
			       struct sde_crtc *sde_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_cfg || !sde_crtc) {
		DRM_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	if (!hw_dspp->ops.validate_rc_pu_roi) {
		SDE_ERROR("invalid rc ops");
		return -EINVAL;
	}

	ret = hw_dspp->ops.validate_rc_pu_roi(hw_dspp, hw_cfg);
	if (ret)
		SDE_ERROR("failed to validate rc pu roi, ret %d", ret);

	return ret;
}

static int set_spr_pu_feature(struct sde_hw_dspp *hw_dspp,
	struct sde_hw_cp_cfg *hw_cfg, struct sde_crtc *sde_crtc)
{
	if (!hw_dspp || !hw_cfg || !sde_crtc) {
		DRM_ERROR("invalid argumets\n");
		return -EINVAL;
	}

	if (hw_dspp->ops.setup_spr_pu_config)
		hw_dspp->ops.setup_spr_pu_config(hw_dspp, hw_cfg);

	return 0;
}

static int check_spr_pu_feature(struct sde_hw_dspp *hw_dspp,
	struct sde_hw_cp_cfg *hw_cfg, struct sde_crtc *sde_crtc)
{
	struct msm_roi_list *roi_list;

	if (!hw_cfg || hw_cfg->len != sizeof(struct sde_drm_roi_v1)) {
		SDE_ERROR("invalid payload\n");
		return -EINVAL;
	}

	roi_list = hw_cfg->payload;
	if (roi_list->num_rects > 1) {
		SDE_ERROR("multiple pu regions not supported with spr\n");
		return -EINVAL;
	}

	if ((roi_list->roi[0].x2 - roi_list->roi[0].x1) != hw_cfg->displayh) {
		SDE_ERROR("pu region not full width %d\n",
				(roi_list->roi[0].x2 - roi_list->roi[0].x1));
		return -EINVAL;
	}

	return 0;
}

static int set_spr_init_feature(struct sde_hw_dspp *hw_dspp,
				struct sde_hw_cp_cfg *hw_cfg,
				struct sde_crtc *sde_crtc)
{
	int ret = 0;

	if (!sde_crtc || !hw_dspp || !hw_dspp->ops.setup_spr_init_config) {
		DRM_ERROR("invalid arguments\n");
		ret = -EINVAL;
	} else {
		hw_dspp->ops.setup_spr_init_config(hw_dspp, hw_cfg);
		update_pu_feature_enable(sde_crtc, SDE_CP_CRTC_DSPP_SPR_PU,
				hw_cfg->payload != NULL);
	}

	return ret;
}

static int set_demura_feature(struct sde_hw_dspp *hw_dspp,
				   struct sde_hw_cp_cfg *hw_cfg,
				   struct sde_crtc *hw_crtc)
{
	int ret = 0;

	if (!hw_dspp || !hw_dspp->ops.setup_demura_cfg)
		ret = -EINVAL;
	else
		hw_dspp->ops.setup_demura_cfg(hw_dspp, hw_cfg);

	return ret;
}

feature_wrapper check_crtc_feature_wrappers[SDE_CP_CRTC_MAX_FEATURES];
#define setup_check_crtc_feature_wrappers(wrappers) \
do { \
	memset(wrappers, 0, sizeof(wrappers)); \
	wrappers[SDE_CP_CRTC_DSPP_RC_MASK] = check_rc_mask_feature; \
} while (0)

feature_wrapper set_crtc_feature_wrappers[SDE_CP_CRTC_MAX_FEATURES];
#define setup_set_crtc_feature_wrappers(wrappers) \
do { \
	memset(wrappers, 0, sizeof(wrappers)); \
	wrappers[SDE_CP_CRTC_DSPP_VLUT] = set_dspp_vlut_feature; \
	wrappers[SDE_CP_CRTC_DSPP_PCC] = set_dspp_pcc_feature; \
	wrappers[SDE_CP_CRTC_DSPP_IGC] = set_dspp_igc_feature; \
	wrappers[SDE_CP_CRTC_DSPP_GC] = set_dspp_gc_feature; \
	wrappers[SDE_CP_CRTC_DSPP_HSIC] =\
		set_dspp_hsic_feature; \
	wrappers[SDE_CP_CRTC_DSPP_MEMCOL_SKIN] = set_dspp_memcol_skin_feature; \
	wrappers[SDE_CP_CRTC_DSPP_MEMCOL_SKY] =\
		set_dspp_memcol_sky_feature; \
	wrappers[SDE_CP_CRTC_DSPP_MEMCOL_FOLIAGE] =\
		set_dspp_memcol_foliage_feature; \
	wrappers[SDE_CP_CRTC_DSPP_MEMCOL_PROT] = set_dspp_memcol_prot_feature; \
	wrappers[SDE_CP_CRTC_DSPP_SIXZONE] = set_dspp_sixzone_feature; \
	wrappers[SDE_CP_CRTC_DSPP_GAMUT] = set_dspp_gamut_feature; \
	wrappers[SDE_CP_CRTC_DSPP_DITHER] = set_dspp_dither_feature; \
	wrappers[SDE_CP_CRTC_DSPP_HIST_CTRL] = set_dspp_hist_ctrl_feature; \
	wrappers[SDE_CP_CRTC_DSPP_HIST_IRQ] = set_dspp_hist_irq_feature; \
	wrappers[SDE_CP_CRTC_DSPP_AD_MODE] = set_dspp_ad_mode_feature; \
	wrappers[SDE_CP_CRTC_DSPP_AD_INIT] = set_dspp_ad_init_feature; \
	wrappers[SDE_CP_CRTC_DSPP_AD_CFG] = set_dspp_ad_cfg_feature; \
	wrappers[SDE_CP_CRTC_DSPP_AD_INPUT] = set_dspp_ad_input_feature; \
	wrappers[SDE_CP_CRTC_DSPP_AD_ASSERTIVENESS] =\
		set_dspp_ad_assertive_feature; \
	wrappers[SDE_CP_CRTC_DSPP_AD_BACKLIGHT] =\
		set_dspp_ad_backlight_feature; \
	wrappers[SDE_CP_CRTC_DSPP_AD_STRENGTH] = set_dspp_ad_strength_feature; \
	wrappers[SDE_CP_CRTC_DSPP_AD_ROI] = set_dspp_ad_roi_feature; \
	wrappers[SDE_CP_CRTC_LM_GC] = set_lm_gc_feature; \
	wrappers[SDE_CP_CRTC_DSPP_LTM_INIT] = set_ltm_init_feature; \
	wrappers[SDE_CP_CRTC_DSPP_LTM_ROI] = set_ltm_roi_feature; \
	wrappers[SDE_CP_CRTC_DSPP_LTM_VLUT] = set_ltm_vlut_feature; \
	wrappers[SDE_CP_CRTC_DSPP_LTM_HIST_THRESH] = set_ltm_thresh_feature; \
	wrappers[SDE_CP_CRTC_DSPP_LTM_SET_BUF] = set_ltm_buffers_feature; \
	wrappers[SDE_CP_CRTC_DSPP_LTM_QUEUE_BUF] = set_ltm_queue_buf_feature; \
	wrappers[SDE_CP_CRTC_DSPP_LTM_QUEUE_BUF2] = set_ltm_queue_buf_feature; \
	wrappers[SDE_CP_CRTC_DSPP_LTM_QUEUE_BUF3] = set_ltm_queue_buf_feature; \
	wrappers[SDE_CP_CRTC_DSPP_LTM_HIST_CTL] = set_ltm_hist_crtl_feature; \
	wrappers[SDE_CP_CRTC_DSPP_RC_MASK] = set_rc_mask_feature; \
	wrappers[SDE_CP_CRTC_DSPP_SPR_INIT] = set_spr_init_feature; \
	wrappers[SDE_CP_CRTC_DSPP_DEMURA_INIT] = set_demura_feature; \
} while (0)

feature_wrapper set_crtc_pu_feature_wrappers[SDE_CP_CRTC_MAX_PU_FEATURES];
#define setup_set_crtc_pu_feature_wrappers(wrappers) \
do { \
	memset(wrappers, 0, sizeof(wrappers)); \
	wrappers[SDE_CP_CRTC_DSPP_RC_PU] = set_rc_pu_feature; \
	wrappers[SDE_CP_CRTC_DSPP_SPR_PU] = set_spr_pu_feature; \
} while (0)

feature_wrapper check_crtc_pu_feature_wrappers[SDE_CP_CRTC_MAX_PU_FEATURES];
#define setup_check_crtc_pu_feature_wrappers(wrappers) \
do { \
	memset(wrappers, 0, sizeof(wrappers)); \
	wrappers[SDE_CP_CRTC_DSPP_RC_PU] = check_rc_pu_feature; \
	wrappers[SDE_CP_CRTC_DSPP_SPR_PU] = check_spr_pu_feature; \
} while (0)

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
		return 0;
	drm_property_blob_put(blob);
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
		drm_property_blob_put(prop_node->blob_ptr);
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

	ret = copy_from_user(blob_ptr->data, u64_to_user_ptr(val),
			blob_ptr->length);
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
		drm_property_blob_put(blob);
		return -EINVAL;
	}
	/* Release refernce to existing payload of the property */
	if (prop_node->blob_ptr)
		drm_property_blob_put(prop_node->blob_ptr);

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

	/* create blob to store histogram data */
	sde_crtc->hist_blob = drm_property_create_blob(crtc->dev,
				sizeof(struct drm_msm_hist), NULL);
	if (IS_ERR(sde_crtc->hist_blob))
		sde_crtc->hist_blob = NULL;

	msm_property_install_blob(&sde_crtc->property_info,
		"dspp_caps", DRM_MODE_PROP_IMMUTABLE, CRTC_PROP_DSPP_INFO);

	mutex_init(&sde_crtc->crtc_cp_lock);
	INIT_LIST_HEAD(&sde_crtc->active_list);
	INIT_LIST_HEAD(&sde_crtc->dirty_list);
	INIT_LIST_HEAD(&sde_crtc->feature_list);
	INIT_LIST_HEAD(&sde_crtc->ad_dirty);
	INIT_LIST_HEAD(&sde_crtc->ad_active);
	mutex_init(&sde_crtc->ltm_buffer_lock);
	spin_lock_init(&sde_crtc->ltm_lock);
	INIT_LIST_HEAD(&sde_crtc->ltm_buf_free);
	INIT_LIST_HEAD(&sde_crtc->ltm_buf_busy);
	sde_cp_crtc_disable(crtc);
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

static struct sde_crtc_irq_info *_sde_cp_get_intr_node(u32 event,
				struct sde_crtc *sde_crtc)
{
	bool found = false;
	struct sde_crtc_irq_info *node = NULL;

	list_for_each_entry(node, &sde_crtc->user_event_list, list) {
		if (node->event == event) {
			found = true;
			break;
		}
	}

	if (!found)
		node = NULL;

	return node;
}

static void _sde_cp_crtc_enable_hist_irq(struct sde_crtc *sde_crtc)
{
	struct drm_crtc *crtc_drm = &sde_crtc->base;
	struct sde_kms *kms = NULL;
	struct sde_hw_mixer *hw_lm;
	struct sde_hw_dspp *hw_dspp = NULL;
	struct sde_crtc_irq_info *node = NULL;
	int i, irq_idx, ret = 0;
	unsigned long flags, state_flags;

	if (!crtc_drm) {
		DRM_ERROR("invalid crtc %pK\n", crtc_drm);
		return;
	}

	kms = get_kms(crtc_drm);

	for (i = 0; i < sde_crtc->num_mixers; i++) {
		hw_lm = sde_crtc->mixers[i].hw_lm;
		hw_dspp = sde_crtc->mixers[i].hw_dspp;
		if (!hw_lm->cfg.right_mixer)
			break;
	}

	if (!hw_dspp) {
		DRM_ERROR("invalid dspp\n");
		return;
	}

	irq_idx = sde_core_irq_idx_lookup(kms, SDE_IRQ_TYPE_HIST_DSPP_DONE,
					hw_dspp->idx);
	if (irq_idx < 0) {
		DRM_ERROR("failed to get irq idx\n");
		return;
	}

	spin_lock_irqsave(&sde_crtc->spin_lock, flags);
	node = _sde_cp_get_intr_node(DRM_EVENT_HISTOGRAM, sde_crtc);

	if (!node) {
		spin_unlock_irqrestore(&sde_crtc->spin_lock, flags);
		return;
	}

	spin_lock_irqsave(&node->state_lock, state_flags);
	if (node->state == IRQ_DISABLED) {
		ret = sde_core_irq_enable(kms, &irq_idx, 1);
		if (ret)
			DRM_ERROR("failed to enable irq %d\n", irq_idx);
		else
			node->state = IRQ_ENABLED;
	}
	spin_unlock_irqrestore(&node->state_lock, state_flags);
	spin_unlock_irqrestore(&sde_crtc->spin_lock, flags);
}

static int sde_cp_crtc_checkfeature(struct sde_cp_node *prop_node,
	struct sde_crtc *sde_crtc, struct sde_crtc_state *sde_crtc_state)
{
	struct sde_hw_cp_cfg hw_cfg;
	struct sde_hw_mixer *hw_lm;
	struct sde_hw_dspp *hw_dspp;
	u32 num_mixers = sde_crtc->num_mixers;
	int i = 0, ret = 0;
	bool feature_enabled = false;
	feature_wrapper check_feature = NULL;

	if (!prop_node) {
		DRM_ERROR("invalid arguments");
		return -EINVAL;
	}

	if (prop_node->feature >= SDE_CP_CRTC_MAX_FEATURES) {
		DRM_ERROR("invalid feature %d\n", prop_node->feature);
		return -EINVAL;
	}

	check_feature = check_crtc_feature_wrappers[prop_node->feature];
	if (check_feature == NULL)
		return 0;

	memset(&hw_cfg, 0, sizeof(hw_cfg));
	sde_cp_get_hw_payload(prop_node, &hw_cfg, &feature_enabled);
	hw_cfg.num_of_mixers = sde_crtc->num_mixers;
	hw_cfg.last_feature = 0;

	for (i = 0; i < num_mixers; i++) {
		hw_dspp = sde_crtc->mixers[i].hw_dspp;
		if (!hw_dspp || i >= DSPP_MAX)
			continue;
		hw_cfg.dspp[i] = hw_dspp;
	}

	for (i = 0; i < num_mixers && !ret; i++) {
		hw_lm = sde_crtc->mixers[i].hw_lm;
		hw_dspp = sde_crtc->mixers[i].hw_dspp;
		if (!hw_lm) {
			ret = -EINVAL;
			continue;
		}
		hw_cfg.ctl = sde_crtc->mixers[i].hw_ctl;
		hw_cfg.mixer_info = hw_lm;

		/* use incoming state information */
		hw_cfg.displayh = num_mixers *
				sde_crtc_state->lm_roi[i].w;
		hw_cfg.displayv = sde_crtc_state->lm_roi[i].h;

		DRM_DEBUG_DRIVER("check cp feature %d on mixer %d\n",
				prop_node->feature, hw_lm->idx - LM_0);
		ret = check_feature(hw_dspp, &hw_cfg, sde_crtc);
		if (ret)
			break;
	}

	return ret;
}

static void sde_cp_crtc_setfeature(struct sde_cp_node *prop_node,
				   struct sde_crtc *sde_crtc)
{
	struct sde_hw_cp_cfg hw_cfg;
	struct sde_hw_mixer *hw_lm;
	struct sde_hw_dspp *hw_dspp;
	u32 num_mixers = sde_crtc->num_mixers;
	int i = 0, ret = 0;
	bool feature_enabled = false;
	struct sde_mdss_cfg *catalog = NULL;

	memset(&hw_cfg, 0, sizeof(hw_cfg));
	sde_cp_get_hw_payload(prop_node, &hw_cfg, &feature_enabled);
	hw_cfg.num_of_mixers = sde_crtc->num_mixers;
	hw_cfg.last_feature = 0;

	for (i = 0; i < num_mixers; i++) {
		hw_dspp = sde_crtc->mixers[i].hw_dspp;
		if (!hw_dspp || i >= DSPP_MAX)
			continue;
		hw_cfg.dspp[i] = hw_dspp;
	}

	if ((prop_node->feature >= SDE_CP_CRTC_MAX_FEATURES) ||
			set_crtc_feature_wrappers[prop_node->feature] == NULL) {
		ret = -EINVAL;
	} else {
		feature_wrapper set_feature =
			set_crtc_feature_wrappers[prop_node->feature];
		catalog = get_kms(&sde_crtc->base)->catalog;
		hw_cfg.broadcast_disabled = catalog->dma_cfg.broadcast_disabled;

		for (i = 0; i < num_mixers && !ret; i++) {
			hw_lm = sde_crtc->mixers[i].hw_lm;
			hw_dspp = sde_crtc->mixers[i].hw_dspp;
			if (!hw_lm) {
				ret = -EINVAL;
				continue;
			}
			hw_cfg.ctl = sde_crtc->mixers[i].hw_ctl;
			hw_cfg.mixer_info = hw_lm;
			hw_cfg.displayh = num_mixers * hw_lm->cfg.out_width;
			hw_cfg.displayv = hw_lm->cfg.out_height;

			ret = set_feature(hw_dspp, &hw_cfg, sde_crtc);
			if (ret)
				break;
		}

		if (ret) {
			DRM_ERROR("failed to %s feature %d\n",
				((feature_enabled) ? "enable" : "disable"),
				prop_node->feature);
			return;
		}
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

static const int dspp_feature_to_sub_blk_tbl[SDE_CP_CRTC_MAX_FEATURES] = {
	[SDE_CP_CRTC_DSPP_IGC] = SDE_DSPP_IGC,
	[SDE_CP_CRTC_DSPP_PCC] = SDE_DSPP_PCC,
	[SDE_CP_CRTC_DSPP_GC] = SDE_DSPP_GC,
	[SDE_CP_CRTC_DSPP_HSIC] = SDE_DSPP_HSIC,
	[SDE_CP_CRTC_DSPP_MEMCOL_SKIN] = SDE_DSPP_MEMCOLOR,
	[SDE_CP_CRTC_DSPP_MEMCOL_SKY] = SDE_DSPP_MEMCOLOR,
	[SDE_CP_CRTC_DSPP_MEMCOL_FOLIAGE] = SDE_DSPP_MEMCOLOR,
	[SDE_CP_CRTC_DSPP_MEMCOL_PROT] = SDE_DSPP_MEMCOLOR,
	[SDE_CP_CRTC_DSPP_SIXZONE] = SDE_DSPP_SIXZONE,
	[SDE_CP_CRTC_DSPP_GAMUT] = SDE_DSPP_GAMUT,
	[SDE_CP_CRTC_DSPP_DITHER] = SDE_DSPP_DITHER,
	[SDE_CP_CRTC_DSPP_HIST_CTRL] = SDE_DSPP_HIST,
	[SDE_CP_CRTC_DSPP_HIST_IRQ] = SDE_DSPP_HIST,
	[SDE_CP_CRTC_DSPP_AD] = SDE_DSPP_AD,
	[SDE_CP_CRTC_DSPP_VLUT] = SDE_DSPP_VLUT,
	[SDE_CP_CRTC_DSPP_AD_MODE] = SDE_DSPP_AD,
	[SDE_CP_CRTC_DSPP_AD_INIT] = SDE_DSPP_AD,
	[SDE_CP_CRTC_DSPP_AD_CFG] = SDE_DSPP_AD,
	[SDE_CP_CRTC_DSPP_AD_INPUT] = SDE_DSPP_AD,
	[SDE_CP_CRTC_DSPP_AD_ASSERTIVENESS] = SDE_DSPP_AD,
	[SDE_CP_CRTC_DSPP_AD_BACKLIGHT] = SDE_DSPP_AD,
	[SDE_CP_CRTC_DSPP_AD_STRENGTH] = SDE_DSPP_AD,
	[SDE_CP_CRTC_DSPP_AD_ROI] = SDE_DSPP_AD,
	[SDE_CP_CRTC_DSPP_LTM] = SDE_DSPP_LTM,
	[SDE_CP_CRTC_DSPP_LTM_INIT] = SDE_DSPP_LTM,
	[SDE_CP_CRTC_DSPP_LTM_ROI] = SDE_DSPP_LTM,
	[SDE_CP_CRTC_DSPP_LTM_HIST_CTL] = SDE_DSPP_LTM,
	[SDE_CP_CRTC_DSPP_LTM_HIST_THRESH] = SDE_DSPP_LTM,
	[SDE_CP_CRTC_DSPP_LTM_SET_BUF] = SDE_DSPP_LTM,
	[SDE_CP_CRTC_DSPP_LTM_QUEUE_BUF] = SDE_DSPP_LTM,
	[SDE_CP_CRTC_DSPP_LTM_QUEUE_BUF2] = SDE_DSPP_LTM,
	[SDE_CP_CRTC_DSPP_LTM_QUEUE_BUF3] = SDE_DSPP_LTM,
	[SDE_CP_CRTC_DSPP_LTM_VLUT] = SDE_DSPP_LTM,
	[SDE_CP_CRTC_DSPP_SB] = SDE_DSPP_SB,
	[SDE_CP_CRTC_DSPP_SPR_INIT] = SDE_DSPP_SPR,
	[SDE_CP_CRTC_DSPP_RC_MASK] = SDE_DSPP_RC,
	[SDE_CP_CRTC_DSPP_DEMURA_INIT] = SDE_DSPP_DEMURA,
	[SDE_CP_CRTC_DSPP_DEMURA_BACKLIGHT] = SDE_DSPP_DEMURA,
	[SDE_CP_CRTC_DSPP_MAX] = SDE_DSPP_MAX,
	[SDE_CP_CRTC_LM_GC] = SDE_DSPP_MAX,
};

static void _flush_sb_dma_hw(int *active_ctls, struct sde_hw_ctl *ctl,
		u32 list_size)
{
	//For each CTL send last CD to SB DMA only once.
	u32 j;
	struct sde_hw_reg_dma_ops *dma_ops = sde_reg_dma_get_ops();

	for (j = 0; j < list_size; j++) {
		if (active_ctls[j] == ctl->idx) {
			break;
		} else if (active_ctls[j] == 0) {
			active_ctls[j] = ctl->idx;
			dma_ops->last_command_sb(ctl, DMA_CTL_QUEUE1,
					REG_DMA_NOWAIT);
			break;
		}
	}
}

void sde_cp_dspp_flush_helper(struct sde_crtc *sde_crtc, u32 feature)
{
	u32 i, sub_blk, num_mixers;
	struct sde_hw_dspp *dspp;
	struct sde_hw_ctl *ctl;
	int active_ctls[CTL_MAX - CTL_0];

	if (!sde_crtc || feature >= SDE_CP_CRTC_MAX_FEATURES) {
		SDE_ERROR("invalid args: sde_crtc %s for feature %d",
				sde_crtc ? "valid" : "invalid", feature);
		return;
	}

	num_mixers = sde_crtc->num_mixers;
	sub_blk = dspp_feature_to_sub_blk_tbl[feature];
	memset(&active_ctls, 0, sizeof(active_ctls));

	for (i = 0; i < num_mixers; i++) {
		ctl = sde_crtc->mixers[i].hw_ctl;
		dspp = sde_crtc->mixers[i].hw_dspp;
		if (ctl && ctl->ops.update_bitmask_dspp_subblk) {
			if (feature == SDE_CP_CRTC_DSPP_SB) {
				if (!dspp->sb_dma_in_use)
					continue;
				dspp->sb_dma_in_use = false;

				_flush_sb_dma_hw(active_ctls, ctl,
						ARRAY_SIZE(active_ctls));
				ctl->ops.update_bitmask_dspp_subblk(ctl,
						dspp->idx, sub_blk, true);
			} else {
				ctl->ops.update_bitmask_dspp_subblk(ctl,
						dspp->idx, sub_blk, true);
			}
		}
	}
}

static int sde_cp_crtc_check_pu_features(struct drm_crtc *crtc)
{
	int ret = 0, i = 0, j = 0;
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *sde_crtc_state;
	struct sde_hw_cp_cfg hw_cfg;
	struct sde_hw_dspp *hw_dspp;

	if (!crtc) {
		DRM_ERROR("invalid crtc %pK\n", crtc);
		return -EINVAL;
	}

	sde_crtc = to_sde_crtc(crtc);
	if (!sde_crtc) {
		DRM_ERROR("invalid sde_crtc %pK\n", sde_crtc);
		return -EINVAL;
	}

	sde_crtc_state = to_sde_crtc_state(crtc->state);
	if (!sde_crtc_state) {
		DRM_ERROR("invalid sde_crtc_state %pK\n", sde_crtc_state);
		return -EINVAL;
	}

	for (i = 0; i < sde_crtc->num_mixers; i++) {
		if (!sde_crtc->mixers[i].hw_lm) {
			DRM_ERROR("invalid lm in mixer %d\n", i);
			return -EINVAL;
		}

		if (!sde_crtc->mixers[i].hw_ctl) {
			DRM_ERROR("invalid ctl in mixer %d\n", i);
			return -EINVAL;
		}
	}

	/* early return when not a partial update frame */
	if (sde_crtc_state->user_roi_list.num_rects == 0) {
		DRM_DEBUG_DRIVER("no partial update required\n");
		return 0;
	}

	memset(&hw_cfg, 0, sizeof(hw_cfg));
	hw_cfg.num_of_mixers = sde_crtc->num_mixers;
	hw_cfg.payload = &sde_crtc_state->user_roi_list;
	hw_cfg.len = sizeof(sde_crtc_state->user_roi_list);
	for (i = 0; i < hw_cfg.num_of_mixers; i++)
		hw_cfg.dspp[i] = sde_crtc->mixers[i].hw_dspp;

	for (i = 0; i < SDE_CP_CRTC_MAX_PU_FEATURES; i++) {
		feature_wrapper check_pu_feature =
				check_crtc_pu_feature_wrappers[i];

		if (!check_pu_feature ||
				!(sde_crtc->cp_pu_feature_mask & BIT(i)))
			continue;

		for (j = 0; j < hw_cfg.num_of_mixers; j++) {
			hw_dspp = sde_crtc->mixers[j].hw_dspp;

			/* use incoming state information */
			hw_cfg.mixer_info = sde_crtc->mixers[j].hw_lm;
			hw_cfg.ctl = sde_crtc->mixers[j].hw_ctl;
			hw_cfg.displayh = hw_cfg.num_of_mixers *
					sde_crtc_state->lm_roi[j].w;
			hw_cfg.displayv = sde_crtc_state->lm_roi[j].h;

			ret = check_pu_feature(hw_dspp, &hw_cfg, sde_crtc);
			if (ret) {
				DRM_ERROR("failed pu feature %d in mixer %d\n",
						i, j);
				return -EAGAIN;
			}
		}
	}

	return ret;
}

int sde_cp_crtc_check_properties(struct drm_crtc *crtc,
	struct drm_crtc_state *state)
{
	struct sde_crtc *sde_crtc = NULL;
	struct sde_crtc_state *sde_crtc_state = NULL;
	struct sde_cp_node *prop_node = NULL, *n = NULL;
	int ret = 0;

	if (!crtc || !crtc->dev || !state) {
		DRM_ERROR("invalid crtc %pK dev %pK state %pK\n", crtc,
				(crtc ? crtc->dev : NULL), state);
		return -EINVAL;
	}

	sde_crtc = to_sde_crtc(crtc);
	if (!sde_crtc) {
		DRM_ERROR("invalid sde_crtc %pK\n", sde_crtc);
		return -EINVAL;
	}

	sde_crtc_state = to_sde_crtc_state(state);
	if (!sde_crtc_state) {
		DRM_ERROR("invalid sde_crtc_state %pK\n", sde_crtc_state);
		return -EINVAL;
	}
	mutex_lock(&sde_crtc->crtc_cp_lock);

	ret = sde_cp_crtc_check_pu_features(crtc);
	if (ret) {
		DRM_ERROR("failed check pu features, ret %d\n", ret);
		goto exit;
	}

	if (list_empty(&sde_crtc->dirty_list)) {
		DRM_DEBUG_DRIVER("Dirty list is empty\n");
		goto exit;
	}

	list_for_each_entry_safe(prop_node, n, &sde_crtc->dirty_list,
		dirty_list) {
		ret = sde_cp_crtc_checkfeature(prop_node, sde_crtc,
				sde_crtc_state);
		if (ret) {
			DRM_ERROR("failed check on prop_node %u\n",
					prop_node->property_id);
			/* remove invalid feature from dirty list */
			list_del_init(&prop_node->dirty_list);
			goto exit;
		}
	}

exit:
	mutex_unlock(&sde_crtc->crtc_cp_lock);
	return ret;
}

static int sde_cp_crtc_set_pu_features(struct drm_crtc *crtc, bool *need_flush)
{
	int ret = 0, i = 0, j = 0;
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *sde_crtc_state;
	struct sde_hw_cp_cfg hw_cfg;
	struct sde_hw_dspp *hw_dspp;
	struct sde_hw_mixer *hw_lm;
	struct sde_mdss_cfg *catalog;
	struct sde_rect user_rect, cached_rect;

	if (!need_flush) {
		DRM_ERROR("invalid need_flush %pK\n", need_flush);
		return -EINVAL;
	}

	if (!crtc) {
		DRM_ERROR("invalid crtc %pK\n", crtc);
		return -EINVAL;
	}

	sde_crtc = to_sde_crtc(crtc);
	if (!sde_crtc) {
		DRM_ERROR("invalid sde_crtc %pK\n", sde_crtc);
		return -EINVAL;
	}

	sde_crtc_state = to_sde_crtc_state(crtc->state);
	if (!sde_crtc_state) {
		DRM_ERROR("invalid sde_crtc_state %pK\n", sde_crtc_state);
		return -EINVAL;
	}

	for (i = 0; i < sde_crtc->num_mixers; i++) {
		if (!sde_crtc->mixers[i].hw_lm) {
			DRM_ERROR("invalid lm in mixer %d\n", i);
			return -EINVAL;
		}

		if (!sde_crtc->mixers[i].hw_ctl) {
			DRM_ERROR("invalid ctl in mixer %d\n", i);
			return -EINVAL;
		}
	}

	/* early return if not a partial update frame or no change in rois */
	if (sde_crtc_state->user_roi_list.num_rects == 0) {
		DRM_DEBUG_DRIVER("no partial update required\n");
		memset(&sde_crtc_state->cached_user_roi_list, 0,
				sizeof(struct msm_roi_list));
		return 0;
	}

	sde_kms_rect_merge_rectangles(&sde_crtc_state->user_roi_list,
			&user_rect);
	sde_kms_rect_merge_rectangles(&sde_crtc_state->cached_user_roi_list,
			&cached_rect);
	if (sde_kms_rect_is_equal(&user_rect, &cached_rect)) {
		DRM_DEBUG_DRIVER("no change in list of ROIs\n");
		return 0;
	}

	catalog = get_kms(&sde_crtc->base)->catalog;
	memset(&hw_cfg, 0, sizeof(hw_cfg));
	hw_cfg.num_of_mixers = sde_crtc->num_mixers;
	hw_cfg.broadcast_disabled = catalog->dma_cfg.broadcast_disabled;
	hw_cfg.payload = &sde_crtc_state->user_roi_list;
	hw_cfg.len = sizeof(sde_crtc_state->user_roi_list);
	for (i = 0; i < hw_cfg.num_of_mixers; i++)
		hw_cfg.dspp[i] = sde_crtc->mixers[i].hw_dspp;

	for (i = 0; i < SDE_CP_CRTC_MAX_PU_FEATURES; i++) {
		feature_wrapper set_pu_feature =
				set_crtc_pu_feature_wrappers[i];

		if (!set_pu_feature ||
				!(sde_crtc->cp_pu_feature_mask & BIT(i)))
			continue;

		for (j = 0; j < hw_cfg.num_of_mixers; j++) {
			hw_lm = sde_crtc->mixers[j].hw_lm;
			hw_dspp = sde_crtc->mixers[j].hw_dspp;

			hw_cfg.mixer_info = hw_lm;
			hw_cfg.ctl = sde_crtc->mixers[j].hw_ctl;
			hw_cfg.displayh = hw_cfg.num_of_mixers *
					hw_lm->cfg.out_width;
			hw_cfg.displayv = hw_lm->cfg.out_height;

			ret = set_pu_feature(hw_dspp, &hw_cfg, sde_crtc);
			/* feature does not need flush when ret > 0 */
			if (ret < 0) {
				DRM_ERROR("failed pu feature %d in mixer %d\n",
						i, j);
				return ret;
			} else if (ret == 0) {
				sde_cp_dspp_flush_helper(sde_crtc,
						sde_cp_crtc_pu_to_feature[i]);
				*need_flush = true;
			}
		}
	}

	memcpy(&sde_crtc_state->cached_user_roi_list,
			&sde_crtc_state->user_roi_list,
			sizeof(struct msm_roi_list));

	return 0;
}

static void _sde_clear_ltm_merge_mode(struct sde_crtc *sde_crtc)
{
	u32 num_mixers = 0, i = 0;
	struct sde_hw_ctl *ctl = NULL;
	struct sde_hw_dspp *hw_dspp = NULL;
	unsigned long irq_flags;

	num_mixers = sde_crtc->num_mixers;
	if (!num_mixers) {
		DRM_ERROR("no mixers for this crtc\n");
		return;
	}

	spin_lock_irqsave(&sde_crtc->ltm_lock, irq_flags);
	if (!sde_crtc->ltm_merge_clear_pending) {
		spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
		return;
	}

	sde_cp_dspp_flush_helper(sde_crtc, SDE_CP_CRTC_DSPP_LTM_HIST_CTL);
	for (i = 0; i < num_mixers; i++) {
		hw_dspp = sde_crtc->mixers[i].hw_dspp;
		ctl = sde_crtc->mixers[i].hw_ctl;
		if (!hw_dspp || !ctl || i >= DSPP_MAX)
			continue;
		if (hw_dspp->ops.clear_ltm_merge_mode)
			hw_dspp->ops.clear_ltm_merge_mode(hw_dspp);
		if (ctl->ops.update_bitmask_dspp)
			ctl->ops.update_bitmask_dspp(ctl, hw_dspp->idx, 1);
	}

	sde_crtc->ltm_merge_clear_pending = false;
	SDE_EVT32(DRMID(&sde_crtc->base), num_mixers, sde_crtc->ltm_merge_clear_pending);
	spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
}

void sde_cp_crtc_apply_properties(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = NULL;
	bool set_dspp_flush = false, set_lm_flush = false;
	struct sde_cp_node *prop_node = NULL, *n = NULL;
	struct sde_hw_ctl *ctl;
	u32 num_mixers = 0, i = 0;
	int rc = 0;
	bool need_flush = false;

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
		DRM_ERROR("no mixers for this crtc\n");
		return;
	}

	mutex_lock(&sde_crtc->crtc_cp_lock);
	_sde_clear_ltm_merge_mode(sde_crtc);

	if (list_empty(&sde_crtc->dirty_list) &&
			list_empty(&sde_crtc->ad_dirty) &&
			list_empty(&sde_crtc->ad_active) &&
			list_empty(&sde_crtc->active_list)) {
		DRM_DEBUG_DRIVER("all lists are empty\n");
		goto exit;
	}

	rc = sde_cp_crtc_set_pu_features(crtc, &need_flush);
	if (rc) {
		DRM_ERROR("failed set pu features, skip cp updates\n");
		goto exit;
	} else if (need_flush) {
		set_dspp_flush = true;
		set_lm_flush = true;
	}

	list_for_each_entry_safe(prop_node, n, &sde_crtc->dirty_list,
			dirty_list) {
		sde_cp_crtc_setfeature(prop_node, sde_crtc);
		sde_cp_dspp_flush_helper(sde_crtc, prop_node->feature);
		if (prop_node->is_dspp_feature &&
				!prop_node->lm_flush_override)
			set_dspp_flush = true;
		else
			set_lm_flush = true;
	}

	sde_cp_dspp_flush_helper(sde_crtc, SDE_CP_CRTC_DSPP_SB);

	if (!list_empty(&sde_crtc->ad_active)) {
		sde_cp_ad_set_prop(sde_crtc, AD_IPC_RESET);
		set_dspp_flush = true;
	}

	list_for_each_entry_safe(prop_node, n, &sde_crtc->ad_dirty,
			dirty_list) {
		sde_cp_crtc_setfeature(prop_node, sde_crtc);
		set_dspp_flush = true;
	}

	for (i = 0; i < num_mixers; i++) {
		ctl = sde_crtc->mixers[i].hw_ctl;
		if (!ctl)
			continue;
		if (set_dspp_flush && ctl->ops.update_bitmask_dspp
				&& sde_crtc->mixers[i].hw_dspp) {
			ctl->ops.update_bitmask_dspp(ctl,
					sde_crtc->mixers[i].hw_dspp->idx, 1);
		}
		if (set_lm_flush && ctl->ops.update_bitmask_mixer
				&& sde_crtc->mixers[i].hw_lm) {
			ctl->ops.update_bitmask_mixer(ctl,
					sde_crtc->mixers[i].hw_lm->idx, 1);
		}
	}
exit:
	mutex_unlock(&sde_crtc->crtc_cp_lock);
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

	mutex_lock(&sde_crtc->crtc_cp_lock);

	/**
	 * Function can be called during the atomic_check with test_only flag
	 * and actual commit. Allocate properties only if feature list is
	 * empty during the atomic_check with test_only flag.
	 */
	if (!list_empty(&sde_crtc->feature_list))
		goto exit;

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
		setup_set_crtc_feature_wrappers(set_crtc_feature_wrappers);
		setup_check_crtc_feature_wrappers(check_crtc_feature_wrappers);
		setup_set_crtc_pu_feature_wrappers(
				set_crtc_pu_feature_wrappers);
		setup_check_crtc_pu_feature_wrappers(
				check_crtc_pu_feature_wrappers);
		setup_dspp_caps_funcs(dspp_cap_update_func);
	}
	if (!priv->cp_property)
		goto exit;

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
		goto exit;

	/* Check for all the LM properties and attach it to CRTC */
	features = catalog->mixer[0].features;
	for (i = 0; i < SDE_MIXER_MAX; i++) {
		if (!test_bit(i, &features))
			continue;
		if (lm_prop_install_func[i])
			lm_prop_install_func[i](crtc);
	}
exit:
	mutex_unlock(&sde_crtc->crtc_cp_lock);

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

	mutex_lock(&sde_crtc->crtc_cp_lock);
	list_for_each_entry(prop_node, &sde_crtc->feature_list, feature_list) {
		if (property->base.id == prop_node->property_id) {
			found = 1;
			break;
		}
	}

	if (!found) {
		ret = -ENOENT;
		goto exit;
	}

	/**
	 * sde_crtc is virtual ensure that hardware has been attached to the
	 * crtc. Check LM and dspp counts based on whether feature is a
	 * dspp/lm feature.
	 */
	if (sde_crtc->num_mixers > ARRAY_SIZE(sde_crtc->mixers)) {
		DRM_INFO("Invalid mixer config act cnt %d max cnt %ld\n",
			sde_crtc->num_mixers,
				(long)ARRAY_SIZE(sde_crtc->mixers));
		ret = -EPERM;
		goto exit;
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
		ret = -EINVAL;
		goto exit;
	} else if (lm_cnt < sde_crtc->num_mixers) {
		DRM_ERROR("invalid lm cnt %d mixer cnt %d\n", lm_cnt,
			sde_crtc->num_mixers);
		ret = -EINVAL;
		goto exit;
	}

	ret = sde_cp_ad_validate_prop(prop_node, sde_crtc);
	if (ret) {
		DRM_ERROR("ad property validation failed ret %d\n", ret);
		goto exit;
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
exit:
	mutex_unlock(&sde_crtc->crtc_cp_lock);
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
	mutex_lock(&sde_crtc->crtc_cp_lock);
	list_for_each_entry(prop_node, &sde_crtc->feature_list, feature_list) {
		if (property->base.id == prop_node->property_id) {
			*val = prop_node->prop_val;
			break;
		}
	}
	mutex_unlock(&sde_crtc->crtc_cp_lock);
	return 0;
}

void sde_cp_crtc_destroy_properties(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = NULL;
	struct sde_cp_node *prop_node = NULL, *n = NULL;
	u32 i = 0;

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
			drm_property_blob_put(prop_node->blob_ptr);

		list_del_init(&prop_node->active_list);
		list_del_init(&prop_node->dirty_list);
		list_del_init(&prop_node->feature_list);
		sde_cp_destroy_local_blob(prop_node);
		kfree(prop_node);
	}

	if (sde_crtc->hist_blob)
		drm_property_blob_put(sde_crtc->hist_blob);

	for (i = 0; i < sde_crtc->ltm_buffer_cnt; i++) {
		if (sde_crtc->ltm_buffers[i]) {
			msm_gem_put_vaddr(sde_crtc->ltm_buffers[i]->gem);
			drm_framebuffer_put(sde_crtc->ltm_buffers[i]->fb);
			msm_gem_put_iova(sde_crtc->ltm_buffers[i]->gem,
					sde_crtc->ltm_buffers[i]->aspace);
			kfree(sde_crtc->ltm_buffers[i]);
			sde_crtc->ltm_buffers[i] = NULL;
		}
	}
	sde_crtc->ltm_buffer_cnt = 0;
	sde_crtc->ltm_hist_en = false;
	sde_crtc->ltm_merge_clear_pending = false;
	SDE_EVT32(DRMID(crtc), sde_crtc->ltm_merge_clear_pending);
	sde_crtc->hist_irq_idx = -1;

	mutex_destroy(&sde_crtc->crtc_cp_lock);
	INIT_LIST_HEAD(&sde_crtc->active_list);
	INIT_LIST_HEAD(&sde_crtc->dirty_list);
	INIT_LIST_HEAD(&sde_crtc->ad_dirty);
	INIT_LIST_HEAD(&sde_crtc->ad_active);
	INIT_LIST_HEAD(&sde_crtc->ltm_buf_free);
	INIT_LIST_HEAD(&sde_crtc->ltm_buf_busy);
}

void sde_cp_crtc_suspend(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = NULL;
	struct sde_cp_node *prop_node = NULL, *n = NULL;
	bool ad_suspend = false;
	unsigned long irq_flags;

	if (!crtc) {
		DRM_ERROR("crtc %pK\n", crtc);
		return;
	}
	sde_crtc = to_sde_crtc(crtc);
	if (!sde_crtc) {
		DRM_ERROR("sde_crtc %pK\n", sde_crtc);
		return;
	}

	mutex_lock(&sde_crtc->crtc_cp_lock);
	list_for_each_entry_safe(prop_node, n, &sde_crtc->active_list,
				 active_list) {
		sde_cp_update_list(prop_node, sde_crtc, true);
		list_del_init(&prop_node->active_list);
	}

	list_for_each_entry_safe(prop_node, n, &sde_crtc->ad_active,
				 active_list) {
		sde_cp_update_list(prop_node, sde_crtc, true);
		list_del_init(&prop_node->active_list);
		ad_suspend = true;
	}
	mutex_unlock(&sde_crtc->crtc_cp_lock);

	spin_lock_irqsave(&sde_crtc->ltm_lock, irq_flags);
	sde_crtc->ltm_hist_en = false;
	spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);

	if (ad_suspend)
		sde_cp_ad_set_prop(sde_crtc, AD_SUSPEND);
}

void sde_cp_crtc_resume(struct drm_crtc *crtc)
{
	/* placeholder for operations needed during resume */
}

void sde_cp_crtc_clear(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = NULL;
	unsigned long flags;
	u32 i = 0;

	if (!crtc) {
		DRM_ERROR("crtc %pK\n", crtc);
		return;
	}
	sde_crtc = to_sde_crtc(crtc);
	if (!sde_crtc) {
		DRM_ERROR("sde_crtc %pK\n", sde_crtc);
		return;
	}

	mutex_lock(&sde_crtc->crtc_cp_lock);
	list_del_init(&sde_crtc->active_list);
	list_del_init(&sde_crtc->dirty_list);
	list_del_init(&sde_crtc->ad_active);
	list_del_init(&sde_crtc->ad_dirty);
	sde_crtc->cp_pu_feature_mask = 0;
	mutex_unlock(&sde_crtc->crtc_cp_lock);

	spin_lock_irqsave(&sde_crtc->spin_lock, flags);
	list_del_init(&sde_crtc->user_event_list);
	spin_unlock_irqrestore(&sde_crtc->spin_lock, flags);

	for (i = 0; i < sde_crtc->ltm_buffer_cnt; i++) {
		if (sde_crtc->ltm_buffers[i]) {
			msm_gem_put_vaddr(sde_crtc->ltm_buffers[i]->gem);
			drm_framebuffer_put(sde_crtc->ltm_buffers[i]->fb);
			msm_gem_put_iova(sde_crtc->ltm_buffers[i]->gem,
					sde_crtc->ltm_buffers[i]->aspace);
			kfree(sde_crtc->ltm_buffers[i]);
			sde_crtc->ltm_buffers[i] = NULL;
		}
	}
	sde_crtc->ltm_buffer_cnt = 0;
	sde_crtc->ltm_hist_en = false;
	sde_crtc->ltm_merge_clear_pending = false;
	sde_crtc->hist_irq_idx = -1;
	INIT_LIST_HEAD(&sde_crtc->ltm_buf_free);
	INIT_LIST_HEAD(&sde_crtc->ltm_buf_busy);
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
	case 4:
	case 5:
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
			"SDE_DSPP_PA_HSIC_V", version);
		sde_cp_crtc_install_blob_property(crtc, feature_name,
			SDE_CP_CRTC_DSPP_HSIC, sizeof(struct drm_msm_pa_hsic));
		break;
	default:
		DRM_ERROR("version %d not supported\n", version);
		break;
	}
}

static void dspp_memcolor_install_property(struct drm_crtc *crtc)
{
	char feature_name[256];
	struct sde_kms *kms = NULL;
	struct sde_mdss_cfg *catalog = NULL;
	u32 version;

	kms = get_kms(crtc);
	catalog = kms->catalog;
	version = catalog->dspp[0].sblk->memcolor.version >> 16;
	switch (version) {
	case 1:
		snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
			"SDE_DSPP_PA_MEMCOL_SKIN_V", version);
		sde_cp_crtc_install_blob_property(crtc, feature_name,
			SDE_CP_CRTC_DSPP_MEMCOL_SKIN,
			sizeof(struct drm_msm_memcol));
		snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
			"SDE_DSPP_PA_MEMCOL_SKY_V", version);
		sde_cp_crtc_install_blob_property(crtc, feature_name,
			SDE_CP_CRTC_DSPP_MEMCOL_SKY,
			sizeof(struct drm_msm_memcol));
		snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
			"SDE_DSPP_PA_MEMCOL_FOLIAGE_V", version);
		sde_cp_crtc_install_blob_property(crtc, feature_name,
			SDE_CP_CRTC_DSPP_MEMCOL_FOLIAGE,
			sizeof(struct drm_msm_memcol));
		snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
			"SDE_DSPP_PA_MEMCOL_PROT_V", version);
		sde_cp_crtc_install_blob_property(crtc, feature_name,
			SDE_CP_CRTC_DSPP_MEMCOL_PROT,
			sizeof(struct drm_msm_memcol));
		break;
	default:
		DRM_ERROR("version %d not supported\n", version);
		break;
	}
}

static void dspp_sixzone_install_property(struct drm_crtc *crtc)
{
	char feature_name[256];
	struct sde_kms *kms = NULL;
	struct sde_mdss_cfg *catalog = NULL;
	u32 version;

	kms = get_kms(crtc);
	catalog = kms->catalog;
	version = catalog->dspp[0].sblk->sixzone.version >> 16;
	switch (version) {
	case 1:
		snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
			"SDE_DSPP_PA_SIXZONE_V", version);
		sde_cp_crtc_install_blob_property(crtc, feature_name,
			SDE_CP_CRTC_DSPP_SIXZONE,
			sizeof(struct drm_msm_sixzone));
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
			"SDE_DSPP_AD_V4_ASSERTIVENESS",
			SDE_CP_CRTC_DSPP_AD_ASSERTIVENESS, 0, (BIT(8) - 1), 0);
		sde_cp_crtc_install_range_property(crtc,
			"SDE_DSPP_AD_V4_STRENGTH",
			SDE_CP_CRTC_DSPP_AD_STRENGTH, 0, U64_MAX, 0);
		sde_cp_create_local_blob(crtc, SDE_CP_CRTC_DSPP_AD_STRENGTH,
			sizeof(struct drm_msm_ad4_manual_str_cfg));
		sde_cp_crtc_install_range_property(crtc, "SDE_DSPP_AD_V4_INPUT",
			SDE_CP_CRTC_DSPP_AD_INPUT, 0, U16_MAX, 0);
		sde_cp_crtc_install_range_property(crtc,
				"SDE_DSPP_AD_V4_BACKLIGHT",
			SDE_CP_CRTC_DSPP_AD_BACKLIGHT, 0, (BIT(16) - 1),
			0);

		sde_cp_crtc_install_range_property(crtc, "SDE_DSPP_AD_V4_ROI",
			SDE_CP_CRTC_DSPP_AD_ROI, 0, U64_MAX, 0);
		sde_cp_create_local_blob(crtc, SDE_CP_CRTC_DSPP_AD_ROI,
			sizeof(struct drm_msm_ad4_roi_cfg));
		break;
	default:
		DRM_ERROR("version %d not supported\n", version);
		break;
	}
}

static void dspp_ltm_install_property(struct drm_crtc *crtc)
{
	char feature_name[256];
	struct sde_kms *kms = NULL;
	struct sde_mdss_cfg *catalog = NULL;
	u32 version;

	kms = get_kms(crtc);
	catalog = kms->catalog;
	version = catalog->dspp[0].sblk->ltm.version >> 16;
	snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
		"SDE_DSPP_LTM_V", version);
	switch (version) {
	case 1:
		sde_cp_crtc_install_immutable_property(crtc,
			feature_name, SDE_CP_CRTC_DSPP_LTM);

		sde_cp_crtc_install_range_property(crtc, "SDE_DSPP_LTM_INIT_V1",
			SDE_CP_CRTC_DSPP_LTM_INIT, 0, U64_MAX, 0);
		sde_cp_create_local_blob(crtc, SDE_CP_CRTC_DSPP_LTM_INIT,
			sizeof(struct drm_msm_ltm_init_param));

		sde_cp_crtc_install_range_property(crtc, "SDE_DSPP_LTM_ROI_V1",
			SDE_CP_CRTC_DSPP_LTM_ROI, 0, U64_MAX, 0);
		sde_cp_create_local_blob(crtc, SDE_CP_CRTC_DSPP_LTM_ROI,
			sizeof(struct drm_msm_ltm_cfg_param));

		sde_cp_crtc_install_enum_property(crtc,
			SDE_CP_CRTC_DSPP_LTM_HIST_CTL, sde_ltm_hist_modes,
			ARRAY_SIZE(sde_ltm_hist_modes),
			"SDE_DSPP_LTM_HIST_CTRL_V1");

		sde_cp_crtc_install_range_property(crtc,
			"SDE_DSPP_LTM_HIST_THRESH_V1",
			SDE_CP_CRTC_DSPP_LTM_HIST_THRESH, 0, (BIT(10) - 1), 0);

		sde_cp_crtc_install_range_property(crtc,
			"SDE_DSPP_LTM_SET_BUF_V1",
			SDE_CP_CRTC_DSPP_LTM_SET_BUF, 0, U64_MAX, 0);
		sde_cp_create_local_blob(crtc, SDE_CP_CRTC_DSPP_LTM_SET_BUF,
			sizeof(struct drm_msm_ltm_buffers_ctrl));

		sde_cp_crtc_install_range_property(crtc,
			"SDE_DSPP_LTM_QUEUE_BUF_V1",
			SDE_CP_CRTC_DSPP_LTM_QUEUE_BUF, 0, U64_MAX, 0);

		sde_cp_crtc_install_range_property(crtc,
			"SDE_DSPP_LTM_QUEUE_BUF2_V1",
			SDE_CP_CRTC_DSPP_LTM_QUEUE_BUF2, 0, U64_MAX, 0);

		sde_cp_crtc_install_range_property(crtc,
			"SDE_DSPP_LTM_QUEUE_BUF3_V1",
			SDE_CP_CRTC_DSPP_LTM_QUEUE_BUF3, 0, U64_MAX, 0);

		sde_cp_crtc_install_range_property(crtc,
			"SDE_DSPP_LTM_VLUT_V1",
			SDE_CP_CRTC_DSPP_LTM_VLUT, 0, U64_MAX, 0);
		sde_cp_create_local_blob(crtc, SDE_CP_CRTC_DSPP_LTM_VLUT,
			sizeof(struct drm_msm_ltm_data));
		break;
	default:
		DRM_ERROR("version %d not supported\n", version);
		break;
	}
}

static void dspp_rc_install_property(struct drm_crtc *crtc)
{
	char feature_name[256];
	struct sde_kms *kms = NULL;
	struct sde_mdss_cfg *catalog = NULL;
	struct sde_cp_node *prop_node = NULL;
	struct sde_crtc *sde_crtc = NULL;
	u32 version;

	if (!crtc) {
		DRM_ERROR("invalid arguments");
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
	if (!sde_crtc) {
		DRM_ERROR("invalid sde_crtc %pK\n", sde_crtc);
		return;
	}

	kms = get_kms(crtc);
	catalog = kms->catalog;
	version = catalog->dspp[0].sblk->rc.version >> 16;
	switch (version) {
	case 1:
		snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
				"SDE_DSPP_RC_MASK_V", version);
		sde_cp_crtc_install_blob_property(crtc, feature_name,
				SDE_CP_CRTC_DSPP_RC_MASK,
				sizeof(struct drm_msm_rc_mask_cfg));

		/* Override flush mechanism to use layer mixer flush bits */
		if (!catalog->rc_lm_flush_override)
			break;

		DRM_DEBUG("rc using lm flush override\n");
		list_for_each_entry(prop_node, &sde_crtc->feature_list,
				feature_list) {
			if (prop_node->feature == SDE_CP_CRTC_DSPP_RC_MASK) {
				prop_node->lm_flush_override = true;
				break;
			}
		}

		break;
	default:
		DRM_ERROR("version %d not supported\n", version);
		break;
	}
}

static void dspp_spr_install_property(struct drm_crtc *crtc)
{
	struct sde_kms *kms = NULL;
	u32 version = 0;

	kms = get_kms(crtc);
	if (!kms) {
		DRM_ERROR("!kms = %d\n ", !kms);
		return;
	}

	version = kms->catalog->dspp[0].sblk->spr.version >> 16;
	switch (version) {
	case 1:
		sde_cp_crtc_install_blob_property(crtc, "SDE_SPR_INIT_CFG_V1",
				SDE_CP_CRTC_DSPP_SPR_INIT,
				sizeof(struct drm_msm_spr_init_cfg));
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

static void dspp_igc_install_property(struct drm_crtc *crtc)
{
	char feature_name[256];
	struct sde_kms *kms = NULL;
	struct sde_mdss_cfg *catalog = NULL;
	u32 version;

	kms = get_kms(crtc);
	catalog = kms->catalog;

	version = catalog->dspp[0].sblk->igc.version >> 16;
	snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
		"SDE_DSPP_IGC_V", version);
	switch (version) {
	case 3:
	case 4:
		sde_cp_crtc_install_blob_property(crtc, feature_name,
			SDE_CP_CRTC_DSPP_IGC, sizeof(struct drm_msm_igc_lut));
		break;
	default:
		DRM_ERROR("version %d not supported\n", version);
		break;
	}
}

static void dspp_hist_install_property(struct drm_crtc *crtc)
{
	struct sde_kms *kms = NULL;
	struct sde_mdss_cfg *catalog = NULL;
	u32 version;

	kms = get_kms(crtc);
	catalog = kms->catalog;

	version = catalog->dspp[0].sblk->hist.version >> 16;
	switch (version) {
	case 1:
		sde_cp_crtc_install_enum_property(crtc,
			SDE_CP_CRTC_DSPP_HIST_CTRL, sde_hist_modes,
			ARRAY_SIZE(sde_hist_modes), "SDE_DSPP_HIST_CTRL_V1");
		sde_cp_crtc_install_range_property(crtc, "SDE_DSPP_HIST_IRQ_V1",
			SDE_CP_CRTC_DSPP_HIST_IRQ, 0, U16_MAX, 0);
		break;
	default:
		DRM_ERROR("version %d not supported\n", version);
		break;
	}
}

static void dspp_dither_install_property(struct drm_crtc *crtc)
{
	char feature_name[256];
	struct sde_kms *kms = NULL;
	struct sde_mdss_cfg *catalog = NULL;
	u32 version;

	kms = get_kms(crtc);
	catalog = kms->catalog;

	version = catalog->dspp[0].sblk->dither.version >> 16;
	snprintf(feature_name, ARRAY_SIZE(feature_name), "%s%d",
		"SDE_DSPP_PA_DITHER_V", version);
	switch (version) {
	case 1:
		sde_cp_crtc_install_blob_property(crtc, feature_name,
			SDE_CP_CRTC_DSPP_DITHER,
			sizeof(struct drm_msm_pa_dither));
		break;
	default:
		DRM_ERROR("version %d not supported\n", version);
		break;
	}
}

static  void dspp_demura_install_property(struct drm_crtc *crtc)
{
	struct sde_kms *kms = NULL;
	struct sde_mdss_cfg *catalog = NULL;
	u32 version;

	kms = get_kms(crtc);
	catalog = kms->catalog;

	version = catalog->dspp[0].sblk->demura.version >> 16;
	switch (version) {
	case 1:
		sde_cp_crtc_install_blob_property(crtc, "DEMURA_INIT_V1",
			SDE_CP_CRTC_DSPP_DEMURA_INIT,
			sizeof(struct drm_msm_dem_cfg));
		sde_cp_crtc_install_range_property(crtc, "DEMURA_BACKLIGHT",
				SDE_CP_CRTC_DSPP_DEMURA_BACKLIGHT,
				0, 1024, 0);
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
	case SDE_CP_CRTC_DSPP_AD_STRENGTH:
	case SDE_CP_CRTC_DSPP_AD_ROI:
		if (dirty_list)
			list_add_tail(&prop_node->dirty_list, &crtc->ad_dirty);
		else
			list_add_tail(&prop_node->active_list,
					&crtc->ad_active);
		break;
	case SDE_CP_CRTC_DSPP_LTM_SET_BUF:
	case SDE_CP_CRTC_DSPP_LTM_QUEUE_BUF:
	case SDE_CP_CRTC_DSPP_LTM_QUEUE_BUF2:
	case SDE_CP_CRTC_DSPP_LTM_QUEUE_BUF3:
		if (dirty_list)
			list_add_tail(&prop_node->dirty_list,
					&crtc->dirty_list);
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
	}
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
		case SDE_CP_CRTC_DSPP_AD_STRENGTH:
			ad_prop = AD_STRENGTH;
			break;
		case SDE_CP_CRTC_DSPP_AD_ROI:
			ad_prop = AD_ROI;
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

	sde_crtc_event_queue(&crtc->base, sde_cp_notify_ad_event,
							NULL, true);
}

static void sde_cp_notify_ad_event(struct drm_crtc *crtc_drm, void *arg)
{
	uint32_t input_bl = 0, output_bl = 0;
	uint32_t scale = MAX_SV_BL_SCALE_LEVEL;
	struct sde_hw_mixer *hw_lm = NULL;
	struct sde_hw_dspp *hw_dspp = NULL;
	u32 num_mixers;
	struct sde_crtc *crtc;
	struct drm_event event;
	int i;
	struct msm_drm_private *priv;
	struct sde_kms *kms;
	int ret;

	crtc = to_sde_crtc(crtc_drm);
	num_mixers = crtc->num_mixers;
	if (!num_mixers)
		return;

	for (i = 0; i < num_mixers; i++) {
		hw_lm = crtc->mixers[i].hw_lm;
		hw_dspp = crtc->mixers[i].hw_dspp;
		if (!hw_lm->cfg.right_mixer)
			break;
	}

	if (!hw_dspp)
		return;

	kms = get_kms(crtc_drm);
	if (!kms || !kms->dev) {
		SDE_ERROR("invalid arg(s)\n");
		return;
	}

	priv = kms->dev->dev_private;
	ret = pm_runtime_get_sync(kms->dev->dev);
	if (ret < 0) {
		SDE_ERROR("failed to enable power resource %d\n", ret);
		SDE_EVT32(ret, SDE_EVTLOG_ERROR);
		return;
	}

	hw_dspp->ops.ad_read_intr_resp(hw_dspp, AD4_IN_OUT_BACKLIGHT,
			&input_bl, &output_bl);

	pm_runtime_put_sync(kms->dev->dev);
	if (!input_bl || input_bl < output_bl)
		return;

	scale = (output_bl * MAX_SV_BL_SCALE_LEVEL) / input_bl;
	event.length = sizeof(u32);
	event.type = DRM_EVENT_AD_BACKLIGHT;
	msm_mode_object_event_notify(&crtc_drm->base, crtc_drm->dev,
			&event, (u8 *)&scale);
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
	unsigned long flags;
	struct sde_cp_node prop_node;
	struct sde_crtc_irq_info *node = NULL;

	if (!crtc_drm || !ad_irq) {
		DRM_ERROR("invalid crtc %pK irq %pK\n", crtc_drm, ad_irq);
		return -EINVAL;
	}

	crtc = to_sde_crtc(crtc_drm);
	if (!crtc) {
		DRM_ERROR("invalid sde_crtc %pK\n", crtc);
		return -EINVAL;
	}

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

	node = container_of(ad_irq, struct sde_crtc_irq_info, irq);

	if (!en) {
		spin_lock_irqsave(&node->state_lock, flags);
		if (node->state == IRQ_ENABLED) {
			ret = sde_core_irq_disable(kms, &irq_idx, 1);
			if (ret)
				DRM_ERROR("disable irq %d error %d\n",
					irq_idx, ret);
			else
				node->state = IRQ_NOINIT;
		} else {
			node->state = IRQ_NOINIT;
		}
		spin_unlock_irqrestore(&node->state_lock, flags);
		sde_core_irq_unregister_callback(kms, irq_idx, ad_irq);
		ret = 0;
		goto exit;
	}

	ad_irq->arg = crtc;
	ad_irq->func = sde_cp_ad_interrupt_cb;
	ret = sde_core_irq_register_callback(kms, irq_idx, ad_irq);
	if (ret) {
		DRM_ERROR("failed to register the callback ret %d\n", ret);
		goto exit;
	}

	spin_lock_irqsave(&node->state_lock, flags);
	if (node->state == IRQ_DISABLED || node->state == IRQ_NOINIT) {
		ret = sde_core_irq_enable(kms, &irq_idx, 1);
		if (ret) {
			DRM_ERROR("enable irq %d error %d\n", irq_idx, ret);
			sde_core_irq_unregister_callback(kms, irq_idx, ad_irq);
		} else {
			node->state = IRQ_ENABLED;
		}
	}
	spin_unlock_irqrestore(&node->state_lock, flags);

exit:
	return ret;
}

static void sde_cp_ad_set_prop(struct sde_crtc *sde_crtc,
		enum ad_property ad_prop)
{
	struct sde_ad_hw_cfg ad_cfg;
	struct sde_hw_cp_cfg hw_cfg;
	struct sde_hw_dspp *hw_dspp = NULL;
	struct sde_hw_mixer *hw_lm = NULL;
	u32 num_mixers = sde_crtc->num_mixers;
	int i = 0, ret = 0;

	hw_cfg.num_of_mixers = sde_crtc->num_mixers;

	for (i = 0; i < num_mixers && !ret; i++) {
		hw_lm = sde_crtc->mixers[i].hw_lm;
		hw_dspp = sde_crtc->mixers[i].hw_dspp;
		if (!hw_lm || !hw_dspp || !hw_dspp->ops.validate_ad ||
				!hw_dspp->ops.setup_ad) {
			ret = -EINVAL;
			continue;
		}

		hw_cfg.displayh = num_mixers * hw_lm->cfg.out_width;
		hw_cfg.displayv = hw_lm->cfg.out_height;
		hw_cfg.mixer_info = hw_lm;
		ad_cfg.prop = ad_prop;
		ad_cfg.hw_cfg = &hw_cfg;
		ret = hw_dspp->ops.validate_ad(hw_dspp, (u32 *)&ad_prop);
		if (!ret)
			hw_dspp->ops.setup_ad(hw_dspp, &ad_cfg);
	}
}

void sde_cp_crtc_pre_ipc(struct drm_crtc *drm_crtc)
{
	struct sde_crtc *sde_crtc;

	sde_crtc = to_sde_crtc(drm_crtc);
	if (!sde_crtc) {
		DRM_ERROR("invalid sde_crtc %pK\n", sde_crtc);
		return;
	}

	sde_cp_ad_set_prop(sde_crtc, AD_IPC_SUSPEND);
}

void sde_cp_crtc_post_ipc(struct drm_crtc *drm_crtc)
{
	struct sde_crtc *sde_crtc;

	sde_crtc = to_sde_crtc(drm_crtc);
	if (!sde_crtc) {
		DRM_ERROR("invalid sde_crtc %pK\n", sde_crtc);
		return;
	}

	sde_cp_ad_set_prop(sde_crtc, AD_IPC_RESUME);
}

static void sde_cp_hist_interrupt_cb(void *arg, int irq_idx)
{
	struct sde_crtc *crtc = arg;
	struct drm_crtc *crtc_drm = &crtc->base;
	struct sde_hw_dspp *hw_dspp;
	u32 lock_hist = 1;
	u32 i;

	/* lock histogram buffer */
	for (i = 0; i < crtc->num_mixers; i++) {
		hw_dspp = crtc->mixers[i].hw_dspp;
		if (hw_dspp && hw_dspp->ops.lock_histogram)
			hw_dspp->ops.lock_histogram(hw_dspp, &lock_hist);
	}

	crtc->hist_irq_idx = irq_idx;
	/* notify histogram event */
	sde_crtc_event_queue(crtc_drm, sde_cp_notify_hist_event,
						&crtc->hist_irq_idx, true);
}

static void sde_cp_notify_hist_event(struct drm_crtc *crtc_drm, void *arg)
{
	struct sde_hw_dspp *hw_dspp = NULL;
	struct sde_crtc *crtc;
	struct drm_event event;
	struct drm_msm_hist *hist_data;
	struct sde_kms *kms;
	struct sde_crtc_irq_info *node = NULL;
	unsigned long flags, state_flags;
	int ret, irq_idx;
	u32 i, lock_hist = 0;

	if (!crtc_drm || !arg) {
		DRM_ERROR("invalid drm crtc %pK or arg %pK\n", crtc_drm, arg);
		return;
	}

	crtc = to_sde_crtc(crtc_drm);
	if (!crtc) {
		DRM_ERROR("invalid sde_crtc %pK\n", crtc);
		return;
	}

	kms = get_kms(crtc_drm);
	if (!kms || !kms->dev) {
		SDE_ERROR("invalid arg(s)\n");
		return;
	}

	/* disable histogram irq */
	spin_lock_irqsave(&crtc->spin_lock, flags);
	node = _sde_cp_get_intr_node(DRM_EVENT_HISTOGRAM, crtc);

	if (!node) {
		spin_unlock_irqrestore(&crtc->spin_lock, flags);
		DRM_DEBUG_DRIVER("cannot find histogram event node in crtc\n");
		/* unlock histogram */
		ret = pm_runtime_get_sync(kms->dev->dev);
		if (ret < 0) {
			SDE_ERROR("failed to enable power resource %d\n", ret);
			SDE_EVT32(ret, SDE_EVTLOG_ERROR);
			return;
		}
		for (i = 0; i < crtc->num_mixers; i++) {
			hw_dspp = crtc->mixers[i].hw_dspp;
			if (hw_dspp && hw_dspp->ops.lock_histogram)
				hw_dspp->ops.lock_histogram(hw_dspp,
					&lock_hist);
		}
		pm_runtime_put_sync(kms->dev->dev);
		return;
	}

	irq_idx = *(int *)arg;
	spin_lock_irqsave(&node->state_lock, state_flags);
	if (node->state == IRQ_ENABLED) {
		ret = sde_core_irq_disable_nolock(kms, irq_idx);
		if (ret) {
			DRM_ERROR("failed to disable irq %d, ret %d\n",
				irq_idx, ret);
			spin_unlock_irqrestore(&node->state_lock, state_flags);
			spin_unlock_irqrestore(&crtc->spin_lock, flags);
			ret = pm_runtime_get_sync(kms->dev->dev);
			if (ret < 0) {
				SDE_ERROR("failed to enable power %d\n", ret);
				SDE_EVT32(ret, SDE_EVTLOG_ERROR);
				return;
			}

			/* unlock histogram */
			for (i = 0; i < crtc->num_mixers; i++) {
				hw_dspp = crtc->mixers[i].hw_dspp;
				if (hw_dspp && hw_dspp->ops.lock_histogram)
					hw_dspp->ops.lock_histogram(hw_dspp,
						&lock_hist);
			}
			pm_runtime_put_sync(kms->dev->dev);
			return;
		}
		node->state = IRQ_DISABLED;
	}
	spin_unlock_irqrestore(&node->state_lock, state_flags);
	spin_unlock_irqrestore(&crtc->spin_lock, flags);

	if (!crtc->hist_blob)
		return;

	ret = pm_runtime_get_sync(kms->dev->dev);
	if (ret < 0) {
		SDE_ERROR("failed to enable power resource %d\n", ret);
		SDE_EVT32(ret, SDE_EVTLOG_ERROR);
		return;
	}

	/* read histogram data into blob */
	hist_data = (struct drm_msm_hist *)crtc->hist_blob->data;
	memset(hist_data->data, 0, sizeof(hist_data->data));
	for (i = 0; i < crtc->num_mixers; i++) {
		hw_dspp = crtc->mixers[i].hw_dspp;
		if (!hw_dspp || !hw_dspp->ops.read_histogram) {
			DRM_ERROR("invalid dspp %pK or read_histogram func\n",
				hw_dspp);
			pm_runtime_put_sync(kms->dev->dev);
			return;
		}
		hw_dspp->ops.read_histogram(hw_dspp, hist_data);
	}

	pm_runtime_put_sync(kms->dev->dev);
	/* send histogram event with blob id */
	event.length = sizeof(u32);
	event.type = DRM_EVENT_HISTOGRAM;
	msm_mode_object_event_notify(&crtc_drm->base, crtc_drm->dev,
			&event, (u8 *)(&crtc->hist_blob->base.id));
}

int sde_cp_hist_interrupt(struct drm_crtc *crtc_drm, bool en,
	struct sde_irq_callback *hist_irq)
{
	struct sde_kms *kms = NULL;
	u32 num_mixers;
	struct sde_hw_mixer *hw_lm;
	struct sde_hw_dspp *hw_dspp = NULL;
	struct sde_crtc *crtc;
	struct sde_crtc_irq_info *node = NULL;
	int i, irq_idx, ret = 0;
	unsigned long flags;

	if (!crtc_drm || !hist_irq) {
		DRM_ERROR("invalid crtc %pK irq %pK\n", crtc_drm, hist_irq);
		return -EINVAL;
	}

	crtc = to_sde_crtc(crtc_drm);
	if (!crtc) {
		DRM_ERROR("invalid sde_crtc %pK\n", crtc);
		return -EINVAL;
	}

	kms = get_kms(crtc_drm);
	num_mixers = crtc->num_mixers;

	for (i = 0; i < num_mixers; i++) {
		hw_lm = crtc->mixers[i].hw_lm;
		hw_dspp = crtc->mixers[i].hw_dspp;
		if (!hw_lm->cfg.right_mixer)
			break;
	}

	if (!hw_dspp) {
		DRM_ERROR("invalid dspp\n");
		ret = -EPERM;
		goto exit;
	}

	irq_idx = sde_core_irq_idx_lookup(kms, SDE_IRQ_TYPE_HIST_DSPP_DONE,
			hw_dspp->idx);
	if (irq_idx < 0) {
		DRM_ERROR("failed to get the irq idx ret %d\n", irq_idx);
		ret = irq_idx;
		goto exit;
	}

	node = container_of(hist_irq, struct sde_crtc_irq_info, irq);

	/* deregister histogram irq */
	if (!en) {
		spin_lock_irqsave(&node->state_lock, flags);
		if (node->state == IRQ_ENABLED) {
			node->state = IRQ_DISABLING;
			spin_unlock_irqrestore(&node->state_lock, flags);
			ret = sde_core_irq_disable(kms, &irq_idx, 1);
			spin_lock_irqsave(&node->state_lock, flags);
			if (ret) {
				DRM_ERROR("disable irq %d error %d\n",
					irq_idx, ret);
				node->state = IRQ_ENABLED;
			} else {
				node->state = IRQ_NOINIT;
			}
			spin_unlock_irqrestore(&node->state_lock, flags);
		} else if (node->state == IRQ_DISABLED) {
			node->state = IRQ_NOINIT;
			spin_unlock_irqrestore(&node->state_lock, flags);
		} else {
			spin_unlock_irqrestore(&node->state_lock, flags);
		}

		sde_core_irq_unregister_callback(kms, irq_idx, hist_irq);
		goto exit;
	}

	/* register histogram irq */
	hist_irq->arg = crtc;
	hist_irq->func = sde_cp_hist_interrupt_cb;
	ret = sde_core_irq_register_callback(kms, irq_idx, hist_irq);
	if (ret) {
		DRM_ERROR("failed to register the callback ret %d\n", ret);
		goto exit;
	}

	spin_lock_irqsave(&node->state_lock, flags);
	if (node->state == IRQ_DISABLED || node->state == IRQ_NOINIT) {
		ret = sde_core_irq_enable(kms, &irq_idx, 1);
		if (ret) {
			DRM_ERROR("enable irq %d error %d\n", irq_idx, ret);
			sde_core_irq_unregister_callback(kms,
				irq_idx, hist_irq);
		} else {
			node->state = IRQ_ENABLED;
		}
	}
	spin_unlock_irqrestore(&node->state_lock, flags);

exit:
	return ret;
}

/* needs to be called within ltm_buffer_lock mutex */
static void _sde_cp_crtc_free_ltm_buffer(struct sde_crtc *sde_crtc, void *cfg)
{
	u32 i = 0, buffer_count = 0;
	unsigned long irq_flags;

	if (!sde_crtc) {
		DRM_ERROR("invalid parameters sde_crtc %pK\n", sde_crtc);
		return;
	}

	spin_lock_irqsave(&sde_crtc->ltm_lock, irq_flags);
	if (sde_crtc->ltm_hist_en) {
		spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
		DRM_ERROR("cannot free LTM buffers when hist is enabled\n");
		return;
	}
	if (!sde_crtc->ltm_buffer_cnt) {
		/* ltm_buffers are already freed */
		spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
		return;
	}
	if (!list_empty(&sde_crtc->ltm_buf_busy)) {
		spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
		DRM_ERROR("ltm_buf_busy is not empty\n");
		return;
	}

	buffer_count = sde_crtc->ltm_buffer_cnt;
	sde_crtc->ltm_buffer_cnt = 0;
	INIT_LIST_HEAD(&sde_crtc->ltm_buf_free);
	INIT_LIST_HEAD(&sde_crtc->ltm_buf_busy);
	spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);

	for (i = 0; i < buffer_count && sde_crtc->ltm_buffers[i]; i++) {
		msm_gem_put_vaddr(sde_crtc->ltm_buffers[i]->gem);
		drm_framebuffer_put(sde_crtc->ltm_buffers[i]->fb);
		msm_gem_put_iova(sde_crtc->ltm_buffers[i]->gem,
			sde_crtc->ltm_buffers[i]->aspace);
		kfree(sde_crtc->ltm_buffers[i]);
		sde_crtc->ltm_buffers[i] = NULL;
	}
}

/* needs to be called within ltm_buffer_lock mutex */
static void _sde_cp_crtc_set_ltm_buffer(struct sde_crtc *sde_crtc, void *cfg)
{
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct drm_msm_ltm_buffers_ctrl *buf_cfg;
	struct drm_framebuffer *fb;
	struct drm_crtc *crtc;
	u32 size = 0, expected_size = 0;
	u32 i = 0, j = 0, num = 0, iova_aligned;
	int ret = 0;
	unsigned long irq_flags;

	if (!sde_crtc || !cfg) {
		DRM_ERROR("invalid parameters sde_crtc %pK cfg %pK\n", sde_crtc,
				cfg);
		return;
	}

	crtc = &sde_crtc->base;
	if (!crtc) {
		DRM_ERROR("invalid parameters drm_crtc %pK\n", crtc);
		return;
	}

	buf_cfg = hw_cfg->payload;
	num = buf_cfg->num_of_buffers;
	if (num == 0 || num > LTM_BUFFER_SIZE) {
		DRM_ERROR("invalid buffer size %d\n", num);
		return;
	}

	spin_lock_irqsave(&sde_crtc->ltm_lock, irq_flags);
	if (sde_crtc->ltm_buffer_cnt) {
		spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
		DRM_DEBUG("%d ltm_buffers already allocated\n",
			sde_crtc->ltm_buffer_cnt);
		return;
	}
	spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);

	expected_size = sizeof(struct drm_msm_ltm_stats_data) + LTM_GUARD_BYTES;
	for (i = 0; i < num; i++) {
		sde_crtc->ltm_buffers[i] = kzalloc(
			sizeof(struct sde_ltm_buffer), GFP_KERNEL);
		if (IS_ERR_OR_NULL(sde_crtc->ltm_buffers[i]))
			goto exit;

		sde_crtc->ltm_buffers[i]->drm_fb_id = buf_cfg->fds[i];
		fb = drm_framebuffer_lookup(crtc->dev, NULL, buf_cfg->fds[i]);
		if (!fb) {
			DRM_ERROR("unknown framebuffer ID %d\n",
					buf_cfg->fds[i]);
			goto exit;
		}

		sde_crtc->ltm_buffers[i]->fb = fb;
		sde_crtc->ltm_buffers[i]->gem = msm_framebuffer_bo(fb, 0);
		if (!sde_crtc->ltm_buffers[i]->gem) {
			DRM_ERROR("failed to get gem object\n");
			goto exit;
		}

		size = PAGE_ALIGN(sde_crtc->ltm_buffers[i]->gem->size);
		if (size < expected_size) {
			DRM_ERROR("Invalid buffer size\n");
			goto exit;
		}

		sde_crtc->ltm_buffers[i]->aspace =
			msm_gem_smmu_address_space_get(crtc->dev,
			MSM_SMMU_DOMAIN_UNSECURE);

		if (PTR_ERR(sde_crtc->ltm_buffers[i]->aspace) == -ENODEV) {
			sde_crtc->ltm_buffers[i]->aspace = NULL;
			DRM_DEBUG("IOMMU not present, relying on VRAM\n");
		} else if (IS_ERR_OR_NULL(sde_crtc->ltm_buffers[i]->aspace)) {
			ret = PTR_ERR(sde_crtc->ltm_buffers[i]->aspace);
			sde_crtc->ltm_buffers[i]->aspace = NULL;
			DRM_ERROR("failed to get aspace\n");
			goto exit;
		}
		ret = msm_gem_get_iova(sde_crtc->ltm_buffers[i]->gem,
				       sde_crtc->ltm_buffers[i]->aspace,
				       &sde_crtc->ltm_buffers[i]->iova);
		if (ret) {
			DRM_ERROR("failed to get the iova ret %d\n", ret);
			goto exit;
		}

		sde_crtc->ltm_buffers[i]->kva = msm_gem_get_vaddr(
			sde_crtc->ltm_buffers[i]->gem);
		if (IS_ERR_OR_NULL(sde_crtc->ltm_buffers[i]->kva)) {
			DRM_ERROR("failed to get kva\n");
			goto exit;
		}
		iova_aligned = (sde_crtc->ltm_buffers[i]->iova +
				LTM_GUARD_BYTES) & ALIGNED_OFFSET;
		sde_crtc->ltm_buffers[i]->offset = iova_aligned -
			sde_crtc->ltm_buffers[i]->iova;
	}
	spin_lock_irqsave(&sde_crtc->ltm_lock, irq_flags);
	/* Add buffers to ltm_buf_free list */
	for (i = 0; i < num; i++)
		list_add(&sde_crtc->ltm_buffers[i]->node,
			&sde_crtc->ltm_buf_free);
	sde_crtc->ltm_buffer_cnt = num;
	spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);

	return;
exit:
	for (j = 0; j < i; j++) {
		if (sde_crtc->ltm_buffers[i]->aspace)
			msm_gem_put_iova(sde_crtc->ltm_buffers[i]->gem,
				sde_crtc->ltm_buffers[i]->aspace);
		if (sde_crtc->ltm_buffers[i]->gem)
			msm_gem_put_vaddr(sde_crtc->ltm_buffers[i]->gem);
		if (sde_crtc->ltm_buffers[i]->fb)
			drm_framebuffer_put(sde_crtc->ltm_buffers[i]->fb);
		kfree(sde_crtc->ltm_buffers[i]);
		sde_crtc->ltm_buffers[i] = NULL;
	}
}

/* needs to be called within ltm_buffer_lock mutex */
static void _sde_cp_crtc_queue_ltm_buffer(struct sde_crtc *sde_crtc, void *cfg)
{
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct drm_msm_ltm_buffer *buf;
	struct drm_msm_ltm_stats_data *ltm_data = NULL;
	struct sde_ltm_buffer *free_buf;
	u32 i;
	bool found = false, already = false;
	unsigned long irq_flags;
	struct sde_ltm_buffer *buffer = NULL, *n = NULL;
	u64 addr = 0;
	bool submit_buf = false;
	uint32_t num_mixers = 0;
	struct sde_hw_dspp *hw_dspp = NULL;

	if (!sde_crtc || !cfg) {
		DRM_ERROR("invalid parameters sde_crtc %pK cfg %pK\n", sde_crtc,
				cfg);
		return;
	}

	buf = hw_cfg->payload;
	if (!buf) {
		DRM_ERROR("invalid parameters payload %pK\n", buf);
		return;
	}
	num_mixers = sde_crtc->num_mixers;

	spin_lock_irqsave(&sde_crtc->ltm_lock, irq_flags);
	if (!sde_crtc->ltm_buffer_cnt) {
		spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
		DRM_ERROR("LTM buffers are not allocated\n");
		return;
	}

	if (list_empty(&sde_crtc->ltm_buf_free))
		submit_buf = true;
	for (i = 0; i < LTM_BUFFER_SIZE; i++) {
		if (sde_crtc->ltm_buffers[i] && buf->fd ==
				sde_crtc->ltm_buffers[i]->drm_fb_id) {
			/* clear the status flag */
			ltm_data = (struct drm_msm_ltm_stats_data *)
				((u8 *)sde_crtc->ltm_buffers[i]->kva +
				 sde_crtc->ltm_buffers[i]->offset);
			ltm_data->status_flag = 0;

			list_for_each_entry_safe(buffer, n,
					&sde_crtc->ltm_buf_free, node) {
				if (buffer->drm_fb_id == buf->fd)
					already =  true;
			}
			if (!already)
				list_add_tail(&sde_crtc->ltm_buffers[i]->node,
					&sde_crtc->ltm_buf_free);
			found = true;
		}
	}
	if (submit_buf && found) {
		free_buf = list_first_entry(&sde_crtc->ltm_buf_free,
				struct sde_ltm_buffer, node);
		addr = free_buf->iova + free_buf->offset;

		for (i = 0; i < num_mixers; i++) {
			hw_dspp = sde_crtc->mixers[i].hw_dspp;
			if (!hw_dspp) {
				DRM_ERROR("invalid dspp for mixer %d\n", i);
				break;
			}
			hw_dspp->ops.setup_ltm_hist_buffer(hw_dspp, addr);
		}
	}
	spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);

	if (!found)
		DRM_ERROR("failed to found a matching buffer fd %d", buf->fd);
}

/* this func needs to be called within the ltm_buffer_lock and ltm_lock */
static int _sde_cp_crtc_get_ltm_buffer(struct sde_crtc *sde_crtc, u64 *addr)
{
	struct sde_ltm_buffer *buf;

	if (!sde_crtc || !addr) {
		DRM_ERROR("invalid parameters sde_crtc %pK cfg %pK\n",
				sde_crtc, addr);
		return -EINVAL;
	}

	/**
	 * for LTM merge mode, both LTM blocks will use the same buffer for
	 * hist collection. The first LTM will acquire a buffer from buf_free
	 * list and move that buffer to buf_busy list; the second LTM block
	 * will get the same buffer from busy list for HW programming
	 */
	if (!list_empty(&sde_crtc->ltm_buf_busy)) {
		buf = list_first_entry(&sde_crtc->ltm_buf_busy,
			struct sde_ltm_buffer, node);
		*addr = buf->iova + buf->offset;
		DRM_DEBUG_DRIVER("ltm_buf_busy list already has a buffer\n");
		return 0;
	}

	buf = list_first_entry(&sde_crtc->ltm_buf_free, struct sde_ltm_buffer,
				node);

	*addr = buf->iova + buf->offset;
	list_del_init(&buf->node);
	list_add_tail(&buf->node, &sde_crtc->ltm_buf_busy);

	return 0;
}

/* this func needs to be called within the ltm_buffer_lock mutex */
static void _sde_cp_crtc_enable_ltm_hist(struct sde_crtc *sde_crtc,
	struct sde_hw_dspp *hw_dspp, struct sde_hw_cp_cfg *hw_cfg)
{
	int ret = 0;
	u64 addr = 0;
	unsigned long irq_flags;
	struct sde_hw_mixer *hw_lm = hw_cfg->mixer_info;

	spin_lock_irqsave(&sde_crtc->ltm_lock, irq_flags);
	if (!sde_crtc->ltm_buffer_cnt) {
		spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
		DRM_ERROR("LTM buffers are not allocated\n");
		return;
	}

	if (!hw_lm->cfg.right_mixer && sde_crtc->ltm_hist_en) {
		/* histogram is already enabled */
		spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
		return;
	}

	ret = _sde_cp_crtc_get_ltm_buffer(sde_crtc, &addr);
	if (!ret) {
		if (!hw_lm->cfg.right_mixer)
			sde_crtc->ltm_hist_en = true;
		hw_dspp->ops.setup_ltm_hist_ctrl(hw_dspp, hw_cfg,
			true, addr);
		SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY);
	}
	spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
}

/* this func needs to be called within the ltm_buffer_lock mutex */
static void _sde_cp_crtc_disable_ltm_hist(struct sde_crtc *sde_crtc,
	struct sde_hw_dspp *hw_dspp, struct sde_hw_cp_cfg *hw_cfg)
{
	unsigned long irq_flags;
	u32 i = 0;
	u8 hist_off = 1;
	struct drm_event event;

	spin_lock_irqsave(&sde_crtc->ltm_lock, irq_flags);
	sde_crtc->ltm_hist_en = false;
	sde_crtc->ltm_merge_clear_pending = true;
	SDE_EVT32(DRMID(&sde_crtc->base), sde_crtc->ltm_merge_clear_pending);
	INIT_LIST_HEAD(&sde_crtc->ltm_buf_free);
	INIT_LIST_HEAD(&sde_crtc->ltm_buf_busy);
	for (i = 0; i < sde_crtc->ltm_buffer_cnt; i++)
		list_add(&sde_crtc->ltm_buffers[i]->node,
			&sde_crtc->ltm_buf_free);
	hw_dspp->ops.setup_ltm_hist_ctrl(hw_dspp, NULL,
			false, 0);
	spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
	event.type = DRM_EVENT_LTM_OFF;
	event.length = sizeof(hist_off);
	SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY);
	msm_mode_object_event_notify(&sde_crtc->base.base,
			sde_crtc->base.dev, &event,
			(u8 *)&hist_off);
}

static void sde_cp_ltm_hist_interrupt_cb(void *arg, int irq_idx)
{
	struct sde_crtc *sde_crtc = arg;
	struct sde_ltm_buffer *busy_buf, *free_buf;
	struct sde_hw_dspp *hw_dspp = NULL;
	struct drm_msm_ltm_stats_data *ltm_data = NULL;
	u32 num_mixers = 0, i = 0, status = 0, ltm_hist_status = 0;
	u64 addr = 0;
	int idx = -1;
	unsigned long irq_flags;
	struct sde_ltm_phase_info phase;
	struct sde_hw_cp_cfg hw_cfg;
	struct sde_hw_mixer *hw_lm;

	if (!sde_crtc) {
		DRM_ERROR("invalid sde_crtc %pK\n", sde_crtc);
		return;
	}

	memset(&phase, 0, sizeof(phase));

	/* read intr_status register value */
	num_mixers = sde_crtc->num_mixers;
	if (!num_mixers)
		return;

	for (i = 0; i < num_mixers; i++) {
		hw_dspp = sde_crtc->mixers[i].hw_dspp;
		if (!hw_dspp) {
			DRM_ERROR("invalid dspp for mixer %d\n", i);
			return;
		}
		hw_dspp->ops.ltm_read_intr_status(hw_dspp, &status);
		if (status & LTM_STATS_SAT)
			ltm_hist_status |= LTM_STATS_SAT;
		if (status & LTM_STATS_MERGE_SAT)
			ltm_hist_status |= LTM_STATS_MERGE_SAT;
	}

	spin_lock_irqsave(&sde_crtc->ltm_lock, irq_flags);
	if (!sde_crtc->ltm_buffer_cnt) {
		spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
		/* all LTM buffers are freed, no further action is needed */
		return;
	}

	if (!sde_crtc->ltm_hist_en) {
		/* histogram is disabled, no need to notify user space */
		for (i = 0; i < sde_crtc->num_mixers; i++) {
			hw_dspp = sde_crtc->mixers[i].hw_dspp;
			if (!hw_dspp || i >= DSPP_MAX)
				continue;
			hw_dspp->ops.setup_ltm_hist_ctrl(hw_dspp, NULL, false,
				0);
		}
		sde_crtc->ltm_merge_clear_pending = true;
		SDE_EVT32(DRMID(&sde_crtc->base), sde_crtc->ltm_merge_clear_pending);
		spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
		DRM_DEBUG_DRIVER("LTM histogram is disabled\n");
		return;
	}

	/* if no free buffer available, the same buffer is used by HW */
	if (list_empty(&sde_crtc->ltm_buf_free)) {
		spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
		DRM_DEBUG_DRIVER("no free buffer available\n");
		return;
	}

	busy_buf = list_first_entry(&sde_crtc->ltm_buf_busy,
			struct sde_ltm_buffer, node);
	free_buf = list_first_entry(&sde_crtc->ltm_buf_free,
			struct sde_ltm_buffer, node);

	/* find the index of buffer in the ltm_buffers */
	for (i = 0; i < sde_crtc->ltm_buffer_cnt; i++) {
		if (busy_buf->drm_fb_id == sde_crtc->ltm_buffers[i]->drm_fb_id)
			idx = i;
	}
	if (idx < 0) {
		spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
		DRM_ERROR("failed to found the buffer in the list fb_id %d\n",
				busy_buf->drm_fb_id);
		return;
	}

	addr = free_buf->iova + free_buf->offset;
	for (i = 0; i < num_mixers; i++) {
		hw_dspp = sde_crtc->mixers[i].hw_dspp;
		if (!hw_dspp) {
			spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
			DRM_ERROR("invalid dspp for mixer %d\n", i);
			return;
		}
		hw_dspp->ops.setup_ltm_hist_buffer(hw_dspp, addr);
	}

	list_del_init(&busy_buf->node);
	list_del_init(&free_buf->node);
	INIT_LIST_HEAD(&sde_crtc->ltm_buf_busy);
	list_add_tail(&free_buf->node, &sde_crtc->ltm_buf_busy);

	ltm_data = (struct drm_msm_ltm_stats_data *)
		((u8 *)sde_crtc->ltm_buffers[idx]->kva +
		sde_crtc->ltm_buffers[idx]->offset);

	hw_dspp = sde_crtc->mixers[0].hw_dspp;
	if (!hw_dspp) {
		spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
		DRM_ERROR("invalid dspp for mixer %d\n", i);
		return;
	}
	if (hw_dspp->ltm_checksum_support) {
		ltm_data->feature_flag |= LTM_HIST_CHECKSUM_SUPPORT;
		ltm_data->checksum = ltm_data->status_flag;
	} else
		ltm_data->feature_flag &= ~LTM_HIST_CHECKSUM_SUPPORT;

	ltm_data->status_flag = ltm_hist_status;

	hw_lm = sde_crtc->mixers[0].hw_lm;
	if (!hw_lm) {
		spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
		DRM_ERROR("invalid layer mixer\n");
		return;
	}
	hw_cfg.num_of_mixers = num_mixers;
	hw_cfg.displayh = num_mixers * hw_lm->cfg.out_width;
	hw_cfg.displayv = hw_lm->cfg.out_height;

	sde_ltm_get_phase_info(&hw_cfg, &phase);
	ltm_data->display_h = hw_cfg.displayh;
	ltm_data->display_v = hw_cfg.displayv;
	ltm_data->init_h[0] = phase.init_h[LTM_0];
	ltm_data->init_h[1] = phase.init_h[LTM_1];
	ltm_data->init_v = phase.init_v;
	ltm_data->inc_v = phase.inc_v;
	ltm_data->inc_h = phase.inc_h;
	ltm_data->portrait_en = phase.portrait_en;
	ltm_data->merge_en = phase.merge_en;
	ltm_data->cfg_param_01 = sde_crtc->ltm_cfg.cfg_param_01;
	ltm_data->cfg_param_02 = sde_crtc->ltm_cfg.cfg_param_02;
	ltm_data->cfg_param_03 = sde_crtc->ltm_cfg.cfg_param_03;
	ltm_data->cfg_param_04 = sde_crtc->ltm_cfg.cfg_param_04;
	sde_crtc_event_queue(&sde_crtc->base, sde_cp_notify_ltm_hist,
				sde_crtc->ltm_buffers[idx], true);
	spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
}

static void sde_cp_ltm_wb_pb_interrupt_cb(void *arg, int irq_idx)
{
	struct sde_crtc *sde_crtc = arg;

	sde_crtc_event_queue(&sde_crtc->base, sde_cp_notify_ltm_wb_pb, NULL,
				true);
}

static void sde_cp_notify_ltm_hist(struct drm_crtc *crtc, void *arg)
{
	struct drm_event event;
	struct drm_msm_ltm_buffer payload = {};
	struct sde_ltm_buffer *buf;
	struct sde_crtc *sde_crtc;
	unsigned long irq_flags;

	if (!crtc || !arg) {
		DRM_ERROR("invalid drm_crtc %pK or arg %pK\n", crtc, arg);
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
	if (!sde_crtc) {
		DRM_ERROR("invalid sde_crtc %pK\n", sde_crtc);
		return;
	}

	mutex_lock(&sde_crtc->ltm_buffer_lock);
	spin_lock_irqsave(&sde_crtc->ltm_lock, irq_flags);
	if (!sde_crtc->ltm_buffer_cnt) {
		spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
		mutex_unlock(&sde_crtc->ltm_buffer_lock);
		/* all LTM buffers are freed, no further action is needed */
		return;
	}

	if (!sde_crtc->ltm_hist_en) {
		/* histogram is disabled, no need to notify user space */
		spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
		mutex_unlock(&sde_crtc->ltm_buffer_lock);
		DRM_DEBUG_DRIVER("ltm histogram is disabled\n");
		return;
	}

	buf = (struct sde_ltm_buffer *)arg;
	payload.fd = buf->drm_fb_id;
	payload.offset = buf->offset;
	event.length = sizeof(struct drm_msm_ltm_buffer);
	event.type = DRM_EVENT_LTM_HIST;
	DRM_DEBUG_DRIVER("notify with LTM hist event drm_fb_id %d\n",
				buf->drm_fb_id);
	msm_mode_object_event_notify(&crtc->base, crtc->dev, &event,
					(u8 *)&payload);
	spin_unlock_irqrestore(&sde_crtc->ltm_lock, irq_flags);
	mutex_unlock(&sde_crtc->ltm_buffer_lock);
}

static void sde_cp_notify_ltm_wb_pb(struct drm_crtc *crtc, void *arg)
{
	struct drm_event event;
	struct drm_msm_ltm_buffer payload = {};

	if (!crtc) {
		DRM_ERROR("invalid drm_crtc %pK\n", crtc);
		return;
	}

	payload.fd = 0;
	payload.offset = 0;
	event.length = sizeof(struct drm_msm_ltm_buffer);
	event.type = DRM_EVENT_LTM_WB_PB;
	msm_mode_object_event_notify(&crtc->base, crtc->dev, &event,
					(u8 *)&payload);
}

static int sde_cp_ltm_register_irq(struct sde_kms *kms,
		struct sde_crtc *sde_crtc, struct sde_hw_dspp *hw_dspp,
		struct sde_irq_callback *ltm_irq, enum sde_intr_type irq)
{
	int irq_idx, ret = 0;

	if (irq == SDE_IRQ_TYPE_LTM_STATS_DONE) {
		ltm_irq->func = sde_cp_ltm_hist_interrupt_cb;
	} else if (irq == SDE_IRQ_TYPE_LTM_STATS_WB_PB) {
		ltm_irq->func = sde_cp_ltm_wb_pb_interrupt_cb;
	} else {
		DRM_ERROR("invalid irq type %d\n", irq);
		return -EINVAL;
	}

	irq_idx = sde_core_irq_idx_lookup(kms, irq, hw_dspp->idx);
	if (irq_idx < 0) {
		DRM_ERROR("failed to get the irq idx %d\n", irq_idx);
		return irq_idx;
	}

	ltm_irq->arg = sde_crtc;
	ret = sde_core_irq_register_callback(kms, irq_idx, ltm_irq);
	if (ret) {
		DRM_ERROR("failed to register the callback ret %d\n", ret);
		return ret;
	}

	ret = sde_core_irq_enable(kms, &irq_idx, 1);
	if (ret) {
		DRM_ERROR("enable irq %d error %d\n", irq_idx, ret);
		sde_core_irq_unregister_callback(kms, irq_idx, ltm_irq);
	}

	return ret;
}

static int sde_cp_ltm_unregister_irq(struct sde_kms *kms,
		struct sde_crtc *sde_crtc, struct sde_hw_dspp *hw_dspp,
		struct sde_irq_callback *ltm_irq, enum sde_intr_type irq)
{
	int irq_idx, ret = 0;

	if (!(irq == SDE_IRQ_TYPE_LTM_STATS_DONE ||
		irq == SDE_IRQ_TYPE_LTM_STATS_WB_PB)) {
		DRM_ERROR("invalid irq type %d\n", irq);
		return -EINVAL;
	}

	irq_idx = sde_core_irq_idx_lookup(kms, irq, hw_dspp->idx);
	if (irq_idx < 0) {
		DRM_ERROR("failed to get the irq idx %d\n", irq_idx);
		return irq_idx;
	}

	ret = sde_core_irq_disable(kms, &irq_idx, 1);
	if (ret)
		DRM_ERROR("disable irq %d error %d\n", irq_idx, ret);

	sde_core_irq_unregister_callback(kms, irq_idx, ltm_irq);
	return ret;
}

int sde_cp_ltm_hist_interrupt(struct drm_crtc *crtc, bool en,
				struct sde_irq_callback *ltm_irq)
{
	struct sde_kms *kms = NULL;
	struct sde_hw_dspp *hw_dspp = NULL;
	struct sde_crtc *sde_crtc;
	int ret = 0;

	if (!crtc || !ltm_irq) {
		DRM_ERROR("invalid params: crtc %pK irq %pK\n", crtc, ltm_irq);
		return -EINVAL;
	}

	kms = get_kms(crtc);
	sde_crtc = to_sde_crtc(crtc);
	if (!kms || !sde_crtc) {
		DRM_ERROR("invalid params: kms %pK sde_crtc %pK\n", kms,
				sde_crtc);
		return -EINVAL;
	}

	/* enable interrupt on master LTM block */
	hw_dspp = sde_crtc->mixers[0].hw_dspp;
	if (!hw_dspp) {
		DRM_ERROR("invalid dspp\n");
		return -ENODEV;
	}

	if (en) {
		ret = sde_cp_ltm_register_irq(kms, sde_crtc, hw_dspp,
				ltm_irq, SDE_IRQ_TYPE_LTM_STATS_DONE);
		if (ret)
			DRM_ERROR("failed to register stats_done irq\n");
	} else {
		ret = sde_cp_ltm_unregister_irq(kms, sde_crtc, hw_dspp,
				ltm_irq, SDE_IRQ_TYPE_LTM_STATS_DONE);
		if (ret)
			DRM_ERROR("failed to unregister stats_done irq\n");
	}
	return ret;
}

int sde_cp_ltm_wb_pb_interrupt(struct drm_crtc *crtc, bool en,
				struct sde_irq_callback *ltm_irq)
{
	struct sde_kms *kms = NULL;
	struct sde_hw_dspp *hw_dspp = NULL;
	struct sde_crtc *sde_crtc;
	int ret = 0;

	if (!crtc || !ltm_irq) {
		DRM_ERROR("invalid params: crtc %pK irq %pK\n", crtc, ltm_irq);
		return -EINVAL;
	}

	kms = get_kms(crtc);
	sde_crtc = to_sde_crtc(crtc);
	if (!kms || !sde_crtc) {
		DRM_ERROR("invalid params: kms %pK sde_crtc %pK\n", kms,
				sde_crtc);
		return -EINVAL;
	}

	/* enable interrupt on master LTM block */
	hw_dspp = sde_crtc->mixers[0].hw_dspp;
	if (!hw_dspp) {
		DRM_ERROR("invalid dspp\n");
		return -ENODEV;
	}

	if (en) {
		ret = sde_cp_ltm_register_irq(kms, sde_crtc, hw_dspp,
				ltm_irq, SDE_IRQ_TYPE_LTM_STATS_WB_PB);
		if (ret)
			DRM_ERROR("failed to register WB_PB irq\n");
	} else {
		ret = sde_cp_ltm_unregister_irq(kms, sde_crtc, hw_dspp,
				ltm_irq, SDE_IRQ_TYPE_LTM_STATS_WB_PB);
		if (ret)
			DRM_ERROR("failed to unregister WB_PB irq\n");
	}
	return ret;
}

static void _sde_cp_crtc_update_ltm_roi(struct sde_crtc *sde_crtc,
		struct sde_hw_cp_cfg *hw_cfg)
{
	struct drm_msm_ltm_cfg_param *cfg_param = NULL;

	/* disable case */
	if (!hw_cfg->payload) {
		memset(&sde_crtc->ltm_cfg, 0,
			sizeof(struct drm_msm_ltm_cfg_param));
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_ltm_cfg_param)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
			hw_cfg->len, sizeof(struct drm_msm_ltm_cfg_param));
		return;
	}

	cfg_param = hw_cfg->payload;
	/* input param exceeds the display width */
	if (cfg_param->cfg_param_01 + cfg_param->cfg_param_03 >
			hw_cfg->displayh) {
		DRM_DEBUG_DRIVER("invalid input = [%u,%u], displayh = %u\n",
			cfg_param->cfg_param_01, cfg_param->cfg_param_03,
			hw_cfg->displayh);
		/* set the roi width to max register value */
		cfg_param->cfg_param_03 = 0xFFFF;
	}

	/* input param exceeds the display height */
	if (cfg_param->cfg_param_02 + cfg_param->cfg_param_04 >
			hw_cfg->displayv) {
		DRM_DEBUG_DRIVER("invalid input = [%u,%u], displayv = %u\n",
			cfg_param->cfg_param_02, cfg_param->cfg_param_04,
			hw_cfg->displayv);
		/* set the roi height to max register value */
		cfg_param->cfg_param_04 = 0xFFFF;
	}

	sde_crtc->ltm_cfg = *cfg_param;
}

int sde_cp_ltm_off_event_handler(struct drm_crtc *crtc_drm, bool en,
	struct sde_irq_callback *hist_irq)
{
	return 0;
}

void sde_cp_crtc_res_change(struct drm_crtc *crtc_drm)
{
	struct sde_cp_node *prop_node = NULL, *n = NULL;
	struct sde_crtc *crtc;

	if (!crtc_drm) {
		DRM_ERROR("invalid crtc handle");
		return;
	}
	crtc = to_sde_crtc(crtc_drm);
	mutex_lock(&crtc->crtc_cp_lock);
	list_for_each_entry_safe(prop_node, n, &crtc->active_list,
				 active_list) {
		if (prop_node->feature == SDE_CP_CRTC_DSPP_LTM_INIT ||
			prop_node->feature == SDE_CP_CRTC_DSPP_LTM_VLUT) {
			list_del_init(&prop_node->active_list);
			list_add_tail(&prop_node->dirty_list,
				&crtc->dirty_list);
		}
	}
	mutex_unlock(&crtc->crtc_cp_lock);
}

/* this func needs to be called within crtc_cp_lock mutex */
static bool _sde_cp_feature_in_dirtylist(u32 feature, struct list_head *list)
{
	struct sde_cp_node *node = NULL;

	list_for_each_entry(node, list, dirty_list) {
		if (feature == node->feature)
			return true;
	}

	return false;
}

/* this func needs to be called within crtc_cp_lock mutex */
static bool _sde_cp_feature_in_activelist(u32 feature, struct list_head *list)
{
	struct sde_cp_node *node = NULL;

	list_for_each_entry(node, list, active_list) {
		if (feature == node->feature)
			return true;
	}

	return false;
}

void sde_cp_crtc_vm_primary_handoff(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = NULL;
	struct sde_cp_node *prop_node = NULL;

	if (!crtc) {
		DRM_ERROR("crtc %pK\n", crtc);
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
	if (!sde_crtc) {
		DRM_ERROR("sde_crtc %pK\n", sde_crtc);
		return;
	}

	mutex_lock(&sde_crtc->crtc_cp_lock);

	list_for_each_entry(prop_node, &sde_crtc->feature_list, feature_list) {
		if (!feature_handoff_mask[prop_node->feature])
			continue;

		if (_sde_cp_feature_in_dirtylist(prop_node->feature,
						 &sde_crtc->dirty_list))
			continue;

		if (_sde_cp_feature_in_activelist(prop_node->feature,
						 &sde_crtc->active_list)) {
			sde_cp_update_list(prop_node, sde_crtc, true);
			list_del_init(&prop_node->active_list);
			continue;
		}

		sde_cp_update_list(prop_node, sde_crtc, true);
	}

	mutex_unlock(&sde_crtc->crtc_cp_lock);
}

static void dspp_idx_caps_update(struct sde_crtc *crtc,
	struct sde_kms_info *info)
{
	struct sde_mdss_cfg *catalog = get_kms(&crtc->base)->catalog;
	u32 i, num_mixers = crtc->num_mixers;
	char blk_name[256];

	if (!catalog->dspp_count || num_mixers > catalog->dspp_count)
		return;

	for (i = 0; i < num_mixers; i++) {
		struct sde_hw_dspp *dspp = crtc->mixers[i].hw_dspp;

		if (!dspp)
			continue;
		snprintf(blk_name, sizeof(blk_name), "dspp%u",
				(dspp->idx - DSPP_0));
		sde_kms_info_add_keyint(info, blk_name, 1);
	}
}

static void rc_caps_update(struct sde_crtc *crtc, struct sde_kms_info *info)
{
	struct sde_mdss_cfg *catalog = get_kms(&crtc->base)->catalog;
	u32 i, num_mixers = crtc->num_mixers;
	char blk_name[256];

	if (!catalog->rc_count || num_mixers > catalog->rc_count)
		return;

	for (i = 0; i < num_mixers; i++) {
		struct sde_hw_dspp *dspp = crtc->mixers[i].hw_dspp;

		if (!dspp || (dspp->idx - DSPP_0) >= catalog->rc_count)
			continue;
		snprintf(blk_name, sizeof(blk_name), "rc%u",
				(dspp->idx - DSPP_0));
		sde_kms_info_add_keyint(info, blk_name, 1);
	}
}

static void demura_caps_update(struct sde_crtc *crtc,
	struct sde_kms_info *info)
{
	struct sde_mdss_cfg *catalog = get_kms(&crtc->base)->catalog;
	u32 i, num_mixers = crtc->num_mixers;
	char blk_name[256];

	if (!catalog->demura_count || num_mixers > catalog->demura_count)
		return;

	for (i = 0; i < num_mixers; i++) {
		struct sde_hw_dspp *dspp = crtc->mixers[i].hw_dspp;

		if (!dspp || (dspp->idx - DSPP_0) >= catalog->demura_count)
			continue;
		snprintf(blk_name, sizeof(blk_name), "demura%u",
			(dspp->idx - DSPP_0));
		sde_kms_info_add_keyint(info, blk_name, 1);
	}
}

static void spr_caps_update(struct sde_crtc *crtc, struct sde_kms_info *info)
{
	struct sde_mdss_cfg *catalog = get_kms(&crtc->base)->catalog;
	u32 i, num_mixers = crtc->num_mixers;
	char blk_name[256];

	if (!catalog->spr_count || num_mixers > catalog->spr_count)
		return;

	for (i = 0; i < num_mixers; i++) {
		struct sde_hw_dspp *dspp = crtc->mixers[i].hw_dspp;

		if (!dspp || (dspp->idx - DSPP_0) >= catalog->spr_count)
			continue;
		snprintf(blk_name, sizeof(blk_name), "spr%u",
			(dspp->idx - DSPP_0));
		sde_kms_info_add_keyint(info, blk_name, 1);
	}
}

static void ltm_caps_update(struct sde_crtc *crtc, struct sde_kms_info *info)
{
	struct sde_mdss_cfg *catalog = get_kms(&crtc->base)->catalog;
	u32 i, num_mixers = crtc->num_mixers;
	char blk_name[256];

	if (!catalog->ltm_count ||  num_mixers > catalog->ltm_count)
		return;

	for (i = 0; i < num_mixers; i++) {
		struct sde_hw_dspp *dspp = crtc->mixers[i].hw_dspp;

		if (!dspp || (dspp->idx - DSPP_0) >= catalog->ltm_count)
			continue;
		snprintf(blk_name, sizeof(blk_name), "ltm%u",
			(dspp->idx - DSPP_0));
		sde_kms_info_add_keyint(info, blk_name, 1);
	}
}

void sde_cp_crtc_enable(struct drm_crtc *drm_crtc)
{
	struct sde_crtc *crtc;
	struct sde_kms_info *info = NULL;
	u32 i, num_mixers;

	if (!drm_crtc) {
		DRM_ERROR("invalid crtc handle");
		return;
	}
	crtc = to_sde_crtc(drm_crtc);
	num_mixers = crtc->num_mixers;
	if (!num_mixers)
		return;
	mutex_lock(&crtc->crtc_cp_lock);
	info = kzalloc(sizeof(struct sde_kms_info), GFP_KERNEL);
	if (info) {
		for (i = 0; i < ARRAY_SIZE(dspp_cap_update_func); i++)
			dspp_cap_update_func[i](crtc, info);
		msm_property_set_blob(&crtc->property_info,
				&crtc->dspp_blob_info,
			info->data, SDE_KMS_INFO_DATALEN(info),
			CRTC_PROP_DSPP_INFO);
	}
	kfree(info);
	mutex_unlock(&crtc->crtc_cp_lock);
}

void sde_cp_crtc_disable(struct drm_crtc *drm_crtc)
{
	struct sde_kms_info *info = NULL;
	struct sde_crtc *crtc;

	if (!drm_crtc) {
		DRM_ERROR("invalid crtc handle");
		return;
	}
	crtc = to_sde_crtc(drm_crtc);
	mutex_lock(&crtc->crtc_cp_lock);
	info = kzalloc(sizeof(struct sde_kms_info), GFP_KERNEL);
	if (info)
		msm_property_set_blob(&crtc->property_info,
				&crtc->dspp_blob_info,
			info->data, SDE_KMS_INFO_DATALEN(info),
			CRTC_PROP_DSPP_INFO);
	mutex_unlock(&crtc->crtc_cp_lock);
	kfree(info);
}
