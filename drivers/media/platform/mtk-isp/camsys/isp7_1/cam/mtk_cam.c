// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/component.h>
#include <linux/freezer.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <linux/platform_data/mtk_ccd.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/rpmsg/mtk_ccd_rpmsg.h>
#include <linux/mtk_ccd_controls.h>
#include <linux/regulator/consumer.h>

#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/kthread.h>
#include <linux/media.h>
#include <linux/jiffies.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-subdev.h>
#include <media/media-entity.h>
#include <uapi/linux/sched/types.h>

#include <trace/hooks/v4l2core.h>
#include <trace/hooks/v4l2mc.h>

#include "mtk_cam.h"
#include "mtk_cam-ctrl.h"
#include "mtk_cam-feature.h"
#include "mtk_cam_pm.h"
#include "mtk_cam-dvfs_qos.h"
#include "mtk_cam-pool.h"
#include "mtk_cam-regs.h"
#include "mtk_cam-smem.h"
#include "mtk_cam-tg-flash.h"
#include "mtk_camera-v4l2-controls.h"
#include "mtk_camera-videodev2.h"
#include "mtk_cam-hsf.h"
#include "mtk_cam-trace.h"
#include "mtk_cam-ufbc-def.h"

#ifdef CAMSYS_TF_DUMP_71_1
#include <dt-bindings/memory/mt6983-larb-port.h>
#include "iommu_debug.h"
#endif
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <aee.h>
#endif
#include "mtk_cam-timesync.h"

#ifdef CONFIG_VIDEO_MTK_ISP_CAMSYS_DUBUG
static unsigned int debug_ae = 1;
#else
static unsigned int debug_ae;
#endif
module_param(debug_ae, uint, 0644);
MODULE_PARM_DESC(debug_ae, "activates debug ae info");

/* FIXME for CIO pad id */
#define MTK_CAM_CIO_PAD_SRC		PAD_SRC_RAW0
#define MTK_CAM_CIO_PAD_SINK		MTK_RAW_SINK

/* Zero out the end of the struct pointed to by p.  Everything after, but
 * not including, the specified field is cleared.
 */
#define CLEAR_AFTER_FIELD(p, field) \
	memset((u8 *)(p) + offsetof(typeof(*(p)), field) + sizeof((p)->field), \
	0, sizeof(*(p)) - offsetof(typeof(*(p)), field) -sizeof((p)->field))

static const struct of_device_id mtk_cam_of_ids[] = {
	{.compatible = "mediatek,camisp",},
	{}
};

MODULE_DEVICE_TABLE(of, mtk_cam_of_ids);

static void mtk_cam_register_iommu_tf_callback(struct mtk_raw_device *raw)
{
#ifdef CAMSYS_TF_DUMP_71_1
	dev_dbg(raw->dev, "%s : raw->id:%d\n", __func__, raw->id);

	if (raw->id == RAW_A) {
		mtk_iommu_register_fault_callback(M4U_PORT_L16_CAM2_IMGP_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_CAM2_CQI_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_CAM2_CQI_R2,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_CAM2_FHO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_CAM2_AAO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_CAM2_TSFSO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_CAM2_FLKO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_CAM2_RAWI_R2,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_CAM2_RAWI_R3,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_CAM2_RAWI_R5,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_CAM2_AAI_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L17_CAM3_YUVO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L17_CAM3_YUVO_R3,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L17_CAM3_YUVCO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L17_CAM3_YUVO_R2,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L17_CAM3_RZH1N2TO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L17_CAM3_DRZS4NO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L17_CAM3_TNCSO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);
	} else if (raw->id == RAW_B) {
		mtk_iommu_register_fault_callback(M4U_PORT_L27_CAM2_IMGP_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L27_CAM2_CQI_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L27_CAM2_CQI_R2,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L27_CAM2_FHO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L27_CAM2_AAO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L27_CAM2_TSFSO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L27_CAM2_FLKO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L27_CAM2_RAWI_R2,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L27_CAM2_RAWI_R3,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L27_CAM2_RAWI_R5,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L27_CAM2_AAI_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L29_CAM3_YUVO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L29_CAM3_YUVO_R3,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L29_CAM3_YUVCO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L29_CAM3_YUVO_R2,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L29_CAM3_RZH1N2TO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L29_CAM3_DRZS4NO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L29_CAM3_TNCSO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);
	} else if (raw->id == RAW_C) {
		mtk_iommu_register_fault_callback(M4U_PORT_L28_CAM2_IMGP_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L28_CAM2_CQI_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L28_CAM2_CQI_R2,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L28_CAM2_FHO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L28_CAM2_AAO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L28_CAM2_TSFSO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L28_CAM2_FLKO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L28_CAM2_RAWI_R2,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L28_CAM2_RAWI_R3,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L28_CAM2_RAWI_R5,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L28_CAM2_AAI_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L30_CAM3_YUVO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L30_CAM3_YUVO_R3,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L30_CAM3_YUVCO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L30_CAM3_YUVO_R2,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L30_CAM3_RZH1N2TO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L30_CAM3_DRZS4NO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L30_CAM3_TNCSO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);
	}
#endif
}

/**
 * All member of mtk_cam_request which may be used after
 * media_request_ioctl_reinit and before next media_request_ioctl_queue
 * must be clean here.
 */
static void mtk_cam_req_clean(struct mtk_cam_request *req)
{
	req->ctx_link_update = 0;
}

/**
 * All member of mtk_cam_request_stream_data which may be used
 * after media_request_ioctl_reinit and before the next
 * media_request_ioctl_queue must be clean here. For
 * example, the pending set fmt, set selection, and sensor
 * switch extension of camsys driver.
 */
static void
mtk_cam_req_s_data_clean(struct mtk_cam_request_stream_data *s_data)
{
	s_data->seninf_old = NULL;
	s_data->seninf_new = NULL;
	s_data->sensor = NULL;
	s_data->pad_fmt_update = 0;
	s_data->vdev_fmt_update = 0;
	s_data->vdev_selection_update = 0;
	s_data->flags = 0;
}

static void
mtk_cam_req_pipe_s_data_clean(struct mtk_cam_request *req, int pipe_id,
			      int index)
{
	struct mtk_cam_request_stream_data *req_stream_data;

	req_stream_data = mtk_cam_req_get_s_data(req, pipe_id, index);
	if (req_stream_data) {
		mtk_cam_req_s_data_clean(req_stream_data);
		/**
		 * Notice that the we clean req_stream_data->bufs here so
		 * we should not use it after this function called on it.
		 * mtk_cam_vb2_return_all_buffers() uses another list
		 * of mtk_cam_video_device to keep the vb2 buffers to be clean.
		 */
		memset(req_stream_data->bufs, 0, sizeof(req_stream_data->bufs));
	}
}

void mtk_cam_s_data_update_timestamp(struct mtk_cam_buffer *buf,
				     struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_ctx *ctx;
	struct vb2_buffer *vb;
	struct mtk_cam_video_device *node;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	if (!ctx) {
		pr_info("%s: get ctx from s_data failed", __func__);
		return;
	}

	vb = &buf->vbb.vb2_buf;
	node = mtk_cam_vbq_to_vdev(vb->vb2_queue);

	buf->vbb.sequence = s_data->frame_seq_no;
	if (vb->vb2_queue->timestamp_flags & V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC)
		vb->timestamp = s_data->timestamp_mono;
	else
		vb->timestamp = s_data->timestamp;

	/*check buffer's timestamp*/
	if (node->desc.dma_port == MTKCAM_IPI_RAW_META_STATS_CFG)
		dev_dbg(ctx->cam->dev,
			"%s:%s:vb sequence:%d, queue type:%d, timestamp_flags:0x%x, timestamp:%lld\n",
			__func__, node->desc.name, buf->vbb.sequence,
			vb->vb2_queue->type, vb->vb2_queue->timestamp_flags,
			vb->timestamp);
}

static void mtk_cam_req_return_pipe_buffers(struct mtk_cam_request *req,
					    int pipe_id, int index)
{
	struct mtk_cam_device *cam =
		container_of(req->req.mdev, struct mtk_cam_device, media_dev);
	struct mtk_cam_request_stream_data *s_data_pipe;
	struct mtk_cam_buffer *buf_ret[MTK_RAW_TOTAL_NODES];
	struct mtk_cam_buffer *buf;
	struct mtk_cam_video_device *node;
	struct vb2_buffer *vb;
	int i, buf_state, buf_start, buf_end, buf_ret_cnt;

	s_data_pipe = mtk_cam_req_get_s_data(req, pipe_id, index);
	if (!s_data_pipe) {
		pr_info("%s: get s_data pipe failed", __func__);
		return;
	}

	if (is_raw_subdev(pipe_id)) {
		if (atomic_read(&req->state) > MTK_CAM_REQ_STATE_PENDING)
			mtk_cam_tg_flash_req_done(s_data_pipe);
		buf_start = MTK_RAW_SINK_NUM;
		buf_end = MTK_RAW_PIPELINE_PADS_NUM;
	}

	if (is_camsv_subdev(pipe_id)) {
		buf_start = MTK_CAMSV_SINK_BEGIN;
		buf_end = MTK_CAMSV_PIPELINE_PADS_NUM;
	}

	if (is_mraw_subdev(pipe_id)) {
		buf_start = MTK_MRAW_SINK_BEGIN;
		buf_end = MTK_MRAW_PIPELINE_PADS_NUM;
	}

	buf_ret_cnt = 0;
	for (i = buf_start; i < buf_end; i++) {
		/* make sure do not touch req/s_data after vb2_buffe_done */
		buf = mtk_cam_s_data_get_vbuf(s_data_pipe, i);
		if (!buf)
			continue;
		buf_ret[buf_ret_cnt++] = buf;
		/* clean the stream data for req reinit case */
		mtk_cam_s_data_reset_vbuf(s_data_pipe, i);
	}

	/* clean the req_stream_data being used right after request reinit */
	mtk_cam_req_pipe_s_data_clean(req, pipe_id, index);

	buf_state = atomic_read(&s_data_pipe->buf_state);
	if (buf_state == -1)
		buf_state = VB2_BUF_STATE_ERROR;

	dev_dbg(cam->dev,
		"%s:%s: pipe_id(%d) buf_state(%d) buf_ret_cnt(%d)\n", __func__,
		req->req.debug_str, pipe_id, buf_state, buf_ret_cnt);

	for (i = 0; i < buf_ret_cnt; i++) {
		buf = buf_ret[i];
		vb = &buf->vbb.vb2_buf;
		node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
		if (node->uid.pipe_id != pipe_id) {
			dev_info(cam->dev,
				"%s:%s:node(%s): invalid pipe id (%d), should be (%d)\n",
				__func__, req->req.debug_str,
				node->desc.name, node->uid.pipe_id, pipe_id);
			continue;
		}

		// TODO(mstream): fill timestamp
		if (atomic_read(&req->state) > MTK_CAM_REQ_STATE_PENDING)
			mtk_cam_s_data_update_timestamp(buf, s_data_pipe);

		vb2_buffer_done(&buf->vbb.vb2_buf, buf_state);
	}
}

struct mtk_cam_request_stream_data*
mtk_cam_get_req_s_data(struct mtk_cam_ctx *ctx, unsigned int pipe_id,
			unsigned int frame_seq_no)

{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request *req, *req_prev;
	struct mtk_cam_request_stream_data *req_stream_data;
	int i;

	spin_lock(&cam->running_job_lock);
	list_for_each_entry_safe(req, req_prev, &cam->running_job_list, list) {
		if (req->pipe_used & (1 << pipe_id)) {
			for (i = 0; i < req->p_data[pipe_id].s_data_num; i++) {
				req_stream_data = &req->p_data[pipe_id].s_data[i];
				if (req_stream_data->frame_seq_no == frame_seq_no) {
					spin_unlock(&cam->running_job_lock);
					return req_stream_data;
				}
			}
		}
	}
	spin_unlock(&cam->running_job_lock);

	return NULL;
}

void mtk_cam_req_update_seq(struct mtk_cam_ctx *ctx, struct mtk_cam_request *req,
			    int seq)
{
	req->p_data[ctx->stream_id].req_seq = seq;
}

struct mtk_cam_request *mtk_cam_get_req(struct mtk_cam_ctx *ctx,
					unsigned int frame_seq_no)
{
	struct mtk_cam_request_stream_data *req_stream_data;

	req_stream_data = mtk_cam_get_req_s_data(ctx, ctx->stream_id, frame_seq_no);
	if (!req_stream_data)
		return NULL;

	return req_stream_data->req;

}
bool watchdog_scenario(struct mtk_cam_ctx *ctx)
{
	if (ctx->sensor && !mtk_cam_is_m2m(ctx) && ctx->used_raw_num)
		return true;
	else
		return false;
}

static bool finish_cq_buf(struct mtk_cam_request_stream_data *req_stream_data)
{
	bool result = false;
	struct mtk_cam_ctx *ctx = req_stream_data->ctx;
	struct mtk_cam_working_buf_entry *cq_buf_entry;

	if (!ctx->used_raw_num)
		return false;

	spin_lock(&ctx->processing_buffer_list.lock);

	cq_buf_entry = req_stream_data->working_buf;
	/* Check if the cq buffer is already finished */
	if (!cq_buf_entry || !cq_buf_entry->s_data) {
		dev_info(ctx->cam->dev,
			 "%s:%s:ctx(%d):req(%d):working_buf is already release\n", __func__,
			req_stream_data->req->req.debug_str, ctx->stream_id,
			req_stream_data->frame_seq_no);
		spin_unlock(&ctx->processing_buffer_list.lock);
		return false;
	}

	list_del(&cq_buf_entry->list_entry);
	mtk_cam_s_data_reset_wbuf(req_stream_data);
	ctx->processing_buffer_list.cnt--;
	spin_unlock(&ctx->processing_buffer_list.lock);

	mtk_cam_working_buf_put(cq_buf_entry);
	result = true;

	dev_dbg(ctx->cam->dev, "put cq buf:%pad, %s\n",
			&cq_buf_entry->buffer.iova,
			req_stream_data->req->req.debug_str);

	return result;
}

static bool finish_img_buf(struct mtk_cam_request_stream_data *req_stream_data)
{
	bool result = false;
	struct mtk_cam_ctx *ctx = req_stream_data->ctx;
	struct mtk_cam_img_working_buf_entry *buf_entry, *buf_entry_prev;

	if (!ctx->used_raw_num)
		return false;

	spin_lock(&ctx->processing_img_buffer_list.lock);
	if (ctx->processing_img_buffer_list.cnt == 0) {
		spin_unlock(&ctx->processing_img_buffer_list.lock);
		return false;
	}

	list_for_each_entry_safe(buf_entry, buf_entry_prev,
				 &ctx->processing_img_buffer_list.list,
				 list_entry) {
		if (buf_entry->s_data == req_stream_data) {
			list_del(&buf_entry->list_entry);
			mtk_cam_img_wbuf_set_s_data(buf_entry, NULL);
			mtk_cam_img_working_buf_put(buf_entry);
			ctx->processing_img_buffer_list.cnt--;
			result = true;
			dev_dbg(ctx->cam->dev, "[%s] iova:0x%x\n",
				__func__, buf_entry->img_buffer.iova);
		}
	}
	spin_unlock(&ctx->processing_img_buffer_list.lock);

	return result;
}


static void update_hw_mapping(struct mtk_cam_ctx *ctx,
	struct mtkcam_ipi_config_param *config_param, int feature, int stagger_path,
	int enabled_raw)
{
	int i, exp_no;
	bool bDcif;

	/* raw */
	config_param->maps[0].pipe_id = ctx->pipe->id;
	config_param->maps[0].dev_mask = MTKCAM_SUBDEV_RAW_MASK & ctx->used_raw_dev;
	config_param->n_maps = 1;

	if (mtk_cam_feature_is_stagger(feature)) {
		/* check exposure number */
		if (mtk_cam_feature_is_2_exposure(feature))
			exp_no = 2;
		else if (mtk_cam_feature_is_3_exposure(feature))
			exp_no = 3;
		else
			exp_no = 1;

		/* check stagger mode */
		bDcif = (stagger_path == STAGGER_DCIF) ? true : false;

		/* update hw maaping */
		for (i = MTKCAM_SUBDEV_CAMSV_START; i < MTKCAM_SUBDEV_CAMSV_END; i++) {
			if (enabled_raw & (1 << i)) {
				if (config_param->n_maps < ARRAY_SIZE(config_param->maps)) {
					config_param->maps[config_param->n_maps].pipe_id = i;
					config_param->maps[config_param->n_maps].dev_mask =
						(1 << i);
					if (ctx->cam->sv.pipelines[i -
						MTKCAM_SUBDEV_CAMSV_START].hw_cap &
						(1 << CAMSV_EXP_ORDER_SHIFT))
						config_param->maps[
						config_param->n_maps].exp_order =
							(bDcif && (exp_no == 1)) ? 2 : 0;
					else if (ctx->cam->sv.pipelines[i -
						MTKCAM_SUBDEV_CAMSV_START].hw_cap &
						(1 << (CAMSV_EXP_ORDER_SHIFT + 1)))
						config_param->maps[
						config_param->n_maps].exp_order =
							(bDcif && (exp_no == 2)) ? 2 : 1;
					else
						config_param->maps[
						config_param->n_maps].exp_order = 2;
					dev_info(ctx->cam->dev, "hw mapping pipe_id:%d exp_order:%d",
						i,
						config_param->maps[config_param->n_maps].exp_order);
					config_param->n_maps++;
				} else
					dev_info(ctx->cam->dev, "hw mapping is over size(enable_raw:%d)",
						ctx->pipe->enabled_raw);
			}
		}
	}
}

static void mtk_cam_del_req_from_running(struct mtk_cam_ctx *ctx,
					 struct mtk_cam_request *req, int pipe_id)
{
	struct mtk_cam_request_stream_data *s_data;

	s_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
	dev_dbg(ctx->cam->dev,
		"%s: %s: removed, req:%d, ctx:(%d/0x%x/0x%x), pipe:(%d/0x%x/0x%x) done_status:0x%x, state:%d)\n",
		__func__, req->req.debug_str, s_data->frame_seq_no,
		ctx->stream_id, req->ctx_used, ctx->cam->streaming_ctx,
		pipe_id, req->pipe_used, ctx->cam->streaming_pipe,
		req->done_status);

	atomic_set(&req->state, MTK_CAM_REQ_STATE_COMPLETE);
	spin_lock(&ctx->cam->running_job_lock);
	list_del(&req->list);
	ctx->cam->running_job_count--;
	spin_unlock(&ctx->cam->running_job_lock);
}

static void mtk_cam_req_works_clean(struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_ctx *ctx = mtk_cam_s_data_get_ctx(s_data);
	char *dbg_str = mtk_cam_s_data_get_dbg_str(s_data);

	/* flush the sensor work */
	if (atomic_read(&s_data->sensor_work.is_queued)) {
		kthread_flush_work(&s_data->sensor_work.work);
		dev_dbg(ctx->cam->dev,
				"%s:ctx(%d):%s:seq(%d): flushed sensor_work\n",
				__func__, ctx->stream_id, dbg_str, s_data->frame_seq_no);
	}
}

void *mtk_cam_get_vbuf_va(struct mtk_cam_ctx *ctx,
		struct mtk_cam_request_stream_data *s_data, int node_id)
{
	struct mtk_cam_buffer *buf;
	struct vb2_buffer *vb;
	void *vaddr;

	buf = mtk_cam_s_data_get_vbuf(s_data, node_id);
	if (!buf) {
		dev_info(ctx->cam->dev,
			 "ctx(%d): can't get MTK_RAW_META_OUT_0 buf from req(%d)\n",
			 ctx->stream_id, s_data->frame_seq_no);
		return NULL;
	}

	vb = &buf->vbb.vb2_buf;
	if (!vb) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d): can't get vb2 buf\n",
			 __func__, ctx->stream_id);
		return NULL;
	}

	vaddr = vb2_plane_vaddr(&buf->vbb.vb2_buf, 0);
	if (!vaddr) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d): can't get plane_vadd\n",
			 __func__, ctx->stream_id);
		return NULL;
	}
	return vaddr;
}

void mtk_cam_get_timestamp(struct mtk_cam_ctx *ctx,
		struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_buffer *buf;
	struct vb2_buffer *vb;
	void *vaddr;
	int subsample = 0;
	uint64_t *pTimestamp;
	u32 *fho_va;
	int i;

	if (ctx->used_raw_num != 0)
		subsample =
			mtk_cam_get_subsample_ratio(ctx->pipe->res_config.raw_feature);

	buf = mtk_cam_s_data_get_vbuf(s_data, MTK_RAW_META_OUT_0);
	if (!buf) {
		dev_info(ctx->cam->dev,
			 "ctx(%d): can't get MTK_RAW_META_OUT_0 buf from req(%d)\n",
			 ctx->stream_id, s_data->frame_seq_no);
		return;
	}

	vb = &buf->vbb.vb2_buf;
	if (!vb) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d): can't get vb2 buf\n",
			 __func__, ctx->stream_id);
		return;
	}

	vaddr = vb2_plane_vaddr(&buf->vbb.vb2_buf, 0);
	if (!vaddr) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d): can't get plane_vadd\n",
			 __func__, ctx->stream_id);
		return;
	}

	if ((s_data->working_buf->buffer.va == (void *)NULL) ||
		s_data->working_buf->buffer.size == 0) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d): can't get working_buf\n",
			 __func__, ctx->stream_id);
		return;
	}

	fho_va = (u32 *)(s_data->working_buf->buffer.va +
		s_data->working_buf->buffer.size - 64 * (subsample + 1));

	pTimestamp = mtk_cam_get_timestamp_addr(vaddr);
	for (i = 0; i < (subsample + 1); i++) {
		/* timstamp_LSB + timestamp_MSB << 32 */
		*(pTimestamp + i*2) = mtk_cam_timesync_to_monotonic
		((u64) (*(fho_va + i*16)) + ((u64)(*(fho_va + i*16 + 1)) << 32))
		/1000;
		*(pTimestamp + i*2 + 1) = mtk_cam_timesync_to_boot
		((u64) (*(fho_va + i*16)) + ((u64)(*(fho_va + i*16 + 1)) << 32))
		/1000;
		dev_dbg(ctx->cam->dev,
			"timestamp TS:momo %ld us boot %ld us, LSB:%d MSB:%d\n",
			*(pTimestamp + i*2), *(pTimestamp + i*2 + 1),
			*(fho_va + i*16), *(fho_va + i*16 + 1));
	}
}

int mtk_cam_dequeue_req_frame(struct mtk_cam_ctx *ctx,
			      unsigned int dequeued_frame_seq_no,
			      int pipe_id)
{
	struct mtk_cam_request *req, *req_prev;
	struct mtk_cam_request_stream_data *s_data, *s_data_pipe, *s_data_mstream;
	struct mtk_cam_request_stream_data *deq_s_data[18];
	struct mtk_raw_pipeline *pipe = ctx->pipe;
	/* consider running_job_list depth and mstream(2 s_data): 3*3*2 */
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	int feature, buf_state;
	int dequeue_cnt, s_data_cnt, handled_cnt;
	bool del_job, del_req;
	bool unreliable = false;
	void *vaddr = NULL;
	struct mtk_ae_debug_data ae_data;

	dequeue_cnt = 0;
	s_data_cnt = 0;
	spin_lock(&ctx->cam->running_job_lock);
	list_for_each_entry_safe(req, req_prev, &ctx->cam->running_job_list, list) {
		if (!(req->pipe_used & (1 << pipe_id)))
			continue;

		s_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		if (!s_data) {
			dev_info(ctx->cam->dev,
				"frame_seq:%d[ctx=%d,pipe=%d], de-queue request not found\n",
				dequeued_frame_seq_no, ctx->stream_id, pipe_id);
			continue;
		}

		if (s_data->frame_seq_no > dequeued_frame_seq_no)
			goto STOP_SCAN;

		deq_s_data[s_data_cnt++] = s_data;
		if (s_data_cnt >= 18) {
			dev_info(ctx->cam->dev,
				 "%s:%s:ctx(%d):pipe(%d):seq(%d/%d) dequeue s_data over local buffer cnt(%d)\n",
				 __func__, req->req.debug_str, ctx->stream_id, pipe_id,
				 s_data->frame_seq_no, dequeued_frame_seq_no,
				 s_data->frame_seq_no, s_data_cnt);
			goto STOP_SCAN;
		}
	}

STOP_SCAN:
	spin_unlock(&ctx->cam->running_job_lock);

	for (handled_cnt = 0; handled_cnt < s_data_cnt; handled_cnt++) {
		s_data = deq_s_data[handled_cnt];
		del_req = false;
		del_job = false;
		feature = s_data->feature.raw_feature;
		req = mtk_cam_s_data_get_req(s_data);
		if (!req) {
			dev_info(ctx->cam->dev,
				"%s:ctx(%d):pipe(%d):seq(%d) req not found\n",
				__func__, ctx->stream_id, pipe_id,
				s_data->frame_seq_no);
			continue;
		}

		spin_lock(&req->done_status_lock);

		if (req->done_status & 1 << pipe_id) {
			/* already handled by another job done work */
			spin_unlock(&req->done_status_lock);
			continue;
		}

		/* Check whether all pipelines of single ctx are done */
		req->done_status |= 1 << pipe_id;
		if ((req->done_status & ctx->streaming_pipe) ==
		    (req->pipe_used & ctx->streaming_pipe))
			del_job = true;


		if ((req->done_status & ctx->cam->streaming_pipe) ==
		    (req->pipe_used & ctx->cam->streaming_pipe)) {
			if (MTK_CAM_REQ_STATE_RUNNING ==
			    atomic_cmpxchg(&req->state,
					   MTK_CAM_REQ_STATE_RUNNING,
					   MTK_CAM_REQ_STATE_DELETING))
				del_req = true;
		}
		if (is_raw_subdev(pipe_id))
			vaddr = mtk_cam_get_vbuf_va(ctx, s_data, MTK_RAW_META_OUT_0);

		if (is_raw_subdev(pipe_id) && debug_ae) {
			dump_aa_info(ctx, &ae_data);
			dev_info(ctx->cam->dev,
				"%s:%s:ctx(%d):pipe(%d):de-queue seq(%d):handle seq(%d),done(0x%x),pipes(req:0x%x,ctx:0x%x,all:0x%x),del_job(%d),del_req(%d),metaout va(0x%llx),size(%d,%d),AA(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)\n",
				__func__, req->req.debug_str, ctx->stream_id, pipe_id,
				dequeued_frame_seq_no, s_data->frame_seq_no, req->done_status,
				req->pipe_used, ctx->streaming_pipe, ctx->cam->streaming_pipe,
				del_job, del_req, vaddr,
				pipe->res_config.sink_fmt.width, pipe->res_config.sink_fmt.height,
				ae_data.OBC_R1_Sum[0], ae_data.OBC_R1_Sum[1],
				ae_data.OBC_R1_Sum[2], ae_data.OBC_R1_Sum[3],
				ae_data.OBC_R2_Sum[0], ae_data.OBC_R2_Sum[1],
				ae_data.OBC_R2_Sum[2], ae_data.OBC_R2_Sum[3],
				ae_data.OBC_R3_Sum[0], ae_data.OBC_R3_Sum[1],
				ae_data.OBC_R3_Sum[2], ae_data.OBC_R3_Sum[3],
				ae_data.AA_Sum[0], ae_data.AA_Sum[1],
				ae_data.AA_Sum[2], ae_data.AA_Sum[3],
				ae_data.LTM_Sum[0], ae_data.LTM_Sum[1],
				ae_data.LTM_Sum[2], ae_data.LTM_Sum[3]);
		} else
			dev_info(ctx->cam->dev,
				"%s:%s:ctx(%d):pipe(%d):de-queue seq(%d):handle seq(%d),done(0x%x),pipes(req:0x%x,ctx:0x%x,all:0x%x),del_job(%d),del_req(%d),metaout va(0x%llx)\n",
				__func__, req->req.debug_str, ctx->stream_id, pipe_id,
				dequeued_frame_seq_no, s_data->frame_seq_no, req->done_status,
				req->pipe_used, ctx->streaming_pipe, ctx->cam->streaming_pipe,
				del_job, del_req, vaddr);

		spin_unlock(&req->done_status_lock);

		if (mtk_cam_feature_is_mstream(feature) || mtk_cam_feature_is_mstream_m2m(feature))
			s_data_mstream = mtk_cam_req_get_s_data(req, ctx->stream_id, 1);
		else
			s_data_mstream = NULL;

		if (is_raw_subdev(pipe_id)) {
			mtk_cam_get_timestamp(ctx, s_data);
			mtk_cam_req_dbg_works_clean(s_data);
			mtk_cam_req_works_clean(s_data);

			if (s_data_mstream) {
				mtk_cam_req_dbg_works_clean(s_data_mstream);
				mtk_cam_req_works_clean(s_data_mstream);
			}
		}

		if (del_job) {
			atomic_dec(&ctx->running_s_data_cnt);
			mtk_camsys_state_delete(ctx, sensor_ctrl, req);

			/* release internal buffers */
			mtk_cam_sv_finish_buf(s_data);
			mtk_cam_mraw_finish_buf(s_data);
			finish_cq_buf(s_data);

			if (mtk_cam_is_time_shared(ctx))
				finish_img_buf(s_data);

			if (s_data_mstream) {
				finish_cq_buf(s_data_mstream);
				mtk_cam_sv_finish_buf(s_data_mstream);
				mtk_cam_mraw_finish_buf(s_data_mstream);
			}
		}

		if (del_req) {
			mtk_cam_del_req_from_running(ctx, req, pipe_id);
			dequeue_cnt++;
		}

		if (mtk_cam_feature_is_mstream(feature)) {
			unreliable |= (s_data->flags &
						   MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_DELAYED);

			if (s_data_mstream) {
				unreliable |= (s_data_mstream->flags &
					MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_DELAYED);
			}
		}

		/* release vb2 buffers of the pipe */
		s_data_pipe = mtk_cam_req_get_s_data(req, pipe_id, 0);
		if (!s_data_pipe) {
			dev_info(ctx->cam->dev,
				"%s:%s:ctx(%d):pipe(%d):seq(%d) s_data_pipe not found\n",
				__func__, req->req.debug_str, ctx->stream_id, pipe_id,
				s_data->frame_seq_no);
			continue;
		}

		if (s_data->frame_seq_no < dequeued_frame_seq_no) {
			buf_state = VB2_BUF_STATE_ERROR;
			dev_dbg(ctx->cam->dev,
				"%s:%s:pipe(%d) seq:%d, time:%lld drop, ctx:%d\n",
				__func__, req->req.debug_str, pipe_id,
				s_data->frame_seq_no, s_data->timestamp,
				ctx->stream_id);
		} else if (s_data->state.estate == E_STATE_DONE_MISMATCH) {
			buf_state = VB2_BUF_STATE_ERROR;
			dev_dbg(ctx->cam->dev,
				"%s:%s:pipe(%d) seq:%d, state done mismatch",
				__func__, req->req.debug_str, pipe_id,
				s_data->frame_seq_no);
		} else if (unreliable) {
			buf_state = VB2_BUF_STATE_ERROR;
			dev_dbg(ctx->cam->dev,
				"%s:%s:pipe(%d) seq:%d, done (unreliable)",
				__func__, req->req.debug_str, pipe_id,
				s_data->frame_seq_no);
		} else {
			buf_state = VB2_BUF_STATE_DONE;
			dev_dbg(ctx->cam->dev,
				"%s:%s:pipe(%d) seq:%d, done success",
				__func__, req->req.debug_str, pipe_id,
				s_data->frame_seq_no);
		}

		if (mtk_cam_s_data_set_buf_state(s_data_pipe, buf_state)) {
			/* handle vb2_buffer_done */
			if (mtk_cam_req_put(req, pipe_id))
				dev_dbg(ctx->cam->dev,
					"%s:%s:pipe(%d) return request",
					__func__, req->req.debug_str, pipe_id);
		}
	}

	return dequeue_cnt;
}

void mtk_cam_dev_req_clean_pending(struct mtk_cam_device *cam, int pipe_id,
				   int buf_state)
{
	struct mtk_cam_request *req, *req_prev;
	struct mtk_cam_request_stream_data *s_data_pipe;
	struct list_head *pending = &cam->pending_job_list;
	struct list_head req_clean_list;

	/* Consider pipe bufs and pipe_used only */

	INIT_LIST_HEAD(&req_clean_list);

	spin_lock(&cam->pending_job_lock);
	list_for_each_entry_safe(req, req_prev, pending, list) {
		/* update pipe_used */
		req->pipe_used &= ~(1 << pipe_id);
		list_add_tail(&req->cleanup_list, &req_clean_list);
		if (!(req->pipe_used & cam->streaming_pipe)) {
			/* the last pipe */
			list_del(&req->list);
			dev_info(cam->dev,
				 "%s:%s:pipe(%d) remove req from pending list\n",
				 __func__, req->req.debug_str, pipe_id);
		}
	}
	spin_unlock(&cam->pending_job_lock);

	list_for_each_entry_safe(req, req_prev, &req_clean_list, cleanup_list) {
		list_del(&req->cleanup_list);
		s_data_pipe = mtk_cam_req_get_s_data(req, pipe_id, 0);
		if (!s_data_pipe) {
			dev_dbg(cam->dev,
				"%s:%s:pipe_used(0x%x):pipe(%d) s_data_pipe not found\n",
				__func__, req->req.debug_str, req->pipe_used,
				pipe_id);
			continue;
		}
		if (mtk_cam_s_data_set_buf_state(s_data_pipe, buf_state)) {
			/* handle vb2_buffer_done */
			if (mtk_cam_req_put(req, pipe_id))
				dev_dbg(cam->dev,
					"%s:%s:pipe_used(0x%x):pipe(%d) return request",
					__func__, req->req.debug_str,
					req->pipe_used, pipe_id);
			/* DO NOT touch req after here */
		}
	}
}

void mtk_cam_dev_req_cleanup(struct mtk_cam_ctx *ctx, int pipe_id, int buf_state)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request *req, *req_prev;
	struct mtk_cam_request_stream_data *s_data, *s_data_pipe;
	struct mtk_cam_request_stream_data *clean_s_data[18];
	/* consider running_job_list depth and mstream(2 s_data): 3*3*2 */
	struct list_head *running = &cam->running_job_list;
	unsigned int other_pipes, done_status;
	int i, num_s_data, s_data_cnt, handled_cnt;
	bool need_clean_req;

	mutex_lock(&cam->queue_lock);
	mtk_cam_dev_req_clean_pending(cam, pipe_id, buf_state);

	s_data_cnt = 0;
	spin_lock(&cam->running_job_lock);
	list_for_each_entry_safe(req, req_prev, running, list) {
		/* only handle requests belong to current ctx */
		if (!(req->pipe_used & ctx->streaming_pipe))
			continue;

		num_s_data = mtk_cam_req_get_num_s_data(req, pipe_id);
		/* reverse the order for release req with s_data_0 */
		for (i = num_s_data - 1; i >= 0; i--) {
			s_data = mtk_cam_req_get_s_data(req, pipe_id, i);
			if (s_data) {
				clean_s_data[s_data_cnt++] = s_data;
				if (s_data_cnt >= 18) {
					dev_info(cam->dev,
						 "%s: over local buffer cnt(%d)\n",
						 __func__,  s_data_cnt);
					goto STOP_SCAN;
				}
			} else {
				dev_info(cam->dev,
					 "%s:%s:pipe(%d): get s_data failed\n",
					 __func__, req->req.debug_str, pipe_id);
			}
		}
	}
STOP_SCAN:
	spin_unlock(&cam->running_job_lock);
	mutex_unlock(&cam->queue_lock);

	for (handled_cnt = 0; handled_cnt < s_data_cnt; handled_cnt++) {
		s_data = clean_s_data[handled_cnt];
		req = mtk_cam_s_data_get_req(s_data);
		if (!req) {
			pr_info("ERR can't be recovered: invalid req found in s_data_clean_list\n");
			continue;
		}

		if (ctx->used_raw_num != 0) {
			if (s_data->index > 0)
				dev_info(cam->dev,
					"%s:%s:pipe(%d):seq(%d): clean s_data_%d, raw_feature(%d)\n",
					__func__, req->req.debug_str, pipe_id,
					s_data->frame_seq_no, s_data->index,
					ctx->pipe->feature_pending);
			else
				dev_dbg(cam->dev,
					"%s:%s:pipe(%d):seq(%d): clean s_data_%d, raw_feature(%d)\n",
					__func__, req->req.debug_str, pipe_id,
					s_data->frame_seq_no, s_data->index,
					ctx->pipe->feature_pending);
		}

		 /* Cancel s_data's works before we clean up the data */
		if (atomic_read(&s_data->sensor_work.is_queued)) {
			kthread_cancel_work_sync(&s_data->sensor_work.work);
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):seq(%d): cancel sensor_work\n",
				 __func__, req->req.debug_str, pipe_id,
				 s_data->frame_seq_no);
		}
		atomic_set(&s_data->sensor_work.is_queued, 1);

		if (atomic_read(&s_data->meta1_done_work.is_queued)) {
			cancel_work_sync(&s_data->meta1_done_work.work);
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):seq(%d): cancel AFO done_work\n",
				 __func__, req->req.debug_str, pipe_id,
				 s_data->frame_seq_no);
		}
		atomic_set(&s_data->meta1_done_work.is_queued, 1);

		if (atomic_read(&s_data->frame_done_work.is_queued)) {
			cancel_work_sync(&s_data->frame_done_work.work);
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):seq(%d): cancel frame_done_work\n",
				 __func__, req->req.debug_str, pipe_id,
				 s_data->frame_seq_no);
		}
		atomic_set(&s_data->frame_done_work.is_queued, 1);

		if (atomic_read(&s_data->dbg_exception_work.state) ==
			MTK_CAM_REQ_DBGWORK_S_PREPARED) {
			atomic_set(&s_data->dbg_exception_work.state, MTK_CAM_REQ_DBGWORK_S_CANCEL);
			mtk_cam_debug_wakeup(&ctx->cam->debug_exception_waitq);
			cancel_work_sync(&s_data->dbg_exception_work.work);
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):seq(%d): cancel dbg_exception_work\n",
				 __func__, req->req.debug_str, pipe_id,
				 s_data->frame_seq_no);
		}
		atomic_set(&s_data->dbg_exception_work.state, MTK_CAM_REQ_DBGWORK_S_FINISHED);

		if (atomic_read(&s_data->dbg_work.state) ==
			MTK_CAM_REQ_DBGWORK_S_PREPARED) {
			cancel_work_sync(&s_data->dbg_work.work);
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):seq(%d): cancel dbg_work\n",
				 __func__, req->req.debug_str, pipe_id,
				 s_data->frame_seq_no);
		}
		atomic_set(&s_data->dbg_work.state, MTK_CAM_REQ_DBGWORK_S_FINISHED);

		spin_lock(&req->done_status_lock);
		dev_dbg(cam->dev,
			"%s:%s:pipe(%d):seq(%d):req staus before clean:done(0x%x),pipe_used(0x%x)\n",
			__func__, req->req.debug_str, pipe_id,
			s_data->frame_seq_no, req->done_status, req->pipe_used);

		need_clean_req = false;
		if (atomic_read(&req->state) == MTK_CAM_REQ_STATE_RUNNING) {
			/* mark request status to done for release */
			req->done_status |= req->pipe_used & (1 << pipe_id);
			if (req->done_status == req->pipe_used &&
			    MTK_CAM_REQ_STATE_RUNNING ==
			    atomic_cmpxchg(&req->state,
					   MTK_CAM_REQ_STATE_RUNNING,
					   MTK_CAM_REQ_STATE_DELETING))
				need_clean_req = true;
		}

		/* if being the last one, check other pipes in the ctx */
		other_pipes = 0;
		done_status = req->done_status;
		if (need_clean_req)
			other_pipes = ctx->streaming_pipe & ~(1 << pipe_id);
		spin_unlock(&req->done_status_lock);

		/**
		 * Before remove the request, flush other pipe's done work
		 * in the same ctx to make sure mtk_cam_dev_job_done finished
		 */
		if (other_pipes) {
			for (i = 0; i < MTKCAM_SUBDEV_MAX; i++) {
				if (!(1 << i & other_pipes & done_status))
					continue;

				s_data_pipe = mtk_cam_req_get_s_data(req, i, 0);
				if (!s_data_pipe)
					continue;

				/**
				 * if done_status is marked, it means the work
				 * is running or complete
				 */
				if (flush_work(&s_data->frame_done_work.work))
					dev_info(cam->dev,
						 "%s:%s:pipe(%d):seq(%d): flush pipe(%d) frame_done_work\n",
						 __func__, req->req.debug_str,
						 pipe_id, s_data_pipe->frame_seq_no,
						 i);
			}
		}

		mtk_cam_complete_sensor_hdl(s_data);
		mtk_cam_complete_raw_hdl(s_data);

		/*
		 * reset fs state, if one sensor off and another one alive,
		 * Let the req be the single sensor case.
		 */
		mutex_lock(&req->fs.op_lock);
		mtk_cam_fs_reset(&req->fs);
		mutex_unlock(&req->fs.op_lock);

		if (need_clean_req) {
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):seq(%d): remove req from running list\n",
				 __func__, req->req.debug_str, pipe_id,
				 s_data->frame_seq_no);
			atomic_set(&req->state, MTK_CAM_REQ_STATE_CLEANUP);
			spin_lock(&cam->running_job_lock);
			list_del(&req->list);
			cam->running_job_count--;
			spin_unlock(&cam->running_job_lock);
		} else {
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):seq(%d): skip remove req from running list\n",
				 __func__, req->req.debug_str, pipe_id,
				 s_data->frame_seq_no);
		}

		if (mtk_cam_s_data_set_buf_state(s_data, buf_state)) {
			if (s_data->index > 0) {
				mtk_cam_req_return_pipe_buffers(req, pipe_id,
								s_data->index);
			} else {
				/* handle vb2_buffer_done */
				if (mtk_cam_req_put(req, pipe_id))
					dev_dbg(cam->dev,
						"%s:%s:pipe(%d) return request",
						__func__, req->req.debug_str,
						pipe_id);
			}
		}
	}

	/* all bufs in this node should be returned by req */

	dev_dbg(cam->dev,
		"%s: cleanup all stream off req, streaming ctx:0x%x, streaming pipe:0x%x)\n",
		__func__, cam->streaming_ctx, cam->streaming_pipe);
}

void mtk_cam_req_get(struct mtk_cam_request *req, int pipe_id)
{
	atomic_inc(&req->ref_cnt);
}

bool mtk_cam_req_put(struct mtk_cam_request *req, int pipe_id)
{
	bool ret = false;

	if (!atomic_dec_return(&req->ref_cnt)) {
		mtk_cam_req_clean(req);
		ret = true;
	}

	/* release the pipe buf with s_data_pipe buf state */
	mtk_cam_req_return_pipe_buffers(req, pipe_id, 0);

	return ret;
}

static void config_img_in_fmt_mstream(struct mtk_cam_device *cam,
				struct mtk_cam_request_stream_data *req_stream_data,
				struct mtk_cam_video_device *node,
				const struct v4l2_format *cfg_fmt,
				int feature)
{
	struct mtkcam_ipi_img_input *in_fmt;
	int input_node;
	struct mtk_cam_ctx *ctx = req_stream_data->ctx;

	if (node->desc.dma_port != MTKCAM_IPI_RAW_IMGO &&
			node->desc.dma_port != MTKCAM_IPI_RAW_RAWI_2)
		return;

	if (mtk_cam_feature_is_m2m(feature)) {
		int exp;
		struct mtk_cam_request *req = req_stream_data->req;

		for (exp = 1; exp <= 3; exp++) {
			if (exp == 1) {
				req_stream_data =
					mtk_cam_req_get_s_data(req,
					ctx->stream_id, 1);
				input_node = MTKCAM_IPI_RAW_RAWI_2;
			} else {
				req_stream_data =
					mtk_cam_req_get_s_data(req,
					ctx->stream_id, 0);
				if (exp == 2)
					input_node = MTKCAM_IPI_RAW_RAWI_2;
				else
					input_node = MTKCAM_IPI_RAW_RAWI_6;
			}
			in_fmt = &req_stream_data->frame_params.img_ins[input_node -
				MTKCAM_IPI_RAW_RAWI_2];
			in_fmt->uid.id = input_node;
			in_fmt->uid.pipe_id = node->uid.pipe_id;
			in_fmt->fmt.format =
				mtk_cam_get_img_fmt(cfg_fmt->fmt.pix_mp.pixelformat);
			in_fmt->fmt.s.w = cfg_fmt->fmt.pix_mp.width;
			in_fmt->fmt.s.h = cfg_fmt->fmt.pix_mp.height;
			in_fmt->fmt.stride[0] =
				cfg_fmt->fmt.pix_mp.plane_fmt[0].bytesperline;
			dev_dbg(cam->dev,
				"[m-stream] ctx: %d dma_port:%d size=%dx%d, stride:%d, fmt:0x%x (0x%x/0x%x)\n",
				ctx->stream_id, input_node, in_fmt->fmt.s.w,
				in_fmt->fmt.s.h, in_fmt->fmt.stride[0],
				in_fmt->fmt.format, in_fmt->buf[0].iova,
				req_stream_data->frame_params.img_outs[0].buf[0][0].iova);
		}
	} else {
		input_node = MTKCAM_IPI_RAW_RAWI_2;
		in_fmt = &req_stream_data->frame_params.img_ins[input_node -
			MTKCAM_IPI_RAW_RAWI_2];
		in_fmt->uid.id = input_node;
		in_fmt->uid.pipe_id = node->uid.pipe_id;
		in_fmt->fmt.format =
			mtk_cam_get_img_fmt(cfg_fmt->fmt.pix_mp.pixelformat);
		in_fmt->fmt.s.w = cfg_fmt->fmt.pix_mp.width;
		in_fmt->fmt.s.h = cfg_fmt->fmt.pix_mp.height;
		in_fmt->fmt.stride[0] =
			cfg_fmt->fmt.pix_mp.plane_fmt[0].bytesperline;
		dev_dbg(cam->dev,
			"[m-stream] ctx: %d dma_port:%d size=%dx%d, stride:%d, fmt:0x%x (0x%x/0x%x)\n",
			ctx->stream_id, input_node, in_fmt->fmt.s.w,
			in_fmt->fmt.s.h, in_fmt->fmt.stride[0],
			in_fmt->fmt.format, in_fmt->buf[0].iova,
			req_stream_data->frame_params.img_outs[0].buf[0][0].iova);
	}

}

static void config_img_in_fmt_stagger(struct mtk_cam_device *cam,
				struct mtk_cam_request_stream_data *req,
				struct mtk_cam_video_device *node,
				const struct v4l2_format *cfg_fmt,
				enum hdr_scenario_id scenario,
				int feature)
{
	struct mtkcam_ipi_img_input *in_fmt;
	int input_node;
	int rawi_port_num = 0;
	int rawi_idx = 0;
	const int *ptr_rawi = NULL;
	static const int stagger_onthefly_2exp_rawi[1] = {
					MTKCAM_IPI_RAW_RAWI_2};
	static const int stagger_onthefly_3exp_rawi[2] = {
					MTKCAM_IPI_RAW_RAWI_2, MTKCAM_IPI_RAW_RAWI_3};
	static const int stagger_dcif_1exp_rawi[1] = {
					MTKCAM_IPI_RAW_RAWI_5};
	static const int stagger_dcif_2exp_rawi[2] = {
					MTKCAM_IPI_RAW_RAWI_2, MTKCAM_IPI_RAW_RAWI_5};
	static const int stagger_dcif_3exp_rawi[3] = {
					MTKCAM_IPI_RAW_RAWI_2, MTKCAM_IPI_RAW_RAWI_3,
					MTKCAM_IPI_RAW_RAWI_5};
	static const int stagger_m2m_2exp_rawi[2] =	{
					MTKCAM_IPI_RAW_RAWI_2, MTKCAM_IPI_RAW_RAWI_6};
	static const int stagger_m2m_3exp_rawi[3] = {
					MTKCAM_IPI_RAW_RAWI_2, MTKCAM_IPI_RAW_RAWI_3,
					MTKCAM_IPI_RAW_RAWI_6};

	if (node->desc.dma_port != MTKCAM_IPI_RAW_IMGO &&
		node->desc.dma_port != MTKCAM_IPI_RAW_RAWI_2)
		return;

	if (scenario == STAGGER_ON_THE_FLY) {
		if (mtk_cam_feature_is_2_exposure(feature)) {
			rawi_port_num = 1;
			ptr_rawi = stagger_onthefly_2exp_rawi;
		} else if (mtk_cam_feature_is_3_exposure(feature)) {
			rawi_port_num = 2;
			ptr_rawi = stagger_onthefly_3exp_rawi;
		}
	} else if (scenario == STAGGER_DCIF) {
		if (mtk_cam_feature_is_2_exposure(feature)) {
			rawi_port_num = 2;
			ptr_rawi = stagger_dcif_2exp_rawi;
		} else if (mtk_cam_feature_is_3_exposure(feature)) {
			rawi_port_num = 3;
			ptr_rawi = stagger_dcif_3exp_rawi;
		} else {
			rawi_port_num = 1;
			ptr_rawi = stagger_dcif_1exp_rawi;
		}
	} else {
		if (mtk_cam_feature_is_2_exposure(feature)) {
			rawi_port_num = 2;
			ptr_rawi = stagger_m2m_2exp_rawi;
		} else if (mtk_cam_feature_is_3_exposure(feature)) {
			rawi_port_num = 3;
			ptr_rawi = stagger_m2m_3exp_rawi;
		}
	}

	if (ptr_rawi == NULL)
		return;

	for (rawi_idx = 0; rawi_idx < rawi_port_num; rawi_idx++) {
		input_node = ptr_rawi[rawi_idx];
		in_fmt = &req->frame_params.img_ins[input_node - MTKCAM_IPI_RAW_RAWI_2];
		in_fmt->uid.id = input_node;
		in_fmt->uid.pipe_id = node->uid.pipe_id;
		in_fmt->fmt.format = mtk_cam_get_img_fmt(cfg_fmt->fmt.pix_mp.pixelformat);
		in_fmt->fmt.s.w = cfg_fmt->fmt.pix_mp.width;
		in_fmt->fmt.s.h = cfg_fmt->fmt.pix_mp.height;
		in_fmt->fmt.stride[0] = cfg_fmt->fmt.pix_mp.plane_fmt[0].bytesperline;

		dev_dbg(cam->dev,
		"[stagger-%d] ctx: %d dma_port:%d size=%dx%d, stride:%d, fmt:0x%x (iova:0x%x)\n",
		rawi_idx, req->ctx->stream_id, input_node, in_fmt->fmt.s.w,
		in_fmt->fmt.s.h, in_fmt->fmt.stride[0], in_fmt->fmt.format,
		in_fmt->buf[0].iova);
	}
}

static void
config_img_in_fmt_time_shared(struct mtk_cam_device *cam,
			      struct mtk_cam_request_stream_data *req,
			      struct mtk_cam_video_device *node,
			      const struct v4l2_format *cfg_fmt)
{
	struct mtkcam_ipi_img_input *in_fmt;
	struct mtk_cam_img_working_buf_entry *buf_entry;
	struct mtk_cam_ctx *ctx = req->ctx;
	int input_node;

	if (node->desc.dma_port != MTKCAM_IPI_RAW_IMGO)
		return;

	input_node = MTKCAM_IPI_RAW_RAWI_2;
	in_fmt = &req->frame_params.img_ins[input_node - MTKCAM_IPI_RAW_RAWI_2];
	node = &ctx->pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
	in_fmt->uid.id = input_node;
	in_fmt->uid.pipe_id = node->uid.pipe_id;
	in_fmt->fmt.format = mtk_cam_get_img_fmt(cfg_fmt->fmt.pix_mp.pixelformat);
	in_fmt->fmt.s.w = cfg_fmt->fmt.pix_mp.width;
	in_fmt->fmt.s.h = cfg_fmt->fmt.pix_mp.height;
	in_fmt->fmt.stride[0] = cfg_fmt->fmt.pix_mp.plane_fmt[0].bytesperline;
	/* prepare working buffer */
	buf_entry = mtk_cam_img_working_buf_get(ctx);
	if (!buf_entry) {
		dev_info(cam->dev, "%s: No img buf availablle: req:%d\n",
		__func__, req->frame_seq_no);
		WARN_ON(1);
		return;
	}

	mtk_cam_img_wbuf_set_s_data(buf_entry, req);
	/* put to processing list */
	spin_lock(&ctx->processing_img_buffer_list.lock);
	list_add_tail(&buf_entry->list_entry, &ctx->processing_img_buffer_list.list);
	ctx->processing_img_buffer_list.cnt++;
	spin_unlock(&ctx->processing_img_buffer_list.lock);
	in_fmt->buf[0].iova = buf_entry->img_buffer.iova;
	in_fmt->buf[0].size = cfg_fmt->fmt.pix_mp.plane_fmt[0].sizeimage;
	dev_info(cam->dev,
	"[%s:%d] ctx:%d dma_port:%d size=%dx%d %d, stride:%d, fmt:0x%x (iova:0x%x)\n",
	__func__, req->frame_seq_no, ctx->stream_id, input_node, in_fmt->fmt.s.w,
	in_fmt->fmt.s.h, in_fmt->buf[0].size, in_fmt->fmt.stride[0],
	in_fmt->fmt.format, in_fmt->buf[0].iova);

}
static void check_buffer_mem_saving(
		struct mtk_cam_request_stream_data *req,
		struct mtkcam_ipi_img_input *in_fmt,
		int rawi_port_num)
{
	if (req->feature.raw_feature & HDR_MEMORY_SAVING) {
		if (rawi_port_num == 2) {
			/*raw's imgo using buf from user*/
			req->frame_params.img_outs[0].buf[0][0].iova =
			req->frame_params.img_ins[0].buf[0].iova;
			/*sv's imgo using img pool buf*/
			in_fmt->buf[0].iova = 0x0;
		} else if (rawi_port_num == 1) {
			/*raw's imgo using buf from user*/
			req->frame_params.img_outs[0].buf[0][0].iova =
			req->frame_params.img_ins[0].buf[0].iova;
			/*sv's imgo using img pool buf*/
			in_fmt->buf[0].iova = 0x0;
		}
		dev_dbg(req->ctx->cam->dev, "%s: req:%d, rawi_num:%d\n",
			__func__, req->frame_seq_no, rawi_port_num);
	}
}

static void check_stagger_buffer(struct mtk_cam_device *cam,
				 struct mtk_cam_ctx *ctx,
				 struct mtk_cam_request *cam_req)
{
	struct mtkcam_ipi_img_input *in_fmt;
	struct mtk_cam_img_working_buf_entry *buf_entry;
	struct mtk_cam_video_device *node;
	struct mtk_cam_request_stream_data *s_data;
	const struct v4l2_format *cfg_fmt;
	int input_node, rawi_port_num = 0, rawi_idx = 0;
	const int *ptr_rawi = NULL;
	int feature;

	static const int stagger_onthefly_2exp_rawi[1] = {
					MTKCAM_IPI_RAW_RAWI_2};
	static const int stagger_onthefly_3exp_rawi[2] = {
					MTKCAM_IPI_RAW_RAWI_2, MTKCAM_IPI_RAW_RAWI_3};
	static const int stagger_dcif_1exp_rawi[1] = {
					MTKCAM_IPI_RAW_RAWI_5};
	static const int stagger_dcif_2exp_rawi[2] = {
					MTKCAM_IPI_RAW_RAWI_2, MTKCAM_IPI_RAW_RAWI_5};
	static const int stagger_dcif_3exp_rawi[3] = {
					MTKCAM_IPI_RAW_RAWI_2, MTKCAM_IPI_RAW_RAWI_3,
					MTKCAM_IPI_RAW_RAWI_5};

	/* check the raw pipe only */
	s_data = mtk_cam_req_get_s_data(cam_req, ctx->stream_id, 0);
	feature = s_data->feature.raw_feature;
	if (ctx->pipe->stagger_path_pending == STAGGER_ON_THE_FLY) {
		s_data->frame_params.raw_param.hardware_scenario =
			MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER;
		if (mtk_cam_feature_is_2_exposure(feature)) {
			s_data->frame_params.raw_param.exposure_num = 2;
			rawi_port_num = 1;
			ptr_rawi = stagger_onthefly_2exp_rawi;
		} else if (mtk_cam_feature_is_3_exposure(feature)) {
			s_data->frame_params.raw_param.exposure_num = 3;
			rawi_port_num = 2;
			ptr_rawi = stagger_onthefly_3exp_rawi;
		} else {
			s_data->frame_params.raw_param.hardware_scenario =
					MTKCAM_IPI_HW_PATH_ON_THE_FLY;
			s_data->frame_params.raw_param.exposure_num = 1;
			rawi_port_num = 0;
			ptr_rawi = NULL;
		}
	} else if (ctx->pipe->stagger_path_pending == STAGGER_DCIF) {
		s_data->frame_params.raw_param.hardware_scenario =
			MTKCAM_IPI_HW_PATH_OFFLINE_SRT_DCIF_STAGGER;
		if (mtk_cam_feature_is_2_exposure(feature)) {
			s_data->frame_params.raw_param.exposure_num = 2;
			rawi_port_num = 2;
			ptr_rawi = stagger_dcif_2exp_rawi;
		} else if (mtk_cam_feature_is_3_exposure(feature)) {
			s_data->frame_params.raw_param.exposure_num = 3;
			rawi_port_num = 3;
			ptr_rawi = stagger_dcif_3exp_rawi;
		} else {
			s_data->frame_params.raw_param.exposure_num = 1;
			rawi_port_num = 1;
			ptr_rawi = stagger_dcif_1exp_rawi;
		}
	}

	for (rawi_idx = 0; rawi_idx < rawi_port_num; rawi_idx++) {
		input_node = ptr_rawi[rawi_idx];
		in_fmt = &s_data->frame_params.img_ins[
					input_node - MTKCAM_IPI_RAW_RAWI_2];
		check_buffer_mem_saving(s_data, in_fmt, rawi_port_num);
		if (in_fmt->buf[0].iova == 0x0) {
			node = &ctx->pipe->vdev_nodes[
				MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];

			cfg_fmt = mtk_cam_s_data_get_vfmt(s_data, node->desc.id);

			/* workaround for raw switch */
			if (!cfg_fmt->fmt.pix_mp.pixelformat)
				cfg_fmt = &node->active_fmt;

			in_fmt->uid.id = input_node;
			in_fmt->uid.pipe_id = node->uid.pipe_id;
			in_fmt->fmt.format = mtk_cam_get_img_fmt(
					cfg_fmt->fmt.pix_mp.pixelformat);
			in_fmt->fmt.s.w = cfg_fmt->fmt.pix_mp.width;
			in_fmt->fmt.s.h = cfg_fmt->fmt.pix_mp.height;
			in_fmt->fmt.stride[0] =
				cfg_fmt->fmt.pix_mp.plane_fmt[0].bytesperline;
			/* prepare working buffer */
			buf_entry = mtk_cam_img_working_buf_get(ctx);
			if (!buf_entry) {
				dev_info(cam->dev,
					 "%s: No img buf availablle: req:%d\n",
					 __func__, s_data->frame_seq_no);
				WARN_ON(1);
				return;
			}
			mtk_cam_img_wbuf_set_s_data(buf_entry, s_data);
			/* put to processing list */
			spin_lock(&ctx->processing_img_buffer_list.lock);
			list_add_tail(&buf_entry->list_entry,
				      &ctx->processing_img_buffer_list.list);
			ctx->processing_img_buffer_list.cnt++;
			spin_unlock(&ctx->processing_img_buffer_list.lock);
			in_fmt->buf[0].iova = buf_entry->img_buffer.iova;
			finish_img_buf(s_data);
		}
		dev_dbg(cam->dev,
			"[%s:%d] ctx:%d dma_port:%d size=%dx%d, stride:%d, fmt:0x%x (in/out:0x%x/0x%x)\n",
			__func__, s_data->frame_seq_no, ctx->stream_id,
			input_node, in_fmt->fmt.s.w, in_fmt->fmt.s.h,
			in_fmt->fmt.stride[0], in_fmt->fmt.format,
			in_fmt->buf[0].iova,
			s_data->frame_params.img_outs[0].buf[0][0].iova);
	}

	switch (s_data->feature.switch_feature_type) {
	case EXPOSURE_CHANGE_NONE:
		s_data->frame_params.raw_param.previous_exposure_num =
				s_data->frame_params.raw_param.exposure_num;
		break;
	case EXPOSURE_CHANGE_3_to_2:
	case EXPOSURE_CHANGE_3_to_1:
		s_data->frame_params.raw_param.previous_exposure_num = 3;
		break;
	case EXPOSURE_CHANGE_2_to_3:
	case EXPOSURE_CHANGE_2_to_1:
		s_data->frame_params.raw_param.previous_exposure_num = 2;
		break;
	case EXPOSURE_CHANGE_1_to_3:
	case EXPOSURE_CHANGE_1_to_2:
		s_data->frame_params.raw_param.previous_exposure_num = 1;
		break;
	default:
		s_data->frame_params.raw_param.previous_exposure_num =
				s_data->frame_params.raw_param.exposure_num;
	}
}

static void check_timeshared_buffer(struct mtk_cam_device *cam,
				    struct mtk_cam_ctx *ctx,
				    struct mtk_cam_request *cam_req)
{
	struct mtkcam_ipi_img_input *in_fmt;
	struct mtk_cam_img_working_buf_entry *buf_entry;
	struct mtk_cam_video_device *node;
	struct mtk_cam_request_stream_data *req;
	const struct v4l2_format *cfg_fmt;
	int input_node;

	/* check raw pipe only */
	input_node = MTKCAM_IPI_RAW_RAWI_2;
	req = mtk_cam_req_get_s_data(cam_req, ctx->stream_id, 0);
	in_fmt = &req->frame_params.img_ins[input_node - MTKCAM_IPI_RAW_RAWI_2];

	if (in_fmt->buf[0].iova == 0x0) {
		node = &ctx->pipe->vdev_nodes[
			MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];

		cfg_fmt = mtk_cam_s_data_get_vfmt(req, node->desc.id);
		/* workaround for raw switch */
		if (!cfg_fmt->fmt.pix_mp.pixelformat)
			cfg_fmt = &node->active_fmt;

		in_fmt->uid.id = input_node;
		in_fmt->uid.pipe_id = node->uid.pipe_id;
		in_fmt->fmt.format =
			mtk_cam_get_img_fmt(cfg_fmt->fmt.pix_mp.pixelformat);
		in_fmt->fmt.s.w = cfg_fmt->fmt.pix_mp.width;
		in_fmt->fmt.s.h = cfg_fmt->fmt.pix_mp.height;
		in_fmt->fmt.stride[0] =
			cfg_fmt->fmt.pix_mp.plane_fmt[0].bytesperline;
		/* prepare working buffer */
		buf_entry = mtk_cam_img_working_buf_get(ctx);
		if (!buf_entry) {
			dev_info(cam->dev, "%s: No img buf availablle: req:%d\n",
				 __func__, req->frame_seq_no);
			WARN_ON(1);
			return;
		}
		mtk_cam_img_wbuf_set_s_data(buf_entry, req);
		/* put to processing list */
		spin_lock(&ctx->processing_img_buffer_list.lock);
		list_add_tail(&buf_entry->list_entry,
			      &ctx->processing_img_buffer_list.list);
		ctx->processing_img_buffer_list.cnt++;
		spin_unlock(&ctx->processing_img_buffer_list.lock);
		in_fmt->buf[0].iova = buf_entry->img_buffer.iova;
	}
	dev_dbg(cam->dev,
		"[%s:%d] ctx:%d dma_port:%d size=%dx%d, stride:%d, fmt:0x%x (iova:0x%x)\n",
		__func__, req->frame_seq_no, ctx->stream_id, input_node,
		in_fmt->fmt.s.w, in_fmt->fmt.s.h, in_fmt->fmt.stride[0],
		in_fmt->fmt.format, in_fmt->buf[0].iova);
}

/* FIXME: should move following to raw's implementation. */
static int config_img_in_fmt(struct mtk_cam_device *cam,
			  struct mtk_cam_video_device *node,
			  const struct v4l2_format *cfg_fmt,
			  struct mtkcam_ipi_img_input *in_fmt)
{
	/* Check output & input image size dimension */
	if (node->desc.dma_port != MTKCAM_IPI_RAW_RAWI_2) {
		dev_info(cam->dev,
			 "pipe(%d):dam_port(%d) only support MTKCAM_IPI_RAW_RAWI_2 now\n",
			 node->uid.pipe_id, node->desc.dma_port);
		return -EINVAL;
	}

	in_fmt->fmt.format =
		mtk_cam_get_img_fmt(cfg_fmt->fmt.pix_mp.pixelformat);
	if (in_fmt->fmt.format == MTKCAM_IPI_IMG_FMT_UNKNOWN) {
		dev_info(cam->dev, "pipe: %d, node:%d unknown pixel fmt:%d\n",
			 node->uid.pipe_id, node->desc.dma_port,
			 cfg_fmt->fmt.pix_mp.pixelformat);
		return -EINVAL;
	}

	in_fmt->fmt.s.w = cfg_fmt->fmt.pix_mp.width;
	in_fmt->fmt.s.h = cfg_fmt->fmt.pix_mp.height;
	in_fmt->fmt.stride[0] = cfg_fmt->fmt.pix_mp.plane_fmt[0].bytesperline;
	dev_dbg(cam->dev,
		"pipe: %d dma_port:%d size=%0dx%0d, stride:%d\n",
		node->uid.pipe_id, node->desc.dma_port, in_fmt->fmt.s.w,
		in_fmt->fmt.s.h, in_fmt->fmt.stride[0]);


	return 0;
}

/* FIXME: should move following to raw's implementation. */
static int config_img_fmt(struct mtk_cam_device *cam,
			  struct mtk_cam_video_device *node,
			  const struct v4l2_format *cfg_fmt,
			  struct mtkcam_ipi_img_output *out_fmt, int sd_width,
			  int sd_height)
{
	/* Check output & input image size dimension */
	if (node->desc.dma_port == MTKCAM_IPI_RAW_IMGO &&
	    (cfg_fmt->fmt.pix_mp.width > sd_width ||
			cfg_fmt->fmt.pix_mp.height > sd_height)) {
		dev_dbg(cam->dev, "pipe: %d cfg(%d,%d) size is larger than sensor(%d,%d)\n",
			node->uid.pipe_id, cfg_fmt->fmt.pix_mp.width, cfg_fmt->fmt.pix_mp.height,
			sd_width, sd_height);
		return -EINVAL;
	}

	out_fmt->fmt.format =
		mtk_cam_get_img_fmt(cfg_fmt->fmt.pix_mp.pixelformat);
	if (out_fmt->fmt.format == MTKCAM_IPI_IMG_FMT_UNKNOWN) {
		dev_dbg(cam->dev, "pipe: %d, node:%d unknown pixel fmt:%d\n",
			node->uid.pipe_id, node->desc.dma_port,
			cfg_fmt->fmt.pix_mp.pixelformat);
		return -EINVAL;
	}
	out_fmt->fmt.s.w = cfg_fmt->fmt.pix_mp.width;
	out_fmt->fmt.s.h = cfg_fmt->fmt.pix_mp.height;

	/* TODO: support multi-plane stride */
	out_fmt->fmt.stride[0] = cfg_fmt->fmt.pix_mp.plane_fmt[0].bytesperline;

	if (out_fmt->crop.p.x == 0 && out_fmt->crop.s.w == 0) {
		out_fmt->crop.p.x = 0;
		out_fmt->crop.p.y = 0;
		out_fmt->crop.s.w = sd_width;
		out_fmt->crop.s.h = sd_height;
	}

	dev_dbg(cam->dev,
		"pipe: %d dma_port:%d size=%0dx%0d, stride:%d, crop=%0dx%0d\n",
		node->uid.pipe_id, node->desc.dma_port, out_fmt->fmt.s.w,
		out_fmt->fmt.s.h, out_fmt->fmt.stride[0], out_fmt->crop.s.w,
		out_fmt->crop.s.h);

	return 0;
}

static int config_img_fmt_mstream(struct mtk_cam_ctx *ctx,
				struct mtk_cam_request *req,
				const struct v4l2_format *cfg_fmt,
				struct mtk_cam_video_device *node,
				int sd_width, int sd_height,
				int feature)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_request_stream_data *mstream_req_stream_data;
	struct mtkcam_ipi_img_output *mstream_first_out_fmt;
	int ret;

	mstream_req_stream_data =
		mtk_cam_req_get_s_data(req, ctx->stream_id, 1);
	mstream_req_stream_data->ctx = ctx;
	mstream_first_out_fmt = &mstream_req_stream_data->frame_params
		.img_outs[node->desc.id - MTK_RAW_SOURCE_BEGIN];
	mstream_first_out_fmt->uid.pipe_id = node->uid.pipe_id;
	mstream_first_out_fmt->uid.id =  node->desc.dma_port;
	ret = config_img_fmt(cam, node, cfg_fmt,
			     mstream_first_out_fmt, sd_width, sd_height);
	if (ret)
		return ret;

	req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
	config_img_in_fmt_mstream(cam, req_stream_data, node, cfg_fmt, feature);

	return ret;
}

static void check_mstream_buffer(struct mtk_cam_device *cam,
				struct mtk_cam_ctx *ctx,
				struct mtk_cam_request *req)
{
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtkcam_ipi_frame_param *frame_param;
	struct mtk_cam_request_stream_data *req_stream_data_mstream;
	struct mtkcam_ipi_frame_param *mstream_frame_param;
	struct mtkcam_ipi_img_input *in_fmt;
	struct mtk_raw_pipeline *pipe = ctx->pipe;
	struct mtk_cam_video_device *vdev;
	unsigned int desc_id;
	unsigned int pipe_id;
	int in_node;
	int is_m2m = 0;
	int feature;

	desc_id = MTKCAM_IPI_RAW_IMGO - MTK_RAW_RAWI_2_IN;
	in_node = MTKCAM_IPI_RAW_RAWI_2;
	pipe_id = ctx->stream_id;

	req_stream_data = mtk_cam_req_get_s_data(req, pipe_id, 0);
	frame_param = &req_stream_data->frame_params;
	in_fmt = &frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2];

	/* support stt-only case in which no imgo is involved */
	if (in_fmt->buf[0].iova == 0x0) {
		struct mtk_cam_img_working_buf_entry *buf_entry;
		const struct v4l2_format *cfg_fmt;
		struct mtkcam_ipi_img_output *out_fmt;
		feature = req_stream_data->feature.raw_feature &
				MTK_CAM_FEATURE_HDR_MASK;
		is_m2m = mtk_cam_feature_is_mstream_m2m(
				req_stream_data->feature.raw_feature);

		/* prepare working buffer */
		buf_entry = mtk_cam_img_working_buf_get(ctx);
		if (!buf_entry) {
			dev_info(cam->dev, "%s: No img buf availablle: req:%d\n",
			__func__, req_stream_data->frame_seq_no);
			WARN_ON(1);
			return;
		}

		/* config format */
		vdev = &pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
		cfg_fmt = mtk_cam_s_data_get_vfmt(req_stream_data, MTK_RAW_MAIN_STREAM_OUT);
		/* workaround for raw switch */
		if (!cfg_fmt->fmt.pix_mp.pixelformat)
			cfg_fmt = &vdev->active_fmt;
		config_img_fmt_mstream(ctx, req, cfg_fmt, vdev,
				       cfg_fmt->fmt.pix_mp.width,
				       cfg_fmt->fmt.pix_mp.height, feature);

		/* fill mstream frame param data */
		req_stream_data_mstream = mtk_cam_req_get_s_data(req, pipe_id, 1);
		mstream_frame_param = &req_stream_data_mstream->frame_params;

		mstream_frame_param->raw_param.exposure_num = 1;
		frame_param->raw_param.exposure_num = 2;

		out_fmt = &mstream_frame_param->img_outs[desc_id];
		out_fmt->buf[0][0].iova = buf_entry->img_buffer.iova;
		in_fmt->buf[0].iova = out_fmt->buf[0][0].iova;
		out_fmt->buf[0][0].size =
			vdev->active_fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
		in_fmt->buf[0].size = out_fmt->buf[0][0].size;

		/* reuse padding img working buffer to reduce memory use */
		mtk_cam_img_working_buf_put(buf_entry);

		if (feature == MSTREAM_NE_SE) {
			frame_param->raw_param.hardware_scenario =
				is_m2m ? MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER :
			MTKCAM_IPI_HW_PATH_ON_THE_FLY_MSTREAM_NE_SE;

			dev_dbg(cam->dev,
				"%s: mstream (m2m %d) ne_se ne imgo:0x%x se rawi:0x%x\n",
				__func__, is_m2m, out_fmt->buf[0][0].iova,
				in_fmt->buf[0].iova);
		} else {
			frame_param->raw_param.hardware_scenario =
				is_m2m ? MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER :
			MTKCAM_IPI_HW_PATH_ON_THE_FLY_MSTREAM_SE_NE;

			dev_dbg(cam->dev,
				"%s: mstream (m2m %d) se_ne se imgo:0x%x ne rawi:0x%x\n",
				__func__, is_m2m, out_fmt->buf[0][0].iova,
				in_fmt->buf[0].iova);
		}
	}
}

static void mtk_cam_req_set_vfmt(struct mtk_cam_device *cam,
				 struct mtk_raw_pipeline *raw_pipeline,
				 struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_video_device *node;
	struct mtk_cam_request *req;
	struct v4l2_format *f;
	struct v4l2_selection *s;
	int i;

	req = mtk_cam_s_data_get_req(s_data);

	/* force update format to every video device before re-streamon */
	for (i = MTK_RAW_SINK_NUM + 1; i < MTK_RAW_META_OUT_BEGIN; i++) {
		node = &raw_pipeline->vdev_nodes[i - MTK_RAW_SINK_NUM];
		f = mtk_cam_s_data_get_vfmt(s_data, node->desc.id);
		if (!f) {
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):%s: can't find the vfmt field to save\n",
				 __func__, req->req.debug_str,
				 node->uid.pipe_id, node->desc.name);
			} else {
				if (s_data->vdev_fmt_update &
				    (1 << node->desc.id)) {
					mtk_cam_video_set_fmt(node, f,
							      s_data->feature.raw_feature);
					node->active_fmt = *f;
					dev_dbg(cam->dev,
						"%s:%s:pipe(%d):%s:apply pending v4l2 fmt: pixelfmt(0x%x), w(%d), h(%d)\n",
						__func__, req->req.debug_str,
						node->uid.pipe_id, node->desc.name,
						f->fmt.pix_mp.pixelformat,
						f->fmt.pix_mp.width,
						f->fmt.pix_mp.height);

				} else {
					*f = node->active_fmt;
					dev_dbg(cam->dev,
						"%s:%s:pipe(%d):%s:save v4l2 fmt: pixelfmt(0x%x), w(%d), h(%d)\n",
						__func__, req->req.debug_str,
						node->uid.pipe_id, node->desc.name,
						f->fmt.pix_mp.pixelformat,
						f->fmt.pix_mp.width,
						f->fmt.pix_mp.height);
				}
			}

			s = mtk_cam_s_data_get_vsel(s_data, node->desc.id);
			if (!s) {
				dev_info(cam->dev,
					 "%s:%s:pipe(%d):%s: can't find the vsel field to save\n",
					 __func__, req->req.debug_str,
					 node->uid.pipe_id,
					 node->desc.name);
			} else {
				if (s_data->vdev_selection_update &
				    (1 << node->desc.id)) {
					node->pending_crop = *s;
					dev_dbg(cam->dev,
						"%s:%s:pipe(%d):%s: apply pending vidioc_s_selection (%d,%d,%d,%d)\n",
						__func__, req->req.debug_str,
						node->uid.pipe_id, node->desc.name,
						node->pending_crop.r.left,
						node->pending_crop.r.top,
						node->pending_crop.r.width,
						node->pending_crop.r.height);
				} else {
					*s = node->pending_crop;
			}
		}
	}
}

static int mtk_cam_req_set_fmt(struct mtk_cam_device *cam,
			       struct mtk_cam_request *req)
{
	int pipe_id;
	int pad;
	struct mtk_cam_request_stream_data *stream_data;
	struct v4l2_subdev *sd;
	struct mtk_raw_pipeline *raw_pipeline;
	int w, h;

	dev_dbg(cam->dev, "%s:%s\n", __func__, req->req.debug_str);

	for (pipe_id = 0; pipe_id < cam->max_stream_num; pipe_id++) {
		if (req->pipe_used & (1 << pipe_id)) {
			if (is_raw_subdev(pipe_id)) {
				stream_data = mtk_cam_req_get_s_data(req, pipe_id, 0);
				raw_pipeline = &cam->raw.pipelines[pipe_id];
				mtk_cam_req_set_vfmt(cam, raw_pipeline,
						     stream_data);

				sd = &raw_pipeline->subdev;
				pad = MTK_RAW_SINK;

				if (stream_data->pad_fmt_update & 1 << MTK_RAW_SINK) {
					/* update stream_data flag */
					w = raw_pipeline->cfg[MTK_RAW_SINK].mbus_fmt.width;
					h = raw_pipeline->cfg[MTK_RAW_SINK].mbus_fmt.height;
					if (w != stream_data->pad_fmt[pad].format.width ||
						h != stream_data->pad_fmt[pad].format.height) {
						dev_info(cam->dev,
							 "%s:%s:pipe(%d):seq(%d):sink fmt change: (%d, %d) --> (%d, %d)\n",
							 __func__, req->req.debug_str, pipe_id,
							 stream_data->frame_seq_no,
							 w, h,
							 stream_data->pad_fmt[pad].format.width,
							 stream_data->pad_fmt[pad].format.height);
						stream_data->flags |=
							MTK_CAM_REQ_S_DATA_FLAG_SINK_FMT_UPDATE;
					}

					if (stream_data->flags &
						MTK_CAM_REQ_S_DATA_FLAG_SENSOR_MODE_UPDATE_T1) {
						stream_data->seninf_fmt.format =
							stream_data->pad_fmt[pad].format;
						dev_info(cam->dev,
							 "%s:%s:pipe(%d):seq(%d):pending sensor/seninf fmt change: (%d, %d, 0x%x)\n",
							 __func__, req->req.debug_str, pipe_id,
							 stream_data->frame_seq_no,
							 stream_data->seninf_fmt.format.width,
							 stream_data->seninf_fmt.format.height,
							 stream_data->seninf_fmt.format.code);
					}
				}

				/* Set MEDIA_PAD_FL_SINK pad's fmt */
				for (pad = MTK_RAW_SINK_BEGIN;
					 pad < MTK_RAW_SOURCE_BEGIN; pad++) {
					if (stream_data->pad_fmt_update & 1 << pad) {
						mtk_raw_call_pending_set_fmt
							(sd,
							 &stream_data->pad_fmt[pad]);
					} else {
						stream_data->pad_fmt[pad].format =
							raw_pipeline->cfg[pad].mbus_fmt;
					}
				}

				/* Set MEDIA_PAD_FL_SOURCE pad's fmt */
				for (pad = MTK_RAW_SOURCE_BEGIN;
					 pad < MTK_RAW_PIPELINE_PADS_NUM; pad++) {
					if (stream_data->pad_fmt_update & 1 << pad) {
						mtk_raw_call_pending_set_fmt
							(sd,
							 &stream_data->pad_fmt[pad]);
					} else {
						stream_data->pad_fmt[pad].format =
							raw_pipeline->cfg[pad].mbus_fmt;
					}
				}
			} else if (is_camsv_subdev(pipe_id)) {
				stream_data = mtk_cam_req_get_s_data(req, pipe_id, 0);
				sd = &cam->sv.pipelines[pipe_id - MTKCAM_SUBDEV_CAMSV_START].subdev;
				for (pad = MTK_CAMSV_SOURCE_BEGIN;
					 pad < MTK_CAMSV_PIPELINE_PADS_NUM; pad++)
					if (stream_data->pad_fmt_update & (1 << pad))
						mtk_camsv_call_pending_set_fmt
							(sd,
							 &stream_data->pad_fmt[pad]);
			} else if (is_mraw_subdev(pipe_id)) {
				stream_data = mtk_cam_req_get_s_data(req, pipe_id, 0);
				sd = &cam->mraw.pipelines[
					pipe_id - MTKCAM_SUBDEV_MRAW_START].subdev;
				pad = MTK_MRAW_SINK;
				if (stream_data->pad_fmt_update & (1 << pad)) {
					mtk_mraw_call_pending_set_fmt
						(sd,
						 &stream_data->pad_fmt[pad]);
					mtk_cam_mraw_update_param(&stream_data->frame_params,
						&cam->mraw.pipelines[pipe_id -
								     MTKCAM_SUBDEV_MRAW_START]);
					mtk_cam_mraw_cal_cfg_info(cam, pipe_id, stream_data, 1);
				}
			}
		}
	}

	return 0;
}

static int mtk_cam_req_update_ctrl(struct mtk_raw_pipeline *raw_pipe,
				   struct mtk_cam_request_stream_data *s_data)
{
	s64 raw_fut_pre;
	char *debug_str = mtk_cam_s_data_get_dbg_str(s_data);
	struct mtk_cam_request *req;
	struct mtk_cam_req_raw_pipe_data *raw_pipe_data;

	raw_pipe_data = mtk_cam_s_data_get_raw_pipe_data(s_data);
	req = mtk_cam_s_data_get_req(s_data);

	/* clear seamless switch mode */
	raw_pipe->sensor_mode_update = 0;
	raw_fut_pre = raw_pipe->feature_pending;
	mtk_cam_req_ctrl_setup(raw_pipe, req);

	/* use raw_res.feature as raw_fut_pre (feature setup before re-streamon)*/
	if (req->ctx_link_update & (1 << raw_pipe->id)) {
		raw_fut_pre = raw_pipe->feature_pending;
		dev_info(raw_pipe->subdev.v4l2_dev->dev,
			"%s:%s:%s:linkupdate: raw_feature(0x%0x), prev_feature(0x%0x), res feature(0x%0x)\n",
			__func__, raw_pipe->subdev.name, debug_str,
			raw_pipe->feature_pending,
			raw_fut_pre,
			raw_pipe->user_res.raw_res.feature);
	}

	s_data->feature.switch_feature_type =
		mtk_cam_get_feature_switch(raw_pipe, raw_fut_pre);
	s_data->feature.raw_feature = raw_pipe->feature_pending;
	s_data->feature.prev_feature = raw_fut_pre;
	atomic_set(&s_data->first_setting_check, 0);
	if (s_data->feature.switch_feature_type) {
		s_data->feature.switch_prev_frame_done = 0;
		s_data->feature.switch_curr_setting_done = 0;
		s_data->feature.switch_done = 0;
	}

	dev_dbg(raw_pipe->subdev.v4l2_dev->dev,
		"%s:%s:%s:raw_feature(0x%0x), prev_feature(0x%0x), switch_feature_type(0x%0x), sensor_mode_update(0x%0x)\n",
		__func__, raw_pipe->subdev.name, debug_str,
		s_data->feature.raw_feature,
		s_data->feature.prev_feature,
		s_data->feature.switch_feature_type,
		raw_pipe->sensor_mode_update);

	if (raw_pipe->sensor_mode_update)
		s_data->flags |= MTK_CAM_REQ_S_DATA_FLAG_SENSOR_MODE_UPDATE_T1;

	if (raw_pipe_data)
		raw_pipe_data->res = raw_pipe->user_res;
	mtk_cam_tg_flash_req_update(raw_pipe, s_data);

	return 0;
}

static void mtk_cam_update_s_data_exp(struct mtk_cam_ctx *ctx,
					struct mtk_cam_request *req,
					int raw_feature,
					struct mtk_cam_mstream_exposure *exp)
{
	struct mtk_cam_request_stream_data *req_stream_data_1st =
		mtk_cam_req_get_s_data(req, ctx->stream_id, 1);
	struct mtk_cam_request_stream_data *req_stream_data_2nd =
		mtk_cam_req_get_s_data(req, ctx->stream_id, 0);

	if (!exp->valid)
		return;

	if (raw_feature == MSTREAM_NE_SE) {
		req_stream_data_1st->mtk_cam_exposure = exp->exposure[0];
		req_stream_data_2nd->mtk_cam_exposure = exp->exposure[1];
	} else {
		req_stream_data_2nd->mtk_cam_exposure = exp->exposure[0];
		req_stream_data_1st->mtk_cam_exposure = exp->exposure[1];
	}

	exp->valid = 0;

	dev_dbg(ctx->cam->dev,
		"update mstream(%d) exposure 1st:%d 2nd:%d gain 1st:%d 2nd:%d\n",
		raw_feature,
		req_stream_data_1st->mtk_cam_exposure.shutter,
		req_stream_data_2nd->mtk_cam_exposure.shutter,
		req_stream_data_1st->mtk_cam_exposure.gain,
		req_stream_data_2nd->mtk_cam_exposure.gain);
}

int mtk_cam_fill_img_buf(struct mtkcam_ipi_img_output *img_out,
						const struct v4l2_format *f, dma_addr_t daddr)
{
	u32 pixelformat = f->fmt.pix_mp.pixelformat;
	u32 width = f->fmt.pix_mp.width;
	u32 height = f->fmt.pix_mp.height;
	const struct v4l2_plane_pix_format *plane = &f->fmt.pix_mp.plane_fmt[0];
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
			if (is_yuv_ufo(pixelformat)) {
				aligned_width = ALIGN(width, 64);
				img_out->buf[0][0].iova = daddr;
				img_out->fmt.stride[0] = aligned_width * info->bit_r_num
							 / info->bit_r_den;
				img_out->buf[0][0].size = img_out->fmt.stride[0] * height;
				img_out->buf[0][0].size += img_out->fmt.stride[0] * height / 2;
				img_out->buf[0][0].size += ALIGN((aligned_width / 64), 8) * height;
				img_out->buf[0][0].size += ALIGN((aligned_width / 64), 8) * height
							   / 2;
				img_out->buf[0][0].size += sizeof(struct UfbcBufferHeader);

				pr_debug("plane:%d stride:%d plane_size:%d addr:0x%x\n",
					0, img_out->fmt.stride[0], img_out->buf[0][0].size,
					img_out->buf[0][0].iova);
			} else if (is_raw_ufo(pixelformat)) {
				aligned_width = ALIGN(width, 64);
				img_out->buf[0][0].iova = daddr;
				img_out->fmt.stride[0] = aligned_width * info->bit_r_num /
							 info->bit_r_den;
				img_out->buf[0][0].size = img_out->fmt.stride[0] * height;
				img_out->buf[0][0].size += ALIGN((aligned_width / 64), 8) * height;
				img_out->buf[0][0].size += sizeof(struct UfbcBufferHeader);

				pr_debug("plane:%d stride:%d plane_size:%d addr:0x%x\n",
					0, img_out->fmt.stride[0], img_out->buf[0][0].size,
					img_out->buf[0][0].iova);
			} else {
				for (i = 0; i < info->comp_planes; i++) {
					unsigned int hdiv = (i == 0) ? 1 : info->hdiv;
					unsigned int vdiv = (i == 0) ? 1 : info->vdiv;

					img_out->buf[0][i].iova = daddr + addr_offset;
					img_out->fmt.stride[i] = info->bpp[i] *
						DIV_ROUND_UP(aligned_width, hdiv);
					img_out->buf[0][i].size = img_out->fmt.stride[i]
						* DIV_ROUND_UP(height, vdiv);
					addr_offset += img_out->buf[0][i].size;
					pr_debug("plane:%d stride:%d plane_size:%d addr:0x%x\n",
						i, img_out->fmt.stride[i], img_out->buf[0][i].size,
						img_out->buf[0][i].iova);
				}
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

				img_out->buf[0][i].iova = daddr + addr_offset;
				img_out->fmt.stride[i] = info->bpp[i] *
					DIV_ROUND_UP(aligned_width, hdiv);
				img_out->buf[0][i].size = img_out->fmt.stride[i]
					* DIV_ROUND_UP(height, vdiv);
				addr_offset += img_out->buf[0][i].size;
				pr_debug("stride:%d plane_size:%d addr:0x%x\n",
					img_out->fmt.stride[i], img_out->buf[0][i].size,
					img_out->buf[0][i].iova);
			}
		} else {
			pr_debug("do not support non contiguous mplane\n");
		}
	}

	return 0;
}

void mtk_cam_mstream_buf_update(struct mtk_cam_request *req,
				unsigned int pipe_id, int feature,
				unsigned int desc_id,
				int buf_plane_idx,
				struct vb2_buffer *vb,
				const struct v4l2_format *f)
{
	struct mtk_cam_request_stream_data *req_stream_data =
		mtk_cam_req_get_s_data(req, pipe_id, 0);
	struct mtkcam_ipi_frame_param *frame_param =
		&req_stream_data->frame_params;
	struct mtk_cam_request_stream_data *req_stream_data_mstream =
		mtk_cam_req_get_s_data(req, pipe_id, 1);
	struct mtkcam_ipi_frame_param *mstream_frame_param =
		&req_stream_data_mstream->frame_params;
	int i = buf_plane_idx;
	struct mtk_cam_buffer *buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);

	if (i >= 2) {
		pr_info("mstream buffer plane over 2\n");
		return;
	}

	mstream_frame_param->raw_param.exposure_num = 1;
	frame_param->raw_param.exposure_num = 2;

	if (mtk_cam_feature_is_mstream_m2m(feature)) {
		/**
		 * MSTREAM_SE_NE M2M orientation
		 * First exposure:
		 * Input = SE(RAWI2 node buffer through RAWI2),
		 * Output = SE(IMGO node buffer)
		 * hw_scenario -> MTKCAM_IPI_HW_PATH_OFFLINE_M2M
		 *
		 * Second exposure:
		 * Intput = NE(RAWI2 buffer through RAWI6) + SE(IMGO buffer through RAWI2),
		 * Output = NE(IMGO node buffer)
		 * hw_scenario -> MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER
		 */

		mstream_frame_param->raw_param.hardware_scenario =
			MTKCAM_IPI_HW_PATH_OFFLINE_M2M;
		frame_param->raw_param.hardware_scenario =
			MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER;

		if ((feature & MSTREAM_NE_SE) == MSTREAM_NE_SE) {
			if (i == 0) {
				if (node->desc.id == MTK_RAW_MAIN_STREAM_OUT) {
					int in_node;
					unsigned int desc_id =
						node->desc.id - MTK_RAW_SOURCE_BEGIN;

					// 3. 1st exp NE output
					mstream_frame_param->img_outs[desc_id].buf[0][0].iova =
						buf->daddr;
					mstream_frame_param->img_outs[desc_id].buf[0][0].size =
						f->fmt.pix_mp.plane_fmt[0].sizeimage;

					// SE output
					frame_param->img_outs[desc_id].buf[0][0].iova =
						buf->daddr + f->fmt.pix_mp.plane_fmt[0].sizeimage;
					frame_param->img_outs[desc_id].buf[0][0].size =
						f->fmt.pix_mp.plane_fmt[0].sizeimage;

					pr_debug("mstream ne_se m2m ne imgo:0x%x size:%d se imgo:0x%x size:%d\n",
					mstream_frame_param->img_outs[desc_id].buf[0][0].iova,
					mstream_frame_param->img_outs[desc_id].buf[0][0].size,
					frame_param->img_outs[desc_id].buf[0][0].iova,
					frame_param->img_outs[desc_id].buf[0][0].size);

					/**
					 * 4. 2nd exp SE Intput =
					 * SE(RAWI2 buffer through RAWI6)(step2) +
					 * NE(IMGO buffer through RAWI2)
					 */
					in_node = MTKCAM_IPI_RAW_RAWI_2;

					frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].iova = buf->daddr;
					frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].size =
						f->fmt.pix_mp.plane_fmt[i].sizeimage;

					pr_debug("mstream ne_se m2m se rawi2:0x%x size:%d\n",
						frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].
						buf[0].iova, frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].
						buf[0].size);
				} else if (node->desc.id == MTK_RAW_RAWI_2_IN) {
					// 1. 1st exp NE input
					int in_node = MTKCAM_IPI_RAW_RAWI_2;

					mstream_frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].iova = buf->daddr;
					mstream_frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].size =
						f->fmt.pix_mp.plane_fmt[0].sizeimage;

					pr_debug("mstream ne_se m2m ne rawi2:0x%x size:%d\n",
						mstream_frame_param->img_ins[
						in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].iova,
						mstream_frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].size);
				}
			} else if (i == 1) {
				if (node->desc.id == MTK_RAW_MAIN_STREAM_OUT) {
					/* main stream numplane is 1, not entering here */
				} else if (node->desc.id == MTK_RAW_RAWI_2_IN) {
					// 2. SE input
					int in_node = MTKCAM_IPI_RAW_RAWI_6;

					frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].iova = buf->daddr +
						f->fmt.pix_mp.plane_fmt[i].sizeimage;
					frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].size =
						f->fmt.pix_mp.plane_fmt[i].sizeimage;

					pr_debug("mstream ne_se m2m se rawi6:0x%x size:%d\n",
						frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].
						buf[0].iova, frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].size);
				}
			}
		} else { // MSTREAM_SE_NE
			if (i == 0) {
				if (node->desc.id == MTK_RAW_MAIN_STREAM_OUT) {
					int in_node;
					unsigned int desc_id =
						node->desc.id - MTK_RAW_SOURCE_BEGIN;


					// 3. 1st exp SE output
					// as normal 1 exposure flow
					mstream_frame_param->img_outs[desc_id].buf[0][0].iova =
						buf->daddr + f->fmt.pix_mp.plane_fmt[i].sizeimage;
					mstream_frame_param->img_outs[desc_id].buf[0][0].size =
						f->fmt.pix_mp.plane_fmt[i].sizeimage;

					// 2nd exp NE output
					frame_param->img_outs[desc_id].buf[0][0].iova = buf->daddr;
					frame_param->img_outs[desc_id].buf[0][0].size =
						f->fmt.pix_mp.plane_fmt[i].sizeimage;

					pr_debug("mstream se_ne m2m se imgo:0x%x size:%d ne imgo:0x%x size:%d\n",
					mstream_frame_param->img_outs[desc_id].buf[0][0].iova,
					mstream_frame_param->img_outs[desc_id].buf[0][0].size,
					frame_param->img_outs[desc_id].buf[0][0].iova,
					frame_param->img_outs[desc_id].buf[0][0].size);

					/**
					 * 4. 2nd exp NE Intput =
					 * NE(RAWI2 buffer through RAWI6)(step2) +
					 * SE(IMGO buffer through RAWI2)
					 */
					in_node = MTKCAM_IPI_RAW_RAWI_2;
					frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].iova =
						buf->daddr + f->fmt.pix_mp.plane_fmt[i].sizeimage;
					frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].size =
						f->fmt.pix_mp.plane_fmt[i].sizeimage;

					pr_debug("mstream se_ne m2m ne rawi2:0x%x size:%d\n",
						frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].iova,
						frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].size);
				} else if (node->desc.id == MTK_RAW_RAWI_2_IN) {
					// 1. 1st exp SE input
					int in_node = MTKCAM_IPI_RAW_RAWI_2;

					mstream_frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].iova =
						buf->daddr + f->fmt.pix_mp.plane_fmt[i].sizeimage;
					mstream_frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].size =
						f->fmt.pix_mp.plane_fmt[i].sizeimage;

					pr_debug("mstream se_ne m2m se rawi2:0x%x size:%d\n",
						mstream_frame_param->img_ins[
						in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0].iova,
						mstream_frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].size);
				}
			} else if (i == 1) {
				if (node->desc.id == MTK_RAW_MAIN_STREAM_OUT) {
					/* main stream numplane is 1, not entering here */
				} else if (node->desc.id == MTK_RAW_RAWI_2_IN) {
					// 2. 2nd exp NE input
					int in_node = MTKCAM_IPI_RAW_RAWI_6;

					frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].iova = buf->daddr;
					frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].size =
						f->fmt.pix_mp.plane_fmt[i].sizeimage;

					pr_debug("mstream se_ne m2m ne rawi6:0x%x size:%d\n",
						frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].iova,
						frame_param->img_ins[in_node -
						MTKCAM_IPI_RAW_RAWI_2].buf[0].size);
				}
			}
		}
	} else {
		if ((feature & MSTREAM_NE_SE) == MSTREAM_NE_SE)
			frame_param->raw_param.hardware_scenario =
			MTKCAM_IPI_HW_PATH_ON_THE_FLY_MSTREAM_NE_SE;
		else
			frame_param->raw_param.hardware_scenario =
			MTKCAM_IPI_HW_PATH_ON_THE_FLY_MSTREAM_SE_NE;

		// imgo mstream buffer layout is fixed plane[0]=NE, plane[1]=SE
		if ((feature & MSTREAM_NE_SE) == MSTREAM_NE_SE) {
			if (i == 0) { // normal output NE(plane[0]) first
				// as normal 1 exposure flow
				mstream_frame_param->img_outs[desc_id].buf[0][0].iova =
					buf->daddr;
				mstream_frame_param->img_outs[desc_id].buf[0][0].size =
					f->fmt.pix_mp.plane_fmt[i].sizeimage;

				pr_debug("mstream ne_se ne imgo:0x%x size:%d\n",
				mstream_frame_param->img_outs[desc_id].buf[0][0].iova,
				mstream_frame_param->img_outs[desc_id].buf[0][0].size);
			} else if (i == 1) { // then SE
				// in = NE output
				int in_node = MTKCAM_IPI_RAW_RAWI_2;

				frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
				.iova = mstream_frame_param->img_outs[desc_id].buf[0][0]
				.iova;
				frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
				.size = f->fmt.pix_mp.plane_fmt[0].sizeimage;
				// out = SE output
				frame_param->img_outs[desc_id].buf[0][0].iova =
					buf->daddr + f->fmt.pix_mp.plane_fmt[i].sizeimage;
				frame_param->img_outs[desc_id].buf[0][0].size =
					f->fmt.pix_mp.plane_fmt[i].sizeimage;
				pr_debug("mstream ne_se se rawi:0x%x size:%d, imgo:0x%x size:%d\n",
					frame_param->img_ins[in_node -
					MTKCAM_IPI_RAW_RAWI_2].buf[0].iova,
					frame_param->img_ins[in_node -
					MTKCAM_IPI_RAW_RAWI_2].buf[0].size,
					frame_param->img_outs[desc_id].buf[0][0].iova,
					frame_param->img_outs[desc_id].buf[0][0].size);
			}
		} else {
			if (i == 0) { // normal output SE(plane[1]) first
				mstream_frame_param->img_outs[desc_id].buf[0][0].iova =
					buf->daddr + f->fmt.pix_mp.plane_fmt[i].sizeimage;
				mstream_frame_param->img_outs[desc_id].buf[0][0].size =
					f->fmt.pix_mp.plane_fmt[i].sizeimage;
				pr_debug("mstream se_ne se imgo:0x%x size:%d\n",
				mstream_frame_param->img_outs[desc_id].buf[0][0].iova,
				mstream_frame_param->img_outs[desc_id].buf[0][0].size);
			} else if (i == 1) { // then NE
				// in = SE output
				int in_node = MTKCAM_IPI_RAW_RAWI_2;

				frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
				.iova = mstream_frame_param->img_outs[desc_id].buf[0][0]
				.iova;
				frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
					.size = f->fmt.pix_mp.plane_fmt[i].sizeimage;
				// out = NE out
				frame_param->img_outs[desc_id].buf[0][0].iova =
					buf->daddr;
				frame_param->img_outs[desc_id].buf[0][0].size =
					f->fmt.pix_mp.plane_fmt[i].sizeimage;
				pr_debug("mstream se_ne ne rawi:0x%x size:%d, imgo:0x%x size:%d\n",
					frame_param->img_ins[in_node -
					MTKCAM_IPI_RAW_RAWI_2].buf[0].iova,
					frame_param->img_ins[in_node -
					MTKCAM_IPI_RAW_RAWI_2].buf[0].size,
					frame_param->img_outs[desc_id].buf[0][0].iova,
					frame_param->img_outs[desc_id].buf[0][0].size);
			}
		}
	}
}

int mtk_cam_hdr_buf_update(struct vb2_buffer *vb,
		enum hdr_scenario_id scenario,
			   struct mtk_cam_request *req,
			   unsigned int pipe_id,
			   int feature, const struct v4l2_format *f)

{
	struct mtk_cam_buffer *buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	unsigned int desc_id;
	int i;
	struct mtk_cam_request_stream_data *req_stream_data =
		mtk_cam_req_get_s_data(req, pipe_id, 0);
	struct mtkcam_ipi_frame_param *frame_param =
		&req_stream_data->frame_params;

	if (scenario != STAGGER_M2M)
		desc_id = node->desc.id - MTK_RAW_SOURCE_BEGIN;
	else
		desc_id = node->desc.id - MTK_RAW_RAWI_2_IN;

	for (i = 0 ; i < vb->num_planes; i++) {
		vb->planes[i].data_offset =
			i * f->fmt.pix_mp.plane_fmt[i].sizeimage;
		if (mtk_cam_feature_is_mstream(feature) ||
				mtk_cam_feature_is_mstream_m2m(feature)) {
			mtk_cam_mstream_buf_update(req, pipe_id, feature,
						desc_id, i, vb, f);
		} else if (mtk_cam_get_sensor_exposure_num(feature) == 3) {
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
		} else if (mtk_cam_get_sensor_exposure_num(feature) == 2) {
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

/* Update raw_param.imgo_path_sel */
static void mtk_cam_config_raw_path(struct mtk_cam_request_stream_data *s_data,
				   struct mtk_cam_buffer *buf)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	struct mtk_cam_video_device *node;
	struct mtk_raw_pipeline *raw_pipline;
	struct mtkcam_ipi_frame_param *frame_param;
	struct vb2_buffer *vb;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	frame_param = &s_data->frame_params;
	vb = &buf->vbb.vb2_buf;
	node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);
	raw_pipline = mtk_cam_dev_get_raw_pipeline(cam, node->uid.pipe_id);

	if (raw_pipline->res_config.raw_path == V4L2_MTK_CAM_RAW_PATH_SELECT_BPC)
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_AFTER_BPC;
	else if (raw_pipline->res_config.raw_path == V4L2_MTK_CAM_RAW_PATH_SELECT_FUS)
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_AFTER_FUS;
	else if (raw_pipline->res_config.raw_path == V4L2_MTK_CAM_RAW_PATH_SELECT_DGN)
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_AFTER_DGN;
	else if (raw_pipline->res_config.raw_path == V4L2_MTK_CAM_RAW_PATH_SELECT_LSC)
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_AFTER_LSC;
	else if (raw_pipline->res_config.raw_path == V4L2_MTK_CAM_RAW_PATH_SELECT_LTM)
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_AFTER_LTM;
	else
		/* un-processed raw frame */
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_UNPROCESSED;

	dev_dbg(cam->dev, "%s: node:%d fd:%d idx:%d raw_path(%d) ipi imgo_path_sel(%d))\n",
		__func__, node->desc.id, buf->vbb.request_fd, buf->vbb.vb2_buf.index,
		raw_pipline->res_config.raw_path, frame_param->raw_param.imgo_path_sel);
}

/*
 * Update:
 * 1. imgo's buffer information (address and size)
 * 2. rawi's buffer information (address and size) if it is stagger or mstream case
 * 3. camsv's buffer information (address and size) if it is stagger
 */
static int mtk_cam_config_raw_img_out_imgo(struct mtk_cam_request_stream_data *s_data,
				      struct mtk_cam_buffer *buf)
{
	enum hdr_scenario_id scenario;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	struct mtk_cam_request *req;
	struct mtk_cam_video_device *node;
	struct mtkcam_ipi_frame_param *frame_param;
	struct mtkcam_ipi_img_output *img_out;
	struct vb2_buffer *vb;
	const struct v4l2_format *cfg_fmt;
	int i;
	int feature, hdr_feature;
	unsigned int pixelformat;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	feature = s_data->feature.raw_feature;
	req = mtk_cam_s_data_get_req(s_data);
	frame_param = &s_data->frame_params;
	vb = &buf->vbb.vb2_buf;
	node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);

	/* TODO: support sub-sampling multi-plane buffer */
	img_out = &frame_param->img_outs[node->desc.id - MTK_RAW_SOURCE_BEGIN];
	cfg_fmt = mtk_cam_s_data_get_vfmt(s_data, node->desc.id);
	if (!cfg_fmt) {
		dev_info(cam->dev,
			 "%s:%s:pipe(%d):%s: can't find the vfmt field to save\n",
			 __func__, req->req.debug_str, node->uid.pipe_id, node->desc.name);
		return -EINVAL;
	}
	pixelformat = cfg_fmt->fmt.pix_mp.pixelformat;
	img_out->buf[0][0].iova = buf->daddr;
	img_out->buf[0][0].ccd_fd = vb->planes[0].m.fd;
	if (is_raw_ufo(pixelformat))
		mtk_cam_fill_img_buf(img_out, cfg_fmt, buf->daddr);

	if ((feature & MTK_CAM_FEATURE_HDR_MASK) &&
		!(feature & MTK_CAM_FEATURE_OFFLINE_M2M_MASK)) {
		hdr_feature = feature & MTK_CAM_FEATURE_HDR_MASK;
		if (hdr_feature == MSTREAM_NE_SE ||
		    hdr_feature == MSTREAM_SE_NE)
			scenario = MSTREAM;
		else
			scenario = STAGGER_ON_THE_FLY;
		mtk_cam_hdr_buf_update(vb, scenario, req, node->uid.pipe_id,
				       feature, cfg_fmt);
	} else if (mtk_cam_feature_is_mstream_m2m(feature)) {
		mtk_cam_hdr_buf_update(vb, MSTREAM_M2M, req,
				       node->uid.pipe_id,
				       feature, cfg_fmt);
	}

	if (feature & MTK_CAM_FEATURE_SUBSAMPLE_MASK) {
		for (i = 0 ; i < vb->num_planes; i++) {
			vb->planes[i].data_offset =
				i * cfg_fmt->fmt.pix_mp.plane_fmt[i].sizeimage;
			img_out->buf[i][0].iova =
				buf->daddr + vb->planes[i].data_offset;
		}
		/* FOR 16 subsample ratios - FIXME */
		if (feature == HIGHFPS_16_SUBSAMPLE)
			for (i = MAX_SUBSAMPLE_PLANE_NUM; i < 16; i++)
				img_out->buf[i][0].iova =
				buf->daddr + i * cfg_fmt->fmt.pix_mp.plane_fmt[0].sizeimage;
		/* FOR 32 subsample ratios - FIXME */
		if (feature == HIGHFPS_32_SUBSAMPLE)
			for (i = MAX_SUBSAMPLE_PLANE_NUM; i < 32; i++)
				img_out->buf[i][0].iova =
				buf->daddr + i * cfg_fmt->fmt.pix_mp.plane_fmt[0].sizeimage;
	}

	return 0;
}

/* Update dmo's buffer information except imgo (address and size) */
static int mtk_cam_config_raw_img_out(struct mtk_cam_request_stream_data *s_data,
				     struct mtk_cam_buffer *buf)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	struct mtk_cam_request *req;
	struct mtk_cam_video_device *node;
	struct mtkcam_ipi_frame_param *frame_param;
	struct mtkcam_ipi_img_output *img_out;
	struct vb2_buffer *vb;
	const struct v4l2_format *cfg_fmt;

	int i, p;
	int feature;
	int comp_planes, plane;
	unsigned int offset;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	feature = s_data->feature.raw_feature;
	req = mtk_cam_s_data_get_req(s_data);
	frame_param = &s_data->frame_params;
	vb = &buf->vbb.vb2_buf;
	node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);

	/* TODO: support sub-sampling multi-plane buffer */
	img_out = &frame_param->img_outs[node->desc.id - MTK_RAW_SOURCE_BEGIN];
	cfg_fmt = mtk_cam_s_data_get_vfmt(s_data, node->desc.id);
	if (!cfg_fmt) {
		dev_info(cam->dev,
			 "%s:%s:pipe(%d):%s: can't find the vfmt field to save\n",
			 __func__, req->req.debug_str, node->uid.pipe_id, node->desc.name);
		return -EINVAL;
	}

	img_out->buf[0][0].ccd_fd = vb->planes[0].m.fd;
	mtk_cam_fill_img_buf(img_out, cfg_fmt, buf->daddr);
	if (feature & MTK_CAM_FEATURE_SUBSAMPLE_MASK) {
		comp_planes = 1;
		if (is_mtk_format(cfg_fmt->fmt.pix_mp.pixelformat)) {
			const struct mtk_format_info *mtk_info =
				mtk_format_info(cfg_fmt->fmt.pix_mp.pixelformat);
			comp_planes = mtk_info->comp_planes;
		} else {
			const struct v4l2_format_info *v4l2_info =
				v4l2_format_info(cfg_fmt->fmt.pix_mp.pixelformat);
			comp_planes = v4l2_info->comp_planes;
		}

		for (i = 1 ; i < vb->num_planes; i++) {
			offset = i * cfg_fmt->fmt.pix_mp.plane_fmt[i].sizeimage;
			vb->planes[i].data_offset = offset;

			img_out->buf[i][0].iova = buf->daddr + offset;
			img_out->buf[i][0].size = img_out->buf[0][0].size;
			for (plane = 1 ; plane < comp_planes; plane++) {
				img_out->buf[i][plane].iova =
					img_out->buf[i][plane-1].iova +
					img_out->buf[i][plane-1].size;
				img_out->buf[i][plane].size =
					img_out->buf[0][plane].size;
			}
		}

		/* FOR 16 subsample ratios - FIXME */
		if (feature == HIGHFPS_16_SUBSAMPLE) {
			for (i = MAX_SUBSAMPLE_PLANE_NUM; i < 16; i++) {
				offset = i * cfg_fmt->fmt.pix_mp.plane_fmt[0].sizeimage;
				img_out->buf[i][0].iova = buf->daddr + offset;
				img_out->buf[i][0].size = img_out->buf[0][0].size;

				for (p = 1 ; p < comp_planes; p++) {
					img_out->buf[i][p].iova =
						img_out->buf[i][p-1].iova +
						img_out->buf[i][p-1].size;
					img_out->buf[i][p].size =
						img_out->buf[0][p].size;
				}
			}
		}

		/* FOR 32 subsample ratios - FIXME */
		if (feature == HIGHFPS_32_SUBSAMPLE) {
			for (i = MAX_SUBSAMPLE_PLANE_NUM; i < 32; i++) {
				offset = i * cfg_fmt->fmt.pix_mp.plane_fmt[0].sizeimage;
				img_out->buf[i][0].iova = buf->daddr + offset;
				img_out->buf[i][0].size = img_out->buf[0][0].size;
				for (p = 1 ; p < comp_planes; p++) {
					img_out->buf[i][p].iova =
						img_out->buf[i][p-1].iova +
						img_out->buf[i][p-1].size;
					img_out->buf[i][p].size =
						img_out->buf[0][p].size;
				}
			}
		}

	}

	return 0;
}

static int
mtk_cam_config_raw_img_fmt(struct mtk_cam_request_stream_data *s_data,
			      struct mtk_cam_buffer *buf)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	struct mtk_cam_request *req;
	struct mtk_cam_video_device *node;
	struct mtkcam_ipi_frame_param *frame_param;
	struct mtkcam_ipi_img_output *img_out;
	struct vb2_buffer *vb;
	int feature;
	struct v4l2_mbus_framefmt *pfmt;
	int sd_width, sd_height, ret;
	const struct v4l2_format *cfg_fmt;
	struct v4l2_selection *cfg_selection;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	feature = s_data->feature.raw_feature;
	req = mtk_cam_s_data_get_req(s_data);
	frame_param = &s_data->frame_params;
	vb = &buf->vbb.vb2_buf;
	node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);

	/* TODO: support sub-sampling multi-plane buffer */
	img_out = &frame_param->img_outs[node->desc.id - MTK_RAW_SOURCE_BEGIN];
	cfg_fmt = mtk_cam_s_data_get_vfmt(s_data, node->desc.id);
	if (!cfg_fmt) {
		dev_info(cam->dev,
			 "%s:%s:pipe(%d):%s: can't find the vfmt field to save\n",
			 __func__, req->req.debug_str, node->uid.pipe_id, node->desc.name);
		return -EINVAL;
	}

	cfg_selection = mtk_cam_s_data_get_vsel(s_data, node->desc.id);

	pfmt = mtk_cam_s_data_get_pfmt(s_data, MTK_RAW_SINK);
	sd_width = pfmt->width;
	sd_height = pfmt->height;

	img_out->uid.pipe_id = node->uid.pipe_id;
	img_out->uid.id =  node->desc.dma_port;

	img_out->crop.p.x = cfg_selection->r.left;
	img_out->crop.p.y = cfg_selection->r.top;
	img_out->crop.s.w = cfg_selection->r.width;
	img_out->crop.s.h = cfg_selection->r.height;

	ret = config_img_fmt(cam, node, cfg_fmt, img_out, sd_width, sd_height);
	if (ret)
		return ret;

	if (mtk_cam_feature_is_stagger(feature))
		config_img_in_fmt_stagger(cam, s_data, node,
					  cfg_fmt, ctx->pipe->stagger_path, feature);

	if (mtk_cam_feature_is_time_shared(feature))
		config_img_in_fmt_time_shared(cam, s_data, node, cfg_fmt);

	if ((mtk_cam_feature_is_mstream(feature) || mtk_cam_feature_is_mstream_m2m(feature))
	    && node->desc.dma_port == MTKCAM_IPI_RAW_IMGO) {
		ret = config_img_fmt_mstream(ctx, req,
					     cfg_fmt, node,
					     sd_width, sd_height, feature);
		if (ret)
			return ret;
	}
	return 0;
}

static int
mtk_cam_config_raw_img_in_rawi2(struct mtk_cam_request_stream_data *s_data,
			  struct mtk_cam_buffer *buf)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	struct mtk_cam_request *req;
	struct mtk_cam_video_device *node;
	struct mtkcam_ipi_frame_param *frame_param;
	struct mtkcam_ipi_img_input *in_fmt;
	struct vb2_buffer *vb;
	const struct v4l2_format *cfg_fmt;
	int feature;
	int ret;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	feature = s_data->feature.raw_feature;
	req = mtk_cam_s_data_get_req(s_data);
	frame_param = &s_data->frame_params;
	vb = &buf->vbb.vb2_buf;
	node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);
	in_fmt = &frame_param->img_ins[node->desc.id - MTK_RAW_RAWI_2_IN];

	cfg_fmt = mtk_cam_s_data_get_vfmt(s_data, node->desc.id);
	if (!cfg_fmt) {
		dev_info(cam->dev,
			 "%s:%s:pipe(%d):%s: can't find the vfmt field to save\n",
			 __func__, req->req.debug_str, node->uid.pipe_id, node->desc.name);
		return -EINVAL;
	}

	if (feature & MTK_CAM_FEATURE_OFFLINE_M2M_MASK &&
		feature & MTK_CAM_FEATURE_HDR_MASK) {
		if (mtk_cam_feature_is_stagger_m2m(feature)) {
			mtk_cam_hdr_buf_update(vb, STAGGER_M2M,
						req, node->uid.pipe_id, feature, cfg_fmt);
			frame_param->raw_param.hardware_scenario =
				MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER;
			frame_param->raw_param.exposure_num =
				mtk_cam_get_sensor_exposure_num(feature);
		} else if (mtk_cam_feature_is_mstream_m2m(feature)) {
			mtk_cam_hdr_buf_update(vb, MSTREAM_M2M,
					       req, node->uid.pipe_id,
					       feature, cfg_fmt);
		}
	} else {
		in_fmt->buf[0].iova = buf->daddr;
		frame_param->raw_param.hardware_scenario =
			MTKCAM_IPI_HW_PATH_OFFLINE_M2M;
	}

	in_fmt->uid.pipe_id = node->uid.pipe_id;
	in_fmt->uid.id = node->desc.dma_port;
	ret = config_img_in_fmt(cam, node, cfg_fmt, in_fmt);
	if (ret)
		return ret;

	if (mtk_cam_feature_is_stagger_m2m(feature))
		config_img_in_fmt_stagger(cam, s_data, node, cfg_fmt, STAGGER_M2M, feature);

	if (mtk_cam_feature_is_mstream_m2m(feature))
		config_img_in_fmt_mstream(cam, s_data, node, cfg_fmt, feature);

	return 0;
}

static int
mtk_cam_camsv_update_fparam(struct mtk_cam_request_stream_data *s_data,
			      struct mtk_cam_buffer *buf)
{
	struct mtk_cam_video_device *node;
	struct mtk_cam_ctx *ctx;
	const struct v4l2_format *cfg_fmt;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);
	cfg_fmt = mtk_cam_s_data_get_vfmt(s_data, node->desc.id);
	if (s_data->vdev_fmt_update && cfg_fmt) {
		mtk_cam_sv_cal_cfg_info(
			ctx, cfg_fmt, &s_data->sv_frame_params);
	}

	return 0;
}

static int mtk_cam_req_update(struct mtk_cam_device *cam,
			      struct mtk_cam_request *req)
{
	struct media_request_object *obj, *obj_prev;
	struct vb2_buffer *vb;
	struct mtk_cam_buffer *buf;
	struct mtk_cam_video_device *node;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_request_stream_data *req_stream_data, *req_stream_data_mstream;
	int i, ctx_cnt;
	int raw_feature;
	int res_feature;
	int ret;
	unsigned long fps;

	dev_dbg(cam->dev, "update request:%s\n", req->req.debug_str);

	mtk_cam_req_set_fmt(cam, req);

	list_for_each_entry_safe(obj, obj_prev, &req->req.objects, list) {
		if (!vb2_request_object_is_buffer(obj))
			continue;

		vb = container_of(obj, struct vb2_buffer, req_obj);
		buf = mtk_cam_vb2_buf_to_dev_buf(vb);
		node = mtk_cam_vbq_to_vdev(vb->vb2_queue);

		ctx = mtk_cam_find_ctx(cam, &node->vdev.entity);
		req->ctx_used |= 1 << ctx->stream_id;

		req_stream_data = mtk_cam_req_get_s_data(req, node->uid.pipe_id, 0);
		req_stream_data->ctx = ctx;
		req_stream_data->no_frame_done_cnt = 0;
		atomic_set(&req_stream_data->sensor_work.is_queued, 0);
		atomic_set(&req_stream_data->dbg_work.state, MTK_CAM_REQ_DBGWORK_S_INIT);
		req_stream_data->dbg_work.dump_flags = 0;
		atomic_set(&req_stream_data->dbg_exception_work.state, MTK_CAM_REQ_DBGWORK_S_INIT);
		req_stream_data->dbg_exception_work.dump_flags = 0;
		atomic_set(&req_stream_data->frame_done_work.is_queued, 0);
		req->sync_id = (ctx->used_raw_num) ? ctx->pipe->sync_id : 0;

		raw_feature = req_stream_data->feature.raw_feature;
		if (mtk_cam_feature_is_mstream(raw_feature))
			mtk_cam_update_s_data_exp(ctx, req, raw_feature,
						  &ctx->pipe->mstream_exposure);

		if (mtk_cam_feature_is_mstream(raw_feature) ||
		    mtk_cam_feature_is_mstream_m2m(raw_feature)) {
			req_stream_data_mstream = mtk_cam_req_get_s_data(req, node->uid.pipe_id, 1);
			req_stream_data_mstream->ctx = ctx;
		}

		/* TODO: AFO independent supports TWIN */
		if (ctx->used_raw_num && node->desc.id == MTK_RAW_META_OUT_1) {
			fps = ctx->pipe->user_res.sensor_res.interval.denominator /
				ctx->pipe->user_res.sensor_res.interval.numerator;

			if (ctx->pipe->res_config.raw_num_used == 1 &&
			    mtk_cam_support_AFO_independent(fps)) {
				req_stream_data->flags |= MTK_CAM_REQ_S_DATA_FLAG_META1_INDEPENDENT;
			} else {
				dev_dbg(cam->dev,
					"%s:%s: disable AFO independent, raw_num_used(%d), fps(%lu), support_AFO_independent(%d)\n",
					__func__, req->req.debug_str,
					ctx->pipe->res_config.raw_num_used, fps,
					mtk_cam_support_AFO_independent(fps));
			}
		}

		if (req_stream_data->seninf_new)
			ctx->seninf = req_stream_data->seninf_new;

		/* update buffer format */
		switch (node->desc.dma_port) {
		case MTKCAM_IPI_RAW_RAWI_2:
			ret = mtk_cam_config_raw_img_in_rawi2(req_stream_data, buf);
			if (ret)
				return ret;
			break;
		case MTKCAM_IPI_RAW_IMGO:
			mtk_cam_config_raw_path(req_stream_data, buf);
			ret = mtk_cam_config_raw_img_out_imgo(req_stream_data, buf);
			if (ret)
				return ret;

			ret = mtk_cam_config_raw_img_fmt(req_stream_data, buf);
			if (ret)
				return ret;
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
			ret = mtk_cam_config_raw_img_out(req_stream_data, buf);
			if (ret)
				return ret;

			ret = mtk_cam_config_raw_img_fmt(req_stream_data, buf);
			if (ret)
				return ret;
			break;

		case MTKCAM_IPI_CAMSV_MAIN_OUT:
			mtk_cam_camsv_update_fparam(req_stream_data, buf);
			break;
		case MTKCAM_IPI_RAW_META_STATS_CFG:
		case MTKCAM_IPI_RAW_META_STATS_0:
		case MTKCAM_IPI_RAW_META_STATS_1:
		case MTKCAM_IPI_RAW_META_STATS_2:
			break;
		default:
			/* Do nothing for the ports not related to crop settings */
			break;
		}
	}

	/* frame sync */
	/* prepare img working buf */
	ctx_cnt = 0;
	for (i = 0; i < cam->max_stream_num; i++) {
		if (!(1 << i & req->ctx_used))
			continue;

		/* TODO: user fs */
		ctx_cnt++;

		ctx = &cam->ctxs[i];
		req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		res_feature = mtk_cam_s_data_get_res_feature(req_stream_data);

		if (mtk_cam_feature_is_time_shared(res_feature))
			check_timeshared_buffer(cam, ctx, req);

		if (mtk_cam_feature_is_mstream(req_stream_data->feature.raw_feature) ||
		    mtk_cam_feature_is_mstream_m2m(req_stream_data->feature.raw_feature))
			check_mstream_buffer(cam, ctx, req);

	}

	req->fs.target = ctx_cnt > 1 ? ctx_cnt : 0;

	return 0;
}

/* Check all pipeline involved in the request are streamed on */
static int mtk_cam_dev_req_is_stream_on(struct mtk_cam_device *cam,
					struct mtk_cam_request *req)
{
	dev_dbg(cam->dev,
		"%s: pipe_used(0x%x), streaming_pipe(0x%x), req(%s)\n",
		__func__, req->pipe_used, cam->streaming_pipe,
		req->req.debug_str);
	return (req->pipe_used & cam->streaming_pipe) == req->pipe_used;
}

static void mtk_cam_req_work_init(struct mtk_cam_req_work *work,
				  struct mtk_cam_request_stream_data *s_data)
{
	work->s_data = s_data;
}

static void fill_mstream_s_data(struct mtk_cam_ctx *ctx,
				struct mtk_cam_request *req)
{
	struct mtk_cam_req_work *frame_work, *frame_done_work;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_request_stream_data *req_stream_data_mstream;
	struct mtk_cam_request_stream_data *pipe_stream_data;
	unsigned int j;

	req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
	req_stream_data_mstream =
		mtk_cam_req_get_s_data(req, ctx->stream_id, 1);

	req_stream_data_mstream->frame_seq_no = req_stream_data->frame_seq_no;
	req_stream_data->frame_seq_no =
		req_stream_data_mstream->frame_seq_no + 1;

	req_stream_data_mstream->feature.raw_feature =
		req_stream_data->feature.raw_feature;

	req_stream_data_mstream->sensor = req_stream_data->sensor;
	req_stream_data_mstream->raw_hdl_obj = req_stream_data->raw_hdl_obj;
	req_stream_data_mstream->sensor_hdl_obj = req_stream_data->sensor_hdl_obj;
	req_stream_data_mstream->flags |= req_stream_data->flags &
					  MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_EN;
	req_stream_data_mstream->flags |= req_stream_data->flags &
					  MTK_CAM_REQ_S_DATA_FLAG_RAW_HDL_EN;

	atomic_set(&ctx->enqueued_frame_seq_no, req_stream_data->frame_seq_no);

	/* sensor switch update s_data state */
	if (req->ctx_link_update & (1 << ctx->stream_id)) {
		req_stream_data->state.estate = E_STATE_READY;
		req_stream_data_mstream->state.estate = E_STATE_SENINF;
	}

	pr_info("%s: frame_seq:%d, frame_mstream_seq:%d\n",
		__func__, req_stream_data->frame_seq_no,
		req_stream_data_mstream->frame_seq_no);

	req_stream_data_mstream->ctx = ctx;
	req_stream_data_mstream->no_frame_done_cnt = 0;
	atomic_set(&req_stream_data_mstream->sensor_work.is_queued, 0);
	atomic_set(&req_stream_data_mstream->dbg_work.state,
			MTK_CAM_REQ_DBGWORK_S_INIT);
	req_stream_data_mstream->dbg_work.dump_flags = 0;
	atomic_set(&req_stream_data_mstream->dbg_exception_work.state,
			MTK_CAM_REQ_DBGWORK_S_INIT);
	req_stream_data_mstream->dbg_exception_work.dump_flags = 0;
	atomic_set(&req_stream_data_mstream->frame_done_work.is_queued, 0);

	frame_work = &req_stream_data_mstream->frame_work;
	mtk_cam_req_work_init(frame_work, req_stream_data_mstream);


	for (j = MTKCAM_SUBDEV_RAW_START ; j < MTKCAM_SUBDEV_RAW_END ; j++) {
		if (1 << j & ctx->streaming_pipe) {
			pipe_stream_data = mtk_cam_req_get_s_data(req, j, 1);
			frame_done_work = &pipe_stream_data->frame_done_work;
			mtk_cam_req_work_init(frame_done_work, pipe_stream_data);
			INIT_WORK(&frame_done_work->work, mtk_cam_frame_done_work);
		}
	}
}

static void fill_sv_mstream_s_data(struct mtk_cam_device *cam,
				struct mtk_cam_request *req, unsigned int pipe_id)
{
	struct mtk_cam_req_work *frame_done_work, *sv_work;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_request_stream_data *req_stream_data_mstream;

	req_stream_data = mtk_cam_req_get_s_data(req, pipe_id, 0);
	req_stream_data_mstream =
		mtk_cam_req_get_s_data(req, pipe_id, 1);

	/* sequence number */
	req_stream_data_mstream->frame_seq_no = req_stream_data->frame_seq_no;
	req_stream_data->frame_seq_no =
		req_stream_data_mstream->frame_seq_no + 1;
	atomic_set(&cam->ctxs[pipe_id].enqueued_frame_seq_no, req_stream_data->frame_seq_no);

	/* frame done work */
	atomic_set(&req_stream_data_mstream->frame_done_work.is_queued, 0);
	frame_done_work = &req_stream_data_mstream->frame_done_work;
	mtk_cam_req_work_init(frame_done_work, req_stream_data_mstream);
	INIT_WORK(&frame_done_work->work, mtk_cam_frame_done_work);

	/* sv work */
	sv_work = &req_stream_data_mstream->sv_work;
	mtk_cam_req_work_init(sv_work, req_stream_data_mstream);
	INIT_WORK(&sv_work->work, mtk_cam_sv_work);

	/* sv parameters */
	req_stream_data_mstream->sv_frame_params = req_stream_data->sv_frame_params;

	dev_dbg(cam->dev, "%s: pipe_id:%d, frame_seq:%d frame_mstream_seq:%d\n",
		__func__, pipe_id, req_stream_data->frame_seq_no,
		req_stream_data_mstream->frame_seq_no);
}

static void fill_mraw_mstream_s_data(struct mtk_cam_device *cam,
				struct mtk_cam_request *req, unsigned int pipe_id)
{
	struct mtk_cam_req_work *frame_done_work;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_request_stream_data *req_stream_data_mstream;

	req_stream_data = mtk_cam_req_get_s_data(req, pipe_id, 0);
	req_stream_data_mstream =
		mtk_cam_req_get_s_data(req, pipe_id, 1);

	/* sequence number */
	req_stream_data_mstream->frame_seq_no = req_stream_data->frame_seq_no;
	req_stream_data->frame_seq_no =
		req_stream_data_mstream->frame_seq_no + 1;
	atomic_set(&cam->ctxs[pipe_id].enqueued_frame_seq_no, req_stream_data->frame_seq_no);

	/* frame done work */
	atomic_set(&req_stream_data_mstream->frame_done_work.is_queued, 0);
	frame_done_work = &req_stream_data_mstream->frame_done_work;
	mtk_cam_req_work_init(frame_done_work, req_stream_data_mstream);
	INIT_WORK(&frame_done_work->work, mtk_cam_frame_done_work);

	/* mraw parameters */
	req_stream_data_mstream->frame_params.mraw_param[0] =
		req_stream_data->frame_params.mraw_param[0];

	dev_dbg(cam->dev, "%s: pipe_id:%d, frame_seq:%d frame_mstream_seq:%d\n",
		__func__, pipe_id, req_stream_data->frame_seq_no,
		req_stream_data_mstream->frame_seq_no);
}

static void
immediate_link_update_chk(struct mtk_cam_ctx *ctx, int pipe_id,
			  int running_s_data_num,
			  struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_request *req = mtk_cam_s_data_get_req(s_data);

	if (req->ctx_link_update && (!s_data->seninf_new || !s_data->seninf_old)) {
		dev_info(ctx->cam->dev,
			 "%s:req(%s):pipe(%d):seq(%d):ctx_link_update(0x%x):seninf_old(%p),seninf_new(%p) can't be null\n",
			 __func__, req->req.debug_str, pipe_id,
			 s_data->frame_seq_no, req->ctx_link_update,
			 s_data->seninf_old, s_data->seninf_new);
		req->ctx_link_update = 0;
	}

	/**
	 * If there is only one running job and it is the
	 * switching request, trigger the seninf change
	 * before set sensor.
	 */
	if (req->ctx_link_update & (1 << pipe_id)) {
		if (running_s_data_num == 1) {
			dev_info(ctx->cam->dev,
				 "%s:req(%s):pipe(%d):link change before enqueue: seq(%d), running_s_data_num(%d)\n",
				 __func__, req->req.debug_str, pipe_id,
				 s_data->frame_seq_no, running_s_data_num);
			s_data->state.estate = E_STATE_SENINF;
			req->flags |= MTK_CAM_REQ_FLAG_SENINF_IMMEDIATE_UPDATE;
		} else {
			dev_info(ctx->cam->dev,
				 "%s:req(%s):pipe(%d):link change after last p1 done: seq(%d), running_s_data_num(%d)\n",
				 __func__, req->req.debug_str, pipe_id,
				 s_data->frame_seq_no, running_s_data_num);
		}
	}
}

static void mtk_cam_req_s_data_init(struct mtk_cam_request *req,
				    int pipe_id,
				    int s_data_index)
{
	struct mtk_cam_request_stream_data *req_stream_data;

	req_stream_data = &req->p_data[pipe_id].s_data[s_data_index];
	req_stream_data->req = req;
	req_stream_data->pipe_id = pipe_id;
	req_stream_data->state.estate = E_STATE_READY;
	req_stream_data->index = s_data_index;
	atomic_set(&req_stream_data->buf_state, -1);

	/**
	 * req_stream_data->flags is cleaned by
	 * mtk_cam_req_s_data_clean () at previous job done
	 * and may by updated by qbuf before request enqueue
	 * so we don't reset it here.
	 */
	mtk_cam_req_work_init(&req_stream_data->seninf_s_fmt_work,
				  req_stream_data);
	mtk_cam_req_work_init(&req_stream_data->frame_work,
				  req_stream_data);
	mtk_cam_req_work_init(&req_stream_data->frame_done_work,
				  req_stream_data);
	mtk_cam_req_work_init(&req_stream_data->meta1_done_work,
				  req_stream_data);
	mtk_cam_req_work_init(&req_stream_data->sv_work,
				  req_stream_data);
	/**
	 * clean the param structs since we support req reinit.
	 * the mtk_cam_request may not be "zero" when it is
	 * enqueued.
	 */
	memset(&req_stream_data->frame_params, 0,
		   sizeof(req_stream_data->frame_params));
	memset(&req_stream_data->sv_frame_params, 0,
		   sizeof(req_stream_data->sv_frame_params));
	memset(&req_stream_data->mtk_cam_exposure, 0,
		   sizeof(req_stream_data->mtk_cam_exposure));

	/* generally is single exposure */
	req_stream_data->frame_params.raw_param.exposure_num = 1;

}

void mtk_cam_dev_req_try_queue(struct mtk_cam_device *cam)
{
	struct mtk_cam_ctx *ctx, *stream_ctx;
	struct mtk_cam_request *req, *req_prev;
	struct mtk_cam_request_stream_data *s_data;
	int i, s_data_flags;
	int feature_change, previous_feature;
	int enqueue_req_cnt, job_count, s_data_cnt;
	struct list_head equeue_list;
	struct v4l2_ctrl_handler *hdl;
	struct media_request_object *sensor_hdl_obj, *raw_hdl_obj, *obj;
	unsigned long flags;

	if (!cam->streaming_ctx) {
		dev_dbg(cam->dev, "streams are off\n");
		return;
	}

	INIT_LIST_HEAD(&equeue_list);

	spin_lock(&cam->running_job_lock);
	job_count = cam->running_job_count;
	spin_unlock(&cam->running_job_lock);

	/* Pick up requests which are runnable */
	enqueue_req_cnt = 0;
	spin_lock(&cam->pending_job_lock);
	list_for_each_entry_safe(req, req_prev, &cam->pending_job_list, list) {
		if (likely(mtk_cam_dev_req_is_stream_on(cam, req))) {
			if (job_count + enqueue_req_cnt >=
			    RAW_PIPELINE_NUM * MTK_CAM_MAX_RUNNING_JOBS) {
				dev_dbg(cam->dev, "jobs are full, job cnt(%d)\n",
					 job_count);
				break;
			}
			dev_dbg(cam->dev, "%s job cnt(%d), allow req_enqueue(%s)\n",
				__func__, job_count + enqueue_req_cnt, req->req.debug_str);

			enqueue_req_cnt++;
			list_del(&req->list);
			list_add_tail(&req->list, &equeue_list);
		}
	}
	spin_unlock(&cam->pending_job_lock);

	if (!enqueue_req_cnt)
		return;

	list_for_each_entry_safe(req, req_prev, &equeue_list, list) {
		for (i = 0; i < cam->max_stream_num; i++) {
			if (!(req->pipe_used & 1 << i))
				continue;

			/* Initialize ctx related s_data fields */
			ctx = &cam->ctxs[i];
			sensor_hdl_obj = NULL;
			raw_hdl_obj = NULL;
			s_data_flags = 0;

			/* Update frame_seq_no */
			s_data = mtk_cam_req_get_s_data(req, i, 0);
			s_data->frame_seq_no = atomic_inc_return(&ctx->enqueued_frame_seq_no);
			mtk_cam_req_update_seq(ctx, req,
					       ++(ctx->enqueued_request_cnt));

			if (is_camsv_subdev(i))
				stream_ctx = mtk_cam_find_ctx(cam,
					&cam->sv.pipelines[i -
					MTKCAM_SUBDEV_CAMSV_START].subdev.entity);
			else if (is_mraw_subdev(i))
				stream_ctx = mtk_cam_find_ctx(cam,
					&cam->mraw.pipelines[i -
					MTKCAM_SUBDEV_MRAW_START].subdev.entity);

			if (is_raw_subdev(i) && ctx->sensor) {
				previous_feature = ctx->pipe->feature_pending;

				s_data_cnt =
					atomic_inc_return(&ctx->running_s_data_cnt);

				immediate_link_update_chk(ctx, i, s_data_cnt,
							  s_data);

				if (!(req->ctx_link_update & (1 << i)))
					s_data->sensor = ctx->sensor;

				spin_lock_irqsave(&req->req.lock, flags);
				list_for_each_entry(obj, &req->req.objects, list) {
					if (vb2_request_object_is_buffer(obj))
						continue;

					hdl = (struct v4l2_ctrl_handler *)obj->priv;
					if (hdl == &ctx->pipe->ctrl_handler)
						raw_hdl_obj = obj;
					else if (hdl == ctx->sensor->ctrl_handler)
						sensor_hdl_obj = obj;
				}
				spin_unlock_irqrestore(&req->req.lock, flags);

				if (raw_hdl_obj) {
					s_data->flags |= MTK_CAM_REQ_S_DATA_FLAG_RAW_HDL_EN;
					s_data->raw_hdl_obj = raw_hdl_obj;
					dev_dbg(cam->dev,
						"%s:%s:ctx(%d): find pipe hdl\n",
						__func__, req->req.debug_str, i);
				}

				/* Apply raw subdev's ctrl */
				mtk_cam_req_update_ctrl(ctx->pipe, s_data);
				feature_change = s_data->feature.switch_feature_type;

				if (s_data->sensor && s_data->sensor->ctrl_handler &&
				    sensor_hdl_obj) {
					s_data->sensor_hdl_obj = sensor_hdl_obj;
					dev_dbg(cam->dev,
						"%s:%s:ctx(%d): find sensor(%s) hdl\n",
						__func__, req->req.debug_str, i,
						s_data->sensor->name);
					s_data->flags |=
						MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_EN;
				}

				s_data_flags = s_data->flags;

				/* mstream: re-init the s_data */
				if (mtk_cam_feature_change_is_mstream(feature_change) ||
						(req->ctx_link_update & (1 << ctx->pipe->id)))
					mstream_seamless_buf_update(ctx, req, i,
								    previous_feature);

				/* reload s_data */
				s_data->flags = s_data_flags;
				s_data->raw_hdl_obj = raw_hdl_obj;
				s_data->sensor_hdl_obj = sensor_hdl_obj;

				/* copy s_data content */
				if (mtk_cam_is_mstream(ctx) || mtk_cam_is_mstream_m2m(ctx))
					fill_mstream_s_data(ctx, req);
				else if (req->p_data[i].s_data_num == 2)
					dev_info(cam->dev,
						 "%s:req(%s): undefined s_data_1, raw_feature(%d)\n",
						 __func__, req->req.debug_str,
						 ctx->pipe->feature_pending);
			} else if (is_camsv_subdev(i) && i == stream_ctx->stream_id) {
				if (!(req->ctx_link_update & (1 << i)))
					s_data->sensor = stream_ctx->sensor;

				spin_lock_irqsave(&req->req.lock, flags);
				list_for_each_entry(obj, &req->req.objects, list) {
					if (vb2_request_object_is_buffer(obj))
						continue;

					hdl = (struct v4l2_ctrl_handler *)obj->priv;
					if (hdl == stream_ctx->sensor->ctrl_handler)
						sensor_hdl_obj = obj;
				}
				spin_unlock_irqrestore(&req->req.lock, flags);

				if (s_data->sensor && s_data->sensor->ctrl_handler &&
					sensor_hdl_obj) {
					s_data->sensor_hdl_obj = sensor_hdl_obj;
					dev_dbg(cam->dev,
						"%s:%s:ctx(%d): find sensor(%s) hdl\n",
						__func__, req->req.debug_str, i,
						s_data->sensor->name);
					s_data->flags |=
						MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_EN;
				}

				/* copy s_data content for mstream case */
				if (mtk_cam_is_mstream(stream_ctx)) {
					req->p_data[i].s_data_num = 2;
					mtk_cam_req_s_data_init(req, i, 1);
					fill_sv_mstream_s_data(cam, req, i);
				}
			} else if (is_camsv_subdev(i) && i != stream_ctx->stream_id) {
				/* copy s_data content for mstream case */
				if (mtk_cam_is_mstream(stream_ctx)) {
					req->p_data[i].s_data_num = 2;
					mtk_cam_req_s_data_init(req, i, 1);
					fill_sv_mstream_s_data(cam, req, i);
				}
			} else if (is_mraw_subdev(i)) {
				/* copy s_data content for mstream case */
				if (mtk_cam_is_mstream(stream_ctx)) {
					req->p_data[i].s_data_num = 2;
					mtk_cam_req_s_data_init(req, i, 1);
					fill_mraw_mstream_s_data(cam, req, i);
				}
			} else if (is_raw_subdev(i) && !ctx->sensor) {
				/* pure m2m raw ctrl handle */
				s_data_cnt =
					atomic_inc_return(&ctx->running_s_data_cnt);

				spin_lock_irqsave(&req->req.lock, flags);
				list_for_each_entry(obj, &req->req.objects, list) {
					if (vb2_request_object_is_buffer(obj))
						continue;

					hdl = (struct v4l2_ctrl_handler *)obj->priv;
					if (hdl == &ctx->pipe->ctrl_handler)
						raw_hdl_obj = obj;
				}
				spin_unlock_irqrestore(&req->req.lock, flags);

				if (raw_hdl_obj) {
					s_data->flags |= MTK_CAM_REQ_S_DATA_FLAG_RAW_HDL_EN;
					s_data->raw_hdl_obj = raw_hdl_obj;
					dev_dbg(cam->dev,
						"%s:%s:ctx(%d): find pipe hdl\n",
						__func__, req->req.debug_str, i);
				}

				/* Apply raw subdev's ctrl */
				mtk_cam_req_update_ctrl(ctx->pipe, s_data);
			}
		}

		if (mtk_cam_req_update(cam, req)) {
			dev_info(cam->dev,
				 "%s:req(%s): invalid req settings which can't be recovered\n",
				 __func__, req->req.debug_str);
			WARN_ON(1);
			return;
		}

		atomic_set(&req->state, MTK_CAM_REQ_STATE_RUNNING);
		spin_lock(&cam->running_job_lock);
		cam->running_job_count++;
		list_del(&req->list);
		list_add_tail(&req->list, &cam->running_job_list);
		spin_unlock(&cam->running_job_lock);
		mtk_cam_dev_req_enqueue(cam, req);
	}
}

static struct media_request *mtk_cam_req_alloc(struct media_device *mdev)
{
	struct mtk_cam_request *cam_req;

	cam_req = vzalloc(sizeof(*cam_req));
	spin_lock_init(&cam_req->done_status_lock);
	mutex_init(&cam_req->fs.op_lock);

	return &cam_req->req;
}

static void mtk_cam_req_free(struct media_request *req)
{
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);

	vfree(cam_req);
}

static int mtk_cam_req_chk_job_list(struct mtk_cam_device *cam,
				    struct mtk_cam_request *new_req,
				    struct list_head *job_list,
				    char *job_list_name)
{
	if (!job_list || !job_list->prev || !job_list->prev->next ||
	    !new_req) {
		dev_dbg(cam->dev,
			"%s:%s: job_list, job_list->prev, job_list->prev->next, new_req can't be NULL\n",
			__func__, job_list_name);
		return -EINVAL;
	}

	if (job_list->prev->next != job_list) {
		dev_dbg(cam->dev, "%s broken: job_list->prev->next(%p), job_list(%p), req(%s)\n",
			job_list_name, job_list->prev->next, job_list,
			new_req->req.debug_str);
		return -EINVAL;
	}

	if (&new_req->list == job_list->prev || &new_req->list == job_list) {
		dev_dbg(cam->dev, "%s job double add: req(%s)\n",
			job_list_name, new_req->req.debug_str);
		return -EINVAL;
	}

	return 0;
}

/* only init extend s_data and update s_data_num */
static void mtk_cam_req_p_data_extend_init(struct mtk_cam_request *req,
				    int pipe_id,
				    int s_data_num)
{
	int i;

	req->p_data[pipe_id].s_data_num = s_data_num;
	for (i = 1; i < s_data_num; i++)
		mtk_cam_req_s_data_init(req, pipe_id, i);
}

static void mtk_cam_req_p_data_init(struct mtk_cam_request *req,
				    int pipe_id,
				    int s_data_num)
{
	int i = 0;

	req->p_data[pipe_id].s_data_num = s_data_num;
	for (i = 0; i < s_data_num; i++)
		mtk_cam_req_s_data_init(req, pipe_id, i);
}

static inline void mtk_cam_req_cnt_init(struct mtk_cam_request *req)
{
	atomic_set(&req->ref_cnt, 0);
}

static unsigned int mtk_cam_req_get_pipe_used(struct media_request *req)
{
	/**
	 * V4L2 framework doesn't trigger q->ops->buf_queue(q, buf) when it is
	 * stream off. We have to check the used context through the request directly
	 * before streaming on.
	 */
	struct media_request_object *obj;
	unsigned int pipe_used = 0;
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);
	unsigned int i, feature;
	struct mtk_cam_device *cam =
		container_of(req->mdev, struct mtk_cam_device, media_dev);
	struct mtk_raw_pipeline *raw_pipeline;

	list_for_each_entry(obj, &req->objects, list) {
		struct vb2_buffer *vb;
		struct mtk_cam_video_device *node;

		if (!vb2_request_object_is_buffer(obj))
			continue;
		vb = container_of(obj, struct vb2_buffer, req_obj);
		node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
		pipe_used |= 1 << node->uid.pipe_id;
	}

	mtk_cam_req_cnt_init(cam_req);

	/* Initialize per pipe's stream data (without ctx)*/
	for (i = 0; i < cam->max_stream_num; i++) {
		if (pipe_used & (1 << i)) {
			/* reset feature for each pipe */
			feature = 0;
			/**
			 * By default, the s_data_num is 1;
			 * for some special feature like mstream, it is 2.
			 */
			if (is_raw_subdev(i)) {
				raw_pipeline = &cam->raw.pipelines[i - MTKCAM_SUBDEV_RAW_0];
				feature = raw_pipeline->feature_pending;
			}
			if (mtk_cam_feature_is_mstream(feature) ||
					mtk_cam_feature_is_mstream_m2m(feature))
				mtk_cam_req_p_data_init(cam_req, i, 2);
			else
				mtk_cam_req_p_data_init(cam_req, i, 1);

			mtk_cam_req_get(cam_req, i); /* pipe id */
		}
	}

	return pipe_used;
}

static void mtk_cam_req_queue(struct media_request *req)
{
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);
	struct mtk_cam_device *cam =
		container_of(req->mdev, struct mtk_cam_device, media_dev);

	/* reset done status */
	cam_req->done_status = 0;
	cam_req->pipe_used = mtk_cam_req_get_pipe_used(req);
	cam_req->flags = 0;
	cam_req->ctx_used = 0;
	mtk_cam_fs_reset(&cam_req->fs);

	MTK_CAM_TRACE_BEGIN(BASIC, "vb2_request_queue");

	/* update frame_params's dma_bufs in mtk_cam_vb2_buf_queue */
	vb2_request_queue(req);

	MTK_CAM_TRACE_END(BASIC);

	/* add to pending job list */
	spin_lock(&cam->pending_job_lock);
	if (mtk_cam_req_chk_job_list(cam, cam_req,
				     &cam->pending_job_list,
				     "pending_job_list")) {
		spin_unlock(&cam->pending_job_lock);
		return;
	}

	MTK_CAM_TRACE_BEGIN(BASIC, "req_try_queue");

	atomic_set(&cam_req->state, MTK_CAM_REQ_STATE_PENDING);
	list_add_tail(&cam_req->list, &cam->pending_job_list);
	spin_unlock(&cam->pending_job_lock);

	mutex_lock(&cam->queue_lock);
	mtk_cam_dev_req_try_queue(cam);
	mutex_unlock(&cam->queue_lock);

	MTK_CAM_TRACE_END(BASIC);
}

static int mtk_cam_link_notify(struct media_link *link, u32 flags,
			      unsigned int notification)
{
	struct media_entity *source = link->source->entity;
	struct media_entity *sink = link->sink->entity;
	struct v4l2_subdev *subdev;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	int request_fd = link->android_vendor_data1;
	struct media_request *req;
	struct mtk_cam_request *cam_req;
	struct mtk_cam_request_stream_data *stream_data;

	if (source->function != MEDIA_ENT_F_VID_IF_BRIDGE ||
		notification != MEDIA_DEV_NOTIFY_POST_LINK_CH)
		return v4l2_pipeline_link_notify(link, flags, notification);

	subdev = media_entity_to_v4l2_subdev(sink);
	cam = container_of(subdev->v4l2_dev->mdev, struct mtk_cam_device, media_dev);

	ctx = mtk_cam_find_ctx(cam, sink);
	if (!ctx || !ctx->streaming || !(flags & MEDIA_LNK_FL_ENABLED))
		return v4l2_pipeline_link_notify(link, flags, notification);

	req = media_request_get_by_fd(&cam->media_dev, request_fd);
	if (IS_ERR(req)) {
		dev_info(cam->dev, "%s:req fd(%d) is invalid\n", __func__, request_fd);
		return v4l2_pipeline_link_notify(link, flags, notification);
	}

	cam_req = to_mtk_cam_req(req);
	stream_data = mtk_cam_req_get_s_data_no_chk(cam_req, ctx->stream_id, 0);
	stream_data->seninf_old = ctx->seninf;
	stream_data->seninf_new = media_entity_to_v4l2_subdev(source);
	stream_data->sensor = mtk_cam_find_sensor(ctx, &stream_data->seninf_new->entity);
	cam_req->ctx_link_update |= 1 << ctx->stream_id;
	media_request_put(req);

	dev_dbg(cam->dev, "link_change ctx:%d, req:%s, old seninf:%s, new seninf:%s\n",
		ctx->stream_id, req->debug_str,
		stream_data->seninf_old->name,
		stream_data->seninf_new->name);

	return v4l2_pipeline_link_notify(link, flags, notification);
}

static const struct media_device_ops mtk_cam_dev_ops = {
	.link_notify = mtk_cam_link_notify,
	.req_alloc = mtk_cam_req_alloc,
	.req_free = mtk_cam_req_free,
	.req_validate = vb2_request_validate,
	.req_queue = mtk_cam_req_queue,
};

static int mtk_cam_of_rproc(struct mtk_cam_device *cam)
{
	struct device *dev = cam->dev;
	int ret;

	ret = of_property_read_u32(dev->of_node, "mediatek,ccd",
				   &cam->rproc_phandle);
	if (ret) {
		dev_dbg(dev, "fail to get rproc_phandle:%d\n", ret);
		return -EINVAL;
	}

	return 0;
}

static int get_available_sv_pipes(struct mtk_cam_device *cam,
							int hw_scen, int req_amount, int master,
							int is_trial)
{
	unsigned int i, j, k, group, exp_order;
	unsigned int idle_pipes = 0, match_cnt = 0;

	if (hw_scen == (1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER) ||
		hw_scen == (1 << MTKCAM_IPI_HW_PATH_OFFLINE_SRT_DCIF_STAGGER)) {
		for (i = 0; i < req_amount; i++) {
			group = master << CAMSV_GROUP_SHIFT;
			exp_order = 1 << (i + CAMSV_EXP_ORDER_SHIFT);
			for (j = 0; j < cam->num_camsv_drivers; j++) {
				if ((cam->sv.pipelines[j].is_occupied == 0) &&
					(cam->sv.pipelines[j].hw_cap & hw_scen) &&
					(cam->sv.pipelines[j].hw_cap & group) &&
					(cam->sv.pipelines[j].hw_cap & exp_order)) {
					match_cnt++;
					idle_pipes |= (1 << cam->sv.pipelines[j].id);
					break;
				}
			}
			if (j == cam->num_camsv_drivers) {
				idle_pipes = 0;
				match_cnt = 0;
				goto EXIT;
			}
		}
	} else if (hw_scen == (1 << MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER)) {
		for (i = 0; i < CAMSV_GROUP_AMOUNT; i++) {
			for (j = 0; j < req_amount; j++) {
				group = 1 << (i + CAMSV_GROUP_SHIFT);
				exp_order = 1 << (j + CAMSV_EXP_ORDER_SHIFT);
				for (k = 0; k < cam->num_camsv_drivers; k++) {
					if ((cam->sv.pipelines[k].is_occupied == 0) &&
						(cam->sv.pipelines[k].hw_cap & hw_scen) &&
						(cam->sv.pipelines[k].hw_cap & group) &&
						(cam->sv.pipelines[k].hw_cap & exp_order)) {
						match_cnt++;
						idle_pipes |= (1 << cam->sv.pipelines[k].id);
						break;
					}
				}
				if (k == cam->num_camsv_drivers) {
					idle_pipes = 0;
					match_cnt = 0;
					goto EXIT;
				}
			}
			if (match_cnt == req_amount)
				break;
		}
	} else {
		for (i = 0; i < cam->num_camsv_drivers; i++) {
			if ((cam->sv.pipelines[i].is_occupied == 0) &&
				(cam->sv.pipelines[i].hw_cap & hw_scen)) {
				match_cnt++;
				idle_pipes |= (1 << cam->sv.pipelines[i].id);
			}
			if (match_cnt == req_amount)
				break;
		}
		if (match_cnt < req_amount) {
			idle_pipes = 0;
			match_cnt = 0;
			goto EXIT;
		}
	}

	if (is_trial == 0) {
		for (i = 0; i < cam->num_camsv_drivers; i++) {
			if (idle_pipes & (1 << cam->sv.pipelines[i].id))
				cam->sv.pipelines[i].is_occupied = 1;
		}
	}

EXIT:
	return idle_pipes;
}

void mstream_seamless_buf_update(struct mtk_cam_ctx *ctx,
				struct mtk_cam_request *req, int pipe_id,
				int previous_feature)
{
	struct mtk_cam_request_stream_data *req_stream_data =
		mtk_cam_req_get_s_data_no_chk(req, pipe_id, 0);
	struct mtkcam_ipi_frame_param *frame_param =
		&req_stream_data->frame_params;
	struct mtk_cam_request_stream_data *req_stream_data_mstream =
		mtk_cam_req_get_s_data_no_chk(req, pipe_id, 1);
	struct mtkcam_ipi_frame_param *mstream_frame_param =
		&req_stream_data_mstream->frame_params;
	int in_node = MTKCAM_IPI_RAW_RAWI_2;
	int desc_id = MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SOURCE_BEGIN;
	int current_feature = ctx->pipe->feature_pending;
	struct mtk_cam_video_device *vdev;
	int main_stream_size;
	__u64 iova;
	__u32 ccd_fd;
	__u8 imgo_path_sel;

	vdev = &ctx->pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
	main_stream_size = vdev->active_fmt.fmt.pix_mp.plane_fmt[1].sizeimage;

	pr_info("%s cur_feature(%d) prev_feature(%d) main_stream_size(%d)",
		__func__, current_feature, previous_feature, main_stream_size);

	/* backup first because main stream buffer is already assigned */
	iova = frame_param->img_outs[desc_id].buf[0][0].iova;
	ccd_fd = frame_param->img_outs[desc_id].buf[0][0].ccd_fd;
	imgo_path_sel = frame_param->raw_param.imgo_path_sel;

	if (mtk_cam_feature_is_mstream(current_feature)) {
		/* for 1->2, 2->2 */
		/* init stream data for mstream */
		mtk_cam_req_p_data_extend_init(req, pipe_id, 2);
	} else {
		/* for 2->1 */
		/* init stream data for normal exp */
		mtk_cam_req_p_data_extend_init(req, pipe_id, 1);
	}

	/* notify scheduler timer update */
	atomic_set(&ctx->sensor_ctrl.isp_update_timer_seq_no,
			req_stream_data->frame_seq_no);

	/* recover main stream buffer */
	frame_param->img_outs[desc_id].buf[0][0].iova = iova;
	frame_param->img_outs[desc_id].buf[0][0].size = main_stream_size;
	frame_param->img_outs[desc_id].buf[0][0].ccd_fd = ccd_fd;
	frame_param->raw_param.imgo_path_sel = imgo_path_sel;

	if (current_feature == MSTREAM_NE_SE) {
		mstream_frame_param->raw_param.exposure_num = 1;
		frame_param->raw_param.exposure_num = 2;
		frame_param->raw_param.hardware_scenario =
		MTKCAM_IPI_HW_PATH_ON_THE_FLY_MSTREAM_NE_SE;
	} else if (current_feature == MSTREAM_SE_NE) {
		mstream_frame_param->raw_param.exposure_num = 1;
		frame_param->raw_param.exposure_num = 2;
		frame_param->raw_param.hardware_scenario =
		MTKCAM_IPI_HW_PATH_ON_THE_FLY_MSTREAM_SE_NE;
	} else {
		// normal single exposure
		mstream_frame_param->raw_param.exposure_num = 0;
		frame_param->raw_param.exposure_num = 1;
		frame_param->raw_param.hardware_scenario =
		MTKCAM_IPI_HW_PATH_ON_THE_FLY;
	}

	// imgo mstream buffer layout is fixed plane[0]=NE, plane[1]=SE
	if (current_feature == MSTREAM_NE_SE) {
		// Normal single exposure seamless to NE_SE
		// NE as normal 1 exposure flow, get iova from frame_param
		mstream_frame_param->img_outs[desc_id].buf[0][0].iova =
				frame_param->img_outs[desc_id].buf[0][0].iova;
		pr_debug("%s mstream ne_se ne imgo:0x%x\n",
			__func__,
			mstream_frame_param->img_outs[desc_id].buf[0][0].iova);

		// SE, in = NE output
		frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
			.iova = mstream_frame_param->img_outs[desc_id].buf[0][0].iova;
		frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
			.size = main_stream_size;
		// out = SE output
		frame_param->img_outs[desc_id].buf[0][0].iova +=
			main_stream_size;
		frame_param->img_outs[desc_id].buf[0][0].size =
			main_stream_size;
		pr_debug("%s mstream ne_se se rawi:0x%x imgo:0x%x size:%d\n",
			__func__,
			frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0].iova,
			frame_param->img_outs[desc_id].buf[0][0].iova,
			frame_param->img_outs[desc_id].buf[0][0].size);
	} else if (current_feature == MSTREAM_SE_NE) {
		// Normal single exposure seamless to NE_SE
		// SE as normal output SE(plane[1]) first
		mstream_frame_param->img_outs[desc_id].buf[0][0].iova =
			frame_param->img_outs[desc_id].buf[0][0].iova +
			main_stream_size;
		mstream_frame_param->img_outs[desc_id].buf[0][0].size =
			main_stream_size;
		pr_debug("%s mstream se_ne se imgo:0x%x size:%d\n",
			__func__,
			mstream_frame_param->img_outs[desc_id].buf[0][0].iova,
			mstream_frame_param->img_outs[desc_id].buf[0][0].size);

		// NE,  in = SE output
		frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
			.iova = mstream_frame_param->img_outs[desc_id].buf[0][0].iova;
		frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
			.size = main_stream_size;
		// out = NE out, already configured in normal single exposure

		pr_debug("%s mstream se_ne ne rawi:0x%x imgo:0x%x size:%d\n",
			__func__,
			frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0].iova,
			frame_param->img_outs[desc_id].buf[0][0].iova,
			frame_param->img_outs[desc_id].buf[0][0].size);
	} else {
		// M-Stream seamless to normal single exposure
		// clear mstream mstream_frame_param
		mstream_frame_param->img_outs[desc_id].buf[0][0].iova = 0;

		// reset frame_param to normal single exposure
		// if previous is NE_SE, recover imgo
		if (previous_feature == MSTREAM_NE_SE) {
			// get NE from rawi
			frame_param->img_outs[desc_id].buf[0][0].iova =
			frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
			.iova;
		}

		// if previous is SE_NE, imgo iova is already correct

		// clear rawi2
		frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
			.iova = 0;
		frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
			.size = 0;

		pr_debug("%s normal imgo:0x%x\n", __func__,
			frame_param->img_outs[desc_id].buf[0][0].iova);
	}
}

int get_main_sv_pipe_id(struct mtk_cam_device *cam,
							int used_dev_mask)
{
	unsigned int i;

	for (i = MTKCAM_SUBDEV_CAMSV_START; i < MTKCAM_SUBDEV_CAMSV_END; i++) {
		if (used_dev_mask & (1 << i))
			if (cam->sv.pipelines[i - MTKCAM_SUBDEV_CAMSV_START].exp_order == 0)
				return i;
	}
	return -1;
}

int get_sub_sv_pipe_id(struct mtk_cam_device *cam,
							int used_dev_mask)
{
	unsigned int i;

	for (i = MTKCAM_SUBDEV_CAMSV_START; i < MTKCAM_SUBDEV_CAMSV_END; i++) {
		if (used_dev_mask & (1 << i))
			if (cam->sv.pipelines[i - MTKCAM_SUBDEV_CAMSV_START].exp_order == 1)
				return i;
	}
	return -1;
}

int get_last_sv_pipe_id(struct mtk_cam_device *cam,
							int used_dev_mask)
{
	unsigned int i;

	for (i = MTKCAM_SUBDEV_CAMSV_START; i < MTKCAM_SUBDEV_CAMSV_END; i++) {
		if (used_dev_mask & (1 << i))
			if (cam->sv.pipelines[i - MTKCAM_SUBDEV_CAMSV_START].exp_order == 2)
				return i;
	}
	return -1;
}

struct mtk_raw_device *get_master_raw_dev(struct mtk_cam_device *cam,
					  struct mtk_raw_pipeline *pipe)
{
	struct device *dev_master;
	unsigned int i;

	for (i = 0; i < cam->num_raw_drivers; i++) {
		if (pipe->enabled_raw & (1 << i)) {
			dev_master = cam->raw.devs[i];
			break;
		}
	}

	return dev_get_drvdata(dev_master);
}

struct mtk_raw_device *get_slave_raw_dev(struct mtk_cam_device *cam,
					 struct mtk_raw_pipeline *pipe)
{
	struct device *dev_slave;
	unsigned int i;

	for (i = 0; i < cam->num_raw_drivers - 1; i++) {
		if (pipe->enabled_raw & (1 << i)) {
			dev_slave = cam->raw.devs[i + 1];
			break;
		}
	}

	return dev_get_drvdata(dev_slave);
}

struct mtk_raw_device *get_slave2_raw_dev(struct mtk_cam_device *cam,
					  struct mtk_raw_pipeline *pipe)
{
	struct device *dev_slave;
	unsigned int i;

	for (i = 0; i < cam->num_raw_drivers; i++) {
		if (pipe->enabled_raw & (1 << i)) {
			dev_slave = cam->raw.devs[i + 2];
			break;
		}
	}

	return dev_get_drvdata(dev_slave);
}

struct mtk_camsv_device *get_camsv_dev(struct mtk_cam_device *cam,
					  struct mtk_camsv_pipeline *pipe)
{
	struct device *dev;

	dev = cam->sv.devs[pipe->id - MTKCAM_SUBDEV_CAMSV_START];

	return dev_get_drvdata(dev);
}

struct mtk_mraw_device *get_mraw_dev(struct mtk_cam_device *cam,
					  struct mtk_mraw_pipeline *pipe)
{
	struct device *dev;

	dev = cam->mraw.devs[pipe->id - MTKCAM_SUBDEV_MRAW_START];

	return dev_get_drvdata(dev);
}

#if CCD_READY
static void isp_composer_uninit(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_ccd *ccd = cam->rproc_handle->priv;

	mtk_destroy_client_msgdevice(ccd->rpmsg_subdev, &ctx->rpmsg_channel);
	ctx->rpmsg_dev = NULL;
}

static int isp_composer_handle_ack(struct mtk_cam_device *cam,
				   struct mtkcam_ipi_event *ipi_msg)
{
	struct device *dev = cam->dev;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_working_buf_entry *buf_entry;
	struct mtk_mraw_working_buf_entry *mraw_buf_entry[MAX_MRAW_PIPES_PER_STREAM];
	struct mtk_cam_request_stream_data *s_data;
	struct mtk_cam_request *req;
	bool is_m2m_apply_cq = false;
	bool is_mux_change_with_apply_cq = false;
	int i;
	struct mtk_raw_device *raw_dev;
	struct mtk_mraw_device *mraw_dev;

	ctx = &cam->ctxs[ipi_msg->cookie.session_id];

	/* check if the ctx is streaming */
	spin_lock(&ctx->streaming_lock);
	if (!ctx->streaming) {
		dev_info(cam->dev,
			 "%s: skip handing composer ack for stream off ctx:%d, frame_no:%d\n",
			 __func__, ctx->stream_id, ipi_msg->cookie.frame_no);
		spin_unlock(&ctx->streaming_lock);
		return 0;
	}
	spin_unlock(&ctx->streaming_lock);

	if (mtk_cam_is_m2m(ctx)) {
		if (ipi_msg->cookie.frame_no > 1) {
			dev_dbg(dev, "[M2M] wait_for_completion +\n");
			wait_for_completion(&ctx->m2m_complete);
			dev_dbg(dev, "[M2M] wait_for_completion -\n");
		}
	}

	spin_lock(&ctx->using_buffer_list.lock);

	ctx->composed_frame_seq_no = ipi_msg->cookie.frame_no;
	for (i = 0; i < ctx->used_mraw_num; i++)
		ctx->mraw_composed_frame_seq_no[i] = ipi_msg->cookie.frame_no;
	if (mtk_cam_is_mstream(ctx)) {
		dev_dbg(dev, "[mstream] ctx:%d ack frame_num:%d\n",
			ctx->stream_id, ctx->composed_frame_seq_no);
	} else {
		dev_dbg(dev, "ctx:%d ack frame_num:%d\n",
			ctx->stream_id, ctx->composed_frame_seq_no);
	}

	/* get from using list */
	if (list_empty(&ctx->using_buffer_list.list)) {
		spin_unlock(&ctx->using_buffer_list.lock);
		return -EINVAL;
	}

	for (i = 0; i < ctx->used_mraw_num; i++) {
		spin_lock(&ctx->mraw_using_buffer_list[i].lock);
		if (list_empty(&ctx->mraw_using_buffer_list[i].list)) {
			spin_unlock(&ctx->mraw_using_buffer_list[i].lock);
			spin_unlock(&ctx->using_buffer_list.lock);
			return -EINVAL;
		}
		spin_unlock(&ctx->mraw_using_buffer_list[i].lock);
	}

	/* assign raw using buf */
	buf_entry = list_first_entry(&ctx->using_buffer_list.list,
				     struct mtk_cam_working_buf_entry,
				     list_entry);
	list_del(&buf_entry->list_entry);
	ctx->using_buffer_list.cnt--;

	s_data = mtk_cam_wbuf_get_s_data(buf_entry);
	if (!s_data) {
		dev_dbg(dev, "ctx:%d no req for ack frame_num:%d\n",
			ctx->stream_id, ctx->composed_frame_seq_no);
		spin_unlock(&ctx->using_buffer_list.lock);
		return -EINVAL;
	}

	req = mtk_cam_s_data_get_req(s_data);
	if (req->flags & MTK_CAM_REQ_FLAG_SENINF_IMMEDIATE_UPDATE &&
			(req->ctx_link_update & (1 << s_data->pipe_id))) {
		if (mtk_cam_is_mstream(ctx)) {
			struct mtk_cam_request_stream_data *mstream_1st_data;

			mstream_1st_data =  mtk_cam_req_get_s_data(req, ctx->stream_id, 1);
			if (ipi_msg->cookie.frame_no == mstream_1st_data->frame_seq_no) {
				is_mux_change_with_apply_cq = true;
				ctx->is_first_cq_done = 0;
			}
		} else {
			is_mux_change_with_apply_cq = true;
		}
	}

	buf_entry->cq_desc_offset =
		ipi_msg->ack_data.frame_result.cq_desc_offset;
	buf_entry->cq_desc_size =
		ipi_msg->ack_data.frame_result.cq_desc_size;
	buf_entry->sub_cq_desc_offset =
		ipi_msg->ack_data.frame_result.sub_cq_desc_offset;
	buf_entry->sub_cq_desc_size =
		ipi_msg->ack_data.frame_result.sub_cq_desc_size;

	/* Do nothing if the user doesn't enable force dump */
	if (mtk_cam_feature_is_mstream(s_data->feature.raw_feature) ||
			mtk_cam_feature_is_mstream_m2m(s_data->feature.raw_feature)) {
		struct mtk_cam_request_stream_data *mstream_s_data;

		mstream_s_data = mtk_cam_req_get_s_data(
							s_data->req, ctx->stream_id, 0);
		if (s_data->frame_seq_no ==
		    mstream_s_data->frame_seq_no) {
			mtk_cam_req_dump(s_data,
					 MTK_CAM_REQ_DUMP_FORCE,
					 "Camsys Force Dump", false);
		}
	} else {
		mtk_cam_req_dump(s_data,
				 MTK_CAM_REQ_DUMP_FORCE,
				 "Camsys Force Dump", false);
	}

	if (ipi_msg->ack_data.ret) {
		mtk_cam_req_dump(s_data,
			MTK_CAM_REQ_DUMP_DEQUEUE_FAILED,
			"Camsys compose error", false);
	}

	/* assign mraw using buf */
	for (i = 0; i < ctx->used_mraw_num; i++) {
		spin_lock(&ctx->mraw_using_buffer_list[i].lock);
		mraw_buf_entry[i] = list_first_entry(&ctx->mraw_using_buffer_list[i].list,
						struct mtk_mraw_working_buf_entry,
						list_entry);
		list_del(&mraw_buf_entry[i]->list_entry);
		ctx->mraw_using_buffer_list[i].cnt--;
		spin_unlock(&ctx->mraw_using_buffer_list[i].lock);
		mraw_buf_entry[i]->buffer.iova = buf_entry->buffer.iova;
		mraw_buf_entry[i]->mraw_cq_desc_offset =
			ipi_msg->ack_data.frame_result.mraw_cq_desc_offset[i];
		mraw_buf_entry[i]->mraw_cq_desc_size =
			ipi_msg->ack_data.frame_result.mraw_cq_desc_size[i];
	}

	if (mtk_cam_is_m2m(ctx)) {
		spin_lock(&ctx->composed_buffer_list.lock);
		dev_dbg(dev, "%s ctx->composed_buffer_list.cnt %d\n", __func__,
			ctx->composed_buffer_list.cnt);

		if (ctx->composed_buffer_list.cnt == 0)
			is_m2m_apply_cq = true;

		spin_unlock(&ctx->composed_buffer_list.lock);

		spin_lock(&ctx->processing_buffer_list.lock);
		dev_dbg(dev, "%s ctx->processing_buffer_list.cnt %d\n", __func__,
			ctx->processing_buffer_list.cnt);
		spin_unlock(&ctx->processing_buffer_list.lock);
	}

	if ((ctx->composed_frame_seq_no == 1 && !mtk_cam_is_time_shared(ctx)) ||
	    is_m2m_apply_cq || is_mux_change_with_apply_cq) {
		struct device *dev;
		/* apply raw CQ */
		spin_lock(&ctx->processing_buffer_list.lock);
		list_add_tail(&buf_entry->list_entry,
			      &ctx->processing_buffer_list.list);
		ctx->processing_buffer_list.cnt++;
		spin_unlock(&ctx->processing_buffer_list.lock);

		for (i = 0; i < ctx->used_mraw_num; i++) {
			spin_lock(&ctx->mraw_processing_buffer_list[i].lock);
			list_add_tail(&mraw_buf_entry[i]->list_entry,
					&ctx->mraw_processing_buffer_list[i].list);
			ctx->mraw_processing_buffer_list[i].cnt++;
			spin_unlock(&ctx->mraw_processing_buffer_list[i].lock);
		}

		spin_unlock(&ctx->using_buffer_list.lock);

		dev = mtk_cam_find_raw_dev(cam, ctx->pipe->enabled_raw);
		if (!dev) {
			dev_dbg(dev, "frm#1 raw device not found\n");
			return -EINVAL;
		}

		raw_dev = dev_get_drvdata(dev);

		if (mtk_cam_is_m2m(ctx)) {
			dev_dbg(dev, "%s M2M apply_cq, composed_buffer_list.cnt %d frame_seq_no %d\n",
				__func__, ctx->composed_buffer_list.cnt,
				s_data->frame_seq_no);
			mtk_cam_m2m_enter_cq_state(&s_data->state);
		}

		/* mmqos update */
		mtk_cam_qos_bw_calc(ctx, s_data->raw_dmas, false);

		apply_cq(raw_dev, 1, buf_entry->buffer.iova,
			 buf_entry->cq_desc_size,
			 buf_entry->cq_desc_offset,
			 buf_entry->sub_cq_desc_size,
			 buf_entry->sub_cq_desc_offset);

		if (mtk_cam_is_with_w_channel(ctx)) {
			if (mtk_cam_sv_rgbw_apply_next_buffer(buf_entry->s_data) == 0)
				dev_info(raw_dev->dev, "rgbw: sv apply next buffer failed");
		}

		if (is_mux_change_with_apply_cq)
			mtk_cam_sv_apply_switch_buffers(ctx);

		/* apply mraw CQ for all streams */
		for (i = 0; i < ctx->used_mraw_num; i++) {
			mraw_dev = get_mraw_dev(ctx->cam, ctx->mraw_pipe[i]);
			if (mraw_buf_entry[i]->s_data->req->pipe_used &
				(1 << ctx->mraw_pipe[i]->id)) {
				mraw_dev->is_enqueued = 1;
				apply_mraw_cq(mraw_dev,
					mraw_buf_entry[i]->buffer.iova,
					mraw_buf_entry[i]->mraw_cq_desc_size,
					mraw_buf_entry[i]->mraw_cq_desc_offset,
					(ctx->composed_frame_seq_no == 1) ? 1 : 0);
			} else {
				mtk_cam_mraw_vf_on(mraw_dev, 0);
			}
		}

		s_data->timestamp = ktime_get_boottime_ns();
		s_data->timestamp_mono = ktime_get_ns();

		return 0;
	}

	spin_lock(&ctx->composed_buffer_list.lock);
	list_add_tail(&buf_entry->list_entry,
		      &ctx->composed_buffer_list.list);
	ctx->composed_buffer_list.cnt++;
	if (mtk_cam_is_m2m(ctx)) {
		dev_dbg(dev, "%s M2M composed_buffer_list.cnt %d\n",
			__func__, ctx->composed_buffer_list.cnt);
	}
	spin_unlock(&ctx->composed_buffer_list.lock);

	for (i = 0; i < ctx->used_mraw_num; i++) {
		spin_lock(&ctx->mraw_composed_buffer_list[i].lock);
		list_add_tail(&mraw_buf_entry[i]->list_entry,
				&ctx->mraw_composed_buffer_list[i].list);
		ctx->mraw_composed_buffer_list[i].cnt++;
		spin_unlock(&ctx->mraw_composed_buffer_list[i].lock);
	}

	spin_unlock(&ctx->using_buffer_list.lock);

	return 0;
}

static int isp_composer_handler(struct rpmsg_device *rpdev, void *data,
				int len, void *priv, u32 src)
{
	struct mtk_cam_device *cam = (struct mtk_cam_device *)priv;
	struct device *dev = cam->dev;
	struct mtkcam_ipi_event *ipi_msg;
	struct mtk_cam_ctx *ctx;

	ipi_msg = (struct mtkcam_ipi_event *)data;
	if (!ipi_msg)
		return -EINVAL;

	if (len < offsetofend(struct mtkcam_ipi_event, ack_data)) {
		dev_dbg(dev, "wrong IPI len:%d\n", len);
		return -EINVAL;
	}

	if (ipi_msg->cmd_id != CAM_CMD_ACK ||
	    (ipi_msg->ack_data.ack_cmd_id != CAM_CMD_FRAME &&
	     ipi_msg->ack_data.ack_cmd_id != CAM_CMD_DESTROY_SESSION))
		return -EINVAL;

	if (ipi_msg->ack_data.ack_cmd_id == CAM_CMD_FRAME) {
		int ret;

		MTK_CAM_TRACE_BEGIN(BASIC, "ipi_frame_ack:%d",
				    ipi_msg->cookie.frame_no);

		ret = isp_composer_handle_ack(cam, ipi_msg);

		MTK_CAM_TRACE_END(BASIC);
		return ret;

	} else if (ipi_msg->ack_data.ack_cmd_id == CAM_CMD_DESTROY_SESSION) {
		ctx = &cam->ctxs[ipi_msg->cookie.session_id];
		complete(&ctx->session_complete);
		dev_info(dev, "%s:ctx(%d): session destroyed",
			 __func__, ctx->stream_id);
	}

	return 0;
}

#endif

static void isp_tx_frame_worker(struct work_struct *work)
{
	struct mtk_cam_req_work *req_work = (struct mtk_cam_req_work *)work;
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_frame_info *frame_info = &event.frame_data;
	struct mtkcam_ipi_frame_param *frame_data, *frame_param;
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_request_stream_data *req_stream_data_mraw;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	struct mtk_cam_working_buf_entry *buf_entry;
	struct mtk_mraw_working_buf_entry *mraw_buf_entry;
	struct mtkcam_ipi_meta_output *meta_1_out;
	struct mtk_cam_buffer *meta1_buf;
	struct mtk_mraw_device *mraw_dev;
	struct mtk_cam_resource *res_user;
	int i;
	int res_feature;

	req_stream_data = mtk_cam_req_work_get_s_data(req_work);
	if (!req_stream_data) {
		pr_info("%s mtk_cam_req_work(%p), req_stream_data(%p), dropped\n",
				__func__, req_work, req_stream_data);
		return;
	}
	req = mtk_cam_s_data_get_req(req_stream_data);
	if (!req) {
		pr_info("%s req_stream_data(%p), req(%p), dropped\n",
				__func__, req_stream_data, req);
		return;
	}
	ctx = mtk_cam_s_data_get_ctx(req_stream_data);
	if (!ctx) {
		pr_info("%s req_stream_data(%p), ctx(%p), dropped\n",
				__func__, req_stream_data, ctx);
		return;
	}

	cam = ctx->cam;

	if (ctx->used_raw_num == 0) {
		dev_dbg(cam->dev, "raw is un-used, skip frame work");
		return;
	}

	/* check if the ctx is streaming */
	spin_lock(&ctx->streaming_lock);
	if (!ctx->streaming) {
		dev_info(cam->dev,
			 "%s: skip frame work, for stream off ctx:%d, req:%d\n",
			 __func__, ctx->stream_id, req_stream_data->frame_seq_no);
		spin_unlock(&ctx->streaming_lock);
		return;
	}
	spin_unlock(&ctx->streaming_lock);

	res_feature = mtk_cam_s_data_get_res_feature(req_stream_data);

	/* Send CAM_CMD_CONFIG if the sink pad fmt is changed */
	if (req->ctx_link_update & 1 << ctx->stream_id ||
	    req_stream_data->flags & MTK_CAM_REQ_S_DATA_FLAG_SINK_FMT_UPDATE)
		mtk_cam_s_data_dev_config(req_stream_data, true, true);

	if (req->ctx_link_update & 1 << ctx->stream_id)
		mtk_cam_s_data_sv_dev_config(req_stream_data);

	/* handle stagger 1,2,3 exposure */
	if (mtk_cam_feature_is_stagger(res_feature))
		check_stagger_buffer(cam, ctx, req);

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_FRAME;
	session->session_id = ctx->stream_id;
	/* prepare raw working buffer */
	buf_entry = mtk_cam_working_buf_get(ctx);
	if (!buf_entry) {
		dev_info(cam->dev, "%s: No CQ buf availablle: req:%d(%s)\n",
			 __func__, req_stream_data->frame_seq_no,
			 req->req.debug_str);
		WARN_ON(1);
		return;
	}
	mtk_cam_s_data_set_wbuf(req_stream_data, buf_entry);
	/* put to raw using list */
	spin_lock(&ctx->using_buffer_list.lock);
	list_add_tail(&buf_entry->list_entry, &ctx->using_buffer_list.list);
	ctx->using_buffer_list.cnt++;
	spin_unlock(&ctx->using_buffer_list.lock);

	/* Prepare MTKCAM_IPI_RAW_META_STATS_1 params */
	meta1_buf = mtk_cam_s_data_get_vbuf(req_stream_data, MTK_RAW_META_OUT_1);
	if (req_stream_data->flags & MTK_CAM_REQ_S_DATA_FLAG_META1_INDEPENDENT &&
	    meta1_buf) {
		/* replace the video buffer with ccd buffer*/
		frame_param = &req_stream_data->frame_params;
		meta_1_out =
			&frame_param->meta_outputs[MTK_RAW_META_OUT_1 - MTK_RAW_META_OUT_BEGIN];
		meta_1_out->buf.ccd_fd = buf_entry->meta_buffer.fd;
		meta_1_out->buf.size = buf_entry->meta_buffer.size;
		meta_1_out->buf.iova = buf_entry->meta_buffer.iova;
		meta_1_out->uid.id = MTKCAM_IPI_RAW_META_STATS_1;
		mtk_cam_set_meta_stats_info(MTKCAM_IPI_RAW_META_STATS_1,
					    buf_entry->meta_buffer.va,
					    NULL);
	}

	for (i = 0; i < ctx->used_mraw_num; i++) {
		req->p_data[ctx->mraw_pipe[i]->id].s_data_num =
			req->p_data[ctx->stream_id].s_data_num;
		req_stream_data_mraw =
			mtk_cam_req_get_s_data(req, ctx->mraw_pipe[i]->id,
			req_stream_data->index);
		mraw_buf_entry = mtk_cam_mraw_working_buf_get(ctx);
		if (!mraw_buf_entry) {
			dev_info(cam->dev, "%s: No Mraw working buf availablle: req:%d(%s)\n",
				__func__, req_stream_data->frame_seq_no,
				req->req.debug_str);
			WARN_ON(1);
			return;
		}
		mtk_cam_mraw_wbuf_set_s_data(mraw_buf_entry, req_stream_data_mraw);
		mraw_buf_entry->ctx = ctx;
		mraw_buf_entry->ts_raw = 0;
		mraw_buf_entry->ts_mraw = 0;
		mraw_buf_entry->is_stagger =
			mtk_cam_feature_is_stagger(res_feature) ? 1 : 0;
		atomic_set(&mraw_buf_entry->is_apply, 0);

		/* align master pipe's sequence number */
		req_stream_data_mraw->frame_seq_no = req_stream_data->frame_seq_no;
		req_stream_data_mraw->req = req_stream_data->req;
		req_stream_data_mraw->pipe_id = ctx->mraw_pipe[i]->id;
		req_stream_data_mraw->ctx = ctx;
		mtk_cam_req_dump_work_init(req_stream_data_mraw);

		if (req_stream_data_mraw->frame_seq_no == 1) {
			if (req->pipe_used & (1 << ctx->mraw_pipe[i]->id)) {
				mraw_dev = get_mraw_dev(cam, ctx->mraw_pipe[i]);
				mraw_dev->is_enqueued = 1;
			} else {
				/* not queued for first frame, so update cq done directly */
				ctx->cq_done_status |= (1 << ctx->mraw_pipe[i]->id);
			}
		}

		/* convert mraw's setting */
		if (req->pipe_used & (1 << ctx->mraw_pipe[i]->id)) {
			req_stream_data->frame_params.mraw_param[i]
				= req_stream_data_mraw->frame_params.mraw_param[0];
			req_stream_data->frame_params.mraw_param[i].pixel_mode
				= ctx->mraw_pipe[i]->res_config.pixel_mode;
		}

		dev_dbg(cam->dev, "%s: idx:%d mraw pipe id:%d tg_w:%d tg_h:%d pixel_mode:%d\n",
			__func__, req_stream_data->index, ctx->mraw_pipe[i]->id,
			req_stream_data->frame_params.mraw_param[i].tg_size_w,
			req_stream_data->frame_params.mraw_param[i].tg_size_h,
			req_stream_data->frame_params.mraw_param[i].pixel_mode);

		spin_lock(&ctx->mraw_using_buffer_list[i].lock);
		list_add_tail(&mraw_buf_entry->list_entry,
			&ctx->mraw_using_buffer_list[i].list);
		ctx->mraw_using_buffer_list[i].cnt++;
		spin_unlock(&ctx->mraw_using_buffer_list[i].lock);
	}

	/* Prepare rp message */
	frame_info->cur_msgbuf_offset =
		buf_entry->msg_buffer.va -
		cam->ctxs[session->session_id].buf_pool.msg_buf_va;
	frame_info->cur_msgbuf_size = buf_entry->msg_buffer.size;
	frame_data = (struct mtkcam_ipi_frame_param *)buf_entry->msg_buffer.va;
	session->frame_no = req_stream_data->frame_seq_no;

	if (mtk_cam_feature_is_stagger(res_feature) ||
		mtk_cam_feature_is_mstream(res_feature) ||
		mtk_cam_feature_is_mstream_m2m(res_feature))
		dev_dbg(cam->dev, "[%s:vhdr-req:%d] ctx:%d type:%d (ipi)hwscene:%d/expN:%d/prev_expN:%d\n",
			__func__, req_stream_data->frame_seq_no, ctx->stream_id,
			req_stream_data->feature.switch_feature_type,
			req_stream_data->frame_params.raw_param.hardware_scenario,
			req_stream_data->frame_params.raw_param.exposure_num,
			req_stream_data->frame_params.raw_param.previous_exposure_num);

	/* record mmqos (skip mstream 1exp) */
	if (!(mtk_cam_is_mstream(ctx) &&
		req_stream_data->frame_params.raw_param.exposure_num == 1)) {
		frame_param = &req_stream_data->frame_params;
		for (i = 0; i < CAM_MAX_IMAGE_INPUT; i++) {
			if (frame_param->img_ins[i].buf[0].iova != 0 &&
				frame_param->img_ins[i].uid.id != 0)
				req_stream_data->raw_dmas |=
					(1ULL << frame_param->img_ins[i].uid.id);
		}
		for (i = 0; i < CAM_MAX_IMAGE_OUTPUT; i++) {
			if (frame_param->img_outs[i].buf[0][0].iova != 0 &&
				frame_param->img_outs[i].uid.id != 0)
				req_stream_data->raw_dmas |=
					(1ULL << frame_param->img_outs[i].uid.id);
		}
		for (i = 0; i < CAM_MAX_META_OUTPUT; i++) {
			if (frame_param->meta_outputs[i].buf.iova != 0 &&
				frame_param->meta_outputs[i].uid.id != 0)
				req_stream_data->raw_dmas |=
					(1ULL << frame_param->meta_outputs[i].uid.id);
		}
		for (i = 0; i < CAM_MAX_PIPE_USED; i++) {
			if (frame_param->meta_inputs[i].buf.iova != 0 &&
				frame_param->meta_outputs[i].uid.id != 0)
				req_stream_data->raw_dmas |=
					(1ULL << frame_param->meta_inputs[i].uid.id);
		}
	}

	memcpy(frame_data, &req_stream_data->frame_params,
	       sizeof(req_stream_data->frame_params));
	frame_data->cur_workbuf_offset =
		buf_entry->buffer.iova -
		cam->ctxs[session->session_id].buf_pool.working_buf_iova;
	frame_data->cur_workbuf_size = buf_entry->buffer.size;

	res_user = mtk_cam_s_data_get_res(req_stream_data);
	if (res_user && res_user->raw_res.bin) {
		frame_data->raw_param.bin_flag = res_user->raw_res.bin;
	} else {
		if (ctx->pipe->res_config.bin_limit == BIN_AUTO)
			frame_data->raw_param.bin_flag = ctx->pipe->res_config.bin_enable;
		else
			frame_data->raw_param.bin_flag = ctx->pipe->res_config.bin_limit;
	}

	if (ctx->rpmsg_dev) {
		MTK_CAM_TRACE_BEGIN(BASIC, "ipi_cmd_frame:%d",
				    req_stream_data->frame_seq_no);

		rpmsg_send(ctx->rpmsg_dev->rpdev.ept, &event, sizeof(event));

		MTK_CAM_TRACE_END(BASIC);

		dev_dbg(cam->dev,
			 "%s: rpmsg_send id: %d, ctx:%d, seq:%d, bin:(0x%x)\n",
			 req->req.debug_str, event.cmd_id, session->session_id,
			 req_stream_data->frame_seq_no,
			 frame_data->raw_param.bin_flag);
	} else {
		dev_dbg(cam->dev, "%s: rpmsg_dev not exist: %d, ctx:%d, req:%d\n",
			__func__, event.cmd_id, session->session_id,
			req_stream_data->frame_seq_no);
	}
}

bool mtk_cam_sv_req_enqueue(struct mtk_cam_ctx *ctx,
	struct mtk_cam_request *req, unsigned int idx)
{
	unsigned int i, pipe_id;
	struct mtk_cam_request_stream_data *ctx_stream_data;
	struct mtk_cam_request_stream_data *pipe_stream_data;
	struct mtk_camsv_working_buf_entry *buf_entry;
	int res_feature;
	bool ret = true;

	if (ctx->used_sv_num == 0)
		return ret;

	ctx_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, idx);
	res_feature = mtk_cam_s_data_get_res_feature(ctx_stream_data);
	for (i = 0 ; i < ctx->used_sv_num ; i++) {
		pipe_id = ctx->sv_pipe[i]->id;
		buf_entry = mtk_cam_sv_working_buf_get(ctx);
		req->p_data[pipe_id].s_data_num = req->p_data[ctx->stream_id].s_data_num;
		pipe_stream_data = mtk_cam_req_get_s_data(req, pipe_id, idx);
		/* align master pipe's sequence number */
		pipe_stream_data->frame_seq_no = ctx_stream_data->frame_seq_no;
		pipe_stream_data->req = ctx_stream_data->req;
		pipe_stream_data->pipe_id = ctx->sv_pipe[i]->id;
		pipe_stream_data->ctx = ctx;
		buf_entry->ts_raw = 0;
		buf_entry->ts_sv = 0;
		buf_entry->is_stagger =
			(mtk_cam_feature_is_stagger(res_feature)) ? 1 : 0;
		atomic_set(&buf_entry->is_apply, 0);

		mtk_cam_req_dump_work_init(pipe_stream_data);
		mtk_cam_req_work_init(&pipe_stream_data->sv_work, pipe_stream_data);
		INIT_WORK(&pipe_stream_data->sv_work.work, mtk_cam_sv_work);
		mtk_cam_sv_wbuf_set_s_data(buf_entry, pipe_stream_data);
		spin_lock(&ctx->sv_using_buffer_list[i].lock);
		list_add_tail(&buf_entry->list_entry,
				&ctx->sv_using_buffer_list[i].list);
		ctx->sv_using_buffer_list[i].cnt++;
		spin_unlock(&ctx->sv_using_buffer_list[i].lock);
	}
	if (ctx_stream_data->frame_seq_no == 1) {
		mtk_cam_sv_update_all_buffer_ts(ctx, ktime_get_boottime_ns());
		mtk_cam_sv_apply_all_buffers(ctx);
		if (ctx->stream_id >= MTKCAM_SUBDEV_CAMSV_START &&
			ctx->stream_id < MTKCAM_SUBDEV_CAMSV_END) {
			if (ctx_stream_data->state.estate == E_STATE_READY ||
				ctx_stream_data->state.estate == E_STATE_SENSOR) {
				ctx_stream_data->state.estate = E_STATE_OUTER;
			}
		}
	}

	return ret;
}

void mtk_cam_sensor_switch_stop_reinit_hw(struct mtk_cam_ctx *ctx,
					  struct mtk_cam_request_stream_data *s_data,
					  int stream_id)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request *req;
	struct mtk_cam_req_raw_pipe_data *s_raw_pipe_data;
	struct mtk_raw_device *raw_dev;
	struct mtk_camsv_device *sv_dev;
	struct mtk_mraw_device *mraw_dev;
	int i;
	int sof_count;
	int ret;
	int feature, feature_first_req;

	req = mtk_cam_s_data_get_req(s_data);
	s_raw_pipe_data = mtk_cam_s_data_get_raw_pipe_data(s_data);
	if (!s_raw_pipe_data) {
		dev_info(cam->dev, "%s: failed to get raw_pipe_data (pipe:%d, seq:%d)\n",
			 __func__, s_data->pipe_id, s_data->frame_seq_no);
		return;
	}

	feature = s_raw_pipe_data->res.raw_res.feature;
	feature_first_req = s_data->feature.raw_feature;

	dev_info(ctx->cam->dev,
		 "%s:%s:pipe(%d): switch op seq(%d), flags(0x%x), ctx_link_update(0x%x), stream_id(%d), feature(0x%x)\n",
		 __func__, req->req.debug_str, s_data->pipe_id, s_data->frame_seq_no,
		 req->flags, req->ctx_link_update, stream_id,
		 feature);

	/* stop the raw */
	if (ctx->used_raw_num) {
		if (mtk_cam_is_hsf(ctx)) {
			ret = mtk_cam_hsf_uninit(ctx);
			if (ret != 0)
				dev_info(cam->dev,
					 "failed to stream off %s:%d mtk_cam_hsf_uninit fail\n",
					 ctx->seninf->name, ret);
		}
		raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
		if (mtk_cam_is_time_shared(ctx)) {
			unsigned int hw_scen =
				(1 << MTKCAM_IPI_HW_PATH_OFFLINE_M2M);
			for (i = MTKCAM_SUBDEV_CAMSV_START ; i < MTKCAM_SUBDEV_CAMSV_END ; i++) {
				if (ctx->pipe->enabled_raw & (1 << i)) {
					mtk_cam_sv_dev_stream_on
						(ctx, i - MTKCAM_SUBDEV_CAMSV_START, 0, hw_scen);
					cam->sv.pipelines[
						i - MTKCAM_SUBDEV_CAMSV_START].is_occupied = 0;
					ctx->pipe->enabled_raw &= ~(1 << i);
				}
			}
		} else {
			if (mtk_cam_is_mstream(ctx)) {
				struct mtk_cam_request_stream_data *mstream_2nd_data;

				mstream_2nd_data = mtk_cam_req_get_s_data(req, stream_id, 0);
				if (mstream_2nd_data->seninf_new) {
					s_data->sensor = mstream_2nd_data->sensor;
					s_data->seninf_new = mstream_2nd_data->seninf_new;
					s_data->seninf_old = mstream_2nd_data->seninf_old;
				}
			}
			// stream_on(raw_dev, 0);
			dev_info(ctx->cam->dev, "%s: Disable cammux: %s\n", __func__,
				 s_data->seninf_old->name);
			mtk_cam_seninf_set_camtg(s_data->seninf_old, PAD_SRC_RAW0, 0xFF);
			mtk_cam_seninf_set_camtg(s_data->seninf_old, PAD_SRC_RAW1, 0xFF);
			mtk_cam_seninf_set_camtg(s_data->seninf_old, PAD_SRC_RAW2, 0xFF);
			immediate_stream_off(raw_dev);
		}
		/* Twin */
		if (ctx->pipe->res_config.raw_num_used != 1) {
			struct mtk_raw_device *raw_dev_slave =
						get_slave_raw_dev(cam, ctx->pipe);
			stream_on(raw_dev_slave, 0);
			if (ctx->pipe->res_config.raw_num_used == 3) {
				struct mtk_raw_device *raw_dev_slave2 =
					get_slave2_raw_dev(cam, ctx->pipe);
				stream_on(raw_dev_slave2, 0);
			}
		}
	}

	/* stop the camsv for special scenario */
	if (mtk_cam_is_stagger(ctx)) {
		unsigned int hw_scen = mtk_raw_get_hdr_scen_id(ctx);

		for (i = MTKCAM_SUBDEV_CAMSV_START; i < MTKCAM_SUBDEV_CAMSV_END; i++) {
			if (ctx->pipe->enabled_raw & (1 << i)) {
				mtk_cam_sv_dev_stream_on
					(ctx, i - MTKCAM_SUBDEV_CAMSV_START, 0, hw_scen);
				cam->sv.pipelines[i - MTKCAM_SUBDEV_CAMSV_START].is_occupied = 0;
				ctx->pipe->enabled_raw &= ~(1 << i);
			}
		}
	} else if (mtk_cam_is_with_w_channel(ctx)) {
		unsigned int hw_scen = (1 << MTKCAM_SV_SPECIAL_SCENARIO_ADDITIONAL_RAW);

		for (i = MTKCAM_SUBDEV_CAMSV_START; i < MTKCAM_SUBDEV_CAMSV_END; i++) {
			if (ctx->pipe->enabled_raw & (1 << i)) {
				mtk_cam_sv_dev_stream_on
					(ctx, i - MTKCAM_SUBDEV_CAMSV_START, 0, hw_scen);
				cam->sv.pipelines[i - MTKCAM_SUBDEV_CAMSV_START].is_occupied = 0;
				ctx->pipe->enabled_raw &= ~(1 << i);
			}
		}
	}

	/* stop the camsv */
	for (i = 0 ; i < ctx->used_sv_num ; i++) {
		sv_dev = get_camsv_dev(cam, ctx->sv_pipe[i]);
		mtk_cam_sv_vf_on(sv_dev, 0);
		sv_dev->is_enqueued = 0;
	}

	for (i = 0 ; i < ctx->used_mraw_num ; i++) {
		mraw_dev = get_mraw_dev(cam, ctx->mraw_pipe[i]);
		mtk_cam_mraw_vf_on(mraw_dev, 0);
		mraw_dev->is_enqueued = 0;
	}

	/* apply sensor setting if needed */
	if ((s_data->frame_seq_no == 1) &&
	    (s_data->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_EN) &&
	    !(s_data->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_COMPLETE))
		mtk_cam_set_sensor_full(s_data, &ctx->sensor_ctrl);

	/* keep the sof_count to restore it after reinit */
	sof_count = raw_dev->sof_count;

	/* reinitialized the settings */
	if (ctx->used_raw_num) {
		if (!mtk_cam_is_hsf(ctx)) {
			initialize(raw_dev, 0);
			raw_dev->sof_count = sof_count;
			/* Stagger */
			if (mtk_cam_feature_is_stagger(feature)) {
				if (mtk_cam_feature_is_stagger(feature_first_req))
					stagger_enable(raw_dev);
				else
					stagger_disable(raw_dev);
			}
			/* Sub sample */
			if (mtk_cam_feature_is_subsample(feature))
				subsample_enable(raw_dev);
			/* Twin */
			if (ctx->pipe->res_config.raw_num_used != 1) {
				struct mtk_raw_device *raw_dev_slave =
				get_slave_raw_dev(cam, ctx->pipe);
				initialize(raw_dev_slave, 1);
				if (ctx->pipe->res_config.raw_num_used == 3) {
					struct mtk_raw_device *raw_dev_slave2 =
						get_slave2_raw_dev(cam, ctx->pipe);
					initialize(raw_dev_slave2, 1);
				}
			}
		}
	}
}

void handle_immediate_switch(struct mtk_cam_ctx *ctx,
			     struct mtk_cam_request_stream_data *s_data,
			     int stream_id)
{
	struct mtk_cam_request *req;

	req = mtk_cam_s_data_get_req(s_data);

	if (!is_raw_subdev(stream_id)) {
		dev_info(ctx->cam->dev,
			 "%s:%s:pipe(%d): switch op seq(%d), flags(0x%x), ctx_link_update(0x%x), stream_id(%d)\n",
			 __func__, req->req.debug_str, s_data->pipe_id, s_data->frame_seq_no,
			 req->flags, req->ctx_link_update, stream_id);
		return;
	}

	if ((req->flags & MTK_CAM_REQ_FLAG_SENINF_IMMEDIATE_UPDATE) &&
	    (req->ctx_link_update & (1 << stream_id)))
		mtk_cam_sensor_switch_stop_reinit_hw(ctx, s_data, stream_id);
}

void mtk_cam_dev_req_enqueue(struct mtk_cam_device *cam,
			     struct mtk_cam_request *req)
{
	unsigned int i, j;

	for (i = 0; i < cam->max_stream_num; i++) {
		if (req->pipe_used & (1 << i)) {
			unsigned int stream_id = i;
			struct mtk_cam_req_work *frame_work, *done_work;
			struct mtk_cam_request_stream_data *req_stream_data;
			struct mtk_cam_request_stream_data *pipe_stream_data;
			struct mtk_cam_ctx *ctx = &cam->ctxs[stream_id];
			struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
			unsigned int drained_seq_no = 0;
			unsigned int initial_frame = 0;

			/**
			 * For sub dev shares ctx's cases, e.g.
			 * PDAF's topoloy, camsv is the part of raw pipeline's
			 * ctx and we need to skip the camsv sub dev's ctx (which
			 * is not streaming here).
			 */
			if (!ctx->streaming)
				continue;
			atomic_set(&ctx->sensor_ctrl.sensor_enq_seq_no,
				atomic_read(&ctx->enqueued_frame_seq_no));
			/*sensor setting after request drained check*/
			if (ctx->used_raw_num) {
				if (ctx->pipe->feature_active == 0 &&
				    ctx->dequeued_frame_seq_no > 3) {
					drained_seq_no =
						atomic_read(&sensor_ctrl->last_drained_seq_no);
					if (atomic_read(&sensor_ctrl->sensor_enq_seq_no) ==
						drained_seq_no)
						mtk_cam_submit_kwork_in_sensorctrl(
							sensor_ctrl->sensorsetting_wq,
							sensor_ctrl);
				}
			}
			req_stream_data = mtk_cam_req_get_s_data(req, stream_id, 0);

			if (req_stream_data->frame_seq_no == 1 ||
			    ((mtk_cam_is_mstream(ctx) || mtk_cam_is_mstream_m2m(ctx)) &&
			    (req_stream_data->frame_seq_no == 2)) ||
			    (req->ctx_link_update & (1 << stream_id)))
				initial_frame = 1;

			frame_work = &req_stream_data->frame_work;
			mtk_cam_req_work_init(frame_work, req_stream_data);

			for (j = 0 ; j < MTKCAM_SUBDEV_MAX ; j++) {
				if ((1 << j & ctx->streaming_pipe) && (1 << j & req->pipe_used)) {
					pipe_stream_data = mtk_cam_req_get_s_data(req, j, 0);
					done_work = &pipe_stream_data->frame_done_work;
					INIT_WORK(&done_work->work, mtk_cam_frame_done_work);

					done_work = &pipe_stream_data->meta1_done_work;
					atomic_set(&done_work->is_queued, 0);
					INIT_WORK(&done_work->work, mtk_cam_meta1_done_work);
				}
			}

			if (mtk_cam_is_time_shared(ctx)) {
				req_stream_data->frame_params.raw_param.hardware_scenario =
						MTKCAM_IPI_HW_PATH_OFFLINE_M2M;
				req_stream_data->frame_params.raw_param.exposure_num = 1;
				req_stream_data->state.estate = E_STATE_TS_READY;
			}

			if (ctx->sensor && (initial_frame ||
					mtk_cam_is_m2m(ctx))) {
				if (mtk_cam_is_mstream(ctx) || mtk_cam_is_mstream_m2m(ctx)) {
					mtk_cam_mstream_initial_sensor_setup(req, ctx);
				} else {
					mtk_cam_initial_sensor_setup(req, ctx);
				}
			}
			if (ctx->used_raw_num != 0) {
				if (ctx->sensor && MTK_CAM_INITIAL_REQ_SYNC == 0 &&
					(ctx->pipe->feature_active == 0 ||
					req_stream_data->frame_params.raw_param.hardware_scenario
					== MTKCAM_IPI_HW_PATH_OFFLINE_SRT_DCIF_STAGGER) &&
					req_stream_data->frame_seq_no == 2) {
					mtk_cam_initial_sensor_setup(req, ctx);
				}
			} else { // for single sv pipe stream
				if (ctx->sensor && MTK_CAM_INITIAL_REQ_SYNC == 0 &&
					req_stream_data->frame_seq_no == 2) {
					mtk_cam_initial_sensor_setup(req, ctx);
				}
			}

			if (mtk_cam_is_mstream(ctx)) {
				if (req->ctx_link_update & (1 << stream_id)) {
					struct mtk_cam_request_stream_data *s_data;

					s_data = mtk_cam_req_get_s_data(req, stream_id, 1);
					ctx->sensor_ctrl.ctx = ctx;
					s_data->ctx = ctx;
					handle_immediate_switch(ctx, s_data, i);
				}
			} else {
				handle_immediate_switch(ctx, req_stream_data, i);
			}

			/* Prepare CQ compose work */
			if (mtk_cam_is_mstream(ctx) || mtk_cam_is_mstream_m2m(ctx)) {
				int frame_cnt;

				for (frame_cnt = 1; frame_cnt <= MTKCAM_MSTREAM_MAX;
						frame_cnt++) {
					if (frame_cnt == 1) { // first exposure
						dev_dbg(cam->dev, "%s: mstream 1st exp frame\n",
							__func__);
						req_stream_data =
							mtk_cam_req_get_s_data(req,
							stream_id, 1);
						mtk_cam_sv_req_enqueue(ctx, req, 1);
					} else { // second exposure
						dev_dbg(cam->dev, "%s: mstream 2nd exp frame\n",
							__func__);
						req_stream_data =
							mtk_cam_req_get_s_data(req,
							stream_id, 0);
						mtk_cam_sv_req_enqueue(ctx, req, 0);
					}
					frame_work = &req_stream_data->frame_work;
					mtk_cam_req_dump_work_init(req_stream_data);
					INIT_WORK(&frame_work->work,
						isp_tx_frame_worker);
					queue_work(ctx->composer_wq,
						&frame_work->work);
				}
			} else {
				mtk_cam_sv_req_enqueue(ctx, req, 0);
				mtk_cam_req_dump_work_init(req_stream_data);
				INIT_WORK(&frame_work->work, isp_tx_frame_worker);
				queue_work(ctx->composer_wq, &frame_work->work);
			}

			if (watchdog_scenario(ctx) &&
			    initial_frame &&
			    !(req->ctx_link_update & (1 << stream_id)))
				mtk_ctx_watchdog_start(ctx, 4);

			dev_dbg(cam->dev, "%s:ctx:%d:req:%d(%s) enqueue ctx_used:0x%x,streaming_ctx:0x%x,job cnt:%d, running(%d)\n",
				__func__, stream_id, req_stream_data->frame_seq_no,
				req->req.debug_str, req->ctx_used, cam->streaming_ctx,
				cam->running_job_count, atomic_read(&ctx->running_s_data_cnt));
		}
	}
}

#if CCD_READY
int isp_composer_create_session(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_session_param	*session_data = &event.session_data;
	int ret;

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_CREATE_SESSION;
	session->session_id = ctx->stream_id;
	session_data->workbuf.iova = ctx->buf_pool.working_buf_iova;
	/* TODO: separated with SCP case */
	session_data->workbuf.ccd_fd = ctx->buf_pool.working_buf_fd;
	session_data->workbuf.size = ctx->buf_pool.working_buf_size;
	session_data->msg_buf.ccd_fd = ctx->buf_pool.msg_buf_fd;
	session_data->msg_buf.size = ctx->buf_pool.msg_buf_size;

	ret = rpmsg_send(ctx->rpmsg_dev->rpdev.ept, &event, sizeof(event));
	dev_dbg(cam->dev,
		"%s: rpmsg_send id: %d, cq_buf(fd:%d,sz:%d, msg_buf(fd:%d,sz%d) ret(%d)\n",
		__func__, event.cmd_id, session_data->workbuf.ccd_fd,
		session_data->workbuf.size, session_data->msg_buf.ccd_fd,
		session_data->msg_buf.size,
		ret);

	return ret;
}

void isp_composer_destroy_session(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_DESTROY_SESSION;
	session->session_id = ctx->stream_id;
	rpmsg_send(ctx->rpmsg_dev->rpdev.ept, &event, sizeof(event));

	dev_info(cam->dev, "rpmsg_send: DESTROY_SESSION\n");
}

static void isp_destroy_session_wq(struct work_struct *work)
{
	struct mtk_cam_ctx *ctx = container_of(work, struct mtk_cam_ctx,
					       session_work);

	isp_composer_destroy_session(ctx);
}

static void isp_composer_destroy_session_async(struct mtk_cam_ctx *ctx)
{
	struct work_struct *w = &ctx->session_work;

	INIT_WORK(w, isp_destroy_session_wq);
	queue_work(ctx->composer_wq, w);
}

#endif

static void
isp_composer_hw_config(struct mtk_cam_device *cam,
		       struct mtk_cam_ctx *ctx,
		       struct mtkcam_ipi_config_param *config_param)
{
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_config_param *config = &event.config_data;

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_CONFIG;
	session->session_id = ctx->stream_id;
	memcpy(config, config_param, sizeof(*config_param));
	dev_dbg(cam->dev, "%s sw_feature:%d\n", __func__, config->sw_feature);
	rpmsg_send(ctx->rpmsg_dev->rpdev.ept, &event, sizeof(event));
	dev_dbg(cam->dev, "rpmsg_send id: %d\n", event.cmd_id);

	/* For debug dump file */
	memcpy(&ctx->config_params, config_param, sizeof(*config_param));
	dev_dbg(cam->dev, "%s:ctx(%d): save config_param to ctx, sz:%d\n",
		__func__, ctx->stream_id, sizeof(*config_param));
}

struct mtk_raw_pipeline*
mtk_cam_dev_get_raw_pipeline(struct mtk_cam_device *cam,
			     unsigned int pipe_id)
{
	if (pipe_id < MTKCAM_SUBDEV_RAW_START || pipe_id >= MTKCAM_SUBDEV_RAW_END)
		return NULL;
	else
		return &cam->raw.pipelines[pipe_id - MTKCAM_SUBDEV_RAW_0];
}

static int
mtk_cam_raw_pipeline_config(struct mtk_cam_ctx *ctx,
			    struct mtkcam_ipi_input_param *cfg_in_param)
{
	struct mtk_raw_pipeline *pipe = ctx->pipe;
	struct mtk_raw *raw = pipe->raw;
	unsigned int i;
	int ret;

	/* reset pm_runtime during streaming dynamic change */
	if (ctx->streaming) {
		for (i = 0; i < ARRAY_SIZE(raw->devs); i++) {
			if (pipe->enabled_raw & 1 << i) {
				dev_info(raw->cam_dev, "%s: power off raw (%d) for reset\n",
						 __func__, i);
				pm_runtime_put_sync(raw->devs[i]);
			}
		}
	}

	ret = mtk_cam_raw_select(ctx, cfg_in_param);
	if (ret) {
		dev_info(raw->cam_dev, "failed select raw: %d\n",
			 ctx->stream_id);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(raw->devs); i++) {
		if (pipe->enabled_raw & 1 << i) {
			dev_info(raw->cam_dev, "%s: power on raw (%d)\n", __func__, i);
			pm_runtime_get_sync(raw->devs[i]);
		}
	}

	if (ret < 0) {
		dev_info(raw->cam_dev,
			 "failed at pm_runtime_get_sync: %s\n",
			 dev_driver_string(raw->devs[i]));
		for (i = i - 1; i >= 0; i--)
			if (pipe->enabled_raw & 1 << i) {
				dev_info(raw->cam_dev, "%s: power off raw (%d)\n",
						 __func__, i);
				pm_runtime_put_sync(raw->devs[i]);
			}
		return ret;
	}

	ctx->used_raw_dev = pipe->enabled_raw;
	dev_info(raw->cam_dev, "ctx_id %d used_raw_dev 0x%x pipe_id %d\n",
		ctx->stream_id, ctx->used_raw_dev, pipe->id);
	return 0;
}

static int
mtk_cam_s_data_raw_pipeline_config(struct mtk_cam_request_stream_data *s_data,
			    struct mtkcam_ipi_input_param *cfg_in_param)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_raw_pipeline *pipe;
	struct mtk_cam_req_raw_pipe_data *s_raw_pipe_data;
	struct mtk_raw *raw;
	int ret;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	s_raw_pipe_data = mtk_cam_s_data_get_raw_pipe_data(s_data);
	if (!s_raw_pipe_data) {
		dev_info(ctx->cam->dev, "%s: failed to get raw_pipe_data (pipe:%d, seq:%d)\n",
			 __func__, s_data->pipe_id, s_data->frame_seq_no);
		return -EINVAL;
	}

	pipe = ctx->pipe;
	raw = pipe->raw;

	ret = mtk_cam_s_data_raw_select(s_data, cfg_in_param);
	if (ret) {
		dev_dbg(raw->cam_dev, "failed select raw: %d\n",
			ctx->stream_id);
		return ret;
	}

	s_raw_pipe_data->enabled_raw |= s_raw_pipe_data->stagger_select.enabled_raw;
	dev_info(raw->cam_dev, "ctx_id %d used_raw_dev 0x%x pipe_id %d\n",
		ctx->stream_id, s_raw_pipe_data->enabled_raw, pipe->id);

	return 0;
}

void mtk_cam_apply_pending_dev_config(struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_req_raw_pipe_data *s_raw_pipe_data;
	struct mtk_cam_ctx *ctx;
	char *debug_str = mtk_cam_s_data_get_dbg_str(s_data);

	ctx = mtk_cam_s_data_get_ctx(s_data);
	ctx->pipe->feature_active = ctx->pipe->user_res.raw_res.feature;

	s_raw_pipe_data = mtk_cam_s_data_get_raw_pipe_data(s_data);
	if (!s_raw_pipe_data) {
		dev_info(ctx->cam->dev, "%s: failed to get raw_pipe_data (pipe:%d, seq:%d)\n",
			 __func__, s_data->pipe_id, s_data->frame_seq_no);
		return;
	}

	ctx->pipe->stagger_path = s_raw_pipe_data->stagger_select.stagger_path;
	ctx->pipe->enabled_raw = s_raw_pipe_data->enabled_raw;
	ctx->used_raw_dev = s_raw_pipe_data->enabled_raw;

	dev_info(ctx->cam->dev,
		"%s:%s:pipe(%d):seq(%d):feature_active(0x%x), ctx->pipe->user_res.raw_res.feature(%d), stagger_path(0x%x), enabled_raw(0x%x)\n",
		__func__, debug_str, ctx->stream_id, s_data->frame_seq_no,
		ctx->pipe->feature_active,
		ctx->pipe->user_res.raw_res.feature,
		ctx->pipe->stagger_path, ctx->pipe->enabled_raw);
}

int mtk_cam_s_data_dev_config(struct mtk_cam_request_stream_data *s_data,
	bool streaming, bool config_pipe)
{
	struct mtk_cam_request *req;
	struct mtk_cam_req_raw_pipe_data *s_raw_pipe_data;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	struct device *dev;
	struct mtkcam_ipi_config_param config_param;
	struct mtkcam_ipi_input_param *cfg_in_param;
	struct mtk_raw_pipeline *pipe;
	struct mtk_raw *raw;
	struct v4l2_mbus_framefmt *mf;
	struct device *dev_raw;
	struct mtk_raw_device *raw_dev;
	struct v4l2_format *img_fmt;
	unsigned int i;
	int ret;
	u32 mf_code;
	int feature;

	req = mtk_cam_s_data_get_req(s_data);
	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	dev = cam->dev;
	pipe = ctx->pipe;
	raw = pipe->raw;
	mf = &pipe->cfg[MTK_RAW_SINK].mbus_fmt;
	s_raw_pipe_data = mtk_cam_s_data_get_raw_pipe_data(s_data);
	if (!s_raw_pipe_data) {
		dev_info(cam->dev, "%s: failed to get raw_pipe_data (pipe:%d, seq:%d)\n",
			 __func__, s_data->pipe_id, s_data->frame_seq_no);
		return -EINVAL;
	}

	feature = s_raw_pipe_data->res.raw_res.feature;

	memset(&config_param, 0, sizeof(config_param));

	/* Update cfg_in_param */
	cfg_in_param = &config_param.input;
	cfg_in_param->pixel_mode = ctx->pipe->res_config.tgo_pxl_mode;
	cfg_in_param->subsample = mtk_cam_get_subsample_ratio(
					ctx->pipe->feature_active);
	/* TODO: data pattern from meta buffer per frame setting */
	cfg_in_param->data_pattern = MTKCAM_IPI_SENSOR_PATTERN_NORMAL;
	img_fmt = &pipe->vdev_nodes[MTK_RAW_SINK].pending_fmt;
	cfg_in_param->in_crop.s.w = img_fmt->fmt.pix_mp.width;
	cfg_in_param->in_crop.s.h = img_fmt->fmt.pix_mp.height;
	dev_dbg(dev, "sink pad code:0x%x, tg size:%d %d\n", mf->code,
		cfg_in_param->in_crop.s.w, cfg_in_param->in_crop.s.h);

	mf_code = mf->code & 0xffff; /* todo: sensor mode issue, need patch */
	cfg_in_param->raw_pixel_id = mtk_cam_get_sensor_pixel_id(mf_code);
	cfg_in_param->fmt = mtk_cam_get_sensor_fmt(mf_code);
	if (cfg_in_param->fmt == MTKCAM_IPI_IMG_FMT_UNKNOWN ||
	    cfg_in_param->raw_pixel_id == MTKCAM_IPI_BAYER_PXL_ID_UNKNOWN) {
		dev_dbg(dev, "unknown sd code:%d\n", mf_code);
		return -EINVAL;
	}

	s_raw_pipe_data->enabled_raw = ctx->pipe->enabled_raw & MTKCAM_SUBDEV_RAW_MASK;
	if (config_pipe && mtk_cam_feature_is_stagger(feature)) {
		ret = mtk_cam_s_data_raw_pipeline_config(s_data, cfg_in_param);
		if (ret)
			return ret;
	}

	config_param.flags = MTK_CAM_IPI_CONFIG_TYPE_INPUT_CHANGE;

	dev_dbg(dev, "%s: config_param flag:0x%x enabled_raw:0x%x\n", __func__,
			config_param.flags, s_raw_pipe_data->enabled_raw);

	if (config_pipe && mtk_cam_feature_is_stagger(feature)) {
		int master = (1 << MTKCAM_SUBDEV_RAW_0);
		int hw_scen, req_amount, idle_pipes;

		for (i = MTKCAM_SUBDEV_RAW_START; i < MTKCAM_SUBDEV_RAW_END; i++) {
			if (s_raw_pipe_data->enabled_raw & (1 << i)) {
				master = (1 << i);
				break;
			}
		}
		if (s_raw_pipe_data->stagger_select.stagger_path == STAGGER_ON_THE_FLY) {
			hw_scen = (1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER);
			req_amount = (mtk_cam_feature_is_3_exposure(feature)) ? 2 : 1;
		} else if (s_raw_pipe_data->stagger_select.stagger_path == STAGGER_DCIF) {
			hw_scen = (1 << MTKCAM_IPI_HW_PATH_OFFLINE_SRT_DCIF_STAGGER);
			req_amount = (mtk_cam_feature_is_3_exposure(feature)) ? 3 : 2;
		} else if (s_raw_pipe_data->stagger_select.stagger_path == STAGGER_OFFLINE) {
			hw_scen = (1 << MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER);
			req_amount = (mtk_cam_feature_is_3_exposure(feature)) ? 3 : 2;
		} else {
			hw_scen = (1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER);
			req_amount = (mtk_cam_feature_is_3_exposure(feature)) ? 2 : 1;
		}
		idle_pipes = get_available_sv_pipes(cam, hw_scen, req_amount, master, 0);
		/* cached used sv pipes */
		s_raw_pipe_data->enabled_raw |= idle_pipes;
		ctx->pipe->enabled_raw |= idle_pipes;
		if (idle_pipes == 0) {
			dev_info(cam->dev, "no available sv pipes(scen:%d/req_amount:%d)",
				hw_scen, req_amount);
			return -EINVAL;
		}
	}

	update_hw_mapping(ctx, &config_param, feature,
		s_raw_pipe_data->stagger_select.stagger_path,
		s_raw_pipe_data->enabled_raw);

	if (mtk_cam_feature_is_mstream(feature) || mtk_cam_feature_is_mstream_m2m(feature)) {
		config_param.sw_feature = MTKCAM_IPI_SW_FEATURE_VHDR_MSTREAM;
		dev_dbg(dev, "%s sw_feature:%d", __func__, config_param.sw_feature);
	} else {
		config_param.sw_feature = (mtk_cam_feature_is_stagger(feature) == 1 ||
			mtk_cam_feature_is_stagger_m2m(feature) == 1) ?
			MTKCAM_IPI_SW_FEATURE_VHDR_STAGGER : MTKCAM_IPI_SW_FEATURE_NORMAL;
	}

	dev_raw = mtk_cam_find_raw_dev(cam, s_raw_pipe_data->enabled_raw);
	if (!dev_raw) {
		dev_dbg(dev, "config raw device not found\n");
		return -EINVAL;
	}

	raw_dev = dev_get_drvdata(dev_raw);
	for (i = 0; i < RAW_PIPELINE_NUM; i++)
		if (raw->pipelines[i].enabled_raw & 1 << raw_dev->id) {
			raw_dev->pipeline = &raw->pipelines[i];
			/* TWIN case */
			if (raw->pipelines[i].res_config.raw_num_used != 1) {
				struct mtk_raw_device *raw_dev_slave =
						get_slave_raw_dev(cam, ctx->pipe);
				raw_dev_slave->pipeline = &raw->pipelines[i];
				dev_dbg(dev, "twin master/slave raw_id:%d/%d\n",
					raw_dev->id, raw_dev_slave->id);
				if (raw->pipelines[i].res_config.raw_num_used == 3) {
					struct mtk_raw_device *raw_dev_slave2 =
						get_slave2_raw_dev(cam, ctx->pipe);
					raw_dev_slave2->pipeline = &raw->pipelines[i];
					dev_dbg(dev, "triplet m/s/s2 raw_id:%d/%d/%d\n",
						raw_dev->id, raw_dev_slave->id, raw_dev_slave2->id);
				}
			}
			break;
		}

	isp_composer_hw_config(cam, ctx, &config_param);
	dev_dbg(dev, "raw %d %s done\n", raw_dev->id, __func__);

	return 0;
}

int mtk_cam_s_data_sv_dev_config(struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_request *req;
	struct mtk_cam_req_raw_pipe_data *s_raw_pipe_data;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	int i, j, ret = 0, used_pipes, src_pad_idx, exp_no, feature;
	unsigned int hw_scen;
	bool bDcif;

	req = mtk_cam_s_data_get_req(s_data);
	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	s_raw_pipe_data = mtk_cam_s_data_get_raw_pipe_data(s_data);
	if (!s_raw_pipe_data) {
		dev_info(cam->dev, "%s: failed to get raw_pipe_data (pipe:%d, seq:%d)\n",
			 __func__, s_data->pipe_id, s_data->frame_seq_no);
		return -EINVAL;
	}

	feature = s_raw_pipe_data->res.raw_res.feature;

	/* check exposure number */
	if (mtk_cam_feature_is_2_exposure(feature))
		exp_no = 2;
	else if (mtk_cam_feature_is_3_exposure(feature))
		exp_no = 3;
	else
		exp_no = 1;

	/* check hardware scenario */
	if (s_raw_pipe_data->stagger_select.stagger_path == STAGGER_ON_THE_FLY) {
		hw_scen = (1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER);
		bDcif = false;
	} else if (s_raw_pipe_data->stagger_select.stagger_path == STAGGER_DCIF) {
		hw_scen = (1 << MTKCAM_IPI_HW_PATH_OFFLINE_SRT_DCIF_STAGGER);
		bDcif = true;
	} else if (s_raw_pipe_data->stagger_select.stagger_path == STAGGER_OFFLINE) {
		hw_scen = (1 << MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER);
		bDcif = false;
	} else {
		hw_scen = (1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER);
		bDcif = false;
	}

	used_pipes = s_raw_pipe_data->enabled_raw;
	for (i = MTKCAM_SUBDEV_CAMSV_START; i < MTKCAM_SUBDEV_CAMSV_END; i++) {
		for (j = 0; j < MAX_STAGGER_EXP_AMOUNT; j++) {
			if (cam->sv.pipelines[i -
				MTKCAM_SUBDEV_CAMSV_START].hw_cap &
				(1 << (j + CAMSV_EXP_ORDER_SHIFT))) {
				src_pad_idx = PAD_SRC_RAW0 + j;
				break;
			}
		}
		if (used_pipes & (1 << i)) {
			ret = mtk_cam_sv_dev_config
				(ctx, i - MTKCAM_SUBDEV_CAMSV_START, hw_scen,
				 (bDcif && (src_pad_idx == exp_no)) ?
				 2 : src_pad_idx - PAD_SRC_RAW0);
			if (ret) {
				dev_info(cam->dev, "%s failed(pipe:%d)", __func__, i);
				return ret;
			}
		}
	}

	return ret;
}

/* FIXME: modified from v5: should move to raw */
int mtk_cam_dev_config(struct mtk_cam_ctx *ctx, bool streaming, bool config_pipe)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev = cam->dev;
	struct mtkcam_ipi_config_param config_param;
	struct mtkcam_ipi_input_param *cfg_in_param;
	struct mtk_raw_pipeline *pipe = ctx->pipe;
	struct mtk_raw *raw = pipe->raw;
	struct v4l2_mbus_framefmt *mf = &pipe->cfg[MTK_RAW_SINK].mbus_fmt;
	struct device *dev_raw;
	struct mtk_raw_device *raw_dev;
	struct v4l2_format *img_fmt;
	unsigned int i;
	int ret;
	u32 mf_code;
	int feature_active; /* Used to know max exposure num */

	feature_active = ctx->pipe->feature_active;
	/**
	 * If you wan't to get the first req's raw_feature (not the max exp. num),
	 * you can use read ctx->pipe->feature_pending here.
	 */

	memset(&config_param, 0, sizeof(config_param));

	/* Update cfg_in_param */
	cfg_in_param = &config_param.input;
	cfg_in_param->pixel_mode = ctx->pipe->res_config.tgo_pxl_mode;
	cfg_in_param->subsample = mtk_cam_get_subsample_ratio(
					ctx->pipe->feature_active);
	/* TODO: data pattern from meta buffer per frame setting */
	cfg_in_param->data_pattern = MTKCAM_IPI_SENSOR_PATTERN_NORMAL;
	img_fmt = &pipe->vdev_nodes[MTK_RAW_SINK].pending_fmt;
	cfg_in_param->in_crop.s.w = img_fmt->fmt.pix_mp.width;
	cfg_in_param->in_crop.s.h = img_fmt->fmt.pix_mp.height;

	if (mtk_cam_is_pure_m2m(ctx)) {
		mf = &pipe->cfg[MTK_RAW_RAWI_2_IN].mbus_fmt;
		dev_dbg(dev, "[pure m2m] rawi2 pad code:0x%x, sink tg size:%d %d\n",
			mf->code, cfg_in_param->in_crop.s.w, cfg_in_param->in_crop.s.h);
	} else {
		dev_dbg(dev, "sink pad code:0x%x, tg size:%d %d\n", mf->code,
			cfg_in_param->in_crop.s.w, cfg_in_param->in_crop.s.h);
	}

	mf_code = mf->code & 0xffff; /* todo: sensor mode issue, need patch */
	cfg_in_param->raw_pixel_id = mtk_cam_get_sensor_pixel_id(mf_code);
	cfg_in_param->fmt = mtk_cam_get_sensor_fmt(mf_code);
	if (cfg_in_param->fmt == MTKCAM_IPI_IMG_FMT_UNKNOWN ||
	    cfg_in_param->raw_pixel_id == MTKCAM_IPI_BAYER_PXL_ID_UNKNOWN) {
		dev_info(dev, "unknown sd code:%d\n", mf_code);
		return -EINVAL;
	}

	if (config_pipe) {
		config_param.flags = MTK_CAM_IPI_CONFIG_TYPE_INIT;
		ret = mtk_cam_raw_pipeline_config(ctx, cfg_in_param);
		if (ret)
			return ret;
	} else {
		/* Change the input size information only */
		config_param.flags = MTK_CAM_IPI_CONFIG_TYPE_INPUT_CHANGE;
	}

	if (config_pipe && is_first_request_sync(ctx))
		config_param.flags |= MTK_CAM_IPI_CONFIG_TYPE_EXEC_TWICE;

	dev_dbg(dev, "%s: config_param flag:0x%x enabled_raw:0x%x\n", __func__,
			config_param.flags, ctx->pipe->enabled_raw);

	if (config_pipe && mtk_cam_feature_is_stagger(feature_active)) {
		int master = (1 << MTKCAM_SUBDEV_RAW_0);
		int hw_scen, req_amount, idle_pipes;

		for (i = MTKCAM_SUBDEV_RAW_START; i < MTKCAM_SUBDEV_RAW_END; i++) {
			if (ctx->pipe->enabled_raw & (1 << i)) {
				master = (1 << i);
				break;
			}
		}
		if (ctx->pipe->stagger_path == STAGGER_ON_THE_FLY) {
			hw_scen = (1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER);
			req_amount = (mtk_cam_feature_is_3_exposure(feature_active)) ? 2 : 1;
		} else if (ctx->pipe->stagger_path == STAGGER_DCIF) {
			hw_scen = (1 << MTKCAM_IPI_HW_PATH_OFFLINE_SRT_DCIF_STAGGER);
			req_amount = (mtk_cam_feature_is_3_exposure(feature_active)) ? 3 : 2;
		} else if (ctx->pipe->stagger_path == STAGGER_OFFLINE) {
			hw_scen = (1 << MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER);
			req_amount = (mtk_cam_feature_is_3_exposure(feature_active)) ? 3 : 2;
		} else {
			hw_scen = (1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER);
			req_amount = (mtk_cam_feature_is_3_exposure(feature_active)) ? 2 : 1;
		}
		idle_pipes = get_available_sv_pipes(cam, hw_scen, req_amount, master, 0);
		/* cached used sv pipes */
		ctx->pipe->enabled_raw |= idle_pipes;
		if (idle_pipes == 0) {
			dev_info(cam->dev, "no available sv pipes(scen:%d/req_amount:%d)",
				hw_scen, req_amount);
			return -EINVAL;
		}
	} else if (config_pipe && mtk_cam_is_time_shared(ctx)) {
		int master = (1 << MTKCAM_SUBDEV_RAW_0);
		int hw_scen, req_amount, idle_pipes;

		for (i = MTKCAM_SUBDEV_RAW_START; i < MTKCAM_SUBDEV_RAW_END; i++) {
			if (ctx->pipe->enabled_raw & (1 << i)) {
				master = (1 << i);
				break;
			}
		}
		hw_scen = (1 << MTKCAM_IPI_HW_PATH_OFFLINE_M2M);
		req_amount = 1;
		idle_pipes = get_available_sv_pipes(cam, hw_scen, req_amount, master, 0);
		/* cached used sv pipes */
		ctx->pipe->enabled_raw |= idle_pipes;
		if (idle_pipes == 0) {
			dev_info(cam->dev, "no available sv pipes(scen:%d/req_amount:%d)",
				hw_scen, req_amount);
			return -EINVAL;
		}
	} else if (config_pipe && mtk_cam_is_stagger_m2m(ctx)) {
		int hw_scen, req_amount;

		hw_scen = (1 << MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER);
		req_amount = (mtk_cam_feature_is_3_exposure(feature_active)) ? 3 : 2;
	} else if (mtk_cam_is_with_w_channel(ctx)) {
		int master, hw_scen, req_amount, idle_pipes;

		master = (ctx->pipe->enabled_raw & (1 << MTKCAM_SUBDEV_RAW_0)) ?
			(1 << MTKCAM_SUBDEV_RAW_0) : (1 << MTKCAM_SUBDEV_RAW_1);
		hw_scen = (1 << MTKCAM_SV_SPECIAL_SCENARIO_ADDITIONAL_RAW);
		req_amount = 1;

		idle_pipes = get_available_sv_pipes(cam, hw_scen, req_amount, master, 0);
		/* cached used sv pipes */
		ctx->pipe->enabled_raw |= idle_pipes;
		if (idle_pipes == 0) {
			dev_info(cam->dev, "no available sv pipes(scen:%d/req_amount:%d)",
				hw_scen, req_amount);
			return -EINVAL;
		}
	}


	update_hw_mapping(ctx, &config_param, feature_active, ctx->pipe->stagger_path,
			  ctx->pipe->enabled_raw);
	if (mtk_cam_feature_is_mstream(feature_active) ||
	    mtk_cam_feature_is_mstream_m2m(feature_active)) {
		config_param.sw_feature = MTKCAM_IPI_SW_FEATURE_VHDR_MSTREAM;
		dev_dbg(dev, "%s sw_feature:%d", __func__, config_param.sw_feature);
	} else {
		config_param.sw_feature = (mtk_cam_feature_is_stagger(feature_active) == 1 ||
			mtk_cam_feature_is_stagger_m2m(feature_active) == 1) ?
			MTKCAM_IPI_SW_FEATURE_VHDR_STAGGER : MTKCAM_IPI_SW_FEATURE_NORMAL;
	}

	dev_raw = mtk_cam_find_raw_dev(cam, ctx->used_raw_dev);
	if (!dev_raw) {
		dev_info(dev, "config raw device not found\n");
		return -EINVAL;
	}
	raw_dev = dev_get_drvdata(dev_raw);
	for (i = 0; i < RAW_PIPELINE_NUM; i++)
		if (raw->pipelines[i].enabled_raw & 1 << raw_dev->id) {
			raw_dev->pipeline = &raw->pipelines[i];
			/* TWIN case */
			if (raw->pipelines[i].res_config.raw_num_used != 1) {
				struct mtk_raw_device *raw_dev_slave =
						get_slave_raw_dev(cam, ctx->pipe);
				raw_dev_slave->pipeline = &raw->pipelines[i];
				dev_dbg(dev, "twin master/slave raw_id:%d/%d\n",
					raw_dev->id, raw_dev_slave->id);
				if (raw->pipelines[i].res_config.raw_num_used == 3) {
					struct mtk_raw_device *raw_dev_slave2 =
						get_slave2_raw_dev(cam, ctx->pipe);
					raw_dev_slave2->pipeline = &raw->pipelines[i];
					dev_dbg(dev, "triplet m/s/s2 raw_id:%d/%d/%d\n",
						raw_dev->id, raw_dev_slave->id, raw_dev_slave2->id);
				}
			}
			break;
		}

	if (!streaming)
		reset(raw_dev);
	isp_composer_hw_config(cam, ctx, &config_param);
	dev_dbg(dev, "raw %d %s done\n", raw_dev->id, __func__);

	return 0;
}


static int mtk_cam_ctx_get_ipi_id(struct mtk_cam_ctx *ctx, unsigned int pipe_id)
{
	unsigned int ipi_id = CCD_IPI_ISP_MAIN + pipe_id - MTKCAM_SUBDEV_RAW_START;

	if (ipi_id < CCD_IPI_ISP_MAIN || ipi_id > CCD_IPI_ISP_TRICAM) {
		dev_info(ctx->cam->dev,
			"%s: mtk_raw_pipeline(%d), ipi_id(%d) invalid(min:%d,max:%d)\n",
			__func__, pipe_id, ipi_id, CCD_IPI_ISP_MAIN,
			CCD_IPI_ISP_TRICAM);
		return -EINVAL;
	}

	dev_dbg(ctx->cam->dev, "%s: mtk_raw_pipeline(%d), ipi_id(%d)\n",
		__func__, pipe_id, ipi_id);

	return ipi_id;
}

#if CCD_READY
static int isp_composer_init(struct mtk_cam_ctx *ctx, unsigned int pipe_id)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev = cam->dev;
	struct mtk_ccd *ccd;
	struct rproc_subdev *rpmsg_subdev;
	struct rpmsg_channel_info *msg = &ctx->rpmsg_channel;
	int ipi_id;

	/* Create message client */
	ccd = (struct mtk_ccd *)cam->rproc_handle->priv;
	rpmsg_subdev = ccd->rpmsg_subdev;

	ipi_id = mtk_cam_ctx_get_ipi_id(ctx, pipe_id);
	if (ipi_id < 0)
		return -EINVAL;

	snprintf(msg->name, RPMSG_NAME_SIZE, "mtk-camsys\%d", pipe_id);
	msg->src = ipi_id;
	ctx->rpmsg_dev = mtk_create_client_msgdevice(rpmsg_subdev, msg);
	if (!ctx->rpmsg_dev)
		return -EINVAL;

	ctx->rpmsg_dev->rpdev.ept = rpmsg_create_ept(&ctx->rpmsg_dev->rpdev,
						     isp_composer_handler,
						     cam, *msg);
	if (IS_ERR(ctx->rpmsg_dev->rpdev.ept)) {
		dev_info(dev, "%s failed rpmsg_create_ept, ctx:%d\n",
			 __func__, ctx->stream_id);
		goto faile_release_msg_dev;
	}

	dev_info(dev, "%s initialized composer of ctx:%d, ipi_id:%d\n",
		 __func__, ctx->stream_id, ipi_id);

	return 0;

faile_release_msg_dev:
	mtk_destroy_client_msgdevice(rpmsg_subdev, &ctx->rpmsg_channel);
	ctx->rpmsg_dev = NULL;
	return -EINVAL;
}
#endif

static int mtk_cam_runtime_suspend(struct device *dev)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	struct mtk_camsys_dvfs *dvfs_info =
				&cam_dev->camsys_ctrl.dvfs_info;

	dev_dbg(dev, "- %s\n", __func__);

	if (dvfs_info->reg_vmm && regulator_is_enabled(dvfs_info->reg_vmm))
		regulator_disable(dvfs_info->reg_vmm);

	return 0;
}

static int mtk_cam_runtime_resume(struct device *dev)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	struct mtk_camsys_dvfs *dvfs_info =
				&cam_dev->camsys_ctrl.dvfs_info;

	dev_dbg(dev, "- %s\n", __func__);

	if (dvfs_info->reg_vmm)
		regulator_enable(dvfs_info->reg_vmm);

	mtk_cam_timesync_init(true);

	return 0;
}

struct mtk_cam_ctx *mtk_cam_find_ctx(struct mtk_cam_device *cam,
				     struct media_entity *entity)
{
	unsigned int i;

	for (i = 0;  i < cam->max_stream_num; i++) {
		if (entity->pipe == &cam->ctxs[i].pipeline)
			return &cam->ctxs[i];
	}

	return NULL;
}

struct mtk_cam_ctx *mtk_cam_start_ctx(struct mtk_cam_device *cam,
				      struct mtk_cam_video_device *node)
{
	struct mtk_cam_ctx *ctx = node->ctx;
	struct media_graph *graph;
	struct v4l2_subdev **target_sd;
	int ret, i, is_first_ctx;
	struct media_entity *entity = &node->vdev.entity;

	dev_info(cam->dev, "%s:ctx(%d): triggered by %s\n",
		 __func__, ctx->stream_id, entity->name);

	atomic_set(&ctx->enqueued_frame_seq_no, 0);
	ctx->composed_frame_seq_no = 0;
	ctx->dequeued_frame_seq_no = 0;
	for (i = 0; i < MAX_SV_PIPES_PER_STREAM; i++)
		ctx->sv_dequeued_frame_seq_no[i] = 0;
	for (i = 0 ; i < MAX_MRAW_PIPES_PER_STREAM ; i++) {
		ctx->mraw_dequeued_frame_seq_no[i] = 0;
		ctx->mraw_processing_buffer_list[i].cnt = 0;
		ctx->mraw_composed_buffer_list[i].cnt = 0;
	}
	ctx->enqueued_request_cnt = 0;
	ctx->next_sof_mask_frame_seq_no = 0;
	ctx->working_request_seq = 0;
	atomic_set(&ctx->running_s_data_cnt, 0);
	init_completion(&ctx->session_complete);
	init_completion(&ctx->m2m_complete);

	is_first_ctx = !cam->composer_cnt;
	if (is_first_ctx) {
		cam->running_job_count = 0;

		dev_info(cam->dev, "%s: power on camsys\n", __func__);
		pm_runtime_get_sync(cam->dev);

		/* power on the remote proc device */
		if (!cam->rproc_handle)  {
			/* Get the remote proc device of composers */
			cam->rproc_handle =
				rproc_get_by_phandle(cam->rproc_phandle);
			if (!cam->rproc_handle) {
				dev_info(cam->dev,
					"fail to get rproc_handle\n");
				return NULL;
			}
			/* Power on remote proc device of composers*/
			ret = rproc_boot(cam->rproc_handle);
			if (ret) {
				dev_info(cam->dev,
					"failed to rproc_boot:%d\n", ret);
				goto fail_rproc_put;
			}
		}

		/* To catch camsys exception and trigger dump */
		if (cam->debug_fs)
			cam->debug_fs->ops->exp_reinit(cam->debug_fs);
	}

#if CCD_READY
	if (node->uid.pipe_id >= MTKCAM_SUBDEV_RAW_START &&
		node->uid.pipe_id < MTKCAM_SUBDEV_RAW_END) {
		ret = isp_composer_init(ctx, node->uid.pipe_id);
		if (ret)
			goto fail_shutdown;
	}
#endif
	cam->composer_cnt++;

	ret = mtk_cam_working_buf_pool_init(ctx);
	if (ret) {
		dev_info(cam->dev, "failed to reserve DMA memory:%d\n", ret);
		goto fail_uninit_composer;
	}

	kthread_init_worker(&ctx->sensor_worker);
	ctx->sensor_worker_task = kthread_run(kthread_worker_fn,
					      &ctx->sensor_worker,
					      "sensor_worker-%d",
					      ctx->stream_id);
	if (IS_ERR(ctx->sensor_worker_task)) {
		dev_info(cam->dev, "%s:ctx(%d): could not create sensor_worker_task\n",
			 __func__, ctx->stream_id);
		goto fail_release_buffer_pool;
	}

	sched_set_fifo(ctx->sensor_worker_task);

	ctx->composer_wq =
			alloc_ordered_workqueue(dev_name(cam->dev),
						WQ_HIGHPRI | WQ_FREEZABLE);
	if (!ctx->composer_wq) {
		dev_info(cam->dev, "failed to alloc composer workqueue\n");
		goto fail_uninit_sensor_worker_task;
	}

	ctx->frame_done_wq =
			alloc_ordered_workqueue(dev_name(cam->dev),
						WQ_HIGHPRI | WQ_FREEZABLE);
	if (!ctx->frame_done_wq) {
		dev_info(cam->dev, "failed to alloc frame_done workqueue\n");
		goto fail_uninit_composer_wq;
	}

	ctx->sv_wq =
			alloc_ordered_workqueue(dev_name(cam->dev),
						WQ_HIGHPRI | WQ_FREEZABLE);
	if (!ctx->sv_wq) {
		dev_info(cam->dev, "failed to alloc sv workqueue\n");
		goto fail_uninit_frame_done_wq;
	}

	mtk_cam_sv_working_buf_pool_init(ctx);

	mtk_cam_mraw_working_buf_pool_init(ctx);

	ret = media_pipeline_start(entity, &ctx->pipeline);
	if (ret) {
		dev_info(cam->dev,
			 "%s:pipe(%d):failed in media_pipeline_start:%d\n",
			 __func__, node->uid.pipe_id, ret);
		goto fail_uninit_sv_wq;
	}

	/* traverse to update used subdevs & number of nodes */
	graph = &ctx->pipeline.graph;
	media_graph_walk_start(graph, entity);

	i = 0;
	while ((entity = media_graph_walk_next(graph))) {
		dev_dbg(cam->dev, "linked entity %s\n", entity->name);

		target_sd = NULL;

		switch (entity->function) {
		case MEDIA_ENT_F_IO_V4L:
			ctx->enabled_node_cnt++;
			break;
		case MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER: /* pipeline */
			if (i >= MAX_PIPES_PER_STREAM)
				goto fail_stop_pipeline;

			target_sd = ctx->pipe_subdevs + i;
			i++;
			break;
		case MEDIA_ENT_F_VID_IF_BRIDGE: /* seninf */
			target_sd = &ctx->seninf;
			break;
		case MEDIA_ENT_F_CAM_SENSOR:
			target_sd = &ctx->sensor;
			break;
		default:
			break;
		}

		if (!target_sd)
			continue;

		if (*target_sd) {
			dev_info(cam->dev, "duplicated subdevs!!!\n");
			goto fail_stop_pipeline;
		}

		if (is_media_entity_v4l2_subdev(entity))
			*target_sd = media_entity_to_v4l2_subdev(entity);
	}

	return ctx;

fail_stop_pipeline:
	media_pipeline_stop(entity);
fail_uninit_sv_wq:
	destroy_workqueue(ctx->sv_wq);
fail_uninit_frame_done_wq:
	destroy_workqueue(ctx->frame_done_wq);
fail_uninit_composer_wq:
	destroy_workqueue(ctx->composer_wq);
fail_uninit_sensor_worker_task:
	kthread_stop(ctx->sensor_worker_task);
	ctx->sensor_worker_task = NULL;
fail_release_buffer_pool:
	mtk_cam_working_buf_pool_release(ctx);
fail_uninit_composer:
#if CCD_READY
	if (node->uid.pipe_id >= MTKCAM_SUBDEV_RAW_START &&
		node->uid.pipe_id < MTKCAM_SUBDEV_RAW_END) {
		isp_composer_uninit(ctx);
		cam->composer_cnt--;
	}
fail_shutdown:
	if (is_first_ctx) {
		pm_runtime_mark_last_busy(cam->dev);
		pm_runtime_put_sync_autosuspend(cam->dev);
		rproc_shutdown(cam->rproc_handle);
	}
fail_rproc_put:
	if (is_first_ctx) {
		rproc_put(cam->rproc_handle);
		cam->rproc_handle = NULL;
		cam->composer_cnt = 0;
	}
#endif
	return NULL;
}

void mtk_cam_stop_ctx(struct mtk_cam_ctx *ctx, struct media_entity *entity)
{
	struct mtk_cam_device *cam = ctx->cam;
	unsigned int i, j;

	dev_info(cam->dev, "%s:ctx(%d): triggered by %s\n",
		 __func__, ctx->stream_id, entity->name);

	media_pipeline_stop(entity);

	/* Consider scenario that stop the ctx while the ctx is not streamed on */
	if (ctx->session_created) {
		dev_dbg(cam->dev,
			"%s:ctx(%d): session_created, wait for composer session destroy\n",
			__func__, ctx->stream_id);
		if (wait_for_completion_timeout(
			&ctx->session_complete, msecs_to_jiffies(300)) == 0)
			dev_info(cam->dev, "%s:ctx(%d): complete timeout\n",
			__func__, ctx->stream_id);
	}

	/* For M2M feature, signal all waiters */
	if (mtk_cam_is_m2m(ctx))
		complete_all(&ctx->m2m_complete);

	if (!cam->streaming_ctx) {
		struct v4l2_subdev *sd;

		v4l2_device_for_each_subdev(sd, &cam->v4l2_dev) {
			if (sd->entity.function == MEDIA_ENT_F_VID_IF_BRIDGE) {
				int ret;

				ret = v4l2_subdev_call(sd, video, s_stream, 0);
				if (ret)
					dev_info(cam->dev,
						 "failed to streamoff %s:%d\n",
						 sd->name, ret);
				sd->entity.stream_count = 0;
				sd->entity.pipe = NULL;
			} else if (sd->entity.function ==
						MEDIA_ENT_F_CAM_SENSOR) {
				sd->entity.stream_count = 0;
				sd->entity.pipe = NULL;
			}
		}
	}

	drain_workqueue(ctx->composer_wq);
	destroy_workqueue(ctx->composer_wq);
	ctx->composer_wq = NULL;
	drain_workqueue(ctx->frame_done_wq);
	destroy_workqueue(ctx->frame_done_wq);
	ctx->frame_done_wq = NULL;
	drain_workqueue(ctx->sv_wq);
	destroy_workqueue(ctx->sv_wq);
	ctx->sv_wq = NULL;
	kthread_flush_worker(&ctx->sensor_worker);
	kthread_stop(ctx->sensor_worker_task);
	ctx->sensor_worker_task = NULL;

	for (i = 0 ; i < ctx->used_sv_num ; i++) {
		for (j = 0 ; j < cam->max_stream_num ; j++) {
			if (ctx->sv_pipe[i]->id == cam->ctxs[j].stream_id) {
				atomic_set(&cam->ctxs[j].enqueued_frame_seq_no, 0);
				break;
			}
		}
	}

	for (i = 0 ; i < ctx->used_mraw_num ; i++) {
		for (j = 0 ; j < cam->max_stream_num ; j++) {
			if (ctx->mraw_pipe[i]->id == cam->ctxs[j].stream_id) {
				atomic_set(&cam->ctxs[j].enqueued_frame_seq_no, 0);
				break;
			}
		}
	}

	for (i = 0 ; i < ctx->used_mraw_num ; i++)
		ctx->mraw_pipe[i]->res_config.is_initial = 1;

	ctx->session_created = 0;
	ctx->enabled_node_cnt = 0;
	ctx->streaming_node_cnt = 0;
	ctx->streaming_pipe = 0;
	ctx->sensor = NULL;
	ctx->prev_sensor = NULL;
	ctx->seninf = NULL;
	ctx->prev_seninf = NULL;
	atomic_set(&ctx->enqueued_frame_seq_no, 0);
	ctx->enqueued_request_cnt = 0;
	ctx->next_sof_mask_frame_seq_no = 0;
	ctx->working_request_seq = 0;
	ctx->composed_frame_seq_no = 0;
	ctx->is_first_cq_done = 0;
	ctx->cq_done_status = 0;
	ctx->used_raw_num = 0;
	ctx->used_sv_num = 0;
	ctx->used_mraw_num = 0;
	if (ctx->pipe) {
		ctx->pipe->enabled_raw = 0;
		ctx->pipe->enabled_dmas = 0;
	}

	INIT_LIST_HEAD(&ctx->using_buffer_list.list);
	INIT_LIST_HEAD(&ctx->composed_buffer_list.list);
	INIT_LIST_HEAD(&ctx->processing_buffer_list.list);

	for (i = 0; i < MAX_SV_PIPES_PER_STREAM; i++) {
		INIT_LIST_HEAD(&ctx->sv_using_buffer_list[i].list);
		INIT_LIST_HEAD(&ctx->sv_processing_buffer_list[i].list);
	}

	for (i = 0; i < MAX_MRAW_PIPES_PER_STREAM; i++) {
		INIT_LIST_HEAD(&ctx->mraw_using_buffer_list[i].list);
		INIT_LIST_HEAD(&ctx->mraw_composed_buffer_list[i].list);
		INIT_LIST_HEAD(&ctx->mraw_processing_buffer_list[i].list);
	}

	INIT_LIST_HEAD(&ctx->processing_img_buffer_list.list);
	for (i = 0; i < MAX_PIPES_PER_STREAM; i++)
		ctx->pipe_subdevs[i] = NULL;

	for (i = 0; i < MAX_SV_PIPES_PER_STREAM; i++) {
		ctx->sv_pipe[i] = NULL;
		ctx->used_sv_dev[i] = 0;
	}

	for (i = 0; i < MAX_MRAW_PIPES_PER_STREAM; i++) {
		ctx->mraw_pipe[i] = NULL;
		ctx->used_mraw_dev[i] = 0;
	}

	isp_composer_uninit(ctx);
	cam->composer_cnt--;

	dev_info(cam->dev, "%s: ctx-%d:  composer_cnt:%d\n",
		__func__, ctx->stream_id, cam->composer_cnt);

	mtk_cam_working_buf_pool_release(ctx);

	if (ctx->cam->rproc_handle && !ctx->cam->composer_cnt) {
		dev_info(cam->dev, "%s power off camsys\n", __func__);
		pm_runtime_mark_last_busy(cam->dev);
		pm_runtime_put_sync_autosuspend(cam->dev);
#if CCD_READY
		rproc_shutdown(cam->rproc_handle);
		rproc_put(cam->rproc_handle);
		cam->rproc_handle = NULL;
#endif
	}
}
int PipeIDtoTGIDX(int pipe_id)
{
	/* camsv/mraw's cammux id is defined in its own dts */

	switch (pipe_id) {
	case MTKCAM_SUBDEV_RAW_0:
					return 0;
	case MTKCAM_SUBDEV_RAW_1:
					return 1;
	case MTKCAM_SUBDEV_RAW_2:
					return 2;
	default:
			break;
	}
	return -1;
}

int mtk_cam_call_seninf_set_pixelmode(struct mtk_cam_ctx *ctx,
				      struct v4l2_subdev *sd,
				      int pad_id, int pixel_mode)
{
	int ret;

	ret = mtk_cam_seninf_set_pixelmode(sd, pad_id, pixel_mode);
	dev_dbg(ctx->cam->dev,
		"%s:ctx(%d): seninf(%s): pad(%d), pixel_mode(%d)\n, ret(%d)",
		__func__, ctx->stream_id, sd->name, pad_id, pixel_mode,
		ret);

	return ret;
}

int mtk_cam_ctx_stream_on(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev;
	struct mtk_raw_device *raw_dev;
	struct mtk_camsv_device *camsv_dev;
	int i, j, ret;
	int tgo_pxl_mode;
	bool need_dump_mem = false;
	int feature_active = 0;	/* Used to know max exposure num */
	int feature_first_req = 0;	/* Used to know first frame's exposure num */

	dev_info(cam->dev, "ctx %d stream on, streaming_pipe:0x%x\n",
		 ctx->stream_id, ctx->streaming_pipe);

	if (ctx->streaming) {
		dev_info(cam->dev, "ctx-%d is already streaming on\n", ctx->stream_id);
		return 0;
	}

	for (i = 0; i < MAX_PIPES_PER_STREAM && ctx->pipe_subdevs[i]; i++) {
		ret = v4l2_subdev_call(ctx->pipe_subdevs[i], video,
				       s_stream, 1);
		if (ret) {
			dev_info(cam->dev, "failed to stream on %d: %d\n",
				 ctx->pipe_subdevs[i]->name, ret);
			goto fail_pipe_off;
		}
	}

	if (ctx->pipe) {
		feature_active = ctx->pipe->feature_active;
		feature_first_req = ctx->pipe->feature_pending;
	}

	if (ctx->used_raw_num) {
		tgo_pxl_mode = ctx->pipe->res_config.tgo_pxl_mode;
		/**
		 * TODO: validate pad's setting of each pipes
		 * return -EPIPE if failed
		 */
		if (ctx->pipe->dynamic_exposure_num_max > 1 ||
		    mtk_cam_feature_is_switchable_hdr(feature_active))
			ret = mtk_cam_img_working_buf_pool_init(ctx,
				2 + mtk_cam_feature_is_3_exposure(feature_active));
		if (mtk_cam_feature_is_time_shared(feature_active))
			ret = mtk_cam_img_working_buf_pool_init(ctx, CAM_IMG_BUF_NUM);
		if (ret) {
			dev_info(cam->dev, "failed to reserve DMA memory:%d\n", ret);
			goto fail_img_buf_release;
		}

		ret = mtk_cam_dev_config(ctx, false, true);
		if (ret)
			goto fail_img_buf_release;
		dev = mtk_cam_find_raw_dev(cam, ctx->used_raw_dev);
		if (!dev) {
			dev_info(cam->dev, "streamon raw device not found\n");
			goto fail_img_buf_release;
		}
		raw_dev = dev_get_drvdata(dev);

		if (mtk_cam_is_hsf(ctx)) {
			initialize(raw_dev, 0);
			/* Stagger */
			if (mtk_cam_feature_is_stagger(feature_first_req))
				stagger_enable(raw_dev);
			/* Sub sample */
			if (mtk_cam_feature_is_subsample(feature_active))
				subsample_enable(raw_dev);
			/* Twin */
			if (ctx->pipe->res_config.raw_num_used != 1) {
				struct mtk_raw_device *raw_dev_slave =
							get_slave_raw_dev(cam, ctx->pipe);
				initialize(raw_dev_slave, 1);
				if (ctx->pipe->res_config.raw_num_used == 3) {
					struct mtk_raw_device *raw_dev_slave2 =
						get_slave2_raw_dev(cam, ctx->pipe);
					initialize(raw_dev_slave2, 1);
				}
			}
		}

		/* stagger mode - use sv to output data to DRAM - online mode */
		if (mtk_cam_feature_is_stagger(feature_active)) {
			int used_pipes, src_pad_idx, exp_no;
			unsigned int hw_scen = mtk_raw_get_hdr_scen_id(ctx);
			bool bDcif;

			/* check exposure number */
			if (mtk_cam_feature_is_2_exposure(feature_active))
				exp_no = 2;
			else if (mtk_cam_feature_is_3_exposure(feature_active))
				exp_no = 3;
			else
				exp_no = 1;

			/* check stagger mode */
			bDcif = (ctx->pipe->stagger_path == STAGGER_DCIF) ? true : false;

			used_pipes = ctx->pipe->enabled_raw;
			for (i = MTKCAM_SUBDEV_CAMSV_START ; i < MTKCAM_SUBDEV_CAMSV_END ; i++) {
				for (j = 0; j < MAX_STAGGER_EXP_AMOUNT; j++) {
					if (cam->sv.pipelines[i -
						MTKCAM_SUBDEV_CAMSV_START].hw_cap &
						(1 << (j + CAMSV_EXP_ORDER_SHIFT))) {
						src_pad_idx = PAD_SRC_RAW0 + j;
						break;
					}
				}
				if (used_pipes & (1 << i)) {
					//HSF control
					if (mtk_cam_is_hsf(ctx)) {
						dev_info(cam->dev, "error: un-support hsf stagger mode\n");
						goto fail_img_buf_release;
					}
					mtk_cam_call_seninf_set_pixelmode(ctx, ctx->seninf,
									  src_pad_idx,
									  tgo_pxl_mode);
					mtk_cam_seninf_set_camtg(ctx->seninf, src_pad_idx,
						cam->sv.pipelines[
							i - MTKCAM_SUBDEV_CAMSV_START].cammux_id);
					dev_info(cam->dev, "seninf_set_camtg(src_pad:%d/i:%d/camtg:%d)",
						src_pad_idx, i,
						cam->sv.pipelines[
							i - MTKCAM_SUBDEV_CAMSV_START].cammux_id);
					ret = mtk_cam_sv_dev_config(
						ctx, i - MTKCAM_SUBDEV_CAMSV_START, hw_scen,
						(bDcif && (src_pad_idx == exp_no)) ?
						2 : src_pad_idx - PAD_SRC_RAW0);
					if (ret)
						goto fail_img_buf_release;
				}
			}
		} else if (mtk_cam_feature_is_time_shared(feature_active)) {
			int used_pipes, src_pad_idx, hw_scen;
			hw_scen = (1 << MTKCAM_IPI_HW_PATH_OFFLINE_M2M);
			src_pad_idx = PAD_SRC_RAW0;
			used_pipes = ctx->pipe->enabled_raw;
			for (i = MTKCAM_SUBDEV_CAMSV_START ; i < MTKCAM_SUBDEV_CAMSV_END ; i++) {
				if (used_pipes & (1 << i)) {
					//HSF control
					if (mtk_cam_is_hsf(ctx)) {
						dev_info(cam->dev, "error: un-support hsf stagger mode\n");
						goto fail_img_buf_release;
					}
					mtk_cam_call_seninf_set_pixelmode(ctx,
									  ctx->seninf,
									  src_pad_idx,
									  tgo_pxl_mode);
					mtk_cam_seninf_set_camtg(ctx->seninf, src_pad_idx,
						cam->sv.pipelines[
							i - MTKCAM_SUBDEV_CAMSV_START].cammux_id);
					ret = mtk_cam_sv_dev_config(
						ctx, i - MTKCAM_SUBDEV_CAMSV_START, hw_scen,
						0);
					if (ret)
						goto fail_img_buf_release;
					src_pad_idx++;
					dev_info(cam->dev, "[TS] scen:0x%x/enabled_raw:0x%x/i(%d)",
					hw_scen, ctx->pipe->enabled_raw, i);
				}
			}
		} else if (mtk_cam_is_with_w_channel(ctx)) {
			for (i = MTKCAM_SUBDEV_CAMSV_START ; i < MTKCAM_SUBDEV_CAMSV_END ; i++) {
				if (ctx->pipe->enabled_raw & 1 << i) {
					int hw_scen =
						(1 << MTKCAM_SV_SPECIAL_SCENARIO_ADDITIONAL_RAW);

					mtk_cam_call_seninf_set_pixelmode(ctx, ctx->seninf,
						PAD_SRC_RAW_W0, tgo_pxl_mode);
					mtk_cam_seninf_set_camtg(ctx->seninf, PAD_SRC_RAW_W0,
						cam->sv.pipelines[
							i - MTKCAM_SUBDEV_CAMSV_START].cammux_id);
					dev_info(cam->dev,
						"seninf_set_camtg(src_pad:%d/i:%d/camtg:%d)",
						PAD_SRC_RAW_W0, i,
						cam->sv.pipelines[
							i - MTKCAM_SUBDEV_CAMSV_START].cammux_id);
					ret = mtk_cam_sv_dev_config(ctx,
							i - MTKCAM_SUBDEV_CAMSV_START,
							hw_scen, 0);
					if (ret)
						goto fail_img_buf_release;
					break;
				}
			}
		}

		/*set cam mux camtg and pixel mode*/
		if (mtk_cam_feature_is_stagger(feature_active)) {
			if (ctx->pipe->stagger_path == STAGGER_ON_THE_FLY) {
				int seninf_pad;

				if (mtk_cam_feature_is_2_exposure(feature_first_req))
					seninf_pad = PAD_SRC_RAW1;
				else if (mtk_cam_feature_is_3_exposure(feature_first_req))
					seninf_pad = PAD_SRC_RAW2;
				else
					seninf_pad = PAD_SRC_RAW0;

				/* todo: backend support one pixel mode only */
				mtk_cam_call_seninf_set_pixelmode(ctx,
								  ctx->seninf,
								  seninf_pad,
								  tgo_pxl_mode);
				mtk_cam_seninf_set_camtg(ctx->seninf, seninf_pad,
							PipeIDtoTGIDX(raw_dev->id));
			}
		} else if (!mtk_cam_feature_is_m2m(feature_active) &&
					!mtk_cam_feature_is_time_shared(feature_active)) {
			if (mtk_cam_is_hsf(ctx)) {
				//HSF control
				dev_info(cam->dev, "enabled_hsf_raw =%d\n",
					ctx->pipe->res_config.enable_hsf_raw);
					ret = mtk_cam_hsf_config(ctx, raw_dev->id);
					if (ret != 0) {
						dev_info(cam->dev, "Error:enabled_hsf fail\n");
						goto fail_img_buf_release;
					}

			}
#ifdef USING_HSF_SENSOR
			if (mtk_cam_is_hsf(ctx))
				mtk_cam_seninf_set_secure(ctx->seninf, 1,
					ctx->hsf->share_buf->chunk_hsfhandle);
			else
				mtk_cam_seninf_set_secure(ctx->seninf, 0, 0);
#endif
			mtk_cam_call_seninf_set_pixelmode(ctx, ctx->seninf,
							  PAD_SRC_RAW0,
							  tgo_pxl_mode);
			mtk_cam_seninf_set_camtg(ctx->seninf, PAD_SRC_RAW0,
						 PipeIDtoTGIDX(raw_dev->id));
		}
	}

	if (!mtk_cam_feature_is_m2m(feature_active)) {
		for (i = 0 ; i < ctx->used_sv_num ; i++) {
			/* use 8-pixel mode as default */
			mtk_cam_call_seninf_set_pixelmode(ctx,
							  ctx->seninf,
							  ctx->sv_pipe[i]->seninf_padidx, 3);
			mtk_cam_seninf_set_camtg(ctx->seninf,
						 ctx->sv_pipe[i]->seninf_padidx,
						 ctx->sv_pipe[i]->cammux_id);
			ret = mtk_cam_sv_dev_config(ctx, i, 1, 0);
			if (ret)
				goto fail_img_buf_release;
		}

		for (i = 0 ; i < ctx->used_mraw_num ; i++) {
			/* use 1-pixel mode as default */
			ctx->mraw_pipe[i]->res_config.pixel_mode = 1;
			mtk_cam_call_seninf_set_pixelmode(ctx, ctx->seninf,
				ctx->mraw_pipe[i]->seninf_padidx, 0);
			mtk_cam_seninf_set_camtg(ctx->seninf, ctx->mraw_pipe[i]->seninf_padidx,
				ctx->mraw_pipe[i]->cammux_id);

			dev_dbg(cam->dev, "Set mraw pad(%d) to pixel_mode(%d)\n",
				ctx->mraw_pipe[i]->seninf_padidx,
				ctx->mraw_pipe[i]->res_config.pixel_mode);
			ret = mtk_cam_mraw_dev_config(ctx, i);
			if (ret)
				goto fail_img_buf_release;
		}

		ret = v4l2_subdev_call(ctx->seninf, video, s_stream, 1);
		if (ret) {
			dev_info(cam->dev, "failed to stream on seninf %s:%d\n",
				 ctx->seninf->name, ret);
			goto fail_img_buf_release;
		}
	} else {
		ctx->processing_buffer_list.cnt = 0;
		ctx->composed_buffer_list.cnt = 0;
		dev_dbg(cam->dev, "[M2M] reset processing_buffer_list.cnt & composed_buffer_list.cnt\n");
	}
	if (ctx->used_raw_num) {
		if (!mtk_cam_is_hsf(ctx)) {
			initialize(raw_dev, 0);
			/* Stagger */
			if (mtk_cam_feature_is_stagger(feature_first_req))
				stagger_enable(raw_dev);
			/* Sub sample */
			if (mtk_cam_feature_is_subsample(feature_active))
				subsample_enable(raw_dev);
			/* Twin */
			if (ctx->pipe->res_config.raw_num_used != 1) {
				struct mtk_raw_device *raw_dev_slave =
				get_slave_raw_dev(cam, ctx->pipe);
				initialize(raw_dev_slave, 1);
				if (ctx->pipe->res_config.raw_num_used == 3) {
					struct mtk_raw_device *raw_dev_slave2 =
						get_slave2_raw_dev(cam, ctx->pipe);
					initialize(raw_dev_slave2, 1);
				}
			}
		}
	}
	/* TODO */
	spin_lock(&ctx->streaming_lock);
	if (!cam->streaming_ctx && cam->debug_fs)
		need_dump_mem = true;
	else
		dev_dbg(cam->dev,
			"No need to alloc mem for ctx: streaming_ctx(0x%x), debug_fs(%p)\n",
			cam->streaming_ctx, cam->debug_fs);
	ctx->streaming = true;
	cam->streaming_ctx |= 1 << ctx->stream_id;

	spin_unlock(&ctx->streaming_lock);
	if (need_dump_mem)
		cam->debug_fs->ops->reinit(cam->debug_fs, ctx->stream_id);
	/* update dvfs */
	if (ctx->used_raw_num)
		mtk_cam_dvfs_update_clk(ctx->cam);

	ret = mtk_camsys_ctrl_start(ctx);
	if (ret)
		goto fail_streaming_off;

	mutex_lock(&cam->queue_lock);
	mtk_cam_dev_req_try_queue(cam);  /* request moved into working list */
	mutex_unlock(&cam->queue_lock);
	/* raw off, no cq done, so sv on after enque */
	if (ctx->used_raw_num == 0) {
		for (i = 0 ; i < ctx->used_sv_num ; i++) {
			camsv_dev = get_camsv_dev(cam, ctx->sv_pipe[i]);
			camsv_dev->is_enqueued = 1;
			ret = mtk_cam_sv_dev_stream_on(ctx, i, 1, 1);
			if (ret)
				goto fail_sv_stream_off;
		}
	}

	dev_dbg(cam->dev, "streamed on camsys ctx:%d\n", ctx->stream_id);

	if (raw_dev)
		mtk_cam_register_iommu_tf_callback(raw_dev);

	return 0;

fail_sv_stream_off:
	if (ctx->used_raw_num == 0) {
		for (i = 0 ; i < ctx->used_sv_num ; i++) {
			camsv_dev = get_camsv_dev(cam, ctx->sv_pipe[i]);
			camsv_dev->is_enqueued = 0;
			ret = mtk_cam_sv_dev_stream_on(ctx, i, 0, 1);
			if (ret)
				dev_info(cam->dev,
					 "%s:failed to stream off camsv(%d)\n",
					 __func__, i);
		}
	}
	mtk_camsys_ctrl_stop(ctx);
fail_streaming_off:
	spin_lock(&ctx->streaming_lock);
	ctx->streaming = false;
	cam->streaming_ctx &= ~(1 << ctx->stream_id);
	spin_unlock(&ctx->streaming_lock);
	/* reset dvfs/qos */
	if (!mtk_cam_is_m2m(ctx))
		v4l2_subdev_call(ctx->seninf, video, s_stream, 0);
fail_img_buf_release:
	if (ctx->img_buf_pool.working_img_buf_size > 0)
		mtk_cam_img_working_buf_pool_release(ctx);
fail_pipe_off:
	for (i = 0; i < MAX_PIPES_PER_STREAM && ctx->pipe_subdevs[i]; i++)
		v4l2_subdev_call(ctx->pipe_subdevs[i], video, s_stream, 0);

	return ret;
}

static int mtk_cam_ts_are_all_ctx_off(struct mtk_cam_device *cam,
			struct mtk_cam_ctx *ctx)
{
	struct mtk_raw_pipeline *pipe_chk, *pipe;
	struct mtk_cam_ctx *ctx_chk;
	int i;
	int ts_id, ts_id_chk;
	int ret = true;

	for (i = 0;  i < cam->max_stream_num; i++) {
		ctx_chk = cam->ctxs + i;
		if (ctx_chk == ctx)
			continue;
		if (ctx_chk->pipe) {
			pipe = ctx->pipe;
			ts_id = pipe->feature_active &
					MTK_CAM_FEATURE_TIMESHARE_MASK;
			pipe_chk = ctx_chk->pipe;
			ts_id_chk = pipe_chk->feature_active &
					MTK_CAM_FEATURE_TIMESHARE_MASK;
			dev_info(cam->dev, "[%s] i:%d pipe/ts:%d/0x%x chk_pipe/ts:%d/0x%x\n",
				__func__, i, pipe->id, ts_id, pipe_chk->id, ts_id_chk);
			if (ts_id == ts_id_chk) {
				if (ctx_chk->streaming)
					ret = false;
			}
		}
	}

	return ret;
}

int mtk_cam_ctx_stream_off(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev;
	struct mtk_raw_device *raw_dev;
	unsigned int i, enabled_sv = 0;
	int ret;
	int feature = 0;

	if (!ctx->streaming) {
		dev_info(cam->dev, "ctx-%d is already streaming off\n",
			 ctx->stream_id);
		return 0;
	}

	if (ctx->pipe)
		feature = ctx->pipe->feature_active;

	if (watchdog_scenario(ctx))
		mtk_ctx_watchdog_stop(ctx);

	dev_info(cam->dev, "%s: ctx-%d:  composer_cnt:%d, streaming_pipe:0x%x\n",
		__func__, ctx->stream_id, cam->composer_cnt, ctx->streaming_pipe);

	spin_lock(&ctx->streaming_lock);
	ctx->streaming = false;
	cam->streaming_ctx &= ~(1 << ctx->stream_id);
	spin_unlock(&ctx->streaming_lock);

	if (ctx->synced) {
		/* after streaming being off, no one can do V4L2_CID_FRAME_SYNC */
		struct v4l2_ctrl *ctrl;

		ctrl = v4l2_ctrl_find(ctx->sensor->ctrl_handler,
				      V4L2_CID_FRAME_SYNC);
		if (ctrl) {
			v4l2_ctrl_s_ctrl(ctrl, 0);
			dev_info(cam->dev,
				 "%s: ctx(%d): apply V4L2_CID_FRAME_SYNC(0)\n",
				 __func__, ctx->stream_id);
		} else {
			dev_info(cam->dev,
				 "%s: ctx(%d): failed to find V4L2_CID_FRAME_SYNC\n",
				 __func__, ctx->stream_id);
		}
		ctx->synced = 0;
	}

	if (!mtk_cam_feature_is_m2m(feature)) {
		ret = v4l2_subdev_call(ctx->seninf, video, s_stream, 0);
		if (ret) {
			dev_info(cam->dev, "failed to stream off %s:%d\n",
				 ctx->seninf->name, ret);
			return -EPERM;
		}
	}

	if (ctx->used_raw_num) {
		if (mtk_cam_is_hsf(ctx)) {
			ret = mtk_cam_hsf_uninit(ctx);
			if (ret != 0) {
				dev_info(cam->dev, "failed to stream off %s:%d mtk_cam_hsf_uninit fail\n",
					 ctx->seninf->name, ret);
				return -EPERM;
			}
		}
		dev = mtk_cam_find_raw_dev(cam, ctx->used_raw_dev);
		if (!dev) {
			dev_info(cam->dev, "streamoff raw device not found\n");
			goto fail_stream_off;
		}
		raw_dev = dev_get_drvdata(dev);
		if (mtk_cam_feature_is_time_shared(feature)) {
			unsigned int hw_scen =
				(1 << MTKCAM_IPI_HW_PATH_OFFLINE_M2M);
			for (i = MTKCAM_SUBDEV_CAMSV_START ; i < MTKCAM_SUBDEV_CAMSV_END ; i++) {
				if (ctx->pipe->enabled_raw & (1 << i)) {
					mtk_cam_sv_dev_stream_on(
						ctx, i - MTKCAM_SUBDEV_CAMSV_START, 0, hw_scen);
					cam->sv.pipelines[
						i - MTKCAM_SUBDEV_CAMSV_START].is_occupied = 0;
					ctx->pipe->enabled_raw &= ~(1 << i);
					enabled_sv |= (1 << i);
				}
			}
			if (mtk_cam_ts_are_all_ctx_off(cam, ctx))
				stream_on(raw_dev, 0);
		} else {
			stream_on(raw_dev, 0);
		}
		/* Twin */
		if (ctx->pipe->res_config.raw_num_used != 1) {
			struct mtk_raw_device *raw_dev_slave =
						get_slave_raw_dev(cam, ctx->pipe);
			stream_on(raw_dev_slave, 0);
			if (ctx->pipe->res_config.raw_num_used == 3) {
				struct mtk_raw_device *raw_dev_slave2 =
					get_slave2_raw_dev(cam, ctx->pipe);
				stream_on(raw_dev_slave2, 0);
			}
		}
	}
	for (i = 0 ; i < ctx->used_sv_num ; i++) {
		ret = mtk_cam_sv_dev_stream_on(ctx, i, 0, 1);
		if (ret)
			return ret;
	}
	if (mtk_cam_feature_is_stagger(feature)) {
		unsigned int hw_scen = mtk_raw_get_hdr_scen_id(ctx);

		for (i = MTKCAM_SUBDEV_CAMSV_START; i < MTKCAM_SUBDEV_CAMSV_END; i++) {
			if (ctx->pipe->enabled_raw & (1 << i)) {
				mtk_cam_sv_dev_stream_on(
					ctx, i - MTKCAM_SUBDEV_CAMSV_START, 0, hw_scen);
				cam->sv.pipelines[i - MTKCAM_SUBDEV_CAMSV_START].is_occupied = 0;
				ctx->pipe->enabled_raw &= ~(1 << i);
				enabled_sv |= (1 << i);
			}
		}
	} else if (mtk_cam_is_with_w_channel(ctx)) {
		unsigned int hw_scen = (1 << MTKCAM_SV_SPECIAL_SCENARIO_ADDITIONAL_RAW);

		for (i = MTKCAM_SUBDEV_CAMSV_START; i < MTKCAM_SUBDEV_CAMSV_END; i++) {
			if (ctx->pipe->enabled_raw & (1 << i)) {
				mtk_cam_sv_dev_stream_on(
					ctx, i - MTKCAM_SUBDEV_CAMSV_START, 0, hw_scen);
				cam->sv.pipelines[i - MTKCAM_SUBDEV_CAMSV_START].is_occupied = 0;
				ctx->pipe->enabled_raw &= ~(1 << i);
				enabled_sv |= (1 << i);
			}
		}
	}

	for (i = 0 ; i < ctx->used_mraw_num ; i++) {
		ret = mtk_cam_mraw_dev_stream_on(ctx, i, 0);
		if (ret)
			return ret;
	}

	/* reset dvfs/qos */
	if (ctx->used_raw_num) {
		mtk_cam_dvfs_update_clk(ctx->cam);
		mtk_cam_qos_bw_reset(ctx, enabled_sv);
	}

	for (i = 0; i < MAX_PIPES_PER_STREAM && ctx->pipe_subdevs[i]; i++) {
		ret = v4l2_subdev_call(ctx->pipe_subdevs[i], video,
				       s_stream, 0);
		if (ret) {
			dev_info(cam->dev, "failed to stream off %d: %d\n",
				 ctx->pipe_subdevs[i]->name, ret);
			return -EPERM;
		}
	}

	if (ctx->img_buf_pool.working_img_buf_size > 0)
		mtk_cam_img_working_buf_pool_release(ctx);

	mtk_camsys_ctrl_stop(ctx);

fail_stream_off:
#if CCD_READY
	if (ctx->used_raw_num)
		isp_composer_destroy_session_async(ctx);
#endif

	dev_dbg(cam->dev, "streamed off camsys ctx:%d\n", ctx->stream_id);

	return 0;
}

static int config_bridge_pad_links(struct mtk_cam_device *cam,
				   struct v4l2_subdev *seninf)
{
	struct media_entity *pipe_entity;
	unsigned int i, j;
	int ret;

	for (i = 0; i < cam->max_stream_num; i++) {
		if (i >= MTKCAM_SUBDEV_RAW_START &&
			i < (MTKCAM_SUBDEV_RAW_START + cam->num_raw_drivers)) {
			pipe_entity = &cam->raw.pipelines[i].subdev.entity;

			dev_info(cam->dev, "create pad link %s %s\n",
				seninf->entity.name, pipe_entity->name);

			ret = media_create_pad_link(&seninf->entity,
					MTK_CAM_CIO_PAD_SRC,
					pipe_entity,
					MTK_CAM_CIO_PAD_SINK,
					MEDIA_LNK_FL_DYNAMIC);

			if (ret) {
				dev_dbg(cam->dev,
					"failed to create pad link %s %s err:%d\n",
					seninf->entity.name, pipe_entity->name,
					ret);
				return ret;
			}
		} else if (i >= MTKCAM_SUBDEV_CAMSV_START && i < MTKCAM_SUBDEV_CAMSV_END) {
			pipe_entity = &cam->sv.pipelines[i-MTKCAM_SUBDEV_CAMSV_START].subdev.entity;

			dev_info(cam->dev, "create pad link %s %s\n",
				seninf->entity.name, pipe_entity->name);

			ret = media_create_pad_link(&seninf->entity,
					PAD_SRC_RAW0,
					pipe_entity,
					MTK_CAMSV_SINK,
					MEDIA_LNK_FL_DYNAMIC);

			if (ret) {
				dev_dbg(cam->dev,
					"failed to create pad link %s %s err:%d\n",
					seninf->entity.name, pipe_entity->name,
					ret);
				return ret;
			}

#if PDAF_READY
			for (j = PAD_SRC_PDAF0; j <= PAD_SRC_PDAF5; j++) {
				ret = media_create_pad_link(&seninf->entity,
						j,
						pipe_entity,
						MTK_CAMSV_SINK,
						MEDIA_LNK_FL_DYNAMIC);

				if (ret) {
					dev_dbg(cam->dev,
						"failed to create pad link %s %s err:%d\n",
						seninf->entity.name, pipe_entity->name,
						ret);
					return ret;
				}
			}
#endif
		} else if (i >= MTKCAM_SUBDEV_MRAW_START && i < MTKCAM_SUBDEV_MRAW_END) {
			pipe_entity =
				&cam->mraw.pipelines[i - MTKCAM_SUBDEV_MRAW_START].subdev.entity;

			dev_info(cam->dev, "create pad link %s %s\n",
				seninf->entity.name, pipe_entity->name);

			for (j = PAD_SRC_PDAF0; j <= PAD_SRC_PDAF5; j++) {
				ret = media_create_pad_link(&seninf->entity,
						j,
						pipe_entity,
						MTK_MRAW_SINK,
						MEDIA_LNK_FL_DYNAMIC);

				if (ret) {
					dev_dbg(cam->dev,
						"failed to create pad link %s %s err:%d\n",
						seninf->entity.name, pipe_entity->name,
						ret);
					return ret;
				}
			}
		}
	}

	return 0;
}

static int mtk_cam_create_links(struct mtk_cam_device *cam)
{
	struct v4l2_subdev *sd;
	unsigned int i;
	int ret;

	i = 0;
	v4l2_device_for_each_subdev(sd, &cam->v4l2_dev) {
		if (i < cam->num_seninf_drivers &&
		    sd->entity.function == MEDIA_ENT_F_VID_IF_BRIDGE) {
			ret = config_bridge_pad_links(cam, sd);
			i++;
		}
	}

	return ret;
}

static int mtk_cam_master_bind(struct device *dev)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	struct media_device *media_dev = &cam_dev->media_dev;
	int ret;

	dev_info(dev, "%s\n", __func__);

	media_dev->dev = cam_dev->dev;
	strscpy(media_dev->model, dev_driver_string(dev),
		sizeof(media_dev->model));
	snprintf(media_dev->bus_info, sizeof(media_dev->bus_info),
		 "platform:%s", dev_name(dev));
	media_dev->hw_revision = 0;
	media_dev->ops = &mtk_cam_dev_ops;
	media_device_init(media_dev);

	cam_dev->v4l2_dev.mdev = media_dev;
	ret = v4l2_device_register(cam_dev->dev, &cam_dev->v4l2_dev);
	if (ret) {
		dev_dbg(dev, "Failed to register V4L2 device: %d\n", ret);
		goto fail_media_device_cleanup;
	}

	ret = media_device_register(media_dev);
	if (ret) {
		dev_dbg(dev, "Failed to register media device: %d\n",
			ret);
		goto fail_v4l2_device_unreg;
	}

	ret = component_bind_all(dev, cam_dev);
	if (ret) {
		dev_dbg(dev, "Failed to bind all component: %d\n", ret);
		goto fail_media_device_unreg;
	}

	ret = mtk_raw_setup_dependencies(&cam_dev->raw);
	if (ret) {
		dev_dbg(dev, "Failed to mtk_raw_setup_dependencies: %d\n", ret);
		goto fail_unbind_all;
	}

	ret = mtk_camsv_setup_dependencies(&cam_dev->sv, &cam_dev->larb);
	if (ret) {
		dev_dbg(dev, "Failed to mtk_camsv_setup_dependencies: %d\n", ret);
		goto fail_remove_dependencies;
	}

	ret = mtk_mraw_setup_dependencies(&cam_dev->mraw, &cam_dev->larb);
	if (ret) {
		dev_dbg(dev, "Failed to mtk_mraw_setup_dependencies: %d\n", ret);
		goto fail_remove_dependencies;
	}

	ret = mtk_raw_register_entities(&cam_dev->raw, &cam_dev->v4l2_dev);
	if (ret) {
		dev_dbg(dev, "Failed to init raw subdevs: %d\n", ret);
		goto fail_remove_dependencies;
	}

	ret = mtk_camsv_register_entities(&cam_dev->sv, &cam_dev->v4l2_dev);
	if (ret) {
		dev_dbg(dev, "Failed to init camsv subdevs: %d\n", ret);
		goto fail_unreg_raw_entities;
	}

	ret = mtk_mraw_register_entities(&cam_dev->mraw, &cam_dev->v4l2_dev);
	if (ret) {
		dev_dbg(dev, "Failed to init mraw subdevs: %d\n", ret);
		goto fail_unreg_camsv_entities;
	}

	mtk_cam_create_links(cam_dev);
	/* Expose all subdev's nodes */
	ret = v4l2_device_register_subdev_nodes(&cam_dev->v4l2_dev);
	if (ret) {
		dev_dbg(dev, "Failed to register subdev nodes\n");
		goto fail_unreg_mraw_entities;
	}

	mtk_cam_dvfs_init(cam_dev);

	dev_info(dev, "%s success\n", __func__);
	return 0;

fail_unreg_mraw_entities:
	mtk_mraw_unregister_entities(&cam_dev->mraw);

fail_unreg_camsv_entities:
	mtk_camsv_unregister_entities(&cam_dev->sv);

fail_unreg_raw_entities:
	mtk_raw_unregister_entities(&cam_dev->raw);

fail_remove_dependencies:
	/* nothing to do for now */

fail_unbind_all:
	component_unbind_all(dev, cam_dev);

fail_media_device_unreg:
	media_device_unregister(&cam_dev->media_dev);

fail_v4l2_device_unreg:
	v4l2_device_unregister(&cam_dev->v4l2_dev);

fail_media_device_cleanup:
	media_device_cleanup(&cam_dev->media_dev);

	return ret;
}

static void mtk_cam_master_unbind(struct device *dev)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);

	dev_info(dev, "%s\n", __func__);

	mtk_raw_unregister_entities(&cam_dev->raw);
	mtk_camsv_unregister_entities(&cam_dev->sv);
	mtk_cam_dvfs_uninit(cam_dev);
	component_unbind_all(dev, cam_dev);

	media_device_unregister(&cam_dev->media_dev);
	v4l2_device_unregister(&cam_dev->v4l2_dev);
	media_device_cleanup(&cam_dev->media_dev);
}

static int compare_dev(struct device *dev, void *data)
{
	return dev == (struct device *)data;
}

static void mtk_cam_match_remove(struct device *dev)
{
	(void) dev;
}

static int add_match_by_driver(struct device *dev,
			       struct component_match **match,
			       struct platform_driver *drv)
{
	struct device *p = NULL, *d;
	int num = 0;

	do {
		d = platform_find_device_by_driver(p, &drv->driver);
		put_device(p);
		p = d;
		if (!d)
			break;

		component_match_add(dev, match, compare_dev, d);
		num++;
	} while (true);

	return num;
}

static struct component_match *mtk_cam_match_add(struct device *dev)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	struct component_match *match = NULL;
	int yuv_num;

	cam_dev->num_raw_drivers =
		add_match_by_driver(dev, &match, &mtk_cam_raw_driver);

	yuv_num = add_match_by_driver(dev, &match, &mtk_cam_yuv_driver);

	cam_dev->num_larb_drivers =
		add_match_by_driver(dev, &match, &mtk_cam_larb_driver);

	cam_dev->num_camsv_drivers =
		add_match_by_driver(dev, &match, &mtk_cam_sv_driver);

	cam_dev->num_mraw_drivers =
		add_match_by_driver(dev, &match, &mtk_cam_mraw_driver);

	cam_dev->num_seninf_drivers =
		add_match_by_driver(dev, &match, &seninf_pdrv);

	if (IS_ERR(match))
		mtk_cam_match_remove(dev);

	dev_info(dev, "#: raw %d, yuv %d, larb %d, sv %d, seninf %d, mraw %d\n",
		 cam_dev->num_raw_drivers, yuv_num,
		 cam_dev->num_larb_drivers,
		 cam_dev->num_camsv_drivers,
		 cam_dev->num_seninf_drivers,
		 cam_dev->num_mraw_drivers);

	return match ? match : ERR_PTR(-ENODEV);
}

static const struct component_master_ops mtk_cam_master_ops = {
	.bind = mtk_cam_master_bind,
	.unbind = mtk_cam_master_unbind,
};

static void mtk_cam_ctx_watchdog_worker(struct work_struct *work)
{
	struct mtk_cam_ctx *ctx;
	struct v4l2_subdev *seninf;
	struct mtk_raw_device *raw;
	u64 watchdog_cnt;
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	u64 watchdog_dump_cnt, watchdog_timeout_cnt;
#endif
	int timeout;
	static u64 last_vsync_count;
	bool is_abnormal_vsync = false;
	unsigned int int_en;

	ctx = container_of(work, struct mtk_cam_ctx, watchdog_work);
	seninf = ctx->seninf;
	if (!seninf) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d):stop watchdog task for no seninf ctx:%d\n",
			 __func__, ctx->stream_id);
		return;
	}
	raw = get_master_raw_dev(ctx->cam, ctx->pipe);
	watchdog_cnt = atomic_read(&ctx->watchdog_cnt);
	timeout = mtk_cam_seninf_check_timeout(seninf,
		watchdog_cnt * MTK_CAM_CTX_WATCHDOG_INTERVAL * 1000000);
	if (last_vsync_count == raw->vsync_count)
		is_abnormal_vsync = true;
	last_vsync_count = raw->vsync_count;
	/**
	 * Current we just call seninf dump, but it is better to check
	 * and restart the stream in the future.
	 */
	if (atomic_read(&ctx->watchdog_dumped)) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d):skip redundant seninf dump for no sof ctx:%d\n",
			 __func__, ctx->stream_id);
	} else {
		if (timeout) {
			dev_info(ctx->cam->dev,
				"%s:ctx(%d): timeout, VF(%d) vsync count(%d) sof count(%d) overrun_debug_dump_cnt(%d) start dump (%dx100ms)\n",
				__func__, ctx->stream_id, atomic_read(&raw->vf_en),
				raw->vsync_count, raw->sof_count, raw->overrun_debug_dump_cnt,
				watchdog_cnt);

			if (is_abnormal_vsync)
				dev_info(ctx->cam->dev, "%s:abnormal vsync\n");
			atomic_set(&ctx->watchdog_dumped, 1); // fixme
			atomic_set(&ctx->watchdog_cnt, 0);
			mtk_cam_seninf_dump(seninf);
			atomic_set(&ctx->watchdog_cnt, 0);
			atomic_inc(&ctx->watchdog_dump_cnt);
			atomic_set(&ctx->watchdog_dumped, 0);
	#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
			watchdog_dump_cnt = atomic_read(&ctx->watchdog_dump_cnt);
			watchdog_timeout_cnt = atomic_read(&ctx->watchdog_timeout_cnt);
			if (watchdog_dump_cnt == watchdog_timeout_cnt) {
				int_en = readl_relaxed(raw->base + REG_CTL_RAW_INT_EN);
				if (raw->vsync_count == 0) {
					dev_info(ctx->cam->dev,
						"vsync count(%d), VF(%d), INT_EN(0x%x)\n",
						raw->vsync_count,
						atomic_read(&raw->vf_en),
						int_en);
					if (int_en == 0)
						aee_kernel_warning_api(
							__FILE__, __LINE__, DB_OPT_DEFAULT,
							"Camsys: 1st CQ done timeout",
							"watchdog timeout");
					else
						aee_kernel_warning_api(
							__FILE__, __LINE__, DB_OPT_DEFAULT,
							"Camsys: Vsync timeout",
							"watchdog timeout");

				} else if (atomic_read(&raw->vf_en) == 0) {
					dev_info(ctx->cam->dev,
						"vsync count(%d), frame inner index(%d) INT_EN(0x%x)\n",
						raw->vsync_count,
						readl_relaxed(raw->base_inner + REG_FRAME_SEQ_NUM),
						readl_relaxed(raw->base + REG_CTL_RAW_INT_EN));

					aee_kernel_warning_api(
						__FILE__, __LINE__, DB_OPT_DEFAULT,
						"Camsys: VF timeout", "watchdog timeout");

				} else if (atomic_read(&raw->vf_en) == 1 &&
					raw->overrun_debug_dump_cnt == 0) {
					dev_info(ctx->cam->dev,
						"[Outer] TG PATHCFG/SENMODE/DCIF_CTL:0x%x/0x%x/0x%x\n",
						readl_relaxed(raw->base + REG_TG_PATH_CFG),
						readl_relaxed(raw->base + REG_TG_SEN_MODE),
						readl_relaxed(raw->base + REG_TG_DCIF_CTL));

					dev_info(ctx->cam->dev,
						"REQ RAW/2/3 DMA/2:%08x/%08x/%08x/%08x/%08x\n",
						readl_relaxed(raw->base +
							REG_CTL_RAW_MOD_REQ_STAT),
						readl_relaxed(raw->base +
							REG_CTL_RAW_MOD2_REQ_STAT),
						readl_relaxed(raw->base +
							REG_CTL_RAW_MOD3_REQ_STAT),
						readl_relaxed(raw->base +
							REG_CTL_RAW_MOD5_REQ_STAT),
						readl_relaxed(raw->base +
							REG_CTL_RAW_MOD6_REQ_STAT));
					dev_info(ctx->cam->dev,
						"RDY RAW/2/3 DMA/2:%08x/%08x/%08x/%08x/%08x\n",
						readl_relaxed(raw->base +
							REG_CTL_RAW_MOD_RDY_STAT),
						readl_relaxed(raw->base +
							REG_CTL_RAW_MOD2_RDY_STAT),
						readl_relaxed(raw->base +
							REG_CTL_RAW_MOD3_RDY_STAT),
						readl_relaxed(raw->base +
							REG_CTL_RAW_MOD5_RDY_STAT),
						readl_relaxed(raw->base +
							REG_CTL_RAW_MOD6_RDY_STAT));

					aee_kernel_warning_api(
						__FILE__, __LINE__, DB_OPT_DEFAULT,
						"Camsys: SOF timeout", "watchdog timeout");
				}
			}
	#endif
		} else {
			dev_info(ctx->cam->dev, "%s:ctx(%d): not timeout, for long exp (%dx100ms)\n",
				__func__, ctx->stream_id, watchdog_cnt);
		}
	}
}

static void mtk_ctx_watchdog(struct timer_list *t)
{
	struct mtk_cam_ctx *ctx = from_timer(ctx, t, watchdog_timer);
	struct mtk_raw_device *raw;
	int watchdog_cnt;
	int watchdog_dump_cnt;

	if (!ctx->streaming)
		return;

	raw = get_master_raw_dev(ctx->cam, ctx->pipe);
	if (!raw) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d):stop watchdog task for no raw ctx\n",
			 __func__, ctx->stream_id);
		return;
	}

	watchdog_cnt = atomic_inc_return(&ctx->watchdog_cnt);
	watchdog_dump_cnt = atomic_read(&ctx->watchdog_dump_cnt);

	if (atomic_read(&ctx->watchdog_dumped)) {
		dev_dbg(ctx->cam->dev,
			 "%s:ctx(%d):skip watchdog worker ctx:%d (worker is ongoing)\n",
			 __func__, ctx->stream_id);
	} else if (watchdog_cnt >= atomic_read(&ctx->watchdog_timeout_cnt)) {
		/**
		 * No raw's sof interrupts were generated by hw for the
		 * Nth time of running the watchdog timer.
		 */
		if (watchdog_dump_cnt < 4) {
			dev_info_ratelimited(ctx->cam->dev, "%s:ctx(%d): timeout! VF(%d) vsync count(%d) sof count(%d) watcgdog_cnt(%d)(+%dms)\n",
				__func__, ctx->stream_id, atomic_read(&raw->vf_en),
				raw->vsync_count, raw->sof_count, watchdog_cnt,
				watchdog_cnt * MTK_CAM_CTX_WATCHDOG_INTERVAL);

			schedule_work(&ctx->watchdog_work);
		} else {
			dev_info_ratelimited(ctx->cam->dev, "%s:ctx(%d): dump > 3! watchdog_dump_cnt(%d)(+%dms)\n",
				__func__, ctx->stream_id, watchdog_dump_cnt,
				watchdog_cnt * MTK_CAM_CTX_WATCHDOG_INTERVAL);
		}
	}

	ctx->watchdog_timer.expires = jiffies +
				msecs_to_jiffies(MTK_CAM_CTX_WATCHDOG_INTERVAL);
	dev_dbg(ctx->cam->dev, "%s:check ctx(%d): dog_cnt(%d), dog_time:%dms\n",
				__func__, ctx->stream_id, watchdog_cnt,
				watchdog_cnt * MTK_CAM_CTX_WATCHDOG_INTERVAL);
	add_timer(&ctx->watchdog_timer);
}

void mtk_ctx_watchdog_kick(struct mtk_cam_ctx *ctx)
{
	dev_dbg(ctx->cam->dev, "%s:ctx(%d): watchdog_cnt(%d)\n",
		__func__, ctx->stream_id, atomic_read(&ctx->watchdog_cnt));
	atomic_set(&ctx->watchdog_cnt, 0);
	atomic_set(&ctx->watchdog_dump_cnt, 0);
}

static void mtk_ctx_watchdog_init(struct mtk_cam_ctx *ctx)
{
	INIT_WORK(&ctx->watchdog_work, mtk_cam_ctx_watchdog_worker);
	timer_setup(&ctx->watchdog_timer, mtk_ctx_watchdog, 0);
}

void mtk_ctx_watchdog_start(struct mtk_cam_ctx *ctx, int timeout_cnt)
{
	dev_info(ctx->cam->dev, "%s:ctx(%d):start the watchdog, timeout setting(%d)\n",
		__func__, ctx->stream_id, MTK_CAM_CTX_WATCHDOG_INTERVAL * timeout_cnt);

	atomic_set(&ctx->watchdog_timeout_cnt, timeout_cnt);
	atomic_set(&ctx->watchdog_cnt, 0);
	atomic_set(&ctx->watchdog_dumped, 0);
	atomic_set(&ctx->watchdog_dump_cnt, 0);
	ctx->watchdog_timer.expires = jiffies +
				msecs_to_jiffies(MTK_CAM_CTX_WATCHDOG_INTERVAL);
	add_timer(&ctx->watchdog_timer);
}

void mtk_ctx_watchdog_stop(struct mtk_cam_ctx *ctx)
{
	dev_info(ctx->cam->dev, "%s:ctx(%d):stop the watchdog\n",
		__func__, ctx->stream_id);
	del_timer_sync(&ctx->watchdog_timer);
}

static void mtk_cam_ctx_init(struct mtk_cam_ctx *ctx,
			     struct mtk_cam_device *cam,
			     unsigned int stream_id)
{
	unsigned int i;

	ctx->cam = cam;
	ctx->stream_id = stream_id;
	ctx->sensor = NULL;
	ctx->prev_sensor = NULL;
	ctx->prev_seninf = NULL;

	ctx->streaming_pipe = 0;
	ctx->streaming_node_cnt = 0;

	ctx->used_raw_num = 0;
	ctx->used_raw_dev = 0;
	ctx->processing_buffer_list.cnt = 0;
	ctx->composed_buffer_list.cnt = 0;
	ctx->is_first_cq_done = 0;
	ctx->cq_done_status = 0;
	ctx->session_created = 0;

	ctx->used_sv_num = 0;
	for (i = 0; i < MAX_SV_PIPES_PER_STREAM; i++) {
		ctx->sv_pipe[i] = NULL;
		ctx->used_sv_dev[i] = 0;
		ctx->sv_dequeued_frame_seq_no[i] = 0;
		ctx->sv_using_buffer_list[i].cnt = 0;
		ctx->sv_processing_buffer_list[i].cnt = 0;
	}

	ctx->used_mraw_num = 0;
	for (i = 0 ; i < MAX_MRAW_PIPES_PER_STREAM ; i++) {
		ctx->mraw_pipe[i] = NULL;
		ctx->used_mraw_dev[i] = 0;
		ctx->mraw_dequeued_frame_seq_no[i] = 0;
		ctx->mraw_using_buffer_list[i].cnt = 0;
		ctx->mraw_processing_buffer_list[i].cnt = 0;
		ctx->mraw_composed_buffer_list[i].cnt = 0;
	}

	INIT_LIST_HEAD(&ctx->using_buffer_list.list);
	INIT_LIST_HEAD(&ctx->composed_buffer_list.list);
	INIT_LIST_HEAD(&ctx->processing_buffer_list.list);
	for (i = 0; i < MAX_SV_PIPES_PER_STREAM; i++) {
		INIT_LIST_HEAD(&ctx->sv_using_buffer_list[i].list);
		INIT_LIST_HEAD(&ctx->sv_processing_buffer_list[i].list);
	}
	for (i = 0; i < MAX_MRAW_PIPES_PER_STREAM; i++) {
		INIT_LIST_HEAD(&ctx->mraw_using_buffer_list[i].list);
		INIT_LIST_HEAD(&ctx->mraw_composed_buffer_list[i].list);
		INIT_LIST_HEAD(&ctx->mraw_processing_buffer_list[i].list);
	}
	INIT_LIST_HEAD(&ctx->processing_img_buffer_list.list);
	spin_lock_init(&ctx->using_buffer_list.lock);
	spin_lock_init(&ctx->composed_buffer_list.lock);
	spin_lock_init(&ctx->processing_buffer_list.lock);
	for (i = 0; i < MAX_SV_PIPES_PER_STREAM; i++) {
		spin_lock_init(&ctx->sv_using_buffer_list[i].lock);
		spin_lock_init(&ctx->sv_processing_buffer_list[i].lock);
	}
	for (i = 0; i < MAX_MRAW_PIPES_PER_STREAM; i++) {
		spin_lock_init(&ctx->mraw_using_buffer_list[i].lock);
		spin_lock_init(&ctx->mraw_composed_buffer_list[i].lock);
		spin_lock_init(&ctx->mraw_processing_buffer_list[i].lock);
	}
	spin_lock_init(&ctx->streaming_lock);
	spin_lock_init(&ctx->first_cq_lock);
	spin_lock_init(&ctx->processing_img_buffer_list.lock);

	mtk_ctx_watchdog_init(ctx);
}

static int mtk_cam_v4l2_subdev_link_validate(struct v4l2_subdev *sd,
				      struct media_link *link,
				      struct v4l2_subdev_format *source_fmt,
				      struct v4l2_subdev_format *sink_fmt)
{
	bool pass = true;

	/* The width, height and code must match. */
	if (source_fmt->format.width != sink_fmt->format.width) {
		dev_dbg(sd->entity.graph_obj.mdev->dev,
			"%s: width does not match (source %u, sink %u)\n",
			__func__,
			source_fmt->format.width, sink_fmt->format.width);
		pass = false;
	}

	if (source_fmt->format.height != sink_fmt->format.height) {
		dev_dbg(sd->entity.graph_obj.mdev->dev,
			"%s: height does not match (source %u, sink %u)\n",
			__func__,
			source_fmt->format.height, sink_fmt->format.height);
		pass = false;
	}

	if (source_fmt->format.code != sink_fmt->format.code) {
		dev_info(sd->entity.graph_obj.mdev->dev,
			"%s: warn: media bus code does not match (source 0x%8.8x, sink 0x%8.8x)\n",
			__func__,
			source_fmt->format.code, sink_fmt->format.code);
	}

	if (pass)
		return 0;

	dev_dbg(sd->entity.graph_obj.mdev->dev,
		"%s: link was \"%s\":%u -> \"%s\":%u\n", __func__,
		link->source->entity->name, link->source->index,
		link->sink->entity->name, link->sink->index);

	return -EPIPE;
}

int mtk_cam_link_validate(struct v4l2_subdev *sd,
			  struct media_link *link,
			  struct v4l2_subdev_format *source_fmt,
			  struct v4l2_subdev_format *sink_fmt)
{
	struct device *dev;
	int ret = 0;

	dev = sd->v4l2_dev->dev;

	ret = mtk_cam_v4l2_subdev_link_validate(sd, link, source_fmt, sink_fmt);
	if (ret)
		dev_info(dev, "%s: link validate failed pad/code/w/h: SRC(%d/0x%x/%d/%d), SINK(%d:0x%x/%d/%d)\n",
			 sd->name, source_fmt->pad, source_fmt->format.code,
			 source_fmt->format.width, source_fmt->format.height,
			 sink_fmt->pad, sink_fmt->format.code,
			 sink_fmt->format.width, sink_fmt->format.height);

	return ret;
}

static int mtk_cam_debug_fs_init(struct mtk_cam_device *cam)
{
	/**
	 * The dump buffer size depdends on the meta buffer size
	 * which is variable among devices using different type of sensors
	 * , e.g. PD's statistic buffers.
	 */
	int dump_mem_size = MTK_CAM_DEBUG_DUMP_HEADER_MAX_SIZE +
			    CQ_BUF_SIZE +
			    mtk_cam_get_meta_size(MTKCAM_IPI_RAW_META_STATS_CFG) +
			    RAW_STATS_CFG_VARIOUS_SIZE +
			    sizeof(struct mtkcam_ipi_frame_param) +
			    sizeof(struct mtkcam_ipi_config_param) *
			    RAW_PIPELINE_NUM;

	cam->debug_fs = mtk_cam_get_debugfs();
	if (!cam->debug_fs)
		return 0;

	cam->debug_fs->ops->init(cam->debug_fs, cam, dump_mem_size);
	cam->debug_wq = alloc_ordered_workqueue(dev_name(cam->dev),
						__WQ_LEGACY | WQ_MEM_RECLAIM |
						WQ_FREEZABLE);
	cam->debug_exception_wq = alloc_ordered_workqueue(dev_name(cam->dev),
						__WQ_LEGACY | WQ_MEM_RECLAIM |
						WQ_FREEZABLE);
	init_waitqueue_head(&cam->debug_exception_waitq);

	if (!cam->debug_wq || !cam->debug_exception_wq)
		return -EINVAL;

	return 0;
}

static void mtk_cam_debug_fs_deinit(struct mtk_cam_device *cam)
{

	drain_workqueue(cam->debug_wq);
	destroy_workqueue(cam->debug_wq);
	drain_workqueue(cam->debug_exception_wq);
	destroy_workqueue(cam->debug_exception_wq);

	if (cam->debug_fs)
		cam->debug_fs->ops->deinit(cam->debug_fs);
}

static int register_sub_drivers(struct device *dev)
{
	struct component_match *match = NULL;
	int ret;

	ret = platform_driver_register(&mtk_cam_larb_driver);
	if (ret) {
		dev_info(dev, "%s mtk_cam_larb_driver fail\n", __func__);
		goto REGISTER_LARB_FAIL;
	}

	ret = platform_driver_register(&seninf_pdrv);
	if (ret) {
		dev_info(dev, "%s seninf_pdrv fail\n", __func__);
		goto REGISTER_SENINF_FAIL;
	}

	ret = platform_driver_register(&seninf_core_pdrv);
	if (ret) {
		dev_info(dev, "%s seninf_core_pdrv fail\n", __func__);
		goto REGISTER_SENINF_CORE_FAIL;
	}

	ret = platform_driver_register(&mtk_cam_sv_driver);
	if (ret) {
		dev_info(dev, "%s mtk_cam_sv_driver fail\n", __func__);
		goto REGISTER_CAMSV_FAIL;
	}

	ret = platform_driver_register(&mtk_cam_mraw_driver);
	if (ret) {
		dev_info(dev, "%s mtk_cam_mraw_driver fail\n", __func__);
		goto REGISTER_MRAW_FAIL;
	}

	ret = platform_driver_register(&mtk_cam_raw_driver);
	if (ret) {
		dev_info(dev, "%s mtk_cam_raw_driver fail\n", __func__);
		goto REGISTER_RAW_FAIL;
	}

	ret = platform_driver_register(&mtk_cam_yuv_driver);
	if (ret) {
		dev_info(dev, "%s mtk_cam_raw_driver fail\n", __func__);
		goto REGISTER_YUV_FAIL;
	}

	match = mtk_cam_match_add(dev);
	if (IS_ERR(match)) {
		ret = PTR_ERR(match);
		goto ADD_MATCH_FAIL;
	}

	ret = component_master_add_with_match(dev, &mtk_cam_master_ops, match);
	if (ret < 0)
		goto MASTER_ADD_MATCH_FAIL;

	return 0;

MASTER_ADD_MATCH_FAIL:
	mtk_cam_match_remove(dev);

ADD_MATCH_FAIL:
	platform_driver_unregister(&mtk_cam_yuv_driver);

REGISTER_YUV_FAIL:
	platform_driver_unregister(&mtk_cam_raw_driver);

REGISTER_RAW_FAIL:
	platform_driver_unregister(&mtk_cam_mraw_driver);

REGISTER_MRAW_FAIL:
	platform_driver_unregister(&mtk_cam_sv_driver);

REGISTER_CAMSV_FAIL:
	platform_driver_unregister(&seninf_core_pdrv);

REGISTER_SENINF_CORE_FAIL:
	platform_driver_unregister(&seninf_pdrv);

REGISTER_SENINF_FAIL:
	platform_driver_unregister(&mtk_cam_larb_driver);

REGISTER_LARB_FAIL:
	return ret;
}

static int mtk_cam_probe(struct platform_device *pdev)
{
	struct mtk_cam_device *cam_dev;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret;
	unsigned int i;

	dev_info(dev, "%s\n", __func__);

	/* initialize structure */
	cam_dev = devm_kzalloc(dev, sizeof(*cam_dev), GFP_KERNEL);
	if (!cam_dev)
		return -ENOMEM;
	if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34)))
		dev_info(dev, "%s: No suitable DMA available\n", __func__);

	if (!dev->dma_parms) {
		dev->dma_parms =
			devm_kzalloc(dev, sizeof(*dev->dma_parms), GFP_KERNEL);
		if (!dev->dma_parms)
			return -ENOMEM;
	}

	if (dev->dma_parms) {
		ret = dma_set_max_seg_size(dev, UINT_MAX);
		if (ret)
			dev_info(dev, "Failed to set DMA segment size\n");
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	cam_dev->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(cam_dev->base)) {
		dev_dbg(dev, "failed to map register base\n");
		return PTR_ERR(cam_dev->base);
	}

	cam_dev->dev = dev;
	dev_set_drvdata(dev, cam_dev);

	cam_dev->composer_cnt = 0;
	cam_dev->num_seninf_drivers = 0;

	/* FIXME: decide max raw stream num by seninf num */
	cam_dev->max_stream_num = MTKCAM_SUBDEV_MAX;
	cam_dev->ctxs = devm_kcalloc(dev, cam_dev->max_stream_num,
				     sizeof(*cam_dev->ctxs), GFP_KERNEL);
	if (!cam_dev->ctxs)
		return -ENOMEM;

	cam_dev->streaming_ctx = 0;
	for (i = 0; i < cam_dev->max_stream_num; i++)
		mtk_cam_ctx_init(cam_dev->ctxs + i, cam_dev, i);

	cam_dev->running_job_count = 0;
	spin_lock_init(&cam_dev->pending_job_lock);
	spin_lock_init(&cam_dev->running_job_lock);
	INIT_LIST_HEAD(&cam_dev->pending_job_list);
	INIT_LIST_HEAD(&cam_dev->running_job_list);

	mutex_init(&cam_dev->queue_lock);

	pm_runtime_enable(dev);

	ret = mtk_cam_of_rproc(cam_dev);
	if (ret)
		goto fail_destroy_mutex;

	ret = register_sub_drivers(dev);
	if (ret) {
		dev_info(dev, "fail to register_sub_drivers\n");
		goto fail_destroy_mutex;
	}


	cam_dev->link_change_wq =
			alloc_ordered_workqueue(dev_name(cam_dev->dev),
						WQ_HIGHPRI | WQ_FREEZABLE);
	if (!cam_dev->link_change_wq) {
		dev_dbg(cam_dev->dev, "failed to alloc link_change_wq\n");
		goto fail_match_remove;
	}

	ret = mtk_cam_debug_fs_init(cam_dev);
	if (ret < 0)
		goto fail_uninit_link_change_wq;

	return 0;

fail_uninit_link_change_wq:
	destroy_workqueue(cam_dev->link_change_wq);

fail_match_remove:
	mtk_cam_match_remove(dev);

fail_destroy_mutex:
	mutex_destroy(&cam_dev->queue_lock);

	return ret;
}

static int mtk_cam_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);

	pm_runtime_disable(dev);

	component_master_del(dev, &mtk_cam_master_ops);
	mtk_cam_match_remove(dev);

	mutex_destroy(&cam_dev->queue_lock);
	mtk_cam_debug_fs_deinit(cam_dev);

	destroy_workqueue(cam_dev->link_change_wq);

	platform_driver_unregister(&mtk_cam_mraw_driver);
	platform_driver_unregister(&mtk_cam_sv_driver);
	platform_driver_unregister(&mtk_cam_raw_driver);
	platform_driver_unregister(&mtk_cam_larb_driver);
	platform_driver_unregister(&seninf_core_pdrv);
	platform_driver_unregister(&seninf_pdrv);

	return 0;
}

static const struct dev_pm_ops mtk_cam_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_cam_runtime_suspend, mtk_cam_runtime_resume,
			   NULL)
};

static struct platform_driver mtk_cam_driver = {
	.probe   = mtk_cam_probe,
	.remove  = mtk_cam_remove,
	.driver  = {
		.name  = "mtk-cam",
		.of_match_table = of_match_ptr(mtk_cam_of_ids),
		.pm     = &mtk_cam_pm_ops,
	}
};

static void media_device_setup_link_hook(void *data, struct media_link *link,
					 struct media_link_desc *linkd, int *ret)
{
	int ret_value;

	pr_debug("%s req_fd:%d\n", __func__, linkd->reserved[0]);
	link->android_vendor_data1 = linkd->reserved[0];
	ret_value = __media_entity_setup_link(link, linkd->flags);
	link->android_vendor_data1 = 0;
	*ret = (ret_value < 0) ? ret_value : 1;
	pr_debug("%s ret:%d\n", __func__, *ret);
}

static void clear_reserved_fmt_fields_hook(void *data, struct v4l2_format *p,
					   int *ret)
{
	*ret = 1;
}

static void fill_ext_fmtdesc_hook(void *data, struct v4l2_fmtdesc *p, const char **descr)
{
	switch (p->pixelformat) {
	case V4L2_PIX_FMT_YUYV10:
		*descr = "YUYV 4:2:2 10 bits";
		break;
	case V4L2_PIX_FMT_YVYU10:
		*descr = "YVYU 4:2:2 10 bits";
		break;
	case V4L2_PIX_FMT_UYVY10:
		*descr = "UYVY 4:2:2 10 bits";
		break;
	case V4L2_PIX_FMT_VYUY10:
		*descr = "VYUY 4:2:2 10 bits";
		break;
	case V4L2_PIX_FMT_NV12_10:
		*descr = "Y/CbCr 4:2:0 10 bits";
		break;
	case V4L2_PIX_FMT_NV21_10:
		*descr = "Y/CrCb 4:2:0 10 bits";
		break;
	case V4L2_PIX_FMT_NV16_10:
		*descr = "Y/CbCr 4:2:2 10 bits";
		break;
	case V4L2_PIX_FMT_NV61_10:
		*descr = "Y/CrCb 4:2:2 10 bits";
		break;
	case V4L2_PIX_FMT_NV12_12:
		*descr = "Y/CbCr 4:2:0 12 bits";
		break;
	case V4L2_PIX_FMT_NV21_12:
		*descr = "Y/CrCb 4:2:0 12 bits";
		break;
	case V4L2_PIX_FMT_NV16_12:
		*descr = "Y/CbCr 4:2:2 12 bits";
		break;
	case V4L2_PIX_FMT_NV61_12:
		*descr = "Y/CrCb 4:2:2 12 bits";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR10:
		*descr = "10-bit Bayer BGGR MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG10:
		*descr = "10-bit Bayer GBRG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG10:
		*descr = "10-bit Bayer GRBG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB10:
		*descr = "10-bit Bayer RGGB MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR12:
		*descr = "12-bit Bayer BGGR MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG12:
		*descr = "12-bit Bayer GBRG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG12:
		*descr = "12-bit Bayer GRBG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB12:
		*descr = "12-bit Bayer RGGB MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR14:
		*descr = "14-bit Bayer BGGR MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG14:
		*descr = "14-bit Bayer GBRG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG14:
		*descr = "14-bit Bayer GRBG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB14:
		*descr = "14-bit Bayer RGGB MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR8F:
		*descr = "8-bit Enhanced BGGR Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG8F:
		*descr = "8-bit Enhanced GBRG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG8F:
		*descr = "8-bit Enhanced GRBG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB8F:
		*descr = "8-bit Enhanced RGGB Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR10F:
		*descr = "10-bit Enhanced BGGR Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG10F:
		*descr = "10-bit Enhanced GBRG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG10F:
		*descr = "10-bit Enhanced GRBG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB10F:
		*descr = "10-bit Enhanced RGGB Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR12F:
		*descr = "12-bit Enhanced BGGR Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG12F:
		*descr = "12-bit Enhanced GBRG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG12F:
		*descr = "12-bit Enhanced GRBG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB12F:
		*descr = "12-bit Enhanced RGGB Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR14F:
		*descr = "14-bit Enhanced BGGR Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG14F:
		*descr = "14-bit Enhanced GBRG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG14F:
		*descr = "14-bit Enhanced GRBG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB14F:
		*descr = "14-bit Enhanced RGGB Packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV12_10P:
		*descr = "Y/CbCr 4:2:0 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV21_10P:
		*descr = "Y/CrCb 4:2:0 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV16_10P:
		*descr = "Y/CbCr 4:2:2 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV61_10P:
		*descr = "Y/CrCb 4:2:2 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_YUYV10P:
		*descr = "YUYV 4:2:2 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_YVYU10P:
		*descr = "YVYU 4:2:2 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_UYVY10P:
		*descr = "UYVY 4:2:2 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_VYUY10P:
		*descr = "VYUY 4:2:2 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV12_12P:
		*descr = "Y/CbCr 4:2:0 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV21_12P:
		*descr = "Y/CrCb 4:2:0 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV16_12P:
		*descr = "Y/CbCr 4:2:2 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV61_12P:
		*descr = "Y/CrCb 4:2:2 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_YUYV12P:
		*descr = "YUYV 4:2:2 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_YVYU12P:
		*descr = "YVYU 4:2:2 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_UYVY12P:
		*descr = "UYVY 4:2:2 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_VYUY12P:
		*descr = "VYUY 4:2:2 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV12_UFBC:
		*descr = "YCbCr 420 8 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_NV21_UFBC:
		*descr = "YCrCb 420 8 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_NV12_10_UFBC:
		*descr = "YCbCr 420 10 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_NV21_10_UFBC:
		*descr = "YCrCb 420 10 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_NV12_12_UFBC:
		*descr = "YCbCr 420 12 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_NV21_12_UFBC:
		*descr = "YCrCb 420 12 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_BAYER8_UFBC:
		*descr = "RAW 8 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_BAYER10_UFBC:
		*descr = "RAW 10 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_BAYER12_UFBC:
		*descr = "RAW 12 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_BAYER14_UFBC:
		*descr = "RAW 14 bits compress";
		break;
	case V4L2_META_FMT_MTISP_3A:
		*descr = "AE/AWB Histogram";
		break;
	case V4L2_META_FMT_MTISP_AF:
		*descr = "AF Histogram";
		break;
	case V4L2_META_FMT_MTISP_LCS:
		*descr = "Local Contrast Enhancement Stat";
		break;
	case V4L2_META_FMT_MTISP_LMV:
		*descr = "Local Motion Vector Histogram";
		break;
	case V4L2_META_FMT_MTISP_PARAMS:
		*descr = "MTK ISP Tuning Metadata";
		break;
	case V4L2_PIX_FMT_MTISP_SGRB8F:
		*descr = "8-bit 3 plane GRB Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRB10F:
		*descr = "10-bit 3 plane GRB Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRB12F:
		*descr = "12-bit 3 plane GRB Packed";
		break;
	default:
		break;
	}
}

static void clear_mask_adjust_hook(void *data, unsigned int ctrl, int *n)
{
	if (ctrl == VIDIOC_S_SELECTION)
		*n = offsetof(struct v4l2_selection, reserved[1]);
}

static void v4l2subdev_set_fmt_hook(void *data,
	struct v4l2_subdev *sd, struct v4l2_subdev_pad_config *pad,
	struct v4l2_subdev_format *format, int *ret)
{
	int retval = 0;
	struct media_entity entity = sd->entity;

	pr_debug("%s entity_name:%s req_fd:%d\n", __func__, entity.name,
		format->reserved[0]);

	retval = v4l2_subdev_call(sd, pad, set_fmt, pad, format);
	*ret = (retval < 0) ? retval : 1;
	pr_debug("%s *ret:%d\n", __func__, *ret);
}

static void v4l2subdev_set_selection_hook(void *data,
	struct v4l2_subdev *sd, struct v4l2_subdev_pad_config *pad,
	struct v4l2_subdev_selection *sel, int *ret)
{
	int retval = 0;

	retval = v4l2_subdev_call(sd, pad, set_selection, pad, sel);
	*ret = (retval < 0) ? retval : 1;
}

static void v4l2subdev_set_frame_interval_hook(void *data,
	struct v4l2_subdev *sd, struct v4l2_subdev_frame_interval *fi,
	int *ret)
{
	int retval = 0;

	retval = v4l2_subdev_call(sd, video, s_frame_interval, fi);
	*ret = (retval < 0) ? retval : 1;
}

static void mtk_cam_trace_init(void)
{
	int ret = 0;

	ret = register_trace_android_rvh_media_device_setup_link(
			media_device_setup_link_hook, NULL);
	if (ret)
		pr_info("register android_rvh_media_device_setup_link failed!\n");
	ret = register_trace_android_vh_clear_reserved_fmt_fields(
			clear_reserved_fmt_fields_hook, NULL);
	if (ret)
		pr_info("register android_vh_clear_reserved_fmt_fields failed!\n");
	ret = register_trace_android_vh_fill_ext_fmtdesc(
			fill_ext_fmtdesc_hook, NULL);
	if (ret)
		pr_info("register android_vh_v4l_fill_fmtdesc failed!\n");
	ret = register_trace_android_vh_clear_mask_adjust(
			clear_mask_adjust_hook, NULL);
	if (ret)
		pr_info("register android_vh_clear_mask_adjust failed!\n");
	ret = register_trace_android_rvh_v4l2subdev_set_fmt(
			v4l2subdev_set_fmt_hook, NULL);
	if (ret)
		pr_info("register android_rvh_v4l2subdev_set_fmt failed!\n");
	ret = register_trace_android_rvh_v4l2subdev_set_selection(
			v4l2subdev_set_selection_hook, NULL);
	if (ret)
		pr_info("register android_rvh_v4l2subdev_set_selection failed!\n");
	ret = register_trace_android_rvh_v4l2subdev_set_frame_interval(
			v4l2subdev_set_frame_interval_hook, NULL);
	if (ret)
		pr_info("register android_rvh_v4l2subdev_set_frame_interval failed!\n");
}

static void mtk_cam_trace_exit(void)
{
	unregister_trace_android_vh_clear_reserved_fmt_fields(clear_reserved_fmt_fields_hook, NULL);
	unregister_trace_android_vh_fill_ext_fmtdesc(fill_ext_fmtdesc_hook, NULL);
	unregister_trace_android_vh_clear_mask_adjust(clear_mask_adjust_hook, NULL);
}

static int __init mtk_cam_init(void)
{
	int ret;

	mtk_cam_trace_init();
	ret = platform_driver_register(&mtk_cam_driver);
	return ret;
}

static void __exit mtk_cam_exit(void)
{
	platform_driver_unregister(&mtk_cam_driver);
	mtk_cam_trace_exit();
}

module_init(mtk_cam_init);
module_exit(mtk_cam_exit);

MODULE_DESCRIPTION("Camera ISP driver");
MODULE_LICENSE("GPL");
