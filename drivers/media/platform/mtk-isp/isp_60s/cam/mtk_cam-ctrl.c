// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/list.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>

#include <media/v4l2-event.h>
#include <media/v4l2-subdev.h>

#include "mtk_cam-ctrl.h"
#include "mtk_cam.h"
#include "mtk_cam-raw.h"
#include "mtk_cam-pool.h"
#include "mtk_cam-regs.h"




#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif

#define SENSOR_SET_DEADLINE_MS  18
#define SENSOR_SET_RESERVED_MS  7
#define SENSOR_SET_COUNT_BEFORE_STREAMON 1
#define STATE_NUM_AT_SOF 3

enum MTK_CAMSYS_STATE_RESULT {
	STATE_RESULT_TRIGGER_CQ = 0,
	STATE_RESULT_PASS_CQ_INIT,
	STATE_RESULT_PASS_CQ_SW_DELAY,
	STATE_RESULT_PASS_CQ_SCQ_DELAY,
	STATE_RESULT_PASS_CQ_HW_DELAY,
};

static void mtk_cam_dvfs_enumget_clktarget(struct mtk_cam_device *cam)
{
	struct mtk_camsys_dvfs *dvfs = &cam->camsys_ctrl.dvfs_info;
	int clk_streaming_max = dvfs->clklv[0];
	int i;

	for (i = 0;  i < cam->max_stream_num; i++) {
		if (cam->ctxs[i].streaming) {
			struct mtk_cam_resource_config *res =
				&cam->ctxs[i].pipe->res_config;
			if (clk_streaming_max < res->clk_target)
				clk_streaming_max = res->clk_target;
			dev_dbg(cam->dev, "on ctx:%d clk needed:%d", i, res->clk_target);
		}
	}
	dvfs->clklv_target = clk_streaming_max;
	dev_dbg(cam->dev, "[%s] dvfs->clk=%d", __func__, dvfs->clklv_target);
}

static void mtk_cam_dvfs_get_clkidx(struct mtk_cam_device *cam)
{
	struct mtk_camsys_dvfs *dvfs = &cam->camsys_ctrl.dvfs_info;

	u64 freq_cur = 0;
	int i;
	freq_cur = dvfs->clklv_target;
	for (i = 0 ; i < dvfs->clklv_num; i++) {
		if (freq_cur == dvfs->clklv[i])
			dvfs->clklv_idx = i;
	}
	dev_dbg(cam->dev, "[%s] get clk=%d, idx=%d",
		 __func__, dvfs->clklv_target, dvfs->clklv_idx);
}

static void mtk_cam_dvfs_update_clk(struct mtk_cam_device *cam)
{
	struct mtk_camsys_dvfs *dvfs = &cam->camsys_ctrl.dvfs_info;

	mtk_cam_dvfs_enumget_clktarget(cam);
	mtk_cam_dvfs_get_clkidx(cam);
	regulator_set_voltage(dvfs->reg, dvfs->voltlv[dvfs->clklv_idx], INT_MAX);
	dev_dbg(cam->dev, "[%s] update idx:%d clk:%d volt:%d", __func__,
		dvfs->clklv_idx, dvfs->clklv_target, dvfs->voltlv[dvfs->clklv_idx]);

}

static void state_transition(struct mtk_camsys_ctrl_state *state_entry,
			     enum MTK_CAMSYS_STATE_IDX from,
			     enum MTK_CAMSYS_STATE_IDX to)
{
	if (state_entry->estate == from)
		state_entry->estate = to;
}

static void mtk_cam_event_frame_sync(struct mtk_raw_device *raw_dev,
				     unsigned int frame_seq_no)
{
	struct v4l2_event event = {
		.type = V4L2_EVENT_FRAME_SYNC,
		.u.frame_sync.frame_sequence = frame_seq_no,
	};
	v4l2_event_queue(raw_dev->pipeline->subdev.devnode, &event);
}

static void mtk_cam_event_request_drained(struct mtk_raw_device *raw_dev)
{
	struct v4l2_event event = {
		.type = V4L2_EVENT_REQUEST_DRAINED,
	};
	v4l2_event_queue(raw_dev->pipeline->subdev.devnode, &event);
}

