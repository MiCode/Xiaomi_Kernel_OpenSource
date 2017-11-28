/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#include "msm_drv.h"
#include "sde_dbg.h"

#include "sde_kms.h"
#include "sde_connector.h"
#include <linux/backlight.h>
#include "dsi_drm.h"
#include "dsi_display.h"

#define BL_NODE_NAME_SIZE 32

/* Autorefresh will occur after FRAME_CNT frames. Large values are unlikely */
#define AUTOREFRESH_MAX_FRAME_CNT 6

#define SDE_DEBUG_CONN(c, fmt, ...) SDE_DEBUG("conn%d " fmt,\
		(c) ? (c)->base.base.id : -1, ##__VA_ARGS__)

#define SDE_ERROR_CONN(c, fmt, ...) SDE_ERROR("conn%d " fmt,\
		(c) ? (c)->base.base.id : -1, ##__VA_ARGS__)
static u32 dither_matrix[DITHER_MATRIX_SZ] = {
	15, 7, 13, 5, 3, 11, 1, 9, 12, 4, 14, 6, 0, 8, 2, 10
};

static const struct drm_prop_enum_list e_topology_name[] = {
	{SDE_RM_TOPOLOGY_NONE,	"sde_none"},
	{SDE_RM_TOPOLOGY_SINGLEPIPE,	"sde_singlepipe"},
	{SDE_RM_TOPOLOGY_SINGLEPIPE_DSC,	"sde_singlepipe_dsc"},
	{SDE_RM_TOPOLOGY_DUALPIPE,	"sde_dualpipe"},
	{SDE_RM_TOPOLOGY_DUALPIPE_DSC,	"sde_dualpipe_dsc"},
	{SDE_RM_TOPOLOGY_DUALPIPE_3DMERGE,	"sde_dualpipemerge"},
	{SDE_RM_TOPOLOGY_DUALPIPE_3DMERGE_DSC,	"sde_dualpipemerge_dsc"},
	{SDE_RM_TOPOLOGY_DUALPIPE_DSCMERGE,	"sde_dualpipe_dscmerge"},
	{SDE_RM_TOPOLOGY_PPSPLIT,	"sde_ppsplit"},
};
static const struct drm_prop_enum_list e_topology_control[] = {
	{SDE_RM_TOPCTL_RESERVE_LOCK,	"reserve_lock"},
	{SDE_RM_TOPCTL_RESERVE_CLEAR,	"reserve_clear"},
	{SDE_RM_TOPCTL_DSPP,		"dspp"},
};
static const struct drm_prop_enum_list e_power_mode[] = {
	{SDE_MODE_DPMS_ON,	"ON"},
	{SDE_MODE_DPMS_LP1,	"LP1"},
	{SDE_MODE_DPMS_LP2,	"LP2"},
	{SDE_MODE_DPMS_OFF,	"OFF"},
};

static int sde_backlight_device_update_status(struct backlight_device *bd)
{
	int brightness;
	struct dsi_display *display;
	struct sde_connector *c_conn;
	int bl_lvl;
	struct drm_event event;

	brightness = bd->props.brightness;

	if ((bd->props.power != FB_BLANK_UNBLANK) ||
			(bd->props.state & BL_CORE_FBBLANK) ||
			(bd->props.state & BL_CORE_SUSPENDED))
		brightness = 0;

	c_conn = bl_get_data(bd);
	display = (struct dsi_display *) c_conn->display;
	if (brightness > display->panel->bl_config.bl_max_level)
		brightness = display->panel->bl_config.bl_max_level;

	/* map UI brightness into driver backlight level with rounding */
	bl_lvl = mult_frac(brightness, display->panel->bl_config.bl_max_level,
			display->panel->bl_config.brightness_max_level);

	if (!bl_lvl && brightness)
		bl_lvl = 1;

	if (c_conn->ops.set_backlight) {
		event.type = DRM_EVENT_SYS_BACKLIGHT;
		event.length = sizeof(u32);
		msm_mode_object_event_notify(&c_conn->base.base,
				c_conn->base.dev, &event, (u8 *)&brightness);
		c_conn->ops.set_backlight(c_conn->display, bl_lvl);
	}

	return 0;
}

static int sde_backlight_device_get_brightness(struct backlight_device *bd)
{
	return 0;
}

