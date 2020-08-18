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

#include <linux/device.h>
#include <linux/platform_device.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/dma-mapping.h>

#include "mtk_rsc.h"
#include "mtk_rsc-dev.h"
#include "mtk_rsc-smem.h"
#include "mtk_rsc-v4l2.h"

#if KERNEL_VERSION(4, 8, 0) >= MTK_RSC_KERNEL_BASE_VERSION
#include <linux/dma-attrs.h>
#endif

struct vb2_v4l2_buffer *mtk_rsc_ctx_buffer_get_vb2_v4l2_buffer
(struct mtk_rsc_ctx_buffer *ctx_buf)
{
	struct mtk_rsc_dev_buffer *dev_buf = NULL;

	if (!ctx_buf) {
		pr_debug("Failed to convert ctx_buf to dev_buf: Null pointer\n");
		return NULL;
	}

	dev_buf	= mtk_rsc_ctx_buf_to_dev_buf(ctx_buf);

	return &dev_buf->m2m2_buf.vbb;
}

int mtk_rsc_ctx_core_queue_setup(struct mtk_rsc_ctx *ctx,
				struct mtk_rsc_ctx_queues_setting *
				queues_setting)
{
	int queue_idx = 0;
	int i = 0;

	for (i = 0; i < queues_setting->total_output_queues; i++) {
		struct mtk_rsc_ctx_queue_desc *queue_desc =
			&queues_setting->output_queue_descs[i];
		ctx->queue[queue_idx].desc = *queue_desc;
		queue_idx++;
	}

	/* Setup the capture queue */
	for (i = 0; i < queues_setting->total_capture_queues; i++) {
		struct mtk_rsc_ctx_queue_desc *queue_desc =
			&queues_setting->capture_queue_descs[i];
		ctx->queue[queue_idx].desc = *queue_desc;
		queue_idx++;
	}

	ctx->queues_attr.master = queues_setting->master;
	ctx->queues_attr.total_num = queue_idx;
	ctx->dev_node_num = ctx->queues_attr.total_num;
	strcpy(ctx->device_name, MTK_RSC_DEV_NAME);
	return 0;
}

/* Mediatek RSC context core initialization */
int mtk_rsc_ctx_core_init(struct mtk_rsc_ctx *ctx,
			 struct platform_device *pdev, int ctx_id,
			 struct mtk_rsc_ctx_desc *ctx_desc,
			 struct platform_device *proc_pdev,
			 struct platform_device *smem_pdev)
{
	/* Initialize main data structure */
	int r = 0;

#if KERNEL_VERSION(4, 8, 0) >= MTK_RSC_KERNEL_BASE_VERSION
	ctx->smem_vb2_alloc_ctx =
		vb2_dma_contig_init_ctx(&smem_pdev->dev);
	ctx->img_vb2_alloc_ctx =
		vb2_dma_contig_init_ctx(&pdev->dev);
#else
	ctx->smem_vb2_alloc_ctx = &smem_pdev->dev;
	ctx->img_vb2_alloc_ctx = &pdev->dev;
#endif
	if (IS_ERR((__force void *)ctx->smem_vb2_alloc_ctx))
		pr_debug("Failed to alloc vb2 dma context: smem_vb2_alloc_ctx");

	if (IS_ERR((__force void *)ctx->img_vb2_alloc_ctx))
		pr_debug("Failed to alloc vb2 dma context: img_vb2_alloc_ctx");

	ctx->pdev = pdev;
	ctx->ctx_id = ctx_id;
	/* keep th smem pdev to use related iommu functions */
	ctx->smem_device = smem_pdev;

	/* Will set default enabled after passing the unit test */
	ctx->mode = MTK_RSC_CTX_MODE_DEBUG_OFF;

	/* initialized the global frame index of the device context */
	atomic_set(&ctx->frame_param_sequence, 0);
	spin_lock_init(&ctx->qlock);

	/* setup the core operation of the device context */
	if (ctx_desc && ctx_desc->init)
		r = ctx_desc->init(ctx);

	return r;
}
EXPORT_SYMBOL_GPL(mtk_rsc_ctx_core_init);

int mtk_rsc_ctx_core_exit(struct mtk_rsc_ctx *ctx)
{
#if KERNEL_VERSION(4, 8, 0) >= MTK_RSC_KERNEL_BASE_VERSION
	vb2_dma_contig_cleanup_ctx(ctx->smem_vb2_alloc_ctx);
	vb2_dma_contig_cleanup_ctx(ctx->img_vb2_alloc_ctx);
#else
	ctx->smem_vb2_alloc_ctx = NULL;
	ctx->img_vb2_alloc_ctx = NULL;
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_rsc_ctx_core_exit);

int mtk_rsc_ctx_next_global_frame_sequence(struct mtk_rsc_ctx *ctx, int locked)
{
	int global_frame_sequence =
		atomic_inc_return(&ctx->frame_param_sequence);

	if (!locked)
		spin_lock(&ctx->qlock);

	global_frame_sequence =
		(global_frame_sequence & 0x0000FFFF) | (ctx->ctx_id << 16);

	if (!locked)
		spin_unlock(&ctx->qlock);

	return global_frame_sequence;
}
EXPORT_SYMBOL_GPL(mtk_rsc_ctx_next_global_frame_sequence);

