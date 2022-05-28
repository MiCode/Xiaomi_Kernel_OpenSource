// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include "mtk_cam.h"
#include "mtk_cam-job-subsample.h"
#include "mtk_cam-job_utils.h"


int fill_imgo_img_buffer_to_ipi_frame_subsample(
	struct req_buffer_helper *helper, struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	struct mtkcam_ipi_img_output *out;
	int ret = -1;

	out = &fp->img_outs[helper->io_idx];
	++helper->io_idx;
	ret = fill_imgo_out_subsample(out, buf, node, helper->job->sub_ratio);

	return ret;

}
int fill_yuvo_img_buffer_to_ipi_frame_subsample(
	struct req_buffer_helper *helper, struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	struct mtkcam_ipi_img_output *out;
	int ret = -1;

	out = &fp->img_outs[helper->io_idx];
	++helper->io_idx;

	ret = fill_yuvo_out_subsample(out, buf, node, helper->job->sub_ratio);

	return ret;

}

void update_event_setting_done_subsample(struct mtk_cam_job *job,
		struct mtk_cam_job_event_info *event_info, int *action)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_raw_device *raw_dev =
		dev_get_drvdata(cam->engines.raw_devs[job->proc_engine & 0xF]);

	if (job->stream_on_seninf) {
		job->state = E_STATE_SUBSPL_OUTER;
		*action |= BIT(CAM_JOB_STREAM_ON);
	}
	if (job->state >= E_STATE_SUBSPL_SCQ &&
		job->state < E_STATE_SUBSPL_INNER) {
		_state_trans(job, E_STATE_SUBSPL_SCQ, E_STATE_SUBSPL_OUTER);
		_state_trans(job, E_STATE_SUBSPL_SCQ_DELAY, E_STATE_SUBSPL_OUTER);
		/* for main cq done in subsample mode , won't not increase outer counter */
		/* here use isp_enq_no instead */
		*action |= BIT(CAM_JOB_READ_ENQ_NO);
		dev_dbg(raw_dev->dev,
			"[CQD-subsample] req:%d, CQ->OUTER state:0x%x\n",
			job->frame_seq_no, job->state);
	} else
		*action = 0;
}

