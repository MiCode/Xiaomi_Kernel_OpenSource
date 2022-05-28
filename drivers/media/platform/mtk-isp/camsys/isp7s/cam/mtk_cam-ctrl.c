// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>

#include <media/v4l2-event.h>
#include <media/v4l2-subdev.h>

#include "mtk_cam.h"
#include "mtk_cam-feature.h"

#include "mtk_cam-ctrl.h"
#include "mtk_cam-debug.h"
#include "mtk_cam-dvfs_qos.h"
#include "mtk_cam-hsf.h"
#include "mtk_cam-pool.h"
#include "mtk_cam-raw.h"
#include "mtk_cam-regs.h"
//#include "mtk_cam-raw_debug.h"
#include "mtk_cam-sv-regs.h"
#include "mtk_cam-mraw-regs.h"
//#include "mtk_cam-tg-flash.h"
#include "mtk_camera-v4l2-controls.h"
#include "mtk_camera-videodev2.h"
// #include "mtk_cam-trace.h"

#include "imgsys/mtk_imgsys-cmdq-ext.h"

#include "frame_sync_camsys.h"

#define STATE_NUM_AT_SOF 5

#define MTK_CAM_CTRL_STATE_START 0
#define MTK_CAM_CTRL_STATE_STOPPING 1
#define MTK_CAM_CTRL_STATE_END 2



#define v4l2_set_frame_interval_which(x, y) (x.reserved[0] = y)

static int mtk_cam_ctrl_get(struct mtk_cam_ctrl *cam_ctrl)
{
	struct mtk_cam_ctx *ctx = cam_ctrl->ctx;

	if (atomic_read(&cam_ctrl->stopped) || !ctx) {
		pr_info("%s stop case : ref_cnt:%d",
			__func__, atomic_read(&cam_ctrl->ref_cnt));
		return -1;
	}
	atomic_inc_return(&cam_ctrl->ref_cnt);
	//dev_dbg(ctx->cam->dev, "%s streaming case : ref_cnt:%d",
	//		__func__, atomic_read(&cam_ctrl->ref_cnt));

	return 0;
}
static int mtk_cam_ctrl_put(struct mtk_cam_ctrl *cam_ctrl)
{
	struct mtk_cam_ctx *ctx = cam_ctrl->ctx;

	if (atomic_read(&cam_ctrl->stopped)) {
		if (atomic_dec_return(&cam_ctrl->ref_cnt) == 0) {
			wake_up_interruptible(&cam_ctrl->stop_wq);
			atomic_set(&cam_ctrl->stopped, MTK_CAM_CTRL_STATE_END);
			dev_info(ctx->cam->dev, "[%s] stop case : ref_cnt:%d",
				__func__, atomic_read(&cam_ctrl->ref_cnt));
		}
		return 0;
	}
	atomic_dec_return(&cam_ctrl->ref_cnt);
	//dev_dbg(ctx->cam->dev, "%s streaming case : ref_cnt:%d",
	//		__func__, atomic_read(&cam_ctrl->ref_cnt));

	return 0;
}

static void mtk_cam_event_eos(struct mtk_cam_ctrl *cam_ctrl)
{
	struct v4l2_event event = {
		.type = V4L2_EVENT_EOS,
	};
	mtk_cam_ctx_send_raw_event(cam_ctrl->ctx, &event);
}

static void mtk_cam_event_frame_sync(struct mtk_cam_ctrl *cam_ctrl,
				     unsigned int frame_seq_no)
{
	struct v4l2_event event = {
		.type = V4L2_EVENT_FRAME_SYNC,
		.u.frame_sync.frame_sequence = frame_seq_no,
	};
	mtk_cam_ctx_send_raw_event(cam_ctrl->ctx, &event);
}


static void mtk_cam_event_request_drained(struct mtk_cam_ctrl *cam_ctrl)
{
	struct v4l2_event event = {
		.type = V4L2_EVENT_REQUEST_DRAINED,
	};
	mtk_cam_ctx_send_raw_event(cam_ctrl->ctx, &event);
}

static bool mtk_cam_request_drained(struct mtk_cam_ctrl *cam_ctrl)
{
	int sensor_seq_no_next =
			atomic_read(&cam_ctrl->sensor_request_seq_no) + 1;
	int res = 0;

	if (sensor_seq_no_next <= atomic_read(&cam_ctrl->enqueued_frame_seq_no))
		res = 1;

	/* Send V4L2_EVENT_REQUEST_DRAINED event */
	if (res == 0) {
		atomic_set(&cam_ctrl->last_drained_seq_no, sensor_seq_no_next);
		mtk_cam_event_request_drained(cam_ctrl);
	}

	return (res == 0);
}

