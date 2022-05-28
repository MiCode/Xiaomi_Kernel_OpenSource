// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include "mtk_cam.h"
#include "mtk_cam-job-stagger.h"
#include "mtk_cam-job_utils.h"

static bool
is_stagger_2_exposure(int feature)
{
	u32 raw_feature = 0;

	raw_feature = feature & 0x0000000F;
	if (raw_feature == STAGGER_2_EXPOSURE_LE_SE ||
		raw_feature == STAGGER_2_EXPOSURE_SE_LE)
		return true;

	return false;
}

static bool
is_stagger_3_exposure(int feature)
{
	u32 raw_feature = 0;

	raw_feature = feature & 0x0000000F;
	if (raw_feature == STAGGER_3_EXPOSURE_LE_NE_SE ||
	    raw_feature == STAGGER_3_EXPOSURE_SE_NE_LE)
		return true;

	return false;
}
bool
is_stagger_need_rawi(int feature)
{
	int is_hdr;

	if (feature & MTK_CAM_FEATURE_OFFLINE_M2M_MASK)
		return false;

	is_hdr = feature & MTK_CAM_FEATURE_HDR_MASK;
	if (is_hdr && is_hdr >= STAGGER_2_EXPOSURE_LE_SE &&
	    is_hdr <= STAGGER_3_EXPOSURE_SE_NE_LE)
		return true;

	return false;
}

static int mtk_cam_dc_last_camsv(int raw)
{
	if (raw == 2)
		return 2;

	return 1;
}

static int select_camsv_engine_stagger(struct mtk_cam_device *cam,
		int hw_scen, int req_amount, int master, int is_trial)
{
	unsigned int i, j, k, group, exp_order;
	unsigned int idle_pipes = 0, match_cnt = 0;
	bool is_dc =
		hw_scen == (1 << MTKCAM_IPI_HW_PATH_OFFLINE_SRT_DCIF_STAGGER);

	if (hw_scen == (1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER) ||
		hw_scen == (1 << MTKCAM_IPI_HW_PATH_OFFLINE_SRT_DCIF_STAGGER)) {
		for (i = 0; i < req_amount; i++) {
			group = master << CAMSV_GROUP_SHIFT;
			exp_order = 1 << (i + CAMSV_EXP_ORDER_SHIFT);

			if (is_dc && (req_amount - 1 == i)) {
				switch (master) {
				case 1 << MTKCAM_SUBDEV_RAW_0:
					exp_order = 1 << (mtk_cam_dc_last_camsv(MTKCAM_SUBDEV_RAW_0)
									  + CAMSV_EXP_ORDER_SHIFT);
					break;
				case 1 << MTKCAM_SUBDEV_RAW_1:
					exp_order = 1 << (mtk_cam_dc_last_camsv(MTKCAM_SUBDEV_RAW_1)
									  + CAMSV_EXP_ORDER_SHIFT);
					break;
				case 1 << MTKCAM_SUBDEV_RAW_2:
					exp_order = 1 << (mtk_cam_dc_last_camsv(MTKCAM_SUBDEV_RAW_2)
									  + CAMSV_EXP_ORDER_SHIFT);
					break;
				default:
					break;
				}
			}

			for (j = 0; j < cam->engines.num_camsv_devices; j++) {
				if ((cam->pipelines.camsv[j].is_occupied == 0) &&
					(cam->pipelines.camsv[j].hw_cap & hw_scen) &&
					(cam->pipelines.camsv[j].hw_cap & group) &&
					(cam->pipelines.camsv[j].hw_cap & exp_order)) {
					match_cnt++;
					idle_pipes |= (1 << (j + _RANGE_POS_camsv));
					break;
				}
			}
			if (j == cam->engines.num_camsv_devices) {
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
				for (k = 0; k < cam->engines.num_camsv_devices; k++) {
					if ((cam->pipelines.camsv[k].is_occupied == 0) &&
						(cam->pipelines.camsv[k].hw_cap & hw_scen) &&
						(cam->pipelines.camsv[k].hw_cap & group) &&
						(cam->pipelines.camsv[k].hw_cap & exp_order)) {
						match_cnt++;
						idle_pipes |= (1 << (k + _RANGE_POS_camsv));
						break;
					}
				}
				if (k == cam->engines.num_camsv_devices) {
					idle_pipes = 0;
					match_cnt = 0;
					goto EXIT;
				}
			}
			if (match_cnt == req_amount)
				break;
		}
	}

EXIT:
	dev_info(cam->dev, "%s idle_pipes 0x%08x", __func__, idle_pipes);
	return idle_pipes;
}
static bool
is_stagger_dc(struct mtk_cam_job *job)
{
	struct mtk_cam_stagger_job *stagger_job =
		(struct mtk_cam_stagger_job *)job;
	bool ret = false;

	if (stagger_job->is_dc_stagger)
		ret = true;

	return ret;
}

