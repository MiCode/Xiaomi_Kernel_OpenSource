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
#include "mtk_cam-dvfs_qos.h"
#include "mtk_cam-pool.h"
#include "mtk_cam-regs.h"
#include "mtk_cam-sv-regs.h"
#include "mtk_cam-mraw-regs.h"
#include "mtk_cam-smem.h"
#include "mtk_cam-tg-flash.h"
#include "mtk_camera-v4l2-controls.h"
#include "mtk_camera-videodev2.h"
#include "mtk_cam-hsf.h"
#include "mtk_cam-trace.h"
#include "mtk_cam-ufbc-def.h"
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <slbc/slbc_ops.h>


#ifdef CAMSYS_TF_DUMP_7S
#include <dt-bindings/memory/mt6985-larb-port.h>
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

#define MTK_CAM_CACI_TABLE_SIZE (50000)

/* Zero out the end of the struct pointed to by p.  Everything after, but
 * not including, the specified field is cleared.
 */
#define CLEAR_AFTER_FIELD(p, field) \
	memset((u8 *)(p) + offsetof(typeof(*(p)), field) + sizeof((p)->field), \
	0, sizeof(*(p)) - offsetof(typeof(*(p)), field) -sizeof((p)->field))

static const struct of_device_id mtk_cam_of_ids[] = {
#ifdef CAMSYS_ISP7S_MT6985
	{.compatible = "mediatek,mt6985-camisp", .data = &mt6985_data},
#endif
#ifdef CAMSYS_ISP7S_MT6886
	{.compatible = "mediatek,mt6886-camisp", .data = &mt6886_data},
#endif
	{}
};

static int debug_preisp_off_data;
module_param(debug_preisp_off_data, int, 0644);
MODULE_PARM_DESC(debug_preisp_off_data, "debug: preisp bypass camsys mode");
enum {
	DEBUG_PREISP_OFF_META = 0,
	DEBUG_PREISP_OFF_PURERAW,
	DEBUG_PREISP_OFF_PROCRAW,
};

static int debug_cam;
module_param(debug_cam, int, 0644);

#undef dev_dbg
#define dev_dbg(dev, fmt, arg...)		\
	do {					\
		if (debug_cam >= 1)		\
			dev_info(dev, fmt,	\
				## arg);	\
	} while (0)

#define MTK_CAM_IMGO_W_IMG_OUT_IDX			CAM_MAX_OUTPUT_PAD

#define MTK_CAM_IMGO_W_IMG_IN_R2_IDX	CAM_MAX_INPUT_PAD
#define MTK_CAM_IMGO_W_IMG_IN_R5_IDX	(CAM_MAX_INPUT_PAD + 1)

MODULE_DEVICE_TABLE(of, mtk_cam_of_ids);

static void mtk_cam_register_iommu_tf_callback(struct mtk_raw_device *raw)
{
#ifdef CAMSYS_TF_DUMP_7S
	dev_dbg(raw->dev, "%s : raw->id:%d\n", __func__, raw->id);

	if (raw->id == RAW_A) {
		mtk_iommu_register_fault_callback(M4U_PORT_L16_CQI_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_RAWI_R2,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_RAWI_R3,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_RAWI_R5,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_IMGO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_BPCI_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_LCSI_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_UFEO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_LTMSO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_DRZB2NO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_AAO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L16_AFO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L17_YUVO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L17_YUVO_R3,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L17_YUVO_R2,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L17_YUVO_R5,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L17_RGBWI_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L17_TCYSO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L17_DRZ4NO_R3,
			mtk_cam_translation_fault_callback, (void *)raw, false);

	} else if (raw->id == RAW_B) {
		mtk_iommu_register_fault_callback(M4U_PORT_L30_CQI_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L30_RAWI_R2,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L30_RAWI_R3,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L30_RAWI_R5,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L30_IMGO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L30_BPCI_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L30_LCSI_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L30_UFEO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L30_LTMSO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L30_DRZB2NO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L30_AAO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L30_AFO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L34_YUVO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L34_YUVO_R3,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L34_YUVO_R2,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L34_YUVO_R5,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L34_RGBWI_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L34_TCYSO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L34_DRZ4NO_R3,
			mtk_cam_translation_fault_callback, (void *)raw, false);
	} else if (raw->id == RAW_C) {
		mtk_iommu_register_fault_callback(M4U_PORT_L31_CQI_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L31_RAWI_R2,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L31_RAWI_R3,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L31_RAWI_R5,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L31_IMGO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L31_BPCI_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L31_LCSI_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L31_UFEO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L31_LTMSO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L31_DRZB2NO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L31_AAO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L31_AFO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L35_YUVO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L35_YUVO_R3,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L35_YUVO_R2,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L35_YUVO_R5,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L35_RGBWI_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L35_TCYSO_R1,
			mtk_cam_translation_fault_callback, (void *)raw, false);

		mtk_iommu_register_fault_callback(M4U_PORT_L35_DRZ4NO_R3,
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
	struct mtk_cam_request_stream_data *s_data_ctx;
	struct mtk_cam_request *req;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	if (!ctx) {
		pr_info("%s: get ctx from s_data failed", __func__);
		return;
	}

	req = mtk_cam_s_data_get_req(s_data);
	s_data_ctx = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
	vb = &buf->vbb.vb2_buf;
	node = mtk_cam_vbq_to_vdev(vb->vb2_queue);

	buf->vbb.sequence = s_data->frame_seq_no;
	if (vb->vb2_queue->timestamp_flags & V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC)
		vb->timestamp = s_data->timestamp_mono;
	else
		vb->timestamp = s_data->timestamp;
	if (mtk_cam_ctx_has_raw(ctx) &&
	    mtk_cam_scen_is_ext_isp(s_data_ctx->feature.scen)) {
		switch (node->desc.id) {
		case MTK_RAW_MAIN_STREAM_SV_1_OUT:
		case MTK_RAW_MAIN_STREAM_SV_2_OUT:
		case MTK_RAW_META_SV_OUT_0:
		case MTK_RAW_META_SV_OUT_1:
		case MTK_RAW_META_SV_OUT_2:
		case MTK_RAW_META_OUT_0:
			dev_dbg(ctx->cam->dev,
			"%s:%s:vb sequence:%d, timestamp:%lld (mono:%d), pure/meta/proc:%lld/%lld/%lld\n",
			__func__, node->desc.name, buf->vbb.sequence, vb->timestamp,
			vb->vb2_queue->timestamp_flags & V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC,
			s_data->preisp_img_ts[0],
			s_data->preisp_meta_ts[0],
			s_data->preisp_img_ts[1]);
			break;
		default:
			break;
		}
	}
	/*check buffer's timestamp*/
	if (node->desc.dma_port == MTKCAM_IPI_RAW_META_STATS_CFG)
		dev_dbg(ctx->cam->dev,
			"%s:%s:vb sequence:%d, queue type:%d, timestamp_flags:0x%x, timestamp:%lld\n",
			__func__, node->desc.name, buf->vbb.sequence,
			vb->vb2_queue->type, vb->vb2_queue->timestamp_flags,
			vb->timestamp);
}

static void dump_request_status(struct mtk_cam_request *cam_req)
{
	struct media_request *req = &cam_req->req;
	struct device *dev = req->mdev->dev;
	unsigned long flags;
	int num_incomplete;

	/* media request internal */
	spin_lock_irqsave(&req->lock, flags);
	num_incomplete = req->num_incomplete_objects;
	spin_unlock_irqrestore(&req->lock, flags);

	if (!num_incomplete)
		return;

	dev_info(dev, "req:%s, state:%d, num_incomplete: %d\n",
		 req->debug_str, req->state, num_incomplete);

	/* TODO(MTK): use buffer queue */
}

static void mtk_cam_remove_req_from_running(struct mtk_cam_device *cam,
					    struct mtk_cam_request *req)
{
	dev_dbg(cam->dev,
		"%s:%s: real removed, ctx:(0x%x/0x%x), pipe:(0x%x/0x%x) done_status:0x%x)\n",
		__func__, req->req.debug_str,
		req->ctx_used, cam->streaming_ctx,
		req->pipe_used, cam->streaming_pipe,
		req->done_status);

	/* maybe no need */
	atomic_set(&req->state, MTK_CAM_REQ_STATE_COMPLETE);

	spin_lock(&cam->running_job_lock);
	list_del(&req->list);
	cam->running_job_count--;
	spin_unlock(&cam->running_job_lock);
}

int mtk_cam_mark_vbuf_done(struct mtk_cam_request *req,
			   struct mtk_cam_buffer *buf)
{
	struct mtk_cam_device *cam;
	struct mtk_cam_video_device *node;
	int current_buf_cnt;
	int p, i;
	bool is_last_buf;

	if (!req) {
		pr_info("%s: get req failed\n", __func__);
		return -1;
	}

	if (!buf) {
		pr_info("%s: get buf failed\n", __func__);
		return -1;
	}

	cam = container_of(req->req.mdev, struct mtk_cam_device, media_dev);
	current_buf_cnt = atomic_inc_return(&req->done_buf_cnt);
	node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);
	is_last_buf = (current_buf_cnt == req->enqeued_buf_cnt);

	dev_dbg(cam->dev,
		"%s:%s: %s complete(%d), cnt(%d/%d), is_last_buf(%d)\n",
		__func__, req->req.debug_str, node->desc.name, buf->final_state,
		current_buf_cnt, req->enqeued_buf_cnt, is_last_buf);

	/* to handle cleanup case, pipe buffers need release immediately */
	if (is_last_buf) {
		/* clean the req_stream_data being used right after request reinit */
		for (p = 0; p < cam->max_stream_num; p++) {
			if (req->pipe_used & (1 << p)) {
				for (i = 0; i < req->p_data[p].s_data_num; i++)
					mtk_cam_req_pipe_s_data_clean(req, p, i);
			}
		}
		if (atomic_read(&req->state) > MTK_CAM_REQ_STATE_PENDING)
			mtk_cam_remove_req_from_running(cam, req);
	}

	vb2_buffer_done(&buf->vbb.vb2_buf, buf->final_state);

	/* it is dangerous to access req after all buffer done */
	if (0 && is_last_buf)
		dump_request_status(req);

	return 0;
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
	int i, buf_state;
	unsigned int buf_start, buf_end, buf_ret_cnt;

	s_data_pipe = mtk_cam_req_get_s_data(req, pipe_id, index);
	if (!s_data_pipe) {
		pr_info("%s: get s_data pipe failed", __func__);
		return;
	}

	buf_start = 0;
	buf_end = 0;
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
#if PURE_RAW_WITH_SV_DONE_CHECK
		/*
		 * when the clean-up thread is running, the sv HW is disabled.
		 * therefore, MTK_RAW_MAIN_STREAM_OUT needs to be handled if
		 * it exist
		 */
		if (mtk_cam_s_data_is_pure_raw_with_sv(s_data_pipe) &&
		    (i == MTK_RAW_MAIN_STREAM_OUT)) {
			// debug only
			dev_dbg(cam->dev,
				"%s:%s:pipe_id(%d) skip IMGO\n",
				__func__, req->req.debug_str, pipe_id);
			continue;
		}
#endif
		/* make sure do not touch req/s_data after vb2_buffe_done */
		buf = mtk_cam_s_data_get_vbuf(s_data_pipe, i);
		if (!buf)
			continue;
		buf_ret[buf_ret_cnt++] = buf;
		/* clean the stream data for req reinit case */
		mtk_cam_s_data_reset_vbuf(s_data_pipe, i);
	}

	buf_state = atomic_read(&s_data_pipe->buf_state);
	if (buf_state == -1)
		buf_state = VB2_BUF_STATE_ERROR;
	if (mtk_cam_ctx_has_raw(s_data_pipe->ctx) &&
		mtk_cam_scen_is_ext_isp(&s_data_pipe->ctx->pipe->scen_active) &&
		is_camsv_subdev(pipe_id) &&
		buf_state == VB2_BUF_STATE_ERROR) {
		dev_info(cam->dev,
				"camsv %s:%s: seq:%d FORCED TO DONE\n",
				__func__, req->req.debug_str,
				s_data_pipe->frame_seq_no);
		buf_state = VB2_BUF_STATE_DONE;
	}
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

		buf->final_state = buf_state;
		mtk_cam_mark_vbuf_done(req, buf);
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

bool
mtk_cam_s_data_get_scen(struct mtk_cam_scen *scen,
			struct mtk_cam_request_stream_data *s_data)
{
	if (!s_data || !scen)
		return false;

	if (!is_raw_subdev(s_data->pipe_id) || !s_data->feature.scen) {
		mtk_cam_scen_init(scen);
		return false;
	}

	*scen = *s_data->feature.scen;

	return true;
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
	/* TBC: in legacy driver we only start watchog when the ctx has raw */
	if (ctx->sensor && mtk_cam_ctx_has_raw(ctx) &&
	    !mtk_cam_scen_is_m2m(&ctx->pipe->scen_active))
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
	/* check if composed error case */
	if (req_stream_data->flags & MTK_CAM_REQ_S_DATA_FLAG_COMPOMSED_ERROR) {
		dev_info(ctx->cam->dev,
			 "%s:%s:ctx(%d):req(%d):working_buf is composed error case\n", __func__,
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

static bool finish_sv_cq_buf(struct mtk_cam_request_stream_data *req_stream_data)
{
	bool result = false;
	struct mtk_cam_ctx *ctx = req_stream_data->ctx;
	struct mtk_camsv_working_buf_entry *cq_buf_entry;

	spin_lock(&ctx->sv_processing_buffer_list.lock);

	cq_buf_entry = req_stream_data->sv_working_buf;
	/* check if the cq buffer is already finished */
	if (!cq_buf_entry || !cq_buf_entry->s_data) {
		dev_info(ctx->cam->dev,
			 "%s:%s:ctx(%d):req(%d):sv_working_buf is already release\n", __func__,
			req_stream_data->req->req.debug_str, ctx->stream_id,
			req_stream_data->frame_seq_no);
		spin_unlock(&ctx->sv_processing_buffer_list.lock);
		return false;
	}

	list_del(&cq_buf_entry->list_entry);
	mtk_cam_s_data_reset_sv_wbuf(req_stream_data);
	ctx->sv_processing_buffer_list.cnt--;
	spin_unlock(&ctx->sv_processing_buffer_list.lock);

	mtk_cam_sv_working_buf_put(cq_buf_entry);
	result = true;

	dev_dbg(ctx->cam->dev, "put cq buf:%pad, %s\n",
			&cq_buf_entry->buffer.iova,
			req_stream_data->req->req.debug_str);

	return result;
}

bool finish_img_buf(struct mtk_cam_request_stream_data *req_stream_data)
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
	struct mtkcam_ipi_config_param *config_param)
{
	/* raw */
	config_param->maps[0].pipe_id = ctx->pipe->id;
	config_param->maps[0].dev_mask = MTKCAM_SUBDEV_RAW_MASK & ctx->used_raw_dev;
	config_param->n_maps = 1;
}

static int update_scen_param(struct mtk_cam_ctx *ctx,
	struct mtkcam_ipi_config_param *config_param, struct mtk_cam_scen *scen)
{
	struct device *dev = ctx->cam->dev;

	if (mtk_cam_scen_is_mstream_types(scen)) {
		config_param->sw_feature = MTKCAM_IPI_SW_FEATURE_VHDR;
		switch (scen->scen.mstream.type) {
		case MTK_CAM_MSTREAM_SE_NE:
			config_param->exp_order = MTKCAM_IPI_ORDER_SE_NE;
			break;
		case MTK_CAM_MSTREAM_NE_SE:
		default:
			config_param->exp_order = MTKCAM_IPI_ORDER_NE_SE;
			break;
		}
	} else if (mtk_cam_scen_is_stagger_types(scen)) {
		config_param->sw_feature = MTKCAM_IPI_SW_FEATURE_VHDR;
		switch (scen->scen.normal.exp_order) {
		case MTK_CAM_EXP_SE_LE:
			config_param->exp_order = MTKCAM_IPI_ORDER_SE_NE;
			break;
		case MTK_CAM_EXP_LE_SE:
		default:
			config_param->exp_order = MTKCAM_IPI_ORDER_NE_SE;
			break;
		}
	} else {
		config_param->sw_feature = MTKCAM_IPI_SW_FEATURE_NORMAL;
		config_param->exp_order = MTKCAM_IPI_ORDER_NE_SE;
	}
	dev_dbg(dev, "%s sw_feature:%d exp_order %d", __func__,
			config_param->sw_feature, config_param->exp_order);

	if (mtk_cam_scen_is_rgbw_enabled(scen)) {
		if (ctx->generic_buf.size < MTK_CAM_CACI_TABLE_SIZE) {
			int ret =
				mtk_cam_generic_buf_alloc(ctx, MTK_CAM_CACI_TABLE_SIZE);

			if (ret) {
				dev_info(dev, "generic buf alloc failed\n");
				return -EINVAL;
			}

			// for w path CAC table, must be 0 filled
			memset_io(ctx->generic_buf.va, 0, ctx->generic_buf.size);
		}

		config_param->w_cac_table.iova = ctx->generic_buf.iova;
		config_param->w_cac_table.size = ctx->generic_buf.size;

		dev_dbg(dev, "generic buf: iova(0x%8x) size(%d)\n",
				  config_param->w_cac_table.iova,
				  config_param->w_cac_table.size);

		// frame order
		switch (scen->scen.normal.frame_order) {
		case MTK_CAM_FRAME_BAYER_W:
			config_param->frame_order = MTKCAM_IPI_ORDER_BAYER_FIRST;
			break;
		case MTK_CAM_FRAME_W_BAYER:
		default:
			config_param->frame_order = MTKCAM_IPI_ORDER_W_FIRST;
			break;
		}
	}

	// vsync order
	if (ctx->seninf)
		config_param->vsync_order =
			mtk_cam_seninf_get_vsync_order(ctx->seninf);
	else
		config_param->vsync_order = MTKCAM_IPI_ORDER_BAYER_FIRST;

	return 0;
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

int mtk_cam_get_timestamp(struct mtk_cam_ctx *ctx,
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
			mtk_cam_raw_get_subsample_ratio(&ctx->pipe->res_config.scen);

	buf = mtk_cam_s_data_get_vbuf(s_data, MTK_RAW_META_OUT_0);
	if (!buf) {
		dev_info(ctx->cam->dev,
			 "ctx(%d): can't get MTK_RAW_META_OUT_0 buf from req(%d)\n",
			 ctx->stream_id, s_data->frame_seq_no);
		return -1;
	}

	vb = &buf->vbb.vb2_buf;
	if (!vb) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d): can't get vb2 buf\n",
			 __func__, ctx->stream_id);
		return -1;
	}

	vaddr = vb2_plane_vaddr(&buf->vbb.vb2_buf, 0);
	if (!vaddr) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d): can't get plane_vadd\n",
			 __func__, ctx->stream_id);
		return -1;
	}

	if ((s_data->working_buf->buffer.va == (void *)NULL) ||
		s_data->working_buf->buffer.size == 0) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d): can't get working_buf\n",
			 __func__, ctx->stream_id);
		return -1;
	}

	fho_va = (u32 *)(s_data->working_buf->buffer.va +
		s_data->working_buf->buffer.size - 64 * (subsample + 1));

	if (*fho_va == 0) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d):pipe(%d):seq(%d) get empty fho\n",
			 __func__, ctx->stream_id, s_data->pipe_id,
			 s_data->frame_seq_no);
		return -1;
	}

	pTimestamp = vaddr + GET_PLAT_V4L2(timestamp_buffer_ofst);
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

	return 0;
}

static void
mtk_cam_dequeue_update_dvfs_tbl(struct mtk_cam_ctx *ctx,
				struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_resource_config *res_cfg;
	struct mtk_cam_device *cam = ctx->cam;

	res_cfg = mtk_cam_s_data_get_res_cfg(s_data);
	if (res_cfg) {
		mutex_lock(&cam->dvfs_op_lock);
		mtk_cam_dvfs_tbl_del_opp(&ctx->dvfs_tbl, res_cfg->opp_idx);
		mtk_cam_dvfs_update_clk(cam, false);
		mutex_unlock(&cam->dvfs_op_lock);
	}
}

int mtk_cam_dequeue_req_frame(struct mtk_cam_ctx *ctx,
			      unsigned int dequeued_frame_seq_no,
			      unsigned int pipe_id)
{
	struct mtk_cam_request *req, *req_prev, *req_next;
	struct mtk_cam_request_stream_data *s_data, *s_data_pipe, *s_data_mstream, *s_data_next;
	struct mtk_cam_request_stream_data *deq_s_data[18];
	/* consider running_job_list depth and mstream(2 s_data): 3*3*2 */
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	int buf_state;
	struct mtk_cam_scen *scen;
	unsigned int dequeue_cnt, s_data_cnt, handled_cnt;
	bool del_job;
	bool chk_sensor_change;
	bool unreliable = false;
	int timestamp_error = 0;
	void *vaddr = NULL;
	struct mtk_ae_debug_data ae_data;
	int frame_seq_next;
	bool trigger_raw_switch;

	dequeue_cnt = 0;
	s_data_cnt = 0;
	mutex_lock(&ctx->cleanup_lock);
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
		chk_sensor_change = false;
		s_data = deq_s_data[handled_cnt];
		del_job = false;
		scen = s_data->feature.scen;
		trigger_raw_switch = false;
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

		if (is_raw_subdev(pipe_id))
			vaddr = mtk_cam_get_vbuf_va(ctx, s_data, MTK_RAW_META_OUT_0);

		if (is_raw_subdev(pipe_id) && debug_ae) {
			memset(&ae_data, 0, sizeof(ae_data));
			dump_aa_info(ctx, &ae_data);
			dev_info(ctx->cam->dev,
				"%s:%s:ctx(%d):pipe(%d):de-queue seq(%d):handle seq(%d),done(0x%x),pipes(req:0x%x,ctx:0x%x,all:0x%x),del_job(%d),metaout va(0x%llx),size(%d,%d),AA(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)|AA(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)\n",
				__func__, req->req.debug_str, ctx->stream_id, pipe_id,
				dequeued_frame_seq_no, s_data->frame_seq_no, req->done_status,
				req->pipe_used, ctx->streaming_pipe, ctx->cam->streaming_pipe,
				del_job, vaddr,
				req->raw_pipe_data[pipe_id].res.sensor_res.width,
				req->raw_pipe_data[pipe_id].res.sensor_res.height,
				ae_data.OBC_R1_Sum[0], ae_data.OBC_R1_Sum[1],
				ae_data.OBC_R1_Sum[2], ae_data.OBC_R1_Sum[3],
				ae_data.OBC_R2_Sum[0], ae_data.OBC_R2_Sum[1],
				ae_data.OBC_R2_Sum[2], ae_data.OBC_R2_Sum[3],
				ae_data.OBC_R3_Sum[0], ae_data.OBC_R3_Sum[1],
				ae_data.OBC_R3_Sum[2], ae_data.OBC_R3_Sum[3],
				ae_data.AA_Sum[0], ae_data.AA_Sum[1],
				ae_data.AA_Sum[2], ae_data.AA_Sum[3],
				ae_data.LTM_Sum[0], ae_data.LTM_Sum[1],
				ae_data.LTM_Sum[2], ae_data.LTM_Sum[3],
				ae_data.OBC_R1_Sum_W[0], ae_data.OBC_R1_Sum_W[1],
				ae_data.OBC_R1_Sum_W[2], ae_data.OBC_R1_Sum_W[3],
				ae_data.OBC_R2_Sum_W[0], ae_data.OBC_R2_Sum_W[1],
				ae_data.OBC_R2_Sum_W[2], ae_data.OBC_R2_Sum_W[3],
				ae_data.OBC_R3_Sum_W[0], ae_data.OBC_R3_Sum_W[1],
				ae_data.OBC_R3_Sum_W[2], ae_data.OBC_R3_Sum_W[3],
				ae_data.AA_Sum_W[0], ae_data.AA_Sum_W[1],
				ae_data.AA_Sum_W[2], ae_data.AA_Sum_W[3],
				ae_data.LTM_Sum_W[0], ae_data.LTM_Sum_W[1],
				ae_data.LTM_Sum_W[2], ae_data.LTM_Sum_W[3]);
		} else
			dev_info(ctx->cam->dev,
				"%s:%s:ctx(%d):pipe(%d):de-queue seq(%d):handle seq(%d),done(0x%x),pipes(req:0x%x,ctx:0x%x,all:0x%x),del_job(%d),metaout va(0x%llx)\n",
				__func__, req->req.debug_str, ctx->stream_id, pipe_id,
				dequeued_frame_seq_no, s_data->frame_seq_no, req->done_status,
				req->pipe_used, ctx->streaming_pipe, ctx->cam->streaming_pipe,
				del_job, vaddr);

		spin_unlock(&req->done_status_lock);

		if (mtk_cam_scen_is_mstream_2exp_types(scen))
			s_data_mstream = mtk_cam_req_get_s_data(req, ctx->stream_id, 1);
		else
			s_data_mstream = NULL;

		if (is_raw_subdev(pipe_id)) {
			timestamp_error = mtk_cam_get_timestamp(ctx, s_data);
			mtk_cam_req_dbg_works_clean(s_data);
			mtk_cam_req_works_clean(s_data);

			if (s_data_mstream) {
				mtk_cam_req_dbg_works_clean(s_data_mstream);
				mtk_cam_req_works_clean(s_data_mstream);
			}
		}

		if (del_job) {
			mtk_camsys_state_delete(ctx, sensor_ctrl, req);

			/* release internal buffers */
			finish_cq_buf(s_data);
			finish_sv_cq_buf(s_data);
			mtk_cam_mraw_finish_buf(s_data);
			mtk_cam_dequeue_update_dvfs_tbl(ctx, s_data);

			if (mtk_cam_ctx_has_raw(ctx) &&
			    mtk_cam_scen_is_time_shared(scen))
				finish_img_buf(s_data);

			if (s_data_mstream) {
				finish_cq_buf(s_data_mstream);
				finish_sv_cq_buf(s_data_mstream);
				mtk_cam_mraw_finish_buf(s_data_mstream);
				mtk_cam_dequeue_update_dvfs_tbl(ctx, s_data_mstream);
			}

			if (s_data->frame_seq_no == dequeued_frame_seq_no)
				chk_sensor_change = true;
		}

		if (mtk_cam_scen_is_mstream(scen) &&
		    scen->scen.mstream.type != MTK_CAM_MSTREAM_1_EXPOSURE) {
			unreliable |= (s_data->flags &
					(MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_DELAYED |
					MTK_CAM_REQ_S_DATA_FLAG_INCOMPLETE));

			if (s_data_mstream) {
				unreliable |= (s_data_mstream->flags &
						(MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_DELAYED |
						MTK_CAM_REQ_S_DATA_FLAG_INCOMPLETE));
			}
		}

		/* release vb2 buffers of the pipe */
		s_data_pipe = mtk_cam_req_get_s_data(req, pipe_id, 0);
		if (!s_data_pipe) {
			dev_info(ctx->cam->dev,
				"%s:%s:ctx(%d):pipe(%d):seq(%d) s_data_pipe not found\n",
				__func__, req->req.debug_str, ctx->stream_id, pipe_id,
				s_data->frame_seq_no);
			if (del_job)
				atomic_dec(&ctx->running_s_data_cnt);
			continue;
		}

		if (s_data->frame_seq_no < dequeued_frame_seq_no) {
			buf_state = VB2_BUF_STATE_DONE;
			dev_info(ctx->cam->dev,
				"%s:%s:pipe(%d) seq:%d, time:%lld, ctx:%d done worker delay\n",
				__func__, req->req.debug_str, pipe_id,
				s_data->frame_seq_no, s_data->timestamp,
				ctx->stream_id);
		} else if (s_data->state.estate == E_STATE_DONE_MISMATCH) {
			buf_state = VB2_BUF_STATE_DONE;
			dev_dbg(ctx->cam->dev,
				"%s:%s:pipe(%d) seq:%d, state done mismatch",
				__func__, req->req.debug_str, pipe_id,
				s_data->frame_seq_no);
		} else if (unreliable) {
			buf_state = VB2_BUF_STATE_ERROR;
			s_data->flags = 0;
			if (s_data_mstream)
				s_data_mstream->flags = 0;
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

		if (timestamp_error) {
			dev_info(ctx->cam->dev,
				"%s:%s:pipe(%d) seq:%d, ctx:%d timestamp_error\n",
				__func__, req->req.debug_str, pipe_id,
				s_data->frame_seq_no, ctx->stream_id);
			buf_state = VB2_BUF_STATE_ERROR;
		}

		if (mtk_cam_s_data_set_buf_state(s_data_pipe, buf_state)) {
			/* handle vb2_buffer_done */
			if (mtk_cam_req_put(req, pipe_id)) {
				/* request is removed */
				dequeue_cnt++;
				dev_dbg(ctx->cam->dev,
					"%s:%s:pipe(%d) return request",
					__func__, req->req.debug_str, pipe_id);
			}
		}

		/* Serialized raw switch check and try queue flow*/
		mutex_lock(&ctx->cam->queue_lock);

		/**
		 * running_s_data_cnt updated must be along with
		 * raw switch trigger since we use it
		 * to determine if we should start the switch flow in try queue
		 * or dequeue timing
		 */
		if (del_job)
			atomic_dec(&ctx->running_s_data_cnt);

		if (chk_sensor_change) {
			frame_seq_next = dequeued_frame_seq_no + 1;
			req_next = mtk_cam_get_req(ctx, frame_seq_next);
			if (!req_next) {
				dev_dbg(ctx->cam->dev, "%s next req (%d) not queued\n",
					__func__, frame_seq_next);
			} else {
				dev_dbg(ctx->cam->dev,
					"%s:req(%d) check: req->ctx_used:0x%x, req->ctx_link_update0x%x\n",
					__func__, frame_seq_next, req_next->ctx_used,
					req_next->ctx_link_update);
				if ((req_next->ctx_used & (1 << ctx->stream_id)) &&
				    mtk_cam_is_nonimmediate_switch_req(req_next, ctx->stream_id))
					trigger_raw_switch = true;
				else
					dev_dbg(ctx->cam->dev, "%s next req (%d) no link stup\n",
						__func__, frame_seq_next);
			}
		}

		/* release the lock once we know if raw switch needs to be triggered or not here */
		mutex_unlock(&ctx->cam->queue_lock);

		if (trigger_raw_switch) {
			s_data_next = mtk_cam_req_get_s_data(req_next, ctx->stream_id, 0);
			mtk_camsys_raw_change_pipeline(ctx, &ctx->sensor_ctrl, s_data_next);
		}
	}
	mutex_unlock(&ctx->cleanup_lock);
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
	struct mtk_cam_request_stream_data *s_data;
	struct mtk_cam_request_stream_data *clean_s_data[18];
	/* consider running_job_list depth and mstream(2 s_data): 3*3*2 */
#if PURE_RAW_WITH_SV_DONE_CHECK
	struct mtk_cam_buffer *buf;
#endif
	struct list_head *running = &cam->running_job_list;
	unsigned int num_s_data, s_data_cnt, handled_cnt;
	int i;

	mutex_lock(&ctx->cleanup_lock);
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
				/* make sure do not touch req/s_data after vb2_buffe_done */
				media_request_get(&req->req);
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
	mutex_unlock(&ctx->cleanup_lock);

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
					"%s:%s:pipe(%d):seq(%d): clean s_data_%d, scen(%s)\n",
					__func__, req->req.debug_str, pipe_id,
					s_data->frame_seq_no, s_data->index,
					s_data->feature.scen->dbg_str);
			else
				dev_dbg(cam->dev,
					"%s:%s:pipe(%d):seq(%d): clean s_data_%d, scen(%s)\n",
					__func__, req->req.debug_str, pipe_id,
					s_data->frame_seq_no, s_data->index,
					s_data->feature.scen->dbg_str);
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

#if PURE_RAW_WITH_SV_DONE_CHECK
		if (mtk_cam_s_data_is_pure_raw_with_sv(s_data)) {
			if (atomic_read(&s_data->pure_raw_done_work.is_queued)) {
				cancel_work_sync(&s_data->pure_raw_done_work.work);
				dev_info(cam->dev,
					 "%s:%s:pipe(%d):seq(%d): cancel pure_raw_done_work\n",
					 __func__, req->req.debug_str, pipe_id,
					 s_data->frame_seq_no);
			}
			atomic_set(&s_data->pure_raw_done_work.is_queued, 1);
			/* force return IMGO buf if it still exists */
			buf = mtk_cam_s_data_get_vbuf(s_data,
						      MTK_RAW_MAIN_STREAM_OUT);
			if (buf) {
				mtk_cam_s_data_reset_vbuf(s_data,
							  MTK_RAW_MAIN_STREAM_OUT);
				buf->final_state = VB2_BUF_STATE_ERROR;
				mtk_cam_mark_vbuf_done(req, buf);
				dev_info(cam->dev,
					 "%s:%s:pipe(%d):seq(%d): force IMGO done\n",
					 __func__, req->req.debug_str, pipe_id,
					 s_data->frame_seq_no);
			}
		}
#endif

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

		/* no need, because the request will be valid until last buf done */
		spin_unlock(&req->done_status_lock);

		mtk_cam_complete_sensor_hdl(s_data);
		mtk_cam_complete_raw_hdl(s_data);

		/*
		 * reset fs state, if one sensor off and another one alive,
		 * Let the req be the single sensor case.
		 */
		mutex_lock(&req->fs.op_lock);
		mtk_cam_fs_reset(&req->fs);
		mutex_unlock(&req->fs.op_lock);

		if (mtk_cam_s_data_set_buf_state(s_data, buf_state)) {
			if (s_data->index > 0) {
				mtk_cam_req_return_pipe_buffers(req, pipe_id,
								s_data->index);
				/* get_vbuf is nothing, just clean s_data */
			} else {
				/* handle vb2_buffer_done */
				if (mtk_cam_req_put(req, pipe_id))
					dev_dbg(cam->dev,
						"%s:%s:pipe(%d) return request",
						__func__, req->req.debug_str,
						pipe_id);
			}
		}

		/* make sure do not touch req/s_data after vb2_buffe_done */
		media_request_put(&req->req);
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
				struct mtk_cam_scen *scen)
{
	struct mtkcam_ipi_img_input *in_fmt;
	int input_node;
	struct mtk_cam_ctx *ctx = req_stream_data->ctx;

	if (node->desc.dma_port != MTKCAM_IPI_RAW_IMGO &&
			node->desc.dma_port != MTKCAM_IPI_RAW_RAWI_2)
		return;