static void mtk_cam_link_try_swap(struct mtk_cam_request *req)
{
	struct mtk_cam_device *cam =
		container_of(req->req.mdev, struct mtk_cam_device, media_dev);
	struct mtk_cam_ctx *ctx, *ctx_swap;
	struct mtk_camsys_sensor_ctrl *camsys_ctrl, *camsys_ctrl_swap;
	struct mtk_camsys_link_ctrl *link_ctrl, *link_ctrl_swap;
	int i, ret;

	for (i = 0; i < cam->max_stream_num; i++) {
		if (req->ctx_used & (1 << cam->ctxs[i].stream_id))
			ctx = &cam->ctxs[i];
	}
	camsys_ctrl = &cam->camsys_ctrl.sensor_ctrl[ctx->stream_id];
	link_ctrl = &camsys_ctrl->link_ctrl;
	ctx_swap = link_ctrl->swapping_ctx;
	camsys_ctrl_swap = &cam->camsys_ctrl.sensor_ctrl[ctx_swap->stream_id];
	link_ctrl_swap = &camsys_ctrl_swap->link_ctrl;
	if (camsys_ctrl->link_change_state == LINK_CHANGE_QUEUED &&
					req == camsys_ctrl->link_change_req) {
		if (link_ctrl->active && link_ctrl_swap->active) {
			// stream_off both seninf
			ret = v4l2_subdev_call(ctx->seninf, video, s_stream, 0);
			if (ret)
				dev_info(cam->dev,
					"failed to stream off seninf %s:%d\n",
					ctx->seninf->name, ret);
			ret = v4l2_subdev_call(ctx_swap->seninf, video, s_stream, 0);
			if (ret)
				dev_info(cam->dev,
					"failed to stream off seninf %s:%d\n",
					ctx_swap->seninf->name, ret);
			// set both cam_mux
			mtk_cam_seninf_set_pixelmode(ctx->seninf, PAD_SRC_RAW0,
					ctx->pipe->res_config.tgo_pxl_mode);
			mtk_cam_seninf_set_camtg(ctx->seninf, PAD_SRC_RAW0,
					ctx->stream_id);
			mtk_cam_seninf_set_pixelmode(ctx_swap->seninf, PAD_SRC_RAW0,
					ctx_swap->pipe->res_config.tgo_pxl_mode);
			mtk_cam_seninf_set_camtg(ctx_swap->seninf, PAD_SRC_RAW0,
					ctx_swap->stream_id);
			// stream_on both seninf
			ret = v4l2_subdev_call(ctx->seninf, video,
				s_stream, 1);
			if (ret)
				dev_info(cam->dev,
					"failed to stream on seninf %s:%d\n",
					ctx->seninf->name, ret);
			ret = v4l2_subdev_call(ctx_swap->seninf, video,
				s_stream, 1);
			if (ret)
				dev_info(cam->dev,
					"failed to stream on seninf %s:%d\n",
					ctx_swap->seninf->name, ret);
			mtk_cam_dev_config(ctx, 1);
			mtk_cam_dev_config(ctx_swap, 1);
			link_ctrl->active = 0;
			link_ctrl_swap->active = 0;

		}
		camsys_ctrl->link_change_state = LINK_CHANGE_IDLE;
		camsys_ctrl_swap->link_change_state = LINK_CHANGE_IDLE;
	}
	if (ctx->prev_sensor || ctx->prev_seninf) {
		ctx->prev_sensor = NULL;
		ctx->prev_seninf = NULL;
	}
	if (ctx_swap->prev_sensor || ctx_swap->prev_seninf) {
		ctx_swap->prev_sensor = NULL;
		ctx_swap->prev_seninf = NULL;
	}
	dev_dbg(cam->dev, "link swap done\n");
}

static void mtk_cam_link_try_change(struct mtk_cam_request *req)
{
	struct mtk_cam_device *cam =
		container_of(req->req.mdev, struct mtk_cam_device, media_dev);
	struct mtk_cam_ctx *ctx;
	struct mtk_camsys_sensor_ctrl *camsys_ctrl;
	struct mtk_camsys_link_ctrl *link_ctrl;
	int i, ret;

	for (i = 0; i < cam->max_stream_num; i++) {
		if (req->ctx_used & (1 << cam->ctxs[i].stream_id))
			ctx = &cam->ctxs[i];
	}
	camsys_ctrl = &cam->camsys_ctrl.sensor_ctrl[ctx->stream_id];
	link_ctrl = &camsys_ctrl->link_ctrl;
	if (camsys_ctrl->link_change_state == LINK_CHANGE_QUEUED &&
					req == camsys_ctrl->link_change_req) {
		if (link_ctrl->active) {
			mtk_cam_dvfs_update_clk(ctx->cam);
			mtk_cam_seninf_set_pixelmode(ctx->seninf,
					PAD_SRC_RAW0, ctx->pipe->res_config.tgo_pxl_mode);
			mtk_cam_seninf_set_camtg(ctx->seninf,
					PAD_SRC_RAW0, ctx->stream_id);
			if (ctx->seninf->entity.stream_count == 0)
				ctx->seninf->entity.stream_count++;
			if (ctx->sensor->entity.stream_count == 0)
				ctx->sensor->entity.stream_count++;
			ctx->seninf->entity.pipe = &ctx->pipeline;
			ctx->sensor->entity.pipe = &ctx->pipeline;
			ret = v4l2_subdev_call(ctx->seninf, video,
				s_stream, 1);
			if (ret)
				dev_info(cam->dev,
					"failed to stream on seninf %s:%d\n",
					ctx->seninf->name, ret);
			mtk_cam_dev_config(ctx, 1);
			link_ctrl->active = 0;

		}
		camsys_ctrl->link_change_state = LINK_CHANGE_IDLE;
	}
	if (ctx->prev_sensor || ctx->prev_seninf) {
		ctx->prev_sensor = NULL;
		ctx->prev_seninf = NULL;
	}
	dev_dbg(cam->dev, "link change done\n");
}

static void mtk_cam_link_change_worker(struct work_struct *work)
{
	struct mtk_cam_request *req =
		container_of(work, struct mtk_cam_request, link_work);
	struct mtk_cam_device *cam =
		container_of(req->req.mdev, struct mtk_cam_device, media_dev);
	struct mtk_cam_ctx *ctx;
	struct mtk_camsys_sensor_ctrl *camsys_ctrl, *ctrl_swap;
	struct mtk_camsys_link_ctrl *link_ctrl, *link_ctrl_swap;
	int i;

	for (i = 0; i < cam->max_stream_num; i++) {
		if (req->ctx_used & (1 << cam->ctxs[i].stream_id))
			ctx = &cam->ctxs[i];
	}
	camsys_ctrl = &cam->camsys_ctrl.sensor_ctrl[ctx->stream_id];
	link_ctrl = &camsys_ctrl->link_ctrl;
	if (link_ctrl->wait_exchange) {
		ctrl_swap = &cam->camsys_ctrl.sensor_ctrl[link_ctrl->swapping_ctx->stream_id];
		link_ctrl_swap = &ctrl_swap->link_ctrl;
		dev_dbg(cam->dev, "link change worker (ctx:%d/%d) wait_exchange:%d/%d\n",
					ctx->stream_id, link_ctrl->swapping_ctx->stream_id,
					link_ctrl->wait_exchange, link_ctrl_swap->wait_exchange);
		mtk_cam_link_try_swap(req);
		link_ctrl->wait_exchange = 0;
		link_ctrl_swap->wait_exchange = 0;
	} else {
		mtk_cam_link_try_change(req);
	}
}

