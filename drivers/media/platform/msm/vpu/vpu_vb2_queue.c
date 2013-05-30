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

#define pr_fmt(fmt)	"VPU, %s: " fmt, __func__

#include <linux/msm_ion.h>
#include "vpu_ioctl_internal.h"
#include "vpu_configuration.h"
#include "vpu_channel.h"
#include "vpu_v4l2.h"

/*
 * Videobuf2 callbacks
 */
#define MIN_NUM_VPU_BUFFERS 2
#define MAX_NUM_VPU_BUFFERS 32

static void vpu_vb2_ops_wait_prepare(struct vb2_queue *q)
{
	struct vpu_dev_session *session =
		(struct vpu_dev_session *) vb2_get_drv_priv(q);
	int port = get_port_number(q->type);

	if (port >= 0)
		mutex_unlock(&session->que_lock[port]);
}

static void vpu_vb2_ops_wait_finish(struct vb2_queue *q)
{
	struct vpu_dev_session *session =
		(struct vpu_dev_session *) vb2_get_drv_priv(q);
	int port = get_port_number(q->type);

	if (port >= 0)
		mutex_lock(&session->que_lock[port]);
}

/*
 * Return number of buffers and planes per buffer given current format
 *
 * called by vb2_reqbufs (confirms # of buffers & planes)
 */
static int vpu_vb2_ops_queue_setup(struct vb2_queue *q,
				const struct v4l2_format *fmt,
				unsigned int *num_buffers,
				unsigned int *num_planes, unsigned int sizes[],
				void *alloc_ctxs[])
{
	struct vpu_dev_session *session =
		(struct vpu_dev_session *) vb2_get_drv_priv(q);
	int i = 0;
	int port = 0;
	int min_buffers = MIN_NUM_VPU_BUFFERS;

	port = get_port_number(q->type);
	if (port < 0)
		return -EINVAL;

	*num_planes = min_t(typeof(session->port_info[port].format.num_planes),
			session->port_info[port].format.num_planes,
			VIDEO_MAX_PLANES);

	for (i = 0; i < *num_planes; i++) {
		sizes[i] = 0; /* not needed */
		alloc_ctxs[i] = (void *) session;
	}

	if (port == INPUT_PORT &&
		session->port_info[port].scan_mode == LINESCANINTERLACED)
		min_buffers = 4; /* 4 buffers minimum for interlaced input */

	if (*num_buffers < min_buffers)
		*num_buffers = min_buffers;
	else if (*num_buffers > MAX_NUM_VPU_BUFFERS)
		*num_buffers =  MAX_NUM_VPU_BUFFERS;

	return 0;
}

static void vpu_vb2_ops_buf_cleanup(struct vb2_buffer *vb); /* prototype */

/*
 * Used to map buffers/planes if memory was not mapped.
 * Unmapped memory is checked before calling vb2_qbuf.
 *
 * When vb2_qbuf or vb2_prepare_buf ioctl on a new buffer is called, get_userptr
 * is called on each plane, then buf_init is called.
 * So it's job is IOMAPPING user buffers. Buffers are in DEQUED state when this
 * is called
 */
static int vpu_vb2_ops_buf_init(struct vb2_buffer *vb)
{
	struct vpu_dev_session *session =
		(struct vpu_dev_session *) vb2_get_drv_priv(vb->vb2_queue);
	struct vpu_buffer *vpu_buf = to_vpu_buffer(vb);
	bool secure;
	int i, port, ret = 0;

	port = get_port_number(vb->vb2_queue->type);
	secure = session->port_info[port].secure_content ? true : false;

	for (i = 0; i < vb->num_planes; i++) {
		if (!vpu_buf->planes[i].new_plane)
			continue;
		vpu_buf->planes[i].new_plane = 0;

		if (!vpu_buf->planes[i].mem_cookie) {
			vpu_buf->planes[i].mem_cookie = vpu_mem_create_handle(
					session->core->resources.mem_client);
			if (!vpu_buf->planes[i].mem_cookie) {
				ret = -ENOMEM;
				goto err_buf_init;
			}
		}

		ret = vpu_mem_map_fd(vpu_buf->planes[i].mem_cookie,
				vpu_buf->planes[i].fd,
				vpu_buf->planes[i].length,
				vpu_buf->planes[i].data_offset,
				secure);
		if (ret) {
			vpu_buf->valid_addresses_mask &= ~ADDR_VALID_VPU;
			goto err_buf_init;
		} else {
			vpu_buf->planes[i].mapped_address[ADDR_INDEX_VPU] =
			      vpu_mem_addr(vpu_buf->planes[i].mem_cookie,
					      MEM_VPU_ID);
			vpu_buf->valid_addresses_mask |= ADDR_VALID_VPU;
		}
	}
	return 0;

err_buf_init:
	vpu_vb2_ops_buf_cleanup(vb);
	return ret;
}

