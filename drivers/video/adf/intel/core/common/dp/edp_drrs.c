/*
 * Copyright (C) 2015, Intel Corporation.
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
 * Author:
 * Durgadoss R <durgadoss.r@intel.com>
 */

#define pr_fmt(fmt) "adf_edp_drrs: " fmt

#include <linux/delay.h>
#include <drm/i915_drm.h>
#include <intel_adf_device.h>
#include <core/common/intel_drrs.h>
#include <core/common/dp/gen_dp_pipe.h>
#include <core/common/drm_modeinfo_ops.h>
#include <core/vlv/vlv_dc_config.h>

void intel_edp_set_drrs_state(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct dp_pipe *dp_pipe = &disp->gen.dp;

	dp_pipe->drrs.platform_ops->set_drrs_state(pipeline);
}

static
struct drm_mode_modeinfo *find_downclock_mode(struct intel_pipe *i_pipe,
				struct drm_mode_modeinfo *fixed)
{
	struct drm_mode_modeinfo *modelist, *tmp = NULL;
	bool ret;
	int i;
	size_t n_modes = 0;
	int lower_clock = fixed->clock;

	if (!i_pipe->ops->get_modelist)
		return NULL;

	i_pipe->ops->get_modelist(i_pipe, &modelist, &n_modes);
	if (!n_modes) {
		pr_err("Cannot obtain modelist\n");
		return NULL;
	}

	for (i = 0; i < n_modes; i++) {
		ret = drm_modeinfo_equal_no_clocks(&modelist[i], fixed);
		if (ret && modelist[i].clock < lower_clock) {
			tmp = &modelist[i];
			lower_clock = modelist[i].clock;
		}
	}

	if (lower_clock < fixed->clock)
		return drm_modeinfo_duplicate(tmp);

	return NULL;
}

int intel_edp_drrs_init(struct intel_pipeline *pipeline)
{
	struct adf_drrs *drrs = pipeline->drrs;
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct dp_pipe *dp_pipe = &disp->gen.dp;
	struct intel_pipe *i_pipe = &dp_pipe->base;
	struct drm_mode_modeinfo *downclock, *fixed, *preferred;
	u8 bpp, link_bw, lane_count;
	int ret = -EINVAL;

	if (drrs->vbt.drrs_type != SEAMLESS_DRRS_SUPPORT) {
		pr_err("VBT does not support DRRS\n");
		return ret;
	}

	if (IS_VALLEYVIEW() || IS_CHERRYVIEW())
		dp_pipe->drrs.platform_ops = get_vlv_edp_drrs_ops();
	else
		dp_pipe->drrs.platform_ops = NULL;

	if (!dp_pipe->drrs.platform_ops ||
			!dp_pipe->drrs.platform_ops->init ||
			!dp_pipe->drrs.platform_ops->set_drrs_state) {
		pr_err("Required platform ops are NULL\n");
		return ret;
	}

	if (!i_pipe->ops->get_preferred_mode)
		return ret;

	i_pipe->ops->get_preferred_mode(i_pipe, &preferred);
	if (!preferred) {
		pr_err("Failed to obtain edp preferred mode\n");
		return ret;
	}

	/* Obtain fixed mode */
	fixed = drm_modeinfo_duplicate(preferred);
	if (!fixed) {
		pr_err("Failed to create fixed mode\n");
		return ret;
	}
	if (fixed->vrefresh == 0)
		fixed->vrefresh = drm_modeinfo_vrefresh(fixed);

	downclock = find_downclock_mode(i_pipe, fixed);
	if (!downclock)
		goto free_fixed_mode;

	pr_debug("eDP DRRS modes:\n");
	drm_modeinfo_debug_printmodeline(fixed);
	drm_modeinfo_debug_printmodeline(downclock);

	lane_count = pipeline->params.dp.lane_count;
	link_bw = pipeline->params.dp.link_bw;
	bpp = pipeline->params.dp.bpp;

	/* Calculate m_n_tu for fixed and downclock modes */
	dp_pipe_compute_m_n(bpp, lane_count, fixed->clock, link_bw,
				&dp_pipe->drrs.fixed_mn);

	dp_pipe_compute_m_n(bpp, lane_count, downclock->clock, link_bw,
				&dp_pipe->drrs.downclock_mn);

	/* We are good to go .. */
	drrs->panel_mode.fixed_mode = fixed;
	drrs->panel_mode.downclock_mode = downclock;
	drrs->panel_mode.target_mode = NULL;

	return 0;

free_fixed_mode:
	drm_modeinfo_destroy(fixed);
	return ret;
}

void intel_edp_drrs_exit(struct intel_pipeline *pipeline)
{
	struct adf_drrs *drrs = pipeline->drrs;

	drm_modeinfo_destroy(drrs->panel_mode.downclock_mode);
	drm_modeinfo_destroy(drrs->panel_mode.fixed_mode);
	drrs->has_drrs = false;
}

struct drrs_encoder_ops edp_drrs_ops = {
	.init = intel_edp_drrs_init,
	.exit = intel_edp_drrs_exit,
	.set_drrs_state = intel_edp_set_drrs_state,
};

/* Called by intel_drrs_init() to get ->ops for edp panel */
struct drrs_encoder_ops *intel_get_edp_drrs_ops(void)
{
	return &edp_drrs_ops;
}
