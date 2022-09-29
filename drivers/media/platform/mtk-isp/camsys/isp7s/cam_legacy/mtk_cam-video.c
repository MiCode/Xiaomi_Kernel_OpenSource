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

static int debug_cam_video;
module_param(debug_cam_video, int, 0644);

#undef dev_dbg
#define dev_dbg(dev, fmt, arg...)		\
	do {					\
		if (debug_cam_video >= 1)	\
			dev_info(dev, fmt,	\
				## arg);	\
	} while (0)

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
				} else if (raw_pipeline &&
					   !mtk_cam_scen_is_sensor_normal(
						   &raw_pipeline->user_res.raw_res.scen) &&
					fmt->fmt.pix_mp.num_planes > 1) {
					*num_planes = fmt->fmt.pix_mp.num_planes;

					for (i = 0; i < *num_planes; i++)
						sizes[i] = size;
				} else {
					*num_planes = 1;
					sizes[0] = size;
				}
			} else if (raw_pipeline &&
				   !mtk_cam_scen_is_sensor_normal(
					   &raw_pipeline->user_res.raw_res.scen) &&
				   fmt->fmt.pix_mp.num_planes > 1) {
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
		for (i = 0; i < *num_planes; i++)
			dev_dbg(cam->dev, "[%s] id:%d, name:%s, np:%d, i:%d, size:%d\n", __func__,
				node->desc.id, node->desc.name, *num_planes, i, sizes[i]);
	}

	return 0;
}