void update_event_sensor_try_set_subsample(struct mtk_cam_job *job,
		struct mtk_cam_job_event_info *event_info, int *action)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	int cur_sen_seq_no = event_info->isp_enq_seq_no;
	u32 aftersof_ms = (ktime_get_boottime_ns() - event_info->ts_ns) / 1000000;

	if (cur_sen_seq_no == 0) {
		dev_info(ctx->cam->dev,
				 "[%s] initial setup sensor job:%d cur/next:%d/%d\n",
			__func__, job->frame_seq_no, event_info->frame_idx_inner,
			event_info->frame_idx);
		if (job->frame_seq_no == 1) {
			*action |= BIT(CAM_JOB_APPLY_SENSOR);
			return;
		}
	}
	if (job->frame_seq_no < cur_sen_seq_no)
		*action = 0;

	if (job->frame_seq_no == cur_sen_seq_no) {
		if (*action & BIT(CAM_JOB_HW_DELAY) ||
			*action & BIT(CAM_JOB_CQ_DELAY) ||
			*action & BIT(CAM_JOB_SENSOR_DELAY))
			return;
		if (job->state == E_STATE_SUBSPL_OUTER)
			*action |= BIT(CAM_JOB_APPLY_SENSOR);
	}
	if (job->frame_seq_no > cur_sen_seq_no)
		*action = 0;
	dev_info(ctx->cam->dev, "[%s] sensor job:%d, cur:%d, state:0x%x, action:%d [sof+%dms]\n",
		__func__, job->frame_seq_no, cur_sen_seq_no, job->state, *action, aftersof_ms);
}
static void
_update_event_sensor_vsync_subsample(struct mtk_cam_job *job,
	struct mtk_cam_job_event_info *event_info, int *action)
{
	unsigned int frame_seq_no_inner = event_info->frame_idx_inner;
#ifdef NOT_READY
	/* touch watchdog*/
	if (watchdog_scenario(ctx))
		mtk_ctx_watchdog_kick(ctx);
#endif
	if (frame_seq_no_inner == job->frame_seq_no) {
		*action |= BIT(CAM_JOB_VSYNC);
		if ((*action & BIT(CAM_JOB_HW_DELAY)) == 0)
			*action |= BIT(CAM_JOB_SETUP_TIMER);
	} else {
		*action &= ~BIT(CAM_JOB_VSYNC);
	}
}
static void
_update_event_frame_start_subsample(struct mtk_cam_job *job,
		struct mtk_cam_job_event_info *event_info, int *action)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	int frame_idx_inner = event_info->frame_idx_inner;
	int write_cnt_offset, write_cnt;
	u64 time_boot = event_info->ts_ns;
	u64 time_mono = ktime_get_ns();

	if (job->state == E_STATE_SUBSPL_INNER) {
		write_cnt_offset = event_info->reset_seq_no - 1;
		write_cnt = ((event_info->isp_request_seq_no - write_cnt_offset) / 256)
					* 256 + event_info->write_cnt;
		/* job - should be dequeued or re-reading out */
		if (frame_idx_inner > event_info->isp_request_seq_no ||
			atomic_read(&job->frame_done_work.is_queued) == 1) {
			dev_info_ratelimited(ctx->cam->dev,
				"[SOF] frame done work delay, req(%d),ts(%lu)\n",
				job->frame_seq_no, event_info->ts_ns / 1000);
		} else if (write_cnt >= job->frame_seq_no - write_cnt_offset) {
			dev_info_ratelimited(ctx->cam->dev,
				"[SOF] frame done sw reading lost %d frames, req(%d),ts(%lu)\n",
				write_cnt - (job->frame_seq_no - write_cnt_offset) + 1,
				job->frame_seq_no, event_info->ts_ns / 1000);
			_set_timestamp(job, time_boot - 1000, time_mono - 1000);
		} else if ((write_cnt >= job->frame_seq_no - write_cnt_offset - 1)
			&& event_info->fbc_cnt == 0) {
			dev_info_ratelimited(ctx->cam->dev,
				"[SOF] frame done sw reading lost frames, req(%d),ts(%lu)\n",
				job->frame_seq_no, event_info->ts_ns / 1000);
			_set_timestamp(job, time_boot - 1000, time_mono - 1000);
		} else {
			_state_trans(job, E_STATE_INNER, E_STATE_INNER_HW_DELAY);
			dev_info_ratelimited(ctx->cam->dev,
				"[SOF] HW_IMCOMPLETE state cnt(%d,%d),req(%d),ts(%lu)\n",
				write_cnt, event_info->write_cnt, job->frame_seq_no,
				event_info->ts_ns / 1000);
			*action |= BIT(CAM_JOB_HW_DELAY);
		}
	} else if (job->state == E_STATE_SUBSPL_SENSOR ||
		job->state == E_STATE_SUBSPL_OUTER) {
		/* job - reading out */
		_set_timestamp(job, time_boot, time_mono);
		if (*action & BIT(CAM_JOB_HW_DELAY))
			return;
		if (job->frame_seq_no > frame_idx_inner) {
			dev_info(ctx->cam->dev,
				"[SOF-noDBLOAD] outer_no:%d, inner_idx:%d <= processing_idx:%d,ts:%lu\n",
				job->frame_seq_no, frame_idx_inner, event_info->isp_request_seq_no,
				event_info->ts_ns / 1000);
			*action |= BIT(CAM_JOB_CQ_DELAY);
			return;
		}

		if (job->frame_seq_no == frame_idx_inner) {
			if (frame_idx_inner > event_info->isp_request_seq_no) {
				_state_trans(job, E_STATE_SUBSPL_OUTER,
						 E_STATE_SUBSPL_INNER);
				_state_trans(job, E_STATE_SUBSPL_SENSOR,
						 E_STATE_SUBSPL_INNER);
				*action |= BIT(CAM_JOB_READ_DEQ_NO);
				dev_dbg(ctx->cam->dev,
					"[SOF-DBLOAD][%s] frame_seq_no:%d, OUTER->INNER state:%d,ts:%lu\n",
					__func__, job->frame_seq_no, job->state,
					event_info->ts_ns / 1000);
			}
		}
		if (job->frame_seq_no == 1)
			_state_trans(job, E_STATE_SUBSPL_READY, E_STATE_SUBSPL_SCQ);

	} else if (job->state == E_STATE_SUBSPL_READY) {
		if (*action & BIT(CAM_JOB_HW_DELAY) ||
			*action & BIT(CAM_JOB_CQ_DELAY))
			return;
		if (job->frame_seq_no == event_info->isp_enq_seq_no + 1)
			*action |= BIT(CAM_JOB_APPLY_CQ);
		else
			*action = 0;
	}
}

void update_frame_start_event_subsample(struct mtk_cam_job *job,
		struct mtk_cam_job_event_info *event_info, int *action)
{
	struct mtk_cam_device *cam = job->src_ctx->cam;
	int engine_type = (event_info->engine >> 8) & 0xFF;

	_update_event_frame_start_subsample(job, event_info, action);
	_update_event_sensor_vsync_subsample(job, event_info, action);

	dev_dbg(cam->dev,
		"[%s] engine_type:%d, job:%d, out/in/isp_enq:%d/%d/%d, ts:%lld, action:0x%x\n",
		__func__, engine_type, job->frame_seq_no, event_info->frame_idx,
		event_info->frame_idx_inner, event_info->isp_enq_seq_no,
		event_info->ts_ns, *action);
}


