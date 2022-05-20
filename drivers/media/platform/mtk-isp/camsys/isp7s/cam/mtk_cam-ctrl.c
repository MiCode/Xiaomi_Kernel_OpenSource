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

#define SENSOR_SET_DEADLINE_MS  18
#define SENSOR_SET_DEADLINE_MS_60FPS  6
#define SENSOR_DELAY_GUARD_TIME_60FPS 16
#define SENSOR_SET_STAGGER_DEADLINE_MS  23


#define STATE_NUM_AT_SOF 5


#define v4l2_set_frame_interval_which(x, y) (x.reserved[0] = y)


static int _timer_reqdrained_chk(int fps_ratio, int sub_sample)
{
	int timer_ms = 0;

	if (sub_sample > 0) {
		if (fps_ratio > 1)
			timer_ms = SENSOR_SET_DEADLINE_MS;
		else
			timer_ms = SENSOR_SET_DEADLINE_MS * fps_ratio;
	} else {
		if (fps_ratio > 1)
			timer_ms = SENSOR_SET_DEADLINE_MS / fps_ratio;
		else
			timer_ms = SENSOR_SET_DEADLINE_MS;
	}
	/* earlier request drained event*/
	if (sub_sample == 0 && fps_ratio > 1)
		timer_ms = SENSOR_SET_DEADLINE_MS_60FPS;

	return timer_ms;
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
	struct mtk_cam_ctx *ctx = cam_ctrl->ctx;
	int sensor_seq_no_next =
			atomic_read(&cam_ctrl->sensor_request_seq_no) + 1;
	int res = 0;

	if (sensor_seq_no_next <= atomic_read(&cam_ctrl->enqueued_frame_seq_no))
		res = 1;

	/* Send V4L2_EVENT_REQUEST_DRAINED event */
	if (res == 0) {
		atomic_set(&cam_ctrl->last_drained_seq_no, sensor_seq_no_next);
		mtk_cam_event_request_drained(cam_ctrl);
		dev_dbg(ctx->cam->dev, "request_drained:(%d)\n",
			sensor_seq_no_next);
	}

	return (res == 0);
}

