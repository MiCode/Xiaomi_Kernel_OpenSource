// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/component.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <linux/platform_data/mtk_ccd.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/rpmsg/mtk_ccd_rpmsg.h>
#include <linux/mtk_ccd_controls.h>

#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/media.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-subdev.h>
#include <media/media-entity.h>

#include <trace/hooks/v4l2core.h>
#include <trace/hooks/v4l2mc.h>

#include "mtk_cam.h"
#include "mtk_cam-ctrl.h"
#include "mtk_cam_pm.h"
#include "mtk_cam-dvfs_qos.h"
#include "mtk_cam-meta.h"
#include "mtk_cam-pool.h"
#include "mtk_cam-regs.h"
#include "mtk_cam-smem.h"
#include "mtk_camera-v4l2-controls.h"
#include "mtk_camera-videodev2.h"

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

void mtk_cam_dev_job_done(struct mtk_cam_ctx *ctx,
			  struct mtk_cam_request *req,
			  int pipe_id,
			  enum vb2_buffer_state state)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_buffer *buf, *buf_prev;
	struct mtk_camsys_ctrl_state *req_state;
	struct mtk_cam_request_stream_data *req_stream_data_pipe;
	struct mtk_cam_request_stream_data *req_stream_data;

	req_stream_data_pipe = &req->stream_data[pipe_id];
	req_stream_data = &req->stream_data[ctx->stream_id];
	dev_dbg(cam->dev,
		 "%s: job done, req:%d, state:%d, ctx:(%d,0x%x), pipe:(%d,0x%x), done_status:0x%x\n",
		 req->req.debug_str, req_stream_data->frame_seq_no, state,
		 ctx->stream_id, req->ctx_used, pipe_id, req->pipe_used,
		 req->done_status);

	if (!(cam->streaming_pipe & (1 << pipe_id))) {
		dev_dbg(cam->dev,
			"%s:%s: skip stream off pipe req:%d, ctx:(%d,0x%x), pipe:(%d,0x%x)\n",
			__func__, req->req.debug_str,
			req_stream_data->frame_seq_no, ctx->stream_id,
			req->ctx_used, pipe_id, req->pipe_used);
		return;
	}

	spin_lock(&req_stream_data_pipe->bufs_lock);
	list_for_each_entry_safe(buf, buf_prev, &req_stream_data_pipe->bufs, stream_data_list) {
		struct vb2_buffer *vb;
		struct mtk_cam_video_device *node;

		vb = &buf->vbb.vb2_buf;
		node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
		if (node->uid.pipe_id != pipe_id) {
			dev_info(cam->dev,
				 "%s:%s:node(%s): invalid pipe id (%d), should be (%d)\n",
				 __func__, req->req.debug_str,
				 node->desc.name, node->uid.pipe_id, pipe_id);
			continue;
		}

		list_del(&buf->stream_data_list);
		spin_lock(&node->buf_list_lock);
		list_del(&buf->list);
		spin_unlock(&node->buf_list_lock);

		buf->vbb.sequence = req_stream_data->frame_seq_no;
		if (vb->vb2_queue->timestamp_flags &
		    V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC)
			vb->timestamp = req_stream_data->timestamp_mono;
		else
			vb->timestamp = req_stream_data->timestamp;
		vb2_buffer_done(&buf->vbb.vb2_buf, state);
	}
	spin_unlock(&req_stream_data_pipe->bufs_lock);

	req_state = &req_stream_data->state;
	req_state->time_deque = ktime_get_boottime_ns() / 1000;
	if (mtk_cam_is_subsample(ctx))
		dev_dbg(cam->dev, "[ctx:%d,0x%x/pipe:%d,0x%x/%4d(us)] %6lld,%6lld,%6lld,%6lld,%6lld,%6lld,%6lld,%6lld,%6lld\n",
		ctx->stream_id, req->ctx_used, pipe_id, req->pipe_used,
		req_stream_data->frame_seq_no,
		req_state->time_composing -	req->time_syscall_enque,
		req_state->time_swirq_composed - req_state->time_composing,
		req_state->time_sensorset -	req_state->time_irq_sof1,
		req_state->time_irq_sof2 - req_state->time_sensorset,
		req_state->time_cqset -	req_state->time_irq_sof1,
		req_state->time_irq_outer -	req_state->time_cqset,
		req_state->time_irq_sof2 - req_state->time_irq_sof1,
		req_state->time_irq_done - req_state->time_irq_sof2,
		req_state->time_deque -	req_state->time_irq_done);
	else
		dev_dbg(cam->dev, "[ctx:%d,0x%x/pipe:%d,0x%x/%4d(us)] %6lld,%6lld,%6lld,%6lld,%6lld,%6lld,%6lld,%6lld,%6lld\n",
		ctx->stream_id, req->ctx_used, pipe_id, req->pipe_used,
		req_stream_data->frame_seq_no,
		req_state->time_composing -	req->time_syscall_enque,
		req_state->time_swirq_composed - req_state->time_composing,
		req_state->time_sensorset -	req_state->time_swirq_timer,
		req_state->time_irq_sof1 - req_state->time_sensorset,
		req_state->time_cqset -	req_state->time_irq_sof1,
		req_state->time_irq_outer -	req_state->time_cqset,
		req_state->time_irq_sof2 - req_state->time_irq_sof1,
		req_state->time_irq_done - req_state->time_irq_sof2,
		req_state->time_deque -	req_state->time_irq_done);
}

struct mtk_cam_request *mtk_cam_dev_get_req(struct mtk_cam_device *cam,
					    struct mtk_cam_ctx *ctx,
					    unsigned int frame_seq_no)
{
	struct mtk_cam_request *req, *req_prev;
	struct mtk_cam_request_stream_data *req_stream_data;
	unsigned long flags;

	spin_lock_irqsave(&cam->running_job_lock, flags);
	list_for_each_entry_safe(req, req_prev, &cam->running_job_list, list) {
		req_stream_data = &req->stream_data[ctx->stream_id];

		/* Match by the en-queued request number */
		if (req->ctx_used & (1 << ctx->stream_id) &&
		    req_stream_data->frame_seq_no == frame_seq_no) {
			spin_unlock_irqrestore(&cam->running_job_lock, flags);
			return req;
		}
	}
	spin_unlock_irqrestore(&cam->running_job_lock, flags);

	return NULL;
}

static bool finish_cq_buf(struct mtk_cam_ctx *ctx, struct mtk_cam_request *req)
{
	bool result = false;
	struct mtk_cam_working_buf_entry *cq_buf_entry, *cq_buf_entry_prev;

	if (!ctx->used_raw_num)
		return false;

	spin_lock(&ctx->processing_buffer_list.lock);
	list_for_each_entry_safe(cq_buf_entry, cq_buf_entry_prev,
				 &ctx->processing_buffer_list.list,
				 list_entry) {
		if (cq_buf_entry->req == req) {
			list_del(&cq_buf_entry->list_entry);
			cq_buf_entry->req = NULL;
			mtk_cam_working_buf_put(ctx, cq_buf_entry);
			ctx->processing_buffer_list.cnt--;
			result = true;
			dev_dbg(ctx->cam->dev, "put cq buf:%pad, %s\n",
				&cq_buf_entry->buffer.iova, req->req.debug_str);
			break;
		}
	}
	spin_unlock(&ctx->processing_buffer_list.lock);

	return result;
}

static bool finish_img_buf(struct mtk_cam_ctx *ctx, struct mtk_cam_request *req)
{
	bool result = false;
	struct mtk_cam_img_working_buf_entry *buf_entry, *buf_entry_prev;

	if (!ctx->used_raw_num)
		return false;
	if (ctx->processing_img_buffer_list.cnt == 0)
		return false;

	spin_lock(&ctx->processing_img_buffer_list.lock);
	list_for_each_entry_safe(buf_entry, buf_entry_prev,
				 &ctx->processing_img_buffer_list.list,
				 list_entry) {
		if (buf_entry->req == req) {
			list_del(&buf_entry->list_entry);
			buf_entry->req = NULL;
			mtk_cam_img_working_buf_put(ctx, buf_entry);
			ctx->processing_img_buffer_list.cnt--;
			result = true;
			dev_dbg(ctx->cam->dev, "[%s] iova:0x%x\n",
				__func__, buf_entry->img_buffer.iova);
		}
	}
	spin_unlock(&ctx->processing_img_buffer_list.lock);

	return result;
}

static bool mtk_cam_update_done_status(struct mtk_cam_ctx *ctx,
					 struct mtk_cam_request *req, int pipe_id)
{
	int ret = false;
	unsigned int done_status;

	spin_lock(&req->done_status_lock);
	req->done_status |= 1 << pipe_id;
	done_status = req->done_status;
	spin_unlock(&req->done_status_lock);

	/* Check whether all pipelines of single ctx are done */
	if ((done_status & ctx->streaming_pipe) == ctx->streaming_pipe)
		ret = true;

	return ret;
}

static bool mtk_cam_del_req_from_running(struct mtk_cam_ctx *ctx,
					 struct mtk_cam_request *req, int pipe_id)
{
	int ret = false;
	unsigned int done_status;
	struct mtk_cam_request_stream_data *req_stream_data = &req->stream_data[ctx->stream_id];

	spin_lock(&req->done_status_lock);
	done_status = req->done_status;
	spin_unlock(&req->done_status_lock);

	/* Check if the frames of all the streams is done */
	if ((done_status & ctx->cam->streaming_pipe) ==
	    (req->pipe_used & ctx->cam->streaming_pipe)) {
		list_del(&req->list);
		ctx->cam->running_job_count--;
		ret = true;
		dev_dbg(ctx->cam->dev,
			"%s: %s: removed, req:%d, ctx:(%d/0x%x/0x%x), pipe:(%d/0x%x/0x%x) done_status:0x%x]\n",
			__func__, req->req.debug_str, req_stream_data->frame_seq_no,
			ctx->stream_id, req->ctx_used, ctx->cam->streaming_ctx,
			pipe_id, req->pipe_used, ctx->cam->streaming_pipe,
			req->done_status);
		media_request_put(&req->req);
	}

	return ret;
}

bool mtk_cam_dequeue_req_frame(struct mtk_cam_ctx *ctx,
			       unsigned int dequeued_frame_seq_no,
			       int pipe_id)
{
	struct mtk_cam_request *req, *req_prev;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	unsigned long flags;
	enum vb2_buffer_state buf_state = VB2_BUF_STATE_DONE;
	bool result = false;

	spin_lock_irqsave(&ctx->cam->running_job_lock, flags);
	list_for_each_entry_safe(req, req_prev, &ctx->cam->running_job_list, list) {
		req_stream_data = &req->stream_data[ctx->stream_id];
		dev_dbg(ctx->cam->dev,
			"frame_seq:%d[ctx=%d,pipe=%d,ctx_used=%x], de-queue frame_seq:%d\n",
			req_stream_data->frame_seq_no, ctx->stream_id,
			pipe_id, req->ctx_used, dequeued_frame_seq_no);

		if (!(req->pipe_used & (1 << pipe_id)))
			continue;

		/* Match by the en-queued request number */
		if (req_stream_data->frame_seq_no == dequeued_frame_seq_no) {
			/**
			 * For request not removed since there is another ctx
			 * in previous mtk_cam_dequeue_req_frame() but can be
			 * removed since another ctx is stream off cases.
			 */
			spin_lock(&req->done_status_lock);
			if (req->done_status & 1 << pipe_id) {
				spin_unlock(&req->done_status_lock);
				result = mtk_cam_del_req_from_running(ctx, req, pipe_id);
				break;
			}
			spin_unlock(&req->done_status_lock);

			if (mtk_camsv_is_sv_pipe(pipe_id)) {
				mtk_cam_sv_finish_buf(ctx, req);
			} else {
				finish_cq_buf(ctx, req);
				if (mtk_cam_is_stagger(ctx) || mtk_cam_is_time_shared(ctx))
					finish_img_buf(ctx, req);
				if (req_stream_data->state.estate == E_STATE_DONE_MISMATCH)
					buf_state = VB2_BUF_STATE_ERROR;
			}

			if (mtk_cam_update_done_status(ctx, req, pipe_id))
				mtk_camsys_state_delete(ctx, sensor_ctrl, req);
			result = mtk_cam_del_req_from_running(ctx, req, pipe_id);
			mtk_cam_dev_job_done(ctx, req, pipe_id, buf_state);
			break;
		} else if (req_stream_data->frame_seq_no < dequeued_frame_seq_no) {
			/**
			 * For request not removed since there is another ctx
			 * in previous mtk_cam_dequeue_req_frame() but can be
			 * removed since another ctx is stream off cases.
			 */
			spin_lock(&req->done_status_lock);
			if (req->done_status & 1 << pipe_id) {
				spin_unlock(&req->done_status_lock);
				result = mtk_cam_del_req_from_running(ctx, req, pipe_id);
				dev_dbg(ctx->cam->dev,
					"req:%d, time:%lld drop, ctx:%d, pipe:%d\n",
					req_stream_data->frame_seq_no, req_stream_data->timestamp,
					ctx->stream_id, pipe_id);
				continue;
			}
			spin_unlock(&req->done_status_lock);

			if (mtk_camsv_is_sv_pipe(pipe_id)) {
				mtk_cam_sv_finish_buf(ctx, req);
			} else {
				finish_cq_buf(ctx, req);
				if (mtk_cam_is_stagger(ctx) || mtk_cam_is_time_shared(ctx))
					finish_img_buf(ctx, req);
			}

			if (mtk_cam_update_done_status(ctx, req, pipe_id))
				mtk_camsys_state_delete(ctx, sensor_ctrl, req);
			result = mtk_cam_del_req_from_running(ctx, req, pipe_id);
			dev_dbg(ctx->cam->dev, "req:%d, time:%lld drop, ctx:%d, pipe:%d\n",
				req_stream_data->frame_seq_no, req_stream_data->timestamp,
				ctx->stream_id, pipe_id);
			mtk_cam_dev_job_done(ctx, req, pipe_id, VB2_BUF_STATE_ERROR);
		}
	}
	spin_unlock_irqrestore(&ctx->cam->running_job_lock, flags);

	return result;
}

