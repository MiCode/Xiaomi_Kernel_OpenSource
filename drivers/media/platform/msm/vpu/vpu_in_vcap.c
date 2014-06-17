/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"VPU, %s: " fmt, __func__

#include <linux/types.h>
#include <linux/mutex.h>
#include <media/vcap_v2.h>

#include "vpu_v4l2.h"
#include "vpu_channel.h"
#include "vpu_ioctl_internal.h"

#define VPU_IN_MIN_BUFFERS	4
#define VPU_IN_MAX_BUFFERS	8

/*
 * struct vpu_in_port_hnd - VPU input tunneling structure definition
 * VCAP source information saved for notification purpose (event callback)
 * @session:		associated tunneling session
 * @port:		session port, see: enum vpu_session_ports
 * @start_lock:		protects @start_req (set according to @buf_count value),
 *			which can be accessed by a vcap call and a user space
 *			call (avoid race condition)
 * @start_req:		synchronizes a streamon req with every buffer reception
 * @buf_num:		number of input tunneling buffers
 * @buf_array:		array of buffers to keep track of them (for unmapping)
 * @vcap_channel:	VCAP physical channel (A or B)
 * @vcap_ctx:		VCAP private data
 * @vcap_ops:		VCAP callback function for event handler
 */
struct vpu_in_port_hnd {
	struct vpu_dev_session     *session;
	int                         port;

	struct mutex                start_lock; /* protects 2 variables below */
	bool                        start_req;
	u32                         buf_count;

	u32                         buf_num;
	struct vpu_buffer          *buf_array;

	enum vcap_ch                vcap_channel;
	void                       *vcap_ctx;
	struct vcap_tunnel_src_ops *vcap_ops;
};

static void vpu_deinit_port_vcap(struct vpu_dev_session *session,
				 struct vpu_port_info *port_info);

/*
 * Helper functions
 */

/*
 * translate_input_src_to_tunnel_channel()
 * @input_source:	see: enum vpu_session_input_source
 *
 * Return:	enum vcap_ch
 */
static u32 translate_input_src_to_tunnel_channel(u32 input_source)
{
	u32 vcap_channel = 0; /* invalid */

	if ((input_source & VPU_PIPE_VCAP0) && (input_source & VPU_PIPE_VCAP1))
		vcap_channel = VCAP_CH_AB;
	else if (input_source & VPU_PIPE_VCAP0)
		vcap_channel = VCAP_CH_A;
	else if (input_source & VPU_PIPE_VCAP1)
		vcap_channel = VCAP_CH_B;
	else
		pr_warn("Unsupported tunnel source (%d)\n", input_source);

	return vcap_channel;
}

/*
 * translate_tunnel_buffer_to_hfi()
 * @vcap_buf:	VCAP tunnel buffer
 * @vpu_buf:	translated VPU driver's HFI buffer
 *
 * Store a maximum of info in vpu_buffer, the rest in vb2_buf and v4l2_buf
 *
 * Note:
 * fd will be used later to map the VPU address (in the same thread)
 *
 * Return:	<void>
 */
static void translate_tunnel_buffer_to_hfi(
		const struct vcap_tunnel_buffer_info *vcap_buf,
		struct vpu_buffer *vpu_buf)
{
	int i;

	for (i = 0; i < vcap_buf->num_planes; i++) {
		struct vpu_plane *vpu_plane = &vpu_buf->planes[i];
		const struct vcap_tunnel_buf_plane_info *vcap_plane =
							&vcap_buf->planes[i];
		vpu_plane->new_plane = 1;
		vpu_plane->fd = vcap_plane->fd;
		vpu_plane->length = vcap_plane->size;
		vpu_plane->data_offset = vcap_plane->offset;
		vpu_plane->mapped_address[ADDR_INDEX_VCAP] =
						vcap_plane->vcap_phy_addr;
	}
	vpu_buf->vb.v4l2_buf.index = vcap_buf->index;
	vpu_buf->vb.num_planes = vcap_buf->num_planes;
}

/*
 * vpu_unmap_buf() - unmap a buffer from VPU address space
 * @vpu_buf:	translated buffer received from VCAP (contains memory info)
 *
 * Return:	<void>
 */
