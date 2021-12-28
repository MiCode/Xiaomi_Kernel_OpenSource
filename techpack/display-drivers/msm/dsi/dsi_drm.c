// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */


#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic.h>

#include "msm_kms.h"
#include "sde_connector.h"
#include "dsi_drm.h"
#include "sde_trace.h"
#include "sde_dbg.h"
#include "msm_drv.h"
#include "sde_encoder.h"

#include "mi_disp_print.h"
#include "mi_dsi_display.h"
#include "mi_panel_id.h"

#define to_dsi_bridge(x)     container_of((x), struct dsi_bridge, base)
#define to_dsi_state(x)      container_of((x), struct dsi_connector_state, base)

#define DEFAULT_PANEL_JITTER_NUMERATOR		2
#define DEFAULT_PANEL_JITTER_DENOMINATOR	1
#define DEFAULT_PANEL_JITTER_ARRAY_SIZE		2
#define DEFAULT_PANEL_PREFILL_LINES	25

static struct dsi_display_mode_priv_info default_priv_info = {
	.panel_jitter_numer = DEFAULT_PANEL_JITTER_NUMERATOR,
	.panel_jitter_denom = DEFAULT_PANEL_JITTER_DENOMINATOR,
	.panel_prefill_lines = DEFAULT_PANEL_PREFILL_LINES,
	.dsc_enabled = false,
};

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

	dsi_mode->timing.refresh_rate = drm_mode_vrefresh(drm_mode);

	dsi_mode->timing.h_sync_polarity =
			!!(drm_mode->flags & DRM_MODE_FLAG_PHSYNC);
	dsi_mode->timing.v_sync_polarity =
			!!(drm_mode->flags & DRM_MODE_FLAG_PVSYNC);
}

static void msm_parse_mode_priv_info(const struct msm_display_mode *msm_mode,
				struct dsi_display_mode *dsi_mode)
{
	dsi_mode->priv_info =
		(struct dsi_display_mode_priv_info *)msm_mode->private;

	if (dsi_mode->priv_info) {
		dsi_mode->timing.dsc_enabled = dsi_mode->priv_info->dsc_enabled;
		dsi_mode->timing.dsc = &dsi_mode->priv_info->dsc;
		dsi_mode->timing.vdc_enabled = dsi_mode->priv_info->vdc_enabled;
		dsi_mode->timing.vdc = &dsi_mode->priv_info->vdc;
		dsi_mode->timing.pclk_scale = dsi_mode->priv_info->pclk_scale;
		dsi_mode->timing.clk_rate_hz = dsi_mode->priv_info->clk_rate_hz;
	}

	if (msm_is_mode_seamless(msm_mode))
		dsi_mode->dsi_mode_flags |= DSI_MODE_FLAG_SEAMLESS;
	if (msm_is_mode_dynamic_fps(msm_mode))
		dsi_mode->dsi_mode_flags |= DSI_MODE_FLAG_DFPS;
	if (msm_needs_vblank_pre_modeset(msm_mode))
		dsi_mode->dsi_mode_flags |= DSI_MODE_FLAG_VBLANK_PRE_MODESET;
	if (msm_is_mode_seamless_dms(msm_mode))
		dsi_mode->dsi_mode_flags |= DSI_MODE_FLAG_DMS;
	if (msm_is_mode_seamless_vrr(msm_mode))
		dsi_mode->dsi_mode_flags |= DSI_MODE_FLAG_VRR;
	if (msm_is_mode_seamless_poms_to_vid(msm_mode))
		dsi_mode->dsi_mode_flags |= DSI_MODE_FLAG_POMS_TO_VID;
	if (msm_is_mode_seamless_poms_to_cmd(msm_mode))
		dsi_mode->dsi_mode_flags |= DSI_MODE_FLAG_POMS_TO_CMD;
	if (msm_is_mode_seamless_dyn_clk(msm_mode))
		dsi_mode->dsi_mode_flags |= DSI_MODE_FLAG_DYN_CLK;
}

void dsi_convert_to_drm_mode(const struct dsi_display_mode *dsi_mode,
				struct drm_display_mode *drm_mode)
{
	char *panel_caps = "vid";

	if ((dsi_mode->panel_mode_caps & DSI_OP_VIDEO_MODE) &&
		(dsi_mode->panel_mode_caps & DSI_OP_CMD_MODE))
		panel_caps = "vid_cmd";
	else if (dsi_mode->panel_mode_caps & DSI_OP_VIDEO_MODE)
		panel_caps = "vid";
	else if (dsi_mode->panel_mode_caps & DSI_OP_CMD_MODE)
		panel_caps = "cmd";

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

	drm_mode->clock = drm_mode->htotal * drm_mode->vtotal * dsi_mode->timing.refresh_rate;
	drm_mode->clock /= 1000;

	if (dsi_mode->timing.h_sync_polarity)
		drm_mode->flags |= DRM_MODE_FLAG_PHSYNC;
	if (dsi_mode->timing.v_sync_polarity)
		drm_mode->flags |= DRM_MODE_FLAG_PVSYNC;

	/* set mode name */
	snprintf(drm_mode->name, DRM_DISPLAY_MODE_LEN, "%dx%dx%d%s",
			drm_mode->hdisplay, drm_mode->vdisplay,
			drm_mode_vrefresh(drm_mode), panel_caps);
}

static void dsi_convert_to_msm_mode(const struct dsi_display_mode *dsi_mode,
				struct msm_display_mode *msm_mode)
{
	msm_mode->private_flags = 0;
	msm_mode->private = (int *)dsi_mode->priv_info;

	if (dsi_mode->dsi_mode_flags & DSI_MODE_FLAG_SEAMLESS)
		msm_mode->private_flags |= DRM_MODE_FLAG_SEAMLESS;
	if (dsi_mode->dsi_mode_flags & DSI_MODE_FLAG_DFPS)
		msm_mode->private_flags |= MSM_MODE_FLAG_SEAMLESS_DYNAMIC_FPS;
	if (dsi_mode->dsi_mode_flags & DSI_MODE_FLAG_VBLANK_PRE_MODESET)
		msm_mode->private_flags |= MSM_MODE_FLAG_VBLANK_PRE_MODESET;
	if (dsi_mode->dsi_mode_flags & DSI_MODE_FLAG_DMS)
		msm_mode->private_flags |= MSM_MODE_FLAG_SEAMLESS_DMS;
	if (dsi_mode->dsi_mode_flags & DSI_MODE_FLAG_VRR)
		msm_mode->private_flags |= MSM_MODE_FLAG_SEAMLESS_VRR;
	if (dsi_mode->dsi_mode_flags & DSI_MODE_FLAG_POMS_TO_VID)
		msm_mode->private_flags |= MSM_MODE_FLAG_SEAMLESS_POMS_VID;
	if (dsi_mode->dsi_mode_flags & DSI_MODE_FLAG_POMS_TO_CMD)
		msm_mode->private_flags |= MSM_MODE_FLAG_SEAMLESS_POMS_CMD;
	if (dsi_mode->dsi_mode_flags & DSI_MODE_FLAG_DYN_CLK)
		msm_mode->private_flags |= MSM_MODE_FLAG_SEAMLESS_DYN_CLK;
}

