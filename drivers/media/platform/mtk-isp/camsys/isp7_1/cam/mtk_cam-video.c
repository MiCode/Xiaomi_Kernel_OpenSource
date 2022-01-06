// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>

#include "mtk_cam.h"
#include "mtk_cam-feature.h"
#include "mtk_cam-video.h"
#include "mtk_camera-v4l2-controls.h"
#include "mtk_camera-videodev2.h"
#include "mtk_cam-ufbc-def.h"

#include "mtk_cam_vb2-dma-contig.h"
#include "mtk_cam-trace.h"

/*
 * Note
 *	differt dma(fmt) would have different bus_size
 *	align xsize(bytes per line) with [bus_size * pixel_mode]
 */
int mtk_cam_is_fullg(unsigned int ipi_fmt)
{
	return (ipi_fmt == MTKCAM_IPI_IMG_FMT_FG_BAYER8)
		|| (ipi_fmt == MTKCAM_IPI_IMG_FMT_FG_BAYER10)
		|| (ipi_fmt == MTKCAM_IPI_IMG_FMT_FG_BAYER12);
}

static inline
int mtk_cam_dma_bus_size(int bpp, int pixel_mode_shift, int is_fg)
{
	unsigned int bus_size = ALIGN(bpp, 16) << pixel_mode_shift;

	if (is_fg)
		bus_size <<= 1;
	return bus_size / 8; /* in bytes */
}

static inline
int mtk_cam_yuv_dma_bus_size(int bpp, int pixel_mode_shift)
{
	unsigned int bus_size = ALIGN(bpp, 32);

	return bus_size / 8; /* in bytes */
}

static inline
int mtk_cam_dmao_xsize(int w, unsigned int ipi_fmt, int pixel_mode_shift)
{
	const int is_fg		= mtk_cam_is_fullg(ipi_fmt);
	const int bpp		= mtk_cam_get_pixel_bits(ipi_fmt);
	const int bytes		= is_fg ?
		DIV_ROUND_UP(w * bpp * 3 / 2, 8) : DIV_ROUND_UP(w * bpp, 8);
	const int bus_size	= mtk_cam_dma_bus_size(bpp, pixel_mode_shift, is_fg);

	return ALIGN(bytes, bus_size);
}

static int mtk_cam_vb2_queue_setup(struct vb2_queue *vq,
				   unsigned int *num_buffers,
				   unsigned int *num_planes,
				   unsigned int sizes[],
				   struct device *alloc_devs[])
{
	struct mtk_cam_device *cam = vb2_get_drv_priv(vq);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vq);
	struct mtk_raw_pipeline *raw_pipeline;
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
		/* TBC: subsampling configuration */
		if (is_raw_subdev(node->uid.pipe_id)) {
			raw_pipeline = mtk_cam_dev_get_raw_pipeline(cam, node->uid.pipe_id);
			if (raw_pipeline && node->desc.id == MTK_RAW_MAIN_STREAM_OUT) {
				if (raw_pipeline->dynamic_exposure_num_max >
				    fmt->fmt.pix_mp.num_planes) {
					*num_planes = raw_pipeline->dynamic_exposure_num_max;
					for (i = 0; i < *num_planes; i++)
						sizes[i] = size;
				} else if (raw_pipeline && raw_pipeline->user_res.raw_res.feature &&
					fmt->fmt.pix_mp.num_planes > 1) {
					*num_planes = fmt->fmt.pix_mp.num_planes;

					for (i = 0; i < *num_planes; i++)
						sizes[i] = size;
				} else {
					*num_planes = 1;
					sizes[0] = size;
				}
			} else if (raw_pipeline && raw_pipeline->user_res.raw_res.feature
				&& fmt->fmt.pix_mp.num_planes > 1) {
				*num_planes = fmt->fmt.pix_mp.num_planes;
				for (i = 0; i < *num_planes; i++)
					sizes[i] = size;
			} else {
				*num_planes = 1;
				sizes[0] = size;
			}
		} else if (is_camsv_subdev(node->uid.pipe_id)) {
			*num_planes = 1;
			sizes[0] = size;
		} else if (is_mraw_subdev(node->uid.pipe_id)) {
			*num_planes = 1;
			sizes[0] = size;
		} else {
			*num_planes = 1;
			sizes[0] = size;
		}
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

static int mtk_cam_vb2_start_streaming(struct vb2_queue *vq,
				       unsigned int count)
{
	struct mtk_cam_device *cam = vb2_get_drv_priv(vq);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vq);
	struct media_entity *entity = &node->vdev.entity;
	struct mtk_cam_ctx *ctx = NULL;
	struct device *dev = cam->dev;
	int ret;

	/* check entity is linked */
	if (!node->enabled) {
		dev_info(cam->dev,
			"%s: stream on failed, node is not enabled\n",
			node->desc.name);
		ret = -ENOLINK;
		goto fail_return_buffer;
	}

	if (!entity->pipe) {
		ctx = mtk_cam_start_ctx(cam, node);
		if (!ctx) {
			ret = -ENOLINK;
			goto fail_return_buffer;
		}
	} else {
		ctx = mtk_cam_find_ctx(cam, entity);
		if (WARN_ON(!ctx)) {
			ret = -ENOLINK;
			goto fail_return_buffer;
		}
	}

	cam->streaming_pipe |= (1 << node->uid.pipe_id);
	ctx->streaming_pipe |= (1 << node->uid.pipe_id);
	ctx->streaming_node_cnt++;

#if CCD_READY
	if (ctx->streaming_node_cnt == 1)
		if (is_raw_subdev(node->uid.pipe_id)) {
			if (!isp_composer_create_session(ctx)) {
				ctx->session_created = 1;
			} else {
				complete(&ctx->session_complete);
				ret = -EBUSY;
				goto fail_stop_ctx;
			}
		}
#endif

	dev_dbg(dev, "%s:%s:ctx(%d): node:%d count info:%d\n", __func__,
		node->desc.name, ctx->stream_id, node->desc.id, ctx->streaming_node_cnt);

	if (ctx->streaming_node_cnt < ctx->enabled_node_cnt)
		return 0;

	/* all enabled nodes are streaming, enable all subdevs */
	MTK_CAM_TRACE_BEGIN(BASIC, "ctx_stream_on");
	ret = mtk_cam_ctx_stream_on(ctx);
	MTK_CAM_TRACE_END(BASIC);

	if (ret)
		goto fail_destroy_session;

	return 0;