static void vpu_unmap_buf(struct vpu_buffer *vpu_buf)
{
	int i;
	struct vb2_buffer *vb = &vpu_buf->vb;

	pr_debug("unmap buffer #%d from iommu\n", vb->v4l2_buf.index);

	for (i = 0; i < vb->num_planes; i++) {
		struct vpu_plane *vp = &vpu_buf->planes[i];

		if (vp->mem_cookie)
			vpu_mem_destroy_handle(vp->mem_cookie);
		memset(vp, 0, sizeof(*vp));
	}
}

/*
 * vpu_in_vcap_map_buf() - map a buffer in VPU address space
 * @session:	associated tunneling session
 * @vpu_buf:	translated buffer received from VCAP (contains memory info)
 * @port:	see enum vpu_session_ports
 *
 * Note:
 * vpu_buf->planes[i].user_fd has to be consumed in the same thread as the
 * caller of this function.
 *
 * Return:	-ENOMEM, if iommu buffer handler creation failed
 *		-EFAULT, if the VCAP mappes address is unknown
 *		0, otherwise.
 */
static int vpu_in_vcap_map_buf(struct vpu_dev_session *session,
			struct vpu_buffer *vpu_buf, int port)
{
	int i, ret = 0;
	struct vb2_buffer *vb = &vpu_buf->vb;
	bool secured;

	secured = session->port_info[port].secure_content ? true : false;

	for (i = 0; i < vb->num_planes; i++) {
		struct vpu_plane *plane = &vpu_buf->planes[i];

		if (!plane->new_plane)
			continue;
		plane->new_plane = 0;

		if (!plane->mem_cookie) {
			plane->mem_cookie = vpu_mem_create_handle(
					session->core->resources.mem_client);
			if (!plane->mem_cookie) {
				ret = -ENOMEM;
				goto err_buf_init;
			}
		}

		/* map VPU address using fd passed in through tunnel */
		ret = vpu_mem_map_fd(plane->mem_cookie, plane->fd,
				plane->length, plane->data_offset, secured);
		if (ret) {
			vpu_buf->valid_addresses_mask &= ~ADDR_VALID_VPU;
			goto err_buf_init;
		} else {
			plane->mapped_address[ADDR_INDEX_VPU] =
				vpu_mem_addr(plane->mem_cookie, MEM_VPU_ID);
			vpu_buf->valid_addresses_mask |= ADDR_VALID_VPU;
			pr_debug("VPU mapped addr = 0x%08x\n",
					plane->mapped_address[ADDR_INDEX_VPU]);
		}

		/* validate VCAP mapped addresses */
		vpu_buf->valid_addresses_mask |= ADDR_VALID_VCAP;
		pr_debug("VCAP mapped addr = 0x%08x\n",
					plane->mapped_address[ADDR_INDEX_VCAP]);
	}
	return 0;

err_buf_init:
	vpu_unmap_buf(vpu_buf);
	return ret;
}

/*
 * vpu_in_unmap_buffers() - unmap all tunnel buffers
 * @session:	associated tunneling session
 * @i_port_hnd:	pointer to the output port control handler
 *
 * Return:	<void>
 */
static void vpu_in_unmap_buffers(struct vpu_dev_session *session,
				struct vpu_in_port_hnd *i_port_hnd)
{
	int buf;

	if (!session || !i_port_hnd) {
		pr_err("Null pointer\n");
		return;
	}
	if (!i_port_hnd->buf_array || !i_port_hnd->buf_num) {
		pr_debug("No buffers\n");
		return;
	}

	BUG_ON(session->streaming_state & INPUT_STREAMING);

	for (buf = 0; buf < i_port_hnd->buf_num; buf++)
		vpu_unmap_buf(&i_port_hnd->buf_array[buf]);
}

/*
 * VCAP -> VPU commands
 */

/*
 * vcap2vpu_set_buffer_num() - create an array of vpu_buffers[@buf_num] in
 * tunneling session struct for input port
 * @sink_ctx:	VPU context (current tunneling session)
 * @buf_num:	number of buffers that will be received from VCAP
 *
 * Tunnel buffer info will be kept in that array for set/unmap buffers
 *
 * Return:	-EINVAL, if a null pointer is received in parameter
 *		or if the number of buffers is incorrect
 *		-ENOMEM, if the buffer array memory allocation failed
 *		0, otherwise
 */
