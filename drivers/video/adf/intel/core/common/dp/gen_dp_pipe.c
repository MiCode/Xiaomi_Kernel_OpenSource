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
 * Created on 15 Dec 2014
 * Author: Sivakumar Thulasimani <sivakumar.thulasimani@intel.com>
 */

#include <drm/i915_adf.h>
#include <core/common/dp/gen_dp_pipe.h>
#include <core/intel_platform_config.h>
#include <intel_adf.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_dc_regs.h>
#include <core/vlv/vlv_pm.h>

#define  DATA_LINK_M_N_MASK     (0xffffff)
#define  DATA_LINK_N_MAX        (0x800000)


static void compute_m_n(u32 m, u32 n,
		u32 *ret_m, u32 *ret_n)
{
	*ret_n = min_t(u32, roundup_pow_of_two(n), DATA_LINK_N_MAX);
	*ret_m = div_u64((uint64_t) m * *ret_n, n);
	while (*ret_m > DATA_LINK_M_N_MASK ||
		*ret_n > DATA_LINK_M_N_MASK) {
		*ret_m >>= 1;
		*ret_n >>= 1;
	}
}

static void dp_pipe_compute_m_n(u32 bits_per_pixel, u32 nlanes,
		u32 pixel_clock, u32 link_clock, struct intel_link_m_n *m_n)
{
	m_n->tu = 64;

	/* link rate was passed so convert to link clock */
	link_clock = LINK_TO_DOT_CLK(link_clock);
	pr_info("bpp = %x, lanes = %x, pixel_clock = %x, link_clock = %x\n",
			bits_per_pixel, nlanes, pixel_clock, link_clock);

	compute_m_n(bits_per_pixel * pixel_clock,
			link_clock * nlanes * BITS_PER_BYTE,
			&m_n->gmch_m, &m_n->gmch_n);
	compute_m_n(pixel_clock, link_clock,
			&m_n->link_m, &m_n->link_n);
}

static void dp_pipe_get_current_mode(struct intel_pipe *pipe,
		struct drm_mode_modeinfo *mode)
{
	struct dp_pipe *dp_pipe = to_dp_pipe(pipe);

	if (dp_pipe->current_mode.clock == 0)
		return;

	if (!mode)
		pr_err("%s: null pointer passed\n", __func__);
	else
		*mode = dp_pipe->current_mode;
}

static bool get_next_link_params(struct dp_panel *panel,
		struct link_params *params, u32 dotclock)
{
	bool found = false;
	int link_bw = 0;
	u32 link_clock = 0, lane_count = 0;
	u64 bw_available = 0, dotclock_req = 0;

	if (params->link_bw == 0) {
		/* start with max bw */
		link_bw = params->link_bw = dp_panel_get_max_link_bw(panel);
		params->lane_count = dp_panel_get_max_lane_count(panel);
	} else {
		/* get the next lower link rate */
		switch (params->link_bw) {
		case DP_LINK_BW_1_62:
			pr_err("%s:Already tried lowest link rate\n", __func__);
			return false;
		case DP_LINK_BW_2_7:
			link_bw = DP_LINK_BW_1_62;
			break;
		case DP_LINK_BW_5_4:
			link_bw = DP_LINK_BW_2_7;
			break;
		default:
			BUG();
			break;
		}
		params->link_bw = link_bw;
	}

	/* temp to keep the value same */
	pr_err("%s::TBD optimize lane usage %x %x\n", __func__,
			params->link_bw, params->lane_count);
	/*
	 * FIXME: HACK : Avoiding lane optimization as of now to be
	 * revisted in future.
	 */
	return true;

	/* return the link_rate & lane_count that can drive the dotclock */
	for (lane_count = 1; lane_count <= params->lane_count;
		lane_count <<= 1) {
		link_clock = LINK_TO_DOT_CLK(link_bw);
		bw_available = (link_clock * lane_count * BITS_PER_BYTE);

		/*
		 * consider the bpp as well, dotclock is in KHz
		 * convert to Hz
		 */
		dotclock_req = dotclock * params->bpp;
		if (dotclock_req <= bw_available) {
			params->link_bw = link_bw;
			params->lane_count = lane_count;
			found = true;
			break;
		}
	}

	pr_err("%s: get %d %d\n", __func__, params->link_bw,
		params->lane_count);
}