fail_destroy_session:
	if (ctx->session_created)
		isp_composer_destroy_session(ctx);
fail_stop_ctx:
	ctx->streaming_node_cnt--;
	ctx->streaming_pipe &= ~(1 << node->uid.pipe_id);
	cam->streaming_pipe &= ~(1 << node->uid.pipe_id);
	mtk_cam_dev_req_cleanup(ctx, node->uid.pipe_id, VB2_BUF_STATE_QUEUED);
	mtk_cam_stop_ctx(ctx, entity);
fail_return_buffer:
	/* relese bufs by request */
	return ret;
}

static void mtk_cam_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct mtk_cam_device *cam = vb2_get_drv_priv(vq);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vq);
	struct device *dev = cam->dev;
	struct mtk_cam_ctx *ctx;

	ctx = mtk_cam_find_ctx(cam, &node->vdev.entity);
	if (WARN_ON(!ctx)) {
		/* the ctx is stop, media_pipeline_stop is called */
		mtk_cam_dev_req_clean_pending(cam, node->uid.pipe_id,
					      VB2_BUF_STATE_ERROR);
		return;
	}

	dev_dbg(dev, "%s:%s:ctx(%d): node:%d count info:%d\n", __func__,
		node->desc.name, ctx->stream_id, node->desc.id, ctx->streaming_node_cnt);

	if (ctx->streaming_node_cnt == ctx->enabled_node_cnt) {
		MTK_CAM_TRACE_BEGIN(BASIC, "ctx_stream_off");
		mtk_cam_ctx_stream_off(ctx);
		MTK_CAM_TRACE_END(BASIC);
	}

	if (cam->streaming_pipe & (1 << node->uid.pipe_id)) {
		/* NOTE: take multi-pipelines case into consideration     */
		/* Moreover, must clean bit mask before req cleanup       */
		/* Otherwise, would cause req not removed in pending list */
		cam->streaming_pipe &= ~(1 << node->uid.pipe_id);
		mtk_cam_dev_req_cleanup(ctx, node->uid.pipe_id, VB2_BUF_STATE_ERROR);
	}

	/* all bufs of node should be return by per requests */

	/* NOTE: take multi-pipelines case into consideration */
	cam->streaming_pipe &= ~(1 << node->uid.pipe_id);
	ctx->streaming_node_cnt--;
	if (ctx->streaming_node_cnt)
		return;

	mtk_cam_stop_ctx(ctx, &node->vdev.entity);
}

int is_mtk_format(u32 pixelformat)
{
	switch (pixelformat) {
	case V4L2_PIX_FMT_YUYV10:
	case V4L2_PIX_FMT_YVYU10:
	case V4L2_PIX_FMT_UYVY10:
	case V4L2_PIX_FMT_VYUY10:
	case V4L2_PIX_FMT_YUYV12:
	case V4L2_PIX_FMT_YVYU12:
	case V4L2_PIX_FMT_UYVY12:
	case V4L2_PIX_FMT_VYUY12:
	case V4L2_PIX_FMT_MTISP_YUYV10P:
	case V4L2_PIX_FMT_MTISP_YVYU10P:
	case V4L2_PIX_FMT_MTISP_UYVY10P:
	case V4L2_PIX_FMT_MTISP_VYUY10P:
	case V4L2_PIX_FMT_MTISP_YUYV12P:
	case V4L2_PIX_FMT_MTISP_YVYU12P:
	case V4L2_PIX_FMT_MTISP_UYVY12P:
	case V4L2_PIX_FMT_MTISP_VYUY12P:
	case V4L2_PIX_FMT_NV12_10:
	case V4L2_PIX_FMT_NV21_10:
	case V4L2_PIX_FMT_NV16_10:
	case V4L2_PIX_FMT_NV61_10:
	case V4L2_PIX_FMT_NV12_12:
	case V4L2_PIX_FMT_NV21_12:
	case V4L2_PIX_FMT_NV16_12:
	case V4L2_PIX_FMT_NV61_12:
	case V4L2_PIX_FMT_MTISP_NV12_10P:
	case V4L2_PIX_FMT_MTISP_NV21_10P:
	case V4L2_PIX_FMT_MTISP_NV16_10P:
	case V4L2_PIX_FMT_MTISP_NV61_10P:
	case V4L2_PIX_FMT_MTISP_NV12_12P:
	case V4L2_PIX_FMT_MTISP_NV21_12P:
	case V4L2_PIX_FMT_MTISP_NV16_12P:
	case V4L2_PIX_FMT_MTISP_NV61_12P:
	case V4L2_PIX_FMT_MTISP_NV12_UFBC:
	case V4L2_PIX_FMT_MTISP_NV21_UFBC:
	case V4L2_PIX_FMT_MTISP_NV12_10_UFBC:
	case V4L2_PIX_FMT_MTISP_NV21_10_UFBC:
	case V4L2_PIX_FMT_MTISP_NV12_12_UFBC:
	case V4L2_PIX_FMT_MTISP_NV21_12_UFBC:
	case V4L2_PIX_FMT_MTISP_SGRB8F:
	case V4L2_PIX_FMT_MTISP_SGRB10F:
	case V4L2_PIX_FMT_MTISP_SGRB12F:
		return 1;
	break;
	default:
		return 0;
	break;
	}
}

int is_yuv_ufo(u32 pixelformat)
{
	switch (pixelformat) {
	case V4L2_PIX_FMT_MTISP_NV12_UFBC:
	case V4L2_PIX_FMT_MTISP_NV21_UFBC:
	case V4L2_PIX_FMT_MTISP_NV12_10_UFBC:
	case V4L2_PIX_FMT_MTISP_NV21_10_UFBC:
	case V4L2_PIX_FMT_MTISP_NV12_12_UFBC:
	case V4L2_PIX_FMT_MTISP_NV21_12_UFBC:
		return 1;
	default:
		return 0;
	}
}

int is_raw_ufo(u32 pixelformat)
{
	switch (pixelformat) {
	case V4L2_PIX_FMT_MTISP_BAYER8_UFBC:
	case V4L2_PIX_FMT_MTISP_BAYER10_UFBC:
	case V4L2_PIX_FMT_MTISP_BAYER12_UFBC:
	case V4L2_PIX_FMT_MTISP_BAYER14_UFBC:
		return 1;
	default:
		return 0;
	}
}

