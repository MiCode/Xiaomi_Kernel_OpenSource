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
#include <linux/list.h>
#include <linux/sizes.h>
#include <linux/err.h>
#include <linux/msm_mdp.h>

#include "vpu_v4l2.h"
#include "vpu_channel.h"
#include "vpu_resources.h"

#define VPU_OUT_MIN_BUFFERS	03
#define VPU_OUT_MAX_BUFFERS	32

/**
 * struct vpu_out_port_hnd - VPU output tunneling structure definition
 * @buf_num:		number of output tunneling buffers
 * @buf_array:		array of buffers to keep track of them (for unmapping)
 * @buf_count:		number of output tunneling buffers received
 * @mdss_domain:	Multimedia Display Subsystem IOMMU domain
 */
struct vpu_out_port_hnd {

	u32                buf_num;
	struct vpu_buffer *buf_array;
	u32                buf_count;

	int                mdss_domain;
};

/*
 * Main fnction prototypes
 */
int vpu_init_port_mdss(struct vpu_dev_session *session,
				 struct vpu_port_info *port_info);
static void vpu_deinit_port_mdss(struct vpu_dev_session *session,
				 struct vpu_port_info *port_info);

/*
 * Helper functions
 */

/*
 * mdss_unmap_buf() - unmap a buffer from MDSS address space
 * @vpu_buf:	tunneling buffer
 *
 * Note: cannot be called after vpu_unmap_buf() for the same buffer
 *
 * Return:	<void>
 */
static void mdss_unmap_buf(struct vpu_out_port_hnd *o_port_hnd,
			struct vpu_buffer *vpu_buf)
{
	int i;
	struct vb2_buffer *vb = &vpu_buf->vb;

	pr_debug("unmap buffer #%d from iommu\n", vb->v4l2_buf.index);

	for (i = 0; i < vb->num_planes; i++) {
		struct vpu_plane *plane = &vpu_buf->planes[i];

		vpu_mem_unmap_from_device(plane->mem_cookie, MEM_MDP_ID);
	}

	vpu_buf->valid_addresses_mask &= ~ADDR_VALID_MDP;
}

/*
 * vpu_unmap_buf() - unmap a buffer from VPU address space
 * @vpu_buf:	tunneling buffer
 *
 * Return:	<void>
 */
static void vpu_unmap_buf(struct vpu_buffer *vpu_buf)
{
	int i;
	struct vb2_buffer *vb = &vpu_buf->vb;

	pr_debug("unmap buffer #%d from iommu\n", vb->v4l2_buf.index);

	for (i = 0; i < vb->num_planes; i++) {
		struct vpu_plane *plane = &vpu_buf->planes[i];

		if (plane->mem_cookie)
			vpu_mem_destroy_handle(plane->mem_cookie);
		memset(plane, 0, sizeof(*plane));
	}

	vpu_buf->valid_addresses_mask &= ~ADDR_VALID_VPU;
}

/*
 * vpu_out_unmap_buffers() - Unmap all tunnel buffers
 * @session:	associated tunneling session
 * @o_port_hnd:	pointer to the output port control handler
 *
 * Return:	<void>
 */
static void vpu_out_unmap_buffers(struct vpu_dev_session *session,
				struct vpu_out_port_hnd *o_port_hnd)
{
	int i;

	if (!session || !o_port_hnd) {
		pr_err("Null pointer\n");
		return;
	}

	BUG_ON(session->streaming_state & OUTPUT_STREAMING);

	for (i = 0; i < o_port_hnd->buf_num; i++) {
		mdss_unmap_buf(o_port_hnd, &o_port_hnd->buf_array[i]);
		vpu_unmap_buf(&o_port_hnd->buf_array[i]);
	}
}

/*
 * VPU tunnel output ops
 */