static void mtk_rsc_ctx_buffer_done
	(struct mtk_rsc_ctx_buffer *ctx_buf, int state)
{
		if (!ctx_buf || state != MTK_RSC_CTX_BUFFER_DONE ||
		    state != MTK_RSC_CTX_BUFFER_FAILED)
			return;

		ctx_buf->state = state;
}

struct mtk_rsc_ctx_frame_bundle *mtk_rsc_ctx_get_processing_frame
(struct mtk_rsc_ctx *dev_ctx, int frame_id)
{
	struct mtk_rsc_ctx_frame_bundle *frame_bundle = NULL;

	spin_lock(&dev_ctx->qlock);

	list_for_each_entry(frame_bundle,
			    &dev_ctx->processing_frames.list, list) {
		if (frame_bundle->id == frame_id) {
			spin_unlock(&dev_ctx->qlock);
			return frame_bundle;
		}
	}

	spin_unlock(&dev_ctx->qlock);

	return NULL;
}

/**
 * structure mtk_rsc_ctx_finish_param must be the first elemt of param
 * So that the buffer can be return to vb2 queue successfully
 */
int mtk_rsc_ctx_core_finish_param_init(void *param, int frame_id, int state)
{
	struct mtk_rsc_ctx_finish_param *fram_param =
		(struct mtk_rsc_ctx_finish_param *)param;
	fram_param->frame_id = frame_id;
	fram_param->state = state;
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_rsc_ctx_core_finish_param_init);

void mtk_rsc_ctx_frame_bundle_add(struct mtk_rsc_ctx *ctx,
				 struct mtk_rsc_ctx_frame_bundle *bundle,
				 struct mtk_rsc_ctx_buffer *ctx_buf)
{
	int queue_id = 0;
	struct mtk_rsc_ctx_queue *ctx_queue = NULL;

	if (!bundle || !ctx_buf) {
		pr_debug("Add buffer to frame bundle failed, bundle(%llx),buf(%llx)\n",
			(long long)bundle, (long long)ctx_buf);
		return;
	}

	queue_id = ctx_buf->queue;

	if (bundle->buffers[queue_id])
		pr_debug("Queue(%d) buffer has alreay in this bundle, overwrite happen\n",
			queue_id);

	pr_debug("Add queue(%d) buffer%llx\n",
		 queue_id, (unsigned long long)ctx_buf);
		 bundle->buffers[queue_id] = ctx_buf;

	/* Fill context queue related information */
	ctx_queue = &ctx->queue[queue_id];

	if (!ctx_queue) {
		pr_debug("Can't find ctx queue (%d)\n", queue_id);
		return;
	}

	if (ctx->queue[ctx_buf->queue].desc.image) {
		if (ctx->queue[ctx_buf->queue].desc.capture)
			bundle->num_img_capture_bufs++;
		else
			bundle->num_img_output_bufs++;
	} else {
		if (ctx->queue[ctx_buf->queue].desc.capture)
			bundle->num_meta_capture_bufs++;
		else
			bundle->num_meta_output_bufs++;
	}
}
EXPORT_SYMBOL_GPL(mtk_rsc_ctx_frame_bundle_add);

void mtk_rsc_ctx_buf_init(struct mtk_rsc_ctx_buffer *b,
			 unsigned int queue, dma_addr_t daddr)
{
	b->state = MTK_RSC_CTX_BUFFER_NEW;
	b->queue = queue;
	b->daddr = daddr;
}
EXPORT_SYMBOL_GPL(mtk_rsc_ctx_buf_init);

enum mtk_rsc_ctx_buffer_state
	mtk_rsc_ctx_get_buffer_state(struct mtk_rsc_ctx_buffer *b)
{
	return b->state;
}

bool mtk_rsc_ctx_is_streaming(struct mtk_rsc_ctx *ctx)
{
	return ctx->streaming;
}
EXPORT_SYMBOL_GPL(mtk_rsc_ctx_is_streaming);

static int mtk_rsc_ctx_free_frame(struct mtk_rsc_ctx *dev_ctx,
				 struct mtk_rsc_ctx_frame_bundle *frame_bundle)
{
	spin_lock(&dev_ctx->qlock);

	frame_bundle->state = MTK_RSC_CTX_FRAME_NEW;
	list_del(&frame_bundle->list);
	list_add_tail(&frame_bundle->list, &dev_ctx->free_frames.list);

	spin_unlock(&dev_ctx->qlock);

	return 0;
}