static int vcap2vpu_set_buffer_num(void *sink_ctx, u32 buf_num)
{
	struct vpu_in_port_hnd *i_port_hnd = (struct vpu_in_port_hnd *)sink_ctx;
	struct vpu_dev_session *session;
	struct vpu_buffer *buf_array_alloc = NULL;

	if (!i_port_hnd) {
		pr_err("Null i_port_hnd pointer\n");
		return -EINVAL;
	}
	session = i_port_hnd->session;
	if (!session) {
		pr_err("Null session pointer\n");
		return -EINVAL;
	}
	if (buf_num == 0) {
		pr_debug("%d buffers requested\n", buf_num);
		return 0;
	} else if ((buf_num < VPU_IN_MIN_BUFFERS) ||
		   (buf_num > VPU_IN_MAX_BUFFERS)) {
		pr_err("Cannot set %d buffers\n", buf_num);
		return -EINVAL;
	}

	pr_debug("%d buffers used for tunneling on input port for session %d\n",
			buf_num, session->id);

	buf_array_alloc = devm_kzalloc(session->core->dev,
			buf_num * sizeof(struct vpu_buffer), GFP_KERNEL);
	if (!buf_array_alloc) {
		pr_err("Cannot allocate memory\n");
		return -ENOMEM;
	}

	i_port_hnd->buf_array = buf_array_alloc;
	i_port_hnd->buf_num = buf_num;
	i_port_hnd->buf_count = 0;

	return 0;
}

/*
 * vcap2vpu_set_src_buffer() - handle buffer received from vcap
 * @sink_ctx:	VPU context (current tunneling session)
 * @vcap_buf:	VCAP buffer, to be translated into a VPU buffer
 *
 * Map received buffer for VPU address space and keep it into an array.
 * Buffers will be sent all at once to VPU HW later (before session start)
 *
 * Note:
 * vcap_buf->fd has to be consumed in the same thread as the caller of this
 * function.
 *
 * Return:	-EINVAL, if a null pointer is received in parameter
 *		-EFAULT, if the VCAP mapped address is unknown
 *		-EACCES, if vcap has not set the number of buffers first.
 *		-ESPIPE, if buffer index is too big
 *		0, otherwise
 */
static int vcap2vpu_set_src_buffer(void *sink_ctx,
		struct vcap_tunnel_buffer_info *vcap_buf)
{
	struct vpu_in_port_hnd *i_port_hnd = (struct vpu_in_port_hnd *)sink_ctx;
	struct vpu_dev_session *session;
	struct vpu_buffer vpu_buf;
	struct v4l2_buffer *v4l2_buf = &vpu_buf.vb.v4l2_buf;
	int ret = 0, port;

	if (!vcap_buf) {
		pr_err("Null buffer pointer\n");
		return -EINVAL;
	}
	if (!i_port_hnd) {
		pr_err("Null i_port_hnd pointer\n");
		return -EINVAL;
	}

	session = i_port_hnd->session;
	port = i_port_hnd->port;
	if (!session) {
		pr_err("Null session pointer\n");
		return -EINVAL;
	}
	if (i_port_hnd->buf_array == NULL) {
		pr_err("Set tunnel buffer number first.\n");
		return -EACCES;
	}
	if (vcap_buf->index >= i_port_hnd->buf_num) {
		pr_err("Wrong buffer index (%d) received on port %d\n",
				vcap_buf->index, port);
		return -ESPIPE;
	}
	if (i_port_hnd->buf_count >= i_port_hnd->buf_num) {
		pr_err("Too many buffers (%d/%d) received on port %d\n",
			i_port_hnd->buf_count, i_port_hnd->buf_num, port);
		return -EINVAL;
	}

	memset(&vpu_buf, 0, sizeof(vpu_buf));
	translate_tunnel_buffer_to_hfi(vcap_buf, &vpu_buf);

	pr_debug("Received tunnel buffer #%d on port %d\n",
				v4l2_buf->index, port);

