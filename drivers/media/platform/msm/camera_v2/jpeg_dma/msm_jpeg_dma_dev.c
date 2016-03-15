/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/ion.h>
#include <linux/msm_ion.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-mem2mem.h>
#include <media/msm_jpeg_dma.h>

#include "msm_jpeg_dma_dev.h"
#include "msm_jpeg_dma_hw.h"
#include "cam_hw_ops.h"

#define MSM_JPEGDMA_DRV_NAME "msm_jpegdma"

/* Jpeg dma stream off timeout */
#define MSM_JPEGDMA_STREAM_OFF_TIMEOUT_MS 500

/* Jpeg dma formats lookup table */
static struct msm_jpegdma_format formats[] = {
	{
		.name = "Greyscale",
		.fourcc = V4L2_PIX_FMT_GREY,
		.depth = 8,
		.num_planes = 1,
		.colplane_h = 1,
		.colplane_v = 1,
		.h_align = 1,
		.v_align = 1,
		.planes[0] = JPEGDMA_PLANE_TYPE_Y,
	},
	{
		.name = "Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12,
		.depth = 12,
		.num_planes = 2,
		.colplane_h = 1,
		.colplane_v = 2,
		.h_align = 2,
		.v_align = 2,
		.planes[0] = JPEGDMA_PLANE_TYPE_Y,
		.planes[1] = JPEGDMA_PLANE_TYPE_CBCR,
	},
	{
		.name = "Y/CrCb 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV21,
		.depth = 12,
		.num_planes = 2,
		.colplane_h = 1,
		.colplane_v = 2,
		.h_align = 2,
		.v_align = 2,
		.planes[0] = JPEGDMA_PLANE_TYPE_Y,
		.planes[1] = JPEGDMA_PLANE_TYPE_CBCR,
	},
	{
		.name = "YVU 4:2:0 planar, YCrCb",
		.fourcc = V4L2_PIX_FMT_YVU420,
		.depth = 12,
		.num_planes = 3,
		.colplane_h = 1,
		.colplane_v = 4,
		.h_align = 2,
		.v_align = 2,
		.planes[0] = JPEGDMA_PLANE_TYPE_Y,
		.planes[1] = JPEGDMA_PLANE_TYPE_CR,
		.planes[2] = JPEGDMA_PLANE_TYPE_CB,
	},
	{
		.name = "YUV 4:2:0 planar, YCbCr",
		.fourcc = V4L2_PIX_FMT_YUV420,
		.depth = 12,
		.num_planes = 3,
		.colplane_h = 1,
		.colplane_v = 4,
		.h_align = 2,
		.v_align = 2,
		.planes[0] = JPEGDMA_PLANE_TYPE_Y,
		.planes[1] = JPEGDMA_PLANE_TYPE_CB,
		.planes[2] = JPEGDMA_PLANE_TYPE_CR,
	},
};

/*
 * msm_jpegdma_ctx_from_fh - Get dma context from v4l2 fh.
 * @fh: Pointer to v4l2 fh.
 */
static inline struct jpegdma_ctx *msm_jpegdma_ctx_from_fh(struct v4l2_fh *fh)
{
	return container_of(fh, struct jpegdma_ctx, fh);
}

/*
 * msm_jpegdma_get_next_config_idx - get next configuration index.
 * @ctx: Pointer to jpegdma context.
 */
static inline int msm_jpegdma_get_next_config_idx(struct jpegdma_ctx *ctx)
{
	return (ctx->config_idx + 1) % MSM_JPEGDMA_MAX_CONFIGS;
}

/*
 * msm_jpegdma_schedule_next_config - Schedule next configuration.
 * @ctx: Pointer to jpegdma context.
 */
static inline void msm_jpegdma_schedule_next_config(struct jpegdma_ctx *ctx)
{
	ctx->config_idx = (ctx->config_idx + 1) % MSM_JPEGDMA_MAX_CONFIGS;
}

/*
 * msm_jpegdma_get_format_idx - Get jpeg dma format lookup index.
 * @ctx: Pointer to dma ctx.
 * @f: v4l2 format.
 */
static int msm_jpegdma_get_format_idx(struct jpegdma_ctx *ctx,
	struct v4l2_format *f)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(formats); i++)
		if (formats[i].fourcc == f->fmt.pix.pixelformat)
			break;

	if (i == ARRAY_SIZE(formats))
		return -EINVAL;

	return i;
}

/*
 * msm_jpegdma_fill_size_from_ctx - Fill jpeg dma format lookup index.
 * @ctx: Pointer to dma ctx.
 * @size: Size config.
 */
static void msm_jpegdma_fill_size_from_ctx(struct jpegdma_ctx *ctx,
	struct msm_jpegdma_size_config *size)
{

	size->in_size.top = ctx->crop.top;
	size->in_size.left = ctx->crop.left;
	size->in_size.width = ctx->crop.width;
	size->in_size.height = ctx->crop.height;
	size->in_size.scanline = ctx->format_out.fmt.pix.height;
	size->in_size.stride = ctx->format_out.fmt.pix.bytesperline;

	size->out_size.top = 0;
	size->out_size.left = 0;
	size->out_size.width = ctx->format_cap.fmt.pix.width;
	size->out_size.height = ctx->format_cap.fmt.pix.height;
	size->out_size.scanline = ctx->format_cap.fmt.pix.height;
	size->out_size.stride = ctx->format_cap.fmt.pix.bytesperline;
}

/*
 * msm_jpegdma_align_format - Align jpeg dma format.
 * @f: v4l2 format.
 * @format_idx: format lookup index.
 */
