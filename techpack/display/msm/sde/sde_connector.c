// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__
#include "msm_drv.h"
#include "sde_dbg.h"

#include "sde_kms.h"
#include "sde_connector.h"
#include "sde_encoder.h"
#include <linux/backlight.h>
#include <linux/string.h>
#include "dsi_drm.h"
#include "dsi_defs.h"
#include "dsi_display.h"
#include "sde_crtc.h"
#include "sde_rm.h"
#include "sde_trace.h"
#include "dsi_mi_feature.h"
#include "dsi_display.h"
#include "dsi_panel_mi.h"
#include "clone_cooling_device.h"

#define BL_NODE_NAME_SIZE 32
#define HDR10_PLUS_VSIF_TYPE_CODE      0x81

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
	{SDE_RM_TOPCTL_DS,		"ds"},
};
static const struct drm_prop_enum_list e_power_mode[] = {
	{SDE_MODE_DPMS_ON,	"ON"},
	{SDE_MODE_DPMS_LP1,	"LP1"},
	{SDE_MODE_DPMS_LP2,	"LP2"},
	{SDE_MODE_DPMS_OFF,	"OFF"},
};
static const struct drm_prop_enum_list e_qsync_mode[] = {
	{SDE_RM_QSYNC_DISABLED,	"none"},
	{SDE_RM_QSYNC_CONTINUOUS_MODE,	"continuous"},
	{SDE_RM_QSYNC_ONE_SHOT_MODE,	"one_shot"},
};
static const struct drm_prop_enum_list e_frame_trigger_mode[] = {
	{FRAME_DONE_WAIT_DEFAULT, "default"},
	{FRAME_DONE_WAIT_SERIALIZE, "serialize_frame_trigger"},
	{FRAME_DONE_WAIT_POSTED_START, "posted_start"},
};

static int sde_backlight_device_update_status(struct backlight_device *bd)
{
	int brightness;
	struct dsi_display *display;
	struct sde_connector *c_conn;
	int bl_lvl;
	struct drm_event event;
	int rc = 0;

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

	if (!c_conn->allow_bl_update) {
		c_conn->unset_bl_level = bl_lvl;
		return 0;
	}

	if (c_conn->ops.set_backlight) {
		/* skip notifying user space if bl is 0 */
		if (brightness != 0) {
			event.type = DRM_EVENT_SYS_BACKLIGHT;
			event.length = sizeof(u32);
			msm_mode_object_event_notify(&c_conn->base.base,
				c_conn->base.dev, &event, (u8 *)&brightness);
		}
		rc = c_conn->ops.set_backlight(&c_conn->base,
				c_conn->display, bl_lvl);
		c_conn->unset_bl_level = 0;
		c_conn->mi_dimlayer_state.current_backlight = bl_lvl;
	}

	return rc;
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
	int rc = 0;

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
	props.brightness = bl_config->brightness_init_level;
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
	rc = sde_backlight_clone_setup(c_conn, dev->dev, c_conn->bl_device);
	if (rc) {
		SDE_ERROR("Failed to register backlight_clone_cdev: %ld\n",
				    PTR_ERR(c_conn->cdev_clone));
		backlight_clone_cdev_unregister(c_conn->cdev_clone);
		backlight_device_unregister(c_conn->bl_device);
		c_conn->bl_device = NULL;
		return -ENODEV;
	}
	return 0;
}

int sde_connector_trigger_event(void *drm_connector,
		uint32_t event_idx, uint32_t instance_idx,
		uint32_t data0, uint32_t data1,
		uint32_t data2, uint32_t data3)
{
	struct sde_connector *c_conn;
	unsigned long irq_flags;
	int (*cb_func)(uint32_t event_idx,
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
		rc = cb_func(event_idx, instance_idx, usr,
			data0, data1, data2, data3);
	else
		rc = -EAGAIN;

	return rc;
}

int sde_connector_register_event(struct drm_connector *connector,
		uint32_t event_idx,
		int (*cb_func)(uint32_t event_idx,
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

	dst_format = c_conn->ops.get_dst_format(&c_conn->base, c_conn->display);
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
	u32 *p = (u32 *)cfg;

	if (!conn || !state || !p)
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
		*cfg = c_conn->blob_dither->data;
		dither_sz = c_conn->blob_dither->length;
	}
	*len = dither_sz;
	return 0;
}

static void sde_connector_get_avail_res_info(struct drm_connector *conn,
		struct msm_resource_caps_info *avail_res)
{
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct drm_encoder *drm_enc = NULL;
	struct msm_display_info display_info;

	if (!conn || !conn->dev || !conn->dev->dev_private)
		return;

	priv = conn->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);

	if (!sde_kms)
		return;

	memset(&display_info, 0, sizeof(display_info));

	if (conn->state && conn->state->best_encoder)
		drm_enc = conn->state->best_encoder;
	else
		drm_enc = conn->encoder;

	sde_connector_get_info(conn, &display_info);

	sde_rm_get_resource_info(&sde_kms->rm, drm_enc, avail_res,
					 display_info.display_type);

	avail_res->max_mixer_width = sde_kms->catalog->max_mixer_width;
}

int sde_connector_get_mode_info(struct drm_connector *conn,
		const struct drm_display_mode *drm_mode,
		struct msm_mode_info *mode_info)
{
	struct sde_connector *sde_conn;
	struct msm_resource_caps_info avail_res;

	memset(&avail_res, 0, sizeof(avail_res));

	sde_conn = to_sde_connector(conn);

	if (!sde_conn)
		return -EINVAL;

	sde_connector_get_avail_res_info(conn, &avail_res);

	return sde_conn->ops.get_mode_info(conn, drm_mode,
			mode_info, sde_conn->display, &avail_res);
}

int sde_connector_state_get_mode_info(struct drm_connector_state *conn_state,
	struct msm_mode_info *mode_info)
{
	struct sde_connector_state *sde_conn_state = NULL;

	if (!conn_state || !mode_info) {
		SDE_ERROR("Invalid arguments\n");
		return -EINVAL;
	}

	sde_conn_state = to_sde_connector_state(conn_state);
	memcpy(mode_info, &sde_conn_state->mode_info,
		sizeof(sde_conn_state->mode_info));

	return 0;
}

static int sde_connector_handle_disp_recovery(uint32_t event_idx,
			uint32_t instance_idx, void *usr,
			uint32_t data0, uint32_t data1,
			uint32_t data2, uint32_t data3)
{
	struct sde_connector *c_conn = usr;
	int rc = 0;

	if (!c_conn)
		return -EINVAL;

	rc = sde_kms_handle_recovery(c_conn->encoder);

	return rc;
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

	return c_conn->ops.get_info(&c_conn->base, info, c_conn->display);
}

void sde_connector_schedule_status_work(struct drm_connector *connector,
		bool en)
{
	struct sde_connector *c_conn;
	struct msm_display_info info;

	c_conn = to_sde_connector(connector);
	if (!c_conn)
		return;

	/* Return if there is no change in ESD status check condition */
	if (en == c_conn->esd_status_check)
		return;

	sde_connector_get_info(connector, &info);
	if (c_conn->ops.check_status &&
		(info.capabilities & MSM_DISPLAY_ESD_ENABLED)) {
		if (en) {
			u32 interval;

			/*
			 * If debugfs property is not set then take
			 * default value
			 */
			interval = c_conn->esd_status_interval ?
				c_conn->esd_status_interval :
					STATUS_CHECK_INTERVAL_MS;
			/* Schedule ESD status check */
			schedule_delayed_work(&c_conn->status_work,
				msecs_to_jiffies(interval));
			c_conn->esd_status_check = true;
		} else {
			/* Cancel any pending ESD status check */
			cancel_delayed_work_sync(&c_conn->status_work);
			c_conn->esd_status_check = false;
		}
	}
}

static int _sde_connector_update_power_locked(struct sde_connector *c_conn)
{
	struct drm_connector *connector;
	void *display;
	int (*set_power)(struct drm_connector *conn, int status, void *disp);
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

	if (SDE_MODE_DPMS_OFF == mode)
		c_conn->fod_frame_count = 0;

	if (mode != c_conn->last_panel_power_mode && c_conn->ops.set_power) {
		display = c_conn->display;
		set_power = c_conn->ops.set_power;

		mutex_unlock(&c_conn->lock);
		rc = set_power(connector, mode, display);
		mutex_lock(&c_conn->lock);
	}
	c_conn->last_panel_power_mode = mode;

	mutex_unlock(&c_conn->lock);
	if (mode != SDE_MODE_DPMS_ON)
		sde_connector_schedule_status_work(connector, false);
	else
		sde_connector_schedule_status_work(connector, true);
	mutex_lock(&c_conn->lock);

	return rc;
}

static int _sde_connector_update_bl_scale(struct sde_connector *c_conn)
{
	struct dsi_display *dsi_display;
	struct dsi_backlight_config *bl_config;
	int rc = 0;

	if (!c_conn) {
		SDE_ERROR("Invalid params sde_connector null\n");
		return -EINVAL;
	}

	dsi_display = c_conn->display;
	if (!dsi_display || !dsi_display->panel) {
		SDE_ERROR("Invalid params(s) dsi_display %pK, panel %pK\n",
			dsi_display,
			((dsi_display) ? dsi_display->panel : NULL));
		return -EINVAL;
	}

	bl_config = &dsi_display->panel->bl_config;

	if (!c_conn->allow_bl_update) {
		c_conn->unset_bl_level = bl_config->bl_level;
		return 0;
	}

	if (c_conn->unset_bl_level)
		bl_config->bl_level = c_conn->unset_bl_level;

	bl_config->bl_scale = c_conn->bl_scale > MAX_BL_SCALE_LEVEL ?
			MAX_BL_SCALE_LEVEL : c_conn->bl_scale;
	bl_config->bl_scale_sv = c_conn->bl_scale_sv > MAX_SV_BL_SCALE_LEVEL ?
			MAX_SV_BL_SCALE_LEVEL : c_conn->bl_scale_sv;

	SDE_DEBUG("bl_scale = %u, bl_scale_sv = %u, bl_level = %u\n",
		bl_config->bl_scale, bl_config->bl_scale_sv,
		bl_config->bl_level);
	rc = c_conn->ops.set_backlight(&c_conn->base,
			dsi_display, bl_config->bl_level);
	c_conn->unset_bl_level = 0;

	return rc;
}