/*
 * vpu_out_mdss_set_buf_num() - create an array of vpu_buffers[@buf_num] in
 * tunneling session struct for output port and get the MDSS domain number
 * @session:	associated tunneling session
 * @port:	see: enum vpu_session_ports
 * @buf_num:	number of buffers that will be received from VCAP
 * @priv:	pointer to the output port control handler
 *
 * Note:
 * Caller needs to fill out session->port_info[OUTPUT_PORT].secure_content
 *
 * Return:	-EINVAL, if a null pointer is received in parameter or
 *		wrong buffer number received
 *		-ENOMEM, if memory allocation for buffer array failed
 *		-EPIPE, if communication with MDSS failed
 *		-EACCES, if releasing buffers while streaming
 *		0, otherwise
 */
static int vpu_out_mdss_set_buf_num(struct vpu_dev_session *session, int port,
			u32 buf_num, void *priv)
{
	struct vpu_out_port_hnd *o_port_hnd = priv;
	struct vpu_buffer *buf_array_alloc = NULL;
	int ret = 0;

	pr_debug("Entering\n");

	if (!session || !o_port_hnd) {
		pr_err("Null pointer\n");
		return -EINVAL;
	}

	if (buf_num != 0) {
		int mdss_domain;

		if ((buf_num < VPU_OUT_MIN_BUFFERS) ||
		    (buf_num > VPU_OUT_MAX_BUFFERS)) {
			pr_err("Cannot set %d buffers\n", buf_num);
			return -EINVAL;
		}

		pr_debug("%d tunnel buffers used on port %d for session %d\n",
				buf_num, port, session->id);

		mdss_domain = msm_fb_get_iommu_domain(NULL,
				session->port_info[port].secure_content ?
				MDP_IOMMU_DOMAIN_CP : MDP_IOMMU_DOMAIN_NS);
		if (IS_ERR_VALUE(o_port_hnd->mdss_domain)) {
			pr_err("Error: MDSS iommu domain number unknown (%d)\n",
					o_port_hnd->mdss_domain);
			return -EPIPE;
		}

		buf_array_alloc = devm_kzalloc(session->core->dev, buf_num *
					sizeof(struct vpu_buffer), GFP_KERNEL);
		if (!buf_array_alloc) {
			pr_err("Cannot allocate memory\n");
			return -ENOMEM;
		}

		o_port_hnd->mdss_domain = mdss_domain;
		o_port_hnd->buf_array = buf_array_alloc;
		o_port_hnd->buf_num = buf_num;

	} else {
		if (!o_port_hnd->buf_array || !o_port_hnd->buf_num) {
			pr_err("No buffers to unmap\n");
			return -EINVAL;
		}

		pr_debug("Release tunnel buffers on port %d\n", port);
		ret = vpu_hw_session_release_buffers(session->id,
			translate_port_id(OUTPUT_PORT), CH_RELEASE_OUT_BUF);
		if (ret)
			pr_err("Release buffer cmd failed\n");
		/*
		 * Since it is a sync command, we can unmap all buffers and
		 * free o_port_hnd->buf_array now.
		 */
		vpu_out_unmap_buffers(session, o_port_hnd);
		devm_kfree(session->core->dev, o_port_hnd->buf_array);
		o_port_hnd->buf_array = NULL;
		o_port_hnd->buf_num = 0;
	}

	o_port_hnd->buf_count = 0;

	pr_debug("Exiting (%d)\n", ret);

	return ret;
}

/*
 * vpu_out_mdss_set_buf() - handle tunneling buffer, already allocated
 * @session:	associated tunneling session
 * @port:	see: enum vpu_session_ports
 * @v4l2_buf:	tunnel buffer, to be mapped
 * @priv:	pointer to the output port control handler
 *
 * Map received buffer for both VPU and MDSS address spaces
 * Keep the buffer into an array
 * Buffers will be sent all at once to VPU HW later (before session start)
 *
 * Return:	-EINVAL, if a null pointer is received in parameter, or
 *		wrong number of planes received
 *		-EACCES, if number of buffers has not been set yet
 *		-ENOMEM, if vpu mem handle cannot be created for this buffer
 *		-ESPIPE, if buffer index is too big
 *		0, otherwise
 */
