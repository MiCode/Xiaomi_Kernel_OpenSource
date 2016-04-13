/* Copyright (c) 2012, 2015-2017 The Linux Foundation. All rights reserved.
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
#define MSM_VDEC_DVC_NAME "msm_vidc_vdec"

int msm_vdec_inst_init(struct msm_vidc_inst *inst);
int msm_vdec_ctrl_init(struct msm_vidc_inst *inst,
	const struct v4l2_ctrl_ops *ctrl_ops);
int msm_vdec_enum_fmt(void *instance, struct v4l2_fmtdesc *f);
int msm_vdec_s_fmt(void *instance, struct v4l2_format *f);
int msm_vdec_g_fmt(void *instance, struct v4l2_format *f);
int msm_vdec_s_ctrl(void *instance, struct v4l2_ctrl *ctrl);
int msm_vdec_g_ctrl(void *instance, struct v4l2_ctrl *ctrl);
int msm_vdec_s_ext_ctrl(void *instance, struct v4l2_ext_controls *a);
struct vb2_ops *msm_vdec_get_vb2q_ops(void);

#endif