void dp_pipe_dump_modes(struct drm_mode_modeinfo *modelist, u32 n_modes)
{
	u32 i = 0;
	struct drm_mode_modeinfo *modeinfo = modelist;

	for (i = 0; i < n_modes; i++) {
		pr_info("Mode::%d-%d-%d-%d-%d-%d-%d-%d-%d-%d\n",
			modeinfo->clock, modeinfo->hdisplay,
			modeinfo->hsync_start, modeinfo->hsync_end,
			modeinfo->htotal, modeinfo->vdisplay,
			modeinfo->vsync_start, modeinfo->vsync_end,
			modeinfo->vtotal, modeinfo->vrefresh);
		modeinfo++;
	}
}

static inline bool dp_pipe_compare_modes(struct dp_pipe *dp_pipe,
		struct drm_mode_modeinfo *mode1)
{
	struct drm_mode_modeinfo *mode2 = &dp_pipe->current_mode;

	/* Only DPMS calls modeset with NULL */
	if (!mode1 || !mode2)
		return false;

	if ((mode1->clock != mode2->clock) ||
		(mode1->hdisplay != mode2->hdisplay) ||
		(mode1->hsync_start != mode2->hsync_start) ||
		(mode1->htotal != mode2->htotal) ||
		(mode1->hsync_end != mode2->hsync_end) ||
		(mode1->vdisplay != mode2->vdisplay) ||
		(mode1->vsync_start != mode2->vsync_start) ||
		(mode1->vtotal != mode2->vtotal) ||
		(mode1->vrefresh != mode2->vrefresh))
		return false;
	else
		return true;
}

static int dp_pipe_modeset(struct intel_pipe *pipe,
		struct drm_mode_modeinfo *mode)
{
	struct dp_pipe *dp_pipe = to_dp_pipe(pipe);
	struct intel_pipeline *pipeline = dp_pipe->pipeline;
	struct intel_link_m_n m_n = {0};
	struct link_params *params;
	bool ret = false, err = 0;
	u8 dpcdval = 1;
	u32 dotclock;
	u32 bpp = 0;

	params = &dp_pipe->link_params;
	params->link_bw = 0;
	params->lane_count = 0;

	/*
	 * Avoid duplicate modesets, check both dpms state
	 * and mode being applied
	 */
	if ((dp_pipe->dpms_state == DRM_MODE_DPMS_ON) &&
		dp_pipe_compare_modes(dp_pipe, mode))
		goto modeset_exit;

	/* if NULL the current call is from dpms so use saved mode */
	if (mode == NULL)
		mode = &dp_pipe->current_mode;
	else
		intel_adf_display_rpm_get();

	dotclock = mode->clock;

	/* bpp = bits per color * 3, for 3 colors in pixel */
	bpp = dp_panel_get_bpc(&dp_pipe->panel, dotclock) * 3;
	dp_pipe_dump_modes(mode, 1);
	vlv_dp_backlight_seq(pipeline, false);

	do {
		/* pps off if edp display */
		vlv_dp_panel_power_seq(pipeline, false);
		err = vlv_pipeline_off(pipeline);
		if (err != 0) {
			pr_err("%s: pipeline off failed\n", __func__);
			goto modeset_exit;
		}

		params->bpp = (u32) bpp;
		ret = get_next_link_params(&dp_pipe->panel,
			params, dotclock);
		if (ret == false) {
			pr_err("%s: get link params failed\n", __func__);
			goto modeset_exit;
		}
		dp_panel_set_dpcd(&dp_pipe->panel, DP_SET_POWER,
			&dpcdval, 1);

		dp_pipe_compute_m_n(bpp, params->lane_count, mode->clock,
			params->link_bw, &m_n);
		pipeline->params.dp.m_n = &m_n;
		pipeline->params.dp.lane_count = params->lane_count;
		pipeline->params.dp.link_bw = params->link_bw;
		pipeline->params.dp.bpp = bpp;
		err = vlv_pipeline_on(pipeline, mode);
		if (err != 0) {
			pr_err("%s: pipeline on failed\n", __func__);
			goto modeset_exit;
		}

		/* pps on if edp display */
		vlv_dp_panel_power_seq(pipeline, true);

		ret = dp_panel_train_link(&dp_pipe->panel,
			&dp_pipe->link_params);
		/* retry with lower linkrate and appropriate lanes if failed */
	} while (ret == false);

	vlv_dp_backlight_seq(pipeline, true);

	/* Backup the current mode */
	dp_pipe->dpms_state = DRM_MODE_DPMS_ON;
	dp_pipe->current_mode = *mode;

modeset_exit:
	if (!err)
		pr_debug("%s:ModeSet Success\n", __func__);
	else
		pr_err("%s:Modeset failed :(\n", __func__);

	return err;
}

