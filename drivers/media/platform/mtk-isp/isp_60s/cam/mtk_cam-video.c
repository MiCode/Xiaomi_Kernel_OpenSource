// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>

#include "mtk_cam.h"
#include "mtk_cam-video.h"
#include "mtk_cam-meta.h"

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

	/* Check the limitation of buffer size */
	if (max_buffer_count)
		*num_buffers = clamp_val(*num_buffers, 1, max_buffer_count);

	if (node->desc.smem_alloc)
		vq->dma_attrs |= DMA_ATTR_NO_KERNEL_MAPPING;
	else
		vq->dma_attrs |= DMA_ATTR_NON_CONSISTENT;

	if (vq->type == V4L2_BUF_TYPE_META_OUTPUT ||
	    vq->type == V4L2_BUF_TYPE_META_CAPTURE)
		size = fmt->fmt.meta.buffersize;
	else
		size = fmt->fmt.pix_mp.plane_fmt[0].sizeimage;

	/* Add for q.create_bufs with fmt.g_sizeimage(p) / 2 test */
	if (*num_planes) {
		if (sizes[0] < size || *num_planes != 1)
			return -EINVAL;
	} else {
		*num_planes = 1;
		sizes[0] = size;
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
		dev_dbg(dev, "failed to map meta addr:%pad\n", &buf->daddr);
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
		dev_dbg(vb->vb2_queue->dev, "plane size is too small:%lu<%u\n",
			vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}

	if (V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		if (vb2_get_plane_payload(vb, 0) != size) {
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
		dev_dbg(cam->dev,
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

	ctx->streaming_node_cnt++;

	if (ctx->streaming_node_cnt == 1)
		isp_composer_create_session(cam, ctx);

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
	struct mtk_cam_uapi_meta_raw_stats_2 *stats2;
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
		set_payload(&stats0->tsf_stats.tsfo_buf, MTK_CAM_UAPI_TSFSO_SIZE, &offset);
		break;
	case MTKCAM_IPI_RAW_META_STATS_1:
		stats1 = (struct mtk_cam_uapi_meta_raw_stats_1 *)vaddr;
		offset = sizeof(*stats1);
		set_payload(&stats1->af_stats.afo_buf, MTK_CAM_UAPI_AFO_MAX_BUF_SIZE, &offset);
		break;
	case MTKCAM_IPI_RAW_META_STATS_2:
		stats2 = (struct mtk_cam_uapi_meta_raw_stats_2 *)vaddr;
		offset = sizeof(*stats2);
		set_payload(&stats2->lce_stats.lceso_buf, MTK_CAM_UAPI_LCESO_SIZE, &offset);
		set_payload(&stats2->lceh_stats.lcesho_buf, MTK_CAM_UAPI_LCESHO_SIZE, &offset);
		break;
	default:
		pr_debug("%s: dma_port err\n", __func__);
		break;
	}
}

static void mtk_cam_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_cam_device *cam = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_cam_buffer *buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	struct mtk_cam_request *req = to_mtk_cam_req(vb->request);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	struct device *dev = cam->dev;
	unsigned long flags;
	unsigned int desc_id;
	void *vaddr;

	dev_dbg(dev, "%s: node:%d fd:%d idx:%d\n", __func__,
		node->desc.id, buf->vbb.request_fd, buf->vbb.vb2_buf.index);

	/* TODO: add vdev_node.enabled check to prevent an useless buffer */

	/* added the buffer into the tracking list */
	spin_lock_irqsave(&node->buf_list_lock, flags);
	list_add_tail(&buf->list, &node->buf_list);
	spin_unlock_irqrestore(&node->buf_list_lock, flags);

	/* update buffer internal address */
	switch (node->desc.dma_port) {
	case MTKCAM_IPI_RAW_IMGO:
	case MTKCAM_IPI_RAW_RRZO:
		/* TODO: support sub-sampling multi-plane buffer */
		desc_id = node->desc.id-MTK_RAW_SOURCE_BEGIN;
		req->frame_params.img_outs[desc_id]
			.buf[0].iova = buf->daddr;
		/* un-processed raw frame */
		req->frame_params.raw_param.main_path_sel = 0;
		break;

	case MTKCAM_IPI_RAW_META_STATS_CFG:
		desc_id = node->desc.id-MTK_RAW_SINK_NUM;
		req->frame_params.meta_inputs[desc_id]
			.buf.ccd_fd = vb->planes[0].m.fd;
		/* vb->planes[0].bytesused; todo: vb2_q byteused is zero before stream on */
		req->frame_params.meta_inputs[desc_id]
			.buf.size = 80000;
		req->frame_params.meta_inputs[desc_id]
			.buf.iova = buf->daddr;
		req->frame_params.meta_inputs[desc_id].uid.id = node->desc.dma_port;
		break;

	case MTKCAM_IPI_RAW_META_STATS_0:
	case MTKCAM_IPI_RAW_META_STATS_1:
	case MTKCAM_IPI_RAW_META_STATS_2:
		desc_id = node->desc.id-MTK_RAW_META_OUT_BEGIN;
		req->frame_params.meta_outputs[desc_id]
			.buf.ccd_fd = vb->planes[0].m.fd;
		req->frame_params.meta_outputs[desc_id]
			.buf.size = vb->planes[0].bytesused;
		req->frame_params.meta_outputs[desc_id]
			.buf.iova = buf->daddr;
		req->frame_params.meta_outputs[desc_id].uid.id = node->desc.dma_port;
		vaddr = vb2_plane_vaddr(vb, 0);
		mtk_cam_set_meta_stats_info(node->desc.dma_port, vaddr);
		break;

	default:
		dev_dbg(dev, "%s buffer with invalid port\n", __func__);
		break;
	}

	/* update stream context to the request */
	req->ctx_used |= 1 << node->ctx->stream_id;
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

static const struct vb2_ops mtk_cam_vb2_ops = {
	.queue_setup = mtk_cam_vb2_queue_setup,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_init = mtk_cam_vb2_buf_init,
	.buf_prepare = mtk_cam_vb2_buf_prepare,
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
	switch (fmt) {
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SBGGR14_1X14:
		return MTK_CAM_RAW_PXL_ID_B;
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGBRG14_1X14:
		return MTK_CAM_RAW_PXL_ID_GB;
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SGRBG14_1X14:
		return MTK_CAM_RAW_PXL_ID_GR;
	case MEDIA_BUS_FMT_SRGGB8_1X8:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
	case MEDIA_BUS_FMT_SRGGB14_1X14:
		return MTK_CAM_RAW_PXL_ID_R;
	default:
		return MTK_CAM_RAW_PXL_ID_UNKNOWN;
	}
}

unsigned int mtk_cam_get_sensor_fmt(unsigned int fmt)
{
	switch (fmt) {
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
		return MTKCAM_IPI_INPUT_FMT_UNKNOWN;
	}
}

unsigned int mtk_cam_get_pixel_bits(unsigned int pix_fmt)
{
	switch (pix_fmt) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
	case V4L2_PIX_FMT_MTISP_SBGGR8F:
	case V4L2_PIX_FMT_MTISP_SGBRG8F:
	case V4L2_PIX_FMT_MTISP_SGRBG8F:
	case V4L2_PIX_FMT_MTISP_SRGGB8F:
		return 8;
	case V4L2_PIX_FMT_MTISP_SBGGR10:
	case V4L2_PIX_FMT_MTISP_SGBRG10:
	case V4L2_PIX_FMT_MTISP_SGRBG10:
	case V4L2_PIX_FMT_MTISP_SRGGB10:
	case V4L2_PIX_FMT_MTISP_SBGGR10F:
	case V4L2_PIX_FMT_MTISP_SGBRG10F:
	case V4L2_PIX_FMT_MTISP_SGRBG10F:
	case V4L2_PIX_FMT_MTISP_SRGGB10F:
		return 10;
	case V4L2_PIX_FMT_MTISP_SBGGR12:
	case V4L2_PIX_FMT_MTISP_SGBRG12:
	case V4L2_PIX_FMT_MTISP_SGRBG12:
	case V4L2_PIX_FMT_MTISP_SRGGB12:
	case V4L2_PIX_FMT_MTISP_SBGGR12F:
	case V4L2_PIX_FMT_MTISP_SGBRG12F:
	case V4L2_PIX_FMT_MTISP_SGRBG12F:
	case V4L2_PIX_FMT_MTISP_SRGGB12F:
		return 12;
	case V4L2_PIX_FMT_MTISP_SBGGR14:
	case V4L2_PIX_FMT_MTISP_SGBRG14:
	case V4L2_PIX_FMT_MTISP_SGRBG14:
	case V4L2_PIX_FMT_MTISP_SRGGB14:
	case V4L2_PIX_FMT_MTISP_SBGGR14F:
	case V4L2_PIX_FMT_MTISP_SGBRG14F:
	case V4L2_PIX_FMT_MTISP_SGRBG14F:
	case V4L2_PIX_FMT_MTISP_SRGGB14F:
		return 14;
	default:
		return 0;
	}
}

unsigned int mtk_cam_get_img_fmt(unsigned int fourcc)
{
	switch (fourcc) {
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
	default:
		return MTKCAM_IPI_INPUT_FMT_UNKNOWN;
	}
}

static void cal_image_pix_mp(unsigned int node_id,
			     struct v4l2_pix_format_mplane *mp)
{
	unsigned int bpl, ppl;
	unsigned int pixel_bits = mtk_cam_get_pixel_bits(mp->pixelformat);
	unsigned int img_fmt = mtk_cam_get_img_fmt(mp->pixelformat);
	unsigned int width = mp->width;

	bpl = 0;
	switch (img_fmt) {
	case MTKCAM_IPI_IMG_FMT_BAYER8:
	case MTKCAM_IPI_IMG_FMT_BAYER10:
	case MTKCAM_IPI_IMG_FMT_BAYER12:
	case MTKCAM_IPI_IMG_FMT_BAYER14:
		bpl = ALIGN(DIV_ROUND_UP(width * pixel_bits, 8), 2);
		break;
	case MTKCAM_IPI_IMG_FMT_FG_BAYER8:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER10:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER12:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER14:
		bpl = 0xFF00 & DIV_ROUND_UP(width * pixel_bits * 3 / 2, 8) + 0xFF;
		break;
	default:
		bpl = 0;
		break;
	}
	/*
	 * This image output buffer will be input buffer of MTK CAM DIP HW
	 * For MTK CAM DIP HW constrained, it needs 4 bytes alignment
	 */
	bpl = ALIGN(bpl, 4);
	mp->plane_fmt[0].bytesperline = bpl;
	mp->plane_fmt[0].sizeimage = bpl * mp->height;
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

	cal_image_pix_mp(desc->id, &active->fmt.pix_mp);

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
		q->dev = cam->dev;
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
		dev_dbg(v4l2_dev->dev, "Failed to init vb2 queue: %d\n", ret);
		goto error_vb2_init;
	}

	pad->flags = output ? MEDIA_PAD_FL_SOURCE : MEDIA_PAD_FL_SINK;
	ret = media_entity_pads_init(&vdev->entity, 1, pad);
	if (ret < 0) {
		dev_dbg(v4l2_dev->dev, "Failed to init video entity: %d\n",
			ret);
		goto error_media_init;
	}

	ret = mtk_video_init_format(video);
	if (ret < 0) {
		dev_dbg(v4l2_dev->dev, "Failed to init format: %d\n", ret);
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

	ret = video_register_device(vdev, VFL_TYPE_GRABBER, -1);
	if (ret < 0) {
		dev_dbg(v4l2_dev->dev, "Failed to register video device: %d\n",
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
		if (f->request_fd > 0) {
			node->pending_fmt = *f;
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
	struct mtk_cam_device *cam = node->ctx->cam;
	const struct v4l2_format *dev_fmt;
	struct v4l2_format try_fmt;
	s32 request_fd = f->request_fd;

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
	try_fmt.fmt.pix_mp.width = ALIGN(try_fmt.fmt.pix_mp.width, 4);

	/* Only support one plane */
	try_fmt.fmt.pix_mp.num_planes = 1;

	/* bytesperline & sizeimage calculation */
	cal_image_pix_mp(node->desc.id, &try_fmt.fmt.pix_mp);

	/* Constant format fields */
	try_fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_SRGB;
	try_fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
	try_fmt.fmt.pix_mp.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	try_fmt.fmt.pix_mp.quantization = V4L2_QUANTIZATION_DEFAULT;
	try_fmt.fmt.pix_mp.xfer_func = V4L2_XFER_FUNC_SRGB;

	*f = try_fmt;
	f->request_fd = request_fd;

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