static void msm_jpegdma_align_format(struct v4l2_format *f, int format_idx)
{
	unsigned int size_image;
	int i;

	if (f->fmt.pix.width > MSM_JPEGDMA_MAX_WIDTH)
		f->fmt.pix.width = MSM_JPEGDMA_MAX_WIDTH;

	if (f->fmt.pix.width < MSM_JPEGDMA_MIN_WIDTH)
		f->fmt.pix.width = MSM_JPEGDMA_MIN_WIDTH;

	if (f->fmt.pix.height > MSM_JPEGDMA_MAX_HEIGHT)
		f->fmt.pix.height = MSM_JPEGDMA_MAX_HEIGHT;

	if (f->fmt.pix.height < MSM_JPEGDMA_MIN_HEIGHT)
		f->fmt.pix.height = MSM_JPEGDMA_MIN_HEIGHT;

	if (formats[format_idx].h_align > 1)
		f->fmt.pix.width &= ~(formats[format_idx].h_align - 1);

	if (formats[format_idx].v_align > 1)
		f->fmt.pix.height &= ~(formats[format_idx].v_align - 1);

	if (f->fmt.pix.bytesperline < f->fmt.pix.width)
		f->fmt.pix.bytesperline = f->fmt.pix.width;

	f->fmt.pix.bytesperline = ALIGN(f->fmt.pix.bytesperline,
		MSM_JPEGDMA_STRIDE_ALIGN);

	f->fmt.pix.sizeimage = f->fmt.pix.bytesperline * f->fmt.pix.height;

	size_image = f->fmt.pix.bytesperline * f->fmt.pix.height;

	if (formats[format_idx].num_planes > 1)
		for (i = 1; i < formats[format_idx].num_planes; i++)
			size_image += (f->fmt.pix.bytesperline *
				(f->fmt.pix.height /
				formats[format_idx].colplane_v));

	f->fmt.pix.sizeimage = size_image;
	f->fmt.pix.field = V4L2_FIELD_NONE;
}

/*
 * msm_jpegdma_config_ok - Check if jpeg dma format is ok for processing.
 * @ctx: Pointer to dma ctx.
 */
static int msm_jpegdma_config_ok(struct jpegdma_ctx *ctx)
{
	int ret;
	int cap_idx;
	int out_idx;
	struct msm_jpegdma_size_config size;

	cap_idx = msm_jpegdma_get_format_idx(ctx, &ctx->format_cap);
	if (cap_idx < 0)
		return 0;

	out_idx = msm_jpegdma_get_format_idx(ctx, &ctx->format_out);
	if (out_idx < 0)
		return 0;

	/* jpeg dma can not convert formats */
	if (cap_idx != out_idx)
		return 0;

	msm_jpegdma_fill_size_from_ctx(ctx, &size);

	size.format = formats[ctx->format_idx];

	ret = msm_jpegdma_hw_check_config(ctx->jdma_device, &size);
	if (ret < 0)
		return 0;

	return 1;
}

/*
 * msm_jpegdma_update_hw_config - Update dma hw configuration/
 * @ctx: Pointer to dma ctx.
 */
static int msm_jpegdma_update_hw_config(struct jpegdma_ctx *ctx)
{
	struct msm_jpegdma_size_config size;
	int idx;
	int ret = 0;

	if (msm_jpegdma_config_ok(ctx)) {
		size.fps = ctx->timeperframe.denominator /
			ctx->timeperframe.numerator;

		size.in_offset = ctx->in_offset;
		size.out_offset = ctx->out_offset;

		size.format = formats[ctx->format_idx];

		msm_jpegdma_fill_size_from_ctx(ctx, &size);

		idx = msm_jpegdma_get_next_config_idx(ctx);

		ret = msm_jpegdma_hw_set_config(ctx->jdma_device,
			&size, &ctx->plane_config[idx]);
		if (ret < 0)
			dev_err(ctx->jdma_device->dev, "Can not get hw cfg\n");
		else
			ctx->pending_config = 1;
	}

	return ret;
}

/*
 * msm_jpegdma_queue_setup - vb2_ops queue_setup callback.
 * @q: Pointer to vb2 queue struct.
 * @fmt: Pointer to v4l2 format struct (NULL is valid argument).
 * @num_buffers: Pointer of number of buffers requested.
 * @num_planes: Pointer to number of planes requested.
 * @sizes: Array containing sizes of planes.
 * @alloc_ctxs: Array of allocated contexts for each plane.
 */
static int msm_jpegdma_queue_setup(struct vb2_queue *q,
	const struct v4l2_format *fmt,
	unsigned int *num_buffers, unsigned int *num_planes,
	unsigned int sizes[], void *alloc_ctxs[])
{
	struct jpegdma_ctx *ctx = vb2_get_drv_priv(q);

	if (NULL == fmt) {
		switch (q->type) {
		case V4L2_BUF_TYPE_VIDEO_OUTPUT:
			sizes[0] = ctx->format_out.fmt.pix.sizeimage;
			break;
		case V4L2_BUF_TYPE_VIDEO_CAPTURE:
			sizes[0] = ctx->format_cap.fmt.pix.sizeimage;
			break;
		default:
			return -EINVAL;
		}
	} else {
		sizes[0] = fmt->fmt.pix.sizeimage;
	}

	*num_planes = 1;
	alloc_ctxs[0] = ctx->jdma_device;

	return 0;
}

/*
 * msm_jpegdma_buf_queue - vb2_ops buf_queue callback.
 * @vb: Pointer to vb2 buffer struct.
 */
static void msm_jpegdma_buf_queue(struct vb2_buffer *vb)
{
	struct jpegdma_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);

	v4l2_m2m_buf_queue(ctx->m2m_ctx, vb);

	return;
}

/*
 * msm_jpegdma_start_streaming - vb2_ops start_streaming callback.
 * @q: Pointer to vb2 queue struct.
 * @count: Number of buffer queued before stream on call.
 */
static int msm_jpegdma_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct jpegdma_ctx *ctx = vb2_get_drv_priv(q);
	int ret;

	ret = msm_jpegdma_hw_get(ctx->jdma_device);
	if (ret < 0) {
		dev_err(ctx->jdma_device->dev, "Fail to get dma hw\n");
		return ret;
	}
	if (!atomic_read(&ctx->active)) {
		ret =  msm_jpegdma_update_hw_config(ctx);
		if (ret < 0) {
			dev_err(ctx->jdma_device->dev, "Fail to configure hw\n");
			return ret;
		}
		atomic_set(&ctx->active, 1);
	}

	return 0;
}

