/*
 * Copyright ©  2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * Author: Shashank Sharma <shashank.sharma@intel.com>
 * Author: Akashdeep Sharma <akashdeep.sharma@intel.com>
 */
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <drm/drm_crtc.h>
#include <drm/drm_modes.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/chv_dc_regs.h>
#include <core/vlv/vlv_pm.h>
#include <core/common/hdmi/gen_hdmi_pipe.h>

/* Encoder options */
int hdmi_hw_init(struct intel_pipe *pipe)
{
	return 0;
}

void hdmi_hw_deinit(struct intel_pipe *pipe)
{

}

void hdmi_handle_one_time_events(struct intel_pipe *pipe, u32 events)
{
	return;
}

long hdmi_dpst_context(struct intel_pipe *pipe, unsigned long arg)
{
	return 0;
}

/* Utility */

/* Returning array of modes, caller must free the ptr */
static void hdmi_get_modelist(struct intel_pipe *pipe,
		struct drm_mode_modeinfo **modelist, size_t *n_modes)
{
	int count = 0;
	struct drm_mode_modeinfo *probed_modes = NULL;
	struct hdmi_mode_info *t, *mode;
	struct hdmi_pipe *hdmi_pipe = hdmi_pipe_from_intel_pipe(pipe);
	struct hdmi_monitor *monitor = hdmi_pipe->config.ctx.monitor;

	if (!modelist) {
		pr_err("ADF: HDMI: %s NULL input\n", __func__);
		return;
	}

	/* No monitor connected */
	if (!monitor || !monitor->no_probed_modes) {
		pr_err("ADF: HDMI: %s No connected monitor\n", __func__);
		return;
	}

	probed_modes = kzalloc(monitor->no_probed_modes *
		sizeof(struct drm_mode_modeinfo), GFP_KERNEL);
	if (!probed_modes) {
		pr_err("ADF: HDMI: %s OOM\n", __func__);
		return;
	}

	list_for_each_entry_safe(mode, t, &monitor->probed_modes, head) {
		memcpy(&probed_modes[count], &mode->drm_mode,
					sizeof(struct drm_mode_modeinfo));
		if (++count == monitor->no_probed_modes)
			break;
	}

	/* Update list */
	*modelist = probed_modes;
	*n_modes = monitor->no_probed_modes;
	pr_info("ADF: HDMI: %s done, no_modes=%d\n", __func__, (int)*n_modes);
}

int hdmi_get_screen_size(struct intel_pipe *pipe,
		u16 *width_mm, u16 *height_mm)
{
	struct hdmi_pipe *hdmi_pipe = hdmi_pipe_from_intel_pipe(pipe);
	struct hdmi_monitor *monitor = hdmi_pipe->config.ctx.monitor;

	if (!monitor) {
		pr_err("ADF: HDMI: %s Monitor not present\n", __func__);
		*width_mm = 0;
		*height_mm = 0;
	} else {
		*width_mm = monitor->screen_width_mm;
		*height_mm = monitor->screen_height_mm;
	}
	pr_info("ADF: HDMI: %s Monitor hXw=%dX%d\n",
		__func__, *width_mm, *height_mm);
	return 0;
}

bool hdmi_is_screen_connected(struct intel_pipe *pipe)
{
	struct hdmi_pipe *hdmi_pipe = hdmi_pipe_from_intel_pipe(pipe);
	struct hdmi_context *hdmi_ctx = &hdmi_pipe->config.ctx;
	struct hdmi_monitor *monitor = hdmi_ctx->monitor;

	pr_info("ADF: HDMI: %s\n", __func__);
	if (!monitor)
		return false;
	else
		return atomic_read(&hdmi_ctx->connected);
}

void hdmi_get_current_mode(struct intel_pipe *pipe,
		struct drm_mode_modeinfo *mode)
{
	struct hdmi_pipe *hdmi_pipe = hdmi_pipe_from_intel_pipe(pipe);
	struct drm_mode_modeinfo *curr_mode =
				hdmi_pipe->config.ctx.current_mode;

	if (!mode) {
		pr_err("ADF: HDMI: %s NULL input\n", __func__);
		return;
	}