static int dsi_bridge_attach(struct drm_bridge *bridge,
			enum drm_bridge_attach_flags flags)
{
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);

	if (!bridge) {
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	DSI_DEBUG("[%d] attached\n", c_bridge->id);

	return 0;

}

static void dsi_bridge_pre_enable(struct drm_bridge *bridge)
{
	int rc = 0;
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);
	struct dsi_display *display;

	if (!bridge) {
		DSI_ERR("Invalid params\n");
		return;
	}

	if (!c_bridge || !c_bridge->display || !c_bridge->display->panel) {
		DSI_ERR("Incorrect bridge details\n");
		return;
	}

	display = c_bridge->display;

	atomic_set(&c_bridge->display->panel->esd_recovery_pending, 0);

	/* By this point mode should have been validated through mode_fixup */
	rc = dsi_display_set_mode(c_bridge->display,
			&(c_bridge->dsi_mode), 0x0);
	if (rc) {
		DSI_ERR("[%d] failed to perform a mode set, rc=%d\n",
		       c_bridge->id, rc);
		return;
	}

	if (c_bridge->dsi_mode.dsi_mode_flags &
		(DSI_MODE_FLAG_SEAMLESS | DSI_MODE_FLAG_VRR |
		 DSI_MODE_FLAG_DYN_CLK)) {
		DSI_DEBUG("[%d] seamless pre-enable\n", c_bridge->id);
		return;
	}

	SDE_ATRACE_BEGIN("dsi_display_prepare");
	rc = dsi_display_prepare(c_bridge->display);
	if (rc) {
		DSI_ERR("[%d] DSI display prepare failed, rc=%d\n",
		       c_bridge->id, rc);
		SDE_ATRACE_END("dsi_display_prepare");
		return;
	}
	SDE_ATRACE_END("dsi_display_prepare");

	SDE_ATRACE_BEGIN("dsi_display_enable");
	rc = dsi_display_enable(c_bridge->display);
	if (rc) {
		DSI_ERR("[%d] DSI display enable failed, rc=%d\n",
				c_bridge->id, rc);
		(void)dsi_display_unprepare(c_bridge->display);
	}
	SDE_ATRACE_END("dsi_display_enable");

	rc = dsi_display_splash_res_cleanup(c_bridge->display);
	if (rc)
		DSI_ERR("Continuous splash pipeline cleanup failed, rc=%d\n",
									rc);
	sde_connector_update_panel_dead(display->drm_conn, !display->panel->panel_initialized);

}

static void dsi_bridge_enable(struct drm_bridge *bridge)
{
	int rc = 0;
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);
	struct dsi_display *display;

	if (!bridge) {
		DSI_ERR("Invalid params\n");
		return;
	}

	if (c_bridge->dsi_mode.dsi_mode_flags &
			(DSI_MODE_FLAG_SEAMLESS | DSI_MODE_FLAG_VRR |
			 DSI_MODE_FLAG_DYN_CLK)) {
		DSI_DEBUG("[%d] seamless enable\n", c_bridge->id);
		return;
	}
	display = c_bridge->display;

	rc = dsi_display_post_enable(display);
	if (rc)
		DSI_ERR("[%d] DSI display post enabled failed, rc=%d\n",
		       c_bridge->id, rc);

	if (display)
		display->enabled = true;

	if (display && display->drm_conn) {
		sde_connector_helper_bridge_enable(display->drm_conn);
		if (display->poms_pending) {
			display->poms_pending = false;
			sde_connector_schedule_status_work(display->drm_conn,
				true);
		}
	}

	rc = mi_dsi_display_esd_irq_ctrl(c_bridge->display, true);
	if (rc) {
		DISP_ERROR("[%d] DSI display enable esd irq failed, rc=%d\n",
				c_bridge->id, rc);
	}
}

static void dsi_bridge_disable(struct drm_bridge *bridge)
{
	int rc = 0;
	struct dsi_display *display;
	struct sde_connector_state *conn_state;
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);

	if (!bridge) {
		DSI_ERR("Invalid params\n");
		return;
	}
	display = c_bridge->display;

	if (display)
		display->enabled = false;

	rc = mi_dsi_display_esd_irq_ctrl(c_bridge->display, false);
	if (rc) {
		DISP_ERROR("[%d] DSI display disable esd irq failed, rc=%d\n",
				c_bridge->id, rc);
	}

	if (display && display->drm_conn) {
		conn_state = to_sde_connector_state(display->drm_conn->state);
		if (!conn_state) {
			DSI_ERR("invalid params\n");
			return;
		}

		display->poms_pending = msm_is_mode_seamless_poms(
						&conn_state->msm_mode);

		sde_connector_helper_bridge_disable(display->drm_conn);
	}

	rc = dsi_display_pre_disable(c_bridge->display);
	if (rc) {
		DSI_ERR("[%d] DSI display pre disable failed, rc=%d\n",
		       c_bridge->id, rc);
	}
}

static void dsi_bridge_post_disable(struct drm_bridge *bridge)
{
	int rc = 0;
	struct dsi_display *display;
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);

	if (!bridge) {
		DSI_ERR("Invalid params\n");
		return;
	}

	display = c_bridge->display;

	SDE_ATRACE_BEGIN("dsi_bridge_post_disable");
	SDE_ATRACE_BEGIN("dsi_display_disable");
	rc = dsi_display_disable(c_bridge->display);
	if (rc) {
		DSI_ERR("[%d] DSI display disable failed, rc=%d\n",
		       c_bridge->id, rc);
		SDE_ATRACE_END("dsi_display_disable");
		return;
	}
	SDE_ATRACE_END("dsi_display_disable");

	if (display && display->drm_conn)
		sde_connector_helper_bridge_post_disable(display->drm_conn);

	rc = dsi_display_unprepare(c_bridge->display);
	if (rc) {
		DSI_ERR("[%d] DSI display unprepare failed, rc=%d\n",
		       c_bridge->id, rc);
		SDE_ATRACE_END("dsi_bridge_post_disable");
		return;
	}
	SDE_ATRACE_END("dsi_bridge_post_disable");
}

