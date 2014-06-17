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

#ifndef _H_VPU_V4L2_H_
#define _H_VPU_V4L2_H_

#include <linux/types.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ctrls.h>
#include <media/videobuf2-core.h>

#include <uapi/media/msm_vpu.h>
#include "vpu_resources.h"
#include "vpu_property.h"


#define VPU_DRV_NAME "msm_vpu"

#define VPU_NUM_SESSIONS 2
#define VPU_MAX_CLIENTS_PER_SESSION 4
#define VPU_MAX_CLIENTS 20

#define VPU_EVENT_Q_SIZE 2

#define get_vpu_client(val)  container_of(val, struct vpu_client, vfh)


enum vpu_session_port_types {
	PORT_TYPE_INPUT = 0,
	PORT_TYPE_OUTPUT,
	NUM_VPU_PORT_TYPES
};

enum vpu_session_ports {
	INPUT_PORT = 0,
	OUTPUT_PORT,
	OUTPUT_PORT2,
	NUM_VPU_PORTS
};

enum vpu_port_streaming_flags {
	INPUT_STREAMING = (0x1 << PORT_TYPE_INPUT),
	OUTPUT_STREAMING = (0x1 << PORT_TYPE_OUTPUT),
	ALL_STREAMING = (0x1 << NUM_VPU_PORT_TYPES) - 1,
};

#define COMMITED 1

enum vpu_client_type {
	VPU_USERSPACE_CLIENT = 0,
	VPU_KERNEL_SUBDEV_CLIENT,
};

struct vpu_dev_session;

struct vpu_port_ops {
	/* called when the port is set/cleared by user space */
	int  (*attach)(struct vpu_dev_session *session, int port, void *priv);
	void (*detach)(struct vpu_dev_session *session, int port, void *priv);

	/* called when reqbufs/qbuf is called on that port */
	int  (*set_buf_num)(struct vpu_dev_session *session, int port,
			u32 buf_num, void *priv);
	int  (*set_buf)(struct vpu_dev_session *session, int port,
			struct v4l2_buffer *v4l2_buf, void *priv);

	/* called when streamon/streamoff is called on that port */
	int  (*streamon)(struct vpu_dev_session *session, int port, void *priv);
	void (*streamoff)(struct vpu_dev_session *session, int port,
								void *priv);
	void *priv;
};

struct vpu_port_info {
	struct v4l2_pix_format_mplane format;
	struct v4l2_rect roi;
	u32 scan_mode;
	u32 framerate;
	u32 video_fmt;
	u32 secure_content;

	/* VPU source/destination info, plus data pipe indexes if applicable */
	union {
		u32 source;
		u32 destination;
	};

	struct vpu_port_ops port_ops;
};

/* core device structure */
struct vpu_dev_core {
	struct device    *dev;
	/* v4l2 device parent structure */
	struct v4l2_device v4l2_dev;
	/* One device node represents multiple sessions */
	struct video_device vdev;

	 /* Array of virtual VPU sessions */
	struct vpu_dev_session *sessions[VPU_NUM_SESSIONS];

	struct list_head  unattached_list;
	int global_client_count;
	struct mutex lock; /* protects global client list and count */

	/* Stores Device Tree resources */
	struct vpu_platform_resources resources;

	struct dentry *debugfs_root; /* VPU debugfs root directory */
};

/* struct to represent a virtual VPU session */
struct vpu_dev_session {
	/* session ID (from 0 to VPU_NUM_SESSIONS-1) */
	u32 id;
	struct vpu_dev_core *core;

	/*
	 * Session ports information.
	 * The mutexes independently protect the queues for each port.
	 * vbqueue: the queue management struct.
	 * pending_list: tracks queued buffers pending all sessions streamon
	 * io_client: indicates the client that owns access to a queue.
	 */
	struct mutex      que_lock[NUM_VPU_PORTS];
	struct vb2_queue  vbqueue[NUM_VPU_PORTS];
	struct list_head  pending_list[NUM_VPU_PORTS];
	struct vpu_client *io_client[NUM_VPU_PORTS];

	/* Protects global session data (all variables below) */
	struct mutex lock;

	/* tracks clients attached to the session */
	struct list_head  clients_list;
	int               client_count;

	/* end-to-end session streaming status */
	u32 streaming_state;

	/* tracks session configuration commit status */
	u32 commit_state;

	/* port configurations cache */
	struct vpu_port_info port_info[NUM_VPU_PORTS];

	/* controls handler */
	struct vpu_controller *controller;

	/* load in kilo-bits per second (kbps)*/
	u32 load;

	/* session has dual outputs */
	bool dual_output;
};

/* client specific data struct */
struct vpu_client {
	struct v4l2_fh  vfh; /* client events handling */

	enum vpu_client_type  type; /* kernel or userpsace client */

	/* entry in core or a session's list of clients */
	struct list_head  clients_entry;

	struct vpu_dev_core *core; /* pointer to core struct */

	/* If client is attached, points to attached session */
	struct vpu_dev_session  *session;

	bool uses_output2; /* client uses second output port */
};


/* Buffer type to port index / port type conversions */
#define get_port_type(type)     \
	(((type) == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)  ? PORT_TYPE_INPUT  : \
	(((type) == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) ? PORT_TYPE_OUTPUT : \
							-1))

static inline int get_port_number(struct vpu_client *client,
		enum v4l2_buf_type type)
{
	int port_type = get_port_type(type);

	if (!client || port_type < 0)
		return -EINVAL;
	else if (port_type == PORT_TYPE_INPUT)
		return INPUT_PORT;
	else if (port_type == PORT_TYPE_OUTPUT && !client->uses_output2)
		return OUTPUT_PORT;
	else
		return OUTPUT_PORT2;
}

static inline int get_queue_port_number(struct vb2_queue *vbq)
{
	struct vpu_dev_session *session = vb2_get_drv_priv(vbq);

	if (vbq == &session->vbqueue[INPUT_PORT])
		return INPUT_PORT;
	else if (vbq == &session->vbqueue[OUTPUT_PORT])
		return OUTPUT_PORT;
	else if (vbq == &session->vbqueue[OUTPUT_PORT2])
		return OUTPUT_PORT2;
	else
		return -EINVAL;
}


#endif /* _H_VPU_V4L2_H_ */