	if (mtk_cam_scen_is_m2m(scen)) {
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
					input_node = MTKCAM_IPI_RAW_RAWI_5;
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

static void mtk_cam_fill_sv_frame_param(struct mtk_cam_ctx *ctx,
	struct mtkcam_ipi_frame_param *frame_param, unsigned int tag_idx,
	struct mtkcam_ipi_pix_fmt fmt, __u64 iova)
{
	struct mtkcam_ipi_img_output *out_fmt;

	out_fmt =
		&frame_param->camsv_param[0][tag_idx].camsv_img_outputs[0];

	frame_param->camsv_param[0][tag_idx].pipe_id =
		ctx->sv_dev->id + MTKCAM_SUBDEV_CAMSV_START;
	frame_param->camsv_param[0][tag_idx].tag_id = tag_idx;
	frame_param->camsv_param[0][tag_idx].hardware_scenario =
		ctx->sv_dev->tag_info[tag_idx].hw_scen;
	out_fmt->uid.id = MTKCAM_IPI_CAMSV_MAIN_OUT;
	out_fmt->uid.pipe_id = ctx->sv_dev->id + MTKCAM_SUBDEV_CAMSV_START;
	out_fmt->fmt = fmt;
	out_fmt->buf[0][0].iova = iova;
}

static void config_img_in_fmt_stagger(struct mtk_cam_device *cam,
				struct mtk_cam_request_stream_data *s_data,
				struct mtk_cam_video_device *node,
				const struct v4l2_format *cfg_fmt,
				struct mtk_cam_scen *scen, s64 hw_mode)
{
	struct mtk_cam_ctx *ctx = s_data->ctx;
	struct mtkcam_ipi_img_input *in_fmt, *in_fmt_w;
	int input_node;
	int rawi_port_num = 0;
	int rawi_idx = 0;
	const int *ptr_rawi = NULL;
	int (*func_ptr_arr[3])(struct mtk_cam_ctx *, unsigned int, bool);
	int tag_idx = 0, tag_idx_w = 0;
	unsigned int exp_no = 0;
	unsigned int uid_w = MTKCAM_IPI_RAW_ID_UNKNOWN, img_in_idx_w;
	bool is_otf = mtk_cam_hw_mode_is_otf(hw_mode);
	bool is_dc = mtk_cam_hw_mode_is_dc(hw_mode);
	bool is_rgbw = mtk_cam_scen_is_rgbw_enabled(scen);

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
					MTKCAM_IPI_RAW_RAWI_2, MTKCAM_IPI_RAW_RAWI_5};
	static const int stagger_m2m_3exp_rawi[3] = {
					MTKCAM_IPI_RAW_RAWI_2, MTKCAM_IPI_RAW_RAWI_3,
					MTKCAM_IPI_RAW_RAWI_5};

	func_ptr_arr[0] = get_first_sv_tag_idx;
	func_ptr_arr[1] = get_second_sv_tag_idx;
	func_ptr_arr[2] = get_last_sv_tag_idx;

	if (node->desc.dma_port != MTKCAM_IPI_RAW_IMGO &&
		node->desc.dma_port != MTKCAM_IPI_RAW_RAWI_2)
		return;

	if (is_otf) {
		if (mtk_cam_scen_is_2_exp(scen)) {
			rawi_port_num = 1;
			ptr_rawi = stagger_onthefly_2exp_rawi;
			exp_no = 2;
		} else if (mtk_cam_scen_is_3_exp(scen)) {
			rawi_port_num = 2;
			ptr_rawi = stagger_onthefly_3exp_rawi;
			exp_no = 3;
		}
	} else if (is_dc) {
		if (mtk_cam_scen_is_2_exp(scen)) {
			rawi_port_num = 2;
			ptr_rawi = stagger_dcif_2exp_rawi;
			exp_no = 2;
		} else if (mtk_cam_scen_is_3_exp(scen)) {
			rawi_port_num = 3;
			ptr_rawi = stagger_dcif_3exp_rawi;
			exp_no = 3;
		} else {
			rawi_port_num = 1;
			ptr_rawi = stagger_dcif_1exp_rawi;
			exp_no = 1;
		}
	} else if (mtk_cam_hw_mode_is_m2m(hw_mode)) {
		if (mtk_cam_scen_is_2_exp(scen)) {
			rawi_port_num = 2;
			ptr_rawi = stagger_m2m_2exp_rawi;
		} else if (mtk_cam_scen_is_3_exp(scen)) {
			rawi_port_num = 3;
			ptr_rawi = stagger_m2m_3exp_rawi;
		}
	}

	if (ptr_rawi == NULL) {
		dev_info(cam->dev, "%s no rawi", __func__);
		return;
	}

	for (rawi_idx = 0; rawi_idx < rawi_port_num; rawi_idx++) {
		if (is_otf || is_dc) {
			tag_idx = (is_dc && (rawi_port_num > 1) &&
				((rawi_idx + 1) == rawi_port_num)) ?
				(*func_ptr_arr[2])(ctx,
					exp_no, false) :
				(*func_ptr_arr[rawi_idx])(s_data->ctx,
					exp_no, false);
			if (is_rgbw) {
				tag_idx_w = (is_dc && (rawi_port_num > 1) &&
					((rawi_idx + 1) == rawi_port_num)) ?
					(*func_ptr_arr[2])(ctx,
						exp_no, true) :
					(*func_ptr_arr[rawi_idx])(s_data->ctx,
						exp_no, true);
			}
		}
		input_node = ptr_rawi[rawi_idx];
		in_fmt = &s_data->frame_params.img_ins[input_node - MTKCAM_IPI_RAW_RAWI_2];
		in_fmt->uid.id = input_node;
		in_fmt->uid.pipe_id = node->uid.pipe_id;
		in_fmt->fmt.format = mtk_cam_get_img_fmt(cfg_fmt->fmt.pix_mp.pixelformat);
		in_fmt->fmt.s.w = cfg_fmt->fmt.pix_mp.width;
		in_fmt->fmt.s.h = cfg_fmt->fmt.pix_mp.height;
		in_fmt->fmt.stride[0] = cfg_fmt->fmt.pix_mp.plane_fmt[0].bytesperline;

		in_fmt_w = NULL;
		if (is_rgbw) {
			uid_w = MTKCAM_IPI_RAW_ID_UNKNOWN;

			switch (in_fmt->uid.id) {
			case MTKCAM_IPI_RAW_RAWI_2:
				uid_w = MTKCAM_IPI_RAW_RAWI_2_W;
				img_in_idx_w = MTK_CAM_IMGO_W_IMG_IN_R2_IDX;
				break;
			case MTKCAM_IPI_RAW_RAWI_5:
				uid_w = MTKCAM_IPI_RAW_RAWI_5_W;
				img_in_idx_w = MTK_CAM_IMGO_W_IMG_IN_R5_IDX;
				break;
			default:
				break;
			}

			if (uid_w != MTKCAM_IPI_RAW_ID_UNKNOWN) {
				in_fmt_w = &s_data->frame_params.img_ins[img_in_idx_w];

				in_fmt_w->uid.id = uid_w;
				in_fmt_w->uid.pipe_id = in_fmt->uid.pipe_id;

				in_fmt_w->fmt = in_fmt->fmt;
			}
		}

		if (is_otf || is_dc) {
			mtk_cam_fill_sv_frame_param(ctx, &s_data->frame_params,
				tag_idx, in_fmt->fmt, in_fmt->buf[0].iova);
			if (is_rgbw) {
				if (in_fmt_w)
					mtk_cam_fill_sv_frame_param(ctx, &s_data->frame_params,
						tag_idx_w, in_fmt_w->fmt, in_fmt_w->buf[0].iova);
				else
					dev_info(cam->dev, "in_fmt_w shall not be null");
			}
		}

		dev_dbg(cam->dev,
		"[stagger-%d] ctx: %d dma_port:%d size=%dx%d, stride:%d, fmt:0x%x (iova:0x%x)\n",
		rawi_idx, s_data->ctx->stream_id, input_node, in_fmt->fmt.s.w,
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
		struct mtk_cam_request_stream_data *s_data,
		struct mtkcam_ipi_img_input *in_fmt,
		const struct v4l2_format *vfmt,
		int rawi_port_num)
{
	// Note: stagger/mstream use RAWI_R2 as first exp
	// the iova top of imgo buffer is set to first exp
	// change if needed
	bool is_imgo_buf_top =
		in_fmt->uid.id == MTKCAM_IPI_RAW_RAWI_2;

	if (s_data->feature.scen &&
		mtk_cam_scen_is_hdr_save_mem(s_data->feature.scen)) {
		if (is_imgo_buf_top) {
			__u64 imgo_iova_top = s_data->frame_params.img_ins[
				in_fmt->uid.id - MTKCAM_IPI_RAW_RAWI_2].buf[0].iova;

			/*raw's imgo using buf from user*/
			s_data->frame_params.img_outs[0].buf[0][0].iova = imgo_iova_top;

			if (mtk_cam_scen_is_rgbw_enabled(s_data->feature.scen)) {
				s_data->frame_params.img_outs[
					MTK_CAM_IMGO_W_IMG_OUT_IDX].buf[0][0].iova = imgo_iova_top +
						vfmt->fmt.pix_mp.plane_fmt[0].sizeimage;
			}
		}

		/* use img working buf */
		in_fmt->buf[0].iova = 0x0;
		dev_dbg(s_data->ctx->cam->dev, "%s: req:%d, ipi video id:%d\n",
			__func__, s_data->frame_seq_no, in_fmt->uid.id);
	}
}

unsigned int mtk_cam_get_sv_idle_tags(
	struct mtk_cam_ctx *ctx, const unsigned int enabled_tags, int hw_scen,
	int exp_no, int req_amount, bool is_check, bool is_rgbw)
{
	unsigned int used_tags = 0;
	int i;

	/* camsv todo: refine it */

	if (hw_scen & (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_ON_THE_FLY))) {
		if (req_amount == 1)
			used_tags = (1 << SVTAG_2);
	} else if (hw_scen & (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_STAGGER))) {
		if (exp_no == 1 && req_amount == 1) {
			used_tags = (1 << SVTAG_2);
			used_tags |= (is_rgbw) ? (1 << SVTAG_3) : 0;
		} else if (exp_no == 2 && req_amount == 1) {
			used_tags = (1 << SVTAG_0);
			used_tags |= (is_rgbw) ? (1 << SVTAG_1) : 0;
		} else if (exp_no == 2 && req_amount == 2) {
			used_tags = (1 << SVTAG_0) | (1 << SVTAG_2);
			used_tags |= (is_rgbw) ? (1 << SVTAG_1) | (1 << SVTAG_3) : 0;
		} else if (exp_no == 3 && req_amount == 2)
			used_tags = (1 << SVTAG_0) | (1 << SVTAG_1);
		else if (exp_no == 3 && req_amount == 3)
			used_tags = (1 << SVTAG_0) | (1 << SVTAG_1) | (1 << SVTAG_2);
	} else if (hw_scen & (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_DC_STAGGER)) ||
				hw_scen & (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER))) {
		if (exp_no == 1 && req_amount == 1) {
			used_tags = (1 << SVTAG_2);
			used_tags |= (is_rgbw) ? (1 << SVTAG_3) : 0;
		} else if (exp_no == 2 && req_amount == 2) {
			used_tags = (1 << SVTAG_0) | (1 << SVTAG_2);
			used_tags |= (is_rgbw) ? (1 << SVTAG_1) | (1 << SVTAG_3) : 0;
		} else if (exp_no == 3 && req_amount == 3)
			used_tags = (1 << SVTAG_0) | (1 << SVTAG_1) | (1 << SVTAG_2);
	} else if (hw_scen & (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_OFFLINE))) {
		if (req_amount == 1)
			used_tags = (1 << SVTAG_2);
	} else if (hw_scen & (1 << MTKCAM_SV_SPECIAL_SCENARIO_ADDITIONAL_RAW)) {
		if (req_amount == 1)
			used_tags = (1 << SVTAG_3);
	} else if (hw_scen & (1 << MTKCAM_SV_SPECIAL_SCENARIO_EXT_ISP)) {
		if (req_amount == 2)
			used_tags = (1 << SVTAG_0) | (1 << SVTAG_3);
		else if (req_amount == 3)
			used_tags = (1 << SVTAG_0) | (1 << SVTAG_1) | (1 << SVTAG_3);
		else if (req_amount == 1)
			used_tags = (1 << SVTAG_3);
	} else if (hw_scen & (1 << MTKCAM_SV_SPECIAL_SCENARIO_DISPLAY_IC)) {
		if (req_amount == 3)
			used_tags = (1 << SVTAG_0) | (1 << SVTAG_1) | (1 << SVTAG_2);
	} else {
		for (i = SVTAG_META_START; i < SVTAG_META_END; i++) {
			if (!(enabled_tags & (1 << i))) {
				used_tags |= (1 << i);
				break;
			}
		}
	}

	if (is_check && enabled_tags & used_tags) {
		used_tags = 0;
		dev_info(ctx->cam->dev, "no available sv tags: hw_scen:%d exp_no:%d req_amount:%d enabled_tags:0x%x",
			hw_scen, exp_no, req_amount, enabled_tags);
	}

	return used_tags;
}

static inline const struct v4l2_format *get_fmt_for_rawi(struct mtk_cam_ctx *ctx,
	struct mtk_cam_request_stream_data *s_data)
{
	const struct v4l2_format *fmt;
	struct mtk_cam_video_device *node;
	const struct v4l2_format *cfg_fmt;
	bool is_dc = mtk_cam_hw_mode_is_dc(ctx->pipe->hw_mode_pending);

	node = &ctx->pipe->vdev_nodes[
		MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];

	if (is_dc)
		fmt = &ctx->pipe->img_fmt_sink_pad;
	else {
		cfg_fmt = mtk_cam_s_data_get_vfmt(s_data, node->desc.id);
		if (cfg_fmt) {
			/* workaround for raw switch */
			if (!cfg_fmt->fmt.pix_mp.pixelformat)
				cfg_fmt = &node->active_fmt;
		} else {
			pr_info("%s:seq:%d get vfmt(%d) failed\n",
				__func__, s_data->frame_seq_no, node->desc.id);
		}

		fmt = cfg_fmt;
	}

	return fmt;
}

static int set_img_in_format(struct mtk_cam_device *cam,
	struct mtk_cam_ctx *ctx,
	struct mtkcam_ipi_img_input *in_fmt,
	struct mtk_cam_request_stream_data *s_data,
	int uid, int pipe_id, const struct v4l2_format *fmt_for_rawi)
{
	struct mtk_cam_img_working_buf_entry *buf_entry;

	in_fmt->uid.id = uid;
	in_fmt->uid.pipe_id = pipe_id;
	in_fmt->fmt.format = mtk_cam_get_img_fmt(
			fmt_for_rawi->fmt.pix_mp.pixelformat);
	in_fmt->fmt.s.w = fmt_for_rawi->fmt.pix_mp.width;
	in_fmt->fmt.s.h = fmt_for_rawi->fmt.pix_mp.height;
	in_fmt->fmt.stride[0] =
		fmt_for_rawi->fmt.pix_mp.plane_fmt[0].bytesperline;
	/* prepare working buffer */
	buf_entry = mtk_cam_img_working_buf_get(ctx);
	if (!buf_entry) {
		dev_info(cam->dev,
			 "%s: No img buf availablle: req:%d\n",
			 __func__, s_data->frame_seq_no);
		WARN_ON(1);
		return -EINVAL;
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

	return 0;
}

static void check_stagger_buffer(struct mtk_cam_device *cam,
				 struct mtk_cam_ctx *ctx,
				 struct mtk_cam_request *cam_req)
{
	struct mtkcam_ipi_img_input *in_fmt, *in_fmt_w;
	struct mtk_cam_video_device *node;
	struct mtk_cam_request_stream_data *s_data;
	struct mtk_cam_req_raw_pipe_data *s_raw_pipe_data;
	const struct v4l2_format *fmt_for_rawi;
	int input_node, rawi_port_num = 0, rawi_idx = 0;
	const int *ptr_rawi = NULL;
	struct mtk_cam_scen *scen;
	bool is_dc = mtk_cam_hw_mode_is_dc(ctx->pipe->hw_mode_pending);
	bool is_rgbw = false;
	int (*func_ptr_arr[3])(struct mtk_cam_ctx *, unsigned int, bool);
	int tag_idx = 0, tag_idx_w = 0;
	unsigned int enabled_sv_tags = 0;
	int uid_w, img_in_idx_w;

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

	func_ptr_arr[0] = get_first_sv_tag_idx;
	func_ptr_arr[1] = get_second_sv_tag_idx;
	func_ptr_arr[2] = get_last_sv_tag_idx;

	/* check the raw pipe only */
	s_data = mtk_cam_req_get_s_data(cam_req, ctx->stream_id, 0);
	s_raw_pipe_data = mtk_cam_s_data_get_raw_pipe_data(s_data);
	scen = s_data->feature.scen;

	is_rgbw = mtk_cam_scen_is_rgbw_enabled(scen);

	if (mtk_cam_hw_is_otf(ctx)) {
		s_data->frame_params.raw_param.hardware_scenario =
			MTKCAM_IPI_HW_PATH_STAGGER;
		if (mtk_cam_scen_is_2_exp(scen)) {
			s_data->frame_params.raw_param.exposure_num = 2;
			rawi_port_num = 1;
			ptr_rawi = stagger_onthefly_2exp_rawi;
		} else if (mtk_cam_scen_is_3_exp(scen)) {
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
		enabled_sv_tags = mtk_cam_get_sv_idle_tags(ctx,
			ctx->sv_dev->enabled_tags,
			1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_STAGGER),
			s_data->frame_params.raw_param.exposure_num,
			rawi_port_num, false, is_rgbw);
	} else if (is_dc) {
		s_data->frame_params.raw_param.hardware_scenario = (is_rgbw) ?
			MTKCAM_IPI_HW_PATH_DC_RGBW : MTKCAM_IPI_HW_PATH_DC_STAGGER;
		if (mtk_cam_scen_is_2_exp(scen)) {
			s_data->frame_params.raw_param.exposure_num = 2;
			rawi_port_num = 2;
			ptr_rawi = stagger_dcif_2exp_rawi;
		} else if (mtk_cam_scen_is_3_exp(scen)) {
			s_data->frame_params.raw_param.exposure_num = 3;
			rawi_port_num = 3;
			ptr_rawi = stagger_dcif_3exp_rawi;
		} else {
			s_data->frame_params.raw_param.exposure_num = 1;
			rawi_port_num = 1;
			ptr_rawi = stagger_dcif_1exp_rawi;
		}
		enabled_sv_tags = mtk_cam_get_sv_idle_tags(ctx,
			ctx->sv_dev->enabled_tags,
			1 << HWPATH_ID(
				s_data->frame_params.raw_param.hardware_scenario),
			s_data->frame_params.raw_param.exposure_num,
			rawi_port_num, false, is_rgbw);
	}

	/* update enabled_sv_tags to raw_pipe_data only in dcif case */
	/* purpose: to re-trigger sv's rcnt_inc if in-complete frame occurs in raw */
	if (s_raw_pipe_data)
		s_raw_pipe_data->enabled_sv_tags |= enabled_sv_tags;

	for (rawi_idx = 0; rawi_idx < rawi_port_num; rawi_idx++) {
		tag_idx = (is_dc && (rawi_port_num > 1) &&
			((rawi_idx + 1) == rawi_port_num)) ?
			(*func_ptr_arr[2])(ctx,
				s_data->frame_params.raw_param.exposure_num, false) :
			(*func_ptr_arr[rawi_idx])(ctx,
				s_data->frame_params.raw_param.exposure_num, false);
		if (is_rgbw) {
			tag_idx_w = (is_dc && (rawi_port_num > 1) &&
				((rawi_idx + 1) == rawi_port_num)) ?
				(*func_ptr_arr[2])(ctx,
					s_data->frame_params.raw_param.exposure_num, true) :
				(*func_ptr_arr[rawi_idx])(ctx,
					s_data->frame_params.raw_param.exposure_num, true);
		}
		input_node = ptr_rawi[rawi_idx];
		in_fmt = &s_data->frame_params.img_ins[
					input_node - MTKCAM_IPI_RAW_RAWI_2];
		fmt_for_rawi = get_fmt_for_rawi(ctx, s_data);
		if (!fmt_for_rawi)
			return;
		check_buffer_mem_saving(s_data, in_fmt, fmt_for_rawi, rawi_port_num);
		if (in_fmt->buf[0].iova == 0x0) {
			node = &ctx->pipe->vdev_nodes[
				MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
			if (set_img_in_format(cam, ctx, in_fmt, s_data, input_node,
					      node->uid.pipe_id, fmt_for_rawi))
				return;


			in_fmt_w = NULL;
			if (is_rgbw) {
				switch (input_node) {
				case MTKCAM_IPI_RAW_RAWI_2:
					uid_w = MTKCAM_IPI_RAW_RAWI_2_W;
					img_in_idx_w = MTK_CAM_IMGO_W_IMG_IN_R2_IDX;
					break;
				case MTKCAM_IPI_RAW_RAWI_5:
					uid_w = MTKCAM_IPI_RAW_RAWI_5_W;
					img_in_idx_w = MTK_CAM_IMGO_W_IMG_IN_R5_IDX;
					break;
				default:
					img_in_idx_w = -1;
					break;
				}

				if (img_in_idx_w != -1) {
					in_fmt_w = &s_data->frame_params.img_ins[img_in_idx_w];

					if (set_img_in_format(cam, ctx, in_fmt_w, s_data, uid_w,
						node->uid.pipe_id, fmt_for_rawi))
						return;
				}
			}

			/* update camsv's frame parameter */
			mtk_cam_fill_sv_frame_param(ctx, &s_data->frame_params,
				tag_idx, in_fmt->fmt, in_fmt->buf[0].iova);
			if (is_rgbw) {
				if (in_fmt_w)
					mtk_cam_fill_sv_frame_param(ctx, &s_data->frame_params,
						tag_idx_w, in_fmt_w->fmt, in_fmt_w->buf[0].iova);
				else
					dev_info(cam->dev, "in_fmt_w shall not be null");
			}
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
		if (cfg_fmt) {
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
		} else {
			dev_info(cam->dev,
				"%s:%s:seq:%d get vfmt(%d) failed\n",
				__func__, cam_req->req.debug_str,
				req->frame_seq_no, node->desc.id);
		}
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

	/* FIXME : 4plane format workaround */
	if (node->desc.dma_port == MTKCAM_IPI_RAW_YUVO_1 &&
			is_4_plane_rgb(cfg_fmt->fmt.pix_mp.pixelformat)) {
		out_fmt->fmt.s.w >>= 1;
		out_fmt->fmt.s.h >>= 1;
		dev_info(cam->dev,
			"pipe: %d dma_port:%d size=%0dx%0d, stride:%d, crop=%0dx%0d\n",
			node->uid.pipe_id, node->desc.dma_port, out_fmt->fmt.s.w,
			out_fmt->fmt.s.h, out_fmt->fmt.stride[0], out_fmt->crop.s.w,
			out_fmt->crop.s.h);
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
				struct mtk_cam_scen *scen)
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
	config_img_in_fmt_mstream(cam, req_stream_data, node, cfg_fmt, scen);

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
	struct mtk_cam_scen *scen;

	desc_id = MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SOURCE_BEGIN;
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
		scen = req_stream_data->feature.scen;
		is_m2m = mtk_cam_scen_is_mstream_m2m(scen) &&
			scen->scen.mstream.type != MTK_CAM_MSTREAM_1_EXPOSURE;

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
		if (cfg_fmt) {
			/* workaround for raw switch */
			if (!cfg_fmt->fmt.pix_mp.pixelformat)
				cfg_fmt = &vdev->active_fmt;
			config_img_fmt_mstream(ctx, req, cfg_fmt, vdev,
					       cfg_fmt->fmt.pix_mp.width,
					       cfg_fmt->fmt.pix_mp.height, scen);
		} else {
			dev_info(cam->dev,
				"%s:%s:seq:%d get vfmt(%d) failed\n",
				__func__, req->req.debug_str,
				req_stream_data->frame_seq_no,
				MTK_RAW_MAIN_STREAM_OUT);
		}

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

		if (scen->scen.mstream.type == MTK_CAM_MSTREAM_NE_SE) {
			frame_param->raw_param.hardware_scenario =
				is_m2m ? MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER :
			MTKCAM_IPI_HW_PATH_MSTREAM;

			dev_dbg(cam->dev,
				"%s: mstream (m2m %d) ne_se ne imgo:0x%x se rawi:0x%x\n",
				__func__, is_m2m, out_fmt->buf[0][0].iova,
				in_fmt->buf[0].iova);
		} else {
			//Dujac todo
			frame_param->raw_param.hardware_scenario =
				is_m2m ? MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER :
			MTKCAM_IPI_HW_PATH_MSTREAM;

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
							      s_data->feature.scen, "s_fmt_req");
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

static void mtk_cam_req_set_sv_vfmt(struct mtk_cam_device *cam,
				 struct mtk_camsv_pipeline *sv_pipeline,
				 struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_video_device *node;
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *s_data_ctx;
	struct mtk_cam_ctx *ctx;
	struct v4l2_format *f;
	int i;
	struct mtk_cam_scen scen;

	mtk_cam_scen_init(&scen);

	ctx = mtk_cam_s_data_get_ctx(s_data);
	req = mtk_cam_s_data_get_req(s_data);
	if (ctx) {
		s_data_ctx = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		mtk_cam_s_data_get_scen(&scen, s_data_ctx);
	}

	/* force update format to every video device before re-streamon */
	for (i = MTK_CAMSV_SOURCE_BEGIN; i < MTK_CAMSV_PIPELINE_PADS_NUM; i++) {
		node = &sv_pipeline->vdev_nodes[i - MTK_CAMSV_SOURCE_BEGIN];
		f = mtk_cam_s_data_get_vfmt(s_data, node->desc.id);
		if (!f) {
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):%s: can't find the vfmt field to save\n",
				 __func__, req->req.debug_str,
				 node->uid.pipe_id, node->desc.name);
		} else {
			if (s_data->vdev_fmt_update & (1 << node->desc.id)) {
				mtk_cam_video_set_fmt(node, f, &scen, "s_fmt_req_sv");
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
	}
}

static int mtk_cam_calc_pending_res(struct mtk_cam_device *cam,
				    struct mtk_cam_resource_config *res_cfg,
				    struct mtk_cam_resource_v2 *res_user,
				    struct v4l2_mbus_framefmt *sink_fmt,
				    int pipe_id)
{
	s64 prate = 0;
	int width, height;

	res_cfg->bin_limit = res_user->raw_res.bin; /* 1: force bin on */
	res_cfg->frz_limit = 0;
	if (res_user->raw_res.raws_must) {
		res_cfg->hwn_limit_max = mtk_cam_res_get_raw_num(res_user->raw_res.raws_must);
		res_cfg->hwn_limit_min = res_cfg->hwn_limit_max;
	} else {
		res_cfg->hwn_limit_max = 2;
		res_cfg->hwn_limit_min = 1;
	}

	res_cfg->hblank = res_user->sensor_res.hblank;
	res_cfg->vblank = res_user->sensor_res.vblank;
	res_cfg->sensor_pixel_rate = res_user->sensor_res.pixel_rate;
	res_cfg->interval = res_user->sensor_res.interval;
	res_cfg->res_plan = RESOURCE_STRATEGY_QRP;
	res_cfg->scen = res_user->raw_res.scen;
	/* res_cfg->hw_mode can't be changed during streaming in ISP 7.1 */

	if (res_user->sensor_res.no_bufferd_prate_calc)
		prate = res_user->sensor_res.pixel_rate;
	else
		prate = mtk_cam_seninf_calc_pixelrate
					(cam->dev, res_user->sensor_res.width,
					 res_user->sensor_res.height,
						 res_user->sensor_res.hblank,
						 res_user->sensor_res.vblank,
						 res_user->sensor_res.interval.denominator,
						 res_user->sensor_res.interval.numerator,
						 res_user->sensor_res.pixel_rate);
	/*worst case throughput prepare for stagger dynamic switch exposure num*/
	if (mtk_cam_scen_is_sensor_stagger(&res_cfg->scen)) {
		if (mtk_cam_scen_is_stagger_2_exp(&res_cfg->scen)) {
			dev_info(cam->dev,
				 "%s:pipe(%d): worst case stagger 2exp prate (%d):%lld->%lld\n",
				 __func__, pipe_id, res_cfg->scen.scen.normal.exp_num,
				 prate, prate * 2);
			prate = 2 * prate;
		} else if (mtk_cam_scen_is_stagger_3_exp(&res_cfg->scen)) {
			dev_info(cam->dev,
				 "%s:pipe(%d): worst case stagger 3exp prate (%d):%lld->%lld\n",
				 __func__, pipe_id, res_cfg->scen.scen.normal.exp_num,
				 prate, prate * 3);
			prate = 3 * prate;
		}
	}

	mtk_raw_resource_calc(cam, res_cfg, prate, res_cfg->res_plan, res_user->sensor_res.width,
			      res_user->sensor_res.height, &width, &height);

	if (res_user->raw_res.bin && !res_cfg->bin_enable) {
		dev_info(cam->dev,
			 "%s:pipe(%d): res calc failed on fource bin: user(%d)/bin_enable(%d)\n",
			 __func__, pipe_id, res_user->raw_res.bin,
			 res_cfg->bin_enable);
		return -EINVAL;
	}

	if (res_cfg->raw_num_used > res_cfg->hwn_limit_max ||
	    res_cfg->raw_num_used < res_cfg->hwn_limit_min) {
		dev_info(cam->dev,
			 "%s:pipe(%d): res calc failed on raw used: user(%d/%d)/raw_num_used(%d)\n",
			 __func__, pipe_id, res_cfg->hwn_limit_max,
			 res_cfg->hwn_limit_min, res_cfg->raw_num_used);
		return -EINVAL;
	}

	if (res_cfg->bin_limit == BIN_AUTO)
		res_user->raw_res.bin = res_cfg->bin_enable;
	else
		res_user->raw_res.bin = res_cfg->bin_limit;

	dev_info(cam->dev,
		 "%s:pipe(%d): res calc result: bin(%d)\n",
		 __func__, pipe_id, res_user->raw_res.bin);

	return 0;
}

static int mtk_cam_req_set_fmt(struct mtk_cam_device *cam,
			       struct mtk_cam_request *req)
{
	int pipe_id;
	int pad;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_request_stream_data *stream_data;
	struct mtk_cam_request_stream_data *ctx_stream_data;
	struct mtk_cam_req_raw_pipe_data *raw_pipe_data;
	struct v4l2_subdev *sd;
	struct mtk_raw_pipeline *raw_pipeline;
	struct mtk_camsv_pipeline *sv_pipeline;
	struct v4l2_mbus_framefmt *sink_fmt;
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
							 &stream_data->pad_fmt[pad],
							 stream_data->feature.scen);
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
							 &stream_data->pad_fmt[pad],
							 stream_data->feature.scen);
					} else {
						stream_data->pad_fmt[pad].format =
							raw_pipeline->cfg[pad].mbus_fmt;
					}
				}

				raw_pipe_data =
					mtk_cam_s_data_get_raw_pipe_data(stream_data);
				if (raw_pipe_data) {
					if (stream_data->flags & MTK_CAM_REQ_S_DATA_FLAG_RES_CALC) {
						sink_fmt =
							&stream_data->pad_fmt[MTK_RAW_SINK].format;
						mtk_cam_calc_pending_res(cam,
									 &raw_pipeline->res_config,
									 &raw_pipe_data->res,
									 sink_fmt,
									 pipe_id);
					}
					raw_pipe_data->res_config = raw_pipeline->res_config;

					if (req->ctx_link_update & (1 << pipe_id))
						raw_pipe_data->res_config.opp_idx =
							cam->camsys_ctrl.dvfs_info.clklv_num - 1;
				} else {
					dev_info(cam->dev, "%s:no raw_pipe_data found!\n",
						 __func__);
				}

			} else if (is_camsv_subdev(pipe_id)) {
				stream_data = mtk_cam_req_get_s_data(req, pipe_id, 0);
				sv_pipeline =
					&cam->sv.pipelines[pipe_id - MTKCAM_SUBDEV_CAMSV_START];
				sd = &sv_pipeline->subdev;
				ctx = mtk_cam_find_ctx(cam, &sd->entity);
				if (ctx) {
					ctx_stream_data =
						mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
				} else {
					ctx_stream_data = NULL;
					dev_info(cam->dev,
						 "%s:sv find ctx failed\n",
						 __func__);
				}

				mtk_cam_req_set_sv_vfmt(cam, sv_pipeline, stream_data);
				for (pad = MTK_CAMSV_SINK_BEGIN;
					 pad < MTK_CAMSV_PIPELINE_PADS_NUM; pad++)
					if (stream_data->pad_fmt_update & (1 << pad)
					    && ctx_stream_data) {
						ctx_stream_data->flags |=
							(ctx_stream_data->frame_seq_no > 1) ?
							MTK_CAM_REQ_S_DATA_FLAG_SINK_FMT_UPDATE : 0;
						mtk_camsv_call_pending_set_fmt
							(sd,
							 &stream_data->pad_fmt[pad]);
					}
			} else if (is_mraw_subdev(pipe_id)) {
				stream_data = mtk_cam_req_get_s_data(req, pipe_id, 0);
				sd = &cam->mraw.pipelines[
					pipe_id - MTKCAM_SUBDEV_MRAW_START].subdev;
				ctx = mtk_cam_find_ctx(cam, &sd->entity);
				if (ctx) {
					ctx_stream_data =
						mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
				} else {
					ctx_stream_data = NULL;
					dev_info(cam->dev,
						 "%s:mraw find ctx failed\n",
						 __func__);
				}

				if (stream_data->pad_fmt_update & (1 << MTK_MRAW_SINK)
				    && ctx_stream_data) {
					ctx_stream_data->flags |=
						(ctx_stream_data->frame_seq_no > 1) ?
						MTK_CAM_REQ_S_DATA_FLAG_SINK_FMT_UPDATE : 0;
					mtk_mraw_call_pending_set_fmt
						(sd,
						 &stream_data->pad_fmt[MTK_MRAW_SINK]);
				}
			}
		}
	}

	return 0;
}

int mtk_cam_ctx_img_working_buf_pool_init(struct mtk_cam_ctx *ctx, int buf_require,
					  int buf_size)
{
	bool img_mem_pool_created = false;
	int ret = 0;

	/* User may pre-alloccate the mem for the pool */
	if (ctx->pipe &&
	    ctx->pipe->pre_alloc_mem.num > 0 &&
	    ctx->pipe->pre_alloc_mem.bufs[0].fd >= 0) {
		if (mtk_cam_user_img_working_buf_pool_init(ctx, buf_require, buf_size) == 0)
			img_mem_pool_created = true;
	}

	if (!img_mem_pool_created)
		ret = mtk_cam_internal_img_working_buf_pool_init(ctx,
								 buf_require,
								 buf_size);

	return ret;
}

static int mtk_cam_req_update_ctrl(struct mtk_raw_pipeline *raw_pipe,
				   struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_scen scen_pre;
	char *debug_str = mtk_cam_s_data_get_dbg_str(s_data);
	struct mtk_cam_request *req;
	struct mtk_cam_req_raw_pipe_data *raw_pipe_data;
	struct mtk_cam_device *cam;
	struct mtk_cam_ctx *ctx;
	int exp_no;
	int buf_size;
	int buf_require;
	int ret = 0;

	raw_pipe_data = mtk_cam_s_data_get_raw_pipe_data(s_data);
	req = mtk_cam_s_data_get_req(s_data);

	/* clear seamless switch mode */
	raw_pipe->sensor_mode_update = 0;
	raw_pipe->req_res_calc  = false;
	scen_pre = raw_pipe->user_res.raw_res.scen;
	mtk_cam_req_ctrl_setup(raw_pipe, req);

	/* for stagger 1exp config */
	if (mtk_cam_scen_is_stagger_types(&scen_pre) &&
		s_data->frame_seq_no == 1) {
		scen_pre.scen.normal.exp_num = scen_pre.scen.normal.max_exp_num;
	}

	/* use raw_res.feature as raw_fut_pre (feature setup before re-streamon)*/
	if (req->ctx_link_update & (1 << raw_pipe->id)) {
		scen_pre = raw_pipe->user_res.raw_res.scen;

		/* create the vhdr internal buf pool if needed */
		exp_no = mtk_cam_scen_get_max_exp_num(&raw_pipe->user_res.raw_res.scen);
		cam = dev_get_drvdata(raw_pipe->raw->cam_dev);
		ctx = &cam->ctxs[raw_pipe->id];

		dev_info(raw_pipe->subdev.v4l2_dev->dev,
			 "%s:%s:%s:ctx(%d)linkupdate: prev_scen(%s), res scen(%s), wimg_buf_size(%d)\n",
			 __func__, raw_pipe->subdev.name, debug_str, ctx->stream_id,
			 scen_pre.dbg_str,
			 raw_pipe->user_res.raw_res.scen.dbg_str,
			 ctx->img_buf_pool.working_img_buf_size);

		if (exp_no > 1 &&
		    ctx->img_buf_pool.working_img_buf_size <= 0) {
			buf_size = ctx->pipe->vdev_nodes
				[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM].
				active_fmt.fmt.pix_mp.plane_fmt[0].sizeimage;

			buf_require =
				mtk_cam_get_internl_buf_num(ctx->pipe->dynamic_exposure_num_max,
							    &raw_pipe->user_res.raw_res.scen,
							    ctx->pipe->hw_mode_pending);

			if (buf_require)
				ret = mtk_cam_ctx_img_working_buf_pool_init(ctx, buf_require,
									    buf_size);

			if (ret)
				dev_info(cam->dev, "%s: ctx(%d)failed to reserve DMA memory:%d\n",
					 __func__, ctx->stream_id, ret);
		}
	}

	if (raw_pipe_data) {
		raw_pipe_data->res = raw_pipe->user_res;
		s_data->feature.scen = &raw_pipe_data->res.raw_res.scen;
	} else {
		dev_info(raw_pipe->subdev.v4l2_dev->dev,
			"%s:%s:%s:get raw_pipe_data failed\n",
			__func__, raw_pipe->subdev.name, debug_str);
	}

	s_data->feature.switch_feature_type =
		mtk_cam_get_feature_switch(raw_pipe, &scen_pre);
	s_data->feature.prev_scen = scen_pre;
	atomic_set(&s_data->first_setting_check, 0);
	if (s_data->feature.switch_feature_type) {
		s_data->feature.switch_prev_frame_done = 0;
		s_data->feature.switch_curr_setting_done = 0;
		s_data->feature.switch_done = 0;
	}

	dev_dbg(raw_pipe->subdev.v4l2_dev->dev,
		"%s:%s:%s:raw_scen(%s), prev_scen(%s), switch_feature_type(0x%0x), sensor_mode_update(%d), res_calc(%d)\n",
		__func__, raw_pipe->subdev.name, debug_str,
		s_data->feature.scen->dbg_str,
		s_data->feature.prev_scen.dbg_str,
		s_data->feature.switch_feature_type,
		raw_pipe->sensor_mode_update,
		raw_pipe->req_res_calc);

	/* change sensor mode for non-stagger seamless switch scenarios */
	if (raw_pipe->sensor_mode_update &&
	    s_data->feature.switch_feature_type == EXPOSURE_CHANGE_NONE)
		s_data->flags |= MTK_CAM_REQ_S_DATA_FLAG_SENSOR_MODE_UPDATE_T1;

	if (raw_pipe->req_res_calc)
		s_data->flags |= MTK_CAM_REQ_S_DATA_FLAG_RES_CALC;

	mtk_cam_tg_flash_req_update(raw_pipe, s_data);

	return 0;
}