static const struct backlight_ops sde_backlight_device_ops = {
	.update_status = sde_backlight_device_update_status,
	.get_brightness = sde_backlight_device_get_brightness,
};

static int sde_backlight_setup(struct sde_connector *c_conn,
					struct drm_device *dev)
{
	struct backlight_properties props;
	struct dsi_display *display;
	struct dsi_backlight_config *bl_config;
	static int display_count;
	char bl_node_name[BL_NODE_NAME_SIZE];

	if (!c_conn || !dev || !dev->dev) {
		SDE_ERROR("invalid param\n");
		return -EINVAL;
	} else if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI) {
		return 0;
	}

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.power = FB_BLANK_UNBLANK;

	display = (struct dsi_display *) c_conn->display;
	bl_config = &display->panel->bl_config;
	props.max_brightness = bl_config->brightness_max_level;
	props.brightness = bl_config->brightness_max_level;
	snprintf(bl_node_name, BL_NODE_NAME_SIZE, "panel%u-backlight",
							display_count);
	c_conn->bl_device = backlight_device_register(bl_node_name, dev->dev,
			c_conn, &sde_backlight_device_ops, &props);
	if (IS_ERR_OR_NULL(c_conn->bl_device)) {
		SDE_ERROR("Failed to register backlight: %ld\n",
				    PTR_ERR(c_conn->bl_device));
		c_conn->bl_device = NULL;
		return -ENODEV;
	}
	display_count++;

	return 0;
}

int sde_connector_trigger_event(void *drm_connector,
		uint32_t event_idx, uint32_t instance_idx,
		uint32_t data0, uint32_t data1,
		uint32_t data2, uint32_t data3)
{
	struct sde_connector *c_conn;
	unsigned long irq_flags;
	void (*cb_func)(uint32_t event_idx,
			uint32_t instance_idx, void *usr,
			uint32_t data0, uint32_t data1,
			uint32_t data2, uint32_t data3);
	void *usr;
	int rc = 0;

	/*
	 * This function may potentially be called from an ISR context, so
	 * avoid excessive logging/etc.
	 */
	if (!drm_connector)
		return -EINVAL;
	else if (event_idx >= SDE_CONN_EVENT_COUNT)
		return -EINVAL;
	c_conn = to_sde_connector(drm_connector);

	spin_lock_irqsave(&c_conn->event_lock, irq_flags);
	cb_func = c_conn->event_table[event_idx].cb_func;
	usr = c_conn->event_table[event_idx].usr;
	spin_unlock_irqrestore(&c_conn->event_lock, irq_flags);

	if (cb_func)
		cb_func(event_idx, instance_idx, usr,
			data0, data1, data2, data3);
	else
		rc = -EAGAIN;

	return rc;
}

int sde_connector_register_event(struct drm_connector *connector,
		uint32_t event_idx,
		void (*cb_func)(uint32_t event_idx,
			uint32_t instance_idx, void *usr,
			uint32_t data0, uint32_t data1,
			uint32_t data2, uint32_t data3),
		void *usr)
{
	struct sde_connector *c_conn;
	unsigned long irq_flags;

	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return -EINVAL;
	} else if (event_idx >= SDE_CONN_EVENT_COUNT) {
		SDE_ERROR("conn%d, invalid event %d\n",
				connector->base.id, event_idx);
		return -EINVAL;
	}
	c_conn = to_sde_connector(connector);

	spin_lock_irqsave(&c_conn->event_lock, irq_flags);
	c_conn->event_table[event_idx].cb_func = cb_func;
	c_conn->event_table[event_idx].usr = usr;
	spin_unlock_irqrestore(&c_conn->event_lock, irq_flags);

	/* optionally notify display of event registration */
	if (c_conn->ops.enable_event && c_conn->display)
		c_conn->ops.enable_event(connector, event_idx,
				cb_func != NULL, c_conn->display);
	return 0;
}

void sde_connector_unregister_event(struct drm_connector *connector,
		uint32_t event_idx)
{
	(void)sde_connector_register_event(connector, event_idx, 0, 0);
}

static int _sde_connector_get_default_dither_cfg_v1(
		struct sde_connector *c_conn, void *cfg)
{
	struct drm_msm_dither *dither_cfg = (struct drm_msm_dither *)cfg;
	enum dsi_pixel_format dst_format = DSI_PIXEL_FORMAT_MAX;