int is_fullg_rb(u32 pixelformat)
{
	switch (pixelformat) {
	case V4L2_PIX_FMT_MTISP_SGRB8F:
	case V4L2_PIX_FMT_MTISP_SGRB10F:
	case V4L2_PIX_FMT_MTISP_SGRB12F:
		return 1;
	default:
		return 0;
	}
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

static void mtk_cam_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_cam_device *cam = vb2_get_drv_priv(vb->vb2_queue);
	unsigned int pipe_id;
	struct mtk_cam_buffer *buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	struct mtk_cam_request *req = to_mtk_cam_req(vb->request);
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	struct mtk_raw_pde_config *pde_cfg;
	struct device *dev = cam->dev;
	unsigned int desc_id;
	unsigned int dma_port;
	unsigned int width, height, stride;
	void *vaddr;
	struct mtkcam_ipi_frame_param *frame_param;
	struct mtkcam_ipi_meta_input *meta_in;
	struct mtkcam_ipi_meta_output *meta_out;
	struct mtk_camsv_frame_params *sv_frame_params;
	struct mtk_raw_pipeline *raw_pipline;


	dma_port = node->desc.dma_port;
	pipe_id = node->uid.pipe_id;
	req_stream_data = mtk_cam_req_get_s_data(req, pipe_id, 0);
	frame_param = &req_stream_data->frame_params;
	sv_frame_params = &req_stream_data->sv_frame_params;
	raw_pipline = mtk_cam_dev_get_raw_pipeline(cam, pipe_id);
	mtk_cam_s_data_set_vbuf(req_stream_data, buf, node->desc.id);

	/* update buffer internal address */
	switch (dma_port) {
	case MTKCAM_IPI_RAW_META_STATS_CFG:
		desc_id = node->desc.id-MTK_RAW_SINK_NUM;
		meta_in = &frame_param->meta_inputs[desc_id];
		meta_in->buf.ccd_fd = vb->planes[0].m.fd;
		/* vb->planes[0].bytesused; todo: vb2_q byteused is zero before stream on */
		meta_in->buf.size = node->active_fmt.fmt.meta.buffersize;
		meta_in->buf.iova = buf->daddr;
		meta_in->uid.id = dma_port;
		vaddr = vb2_plane_vaddr(vb, 0);
		break;
	case MTKCAM_IPI_RAW_META_STATS_0:
	case MTKCAM_IPI_RAW_META_STATS_1:
	case MTKCAM_IPI_RAW_META_STATS_2:
		pde_cfg = &cam->raw.pipelines[node->uid.pipe_id].pde_config;
		desc_id = node->desc.id-MTK_RAW_META_OUT_BEGIN;
		meta_out = &frame_param->meta_outputs[desc_id];
		meta_out->buf.ccd_fd = vb->planes[0].m.fd;
		meta_out->buf.size = node->active_fmt.fmt.meta.buffersize;
		meta_out->buf.iova = buf->daddr;
		meta_out->uid.id = dma_port;
		vaddr = vb2_plane_vaddr(vb, 0);
		mtk_cam_set_meta_stats_info(dma_port, vaddr, pde_cfg);
		break;
	case MTKCAM_IPI_CAMSV_MAIN_OUT:
		if (node->desc.id == MTK_RAW_MAIN_STREAM_SV_1_OUT) {
			sv_frame_params->img_out.buf[0][0].iova = buf->daddr;
		} else {
#if PDAF_READY
			sv_frame_params->img_out.buf[0][0].iova = buf->daddr +
				mtk_cam_get_meta_size(dma_port);
#else
			sv_frame_params->img_out.buf[0][0].iova = buf->daddr;
#endif
			width = node->active_fmt.fmt.pix_mp.width;
			height = node->active_fmt.fmt.pix_mp.height;
			stride = node->active_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
#if PDAF_READY
			vaddr = vb2_plane_vaddr(vb, 0);
			mtk_cam_set_sv_meta_stats_info(
				node->desc.dma_port, vaddr, width, height, stride);
#endif
		}
		break;
#if MRAW_READY
	case MTKCAM_IPI_MRAW_META_STATS_CFG:
	case MTKCAM_IPI_MRAW_META_STATS_0:
		mtk_cam_mraw_handle_enque(vb);
		break;
#endif
	default:
		dev_dbg(dev, "%s:pipe(%d):buffer with invalid port(%d)\n",
			__func__, pipe_id, dma_port);
		break;
	}

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

	dev_dbg(vb->vb2_queue->dev, "%s\n", __func__);

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

unsigned int mtk_cam_get_sensor_pixel_id(unsigned int fmt)
{
	switch (fmt & SENSOR_FMT_MASK) {
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SBGGR14_1X14:
		return MTKCAM_IPI_BAYER_PXL_ID_B;
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGBRG14_1X14:
		return MTKCAM_IPI_BAYER_PXL_ID_GB;
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SGRBG14_1X14:
		return MTKCAM_IPI_BAYER_PXL_ID_GR;
	case MEDIA_BUS_FMT_SRGGB8_1X8:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
	case MEDIA_BUS_FMT_SRGGB14_1X14:
		return MTKCAM_IPI_BAYER_PXL_ID_R;
	default:
		return MTKCAM_IPI_BAYER_PXL_ID_UNKNOWN;
	}
}

unsigned int mtk_cam_get_sensor_fmt(unsigned int fmt)
{
	switch (fmt & SENSOR_FMT_MASK) {
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		return MTKCAM_IPI_IMG_FMT_BAYER8;
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		return MTKCAM_IPI_IMG_FMT_BAYER10;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		return MTKCAM_IPI_IMG_FMT_BAYER12;
	case MEDIA_BUS_FMT_SBGGR14_1X14:
	case MEDIA_BUS_FMT_SGBRG14_1X14:
	case MEDIA_BUS_FMT_SGRBG14_1X14:
	case MEDIA_BUS_FMT_SRGGB14_1X14:
		return MTKCAM_IPI_IMG_FMT_BAYER14;
	default:
		return MTKCAM_IPI_IMG_FMT_UNKNOWN;
	}
}

unsigned int mtk_cam_get_pixel_bits(unsigned int ipi_fmt)
{
	switch (ipi_fmt) {
	case MTKCAM_IPI_IMG_FMT_BAYER8:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER8:
		return 8;
	case MTKCAM_IPI_IMG_FMT_BAYER10:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER10:
	case MTKCAM_IPI_IMG_FMT_BAYER10_MIPI:
		return 10;
	case MTKCAM_IPI_IMG_FMT_BAYER12:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER12:
		return 12;
	case MTKCAM_IPI_IMG_FMT_BAYER14:
	case MTKCAM_IPI_IMG_FMT_UFBC_BAYER14:
		return 14;
	case MTKCAM_IPI_IMG_FMT_BAYER10_UNPACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER12_UNPACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER14_UNPACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER16:
	case MTKCAM_IPI_IMG_FMT_YUYV:
	case MTKCAM_IPI_IMG_FMT_YVYU:
	case MTKCAM_IPI_IMG_FMT_UYVY:
	case MTKCAM_IPI_IMG_FMT_VYUY:
		return 16;
	case MTKCAM_IPI_IMG_FMT_Y8:
	case MTKCAM_IPI_IMG_FMT_YUV_422_2P:
	case MTKCAM_IPI_IMG_FMT_YVU_422_2P:
	case MTKCAM_IPI_IMG_FMT_YUV_422_3P:
	case MTKCAM_IPI_IMG_FMT_YVU_422_3P:
	case MTKCAM_IPI_IMG_FMT_YUV_420_2P:
	case MTKCAM_IPI_IMG_FMT_YVU_420_2P:
	case MTKCAM_IPI_IMG_FMT_YUV_420_3P:
	case MTKCAM_IPI_IMG_FMT_YVU_420_3P:
		return 8;
	case MTKCAM_IPI_IMG_FMT_YUYV_Y210:
	case MTKCAM_IPI_IMG_FMT_YVYU_Y210:
	case MTKCAM_IPI_IMG_FMT_UYVY_Y210:
	case MTKCAM_IPI_IMG_FMT_VYUY_Y210:
		return 32;
	case MTKCAM_IPI_IMG_FMT_YUV_P210:
	case MTKCAM_IPI_IMG_FMT_YVU_P210:
	case MTKCAM_IPI_IMG_FMT_YUV_P010:
	case MTKCAM_IPI_IMG_FMT_YVU_P010:
	case MTKCAM_IPI_IMG_FMT_YUV_P212:
	case MTKCAM_IPI_IMG_FMT_YVU_P212:
	case MTKCAM_IPI_IMG_FMT_YUV_P012:
	case MTKCAM_IPI_IMG_FMT_YVU_P012:
		return 16;
	case MTKCAM_IPI_IMG_FMT_YUYV_Y210_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVYU_Y210_PACKED:
	case MTKCAM_IPI_IMG_FMT_UYVY_Y210_PACKED:
	case MTKCAM_IPI_IMG_FMT_VYUY_Y210_PACKED:
		return 20;
	case MTKCAM_IPI_IMG_FMT_YUV_P210_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P210_PACKED:
	case MTKCAM_IPI_IMG_FMT_YUV_P010_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P010_PACKED:
		return 10;
	case MTKCAM_IPI_IMG_FMT_YUV_P212_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P212_PACKED:
	case MTKCAM_IPI_IMG_FMT_YUV_P012_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P012_PACKED:
		return 12;
	case MTKCAM_IPI_IMG_FMT_RGB_8B_3P:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER8_3P:
	case MTKCAM_IPI_IMG_FMT_UFBC_NV12:
	case MTKCAM_IPI_IMG_FMT_UFBC_NV21:
	case MTKCAM_IPI_IMG_FMT_UFBC_BAYER8:
		return 8;
	case MTKCAM_IPI_IMG_FMT_RGB_10B_3P_PACKED:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER10_3P_PACKED:
	case MTKCAM_IPI_IMG_FMT_UFBC_YUV_P010:
	case MTKCAM_IPI_IMG_FMT_UFBC_YVU_P010:
	case MTKCAM_IPI_IMG_FMT_UFBC_BAYER10:
		return 10;
	case MTKCAM_IPI_IMG_FMT_RGB_12B_3P_PACKED:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER12_3P_PACKED:
	case MTKCAM_IPI_IMG_FMT_UFBC_YUV_P012:
	case MTKCAM_IPI_IMG_FMT_UFBC_YVU_P012:
	case MTKCAM_IPI_IMG_FMT_UFBC_BAYER12:
		return 12;
	case MTKCAM_IPI_IMG_FMT_RGB_10B_3P:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER10_3P:
	case MTKCAM_IPI_IMG_FMT_RGB_12B_3P:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER12_3P:
		return 16;

	default:
		break;
	}
	pr_debug("not supported ipi-fmt 0x%08x", ipi_fmt);

	return -1;
}

unsigned int mtk_cam_get_img_fmt(unsigned int fourcc)
{
	switch (fourcc) {
	case V4L2_PIX_FMT_GREY:
		return MTKCAM_IPI_IMG_FMT_Y8;
	case V4L2_PIX_FMT_YUYV:
		return MTKCAM_IPI_IMG_FMT_YUYV;
	case V4L2_PIX_FMT_YVYU:
		return MTKCAM_IPI_IMG_FMT_YVYU;
	case V4L2_PIX_FMT_NV16:
		return MTKCAM_IPI_IMG_FMT_YUV_422_2P;
	case V4L2_PIX_FMT_NV61:
		return MTKCAM_IPI_IMG_FMT_YVU_422_2P;
	case V4L2_PIX_FMT_NV12:
		return MTKCAM_IPI_IMG_FMT_YUV_420_2P;
	case V4L2_PIX_FMT_NV21:
		return MTKCAM_IPI_IMG_FMT_YVU_420_2P;
	case V4L2_PIX_FMT_YUV422P:
		return MTKCAM_IPI_IMG_FMT_YUV_422_3P;
	case V4L2_PIX_FMT_YUV420:
		return MTKCAM_IPI_IMG_FMT_YUV_420_3P;
	case V4L2_PIX_FMT_YVU420:
		return MTKCAM_IPI_IMG_FMT_YVU_420_3P;
	case V4L2_PIX_FMT_NV12_10:
		return MTKCAM_IPI_IMG_FMT_YUV_P010;
	case V4L2_PIX_FMT_NV21_10:
		return MTKCAM_IPI_IMG_FMT_YVU_P010;
	case V4L2_PIX_FMT_NV16_10:
		return MTKCAM_IPI_IMG_FMT_YUV_P210;
	case V4L2_PIX_FMT_NV61_10:
		return MTKCAM_IPI_IMG_FMT_YVU_P210;
	case V4L2_PIX_FMT_MTISP_NV12_10P:
		return MTKCAM_IPI_IMG_FMT_YUV_P010_PACKED;
	case V4L2_PIX_FMT_MTISP_NV21_10P:
		return MTKCAM_IPI_IMG_FMT_YVU_P010_PACKED;
	case V4L2_PIX_FMT_MTISP_NV16_10P:
		return MTKCAM_IPI_IMG_FMT_YUV_P210_PACKED;
	case V4L2_PIX_FMT_MTISP_NV61_10P:
		return MTKCAM_IPI_IMG_FMT_YVU_P210_PACKED;
	case V4L2_PIX_FMT_YUYV10:
		return MTKCAM_IPI_IMG_FMT_YUYV_Y210;
	case V4L2_PIX_FMT_YVYU10:
		return MTKCAM_IPI_IMG_FMT_YVYU_Y210;
	case V4L2_PIX_FMT_UYVY10:
		return MTKCAM_IPI_IMG_FMT_UYVY_Y210;
	case V4L2_PIX_FMT_VYUY10:
		return MTKCAM_IPI_IMG_FMT_VYUY_Y210;
	case V4L2_PIX_FMT_MTISP_YUYV10P:
		return MTKCAM_IPI_IMG_FMT_YUYV_Y210_PACKED;
	case V4L2_PIX_FMT_MTISP_YVYU10P:
		return MTKCAM_IPI_IMG_FMT_YVYU_Y210_PACKED;
	case V4L2_PIX_FMT_MTISP_UYVY10P:
		return MTKCAM_IPI_IMG_FMT_UYVY_Y210_PACKED;
	case V4L2_PIX_FMT_MTISP_VYUY10P:
		return MTKCAM_IPI_IMG_FMT_VYUY_Y210_PACKED;
	case V4L2_PIX_FMT_NV12_12:
		return MTKCAM_IPI_IMG_FMT_YUV_P012;
	case V4L2_PIX_FMT_NV21_12:
		return MTKCAM_IPI_IMG_FMT_YVU_P012;
	case V4L2_PIX_FMT_NV16_12:
		return MTKCAM_IPI_IMG_FMT_YUV_P212;
	case V4L2_PIX_FMT_NV61_12:
		return MTKCAM_IPI_IMG_FMT_YVU_P212;
	case V4L2_PIX_FMT_MTISP_NV12_12P:
		return MTKCAM_IPI_IMG_FMT_YUV_P012_PACKED;
	case V4L2_PIX_FMT_MTISP_NV21_12P:
		return MTKCAM_IPI_IMG_FMT_YVU_P012_PACKED;
	case V4L2_PIX_FMT_MTISP_NV16_12P:
		return MTKCAM_IPI_IMG_FMT_YUV_P212_PACKED;
	case V4L2_PIX_FMT_MTISP_NV61_12P:
		return MTKCAM_IPI_IMG_FMT_YVU_P212_PACKED;
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		return MTKCAM_IPI_IMG_FMT_BAYER8;
	case V4L2_PIX_FMT_MTISP_SBGGR8F:
	case V4L2_PIX_FMT_MTISP_SGBRG8F:
	case V4L2_PIX_FMT_MTISP_SGRBG8F:
	case V4L2_PIX_FMT_MTISP_SRGGB8F:
		return MTKCAM_IPI_IMG_FMT_FG_BAYER8;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
		return MTKCAM_IPI_IMG_FMT_BAYER10_UNPACKED;
	case V4L2_PIX_FMT_SBGGR10P:
	case V4L2_PIX_FMT_SGBRG10P:
	case V4L2_PIX_FMT_SGRBG10P:
	case V4L2_PIX_FMT_SRGGB10P:
		return MTKCAM_IPI_IMG_FMT_BAYER10_MIPI;
	case V4L2_PIX_FMT_MTISP_SBGGR10:
	case V4L2_PIX_FMT_MTISP_SGBRG10:
	case V4L2_PIX_FMT_MTISP_SGRBG10:
	case V4L2_PIX_FMT_MTISP_SRGGB10:
		return MTKCAM_IPI_IMG_FMT_BAYER10;
	case V4L2_PIX_FMT_MTISP_SBGGR10F:
	case V4L2_PIX_FMT_MTISP_SGBRG10F:
	case V4L2_PIX_FMT_MTISP_SGRBG10F:
	case V4L2_PIX_FMT_MTISP_SRGGB10F:
		return MTKCAM_IPI_IMG_FMT_FG_BAYER10;
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		return MTKCAM_IPI_IMG_FMT_BAYER12_UNPACKED;
	case V4L2_PIX_FMT_MTISP_SBGGR12:
	case V4L2_PIX_FMT_MTISP_SGBRG12:
	case V4L2_PIX_FMT_MTISP_SGRBG12:
	case V4L2_PIX_FMT_MTISP_SRGGB12:
		return MTKCAM_IPI_IMG_FMT_BAYER12;
	case V4L2_PIX_FMT_MTISP_SBGGR12F:
	case V4L2_PIX_FMT_MTISP_SGBRG12F:
	case V4L2_PIX_FMT_MTISP_SGRBG12F:
	case V4L2_PIX_FMT_MTISP_SRGGB12F:
		return MTKCAM_IPI_IMG_FMT_FG_BAYER12;
	case V4L2_PIX_FMT_SBGGR14:
	case V4L2_PIX_FMT_SGBRG14:
	case V4L2_PIX_FMT_SGRBG14:
	case V4L2_PIX_FMT_SRGGB14:
		return MTKCAM_IPI_IMG_FMT_BAYER14_UNPACKED;
	case V4L2_PIX_FMT_MTISP_SBGGR14:
	case V4L2_PIX_FMT_MTISP_SGBRG14:
	case V4L2_PIX_FMT_MTISP_SGRBG14:
	case V4L2_PIX_FMT_MTISP_SRGGB14:
		return MTKCAM_IPI_IMG_FMT_BAYER14;
	case V4L2_PIX_FMT_MTISP_SBGGR14F:
	case V4L2_PIX_FMT_MTISP_SGBRG14F:
	case V4L2_PIX_FMT_MTISP_SGRBG14F:
	case V4L2_PIX_FMT_MTISP_SRGGB14F:
		return MTKCAM_IPI_IMG_FMT_FG_BAYER14;
	case V4L2_PIX_FMT_SBGGR16:
	case V4L2_PIX_FMT_SGBRG16:
	case V4L2_PIX_FMT_SGRBG16:
	case V4L2_PIX_FMT_SRGGB16:
		return MTKCAM_IPI_IMG_FMT_BAYER16;
	case V4L2_PIX_FMT_MTISP_NV12_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_NV12;
	case V4L2_PIX_FMT_MTISP_NV21_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_NV21;
	case V4L2_PIX_FMT_MTISP_NV12_10_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_YUV_P010;
	case V4L2_PIX_FMT_MTISP_NV21_10_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_YVU_P010;
	case V4L2_PIX_FMT_MTISP_NV12_12_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_YUV_P012;
	case V4L2_PIX_FMT_MTISP_NV21_12_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_YVU_P012;
	case V4L2_PIX_FMT_MTISP_BAYER8_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_BAYER8;
	case V4L2_PIX_FMT_MTISP_BAYER10_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_BAYER10;
	case V4L2_PIX_FMT_MTISP_BAYER12_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_BAYER12;
	case V4L2_PIX_FMT_MTISP_BAYER14_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_BAYER14;
	case V4L2_PIX_FMT_MTISP_SGRB8F:
		return MTKCAM_IPI_IMG_FMT_FG_BAYER8_3P;
	case V4L2_PIX_FMT_MTISP_SGRB10F:
		return MTKCAM_IPI_IMG_FMT_FG_BAYER10_3P_PACKED;
	case V4L2_PIX_FMT_MTISP_SGRB12F:
		return MTKCAM_IPI_IMG_FMT_FG_BAYER12_3P_PACKED;
	default:
		return MTKCAM_IPI_IMG_FMT_UNKNOWN;
	}
}
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
	case V4L2_PIX_FMT_MTISP_BAYER8_UFBC:
	case V4L2_PIX_FMT_MTISP_BAYER10_UFBC:
	case V4L2_PIX_FMT_MTISP_BAYER12_UFBC:
	case V4L2_PIX_FMT_MTISP_BAYER14_UFBC:
		return 100;
	case MTKCAM_IPI_IMG_FMT_FG_BAYER8_3P:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER10_3P_PACKED:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER12_3P_PACKED:
		return 300;
	default:
		break;
	}
	return -1;
}