void sde_connector_set_colorspace(struct sde_connector *c_conn)
{
	int rc = 0;

	if (c_conn->ops.set_colorspace)
		rc = c_conn->ops.set_colorspace(&c_conn->base,
			c_conn->display);

	if (rc)
		SDE_ERROR_CONN(c_conn, "cannot apply new colorspace %d\n", rc);

}

void sde_connector_set_qsync_params(struct drm_connector *connector)
{
	struct sde_connector *c_conn;
	struct sde_connector_state *c_state;
	u32 qsync_propval = 0;
	bool prop_dirty;

	if (!connector)
		return;

	c_conn = to_sde_connector(connector);
	c_state = to_sde_connector_state(connector->state);
	c_conn->qsync_updated = false;

	prop_dirty = msm_property_is_dirty(&c_conn->property_info,
					&c_state->property_state,
					CONNECTOR_PROP_QSYNC_MODE);
	if (prop_dirty) {
		qsync_propval = sde_connector_get_property(c_conn->base.state,
						CONNECTOR_PROP_QSYNC_MODE);
		if (qsync_propval != c_conn->qsync_mode) {
			SDE_DEBUG("updated qsync mode %d -> %d\n",
					c_conn->qsync_mode, qsync_propval);
			c_conn->qsync_updated = true;
			c_conn->qsync_mode = qsync_propval;
		}
	}
}

void sde_connector_complete_qsync_commit(struct drm_connector *conn,
				struct msm_display_conn_params *params)
{
	struct sde_connector *c_conn;

	if (!conn || !params) {
		SDE_ERROR("invalid params\n");
		return;
	}

	c_conn = to_sde_connector(conn);

	if (c_conn && c_conn->qsync_updated &&
		(c_conn->qsync_mode == SDE_RM_QSYNC_ONE_SHOT_MODE)) {
		/* Reset qsync states if mode is one shot */
		params->qsync_mode = c_conn->qsync_mode = 0;
		params->qsync_update = true;
		SDE_EVT32(conn->base.id, c_conn->qsync_mode);
	}
}

static int _sde_connector_update_hdr_metadata(struct sde_connector *c_conn,
		struct sde_connector_state *c_state)
{
	int rc = 0;

	if (c_conn->ops.config_hdr)
		rc = c_conn->ops.config_hdr(&c_conn->base, c_conn->display,
				c_state);

	if (rc)
		SDE_ERROR_CONN(c_conn, "cannot apply hdr metadata %d\n", rc);

	SDE_DEBUG_CONN(c_conn, "updated hdr metadata: %d\n", rc);
	return rc;
}

static int _sde_connector_update_dirty_properties(
				struct drm_connector *connector)
{
	struct sde_connector *c_conn;
	struct sde_connector_state *c_state;
	int idx;

	if (!connector) {
		SDE_ERROR("invalid argument\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);
	c_state = to_sde_connector_state(connector->state);

	mutex_lock(&c_conn->property_info.property_lock);
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
		case CONNECTOR_PROP_BL_SCALE:
		case CONNECTOR_PROP_SV_BL_SCALE:
			_sde_connector_update_bl_scale(c_conn);
			break;
		case CONNECTOR_PROP_HDR_METADATA:
			_sde_connector_update_hdr_metadata(c_conn, c_state);
			break;
		default:
			/* nothing to do for most properties */
			break;
		}
	}
	mutex_unlock(&c_conn->property_info.property_lock);

	/* if colorspace needs to be updated do it first */
	if (c_conn->colorspace_updated) {
		c_conn->colorspace_updated = false;
		sde_connector_set_colorspace(c_conn);
	}

	/*
	 * Special handling for postproc properties and
	 * for updating backlight if any unset backlight level is present
	 */
	if (c_conn->bl_scale_dirty || c_conn->unset_bl_level) {
		_sde_connector_update_bl_scale(c_conn);
		c_conn->bl_scale_dirty = false;
	}

	return 0;
}

struct sde_connector_dyn_hdr_metadata *sde_connector_get_dyn_hdr_meta(
		struct drm_connector *connector)
{
	struct sde_connector_state *c_state;

	if (!connector)
		return NULL;

	c_state = to_sde_connector_state(connector->state);
	return &c_state->dyn_hdr_meta;
}

void sde_crtc_fod_ui_ready(struct dsi_display *display, int type, int value)
{
	if (!display)
		return;

	if (type == 1) /* HBM */
	{
		if (value == 0)
			display->panel->mi_cfg.fod_ui_ready &= ~0x01;
		else if (value == 1)
			display->panel->mi_cfg.fod_ui_ready |= 0x01;
	}

	if (type == 2) /* ICON */
	{
		if (value == 0)
			display->panel->mi_cfg.fod_ui_ready &= ~0x02;
		else if (value == 1)
			display->panel->mi_cfg.fod_ui_ready |= 0x02;

	}

	SDE_INFO("fod_ui_ready notify=%d", display->panel->mi_cfg.fod_ui_ready);
	sysfs_notify(&display->drm_conn->kdev->kobj, NULL, "fod_ui_ready");
}

static int _sde_connector_mi_dimlayer_hbm_fence(struct drm_connector *connector)
{
	int rc = 0;
	struct sde_connector *c_conn;
	struct dsi_display *dsi_display;
	bool skip = false;
	static int skip_frame_count = 0;
	bool hbm_overlay;
	static bool last_fod_unlock_success;
	static bool last_layer_aod_flag;
	bool crc_off_after_delay_of_hbm_on = false;
	struct dsi_panel_mi_cfg *mi_cfg;
#if 0
	bool icon;
	static bool last_icon = false;
#endif
	if (!connector) {
		SDE_ERROR("invalid argument\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);

	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI)
		return 0;

	dsi_display = (struct dsi_display *) c_conn->display;
	if (!dsi_display || !dsi_display->panel) {
		SDE_ERROR("invalid display/panel\n");
		return -EINVAL;
	}

	mi_cfg = &dsi_display->panel->mi_cfg;
	if (!mi_cfg) {
		SDE_ERROR("invalid mi_cfg\n");
		return -EINVAL;
	}

	if (!c_conn->allow_bl_update) {
		/*Skip 2 frames after panel on to avoid hbm flicker*/
		if (mi_cfg->dc_type == 1 && dsi_display->panel->power_mode == SDE_MODE_DPMS_ON)
			skip_frame_count = 2;
		return 0;
	}

	if (skip_frame_count) {
		SDE_INFO("skip_frame_count=%d\n", skip_frame_count);
		skip_frame_count--;
		return 0;
	}

	mi_cfg->layer_fod_unlock_success =
			c_conn->mi_dimlayer_state.mi_dimlayer_type & MI_FOD_UNLOCK_SUCCESS;
	if (last_fod_unlock_success != mi_cfg->layer_fod_unlock_success)
		SDE_INFO("layer_fod_unlock_success = %d\n",
					mi_cfg->layer_fod_unlock_success);

	last_fod_unlock_success = mi_cfg->layer_fod_unlock_success;

	mi_cfg->layer_aod_flag = c_conn->mi_dimlayer_state.mi_dimlayer_type & MI_DIMLAYER_AOD;
	if (last_layer_aod_flag != mi_cfg->layer_aod_flag)
		SDE_INFO("layer_aod_flag = %d\n", mi_cfg->layer_aod_flag);

	last_layer_aod_flag = mi_cfg->layer_aod_flag;

	hbm_overlay = c_conn->mi_dimlayer_state.mi_dimlayer_type & MI_DIMLAYER_FOD_HBM_OVERLAY;
	if (hbm_overlay) {
		/* TODO: mutex_lock(&panel->panel_lock); */
		if (mi_cfg->fod_hbm_layer_enabled == false) {
			/* in AOD, first frame should be skipped for hardware limitation */
			if (mi_cfg->dc_type != 2 &&
					(dsi_display->panel->power_mode == SDE_MODE_DPMS_LP1 ||
					 dsi_display->panel->power_mode == SDE_MODE_DPMS_LP2)) {
				SDE_INFO("fod_frame_count=%d\n", c_conn->fod_frame_count);
				if (c_conn->fod_frame_count == 0)
					skip = true;

				c_conn->fod_frame_count++;
			} else {
				c_conn->fod_frame_count = 0;
			}
			if (skip == false) {
				/* dimming off before hbm ctl */
				if (mi_cfg->prepare_before_fod_hbm_on && ((mi_cfg->panel_id >> 8) == 0x4A3232003808)) {
					/* Set flags to disable dimming and backlight */
					mi_cfg->dimming_state = STATE_DIM_BLOCK;
					mi_cfg->fod_hbm_enabled = true;

					sde_connector_pre_hbm_ctl(connector);
					sde_encoder_wait_for_event(c_conn->encoder, MSM_ENC_VBLANK);
				}

				if (mi_cfg->delay_before_fod_hbm_on)
					sde_encoder_wait_for_event(c_conn->encoder, MSM_ENC_VBLANK);

				sde_connector_hbm_ctl(connector, DISPPARAM_HBM_FOD_ON);

				/* Send crc off cmd before delay only if DC off(MIUI-1755728) */
				if (mi_cfg->dc_type == 2) {
					if (!mi_cfg->dc_enable || (mi_cfg->dc_enable &&
						mi_cfg->last_bl_level > mi_cfg->dc_threshold)) {
						dsi_panel_acquire_panel_lock(dsi_display->panel);
						rc = dsi_panel_tx_cmd_set(dsi_display->panel,
								DSI_CMD_SET_MI_CRC_OFF);
						dsi_panel_release_panel_lock(dsi_display->panel);
					} else {
						crc_off_after_delay_of_hbm_on = true;
					}
				}

				if (mi_cfg->delay_after_fod_hbm_on) {
					sde_encoder_wait_for_event(c_conn->encoder, MSM_ENC_VBLANK);
				}

				/* Turn off crc after delay of hbm on can avoid flash high
				 * brightness if DC on (MIUI-1755728) */
				if (mi_cfg->dc_type == 2 && crc_off_after_delay_of_hbm_on) {
					dsi_panel_acquire_panel_lock(dsi_display->panel);
					rc = dsi_panel_tx_cmd_set(dsi_display->panel,
							DSI_CMD_SET_MI_CRC_OFF);
					dsi_panel_release_panel_lock(dsi_display->panel);
				}

				mi_cfg->fod_hbm_layer_enabled = true;
				/*sde_crtc_fod_ui_ready(dsi_display, 1, 1);*/
			}
		}
	} else {
		if (mi_cfg->fod_hbm_layer_enabled == true) {
			SDE_INFO("layer_fod_unlock_success = %d, sysfs_fod_unlock_success = %d\n",
					mi_cfg->layer_fod_unlock_success,
					mi_cfg->sysfs_fod_unlock_success);
			if (mi_cfg->delay_before_fod_hbm_off)
				sde_encoder_wait_for_event(c_conn->encoder, MSM_ENC_VBLANK);
			sde_connector_hbm_ctl(connector, DISPPARAM_HBM_FOD_OFF);
			if (mi_cfg->dc_type)
				sysfs_notify(&c_conn->bl_device->dev.kobj, NULL, "brightness_clone");
			if (mi_cfg->delay_after_fod_hbm_off)
				sde_encoder_wait_for_event(c_conn->encoder, MSM_ENC_VBLANK);

			mi_cfg->fod_hbm_layer_enabled = false;
			/*sde_crtc_fod_ui_ready(dsi_display, 1, 0);*/
		}
	}
#if 0
	icon = c_conn->mi_dimlayer_state.mi_dimlayer_type & MI_DIMLAYER_FOD_ICON;
	if (last_icon != icon) {
		if (icon) {
			sde_crtc_fod_ui_ready(dsi_display, 2, 1);
		} else {
			if (last_icon)
				sde_crtc_fod_ui_ready(dsi_display, 2, 0);
		}
	}
	last_icon = icon;
#endif
	return rc;
}

void sde_connector_fod_notify(struct drm_connector *conn)
{
	struct sde_connector *c_conn;
	bool icon, hbm_state;
	static bool last_icon = false;
	static bool last_hbm_state = false;
	struct dsi_display *dsi_display;

	if (!conn) {
		SDE_ERROR("invalid params\n");
		return;
	}

	c_conn = to_sde_connector(conn);
	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI) {
		SDE_ERROR("not DRM_MODE_CONNECTOR_DSIl\n");
		return;
	}

	dsi_display = (struct dsi_display *) c_conn->display;
	if (!dsi_display || !dsi_display->panel) {
		SDE_ERROR("invalid display/panel\n");
		return;
	}

	icon = c_conn->mi_dimlayer_state.mi_dimlayer_type & MI_DIMLAYER_FOD_ICON;
	if (last_icon != icon) {
		if (icon) {
			/* Make sure icon was displayed on panel before notifying
			 * fingerprint to capture image */
			if (dsi_display->panel->mi_cfg.fod_hbm_layer_enabled) {
				sde_encoder_wait_for_event(c_conn->encoder,
						MSM_ENC_TX_COMPLETE);
			}

			sde_crtc_fod_ui_ready(dsi_display, 2, 1);
		} else {
			sde_crtc_fod_ui_ready(dsi_display, 2, 0);
		}
	}
	last_icon = icon;

	hbm_state = dsi_display->panel->mi_cfg.fod_hbm_layer_enabled;
	if (last_hbm_state != hbm_state) {
		if (hbm_state) {
		   /* The black screen fingerprint unlocks, waits for HBM effect */
			if (icon) {
				sde_encoder_wait_for_event(c_conn->encoder,
						MSM_ENC_TX_COMPLETE);
			}
			sde_crtc_fod_ui_ready(dsi_display, 1, 1);
		} else {
			sde_crtc_fod_ui_ready(dsi_display, 1, 0);
		}
	}
	last_hbm_state = hbm_state;
}