int mtk_rsc_ctx_core_job_finish(struct mtk_rsc_ctx *dev_ctx,
			       struct mtk_rsc_ctx_finish_param *param)
{
	int i = 0;
	struct platform_device *pdev = dev_ctx->pdev;
	struct mtk_rsc_ctx_finish_param *fram_param =
		(struct mtk_rsc_ctx_finish_param *)param;
	struct mtk_rsc_dev *rsc_dev = NULL;
	struct mtk_rsc_ctx_frame_bundle *frame = NULL;
	enum vb2_buffer_state vbf_state = VB2_BUF_STATE_DONE;
	enum mtk_rsc_ctx_buffer_state ctxf_state =
		MTK_RSC_CTX_BUFFER_DONE;
	int user_sequence = 0;

	const int ctx_id =
		MTK_RSC_GET_CTX_ID_FROM_SEQUENCE(fram_param->frame_id);
	u64 timestamp = 0;

	pr_debug("mtk_rsc_ctx_core_job_finish_cb: param (%llx), platform_device(%llx)\n",
		 (unsigned long long)param, (unsigned long long)pdev);

	if (!dev_ctx)
		pr_debug("dev_ctx can't be null, can't release the frame\n");

	rsc_dev = mtk_rsc_ctx_to_dev(dev_ctx);

	if (fram_param) {
		dev_dbg(&rsc_dev->pdev->dev,
			 "CB recvied from ctx(%d), frame(%d), state(%d), rsc_dev(%llx)\n",
			 ctx_id, fram_param->frame_id,
			 fram_param->state, (long long)rsc_dev);
	} else {
		dev_dbg(&rsc_dev->pdev->dev,
			"CB recvied from ctx(%d), frame param is NULL\n",
			ctx_id);
			return -EINVAL;
	}

	/* Get the buffers of the processed frame */
	frame = mtk_rsc_ctx_get_processing_frame(&rsc_dev->ctx,
						fram_param->frame_id);

	if (!frame) {
		pr_debug("Can't find the frame boundle, Frame(%d)\n",
		       fram_param->frame_id);
		return -EINVAL;
	}

	if (fram_param->state == MTK_RSC_CTX_FRAME_DATA_ERROR) {
		vbf_state = VB2_BUF_STATE_ERROR;
		ctxf_state = MTK_RSC_CTX_BUFFER_FAILED;
	}

	/**
	 * Set the buffer's VB2 status so that
	 * the user can dequeue the buffer
	 */
	timestamp = ktime_get_ns();
	for (i = 0; i <= frame->last_index; i++) {
		struct mtk_rsc_ctx_buffer *ctx_buf = frame->buffers[i];

		if (!ctx_buf) {
			dev_dbg(&rsc_dev->pdev->dev,
				"ctx_buf(queue id= %d) of frame(%d)is NULL\n",
				i, fram_param->frame_id);
			continue;
		} else {
			struct vb2_v4l2_buffer *b =
				mtk_rsc_ctx_buffer_get_vb2_v4l2_buffer(ctx_buf);
#if KERNEL_VERSION(4, 5, 0) >= KERNEL_VERSION(4, 14, 0)
			b->vb2_buf.timestamp = ktime_get_ns();
#endif
			user_sequence = ctx_buf->user_sequence;
			mtk_rsc_ctx_buffer_done(ctx_buf, ctxf_state);
			mtk_rsc_v4l2_buffer_done(&b->vb2_buf, vbf_state);
		}
	}

	mtk_rsc_ctx_free_frame(&rsc_dev->ctx, frame);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_rsc_ctx_core_job_finish);

int mtk_rsc_ctx_finish_frame(struct mtk_rsc_ctx *dev_ctx,
			    struct mtk_rsc_ctx_frame_bundle *frame_bundle,
			    int done)
{
	spin_lock(&dev_ctx->qlock);
	frame_bundle->state = MTK_RSC_CTX_FRAME_PROCESSING;
	list_add_tail(&frame_bundle->list, &dev_ctx->processing_frames.list);
	spin_unlock(&dev_ctx->qlock);
	return 0;
}

static void set_img_fmt(struct v4l2_pix_format_mplane *mfmt_to_fill,
			struct mtk_rsc_ctx_format *ctx_fmt)
{
	int i = 0;

	mfmt_to_fill->pixelformat = ctx_fmt->fmt.img.pixelformat;
	mfmt_to_fill->num_planes = ctx_fmt->fmt.img.num_planes;

	pr_debug("%s: Fmt(%d),w(%d),h(%d)\n",
		__func__,
		mfmt_to_fill->pixelformat,
		mfmt_to_fill->width,
		mfmt_to_fill->height);

	/**
	 * The implementation wil be adjust after integrating MDP module
	 * since it provides the common format suppporting function
	 */
	for (i = 0 ; i < mfmt_to_fill->num_planes; ++i) {
		int bpl = 0;
		int sizeimage = 0;

		if (mfmt_to_fill->plane_fmt[i].bytesperline != 0) {
			bpl = mfmt_to_fill->plane_fmt[i].bytesperline;
		} else {
			bpl = (mfmt_to_fill->width *
				ctx_fmt->fmt.img.row_depth[i]) / 8;
		}

		if (mfmt_to_fill->plane_fmt[i].sizeimage != 0) {
			sizeimage = mfmt_to_fill->plane_fmt[i].sizeimage;
		} else {
			sizeimage = (bpl * mfmt_to_fill->height *
				 ctx_fmt->fmt.img.depth[i]) / 8;
		}

		mfmt_to_fill->plane_fmt[i].bytesperline = bpl;
		mfmt_to_fill->plane_fmt[i].sizeimage = sizeimage;
		pr_debug("plane(%d):bpl(%d),sizeimage(%u)\n",
			i, bpl, mfmt_to_fill->plane_fmt[i].sizeimage);
	}
}