int mtk_cam_fill_pixfmt_mp(struct v4l2_pix_format_mplane *pixfmt,
			u32 pixelformat, u32 width, u32 height)
{
	struct v4l2_plane_pix_format *plane;
	unsigned int ipi_fmt = mtk_cam_get_img_fmt(pixelformat);
	u8 pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
	u32 stride;
	u32 aligned_width;
	u8 pixel_mode_shift = 0; /* todo: should set by resMgr */
	u8 bus_size;
	u8 i;

	pixfmt->width = width;
	pixfmt->height = height;
	pixfmt->pixelformat = pixelformat;
	plane = &pixfmt->plane_fmt[0];
	bus_size = mtk_cam_yuv_dma_bus_size(pixel_bits, pixel_mode_shift);
	plane->sizeimage = 0;

	if (is_mtk_format(pixelformat)) {
		const struct mtk_format_info *info;

		info = mtk_format_info(pixelformat);
		pixfmt->num_planes = info->mem_planes;

		if (!info)
			return -EINVAL;

		if (info->mem_planes == 1) {
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
				aligned_width = ALIGN(DIV_ROUND_UP(width
					* info->bit_r_num, info->bit_r_den), bus_size);
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
		} else {
			pr_debug("do not support non contiguous mplane\n");
		}
	} else {
		const struct v4l2_format_info *info;

		pr_debug("pixelformat:0x%x sizeimage:%d\n", pixelformat, plane->sizeimage);
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

	if (!desc->image) {
		active->fmt.meta.dataformat = default_fmt->fmt.meta.dataformat;
		active->fmt.meta.buffersize = default_fmt->fmt.meta.buffersize;
		return 0;
	}

	active->fmt.pix_mp.pixelformat = default_fmt->fmt.pix_mp.pixelformat;
	active->fmt.pix_mp.width = default_fmt->fmt.pix_mp.width;
	active->fmt.pix_mp.height = default_fmt->fmt.pix_mp.height;
	active->fmt.pix_mp.num_planes = default_fmt->fmt.pix_mp.num_planes;

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
				q->dev = find_larb(&cam->larb, 13);
				break;
			default:
				q->dev = find_larb(&cam->larb, 14);
				break;
			}
		} else if (video->uid.pipe_id >= MTKCAM_SUBDEV_MRAW_START &&
			video->uid.pipe_id < MTKCAM_SUBDEV_MRAW_END) {
			switch (video->uid.pipe_id) {
			case MTKCAM_SUBDEV_MRAW_0:
			case MTKCAM_SUBDEV_MRAW_2:
				q->dev = find_larb(&cam->larb, 25);
				break;
			default:
				q->dev = find_larb(&cam->larb, 26);
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
				q->dev = cam->raw.yuvs[0];
				break;
			default:
				q->dev = cam->raw.devs[0];
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
	strlcpy(vdev->name, video->desc.name, sizeof(vdev->name));

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

	/* f->description is filled in v4l_fill_fmtdesc function */
	f->pixelformat = node->desc.fmts[f->index].vfmt.fmt.pix_mp.pixelformat;
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
	struct mtk_cam_device *cam = video_drvdata(file);
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);
	struct mtk_cam_request *cam_req;
	struct media_request *req;
	struct v4l2_format *vfmt;
	struct mtk_cam_request_stream_data *stream_data;
	s32 fd;
	struct mtk_raw_pipeline *raw_pipeline;
	int raw_feature = 0;

	if (!vb2_is_busy(node->vdev.queue)) {
		/* Get the valid format */
		raw_pipeline = mtk_cam_dev_get_raw_pipeline(cam, node->uid.pipe_id);
		if (raw_pipeline)
			raw_feature = raw_pipeline->user_res.raw_res.feature;

		mtk_cam_video_set_fmt(node, f, raw_feature);

		/* Configure to video device */
		node->active_fmt = *f;
		return 0;
	}

	fd = mtk_cam_fmt_get_request(&f->fmt.pix_mp);
	if (fd < 0)
		return -EINVAL;

	req = media_request_get_by_fd(&cam->media_dev, fd);
	if (IS_ERR(req)) {
		dev_info(cam->dev,
			"%s:pipe(%d):%s:invalid request_fd:%d\n",
			__func__, node->uid.pipe_id, node->desc.name, fd);

		return -EINVAL;
	}

	cam_req = to_mtk_cam_req(req);
	dev_dbg(cam->dev,
		"%s:%s:pipe(%d):%s:pending s_fmt: pixelfmt(0x%x), w(%d), h(%d)\n",
		__func__, cam_req->req.debug_str, node->uid.pipe_id, node->desc.name,
		f->fmt.pix_mp.pixelformat,  f->fmt.pix_mp.width, f->fmt.pix_mp.height);

	stream_data = mtk_cam_req_get_s_data_no_chk(cam_req, node->uid.pipe_id, 0);
	stream_data->vdev_fmt_update |= (1 << node->desc.id);
	vfmt = mtk_cam_s_data_get_vfmt(stream_data, node->desc.id);
	*vfmt = *f;
	media_request_put(req);

	return 0;
}