	ret = vpu_in_vcap_map_buf(session, &vpu_buf, port);
	if (unlikely(ret)) {
		pr_err("Cannot map buffer #%d\n", v4l2_buf->index);
	} else {
		memcpy(&i_port_hnd->buf_array[v4l2_buf->index],
				 &vpu_buf, sizeof(vpu_buf));
	}

	mutex_lock(&i_port_hnd->start_lock);
	i_port_hnd->buf_count++;

	/*
	 * If delayed, start the end-to-end session streaming
	 * see vpu_in_vcap_streamon()
	 */
	if (i_port_hnd->buf_count != i_port_hnd->buf_num) {
		pr_debug("Waiting for %d more buffer(s)\n",
				i_port_hnd->buf_num - i_port_hnd->buf_count);
	} else if (i_port_hnd->start_req) {
		pr_debug("Last buffer received, streamon requested, start\n");

		ret = vpu_hw_session_register_buffers(session->id,
				translate_port_id(INPUT_PORT),
				i_port_hnd->buf_array, i_port_hnd->buf_num);
		if (ret) {
			pr_err("Register buffer failed (error %d)\n", ret);
			goto exit_set_src_buffer;
		}

		ret = vpu_trigger_stream(session);
		if (ret) {
			pr_err("channel start failed\n");
			goto exit_set_src_buffer;
		}

		i_port_hnd->start_req = false;
		pr_debug("Session streaming started successfully\n");
	}

exit_set_src_buffer:
	mutex_unlock(&i_port_hnd->start_lock);
	return ret;
}

/*
 * VPU tunnel input ops
 */

/*
 * vpu_in_vcap_streamoff() - sends command to VPU HW, through HFI layer, to
 * release the input tunnel buffers
 * @session:	associated tunneling session
 * @port:	see: enum vpu_session_ports
 * @priv:	pointer to the input port control handler
 *
 * Input tunnel (vcap) is notified once done.
 *
 * Return:	<void>
 */
static void vpu_in_vcap_streamoff(struct vpu_dev_session *session,
		int port, void *priv)
{
	struct vpu_in_port_hnd *i_port_hnd = priv;

	BUG_ON(!session);
	BUG_ON(!i_port_hnd);
	BUG_ON(!i_port_hnd->vcap_ops);
	BUG_ON(session != i_port_hnd->session);
	BUG_ON(session->streaming_state == ALL_STREAMING);

	if (vpu_hw_session_release_buffers(session->id,
			translate_port_id(INPUT_PORT), CH_RELEASE_IN_BUF))
		pr_err("Release buffer failed\n");

	/* notify VCAP*/
	i_port_hnd->vcap_ops->notify(i_port_hnd->vcap_ctx,
					NOTIFY_ID_SINK_PUT_BUFFER, NULL);

	pr_debug("Tunnel buffers released\n");
}

/*
 * vpu_in_vcap_streamon() - sends all the tunneling buffers to VPU HW, through
 * HFI layer, using the array of received tunneling buffers.
 * @session:	associated tunneling session
 * @port:	see: enum vpu_session_ports
 * @priv:	pointer to the input port control handler
 *
 * This function needs to be called after commit and before start commands.
 *
 * Return:	-EINVAL, if a null pointer is received in parameter
 *		-EBUSY, if a session is already connected to that port
 *		-EACCES, if the session is currently streaming
 *		-EAGAIN, if not all buffers are received from VCAP
 *		0, otherwise.
 */
static int vpu_in_vcap_streamon(struct vpu_dev_session *session,
		int port, void *priv)
{
	int ret;
	struct vpu_in_port_hnd *i_port_hnd = priv;

	pr_debug("received\n");

	if (!session) {
		pr_err("Null session pointer\n");
		return -EINVAL;
	}
	if (!i_port_hnd) {
		pr_err("Null i_port_hnd pointer\n");
		return -EINVAL;
	}
	if (session->streaming_state == ALL_STREAMING) {
		pr_err("Already streaming\n");
		return -EACCES;
	}
	if (session != i_port_hnd->session) {
		pr_err("Session %d already connected\n",
				i_port_hnd->session->id);
		return -EBUSY;
	}

	mutex_lock(&i_port_hnd->start_lock);
	if (i_port_hnd->buf_num == 0 ||
			i_port_hnd->buf_count != i_port_hnd->buf_num) {
		/*
		 * Buffer registration and session start will be executed in
		 * vcap2vpu_set_src_buffer() on the last buffer received.
		 */
		pr_debug("Received %d/%d buffers only, waiting for all.\n",
				i_port_hnd->buf_count, i_port_hnd->buf_num);

		i_port_hnd->start_req = true;
		ret = -EAGAIN;
		goto exit_streamon;
	}

	pr_debug("Set tunnel buffers to VPU on port %d\n", port);
	ret = vpu_hw_session_register_buffers(session->id,
			translate_port_id(INPUT_PORT),
			i_port_hnd->buf_array, i_port_hnd->buf_num);
	if (unlikely(ret))
		pr_err("Register buffer failed (error %d)\n", ret);

exit_streamon:
	mutex_unlock(&i_port_hnd->start_lock);
	return ret;
}