	if (!c_conn || !cfg) {
		SDE_ERROR("invalid argument(s), c_conn %pK, cfg %pK\n",
				c_conn, cfg);
		return -EINVAL;
	}

	if (!c_conn->ops.get_dst_format) {
		SDE_DEBUG("get_dst_format is unavailable\n");
		return 0;
	}

	dst_format = c_conn->ops.get_dst_format(c_conn->display);
	switch (dst_format) {
	case DSI_PIXEL_FORMAT_RGB888:
		dither_cfg->c0_bitdepth = 8;
		dither_cfg->c1_bitdepth = 8;
		dither_cfg->c2_bitdepth = 8;
		dither_cfg->c3_bitdepth = 8;
		break;
	case DSI_PIXEL_FORMAT_RGB666:
	case DSI_PIXEL_FORMAT_RGB666_LOOSE:
		dither_cfg->c0_bitdepth = 6;
		dither_cfg->c1_bitdepth = 6;
		dither_cfg->c2_bitdepth = 6;
		dither_cfg->c3_bitdepth = 6;
		break;
	default:
		SDE_DEBUG("no default dither config for dst_format %d\n",
			dst_format);
		return -ENODATA;
	}

	memcpy(&dither_cfg->matrix, dither_matrix,
			sizeof(u32) * DITHER_MATRIX_SZ);
	dither_cfg->temporal_en = 0;
	return 0;
}

static void _sde_connector_install_dither_property(struct drm_device *dev,
		struct sde_kms *sde_kms, struct sde_connector *c_conn)
{
	char prop_name[DRM_PROP_NAME_LEN];
	struct sde_mdss_cfg *catalog = NULL;
	struct drm_property_blob *blob_ptr;
	void *cfg;
	int ret = 0;
	u32 version = 0, len = 0;
	bool defalut_dither_needed = false;

	if (!dev || !sde_kms || !c_conn) {
		SDE_ERROR("invld args (s), dev %pK, sde_kms %pK, c_conn %pK\n",
				dev, sde_kms, c_conn);
		return;
	}

	catalog = sde_kms->catalog;
	version = SDE_COLOR_PROCESS_MAJOR(
			catalog->pingpong[0].sblk->dither.version);
	snprintf(prop_name, ARRAY_SIZE(prop_name), "%s%d",
			"SDE_PP_DITHER_V", version);
	switch (version) {
	case 1:
		msm_property_install_blob(&c_conn->property_info, prop_name,
			DRM_MODE_PROP_BLOB,
			CONNECTOR_PROP_PP_DITHER);
		len = sizeof(struct drm_msm_dither);
		cfg = kzalloc(len, GFP_KERNEL);
		if (!cfg)
			return;

		ret = _sde_connector_get_default_dither_cfg_v1(c_conn, cfg);
		if (!ret)
			defalut_dither_needed = true;
		break;
	default:
		SDE_ERROR("unsupported dither version %d\n", version);
		return;
	}

	if (defalut_dither_needed) {
		blob_ptr = drm_property_create_blob(dev, len, cfg);
		if (IS_ERR_OR_NULL(blob_ptr))
			goto exit;
		c_conn->blob_dither = blob_ptr;
	}
exit:
	kfree(cfg);
}

int sde_connector_get_dither_cfg(struct drm_connector *conn,
			struct drm_connector_state *state, void **cfg,
			size_t *len)
{
	struct sde_connector *c_conn = NULL;
	struct sde_connector_state *c_state = NULL;
	size_t dither_sz = 0;

	if (!conn || !state || !(*cfg))
		return -EINVAL;

	c_conn = to_sde_connector(conn);
	c_state = to_sde_connector_state(state);

	/* try to get user config data first */
	*cfg = msm_property_get_blob(&c_conn->property_info,
					&c_state->property_state,
					&dither_sz,
					CONNECTOR_PROP_PP_DITHER);
	/* if user config data doesn't exist, use default dither blob */
	if (*cfg == NULL && c_conn->blob_dither) {
		*cfg = &c_conn->blob_dither->data;
		dither_sz = c_conn->blob_dither->length;
	}
	*len = dither_sz;
	return 0;
}

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