	if (curr_mode) {
		memcpy(mode, curr_mode, sizeof(*mode));
		pr_debug("ADF: HDMI: %s curr mode %s\n",
			__func__, curr_mode->name);
	} else {
		memset(mode, 0, sizeof(*mode));
		pr_info("ADF: HDMI: %s No curr mode\n", __func__);
	}
}

static void hdmi_get_preferred_mode(struct intel_pipe *pipe,
		struct drm_mode_modeinfo **mode)
{
	struct hdmi_pipe *hdmi_pipe = hdmi_pipe_from_intel_pipe(pipe);
	struct hdmi_monitor *monitor = hdmi_pipe->config.ctx.monitor;

	pr_info("ADF: HDMI: %s\n", __func__);

	if (!mode) {
		pr_err("ADF: HDMI: %s NULL input\n", __func__);
		return;
	}

	if (monitor)
		*mode = monitor->preferred_mode;
	else
		*mode = NULL;
}

u32 hdmi_get_vsync_counter(struct intel_pipe *pipe, u32 interval)
{
	int count = 0;

	count = pipe->vsync_counter;
	count |= ~VSYNC_COUNT_MAX_MASK;
	count += interval;
	count &= VSYNC_COUNT_MAX_MASK;

	pr_debug("ADF: HDMI: vsync count = %#x\n", count);
	return count;
}

void hdmi_pre_validate(struct intel_pipe *pipe,
			struct intel_adf_post_custom_data *custom)
{
	struct hdmi_pipe *hdmi_pipe = hdmi_pipe_from_intel_pipe(pipe);
	struct intel_pipeline *pipeline = hdmi_pipe->base.pipeline;
	struct vlv_pipeline *vlv_pipeline = to_vlv_pipeline(pipeline);
	struct intel_dc_config *intel_config = &vlv_pipeline->config->base;

	pr_debug("ADF: HDMI: %s\n", __func__);
	vlv_pm_pre_validate(intel_config, custom, pipeline, pipe);
}

void hdmi_pre_post(struct intel_pipe *pipe)
{
	struct hdmi_pipe *hdmi_pipe = hdmi_pipe_from_intel_pipe(pipe);
	struct intel_pipeline *pipeline = hdmi_pipe->base.pipeline;
	struct vlv_pipeline *vlv_pipeline = to_vlv_pipeline(pipeline);
	struct intel_dc_config *intel_config = &vlv_pipeline->config->base;

	pr_debug("ADF: HDMI: %s\n", __func__);
	vlv_pm_pre_post(intel_config, pipeline, pipe);
}

void hdmi_on_post(struct intel_pipe *pipe)
{
	struct hdmi_pipe *hdmi_pipe = hdmi_pipe_from_intel_pipe(pipe);
	struct intel_pipeline *pipeline = hdmi_pipe->base.pipeline;
	struct vlv_pipeline *vlv_pipeline = to_vlv_pipeline(pipeline);
	struct intel_dc_config *intel_config = &vlv_pipeline->config->base;

	pr_debug("ADF: HDMI: %s\n", __func__);
	if (hdmi_pipe->ops.on_post)
		hdmi_pipe->ops.on_post(hdmi_pipe);

	vlv_pm_on_post(intel_config, pipe);
}

/* Core modeset */
static int hdmi_prepare(struct intel_pipe *pipe,
		struct drm_mode_modeinfo *mode)
{
	int err = 0;
	u32 val = 0;
	struct hdmi_pipe *hdmi_pipe = hdmi_pipe_from_intel_pipe(pipe);
	struct intel_pipeline *pipeline = hdmi_pipe->base.pipeline;
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_hdmi_port *hdmi_port;

	pr_info("ADF:HDMI: %s\n", __func__);

	if (!pipeline) {
		pr_err("ADF:HDMI: %s: Pipeline not set\n", __func__);
		err = -EINVAL;
		return err;
	}

	hdmi_port = &disp->port.hdmi_port;
	if (!hdmi_port) {
		pr_err("ADF:HDMI: %s: Port not set\n", __func__);
		err = -EINVAL;
		return err;
	}