/*
 * msm_jpegdma_stop_streaming - vb2_ops stop_streaming callback.
 * @q: Pointer to vb2 queue struct.
 */
static void msm_jpegdma_stop_streaming(struct vb2_queue *q)
{
	struct jpegdma_ctx *ctx = vb2_get_drv_priv(q);
	unsigned long time;
	int ret = 0;

	atomic_set(&ctx->active, 0);

	time = wait_for_completion_timeout(&ctx->completion,
		msecs_to_jiffies(MSM_JPEGDMA_STREAM_OFF_TIMEOUT_MS));
	if (!time) {
		dev_err(ctx->jdma_device->dev, "Ctx wait timeout\n");
		ret = -ETIME;
	}

	if (ctx->jdma_device->ref_count > 0)
		msm_jpegdma_hw_put(ctx->jdma_device);
}

/* Videobuf2 queue callbacks. */
static struct vb2_ops msm_jpegdma_vb2_q_ops = {
	.queue_setup     = msm_jpegdma_queue_setup,
	.buf_queue       = msm_jpegdma_buf_queue,
	.start_streaming = msm_jpegdma_start_streaming,
	.stop_streaming  = msm_jpegdma_stop_streaming,
};

/*
 * msm_jpegdma_get_userptr - Map and get buffer handler for user pointer buffer.
 * @alloc_ctx: Contexts allocated in buf_setup.
 * @vaddr: Virtual addr passed from userpsace (in our case ion fd)
 * @size: Size of the buffer
 * @write: True if buffer will be used for writing the data.
 */
static void *msm_jpegdma_get_userptr(void *alloc_ctx,
	unsigned long vaddr, unsigned long size, int write)
{
	struct msm_jpegdma_device *dma = alloc_ctx;
	struct msm_jpegdma_buf_handle *buf;
	struct msm_jpeg_dma_buff __user *up_buff = compat_ptr(vaddr);
	struct msm_jpeg_dma_buff kp_buff;
	int ret;

	if (!access_ok(VERIFY_READ, up_buff,
		sizeof(struct msm_jpeg_dma_buff)) ||
		get_user(kp_buff.fd, &up_buff->fd)) {
		dev_err(dma->dev, "Error getting user data\n");
		return ERR_PTR(-ENOMEM);
	}

	if (!access_ok(VERIFY_WRITE, up_buff,
		sizeof(struct msm_jpeg_dma_buff)) ||
		put_user(kp_buff.fd, &up_buff->fd)) {
		dev_err(dma->dev, "Error putting user data\n");
		return ERR_PTR(-ENOMEM);
	}

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	ret = msm_jpegdma_hw_map_buffer(dma, kp_buff.fd, buf);
	if (ret < 0 || buf->size < size)
		goto error;

	return buf;
error:
	kzfree(buf);
	return ERR_PTR(-ENOMEM);
}

/*
 * msm_jpegdma_put_userptr - Unmap and free buffer handler.
 * @buf_priv: Buffer handler allocated get_userptr callback.
 */
static void msm_jpegdma_put_userptr(void *buf_priv)
{
	if (IS_ERR_OR_NULL(buf_priv))
		return;

	msm_jpegdma_hw_unmap_buffer(buf_priv);

	kzfree(buf_priv);
}

/* Videobuf2 memory callbacks. */
static struct vb2_mem_ops msm_jpegdma_vb2_mem_ops = {
	.get_userptr = msm_jpegdma_get_userptr,
	.put_userptr = msm_jpegdma_put_userptr,
};

/*
 * msm_jpegdma_queue_init - m2m_ops queue_setup callback.
 * @priv: Pointer to jpegdma ctx.
 * @src_vq: vb2 source queue.
 * @dst_vq: vb2 destination queue.
 */
static int msm_jpegdma_queue_init(void *priv, struct vb2_queue *src_vq,
	struct vb2_queue *dst_vq)
{
	struct jpegdma_ctx *ctx = priv;
	int ret;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	src_vq->io_modes = VB2_USERPTR;
	src_vq->drv_priv = ctx;
	src_vq->mem_ops = &msm_jpegdma_vb2_mem_ops;
	src_vq->ops = &msm_jpegdma_vb2_q_ops;
	src_vq->buf_struct_size = sizeof(struct vb2_buffer);
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;

	ret = vb2_queue_init(src_vq);
	if (ret) {
		dev_err(ctx->jdma_device->dev, "Can not init src queue\n");
		return ret;
	}

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	dst_vq->io_modes = VB2_USERPTR;
	dst_vq->drv_priv = ctx;
	dst_vq->mem_ops = &msm_jpegdma_vb2_mem_ops;
	dst_vq->ops = &msm_jpegdma_vb2_q_ops;
	dst_vq->buf_struct_size = sizeof(struct vb2_buffer);
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;

	ret = vb2_queue_init(dst_vq);
	if (ret) {
		dev_err(ctx->jdma_device->dev, "Can not init dst queue\n");
		return ret;
	}

	return 0;
}

/*
 * msm_jpegdma_open - Fd device open method.
 * @file: Pointer to file struct.
 */