/*
 * Only called if session is streaming.
 * Commits any pending configuration changes, and sends buffer to IPC channel.
 *
 * called to pass ownership of buffer to driver (its status becomes ACTIVE just
 * before calling this).
 * It's called in beginning of vb2_streamon to pass all QUEUED buffers, or at
 * end vb2_qbuf if device is already streaming
 * Function cannot fail. So prepare/init should make any required checks if
 * needed
 */
static void vpu_vb2_ops_buf_queue(struct vb2_buffer *vb)
{
	struct vpu_buffer *vpu_buf;
	struct vb2_queue *q = vb->vb2_queue;
	struct vpu_dev_session *session =
			(struct vpu_dev_session *) vb2_get_drv_priv(q);
	int ret = 0, port = get_port_number(q->type);
	vpu_buf = to_vpu_buffer(vb);

	if (session->streaming_state != ALL_STREAMING ||
		!list_empty(&session->pending_list[port])) {
		/*
		 * pending list tracks buffers which were queued by client but
		 * not yet passed to fw, waiting for session to stream on
		 * end-to-end. Access to this list is protected by port mutex
		 */
		INIT_LIST_HEAD(&vpu_buf->buffers_entry);
		list_add_tail(&vpu_buf->buffers_entry,
				&session->pending_list[port]);
	} else {
		if (port == INPUT_PORT)
			ret = vpu_hw_session_empty_buffer(session->id, vpu_buf);
		else
			ret = vpu_hw_session_fill_buffer(session->id, vpu_buf);

		if (ret) {
			pr_err("buf_queue fail, returning buffer\n");
			vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
		}
	}
}

/*
 * Frees all mapped memory for buffer
 *
 * Called when buffer memory is about to be cleaned up (but before put_userptr).
 * One way to initiate this is by calling vb2_reqbufs with count == 0 (or
 * vb2_queue_release)
 */
static void vpu_vb2_ops_buf_cleanup(struct vb2_buffer *vb)
{
	struct vpu_buffer *vpu_buf;
	int i;

	pr_debug("unmap buffer #%d from iommu\n", vb->v4l2_buf.index);
	vpu_buf = to_vpu_buffer(vb);

	for (i = 0; i < vb->num_planes; i++) {
		if (vpu_buf->planes[i].mem_cookie)
			vpu_mem_destroy_handle(vpu_buf->planes[i].mem_cookie);
		memset(&vpu_buf->planes[i], 0, sizeof(struct vpu_plane));
	}
}

/*
 * In vb2_streamon, after all QUEUED buffers are made ACTIVE, this is called and
 * passed number of queued (ACTIVE) buffers.
 * After function returns with success, vb2_queue status becomes streaming
 */
static int vpu_vb2_ops_start_streaming(struct vb2_queue *q, unsigned int count)
{
	return 0; /* does nothing but required by vb2 framework */
}

/*
 * Flushes all buffers passed to a port
 *
 * called to inform that streaming will be stopped. Immediately after this queue
 * streaming status is cleared.
 * All QUEUED or DONE/ERRORed buffers are moved to DEQUED
 * Can be called from vb2_queue_release or vb2_streamoff
 * Use to flush all buffers passed to a port
 */
static int vpu_vb2_ops_stop_streaming(struct vb2_queue *q)
{
	struct vpu_dev_session *session =
			(struct vpu_dev_session *) vb2_get_drv_priv(q);
	int port = get_port_number(q->type);

	pr_debug("called for port %d\n", port);

	/* Flush/Retrieve all queued buffers */
	vpu_vb2_flush_buffers(session, port);
	return 0;
}