/*
 * vpu_in_vcap_detach() - Detach from VCAP sub-device. Also call deinit function
 * @session:	associated tunneling session
 * @port:	see: enum vpu_session_ports
 * @priv:	pointer to the input port control handler
 *
 * Once detach is received, action commands (detach, streamon, streamoff) become
 * inaccessible
 *
 * Return:	<void>
 */
static void vpu_in_vcap_detach(struct vpu_dev_session *session,
		int port, void *priv)
{
	struct vpu_in_port_hnd *i_port_hnd = priv;

	if (!session || !i_port_hnd) {
		pr_err("Null pointer\n");
		return;
	}
	if (session != i_port_hnd->session) {
		pr_err("Not attached to session %d\n", session->id);
		return;
	}

	vcap_tunnel_unregister_sink(i_port_hnd->vcap_channel);

	if (i_port_hnd->buf_array) {
		vpu_in_unmap_buffers(session, i_port_hnd);
		devm_kfree(session->core->dev, i_port_hnd->buf_array);
		i_port_hnd->buf_array = NULL;
		i_port_hnd->buf_num = 0;
	}

	vpu_deinit_port_vcap(session, &session->port_info[port]);

	pr_debug("Input tunnel channel %d detached from session %d\n",
			i_port_hnd->vcap_channel, session->id);
}

/*
 * vpu_in_vcap_attach() - Attach VPU input port to VCAP
 * @session:	associated tunneling session
 * @port:	see: enum vpu_session_ports
 * @priv:	pointer to the input port control handler
 *
 * Note:
 * Caller needs to fill out session->port_info[INPUT_PORT].source
 *
 * Once attach is received, other commands (detach, streamon, streamoff) become
 * accessible
 *
 * Return:	-EINVAL, if a null pointer is received in parameter or
 *		wrong input source configured
 *		-EBUSY, if port already attached, or session already connected
 *		-ECANCELED, connection with vcap failed
 *		0, otherwise
 */
static int vpu_in_vcap_attach(struct vpu_dev_session *session,
				int port, void *priv)
{
	struct vpu_in_port_hnd *i_port_hnd = priv;
	enum vcap_ch ch;
	struct vpu_port_ops *port_ops;
	u32 input_source;
	int ret = 0;
	struct vcap_tunnel_sink_ops vcap2vpu_ops = {
		.set_buffer_num = vcap2vpu_set_buffer_num,
		.set_buffer = vcap2vpu_set_src_buffer,
	};

	if (!session || !i_port_hnd || !session->core) {
		pr_err("Null pointer\n");
		return -EINVAL;
	}
	if (session != i_port_hnd->session) {
		pr_err("Session %d already connected\n",
				i_port_hnd->session->id);
		return -EBUSY;
	}
	port_ops = &session->port_info[port].port_ops;
	if (port_ops->detach || port_ops->streamon || port_ops->streamoff) {
		pr_err("Port already attached to session %d\n", session->id);
		return -EBUSY;
	}

	input_source = session->port_info[INPUT_PORT].source;
	pr_debug("Opening input tunnel (source %d) for session %d\n",
					input_source, session->id);

	ch = translate_input_src_to_tunnel_channel(input_source);
	if (!ch) {
		pr_err("Unknown input source (%d)\n", input_source);
		return -EINVAL;
	}

	/* associate VCAP channel with the VPU session passed in */
	ret = vcap_tunnel_register_sink(ch, (void *)i_port_hnd, &vcap2vpu_ops,
			&i_port_hnd->vcap_ctx, &i_port_hnd->vcap_ops);
	if (ret) {
		pr_err("Input tunnel registration failed: %d\n", ret);
		goto free_mem;
	}

	if (!i_port_hnd->vcap_ctx || !i_port_hnd->vcap_ops) {
		pr_err("Received null pointer(s) from input tunnel\n");
		ret = -ECANCELED;
		goto tunnel_unregister;
	}
	i_port_hnd->port = port;
	i_port_hnd->vcap_channel = ch;

	port_ops->detach = vpu_in_vcap_detach;
	port_ops->streamon = vpu_in_vcap_streamon;
	port_ops->streamoff = vpu_in_vcap_streamoff;

	pr_debug("Input tunnel channel %d attached to session %d\n",
			i_port_hnd->vcap_channel, session->id);

	return ret;

tunnel_unregister:
	vcap_tunnel_unregister_sink(ch);
free_mem:
	memset(i_port_hnd, 0, sizeof(*i_port_hnd));
	return ret;
}