void sde_connector_schedule_status_work(struct drm_connector *connector,
		bool en)
{
	struct sde_connector *c_conn;
	struct msm_display_info info;

	c_conn = to_sde_connector(connector);
	if (!c_conn)
		return;

	sde_connector_get_info(connector, &info);
	if (c_conn->ops.check_status &&
		(info.capabilities & MSM_DISPLAY_ESD_ENABLED)) {
		if (en)
			/* Schedule ESD status check */
			schedule_delayed_work(&c_conn->status_work,
				msecs_to_jiffies(STATUS_CHECK_INTERVAL_MS));
		else
			/* Cancel any pending ESD status check */
			cancel_delayed_work_sync(&c_conn->status_work);
	}
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

	SDE_EVT32(connector->base.id, c_conn->dpms_mode, c_conn->lp_mode, mode);
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

	if (mode != SDE_MODE_DPMS_ON)
		sde_connector_schedule_status_work(connector, false);

	return rc;
}

int sde_connector_pre_kickoff(struct drm_connector *connector)
{
	struct sde_connector *c_conn;
	struct sde_connector_state *c_state;
	struct msm_display_kickoff_params params;
	int idx, rc;

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

	while ((idx = msm_property_pop_dirty(&c_conn->property_info,
					&c_state->property_state)) >= 0) {
		switch (idx) {
		case CONNECTOR_PROP_LP:
			mutex_lock(&c_conn->lock);
			c_conn->lp_mode = sde_connector_get_property(
					connector->state, CONNECTOR_PROP_LP);
			_sde_connector_update_power_locked(c_conn);
			mutex_unlock(&c_conn->lock);
			break;
		default:
			/* nothing to do for most properties */
			break;
		}
	}

	if (!c_conn->ops.pre_kickoff)
		return 0;

	params.rois = &c_state->rois;

	SDE_EVT32_VERBOSE(connector->base.id);

	rc = c_conn->ops.pre_kickoff(connector, c_conn->display, &params);

	return rc;
}

void sde_connector_clk_ctrl(struct drm_connector *connector, bool enable)
{
	struct sde_connector *c_conn;
	struct dsi_display *display;
	u32 state = enable ? DSI_CLK_ON : DSI_CLK_OFF;

	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return;
	}

	c_conn = to_sde_connector(connector);
	display = (struct dsi_display *) c_conn->display;

	if (display && c_conn->ops.clk_ctrl)
		c_conn->ops.clk_ctrl(display->mdp_clk_handle,
				DSI_ALL_CLKS, state);
}

static void sde_connector_destroy(struct drm_connector *connector)
{
	struct sde_connector *c_conn;

	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return;
	}

	c_conn = to_sde_connector(connector);

	/* cancel if any pending esd work */
	sde_connector_schedule_status_work(connector, false);

	if (c_conn->ops.put_modes)
		c_conn->ops.put_modes(connector, c_conn->display);

	if (c_conn->blob_caps)
		drm_property_unreference_blob(c_conn->blob_caps);
	if (c_conn->blob_hdr)
		drm_property_unreference_blob(c_conn->blob_hdr);
	if (c_conn->blob_dither)
		drm_property_unreference_blob(c_conn->blob_dither);
	msm_property_destroy(&c_conn->property_info);

	if (c_conn->bl_device)
		backlight_device_unregister(c_conn->bl_device);
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

	drm_framebuffer_unreference(c_state->out_fb);
	c_state->out_fb = NULL;

	if (c_conn)
		c_state->property_values[CONNECTOR_PROP_OUT_FB].value =
			msm_property_get_default(&c_conn->property_info,
					CONNECTOR_PROP_OUT_FB);
	else
		c_state->property_values[CONNECTOR_PROP_OUT_FB].value = ~0;
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
				&c_state->property_state);
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
			&c_state->property_state,
			c_state->property_values);

	c_state->base.connector = connector;
	connector->state = &c_state->base;
}

static struct drm_connector_state *
sde_connector_atomic_duplicate_state(struct drm_connector *connector)
{
	struct sde_connector *c_conn;
	struct sde_connector_state *c_state, *c_oldstate;

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
			c_oldstate, c_state,
			&c_state->property_state, c_state->property_values);

	/* additional handling for drm framebuffer objects */
	if (c_state->out_fb)
		drm_framebuffer_reference(c_state->out_fb);

	return &c_state->base;
}

