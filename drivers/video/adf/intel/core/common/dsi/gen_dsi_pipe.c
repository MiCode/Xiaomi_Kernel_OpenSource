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

#include <core/intel_dc_config.h>
#include <core/vlv/vlv_dc_regs.h>
#include <core/vlv/vlv_dc_hw.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/common/dsi/dsi_pipe.h>
#include <core/common/dsi/dsi_config.h>
#include <core/common/intel_gen_backlight.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_dpst.h>
#include <intel_adf_device.h>
#include "dsi_vbt.h"
#include "intel_dsi.h"

static void dsi_pipe_suspend(struct intel_dc_component *component)
{
	struct intel_pipe *pipe = to_intel_pipe(component);
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);

	/*power gate the power rail directly*/
	dsi_pipe->ops.power_off(dsi_pipe);
}

static void dsi_pipe_resume(struct intel_dc_component *component)
{
	struct intel_pipe *pipe = to_intel_pipe(component);
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);

	dsi_pipe->ops.power_on(dsi_pipe);
}

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

	pr_err("ADF: %s: Preferred Mode = %dx%d @%d\n", __func__,
	       (*mode)->hdisplay, (*mode)->vdisplay, (*mode)->vrefresh);
}

static bool dsi_is_screen_connected(struct intel_pipe *pipe)
{
	return true;
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
		err = vlv_display_on(pipe);
		break;
	case DRM_MODE_DPMS_OFF:
		err = vlv_display_off(pipe);
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

	mutex_lock(&config->ctx_lock);
	vlv_display_off(pipe);
	vlv_display_on(pipe);
	mutex_unlock(&config->ctx_lock);

	return err;
}

static int dsi_get_screen_size(struct intel_pipe *pipe,
		u16 *width_mm, u16 *height_mm)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_panel *panel = dsi_pipe->panel;
	struct panel_info pi;
	int err;

	if (!panel || !panel->ops || !panel->ops->get_panel_info) {
		pr_err("%s: failed to get panel info\n", __func__);
		err = -ENODEV;
		goto out_err;
	}

	panel->ops->get_panel_info(&dsi_pipe->config, &pi);

	*width_mm = pi.width_mm;
	*height_mm = pi.height_mm;

	return 0;
out_err:
	return err;
}

static unsigned int usecs_to_scanlines(struct drm_display_mode *hwmode,
				       unsigned int usecs)
{
	/* paranoia */
	if (!hwmode->crtc_htotal)
		return 1;

	return DIV_ROUND_UP(usecs * hwmode->clock,
			    1000 * hwmode->crtc_htotal);
}

static void intel_pipe_vblank_evade(struct intel_pipe *intel_pipe)
{
	u32 val;
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(intel_pipe);
	struct drm_display_mode *hwmode = &dsi_pipe->config.vbt_mode;
	int pipe = intel_pipe->base.idx;
	/* FIXME needs to be calibrated sensibly */
	u32 min = hwmode->crtc_vdisplay - usecs_to_scanlines(hwmode, 50);
	u32 max = hwmode->crtc_vdisplay - 1;
	long timeout = msecs_to_jiffies(3);

	local_irq_disable();
	val = REG_READ(PIPEDSL(pipe));
	local_irq_enable();

	while (val >= min && val <= max && timeout > 0) {

		vlv_wait_for_vblank(pipe);
		local_irq_disable();
		val = REG_READ(PIPEDSL(pipe));
		local_irq_enable();
		intel_pipe->status.wait_vblank = false;
	}

	if (val >= min && val <= max)
		pr_warn("ADF: Page flipping close to vblank start (DSL=%u, VBL=%u)\n",
			 val, hwmode->crtc_vdisplay);
}

static void dsi_on_post(struct intel_pipe *pipe)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);

	/* Enable maxfifo if required */
	if (!pipe->status.maxfifo_enabled &&
				(vlv_num_planes_enabled(pipe) == 1)) {
		REG_WRITE(FW_BLC_SELF_VLV, FW_CSPWRDWNEN);
		pipe->status.maxfifo_enabled = true;
	}
	if (dsi_pipe->ops.on_post)
		dsi_pipe->ops.on_post(dsi_pipe);
}

static void dsi_pre_validate(struct intel_pipe *pipe,
		struct intel_adf_post_custom_data *custom)
{
	struct intel_adf_config *custom_config;
	u8 i = 0, planes_enabled = 0;

