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
 * Create on 15 Sep 2014
 * Author: Sivakumar Thulasimani <sivakumar.thulasimani@intel.com>
 */

#ifndef _INTEL_DP_PIPE_H_
#define _INTEL_DP_PIPE_H_

#include <drm/drmP.h>
#include <core/intel_dc_config.h>
#include <core/common/dp/dp_panel.h>

#define DP_LINK_BW_1_62	    0x06
#define DP_LINK_BW_2_7      0x0a
#define DP_LINK_BW_5_4      0x14

struct dp_pipe {
	struct intel_pipe base;
	struct intel_pipeline *pipeline;
	struct dp_panel panel;
	struct drm_mode_modeinfo preferred_mode;
	struct drm_mode_modeinfo current_mode;
	struct link_params link_params;
	u8 dpms_state;
	bool panel_present;
};


/* Used by dp and fdi links */
struct intel_link_m_n {
	u32        tu;
	u32        gmch_m;
	u32        gmch_n;
	u32        link_m;
	u32        link_n;
};

static inline struct dp_pipe *to_dp_pipe(struct intel_pipe *pipe)
{
	return container_of(pipe, struct dp_pipe, base);
}

u32 dp_pipe_init(struct dp_pipe *pipe, struct device *dev,
	struct intel_plane *primary_plane, u8 idx,
	struct intel_pipeline *pipeline, enum intel_pipe_type type);

u32 dp_pipe_destroy(struct dp_pipe *pipe);

#endif /* _INTEL_DP_PIPE_H_ */