static int
get_hdr_scen_id(struct mtk_cam_job *job)
{
	int hw_scen =
		(1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER);

	if (is_stagger_dc(job))
		hw_scen = (1 << MTKCAM_IPI_HW_PATH_OFFLINE_SRT_DCIF_STAGGER);
	/* TBC mstream / offline stagger*/

	pr_info("%s hw_scen:%d", __func__, hw_scen);
	return hw_scen;
}
int get_hard_scenario_stagger(struct mtk_cam_job *job)
{
	int feature = job->feature_job;
	int isDC = is_stagger_dc(job);
	int hard_scenario;

	if (is_stagger_2_exposure(feature))
		hard_scenario = isDC ?
				MTKCAM_IPI_HW_PATH_OFFLINE_SRT_DCIF_STAGGER :
				MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER;
	else if (is_stagger_3_exposure(feature))
		hard_scenario = isDC ?
				MTKCAM_IPI_HW_PATH_OFFLINE_SRT_DCIF_STAGGER :
				MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER;
	else
		hard_scenario = isDC ?
				MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER :
				MTKCAM_IPI_HW_PATH_ON_THE_FLY;

	return hard_scenario;
}

int get_exp_order(struct mtk_cam_job *job, int sv_index)
{
	struct mtk_camsv_pipeline *sv_pipe =
		&job->src_ctx->cam->pipelines.camsv[sv_index];
	int feature = job->feature_job;
	int isDC = is_stagger_dc(job);
	int exp_no;
	int exp_order = -1;

	if (is_stagger_2_exposure(feature))
		exp_no = 2;
	else if (is_stagger_3_exposure(feature))
		exp_no = 3;
	else
		exp_no = 1;
	/* exp_order judgment */
	if (sv_pipe->hw_cap & BIT(CAMSV_EXP_ORDER_SHIFT))
		exp_order = (isDC && (exp_no == 1)) ? 2 : 0;
	else if (sv_pipe->hw_cap & BIT(CAMSV_EXP_ORDER_SHIFT + 1))
		exp_order = (isDC && (exp_no != 3)) ? 2 : 1;
	else
		exp_order = 2;

	return exp_order;
}
int fill_imgo_img_buffer_to_ipi_frame_stagger(
	struct req_buffer_helper *helper, struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	struct mtkcam_ipi_img_output *out;
	struct mtkcam_ipi_img_input *in;
	int isneedrawi = is_stagger_need_rawi(
			helper->job->feature_job);
	int ret = -1;

	out = &fp->img_outs[helper->io_idx];
	++helper->io_idx;
	if (isneedrawi) {
		ret = fill_img_out_hdr(out, buf, node);
		in = &fp->img_ins[helper->ii_idx];
		++helper->ii_idx;
		ret = fill_img_in_hdr(in, buf, node);

		helper->filled_hdr_buffer = true;
	} else {
		ret = fill_img_out(out, buf, node);
	}

	return ret;

}