int mtk_cam_video_set_fmt(struct mtk_cam_video_device *node, struct v4l2_format *f, int raw_feature)
{
	struct mtk_cam_device *cam = video_get_drvdata(&node->vdev);
	const struct v4l2_format *dev_fmt;
	struct v4l2_format try_fmt;
	s32 request_fd, i;
	u32 bytesperline, sizeimage;
	u32 is_hdr = 0, is_hdr_m2m = 0;

	dev_dbg(cam->dev,
			"%s:pipe(%d):%s:feature(0x%x)\n",
			__func__, node->uid.pipe_id, node->desc.name, raw_feature);

	request_fd = mtk_cam_fmt_get_request(&f->fmt.pix_mp);
	memset(&try_fmt, 0, sizeof(try_fmt));
	try_fmt.type = f->type;

	/* Validate pixelformat */
	dev_fmt = mtk_cam_dev_find_fmt(&node->desc, f->fmt.pix_mp.pixelformat);
	if (!dev_fmt) {
		dev_dbg(cam->dev, "unknown fmt:%d\n",
			f->fmt.pix_mp.pixelformat);
		dev_fmt = &node->desc.fmts[node->desc.default_fmt_idx].vfmt;
	}
	try_fmt.fmt.pix_mp.pixelformat = dev_fmt->fmt.pix_mp.pixelformat;

	/* Validate image width & height range */
	try_fmt.fmt.pix_mp.width = clamp_val(f->fmt.pix_mp.width,
					     IMG_MIN_WIDTH, IMG_MAX_WIDTH);
	try_fmt.fmt.pix_mp.height = clamp_val(f->fmt.pix_mp.height,
					      IMG_MIN_HEIGHT, IMG_MAX_HEIGHT);
	/* 4 bytes alignment for width */
	/* Todo: width and stride should align bus_size */
	try_fmt.fmt.pix_mp.width = ALIGN(try_fmt.fmt.pix_mp.width, IMG_PIX_ALIGN);
	try_fmt.fmt.pix_mp.num_planes = 1;

	if (raw_feature & MTK_CAM_FEATURE_HDR_MASK) {
		if (raw_feature & MTK_CAM_FEATURE_OFFLINE_M2M_MASK)
			is_hdr_m2m = 1;
		else
			is_hdr = 1;
	}

	/* support 1/2/3 plane */
	/* Note: vhdr m2m main stream is implicitly multiple plane */
	/* but not nego through try format */
	if ((node->desc.id == MTK_RAW_MAIN_STREAM_OUT && is_hdr) ||
		(node->desc.id == MTK_RAW_RAWI_2_IN && is_hdr_m2m)) {

		switch (raw_feature & MTK_CAM_FEATURE_HDR_MASK) {
		case STAGGER_2_EXPOSURE_LE_SE:
		case STAGGER_2_EXPOSURE_SE_LE:
		case MSTREAM_NE_SE:
		case MSTREAM_SE_NE:
			try_fmt.fmt.pix_mp.num_planes = 2;
			break;
		case STAGGER_3_EXPOSURE_LE_NE_SE:
		case STAGGER_3_EXPOSURE_SE_NE_LE:
			try_fmt.fmt.pix_mp.num_planes = 3;
			break;
		default:
			try_fmt.fmt.pix_mp.num_planes = 1;
		}
	}

	for (i = 0 ; i < try_fmt.fmt.pix_mp.num_planes ; i++)
		try_fmt.fmt.pix_mp.plane_fmt[i].bytesperline =
				f->fmt.pix_mp.plane_fmt[i].bytesperline;

	/* bytesperline & sizeimage calculation */
	if (node->desc.dma_port == MTKCAM_IPI_CAMSV_MAIN_OUT)
		cal_image_pix_mp(node->desc.id, &try_fmt.fmt.pix_mp, 3);
	else
		cal_image_pix_mp(node->desc.id, &try_fmt.fmt.pix_mp, 0);

	/* subsample */
	if (node->desc.id >= MTK_RAW_MAIN_STREAM_OUT &&
		node->desc.id < MTK_RAW_RZH1N2TO_1_OUT &&
		(raw_feature & MTK_CAM_FEATURE_SUBSAMPLE_MASK)) {
		switch (raw_feature) {
		case HIGHFPS_2_SUBSAMPLE:
			try_fmt.fmt.pix_mp.num_planes = 2;
			break;
		case HIGHFPS_4_SUBSAMPLE:
			try_fmt.fmt.pix_mp.num_planes = 4;
			break;
		case HIGHFPS_8_SUBSAMPLE:
			try_fmt.fmt.pix_mp.num_planes = 8;
			break;
		case HIGHFPS_16_SUBSAMPLE:
			try_fmt.fmt.pix_mp.num_planes =
				MAX_SUBSAMPLE_PLANE_NUM;
			break;
		default:
			try_fmt.fmt.pix_mp.num_planes = 1;
		}
		bytesperline = try_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
		sizeimage = try_fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
		for (i = 0 ; i < try_fmt.fmt.pix_mp.num_planes ; i++) {
			try_fmt.fmt.pix_mp.plane_fmt[i].bytesperline =
				bytesperline;
			try_fmt.fmt.pix_mp.plane_fmt[i].sizeimage =
				sizeimage;
		}
		dev_info(cam->dev, "%s id:%d raw_feature:0x%x stride:%d size:%d\n",
			 __func__, node->desc.id, raw_feature,
			 bytesperline, sizeimage);
	}
	/* TODO: support camsv meta header */
#if PDAF_READY
	/* add header size for vc channel */
	if (node->desc.dma_port == MTKCAM_IPI_CAMSV_MAIN_OUT &&
		node->desc.id == MTK_CAMSV_MAIN_STREAM_OUT)
		try_fmt.fmt.pix_mp.plane_fmt[0].sizeimage +=
		mtk_cam_get_meta_size(MTKCAM_IPI_CAMSV_MAIN_OUT);
#endif

	/* Constant format fields */
	try_fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_SRGB;
	try_fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
	try_fmt.fmt.pix_mp.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	try_fmt.fmt.pix_mp.quantization = V4L2_QUANTIZATION_DEFAULT;
	try_fmt.fmt.pix_mp.xfer_func = V4L2_XFER_FUNC_SRGB;

	*f = try_fmt;
	mtk_cam_fmt_set_request(&f->fmt.pix_mp, request_fd);

	return 0;
}