static void dsi_bridge_mode_set(struct drm_bridge *bridge,
				const struct drm_display_mode *mode,
				const struct drm_display_mode *adjusted_mode)
{
	int rc = 0;
	struct dsi_bridge *c_bridge = NULL;
	struct dsi_display *display;
	struct drm_connector *conn;
	struct sde_connector_state *conn_state;

	if (!bridge || !mode || !adjusted_mode) {
		DSI_ERR("Invalid params\n");
		return;
	}

	c_bridge = to_dsi_bridge(bridge);
	if (!c_bridge) {
		DSI_ERR("invalid dsi bridge\n");
		return;
	}

	display = c_bridge->display;
	if (!display || !display->drm_conn || !display->drm_conn->state) {
		DSI_ERR("invalid display\n");
		return;
	}

	memset(&(c_bridge->dsi_mode), 0x0, sizeof(struct dsi_display_mode));
	convert_to_dsi_mode(adjusted_mode, &(c_bridge->dsi_mode));
	conn = sde_encoder_get_connector(bridge->dev, bridge->encoder);
	if (!conn)
		return;

	conn_state = to_sde_connector_state(conn->state);
	if (!conn_state) {
		DSI_ERR("invalid connector state\n");
		return;
	}

	msm_parse_mode_priv_info(&conn_state->msm_mode,
					&(c_bridge->dsi_mode));

	rc = dsi_display_restore_bit_clk(display, &c_bridge->dsi_mode);
	if (rc) {
		DSI_ERR("[%s] bit clk rate cannot be restored\n", display->name);
		return;
	}

	DSI_DEBUG("clk_rate: %llu\n", c_bridge->dsi_mode.timing.clk_rate_hz);
}

static bool dsi_bridge_mode_fixup(struct drm_bridge *bridge,
				  const struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	int rc = 0;
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);
	struct dsi_display *display;
	struct dsi_display_mode dsi_mode, cur_dsi_mode, *panel_dsi_mode;
	struct drm_crtc_state *crtc_state;
	struct drm_connector_state *drm_conn_state;
	struct sde_connector_state *conn_state, *old_conn_state;
	struct msm_sub_mode new_sub_mode;

	crtc_state = container_of(mode, struct drm_crtc_state, mode);

	if (!bridge || !mode || !adjusted_mode) {
		DSI_ERR("invalid params\n");
		return false;
	}

	display = c_bridge->display;
	if (!display || !display->drm_conn || !display->drm_conn->state) {
		DSI_ERR("invalid params\n");
		return false;
	}

	drm_conn_state = drm_atomic_get_new_connector_state(crtc_state->state,
				display->drm_conn);
	conn_state = to_sde_connector_state(drm_conn_state);
	if (!conn_state) {
		DSI_ERR("invalid params\n");
		return false;
	}

	/*
	 * if no timing defined in panel, it must be external mode
	 * and we'll use empty priv info to populate the mode
	 */
	if (display->panel && !display->panel->num_timing_nodes) {
		*adjusted_mode = *mode;
		conn_state->msm_mode.base = adjusted_mode;
		conn_state->msm_mode.private = (int *)&default_priv_info;
		conn_state->msm_mode.private_flags = 0;
		return true;
	}

	convert_to_dsi_mode(mode, &dsi_mode);
	msm_parse_mode_priv_info(&conn_state->msm_mode, &dsi_mode);
	new_sub_mode.dsc_mode = sde_connector_get_property(drm_conn_state,
				CONNECTOR_PROP_DSC_MODE);

	/*
	 * retrieve dsi mode from dsi driver's cache since not safe to take
	 * the drm mode config mutex in all paths
	 */
	rc = dsi_display_find_mode(display, &dsi_mode, &new_sub_mode,
						&panel_dsi_mode);
	if (rc)
		return rc;

	/* propagate the private info to the adjusted_mode derived dsi mode */
	dsi_mode.priv_info = panel_dsi_mode->priv_info;
	dsi_mode.dsi_mode_flags = panel_dsi_mode->dsi_mode_flags;
	dsi_mode.panel_mode_caps = panel_dsi_mode->panel_mode_caps;
	dsi_mode.timing.dsc_enabled = dsi_mode.priv_info->dsc_enabled;
	dsi_mode.timing.dsc = &dsi_mode.priv_info->dsc;

	rc = dsi_display_restore_bit_clk(display, &dsi_mode);
	if (rc) {
		DSI_ERR("[%s] bit clk rate cannot be restored\n", display->name);
		return false;
	}

	rc = dsi_display_update_dyn_bit_clk(display, &dsi_mode);
	if (rc) {
		DSI_ERR("[%s] failed to update bit clock\n", display->name);
		return false;
	}

	rc = dsi_display_validate_mode(c_bridge->display, &dsi_mode,
			DSI_VALIDATE_FLAG_ALLOW_ADJUST);
	if (rc) {
		DSI_ERR("[%d] mode is not valid, rc=%d\n", c_bridge->id, rc);
		return false;
	}

	if (bridge->encoder && bridge->encoder->crtc &&
			crtc_state->crtc) {
		const struct drm_display_mode *cur_mode =
				&crtc_state->crtc->state->mode;
		old_conn_state = to_sde_connector_state(display->drm_conn->state);

		convert_to_dsi_mode(cur_mode, &cur_dsi_mode);
		msm_parse_mode_priv_info(&old_conn_state->msm_mode, &cur_dsi_mode);

		rc = dsi_display_validate_mode_change(c_bridge->display,
					&cur_dsi_mode, &dsi_mode);
		if (rc) {
			DSI_ERR("[%s] seamless mode mismatch failure rc=%d\n",
				c_bridge->display->name, rc);
			return false;
		}

		/*
		 * DMS Flag if set during active changed condition cannot be
		 * treated as seamless. Hence, removing DMS flag in such cases.
		 */
		if ((dsi_mode.dsi_mode_flags & DSI_MODE_FLAG_DMS) &&
				crtc_state->active_changed)
			dsi_mode.dsi_mode_flags &= ~DSI_MODE_FLAG_DMS;

		/* No DMS/VRR when drm pipeline is changing */
		if (!dsi_display_mode_match(&cur_dsi_mode, &dsi_mode,
			DSI_MODE_MATCH_FULL_TIMINGS) &&
			(!(dsi_mode.dsi_mode_flags & DSI_MODE_FLAG_VRR)) &&
			(!(dsi_mode.dsi_mode_flags & DSI_MODE_FLAG_DYN_CLK)) &&
			(!(dsi_mode.dsi_mode_flags & DSI_MODE_FLAG_POMS_TO_VID)) &&
			(!(dsi_mode.dsi_mode_flags & DSI_MODE_FLAG_POMS_TO_CMD)) &&
			(!crtc_state->active_changed ||
			 display->is_cont_splash_enabled)) {
			dsi_mode.dsi_mode_flags |= DSI_MODE_FLAG_DMS;

			SDE_EVT32(SDE_EVTLOG_FUNC_CASE2,
				dsi_mode.timing.h_active,
				dsi_mode.timing.v_active,
				dsi_mode.timing.refresh_rate,
				dsi_mode.pixel_clk_khz,
				dsi_mode.panel_mode_caps);
		}
	}

	/* Reject seamless transition when active changed */
	if (crtc_state->active_changed &&
		((dsi_mode.dsi_mode_flags & DSI_MODE_FLAG_VRR) ||
		(dsi_mode.dsi_mode_flags & DSI_MODE_FLAG_DYN_CLK) ||
		(dsi_mode.dsi_mode_flags & DSI_MODE_FLAG_POMS_TO_VID) ||
		(dsi_mode.dsi_mode_flags & DSI_MODE_FLAG_POMS_TO_CMD))) {
		DSI_INFO("seamless upon active changed 0x%x %d\n",
			dsi_mode.dsi_mode_flags, crtc_state->active_changed);
		return false;
	}

	/* convert back to drm mode, propagating the private info & flags */
	dsi_convert_to_drm_mode(&dsi_mode, adjusted_mode);
	dsi_convert_to_msm_mode(&dsi_mode, &conn_state->msm_mode);

	return true;
}

