/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#define pr_fmt(fmt)	"dsi-drm:[%s] " fmt, __func__
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_notifier.h>
#include <linux/notifier.h>
#include <drm/drm_bridge.h>
#include <linux/pm_wakeup.h>

#include "msm_kms.h"
#include "sde_connector.h"
#include "dsi_drm.h"
#include "sde_trace.h"

static BLOCKING_NOTIFIER_HEAD(drm_notifier_list);

#define to_dsi_bridge(x)     container_of((x), struct dsi_bridge, base)
#define to_dsi_state(x)      container_of((x), struct dsi_connector_state, base)

#define WAIT_RESUME_TIMEOUT 200

struct dsi_bridge *gbridge;
static struct delayed_work prim_panel_work;
static atomic_t prim_panel_is_on;
static struct wakeup_source prim_panel_wakelock;

struct drm_notify_data g_notify_data;

/**
 *	drm_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int drm_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&drm_notifier_list, nb);
}
EXPORT_SYMBOL(drm_register_client);

/**
 *	drm_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int drm_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&drm_notifier_list, nb);
}
EXPORT_SYMBOL(drm_unregister_client);

/**
 * drm_notifier_call_chain - notify clients of drm_events
 *
 */
int drm_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&drm_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(drm_notifier_call_chain);

static void convert_to_dsi_mode(const struct drm_display_mode *drm_mode,
				struct dsi_display_mode *dsi_mode)
{
	memset(dsi_mode, 0, sizeof(*dsi_mode));

	dsi_mode->timing.h_active = drm_mode->hdisplay;
	dsi_mode->timing.h_back_porch = drm_mode->htotal - drm_mode->hsync_end;
	dsi_mode->timing.h_sync_width = drm_mode->htotal -
			(drm_mode->hsync_start + dsi_mode->timing.h_back_porch);
	dsi_mode->timing.h_front_porch = drm_mode->hsync_start -
					 drm_mode->hdisplay;
	dsi_mode->timing.h_skew = drm_mode->hskew;

	dsi_mode->timing.v_active = drm_mode->vdisplay;
	dsi_mode->timing.v_back_porch = drm_mode->vtotal - drm_mode->vsync_end;
	dsi_mode->timing.v_sync_width = drm_mode->vtotal -
		(drm_mode->vsync_start + dsi_mode->timing.v_back_porch);

	dsi_mode->timing.v_front_porch = drm_mode->vsync_start -
					 drm_mode->vdisplay;

	dsi_mode->timing.refresh_rate = drm_mode->vrefresh;

	dsi_mode->pixel_clk_khz = drm_mode->clock;

	dsi_mode->priv_info =
		(struct dsi_display_mode_priv_info *)drm_mode->private;

	if (msm_is_mode_seamless(drm_mode))
		dsi_mode->dsi_mode_flags |= DSI_MODE_FLAG_SEAMLESS;
	if (msm_is_mode_dynamic_fps(drm_mode))
		dsi_mode->dsi_mode_flags |= DSI_MODE_FLAG_DFPS;
	if (msm_needs_vblank_pre_modeset(drm_mode))
		dsi_mode->dsi_mode_flags |= DSI_MODE_FLAG_VBLANK_PRE_MODESET;
	if (msm_is_mode_seamless_dms(drm_mode))
		dsi_mode->dsi_mode_flags |= DSI_MODE_FLAG_DMS;
	if (msm_is_mode_seamless_vrr(drm_mode))
		dsi_mode->dsi_mode_flags |= DSI_MODE_FLAG_VRR;

	dsi_mode->timing.h_sync_polarity =
			!!(drm_mode->flags & DRM_MODE_FLAG_PHSYNC);
	dsi_mode->timing.v_sync_polarity =
			!!(drm_mode->flags & DRM_MODE_FLAG_PVSYNC);
}

void dsi_convert_to_drm_mode(const struct dsi_display_mode *dsi_mode,
				struct drm_display_mode *drm_mode)
{
	memset(drm_mode, 0, sizeof(*drm_mode));

	drm_mode->hdisplay = dsi_mode->timing.h_active;
	drm_mode->hsync_start = drm_mode->hdisplay +
				dsi_mode->timing.h_front_porch;
	drm_mode->hsync_end = drm_mode->hsync_start +
			      dsi_mode->timing.h_sync_width;
	drm_mode->htotal = drm_mode->hsync_end + dsi_mode->timing.h_back_porch;
	drm_mode->hskew = dsi_mode->timing.h_skew;

	drm_mode->vdisplay = dsi_mode->timing.v_active;
	drm_mode->vsync_start = drm_mode->vdisplay +
				dsi_mode->timing.v_front_porch;
	drm_mode->vsync_end = drm_mode->vsync_start +
			      dsi_mode->timing.v_sync_width;
	drm_mode->vtotal = drm_mode->vsync_end + dsi_mode->timing.v_back_porch;

