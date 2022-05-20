// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>

#include "mtk_cam.h"
#include "mtk_cam-video.h"
#include "mtk_cam-fmt_utils.h"
#include "mtk_camera-v4l2-controls.h"
#include "mtk_camera-videodev2.h"
#include "mtk_cam-ufbc-def.h"
#include "mtk_cam-plat.h"
#include "mtk_cam_vb2-dma-contig.h"
#include "mtk_cam-trace.h"

#define MAX_SUBSAMPLE_PLANE_NUM VB2_MAX_PLANES /* 8 */

static int mtk_cam_video_set_fmt(struct mtk_cam_video_device *node,
				 struct v4l2_format *f);

static int mtk_cam_vb2_queue_setup(struct vb2_queue *vq,
				   unsigned int *num_buffers,
				   unsigned int *num_planes,
				   unsigned int sizes[],
				   struct device *alloc_devs[])
{
	struct mtk_cam_device *cam = vb2_get_drv_priv(vq);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vq);
	unsigned int max_buffer_count = node->desc.max_buf_count;
	const struct v4l2_format *fmt = &node->active_fmt;
	unsigned int size;
	int i;
	int min_buf_sz;

	min_buf_sz = ALIGN(IMG_MIN_WIDTH, IMG_PIX_ALIGN) * IMG_MIN_HEIGHT;

	/* Check the limitation of buffer size */
	if (max_buffer_count)
		*num_buffers = clamp_val(*num_buffers, 1, max_buffer_count);

	if (node->desc.smem_alloc)
		vq->dma_attrs |= DMA_ATTR_NO_KERNEL_MAPPING;

	if (vq->type == V4L2_BUF_TYPE_META_OUTPUT ||
	    vq->type == V4L2_BUF_TYPE_META_CAPTURE)
		size = fmt->fmt.meta.buffersize;
	else
		size = min_buf_sz;

	/* Add for q.create_bufs with fmt.g_sizeimage(p) / 2 test */
	if (*num_planes) {
		if (sizes[0] < size || *num_planes != 1)
			return -EINVAL;
	} else {
		*num_planes = 1;
		sizes[0] = size;

		for (i = 0; i < *num_planes; i++)
			dev_dbg(cam->dev, "[%s] id:%d, name:%s, np:%d, i:%d, size:%d\n",
				__func__,
				node->desc.id, node->desc.name,
				*num_planes, i, sizes[i]);
	}
	return 0;
}

static int mtk_cam_vb2_buf_init(struct vb2_buffer *vb)
{
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	struct device *dev = vb->vb2_queue->dev;
	struct mtk_cam_buffer *buf;
	dma_addr_t addr;

	buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	buf->daddr = vb2_dma_contig_plane_dma_addr(vb, 0);
	buf->scp_addr = 0;

	/* SCP address is only valid for meta input buffer */
	if (!node->desc.smem_alloc)
		return 0;

	buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	/* Use coherent address to get iova address */
	addr = dma_map_resource(dev, buf->daddr, vb->planes[0].length,
				DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);
	if (dma_mapping_error(dev, addr)) {
		dev_info(dev, "failed to map meta addr:%pad\n", &buf->daddr);
		return -EFAULT;
	}
	buf->scp_addr = buf->daddr;
	buf->daddr = addr;

	return 0;
}

static void mtk_cam_vb2_buf_collect_image_info(struct vb2_buffer *vb)
{
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	struct mtk_cam_buffer *buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	struct mtk_cam_cached_image_info *cached = &buf->image_info;
	const struct v4l2_pix_format_mplane *mpfmt =
		&node->active_fmt.fmt.pix_mp;
	int i;

	cached->v4l2_pixelformat = mpfmt->pixelformat;
	cached->width = mpfmt->width;
	cached->height = mpfmt->height;
	for (i = 0; i < ARRAY_SIZE(cached->bytesperline); i++)
		cached->bytesperline[i] = i >= mpfmt->num_planes ? 0 :
			mpfmt->plane_fmt[i].bytesperline;
	cached->crop = node->active_crop.r;
}

static int mtk_cam_vb2_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	struct vb2_v4l2_buffer *v4l2_buf = to_vb2_v4l2_buffer(vb);
	const struct v4l2_format *fmt = &node->active_fmt;
	unsigned int size;
	unsigned int plane;

	if (node->desc.need_cache_sync_on_prepare) {
		dev_dbg(vb->vb2_queue->dev, "%s: %s\n",
			__func__, node->desc.name);
		for (plane = 0; plane < vb->num_planes; ++plane)
			mtk_cam_vb2_sync_for_device(vb->planes[plane].mem_priv);
	}

	if (vb->vb2_queue->type == V4L2_BUF_TYPE_META_OUTPUT ||
	    vb->vb2_queue->type == V4L2_BUF_TYPE_META_CAPTURE)
		size = fmt->fmt.meta.buffersize;
	else
		size = fmt->fmt.pix_mp.plane_fmt[0].sizeimage;

	if (vb2_plane_size(vb, 0) < size) {
		dev_info(vb->vb2_queue->dev, "plane size is too small:%lu<%u\n",
			vb2_plane_size(vb, 0), size);
		/* return -EINVAL; */
	}

	if (V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		if ((vb2_get_plane_payload(vb, 0) != size) && (vb->vb2_queue->streaming)) {
			dev_dbg(vb->vb2_queue->dev, "plane payload is mismatch:%lu:%u\n",
				vb2_get_plane_payload(vb, 0), size);
			/* todo: user must set correct byteused */
			/* return -EINVAL;*/
		}
		return 0;
	}

	v4l2_buf->field = V4L2_FIELD_NONE;
	vb2_set_plane_payload(vb, 0, size);

	if (node->desc.image)
		mtk_cam_vb2_buf_collect_image_info(vb);

	return 0;
}

static void mtk_cam_vb2_buf_finish(struct vb2_buffer *vb)
{
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	unsigned int plane;

	if (node->desc.need_cache_sync_on_finish) {
		dev_dbg(vb->vb2_queue->dev, "%s: %s\n",
			__func__, node->desc.name);
		for (plane = 0; plane < vb->num_planes; ++plane)
			mtk_cam_vb2_sync_for_cpu(vb->planes[plane].mem_priv);
	}
}