static void set_meta_fmt(struct v4l2_meta_format *metafmt_to_fill,
			 struct mtk_rsc_ctx_format *ctx_fmt)
{
	metafmt_to_fill->dataformat = ctx_fmt->fmt.meta.dataformat;

	if (ctx_fmt->fmt.meta.max_buffer_size <= 0 ||
	    ctx_fmt->fmt.meta.max_buffer_size >
	    MTK_RSC_CTX_META_BUF_DEFAULT_SIZE) {
		pr_debug("buf size of meta(%u) can't be 0, use default %u\n",
			ctx_fmt->fmt.meta.dataformat,
			MTK_RSC_CTX_META_BUF_DEFAULT_SIZE);
		metafmt_to_fill->buffersize = MTK_RSC_CTX_META_BUF_DEFAULT_SIZE;
	} else {
		pr_debug("Load the meta size setting %u\n",
			ctx_fmt->fmt.meta.max_buffer_size);
		metafmt_to_fill->buffersize = ctx_fmt->fmt.meta.max_buffer_size;
	}
}

/* Get the default format setting */
int mtk_rsc_ctx_format_load_default_fmt(struct mtk_rsc_ctx_queue *queue,
				       struct v4l2_format *fmt_to_fill)
{
	struct mtk_rsc_ctx_format *ctx_fmt = NULL;

	if (queue->desc.num_fmts == 0)
		return 0; /* no format support list associated to this queue */

	if (queue->desc.default_fmt_idx >= queue->desc.num_fmts) {
		pr_debug("Queue(%s) err: default idx(%d) must < num_fmts(%d)\n",
			queue->desc.name, queue->desc.default_fmt_idx,
			queue->desc.num_fmts);
		queue->desc.default_fmt_idx = 0;
		pr_debug("Queue(%s) : reset default idx(%d)\n",
			queue->desc.name, queue->desc.default_fmt_idx);
	}

	ctx_fmt	= &queue->desc.fmts[queue->desc.default_fmt_idx];

	/* Check the type of the buffer */
	if (queue->desc.image) {
		struct v4l2_pix_format_mplane *node_fmt =
			&fmt_to_fill->fmt.pix_mp;

		if (queue->desc.capture) {
			fmt_to_fill->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
			node_fmt->width = MTK_RSC_OUTPUT_MAX_WIDTH;
			node_fmt->height = MTK_RSC_OUTPUT_MAX_HEIGHT;
		} else {
			fmt_to_fill->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
			node_fmt->width = MTK_RSC_INPUT_MAX_WIDTH;
			node_fmt->height = MTK_RSC_INPUT_MAX_HEIGHT;
		}
		set_img_fmt(node_fmt, ctx_fmt);
	}	else {
		/* meta buffer type */
		struct v4l2_meta_format *node_fmt = &fmt_to_fill->fmt.meta;

		if (queue->desc.capture)
			fmt_to_fill->type = V4L2_BUF_TYPE_META_CAPTURE;
		else
			fmt_to_fill->type = V4L2_BUF_TYPE_META_OUTPUT;

		set_meta_fmt(node_fmt, ctx_fmt);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_rsc_ctx_format_load_default_fmt);

static struct mtk_rsc_ctx_format *mtk_rsc_ctx_find_fmt
	(struct mtk_rsc_ctx_queue *queue, u32 format)
{
	int i;
	struct mtk_rsc_ctx_format *ctx_fmt;

	for (i = 0; i < queue->desc.num_fmts; i++) {
		ctx_fmt = &queue->desc.fmts[i];
		if (queue->desc.image) {
			pr_debug("idx(%d), pixelformat(%x), fmt(%x)\n",
				 i, ctx_fmt->fmt.img.pixelformat, format);
			if (ctx_fmt->fmt.img.pixelformat == format)
				return ctx_fmt;
		} else {
			if (ctx_fmt->fmt.meta.dataformat == format)
				return ctx_fmt;
		}
	}
	return NULL;
}

int mtk_rsc_ctx_fmt_set_meta(struct mtk_rsc_ctx *dev_ctx, int queue_id,
			    struct v4l2_meta_format *user_fmt,
			    struct v4l2_meta_format *node_fmt)
{
	struct mtk_rsc_ctx_queue *queue = NULL;
	struct mtk_rsc_ctx_format *ctx_fmt;

	if (queue_id >= dev_ctx->queues_attr.total_num) {
		pr_debug("Invalid queue id:%d\n", queue_id);
		return -EINVAL;
	}

	queue = &dev_ctx->queue[queue_id];
	if (!user_fmt || !node_fmt)
		return -EINVAL;