int sde_connector_pre_kickoff(struct drm_connector *connector)
{
	struct sde_connector *c_conn;
	struct sde_connector_state *c_state;
	struct msm_display_kickoff_params params;
	struct dsi_display *display;
	int rc;

	if (!connector) {
		SDE_ERROR("invalid argument\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);
	c_state = to_sde_connector_state(connector->state);
	if (!c_conn->display) {
		SDE_ERROR("invalid connector display\n");
		return -EINVAL;
	}

	/*
	 * During pre kickoff DCS commands have to have an
	 * asynchronous wait to avoid an unnecessary stall
	 * in pre-kickoff. This flag must be reset at the
	 * end of display pre-kickoff.
	 */
	if (c_conn->connector_type == DRM_MODE_CONNECTOR_DSI) {
		display = (struct dsi_display *)c_conn->display;
		display->queue_cmd_waits = true;
	}

	rc = _sde_connector_update_dirty_properties(connector);
	if (rc) {
		SDE_EVT32(connector->base.id, SDE_EVTLOG_ERROR);
		goto end;
	}

	if (!c_conn->ops.pre_kickoff)
		return 0;

	params.rois = &c_state->rois;
	params.hdr_meta = &c_state->hdr_meta;

	SDE_EVT32_VERBOSE(connector->base.id);

	/* fingerprint hbm fence */
	_sde_connector_mi_dimlayer_hbm_fence(connector);

	rc = c_conn->ops.pre_kickoff(connector, c_conn->display, &params);

	if (c_conn->connector_type == DRM_MODE_CONNECTOR_DSI)
		display->queue_cmd_waits = false;
end:
	return rc;
}

int sde_connector_prepare_commit(struct drm_connector *connector)
{
	struct sde_connector *c_conn;
	struct sde_connector_state *c_state;
	struct msm_display_conn_params params;
	int rc;

	if (!connector) {
		SDE_ERROR("invalid argument\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);
	c_state = to_sde_connector_state(connector->state);
	if (!c_conn->display) {
		SDE_ERROR("invalid connector display\n");
		return -EINVAL;
	}

	if (!c_conn->ops.prepare_commit)
		return 0;

	memset(&params, 0, sizeof(params));

	if (c_conn->qsync_updated) {
		params.qsync_mode = c_conn->qsync_mode;
		params.qsync_update = true;
	}

	rc = c_conn->ops.prepare_commit(c_conn->display, &params);

	SDE_EVT32(connector->base.id, params.qsync_mode,
		  params.qsync_update, rc);

	return rc;
}

void sde_connector_helper_bridge_disable(struct drm_connector *connector)
{
	int rc;
	struct sde_connector *c_conn = NULL;
	struct dsi_display *display;
	bool poms_pending = false;

	if (!connector)
		return;

	c_conn = to_sde_connector(connector);
	if (c_conn->connector_type == DRM_MODE_CONNECTOR_DSI) {
		display = (struct dsi_display *) c_conn->display;
		poms_pending = display->poms_pending;
	}

	if (!poms_pending) {
		rc = _sde_connector_update_dirty_properties(connector);
		if (rc) {
			SDE_ERROR("conn %d final pre kickoff failed %d\n",
					connector->base.id, rc);
			SDE_EVT32(connector->base.id, SDE_EVTLOG_ERROR);
		}
	}
	/* Disable ESD thread */
	sde_connector_schedule_status_work(connector, false);

	if (c_conn->bl_device) {
		c_conn->bl_device->props.power = FB_BLANK_POWERDOWN;
		c_conn->bl_device->props.state |= BL_CORE_FBBLANK;
		backlight_update_status(c_conn->bl_device);
	}

	c_conn->allow_bl_update = false;
}

void sde_connector_helper_bridge_enable(struct drm_connector *connector)
{
	struct sde_connector *c_conn = NULL;
	struct dsi_display *display;

	if (!connector)
		return;

	c_conn = to_sde_connector(connector);
	display = (struct dsi_display *) c_conn->display;

	/*
	 * Special handling for some panels which need atleast
	 * one frame to be transferred to GRAM before enabling backlight.
	 * So delay backlight update to these panels until the
	 * first frame commit is received from the HW.
	 */
	if (display->panel->bl_config.bl_update ==
				BL_UPDATE_DELAY_UNTIL_FIRST_FRAME)
		sde_encoder_wait_for_event(c_conn->encoder,
				MSM_ENC_TX_COMPLETE);
	c_conn->allow_bl_update = true;

	if (c_conn->bl_device) {
		c_conn->bl_device->props.power = FB_BLANK_UNBLANK;
		c_conn->bl_device->props.state &= ~BL_CORE_FBBLANK;
	if (!(display->panel->cur_mode->dsi_mode_flags & DSI_MODE_FLAG_DMS)
		&& !(display->panel->cur_mode->dsi_mode_flags & DSI_MODE_FLAG_DMS_FPS))
		backlight_update_status(c_conn->bl_device);
	}
	c_conn->panel_dead = false;
}

int sde_connector_clk_ctrl(struct drm_connector *connector, bool enable)
{
	struct sde_connector *c_conn;
	struct dsi_display *display;
	u32 state = enable ? DSI_CLK_ON : DSI_CLK_OFF;
	int rc = 0;

	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);
	display = (struct dsi_display *) c_conn->display;

	if (display && c_conn->ops.clk_ctrl)
		rc = c_conn->ops.clk_ctrl(display->mdp_clk_handle,
				DSI_ALL_CLKS, state);

	return rc;
}

void sde_connector_destroy(struct drm_connector *connector)
{
	struct sde_connector *c_conn;

	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return;
	}

	c_conn = to_sde_connector(connector);

	/* cancel if any pending esd work */
	sde_connector_schedule_status_work(connector, false);

	if (c_conn->ops.pre_destroy)
		c_conn->ops.pre_destroy(connector, c_conn->display);

	if (c_conn->blob_caps)
		drm_property_blob_put(c_conn->blob_caps);
	if (c_conn->blob_hdr)
		drm_property_blob_put(c_conn->blob_hdr);
	if (c_conn->blob_dither)
		drm_property_blob_put(c_conn->blob_dither);
	if (c_conn->blob_mode_info)
		drm_property_blob_put(c_conn->blob_mode_info);
	if (c_conn->blob_ext_hdr)
		drm_property_blob_put(c_conn->blob_ext_hdr);
	if (c_conn->cdev_clone)
		backlight_clone_cdev_unregister(c_conn->cdev_clone);
	if (c_conn->bl_device)
		backlight_device_unregister(c_conn->bl_device);
	drm_connector_unregister(connector);
	mutex_destroy(&c_conn->lock);
	sde_fence_deinit(c_conn->retire_fence);
	drm_connector_cleanup(connector);
	msm_property_destroy(&c_conn->property_info);
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

	drm_framebuffer_put(c_state->out_fb);
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

	__drm_atomic_helper_connector_destroy_state(&c_state->base);

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

	if (connector->state &&
			!sde_crtc_is_reset_required(connector->state->crtc)) {
		SDE_DEBUG_CONN(c_conn, "avoid reset for connector\n");
		return;
	}

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

	__drm_atomic_helper_connector_reset(connector, &c_state->base);
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

	__drm_atomic_helper_connector_duplicate_state(connector,
			&c_state->base);

	/* additional handling for drm framebuffer objects */
	if (c_state->out_fb)
		drm_framebuffer_get(c_state->out_fb);

	/* clear dynamic HDR metadata from prev state */
	if (c_state->dyn_hdr_meta.dynamic_hdr_update) {
		c_state->dyn_hdr_meta.dynamic_hdr_update = false;
		c_state->dyn_hdr_meta.dynamic_hdr_payload_size = 0;
	}

	return &c_state->base;
}

int sde_connector_roi_v1_check_roi(struct drm_connector_state *conn_state)
{
	const struct msm_roi_alignment *align = NULL;
	struct sde_connector *c_conn = NULL;
	struct msm_mode_info mode_info;
	struct sde_connector_state *c_state;
	int i, w, h;

	if (!conn_state)
		return -EINVAL;

	memset(&mode_info, 0, sizeof(mode_info));

	c_state = to_sde_connector_state(conn_state);
	c_conn = to_sde_connector(conn_state->connector);

	memcpy(&mode_info, &c_state->mode_info, sizeof(c_state->mode_info));

	if (!mode_info.roi_caps.enabled)
		return 0;

	if (c_state->rois.num_rects > mode_info.roi_caps.num_roi) {
		SDE_ERROR_CONN(c_conn, "too many rects specified: %d > %d\n",
				c_state->rois.num_rects,
				mode_info.roi_caps.num_roi);
		return -E2BIG;
	}

	align = &mode_info.roi_caps.align;
	for (i = 0; i < c_state->rois.num_rects; ++i) {
		struct drm_clip_rect *roi_conn;

		roi_conn = &c_state->rois.roi[i];
		w = roi_conn->x2 - roi_conn->x1;
		h = roi_conn->y2 - roi_conn->y1;

		SDE_EVT32_VERBOSE(DRMID(&c_conn->base),
				roi_conn->x1, roi_conn->y1,
				roi_conn->x2, roi_conn->y2);

		if (w <= 0 || h <= 0) {
			SDE_ERROR_CONN(c_conn, "invalid conn roi w %d h %d\n",
					w, h);
			return -EINVAL;
		}

		if (w < align->min_width || w % align->width_pix_align) {
			SDE_ERROR_CONN(c_conn,
					"invalid conn roi width %d min %d align %d\n",
					w, align->min_width,
					align->width_pix_align);
			return -EINVAL;
		}

		if (h < align->min_height || h % align->height_pix_align) {
			SDE_ERROR_CONN(c_conn,
					"invalid conn roi height %d min %d align %d\n",
					h, align->min_height,
					align->height_pix_align);
			return -EINVAL;
		}

		if (roi_conn->x1 % align->xstart_pix_align) {
			SDE_ERROR_CONN(c_conn,
					"invalid conn roi x1 %d align %d\n",
					roi_conn->x1, align->xstart_pix_align);
			return -EINVAL;
		}

		if (roi_conn->y1 % align->ystart_pix_align) {
			SDE_ERROR_CONN(c_conn,
					"invalid conn roi y1 %d align %d\n",
					roi_conn->y1, align->ystart_pix_align);
			return -EINVAL;
		}
	}

	return 0;
}

static int _sde_connector_set_roi_v1(
		struct sde_connector *c_conn,
		struct sde_connector_state *c_state,
		void __user *usr_ptr)
{
	struct sde_drm_roi_v1 roi_v1;
	int i;

	if (!c_conn || !c_state) {
		SDE_ERROR("invalid args\n");
		return -EINVAL;
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

	if (roi_v1.num_rects > SDE_MAX_ROI_V1) {
		SDE_ERROR_CONN(c_conn, "num roi rects more than supported: %d",
				roi_v1.num_rects);
		return -EINVAL;
	}

	c_state->rois.num_rects = roi_v1.num_rects;
	for (i = 0; i < roi_v1.num_rects; ++i) {
		c_state->rois.roi[i] = roi_v1.roi[i];
		SDE_DEBUG_CONN(c_conn, "roi%d: roi (%d,%d) (%d,%d)\n", i,
				c_state->rois.roi[i].x1,
				c_state->rois.roi[i].y1,
				c_state->rois.roi[i].x2,
				c_state->rois.roi[i].y2);
	}

	return 0;
}

static int _sde_connector_set_ext_hdr_info(
	struct sde_connector *c_conn,
	struct sde_connector_state *c_state,
	void __user *usr_ptr)
{
	int rc = 0;
	struct drm_connector *connector;
	struct drm_msm_ext_hdr_metadata *hdr_meta;
	size_t payload_size = 0;
	u8 *payload = NULL;
	int i;

	if (!c_conn || !c_state) {
		SDE_ERROR_CONN(c_conn, "invalid args\n");
		rc = -EINVAL;
		goto end;
	}

	connector = &c_conn->base;

	memset(&c_state->hdr_meta, 0, sizeof(c_state->hdr_meta));

	if (!usr_ptr) {
		SDE_DEBUG_CONN(c_conn, "hdr metadata cleared\n");
		goto end;
	}

	if (!connector->hdr_supported) {
		SDE_ERROR_CONN(c_conn, "sink doesn't support HDR\n");
		rc = -ENOTSUPP;
		goto end;
	}

	if (copy_from_user(&c_state->hdr_meta,
		(void __user *)usr_ptr,
			sizeof(*hdr_meta))) {
		SDE_ERROR_CONN(c_conn, "failed to copy hdr metadata\n");
		rc = -EFAULT;
		goto end;
	}

	hdr_meta = &c_state->hdr_meta;

	/* dynamic metadata support */
	if (!hdr_meta->hdr_plus_payload_size || !hdr_meta->hdr_plus_payload)
		goto skip_dhdr;

	if (!connector->hdr_plus_app_ver) {
		SDE_ERROR_CONN(c_conn, "sink doesn't support dynamic HDR\n");
		rc = -ENOTSUPP;
		goto end;
	}

	payload_size = hdr_meta->hdr_plus_payload_size;
	if (payload_size > sizeof(c_state->dyn_hdr_meta.dynamic_hdr_payload)) {
		SDE_ERROR_CONN(c_conn, "payload size exceeds limit\n");
		rc = -EINVAL;
		goto end;
	}

	payload = c_state->dyn_hdr_meta.dynamic_hdr_payload;
	if (copy_from_user(payload,
			(void __user *)c_state->hdr_meta.hdr_plus_payload,
			payload_size)) {
		SDE_ERROR_CONN(c_conn, "failed to copy dhdr metadata\n");
		rc = -EFAULT;
		goto end;
	}

	/* verify 1st header byte, programmed in DP Infoframe SDP header */
	if (payload_size < 1 || (payload[0] != HDR10_PLUS_VSIF_TYPE_CODE)) {
		SDE_ERROR_CONN(c_conn, "invalid payload detected, size: %zd\n",
				payload_size);
		rc = -EINVAL;
		goto end;
	}

	c_state->dyn_hdr_meta.dynamic_hdr_update = true;

skip_dhdr:
	c_state->dyn_hdr_meta.dynamic_hdr_payload_size = payload_size;

	SDE_DEBUG_CONN(c_conn, "hdr_state %d\n", hdr_meta->hdr_state);
	SDE_DEBUG_CONN(c_conn, "hdr_supported %d\n", hdr_meta->hdr_supported);
	SDE_DEBUG_CONN(c_conn, "eotf %d\n", hdr_meta->eotf);
	SDE_DEBUG_CONN(c_conn, "white_point_x %d\n", hdr_meta->white_point_x);
	SDE_DEBUG_CONN(c_conn, "white_point_y %d\n", hdr_meta->white_point_y);
	SDE_DEBUG_CONN(c_conn, "max_luminance %d\n", hdr_meta->max_luminance);
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
	SDE_DEBUG_CONN(c_conn, "hdr_plus payload%s updated, size %d\n",
			c_state->dyn_hdr_meta.dynamic_hdr_update ? "" : " NOT",
			c_state->dyn_hdr_meta.dynamic_hdr_payload_size);

end:
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
	uint64_t fence_user_fd;
	uint64_t __user prev_user_fd;

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
			drm_framebuffer_lookup(connector->dev, NULL, val);
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
	case CONNECTOR_PROP_RETIRE_FENCE:
		if (!val)
			goto end;

		rc = copy_from_user(&prev_user_fd, (void __user *)val,
				sizeof(uint64_t));
		if (rc) {
			SDE_ERROR("copy from user failed rc:%d\n", rc);
			rc = -EFAULT;
			goto end;
		}

		/*
		 * client is expected to reset the property to -1 before
		 * requesting for the retire fence
		 */
		if (prev_user_fd == -1) {
			/*
			 * update the offset to a timeline for
			 * commit completion
			 */
			rc = sde_fence_create(c_conn->retire_fence,
						&fence_user_fd, 1);
			if (rc) {
				SDE_ERROR("fence create failed rc:%d\n", rc);
				goto end;
			}

			rc = copy_to_user((uint64_t __user *)(uintptr_t)val,
					&fence_user_fd, sizeof(uint64_t));
			if (rc) {
				SDE_ERROR("copy to user failed rc:%d\n", rc);
				/*
				 * fence will be released with timeline
				 * update
				 */
				put_unused_fd(fence_user_fd);
				rc = -EFAULT;
				goto end;
			}
		}
		break;
	case CONNECTOR_PROP_ROI_V1:
		rc = _sde_connector_set_roi_v1(c_conn, c_state,
				(void *)(uintptr_t)val);
		if (rc)
			SDE_ERROR_CONN(c_conn, "invalid roi_v1, rc: %d\n", rc);
		break;
	/* CONNECTOR_PROP_BL_SCALE and CONNECTOR_PROP_SV_BL_SCALE are
	 * color-processing properties. These two properties require
	 * special handling since they don't quite fit the current standard
	 * atomic set property framework.
	 */
	case CONNECTOR_PROP_BL_SCALE:
		//c_conn->bl_scale = val;
		c_conn->bl_scale = MAX_BL_SCALE_LEVEL;
		c_conn->bl_scale_dirty = true;
		break;
	case CONNECTOR_PROP_SV_BL_SCALE:
		//c_conn->bl_scale_sv = val;
		c_conn->bl_scale_sv = MAX_SV_BL_SCALE_LEVEL;
		c_conn->bl_scale_dirty = true;
		break;
	case CONNECTOR_PROP_HDR_METADATA:
		rc = _sde_connector_set_ext_hdr_info(c_conn,
			c_state, (void *)(uintptr_t)val);
		if (rc)
			SDE_ERROR_CONN(c_conn, "cannot set hdr info %d\n", rc);
		break;
	case CONNECTOR_PROP_QSYNC_MODE:
		msm_property_set_dirty(&c_conn->property_info,
				&c_state->property_state, idx);
		break;
	default:
		break;
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
				&c_state->property_state, property, val);
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

void sde_conn_timeline_status(struct drm_connector *conn)
{
	struct sde_connector *c_conn;

	if (!conn) {
		SDE_ERROR("invalid connector\n");
		return;
	}

	c_conn = to_sde_connector(conn);
	sde_fence_timeline_status(c_conn->retire_fence, &conn->base);
}

void sde_connector_prepare_fence(struct drm_connector *connector)
{
	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return;
	}

	sde_fence_prepare(to_sde_connector(connector)->retire_fence);
}

void sde_connector_complete_commit(struct drm_connector *connector,
		ktime_t ts, enum sde_fence_event fence_event)
{
	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return;
	}

	/* signal connector's retire fence */
	sde_fence_signal(to_sde_connector(connector)->retire_fence,
			ts, fence_event);
}

void sde_connector_commit_reset(struct drm_connector *connector, ktime_t ts)
{
	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return;
	}

	/* signal connector's retire fence */
	sde_fence_signal(to_sde_connector(connector)->retire_fence,
			ts, SDE_FENCE_RESET_TIMELINE);
}

