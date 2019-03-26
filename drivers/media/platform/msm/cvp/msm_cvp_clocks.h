/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */


#ifndef _MSM_CVP_CLOCKS_H_
#define _MSM_CVP_CLOCKS_H_
#include "msm_cvp_internal.h"

int msm_cvp_set_clocks(struct msm_cvp_core *core);
int msm_cvp_comm_vote_bus(struct msm_cvp_core *core);
int msm_cvp_dcvs_try_enable(struct msm_cvp_inst *inst);
int msm_cvp_comm_scale_clocks_and_bus(struct msm_cvp_inst *inst);
int msm_cvp_comm_init_clocks_and_bus_data(struct msm_cvp_inst *inst);
void msm_cvp_comm_free_freq_table(struct msm_cvp_inst *inst);
int msm_cvp_decide_work_route(struct msm_cvp_inst *inst);
int msm_cvp_decide_work_mode(struct msm_cvp_inst *inst);
void msm_cvp_init_core_clk_ops(struct msm_cvp_core *core);
#endif