static int _sde_connector_roi_v1_check_roi(
		struct sde_connector *c_conn,
		struct drm_clip_rect *roi_conn,
		const struct msm_roi_caps *caps)
{
	const struct msm_roi_alignment *align = &caps->align;
	int w = roi_conn->x2 - roi_conn->x1;
	int h = roi_conn->y2 - roi_conn->y1;

	if (w <= 0 || h <= 0) {
		SDE_ERROR_CONN(c_conn, "invalid conn roi w %d h %d\n", w, h);
		return -EINVAL;
	}

	if (w < align->min_width || w % align->width_pix_align) {
		SDE_ERROR_CONN(c_conn,
				"invalid conn roi width %d min %d align %d\n",
				w, align->min_width, align->width_pix_align);
		return -EINVAL;
	}

	if (h < align->min_height || h % align->height_pix_align) {
		SDE_ERROR_CONN(c_conn,
				"invalid conn roi height %d min %d align %d\n",
				h, align->min_height, align->height_pix_align);
		return -EINVAL;
	}

	if (roi_conn->x1 % align->xstart_pix_align) {
		SDE_ERROR_CONN(c_conn, "invalid conn roi x1 %d align %d\n",
				roi_conn->x1, align->xstart_pix_align);
		return -EINVAL;
	}

	if (roi_conn->y1 % align->ystart_pix_align) {
		SDE_ERROR_CONN(c_conn, "invalid conn roi y1 %d align %d\n",
				roi_conn->y1, align->ystart_pix_align);
		return -EINVAL;
	}

	return 0;
}

static int _sde_connector_set_roi_v1(
		struct sde_connector *c_conn,
		struct sde_connector_state *c_state,
		void *usr_ptr)
{
	struct sde_drm_roi_v1 roi_v1;
	struct msm_display_info display_info;
	struct msm_roi_caps *caps;
	int i, rc;

	if (!c_conn || !c_state) {
		SDE_ERROR("invalid args\n");
		return -EINVAL;
	}

	rc = sde_connector_get_info(&c_conn->base, &display_info);
	if (rc) {
		SDE_ERROR_CONN(c_conn, "display get info error: %d\n", rc);
		return rc;
	}

	caps = &display_info.roi_caps;
	if (!caps->enabled) {
		SDE_ERROR_CONN(c_conn, "display roi capability is disabled\n");
		return -ENOTSUPP;
	}

	memset(&c_state->rois, 0, sizeof(c_state->rois));

	if (!usr_ptr) {
		SDE_DEBUG_CONN(c_conn, "rois cleared\n");
		return 0;
	}

	if (copy_from_user(&roi_v1, usr_ptr, sizeof(roi_v1))) {
		SDE_ERROR_CONN(c_conn, "failed to copy roi_v1 data\n");
		return -EINVAL;
	}

	SDE_DEBUG_CONN(c_conn, "num_rects %d\n", roi_v1.num_rects);

	if (roi_v1.num_rects == 0) {
		SDE_DEBUG_CONN(c_conn, "rois cleared\n");
		return 0;
	}

	if (roi_v1.num_rects > SDE_MAX_ROI_V1 ||
			roi_v1.num_rects > caps->num_roi) {
		SDE_ERROR_CONN(c_conn, "too many rects specified: %d\n",
				roi_v1.num_rects);
		return -EINVAL;
	}

	c_state->rois.num_rects = roi_v1.num_rects;
	for (i = 0; i < roi_v1.num_rects; ++i) {
		int rc;

		rc = _sde_connector_roi_v1_check_roi(c_conn, &roi_v1.roi[i],
				caps);
		if (rc)
			return rc;

		c_state->rois.roi[i] = roi_v1.roi[i];
		SDE_DEBUG_CONN(c_conn, "roi%d: roi (%d,%d) (%d,%d)\n", i,
				c_state->rois.roi[i].x1,
				c_state->rois.roi[i].y1,
				c_state->rois.roi[i].x2,
				c_state->rois.roi[i].y2);
	}

	return 0;
}

