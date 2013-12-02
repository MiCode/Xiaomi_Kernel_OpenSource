/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#ifndef _H_VPU_CONFIGURATION_H_
#define _H_VPU_CONFIGURATION_H_

#include <linux/videodev2.h>
#include <uapi/media/msm_vpu.h>
#include "vpu_v4l2.h"


/*
 * VPU private controls
 */
#define NUM_NR_BUFFERS 2

struct vpu_controller {
	/* controls cache */
	u32 cache_size;
	void *cache;

	/* Control specific metadata */
	void *nr_buffers[NUM_NR_BUFFERS];
};

void setup_vpu_controls(void);

struct vpu_controller *init_vpu_controller(struct vpu_platform_resources *res);

void deinit_vpu_controller(struct vpu_controller *controller);

int apply_vpu_control(struct vpu_dev_session *session, int cmd,
		struct vpu_control *control);

int apply_vpu_control_extended(struct vpu_client *client, int cmd,
		struct vpu_control_extended *control);

void *get_control(struct vpu_controller *controller, u32 id);

int configure_colorspace(struct vpu_dev_session *session, int port);

int configure_nr_buffers(struct vpu_dev_session *session,
		const struct vpu_ctrl_auto_manual *nr);

/**
 * commit() - Configuration commit functions.
 * @session:	Session pointer
 * @new_load:	Port number.
 * @new_load:	Indicates that session load needs to be recalculated.

 * commit_initial_config is used for initial session configuration commit.
 * commit_port_config: sets and commits a specific port config in runtime
 * commit_contrl: Commits a control change in runtime
 */
int commit_initial_config(struct vpu_dev_session *session);

int commit_port_config(struct vpu_dev_session *session, int port, int new_load);

int commit_control(struct vpu_dev_session *session, int new_load);


/*
 * Port configuration
 */
struct vpu_format_desc {
	u8 description[32];
	u32 fourcc;
	int num_planes;
	struct {
		int bitsperpixel;
		int heightfactor;
	} plane[VPU_MAX_PLANES];
};

const struct vpu_format_desc *query_supported_formats(int index);

u32 get_bytesperline(u32 width, u32 bitsperpixel, u32 input_bytesperline,
		u32 pixelformat);

static inline u32 get_sizeimage(u32 bytesperline, u32 height, u32 heightfactor)
{
	return bytesperline * height / heightfactor;
}

int is_format_valid(struct v4l2_format *fmt);

#endif /* _H_VPU_CONFIGURATION_H_ */