	for (i = 0; i < custom->n_configs; i++) {
		custom_config = &custom->configs[i];

		/* Get the number of planes enabled */
		if (custom_config->type == INTEL_ADF_CONFIG_PLANE)
			planes_enabled++;
	}

	/* If we are moving to multiple plane then disable maxfifo */
	if (planes_enabled > 1 && pipe->status.maxfifo_enabled) {
		REG_WRITE(FW_BLC_SELF_VLV, ~FW_CSPWRDWNEN);
		pipe->status.maxfifo_enabled = false;
		pipe->status.wait_vblank = true;
		pipe->status.vsync_counter =
				pipe->ops->get_vsync_counter(pipe, 0);
	}
}

static void dsi_pre_post(struct intel_pipe *pipe)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);

	if (pipe->status.wait_vblank && pipe->status.vsync_counter ==
			pipe->ops->get_vsync_counter(pipe, 0)) {
		vlv_wait_for_vblank(pipe->base.idx);
		pipe->status.wait_vblank = false;
	}
	if (dsi_pipe->ops.pre_post)
		dsi_pipe->ops.pre_post(dsi_pipe);
	intel_pipe_vblank_evade(pipe);
}

static u32 dsi_get_supported_events(struct intel_pipe *pipe)
{
	return INTEL_PIPE_EVENT_VSYNC;
}

int dsi_set_event(struct intel_pipe *pipe, u16 event, bool enabled)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);

	u32 pipestat, value = 0;
	u8 idx = pipe->base.idx;
	value = REG_READ(PIPESTAT(idx));

	if (enabled)
		pipestat = 0;
	else {
		if (event == INTEL_PIPE_EVENT_VSYNC) {
			pr_debug("ADF: %s: Not allowing VSYNC OFF\n", __func__);
			return 0;
		} else
			pipestat = 0xFFFFFFFF;
	}

	switch (event) {
	case INTEL_PIPE_EVENT_SPRITE2_FLIP:
		enabled ? (pipestat |= SPRITE2_FLIP_DONE_EN) :
			(pipestat &= ~SPRITE2_FLIP_DONE_EN);
		break;
	case INTEL_PIPE_EVENT_SPRITE1_FLIP:
		enabled ? (pipestat |= SPRITE1_FLIP_DONE_EN) :
			(pipestat &= ~SPRITE1_FLIP_DONE_EN);
		break;
	case INTEL_PIPE_EVENT_PRIMARY_FLIP:
		enabled ? (pipestat |= PLANE_FLIP_DONE_EN) :
			(pipestat &= ~PLANE_FLIP_DONE_EN);
		break;
	case INTEL_PIPE_EVENT_VSYNC:
		enabled ? (pipestat |= VSYNC_EN) : (pipestat &= ~VSYNC_EN);
		break;
	case INTEL_PIPE_EVENT_DPST:
		enabled ? (pipestat |= DPST_EVENT_EN) :
			(pipestat &= ~DPST_EVENT_EN);
		break;
	}

	if (enabled)
		/* Enable interrupts */
		REG_WRITE(PIPESTAT(idx), value | pipestat);
	else
		/* Disable interrupts */
		REG_WRITE(PIPESTAT(idx), value & pipestat);

	/* In case specififc interrupts for DSI like TE */
	if (dsi_pipe->ops.set_event)
		return dsi_pipe->ops.set_event(dsi_pipe, event, enabled);

	return 0;
}

/**
 * FIXME: hardware vsync counter failed to work on ANN. use static SW
 * counter for now.
 */
static u32 vsync_counter;

#define VSYNC_COUNT_MAX_MASK 0xffffff