static int dp_pipe_dpms(struct intel_pipe *pipe, u8 state)
{
	struct dp_pipe *dp_pipe = to_dp_pipe(pipe);
	struct intel_pipeline *pipeline = dp_pipe->pipeline;
	u8 dpcdval = 0;
	u32 err = 0;

	pr_err("ADF: %s current_state = %d, requested_state = %d\n",
			__func__, dp_pipe->dpms_state, state);

	if (dp_pipe->dpms_state == state)
		return 0;

	switch (state) {
	case DRM_MODE_DPMS_ON:
		intel_adf_display_rpm_get();
		vlv_dpms(pipeline, state);
		err = dp_pipe_modeset(pipe, NULL);
		if (err != 0)
			goto dpms_exit;
		break;
	case DRM_MODE_DPMS_OFF:
		vlv_dp_backlight_seq(pipeline, false);
		vlv_dp_panel_power_seq(pipeline, false);
		dp_panel_set_dpcd(&dp_pipe->panel, DP_SET_POWER,
			&dpcdval, 1);
		err = vlv_pipeline_off(pipeline);
		vlv_dpms(pipeline, state);
		intel_adf_display_rpm_put();
		if (err != 0)
			goto dpms_exit;
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	default:
		pr_debug("%s: unsupported dpms mode\n", __func__);
		return -EOPNOTSUPP;
	}
	dp_pipe->dpms_state = state;

dpms_exit:
	pr_info("%s::exit:%x\n", __func__, (unsigned int) err);
	return err;
}

static bool dp_pipe_is_screen_connected(struct intel_pipe *pipe)
{
	struct dp_pipe *dp_pipe = to_dp_pipe(pipe);
	struct intel_pipeline *pipeline = dp_pipe->pipeline;
	struct dp_panel *panel = &dp_pipe->panel;
	bool panel_detected = false;

	panel_detected = vlv_is_screen_connected(pipeline);
	panel_detected = dp_panel_probe(panel, pipeline);
	if (panel_detected && dp_pipe->panel_present == false) {
		dp_panel_init(&dp_pipe->panel, pipeline);
		dp_pipe->panel_present = panel_detected;
	} else if ((panel_detected == false) && dp_pipe->panel_present) {
		/* panel removed */
		dp_pipe->panel_present = false;
		dp_panel_destroy(&dp_pipe->panel);
	}

	return panel_detected;
}

static void dp_pipe_get_modelist(struct intel_pipe *pipe,
		struct drm_mode_modeinfo **modelist, size_t *n_modes)
{
	struct dp_pipe *dp_pipe = to_dp_pipe(pipe);
	struct dp_panel *panel = &dp_pipe->panel;

	if (!dp_pipe->panel_present) {
		pr_err("DP panel not present\n");
		return;
	}

	if (!panel->no_probed_modes) {
		pr_err("%s call before probe, returning 0 modes\n", __func__);
		*n_modes = 0;
		return;
	}

	*modelist = panel->modelist;
	*n_modes = panel->no_probed_modes;

	dp_pipe_dump_modes(*modelist, *n_modes);
	pr_err("%s, number of modes =%d\n", __func__, (int)*n_modes);
}

static void dp_pipe_get_preferred_mode(struct intel_pipe *pipe,
		struct drm_mode_modeinfo **mode)
{
	struct dp_pipe *dp_pipe = to_dp_pipe(pipe);

	if (dp_pipe->panel.preferred_mode->clock == 0) {
		pr_err("%s:Panel not detected yet\n", __func__);
		return;
	}

	*mode = dp_pipe->panel.preferred_mode;
	pr_info("ADF: %s: Preferred Mode = %dx%d @%d\n", __func__,
		(*mode)->hdisplay, (*mode)->vdisplay, (*mode)->vrefresh);
}

static void dp_pipe_hw_deinit(struct intel_pipe *pipe)
{
	pr_err("ADF: %s\n", __func__);
	return;
}

static int dp_pipe_hw_init(struct intel_pipe *pipe)
{
	pr_err("ADF: %s\n", __func__);

	return 0;
}

static void dp_pipe_handle_events(struct intel_pipe *pipe, u32 events)
{
	struct workqueue_struct *hotplug_wq;
	struct work_struct *hotplug_work;

	if (intel_adf_handle_events(pipe, events))
		pr_err("ADF: DP: %s handle events (type=%d) failed\n",
				__func__, pipe->type);

	/* DP expects hot plug event */
	if (events & INTEL_PORT_EVENT_HOTPLUG_DISPLAY) {

		/* Validate input */
		if (!g_adf_context) {
			pr_err("ADF: %s: No adf context present\n", __func__);
			return;
		}

		hotplug_wq = g_adf_context->hotplug_wq;
		hotplug_work = (struct work_struct *)
			&g_adf_context->hotplug_work;
		queue_work(hotplug_wq, hotplug_work);
	}

	return;
}