static int vpu_out_mdss_set_buf(struct vpu_dev_session *session, int port,
			struct v4l2_buffer *v4l2_buf, void *priv)
{
	struct vpu_out_port_hnd *o_port_hnd = priv;
	struct vpu_buffer vpu_buf;
	struct vpu_port_ops *port_ops;
	int i, ret = 0;
	bool secured;

	pr_debug("Entering\n");

	if (!session || !v4l2_buf || !priv) {
		pr_err("Null pointer\n");
		return -EINVAL;
	}
	port_ops = &session->port_info[port].port_ops;
	if (o_port_hnd->buf_array == NULL) {
		pr_err("Set tunnel buffer number first.\n");
		return -EACCES;
	}
	if (v4l2_buf->index >= o_port_hnd->buf_num) {
		pr_err("Wrong buffer index (%d) received on port %d\n",
				v4l2_buf->index, port);
		return -ESPIPE;
	}
	if (v4l2_buf->length != session->port_info[port].format.num_planes) {
		pr_err("Wrong number of planes (%d)\n", v4l2_buf->length);
		return -EINVAL;
	}
	if (!v4l2_buf->m.planes) {
		pr_err("Plane array is null\n");
		return -EINVAL;
	}
	if (o_port_hnd->buf_count >= o_port_hnd->buf_num) {
		pr_err("Too many buffers (%d/%d) received on port %d\n",
			o_port_hnd->buf_count, o_port_hnd->buf_num, port);
		return -EINVAL;
	}

	pr_debug("Received tunnel buffer #%d on port %d\n",
						v4l2_buf->index, port);

	secured = session->port_info[port].secure_content ? true : false;

	memset(&vpu_buf, 0, sizeof(vpu_buf));
	vpu_buf.vb.num_planes = v4l2_buf->length;

	/* for each plane... */
	for (i = 0; i < vpu_buf.vb.num_planes; i++) {
		struct vpu_plane *plane = &vpu_buf.planes[i];
		struct v4l2_plane *v4l2_plane = &v4l2_buf->m.planes[i];

		/* convert v4l2_buffer into vpu_buffer */
		plane->fd = v4l2_plane->m.fd;
		plane->length = v4l2_plane->length;
		plane->data_offset = v4l2_plane->reserved[0];

		/* get a vpu buffer memory handle */
		plane->mem_cookie = vpu_mem_create_handle(
				session->core->resources.mem_client);
		if (!plane->mem_cookie) {
			pr_err("Cannot create mem handle\n");
			ret = -ENOMEM;
			goto err_unmap_all;
		}

		/* map VPU address using fd passed in through tunnel */
		ret = vpu_mem_map_fd(plane->mem_cookie, plane->fd,
				plane->length, plane->data_offset, secured);
		if (ret) {
			vpu_buf.valid_addresses_mask &= ~ADDR_VALID_VPU;
			pr_err("Cannot map for VPU: error %d\n", ret);
			goto err_unmap_all;
		} else {
			plane->mapped_address[ADDR_INDEX_VPU] =
				vpu_mem_addr(plane->mem_cookie, MEM_VPU_ID);
			vpu_buf.valid_addresses_mask |= ADDR_VALID_VPU;
			pr_debug("VPU mapped addr = 0x%08x\n",
					plane->mapped_address[ADDR_INDEX_VPU]);
		}

		/* map for MDSS (can only be done after VPU mapping) */
		ret = vpu_mem_map_to_device(plane->mem_cookie, MEM_MDP_ID,
				o_port_hnd->mdss_domain);
		if (ret) {
			vpu_buf.valid_addresses_mask &= ~ADDR_VALID_MDP;
			pr_err("Cannot map for MDSS: error %d\n", ret);
			goto err_unmap_all;
		} else {
			plane->mapped_address[ADDR_INDEX_MDP] =
				vpu_mem_addr(plane->mem_cookie, MEM_MDP_ID);
			vpu_buf.valid_addresses_mask |= ADDR_VALID_MDP;
			pr_debug("MDP mapped addr = 0x%08x\n",
					plane->mapped_address[ADDR_INDEX_MDP]);
		}
	}

