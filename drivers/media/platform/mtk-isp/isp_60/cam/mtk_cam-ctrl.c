// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/list.h>

#include <media/v4l2-event.h>
#include <media/v4l2-subdev.h>

#include "mtk_cam-ctrl.h"
#include "mtk_cam.h"
#include "mtk_cam-raw.h"
#include "mtk_cam-pool.h"
#include "mtk_cam-regs.h"

#ifdef CONFIG_MTK_MMDVFS
#include "mmdvfs_pmqos.h"
#endif

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

static void mtk_cam_pmqos_get_clk(struct mtk_cam_device *cam)
{
	struct mtk_camsys_clkinfo *clk = &cam->camsys_ctrl.clk_info;

	u64 freq_cur = 0;
	int i;
#ifdef CONFIG_MTK_MMDVFS
	freq_cur = mmdvfs_qos_get_freq(PM_QOS_CAM_FREQ);
#else
	freq_cur = clk->clklv_target;
#endif
	for (i = 0 ; i < clk->clklv_num; i++) {
		if (freq_cur == clk->clklv[i])
			clk->clklv_idx = i;
	}
	dev_info(cam->dev, "[%s] get clk=%d, idx=%d, target=%d",
		 __func__, freq_cur,
		 clk->clklv_idx, clk->clklv_target);
}