static int mtk_cam_vb2_buf_init(struct vb2_buffer *vb)
{
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	struct device *dev = vb->vb2_queue->dev;
	struct mtk_cam_buffer *buf;
	dma_addr_t addr;
	unsigned int dma_port;

	buf = mtk_cam_vb2_buf_to_dev_buf(vb);

	/* note: flags should be reset here */
	/* buf->flags = 0; */
	buf->daddr = vb2_dma_contig_plane_dma_addr(vb, 0);
	buf->scp_addr = 0;

	/* SCP address is only valid for meta input buffer */
	if (!node->desc.smem_alloc)
		return 0;
	/* debug log use */
	dma_port = node->desc.dma_port;
	switch (dma_port) {
	case MTKCAM_IPI_MRAW_META_STATS_CFG:
	case MTKCAM_IPI_MRAW_META_STATS_0:
		dev_info(dev, "buf_length %d", vb->planes[0].length);
		break;
	default:
		break;
	}

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
	struct mtk_cam_buffer *mtk_buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	const struct v4l2_format *fmt = &node->active_fmt;
	unsigned int size;
	unsigned int plane;

	if (V4L2_TYPE_IS_OUTPUT(vb->type) &&
	    !(mtk_buf->flags & FLAG_NO_CACHE_CLEAN)) {
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
	struct mtk_cam_buffer *mtk_buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	unsigned int plane;

	if (V4L2_TYPE_IS_CAPTURE(vb->type) &&
	    !(mtk_buf->flags & FLAG_NO_CACHE_INVALIDATE)) {
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
	if (ctx->streaming_node_cnt == 1) {
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
	case V4L2_PIX_FMT_MTISP_PLANAR_BGGR_8:
	case V4L2_PIX_FMT_MTISP_PLANAR_GBRG_8:
	case V4L2_PIX_FMT_MTISP_PLANAR_GRBG_8:
	case V4L2_PIX_FMT_MTISP_PLANAR_RGGB_8:
	case V4L2_PIX_FMT_MTISP_PLANAR_BGGR_10:
	case V4L2_PIX_FMT_MTISP_PLANAR_GBRG_10:
	case V4L2_PIX_FMT_MTISP_PLANAR_GRBG_10:
	case V4L2_PIX_FMT_MTISP_PLANAR_RGGB_10:
	case V4L2_PIX_FMT_MTISP_PLANAR_BGGR_12:
	case V4L2_PIX_FMT_MTISP_PLANAR_GBRG_12:
	case V4L2_PIX_FMT_MTISP_PLANAR_GRBG_12:
	case V4L2_PIX_FMT_MTISP_PLANAR_RGGB_12:
	case V4L2_PIX_FMT_MTISP_PLANAR_BGGR_10P:
	case V4L2_PIX_FMT_MTISP_PLANAR_GBRG_10P:
	case V4L2_PIX_FMT_MTISP_PLANAR_GRBG_10P:
	case V4L2_PIX_FMT_MTISP_PLANAR_RGGB_10P:
	case V4L2_PIX_FMT_MTISP_PLANAR_BGGR_12P:
	case V4L2_PIX_FMT_MTISP_PLANAR_GBRG_12P:
	case V4L2_PIX_FMT_MTISP_PLANAR_GRBG_12P:
	case V4L2_PIX_FMT_MTISP_PLANAR_RGGB_12P:
	case V4L2_PIX_FMT_SBGGR16:
	case V4L2_PIX_FMT_SGBRG16:
	case V4L2_PIX_FMT_SGRBG16:
	case V4L2_PIX_FMT_SRGGB16:
	case V4L2_PIX_FMT_MTISP_SBGGR12:
	case V4L2_PIX_FMT_MTISP_SGBRG12:
	case V4L2_PIX_FMT_MTISP_SGRBG12:
	case V4L2_PIX_FMT_MTISP_SRGGB12:
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

int is_4_plane_rgb(u32 pixelformat)
{
	switch (pixelformat) {
	case V4L2_PIX_FMT_MTISP_PLANAR_BGGR_8:
	case V4L2_PIX_FMT_MTISP_PLANAR_GBRG_8:
	case V4L2_PIX_FMT_MTISP_PLANAR_GRBG_8:
	case V4L2_PIX_FMT_MTISP_PLANAR_RGGB_8:
	case V4L2_PIX_FMT_MTISP_PLANAR_BGGR_10:
	case V4L2_PIX_FMT_MTISP_PLANAR_GBRG_10:
	case V4L2_PIX_FMT_MTISP_PLANAR_GRBG_10:
	case V4L2_PIX_FMT_MTISP_PLANAR_RGGB_10:
	case V4L2_PIX_FMT_MTISP_PLANAR_BGGR_12:
	case V4L2_PIX_FMT_MTISP_PLANAR_GBRG_12:
	case V4L2_PIX_FMT_MTISP_PLANAR_GRBG_12:
	case V4L2_PIX_FMT_MTISP_PLANAR_RGGB_12:
	case V4L2_PIX_FMT_MTISP_PLANAR_BGGR_10P:
	case V4L2_PIX_FMT_MTISP_PLANAR_GBRG_10P:
	case V4L2_PIX_FMT_MTISP_PLANAR_GRBG_10P:
	case V4L2_PIX_FMT_MTISP_PLANAR_RGGB_10P:
	case V4L2_PIX_FMT_MTISP_PLANAR_BGGR_12P:
	case V4L2_PIX_FMT_MTISP_PLANAR_GBRG_12P:
	case V4L2_PIX_FMT_MTISP_PLANAR_GRBG_12P:
	case V4L2_PIX_FMT_MTISP_PLANAR_RGGB_12P:
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
		/* RGB 4P formats */
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_BGGR_8, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 1, .bit_r_den = 1, .pixel_id = 0 },
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_GBRG_8, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 1, .bit_r_den = 1, .pixel_id = 1 },
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_GRBG_8, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 1, .bit_r_den = 1, .pixel_id = 2 },
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_RGGB_8, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 1, .bit_r_den = 1, .pixel_id = 3 },
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_BGGR_10, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 2, .bit_r_den = 1, .pixel_id = 0 },
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_GBRG_10, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 2, .bit_r_den = 1, .pixel_id = 1 },
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_GRBG_10, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 2, .bit_r_den = 1, .pixel_id = 2 },
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_RGGB_10, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 2, .bit_r_den = 1, .pixel_id = 3 },
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_BGGR_12, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 2, .bit_r_den = 1, .pixel_id = 0 },
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_GBRG_12, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 2, .bit_r_den = 1, .pixel_id = 1 },
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_GRBG_12, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 2, .bit_r_den = 1, .pixel_id = 2 },
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_RGGB_12, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 2, .bit_r_den = 1, .pixel_id = 3 },
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_BGGR_10P, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 5, .bit_r_den = 4, .pixel_id = 0 },
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_GBRG_10P, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 5, .bit_r_den = 4, .pixel_id = 1 },
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_GRBG_10P, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 5, .bit_r_den = 4, .pixel_id = 2 },
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_RGGB_10P, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 5, .bit_r_den = 4, .pixel_id = 3 },
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_BGGR_12P, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 3, .bit_r_den = 2, .pixel_id = 0 },
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_GBRG_12P, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 3, .bit_r_den = 2, .pixel_id = 1 },
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_GRBG_12P, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 3, .bit_r_den = 2, .pixel_id = 2 },
		{ .format = V4L2_PIX_FMT_MTISP_PLANAR_RGGB_12P, .mem_planes = 1, .comp_planes = 4,
			.bpp = { 1, 1, 1, 1 }, .hdiv = 2, .vdiv = 2,
			.bit_r_num = 3, .bit_r_den = 2, .pixel_id = 3 },
		/* Bayer RGB format*/
		{ .format = V4L2_PIX_FMT_SBGGR16, .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bit_r_num = 1, .bit_r_den = 1, .pixel_id = 0 },
		{ .format = V4L2_PIX_FMT_SGBRG16, .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bit_r_num = 1, .bit_r_den = 1, .pixel_id = 1 },
		{ .format = V4L2_PIX_FMT_SGRBG16, .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bit_r_num = 1, .bit_r_den = 1, .pixel_id = 2 },
		{ .format = V4L2_PIX_FMT_SRGGB16, .mem_planes = 1, .comp_planes = 1,
			.bpp = { 2, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bit_r_num = 1, .bit_r_den = 1, .pixel_id = 3 },
		{ .format = V4L2_PIX_FMT_MTISP_SBGGR12, .mem_planes = 1, .comp_planes = 1,
			.bpp = { 1, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bit_r_num = 1, .bit_r_den = 1, .pixel_id = 0 },
		{ .format = V4L2_PIX_FMT_MTISP_SGBRG12, .mem_planes = 1, .comp_planes = 1,
			.bpp = { 1, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bit_r_num = 1, .bit_r_den = 1, .pixel_id = 1 },
		{ .format = V4L2_PIX_FMT_MTISP_SGRBG12, .mem_planes = 1, .comp_planes = 1,
			.bpp = { 1, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bit_r_num = 1, .bit_r_den = 1, .pixel_id = 2 },
		{ .format = V4L2_PIX_FMT_MTISP_SRGGB12, .mem_planes = 1, .comp_planes = 1,
			.bpp = { 1, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bit_r_num = 3, .bit_r_den = 2, .pixel_id = 3 },
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
	void *vaddr;
	struct mtkcam_ipi_frame_param *frame_param;
	struct mtkcam_ipi_meta_input *meta_in;
	struct mtkcam_ipi_meta_output *meta_out;
	struct mtk_raw_pipeline *raw_pipline;
	int pdo_max_sz;

	dma_port = node->desc.dma_port;
	pipe_id = node->uid.pipe_id;
	req_stream_data = mtk_cam_req_get_s_data(req, pipe_id, 0);
	frame_param = &req_stream_data->frame_params;
	raw_pipline = mtk_cam_dev_get_raw_pipeline(cam, pipe_id);
	if (raw_pipline) {
		mtk_cam_req_save_link_change(raw_pipline, req,
					     req_stream_data);
		mtk_cam_req_save_raw_vfmts(raw_pipline, req,
					   req_stream_data);
	}
	if (WARN_ON(mtk_cam_s_data_set_vbuf(req_stream_data, buf, node->desc.id))) {
		/* the buffer is invalid, mark done immediately */
		dev_info(dev, "%s:%s:pipe(%d): doubel enque %s\n",
			__func__, req->req.debug_str, pipe_id, node->desc.name);
		vb2_buffer_done(&buf->vbb.vb2_buf, VB2_BUF_STATE_ERROR);
		return;
	}
	req->enqeued_buf_cnt++;
	dev_dbg(dev, "%s:%s:pipe(%d):add buffer(%s), enqeued_buf_cnt(%d)\n",
		__func__, req->req.debug_str, pipe_id, node->desc.name,
		req->enqeued_buf_cnt);

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
		desc_id = node->desc.id-MTK_RAW_META_OUT_BEGIN;
		meta_out = &frame_param->meta_outputs[desc_id];
		meta_out->buf.ccd_fd = vb->planes[0].m.fd;
		meta_out->buf.size = node->active_fmt.fmt.meta.buffersize;
		meta_out->buf.iova = buf->daddr;
		meta_out->uid.id = dma_port;
		vaddr = vb2_plane_vaddr(vb, 0);
		pdo_max_sz = 0;
		if (raw_pipline) {
			pde_cfg = &raw_pipline->pde_config;
			if (pde_cfg->pde_info[CAM_SET_CTRL].pd_table_offset)
				pdo_max_sz = pde_cfg->pde_info[CAM_SET_CTRL].pdo_max_size;
		}
		CALL_PLAT_V4L2(set_meta_stats_info, dma_port, vaddr, pdo_max_sz,
			mtk_cam_scen_is_rgbw_enabled(req_stream_data->feature.scen));
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
	case MTKCAM_IPI_IMG_FMT_BAYER22:
		return 22;
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
	case MTKCAM_IPI_IMG_FMT_BAYER_8B_4P_BGGR:
	case MTKCAM_IPI_IMG_FMT_BAYER_8B_4P_GBRG:
	case MTKCAM_IPI_IMG_FMT_BAYER_8B_4P_GRBG:
	case MTKCAM_IPI_IMG_FMT_BAYER_8B_4P_RGGB:
		return 8;
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_BGGR:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_GBRG:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_GRBG:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_RGGB:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_BGGR:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_GBRG:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_GRBG:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_RGGB:
		return 16;
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_BGGR_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_GBRG_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_GRBG_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_RGGB_PACKED:
		return 10;
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_BGGR_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_GBRG_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_GRBG_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_RGGB_PACKED:
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
	case V4L2_PIX_FMT_MTISP_SBGGR22:
	case V4L2_PIX_FMT_MTISP_SGBRG22:
	case V4L2_PIX_FMT_MTISP_SGRBG22:
	case V4L2_PIX_FMT_MTISP_SRGGB22:
		return MTKCAM_IPI_IMG_FMT_BAYER22;
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
	case V4L2_PIX_FMT_MTISP_PLANAR_BGGR_8:
		return MTKCAM_IPI_IMG_FMT_BAYER_8B_4P_BGGR;
	case V4L2_PIX_FMT_MTISP_PLANAR_GBRG_8:
		return MTKCAM_IPI_IMG_FMT_BAYER_8B_4P_GBRG;
	case V4L2_PIX_FMT_MTISP_PLANAR_GRBG_8:
		return MTKCAM_IPI_IMG_FMT_BAYER_8B_4P_GRBG;
	case V4L2_PIX_FMT_MTISP_PLANAR_RGGB_8:
		return MTKCAM_IPI_IMG_FMT_BAYER_8B_4P_RGGB;
	case V4L2_PIX_FMT_MTISP_PLANAR_BGGR_10:
		return MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_BGGR;
	case V4L2_PIX_FMT_MTISP_PLANAR_GBRG_10:
		return MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_GBRG;
	case V4L2_PIX_FMT_MTISP_PLANAR_GRBG_10:
		return MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_GRBG;
	case V4L2_PIX_FMT_MTISP_PLANAR_RGGB_10:
		return MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_RGGB;
	case V4L2_PIX_FMT_MTISP_PLANAR_BGGR_12:
		return MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_BGGR;
	case V4L2_PIX_FMT_MTISP_PLANAR_GBRG_12:
		return MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_GBRG;
	case V4L2_PIX_FMT_MTISP_PLANAR_GRBG_12:
		return MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_GRBG;
	case V4L2_PIX_FMT_MTISP_PLANAR_RGGB_12:
		return MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_RGGB;
	case V4L2_PIX_FMT_MTISP_PLANAR_BGGR_10P:
		return MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_BGGR_PACKED;
	case V4L2_PIX_FMT_MTISP_PLANAR_GBRG_10P:
		return MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_GBRG_PACKED;
	case V4L2_PIX_FMT_MTISP_PLANAR_GRBG_10P:
		return MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_GRBG_PACKED;
	case V4L2_PIX_FMT_MTISP_PLANAR_RGGB_10P:
		return MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_RGGB_PACKED;
	case V4L2_PIX_FMT_MTISP_PLANAR_BGGR_12P:
		return MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_BGGR_PACKED;
	case V4L2_PIX_FMT_MTISP_PLANAR_GBRG_12P:
		return MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_GBRG_PACKED;
	case V4L2_PIX_FMT_MTISP_PLANAR_GRBG_12P:
		return MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_GRBG_PACKED;
	case V4L2_PIX_FMT_MTISP_PLANAR_RGGB_12P:
		return MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_RGGB_PACKED;
	default:
		pr_info("unknown fmt:0x%8x", fourcc);
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
	case MTKCAM_IPI_IMG_FMT_BAYER_8B_4P_BGGR:
	case MTKCAM_IPI_IMG_FMT_BAYER_8B_4P_GBRG:
	case MTKCAM_IPI_IMG_FMT_BAYER_8B_4P_GRBG:
	case MTKCAM_IPI_IMG_FMT_BAYER_8B_4P_RGGB:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_BGGR:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_GBRG:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_GRBG:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_RGGB:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_BGGR:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_GBRG:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_GRBG:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_RGGB:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_BGGR_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_GBRG_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_GRBG_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_RGGB_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_BGGR_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_GBRG_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_GRBG_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_RGGB_PACKED:
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
		if (!info)
			return -EINVAL;
		pixfmt->num_planes = info->mem_planes;

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
			} else if (is_4_plane_rgb(pixelformat)) {
				/* width should be bus_size align */
				aligned_width = ALIGN(DIV_ROUND_UP(width / 2
					* info->bit_r_num, info->bit_r_den), bus_size);
				stride = aligned_width * info->bpp[0];

				if (stride > plane->bytesperline)
					plane->bytesperline = stride;

				plane->sizeimage = plane->bytesperline * height / 2 * 4;
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
	case MTKCAM_IPI_IMG_FMT_BAYER22:
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
	case MTKCAM_IPI_IMG_FMT_BAYER_8B_4P_BGGR:
	case MTKCAM_IPI_IMG_FMT_BAYER_8B_4P_GBRG:
	case MTKCAM_IPI_IMG_FMT_BAYER_8B_4P_GRBG:
	case MTKCAM_IPI_IMG_FMT_BAYER_8B_4P_RGGB:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_BGGR:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_GBRG:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_GRBG:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_RGGB:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_BGGR:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_GBRG:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_GRBG:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_RGGB:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_BGGR_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_GBRG_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_GRBG_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_10B_4P_RGGB_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_BGGR_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_GBRG_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_GRBG_PACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER_12B_4P_RGGB_PACKED:
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
			/* camsv todo: should have a better implementation here */
			q->dev = cam->sv.devs[0];
		} else if (video->uid.pipe_id >= MTKCAM_SUBDEV_MRAW_START &&
			video->uid.pipe_id < MTKCAM_SUBDEV_MRAW_END) {
			q->dev =
				cam->mraw.devs[video->uid.pipe_id - MTKCAM_SUBDEV_MRAW_START];
		} else {
			switch (video->desc.id) {
			case MTK_RAW_YUVO_1_OUT:
			case MTK_RAW_YUVO_2_OUT:
			case MTK_RAW_YUVO_3_OUT:
			case MTK_RAW_YUVO_4_OUT:
			case MTK_RAW_YUVO_5_OUT:
			case MTK_RAW_DRZS4NO_1_OUT:
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
	strscpy(vdev->name, video->desc.name, sizeof(vdev->name));

	ret = video_register_device(vdev, VFL_TYPE_VIDEO, CAMSYS_VIDEO_DEV_NR);
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
		if (desc->buf_type == V4L2_BUF_TYPE_META_OUTPUT ||
			desc->buf_type == V4L2_BUF_TYPE_META_CAPTURE) {
			if (fmt->fmt.meta.dataformat == format)
				return fmt;
		} else if (fmt->fmt.pix_mp.pixelformat == format)
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
	snprintf_safe(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
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

static void fill_ext_fmtdesc(struct v4l2_fmtdesc *fmt)
{
	const char *descr = NULL;
	const unsigned int sz = sizeof(fmt->description);

	switch (fmt->pixelformat) {
	case V4L2_PIX_FMT_YUYV10:
		descr = "YUYV 4:2:2 10 bits";
		break;
	case V4L2_PIX_FMT_YVYU10:
		descr = "YVYU 4:2:2 10 bits";
		break;
	case V4L2_PIX_FMT_UYVY10:
		descr = "UYVY 4:2:2 10 bits";
		break;
	case V4L2_PIX_FMT_VYUY10:
		descr = "VYUY 4:2:2 10 bits";
		break;
	case V4L2_PIX_FMT_NV12_10:
		descr = "Y/CbCr 4:2:0 10 bits";
		break;
	case V4L2_PIX_FMT_NV21_10:
		descr = "Y/CrCb 4:2:0 10 bits";
		break;
	case V4L2_PIX_FMT_NV16_10:
		descr = "Y/CbCr 4:2:2 10 bits";
		break;
	case V4L2_PIX_FMT_NV61_10:
		descr = "Y/CrCb 4:2:2 10 bits";
		break;
	case V4L2_PIX_FMT_NV12_12:
		descr = "Y/CbCr 4:2:0 12 bits";
		break;
	case V4L2_PIX_FMT_NV21_12:
		descr = "Y/CrCb 4:2:0 12 bits";
		break;
	case V4L2_PIX_FMT_NV16_12:
		descr = "Y/CbCr 4:2:2 12 bits";
		break;
	case V4L2_PIX_FMT_NV61_12:
		descr = "Y/CrCb 4:2:2 12 bits";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR10:
		descr = "10-bit Bayer BGGR MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG10:
		descr = "10-bit Bayer GBRG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG10:
		descr = "10-bit Bayer GRBG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB10:
		descr = "10-bit Bayer RGGB MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR12:
		descr = "12-bit Bayer BGGR MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG12:
		descr = "12-bit Bayer GBRG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG12:
		descr = "12-bit Bayer GRBG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB12:
		descr = "12-bit Bayer RGGB MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR14:
		descr = "14-bit Bayer BGGR MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG14:
		descr = "14-bit Bayer GBRG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG14:
		descr = "14-bit Bayer GRBG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB14:
		descr = "14-bit Bayer RGGB MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR22:
		descr = "22-bit Bayer BGGR MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG22:
		descr = "22-bit Bayer GBRG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG22:
		descr = "22-bit Bayer GRBG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB22:
		descr = "22-bit Bayer RGGB MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR8F:
		descr = "8-bit Enhanced BGGR Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG8F:
		descr = "8-bit Enhanced GBRG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG8F:
		descr = "8-bit Enhanced GRBG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB8F:
		descr = "8-bit Enhanced RGGB Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR10F:
		descr = "10-bit Enhanced BGGR Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG10F:
		descr = "10-bit Enhanced GBRG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG10F:
		descr = "10-bit Enhanced GRBG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB10F:
		descr = "10-bit Enhanced RGGB Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR12F:
		descr = "12-bit Enhanced BGGR Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG12F:
		descr = "12-bit Enhanced GBRG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG12F:
		descr = "12-bit Enhanced GRBG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB12F:
		descr = "12-bit Enhanced RGGB Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR14F:
		descr = "14-bit Enhanced BGGR Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG14F:
		descr = "14-bit Enhanced GBRG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG14F:
		descr = "14-bit Enhanced GRBG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB14F:
		descr = "14-bit Enhanced RGGB Packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV12_10P:
		descr = "Y/CbCr 4:2:0 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV21_10P:
		descr = "Y/CrCb 4:2:0 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV16_10P:
		descr = "Y/CbCr 4:2:2 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV61_10P:
		descr = "Y/CrCb 4:2:2 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_YUYV10P:
		descr = "YUYV 4:2:2 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_YVYU10P:
		descr = "YVYU 4:2:2 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_UYVY10P:
		descr = "UYVY 4:2:2 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_VYUY10P:
		descr = "VYUY 4:2:2 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV12_12P:
		descr = "Y/CbCr 4:2:0 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV21_12P:
		descr = "Y/CrCb 4:2:0 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV16_12P:
		descr = "Y/CbCr 4:2:2 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV61_12P:
		descr = "Y/CrCb 4:2:2 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_YUYV12P:
		descr = "YUYV 4:2:2 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_YVYU12P:
		descr = "YVYU 4:2:2 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_UYVY12P:
		descr = "UYVY 4:2:2 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_VYUY12P:
		descr = "VYUY 4:2:2 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV12_UFBC:
		descr = "YCbCr 420 8 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_NV21_UFBC:
		descr = "YCrCb 420 8 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_NV12_10_UFBC:
		descr = "YCbCr 420 10 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_NV21_10_UFBC:
		descr = "YCrCb 420 10 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_NV12_12_UFBC:
		descr = "YCbCr 420 12 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_NV21_12_UFBC:
		descr = "YCrCb 420 12 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_BAYER8_UFBC:
		descr = "RAW 8 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_BAYER10_UFBC:
		descr = "RAW 10 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_BAYER12_UFBC:
		descr = "RAW 12 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_BAYER14_UFBC:
		descr = "RAW 14 bits compress";
		break;
	case V4L2_META_FMT_MTISP_3A:
		descr = "AE/AWB Histogram";
		break;
	case V4L2_META_FMT_MTISP_AF:
		descr = "AF Histogram";
		break;
	case V4L2_META_FMT_MTISP_LCS:
		descr = "Local Contrast Enhancement Stat";
		break;
	case V4L2_META_FMT_MTISP_LMV:
		descr = "Local Motion Vector Histogram";
		break;
	case V4L2_META_FMT_MTISP_PARAMS:
		descr = "MTK ISP Tuning Metadata";
		break;
	case V4L2_PIX_FMT_MTISP_SGRB8F:
		descr = "8-bit 3 plane GRB Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRB10F:
		descr = "10-bit 3 plane GRB Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRB12F:
		descr = "12-bit 3 plane GRB Packed";
		break;
	default:
		descr = "unknown mtk ext fmt";
		break;
	}

	if (descr)
		WARN_ON(strscpy(fmt->description, descr, sz) < 0);
}

int mtk_cam_vidioc_enum_fmt(struct file *file, void *fh,
			    struct v4l2_fmtdesc *f)
{
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);

	if (f->index >= node->desc.num_fmts)
		return -EINVAL;

	f->pixelformat = node->desc.fmts[f->index].vfmt.fmt.pix_mp.pixelformat;
	f->flags = 0;
	fill_ext_fmtdesc(f);

	return 0;
}

int mtk_cam_vidioc_g_fmt(struct file *file, void *fh,
			 struct v4l2_format *f)
{
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);

	f->fmt = node->active_fmt.fmt;

	return 0;
}

int mtk_cam_collect_vfmt(struct mtk_raw_pipeline *pipe,
			 struct mtk_cam_video_device *node,
			 struct v4l2_format *f)
{
	pipe->req_vfmt_update |= (1 << node->desc.id);
	node->pending_fmt = *f;

	return 0;
}

int mtk_cam_collect_sv_vfmt(struct mtk_camsv_pipeline *pipe,
			 struct mtk_cam_video_device *node,
			 struct v4l2_format *f)
{
	pipe->req_vfmt_update |= (1 << node->desc.id);
	node->pending_fmt = *f;

	return 0;
}

/* check the setting from user with scen and log the error for integration test */
int mtk_cam_video_s_fmt_chk_feature(struct mtk_cam_video_device *node,
				    struct v4l2_format *f,
				    struct mtk_cam_scen *scen,
				    char *dbg_str)
{
	struct mtk_cam_device *cam = video_get_drvdata(&node->vdev);
	struct v4l2_format try_fmt;
	u32 bytesperline, sizeimage;
	bool is_hdr = false, is_hdr_m2m = false;
	int stride, num_planes;
	int img_fmt;
	int i;

	dev_dbg(cam->dev,
		"%s:pipe(%d):%s: scen(%s)\n",
		__func__, node->uid.pipe_id, node->desc.name, scen->dbg_str);

	try_fmt = *f;

	if (mtk_cam_scen_is_hdr(scen)) {
		if (mtk_cam_scen_is_odt(scen))
			is_hdr_m2m = true;
		else
			is_hdr = true;
	}

	/**
	 * check HDR num_planes setting from user
	 * - support 1/2/3 plane
	 * - vhdr m2m main stream is implicitly multiple plane
	 */
	if ((node->desc.id == MTK_RAW_MAIN_STREAM_OUT && is_hdr) ||
	    (node->desc.id == MTK_RAW_RAWI_2_IN && is_hdr_m2m)) {
		num_planes = mtk_cam_scen_get_max_exp_num(scen);

		if (mtk_cam_scen_is_rgbw_enabled(scen))
			num_planes *= 2;

		/**
		 * TODO: remove the workaorund, we can only use fmt[0] in single
		 * fd case
		 */
		if (try_fmt.fmt.pix_mp.num_planes < num_planes) {
			bytesperline = try_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
			sizeimage = try_fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
			try_fmt.fmt.pix_mp.num_planes = num_planes;
			/* correct the plane_fmt settings */
			for (i = 0 ; i < try_fmt.fmt.pix_mp.num_planes ; i++) {
				try_fmt.fmt.pix_mp.plane_fmt[i].bytesperline =
					bytesperline;
				try_fmt.fmt.pix_mp.plane_fmt[i].sizeimage =
					sizeimage;
			}
		}
	}

	/**
	 * check subsample num_planes setting from user
	 * - support 1/2/3 plane
	 * - vhdr m2m main stream is implicitly multiple plane
	 */
	if (node->desc.id >= MTK_RAW_MAIN_STREAM_OUT &&
	    node->desc.id < MTK_RAW_RZH1N2TO_1_OUT &&
	    mtk_cam_scen_is_subsample(scen)) {
		if (scen->scen.smvr.subsample_num > MAX_SUBSAMPLE_PLANE_NUM)
			num_planes = MAX_SUBSAMPLE_PLANE_NUM;
		else
			num_planes = scen->scen.smvr.subsample_num;
		/**
		 * TODO: remove the workaorund, we can only use fmt[0] in single
		 * fd case
		 */
		if (try_fmt.fmt.pix_mp.num_planes < num_planes) {
			bytesperline = try_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
			sizeimage = try_fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
			for (i = 0 ; i < try_fmt.fmt.pix_mp.num_planes ; i++) {
				try_fmt.fmt.pix_mp.plane_fmt[i].bytesperline =
					bytesperline;
				try_fmt.fmt.pix_mp.plane_fmt[i].sizeimage =
					sizeimage;
			}

			dev_dbg(cam->dev,
				 "%s:%s:pipe(%d):%s:scen(%s):stride:%d size:%d\n",
				 __func__, dbg_str, node->uid.pipe_id,
				 node->desc.name, scen->dbg_str,
				 bytesperline, sizeimage);
		}
	}

	/**
	 * check extisp settings
	 * extisp may use mainstream for yuvformat
	 */
	if (node->desc.id == MTK_RAW_MAIN_STREAM_OUT &&
	    mtk_cam_scen_is_ext_isp(scen) &&
	    scen->scen.extisp.type == MTK_CAM_EXTISP_CUS_2) {
		img_fmt = mtk_cam_get_img_fmt(try_fmt.fmt.pix_mp.pixelformat);
		stride = mtk_cam_dmao_xsize(try_fmt.fmt.pix_mp.width,
					    img_fmt, 0);
		if (try_fmt.fmt.pix_mp.plane_fmt[0].bytesperline < stride) {
			dev_info(cam->dev,
				 " %s:%s:pipe(%d):%s:scen(%s):invalid stride(%d), should be %d\n",
				 __func__, dbg_str, node->uid.pipe_id,
				 node->desc.name, scen->dbg_str,
				 try_fmt.fmt.pix_mp.plane_fmt[0].bytesperline, stride);
			try_fmt.fmt.pix_mp.plane_fmt[0].bytesperline =
				stride * 2;
		}

		sizeimage = stride * try_fmt.fmt.pix_mp.height;
		if (try_fmt.fmt.pix_mp.plane_fmt[0].sizeimage < sizeimage) {
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):%s:scen(%s):invalid stride(%d), should be %d\n",
				 __func__, dbg_str, node->uid.pipe_id,
				 node->desc.name, scen->dbg_str,
				 try_fmt.fmt.pix_mp.plane_fmt[0].sizeimage, sizeimage);
			try_fmt.fmt.pix_mp.plane_fmt[0].sizeimage = sizeimage;
		}
	}

	/**
	 * check rgbw
	 */
	if (node->desc.id == MTK_RAW_MAIN_STREAM_OUT &&
	    mtk_cam_scen_is_rgbw_enabled(scen)) {
		num_planes = scen->scen.normal.exp_num * 2; // bayer + w

		if (try_fmt.fmt.pix_mp.num_planes < num_planes) {
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):%s:scen(%s):invalid num_planes(%d), should be %d\n",
				 __func__, "rgbw", node->uid.pipe_id,
				 node->desc.name, scen->dbg_str,
				 try_fmt.fmt.pix_mp.num_planes, num_planes);

			try_fmt.fmt.pix_mp.num_planes = num_planes;
			bytesperline = try_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
			sizeimage = try_fmt.fmt.pix_mp.plane_fmt[0].sizeimage;

			for (i = 0 ; i < try_fmt.fmt.pix_mp.num_planes ; i++) {
				try_fmt.fmt.pix_mp.plane_fmt[i].bytesperline =
					bytesperline;
				try_fmt.fmt.pix_mp.plane_fmt[i].sizeimage =
					sizeimage;
			}
		}
	}

	/* re-config rawi for dc mode */
	if (node->desc.id == MTK_RAW_MAIN_STREAM_OUT) {
		struct v4l2_format *img_fmt;
		unsigned int sink_ipi_fmt;
		struct mtk_raw_pipeline *pipe;

		if (is_raw_subdev(node->uid.pipe_id)) {
			pipe = mtk_cam_dev_get_raw_pipeline(cam, node->uid.pipe_id);
			if (pipe && mtk_cam_hw_mode_is_dc(pipe->res_config.hw_mode)) {
				img_fmt = &pipe->img_fmt_sink_pad;
				img_fmt->fmt.pix_mp.width = try_fmt.fmt.pix_mp.width;
				img_fmt->fmt.pix_mp.height = try_fmt.fmt.pix_mp.height;
				img_fmt->fmt.pix_mp.pixelformat = try_fmt.fmt.pix_mp.pixelformat;
				sink_ipi_fmt = mtk_cam_get_img_fmt(try_fmt.fmt.pix_mp.pixelformat);
				img_fmt->fmt.pix_mp.plane_fmt[0].bytesperline =
					mtk_cam_dmao_xsize(
						try_fmt.fmt.pix_mp.width, sink_ipi_fmt, 3);
				img_fmt->fmt.pix_mp.plane_fmt[0].sizeimage =
						img_fmt->fmt.pix_mp.plane_fmt[0].bytesperline *
						img_fmt->fmt.pix_mp.height;
				dev_info(cam->dev,
					"%s id:%d hwmode:0x%x ipi_fmt:%d pixelformat:0x%x\n",
					__func__, node->desc.id,
					pipe->res_config.hw_mode, sink_ipi_fmt,
					img_fmt->fmt.pix_mp.pixelformat);
			}
		}
	}

	*f = try_fmt;

	return 0;
}

/* the raw_feature or scen will be removed in try fmt case (vendor hook removal) */
int mtk_cam_video_s_fmt_common(struct mtk_cam_video_device *node,
			       struct v4l2_format *f, char *dbg_str)
{
	struct mtk_cam_device *cam = video_get_drvdata(&node->vdev);
	const struct v4l2_format *dev_fmt;
	struct v4l2_format try_fmt;
	s32 i;
	u32 bytesperline, sizeimage;
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

	/**
	 * The user must provide the num_planes information for special scenario which
	 * may carry two or more images.
	 *
	 * Note: vhdr m2m main stream is implicitly multiple plane
	 * but not negotiatied through try format
	 */
	if (try_fmt.fmt.pix_mp.num_planes <= 0)
		try_fmt.fmt.pix_mp.num_planes = 1;

	if (try_fmt.fmt.pix_mp.num_planes > MAX_SUBSAMPLE_PLANE_NUM) {
		dev_info_ratelimited(cam->dev, "%s:%s:pipe(%d):%s:invalid num_planes(%d)\n",
			 __func__, dbg_str, node->uid.pipe_id, node->desc.name,
			 try_fmt.fmt.pix_mp.num_planes);
		try_fmt.fmt.pix_mp.num_planes = MAX_SUBSAMPLE_PLANE_NUM;
	}

	for (i = 0 ; i < try_fmt.fmt.pix_mp.num_planes ; i++)
		try_fmt.fmt.pix_mp.plane_fmt[i].bytesperline =
				f->fmt.pix_mp.plane_fmt[i].bytesperline;

	/* bytesperline & sizeimage calculation */
	if (node->desc.dma_port == MTKCAM_IPI_CAMSV_MAIN_OUT ||
	    node->desc.dma_port == MTKCAM_IPI_RAW_IMGO ||
	    node->desc.dma_port == MTKCAM_IPI_RAW_RAWI_2)
		cal_image_pix_mp(node->desc.id, &try_fmt.fmt.pix_mp, 3);
	else
		cal_image_pix_mp(node->desc.id, &try_fmt.fmt.pix_mp, 0);

	if (node->desc.id >= MTK_RAW_MAIN_STREAM_OUT &&
	    node->desc.id < MTK_RAW_RZH1N2TO_1_OUT) {
		bytesperline = try_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
		sizeimage = try_fmt.fmt.pix_mp.plane_fmt[0].sizeimage;

		/**
		 * Considering subsample case or any multiple image case,
		 * the user uses num_planes to indicate the number of image
		 */
		for (i = 0 ; i < try_fmt.fmt.pix_mp.num_planes ; i++) {
			try_fmt.fmt.pix_mp.plane_fmt[i].bytesperline =
				bytesperline;
			try_fmt.fmt.pix_mp.plane_fmt[i].sizeimage =
				sizeimage;
		}

		dev_info_ratelimited(cam->dev,
			 "%s:%s:pipe(%d):%s:stride:%d, size:%d, num_planes(%d)\n",
			 __func__, dbg_str, node->uid.pipe_id, node->desc.name,
			 bytesperline, sizeimage,
			 try_fmt.fmt.pix_mp.num_planes);
	}

	/* Constant format fields */
	try_fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_SRGB;
	try_fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
	try_fmt.fmt.pix_mp.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	try_fmt.fmt.pix_mp.quantization = V4L2_QUANTIZATION_DEFAULT;
	try_fmt.fmt.pix_mp.xfer_func = V4L2_XFER_FUNC_SRGB;

	*f = try_fmt;

	return 0;
}

/* the raw_feature or scen will be removed in try fmt case (vendor hook removal) */
int mtk_cam_video_set_fmt(struct mtk_cam_video_device *node, struct v4l2_format *f,
			  struct mtk_cam_scen *scen, char *dbg_str)
{
	int ret;

	ret = mtk_cam_video_s_fmt_common(node, f, dbg_str);
	ret = mtk_cam_video_s_fmt_chk_feature(node, f, scen, dbg_str);

	return ret;
}

int mtk_cam_vidioc_s_fmt(struct file *file, void *fh, struct v4l2_format *f)
{
	struct mtk_cam_device *cam = video_drvdata(file);
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);
	struct mtk_raw_pipeline *raw_pipeline;
	struct mtk_camsv_pipeline *sv_pipeline;
	struct mtk_cam_scen scen;

	raw_pipeline = mtk_cam_dev_get_raw_pipeline(cam, node->uid.pipe_id);
	sv_pipeline = mtk_cam_dev_get_sv_pipeline(cam, node->uid.pipe_id);

	if (!vb2_is_busy(node->vdev.queue)) {
		mtk_cam_scen_init(&scen);

		/* Get the valid format */
		if (raw_pipeline)
			scen = raw_pipeline->user_res.raw_res.scen;

		mtk_cam_video_set_fmt(node, f, &scen, "s_fmt");

		if (is_camsv_subdev(node->uid.pipe_id))
			mtk_cam_sv_update_feature(node);

		/* Configure to video device */
		node->active_fmt = *f;

		return 0;
	}

	if (raw_pipeline)
		mtk_cam_collect_vfmt(raw_pipeline, node, f);
	if (sv_pipeline)
		mtk_cam_collect_sv_vfmt(sv_pipeline, node, f);

	dev_dbg(cam->dev,
		"%s:pipe(%d):%s:pending s_fmt: pixelfmt(0x%x), w(%d), h(%d)\n",
		__func__, node->uid.pipe_id, node->desc.name,
		f->fmt.pix_mp.pixelformat,  f->fmt.pix_mp.width, f->fmt.pix_mp.height);

	return 0;
}

