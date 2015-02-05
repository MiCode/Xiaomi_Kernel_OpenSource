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

#define pr_fmt(fmt) "vlv_edp_drrs: " fmt

#include <core/common/dp/gen_dp_pipe.h>
#include <core/vlv/vlv_dc_config.h>

static void vlv_edp_drrs_init(struct intel_pipeline *pipeline)
{
	return;
}

static void vlv_edp_drrs_exit(struct intel_pipeline *pipeline)
{
	return;
}

static void vlv_edp_set_drrs_state(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_pipe *pipe = &disp->pipe;
	struct dp_pipe *dp_pipe = &disp->gen.dp;
	struct edp_drrs *drrs = &dp_pipe->drrs;
	struct drrs_info *state = &pipeline->drrs->drrs_state;
	enum drrs_refresh_rate_type target = state->target_rr_type;
	int val = REG_READ(pipe->offset);

	switch (target) {
	case DRRS_HIGH_RR:
		REG_WRITE(pipe->offset, val & ~PIPECONF_EDP_RR_SWITCH_VLV);
		break;
	case DRRS_LOW_RR:
		vlv_pipe_program_m2_n2(pipe, &drrs->downclock_mn);
		REG_WRITE(pipe->offset, val | PIPECONF_EDP_RR_SWITCH_VLV);
		break;
	default:
		pr_err("invalid refresh rate type\n");
	}
}

struct edp_drrs_platform_ops vlv_edp_drrs_ops = {
	.init = vlv_edp_drrs_init,
	.exit = vlv_edp_drrs_exit,
	.set_drrs_state = vlv_edp_set_drrs_state,
};

struct edp_drrs_platform_ops *get_vlv_edp_drrs_ops(void)
{
	return &vlv_edp_drrs_ops;
}
