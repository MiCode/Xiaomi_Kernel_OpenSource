// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Fish Wu <fish.wu@mediatek.com>
 *
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-core.h>

#include <soc/mediatek/smi.h>

#include "mtk_aie.h"

#define V4L2_CID_MTK_AIE_INIT	(V4L2_CID_USER_MTK_FD_BASE + 1)
#define V4L2_CID_MTK_AIE_PARAM	(V4L2_CID_USER_MTK_FD_BASE + 2)
#define V4L2_CID_MTK_AIE_MAX	2

static const struct v4l2_pix_format_mplane mtk_aie_img_fmts[] = {
	{
		.pixelformat = V4L2_PIX_FMT_NV16M,
		.num_planes = 2,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV61M,
		.num_planes = 2,
	},
	{
		.pixelformat = V4L2_PIX_FMT_YUYV,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_YVYU,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_UYVY,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_VYUY,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_GREY,
		.num_planes = 1,
	},
};

#define NUM_FORMATS ARRAY_SIZE(mtk_aie_img_fmts)

static inline struct mtk_aie_ctx *fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_aie_ctx, fh);
}

static void mtk_aie_fill_init_param(struct mtk_aie_dev *fd,
				   struct user_init *user_init,
				   struct v4l2_ctrl_handler *hdl)
{
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(hdl, V4L2_CID_MTK_AIE_INIT);
	if (ctrl) {
		user_init->max_img_width = ctrl->p_new.p_u32[0];
		user_init->max_img_height = ctrl->p_new.p_u32[1];
		user_init->pyramid_width = ctrl->p_new.p_u32[2];
		user_init->pyramid_height = ctrl->p_new.p_u32[3];
		user_init->feature_thread = ctrl->p_new.p_u32[4];

		dev_dbg(fd->dev, "init param : max w:%d, max h:%d",
			user_init->max_img_width,
			user_init->max_img_height);
		dev_dbg(fd->dev, "init param : p_w%d, p_h:%d, f thread:%d",
			user_init->pyramid_width,
			user_init->pyramid_height,
			user_init->feature_thread);
	}
}

static int mtk_aie_hw_enable(struct mtk_aie_dev *fd)
{
	struct mtk_aie_ctx *ctx = fd->ctx;
	struct user_init user_init;
	struct aie_init_info init_info;

	/* initial value */
	mtk_aie_fill_init_param(fd, &user_init, &ctx->hdl);
	init_info.max_img_width = user_init.max_img_width;
	init_info.max_img_height = user_init.max_img_height;
	init_info.is_secure = 0;
	init_info.pyramid_height = user_init.pyramid_height;
	init_info.pyramid_width = user_init.pyramid_width;
	init_info.feature_threshold = (signed short)(user_init.feature_thread &
							0x0000FFFF);

	return aie_init(fd, init_info);
}

static void mtk_aie_hw_job_finish(struct mtk_aie_dev *fd,
				 enum vb2_buffer_state vb_state)
{
	struct mtk_aie_ctx *ctx;
	struct vb2_v4l2_buffer *src_vbuf = NULL, *dst_vbuf = NULL;

	pm_runtime_put(fd->dev);

	ctx = v4l2_m2m_get_curr_priv(fd->m2m_dev);
	src_vbuf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	dst_vbuf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);

	v4l2_m2m_buf_copy_metadata(src_vbuf, dst_vbuf,
				   V4L2_BUF_FLAG_TSTAMP_SRC_MASK);
	v4l2_m2m_buf_done(src_vbuf, vb_state);
	v4l2_m2m_buf_done(dst_vbuf, vb_state);
	v4l2_m2m_job_finish(fd->m2m_dev, ctx->fh.m2m_ctx);
	complete_all(&fd->fd_job_finished);
}

static void mtk_aie_hw_done(struct mtk_aie_dev *fd,
			   enum vb2_buffer_state vb_state)
{
	if (!cancel_delayed_work(&fd->job_timeout_work))
		return;

	mtk_aie_hw_job_finish(fd, vb_state);
}

static int mtk_aie_hw_connect(struct mtk_aie_dev *fd)
{
	fd->fd_stream_count++;
	if (fd->fd_stream_count == 1) {
		if (mtk_aie_hw_enable(fd))
			return -EINVAL;
	}
	return 0;
}

static void mtk_aie_hw_disconnect(struct mtk_aie_dev *fd)
{
	fd->fd_stream_count--;
	if (fd->fd_stream_count == 0)
		aie_uninit(fd);
}

static int mtk_aie_hw_job_exec(struct mtk_aie_dev *fd,
			       struct fd_enq_param *fd_param)
{
	pm_runtime_get_sync((fd->dev));