int mtk_cam_vidioc_try_fmt(struct file *file, void *fh,
			   struct v4l2_format *f)
{
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);
	int ret;

	ret = mtk_cam_video_s_fmt_common(node, f, "try_fmt");

	if (is_camsv_subdev(node->uid.pipe_id))
		ret |= mtk_cam_sv_update_image_size(node, f);

	return ret;
}

int mtk_cam_vidioc_meta_enum_fmt(struct file *file, void *fh,
				 struct v4l2_fmtdesc *f)
{
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);

	if (f->index)
		return -EINVAL;

	f->pixelformat = node->active_fmt.fmt.meta.dataformat;
	f->flags = 0;
	fill_ext_fmtdesc(f);

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
	u32 extmeta_size = 0;

	switch (node->desc.id) {
	case MTK_RAW_META_SV_OUT_0:
	case MTK_RAW_META_SV_OUT_1:
	case MTK_RAW_META_SV_OUT_2:
		if (node->enabled && node->ctx)
			extmeta_size = cam->raw.pipelines[node->uid.pipe_id]
				.cfg[node->desc.id].mbus_fmt.width *
				cam->raw.pipelines[node->uid.pipe_id]
				.cfg[node->desc.id].mbus_fmt.height;
		if (extmeta_size) {
			f->fmt.meta.buffersize = extmeta_size;
			f->fmt.meta.dataformat = default_fmt->fmt.meta.dataformat;
		} else {
			f->fmt.meta.buffersize =
				CAMSV_EXT_META_0_WIDTH * CAMSV_EXT_META_0_HEIGHT;
			f->fmt.meta.dataformat = default_fmt->fmt.meta.dataformat;
		}
		/* fake for backend compose */
		node->active_fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SBGGR8;
		node->active_fmt.fmt.pix_mp.width = cam->raw
			.pipelines[node->uid.pipe_id].cfg[node->desc.id].mbus_fmt.width;
		node->active_fmt.fmt.pix_mp.height = cam->raw
			.pipelines[node->uid.pipe_id].cfg[node->desc.id].mbus_fmt.height;
		node->active_fmt.fmt.pix_mp.num_planes = 1;
		cal_image_pix_mp(node->desc.id, &node->active_fmt.fmt.pix_mp, 3);
		dev_dbg(cam->dev,
			"%s:extmeta name:%s buffersize:%d, fmt:0x%x, w/h/byteline:%d/%d/%d\n",
			__func__, node->desc.name, node->active_fmt.fmt.meta.buffersize,
			node->active_fmt.fmt.pix_mp.pixelformat,
			node->active_fmt.fmt.pix_mp.width,
			node->active_fmt.fmt.pix_mp.height,
			node->active_fmt.fmt.pix_mp.plane_fmt[0].bytesperline);

		return 0;
	default:
		break;
	}

	f->fmt.meta.dataformat = node->active_fmt.fmt.meta.dataformat;
	f->fmt.meta.buffersize = node->active_fmt.fmt.meta.buffersize;
	dev_dbg(cam->dev,
		"%s: node:%d dataformat:%d buffersize:%d\n",
		__func__, node->desc.id, f->fmt.meta.dataformat, f->fmt.meta.buffersize);

	return 0;
}

