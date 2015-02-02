/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  Created on: September 1, 2014
 *  Author: Shobhit Kumar <shobhit.kumar@intel.com>
 */

#include <drm/drm_mode.h>
#include <drm/i915_drm.h>
#include <drm/i915_adf.h>
#include <intel_adf_device.h>
#include <core/intel_dc_config.h>
#include <core/common/dsi/dsi_pipe.h>
#include <core/common/dsi/dsi_config.h>
#include <core/common/intel_gen_backlight.h>
/* FIXME: remove this once gpio calls are abstracted */
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_dc_regs.h>
#include <core/vlv/vlv_pm.h>
#include "dsi_vbt.h"
#include "intel_dsi.h"
#include "intel_dsi_cmd.h"

#define TURN_ON (1 << 1)

#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
static int dsi_set_brightness(struct intel_pipe *pipe, int level)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_panel *panel = NULL;
	struct dsi_config *config = NULL;
	struct dsi_context *ctx = NULL;
	struct dsi_vbt *vbt;
	int err = 0;

	vbt = dsi_pipe->config.dsi;

	if (!dsi_pipe) {
		pr_err("%s: invalid DSI interface", __func__);
		return -EINVAL;
	}

	config = &dsi_pipe->config;
	ctx = &config->ctx;

	mutex_lock(&config->ctx_lock);
	level = (level * 0xFF / BRIGHTNESS_MAX_LEVEL);

	if (pipe->dpst_enabled)
		/*
		 * FIXME: enable once dpst is enabled
		 * vlv_dpst_set_brightness(pipe, level);
		 * level = dsi_pipe->pipeline->ops.set_brightness(
		 *	dsi_pipe->pipeline, brightness_val);
		 * panel->ops->set_brightness(dsi_pipe, level);
		 */
		 ;
	else {
		if (dsi_pipe->ops.set_brightness)
			dsi_pipe->ops.set_brightness(level);
	}

	panel = dsi_pipe->panel;
	if (!panel || !panel->ops || !panel->ops->set_brightness) {
		pr_err("%s: invalid panel\n", __func__);
		mutex_unlock(&config->ctx_lock);
		return -EINVAL;
	}

	ctx->backlight_level = level;
	err = panel->ops->set_brightness(dsi_pipe, level);

	mutex_unlock(&config->ctx_lock);

	return err;
}
#endif

static int dsi_pipe_hw_init(struct intel_pipe *pipe)
{
	return 0;
}

static void dsi_pipe_hw_deinit(struct intel_pipe *pipe)
{

}

static void dsi_get_modelist(struct intel_pipe *pipe,
		struct drm_mode_modeinfo **modelist, size_t *n_modes)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_config *config = &dsi_pipe->config;

	mutex_lock(&config->ctx_lock);
	*modelist = &config->perferred_mode;
	*n_modes = 1;
	mutex_unlock(&config->ctx_lock);
}

static void dsi_get_current_mode(struct intel_pipe *pipe,
		struct drm_mode_modeinfo *mode)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_config *config = &dsi_pipe->config;
	bool lock;

	if (!mode)
		return;

	lock = mutex_trylock(&config->ctx_lock);
	dsi_pipe->panel->ops->get_config_mode(&dsi_pipe->config,
				mode);
	if (lock)
		mutex_unlock(&config->ctx_lock);
}
static void dsi_get_preferred_mode(struct intel_pipe *pipe,
		struct drm_mode_modeinfo **mode)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_config *config = &dsi_pipe->config;
	bool lock;

	lock = mutex_trylock(&config->ctx_lock);
	*mode = &config->perferred_mode;

	if (lock)
		mutex_unlock(&config->ctx_lock);

	pr_debug("ADF: %s: Preferred Mode = %dx%d @%d\n", __func__,
		(*mode)->hdisplay, (*mode)->vdisplay, (*mode)->vrefresh);
}

static bool dsi_is_screen_connected(struct intel_pipe *pipe)
{
	return true;
}