static void mtk_cam_try_set_sensor(struct mtk_cam_ctrl *cam_ctrl)
{
	struct mtk_cam_device *cam = cam_ctrl->ctx->cam;
	struct mtk_cam_job *job, *job_sensor = NULL, *job_change = NULL;
	struct mtk_cam_job_event_info event_info;
	int from_sof = ktime_get_boottime_ns() / 1000000 -
			   cam_ctrl->sof_time;
	int action = 0;

	/* create update event for job */
	event_info.ctx_id = cam_ctrl->ctx->stream_id;
	event_info.frame_idx = atomic_read(&cam_ctrl->sensor_request_seq_no) + 1;
	event_info.frame_idx_inner = atomic_read(&cam_ctrl->sensor_request_seq_no);
	event_info.ts_ns = cam_ctrl->sof_time * 1000000;
	event_info.isp_request_seq_no = atomic_read(&cam_ctrl->isp_request_seq_no);
	event_info.reset_seq_no = atomic_read(&cam_ctrl->reset_seq_no);
	event_info.isp_enq_seq_no = atomic_read(&cam_ctrl->isp_enq_seq_no);
	spin_lock(&cam_ctrl->camsys_state_lock);
	/* Check if previous state was without cq done */
	list_for_each_entry(job, &cam_ctrl->camsys_state_list,
			    list) {
		job->ops.update_sensor_try_set_event(job, &event_info, &action);
		if (action & BIT(CAM_JOB_APPLY_SENSOR))
			job_sensor = job;
		if (action & BIT(CAM_JOB_SENSOR_EXPNUM_CHANGE))
			job_change = job;
	}
	spin_unlock(&cam_ctrl->camsys_state_lock);

	/* job layer call v4l2_setup_ctrl */
	if (job_sensor) {
		atomic_set(&cam_ctrl->sensor_request_seq_no,
			job_sensor->frame_seq_no);
		job_sensor->ops.apply_sensor(job_sensor);
		dev_info(cam->dev, "[%s] ctx:%d, seq:%d (SOF+%dms)\n", __func__,
			job_sensor->src_ctx->stream_id, job_sensor->frame_seq_no, from_sof);
	}
	/* job layer call v4l2_setup_ctrl */
	if (job_change) {
		job_change->ops.wait_apply_sensor(job_change);
		atomic_set(&cam_ctrl->sensor_request_seq_no,
			job_change->frame_seq_no);
		dev_info(cam->dev, "[%s] ctx:%d, seq:%d, exposure change(SOF+%dms)\n", __func__,
			job_change->src_ctx->stream_id, job_change->frame_seq_no, from_sof);
	}
}
/* workqueue context */
static void
mtk_cam_meta1_done_work(struct work_struct *work)
{
	struct mtk_cam_job_work *meta1_done_work =
		container_of(work, struct mtk_cam_job_work, work);
	struct mtk_cam_job *job =
		container_of(meta1_done_work, struct mtk_cam_job, meta1_done_work);
	struct mtk_cam_ctrl *cam_ctrl = &job->src_ctx->cam_ctrl;

	if (mtk_cam_ctrl_get(cam_ctrl))
		return;
	job->ops.afo_done(job);
	mtk_cam_ctrl_put(cam_ctrl);
}

static int
mtk_cam_submit_meta1_done_work(struct mtk_cam_ctrl *cam_ctrl,
	struct mtk_cam_job *job)
{

	queue_work(cam_ctrl->ctx->frame_done_wq, &job->meta1_done_work.work);

	return 0;
}
/* workqueue context */
static void
mtk_cam_frame_done_work(struct work_struct *work)
{
	struct mtk_cam_job_work *frame_done_work =
		container_of(work, struct mtk_cam_job_work, work);
	struct mtk_cam_job *job =
		container_of(frame_done_work, struct mtk_cam_job, frame_done_work);
	struct mtk_cam_ctrl *cam_ctrl = &job->src_ctx->cam_ctrl;
	struct mtk_cam_job *_job, *_job_prev;

	if (mtk_cam_ctrl_get(cam_ctrl))
		return;
	spin_lock(&cam_ctrl->camsys_state_lock);
	list_for_each_entry_safe(_job, _job_prev, &cam_ctrl->camsys_state_list, list) {
		if (_job == job) {
			dev_info(job->src_ctx->cam->dev, "[%s] ctx:%d/seq:%d (state delete)\n",
				__func__, job->src_ctx->stream_id, job->frame_seq_no);
			list_del(&job->list);
		}
	}
	spin_unlock(&cam_ctrl->camsys_state_lock);
	job->ops.frame_done(job);
	mtk_cam_ctx_job_finish(job->src_ctx, job);
	mtk_cam_ctrl_put(cam_ctrl);
}


static int
mtk_cam_submit_frame_done_work(struct mtk_cam_ctrl *cam_ctrl,
	struct mtk_cam_job *job)
{

	queue_work(cam_ctrl->ctx->frame_done_wq, &job->frame_done_work.work);