	drm_mode->vrefresh = dsi_mode->timing.refresh_rate;
	drm_mode->clock = dsi_mode->pixel_clk_khz;

	drm_mode->private = (int *)dsi_mode->priv_info;

	if (dsi_mode->dsi_mode_flags & DSI_MODE_FLAG_SEAMLESS)
		drm_mode->flags |= DRM_MODE_FLAG_SEAMLESS;
	if (dsi_mode->dsi_mode_flags & DSI_MODE_FLAG_DFPS)
		drm_mode->private_flags |= MSM_MODE_FLAG_SEAMLESS_DYNAMIC_FPS;
	if (dsi_mode->dsi_mode_flags & DSI_MODE_FLAG_VBLANK_PRE_MODESET)
		drm_mode->private_flags |= MSM_MODE_FLAG_VBLANK_PRE_MODESET;
	if (dsi_mode->dsi_mode_flags & DSI_MODE_FLAG_DMS)
		drm_mode->private_flags |= MSM_MODE_FLAG_SEAMLESS_DMS;
	if (dsi_mode->dsi_mode_flags & DSI_MODE_FLAG_VRR)
		drm_mode->private_flags |= MSM_MODE_FLAG_SEAMLESS_VRR;

	if (dsi_mode->timing.h_sync_polarity)
		drm_mode->flags |= DRM_MODE_FLAG_PHSYNC;
	if (dsi_mode->timing.v_sync_polarity)
		drm_mode->flags |= DRM_MODE_FLAG_PVSYNC;

	drm_mode_set_name(drm_mode);
}

static int dsi_bridge_attach(struct drm_bridge *bridge)
{
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);

	if (!bridge) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	pr_debug("[%d] attached\n", c_bridge->id);

	return 0;

}

static void dsi_bridge_pre_enable(struct drm_bridge *bridge)
{
	int rc = 0;
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);
	struct drm_device *dev = bridge->dev;
	int event = 0;

	if (dev->doze_state == DRM_BLANK_POWERDOWN) {
		dev->doze_state = DRM_BLANK_UNBLANK;
		pr_info("%s power on from power off\n", __func__);
	}

	event = dev->doze_state;

	g_notify_data.data = &event;

	if (!bridge) {
		pr_err("Invalid params\n");
		return;
	}

	if (!c_bridge || !c_bridge->display || !c_bridge->display->panel) {
		pr_err("Incorrect bridge details\n");
		return;
	}

	atomic_set(&c_bridge->display->panel->esd_recovery_pending, 0);

	if (c_bridge->display->is_prim_display && atomic_read(&prim_panel_is_on)) {
		cancel_delayed_work_sync(&prim_panel_work);
		__pm_relax(&prim_panel_wakelock);
		if (dev->fp_quickon &&
			(dev->doze_state == DRM_BLANK_LP1 || dev->doze_state == DRM_BLANK_LP2)) {
			event = DRM_BLANK_POWERDOWN;
			drm_notifier_call_chain(DRM_EARLY_EVENT_BLANK, &g_notify_data);
			drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data);
			dev->fp_quickon = false;
		}
		pr_info("%s panel already on\n", __func__);
		return;
	}

	drm_notifier_call_chain(DRM_EARLY_EVENT_BLANK, &g_notify_data);
	/* By this point mode should have been validated through mode_fixup */
	rc = dsi_display_set_mode(c_bridge->display,
			&(c_bridge->dsi_mode), 0x0);
	if (rc) {
		pr_err("[%d] failed to perform a mode set, rc=%d\n",
		       c_bridge->id, rc);
		return;
	}

	if (c_bridge->dsi_mode.dsi_mode_flags &
		(DSI_MODE_FLAG_SEAMLESS | DSI_MODE_FLAG_VRR)) {
		pr_debug("[%d] seamless pre-enable\n", c_bridge->id);
		return;
	}

	SDE_ATRACE_BEGIN("dsi_bridge_pre_enable");
	pr_debug("[lcd_performance]dsi_bridge_pre_enable -- start");

	pr_debug("[lcd_performance]dsi_display_prepare -- start");
	rc = dsi_display_prepare(c_bridge->display);
	if (rc) {
		pr_err("[%d] DSI display prepare failed, rc=%d\n",
		       c_bridge->id, rc);
		SDE_ATRACE_END("dsi_bridge_pre_enable");
		pr_debug("[lcd_performance]dsi_bridge_pre_enable -- end");
		return;
	}
	pr_debug("[lcd_performance]dsi_display_prepare -- end");

	SDE_ATRACE_BEGIN("dsi_display_enable");
	pr_debug("[lcd_performance]dsi_display_enable -- start");
	rc = dsi_display_enable(c_bridge->display);
	if (rc) {
		pr_err("[%d] DSI display enable failed, rc=%d\n",
				c_bridge->id, rc);
		(void)dsi_display_unprepare(c_bridge->display);
	}

	pr_debug("[lcd_performance]drm_notifier_call_chain DRM_EVENT_BLANK -- start");
	drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data);
	pr_debug("[lcd_performance]drm_notifier_call_chain DRM_EVENT_BLANK -- end");

	pr_debug("[lcd_performance]dsi_display_enable -- end");
	SDE_ATRACE_END("dsi_display_enable");
	pr_debug("[lcd_performance]dsi_bridge_pre_enable -- end");
	SDE_ATRACE_END("dsi_bridge_pre_enable");


	rc = dsi_display_splash_res_cleanup(c_bridge->display);
	if (rc)
		pr_err("Continuous splash pipeline cleanup failed, rc=%d\n",
									rc);
	if (c_bridge->display->is_prim_display)
		atomic_set(&prim_panel_is_on, true);
}