	reinit_completion(&fd->fd_job_finished);
	schedule_delayed_work(&fd->job_timeout_work,
			      msecs_to_jiffies(MTK_FD_HW_TIMEOUT));

	return 0;
}

static int mtk_aie_vb2_buf_out_validate(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *v4l2_buf = to_vb2_v4l2_buffer(vb);

	if (v4l2_buf->field == V4L2_FIELD_ANY)
		v4l2_buf->field = V4L2_FIELD_NONE;
	if (v4l2_buf->field != V4L2_FIELD_NONE)
		return -EINVAL;

	return 0;
}

static int mtk_aie_vb2_buf_prepare(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct vb2_queue *vq = vb->vb2_queue;
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vq);
	struct device *dev = ctx->dev;
	struct v4l2_pix_format_mplane *pixfmt;

	switch (vq->type) {
	case V4L2_BUF_TYPE_META_CAPTURE:
		if (vb2_plane_size(vb, 0) < ctx->dst_fmt.buffersize) {
			dev_dbg(dev, "meta size %lu is too small\n",
				vb2_plane_size(vb, 0));
			return -EINVAL;
		}
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		pixfmt = &ctx->src_fmt;

		if (vbuf->field == V4L2_FIELD_ANY)
			vbuf->field = V4L2_FIELD_NONE;

		if (vb->num_planes > 2 || vbuf->field != V4L2_FIELD_NONE) {
			dev_dbg(dev, "plane %d or field %d not supported\n",
				vb->num_planes, vbuf->field);
			return -EINVAL;
		}

		if (vb2_plane_size(vb, 0) < pixfmt->plane_fmt[0].sizeimage) {
			dev_dbg(dev, "plane 0 %lu is too small than %x\n",
				vb2_plane_size(vb, 0),
				pixfmt->plane_fmt[0].sizeimage);
			return -EINVAL;
		}

		if (pixfmt->num_planes == 2 &&
			vb2_plane_size(vb, 1) < pixfmt->plane_fmt[1].sizeimage) {
			dev_dbg(dev, "plane 1 %lu is too small than %x\n",
				vb2_plane_size(vb, 1),
				pixfmt->plane_fmt[1].sizeimage);
			return -EINVAL;
		}
		break;
	}

	return 0;
}

static void mtk_aie_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);

	v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, vbuf);
}

static int mtk_aie_vb2_queue_setup(struct vb2_queue *vq,
				  unsigned int *num_buffers,
				  unsigned int *num_planes,
				  unsigned int sizes[],
				  struct device *alloc_devs[])
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vq);
	unsigned int size[2];
	unsigned int plane;

	switch (vq->type) {
	case V4L2_BUF_TYPE_META_CAPTURE:
		size[0] = ctx->dst_fmt.buffersize;
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		size[0] = ctx->src_fmt.plane_fmt[0].sizeimage;
		size[1] = ctx->src_fmt.plane_fmt[1].sizeimage;
		break;
	}

	if (*num_planes > 2)
		return -EINVAL;
	if (*num_planes == 0) {
		if (vq->type == V4L2_BUF_TYPE_META_CAPTURE) {
			sizes[0] = ctx->dst_fmt.buffersize;
			*num_planes = 1;
			return 0;
		}

		*num_planes = ctx->src_fmt.num_planes;
		if (*num_planes > 2)
			return -EINVAL;
		for (plane = 0; plane < *num_planes; plane++)
			sizes[plane] = ctx->src_fmt.plane_fmt[plane].sizeimage;

		return 0;
	}

	return 0;
}

static int mtk_aie_vb2_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vq);

	if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return mtk_aie_hw_connect(ctx->fd_dev);

	return 0;
}

static void mtk_aie_job_timeout_work(struct work_struct *work)
{
	struct mtk_aie_dev *fd =
		container_of(work, struct mtk_aie_dev, job_timeout_work.work);

	dev_dbg(fd->dev, "FD Job timeout!");
	aie_irqhandle(fd);
	aie_reset(fd);
	mtk_aie_hw_job_finish(fd, VB2_BUF_STATE_ERROR);
}

static void mtk_aie_job_wait_finish(struct mtk_aie_dev *fd)
{
	wait_for_completion(&fd->fd_job_finished);
}

