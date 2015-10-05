/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_BA_COMMON_H_
#define _MSM_BA_COMMON_H_

#include "msm_ba_internal.h"

#define BA_IS_PRIV_CTRL(idx) (\
		(V4L2_CTRL_ID2CLASS(idx) == V4L2_CTRL_CLASS_USER) && \
		V4L2_CTRL_DRIVER_PRIV(idx))

struct msm_ba_dev *get_ba_dev(void);
void msm_ba_queue_v4l2_event(struct msm_ba_inst *inst,
		const struct v4l2_event *sd_event);
struct v4l2_subdev *msm_ba_sd_find(const char *name);
void msm_ba_add_inputs(struct v4l2_subdev *sd);
void msm_ba_del_inputs(struct v4l2_subdev *sd);
void msm_ba_set_out_in_use(struct v4l2_subdev *sd, int on);
int msm_ba_find_ip_in_use_from_sd(struct v4l2_subdev *sd);
void msm_ba_reset_ip_in_use_from_sd(struct v4l2_subdev *sd);
struct msm_ba_input *msm_ba_find_input_from_sd(struct v4l2_subdev *sd,
		int bridge_chip_ip);
struct msm_ba_input *msm_ba_find_input(int ba_input_idx);
struct msm_ba_input *msm_ba_find_output(int ba_output);
int msm_ba_g_fps(void *instance, int *fps_q16);
int msm_ba_ctrl_init(struct msm_ba_inst *inst);
void msm_ba_ctrl_deinit(struct msm_ba_inst *inst);

#endif
