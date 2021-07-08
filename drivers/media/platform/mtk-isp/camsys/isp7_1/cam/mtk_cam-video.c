// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>

#include "mtk_cam.h"
#include "mtk_cam-video.h"
#include "mtk_cam-meta.h"
#include "mtk_cam-v4l2.h"

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
	unsigned int bus_size = ALIGN(bpp, 32) << pixel_mode_shift;

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
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vq);
	unsigned int max_buffer_count = node->desc.max_buf_count;
	const struct v4l2_format *fmt = &node->active_fmt;
	unsigned int size;
	int i;

	/* Check the limitation of buffer size */
	if (max_buffer_count)
		*num_buffers = clamp_val(*num_buffers, 1, max_buffer_count);

	if (node->desc.smem_alloc)
		vq->dma_attrs |= DMA_ATTR_NO_KERNEL_MAPPING;

	if (vq->type == V4L2_BUF_TYPE_META_OUTPUT ||
	    vq->type == V4L2_BUF_TYPE_META_CAPTURE)
		size = fmt->fmt.meta.buffersize;
	else
		size = fmt->fmt.pix_mp.plane_fmt[0].sizeimage;

	if (vq->type == V4L2_BUF_TYPE_META_OUTPUT)
		vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;

	/* Add for q.create_bufs with fmt.g_sizeimage(p) / 2 test */
	if (*num_planes) {
		if (sizes[0] < size || *num_planes != 1)
			return -EINVAL;
	} else {
		if (node->raw_feature && fmt->fmt.pix_mp.num_planes > 1) {
			*num_planes = fmt->fmt.pix_mp.num_planes;
			for (i = 0; i < *num_planes; i++)
				sizes[i] = fmt->fmt.pix_mp.plane_fmt[i].sizeimage;
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

static void mtk_cam_vb2_return_all_buffers(struct mtk_cam_device *cam,
					   struct mtk_cam_video_device *node,
					   enum vb2_buffer_state state)
{
	struct mtk_cam_buffer *buf, *buf_prev;
	unsigned long flags;

	spin_lock_irqsave(&node->buf_list_lock, flags);
	list_for_each_entry_safe(buf, buf_prev, &node->buf_list, list) {
		list_del(&buf->list);
		vb2_buffer_done(&buf->vbb.vb2_buf, state);
	}
	spin_unlock_irqrestore(&node->buf_list_lock, flags);
}

static int mtk_cam_vb2_start_streaming(struct vb2_queue *vq,
				       unsigned int count)
{
	struct mtk_cam_device *cam = vb2_get_drv_priv(vq);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vq);
	struct media_entity *entity = &node->vdev.entity;
	struct mtk_cam_ctx *ctx;
	int ret;

	mutex_lock(&cam->op_lock);

	/* check entity is linked */
	if (!node->enabled) {
		dev_info(cam->dev,
			"%s: stream on failed, node is not enabled\n",
			node->desc.name);
		ret = -ENOLINK;
		goto fail_unlock;
	}

	if (!entity->pipe) {
		ctx = mtk_cam_start_ctx(cam, node);
		if (!ctx) {
			ret = -ENOLINK;
			goto fail_unlock;
		}
	} else {
		ctx = mtk_cam_find_ctx(cam, entity);
		if (WARN_ON(!ctx)) {
			ret = -ENOLINK;
			goto fail_unlock;
		}
	}

	if (node->uid.pipe_id >= MTKCAM_SUBDEV_RAW_START &&
		node->uid.pipe_id < MTKCAM_SUBDEV_RAW_END)
		ctx->used_raw_dmas |= node->desc.dma_port;

	cam->streaming_pipe |= (1 << node->uid.pipe_id);
	ctx->streaming_pipe |= (1 << node->uid.pipe_id);
	ctx->streaming_node_cnt++;

#if CCD_READY
	if (ctx->streaming_node_cnt == 1)
		if (node->uid.pipe_id >= MTKCAM_SUBDEV_RAW_START &&
			node->uid.pipe_id < MTKCAM_SUBDEV_RAW_END)
			isp_composer_create_session(ctx);
#endif

	if (ctx->streaming_node_cnt < ctx->enabled_node_cnt) {
		mutex_unlock(&cam->op_lock);
		return 0;
	}

	/* all enabled nodes are streaming, enable all subdevs */
	ret = mtk_cam_ctx_stream_on(ctx);
	if (ret)
		goto fail_stop_ctx;

	mutex_unlock(&cam->op_lock);
	return 0;

fail_stop_ctx:
	cam->streaming_pipe &= ~(1 << node->uid.pipe_id);
	mtk_cam_stop_ctx(ctx, entity);
fail_unlock:
	mtk_cam_dev_req_cleanup(cam);
	mutex_unlock(&cam->op_lock);
	mtk_cam_vb2_return_all_buffers(cam, node, VB2_BUF_STATE_QUEUED);

	return ret;
}

static void mtk_cam_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct mtk_cam_device *cam = vb2_get_drv_priv(vq);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vq);
	struct device *dev = cam->dev;
	struct mtk_cam_ctx *ctx;

	mutex_lock(&cam->op_lock);

	ctx = mtk_cam_find_ctx(cam, &node->vdev.entity);
	if (WARN_ON(!ctx)) {
		mutex_unlock(&cam->op_lock);
		return;
	}

	dev_dbg(dev, "%s ctx:%d node:%d count info:%d\n", __func__,
		ctx->stream_id, node->desc.id, ctx->streaming_node_cnt);

	if (ctx->streaming_node_cnt == ctx->enabled_node_cnt)
		mtk_cam_ctx_stream_off(ctx);

	mtk_cam_vb2_return_all_buffers(cam, node, VB2_BUF_STATE_ERROR);
	ctx->streaming_node_cnt--;
	if (ctx->streaming_node_cnt) {
		mutex_unlock(&cam->op_lock);
		return;
	}

	cam->streaming_pipe &= ~(1 << node->uid.pipe_id);
	mtk_cam_dev_req_cleanup(cam);
	mtk_cam_stop_ctx(ctx, &node->vdev.entity);
	mutex_unlock(&cam->op_lock);
}

