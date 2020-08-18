/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/videodev2.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-event.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include "mtk_rsc-dev.h"

static u32 mtk_rsc_node_get_v4l2_cap
	(struct mtk_rsc_ctx_queue *node_ctx);

static int mtk_rsc_videoc_s_meta_fmt(struct file *file, void *fh,
				    struct v4l2_format *f);

static int mtk_rsc_subdev_open(struct v4l2_subdev *sd,
			      struct v4l2_subdev_fh *fh)
{
	struct mtk_rsc_dev *rsc_dev = mtk_rsc_subdev_to_dev(sd);

	rsc_dev->ctx.fh = fh;

	return mtk_rsc_ctx_open(&rsc_dev->ctx);
}

static int mtk_rsc_subdev_close(struct v4l2_subdev *sd,
			       struct v4l2_subdev_fh *fh)
{
	struct mtk_rsc_dev *rsc_dev = mtk_rsc_subdev_to_dev(sd);

	return mtk_rsc_ctx_release(&rsc_dev->ctx);
}

static int mtk_rsc_subdev_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	struct mtk_rsc_dev *rsc_dev = mtk_rsc_subdev_to_dev(sd);

	if (enable) {
		ret = mtk_rsc_ctx_streamon(&rsc_dev->ctx);

		if (!ret)
			ret = mtk_rsc_dev_queue_buffers
				(mtk_rsc_ctx_to_dev(&rsc_dev->ctx), true);
		if (ret)
			pr_debug("failed to queue initial buffers (%d)", ret);
	}	else {
		ret = mtk_rsc_ctx_streamoff(&rsc_dev->ctx);
	}

	if (!ret)
		rsc_dev->mem2mem2.streaming = enable;

	return ret;
}

static int mtk_rsc_link_setup(struct media_entity *entity,
			     const struct media_pad *local,
			     const struct media_pad *remote, u32 flags)
{
	struct mtk_rsc_mem2mem2_device *m2m2 =
		container_of(entity, struct mtk_rsc_mem2mem2_device,
			     subdev.entity);
	struct mtk_rsc_dev *rsc_dev =
		container_of(m2m2, struct mtk_rsc_dev, mem2mem2);

	u32 pad = local->index;

	pr_debug("link setup: %d --> %d\n", pad, remote->index);

#if KERNEL_VERSION(4, 5, 0) >= MTK_RSC_KERNEL_BASE_VERSION
	WARN_ON(entity->type != MEDIA_ENT_T_V4L2_SUBDEV);
#else
	WARN_ON(entity->obj_type != MEDIA_ENTITY_TYPE_V4L2_SUBDEV);
#endif

	WARN_ON(pad >= m2m2->num_nodes);

	m2m2->nodes[pad].enabled = !!(flags & MEDIA_LNK_FL_ENABLED);

	/**
	 * queue_enable can be phase out in the future since
	 * we don't have internal queue of each node in
	 * v4l2 common module
	 */
	rsc_dev->queue_enabled[pad] = m2m2->nodes[pad].enabled;

	return 0;
}

static void mtk_rsc_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_rsc_mem2mem2_device *m2m2 = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_rsc_dev *mtk_rsc_dev = mtk_rsc_m2m_to_dev(m2m2);
	struct device *dev = &mtk_rsc_dev->pdev->dev;
	struct mtk_rsc_dev_buffer *buf = NULL;
	struct vb2_v4l2_buffer *v4l2_buf = NULL;
	struct mtk_rsc_dev_video_device *node =
		mtk_rsc_vbq_to_isp_node(vb->vb2_queue);
	int queue = mtk_rsc_dev_get_queue_id_of_dev_node(mtk_rsc_dev, node);

	dev_dbg(dev,
		"queue vb2_buf: Node(%s) queue id(%d)\n",
		node->name,
		queue);

	if (queue < 0) {
		dev_dbg(m2m2->dev, "Invalid mtk_rsc_dev node.\n");
		return;
	}

	if (mtk_rsc_dev->ctx.mode == MTK_RSC_CTX_MODE_DEBUG_BYPASS_ALL) {
		dev_dbg(m2m2->dev, "By pass mode, just loop back the buffer\n");
		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
		return;
	}

	if (!vb)
		pr_debug("VB can't be null\n");

	buf = mtk_rsc_vb2_buf_to_dev_buf(vb);

	if (!buf)
		pr_debug("buf can't be null\n");

	v4l2_buf = to_vb2_v4l2_buffer(vb);

	if (!v4l2_buf)
		pr_debug("v4l2_buf can't be null\n");

	mutex_lock(&mtk_rsc_dev->lock);

	pr_debug("init  mtk_rsc_ctx_buf_init, sequence(%d)\n",
		v4l2_buf->sequence);

	/* the dma address will be filled in later frame buffer handling*/
	mtk_rsc_ctx_buf_init(&buf->ctx_buf, queue, (dma_addr_t)0);
	pr_debug("set mtk_rsc_ctx_buf_init: user seq=%d\n",
		buf->ctx_buf.user_sequence);

	/* Added the buffer into the tracking list */
	list_add_tail(&buf->m2m2_buf.list,
		      &m2m2->nodes[node - m2m2->nodes].buffers);
	mutex_unlock(&mtk_rsc_dev->lock);

	/* Enqueue the buffer */
	if (mtk_rsc_dev->mem2mem2.streaming) {
		pr_debug("%s: mtk_rsc_dev_queue_buffers\n", node->name);
		mtk_rsc_dev_queue_buffers(mtk_rsc_dev, false);
	}
}

