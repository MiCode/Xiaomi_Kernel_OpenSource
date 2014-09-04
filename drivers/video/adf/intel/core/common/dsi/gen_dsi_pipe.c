/*
 * ann_dsi_pipe.c
 *
 *  Created on: May 23, 2014
 *      Author: root
 */

#include <drm/drm_mode.h>
#include <drm/i915_drm.h>

#include <core/intel_dc_config.h>
#include <core/vlv/vlv_dc_regs.h>
#include <core/common/dsi/dsi_pipe.h>
#include <core/common/dsi/dsi_config.h>
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
	int err = 0;

	if (!dsi_pipe) {
		pr_err("%s: invalid DSI interface", __func__);
		return -EINVAL;
	}

	panel = dsi_pipe->panel;
	if (!panel || !panel->ops || !panel->ops->set_brightness) {
		pr_err("%s: invalid panel\n", __func__);
		return -EINVAL;
	}

	config = &dsi_pipe->config;
	ctx = &config->ctx;

	mutex_lock(&config->ctx_lock);

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

	mutex_lock(&config->ctx_lock);
	*mode = &config->perferred_mode;
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

	if (!config)
		return -EINVAL;

	mutex_lock(&config->ctx_lock);

	switch (state) {
	case DRM_MODE_DPMS_ON:
		err = dsi_pipe->ops.power_on(dsi_pipe);
		break;
	case DRM_MODE_DPMS_OFF:
		err = dsi_pipe->ops.power_off(dsi_pipe);
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

	err = dsi_pipe->ops.mode_set(dsi_pipe, mode);
	if (err) {
		pr_err("%s: failed to set mode\n", __func__);
		goto out_err;
	}

out_err:
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

static void dsi_on_post(struct intel_pipe *pipe)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);

	if (dsi_pipe->ops.on_post)
		dsi_pipe->ops.on_post(dsi_pipe);
}

static void dsi_pre_post(struct intel_pipe *pipe)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);

	if (dsi_pipe->ops.pre_post)
		dsi_pipe->ops.pre_post(dsi_pipe);
}

static u32 dsi_get_supported_events(struct intel_pipe *pipe)
{
	return INTEL_PIPE_EVENT_VSYNC;
}

static int dsi_set_event(struct intel_pipe *pipe, u8 event, bool enabled)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);

	if (dsi_pipe->ops.set_event)
		return dsi_pipe->ops.set_event(dsi_pipe, event, enabled);
	else
		return -EOPNOTSUPP;
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
	u32 dc_events = REG_READ(VLV_IIR);
	u32 event_bit;

	*events = 0;

	pr_debug("%s: IIR = %#x\n", __func__, dc_events);

	switch (idx) {
	case 0:
		event_bit = I915_DISPLAY_PIPE_A_VBLANK_INTERRUPT;
		break;
	case 2:
		event_bit = I915_DISPLAY_PIPE_B_VBLANK_INTERRUPT;
		break;
	default:
		pr_err("%s: invalid pipe index %d\n", __func__, idx);
		return;
	}

	if (!(dc_events & event_bit))
		return;

	/* Clear the 1st level interrupt. */
	REG_WRITE(VLV_IIR, dc_events);

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

	/*init config*/
	err = dsi_config_init(&pipe->config, panel, idx);
	if (err)
		goto err;

	/*init dsi interface ops*/
	pipe->ops.power_on = intel_dsi_pre_enable;
	pipe->ops.pre_power_off = intel_dsi_pre_disable;
	pipe->ops.power_off = intel_dsi_post_disable;
	pipe->ops.mode_set = intel_dsi_modeset;
	pipe->ops.pre_post = intel_dsi_pre_post;
	pipe->ops.set_event = intel_dsi_set_events;
	pipe->ops.get_events = intel_dsi_get_events;
	pipe->ops.handle_events = intel_dsi_handle_events;

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

	return 0;
err:
	dsi_pipe_destroy(pipe);
	return err;
}