/**
 *  dsi_bridge_interface_enable - Panel light on interface for fingerprint
 *  In order to improve panel light on performance when unlock device by
 *  fingerprint, export this interface for fingerprint.Once finger touch
 *  happened, it could light on LCD panel in advance of android resume.
 *
 *  @timeout: DSI bridge wait time for android resume and set panel on.
 *            If timeout, dsi bridge will disable panel to avoid fingerprint
 *            touch by mistake.
 */

int dsi_bridge_interface_enable(int timeout)
{
	int ret = 0;

	ret = wait_event_timeout(resume_wait_q,
		!atomic_read(&resume_pending),
		msecs_to_jiffies(WAIT_RESUME_TIMEOUT));
	if (!ret) {
		pr_info("Primary fb resume timeout\n");
		return -ETIMEDOUT;
	}

	mutex_lock(&gbridge->base.lock);

	if (atomic_read(&prim_panel_is_on)) {
		mutex_unlock(&gbridge->base.lock);
		return 0;
	}

	gbridge->base.dev->fp_quickon = true;

	__pm_stay_awake(&prim_panel_wakelock);
	dsi_bridge_pre_enable(&gbridge->base);

	if (timeout > 0)
		schedule_delayed_work(&prim_panel_work, msecs_to_jiffies(timeout));
	else
		__pm_relax(&prim_panel_wakelock);

	mutex_unlock(&gbridge->base.lock);
	return ret;
}
EXPORT_SYMBOL(dsi_bridge_interface_enable);

int panel_disp_param_send(struct dsi_display *display, int cmd);
static void dsi_bridge_disp_param_set(struct drm_bridge *bridge, int cmd)
{
	int rc = 0;
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);

	if (!c_bridge) {
		pr_err("Invalid params\n");
		return;
	}

	SDE_ATRACE_BEGIN("panel_disp_param_send");
	rc = panel_disp_param_send(c_bridge->display, cmd);
	if (rc) {
		pr_err("[%d] DSI disp param send failed, rc=%d\n",
		       c_bridge->id, rc);
	}
	SDE_ATRACE_END("panel_disp_param_send");
}

static ssize_t dsi_bridge_disp_param_get(struct drm_bridge *bridge, char *buf)
{
	struct dsi_bridge *c_bridge;
	struct dsi_display *display;
	struct dsi_panel *panel;
	ssize_t ret = 0;

	if (!bridge) {
		pr_err("Invalid params\n");
		return 0;
	} else {
		SDE_ATRACE_BEGIN("panel_disp_param_get");
		pr_debug("[lcd_performance]panel_disp_param_get -- start");
		c_bridge = to_dsi_bridge(bridge);
		if (c_bridge == NULL)
			return 0;
		display = c_bridge->display;
		if (display == NULL)
			return 0;
		panel = display->panel;
		if (panel) {
			ret = strlen(panel->panel_read_data);
			ret = ret > 255 ? 255 : ret;
			if (ret > 0)
				memcpy(buf, panel->panel_read_data, ret);
		}
		pr_debug("[lcd_performance]panel_disp_param_get -- end");
		SDE_ATRACE_END("panel_disp_param_get");
	}
	return ret;
}

static int dsi_bridge_get_panel_info(struct drm_bridge *bridge, char *buf)
{
	int rc = 0;
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);

	if (!c_bridge) {
		pr_err("Invalid params\n");
		return rc;
	}

	if (c_bridge->display->name)
		return snprintf(buf, PAGE_SIZE, c_bridge->display->name);

	return rc;
}