/*
 * VPU Tunnel init/deinit functions
 */

/**
 * vpu_init_port_vcap() - creates a vpu_in_port_hnd handler and initializes port_ops
 * function pointers in the @session @port_info.
 * @session:	associated tunneling session
 * @port_info:	port information containing port ops to be initialized by
 *		vpu_in_port_hnd module's function pointers.
 *
 * Once the function pointers are initialized with values different than NULL,
 * they are automatically called during operational session phases (attach,
 * detach, streamon, streamoff)
 *
 * Right after initialization, only attach can be accessed
 * Only after attaching, other commands (detach, streamon, streamoff) become
 * accessible
 *
 * Return:	-EINVAL, if a null pointer is received in parameter
 *		-EISCONN, if the port is already connected to this session
 *		-ENOMEM, if the vpu_in_port_hnd handler memory allocation failed
 *		0, otherwise
 */
int vpu_init_port_vcap(struct vpu_dev_session *session,
			struct vpu_port_info *port_info)
{
	struct vpu_port_ops *port_ops;
	struct vpu_in_port_hnd *i_port_hnd;

	if (!session || !port_info) {
		pr_err("Null param pointer\n");
		return -EINVAL;
	}
	port_ops = &port_info->port_ops;
	if (port_ops->priv || port_ops->attach) {
		pr_err("End port already connected for this session\n");
		return -EISCONN;
	}

	i_port_hnd = devm_kzalloc(session->core->dev,
				sizeof(*i_port_hnd), GFP_KERNEL);
	if (!i_port_hnd) {
		pr_err("No memory\n");
		return -ENOMEM;
	}

	mutex_init(&i_port_hnd->start_lock);
	i_port_hnd->session = session;

	port_ops->attach = vpu_in_vcap_attach;
	port_ops->priv = i_port_hnd;

	pr_debug("Init input port for session %d successful\n", session->id);

	return 0;
}

/*
 * vpu_deinit_port_vcap() - destroys the init_vcap handler and deinit port_ops
 * function pointers in the @session's @port_info.
 * @session:	associated tunneling session
 * @port_info:	port information containing port ops to be initialized by
 *		vpu_in_port_hnd module's function pointers.
 *
 * Once the function pointers are reset to NULL, they are automatically *not*
 * called during operational phases (attach, detach, streamon, streamoff)
 *
 * Return:	<void>
 */
static void vpu_deinit_port_vcap(struct vpu_dev_session *session,
				 struct vpu_port_info *port_info)
{
	struct vpu_port_ops *port_ops;
	struct vpu_in_port_hnd *i_port_hnd;

	if (!session || !port_info) {
		pr_err("Null parameter\n");
		return;
	}
	if (!port_info->port_ops.priv) {
		pr_err("Null priv pointer\n");
		return;
	}

	i_port_hnd = port_info->port_ops.priv;
	devm_kfree(session->core->dev, i_port_hnd);

	port_ops = &port_info->port_ops;
	memset(port_ops, 0, sizeof(*port_ops));

	pr_debug("Deinit input port for session %d successful\n", session->id);
}