static void mtk_cam_update_s_data_exp(struct mtk_cam_ctx *ctx,
					struct mtk_cam_request *req,
					struct mtk_cam_scen *scen,
					struct mtk_cam_mstream_exposure *exp)
{
	struct mtk_cam_request_stream_data *req_stream_data_1st =
		mtk_cam_req_get_s_data(req, ctx->stream_id, 1);
	struct mtk_cam_request_stream_data *req_stream_data_2nd =
		mtk_cam_req_get_s_data(req, ctx->stream_id, 0);

	if (!exp->valid)
		return;

	if (scen->scen.mstream.type == MTK_CAM_MSTREAM_NE_SE) {
		req_stream_data_1st->mtk_cam_exposure = exp->exposure[0];
		req_stream_data_2nd->mtk_cam_exposure = exp->exposure[1];
	} else {
		req_stream_data_2nd->mtk_cam_exposure = exp->exposure[0];
		req_stream_data_1st->mtk_cam_exposure = exp->exposure[1];
	}
	req_stream_data_1st->req_id = exp->req_id;
	req_stream_data_2nd->req_id = exp->req_id;

	exp->valid = 0;

	dev_dbg(ctx->cam->dev,
		"update mstream type(%d) exposure 1st:%d 2nd:%d gain 1st:%d 2nd:%d\n",
		scen->scen.mstream.type,
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
			} else if (is_4_plane_rgb(pixelformat)) {
				u8 rgb_4p_size[4];

				switch (info->pixel_id) {
				case 0:
					rgb_4p_size[0] = 2;
					rgb_4p_size[1] = 3;
					rgb_4p_size[2] = 0;
					rgb_4p_size[3] = 1;
					break;
				case 1:
					rgb_4p_size[0] = 3;
					rgb_4p_size[1] = 2;
					rgb_4p_size[2] = 1;
					rgb_4p_size[3] = 0;
					break;
				case 2:
					rgb_4p_size[0] = 0;
					rgb_4p_size[1] = 1;
					rgb_4p_size[2] = 2;
					rgb_4p_size[3] = 3;
					break;
				case 3:
					rgb_4p_size[0] = 1;
					rgb_4p_size[1] = 0;
					rgb_4p_size[2] = 3;
					rgb_4p_size[3] = 2;
					break;
				default:
					rgb_4p_size[0] = 0;
					rgb_4p_size[1] = 1;
					rgb_4p_size[2] = 2;
					rgb_4p_size[3] = 3;
					break;
				}

				for (i = 0; i < info->comp_planes; i++) {
					img_out->fmt.stride[i] = stride;
					img_out->buf[0][i].size = img_out->fmt.stride[i] *
						height / 2;
					img_out->buf[0][i].iova = daddr +
						img_out->buf[0][i].size
						* (dma_addr_t)rgb_4p_size[i];
					pr_debug("plane:%d stride:%d plane_size:%d addr:0x%x\n",
						i, img_out->fmt.stride[i], img_out->buf[0][i].size,
						img_out->buf[0][i].iova);
				}
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
				unsigned int pipe_id, struct mtk_cam_scen *scen,
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
		pr_debug("mstream buffer plane over 2\n");
		return;
	}

	mstream_frame_param->raw_param.exposure_num = 1;
	frame_param->raw_param.exposure_num = 2;