int mtk_cam_vidioc_try_fmt(struct file *file, void *fh,
			   struct v4l2_format *f)
{
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);
	int raw_feature = 0;

	if (is_raw_subdev(node->uid.pipe_id))
		raw_feature = mtk_cam_fmt_get_raw_feature(&f->fmt.pix_mp);

	mtk_cam_video_set_fmt(node, f, raw_feature);

	return 0;
}

int mtk_cam_vidioc_meta_enum_fmt(struct file *file, void *fh,
				 struct v4l2_fmtdesc *f)
{
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);

	if (f->index)
		return -EINVAL;

	/* f->description is filled in v4l_fill_fmtdesc function */
	f->pixelformat = node->active_fmt.fmt.meta.dataformat;
	f->flags = 0;

	return 0;
}

int mtk_cam_vidioc_g_meta_fmt(struct file *file, void *fh,
			      struct v4l2_format *f)
{
	struct mtk_cam_device *cam = video_drvdata(file);
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);
	struct mtk_cam_dev_node_desc *desc = &node->desc;
	const struct v4l2_format *default_fmt =
		&desc->fmts[desc->default_fmt_idx].vfmt;
	struct mtk_raw_pde_config *pde_cfg;
	struct mtk_cam_pde_info *pde_info;

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
	f->fmt.meta.dataformat = node->active_fmt.fmt.meta.dataformat;
	f->fmt.meta.buffersize = node->active_fmt.fmt.meta.buffersize;

	return 0;
}