	val = SDVO_ENCODING_HDMI;
	if (mode->flags & DRM_MODE_FLAG_PVSYNC)
		val |= SDVO_VSYNC_ACTIVE_HIGH;
	if (mode->flags & DRM_MODE_FLAG_PHSYNC)
		val |= SDVO_HSYNC_ACTIVE_HIGH;

	val |= SDVO_COLOR_FORMAT_8bpc;
	val |= HDMI_MODE_SELECT_HDMI;

	val |= SDVO_AUDIO_ENABLE;
	val |= SDVO_PIPE_SEL_CHV(pipe->base.idx);

	err = vlv_hdmi_port_prepare(hdmi_port, val);
	if (err)
		pr_err("ADF: HDMI: %s: prepare port failed\n", __func__);

	return err;
}

static int hdmi_modeset(struct intel_pipe *pipe,
		struct drm_mode_modeinfo *mode)
{
	struct hdmi_pipe *hdmi_pipe = hdmi_pipe_from_intel_pipe(pipe);
	struct hdmi_config *config = &hdmi_pipe->config;
	struct intel_pipeline *pipeline = hdmi_pipe->base.pipeline;
	struct drm_mode_modeinfo *curr_mode;

	int err = 0;
	pr_info("ADF:HDMI: %s\n", __func__);

	if (!mode || !config || !pipeline) {
		pr_err("ADF:HDMI: %s: NULL input\n", __func__);
		err = -EINVAL;
		return err;
	}

	if (hdmi_pipe->dpms_state == DRM_MODE_DPMS_ON) {
		pr_info("ADF: HDMI: %s: HDMI already enabled\n", __func__);
		return 0;
	}

	mutex_lock(&config->ctx_lock);

	/* Avoiding i915 enter into DPMS */
	intel_adf_display_rpm_get();

	curr_mode = config->ctx.current_mode;
	err = chv_pipeline_off(pipeline);
	if (err) {
		pr_err("ADF:HDMI: %s: Pipeline off failed\n", __func__);
		goto out;
	}
	hdmi_pipe->dpms_state = DRM_MODE_DPMS_OFF;

	err = hdmi_prepare(pipe, mode);
	if (err) {
		pr_err("ADF:HDMI: %s: Prepare failed\n", __func__);
		goto out;
	}

	err = vlv_pipeline_on(pipeline, mode);
	if (err) {
		pr_err("ADF:HDMI: %s: Pipeline on failed\n", __func__);
		goto out;
	}
	hdmi_pipe->dpms_state = DRM_MODE_DPMS_ON;

	/* Update the latest applied mode to current context */
	memcpy(curr_mode, mode, sizeof(*mode));

out:
	mutex_unlock(&config->ctx_lock);
	return err;
}

