/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#ifndef _WFD_MDP_SUBDEV_
#define _WFD_MDP_SUBDEV_

#include <linux/videodev2.h>
#include <media/v4l2-subdev.h>

#define MDP_MAGIC_IOCTL 'M'

struct mdp_buf_info {
	void *inst;
	void *cookie;
	u32 fd;
	u32 offset;
	u32 kvaddr;
	u32 paddr;
};

struct mdp_prop {
	void *inst;
	u32 height;
	u32 width;
};

static inline bool mdp_buf_info_equals(struct mdp_buf_info *a,
		struct mdp_buf_info *b)
{
	return a->inst == b->inst
		&& a->fd == b->fd
		&& a->offset == b->offset
		&& a->kvaddr == b->kvaddr
		&& a->paddr == b->paddr;
}

#define MDP_Q_BUFFER  _IOW(MDP_MAGIC_IOCTL, 1, struct mdp_buf_info *)
#define MDP_DQ_BUFFER  _IOR(MDP_MAGIC_IOCTL, 2, struct mdp_out_buf *)
#define MDP_OPEN  _IOR(MDP_MAGIC_IOCTL, 3, void **)
#define MDP_SET_PROP  _IOW(MDP_MAGIC_IOCTL, 4, struct mdp_prop *)
#define MDP_CLOSE  _IOR(MDP_MAGIC_IOCTL, 5, void *)
#define MDP_START  _IOR(MDP_MAGIC_IOCTL, 6, void *)
#define MDP_STOP  _IOR(MDP_MAGIC_IOCTL, 7, void *)
extern int mdp_init(struct v4l2_subdev *sd, u32 val);
extern long mdp_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg);


#endif /* _WFD_MDP_SUBDEV_ */