#if KERNEL_VERSION(4, 5, 0) >= MTK_RSC_KERNEL_BASE_VERSION
static int mtk_rsc_vb2_queue_setup(struct vb2_queue *vq,
				  const void *parg,
				  unsigned int *num_buffers,
				  unsigned int *num_planes,
				  unsigned int sizes[], void *alloc_ctxs[])
#else
static int mtk_rsc_vb2_queue_setup(struct vb2_queue *vq,
				  unsigned int *num_buffers,
				  unsigned int *num_planes,
				  unsigned int sizes[],
				  struct device *alloc_devs[])
#endif
{
	struct mtk_rsc_mem2mem2_device *m2m2 = vb2_get_drv_priv(vq);
	struct mtk_rsc_dev_video_device *node = mtk_rsc_vbq_to_isp_node(vq);
	struct mtk_rsc_dev *rsc_dev = mtk_rsc_m2m_to_dev(m2m2);
	void *buf_alloc_ctx = NULL;

	/* Get V4L2 format with the following method */
	const struct v4l2_format *fmt = &node->vdev_fmt;

	*num_planes = 1;
	*num_buffers = clamp_val(*num_buffers, 1, VB2_MAX_FRAME);

	if (vq->type == V4L2_BUF_TYPE_META_CAPTURE ||
	    vq->type == V4L2_BUF_TYPE_META_OUTPUT) {
		sizes[0] = fmt->fmt.meta.buffersize;
		buf_alloc_ctx = rsc_dev->ctx.img_vb2_alloc_ctx;
		pr_debug("Select smem_vb2_alloc_ctx(%llx) node_name(%s)\n",
			(unsigned long long)buf_alloc_ctx, node->name);
	} else {
		sizes[0] = fmt->fmt.pix_mp.plane_fmt[0].sizeimage;
		buf_alloc_ctx = rsc_dev->ctx.img_vb2_alloc_ctx;
		pr_debug("Select img_vb2_alloc_ctx(%llx) node_name(%s)\n",
			(unsigned long long)buf_alloc_ctx, node->name);
	}

#if KERNEL_VERSION(4, 5, 0) >= MTK_RSC_KERNEL_BASE_VERSION
	alloc_ctxs[0] = buf_alloc_ctx;
#else
	alloc_devs[0] = (struct device *)buf_alloc_ctx;
#endif

	pr_debug("mtk_rsc_vb2_queue_setup:type(%d),size(%d),ctx(%llx)\n",
		vq->type, sizes[0], (unsigned long long)buf_alloc_ctx);

	/* Initialize buffer queue */
	INIT_LIST_HEAD(&node->buffers);

	return 0;
}

static bool mtk_rsc_all_nodes_streaming(struct mtk_rsc_mem2mem2_device *m2m2,
				       struct mtk_rsc_dev_video_device *except)
{
	int i;

	for (i = 0; i < m2m2->num_nodes; i++) {
		struct mtk_rsc_dev_video_device *node = &m2m2->nodes[i];

		if (node == except)
			continue;
		if (node->enabled && !vb2_start_streaming_called(&node->vbq))
			return false;
	}

	return true;
}

static void mtk_rsc_return_all_buffers(struct mtk_rsc_mem2mem2_device *m2m2,
				      struct mtk_rsc_dev_video_device *node,
				      enum vb2_buffer_state state)
{
	struct mtk_rsc_dev *mtk_rsc_dev = mtk_rsc_m2m_to_dev(m2m2);
	struct mtk_rsc_mem2mem2_buffer *b, *b0;

	/* Return all buffers */
	mutex_lock(&mtk_rsc_dev->lock);
	list_for_each_entry_safe(b, b0, &node->buffers, list) {
		list_del(&b->list);
		vb2_buffer_done(&b->vbb.vb2_buf, state);
	}
	mutex_unlock(&mtk_rsc_dev->lock);
}

static int mtk_rsc_vb2_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct mtk_rsc_mem2mem2_device *m2m2 = vb2_get_drv_priv(vq);
	struct mtk_rsc_dev_video_device *node =
		mtk_rsc_vbq_to_isp_node(vq);
	int r;

	if (m2m2->streaming) {
		r = -EBUSY;
		goto fail_return_bufs;
	}

	if (!node->enabled) {
		pr_debug("Node (%ld) is not enable\n", node - m2m2->nodes);
		r = -EINVAL;
		goto fail_return_bufs;
	}