static void mtk_aie_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vq);
	struct mtk_aie_dev *fd = ctx->fd_dev;
	struct vb2_v4l2_buffer *vb;
	struct v4l2_m2m_ctx *m2m_ctx = ctx->fh.m2m_ctx;
	struct v4l2_m2m_queue_ctx *queue_ctx;

	mtk_aie_job_wait_finish(fd);
	queue_ctx = V4L2_TYPE_IS_OUTPUT(vq->type) ?
					&m2m_ctx->out_q_ctx :
					&m2m_ctx->cap_q_ctx;
	while ((vb = v4l2_m2m_buf_remove(queue_ctx)))
		v4l2_m2m_buf_done(vb, VB2_BUF_STATE_ERROR);

	if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		mtk_aie_hw_disconnect(fd);
}

static void mtk_aie_vb2_request_complete(struct vb2_buffer *vb)
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);

	v4l2_ctrl_request_complete(vb->req_obj.req, &ctx->hdl);
}

static int mtk_aie_querycap(struct file *file, void *fh,
			   struct v4l2_capability *cap)
{
	struct mtk_aie_dev *fd = video_drvdata(file);
	struct device *dev = fd->dev;

	strscpy(cap->driver, dev_driver_string(dev), sizeof(cap->driver));
	strscpy(cap->card, dev_driver_string(dev), sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
		 dev_name(fd->dev));

	return 0;
}

static int mtk_aie_enum_fmt_out_mp(struct file *file, void *fh,
				  struct v4l2_fmtdesc *f)
{
	if (f->index >= NUM_FORMATS)
		return -EINVAL;

	f->pixelformat = mtk_aie_img_fmts[f->index].pixelformat;
	return 0;
}

static void mtk_aie_fill_pixfmt_mp(struct v4l2_pix_format_mplane *dfmt,
				  const struct v4l2_pix_format_mplane *sfmt)
{
	dfmt->field = V4L2_FIELD_NONE;
	dfmt->colorspace = V4L2_COLORSPACE_BT2020;
	dfmt->num_planes = sfmt->num_planes;
	dfmt->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	dfmt->quantization = V4L2_QUANTIZATION_DEFAULT;
	dfmt->xfer_func =
		V4L2_MAP_XFER_FUNC_DEFAULT(dfmt->colorspace);

	/* Keep user setting as possible */
	dfmt->width = clamp(dfmt->width, MTK_FD_OUTPUT_MIN_WIDTH, MTK_FD_OUTPUT_MAX_WIDTH);
	dfmt->height = clamp(dfmt->height, MTK_FD_OUTPUT_MIN_HEIGHT, MTK_FD_OUTPUT_MAX_HEIGHT);

	if (sfmt->num_planes == 2) {
		dfmt->plane_fmt[0].sizeimage = dfmt->height * dfmt->plane_fmt[0].bytesperline;
		dfmt->plane_fmt[1].sizeimage = dfmt->height * dfmt->plane_fmt[1].bytesperline;
	} else {
		dfmt->plane_fmt[0].sizeimage = dfmt->height * dfmt->plane_fmt[0].bytesperline;
	}
}

static const struct v4l2_pix_format_mplane *mtk_aie_find_fmt(u32 format)
{
	unsigned int i;

	for (i = 0; i < NUM_FORMATS; i++) {
		if (mtk_aie_img_fmts[i].pixelformat == format)
			return &mtk_aie_img_fmts[i];
	}

	return NULL;
}

static int mtk_aie_try_fmt_out_mp(struct file *file,
				 void *fh,
				 struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	const struct v4l2_pix_format_mplane *fmt;

	fmt = mtk_aie_find_fmt(pix_mp->pixelformat);
	if (!fmt)
		fmt = &mtk_aie_img_fmts[0];	/* Get default img fmt */

	mtk_aie_fill_pixfmt_mp(pix_mp, fmt);
	return 0;
}

static int mtk_aie_g_fmt_out_mp(struct file *file, void *fh,
				   struct v4l2_format *f)
{
	struct mtk_aie_ctx *ctx = fh_to_ctx(fh);

	f->fmt.pix_mp = ctx->src_fmt;

	return 0;
}

static int mtk_aie_s_fmt_out_mp(struct file *file, void *fh,
				   struct v4l2_format *f)
{
	struct mtk_aie_ctx *ctx = fh_to_ctx(fh);
	struct vb2_queue *vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);

	/* Change not allowed if queue is streaming. */
	if (vb2_is_streaming(vq)) {
		dev_dbg(ctx->dev, "Failed to set format, vb2 is busy\n");
		return -EBUSY;
	}

	mtk_aie_try_fmt_out_mp(file, fh, f);
	ctx->src_fmt = f->fmt.pix_mp;

	return 0;
}

static int mtk_aie_enum_fmt_meta_cap(struct file *file, void *fh,
					struct v4l2_fmtdesc *f)
{
	if (f->index)
		return -EINVAL;