int mtk_cam_vidioc_s_selection(struct file *file, void *fh,
				struct v4l2_selection *s)
{
	struct mtk_cam_device *cam = video_drvdata(file);
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);
	struct mtk_cam_request_stream_data *stream_data;
	struct mtk_cam_request *cam_req;
	struct media_request *req;
	struct v4l2_selection *vsel;
	s32 fd;

	fd = mtk_cam_selection_get_request(s);
	if (fd < 0)
		return -EINVAL;

	req = media_request_get_by_fd(&cam->media_dev, fd);
	if (IS_ERR(req)) {
		if (!vb2_is_busy(node->vdev.queue)) {
			dev_info(cam->dev,
				"%s:pipe(%d):%s: apply setting without fd\n",
				__func__, node->uid.pipe_id, node->desc.name);
			node->pending_crop = *s;
			return 0;
		}

		dev_info(cam->dev,
			"%s:pipe(%d):%s:invalid request_fd:%d\n",
			__func__, node->uid.pipe_id, node->desc.name, fd);

		return -EINVAL;
	}

	cam_req = to_mtk_cam_req(req);
	stream_data = mtk_cam_req_get_s_data_no_chk(cam_req, node->uid.pipe_id, 0);
	stream_data->vdev_selection_update |= (1 << node->desc.id);
	vsel = mtk_cam_s_data_get_vsel(stream_data, node->desc.id);
	*vsel = *s;
	dev_dbg(cam->dev,
		"%s:%s:pipe(%d):%s:pending vidioc_s_selection (%d,%d,%d,%d)\n",
		__func__, cam_req->req.debug_str, node->uid.pipe_id, node->desc.name,
		s->r.left, s->r.top, s->r.width, s->r.height);

	media_request_put(req);

	return 0;
}