static int dp_pipe_set_event(struct intel_pipe *pipe, u16 event, bool enabled)
{
	return intel_adf_set_event(pipe, event, enabled);
}

/*
 * FIXME: hardware vsync counter failed to work on ANN. use static SW
 * counter for now.
 */
static u32 vsync_counter;

#define VSYNC_COUNT_MAX_MASK 0xffffff

static void dp_pipe_get_events(struct intel_pipe *pipe, u32 *events)
{
	intel_adf_get_events(pipe, events);

	/*
	 * FIXME: should use hardware vsync counter.
	 */
	if (*events & INTEL_PIPE_EVENT_VSYNC) {
		if (++vsync_counter > VSYNC_COUNT_MAX_MASK)
			vsync_counter = 0;
	}

}

static int dp_pipe_get_screen_size(struct intel_pipe *pipe,
		u16 *width_mm, u16 *height_mm)
{
	struct dp_pipe *dp_pipe = to_dp_pipe(pipe);
	struct dp_panel *panel = &dp_pipe->panel;

	if (dp_pipe->panel_present) {
		*width_mm = panel->screen_width_mm;
		*height_mm = panel->screen_height_mm;
	} else {
		*width_mm = 0;
		*height_mm = 0;
	}

	pr_info("%s: DP Panel hXw=%dX%d\n",
		__func__, *width_mm, *height_mm);
	return 0;
}

static void dp_pipe_on_post(struct intel_pipe *pipe)
{
	struct dp_pipe *dp_pipe = to_dp_pipe(pipe);
	struct intel_pipeline *pipeline = dp_pipe->pipeline;
	struct vlv_pipeline *vlv_pipeline = to_vlv_pipeline(pipeline);
	struct intel_dc_config *intel_config = &vlv_pipeline->config->base;

	vlv_pm_on_post(intel_config, pipe);

	/* Re-enable PSR, if possible */
	vlv_edp_psr_update(pipeline);
}

static void dp_pipe_pre_validate(struct intel_pipe *pipe,
		struct intel_adf_post_custom_data *custom)
{
	struct dp_pipe *dp_pipe = to_dp_pipe(pipe);
	struct intel_pipeline *pipeline = dp_pipe->pipeline;
	struct vlv_pipeline *vlv_pipeline = to_vlv_pipeline(pipeline);
	struct intel_dc_config *intel_config = &vlv_pipeline->config->base;

	vlv_pm_pre_validate(intel_config, custom, pipeline, pipe);
}

static void dp_pipe_pre_post(struct intel_pipe *pipe)
{
	struct dp_pipe *dp_pipe = to_dp_pipe(pipe);
	struct intel_pipeline *pipeline = dp_pipe->pipeline;
	struct vlv_pipeline *vlv_pipeline = to_vlv_pipeline(pipeline);
	struct intel_dc_config *intel_config = &vlv_pipeline->config->base;

	/* Exit eDP PSR */
	vlv_edp_psr_exit(pipeline, false);

	vlv_pm_pre_post(intel_config, pipeline, pipe);
}

static u32 dp_pipe_get_supported_events(struct intel_pipe *pipe)
{
	return INTEL_PIPE_EVENT_VSYNC |
		INTEL_PORT_EVENT_HOTPLUG_DISPLAY |
		INTEL_PIPE_EVENT_HOTPLUG_CONNECTED |
		INTEL_PIPE_EVENT_HOTPLUG_DISCONNECTED;
}

u32 dp_pipe_get_vsync_counter(struct intel_pipe *pipe, u32 interval)
{
	u32 count = 0;
	u32 max_count_mask = VSYNC_COUNT_MAX_MASK;

	count = vsync_counter;
	count |= (~max_count_mask);
	count += interval;
	count &= max_count_mask;

	pr_debug("%s: count = %#x\n", __func__, count);

	return count;
}

static long dp_pipe_dpst_context(struct intel_pipe *pipe, unsigned long arg)
{
	struct dp_pipe *dp_pipe = to_dp_pipe(pipe);
	struct intel_pipeline *pipeline = dp_pipe->pipeline;
	long val = vlv_dpst_context(pipeline, arg);

	return val;
}

