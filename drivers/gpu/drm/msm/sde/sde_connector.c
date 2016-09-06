/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"sde-drm:[%s] " fmt, __func__
#include "msm_drv.h"

#include "sde_kms.h"
#include "sde_connector.h"
#include "sde_backlight.h"

static const struct drm_prop_enum_list e_topology_name[] = {
	{SDE_RM_TOPOLOGY_UNKNOWN,	"sde_unknown"},
	{SDE_RM_TOPOLOGY_SINGLEPIPE,	"sde_singlepipe"},
	{SDE_RM_TOPOLOGY_DUALPIPE,	"sde_dualpipe"},
	{SDE_RM_TOPOLOGY_PPSPLIT,	"sde_ppsplit"},
	{SDE_RM_TOPOLOGY_DUALPIPEMERGE,	"sde_dualpipemerge"}
};
static const struct drm_prop_enum_list e_topology_control[] = {
	{SDE_RM_TOPCTL_RESERVE_LOCK,	"reserve_lock"},
	{SDE_RM_TOPCTL_RESERVE_CLEAR,	"reserve_clear"},
	{SDE_RM_TOPCTL_DSPP,		"dspp"},
	{SDE_RM_TOPCTL_FORCE_TILING,	"force_tiling"},
	{SDE_RM_TOPCTL_PPSPLIT,		"ppsplit"}
};

int sde_connector_get_info(struct drm_connector *connector,
		struct msm_display_info *info)
{
	struct sde_connector *c_conn;

	if (!connector || !info) {
		SDE_ERROR("invalid argument(s), conn %pK, info %pK\n",
				connector, info);
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);

	if (!c_conn->display || !c_conn->ops.get_info) {
		SDE_ERROR("display info not supported for %pK\n",
				c_conn->display);
		return -EINVAL;
	}

	return c_conn->ops.get_info(info, c_conn->display);
}

static void sde_connector_destroy(struct drm_connector *connector)
{
	struct sde_connector *c_conn;

	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return;
	}

	c_conn = to_sde_connector(connector);

	if (c_conn->blob_caps)
		drm_property_unreference_blob(c_conn->blob_caps);
	msm_property_destroy(&c_conn->property_info);

	drm_connector_unregister(connector);
	sde_fence_deinit(&c_conn->retire_fence);
	drm_connector_cleanup(connector);
	kfree(c_conn);
}

/**
 * _sde_connector_destroy_fb - clean up connector state's out_fb buffer
 * @c_conn: Pointer to sde connector structure
 * @c_state: Pointer to sde connector state structure
 */
static void _sde_connector_destroy_fb(struct sde_connector *c_conn,
		struct sde_connector_state *c_state)
{
	if (!c_state || !c_state->out_fb) {
		SDE_ERROR("invalid state %pK\n", c_state);
		return;
	}

	msm_framebuffer_cleanup(c_state->out_fb,
			c_state->mmu_id);
	drm_framebuffer_unreference(c_state->out_fb);
	c_state->out_fb = NULL;

	if (c_conn) {
		c_state->property_values[CONNECTOR_PROP_OUT_FB] =
			msm_property_get_default(&c_conn->property_info,
					CONNECTOR_PROP_OUT_FB);
	} else {
		c_state->property_values[CONNECTOR_PROP_OUT_FB] = ~0;
	}
}

static void sde_connector_atomic_destroy_state(struct drm_connector *connector,
		struct drm_connector_state *state)
{
	struct sde_connector *c_conn = NULL;
	struct sde_connector_state *c_state = NULL;

	if (!state) {
		SDE_ERROR("invalid state\n");
		return;
	}

	/*
	 * The base DRM framework currently always passes in a NULL
	 * connector pointer. This is not correct, but attempt to
	 * handle that case as much as possible.
	 */
	if (connector)
		c_conn = to_sde_connector(connector);
	c_state = to_sde_connector_state(state);

	if (c_state->out_fb)
		_sde_connector_destroy_fb(c_conn, c_state);

	if (!c_conn) {
		kfree(c_state);
	} else {
		/* destroy value helper */
		msm_property_destroy_state(&c_conn->property_info, c_state,
				c_state->property_values, 0);
	}
}

static void sde_connector_atomic_reset(struct drm_connector *connector)
{
	struct sde_connector *c_conn;
	struct sde_connector_state *c_state;

	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return;
	}

	c_conn = to_sde_connector(connector);

	if (connector->state) {
		sde_connector_atomic_destroy_state(connector, connector->state);
		connector->state = 0;
	}

	c_state = msm_property_alloc_state(&c_conn->property_info);
	if (!c_state) {
		SDE_ERROR("state alloc failed\n");
		return;
	}

	/* reset value helper, zero out state structure and reset properties */
	msm_property_reset_state(&c_conn->property_info, c_state,
			c_state->property_values, 0);

	c_state->base.connector = connector;
	connector->state = &c_state->base;
}