void mtk_cam_dev_req_cleanup(struct mtk_cam_device *cam)
{
	struct mtk_cam_request *req, *req_prev;
	unsigned long flags;
	struct list_head *pending = &cam->pending_job_list;
	struct list_head *running = &cam->running_job_list;

	spin_lock_irqsave(&cam->pending_job_lock, flags);
	list_for_each_entry_safe(req, req_prev, pending, list) {
		if (!(req->pipe_used & cam->streaming_pipe)) {
			list_del(&req->list);
			media_request_put(&req->req);
		}
	}
	spin_unlock_irqrestore(&cam->pending_job_lock, flags);

	spin_lock_irqsave(&cam->running_job_lock, flags);
	list_for_each_entry_safe(req, req_prev, running, list) {
		if (!(req->pipe_used & cam->streaming_pipe)) {
			list_del(&req->list);
			media_request_put(&req->req);
		}
	}
	spin_unlock_irqrestore(&cam->running_job_lock, flags);

	dev_dbg(cam->dev,
		"%s: cleanup all stream off req, streaming ctx:0x%x, streaming pipe:0x%x)\n",
		__func__, cam->streaming_ctx, cam->streaming_pipe);
}
static void config_img_in_fmt_stagger(struct mtk_cam_device *cam,
				struct mtk_cam_request_stream_data *req,
				struct mtk_cam_video_device *node,
				enum hdr_scenario_id scenario)
{
	const struct v4l2_format *cfg_fmt = &node->active_fmt;
	struct mtkcam_ipi_img_input *in_fmt;
	int input_node;
	int rawi_port_num = 0;
	int rawi_idx = 0;
	const int *ptr_rawi = NULL;
	static const int stagger_onthefly_2exp_rawi[1] = {
					MTKCAM_IPI_RAW_RAWI_2};
	static const int stagger_onthefly_3exp_rawi[2] = {
					MTKCAM_IPI_RAW_RAWI_2, MTKCAM_IPI_RAW_RAWI_3};
	static const int stagger_m2m_2exp_rawi[2] =	{
					MTKCAM_IPI_RAW_RAWI_2, MTKCAM_IPI_RAW_RAWI_6};
	static const int stagger_m2m_3exp_rawi[3] = {
					MTKCAM_IPI_RAW_RAWI_2, MTKCAM_IPI_RAW_RAWI_3,
					MTKCAM_IPI_RAW_RAWI_6};

	if (node->desc.dma_port != MTKCAM_IPI_RAW_IMGO &&
		node->desc.dma_port != MTKCAM_IPI_RAW_RAWI_2)
		return;

	if (scenario == STAGGER_ON_THE_FLY) {
		if (mtk_cam_is_2_exposure(req->state.ctx)) {
			input_node = MTKCAM_IPI_RAW_RAWI_2;
			in_fmt = &req->frame_params.img_ins[input_node - MTKCAM_IPI_RAW_RAWI_2];
			in_fmt->uid.id = input_node;
			in_fmt->uid.pipe_id = node->uid.pipe_id;
			in_fmt->fmt.format = mtk_cam_get_img_fmt(cfg_fmt->fmt.pix_mp.pixelformat);
			in_fmt->fmt.s.w = cfg_fmt->fmt.pix_mp.width;
			in_fmt->fmt.s.h = cfg_fmt->fmt.pix_mp.height;
			in_fmt->fmt.stride[0] = cfg_fmt->fmt.pix_mp.plane_fmt[0].bytesperline;
			dev_dbg(cam->dev,
			"[stagger-1/2Exp] ctx: %d dma_port:%d size=%dx%d, stride:%d, fmt:0x%x (0x%x/0x%x)\n",
			req->state.ctx->stream_id, input_node, in_fmt->fmt.s.w,
			in_fmt->fmt.s.h, in_fmt->fmt.stride[0], in_fmt->fmt.format,
			in_fmt->buf[0].iova,
			req->frame_params.img_outs[0].buf[0][0].iova);
		} else if (mtk_cam_is_3_exposure(req->state.ctx)) {
			input_node = MTKCAM_IPI_RAW_RAWI_2;
			in_fmt = &req->frame_params.img_ins[input_node - MTKCAM_IPI_RAW_RAWI_2];
			in_fmt->uid.id = input_node;
			in_fmt->uid.pipe_id = node->uid.pipe_id;
			in_fmt->fmt.format = mtk_cam_get_img_fmt(cfg_fmt->fmt.pix_mp.pixelformat);
			in_fmt->fmt.s.w = cfg_fmt->fmt.pix_mp.width;
			in_fmt->fmt.s.h = cfg_fmt->fmt.pix_mp.height;
			in_fmt->fmt.stride[0] = cfg_fmt->fmt.pix_mp.plane_fmt[0].bytesperline;
			dev_dbg(cam->dev,
			"[stagger-1/3Exp] ctx: %d dma_port:%d size=%dx%d, stride:%d, fmt:0x%x (iova:0x%x)\n",
			req->state.ctx->stream_id, input_node, in_fmt->fmt.s.w,
			in_fmt->fmt.s.h, in_fmt->fmt.stride[0], in_fmt->fmt.format,
			in_fmt->buf[0].iova);
			input_node = MTKCAM_IPI_RAW_RAWI_3;
			in_fmt = &req->frame_params.img_ins[input_node - MTKCAM_IPI_RAW_RAWI_2];
			in_fmt->uid.id = input_node;
			in_fmt->uid.pipe_id = node->uid.pipe_id;
			in_fmt->fmt.format = mtk_cam_get_img_fmt(cfg_fmt->fmt.pix_mp.pixelformat);
			in_fmt->fmt.s.w = cfg_fmt->fmt.pix_mp.width;
			in_fmt->fmt.s.h = cfg_fmt->fmt.pix_mp.height;
			in_fmt->fmt.stride[0] = cfg_fmt->fmt.pix_mp.plane_fmt[0].bytesperline;
			dev_dbg(cam->dev,
			"[stagger-2/3Exp] ctx: %d dma_port:%d size=%dx%d, stride:%d, fmt:0x%x (iova:0x%x)\n",
			req->state.ctx->stream_id, input_node, in_fmt->fmt.s.w,
			in_fmt->fmt.s.h, in_fmt->fmt.stride[0], in_fmt->fmt.format,
			in_fmt->buf[0].iova);
		}
	} else {
		if (mtk_cam_is_2_exposure(req->state.ctx)) {
			rawi_port_num = 1;
			if (scenario == STAGGER_ON_THE_FLY)
				ptr_rawi = stagger_onthefly_2exp_rawi;
			else
				ptr_rawi = stagger_m2m_2exp_rawi;
		} else if (mtk_cam_is_3_exposure(req->state.ctx)) {
			rawi_port_num = 2;
			if (scenario == STAGGER_ON_THE_FLY)
				ptr_rawi = stagger_onthefly_3exp_rawi;
			else
				ptr_rawi = stagger_m2m_3exp_rawi;
		}

		if (scenario != STAGGER_ON_THE_FLY)
			rawi_port_num++;

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
			rawi_idx, req->state.ctx->stream_id, input_node, in_fmt->fmt.s.w,
			in_fmt->fmt.s.h, in_fmt->fmt.stride[0], in_fmt->fmt.format,
			in_fmt->buf[0].iova);
		}
	}
}

static void config_img_in_fmt_time_shared(struct mtk_cam_device *cam,
				struct mtk_cam_request_stream_data *req,
				struct mtk_cam_video_device *node)
{
	const struct v4l2_format *cfg_fmt = &node->active_fmt;
	struct mtkcam_ipi_img_input *in_fmt;
	struct mtk_cam_img_working_buf_entry *buf_entry;
	struct mtk_cam_ctx *ctx = req->state.ctx;
	int input_node;

	if (node->desc.dma_port != MTKCAM_IPI_RAW_IMGO)
		return;
	input_node = MTKCAM_IPI_RAW_RAWI_2;
	in_fmt = &req->frame_params.img_ins[input_node - MTKCAM_IPI_RAW_RAWI_2];
	node = &ctx->pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
	cfg_fmt = &node->active_fmt;
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
	buf_entry->ctx = ctx;
	buf_entry->req = req->state.req;
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

static void check_stagger_buffer(struct mtk_cam_device *cam,
				struct mtk_cam_request_stream_data *req)
{
	struct mtk_cam_ctx *ctx = req->state.ctx;
	struct mtkcam_ipi_img_input *in_fmt;
	struct mtkcam_ipi_img_output *out_fmt;
	struct mtk_cam_img_working_buf_entry *buf_entry;
	struct mtk_cam_video_device *node;
	const struct v4l2_format *cfg_fmt;
	int input_node;