	ctx_fmt = mtk_rsc_ctx_find_fmt(queue, user_fmt->dataformat);
	if (!ctx_fmt)
		return -EINVAL;

	queue->ctx_fmt = ctx_fmt;
	set_meta_fmt(node_fmt, ctx_fmt);
	*user_fmt = *node_fmt;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_rsc_ctx_fmt_set_meta);

int mtk_rsc_ctx_fmt_set_img(struct mtk_rsc_ctx *dev_ctx, int queue_id,
			   struct v4l2_pix_format_mplane *user_fmt,
			   struct v4l2_pix_format_mplane *node_fmt)
{
	struct mtk_rsc_ctx_queue *queue = NULL;
	struct mtk_rsc_ctx_format *ctx_fmt;

	if (queue_id >= dev_ctx->queues_attr.total_num) {
		pr_debug("Invalid queue id:%d\n", queue_id);
		return -EINVAL;
	}

	queue = &dev_ctx->queue[queue_id];
	if (!user_fmt || !node_fmt)
		return -EINVAL;

	ctx_fmt = mtk_rsc_ctx_find_fmt(queue, user_fmt->pixelformat);
	if (!ctx_fmt)
		return -EINVAL;

	queue->ctx_fmt = ctx_fmt;
	node_fmt->width = user_fmt->width;
	node_fmt->height = user_fmt->height;
	if (user_fmt->plane_fmt[0].bytesperline != 0) {
		node_fmt->plane_fmt[0].bytesperline =
			user_fmt->plane_fmt[0].bytesperline;
	}

	if (user_fmt->plane_fmt[0].sizeimage != 0) {
		node_fmt->plane_fmt[0].sizeimage =
			user_fmt->plane_fmt[0].sizeimage;
	}

	set_img_fmt(node_fmt, ctx_fmt);

	*user_fmt = *node_fmt;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_rsc_ctx_fmt_set_img);

int mtk_rsc_ctx_streamon(struct mtk_rsc_ctx *dev_ctx)
{
	int ret = 0;

	if (dev_ctx->streaming) {
		pr_debug("stream on failed, pdev(%llx), ctx(%d) already on\n",
			(long long)dev_ctx->pdev, dev_ctx->ctx_id);
		return -EBUSY;
	}

	ret = mtk_rsc_streamon(dev_ctx->pdev, dev_ctx->ctx_id);
	if (ret) {
		pr_debug("streamon: ctx(%d) failed, notified by context\n",
		       dev_ctx->ctx_id);
		return -EBUSY;
	}

	dev_ctx->streaming = true;
	ret = mtk_rsc_dev_queue_buffers(mtk_rsc_ctx_to_dev(dev_ctx), true);
	if (ret)
		pr_debug("failed to queue initial buffers (%d)", ret);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_rsc_ctx_streamon);

int mtk_rsc_ctx_streamoff(struct mtk_rsc_ctx *dev_ctx)
{
	int ret = 0;

	if (!dev_ctx->streaming) {
		pr_debug("Do nothing, pdev(%llx), ctx(%d) is already stream off\n",
			(long long)dev_ctx->pdev, dev_ctx->ctx_id);
		return -EBUSY;
	}

	ret = mtk_rsc_streamoff(dev_ctx->pdev, dev_ctx->ctx_id);
	if (ret) {
		pr_debug("streamoff: ctx(%d) failed, notified by context\n",
			dev_ctx->ctx_id);
		return -EBUSY;
	}

	dev_ctx->streaming = false;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_rsc_ctx_streamoff);

int mtk_rsc_ctx_init_frame_bundles(struct mtk_rsc_ctx *dev_ctx)
{
	int i = 0;

	dev_ctx->num_frame_bundle = VB2_MAX_FRAME;

	spin_lock(&dev_ctx->qlock);

	/* Reset the queue*/
	INIT_LIST_HEAD(&dev_ctx->processing_frames.list);
	INIT_LIST_HEAD(&dev_ctx->free_frames.list);

	for (i = 0; i < dev_ctx->num_frame_bundle; i++) {
		struct mtk_rsc_ctx_frame_bundle *frame_bundle =
			&dev_ctx->frame_bundles[i];
		frame_bundle->state = MTK_RSC_CTX_FRAME_NEW;
		list_add_tail(&frame_bundle->list, &dev_ctx->free_frames.list);
	}

	spin_unlock(&dev_ctx->qlock);

	return 0;
}

int mtk_rsc_ctx_open(struct mtk_rsc_ctx *dev_ctx)
{
	struct mtk_rsc_dev *rsc_dev = mtk_rsc_ctx_to_dev(dev_ctx);
	unsigned int enabled_dma_ports = 0;
	int i = 0;

	if (!dev_ctx)
		return -EINVAL;

	/* Get the enabled DMA ports */
	for (i = 0; i < rsc_dev->mem2mem2.num_nodes; i++) {
		if (rsc_dev->mem2mem2.nodes[i].enabled)
			enabled_dma_ports |= dev_ctx->queue[i].desc.dma_port;
	}

	dev_ctx->enabled_dma_ports = enabled_dma_ports;

	dev_dbg(&rsc_dev->pdev->dev, "open device: (%llx)\n",
		(long long)&rsc_dev->pdev->dev);

#if KERNEL_VERSION(4, 5, 0) >= KERNEL_VERSION(4, 14, 0)
	/* Workaround for SCP EMI access */
	mtk_rsc_smem_enable_mpu(&dev_ctx->smem_device->dev);
#endif
	/* Init the frame bundle pool */
	mtk_rsc_ctx_init_frame_bundles(dev_ctx);

	return mtk_rsc_open(dev_ctx->pdev);
}
EXPORT_SYMBOL_GPL(mtk_rsc_ctx_open);

int mtk_rsc_ctx_release(struct mtk_rsc_ctx *dev_ctx)
{
	return mtk_rsc_release(dev_ctx->pdev);
}
EXPORT_SYMBOL_GPL(mtk_rsc_ctx_release);

static int mtk_rsc_ctx_core_job_start(struct mtk_rsc_ctx *dev_ctx,
				     struct mtk_rsc_ctx_frame_bundle *bundle)
{
	struct platform_device *pdev = dev_ctx->pdev;
	int ret = 0;
	struct v4l2_rsc_frame_param rsc_param;
	struct mtk_rsc_ctx_buffer *buf_pre_rrzo_in = NULL;
	struct mtk_rsc_ctx_buffer *buf_cur_rrzo_in = NULL;
	struct mtk_rsc_ctx_buffer *buf_tuning_in = NULL;
	struct mtk_rsc_ctx_buffer *buf_result_out = NULL;

