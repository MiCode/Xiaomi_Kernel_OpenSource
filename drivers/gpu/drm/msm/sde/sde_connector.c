/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__
#include <linux/suspend.h>

#include "msm_drv.h"

#include "sde_kms.h"
#include "sde_connector.h"
#include "sde_backlight.h"
#include "sde_splash.h"

#define SDE_DEBUG_CONN(c, fmt, ...) SDE_DEBUG("conn%d " fmt,\
		(c) ? (c)->base.base.id : -1, ##__VA_ARGS__)

#define SDE_ERROR_CONN(c, fmt, ...) SDE_ERROR("conn%d " fmt,\
		(c) ? (c)->base.base.id : -1, ##__VA_ARGS__)

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
	{SDE_RM_TOPCTL_PPSPLIT,		"ppsplit"},
	{SDE_RM_TOPCTL_FORCE_MIXER,	"force_mixer"}
};

static const struct drm_prop_enum_list e_power_mode[] = {
	{SDE_MODE_DPMS_ON,      "ON"},
	{SDE_MODE_DPMS_LP1,     "LP1"},
	{SDE_MODE_DPMS_LP2,     "LP2"},
	{SDE_MODE_DPMS_OFF,     "OFF"},
};

static const struct drm_prop_enum_list hpd_clock_state[] = {
	{SDE_MODE_HPD_ON,      "ON"},
	{SDE_MODE_HPD_OFF,     "OFF"},
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

int sde_connector_pre_kickoff(struct drm_connector *connector)
{
	struct sde_connector *c_conn;
	struct sde_connector_state *c_state;
	struct msm_display_kickoff_params params;
	int rc;

	if (!connector) {
		SDE_ERROR("invalid argument\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);
	c_state = to_sde_connector_state(connector->state);

	if (!c_conn->display) {
		SDE_ERROR("invalid argument\n");
		return -EINVAL;
	}

	if (!c_conn->ops.pre_kickoff)
		return 0;

	params.hdr_ctrl = &c_state->hdr_ctrl;

	rc = c_conn->ops.pre_kickoff(connector, c_conn->display, &params);

	return rc;
}

enum sde_csc_type sde_connector_get_csc_type(struct drm_connector *conn)
{
	struct sde_connector *c_conn;

	if (!conn) {
		SDE_ERROR("invalid argument\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(conn);

	if (!c_conn->display) {
		SDE_ERROR("invalid argument\n");
		return -EINVAL;
	}

	if (!c_conn->ops.get_csc_type)
		return SDE_CSC_RGB2YUV_601L;

	return c_conn->ops.get_csc_type(conn, c_conn->display);
}

bool sde_connector_mode_needs_full_range(struct drm_connector *connector)
{
	struct sde_connector *c_conn;

	if (!connector) {
		SDE_ERROR("invalid argument\n");
		return false;
	}

	c_conn = to_sde_connector(connector);

	if (!c_conn->display) {
		SDE_ERROR("invalid argument\n");
		return false;
	}

	if (!c_conn->ops.mode_needs_full_range)
		return false;

	return c_conn->ops.mode_needs_full_range(c_conn->display);
}

static void sde_connector_destroy(struct drm_connector *connector)
{
	struct sde_connector *c_conn;

	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return;
	}

	c_conn = to_sde_connector(connector);

	if (c_conn->ops.pre_deinit)
		c_conn->ops.pre_deinit(connector, c_conn->display);

	if (c_conn->blob_caps)
		drm_property_unreference_blob(c_conn->blob_caps);
	if (c_conn->blob_hdr)
		drm_property_unreference_blob(c_conn->blob_hdr);
	msm_property_destroy(&c_conn->property_info);

	drm_connector_unregister(connector);
	mutex_destroy(&c_conn->lock);
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

	msm_framebuffer_cleanup(c_state->out_fb, c_state->aspace);
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
				c_state->aspace);
		if (rc)
			SDE_ERROR("failed to prepare fb, %d\n", rc);
	}

	return &c_state->base;
}

static int _sde_connector_set_hdr_info(
	struct sde_connector *c_conn,
	struct sde_connector_state *c_state,
	void *usr_ptr)
{
	struct drm_connector *connector;
	struct drm_msm_ext_panel_hdr_ctrl *hdr_ctrl;
	struct drm_msm_ext_panel_hdr_metadata *hdr_meta;
	int i;

	if (!c_conn || !c_state) {
		SDE_ERROR_CONN(c_conn, "invalid args\n");
		return -EINVAL;
	}

	connector = &c_conn->base;

	if (!connector->hdr_supported) {
		SDE_ERROR_CONN(c_conn, "sink doesn't support HDR\n");
		return -ENOTSUPP;
	}

	memset(&c_state->hdr_ctrl, 0, sizeof(c_state->hdr_ctrl));

	if (!usr_ptr) {
		SDE_DEBUG_CONN(c_conn, "hdr control cleared\n");
		return 0;
	}

	if (copy_from_user(&c_state->hdr_ctrl,
		(void __user *)usr_ptr,
			sizeof(*hdr_ctrl))) {
		SDE_ERROR_CONN(c_conn, "failed to copy hdr control\n");
		return -EFAULT;
	}

	hdr_ctrl = &c_state->hdr_ctrl;

	SDE_DEBUG_CONN(c_conn, "hdr_supported %d\n",
				   hdr_ctrl->hdr_state);

	hdr_meta = &hdr_ctrl->hdr_meta;

	SDE_DEBUG_CONN(c_conn, "hdr_supported %d\n",
				   hdr_meta->hdr_supported);
	SDE_DEBUG_CONN(c_conn, "eotf %d\n",
				   hdr_meta->eotf);
	SDE_DEBUG_CONN(c_conn, "white_point_x %d\n",
				   hdr_meta->white_point_x);
	SDE_DEBUG_CONN(c_conn, "white_point_y %d\n",
				   hdr_meta->white_point_y);
	SDE_DEBUG_CONN(c_conn, "max_luminance %d\n",
				   hdr_meta->max_luminance);
	SDE_DEBUG_CONN(c_conn, "max_content_light_level %d\n",
				   hdr_meta->max_content_light_level);
	SDE_DEBUG_CONN(c_conn, "max_average_light_level %d\n",
				   hdr_meta->max_average_light_level);

	for (i = 0; i < HDR_PRIMARIES_COUNT; i++) {
		SDE_DEBUG_CONN(c_conn, "display_primaries_x [%d]\n",
				   hdr_meta->display_primaries_x[i]);
		SDE_DEBUG_CONN(c_conn, "display_primaries_y [%d]\n",
				   hdr_meta->display_primaries_y[i]);
	}

	return 0;
}

static int _sde_connector_update_power_locked(struct sde_connector *c_conn)
{
	struct drm_connector *connector;
	void *display;
	int (*set_power)(struct drm_connector *, int, void *);
	int mode, rc = 0;

	if (!c_conn)
		return -EINVAL;
	connector = &c_conn->base;

	mode = c_conn->lp_mode;
	if (c_conn->dpms_mode != DRM_MODE_DPMS_ON)
		mode = SDE_MODE_DPMS_OFF;
	switch (c_conn->dpms_mode) {
	case DRM_MODE_DPMS_ON:
		mode = c_conn->lp_mode;
		break;
	case DRM_MODE_DPMS_STANDBY:
		mode = SDE_MODE_DPMS_STANDBY;
		break;
	case DRM_MODE_DPMS_SUSPEND:
		mode = SDE_MODE_DPMS_SUSPEND;
		break;
	case DRM_MODE_DPMS_OFF:
		mode = SDE_MODE_DPMS_OFF;
		break;
	default:
		mode = c_conn->lp_mode;
		SDE_ERROR("conn %d dpms set to unrecognized mode %d\n",
				connector->base.id, mode);
		break;
	}

	SDE_DEBUG("conn %d - dpms %d, lp %d, panel %d\n", connector->base.id,
			c_conn->dpms_mode, c_conn->lp_mode, mode);

	if (mode != c_conn->last_panel_power_mode && c_conn->ops.set_power) {
		display = c_conn->display;
		set_power = c_conn->ops.set_power;

		mutex_unlock(&c_conn->lock);
		rc = set_power(connector, mode, display);
		mutex_lock(&c_conn->lock);
	}
	c_conn->last_panel_power_mode = mode;

	return rc;
}

static int sde_connector_atomic_set_property(struct drm_connector *connector,
		struct drm_connector_state *state,
		struct drm_property *property,
		uint64_t val)
{
	struct sde_connector *c_conn;
	struct sde_connector_state *c_state;
	int idx, rc;
	uint64_t fence_fd = 0;

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
	switch (idx) {
	case CONNECTOR_PROP_OUT_FB:
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
				c_state->aspace =
				c_conn->aspace[SDE_IOMMU_DOMAIN_SECURE];
			else
				c_state->aspace =
				c_conn->aspace[SDE_IOMMU_DOMAIN_UNSECURE];

			rc = msm_framebuffer_prepare(c_state->out_fb,
					c_state->aspace);
			if (rc)
				SDE_ERROR("prep fb failed, %d\n", rc);
		}
		break;
	case CONNECTOR_PROP_RETIRE_FENCE:
		if (!val)
			goto end;

		/*
		 * update the the offset to a timeline for commit completion
		 */
		rc = sde_fence_create(&c_conn->retire_fence, &fence_fd, 1);
		if (rc) {
			SDE_ERROR("fence create failed rc:%d\n", rc);
			goto end;
		}

		rc = copy_to_user((uint64_t __user *)val, &fence_fd,
			sizeof(uint64_t));
		if (rc) {
			SDE_ERROR("copy to user failed rc:%d\n", rc);
			/* fence will be released with timeline update */
			put_unused_fd(fence_fd);
			rc = -EFAULT;
			goto end;
		}
		break;
	case CONNECTOR_PROP_TOPOLOGY_CONTROL:
		rc = sde_rm_check_property_topctl(val);
		if (rc)
			SDE_ERROR("invalid topology_control: 0x%llX\n", val);
		break;
	case CONNECTOR_PROP_LP:
		mutex_lock(&c_conn->lock);
		c_conn->lp_mode = val;
		_sde_connector_update_power_locked(c_conn);
		mutex_unlock(&c_conn->lock);
		break;
	case CONNECTOR_PROP_HPD_OFF:
		c_conn->hpd_mode = val;
		break;
	default:
		break;
	}

	if (idx == CONNECTOR_PROP_HDR_CONTROL) {
		rc = _sde_connector_set_hdr_info(c_conn, c_state, (void *)val);
		if (rc)
			SDE_ERROR_CONN(c_conn, "cannot set hdr info %d\n", rc);
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
	if (idx == CONNECTOR_PROP_RETIRE_FENCE) {
		*val = ~0;
		rc = 0;
	} else {
		/* get cached property value */
		rc = msm_property_atomic_get(&c_conn->property_info,
				c_state->property_values, 0, property, val);
	}

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
	struct drm_device *dev;
	struct msm_drm_private *priv;
	struct sde_connector *c_conn;

	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return;
	}

	dev = connector->dev;
	priv = dev->dev_private;

	/* signal connector's retire fence */
	sde_fence_signal(&to_sde_connector(connector)->retire_fence, 0);

	/* If below both 2 conditions are met, LK's early splash resources
	 * should be freed.
	 *	1) When get_hibernation_status() is returned as true.
	 *		a. hibernation image snapshot failed.
	 *		b. hibernation restore successful.
	 *		c. hibernation restore failed.
	 *	2) After LK totally exits.
	 */
	if (get_hibernation_status() &&
		sde_splash_get_lk_complete_status(priv->kms)) {
		c_conn = to_sde_connector(connector);

		sde_splash_free_resource(priv->kms, &priv->phandle,
					c_conn->connector_type,
					c_conn->display,
					c_conn->is_shared);
	}

}

static int sde_connector_dpms(struct drm_connector *connector,
		int mode)
{
	struct sde_connector *c_conn;

	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return -EINVAL;
	}
	c_conn = to_sde_connector(connector);

	/* validate incoming dpms request */
	switch (mode) {
	case DRM_MODE_DPMS_ON:
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		SDE_DEBUG("conn %d dpms set to %d\n",
			connector->base.id, mode);
		break;
	default:
		SDE_ERROR("conn %d dpms set to unrecognized mode %d\n",
			connector->base.id, mode);
		break;
	}

	mutex_lock(&c_conn->lock);
	c_conn->dpms_mode = mode;
	_sde_connector_update_power_locked(c_conn);
	mutex_unlock(&c_conn->lock);

	/* use helper for boilerplate handling */
	return drm_atomic_helper_connector_dpms(connector, mode);
}