	if (mtk_cam_is_2_exposure(ctx)) {
		req->frame_params.raw_param.hardware_scenario =
					MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER;
		req->frame_params.raw_param.exposure_num = 2;
		/* rawi_r3 should be 0x0*/
		input_node = MTKCAM_IPI_RAW_RAWI_3;
		in_fmt = &req->frame_params.img_ins[input_node - MTKCAM_IPI_RAW_RAWI_2];
		in_fmt->buf[0].iova = 0x0;
		/* chech rawi_r2 if 0x0*/
		input_node = MTKCAM_IPI_RAW_RAWI_2;
		in_fmt = &req->frame_params.img_ins[input_node - MTKCAM_IPI_RAW_RAWI_2];
		if (in_fmt->buf[0].iova == 0x0) {
			node = &ctx->pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
			cfg_fmt = &node->active_fmt;
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
			buf_entry->ctx = ctx;
			buf_entry->req = req->state.req;
			/* put to processing list */
			spin_lock(&ctx->processing_img_buffer_list.lock);
			list_add_tail(&buf_entry->list_entry,
				&ctx->processing_img_buffer_list.list);
			ctx->processing_img_buffer_list.cnt++;
			spin_unlock(&ctx->processing_img_buffer_list.lock);
			in_fmt->buf[0].iova = buf_entry->img_buffer.iova;
		}
		dev_dbg(cam->dev,
		"[%s:%d] 2-exp : ctx:%d dma_port:%d size=%dx%d, stride:%d, fmt:0x%x (in/out:0x%x/0x%x)\n",
		__func__, req->frame_seq_no, ctx->stream_id, input_node, in_fmt->fmt.s.w,
		in_fmt->fmt.s.h, in_fmt->fmt.stride[0], in_fmt->fmt.format, in_fmt->buf[0].iova,
		req->frame_params.img_outs[0].buf[0][0].iova);
	} else if (mtk_cam_is_3_exposure(ctx)) {
		req->frame_params.raw_param.exposure_num = 3;
		req->frame_params.raw_param.hardware_scenario =
					MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER;
		/* chech rawi_r2 if 0x0*/
		input_node = MTKCAM_IPI_RAW_RAWI_2;
		in_fmt = &req->frame_params.img_ins[input_node - MTKCAM_IPI_RAW_RAWI_2];
		if (in_fmt->buf[0].iova == 0x0) {
			node = &ctx->pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
			cfg_fmt = &node->active_fmt;
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
			buf_entry->ctx = ctx;
			buf_entry->req = req->state.req;
			/* put to processing list */
			spin_lock(&ctx->processing_img_buffer_list.lock);
			list_add_tail(&buf_entry->list_entry,
				&ctx->processing_img_buffer_list.list);
			ctx->processing_img_buffer_list.cnt++;
			spin_unlock(&ctx->processing_img_buffer_list.lock);
			in_fmt->buf[0].iova = buf_entry->img_buffer.iova;
		}
		/* chech rawi_r3 if 0x0*/
		input_node = MTKCAM_IPI_RAW_RAWI_3;
		in_fmt = &req->frame_params.img_ins[input_node - MTKCAM_IPI_RAW_RAWI_2];
		if (in_fmt->buf[0].iova == 0x0) {
			node = &ctx->pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
			cfg_fmt = &node->active_fmt;
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
			buf_entry->ctx = ctx;
			buf_entry->req = req->state.req;
			/* put to processing list */
			spin_lock(&ctx->processing_img_buffer_list.lock);
			list_add_tail(&buf_entry->list_entry,
				&ctx->processing_img_buffer_list.list);
			ctx->processing_img_buffer_list.cnt++;
			spin_unlock(&ctx->processing_img_buffer_list.lock);
			in_fmt->buf[0].iova = buf_entry->img_buffer.iova;
		}
		dev_dbg(cam->dev,
		"[%s:%d] 3-exp : ctx:%d size=%dx%d, stride:%d, fmt:0x%x (inx2/out:0x%x/0x%x/0x%x)\n",
		__func__, req->frame_seq_no, ctx->stream_id, in_fmt->fmt.s.w, in_fmt->fmt.s.h,
		in_fmt->fmt.stride[0], in_fmt->fmt.format,
		req->frame_params.img_ins[0].buf[0].iova, req->frame_params.img_ins[2].buf[0].iova,
		req->frame_params.img_outs[0].buf[0][0].iova);
	} else {
		req->frame_params.raw_param.exposure_num = 1;
		req->frame_params.raw_param.hardware_scenario =
					MTKCAM_IPI_HW_PATH_ON_THE_FLY;
		node = &ctx->pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
		out_fmt = &req->frame_params.img_outs[0];
		input_node = MTKCAM_IPI_RAW_RAWI_2;
		out_fmt->buf[0][0].iova =
			req->frame_params.img_ins[input_node - MTKCAM_IPI_RAW_RAWI_2].buf[0].iova;
		/* rawi_r2 should be 0x0*/
		input_node = MTKCAM_IPI_RAW_RAWI_2;
		in_fmt = &req->frame_params.img_ins[input_node - MTKCAM_IPI_RAW_RAWI_2];
		in_fmt->buf[0].iova = 0x0;
		/* rawi_r3 should be 0x0*/
		input_node = MTKCAM_IPI_RAW_RAWI_3;
		in_fmt = &req->frame_params.img_ins[input_node - MTKCAM_IPI_RAW_RAWI_2];
		in_fmt->buf[0].iova = 0x0;
		dev_dbg(cam->dev,
		"[%s:%d] 1-exp : ctx:%d size=%dx%d, stride:%d, fmt:0x%x (out:0x%x)\n",
		__func__, req->frame_seq_no, ctx->stream_id, out_fmt->fmt.s.w,
		out_fmt->fmt.s.h, out_fmt->fmt.stride[0], out_fmt->fmt.format,
		out_fmt->buf[0][0].iova);
	}

	switch (req->feature.switch_feature_type) {
	case EXPOSURE_CHANGE_NONE:
		req->frame_params.raw_param.previous_exposure_num =
				req->frame_params.raw_param.exposure_num;
		break;
	case EXPOSURE_CHANGE_3_to_2:
	case EXPOSURE_CHANGE_3_to_1:
		req->frame_params.raw_param.previous_exposure_num = 3;
		break;
	case EXPOSURE_CHANGE_2_to_3:
	case EXPOSURE_CHANGE_2_to_1:
		req->frame_params.raw_param.previous_exposure_num = 2;
		break;
	case EXPOSURE_CHANGE_1_to_3:
	case EXPOSURE_CHANGE_1_to_2:
		req->frame_params.raw_param.previous_exposure_num = 1;
		break;
	default:
		req->frame_params.raw_param.previous_exposure_num =
				req->frame_params.raw_param.exposure_num;
	}

}
static void check_timeshared_buffer(struct mtk_cam_device *cam,
				struct mtk_cam_request_stream_data *req)
{
	struct mtk_cam_ctx *ctx = req->state.ctx;
	struct mtkcam_ipi_img_input *in_fmt;
	struct mtk_cam_img_working_buf_entry *buf_entry;
	struct mtk_cam_video_device *node;
	const struct v4l2_format *cfg_fmt;
	int input_node;

		input_node = MTKCAM_IPI_RAW_RAWI_2;
		in_fmt = &req->frame_params.img_ins[input_node - MTKCAM_IPI_RAW_RAWI_2];
		if (in_fmt->buf[0].iova == 0x0) {
			node = &ctx->pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
			cfg_fmt = &node->active_fmt;
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
			buf_entry->ctx = ctx;
			buf_entry->req = req->state.req;
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
		__func__, req->frame_seq_no, ctx->stream_id, input_node, in_fmt->fmt.s.w,
		in_fmt->fmt.s.h, in_fmt->fmt.stride[0], in_fmt->fmt.format, in_fmt->buf[0].iova);
}

/* FIXME: should move following to raw's implementation. */
static int config_img_in_fmt(struct mtk_cam_device *cam,
			  struct mtk_cam_video_device *node,
			  struct mtkcam_ipi_img_input *in_fmt)
{
	const struct v4l2_format *cfg_fmt = &node->active_fmt;

	/* Check output & input image size dimension */
	if (node->desc.dma_port != MTKCAM_IPI_RAW_RAWI_2) {
		dev_info(cam->dev, "pipe(%d):dam_port(%d) only support MTKCAM_IPI_RAW_RAWI_2 now\n",
			node->uid.pipe_id, node->desc.dma_port);
		return -EINVAL;
	}