	return 0;
}
/* kthread context */
static void
mtk_cam_sensor_worker(struct kthread_work *work)
{
	struct mtk_cam_ctrl *cam_ctrl =
			container_of(work, struct mtk_cam_ctrl, work);

	if (mtk_cam_ctrl_get(cam_ctrl))
		return;
	mtk_cam_try_set_sensor(cam_ctrl);
	mtk_cam_ctrl_put(cam_ctrl);
}

static bool
mtk_cam_submit_kwork(struct kthread_worker *worker,
				 struct mtk_cam_ctrl *cam_ctrl)
{
	if (!worker) {
		pr_info("%s: not queue work since kthread_worker is null\n",
			__func__);

		return false;
	}
	if (atomic_read(&cam_ctrl->stopped)) {
		pr_info("%s: stop ctrl\n", __func__);

		return false;
	}
	return kthread_queue_work(worker, &cam_ctrl->work);
}
/* sw irq - hrtimer context */
static enum hrtimer_restart
sensor_deadline_timer_handler(struct hrtimer *t)
{
	struct mtk_cam_ctrl *cam_ctrl =
		container_of(t, struct mtk_cam_ctrl,
			     sensor_deadline_timer);
	int time_after_sof = ktime_get_boottime_ns() / 1000000 -
			   cam_ctrl->sof_time;
	bool drained_res = false;
	bool subsample_flow = atomic_read(&cam_ctrl->isp_enq_seq_no) > 0;
	if (mtk_cam_ctrl_get(cam_ctrl))
		return HRTIMER_NORESTART;
	/* handle V4L2_EVENT_REQUEST_DRAINED event */
	drained_res = mtk_cam_request_drained(cam_ctrl);

	if (drained_res == 0 && !subsample_flow)
		mtk_cam_submit_kwork(&cam_ctrl->ctx->sensor_worker, cam_ctrl);
	dev_dbg(cam_ctrl->ctx->cam->dev, "[%s] drained:%d [sof+%dms]\n",
		__func__, drained_res, time_after_sof);
	mtk_cam_ctrl_put(cam_ctrl);

	return HRTIMER_NORESTART;

}