static struct drm_connector_state *
sde_connector_atomic_duplicate_state(struct drm_connector *connector)
{
	struct sde_connector *c_conn;
	struct sde_connector_state *c_state, *c_oldstate;
	int rc;

	if (!connector || !connector->state) {
		SDE_ERROR("invalid connector %pK\n", connector);
		return NULL;
	}

	c_conn = to_sde_connector(connector);
	c_oldstate = to_sde_connector_state(connector->state);
	c_state = msm_property_alloc_state(&c_conn->property_info);
	if (!c_state) {
		SDE_ERROR("state alloc failed\n");
		return NULL;
	}

	/* duplicate value helper */
	msm_property_duplicate_state(&c_conn->property_info,
			c_oldstate, c_state, c_state->property_values, 0);

	/* additional handling for drm framebuffer objects */
	if (c_state->out_fb) {
		drm_framebuffer_reference(c_state->out_fb);
		rc = msm_framebuffer_prepare(c_state->out_fb,
				c_state->mmu_id);
		if (rc)
			SDE_ERROR("failed to prepare fb, %d\n", rc);
	}

	return &c_state->base;
}

static int sde_connector_atomic_set_property(struct drm_connector *connector,
		struct drm_connector_state *state,
		struct drm_property *property,
		uint64_t val)
{
	struct sde_connector *c_conn;
	struct sde_connector_state *c_state;
	int idx, rc;

	if (!connector || !state || !property) {
		SDE_ERROR("invalid argument(s), conn %pK, state %pK, prp %pK\n",
				connector, state, property);
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);
	c_state = to_sde_connector_state(state);

	/* generic property handling */
	rc = msm_property_atomic_set(&c_conn->property_info,
			c_state->property_values, 0, property, val);
	if (rc)
		goto end;

	/* connector-specific property handling */
	idx = msm_property_index(&c_conn->property_info, property);

	if (idx == CONNECTOR_PROP_OUT_FB) {
		/* clear old fb, if present */
		if (c_state->out_fb)
			_sde_connector_destroy_fb(c_conn, c_state);

		/* convert fb val to drm framebuffer and prepare it */
		c_state->out_fb =
			drm_framebuffer_lookup(connector->dev, val);
		if (!c_state->out_fb) {
			SDE_ERROR("failed to look up fb %lld\n", val);
			rc = -EFAULT;
		} else {
			if (c_state->out_fb->flags & DRM_MODE_FB_SECURE)
				c_state->mmu_id =
				c_conn->mmu_id[SDE_IOMMU_DOMAIN_SECURE];
			else
				c_state->mmu_id =
				c_conn->mmu_id[SDE_IOMMU_DOMAIN_UNSECURE];

			rc = msm_framebuffer_prepare(c_state->out_fb,
					c_state->mmu_id);
			if (rc)
				SDE_ERROR("prep fb failed, %d\n", rc);
		}
	}

	if (idx == CONNECTOR_PROP_TOPOLOGY_CONTROL) {
		rc = sde_rm_check_property_topctl(val);
		if (rc)
			SDE_ERROR("invalid topology_control: 0x%llX\n", val);
	}

	/* check for custom property handling */
	if (!rc && c_conn->ops.set_property) {
		rc = c_conn->ops.set_property(connector,
				state,
				idx,
				val,
				c_conn->display);

		/* potentially clean up out_fb if rc != 0 */
		if ((idx == CONNECTOR_PROP_OUT_FB) && rc)
			_sde_connector_destroy_fb(c_conn, c_state);
	}
end:
	return rc;
}

static int sde_connector_set_property(struct drm_connector *connector,
		struct drm_property *property,
		uint64_t val)
{
	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return -EINVAL;
	}

	return sde_connector_atomic_set_property(connector,
			connector->state, property, val);
}

static int sde_connector_atomic_get_property(struct drm_connector *connector,
		const struct drm_connector_state *state,
		struct drm_property *property,
		uint64_t *val)
{
	struct sde_connector *c_conn;
	struct sde_connector_state *c_state;
	int idx, rc = -EINVAL;

	if (!connector || !state) {
		SDE_ERROR("invalid argument(s), conn %pK, state %pK\n",
				connector, state);
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);
	c_state = to_sde_connector_state(state);

	idx = msm_property_index(&c_conn->property_info, property);
	if (idx == CONNECTOR_PROP_RETIRE_FENCE)
		/*
		 * Set a fence offset if not a virtual connector, so that the
		 * fence signals after one additional commit rather than at the
		 * end of the current one.
		 */
		rc = sde_fence_create(&c_conn->retire_fence, val,
			c_conn->connector_type != DRM_MODE_CONNECTOR_VIRTUAL);
	else
		/* get cached property value */
		rc = msm_property_atomic_get(&c_conn->property_info,
				c_state->property_values, 0, property, val);

	/* allow for custom override */
	if (c_conn->ops.get_property)
		rc = c_conn->ops.get_property(connector,
				(struct drm_connector_state *)state,
				idx,
				val,
				c_conn->display);
	return rc;
}