u32 dsi_drm_get_dfps_maxfps(void *display)
{
	u32 dfps_maxfps = 0;
	struct dsi_display *dsi_display = display;

	/*
	 * The time of SDE transmitting one frame active data
	 * will not be changed, if frame rate is adjusted with
	 * VFP method.
	 * So only return max fps of DFPS for UIDLE update, if DFPS
	 * is enabled with VFP.
	 */
	if (dsi_display && dsi_display->panel &&
		dsi_display->panel->panel_mode == DSI_OP_VIDEO_MODE &&
		dsi_display->panel->dfps_caps.type ==
					DSI_DFPS_IMMEDIATE_VFP)
		dfps_maxfps =
			dsi_display->panel->dfps_caps.max_refresh_rate;

	return dfps_maxfps;
}

int dsi_conn_get_lm_from_mode(void *display, const struct drm_display_mode *drm_mode)
{
	struct dsi_display *dsi_display = display;
	struct dsi_display_mode dsi_mode, *panel_dsi_mode;
	int rc = -EINVAL;

	if (!dsi_display || !drm_mode) {
		DSI_ERR("Invalid params %d %d\n", !display, !drm_mode);
		return rc;
	}

	convert_to_dsi_mode(drm_mode, &dsi_mode);

	rc = dsi_display_find_mode(dsi_display, &dsi_mode, NULL, &panel_dsi_mode);
	if (rc) {
		DSI_ERR("mode not found %d\n", rc);
		drm_mode_debug_printmodeline(drm_mode);
		return rc;
	}

	return panel_dsi_mode->priv_info->topology.num_lm;
}

int dsi_conn_get_mode_info(struct drm_connector *connector,
		const struct drm_display_mode *drm_mode,
		struct msm_sub_mode *sub_mode,
		struct msm_mode_info *mode_info,
		void *display, const struct msm_resource_caps_info *avail_res)
{
	struct dsi_display_mode partial_dsi_mode, *dsi_mode = NULL;
	struct dsi_mode_info *timing;
	int src_bpp, tar_bpp, rc = 0;
	struct dsi_display *dsi_display = (struct dsi_display *) display;

	if (!drm_mode || !mode_info)
		return -EINVAL;

	convert_to_dsi_mode(drm_mode, &partial_dsi_mode);
	rc = dsi_display_find_mode(dsi_display, &partial_dsi_mode, sub_mode, &dsi_mode);
	if (rc || !dsi_mode->priv_info)
		return -EINVAL;

	memset(mode_info, 0, sizeof(*mode_info));

	timing = &dsi_mode->timing;
	mode_info->frame_rate = dsi_mode->timing.refresh_rate;
	mode_info->vtotal = DSI_V_TOTAL(timing);
	mode_info->prefill_lines = dsi_mode->priv_info->panel_prefill_lines;
	mode_info->jitter_numer = dsi_mode->priv_info->panel_jitter_numer;
	mode_info->jitter_denom = dsi_mode->priv_info->panel_jitter_denom;
	mode_info->dfps_maxfps = dsi_drm_get_dfps_maxfps(display);
	mode_info->panel_mode_caps = dsi_mode->panel_mode_caps;
	mode_info->mdp_transfer_time_us =
		dsi_mode->priv_info->mdp_transfer_time_us;
	mode_info->disable_rsc_solver = dsi_mode->priv_info->disable_rsc_solver;
	mode_info->qsync_min_fps = dsi_mode->timing.qsync_min_fps;

	memcpy(&mode_info->topology, &dsi_mode->priv_info->topology,
			sizeof(struct msm_display_topology));

	if (dsi_mode->priv_info->bit_clk_list.count) {
		struct msm_dyn_clk_list *dyn_clk_list = &mode_info->dyn_clk_list;

		dyn_clk_list->rates = dsi_mode->priv_info->bit_clk_list.rates;
		dyn_clk_list->count = dsi_mode->priv_info->bit_clk_list.count;
		dyn_clk_list->type = dsi_display->panel->dyn_clk_caps.type;
		dyn_clk_list->front_porches = dsi_mode->priv_info->bit_clk_list.front_porches;
		dyn_clk_list->pixel_clks_khz = dsi_mode->priv_info->bit_clk_list.pixel_clks_khz;

		rc = dsi_display_restore_bit_clk(dsi_display, dsi_mode);
		if (rc) {
			DSI_ERR("[%s] bit clk rate cannot be restored\n", dsi_display->name);
			return rc;
		}
	}

	mode_info->clk_rate = dsi_mode->timing.clk_rate_hz;

	if (dsi_mode->priv_info->dsc_enabled) {
		mode_info->comp_info.comp_type = MSM_DISPLAY_COMPRESSION_DSC;
		mode_info->topology.comp_type = MSM_DISPLAY_COMPRESSION_DSC;
		memcpy(&mode_info->comp_info.dsc_info, &dsi_mode->priv_info->dsc,
			sizeof(dsi_mode->priv_info->dsc));
	} else if (dsi_mode->priv_info->vdc_enabled) {
		mode_info->comp_info.comp_type = MSM_DISPLAY_COMPRESSION_VDC;
		mode_info->topology.comp_type = MSM_DISPLAY_COMPRESSION_VDC;
		memcpy(&mode_info->comp_info.vdc_info, &dsi_mode->priv_info->vdc,
			sizeof(dsi_mode->priv_info->vdc));
	}

	if (mode_info->comp_info.comp_type) {
		tar_bpp = dsi_mode->priv_info->pclk_scale.numer;
		src_bpp = dsi_mode->priv_info->pclk_scale.denom;
		mode_info->comp_info.comp_ratio = mult_frac(1, src_bpp,
				tar_bpp);
		mode_info->wide_bus_en = dsi_mode->priv_info->widebus_support;
	}

	if (dsi_mode->priv_info->roi_caps.enabled) {
		memcpy(&mode_info->roi_caps, &dsi_mode->priv_info->roi_caps,
			sizeof(dsi_mode->priv_info->roi_caps));
	}

	mode_info->allowed_mode_switches =
		dsi_mode->priv_info->allowed_mode_switch;

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
};