	if (!pdev || !bundle) {
		dev_dbg(&pdev->dev,
			"pdev(%llx) and param(%llx) in start can't be NULL\n",
			(long long)pdev, (long long)bundle);
		return -EINVAL;
	}
	memset(&rsc_param, 0, sizeof(struct v4l2_rsc_frame_param));
	rsc_param.frame_id = bundle->id;

	/* pre rrzo in buffer */
	buf_pre_rrzo_in = bundle->buffers[MTK_ISP_CTX_RSC_PRE_RRZO_IN];
	if (buf_pre_rrzo_in) {
		rsc_param.pre_rrzo_in[0].iova[0] =
			(uint32_t)buf_pre_rrzo_in->daddr;
		rsc_param.pre_rrzo_in[0].format.width =
			(uint16_t)buf_pre_rrzo_in->fmt.pix_mp.width;
		rsc_param.pre_rrzo_in[0].format.height =
			(uint16_t)buf_pre_rrzo_in->fmt.pix_mp.height;
		rsc_param.pre_rrzo_in[0].format.plane_fmt[0].stride =
			buf_pre_rrzo_in->fmt.pix_mp.plane_fmt[0].bytesperline;
		rsc_param.pre_rrzo_in[0].format.plane_fmt[0].size =
			buf_pre_rrzo_in->fmt.pix_mp.plane_fmt[0].sizeimage;
	}

	dev_dbg(&pdev->dev,
		 "pre_rrzo_in info:width(%d) height(%d) stride(%d) size(%d) iova(0x%11x)\n",
		 rsc_param.pre_rrzo_in[0].format.width,
		 rsc_param.pre_rrzo_in[0].format.height,
		 rsc_param.pre_rrzo_in[0].format.plane_fmt[0].stride,
		 rsc_param.pre_rrzo_in[0].format.plane_fmt[0].size,
		 rsc_param.pre_rrzo_in[0].iova[0]);

	/* current rrzo in buffer */
	buf_cur_rrzo_in = bundle->buffers[MTK_ISP_CTX_RSC_CUR_RRZO_IN];
	if (buf_cur_rrzo_in) {
		rsc_param.cur_rrzo_in[0].iova[0] =
			(uint32_t)buf_cur_rrzo_in->daddr;
		rsc_param.cur_rrzo_in[0].format.width =
			(uint16_t)buf_cur_rrzo_in->fmt.pix_mp.width;
		rsc_param.cur_rrzo_in[0].format.height =
			(uint16_t)buf_cur_rrzo_in->fmt.pix_mp.height;
		rsc_param.cur_rrzo_in[0].format.plane_fmt[0].stride =
			buf_cur_rrzo_in->fmt.pix_mp.plane_fmt[0].bytesperline;
		rsc_param.cur_rrzo_in[0].format.plane_fmt[0].size =
			buf_cur_rrzo_in->fmt.pix_mp.plane_fmt[0].sizeimage;
	}

	dev_dbg(&pdev->dev,
		 "cur_rrzo_in info:width(%d) height(%d) stride(%d) size(%d) iova(0x%11x)\n",
		 rsc_param.cur_rrzo_in[0].format.width,
		 rsc_param.cur_rrzo_in[0].format.height,
		 rsc_param.cur_rrzo_in[0].format.plane_fmt[0].stride,
		 rsc_param.cur_rrzo_in[0].format.plane_fmt[0].size,
		 rsc_param.cur_rrzo_in[0].iova[0]);


