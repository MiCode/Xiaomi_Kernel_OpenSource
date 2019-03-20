/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef __H_MSM_VIDC_BUFFER_MEM_DEFS_H__
#define __H_MSM_VIDC_BUFFER_MEM_DEFS_H__

struct msm_vidc_dec_buff_size_calculators {
	u32 (*calculate_scratch_size)(struct msm_vidc_inst *inst, u32 width,
		u32 height, bool is_interlaced);
	u32 (*calculate_scratch1_size)(struct msm_vidc_inst *inst, u32 width,
		u32 height, u32 min_buf_count, bool split_mode_enabled);
	u32 (*calculate_persist1_size)(void);
};

struct msm_vidc_enc_buff_size_calculators {
	u32 (*calculate_scratch_size)(struct msm_vidc_inst *inst, u32 width,
		u32 height, u32 work_mode);
	u32 (*calculate_scratch1_size)(struct msm_vidc_inst *inst,
		u32 width, u32 height, u32 num_ref, bool ten_bit);
	u32 (*calculate_scratch2_size)(struct msm_vidc_inst *inst,
		u32 width, u32 height, u32 num_ref, bool ten_bit);
	u32 (*calculate_persist_size)(void);
};

void msm_vidc_init_buffer_size_calculators(struct msm_vidc_inst *inst);
int msm_vidc_init_buffer_count(struct msm_vidc_inst *inst);
int msm_vidc_get_extra_buff_count(struct msm_vidc_inst *inst,
	enum hal_buffer buffer_type);
u32 msm_vidc_calculate_dec_input_frame_size(struct msm_vidc_inst *inst);
u32 msm_vidc_calculate_dec_output_frame_size(struct msm_vidc_inst *inst);
u32 msm_vidc_calculate_dec_output_extra_size(struct msm_vidc_inst *inst);
u32 msm_vidc_calculate_enc_input_frame_size(struct msm_vidc_inst *inst);
u32 msm_vidc_calculate_enc_output_frame_size(struct msm_vidc_inst *inst);
u32 msm_vidc_calculate_enc_input_extra_size(struct msm_vidc_inst *inst);
u32 msm_vidc_calculate_enc_output_extra_size(struct msm_vidc_inst *inst);
u32 msm_vidc_set_buffer_count_for_thumbnail(struct msm_vidc_inst *inst);

#endif // __H_MSM_VIDC_BUFFER_MEM_DEFS_H__