#if KERNEL_VERSION(4, 9, 0) >= MTK_RSC_KERNEL_BASE_VERSION
	r = media_entity_pipeline_start(&node->vdev.entity, &m2m2->pipeline);
#else
	r = media_pipeline_start(&node->vdev.entity, &m2m2->pipeline);
#endif
	if (r < 0) {
		pr_debug("Node (%ld) media_pipeline_start failed\n",
		       node - m2m2->nodes);
		goto fail_return_bufs;
	}

	if (!mtk_rsc_all_nodes_streaming(m2m2, node))
		return 0;

	/* Start streaming of the whole pipeline now */

	r = v4l2_subdev_call(&m2m2->subdev, video, s_stream, 1);
	if (r < 0) {
		pr_debug("Node (%ld) v4l2_subdev_call s_stream failed\n",
		       node - m2m2->nodes);
		goto fail_stop_pipeline;
	}
	return 0;

fail_stop_pipeline:
#if KERNEL_VERSION(4, 9, 0) >= MTK_RSC_KERNEL_BASE_VERSION
	media_entity_pipeline_stop(&node->vdev.entity);
#else
	media_pipeline_stop(&node->vdev.entity);
#endif
fail_return_bufs:
	mtk_rsc_return_all_buffers(m2m2, node, VB2_BUF_STATE_QUEUED);

	return r;
}

static void mtk_rsc_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct mtk_rsc_mem2mem2_device *m2m2 = vb2_get_drv_priv(vq);
	struct mtk_rsc_dev_video_device *node =
		mtk_rsc_vbq_to_isp_node(vq);
	int r;

	WARN_ON(!node->enabled);

	/* Was this the first node with streaming disabled? */
	if (mtk_rsc_all_nodes_streaming(m2m2, node)) {
		/* Yes, really stop streaming now */
		r = v4l2_subdev_call(&m2m2->subdev, video, s_stream, 0);
		if (r)
			dev_dbg(m2m2->dev, "failed to stop streaming\n");
	}

	mtk_rsc_return_all_buffers(m2m2, node, VB2_BUF_STATE_ERROR);
#if KERNEL_VERSION(4, 9, 0) >= MTK_RSC_KERNEL_BASE_VERSION
	media_entity_pipeline_stop(&node->vdev.entity);
#else
	media_pipeline_stop(&node->vdev.entity);
#endif
}

static int mtk_rsc_videoc_querycap(struct file *file, void *fh,
				  struct v4l2_capability *cap)
{
	struct mtk_rsc_mem2mem2_device *m2m2 = video_drvdata(file);
	struct mtk_rsc_dev_video_device *node = file_to_mtk_rsc_node(file);
	struct mtk_rsc_dev *rsc_dev = mtk_rsc_m2m_to_dev(m2m2);
	int queue_id =
		mtk_rsc_dev_get_queue_id_of_dev_node(rsc_dev, node);
	struct mtk_rsc_ctx_queue *node_ctx = &rsc_dev->ctx.queue[queue_id];

	strlcpy(cap->driver, m2m2->name, sizeof(cap->driver));
	strlcpy(cap->card, m2m2->model, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info),
		 "platform:%s", node->name);

	cap->device_caps =
		mtk_rsc_node_get_v4l2_cap(node_ctx) | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

/* Propagate forward always the format from the CIO2 subdev */
static int mtk_rsc_videoc_g_fmt(struct file *file, void *fh,
			       struct v4l2_format *f)
{
	struct mtk_rsc_dev_video_device *node = file_to_mtk_rsc_node(file);

	f->fmt = node->vdev_fmt.fmt;

	return 0;
}

static int mtk_rsc_videoc_try_fmt(struct file *file, void *fh,
				 struct v4l2_format *f)
{
	struct mtk_rsc_mem2mem2_device *m2m2 = video_drvdata(file);
	struct mtk_rsc_dev *rsc_dev = mtk_rsc_m2m_to_dev(m2m2);
	struct mtk_rsc_ctx *dev_ctx = &rsc_dev->ctx;
	struct mtk_rsc_dev_video_device *node = file_to_mtk_rsc_node(file);
	int queue_id =
		mtk_rsc_dev_get_queue_id_of_dev_node(rsc_dev, node);
	int ret = 0;

	ret = mtk_rsc_ctx_fmt_set_img(dev_ctx, queue_id, &f->fmt.pix_mp,
				     &node->vdev_fmt.fmt.pix_mp);

	/* Simply set the format to the node context in the initial version */
	if (ret) {
		pr_debug("Fmt(%d) not support for queue(%d), load default fmt\n",
			f->fmt.pix_mp.pixelformat, queue_id);

		ret = mtk_rsc_ctx_format_load_default_fmt
			(&dev_ctx->queue[queue_id], f);
	}