static void sde_connector_update_hdr_props(struct drm_connector *connector)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct drm_msm_ext_hdr_properties hdr = {0};

	hdr.hdr_metadata_type_one = connector->hdr_metadata_type_one ? 1 : 0;
	hdr.hdr_supported = connector->hdr_supported ? 1 : 0;
	hdr.hdr_eotf = connector->hdr_eotf;
	hdr.hdr_max_luminance = connector->hdr_max_luminance;
	hdr.hdr_avg_luminance = connector->hdr_avg_luminance;
	hdr.hdr_min_luminance = connector->hdr_min_luminance;
	hdr.hdr_plus_supported = connector->hdr_plus_app_ver;

	msm_property_set_blob(&c_conn->property_info, &c_conn->blob_ext_hdr,
			&hdr, sizeof(hdr), CONNECTOR_PROP_EXT_HDR_INFO);
}

static void sde_connector_update_colorspace(struct drm_connector *connector)
{
	int ret;

	ret = msm_property_set_property(
			sde_connector_get_propinfo(connector),
			sde_connector_get_property_state(connector->state),
			CONNECTOR_PROP_SUPPORTED_COLORSPACES,
				connector->color_enc_fmt);

	if (ret)
		SDE_ERROR("failed to set colorspace property for connector\n");
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

	return sde_connector_atomic_set_property(
			connector, state, property, value);
}

