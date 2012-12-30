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

static spinlock_t vb2_buf_lock;

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
	struct msm_vb2_buffer *msm_vb2_buf;

	msm_vb2_buf = container_of(vb, struct msm_vb2_buffer, vb2_buf);
	msm_vb2_buf->in_freeq = 0;
	spin_lock_init(&vb2_buf_lock);

	return 0;
}

static void msm_vb2_buf_queue(struct vb2_buffer *vb)
{
}

static struct vb2_ops msm_vb2_get_q_op = {
	.queue_setup		= msm_vb2_queue_setup,
	.buf_init		= msm_vb2_buf_init,
	.buf_queue              = msm_vb2_buf_queue,
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
	kfree(buf_priv);
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
	struct vb2_queue *q;
	struct vb2_buffer *vb2_buf = NULL;
	struct msm_vb2_buffer *msm_vb2;
	unsigned long flags;

	spin_lock_irqsave(&vb2_buf_lock, flags);

	q = msm_get_stream_vb2q(session_id, stream_id);

	/*FIXME: need a check if stream on issue*/
	if (!q) {
		pr_err("%s: stream q not available\n", __func__);
		goto end;
	}

	list_for_each_entry(vb2_buf, &(q->queued_list),
		queued_entry) {
		if (vb2_buf->state != VB2_BUF_STATE_ACTIVE)
			continue;

		msm_vb2 = container_of(vb2_buf, struct msm_vb2_buffer, vb2_buf);
		if (msm_vb2->in_freeq)
			continue;

		msm_vb2->in_freeq = 1;
		goto end;
	}
	vb2_buf = NULL;
end:
	spin_unlock_irqrestore(&vb2_buf_lock, flags);
	return vb2_buf;
}

static int msm_vb2_put_buf(struct vb2_buffer *vb)
{
	struct msm_vb2_buffer *msm_vb2;
	int rc = 0;

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
	return rc;
}

static int msm_vb2_buf_done(struct vb2_buffer *vb)
{
	unsigned long flags;
	struct msm_vb2_buffer *msm_vb2;
	int rc = 0;

	spin_lock_irqsave(&vb2_buf_lock, flags);
	if (vb) {
		msm_vb2 =
			container_of(vb, struct msm_vb2_buffer, vb2_buf);
		/* put buf before buf done */
		if (msm_vb2->in_freeq) {
			vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
			rc = 0;
		} else
			rc = -EINVAL;
	} else {
		pr_err("%s: VB buffer is null\n", __func__);
		rc = -EINVAL;
	}

	spin_unlock_irqrestore(&vb2_buf_lock, flags);
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