	strscpy(f->description, "Face detection result",
		sizeof(f->description));

	f->pixelformat = V4L2_META_FMT_MTFD_RESULT;
	f->flags = 0;

	return 0;
}

static int mtk_aie_g_fmt_meta_cap(struct file *file, void *fh,
				 struct v4l2_format *f)
{
	f->fmt.meta.dataformat = V4L2_META_FMT_MTFD_RESULT;
	f->fmt.meta.buffersize = sizeof(struct aie_enq_info);

	return 0;
}

static const struct vb2_ops mtk_aie_vb2_ops = {
	.queue_setup = mtk_aie_vb2_queue_setup,
	.buf_out_validate = mtk_aie_vb2_buf_out_validate,
	.buf_prepare  = mtk_aie_vb2_buf_prepare,
	.buf_queue = mtk_aie_vb2_buf_queue,
	.start_streaming = mtk_aie_vb2_start_streaming,
	.stop_streaming = mtk_aie_vb2_stop_streaming,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_request_complete = mtk_aie_vb2_request_complete,
};

static const struct v4l2_ioctl_ops mtk_aie_v4l2_video_out_ioctl_ops = {
	.vidioc_querycap = mtk_aie_querycap,
	.vidioc_enum_fmt_vid_out = mtk_aie_enum_fmt_out_mp,
	.vidioc_g_fmt_vid_out_mplane = mtk_aie_g_fmt_out_mp,
	.vidioc_s_fmt_vid_out_mplane = mtk_aie_s_fmt_out_mp,
	.vidioc_try_fmt_vid_out_mplane = mtk_aie_try_fmt_out_mp,
	.vidioc_enum_fmt_meta_cap = mtk_aie_enum_fmt_meta_cap,
	.vidioc_g_fmt_meta_cap = mtk_aie_g_fmt_meta_cap,
	.vidioc_s_fmt_meta_cap = mtk_aie_g_fmt_meta_cap,
	.vidioc_try_fmt_meta_cap = mtk_aie_g_fmt_meta_cap,
	.vidioc_reqbufs = v4l2_m2m_ioctl_reqbufs,
	.vidioc_create_bufs = v4l2_m2m_ioctl_create_bufs,
	.vidioc_expbuf = v4l2_m2m_ioctl_expbuf,
	.vidioc_prepare_buf = v4l2_m2m_ioctl_prepare_buf,
	.vidioc_querybuf = v4l2_m2m_ioctl_querybuf,
	.vidioc_qbuf = v4l2_m2m_ioctl_qbuf,
	.vidioc_dqbuf = v4l2_m2m_ioctl_dqbuf,
	.vidioc_streamon = v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff = v4l2_m2m_ioctl_streamoff,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static int
mtk_aie_queue_init(void *priv, struct vb2_queue *src_vq,
		  struct vb2_queue *dst_vq)
{
	struct mtk_aie_ctx *ctx = priv;
	int ret;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	src_vq->supports_requests = true;
	src_vq->drv_priv = ctx;
	src_vq->ops = &mtk_aie_vb2_ops;
	src_vq->mem_ops = &vb2_dma_contig_memops;
	src_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->fd_dev->vfd_lock;
	src_vq->dev = ctx->fd_dev->v4l2_dev.dev;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_META_CAPTURE;
	dst_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	dst_vq->drv_priv = ctx;
	dst_vq->ops = &mtk_aie_vb2_ops;
	dst_vq->mem_ops = &vb2_dma_contig_memops;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->fd_dev->vfd_lock;
	dst_vq->dev = ctx->fd_dev->v4l2_dev.dev;

	return vb2_queue_init(dst_vq);
}

static struct v4l2_ctrl_config mtk_aie_controls[] = {
	{
		.id = V4L2_CID_MTK_AIE_INIT,
		.name = "FD detection init",
		.type = V4L2_CTRL_TYPE_U32,
		.min = 0,
		.max = 0xffffffff,
		.step = 1,
		.def = 0,
		.dims = { sizeof(struct user_init)/4 },
	},
	{
		.id = V4L2_CID_MTK_AIE_PARAM,
		.name = "FD detection param",
		.type = V4L2_CTRL_TYPE_U32,
		.min = 0,
		.max = 0xffffffff,
		.step = 1,
		.def = 0,
		.dims = { sizeof(struct user_param)/4 },
	},
};

static int mtk_aie_ctrls_setup(struct mtk_aie_ctx *ctx)
{
	struct v4l2_ctrl_handler *hdl = &ctx->hdl;
	int i;

	v4l2_ctrl_handler_init(hdl, V4L2_CID_MTK_AIE_MAX);
	if (hdl->error)
		return hdl->error;

	for (i = 0; i < ARRAY_SIZE(mtk_aie_controls); i++) {
		v4l2_ctrl_new_custom(hdl, &mtk_aie_controls[i], ctx);
		if (hdl->error) {
			v4l2_ctrl_handler_free(hdl);
			dev_dbg(ctx->dev, "Failed to register controls:%d", i);
			return hdl->error;
		}
	}

	ctx->fh.ctrl_handler = &ctx->hdl;
	v4l2_ctrl_handler_setup(hdl);

	return 0;
}

static void init_ctx_fmt(struct mtk_aie_ctx *ctx)
{
	struct v4l2_pix_format_mplane *src_fmt = &ctx->src_fmt;
	struct v4l2_meta_format *dst_fmt = &ctx->dst_fmt;

	/* Initialize M2M source fmt */
	src_fmt->width = MTK_FD_OUTPUT_MAX_WIDTH;
	src_fmt->height = MTK_FD_OUTPUT_MAX_HEIGHT;
	mtk_aie_fill_pixfmt_mp(src_fmt, &mtk_aie_img_fmts[0]);

	/* Initialize M2M destination fmt */
	dst_fmt->buffersize = sizeof(struct aie_enq_info);
	dst_fmt->dataformat = V4L2_META_FMT_MTFD_RESULT;
}

/*
 * V4L2 file operations.
 */
static int mtk_vfd_open(struct file *filp)
{
	struct mtk_aie_dev *fd = video_drvdata(filp);
	struct video_device *vdev = video_devdata(filp);
	struct mtk_aie_ctx *ctx;
	int ret;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->fd_dev = fd;
	ctx->dev = fd->dev;
	fd->ctx = ctx;

	v4l2_fh_init(&ctx->fh, vdev);
	filp->private_data = &ctx->fh;

	init_ctx_fmt(ctx);

	ret = mtk_aie_ctrls_setup(ctx);
	if (ret) {
		dev_dbg(ctx->dev, "Failed to set up controls:%d\n", ret);
		goto err_fh_exit;
	}

	ctx->fh.m2m_ctx = v4l2_m2m_ctx_init(fd->m2m_dev, ctx,
						&mtk_aie_queue_init);
	if (IS_ERR(ctx->fh.m2m_ctx)) {
		ret = PTR_ERR(ctx->fh.m2m_ctx);
		goto err_free_ctrl_handler;
	}

	v4l2_fh_add(&ctx->fh);

	return 0;

err_free_ctrl_handler:
	v4l2_ctrl_handler_free(&ctx->hdl);
err_fh_exit:
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);

	return ret;
}