static void set_payload(struct mtk_cam_uapi_meta_hw_buf *buf,
			unsigned int size, unsigned long *offset)
{
	buf->offset = *offset;
	buf->size = size;
	*offset += size;
}

static void mtk_cam_set_meta_stats_info(u32 dma_port, void *vaddr)
{
	struct mtk_cam_uapi_meta_raw_stats_0 *stats0;
	struct mtk_cam_uapi_meta_raw_stats_1 *stats1;
	unsigned long offset;

	switch (dma_port) {
	case MTKCAM_IPI_RAW_META_STATS_0:
		stats0 = (struct mtk_cam_uapi_meta_raw_stats_0 *)vaddr;
		offset = sizeof(*stats0);
		set_payload(&stats0->ae_awb_stats.aao_buf, MTK_CAM_UAPI_AAO_MAX_BUF_SIZE, &offset);
		set_payload(&stats0->ae_awb_stats.aaho_buf,
			MTK_CAM_UAPI_AAHO_MAX_BUF_SIZE, &offset);
		set_payload(&stats0->ltm_stats.ltmso_buf, MTK_CAM_UAPI_LTMSO_SIZE, &offset);
		set_payload(&stats0->flk_stats.flko_buf, MTK_CAM_UAPI_FLK_MAX_BUF_SIZE, &offset);
		set_payload(&stats0->tsf_stats.tsfo_r1_buf, MTK_CAM_UAPI_TSFSO_SIZE, &offset);
		set_payload(&stats0->tsf_stats.tsfo_r2_buf, MTK_CAM_UAPI_TSFSO_SIZE, &offset);
		set_payload(&stats0->tncy_stats.tncsyo_buf, MTK_CAM_UAPI_TNCSYO_SIZE, &offset);
		break;
	case MTKCAM_IPI_RAW_META_STATS_1:
		stats1 = (struct mtk_cam_uapi_meta_raw_stats_1 *)vaddr;
		offset = sizeof(*stats1);
		set_payload(&stats1->af_stats.afo_buf, MTK_CAM_UAPI_AFO_MAX_BUF_SIZE, &offset);
		break;
	case MTKCAM_IPI_RAW_META_STATS_2:
		//todo
		pr_info("stats 2 not support");
		break;
	default:
		pr_debug("%s: dma_port err\n", __func__);
		break;
	}
}