static void mtk_cam_try_set_sensor(struct mtk_cam_ctrl *cam_ctrl)
{
	struct mtk_cam_device *cam = cam_ctrl->ctx->cam;
	struct mtk_cam_job *job, *job_sensor = NULL, *job_fs = NULL;
	struct mtk_camsys_irq_info irq_info;
	int from_sof = ktime_get_boottime_ns() / 1000000 -
			   cam_ctrl->sof_time;
	int action = 0;

	/* create update event for job */
	irq_info.ctx_id = cam_ctrl->ctx->stream_id;
	irq_info.irq_type = 1 << CAMSYS_IRQ_TRY_SENSOR_SET;
	irq_info.frame_idx = atomic_read(&cam_ctrl->sensor_request_seq_no) + 1;
	irq_info.frame_idx_inner = atomic_read(&cam_ctrl->sensor_request_seq_no);
	irq_info.ts_ns = cam_ctrl->sof_time * 1000000;
	irq_info.isp_request_seq_no = atomic_read(&cam_ctrl->isp_request_seq_no);
	irq_info.reset_seq_no = atomic_read(&cam_ctrl->reset_seq_no);
	spin_lock(&cam_ctrl->camsys_state_lock);
	/* Check if previous state was without cq done */
	list_for_each_entry(job, &cam_ctrl->camsys_state_list,
			    list) {
		job->ops.update_event(job, &irq_info, &action);
		if (action & (1 << CAM_JOB_APPLY_SENSOR))
			job_sensor = job;
		if (action & (1 << CAM_JOB_APPLY_FS))
			job_fs = job;
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
	/** Frame sync **/
	/* make sure the all ctxs of previous request are triggered [TBC] */
	if (job_fs)
		job_fs->ops.apply_fs(job_fs);
}
static void
mtk_cam_meta1_done_work(struct work_struct *work)
{
	struct mtk_cam_job_work *meta1_done_work =
		container_of(work, struct mtk_cam_job_work, work);
	struct mtk_cam_job *job =
		container_of(meta1_done_work, struct mtk_cam_job, meta1_done_work);
	job->ops.afo_done(job);
}

static int
mtk_cam_submit_meta1_done_work(struct mtk_cam_ctrl *cam_ctrl,
	struct mtk_cam_job *job)
{

	queue_work(cam_ctrl->ctx->frame_done_wq, &job->meta1_done_work.work);

	return 0;
}
static void
mtk_cam_frame_done_work(struct work_struct *work)
{
	struct mtk_cam_job_work *frame_done_work =
		container_of(work, struct mtk_cam_job_work, work);
	struct mtk_cam_job *job =
		container_of(frame_done_work, struct mtk_cam_job, frame_done_work);
	struct mtk_cam_ctrl *cam_ctrl = &job->src_ctx->cam_ctrl;
	struct mtk_cam_job *_job, *_job_prev;

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
}


static int
mtk_cam_submit_frame_done_work(struct mtk_cam_ctrl *cam_ctrl,
	struct mtk_cam_job *job)
{

	queue_work(cam_ctrl->ctx->frame_done_wq, &job->frame_done_work.work);

	return 0;
}

static void
mtk_cam_sensor_worker(struct kthread_work *work)
{
	struct mtk_cam_ctrl *cam_ctrl =
			container_of(work, struct mtk_cam_ctrl, work);
	mtk_cam_try_set_sensor(cam_ctrl);
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

	return kthread_queue_work(worker, &cam_ctrl->work);
}

static enum hrtimer_restart
sensor_deadline_timer_handler(struct hrtimer *t)
{
	struct mtk_cam_ctrl *cam_ctrl =
		container_of(t, struct mtk_cam_ctrl,
			     sensor_deadline_timer);
	struct mtk_cam_ctx *ctx = cam_ctrl->ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int time_after_sof = ktime_get_boottime_ns() / 1000000 -
			   cam_ctrl->sof_time;
	bool drained_res = false;


	/* handle V4L2_EVENT_REQUEST_DRAINED event */
	drained_res = mtk_cam_request_drained(cam_ctrl);

	if (drained_res == 0) {
		mtk_cam_submit_kwork(&ctx->sensor_worker, cam_ctrl);
		dev_dbg(cam->dev, "[TimerIRQ:Not Drained trigger sensor [SOF+%dms]] ctx:%d\n",
			time_after_sof, ctx->stream_id);
	}

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
		after_sof_ms = cam_ctrl->timer_req_event - 1;
	m_kt = ktime_set(0, cam_ctrl->timer_req_event * 1000000
			- after_sof_ms * 1000000);
	hrtimer_start(&cam_ctrl->sensor_deadline_timer, m_kt,
		      HRTIMER_MODE_REL);
}
static void handle_setting_done(struct mtk_cam_ctrl *cam_ctrl,
		unsigned int engine_id, struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_cam_job *job, *job_expswitch = NULL;
	struct mtk_cam_job *job_on = NULL, *job_senswitch = NULL;
	int action = 0;

	spin_lock(&cam_ctrl->camsys_state_lock);
	/* Check if previous state was without cq done */
	list_for_each_entry(job, &cam_ctrl->camsys_state_list,
				list) {
		job->ops.update_event(job, irq_info, &action);
		if (action & (1 << CAM_JOB_STREAM_ON))
			job_on = job;
		if (action & (1 << CAM_JOB_EXP_NUM_SWITCH))
			job_expswitch = job;
		if (action & (1 << CAM_JOB_SENSOR_SWITCH))
			job_senswitch = job;
	}
	spin_unlock(&cam_ctrl->camsys_state_lock);

	/* stream on */
	if (job_on)
		job_on->ops.stream_on(job_on, true);

	/* hdr exposure number switch */
	if (job_expswitch)
		pr_info("not ready");

	/*  sensor mode switch for cq done operation */
	if (job_senswitch)
		pr_info("not ready");

}
static void handle_meta1_done(struct mtk_cam_ctrl *cam_ctrl,
		unsigned int engine_id, struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_cam_job *job, *job_afodeq = NULL;
	int action = 0;

	/* inner register correction */
	irq_info->frame_idx_inner = atomic_read(&cam_ctrl->dequeued_frame_seq_no);
	spin_lock(&cam_ctrl->camsys_state_lock);
	/* Check if previous state was without cq done */
	list_for_each_entry(job, &cam_ctrl->camsys_state_list,
				list) {
		job->ops.update_event(job, irq_info, &action);
		if (action & (1 << CAM_JOB_DEQUE_META1))
			job_afodeq = job;
	}
	spin_unlock(&cam_ctrl->camsys_state_lock);

	/* trigger cq */
	if (job_afodeq)
		mtk_cam_submit_meta1_done_work(cam_ctrl, job_afodeq);
}
static void handle_frame_done(struct mtk_cam_ctrl *cam_ctrl,
		unsigned int engine_id, struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_cam_job *job, *job_deq = NULL;
	int action = 0;

	/* inner register correction */
	irq_info->frame_idx_inner = atomic_read(&cam_ctrl->dequeued_frame_seq_no);
	spin_lock(&cam_ctrl->camsys_state_lock);
	/* Check if previous state was without cq done */
	list_for_each_entry(job, &cam_ctrl->camsys_state_list,
				list) {
		job->ops.update_event(job, irq_info, &action);
		if (action & (1 << CAM_JOB_DEQUE_ALL))
			job_deq = job;
	}
	spin_unlock(&cam_ctrl->camsys_state_lock);

	/* trigger cq */
	if (job_deq)
		mtk_cam_submit_frame_done_work(cam_ctrl, job_deq);

}

static void mtk_cam_raw_frame_start(struct mtk_cam_ctrl *cam_ctrl,
		unsigned int engine_id, struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_cam_job *job, *job_cq = NULL, *job_timer = NULL, *job_swh = NULL;
	struct mtk_cam_job *job_rd_deqno = NULL, *job_vsync = NULL;
	struct mtk_cam_ctx *ctx = cam_ctrl->ctx;
	int from_sof = ktime_get_boottime_ns() / 1000000 -
			   cam_ctrl->sof_time;
	int action = 0;
	int vsync_engine_id = irq_info->engine << 8 | engine_id;

	spin_lock(&cam_ctrl->camsys_state_lock);
	/* Check if previous state was without cq done */
	list_for_each_entry(job, &cam_ctrl->camsys_state_list,
				list) {
		job->ops.update_event(job, irq_info, &action);
		dev_dbg(ctx->cam->dev,
		"[%s] job:%d, state:0x%x, action:0x%x\n", __func__,
			job->frame_seq_no, job->state, action);
		if (action & (1 << CAM_JOB_APPLY_CQ))
			job_cq = job;
		if (action & (1 << CAM_JOB_EXP_NUM_SWITCH))
			job_swh = job;
		if (action & (1 << CAM_JOB_SETUP_TIMER))
			job_timer = job;
		if (action & (1 << CAM_JOB_READ_DEQ_NO))
			job_rd_deqno = job;
		if (action & (1 << CAM_JOB_VSYNC))
			job_vsync = job;
	}
	spin_unlock(&cam_ctrl->camsys_state_lock);
	/*  vsync action update cam_ctrl->sof_time for scheduler coordinates */
	if (job_vsync) {
		if (!ctx) {
			dev_dbg(ctx->cam->dev, "cannot find ctx\n");
			return;
		}
		cam_ctrl->sof_time = irq_info->ts_ns / 1000000;
		mtk_cam_event_frame_sync(cam_ctrl, irq_info->frame_idx_inner);
		cam_ctrl->vsync_engine = vsync_engine_id;
	} else if (vsync_engine_id == cam_ctrl->vsync_engine) {
		cam_ctrl->sof_time = irq_info->ts_ns / 1000000;
		mtk_cam_event_frame_sync(cam_ctrl, irq_info->frame_idx_inner);
	}
	/* setup timer for drained event */
	if (job_timer)
		mtk_cam_sof_timer_setup(cam_ctrl);
	else if ((action & (1 << CAM_JOB_HW_DELAY)) == 0 &&
			vsync_engine_id == cam_ctrl->vsync_engine)
		mtk_cam_sof_timer_setup(cam_ctrl);
	/* update cam_ctrl->dequeued_frame_seq_no for sw done */
	if (job_rd_deqno) {
		atomic_set(&cam_ctrl->dequeued_frame_seq_no,
			irq_info->frame_idx_inner);
		atomic_set(&cam_ctrl->isp_request_seq_no,
			irq_info->frame_idx_inner);
	}
	/* trigger cq */
	if (job_cq) {
		job_cq->ops.apply_isp(job_cq);
		dev_info(ctx->cam->dev, "[%s][out/in/deq:%d/%d/%d] ctx:%d, cq-%d triggered(SOF+%dms)\n",
			__func__, irq_info->frame_idx, irq_info->frame_idx_inner,
			cam_ctrl->dequeued_frame_seq_no, job_cq->src_ctx->stream_id,
			job_cq->frame_seq_no, from_sof);
	}
	/* switch exp */
	if (job_swh)
		job_swh->ops.apply_exp_switch(job_swh);

}

static int mtk_cam_event_handle_raw(struct mtk_cam_device *cam,
				       unsigned int engine_id,
				       struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_raw_device *raw_dev;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_ctrl *cam_ctrl;

	raw_dev = dev_get_drvdata(cam->engines.raw_devs[engine_id]);
	ctx = &cam->ctxs[irq_info->ctx_id];
	if (!ctx) {
		dev_dbg(raw_dev->dev, "cannot find ctx\n");
		return -EINVAL;
	}
	cam_ctrl = &ctx->cam_ctrl;
	irq_info->isp_request_seq_no = atomic_read(&cam_ctrl->isp_request_seq_no);
	irq_info->reset_seq_no = atomic_read(&cam_ctrl->reset_seq_no);

	/* raw's CQ done */
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_SETTING_DONE)) {
		handle_setting_done(cam_ctrl, engine_id, irq_info);
	}
	/* raw's subsample case : try sensor setting */
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_TRY_SENSOR_SET))
		mtk_cam_submit_kwork(&ctx->sensor_worker, cam_ctrl);
	/* raw's DMA done, we only allow AFO done here */
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_AFO_DONE))
		handle_meta1_done(cam_ctrl, engine_id, irq_info);

	/* raw's SW done */
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_DONE))
		handle_frame_done(cam_ctrl, engine_id, irq_info);

	if (atomic_read(&raw_dev->vf_en) == 0) {
		dev_info(raw_dev->dev, "skip sof event when vf off\n");
		return 0;
	}

	/* raw's SOF (proc engine frame start) */
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_START)) {
		mtk_cam_raw_frame_start(cam_ctrl, engine_id, irq_info);
	}

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
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_DONE)) {
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
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_START))
		mtk_camsys_mraw_frame_start(mraw_dev, ctx,
			irq_info->frame_idx_inner, irq_info->ts_ns);
	/* mraw's CQ done */
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_SETTING_DONE)) {
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

static int mtk_camsys_event_handle_camsv(struct mtk_cam_device *cam,
					 unsigned int engine_id,
					 struct mtk_camsys_irq_info *irq_info)
{
#ifdef NOT_READY

	struct mtk_camsv_device *camsv_dev;
	struct mtk_cam_ctx *ctx;
	int sv_dev_index;
	unsigned int stream_id;
	unsigned int seq;
	bool bDcif = false;

	camsv_dev = dev_get_drvdata(cam->sv.devs[engine_id]);
	bDcif = camsv_dev->pipeline->hw_scen &
		(1 << MTKCAM_IPI_HW_PATH_OFFLINE_SRT_DCIF_STAGGER);

	if (camsv_dev->pipeline->hw_scen &
	    MTK_CAMSV_SUPPORTED_SPECIAL_HW_SCENARIO) {
		struct mtk_raw_pipeline *pipeline = &cam->raw
			.pipelines[camsv_dev->pipeline->master_pipe_id];
		struct mtk_raw_device *raw_dev =
			get_master_raw_dev(cam, pipeline);
		struct mtk_cam_ctx *ctx =
			mtk_cam_find_ctx(cam, &pipeline->subdev.entity);

		dev_dbg(camsv_dev->dev, "sv special hw scenario: %d/%d/%d\n",
			camsv_dev->pipeline->master_pipe_id,
			raw_dev->id, ctx->stream_id);
		if (camsv_dev->pipeline->hw_scen &
			(1 << MTKCAM_SV_SPECIAL_SCENARIO_EXT_ISP)) {
			int seninf_padidx = cam->sv.pipelines[camsv_dev->id]
						.seninf_padidx;
			/*raw/yuv pipeline frame start from camsv engine*/
			if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_START)) {
				/*stream on delayed patch*/
				mtk_cam_extisp_sv_stream_delayed(ctx,
					camsv_dev, seninf_padidx);
				/*preisp frame start*/
				if (is_extisp_sv_all_frame_start(camsv_dev, ctx))
					mtk_cam_extisp_sv_frame_start(ctx,
							irq_info->frame_idx_inner);
				/*yuv pipeline processed raw frame start*/
				if (mtk_cam_is_ext_isp_yuv(ctx) &&
					seninf_padidx == PAD_SRC_RAW_EXT0)
					mtk_camsys_extisp_yuv_frame_start(camsv_dev, ctx,
					irq_info);
			}
			/*yuv pipeline frame done from camsv engine*/
			if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_DONE)) {
				if (mtk_cam_is_ext_isp_yuv(ctx) &&
					seninf_padidx == PAD_SRC_RAW_EXT0)
					mtk_camsys_frame_done(ctx, ctx->dequeued_frame_seq_no,
						ctx->stream_id);
			}
			return 0;
		}
		// first exposure camsv's SOF
		if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_START)) {
			if (!bDcif && camsv_dev->pipeline->exp_order == 0)
				mtk_camsys_raw_frame_start(raw_dev, ctx, irq_info);
		}
		if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_DONE)) {
			if (camsv_dev->pipeline->exp_order == 0) {
				ctx->component_dequeued_frame_seq_no =
					irq_info->frame_idx_inner;
			}
		}
		// time sharing - camsv write DRAM mode
		if (camsv_dev->pipeline->hw_scen &
		    (1 << MTKCAM_IPI_HW_PATH_OFFLINE_M2M)) {
			if (irq_info->irq_type & (1<<CAMSYS_IRQ_FRAME_DONE)) {
				mtk_camsys_ts_sv_done(ctx, irq_info->frame_idx_inner);
				mtk_camsys_ts_raw_try_set(raw_dev, ctx,
						ctx->dequeued_frame_seq_no + 1);
			}
			if (irq_info->irq_type & (1<<CAMSYS_IRQ_FRAME_START))
				mtk_camsys_ts_frame_start(ctx, irq_info->frame_idx_inner);
		}
	} else {
		ctx = mtk_cam_find_ctx(cam, &camsv_dev->pipeline->subdev.entity);
		if (!ctx) {
			dev_dbg(camsv_dev->dev, "cannot find ctx\n");
			return -EINVAL;
		}
		stream_id = engine_id + MTKCAM_SUBDEV_CAMSV_START;
		/* camsv's SW done */
		if (irq_info->irq_type & (1<<CAMSYS_IRQ_FRAME_DONE)) {
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
		if (irq_info->irq_type & (1<<CAMSYS_IRQ_FRAME_START))
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
	struct mtk_cam_ctx *ctx = &cam->ctxs[ctx_id];
	unsigned int seq_nearby =
		atomic_read(&ctx->cam_ctrl.enqueued_frame_seq_no);
	int ret = 0;
	unsigned int idx = irq_info->frame_idx;
	unsigned int idx_inner = irq_info->frame_idx_inner;

	irq_info->frame_idx =
		decode_fh_reserved_data_to_seq(seq_nearby, idx);
	irq_info->frame_idx_inner =
		decode_fh_reserved_data_to_seq(seq_nearby, idx_inner);
	irq_info->ctx_id = ctx_id;

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

		ret = mtk_cam_event_handle_raw(cam, engine_id, irq_info);

		break;
	case CAMSYS_ENGINE_MRAW:

		ret = mtk_camsys_event_handle_mraw(cam, engine_id, irq_info);

		break;
	case CAMSYS_ENGINE_CAMSV:

		ret = mtk_camsys_event_handle_camsv(cam, engine_id, irq_info);

		break;
	case CAMSYS_ENGINE_SENINF:
		/* ToDo - cam mux setting delay handling */
		if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_DROP))
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
	struct mtk_cam_ctx *ctx = cam_ctrl->ctx;
	u32 next_frame_seq = atomic_inc_return(&cam_ctrl->enqueued_frame_seq_no);
	/* EnQ this request's state element to state_list (STATE:READY) */
	spin_lock(&cam_ctrl->camsys_state_lock);
	list_add_tail(&job->list,
		      &cam_ctrl->camsys_state_list);
	spin_unlock(&cam_ctrl->camsys_state_lock);
	job->frame_seq_no = next_frame_seq;
	INIT_WORK(&job->frame_done_work.work, mtk_cam_frame_done_work);
	INIT_WORK(&job->meta1_done_work.work, mtk_cam_meta1_done_work);
	job->ops.compose(job);
	dev_info(ctx->cam->dev, "[%s] ctx:%d frame_no:0x%x next_frame_seq:%d\n",
		__func__, ctx->stream_id, job->frame_seq_no, next_frame_seq);
	/* if this job's drained event been sent, then try set sensor at kthread */
	if (next_frame_seq <= 2 ||
		next_frame_seq == atomic_read(&cam_ctrl->last_drained_seq_no))
		mtk_cam_submit_kwork(&ctx->sensor_worker, cam_ctrl);
}
void mtk_cam_ctrl_job_composed(struct mtk_cam_ctrl *cam_ctrl,
	int frame_seq, struct mtkcam_ipi_frame_ack_result *cq_ret)
{
	struct mtk_cam_job *job, *job_composed = NULL;
	struct mtk_cam_device *cam = cam_ctrl->ctx->cam;
	int fh_temp_ctx_id, fh_temp_seq;

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

}