	if (mtk_cam_scen_is_mstream_m2m(scen) &&
	    scen->scen.mstream.type != MTK_CAM_MSTREAM_1_EXPOSURE) {
		/**
		 * MSTREAM_SE_NE M2M orientation
		 * First exposure:
		 * Input = SE(RAWI2 node buffer through RAWI2),
		 * Output = SE(IMGO node buffer)
		 * hw_scenario -> MTKCAM_IPI_HW_PATH_OFFLINE
		 *
		 * Second exposure:
		 * Intput = NE(RAWI2 buffer through RAWI6) + SE(IMGO buffer through RAWI2),
		 * Output = NE(IMGO node buffer)
		 * hw_scenario -> MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER
		 */

		mstream_frame_param->raw_param.hardware_scenario =
			MTKCAM_IPI_HW_PATH_OFFLINE;
		frame_param->raw_param.hardware_scenario =
			MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER;

		if (scen->scen.mstream.type == MTK_CAM_MSTREAM_NE_SE) {
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
					int in_node = MTKCAM_IPI_RAW_RAWI_5;

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
					int in_node = MTKCAM_IPI_RAW_RAWI_5;

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
		//Dujac todo
		if (scen->scen.mstream.type == MTK_CAM_MSTREAM_NE_SE)
			frame_param->raw_param.hardware_scenario =
			MTKCAM_IPI_HW_PATH_MSTREAM;
		else
			frame_param->raw_param.hardware_scenario =
			MTKCAM_IPI_HW_PATH_MSTREAM;

		// imgo mstream buffer layout is fixed plane[0]=NE, plane[1]=SE
		if (scen->scen.mstream.type == MTK_CAM_MSTREAM_NE_SE) {
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
			   struct mtk_cam_scen *scen, const struct v4l2_format *f)

{
	struct mtk_cam_buffer *buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	unsigned int desc_id;
	int i;
	struct mtk_cam_request_stream_data *req_stream_data =
		mtk_cam_req_get_s_data(req, pipe_id, 0);
	struct mtkcam_ipi_frame_param *frame_param =
		&req_stream_data->frame_params;
	int exp_num = mtk_cam_scen_get_exp_num(scen);
	int sizeimage = f->fmt.pix_mp.plane_fmt[0].sizeimage;
	int data_offset;
	bool is_rgbw = mtk_cam_scen_is_rgbw_enabled(scen);
	int num_of_plane = is_rgbw ? 2 : 1;

	if (scenario != STAGGER_M2M)
		desc_id = node->desc.id - MTK_RAW_SOURCE_BEGIN;
	else
		desc_id = node->desc.id - MTK_RAW_RAWI_2_IN;

	for (i = 0 ; i < exp_num; i++) {
		data_offset = i * sizeimage * num_of_plane;
		if (mtk_cam_scen_is_mstream_2exp_types(scen)) {
			mtk_cam_mstream_buf_update(req, pipe_id, scen,
						desc_id, i, vb, f);
		} else if (exp_num == 3) {
			// TODO: RGBW 3 DOL
			if (i == 0) { /* camsv1*/
				int in_node = MTKCAM_IPI_RAW_RAWI_2;

				frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
					.iova = buf->daddr + data_offset;
				frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
					.size = sizeimage;
			} else if (i == 1) { /*camsv2*/
				int in_node = MTKCAM_IPI_RAW_RAWI_3;

				frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
				.iova = buf->daddr + data_offset;
				frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
					.size = sizeimage;
			} else if (i == 2) { /*raw*/
				if (scenario == STAGGER_M2M) {
					int in_node = MTKCAM_IPI_RAW_RAWI_5;

					frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
						.iova = buf->daddr + data_offset;
					frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
						.size = sizeimage;
				} else {
#if PURE_RAW_WITH_SV
					if (mtk_cam_s_data_is_pure_raw_with_sv(req_stream_data))
						frame_param->camsv_param[0][SVTAG_2]
							.camsv_img_outputs[0].buf[0][0].iova =
							buf->daddr + data_offset;
					else
						frame_param->img_outs[desc_id].buf[0][0].iova =
							buf->daddr + data_offset;
#else
					frame_param->img_outs[desc_id].buf[0][0].iova =
						buf->daddr + data_offset;
#endif
				}
			}
		} else if (exp_num == 2) {
			if (i == 0) { /* camsv1*/
				int in_node = MTKCAM_IPI_RAW_RAWI_2;

				frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
				.iova = buf->daddr + data_offset;
				frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
					.size = sizeimage;

				if (is_rgbw) {
					frame_param->img_ins[MTK_CAM_IMGO_W_IMG_IN_R2_IDX].buf[0]
					.iova = buf->daddr + data_offset + sizeimage;
					frame_param->img_ins[MTK_CAM_IMGO_W_IMG_IN_R2_IDX].buf[0]
					.size = sizeimage;
				}
			} else if (i == 1) { /*raw*/
				if (scenario == STAGGER_M2M) {
					int in_node = MTKCAM_IPI_RAW_RAWI_5;

					frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
						.iova = buf->daddr + data_offset;
					frame_param->img_ins[in_node - MTKCAM_IPI_RAW_RAWI_2].buf[0]
						.size = sizeimage;
					if (is_rgbw) {
						frame_param->img_ins[
							MTK_CAM_IMGO_W_IMG_IN_R5_IDX].buf[0].iova =
							buf->daddr + data_offset + sizeimage;
						frame_param->img_ins[
							MTK_CAM_IMGO_W_IMG_IN_R5_IDX].buf[0].size =
							sizeimage;
					}
				} else {
#if PURE_RAW_WITH_SV
					if (mtk_cam_s_data_is_pure_raw_with_sv(req_stream_data)) {
						frame_param->camsv_param[0][SVTAG_2]
							.camsv_img_outputs[0].buf[0][0].iova =
							buf->daddr + data_offset;

						frame_param->camsv_param[0][SVTAG_3]
							.camsv_img_outputs[0].buf[0][0].iova =
							(is_rgbw) ?
							buf->daddr + data_offset + sizeimage : 0;
					} else {
						frame_param->img_outs[desc_id].buf[0][0].iova =
							buf->daddr + data_offset;

						frame_param->img_outs[
							MTK_CAM_IMGO_W_IMG_OUT_IDX].buf[0][0].iova =
							(is_rgbw) ?
							buf->daddr + data_offset + sizeimage : 0;
					}
#else
					frame_param->img_outs[desc_id].buf[0][0].iova =
						buf->daddr + data_offset;

					if (is_rgbw)
						frame_param->img_outs[
							MTK_CAM_IMGO_W_IMG_OUT_IDX].buf[0][0].iova =
							buf->daddr + data_offset + sizeimage;
#endif
				}
			}
		}
	}

	return 0;
}

/* Update raw_param.imgo_path_sel */
static int mtk_cam_config_raw_path(struct mtk_cam_request_stream_data *s_data,
				   struct mtk_cam_buffer *buf)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	struct mtk_cam_video_device *node;
	struct mtk_cam_scen scen;
	struct mtk_raw_pipeline *raw_pipline;
	struct mtkcam_ipi_frame_param *frame_param;
	struct vb2_buffer *vb;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	frame_param = &s_data->frame_params;
	vb = &buf->vbb.vb2_buf;
	node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);
	raw_pipline = mtk_cam_dev_get_raw_pipeline(cam, node->uid.pipe_id);
	mtk_cam_s_data_get_scen(&scen, s_data);

	if (!raw_pipline) {
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_UNPROCESSED;
		dev_info(cam->dev,
			"%s: node:%d fd:%d idx:%d: get raw_pipline failed\n",
			__func__, node->desc.id, buf->vbb.request_fd,
			buf->vbb.vb2_buf.index);
		goto EXIT;
	}

	if (raw_pipline->res_config.raw_path == V4L2_MTK_CAM_RAW_PATH_SELECT_BPC)
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_AFTER_BPC;
	else if (raw_pipline->res_config.raw_path == V4L2_MTK_CAM_RAW_PATH_SELECT_FUS)
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_AFTER_FUS;
	else if (raw_pipline->res_config.raw_path == V4L2_MTK_CAM_RAW_PATH_SELECT_DGN)
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_AFTER_DGN;
	else if (raw_pipline->res_config.raw_path == V4L2_MTK_CAM_RAW_PATH_SELECT_LSC)
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_AFTER_LSC;
	else if (raw_pipline->res_config.raw_path == V4L2_MTK_CAM_RAW_PATH_SELECT_HLR)
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_AFTER_HLR;
	else if (raw_pipline->res_config.raw_path == V4L2_MTK_CAM_RAW_PATH_SELECT_LTM)
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_AFTER_LTM;
	else
		/* un-processed raw frame */
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_UNPROCESSED;

	dev_dbg(cam->dev, "%s: node:%d fd:%d idx:%d raw_path(%d) ipi imgo_path_sel(%d)\n",
		__func__, node->desc.id, buf->vbb.request_fd, buf->vbb.vb2_buf.index,
		raw_pipline->res_config.raw_path, frame_param->raw_param.imgo_path_sel);
EXIT:
#if PURE_RAW_WITH_SV
	if (mtk_cam_ctx_support_pure_raw_with_sv(ctx) &&
	    frame_param->raw_param.imgo_path_sel == MTKCAM_IPI_IMGO_UNPROCESSED &&
	    !mtk_cam_scen_is_mstream(&scen) &&
	    !mtk_cam_scen_is_mstream_m2m(&scen) &&
	    !mtk_cam_scen_is_subsample(&scen) &&
	    !mtk_cam_scen_is_rgbw_enabled(&scen) &&
	    mtk_cam_scen_is_sensor_normal(&scen) &&
	    !mtk_cam_hw_mode_is_dc(ctx->pipe->hw_mode_pending)) {
		dev_dbg(cam->dev,
			"%s: seq(%d) is pure raw with camsv(%d)\n",
			__func__, s_data->frame_seq_no,
			ctx->pure_raw_sv_tag_idx);
		return ctx->pure_raw_sv_tag_idx;
	}
#endif
	return -1;
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
	struct mtk_cam_scen *scen;
	unsigned int pixelformat, num_planes;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	scen = s_data->feature.scen;
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
#if PURE_RAW_WITH_SV
	if (mtk_cam_s_data_is_pure_raw_with_sv(s_data)) {
		frame_param->camsv_param[0][SVTAG_2]
			.camsv_img_outputs[0].buf[0][0].iova = buf->daddr;
		frame_param->camsv_param[0][SVTAG_2]
			.camsv_img_outputs[0].buf[0][0].ccd_fd = vb->planes[0].m.fd;
	} else {
		/* raw use iova to check whether composes imgo branch */
		img_out->buf[0][0].iova = buf->daddr;
		img_out->buf[0][0].ccd_fd = vb->planes[0].m.fd;
		if (is_raw_ufo(pixelformat))
			mtk_cam_fill_img_buf(img_out, cfg_fmt, buf->daddr);
	}
#else
	img_out->buf[0][0].iova = buf->daddr;
	img_out->buf[0][0].ccd_fd = vb->planes[0].m.fd;
	if (is_raw_ufo(pixelformat))
		mtk_cam_fill_img_buf(img_out, cfg_fmt, buf->daddr);
#endif

	if (mtk_cam_scen_is_hdr(scen) && !mtk_cam_scen_is_odt(scen)
	    && mtk_cam_scen_get_exp_num(scen) > 1) {
		if (mtk_cam_scen_is_mstream_2exp_types(scen))
			scenario = MSTREAM;
		else
			scenario = STAGGER_ON_THE_FLY;

		mtk_cam_hdr_buf_update(vb, scenario, req, node->uid.pipe_id,
				       scen, cfg_fmt);
	} else if (mtk_cam_scen_is_mstream_m2m(scen) &&
				scen->scen.mstream.type != MTK_CAM_MSTREAM_1_EXPOSURE) {
		mtk_cam_hdr_buf_update(vb, MSTREAM_M2M, req,
				       node->uid.pipe_id,
				       scen, cfg_fmt);
	} else if (mtk_cam_scen_is_rgbw_enabled(scen)) {
		// RGBW 1 exp
		struct mtkcam_ipi_img_output *img_out_w =
			&frame_param->img_outs[MTK_CAM_IMGO_W_IMG_OUT_IDX];

		img_out_w->buf[0][0].iova =
				buf->daddr + cfg_fmt->fmt.pix_mp.plane_fmt[0].sizeimage;
		img_out_w->buf[0][0].ccd_fd = vb->planes[0].m.fd;

		if (is_raw_ufo(pixelformat))
			mtk_cam_fill_img_buf(img_out_w, cfg_fmt,
				img_out_w->buf[0][0].iova);

		dev_dbg(cam->dev,
			 "%s:imgo iova 0x%x imgo_w iova 0x%x size %d\n",
			 __func__, img_out->buf[0][0].iova, img_out_w->buf[0][0].iova,
			 cfg_fmt->fmt.pix_mp.plane_fmt[0].sizeimage);
	}

	if (mtk_cam_scen_is_subsample(scen)) {
		num_planes =
			(scen->scen.smvr.subsample_num < MAX_SUBSAMPLE_PLANE_NUM) ?
			 scen->scen.smvr.subsample_num : MAX_SUBSAMPLE_PLANE_NUM;

		for (i = 0 ; i < num_planes; i++) {
			vb->planes[i].data_offset =
				i * cfg_fmt->fmt.pix_mp.plane_fmt[0].sizeimage;
			img_out->buf[i][0].iova =
				buf->daddr + vb->planes[i].data_offset;
		}
		/* FOR 16 subsample ratios - FIXME */
		if (scen->scen.smvr.subsample_num  == 16)
			for (i = MAX_SUBSAMPLE_PLANE_NUM; i < 16; i++)
				img_out->buf[i][0].iova =
				buf->daddr + i * cfg_fmt->fmt.pix_mp.plane_fmt[0].sizeimage;
		/* FOR 32 subsample ratios - FIXME */
		if (scen->scen.smvr.subsample_num  == 32)
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
	struct mtk_cam_scen *scen;
	int comp_planes, plane;
	unsigned int offset, num_planes;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	scen = s_data->feature.scen;
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
	if (mtk_cam_scen_is_subsample(scen)) {
		comp_planes = 1;
		if (is_mtk_format(cfg_fmt->fmt.pix_mp.pixelformat)) {
			const struct mtk_format_info *mtk_info =
				mtk_format_info(cfg_fmt->fmt.pix_mp.pixelformat);
			if (mtk_info)
				comp_planes = mtk_info->comp_planes;
		} else {
			const struct v4l2_format_info *v4l2_info =
				v4l2_format_info(cfg_fmt->fmt.pix_mp.pixelformat);
			if (v4l2_info)
				comp_planes = v4l2_info->comp_planes;
		}

		num_planes =
			(scen->scen.smvr.subsample_num < MAX_SUBSAMPLE_PLANE_NUM) ?
			 scen->scen.smvr.subsample_num : MAX_SUBSAMPLE_PLANE_NUM;
		for (i = 0 ; i < num_planes; i++) {
			offset = i * cfg_fmt->fmt.pix_mp.plane_fmt[0].sizeimage;
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
		if (scen->scen.smvr.subsample_num == 16) {
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
		if (scen->scen.smvr.subsample_num == 32) {
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
	struct mtk_cam_scen *scen;
	struct v4l2_mbus_framefmt *pfmt;
	int sd_width, sd_height, ret;
	const struct v4l2_format *cfg_fmt;
	struct v4l2_selection *cfg_selection;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	scen = s_data->feature.scen;
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

	if (cfg_selection) {
		img_out->crop.p.x = cfg_selection->r.left;
		img_out->crop.p.y = cfg_selection->r.top;
		img_out->crop.s.w = cfg_selection->r.width;
		img_out->crop.s.h = cfg_selection->r.height;
	} else {
		dev_info(cam->dev,
			 "%s:%s:pipe(%d):%s: get selection failed\n",
			 __func__, req->req.debug_str,
			 node->uid.pipe_id, node->desc.name);
	}

	ret = config_img_fmt(cam, node, cfg_fmt, img_out, sd_width, sd_height);
	if (ret)
		return ret;

#if PURE_RAW_WITH_SV
		if (node->desc.dma_port == MTKCAM_IPI_RAW_IMGO &&
			mtk_cam_s_data_is_pure_raw_with_sv(s_data)) {
			/* update camsv's frame parameter */
			mtk_cam_fill_sv_frame_param(ctx, frame_param, SVTAG_2, img_out->fmt,
				frame_param->camsv_param[0][SVTAG_2]
				.camsv_img_outputs[0].buf[0][0].iova);
		}
#endif

	if ((node->desc.dma_port == MTKCAM_IPI_RAW_IMGO) &&
		(mtk_cam_scen_is_rgbw_enabled(scen))) {
		struct mtkcam_ipi_img_output *img_out_w =
			&frame_param->img_outs[MTK_CAM_IMGO_W_IMG_OUT_IDX];

		img_out_w->uid.pipe_id = node->uid.pipe_id;
		img_out_w->uid.id = MTKCAM_IPI_RAW_IMGO_W;

		if (cfg_selection) {
			img_out_w->crop.p.x = cfg_selection->r.left;
			img_out_w->crop.p.y = cfg_selection->r.top;
			img_out_w->crop.s.w = cfg_selection->r.width;
			img_out_w->crop.s.h = cfg_selection->r.height;
		} else {
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):%s: get selection failed\n",
				 __func__, req->req.debug_str,
				 node->uid.pipe_id, node->desc.name);
		}

		config_img_fmt(cam, node, cfg_fmt, img_out_w, sd_width, sd_height);
	}

	if (mtk_cam_scen_is_sensor_stagger(scen) ||
		mtk_cam_hw_mode_is_dc(ctx->pipe->hw_mode_pending))
		config_img_in_fmt_stagger(cam, s_data, node,
				cfg_fmt, scen, ctx->pipe->hw_mode_pending);

	if (mtk_cam_scen_is_time_shared(scen))
		config_img_in_fmt_time_shared(cam, s_data, node, cfg_fmt);

	if ((mtk_cam_scen_is_mstream_2exp_types(scen))
	    && node->desc.dma_port == MTKCAM_IPI_RAW_IMGO) {
		ret = config_img_fmt_mstream(ctx, req,
					     cfg_fmt, node,
					     sd_width, sd_height, scen);
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
	struct mtkcam_ipi_img_input *in_fmt, *in_fmt_w;
	struct vb2_buffer *vb;
	const struct v4l2_format *cfg_fmt;
	struct mtk_cam_scen *scen;
	int ret;
	bool is_rgbw, buf_updated = false;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	scen = s_data->feature.scen;
	req = mtk_cam_s_data_get_req(s_data);
	frame_param = &s_data->frame_params;
	vb = &buf->vbb.vb2_buf;
	node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);
	in_fmt = &frame_param->img_ins[node->desc.id - MTK_RAW_RAWI_2_IN];
	is_rgbw = mtk_cam_scen_is_rgbw_enabled(scen);

	cfg_fmt = mtk_cam_s_data_get_vfmt(s_data, node->desc.id);
	if (!cfg_fmt) {
		dev_info(cam->dev,
			 "%s:%s:pipe(%d):%s: can't find the vfmt field to save\n",
			 __func__, req->req.debug_str, node->uid.pipe_id, node->desc.name);
		return -EINVAL;
	}

	dev_dbg(cam->dev, "update odt scen(%s)\n", scen->dbg_str);
	if (mtk_cam_scen_is_odt(scen) && mtk_cam_scen_is_hdr(scen)) {
		/* ODT/ STAGGER/ 2exp or 3exp */
		if (mtk_cam_scen_is_stagger_m2m(scen) &&
		    scen->scen.normal.exp_num != 1) {
			mtk_cam_hdr_buf_update(vb, STAGGER_M2M,
					       req, node->uid.pipe_id,
					       scen, cfg_fmt);
			frame_param->raw_param.hardware_scenario =
				MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER;
			frame_param->raw_param.exposure_num =
				mtk_cam_scen_get_exp_num(scen);
			buf_updated = true;
		} else if (mtk_cam_scen_is_mstream_m2m(scen) &&
			   scen->scen.mstream.type != MTK_CAM_MSTREAM_1_EXPOSURE) {
			mtk_cam_hdr_buf_update(vb, MSTREAM_M2M,
					       req, node->uid.pipe_id,
					       scen, cfg_fmt);
			buf_updated = true;
		}
	}

	in_fmt_w = NULL;
	if (!buf_updated) {
		// 1 exp
		if (!is_rgbw) {
			in_fmt->buf[0].iova = buf->daddr;
			frame_param->raw_param.hardware_scenario =
				MTKCAM_IPI_HW_PATH_OFFLINE;
			in_fmt->uid.pipe_id = node->uid.pipe_id;
			in_fmt->uid.id = node->desc.dma_port;
		} else {
			in_fmt = &frame_param->img_ins[
				MTKCAM_IPI_RAW_RAWI_5 - MTKCAM_IPI_RAW_RAWI_2];
			in_fmt->buf[0].iova = buf->daddr;
			in_fmt->uid.pipe_id = node->uid.pipe_id;
			in_fmt->uid.id = MTKCAM_IPI_RAW_RAWI_5;

			in_fmt_w = &frame_param->img_ins[MTK_CAM_IMGO_W_IMG_IN_R5_IDX];
			in_fmt_w->buf[0].iova = buf->daddr +
				cfg_fmt->fmt.pix_mp.plane_fmt[0].sizeimage;
			in_fmt_w->uid.id = MTKCAM_IPI_RAW_RAWI_5_W;
			in_fmt_w->uid.pipe_id = node->uid.pipe_id;

			frame_param->raw_param.hardware_scenario =
				MTKCAM_IPI_HW_PATH_OFFLINE_RGBW;
		}
	} else {
		in_fmt->uid.pipe_id = node->uid.pipe_id;
		in_fmt->uid.id = node->desc.dma_port;
	}

	ret = config_img_in_fmt(cam, node, cfg_fmt, in_fmt) ||
		  (in_fmt_w && config_img_in_fmt(cam, node, cfg_fmt, in_fmt_w));
	if (ret)
		return ret;

	if (mtk_cam_scen_is_stagger_m2m(scen))
		config_img_in_fmt_stagger(cam, s_data, node, cfg_fmt, scen,
					  HW_MODE_M2M);

	if (mtk_cam_scen_is_mstream_m2m(scen) &&
	    scen->scen.mstream.type != MTK_CAM_MSTREAM_1_EXPOSURE)
		config_img_in_fmt_mstream(cam, s_data, node, cfg_fmt, scen);

	return 0;
}

static int
mtk_cam_config_raw_img_in_ipui(struct mtk_cam_request_stream_data *s_data,
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
	struct mtk_cam_scen *scen;
	int ret;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	scen = s_data->feature.scen;
	req = mtk_cam_s_data_get_req(s_data);
	frame_param = &s_data->frame_params;
	vb = &buf->vbb.vb2_buf;
	node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);
	in_fmt = &frame_param->img_ins[MTKCAM_IPI_RAW_IPUI - MTK_RAW_RAWI_2_IN];

	cfg_fmt = mtk_cam_s_data_get_vfmt(s_data, node->desc.id);
	if (!cfg_fmt) {
		dev_info(cam->dev,
			 "%s:%s:pipe(%d):%s: can't find the vfmt field to save\n",
			 __func__, req->req.debug_str, node->uid.pipe_id, node->desc.name);
		return -EINVAL;
	}

	in_fmt->buf[0].iova = buf->daddr;
	frame_param->raw_param.hardware_scenario =
		MTKCAM_IPI_HW_PATH_OFFLINE_ADL;

	if (s_data->apu_info.apu_path == APU_DC_RAW ||
	    s_data->apu_info.apu_path == APU_FRAME_MODE) {
		// TODO: dc mode, change buffer
		if (s_data->apu_info.vpu_i_point == AFTER_SEP_R1) {
			frame_param->adl_param.vpu_i_point =
				MTKCAM_IPI_ADL_AFTER_SEP_R1;
		} else if (s_data->apu_info.vpu_i_point == AFTER_BPC) {
			frame_param->adl_param.vpu_i_point =
				MTKCAM_IPI_ADL_AFTER_BPC;
		} else if (s_data->apu_info.vpu_i_point == AFTER_LTM) {
			frame_param->adl_param.vpu_i_point =
				MTKCAM_IPI_ADL_AFTER_LTM;
		} else {
			return -EINVAL;
		}

		if (s_data->apu_info.vpu_o_point == AFTER_SEP_R1) {
			frame_param->adl_param.vpu_o_point =
				MTKCAM_IPI_ADL_AFTER_SEP_R1;
		} else if (s_data->apu_info.vpu_o_point == AFTER_BPC) {
			frame_param->adl_param.vpu_o_point =
				MTKCAM_IPI_ADL_AFTER_BPC;
		} else if (s_data->apu_info.vpu_o_point == AFTER_LTM) {
			frame_param->adl_param.vpu_o_point =
				MTKCAM_IPI_ADL_AFTER_LTM;
		} else {
			return -EINVAL;
		}

		frame_param->adl_param.sysram_en =
			s_data->apu_info.sysram_en;
		frame_param->adl_param.block_y_size =
			s_data->apu_info.block_y_size;
	}

	if (s_data->apu_info.apu_path == APU_DC_RAW) {
		struct slbc_data slb;

		frame_param->raw_param.hardware_scenario =
			MTKCAM_IPI_HW_PATH_DC_ADL;

		if (!ctx->slb_addr) {
			slb.uid = UID_SH_P1;
			slb.type = TP_BUFFER;
			ret = slbc_request(&slb);

			if (ret < 0) {
				dev_info(cam->dev,
					"%s: allocate slb fail\n", __func__);
			} else {
				dev_info(cam->dev,
					"%s: slb buffer base(0x%x), size(%ld): %x",
					__func__, (uintptr_t)slb.paddr, slb.size);
				ctx->slb_addr = slb.paddr;
				ctx->slb_size = slb.size;
			}
		}

	} else if (s_data->apu_info.apu_path == APU_FRAME_MODE) {
		frame_param->raw_param.hardware_scenario =
			MTKCAM_IPI_HW_PATH_OFFLINE_ADL;
	} else {
		return -EINVAL;
	}

	in_fmt->uid.pipe_id = node->uid.pipe_id;
	in_fmt->uid.id = MTKCAM_IPI_RAW_IPUI;//node->desc.dma_port;
	ret = config_img_in_fmt(cam, node, cfg_fmt, in_fmt);
	if (ret)
		return ret;

	return 0;
}

static int mtk_cam_config_sv_img_out_imgo(struct mtk_cam_request_stream_data *s_data,
				      struct vb2_buffer *vb)
{
	struct mtk_cam_ctx *ctx = s_data->ctx;
	struct mtk_cam_request_stream_data *ctx_stream_data;
	struct mtk_cam_request_stream_data *ctx_stream_data_mstream;
	struct mtkcam_ipi_frame_param *frame_param;
	struct mtkcam_ipi_frame_param *frame_param_mstream;
	struct mtk_cam_video_device *node;
	struct mtk_cam_buffer *buf;
	struct mtkcam_ipi_pix_fmt fmt;
	unsigned int tag_idx, offset;
	__u64 iova;
	void *vaddr;
	struct dma_info info;

	buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	ctx_stream_data = mtk_cam_req_get_s_data(s_data->req, ctx->stream_id, 0);
	frame_param = &ctx_stream_data->frame_params;
	node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);

	if (mtk_cam_is_display_ic(ctx) &&
		node->desc.id == MTK_CAMSV_MAIN_STREAM_OUT) {
		tag_idx = SVTAG_0;
		fmt.format = mtk_cam_get_img_fmt(
			ctx->sv_dev->tag_info[tag_idx].img_fmt.fmt.pix_mp.pixelformat);
		fmt.s.w = ctx->sv_dev->tag_info[tag_idx].img_fmt.fmt.pix_mp.width;
		fmt.s.h = ctx->sv_dev->tag_info[tag_idx].img_fmt.fmt.pix_mp.height;
		fmt.stride[0] =
			ctx->sv_dev->tag_info[tag_idx].img_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
		offset = fmt.stride[0] * fmt.s.h;
		iova = buf->daddr;

		/* update camsv's frame parameter */
		mtk_cam_fill_sv_frame_param(ctx, frame_param, tag_idx, fmt, iova);

		tag_idx = SVTAG_1;
		fmt.format = mtk_cam_get_img_fmt(
			ctx->sv_dev->tag_info[tag_idx].img_fmt.fmt.pix_mp.pixelformat);
		fmt.s.w = ctx->sv_dev->tag_info[tag_idx].img_fmt.fmt.pix_mp.width;
		fmt.s.h = ctx->sv_dev->tag_info[tag_idx].img_fmt.fmt.pix_mp.height;
		fmt.stride[0] =
			ctx->sv_dev->tag_info[tag_idx].img_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
		iova = (((buf->daddr + offset + 15) >> 4) << 4);

		/* update camsv's frame parameter */
		mtk_cam_fill_sv_frame_param(ctx, frame_param, tag_idx, fmt, iova);
	} else if (mtk_cam_is_display_ic(ctx) &&
		node->desc.id == MTK_CAMSV_EXT_STREAM_OUT) {
		tag_idx = SVTAG_2;
		fmt.format = mtk_cam_get_img_fmt(
			ctx->sv_dev->tag_info[tag_idx].img_fmt.fmt.pix_mp.pixelformat);
		fmt.s.w = ctx->sv_dev->tag_info[tag_idx].img_fmt.fmt.pix_mp.width;
		fmt.s.h = ctx->sv_dev->tag_info[tag_idx].img_fmt.fmt.pix_mp.height;
		fmt.stride[0] =
			ctx->sv_dev->tag_info[tag_idx].img_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
		iova = buf->daddr;

		/* update camsv's frame parameter */
		mtk_cam_fill_sv_frame_param(ctx, frame_param, tag_idx, fmt, iova);
	} else if (mtk_cam_scen_is_rgbw_using_camsv(ctx_stream_data->feature.scen) &&
		node->desc.id == MTK_RAW_MAIN_STREAM_SV_1_OUT) {
		/* camsv todo: rgbw */
		tag_idx = SVTAG_2;
		fmt.format = mtk_cam_get_img_fmt(node->active_fmt.fmt.pix_mp.pixelformat);
		fmt.s.w = node->active_fmt.fmt.pix_mp.width;
		fmt.s.h = node->active_fmt.fmt.pix_mp.height;
		fmt.stride[0] = node->active_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
		iova = buf->daddr;

		/* update camsv's frame parameter */
		mtk_cam_fill_sv_frame_param(ctx, frame_param, tag_idx, fmt, iova);
	} else if (node->desc.id == MTK_RAW_MAIN_STREAM_SV_1_OUT ||
		node->desc.id == MTK_RAW_MAIN_STREAM_SV_2_OUT) {
		if (node->desc.id == MTK_RAW_MAIN_STREAM_SV_1_OUT)
			tag_idx = SVTAG_0;
		else
			tag_idx = SVTAG_1;
		fmt.format = mtk_cam_get_img_fmt(node->active_fmt.fmt.pix_mp.pixelformat);
		fmt.s.w = node->active_fmt.fmt.pix_mp.width;
		fmt.s.h = node->active_fmt.fmt.pix_mp.height;
		fmt.stride[0] = node->active_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
		iova = buf->daddr;

		/* update camsv's frame parameter */
		mtk_cam_fill_sv_frame_param(ctx, frame_param, tag_idx, fmt, iova);
	} else if (node->desc.id == MTK_RAW_META_SV_OUT_0) {
		tag_idx = SVTAG_3;
		fmt.format = mtk_cam_get_img_fmt(node->active_fmt.fmt.pix_mp.pixelformat);
		fmt.s.w = node->active_fmt.fmt.pix_mp.width;
		fmt.s.h = node->active_fmt.fmt.pix_mp.height;
		fmt.stride[0] = node->active_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
		iova = buf->daddr;

		/* update camsv's frame parameter */
		mtk_cam_fill_sv_frame_param(ctx, frame_param, tag_idx, fmt, iova);
	} else {
		tag_idx = mtk_cam_get_sv_tag_index(ctx, s_data->pipe_id);
		fmt.format = mtk_cam_get_img_fmt(node->active_fmt.fmt.pix_mp.pixelformat);
		fmt.s.w = node->active_fmt.fmt.pix_mp.width;
		fmt.s.h = node->active_fmt.fmt.pix_mp.height;
		fmt.stride[0] = node->active_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
		iova = ((((buf->daddr + GET_PLAT_V4L2(meta_sv_ext_size)) + 15) >> 4) << 4);

		/* update meta header */
		vaddr = vb2_plane_vaddr(vb, 0);
		info.width = fmt.s.w;
		info.height = fmt.s.h;
		info.stride = fmt.stride[0];
		CALL_PLAT_V4L2(
			set_sv_meta_stats_info, node->desc.dma_port, vaddr, &info);

		/* update camsv's frame parameter */
		mtk_cam_fill_sv_frame_param(ctx, frame_param, tag_idx, fmt, iova);

		/* update mstream's frame parameter */
		if (mtk_cam_scen_is_mstream_2exp_types(ctx_stream_data->feature.scen)) {
			ctx_stream_data_mstream =
				mtk_cam_req_get_s_data(s_data->req, ctx->stream_id, 1);
			frame_param_mstream = &ctx_stream_data_mstream->frame_params;
			/* update camsv's frame parameter for mstream */
			mtk_cam_fill_sv_frame_param(ctx, frame_param_mstream, tag_idx, fmt, iova);
		}
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
	struct mtk_cam_request_stream_data *req_stream_data;
	int i, ctx_cnt;
	struct mtk_cam_scen scen;
	int ret;
	unsigned long fps;

	dev_dbg(cam->dev, "update request:%s\n", req->req.debug_str);

	mtk_cam_scen_init(&scen);
	mtk_cam_req_set_fmt(cam, req);

	list_for_each_entry_safe(obj, obj_prev, &req->req.objects, list) {
		if (!vb2_request_object_is_buffer(obj))
			continue;

		vb = container_of(obj, struct vb2_buffer, req_obj);
		buf = mtk_cam_vb2_buf_to_dev_buf(vb);
		node = mtk_cam_vbq_to_vdev(vb->vb2_queue);

		ctx = mtk_cam_find_ctx(cam, &node->vdev.entity);
		if (!ctx)
			continue;
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
#if PURE_RAW_WITH_SV
		atomic_set(&req_stream_data->pure_raw_done_work.is_queued, 0);
#endif
		req->sync_id = (ctx->used_raw_num) ? ctx->pipe->sync_id : 0;

		mtk_cam_s_data_get_scen(&scen, req_stream_data);
		if (mtk_cam_scen_is_mstream(&scen) &&
			scen.scen.mstream.type != MTK_CAM_MSTREAM_1_EXPOSURE)
			mtk_cam_update_s_data_exp(ctx, req, &scen,
						  &ctx->pipe->mstream_exposure);

		/* TODO: AFO independent supports TWIN */
		if (mtk_cam_ctx_has_raw(ctx) &&
		    node->desc.id == MTK_RAW_META_OUT_1) {
			fps = ctx->pipe->user_res.sensor_res.interval.denominator /
				ctx->pipe->user_res.sensor_res.interval.numerator;

			if (ctx->pipe->res_config.raw_num_used == 1 &&
			    GET_PLAT_V4L2(support_afo_independent)) {
				req_stream_data->flags |= MTK_CAM_REQ_S_DATA_FLAG_META1_INDEPENDENT;
			} else {
				dev_dbg(cam->dev,
					"%s:%s: disable AFO independent, raw_num_used(%d), fps(%lu), support_AFO_independent(%d)\n",
					__func__, req->req.debug_str,
					ctx->pipe->res_config.raw_num_used, fps,
					GET_PLAT_V4L2(support_afo_independent));
			}
		}

		if (req_stream_data->seninf_new)
			ctx->seninf = req_stream_data->seninf_new;

		/* update buffer format */
		switch (node->desc.dma_port) {
		case MTKCAM_IPI_RAW_RAWI_2:
			/* get s_data->apu_info for config */
			/* is the rawi always enqued? */
			if (req_stream_data->apu_info.apu_path == APU_FRAME_MODE ||
			    req_stream_data->apu_info.apu_path == APU_DC_RAW)
				ret = mtk_cam_config_raw_img_in_ipui(req_stream_data, buf);
			else
				ret = mtk_cam_config_raw_img_in_rawi2(req_stream_data, buf);
			if (ret)
				return ret;
			break;
		case MTKCAM_IPI_RAW_IMGO:
#if PURE_RAW_WITH_SV
			req_stream_data->pure_raw_sv_tag_idx =
				mtk_cam_config_raw_path(req_stream_data, buf);
			ret = mtk_cam_config_raw_img_out_imgo(req_stream_data, buf);
			if (ret)
				return ret;

			ret = mtk_cam_config_raw_img_fmt(req_stream_data, buf);
			if (ret)
				return ret;
#else
			mtk_cam_config_raw_path(req_stream_data, buf);
			ret = mtk_cam_config_raw_img_out_imgo(req_stream_data, buf);
			if (ret)
				return ret;

			ret = mtk_cam_config_raw_img_fmt(req_stream_data, buf);
			if (ret)
				return ret;
#endif
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
		case MTKCAM_IPI_RAW_DRZS4NO_3:
		case MTKCAM_IPI_RAW_DRZB2NO_1:
			ret = mtk_cam_config_raw_img_out(req_stream_data, buf);
			if (ret)
				return ret;

			ret = mtk_cam_config_raw_img_fmt(req_stream_data, buf);
			if (ret)
				return ret;
			break;

		case MTKCAM_IPI_CAMSV_MAIN_OUT:
			ret = mtk_cam_config_sv_img_out_imgo(req_stream_data, vb);
			if (ret)
				return ret;
			break;
		case MTKCAM_IPI_MRAW_META_STATS_CFG:
		case MTKCAM_IPI_MRAW_META_STATS_0:
			ret = mtk_cam_config_mraw_stats_out(req_stream_data, vb);
			if (ret)
				return ret;
			break;
		case MTKCAM_IPI_RAW_META_STATS_CFG:
		case MTKCAM_IPI_RAW_META_STATS_0:
		case MTKCAM_IPI_RAW_META_STATS_1:
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

		ctx_cnt++;

		ctx = &cam->ctxs[i];

		/* check fs state update (the raw_pipe is updated) */
		if (ctx->pipe && (ctx->pipe->fs_config & MTK_RAW_CTRL_UPDATE)) {
			ctx->pipe->fs_config &= MTK_RAW_CTRL_VALUE;
			req->fs.update_ctx |= 1 << ctx->stream_id;
			req->fs.update_value |=
				(ctx->pipe->fs_config & 0x1) << ctx->stream_id;
			/* both 0->1/1->0 are possible */
			dev_info(cam->dev,
				 "%s: req(%s) ctx-%d fs update(%d), update_ctx/value(0x%x/0x%x)\n",
				 __func__, req->req.debug_str, ctx->stream_id,
				 ctx->pipe->fs_config, req->fs.update_ctx,
				 req->fs.update_value);
		}

		req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		if (!req_stream_data)
			continue;

		mtk_cam_s_data_get_scen(&scen, req_stream_data);
		if (mtk_cam_scen_is_time_shared(&scen))
			check_timeshared_buffer(cam, ctx, req);

		if (mtk_cam_scen_is_mstream_2exp_types(&scen))
			check_mstream_buffer(cam, ctx, req);

		if (mtk_cam_scen_is_rgbw_enabled(&scen)) {
			if (mtk_cam_scen_is_odt(&scen))
				req_stream_data->frame_params.raw_param.hardware_scenario =
					MTKCAM_IPI_HW_PATH_OFFLINE_RGBW;
			else if (mtk_cam_hw_mode_is_dc(ctx->pipe->hw_mode_pending))
				req_stream_data->frame_params.raw_param.hardware_scenario =
					MTKCAM_IPI_HW_PATH_DC_RGBW;
			else
				req_stream_data->frame_params.raw_param.hardware_scenario =
					MTKCAM_IPI_HW_PATH_OTF_RGBW;
		}

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

	req_stream_data_mstream->feature.scen =
		req_stream_data->feature.scen;

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
#if PURE_RAW_WITH_SV
	atomic_set(&req_stream_data->pure_raw_done_work.is_queued, 0);
#endif

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

static void fill_sv_mstream_s_data(struct mtk_cam_ctx *ctx,
				struct mtk_cam_request *req, unsigned int pipe_id)
{
	struct mtk_cam_req_work *frame_done_work;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_request_stream_data *req_stream_data_mstream;

	req_stream_data = mtk_cam_req_get_s_data(req, pipe_id, 0);
	req_stream_data_mstream =
		mtk_cam_req_get_s_data(req, pipe_id, 1);
	req_stream_data_mstream->ctx = ctx;

	/* sequence number */
	req_stream_data_mstream->frame_seq_no = req_stream_data->frame_seq_no;
	req_stream_data->frame_seq_no =
		req_stream_data_mstream->frame_seq_no + 1;
	atomic_set(&ctx->cam->ctxs[pipe_id].enqueued_frame_seq_no, req_stream_data->frame_seq_no);

	/* frame done work */
	atomic_set(&req_stream_data_mstream->frame_done_work.is_queued, 0);
#if PURE_RAW_WITH_SV
	atomic_set(&req_stream_data->pure_raw_done_work.is_queued, 0);
#endif
	frame_done_work = &req_stream_data_mstream->frame_done_work;
	mtk_cam_req_work_init(frame_done_work, req_stream_data_mstream);
	INIT_WORK(&frame_done_work->work, mtk_cam_frame_done_work);

	dev_dbg(ctx->cam->dev, "%s: pipe_id:%d, frame_seq:%d frame_mstream_seq:%d\n",
		__func__, pipe_id, req_stream_data->frame_seq_no,
		req_stream_data_mstream->frame_seq_no);
}

static void fill_mraw_mstream_s_data(struct mtk_cam_ctx *ctx,
				struct mtk_cam_request *req, unsigned int pipe_id)
{
	struct mtk_cam_req_work *frame_done_work;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_request_stream_data *req_stream_data_mstream;

	req_stream_data = mtk_cam_req_get_s_data(req, pipe_id, 0);
	req_stream_data_mstream =
		mtk_cam_req_get_s_data(req, pipe_id, 1);
	req_stream_data_mstream->ctx = ctx;

	/* sequence number */
	req_stream_data_mstream->frame_seq_no = req_stream_data->frame_seq_no;
	req_stream_data->frame_seq_no =
		req_stream_data_mstream->frame_seq_no + 1;
	atomic_set(&ctx->cam->ctxs[pipe_id].enqueued_frame_seq_no, req_stream_data->frame_seq_no);

	/* frame done work */
	atomic_set(&req_stream_data_mstream->frame_done_work.is_queued, 0);
#if PURE_RAW_WITH_SV
	atomic_set(&req_stream_data->pure_raw_done_work.is_queued, 0);
#endif
	frame_done_work = &req_stream_data_mstream->frame_done_work;
	mtk_cam_req_work_init(frame_done_work, req_stream_data_mstream);
	INIT_WORK(&frame_done_work->work, mtk_cam_frame_done_work);

	dev_dbg(ctx->cam->dev, "%s: pipe_id:%d, frame_seq:%d frame_mstream_seq:%d\n",
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
			s_data->state.estate = E_STATE_SENINF;
		}
	}
}

static void mtk_cam_req_s_data_init(struct mtk_cam_request *req,
				    int pipe_id,
				    int s_data_index)
{
	struct mtk_cam_request_stream_data *req_stream_data;

	if (pipe_id < 0 || pipe_id >= MTKCAM_SUBDEV_MAX ||
	    s_data_index < 0 || s_data_index >= MTK_CAM_REQ_MAX_S_DATA) {
		pr_info("%s: Invalid pipe_id(%d), s_data_index(%d)",
			__func__, pipe_id, s_data_index);
		return;
	}

	req_stream_data = &req->p_data[pipe_id].s_data[s_data_index];
	req_stream_data->req = req;
	req_stream_data->pipe_id = pipe_id;
	req_stream_data->state.estate = E_STATE_READY;
	req_stream_data->state.sof_cnt_key = 0;
	req_stream_data->index = s_data_index;
	req_stream_data->req_id = 0;
	req_stream_data->feature.scen = NULL;
#if PURE_RAW_WITH_SV
	req_stream_data->pure_raw_sv_tag_idx = -1;
#endif
	atomic_set(&req_stream_data->buf_state, -1);
	memset(&req_stream_data->apu_info, 0,
		sizeof(req_stream_data->apu_info));
	memset(&req_stream_data->hdr_timestamp_cache, 0,
		sizeof(req_stream_data->hdr_timestamp_cache));

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
#if PURE_RAW_WITH_SV
	mtk_cam_req_work_init(&req_stream_data->pure_raw_done_work,
				  req_stream_data);
#endif

	/**
	 * clean the param structs since we support req reinit.
	 * the mtk_cam_request may not be "zero" when it is
	 * enqueued.
	 */
	memset(&req_stream_data->frame_params, 0,
		   sizeof(req_stream_data->frame_params));
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
	int feature_change;
	int enqueue_req_cnt, job_count, s_data_cnt;
	struct list_head equeue_list;
	struct v4l2_ctrl_handler *hdl;
	struct media_request_object *sensor_hdl_obj, *raw_hdl_obj, *obj;
	unsigned long flags;
	struct mtk_cam_scen *scen;

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
			    RAW_PIPELINE_NUM * MTK_CAM_MAX_RUNNING_JOBS +
			    MTK_CAM_MAX_DISPLAY_IC_RUNNING_JOBS) {
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
			stream_ctx = NULL;
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
				if (mtk_cam_scen_is_mstream_types(s_data->feature.scen) &&
				    (mtk_cam_feature_change_is_mstream(feature_change) ||
				     (req->ctx_link_update & (1 << ctx->pipe->id))))
					mstream_seamless_buf_update(ctx, req, i,
								    s_data->feature.scen,
								    &s_data->feature.prev_scen);

				/* reload s_data */
				s_data->flags = s_data_flags;
				s_data->raw_hdl_obj = raw_hdl_obj;
				s_data->sensor_hdl_obj = sensor_hdl_obj;

				/* copy s_data content */
				if (mtk_cam_scen_is_mstream_types(s_data->feature.scen)) {
					if (mtk_cam_scen_get_exp_num(s_data->feature.scen) == 2)
						fill_mstream_s_data(ctx, req);
					else
						req->p_data[i].s_data_num = 1;
				} else if (req->p_data[i].s_data_num == 2) {
					dev_dbg(cam->dev,
						 "%s:req(%s): update s_data_num, scen(%s)\n",
						 __func__, req->req.debug_str,
						 s_data->feature.scen->dbg_str);
					req->p_data[i].s_data_num = 1;
				}
			} else if (is_camsv_subdev(i) && stream_ctx &&
				   i == stream_ctx->stream_id) {
				if (!(req->ctx_link_update & (1 << i)))
					s_data->sensor = stream_ctx->sensor;

				spin_lock_irqsave(&req->req.lock, flags);
				list_for_each_entry(obj, &req->req.objects, list) {
					if (vb2_request_object_is_buffer(obj))
						continue;

					hdl = (struct v4l2_ctrl_handler *)obj->priv;
					if (stream_ctx->sensor && hdl ==
						stream_ctx->sensor->ctrl_handler)
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
			} else if (is_camsv_subdev(i) && stream_ctx &&
				   i != stream_ctx->stream_id) {
				/* copy s_data content for mstream case */
				scen = &stream_ctx->pipe->user_res.raw_res.scen;
				if (mtk_cam_scen_is_mstream_2exp_types(scen)) {
					req->p_data[i].s_data_num = 2;
					mtk_cam_req_s_data_init(req, i, 1);
					fill_sv_mstream_s_data(stream_ctx, req, i);
				}
			} else if (is_mraw_subdev(i)) {
				/* copy s_data content for mstream case */
				scen = &stream_ctx->pipe->user_res.raw_res.scen;
				if (mtk_cam_scen_is_mstream_2exp_types(scen)) {
					req->p_data[i].s_data_num = 2;
					mtk_cam_req_s_data_init(req, i, 1);
					fill_mraw_mstream_s_data(stream_ctx, req, i);
				}
			} else if (is_raw_subdev(i) && !ctx->sensor) {
				/* pure m2m raw ctrl handle */
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
				/* req->apu_info will be updated after here */
			}
			if (is_raw_subdev(i) && ctx->pipe->apu_info.is_update) {
				memcpy(&s_data->apu_info, &ctx->pipe->apu_info,
					sizeof(s_data->apu_info));
				ctx->pipe->apu_info.is_update = 0;
				dev_dbg(cam->dev,
					"%s:req(%s):pipe(%d) apu update\n",
					__func__, req->req.debug_str, i);
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
	struct mtk_cam_request *cam_req = NULL;

	cam_req = vzalloc(sizeof(*cam_req));
	if (!cam_req)
		return NULL;
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

	if (pipe_id < 0 || pipe_id >= MTKCAM_SUBDEV_MAX)
		pr_info("%s: Invalid pipe_id(%d)", __func__, pipe_id);
	else
		req->p_data[pipe_id].s_data_num = s_data_num;

	for (i = 1; i < s_data_num; i++)
		mtk_cam_req_s_data_init(req, pipe_id, i);
}

static void mtk_cam_req_p_data_init(struct mtk_cam_request *req,
				    int pipe_id,
				    int s_data_num)
{
	int i = 0;

	if (pipe_id < 0 || pipe_id >= MTKCAM_SUBDEV_MAX)
		pr_info("%s: Invalid pipe_id(%d)", __func__, pipe_id);
	else
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
	unsigned int i, num_s_data;
	struct mtk_cam_device *cam =
		container_of(req->mdev, struct mtk_cam_device, media_dev);

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
			/**
			 * By default, the s_data_num is 1;
			 * for some special feature like mstream, it is 2.
			 */
			num_s_data = 1;
			if (is_raw_subdev(i))
				/* init total s_data for raw subdev */
				num_s_data = 2;

			/*
			 * May need to move the init after mtk_cam_req_update_ctrl to get
			 * the correct num_s_data (need per-frame's scen information)
			 */
			mtk_cam_req_p_data_init(cam_req, i, num_s_data);
			mtk_cam_req_get(cam_req, i); /* pipe id */
		}
	}

	return pipe_used;
}

int mtk_cam_req_save_raw_vfmts(struct mtk_raw_pipeline *pipe,
			       struct mtk_cam_request *cam_req,
			       struct mtk_cam_request_stream_data *s_data)
{
	int i;
	struct mtk_cam_video_device *node;
	struct v4l2_format *vfmt;

	for (i = MTK_RAW_SINK_NUM + 1; i < MTK_RAW_META_OUT_BEGIN; i++) {
		if (!(pipe->req_vfmt_update & 1 << i))
			continue;

		node = &pipe->vdev_nodes[i - MTK_RAW_SINK_NUM];
		s_data->vdev_fmt_update |= (1 << node->desc.id);
		vfmt = mtk_cam_s_data_get_vfmt(s_data, node->desc.id);
		if (vfmt)
			*vfmt = node->pending_fmt;
	}

	pipe->req_vfmt_update = 0;

	return 0;
}

int mtk_cam_req_save_raw_vsels(struct mtk_raw_pipeline *pipe,
			       struct mtk_cam_request *cam_req,
			       struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_video_device *node;
	struct v4l2_selection *s;
	int i;

	s_data->vdev_selection_update = pipe->req_vsel_update;

	/* force update format to every video device before re-streamon */
	for (i = MTK_RAW_SINK_NUM + 1; i < MTK_RAW_META_OUT_BEGIN; i++) {
		node = &pipe->vdev_nodes[i - MTK_RAW_SINK_NUM];
		s = mtk_cam_s_data_get_vsel(s_data, node->desc.id);
		if (!s) {
			dev_info(pipe->subdev.v4l2_dev->dev,
				 "%s:%s:%s:%s: can't find the vsel field to save\n",
				 __func__, pipe->subdev.name,
				 cam_req->req.debug_str, node->desc.name);
			continue;
		}

		*s = node->pending_crop;
		if (s_data->vdev_selection_update & (1 << node->desc.id))
			dev_dbg(pipe->subdev.v4l2_dev->dev,
				"%s:%s:%s:%s: update selection (%d,%d,%d,%d)\n",
				__func__, pipe->subdev.name,
				cam_req->req.debug_str, node->desc.name,
				s->r.left, s->r.top,
				s->r.width, s->r.height);
	}

	return 0;
}

int mtk_cam_req_save_raw_pfmt(struct mtk_raw_pipeline *pipe,
			      struct mtk_cam_request *cam_req,
			      struct mtk_cam_request_stream_data *s_data)
{
	int pad;

	s_data->pad_fmt_update = pipe->req_pfmt_update;
	/* save MEDIA_PAD_FL_SINK pad's fmt to request */
	for (pad = MTK_RAW_SINK_BEGIN; pad < MTK_RAW_SOURCE_BEGIN; pad++) {
		if (s_data->pad_fmt_update & 1 << pad)
			s_data->pad_fmt[pad] = pipe->req_pad_fmt[pad];
	}

	/* save MEDIA_PAD_FL_SOURCE pad's fmt to request */
	for (pad = MTK_RAW_SOURCE_BEGIN; pad < MTK_RAW_PIPELINE_PADS_NUM; pad++) {
		if (s_data->pad_fmt_update & 1 << pad)
			s_data->pad_fmt[pad] = pipe->req_pad_fmt[pad];
	}

	pipe->req_pfmt_update = 0;

	return 0;
}

int mtk_cam_req_save_raw_psel(struct mtk_raw_pipeline *pipe,
			      struct mtk_cam_request *cam_req,
			      struct mtk_cam_request_stream_data *s_data)
{
	int pad;

	s_data->pad_selection_update = pipe->req_psel_update;
	/* save MEDIA_PAD_FL_SINK pad's selection to request */
	for (pad = MTK_RAW_SINK_BEGIN; pad < MTK_RAW_SOURCE_BEGIN; pad++) {
		if (s_data->pad_selection_update & 1 << pad)
			s_data->pad_selection[pad] = pipe->req_psel[pad].r;
	}

	/* save MEDIA_PAD_FL_SOURCE pad's selection to request */
	for (pad = MTK_RAW_SOURCE_BEGIN; pad < MTK_RAW_PIPELINE_PADS_NUM; pad++) {
		if (s_data->pad_selection_update & 1 << pad)
			s_data->pad_selection[pad] = pipe->req_psel[pad].r;
	}

	pipe->req_psel_update = 0;

	return 0;
}

int mtk_cam_req_save_sv_vfmts(struct mtk_camsv_pipeline *pipe,
			       struct mtk_cam_request *cam_req,
			       struct mtk_cam_request_stream_data *s_data)
{
	int i;
	struct mtk_cam_video_device *node;
	struct v4l2_format *vfmt;

	for (i = MTK_CAMSV_SOURCE_BEGIN; i < MTK_CAMSV_PIPELINE_PADS_NUM; i++) {
		if (!(pipe->req_vfmt_update & 1 << i))
			continue;

		node = &pipe->vdev_nodes[i - MTK_CAMSV_SOURCE_BEGIN];
		s_data->vdev_fmt_update |= (1 << node->desc.id);
		vfmt = mtk_cam_s_data_get_vfmt(s_data, node->desc.id);
		if (vfmt)
			*vfmt = node->pending_fmt;
	}

	pipe->req_vfmt_update = 0;

	return 0;
}

int mtk_cam_req_save_sv_pfmt(struct mtk_camsv_pipeline *pipe,
			      struct mtk_cam_request *cam_req,
			      struct mtk_cam_request_stream_data *s_data)
{
	int pad;

	s_data->pad_fmt_update = pipe->req_pfmt_update;
	/* save MEDIA_PAD_FL_SINK pad's fmt to request */
	for (pad = MTK_CAMSV_SINK_BEGIN; pad < MTK_CAMSV_PIPELINE_PADS_NUM; pad++) {
		if (s_data->pad_fmt_update & 1 << pad)
			s_data->pad_fmt[pad] = pipe->req_pad_fmt[pad];
	}

	pipe->req_pfmt_update = 0;

	return 0;
}

int mtk_cam_req_save_mraw_pfmt(struct mtk_mraw_pipeline *pipe,
			      struct mtk_cam_request *cam_req,
			      struct mtk_cam_request_stream_data *s_data)
{
	int pad;

	s_data->pad_fmt_update = pipe->req_pfmt_update;
	/* save MEDIA_PAD_FL_SINK pad's fmt to request */
	for (pad = MTK_MRAW_SINK_BEGIN; pad < MTK_MRAW_PIPELINE_PADS_NUM; pad++) {
		if (s_data->pad_fmt_update & 1 << pad)
			s_data->pad_fmt[pad] = pipe->req_pad_fmt[pad];
	}

	pipe->req_pfmt_update = 0;

	return 0;
}

static void mtk_cam_req_init(struct mtk_cam_request *cam_req)
{
	/* reset done status */
	cam_req->done_status = 0;
	cam_req->flags = 0;
	cam_req->ctx_used = 0;
	mtk_cam_fs_reset(&cam_req->fs);
	/* reset buffer count */
	cam_req->enqeued_buf_cnt = 0;
	atomic_set(&cam_req->done_buf_cnt, 0);
}

static void mtk_cam_req_queue(struct media_request *req)
{
	int i;
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);
	struct mtk_cam_device *cam =
		container_of(req->mdev, struct mtk_cam_device, media_dev);
	struct mtk_raw_pipeline *raw_pipeline;
	struct mtk_camsv_pipeline *sv_pipeline;
	struct mtk_mraw_pipeline *mraw_pipeline;
	struct mtk_cam_request_stream_data *s_data;

	mtk_cam_req_init(cam_req);
	cam_req->pipe_used = mtk_cam_req_get_pipe_used(req);

	MTK_CAM_TRACE_BEGIN(BASIC, "vb2_request_queue");

	/* update frame_params's dma_bufs in mtk_cam_vb2_buf_queue */
	vb2_request_queue(req);

	MTK_CAM_TRACE_END(BASIC);

	for (i = MTKCAM_SUBDEV_RAW_0; i < MTKCAM_SUBDEV_RAW_END; i++) {
		if (cam_req->pipe_used & 1 << i) {
			raw_pipeline = &cam->raw.pipelines[i - MTKCAM_SUBDEV_RAW_0];
			s_data = mtk_cam_req_get_s_data_no_chk(cam_req, i, 0);
			mtk_cam_req_save_raw_vsels(raw_pipeline, cam_req, s_data);
			mtk_cam_req_save_raw_pfmt(raw_pipeline, cam_req, s_data);
			mtk_cam_req_save_raw_psel(raw_pipeline, cam_req, s_data);
		}
	}
	for (i = MTKCAM_SUBDEV_CAMSV_START; i < MTKCAM_SUBDEV_CAMSV_END; i++) {
		if (cam_req->pipe_used & 1 << i) {
			sv_pipeline = &cam->sv.pipelines[i - MTKCAM_SUBDEV_CAMSV_START];
			s_data = mtk_cam_req_get_s_data_no_chk(cam_req, i, 0);
			mtk_cam_req_save_sv_pfmt(sv_pipeline, cam_req, s_data);
			mtk_cam_req_save_sv_vfmts(sv_pipeline, cam_req, s_data);
		}
	}
	for (i = MTKCAM_SUBDEV_MRAW_START; i < MTKCAM_SUBDEV_MRAW_END; i++) {
		if (cam_req->pipe_used & 1 << i) {
			mraw_pipeline = &cam->mraw.pipelines[i - MTKCAM_SUBDEV_MRAW_START];
			s_data = mtk_cam_req_get_s_data_no_chk(cam_req, i, 0);
			mtk_cam_req_save_mraw_pfmt(mraw_pipeline, cam_req, s_data);
		}
	}

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

/* to save the link change information in request */
int mtk_cam_collect_link_change(struct mtk_raw_pipeline *pipe,
				struct v4l2_subdev *req_sensor_new,
				struct v4l2_subdev *req_seninf_old,
				struct v4l2_subdev *req_seninf_new)
{
	char warn_desc[48];

	if (pipe->req_sensor_new || pipe->req_seninf_old || pipe->req_seninf_new) {
		snprintf_safe(warn_desc, 48, "%s(%p/%p/%p)",
			      pipe->subdev.name, pipe->req_sensor_new,
			      pipe->req_seninf_old, pipe->req_seninf_new);
		pr_info("%s:%s:prev link change has not been queued:%s\n",
			__func__, pipe->subdev.name, warn_desc);

		mtk_cam_event_error(pipe, "Camsys: prev link change has not been");
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		aee_kernel_exception_api(__FILE__, __LINE__, DB_OPT_DEFAULT,
				       "Camsys: prev link change has not been",
				       warn_desc);
#else
		WARN_ON(1);
#endif
		return -EINVAL;
	}

	pipe->req_sensor_new = req_sensor_new;
	pipe->req_seninf_old = req_seninf_old;
	pipe->req_seninf_new = req_seninf_new;

	return 0;
}

int mtk_cam_req_save_link_change(struct mtk_raw_pipeline *pipe,
				 struct mtk_cam_request *cam_req,
				 struct mtk_cam_request_stream_data *s_data)
{
	char warn_desc[48];

	if (pipe->req_sensor_new || pipe->req_seninf_old || pipe->req_seninf_new) {
		if (pipe->req_sensor_new && pipe->req_seninf_old && pipe->req_seninf_new) {
			s_data->seninf_old = pipe->req_seninf_old;
			s_data->seninf_new = pipe->req_seninf_new;
			s_data->sensor = pipe->req_sensor_new;
			cam_req->ctx_link_update |= 1 << pipe->id;
			dev_info(pipe->subdev.v4l2_dev->dev,
				 "%s:%s:%s: queued link setup: senor_n:%s,seninf_n:%s), seninf_o:%s\n",
				 __func__, pipe->subdev.name, cam_req->req.debug_str,
				 s_data->sensor->name, s_data->seninf_old->name,
				 s_data->seninf_new->name);
			/* clear the setting after it is alrady saved in request */
			pipe->req_sensor_new = NULL;
			pipe->req_seninf_old = NULL;
			pipe->req_seninf_new = NULL;
		} else {
			snprintf_safe(warn_desc, 48,
				      "%s:%s:param's can't be null:sensor_n(%p)/seninf_o(%p)/seninf_n(%p)",
				      pipe->subdev.name, cam_req->req.debug_str,
				      pipe->req_sensor_new, pipe->req_seninf_old,
				      pipe->req_seninf_new);
			dev_info(pipe->subdev.v4l2_dev->dev,
				 "%s:%s\n", __func__, warn_desc);
			mtk_cam_event_error(pipe, "Camsys: invalid link setup param");
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
			aee_kernel_exception_api(__FILE__, __LINE__, DB_OPT_DEFAULT,
					       "Camsys: invalid link setup param",
					       warn_desc);
#else
			WARN_ON(1);
#endif
			return -EINVAL;
		}
	}
	/* not link setup need to be handled*/
	return 0;
}

static struct v4l2_subdev
*mtk_cam_find_sensor_nolock(struct mtk_cam_ctx *ctx,
			    struct media_entity *entity)
{
	struct media_graph *graph;
	struct v4l2_subdev *sensor = NULL;
	struct mtk_cam_device *cam = ctx->cam;

	graph = &ctx->pipeline.graph;
	media_graph_walk_start(graph, entity);

	while ((entity = media_graph_walk_next(graph))) {
		dev_dbg(cam->dev, "linked entity: %s\n", entity->name);
		sensor = NULL;

		switch (entity->function) {
		case MEDIA_ENT_F_CAM_SENSOR:
			sensor = media_entity_to_v4l2_subdev(entity);
			break;
		default:
			break;
		}

		if (sensor)
			break;
	}

	return sensor;
}

static int mtk_cam_link_notify(struct media_link *link, u32 flags,
			      unsigned int notification)
{
	struct media_entity *source = link->source->entity;
	struct media_entity *sink = link->sink->entity;
	struct v4l2_subdev *subdev;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	struct mtk_raw_pipeline *pipe;
	struct v4l2_subdev *req_sensor_new, *req_seninf_old, *req_seninf_new;

	if (source->function != MEDIA_ENT_F_VID_IF_BRIDGE ||
		notification != MEDIA_DEV_NOTIFY_POST_LINK_CH)
		return v4l2_pipeline_link_notify(link, flags, notification);

	subdev = media_entity_to_v4l2_subdev(sink);
	if (!subdev) {
		pr_info("%s: can't find the subdev of sink\n",
			__func__, subdev->name);
		return v4l2_pipeline_link_notify(link, flags, notification);
	}
	cam = container_of(subdev->v4l2_dev->mdev, struct mtk_cam_device, media_dev);
	ctx = mtk_cam_find_ctx(cam, sink);
	if (!ctx || !ctx->streaming || !(flags & MEDIA_LNK_FL_ENABLED)) {
		pr_info("%s: call v4l2_pipeline_link_notify for subdev:%s\n",
			__func__, subdev->name);
		return v4l2_pipeline_link_notify(link, flags, notification);
	}

	pipe = mtk_cam_dev_get_raw_pipeline(cam, ctx->stream_id);
	if (!pipe) {
		dev_info(cam->dev, "%s: can't find the raw pipe(%d)\n",
			 __func__, ctx->stream_id);
		return v4l2_pipeline_link_notify(link, flags, notification);
	}

	if (&pipe->subdev != subdev) {
		dev_info(cam->dev, "%s: skip mtk_cam_collect_link_change for subdev:%s\n",
			 __func__, subdev->name);
		return v4l2_pipeline_link_notify(link, flags, notification);
	}

	req_seninf_old = ctx->seninf;
	req_seninf_new = media_entity_to_v4l2_subdev(source);
	req_sensor_new = mtk_cam_find_sensor_nolock(ctx, &req_seninf_new->entity);
	if (!mtk_cam_collect_link_change(pipe, req_sensor_new,
					 req_seninf_old, req_seninf_new))
		dev_info(cam->dev,
			 "link_change req ctx:%d, old seninf:%s, new seninf:%s, sensor:%s\n",
			 ctx->stream_id, req_seninf_old->name,
			 req_seninf_new->name, req_sensor_new->name);

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

void mstream_seamless_buf_update(struct mtk_cam_ctx *ctx,
				struct mtk_cam_request *req, int pipe_id,
				struct mtk_cam_scen *scen, struct mtk_cam_scen *scen_prev)
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
	struct mtk_cam_video_device *vdev;
	int main_stream_size;
	__u64 iova;
	__u32 ccd_fd;
	__u8 imgo_path_sel;

	vdev = &ctx->pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
	main_stream_size = vdev->active_fmt.fmt.pix_mp.plane_fmt[1].sizeimage;

	pr_info("%s cur_scen(%s) prev_scen(%s) main_stream_size(%d)",
		__func__, scen->dbg_str, scen_prev->dbg_str,
		main_stream_size);

	/* backup first because main stream buffer is already assigned */
	iova = frame_param->img_outs[desc_id].buf[0][0].iova;
	ccd_fd = frame_param->img_outs[desc_id].buf[0][0].ccd_fd;
	imgo_path_sel = frame_param->raw_param.imgo_path_sel;

	if (mtk_cam_scen_is_mstream_2exp_types(scen)) {
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

	if (scen->scen.mstream.type == MTK_CAM_MSTREAM_NE_SE) {
		mstream_frame_param->raw_param.exposure_num = 1;
		frame_param->raw_param.exposure_num = 2;
		frame_param->raw_param.hardware_scenario =
			MTKCAM_IPI_HW_PATH_MSTREAM;
	} else if (scen->scen.mstream.type == MTK_CAM_MSTREAM_SE_NE) {
		mstream_frame_param->raw_param.exposure_num = 1;
		frame_param->raw_param.exposure_num = 2;
		// Dujac todo
		frame_param->raw_param.hardware_scenario =
			MTKCAM_IPI_HW_PATH_MSTREAM;
	} else {
		// normal single exposure
		mstream_frame_param->raw_param.exposure_num = 0;
		frame_param->raw_param.exposure_num = 1;
		// Dujac todo
		frame_param->raw_param.hardware_scenario =
			MTKCAM_IPI_HW_PATH_ON_THE_FLY;
	}

	// imgo mstream buffer layout is fixed plane[0]=NE, plane[1]=SE
	if (scen->scen.mstream.type == MTK_CAM_MSTREAM_NE_SE) {
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
	} else if (scen->scen.mstream.type == MTK_CAM_MSTREAM_SE_NE) {
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

		if (scen_prev->scen.mstream.type  == MTK_CAM_MSTREAM_NE_SE) {
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

int get_first_sv_tag_idx(struct mtk_cam_ctx *ctx,
							 unsigned int exp_no, bool is_w)
{
	int tag_idx = -1;

	if (exp_no == 1)
		tag_idx = (is_w) ? SVTAG_3 : SVTAG_2;
	else if (exp_no == 2)
		tag_idx = (is_w) ? SVTAG_1 : SVTAG_0;
	else if (exp_no == 3)
		tag_idx = SVTAG_0;

	return tag_idx;
}

int get_second_sv_tag_idx(struct mtk_cam_ctx *ctx,
							unsigned int exp_no, bool is_w)
{
	int tag_idx = -1;

	if (exp_no == 3)
		tag_idx = SVTAG_1;

	return tag_idx;
}

int get_last_sv_tag_idx(struct mtk_cam_ctx *ctx,
							unsigned int exp_no, bool is_w)
{
	int tag_idx = -1;

	if (exp_no == 2)
		tag_idx = (is_w) ? SVTAG_3 : SVTAG_2;
	else if (exp_no == 3)
		tag_idx = SVTAG_2;

	return tag_idx;
}

unsigned int get_master_raw_id(unsigned int num_raw_drivers,
									  unsigned int enabled_raw)
{
	unsigned int i;

	for (i = 0; i < num_raw_drivers; i++) {
		if (enabled_raw & (1 << i))
			break;
	}

	if (i == num_raw_drivers)
		pr_info("no master raw id found, enabled_raw 0x%x", enabled_raw);

	return i;
}

struct mtk_raw_device *get_master_raw_dev(struct mtk_cam_device *cam,
					  struct mtk_raw_pipeline *pipe)
{
	unsigned int i = get_master_raw_id(cam->num_raw_drivers, pipe->enabled_raw);
	struct device *dev_master = cam->raw.devs[i];

	return dev_get_drvdata(dev_master);
}

struct mtk_raw_device *get_slave_raw_dev(struct mtk_cam_device *cam,
					 struct mtk_raw_pipeline *pipe)
{
	struct device *dev_slave = NULL;
	unsigned int i;

	for (i = 0; i < cam->num_raw_drivers - 1; i++) {
		if (pipe->enabled_raw & (1 << i)) {
			dev_slave = cam->raw.devs[i + 1];
			break;
		}
	}

	return (dev_slave) ? dev_get_drvdata(dev_slave) : NULL;
}

struct mtk_raw_device *get_slave2_raw_dev(struct mtk_cam_device *cam,
					  struct mtk_raw_pipeline *pipe)
{
	struct device *dev_slave = NULL;
	unsigned int i;

	for (i = 0; i < cam->num_raw_drivers; i++) {
		if (pipe->enabled_raw & (1 << i)) {
			dev_slave = cam->raw.devs[i + 2];
			break;
		}
	}

	return (dev_slave) ? dev_get_drvdata(dev_slave) : NULL;
}

struct mtk_yuv_device *get_yuv_dev(struct mtk_raw_device *raw_dev)
{
	struct device *dev;
	struct mtk_cam_device *cam = raw_dev->cam;

	dev = cam->raw.yuvs[raw_dev->id - MTKCAM_SUBDEV_RAW_START];

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
	struct mtk_camsv_working_buf_entry *sv_buf_entry;
	struct mtk_mraw_working_buf_entry *mraw_buf_entry[MAX_MRAW_PIPES_PER_STREAM];
	struct mtk_cam_request_stream_data *s_data;
	struct mtk_cam_request *req;
	bool is_m2m_apply_cq = false;
	bool is_mux_change_with_apply_cq = false;
	int i;
	struct mtk_raw_device *raw_dev;
	struct mtk_mraw_device *mraw_dev;
	struct mtk_cam_scen scen;

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

	spin_lock(&ctx->using_buffer_list.lock);

	ctx->composed_frame_seq_no = ipi_msg->cookie.frame_no;
	for (i = 0; i < ctx->used_mraw_num; i++)
		ctx->mraw_composed_frame_seq_no[i] = ipi_msg->cookie.frame_no;

	/**
	 * To be noticed that it is the last scen of the enqueue's s_data,
	 * not the latest composed s_data. We may need some modification
	 * to get the precise scen of the latest atest composed s_data.
	 */
	if (mtk_cam_scen_is_mstream_2exp_types(&ctx->pipe->user_res.raw_res.scen)) {
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

	spin_lock(&ctx->sv_using_buffer_list.lock);
	if (list_empty(&ctx->sv_using_buffer_list.list)) {
		spin_unlock(&ctx->sv_using_buffer_list.lock);
		spin_unlock(&ctx->using_buffer_list.lock);
		return -EINVAL;
	}
	spin_unlock(&ctx->sv_using_buffer_list.lock);

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

	mtk_cam_scen_init(&scen);
	if (!mtk_cam_s_data_get_scen(&scen, s_data)) {
		dev_info(dev,
			 "%s:%d: can't get scen from s_data(%d):%p\n",
			 __func__, s_data->pipe_id,
			 s_data->frame_seq_no, s_data->feature.scen);
	}

	req = mtk_cam_s_data_get_req(s_data);
	if (mtk_cam_is_immediate_switch_req(req, s_data->pipe_id)) {
		if (mtk_cam_scen_is_mstream_2exp_types(&scen)) {
			struct mtk_cam_request_stream_data *mstream_1st_data;

			mstream_1st_data =  mtk_cam_req_get_s_data(req, ctx->stream_id, 1);
			if (ipi_msg->cookie.frame_no == mstream_1st_data->frame_seq_no) {
				is_mux_change_with_apply_cq = true;
				spin_lock(&ctx->first_cq_lock);
				ctx->is_first_cq_done = 0;
				ctx->cq_done_status = 0;
				spin_unlock(&ctx->first_cq_lock);
			}
		} else {
			is_mux_change_with_apply_cq = true;
			spin_lock(&ctx->first_cq_lock);
			ctx->is_first_cq_done = 0;
			ctx->cq_done_status = 0;
			spin_unlock(&ctx->first_cq_lock);
		}
	}

	if (mtk_cam_is_nonimmediate_switch_req(req, s_data->pipe_id) &&
	    (s_data->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_SWITCH_BACKEND_DELAYED))
		is_mux_change_with_apply_cq = true;

	buf_entry->cq_desc_offset =
		ipi_msg->ack_data.frame_result.main.offset;
	buf_entry->cq_desc_size =
		ipi_msg->ack_data.frame_result.main.size;
	buf_entry->sub_cq_desc_offset =
		ipi_msg->ack_data.frame_result.sub.offset;
	buf_entry->sub_cq_desc_size =
		ipi_msg->ack_data.frame_result.sub.size;

	/* Do nothing if the user doesn't enable force dump */
	if (mtk_cam_scen_is_mstream_2exp_types(&scen)) {
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
			"Camsys: compose error", false);
		spin_unlock(&ctx->using_buffer_list.lock);
		s_data->flags |= MTK_CAM_REQ_S_DATA_FLAG_COMPOMSED_ERROR;
		return -EINVAL;
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
			ipi_msg->ack_data.frame_result.mraw[i].offset;
		mraw_buf_entry[i]->mraw_cq_desc_size =
			ipi_msg->ack_data.frame_result.mraw[i].size;
	}

	/* assign camsv using buf */
	spin_lock(&ctx->sv_using_buffer_list.lock);
	sv_buf_entry = list_first_entry(&ctx->sv_using_buffer_list.list,
						struct mtk_camsv_working_buf_entry,
						list_entry);
	list_del(&sv_buf_entry->list_entry);
	ctx->sv_using_buffer_list.cnt--;
	spin_unlock(&ctx->sv_using_buffer_list.lock);

	/* camsv todo: ais */
	sv_buf_entry->buffer.iova = buf_entry->buffer.iova;
	sv_buf_entry->sv_cq_desc_offset =
		ipi_msg->ack_data.frame_result.camsv[0].offset;
	sv_buf_entry->sv_cq_desc_size = (mtk_cam_scen_is_m2m(&scen)) ?
		0 : ipi_msg->ack_data.frame_result.camsv[0].size;

	if (mtk_cam_scen_is_m2m(&scen)) {
		struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
		struct mtk_camsys_ctrl_state *state_temp;
		bool allow_m2m_trigger = true;

		/* List state-queue status*/
		spin_lock(&sensor_ctrl->camsys_state_lock);
		list_for_each_entry(state_temp, &sensor_ctrl->camsys_state_list,
				    state_element) {
			if (state_temp->estate >= E_STATE_CQ)
				allow_m2m_trigger = false;
		}
		spin_unlock(&sensor_ctrl->camsys_state_lock);

		/* here do nothing */
		spin_lock(&ctx->composed_buffer_list.lock);
		dev_dbg(dev, "%s ctx->composed_buffer_list.cnt %d\n", __func__,
			ctx->composed_buffer_list.cnt);

		if (allow_m2m_trigger && ctx->composed_buffer_list.cnt == 0)
			is_m2m_apply_cq = true;

		spin_unlock(&ctx->composed_buffer_list.lock);

		spin_lock(&ctx->processing_buffer_list.lock);
		dev_dbg(dev, "%s ctx->processing_buffer_list.cnt %d\n", __func__,
			ctx->processing_buffer_list.cnt);
		spin_unlock(&ctx->processing_buffer_list.lock);
	}
	if (mtk_cam_scen_is_ext_isp(&scen) &&
		ctx->ext_isp_meta_off &&
		ctx->ext_isp_pureraw_off) {
		if (mtk_cam_scen_is_ext_isp_yuv(&scen)) {
			s_data->state.estate = E_STATE_EXTISP_SV_INNER;
			dev_info(dev, "only porc yuv req:%d [E_STATE_EXTISP_SV_INNER]\n",
					s_data->frame_seq_no);
		} else {
			/* TBC: Yu-ming, need confirmation */
			/* ctx->pipe->feature_active = 0x0; */
			ctx->pipe->scen_active.id = MTK_CAM_SCEN_NORMAL;
		}
	}
	/* initial frame */
	if ((ctx->composed_frame_seq_no == 1 &&
		!mtk_cam_scen_is_time_shared(&scen)) ||
	    is_m2m_apply_cq || is_mux_change_with_apply_cq) {
		struct device *dev;

		if (mtk_cam_scen_is_ext_isp(&scen)) {
			spin_lock(&ctx->composed_buffer_list.lock);
			list_add_tail(&buf_entry->list_entry,
					  &ctx->composed_buffer_list.list);
			ctx->composed_buffer_list.cnt++;
			spin_unlock(&ctx->composed_buffer_list.lock);
		} else {
			spin_lock(&ctx->processing_buffer_list.lock);
			list_add_tail(&buf_entry->list_entry,
					  &ctx->processing_buffer_list.list);
			ctx->processing_buffer_list.cnt++;
			spin_unlock(&ctx->processing_buffer_list.lock);
		}

		for (i = 0; i < ctx->used_mraw_num; i++) {
			spin_lock(&ctx->mraw_processing_buffer_list[i].lock);
			list_add_tail(&mraw_buf_entry[i]->list_entry,
					&ctx->mraw_processing_buffer_list[i].list);
			ctx->mraw_processing_buffer_list[i].cnt++;
			spin_unlock(&ctx->mraw_processing_buffer_list[i].lock);
		}

		spin_lock(&ctx->sv_processing_buffer_list.lock);
		list_add_tail(&sv_buf_entry->list_entry,
				&ctx->sv_processing_buffer_list.list);
		ctx->sv_processing_buffer_list.cnt++;
		spin_unlock(&ctx->sv_processing_buffer_list.lock);

		spin_unlock(&ctx->using_buffer_list.lock);

		dev = mtk_cam_find_raw_dev(cam, ctx->pipe->enabled_raw);
		if (!dev) {
			dev_dbg(dev, "frm#1 raw device not found\n");
			return -EINVAL;
		}

		raw_dev = dev_get_drvdata(dev);

		if (mtk_cam_scen_is_m2m(&scen)) {
			dev_dbg(dev, "%s M2M apply_cq, composed_buffer_list.cnt %d frame_seq_no %d\n",
				__func__, ctx->composed_buffer_list.cnt,
				s_data->frame_seq_no);
			mtk_cam_m2m_enter_cq_state(&s_data->state);
		}

		/* mmqos update */
		mtk_cam_qos_bw_calc(ctx, s_data->raw_dmas, false);

		if (ctx->sv_dev) {
			/* may be programmed by raw's scq under dcif case */
			/* or not enqueued for any tag */
			if (sv_buf_entry->sv_cq_desc_size > 0)
				atomic_set(&ctx->sv_dev->is_first_frame, 1);
			else {
				/* not queued for first frame, so update cq done directly */
				spin_lock(&ctx->first_cq_lock);
				ctx->cq_done_status |=
					1 << (ctx->sv_dev->id + MTKCAM_SUBDEV_CAMSV_START);
				spin_unlock(&ctx->first_cq_lock);
				atomic_set(&ctx->sv_dev->is_first_frame, 0);
			}
		}

		for (i = 0; i < ctx->used_mraw_num; i++) {
			mraw_dev = get_mraw_dev(ctx->cam, ctx->mraw_pipe[i]);
			if (mraw_buf_entry[i]->s_data->req->pipe_used &
				(1 << ctx->mraw_pipe[i]->id)) {
				atomic_set(&mraw_dev->is_enqueued, 1);
				atomic_set(&mraw_dev->is_first_frame, 1);
			} else {
				/* not queued for first frame, so update cq done directly */
				spin_lock(&ctx->first_cq_lock);
				ctx->cq_done_status |= (1 << ctx->mraw_pipe[i]->id);
				spin_unlock(&ctx->first_cq_lock);
				atomic_set(&mraw_dev->is_first_frame, 0);
			}
		}

		/* apply raw cq */
		if (!mtk_cam_scen_is_ext_isp(&scen))
			apply_cq(raw_dev, 1, buf_entry->buffer.iova,
				buf_entry->cq_desc_size,
				buf_entry->cq_desc_offset,
				buf_entry->sub_cq_desc_size,
				buf_entry->sub_cq_desc_offset);
		else {
			spin_lock(&ctx->first_cq_lock);
			ctx->cq_done_status |= (1 << ctx->pipe->id);
			spin_unlock(&ctx->first_cq_lock);
		}

		/* apply camsv cq */
		if (ctx->sv_dev)
			apply_camsv_cq(ctx->sv_dev,
				sv_buf_entry->buffer.iova,
				sv_buf_entry->sv_cq_desc_size,
				sv_buf_entry->sv_cq_desc_offset,
				(ctx->composed_frame_seq_no == 1) ? 1 : 0);
		for (i = 0; i < ctx->used_sv_num; i++) {
			if ((sv_buf_entry->s_data->req->pipe_used &
				(1 << ctx->sv_pipe[i]->id)))
				mtk_ctx_watchdog_start(ctx, 4, ctx->sv_pipe[i]->id);
			else
				mtk_ctx_watchdog_stop(ctx, ctx->sv_pipe[i]->id);
		}

		/* apply mraw CQ for all streams */
		for (i = 0; i < ctx->used_mraw_num; i++) {
			mraw_dev = get_mraw_dev(ctx->cam, ctx->mraw_pipe[i]);
			if (mraw_buf_entry[i]->s_data->req->pipe_used &
				(1 << ctx->mraw_pipe[i]->id)) {
				apply_mraw_cq(mraw_dev,
					mraw_buf_entry[i]->buffer.iova,
					mraw_buf_entry[i]->mraw_cq_desc_size,
					mraw_buf_entry[i]->mraw_cq_desc_offset,
					(ctx->composed_frame_seq_no == 1) ? 1 : 0);
			} else {
				mtk_ctx_watchdog_stop(ctx, ctx->mraw_pipe[i]->id);
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
	if (mtk_cam_scen_is_m2m(&scen)) {
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

	spin_lock(&ctx->sv_composed_buffer_list.lock);
	list_add_tail(&sv_buf_entry->list_entry,
			&ctx->sv_composed_buffer_list.list);
	ctx->sv_composed_buffer_list.cnt++;
	spin_unlock(&ctx->sv_composed_buffer_list.lock);

	spin_unlock(&ctx->using_buffer_list.lock);
	/*composed delay to over sof, to keep fps, trigger cq here by conditions*/
	if (s_data->frame_seq_no == atomic_read(&ctx->composed_delay_seq_no)) {
		dev = mtk_cam_find_raw_dev(cam, ctx->pipe->enabled_raw);
		raw_dev = dev_get_drvdata(dev);
		mtk_camsys_composed_delay_enque(raw_dev, ctx, s_data);
	}

	return 0;
}

static int isp_composer_handle_sv_ack(struct mtk_cam_device *cam,
				   struct mtkcam_ipi_event *ipi_msg)
{
	struct device *dev = cam->dev;
	struct mtk_cam_ctx *ctx;
	struct mtk_camsv_working_buf_entry *buf_entry;
	struct mtk_cam_request_stream_data *s_data;
	struct mtk_cam_request *req;

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

	spin_lock(&ctx->sv_using_buffer_list.lock);

	ctx->sv_composed_frame_seq_no = ipi_msg->cookie.frame_no;

	/* get from sv using list */
	if (list_empty(&ctx->sv_using_buffer_list.list)) {
		spin_unlock(&ctx->sv_using_buffer_list.lock);
		return -EINVAL;
	}

	/* assign raw using buf */
	buf_entry = list_first_entry(&ctx->sv_using_buffer_list.list,
				     struct mtk_camsv_working_buf_entry,
				     list_entry);
	list_del(&buf_entry->list_entry);
	ctx->sv_using_buffer_list.cnt--;

	s_data = mtk_cam_sv_wbuf_get_s_data(buf_entry);
	if (!s_data) {
		dev_dbg(dev, "ctx:%d no req for ack frame_num:%d\n",
			ctx->stream_id, ctx->sv_composed_frame_seq_no);
		spin_unlock(&ctx->sv_using_buffer_list.lock);
		return -EINVAL;
	}

	req = mtk_cam_s_data_get_req(s_data);

	/* camsv todo: ais */
	buf_entry->sv_cq_desc_offset =
		ipi_msg->ack_data.frame_result.camsv[0].offset;
	buf_entry->sv_cq_desc_size =
		ipi_msg->ack_data.frame_result.camsv[0].size;

	if (ctx->sv_composed_frame_seq_no == 1) {
		spin_lock(&ctx->sv_processing_buffer_list.lock);
		list_add_tail(&buf_entry->list_entry,
			      &ctx->sv_processing_buffer_list.list);
		ctx->sv_processing_buffer_list.cnt++;
		spin_unlock(&ctx->sv_processing_buffer_list.lock);

		spin_unlock(&ctx->sv_using_buffer_list.lock);

		atomic_set(&ctx->sv_dev->is_first_frame, 1);
		/* mmqos update for single sv case */
		mtk_cam_qos_sv_bw_calc(ctx, false);

		apply_camsv_cq(ctx->sv_dev,
			buf_entry->buffer.iova,
			buf_entry->sv_cq_desc_size,
			buf_entry->sv_cq_desc_offset,
			(ctx->sv_composed_frame_seq_no == 1) ? 1 : 0);

		s_data->timestamp = ktime_get_boottime_ns();
		s_data->timestamp_mono = ktime_get_ns();

		return 0;
	}
	spin_lock(&ctx->sv_composed_buffer_list.lock);
	list_add_tail(&buf_entry->list_entry,
		      &ctx->sv_composed_buffer_list.list);
	ctx->sv_composed_buffer_list.cnt++;
	spin_unlock(&ctx->sv_composed_buffer_list.lock);

	spin_unlock(&ctx->sv_using_buffer_list.lock);

	/* camsv todo: consider delay enque */

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

		if (is_raw_subdev(ipi_msg->cookie.session_id)) {
			ctx = &cam->ctxs[ipi_msg->cookie.session_id];
			mutex_lock(&ctx->sensor_switch_op_lock);
			ret = isp_composer_handle_ack(cam, ipi_msg);
			mutex_unlock(&ctx->sensor_switch_op_lock);
		} else {
			ret = isp_composer_handle_sv_ack(cam, ipi_msg);
		}

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

static int isp_tx_frame_get_bin_flag(int bin)
{
	int bin_flag;

	switch (bin) {
	case MTK_CAM_BIN_OFF:
		bin_flag = BIN_OFF;
		break;
	case MTK_CAM_BIN_ON:
		bin_flag = BIN_ON;
		break;
	case MTK_CAM_CBN_2X2_ON:
		bin_flag = CBN_2X2_ON;
		break;
	case MTK_CAM_CBN_3X3_ON:
		bin_flag = CBN_3X3_ON;
		break;
	case MTK_CAM_CBN_4X4_ON:
		bin_flag = CBN_4X4_ON;
		break;
	case MTK_CAM_QBND_ON:
		bin_flag = QBND_ON;
		break;
	default:
		bin_flag = -EINVAL;
		break;
	}

	return bin_flag;
}

static void isp_tx_frame_worker(struct work_struct *work)
{
	struct mtk_cam_req_work *req_work = (struct mtk_cam_req_work *)work;
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_frame_info *frame_info = &event.frame_data;
	struct mtkcam_ipi_frame_param *frame_data, *frame_param;
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_request_stream_data *req_stream_data_sv;
	struct mtk_cam_request_stream_data *req_stream_data_mraw;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	struct mtk_cam_working_buf_entry *buf_entry;
	struct mtk_camsv_working_buf_entry *sv_buf_entry;
	struct mtk_mraw_working_buf_entry *mraw_buf_entry;
	struct mtkcam_ipi_img_output *imgo_out_fmt;
	struct mtk_cam_buffer *meta1_buf;
	struct mtk_cam_resource_v2 *res_user;
	struct mtk_cam_resource_config *res_cfg;
	struct mtkcam_ipi_config_param *config_param;
	int i;
	struct mtk_cam_scen *scen;
	int bin_flag;

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

	scen = mtk_cam_s_data_get_res_feature(req_stream_data);

	/* Send CAM_CMD_CONFIG if the sink pad fmt is changed */
	if (req->ctx_link_update & 1 << ctx->stream_id ||
	    req_stream_data->flags & MTK_CAM_REQ_S_DATA_FLAG_SINK_FMT_UPDATE)
		mtk_cam_s_data_dev_config(req_stream_data, true, true);

	/* save config_param for debug and exception dump */
	config_param = mtk_cam_s_data_get_config_param(req_stream_data);
	if (config_param)
		*config_param = ctx->config_params;

	/* handle stagger 1,2,3 exposure */
	if (mtk_cam_scen_is_sensor_stagger(scen) ||
	    mtk_cam_hw_mode_is_dc(ctx->pipe->hw_mode_pending))
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

	/* mraw todo: move to req update? */
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

		/* align master pipe's sequence number */
		req_stream_data_mraw->frame_seq_no = req_stream_data->frame_seq_no;
		req_stream_data_mraw->req = req_stream_data->req;
		req_stream_data_mraw->pipe_id = ctx->mraw_pipe[i]->id;
		req_stream_data_mraw->ctx = ctx;
		mtk_cam_req_dump_work_init(req_stream_data_mraw);

		spin_lock(&ctx->mraw_using_buffer_list[i].lock);
		list_add_tail(&mraw_buf_entry->list_entry,
			&ctx->mraw_using_buffer_list[i].list);
		ctx->mraw_using_buffer_list[i].cnt++;
		spin_unlock(&ctx->mraw_using_buffer_list[i].lock);
	}

	/* prepare sv working buffer */
	for (i = 0; i < ctx->used_sv_num; i++) {
		req->p_data[ctx->sv_pipe[i]->id].s_data_num =
			req->p_data[ctx->stream_id].s_data_num;
		req_stream_data_sv =
			mtk_cam_req_get_s_data(req, ctx->sv_pipe[i]->id,
			req_stream_data->index);

		/* align master pipe's sequence number */
		req_stream_data_sv->frame_seq_no = req_stream_data->frame_seq_no;
		req_stream_data_sv->req = req_stream_data->req;
		req_stream_data_sv->pipe_id = ctx->sv_pipe[i]->id;
		req_stream_data_sv->ctx = ctx;
		mtk_cam_req_dump_work_init(req_stream_data_sv);
	}
	sv_buf_entry = mtk_cam_sv_working_buf_get(ctx);
	if (!sv_buf_entry) {
		dev_info(cam->dev, "%s: No CQ buf availablle: req:%d(%s)\n",
			 __func__, req_stream_data->frame_seq_no,
			 req->req.debug_str);
		WARN_ON(1);
		return;
	}
	mtk_cam_s_data_set_sv_wbuf(req_stream_data, sv_buf_entry);
	/* put to sv using list */
	spin_lock(&ctx->sv_using_buffer_list.lock);
	list_add_tail(&sv_buf_entry->list_entry, &ctx->sv_using_buffer_list.list);
	ctx->sv_using_buffer_list.cnt++;
	spin_unlock(&ctx->sv_using_buffer_list.lock);

	/* Prepare rp message */
	frame_info->cur_msgbuf_offset =
		buf_entry->msg_buffer.va -
		cam->ctxs[session->session_id].buf_pool.msg_buf_va;
	frame_info->cur_msgbuf_size = buf_entry->msg_buffer.size;
	frame_data = (struct mtkcam_ipi_frame_param *)buf_entry->msg_buffer.va;
	session->frame_no = req_stream_data->frame_seq_no;

	imgo_out_fmt = &req_stream_data->frame_params.img_outs[MTK_RAW_MAIN_STREAM_OUT -
			MTK_RAW_SOURCE_BEGIN];

	if (mtk_cam_scen_is_sensor_stagger(scen) ||
	    mtk_cam_scen_is_mstream(scen) ||
	    mtk_cam_scen_is_mstream_m2m(scen) ||
		mtk_cam_scen_is_odt(scen))
		dev_dbg(cam->dev, "[%s:vhdr-req:%d] ctx:%d type:%d (ipi)hwscene:%d/expN:%d/prev_expN:%d, imgo:0x%x\n",
			__func__, req_stream_data->frame_seq_no, ctx->stream_id,
			req_stream_data->feature.switch_feature_type,
			req_stream_data->frame_params.raw_param.hardware_scenario,
			req_stream_data->frame_params.raw_param.exposure_num,
			req_stream_data->frame_params.raw_param.previous_exposure_num,
			imgo_out_fmt->buf[0][0].iova);

	/* record mmqos */
	frame_param = &req_stream_data->frame_params;
	req_stream_data->raw_dmas = 0;

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
	req_stream_data->raw_dmas |= (1ULL << MTKCAM_IPI_RAW_META_STATS_CFG);

	if (req_stream_data->apu_info.apu_path == APU_FRAME_MODE ||
	    req_stream_data->apu_info.apu_path == APU_DC_RAW) {
		frame_param->adl_param.slb_addr = (__u64)ctx->slb_addr;
		frame_param->adl_param.slb_size = ctx->slb_size;
	}

	memcpy(frame_data, &req_stream_data->frame_params,
	       sizeof(req_stream_data->frame_params));
	frame_data->cur_workbuf_offset =
		buf_entry->buffer.iova -
		cam->ctxs[session->session_id].buf_pool.working_buf_iova;
	frame_data->cur_workbuf_size = buf_entry->buffer.size;

	res_user = mtk_cam_s_data_get_res(req_stream_data);
	frame_data->raw_param.bin_flag = BIN_OFF;
	if (res_user) {
		bin_flag = isp_tx_frame_get_bin_flag(res_user->raw_res.bin);
		if (bin_flag == -EINVAL)
			dev_info(cam->dev,
				 "%s: rpmsg_send id: %d, ctx:%d, seq:%d, invalid bin setting:(0x%x) from user\n",
				 req->req.debug_str, event.cmd_id, session->session_id,
				 req_stream_data->frame_seq_no,
				 res_user->raw_res.bin);
		else
			frame_data->raw_param.bin_flag = bin_flag;
	}

	res_cfg =  mtk_cam_s_data_get_res_cfg(req_stream_data);
	if (res_cfg) {
		mutex_lock(&cam->dvfs_op_lock);
		mtk_cam_dvfs_tbl_add_opp(&ctx->dvfs_tbl, res_cfg->opp_idx);
		mtk_cam_dvfs_update_clk(cam, false);
		mutex_unlock(&cam->dvfs_op_lock);
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

static void isp_tx_sv_frame_worker(struct work_struct *work)
{
	struct mtk_cam_req_work *req_work = (struct mtk_cam_req_work *)work;
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_frame_info *frame_info = &event.frame_data;
	struct mtkcam_ipi_frame_param *frame_data;
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_camsv_working_buf_entry *buf_entry;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;

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

	if (ctx->used_sv_num == 0) {
		dev_info(cam->dev, "sv is un-used, skip sv frame work");
		return;
	}

	/* check if the ctx is streaming */
	spin_lock(&ctx->streaming_lock);
	if (!ctx->streaming) {
		dev_info(cam->dev,
			 "%s: skip sv frame work, for stream off ctx:%d, req:%d\n",
			 __func__, ctx->stream_id, req_stream_data->frame_seq_no);
		spin_unlock(&ctx->streaming_lock);
		return;
	}
	spin_unlock(&ctx->streaming_lock);

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_FRAME;
	session->session_id = ctx->stream_id;

	/* prepare sv working buffer */
	buf_entry = mtk_cam_sv_working_buf_get(ctx);
	if (!buf_entry) {
		dev_info(cam->dev, "%s: No CQ buf availablle: req:%d(%s)\n",
			 __func__, req_stream_data->frame_seq_no,
			 req->req.debug_str);
		WARN_ON(1);
		return;
	}
	mtk_cam_s_data_set_sv_wbuf(req_stream_data, buf_entry);
	buf_entry->ctx = ctx;

	/* put to sv using list */
	spin_lock(&ctx->sv_using_buffer_list.lock);
	list_add_tail(&buf_entry->list_entry, &ctx->sv_using_buffer_list.list);
	ctx->sv_using_buffer_list.cnt++;
	spin_unlock(&ctx->sv_using_buffer_list.lock);

	/* prepare rp message */
	frame_info->cur_msgbuf_offset =
		buf_entry->msg_buffer.va -
		cam->ctxs[session->session_id].buf_pool.msg_buf_va;
	frame_info->cur_msgbuf_size = buf_entry->msg_buffer.size;
	frame_data = (struct mtkcam_ipi_frame_param *)buf_entry->msg_buffer.va;
	session->frame_no = req_stream_data->frame_seq_no;

	memcpy(frame_data, &req_stream_data->frame_params,
	       sizeof(req_stream_data->frame_params));
	frame_data->cur_workbuf_offset =
		buf_entry->buffer.iova -
		cam->ctxs[session->session_id].buf_pool.working_buf_iova;
	frame_data->cur_workbuf_size = buf_entry->buffer.size;

	if (ctx->rpmsg_dev) {
		rpmsg_send(ctx->rpmsg_dev->rpdev.ept, &event, sizeof(event));

		dev_dbg(cam->dev,
			 "%s: rpmsg_send id: %d, ctx:%d, seq:%d\n",
			 req->req.debug_str, event.cmd_id, session->session_id,
			 req_stream_data->frame_seq_no);
	} else {
		dev_dbg(cam->dev, "%s: rpmsg_dev not exist: %d, ctx:%d, req:%d\n",
			__func__, event.cmd_id, session->session_id,
			req_stream_data->frame_seq_no);
	}
}

void mtk_cam_sv_reset_tag_info(struct mtk_camsv_tag_info *arr_tag)
{
	struct mtk_camsv_tag_info *tag_info;
	int i;

	for (i = SVTAG_START; i < SVTAG_END; i++) {
		tag_info = &arr_tag[i];
		tag_info->sv_pipe = NULL;
		tag_info->seninf_padidx = 0;
		tag_info->hw_scen = 0;
		tag_info->tag_order = MTKCAM_IPI_ORDER_FIRST_TAG;
		/* camsv todo: stride may not be necessary */
		tag_info->stride = 0;
		/*
		 * camsv todo: check that all params in
		 * cfg_in_param is covered when next dev_config
		 */
		atomic_set(&tag_info->is_config_done, 0);
		atomic_set(&tag_info->is_stream_on, 0);
	}
}

void mtk_cam_sensor_switch_stop_reinit_hw(struct mtk_cam_ctx *ctx,
					  struct mtk_cam_request_stream_data *s_data,
					  int stream_id)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request *req;
	struct mtk_cam_req_raw_pipe_data *s_raw_pipe_data;
	struct mtk_raw_device *raw_dev;
	struct mtk_mraw_device *mraw_dev;
	int i;
	int sof_count;
#ifdef MTK_CAM_HSF_SUPPORT
	int ret;
#endif
	struct mtk_cam_scen *scen_first_req;

	req = mtk_cam_s_data_get_req(s_data);
	s_raw_pipe_data = mtk_cam_s_data_get_raw_pipe_data(s_data);
	if (!s_raw_pipe_data) {
		dev_info(cam->dev, "%s: failed to get raw_pipe_data (pipe:%d, seq:%d)\n",
			 __func__, s_data->pipe_id, s_data->frame_seq_no);
		return;
	}

	scen_first_req = s_data->feature.scen;

	dev_info(ctx->cam->dev,
		 "%s:%s:pipe(%d): switch op seq(%d), flags(0x%x), ctx_link_update(0x%x), stream_id(%d), scen(%d)\n",
		 __func__, req->req.debug_str, s_data->pipe_id, s_data->frame_seq_no,
		 req->flags, req->ctx_link_update, stream_id,
		 scen_first_req->id);

	/* stop the camsv */
	mtk_cam_sv_dev_stream_on(ctx, 0);

	/* stop the raw */
	if (ctx->used_raw_num) {
#ifdef MTK_CAM_HSF_SUPPORT
		if (mtk_cam_is_hsf(ctx)) {
			ret = mtk_cam_hsf_uninit(ctx);
			if (ret != 0)
				dev_info(cam->dev,
					 "failed to stream off %s:%d mtk_cam_hsf_uninit fail\n",
					 ctx->seninf->name, ret);
		}
#endif
		raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
		if (mtk_cam_scen_is_mstream_2exp_types(scen_first_req)) {
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
		mtk_ctx_watchdog_stop(ctx, raw_dev->id + MTKCAM_SUBDEV_RAW_START);
		mtk_cam_seninf_set_camtg(s_data->seninf_old, PAD_SRC_RAW0, 0xFF);
		mtk_cam_seninf_set_camtg(s_data->seninf_old, PAD_SRC_RAW1, 0xFF);
		mtk_cam_seninf_set_camtg(s_data->seninf_old, PAD_SRC_RAW2, 0xFF);
		immediate_stream_off(raw_dev);
		/* Twin */
		if (ctx->pipe->res_config.raw_num_used != 1) {
			struct mtk_raw_device *raw_dev_slave =
						get_slave_raw_dev(cam, ctx->pipe);
			if (raw_dev_slave)
				stream_on(raw_dev_slave, 0);
			if (ctx->pipe->res_config.raw_num_used == 3) {
				struct mtk_raw_device *raw_dev_slave2 =
					get_slave2_raw_dev(cam, ctx->pipe);
				if (raw_dev_slave2)
					stream_on(raw_dev_slave2, 0);
			}
		}
	}

	if (ctx->sv_dev) {
		for (i = SVTAG_START; i < SVTAG_END; i++) {
			if (ctx->sv_dev->enabled_tags & (1 << i)) {
				if (i >= SVTAG_META_START && i < SVTAG_META_END)
					mtk_ctx_watchdog_stop(ctx,
						ctx->sv_dev->tag_info[i].sv_pipe->id);
				mtk_cam_seninf_set_camtg_camsv(s_data->seninf_old,
					ctx->sv_dev->tag_info[i].seninf_padidx, 0xFF, i);
			}
		}
	}

	for (i = 0 ; i < ctx->used_mraw_num ; i++) {
		mraw_dev = get_mraw_dev(cam, ctx->mraw_pipe[i]);
		mtk_ctx_watchdog_stop(ctx, ctx->mraw_pipe[i]->id);
		mtk_cam_mraw_vf_on(mraw_dev, 0);
		atomic_set(&mraw_dev->is_enqueued, 0);
		mtk_cam_seninf_set_camtg(s_data->seninf_old,
			ctx->mraw_pipe[i]->seninf_padidx, 0xFF);
	}

	/* apply sensor setting if needed */
	mtk_cam_set_sensor_full(s_data, &ctx->sensor_ctrl);

	/* keep the sof_count to restore it after reinit */
	sof_count = raw_dev->sof_count;

	/* reinitialized the settings */
	if (ctx->used_raw_num) {
		if (!mtk_cam_is_hsf(ctx)) {
			initialize(raw_dev, 0);
			raw_dev->sof_count = sof_count;
			/* Stagger */
			if (mtk_cam_scen_is_sensor_stagger(scen_first_req)) {
				if (scen_first_req->scen.normal.exp_num != 1)
					stagger_enable(raw_dev);
				else
					stagger_disable(raw_dev);
			}
			/* Sub sample */
			if (mtk_cam_scen_is_subsample(scen_first_req))
				subsample_enable(raw_dev);
			/* Twin */
			if (ctx->pipe->res_config.raw_num_used != 1) {
				struct mtk_raw_device *raw_dev_slave =
				get_slave_raw_dev(cam, ctx->pipe);
				if (raw_dev_slave)
					initialize(raw_dev_slave, 1);
				if (ctx->pipe->res_config.raw_num_used == 3) {
					struct mtk_raw_device *raw_dev_slave2 =
						get_slave2_raw_dev(cam, ctx->pipe);
					if (raw_dev_slave2)
						initialize(raw_dev_slave2, 1);
				}
			}
		}
	}

	if (ctx->sv_dev) {
		ctx->sv_dev = mtk_cam_get_used_sv_dev(ctx);
		mtk_cam_sv_pipeline_config(ctx->sv_dev);
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

	if (mtk_cam_is_immediate_switch_req(req, stream_id))
		mtk_cam_sensor_switch_stop_reinit_hw(ctx, s_data, stream_id);
}

void mtk_cam_dev_req_enqueue(struct mtk_cam_device *cam,
			     struct mtk_cam_request *req)
{
	unsigned int i, j;
	struct mtk_cam_scen *scen;
	unsigned long flags;

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
			bool immediate_switch_sensor = false;
			bool switch_sensor = false;

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
			/* state handling first*/
			req_stream_data = mtk_cam_req_get_s_data(req, stream_id, 0);
			scen = req_stream_data->feature.scen;
			if (mtk_cam_ctx_has_raw(ctx) && mtk_cam_scen_is_time_shared(scen)) {
				req_stream_data->frame_params.raw_param.hardware_scenario =
						MTKCAM_IPI_HW_PATH_OFFLINE;
				req_stream_data->frame_params.raw_param.exposure_num = 1;
				req_stream_data->state.estate = E_STATE_TS_READY;
			}
			if (mtk_cam_ctx_has_raw(ctx) && mtk_cam_scen_is_ext_isp(scen))
				req_stream_data->state.estate = E_STATE_EXTISP_READY;
			/*sensor setting after request drained check*/
			if (ctx->used_raw_num) {
				req_stream_data = mtk_cam_req_get_s_data(req, stream_id, 0);
				scen = req_stream_data->feature.scen;
				spin_lock_irqsave(&sensor_ctrl->drained_check_lock, flags);
				drained_seq_no = atomic_read(&sensor_ctrl->last_drained_seq_no);
				dev_dbg(cam->dev, "%s: feature s_data(%d) scen(%s)\n",
					__func__, req_stream_data->frame_seq_no,
					scen->dbg_str);
				/* TBC: Ryan, should we use scen_active? */
				if (mtk_cam_scen_is_sensor_normal(scen) ||
				    (mtk_cam_scen_is_mstream(scen) &&
				     scen->scen.mstream.type == MTK_CAM_MSTREAM_1_EXPOSURE) ||
				     mtk_cam_scen_is_ext_isp(scen)) {
					/* check if deadline timer drained ever triggered */
					/* should exclude sensor set in below <= second request */
					if (atomic_read(&sensor_ctrl->sensor_enq_seq_no) ==
						drained_seq_no && drained_seq_no > 2)
						mtk_cam_try_set_sensor_at_enque(req_stream_data);
				} else if (mtk_cam_scen_is_mstream(scen) &&
					   scen->scen.mstream.type !=
					   MTK_CAM_MSTREAM_1_EXPOSURE) {
					req_stream_data = mtk_cam_req_get_s_data(req, stream_id, 1);
					if (req_stream_data->frame_seq_no ==
							drained_seq_no && drained_seq_no > 2)
						mtk_cam_submit_kwork_in_sensorctrl(
							sensor_ctrl->sensorsetting_wq,
							sensor_ctrl);
				}
				spin_unlock_irqrestore(&sensor_ctrl->drained_check_lock, flags);
			}
			immediate_switch_sensor = mtk_cam_is_immediate_switch_req(req, stream_id);
			switch_sensor = mtk_cam_is_raw_switch_req(req, stream_id);
			/**
			 * initial frame condition:
			 * 1. must not immediate sensor link switch
			 * 2. frame seq no = 1 or
			 * 3. mstream frame seq no = 2
			 */
			if (req_stream_data->frame_seq_no == 1 ||
			    (mtk_cam_scen_is_mstream_2exp_types(scen) &&
			    req_stream_data->frame_seq_no == 2))
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
			if (!switch_sensor &&
			    ctx->sensor && (initial_frame || mtk_cam_scen_is_m2m(scen))) {
				if (mtk_cam_ctx_has_raw(ctx) &&
				    mtk_cam_scen_is_mstream_2exp_types(scen)) {
					mtk_cam_mstream_initial_sensor_setup(req, ctx);
					ctx->next_sof_frame_seq_no = 1;
				} else {
					mtk_cam_initial_sensor_setup(req, ctx);
				}
			} else if (!ctx->sensor && mtk_cam_ctx_has_raw(ctx) &&
				   mtk_cam_scen_is_pure_m2m(scen)) {
				mtk_cam_initial_sensor_setup(req, ctx);
			}
			if (ctx->used_raw_num != 0) {
				if (!switch_sensor &&
				    ctx->sensor && MTK_CAM_INITIAL_REQ_SYNC == 0 &&
					(mtk_cam_scen_is_sensor_normal(scen) ||
					 req_stream_data->frame_params.raw_param.hardware_scenario
					 == MTKCAM_IPI_HW_PATH_DC_STAGGER) &&
					req_stream_data->frame_seq_no == 2) {
					mtk_cam_initial_sensor_setup(req, ctx);
				}
			} else { // for single sv pipe stream
				if (!switch_sensor &&
				    ctx->sensor && MTK_CAM_INITIAL_REQ_SYNC == 0 &&
					req_stream_data->frame_seq_no == 2) {
					mtk_cam_initial_sensor_setup(req, ctx);
				}
			}

			if (mtk_cam_scen_is_mstream(scen) &&
			    scen->scen.mstream.type != MTK_CAM_MSTREAM_1_EXPOSURE) {
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
			if (mtk_cam_scen_is_mstream_2exp_types(scen)) {
				int frame_cnt;

				for (frame_cnt = 1; frame_cnt <= MTKCAM_MSTREAM_MAX;
						frame_cnt++) {
					if (frame_cnt == 1) { // first exposure
						dev_dbg(cam->dev, "%s: mstream 1st exp frame\n",
							__func__);
						req_stream_data =
							mtk_cam_req_get_s_data(req,
							stream_id, 1);
					} else { // second exposure
						dev_dbg(cam->dev, "%s: mstream 2nd exp frame\n",
							__func__);
						req_stream_data =
							mtk_cam_req_get_s_data(req,
							stream_id, 0);
					}
					frame_work = &req_stream_data->frame_work;
					mtk_cam_req_dump_work_init(req_stream_data);
					INIT_WORK(&frame_work->work,
						isp_tx_frame_worker);
					queue_work(ctx->composer_wq,
						&frame_work->work);
				}
			} else {
				/* go on here */
				mtk_cam_req_dump_work_init(req_stream_data);
				if (is_raw_subdev(stream_id))
					INIT_WORK(&frame_work->work, isp_tx_frame_worker);
				else
					INIT_WORK(&frame_work->work, isp_tx_sv_frame_worker);
				queue_work(ctx->composer_wq, &frame_work->work);
			}

			if (watchdog_scenario(ctx) &&
			    initial_frame &&
			    !immediate_switch_sensor) {
				mtk_ctx_watchdog_start(ctx, 4,
					get_master_raw_id(cam->num_raw_drivers,
					ctx->pipe->enabled_raw));
				for (j = 0; j < ctx->used_sv_num; j++) {
					struct v4l2_format img_fmt =
						ctx->sv_pipe[j]->vdev_nodes[
							MTK_CAMSV_MAIN_STREAM_OUT -
							MTK_CAMSV_SINK_NUM].active_fmt;

					if (img_fmt.fmt.pix_mp.width == IMG_MIN_WIDTH &&
						img_fmt.fmt.pix_mp.height == IMG_MIN_HEIGHT) {
						dev_info(cam->dev, "%s:ctx/pipe_id(%d/%d): skip watchdog_start for no config size\n",
							__func__, stream_id, ctx->sv_pipe[j]->id);
						continue;
					}
					mtk_ctx_watchdog_start(ctx, 4, ctx->sv_pipe[j]->id);
				}
				for (j = 0; j < ctx->used_mraw_num; j++) {
					if (ctx->mraw_pipe[j]->res_config.tg_crop.s.w == 0 &&
						ctx->mraw_pipe[j]->res_config.tg_crop.s.h == 0) {
						dev_info(cam->dev, "%s:ctx/pipe_id(%d/%d): skip watchdog_start for no config size\n",
							__func__, stream_id, ctx->mraw_pipe[j]->id);
						continue;
					}
					mtk_ctx_watchdog_start(ctx, 4, ctx->mraw_pipe[j]->id);
				}
			}

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
	if (pipe_id >= MTKCAM_SUBDEV_RAW_END)
		return NULL;
	else
		return &cam->raw.pipelines[pipe_id - MTKCAM_SUBDEV_RAW_0];
}

struct mtk_camsv_pipeline*
mtk_cam_dev_get_sv_pipeline(struct mtk_cam_device *cam,
			     unsigned int pipe_id)
{
	if (pipe_id < MTKCAM_SUBDEV_CAMSV_START || pipe_id >= MTKCAM_SUBDEV_CAMSV_END)
		return NULL;
	else
		return &cam->sv.pipelines[pipe_id - MTKCAM_SUBDEV_CAMSV_START];
}

static int
mtk_cam_raw_pipeline_config(struct mtk_cam_ctx *ctx,
			    struct mtkcam_ipi_input_param *cfg_in_param)
{
	struct mtk_raw_pipeline *pipe = ctx->pipe;
	struct mtk_raw *raw = pipe->raw;
	int i;
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
	if (ctx->seninf)
		dev_info(raw->cam_dev, "ctx_id %d seninf:%s used_raw_dev 0x%x pipe_id %d\n",
			ctx->stream_id, ctx->seninf->name, ctx->used_raw_dev, pipe->id);
	else
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
	if (ctx->seninf)
		dev_info(raw->cam_dev, "ctx_id %d seninf:%s used_raw_dev 0x%x pipe_id %d\n",
			ctx->stream_id, ctx->seninf->name, s_raw_pipe_data->enabled_raw, pipe->id);
	else
		dev_info(raw->cam_dev, "ctx_id %d used_raw_dev 0x%x pipe_id %d\n",
			ctx->stream_id, s_raw_pipe_data->enabled_raw, pipe->id);

	return 0;
}

void mtk_cam_apply_pending_dev_config(struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_req_raw_pipe_data *s_raw_pipe_data;
	struct mtk_cam_ctx *ctx;
	struct mtk_camsys_sensor_ctrl *camsys_sensor_ctrl;
	char *debug_str = mtk_cam_s_data_get_dbg_str(s_data);

	ctx = mtk_cam_s_data_get_ctx(s_data);
	ctx->pipe->scen_active = ctx->pipe->user_res.raw_res.scen;
	camsys_sensor_ctrl = &ctx->sensor_ctrl;

	s_raw_pipe_data = mtk_cam_s_data_get_raw_pipe_data(s_data);
	if (!s_raw_pipe_data) {
		dev_info(ctx->cam->dev, "%s: failed to get raw_pipe_data (pipe:%d, seq:%d)\n",
			 __func__, s_data->pipe_id, s_data->frame_seq_no);
		return;
	}

	ctx->pipe->hw_mode = ctx->pipe->hw_mode_pending;
	ctx->pipe->enabled_raw = s_raw_pipe_data->enabled_raw;
	ctx->used_raw_dev = s_raw_pipe_data->enabled_raw;
	atomic_set(&camsys_sensor_ctrl->reset_seq_no, s_data->frame_seq_no);

	ctx->sv_dev->enabled_tags = s_raw_pipe_data->enabled_sv_tags;
	ctx->sv_dev->used_tag_cnt = s_raw_pipe_data->used_tag_cnt;
	memcpy(ctx->sv_dev->tag_info, s_raw_pipe_data->tag_info,
		sizeof(struct mtk_camsv_tag_info) * MAX_SV_HW_TAGS);

	dev_info(ctx->cam->dev,
		"%s:%s:pipe(%d):seq(%d):scen_active(%d), hw_mode(0x%x), enabled_raw(0x%x)\n",
		__func__, debug_str, ctx->stream_id, s_data->frame_seq_no,
		ctx->pipe->scen_active.id,
		ctx->pipe->hw_mode, ctx->pipe->enabled_raw);
}

unsigned int mtk_cam_get_hdr_seninf_padidx(
	int hw_scen, int exp_no, int tag_idx)
{
	unsigned int seninf_padidx = PAD_SRC_RAW0;

	if (hw_scen & (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_STAGGER)) ||
		hw_scen & (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_DC_STAGGER)) ||
		hw_scen & (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER))) {
		if (exp_no == 1) {
			if (tag_idx == SVTAG_2)
				seninf_padidx = PAD_SRC_RAW0;
			else if (tag_idx == SVTAG_3)
				seninf_padidx = PAD_SRC_RAW_W0;
		} else if (exp_no == 2) {
			if (tag_idx == SVTAG_0)
				seninf_padidx = PAD_SRC_RAW0;
			else if (tag_idx == SVTAG_1)
				seninf_padidx = PAD_SRC_RAW_W0;
			else if (tag_idx == SVTAG_2)
				seninf_padidx = PAD_SRC_RAW1;
			else if (tag_idx == SVTAG_3)
				seninf_padidx = PAD_SRC_RAW_W1;
		} else if (exp_no == 3) {
			if (tag_idx == SVTAG_0)
				seninf_padidx = PAD_SRC_RAW0;
			else if (tag_idx == SVTAG_1)
				seninf_padidx = PAD_SRC_RAW1;
			else if (tag_idx == SVTAG_2)
				seninf_padidx = PAD_SRC_RAW2;
		}
	}

	return seninf_padidx;
}

int mtk_cam_sv_hdr_tag_update(struct mtk_cam_ctx *ctx,
	struct mtk_camsv_tag_info *arr_tag, const unsigned int enabled_tags,
	unsigned int hw_scen, unsigned int exp_no, unsigned int sub_ratio,
	struct v4l2_mbus_framefmt *mf, struct v4l2_format *img_fmt)
{
	int i;
	unsigned int seninf_padidx, tag_order;

	for (i = SVTAG_IMG_START; i < SVTAG_IMG_END; i++) {
		if (enabled_tags & (1 << i)) {
			tag_order =
				mtk_cam_get_sv_mapped_tag_order(hw_scen, exp_no, i);
			seninf_padidx =
				mtk_cam_get_hdr_seninf_padidx(hw_scen, exp_no, i);
			mtk_cam_call_sv_pipeline_config(ctx, arr_tag, i, seninf_padidx,
				hw_scen, tag_order, 3, sub_ratio, NULL, mf, img_fmt);
		}
	}

	return 0;
}

int mtk_cam_sv_extisp_tag_update(struct mtk_cam_ctx *ctx,
	struct mtk_camsv_tag_info *arr_tag, unsigned int enabled_tags,
	unsigned int hw_scen, unsigned int exp_no, unsigned int sub_ratio,
	struct v4l2_mbus_framefmt *mf)
{
	int i;
	unsigned int seninf_padidx, tag_order;
	struct v4l2_format *img_fmt;
	struct v4l2_mbus_framefmt mbus_fmt = *mf;

	for (i = SVTAG_IMG_START; i < SVTAG_IMG_END; i++) {
		if (enabled_tags & (1 << i)) {
			tag_order =
				mtk_cam_get_sv_mapped_tag_order(hw_scen, exp_no, i);
			if (i == SVTAG_0) {
				seninf_padidx = PAD_SRC_RAW0;
				img_fmt = &ctx->pipe->vdev_nodes[
					MTK_RAW_MAIN_STREAM_SV_1_OUT - MTK_RAW_SINK_NUM].active_fmt;
			} else if (i == SVTAG_1) {
				seninf_padidx = PAD_SRC_RAW1;
				img_fmt = &ctx->pipe->vdev_nodes[
					MTK_RAW_MAIN_STREAM_SV_2_OUT - MTK_RAW_SINK_NUM].active_fmt;
			} else if (i == SVTAG_3) {
				seninf_padidx = PAD_SRC_GENERAL0;
				img_fmt = &ctx->pipe->vdev_nodes[
					MTK_RAW_META_SV_OUT_0 - MTK_RAW_SINK_NUM].active_fmt;
				mbus_fmt.code = MEDIA_BUS_FMT_SBGGR8_1X8;
			} else {
				dev_info(ctx->cam->dev, "%s: illegal tag_idx(%d) used for extisp\n",
					 __func__, i);
				return -EINVAL;
			}
			mtk_cam_call_sv_pipeline_config(ctx, arr_tag, i, seninf_padidx,
				hw_scen, tag_order, 3, sub_ratio, NULL, &mbus_fmt, img_fmt);
		}
	}

	return 0;
}

int mtk_cam_s_data_dev_config(struct mtk_cam_request_stream_data *s_data,
	bool streaming, bool config_pipe)
{
	struct mtk_cam_request *req;
	struct mtk_cam_req_raw_pipe_data *s_raw_pipe_data;
	struct mtk_cam_resource_config *res_config;
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
	unsigned int i, j;
	int ret;
	u32 mf_code;
	struct mtk_cam_scen *scen;

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

	scen = &s_raw_pipe_data->res.raw_res.scen;
	res_config = &s_raw_pipe_data->res_config;

	memset(&config_param, 0, sizeof(config_param));

	/* Update cfg_in_param */
	cfg_in_param = &config_param.input;
	cfg_in_param->pixel_mode = res_config->tgo_pxl_mode;
	cfg_in_param->pixel_mode_before_raw =
		res_config->tgo_pxl_mode_before_raw;
	cfg_in_param->subsample =
		mtk_cam_raw_get_subsample_ratio(scen);
	/* TODO: data pattern from meta buffer per frame setting */
	cfg_in_param->data_pattern = MTKCAM_IPI_SENSOR_PATTERN_NORMAL;
	img_fmt = &pipe->img_fmt_sink_pad;
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
	if (config_pipe && mtk_cam_scen_is_sensor_stagger(scen)) {
		ret = mtk_cam_s_data_raw_pipeline_config(s_data, cfg_in_param);
		if (ret)
			return ret;
	}

	config_param.flags = MTK_CAM_IPI_CONFIG_TYPE_INPUT_CHANGE;

	dev_dbg(dev, "%s: config_param flag:0x%x enabled_raw:0x%x\n", __func__,
			config_param.flags, s_raw_pipe_data->enabled_raw);

	/* reset tag info. and must be after raw selected done */
	ctx->sv_dev = mtk_cam_get_used_sv_dev(ctx);
	if (!ctx->sv_dev) {
		dev_info(dev, "%s: get sv_dev failed\n", __func__);
		return -EINVAL;
	}
	s_raw_pipe_data->used_tag_cnt = 0;
	s_raw_pipe_data->enabled_sv_tags = 0;
	mtk_cam_sv_reset_tag_info(s_raw_pipe_data->tag_info);

	if (config_pipe && mtk_cam_scen_is_sensor_stagger(scen)) {
		int hw_scen, exp_no, req_amount, idle_tags;
		bool is_rgbw = mtk_cam_scen_is_rgbw_enabled(scen);

		if (s_raw_pipe_data->stagger_select.hw_mode == HW_MODE_ON_THE_FLY) {
			hw_scen = (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_STAGGER));
			exp_no = (mtk_cam_scen_is_3_exp(scen)) ? 3 : 2;
			req_amount = (mtk_cam_scen_is_3_exp(scen)) ? 2 : 1;
#if PURE_RAW_WITH_SV_VHDR
			if (!is_rgbw) {
				req_amount = req_amount + 1;
				ctx->pure_raw_sv_tag_idx = SVTAG_2;
			}
#endif
		} else if (s_raw_pipe_data->stagger_select.hw_mode == HW_MODE_DIRECT_COUPLED) {
			hw_scen = (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_DC_STAGGER));
			exp_no = (mtk_cam_scen_is_3_exp(scen)) ? 3 : 2;
			req_amount = (mtk_cam_scen_is_3_exp(scen)) ? 3 : 2;
		} else if (s_raw_pipe_data->stagger_select.hw_mode == HW_MODE_OFFLINE) {
			hw_scen = (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER));
			exp_no = (mtk_cam_scen_is_3_exp(scen)) ? 3 : 2;
			req_amount = (mtk_cam_scen_is_3_exp(scen)) ? 3 : 2;
		} else {
			hw_scen = (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_STAGGER));
			exp_no = (mtk_cam_scen_is_3_exp(scen)) ? 3 : 2;
			req_amount = (mtk_cam_scen_is_3_exp(scen)) ? 2 : 1;
#if PURE_RAW_WITH_SV_VHDR
			if (!is_rgbw) {
				req_amount = req_amount + 1;
				ctx->pure_raw_sv_tag_idx = SVTAG_2;
			}
#endif
		}
		if (exp_no > 2 && is_rgbw == true) {
			dev_info(cam->dev, "exposure number(%d) not allowed under rgbw case",
				exp_no);
			return -EINVAL;
		}

		idle_tags = mtk_cam_get_sv_idle_tags(ctx,
			s_raw_pipe_data->enabled_sv_tags,
			hw_scen, exp_no, req_amount, true, is_rgbw);
		if (idle_tags == 0) {
#if PURE_RAW_WITH_SV_VHDR
			ctx->pure_raw_sv_tag_idx = -1;
#endif
			dev_info(cam->dev, "no available sv tags(scen:%d/req_amount:%d)",
				hw_scen, req_amount);
			return -EINVAL;
		}
		req_amount *= (is_rgbw) ? 2 : 1;

		mtk_cam_sv_hdr_tag_update(ctx, s_raw_pipe_data->tag_info, idle_tags,
			hw_scen, exp_no, cfg_in_param->subsample, mf, img_fmt);
		s_raw_pipe_data->used_tag_cnt += req_amount;
		s_raw_pipe_data->enabled_sv_tags |= idle_tags;
	} else if (config_pipe && mtk_cam_hw_is_dc(ctx)) {
		int hw_scen, exp_no, req_amount, idle_tags;
		bool is_rgbw;

		hw_scen = (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_DC_STAGGER));
		exp_no = 1;
		req_amount = 1;
		is_rgbw = mtk_cam_scen_is_rgbw_enabled(scen);

		idle_tags = mtk_cam_get_sv_idle_tags(ctx,
			s_raw_pipe_data->enabled_sv_tags,
			hw_scen, exp_no, req_amount, true, is_rgbw);
		if (idle_tags == 0) {
			dev_info(cam->dev, "no available sv tags(scen:%d/req_amount:%d)",
				hw_scen, req_amount);
			return -EINVAL;
		}
		req_amount *= (is_rgbw) ? 2 : 1;

		mtk_cam_sv_hdr_tag_update(ctx, s_raw_pipe_data->tag_info, idle_tags,
			hw_scen, exp_no, cfg_in_param->subsample, mf, img_fmt);
		s_raw_pipe_data->used_tag_cnt += req_amount;
		s_raw_pipe_data->enabled_sv_tags |= idle_tags;
	} else if (config_pipe && !mtk_cam_scen_is_mstream(scen) &&
		!mtk_cam_scen_is_mstream_m2m(scen) && !mtk_cam_scen_is_subsample(scen) &&
		!mtk_cam_is_hsf(ctx)) {
#if PURE_RAW_WITH_SV
		unsigned int hw_scen, exp_no, req_amount;
		unsigned int idle_tags, seninf_padidx, tag_order;

		hw_scen = (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_ON_THE_FLY));
		exp_no = 1;
		req_amount = 1;
		idle_tags = mtk_cam_get_sv_idle_tags(ctx,
			s_raw_pipe_data->enabled_sv_tags,
			hw_scen, exp_no, req_amount, true, false);
		if (idle_tags == 0) {
			dev_info(cam->dev, "no available sv tags(scen:%d/req_amount:%d)",
				hw_scen, req_amount);
			return -EINVAL;
		}

		ctx->pure_raw_sv_tag_idx = SVTAG_2;

		tag_order = MTKCAM_IPI_ORDER_FIRST_TAG;
		seninf_padidx = PAD_SRC_RAW0;
		mtk_cam_call_sv_pipeline_config(ctx, s_raw_pipe_data->tag_info, SVTAG_2,
			seninf_padidx, hw_scen, tag_order, 3,
			cfg_in_param->subsample, NULL, mf, img_fmt);
		s_raw_pipe_data->used_tag_cnt += req_amount;
		s_raw_pipe_data->enabled_sv_tags |= idle_tags;
#endif
	}

	/* camsv's meta config */
	for (i = 0; i < ctx->used_sv_num; i++) {
		unsigned int idle_tags, tag_idx = SVTAG_META_START;
		unsigned int sv_cammux_id;

		idle_tags = mtk_cam_get_sv_idle_tags(ctx,
			s_raw_pipe_data->enabled_sv_tags, 0, 1, 1, true, false);
		if (idle_tags == 0) {
			dev_info(cam->dev, "no available sv tags for meta use");
			return -EINVAL;
		}
		for (j = SVTAG_META_START; j < SVTAG_META_END; j++) {
			if (idle_tags & (1 << j)) {
				tag_idx = j;
				break;
			}
		}

		sv_cammux_id = mtk_cam_get_sv_cammux_id(ctx->sv_dev, tag_idx);
		mf = &ctx->sv_pipe[i]->cfg[MTK_CAMSV_SINK].mbus_fmt;
		img_fmt = &ctx->sv_pipe[i]->vdev_nodes[
			MTK_CAMSV_MAIN_STREAM_OUT - MTK_CAMSV_SINK_NUM].active_fmt;
		if (req->ctx_link_update & 1 << ctx->stream_id)
			mtk_cam_call_sv_pipeline_config(ctx, s_raw_pipe_data->tag_info,
				tag_idx, ctx->sv_pipe[i]->seninf_padidx, 1,
				mtk_cam_seninf_get_tag_order(s_data->seninf_new,
				ctx->sv_pipe[i]->seninf_padidx),
				3, cfg_in_param->subsample,
				ctx->sv_pipe[i], mf, img_fmt);
		else
			mtk_cam_call_sv_pipeline_config(ctx, s_raw_pipe_data->tag_info,
				tag_idx, ctx->sv_pipe[i]->seninf_padidx, 1,
				mtk_cam_seninf_get_tag_order(ctx->seninf,
				ctx->sv_pipe[i]->seninf_padidx),
				3, cfg_in_param->subsample,
				ctx->sv_pipe[i], mf, img_fmt);

		s_raw_pipe_data->enabled_sv_tags |= (1 << tag_idx);
		s_raw_pipe_data->used_tag_cnt++;
	}

	/* camsv's config param update */
	for (i = SVTAG_START; i < SVTAG_END; i++) {
		if (s_raw_pipe_data->enabled_sv_tags & (1 << i)) {
			/* camsv todo: ais */
			config_param.sv_input[0][i].pipe_id =
				ctx->sv_dev->id + MTKCAM_SUBDEV_CAMSV_START;
			config_param.sv_input[0][i].tag_id = i;
			config_param.sv_input[0][i].tag_order =
				s_raw_pipe_data->tag_info[i].tag_order;
			config_param.sv_input[0][i].is_first_frame =
				(req->ctx_link_update & 1 << ctx->stream_id) ? 1 : 0;
			config_param.sv_input[0][i].input =
				s_raw_pipe_data->tag_info[i].cfg_in_param;
		}
	}

	/* mraw's config param update */
	for (i = 0; i < ctx->used_mraw_num; i++) {
		config_param.mraw_input[i].pipe_id =
			ctx->mraw_pipe[i]->id;
		config_param.mraw_input[i].input.fmt =
			ctx->mraw_pipe[i]->res_config.tg_fmt;
		config_param.mraw_input[i].input.raw_pixel_id = 0;
		config_param.mraw_input[i].input.data_pattern = 0;
		config_param.mraw_input[i].input.pixel_mode = 3;
		config_param.mraw_input[i].input.pixel_mode_before_raw = 3;
		config_param.mraw_input[i].input.subsample =
			cfg_in_param->subsample;
		config_param.mraw_input[i].input.in_crop =
			ctx->mraw_pipe[i]->res_config.tg_crop;
	}

	update_hw_mapping(ctx, &config_param);
	ret = update_scen_param(ctx, &config_param, scen);
	if (ret)
		return ret;

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
				if (raw_dev_slave) {
					raw_dev_slave->pipeline = &raw->pipelines[i];
					dev_dbg(dev, "twin master/slave raw_id:%d/%d\n",
						raw_dev->id, raw_dev_slave->id);
				} else {
					dev_info(dev,
						"%s:master:%d get slave failed\n",
						__func__, raw_dev->id);
				}
				if (raw->pipelines[i].res_config.raw_num_used == 3) {
					struct mtk_raw_device *raw_dev_slave2 =
						get_slave2_raw_dev(cam, ctx->pipe);
					if (raw_dev_slave2) {
						raw_dev_slave2->pipeline = &raw->pipelines[i];
						dev_dbg(dev,
							"triplet m/s2 raw_id:%d/%d\n",
							raw_dev->id,
							raw_dev_slave2->id);
					} else {
						dev_info(dev,
							"%s:master:%d get slave2 failed\n",
							__func__, raw_dev->id);
					}
				}
			}
			break;
		}

	isp_composer_hw_config(cam, ctx, &config_param);
	dev_dbg(dev, "raw %d %s done\n", raw_dev->id, __func__);

	return 0;
}

unsigned int mtk_cam_get_sv_mapped_tag_order(
	int hw_scen, int exp_no, int tag_idx)
{
	unsigned int tag_order = MTKCAM_IPI_ORDER_NORMAL_TAG;

	/* camsv todo: refine it */

	if (hw_scen & (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_STAGGER)) ||
		hw_scen & (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_DC_STAGGER)) ||
		hw_scen & (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER))) {
		if (exp_no == 1) {
			if (tag_idx == SVTAG_2 || tag_idx == SVTAG_3)
				tag_order = MTKCAM_IPI_ORDER_FIRST_TAG;
		} else if (exp_no == 2) {
			if (tag_idx == SVTAG_0 || tag_idx == SVTAG_1)
				tag_order = MTKCAM_IPI_ORDER_FIRST_TAG;
			else if (tag_idx == SVTAG_2 || tag_idx == SVTAG_3)
				tag_order = MTKCAM_IPI_ORDER_LAST_TAG;
		} else if (exp_no == 3) {
			if (tag_idx == SVTAG_0)
				tag_order = MTKCAM_IPI_ORDER_FIRST_TAG;
			else if (tag_idx == SVTAG_1)
				tag_order = MTKCAM_IPI_ORDER_NORMAL_TAG;
			else if (tag_idx == SVTAG_2)
				tag_order = MTKCAM_IPI_ORDER_LAST_TAG;
		}
	} else if (hw_scen & (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_OFFLINE))) {
		if (tag_idx == SVTAG_2)
			tag_order = MTKCAM_IPI_ORDER_FIRST_TAG;
	} else if (hw_scen & (1 << MTKCAM_SV_SPECIAL_SCENARIO_ADDITIONAL_RAW)) {
		tag_order = MTKCAM_IPI_ORDER_FIRST_TAG;
	} else if (hw_scen & (1 << MTKCAM_SV_SPECIAL_SCENARIO_EXT_ISP)) {
		if (tag_idx == SVTAG_0 || tag_idx == SVTAG_1 || tag_idx == SVTAG_3)
			tag_order = MTKCAM_IPI_ORDER_FIRST_TAG;
	}

	return tag_order;
}

int mtk_cam_sv_dev_config(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtkcam_ipi_config_param config_param;
	struct v4l2_mbus_framefmt *mf;
	struct v4l2_format img_fmt;
	int i;
	unsigned int idle_tags;
	unsigned int sv_cammux_id, hw_scen, req_amount, seninf_padidx;

	if (ctx->used_sv_num != 1) {
		dev_info(cam->dev, "%s un-expected used camsv number(%d)",
			__func__, ctx->used_sv_num);
		return -EINVAL;
	}

	memset(&config_param, 0, sizeof(config_param));
	ctx->sv_dev = mtk_cam_get_used_sv_dev(ctx);
	if (!ctx->sv_dev) {
		dev_info(cam->dev, "%s sv_dev is NULL", __func__);
		return -EINVAL;
	}
	ctx->sv_dev->used_tag_cnt = 0;
	ctx->sv_dev->enabled_tags = 0;
	mtk_cam_sv_reset_tag_info(ctx->sv_dev->tag_info);

	if (mtk_cam_is_display_ic(ctx)) {
		hw_scen = (1 << MTKCAM_SV_SPECIAL_SCENARIO_DISPLAY_IC);
		req_amount = 3;
	} else {
		hw_scen = (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_ON_THE_FLY));
		req_amount = 1;
	}
	idle_tags =
		mtk_cam_get_sv_idle_tags(ctx,
			ctx->sv_dev->enabled_tags, hw_scen, 1, req_amount, true, false);
	if (idle_tags == 0) {
		dev_info(cam->dev, "no available sv tags for use");
		return -EINVAL;
	}
	ctx->sv_dev->enabled_tags |= idle_tags;
	ctx->sv_dev->used_tag_cnt += req_amount;