static int _sde_connector_update_bl_scale(struct sde_connector *c_conn,
		int idx,
		uint64_t value)
{
	struct dsi_display *dsi_display = c_conn->display;
	struct dsi_backlight_config *bl_config;
	int rc = 0;

	if (!dsi_display || !dsi_display->panel) {
		pr_err("Invalid params(s) dsi_display %pK, panel %pK\n",
			dsi_display,
			((dsi_display) ? dsi_display->panel : NULL));
		return -EINVAL;
	}

	bl_config = &dsi_display->panel->bl_config;
	if (idx == CONNECTOR_PROP_BL_SCALE) {
		bl_config->bl_scale = value;
		if (value > MAX_BL_SCALE_LEVEL)
			bl_config->bl_scale = MAX_BL_SCALE_LEVEL;
		SDE_DEBUG("set to panel: bl_scale = %u, bl_level = %u\n",
			bl_config->bl_scale, bl_config->bl_level);
		rc = c_conn->ops.set_backlight(dsi_display,
					       bl_config->bl_level);
	} else if (idx == CONNECTOR_PROP_AD_BL_SCALE) {
		bl_config->bl_scale_ad = value;
		if (value > MAX_AD_BL_SCALE_LEVEL)
			bl_config->bl_scale_ad = MAX_AD_BL_SCALE_LEVEL;
		SDE_DEBUG("set to panel: bl_scale_ad = %u, bl_level = %u\n",
			bl_config->bl_scale_ad, bl_config->bl_level);
		rc = c_conn->ops.set_backlight(dsi_display,
					       bl_config->bl_level);
	}
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

	if (!connector || !state || !property) {
		SDE_ERROR("invalid argument(s), conn %pK, state %pK, prp %pK\n",
				connector, state, property);
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);
	c_state = to_sde_connector_state(state);

	/* generic property handling */
	rc = msm_property_atomic_set(&c_conn->property_info,
			&c_state->property_state, property, val);
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
		if (!c_state->out_fb && val) {
			SDE_ERROR("failed to look up fb %lld\n", val);
			rc = -EFAULT;
		} else if (!c_state->out_fb && !val) {
			SDE_DEBUG("cleared fb_id\n");
			rc = 0;
		} else {
			msm_framebuffer_set_kmap(c_state->out_fb,
					c_conn->fb_kmap);
		}
		break;
	case CONNECTOR_PROP_BL_SCALE:
	case CONNECTOR_PROP_AD_BL_SCALE:
		rc = _sde_connector_update_bl_scale(c_conn, idx, val);
		break;
	default:
		break;
	}

	if (idx == CONNECTOR_PROP_ROI_V1) {
		rc = _sde_connector_set_roi_v1(c_conn, c_state, (void *)val);
		if (rc)
			SDE_ERROR_CONN(c_conn, "invalid roi_v1, rc: %d\n", rc);
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
		rc = sde_fence_create(&c_conn->retire_fence, val, 0);
	else
		/* get cached property value */
		rc = msm_property_atomic_get(&c_conn->property_info,
				&c_state->property_state, property, val);

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

void sde_connector_complete_commit(struct drm_connector *connector,
		ktime_t ts)
{
	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return;
	}

	/* signal connector's retire fence */
	sde_fence_signal(&to_sde_connector(connector)->retire_fence, ts, false);
}

void sde_connector_commit_reset(struct drm_connector *connector, ktime_t ts)
{
	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return;
	}

	/* signal connector's retire fence */
	sde_fence_signal(&to_sde_connector(connector)->retire_fence, ts, true);
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
		SDE_DEBUG("conn %d dpms set to %d\n", connector->base.id, mode);
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

int sde_connector_set_property_for_commit(struct drm_connector *connector,
		struct drm_atomic_state *atomic_state,
		uint32_t property_idx, uint64_t value)
{
	struct drm_connector_state *state;
	struct drm_property *property;
	struct sde_connector *c_conn;

	if (!connector || !atomic_state) {
		SDE_ERROR("invalid argument(s), conn %d, state %d\n",
				connector != NULL, atomic_state != NULL);
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);
	property = msm_property_index_to_drm_property(
			&c_conn->property_info, property_idx);
	if (!property) {
		SDE_ERROR("invalid property index %d\n", property_idx);
		return -EINVAL;
	}

	state = drm_atomic_get_connector_state(atomic_state, connector);
	if (IS_ERR_OR_NULL(state)) {
		SDE_ERROR("failed to get conn %d state\n",
				connector->base.id);
		return -EINVAL;
	}

	return drm_atomic_connector_set_property(
			connector, state, property, value);
}