static int msm_jpegdma_open(struct file *file)
{
	struct msm_jpegdma_device *device = video_drvdata(file);
	struct video_device *video = video_devdata(file);
	struct jpegdma_ctx *ctx;
	int ret;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->jdma_device = device;
	dev_dbg(ctx->jdma_device->dev, "Jpeg v4l2 dma open\n");
	/* Set ctx defaults */
	ctx->timeperframe.numerator = 1;
	ctx->timeperframe.denominator = MSM_JPEGDMA_DEFAULT_FPS;
	atomic_set(&ctx->active, 0);

	v4l2_fh_init(&ctx->fh, video);

	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);

	ctx->m2m_ctx = v4l2_m2m_ctx_init(device->m2m_dev,
		ctx, msm_jpegdma_queue_init);
	if (IS_ERR_OR_NULL(ctx->m2m_ctx)) {
		ret = PTR_ERR(ctx->m2m_ctx);
		goto error_m2m_init;
	}
	ret = cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_JPEG,
			CAM_AHB_SVS_VOTE);
	if (ret < 0) {
		pr_err("%s: failed to vote for AHB\n", __func__);
		goto ahb_vote_fail;
	}
	init_completion(&ctx->completion);
	complete_all(&ctx->completion);
	dev_dbg(ctx->jdma_device->dev, "Jpeg v4l2 dma open success\n");

	return 0;

ahb_vote_fail:
	v4l2_m2m_ctx_release(ctx->m2m_ctx);
error_m2m_init:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);
	return ret;
}

/*
 * msm_jpegdma_release - Fd device release method.
 * @file: Pointer to file struct.
 */
static int msm_jpegdma_release(struct file *file)
{
	struct jpegdma_ctx *ctx = msm_jpegdma_ctx_from_fh(file->private_data);

	/* release all the resources */
	if (ctx->jdma_device->ref_count > 0)
		msm_jpegdma_hw_put(ctx->jdma_device);

	atomic_set(&ctx->active, 0);
	complete_all(&ctx->completion);
	v4l2_m2m_ctx_release(ctx->m2m_ctx);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);

	if (cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_JPEG,
		CAM_AHB_SUSPEND_VOTE) < 0)
		pr_err("%s: failed to remove vote for AHB\n", __func__);

	return 0;
}

/*
 * msm_jpegdma_poll - Fd device pool method.
 * @file: Pointer to file struct.
 * @wait: Pointer to pool table struct.
 */
static unsigned int msm_jpegdma_poll(struct file *file,
	struct poll_table_struct *wait)
{
	struct jpegdma_ctx *ctx = msm_jpegdma_ctx_from_fh(file->private_data);

	return v4l2_m2m_poll(file, ctx->m2m_ctx, wait);
}

/* Dma device file operations callbacks */
static const struct v4l2_file_operations fd_fops = {
	.owner          = THIS_MODULE,
	.open           = msm_jpegdma_open,
	.release        = msm_jpegdma_release,
	.poll           = msm_jpegdma_poll,
	.unlocked_ioctl = video_ioctl2,
};

/*
 * msm_jpegdma_querycap - V4l2 ioctl query capability handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @cap: Pointer to v4l2_capability struct need to be filled.
 */
static int msm_jpegdma_querycap(struct file *file,
	void *fh, struct v4l2_capability *cap)
{
	cap->bus_info[0] = 0;
	strlcpy(cap->driver, MSM_JPEGDMA_DRV_NAME, sizeof(cap->driver));
	strlcpy(cap->card, MSM_JPEGDMA_DRV_NAME, sizeof(cap->card));
	cap->capabilities = V4L2_CAP_STREAMING |
		V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VIDEO_CAPTURE;

	return 0;
}

/*
 * msm_jpegdma_enum_fmt_vid_cap - V4l2 ioctl enumerate output format handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @f: Pointer to v4l2_fmtdesc struct need to be filled.
 */
static int msm_jpegdma_enum_fmt_vid_cap(struct file *file,
	void *fh, struct v4l2_fmtdesc *f)
{
	if (f->index >= ARRAY_SIZE(formats))
		return -EINVAL;

	f->pixelformat = formats[f->index].fourcc;
	strlcpy(f->description, formats[f->index].name,
		sizeof(f->description));

	return 0;
}

/*
 * msm_jpegdma_enum_fmt_vid_out - V4l2 ioctl enumerate capture format handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @f: Pointer to v4l2_fmtdesc struct need to be filled.
 */
static int msm_jpegdma_enum_fmt_vid_out(struct file *file,
	void *fh, struct v4l2_fmtdesc *f)
{
	if (f->index >= ARRAY_SIZE(formats))
		return -EINVAL;

	f->pixelformat = formats[f->index].fourcc;
	strlcpy(f->description, formats[f->index].name,
		sizeof(f->description));

	return 0;
}

/*
 * msm_jpegdma_g_fmt_cap - V4l2 ioctl get capture format handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @f: Pointer to v4l2_format struct need to be filled.
 */
static int msm_jpegdma_g_fmt_cap(struct file *file, void *fh,
	struct v4l2_format *f)
{
	struct jpegdma_ctx *ctx = msm_jpegdma_ctx_from_fh(fh);

	*f = ctx->format_cap;

	return 0;
}

/*
 * msm_jpegdma_g_fmt_out - V4l2 ioctl get output format handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @f: Pointer to v4l2_format struct need to be filled.
 */
static int msm_jpegdma_g_fmt_out(struct file *file, void *fh,
	struct v4l2_format *f)
{
	struct jpegdma_ctx *ctx = msm_jpegdma_ctx_from_fh(fh);

	*f = ctx->format_out;

	return 0;
}

/*
 * msm_jpegdma_try_fmt_vid_cap - V4l2 ioctl try capture format handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @f: Pointer to v4l2_format struct.
 */
static int msm_jpegdma_try_fmt_vid_cap(struct file *file,
	void *fh, struct v4l2_format *f)
{
	struct jpegdma_ctx *ctx = msm_jpegdma_ctx_from_fh(fh);

	msm_jpegdma_align_format(f, ctx->format_idx);

	return 0;
}

/*
 * msm_jpegdma_try_fmt_vid_out - V4l2 ioctl try output format handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @f: Pointer to v4l2_format struct.
 */
static int msm_jpegdma_try_fmt_vid_out(struct file *file,
	void *fh, struct v4l2_format *f)
{
	struct jpegdma_ctx *ctx = msm_jpegdma_ctx_from_fh(fh);

	msm_jpegdma_align_format(f, ctx->format_idx);

	return 0;
}

