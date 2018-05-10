/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MSM_VIDC_CVP_H_
#define _MSM_VIDC_CVP_H_

#include "msm_vidc_internal.h"
#include "msm_vidc_common.h"
#include "msm_vidc_clocks.h"
#include "msm_vidc_debug.h"

void handle_session_register_buffer_done(enum hal_command_response cmd,
		void *resp);
void handle_session_unregister_buffer_done(enum hal_command_response cmd,
		void *resp);
int msm_vidc_cvp(struct msm_vidc_inst *inst, struct msm_vidc_arg *arg);
int msm_cvp_inst_init(struct msm_vidc_inst *inst);
int msm_cvp_inst_deinit(struct msm_vidc_inst *inst);
int msm_cvp_inst_pause(struct msm_vidc_inst *inst);
int msm_cvp_inst_resume(struct msm_vidc_inst *inst);
int msm_cvp_ctrl_init(struct msm_vidc_inst *inst,
		const struct v4l2_ctrl_ops *ctrl_ops);
#endif