int mtk_cam_select_hw_stagger(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct device *raw = NULL;
	int available, raw_available;
	int selected;
	int i, sv_cnt = 0;
	int sv_selected, sv_submask;
	int master, hw_scen, req_amount;
	int feature = job->feature_job;

	selected = 0;
	available = mtk_cam_get_available_engine(cam);
	raw_available = USED_MASK_GET_SUBMASK(&available, raw);

	/* todo: more rules */
	for (i = 0; i < cam->engines.num_raw_devices; i++)
		if (SUBMASK_HAS(&raw_available, raw, i)) {
			USED_MASK_SET(&selected, raw, i);
			raw = cam->engines.raw_devs[i];
			break;
		}

	if (!selected) {
		dev_info(cam->dev, "select hw failed\n");
		return -1;
	}

	ctx->hw_raw = raw;
	ctx->hw_sv[0] = NULL;
	ctx->hw_sv[1] = NULL;
	/* otf stagger case */
	req_amount = (is_stagger_3_exposure(feature)) ? 2 : 1;
	master = (1 << MTKCAM_SUBDEV_RAW_0);
	hw_scen = (1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER);
	sv_selected = select_camsv_engine_stagger(cam, hw_scen, req_amount, master, 0);
	sv_submask = USED_MASK_GET_SUBMASK(&sv_selected, camsv);
	dev_info(cam->dev, "[%s:stagger] sel/sv_sub/sv_sel:0x%x/0x%x/0x%x\n",
		__func__, selected, sv_submask, sv_selected);
	for (i = 0; i < cam->engines.num_camsv_devices; i++)
		if (SUBMASK_HAS(&sv_submask, camsv, i)) {
			ctx->camsv_subdev_idx[sv_cnt] = i;
			ctx->hw_sv[i] = cam->engines.sv_devs[i];
			sv_cnt++;
		}
	selected |= sv_selected;

	return selected;
}
int get_feature_switch_stagger(int cur, int prev)
{
	int res = EXPOSURE_CHANGE_NONE;

	if (cur == prev)
		return EXPOSURE_CHANGE_NONE;
	if (cur & MTK_CAM_FEATURE_HDR_MASK || prev & MTK_CAM_FEATURE_HDR_MASK) {
		cur &= MTK_CAM_FEATURE_HDR_MASK;
		prev &= MTK_CAM_FEATURE_HDR_MASK;
		if ((cur == STAGGER_2_EXPOSURE_LE_SE || cur == STAGGER_2_EXPOSURE_LE_SE) &&
		    (prev == STAGGER_3_EXPOSURE_LE_NE_SE ||
		     prev == STAGGER_3_EXPOSURE_SE_NE_LE))
			res = EXPOSURE_CHANGE_3_to_2;
		else if ((prev == STAGGER_2_EXPOSURE_LE_SE ||
			  prev == STAGGER_2_EXPOSURE_SE_LE) &&
			 (cur == STAGGER_3_EXPOSURE_LE_NE_SE ||
			  cur == STAGGER_3_EXPOSURE_SE_NE_LE))
			res = EXPOSURE_CHANGE_2_to_3;
		else if (prev == 0 &&
			 (cur == STAGGER_3_EXPOSURE_LE_NE_SE ||
			  cur == STAGGER_3_EXPOSURE_SE_NE_LE))
			res = EXPOSURE_CHANGE_1_to_3;
		else if (cur == 0 &&
			 (prev == STAGGER_3_EXPOSURE_LE_NE_SE ||
			  prev == STAGGER_3_EXPOSURE_SE_NE_LE))
			res = EXPOSURE_CHANGE_3_to_1;
		else if (prev == 0 &&
			 (cur == STAGGER_2_EXPOSURE_LE_SE ||
			  cur == STAGGER_2_EXPOSURE_SE_LE))
			res = EXPOSURE_CHANGE_1_to_2;
		else if (cur == 0 &&
			 (prev == STAGGER_2_EXPOSURE_LE_SE ||
			  prev == STAGGER_2_EXPOSURE_SE_LE))
			res = EXPOSURE_CHANGE_2_to_1;
	}
	pr_info("[%s] res:%d cur:0x%x prev:0x%x\n",
			__func__, res, cur, prev);

	return res;
}
void update_stagger_job_exp(struct mtk_cam_job *job, int switch_type)
{
	int feature = job->feature_job;

	if (is_stagger_2_exposure(feature))
		job->exp_num_cur = 2;
	else if (is_stagger_3_exposure(feature))
		job->exp_num_cur = 3;
	else
		job->exp_num_cur = 1;

	switch (switch_type) {
	case EXPOSURE_CHANGE_NONE:
		job->exp_num_prev = job->exp_num_cur;
		break;
	case EXPOSURE_CHANGE_3_to_2:
	case EXPOSURE_CHANGE_3_to_1:
		job->exp_num_prev = 3;
		break;
	case EXPOSURE_CHANGE_2_to_1:
	case EXPOSURE_CHANGE_2_to_3:
		job->exp_num_prev = 2;
		break;
	case EXPOSURE_CHANGE_1_to_2:
	case EXPOSURE_CHANGE_1_to_3:
		job->exp_num_prev = 1;
		break;
	default:
		break;
	}
	//pr_info("[%s] prev:%d-exp -> cur:%d-exp\n",
	//	__func__, job->feature->exp_num_prev, job->feature->exp_num_cur);
}


