/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "CAM-VB2 %s:%d " fmt, __func__, __LINE__
#include "msm_vb2.h"

static int msm_vb2_queue_setup(struct vb2_queue *q,
	const void *parg,
	unsigned int *num_buffers, unsigned int *num_planes,
	unsigned int sizes[], void *alloc_ctxs[])
{
	int i;
	struct msm_v4l2_format_data *data = q->drv_priv;

	if (!data) {
		pr_err("%s: drv_priv NULL\n", __func__);
		return -EINVAL;
	}
	if (data->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (WARN_ON(data->num_planes > VIDEO_MAX_PLANES))
			return -EINVAL;

		*num_planes = data->num_planes;

		for (i = 0; i < data->num_planes; i++)
			sizes[i] = data->plane_sizes[i];
	} else {
		pr_err("%s: Unsupported buf type :%d\n", __func__,
			   data->type);
		return -EINVAL;
	}
	return 0;
}

int msm_vb2_buf_init(struct vb2_buffer *vb)
{
	struct msm_stream *stream;
	struct msm_session *session;
	struct msm_vb2_buffer *msm_vb2_buf;
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);

	session = msm_get_session_from_vb2q(vb->vb2_queue);
	if (IS_ERR_OR_NULL(session))
		return -EINVAL;

	read_lock(&session->stream_rwlock);

	stream = msm_get_stream_from_vb2q(vb->vb2_queue);
	if (!stream) {
		pr_err("%s: Couldn't find stream\n", __func__);
		read_unlock(&session->stream_rwlock);
		return -EINVAL;
	}
	msm_vb2_buf = container_of(vbuf, struct msm_vb2_buffer, vb2_v4l2_buf);
	msm_vb2_buf->in_freeq = 0;
	read_unlock(&session->stream_rwlock);
	return 0;
}

static void msm_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct msm_vb2_buffer *msm_vb2;
	struct msm_stream *stream;
	struct msm_session *session;
	unsigned long flags;
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);

	msm_vb2 = container_of(vbuf, struct msm_vb2_buffer, vb2_v4l2_buf);
	if (!msm_vb2) {
		pr_err("%s:%d] vb2_buf NULL", __func__, __LINE__);
		return;
	}

	session = msm_get_session_from_vb2q(vb->vb2_queue);
	if (IS_ERR_OR_NULL(session))
		return;

	read_lock(&session->stream_rwlock);

	stream = msm_get_stream_from_vb2q(vb->vb2_queue);
	if (!stream) {
		pr_err("%s:%d] NULL stream", __func__, __LINE__);
		read_unlock(&session->stream_rwlock);
		return;
	}

	spin_lock_irqsave(&stream->stream_lock, flags);
	list_add_tail(&msm_vb2->list, &stream->queued_list);
	spin_unlock_irqrestore(&stream->stream_lock, flags);
	read_unlock(&session->stream_rwlock);
}

static void msm_vb2_buf_finish(struct vb2_buffer *vb)
{
	struct msm_vb2_buffer *msm_vb2;
	struct msm_stream *stream;
	struct msm_session *session;
	unsigned long flags;
	struct msm_vb2_buffer *msm_vb2_entry, *temp;
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);

	msm_vb2 = container_of(vbuf, struct msm_vb2_buffer, vb2_v4l2_buf);
	if (!msm_vb2) {
		pr_err("%s:%d] vb2_buf NULL", __func__, __LINE__);
		return; 
	}

	session = msm_get_session_from_vb2q(vb->vb2_queue);
	if (IS_ERR_OR_NULL(session))
		return;

	read_lock(&session->stream_rwlock);

	stream = msm_get_stream_from_vb2q(vb->vb2_queue);
	if (!stream) {
		pr_err("%s:%d] NULL stream", __func__, __LINE__);
		read_unlock(&session->stream_rwlock);
		return;
	}

	spin_lock_irqsave(&stream->stream_lock, flags);
	list_for_each_entry_safe(msm_vb2_entry, temp, &(stream->queued_list),
		list) {
		if (msm_vb2_entry == msm_vb2) {
			list_del_init(&msm_vb2_entry->list);
			break;
		}
	}
	spin_unlock_irqrestore(&stream->stream_lock, flags);
	read_unlock(&session->stream_rwlock);
	return;
}

