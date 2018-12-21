/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */


#ifndef _MSM_CVP_CLOCKS_H_
#define _MSM_CVP_CLOCKS_H_
#include "msm_cvp_internal.h"

/* extra o/p buffers in case of encoder dcvs */
#define DCVS_ENC_EXTRA_INPUT_BUFFERS 4

/* extra o/p buffers in case of decoder dcvs */
#define DCVS_DEC_EXTRA_OUTPUT_BUFFERS 4

void msm_cvp_clock_data_reset(struct msm_cvp_inst *inst);
int msm_cvp_validate_operating_rate(struct msm_cvp_inst *inst,
	u32 operating_rate);
int msm_cvp_get_extra_buff_count(struct msm_cvp_inst *inst,
	enum hal_buffer buffer_type);
int msm_cvp_set_clocks(struct msm_cvp_core *core);
int msm_cvp_comm_vote_bus(struct msm_cvp_core *core);
int msm_cvp_dcvs_try_enable(struct msm_cvp_inst *inst);
int msm_cvp_get_mbs_per_frame(struct msm_cvp_inst *inst);
int msm_cvp_comm_scale_clocks_and_bus(struct msm_cvp_inst *inst);
int msm_cvp_comm_init_clocks_and_bus_data(struct msm_cvp_inst *inst);
void msm_cvp_comm_free_freq_table(struct msm_cvp_inst *inst);
int msm_cvp_decide_work_route(struct msm_cvp_inst *inst);
int msm_cvp_decide_work_mode(struct msm_cvp_inst *inst);
int msm_cvp_decide_core_and_power_mode(struct msm_cvp_inst *inst);
void msm_cvp_print_core_status(struct msm_cvp_core *core, u32 core_id);
void msm_cvp_clear_freq_entry(struct msm_cvp_inst *inst,
	u32 device_addr);
void msm_cvp_comm_free_input_cr_table(struct msm_cvp_inst *inst);
void msm_cvp_comm_update_input_cr(struct msm_cvp_inst *inst, u32 index,
	u32 cr);
void cvp_update_recon_stats(struct msm_cvp_inst *inst,
	struct recon_stats_type *recon_stats);
void msm_cvp_init_core_clk_ops(struct msm_cvp_core *core);
#endif
