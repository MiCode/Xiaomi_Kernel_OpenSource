/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_CVP_INTERNAL_H_
#define _MSM_CVP_INTERNAL_H_

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