static void msm_vb2_stop_stream(struct vb2_queue *q)
{
	struct msm_vb2_buffer *msm_vb2, *temp;
	struct msm_stream *stream;
	struct msm_session *session;
	unsigned long flags;
	struct vb2_v4l2_buffer *vb2_v4l2_buf;

	session = msm_get_session_from_vb2q(q);
	if (IS_ERR_OR_NULL(session))
		return;

	read_lock(&session->stream_rwlock);

	stream = msm_get_stream_from_vb2q(q);
	if (!stream) {
		pr_err_ratelimited("%s:%d] NULL stream", __func__, __LINE__);
		read_unlock(&session->stream_rwlock);
		return;
	}

	/*
	 * Release all the buffers enqueued to driver
	 * when streamoff is issued
	 */

	spin_lock_irqsave(&stream->stream_lock, flags);
	list_for_each_entry_safe(msm_vb2, temp, &(stream->queued_list),
		list) {
			vb2_v4l2_buf = &(msm_vb2->vb2_v4l2_buf);
			if (vb2_v4l2_buf->vb2_buf.state == VB2_BUF_STATE_DONE)
				continue;
			vb2_buffer_done(&vb2_v4l2_buf->vb2_buf,
				VB2_BUF_STATE_DONE);
			msm_vb2->in_freeq = 0;
		}
	spin_unlock_irqrestore(&stream->stream_lock, flags);
	read_unlock(&session->stream_rwlock);
}

