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

#ifndef _WFD_VSG_SUBDEV_
#define _WFD_VSG_SUBDEV_

#include <linux/videodev2.h>
#include <linux/list.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <media/v4l2-subdev.h>
#include "mdp-subdev.h"

#define VSG_MAGIC_IOCTL 'V'

enum vsg_flags {
	VSG_NEVER_RELEASE = 1<<0,
	VSG_NEVER_SET_LAST_BUFFER = 1<<1,
	VSG_BUF_BEING_ENCODED = 1<<2,
};

enum vsg_modes {
	VSG_MODE_CFR,
	VSG_MODE_VFR,
};

enum vsg_states {
	VSG_STATE_NONE,
	VSG_STATE_STARTED,
	VSG_STATE_STOPPED,
	VSG_STATE_ERROR
};

struct vsg_buf_info {
	struct mdp_buf_info mdp_buf_info;
	struct timespec time;
	/* Internal */
	struct list_head node;
	uint32_t flags;
};

struct vsg_msg_ops {
	void *cbdata;
	int (*encode_frame)(void *cbdata, struct vsg_buf_info *buffer);
	int (*release_input_frame)(void *cbdata, struct vsg_buf_info *buffer);
};

struct vsg_context {
	struct vsg_buf_info	free_queue, busy_queue;
	struct vsg_msg_ops vmops;
	/* All time related values below in nanosecs */
	int64_t frame_interval, max_frame_interval;
	struct workqueue_struct *work_queue;
	struct hrtimer threshold_timer;
	struct mutex mutex;
	struct vsg_buf_info *last_buffer;
	int mode;
	int state;
};

struct vsg_work {
	struct vsg_context *context;
	struct work_struct work;
};

struct vsg_encode_work {
	struct vsg_buf_info *buf;
	struct vsg_context *context;
	struct work_struct work;
};

#define VSG_OPEN  _IO(VSG_MAGIC_IOCTL, 1)
#define VSG_CLOSE  _IO(VSG_MAGIC_IOCTL, 2)
#define VSG_START  _IO(VSG_MAGIC_IOCTL, 3)
#define VSG_STOP  _IO(VSG_MAGIC_IOCTL, 4)
#define VSG_Q_BUFFER  _IOW(VSG_MAGIC_IOCTL, 5, struct vsg_buf_info *)
#define VSG_DQ_BUFFER  _IOR(VSG_MAGIC_IOCTL, 6, struct vsg_out_buf *)
#define VSG_RETURN_IP_BUFFER _IOW(VSG_MAGIC_IOCTL, 7, struct vsg_buf_info *)
#define VSG_ENCODE_DONE _IO(VSG_MAGIC_IOCTL, 8)
/* Time related arguments for frame interval ioctls are always in nanosecs*/
#define VSG_SET_FRAME_INTERVAL _IOW(VSG_MAGIC_IOCTL, 9, int64_t *)
#define VSG_GET_FRAME_INTERVAL _IOR(VSG_MAGIC_IOCTL, 10, int64_t *)
#define VSG_SET_MAX_FRAME_INTERVAL _IOW(VSG_MAGIC_IOCTL, 11, int64_t *)
#define VSG_GET_MAX_FRAME_INTERVAL _IOR(VSG_MAGIC_IOCTL, 12, int64_t *)
#define VSG_SET_MODE _IOW(VSG_MAGIC_IOCTL, 13, enum vsg_modes *)

extern int vsg_init(struct v4l2_subdev *sd, u32 val);
extern long vsg_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg);

#endif /* _WFD_VSG_SUBDEV_ */