void sde_connector_prepare_fence(struct drm_connector *connector)
{
	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return;
	}

	sde_fence_prepare(&to_sde_connector(connector)->retire_fence);
}

void sde_connector_complete_commit(struct drm_connector *connector)
{
	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return;
	}

	/* signal connector's retire fence */
	sde_fence_signal(&to_sde_connector(connector)->retire_fence, 0);
}

static enum drm_connector_status
sde_connector_detect(struct drm_connector *connector, bool force)
{
	enum drm_connector_status status = connector_status_unknown;
	struct sde_connector *c_conn;

	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return status;
	}

	c_conn = to_sde_connector(connector);

	if (c_conn->ops.detect)
		status = c_conn->ops.detect(connector,
				force,
				c_conn->display);

	return status;
}

static const struct drm_connector_funcs sde_connector_ops = {
	.dpms =                   drm_atomic_helper_connector_dpms,
	.reset =                  sde_connector_atomic_reset,
	.detect =                 sde_connector_detect,
	.destroy =                sde_connector_destroy,
	.fill_modes =             drm_helper_probe_single_connector_modes,
	.atomic_duplicate_state = sde_connector_atomic_duplicate_state,
	.atomic_destroy_state =   sde_connector_atomic_destroy_state,
	.atomic_set_property =    sde_connector_atomic_set_property,
	.atomic_get_property =    sde_connector_atomic_get_property,
	.set_property =           sde_connector_set_property,
};

static int sde_connector_get_modes(struct drm_connector *connector)
{
	struct sde_connector *c_conn;

	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return 0;
	}

	c_conn = to_sde_connector(connector);
	if (!c_conn->ops.get_modes) {
		SDE_DEBUG("missing get_modes callback\n");
		return 0;
	}

	return c_conn->ops.get_modes(connector, c_conn->display);
}

static enum drm_mode_status
sde_connector_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode)
{
	struct sde_connector *c_conn;

	if (!connector || !mode) {
		SDE_ERROR("invalid argument(s), conn %pK, mode %pK\n",
				connector, mode);
		return MODE_ERROR;
	}

	c_conn = to_sde_connector(connector);

	if (c_conn->ops.mode_valid)
		return c_conn->ops.mode_valid(connector, mode, c_conn->display);

	/* assume all modes okay by default */
	return MODE_OK;
}

static struct drm_encoder *
sde_connector_best_encoder(struct drm_connector *connector)
{
	struct sde_connector *c_conn = to_sde_connector(connector);

	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return NULL;
	}

	/*
	 * This is true for now, revisit this code when multiple encoders are
	 * supported.
	 */
	return c_conn->encoder;
}

static const struct drm_connector_helper_funcs sde_connector_helper_ops = {
	.get_modes =    sde_connector_get_modes,
	.mode_valid =   sde_connector_mode_valid,
	.best_encoder = sde_connector_best_encoder,
};