void mtk_cam_fmt_set_raw_feature(struct v4l2_pix_format_mplane *fmt_mp, int raw_feature)
{
	u8 *reserved = fmt_mp->reserved;

	fmt_mp->flags = raw_feature & 0x000000FF;
	reserved[4] = (raw_feature & 0x0000FF00) >> 8;
	reserved[5] = (raw_feature & 0x00FF0000) >> 16;
	reserved[6] = (raw_feature & 0xFF000000) >> 24;
}

int mtk_cam_fmt_get_raw_feature(struct v4l2_pix_format_mplane *fmt_mp)
{
	int raw_feature = fmt_mp->flags;

	/**
	 * Current 8 bits flag is not enough so we also use the reserved[4-6] to
	 * save the feature flags.
	 */
	raw_feature |= ((unsigned int)fmt_mp->reserved[4]) << 8 & 0x0000FF00;
	raw_feature |= ((unsigned int)fmt_mp->reserved[5]) << 16 & 0x00FF0000;
	raw_feature |= ((unsigned int)fmt_mp->reserved[6]) << 24 & 0xFF000000;

	return raw_feature;
}

int mtk_cam_fmt_get_request(struct v4l2_pix_format_mplane *fmt_mp)
{
	int field;
	int reserved_fields = 4;
	s32 request_fd = 0;

	for (field = 0; field < reserved_fields; field++) {
		request_fd +=
			fmt_mp->reserved[field] << BITS_PER_BYTE * field;
		fmt_mp->reserved[field] = 0;
	}

	return request_fd;
}

void mtk_cam_fmt_set_request(struct v4l2_pix_format_mplane *fmt_mp, int request_fd)
{
	u8 *reserved = fmt_mp->reserved;

	reserved[0] = request_fd & 0x000000FF;
	reserved[1] = (request_fd & 0x0000FF00) >> 8;
	reserved[2] = (request_fd & 0x00FF0000) >> 16;
	reserved[3] = (request_fd & 0xFF000000) >> 24;
}

int mtk_cam_selection_get_request(struct v4l2_selection *crop)
{
	s32 request_fd = 0;

	request_fd = crop->reserved[0];
	crop->reserved[0] = 0;

	return request_fd;
}

void mtk_cam_selection_set_request(struct v4l2_selection *crop, int request_fd)
{
	u32 *reserved = crop->reserved;

	reserved[0] = request_fd;
}