	/* tuning in buffer */
	buf_tuning_in = bundle->buffers[MTK_ISP_CTX_RSC_TUNING_IN];
	if (buf_tuning_in) {
		memcpy(rsc_param.tuning_data, buf_tuning_in->vaddr,
			MTK_ISP_CTX_RSC_TUNING_DATA_NUM * sizeof(uint32_t));
	}

	/* result out buffer */
	buf_result_out = bundle->buffers[MTK_ISP_CTX_RSC_RESULT_OUT];
	if (buf_result_out) {
		rsc_param.meta_out.va = (uint64_t)buf_result_out->vaddr;
		rsc_param.meta_out.pa = (uint32_t)buf_result_out->paddr;
		rsc_param.meta_out.iova = (uint32_t)buf_result_out->daddr;
	} else {
		dev_dbg(&pdev->dev, "meta out is null!\n");
		rsc_param.meta_out.pa = 0;
		rsc_param.meta_out.va = 0;
		rsc_param.meta_out.iova = 0;
	}

	dev_dbg(&pdev->dev,
		 "Delegate job to mtk_rsc_enqueue: pdev(0x%llx), frame(%d)\n",
		 (long long)pdev, bundle->id);
	ret = mtk_rsc_enqueue(pdev, &rsc_param);

	if (ret) {
		dev_dbg(&pdev->dev, "mtk_rsc_enqueue failed: %d\n", ret);
		return -EBUSY;
	}

	return 0;
}

static void debug_bundle(struct mtk_rsc_ctx_frame_bundle *bundle_data)
{
	int i = 0;

	if (!bundle_data) {
		pr_debug("bundle_data is NULL\n");
		return;
	}

	pr_debug("bundle buf nums (%d, %d,%d,%d)\n",
		 bundle_data->num_img_capture_bufs,
		 bundle_data->num_img_output_bufs,
		 bundle_data->num_meta_capture_bufs,
		 bundle_data->num_meta_output_bufs);

	for (i = 0; i < 16 ; i++) {
		pr_debug("Bundle, buf[%d] = %llx\n",
			 i,
			 (unsigned long long)bundle_data->buffers[i]);
	}

	pr_debug("Bundle last idx: %d\n", bundle_data->last_index);
}

static struct mtk_rsc_ctx_frame_bundle *mtk_rsc_ctx_get_free_frame
	(struct mtk_rsc_ctx *dev_ctx)
{
	struct mtk_rsc_ctx_frame_bundle *frame_bundle = NULL;

	spin_lock(&dev_ctx->qlock);
	list_for_each_entry(frame_bundle,
			    &dev_ctx->free_frames.list, list){
		pr_debug("Check frame: state %d, new should be %d\n",
			 frame_bundle->state, MTK_RSC_CTX_FRAME_NEW);
		if (frame_bundle->state == MTK_RSC_CTX_FRAME_NEW) {
			frame_bundle->state = MTK_RSC_CTX_FRAME_PREPARED;
			pr_debug("Found free frame\n");
			spin_unlock(&dev_ctx->qlock);
			return frame_bundle;
		}
	}
	spin_unlock(&dev_ctx->qlock);
	pr_debug("Can't found any bundle is MTK_RSC_CTX_FRAME_NEW\n");
	return NULL;
}

static int mtk_rsc_ctx_process_frame
	(struct mtk_rsc_ctx *dev_ctx,
	 struct mtk_rsc_ctx_frame_bundle *frame_bundle)
{
	spin_lock(&dev_ctx->qlock);

	frame_bundle->state = MTK_RSC_CTX_FRAME_PROCESSING;
	list_del(&frame_bundle->list);
	list_add_tail(&frame_bundle->list, &dev_ctx->processing_frames.list);

	spin_unlock(&dev_ctx->qlock);
	return 0;
}

int mtk_rsc_ctx_trigger_job(struct mtk_rsc_ctx  *dev_ctx,
			   struct mtk_rsc_ctx_frame_bundle *bundle_data)
{
	/* Scan all buffers and filled the ipi frame data*/
	int i = 0;
	struct mtk_rsc_ctx_finish_param fram_param;

	struct mtk_rsc_ctx_frame_bundle *bundle =
		mtk_rsc_ctx_get_free_frame(dev_ctx);

	pr_debug("Bundle data: , ctx id:%d\n",
		 dev_ctx->ctx_id);

	debug_bundle(bundle_data);

	if (!bundle) {
		pr_debug("bundle can't be NULL\n");
		goto FAILE_JOB_NOT_TRIGGER;
	}
	if (!bundle_data) {
		pr_debug("bundle_data can't be NULL\n");
		goto FAILE_JOB_NOT_TRIGGER;
	}

	pr_debug("Copy bundle_data->buffers to bundle->buffers\n");
	memcpy(bundle->buffers, bundle_data->buffers,
	       sizeof(struct mtk_rsc_ctx_buffer *) *
	       MTK_RSC_CTX_FRAME_BUNDLE_BUFFER_MAX);