	/* keep a copy of the mapped buffer */
	memcpy(&vpu_buf.vb.v4l2_buf, v4l2_buf, sizeof(*v4l2_buf));
	memcpy(&o_port_hnd->buf_array[v4l2_buf->index],
					 &vpu_buf, sizeof(vpu_buf));
	o_port_hnd->buf_count++;

	return 0;

err_unmap_all:
	mdss_unmap_buf(o_port_hnd, &vpu_buf);
	vpu_unmap_buf(&vpu_buf);
	pr_err("Exiting with error %d\n", ret);
	return ret;
}

/*
 * vpu_out_mdss_streamon() - sends all the tunneling buffers to VPU HW, through
 * HFI layer, using the array of received tunneling buffers.
 * @session:	associated tunneling session
 * @port:	see: enum vpu_session_ports
 * @priv:	pointer to the output port control handler
 *
 * This function needs to be called after commit and before start commands.
 *
 * Return:	-EINVAL, if a null pointer is received in parameter
 *		-EBUSY, if a session is already connected to that port, or
 *		already streaming
 *		-EACCES, if no buffers are set
 *		set yet
 *		0, otherwise.
 */
static int  vpu_out_mdss_streamon(struct vpu_dev_session *session,
				int port, void *priv)
{
	struct vpu_out_port_hnd *o_port_hnd = priv;
	struct vpu_port_ops *port_ops;
	int ret;

	pr_debug("Entering\n");

	if (!session || !priv) {
		pr_err("Null pointer\n");
		return -EINVAL;
	}
	port_ops = &session->port_info[port].port_ops;
	if (!o_port_hnd->buf_array || !o_port_hnd->buf_num) {
		pr_err("No buffers\n");
		return -EACCES;
	}
	if (o_port_hnd->buf_count != o_port_hnd->buf_num) {
		pr_err("Received %d/%d buffers; cannot proceed\n",
				o_port_hnd->buf_count, o_port_hnd->buf_num);
		return -EACCES;
	}

	if (session->streaming_state == ALL_STREAMING) {
		pr_err("Cannot set buffers while streaming\n");
		return -EBUSY;
	}

	pr_debug("Set tunnel buffers to VPU on port %d\n", port);


	ret = vpu_hw_session_register_buffers(session->id,
			translate_port_id(OUTPUT_PORT),
			o_port_hnd->buf_array, o_port_hnd->buf_num);
	if (unlikely(ret))
		pr_err("Register buffer failed (error %d)\n", ret);

	return ret;
}

/*
 * vpu_out_mdss_streamoff() - sends command to VPU HW, through HFI layer, to
 * release the output tunnel buffers
 * @session:	associated tunneling session
 * @port:	see: enum vpu_session_ports
 * @priv:	pointer to the output port control handler
 *
 * Return:	<void>
 */
static void vpu_out_mdss_streamoff(struct vpu_dev_session *session,
				int port, void *priv)
{
	pr_debug("Entering\n");
}

/*
 * vpu_in_attach() - Attach VPU output port to MDSS
 * @session:	associated tunneling session
 * @port:	see: enum vpu_session_ports
 * @priv:	pointer to the output port control handler
 *
 * Note:
 * Caller needs to fill out session->port_info[OUTPUT_PORT].destination
 *
 * Return:	-EINVAL, if a null pointer is received in parameter or
 *		wrong output destination configured
 *		-EPIPE, connection with MDSS failed
 *		0, otherwise
 */
static int vpu_out_mdss_attach(struct vpu_dev_session *session,
				int port, void *priv)
{
	u32 dest;
	int ret = 0;

	pr_debug("Entering\n");

	if (!session || !priv) {
		pr_err("Null pointer\n");
		return -EINVAL;
	}

	dest = session->port_info[port].destination;
	if (!(dest & VPU_OUTPUT_TYPE_DISPLAY)) {
		pr_err("Wrong destination (%d) set on port %d\n", dest, port);
		return -EINVAL;
	}

	pr_debug("Output tunnel channel %x attached to session %d\n",
			dest, session->id);

	return ret;
}