int mtk_cam_vidioc_try_meta_fmt(struct file *file, void *fh,
			      struct v4l2_format *f)
{
	struct mtk_cam_device *cam = video_drvdata(file);
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);
	const struct v4l2_format *fmt;

	switch (node->desc.dma_port) {
	case MTKCAM_IPI_RAW_META_STATS_CFG:
	case MTKCAM_IPI_RAW_META_STATS_0:
	case MTKCAM_IPI_RAW_META_STATS_1:
		fmt = mtk_cam_dev_find_fmt(&node->desc, f->fmt.meta.dataformat);
		if (fmt) {
			f->fmt.meta.dataformat = fmt->fmt.meta.dataformat;
			f->fmt.meta.buffersize = fmt->fmt.meta.buffersize;
			dev_dbg(cam->dev,
				"%s: node:%d port:%d dataformat:%c%c%c%c buffersize:%d\n",
					__func__, node->desc.id, node->desc.dma_port,
					((char *)&f->fmt.meta.dataformat)[0],
					((char *)&f->fmt.meta.dataformat)[1],
					((char *)&f->fmt.meta.dataformat)[2],
					((char *)&f->fmt.meta.dataformat)[3],
					f->fmt.meta.buffersize);
		} else
			dev_info(cam->dev, "%s: unknown meta format(%c%c%c%c, size %d) for port(%d)",
					__func__,
					((char *)&f->fmt.meta.dataformat)[0],
					((char *)&f->fmt.meta.dataformat)[1],
					((char *)&f->fmt.meta.dataformat)[2],
					((char *)&f->fmt.meta.dataformat)[3],
					f->fmt.meta.buffersize,
					node->desc.dma_port);
		return (fmt) ? 0 : -EINVAL;
	default:
		break;
	}

	return mtk_cam_vidioc_g_meta_fmt(file, fh, f);
}