static void dsi_bridge_enable(struct drm_bridge *bridge)
{
	int rc = 0;
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);
	struct dsi_display *display;

	if (!bridge) {
		pr_err("Invalid params\n");
		return;
	}

	if (c_bridge->dsi_mode.dsi_mode_flags &
			(DSI_MODE_FLAG_SEAMLESS | DSI_MODE_FLAG_VRR)) {
		pr_debug("[%d] seamless enable\n", c_bridge->id);
		return;
	}
	display = c_bridge->display;

	pr_debug("[lcd_performance]dsi_display_post_enable -- start");
	rc = dsi_display_post_enable(display);
	if (rc)
		pr_err("[%d] DSI display post enabled failed, rc=%d\n",
		       c_bridge->id, rc);
	pr_debug("[lcd_performance]dsi_display_post_enable -- start");

	if (display && display->drm_conn)
		sde_connector_helper_bridge_enable(display->drm_conn);
}

static void dsi_bridge_disable(struct drm_bridge *bridge)
{
	int rc = 0;
	struct dsi_display *display;
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);

	if (!bridge) {
		pr_err("Invalid params\n");
		return;
	}
	display = c_bridge->display;

	if (display && display->drm_conn)
		sde_connector_helper_bridge_disable(display->drm_conn);

	pr_debug("[lcd_performance]dsi_display_pre_disable -- start");
	rc = dsi_display_pre_disable(c_bridge->display);
	if (rc) {
		pr_err("[%d] DSI display pre disable failed, rc=%d\n",
		       c_bridge->id, rc);
	}
	pr_debug("[lcd_performance]dsi_display_pre_disable -- start");
}

static void dsi_bridge_post_disable(struct drm_bridge *bridge)
{
	int rc = 0;
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);
	struct drm_device *dev = bridge->dev;
	int event = 0;

	if (dev->doze_state == DRM_BLANK_UNBLANK) {
		dev->doze_state = DRM_BLANK_POWERDOWN;
		pr_info("%s wrong doze state\n", __func__);
	}

	event = dev->doze_state;

	g_notify_data.data = &event;

	if (!bridge) {
		pr_err("Invalid params\n");
		return;
	}

	if (c_bridge->display->is_prim_display && !atomic_read(&prim_panel_is_on)) {
		pr_err("%s Already power off\n", __func__);
		return;
	}

	if (dev->doze_state == DRM_BLANK_LP1 || dev->doze_state == DRM_BLANK_LP2) {
		pr_err("%s doze state can't power off panel\n", __func__);
		event = DRM_BLANK_POWERDOWN;
		drm_notifier_call_chain(DRM_EARLY_EVENT_BLANK, &g_notify_data);
		drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data);
		return;
	}

	drm_notifier_call_chain(DRM_EARLY_EVENT_BLANK, &g_notify_data);

	SDE_ATRACE_BEGIN("dsi_bridge_post_disable");
	pr_debug("[lcd_performance]dsi_bridge_post_disable -- start");
	SDE_ATRACE_BEGIN("dsi_display_disable");
	pr_debug("[lcd_performance]dsi_display_disable -- start");
	rc = dsi_display_disable(c_bridge->display);
	if (rc) {
		pr_err("[%d] DSI display disable failed, rc=%d\n",
		       c_bridge->id, rc);
		pr_debug("[lcd_performance]dsi_display_disable -- end");
		SDE_ATRACE_END("dsi_display_disable");
		return;
	}
	pr_debug("[lcd_performance]dsi_display_disable -- end");
	SDE_ATRACE_END("dsi_display_disable");

	pr_debug("[lcd_performance]dsi_display_unprepare -- start");
	rc = dsi_display_unprepare(c_bridge->display);
	if (rc) {
		pr_err("[%d] DSI display unprepare failed, rc=%d\n",
		       c_bridge->id, rc);
		pr_debug("[lcd_performance]dsi_bridge_post_disable -- end");
		SDE_ATRACE_END("dsi_bridge_post_disable");
		return;
	}
	pr_debug("[lcd_performance]dsi_display_unprepare -- end");
	pr_debug("[lcd_performance]dsi_bridge_post_disable -- end");
	SDE_ATRACE_END("dsi_bridge_post_disable");

	drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data);

	if (gbridge)
		gbridge->base.dev->fp_quickon = false;

	if (c_bridge->display->is_prim_display)
		atomic_set(&prim_panel_is_on, false);
}

static void prim_panel_off_delayed_work(struct work_struct *work)
{
	mutex_lock(&gbridge->base.lock);
	if (atomic_read(&prim_panel_is_on)) {
		dsi_bridge_post_disable(&gbridge->base);
		__pm_relax(&prim_panel_wakelock);
		gbridge->base.dev->fp_quickon = false;
		mutex_unlock(&gbridge->base.lock);
		return;
	}
	mutex_unlock(&gbridge->base.lock);
}