static void mtk_cam_request_drained(struct mtk_raw_device *raw_dev,
				    struct mtk_camsys_sensor_ctrl *sensor_ctrl)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request *req, *req_prev;
	unsigned long flags;
	int sensor_seq_no_next = sensor_ctrl->sensor_request_seq_no + 1;
	int res = 0;

	spin_lock_irqsave(&cam->running_job_lock, flags);
	list_for_each_entry_safe(req, req_prev, &cam->running_job_list, list) {
		/* Match by the en-queued request number */
		if (req->ctx_used & (1 << ctx->stream_id) &&
		    req->frame_seq_no == sensor_seq_no_next)
			res = 1;
	}
	spin_unlock_irqrestore(&cam->running_job_lock, flags);
	/* Send V4L2_EVENT_REQUEST_DRAINED event */
	if (res == 0) {
		mtk_cam_event_request_drained(raw_dev);
		dev_dbg(raw_dev->dev, "request_drained:(%d)\n",
			sensor_seq_no_next);
	}
}

static void mtk_cam_sensor_worker(struct work_struct *work)
{
	struct mtk_cam_request *req =
		container_of(work, struct mtk_cam_request, sensor_work);
	struct mtk_cam_device *cam =
		container_of(req->req.mdev, struct mtk_cam_device, media_dev);
	struct media_request_object *obj;
	struct v4l2_ctrl_handler *parent_hdl;
	struct mtk_cam_ctx *ctx;
	unsigned int i = 0, time_after_sof = 0;
	for (i = 0; i < cam->max_stream_num; i++) {
		if (req->ctx_used & (1 << cam->ctxs[i].stream_id)) {
			ctx = &cam->ctxs[i];
			dev_dbg(cam->dev, "sensor try set on ctx:%d\n", i);
			break;
		}
	}
	list_for_each_entry(obj, &req->req.objects, list) {
		if (likely(obj))
			parent_hdl = obj->priv;
		else
			continue;
		if (parent_hdl == ctx->sensor->ctrl_handler ||
		    (ctx->prev_sensor && parent_hdl ==
		     ctx->prev_sensor->ctrl_handler)) {
			struct mtk_camsys_sensor_ctrl *sensor_ctrl =
				&cam->camsys_ctrl.sensor_ctrl[ctx->stream_id];
			v4l2_ctrl_request_setup(&req->req, parent_hdl);
			time_after_sof = ktime_get_boottime_ns() / 1000000 -
				sensor_ctrl->sof_time;
			dev_dbg(cam->dev,
				"[SOF+%dms] Sensor request:%d[ctx:%d] setup\n",
				time_after_sof, req->frame_seq_no,
				ctx->stream_id);
			state_transition(&req->state, E_STATE_READY, E_STATE_SENSOR);
			v4l2_ctrl_request_complete(&req->req, parent_hdl);
		}
	}
	dev_dbg(cam->dev, "sensor try set done\n");
}

static void mtk_cam_set_sensor(struct mtk_cam_request *current_req,
			       struct mtk_camsys_sensor_ctrl *sensor_ctrl)
{
	unsigned long flags;
	/* EnQ this request's state element to state_list (STATE:READY) */
	spin_lock_irqsave(&sensor_ctrl->camsys_state_lock, flags);
	list_add_tail(&current_req->state.state_element,
		      &sensor_ctrl->camsys_state_list);
	sensor_ctrl->sensor_request_seq_no = current_req->frame_seq_no;
	spin_unlock_irqrestore(&sensor_ctrl->camsys_state_lock, flags);
	INIT_WORK(&current_req->sensor_work, mtk_cam_sensor_worker);
	queue_work(sensor_ctrl->sensorsetting_wq, &current_req->sensor_work);
}