int dsi_conn_set_avr_step_info(struct dsi_panel *panel, void *info)
{
	u32 i;
	int idx = 0;
	size_t buff_sz = PAGE_SIZE;
	char *buff;

	buff = kzalloc(buff_sz, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	for (i = 0; i < panel->avr_caps.avr_step_fps_list_len && (idx < (buff_sz - 1)); i++)
		idx += scnprintf(&buff[idx], buff_sz - idx, "%u@%u ",
				 panel->avr_caps.avr_step_fps_list[i],
				 panel->dfps_caps.dfps_list[i]);

	sde_kms_info_add_keystr(info, "avr step requirement", buff);
	kfree(buff);

	return 0;
}

int dsi_conn_get_qsync_min_fps(struct drm_connector_state *conn_state)
{
	struct sde_connector_state *sde_conn_state = to_sde_connector_state(conn_state);
	struct msm_display_mode *msm_mode;
	struct dsi_display_mode_priv_info *priv_info;

	if (!sde_conn_state)
		return -EINVAL;

	msm_mode = &sde_conn_state->msm_mode;
	if (!msm_mode || !msm_mode->private)
		return -EINVAL;

	priv_info = (struct dsi_display_mode_priv_info *)(msm_mode->private);
	return priv_info->qsync_min_fps;
}

int dsi_conn_set_info_blob(struct drm_connector *connector,
		void *info, void *display, struct msm_mode_info *mode_info)
{
	struct dsi_display *dsi_display = display;
	struct dsi_panel *panel;
	enum dsi_pixel_format fmt;
	u32 bpp;

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
		DSI_DEBUG("invalid display type:%d\n", dsi_display->type);
		break;
	}

	if (!dsi_display->panel) {
		DSI_DEBUG("invalid panel data\n");
		goto end;
	}

	panel = dsi_display->panel;
	sde_kms_info_add_keystr(info, "panel name", panel->name);

	switch (panel->panel_mode) {
	case DSI_OP_VIDEO_MODE:
		sde_kms_info_add_keystr(info, "panel mode", "video");
		if (panel->avr_caps.avr_step_fps_list_len)
			dsi_conn_set_avr_step_info(panel, info);
		break;
	case DSI_OP_CMD_MODE:
		sde_kms_info_add_keystr(info, "panel mode", "command");
		sde_kms_info_add_keyint(info, "mdp_transfer_time_us",
				mode_info->mdp_transfer_time_us);
		break;
	default:
		DSI_DEBUG("invalid panel type:%d\n", panel->panel_mode);
		break;
	}

	sde_kms_info_add_keystr(info, "qsync support",
		panel->qsync_caps.qsync_support ?
			"true" : "false");
	if (panel->qsync_caps.qsync_min_fps)
		sde_kms_info_add_keyint(info, "qsync_fps",
			panel->qsync_caps.qsync_min_fps);

	sde_kms_info_add_keystr(info, "dfps support",
			panel->dfps_caps.dfps_support ? "true" : "false");

	if (panel->dfps_caps.dfps_support) {
		sde_kms_info_add_keyint(info, "min_fps",
			panel->dfps_caps.min_refresh_rate);
		sde_kms_info_add_keyint(info, "max_fps",
			panel->dfps_caps.max_refresh_rate);
	}

	sde_kms_info_add_keystr(info, "dyn bitclk support",
			panel->dyn_clk_caps.dyn_clk_support ? "true" : "false");

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
		DSI_DEBUG("invalid panel rotation:%d\n",
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
		DSI_DEBUG("invalid panel backlight type:%d\n",
						panel->bl_config.type);
		break;
	}

	sde_kms_info_add_keyint(info, "max os brightness", panel->bl_config.brightness_max_level);
	sde_kms_info_add_keyint(info, "max panel backlight", panel->bl_config.bl_max_level);

	if (panel->spr_info.enable)
		sde_kms_info_add_keystr(info, "spr_pack_type",
			msm_spr_pack_type_str[panel->spr_info.pack_type]);

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

	fmt = dsi_display->config.common_config.dst_format;
	bpp = dsi_ctrl_pixel_format_to_bpp(fmt);

	sde_kms_info_add_keyint(info, "bit_depth", bpp);

end:
	return 0;
}

