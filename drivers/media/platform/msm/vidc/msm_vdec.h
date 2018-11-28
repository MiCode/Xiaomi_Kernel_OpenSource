/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 */
#ifndef _MSM_VDEC_H_
#define _MSM_VDEC_H_

#include "msm_vidc.h"
#include "msm_vidc_internal.h"
#define MSM_VDEC_DVC_NAME "msm_vidc_vdec"

int msm_vdec_inst_init(struct msm_vidc_inst *inst);
int msm_vdec_ctrl_init(struct msm_vidc_inst *inst,
	const struct v4l2_ctrl_ops *ctrl_ops);
int msm_vdec_enum_fmt(void *instance, struct v4l2_fmtdesc *f);
int msm_vdec_s_fmt(void *instance, struct v4l2_format *f);
int msm_vdec_s_ctrl(void *instance, struct v4l2_ctrl *ctrl);
int msm_vdec_g_ctrl(void *instance, struct v4l2_ctrl *ctrl);
#endif
