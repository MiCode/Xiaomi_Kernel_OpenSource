/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */
#ifndef _MSM_VENC_H_
#define _MSM_VENC_H_

#include "msm_vidc.h"
#include "msm_vidc_internal.h"
#define MSM_VENC_DVC_NAME "msm_vidc_venc"

int msm_venc_inst_init(struct msm_vidc_inst *inst);
int msm_venc_ctrl_init(struct msm_vidc_inst *inst,
		const struct v4l2_ctrl_ops *ctrl_ops);
int msm_venc_enum_fmt(struct msm_vidc_inst *inst,
		struct v4l2_fmtdesc *f);
int msm_venc_s_fmt(struct msm_vidc_inst *inst,
		struct v4l2_format *f);
int msm_venc_g_fmt(struct msm_vidc_inst *inst,
		struct v4l2_format *f);
int msm_venc_set_default_profile(struct msm_vidc_inst *inst);
int msm_venc_s_ctrl(struct msm_vidc_inst *inst,
		struct v4l2_ctrl *ctrl);
int msm_venc_set_properties(struct msm_vidc_inst *inst);
int msm_venc_set_extradata(struct msm_vidc_inst *inst);
int msm_venc_set_frame_rate(struct msm_vidc_inst *inst);
int msm_venc_set_bitrate(struct msm_vidc_inst *inst);
int msm_venc_set_layer_bitrate(struct msm_vidc_inst *inst);
int msm_venc_set_operating_rate(struct msm_vidc_inst *inst);
int msm_venc_set_idr_period(struct msm_vidc_inst *inst);
int msm_venc_set_intra_period(struct msm_vidc_inst *inst);
int msm_venc_set_ltr_useframe(struct msm_vidc_inst *inst);
int msm_venc_set_ltr_markframe(struct msm_vidc_inst *inst);
int msm_venc_set_dyn_qp(struct msm_vidc_inst *inst, struct v4l2_ctrl *ctrl);
int msm_venc_set_request_keyframe(struct msm_vidc_inst *inst);
int msm_venc_set_intra_refresh_mode(struct msm_vidc_inst *inst);
int msm_venc_set_hp_max_layer(struct msm_vidc_inst *inst);
int msm_venc_set_hp_layer(struct msm_vidc_inst *inst);
int msm_venc_set_base_layer_priority_id(struct msm_vidc_inst *inst);
int msm_venc_check_dynamic_flip_constraints(struct msm_vidc_inst *inst);
int msm_venc_set_dynamic_flip(struct msm_vidc_inst *inst);
int msm_venc_set_lossless(struct msm_vidc_inst *inst);
int msm_venc_set_blur_resolution(struct msm_vidc_inst *inst);
int msm_venc_set_cvp_skipratio(struct msm_vidc_inst *inst);
int handle_all_intra_restrictions(struct msm_vidc_inst *inst);
int check_blur_restrictions(struct msm_vidc_inst *inst);
int msm_venc_set_frame_quality(struct msm_vidc_inst *inst);
int msm_venc_set_image_grid(struct msm_vidc_inst *inst);
#endif
