// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/dma-buf.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_vblank.h>

#include "sprd_drm.h"
#include "sprd_gem.h"
#include "sprd_crtc.h"
#include "sprd_plane.h"
#include "sprd_iommu.h"

int sprd_crtc_iommu_map(struct device *dev,
				struct sprd_gem_obj *sprd_gem)
{
	struct dma_buf *dma_buf;
	struct sprd_iommu_map_data iommu_data = {};

	if (!sprd_gem->base.import_attach)
		return 0;

	dma_buf = sprd_gem->base.import_attach->dmabuf;
	iommu_data.buf = dma_buf->priv;
	iommu_data.iova_size = dma_buf->size;
	iommu_data.ch_type = SPRD_IOMMU_FM_CH_RW;

	if (sprd_iommu_map(dev, &iommu_data)) {
		DRM_ERROR("failed to map iommu address\n");
		return -EINVAL;
	}

	sprd_gem->dma_addr = iommu_data.iova_addr;

	return 0;
}

void sprd_crtc_iommu_unmap(struct device *dev,
				struct sprd_gem_obj *sprd_gem)
{
	struct sprd_iommu_unmap_data iommu_data = {};

	if (!sprd_gem->base.import_attach)
		return;

	iommu_data.iova_size = sprd_gem->base.size;
	iommu_data.iova_addr = sprd_gem->dma_addr;
	iommu_data.ch_type = SPRD_IOMMU_FM_CH_RW;

	if (sprd_iommu_unmap(dev, &iommu_data))
		DRM_ERROR("failed to unmap iommu address\n");
}

void sprd_crtc_wait_last_commit_complete(struct drm_crtc *crtc)
{
	struct drm_crtc_commit *commit;
	int ret, i = 0;

	spin_lock(&crtc->commit_lock);
	list_for_each_entry(commit, &crtc->commit_list, commit_entry) {
		i++;
		/* skip the first entry, that's the current commit */
		if (i == 2)
			break;
	}
	if (i == 2)
		drm_crtc_commit_get(commit);
	spin_unlock(&crtc->commit_lock);

	if (i != 2)
		return;

	ret = wait_for_completion_interruptible_timeout(&commit->cleanup_done,
							HZ);
	if (ret == 0)
		DRM_WARN("wait last commit completion timed out\n");

	drm_crtc_commit_put(commit);
}

static void sprd_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct sprd_crtc *sprd_crtc = to_sprd_crtc(crtc);

	if (sprd_crtc->ops->mode_set_nofb)
		return sprd_crtc->ops->mode_set_nofb(sprd_crtc);
}

static enum drm_mode_status sprd_crtc_mode_valid(struct drm_crtc *crtc,
	const struct drm_display_mode *mode)
{
	struct sprd_crtc *sprd_crtc = to_sprd_crtc(crtc);

	if (sprd_crtc->ops->mode_valid)
		return sprd_crtc->ops->mode_valid(sprd_crtc, mode);

	return MODE_OK;
}

static int sprd_crtc_atomic_check(struct drm_crtc *crtc,
				     struct drm_atomic_state *state)
{
	struct sprd_crtc *sprd_crtc = to_sprd_crtc(crtc);
	/* state->crtcs->state ? old_state? new_state? */
	struct drm_crtc_state *crtc_state = state->crtcs->state;

	if (!crtc_state->enable)
		return 0;

	if (sprd_crtc->ops->atomic_check)
		return sprd_crtc->ops->atomic_check(sprd_crtc, crtc_state);

	return 0;
}

static void sprd_crtc_atomic_begin(struct drm_crtc *crtc,
				     struct drm_atomic_state *state)
{
	struct sprd_crtc *sprd_crtc = to_sprd_crtc(crtc);

	if (sprd_crtc->ops->atomic_begin)
		sprd_crtc->ops->atomic_begin(sprd_crtc);
}

static void sprd_crtc_atomic_enable(struct drm_crtc *crtc,
					struct drm_atomic_state *state)
{
	struct sprd_crtc *sprd_crtc = to_sprd_crtc(crtc);

	/*
	 * FIXME:
	 * Add if condition to avoid resume dpu for SR feature.
	 * Because of unisoc sr/vrr changed style, it's no need to suspend and resume panel.
	 * If there is no dpms state change, it's no need to resume display pipeline.
	 * Due to drm modeset check, we still should enable vblank in mode switching phase.
	 */
	if (crtc->state->mode_changed && !crtc->state->active_changed) {
		drm_crtc_vblank_on(crtc);
		return;
	}

	if (sprd_crtc->ops->atomic_enable)
		sprd_crtc->ops->atomic_enable(sprd_crtc);

	drm_crtc_vblank_on(crtc);
}

static void sprd_crtc_atomic_disable(struct drm_crtc *crtc,
					   struct drm_atomic_state *state)
{
	struct sprd_crtc *sprd_crtc = to_sprd_crtc(crtc);