static int mtk_vfd_release(struct file *filp)
{
	struct mtk_aie_ctx *ctx = container_of(filp->private_data,
						  struct mtk_aie_ctx, fh);

	v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);

	v4l2_ctrl_handler_free(&ctx->hdl);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);

	kfree(ctx);

	return 0;
}

static const struct v4l2_file_operations fd_video_fops = {
	.owner = THIS_MODULE,
	.open = mtk_vfd_open,
	.release = mtk_vfd_release,
	.poll = v4l2_m2m_fop_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap = v4l2_m2m_fop_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = v4l2_compat_ioctl32,
#endif

};

static void mtk_aie_fill_user_param(struct mtk_aie_dev *fd,
				   struct user_param *user_param,
				   struct v4l2_ctrl_handler *hdl)
{
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(hdl, V4L2_CID_MTK_AIE_PARAM);
	if (ctrl) {
		user_param->fd_mode = ctrl->p_new.p_u32[0];
		user_param->src_img_fmt = ctrl->p_new.p_u32[1];
		user_param->src_img_width = ctrl->p_new.p_u32[2];
		user_param->src_img_height = ctrl->p_new.p_u32[3];
		user_param->src_img_stride = ctrl->p_new.p_u32[4];
		user_param->rotate_degree = ctrl->p_new.p_u32[5];
		user_param->en_roi = ctrl->p_new.p_u32[6];
		user_param->src_roi_x1 = ctrl->p_new.p_u32[7];
		user_param->src_roi_y1 = ctrl->p_new.p_u32[8];
		user_param->src_roi_x2 = ctrl->p_new.p_u32[9];
		user_param->src_roi_y2 = ctrl->p_new.p_u32[10];
		user_param->en_padding = ctrl->p_new.p_u32[11];
		user_param->src_padding_left = ctrl->p_new.p_u32[12];
		user_param->src_padding_right = ctrl->p_new.p_u32[13];
		user_param->src_padding_down = ctrl->p_new.p_u32[14];
		user_param->src_padding_up = ctrl->p_new.p_u32[15];
	}
}