static enum hrtimer_restart sensor_set_handler(struct hrtimer *t)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl =
		container_of(t, struct mtk_camsys_sensor_ctrl,
			     sensor_deadline_timer);
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_camsys_ctrl *camsys = &cam->camsys_ctrl;
	struct mtk_cam_request *current_req = NULL;
	struct mtk_cam_request *state_req = NULL;
	struct mtk_camsys_ctrl_state *state_entry;
	int sensor_seq_no_next = sensor_ctrl->sensor_request_seq_no + 1;
	int time_after_sof = ktime_get_boottime_ns() / 1000000 -
			   camsys->sensor_ctrl[ctx->stream_id].sof_time;

	spin_lock(&sensor_ctrl->camsys_state_lock);
	/* Check if previous state was without cq done */
	list_for_each_entry(state_entry, &sensor_ctrl->camsys_state_list,
				state_element) {
		state_req = container_of(state_entry, struct mtk_cam_request, state);
		if (state_req->frame_seq_no == sensor_ctrl->sensor_request_seq_no) {
			if (sensor_ctrl->link_change_state == LINK_CHANGE_QUEUED) {
				spin_unlock(&sensor_ctrl->camsys_state_lock);
				dev_dbg(cam->dev, "[TimerIRQ] LINK CHANGE STATE, ctx:%d req:%d\n",
				ctx->stream_id, state_req->frame_seq_no);
				return HRTIMER_NORESTART;
			} else if (state_entry->estate == E_STATE_CQ && USINGSCQ &&
			    state_req->frame_seq_no >
			    SENSOR_SET_COUNT_BEFORE_STREAMON) {
				state_entry->estate = E_STATE_CQ_SCQ_DELAY;
				dev_dbg(ctx->cam->dev,
					"[TimerIRQ] SCQ DELAY STATE\n");
				spin_unlock(&sensor_ctrl->camsys_state_lock);
				return HRTIMER_NORESTART;
			} else if (state_entry->estate <= E_STATE_SENSOR) {
				dev_dbg(ctx->cam->dev,
					"[TimerIRQ] wrong state:%d (sensor workqueue delay)\n",
					state_entry->estate);
				spin_unlock(&sensor_ctrl->camsys_state_lock);
				return HRTIMER_NORESTART;
			}

			break;
		}
	}
	spin_unlock(&sensor_ctrl->camsys_state_lock);
	current_req = mtk_cam_dev_get_req(cam, ctx, sensor_seq_no_next);
	if (current_req) {
		mtk_cam_set_sensor(current_req,
				   &camsys->sensor_ctrl[ctx->stream_id]);
		dev_dbg(cam->dev,
			"[TimerIRQ [SOF+%dms]] ctx:%d, sensor_req_seq_no:%d\n",
			time_after_sof, ctx->stream_id, sensor_seq_no_next);
	} else {
		dev_dbg(cam->dev,
			"[TimerIRQ [SOF+%dms]] ctx:%d, empty req_queue, sensor_req_seq_no:%d\n",
			time_after_sof, ctx->stream_id,
			sensor_ctrl->sensor_request_seq_no);
	}

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart sensor_deadline_timer_handler(struct hrtimer *t)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl =
		container_of(t, struct mtk_camsys_sensor_ctrl,
			     sensor_deadline_timer);
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_raw_device *raw_dev =
		cam->camsys_ctrl.raw_dev[ctx->pipe->id];
	ktime_t m_kt;
	int time_after_sof = ktime_get_boottime_ns() / 1000000 -
			   sensor_ctrl->sof_time;

	sensor_ctrl->sensor_deadline_timer.function = sensor_set_handler;
	m_kt = ktime_set(0, sensor_ctrl->timer_req_sensor * 1000000);
	dev_dbg(raw_dev->dev, "[SOF+%dms]\n", time_after_sof);
	/* handle V4L2_EVENT_REQUEST_DRAINED event */
	mtk_cam_request_drained(raw_dev, sensor_ctrl);
	hrtimer_forward_now(&sensor_ctrl->sensor_deadline_timer, m_kt);

	return HRTIMER_RESTART;
}

static void mtk_cam_sof_timer_setup(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl =
		&cam->camsys_ctrl.sensor_ctrl[ctx->stream_id];
	ktime_t m_kt;

	sensor_ctrl->sof_time = ktime_get_boottime_ns() / 1000000;
	sensor_ctrl->sensor_deadline_timer.function =
		sensor_deadline_timer_handler;
	sensor_ctrl->ctx = ctx;
	m_kt = ktime_set(0, sensor_ctrl->timer_req_event * 1000000);
	hrtimer_start(&sensor_ctrl->sensor_deadline_timer, m_kt,
		      HRTIMER_MODE_REL);
}