static int dsi_display_on(struct intel_pipe *pipe,
		struct drm_mode_modeinfo *mode)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;
	struct dsi_panel *panel = dsi_pipe->panel;
	struct dsi_context *intel_dsi = &dsi_pipe->config.ctx;
	int err = 0;
	bool need_free_mem = false;

	/*
	 * program rcomp for compliance, reduce from 50 ohms to 45 ohms
	 * needed everytime after power gate
	 */
	vlv_flisdsi_write(0x04, 0x0004);

	/* bandgap reset is needed after everytime we do power gate */
	band_gap_reset(dsi_pipe);

	/* Panel Enable */
	if (panel->ops->panel_power_on)
		panel->ops->panel_power_on(dsi_pipe);

	msleep(intel_dsi->panel_on_delay);

	if (panel->ops->reset)
		panel->ops->reset(dsi_pipe);

	/* get the configured mode */
	if (mode == NULL) {
		need_free_mem = true;
		mode = kzalloc(sizeof(struct drm_mode_modeinfo), GFP_KERNEL);
		if (mode == NULL) {
			pr_err("%s: mem alloc for mode failed\n", __func__);
			err = -ENOMEM;
			goto out;
		}
		dsi_pipe->panel->ops->get_config_mode(&dsi_pipe->config, mode);
	}

	/* FIXME: compare if mode passed is same as current mode */

	/*
	 * DSI is a special beast that requires 3 calls to pipeline
	 * 1) setup pll : dsi_prepare_on
	 * 2) setup port: dsi_pre_display_on
	 * 3) enable port, pipe, plane etc : display_on
	 * this is because of the panel calls needed to be performed
	 * between these operations and hence we return to common code
	 * to make these calls.
	 */

	/* FIXME: plan to remove config as very few params inside it are used */
	pipeline->params.dsi.dsi_config = &dsi_pipe->config;

	err = vlv_dsi_prepare_on(pipeline, mode);
	if (err != 0) {
		pr_err("%s:dsi prepare pipeline on failed !!!\n", __func__);
		/* recovery ? */
		goto out;
	}

	if (panel->ops->drv_ic_init)
		panel->ops->drv_ic_init(dsi_pipe);

	err = vlv_dsi_pre_pipeline_on(pipeline, mode);
	if (err != 0) {
		pr_err("%s: dsi pre pipeline on failed !!!\n", __func__);
		/* recovery ? */
		goto out;
	}

	if (vlv_is_vid_mode(pipeline)) {
		msleep(20); /* XXX */
		adf_dpi_send_cmd(dsi_pipe, TURN_ON, DPI_LP_MODE_EN);
		msleep(100);

		if (panel->ops->power_on)
			panel->ops->power_on(dsi_pipe);
	}

	err = vlv_pipeline_on(pipeline, mode);
	if (err != 0) {
		pr_err("%s: dsi pipeline on failed !!!\n", __func__);
		/* recovery ? */
		goto out;
	}

	if (intel_dsi->backlight_on_delay >= 20)
		msleep(intel_dsi->backlight_on_delay);
	else
		usleep_range(intel_dsi->backlight_on_delay * 1000,
				(intel_dsi->backlight_on_delay * 1000) + 500);

	intel_enable_backlight(&dsi_pipe->base);

	/* enable vsyncs */
	pipe->ops->set_event(pipe, INTEL_PIPE_EVENT_VSYNC, true);

	dsi_pipe->dpms_state = DRM_MODE_DPMS_ON;

out:
	if (need_free_mem)
		kfree(mode);

	return err;
}

static int dsi_display_off(struct intel_pipe *pipe)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;
	struct dsi_context *intel_dsi = &dsi_pipe->config.ctx;
	struct dsi_panel *panel = dsi_pipe->panel;
	int err = 0;

	/* check harwdare state before disabling */
	if (!vlv_can_be_disabled(pipeline)) {
		pr_err("%s: DSI device already disabled\n", __func__);
		goto out;
	}
	intel_disable_backlight(&dsi_pipe->base);

	if (intel_dsi->backlight_off_delay >= 20)
		msleep(intel_dsi->backlight_off_delay);
	else
		usleep_range(intel_dsi->backlight_off_delay * 1000,
				(intel_dsi->backlight_off_delay * 1000) + 500);

	/* disable vsyncs */
	pipe->ops->set_event(pipe, INTEL_PIPE_EVENT_VSYNC, false);

	err = vlv_pipeline_off(pipeline);

	if (err != 0) {
		pr_err("%s: DSI pipeline off failed\n", __func__);
		/* FIXME: error recovery ??? */
		goto out;
	}

	/*
	 * if disable packets are sent before sending shutdown packet then in
	 * some next enable sequence send turn on packet error is observed
	 */
	if (panel->ops->power_off)
		panel->ops->power_off(dsi_pipe);

	vlv_post_pipeline_off(pipeline);

	if (panel->ops->disable_panel_power)
		panel->ops->disable_panel_power(dsi_pipe);

	/* Disable Panel */
	if (panel->ops->panel_power_off)
		panel->ops->panel_power_off(dsi_pipe);

	msleep(intel_dsi->panel_off_delay);
	msleep(intel_dsi->panel_pwr_cycle_delay);

	dsi_pipe->dpms_state = DRM_MODE_DPMS_OFF;