	/*
	 * FIXME:
	 * Add if condition to avoid suspend dpu for SR feature.
	 * Because of unisoc sr/vrr changed style, it's no need to suspend and resume panel.
	 * If there is no dpms state change, it's no need to suspend display pipeline.
	 * Due to drm modeset check, we still should disable vblank in mode switching phase.
	 */
	if (crtc->state->mode_changed && !crtc->state->active_changed) {
		drm_crtc_vblank_off(crtc);
		return;
	}

	drm_crtc_vblank_off(crtc);

	if (sprd_crtc->ops->atomic_disable)
		sprd_crtc->ops->atomic_disable(sprd_crtc);

	if (crtc->state->event && !crtc->state->active) {
		spin_lock_irq(&crtc->dev->event_lock);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		spin_unlock_irq(&crtc->dev->event_lock);

		crtc->state->event = NULL;
	}
}

static void sprd_crtc_atomic_flush(struct drm_crtc *crtc,
				     struct drm_atomic_state *state)
{
	struct sprd_crtc *sprd_crtc = to_sprd_crtc(crtc);

	if (sprd_crtc->ops->atomic_flush)
		sprd_crtc->ops->atomic_flush(sprd_crtc);

	spin_lock_irq(&crtc->dev->event_lock);
	if (crtc->state->event) {
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		crtc->state->event = NULL;
	}
	spin_unlock_irq(&crtc->dev->event_lock);
}

static const struct drm_crtc_helper_funcs sprd_crtc_helper_funcs = {
	.mode_set_nofb	= sprd_crtc_mode_set_nofb,
	.mode_valid	= sprd_crtc_mode_valid,
	.atomic_check	= sprd_crtc_atomic_check,
	.atomic_begin	= sprd_crtc_atomic_begin,
	.atomic_enable	= sprd_crtc_atomic_enable,
	.atomic_disable	= sprd_crtc_atomic_disable,
	.atomic_flush	= sprd_crtc_atomic_flush,
};

static void sprd_crtc_cleanup(struct drm_crtc *crtc)
{
	struct sprd_crtc *sprd_crtc = to_sprd_crtc(crtc);

	drm_crtc_cleanup(crtc);
	kfree(sprd_crtc);
}

static int sprd_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct sprd_crtc *sprd_crtc = to_sprd_crtc(crtc);

	if (sprd_crtc->ops->enable_vblank)
		return sprd_crtc->ops->enable_vblank(sprd_crtc);

	return 0;
}

static void sprd_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct sprd_crtc *sprd_crtc = to_sprd_crtc(crtc);

	if (sprd_crtc->ops->disable_vblank)
		sprd_crtc->ops->disable_vblank(sprd_crtc);
}

static int sprd_crtc_atomic_get_property(struct drm_crtc *drm_crtc,
					const struct drm_crtc_state *crtc_state,
					struct drm_property *property,
					uint64_t *val)
{
	struct sprd_crtc *crtc = to_sprd_crtc(drm_crtc);
	struct sprd_crtc_state *state = to_sprd_crtc_state(crtc_state);

	DRM_DEBUG("%s() name = %s\n", __func__, property->name);

	if (property == crtc->resolution_property)
		*val = state->resolution_change;
	else if (property == crtc->frame_rate_property)
		*val = state->frame_rate_change;
	else {
		DRM_ERROR("property %s is invalid\n", property->name);
		return -EINVAL;
	}

	return 0;
}

static int sprd_crtc_atomic_set_property(struct drm_crtc *drm_crtc,
					struct drm_crtc_state *crtc_state,
					struct drm_property *property,
					uint64_t val)
{
	struct sprd_crtc *crtc = to_sprd_crtc(drm_crtc);
	struct sprd_crtc_state *state = to_sprd_crtc_state(crtc_state);

	DRM_DEBUG("%s() name = %s, val = %llu\n",
		  __func__, property->name, val);

	if (property == crtc->resolution_property) {
		state->resolution_change = val;
		crtc->sr_mode_changed = val;
	} else if (property == crtc->frame_rate_property) {
		state->frame_rate_change = val;
		crtc->fps_mode_changed = val;
	} else {
		DRM_ERROR("property %s is invalid\n", property->name);
		return -EINVAL;
	}

	return 0;
}

static struct drm_crtc_state *sprd_crtc_atomic_duplicate_state(struct drm_crtc *crtc)
{
	struct sprd_crtc_state *state;

	DRM_DEBUG("%s()\n", __func__);

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &state->base);

	WARN_ON(state->base.crtc != crtc);

	state->resolution_change = false;
	state->frame_rate_change = false;

	return &state->base;
}

static void sprd_crtc_atomic_destroy_state(struct drm_crtc *crtc,
					    struct drm_crtc_state *state)
{
	DRM_DEBUG("%s()\n", __func__);

	__drm_atomic_helper_crtc_destroy_state(state);
	kfree(to_sprd_crtc_state(state));
}