static int mtk_camsys_state_handle(struct mtk_raw_device *raw_dev,
				   struct mtk_camsys_sensor_ctrl *sensor_ctrl,
		struct mtk_camsys_ctrl_state **current_state,
		int frame_inner_idx)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_camsys_ctrl_state *state_temp, *state_outer = NULL;
	struct mtk_camsys_ctrl_state *state_rec[STATE_NUM_AT_SOF];
	struct mtk_cam_request *req;
	int stateidx;
	int que_cnt = 0;
	/* List state-queue status*/
	list_for_each_entry(state_temp, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req = container_of(state_temp, struct mtk_cam_request, state);
		stateidx = sensor_ctrl->sensor_request_seq_no -
			   req->frame_seq_no;
		if (stateidx < STATE_NUM_AT_SOF && stateidx > -1) {
			state_rec[stateidx] = state_temp;
			/* Find outer state element */
			if (state_temp->estate == E_STATE_OUTER ||
			    state_temp->estate == E_STATE_OUTER_HW_DELAY)
				state_outer = state_temp;
			dev_dbg(raw_dev->dev,
			"[SOF] STATE_CHECK [N-%d] Req:%d / State:%d\n",
			stateidx, req->frame_seq_no,
			state_rec[stateidx]->estate);
		}
		/* counter for state queue*/
		que_cnt++;
	}
	/* HW imcomplete case */
	if (que_cnt >= STATE_NUM_AT_SOF) {
		state_transition(state_rec[2], E_STATE_INNER,
				 E_STATE_INNER_HW_DELAY);
		state_transition(state_rec[1], E_STATE_OUTER,
				 E_STATE_OUTER_HW_DELAY);
		if (~USINGSCQ)
			state_transition(state_rec[1], E_STATE_CQ,
					 E_STATE_OUTER_HW_DELAY);
		dev_dbg(raw_dev->dev, "[SOF] HW_DELAY state\n");

		return STATE_RESULT_PASS_CQ_HW_DELAY;
	}
	/* Trigger high resolution timer to try sensor setting */
	mtk_cam_sof_timer_setup(ctx);
	/* Transit outer state to inner state */
	if (state_outer) {
		req = container_of(state_outer, struct mtk_cam_request,
				   state);
		if (req->frame_seq_no == frame_inner_idx) {
			if (frame_inner_idx ==
			   (sensor_ctrl->isp_request_seq_no + 1)) {
				state_transition(state_outer,
						 E_STATE_OUTER_HW_DELAY,
						 E_STATE_INNER_HW_DELAY);
				state_transition(state_outer, E_STATE_OUTER,
						 E_STATE_INNER);
				sensor_ctrl->isp_request_seq_no =
					frame_inner_idx;
				dev_dbg(raw_dev->dev,
					"[SOF-DBLOAD] req:%d, OUTER->INNER state:%d\n",
					req->frame_seq_no, state_outer->estate);
			}
		}
	}
	/* Initial request case */
	if (sensor_ctrl->sensor_request_seq_no <=
		SENSOR_SET_COUNT_BEFORE_STREAMON) {
		dev_dbg(raw_dev->dev, "[SOF] INIT STATE cnt:%d\n", que_cnt);
		if (que_cnt > 0)
			state_transition(state_rec[0], E_STATE_SENSOR,
					 E_STATE_CQ);

		return STATE_RESULT_PASS_CQ_INIT;
	}
	if (que_cnt > 0) {
		/* CQ triggering judgment*/
		if (state_rec[0]->estate == E_STATE_SENSOR) {
			*current_state = state_rec[0];
			return STATE_RESULT_TRIGGER_CQ;
		}
		/* last SCQ triggering delay judgment*/
		if (state_rec[0]->estate == E_STATE_CQ_SCQ_DELAY) {
			state_transition(state_rec[0], E_STATE_CQ_SCQ_DELAY,
					 E_STATE_OUTER);
			dev_dbg(raw_dev->dev, "[SOF] SCQ_DELAY state:%d\n",
				state_rec[0]->estate);
			return STATE_RESULT_PASS_CQ_SCQ_DELAY;
		}
	}

	return STATE_RESULT_PASS_CQ_SW_DELAY;
}

static void mtk_camsys_raw_frame_start(struct mtk_raw_device *raw_dev,
				       struct mtk_cam_ctx *ctx,
				       unsigned int dequeued_frame_seq_no)
{
	struct mtk_cam_device *cam = raw_dev->cam;
	struct mtk_cam_request *req = NULL, *req_cq = NULL;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl =
		&cam->camsys_ctrl.sensor_ctrl[ctx->stream_id];
	struct mtk_cam_working_buf_entry *buf_entry;
	struct mtk_camsys_ctrl_state *current_state;
	dma_addr_t base_addr;
	struct mtk_camsys_ctrl *camsys_ctrl = &cam->camsys_ctrl;
	enum MTK_CAMSYS_STATE_RESULT state_handle_ret;
	/* inner register dequeue number */
	ctx->dequeued_frame_seq_no = dequeued_frame_seq_no;
	/* Send V4L2_EVENT_FRAME_SYNC event */
	mtk_cam_event_frame_sync(raw_dev, dequeued_frame_seq_no);

	if (ctx->sensor) {
		state_handle_ret =
			mtk_camsys_state_handle(raw_dev, sensor_ctrl,
						&current_state,
						dequeued_frame_seq_no);
		if (state_handle_ret != STATE_RESULT_TRIGGER_CQ)
			return;
	} else if (req == sensor_ctrl->link_change_req &&
			sensor_ctrl->link_change_state == LINK_CHANGE_QUEUED) {
		INIT_WORK(&req->link_work, mtk_cam_link_change_worker);
		queue_work(cam->link_change_wq, &req->link_work);
	}

	/*Find request of this dequeued frame*/
	req = mtk_cam_dev_get_req(cam, ctx, dequeued_frame_seq_no);
	if (req)
		req->timestamp = ktime_get_boottime_ns();
	/* Update CQ base address if needed */
	if (ctx->composed_frame_seq_no <= dequeued_frame_seq_no) {
		dev_dbg(raw_dev->dev,
			"SOF[ctx:%d-#%d], CQ isn't updated [composed_frame_deq (%d) ]\n",
			ctx->stream_id, dequeued_frame_seq_no,
			ctx->composed_frame_seq_no);
		return;
	}
	/* apply next composed buffer */
	spin_lock(&ctx->composed_buffer_list.lock);
	if (list_empty(&ctx->composed_buffer_list.list)) {
		dev_dbg(raw_dev->dev,
			"SOF_INT_ST, no buffer update, cq_num:%d, frame_seq:%d\n",
			ctx->composed_frame_seq_no, dequeued_frame_seq_no);
		spin_unlock(&ctx->composed_buffer_list.lock);
	} else {
		buf_entry = list_first_entry(&ctx->composed_buffer_list.list,
					     struct mtk_cam_working_buf_entry,
					     list_entry);
		list_del(&buf_entry->list_entry);
		ctx->composed_buffer_list.cnt--;
		spin_unlock(&ctx->composed_buffer_list.lock);
		spin_lock(&ctx->processing_buffer_list.lock);
		list_add_tail(&buf_entry->list_entry,
			      &ctx->processing_buffer_list.list);
		ctx->processing_buffer_list.cnt++;
		spin_unlock(&ctx->processing_buffer_list.lock);
		base_addr = buf_entry->buffer.iova;
		apply_cq(raw_dev, base_addr, buf_entry->cq_size, 0);
		/* Transit state from Sensor -> CQ */
		if (ctx->sensor)
			state_transition(current_state, E_STATE_SENSOR,
					 E_STATE_CQ);
		req_cq = container_of(current_state, struct mtk_cam_request, state);
		dev_dbg(raw_dev->dev,
			"SOF[ctx:%d-#%d], CQ-%d is update, composed:%d, cq_addr:0x%x\n",
			ctx->stream_id, dequeued_frame_seq_no, req_cq->frame_seq_no,
			ctx->composed_frame_seq_no, base_addr);
	}
}