#ifdef CONFIG_DEBUG_FS
/**
 * sde_connector_init_debugfs - initialize connector debugfs
 * @connector: Pointer to drm connector
 */
static int sde_connector_init_debugfs(struct drm_connector *connector)
{
	struct sde_connector *sde_connector;

	if (!connector || !connector->debugfs_entry) {
		SDE_ERROR("invalid connector\n");
		return -EINVAL;
	}

	sde_connector = to_sde_connector(connector);

	if (!debugfs_create_bool("fb_kmap", 0600, connector->debugfs_entry,
			&sde_connector->fb_kmap)) {
		SDE_ERROR("failed to create connector fb_kmap\n");
		return -ENOMEM;
	}

	return 0;
}
#else
static int sde_connector_init_debugfs(struct drm_connector *connector)
{
	return 0;
}
#endif

static int sde_connector_late_register(struct drm_connector *connector)
{
	return sde_connector_init_debugfs(connector);
}

static void sde_connector_early_unregister(struct drm_connector *connector)
{
	/* debugfs under connector->debugfs are deleted by drm_debugfs */
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
	.late_register =          sde_connector_late_register,
	.early_unregister =       sde_connector_early_unregister,
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

static void sde_connector_check_status_work(struct work_struct *work)
{
	struct sde_connector *conn;
	struct drm_event event;
	int rc = 0;
	bool panel_dead = false;

	conn = container_of(to_delayed_work(work),
			struct sde_connector, status_work);
	if (!conn) {
		SDE_ERROR("not able to get connector object\n");
		return;
	}

	mutex_lock(&conn->lock);
	if (!conn->ops.check_status ||
			(conn->dpms_mode != DRM_MODE_DPMS_ON)) {
		SDE_DEBUG("dpms mode: %d\n", conn->dpms_mode);
		mutex_unlock(&conn->lock);
		return;
	}

	rc = conn->ops.check_status(conn->display);
	mutex_unlock(&conn->lock);
	if (rc > 0) {
		SDE_DEBUG("esd check status success conn_id: %d enc_id: %d\n",
				conn->base.base.id, conn->encoder->base.id);
		schedule_delayed_work(&conn->status_work,
			msecs_to_jiffies(STATUS_CHECK_INTERVAL_MS));
	} else {
		SDE_EVT32(rc, SDE_EVTLOG_ERROR);
		SDE_ERROR("failed report PANEL_DEAD conn_id: %d enc_id: %d\n",
				conn->base.base.id, conn->encoder->base.id);
		/* report panel dead event */
		panel_dead = true;
		event.type = DRM_EVENT_PANEL_DEAD;
		event.length = sizeof(u32);
		msm_mode_object_event_notify(&conn->base.base,
			conn->base.dev, &event, (u8 *)&panel_dead);
	}
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
	struct dsi_display *dsi_display;
	struct msm_display_info display_info;
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

	memset(&display_info, 0, sizeof(display_info));

	rc = drm_connector_init(dev,
			&c_conn->base,
			&sde_connector_ops,
			connector_type);
	if (rc)
		goto error_free_conn;

	spin_lock_init(&c_conn->event_lock);

	c_conn->connector_type = connector_type;
	c_conn->encoder = encoder;
	c_conn->panel = panel;
	c_conn->display = display;

	c_conn->dpms_mode = DRM_MODE_DPMS_ON;
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

	rc = drm_mode_connector_attach_encoder(&c_conn->base, encoder);
	if (rc) {
		SDE_ERROR("failed to attach encoder to connector, %d\n", rc);
		goto error_cleanup_fence;
	}

	rc = sde_backlight_setup(c_conn, dev);
	if (rc) {
		SDE_ERROR("failed to setup backlight, rc=%d\n", rc);
		goto error_cleanup_fence;
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
			goto error_cleanup_fence;
		}

		sde_kms_info_reset(info);
		rc = c_conn->ops.post_init(&c_conn->base, info, display);
		if (rc) {
			SDE_ERROR("post-init failed, %d\n", rc);
			kfree(info);
			goto error_cleanup_fence;
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

	if (connector_type == DRM_MODE_CONNECTOR_DSI) {
		dsi_display = (struct dsi_display *)(display);
		if (dsi_display && dsi_display->panel &&
			dsi_display->panel->hdr_props.hdr_enabled == true) {
			msm_property_install_blob(&c_conn->property_info,
				"hdr_properties",
				DRM_MODE_PROP_IMMUTABLE,
				CONNECTOR_PROP_HDR_INFO);

			msm_property_set_blob(&c_conn->property_info,
				&c_conn->blob_hdr,
				&dsi_display->panel->hdr_props,
				sizeof(dsi_display->panel->hdr_props),
				CONNECTOR_PROP_HDR_INFO);
		}
	}

	rc = sde_connector_get_info(&c_conn->base, &display_info);
	if (!rc && display_info.roi_caps.enabled) {
		msm_property_install_volatile_range(
				&c_conn->property_info, "sde_drm_roi_v1", 0x0,
				0, ~0, 0, CONNECTOR_PROP_ROI_V1);
	}
	/* install PP_DITHER properties */
	_sde_connector_install_dither_property(dev, sde_kms, c_conn);

	msm_property_install_range(&c_conn->property_info, "RETIRE_FENCE",
			0x0, 0, INR_OPEN_MAX, 0, CONNECTOR_PROP_RETIRE_FENCE);

	msm_property_install_range(&c_conn->property_info, "autorefresh",
			0x0, 0, AUTOREFRESH_MAX_FRAME_CNT, 0,
			CONNECTOR_PROP_AUTOREFRESH);

	msm_property_install_range(&c_conn->property_info, "bl_scale",
		0x0, 0, MAX_BL_SCALE_LEVEL, MAX_BL_SCALE_LEVEL,
		CONNECTOR_PROP_BL_SCALE);

	msm_property_install_range(&c_conn->property_info, "ad_bl_scale",
		0x0, 0, MAX_AD_BL_SCALE_LEVEL, MAX_AD_BL_SCALE_LEVEL,
		CONNECTOR_PROP_AD_BL_SCALE);

	/* enum/bitmask properties */
	msm_property_install_enum(&c_conn->property_info, "topology_name",
			DRM_MODE_PROP_IMMUTABLE, 0, e_topology_name,
			ARRAY_SIZE(e_topology_name),
			CONNECTOR_PROP_TOPOLOGY_NAME);
	msm_property_install_enum(&c_conn->property_info, "topology_control",
			0, 1, e_topology_control,
			ARRAY_SIZE(e_topology_control),
			CONNECTOR_PROP_TOPOLOGY_CONTROL);
	msm_property_install_enum(&c_conn->property_info, "LP",
			0, 0, e_power_mode,
			ARRAY_SIZE(e_power_mode),
			CONNECTOR_PROP_LP);

	rc = msm_property_install_get_status(&c_conn->property_info);
	if (rc) {
		SDE_ERROR("failed to create one or more properties\n");
		goto error_destroy_property;
	}

	SDE_DEBUG("connector %d attach encoder %d\n",
			c_conn->base.base.id, encoder->base.id);

	priv->connectors[priv->num_connectors++] = &c_conn->base;

	INIT_DELAYED_WORK(&c_conn->status_work,
			sde_connector_check_status_work);

	return &c_conn->base;

error_destroy_property:
	if (c_conn->blob_caps)
		drm_property_unreference_blob(c_conn->blob_caps);
	if (c_conn->blob_hdr)
		drm_property_unreference_blob(c_conn->blob_hdr);
	if (c_conn->blob_dither)
		drm_property_unreference_blob(c_conn->blob_dither);

	msm_property_destroy(&c_conn->property_info);
error_cleanup_fence:
	mutex_destroy(&c_conn->lock);
	sde_fence_deinit(&c_conn->retire_fence);
error_cleanup_conn:
	drm_connector_cleanup(&c_conn->base);
error_free_conn:
	kfree(c_conn);

	return ERR_PTR(rc);
}

int sde_connector_register_custom_event(struct sde_kms *kms,
		struct drm_connector *conn_drm, u32 event, bool val)
{
	int ret = -EINVAL;

	switch (event) {
	case DRM_EVENT_SYS_BACKLIGHT:
		ret = 0;
		break;
	case DRM_EVENT_PANEL_DEAD:
		ret = 0;
		break;
	default:
		break;
	}
	return ret;
}