void dsi_conn_set_submode_blob_info(struct drm_connector *conn,
		void *info, void *display, struct drm_display_mode *drm_mode)
{
	struct dsi_display *dsi_display = display;
	struct dsi_display_mode partial_dsi_mode;
	int count, i;
	int preferred_submode_idx = -EINVAL;
	enum dsi_dyn_clk_feature_type dyn_clk_type;
	char *dyn_clk_types[DSI_DYN_CLK_TYPE_MAX] = {
		[DSI_DYN_CLK_TYPE_LEGACY] = "none",
		[DSI_DYN_CLK_TYPE_CONST_FPS_ADJUST_HFP] = "hfp",
		[DSI_DYN_CLK_TYPE_CONST_FPS_ADJUST_VFP] = "vfp",
	};

	if (!conn || !display || !drm_mode) {
		DSI_ERR("Invalid params\n");
		return;
	}

	convert_to_dsi_mode(drm_mode, &partial_dsi_mode);

	mutex_lock(&dsi_display->display_lock);
	count = dsi_display->panel->num_display_modes;
	for (i = 0; i < count; i++) {
		struct dsi_display_mode *dsi_mode = &dsi_display->modes[i];

		u32 panel_mode_caps = 0;
		const char *topo_name = NULL;

		if (!dsi_display_mode_match(&partial_dsi_mode, dsi_mode,
				DSI_MODE_MATCH_FULL_TIMINGS))
			continue;

		sde_kms_info_add_keyint(info, "submode_idx", i);

		if (dsi_mode->is_preferred)
			preferred_submode_idx = i;

		if (dsi_mode->panel_mode_caps & DSI_OP_CMD_MODE)
			panel_mode_caps |= DRM_MODE_FLAG_CMD_MODE_PANEL;
		if (dsi_mode->panel_mode_caps & DSI_OP_VIDEO_MODE)
			panel_mode_caps |= DRM_MODE_FLAG_VID_MODE_PANEL;

		sde_kms_info_add_keyint(info, "panel_mode_capabilities",
			panel_mode_caps);

		sde_kms_info_add_keyint(info, "dsc_mode",
			dsi_mode->priv_info->dsc_enabled ? MSM_DISPLAY_DSC_MODE_ENABLED :
				MSM_DISPLAY_DSC_MODE_DISABLED);
		topo_name = sde_conn_get_topology_name(conn,
			dsi_mode->priv_info->topology);
		if (topo_name)
			sde_kms_info_add_keystr(info, "topology", topo_name);

		if (!dsi_mode->priv_info->bit_clk_list.count)
			continue;

		dyn_clk_type = dsi_display->panel->dyn_clk_caps.type;
		sde_kms_info_add_list(info, "dyn_bitclk_list",
				dsi_mode->priv_info->bit_clk_list.rates,
				dsi_mode->priv_info->bit_clk_list.count);
		sde_kms_info_add_keystr(info, "dyn_fp_type",
				dyn_clk_types[dyn_clk_type]);
		sde_kms_info_add_list(info, "dyn_fp_list",
				dsi_mode->priv_info->bit_clk_list.front_porches,
				dsi_mode->priv_info->bit_clk_list.count);
		sde_kms_info_add_list(info, "dyn_pclk_list",
				dsi_mode->priv_info->bit_clk_list.pixel_clks_khz,
				dsi_mode->priv_info->bit_clk_list.count);
	}

	if (preferred_submode_idx >= 0)
		sde_kms_info_add_keyint(info, "preferred_submode_idx",
			preferred_submode_idx);

	mutex_unlock(&dsi_display->display_lock);
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
	rc = dsi_display_get_info(conn, &info, display);
	if (rc) {
		DSI_ERR("failed to get display info, rc=%d\n", rc);
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
	struct dsi_display *dsi_display;
	int count, i;

	if (!connector || !display)
		return;

	dsi_display = display;
	count = dsi_display->panel->num_display_modes;
	for (i = 0; i < count; i++) {
		struct dsi_display_mode *dsi_mode = &dsi_display->modes[i];

		dsi_display_put_mode(dsi_display, dsi_mode);
	}

	/* free the display structure modes also */
	kfree(dsi_display->modes);
	dsi_display->modes = NULL;
}


static int dsi_drm_update_edid_name(struct edid *edid, const char *name)
{
	u8 *dtd = (u8 *)&edid->detailed_timings[3];
	u8 standard_header[] = {0x00, 0x00, 0x00, 0xFE, 0x00};
	u32 dtd_size = 18;
	u32 header_size = sizeof(standard_header);

	if (!name)
		return -EINVAL;

	/* Fill standard header */
	memcpy(dtd, standard_header, header_size);

	dtd_size -= header_size;
	dtd_size = min_t(u32, dtd_size, strlen(name));

	memcpy(dtd + header_size, name, dtd_size);

	return 0;
}

static void dsi_drm_update_dtd(struct edid *edid,
		struct dsi_display_mode *modes, u32 modes_count)
{
	u32 i;
	u32 count = min_t(u32, modes_count, 3);

	for (i = 0; i < count; i++) {
		struct detailed_timing *dtd = &edid->detailed_timings[i];
		struct dsi_display_mode *mode = &modes[i];
		struct dsi_mode_info *timing = &mode->timing;
		struct detailed_pixel_timing *pd = &dtd->data.pixel_data;
		u32 h_blank = timing->h_front_porch + timing->h_sync_width +
				timing->h_back_porch;
		u32 v_blank = timing->v_front_porch + timing->v_sync_width +
				timing->v_back_porch;
		u32 h_img = 0, v_img = 0;

		dtd->pixel_clock = mode->pixel_clk_khz / 10;

		pd->hactive_lo = timing->h_active & 0xFF;
		pd->hblank_lo = h_blank & 0xFF;
		pd->hactive_hblank_hi = ((h_blank >> 8) & 0xF) |
				((timing->h_active >> 8) & 0xF) << 4;

		pd->vactive_lo = timing->v_active & 0xFF;
		pd->vblank_lo = v_blank & 0xFF;
		pd->vactive_vblank_hi = ((v_blank >> 8) & 0xF) |
				((timing->v_active >> 8) & 0xF) << 4;

		pd->hsync_offset_lo = timing->h_front_porch & 0xFF;
		pd->hsync_pulse_width_lo = timing->h_sync_width & 0xFF;
		pd->vsync_offset_pulse_width_lo =
			((timing->v_front_porch & 0xF) << 4) |
			(timing->v_sync_width & 0xF);

		pd->hsync_vsync_offset_pulse_width_hi =
			(((timing->h_front_porch >> 8) & 0x3) << 6) |
			(((timing->h_sync_width >> 8) & 0x3) << 4) |
			(((timing->v_front_porch >> 4) & 0x3) << 2) |
			(((timing->v_sync_width >> 4) & 0x3) << 0);

		pd->width_mm_lo = h_img & 0xFF;
		pd->height_mm_lo = v_img & 0xFF;
		pd->width_height_mm_hi = (((h_img >> 8) & 0xF) << 4) |
			((v_img >> 8) & 0xF);

		pd->hborder = 0;
		pd->vborder = 0;
		pd->misc = 0;
	}
}

static void dsi_drm_update_checksum(struct edid *edid)
{
	u8 *data = (u8 *)edid;
	u32 i, sum = 0;

	for (i = 0; i < EDID_LENGTH - 1; i++)
		sum += data[i];

	edid->checksum = 0x100 - (sum & 0xFF);
}

int dsi_connector_get_modes(struct drm_connector *connector, void *data,
		const struct msm_resource_caps_info *avail_res)
{
	int rc, i;
	u32 count = 0, edid_size;
	struct dsi_display_mode *modes = NULL;
	struct drm_display_mode drm_mode;
	struct dsi_display *display = data;
	struct edid edid;
	/* use u16 to handle panel AA size in 0.1*(mm) */
	u16 width_mm = connector->display_info.width_mm;
	u16 height_mm = connector->display_info.height_mm;
	const u8 edid_buf[EDID_LENGTH] = {
		0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x44, 0x6D,
		0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1B, 0x10, 0x01, 0x03,
		0x80, 0x00, 0x00, 0x78, 0x0A, 0x0D, 0xC9, 0xA0, 0x57, 0x47,
		0x98, 0x27, 0x12, 0x48, 0x4C, 0x00, 0x00, 0x00, 0x01, 0x01,
		0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
		0x01, 0x01, 0x01, 0x01,
	};

	edid_size = min_t(u32, sizeof(edid), EDID_LENGTH);

	memcpy(&edid, edid_buf, edid_size);

	rc = dsi_display_get_mode_count(display, &count);
	if (rc) {
		DSI_ERR("failed to get num of modes, rc=%d\n", rc);
		goto end;
	}

	rc = dsi_display_get_modes(display, &modes);
	if (rc) {
		DSI_ERR("failed to get modes, rc=%d\n", rc);
		count = 0;
		goto end;
	}

	for (i = 0; i < count; i++) {
		struct drm_display_mode *m;

		memset(&drm_mode, 0x0, sizeof(drm_mode));
		dsi_convert_to_drm_mode(&modes[i], &drm_mode);
		m = drm_mode_duplicate(connector->dev, &drm_mode);
		if (!m) {
			DSI_ERR("failed to add mode %ux%u\n",
			       drm_mode.hdisplay,
			       drm_mode.vdisplay);
			count = -ENOMEM;
			goto end;
		}
		m->width_mm = connector->display_info.width_mm;
		m->height_mm = connector->display_info.height_mm;

		if (display->cmdline_timing != NO_OVERRIDE) {
			/* get the preferred mode from dsi display mode */
			if (modes[i].is_preferred)
				m->type |= DRM_MODE_TYPE_PREFERRED;
		} else if (modes[i].mode_idx == 0) {
			/* set the first mode in device tree list as preferred */
			m->type |= DRM_MODE_TYPE_PREFERRED;
		}
		drm_mode_probed_add(connector, m);
	}

	rc = dsi_drm_update_edid_name(&edid, display->panel->name);
	if (rc) {
		count = 0;
		goto end;
	}

	edid.width_cm = (connector->display_info.width_mm) / 10;
	edid.height_cm = (connector->display_info.height_mm) / 10;

	dsi_drm_update_dtd(&edid, modes, count);
	dsi_drm_update_checksum(&edid);
	rc =  drm_connector_update_edid_property(connector, &edid);
	if (rc)
		count = 0;
	/*
	 * DRM EDID structure maintains panel physical dimensions in
	 * centimeters, we will be losing the precision anything below cm.
	 * Changing DRM framework will effect other clients at this
	 * moment, overriding the values back to millimeter.
	 */
	connector->display_info.width_mm = width_mm;
	connector->display_info.height_mm = height_mm;
end:
	DSI_DEBUG("MODE COUNT =%d\n\n", count);
	return count;
}

enum drm_mode_status dsi_conn_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode,
		void *display, const struct msm_resource_caps_info *avail_res)
{
	struct dsi_display_mode dsi_mode;
	struct dsi_display_mode *full_dsi_mode = NULL;
	struct sde_connector_state *conn_state;
	int rc;

	if (!connector || !mode) {
		DSI_ERR("Invalid params\n");
		return MODE_ERROR;
	}

	convert_to_dsi_mode(mode, &dsi_mode);

	conn_state = to_sde_connector_state(connector->state);
	if (conn_state)
		msm_parse_mode_priv_info(&conn_state->msm_mode, &dsi_mode);

	rc = dsi_display_find_mode(display, &dsi_mode, NULL, &full_dsi_mode);
	if (rc) {
		DSI_ERR("could not find mode %s\n", mode->name);
		return MODE_ERROR;
	}

	rc = dsi_display_validate_mode(display, full_dsi_mode,
			DSI_VALIDATE_FLAG_ALLOW_ADJUST);
	if (rc) {
		DSI_ERR("mode not supported, rc=%d\n", rc);
		return MODE_BAD;
	}

	return MODE_OK;
}