static void mtk_camsys_raw_cq_done(struct mtk_raw_device *raw_dev,
				   struct mtk_cam_ctx *ctx,
				   unsigned int frame_seq_no_outer)
{
	struct mtk_cam_device *cam = raw_dev->cam;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl =
		&cam->camsys_ctrl.sensor_ctrl[ctx->stream_id];
	struct mtk_camsys_ctrl_state *state_entry;
	struct mtk_cam_request *req;
	/* initial CQ done */
	if (raw_dev->sof_count == 0) {
		spin_lock(&ctx->streaming_lock);
		if (ctx->streaming)
			stream_on(raw_dev, 1);
		spin_unlock(&ctx->streaming_lock);
	}
	/* Legacy CQ done will be always happened at frame done */
	spin_lock(&sensor_ctrl->camsys_state_lock);
	list_for_each_entry(state_entry, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req = container_of(state_entry, struct mtk_cam_request, state);
		if (req->frame_seq_no == frame_seq_no_outer) {
			if (frame_seq_no_outer ==
			    (sensor_ctrl->isp_request_seq_no + 1)) {
				/**
				 * outer number is 1 more from last SOF's
				 * inner number
				 */
				state_transition(state_entry, E_STATE_CQ,
						 E_STATE_OUTER);
				state_transition(state_entry,
						 E_STATE_CQ_SCQ_DELAY,
						 E_STATE_OUTER);
				dev_dbg(raw_dev->dev,
					"[CQD] req:%d, CQ->OUTER state:%d\n",
					req->frame_seq_no, state_entry->estate);
			}
		}
	}
	spin_unlock(&sensor_ctrl->camsys_state_lock);
}

static void mtk_camsys_raw_frame_done(struct mtk_raw_device *raw_dev,
				      struct mtk_cam_ctx *ctx,
				      unsigned int dequeued_frame_seq_no)
{
	struct mtk_cam_device *cam = raw_dev->cam;
	struct device *dev = raw_dev->dev;
	struct mtk_camsys_sensor_ctrl *camsys_sensor_ctrl =
			&cam->camsys_ctrl.sensor_ctrl[ctx->stream_id];
	struct mtk_cam_working_buf_entry *buf_entry;
	struct mtk_camsys_ctrl_state *state_entry;
	struct mtk_cam_request *state_req;
	struct mtk_cam_request *req;
	struct mtk_cam_ctx *ctx_swap;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl_swap;
	int val;

	if (ctx->sensor) {
		spin_lock(&camsys_sensor_ctrl->camsys_state_lock);
		/**
		 * Find inner register number's request and transit to
		 * STATE_DONE_xxx
		 */
		list_for_each_entry(state_entry,
				    &camsys_sensor_ctrl->camsys_state_list,
				    state_element) {
			state_req = container_of(state_entry,
						 struct mtk_cam_request, state);
			if (state_req->frame_seq_no ==
				ctx->dequeued_frame_seq_no) {
				state_transition(state_entry,
						 E_STATE_INNER_HW_DELAY,
						 E_STATE_DONE_MISMATCH);
				state_transition(state_entry, E_STATE_INNER,
						 E_STATE_DONE_NORMAL);
				dev_dbg(raw_dev->dev, "[SWD] req:%d/state:%d\n",
					state_req->frame_seq_no,
					state_entry->estate);
				if (camsys_sensor_ctrl->isp_request_seq_no ==
				    0) {
					val = readl_relaxed(raw_dev->base +
						REG_CTL_DMA_EN);
					writel_relaxed(val |
						       CAMCTL_IMGO_R1_RCNT_INC,
						       raw_dev->base +
						       REG_CAMCTL_FBC_RCNT_INC);
					wmb(); /* TBC */
					state_transition(state_entry,
							 E_STATE_CQ,
							 E_STATE_OUTER);
					dev_dbg(raw_dev->dev,
						"[SWD] Forced write READCNT:0x%x\n", val);
				}
			}
		}
		spin_unlock(&camsys_sensor_ctrl->camsys_state_lock);
		/* Initial request readout will be delayed 1 frame*/
		if (camsys_sensor_ctrl->isp_request_seq_no == 0) {
			dev_dbg(dev,
				"1st SWD passed for initial request setting\n");
			return;
		}
	}
	spin_lock(&ctx->processing_buffer_list.lock);
	buf_entry = list_first_entry(&ctx->processing_buffer_list.list,
				     struct mtk_cam_working_buf_entry,
				     list_entry);
	list_del(&buf_entry->list_entry);
	ctx->processing_buffer_list.cnt--;
	mtk_cam_working_buf_put(cam, buf_entry);
	spin_unlock(&ctx->processing_buffer_list.lock);
	mtk_cam_dequeue_req_frame(cam, ctx);
	mtk_cam_dev_req_try_queue(cam);
	// handle monitoring 1st request after dynamic change
	if (camsys_sensor_ctrl->link_change_state == LINK_CHANGE_QUEUED) {
		if (camsys_sensor_ctrl->link_ctrl.wait_exchange) {
			ctx_swap = camsys_sensor_ctrl->link_ctrl.swapping_ctx;
			sensor_ctrl_swap = &cam->camsys_ctrl.sensor_ctrl[ctx_swap->stream_id];
			if (sensor_ctrl_swap->link_change_state == LINK_CHANGE_QUEUED) {
				if ((ctx->dequeued_frame_seq_no ==
					camsys_sensor_ctrl->link_change_req->frame_seq_no - 1) &&
					(ctx_swap->dequeued_frame_seq_no ==
					sensor_ctrl_swap->link_change_req->frame_seq_no - 1)) {
					req = mtk_cam_dev_get_req(cam, ctx,
							dequeued_frame_seq_no + 1);
					INIT_WORK(&req->link_work, mtk_cam_link_change_worker);
					queue_work(cam->link_change_wq, &req->link_work);
					dev_dbg(raw_dev->dev, "[SWD] exchange streams [req ctx:%d ctx_swap:%d]\n",
							ctx->stream_id, ctx_swap->stream_id);
				} else
					dev_dbg(raw_dev->dev, "[SWD] wait both done ctx:%d:%d (swapctx:%d:%d)\n",
						ctx->stream_id, ctx->dequeued_frame_seq_no,
						ctx_swap->stream_id,
						ctx_swap->dequeued_frame_seq_no);
			} else
				dev_dbg(raw_dev->dev, "[SWD] wait both enQed ctx:%d:%d (swapctx:%d:%d)\n",
					ctx->stream_id, ctx->dequeued_frame_seq_no,
					ctx_swap->stream_id, ctx_swap->dequeued_frame_seq_no);
		} else {
			req = mtk_cam_dev_get_req(cam, ctx, dequeued_frame_seq_no + 1);
			if (req == NULL)
				return;
			if (ctx->dequeued_frame_seq_no ==
				camsys_sensor_ctrl->link_change_req->frame_seq_no - 1) {
				INIT_WORK(&req->link_work, mtk_cam_link_change_worker);
				queue_work(cam->link_change_wq, &req->link_work);
			}
		}
	}
	return;
}