	if (!ret) {
		node->vdev_fmt.fmt.pix_mp = f->fmt.pix_mp;
		dev_ctx->queue[queue_id].fmt.pix_mp = node->vdev_fmt.fmt.pix_mp;
	}

	return ret;
}

static int mtk_rsc_videoc_s_fmt(struct file *file, void *fh,
			       struct v4l2_format *f)
{
	struct mtk_rsc_mem2mem2_device *m2m2 = video_drvdata(file);
	struct mtk_rsc_dev *rsc_dev = mtk_rsc_m2m_to_dev(m2m2);
	struct mtk_rsc_ctx *dev_ctx = &rsc_dev->ctx;
	struct mtk_rsc_dev_video_device *node = file_to_mtk_rsc_node(file);
	int queue_id = mtk_rsc_dev_get_queue_id_of_dev_node(rsc_dev, node);
	int ret = 0;

	ret = mtk_rsc_ctx_fmt_set_img(dev_ctx, queue_id, &f->fmt.pix_mp,
				     &node->vdev_fmt.fmt.pix_mp);

	/* Simply set the format to the node context in the initial version */
	if (!ret)
		dev_ctx->queue[queue_id].fmt.pix_mp = node->vdev_fmt.fmt.pix_mp;
	else
		dev_dbg(&rsc_dev->pdev->dev, "s_fmt, format not support\n");

	return ret;
}

static int mtk_rsc_meta_enum_format(struct file *file, void *fh,
				   struct v4l2_fmtdesc *f)
{
	struct mtk_rsc_dev_video_device *node = file_to_mtk_rsc_node(file);

	if (f->index > 0 || f->type != node->vbq.type)
		return -EINVAL;

	f->pixelformat = node->vdev_fmt.fmt.meta.dataformat;

	return 0;
}

static int mtk_rsc_videoc_s_meta_fmt(struct file *file, void *fh,
				    struct v4l2_format *f)
{
	struct mtk_rsc_mem2mem2_device *m2m2 = video_drvdata(file);
	struct mtk_rsc_dev *rsc_dev = mtk_rsc_m2m_to_dev(m2m2);
	struct mtk_rsc_ctx *dev_ctx = &rsc_dev->ctx;
	struct mtk_rsc_dev_video_device *node = file_to_mtk_rsc_node(file);
	int queue_id = mtk_rsc_dev_get_queue_id_of_dev_node(rsc_dev, node);

	int ret = 0;

	if (f->type != node->vbq.type)
		return -EINVAL;

	ret = mtk_rsc_ctx_format_load_default_fmt(&dev_ctx->queue[queue_id], f);

	if (!ret) {
		node->vdev_fmt.fmt.meta = f->fmt.meta;
		dev_ctx->queue[queue_id].fmt.meta = node->vdev_fmt.fmt.meta;
	} else {
		dev_dbg(&rsc_dev->pdev->dev,
			 "s_meta_fm failed, format not support\n");
	}

	return ret;
}

static int mtk_rsc_videoc_g_meta_fmt(struct file *file, void *fh,
				    struct v4l2_format *f)
{
	struct mtk_rsc_dev_video_device *node = file_to_mtk_rsc_node(file);

	if (f->type != node->vbq.type)
		return -EINVAL;

	f->fmt = node->vdev_fmt.fmt;

	return 0;
}

int mtk_rsc_videoc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct video_device *vdev = video_devdata(file);
	struct vb2_buffer *vb;
	struct mtk_rsc_dev_buffer *db;
	int r = 0;

	/* check if vb2 queue is busy */
	if (vdev->queue->owner && vdev->queue->owner != file->private_data)
		return -EBUSY;

	/**
	 * Keep the value of sequence in v4l2_buffer
	 * in ctx buf since vb2_qbuf will set it to 0
	 */
	vb = vdev->queue->bufs[p->index];

	if (vb) {
		db = mtk_rsc_vb2_buf_to_dev_buf(vb);
		pr_debug("qbuf: p:%llx,vb:%llx, db:%llx\n",
			(unsigned long long)p,
			(unsigned long long)vb,
			(unsigned long long)db);
		db->ctx_buf.user_sequence = p->sequence;
	}

	r = vb2_qbuf(vdev->queue, p);
	if (r)
		pr_debug("vb2_qbuf failed(err=%d): buf idx(%d)\n", r, p->index);
	return r;
}
EXPORT_SYMBOL_GPL(mtk_rsc_videoc_qbuf);

/******************** function pointers ********************/

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops mtk_rsc_subdev_internal_ops = {
	.open = mtk_rsc_subdev_open,
	.close = mtk_rsc_subdev_close,
};

static const struct v4l2_subdev_core_ops mtk_rsc_subdev_core_ops = {
#if KERNEL_VERSION(4, 5, 0) >= MTK_RSC_KERNEL_BASE_VERSION
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.try_ext_ctrls = v4l2_subdev_try_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.queryctrl = v4l2_subdev_queryctrl,
	.querymenu = v4l2_subdev_querymenu,
#endif
};