static void
mtk_cam_sof_timer_setup(struct mtk_cam_ctrl *cam_ctrl)
{
	ktime_t m_kt;
	struct mtk_seninf_sof_notify_param param;
	int after_sof_ms = ktime_get_boottime_ns() / 1000000
			- cam_ctrl->sof_time;

	/*notify sof to sensor*/
	param.sd = cam_ctrl->ctx->seninf;
	param.sof_cnt = atomic_read(&cam_ctrl->sensor_request_seq_no);
	mtk_cam_seninf_sof_notify(&param);

	cam_ctrl->sensor_deadline_timer.function =
		sensor_deadline_timer_handler;
	if (after_sof_ms < 0)
		after_sof_ms = 0;
	else if (after_sof_ms > cam_ctrl->timer_req_event)
		after_sof_ms = cam_ctrl->timer_req_event;
	m_kt = ktime_set(0, cam_ctrl->timer_req_event * 1000000
			- after_sof_ms * 1000000);
	hrtimer_start(&cam_ctrl->sensor_deadline_timer, m_kt,
		      HRTIMER_MODE_REL);
}
static void handle_setting_done(struct mtk_cam_ctrl *cam_ctrl,
		unsigned int engine_id, struct mtk_cam_job_event_info *event_info)
{
	struct mtk_cam_job *job, *job_expswitch = NULL;
	struct mtk_cam_job *job_on = NULL, *job_senswitch = NULL;
	struct mtk_cam_job *job_enq = NULL;
	int action = 0;

	spin_lock(&cam_ctrl->camsys_state_lock);
	/* Check if previous state was without cq done */
	list_for_each_entry(job, &cam_ctrl->camsys_state_list,
				list) {
		job->ops.update_setting_done_event(job, event_info, &action);
		if (action & BIT(CAM_JOB_STREAM_ON))
			job_on = job;
		if (action & BIT(CAM_JOB_READ_ENQ_NO))
			job_enq = job;
		if (action & BIT(CAM_JOB_EXP_NUM_SWITCH))
			job_expswitch = job;
		if (action & BIT(CAM_JOB_SENSOR_SWITCH))
			job_senswitch = job;
		dev_dbg(cam_ctrl->ctx->cam->dev,
		"[%s] job:%d, state:0x%x, action:0x%x\n", __func__,
			job->frame_seq_no, job->state, action);
	}
	spin_unlock(&cam_ctrl->camsys_state_lock);

	/* stream on */
	if (job_on) {
		job_on->ops.stream_on(job_on, true);
		cam_ctrl->timer_req_event =
			mtk_cam_job_get_sensor_margin(job_on);
	}

	/* subsample */
	/* for main cq done in subsample mode , won't not increase outer counter */
	/* here use isp_enq_no instead */
	if (job_enq)
		atomic_set(&cam_ctrl->isp_enq_seq_no,
			job_enq->frame_seq_no);

	/* hdr exposure number switch */
	if (job_expswitch)
		pr_info("not ready");

	/*  sensor mode switch for cq done operation */
	if (job_senswitch)
		pr_info("not ready");

}
static void handle_meta1_done(struct mtk_cam_ctrl *cam_ctrl,
		unsigned int engine_id, struct mtk_cam_job_event_info *event_info)
{
	struct mtk_cam_job *job, *job_afodeq = NULL;
	int action = 0;

	/* inner register correction */
	event_info->frame_idx_inner = atomic_read(&cam_ctrl->dequeued_frame_seq_no);
	spin_lock(&cam_ctrl->camsys_state_lock);
	/* Check if previous state was without cq done */
	list_for_each_entry(job, &cam_ctrl->camsys_state_list,
				list) {
		job->ops.update_afo_done_event(job, event_info, &action);
		if (action & BIT(CAM_JOB_DEQUE_META1))
			job_afodeq = job;
	}
	spin_unlock(&cam_ctrl->camsys_state_lock);

	/* trigger cq */
	if (job_afodeq)
		mtk_cam_submit_meta1_done_work(cam_ctrl, job_afodeq);
}
static void handle_frame_done(struct mtk_cam_ctrl *cam_ctrl,
		unsigned int engine_id, struct mtk_cam_job_event_info *event_info)
{
	struct mtk_cam_job *job, *job_deq = NULL;
	int action = 0;

	/* inner register correction */
	event_info->isp_deq_seq_no = atomic_read(&cam_ctrl->dequeued_frame_seq_no);
	spin_lock(&cam_ctrl->camsys_state_lock);
	/* Check if previous state was without cq done */
	list_for_each_entry(job, &cam_ctrl->camsys_state_list,
				list) {
		job->ops.update_frame_done_event(job, event_info, &action);
		if (action & BIT(CAM_JOB_DEQUE_ALL))
			job_deq = job;
	}
	spin_unlock(&cam_ctrl->camsys_state_lock);

	/* trigger cq */
	if (job_deq)
		mtk_cam_submit_frame_done_work(cam_ctrl, job_deq);

}
static void handle_raw_frame_start(struct mtk_cam_ctrl *cam_ctrl,
		unsigned int engine_id, struct mtk_cam_job_event_info *event_info)
{
	struct mtk_cam_job *job, *job_cq = NULL, *job_timer = NULL, *job_swh = NULL;
	struct mtk_cam_job *job_rd_deqno = NULL, *job_vsync = NULL;
	struct mtk_cam_ctx *ctx = cam_ctrl->ctx;
	int from_sof = ktime_get_boottime_ns() / 1000000 -
			   cam_ctrl->sof_time;
	int action = 0;
	int vsync_engine_id = event_info->engine;

	spin_lock(&cam_ctrl->camsys_state_lock);
	/* Check if previous state was without cq done */
	list_for_each_entry(job, &cam_ctrl->camsys_state_list,
				list) {
		job->ops.update_frame_start_event(job, event_info, &action);
		dev_dbg(ctx->cam->dev,
		"[%s] job:%d, state:0x%x, action:0x%x\n", __func__,
			job->frame_seq_no, job->state, action);
		if (action & BIT(CAM_JOB_APPLY_CQ))
			job_cq = job;
		if (action & BIT(CAM_JOB_EXP_NUM_SWITCH))
			job_swh = job;
		if (action & BIT(CAM_JOB_SETUP_TIMER))
			job_timer = job;
		if (action & BIT(CAM_JOB_READ_DEQ_NO))
			job_rd_deqno = job;
		if (action & BIT(CAM_JOB_VSYNC))
			job_vsync = job;
	}
	spin_unlock(&cam_ctrl->camsys_state_lock);

	/*  vsync action update cam_ctrl->sof_time for scheduler coordinates */
	if (job_vsync) {
		if (!ctx) {
			dev_dbg(ctx->cam->dev, "cannot find ctx\n");
			return;
		}
		cam_ctrl->sof_time = event_info->ts_ns / 1000000;
		mtk_cam_event_frame_sync(cam_ctrl, event_info->frame_idx_inner);
		cam_ctrl->vsync_engine = vsync_engine_id;
	} else if (vsync_engine_id == cam_ctrl->vsync_engine) {
		cam_ctrl->sof_time = event_info->ts_ns / 1000000;
		mtk_cam_event_frame_sync(cam_ctrl, event_info->frame_idx_inner);
	}
	/* setup timer for drained event */
	if (job_timer)
		mtk_cam_sof_timer_setup(cam_ctrl);
	else if ((action & BIT(CAM_JOB_HW_DELAY)) == 0 &&
			vsync_engine_id == cam_ctrl->vsync_engine)
		mtk_cam_sof_timer_setup(cam_ctrl);
	/* update cam_ctrl->dequeued_frame_seq_no for sw done */
	if (job_rd_deqno) {
		atomic_set(&cam_ctrl->dequeued_frame_seq_no,
			event_info->frame_idx_inner);
		atomic_set(&cam_ctrl->isp_request_seq_no,
			event_info->frame_idx_inner);
	}
	/* switch exp */
	if (job_swh) {
		job_swh->ops.wake_up_apply_sensor(job_swh);
		job_swh->ops.apply_cam_mux(job_swh);
	}
	/* trigger cq */
	if (job_cq) {
		job_cq->ops.apply_isp(job_cq);
		dev_info(ctx->cam->dev, "[%s][out/in/deq:%d/%d/%d] ctx:%d, cq-%d triggered(SOF+%dms)\n",
			__func__, event_info->frame_idx, event_info->frame_idx_inner,
			cam_ctrl->dequeued_frame_seq_no, job_cq->src_ctx->stream_id,
			job_cq->frame_seq_no, from_sof);
	}
}