static void dsi_bridge_mode_set(struct drm_bridge *bridge,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);

	if (!bridge || !mode || !adjusted_mode) {
		pr_err("Invalid params\n");
		return;
	}

	memset(&(c_bridge->dsi_mode), 0x0, sizeof(struct dsi_display_mode));
	convert_to_dsi_mode(adjusted_mode, &(c_bridge->dsi_mode));
}

static bool dsi_bridge_mode_fixup(struct drm_bridge *bridge,
				  const struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	int rc = 0;
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);
	struct dsi_display *display;
	struct dsi_display_mode dsi_mode, cur_dsi_mode, *panel_dsi_mode;
	struct drm_display_mode cur_mode;
	struct drm_crtc_state *crtc_state;

	crtc_state = container_of(mode, struct drm_crtc_state, mode);

	if (!bridge || !mode || !adjusted_mode) {
		pr_err("Invalid params\n");
		return false;
	}

	display = c_bridge->display;
	if (!display) {
		pr_err("Invalid params\n");
		return false;
	}

	convert_to_dsi_mode(mode, &dsi_mode);

	/* external bridge doesn't use priv_info and dsi_mode_flags */
	if (!dsi_display_has_ext_bridge(display)) {
		/*
		 * retrieve dsi mode from dsi driver's cache since not safe to
		 * take the drm mode config mutex in all paths
		 */
		rc = dsi_display_find_mode(display, &dsi_mode, &panel_dsi_mode);
		if (rc)
			return rc;

		/*
		 * propagate the private info to the adjusted_mode derived dsi
		 * mode
		 */
		dsi_mode.priv_info = panel_dsi_mode->priv_info;
		dsi_mode.dsi_mode_flags = panel_dsi_mode->dsi_mode_flags;
	}

	rc = dsi_display_validate_mode(c_bridge->display, &dsi_mode,
			DSI_VALIDATE_FLAG_ALLOW_ADJUST);
	if (rc) {
		pr_err("[%d] mode is not valid, rc=%d\n", c_bridge->id, rc);
		return false;
	}

	if (bridge->encoder && bridge->encoder->crtc &&
			crtc_state->crtc) {

		convert_to_dsi_mode(&crtc_state->crtc->state->mode,
							&cur_dsi_mode);
		rc = dsi_display_validate_mode_vrr(c_bridge->display,
					&cur_dsi_mode, &dsi_mode);
		if (rc)
			pr_debug("[%s] vrr mode mismatch failure rc=%d\n",
				c_bridge->display->name, rc);

		cur_mode = crtc_state->crtc->mode;

		/* No DMS/VRR when drm pipeline is changing */
		if (!drm_mode_equal(&cur_mode, adjusted_mode) &&
			(!(dsi_mode.dsi_mode_flags & DSI_MODE_FLAG_VRR)) &&
			(!crtc_state->active_changed ||
			 display->is_cont_splash_enabled))
			dsi_mode.dsi_mode_flags |= DSI_MODE_FLAG_DMS;
	}

	/* convert back to drm mode, propagating the private info & flags */
	dsi_convert_to_drm_mode(&dsi_mode, adjusted_mode);

	return true;
}

int dsi_conn_get_mode_info(const struct drm_display_mode *drm_mode,
	struct msm_mode_info *mode_info,
	u32 max_mixer_width, void *display)
{
	struct dsi_display_mode dsi_mode;
	struct dsi_mode_info *timing;

	if (!drm_mode || !mode_info)
		return -EINVAL;

	convert_to_dsi_mode(drm_mode, &dsi_mode);

	if (!dsi_mode.priv_info)
		return -EINVAL;

	memset(mode_info, 0, sizeof(*mode_info));

	timing = &dsi_mode.timing;
	mode_info->frame_rate = dsi_mode.timing.refresh_rate;
	mode_info->vtotal = DSI_V_TOTAL(timing);
	mode_info->prefill_lines = dsi_mode.priv_info->panel_prefill_lines;
	mode_info->jitter_numer = dsi_mode.priv_info->panel_jitter_numer;
	mode_info->jitter_denom = dsi_mode.priv_info->panel_jitter_denom;
	mode_info->clk_rate = dsi_mode.priv_info->clk_rate_hz;

	memcpy(&mode_info->topology, &dsi_mode.priv_info->topology,
			sizeof(struct msm_display_topology));

	mode_info->comp_info.comp_type = MSM_DISPLAY_COMPRESSION_NONE;
	if (dsi_mode.priv_info->dsc_enabled) {
		mode_info->comp_info.comp_type = MSM_DISPLAY_COMPRESSION_DSC;
		memcpy(&mode_info->comp_info.dsc_info, &dsi_mode.priv_info->dsc,
			sizeof(dsi_mode.priv_info->dsc));
	}

	if (dsi_mode.priv_info->roi_caps.enabled) {
		memcpy(&mode_info->roi_caps, &dsi_mode.priv_info->roi_caps,
			sizeof(dsi_mode.priv_info->roi_caps));
	}

	return 0;
}