int sde_connector_helper_reset_custom_properties(
		struct drm_connector *connector,
		struct drm_connector_state *connector_state)
{
	struct sde_connector *c_conn;
	struct sde_connector_state *c_state;
	struct drm_property *drm_prop;
	enum msm_mdp_conn_property prop_idx;

	if (!connector || !connector_state) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);
	c_state = to_sde_connector_state(connector_state);

	for (prop_idx = 0; prop_idx < CONNECTOR_PROP_COUNT; prop_idx++) {
		uint64_t val = c_state->property_values[prop_idx].value;
		uint64_t def;
		int ret;

		drm_prop = msm_property_index_to_drm_property(
				&c_conn->property_info, prop_idx);
		if (!drm_prop) {
			/* not all props will be installed, based on caps */
			SDE_DEBUG_CONN(c_conn, "invalid property index %d\n",
					prop_idx);
			continue;
		}

		def = msm_property_get_default(&c_conn->property_info,
				prop_idx);
		if (val == def)
			continue;

		SDE_DEBUG_CONN(c_conn, "set prop %s idx %d from %llu to %llu\n",
				drm_prop->name, prop_idx, val, def);

		ret = sde_connector_atomic_set_property(connector,
				connector_state, drm_prop, def);
		if (ret) {
			SDE_ERROR_CONN(c_conn,
					"set property failed, idx %d ret %d\n",
					prop_idx, ret);
			continue;
		}
	}

	return 0;
}

static int _sde_connector_lm_preference(struct sde_connector *sde_conn,
		 struct sde_kms *sde_kms, uint32_t disp_type)
{
	int ret = 0;
	u32 num_lm = 0;

	if (!sde_conn || !sde_kms || !sde_conn->ops.get_default_lms) {
		SDE_DEBUG("invalid input params");
		return -EINVAL;
	}

	if (!disp_type || disp_type >= SDE_CONNECTOR_MAX) {
		SDE_DEBUG("invalid display_type");
		return -EINVAL;
	}

	ret = sde_conn->ops.get_default_lms(sde_conn->display, &num_lm);
	if (ret || !num_lm) {
		SDE_DEBUG("failed to get default lm count");
		return ret;
	}

	if (num_lm > sde_kms->catalog->mixer_count) {
		SDE_DEBUG(
				"topology requesting more lms [%d] than hw exists [%d]",
				num_lm, sde_kms->catalog->mixer_count);
		return -EINVAL;
	}

	sde_hw_mixer_set_preference(sde_kms->catalog, num_lm, disp_type);

	return ret;
}

int sde_connector_get_panel_vfp(struct drm_connector *connector,
	struct drm_display_mode *mode)
{
	struct sde_connector *c_conn;
	int vfp = -EINVAL;

	if (!connector || !mode) {
		SDE_ERROR("invalid connector\n");
		return vfp;
	}
	c_conn = to_sde_connector(connector);
	if (!c_conn->ops.get_panel_vfp)
		return vfp;

	vfp = c_conn->ops.get_panel_vfp(c_conn->display,
		mode->hdisplay, mode->vdisplay);
	if (vfp <= 0)
		SDE_ERROR("Failed get_panel_vfp %d\n", vfp);

	return vfp;
}

static int _sde_debugfs_conn_cmd_tx_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->private_data = inode->i_private;
	return nonseekable_open(inode, file);
}

static ssize_t _sde_debugfs_conn_cmd_tx_sts_read(struct file *file,
		char __user *buf, size_t count, loff_t *ppos)
{
	struct drm_connector *connector = file->private_data;
	struct sde_connector *c_conn;
	char buffer[MAX_CMD_PAYLOAD_SIZE];
	int blen = 0;

	if (*ppos)
		return 0;

	if (!connector) {
		SDE_ERROR("invalid argument, conn is NULL\n");
		return 0;
	}

	c_conn = to_sde_connector(connector);

	mutex_lock(&c_conn->lock);
	blen = snprintf(buffer, MAX_CMD_PAYLOAD_SIZE,
		"last_cmd_tx_sts:0x%x",
		c_conn->last_cmd_tx_sts);
	mutex_unlock(&c_conn->lock);

	SDE_DEBUG("output: %s\n", buffer);
	if (blen <= 0) {
		SDE_ERROR("snprintf failed, blen %d\n", blen);
		return 0;
	}

	if (blen > count)
		blen = count;

	blen = min_t(size_t, blen, MAX_CMD_PAYLOAD_SIZE);
	if (copy_to_user(buf, buffer, blen)) {
		SDE_ERROR("copy to user buffer failed\n");
		return -EFAULT;
	}

	*ppos += blen;
	return blen;
}

static ssize_t _sde_debugfs_conn_cmd_tx_write(struct file *file,
			const char __user *p, size_t count, loff_t *ppos)
{
	struct drm_connector *connector = file->private_data;
	struct sde_connector *c_conn;
	char *input, *token, *input_copy, *input_dup = NULL;
	const char *delim = " ";
	u32 buf_size = 0;
	char buffer[MAX_CMD_PAYLOAD_SIZE];
	int rc = 0, strtoint;

	if (*ppos || !connector) {
		SDE_ERROR("invalid argument(s), conn %d\n", connector != NULL);
		return 0;
	}

	c_conn = to_sde_connector(connector);

	if (!c_conn->ops.cmd_transfer) {
		SDE_ERROR("no cmd transfer support for connector name %s\n",
				c_conn->name);
		return 0;
	}

	input = kmalloc(count + 1, GFP_KERNEL);
	if (!input)
		return -ENOMEM;

	if (copy_from_user(input, p, count)) {
		SDE_ERROR("copy from user failed\n");
		rc = -EFAULT;
		goto end;
	}
	input[count] = '\0';

	SDE_INFO("Command requested for trasnfer to panel: %s\n", input);

	input_copy = kstrdup(input, GFP_KERNEL);
	if (!input_copy) {
		rc = -ENOMEM;
		goto end;
	}

	input_dup = input_copy;
	token = strsep(&input_copy, delim);
	while (token) {
		rc = kstrtoint(token, 0, &strtoint);
		if (rc) {
			SDE_ERROR("input buffer conversion failed\n");
			goto end;
		}

		if (buf_size >= MAX_CMD_PAYLOAD_SIZE) {
			SDE_ERROR("buffer size exceeding the limit %d\n",
					MAX_CMD_PAYLOAD_SIZE);
			goto end;
		}
		buffer[buf_size++] = (strtoint & 0xff);
		token = strsep(&input_copy, delim);
	}
	SDE_DEBUG("command packet size in bytes: %u\n", buf_size);
	if (!buf_size)
		goto end;

	mutex_lock(&c_conn->lock);
	rc = c_conn->ops.cmd_transfer(&c_conn->base, c_conn->display, buffer,
			buf_size);
	c_conn->last_cmd_tx_sts = !rc ? true : false;
	mutex_unlock(&c_conn->lock);

	rc = count;
end:
	kfree(input_dup);
	kfree(input);
	return rc;
}

static const struct file_operations conn_cmd_tx_fops = {
	.open =		_sde_debugfs_conn_cmd_tx_open,
	.read =		_sde_debugfs_conn_cmd_tx_sts_read,
	.write =	_sde_debugfs_conn_cmd_tx_write,
};

static void _sde_connector_report_panel_dead(struct sde_connector *conn, bool skip_pre_kickoff);
static int _sde_debugfs_conn_esd_test_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->private_data = inode->i_private;
	return nonseekable_open(inode, file);
}


