/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_VIDC_CLOCKS_H_
#define _MSM_VIDC_CLOCKS_H_
#include "msm_vidc_internal.h"

void msm_clock_data_reset(struct msm_vidc_inst *inst);
void msm_dcvs_reset(struct msm_vidc_inst *inst);
int msm_vidc_set_clocks(struct msm_vidc_core *core, u32 sid);
int msm_comm_vote_bus(struct msm_vidc_inst *inst);
int msm_dcvs_try_enable(struct msm_vidc_inst *inst);
bool res_is_less_than(u32 width, u32 height, u32 ref_width, u32 ref_height);
bool res_is_greater_than(u32 width, u32 height, u32 ref_width, u32 ref_height);
bool res_is_less_than_or_equal_to(u32 width, u32 height,
	u32 ref_width, u32 ref_height);
bool res_is_greater_than_or_equal_to(u32 width, u32 height,
	u32 ref_width, u32 ref_height);
int msm_vidc_get_mbs_per_frame(struct msm_vidc_inst *inst);
int msm_vidc_get_fps(struct msm_vidc_inst *inst);
int msm_comm_scale_clocks_and_bus(struct msm_vidc_inst *inst, bool do_bw_calc);
int msm_comm_init_clocks_and_bus_data(struct msm_vidc_inst *inst);
int msm_vidc_decide_work_route_iris1(struct msm_vidc_inst *inst);
int msm_vidc_decide_work_mode_iris1(struct msm_vidc_inst *inst);
int msm_vidc_decide_work_route_iris2(struct msm_vidc_inst *inst);
int msm_vidc_decide_work_mode_iris2(struct msm_vidc_inst *inst);
int msm_vidc_decide_core_and_power_mode_ar50lt(struct msm_vidc_inst *inst);
int msm_vidc_decide_core_and_power_mode_iris1(struct msm_vidc_inst *inst);
int msm_vidc_decide_core_and_power_mode_iris2(struct msm_vidc_inst *inst);
void msm_print_core_status(struct msm_vidc_core *core, u32 core_id, u32 sid);
void msm_comm_free_input_cr_table(struct msm_vidc_inst *inst);
void msm_comm_update_input_cr(struct msm_vidc_inst *inst, u32 index,
	u32 cr);
void update_recon_stats(struct msm_vidc_inst *inst,
	struct recon_stats_type *recon_stats);
void msm_vidc_init_core_clk_ops(struct msm_vidc_core *core);
bool res_is_greater_than(u32 width, u32 height,
		u32 ref_width, u32 ref_height);
bool res_is_less_than(u32 width, u32 height,
		u32 ref_width, u32 ref_height);
int msm_vidc_set_bse_vpp_delay(struct msm_vidc_inst *inst);
bool is_vpp_delay_allowed(struct msm_vidc_inst *inst);
#endif