/* TODO: support camsv meta header */
#if PDAF_READY
static void mtk_cam_sv_set_meta_stats_info(
	u32 dma_port, void *vaddr, unsigned int width,
	unsigned int height, unsigned int stride)
{
	struct mtk_cam_uapi_meta_camsv_stats_0 *sv_stats0;
	unsigned long offset;
	unsigned int size;

	switch (dma_port) {
	case MTKCAM_IPI_CAMSV_MAIN_OUT:
		size = stride * height;
		sv_stats0 = (struct mtk_cam_uapi_meta_camsv_stats_0 *)vaddr;
		offset = sizeof(*sv_stats0);
		set_payload(&sv_stats0->pd_stats.pdo_buf, size, &offset);
		sv_stats0->pd_stats_enabled = 1;
		sv_stats0->pd_stats.stats_src.width = width;
		sv_stats0->pd_stats.stats_src.height = height;
		sv_stats0->pd_stats.stride = stride;
		break;
	default:
		pr_debug("%s: dma_port err\n", __func__);
		break;
	}
}
#endif

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
		return 1;
	break;
	default:
		return 0;
	break;
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
	};
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(formats); ++i)
		if (formats[i].format == format)
			return &formats[i];
	return NULL;
}

int mtk_cam_fill_img_buf(struct mtkcam_ipi_img_output *img_out,
						struct v4l2_format *f, dma_addr_t daddr)
{
	u32 pixelformat = f->fmt.pix_mp.pixelformat;
	u32 width = f->fmt.pix_mp.width;
	u32 height = f->fmt.pix_mp.height;
	struct v4l2_plane_pix_format *plane = &f->fmt.pix_mp.plane_fmt[0];
	u32 stride = plane->bytesperline;
	u32 aligned_width;
	unsigned int addr_offset = 0;
	int i;
	(void) width;

	if (is_mtk_format(pixelformat)) {
		const struct mtk_format_info *info;

		info = mtk_format_info(pixelformat);
		if (!info)
			return -EINVAL;

		aligned_width = stride / info->bpp[0];
		if (info->mem_planes == 1) {
			for (i = 0; i < info->comp_planes; i++) {
				unsigned int hdiv = (i == 0) ? 1 : info->hdiv;
				unsigned int vdiv = (i == 0) ? 1 : info->vdiv;

				img_out->buf[i][0].iova = daddr + addr_offset;
				img_out->fmt.stride[i] = info->bpp[i] *
					DIV_ROUND_UP(aligned_width, hdiv);
				img_out->buf[i][0].size = img_out->fmt.stride[i]
					* DIV_ROUND_UP(height, vdiv);
				addr_offset += img_out->buf[i][0].size;
				pr_debug("plane:%d stride:%d plane_size:%d addr:0x%x\n",
					i, img_out->fmt.stride[i], img_out->buf[i][0].size,
					img_out->buf[i][0].iova);
			}
		} else {
			pr_debug("do not support non contiguous mplane\n");
		}
	} else {
		const struct v4l2_format_info *info;

		info = v4l2_format_info(pixelformat);
		if (!info)
			return -EINVAL;

		aligned_width = stride / info->bpp[0];
		if (info->mem_planes == 1) {
			for (i = 0; i < info->comp_planes; i++) {
				unsigned int hdiv = (i == 0) ? 1 : info->hdiv;
				unsigned int vdiv = (i == 0) ? 1 : info->vdiv;

				img_out->buf[i][0].iova = daddr + addr_offset;
				img_out->fmt.stride[i] = info->bpp[i] *
					DIV_ROUND_UP(aligned_width, hdiv);
				img_out->buf[i][0].size = img_out->fmt.stride[i]
					* DIV_ROUND_UP(height, vdiv);
				addr_offset += img_out->buf[i][0].size;
				pr_debug("stride:%d plane_size:%d addr:0x%x\n",
					img_out->fmt.stride[i], img_out->buf[i][0].size,
					img_out->buf[i][0].iova);
			}
		} else {
			pr_debug("do not support non contiguous mplane\n");
		}
	}