int dsi_conn_ext_bridge_get_mode_info(const struct drm_display_mode *drm_mode,
	struct msm_mode_info *mode_info,
	u32 max_mixer_width, void *display)
{
	struct msm_display_topology *topology;
	struct dsi_display_mode dsi_mode;
	struct dsi_mode_info *timing;

	if (!drm_mode || !mode_info)
		return -EINVAL;

	convert_to_dsi_mode(drm_mode, &dsi_mode);

	memset(mode_info, 0, sizeof(*mode_info));

	timing = &dsi_mode.timing;
	mode_info->frame_rate = dsi_mode.timing.refresh_rate;
	mode_info->vtotal = DSI_V_TOTAL(timing);

	topology = &mode_info->topology;
	topology->num_lm = (max_mixer_width <= drm_mode->hdisplay) ? 2 : 1;
	topology->num_enc = 0;
	topology->num_intf = topology->num_lm;

	mode_info->comp_info.comp_type = MSM_DISPLAY_COMPRESSION_NONE;

	return 0;
}

static const struct drm_bridge_funcs dsi_bridge_ops = {
	.attach       = dsi_bridge_attach,
	.mode_fixup   = dsi_bridge_mode_fixup,
	.pre_enable   = dsi_bridge_pre_enable,
	.enable       = dsi_bridge_enable,
	.disable      = dsi_bridge_disable,
	.post_disable = dsi_bridge_post_disable,
	.mode_set     = dsi_bridge_mode_set,
	.disp_param_set = dsi_bridge_disp_param_set,
	.disp_get_panel_info = dsi_bridge_get_panel_info,
	.disp_param_get = dsi_bridge_disp_param_get,
};

int dsi_conn_set_info_blob(struct drm_connector *connector,
		void *info, void *display, struct msm_mode_info *mode_info)
{
	struct dsi_display *dsi_display = display;
	struct dsi_panel *panel;

	if (!info || !dsi_display)
		return -EINVAL;

	dsi_display->drm_conn = connector;

	sde_kms_info_add_keystr(info,
		"display type", dsi_display->display_type);

	switch (dsi_display->type) {
	case DSI_DISPLAY_SINGLE:
		sde_kms_info_add_keystr(info, "display config",
					"single display");
		break;
	case DSI_DISPLAY_EXT_BRIDGE:
		sde_kms_info_add_keystr(info, "display config", "ext bridge");
		break;
	case DSI_DISPLAY_SPLIT:
		sde_kms_info_add_keystr(info, "display config",
					"split display");
		break;
	case DSI_DISPLAY_SPLIT_EXT_BRIDGE:
		sde_kms_info_add_keystr(info, "display config",
					"split ext bridge");
		break;
	default:
		pr_debug("invalid display type:%d\n", dsi_display->type);
		break;
	}

	if (!dsi_display->panel) {
		pr_debug("invalid panel data\n");
		goto end;
	}

	panel = dsi_display->panel;
	sde_kms_info_add_keystr(info, "panel name", panel->name);

	switch (panel->panel_mode) {
	case DSI_OP_VIDEO_MODE:
		sde_kms_info_add_keystr(info, "panel mode", "video");
		break;
	case DSI_OP_CMD_MODE:
		sde_kms_info_add_keystr(info, "panel mode", "command");
		sde_kms_info_add_keyint(info, "mdp_transfer_time_us",
				panel->cmd_config.mdp_transfer_time_us);
		break;
	default:
		pr_debug("invalid panel type:%d\n", panel->panel_mode);
		break;
	}
	sde_kms_info_add_keystr(info, "dfps support",
			panel->dfps_caps.dfps_support ? "true" : "false");

	if (panel->dfps_caps.dfps_support) {
		sde_kms_info_add_keyint(info, "min_fps",
			panel->dfps_caps.min_refresh_rate);
		sde_kms_info_add_keyint(info, "max_fps",
			panel->dfps_caps.max_refresh_rate);
	}

	switch (panel->phy_props.rotation) {
	case DSI_PANEL_ROTATE_NONE:
		sde_kms_info_add_keystr(info, "panel orientation", "none");
		break;
	case DSI_PANEL_ROTATE_H_FLIP:
		sde_kms_info_add_keystr(info, "panel orientation", "horz flip");
		break;
	case DSI_PANEL_ROTATE_V_FLIP:
		sde_kms_info_add_keystr(info, "panel orientation", "vert flip");
		break;
	case DSI_PANEL_ROTATE_HV_FLIP:
		sde_kms_info_add_keystr(info, "panel orientation",
							"horz & vert flip");
		break;
	default:
		pr_debug("invalid panel rotation:%d\n",
						panel->phy_props.rotation);
		break;
	}

	switch (panel->bl_config.type) {
	case DSI_BACKLIGHT_PWM:
		sde_kms_info_add_keystr(info, "backlight type", "pwm");
		break;
	case DSI_BACKLIGHT_WLED:
		sde_kms_info_add_keystr(info, "backlight type", "wled");
		break;
	case DSI_BACKLIGHT_DCS:
		sde_kms_info_add_keystr(info, "backlight type", "dcs");
		break;
	default:
		pr_debug("invalid panel backlight type:%d\n",
						panel->bl_config.type);
		break;
	}

	if (mode_info && mode_info->roi_caps.enabled) {
		sde_kms_info_add_keyint(info, "partial_update_num_roi",
				mode_info->roi_caps.num_roi);
		sde_kms_info_add_keyint(info, "partial_update_xstart",
				mode_info->roi_caps.align.xstart_pix_align);
		sde_kms_info_add_keyint(info, "partial_update_walign",
				mode_info->roi_caps.align.width_pix_align);
		sde_kms_info_add_keyint(info, "partial_update_wmin",
				mode_info->roi_caps.align.min_width);
		sde_kms_info_add_keyint(info, "partial_update_ystart",
				mode_info->roi_caps.align.ystart_pix_align);
		sde_kms_info_add_keyint(info, "partial_update_halign",
				mode_info->roi_caps.align.height_pix_align);
		sde_kms_info_add_keyint(info, "partial_update_hmin",
				mode_info->roi_caps.align.min_height);
		sde_kms_info_add_keyint(info, "partial_update_roimerge",
				mode_info->roi_caps.merge_rois);
	}

end:
	return 0;
}