static int mtk_cam_event_handle_raw(struct mtk_cam_ctrl *cam_ctrl,
				       unsigned int engine_id,
				       struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_cam_job_event_info event_info;
	unsigned int ctx_id =
		decode_fh_reserved_data_to_ctx(irq_info->frame_idx);
	unsigned int seq_nearby =
		atomic_read(&cam_ctrl->enqueued_frame_seq_no);

	event_info.engine = (CAMSYS_ENGINE_RAW << 8) + engine_id;
	event_info.ctx_id = ctx_id;
	event_info.ts_ns = irq_info->ts_ns;
	event_info.frame_idx =
		decode_fh_reserved_data_to_seq(seq_nearby, irq_info->frame_idx);
	event_info.frame_idx_inner =
		decode_fh_reserved_data_to_seq(seq_nearby, irq_info->frame_idx_inner);
	event_info.write_cnt = irq_info->write_cnt;
	event_info.fbc_cnt = irq_info->fbc_cnt;
	event_info.isp_request_seq_no = atomic_read(&cam_ctrl->isp_request_seq_no);
	event_info.reset_seq_no = atomic_read(&cam_ctrl->reset_seq_no);
	event_info.isp_enq_seq_no = atomic_read(&cam_ctrl->isp_enq_seq_no);
	/* raw's CQ done */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_SETTING_DONE))
		handle_setting_done(cam_ctrl, engine_id, &event_info);

	/* raw's subsample case : try sensor setting */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_TRY_SENSOR_SET))
		mtk_cam_submit_kwork(&cam_ctrl->ctx->sensor_worker, cam_ctrl);
	/* raw's DMA done, we only allow AFO done here */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_AFO_DONE))
		handle_meta1_done(cam_ctrl, engine_id, &event_info);

	/* raw's SW done */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_DONE))
		handle_frame_done(cam_ctrl, engine_id, &event_info);

	/* raw's SOF (proc engine frame start) */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_START))
		handle_raw_frame_start(cam_ctrl, engine_id, &event_info);

	/* DCIF' SOF (dc link engine frame start (first exposure) ) */
	//if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_START_DCIF_MAIN)) {
		// handle_dcif_frame_start(); - TBC
	//}

	return 0;
}

static int mtk_camsys_event_handle_mraw(struct mtk_cam_device *cam,
					unsigned int engine_id,
					struct mtk_camsys_irq_info *irq_info)
{
#ifdef NOT_READY
	struct mtk_mraw_device *mraw_dev;
	struct mtk_raw_device *raw_dev;
	struct mtk_cam_ctx *ctx;
	int mraw_dev_index;
	unsigned int stream_id;
	unsigned int seq;

	mraw_dev = dev_get_drvdata(cam->mraw.devs[engine_id]);
	ctx = mtk_cam_find_ctx(cam, &mraw_dev->pipeline->subdev.entity);
	if (!ctx) {
		dev_dbg(mraw_dev->dev, "cannot find ctx\n");
		return -EINVAL;
	}
	stream_id = engine_id + MTKCAM_SUBDEV_MRAW_START;
	/* mraw's SW done */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_DONE)) {
		mraw_dev_index = mtk_cam_find_mraw_dev_index(ctx, mraw_dev->id);
		if (mraw_dev_index == -1) {
			dev_dbg(mraw_dev->dev,
				"cannot find mraw_dev_index(%d)", mraw_dev->id);
			return -EINVAL;
		}
		seq = ctx->mraw_dequeued_frame_seq_no[mraw_dev_index];
		mtk_camsys_frame_done(ctx, seq, stream_id);
	}
	/* mraw's SOF */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_START))
		mtk_camsys_mraw_frame_start(mraw_dev, ctx,
			irq_info->frame_idx_inner, irq_info->ts_ns);
	/* mraw's CQ done */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_SETTING_DONE)) {
		if (mtk_camsys_is_all_cq_done(ctx, stream_id)) {
			/* stream on after all pipes' cq done */
			if (irq_info->frame_idx == 1) {
				raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
				mtk_camsys_raw_cq_done(raw_dev, ctx, irq_info->frame_idx);
			} else {
				mtk_cam_mraw_vf_on(mraw_dev, 1);
			}
		}
	}