	/* camsv's device config */
	for (i = SVTAG_START; i < SVTAG_END; i++) {
		if (!(ctx->sv_dev->enabled_tags & (1 << i)))
			continue;
		sv_cammux_id = mtk_cam_get_sv_cammux_id(ctx->sv_dev, i);
		mf = &ctx->sv_pipe[0]->cfg[MTK_CAMSV_SINK].mbus_fmt;
		if (mtk_cam_is_display_ic(ctx)) {
			if (i == SVTAG_0) {
				img_fmt = ctx->sv_pipe[0]->vdev_nodes[
					MTK_CAMSV_MAIN_STREAM_OUT - MTK_CAMSV_SINK_NUM].active_fmt;
				if (img_fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV21)
					img_fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SBGGR8;
				else
					img_fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SBGGR10;
				seninf_padidx = PAD_SRC_RAW0;
			} else if (i == SVTAG_1) {
				img_fmt = ctx->sv_pipe[0]->vdev_nodes[
					MTK_CAMSV_MAIN_STREAM_OUT - MTK_CAMSV_SINK_NUM].active_fmt;
				img_fmt.fmt.pix_mp.height /= 2;
				if (img_fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV21)
					img_fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SBGGR8;
				else
					img_fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SBGGR10;
				seninf_padidx = PAD_SRC_RAW1;
			} else {
				img_fmt = ctx->sv_pipe[0]->vdev_nodes[
					MTK_CAMSV_EXT_STREAM_OUT - MTK_CAMSV_SINK_NUM].active_fmt;
				seninf_padidx = PAD_SRC_GENERAL0;
			}
		} else {
			img_fmt = ctx->sv_pipe[0]->vdev_nodes[
				MTK_CAMSV_MAIN_STREAM_OUT - MTK_CAMSV_SINK_NUM].active_fmt;
			seninf_padidx = ctx->sv_pipe[0]->seninf_padidx;
		}

		mtk_cam_call_sv_pipeline_config(ctx, ctx->sv_dev->tag_info, i,
			seninf_padidx, 1, MTKCAM_IPI_ORDER_FIRST_TAG,
			3, 0, ctx->sv_pipe[0], mf, &img_fmt);
	}