enum drm_connector_status dsi_conn_detect(struct drm_connector *conn,
		bool force,
		void *display)
{
	enum drm_connector_status status = connector_status_unknown;
	struct msm_display_info info;
	int rc;

	if (!conn || !display)
		return status;

	/* get display dsi_info */
	memset(&info, 0x0, sizeof(info));
	rc = dsi_display_get_info(&info, display);
	if (rc) {
		pr_err("failed to get display info, rc=%d\n", rc);
		return connector_status_disconnected;
	}

	if (info.capabilities & MSM_DISPLAY_CAP_HOT_PLUG)
		status = (info.is_connected ? connector_status_connected :
					      connector_status_disconnected);
	else
		status = connector_status_connected;

	conn->display_info.width_mm = info.width_mm;
	conn->display_info.height_mm = info.height_mm;

	return status;
}

void dsi_connector_put_modes(struct drm_connector *connector,
	void *display)
{
	struct drm_display_mode *drm_mode;
	struct dsi_display_mode dsi_mode;

	if (!connector || !display)
		return;

	 list_for_each_entry(drm_mode, &connector->modes, head) {
		convert_to_dsi_mode(drm_mode, &dsi_mode);
		dsi_display_put_mode(display, &dsi_mode);
	}
}

int dsi_connector_get_modes(struct drm_connector *connector,
		void *display)
{
	u32 count = 0;
	struct dsi_display_mode *modes = NULL;
	struct drm_display_mode drm_mode;
	int rc, i;

	if (sde_connector_get_panel(connector)) {
		/*
		 * TODO: If drm_panel is attached, query modes from the panel.
		 * This is complicated in split dsi cases because panel is not
		 * attached to both connectors.
		 */
		goto end;
	}
	rc = dsi_display_get_mode_count(display, &count);
	if (rc) {
		pr_err("failed to get num of modes, rc=%d\n", rc);
		goto end;
	}

	rc = dsi_display_get_modes(display, &modes);
	if (rc) {
		pr_err("failed to get modes, rc=%d\n", rc);
		count = 0;
		goto end;
	}

	for (i = 0; i < count; i++) {
		struct drm_display_mode *m;

		memset(&drm_mode, 0x0, sizeof(drm_mode));
		dsi_convert_to_drm_mode(&modes[i], &drm_mode);
		m = drm_mode_duplicate(connector->dev, &drm_mode);
		if (!m) {
			pr_err("failed to add mode %ux%u\n",
			       drm_mode.hdisplay,
			       drm_mode.vdisplay);
			count = -ENOMEM;
			goto end;
		}
		m->width_mm = connector->display_info.width_mm;
		m->height_mm = connector->display_info.height_mm;
		drm_mode_probed_add(connector, m);
	}
end:
	pr_debug("MODE COUNT =%d\n\n", count);
	return count;
}