int sde_connector_get_dpms(struct drm_connector *connector)
{
	struct sde_connector *c_conn;
	int rc;

	if (!connector) {
		SDE_DEBUG("invalid connector\n");
		return DRM_MODE_DPMS_OFF;
	}

	c_conn = to_sde_connector(connector);

	mutex_lock(&c_conn->lock);
	rc = c_conn->dpms_mode;
	mutex_unlock(&c_conn->lock);

	return rc;
}

static void sde_connector_update_hdr_props(struct drm_connector *connector)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct drm_msm_ext_panel_hdr_properties hdr_prop = {};

	hdr_prop.hdr_supported = connector->hdr_supported;

	if (hdr_prop.hdr_supported) {
		hdr_prop.hdr_eotf =
		  connector->hdr_eotf;
		hdr_prop.hdr_metadata_type_one =
		  connector->hdr_metadata_type_one;
		hdr_prop.hdr_max_luminance =
		  connector->hdr_max_luminance;
		hdr_prop.hdr_avg_luminance =
		  connector->hdr_avg_luminance;
		hdr_prop.hdr_min_luminance =
		  connector->hdr_min_luminance;

		msm_property_set_blob(&c_conn->property_info,
				&c_conn->blob_hdr,
				&hdr_prop,
				sizeof(hdr_prop),
				CONNECTOR_PROP_HDR_INFO);
	}
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
	.dpms =                   sde_connector_dpms,
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
	int ret = 0;

	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return 0;
	}

	c_conn = to_sde_connector(connector);
	if (!c_conn->ops.get_modes) {
		SDE_DEBUG("missing get_modes callback\n");
		return 0;
	}
	ret = c_conn->ops.get_modes(connector, c_conn->display);
	if (ret)
		sde_connector_update_hdr_props(connector);

	return ret;
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
	struct sde_splash_info *sinfo;
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

	c_conn->dpms_mode = DRM_MODE_DPMS_ON;
	c_conn->hpd_mode = SDE_MODE_HPD_ON;
	c_conn->lp_mode = 0;
	c_conn->last_panel_power_mode = SDE_MODE_DPMS_ON;


	sde_kms = to_sde_kms(priv->kms);
	if (sde_kms->vbif[VBIF_NRT]) {
		c_conn->aspace[SDE_IOMMU_DOMAIN_UNSECURE] =
			sde_kms->aspace[MSM_SMMU_DOMAIN_NRT_UNSECURE];
		c_conn->aspace[SDE_IOMMU_DOMAIN_SECURE] =
			sde_kms->aspace[MSM_SMMU_DOMAIN_NRT_SECURE];
	} else {
		c_conn->aspace[SDE_IOMMU_DOMAIN_UNSECURE] =
			sde_kms->aspace[MSM_SMMU_DOMAIN_UNSECURE];
		c_conn->aspace[SDE_IOMMU_DOMAIN_SECURE] =
			sde_kms->aspace[MSM_SMMU_DOMAIN_SECURE];
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

	mutex_init(&c_conn->lock);

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

	if (connector_type == DRM_MODE_CONNECTOR_HDMIA) {
		msm_property_install_blob(&c_conn->property_info,
				"hdr_properties",
				DRM_MODE_PROP_IMMUTABLE,
				CONNECTOR_PROP_HDR_INFO);
	}

	msm_property_install_volatile_range(&c_conn->property_info,
		"hdr_control", 0x0, 0, ~0, 0,
		CONNECTOR_PROP_HDR_CONTROL);

	msm_property_install_volatile_range(&c_conn->property_info,
		"RETIRE_FENCE", 0x0, 0, ~0, 0, CONNECTOR_PROP_RETIRE_FENCE);

	msm_property_install_volatile_signed_range(&c_conn->property_info,
			"PLL_DELTA", 0x0, INT_MIN, INT_MAX, 0,
			CONNECTOR_PROP_PLL_DELTA);

	msm_property_install_volatile_range(&c_conn->property_info,
			"PLL_ENABLE", 0x0, 0, 1, 0,
			CONNECTOR_PROP_PLL_ENABLE);

	msm_property_install_volatile_range(&c_conn->property_info,
			"HDCP_VERSION", 0x0, 0, U8_MAX, 0,
			CONNECTOR_PROP_HDCP_VERSION);

	/* enum/bitmask properties */
	msm_property_install_enum(&c_conn->property_info, "topology_name",
			DRM_MODE_PROP_IMMUTABLE, 0, e_topology_name,
			ARRAY_SIZE(e_topology_name),
			CONNECTOR_PROP_TOPOLOGY_NAME, 0);
	msm_property_install_enum(&c_conn->property_info, "topology_control",
			0, 1, e_topology_control,
			ARRAY_SIZE(e_topology_control),
			CONNECTOR_PROP_TOPOLOGY_CONTROL, 0);

	msm_property_install_enum(&c_conn->property_info, "LP",
			0, 0, e_power_mode,
			ARRAY_SIZE(e_power_mode),
			CONNECTOR_PROP_LP, 0);

	msm_property_install_enum(&c_conn->property_info, "HPD_OFF",
			DRM_MODE_PROP_ATOMIC, 0, hpd_clock_state,
			ARRAY_SIZE(hpd_clock_state),
			CONNECTOR_PROP_HPD_OFF, 0);

	rc = msm_property_install_get_status(&c_conn->property_info);
	if (rc) {
		SDE_ERROR("failed to create one or more properties\n");
		goto error_destroy_property;
	}

	SDE_DEBUG("connector %d attach encoder %d\n",
			c_conn->base.base.id, encoder->base.id);

	sinfo = &sde_kms->splash_info;
	if (sinfo && sinfo->handoff)
		sde_splash_setup_connector_count(sinfo, connector_type,
					display, c_conn->is_shared);

	priv->connectors[priv->num_connectors++] = &c_conn->base;

	return &c_conn->base;

error_destroy_property:
	if (c_conn->blob_caps)
		drm_property_unreference_blob(c_conn->blob_caps);
	if (c_conn->blob_hdr)
		drm_property_unreference_blob(c_conn->blob_hdr);
	msm_property_destroy(&c_conn->property_info);
error_unregister_conn:
	drm_connector_unregister(&c_conn->base);
error_cleanup_fence:
	mutex_destroy(&c_conn->lock);
	sde_fence_deinit(&c_conn->retire_fence);
error_cleanup_conn:
	drm_connector_cleanup(&c_conn->base);
error_free_conn:
	kfree(c_conn);

	return ERR_PTR(rc);
}