#endif
	return 0;
}

static int mtk_camsys_event_handle_camsv(struct mtk_cam_ctrl *cam_ctrl,
			unsigned int engine_id, struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_cam_ctx *ctx = cam_ctrl->ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_camsv_device *camsv_dev;
	struct mtk_cam_job_event_info event_info;
	unsigned int ctx_id =
		decode_fh_reserved_data_to_ctx(irq_info->frame_idx);
	unsigned int seq_nearby =
		atomic_read(&cam_ctrl->enqueued_frame_seq_no);
	bool bDcif = false;

	camsv_dev = dev_get_drvdata(cam->engines.sv_devs[engine_id]);
	bDcif = camsv_dev->pipeline->hw_scen &
		(1 << MTKCAM_IPI_HW_PATH_OFFLINE_SRT_DCIF_STAGGER);
	event_info.engine = (CAMSYS_ENGINE_CAMSV << 8) + engine_id;
	event_info.ctx_id = ctx_id;
	event_info.ts_ns = irq_info->ts_ns;
	event_info.frame_idx =
		decode_fh_reserved_data_to_seq(seq_nearby, irq_info->frame_idx);
	event_info.frame_idx_inner =
		decode_fh_reserved_data_to_seq(seq_nearby, irq_info->frame_idx_inner);
	event_info.write_cnt = irq_info->write_cnt;
	event_info.fbc_cnt = irq_info->fbc_cnt;
	event_info.isp_request_seq_no = atomic_read(&cam_ctrl->isp_request_seq_no);
	event_info.reset_seq_no = atomic_read(&cam_ctrl->reset_seq_no);
	if (camsv_dev->pipeline->hw_scen &
	    MTK_CAMSV_SUPPORTED_SPECIAL_HW_SCENARIO) {
		dev_dbg(camsv_dev->dev, "sv special hw scenario: %d/%d\n",
			camsv_dev->pipeline->master_pipe_id, ctx->stream_id);
		// first exposure camsv's SOF
		if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_START))
			if (camsv_dev->pipeline->exp_order == 0)
				handle_raw_frame_start(cam_ctrl, engine_id, &event_info);
		// first exposure camsv's frame done
		if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_DONE))
			if (camsv_dev->pipeline->exp_order == 0)
				cam_ctrl->component_dequeued_frame_seq_no =
					irq_info->frame_idx_inner;
	}
#ifdef NOT_READY
	else {
		ctx = mtk_cam_find_ctx(cam, &camsv_dev->pipeline->subdev.entity);
		if (!ctx) {
			dev_dbg(camsv_dev->dev, "cannot find ctx\n");
			return -EINVAL;
		}
		stream_id = engine_id + MTKCAM_SUBDEV_CAMSV_START;
		/* camsv's SW done */
		if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_DONE)) {
			sv_dev_index = mtk_cam_find_sv_dev_index(ctx, camsv_dev->id);
			if (sv_dev_index == -1) {
				dev_dbg(camsv_dev->dev,
					"cannot find sv_dev_index(%d)", camsv_dev->id);
				return -EINVAL;
			}
			seq = ctx->sv_dequeued_frame_seq_no[sv_dev_index];
			mtk_camsys_frame_done(ctx, seq, stream_id);
		}
		/* camsv's SOF */
		if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_START))
			mtk_camsys_camsv_frame_start(camsv_dev, ctx,
				irq_info->frame_idx_inner, irq_info->ts_ns);
	}
#endif

	return 0;
}

int mtk_cam_ctrl_isr_event(struct mtk_cam_device *cam,
			 enum MTK_CAMSYS_ENGINE_TYPE engine_type,
			 unsigned int engine_id,
			 struct mtk_camsys_irq_info *irq_info)
{
	unsigned int ctx_id =
		decode_fh_reserved_data_to_ctx(irq_info->frame_idx);
	struct mtk_cam_ctrl *cam_ctrl = &cam->ctxs[ctx_id].cam_ctrl;
	int ret = 0;

	/* TBC
	MTK_CAM_TRACE_BEGIN(BASIC, "irq_type %d, inner %d",
			    irq_info->irq_type, irq_info->frame_idx_inner);
	*/
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