static const struct v4l2_subdev_video_ops mtk_rsc_subdev_video_ops = {
	.s_stream = mtk_rsc_subdev_s_stream,
};

static const struct v4l2_subdev_ops mtk_rsc_subdev_ops = {
	.core = &mtk_rsc_subdev_core_ops,
	.video = &mtk_rsc_subdev_video_ops,
};

static const struct media_entity_operations mtk_rsc_media_ops = {
	.link_setup = mtk_rsc_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};

static const struct vb2_ops mtk_rsc_vb2_ops = {
	.buf_queue = mtk_rsc_vb2_buf_queue,
	.queue_setup = mtk_rsc_vb2_queue_setup,
	.start_streaming = mtk_rsc_vb2_start_streaming,
	.stop_streaming = mtk_rsc_vb2_stop_streaming,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
};

static const struct v4l2_file_operations mtk_rsc_v4l2_fops = {
	.unlocked_ioctl = video_ioctl2,
	.open = v4l2_fh_open,
	.release = vb2_fop_release,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = v4l2_compat_ioctl32,
#endif
};

static const struct v4l2_ioctl_ops mtk_rsc_v4l2_ioctl_ops = {
	.vidioc_querycap = mtk_rsc_videoc_querycap,

	.vidioc_g_fmt_vid_cap_mplane = mtk_rsc_videoc_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane = mtk_rsc_videoc_s_fmt,
	.vidioc_try_fmt_vid_cap_mplane = mtk_rsc_videoc_try_fmt,

	.vidioc_g_fmt_vid_out_mplane = mtk_rsc_videoc_g_fmt,
	.vidioc_s_fmt_vid_out_mplane = mtk_rsc_videoc_s_fmt,
	.vidioc_try_fmt_vid_out_mplane = mtk_rsc_videoc_try_fmt,

	/* buffer queue management */
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = mtk_rsc_videoc_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
};

static const struct v4l2_ioctl_ops mtk_rsc_v4l2_meta_ioctl_ops = {
	.vidioc_querycap = mtk_rsc_videoc_querycap,

	.vidioc_enum_fmt_meta_cap = mtk_rsc_meta_enum_format,
	.vidioc_g_fmt_meta_cap = mtk_rsc_videoc_g_meta_fmt,
	.vidioc_s_fmt_meta_cap = mtk_rsc_videoc_s_meta_fmt,
	.vidioc_try_fmt_meta_cap = mtk_rsc_videoc_g_meta_fmt,

	.vidioc_enum_fmt_meta_out = mtk_rsc_meta_enum_format,
	.vidioc_g_fmt_meta_out = mtk_rsc_videoc_g_meta_fmt,
	.vidioc_s_fmt_meta_out = mtk_rsc_videoc_s_meta_fmt,
	.vidioc_try_fmt_meta_out = mtk_rsc_videoc_g_meta_fmt,

	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = mtk_rsc_videoc_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
};

static u32 mtk_rsc_node_get_v4l2_cap(struct mtk_rsc_ctx_queue *node_ctx)
{
	u32 cap = 0;

	if (node_ctx->desc.capture)
		if (node_ctx->desc.image)
			cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE;
		else
			cap = V4L2_CAP_META_CAPTURE;
	else
		if (node_ctx->desc.image)
			cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE;
		else
			cap = V4L2_CAP_META_OUTPUT;

	return cap;
}

static u32 mtk_rsc_node_get_format_type(struct mtk_rsc_ctx_queue *node_ctx)
{
	u32 type;

	if (node_ctx->desc.capture)
		if (node_ctx->desc.image)
			type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		else
			type = V4L2_BUF_TYPE_META_CAPTURE;
	else
		if (node_ctx->desc.image)
			type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		else
			type = V4L2_BUF_TYPE_META_OUTPUT;

	return type;
}

static const struct v4l2_ioctl_ops *mtk_rsc_node_get_ioctl_ops
	(struct mtk_rsc_ctx_queue *node_ctx)
{
	const struct v4l2_ioctl_ops *ops = NULL;

	if (node_ctx->desc.image)
		ops = &mtk_rsc_v4l2_ioctl_ops;
	else
		ops = &mtk_rsc_v4l2_meta_ioctl_ops;
	return ops;
}

/**
 * Config node's video properties
 * according to the device context requirement
 */
static void mtk_rsc_node_to_v4l2(struct mtk_rsc_dev *rsc_dev, u32 node,
				struct video_device *vdev,
				struct v4l2_format *f)
{
	u32 cap;
	struct mtk_rsc_ctx *device_ctx = &rsc_dev->ctx;
	struct mtk_rsc_ctx_queue *node_ctx = &device_ctx->queue[node];

	WARN_ON(node >= mtk_rsc_dev_get_total_node(rsc_dev));
	WARN_ON(!node_ctx);