enum drm_mode_status dsi_conn_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode,
		void *display)
{
	struct dsi_display_mode dsi_mode;
	int rc;

	if (!connector || !mode) {
		pr_err("Invalid params\n");
		return MODE_ERROR;
	}

	convert_to_dsi_mode(mode, &dsi_mode);

	rc = dsi_display_validate_mode(display, &dsi_mode,
			DSI_VALIDATE_FLAG_ALLOW_ADJUST);
	if (rc) {
		pr_err("mode not supported, rc=%d\n", rc);
		return MODE_BAD;
	}

	return MODE_OK;
}

int dsi_conn_pre_kickoff(struct drm_connector *connector,
		void *display,
		struct msm_display_kickoff_params *params)
{
	if (!connector || !display || !params) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	return dsi_display_pre_kickoff(display, params);
}

void dsi_conn_enable_event(struct drm_connector *connector,
		uint32_t event_idx, bool enable, void *display)
{
	struct dsi_event_cb_info event_info;

	memset(&event_info, 0, sizeof(event_info));

	event_info.event_cb = sde_connector_trigger_event;
	event_info.event_usr_ptr = connector;

	dsi_display_enable_event(display, event_idx, &event_info, enable);
}

int dsi_conn_post_kickoff(struct drm_connector *connector)
{
	struct drm_encoder *encoder;
	struct dsi_bridge *c_bridge;
	struct dsi_display_mode adj_mode;
	struct dsi_display *display;
	struct dsi_display_ctrl *m_ctrl, *ctrl;
	int i, rc = 0;

	if (!connector || !connector->state) {
		pr_err("invalid connector or connector state");
		return -EINVAL;
	}

	encoder = connector->state->best_encoder;
	if (!encoder) {
		pr_debug("best encoder is not available");
		return 0;
	}

	c_bridge = to_dsi_bridge(encoder->bridge);
	adj_mode = c_bridge->dsi_mode;
	display = c_bridge->display;

	if (adj_mode.dsi_mode_flags & DSI_MODE_FLAG_VRR) {
		m_ctrl = &display->ctrl[display->clk_master_idx];
		rc = dsi_ctrl_timing_db_update(m_ctrl->ctrl, false);
		if (rc) {
			pr_err("[%s] failed to dfps update  rc=%d\n",
				display->name, rc);
			return -EINVAL;
		}

		/* Update the rest of the controllers */
		for (i = 0; i < display->ctrl_count; i++) {
			ctrl = &display->ctrl[i];
			if (!ctrl->ctrl || (ctrl == m_ctrl))
				continue;

			rc = dsi_ctrl_timing_db_update(ctrl->ctrl, false);
			if (rc) {
				pr_err("[%s] failed to dfps update rc=%d\n",
					display->name,  rc);
				return -EINVAL;
			}
		}

		c_bridge->dsi_mode.dsi_mode_flags &= ~DSI_MODE_FLAG_VRR;
	}

	return 0;
}

struct dsi_bridge *dsi_drm_bridge_init(struct dsi_display *display,
				       struct drm_device *dev,
				       struct drm_encoder *encoder)
{
	int rc = 0;
	struct dsi_bridge *bridge;

	bridge = kzalloc(sizeof(*bridge), GFP_KERNEL);
	if (!bridge) {
		rc = -ENOMEM;
		goto error;
	}

	bridge->display = display;
	bridge->base.funcs = &dsi_bridge_ops;
	bridge->base.encoder = encoder;

	rc = drm_bridge_attach(dev, &bridge->base);
	if (rc) {
		pr_err("failed to attach bridge, rc=%d\n", rc);
		goto error_free_bridge;
	}

	encoder->bridge = &bridge->base;
	encoder->bridge->is_dsi_drm_bridge = true;
	mutex_init(&encoder->bridge->lock);

	if (display->is_prim_display) {
		gbridge = bridge;
		atomic_set(&resume_pending, 0);
		wakeup_source_init(&prim_panel_wakelock, "prim_panel_wakelock");
		atomic_set(&prim_panel_is_on, false);
		init_waitqueue_head(&resume_wait_q);
		INIT_DELAYED_WORK(&prim_panel_work, prim_panel_off_delayed_work);
	}

	return bridge;
error_free_bridge:
	kfree(bridge);
error:
	return ERR_PTR(rc);
}

void dsi_drm_bridge_cleanup(struct dsi_bridge *bridge)
{
	if (bridge && bridge->base.encoder)
		bridge->base.encoder->bridge = NULL;

	if (bridge == gbridge) {
		atomic_set(&prim_panel_is_on, false);
		cancel_delayed_work_sync(&prim_panel_work);
		wakeup_source_trash(&prim_panel_wakelock);
	}

	kfree(bridge);
}