static void dsi_get_events(struct intel_pipe *pipe, u32 *events)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	u8 idx = pipe->base.idx;

	u32 pipestat = 0, value = 0;
	pipestat = REG_READ(PIPESTAT(idx));

	*events = 0;

	pr_debug("%s: PIPESTAT = 0x%x\n", __func__, pipestat);

	/* FIFO under run */
	if (pipestat & FIFO_UNDERRUN_STAT) {
		*events |= INTEL_PIPE_EVENT_UNDERRUN;
		value |= FIFO_UNDERRUN_STAT;
	}

	/* Sprite B Flip done interrupt */
	if (pipestat & SPRITE2_FLIP_DONE_STAT) {
		*events |= INTEL_PIPE_EVENT_SPRITE2_FLIP;
		value |= SPRITE2_FLIP_DONE_STAT;
		/* program the pre-calculated ddl value */
		if (pipe->regs.sp2_ddl) {
			REG_WRITE_BITS(VLV_DDL(idx), pipe->regs.sp2_ddl,
					pipe->regs.sp2_ddl_mask);
			pipe->regs.sp2_ddl = 0;
		}
	}

	/* Sprite A Flip done interrupt */
	if (pipestat & SPRITE1_FLIP_DONE_STAT) {
		*events |= INTEL_PIPE_EVENT_SPRITE1_FLIP;
		value |= SPRITE2_FLIP_DONE_STAT;
		/* program the pre-calculated ddl value */
		if (pipe->regs.sp1_ddl) {
			REG_WRITE_BITS(VLV_DDL(idx), pipe->regs.sp1_ddl,
					pipe->regs.sp1_ddl_mask);
			pipe->regs.sp1_ddl = 0;
		}
	}

	/* Plane A Flip done interrupt */
	if (pipestat & PLANE_FLIP_DONE_STAT) {
		*events |= INTEL_PIPE_EVENT_PRIMARY_FLIP;
		value |= PLANE_FLIP_DONE_STAT;
		/* program the pre-calculated ddl value */
		if (pipe->regs.pri_ddl) {
			REG_WRITE_BITS(VLV_DDL(idx), pipe->regs.pri_ddl,
					pipe->regs.pri_ddl_mask);
			pipe->regs.pri_ddl = 0;
		}
	}

	/* Vsync interrupt */
	if (pipestat & VSYNC_STAT) {
		*events |= INTEL_PIPE_EVENT_VSYNC;
		value |= VSYNC_STAT;
	}

	/* DPST event */
	if (pipestat & DPST_EVENT_STAT) {
		*events |= INTEL_PIPE_EVENT_DPST;
		value |= DPST_EVENT_STAT;
	}

	/* Clear the 1st level interrupt. */
	REG_WRITE(PIPESTAT(idx), pipestat | value);

	if (dsi_pipe->ops.get_events)
		dsi_pipe->ops.get_events(dsi_pipe, events);

	/**
	 * FIXME: should use hardware vsync counter.
	 */
	if (*events & INTEL_PIPE_EVENT_VSYNC) {
		if (++vsync_counter > VSYNC_COUNT_MAX_MASK)
			vsync_counter = 0;
	}
}

u32 dsi_get_vsync_counter(struct intel_pipe *pipe, u32 interval)
{
	u32 count;
	u32 max_count_mask = VSYNC_COUNT_MAX_MASK;

	count = vsync_counter;
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

	if (dsi_pipe->ops.handle_events)
		dsi_pipe->ops.handle_events(dsi_pipe, events);
}

static struct intel_pipe_ops dsi_base_ops = {
	.base = {
		.suspend = dsi_pipe_suspend,
		.resume = dsi_pipe_resume,
	},
	.hw_init = dsi_pipe_hw_init,
	.hw_deinit = dsi_pipe_hw_deinit,
	.get_preferred_mode = dsi_get_preferred_mode,
	.is_screen_connected = dsi_is_screen_connected,
	.get_modelist = dsi_get_modelist,
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
	struct intel_plane *primary_plane, u8 idx)
{
	struct dsi_panel *panel;
	struct dsi_vbt *vbt;
	int err, i;

	pr_debug("ADF:%s:\n", __func__);

	if (!pipe || !primary_plane)
		return -EINVAL;

	memset(pipe, 0, sizeof(struct dsi_pipe));

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
			panel->ops->panel_power_on =
					intel_adf_dsi_soc_power_on;
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

	/*init dsi interface ops*/
	pipe->ops.power_on = intel_adf_dsi_pre_pll_enable;
	pipe->ops.pre_power_off = intel_adf_dsi_pre_disable;
	pipe->ops.power_off = intel_adf_dsi_post_disable;
	pipe->ops.mode_set = intel_adf_dsi_modeset;
	pipe->ops.pre_post = intel_adf_dsi_pre_post;
	pipe->ops.set_event = intel_adf_dsi_set_events;
	pipe->ops.get_events = intel_adf_dsi_get_events;
	pipe->ops.handle_events = intel_adf_dsi_handle_events;
	pipe->ops.get_hw_state = intel_adf_dsi_get_hw_state;
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