int msm_vb2_get_stream_state(struct msm_stream *stream)
{
	struct msm_vb2_buffer *msm_vb2, *temp;
	unsigned long flags;
	int rc = 1;

	spin_lock_irqsave(&stream->stream_lock, flags);
	list_for_each_entry_safe(msm_vb2, temp, &(stream->queued_list), list) {
		if (msm_vb2->in_freeq != 0) {
			rc = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&stream->stream_lock, flags);
	return rc;
}
EXPORT_SYMBOL(msm_vb2_get_stream_state);


static struct vb2_ops msm_vb2_get_q_op = {
	.queue_setup	= msm_vb2_queue_setup,
	.buf_init	= msm_vb2_buf_init,
	.buf_queue	= msm_vb2_buf_queue,
	.buf_finish	= msm_vb2_buf_finish,
	.stop_streaming = msm_vb2_stop_stream,
};


struct vb2_ops *msm_vb2_get_q_ops(void)
{
	return &msm_vb2_get_q_op;
}

static void *msm_vb2_dma_contig_get_userptr(void *alloc_ctx,
	unsigned long vaddr, unsigned long size,
	enum dma_data_direction dma_dir)
{
	struct msm_vb2_private_data *priv;
	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return ERR_PTR(-ENOMEM);
	priv->vaddr = (void *)vaddr;
	priv->size = size;
	priv->alloc_ctx = alloc_ctx;
	return priv;
}

static void msm_vb2_dma_contig_put_userptr(void *buf_priv)
{
	kzfree(buf_priv);
}

static struct vb2_mem_ops msm_vb2_get_q_mem_op = {
	.get_userptr		= msm_vb2_dma_contig_get_userptr,
	.put_userptr		= msm_vb2_dma_contig_put_userptr,
};

struct vb2_mem_ops *msm_vb2_get_q_mem_ops(void)
{
	return &msm_vb2_get_q_mem_op;
}

static struct vb2_queue *msm_vb2_get_queue(int session_id,
	unsigned int stream_id)
{
	return msm_get_stream_vb2q(session_id, stream_id);
}

static struct vb2_v4l2_buffer *msm_vb2_get_buf(int session_id,
	unsigned int stream_id)
{
	struct msm_stream *stream;
	struct msm_session *session;
	struct vb2_v4l2_buffer *vb2_v4l2_buf = NULL;
	struct msm_vb2_buffer *msm_vb2 = NULL;
	unsigned long flags;

	session = msm_get_session(session_id);
	if (IS_ERR_OR_NULL(session))
		return NULL;

	read_lock(&session->stream_rwlock);

	stream = msm_get_stream(session, stream_id);
	if (IS_ERR_OR_NULL(stream)) {
		read_unlock(&session->stream_rwlock);
		return NULL;
	}

	spin_lock_irqsave(&stream->stream_lock, flags);

	if (!stream->vb2_q) {
		pr_err("%s: stream q not available\n", __func__);
		goto end;
	}

	list_for_each_entry(msm_vb2, &(stream->queued_list), list) {
		vb2_v4l2_buf = &(msm_vb2->vb2_v4l2_buf);
		if (vb2_v4l2_buf->vb2_buf.state != VB2_BUF_STATE_ACTIVE)
			continue;

		if (msm_vb2->in_freeq)
			continue;

		msm_vb2->in_freeq = 1;
		goto end;
	}
	msm_vb2 = NULL;
	vb2_v4l2_buf = NULL;
end:
	spin_unlock_irqrestore(&stream->stream_lock, flags);
	read_unlock(&session->stream_rwlock);
	return vb2_v4l2_buf;
}

static struct vb2_v4l2_buffer *msm_vb2_get_buf_by_idx(int session_id,
	unsigned int stream_id, uint32_t index)
{
	struct msm_stream *stream;
	struct msm_session *session;
	struct vb2_v4l2_buffer *vb2_v4l2_buf = NULL;
	struct msm_vb2_buffer *msm_vb2 = NULL;
	unsigned long flags;

	session = msm_get_session(session_id);
	if (IS_ERR_OR_NULL(session))
		return NULL;

	read_lock(&session->stream_rwlock);

	stream = msm_get_stream(session, stream_id);

	if (IS_ERR_OR_NULL(stream)) {
		read_unlock(&session->stream_rwlock);
		return NULL;
	}

	spin_lock_irqsave(&stream->stream_lock, flags);

	if (!stream->vb2_q) {
		pr_err("%s: stream q not available\n", __func__);
		goto end;
	}

	list_for_each_entry(msm_vb2, &(stream->queued_list), list) {
		vb2_v4l2_buf = &(msm_vb2->vb2_v4l2_buf);
		if ((vb2_v4l2_buf->vb2_buf.index != index) || msm_vb2->in_freeq
			|| vb2_v4l2_buf->vb2_buf.state != VB2_BUF_STATE_ACTIVE)
			continue;

		msm_vb2->in_freeq = 1;
		goto end;
	}
	msm_vb2 = NULL;
	vb2_v4l2_buf = NULL;
end:
	spin_unlock_irqrestore(&stream->stream_lock, flags);
	read_unlock(&session->stream_rwlock);
	return vb2_v4l2_buf;
}

static int msm_vb2_put_buf(struct vb2_v4l2_buffer *vb, int session_id,
				unsigned int stream_id)
{
	struct msm_stream *stream;
	struct msm_session *session;
	struct msm_vb2_buffer *msm_vb2;
	struct vb2_v4l2_buffer *vb2_v4l2_buf = NULL;
	int rc = 0;
	unsigned long flags;

	session = msm_get_session(session_id);
	if (IS_ERR_OR_NULL(session))
		return -EINVAL;

	read_lock(&session->stream_rwlock);

	stream = msm_get_stream(session, stream_id);
	if (IS_ERR_OR_NULL(stream)) {
		read_unlock(&session->stream_rwlock);
		return -EINVAL;
	}

	spin_lock_irqsave(&stream->stream_lock, flags);
	if (vb) {
		list_for_each_entry(msm_vb2, &(stream->queued_list), list) {
			vb2_v4l2_buf = &(msm_vb2->vb2_v4l2_buf);
			if (vb2_v4l2_buf == vb)
				break;
		}
		if (vb2_v4l2_buf != vb) {
			pr_err("VB buffer is INVALID vb=%pK, ses_id=%d, str_id=%d\n",
					vb, session_id, stream_id);
			spin_unlock_irqrestore(&stream->stream_lock, flags);
			read_unlock(&session->stream_rwlock);
			return -EINVAL;
		}
		msm_vb2 =
			container_of(vb2_v4l2_buf, struct msm_vb2_buffer,
				vb2_v4l2_buf);
		if (msm_vb2->in_freeq) {
			msm_vb2->in_freeq = 0;
			rc = 0;
		} else
			rc = -EINVAL;
	} else {
		pr_err(" VB buffer is null for ses_id=%d, str_id=%d\n",
			    session_id, stream_id);
		rc = -EINVAL;
	}
	spin_unlock_irqrestore(&stream->stream_lock, flags);
	read_unlock(&session->stream_rwlock);
	return rc;
}

static int msm_vb2_buf_done(struct vb2_v4l2_buffer *vb, int session_id,
				unsigned int stream_id, uint32_t sequence,
				struct timeval *ts, uint32_t reserved)
{
	unsigned long flags;
	struct msm_vb2_buffer *msm_vb2;
	struct msm_stream *stream;
	struct msm_session *session;
	struct vb2_v4l2_buffer *vb2_v4l2_buf = NULL;
	int rc = 0;

	session = msm_get_session(session_id);
	if (IS_ERR_OR_NULL(session))
		return -EINVAL;

	read_lock(&session->stream_rwlock);

	stream = msm_get_stream(session, stream_id);
	if (IS_ERR_OR_NULL(stream)) {
		read_unlock(&session->stream_rwlock);
		return -EINVAL;
	}

	spin_lock_irqsave(&stream->stream_lock, flags);
	if (vb) {
		list_for_each_entry(msm_vb2, &(stream->queued_list), list) {
			vb2_v4l2_buf = &(msm_vb2->vb2_v4l2_buf);
			if (vb2_v4l2_buf == vb)
				break;
		}
		if (vb2_v4l2_buf != vb) {
			pr_err("VB buffer is INVALID ses_id=%d, str_id=%d, vb=%pK\n",
				    session_id, stream_id, vb);
			spin_unlock_irqrestore(&stream->stream_lock, flags);
			read_unlock(&session->stream_rwlock);
			return -EINVAL;
		}
		msm_vb2 =
			container_of(vb2_v4l2_buf, struct msm_vb2_buffer, vb2_v4l2_buf);
		/* put buf before buf done */
		if (msm_vb2->in_freeq) {
			vb2_v4l2_buf->sequence = sequence;
			vb2_v4l2_buf->timestamp = *ts;
			vb2_buffer_done(&vb2_v4l2_buf->vb2_buf,
				VB2_BUF_STATE_DONE);
			msm_vb2->in_freeq = 0;
			rc = 0;
		} else
			rc = -EINVAL;
	} else {
		pr_err(" VB buffer is NULL for ses_id=%d, str_id=%d\n",
			    session_id, stream_id);
		rc = -EINVAL;
	}
	spin_unlock_irqrestore(&stream->stream_lock, flags);
	read_unlock(&session->stream_rwlock);
	return rc;
}

long msm_vb2_return_buf_by_idx(int session_id, unsigned int stream_id,
				uint32_t index)
{
	struct msm_stream *stream;
	struct msm_session *session;
	struct vb2_v4l2_buffer *vb2_v4l2_buf = NULL;
	struct msm_vb2_buffer *msm_vb2 = NULL;
	unsigned long flags;
	long rc = -EINVAL;

	session = msm_get_session(session_id);
	if (IS_ERR_OR_NULL(session))
		return rc;

	read_lock(&session->stream_rwlock);

	stream = msm_get_stream(session, stream_id);
	if (IS_ERR_OR_NULL(stream)) {
		read_unlock(&session->stream_rwlock);
		return -EINVAL;
	}

	spin_lock_irqsave(&stream->stream_lock, flags);

	if (!stream->vb2_q) {
		pr_err("%s: stream q not available\n", __func__);
		goto end;
	}

	list_for_each_entry(msm_vb2, &(stream->queued_list), list) {
		vb2_v4l2_buf = &(msm_vb2->vb2_v4l2_buf);
		if ((vb2_v4l2_buf->vb2_buf.index != index)
			|| vb2_v4l2_buf->vb2_buf.state != VB2_BUF_STATE_ACTIVE)
			continue;

		if (!msm_vb2->in_freeq) {
			vb2_buffer_done(&vb2_v4l2_buf->vb2_buf,
				VB2_BUF_STATE_ERROR);
			rc = 0;
		} else {
			rc = -EINVAL;
		}
		break;
	}

end:
	spin_unlock_irqrestore(&stream->stream_lock, flags);
	read_unlock(&session->stream_rwlock);
	return rc;
}
EXPORT_SYMBOL(msm_vb2_return_buf_by_idx);

static int msm_vb2_flush_buf(int session_id, unsigned int stream_id)
{
	unsigned long flags;
	struct msm_vb2_buffer *msm_vb2;
	struct msm_stream *stream;
	struct msm_session *session;
	struct vb2_v4l2_buffer *vb2_v4l2_buf = NULL;

	session = msm_get_session(session_id);
	if (IS_ERR_OR_NULL(session))
		return -EINVAL;

	read_lock(&session->stream_rwlock);

	stream = msm_get_stream(session, stream_id);
	if (IS_ERR_OR_NULL(stream)) {
		read_unlock(&session->stream_rwlock);
		return -EINVAL;
	}

	spin_lock_irqsave(&stream->stream_lock, flags);
	list_for_each_entry(msm_vb2, &(stream->queued_list), list) {
		vb2_v4l2_buf = &(msm_vb2->vb2_v4l2_buf);
		/* Do buf done for all buffers*/
		vb2_buffer_done(&vb2_v4l2_buf->vb2_buf, VB2_BUF_STATE_DONE);
		msm_vb2->in_freeq = 0;
	}
	spin_unlock_irqrestore(&stream->stream_lock, flags);
	read_unlock(&session->stream_rwlock);
	return 0;
}


int msm_vb2_request_cb(struct msm_sd_req_vb2_q *req)
{
	if (!req) {
		pr_err("%s: suddev is null\n", __func__);
		return -EINVAL;
	}

	req->get_buf = msm_vb2_get_buf;
	req->get_buf_by_idx = msm_vb2_get_buf_by_idx;
	req->get_vb2_queue = msm_vb2_get_queue;
	req->put_buf = msm_vb2_put_buf;
	req->buf_done = msm_vb2_buf_done;
	req->flush_buf = msm_vb2_flush_buf;
	return 0;
}