	in_fmt->fmt.format =
		mtk_cam_get_img_fmt(cfg_fmt->fmt.pix_mp.pixelformat);
	if (in_fmt->fmt.format == MTKCAM_IPI_IMG_FMT_UNKNOWN) {
		dev_dbg(cam->dev, "pipe: %d, node:%d unknown pixel fmt:%d\n",
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
			  struct mtkcam_ipi_img_output *out_fmt, int sd_width,
			  int sd_height)
{
	const struct v4l2_format *cfg_fmt = &node->active_fmt;

	/* Check output & input image size dimension */
	if (node->desc.dma_port == MTKCAM_IPI_RAW_IMGO &&
	    (cfg_fmt->fmt.pix_mp.width > sd_width ||
			cfg_fmt->fmt.pix_mp.height > sd_height)) {
		dev_dbg(cam->dev, "pipe: %d cfg size is larger than sensor\n",
			node->uid.pipe_id);
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


s32 get_crop_request_fd(struct v4l2_selection *crop)
{
	s32 request_fd = 0;

	request_fd = crop->reserved[0];
	crop->reserved[0] = 0;

	return request_fd;
}

void set_crop_request_fd(struct v4l2_selection *crop, s32 request_fd)
{
	u32 *reserved = crop->reserved;

	reserved[0] = request_fd;
}

int mtk_cam_get_feature_switch(struct mtk_cam_ctx *ctx,
			int prev)
{
	int cur = ctx->pipe->res_config.raw_feature;
	int res = EXPOSURE_CHANGE_NONE;

	if (cur == prev)
		return EXPOSURE_CHANGE_NONE;
	if (cur & MTK_CAM_FEATURE_STAGGER_MASK ||
		prev & MTK_CAM_FEATURE_STAGGER_MASK) {
		if ((cur == STAGGER_2_EXPOSURE_LE_SE ||
			cur == STAGGER_2_EXPOSURE_LE_SE) &&
			(prev == STAGGER_3_EXPOSURE_LE_NE_SE ||
			prev == STAGGER_3_EXPOSURE_SE_NE_LE))
			res = EXPOSURE_CHANGE_3_to_2;
		else if ((prev == STAGGER_2_EXPOSURE_LE_SE ||
			prev == STAGGER_2_EXPOSURE_SE_LE) &&
			(cur == STAGGER_3_EXPOSURE_LE_NE_SE ||
			cur == STAGGER_3_EXPOSURE_SE_NE_LE))
			res = EXPOSURE_CHANGE_2_to_3;
		else if ((prev == 0) &&
			(cur == STAGGER_3_EXPOSURE_LE_NE_SE ||
			cur == STAGGER_3_EXPOSURE_SE_NE_LE))
			res = EXPOSURE_CHANGE_1_to_3;
		else if ((cur == 0) &&
			(prev == STAGGER_3_EXPOSURE_LE_NE_SE ||
			prev == STAGGER_3_EXPOSURE_SE_NE_LE))
			res = EXPOSURE_CHANGE_3_to_1;
		else if ((prev == 0) &&
			(cur == STAGGER_2_EXPOSURE_LE_SE ||
			cur == STAGGER_2_EXPOSURE_SE_LE))
			res = EXPOSURE_CHANGE_1_to_2;
		else if ((cur == 0) &&
			(prev == STAGGER_2_EXPOSURE_LE_SE ||
			prev == STAGGER_2_EXPOSURE_SE_LE))
			res = EXPOSURE_CHANGE_2_to_1;
	}
	dev_dbg(ctx->cam->dev, "[%s] res:%d\n", __func__, res);

	return res;
}

int mtk_cam_is_time_shared(struct mtk_cam_ctx *ctx)
{
	if (ctx->used_raw_num) {
		if (ctx->pipe->res_config.raw_feature & MTK_CAM_FEATURE_TIMESHARE_MASK)
			return 1;
		else
			return 0;
	} else {
		return 0;
	}
}

int mtk_cam_is_stagger_m2m(struct mtk_cam_ctx *ctx)
{
	if (ctx->used_raw_num) {
		if (ctx->pipe->res_config.raw_feature & MTK_CAM_FEATURE_STAGGER_M2M_MASK)
			return 1;
		else
			return 0;
	} else {
		return 0;
	}
}

int mtk_cam_is_stagger(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_video_device *node;

	if (ctx->used_raw_num) {
		node = &ctx->pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
		if (node->raw_feature & MTK_CAM_FEATURE_STAGGER_MASK)
			return 1;
		else
			return 0;
	} else {
		return 0;
	}
}

int mtk_cam_is_subsample(struct mtk_cam_ctx *ctx)
{
	if (ctx->used_raw_num) {
		if (ctx->pipe->res_config.raw_feature & MTK_CAM_FEATURE_SUBSAMPLE_MASK)
			return 1;
		else
			return 0;
	} else {
		return 0;
	}
}

int mtk_cam_is_2_exposure(struct mtk_cam_ctx *ctx)
{
	if (ctx->used_raw_num) {
		if (ctx->pipe->res_config.raw_feature == STAGGER_2_EXPOSURE_LE_SE ||
			ctx->pipe->res_config.raw_feature == STAGGER_2_EXPOSURE_SE_LE ||
			ctx->pipe->res_config.raw_feature == STAGGER_M2M_2_EXPOSURE_LE_SE ||
			ctx->pipe->res_config.raw_feature == STAGGER_M2M_2_EXPOSURE_SE_LE) {
			return 1;
		} else {
			return 0;
		}
	} else {
		return 0;
	}
}

int mtk_cam_is_3_exposure(struct mtk_cam_ctx *ctx)
{
	if (ctx->used_raw_num) {
		if (ctx->pipe->res_config.raw_feature == STAGGER_3_EXPOSURE_LE_NE_SE ||
			ctx->pipe->res_config.raw_feature == STAGGER_3_EXPOSURE_SE_NE_LE ||
			ctx->pipe->res_config.raw_feature == STAGGER_M2M_3_EXPOSURE_LE_NE_SE ||
			ctx->pipe->res_config.raw_feature == STAGGER_M2M_3_EXPOSURE_SE_NE_LE) {
			return 1;
		} else {
			return 0;
		}
	} else {
		return 0;
	}
}

s32 get_format_request_fd(struct v4l2_pix_format_mplane *fmt_mp)
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

void set_format_request_fd(struct v4l2_pix_format_mplane *fmt_mp, s32 request_fd)
{
	u8 *reserved = fmt_mp->reserved;

	reserved[0] = request_fd & 0x000000FF;
	reserved[1] = (request_fd & 0x0000FF00) >> 8;
	reserved[2] = (request_fd & 0x00FF0000) >> 16;
	reserved[3] = (request_fd & 0xFF000000) >> 24;
}

static int mtk_cam_req_set_fmt(struct mtk_cam_device *cam,
			      struct mtk_cam_request *req)
{
	int pipe_id;
	int pad;
	struct mtk_cam_request_stream_data *stream_data;
	struct v4l2_subdev *sd;

	dev_dbg(cam->dev, "%s:%s\n", __func__, req->req.debug_str);

	for (pipe_id = 0; pipe_id < cam->max_stream_num; pipe_id++) {
		if ((req->pipe_used & (1 << pipe_id)) &&
			(is_raw_subdev(pipe_id))) {
			stream_data = &req->stream_data[pipe_id];
			sd = &cam->raw.pipelines[pipe_id].subdev;
			/* Set MEDIA_PAD_FL_SINK pad's fmt */
			for (pad = MTK_RAW_SINK_BEGIN;
			     pad < MTK_RAW_SOURCE_BEGIN; pad++)
				if (stream_data->pad_fmt_update & (1 << pad))
					mtk_raw_call_pending_set_fmt
						(sd,
						 &stream_data->pad_fmt[pad]);
			/* Set MEDIA_PAD_FL_SOURCE pad's fmt */
			for (pad = MTK_RAW_SOURCE_BEGIN;
			     pad < MTK_RAW_PIPELINE_PADS_NUM; pad++)
				if (stream_data->pad_fmt_update & (1 << pad))
					mtk_raw_call_pending_set_fmt
						(sd,
						 &stream_data->pad_fmt[pad]);
		}
	}

	return 0;
}

int mtk_cam_get_sensor_exposure_num(u32 raw_feature)
{
	int result = 1;

	switch (raw_feature) {
	case STAGGER_3_EXPOSURE_LE_NE_SE:
	case STAGGER_3_EXPOSURE_SE_NE_LE:
	case STAGGER_M2M_3_EXPOSURE_LE_NE_SE:
	case STAGGER_M2M_3_EXPOSURE_SE_NE_LE:
		result = 3;
		break;
	case STAGGER_2_EXPOSURE_LE_SE:
	case STAGGER_2_EXPOSURE_SE_LE:
	case STAGGER_M2M_2_EXPOSURE_LE_SE:
	case STAGGER_M2M_2_EXPOSURE_SE_LE:
		result = 2;
		break;
	default:
		result = 1;
		break;
	}
	return result;
}

static int mtk_cam_req_update(struct mtk_cam_device *cam,
			      struct mtk_cam_request *req)
{
	struct media_request_object *obj, *obj_prev;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_request_stream_data *req_stream_data_pipe;

	mtk_cam_req_set_fmt(cam, req);

	dev_dbg(cam->dev, "update request:%s\n", req->req.debug_str);

	list_for_each_entry_safe(obj, obj_prev, &req->req.objects, list) {
		struct vb2_buffer *vb;
		struct mtk_cam_buffer *buf;
		struct mtk_cam_video_device *node;
		struct v4l2_subdev_format sd_fmt;
		int sd_width, sd_height, ret;
		struct mtkcam_ipi_img_output *out_fmt;
		struct mtkcam_ipi_img_input *in_fmt;
		struct v4l2_format *fmt;
		struct media_request *request;
		__s32 fd;
		unsigned long flags;

		if (!vb2_request_object_is_buffer(obj))
			continue;
		vb = container_of(obj, struct vb2_buffer, req_obj);
		buf = mtk_cam_vb2_buf_to_dev_buf(vb);
		node = mtk_cam_vbq_to_vdev(vb->vb2_queue);

		ctx = mtk_cam_find_ctx(cam, &node->vdev.entity);
		req->ctx_used |= 1 << ctx->stream_id;

		req_stream_data = &req->stream_data[ctx->stream_id];
		req_stream_data->state.ctx = ctx;
		req_stream_data->no_frame_done_cnt = 0;
		req_stream_data->dbg_work.state = MTK_CAM_REQ_DBGWORK_S_INIT;
		req_stream_data->dbg_work.dump_flags = 0;
		req_stream_data->dbg_exception_work.state = MTK_CAM_REQ_DBGWORK_S_INIT;
		req_stream_data->dbg_exception_work.dump_flags = 0;
		req_stream_data->frame_done_queue_work = 0;
		req->sync_id = (ctx->used_raw_num) ? ctx->pipe->sync_id : 0;

		if (req_stream_data->seninf_new)
			ctx->seninf = req_stream_data->seninf_new;

		req_stream_data_pipe = &req->stream_data[node->uid.pipe_id];
		spin_lock_irqsave(&req_stream_data_pipe->bufs_lock, flags);
		list_add_tail(&buf->stream_data_list, &req_stream_data_pipe->bufs);
		spin_unlock_irqrestore(&req_stream_data_pipe->bufs_lock, flags);

		/* update buffer format */
		switch (node->desc.dma_port) {
		case MTKCAM_IPI_RAW_RAWI_2:
			fd = get_format_request_fd(&node->pending_fmt.fmt.pix_mp);
			if (fd > 0) {
				request = media_request_get_by_fd
						(&cam->media_dev, fd);

				if (request == &req->req) {
					fmt = &node->pending_fmt;
					video_try_fmt(node, fmt);
					node->active_fmt = *fmt;
				}

				if (!IS_ERR(request))
					media_request_put(request);
			}

			in_fmt = &req_stream_data->frame_params
					.img_ins[node->desc.id - MTK_RAW_RAWI_2_IN];
			in_fmt->uid.pipe_id = node->uid.pipe_id;
			in_fmt->uid.id =  node->desc.dma_port;
			ret = config_img_in_fmt(cam, node, in_fmt);
			if (ret)
				return ret;
			if (mtk_cam_is_stagger_m2m(ctx))
				config_img_in_fmt_stagger(cam, req_stream_data, node, STAGGER_M2M);
			break;
		case MTKCAM_IPI_RAW_IMGO:
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
			sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
			sd_fmt.pad = PAD_SRC_RAW0;
			ret = v4l2_subdev_call(ctx->seninf, pad, get_fmt, NULL, &sd_fmt);
			if (ret) {
				dev_dbg(cam->dev, "seninf(%s) g_fmt failed:%d\n",
					ctx->seninf->name, ret);
				return ret;
			}

			fd = get_format_request_fd(&node->pending_fmt.fmt.pix_mp);
			if (fd > 0) {
				request = media_request_get_by_fd
						(&cam->media_dev, fd);

				if (request == &req->req) {
					fmt = &node->pending_fmt;
					video_try_fmt(node, fmt);
					node->active_fmt = *fmt;
				}

				if (!IS_ERR(request))
					media_request_put(request);
			}

			sd_width = sd_fmt.format.width;
			sd_height = sd_fmt.format.height;

			out_fmt = &req_stream_data->frame_params
					.img_outs[node->desc.id - MTK_RAW_SOURCE_BEGIN];
			out_fmt->uid.pipe_id = node->uid.pipe_id;
			out_fmt->uid.id =  node->desc.dma_port;

			fd = get_crop_request_fd(&node->pending_crop);
			if (fd > 0) {
				request = media_request_get_by_fd(
					&cam->media_dev, fd);

				if (request == &req->req ||
				    ctx->enqueued_frame_seq_no == 0) {
					out_fmt->crop.p.x =
						node->pending_crop.r.left;
					out_fmt->crop.p.y =
						node->pending_crop.r.top;
					out_fmt->crop.s.w =
						node->pending_crop.r.width;
					out_fmt->crop.s.h =
						node->pending_crop.r.height;
				}

				if (!IS_ERR(request))
					media_request_put(request);
			} else {
				out_fmt->crop.p.x = node->pending_crop.r.left;
				out_fmt->crop.p.y = node->pending_crop.r.top;
				out_fmt->crop.s.w = node->pending_crop.r.width;
				out_fmt->crop.s.h = node->pending_crop.r.height;
			}

			ret = config_img_fmt(cam, node, out_fmt,
					     sd_width, sd_height);
			if (ret)
				return ret;
			if (mtk_cam_is_stagger(ctx))
				config_img_in_fmt_stagger(cam, req_stream_data, node,
					STAGGER_ON_THE_FLY);
			if (mtk_cam_is_time_shared(ctx))
				config_img_in_fmt_time_shared(cam, req_stream_data, node);
			break;

		default:
			/* Do nothing for the ports not related to crop settings */
			break;
		}
	}
	if (mtk_cam_is_stagger(ctx))
		check_stagger_buffer(cam, req_stream_data);
	if (mtk_cam_is_time_shared(ctx))
		check_timeshared_buffer(cam, req_stream_data);
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

void mtk_cam_dev_req_try_queue(struct mtk_cam_device *cam)
{
	struct mtk_cam_request *req, *req_prev, *req_tmp;
	unsigned long flags;
	int i;
	int raw_fut_pre;

	if (!cam->streaming_ctx) {
		dev_dbg(cam->dev, "streams are off\n");
		return;
	}
	req_tmp = NULL;
	spin_lock_irqsave(&cam->pending_job_lock, flags);
	list_for_each_entry_safe(req, req_prev, &cam->pending_job_list, list) {
		if (likely(mtk_cam_dev_req_is_stream_on(cam, req))) {
			spin_lock(&cam->running_job_lock);
			if (cam->running_job_count >=
			    2 * MTK_CAM_MAX_RUNNING_JOBS) {
				dev_dbg(cam->dev, "jobs are full, job cnt(%d)\n",
					cam->running_job_count);
				spin_unlock(&cam->running_job_lock);
				break;
			}
			dev_dbg(cam->dev,
				"%s job cnt(%d), allow req_enqueue(%s)\n",
				__func__, cam->running_job_count, req->req.debug_str);
			cam->running_job_count++;
			list_del(&req->list);
			list_add_tail(&req->list, &cam->running_job_list);
			spin_unlock(&cam->running_job_lock);
			req_tmp = req;
			break;
		}
	}
	spin_unlock_irqrestore(&cam->pending_job_lock, flags);
	if (!req_tmp)
		return;
	for (i = 0; i < cam->max_stream_num; i++) {
		if (req_tmp->pipe_used & (1 << i)) {
			/* Accumulated frame sequence number */
			req_tmp->stream_data[i].frame_seq_no =
				++(cam->ctxs[i].enqueued_frame_seq_no);
			req_tmp->stream_data[i].state.estate = E_STATE_READY;
			req_tmp->stream_data[i].state.req = req_tmp;
			if (is_raw_subdev(i)) {
				raw_fut_pre = cam->ctxs[i].pipe->res_config.raw_feature;
				mtk_cam_req_ctrl_setup(&cam->ctxs[i], req_tmp);
				req_tmp->stream_data[i].feature.switch_feature_type =
					mtk_cam_get_feature_switch(&cam->ctxs[i], raw_fut_pre);
				req_tmp->stream_data[i].feature.raw_feature =
					cam->ctxs[i].pipe->res_config.raw_feature;
			}
			spin_lock_init(&req_tmp->stream_data[i].bufs_lock);
			INIT_LIST_HEAD(&req_tmp->stream_data[i].bufs);
		}
	}
	if (mtk_cam_req_update(cam, req_tmp)) {
		dev_dbg(cam->dev,
			"%s:req(%s): invalid req settings which can't be recovered\n",
			__func__, req_tmp->req.debug_str);
		WARN_ON(1);
		return;
	}
	mtk_cam_dev_req_enqueue(cam, req_tmp);
}

static struct media_request *mtk_cam_req_alloc(struct media_device *mdev)
{
	struct mtk_cam_request *cam_req;

	cam_req = vzalloc(sizeof(*cam_req));
	spin_lock_init(&cam_req->done_status_lock);
	mutex_init(&cam_req->fs_op_lock);

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

static unsigned int mtk_cam_req_get_pipe_used(struct media_request *req)
{
	/**
	 * V4L2 framework doesn't trigger q->ops->buf_queue(q, buf) when it is
	 * stream off. We have to check the used context through the request directly
	 * before streaming on.
	 */
	struct media_request_object *obj;
	unsigned int pipe_used = 0;

	list_for_each_entry(obj, &req->objects, list) {
		struct vb2_buffer *vb;
		struct mtk_cam_video_device *node;

		if (!vb2_request_object_is_buffer(obj))
			continue;
		vb = container_of(obj, struct vb2_buffer, req_obj);
		node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
		pipe_used |= 1 << node->uid.pipe_id;
	}

	return pipe_used;
}

static void mtk_cam_req_queue(struct media_request *req)
{
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);
	struct mtk_cam_device *cam =
		container_of(req->mdev, struct mtk_cam_device, media_dev);
	unsigned long flags;

	cam_req->time_syscall_enque = ktime_get_boottime_ns() / 1000;

	/* reset done status */
	cam_req->done_status = 0;
	cam_req->pipe_used = mtk_cam_req_get_pipe_used(req);
	cam_req->fs_on_cnt = 0;

	/* update frame_params's dma_bufs in mtk_cam_vb2_buf_queue */
	vb2_request_queue(req);

	/* add to pending job list */
	spin_lock_irqsave(&cam->pending_job_lock, flags);
	if (mtk_cam_req_chk_job_list(cam, cam_req,
				     &cam->pending_job_list,
				     "pending_job_list")) {
		spin_unlock_irqrestore(&cam->pending_job_lock, flags);
		return;
	}

	/**
	 * Add req's ref cnt since it is used by pending_job_list and running
	 * pending_job_list.
	 */
	media_request_get(req);
	list_add_tail(&cam_req->list, &cam->pending_job_list);
	spin_unlock_irqrestore(&cam->pending_job_lock, flags);

	mutex_lock(&cam->op_lock);
	mtk_cam_dev_req_try_queue(cam);
	mutex_unlock(&cam->op_lock);
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
	stream_data = &cam_req->stream_data[ctx->stream_id];
	stream_data->seninf_old = ctx->seninf;
	stream_data->seninf_new = media_entity_to_v4l2_subdev(source);
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

static struct device *mtk_cam_find_raw_dev(struct mtk_cam_device *cam,
					   unsigned int raw_mask)
{
	struct mtk_cam_ctx *ctx;
	unsigned int i;

	for (i = 0; i < cam->num_raw_drivers; i++) {
		if (raw_mask & (1 << i)) {
			ctx = cam->ctxs + i;
			/* FIXME: correct TWIN case */
			return cam->raw.devs[i];
		}
	}

	return NULL;
}

static int get_available_sv_pipes(struct mtk_cam_device *cam,
							int hw_scen, int req_amount, int is_trial)
{
	unsigned int i, idle_pipes = 0, match_cnt = 0;

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
		goto EXIT;
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

int get_main_sv_pipe_id(struct mtk_cam_device *cam,
							int used_dev_mask)
{
	unsigned int i, engine_id = 0;

	for (i = 0; i < cam->num_camsv_drivers; i++) {
		engine_id = i + MTKCAM_PIPE_CAMSV_0;
		if (used_dev_mask & (1 << engine_id))
			return engine_id;
	}
	return -1;
}

int get_sub_sv_pipe_id(struct mtk_cam_device *cam,
							int used_dev_mask)
{
	unsigned int i, engine_id = 0;

	for (i = 0; i < cam->num_camsv_drivers; i++) {
		engine_id = i + MTKCAM_PIPE_CAMSV_0;
		if (used_dev_mask & (1 << engine_id))
			return engine_id + 1;
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
#if CCD_READY
static void isp_composer_uninit(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_ccd *ccd = cam->rproc_handle->priv;

	mtk_destroy_client_msgdevice(ccd->rpmsg_subdev, &ctx->rpmsg_channel);
	ctx->rpmsg_dev = NULL;
}

static int isp_composer_handler(struct rpmsg_device *rpdev, void *data,
				int len, void *priv, u32 src)
{
	struct mtk_cam_device *cam = (struct mtk_cam_device *)priv;
	struct device *dev = cam->dev;
	struct mtkcam_ipi_event *ipi_msg;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_working_buf_entry *buf_entry;
	struct mtk_cam_request_stream_data *stream_data;
	unsigned long flags;
	bool is_m2m_apply_cq = false;

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
		struct mtk_cam_request *req;

		ctx = &cam->ctxs[ipi_msg->cookie.session_id];
		spin_lock(&ctx->using_buffer_list.lock);
		ctx->composed_frame_seq_no = ipi_msg->cookie.frame_no;
		req = mtk_cam_dev_get_req(cam, ctx,
						  ctx->composed_frame_seq_no);
		if (!req) {
			dev_dbg(dev, "ctx:%d no req for ack frame_num:%d\n",
				ctx->stream_id, ctx->composed_frame_seq_no);
			spin_unlock(&ctx->using_buffer_list.lock);
			return -EINVAL;
		}

		/* Do nothing if the user doesn't enable force dump */
		mtk_cam_req_dump(ctx, req,
				 MTK_CAM_REQ_DUMP_FORCE, "Camsys Force Dump");

		stream_data = &req->stream_data[ctx->stream_id];
		stream_data->state.time_swirq_composed = ktime_get_boottime_ns() / 1000;
		dev_dbg(dev, "ctx:%d ack frame_num:%d\n",
			ctx->stream_id, ctx->composed_frame_seq_no);

		/* get from using list */
		if (list_empty(&ctx->using_buffer_list.list)) {
			spin_unlock(&ctx->using_buffer_list.lock);
			return -EINVAL;
		}
		buf_entry = list_first_entry(&ctx->using_buffer_list.list,
					     struct mtk_cam_working_buf_entry,
					     list_entry);
		list_del(&buf_entry->list_entry);
		ctx->using_buffer_list.cnt--;
		buf_entry->cq_desc_offset =
			ipi_msg->ack_data.frame_result.cq_desc_offset;
		buf_entry->cq_desc_size =
			ipi_msg->ack_data.frame_result.cq_desc_size;
		buf_entry->sub_cq_desc_offset =
			ipi_msg->ack_data.frame_result.sub_cq_desc_offset;
		buf_entry->sub_cq_desc_size =
			ipi_msg->ack_data.frame_result.sub_cq_desc_size;

		if (mtk_cam_is_stagger_m2m(ctx)) {
			spin_lock_irqsave(&ctx->processing_buffer_list.lock, flags);
			dev_dbg(dev, "%s ctx->processing_buffer_list.cnt %d\n", __func__,
				ctx->processing_buffer_list.cnt);

			if (ctx->processing_buffer_list.cnt == 0)
				is_m2m_apply_cq = true;

			spin_unlock_irqrestore(&ctx->processing_buffer_list.lock, flags);
		}
		if ((ctx->composed_frame_seq_no == 1 && !mtk_cam_is_time_shared(ctx))
			|| is_m2m_apply_cq) {

			struct mtk_raw_device *raw_dev;
			struct device *dev;

			spin_lock_irqsave(&ctx->processing_buffer_list.lock,
					  flags);
			list_add_tail(&buf_entry->list_entry,
				      &ctx->processing_buffer_list.list);
			ctx->processing_buffer_list.cnt++;
			spin_unlock_irqrestore
				(&ctx->processing_buffer_list.lock, flags);
			spin_unlock(&ctx->using_buffer_list.lock);

			dev = mtk_cam_find_raw_dev(cam, ctx->pipe->enabled_raw);
			if (!dev) {
				dev_dbg(dev, "frm#1 raw device not found\n");
				return -EINVAL;
			}

			raw_dev = dev_get_drvdata(dev);

			if (mtk_cam_is_stagger_m2m(ctx)) {
				dev_dbg(dev, "%s M2M apply_cq, composed_buffer_list.cnt %d frame_seq_no %d\n",
					__func__, ctx->composed_buffer_list.cnt,
					stream_data->frame_seq_no);
				mtk_cam_m2m_enter_cq_state(&stream_data->state);
			}

			apply_cq(raw_dev,
				buf_entry->buffer.iova, buf_entry->cq_desc_size,
				buf_entry->cq_desc_offset, 1, buf_entry->sub_cq_desc_size,
				buf_entry->sub_cq_desc_offset);
			stream_data->timestamp = ktime_get_boottime_ns();
			stream_data->timestamp_mono = ktime_get_ns();
			stream_data->state.time_cqset = ktime_get_boottime_ns() / 1000;

			return 0;
		}
		spin_lock_irqsave(&ctx->composed_buffer_list.lock,
				  flags);
		list_add_tail(&buf_entry->list_entry,
			      &ctx->composed_buffer_list.list);
		ctx->composed_buffer_list.cnt++;
		spin_unlock_irqrestore(&ctx->composed_buffer_list.lock, flags);
		spin_unlock(&ctx->using_buffer_list.lock);
	} else if (ipi_msg->ack_data.ack_cmd_id == CAM_CMD_DESTROY_SESSION) {
		ctx = &cam->ctxs[ipi_msg->cookie.session_id];
		drain_workqueue(ctx->composer_wq);
		destroy_workqueue(ctx->composer_wq);
		drain_workqueue(ctx->frame_done_wq);
		destroy_workqueue(ctx->frame_done_wq);
		ctx->composer_wq = NULL;
		ctx->frame_done_wq = NULL;
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
	struct mtkcam_ipi_frame_param *frame_data;
	struct mtk_cam_request *req = mtk_cam_req_work_to_req(work);
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	struct mtk_cam_working_buf_entry *buf_entry;
	unsigned long flags;

	if (!req_work->req || !req_work->ctx) {
		pr_info("%s req_work->req(%p) req_work->ctx(%p)\n", __func__,
			req_work->req, req_work->ctx);
		return;
	}

	if (req_work->ctx->used_raw_num == 0) {
		pr_info("%s raw is un-used, skip frame work", __func__);
		return;
	}

	ctx = req_work->ctx;
	cam = ctx->cam;
	req_stream_data = &req->stream_data[ctx->stream_id];

	/* check if the ctx is streaming */
	spin_lock_irqsave(&ctx->streaming_lock, flags);
	if (!ctx->streaming) {
		dev_info(cam->dev,
			 "%s: skip frame work, for stream off ctx:%d, req:%d\n",
			 __func__, ctx->stream_id, req_stream_data->frame_seq_no);
		spin_unlock_irqrestore(&ctx->streaming_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&ctx->streaming_lock, flags);

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_FRAME;
	session->session_id = ctx->stream_id;

	/* prepare working buffer */
	buf_entry = mtk_cam_working_buf_get(ctx);
	if (!buf_entry) {
		dev_info(cam->dev, "%s: No CQ buf availablle: req:%d(%s)\n",
			 __func__, req_stream_data->frame_seq_no,
			 req->req.debug_str);
		WARN_ON(1);
		return;
	}

	buf_entry->ctx = ctx;
	buf_entry->req = req;
	req_stream_data->working_buf = buf_entry;

	/* put to using list */
	spin_lock(&ctx->using_buffer_list.lock);
	list_add_tail(&buf_entry->list_entry, &ctx->using_buffer_list.list);
	ctx->using_buffer_list.cnt++;
	spin_unlock(&ctx->using_buffer_list.lock);

	/* Prepare rp message */
	frame_info->cur_msgbuf_offset =
		buf_entry->msg_buffer.va -
		cam->ctxs[session->session_id].buf_pool.msg_buf_va;
	frame_info->cur_msgbuf_size = buf_entry->msg_buffer.size;
	frame_data = (struct mtkcam_ipi_frame_param *)buf_entry->msg_buffer.va;

	session->frame_no = req_stream_data->frame_seq_no;
	if (mtk_cam_is_stagger(ctx))
		dev_dbg(cam->dev, "[%s:stagger-req:%d] ctx:%d type:%d (ipi)hwscene:%d/expN:%d/prev_expN:%d\n",
			__func__, req_stream_data->frame_seq_no, ctx->stream_id,
			req_stream_data->feature.switch_feature_type,
			req_stream_data->frame_params.raw_param.hardware_scenario,
			req_stream_data->frame_params.raw_param.exposure_num,
			req_stream_data->frame_params.raw_param.previous_exposure_num);
	memcpy(frame_data, &req_stream_data->frame_params,
	       sizeof(req_stream_data->frame_params));
	frame_data->cur_workbuf_offset =
		buf_entry->buffer.iova -
		cam->ctxs[session->session_id].buf_pool.working_buf_iova;
	frame_data->cur_workbuf_size = buf_entry->buffer.size;

	req_stream_data->state.time_composing = ktime_get_boottime_ns() / 1000;

	/* Send CAM_CMD_CONFIG if we change sensor */
	if (req->ctx_link_update & 1 << ctx->stream_id)
		mtk_cam_dev_config(ctx, true, false);

	if (ctx->rpmsg_dev) {
		rpmsg_send(ctx->rpmsg_dev->rpdev.ept, &event, sizeof(event));
		dev_info(cam->dev, "rpmsg_send id: %d, ctx:%d, req:%d\n",
			event.cmd_id, session->session_id,
			req_stream_data->frame_seq_no);
	} else {
		dev_dbg(cam->dev, "%s: rpmsg_dev not exist: %d, ctx:%d, req:%d\n",
			__func__, event.cmd_id, session->session_id,
			req_stream_data->frame_seq_no);

	}
}

static void mtk_cam_req_work_init(struct mtk_cam_req_work *work,
				  struct mtk_cam_request *req, struct mtk_cam_ctx *ctx, int pipe_id)
{
	work->req = req;
	work->ctx = ctx;
	work->pipe_id = pipe_id;
}

bool mtk_cam_sv_req_enqueue(struct mtk_cam_ctx *ctx, struct mtk_cam_request *req)
{
	unsigned int i;
	int camsv_pipe_id;
	struct mtk_cam_request_stream_data *ctx_stream_data;
	struct mtk_cam_request_stream_data *pipe_stream_data;
	struct mtk_camsv_working_buf_entry *buf_entry;
	struct device *dev_sv;
	struct mtk_camsv_device *camsv_dev;
	bool ret = false;

	ctx_stream_data = &req->stream_data[ctx->stream_id];
	for (i = 0 ; i < ctx->used_sv_num ; i++) {
		camsv_pipe_id = ctx->sv_pipe[i]->id;
		if (!(req->pipe_used & 1 << camsv_pipe_id)) {
			dev_info(ctx->cam->dev,
				"%s:ctx(%d):pipe(%d):req(%d) no sv buffer enqueued\n",
				__func__, ctx->stream_id, camsv_pipe_id,
				ctx_stream_data->frame_seq_no);
			continue;
		}

		pipe_stream_data = &req->stream_data[camsv_pipe_id];
		buf_entry = mtk_cam_sv_working_buf_get(ctx);
		buf_entry->buffer.used_sv_dev = ctx->used_sv_dev[i];
		buf_entry->buffer.frame_seq_no = ctx_stream_data->frame_seq_no;
		buf_entry->buffer.img_iova =
			pipe_stream_data->sv_frame_params.img_out.buf[0][0].iova;

		if (ctx_stream_data->frame_seq_no == 1) {
			dev_sv = mtk_cam_find_sv_dev(ctx->cam, ctx->used_sv_dev[i]);
			if (dev_sv == NULL) {
				dev_dbg(ctx->cam->dev, "camsv device not found\n");
				return false;
			}
			camsv_dev = dev_get_drvdata(dev_sv);
			mtk_cam_sv_enquehwbuf(camsv_dev,
				buf_entry->buffer.img_iova,
				buf_entry->buffer.frame_seq_no);
			/* initial request readout will be delayed 1 frame */
			if (ctx->used_raw_num && !mtk_cam_is_subsample(ctx) &&
				!mtk_cam_is_stagger(ctx) && !mtk_cam_is_stagger_m2m(ctx) &&
				!mtk_cam_is_time_shared(ctx))
				mtk_cam_sv_write_rcnt(camsv_dev);
			if (ctx->stream_id >= MTKCAM_SUBDEV_CAMSV_START &&
				ctx->stream_id < MTKCAM_SUBDEV_CAMSV_END) {
				if (ctx_stream_data->state.estate == E_STATE_READY ||
					ctx_stream_data->state.estate == E_STATE_SENSOR) {
					ctx_stream_data->state.estate = E_STATE_OUTER;
				}
			}
			spin_lock(&ctx->sv_processing_buffer_list.lock);
			list_add_tail(&buf_entry->list_entry,
					&ctx->sv_processing_buffer_list.list);
			ctx->sv_processing_buffer_list.cnt++;
			spin_unlock(&ctx->sv_processing_buffer_list.lock);
		} else {
			spin_lock(&ctx->sv_using_buffer_list.lock);
			list_add_tail(&buf_entry->list_entry,
					&ctx->sv_using_buffer_list.list);
			ctx->sv_using_buffer_list.cnt++;
			spin_unlock(&ctx->sv_using_buffer_list.lock);
		}
		ret = true;
	}
	return ret;
}

void mtk_cam_dev_req_enqueue(struct mtk_cam_device *cam,
			     struct mtk_cam_request *req)
{
	unsigned int i, j;

	for (i = 0; i < cam->max_stream_num; i++) {
		if (req->ctx_used & (1 << cam->ctxs[i].stream_id)) {
			unsigned int stream_id = i;
			struct mtk_cam_req_work *sensor_work, *frame_work, *frame_done_work;
			struct mtk_cam_request_stream_data *req_stream_data;
			struct mtk_camsys_ctrl_state *req_state;
			struct mtk_camsys_sensor_ctrl *sensor_ctrl;
			struct mtk_cam_ctx *ctx = &cam->ctxs[stream_id];

			/**
			 * For sub dev shares ctx's cases, e.g.
			 * PDAF's topoloy, camsv is the part of raw pipeline's
			 * ctx and we need to skip the camsv sub dev's ctx (which
			 * is not streaming here).
			 */
			if (!ctx->streaming)
				continue;

			sensor_ctrl = &ctx->sensor_ctrl;
			req_stream_data = &req->stream_data[stream_id];
			sensor_work = &req_stream_data->sensor_work;
			frame_work = &req_stream_data->frame_work;
			req_state = &req_stream_data->state;

			for (j = 0 ; j < MTKCAM_SUBDEV_MAX ; j++) {
				if (1 << j & ctx->streaming_pipe) {
					frame_done_work = &req->stream_data[j].frame_done_work;
					mtk_cam_req_work_init(frame_done_work, req, ctx, stream_id);
					INIT_WORK(&frame_done_work->work, mtk_cam_frame_done_work);
				}
			}

			/* Prepare sensor work */
			mtk_cam_req_work_init(sensor_work, req, ctx, stream_id);

			if (mtk_cam_is_time_shared(ctx)) {
				req_stream_data->frame_params.raw_param.hardware_scenario =
						MTKCAM_IPI_HW_PATH_OFFLINE_M2M;
				req_stream_data->frame_params.raw_param.exposure_num = 1;
				req_stream_data->state.estate = E_STATE_TS_READY;
			}

			if (ctx->sensor && (req_stream_data->frame_seq_no == 1 ||
				mtk_cam_is_stagger_m2m(ctx)))
				mtk_cam_initial_sensor_setup(req, ctx);

			/* Prepare CQ compose work */
			mtk_cam_req_work_init(frame_work, req, ctx, stream_id);

			mtk_cam_sv_req_enqueue(ctx, req);
			INIT_WORK(&frame_work->work, isp_tx_frame_worker);
			queue_work(ctx->composer_wq, &frame_work->work);

			dev_dbg(cam->dev, "%s:ctx:%d:req:%d(%s) enqueue ctx_used:0x%x,streaming_ctx:0x%x,job cnt:%d\n",
				__func__, stream_id, req_stream_data->frame_seq_no,
				req->req.debug_str, req->ctx_used, cam->streaming_ctx,
				cam->running_job_count);
		}
	}
}

#if CCD_READY
void isp_composer_create_session(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_session_param	*session_data = &event.session_data;

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_CREATE_SESSION;
	session->session_id = ctx->stream_id;
	session_data->workbuf.iova = ctx->buf_pool.working_buf_iova;
	/* TODO: separated with SCP case */
	session_data->workbuf.ccd_fd = ctx->buf_pool.working_buf_fd;
	session_data->workbuf.size = ctx->buf_pool.working_buf_size;
	session_data->msg_buf.ccd_fd = ctx->buf_pool.msg_buf_fd;
	session_data->msg_buf.size = ctx->buf_pool.msg_buf_size;

	rpmsg_send(ctx->rpmsg_dev->rpdev.ept, &event, sizeof(event));
	dev_dbg(cam->dev, "%s: rpmsg_send id: %d, cq_buf(fd:%d,sz:%d, msg_buf(fd:%d,sz%d)\n",
		__func__, event.cmd_id, session_data->workbuf.ccd_fd,
		session_data->workbuf.size, session_data->msg_buf.ccd_fd,
		session_data->msg_buf.size);
}

static void isp_composer_destroy_session(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_DESTROY_SESSION;
	session->session_id = ctx->stream_id;
	rpmsg_send(ctx->rpmsg_dev->rpdev.ept, &event, sizeof(event));
	dev_dbg(cam->dev, "rpmsg_send id: %d\n", event.cmd_id);
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
				pm_runtime_put(raw->devs[i]);
			}
		}
	}

	ret = mtk_cam_raw_select(pipe, cfg_in_param);
	if (ret) {
		dev_dbg(raw->cam_dev, "failed select raw: %d\n",
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
		dev_dbg(raw->cam_dev,
			"failed at pm_runtime_get_sync: %s\n",
			dev_driver_string(raw->devs[i]));
		for (i = i - 1; i >= 0; i--)
			if (pipe->enabled_raw & 1 << i) {
				dev_info(raw->cam_dev, "%s: power off raw (%d)\n",
						 __func__, i);
				pm_runtime_put(raw->devs[i]);
			}
		return ret;
	}
	ctx->used_raw_dev = pipe->enabled_raw;
	dev_info(raw->cam_dev, "ctx_id %d used_raw_dev 0x%x pipe_id %d\n",
		ctx->stream_id, ctx->used_raw_dev, pipe->id);
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
	unsigned int i;
	int ret;
	u32 mf_code;

	memset(&config_param, 0, sizeof(config_param));

	/* Update cfg_in_param */
	cfg_in_param = &config_param.input;
	cfg_in_param->pixel_mode = ctx->pipe->res_config.tgo_pxl_mode;
	cfg_in_param->subsample = mtk_cam_get_subsample_ratio(
					ctx->pipe->res_config.raw_feature);
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

	if (config_pipe) {
		config_param.flags = MTK_CAM_IPI_CONFIG_TYPE_INIT;
		ret = mtk_cam_raw_pipeline_config(ctx, cfg_in_param);
		if (ret)
			return ret;
	} else {
		/* Change the input size information only */
		config_param.flags = MTK_CAM_IPI_CONFIG_TYPE_INPUT_CHANGE;
	}
	dev_dbg(dev, "%s: config_param flag: 0x%x\n", __func__, config_param.flags);

	config_param.n_maps = 1;
	config_param.maps[0].pipe_id = ctx->pipe->id;
	config_param.maps[0].dev_mask = MTKCAM_SUBDEV_RAW_MASK & ctx->used_raw_dev;
	config_param.sw_feature = (mtk_cam_is_stagger(ctx) == 1) ?
			MTKCAM_IPI_SW_FEATURE_VHDR_STAGGER : MTKCAM_IPI_SW_FEATURE_NORMAL;
	dev_raw = mtk_cam_find_raw_dev(cam, ctx->used_raw_dev);
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

	dev_info(ctx->cam->dev, "%s: mtk_raw_pipeline(%d), ipi_id(%d)\n",
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

	dev_info(dev, "%s initialized composer of ctx:%d\n",
		 __func__, ctx->stream_id);

	return 0;

faile_release_msg_dev:
	mtk_destroy_client_msgdevice(rpmsg_subdev, &ctx->rpmsg_channel);
	ctx->rpmsg_dev = NULL;
	return -EINVAL;
}
#endif

static int mtk_cam_pm_suspend(struct device *dev)
{
	int ret;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	ret = pm_runtime_force_suspend(dev);
	if (ret)
		dev_dbg(dev, "failed to force suspend:%d\n", ret);

	return ret;
}

static int mtk_cam_pm_resume(struct device *dev)
{
	int ret;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	ret = pm_runtime_force_resume(dev);
	return ret;
}

static int mtk_cam_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "- %s\n", __func__);
	return 0;
}

static int mtk_cam_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "- %s\n", __func__);
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
	int ret, i;
	struct media_entity *entity = &node->vdev.entity;

	ctx->enqueued_frame_seq_no = 0;
	ctx->composed_frame_seq_no = 0;
	ctx->dequeued_frame_seq_no = 0;

	if (!cam->composer_cnt) {
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

	ret = mtk_cam_working_buf_pool_init(ctx);
	if (ret) {
		dev_dbg(cam->dev, "failed to reserve DMA memory:%d\n", ret);
		goto fail_uninit_composer;
	}

	ctx->composer_wq =
			alloc_ordered_workqueue(dev_name(cam->dev),
						WQ_HIGHPRI | WQ_FREEZABLE);
	if (!ctx->composer_wq) {
		dev_dbg(cam->dev, "failed to alloc composer workqueue\n");
		goto fail_release_buffer_pool;
	}

	ctx->frame_done_wq =
			alloc_ordered_workqueue(dev_name(cam->dev),
						WQ_HIGHPRI | WQ_FREEZABLE);
	if (!ctx->frame_done_wq) {
		dev_dbg(cam->dev, "failed to alloc frame_done workqueue\n");
		goto fail_uninit_composer_wq;
	}

	mtk_cam_sv_working_buf_pool_init(ctx);

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
			dev_dbg(cam->dev, "duplicated subdevs!!!\n");
			goto fail_stop_pipeline;
		}

		if (is_media_entity_v4l2_subdev(entity))
			*target_sd = media_entity_to_v4l2_subdev(entity);
	}

	return ctx;

fail_stop_pipeline:
	media_pipeline_stop(entity);
fail_uninit_frame_done_wq:
	destroy_workqueue(ctx->frame_done_wq);
fail_uninit_composer_wq:
	destroy_workqueue(ctx->composer_wq);
fail_release_buffer_pool:
	mtk_cam_working_buf_pool_release(ctx);
fail_uninit_composer:
#if CCD_READY
	isp_composer_uninit(ctx);
fail_shutdown:
	if (!cam->composer_cnt) {
		pm_runtime_mark_last_busy(cam->dev);
		pm_runtime_put_autosuspend(cam->dev);
		rproc_shutdown(cam->rproc_handle);
	}
fail_rproc_put:
	if (!cam->composer_cnt) {
		rproc_put(cam->rproc_handle);
		cam->rproc_handle = NULL;
	}
#endif
	return NULL;
}

#if CCD_READY
static void isp_composer_uninit_wait(struct mtk_cam_ctx *ctx)
{
	unsigned long end = jiffies + msecs_to_jiffies(100);

	while (time_before(jiffies, end) && ctx->composer_wq) {
		dev_dbg(ctx->cam->dev, "wait composer session destroy\n");
		usleep_range(0, 1000);
	}

	isp_composer_uninit(ctx);
}
#endif

void mtk_cam_stop_ctx(struct mtk_cam_ctx *ctx, struct media_entity *entity)
{
	struct mtk_cam_device *cam = ctx->cam;
	unsigned int i;

	pr_info("%s\n", __func__);
	media_pipeline_stop(entity);

	if (!cam->streaming_ctx) {
		struct v4l2_subdev *sd;

		v4l2_device_for_each_subdev(sd, &cam->v4l2_dev) {
			if (sd->entity.function == MEDIA_ENT_F_VID_IF_BRIDGE) {
				int ret;

				ret = v4l2_subdev_call(sd, video, s_stream, 0);
				if (ret)
					dev_dbg(cam->dev,
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

	ctx->enabled_node_cnt = 0;
	ctx->streaming_node_cnt = 0;
	ctx->streaming_pipe = 0;
	ctx->sensor = NULL;
	ctx->prev_sensor = NULL;
	ctx->seninf = NULL;
	ctx->prev_seninf = NULL;
	ctx->enqueued_frame_seq_no = 0;
	ctx->composed_frame_seq_no = 0;
	ctx->used_raw_num = 0;
	ctx->used_sv_num = 0;

	INIT_LIST_HEAD(&ctx->using_buffer_list.list);
	INIT_LIST_HEAD(&ctx->composed_buffer_list.list);
	INIT_LIST_HEAD(&ctx->processing_buffer_list.list);

	INIT_LIST_HEAD(&ctx->sv_using_buffer_list.list);
	INIT_LIST_HEAD(&ctx->sv_processing_buffer_list.list);

	INIT_LIST_HEAD(&ctx->processing_img_buffer_list.list);
	for (i = 0; i < MAX_PIPES_PER_STREAM; i++)
		ctx->pipe_subdevs[i] = NULL;

	for (i = 0; i < MAX_SV_PIPES_PER_STREAM; i++) {
		ctx->sv_pipe[i] = NULL;
		ctx->used_sv_dev[i] = 0;
	}

	isp_composer_uninit_wait(ctx);
	cam->composer_cnt--;

	dev_info(cam->dev, "%s: ctx-%d:  composer_cnt:%d\n",
		__func__, ctx->stream_id, cam->composer_cnt);

	mtk_cam_working_buf_pool_release(ctx);

	if (ctx->cam->rproc_handle && !ctx->cam->composer_cnt) {
		dev_info(cam->dev, "%s power off camsys\n", __func__);
		pm_runtime_mark_last_busy(cam->dev);
		pm_runtime_put_autosuspend(cam->dev);
#if CCD_READY
		rproc_shutdown(cam->rproc_handle);
		rproc_put(cam->rproc_handle);
		cam->rproc_handle = NULL;
#endif
	}
}
int PipeIDtoTGIDX(int pipe_id)
{
	switch (pipe_id) {
	case MTKCAM_PIPE_RAW_A:
					return 0;
	case MTKCAM_PIPE_RAW_B:
					return 1;
	case MTKCAM_PIPE_RAW_C:
					return 2;
	case MTKCAM_PIPE_CAMSV_0:
					return 3;
	case MTKCAM_PIPE_CAMSV_1:
					return 4;
	case MTKCAM_PIPE_CAMSV_2:
					return 5;
	case MTKCAM_PIPE_CAMSV_3:
					return 6;
	case MTKCAM_PIPE_CAMSV_4:
					return 7;
	case MTKCAM_PIPE_CAMSV_5:
					return 8;
	case MTKCAM_PIPE_MRAW_0:
					return 15;
	case MTKCAM_PIPE_MRAW_1:
					return 16;
	default:
			break;
	}
	return -1;
}
int mtk_cam_ctx_stream_on(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev;
	struct mtk_raw_device *raw_dev;
	int i, ret;
	unsigned long flags;
	bool need_dump_mem = false;

	dev_info(cam->dev, "ctx %d stream on\n", ctx->stream_id);
	if (ctx->streaming) {
		dev_dbg(cam->dev, "ctx-%d is already streaming on\n",
			ctx->stream_id);
		return 0;
	}
	cam->composer_cnt++;
	for (i = 0; i < MAX_PIPES_PER_STREAM && ctx->pipe_subdevs[i]; i++) {
		ret = v4l2_subdev_call(ctx->pipe_subdevs[i], video,
				       s_stream, 1);
		if (ret) {
			dev_dbg(cam->dev, "failed to stream on %d: %d\n",
				ctx->pipe_subdevs[i]->name, ret);
			goto fail_pipe_off;
		}
	}
	if (ctx->used_raw_num) {
		/**
		 * TODO: validate pad's setting of each pipes
		 * return -EPIPE if failed
		 */
		if (mtk_cam_is_stagger(ctx)
			|| mtk_cam_is_time_shared(ctx))
			mtk_cam_img_working_buf_pool_init(ctx);
		ret = mtk_cam_dev_config(ctx, false, true);
		if (ret)
			return ret;
		dev = mtk_cam_find_raw_dev(cam, ctx->used_raw_dev);
		if (!dev) {
			dev_info(cam->dev, "streamon raw device not found\n");
			goto fail_pipe_off;
		}
		raw_dev = dev_get_drvdata(dev);
		/* stagger mode - use sv to output data to DRAM - online mode */
		if (mtk_cam_is_stagger(ctx)) {
			int hw_scen, req_amount, idle_pipes, src_pad_idx;

			hw_scen = (1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER);
			req_amount = (mtk_cam_is_3_exposure(ctx)) ? 2 : 1;
			idle_pipes = get_available_sv_pipes(cam, hw_scen, req_amount, 1);
			src_pad_idx = PAD_SRC_RAW0;
			/* cached used sv pipes */
			ctx->pipe->enabled_raw |= idle_pipes;
			if (idle_pipes == 0) {
				dev_info(cam->dev, "no available sv pipes(scen:%d/req_amount:%d)",
					hw_scen, req_amount);
				goto fail_pipe_off;
			}
			for (i = MTKCAM_SUBDEV_CAMSV_START;
				i < MTKCAM_SUBDEV_CAMSV_END; i++) {
				if (idle_pipes & (1 << i)) {
					mtk_cam_seninf_set_pixelmode(
							ctx->seninf, src_pad_idx,
							ctx->pipe->res_config.tgo_pxl_mode);
					mtk_cam_seninf_set_camtg(ctx->seninf, src_pad_idx, i - 1);
					ret = mtk_cam_sv_dev_config(
						ctx, i - MTKCAM_SUBDEV_CAMSV_START, hw_scen,
						(src_pad_idx == PAD_SRC_RAW0));
					cam->camsys_ctrl.camsv_dev[
						i - MTKCAM_SUBDEV_CAMSV_START] =
						dev_get_drvdata(
						  cam->sv.devs[i - MTKCAM_SUBDEV_CAMSV_START]);
					if (ret)
						goto fail_pipe_off;
					src_pad_idx++;
				}
			}
			/* todo: backend support one pixel mode only */
			mtk_cam_seninf_set_pixelmode(ctx->seninf, PAD_SRC_RAW2,
						ctx->pipe->res_config.tgo_pxl_mode);
			mtk_cam_seninf_set_camtg(ctx->seninf, PAD_SRC_RAW2,
						PipeIDtoTGIDX(raw_dev->id));
		} else if (mtk_cam_is_time_shared(ctx)) {
			int hw_scen, req_amount, idle_pipes, src_pad_idx;

			hw_scen = (1 << MTKCAM_IPI_HW_PATH_OFFLINE_M2M);
			req_amount = 1;
			idle_pipes = get_available_sv_pipes(cam, hw_scen, req_amount, 0);
			src_pad_idx = PAD_SRC_RAW0;
			/* cached used sv pipes */
			ctx->pipe->enabled_raw |= idle_pipes;
			if (idle_pipes == 0) {
				dev_info(cam->dev, "no available sv pipes(scen:%d/req_amount:%d)",
					hw_scen, req_amount);
				goto fail_pipe_off;
			}
			for (i = MTKCAM_SUBDEV_CAMSV_START;
				i < MTKCAM_SUBDEV_CAMSV_END; i++) {
				if (idle_pipes & (1 << i)) {
					mtk_cam_seninf_set_pixelmode(
							ctx->seninf, src_pad_idx,
							ctx->pipe->res_config.tgo_pxl_mode);
					mtk_cam_seninf_set_camtg(ctx->seninf, src_pad_idx, i - 1);
					ret = mtk_cam_sv_dev_config(
						ctx, i - MTKCAM_SUBDEV_CAMSV_START, hw_scen,
						0);
					cam->camsys_ctrl.camsv_dev[
						i - MTKCAM_SUBDEV_CAMSV_START] =
						dev_get_drvdata(
						  cam->sv.devs[i - MTKCAM_SUBDEV_CAMSV_START]);
					if (ret)
						goto fail_pipe_off;
					src_pad_idx++;
					dev_info(cam->dev, "[TS] scen:0x%x/idle_pipes:0x%x/enabled_raw:0x%x/i(%d)",
					hw_scen, idle_pipes, ctx->pipe->enabled_raw, i);
				}
			}
		} else if (!mtk_cam_is_stagger_m2m(ctx)) {
			mtk_cam_seninf_set_pixelmode(ctx->seninf, PAD_SRC_RAW0,
					ctx->pipe->res_config.tgo_pxl_mode);
			mtk_cam_seninf_set_camtg(ctx->seninf, PAD_SRC_RAW0,
					PipeIDtoTGIDX(raw_dev->id));
		}
	}

	if (!mtk_cam_is_stagger_m2m(ctx)) {
		for (i = 0 ; i < ctx->used_sv_num ; i++) {
			mtk_cam_seninf_set_pixelmode(ctx->seninf, ctx->sv_pipe[i]->seninf_padidx,
				3); /* use 8-pixel mode as default */
			mtk_cam_seninf_set_camtg(ctx->seninf, ctx->sv_pipe[i]->seninf_padidx,
						PipeIDtoTGIDX(ctx->sv_pipe[i]->id));
			ret = mtk_cam_sv_dev_config(ctx, i, 1, 0);
			if (ret)
				goto fail_pipe_off;
		}
		ret = v4l2_subdev_call(ctx->seninf, video, s_stream, 1);
		if (ret) {
			dev_dbg(cam->dev, "failed to stream on seninf %s:%d\n",
				ctx->seninf->name, ret);
			return ret;
		}
	} else {
		ctx->processing_buffer_list.cnt = 0;
		ctx->composed_buffer_list.cnt = 0;
		dev_dbg(cam->dev, "[M2M] reset processing_buffer_list.cnt & composed_buffer_list.cnt\n");
	}

	if (ctx->used_raw_num) {
		initialize(raw_dev);
		/* Stagger */
		if (mtk_cam_is_stagger(ctx))
			stagger_enable(raw_dev);
		/* Sub sample */
		if (mtk_cam_is_subsample(ctx))
			subsample_enable(raw_dev);
		/* Twin */
		if (ctx->pipe->res_config.raw_num_used != 1) {
			struct mtk_raw_device *raw_dev_slave =
						get_slave_raw_dev(cam, ctx->pipe);
			initialize(raw_dev_slave);
			if (ctx->pipe->res_config.raw_num_used == 3) {
				struct mtk_raw_device *raw_dev_slave2 =
					get_slave2_raw_dev(cam, ctx->pipe);
				initialize(raw_dev_slave2);
			}
		}
	}
	/* TODO */
	spin_lock_irqsave(&ctx->streaming_lock, flags);
	if (!cam->streaming_ctx && cam->debug_fs)
		need_dump_mem = true;
	else
		dev_dbg(cam->dev,
			"No need to alloc mem for ctx: streaming_ctx(0x%x), debug_fs(%p)\n",
			cam->streaming_ctx, cam->debug_fs);
	ctx->streaming = true;
	cam->streaming_ctx |= 1 << ctx->stream_id;
	spin_unlock_irqrestore(&ctx->streaming_lock, flags);
	if (need_dump_mem)
		cam->debug_fs->ops->reinit(cam->debug_fs, ctx->stream_id);
	/* update dvfs/qos */
#ifndef FPGA_EP
	if (ctx->used_raw_num) {
		mtk_cam_dvfs_update_clk(ctx->cam);
		mtk_cam_qos_bw_calc(ctx);
	}
#endif
	ret = mtk_camsys_ctrl_start(ctx);
	if (ret) {
		ctx->streaming = false;
		cam->streaming_ctx &= ~(1 << ctx->stream_id);
		goto fail_pipe_off;
	}
	mtk_cam_dev_req_try_queue(cam);
	/* raw off, no cq done, so sv on after enque */
	if (ctx->used_raw_num == 0) {
		for (i = 0 ; i < ctx->used_sv_num ; i++) {
			ret = mtk_cam_sv_dev_stream_on(ctx, i, 1, 1);
			if (ret)
				goto fail_pipe_off;
		}
	}
	dev_dbg(cam->dev, "streamed on camsys ctx:%d\n", ctx->stream_id);
	return 0;
fail_pipe_off:
#if CCD_READY
	isp_composer_destroy_session(ctx);
#endif
	cam->composer_cnt--;
	for (i = 0; i < MAX_PIPES_PER_STREAM && ctx->pipe_subdevs[i]; i++)
		v4l2_subdev_call(ctx->pipe_subdevs[i], video, s_stream, 0);
	v4l2_subdev_call(ctx->seninf, video, s_stream, 0);
	return ret;
}

int mtk_cam_ctx_stream_off(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev;
	struct mtk_raw_device *raw_dev;
	unsigned int i;
	int ret;

	if (!ctx->streaming) {
		dev_dbg(cam->dev, "ctx-%d is already streaming off\n",
			ctx->stream_id);
		return 0;
	}

	dev_info(cam->dev, "%s: ctx-%d:  composer_cnt:%d\n",
		__func__, ctx->stream_id, cam->composer_cnt);

	ctx->streaming = false;
	cam->streaming_ctx &= ~(1 << ctx->stream_id);

	if (ctx->synced) {
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
	ret = v4l2_subdev_call(ctx->seninf, video, s_stream, 0);
	if (ret) {
		dev_dbg(cam->dev, "failed to stream off %s:%d\n",
			ctx->seninf->name, ret);
		return -EPERM;
	}

	if (ctx->used_raw_num) {
		dev = mtk_cam_find_raw_dev(cam, ctx->used_raw_dev);
		if (!dev) {
			dev_dbg(cam->dev, "streamoff raw device not found\n");
			goto fail_stream_off;
		}
		raw_dev = dev_get_drvdata(dev);
		stream_on(raw_dev, 0);
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
	if (mtk_cam_is_stagger(ctx)) {
		unsigned int hw_scen =
			(1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER);
		for (i = MTKCAM_SUBDEV_CAMSV_START; i < MTKCAM_SUBDEV_CAMSV_END; i++) {
			if (ctx->pipe->enabled_raw & (1 << i)) {
				mtk_cam_sv_dev_stream_on(
					ctx, i - MTKCAM_SUBDEV_CAMSV_START, 0, hw_scen);
				cam->sv.pipelines[i - MTKCAM_SUBDEV_CAMSV_START].is_occupied = 0;
			}
		}
	}
	if (mtk_cam_is_time_shared(ctx)) {
		unsigned int hw_scen =
			(1 << MTKCAM_IPI_HW_PATH_OFFLINE_M2M);
		for (i = MTKCAM_SUBDEV_CAMSV_START; i < MTKCAM_SUBDEV_CAMSV_END; i++) {
			if (ctx->pipe->enabled_raw & (1 << i)) {
				mtk_cam_sv_dev_stream_on(
					ctx, i - MTKCAM_SUBDEV_CAMSV_START, 0, hw_scen);
				cam->sv.pipelines[i - MTKCAM_SUBDEV_CAMSV_START].is_occupied = 0;
			}
		}
	}
	/* reset dvfs/qos */
#ifndef FPGA_EP
	if (ctx->used_raw_num) {
		mtk_cam_dvfs_update_clk(ctx->cam);
		mtk_cam_qos_bw_reset(ctx->cam);
	}
#endif
	for (i = 0; i < MAX_PIPES_PER_STREAM && ctx->pipe_subdevs[i]; i++) {
		ret = v4l2_subdev_call(ctx->pipe_subdevs[i], video,
				       s_stream, 0);
		if (ret) {
			dev_dbg(cam->dev, "failed to stream off %d: %d\n",
				ctx->pipe_subdevs[i]->name, ret);
			return -EPERM;
		}
	}
	if (mtk_cam_is_stagger(ctx))
		mtk_cam_img_working_buf_pool_release(ctx);
	mtk_camsys_ctrl_stop(ctx);

fail_stream_off:
#if CCD_READY
	if (ctx->used_raw_num) {
		/* FIXME: need to receive destroy ack then to destroy rproc_phandle */
		isp_composer_destroy_session(ctx);
	}
#endif

	dev_dbg(cam->dev, "streamed off camsys ctx:%d\n", ctx->stream_id);

	return 0;
}

static int config_bridge_pad_links(struct mtk_cam_device *cam,
				   struct v4l2_subdev *seninf)
{
	struct media_entity *pipe_entity;
	unsigned int i;
	int ret;

	for (i = 0; i < cam->max_stream_num; i++) {
		if (i >= MTKCAM_SUBDEV_RAW_START && i < MTKCAM_SUBDEV_RAW_2) {
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
			ret = media_create_pad_link(&seninf->entity,
					PAD_SRC_PDAF0,
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
#endif
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
		    sd->entity.function == MEDIA_ENT_F_VID_IF_BRIDGE)
			ret = config_bridge_pad_links(cam, sd);

		i++;
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

	/* TODO: bind mraw ? */

	mtk_cam_create_links(cam_dev);
	/* Expose all subdev's nodes */
	ret = v4l2_device_register_subdev_nodes(&cam_dev->v4l2_dev);
	if (ret) {
		dev_dbg(dev, "Failed to register subdev nodes\n");
		goto fail_unreg_camsv_entities;
	}
#ifndef FPGA_EP
	mtk_cam_dvfs_init(cam_dev);
#endif
	dev_info(dev, "%s success\n", __func__);
	return 0;

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

	cam_dev->num_seninf_drivers =
		add_match_by_driver(dev, &match, &seninf_pdrv);

	if (IS_ERR(match))
		mtk_cam_match_remove(dev);

	dev_info(dev, "#: raw %d, yuv %d, larb %d, sv %d, seninf %d\n",
		 cam_dev->num_raw_drivers, yuv_num,
		 cam_dev->num_larb_drivers,
		 cam_dev->num_camsv_drivers,
		 cam_dev->num_seninf_drivers);

	return match ? match : ERR_PTR(-ENODEV);
}

static const struct component_master_ops mtk_cam_master_ops = {
	.bind = mtk_cam_master_bind,
	.unbind = mtk_cam_master_unbind,
};

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
	ctx->used_raw_dmas = 0;
	ctx->processing_buffer_list.cnt = 0;
	ctx->composed_buffer_list.cnt = 0;

	ctx->used_sv_num = 0;
	for (i = 0 ; i < MAX_SV_PIPES_PER_STREAM ; i++) {
		ctx->sv_pipe[i] = NULL;
		ctx->used_sv_dev[i] = 0;
		ctx->sv_dequeued_frame_seq_no[i] = 0;
	}

	INIT_LIST_HEAD(&ctx->using_buffer_list.list);
	INIT_LIST_HEAD(&ctx->composed_buffer_list.list);
	INIT_LIST_HEAD(&ctx->processing_buffer_list.list);
	INIT_LIST_HEAD(&ctx->sv_using_buffer_list.list);
	INIT_LIST_HEAD(&ctx->sv_processing_buffer_list.list);
	INIT_LIST_HEAD(&ctx->processing_img_buffer_list.list);
	spin_lock_init(&ctx->using_buffer_list.lock);
	spin_lock_init(&ctx->composed_buffer_list.lock);
	spin_lock_init(&ctx->processing_buffer_list.lock);
	spin_lock_init(&ctx->sv_using_buffer_list.lock);
	spin_lock_init(&ctx->sv_processing_buffer_list.lock);
	spin_lock_init(&ctx->streaming_lock);
	spin_lock_init(&ctx->processing_img_buffer_list.lock);
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
			    RAW_STATS_CFG_SIZE +
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
#ifdef CONFIG_MTK_IOMMU_PGTABLE_EXT
#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
	if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34)))
		dev_dbg(dev, "%s: No suitable DMA available\n", __func__);
#endif
#endif
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
#ifdef FPGA_EP
	writel_relaxed(0xffffffff, cam_dev->base + 0x8);
#endif
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

	mutex_init(&cam_dev->op_lock);

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
	mutex_destroy(&cam_dev->op_lock);

	return ret;
}

static int mtk_cam_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);

	pm_runtime_disable(dev);

	component_master_del(dev, &mtk_cam_master_ops);
	mtk_cam_match_remove(dev);

	mutex_destroy(&cam_dev->op_lock);
	mtk_cam_debug_fs_deinit(cam_dev);

	destroy_workqueue(cam_dev->link_change_wq);

	platform_driver_unregister(&mtk_cam_sv_driver);
	platform_driver_unregister(&mtk_cam_raw_driver);
	platform_driver_unregister(&mtk_cam_larb_driver);
	platform_driver_unregister(&seninf_core_pdrv);
	platform_driver_unregister(&seninf_pdrv);

	return 0;
}

static const struct dev_pm_ops mtk_cam_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_cam_pm_suspend, mtk_cam_pm_resume)
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

static void media_device_setup_link_hook(void *data,
			struct media_link *link, struct media_link_desc *linkd, int *ret)
{
	int ret_value;

	link->android_vendor_data1 = linkd->reserved[0];
	ret_value = __media_entity_setup_link(link, linkd->flags);
	link->android_vendor_data1 = 0;
	*ret = (ret_value < 0) ? ret_value : 1;
}

static void clear_reserved_fmt_fields_hook(void *data,
			struct v4l2_format *p, int *ret)
{
	CLEAR_AFTER_FIELD(p, fmt.pix_mp.reserved[3]);
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

	retval = v4l2_subdev_call(sd, pad, set_fmt, pad, format);
	*ret = (retval < 0) ? retval : 1;
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

	ret = register_trace_android_vh_media_device_setup_link(
			media_device_setup_link_hook, NULL);
	if (ret)
		pr_info("register android_vh_media_device_setup_link failed!\n");
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
	ret = register_trace_android_vh_v4l2subdev_set_fmt(
			v4l2subdev_set_fmt_hook, NULL);
	if (ret)
		pr_info("register android_vh_v4l2subdev_set_fmt failed!\n");
	ret = register_trace_android_vh_v4l2subdev_set_selection(
			v4l2subdev_set_selection_hook, NULL);
	if (ret)
		pr_info("register android_vh_v4l2subdev_set_selection failed!\n");
	ret = register_trace_android_vh_v4l2subdev_set_frame_interval(
			v4l2subdev_set_frame_interval_hook, NULL);
	if (ret)
		pr_info("register android_vh_v4l2subdev_set_frame_interval failed!\n");
}

static void mtk_cam_trace_exit(void)
{
	unregister_trace_android_vh_media_device_setup_link(media_device_setup_link_hook, NULL);
	unregister_trace_android_vh_clear_reserved_fmt_fields(clear_reserved_fmt_fields_hook, NULL);
	unregister_trace_android_vh_fill_ext_fmtdesc(fill_ext_fmtdesc_hook, NULL);
	unregister_trace_android_vh_clear_mask_adjust(clear_mask_adjust_hook, NULL);
	unregister_trace_android_vh_v4l2subdev_set_fmt(
		v4l2subdev_set_fmt_hook, NULL);
	unregister_trace_android_vh_v4l2subdev_set_selection(
		v4l2subdev_set_selection_hook, NULL);
	unregister_trace_android_vh_v4l2subdev_set_frame_interval(
		v4l2subdev_set_frame_interval_hook, NULL);
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