void mtk_cam_ctrl_start(struct mtk_cam_ctrl *cam_ctrl, struct mtk_cam_ctx *ctx)
{
	struct v4l2_subdev_frame_interval fi;
	int fps_factor = 1, sub_ratio = 0;

	cam_ctrl->ctx = ctx;
	fi.pad = 0;

	/* FIXME: this is a hook */
	v4l2_set_frame_interval_which(fi, V4L2_SUBDEV_FORMAT_ACTIVE);
	v4l2_subdev_call(ctx->sensor, video, g_frame_interval, &fi);
	fps_factor = (fi.interval.numerator > 0) ?
		(fi.interval.denominator / fi.interval.numerator / 30) : 1;
	sub_ratio = 0;
	// TBC mtk_cam_get_subsample_ratio(ctx->pipe->res_config.raw_feature);


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
	cam_ctrl->timer_req_event =
		_timer_reqdrained_chk(fps_factor, sub_ratio);
	INIT_LIST_HEAD(&cam_ctrl->camsys_state_list);
	spin_lock_init(&cam_ctrl->camsys_state_lock);
	if (ctx->sensor) {
		hrtimer_init(&cam_ctrl->sensor_deadline_timer,
			     CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		cam_ctrl->sensor_deadline_timer.function =
			sensor_deadline_timer_handler;
	}
	kthread_init_work(&cam_ctrl->work, mtk_cam_sensor_worker);

	dev_info(ctx->cam->dev, "[%s] ctx:%d (fps) drained (%d)%d\n",
		__func__, ctx->stream_id, fps_factor,
		cam_ctrl->timer_req_event);
}
void mtk_cam_ctrl_stop(struct mtk_cam_ctrl *cam_ctrl)
{
	struct mtk_cam_ctx *ctx = cam_ctrl->ctx;
	struct mtk_cam_job *job, *job_prev;

	/* stop procedure
	 * 1. mark 'stopped' status to skip further processing
	 * 2. stop all working context
	 *   a. disable_irq for threaded_irq
	 *   b. workqueue: cancel_work_sync & (drain/flush)_workqueue
	 *   c. kthread: cancel_work_sync & flush_worker
	 * 3. Now, all contexts are stopped. return resources
	 */
	/* release hw - to-do */
	mtk_cam_release_engine(ctx->cam, ctx->used_engine);
	spin_lock(&cam_ctrl->camsys_state_lock);
	list_for_each_entry_safe(job, job_prev,
				 &cam_ctrl->camsys_state_list, list) {
		list_del(&job->list);
	}
	spin_unlock(&cam_ctrl->camsys_state_lock);
	if (ctx->sensor) {
		hrtimer_cancel(&cam_ctrl->sensor_deadline_timer);
	}
	kthread_flush_work(&cam_ctrl->work);

	mtk_cam_event_eos(cam_ctrl);

	dev_info(ctx->cam->dev, "[%s] ctx:%d\n", __func__, ctx->stream_id);
}