	return 0;
}
int mtk_cam_hdr_buf_update(struct vb2_buffer *vb,
		struct mtkcam_ipi_frame_param *frame_param, enum hdr_scenario_id scenario)
{
	struct mtk_cam_buffer *buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	struct v4l2_format *f = &node->active_fmt;
	unsigned int desc_id;
	int i;

	if (scenario != STAGGER_M2M)
		desc_id = node->desc.id - MTK_RAW_SOURCE_BEGIN;
	else
		desc_id = node->desc.id - MTK_RAW_RAWI_2_IN;

	for (i = 0 ; i < vb->num_planes; i++) {
		vb->planes[i].data_offset =
			i * f->fmt.pix_mp.plane_fmt[i].sizeimage;
		if (mtk_cam_get_sensor_exposure_num(node->raw_feature) == 3) {
			if (i == 0) { /* camsv1*/
				int in_node = MTKCAM_IPI_RAW_RAWI_2;

				frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
				.iova = buf->daddr + vb->planes[i].data_offset;
				frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
					.size = f->fmt.pix_mp.plane_fmt[i].sizeimage;
			} else if (i == 1) { /*camsv2*/
				int in_node = MTKCAM_IPI_RAW_RAWI_3;

				frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
				.iova = buf->daddr + vb->planes[i].data_offset;
				frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
					.size = f->fmt.pix_mp.plane_fmt[i].sizeimage;
			} else if (i == 2) { /*raw*/
				if (scenario == STAGGER_M2M) {
					int in_node = MTKCAM_IPI_RAW_RAWI_6;

					frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
						.iova = buf->daddr + vb->planes[i].data_offset;
					frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
						.size = f->fmt.pix_mp.plane_fmt[i].sizeimage;
				} else {
					frame_param->img_outs[desc_id].buf[0][0].iova =
						buf->daddr + vb->planes[i].data_offset;
				}
			}
		} else if (mtk_cam_get_sensor_exposure_num(node->raw_feature) == 2) {
			if (i == 0) { /* camsv1*/
				int in_node = MTKCAM_IPI_RAW_RAWI_2;

				frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
				.iova = buf->daddr + vb->planes[i].data_offset;
				frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
					.size = f->fmt.pix_mp.plane_fmt[i].sizeimage;
			} else if (i == 1) { /*raw*/
				if (scenario == STAGGER_M2M) {
					int in_node = MTKCAM_IPI_RAW_RAWI_6;

					frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
						.iova = buf->daddr + vb->planes[i].data_offset;
					frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
						.size = f->fmt.pix_mp.plane_fmt[i].sizeimage;
				} else {
					frame_param->img_outs[desc_id].buf[0][0].iova =
						buf->daddr + vb->planes[i].data_offset;
				}
			}
		}
	}

