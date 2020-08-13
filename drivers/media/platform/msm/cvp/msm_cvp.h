/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_CVP_H_
#define _MSM_CVP_H_

#include "msm_cvp_internal.h"
#include "msm_cvp_common.h"
#include "msm_cvp_clocks.h"
#include "msm_cvp_debug.h"
#include "msm_cvp_dsp.h"

static inline bool is_buf_param_valid(u32 buf_num, u32 offset)
{
	int max_buf_num;

	max_buf_num = sizeof(struct cvp_kmd_hfi_packet) /
			sizeof(struct cvp_buf_type);

	if (buf_num > max_buf_num)
		return false;

	if ((offset + buf_num * sizeof(struct cvp_buf_type)) >
			sizeof(struct cvp_kmd_hfi_packet))
		return false;

	return true;
}

int msm_cvp_handle_syscall(struct msm_cvp_inst *inst, struct cvp_kmd_arg *arg);
int msm_cvp_session_init(struct msm_cvp_inst *inst);
int msm_cvp_session_deinit(struct msm_cvp_inst *inst);
int msm_cvp_session_queue_stop(struct msm_cvp_inst *inst);
int cvp_stop_clean_fence_queue(struct msm_cvp_inst *inst);
#endif