static void mtk_cam_pmqos_update_clk(struct mtk_cam_device *cam)
{
	struct mtk_camsys_ctrl *camsys_ctrl = &cam->camsys_ctrl;
#ifdef CONFIG_MTK_MMDVFS
	pm_qos_update_request(&camsys_ctrl->isp_pmqos_req,
			      camsys_ctrl->clk_info.clklv_target);
#endif
	dev_dbg(cam->dev, "[%s] update clk=%d", __func__,
		camsys_ctrl->clk_info.clklv_target);
	mtk_cam_pmqos_get_clk(cam);
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

static void mtk_cam_link_try_change(struct mtk_cam_request *req)
{
	struct mtk_cam_device *cam = container_of(req->req.mdev,
						  struct mtk_cam_device,
						  media_dev);
	struct mtk_camsys_ctrl *camsys_ctrl = &cam->camsys_ctrl;

	if (camsys_ctrl->link_change_state == LINK_CHANGE_QUEUED &&
	    req == camsys_ctrl->link_change_req) {
		unsigned int i;

		for (i = 0; i < cam->max_stream_num; i++) {
			struct mtk_camsys_link_ctrl *link_ctrl;

			link_ctrl = &camsys_ctrl->link_ctrl[i];
			if ((cam->streaming_ctx & (1 << i)) &&
			    link_ctrl->active) {
				struct mtk_raw_pipeline *pipe;
				struct mtk_cam_ctx *ctx;
				int ret;

				pipe = link_ctrl->pipe;
				dev_dbg(cam->dev,
					"link changing:%d(%d)\n", i, pipe->id);

				ctx = &cam->ctxs[i];

				mtk_cam_seninf_set_camtg(ctx->seninf,
							 PAD_SRC_RAW0,
						ctx->stream_id);

				ret = v4l2_subdev_call(ctx->seninf, video,
						       s_stream, 1);
				if (ret) {
					dev_err(cam->dev,
						"failed to stream on seninf %s:%d\n",
						ctx->seninf->name, ret);
					break;
				}

				mtk_cam_dev_config(ctx, 1);
				link_ctrl->active = 0;
			}
		}
		camsys_ctrl->link_change_state = LINK_CHANGE_IDLE;
	}

	dev_dbg(cam->dev, "link change done\n");
}

static void mtk_cam_link_change_worker(struct work_struct *work)
{
	struct mtk_cam_request *req =
		container_of(work, struct mtk_cam_request, link_work);

	mtk_cam_link_try_change(req);
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

	if (cam->camsys_ctrl.link_change_state == LINK_CHANGE_QUEUED)
		mtk_cam_link_try_change(req); /* TODO: align with SCQ */

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
			v4l2_ctrl_request_complete(&req->req, parent_hdl);
		}
	}
	state_transition(&req->state, E_STATE_READY, E_STATE_SENSOR);
	if (ctx->prev_sensor)
		ctx->prev_sensor = NULL;
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
	/* Check if previous state was without cq done (STATE !=OUTER) */
	list_for_each_entry(state_entry, &sensor_ctrl->camsys_state_list,
			    state_element) {
		state_req = container_of(state_entry,
					 struct mtk_cam_request, state);
		if (state_req->frame_seq_no ==
		    sensor_ctrl->sensor_request_seq_no) {
			if (state_entry->estate == E_STATE_CQ && USINGSCQ &&
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
		if (stateidx < STATE_NUM_AT_SOF) {
			state_rec[stateidx] = state_temp;
			/* Find outer state element */
			if (state_temp->estate == E_STATE_OUTER ||
			    state_temp->estate == E_STATE_OUTER_HW_DELAY)
				state_outer = state_temp;
		}
		/* counter for state queue*/
		que_cnt++;
		dev_dbg(raw_dev->dev,
			"[SOF] STATE_CHECK [N-%d] Req:%d / State:%d\n",
			stateidx, req->frame_seq_no,
			state_rec[stateidx]->estate);
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
	struct mtk_cam_request *req = NULL;
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
	} else if (req == camsys_ctrl->link_change_req &&
			camsys_ctrl->link_change_state == LINK_CHANGE_QUEUED) {
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
		dev_dbg(raw_dev->dev,
			"SOF[ctx:%d-#%d], CQ is update, composed_frame_seq:%d, cq_addr:0x%x\n",
			ctx->stream_id, dequeued_frame_seq_no,
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
						REG_CAMCTL_FBC_RCNT_INC);
					writel_relaxed(val |
						       CAMCTL_IMGO_R1_RCNT_INC,
						       raw_dev->base +
						       REG_CAMCTL_FBC_RCNT_INC);
					wmb(); /* TBC */
					state_transition(state_entry,
							 E_STATE_CQ,
							 E_STATE_OUTER);
					dev_dbg(raw_dev->dev,
						"[SWD] Forced write READCNT for next frame image\n");
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
}

void mtk_cam_pmqos_get_clkinfo(struct mtk_cam_device *cam)
{
	struct mtk_camsys_clkinfo *clk = &cam->camsys_ctrl.clk_info;
	u64 freq[ISP_CLK_LEVEL_CNT] = {0};
	int clk_num, i;
#ifdef CONFIG_MTK_MMDVFS
	mmdvfs_qos_get_freq_steps(PM_QOS_CAM_FREQ, freq, &clk_num);
#else
	clk_num = 4;
	freq[0] = 624;
	freq[1] = 499;
	freq[2] = 392;
	freq[3] = 312;
#endif
	memset((void *)clk, 0x0, sizeof(struct mtk_camsys_clkinfo));
	clk->clklv_num = clk_num;
	clk->clklv_target = (u32)freq[clk_num - 1];
	for (i = 0 ; i < clk_num; i++) {
		dev_info(cam->dev, "[%s] clk_%d:%d", __func__, i, freq[i]);
		clk->clklv[i] = freq[i];
	}
	mtk_cam_pmqos_get_clk(cam);
}

void mtk_cam_pmqos_add_req(struct mtk_cam_device *cam)
{
#ifdef CONFIG_MTK_MMDVFS
	struct mtk_camsys_ctrl *camsys_ctrl = &cam->camsys_ctrl;

	pm_qos_add_request(&camsys_ctrl->isp_pmqos_req, PM_QOS_CAM_FREQ, 0);
#endif
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
			dev_warn(raw_dev->dev, "cannot find ctx\n");
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
			dev_err(ctx->cam->dev,
				"failed to alloc sensor setting workqueue\n");
			return -ENOMEM;
		}
	}
	camsys_ctrl->link_change_state = LINK_CHANGE_IDLE;
	mtk_cam_pmqos_update_clk(ctx->cam);
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
	if (ctx->sensor) {
		hrtimer_cancel(&camsys_sensor_ctrl->sensor_deadline_timer);
		drain_workqueue(camsys_sensor_ctrl->sensorsetting_wq);
		destroy_workqueue(camsys_sensor_ctrl->sensorsetting_wq);
		camsys_sensor_ctrl->sensorsetting_wq = NULL;
	}
	dev_dbg(ctx->cam->dev, "[camsys:stop]  ctx:%d/raw_dev:%d\n",
		ctx->stream_id, camsys_ctrl->raw_dev[ctx->pipe->id]->id);
}