static void mtk_aie_device_run(void *priv)
{
	struct mtk_aie_ctx *ctx = priv;
	struct mtk_aie_dev *fd = ctx->fd_dev;
	struct vb2_v4l2_buffer *src_buf, *dst_buf;
	struct fd_enq_param fd_param;
	void *plane_vaddr;
	int ret = 0;

	src_buf = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);

	fd_param.src_img[0].dma_addr =
		vb2_dma_contig_plane_dma_addr(&src_buf->vb2_buf, 0);

	if (ctx->src_fmt.num_planes == 2) {
		fd_param.src_img[1].dma_addr =
			vb2_dma_contig_plane_dma_addr(&src_buf->vb2_buf, 1);
	}

	mtk_aie_fill_user_param(fd, &fd_param.user_param, &ctx->hdl);

	plane_vaddr = vb2_plane_vaddr(&dst_buf->vb2_buf, 0);
	fd->aie_cfg = (struct aie_enq_info *)plane_vaddr;

	fd->aie_cfg->sel_mode = fd_param.user_param.fd_mode;
	fd->aie_cfg->src_img_fmt = fd_param.user_param.src_img_fmt;
	fd->aie_cfg->src_img_width = fd_param.user_param.src_img_width;
	fd->aie_cfg->src_img_height = fd_param.user_param.src_img_height;
	fd->aie_cfg->src_img_stride = fd_param.user_param.src_img_stride;
	fd->aie_cfg->rotate_degree = fd_param.user_param.rotate_degree;
	fd->aie_cfg->en_roi = fd_param.user_param.en_roi;
	fd->aie_cfg->src_roi.x1 = fd_param.user_param.src_roi_x1;
	fd->aie_cfg->src_roi.y1 = fd_param.user_param.src_roi_y1;
	fd->aie_cfg->src_roi.x2 = fd_param.user_param.src_roi_x2;
	fd->aie_cfg->src_roi.y2 = fd_param.user_param.src_roi_y2;
	fd->aie_cfg->en_padding = fd_param.user_param.en_padding;
	fd->aie_cfg->src_padding.left = fd_param.user_param.src_padding_left;
	fd->aie_cfg->src_padding.right = fd_param.user_param.src_padding_right;
	fd->aie_cfg->src_padding.down = fd_param.user_param.src_padding_down;
	fd->aie_cfg->src_padding.up = fd_param.user_param.src_padding_up;

	fd->aie_cfg->src_img_addr = fd_param.src_img[0].dma_addr;
	fd->aie_cfg->src_img_addr_uv = fd_param.src_img[1].dma_addr;

	ret = aie_prepare(fd, fd->aie_cfg);

	/* Complete request controls if any */
	v4l2_ctrl_request_complete(src_buf->vb2_buf.req_obj.req, &ctx->hdl);

	mtk_aie_hw_job_exec(fd, &fd_param);

	if (ret) {
		dev_dbg(fd->dev, "Failed to prepare aie setting\n");
		return;
	}

	aie_execute(fd, fd->aie_cfg);
}

static struct v4l2_m2m_ops fd_m2m_ops = {
	.device_run = mtk_aie_device_run,
};

static const struct media_device_ops fd_m2m_media_ops = {
	.req_validate	= vb2_request_validate,
	.req_queue	= v4l2_m2m_request_queue,
};

static int mtk_aie_video_device_register(struct mtk_aie_dev *fd)
{
	struct video_device *vfd = &fd->vfd;
	struct v4l2_m2m_dev *m2m_dev = fd->m2m_dev;
	struct device *dev = fd->dev;
	int ret;

	vfd->fops = &fd_video_fops;
	vfd->release = video_device_release;
	vfd->lock = &fd->vfd_lock;
	vfd->v4l2_dev = &fd->v4l2_dev;
	vfd->vfl_dir = VFL_DIR_M2M;
	vfd->device_caps = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_OUTPUT_MPLANE |
		V4L2_CAP_META_CAPTURE;
	vfd->ioctl_ops = &mtk_aie_v4l2_video_out_ioctl_ops;

	strscpy(vfd->name, dev_driver_string(dev), sizeof(vfd->name));

	video_set_drvdata(vfd, fd);

	ret = video_register_device(vfd, VFL_TYPE_GRABBER, 0);
	if (ret) {
		dev_dbg(dev, "Failed to register video device\n");
		goto err_free_dev;
	}

	ret = v4l2_m2m_register_media_controller(m2m_dev, vfd,
					MEDIA_ENT_F_PROC_VIDEO_STATISTICS);
	if (ret) {
		dev_dbg(dev, "Failed to init mem2mem media controller\n");
		goto err_unreg_video;
	}
	return 0;

err_unreg_video:
	video_unregister_device(vfd);
err_free_dev:
	video_device_release(vfd);
	return ret;
}