	/* set cap of the node */
	cap = mtk_rsc_node_get_v4l2_cap(node_ctx);
	f->type = mtk_rsc_node_get_format_type(node_ctx);
	vdev->ioctl_ops = mtk_rsc_node_get_ioctl_ops(node_ctx);

	if (mtk_rsc_ctx_format_load_default_fmt(&device_ctx->queue[node], f)) {
		dev_dbg(&rsc_dev->pdev->dev,
			"Can't load default for node (%d): (%s)",
			node, device_ctx->queue[node].desc.name);
	} else {
		if (device_ctx->queue[node].desc.image) {
			dev_dbg(&rsc_dev->pdev->dev,
				"Node (%d): (%s), dfmt (f:0x%x w:%d: h:%d s:%d)\n",
				node, device_ctx->queue[node].desc.name,
				f->fmt.pix_mp.pixelformat,
				f->fmt.pix_mp.width,
				f->fmt.pix_mp.height,
				f->fmt.pix_mp.plane_fmt[0].sizeimage);
			node_ctx->fmt.pix_mp = f->fmt.pix_mp;
		} else {
			dev_dbg(&rsc_dev->pdev->dev,
				 "Node (%d): (%s), dfmt (f:0x%x s:%u)\n",
				 node, device_ctx->queue[node].desc.name,
				 f->fmt.meta.dataformat,
				 f->fmt.meta.buffersize);
			node_ctx->fmt.meta = f->fmt.meta;
		}
	}

#if KERNEL_VERSION(4, 7, 0) < MTK_RSC_KERNEL_BASE_VERSION
	/* device_caps was supported after 4.7 */
	vdev->device_caps = V4L2_CAP_STREAMING | cap;
#endif
}

int mtk_rsc_media_register(struct device *dev, struct media_device *media_dev,
			  const char *model)
{
	int r = 0;

	media_dev->dev = dev;
	dev_dbg(dev, "setup media_dev.dev: %llx\n",
		 (unsigned long long)media_dev->dev);

	strlcpy(media_dev->model, model, sizeof(media_dev->model));
	dev_dbg(dev, "setup media_dev.model: %s\n",
		 media_dev->model);

	snprintf(media_dev->bus_info, sizeof(media_dev->bus_info),
		 "%s", dev_name(dev));
	dev_dbg(dev, "setup media_dev.bus_info: %s\n",
		 media_dev->bus_info);

	media_dev->hw_revision = 0;
	dev_dbg(dev, "setup media_dev.hw_revision: %d\n",
		 media_dev->hw_revision);

#if KERNEL_VERSION(4, 5, 0) <= MTK_RSC_KERNEL_BASE_VERSION
	dev_dbg(dev, "media_device_init: media_dev:%llx\n",
		 (unsigned long long)media_dev);
	media_device_init(media_dev);
#endif

	pr_debug("Register media device: %s, %llx",
		media_dev->model,
		(unsigned long long)media_dev);

	r = media_device_register(media_dev);

	if (r) {
		dev_dbg(dev, "failed to register media device (%d)\n", r);
		goto fail_v4l2_dev;
	}
	return 0;

fail_v4l2_dev:
	media_device_unregister(media_dev);
#if KERNEL_VERSION(4, 5, 0) <= MTK_RSC_KERNEL_BASE_VERSION
	media_device_cleanup(media_dev);
#endif

	return r;
}
EXPORT_SYMBOL_GPL(mtk_rsc_media_register);

int mtk_rsc_v4l2_register(struct device *dev,
			 struct media_device *media_dev,
			 struct v4l2_device *v4l2_dev)
{
	int r = 0;
	/* Set up v4l2 device */
	v4l2_dev->mdev = media_dev;
	dev_dbg(dev, "setup v4l2_dev->mdev: %llx",
		 (unsigned long long)v4l2_dev->mdev);

	dev_dbg(dev, "Register v4l2 device: %llx",
		 (unsigned long long)v4l2_dev);

	r = v4l2_device_register(dev, v4l2_dev);

	if (r) {
		dev_dbg(dev, "failed to register V4L2 device (%d)\n", r);
		goto fail_v4l2_dev;
	}

	return 0;

fail_v4l2_dev:
	media_device_unregister(media_dev);
#if KERNEL_VERSION(4, 5, 0) <= MTK_RSC_KERNEL_BASE_VERSION
	media_device_cleanup(media_dev);
#endif

	return r;
}
EXPORT_SYMBOL_GPL(mtk_rsc_v4l2_register);

int mtk_rsc_mem2mem2_v4l2_register(struct mtk_rsc_dev *dev,
				  struct media_device *media_dev,
				  struct v4l2_device *v4l2_dev)
{
	struct mtk_rsc_mem2mem2_device *m2m2 = &dev->mem2mem2;

	int i, r;