	switch (engine_type) {
	case CAMSYS_ENGINE_RAW:
		if (mtk_cam_ctrl_get(cam_ctrl))
			return 0;
		ret = mtk_cam_event_handle_raw(cam_ctrl, engine_id, irq_info);
		mtk_cam_ctrl_put(cam_ctrl);
		break;
	case CAMSYS_ENGINE_MRAW:

		ret = mtk_camsys_event_handle_mraw(cam, engine_id, irq_info);

		break;
	case CAMSYS_ENGINE_CAMSV:
		if (mtk_cam_ctrl_get(cam_ctrl))
			return 0;
		ret = mtk_camsys_event_handle_camsv(cam_ctrl, engine_id, irq_info);
		mtk_cam_ctrl_put(cam_ctrl);
		break;
		break;
	case CAMSYS_ENGINE_SENINF:
		/* ToDo - cam mux setting delay handling */
		if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_DROP))
			dev_info(cam->dev, "MTK_CAMSYS_ENGINE_SENINF_TAG engine:%d type:0x%x\n",
				engine_id, irq_info->irq_type);
		break;
	default:
		break;
	}
	/* TBC
	MTK_CAM_TRACE_END(BASIC);
	*/
	return ret;
}

/* request queue */
void mtk_cam_ctrl_job_enque(struct mtk_cam_ctrl *cam_ctrl,
	struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx;
	u32 next_frame_seq;
	bool subsample_flow = atomic_read(&cam_ctrl->isp_enq_seq_no) > 0;

	if (mtk_cam_ctrl_get(cam_ctrl))
		return;
	ctx = cam_ctrl->ctx;
	next_frame_seq = atomic_inc_return(&cam_ctrl->enqueued_frame_seq_no);
	/* EnQ this request's state element to state_list (STATE:READY) */
	spin_lock(&cam_ctrl->camsys_state_lock);
	list_add_tail(&job->list,
		      &cam_ctrl->camsys_state_list);
	spin_unlock(&cam_ctrl->camsys_state_lock);
	job->frame_seq_no = next_frame_seq;
	INIT_WORK(&job->frame_done_work.work, mtk_cam_frame_done_work);
	INIT_WORK(&job->meta1_done_work.work, mtk_cam_meta1_done_work);
	job->ops.compose(job);
	dev_dbg(ctx->cam->dev, "[%s] ctx:%d, frame_no:%d, next_frame_seq:%d\n",
		__func__, ctx->stream_id, job->frame_seq_no, next_frame_seq);
	/* if this job's drained event been sent, then try set sensor at kthread */
	if (next_frame_seq <= 2 ||
		next_frame_seq == atomic_read(&cam_ctrl->last_drained_seq_no))
		if (!subsample_flow)
			mtk_cam_submit_kwork(&ctx->sensor_worker, cam_ctrl);
	mtk_cam_ctrl_put(cam_ctrl);
}
void mtk_cam_ctrl_job_composed(struct mtk_cam_ctrl *cam_ctrl,
	int frame_seq, struct mtkcam_ipi_frame_ack_result *cq_ret)
{
	struct mtk_cam_job *job, *job_composed = NULL;
	struct mtk_cam_device *cam;
	int fh_temp_ctx_id, fh_temp_seq;

	if (mtk_cam_ctrl_get(cam_ctrl))
		return;
	cam = cam_ctrl->ctx->cam;
	fh_temp_ctx_id = decode_fh_reserved_data_to_ctx(frame_seq);
	fh_temp_seq = decode_fh_reserved_data_to_seq(
		atomic_read(&cam_ctrl->enqueued_frame_seq_no), frame_seq);
	spin_lock(&cam_ctrl->camsys_state_lock);
	/* Check if previous state was without cq done */
	list_for_each_entry(job, &cam_ctrl->camsys_state_list,
				list) {
		if (job->frame_seq_no == fh_temp_seq) {
			job_composed = job;
			break;
		}

	}
	spin_unlock(&cam_ctrl->camsys_state_lock);
	if (job_composed) {
		if (job_composed->ctx_id == fh_temp_ctx_id) {
			job_composed->ops.compose_done(job_composed, cq_ret);
		} else {
			dev_info(cam->dev, "[%s] job->ctx_id/ fh_temp_ctx_id = %d/%d\n",
			__func__, job_composed->ctx_id, fh_temp_ctx_id);
		}
	} else {
		dev_info(cam->dev, "[%s] not found, ctx_id/ frame_id = %d/%d\n",
		__func__, fh_temp_ctx_id, fh_temp_seq);
	}
	mtk_cam_ctrl_put(cam_ctrl);
}