static ssize_t _sde_debugfs_conn_esd_test_write(struct file *file,
			const char __user *p, size_t count, loff_t *ppos)
{
	struct drm_connector *connector = file->private_data;
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dsi_display *display = c_conn->display;
	struct drm_event event;
	int power_mode;
	char *input;
	int rc = 0;
	const char *sde_power_mode_str[] = {
		[SDE_MODE_DPMS_ON] = "SDE_MODE_DPMS_ON",
		[SDE_MODE_DPMS_LP1] = "SDE_MODE_DPMS_LP1",
		[SDE_MODE_DPMS_LP2] = "SDE_MODE_DPMS_LP2",
		[SDE_MODE_DPMS_STANDBY] = "SDE_MODE_DPMS_STANDBY",
		[SDE_MODE_DPMS_SUSPEND] = "SDE_MODE_DPMS_SUSPEND",
		[SDE_MODE_DPMS_OFF] = "SDE_MODE_DPMS_OFF",
	};

	if (!display || !display->panel) {
		SDE_ERROR("invalid display/panel\n");
		return -EINVAL;
	}

	input = kmalloc(count + 1, GFP_KERNEL);
	if (!input)
		return -ENOMEM;

	if (copy_from_user(input, p, count)) {
		SDE_ERROR("copy from user failed\n");
		rc = -EFAULT;
		goto end;
	}
	input[count] = '\0';
	DSI_INFO("[esd-test]intput esd test: %s\n", input);

	if (!strncmp(input, "1", 1) || !strncmp(input, "on", 2) ||
		!strncmp(input, "true", 4)) {
		DSI_INFO("[esd-test]panel esd irq trigging \n");
	} else {
		goto end;
	}
	if (c_conn->connector_type == DRM_MODE_CONNECTOR_DSI) {
		if (dsi_panel_initialized(display->panel)) {
			if (atomic_read(&(display->panel->esd_recovery_pending))) {
				SDE_ERROR("[esd-test]ESD recovery already pending\n");
				rc = count;
				goto end;
			}
			power_mode = display->panel->power_mode;
			DSI_INFO("[esd-test]power_mode = %s\n", sde_power_mode_str[power_mode]);
			if (power_mode == SDE_MODE_DPMS_ON ||
				power_mode == SDE_MODE_DPMS_LP1) {
				atomic_set(&display->panel->esd_recovery_pending, 1);
				_sde_connector_report_panel_dead(c_conn, false);
			} else {
				if (!c_conn->panel_dead) {
					atomic_set(&display->panel->esd_recovery_pending, 1);
					c_conn->panel_dead = true;
					event.type = DRM_EVENT_PANEL_DEAD;
					event.length = sizeof(bool);
					msm_mode_object_event_notify(&c_conn->base.base,
						c_conn->base.dev, &event, (u8 *)&c_conn->panel_dead);
					SDE_EVT32(SDE_EVTLOG_ERROR);
					SDE_ERROR("[esd-test]esd irq check failed report PANEL_DEAD"
						" conn_id: %d enc_id: %d\n",
						c_conn->base.base.id, c_conn->encoder->base.id);
				}
			}
			rc = count;
		}
	}

end:
	kfree(input);
	return rc;
}


static const struct file_operations conn_esd_test_fops = {
	.open  = _sde_debugfs_conn_esd_test_open,
	.write = _sde_debugfs_conn_esd_test_write,
};

#ifdef CONFIG_DEBUG_FS
/**
 * sde_connector_init_debugfs - initialize connector debugfs
 * @connector: Pointer to drm connector
 */
static int sde_connector_init_debugfs(struct drm_connector *connector)
{
	struct sde_connector *sde_connector;
	struct msm_display_info info;

	if (!connector || !connector->debugfs_entry) {
		SDE_ERROR("invalid connector\n");
		return -EINVAL;
	}

	sde_connector = to_sde_connector(connector);

	sde_connector_get_info(connector, &info);
	if (sde_connector->ops.check_status &&
		(info.capabilities & MSM_DISPLAY_ESD_ENABLED)) {
		debugfs_create_u32("esd_status_interval", 0600,
				connector->debugfs_entry,
				&sde_connector->esd_status_interval);
	}

	if (!debugfs_create_bool("fb_kmap", 0600, connector->debugfs_entry,
			&sde_connector->fb_kmap)) {
		SDE_ERROR("failed to create connector fb_kmap\n");
		return -ENOMEM;
	}

	if (sde_connector->ops.cmd_transfer) {
		if (!debugfs_create_file("tx_cmd", 0600,
			connector->debugfs_entry,
			connector, &conn_cmd_tx_fops)) {
			SDE_ERROR("failed to create connector cmd_tx\n");
			return -ENOMEM;
		}
	}

	if (!debugfs_create_file("esd_test", 0600,
		connector->debugfs_entry,
		connector, &conn_esd_test_fops)) {
		SDE_ERROR("[esd-test]failed to create connector esd_test\n");
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

static int sde_connector_fill_modes(struct drm_connector *connector,
		uint32_t max_width, uint32_t max_height)
{
	int rc, mode_count = 0;
	struct sde_connector *sde_conn = NULL;

	sde_conn = to_sde_connector(connector);
	if (!sde_conn) {
		SDE_ERROR("invalid arguments\n");
		return 0;
	}

	mode_count = drm_helper_probe_single_connector_modes(connector,
			max_width, max_height);

	rc = sde_connector_set_blob_data(connector,
				connector->state,
				CONNECTOR_PROP_MODE_INFO);
	if (rc) {
		SDE_ERROR_CONN(sde_conn,
			"failed to setup mode info prop, rc = %d\n", rc);
		return 0;
	}

	return mode_count;
}

static const struct drm_connector_funcs sde_connector_ops = {
	.reset =                  sde_connector_atomic_reset,
	.detect =                 sde_connector_detect,
	.destroy =                sde_connector_destroy,
	.fill_modes =             sde_connector_fill_modes,
	.atomic_duplicate_state = sde_connector_atomic_duplicate_state,
	.atomic_destroy_state =   sde_connector_atomic_destroy_state,
	.atomic_set_property =    sde_connector_atomic_set_property,
	.atomic_get_property =    sde_connector_atomic_get_property,
	.late_register =          sde_connector_late_register,
	.early_unregister =       sde_connector_early_unregister,
};

static int sde_connector_get_modes(struct drm_connector *connector)
{
	struct sde_connector *c_conn;
	struct msm_resource_caps_info avail_res;
	int mode_count = 0;

	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return 0;
	}

	c_conn = to_sde_connector(connector);
	if (!c_conn->ops.get_modes) {
		SDE_DEBUG("missing get_modes callback\n");
		return 0;
	}

	memset(&avail_res, 0, sizeof(avail_res));
	sde_connector_get_avail_res_info(connector, &avail_res);

	mode_count = c_conn->ops.get_modes(connector, c_conn->display,
			&avail_res);
	if (!mode_count) {
		SDE_ERROR_CONN(c_conn, "failed to get modes\n");
		return 0;
	}

	if (c_conn->hdr_capable)
		sde_connector_update_hdr_props(connector);

	if (c_conn->connector_type == DRM_MODE_CONNECTOR_DisplayPort)
		sde_connector_update_colorspace(connector);

	return mode_count;
}

static enum drm_mode_status
sde_connector_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode)
{
	struct sde_connector *c_conn;
	struct msm_resource_caps_info avail_res;

	if (!connector || !mode) {
		SDE_ERROR("invalid argument(s), conn %pK, mode %pK\n",
				connector, mode);
		return MODE_ERROR;
	}

	c_conn = to_sde_connector(connector);

	memset(&avail_res, 0, sizeof(avail_res));
	sde_connector_get_avail_res_info(connector, &avail_res);

	if (c_conn->ops.mode_valid)
		return c_conn->ops.mode_valid(connector, mode, c_conn->display,
				&avail_res);

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

static struct drm_encoder *
sde_connector_atomic_best_encoder(struct drm_connector *connector,
		struct drm_connector_state *connector_state)
{
	struct sde_connector *c_conn;
	struct drm_encoder *encoder = NULL;

	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return NULL;
	}

	c_conn = to_sde_connector(connector);

	if (c_conn->ops.atomic_best_encoder)
		encoder = c_conn->ops.atomic_best_encoder(connector,
				c_conn->display, connector_state);

	c_conn->encoder = encoder;

	return encoder;
}