	/**
	 * If media_dev or v4l2_dev is not set,
	 * use the default one in mtk_rsc_dev
	 */
	if (!media_dev) {
		m2m2->media_dev = &dev->media_dev;
		r = mtk_rsc_media_register(&dev->pdev->dev, m2m2->media_dev,
					  m2m2->model);

	if (r) {
		dev_dbg(m2m2->dev, "failed to register media device (%d)\n", r);
		goto fail_media_dev;
	}
	} else {
		m2m2->media_dev = media_dev;
	}

	if (!v4l2_dev) {
		m2m2->v4l2_dev = &dev->v4l2_dev;
		r = mtk_rsc_v4l2_register(&dev->pdev->dev,
					 m2m2->media_dev,
					 m2m2->v4l2_dev);
	if (r) {
		dev_dbg(m2m2->dev, "failed to register V4L2 device (%d)\n", r);
		goto fail_v4l2_dev;
	}
	} else {
		m2m2->v4l2_dev = v4l2_dev;
	}

	/* Initialize miscellaneous variables */
	m2m2->streaming = false;
	m2m2->v4l2_file_ops = mtk_rsc_v4l2_fops;

	/* Initialize subdev media entity */
	m2m2->subdev_pads = kcalloc(m2m2->num_nodes, sizeof(*m2m2->subdev_pads),
				    GFP_KERNEL);
	if (!m2m2->subdev_pads) {
		r = -ENOMEM;
		goto fail_subdev_pads;
	}
#if KERNEL_VERSION(4, 5, 0) >= MTK_RSC_KERNEL_BASE_VERSION
	r = media_entity_init(&m2m2->subdev.entity, m2m2->num_nodes,
			      m2m2->subdev_pads, 0);
#else
	r = media_entity_pads_init(&m2m2->subdev.entity, m2m2->num_nodes,
				   m2m2->subdev_pads);
#endif
	if (r) {
		dev_dbg(m2m2->dev,
			"failed initialize subdev media entity (%d)\n", r);
		goto fail_media_entity;
	}

	/* Initialize subdev */
	v4l2_subdev_init(&m2m2->subdev, &mtk_rsc_subdev_ops);

#if KERNEL_VERSION(4, 5, 0) >= KERNEL_VERSION(4, 14, 0)
	m2m2->subdev.entity.function =
		MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER;
#endif
	m2m2->subdev.entity.ops = &mtk_rsc_media_ops;

	for (i = 0; i < m2m2->num_nodes; i++) {
		m2m2->subdev_pads[i].flags = m2m2->nodes[i].output ?
			MEDIA_PAD_FL_SINK : MEDIA_PAD_FL_SOURCE;
	}

	m2m2->subdev.flags =
		V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	snprintf(m2m2->subdev.name, sizeof(m2m2->subdev.name),
		 "%s", m2m2->name);
	v4l2_set_subdevdata(&m2m2->subdev, m2m2);
	m2m2->subdev.internal_ops = &mtk_rsc_subdev_internal_ops;

	pr_debug("register subdev: %s\n", m2m2->subdev.name);
	r = v4l2_device_register_subdev(m2m2->v4l2_dev, &m2m2->subdev);
	if (r) {
		dev_dbg(m2m2->dev, "failed initialize subdev (%d)\n", r);
		goto fail_subdev;
	}
	r = v4l2_device_register_subdev_nodes(m2m2->v4l2_dev);
	if (r) {
		dev_dbg(m2m2->dev, "failed to register subdevs (%d)\n", r);
		goto fail_subdevs;
	}

