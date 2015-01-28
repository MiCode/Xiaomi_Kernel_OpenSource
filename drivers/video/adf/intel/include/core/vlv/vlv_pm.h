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
 */

#ifndef _VLV_WATERMARK_H_
#define _VLV_WATERMARK_H_

#include <core/intel_dc_config.h>
#include <core/vlv/vlv_dc_regs.h>

struct vlv_pm {
	u32 offset;
	u32 pri_value;
	u32 sp1_value;
	u32 sp2_value;
};

extern bool vlv_pm_init(struct vlv_pm *pm, enum pipe);
extern bool vlv_pm_destroy(struct vlv_pm *pm);
bool vlv_pm_update_maxfifo_status(struct vlv_pm *pm, bool enable);
u32 vlv_pm_save_values(struct vlv_pm *pm, bool pri_plane,
		bool sp1_plane, bool sp2_plane, u32 val);
u32 vlv_pm_program_values(struct vlv_pm *pm, int num_planes);
u32 vlv_pm_flush_values(struct vlv_pm *pm, u32 event);
void vlv_pm_on_post(struct intel_dc_config *intel_config);
void vlv_pm_pre_validate(struct intel_dc_config *intel_config,
		struct intel_adf_post_custom_data *custom,
		struct intel_pipeline *intel_pipeline, struct intel_pipe *pipe);
void vlv_pm_pre_post(struct intel_dc_config *intel_config,
		struct intel_pipeline *intel_pipeline, struct intel_pipe *pipe);

#endif /*_VLV_WATERMARK_H_*/
