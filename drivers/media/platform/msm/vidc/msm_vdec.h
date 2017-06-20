/* Copyright (c) 2012, 2015 The Linux Foundation. All rights reserved.
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
#ifndef _MSM_VDEC_H_
#define _MSM_VDEC_H_

#include <media/msm_vidc.h>
#include "msm_vidc_internal.h"

int msm_vdec_inst_init(struct msm_vidc_inst *inst);
int msm_vdec_ctrl_init(struct msm_vidc_inst *inst);
int msm_vdec_querycap(struct msm_vidc_inst *inst, struct v4l2_capability *cap);
int msm_vdec_enum_fmt(struct msm_vidc_inst *inst, struct v4l2_fmtdesc *f);
int msm_vdec_s_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f);
int msm_vdec_g_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f);
int msm_vdec_s_ext_ctrl(struct msm_vidc_inst *inst,
		struct v4l2_ext_controls *a);
int msm_vdec_reqbufs(struct msm_vidc_inst *inst, struct v4l2_requestbuffers *b);
int msm_vdec_prepare_buf(struct msm_vidc_inst *inst, struct v4l2_buffer *b);
int msm_vdec_release_buf(struct msm_vidc_inst *inst, struct v4l2_buffer *b);
int msm_vdec_qbuf(struct msm_vidc_inst *inst, struct v4l2_buffer *b);
int msm_vdec_dqbuf(struct msm_vidc_inst *inst, struct v4l2_buffer *b);
int msm_vdec_streamon(struct msm_vidc_inst *inst, enum v4l2_buf_type i);
int msm_vdec_streamoff(struct msm_vidc_inst *inst, enum v4l2_buf_type i);
int msm_vdec_cmd(struct msm_vidc_inst *inst, struct v4l2_decoder_cmd *dec);
int msm_vdec_s_parm(struct msm_vidc_inst *inst, struct v4l2_streamparm *a);
const struct vb2_ops *msm_vdec_get_vb2q_ops(void);
void msm_vdec_g_ctrl(struct msm_vidc_ctrl **ctrls, int *num_ctrls);
void msm_vdec_ctrl_sort(void);

#endif