int dsi_conn_pre_kickoff(struct drm_connector *connector,
		void *display,
		struct msm_display_kickoff_params *params,
		bool force_update_dsi_clocks)
{
	if (!connector || !display || !params) {
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	return dsi_display_pre_kickoff(connector, display, params, force_update_dsi_clocks);
}

int dsi_conn_prepare_commit(void *display,
		struct msm_display_conn_params *params)
{
	if (!display || !params) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	return dsi_display_pre_commit(display, params);
}

void dsi_conn_enable_event(struct drm_connector *connector,
		uint32_t event_idx, bool enable, void *display)
{
	struct dsi_event_cb_info event_info;

	memset(&event_info, 0, sizeof(event_info));

	event_info.event_cb = sde_connector_trigger_event;
	event_info.event_usr_ptr = connector;

	dsi_display_enable_event(connector, display,
			event_idx, &event_info, enable);
}

int dsi_conn_post_kickoff(struct drm_connector *connector,
	struct msm_display_conn_params *params)
{
	struct drm_encoder *encoder;
	struct drm_bridge *bridge;
	struct dsi_bridge *c_bridge;
	struct dsi_display_mode adj_mode;
	struct dsi_display *display;
	struct dsi_display_ctrl *m_ctrl, *ctrl;
	int i, rc = 0, ctrl_version;
	bool enable;
	struct dsi_dyn_clk_caps *dyn_clk_caps;

	if (!connector || !connector->state) {
		DSI_ERR("invalid connector or connector state\n");
		return -EINVAL;
	}

	encoder = connector->state->best_encoder;
	if (!encoder) {
		DSI_DEBUG("best encoder is not available\n");
		return 0;
	}

	bridge = drm_bridge_chain_get_first_bridge(encoder);
	if (!bridge) {
		DSI_DEBUG("bridge is not available\n");
		return 0;
	}
	c_bridge = to_dsi_bridge(bridge);
	adj_mode = c_bridge->dsi_mode;
	display = c_bridge->display;
	dyn_clk_caps = &(display->panel->dyn_clk_caps);

	if (adj_mode.dsi_mode_flags & DSI_MODE_FLAG_VRR) {
		m_ctrl = &display->ctrl[display->clk_master_idx];
		ctrl_version = m_ctrl->ctrl->version;
		rc = dsi_ctrl_timing_db_update(m_ctrl->ctrl, false);
		if (rc) {
			DSI_ERR("[%s] failed to dfps update  rc=%d\n",
				display->name, rc);
			return -EINVAL;
		}

		/*
		 * When both DFPS and dynamic clock switch with constant
		 * fps features are enabled, wait for dynamic refresh done
		 * only in case of clock switch.
		 * In case where only fps changes, clock remains same.
		 * So, wait for dynamic refresh done is not required.
		 */
		if ((ctrl_version >= DSI_CTRL_VERSION_2_5) &&
			(dyn_clk_caps->maintain_const_fps) &&
			(adj_mode.dsi_mode_flags & DSI_MODE_FLAG_DYN_CLK)) {
			display_for_each_ctrl(i, display) {
				ctrl = &display->ctrl[i];
				rc = dsi_ctrl_wait4dynamic_refresh_done(
						ctrl->ctrl);
				if (rc)
					DSI_ERR("wait4dfps refresh failed\n");

				dsi_phy_dynamic_refresh_clear(ctrl->phy);
				dsi_clk_disable_unprepare(&display->clock_info.pll_clks);
			}
		}

		/* Update the rest of the controllers */
		display_for_each_ctrl(i, display) {
			ctrl = &display->ctrl[i];
			if (!ctrl->ctrl || (ctrl == m_ctrl))
				continue;

			rc = dsi_ctrl_timing_db_update(ctrl->ctrl, false);
			if (rc) {
				DSI_ERR("[%s] failed to dfps update rc=%d\n",
					display->name,  rc);
				return -EINVAL;
			}
		}

		c_bridge->dsi_mode.dsi_mode_flags &= ~DSI_MODE_FLAG_VRR;
	}

	/* ensure dynamic clk switch flag is reset */
	c_bridge->dsi_mode.dsi_mode_flags &= ~DSI_MODE_FLAG_DYN_CLK;

	if (params->qsync_update) {
		enable = (params->qsync_mode > 0) ? true : false;
		display_for_each_ctrl(i, display)
			dsi_ctrl_setup_avr(display->ctrl[i].ctrl, enable);
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

	rc = drm_bridge_attach(encoder, &bridge->base, NULL, 0);
	if (rc) {
		DSI_ERR("failed to attach bridge, rc=%d\n", rc);
		goto error_free_bridge;
	}

	return bridge;
error_free_bridge:
	kfree(bridge);
error:
	return ERR_PTR(rc);
}

void dsi_drm_bridge_cleanup(struct dsi_bridge *bridge)
{
	kfree(bridge);
}

static bool is_valid_poms_switch(struct dsi_display_mode *mode_a,
		struct dsi_display_mode *mode_b)
{
	/*
	 * POMS cannot happen in conjunction with any other type of mode set.
	 * Check to ensure FPS remains same between the modes and also
	 * resolution.
	 */
	return((mode_a->timing.refresh_rate == mode_b->timing.refresh_rate) &&
			(mode_a->timing.v_active == mode_b->timing.v_active) &&
			(mode_a->timing.h_active == mode_b->timing.h_active));
}

void dsi_conn_set_allowed_mode_switch(struct drm_connector *connector,
		void *display)
{
	u32 mode_idx = 0, cmp_mode_idx = 0;
	u32 common_mode_caps = 0;
	struct drm_display_mode *drm_mode, *cmp_drm_mode;
	struct dsi_display_mode dsi_mode, *panel_dsi_mode, *cmp_panel_dsi_mode;
	struct list_head *mode_list = &connector->modes;
	struct dsi_display *disp = display;
	struct dsi_panel *panel;
	int mode_count = 0, rc = 0;
	struct dsi_display_mode_priv_info *dsi_mode_info, *cmp_dsi_mode_info;
	bool allow_switch = false;

	if (!disp || !disp->panel) {
		DSI_ERR("invalid parameters");
		return;
	}

	panel = disp->panel;
	list_for_each_entry(drm_mode, &connector->modes, head)
		mode_count++;

	list_for_each_entry(drm_mode, &connector->modes, head) {

		convert_to_dsi_mode(drm_mode, &dsi_mode);

		rc = dsi_display_find_mode(display, &dsi_mode, NULL, &panel_dsi_mode);
		if (rc)
			return;

		dsi_mode_info =  panel_dsi_mode->priv_info;
		dsi_mode_info->allowed_mode_switch |= BIT(mode_idx);
		if (mode_idx == mode_count - 1)
			break;

		mode_list = mode_list->next;
		cmp_mode_idx = 1;
		list_for_each_entry(cmp_drm_mode, mode_list, head) {
			if (&cmp_drm_mode->head == &connector->modes)
				continue;
			convert_to_dsi_mode(cmp_drm_mode, &dsi_mode);

			rc = dsi_display_find_mode(display, &dsi_mode,
					NULL, &cmp_panel_dsi_mode);
			if (rc)
				return;

			cmp_dsi_mode_info = cmp_panel_dsi_mode->priv_info;
			allow_switch = false;
			common_mode_caps = (panel_dsi_mode->panel_mode_caps &
					cmp_panel_dsi_mode->panel_mode_caps);

			/*
			 * FPS switch among video modes, is only supported
			 * if DFPS or dynamic clocks are specified.
			 * Reject any mode switches between video mode timing
			 * nodes if support for those features is not present.
			 */
			if (common_mode_caps & DSI_OP_CMD_MODE) {
				allow_switch = true;
			} else if ((common_mode_caps & DSI_OP_VIDEO_MODE) &&
				(panel->dfps_caps.dfps_support ||
				panel->dyn_clk_caps.dyn_clk_support)) {
				allow_switch = true;
			} else {
				if (is_valid_poms_switch(panel_dsi_mode,
						cmp_panel_dsi_mode))
					allow_switch = true;
			}

			if (allow_switch) {
				dsi_mode_info->allowed_mode_switch |=
					BIT(mode_idx + cmp_mode_idx);
				cmp_dsi_mode_info->allowed_mode_switch |=
					BIT(mode_idx);
			}

			if ((mode_idx + cmp_mode_idx) >= mode_count - 1)
				break;

			cmp_mode_idx++;
		}
		mode_idx++;
	}
}

int dsi_conn_set_dyn_bit_clk(struct drm_connector *connector, uint64_t value)
{
	struct sde_connector *c_conn = NULL;
	struct dsi_display *display;

	if (!connector) {
		DSI_ERR("invalid connector\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);
	display = (struct dsi_display *) c_conn->display;

	display->dyn_bit_clk = value;
	display->dyn_bit_clk_pending = true;

	SDE_EVT32(display->dyn_bit_clk);
	DSI_DEBUG("update dynamic bit clock rate to %llu\n", display->dyn_bit_clk);

	return 0;
}
