/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#ifndef _MSM_VENC_H_
#define _MSM_VENC_H_

#include <media/msm_vidc.h>
#include "msm_vidc_internal.h"

int msm_venc_inst_init(struct msm_vidc_inst *inst);
int msm_venc_ctrl_init(struct msm_vidc_inst *inst);
int msm_venc_querycap(void *instance, struct v4l2_capability *cap);
int msm_venc_enum_fmt(void *instance, struct v4l2_fmtdesc *f);
int msm_venc_s_fmt(void *instance, struct v4l2_format *f);
int msm_venc_g_fmt(void *instance, struct v4l2_format *f);
int msm_venc_s_ctrl(void *instance, struct v4l2_control *a);
int msm_venc_g_ctrl(void *instance, struct v4l2_control *a);
int msm_venc_reqbufs(void *instance, struct v4l2_requestbuffers *b);
int msm_venc_prepare_buf(struct msm_vidc_inst *inst, struct v4l2_buffer *b);
int msm_venc_qbuf(struct msm_vidc_inst *inst, struct v4l2_buffer *b);
int msm_venc_dqbuf(struct msm_vidc_inst *inst, struct v4l2_buffer *b);
int msm_venc_streamon(struct msm_vidc_inst *inst, enum v4l2_buf_type i);
int msm_venc_streamoff(struct msm_vidc_inst *inst, enum v4l2_buf_type i);
struct vb2_ops *msm_venc_get_vb2q_ops(void);

#endif