	return 0;
}
static void mtk_cam_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_cam_device *cam = vb2_get_drv_priv(vb->vb2_queue);
	unsigned int pipe_id;
	struct mtk_cam_buffer *buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	struct mtk_cam_request *req = to_mtk_cam_req(vb->request);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	struct device *dev = cam->dev;
	unsigned long flags;
	unsigned int desc_id;
	unsigned int dma_port = node->desc.dma_port;
	unsigned int width, height, stride;
	void *vaddr;
	int i, plane_i;
	struct v4l2_format *f = &node->active_fmt;
	struct mtkcam_ipi_img_output *img_out;
	struct mtkcam_ipi_frame_param *frame_param;
	struct mtkcam_ipi_meta_input *meta_in;
	struct mtkcam_ipi_meta_output *meta_out;
	struct mtk_camsv_frame_params *sv_frame_params;
	struct mtk_raw_pipeline *raw_pipline;

	dma_port = node->desc.dma_port;
	pipe_id = node->uid.pipe_id;
	frame_param = &req->stream_data[pipe_id].frame_params;
	sv_frame_params = &req->stream_data[pipe_id].sv_frame_params;
	raw_pipline = mtk_cam_dev_get_raw_pipeline(cam, pipe_id);

	dev_dbg(dev, "%s: node:%d fd:%d idx:%d\n", __func__,
		node->desc.id, buf->vbb.request_fd, buf->vbb.vb2_buf.index);
	/* TODO: add vdev_node.enabled check to prevent an useless buffer */

	/* added the buffer into the tracking list */
	spin_lock_irqsave(&node->buf_list_lock, flags);
	list_add_tail(&buf->list, &node->buf_list);
	spin_unlock_irqrestore(&node->buf_list_lock, flags);

	/* update buffer internal address */
	switch (dma_port) {
	case MTKCAM_IPI_RAW_RAWI_2:
		if (node->raw_feature & MTK_CAM_FEATURE_STAGGER_M2M_MASK) {
			mtk_cam_hdr_buf_update(vb, frame_param, STAGGER_M2M);
			frame_param->raw_param.hardware_scenario =
				MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER;
			frame_param->raw_param.exposure_num =
				mtk_cam_get_sensor_exposure_num(node->raw_feature);
		} else {
			desc_id = node->desc.id - MTK_RAW_RAWI_2_IN;
			frame_param->img_ins[desc_id].buf[0].iova = buf->daddr;
			frame_param->raw_param.hardware_scenario =
				MTKCAM_IPI_HW_PATH_ON_THE_FLY_M2M_REINJECT;
		}
		break;
	case MTKCAM_IPI_RAW_IMGO:
		/* TODO: support sub-sampling multi-plane buffer */
		desc_id = node->desc.id-MTK_RAW_SOURCE_BEGIN;
		frame_param->img_outs[desc_id].buf[0][0].iova = buf->daddr;
		if (raw_pipline->res_config.raw_path == V4L2_MTK_CAM_RAW_PATH_SELECT_LSC)
			frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_AFTER_LSC;
		else
			/* un-processed raw frame */
			frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_UNPROCESSED;

		dev_dbg(dev, "%s: node:%d fd:%d idx:%d raw_path(%d) ipi imgo_path_sel(%d))\n",
			__func__, node->desc.id, buf->vbb.request_fd, buf->vbb.vb2_buf.index,
			raw_pipline->res_config.raw_path, frame_param->raw_param.imgo_path_sel);

		if (node->raw_feature & MTK_CAM_FEATURE_STAGGER_MASK)
			mtk_cam_hdr_buf_update(vb, frame_param, STAGGER_ON_THE_FLY);
		if (node->raw_feature & MTK_CAM_FEATURE_SUBSAMPLE_MASK) {
			for (i = 0 ; i < vb->num_planes; i++) {
				vb->planes[i].data_offset =
					i * f->fmt.pix_mp.plane_fmt[i].sizeimage;
				frame_param->img_outs[desc_id].buf[0][i].iova =
						buf->daddr + vb->planes[i].data_offset;
			}
		}
		break;
	case MTKCAM_IPI_RAW_YUVO_1:
	case MTKCAM_IPI_RAW_YUVO_2:
	case MTKCAM_IPI_RAW_YUVO_3:
	case MTKCAM_IPI_RAW_YUVO_4:
	case MTKCAM_IPI_RAW_YUVO_5:
	case MTKCAM_IPI_RAW_RZH1N2TO_1:
	case MTKCAM_IPI_RAW_RZH1N2TO_2:
	case MTKCAM_IPI_RAW_RZH1N2TO_3:
	case MTKCAM_IPI_RAW_DRZS4NO_1:
	case MTKCAM_IPI_RAW_DRZS4NO_2:
	case MTKCAM_IPI_RAW_DRZS4NO_3:
		/* TODO: support sub-sampling multi-plane buffer */
		desc_id = node->desc.id-MTK_RAW_SOURCE_BEGIN;
		img_out = &frame_param->img_outs[desc_id];
		mtk_cam_fill_img_buf(img_out, f, buf->daddr);
		if (node->raw_feature & MTK_CAM_FEATURE_SUBSAMPLE_MASK) {
			for (i = 0 ; i < vb->num_planes; i++) {
				const struct mtk_format_info *mtk_info;
				const struct v4l2_format_info *v4l2_info;
				int comp_planes = 1;

				if (is_mtk_format(f->fmt.pix_mp.pixelformat)) {
					mtk_info = mtk_format_info(f->fmt.pix_mp.pixelformat);
					comp_planes = mtk_info->comp_planes;
				} else {
					v4l2_info = v4l2_format_info(f->fmt.pix_mp.pixelformat);
					comp_planes = v4l2_info->comp_planes;
				}
				vb->planes[i].data_offset =
					i * f->fmt.pix_mp.plane_fmt[i].sizeimage;
				frame_param->img_outs[desc_id].buf[0][i].iova =
						buf->daddr + vb->planes[i].data_offset;
				for (plane_i = 1 ; plane_i < comp_planes; plane_i++) {
					frame_param->img_outs[desc_id].buf[plane_i][i].iova =
						frame_param->img_outs[desc_id].buf[0][i].iova +
						frame_param->img_outs[desc_id].buf[0][0].size;
				}
			}
		}
		break;

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
		desc_id = node->desc.id-MTK_RAW_META_OUT_BEGIN;
		meta_out = &frame_param->meta_outputs[desc_id];
		meta_out->buf.ccd_fd = vb->planes[0].m.fd;
		meta_out->buf.size = node->active_fmt.fmt.meta.buffersize;
		meta_out->buf.iova = buf->daddr;
		meta_out->uid.id = dma_port;
		vaddr = vb2_plane_vaddr(vb, 0);
		mtk_cam_set_meta_stats_info(dma_port, vaddr);
		break;
	case MTKCAM_IPI_CAMSV_MAIN_OUT:
		/* TODO: support multiple vc and meta header  */
#if PDAF_READY
		sv_frame_params->img_out.buf[0][0].iova = buf->daddr +
			sizeof(struct mtk_cam_uapi_meta_camsv_stats_0);
#else
		sv_frame_params->img_out.buf[0][0].iova = buf->daddr;
#endif
		width = node->active_fmt.fmt.pix_mp.width;
		height = node->active_fmt.fmt.pix_mp.height;
		stride = node->active_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
#if PDAF_READY
		vaddr = vb2_plane_vaddr(vb, 0);
		mtk_cam_sv_set_meta_stats_info(node->desc.dma_port, vaddr, width, height, stride);
#endif
		break;
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
	.buf_init = mtk_cam_vb2_buf_init,
	.buf_prepare = mtk_cam_vb2_buf_prepare,
	.buf_out_validate = mtk_cam_vb2_buf_out_validate,
	.start_streaming = mtk_cam_vb2_start_streaming,
	.stop_streaming = mtk_cam_vb2_stop_streaming,
	.buf_queue = mtk_cam_vb2_buf_queue,
	.buf_cleanup = mtk_cam_vb2_buf_cleanup,
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
	default:
		return MTKCAM_IPI_IMG_FMT_UNKNOWN;
	}
}
int mtk_cam_get_plane_num(unsigned int ipi_fmt)
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
						return 1;
	case MTKCAM_IPI_IMG_FMT_YUV_422_2P:
	case MTKCAM_IPI_IMG_FMT_YVU_422_2P:
	case MTKCAM_IPI_IMG_FMT_YUV_420_2P:
	case MTKCAM_IPI_IMG_FMT_YVU_420_2P:
	case MTKCAM_IPI_IMG_FMT_YUV_P210:
	case MTKCAM_IPI_IMG_FMT_YVU_P210:
	case MTKCAM_IPI_IMG_FMT_YUV_P010:
	case MTKCAM_IPI_IMG_FMT_YVU_P010:
	case MTKCAM_IPI_IMG_FMT_YUV_P212:
	case MTKCAM_IPI_IMG_FMT_YVU_P212:
	case MTKCAM_IPI_IMG_FMT_YUV_P012:
	case MTKCAM_IPI_IMG_FMT_YVU_P012:
	case MTKCAM_IPI_IMG_FMT_YUV_P210_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P210_PACKED:
	case MTKCAM_IPI_IMG_FMT_YUV_P010_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P010_PACKED:
	case MTKCAM_IPI_IMG_FMT_YUV_P212_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P212_PACKED:
	case MTKCAM_IPI_IMG_FMT_YUV_P012_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P012_PACKED:
						return 2;
	case MTKCAM_IPI_IMG_FMT_YUV_422_3P:
	case MTKCAM_IPI_IMG_FMT_YVU_422_3P:
	case MTKCAM_IPI_IMG_FMT_YUV_420_3P:
	case MTKCAM_IPI_IMG_FMT_YVU_420_3P:
						return 3;

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
			/* width should be bus_size align */
			aligned_width = ALIGN(DIV_ROUND_UP(width
				* info->bit_r_num, info->bit_r_den), bus_size);
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
		&desc->fmts[desc->default_fmt_idx];

	active->type = desc->buf_type;

	if (!desc->image) {
		active->fmt.meta.dataformat = default_fmt->fmt.meta.dataformat;
		active->fmt.meta.buffersize = default_fmt->fmt.meta.buffersize;
		return 0;
	}

	active->fmt.pix_mp.pixelformat = default_fmt->fmt.pix_mp.pixelformat;
	active->fmt.pix_mp.width = default_fmt->fmt.pix_mp.width;
	active->fmt.pix_mp.height = default_fmt->fmt.pix_mp.height;

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
	if (q->type == V4L2_BUF_TYPE_META_OUTPUT)
		q->io_modes = VB2_MMAP | VB2_DMABUF;
	else
		q->io_modes = VB2_MMAP | VB2_DMABUF;
	if (video->desc.smem_alloc) {
		q->bidirectional = 1;
		/* q->dev = cam->smem_dev; FIXME impl for real SCP */
		q->dev = cam->dev;
	} else {
		if (video->uid.pipe_id >= MTKCAM_SUBDEV_CAMSV_START &&
			video->uid.pipe_id < MTKCAM_SUBDEV_CAMSV_END) {
			switch (video->uid.pipe_id) {
			case MTKCAM_SUBDEV_CAMSV_0:
			case MTKCAM_SUBDEV_CAMSV_2:
			case MTKCAM_SUBDEV_CAMSV_3:
				q->dev = find_larb(&cam->larb, 13);
				break;
			default:
				q->dev = find_larb(&cam->larb, 14);
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
	q->mem_ops = &vb2_dma_contig_memops;
	q->drv_priv = cam;
	q->buf_struct_size = sizeof(struct mtk_cam_buffer);
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;

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

	INIT_LIST_HEAD(&video->buf_list);
	spin_lock_init(&video->buf_list_lock);

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
		fmt = &desc->fmts[i];
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
	f->pixelformat = node->desc.fmts[f->index].fmt.pix_mp.pixelformat;
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

	if (vb2_is_busy(node->vdev.queue)) {
		s32 fd = get_format_request_fd(&f->fmt.pix_mp);

		if (fd > 0) {
			node->pending_fmt = *f;
			set_format_request_fd(&node->pending_fmt.fmt.pix_mp, fd);
			return 0;
		}

		dev_dbg(cam->dev, "%s: queue is busy\n", __func__);
		return -EBUSY;
	}

	/* Get the valid format */
	mtk_cam_vidioc_try_fmt(file, fh, f);
	/* Configure to video device */
	node->active_fmt = *f;
	return 0;
}

int video_try_fmt(struct mtk_cam_video_device *node, struct v4l2_format *f)
{
	struct mtk_cam_device *cam = video_get_drvdata(&node->vdev);
	const struct v4l2_format *dev_fmt;
	struct v4l2_format try_fmt;
	s32 request_fd, i;
	u32 bytesperline, sizeimage;

	request_fd = get_format_request_fd(&f->fmt.pix_mp);
	memset(&try_fmt, 0, sizeof(try_fmt));
	try_fmt.type = f->type;

	/* Validate pixelformat */
	dev_fmt = mtk_cam_dev_find_fmt(&node->desc, f->fmt.pix_mp.pixelformat);
	if (!dev_fmt) {
		dev_dbg(cam->dev, "unknown fmt:%d\n",
			f->fmt.pix_mp.pixelformat);
		dev_fmt = &node->desc.fmts[node->desc.default_fmt_idx];
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
	/* support 1/2/3 plane */
	if (node->desc.id == MTK_RAW_MAIN_STREAM_OUT &&
		(node->raw_feature & MTK_CAM_FEATURE_STAGGER_MASK)) {
		switch (node->raw_feature) {
		case STAGGER_2_EXPOSURE_LE_SE:
		case STAGGER_2_EXPOSURE_SE_LE:
			try_fmt.fmt.pix_mp.num_planes = 2;
			break;
		case STAGGER_3_EXPOSURE_LE_NE_SE:
		case STAGGER_3_EXPOSURE_SE_NE_LE:
			try_fmt.fmt.pix_mp.num_planes = 3;
			break;
		default:
			try_fmt.fmt.pix_mp.num_planes = 1;
		}
	} else if (node->desc.id == MTK_RAW_RAWI_2_IN &&
		(node->raw_feature & MTK_CAM_FEATURE_STAGGER_M2M_MASK)) {
		switch (node->raw_feature) {
		case STAGGER_M2M_2_EXPOSURE_LE_SE:
		case STAGGER_M2M_2_EXPOSURE_SE_LE:
			try_fmt.fmt.pix_mp.num_planes = 2;
			break;
		case STAGGER_M2M_3_EXPOSURE_LE_NE_SE:
		case STAGGER_M2M_3_EXPOSURE_SE_NE_LE:
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
		(node->raw_feature & MTK_CAM_FEATURE_SUBSAMPLE_MASK)) {
		switch (node->raw_feature) {
		case HIGHFPS_2_SUBSAMPLE:
			try_fmt.fmt.pix_mp.num_planes = 2;
			break;
		case HIGHFPS_4_SUBSAMPLE:
			try_fmt.fmt.pix_mp.num_planes = 4;
			break;
		case HIGHFPS_8_SUBSAMPLE:
			try_fmt.fmt.pix_mp.num_planes = 8;
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
		dev_info(cam->dev, "%s id:%d raw_feature:%d stride:%d size:%d\n",
			__func__, node->desc.id, node->raw_feature,
			bytesperline, sizeimage);
	}
	/* TODO: support camsv meta header */
#if PDAF_READY
	/* add header size for vc channel */
	if (node->desc.dma_port == MTKCAM_IPI_CAMSV_MAIN_OUT)
		try_fmt.fmt.pix_mp.plane_fmt[0].sizeimage +=
		sizeof(struct mtk_cam_uapi_meta_camsv_stats_0);
#endif

	/* Constant format fields */
	try_fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_SRGB;
	try_fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
	try_fmt.fmt.pix_mp.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	try_fmt.fmt.pix_mp.quantization = V4L2_QUANTIZATION_DEFAULT;
	try_fmt.fmt.pix_mp.xfer_func = V4L2_XFER_FUNC_SRGB;

	*f = try_fmt;
	set_format_request_fd(&f->fmt.pix_mp, request_fd);
	return 0;
}

int mtk_cam_vidioc_try_fmt(struct file *file, void *fh,
			   struct v4l2_format *f)
{
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);

	video_try_fmt(node, f);
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
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);

	f->fmt.meta.dataformat = node->active_fmt.fmt.meta.dataformat;
	f->fmt.meta.buffersize = node->active_fmt.fmt.meta.buffersize;

	return 0;
}

int mtk_cam_vidioc_s_selection(struct file *file, void *fh,
				struct v4l2_selection *s)
{
	struct mtk_cam_device *cam = video_drvdata(file);
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);

	if (vb2_is_busy(node->vdev.queue)) {
		s32 fd = get_crop_request_fd(s);

		if (fd > 0) {
			node->pending_crop = *s;
			set_crop_request_fd(&node->pending_crop, fd);
		} else {
			dev_dbg(cam->dev, "%s: queue is busy\n", __func__);
			return -EBUSY;
		}
	} else
		node->pending_crop = *s;

	return 0;
}