	/* camsv's config param update */
	for (i = SVTAG_START; i < SVTAG_END; i++) {
		if (ctx->sv_dev->enabled_tags & (1 << i)) {
			/* camsv todo: ais */
			config_param.sv_input[0][i].pipe_id =
				ctx->sv_dev->id + MTKCAM_SUBDEV_CAMSV_START;
			config_param.sv_input[0][i].tag_id = i;
			config_param.sv_input[0][i].tag_order =
				ctx->sv_dev->tag_info[i].tag_order;
			config_param.sv_input[0][i].input =
				ctx->sv_dev->tag_info[i].cfg_in_param;
		}
	}

	isp_composer_hw_config(cam, ctx, &config_param);

	return 0;
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
	unsigned int i, j;
	int ret;
	u32 mf_code;
	struct mtk_cam_scen *scen_active; /* Used to know max exposure num */

	scen_active = &ctx->pipe->scen_active;

	memset(&config_param, 0, sizeof(config_param));

	/* Update cfg_in_param */
	cfg_in_param = &config_param.input;
	cfg_in_param->pixel_mode = ctx->pipe->res_config.tgo_pxl_mode;
	cfg_in_param->pixel_mode_before_raw =
		ctx->pipe->res_config.tgo_pxl_mode_before_raw;
	cfg_in_param->subsample = mtk_cam_raw_get_subsample_ratio(scen_active);
	/* TODO: data pattern from meta buffer per frame setting */
	cfg_in_param->data_pattern = MTKCAM_IPI_SENSOR_PATTERN_NORMAL;
	img_fmt = &pipe->img_fmt_sink_pad;
	cfg_in_param->in_crop.s.w = img_fmt->fmt.pix_mp.width;
	cfg_in_param->in_crop.s.h = img_fmt->fmt.pix_mp.height;