static struct vb2_ops vpu_vb2_ops = {
	.queue_setup      = vpu_vb2_ops_queue_setup,
	.wait_prepare     = vpu_vb2_ops_wait_prepare,
	.wait_finish      = vpu_vb2_ops_wait_finish,
	.buf_init         = vpu_vb2_ops_buf_init,
	.buf_prepare      = NULL,
	.buf_queue        = vpu_vb2_ops_buf_queue,
	.buf_finish       = NULL,
	.buf_cleanup      = vpu_vb2_ops_buf_cleanup,
	.start_streaming  = vpu_vb2_ops_start_streaming,
	.stop_streaming   = vpu_vb2_ops_stop_streaming,
};

/*
 * Videobuf2 memops (not used but required by vb2 framework)
 */
static void *vpu_vb2_mem_ops_get_userptr(void *alloc_ctx,
		unsigned long vaddr, unsigned long size, int write)
{
	return (void *) 0xD15EA5E; /* return any non-null value */
}

static void vpu_vb2_mem_ops_put_userptr(void *buf_priv) {}

static struct vb2_mem_ops vpu_vb2_mem_ops = {
	.get_userptr  = vpu_vb2_mem_ops_get_userptr,
	.put_userptr  = vpu_vb2_mem_ops_put_userptr,
};

/*
 * Header defined vb2 helper functions
 */
int vpu_vb2_queue_init(struct vb2_queue *q, enum v4l2_buf_type type,
			void *pdata)
{
	int ret = 0;

	memset(q, 0, sizeof(*q));
	q->type = type;
	q->io_modes = VB2_USERPTR;
	q->drv_priv = pdata;
	q->buf_struct_size = sizeof(struct vpu_buffer);
	q->ops = &vpu_vb2_ops;
	q->mem_ops = &vpu_vb2_mem_ops;
	q->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	ret = vb2_queue_init(q);
	if (ret < 0)
		pr_err("init vb2 queue (type = %d) fails\n", type);

	return ret;
}

int vpu_vb2_flush_buffers(struct vpu_dev_session *session, int port)
{
	int i, ret = 0;
	struct vb2_queue *q = &session->vbqueue[port];
	struct vpu_buffer *buff, *n;
	enum flush_buf_type flush_port =
		(port == INPUT_PORT) ? CH_FLUSH_IN_BUF : CH_FLUSH_OUT_BUF;

	/* retrieve any buffers on pending list (not sent to fw yet) */
	list_for_each_entry_safe(buff, n,
		&session->pending_list[port], buffers_entry)
	{
		vb2_buffer_done(&buff->vb, VB2_BUF_STATE_ERROR);
		list_del(&buff->buffers_entry);
	}

	/* return if no buffers remain with FW */
	if (!atomic_read(&q->queued_count))
		return 0;

	/* pause, if session is streaming */
	if (session->streaming_state == ALL_STREAMING) {
		ret = vpu_hw_session_pause(session->id);
		if (ret) {
			pr_err("Session Pause failed\n");
			return ret;
		}
	}

	/* Flush buffers from FW */
	pr_debug("Flushing port %d buffers from FW\n", port);
	ret = vpu_hw_session_flush(session->id, flush_port);
	if (ret)
		pr_err("port %d flush failed\n", port);

	/* resume, if session is streaming */
	if (session->streaming_state == ALL_STREAMING)
		if (vpu_hw_session_resume(session->id))
			pr_err("Session Resume failed\n");

	if (!atomic_read(&q->queued_count))
		return 0;


	/* Forced retrieve of buffers not returned by FW. Should never happen */
	pr_warn("Forced retrieve of %d buffers from port %d\n",
		atomic_read(&q->queued_count), get_port_number(q->type));
	for (i = 0; i < q->num_buffers; i++) {
		if (q->bufs[i] && q->bufs[i]->state == VB2_BUF_STATE_ACTIVE)
			vb2_buffer_done(q->bufs[i], VB2_BUF_STATE_ERROR);
	}

	return ret;
}
