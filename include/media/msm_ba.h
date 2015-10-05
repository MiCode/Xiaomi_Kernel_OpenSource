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

#ifndef _MSM_BA_H_
#define _MSM_BA_H_

#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <linux/poll.h>

enum msm_ba_ip {
	BA_IP_CVBS_0 = 0,
	BA_IP_CVBS_1,
	BA_IP_CVBS_2,
	BA_IP_CVBS_3,
	BA_IP_CVBS_4,
	BA_IP_CVBS_5,
	BA_IP_SVIDEO_0,
	BA_IP_SVIDEO_1,
	BA_IP_SVIDEO_2,
	BA_IP_COMPONENT_0,
	BA_IP_COMPONENT_1,
	BA_IP_DVI_0,
	BA_IP_DVI_1,
	BA_IP_HDMI_1,
	BA_IP_MHL_1,
	BA_IP_TTL,
	BA_IP_MAX = 0xffffffff
};

enum msm_ba_save_restore_ip {
	BA_SR_RESTORE_IP = 0,
	BA_SR_SAVE_IP,
	BA_SR_MAX = 0xffffffff
};

struct msm_ba_ext_ops {
	void (*msm_ba_cb)(void *instance,
		unsigned int event_id, void *arg);
};

void *msm_ba_open(const struct msm_ba_ext_ops *ext_ops);
int msm_ba_close(void *instance);
int msm_ba_querycap(void *instance, struct v4l2_capability *cap);
int msm_ba_g_priority(void *instance, enum v4l2_priority *prio);
int msm_ba_s_priority(void *instance, enum v4l2_priority prio);
int msm_ba_enum_input(void *instance, struct v4l2_input *input);
int msm_ba_g_input(void *instance, unsigned int *index);
int msm_ba_s_input(void *instance, unsigned int index);
int msm_ba_enum_output(void *instance, struct v4l2_output *output);
int msm_ba_g_output(void *instance, unsigned int *index);
int msm_ba_s_output(void *instance, unsigned int index);
int msm_ba_enum_fmt(void *instance, struct v4l2_fmtdesc *f);
int msm_ba_s_fmt(void *instance, struct v4l2_format *f);
int msm_ba_g_fmt(void *instance, struct v4l2_format *f);
int msm_ba_s_ctrl(void *instance, struct v4l2_control *a);
int msm_ba_s_ext_ctrl(void *instance, struct v4l2_ext_controls *a);
int msm_ba_g_ctrl(void *instance, struct v4l2_control *a);
int msm_ba_streamon(void *instance, enum v4l2_buf_type i);
int msm_ba_streamoff(void *instance, enum v4l2_buf_type i);
int msm_ba_save_restore_input(void *instance, enum msm_ba_save_restore_ip sr);
int msm_ba_poll(void *instance, struct file *filp,
		struct poll_table_struct *pt);
int msm_ba_subscribe_event(void *instance,
				const struct v4l2_event_subscription *sub);
int msm_ba_unsubscribe_event(void *instance,
				const struct v4l2_event_subscription *sub);
int msm_ba_s_parm(void *instance, struct v4l2_streamparm *a);
int msm_ba_register_subdev_node(struct v4l2_subdev *sd);
int msm_ba_unregister_subdev_node(struct v4l2_subdev *sd);
#endif
