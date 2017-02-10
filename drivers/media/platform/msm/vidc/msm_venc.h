/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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
#define MSM_VENC_DVC_NAME "msm_vidc_venc"

int msm_venc_inst_init(struct msm_vidc_inst *inst);
int msm_venc_ctrl_init(struct msm_vidc_inst *inst,
	const struct v4l2_ctrl_ops *ctrl_ops);
int msm_venc_enum_fmt(void *instance, struct v4l2_fmtdesc *f);
int msm_venc_s_fmt(void *instance, struct v4l2_format *f);
int msm_venc_s_ctrl(void *instance, struct v4l2_ctrl *ctrl);
int msm_venc_s_ext_ctrl(void *instance, struct v4l2_ext_controls *a);

#endif
