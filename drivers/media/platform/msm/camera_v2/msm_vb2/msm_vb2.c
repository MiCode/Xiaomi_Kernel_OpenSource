/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include "msm_vb2.h"

static int msm_vb2_queue_setup(struct vb2_queue *q,
	const struct v4l2_format *fmt,
	unsigned int *num_buffers, unsigned int *num_planes,
	unsigned int sizes[], void *alloc_ctxs[])
{
	int i;
	struct msm_v4l2_format_data *data = q->drv_priv;

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
	struct msm_vb2_buffer *msm_vb2_buf;

	stream = msm_get_stream_from_vb2q(vb->vb2_queue);
	if (!stream) {
		pr_err("%s: Couldn't find stream\n", __func__);
		return -EINVAL;
	}
	msm_vb2_buf = container_of(vb, struct msm_vb2_buffer, vb2_buf);
	msm_vb2_buf->in_freeq = 0;

	return 0;
}

static void msm_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct msm_vb2_buffer *msm_vb2;
	struct msm_stream *stream;
	unsigned long flags;

	msm_vb2 = container_of(vb, struct msm_vb2_buffer, vb2_buf);

	if (!msm_vb2) {
		pr_err("%s:%d] vb2_buf NULL", __func__, __LINE__);
		return;
	}

	stream = msm_get_stream_from_vb2q(vb->vb2_queue);
	if (!stream) {
		pr_err("%s:%d] NULL stream", __func__, __LINE__);
		return;
	}

	spin_lock_irqsave(&stream->stream_lock, flags);
	list_add_tail(&msm_vb2->list, &stream->queued_list);
	spin_unlock_irqrestore(&stream->stream_lock, flags);
}

static int msm_vb2_buf_finish(struct vb2_buffer *vb)
{
	struct msm_vb2_buffer *msm_vb2;
	struct msm_stream *stream;
	unsigned long flags;
	struct msm_vb2_buffer *msm_vb2_entry, *temp;

	msm_vb2 = container_of(vb, struct msm_vb2_buffer, vb2_buf);

	if (!msm_vb2) {
		pr_err("%s:%d] vb2_buf NULL", __func__, __LINE__);
		return -EINVAL;
	}

	stream = msm_get_stream_from_vb2q(vb->vb2_queue);
	if (!stream) {
		pr_err("%s:%d] NULL stream", __func__, __LINE__);
		return -EINVAL;
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
	return 0;
}

static void msm_vb2_buf_cleanup(struct vb2_buffer *vb)
{
	struct msm_vb2_buffer *msm_vb2;
	struct msm_stream *stream;
	unsigned long flags;

	msm_vb2 = container_of(vb, struct msm_vb2_buffer, vb2_buf);

	if (!msm_vb2) {
		pr_err("%s:%d] vb2 NULL", __func__, __LINE__);
		return;
	}

	stream = msm_get_stream_from_vb2q(vb->vb2_queue);
	if (!stream) {
		pr_err("%s:%d] NULL stream", __func__, __LINE__);
		return;
	}

	spin_lock_irqsave(&stream->stream_lock, flags);
	INIT_LIST_HEAD(&stream->queued_list);
	spin_unlock_irqrestore(&stream->stream_lock, flags);
}

static struct vb2_ops msm_vb2_get_q_op = {
	.queue_setup	= msm_vb2_queue_setup,
	.buf_init	= msm_vb2_buf_init,
	.buf_queue	= msm_vb2_buf_queue,
	.buf_cleanup	= msm_vb2_buf_cleanup,
	.buf_finish	= msm_vb2_buf_finish,
};


struct vb2_ops *msm_vb2_get_q_ops(void)
{
	return &msm_vb2_get_q_op;
}

static void *msm_vb2_dma_contig_get_userptr(void *alloc_ctx,
	unsigned long vaddr, unsigned long size, int write)
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

static struct vb2_buffer *msm_vb2_get_buf(int session_id,
	unsigned int stream_id)
{
	struct msm_stream *stream;
	struct vb2_buffer *vb2_buf = NULL;
	struct msm_vb2_buffer *msm_vb2 = NULL;
	unsigned long flags;

	stream = msm_get_stream(session_id, stream_id);
	if (IS_ERR_OR_NULL(stream))
		return NULL;

	spin_lock_irqsave(&stream->stream_lock, flags);

	if (!stream->vb2_q) {
		pr_err("%s: stream q not available\n", __func__);
		goto end;
	}

	list_for_each_entry(msm_vb2, &(stream->queued_list), list) {
		vb2_buf = &(msm_vb2->vb2_buf);
		if (vb2_buf->state != VB2_BUF_STATE_ACTIVE)
			continue;

		if (msm_vb2->in_freeq)
			continue;

		msm_vb2->in_freeq = 1;
		goto end;
	}
	msm_vb2 = NULL;
	vb2_buf = NULL;
end:
	spin_unlock_irqrestore(&stream->stream_lock, flags);
	return vb2_buf;
}

static int msm_vb2_put_buf(struct vb2_buffer *vb, int session_id,
				unsigned int stream_id)
{
	struct msm_stream *stream;
	struct msm_vb2_buffer *msm_vb2;
	int rc = 0;
	unsigned long flags;
	stream = msm_get_stream(session_id, stream_id);
	if (IS_ERR_OR_NULL(stream))
		return -EINVAL;

	spin_lock_irqsave(&stream->stream_lock, flags);
	if (vb) {
		msm_vb2 =
			container_of(vb, struct msm_vb2_buffer, vb2_buf);
		if (msm_vb2->in_freeq) {
			msm_vb2->in_freeq = 0;
			rc = 0;
		} else
			rc = -EINVAL;
	} else {
		pr_err("%s: VB buffer is null\n", __func__);
		rc = -EINVAL;
	}
	spin_unlock_irqrestore(&stream->stream_lock, flags);
	return rc;
}

static int msm_vb2_buf_done(struct vb2_buffer *vb, int session_id,
				unsigned int stream_id)
{
	unsigned long flags;
	struct msm_vb2_buffer *msm_vb2;
	struct msm_stream *stream;
	int rc = 0;

	stream = msm_get_stream(session_id, stream_id);
	if (IS_ERR_OR_NULL(stream))
		return 0;
	spin_lock_irqsave(&stream->stream_lock, flags);
	if (vb) {
		msm_vb2 =
			container_of(vb, struct msm_vb2_buffer, vb2_buf);
		/* put buf before buf done */
		if (msm_vb2->in_freeq) {
			vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
			msm_vb2->in_freeq = 0;
			rc = 0;
		} else
			rc = -EINVAL;
	} else {
		pr_err("%s: VB buffer is null\n", __func__);
		rc = -EINVAL;
	}

	spin_unlock_irqrestore(&stream->stream_lock, flags);
	return rc;
}

int msm_vb2_request_cb(struct msm_sd_req_vb2_q *req)
{
	if (!req) {
		pr_err("%s: suddev is null\n", __func__);
		return -EINVAL;
	}

	req->get_buf = msm_vb2_get_buf;
	req->get_vb2_queue = msm_vb2_get_queue;
	req->put_buf = msm_vb2_put_buf;
	req->buf_done = msm_vb2_buf_done;

	return 0;
}