void mtk_cam_dvfs_uninit(struct mtk_cam_device *cam)
{
	struct mtk_camsys_dvfs *dvfs_info = &cam->camsys_ctrl.dvfs_info;

	if (dvfs_info->clklv_num)
		dev_pm_opp_of_remove_table(dvfs_info->dev);
}

void mtk_cam_dvfs_init(struct mtk_cam_device *cam)
{
	struct mtk_camsys_dvfs *dvfs_info = &cam->camsys_ctrl.dvfs_info;
	struct dev_pm_opp *opp;
	unsigned long freq = 0;
	int ret = 0, clk_num = 0, i = 0;

	memset((void *)dvfs_info, 0x0, sizeof(struct mtk_camsys_dvfs));
	dvfs_info->dev = cam->dev;
	ret = dev_pm_opp_of_add_table(dvfs_info->dev);
	if (ret < 0) {
		dev_dbg(dvfs_info->dev, "fail to init opp table: %d\n", ret);
		return;
	}
	dvfs_info->reg = devm_regulator_get_optional(dvfs_info->dev, "dvfsrc-vcore");
	if (IS_ERR(dvfs_info->reg)) {
		dev_dbg(dvfs_info->dev, "can't get dvfsrc-vcore\n");
		return;
	}
	clk_num = dev_pm_opp_get_opp_count(dvfs_info->dev);
	while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(dvfs_info->dev, &freq))) {
		dvfs_info->clklv[i] = freq;
		dvfs_info->voltlv[i] = dev_pm_opp_get_voltage(opp);
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}
	dvfs_info->clklv_num = clk_num;
	dvfs_info->clklv_target = dvfs_info->clklv[0];
	dvfs_info->clklv_idx = 0;
	for (i = 0; i < dvfs_info->clklv_num; i++) {
		dev_info(cam->dev, "[%s] idx=%d, clk=%d volt=%d\n",
			 __func__, i, dvfs_info->clklv[i], dvfs_info->voltlv[i]);
	}
}

int mtk_camsys_isr_event(struct mtk_cam_device *cam,
			 struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_raw_device *raw_dev;
	struct mtk_cam_ctx *ctx;
	int sub_engine_type = irq_info->engine_id & MTK_CAMSYS_ENGINE_IDXMASK;
	int ret = 0;

	/**
	 * Here it will be implemented dispatch rules for some scenarios
	 * like twin/stagger/m-stream,
	 * such cases that camsys will collect all coworked sub-engine's
	 * signals and trigger some engine of them to do some job
	 * individually.
	 * twin - rawx2
	 * stagger - rawx1, camsv x2
	 * m-stream - rawx1 , camsv x2
	 */
	switch (sub_engine_type) {
	case MTK_CAMSYS_ENGINE_RAW_TAG:
		raw_dev = cam->camsys_ctrl.raw_dev[irq_info->engine_id -
			CAMSYS_ENGINE_RAW_BEGIN];
		ctx = mtk_cam_find_ctx(cam, &raw_dev->pipeline->subdev.entity);
		if (!ctx) {
			dev_dbg(raw_dev->dev, "cannot find ctx\n");
			ret = -EINVAL;
			break;
		}
		/* raw's CQ done */
		if (irq_info->irq_type & (1 << CAMSYS_IRQ_SETTING_DONE))
			mtk_camsys_raw_cq_done(raw_dev, ctx,
					       irq_info->frame_idx);
		/* raw's SW done */
		if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_DONE))
			mtk_camsys_raw_frame_done(raw_dev, ctx,
						  irq_info->frame_idx);
		/* raw's SOF */
		if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_START))
			mtk_camsys_raw_frame_start(raw_dev, ctx,
						   irq_info->frame_inner_idx);
		break;
	case MTK_CAMSYS_ENGINE_MRAW_TAG:
		/* struct mtk_mraw_device *mraw_dev; */
		break;
	case MTK_CAMSYS_ENGINE_CAMSV_TAG:
		/* struct mtk_camsv_device *camsv_dev; */
		break;
	default:
		break;
	}

	return ret;
}