out:
	return err;
}

static int dsi_dpms(struct intel_pipe *pipe, u8 state)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_config *config = &dsi_pipe->config;
	int err = 0;

	pr_debug("ADF: %s current_state = %d, requested_state = %d\n",
			__func__, dsi_pipe->dpms_state, state);

	if (!config) {
		pr_err("ADF: %s config not set!!\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&config->ctx_lock);

	if (dsi_pipe->dpms_state == state) {
		pr_err("ADF: %s: Current DPMS State same as requested = %s\n",
				__func__, state ? "DPMS_OFF" : "DPMS_ON");

		mutex_unlock(&config->ctx_lock);
		return 0;
	}

	switch (state) {
	case DRM_MODE_DPMS_ON:
		intel_adf_display_rpm_get();
		err = dsi_display_on(pipe, NULL);
		break;
	case DRM_MODE_DPMS_OFF:
		err = dsi_display_off(pipe);
		intel_adf_display_rpm_put();
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	default:
		mutex_unlock(&config->ctx_lock);
		pr_err("%s: unsupported dpms mode\n", __func__);
		return -EOPNOTSUPP;
	}

	mutex_unlock(&config->ctx_lock);
	return err;
}

static int dsi_modeset(struct intel_pipe *pipe,
		struct drm_mode_modeinfo *mode)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_config *config = &dsi_pipe->config;
	int err = 0;

	pr_debug("ADF: %s\n", __func__);

	if (!mode) {
		pr_err("%s: invalid mode\n", __func__);
		err = -EINVAL;
		return err;
	}

	if (!config) {
		pr_err("%s: invalid DSI config\n", __func__);
		err = -EINVAL;
		return err;
	}

	/* Avoid duplicate modesets */
	if (dsi_pipe->dpms_state == DRM_MODE_DPMS_ON) {
		pr_info("ADF: %s: DSI already enabled\n", __func__);
		return err;
	}

	mutex_lock(&config->ctx_lock);

	/* Avoiding i915 enter into DPMS */
	intel_adf_display_rpm_get();
	dsi_display_off(pipe);
	dsi_display_on(pipe, mode);
	mutex_unlock(&config->ctx_lock);

	return err;
}

static int dsi_get_screen_size(struct intel_pipe *pipe,
		u16 *width_mm, u16 *height_mm)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_panel *panel = dsi_pipe->panel;
	struct panel_info pi;
	int err = 0;

	if (!panel || !panel->ops || !panel->ops->get_panel_info) {
		pr_err("%s: failed to get panel info\n", __func__);
		err = -ENODEV;
		goto out_err;
	}

	panel->ops->get_panel_info(&dsi_pipe->config, &pi);

	*width_mm = pi.width_mm;
	*height_mm = pi.height_mm;

out_err:
	return err;
}