	if (mtk_cam_scen_is_pure_m2m(scen_active)) {
		/* get ctx->pipe.apu_info for config */
		/* init config, need to do something? */
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

	/* reset tag info. and must be after raw selected done */
	ctx->pipe->enabled_sv_tags = 0;
	ctx->sv_dev = mtk_cam_get_used_sv_dev(ctx);
	if (!ctx->sv_dev) {
		dev_info(dev, "%s: get sv_dev failed\n", __func__);
		return -EINVAL;
	}
	ctx->sv_dev->used_tag_cnt = 0;
	ctx->sv_dev->enabled_tags = 0;
	mtk_cam_sv_reset_tag_info(ctx->sv_dev->tag_info);

	if (config_pipe && mtk_cam_scen_is_sensor_stagger(scen_active)) {
		unsigned int hw_scen, exp_no, req_amount, idle_tags;
		bool is_rgbw;

		exp_no = mtk_cam_scen_get_max_exp_num(scen_active);
		is_rgbw = mtk_cam_scen_is_rgbw_enabled(scen_active);
		if (exp_no > 2 && is_rgbw == true) {
			dev_info(cam->dev, "exposure number(%d) not allowed under rgbw case",
				exp_no);
			return -EINVAL;
		}
		if (mtk_cam_hw_is_otf(ctx)) {
			hw_scen = (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_STAGGER));
			req_amount = (exp_no == 3) ? 2 : 1;
#if PURE_RAW_WITH_SV_VHDR
			if (!is_rgbw) {
				req_amount = req_amount + 1;
				ctx->pure_raw_sv_tag_idx = SVTAG_2;
			}
#endif
		} else if (mtk_cam_hw_is_dc(ctx)) {
			hw_scen = (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_DC_STAGGER));
			req_amount = (exp_no == 3) ? 3 : 2;
		} else if (mtk_cam_hw_is_offline(ctx)) {
			hw_scen = (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER));
			req_amount = (exp_no == 3) ? 3 : 2;
		} else {
			hw_scen = (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_STAGGER));
			req_amount = (exp_no == 3) ? 2 : 1;
#if PURE_RAW_WITH_SV_VHDR
			if (!is_rgbw) {
				req_amount = req_amount + 1;
				ctx->pure_raw_sv_tag_idx = SVTAG_2;
			}
#endif
		}
		idle_tags = mtk_cam_get_sv_idle_tags(ctx,
			ctx->sv_dev->enabled_tags, hw_scen, exp_no, req_amount, true, is_rgbw);
		if (idle_tags == 0) {
#if PURE_RAW_WITH_SV_VHDR
			ctx->pure_raw_sv_tag_idx = -1;
#endif
			dev_info(cam->dev, "no available sv tags(scen:%d/req_amount:%d)",
				hw_scen, req_amount);
			return -EINVAL;
		}
		req_amount *= (is_rgbw) ? 2 : 1;

		mtk_cam_sv_hdr_tag_update(ctx, ctx->sv_dev->tag_info, idle_tags,
			hw_scen, exp_no, cfg_in_param->subsample, mf, img_fmt);
		ctx->pipe->enabled_sv_tags |= idle_tags;
		ctx->sv_dev->enabled_tags |= idle_tags;
		ctx->sv_dev->used_tag_cnt += req_amount;
	} else if (config_pipe && mtk_cam_hw_is_dc(ctx)) {
		unsigned int hw_scen, exp_no, req_amount, idle_tags;
		bool is_rgbw;

		hw_scen = (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_DC_STAGGER));
		exp_no = 1;
		req_amount = 1;
		is_rgbw = mtk_cam_scen_is_rgbw_enabled(scen_active);
		idle_tags = mtk_cam_get_sv_idle_tags(ctx,
			ctx->sv_dev->enabled_tags, hw_scen, exp_no, req_amount, true, is_rgbw);
		if (idle_tags == 0) {
			dev_info(cam->dev, "no available sv tags(scen:%d/req_amount:%d)",
				hw_scen, req_amount);
			return -EINVAL;
		}
		req_amount *= (is_rgbw) ? 2 : 1;

		mtk_cam_sv_hdr_tag_update(ctx, ctx->sv_dev->tag_info, idle_tags,
			hw_scen, exp_no, cfg_in_param->subsample, mf, img_fmt);
		ctx->pipe->enabled_sv_tags |= idle_tags;
		ctx->sv_dev->enabled_tags |= idle_tags;
		ctx->sv_dev->used_tag_cnt += req_amount;
	} else if (config_pipe && mtk_cam_scen_is_time_shared(scen_active)) {
		unsigned int hw_scen, exp_no, req_amount;
		unsigned int idle_tags, seninf_padidx, tag_order;

		hw_scen = (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_OFFLINE));
		exp_no = 1;
		req_amount = 1;
		idle_tags = mtk_cam_get_sv_idle_tags(ctx,
			ctx->sv_dev->enabled_tags, hw_scen, exp_no, req_amount, true, false);
		if (idle_tags == 0) {
			dev_info(cam->dev, "no available sv tags(scen:%d/req_amount:%d)",
				hw_scen, req_amount);
			return -EINVAL;
		}

		for (i = SVTAG_IMG_START; i < SVTAG_IMG_END; i++) {
			if (idle_tags & (1 << i)) {
				tag_order = mtk_cam_get_sv_mapped_tag_order(hw_scen, exp_no, i);
				seninf_padidx = PAD_SRC_RAW0;
				mtk_cam_call_sv_pipeline_config(ctx, ctx->sv_dev->tag_info, i,
					seninf_padidx, hw_scen, tag_order, 3,
					cfg_in_param->subsample, NULL, mf, img_fmt);
				ctx->pipe->enabled_sv_tags |= (1 << i);
				ctx->sv_dev->enabled_tags |= (1 << i);
				ctx->sv_dev->used_tag_cnt++;
			}
		}
	} else if (config_pipe && mtk_cam_scen_is_rgbw_using_camsv(scen_active)) {
		unsigned int hw_scen, exp_no, req_amount;
		unsigned int idle_tags, seninf_padidx, tag_order;

		hw_scen = (1 << MTKCAM_SV_SPECIAL_SCENARIO_ADDITIONAL_RAW);
		exp_no = 1;
		req_amount = 1;
		idle_tags = mtk_cam_get_sv_idle_tags(ctx,
			ctx->sv_dev->enabled_tags, hw_scen, exp_no, req_amount, true, false);
		if (idle_tags == 0) {
			dev_info(cam->dev, "no available sv tags(scen:%d/req_amount:%d)",
				hw_scen, req_amount);
			return -EINVAL;
		}

		/* update image format for corresponding scenario */
		img_fmt = &ctx->pipe->vdev_nodes[
			MTK_RAW_MAIN_STREAM_SV_1_OUT - MTK_RAW_SINK_NUM].active_fmt;

		for (i = SVTAG_IMG_START; i < SVTAG_IMG_END; i++) {
			if (idle_tags & (1 << i)) {
				tag_order = mtk_cam_get_sv_mapped_tag_order(hw_scen, exp_no, i);
				seninf_padidx = PAD_SRC_RAW_W0;
				mtk_cam_call_sv_pipeline_config(ctx, ctx->sv_dev->tag_info, i,
					seninf_padidx, hw_scen, tag_order, 3,
					cfg_in_param->subsample, NULL, mf, img_fmt);
				ctx->pipe->enabled_sv_tags |= (1 << i);
				ctx->sv_dev->enabled_tags |= (1 << i);
				ctx->sv_dev->used_tag_cnt++;
			}
		}
	} else if (config_pipe && mtk_cam_scen_is_ext_isp(scen_active)) {
		unsigned int hw_scen, exp_no, req_amount, idle_tags;

		hw_scen = (1 << MTKCAM_SV_SPECIAL_SCENARIO_EXT_ISP);
		exp_no = 1;
		if (mtk_cam_scen_is_ext_isp_yuv(scen_active))
			req_amount = 3;
		else
			req_amount = 2;
		if (ctx->ext_isp_pureraw_off)
			req_amount = req_amount - 1;
		idle_tags = mtk_cam_get_sv_idle_tags(ctx,
			ctx->sv_dev->enabled_tags, hw_scen, exp_no, req_amount, true, false);
		if (idle_tags == 0) {
			dev_info(cam->dev, "no available sv tags(scen:%d/req_amount:%d)",
				hw_scen, req_amount);
			return -EINVAL;
		}

		mtk_cam_sv_extisp_tag_update(ctx, ctx->sv_dev->tag_info, idle_tags,
			hw_scen, exp_no, cfg_in_param->subsample, mf);
		ctx->sv_dev->enabled_tags |= idle_tags;
		ctx->sv_dev->used_tag_cnt += req_amount;
	} else if (config_pipe && !mtk_cam_scen_is_mstream(scen_active) &&
		!mtk_cam_scen_is_mstream_m2m(scen_active) &&
		!mtk_cam_scen_is_subsample(scen_active) &&
		!mtk_cam_is_hsf(ctx)) {
#if PURE_RAW_WITH_SV
		unsigned int hw_scen, exp_no, req_amount;
		unsigned int idle_tags, seninf_padidx, tag_order;

		hw_scen = (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_ON_THE_FLY));
		exp_no = 1;
		req_amount = 1;
		idle_tags = mtk_cam_get_sv_idle_tags(ctx,
			ctx->sv_dev->enabled_tags, hw_scen, exp_no, req_amount, true, false);
		if (idle_tags == 0) {
			dev_info(cam->dev, "no available sv tags(scen:%d/req_amount:%d)",
				hw_scen, req_amount);
			return -EINVAL;
		}

		ctx->pure_raw_sv_tag_idx = SVTAG_2;

		tag_order = MTKCAM_IPI_ORDER_FIRST_TAG;
		seninf_padidx = PAD_SRC_RAW0;
		mtk_cam_call_sv_pipeline_config(ctx, ctx->sv_dev->tag_info, SVTAG_2,
			seninf_padidx, hw_scen, tag_order, 3,
			cfg_in_param->subsample, NULL, mf, img_fmt);
		ctx->sv_dev->enabled_tags |= idle_tags;
		ctx->sv_dev->used_tag_cnt += req_amount;
#endif
	}

	/* camsv's meta config */
	for (i = 0; i < ctx->used_sv_num; i++) {
		unsigned int idle_tags, tag_idx = SVTAG_META_START;
		unsigned int sv_cammux_id;

		idle_tags = mtk_cam_get_sv_idle_tags(ctx,
			ctx->sv_dev->enabled_tags, 0, 1, 1, true, false);
		if (idle_tags == 0) {
			dev_info(cam->dev, "no available sv tags for meta use");
			return -EINVAL;
		}
		for (j = SVTAG_META_START; j < SVTAG_META_END; j++) {
			if (idle_tags & (1 << j)) {
				tag_idx = j;
				break;
			}
		}

		sv_cammux_id = mtk_cam_get_sv_cammux_id(ctx->sv_dev, tag_idx);
		mf = &ctx->sv_pipe[i]->cfg[MTK_CAMSV_SINK].mbus_fmt;
		img_fmt = &ctx->sv_pipe[i]->vdev_nodes[
			MTK_CAMSV_MAIN_STREAM_OUT - MTK_CAMSV_SINK_NUM].active_fmt;

		mtk_cam_call_sv_pipeline_config(ctx, ctx->sv_dev->tag_info,
			tag_idx, ctx->sv_pipe[i]->seninf_padidx, 1,
			mtk_cam_seninf_get_tag_order(ctx->seninf,
			ctx->sv_pipe[i]->seninf_padidx),
			3, cfg_in_param->subsample,
			ctx->sv_pipe[i], mf, img_fmt);
		ctx->sv_dev->enabled_tags |= (1 << tag_idx);
		ctx->sv_dev->used_tag_cnt++;
	}

	/* camsv's config param update */
	for (i = SVTAG_START; i < SVTAG_END; i++) {
		if (ctx->sv_dev->enabled_tags & (1 << i)) {
			/* camsv todo: ais */
			config_param.sv_input[0][i].pipe_id =
				ctx->sv_dev->id + MTKCAM_SUBDEV_CAMSV_START;
			config_param.sv_input[0][i].tag_id = i;
			config_param.sv_input[0][i].tag_order =
				ctx->sv_dev->tag_info[i].tag_order;
			config_param.sv_input[0][i].is_first_frame = 1;
			config_param.sv_input[0][i].input =
				ctx->sv_dev->tag_info[i].cfg_in_param;
		}
	}

	/* mraw's config param update */
	for (i = 0; i < ctx->used_mraw_num; i++) {
		config_param.mraw_input[i].pipe_id =
			ctx->mraw_pipe[i]->id;
		config_param.mraw_input[i].input.fmt =
			ctx->mraw_pipe[i]->res_config.tg_fmt;
		config_param.mraw_input[i].input.raw_pixel_id = 0;
		config_param.mraw_input[i].input.data_pattern = 0;
		config_param.mraw_input[i].input.pixel_mode = 3;
		config_param.mraw_input[i].input.pixel_mode_before_raw = 3;
		config_param.mraw_input[i].input.subsample =
			cfg_in_param->subsample;
		config_param.mraw_input[i].input.in_crop =
			ctx->mraw_pipe[i]->res_config.tg_crop;
		dev_info(dev, "%s pipe id:%d tg crop width: %d height: %d", __func__,
			config_param.mraw_input[i].pipe_id,
			config_param.mraw_input[i].input.in_crop.s.w,
			config_param.mraw_input[i].input.in_crop.s.h);
	}

	update_hw_mapping(ctx, &config_param);
	ret = update_scen_param(ctx, &config_param, scen_active);
	if (ret)
		return ret;

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
				if (raw_dev_slave) {
					raw_dev_slave->pipeline = &raw->pipelines[i];
					dev_dbg(dev,
						"twin master/slave raw_id:%d/%d\n",
						raw_dev->id, raw_dev_slave->id);
				} else {
					dev_info(dev,
						"%s:master:%d get slave failed\n",
						__func__, raw_dev->id);
				}

				if (raw->pipelines[i].res_config.raw_num_used == 3) {
					struct mtk_raw_device *raw_dev_slave2 =
						get_slave2_raw_dev(cam, ctx->pipe);
					if (raw_dev_slave2) {
						raw_dev_slave2->pipeline =
							&raw->pipelines[i];
						dev_dbg(dev,
							"triplet master/slave2 raw_id:%d/%d\n",
							raw_dev->id,
							raw_dev_slave2->id);
					} else {
						dev_info(dev,
							"%s:master:%d get slave2 failed\n",
							__func__, raw_dev->id);
					}
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
	unsigned int ipi_id;

	/* camsv todo: correct the name after expand ipi id */
	if (is_camsv_subdev(pipe_id))
		ipi_id = CCD_IPI_MRAW_CMD;
	else
		ipi_id = CCD_IPI_ISP_MAIN + pipe_id - MTKCAM_SUBDEV_RAW_START;

	if (ipi_id < CCD_IPI_ISP_MAIN || ipi_id > CCD_IPI_MRAW_CMD) {
		dev_info(ctx->cam->dev,
			"%s: pipeline(%d), ipi_id(%d) invalid(min:%d,max:%d)\n",
			__func__, pipe_id, ipi_id, CCD_IPI_ISP_MAIN,
			CCD_IPI_MRAW_CMD);
		return -EINVAL;
	}

	dev_dbg(ctx->cam->dev, "%s: pipeline(%d), ipi_id(%d)\n",
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

	snprintf_safe(msg->name, RPMSG_NAME_SIZE, "mtk-camsys\%d", ipi_id - 1);
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
	dev_dbg(dev, "- %s\n", __func__);

	return 0;
}

static int mtk_cam_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "- %s\n", __func__);

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
	for (i = 0; i < MAX_SV_HW_TAGS; i++)
		ctx->sv_dequeued_frame_seq_no[i] = 0;
	ctx->sv_using_buffer_list.cnt = 0;
	ctx->sv_composed_buffer_list.cnt = 0;
	ctx->sv_processing_buffer_list.cnt = 0;
	for (i = 0; i < MAX_MRAW_PIPES_PER_STREAM; i++) {
		ctx->mraw_dequeued_frame_seq_no[i] = 0;
		ctx->mraw_using_buffer_list[i].cnt = 0;
		ctx->mraw_composed_buffer_list[i].cnt = 0;
		ctx->mraw_processing_buffer_list[i].cnt = 0;
	}
	ctx->enqueued_request_cnt = 0;
	ctx->next_sof_mask_frame_seq_no = 0;
	ctx->working_request_seq = 0;
	ctx->ext_isp_meta_off = 0;
	ctx->ext_isp_pureraw_off = 0;
	ctx->ext_isp_procraw_off = 0;
	atomic_set(&ctx->running_s_data_cnt, 0);
	mutex_init(&ctx->sensor_switch_op_lock);
	init_completion(&ctx->session_complete);

#if PURE_RAW_WITH_SV
	ctx->pure_raw_sv_tag_idx = -1;
#endif

	mtk_cam_dvfs_tbl_init(&ctx->dvfs_tbl, cam->camsys_ctrl.dvfs_info.clklv_num);

	is_first_ctx = !cam->composer_cnt;
	if (is_first_ctx) {
		INIT_LIST_HEAD(&cam->running_job_list);
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
	ret = isp_composer_init(ctx, node->uid.pipe_id);
	if (ret)
		goto fail_shutdown;
#endif
	cam->composer_cnt++;

	cmdq_mbox_enable(cam->cmdq_clt->chan);

	ret = mtk_cam_working_buf_pool_alloc(ctx);
	if (ret) {
		dev_info(cam->dev, "failed to reserve DMA memory:%d\n", ret);
		goto fail_uninit_composer;
	}

	ret = mtk_cam_working_buf_pool_init(ctx);
	if (ret) {
		dev_info(cam->dev, "failed to initialize raw working buf pool:%d\n", ret);
		goto fail_uninit_composer;
	}

	ret = mtk_cam_sv_working_buf_pool_init(ctx);
	if (ret) {
		dev_info(cam->dev, "failed to initialize sv working buf pool:%d\n", ret);
		goto fail_uninit_composer;
	}

	ret = mtk_cam_mraw_working_buf_pool_init(ctx);
	if (ret) {
		dev_info(cam->dev, "failed to initialize mraw working buf pool:%d\n", ret);
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

	ctx->cmdq_wq =
			alloc_ordered_workqueue(dev_name(cam->dev),
						WQ_HIGHPRI | WQ_FREEZABLE);
	if (!ctx->cmdq_wq) {
		dev_info(cam->dev, "failed to alloc cmdq workqueue\n");
		goto fail_uninit_cmdq_worker_task;
	}

	ctx->frame_done_wq =
			alloc_ordered_workqueue(dev_name(cam->dev),
						WQ_HIGHPRI | WQ_FREEZABLE);
	if (!ctx->frame_done_wq) {
		dev_info(cam->dev, "failed to alloc frame_done workqueue\n");
		goto fail_uninit_composer_wq;
	}

	ret = media_pipeline_start(entity, &ctx->pipeline);
	if (ret) {
		dev_info(cam->dev,
			 "%s:pipe(%d):failed in media_pipeline_start:%d\n",
			 __func__, node->uid.pipe_id, ret);
		goto fail_uninit_frame_done_wq;
	}

	/* traverse to update used subdevs & number of nodes */
	graph = &ctx->pipeline.graph;
	media_graph_walk_start(graph, entity);

	mutex_lock(&cam->v4l2_dev.mdev->graph_mutex);

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
	mutex_unlock(&cam->v4l2_dev.mdev->graph_mutex);

	return ctx;

fail_stop_pipeline:
	mutex_unlock(&cam->v4l2_dev.mdev->graph_mutex);
	media_pipeline_stop(entity);
fail_uninit_frame_done_wq:
	destroy_workqueue(ctx->frame_done_wq);
fail_uninit_composer_wq:
	destroy_workqueue(ctx->composer_wq);
fail_uninit_cmdq_worker_task:
	destroy_workqueue(ctx->cmdq_wq);
fail_uninit_sensor_worker_task:
	kthread_stop(ctx->sensor_worker_task);
	ctx->sensor_worker_task = NULL;
fail_release_buffer_pool:
	mtk_cam_working_buf_pool_release(ctx);
fail_uninit_composer:
#if CCD_READY
	isp_composer_uninit(ctx);
	cam->composer_cnt--;
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
	struct mtk_raw_device *raw_dev;
	unsigned int i, j;
	struct slbc_data slb;
	int ret = 0;

	dev_info(cam->dev, "%s:ctx(%d): triggered by %s\n",
		 __func__, ctx->stream_id, entity->name);

	if (watchdog_scenario(ctx)) {
		raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
		mtk_ctx_watchdog_stop(ctx, raw_dev->id + MTKCAM_SUBDEV_RAW_START);
		for (i = 0; i < ctx->used_sv_num; i++)
			mtk_ctx_watchdog_stop(ctx, ctx->sv_pipe[i]->id);
		for (i = 0; i < ctx->used_mraw_num; i++)
			mtk_ctx_watchdog_stop(ctx, ctx->mraw_pipe[i]->id);
	}

	media_pipeline_stop(entity);

	/* Consider scenario that stop the ctx while the ctx is not streamed on */
	if (ctx->session_created) {
		dev_dbg(cam->dev,
			"%s:ctx(%d): session_created, wait for composer session destroy\n",
			__func__, ctx->stream_id);
		if (wait_for_completion_timeout(
			&ctx->session_complete, msecs_to_jiffies(1000)) == 0)
			dev_info(cam->dev, "%s:ctx(%d): complete timeout\n",
			__func__, ctx->stream_id);
	}

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
	drain_workqueue(ctx->cmdq_wq);
	destroy_workqueue(ctx->cmdq_wq);
	ctx->cmdq_wq = NULL;
	drain_workqueue(ctx->frame_done_wq);
	destroy_workqueue(ctx->frame_done_wq);
	ctx->frame_done_wq = NULL;
	kthread_flush_worker(&ctx->sensor_worker);
	kthread_stop(ctx->sensor_worker_task);
	ctx->sensor_worker_task = NULL;

	for (i = 0 ; i < ctx->used_sv_num ; i++) {
		ctx->sv_pipe[i]->feature_pending = 0;
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

	if (mtk_cam_ctx_has_raw(ctx)) {
		mutex_lock(&cam->dvfs_op_lock);
		memset(&ctx->dvfs_tbl, 0, sizeof(ctx->dvfs_tbl.opp_cnt));
		mtk_cam_dvfs_update_clk(cam, false);
		mutex_unlock(&cam->dvfs_op_lock);
	}

	ctx->session_created = 0;
	ctx->enabled_node_cnt = 0;
	ctx->streaming_node_cnt = 0;
	ctx->streaming_pipe = 0;
	ctx->sensor = NULL;
	ctx->prev_sensor = NULL;
	ctx->seninf = NULL;
	ctx->prev_seninf = NULL;
	atomic_set(&ctx->enqueued_frame_seq_no, 0);
	atomic_set(&ctx->composed_delay_seq_no, 0);
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
		ctx->pipe->req_sensor_new = NULL;
		ctx->pipe->req_seninf_old = NULL;
		ctx->pipe->req_seninf_new = NULL;
		ctx->pipe->enabled_raw = 0;
		ctx->pipe->enabled_dmas = 0;
		memset(&ctx->pipe->pde_config, 0, sizeof(ctx->pipe->pde_config));
	}

	if (ctx->sv_dev) {
		ctx->sv_dev->used_tag_cnt = 0;
		ctx->sv_dev->enabled_tags = 0;
		mtk_cam_sv_reset_tag_info(ctx->sv_dev->tag_info);
		ctx->sv_dev = NULL;
	}

#if PURE_RAW_WITH_SV
	ctx->pure_raw_sv_tag_idx = -1;
#endif

	INIT_LIST_HEAD(&ctx->using_buffer_list.list);
	INIT_LIST_HEAD(&ctx->composed_buffer_list.list);
	INIT_LIST_HEAD(&ctx->processing_buffer_list.list);

	INIT_LIST_HEAD(&ctx->sv_using_buffer_list.list);
	INIT_LIST_HEAD(&ctx->sv_composed_buffer_list.list);
	INIT_LIST_HEAD(&ctx->sv_processing_buffer_list.list);

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
	}

	for (i = 0; i < MAX_MRAW_PIPES_PER_STREAM; i++) {
		ctx->mraw_pipe[i] = NULL;
		ctx->used_mraw_dev[i] = 0;
	}

	isp_composer_uninit(ctx);
	cam->composer_cnt--;

	dev_info(cam->dev, "%s: ctx-%d:  composer_cnt:%d\n",
		__func__, ctx->stream_id, cam->composer_cnt);

	if (ctx->slb_addr) {
		slb.uid = UID_SH_P1;
		slb.type = TP_BUFFER;
		ret = slbc_release(&slb);
		if (ret < 0)
			dev_info(cam->dev, "failed to release slb buffer");
	}
	ctx->slb_addr = NULL;

	if (cam->cmdq_clt)
		cmdq_mbox_disable(cam->cmdq_clt->chan);

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
		if (cam->running_job_count)
			dev_info(cam->dev,
				"%s running job leak(%d)\n",
				__func__, cam->running_job_count);
	}
}
int PipeIDtoTGIDX(int pipe_id)
{
	/* camsv/mraw's cammux id is defined in its own dts */
	int cammux_id_raw_start = GET_PLAT_V4L2(cammux_id_raw_start);

	switch (pipe_id) {
	case MTKCAM_SUBDEV_RAW_0:
		return cammux_id_raw_start;
	case MTKCAM_SUBDEV_RAW_1:
		return cammux_id_raw_start + 1;
	case MTKCAM_SUBDEV_RAW_2:
		return cammux_id_raw_start + 2;
	default:
			break;
	}
	return -1;
}

static void mtk_cam_ctx_img_working_buf_pool_release(struct mtk_cam_ctx *ctx)
{
	if (ctx->img_buf_pool.working_img_buf_size > 0) {
		if (ctx->img_buf_pool.pre_alloc_img_buf.size > 0)
			mtk_cam_user_img_working_buf_pool_release(ctx);
		else
			mtk_cam_internal_img_working_buf_pool_release(ctx);
	}
}