void mtk_cam_ctrl_start(struct mtk_cam_ctrl *cam_ctrl, struct mtk_cam_ctx *ctx)
{
	cam_ctrl->ctx = ctx;
	atomic_set(&cam_ctrl->stopped, MTK_CAM_CTRL_STATE_START);
	atomic_set(&cam_ctrl->reset_seq_no, 1);
	atomic_set(&cam_ctrl->enqueued_frame_seq_no, 0);
	atomic_set(&cam_ctrl->dequeued_frame_seq_no, 0);
	atomic_set(&cam_ctrl->sensor_request_seq_no, 0);
	atomic_set(&cam_ctrl->last_drained_seq_no, 0);
	atomic_set(&cam_ctrl->isp_request_seq_no, 0);
	atomic_set(&cam_ctrl->isp_enq_seq_no, 0);
	atomic_set(&cam_ctrl->isp_update_timer_seq_no, 0);
	cam_ctrl->initial_cq_done = 0;
	cam_ctrl->sof_time = ktime_get_boottime_ns() / 1000000;
	cam_ctrl->component_dequeued_frame_seq_no = 0;
	cam_ctrl->timer_req_event = 0;
	INIT_LIST_HEAD(&cam_ctrl->camsys_state_list);
	spin_lock_init(&cam_ctrl->camsys_state_lock);
	init_waitqueue_head(&cam_ctrl->stop_wq);
	if (ctx->sensor) {
		hrtimer_init(&cam_ctrl->sensor_deadline_timer,
			     CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		cam_ctrl->sensor_deadline_timer.function =
			sensor_deadline_timer_handler;
	}
	kthread_init_work(&cam_ctrl->work, mtk_cam_sensor_worker);

	dev_info(ctx->cam->dev, "[%s] ctx:%d\n", __func__, ctx->stream_id);
}

void mtk_cam_ctrl_stop(struct mtk_cam_ctrl *cam_ctrl)
{
	struct mtk_cam_ctx *ctx = cam_ctrl->ctx;
	struct mtk_cam_job *job, *job_prev;
	struct mtk_raw_device *raw_dev;
	struct mtk_camsv_device *camsv_dev;
	int sv_i;

	/* stop procedure
	 * 1. mark 'stopped' status to skip further processing
	 * 2. stop all working context
	 *   a. disable_irq for threaded_irq
	 *   b. workqueue: cancel_work_sync & (drain/flush)_workqueue
	 *   c. kthread: cancel_work_sync & flush_worker
	 * 3. Now, all contexts are stopped. return resources
	 */
	atomic_set(&cam_ctrl->stopped, MTK_CAM_CTRL_STATE_STOPPING);
	/* disable irq first */
	if (ctx->hw_raw) {
		raw_dev = dev_get_drvdata(ctx->hw_raw);
		disable_irq(raw_dev->irq);
		for (sv_i = 0; sv_i < 2; sv_i++)
			if (ctx->hw_sv[sv_i]) {
				camsv_dev = dev_get_drvdata(ctx->hw_sv[sv_i]);
				disable_irq(camsv_dev->irq);
			}
	}
	/* check if any process is still on working */
	if (atomic_read(&cam_ctrl->ref_cnt)) {
		dev_info(ctx->cam->dev, "[%s] ctx:%d, STATE waiting END, start waiting\n",
			__func__, ctx->stream_id);
		/* wait on-going threaded irq processing finished */
		wait_event_interruptible(cam_ctrl->stop_wq,
			atomic_read(&cam_ctrl->stopped) == MTK_CAM_CTRL_STATE_END);
		dev_info(ctx->cam->dev, "[%s] ctx:%d, STATE waiting END, end waiting\n",
			__func__, ctx->stream_id);
	} else {
		atomic_set(&cam_ctrl->stopped, MTK_CAM_CTRL_STATE_END);
		dev_info(ctx->cam->dev, "[%s] ctx:%d, STATE END\n",
			__func__, ctx->stream_id);
	}
	/* reset hw */
	if (ctx->hw_raw)
		reset(raw_dev);
	for (sv_i = 0; sv_i < 2; sv_i++)
		if (ctx->hw_sv[sv_i]) {
			camsv_dev = dev_get_drvdata(ctx->hw_sv[sv_i]);
			sv_reset(camsv_dev);
		}
	mtk_cam_event_eos(cam_ctrl);
	spin_lock(&cam_ctrl->camsys_state_lock);
	list_for_each_entry_safe(job, job_prev,
				 &cam_ctrl->camsys_state_list, list) {
		job->ops.cancel(job);
		mtk_cam_ctx_job_finish(ctx, job);
		list_del(&job->list);
	}
	spin_unlock(&cam_ctrl->camsys_state_lock);

	drain_workqueue(ctx->frame_done_wq);
	if (ctx->sensor) {
		hrtimer_cancel(&cam_ctrl->sensor_deadline_timer);
	}
	/* using func. kthread_cancel_work_sync, which contains kthread_flush_work func.*/
	kthread_cancel_work_sync(&cam_ctrl->work);
	kthread_flush_worker(&ctx->sensor_worker);

	dev_info(ctx->cam->dev, "[%s] ctx:%d, stop status:%d\n",
		__func__, ctx->stream_id, atomic_read(&cam_ctrl->stopped));
}