static void dsi_on_post(struct intel_pipe *pipe)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;
	struct vlv_pipeline *vlv_pipeline = to_vlv_pipeline(pipeline);
	struct intel_dc_config *intel_config = &vlv_pipeline->config->base;
	struct dsi_context *dsi_ctx = &dsi_pipe->config.ctx;
	struct vlv_pri_plane *vlv_p_plane = &vlv_pipeline->pplane;
	struct vlv_dsi_port *dsi_port = NULL;
	enum port port;
	u32 temp;

	if ((vlv_p_plane->ctx.pri_plane_bpp == 24) && ((dsi_ctx->pixel_format ==
			VID_MODE_FORMAT_RGB666) || (dsi_ctx->pixel_format ==
			VID_MODE_FORMAT_RGB666_LOOSE)))
		dsi_pipe->dither_enable = true;

	if (dsi_pipe->dither_enable) {
		for_each_dsi_port(port, dsi_ctx->ports) {
			dsi_port = &vlv_pipeline->port.dsi_port[port];
			temp = REG_READ(dsi_port->offset);
			temp |= DITHERING_ENABLE;
			REG_WRITE(dsi_port->offset, temp);
			REG_POSTING_READ(dsi_port->offset);
		}
	}

	if (dsi_pipe->ops.on_post)
		dsi_pipe->ops.on_post(dsi_pipe);

	vlv_pm_on_post(intel_config, pipe);
}

static void dsi_pre_validate(struct intel_pipe *pipe,
		struct intel_adf_post_custom_data *custom)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;
	struct vlv_pipeline *vlv_pipeline = to_vlv_pipeline(pipeline);
	struct intel_dc_config *intel_config = &vlv_pipeline->config->base;

	vlv_pm_pre_validate(intel_config, custom, pipeline, pipe);
}

static void dsi_pre_post(struct intel_pipe *pipe)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;
	struct vlv_pipeline *vlv_pipeline = to_vlv_pipeline(pipeline);
	struct intel_dc_config *intel_config = &vlv_pipeline->config->base;

	vlv_pm_pre_post(intel_config, pipeline, pipe);
}

static u32 dsi_get_supported_events(struct intel_pipe *pipe)
{
	return INTEL_PIPE_EVENT_VSYNC;
}

int dsi_set_event(struct intel_pipe *pipe, u16 event, bool enabled)
{
	int ret;
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);

	pr_debug("ADF: %s\n", __func__);

	/* HW events */
	ret = intel_adf_set_event(pipe, event, enabled);
	if (ret) {
		pr_err("ADF: %s: Failed to set events\n", __func__);
		return ret;
	}

	/* Encoder events */
	if (dsi_pipe->ops.set_event) {
		ret = dsi_pipe->ops.set_event(dsi_pipe, event, enabled);
		if (ret)
			pr_err("ADF: %s: Failed to set DSI events\n", __func__);
	}
	return ret;
}

static void dsi_get_events(struct intel_pipe *pipe, u32 *events)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);

	pr_debug("ADF: %s\n", __func__);

	/* HW events */
	if (intel_adf_get_events(pipe, events)) {
		pr_err("ADF: %s: Failed to get events\n", __func__);
		return;
	}

	/* Encoder events */
	if (dsi_pipe->ops.get_events)
		dsi_pipe->ops.get_events(dsi_pipe, events);
}

u32 dsi_get_vsync_counter(struct intel_pipe *pipe, u32 interval)
{
	u32 count;
	u32 max_count_mask = VSYNC_COUNT_MAX_MASK;

	count = pipe->vsync_counter;
	count |= (~max_count_mask);
	count += interval;
	count &= max_count_mask;
	pr_debug("%s: count = %#x\n", __func__, count);

	return count;
}

/* Handle more device custom events. */
static void dsi_handle_events(struct intel_pipe *pipe, u32 events)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);

	pr_debug("ADF: %s\n", __func__);

	/* HW events */
	if (intel_adf_handle_events(pipe, events)) {
		pr_err("ADF: %s: failed to handle events\n", __func__);
		return;
	}

	/* Encoder specific events */
	if (dsi_pipe->ops.handle_events)
		dsi_pipe->ops.handle_events(dsi_pipe, events);
}

static long dsi_pipe_dpst_context(struct intel_pipe *pipe, unsigned long arg)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct intel_pipeline *pipeline = dsi_pipe->pipeline;
	long val = vlv_dpst_context(pipeline, arg);

	return val;
}

static long dsi_pipe_dpst_irq_handler(struct intel_pipe *pipe)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;

	return vlv_dpst_irq_handler(pipeline);

}