int mtk_cam_ctx_stream_on(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev;
	struct mtk_raw_device *raw_dev = NULL;
	int i, ret = 0;
	int tgo_pxl_mode;
	bool need_dump_mem = false;
	struct mtk_cam_scen *scen_active = NULL;	/* Used to know max exposure num */
	int exp_no;
	int buf_require = 0;
	int buf_size = 0;

	dev_info(cam->dev, "%s: ctx %d stream on, streaming_pipe:0x%x\n",
		 __func__, ctx->stream_id, ctx->streaming_pipe);

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

	if (ctx->pipe)
		scen_active = &ctx->pipe->scen_active;

	if (ctx->used_raw_num && ctx->pipe) {
		/* check exposure number */
		exp_no = mtk_cam_scen_get_max_exp_num(scen_active);

		tgo_pxl_mode = ctx->pipe->res_config.tgo_pxl_mode_before_raw;
		buf_size = ctx->pipe->vdev_nodes
			[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM].
			active_fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
		if (mtk_cam_hw_is_dc(ctx) &&
		    ctx->pipe->img_fmt_sink_pad.fmt.pix_mp.plane_fmt[0].sizeimage > buf_size)
			buf_size = ctx->pipe->img_fmt_sink_pad.fmt.pix_mp.plane_fmt[0].sizeimage;

#ifdef MTK_CAM_USER_WBUF_TEST
		if (ctx->pipe &&
		    ctx->pipe->pre_alloc_mem.num > 0 &&
		    ctx->pipe->pre_alloc_mem.bufs[0].fd >= 0) {
			int num = 4;
			int buf_sz = ctx->pipe->pre_alloc_mem.bufs[0].length;

			mtk_cam_user_img_working_buf_pool_init(ctx, num, buf_sz);
			mtk_cam_user_img_working_buf_pool_release(ctx);
		} else {
			dev_info(cam->dev,
				 "pre-alloc mem test failed(%d, %d, %d)\n",
				 ctx->pipe->pre_alloc_mem.num,
				 ctx->pipe->pre_alloc_mem.bufs[0].fd,
				 ctx->pipe->pre_alloc_mem.bufs[0].length);
		}
#endif
		/**
		 * TODO: validate pad's setting of each pipes
		 * return -EPIPE if failed
		 */
		buf_require =
			mtk_cam_get_internl_buf_num(ctx->pipe->dynamic_exposure_num_max,
						    scen_active,
						    ctx->pipe->hw_mode_pending);
		if (buf_require)
			ret = mtk_cam_ctx_img_working_buf_pool_init(ctx, buf_require, buf_size);

		if (ret) {
			dev_info(cam->dev, "failed to reserve DMA memory:%d\n", ret);
			goto fail_img_buf_release;
		}

		if (mtk_cam_scen_is_ext_isp(scen_active)) {
			ctx->ext_isp_meta_off = debug_preisp_off_data &
					(1 << DEBUG_PREISP_OFF_META);
			ctx->ext_isp_pureraw_off = debug_preisp_off_data &
					(1 << DEBUG_PREISP_OFF_PURERAW);
			ctx->ext_isp_procraw_off = debug_preisp_off_data &
					(1 << DEBUG_PREISP_OFF_PROCRAW);
			mtk_cam_extisp_prepare_meta(ctx, PAD_SRC_GENERAL0);
			if (mtk_cam_extisp_prepare_meta(ctx, PAD_SRC_RAW0) == 0) {
				ctx->ext_isp_pureraw_off = 1;
				dev_info(ctx->cam->dev, "[%s] no pureraw: debug_preisp_off_dara:%d, 0/1/2 = %d/%d/%d\n",
					__func__, debug_preisp_off_data,
					ctx->ext_isp_meta_off,
					ctx->ext_isp_pureraw_off,
					ctx->ext_isp_procraw_off);
			}
			dev_info(ctx->cam->dev, "[%s] debug_preisp_off_dara:%d, 0/1/2 = %d/%d/%d\n",
					__func__, debug_preisp_off_data,
					ctx->ext_isp_meta_off,
					ctx->ext_isp_pureraw_off,
					ctx->ext_isp_procraw_off);
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
			if (mtk_cam_scen_is_sensor_stagger(scen_active) &&
			    mtk_cam_scen_get_exp_num(scen_active) > 1)
				stagger_enable(raw_dev);

			if (mtk_cam_scen_is_rgbw_enabled(scen_active)) {
				struct mtk_raw_device *w_raw_dev =
					get_slave_raw_dev(cam, ctx->pipe);

				scq_stag_mode_enable(raw_dev, 1);
				if (w_raw_dev)
					scq_stag_mode_enable(w_raw_dev, 1);
				else {
					dev_info(cam->dev, "Error: W path raw not found\n");
					scq_stag_mode_enable(raw_dev, 0);
					goto fail_img_buf_release;
				}
			}

			/* Sub sample */
			if (mtk_cam_scen_is_subsample(scen_active))
				subsample_enable(raw_dev);
			/* Twin */
			if (ctx->pipe->res_config.raw_num_used != 1) {
				struct mtk_raw_device *raw_dev_slave =
							get_slave_raw_dev(cam, ctx->pipe);
				if (raw_dev_slave)
					initialize(raw_dev_slave, 1);
				if (ctx->pipe->res_config.raw_num_used == 3) {
					struct mtk_raw_device *raw_dev_slave2 =
						get_slave2_raw_dev(cam, ctx->pipe);
					if (raw_dev_slave2)
						initialize(raw_dev_slave2, 1);
				}
			}
		}

		/*set cam mux camtg and pixel mode*/
		if (mtk_cam_scen_is_sensor_stagger(scen_active)) {
			if (mtk_cam_hw_is_otf(ctx)) {
				int seninf_pad;
				int max_exp_num = mtk_cam_scen_get_max_exp_num(scen_active);

				if (max_exp_num == 2)
					seninf_pad = PAD_SRC_RAW1;
				else if (max_exp_num == 3)
					seninf_pad = PAD_SRC_RAW2;
				else
					seninf_pad = PAD_SRC_RAW0;
				mtk_cam_seninf_set_camtg(ctx->seninf, seninf_pad,
							PipeIDtoTGIDX(raw_dev->id));
			}
		} else if (mtk_cam_scen_is_ext_isp(scen_active)) {
			if (mtk_cam_scen_is_ext_isp_yuv(scen_active)) {
				dev_info(cam->dev, "[PreISP] YUV pipeline enabled_raw:0x%x\n",
					ctx->pipe->enabled_raw);
			} else {
				mtk_cam_seninf_set_camtg(ctx->seninf, PAD_SRC_RAW_EXT0,
						PipeIDtoTGIDX(raw_dev->id));
			}
		} else if (!mtk_cam_scen_is_m2m(scen_active) &&
			   !mtk_cam_scen_is_time_shared(scen_active)) {
#ifdef MTK_CAM_HSF_SUPPORT
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
#endif
			if (!(exp_no == 1 && mtk_cam_hw_is_dc(ctx))) {
				mtk_cam_seninf_set_camtg(ctx->seninf, PAD_SRC_RAW0,
							 PipeIDtoTGIDX(raw_dev->id));
			}
		}
	}

	/* go on m2m flow */

	if (!mtk_cam_scen_is_m2m(scen_active)) {
		if (ctx->used_raw_num == 0) {
			ret = mtk_cam_sv_dev_config(ctx);
			if (ret)
				goto fail_img_buf_release;
		}

		if (ctx->sv_dev) {
			for (i = SVTAG_START; i < SVTAG_END; i++) {
				if (ctx->sv_dev->enabled_tags & (1 << i)) {
					unsigned int sv_cammux_id =
						mtk_cam_get_sv_cammux_id(ctx->sv_dev, i);
					mtk_cam_seninf_set_camtg_camsv(ctx->seninf,
						ctx->sv_dev->tag_info[i].seninf_padidx,
						sv_cammux_id, i);
				}
			}
		}

		for (i = 0; i < ctx->used_mraw_num; i++) {
			mtk_cam_seninf_set_camtg(ctx->seninf,
				ctx->mraw_pipe[i]->seninf_padidx,
				ctx->mraw_pipe[i]->cammux_id);
			ret = mtk_cam_mraw_dev_config(ctx, i);
			if (ret)
				goto fail_img_buf_release;
		}
		if (mtk_cam_scen_is_ext_isp(scen_active))
			dev_info(cam->dev, "not to stream on seninf %s for preisp mode\n",
					 ctx->seninf->name);
		else {
			/* must be called after all pipes' set_pixel/set_camtg done */
			ret = v4l2_subdev_call(ctx->seninf, video, s_stream, 1);
			if (ret) {
				dev_info(cam->dev, "failed to stream on seninf %s:%d\n",
					 ctx->seninf->name, ret);
				goto fail_img_buf_release;
			}
		}
	} else {
		spin_lock(&ctx->processing_buffer_list.lock);
		ctx->processing_buffer_list.cnt = 0;
		spin_unlock(&ctx->processing_buffer_list.lock);
		spin_lock(&ctx->composed_buffer_list.lock);
		ctx->composed_buffer_list.cnt = 0;
		spin_unlock(&ctx->composed_buffer_list.lock);
		dev_dbg(cam->dev, "[M2M] reset processing_buffer_list.cnt & composed_buffer_list.cnt\n");
	}

	/* config camsv */
	if (ctx->sv_dev) {
		pm_runtime_get_sync(ctx->sv_dev->dev);
		mtk_cam_sv_pipeline_config(ctx->sv_dev);
	}

	if (ctx->used_raw_num) {
		if (!mtk_cam_is_hsf(ctx)) {
			initialize(raw_dev, 0);
			/* Stagger */
			if (mtk_cam_scen_is_sensor_stagger(scen_active) &&
			    mtk_cam_scen_get_exp_num(scen_active))
				stagger_enable(raw_dev);

			if (mtk_cam_scen_is_rgbw_enabled(scen_active)) {
				struct mtk_raw_device *w_raw_dev =
					get_slave_raw_dev(cam, ctx->pipe);

				scq_stag_mode_enable(raw_dev, 1);
				if (w_raw_dev)
					scq_stag_mode_enable(w_raw_dev, 1);
				else {
					dev_info(cam->dev, "Error: W path raw not found\n");
					scq_stag_mode_enable(raw_dev, 0);
					goto fail_img_buf_release;
				}
			}

			/* Sub sample */
			if (mtk_cam_scen_is_subsample(scen_active))
				subsample_enable(raw_dev);
			/* Twin */
			if (ctx->pipe->res_config.raw_num_used != 1) {
				struct mtk_raw_device *raw_dev_slave =
					get_slave_raw_dev(cam, ctx->pipe);
				if (raw_dev_slave)
					initialize(raw_dev_slave, 1);
				if (ctx->pipe->res_config.raw_num_used == 3) {
					struct mtk_raw_device *raw_dev_slave2 =
						get_slave2_raw_dev(cam, ctx->pipe);
					if (raw_dev_slave2)
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
		mtk_cam_dvfs_update_clk(ctx->cam, true);

	ret = mtk_camsys_ctrl_start(ctx);
	if (ret)
		goto fail_streaming_off;

	mutex_lock(&cam->queue_lock);
	mtk_cam_dev_req_try_queue(cam);  /* request moved into working list */
	mutex_unlock(&cam->queue_lock);

	dev_dbg(cam->dev, "streamed on camsys ctx:%d\n", ctx->stream_id);

	if (raw_dev)
		mtk_cam_register_iommu_tf_callback(raw_dev);

	return 0;
fail_streaming_off:
	spin_lock(&ctx->streaming_lock);
	ctx->streaming = false;
	cam->streaming_ctx &= ~(1 << ctx->stream_id);
	spin_unlock(&ctx->streaming_lock);
	/* reset dvfs/qos */
	if (!mtk_cam_scen_is_m2m(&ctx->pipe->scen_active))
		v4l2_subdev_call(ctx->seninf, video, s_stream, 0);
fail_img_buf_release:
	mtk_cam_ctx_img_working_buf_pool_release(ctx);
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
			ts_id = mtk_cam_scen_is_time_shared(&pipe->scen_active);
			pipe_chk = ctx_chk->pipe;
			ts_id_chk = mtk_cam_scen_is_time_shared(&pipe_chk->scen_active);
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

	unsigned int i;
	int ret;
	struct mtk_cam_scen *scen_active = NULL;

	if (!ctx->streaming) {
		dev_info(cam->dev, "ctx-%d is already streaming off\n",
			 ctx->stream_id);
		return 0;
	}

	if (mtk_cam_ctx_has_raw(ctx))
		scen_active = &ctx->pipe->scen_active;

	dev_info(cam->dev, "%s: ctx-%d:  composer_cnt:%d, streaming_pipe:0x%x\n",
		__func__, ctx->stream_id, cam->composer_cnt, ctx->streaming_pipe);

	spin_lock(&ctx->streaming_lock);
	ctx->streaming = false;
	cam->streaming_ctx &= ~(1 << ctx->stream_id);
	spin_unlock(&ctx->streaming_lock);

	// If stagger, need to turn off cam sv in advanced
	mtk_cam_sv_dev_stream_on(ctx, 0);

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

	if (ctx->used_raw_num) {
		dev = mtk_cam_find_raw_dev(cam, ctx->used_raw_dev);
		if (!dev) {
			dev_info(cam->dev, "streamoff raw device not found\n");
			goto fail_stream_off;
		}
		raw_dev = dev_get_drvdata(dev);
		if (scen_active && mtk_cam_scen_is_time_shared(scen_active)) {
			if (mtk_cam_ts_are_all_ctx_off(cam, ctx))
				stream_on(raw_dev, 0);
		} else {
			stream_on(raw_dev, 0);
#ifdef MTK_CAM_HSF_SUPPORT
			if (mtk_cam_is_hsf(ctx)) {
				ret = mtk_cam_hsf_uninit(ctx);
				if (ret != 0) {
					dev_info(cam->dev, "failed to stream off %s:%d mtk_cam_hsf_uninit fail\n",
						ctx->seninf->name, ret);
					return -EPERM;
				}
			}
#endif

		}
		/* Twin */
		if (ctx->pipe->res_config.raw_num_used != 1) {
			struct mtk_raw_device *raw_dev_slave =
						get_slave_raw_dev(cam, ctx->pipe);
			if (raw_dev_slave)
				stream_on(raw_dev_slave, 0);
			if (ctx->pipe->res_config.raw_num_used == 3) {
				struct mtk_raw_device *raw_dev_slave2 =
					get_slave2_raw_dev(cam, ctx->pipe);
				if (raw_dev_slave2)
					stream_on(raw_dev_slave2, 0);
			}
		}
	}

	for (i = 0 ; i < ctx->used_mraw_num ; i++) {
		ret = mtk_cam_mraw_dev_stream_on(ctx,
			get_mraw_dev(ctx->cam, ctx->mraw_pipe[i]), 0);
		if (ret)
			return ret;
	}

	/* reset dvfs/qos */
	if (ctx->used_raw_num)
		mtk_cam_qos_bw_reset(ctx);
	else
		mtk_cam_qos_sv_bw_reset(ctx);

	for (i = 0; i < MAX_PIPES_PER_STREAM && ctx->pipe_subdevs[i]; i++) {
		ret = v4l2_subdev_call(ctx->pipe_subdevs[i], video,
				       s_stream, 0);
		if (ret) {
			dev_info(cam->dev, "failed to stream off %d: %d\n",
				 ctx->pipe_subdevs[i]->name, ret);
			return -EPERM;
		}
	}

	/* stream off seninf in non-m2m scenario including camsv/mraw only case */
	if (!scen_active || !mtk_cam_scen_is_m2m(scen_active)) {
		if (mtk_cam_scen_is_ext_isp(scen_active))
			dev_info(cam->dev, "not to stream off seninf %s for preisp mode\n",
					 ctx->seninf->name);
		else {
			ret = v4l2_subdev_call(ctx->seninf, video, s_stream, 0);
			if (ret) {
				dev_info(cam->dev, "failed to stream off %s:%d\n",
					 ctx->seninf->name, ret);
				return -EPERM;
			}
		}
	}

	if (ctx->sv_dev)
		pm_runtime_put_sync(ctx->sv_dev->dev);

	mtk_cam_ctx_img_working_buf_pool_release(ctx);
	mtk_cam_generic_buf_release(ctx);
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
	int i, j, ret;

	for (i = 0; i < cam->max_stream_num; i++) {
		if (i >= MTKCAM_SUBDEV_RAW_START &&
		    i < (MTKCAM_SUBDEV_RAW_START + cam->num_raw_drivers) &&
		    i < RAW_PIPELINE_NUM) {
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
			pipe_entity =
				&cam->sv.pipelines[i - MTKCAM_SUBDEV_CAMSV_START].subdev.entity;

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
		} else if (i >= MTKCAM_SUBDEV_MRAW_START && i < (MTKCAM_SUBDEV_MRAW_START +
			   GET_PLAT_V4L2(mraw_pipeline_num))) {

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
	int ret = 0;

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

	media_dev->dev = cam_dev->dev;
	strscpy(media_dev->model, dev_driver_string(dev),
		sizeof(media_dev->model));
	snprintf_safe(media_dev->bus_info, sizeof(media_dev->bus_info),
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

	mutex_lock(&cam_dev->v4l2_dev.mdev->graph_mutex);
	mtk_cam_create_links(cam_dev);
	/* Expose all subdev's nodes */
	ret = v4l2_device_register_subdev_nodes(&cam_dev->v4l2_dev);
	mutex_unlock(&cam_dev->v4l2_dev.mdev->graph_mutex);
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

	cam_dev->num_camsv_drivers =
		add_match_by_driver(dev, &match, &mtk_cam_sv_driver);

	cam_dev->num_mraw_drivers =
		add_match_by_driver(dev, &match, &mtk_cam_mraw_driver);

	cam_dev->num_seninf_drivers =
		add_match_by_driver(dev, &match, &seninf_pdrv);

	if (IS_ERR(match))
		mtk_cam_match_remove(dev);

	dev_info(dev, "#: raw %d, yuv %d, sv %d, seninf %d, mraw %d\n",
		 cam_dev->num_raw_drivers, yuv_num,
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
	struct mtk_cam_watchdog_data *watchdog_data;
	u64 watchdog_cnt;
	u64 watchdog_dump_cnt, watchdog_timeout_cnt;
	int timeout;
	static u64 last_vsync_count;
	bool is_abnormal_vsync = false;
	unsigned int int_en, dequeued_frame_seq_no;
	int vf_en, sof_count;
	int pipe_id;
	unsigned int idx;
	struct device *dev;
	struct mtk_camsv_device *camsv_dev;
	struct mtk_mraw_device *mraw_dev;
	int enabled_watchdog_pipe;
	unsigned long flags;

	watchdog_data =
		container_of(work, struct mtk_cam_watchdog_data, watchdog_work);

	ctx = watchdog_data->ctx;
	if (!ctx) {
		pr_info("%s:ctx(%d):stop watchdog task for ctx is null\n",
			__func__);
		return;
	}
	seninf = ctx->seninf;
	if (!seninf) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d):stop watchdog task for no seninf ctx:%d\n",
			 __func__, ctx->stream_id);
		return;
	}
	pipe_id = watchdog_data->pipe_id;
	watchdog_cnt = atomic_read(&watchdog_data->watchdog_cnt);

	spin_lock_irqsave(&ctx->watchdog_pipe_lock, flags);
	enabled_watchdog_pipe = ctx->enabled_watchdog_pipe;
	spin_unlock_irqrestore(&ctx->watchdog_pipe_lock, flags);

	if (!(enabled_watchdog_pipe & (1 << pipe_id))) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d):watchdog task(pipe_id:%d) is stopped, return\n",
			 __func__, ctx->stream_id, pipe_id);
		return;
	}

	raw = get_master_raw_dev(ctx->cam, ctx->pipe);
	timeout = mtk_cam_seninf_check_timeout(seninf,
					       watchdog_data->watchdog_time_diff_ns);
	if (last_vsync_count == raw->vsync_count)
		is_abnormal_vsync = true;
	last_vsync_count = raw->vsync_count;

	vf_en = 0;
	sof_count = 0;
	dequeued_frame_seq_no = 0;
	if (is_raw_subdev(pipe_id)) {
		dequeued_frame_seq_no = readl_relaxed(raw->base_inner + REG_FRAME_SEQ_NUM);
		vf_en = atomic_read(&raw->vf_en);
		sof_count = raw->sof_count;
	} else if (is_camsv_subdev(pipe_id)) {
		camsv_dev = mtk_cam_get_used_sv_dev(ctx);
		if (camsv_dev == NULL) {
			dev_info(ctx->cam->dev, "%s:ctx/pipe_id(%d/%d):camsv device not found\n",
				__func__, ctx->stream_id, pipe_id);
			return;
		}
		dequeued_frame_seq_no =
			readl_relaxed(camsv_dev->base_inner + REG_CAMSVCENTRAL_FRAME_SEQ_NO);
		vf_en = readl_relaxed(camsv_dev->base_inner + REG_CAMSVCENTRAL_VF_CON) &
			CAMSVCENTRAL_VF_CON_VFDATA_EN;
		sof_count = camsv_dev->sof_count;
	} else if (is_mraw_subdev(pipe_id)) {
		idx = pipe_id - MTKCAM_SUBDEV_MRAW_START;
		dev = ctx->cam->mraw.devs[idx];
		if (dev == NULL) {
			dev_info(ctx->cam->dev, "%s:ctx/pipe_id(%d/%d):mraw device not found\n",
				__func__, ctx->stream_id, pipe_id);
			return;
		}
		mraw_dev = dev_get_drvdata(dev);
		dequeued_frame_seq_no =
			readl_relaxed(mraw_dev->base_inner + REG_MRAW_FRAME_SEQ_NUM);
		vf_en = mtk_cam_mraw_is_vf_on(mraw_dev);
		sof_count = mraw_dev->sof_count;
	}

	/**
	 * Current we just call seninf dump, but it is better to check
	 * and restart the stream in the future.
	 */
	if (atomic_read(&watchdog_data->watchdog_dumped)) {
		dev_info(ctx->cam->dev,
			"%s:ctx/pipe_id(%d/%d):skip redundant seninf dump for no sof ctx:%d\n",
			__func__, pipe_id, ctx->stream_id);
	} else {
		if (timeout) {
			if (is_raw_subdev(pipe_id)) {
				dev_info(ctx->cam->dev,
					"%s:ctx/pipe_id(%d/%d): timeout, VF(%d) raw vsync count(%d) sof count(%d) raw overrun_debug_dump_cnt(%d) watchdog count(%d) start dump (+%lldms)\n",
					__func__, ctx->stream_id, pipe_id, vf_en,
					raw->vsync_count, sof_count, raw->overrun_debug_dump_cnt,
					watchdog_cnt, watchdog_data->watchdog_time_diff_ns/1000000);

				if (is_abnormal_vsync)
					dev_info(ctx->cam->dev, "abnormal vsync\n");
			} else {
				dev_info(ctx->cam->dev,
					"%s:ctx/pipe_id(%d/%d): timeout, VF(%d) raw vsync count(%d) sof count(%d) watchdog count(%d) start dump (+%lldms)\n",
					__func__, ctx->stream_id, pipe_id, vf_en,
					raw->vsync_count, sof_count,
					watchdog_cnt, watchdog_data->watchdog_time_diff_ns/1000000);
			}
			atomic_set(&watchdog_data->watchdog_dumped, 1); // fixme
			atomic_set(&watchdog_data->watchdog_cnt, 0);
			if (mtk_cam_seninf_dump(seninf, dequeued_frame_seq_no, true))
				mtk_cam_event_esd_recovery(ctx->pipe, ctx->dequeued_frame_seq_no);
			/* both reset are required */
			atomic_set(&watchdog_data->watchdog_cnt, 0);
			atomic_inc(&watchdog_data->watchdog_dump_cnt);
			atomic_set(&watchdog_data->watchdog_dumped, 0);

			if (!(is_raw_subdev(pipe_id)))
				return;
			watchdog_dump_cnt = atomic_read(
				&watchdog_data->watchdog_dump_cnt);
			watchdog_timeout_cnt = atomic_read(
				&watchdog_data->watchdog_timeout_cnt);
			if (watchdog_dump_cnt == watchdog_timeout_cnt) {
				int_en = readl_relaxed(raw->base + REG_CTL_RAW_INT_EN);
				if (raw->vsync_count == 0) {
					dev_info(ctx->cam->dev,
						"vsync count(%d), VF(%d), INT_EN(0x%x)\n",
						raw->vsync_count,
						atomic_read(&raw->vf_en),
						int_en);
					if (int_en == 0 && ctx->composed_frame_seq_no) {
						mtk_cam_event_error(ctx->pipe,
								    "Camsys: 1st CQ done timeout");
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
						aee_kernel_exception_api(
							__FILE__, __LINE__, DB_OPT_DEFAULT,
							"Camsys: 1st CQ done timeout",
							"watchdog timeout");
#else
						WARN_ON(1);
#endif
					} else {
						mtk_cam_event_error(ctx->pipe,
								    "Camsys: Vsync timeout");
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
						aee_kernel_exception_api(
							__FILE__, __LINE__, DB_OPT_DEFAULT,
							"Camsys: Vsync timeout",
							"watchdog timeout");
#else
						WARN_ON(1);
#endif
					}
				} else if (atomic_read(&raw->vf_en) == 0) {
					dev_info(ctx->cam->dev,
						"vsync count(%d), frame inner index(%d) INT_EN(0x%x)\n",
						raw->vsync_count,
						readl_relaxed(
							raw->base_inner + REG_FRAME_SEQ_NUM),
						readl_relaxed(
							raw->base + REG_CTL_RAW_INT_EN));
					mtk_cam_event_error(ctx->pipe,
							    "Camsys: VF timeout");
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
					aee_kernel_exception_api(
						__FILE__, __LINE__, DB_OPT_DEFAULT,
						"Camsys: VF timeout", "watchdog timeout");
#else
					WARN_ON(1);
#endif
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
					mtk_cam_event_error(ctx->pipe,
							    "Camsys: SOF timeout");
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
					aee_kernel_exception_api(
						__FILE__, __LINE__, DB_OPT_DEFAULT,
						"Camsys: SOF timeout", "watchdog timeout");
#else
					WARN_ON(1);
#endif
				}
			}
		} else {
			dev_info(ctx->cam->dev,
				"%s:ctx/pipe_id(%d/%d): not timeout, for long exp watchdog count(%d) (+%lldms)\n",
				__func__, ctx->stream_id, pipe_id, watchdog_cnt,
				watchdog_data->watchdog_time_diff_ns/1000000);
		}
	}
}

static void mtk_ctx_watchdog(struct timer_list *t)
{
	struct mtk_cam_ctx *ctx = from_timer(ctx, t, watchdog_timer);
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_raw_device *raw;
	struct device *dev;
	struct mtk_camsv_device *camsv_dev;
	struct mtk_mraw_device *mraw_dev;
	struct mtk_cam_watchdog_data *watchdog_data;
	int watchdog_cnt = 0;
	int watchdog_dump_cnt;
	u64 current_time_ns = ktime_get_boottime_ns();
	u64 cost_time_ms, timer_expires_ms;
	int sof_count = 0, is_vf_on = 0;
	int enabled_watchdog_pipe;
	unsigned int idx;
	int i;
	unsigned long flags;

	if (!ctx->streaming)
		return;

	raw = get_master_raw_dev(ctx->cam, ctx->pipe);
	if (!raw) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d):stop watchdog task for no raw ctx\n",
			 __func__, ctx->stream_id);
		return;
	}
	if (atomic_read(&raw->vf_en) == 0 &&
	    !(mtk_cam_ctx_has_raw(ctx) &&
	    mtk_cam_scen_is_ext_isp(&ctx->pipe->scen_active))) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d):vf_en = 0\n",
			 __func__, ctx->stream_id);
		return;
	}

	spin_lock_irqsave(&ctx->watchdog_pipe_lock, flags);
	enabled_watchdog_pipe = ctx->enabled_watchdog_pipe;
	spin_unlock_irqrestore(&ctx->watchdog_pipe_lock, flags);

	for (i = MTKCAM_SUBDEV_RAW_START; i < MTKCAM_SUBDEV_MAX; i++) {
		if (enabled_watchdog_pipe & (1 << i)) {
			watchdog_data = &ctx->watchdog_data[i];
			watchdog_cnt = atomic_inc_return(&watchdog_data->watchdog_cnt);
			watchdog_dump_cnt = atomic_read(&watchdog_data->watchdog_dump_cnt);
			if (is_raw_subdev(i)) {
				watchdog_data->watchdog_time_diff_ns =
					current_time_ns - raw->last_sof_time_ns;
				sof_count = raw->sof_count;
				is_vf_on = atomic_read(&raw->vf_en);
			} else if (is_camsv_subdev(i)) {
				camsv_dev = mtk_cam_get_used_sv_dev(ctx);
				if (camsv_dev == NULL) {
					dev_info(ctx->cam->dev, "%s:ctx/pipe_id(%d/%d):camsv device not found\n",
						__func__, ctx->stream_id, i);
					goto GET_DEV_FAILED;
				}
				watchdog_data->watchdog_time_diff_ns =
					current_time_ns - camsv_dev->last_sof_time_ns;
				sof_count = camsv_dev->sof_count;
				is_vf_on = readl_relaxed(
					camsv_dev->base_inner + REG_CAMSVCENTRAL_VF_CON) &
					CAMSVCENTRAL_VF_CON_VFDATA_EN;
			} else if (is_mraw_subdev(i)) {
				idx = watchdog_data->pipe_id - MTKCAM_SUBDEV_MRAW_START;
				dev = cam->mraw.devs[idx];
				if (dev == NULL) {
					dev_info(ctx->cam->dev, "%s:ctx/pipe_id(%d/%d):mraw %d device not found\n",
						__func__, ctx->stream_id, i, idx);
					goto GET_DEV_FAILED;
				}
				mraw_dev = dev_get_drvdata(dev);
				watchdog_data->watchdog_time_diff_ns =
					current_time_ns - mraw_dev->last_sof_time_ns;
				sof_count = mraw_dev->sof_count;
				is_vf_on = mtk_cam_mraw_is_vf_on(mraw_dev);
			}

			/** FIXME:
			 * redundant check watchdog_dump in worker
			 * the timeout of same period doesn't need to be dump again
			 */
			if (atomic_read(&watchdog_data->watchdog_dumped)) {
				dev_dbg(ctx->cam->dev,
					"%s:ctx/pipe_id(%d/%d):skip watchdog worker ctx:%d (worker is ongoing)\n",
					__func__, ctx->stream_id, i);
			} else if (watchdog_cnt
				>= atomic_read(&watchdog_data->watchdog_timeout_cnt)) {
				/**
				 * No raw's sof interrupts were generated by hw for the
				 * Nth time of running the watchdog timer.
				 */
				if (watchdog_dump_cnt < 4) {
					dev_info_ratelimited(ctx->cam->dev, "%s:ctx/pipe_id(%d/%d): timeout! VF(%d) raw vsync count(%d) sof count(%d) watchdog_cnt(%d)(+%lldms)\n",
						__func__, ctx->stream_id, i, is_vf_on,
						raw->vsync_count, sof_count, watchdog_cnt,
						watchdog_data->watchdog_time_diff_ns/1000000);
					schedule_work(&watchdog_data->watchdog_work);
				} else {
					dev_info_ratelimited(ctx->cam->dev, "%s:ctx/pipe_id(%d/%d): dump > 3! watchdog_dump_cnt(%d)(+%lldms)\n",
						__func__, ctx->stream_id, i, watchdog_dump_cnt,
						watchdog_data->watchdog_time_diff_ns/1000000);
				}
			}
		}
	}

GET_DEV_FAILED:
	cost_time_ms = (ktime_get_boottime_ns() - current_time_ns)/1000000;
	timer_expires_ms = MTK_CAM_CTX_WATCHDOG_INTERVAL - cost_time_ms;
	ctx->watchdog_timer.expires = jiffies +
				msecs_to_jiffies(timer_expires_ms);
	dev_dbg(ctx->cam->dev, "%s:check ctx(%d): dog_cnt(%d), dog_time:%dms\n",
				__func__, ctx->stream_id, watchdog_cnt,
				timer_expires_ms);
	add_timer(&ctx->watchdog_timer);
}

void mtk_ctx_watchdog_kick(struct mtk_cam_ctx *ctx, int pipe_id)
{
	struct mtk_cam_watchdog_data *watchdog_data;
	int enabled_watchdog_pipe;

	/* Only read, don't use lock to prevent irq thread pending */
	enabled_watchdog_pipe = ctx->enabled_watchdog_pipe;

	if (!(enabled_watchdog_pipe & (1 << pipe_id))) {
		dev_info_ratelimited(ctx->cam->dev, "%s:ctx/pipe_id(%d/%d): kick target is not enabled\n",
			__func__, ctx->stream_id, pipe_id);
		return;
	}

	watchdog_data = &ctx->watchdog_data[pipe_id];

	dev_dbg(ctx->cam->dev, "%s:ctx/pipe_id(%d/%d): watchdog_cnt(%d)\n",
		__func__, ctx->stream_id, pipe_id,
		atomic_read(&watchdog_data->watchdog_cnt));
	atomic_set(&watchdog_data->watchdog_cnt, 0);
	atomic_set(&watchdog_data->watchdog_dump_cnt, 0);
}

static void mtk_ctx_watchdog_init(struct mtk_cam_ctx *ctx)
{
	int i;

	for (i = 0 ; i < MTKCAM_SUBDEV_MAX ; i++) {
		INIT_WORK(&ctx->watchdog_data[i].watchdog_work,
			mtk_cam_ctx_watchdog_worker);
		ctx->watchdog_data[i].pipe_id = i;
	}
	timer_setup(&ctx->watchdog_timer, mtk_ctx_watchdog, 0);
}

void mtk_ctx_watchdog_start(struct mtk_cam_ctx *ctx, int timeout_cnt, int pipe_id)
{
	struct mtk_cam_watchdog_data *watchdog_data = &ctx->watchdog_data[pipe_id];
	int enabled_watchdog_pipe;
	int is_timer_add = 0;
	unsigned long flags;
	int raw_pipe_mask = 0;
	int i;

	for (i = MTKCAM_SUBDEV_RAW_START; i < MTKCAM_SUBDEV_RAW_END; i++)
		raw_pipe_mask |= (1 << i);

	spin_lock_irqsave(&ctx->watchdog_pipe_lock, flags);
	enabled_watchdog_pipe = ctx->enabled_watchdog_pipe;
	spin_unlock_irqrestore(&ctx->watchdog_pipe_lock, flags);

	if (enabled_watchdog_pipe & (1 << pipe_id))
		return;

	dev_info(ctx->cam->dev, "%s:ctx/pipe_id(%d/%d):start the watchdog, timeout setting(%d)\n",
		__func__, ctx->stream_id, pipe_id,
		MTK_CAM_CTX_WATCHDOG_INTERVAL * timeout_cnt);

	atomic_set(&watchdog_data->watchdog_timeout_cnt, timeout_cnt);
	atomic_set(&watchdog_data->watchdog_cnt, 0);
	atomic_set(&watchdog_data->watchdog_dumped, 0);
	atomic_set(&watchdog_data->watchdog_dump_cnt, 0);

	spin_lock_irqsave(&ctx->watchdog_pipe_lock, flags);
	/* Start timer when the first raw watchdog start */
	if (is_raw_subdev(pipe_id) && !(ctx->enabled_watchdog_pipe & raw_pipe_mask))
		is_timer_add = 1;
	watchdog_data->ctx = ctx;
	ctx->enabled_watchdog_pipe |= (1 << pipe_id);
	spin_unlock_irqrestore(&ctx->watchdog_pipe_lock, flags);

	if (is_timer_add) {
		ctx->watchdog_timer.expires = jiffies +
			msecs_to_jiffies(MTK_CAM_CTX_WATCHDOG_INTERVAL);
		add_timer(&ctx->watchdog_timer);
	}
}

void mtk_ctx_watchdog_stop(struct mtk_cam_ctx *ctx, int pipe_id)
{
	struct mtk_cam_watchdog_data *watchdog_data = &ctx->watchdog_data[pipe_id];
	int enabled_watchdog_pipe;
	int is_timer_delete = 0;
	unsigned long flags;
	int raw_pipe_mask = 0;
	int i;

	for (i = MTKCAM_SUBDEV_RAW_START; i < MTKCAM_SUBDEV_RAW_END; i++)
		raw_pipe_mask |= (1 << i);

	spin_lock_irqsave(&ctx->watchdog_pipe_lock, flags);
	enabled_watchdog_pipe = ctx->enabled_watchdog_pipe;
	spin_unlock_irqrestore(&ctx->watchdog_pipe_lock, flags);

	if (!(enabled_watchdog_pipe & (1 << pipe_id)))
		return;

	dev_info(ctx->cam->dev, "%s:ctx/pipe_id(%d/%d):stop the watchdog\n",
		__func__, ctx->stream_id, pipe_id);

	spin_lock_irqsave(&ctx->watchdog_pipe_lock, flags);
	ctx->enabled_watchdog_pipe &= ~(1 << pipe_id);
	watchdog_data->ctx = NULL;
	/* Stop timer when the last raw watchdog stop */
	if (is_raw_subdev(pipe_id) && !(ctx->enabled_watchdog_pipe & raw_pipe_mask))
		is_timer_delete = 1;
	spin_unlock_irqrestore(&ctx->watchdog_pipe_lock, flags);

	if (is_timer_delete)
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
	ctx->is_first_cq_done = 0;
	ctx->cq_done_status = 0;
	ctx->session_created = 0;

	ctx->used_sv_num = 0;
	for (i = 0; i < MAX_SV_HW_TAGS; i++)
		ctx->sv_dequeued_frame_seq_no[i] = 0;
	for (i = 0; i < MAX_SV_PIPES_PER_STREAM; i++) {
		ctx->sv_pipe[i] = NULL;
	}
	ctx->sv_using_buffer_list.cnt = 0;
	ctx->sv_composed_buffer_list.cnt = 0;
	ctx->sv_processing_buffer_list.cnt = 0;

	ctx->used_mraw_num = 0;
	for (i = 0 ; i < MAX_MRAW_PIPES_PER_STREAM ; i++) {
		ctx->mraw_pipe[i] = NULL;
		ctx->used_mraw_dev[i] = 0;
		ctx->mraw_dequeued_frame_seq_no[i] = 0;
		ctx->mraw_using_buffer_list[i].cnt = 0;
		ctx->mraw_composed_buffer_list[i].cnt = 0;
		ctx->mraw_processing_buffer_list[i].cnt = 0;
	}

	INIT_LIST_HEAD(&ctx->using_buffer_list.list);
	INIT_LIST_HEAD(&ctx->composed_buffer_list.list);
	INIT_LIST_HEAD(&ctx->processing_buffer_list.list);
	INIT_LIST_HEAD(&ctx->sv_using_buffer_list.list);
	INIT_LIST_HEAD(&ctx->sv_composed_buffer_list.list);
	INIT_LIST_HEAD(&ctx->sv_processing_buffer_list.list);
	for (i = 0; i < MAX_MRAW_PIPES_PER_STREAM; i++) {
		INIT_LIST_HEAD(&ctx->mraw_using_buffer_list[i].list);
		INIT_LIST_HEAD(&ctx->mraw_composed_buffer_list[i].list);
		INIT_LIST_HEAD(&ctx->mraw_processing_buffer_list[i].list);
	}
	INIT_LIST_HEAD(&ctx->processing_img_buffer_list.list);
	spin_lock_init(&ctx->using_buffer_list.lock);
	spin_lock_init(&ctx->composed_buffer_list.lock);
	spin_lock_init(&ctx->processing_buffer_list.lock);
	spin_lock_init(&ctx->sv_using_buffer_list.lock);
	spin_lock_init(&ctx->sv_composed_buffer_list.lock);
	spin_lock_init(&ctx->sv_processing_buffer_list.lock);
	for (i = 0; i < MAX_MRAW_PIPES_PER_STREAM; i++) {
		spin_lock_init(&ctx->mraw_using_buffer_list[i].lock);
		spin_lock_init(&ctx->mraw_composed_buffer_list[i].lock);
		spin_lock_init(&ctx->mraw_processing_buffer_list[i].lock);
	}
	spin_lock_init(&ctx->streaming_lock);
	spin_lock_init(&ctx->first_cq_lock);
	spin_lock_init(&ctx->processing_img_buffer_list.lock);
	spin_lock_init(&ctx->watchdog_pipe_lock);
	mutex_init(&ctx->cleanup_lock);
	spin_lock(&ctx->processing_buffer_list.lock);
	ctx->processing_buffer_list.cnt = 0;
	spin_unlock(&ctx->processing_buffer_list.lock);
	spin_lock(&ctx->composed_buffer_list.lock);
	ctx->composed_buffer_list.cnt = 0;
	spin_unlock(&ctx->composed_buffer_list.lock);
	mtk_ctx_watchdog_init(ctx);
}

static int mtk_cam_v4l2_subdev_link_validate(struct v4l2_subdev *sd,
				      struct media_link *link,
				      struct v4l2_subdev_format *source_fmt,
				      struct v4l2_subdev_format *sink_fmt,
				      bool bypass_size_check)
{
	bool pass = true;

	/* The width, height and code must match. */
	if (source_fmt->format.width != sink_fmt->format.width) {
		dev_dbg(sd->entity.graph_obj.mdev->dev,
			"%s: width does not match (source %u, sink %u)\n",
			__func__,
			source_fmt->format.width, sink_fmt->format.width);
		pass = (bypass_size_check) ? true : false;
	}

	if (source_fmt->format.height != sink_fmt->format.height) {
		dev_dbg(sd->entity.graph_obj.mdev->dev,
			"%s: height does not match (source %u, sink %u)\n",
			__func__,
			source_fmt->format.height, sink_fmt->format.height);
		pass = (bypass_size_check) ? true : false;
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

	ret = mtk_cam_v4l2_subdev_link_validate(sd, link, source_fmt, sink_fmt, false);
	if (ret)
		dev_info(dev, "%s: link validate failed pad/code/w/h: SRC(%d/0x%x/%d/%d), SINK(%d:0x%x/%d/%d)\n",
			 sd->name, source_fmt->pad, source_fmt->format.code,
			 source_fmt->format.width, source_fmt->format.height,
			 sink_fmt->pad, sink_fmt->format.code,
			 sink_fmt->format.width, sink_fmt->format.height);

	return ret;
}

int mtk_cam_seninf_link_validate(struct v4l2_subdev *sd,
			  struct media_link *link,
			  struct v4l2_subdev_format *source_fmt,
			  struct v4l2_subdev_format *sink_fmt)
{
	struct device *dev;
	int ret = 0;

	dev = sd->v4l2_dev->dev;

	ret = mtk_cam_v4l2_subdev_link_validate(sd, link, source_fmt, sink_fmt, true);
	if (ret)
		dev_info(dev, "%s: link validate failed pad/code/w/h: SRC(%d/0x%x/%d/%d), SINK(%d:0x%x/%d/%d)\n",
			 sd->name, source_fmt->pad, source_fmt->format.code,
			 source_fmt->format.width, source_fmt->format.height,
			 sink_fmt->pad, sink_fmt->format.code,
			 sink_fmt->format.width, sink_fmt->format.height);

	return ret;
}

int mtk_cam_sv_link_validate(struct v4l2_subdev *sd,
			  struct media_link *link,
			  struct v4l2_subdev_format *source_fmt,
			  struct v4l2_subdev_format *sink_fmt)
{
	struct device *dev;
	int ret = 0;

	dev = sd->v4l2_dev->dev;

	ret = mtk_cam_v4l2_subdev_link_validate(sd, link, source_fmt, sink_fmt, true);
	if (ret)
		dev_info(dev, "%s: link validate failed pad/code/w/h: SRC(%d/0x%x/%d/%d), SINK(%d:0x%x/%d/%d)\n",
			 sd->name, source_fmt->pad, source_fmt->format.code,
			 source_fmt->format.width, source_fmt->format.height,
			 sink_fmt->pad, sink_fmt->format.code,
			 sink_fmt->format.width, sink_fmt->format.height);

	return ret;
}

int mtk_cam_mraw_link_validate(struct v4l2_subdev *sd,
			  struct media_link *link,
			  struct v4l2_subdev_format *source_fmt,
			  struct v4l2_subdev_format *sink_fmt)
{
	struct device *dev;
	int ret = 0;

	dev = sd->v4l2_dev->dev;

	ret = mtk_cam_v4l2_subdev_link_validate(sd, link, source_fmt, sink_fmt, true);
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
			    GET_PLAT_V4L2(meta_cfg_size_rgbw) +
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
	return ret;
}

static irqreturn_t mtk_irq_adl(int irq, void *data)
{
	struct mtk_cam_device *drvdata = (struct mtk_cam_device *)data;
	struct device *dev = drvdata->dev;

	unsigned int irq_status;

	irq_status =
		readl_relaxed(drvdata->adl_base + 0x18a0);

	dev_dbg(dev, "ADL-INT: INT 0x%x\n", irq_status);

	return IRQ_HANDLED;
}


static int mtk_cam_probe(struct platform_device *pdev)
{
	struct mtk_cam_device *cam_dev;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret;
	unsigned int i;
	const struct camsys_platform_data *platform_data;
	int irq;

	platform_data = of_device_get_match_data(dev);
	if (!platform_data) {
		dev_info(dev, "Error: failed to get match data\n");
		return -ENODEV;
	}
	set_platform_data(platform_data);
	dev_info(dev, "platform = %s\n", platform_data->platform);

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

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_info(dev, "failed to get irq\n");
		return -ENODEV;
	}

	ret = devm_request_irq(dev, irq, mtk_irq_adl, 0,
			       dev_name(dev), cam_dev);
	if (ret) {
		dev_info(dev, "failed to request irq=%d\n", irq);
		return ret;
	}
	dev_dbg(dev, "registered adl irq=%d\n", irq);
	disable_irq(irq);

	cam_dev->dev = dev;
	dev_set_drvdata(dev, cam_dev);

	cam_dev->composer_cnt = 0;
	cam_dev->num_seninf_drivers = 0;

	/* FIXME: decide max raw stream num by seninf num */
	cam_dev->max_stream_num = MTKCAM_SUBDEV_MAX;
	cam_dev->ctxs = devm_kcalloc(dev, cam_dev->max_stream_num,
				     sizeof(*cam_dev->ctxs), GFP_KERNEL);

	cam_dev->cmdq_clt = cmdq_mbox_create(dev, 0);

	if (!cam_dev->cmdq_clt)
		pr_info("probe cmdq_mbox_create fail\n");
	else
		pr_info("probe cmdq_mbox_create: client: %d\n", cam_dev->cmdq_clt);

	cam_dev->adl_base = ioremap(0x1a0f0000, 0x1900);

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
	mutex_init(&cam_dev->dvfs_op_lock);

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

	if (cam_dev->cmdq_clt)
		cmdq_mbox_destroy(cam_dev->cmdq_clt);

	mutex_destroy(&cam_dev->queue_lock);
	mtk_cam_debug_fs_deinit(cam_dev);

	destroy_workqueue(cam_dev->link_change_wq);

	platform_driver_unregister(&mtk_cam_mraw_driver);
	platform_driver_unregister(&mtk_cam_sv_driver);
	platform_driver_unregister(&mtk_cam_raw_driver);
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

static int __init mtk_cam_init(void)
{
	int ret;

	ret = platform_driver_register(&mtk_cam_driver);
	return ret;
}

static void __exit mtk_cam_exit(void)
{
	platform_driver_unregister(&mtk_cam_driver);
}

module_init(mtk_cam_init);
module_exit(mtk_cam_exit);

MODULE_DESCRIPTION("Camera ISP driver");
MODULE_LICENSE("GPL");