static void sprd_crtc_reset(struct drm_crtc *drm_crtc)
{
	struct sprd_crtc_state *state;

	DRM_INFO("%s()\n", __func__);

	if (drm_crtc->state) {
		__drm_atomic_helper_crtc_destroy_state(drm_crtc->state);

		state = to_sprd_crtc_state(drm_crtc->state);
		memset(state, 0, sizeof(*state));
	} else {
		state = kzalloc(sizeof(*state), GFP_KERNEL);
		if (!state)
			return;
		drm_crtc->state = &state->base;
	}

	state->base.crtc = drm_crtc;
}

static const struct drm_crtc_funcs sprd_crtc_funcs = {
	.destroy	= sprd_crtc_cleanup,
	.set_config	= drm_atomic_helper_set_config,
	.page_flip	= drm_atomic_helper_page_flip,
	.reset 		= sprd_crtc_reset,
	.atomic_duplicate_state	= sprd_crtc_atomic_duplicate_state,
	.atomic_destroy_state	= sprd_crtc_atomic_destroy_state,
	.enable_vblank	= sprd_crtc_enable_vblank,
	.disable_vblank	= sprd_crtc_disable_vblank,
	.atomic_set_property = sprd_crtc_atomic_set_property,
	.atomic_get_property = sprd_crtc_atomic_get_property,
};

static int sprd_crtc_create_properties(struct sprd_crtc *crtc, const char *version, u32 corner_size)
{
	struct drm_property *prop;
	struct drm_property_blob *blob;
	size_t blob_size;

	blob_size = strlen(version) + 1;

	blob = drm_property_create_blob(crtc->base.dev, blob_size, version);
	if (IS_ERR(blob)) {
		DRM_ERROR("drm_property_create_blob dpu version failed\n");
		return PTR_ERR(blob);
	}

	/* create dpu version property */
	prop = drm_property_create(crtc->base.dev,
		DRM_MODE_PROP_IMMUTABLE | DRM_MODE_PROP_BLOB,
		"dpu version", 0);
	if (!prop) {
		DRM_ERROR("drm_property_create dpu version failed\n");
		return -ENOMEM;
	}
	drm_object_attach_property(&crtc->base.base, prop, blob->base.id);

	/* create corner size property */
	prop = drm_property_create(crtc->base.dev,
		DRM_MODE_PROP_IMMUTABLE | DRM_MODE_PROP_RANGE,
		"corner size", 0);
	if (!prop) {
		DRM_ERROR("drm_property_create corner size failed\n");
		return -ENOMEM;
	}
	drm_object_attach_property(&crtc->base.base, prop, corner_size);

	/* create resolution change property */
	prop = drm_property_create_range(crtc->base.dev, 0,
			"resolution change", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&crtc->base.base, prop, 0);
	crtc->resolution_property = prop;

	/* create frame rate change property */
	prop = drm_property_create_range(crtc->base.dev, 0,
			"frame rate change", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&crtc->base.base, prop, 0);
	crtc->frame_rate_property = prop;

	return 0;
}

struct sprd_crtc *sprd_crtc_init(struct drm_device *drm,
					struct sprd_plane *planes,
					enum sprd_crtc_output_type type,
					const struct sprd_crtc_ops *ops,
					const char *version,
					u32 corner_size,
					void *priv)
{
	struct sprd_crtc *crtc;
	struct drm_plane *primary = &planes[0].base;
	int ret;

	crtc = devm_kzalloc(drm->dev, sizeof(*crtc), GFP_KERNEL);
	if (!crtc)
		return ERR_PTR(-ENOMEM);

	crtc->type = type;
	crtc->ops = ops;
	crtc->priv = priv;
	crtc->planes = planes;

	ret = drm_crtc_init_with_planes(drm, &crtc->base, primary, NULL,
					&sprd_crtc_funcs, NULL);
	if (ret < 0) {
		DRM_ERROR("failed to initial crtc.\n");
		goto err_crtc;
	}

	drm_crtc_helper_add(&crtc->base, &sprd_crtc_helper_funcs);

	sprd_crtc_create_properties(crtc, version, corner_size);

	return crtc;

err_crtc:
	//plane->funcs->destroy(primary);
	return ERR_PTR(ret);
}

struct sprd_crtc *sprd_crtc_get_by_type(struct drm_device *drm,
				       enum sprd_crtc_output_type out_type)
{
	struct drm_crtc *crtc;

	drm_for_each_crtc(crtc, drm)
		if (to_sprd_crtc(crtc)->type == out_type)
			return to_sprd_crtc(crtc);

	return ERR_PTR(-EPERM);
}

int sprd_drm_set_possible_crtcs(struct drm_encoder *encoder,
		enum sprd_crtc_output_type out_type)
{
	struct sprd_crtc *crtc = sprd_crtc_get_by_type(encoder->dev,
						out_type);

	if (IS_ERR(crtc))
		return PTR_ERR(crtc);

	encoder->possible_crtcs = drm_crtc_mask(&crtc->base);

	return 0;
}