/*
 * msm_jpegdma_s_fmt_vid_cap - V4l2 ioctl set capture format handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @f: Pointer to v4l2_format struct.
 */
static int msm_jpegdma_s_fmt_vid_cap(struct file *file,
	void *fh, struct v4l2_format *f)
{
	struct jpegdma_ctx *ctx = msm_jpegdma_ctx_from_fh(fh);
	int ret;

	ret = msm_jpegdma_get_format_idx(ctx, f);
	if (ret < 0)
		return -EINVAL;

	ctx->format_idx = ret;

	msm_jpegdma_align_format(f, ctx->format_idx);

	/* Initialize crop with output height */
	ctx->crop.top = 0;
	ctx->crop.left = 0;
	ctx->crop.width = ctx->format_out.fmt.pix.width;
	ctx->crop.height = ctx->format_out.fmt.pix.height;

	ctx->format_cap = *f;

	return 0;
}

/*
 * msm_jpegdma_s_fmt_vid_out - V4l2 ioctl set output format handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @f: Pointer to v4l2_format struct.
 */
static int msm_jpegdma_s_fmt_vid_out(struct file *file,
	void *fh, struct v4l2_format *f)
{
	struct jpegdma_ctx *ctx = msm_jpegdma_ctx_from_fh(fh);
	int ret;

	ret = msm_jpegdma_get_format_idx(ctx, f);
	if (ret < 0)
		return -EINVAL;

	ctx->format_idx = ret;

	msm_jpegdma_align_format(f, ctx->format_idx);

	/* Initialize crop */
	ctx->crop.top = 0;
	ctx->crop.left = 0;
	ctx->crop.width = f->fmt.pix.width;
	ctx->crop.height = f->fmt.pix.height;

	ctx->format_out = *f;

	return 0;
}

/*
 * msm_jpegdma_reqbufs - V4l2 ioctl request buffers handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @req: Pointer to v4l2_requestbuffer struct.
 */
static int msm_jpegdma_reqbufs(struct file *file,
	void *fh, struct v4l2_requestbuffers *req)
{
	struct jpegdma_ctx *ctx = msm_jpegdma_ctx_from_fh(fh);

	return v4l2_m2m_reqbufs(file, ctx->m2m_ctx, req);
}

/*
 * msm_jpegdma_qbuf - V4l2 ioctl queue buffer handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @buf: Pointer to v4l2_buffer struct.
 */
static int msm_jpegdma_qbuf(struct file *file, void *fh,
	struct v4l2_buffer *buf)
{
	struct jpegdma_ctx *ctx = msm_jpegdma_ctx_from_fh(fh);
	struct msm_jpeg_dma_buff __user *up_buff = compat_ptr(buf->m.userptr);
	struct msm_jpeg_dma_buff kp_buff;
	int ret;

	if (!access_ok(VERIFY_READ, up_buff,
		sizeof(struct msm_jpeg_dma_buff)) ||
		get_user(kp_buff.fd, &up_buff->fd) ||
		get_user(kp_buff.offset, &up_buff->offset)) {
		dev_err(ctx->jdma_device->dev, "Error getting user data\n");
		return -EFAULT;
	}

	if (!access_ok(VERIFY_WRITE, up_buff,
		sizeof(struct msm_jpeg_dma_buff)) ||
		put_user(kp_buff.fd, &up_buff->fd) ||
		put_user(kp_buff.offset, &up_buff->offset)) {
		dev_err(ctx->jdma_device->dev, "Error putting user data\n");
		return -EFAULT;
	}

	switch (buf->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		ctx->in_offset = kp_buff.offset;
		dev_dbg(ctx->jdma_device->dev, "input buf offset %d\n",
			ctx->in_offset);
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		ctx->out_offset = kp_buff.offset;
		dev_dbg(ctx->jdma_device->dev, "output buf offset %d\n",
			ctx->out_offset);
		break;
	}

	if (atomic_read(&ctx->active))
		ret = msm_jpegdma_update_hw_config(ctx);

	ret = v4l2_m2m_qbuf(file, ctx->m2m_ctx, buf);
	if (ret < 0)
		dev_err(ctx->jdma_device->dev, "QBuf fail\n");

	return ret;
}

/*
 * msm_jpegdma_dqbuf - V4l2 ioctl dequeue buffer handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @buf: Pointer to v4l2_buffer struct.
 */
static int msm_jpegdma_dqbuf(struct file *file,
	void *fh, struct v4l2_buffer *buf)
{
	struct jpegdma_ctx *ctx = msm_jpegdma_ctx_from_fh(fh);

	return v4l2_m2m_dqbuf(file, ctx->m2m_ctx, buf);
}

/*
 * msm_jpegdma_streamon - V4l2 ioctl stream on handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @buf_type: V4l2 buffer type.
 */
static int msm_jpegdma_streamon(struct file *file,
	void *fh, enum v4l2_buf_type buf_type)
{
	struct jpegdma_ctx *ctx = msm_jpegdma_ctx_from_fh(fh);
	int ret;

	if (!msm_jpegdma_config_ok(ctx))
		return -EINVAL;

	ret = v4l2_m2m_streamon(file, ctx->m2m_ctx, buf_type);
	if (ret < 0)
		dev_err(ctx->jdma_device->dev, "Stream on fail\n");

	return ret;
}

/*
 * msm_jpegdma_streamoff - V4l2 ioctl stream off handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @buf_type: V4l2 buffer type.
 */
static int msm_jpegdma_streamoff(struct file *file,
	void *fh, enum v4l2_buf_type buf_type)
{
	struct jpegdma_ctx *ctx = msm_jpegdma_ctx_from_fh(fh);
	int ret;

	ret = v4l2_m2m_streamoff(file, ctx->m2m_ctx, buf_type);
	if (ret < 0)
		dev_err(ctx->jdma_device->dev, "Stream off fails\n");

	return ret;
}