static int check_node_linked(struct vb2_queue *vq)
{
	struct mtk_cam_device *cam = vb2_get_drv_priv(vq);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vq);
	int ret = 0;

	if (!node->enabled) {
		dev_info(cam->dev,
			"%s: stream on failed, node is not enabled\n",
			node->desc.name);
		ret = -ENOLINK;
	}

	return ret;
}

static int mtk_cam_vb2_start_streaming(struct vb2_queue *vq,
				       unsigned int count)
{
	struct mtk_cam_device *cam = vb2_get_drv_priv(vq);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vq);
	struct mtk_cam_ctx *ctx;
	int ret;

	dev_info(cam->dev, "%s: node %s\n", __func__, node->desc.name);

	ret = check_node_linked(vq);
	if (ret)
		return ret;

	ctx = mtk_cam_start_ctx(cam, node);
	if (!ctx)
		return -EPIPE;

	++ctx->streaming_node_cnt;
	if (mtk_cam_ctx_all_nodes_streaming(ctx))
		mtk_cam_ctx_stream_on(ctx);

	return 0;
}

static void mtk_cam_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct mtk_cam_device *cam = vb2_get_drv_priv(vq);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vq);
	struct mtk_cam_ctx *ctx;

	ctx = mtk_cam_find_ctx(cam, &node->vdev.entity);
	if (WARN_ON(!ctx)) {
		// TODO: clean pending?
		return;
	}

	if (mtk_cam_ctx_all_nodes_streaming(ctx))
		mtk_cam_ctx_stream_off(ctx);
	--ctx->streaming_node_cnt;

	// TODO: clean pending req?

	if (!mtk_cam_ctx_all_nodes_idle(ctx))
		return;

	mtk_cam_stop_ctx(ctx, &node->vdev.entity);
}

