/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef _H_VPU_IOCTL_INTERNAL_H_
#define _H_VPU_IOCTL_INTERNAL_H_

#include "vpu_v4l2.h"

/*
 * VPU API IOCTLs
 */
int vpu_open_user_client(struct file *file);

struct vpu_client *vpu_open_kernel_client(struct vpu_dev_core *core);

int vpu_close_client(struct vpu_client *client);


static inline int get_vpu_num_sessions(unsigned *ret)
{
	*ret = VPU_NUM_SESSIONS;
	return 0;
}

int vpu_attach_client(struct vpu_client *client, int session_num);
void vpu_detach_client(struct vpu_client *client);


int vpu_enum_fmt(struct v4l2_fmtdesc *f);
int vpu_get_fmt(struct vpu_client *client, struct v4l2_format *f);
int vpu_set_fmt(struct vpu_client *client, struct v4l2_format *f);
int vpu_try_fmt(struct vpu_client *client, struct v4l2_format *f);

int vpu_get_region_of_intereset(struct vpu_client *client,
		struct v4l2_crop *c);
int vpu_set_region_of_intereset(struct vpu_client *client,
		const struct v4l2_crop *c);

int vpu_get_input(struct vpu_client *client, unsigned int *i);
int vpu_set_input(struct vpu_client *client, unsigned int i);

int vpu_get_output(struct vpu_client *client, unsigned int *i);
int vpu_set_output(struct vpu_client *client, unsigned int i);

int vpu_get_control(struct vpu_client *client, struct vpu_control *control);
int vpu_set_control(struct vpu_client *client, struct vpu_control *control);

int vpu_get_control_extended(struct vpu_client *client,
		struct vpu_control_extended *control);
int vpu_set_control_extended(struct vpu_client *client,
		struct vpu_control_extended *control);

int vpu_get_control_port(struct vpu_client *client,
		struct vpu_control_port *control);
int vpu_set_control_port(struct vpu_client *client,
		struct vpu_control_port *control);

int vpu_commit_configuration(struct vpu_client *client);


int vpu_reqbufs(struct vpu_client *client, struct v4l2_requestbuffers *rb);
int vpu_qbuf(struct vpu_client *client, struct v4l2_buffer *b);
int vpu_dqbuf(struct vpu_client *client, struct v4l2_buffer *b,
		bool nonblocking);

int vpu_flush_bufs(struct vpu_client *client, enum v4l2_buf_type i);

int vpu_streamon(struct vpu_client *client, enum v4l2_buf_type i);
int vpu_streamoff(struct vpu_client *client, enum v4l2_buf_type i);

int vpu_trigger_stream(struct vpu_dev_session *session);

/*
 * Notify client with event of ID type.
 * If data is not null then it points to payload of size (<=64)
 */
void notify_vpu_event_client(struct vpu_client *client,
		u32 type, u8 *data, u32 size);

/*
 * Notify All clients in session with event of ID type.
 * If data is not null then it points to payload of size (<=64)
 */
void notify_vpu_event_session(struct vpu_dev_session *session,
		u32 type, u8 *data, u32 size);

/*
 * Videobuf2 related functions
 */
int vpu_vb2_queue_init(struct vb2_queue *q, enum v4l2_buf_type type,
		void *pdata);

int vpu_vb2_flush_buffers(struct vpu_dev_session *session, int port);

#endif /* _H_VPU_IOCTL_INTERNAL_H_ */

