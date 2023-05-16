/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __H_MSM_VIDC_BUFFER_MEM_DEFS_H__
#define __H_MSM_VIDC_BUFFER_MEM_DEFS_H__

/* extra o/p buffers in case of dcvs */
#define DCVS_DEC_EXTRA_OUTPUT_BUFFERS 4
#define DCVS_ENC_EXTRA_INPUT_BUFFERS DCVS_DEC_EXTRA_OUTPUT_BUFFERS

struct msm_vidc_dec_buff_size_calculators {
	u32 (*calculate_scratch_size)(struct msm_vidc_inst *inst, u32 width,
		u32 height, bool is_interlaced);
	u32 (*calculate_scratch1_size)(struct msm_vidc_inst *inst, u32 width,
		u32 height, u32 min_buf_count, bool split_mode_enabled,
		u32 num_vpp_pipes);
	u32 (*calculate_persist1_size)(void);
};

struct msm_vidc_enc_buff_size_calculators {
	u32 (*calculate_scratch_size)(struct msm_vidc_inst *inst, u32 width,
		u32 height, u32 work_mode, u32 num_vpp_pipes);
	u32 (*calculate_scratch1_size)(struct msm_vidc_inst *inst,
		u32 width, u32 height, u32 num_ref, bool ten_bit,
		u32 num_vpp_pipes);
	u32 (*calculate_scratch2_size)(struct msm_vidc_inst *inst,
		u32 width, u32 height, u32 num_ref, bool ten_bit,
		bool downscale, u32 rotation_val, u32 flip);
	u32 (*calculate_persist_size)(void);
};

void msm_vidc_init_buffer_size_calculators(struct msm_vidc_inst *inst);
int msm_vidc_calculate_input_buffer_count(struct msm_vidc_inst *inst);
int msm_vidc_calculate_output_buffer_count(struct msm_vidc_inst *inst);
int msm_vidc_calculate_buffer_counts(struct msm_vidc_inst *inst);
int msm_vidc_get_extra_buff_count(struct msm_vidc_inst *inst,
	enum hal_buffer buffer_type);
u32 msm_vidc_calculate_dec_input_frame_size(struct msm_vidc_inst *inst);
u32 msm_vidc_calculate_dec_output_frame_size(struct msm_vidc_inst *inst);
u32 msm_vidc_calculate_dec_output_extra_size(struct msm_vidc_inst *inst);
u32 msm_vidc_calculate_enc_input_frame_size(struct msm_vidc_inst *inst);
u32 msm_vidc_calculate_enc_output_frame_size(struct msm_vidc_inst *inst);
u32 msm_vidc_calculate_enc_input_extra_size(struct msm_vidc_inst *inst);
u32 msm_vidc_calculate_enc_output_extra_size(struct msm_vidc_inst *inst);

#endif // __H_MSM_VIDC_BUFFER_MEM_DEFS_H__