void update_event_setting_done_stagger(struct mtk_cam_job *job,
		struct mtk_cam_job_event_info *event_info, int *action)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_raw_device *raw_dev =
		dev_get_drvdata(cam->engines.raw_devs[job->proc_engine & 0xF]);
	struct mtk_cam_stagger_job *stagger_job =
			(struct mtk_cam_stagger_job *)job;
	unsigned int frame_seq_no_outer = event_info->frame_idx;
	int type;

	if ((job->frame_seq_no == frame_seq_no_outer) &&
		((frame_seq_no_outer - event_info->isp_request_seq_no) > 0)) {
		/**
		 * outer number is 1 more from last SOF's
		 * inner number
		 */
		if (frame_seq_no_outer == 1) {
			job->state = E_STATE_OUTER;
			*action |= BIT(CAM_JOB_STREAM_ON);
		}
		_state_trans(job, E_STATE_CQ, E_STATE_OUTER);
		_state_trans(job, E_STATE_CQ_SCQ_DELAY, E_STATE_OUTER);
		_state_trans(job, E_STATE_SENINF, E_STATE_OUTER);
		type = stagger_job->switch_feature_type;
		if (type) {
			if (type == EXPOSURE_CHANGE_3_to_1 ||
				type == EXPOSURE_CHANGE_2_to_1) {
				stagger_disable(raw_dev);
				stagger_job->dcif_enable = 0;
			} else if (type == EXPOSURE_CHANGE_1_to_2 ||
				type == EXPOSURE_CHANGE_1_to_3) {
				stagger_enable(raw_dev);
				stagger_job->dcif_enable = 1;
			}
			dbload_force(raw_dev);
			dev_dbg(raw_dev->dev,
				"[CQD-switch] req:%d type:%d\n",
				job->frame_seq_no, type);
		}
		dev_info(raw_dev->dev,
			"[%s] req:%d, CQ->OUTER state:%d\n", __func__,
			job->frame_seq_no, job->state);
		// TBC - mtk_cam_handle_seamless_switch(job);
		// TBC - mtk_cam_handle_mux_switch(raw_dev, ctx, job->req);
	}
}

void update_event_sensor_try_set_stagger(struct mtk_cam_job *job,
		struct mtk_cam_job_event_info *event_info, int *action)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_stagger_job *stagger_job =
			(struct mtk_cam_stagger_job *)job;
	int cur_sen_seq_no = event_info->frame_idx_inner;
	u64 aftersof_ms = (ktime_get_boottime_ns() - event_info->ts_ns) / 1000000;

	if (job->frame_seq_no <= 2) {
		dev_info(ctx->cam->dev,
				 "[%s] initial setup sensor job:%d cur/next:%d/%d\n",
			__func__, job->frame_seq_no, event_info->frame_idx_inner,
			event_info->frame_idx);
		if (job->frame_seq_no == cur_sen_seq_no + 1) {
			*action |= BIT(CAM_JOB_APPLY_SENSOR);
			return;
		}
	}

	if (job->frame_seq_no == cur_sen_seq_no - 1) {
		if (job->state < E_STATE_INNER) {
			dev_info(ctx->cam->dev,
				 "[%s] req:%d isn't arrive inner (sen_seq_no:%d)\n",
				 __func__, job->frame_seq_no, cur_sen_seq_no);
			*action = BIT(CAM_JOB_HW_DELAY);
			return;
		}
	}
	if (job->frame_seq_no == cur_sen_seq_no) {
		if (job->state == E_STATE_CAMMUX_OUTER_CFG) {
			job->state = E_STATE_CAMMUX_OUTER_CFG_DELAY;
			dev_info(ctx->cam->dev,
				"[%s] CAMMUX OUTTER CFG DELAY STATE\n", __func__);
			*action = BIT(CAM_JOB_SENSOR_DELAY);
			return;
		} else if (job->state <= E_STATE_SENSOR) {
			dev_info(ctx->cam->dev,
				 "[%s] wrong state:%d (sensor delay)\n",
				 __func__, job->state);
			*action = BIT(CAM_JOB_SENSOR_DELAY);
			return;
		}
	}
	if (job->frame_seq_no == cur_sen_seq_no + 1) {
		if (aftersof_ms > job->sensor_set_margin) {
			dev_info(ctx->cam->dev,
				 "[%s] req:%d over setting margin (%d>%d)\n",
				 __func__, job->frame_seq_no, aftersof_ms,
				 job->sensor_set_margin);
			*action = 0;
			return;
		}
		if (*action & BIT(CAM_JOB_HW_DELAY) ||
			*action & BIT(CAM_JOB_CQ_DELAY) ||
			*action & BIT(CAM_JOB_SENSOR_DELAY))
			return;
		if (stagger_job->switch_feature_type && job->frame_seq_no > 1) {
			dev_info(ctx->cam->dev,
				 "[%s] switch type:%d request:%d - pass sensor\n",
				 __func__, stagger_job->switch_feature_type,
				 job->frame_seq_no);
			*action |= BIT(CAM_JOB_SENSOR_EXPNUM_CHANGE);
			return;
		}

		*action |= BIT(CAM_JOB_APPLY_SENSOR);
	}
	if (job->frame_seq_no > cur_sen_seq_no + 1)
		*action = 0;
}