static int mtk_aie_dev_larb_init(struct mtk_aie_dev *fd)
{
	struct device_node *node;
	struct platform_device *pdev;

	node = of_parse_phandle(fd->dev->of_node, "mediatek,larb", 0);
	if (!node)
		return -EINVAL;
	pdev = of_find_device_by_node(node);
	if (WARN_ON(!pdev)) {
		of_node_put(node);
		return -EINVAL;
	}
	of_node_put(node);

	fd->larb = &pdev->dev;

	return 0;
}

static int mtk_aie_dev_v4l2_init(struct mtk_aie_dev *fd)
{
	struct media_device *mdev = &fd->mdev;
	struct device *dev = fd->dev;
	int ret;

	ret = v4l2_device_register(dev, &fd->v4l2_dev);
	if (ret) {
		dev_dbg(dev, "Failed to register v4l2 device\n");
		return ret;
	}

	fd->m2m_dev = v4l2_m2m_init(&fd_m2m_ops);
	if (IS_ERR(fd->m2m_dev)) {
		dev_dbg(dev, "Failed to init mem2mem device\n");
		ret = PTR_ERR(fd->m2m_dev);
		goto err_unreg_v4l2_dev;
	}

	mdev->dev = dev;
	strscpy(mdev->model, dev_driver_string(dev), sizeof(mdev->model));
	snprintf(mdev->bus_info, sizeof(mdev->bus_info),
		 "platform:%s", dev_name(dev));
	media_device_init(mdev);
	mdev->ops = &fd_m2m_media_ops;
	fd->v4l2_dev.mdev = mdev;

	ret = mtk_aie_video_device_register(fd);
	if (ret) {
		dev_dbg(dev, "Failed to register video device\n");
		goto err_cleanup_mdev;
	}

	ret = media_device_register(mdev);
	if (ret) {
		dev_dbg(dev, "Failed to register mem2mem media device\n");
		goto err_unreg_vdev;
	}

	return 0;

err_unreg_vdev:
	v4l2_m2m_unregister_media_controller(fd->m2m_dev);
	video_unregister_device(&fd->vfd);
	video_device_release(&fd->vfd);
err_cleanup_mdev:
	media_device_cleanup(mdev);
	v4l2_m2m_release(fd->m2m_dev);
err_unreg_v4l2_dev:
	v4l2_device_unregister(&fd->v4l2_dev);
	return ret;
}

static void mtk_aie_dev_v4l2_release(struct mtk_aie_dev *fd)
{
	v4l2_m2m_unregister_media_controller(fd->m2m_dev);
	video_unregister_device(&fd->vfd);
	video_device_release(&fd->vfd);
	media_device_cleanup(&fd->mdev);
	v4l2_m2m_release(fd->m2m_dev);
	v4l2_device_unregister(&fd->v4l2_dev);
}

static irqreturn_t mtk_aie_irq(int irq, void *data)
{
	struct mtk_aie_dev *fd = (struct mtk_aie_dev *)data;
	static int fd_cnt;

	aie_irqhandle(fd);

	if (fd->aie_cfg->sel_mode == 0) {
		fd_cnt++;
		if (fd_cnt == 1) {
			aie_execute_pose(fd);
			return IRQ_HANDLED;
		}
		if (fd_cnt == 2)
			fd_cnt = 0;
	}

	if (fd->aie_cfg->sel_mode == 0)
		aie_get_fd_result(fd, fd->aie_cfg);
	else
		aie_get_attr_result(fd, fd->aie_cfg);

	mtk_aie_hw_done(fd, VB2_BUF_STATE_DONE);

	return IRQ_HANDLED;
}