static long dp_pipe_dpst_irq_handler(struct intel_pipe *pipe)
{
	struct dp_pipe *dsi_pipe = to_dp_pipe(pipe);
	struct intel_pipeline *pipeline = dsi_pipe->pipeline;

	return vlv_dpst_irq_handler(pipeline);

}

static int dp_set_brightness(struct intel_pipe *pipe, int level)
{
	struct dp_pipe *dp_pipe = to_dp_pipe(pipe);
	struct intel_pipeline *pipeline = dp_pipe->pipeline;

	return vlv_dp_set_brightness(pipeline, level);
}

static int dp_get_brightness(struct intel_pipe *pipe)
{
	struct dp_pipe *dp_pipe = to_dp_pipe(pipe);
	struct intel_pipeline *pipeline = dp_pipe->pipeline;
	return vlv_dp_get_brightness(pipeline);
}

static struct intel_pipe_ops dp_base_ops = {
	.hw_init = dp_pipe_hw_init,
	.hw_deinit = dp_pipe_hw_deinit,
	.get_preferred_mode = dp_pipe_get_preferred_mode,
	.get_current_mode = dp_pipe_get_current_mode,
	.get_modelist = dp_pipe_get_modelist,
	.dpms = dp_pipe_dpms,
	.modeset = dp_pipe_modeset,
	.get_screen_size = dp_pipe_get_screen_size,
	.is_screen_connected = dp_pipe_is_screen_connected,
	.get_supported_events = dp_pipe_get_supported_events,
	.set_event = dp_pipe_set_event,
	.get_events = dp_pipe_get_events,
	.get_vsync_counter = dp_pipe_get_vsync_counter,
	.handle_events = dp_pipe_handle_events,
	.pre_validate = dp_pipe_pre_validate,
	.pre_post = dp_pipe_pre_post,
	.on_post = dp_pipe_on_post,
	.dpst_context = dp_pipe_dpst_context,
	.dpst_irq_handler = dp_pipe_dpst_irq_handler,
};

static struct intel_pipe_ops edp_base_ops;

u32 dp_pipe_init(struct dp_pipe *dp_pipe, struct device *dev,
	struct intel_plane *primary_plane, u8 idx,
	struct intel_pipeline *pipeline, enum intel_pipe_type type)
{
	struct intel_pipe *intel_pipe = &dp_pipe->base;
	u32 err = 0;

	intel_pipe->pipeline = pipeline;
	dp_pipe->pipeline = pipeline;
	dp_pipe->panel_present = false;
	dp_pipe->dpms_state = DRM_MODE_DPMS_OFF;

	/* encoder init  */
	if (type == INTEL_PIPE_DP)
		err = intel_pipe_init(intel_pipe, dev, idx, true,
			INTEL_PIPE_DP, primary_plane, &dp_base_ops, "dp_pipe");
	else {
		edp_base_ops = dp_base_ops;
		edp_base_ops.set_brightness = dp_set_brightness;
		edp_base_ops.get_brightness = dp_get_brightness;

		err = intel_pipe_init(intel_pipe, dev, idx, true,
			INTEL_PIPE_EDP, primary_plane, &edp_base_ops,
			"edp_pipe");

		/* Initialize PSR for eDP */
		vlv_edp_psr_init(pipeline);
	}
	dp_pipe->base.hp_reqd = true;
	pr_debug("%s: exit :%x\n", __func__, (unsigned int)err);
	return err;
}

u32 dp_pipe_destroy(struct dp_pipe *pipe)
{
	return 0;
}

int intel_adf_dp_hot_plug(struct dp_pipe *dp_pipe)
{
	if (dp_pipe_is_screen_connected(&dp_pipe->base)) {
		pr_info("DP: %s: Triggering self modeset\n", __func__);
		return intel_dp_self_modeset(dp_pipe);
	} else
		pr_err("DP screen is not connected\n");
	return 0;
}

int intel_dp_self_modeset(struct dp_pipe *dp_pipe)
{
	int ret = 0;
	struct intel_pipe *pipe = &dp_pipe->base;
	struct dp_panel *panel = &dp_pipe->panel;
	struct drm_mode_modeinfo *mode;

	mode = panel->preferred_mode;
	if (!mode) {
		pr_err("DP: %s No preferred mode\n", __func__);
		return -EINVAL;
	}

	if (pipe->ops->modeset) {
		ret = pipe->ops->modeset(pipe, mode);
		if (ret) {
			pr_err("DP: %s Self modeset failed\n", __func__);
			return ret;
		}
		pr_info("DP: %s Self modeset success\n", __func__);
	}

	return ret;
}