	pr_debug("bundle setup (%d, %d,%d,%d)\n",
		 bundle_data->num_img_capture_bufs,
		 bundle_data->num_img_output_bufs,
		 bundle_data->num_meta_capture_bufs,
		 bundle_data->num_meta_output_bufs);

	bundle->num_img_capture_bufs = bundle_data->num_img_capture_bufs;
	bundle->num_img_output_bufs = bundle_data->num_img_output_bufs;
	bundle->num_meta_capture_bufs = bundle_data->num_meta_capture_bufs;
	bundle->num_meta_output_bufs = bundle_data->num_meta_output_bufs;
	bundle->id = mtk_rsc_ctx_next_global_frame_sequence(dev_ctx,
							   dev_ctx->ctx_id);
	bundle->last_index = dev_ctx->queues_attr.total_num - 1;

	debug_bundle(bundle);

	pr_debug("Fill Address data\n");
	for (i = 0; i <= bundle->last_index; i++) {
		struct mtk_rsc_ctx_buffer *ctx_buf = bundle->buffers[i];
		struct vb2_v4l2_buffer *b = NULL;

		pr_debug("Process queue[%d], ctx_buf:(%llx)\n",
			 i, (unsigned long long)ctx_buf);

		if (!ctx_buf) {
			pr_debug("queue[%d], ctx_buf is NULL!!\n", i);
			continue;
		}

		pr_debug("Get VB2 V4L2 buffer\n");
		b = mtk_rsc_ctx_buffer_get_vb2_v4l2_buffer(ctx_buf);

		ctx_buf->image = dev_ctx->queue[ctx_buf->queue].desc.image;
		ctx_buf->capture = dev_ctx->queue[ctx_buf->queue].desc.capture;
		/* copy the fmt setting for queue's fmt*/
		ctx_buf->fmt = dev_ctx->queue[ctx_buf->queue].fmt;
		ctx_buf->ctx_fmt = dev_ctx->queue[ctx_buf->queue].ctx_fmt;
			ctx_buf->frame_id = bundle->id;
		ctx_buf->daddr =
			vb2_dma_contig_plane_dma_addr(&b->vb2_buf, 0);
		pr_debug("%s:vb2_buf: type(%d),idx(%d),mem(%d)\n",
			 __func__,
			 b->vb2_buf.type,
			 b->vb2_buf.index,
			 b->vb2_buf.memory);
		ctx_buf->vaddr = vb2_plane_vaddr(&b->vb2_buf, 0);
		ctx_buf->buffer_usage = dev_ctx->queue[i].buffer_usage;
		ctx_buf->rotation = dev_ctx->queue[i].rotation;
		pr_debug("Buf: queue(%d), vaddr(%llx), daddr(%llx)",
			 ctx_buf->queue, (unsigned long long)ctx_buf->vaddr,
			 (unsigned long long)ctx_buf->daddr);

		if (!ctx_buf->image) {
#if KERNEL_VERSION(4, 5, 0) >= KERNEL_VERSION(4, 14, 0)
			ctx_buf->paddr =
				mtk_rsc_smem_iova_to_phys
					(&dev_ctx->smem_device->dev,
					 ctx_buf->daddr);
#endif
		} else {
			pr_debug("No pa: it is a image buffer\n");
			ctx_buf->paddr = 0;
		}
		ctx_buf->state = MTK_RSC_CTX_BUFFER_PROCESSING;
	}

	if (mtk_rsc_ctx_process_frame(dev_ctx, bundle)) {
		pr_debug("mtk_rsc_ctx_process_frame failed: frame(%d)\n",
		       bundle->id);
		goto FAILE_JOB_NOT_TRIGGER;
	}

	if (dev_ctx->mode == MTK_RSC_CTX_MODE_DEBUG_BYPASS_JOB_TRIGGER) {
		memset(&fram_param, 0,
			sizeof(struct mtk_rsc_ctx_finish_param));
		fram_param.frame_id = bundle->id;
		fram_param.state = MTK_RSC_CTX_FRAME_DATA_DONE;
		pr_debug("Ctx(%d) in HW bypass mode, will not trigger hw\n",
			 dev_ctx->ctx_id);

		mtk_rsc_ctx_core_job_finish(dev_ctx, (void *)&fram_param);
		return 0;
	}

	if (mtk_rsc_ctx_core_job_start(dev_ctx, bundle))
		goto FAILE_JOB_NOT_TRIGGER;
	return 0;

FAILE_JOB_NOT_TRIGGER:
	pr_debug("FAILE_JOB_NOT_TRIGGER: init fram_param: %llx\n",
		 (unsigned long long)&fram_param);
	memset(&fram_param, 0, sizeof(struct mtk_rsc_ctx_finish_param));
	fram_param.frame_id = bundle->id;
	fram_param.state = MTK_RSC_CTX_FRAME_DATA_ERROR;
	pr_debug("Call mtk_rsc_ctx_core_job_finish_cb: fram_param: %llx",
		 (unsigned long long)&fram_param);
	mtk_rsc_ctx_core_job_finish(dev_ctx, (void *)&fram_param);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(mtk_rsc_ctx_trigger_job);