/*
 * vpu_in_vcap_detach() - Detach VPU output port from MDSS.
 * @session:	associated tunneling session
 * @port:	see: enum vpu_session_ports
 * @priv:	pointer to the output port control handler
 *
 * Buffer unmap/free should already be done in vpu_out_mdss_set_buf_num(0)
 * if not done, let's do it now
 *
 * Return:	<void>
 */
static void vpu_out_mdss_detach(struct vpu_dev_session *session,
				int port, void *priv)
{
	struct vpu_out_port_hnd *o_port_hnd = priv;

	if (!session || !o_port_hnd) {
		pr_err("Null pointer\n");
		return;
	}

	if (o_port_hnd->buf_array)
		vpu_out_mdss_set_buf_num(session, port, 0, priv);

	vpu_deinit_port_mdss(session, &session->port_info[port]);
}

/**
 * vpu_init_port_mdss() - creates a vpu_out_port_hnd handler and initializes the
 * associated @session's port_ops function pointers in @port_info.
 * @session:	associated tunneling session
 * @port_info:	port information containing port ops to be initialized by
 *		vpu_in_vcap module's function pointers.
 *
 * Return:	-EINVAL, if a null pointer is received in parameter
 *		-EISCONN, if the port is already connected to this session
 *		-ENOMEM, if the vpu_in_ctrl handler memory allocation failed
 *		0, otherwise
 */
int vpu_init_port_mdss(struct vpu_dev_session *session,
			struct vpu_port_info *port_info)
{
	struct vpu_out_port_hnd	*o_port_hnd;

	pr_debug("Entering\n");

	if (!session || !port_info) {
		pr_err("Null param pointer\n");
		return -EINVAL;
	}
	if (port_info->port_ops.priv || port_info->port_ops.attach) {
		pr_err("End port already connected for this session\n");
		return -EISCONN;
	}

	o_port_hnd = devm_kzalloc(session->core->dev,
			sizeof(struct vpu_out_port_hnd), GFP_KERNEL);
	if (!o_port_hnd) {
		pr_err("memory allocation failed\n");
		return -ENOMEM;
	}

	port_info->port_ops.priv = (void *)o_port_hnd;
	port_info->port_ops.attach = vpu_out_mdss_attach;
	port_info->port_ops.set_buf_num = vpu_out_mdss_set_buf_num;
	port_info->port_ops.set_buf = vpu_out_mdss_set_buf;
	port_info->port_ops.detach = vpu_out_mdss_detach;
	port_info->port_ops.streamon = vpu_out_mdss_streamon;
	port_info->port_ops.streamoff = vpu_out_mdss_streamoff;

	pr_debug("Init output port for session %d successful\n", session->id);

	return 0;
}

/*
 * vpu_deinit_port_mdss() - destroys the vpu_out_ctrl handler and deinit
 * port_ops function pointers in the @session's @port_info.
 * @session:	associated tunneling session
 * @port_info:	port information containing port ops to be initialized by
 *		vpu_in_ctrl module's function pointers.
 *
 * Once the function pointers are reset to NULL, they are automatically *not*
 * called during operational phases (attach, detach, streamon, streamoff)
 *
 * Return:	<void>
 */
static void vpu_deinit_port_mdss(struct vpu_dev_session *session,
				 struct vpu_port_info *port_info)
{
	struct vpu_out_port_hnd *o_port_hnd;

	pr_debug("Entering\n");

	if (!session || !port_info) {
		pr_err("Null parameter\n");
		return;
	}
	if (!port_info->port_ops.priv) {
		pr_err("Null priv pointer\n");
		return;
	}

	o_port_hnd = port_info->port_ops.priv;
	devm_kfree(session->core->dev, o_port_hnd);
	memset(&port_info->port_ops, 0, sizeof(port_info->port_ops));

	pr_debug("Deinit output port for session %d successful\n", session->id);
}