static int hdmi_dpms(struct intel_pipe *pipe, u8 state)
{
	struct hdmi_pipe *hdmi_pipe = hdmi_pipe_from_intel_pipe(pipe);
	struct hdmi_config *config = &hdmi_pipe->config;
	struct intel_pipeline *pipeline = hdmi_pipe->base.pipeline;
	struct hdmi_monitor *monitor;
	struct drm_mode_modeinfo *mode;

	int err = 0;
	pr_info("ADF:HDMI: %s: Current_state = %d, requested_state = %d\n",
			__func__, hdmi_pipe->dpms_state, state);

	if (!config) {
		pr_err("ADF:HDMI: %s: Config not set\n", __func__);
		return -EINVAL;
	}

	if (!pipeline) {
		pr_err("ADF:HDMI: %s: Pipeline not set\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&config->ctx_lock);

	if (hdmi_pipe->dpms_state == state) {
		pr_err("ADF:HDMI: %s: DPMS State same as requested = %s\n",
				__func__, state ? "DPMS_OFF" : "DPMS_ON");

		goto exit;
	}

	switch (state) {
	case DRM_MODE_DPMS_ON:
		/* Get mode from connected monitor */
		monitor = config->ctx.monitor;
		if (!monitor) {
			pr_err("ADF: HDMI: %s: no connected monitor found\n",
					__func__);
			err = -EINVAL;
			goto exit;
		}

		/*
		 * In a resume call, we should pick the mode
		 * which was present before suspend, so give priority
		 * to current mode, than preferred mode
		 */
		if (config->ctx.current_mode)
			mode = config->ctx.current_mode;
		else
			mode = monitor->preferred_mode;
		intel_adf_display_rpm_get();
		/* Prepare pipe and port values */
		err = hdmi_prepare(pipe, mode);
		if (err) {
			pr_err("ADF:HDMI: %s: prepare failed\n", __func__);
			goto exit;
		}

		/* Enable pipeline */
		err = vlv_pipeline_on(pipeline, mode);
		if (err) {
			pr_err("ADF:HDMI: %s: Pipeline on failed\n", __func__);
			goto exit;
		}
		break;

	case DRM_MODE_DPMS_OFF:
		err = chv_pipeline_off(pipeline);
		intel_adf_display_rpm_put();
		if (err) {
			pr_err("ADF:HDMI: %s: Pipeline off failed\n", __func__);
			goto exit;
		}
		break;

	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	default:
		pr_err("ADF:HDMI: %s: Unsupported dpms mode\n", __func__);
		err = -EOPNOTSUPP;
		goto exit;
	}

	/* Update staus for successful DPMS */
	hdmi_pipe->dpms_state = state;

exit:
	mutex_unlock(&config->ctx_lock);
	return err;
}

/* Event handling funcs */
int hdmi_set_event(struct intel_pipe *pipe, u16 event, bool enabled)
{
	int ret;

	pr_info("ADF: HDMI: %s\n", __func__);
	ret = intel_adf_set_event(pipe, event, enabled);
	if (ret) {
		pr_err("ADF: HDMI: %s set event type=%d(%hu) failed\n",
			__func__, pipe->type, event);
		return ret;
	}
	return 0;
}

void hdmi_get_events(struct intel_pipe *pipe, u32 *events)
{
	pr_debug("ADF: HDMI: %s\n", __func__);
	if (intel_adf_get_events(pipe, events))
		pr_err("ADF: HDMI: %s get events (type=%d) failed\n",
			__func__, pipe->type);
}

void hdmi_handle_events(struct intel_pipe *pipe, u32 events)
{
	struct hdmi_pipe *hdmi_pipe = hdmi_pipe_from_intel_pipe(pipe);

	if (intel_adf_handle_events(pipe, events))
		pr_err("ADF: HDMI: %s handle events (type=%d) failed\n",
			 __func__, pipe->type);

	if (hdmi_pipe->ops.handle_events)
		if (hdmi_pipe->ops.handle_events(hdmi_pipe, events))
			pr_err("ADF: HDMI: %s handle pipe events failed\n",
					__func__);
}

u32 hdmi_get_supported_events(struct intel_pipe *pipe)
{
	return INTEL_PIPE_EVENT_VSYNC |
		INTEL_PORT_EVENT_HOTPLUG_DISPLAY |
		INTEL_PIPE_EVENT_AUDIO_BUFFERDONE |
		INTEL_PIPE_EVENT_AUDIO_UNDERRUN |
		INTEL_PIPE_EVENT_HOTPLUG_CONNECTED |
		INTEL_PIPE_EVENT_HOTPLUG_DISCONNECTED;
}

/* HDMI external ops */
struct intel_pipe_ops hdmi_base_ops = {
	.hw_init = hdmi_hw_init,
	.hw_deinit = hdmi_hw_deinit,
	.get_preferred_mode = hdmi_get_preferred_mode,
	.get_modelist = hdmi_get_modelist,
	.dpms = hdmi_dpms,
	.modeset = hdmi_modeset,
	.get_screen_size = hdmi_get_screen_size,
	.is_screen_connected = hdmi_is_screen_connected,
	.get_vsync_counter = hdmi_get_vsync_counter,
	.get_supported_events = hdmi_get_supported_events,
	.pre_post = hdmi_pre_post,
	.on_post = hdmi_on_post,
	.get_events = hdmi_get_events,
	.set_event = hdmi_set_event,
	.handle_events = hdmi_handle_events,
	.pre_validate = hdmi_pre_validate,
	.pre_post = hdmi_pre_post,
	.on_post = hdmi_on_post,
	.get_current_mode = hdmi_get_current_mode,
};
