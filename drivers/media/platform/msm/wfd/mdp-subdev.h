/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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

/* Converts a dma_addr_t (which _might_ be 64 bits) to a 32 bit value.
 * This is required because hardware can only access 32 bit addresses,
 * and more importantly, the v4l2 spec only allows for 32 bit addresses.
 * However in the off chance that we actually manage to get an address
 * that can't fit into 32 bits, this macro triggers a WARN_ON and returns
 * the truncated 32 bit address */
static inline u32 dma_addr_to_u32(dma_addr_t x)
{
	u32 temp = (u32)x;
	WARN_ON((dma_addr_t)temp != x);
	return temp;
}

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

struct mdp_msg_ops {
	void *cookie;
	bool secure;
	bool iommu_split_domain;
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
#define MDP_MMAP  _IOR(MDP_MAGIC_IOCTL, 8, struct mem_region_map *)
#define MDP_MUNMAP  _IOR(MDP_MAGIC_IOCTL, 9, struct mem_region_map *)
#define MDP_SECURE  _IO(MDP_MAGIC_IOCTL, 9)


#ifdef CONFIG_FB_MSM_MDSS_WRITEBACK
extern int mdp_init(struct v4l2_subdev *sd, u32 val);
extern long mdp_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg);
#else
static inline int mdp_init(struct v4l2_subdev *sd, u32 val)
{
	return -ENODEV;
}
static inline long mdp_ioctl(struct v4l2_subdev *sd, unsigned int cmd,
			void *arg)
{
	return -ENODEV;
}
#endif


#endif /* _WFD_MDP_SUBDEV_ */