int mtk_cam_vidioc_s_meta_fmt(struct file *file, void *fh,
			      struct v4l2_format *f)
{
	int ret = 0;
	struct mtk_cam_device *cam = video_drvdata(file);
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);
	struct mtk_raw_pipeline *raw_pipeline;
	const struct v4l2_format *fmt;

	switch (node->desc.dma_port) {
	case MTKCAM_IPI_RAW_META_STATS_CFG:
	case MTKCAM_IPI_RAW_META_STATS_0:
	case MTKCAM_IPI_RAW_META_STATS_1:
		fmt = mtk_cam_dev_find_fmt(&node->desc, f->fmt.meta.dataformat);

		if (fmt) {
			raw_pipeline =
				mtk_cam_dev_get_raw_pipeline(cam, node->uid.pipe_id);
			node->active_fmt.fmt.meta.dataformat = fmt->fmt.meta.dataformat;
			node->active_fmt.fmt.meta.buffersize = f->fmt.meta.buffersize;
		} else {
			dev_info(cam->dev, "%s: unknown meta format(%d, size %d) for port(%d)",
					__func__,
					f->fmt.meta.dataformat,
					f->fmt.meta.buffersize,
					node->desc.dma_port);
			ret = -EINVAL;
		}
		break;
	default:
		break;
	}

	if (!ret && node->desc.dma_port == MTKCAM_IPI_RAW_META_STATS_CFG) {
		ret = mtk_cam_update_pd_meta_cfg_info(raw_pipeline, CAM_SET_CTRL);
		if (ret)
			dev_info(cam->dev, "%s: mtk_cam_update_pd_info fail %d",
				__func__, ret);
	} else if (!ret && node->desc.dma_port == MTKCAM_IPI_RAW_META_STATS_0) {
		ret = mtk_cam_update_pd_meta_out_info(raw_pipeline, CAM_SET_CTRL);
		if (ret)
			dev_info(cam->dev, "%s: mtk_cam_update_pd_info fail %d",
				__func__, ret);
	}

	return ((ret) ? ret : mtk_cam_vidioc_g_meta_fmt(file, fh, f));
}