static int mtk_aie_probe(struct platform_device *pdev)
{
	struct mtk_aie_dev *fd;
	struct device *dev = &pdev->dev;

	struct resource *res;
	int irq;
	int ret;

	fd = devm_kzalloc(&pdev->dev, sizeof(*fd), GFP_KERNEL);
	if (!fd)
		return -ENOMEM;

	if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34)))
		dev_dbg(dev, "%s: No suitable DMA available\n", __func__);

	dev_set_drvdata(dev, fd);
	fd->dev = dev;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_dbg(dev, "Failed to get irq by platform: %d\n", irq);
		return irq;
	}

	ret = devm_request_irq(dev, irq, mtk_aie_irq, IRQF_SHARED, dev_driver_string(dev), fd);
	if (ret) {
		dev_dbg(dev, "Failed to request irq\n");
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	fd->fd_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(fd->fd_base)) {
		dev_dbg(dev, "Failed to get fd reg base\n");
		return PTR_ERR(fd->fd_base);
	}

	ret = mtk_aie_dev_larb_init(fd);
	if (ret) {
		dev_dbg(dev, "Failed to init larb : %d\n", ret);
		return ret;
	}

	fd->fd_clk = devm_clk_get(dev, "aie");
	if (IS_ERR(fd->fd_clk)) {
		dev_dbg(dev, "Failed to get fd_clk_img_fd clock\n");
		return PTR_ERR(fd->fd_clk);
	}

	mutex_init(&fd->vfd_lock);
	init_completion(&fd->fd_job_finished);
	INIT_DELAYED_WORK(&fd->job_timeout_work, mtk_aie_job_timeout_work);
	pm_runtime_enable(dev);

	ret = mtk_aie_dev_v4l2_init(fd);
	if (ret) {
		dev_dbg(dev, "Failed to init v4l2 device: %d\n", ret);
		goto err_destroy_mutex;
	}
	dev_info(dev, "AIE : Success to %s\n", __func__);

	return 0;

err_destroy_mutex:
	mutex_destroy(&fd->vfd_lock);
	pm_runtime_disable(fd->dev);

	return ret;
}

static int mtk_aie_remove(struct platform_device *pdev)
{
	struct mtk_aie_dev *fd = dev_get_drvdata(&pdev->dev);

	mtk_aie_dev_v4l2_release(fd);
	pm_runtime_disable(&pdev->dev);
	mutex_destroy(&fd->vfd_lock);

	return 0;
}

static int mtk_aie_suspend(struct device *dev)
{
	struct mtk_aie_dev *fd = dev_get_drvdata(dev);

	if (pm_runtime_suspended(dev))
		return 0;

	v4l2_m2m_suspend(fd->m2m_dev);
	clk_disable_unprepare(fd->fd_clk);

	mtk_smi_larb_put(fd->larb);

	dev_dbg(dev, "%s:disable clock\n", __func__);

	return 0;
}

static int mtk_aie_resume(struct device *dev)
{
	struct mtk_aie_dev *fd = dev_get_drvdata(dev);
	int ret;

	if (pm_runtime_suspended(dev))
		return 0;

	ret = mtk_smi_larb_get(fd->larb);
	if (ret) {
		dev_dbg(dev, "mtk_smi_larb_get larbvdec fail %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(fd->fd_clk);
	if (ret) {
		dev_dbg(dev, "Failed to open fd clk:%d\n", ret);
		return ret;
	}

	v4l2_m2m_resume(fd->m2m_dev);

	return 0;
}

static int mtk_aie_runtime_suspend(struct device *dev)
{
	struct mtk_aie_dev *fd = dev_get_drvdata(dev);

	clk_disable_unprepare(fd->fd_clk);

	mtk_smi_larb_put(fd->larb);

	return 0;
}

static int mtk_aie_runtime_resume(struct device *dev)
{
	struct mtk_aie_dev *fd = dev_get_drvdata(dev);
	int ret;

	ret = mtk_smi_larb_get(fd->larb);
	if (ret) {
		dev_dbg(dev, "mtk_smi_larb_get larbvdec fail %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(fd->fd_clk);
	if (ret) {
		dev_dbg(dev, "Failed to open fd clk:%d\n", ret);
		return ret;
	}

	return 0;
}

static const struct dev_pm_ops mtk_aie_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_aie_suspend, mtk_aie_resume)
	SET_RUNTIME_PM_OPS(mtk_aie_runtime_suspend, mtk_aie_runtime_resume, NULL)
};

static const struct of_device_id mtk_aie_of_ids[] = {
	{ .compatible = "mediatek,aie-hw2.0", },
	{}
};
MODULE_DEVICE_TABLE(of, mtk_aie_of_ids);

static struct platform_driver mtk_aie_driver = {
	.probe   = mtk_aie_probe,
	.remove  = mtk_aie_remove,
	.driver  = {
		.name  = "mtk-aie-5.2",
		.of_match_table = of_match_ptr(mtk_aie_of_ids),
		.pm = &mtk_aie_pm_ops,
	}
};

module_platform_driver(mtk_aie_driver);
MODULE_AUTHOR("Fish Wu <fish.wu@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek AIE driver");