const struct mtk_format_info *mtk_format_info(u32 format)
{
	static const struct mtk_format_info formats[] = {
		/* YUV planar formats */
		{ .format = V4L2_PIX_FMT_NV12_10,  .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 2, .bit_r_den = 1 },
		{ .format = V4L2_PIX_FMT_NV21_10,  .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 2, .bit_r_den = 1 },
		{ .format = V4L2_PIX_FMT_NV16_10,  .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 2, .bit_r_den = 1 },
		{ .format = V4L2_PIX_FMT_NV61_10,  .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 2, .bit_r_den = 1 },
		{ .format = V4L2_PIX_FMT_YUYV10,	  .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 2, .bit_r_den = 1 },
		{ .format = V4L2_PIX_FMT_YVYU10,	  .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 2, .bit_r_den = 1 },
		{ .format = V4L2_PIX_FMT_UYVY10,	  .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 2, .bit_r_den = 1 },
		{ .format = V4L2_PIX_FMT_VYUY10,	  .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 2, .bit_r_den = 1 },
		{ .format = V4L2_PIX_FMT_NV12_12,  .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 2, .bit_r_den = 1 },
		{ .format = V4L2_PIX_FMT_NV21_12,  .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 2, .bit_r_den = 1 },
		{ .format = V4L2_PIX_FMT_NV16_12,  .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 2, .bit_r_den = 1 },
		{ .format = V4L2_PIX_FMT_NV61_12,  .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 2, .bit_r_den = 1 },
		{ .format = V4L2_PIX_FMT_YUYV12,	  .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 2, .bit_r_den = 1 },
		{ .format = V4L2_PIX_FMT_YVYU12,	  .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 2, .bit_r_den = 1 },
		{ .format = V4L2_PIX_FMT_UYVY12,	  .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 2, .bit_r_den = 1 },
		{ .format = V4L2_PIX_FMT_VYUY12,	  .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 2, .bit_r_den = 1 },
		/* YUV packed formats */
		{ .format = V4L2_PIX_FMT_MTISP_YUYV10P,	  .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 5, .bit_r_den = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_YVYU10P,	  .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 5, .bit_r_den = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_UYVY10P,	  .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 5, .bit_r_den = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_VYUY10P,	  .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 5, .bit_r_den = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_NV12_10P, .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 5, .bit_r_den = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_NV21_10P, .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 5, .bit_r_den = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_NV16_10P, .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 5, .bit_r_den = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_NV61_10P, .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 5, .bit_r_den = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_YUYV12P,	  .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 3, .bit_r_den = 2 },
		{ .format = V4L2_PIX_FMT_MTISP_YVYU12P,	  .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 3, .bit_r_den = 2 },
		{ .format = V4L2_PIX_FMT_MTISP_UYVY12P,	  .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 3, .bit_r_den = 2 },
		{ .format = V4L2_PIX_FMT_MTISP_VYUY12P,	  .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 3, .bit_r_den = 2 },
		{ .format = V4L2_PIX_FMT_MTISP_NV12_12P, .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 3, .bit_r_den = 2 },
		{ .format = V4L2_PIX_FMT_MTISP_NV21_12P, .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 3, .bit_r_den = 2 },
		{ .format = V4L2_PIX_FMT_MTISP_NV16_12P, .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 3, .bit_r_den = 2 },
		{ .format = V4L2_PIX_FMT_MTISP_NV61_12P, .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bit_r_num = 3, .bit_r_den = 2 },
		/* YUV UFBC formats */
		{ .format = V4L2_PIX_FMT_MTISP_NV12_UFBC, .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 2, .bit_r_den = 1 },
		{ .format = V4L2_PIX_FMT_MTISP_NV21_UFBC, .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 2, .bit_r_den = 1 },
		{ .format = V4L2_PIX_FMT_MTISP_NV12_10_UFBC, .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 5, .bit_r_den = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_NV21_10_UFBC, .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 5, .bit_r_den = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_NV12_12_UFBC, .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 3, .bit_r_den = 2 },
		{ .format = V4L2_PIX_FMT_MTISP_NV21_12_UFBC, .mem_planes = 1, .comp_planes = 2,
			.bpp = { 1, 2, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 3, .bit_r_den = 2 },
		{ .format = V4L2_PIX_FMT_MTISP_BAYER8_UFBC, .mem_planes = 1, .comp_planes = 1,
			.bpp = { 1, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bit_r_num = 1, .bit_r_den = 1 },
		{ .format = V4L2_PIX_FMT_MTISP_BAYER10_UFBC, .mem_planes = 1, .comp_planes = 1,
			.bpp = { 1, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bit_r_num = 5, .bit_r_den = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_BAYER12_UFBC, .mem_planes = 1, .comp_planes = 1,
			.bpp = { 1, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bit_r_num = 3, .bit_r_den = 2 },
		{ .format = V4L2_PIX_FMT_MTISP_BAYER14_UFBC, .mem_planes = 1, .comp_planes = 1,
			.bpp = { 1, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bit_r_num = 7, .bit_r_den = 4 },
		/* Full-G RGB formats */
		{ .format = V4L2_PIX_FMT_MTISP_SGRB8F, .mem_planes = 1, .comp_planes = 3,
			.bpp = { 1, 1, 1, 0 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 1, .bit_r_den = 1 },
		{ .format = V4L2_PIX_FMT_MTISP_SGRB10F, .mem_planes = 1, .comp_planes = 3,
			.bpp = { 1, 1, 1, 0 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 5, .bit_r_den = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_SGRB12F, .mem_planes = 1, .comp_planes = 3,
			.bpp = { 1, 1, 1, 0 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 3, .bit_r_den = 2 },
	};
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(formats); ++i)
		if (formats[i].format == format)
			return &formats[i];
	return NULL;
}

static int _temp_ipi_pipe_id_to_idx(int pipe_id)
{
	pr_info("%s: update for 7s\n", __func__);

	if (is_raw_subdev(pipe_id))
		return pipe_id;

	if (is_mraw_subdev(pipe_id))
		return pipe_id - MTKCAM_SUBDEV_MRAW_START;

	if (is_camsv_subdev(pipe_id))
		return pipe_id - MTKCAM_SUBDEV_CAMSV_START;

	return -1;
}

void mtk_cam_mark_pipe_used(int *used_mask, int ipi_pipe_id)
{
	int pipe_idx;

	pipe_idx = _temp_ipi_pipe_id_to_idx(ipi_pipe_id);

	if (is_raw_subdev(ipi_pipe_id))
		USED_MASK_SET(used_mask, raw, pipe_idx);
	else if (is_camsv_subdev(ipi_pipe_id))
		USED_MASK_SET(used_mask, camsv, pipe_idx);
	else if (is_mraw_subdev(ipi_pipe_id))
		USED_MASK_SET(used_mask, mraw, pipe_idx);
	else
		pr_info("%s: wrong pipe id 0x%x\n", __func__, ipi_pipe_id);
}

static void mtk_cam_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_cam_buffer *buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	struct mtk_cam_request *req = to_mtk_cam_req(vb->request);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	int ipi_pipe_id = node->uid.pipe_id;

	if (WARN_ON(!req))
		return;

	mtk_cam_mark_pipe_used(&req->used_pipe, ipi_pipe_id);
	list_add_tail(&buf->list, &req->buf_list);
}

static void mtk_cam_vb2_buf_cleanup(struct vb2_buffer *vb)
{
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	struct mtk_cam_buffer *buf;
	struct device *dev = vb->vb2_queue->dev;

	if (!node->desc.smem_alloc)
		return;

	buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	dma_unmap_page_attrs(dev, buf->daddr,
			     vb->planes[0].length,
			     DMA_BIDIRECTIONAL,
			     DMA_ATTR_SKIP_CPU_SYNC);
}

static void mtk_cam_vb2_request_complete(struct vb2_buffer *vb)
{
	struct mtk_cam_device *cam = vb2_get_drv_priv(vb->vb2_queue);

	//dev_dbg(vb->vb2_queue->dev, "%s\n", __func__);

	v4l2_ctrl_request_complete(vb->req_obj.req,
				   cam->v4l2_dev.ctrl_handler);
}

static int mtk_cam_vb2_buf_out_validate(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *v4l2_buf = to_vb2_v4l2_buffer(vb);

	if (vb->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (v4l2_buf->field == V4L2_FIELD_ANY)
			v4l2_buf->field = V4L2_FIELD_NONE;

		if (v4l2_buf->field != V4L2_FIELD_NONE)
			return -EINVAL;
	}
	return 0;
}

static const struct vb2_ops mtk_cam_vb2_ops = {
	.queue_setup = mtk_cam_vb2_queue_setup,

	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,

	.buf_out_validate = mtk_cam_vb2_buf_out_validate,
	.buf_init = mtk_cam_vb2_buf_init,
	.buf_prepare = mtk_cam_vb2_buf_prepare,
	.buf_finish = mtk_cam_vb2_buf_finish,
	.buf_cleanup = mtk_cam_vb2_buf_cleanup,

	.start_streaming = mtk_cam_vb2_start_streaming,
	.stop_streaming = mtk_cam_vb2_stop_streaming,

	.buf_queue = mtk_cam_vb2_buf_queue,
	.buf_request_complete = mtk_cam_vb2_request_complete,
};

static const struct v4l2_file_operations mtk_cam_v4l2_fops = {
	.unlocked_ioctl = video_ioctl2,
	.open = v4l2_fh_open,
	.release = vb2_fop_release,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = v4l2_compat_ioctl32,
#endif
};

/* for mmqos and base is 100 */
int mtk_cam_get_fmt_size_factor(unsigned int ipi_fmt)
{
	switch (ipi_fmt) {
	case MTKCAM_IPI_IMG_FMT_BAYER8:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER8:
	case MTKCAM_IPI_IMG_FMT_BAYER10:
	case MTKCAM_IPI_IMG_FMT_BAYER10_MIPI:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER10:
	case MTKCAM_IPI_IMG_FMT_BAYER12:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER12:
	case MTKCAM_IPI_IMG_FMT_BAYER14:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER14:
	case MTKCAM_IPI_IMG_FMT_BAYER16:
	case MTKCAM_IPI_IMG_FMT_BAYER10_UNPACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER12_UNPACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER14_UNPACKED:
	case MTKCAM_IPI_IMG_FMT_UFBC_BAYER8:
	case MTKCAM_IPI_IMG_FMT_UFBC_BAYER10:
	case MTKCAM_IPI_IMG_FMT_UFBC_BAYER12:
	case MTKCAM_IPI_IMG_FMT_UFBC_BAYER14:
	case MTKCAM_IPI_IMG_FMT_YUYV:
	case MTKCAM_IPI_IMG_FMT_YVYU:
	case MTKCAM_IPI_IMG_FMT_UYVY:
	case MTKCAM_IPI_IMG_FMT_VYUY:
	case MTKCAM_IPI_IMG_FMT_Y8:
	case MTKCAM_IPI_IMG_FMT_YUYV_Y210:
	case MTKCAM_IPI_IMG_FMT_YVYU_Y210:
	case MTKCAM_IPI_IMG_FMT_UYVY_Y210:
	case MTKCAM_IPI_IMG_FMT_VYUY_Y210:
	case MTKCAM_IPI_IMG_FMT_YUYV_Y210_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVYU_Y210_PACKED:
	case MTKCAM_IPI_IMG_FMT_UYVY_Y210_PACKED:
	case MTKCAM_IPI_IMG_FMT_VYUY_Y210_PACKED:
		return 100;
	case MTKCAM_IPI_IMG_FMT_YUV_420_2P:
	case MTKCAM_IPI_IMG_FMT_YVU_420_2P:
	case MTKCAM_IPI_IMG_FMT_YUV_420_3P:
	case MTKCAM_IPI_IMG_FMT_YVU_420_3P:
	case MTKCAM_IPI_IMG_FMT_YUV_P010_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P010_PACKED:
	case MTKCAM_IPI_IMG_FMT_YUV_P012_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P012_PACKED:
	case MTKCAM_IPI_IMG_FMT_YUV_P010:
	case MTKCAM_IPI_IMG_FMT_YVU_P010:
	case MTKCAM_IPI_IMG_FMT_YUV_P012:
	case MTKCAM_IPI_IMG_FMT_YVU_P012:
	case MTKCAM_IPI_IMG_FMT_UFBC_YUV_P010:
	case MTKCAM_IPI_IMG_FMT_UFBC_YVU_P010:
	case MTKCAM_IPI_IMG_FMT_UFBC_NV12:
	case MTKCAM_IPI_IMG_FMT_UFBC_NV21:
	case MTKCAM_IPI_IMG_FMT_UFBC_YUV_P012:
	case MTKCAM_IPI_IMG_FMT_UFBC_YVU_P012:
		return 150;
	case MTKCAM_IPI_IMG_FMT_YUV_422_2P:
	case MTKCAM_IPI_IMG_FMT_YVU_422_2P:
	case MTKCAM_IPI_IMG_FMT_YUV_422_3P:
	case MTKCAM_IPI_IMG_FMT_YVU_422_3P:
	case MTKCAM_IPI_IMG_FMT_YUV_P210:
	case MTKCAM_IPI_IMG_FMT_YVU_P210:
	case MTKCAM_IPI_IMG_FMT_YUV_P212:
	case MTKCAM_IPI_IMG_FMT_YVU_P212:
	case MTKCAM_IPI_IMG_FMT_YUV_P210_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P210_PACKED:
	case MTKCAM_IPI_IMG_FMT_YUV_P212_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P212_PACKED:
		return 200;
	case MTKCAM_IPI_IMG_FMT_FG_BAYER8_3P:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER10_3P:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER12_3P:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER10_3P_PACKED:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER12_3P_PACKED:
	case MTKCAM_IPI_IMG_FMT_RGB_8B_3P:
	case MTKCAM_IPI_IMG_FMT_RGB_10B_3P:
	case MTKCAM_IPI_IMG_FMT_RGB_12B_3P:
	case MTKCAM_IPI_IMG_FMT_RGB_10B_3P_PACKED:
	case MTKCAM_IPI_IMG_FMT_RGB_12B_3P_PACKED:
		return 300;
	default:
		pr_info("%s unsupport format(%d)", __func__, ipi_fmt);
		break;
	}
	return 100;
}

static int mtk_cam_format_fill_pixfmt_mp(struct v4l2_pix_format_mplane *pixfmt,
					 struct v4l2_plane_pix_format *plane,
					 u32 bus_size, /* TODO: need this? */
					 u32 pixelformat, u32 width, u32 height)
{
	const struct mtk_format_info *info;
	u32 aligned_width;
	u32 stride;
	u8 i;

	info = mtk_format_info(pixelformat);
	pixfmt->num_planes = info->mem_planes;

	if (!info)
		return -EINVAL;

	if (info->mem_planes != 1) {
		pr_info("do not support non contiguous mplane\n");
		return 0; /* TODO: return error? */
	}

	if (is_yuv_ufo(pixelformat)) {
		/* UFO format width should align 64 pixel */
		aligned_width = ALIGN(width, 64);
		stride = aligned_width * info->bit_r_num / info->bit_r_den;

		if (stride > plane->bytesperline)
			plane->bytesperline = stride;
		plane->sizeimage = stride * height;
		plane->sizeimage += stride * height / 2;
		plane->sizeimage += ALIGN((aligned_width / 64), 8) * height;
		plane->sizeimage += ALIGN((aligned_width / 64), 8) * height / 2;
		plane->sizeimage += sizeof(struct UfbcBufferHeader);
	} else if (is_raw_ufo(pixelformat)) {
		/* UFO format width should align 64 pixel */
		aligned_width = ALIGN(width, 64);
		stride = aligned_width * info->bit_r_num / info->bit_r_den;

		if (stride > plane->bytesperline)
			plane->bytesperline = stride;
		plane->sizeimage = stride * height;
		plane->sizeimage += ALIGN((aligned_width / 64), 8) * height;
		plane->sizeimage += sizeof(struct UfbcBufferHeader);
	} else {
		/* width should be bus_size align */
		aligned_width = ALIGN(DIV_ROUND_UP(width * info->bit_r_num,
						   info->bit_r_den), bus_size);
		stride = aligned_width * info->bpp[0];

		if (stride > plane->bytesperline)
			plane->bytesperline = stride;

		for (i = 0; i < info->comp_planes; i++) {
			unsigned int hdiv = (i == 0) ? 1 : info->hdiv;
			unsigned int vdiv = (i == 0) ? 1 : info->vdiv;

			if (plane->bytesperline > stride) {
				if (is_fullg_rb(pixelformat)) {
					plane->sizeimage +=
						DIV_ROUND_UP(plane->bytesperline, hdiv)
						* DIV_ROUND_UP(height, vdiv);
				} else {
					plane->sizeimage += plane->bytesperline
						* DIV_ROUND_UP(height, vdiv);
				}
			} else {
				plane->sizeimage += info->bpp[i]
					* DIV_ROUND_UP(aligned_width, hdiv)
					* DIV_ROUND_UP(height, vdiv);
			}

		}
	}
	pr_debug("%s stride %d sizeimage %d\n", __func__,
		 plane->bytesperline, plane->sizeimage);

	return 0;
}

int mtk_cam_fill_pixfmt_mp(struct v4l2_pix_format_mplane *pixfmt,
			u32 pixelformat, u32 width, u32 height)
{
	struct v4l2_plane_pix_format *plane;
	unsigned int ipi_fmt = mtk_cam_get_img_fmt(pixelformat);
	u8 pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
	u32 stride;
	u32 aligned_width;
	u8 pixel_mode_shift = 0;
	u8 bus_size;
	u8 i;

	pixfmt->width = width;
	pixfmt->height = height;
	pixfmt->pixelformat = pixelformat;
	plane = &pixfmt->plane_fmt[0];
	bus_size = mtk_cam_yuv_dma_bus_size(pixel_bits, pixel_mode_shift);
	plane->sizeimage = 0;

	if (is_mtk_format(pixelformat))
		mtk_cam_format_fill_pixfmt_mp(pixfmt, plane, bus_size,
					      pixelformat, width, height);
	else {
		const struct v4l2_format_info *info;

		pr_debug("pixelformat:0x%x sizeimage:%d\n",
			 pixelformat, plane->sizeimage);

		info = v4l2_format_info(pixelformat);
		pixfmt->num_planes = info->mem_planes;

		if (!info)
			return -EINVAL;

		if (info->mem_planes == 1) {

			aligned_width = ALIGN(width, bus_size);
			stride = aligned_width * info->bpp[0];
			if (stride > plane->bytesperline)
				plane->bytesperline = stride;

			for (i = 0; i < info->comp_planes; i++) {
				unsigned int hdiv = (i == 0) ? 1 : info->hdiv;
				unsigned int vdiv = (i == 0) ? 1 : info->vdiv;

				plane->sizeimage += info->bpp[i]
					* DIV_ROUND_UP(aligned_width, hdiv)
					* DIV_ROUND_UP(height, vdiv);
			}
			pr_debug("%s stride %d sizeimage %d\n", __func__,
				plane->bytesperline, plane->sizeimage);
		} else {
			pr_debug("do not support non contiguous mplane\n");
		}
	}

	return 0;
}

static void cal_image_pix_mp(unsigned int node_id,
			     struct v4l2_pix_format_mplane *mp,
			     unsigned int pixel_mode)
{
	unsigned int ipi_fmt = mtk_cam_get_img_fmt(mp->pixelformat);
	unsigned int width = mp->width;
	unsigned int height = mp->height;
	unsigned int stride, i;

	pr_debug("fmt:0x%x ipi_fmt:%d\n", mp->pixelformat, ipi_fmt);
	switch (ipi_fmt) {
	case MTKCAM_IPI_IMG_FMT_BAYER8:
	case MTKCAM_IPI_IMG_FMT_BAYER10:
	case MTKCAM_IPI_IMG_FMT_BAYER12:
	case MTKCAM_IPI_IMG_FMT_BAYER14:
	case MTKCAM_IPI_IMG_FMT_BAYER16:
	case MTKCAM_IPI_IMG_FMT_BAYER10_MIPI:
	case MTKCAM_IPI_IMG_FMT_BAYER10_UNPACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER12_UNPACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER14_UNPACKED:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER8:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER10:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER12:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER14:
		stride = mtk_cam_dmao_xsize(width, ipi_fmt, pixel_mode);
		for (i = 0; i < mp->num_planes; i++) {
			if (stride > mp->plane_fmt[i].bytesperline)
				mp->plane_fmt[i].bytesperline = stride;
			mp->plane_fmt[i].sizeimage = mp->plane_fmt[i].bytesperline * height;
		}
	break;
	case MTKCAM_IPI_IMG_FMT_YUYV:
	case MTKCAM_IPI_IMG_FMT_YVYU:
	case MTKCAM_IPI_IMG_FMT_UYVY:
	case MTKCAM_IPI_IMG_FMT_VYUY:
	case MTKCAM_IPI_IMG_FMT_YUV_422_2P:
	case MTKCAM_IPI_IMG_FMT_YVU_422_2P:
	case MTKCAM_IPI_IMG_FMT_YUV_422_3P:
	case MTKCAM_IPI_IMG_FMT_YVU_422_3P:
	case MTKCAM_IPI_IMG_FMT_YUV_420_2P:
	case MTKCAM_IPI_IMG_FMT_YVU_420_2P:
	case MTKCAM_IPI_IMG_FMT_YUV_420_3P:
	case MTKCAM_IPI_IMG_FMT_YVU_420_3P:
	case MTKCAM_IPI_IMG_FMT_Y8:
	case MTKCAM_IPI_IMG_FMT_YUYV_Y210:
	case MTKCAM_IPI_IMG_FMT_YVYU_Y210:
	case MTKCAM_IPI_IMG_FMT_UYVY_Y210:
	case MTKCAM_IPI_IMG_FMT_VYUY_Y210:
	case MTKCAM_IPI_IMG_FMT_YUYV_Y210_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVYU_Y210_PACKED:
	case MTKCAM_IPI_IMG_FMT_UYVY_Y210_PACKED:
	case MTKCAM_IPI_IMG_FMT_VYUY_Y210_PACKED:
	case MTKCAM_IPI_IMG_FMT_YUV_P210:
	case MTKCAM_IPI_IMG_FMT_YVU_P210:
	case MTKCAM_IPI_IMG_FMT_YUV_P010:
	case MTKCAM_IPI_IMG_FMT_YVU_P010:
	case MTKCAM_IPI_IMG_FMT_YUV_P210_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P210_PACKED:
	case MTKCAM_IPI_IMG_FMT_YUV_P010_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P010_PACKED:
	case MTKCAM_IPI_IMG_FMT_YUV_P212:
	case MTKCAM_IPI_IMG_FMT_YVU_P212:
	case MTKCAM_IPI_IMG_FMT_YUV_P012:
	case MTKCAM_IPI_IMG_FMT_YVU_P012:
	case MTKCAM_IPI_IMG_FMT_YUV_P212_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P212_PACKED:
	case MTKCAM_IPI_IMG_FMT_YUV_P012_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P012_PACKED:
	case MTKCAM_IPI_IMG_FMT_UFBC_NV12:
	case MTKCAM_IPI_IMG_FMT_UFBC_NV21:
	case MTKCAM_IPI_IMG_FMT_UFBC_YUV_P010:
	case MTKCAM_IPI_IMG_FMT_UFBC_YVU_P010:
	case MTKCAM_IPI_IMG_FMT_UFBC_YUV_P012:
	case MTKCAM_IPI_IMG_FMT_UFBC_YVU_P012:
	case MTKCAM_IPI_IMG_FMT_UFBC_BAYER8:
	case MTKCAM_IPI_IMG_FMT_UFBC_BAYER10:
	case MTKCAM_IPI_IMG_FMT_UFBC_BAYER12:
	case MTKCAM_IPI_IMG_FMT_UFBC_BAYER14:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER8_3P:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER10_3P_PACKED:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER12_3P_PACKED:
		mtk_cam_fill_pixfmt_mp(mp, mp->pixelformat, width, height);
	default:
		break;
	}
}

static int mtk_video_init_format(struct mtk_cam_video_device *video)
{
	struct mtk_cam_dev_node_desc *desc = &video->desc;
	struct v4l2_format *active = &video->active_fmt;
	const struct v4l2_format *default_fmt =
		&desc->fmts[desc->default_fmt_idx].vfmt;

	active->type = desc->buf_type;

	if (!default_fmt)
		return -1;

	if (!desc->image) {
		active->fmt.meta.dataformat = default_fmt->fmt.meta.dataformat;
		active->fmt.meta.buffersize = default_fmt->fmt.meta.buffersize;
		return 0;
	}

	active->fmt.pix_mp.pixelformat = default_fmt->fmt.pix_mp.pixelformat;
	active->fmt.pix_mp.width = default_fmt->fmt.pix_mp.width;
	active->fmt.pix_mp.height = default_fmt->fmt.pix_mp.height;
	active->fmt.pix_mp.num_planes = default_fmt->fmt.pix_mp.num_planes;

	/* TODO: may have wrong stride */
	cal_image_pix_mp(desc->id, &active->fmt.pix_mp, 0);

	/**
	 * TODO: to support multi-plane: for example, yuv or do it as
	 * following?
	 */
	active->fmt.pix_mp.num_planes = 1;
	active->fmt.pix_mp.colorspace = V4L2_COLORSPACE_SRGB;
	active->fmt.pix_mp.field = V4L2_FIELD_NONE;
	active->fmt.pix_mp.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	active->fmt.pix_mp.quantization = V4L2_QUANTIZATION_DEFAULT;
	active->fmt.pix_mp.xfer_func = V4L2_XFER_FUNC_SRGB;

	return 0;
}

int mtk_cam_video_register(struct mtk_cam_video_device *video,
			   struct v4l2_device *v4l2_dev)
{
	struct mtk_cam_device *cam =
		container_of(v4l2_dev, struct mtk_cam_device, v4l2_dev);
	struct media_pad *pad = &video->pad;
	struct video_device *vdev = &video->vdev;
	struct vb2_queue *q = &video->vb2_q;
	unsigned int output = V4L2_TYPE_IS_OUTPUT(video->desc.buf_type);
	int ret;

	if (video->desc.link_flags & MEDIA_LNK_FL_ENABLED)
		video->enabled = true;
	else
		video->enabled = false;

	mutex_init(&video->q_lock);

	/* initialize vb2_queue */
	q->type = video->desc.buf_type;
	q->io_modes = VB2_MMAP | VB2_DMABUF;

	if (q->type == V4L2_BUF_TYPE_META_OUTPUT || video->desc.id == MTK_RAW_META_OUT_1)
		q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	else
		/**
		 *  Actually we want to configure it as boot time but
		 *  there is no such option now. We will upstream
		 *  a new flag such as V4L2_BUF_FLAG_TIMESTAMP_BOOT
		 *  and use that in the future.
		 */
		q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_BOOT;

	if (video->desc.smem_alloc) {
		q->bidirectional = 1;
		/* q->dev = cam->smem_dev; FIXME impl for real SCP */
		q->dev = cam->dev;
	} else {
		if (video->uid.pipe_id >= MTKCAM_SUBDEV_CAMSV_START &&
			video->uid.pipe_id < MTKCAM_SUBDEV_CAMSV_END) {
			switch (video->uid.pipe_id) {
			case MTKCAM_SUBDEV_CAMSV_0:
			case MTKCAM_SUBDEV_CAMSV_1:
			case MTKCAM_SUBDEV_CAMSV_4:
			case MTKCAM_SUBDEV_CAMSV_5:
			case MTKCAM_SUBDEV_CAMSV_8:
			case MTKCAM_SUBDEV_CAMSV_9:
			case MTKCAM_SUBDEV_CAMSV_12:
			case MTKCAM_SUBDEV_CAMSV_13:
				q->dev = mtk_cam_get_larb(cam->dev, 13);
				break;
			default:
				q->dev = mtk_cam_get_larb(cam->dev, 14);
				break;
			}
		} else if (video->uid.pipe_id >= MTKCAM_SUBDEV_MRAW_START &&
			video->uid.pipe_id < MTKCAM_SUBDEV_MRAW_END) {
			switch (video->uid.pipe_id) {
			case MTKCAM_SUBDEV_MRAW_0:
			case MTKCAM_SUBDEV_MRAW_2:
				q->dev = mtk_cam_get_larb(cam->dev, 25);
				break;
			default:
				q->dev = mtk_cam_get_larb(cam->dev, 26);
				break;
			}
		} else {
			switch (video->desc.id) {
			case MTK_RAW_YUVO_1_OUT:
			case MTK_RAW_YUVO_2_OUT:
			case MTK_RAW_YUVO_3_OUT:
			case MTK_RAW_YUVO_4_OUT:
			case MTK_RAW_YUVO_5_OUT:
			case MTK_RAW_DRZS4NO_1_OUT:
			case MTK_RAW_DRZS4NO_2_OUT:
			case MTK_RAW_DRZS4NO_3_OUT:
			case MTK_RAW_RZH1N2TO_1_OUT:
			case MTK_RAW_RZH1N2TO_2_OUT:
			case MTK_RAW_RZH1N2TO_3_OUT:
				/* should have a better implementation here */
				q->dev = cam->engines.yuv_devs[0];
				break;
			default:
				q->dev = cam->engines.raw_devs[0];
				break;
			}
		}
	}

	q->supports_requests = true;
	q->lock = &video->q_lock;
	q->ops = &mtk_cam_vb2_ops;
	q->mem_ops = &mtk_cam_dma_contig_memops;
	q->drv_priv = cam;
	q->buf_struct_size = sizeof(struct mtk_cam_buffer);

	if (output)
		q->timestamp_flags |= V4L2_BUF_FLAG_TSTAMP_SRC_EOF;
	else
		q->timestamp_flags |= V4L2_BUF_FLAG_TSTAMP_SRC_SOE;

	/* No minimum buffers limitation */
	q->min_buffers_needed = 0;

	ret = vb2_queue_init(q);
	if (ret < 0) {
		dev_info(v4l2_dev->dev, "Failed to init vb2 queue: %d\n", ret);
		goto error_vb2_init;
	}

	pad->flags = output ? MEDIA_PAD_FL_SOURCE : MEDIA_PAD_FL_SINK;
	ret = media_entity_pads_init(&vdev->entity, 1, pad);
	if (ret < 0) {
		dev_info(v4l2_dev->dev, "Failed to init video entity: %d\n",
			ret);
		goto error_media_init;
	}

	ret = mtk_video_init_format(video);
	if (ret < 0) {
		dev_info(v4l2_dev->dev, "Failed to init format: %d\n", ret);
		goto error_video_register;
	}

	vdev->entity.function = MEDIA_ENT_F_IO_V4L;
	vdev->entity.ops = NULL;
	vdev->fops = &mtk_cam_v4l2_fops;
	vdev->device_caps = video->desc.cap | V4L2_CAP_STREAMING;
	vdev->v4l2_dev = v4l2_dev;

	vdev->vfl_dir = output ? VFL_DIR_TX : VFL_DIR_RX;
	vdev->queue = &video->vb2_q;
	vdev->ioctl_ops = video->desc.ioctl_ops;
	vdev->release = video_device_release_empty;
	/* TODO: share q_lock or use another lock? */
	vdev->lock = &video->q_lock;
	strscpy(vdev->name, video->desc.name, sizeof(vdev->name));

	ret = video_register_device(vdev, VFL_TYPE_VIDEO, -1);
	if (ret < 0) {
		dev_info(v4l2_dev->dev, "Failed to register video device: %d\n",
			ret);
		goto error_video_register;
	}
	video_set_drvdata(vdev, cam);

	dev_dbg(v4l2_dev->dev, "registered vdev:%d:%s\n",
		video->desc.id, vdev->name);

	return 0;

error_video_register:
	media_entity_cleanup(&vdev->entity);
error_media_init:
	vb2_queue_release(&video->vb2_q);
error_vb2_init:
	mutex_destroy(&video->q_lock);

	return ret;
}

void mtk_cam_video_unregister(struct mtk_cam_video_device *video)
{
	video_unregister_device(&video->vdev);
	vb2_queue_release(&video->vb2_q);
	media_entity_cleanup(&video->vdev.entity);
	mutex_destroy(&video->q_lock);
}

const struct v4l2_format *
mtk_cam_dev_find_fmt(struct mtk_cam_dev_node_desc *desc, u32 format)
{
	unsigned int i;
	const struct v4l2_format *fmt;

	for (i = 0; i < desc->num_fmts; i++) {
		fmt = &desc->fmts[i].vfmt;
		if (fmt->fmt.pix_mp.pixelformat == format)
			return fmt;
	}

	return NULL;
}

int mtk_cam_vidioc_querycap(struct file *file, void *fh,
			    struct v4l2_capability *cap)
{
	struct mtk_cam_device *cam = video_drvdata(file);

	strscpy(cap->driver, dev_driver_string(cam->dev), sizeof(cap->driver));
	strscpy(cap->card, dev_driver_string(cam->dev), sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
		 dev_name(cam->dev));

	return 0;
}

int mtk_cam_vidioc_enum_framesizes(struct file *filp, void *priv,
				   struct v4l2_frmsizeenum *sizes)
{
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(filp);
	const struct v4l2_format *dev_fmt;

	dev_fmt = mtk_cam_dev_find_fmt(&node->desc, sizes->pixel_format);
	if (!dev_fmt || sizes->index)
		return -EINVAL;

	sizes->type = node->desc.frmsizes->type;
	memcpy(&sizes->stepwise, &node->desc.frmsizes->stepwise,
	       sizeof(sizes->stepwise));
	return 0;
}

int mtk_cam_vidioc_enum_fmt(struct file *file, void *fh,
			    struct v4l2_fmtdesc *f)
{
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);

	if (f->index >= node->desc.num_fmts)
		return -EINVAL;

	f->pixelformat = node->desc.fmts[f->index].vfmt.fmt.pix_mp.pixelformat;
	fill_ext_mtkcam_fmtdesc(f);
	f->flags = 0;
	return 0;
}

int mtk_cam_vidioc_g_fmt(struct file *file, void *fh,
			 struct v4l2_format *f)
{
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);

	f->fmt = node->active_fmt.fmt;

	return 0;
}

int mtk_cam_vidioc_s_fmt(struct file *file, void *fh,
			 struct v4l2_format *f)
{
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);
	int ret;

	ret = mtk_cam_video_set_fmt(node, f);
	if (!ret)
		node->active_fmt = *f;

	return ret;
}

int mtk_cam_video_set_fmt(struct mtk_cam_video_device *node,
			  struct v4l2_format *f)
{
	struct mtk_cam_device *cam = video_get_drvdata(&node->vdev);
	const struct v4l2_format *dev_fmt;
	struct v4l2_format try_fmt;
	int i;

	memset(&try_fmt, 0, sizeof(try_fmt));
	try_fmt.type = f->type;

	/* Validate pixelformat */
	dev_fmt = mtk_cam_dev_find_fmt(&node->desc, f->fmt.pix_mp.pixelformat);
	if (!dev_fmt) {
		dev_info(cam->dev, "unknown fmt:%d\n",
			 f->fmt.pix_mp.pixelformat);
		return -EINVAL;
	}

	try_fmt.fmt.pix_mp.pixelformat = dev_fmt->fmt.pix_mp.pixelformat;

	/* TODO: move limitation rules to each pipeline */
	/* Validate image width & height range */
	try_fmt.fmt.pix_mp.width = clamp_val(f->fmt.pix_mp.width,
					     IMG_MIN_WIDTH, IMG_MAX_WIDTH);
	try_fmt.fmt.pix_mp.height = clamp_val(f->fmt.pix_mp.height,
					      IMG_MIN_HEIGHT, IMG_MAX_HEIGHT);

	try_fmt.fmt.pix_mp.width = ALIGN(try_fmt.fmt.pix_mp.width, IMG_PIX_ALIGN);
	try_fmt.fmt.pix_mp.num_planes = 1;

	for (i = 0 ; i < try_fmt.fmt.pix_mp.num_planes ; i++)
		try_fmt.fmt.pix_mp.plane_fmt[i].bytesperline =
				f->fmt.pix_mp.plane_fmt[i].bytesperline;

	/* bytesperline & sizeimage calculation */
	if (node->desc.dma_port == MTKCAM_IPI_CAMSV_MAIN_OUT)
		cal_image_pix_mp(node->desc.id, &try_fmt.fmt.pix_mp, 3);
	else
		cal_image_pix_mp(node->desc.id, &try_fmt.fmt.pix_mp, 0);

#ifdef NOT_READY
#if PDAF_READY
	/* add header size for vc channel */
	if (node->desc.dma_port == MTKCAM_IPI_CAMSV_MAIN_OUT &&
		node->desc.id == MTK_CAMSV_MAIN_STREAM_OUT)
		try_fmt.fmt.pix_mp.plane_fmt[0].sizeimage +=
			GET_PLAT_V4L2(meta_sv_ext_size);
#endif
#endif

	/* Constant format fields */
	try_fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_SRGB;
	try_fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
	try_fmt.fmt.pix_mp.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	try_fmt.fmt.pix_mp.quantization = V4L2_QUANTIZATION_DEFAULT;
	try_fmt.fmt.pix_mp.xfer_func = V4L2_XFER_FUNC_SRGB;

	*f = try_fmt;
	return 0;
}

int mtk_cam_vidioc_try_fmt(struct file *file, void *fh,
			   struct v4l2_format *f)
{
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);

	return mtk_cam_video_set_fmt(node, f);
}

int mtk_cam_vidioc_meta_enum_fmt(struct file *file, void *fh,
				 struct v4l2_fmtdesc *f)
{
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);

	if (f->index)
		return -EINVAL;

	f->pixelformat = node->active_fmt.fmt.meta.dataformat;
	fill_ext_mtkcam_fmtdesc(f);
	f->flags = 0;

	return 0;
}

int mtk_cam_vidioc_g_meta_fmt(struct file *file, void *fh,
			      struct v4l2_format *f)
{
	struct mtk_cam_device *cam = video_drvdata(file);
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);
#ifdef NOT_READY
	struct mtk_cam_dev_node_desc *desc = &node->desc;
	const struct v4l2_format *default_fmt =
		&desc->fmts[desc->default_fmt_idx].vfmt;
	struct mtk_raw_pde_config *pde_cfg;
	struct mtk_cam_pde_info *pde_info;
	u32 extmeta_size = 0;
#endif

#ifdef NOT_READY
	if (node->desc.dma_port == MTKCAM_IPI_RAW_META_STATS_CFG) {
		pde_cfg = &cam->raw.pipelines[node->uid.pipe_id].pde_config;
		pde_info = &pde_cfg->pde_info;
		if (pde_info->pd_table_offset) {
			node->active_fmt.fmt.meta.buffersize =
				default_fmt->fmt.meta.buffersize
				+ pde_info->pdi_max_size;
			dev_dbg(cam->dev, "PDE: node(%d), enlarge meta size()",
				node->desc.dma_port,
				node->active_fmt.fmt.meta.buffersize);
		}
	}
	if (node->desc.dma_port == MTKCAM_IPI_RAW_META_STATS_0) {
		pde_cfg = &cam->raw.pipelines[node->uid.pipe_id].pde_config;
		pde_info = &pde_cfg->pde_info;
		if (pde_info->pd_table_offset) {
			node->active_fmt.fmt.meta.buffersize =
				default_fmt->fmt.meta.buffersize
				+ pde_info->pdo_max_size;
			dev_dbg(cam->dev, "PDE: node(%d), enlarge meta size()",
				node->desc.dma_port,
				node->active_fmt.fmt.meta.buffersize);
		}
	}
#endif

#ifdef NOT_READY
	switch (node->desc.id) {
	case MTK_RAW_MAIN_STREAM_SV_1_OUT:
	case MTK_RAW_MAIN_STREAM_SV_2_OUT:
		break;
	case MTK_RAW_META_SV_OUT_0:
	case MTK_RAW_META_SV_OUT_1:
	case MTK_RAW_META_SV_OUT_2:
		if (node->enabled && node->ctx)
			extmeta_size = cam->raw.pipelines[node->uid.pipe_id]
				.cfg[MTK_RAW_META_SV_OUT_0].mbus_fmt.width *
				cam->raw.pipelines[node->uid.pipe_id]
				.cfg[MTK_RAW_META_SV_OUT_0].mbus_fmt.height;
		if (extmeta_size)
			node->active_fmt.fmt.meta.buffersize = extmeta_size;
		else
			node->active_fmt.fmt.meta.buffersize =
				CAMSV_EXT_META_0_WIDTH * CAMSV_EXT_META_0_HEIGHT;
		dev_dbg(cam->dev,
			"%s:extmeta name:%s buffersize:%d\n",
			__func__, node->desc.name, node->active_fmt.fmt.meta.buffersize);
		break;
	default:
		break;
	}
#endif

	f->fmt.meta.dataformat = node->active_fmt.fmt.meta.dataformat;
	f->fmt.meta.buffersize = node->active_fmt.fmt.meta.buffersize;
	dev_dbg(cam->dev,
		"%s: node:%d dataformat:%d buffersize:%d\n",
		__func__, node->desc.id, f->fmt.meta.dataformat, f->fmt.meta.buffersize);

	return 0;

}

int mtk_cam_vidioc_s_selection(struct file *file, void *fh,
				struct v4l2_selection *s)
{
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);

	node->active_crop = *s;
	return 0;
}
