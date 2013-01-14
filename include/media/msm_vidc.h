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

#ifndef _MSM_VIDC_H_
#define _MSM_VIDC_H_

#include <linux/videodev2.h>
#include <linux/poll.h>

enum core_id {
	MSM_VIDC_CORE_0 = 0,
	MSM_VIDC_CORES_MAX,
};

enum session_type {
	MSM_VIDC_ENCODER = 0,
	MSM_VIDC_DECODER,
	MSM_VIDC_MAX_DEVICES,
};

int msm_vidc_open(void *vidc_inst, int core_id, int session_type);
int msm_vidc_close(void *instance);
int msm_vidc_querycap(void *instance, struct v4l2_capability *cap);
int msm_vidc_enum_fmt(void *instance, struct v4l2_fmtdesc *f);
int msm_vidc_s_fmt(void *instance, struct v4l2_format *f);
int msm_vidc_g_fmt(void *instance, struct v4l2_format *f);
int msm_vidc_s_ctrl(void *instance, struct v4l2_control *a);
int msm_vidc_g_ctrl(void *instance, struct v4l2_control *a);
int msm_vidc_reqbufs(void *instance, struct v4l2_requestbuffers *b);
int msm_vidc_prepare_buf(void *instance, struct v4l2_buffer *b);
int msm_vidc_release_buf(void *instance, struct v4l2_buffer *b);
int msm_vidc_qbuf(void *instance, struct v4l2_buffer *b);
int msm_vidc_dqbuf(void *instance, struct v4l2_buffer *b);
int msm_vidc_streamon(void *instance, enum v4l2_buf_type i);
int msm_vidc_streamoff(void *instance, enum v4l2_buf_type i);
int msm_vidc_decoder_cmd(void *instance, struct v4l2_decoder_cmd *dec);
int msm_vidc_poll(void *instance, struct file *filp,
		struct poll_table_struct *pt);
#endif