int mtk_cam_collect_vsel(struct mtk_raw_pipeline *pipe,
			 struct mtk_cam_video_device *node,
				struct v4l2_selection *s)
{
	pipe->req_vsel_update |= (1 << node->desc.id); /* debug only*/
	node->pending_crop = *s;

	dev_dbg(pipe->subdev.v4l2_dev->dev,
		"%s:%s:%s:pending vidioc_s_selection (%d,%d,%d,%d)\n",
		__func__, pipe->subdev.name, node->desc.name,
		s->r.left, s->r.top, s->r.width, s->r.height);

			return 0;
}

int mtk_cam_vidioc_s_selection(struct file *file, void *fh,
			       struct v4l2_selection *s)
{
	struct mtk_cam_device *cam = video_drvdata(file);
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);
	struct mtk_raw_pipeline *raw_pipeline;

	raw_pipeline = mtk_cam_dev_get_raw_pipeline(cam, node->uid.pipe_id);
	if (raw_pipeline)
		mtk_cam_collect_vsel(raw_pipeline, node, s);

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

static struct mtk_cam_buffer *
mtk_cam_vb2_queue_get_mtkbuf(struct vb2_queue *q, struct v4l2_buffer *b)
{
	struct vb2_buffer *vb;

	if (b->index >= q->num_buffers) {
		dev_info(q->dev, "%s: buffer index out of range (idx/num: %d/%d)\n",
			 __func__, b->index, q->num_buffers);
		return NULL;
	}

	vb = q->bufs[b->index];
	if (vb == NULL) {
		/* Should never happen */
		dev_info(q->dev, "%s: buffer is NULL\n", __func__);
		return NULL;
	}

	return mtk_cam_vb2_buf_to_dev_buf(vb);
}

int mtk_cam_vidioc_qbuf(struct file *file, void *priv,
			struct v4l2_buffer *buf)
{
	struct video_device *vdev = video_devdata(file);
	struct mtk_cam_buffer *cam_buf;

	cam_buf = mtk_cam_vb2_queue_get_mtkbuf(vdev->queue, buf);
	if (cam_buf == NULL)
		return -EINVAL;

	cam_buf->flags = 0;
	if (buf->flags & V4L2_BUF_FLAG_NO_CACHE_CLEAN)
		cam_buf->flags |= FLAG_NO_CACHE_CLEAN;

	if (buf->flags & V4L2_BUF_FLAG_NO_CACHE_INVALIDATE)
		cam_buf->flags |= FLAG_NO_CACHE_INVALIDATE;

	return vb2_qbuf(vdev->queue, vdev->v4l2_dev->mdev, buf);
}