void mtk_cam_initial_sensor_setup(struct mtk_cam_request *initial_req,
				  struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl =
		&cam->camsys_ctrl.sensor_ctrl[ctx->stream_id];

	sensor_ctrl->ctx = ctx;
	mtk_cam_set_sensor(initial_req, sensor_ctrl);
	dev_dbg(ctx->cam->dev, "Initial sensor timer setup\n");
}

int mtk_camsys_ctrl_start(struct mtk_cam_ctx *ctx)
{
	struct mtk_camsys_ctrl *camsys_ctrl = &ctx->cam->camsys_ctrl;
	struct mtk_camsys_sensor_ctrl *camsys_sensor_ctrl =
		&camsys_ctrl->sensor_ctrl[ctx->stream_id];
	int fps_factor = ctx->pipe->res_config.pixel_rate /
			 ctx->pipe->cfg[MTK_RAW_SINK].mbus_fmt.width /
			 ctx->pipe->cfg[MTK_RAW_SINK].mbus_fmt.height / 30;
	camsys_ctrl->raw_dev[ctx->pipe->id] =
		dev_get_drvdata(ctx->cam->raw.devs[ctx->pipe->id]);
	camsys_sensor_ctrl->sensor_request_seq_no = 0;
	camsys_sensor_ctrl->isp_request_seq_no = 0;
	camsys_sensor_ctrl->sof_time = 0;
	camsys_sensor_ctrl->timer_req_event = fps_factor > 1 ?
			(SENSOR_SET_DEADLINE_MS / fps_factor) :
			SENSOR_SET_DEADLINE_MS;
	camsys_sensor_ctrl->timer_req_sensor = fps_factor > 1 ?
			(SENSOR_SET_RESERVED_MS / fps_factor) :
			SENSOR_SET_RESERVED_MS;
	INIT_LIST_HEAD(&camsys_sensor_ctrl->camsys_state_list);
	spin_lock_init(&camsys_sensor_ctrl->camsys_state_lock);
	spin_lock_init(&camsys_sensor_ctrl->link_change_lock);
	if (ctx->sensor) {
		hrtimer_init(&camsys_sensor_ctrl->sensor_deadline_timer,
			     CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		camsys_sensor_ctrl->sensor_deadline_timer.function =
			sensor_deadline_timer_handler;
		camsys_sensor_ctrl->sensorsetting_wq =
			alloc_ordered_workqueue(dev_name(ctx->cam->dev),
						__WQ_LEGACY | WQ_MEM_RECLAIM |
						WQ_FREEZABLE);
		if (!camsys_sensor_ctrl->sensorsetting_wq) {
			dev_dbg(ctx->cam->dev,
				"failed to alloc sensor setting workqueue\n");
			return -ENOMEM;
		}
	}
	camsys_sensor_ctrl->link_change_state = LINK_CHANGE_IDLE;
	mtk_cam_dvfs_update_clk(ctx->cam);
	dev_dbg(ctx->cam->dev, "[camsys:start]  ctx:%d/raw_dev:%d\n",
		ctx->stream_id, camsys_ctrl->raw_dev[ctx->pipe->id]->id);

	return 0;
}

void mtk_camsys_ctrl_stop(struct mtk_cam_ctx *ctx)
{
	struct mtk_camsys_ctrl *camsys_ctrl = &ctx->cam->camsys_ctrl;
	struct mtk_camsys_sensor_ctrl *camsys_sensor_ctrl =
		&camsys_ctrl->sensor_ctrl[ctx->stream_id];
	struct mtk_camsys_ctrl_state *state_entry, *state_entry_prev;
	unsigned long flags;

	spin_lock_irqsave(&camsys_sensor_ctrl->camsys_state_lock, flags);
	list_for_each_entry_safe(state_entry, state_entry_prev,
				 &camsys_sensor_ctrl->camsys_state_list,
				 state_element) {
		list_del(&state_entry->state_element);
	}
	spin_unlock_irqrestore(&camsys_sensor_ctrl->camsys_state_lock, flags);
	mtk_cam_dvfs_update_clk(ctx->cam);
	if (ctx->sensor) {
		hrtimer_cancel(&camsys_sensor_ctrl->sensor_deadline_timer);
		drain_workqueue(camsys_sensor_ctrl->sensorsetting_wq);
		destroy_workqueue(camsys_sensor_ctrl->sensorsetting_wq);
		camsys_sensor_ctrl->sensorsetting_wq = NULL;
	}
	dev_dbg(ctx->cam->dev, "[camsys:stop]  ctx:%d/raw_dev:%d\n",
		ctx->stream_id, camsys_ctrl->raw_dev[ctx->pipe->id]->id);
}