static int sde_connector_atomic_check(struct drm_connector *connector,
		struct drm_connector_state *new_conn_state)
{
	struct sde_connector *c_conn;
	struct sde_connector_state *c_state;
	bool qsync_dirty = false, has_modeset = false;

	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return -EINVAL;
	}

	if (!new_conn_state) {
		SDE_ERROR("invalid connector state\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);
	c_state = to_sde_connector_state(new_conn_state);

	has_modeset = sde_crtc_atomic_check_has_modeset(new_conn_state->state,
						new_conn_state->crtc);
	qsync_dirty = msm_property_is_dirty(&c_conn->property_info,
					&c_state->property_state,
					CONNECTOR_PROP_QSYNC_MODE);

	SDE_DEBUG("has_modeset %d qsync_dirty %d\n", has_modeset, qsync_dirty);
	if (has_modeset && qsync_dirty) {
		SDE_ERROR("invalid qsync update during modeset\n");
		return -EINVAL;
	}

	if (c_conn->ops.atomic_check)
		return c_conn->ops.atomic_check(connector,
				c_conn->display, new_conn_state);

	return 0;
}

static void _sde_connector_report_panel_dead(struct sde_connector *conn,
	bool skip_pre_kickoff)
{
	struct drm_event event;
	struct dsi_display *display = (struct dsi_display *)(conn->display);

	if (!conn)
		return;

	/* Panel dead notification can come:
	 * 1) ESD thread
	 * 2) Commit thread (if TE stops coming)
	 * So such case, avoid failure notification twice.
	 */
	if (conn->panel_dead) {
		SDE_INFO("panel_dead is true, return!\n");
		return;
	}

	conn->panel_dead = true;
	display->panel->mi_cfg.panel_dead_flag = true;
	event.type = DRM_EVENT_PANEL_DEAD;
	event.length = sizeof(bool);
	msm_mode_object_event_notify(&conn->base.base,
		conn->base.dev, &event, (u8 *)&conn->panel_dead);
	sde_encoder_display_failure_notification(conn->encoder,
		skip_pre_kickoff);
	SDE_EVT32(SDE_EVTLOG_ERROR);
	SDE_ERROR("esd check failed report PANEL_DEAD conn_id: %d enc_id: %d\n",
			conn->base.base.id, conn->encoder->base.id);
}

int sde_connector_esd_status(struct drm_connector *conn)
{
	struct sde_connector *sde_conn = NULL;
	struct dsi_display *display;
	int ret = 0;

	if (!conn)
		return ret;

	sde_conn = to_sde_connector(conn);
	if (!sde_conn || !sde_conn->ops.check_status)
		return ret;

	display = sde_conn->display;

	/* protect this call with ESD status check call */
	mutex_lock(&sde_conn->lock);
	if (atomic_read(&(display->panel->esd_recovery_pending))) {
		SDE_ERROR("ESD recovery already pending\n");
		mutex_unlock(&sde_conn->lock);
		return -ETIMEDOUT;
	}
	ret = sde_conn->ops.check_status(&sde_conn->base,
					 sde_conn->display, true);
	mutex_unlock(&sde_conn->lock);

	if (ret <= 0) {
		/* cancel if any pending esd work */
		sde_connector_schedule_status_work(conn, false);
		_sde_connector_report_panel_dead(sde_conn, true);
		ret = -ETIMEDOUT;
	} else {
		SDE_DEBUG("Successfully received TE from panel\n");
		ret = 0;
	}
	SDE_EVT32(ret);

	return ret;
}

static void sde_connector_check_status_work(struct work_struct *work)
{
	struct sde_connector *conn;
	int rc = 0;
	struct device *dev;

	conn = container_of(to_delayed_work(work),
			struct sde_connector, status_work);
	if (!conn) {
		SDE_ERROR("not able to get connector object\n");
		return;
	}

	mutex_lock(&conn->lock);
	dev = conn->base.dev->dev;

	if (!conn->ops.check_status || dev->power.is_suspended ||
			(conn->dpms_mode != DRM_MODE_DPMS_ON)) {
		SDE_DEBUG("dpms mode: %d\n", conn->dpms_mode);
		mutex_unlock(&conn->lock);
		return;
	}

	rc = conn->ops.check_status(&conn->base, conn->display, false);
	mutex_unlock(&conn->lock);

	if (rc > 0) {
		u32 interval;

		SDE_DEBUG("esd check status success conn_id: %d enc_id: %d\n",
				conn->base.base.id, conn->encoder->base.id);

		/* If debugfs property is not set then take default value */
		interval = conn->esd_status_interval ?
			conn->esd_status_interval : STATUS_CHECK_INTERVAL_MS;
		schedule_delayed_work(&conn->status_work,
			msecs_to_jiffies(interval));
		return;
	}

	_sde_connector_report_panel_dead(conn, false);
}

static const struct drm_connector_helper_funcs sde_connector_helper_ops = {
	.get_modes =    sde_connector_get_modes,
	.mode_valid =   sde_connector_mode_valid,
	.best_encoder = sde_connector_best_encoder,
	.atomic_check = sde_connector_atomic_check,
};

static const struct drm_connector_helper_funcs sde_connector_helper_ops_v2 = {
	.get_modes =    sde_connector_get_modes,
	.mode_valid =   sde_connector_mode_valid,
	.best_encoder = sde_connector_best_encoder,
	.atomic_best_encoder = sde_connector_atomic_best_encoder,
	.atomic_check = sde_connector_atomic_check,
};

static int sde_connector_populate_mode_info(struct drm_connector *conn,
	struct sde_kms_info *info)
{
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct sde_connector *c_conn = NULL;
	struct drm_display_mode *mode;
	struct msm_mode_info mode_info;
	int rc = 0;

	if (!conn || !conn->dev || !conn->dev->dev_private) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	priv = conn->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);

	c_conn = to_sde_connector(conn);
	if (!c_conn->ops.get_mode_info) {
		SDE_ERROR_CONN(c_conn, "get_mode_info not defined\n");
		return -EINVAL;
	}

	list_for_each_entry(mode, &conn->modes, head) {
		int topology_idx = 0;

		memset(&mode_info, 0, sizeof(mode_info));

		rc = sde_connector_get_mode_info(&c_conn->base, mode,
				&mode_info);
		if (rc) {
			SDE_ERROR_CONN(c_conn,
				"failed to get mode info for mode %s\n",
				mode->name);
			continue;
		}

		sde_kms_info_add_keystr(info, "mode_name", mode->name);

		sde_kms_info_add_keyint(info, "bit_clk_rate",
					mode_info.clk_rate);

		topology_idx = (int)sde_rm_get_topology_name(
							mode_info.topology);
		if (topology_idx < SDE_RM_TOPOLOGY_MAX) {
			sde_kms_info_add_keystr(info, "topology",
					e_topology_name[topology_idx].name);
		} else {
			SDE_ERROR_CONN(c_conn, "invalid topology\n");
			continue;
		}

		sde_kms_info_add_keyint(info, "mdp_transfer_time_us",
			mode_info.mdp_transfer_time_us);

		if (!mode_info.roi_caps.num_roi)
			continue;

		sde_kms_info_add_keyint(info, "partial_update_num_roi",
			mode_info.roi_caps.num_roi);
		sde_kms_info_add_keyint(info, "partial_update_xstart",
			mode_info.roi_caps.align.xstart_pix_align);
		sde_kms_info_add_keyint(info, "partial_update_walign",
			mode_info.roi_caps.align.width_pix_align);
		sde_kms_info_add_keyint(info, "partial_update_wmin",
			mode_info.roi_caps.align.min_width);
		sde_kms_info_add_keyint(info, "partial_update_ystart",
			mode_info.roi_caps.align.ystart_pix_align);
		sde_kms_info_add_keyint(info, "partial_update_halign",
			mode_info.roi_caps.align.height_pix_align);
		sde_kms_info_add_keyint(info, "partial_update_hmin",
			mode_info.roi_caps.align.min_height);
		sde_kms_info_add_keyint(info, "partial_update_roimerge",
			mode_info.roi_caps.merge_rois);
	}

	return rc;
}

int sde_connector_set_blob_data(struct drm_connector *conn,
		struct drm_connector_state *state,
		enum msm_mdp_conn_property prop_id)
{
	struct sde_kms_info *info;
	struct sde_connector *c_conn = NULL;
	struct sde_connector_state *sde_conn_state = NULL;
	struct msm_mode_info mode_info;
	struct drm_property_blob **blob = NULL;
	int rc = 0;

	c_conn = to_sde_connector(conn);
	if (!c_conn) {
		SDE_ERROR("invalid argument\n");
		return -EINVAL;
	}

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	sde_kms_info_reset(info);

	switch (prop_id) {
	case CONNECTOR_PROP_SDE_INFO:
		memset(&mode_info, 0, sizeof(mode_info));

		if (state) {
			sde_conn_state = to_sde_connector_state(state);
			memcpy(&mode_info, &sde_conn_state->mode_info,
					sizeof(sde_conn_state->mode_info));
		} else {
			/**
			 * connector state is assigned only on first
			 * atomic_commit. But this function is allowed to be
			 * invoked during probe/init sequence. So not throwing
			 * an error.
			 */
			SDE_DEBUG_CONN(c_conn, "invalid connector state\n");
		}

		if (c_conn->ops.set_info_blob) {
			rc = c_conn->ops.set_info_blob(conn, info,
					c_conn->display, &mode_info);
			if (rc) {
				SDE_ERROR_CONN(c_conn,
						"set_info_blob failed, %d\n",
						rc);
				goto exit;
			}
		}

		blob = &c_conn->blob_caps;
	break;
	case CONNECTOR_PROP_MODE_INFO:
		rc = sde_connector_populate_mode_info(conn, info);
		if (rc) {
			SDE_ERROR_CONN(c_conn,
					"mode info population failed, %d\n",
					rc);
			goto exit;
		}
		blob = &c_conn->blob_mode_info;
	break;
	default:
		SDE_ERROR_CONN(c_conn, "invalid prop_id: %d\n", prop_id);
		goto exit;
	}

	msm_property_set_blob(&c_conn->property_info,
			blob,
			SDE_KMS_INFO_DATA(info),
			SDE_KMS_INFO_DATALEN(info),
			prop_id);
exit:
	kfree(info);

	return rc;
}

static int _sde_connector_install_properties(struct drm_device *dev,
	struct sde_kms *sde_kms, struct sde_connector *c_conn,
	int connector_type, void *display,
	struct msm_display_info *display_info)
{
	struct dsi_display *dsi_display;
	int rc;
	struct drm_connector *connector;

	msm_property_install_blob(&c_conn->property_info, "capabilities",
			DRM_MODE_PROP_IMMUTABLE, CONNECTOR_PROP_SDE_INFO);

	rc = sde_connector_set_blob_data(&c_conn->base,
			NULL, CONNECTOR_PROP_SDE_INFO);
	if (rc) {
		SDE_ERROR_CONN(c_conn,
			"failed to setup connector info, rc = %d\n", rc);
		return rc;
	}

	connector = &c_conn->base;

	msm_property_install_blob(&c_conn->property_info, "mode_properties",
			DRM_MODE_PROP_IMMUTABLE, CONNECTOR_PROP_MODE_INFO);

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

	msm_property_install_volatile_range(
			&c_conn->property_info, "sde_drm_roi_v1", 0x0,
			0, ~0, 0, CONNECTOR_PROP_ROI_V1);

	/* install PP_DITHER properties */
	_sde_connector_install_dither_property(dev, sde_kms, c_conn);

	if (connector_type == DRM_MODE_CONNECTOR_DisplayPort) {
		struct drm_msm_ext_hdr_properties hdr = {0};

		c_conn->hdr_capable = true;

		msm_property_install_blob(&c_conn->property_info,
				"ext_hdr_properties",
				DRM_MODE_PROP_IMMUTABLE,
				CONNECTOR_PROP_EXT_HDR_INFO);

		/* set default values to avoid reading uninitialized data */
		msm_property_set_blob(&c_conn->property_info,
			      &c_conn->blob_ext_hdr,
			      &hdr,
			      sizeof(hdr),
			      CONNECTOR_PROP_EXT_HDR_INFO);

		/* create and attach colorspace property for DP */
		if (!drm_mode_create_colorspace_property(connector))
			drm_object_attach_property(&connector->base,
				connector->colorspace_property, 0);
	}

	msm_property_install_volatile_range(&c_conn->property_info,
		"hdr_metadata", 0x0, 0, ~0, 0, CONNECTOR_PROP_HDR_METADATA);

	msm_property_install_volatile_range(&c_conn->property_info,
		"RETIRE_FENCE", 0x0, 0, ~0, 0, CONNECTOR_PROP_RETIRE_FENCE);

	msm_property_install_range(&c_conn->property_info, "autorefresh",
			0x0, 0, AUTOREFRESH_MAX_FRAME_CNT, 0,
			CONNECTOR_PROP_AUTOREFRESH);