struct drm_connector *sde_connector_init(struct drm_device *dev,
		struct drm_encoder *encoder,
		struct drm_panel *panel,
		void *display,
		const struct sde_connector_ops *ops,
		int connector_poll,
		int connector_type)
{
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct sde_kms_info *info;
	struct sde_connector *c_conn = NULL;
	int rc;

	if (!dev || !dev->dev_private || !encoder) {
		SDE_ERROR("invalid argument(s), dev %pK, enc %pK\n",
				dev, encoder);
		return ERR_PTR(-EINVAL);
	}

	priv = dev->dev_private;
	if (!priv->kms) {
		SDE_ERROR("invalid kms reference\n");
		return ERR_PTR(-EINVAL);
	}

	c_conn = kzalloc(sizeof(*c_conn), GFP_KERNEL);
	if (!c_conn) {
		SDE_ERROR("failed to alloc sde connector\n");
		return ERR_PTR(-ENOMEM);
	}

	rc = drm_connector_init(dev,
			&c_conn->base,
			&sde_connector_ops,
			connector_type);
	if (rc)
		goto error_free_conn;

	c_conn->connector_type = connector_type;
	c_conn->encoder = encoder;
	c_conn->panel = panel;
	c_conn->display = display;

	/* cache mmu_id's for later */
	sde_kms = to_sde_kms(priv->kms);
	if (sde_kms->vbif[VBIF_NRT]) {
		c_conn->mmu_id[SDE_IOMMU_DOMAIN_UNSECURE] =
			sde_kms->mmu_id[MSM_SMMU_DOMAIN_NRT_UNSECURE];
		c_conn->mmu_id[SDE_IOMMU_DOMAIN_SECURE] =
			sde_kms->mmu_id[MSM_SMMU_DOMAIN_NRT_SECURE];
	} else {
		c_conn->mmu_id[SDE_IOMMU_DOMAIN_UNSECURE] =
			sde_kms->mmu_id[MSM_SMMU_DOMAIN_UNSECURE];
		c_conn->mmu_id[SDE_IOMMU_DOMAIN_SECURE] =
			sde_kms->mmu_id[MSM_SMMU_DOMAIN_SECURE];
	}

	if (ops)
		c_conn->ops = *ops;

	c_conn->base.helper_private = &sde_connector_helper_ops;
	c_conn->base.polled = connector_poll;
	c_conn->base.interlace_allowed = 0;
	c_conn->base.doublescan_allowed = 0;

	snprintf(c_conn->name,
			SDE_CONNECTOR_NAME_SIZE,
			"conn%u",
			c_conn->base.base.id);

	rc = sde_fence_init(&c_conn->retire_fence, c_conn->name,
			c_conn->base.base.id);
	if (rc) {
		SDE_ERROR("failed to init fence, %d\n", rc);
		goto error_cleanup_conn;
	}

	rc = drm_connector_register(&c_conn->base);
	if (rc) {
		SDE_ERROR("failed to register drm connector, %d\n", rc);
		goto error_cleanup_fence;
	}

	rc = drm_mode_connector_attach_encoder(&c_conn->base, encoder);
	if (rc) {
		SDE_ERROR("failed to attach encoder to connector, %d\n", rc);
		goto error_unregister_conn;
	}

	if (c_conn->ops.set_backlight) {
		rc = sde_backlight_setup(&c_conn->base);
		if (rc) {
			pr_err("failed to setup backlight, rc=%d\n", rc);
			goto error_unregister_conn;
		}
	}

	/* create properties */
	msm_property_init(&c_conn->property_info, &c_conn->base.base, dev,
			priv->conn_property, c_conn->property_data,
			CONNECTOR_PROP_COUNT, CONNECTOR_PROP_BLOBCOUNT,
			sizeof(struct sde_connector_state));

	if (c_conn->ops.post_init) {
		info = kmalloc(sizeof(*info), GFP_KERNEL);
		if (!info) {
			SDE_ERROR("failed to allocate info buffer\n");
			rc = -ENOMEM;
			goto error_unregister_conn;
		}

		sde_kms_info_reset(info);
		rc = c_conn->ops.post_init(&c_conn->base, info, display);
		if (rc) {
			SDE_ERROR("post-init failed, %d\n", rc);
			kfree(info);
			goto error_unregister_conn;
		}

		msm_property_install_blob(&c_conn->property_info,
				"capabilities",
				DRM_MODE_PROP_IMMUTABLE,
				CONNECTOR_PROP_SDE_INFO);

		msm_property_set_blob(&c_conn->property_info,
				&c_conn->blob_caps,
				SDE_KMS_INFO_DATA(info),
				SDE_KMS_INFO_DATALEN(info),
				CONNECTOR_PROP_SDE_INFO);
		kfree(info);
	}

	msm_property_install_range(&c_conn->property_info, "RETIRE_FENCE",
			0x0, 0, INR_OPEN_MAX, 0, CONNECTOR_PROP_RETIRE_FENCE);

	/* enum/bitmask properties */
	msm_property_install_enum(&c_conn->property_info, "topology_name",
			DRM_MODE_PROP_IMMUTABLE, 0, e_topology_name,
			ARRAY_SIZE(e_topology_name),
			CONNECTOR_PROP_TOPOLOGY_NAME);
	msm_property_install_enum(&c_conn->property_info, "topology_control",
			0, 1, e_topology_control,
			ARRAY_SIZE(e_topology_control),
			CONNECTOR_PROP_TOPOLOGY_CONTROL);

	rc = msm_property_install_get_status(&c_conn->property_info);
	if (rc) {
		SDE_ERROR("failed to create one or more properties\n");
		goto error_destroy_property;
	}

	SDE_DEBUG("connector %d attach encoder %d\n",
			c_conn->base.base.id, encoder->base.id);

	priv->connectors[priv->num_connectors++] = &c_conn->base;

	return &c_conn->base;

error_destroy_property:
	if (c_conn->blob_caps)
		drm_property_unreference_blob(c_conn->blob_caps);
	msm_property_destroy(&c_conn->property_info);
error_unregister_conn:
	drm_connector_unregister(&c_conn->base);
error_cleanup_fence:
	sde_fence_deinit(&c_conn->retire_fence);
error_cleanup_conn:
	drm_connector_cleanup(&c_conn->base);
error_free_conn:
	kfree(c_conn);

	return ERR_PTR(rc);
}