	/* Create video nodes and links */
	for (i = 0; i < m2m2->num_nodes; i++) {
		struct mtk_rsc_dev_video_device *node = &m2m2->nodes[i];
		struct video_device *vdev = &node->vdev;
		struct vb2_queue *vbq = &node->vbq;
		u32 flags;

		/* Initialize miscellaneous variables */
		mutex_init(&node->lock);
		INIT_LIST_HEAD(&node->buffers);

		/* Initialize formats to default values */
		mtk_rsc_node_to_v4l2(dev, i, vdev, &node->vdev_fmt);

		/* Initialize media entities */
#if KERNEL_VERSION(4, 5, 0) >= MTK_RSC_KERNEL_BASE_VERSION
		r = media_entity_init(&vdev->entity, 1, &node->vdev_pad, 0);
#else
		r = media_entity_pads_init(&vdev->entity, 1, &node->vdev_pad);
#endif
		if (r) {
			dev_dbg(m2m2->dev,
				"failed initialize media entity (%d)\n", r);
			goto fail_vdev_media_entity;
		}
		node->vdev_pad.flags = node->output ?
			MEDIA_PAD_FL_SOURCE : MEDIA_PAD_FL_SINK;
		vdev->entity.ops = NULL;

		/* Initialize vbq */
		vbq->type = node->vdev_fmt.type;
		vbq->io_modes = VB2_MMAP | VB2_DMABUF;
		vbq->ops = &mtk_rsc_vb2_ops;
		vbq->mem_ops = m2m2->vb2_mem_ops;
		m2m2->buf_struct_size = sizeof(struct mtk_rsc_dev_buffer);
		vbq->buf_struct_size = m2m2->buf_struct_size;
		vbq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
		vbq->min_buffers_needed = 0;	/* Can streamon w/o buffers */
		/* Put the process hub sub device in the vb2 private data*/
		vbq->drv_priv = m2m2;
		vbq->lock = &node->lock;
		r = vb2_queue_init(vbq);
		if (r) {
			dev_dbg(m2m2->dev,
				"failed to initialize video queue (%d)\n", r);
			goto fail_vdev;
		}

		/* Initialize vdev */
		snprintf(vdev->name, sizeof(vdev->name), "%s %s",
			 m2m2->name, node->name);
		vdev->release = video_device_release_empty;
		vdev->fops = &m2m2->v4l2_file_ops;
		vdev->lock = &node->lock;
		vdev->v4l2_dev = m2m2->v4l2_dev;
		vdev->queue = &node->vbq;
		vdev->vfl_dir = node->output ? VFL_DIR_TX : VFL_DIR_RX;
		video_set_drvdata(vdev, m2m2);
		pr_debug("register vdev: %s\n", vdev->name);
		r = video_register_device(vdev, VFL_TYPE_GRABBER, -1);
		if (r) {
			dev_dbg(m2m2->dev,
				"failed to register video device (%d)\n", r);
			goto fail_vdev;
		}

		/* Create link between video node and the subdev pad */
		flags = 0;
		if (node->enabled)
			flags |= MEDIA_LNK_FL_ENABLED;
		if (node->immutable)
			flags |= MEDIA_LNK_FL_IMMUTABLE;
		if (node->output) {
#if KERNEL_VERSION(4, 5, 0) >= MTK_RSC_KERNEL_BASE_VERSION
			r = media_entity_create_link
#else
			r = media_create_pad_link
#endif
						(&vdev->entity, 0,
						 &m2m2->subdev.entity,
						 i, flags);
		} else {
#if KERNEL_VERSION(4, 5, 0) >= MTK_RSC_KERNEL_BASE_VERSION
			r = media_entity_create_link
#else
			r = media_create_pad_link
#endif
						(&m2m2->subdev.entity,
						 i, &vdev->entity, 0,
						 flags);
		}
		if (r)
			goto fail_link;
	}

	return 0;

	for (; i >= 0; i--) {
fail_link:
		video_unregister_device(&m2m2->nodes[i].vdev);
fail_vdev:
		media_entity_cleanup(&m2m2->nodes[i].vdev.entity);
fail_vdev_media_entity:
		mutex_destroy(&m2m2->nodes[i].lock);
	}
fail_subdevs:
	v4l2_device_unregister_subdev(&m2m2->subdev);
fail_subdev:
	media_entity_cleanup(&m2m2->subdev.entity);
fail_media_entity:
	kfree(m2m2->subdev_pads);
fail_subdev_pads:
	v4l2_device_unregister(m2m2->v4l2_dev);
fail_v4l2_dev:
fail_media_dev:
	pr_debug("fail_v4l2_dev: media_device_unregister and clenaup:%llx",
	       (unsigned long long)m2m2->media_dev);
	media_device_unregister(m2m2->media_dev);
#if KERNEL_VERSION(4, 5, 0) <= MTK_RSC_KERNEL_BASE_VERSION
	media_device_cleanup(m2m2->media_dev);
#endif

	return r;
}
EXPORT_SYMBOL_GPL(mtk_rsc_mem2mem2_v4l2_register);

int mtk_rsc_v4l2_unregister(struct mtk_rsc_dev *dev)
{
	struct mtk_rsc_mem2mem2_device *m2m2 = &dev->mem2mem2;
	unsigned int i;

	for (i = 0; i < m2m2->num_nodes; i++) {
		video_unregister_device(&m2m2->nodes[i].vdev);
		media_entity_cleanup(&m2m2->nodes[i].vdev.entity);
		mutex_destroy(&m2m2->nodes[i].lock);
	}

	v4l2_device_unregister_subdev(&m2m2->subdev);
	media_entity_cleanup(&m2m2->subdev.entity);
	kfree(m2m2->subdev_pads);
	v4l2_device_unregister(m2m2->v4l2_dev);
	media_device_unregister(m2m2->media_dev);
#if KERNEL_VERSION(4, 5, 0) <= MTK_RSC_KERNEL_BASE_VERSION
	media_device_cleanup(m2m2->media_dev);
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_rsc_v4l2_unregister);

void mtk_rsc_v4l2_buffer_done(struct vb2_buffer *vb,
			     enum vb2_buffer_state state)
{
	struct mtk_rsc_mem2mem2_buffer *b =
		container_of(vb, struct mtk_rsc_mem2mem2_buffer, vbb.vb2_buf);

	list_del(&b->list);
	vb2_buffer_done(&b->vbb.vb2_buf, state);
}
EXPORT_SYMBOL_GPL(mtk_rsc_v4l2_buffer_done);