	if (connector_type == DRM_MODE_CONNECTOR_DSI) {
		if (sde_kms->catalog->has_qsync && display_info->qsync_min_fps)
			msm_property_install_enum(&c_conn->property_info,
					"qsync_mode", 0, 0, e_qsync_mode,
					ARRAY_SIZE(e_qsync_mode),
					CONNECTOR_PROP_QSYNC_MODE);

		if (display_info->capabilities & MSM_DISPLAY_CAP_CMD_MODE)
			msm_property_install_enum(&c_conn->property_info,
				"frame_trigger_mode", 0, 0,
				e_frame_trigger_mode,
				ARRAY_SIZE(e_frame_trigger_mode),
				CONNECTOR_PROP_CMD_FRAME_TRIGGER_MODE);
	}

	msm_property_install_range(&c_conn->property_info, "bl_scale",
		0x0, 0, MAX_BL_SCALE_LEVEL, MAX_BL_SCALE_LEVEL,
		CONNECTOR_PROP_BL_SCALE);

	msm_property_install_range(&c_conn->property_info, "sv_bl_scale",
		0x0, 0, MAX_SV_BL_SCALE_LEVEL, MAX_SV_BL_SCALE_LEVEL,
		CONNECTOR_PROP_SV_BL_SCALE);

	c_conn->bl_scale_dirty = false;
	c_conn->bl_scale = MAX_BL_SCALE_LEVEL;
	c_conn->bl_scale_sv = MAX_SV_BL_SCALE_LEVEL;

	if (connector_type == DRM_MODE_CONNECTOR_DisplayPort)
		msm_property_install_range(&c_conn->property_info,
			"supported_colorspaces",
			DRM_MODE_PROP_IMMUTABLE, 0, 0xffff, 0,
			CONNECTOR_PROP_SUPPORTED_COLORSPACES);

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

	return 0;
}

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
	struct sde_connector *c_conn = NULL;
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

	c_conn->base.panel = panel;
	c_conn->connector_type = connector_type;
	c_conn->encoder = encoder;
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

	if (ops && ops->atomic_best_encoder && ops->atomic_check)
		c_conn->base.helper_private = &sde_connector_helper_ops_v2;
	else
		c_conn->base.helper_private = &sde_connector_helper_ops;

	c_conn->base.polled = connector_poll;
	c_conn->base.interlace_allowed = 0;
	c_conn->base.doublescan_allowed = 0;

	snprintf(c_conn->name,
			SDE_CONNECTOR_NAME_SIZE,
			"conn%u",
			c_conn->base.base.id);

	c_conn->retire_fence = sde_fence_init(c_conn->name,
			c_conn->base.base.id);
	if (IS_ERR(c_conn->retire_fence)) {
		rc = PTR_ERR(c_conn->retire_fence);
		SDE_ERROR("failed to init fence, %d\n", rc);
		goto error_cleanup_conn;
	}

	mutex_init(&c_conn->lock);

	rc = drm_connector_attach_encoder(&c_conn->base, encoder);
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
		rc = c_conn->ops.post_init(&c_conn->base, display);
		if (rc) {
			SDE_ERROR("post-init failed, %d\n", rc);
			goto error_cleanup_fence;
		}
	}

	rc = sde_connector_get_info(&c_conn->base, &display_info);
	if (!rc && (connector_type == DRM_MODE_CONNECTOR_DSI) &&
			(display_info.capabilities & MSM_DISPLAY_CAP_VID_MODE))
		sde_connector_register_event(&c_conn->base,
			SDE_CONN_EVENT_VID_FIFO_OVERFLOW,
			sde_connector_handle_disp_recovery,
			c_conn);

	rc = _sde_connector_install_properties(dev, sde_kms, c_conn,
		connector_type, display, &display_info);
	if (rc)
		goto error_cleanup_fence;

	rc = msm_property_install_get_status(&c_conn->property_info);
	if (rc) {
		SDE_ERROR("failed to create one or more properties\n");
		goto error_destroy_property;
	}

	_sde_connector_lm_preference(c_conn, sde_kms,
			display_info.display_type);

	SDE_DEBUG("connector %d attach encoder %d\n",
			c_conn->base.base.id, encoder->base.id);

	INIT_DELAYED_WORK(&c_conn->status_work,
			sde_connector_check_status_work);

	return &c_conn->base;

error_destroy_property:
	if (c_conn->blob_caps)
		drm_property_blob_put(c_conn->blob_caps);
	if (c_conn->blob_hdr)
		drm_property_blob_put(c_conn->blob_hdr);
	if (c_conn->blob_dither)
		drm_property_blob_put(c_conn->blob_dither);
	if (c_conn->blob_mode_info)
		drm_property_blob_put(c_conn->blob_mode_info);
	if (c_conn->blob_ext_hdr)
		drm_property_blob_put(c_conn->blob_ext_hdr);

	msm_property_destroy(&c_conn->property_info);
error_cleanup_fence:
	mutex_destroy(&c_conn->lock);
	sde_fence_deinit(c_conn->retire_fence);
error_cleanup_conn:
	drm_connector_cleanup(&c_conn->base);
error_free_conn:
	kfree(c_conn);

	return ERR_PTR(rc);
}

static int _sde_conn_hw_recovery_handler(
		struct drm_connector *connector, bool val)
{
	struct sde_connector *c_conn;

	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return -EINVAL;
	}
	c_conn = to_sde_connector(connector);

	if (c_conn->encoder)
		sde_encoder_recovery_events_handler(c_conn->encoder, val);

	return 0;
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
	case DRM_EVENT_SDE_HW_RECOVERY:
		ret = _sde_conn_hw_recovery_handler(conn_drm, val);
		break;
	default:
		break;
	}
	return ret;
}

int sde_connector_event_notify(struct drm_connector *connector, uint32_t type,
		uint32_t len, uint32_t val)
{
	struct drm_event event;
	int ret;

	if (!connector) {
		SDE_ERROR("invalid connector\n");
		return -EINVAL;
	}

	switch (type) {
	case DRM_EVENT_SYS_BACKLIGHT:
	case DRM_EVENT_PANEL_DEAD:
	case DRM_EVENT_SDE_HW_RECOVERY:
		ret = 0;
		break;
	default:
		SDE_ERROR("connector %d, Unsupported event %d\n",
				connector->base.id, type);
		return -EINVAL;
	}

	event.type = type;
	event.length = len;
	msm_mode_object_event_notify(&connector->base, connector->dev, &event,
			(u8 *)&val);

	SDE_EVT32(connector->base.id, type, len, val);
	SDE_DEBUG("connector:%d hw recovery event(%d) value (%d) notified\n",
			connector->base.id, type, val);

	return ret;
}

int sde_connector_hbm_ctl(struct drm_connector *connector, uint32_t op_code)
{
	int ret = 0;

	SDE_ATRACE_BEGIN("sde_connector_hbm_ctl");
	ret = dsi_display_hbm_set_disp_param(connector, op_code);
	SDE_ATRACE_END("sde_connector_hbm_ctl");
	return ret;
}

int sde_connector_pre_hbm_ctl(struct drm_connector *connector)
{
	int ret;
	/* close dimming */
	ret = dsi_display_hbm_set_disp_param(connector, DISPPARAM_HBM_BACKLIGHT_RESEND);
	return ret;
}

#define to_dsi_bridge(x)     container_of((x), struct dsi_bridge, base)

static uint32_t interpolate(uint32_t x, uint32_t xa, uint32_t xb, uint32_t ya, uint32_t yb)
{
	uint32_t bf;

	bf = ya - (ya - yb) * (x - xa) / (xb - xa);

	SDE_DEBUG("backlight brightness:%d, [i-1]bl:%d, [i]bl:%d, [i-1]alpha:%d, [i]alpha:%d, bf:%d",
			x, xa, xb, ya, yb, bf);

	return bf;
}

static uint32_t brightness_to_alpha(struct dsi_panel_mi_cfg *mi_cfg, uint32_t brightness)
{
	int i;
	int level = mi_cfg->brightnes_alpha_lut_item_count;

	if (brightness == 0x0)
		return mi_cfg->brightness_alpha_lut[0].alpha;

	for (i = 0; i < level; i++){
		if (mi_cfg->brightness_alpha_lut[i].brightness >= brightness)
			break;
	}

	if (i == level)
		return mi_cfg->brightness_alpha_lut[i - 1].alpha;
	else
		return interpolate(brightness,
							mi_cfg->brightness_alpha_lut[i-1].brightness, mi_cfg->brightness_alpha_lut[i].brightness,
							mi_cfg->brightness_alpha_lut[i-1].alpha, mi_cfg->brightness_alpha_lut[i].alpha);
}

void sde_connector_mi_get_current_alpha(struct drm_connector *connector, uint32_t brightness, uint32_t *alpha)
{
	struct dsi_display *display = NULL;
	struct dsi_bridge *c_bridge = NULL;
	struct dsi_panel_mi_cfg *mi_cfg = NULL;

	if (!connector || !connector->encoder || !connector->encoder->bridge) {
		SDE_ERROR("Invalid connector/encoder/bridge ptr\n");
		return;
	}

	c_bridge =  to_dsi_bridge(connector->encoder->bridge);
	display = c_bridge->display;
	if (!display || !display->panel) {
		SDE_ERROR("invalid display/panel ptr\n");
		return;
	}

	mi_cfg = &display->panel->mi_cfg;

	*alpha = brightness_to_alpha(mi_cfg, brightness);
	return;
}

void sde_connector_mi_get_current_backlight(struct drm_connector *connector, uint32_t *brightness)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dsi_display *display = NULL;
	struct dsi_bridge *c_bridge = NULL;

	if (!connector || !connector->encoder || !connector->encoder->bridge) {
		SDE_ERROR("Invalid connector/encoder/bridge ptr\n");
		return;
	}

	c_bridge =  to_dsi_bridge(connector->encoder->bridge);
	display = c_bridge->display;
	if (!display || !display->panel) {
		SDE_ERROR("invalid display/panel ptr\n");
		return;
	}

	if (display->panel->mi_cfg.in_aod) {
		*brightness = display->panel->mi_cfg.aod_backlight;
		return;
	}

	*brightness = c_conn->mi_dimlayer_state.current_backlight;
}

void sde_connector_mi_update_dimlayer_state(struct drm_connector *connector,
	enum mi_dimlayer_type mi_dimlayer_type)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	c_conn->mi_dimlayer_state.mi_dimlayer_type = mi_dimlayer_type;
}