static struct intel_pipe_ops dsi_base_ops = {
	.hw_init = dsi_pipe_hw_init,
	.hw_deinit = dsi_pipe_hw_deinit,
	.get_preferred_mode = dsi_get_preferred_mode,
	.is_screen_connected = dsi_is_screen_connected,
	.get_modelist = dsi_get_modelist,
	.get_current_mode = dsi_get_current_mode,
	.dpms = dsi_dpms,
	.modeset = dsi_modeset,
	.get_screen_size = dsi_get_screen_size,
	.pre_validate = dsi_pre_validate,
	.pre_post = dsi_pre_post,
	.on_post = dsi_on_post,
	.get_supported_events = dsi_get_supported_events,
	.set_event = dsi_set_event,
	.get_events = dsi_get_events,
	.get_vsync_counter = dsi_get_vsync_counter,
	.handle_events = dsi_handle_events,
	.dpst_context = dsi_pipe_dpst_context,
	.dpst_irq_handler = dsi_pipe_dpst_irq_handler,
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	.set_brightness = dsi_set_brightness,
#endif
};

void dsi_pipe_destroy(struct dsi_pipe *pipe)
{
	if (pipe)
		dsi_config_destroy(&pipe->config);
}

int dsi_pipe_init(struct dsi_pipe *pipe, struct device *dev,
	struct intel_plane *primary_plane, u8 idx,
	struct intel_pipeline *pipeline, int port)
{
	struct dsi_panel *panel;
	struct dsi_vbt *vbt;
	int err, i;

	pr_debug("ADF:%s:\n", __func__);

	if (!pipe || !primary_plane)
		return -EINVAL;

	memset(pipe, 0, sizeof(struct dsi_pipe));

	pipe->config.ctx.ports = 1 << port;
	pipe->base.pipeline = pipeline;

	/*get panel*/
	panel = get_dsi_panel_by_id(MIPI_DSI_GENERIC_PANEL_ID);
	if (!panel)
		return -ENODEV;
	/*
	 * For GEN DSI implementation of generic driver, we need this call
	 * before any other panel ops
	 */
	if (!panel->ops || !panel->ops->dsi_controller_init) {
		pr_err("%s: panel doesn't have initialization params\n",
		       __func__);
		err = -ENODEV;
		goto err;
	}

	err = panel->ops->dsi_controller_init(pipe);
	if (err) {
		pr_err("%s: panel doesn't have initialization params\n",
		       __func__);
		err = -ENODEV;
		goto err;
	}

	vbt = pipe->config.dsi;
	if (vbt->seq_version < 3) {
		/*
		 * We have device with older version of VBT which
		 * needs static panel and backlight enabling routines
		 *
		 * Overwrite the panel ops function with static
		 * functions
		 */
		if (vbt->config->pwm_blc) {
			/* using SOC PWM */
			panel->ops->panel_power_on = intel_adf_dsi_soc_power_on;
			panel->ops->panel_power_off =
					intel_adf_dsi_soc_power_off;
			panel->ops->enable_backlight =
					intel_adf_dsi_soc_backlight_on;
			panel->ops->disable_backlight =
					intel_adf_dsi_soc_backlight_off;
		} else {
			/* Using PMIC */
			panel->ops->panel_power_on =
					intel_adf_dsi_pmic_power_on;
			panel->ops->panel_power_off =
					intel_adf_dsi_pmic_power_off;
			panel->ops->enable_backlight =
					intel_adf_dsi_pmic_backlight_on;
			panel->ops->disable_backlight =
					intel_adf_dsi_pmic_backlight_off;
		}
	}

	/*init config*/
	err = dsi_config_init(&pipe->config, panel, idx);
	if (err)
		goto err;

	pipe->dpms_state = DRM_MODE_DPMS_OFF;
	pipe->panel = panel;

	err = intel_pipe_init(&pipe->base, dev, idx, true, INTEL_PIPE_DSI,
		primary_plane, &dsi_base_ops, "dsi_pipe");
	if (err)
		goto err;

	/* initialize the LUT */
	for (i = 0; i < 256; i++) {
		pipe->config.lut_r[i] = i;
		pipe->config.lut_g[i] = i;
		pipe->config.lut_b[i] = i;
	}

	pipe->config.pixel_multiplier = 1;

	/* initialize the backlight ops */
	intel_backlight_init(&pipe->base);

	return 0;
err:
	dsi_pipe_destroy(pipe);
	return err;
}