/*
 * msm_jpegdma_cropcap - V4l2 ioctl crop capabilites.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @a: Pointer to v4l2_cropcap struct need to be set.
 */
static int msm_jpegdma_cropcap(struct file *file, void *fh,
	struct v4l2_cropcap *a)
{
	struct jpegdma_ctx *ctx = msm_jpegdma_ctx_from_fh(fh);
	struct v4l2_format *format;

	switch (a->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		format = &ctx->format_out;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		format = &ctx->format_cap;
		break;
	default:
		return -EINVAL;
	}

	a->bounds.top = 0;
	a->bounds.left = 0;
	a->bounds.width = format->fmt.pix.width;
	a->bounds.height = format->fmt.pix.height;

	a->defrect = ctx->crop;

	a->pixelaspect.numerator = 1;
	a->pixelaspect.denominator = 1;

	return 0;
}

/*
 * msm_jpegdma_g_crop - V4l2 ioctl get crop.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @crop: Pointer to v4l2_crop struct need to be set.
 */
static int msm_jpegdma_g_crop(struct file *file, void *fh,
	struct v4l2_crop *crop)
{
	struct jpegdma_ctx *ctx = msm_jpegdma_ctx_from_fh(fh);

	switch (crop->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		crop->c = ctx->crop;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		crop->c.left = 0;
		crop->c.top = 0;
		crop->c.width = ctx->format_cap.fmt.pix.width;
		crop->c.height = ctx->format_cap.fmt.pix.height;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/*
 * msm_jpegdma_s_crop - V4l2 ioctl set crop.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @crop: Pointer to v4l2_crop struct need to be set.
 */
static int msm_jpegdma_s_crop(struct file *file, void *fh,
	const struct v4l2_crop *crop)
{
	struct jpegdma_ctx *ctx = msm_jpegdma_ctx_from_fh(fh);
	int ret = 0;

	/* Crop is supported only for input buffers */
	if (crop->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;

	if (crop->c.left < 0 || crop->c.top < 0 ||
	    crop->c.height < 0 || crop->c.width < 0)
		return -EINVAL;

	/* Upscale is not supported */
	if (crop->c.width < ctx->format_cap.fmt.pix.width)
		return -EINVAL;

	if (crop->c.height < ctx->format_cap.fmt.pix.height)
		return -EINVAL;

	if (crop->c.width + crop->c.left > ctx->format_out.fmt.pix.width)
		return -EINVAL;

	if (crop->c.height + crop->c.top > ctx->format_out.fmt.pix.height)
		return -EINVAL;

	if (crop->c.width % formats[ctx->format_idx].h_align)
		return -EINVAL;

	if (crop->c.left % formats[ctx->format_idx].h_align)
		return -EINVAL;

	if (crop->c.height % formats[ctx->format_idx].v_align)
		return -EINVAL;

	if (crop->c.top % formats[ctx->format_idx].v_align)
		return -EINVAL;

	ctx->crop = crop->c;
	if (atomic_read(&ctx->active))
		ret = msm_jpegdma_update_hw_config(ctx);

	return ret;
}

/*
 * msm_jpegdma_g_crop - V4l2 ioctl get parm.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @a: Pointer to v4l2_streamparm struct need to be filled.
 */
static int msm_jpegdma_g_parm(struct file *file, void *fh,
	struct v4l2_streamparm *a)
{
	struct jpegdma_ctx *ctx = msm_jpegdma_ctx_from_fh(fh);

	/* Get param is supported only for input buffers */
	if (a->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;

	a->parm.output.capability = 0;
	a->parm.output.extendedmode = 0;
	a->parm.output.outputmode = 0;
	a->parm.output.writebuffers = 0;
	a->parm.output.timeperframe = ctx->timeperframe;

	return 0;
}

/*
 * msm_jpegdma_s_crop - V4l2 ioctl set parm.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @a: Pointer to v4l2_streamparm struct need to be set.
 */
static int msm_jpegdma_s_parm(struct file *file, void *fh,
	struct v4l2_streamparm *a)
{
	struct jpegdma_ctx *ctx = msm_jpegdma_ctx_from_fh(fh);
	/* Set param is supported only for input buffers */
	if (a->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;

	if (!a->parm.output.timeperframe.numerator ||
		!a->parm.output.timeperframe.denominator)
		return -EINVAL;

	/* Frame rate is not supported during streaming */
	if (atomic_read(&ctx->active))
		return -EINVAL;

	ctx->timeperframe = a->parm.output.timeperframe;
	return 0;
}

/* V4l2 ioctl handlers */
static const struct v4l2_ioctl_ops fd_ioctl_ops = {
	.vidioc_querycap          = msm_jpegdma_querycap,
	.vidioc_enum_fmt_vid_out  = msm_jpegdma_enum_fmt_vid_out,
	.vidioc_enum_fmt_vid_cap  = msm_jpegdma_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_out     = msm_jpegdma_g_fmt_out,
	.vidioc_g_fmt_vid_cap     = msm_jpegdma_g_fmt_cap,
	.vidioc_try_fmt_vid_out   = msm_jpegdma_try_fmt_vid_out,
	.vidioc_try_fmt_vid_cap   = msm_jpegdma_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_out     = msm_jpegdma_s_fmt_vid_out,
	.vidioc_s_fmt_vid_cap     = msm_jpegdma_s_fmt_vid_cap,
	.vidioc_reqbufs           = msm_jpegdma_reqbufs,
	.vidioc_qbuf              = msm_jpegdma_qbuf,
	.vidioc_dqbuf             = msm_jpegdma_dqbuf,
	.vidioc_streamon          = msm_jpegdma_streamon,
	.vidioc_streamoff         = msm_jpegdma_streamoff,
	.vidioc_cropcap           = msm_jpegdma_cropcap,
	.vidioc_g_crop            = msm_jpegdma_g_crop,
	.vidioc_s_crop            = msm_jpegdma_s_crop,
	.vidioc_g_parm            = msm_jpegdma_g_parm,
	.vidioc_s_parm            = msm_jpegdma_s_parm,
};

/*
 * msm_jpegdma_process_buffers - Start dma processing.
 * @ctx: Pointer dma context.
 * @src_buf: Pointer to Vb2 source buffer.
 * @dst_buf: Pointer to Vb2 destination buffer.
 */
static void msm_jpegdma_process_buffers(struct jpegdma_ctx *ctx,
	struct vb2_buffer *src_buf, struct vb2_buffer *dst_buf)
{
	struct msm_jpegdma_buf_handle *buf_handle;
	struct msm_jpegdma_addr addr;
	int plane_idx;
	int config_idx;

	buf_handle = dst_buf->planes[0].mem_priv;
	addr.out_addr = buf_handle->addr;

	buf_handle = src_buf->planes[0].mem_priv;
	addr.in_addr = buf_handle->addr;

	plane_idx = ctx->plane_idx;
	config_idx = ctx->config_idx;
	msm_jpegdma_hw_start(ctx->jdma_device, &addr,
		&ctx->plane_config[config_idx].plane[plane_idx],
		&ctx->plane_config[config_idx].speed);
}

/*
 * msm_jpegdma_device_run - Dma device run.
 * @priv: Pointer dma context.
 */
static void msm_jpegdma_device_run(void *priv)
{
	struct vb2_buffer *src_buf;
	struct vb2_buffer *dst_buf;
	struct jpegdma_ctx *ctx = priv;

	dev_dbg(ctx->jdma_device->dev, "Jpeg v4l2 dma device run E\n");

	dst_buf = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	src_buf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	if (src_buf == NULL || dst_buf == NULL) {
		dev_err(ctx->jdma_device->dev, "Error, buffer list empty\n");
		return;
	}

	if (ctx->pending_config) {
		msm_jpegdma_schedule_next_config(ctx);
		ctx->pending_config = 0;
	}

	msm_jpegdma_process_buffers(ctx, src_buf, dst_buf);
	dev_dbg(ctx->jdma_device->dev, "Jpeg v4l2 dma device run X\n");
}

/*
 * msm_jpegdma_job_abort - Dma abort job.
 * @priv: Pointer dma context.
 */
static void msm_jpegdma_job_abort(void *priv)
{
	struct jpegdma_ctx *ctx = priv;

	msm_jpegdma_hw_abort(ctx->jdma_device);
	v4l2_m2m_job_finish(ctx->jdma_device->m2m_dev, ctx->m2m_ctx);
}

/*
 * msm_jpegdma_job_ready - Dma check if job is ready
 * @priv: Pointer dma context.
 */
static int msm_jpegdma_job_ready(void *priv)
{
	struct jpegdma_ctx *ctx = priv;

	if (atomic_read(&ctx->active)) {
		init_completion(&ctx->completion);
		return 1;
	}
	return 0;
}

/* V4l2 mem2mem handlers */
static struct v4l2_m2m_ops msm_jpegdma_m2m_ops = {
	.device_run = msm_jpegdma_device_run,
	.job_abort = msm_jpegdma_job_abort,
	.job_ready = msm_jpegdma_job_ready,
};

/*
 * msm_jpegdma_isr_processing_done - Invoked by dma_hw when processing is done.
 * @dma: Pointer dma device.
 */
void msm_jpegdma_isr_processing_done(struct msm_jpegdma_device *dma)
{
	struct vb2_buffer *src_buf;
	struct vb2_buffer *dst_buf;
	struct jpegdma_ctx *ctx;

	mutex_lock(&dma->lock);

	ctx = v4l2_m2m_get_curr_priv(dma->m2m_dev);
	if (ctx) {
		ctx->plane_idx++;
		if (ctx->plane_idx >= formats[ctx->format_idx].num_planes) {
			src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
			dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
			if (src_buf == NULL || dst_buf == NULL) {
				dev_err(ctx->jdma_device->dev, "Error, buffer list empty\n");
				mutex_unlock(&dma->lock);
				return;
			}
			complete_all(&ctx->completion);
			ctx->plane_idx = 0;

			v4l2_m2m_buf_done(src_buf, VB2_BUF_STATE_DONE);
			v4l2_m2m_buf_done(dst_buf, VB2_BUF_STATE_DONE);
			v4l2_m2m_job_finish(ctx->jdma_device->m2m_dev,
				ctx->m2m_ctx);
		} else {
			dst_buf = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
			src_buf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
			if (src_buf == NULL || dst_buf == NULL) {
				dev_err(ctx->jdma_device->dev, "Error, buffer list empty\n");
				mutex_unlock(&dma->lock);
				return;
			}
			msm_jpegdma_process_buffers(ctx, src_buf, dst_buf);
		}
	}
	mutex_unlock(&dma->lock);
}

/*
 * jpegdma_probe - Dma device probe method.
 * @pdev: Pointer Dma platform device.
 */
static int jpegdma_probe(struct platform_device *pdev)
{
	struct msm_jpegdma_device *jpegdma;
	int ret;

	dev_dbg(&pdev->dev, "jpeg v4l2 DMA probed\n");
	/* Jpeg dma device struct */
	jpegdma = kzalloc(sizeof(struct msm_jpegdma_device), GFP_KERNEL);
	if (!jpegdma)
		return -ENOMEM;

	mutex_init(&jpegdma->lock);

	init_completion(&jpegdma->hw_reset_completion);
	init_completion(&jpegdma->hw_halt_completion);
	jpegdma->dev = &pdev->dev;
	jpegdma->pdev = pdev;

	if (pdev->dev.of_node)
		of_property_read_u32((&pdev->dev)->of_node, "cell-index",
			&pdev->id);

	/* Get resources */
	ret = msm_jpegdma_hw_get_mem_resources(pdev, jpegdma);
	if (ret < 0)
		goto error_mem_resources;

	/* get all the regulators */
	ret = msm_camera_get_regulator_info(pdev, &jpegdma->dma_vdd,
		&jpegdma->num_reg);
	if (ret < 0)
		goto error_get_regulators;

	/* get all the clocks */
	ret = msm_camera_get_clk_info(pdev, &jpegdma->jpeg_clk_info,
		&jpegdma->clk, &jpegdma->num_clk);
	if (ret < 0)
		goto error_get_clocks;

	ret = msm_jpegdma_hw_get_qos(jpegdma);
	if (ret < 0)
		goto error_qos_get;

	ret = msm_jpegdma_hw_get_vbif(jpegdma);
	if (ret < 0)
		goto error_vbif_get;

	ret = msm_jpegdma_hw_get_prefetch(jpegdma);
	if (ret < 0)
		goto error_prefetch_get;

	/* get the irq resource */
	jpegdma->irq = msm_camera_get_irq(pdev, "jpeg");
	if (!jpegdma->irq)
		goto error_hw_get_irq;

	switch (pdev->id) {
	case 3:
		jpegdma->bus_client = CAM_BUS_CLIENT_JPEG_DMA;
		break;
	default:
		pr_err("%s: invalid cell id :%d\n",
			__func__, pdev->id);
		goto error_reg_bus;
	}

	/* register bus client */
	ret = msm_camera_register_bus_client(pdev,
			jpegdma->bus_client);
	if (ret < 0) {
		pr_err("Fail to register bus client\n");
		ret = -EINVAL;
		goto error_reg_bus;
	}

	ret = msm_jpegdma_hw_get_capabilities(jpegdma);
	if (ret < 0)
		goto error_hw_get_cap;

	/* mem2mem device */
	jpegdma->m2m_dev = v4l2_m2m_init(&msm_jpegdma_m2m_ops);
	if (IS_ERR(jpegdma->m2m_dev)) {
		dev_err(&pdev->dev, "Failed to init mem2mem device\n");
		ret = PTR_ERR(jpegdma->m2m_dev);
		goto error_m2m_init;
	}

	/* v4l2 device */
	ret = v4l2_device_register(&pdev->dev, &jpegdma->v4l2_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register v4l2 device\n");
		goto error_v4l2_register;
	}

	jpegdma->video.fops = &fd_fops;
	jpegdma->video.ioctl_ops = &fd_ioctl_ops;
	jpegdma->video.minor = -1;
	jpegdma->video.release = video_device_release;
	jpegdma->video.v4l2_dev = &jpegdma->v4l2_dev;
	jpegdma->video.vfl_dir = VFL_DIR_M2M;
	jpegdma->video.vfl_type = VFL_TYPE_GRABBER;
	strlcpy(jpegdma->video.name, MSM_JPEGDMA_DRV_NAME,
		sizeof(jpegdma->video.name));

	ret = video_register_device(&jpegdma->video, VFL_TYPE_GRABBER, -1);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register video device\n");
		goto error_video_register;
	}

	video_set_drvdata(&jpegdma->video, jpegdma);

	platform_set_drvdata(pdev, jpegdma);

	dev_dbg(&pdev->dev, "jpeg v4l2 DMA probe success\n");
	return 0;

error_video_register:
	v4l2_device_unregister(&jpegdma->v4l2_dev);
error_v4l2_register:
	v4l2_m2m_release(jpegdma->m2m_dev);
error_m2m_init:
error_hw_get_cap:
	msm_camera_unregister_bus_client(jpegdma->bus_client);
error_reg_bus:
error_hw_get_irq:
	msm_jpegdma_hw_put_prefetch(jpegdma);
error_prefetch_get:
	msm_jpegdma_hw_put_vbif(jpegdma);
error_vbif_get:
	msm_jpegdma_hw_put_qos(jpegdma);
error_qos_get:
	msm_camera_put_clk_info(pdev, &jpegdma->jpeg_clk_info,
		&jpegdma->clk, jpegdma->num_clk);
error_get_clocks:
	msm_camera_put_regulators(pdev, &jpegdma->dma_vdd,
		jpegdma->num_reg);
error_get_regulators:
	msm_jpegdma_hw_release_mem_resources(jpegdma);
error_mem_resources:
	kfree(jpegdma);
	return ret;
}

/*
 * jpegdma_device_remove - Jpegdma device remove method.
 * @pdev: Pointer jpegdma platform device.
 */
static int jpegdma_device_remove(struct platform_device *pdev)
{
	struct msm_jpegdma_device *dma;

	dma = platform_get_drvdata(pdev);
	if (NULL == dma) {
		dev_err(&pdev->dev, "Can not get jpeg dma drvdata\n");
		return 0;
	}
	video_unregister_device(&dma->video);
	v4l2_device_unregister(&dma->v4l2_dev);
	v4l2_m2m_release(dma->m2m_dev);
	/* unregister bus client */
	msm_camera_unregister_bus_client(dma->bus_client);
	/* release all the regulators */
	msm_camera_put_regulators(dma->pdev, &dma->dma_vdd,
		dma->num_reg);
	/* release all the clocks */
	msm_camera_put_clk_info(dma->pdev, &dma->jpeg_clk_info,
		&dma->clk, dma->num_clk);
	msm_jpegdma_hw_release_mem_resources(dma);
	kfree(dma);

	return 0;
}

/* Device tree match struct */
static const struct of_device_id msm_jpegdma_dt_match[] = {
	{.compatible = "qcom,jpegdma"},
	{}
};

/* Jpeg dma platform driver definition */
static struct platform_driver jpegdma_driver = {
	.probe = jpegdma_probe,
	.remove = jpegdma_device_remove,
	.driver = {
		.name = MSM_JPEGDMA_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_jpegdma_dt_match,
	},
};

static int __init msm_jpegdma_init_module(void)
{
	return platform_driver_register(&jpegdma_driver);
}

static void __exit msm_jpegdma_exit_module(void)
{
	platform_driver_unregister(&jpegdma_driver);
}

module_init(msm_jpegdma_init_module);
module_exit(msm_jpegdma_exit_module);
MODULE_DESCRIPTION("MSM JPEG DMA driver");