static void
_update_event_frame_start_stagger(struct mtk_cam_job *job,
		struct mtk_cam_job_event_info *event_info, int *action)
{
	struct mtk_cam_stagger_job *stagger_job =
			(struct mtk_cam_stagger_job *)job;
	struct mtk_cam_ctx *ctx = job->src_ctx;
	int frame_idx_inner = event_info->frame_idx_inner;
	int write_cnt_offset, write_cnt;
	u64 time_boot = event_info->ts_ns;
	u64 time_mono = ktime_get_ns();
	int switch_type = stagger_job->switch_feature_type;

	if (job->state == E_STATE_INNER ||
		job->state == E_STATE_INNER_HW_DELAY) {
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
	} else if (job->state == E_STATE_CQ ||
		job->state == E_STATE_OUTER ||
		job->state == E_STATE_CAMMUX_OUTER ||
		job->state == E_STATE_OUTER_HW_DELAY) {
		/* job - reading out */
		_set_timestamp(job, time_boot, time_mono);
		if (*action & BIT(CAM_JOB_HW_DELAY)) {
			_state_trans(job, E_STATE_OUTER,
			 E_STATE_OUTER_HW_DELAY);
			_state_trans(job, E_STATE_CAMMUX_OUTER,
			 E_STATE_OUTER_HW_DELAY);
			return;
		}
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
				_state_trans(job, E_STATE_OUTER_HW_DELAY,
						 E_STATE_INNER_HW_DELAY);
				_state_trans(job, E_STATE_OUTER, E_STATE_INNER);
				_state_trans(job, E_STATE_CAMMUX_OUTER,
						 E_STATE_INNER);
				*action |= BIT(CAM_JOB_READ_DEQ_NO);
				dev_dbg(ctx->cam->dev,
					"[SOF-DBLOAD][%s] frame_seq_no:%d, OUTER->INNER state:%d,ts:%lu\n",
					__func__, job->frame_seq_no, job->state,
					event_info->ts_ns / 1000);
			}
		}
		if (job->frame_seq_no == 1)
			_state_trans(job, E_STATE_SENSOR, E_STATE_INNER);

	} else if (job->state == E_STATE_SENSOR ||
		job->state == E_STATE_SENINF) {
		if (*action & BIT(CAM_JOB_HW_DELAY) ||
			*action & BIT(CAM_JOB_CQ_DELAY))
			return;
		/* job - to be set */
		if (job->state == E_STATE_SENINF) {
			dev_info(ctx->cam->dev, "[SOF] sensor switch delay\n");
			*action |= BIT(CAM_JOB_SENSOR_DELAY);
		} else if (job->state == E_STATE_SENSOR) {
			*action |= BIT(CAM_JOB_APPLY_CQ);
		}

	} else if (job->state == E_STATE_READY) {
		if (*action & BIT(CAM_JOB_HW_DELAY) ||
			*action & BIT(CAM_JOB_CQ_DELAY))
			return;
		if (switch_type && job->frame_seq_no > 1 &&
			job->frame_seq_no == frame_idx_inner + 1) {
			*action |= BIT(CAM_JOB_EXP_NUM_SWITCH);
			*action |= BIT(CAM_JOB_APPLY_CQ);
			_state_trans(job, E_STATE_READY, E_STATE_SENSOR);
		} else {
			dev_info(ctx->cam->dev,
			"[%s] need check, req:%d, state:%d\n", __func__,
			job->frame_seq_no, job->state);
			*action = 0;
		}
	}

}
static void
_update_event_sensor_vsync_stagger(struct mtk_cam_job *job,
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

void update_frame_start_event_stagger(struct mtk_cam_job *job,
		struct mtk_cam_job_event_info *event_info, int *action)
{
	struct mtk_cam_stagger_job *stagger_job =
			(struct mtk_cam_stagger_job *)job;
	struct mtk_cam_device *cam = job->src_ctx->cam;
	int engine_type = (event_info->engine >> 8) & 0xFF;

	if (stagger_job->dcif_enable) {
		if (engine_type == CAMSYS_ENGINE_CAMSV)
			_update_event_sensor_vsync_stagger(job, event_info, action);
		else if (engine_type == CAMSYS_ENGINE_RAW)
			_update_event_frame_start_stagger(job, event_info, action);
	} else {
		_update_event_frame_start_stagger(job, event_info, action);
		_update_event_sensor_vsync_stagger(job, event_info, action);
	}

	dev_dbg(cam->dev,
		"[%s] engine_type:%d, job:%d, out/in:%d/%d, ts:%lld, dc_en:%d, action:0x%x\n",
		__func__, engine_type, job->frame_seq_no, event_info->frame_idx,
		event_info->frame_idx_inner, event_info->ts_ns,
		stagger_job->dcif_enable, *action);
}
int wait_apply_sensor_stagger(struct mtk_cam_job *job)
{
	struct mtk_cam_stagger_job *stagger_job =
			(struct mtk_cam_stagger_job *)job;

	atomic_set(&stagger_job->expnum_change, 0);
	wait_event_interruptible(stagger_job->expnum_change_wq,
		atomic_read(&stagger_job->expnum_change) > 0);
	job->ops.apply_sensor(job);
	atomic_dec_return(&stagger_job->expnum_change);

	return 0;
}

int apply_cam_mux_stagger(struct mtk_cam_job *job)
{
	struct mtk_cam_stagger_job *stagger_job =
			(struct mtk_cam_stagger_job *)job;
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_seninf_mux_param param;
	struct mtk_cam_seninf_mux_setting settings[3];
	int type = stagger_job->switch_feature_type;
	int sv_main_id, sv_sub_id, sv_last_id;
	int sv_main_tg, sv_last_tg, raw_tg;
	int config_exposure_num;
	int feature = job->feature_config;
	int is_dc = is_stagger_dc(job);
	int raw_id = _get_master_raw_id(ctx->cam->engines.num_raw_devices,
			job->used_engine);
	/**
	 * To identify the "max" exposure_num, we use
	 * feature_active, not job->feature.raw_feature
	 * since the latter one stores the exposure_num information,
	 * not the max one.
	 */
	if (is_stagger_3_exposure(feature))
		config_exposure_num = 3;
	else if (is_stagger_2_exposure(feature))
		config_exposure_num = 2;
	else
		config_exposure_num = 1;

	if (type != EXPOSURE_CHANGE_NONE && config_exposure_num == 3) {
		sv_main_id = ctx->camsv_subdev_idx[0];
		sv_sub_id = ctx->camsv_subdev_idx[1];
		switch (type) {
		case EXPOSURE_CHANGE_3_to_2:
		case EXPOSURE_CHANGE_1_to_2:
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  = ctx->cam
				->pipelines.camsv[sv_main_id].cammux_id;
			settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  = raw_id;
			settings[1].enable = 1;

			settings[2].seninf = ctx->seninf;
			settings[2].source = PAD_SRC_RAW2;
			settings[2].camtg  = ctx->cam
				->pipelines.camsv[sv_sub_id].cammux_id;
			settings[2].enable = 0;
			break;
		case EXPOSURE_CHANGE_3_to_1:
		case EXPOSURE_CHANGE_2_to_1:
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  = raw_id;
			settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  = ctx->cam
				->pipelines.camsv[sv_sub_id].cammux_id;
			settings[1].enable = 0;

			settings[2].seninf = ctx->seninf;
			settings[2].source = PAD_SRC_RAW2;
			settings[2].camtg  = ctx->cam
				->pipelines.camsv[sv_main_id].cammux_id;
			settings[2].enable = 0;
			break;
		case EXPOSURE_CHANGE_2_to_3:
		case EXPOSURE_CHANGE_1_to_3:
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  = ctx->cam
				->pipelines.camsv[sv_main_id].cammux_id;
			settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  = ctx->cam
				->pipelines.camsv[sv_sub_id].cammux_id;
			settings[1].enable = 1;

			settings[2].seninf = ctx->seninf;
			settings[2].source = PAD_SRC_RAW2;
			settings[2].camtg  = raw_id;
			settings[2].enable = 1;
			break;
		default:
			break;
		}
		param.settings = &settings[0];
		param.num = 3;
		mtk_cam_seninf_streaming_mux_change(&param);
		dev_info(ctx->cam->dev,
			"[%s] switch Req:%d, type:%d, cam_mux[0][1][2]:[%d/%d/%d][%d/%d/%d][%d/%d/%d]\n",
			__func__, job->frame_seq_no, type,
			settings[0].source, settings[0].camtg, settings[0].enable,
			settings[1].source, settings[1].camtg, settings[1].enable,
			settings[2].source, settings[2].camtg, settings[2].enable);
	} else if (type != EXPOSURE_CHANGE_NONE && config_exposure_num == 2) {
		sv_main_id = ctx->camsv_subdev_idx[0];
		sv_last_id = ctx->camsv_subdev_idx[1];
		raw_tg = raw_id;
		sv_main_tg = ctx->cam->pipelines.camsv[sv_main_id].cammux_id;

		if (is_dc) {
			if (sv_last_id == -1) {
				dev_info(ctx->cam->dev, "dc mode without exp_order 2 sv");
				sv_last_tg = raw_tg;
			} else {
				sv_last_tg =
					ctx->cam->pipelines.camsv[sv_last_id].cammux_id;
			}
		}

		switch (type) {
		case EXPOSURE_CHANGE_2_to_1:
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  = (is_dc) ? sv_last_tg : raw_tg;
			settings[0].enable = 1;
			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  = sv_main_tg;
			settings[1].enable = 0;
			break;
		case EXPOSURE_CHANGE_1_to_2:
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  = sv_main_tg;
			settings[0].enable = 1;
			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  = (is_dc) ? sv_last_tg : raw_tg;
			settings[1].enable = 1;
			break;
		default:
			break;
		}
		param.settings = &settings[0];
		param.num = 2;
		mtk_cam_seninf_streaming_mux_change(&param);
		dev_info(ctx->cam->dev,
			"[%s] switch Req:%d, type:%d, cam_mux[0][1]:[%d/%d/%d][%d/%d/%d] ts:%lu\n",
			__func__, job->frame_seq_no, type,
			settings[0].source, settings[0].camtg, settings[0].enable,
			settings[1].source, settings[1].camtg, settings[1].enable,
			ktime_get_boottime_ns() / 1000);
	}
	/*switch state*/
	if (type == EXPOSURE_CHANGE_3_to_1 ||
		type == EXPOSURE_CHANGE_2_to_1) {
		_state_trans(job, E_STATE_CQ, E_STATE_CAMMUX_OUTER);
		_state_trans(job, E_STATE_OUTER, E_STATE_CAMMUX_OUTER);
	}

	return 0;
}

/* threaded irq context */
int wakeup_apply_sensor(struct mtk_cam_job *job)
{
	struct mtk_cam_stagger_job *stagger_job =
			(struct mtk_cam_stagger_job *)job;

	atomic_set(&stagger_job->expnum_change, 1);
	wake_up_interruptible(&stagger_job->expnum_change_wq);

	return 0;
}

int stream_on_otf_stagger(struct mtk_cam_job *job, bool on)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int raw_id = _get_master_raw_id(cam->engines.num_raw_devices,
			job->used_engine);
	struct mtk_raw_device *raw_dev =
		dev_get_drvdata(cam->engines.raw_devs[raw_id]);
	int feature = job->feature_job;
	int hw_scen;
	int scq_ms = SCQ_DEADLINE_MS * 3;

	if (is_stagger_2_exposure(feature)) {
		int ne_src_pad_idx = PAD_SRC_RAW0;
		int se_src_pad_idx = PAD_SRC_RAW1;
		int ne_tgo_pxl_mode = 1;
		int se_tgo_pxl_mode = 1;
		int ne_sv_ppl_idx = job->src_ctx->camsv_subdev_idx[0];

		/* fixme */
		hw_scen = BIT(MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER);
		/* ne */
		mtk_cam_seninf_set_pixelmode(ctx->seninf,
			ne_src_pad_idx, ne_tgo_pxl_mode);
		mtk_cam_seninf_set_camtg(ctx->seninf, ne_src_pad_idx,
			ctx->cam->pipelines.camsv[ne_sv_ppl_idx].cammux_id);
		mtk_cam_sv_dev_config(ctx, ne_sv_ppl_idx, get_hdr_scen_id(job),
				get_exp_order(job, ne_sv_ppl_idx), ne_tgo_pxl_mode);
		mtk_cam_sv_dev_stream_on(
			ctx, ne_sv_ppl_idx, on, hw_scen);
		/* se */
		stream_on(raw_dev, on, scq_ms, 0);
		if (job->stream_on_seninf)
			ctx_stream_on_seninf_sensor_hdr(job->src_ctx, on,
				se_src_pad_idx, se_tgo_pxl_mode, raw_dev->id);
	} else if (is_stagger_3_exposure(feature)) {
		int le_src_pad_idx = PAD_SRC_RAW0;
		int ne_src_pad_idx = PAD_SRC_RAW1;
		int se_src_pad_idx = PAD_SRC_RAW2;
		int le_tgo_pxl_mode = 1;
		int ne_tgo_pxl_mode = 1;
		int se_tgo_pxl_mode = 1;
		int le_sv_ppl_idx = job->src_ctx->camsv_subdev_idx[0];
		int ne_sv_ppl_idx = job->src_ctx->camsv_subdev_idx[1];

		/* fixme */
		hw_scen = BIT(MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER);
		/* le */
		mtk_cam_seninf_set_pixelmode(ctx->seninf,
			le_src_pad_idx, le_tgo_pxl_mode);
		mtk_cam_seninf_set_camtg(ctx->seninf, le_src_pad_idx,
			ctx->cam->pipelines.camsv[le_sv_ppl_idx].cammux_id);
		mtk_cam_sv_dev_config(ctx, le_sv_ppl_idx, get_hdr_scen_id(job),
				get_exp_order(job, le_sv_ppl_idx), le_tgo_pxl_mode);
		mtk_cam_sv_dev_stream_on(
			ctx, le_sv_ppl_idx, on, hw_scen);
		/* ne */
		mtk_cam_seninf_set_pixelmode(ctx->seninf,
			ne_src_pad_idx, ne_tgo_pxl_mode);
		mtk_cam_seninf_set_camtg(ctx->seninf, ne_src_pad_idx,
			ctx->cam->pipelines.camsv[ne_sv_ppl_idx].cammux_id);
		mtk_cam_sv_dev_config(ctx, ne_sv_ppl_idx, get_hdr_scen_id(job),
				get_exp_order(job, ne_sv_ppl_idx), ne_tgo_pxl_mode);
		mtk_cam_sv_dev_stream_on(
			ctx, ne_sv_ppl_idx, on, hw_scen);
		/* se */
		stream_on(raw_dev, on, scq_ms, 0);
		if (job->stream_on_seninf)
			ctx_stream_on_seninf_sensor_hdr(job->src_ctx, on,
				se_src_pad_idx, se_tgo_pxl_mode, raw_dev->id);
	} else {
		dev_dbg(cam->dev, "[%s] ctx:%d, job:%d, weird feature:%d\n",
			__func__, ctx->stream_id, job->frame_seq_no, feature);
	}

	return 0;
}


