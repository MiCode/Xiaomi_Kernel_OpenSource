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
 * Create on 12 Dec 2014
 * Author: Shashank Sharma <shashank.sharma@intel.com>
 */

#include <drm/drm_mode.h>
#include <drm/i915_drm.h>

#include <core/intel_dc_config.h>
#include <core/vlv/vlv_dc_regs.h>
#include <core/vlv/vlv_dc_hw.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/chv_dc_regs.h>
#include <core/common/hdmi/gen_hdmi_pipe.h>
#include <intel_adf_device.h>

bool intel_adf_hdmi_supports_audio(struct edid *edid)
{
	return true;
}

bool intel_adf_hdmi_mode_valid(struct drm_mode_modeinfo *mode)
{
	if (mode) {
		/* Check pixclk and interlaced modes for now */
		if ((mode->clock > CHV_HDMI_MAX_CLK_KHZ) ||
			(mode->clock < CHV_HDMI_MIN_CLK_KHZ) ||
				(mode->flags & DRM_MODE_FLAG_DBLSCAN) ||
					(mode->flags & DRM_MODE_FLAG_INTERLACE))
			goto invalid;
		return true;
	}

invalid:
	return false;
}

struct drm_display_mode *
intel_adf_hdmi_get_force_mode(void)
{
	/* Fixme: Get forced mode from command line */
	return NULL;
}

void hdmi_monitor_destroy(struct hdmi_monitor *monitor)
{
	if (monitor) {
		kfree(monitor->preferred_mode);
		kfree(monitor->edid);
		kfree(monitor);
	}

	pr_info("ADF: HDMI: %s", __func__);
	return;
}

int hdmi_context_init(struct hdmi_context *context)
{
	struct hdmi_config *hdmi_config;
	struct hdmi_pipe *hdmi_pipe;
	int ret;

	if (!context) {
		/* How ?? */
		pr_err("ADF: HDMI: %s NULL input\n", __func__);
		return -EINVAL;
	}

	pr_info("ADF: HDMI: %s\n", __func__);
	hdmi_config = hdmi_config_from_ctx(context);
	hdmi_pipe = hdmi_pipe_from_cfg(hdmi_config);

	memset((void *)context, 0, sizeof(*context));
	atomic_set(&context->connected, 0);
	context->current_mode = kzalloc(sizeof(struct drm_mode_modeinfo),
			GFP_KERNEL);
	if (!context->current_mode) {
		pr_err("ADF: HDMI: %s OOM (current mode)\n", __func__);
		return -EINVAL;
	}

	/* Probe the current HDMI status */
	ret = intel_adf_hdmi_probe(hdmi_pipe, true);
	if (ret) {
		pr_err("ADF: HDMI: %s Probing HDMI failed\n", __func__);
		return ret;
	}

#ifdef INTEL_ADF_HDMI_SELF_MODESET_AT_BOOT
	/* If hdmi connected, do modeset on pref mode */
	if (atomic_read(&context->connected)) {
		pr_info("ADF: HDMI: %s: Triggering self modeset\n", __func__);
		ret = intel_hdmi_self_modeset(hdmi_pipe);
		if (ret) {
			pr_err("ADF: HDMI: %s Modeset failed\n", __func__);
			return ret;
		}
	}
#endif
	return 0;
}

void hdmi_context_destroy(struct hdmi_context *context)
{
	if (context) {
		kfree(context->current_mode);
		hdmi_monitor_destroy(context->monitor);
	}
	pr_err("ADF: HDMI: %s\n", __func__);
}

int hdmi_config_init(struct hdmi_config *config, u8 pipe_id)
{
	int ret;

	pr_info("ADF: HDMI: %s\n", __func__);
	if (!config) {
		pr_err("ADF: HDMI: %s: NULL parameter\n", __func__);
		return -EINVAL;
	}

	/*
	 * Init mutex before creating context, as it can trigger
	 * a modeset
	 */
	mutex_init(&config->ctx_lock);

	/* Clear context */
	memset(&config->ctx, 0, sizeof(struct hdmi_context));

	/* Init current context */
	ret = hdmi_context_init(&config->ctx);
	if (ret) {
		pr_err("ADF: HDMI: %s Context init failed\n", __func__);
		return ret;
	}

	/* Now load config */
	config->pipe = pipe_id;
	config->changed = 0;
	config->force_mode =
			intel_adf_hdmi_get_force_mode();
	if (!config->force_mode)
		pr_info("ADF: HDMI: %s No forced mode found\n", __func__);

	return 0;
}

void hdmi_config_destroy(struct hdmi_config *config)
{
	if (config) {
		hdmi_context_destroy(&config->ctx);
		kfree(config->force_mode);
	}
	pr_info("ADF: HDMI: %s\n", __func__);
	return;
}

void hdmi_pipe_destroy(struct hdmi_pipe *pipe)
{
	if (pipe)
		hdmi_config_destroy(&pipe->config);
	pr_info("ADF: HDMI: %s", __func__);
}

int hdmi_pipe_init(struct hdmi_pipe *pipe,
	struct device *dev, struct intel_plane *primary_plane,
	u8 pipe_id, struct intel_pipeline *pipeline)
{
	int ret;
	int count;

	pr_info("ADF: HDMI: %s:\n", __func__);

	if (!pipe || !primary_plane | !pipeline) {
		pr_err("ADF: HDMI: %s NULL input\n", __func__);
		return -EINVAL;
	}

	memset(pipe, 0, sizeof(struct hdmi_pipe));
	pipe->base.pipeline = pipeline;
	/* Fixme: Hardcoding bpp */
	pipeline->params.hdmi.bpp = 24;

	/* Load HDMI interface ops */
	pipe->ops.set_event = intel_adf_hdmi_set_events;
	pipe->ops.get_events = intel_adf_hdmi_get_events;
	pipe->ops.handle_events = intel_adf_hdmi_handle_events;
	pipe->ops.get_hw_state = intel_adf_hdmi_get_hw_events;
	pipe->dpms_state = DRM_MODE_DPMS_OFF;

	/* Init the PIPE */
	ret = intel_pipe_init(&pipe->base, dev, pipe_id, true, INTEL_PIPE_HDMI,
		primary_plane, &hdmi_base_ops, "hdmi_pipe");
	if (ret) {
		pr_err("ADF: HDMI: pipe init failed [%s]\n", __func__);
		goto fail_pipe;
	}

	/*
	 * Configure HDMI
	 * This function can probe HDMI and try to do a modeset
	 * if a monitor is connected during bootup
	 * So make sure, to call this function after the ops are
	 * loaded
	 */
	ret = hdmi_config_init(&pipe->config, pipe_id);
	if (ret) {
		pr_err("ADF: HDMI: %s: Config init failed\n", __func__);
		goto fail_config;
	}

	/* Initialize the LUT */
	for (count = 0; count < 256; count++) {
		pipe->config.lut_r[count] = count;
		pipe->config.lut_g[count] = count;
		pipe->config.lut_b[count] = count;
	}

	pipe->config.pixel_multiplier = 1;

	/* Request hotplug enabling */
	pipe->base.hp_reqd = true;

	return 0;

fail_pipe:
	hdmi_pipe_destroy(pipe);

fail_config:
	return ret;
}
