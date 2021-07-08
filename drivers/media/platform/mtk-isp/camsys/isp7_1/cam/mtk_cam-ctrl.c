// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/of.h>

#include <media/v4l2-event.h>
#include <media/v4l2-subdev.h>

#include "mtk_cam.h"
#include "mtk_cam-ctrl.h"
#include "mtk_cam-debug.h"
#include "mtk_cam-dvfs_qos.h"
#include "mtk_cam-pool.h"
#include "mtk_cam-raw.h"
#include "mtk_cam-regs.h"
#include "mtk_cam-sv-regs.h"
#include "mtk_cam-v4l2.h"
#include "imgsys/mtk_imgsys-cmdq-ext.h"

#include "imgsensor-v4l2-controls.h"
#include "frame_sync_camsys.h"

#define SENSOR_SET_DEADLINE_MS  18
#define SENSOR_SET_RESERVED_MS  7
#define STATE_NUM_AT_SOF 3
#define INITIAL_DROP_FRAME_CNT 1

enum MTK_CAMSYS_STATE_RESULT {
	STATE_RESULT_TRIGGER_CQ = 0,
	STATE_RESULT_PASS_CQ_INIT,
	STATE_RESULT_PASS_CQ_SW_DELAY,
	STATE_RESULT_PASS_CQ_SCQ_DELAY,
	STATE_RESULT_PASS_CQ_HW_DELAY,
};

static void state_transition(struct mtk_camsys_ctrl_state *state_entry,
			     enum MTK_CAMSYS_STATE_IDX from,
			     enum MTK_CAMSYS_STATE_IDX to)
{
	if (state_entry->estate == from)
		state_entry->estate = to;
}

static void mtk_cam_event_eos(struct mtk_raw_pipeline *pipeline)
{
	struct v4l2_event event = {
		.type = V4L2_EVENT_EOS,
	};
	v4l2_event_queue(pipeline->subdev.devnode, &event);
}

static void mtk_cam_event_frame_sync(struct mtk_raw_pipeline *pipeline,
				     unsigned int frame_seq_no)
{
	struct v4l2_event event = {
		.type = V4L2_EVENT_FRAME_SYNC,
		.u.frame_sync.frame_sequence = frame_seq_no,
	};
	v4l2_event_queue(pipeline->subdev.devnode, &event);
}

static void mtk_cam_sv_event_frame_sync(struct mtk_camsv_device *camsv_dev,
				unsigned int frame_seq_no)
{
	struct v4l2_event event = {
		.type = V4L2_EVENT_FRAME_SYNC,
		.u.frame_sync.frame_sequence = frame_seq_no,
	};
	v4l2_event_queue(camsv_dev->pipeline->subdev.devnode, &event);
}

static void mtk_cam_event_request_drained(struct mtk_raw_pipeline *pipeline)
{
	struct v4l2_event event = {
		.type = V4L2_EVENT_REQUEST_DRAINED,
	};
	v4l2_event_queue(pipeline->subdev.devnode, &event);
}

static void mtk_cam_sv_event_request_drained(struct mtk_camsv_device *camsv_dev)
{
	struct v4l2_event event = {
		.type = V4L2_EVENT_REQUEST_DRAINED,
	};
	v4l2_event_queue(camsv_dev->pipeline->subdev.devnode, &event);
}

static bool mtk_cam_request_drained(struct mtk_camsys_sensor_ctrl *sensor_ctrl)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request *req, *req_prev;
	struct mtk_cam_request_stream_data *req_stream_data;
	unsigned long flags;
	int sensor_seq_no_next = sensor_ctrl->sensor_request_seq_no + 1;
	int res = 0;

	spin_lock_irqsave(&cam->running_job_lock, flags);
	list_for_each_entry_safe(req, req_prev, &cam->running_job_list, list) {
		req_stream_data = &req->stream_data[ctx->stream_id];
		/* Match by the en-queued request number */
		if (req->ctx_used & (1 << ctx->stream_id) &&
		    req_stream_data->frame_seq_no == sensor_seq_no_next)
			res = 1;
	}
	spin_unlock_irqrestore(&cam->running_job_lock, flags);
	/* Send V4L2_EVENT_REQUEST_DRAINED event */
	if (res == 0) {
		mtk_cam_event_request_drained(ctx->pipe);
		dev_dbg(ctx->cam->dev, "request_drained:(%d)\n",
			sensor_seq_no_next);
	}
	return (res == 0);
}

static void mtk_cam_sv_request_drained(struct mtk_camsv_device *camsv_dev,
		struct mtk_camsys_sensor_ctrl *sensor_ctrl)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request *req, *req_prev;
	struct mtk_cam_request_stream_data *req_stream_data;
	unsigned long flags;
	int sensor_seq_no_next = sensor_ctrl->sensor_request_seq_no + 1;
	int res = 0;

	spin_lock_irqsave(&cam->running_job_lock, flags);
	list_for_each_entry_safe(req, req_prev, &cam->running_job_list, list) {
		req_stream_data = &req->stream_data[ctx->stream_id];
		/* Match by the en-queued request number */
		if (req->ctx_used & (1 << ctx->stream_id) &&
			req_stream_data->frame_seq_no == sensor_seq_no_next)
			res = 1;
	}
	spin_unlock_irqrestore(&cam->running_job_lock, flags);
	/* Send V4L2_EVENT_REQUEST_DRAINED event */
	if (res == 0) {
		mtk_cam_sv_event_request_drained(camsv_dev);
		dev_dbg(camsv_dev->dev, "request_drained:(%d)\n", sensor_seq_no_next);
	}
}

static bool mtk_cam_req_frame_sync_start(struct mtk_cam_request *req)
{
	/* All ctx with sensor is in ready state */
	struct mtk_cam_device *cam =
		container_of(req->req.mdev, struct mtk_cam_device, media_dev);
	struct mtk_cam_ctx *ctx;
	int i;
	int ctx_cnt = 0;
	struct mtk_cam_ctx *sync_ctx[MTKCAM_SUBDEV_MAX];

	for (i = 0; i < cam->max_stream_num; i++) {
		if (!(1 << i & req->ctx_used))
			continue;

		ctx = &cam->ctxs[i];
		if (!ctx->sensor) {
			dev_info(cam->dev, "%s: ctx(%d): no sensor found\n",
				 __func__, ctx->stream_id);
			continue;
		}

		sync_ctx[ctx_cnt] = ctx;
		ctx_cnt++;
	}

	mutex_lock(&req->fs_op_lock);
	if (ctx_cnt > 1) {
		if (req->fs_on_cnt) { /* not first time */
			req->fs_on_cnt++;
			mutex_unlock(&req->fs_op_lock);
			return false;
		}
		req->fs_on_cnt++;
		for (i = 0; i < ctx_cnt; i++) {
			if (!sync_ctx[i]->synced) {
				struct v4l2_ctrl *ctrl = NULL;
				/**
				 * Use V4L2_CID_FRAME_SYNC to group sensors
				 * to be frame sync.
				 */
				ctx = sync_ctx[i];
				ctrl = v4l2_ctrl_find(ctx->sensor->ctrl_handler,
						      V4L2_CID_FRAME_SYNC);
				if (ctrl) {
					v4l2_ctrl_s_ctrl(ctrl, 1);
					ctx->synced = 1;
					dev_info(cam->dev,
						 "%s: ctx(%d): apply V4L2_CID_FRAME_SYNC(1)\n",
						 __func__, ctx->stream_id);
				} else {
					dev_info(cam->dev,
						 "%s: ctx(%d): failed to find V4L2_CID_FRAME_SYNC\n",
						 __func__, ctx->stream_id);
				}
			} else {
				dev_dbg(cam->dev,
					"%s: ctx(%d): skip V4L2_CID_FRAME_SYNC (already applied)\n",
					__func__, ctx->stream_id);
			}
		}

		dev_info(cam->dev, "%s:%s:fs_sync_frame(1): sync %d ctxs: 0x%x\n",
			__func__, req->req.debug_str, ctx_cnt, req->ctx_used);
		fs_sync_frame(1);

		mutex_unlock(&req->fs_op_lock);
		return true;
	}
	mutex_unlock(&req->fs_op_lock);
	return false;
}

static bool mtk_cam_req_frame_sync_end(struct mtk_cam_request *req)
{
	/* All ctx with sensor is not in ready state */
	struct mtk_cam_device *cam =
		container_of(req->req.mdev, struct mtk_cam_device, media_dev);
	struct mtk_cam_ctx *ctx;
	int i;
	int ctx_cnt = 0;

	for (i = 0; i < cam->max_stream_num; i++) {
		if (!(1 << i & req->ctx_used))
			continue;

		ctx = &cam->ctxs[i];
		if (!ctx->sensor) {
			dev_info(cam->dev, "%s: ctx(%d): no sensor found\n",
				 __func__, ctx->stream_id);
			continue;
		}

		ctx_cnt++;
	}

	mutex_lock(&req->fs_op_lock);
	if (ctx_cnt > 1 && req->fs_on_cnt) { /* check fs on */
		req->fs_on_cnt--;
		if (req->fs_on_cnt) { /* not the last */
			mutex_unlock(&req->fs_op_lock);
			return false;
		}
		dev_info(cam->dev,
			 "%s:%s:fs_sync_frame(0): sync %d ctxs: 0x%x\n",
			 __func__, req->req.debug_str, ctx_cnt, req->ctx_used);
		fs_sync_frame(0);

		mutex_unlock(&req->fs_op_lock);
		return true;
	}
	mutex_unlock(&req->fs_op_lock);
	return false;
}

/**
 * Handling E_STATE_CAMMUX_OUTER_CFG_DELAY state, we must
 * disable SENINF_MUX_EN and change the cammux setting.
 */
static void mtk_cam_link_change_worker(struct work_struct *work)
{
	struct mtk_cam_request *req =
		container_of(work, struct mtk_cam_request, link_work);
	struct mtk_cam_device *cam =
		container_of(req->req.mdev, struct mtk_cam_device, media_dev);
	struct mtk_cam_ctx *ctx;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl;
	struct mtk_cam_request_stream_data *req_stream_data;
	int i, ret, stream_id;

	dev_info(cam->dev, "%s, req->ctx_used:0x%x, req->ctx_link_update:0x%x\n",
		__func__, req->ctx_used, req->ctx_link_update);

	for (i = 0; i < cam->max_stream_num; i++) {
		if (req->ctx_used & (1 << i) && (req->ctx_link_update & (1 << i))) {
			stream_id = i;
			sensor_ctrl = &cam->ctxs[stream_id].sensor_ctrl;
			req_stream_data = &req->stream_data[stream_id];

			dev_info(cam->dev, "%s: pipe(%d): stream off seninf %s to switch sensor\n",
				 __func__, stream_id, req_stream_data->seninf_old->name);

			/* TBC: may not the normal way */
			req_stream_data->seninf_old->entity.stream_count--;
			ret = v4l2_subdev_call(req_stream_data->seninf_old, video, s_stream, 0);
			if (ret)
				dev_info(cam->dev, "failed to stream off seninf %s:%d\n",
					 req_stream_data->seninf_old->name, ret);
		}
	}

	dev_info(cam->dev, "%s: pipe(%d): update DVFS for %s\n",
		 __func__, stream_id, req_stream_data->seninf_new->name);
	mtk_cam_dvfs_update_clk(cam);

	for (i = 0; i < cam->max_stream_num; i++) {
		if (req->ctx_used & (1 << i) && req->ctx_link_update & (1 << i)) {
			stream_id = i;
			ctx = &cam->ctxs[stream_id];
			req_stream_data = &req->stream_data[stream_id];
			sensor_ctrl = &cam->ctxs[stream_id].sensor_ctrl;

			dev_info(cam->dev, "%s: pipe(%d): stream on seninf %s to switch sensor\n",
				 __func__, stream_id, req_stream_data->seninf_new->name);

			/**
			 * TO BE VERIFY: PAD_SRC_RAW0 here should be
			 * stream_id - MTKCAM_SUBDEV_RAW_0 +  PAD_SRC_RAW0
			 */
			mtk_cam_seninf_set_pixelmode(req_stream_data->seninf_new, PAD_SRC_RAW0,
						     ctx->pipe->res_config.tgo_pxl_mode);
			mtk_cam_seninf_set_camtg(req_stream_data->seninf_new, PAD_SRC_RAW0,
						 stream_id);
			/* TBC: may not the normal way */
			req_stream_data->seninf_new->entity.stream_count++;

			state_transition(&req_stream_data->state,
				E_STATE_CAMMUX_OUTER_CFG_DELAY, E_STATE_INNER);

			dev_info(cam->dev, "%s: pipe(%d): update BW for %s\n",
				 __func__, stream_id, req_stream_data->seninf_new->name);
			mtk_cam_qos_bw_calc(ctx);

			ret = v4l2_subdev_call(req_stream_data->seninf_new, video, s_stream, 1);
			if (ret)
				dev_info(cam->dev, "failed to stream on seninf %s:%d\n",
					 req_stream_data->seninf_new->name, ret);
			if (ctx->prev_sensor || ctx->prev_seninf) {
				ctx->prev_sensor = NULL;
				ctx->prev_seninf = NULL;
			}
		}
	}
	dev_dbg(cam->dev, "%s: cam mux switch done\n", __func__);
}

static void mtk_cam_sensor_worker(struct work_struct *work)
{
	struct mtk_cam_request *req = mtk_cam_req_work_to_req(work);
	struct mtk_cam_req_work *sensor_work = (struct mtk_cam_req_work *)work;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_device *cam =
		container_of(req->req.mdev, struct mtk_cam_device, media_dev);
	struct media_request_object *obj;
	struct media_request_object *pipe_obj = NULL;
	struct v4l2_ctrl_handler *parent_hdl;
	struct mtk_cam_ctx *ctx;
	struct mtk_raw_device *raw_dev = NULL;
	unsigned int time_after_sof = 0;
	int sv_i;
	int i;

	ctx = sensor_work->ctx;
	req_stream_data = &req->stream_data[ctx->stream_id];

	/* Update ctx->sensor for switch sensor cases */
	if (req_stream_data->seninf_new)
		mtk_cam_update_sensor(ctx, &req_stream_data->seninf_new->entity);

	dev_dbg(cam->dev, "%s:%s:ctx(%d):sensor try set start\n",
		__func__, req->req.debug_str, ctx->stream_id);

	if (mtk_cam_req_frame_sync_start(req))
		dev_dbg(cam->dev, "%s:%s:ctx(%d): sensor ctrl with frame sync - start\n",
			__func__, req->req.debug_str, ctx->stream_id);
	/* request setup*/
	if (!mtk_cam_is_stagger_m2m(ctx)) {
		list_for_each_entry(obj, &req->req.objects, list) {
			if (likely(obj))
				parent_hdl = obj->priv;
			else
				continue;
			if (parent_hdl == ctx->sensor->ctrl_handler ||
			    (ctx->prev_sensor && parent_hdl ==
			     ctx->prev_sensor->ctrl_handler)) {
				struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;

				v4l2_ctrl_request_setup(&req->req, parent_hdl);
				time_after_sof = ktime_get_boottime_ns() / 1000000 -
					sensor_ctrl->sof_time;
				dev_dbg(cam->dev,
					"[SOF+%dms] Sensor request:%d[ctx:%d] setup\n",
					time_after_sof, req_stream_data->frame_seq_no,
					ctx->stream_id);
			}

			if (parent_hdl == &ctx->pipe->ctrl_handler)
				pipe_obj = obj;
		}
	}
	if (mtk_cam_is_subsample(ctx))
		state_transition(&req_stream_data->state,
		E_STATE_SUBSPL_OUTER, E_STATE_SUBSPL_SENSOR);
	else if (mtk_cam_is_time_shared(ctx))
		state_transition(&req_stream_data->state,
		E_STATE_TS_READY, E_STATE_TS_SENSOR);
	else
		state_transition(&req_stream_data->state,
		E_STATE_READY, E_STATE_SENSOR);

	if (mtk_cam_req_frame_sync_end(req))
		dev_dbg(cam->dev, "%s:ctx(%d): sensor ctrl with frame sync - stop\n",
			__func__, ctx->stream_id);
	raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
	if (req_stream_data->frame_seq_no == 1 &&
		raw_dev->vf_en == 0 && ctx->sensor_ctrl.initial_cq_done == 1) {
		spin_lock(&ctx->streaming_lock);
		if (ctx->streaming) {
			stream_on(raw_dev, 1);
			for (i = 0; i < ctx->used_sv_num; i++)
				mtk_cam_sv_dev_stream_on(ctx, i, 1, 1);
			if (mtk_cam_is_stagger(ctx)) {
				unsigned int hw_scen =
					(1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER);
				for (i = MTKCAM_SUBDEV_CAMSV_END - 1;
					i >= MTKCAM_SUBDEV_CAMSV_START; i--) {
					if (ctx->pipe->enabled_raw & (1 << i))
						mtk_cam_sv_dev_stream_on(
							ctx, i - MTKCAM_SUBDEV_CAMSV_START,
							1, hw_scen);
				}
			}
		}
		spin_unlock(&ctx->streaming_lock);
	}
	req_stream_data->state.time_sensorset = ktime_get_boottime_ns() / 1000;
	dev_info(cam->dev, "%s:%s:ctx(%d)req(%d):sensor done at SOF+%dms\n",
		__func__, req->req.debug_str, ctx->stream_id,
		req_stream_data->frame_seq_no, time_after_sof);

	/* request complete - time consuming*/
	list_for_each_entry(obj, &req->req.objects, list) {
		if (likely(obj))
			parent_hdl = obj->priv;
		else
			continue;
		if (parent_hdl == ctx->sensor->ctrl_handler ||
		    (ctx->prev_sensor && parent_hdl ==
		     ctx->prev_sensor->ctrl_handler)) {
#ifdef SENSOR_AE_CTRL_COMPLETE
			v4l2_ctrl_request_complete(&req->req, parent_hdl);
#else
			media_request_object_complete(obj);
#endif
		}
	}

	/* mark pipeline control completed */
	if (likely(pipe_obj))
		media_request_object_complete(pipe_obj);
	/* time sharing sv wdma flow - stream on at 1st request*/
	if (mtk_cam_is_time_shared(ctx) &&
		req_stream_data->frame_seq_no == 1) {
		unsigned int hw_scen =
			(1 << MTKCAM_IPI_HW_PATH_OFFLINE_M2M);
		for (sv_i = MTKCAM_SUBDEV_CAMSV_END - 1;
			sv_i >= MTKCAM_SUBDEV_CAMSV_START; sv_i--)
			if (ctx->pipe->enabled_raw & (1 << sv_i))
				mtk_cam_sv_dev_stream_on(
					ctx, sv_i - MTKCAM_SUBDEV_CAMSV_START,
					1, hw_scen);
	}
}

static void mtk_cam_exp_switch_sensor_worker(struct work_struct *work)
{
	struct mtk_cam_request *req = mtk_cam_req_work_to_req(work);
	struct mtk_cam_req_work *sensor_work = (struct mtk_cam_req_work *)work;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_device *cam =
		container_of(req->req.mdev, struct mtk_cam_device, media_dev);
	struct media_request_object *obj;
	struct media_request_object *pipe_obj = NULL;
	struct v4l2_ctrl_handler *parent_hdl;
	struct mtk_cam_ctx *ctx;
	unsigned int time_after_sof = 0;

	ctx = sensor_work->ctx;
	req_stream_data = &req->stream_data[ctx->stream_id];
	dev_dbg(cam->dev, "%s:%s:ctx(%d):sensor try set start\n",
		__func__, req->req.debug_str, ctx->stream_id);

	if (mtk_cam_req_frame_sync_start(req))
		dev_dbg(cam->dev, "%s:%s:ctx(%d): sensor ctrl with frame sync - start\n",
			__func__, req->req.debug_str, ctx->stream_id);
	/* request setup*/
	list_for_each_entry(obj, &req->req.objects, list) {
		if (likely(obj))
			parent_hdl = obj->priv;
		else
			continue;
		if (parent_hdl == ctx->sensor->ctrl_handler ||
		    (ctx->prev_sensor && parent_hdl ==
		     ctx->prev_sensor->ctrl_handler)) {
			struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;

			v4l2_ctrl_request_setup(&req->req, parent_hdl);
			time_after_sof = ktime_get_boottime_ns() / 1000000 -
				sensor_ctrl->sof_time;
			dev_dbg(cam->dev,
				"[SOF+%dms] Sensor request:%d[ctx:%d] setup\n",
				time_after_sof, req_stream_data->frame_seq_no,
				ctx->stream_id);
		}

		if (parent_hdl == &ctx->pipe->ctrl_handler)
			pipe_obj = obj;
	}
	state_transition(&req_stream_data->state,
		E_STATE_READY, E_STATE_SENSOR);
	if (mtk_cam_req_frame_sync_end(req))
		dev_dbg(cam->dev, "%s:ctx(%d): sensor ctrl with frame sync - stop\n",
			__func__, ctx->stream_id);


	req_stream_data->state.time_sensorset = ktime_get_boottime_ns() / 1000;
	dev_info(cam->dev, "%s:%s:ctx(%d)req(%d):sensor done at SOF+%dms\n",
		__func__, req->req.debug_str, ctx->stream_id,
		req_stream_data->frame_seq_no, time_after_sof);

	/* request complete - time consuming*/
	list_for_each_entry(obj, &req->req.objects, list) {
		if (likely(obj))
			parent_hdl = obj->priv;
		else
			continue;
		if (parent_hdl == ctx->sensor->ctrl_handler ||
		    (ctx->prev_sensor && parent_hdl ==
		     ctx->prev_sensor->ctrl_handler)) {
#ifdef SENSOR_AE_CTRL_COMPLETE
			v4l2_ctrl_request_complete(&req->req, parent_hdl);
#else
			media_request_object_complete(obj);
#endif
		}
	}

	/* mark pipeline control completed */
	if (likely(pipe_obj))
		media_request_object_complete(pipe_obj);
}

static int mtk_camsys_exp_switch_cam_mux(struct mtk_raw_device *raw_dev,
		struct mtk_cam_ctx *ctx, struct mtk_cam_request_stream_data *req_stream_data)
{
	struct mtk_cam_seninf_mux_param param;
	struct mtk_cam_seninf_mux_setting settings[3];
	int type = req_stream_data->feature.switch_feature_type;
	int sv_main_id, sv_sub_id;

	if (type != EXPOSURE_CHANGE_NONE) {
		sv_main_id = get_main_sv_pipe_id(ctx->cam, ctx->pipe->enabled_raw);
		sv_sub_id = get_sub_sv_pipe_id(ctx->cam, ctx->pipe->enabled_raw);
		switch (type) {
		case EXPOSURE_CHANGE_3_to_2:
		case EXPOSURE_CHANGE_1_to_2:
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  = PipeIDtoTGIDX(sv_main_id);
			//settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  = PipeIDtoTGIDX(sv_sub_id);
			//settings[1].enable = 0;

			settings[2].seninf = ctx->seninf;
			settings[2].source = PAD_SRC_RAW2;
			settings[2].camtg  = PipeIDtoTGIDX(raw_dev->id);
			//settings[2].enable = 1;
			break;
		case EXPOSURE_CHANGE_3_to_1:
		case EXPOSURE_CHANGE_2_to_1:
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  = PipeIDtoTGIDX(raw_dev->id);
			//settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  = PipeIDtoTGIDX(sv_sub_id);
			//settings[1].enable = 0;

			settings[2].seninf = ctx->seninf;
			settings[2].source = PAD_SRC_RAW2;
			settings[2].camtg  = PipeIDtoTGIDX(sv_main_id);
			//settings[2].enable = 0;
			break;
		case EXPOSURE_CHANGE_2_to_3:
		case EXPOSURE_CHANGE_1_to_3:
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  = PipeIDtoTGIDX(sv_main_id);
			//settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  = PipeIDtoTGIDX(sv_sub_id);
			//settings[1].enable = 1;

			settings[2].seninf = ctx->seninf;
			settings[2].source = PAD_SRC_RAW2;
			settings[2].camtg  = PipeIDtoTGIDX(raw_dev->id);
			//settings[2].enable = 1;
			break;
		default:
			break;
		}
		param.settings = &settings[0];
		param.num = 3;
		mtk_cam_seninf_streaming_mux_change(&param);
		dev_info(ctx->cam->dev,
			"[%s] switch Req:%d, type:%d, cam_mux[0][1][2]:[%d/%d][%d/%d][%d/%d]\n",
			__func__, req_stream_data->frame_seq_no, type,
			settings[0].source, settings[0].camtg,/* settings[0].enable,*/
			settings[1].source, settings[1].camtg,/* settings[1].enable,*/
			settings[2].source, settings[2].camtg/*, settings[2].enable*/);
	}

	return 0;
}

static int mtk_cam_exp_sensor_switch(struct mtk_cam_ctx *ctx,
		struct mtk_cam_request_stream_data *req_stream_data)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_raw_device *raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
	int time_after_sof = ktime_get_boottime_ns() / 1000000 -
			   ctx->sensor_ctrl.sof_time;
	int type = req_stream_data->feature.switch_feature_type;

	if (!ctx->sensor_ctrl.sensorsetting_wq) {
		dev_info(cam->dev, "[set_sensor] return:workqueue null\n");
	} else {
		INIT_WORK(&req_stream_data->sensor_work.work,
		  mtk_cam_exp_switch_sensor_worker);
		queue_work(ctx->sensor_ctrl.sensorsetting_wq, &req_stream_data->sensor_work.work);
	}
	/*Normal to HDR switch case timing will be same as sensor mode switch*/
	if (type == EXPOSURE_CHANGE_1_to_2 ||
		type == EXPOSURE_CHANGE_1_to_3)
		mtk_camsys_exp_switch_cam_mux(raw_dev, ctx, req_stream_data);
	dev_dbg(cam->dev,
		"[%s] [SOF+%dms]] ctx:%d, req:%d\n",
		__func__, time_after_sof, ctx->stream_id, req_stream_data->frame_seq_no);

	return 0;
}

void mtk_cam_set_sub_sample_sensor(struct mtk_raw_device *raw_dev,
			       struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_request *current_req;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	int sensor_seq_no_next = sensor_ctrl->sensor_request_seq_no + 1;

	dev_dbg(ctx->cam->dev, "[%s:check] sensor_no:%d isp_no:%d\n", __func__,
				sensor_seq_no_next, sensor_ctrl->isp_request_seq_no);
	current_req = mtk_cam_dev_get_req(raw_dev->cam, ctx, sensor_seq_no_next);

	if (current_req && (sensor_seq_no_next > 1) &&
		(sensor_seq_no_next > sensor_ctrl->isp_request_seq_no)) {
		req_stream_data = &current_req->stream_data[ctx->stream_id];
		if (req_stream_data->state.estate == E_STATE_SUBSPL_OUTER) {
			dev_dbg(ctx->cam->dev, "[%s:setup] sensor_no:%d stream_no:%d\n", __func__,
				sensor_seq_no_next, req_stream_data->frame_seq_no);
			INIT_WORK(&req_stream_data->sensor_work.work,
				mtk_cam_sensor_worker);
			queue_work(sensor_ctrl->sensorsetting_wq,
				&req_stream_data->sensor_work.work);
			sensor_ctrl->sensor_request_seq_no++;
		} else if (req_stream_data->state.estate == E_STATE_SUBSPL_SCQ) {
			dev_dbg(ctx->cam->dev, "[%s:setup:SCQ] sensor_no:%d stream_no:%d\n",
				__func__, sensor_seq_no_next, req_stream_data->frame_seq_no);
		}
	}
}
void mtk_cam_subspl_req_prepare(struct mtk_camsys_sensor_ctrl *sensor_ctrl)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request *current_req = NULL;
	struct mtk_cam_request_stream_data *req_stream_data;
	int sensor_seq_no_next = sensor_ctrl->sensor_request_seq_no + 1;
	unsigned long flags;

	current_req = mtk_cam_dev_get_req(cam, ctx, sensor_seq_no_next);
	if (current_req) {
		req_stream_data = &current_req->stream_data[ctx->stream_id];
		if (req_stream_data->state.estate == E_STATE_READY) {
			req_stream_data->state.time_swirq_timer =
				ktime_get_boottime_ns() / 1000;
			dev_dbg(cam->dev, "[%s] sensor_no:%d stream_no:%d\n", __func__,
					sensor_seq_no_next, req_stream_data->frame_seq_no);
			/* EnQ this request's state element to state_list (STATE:READY) */
			spin_lock_irqsave(&sensor_ctrl->camsys_state_lock, flags);
			list_add_tail(&req_stream_data->state.state_element,
					  &sensor_ctrl->camsys_state_list);
			state_transition(&req_stream_data->state,
				E_STATE_READY, E_STATE_SUBSPL_READY);
			spin_unlock_irqrestore(&sensor_ctrl->camsys_state_lock, flags);
			req_stream_data->sensor_work.req = current_req;
			req_stream_data->sensor_work.ctx = sensor_ctrl->ctx;
		}
	}
}


static void mtk_cam_set_sensor(struct mtk_cam_request *current_req,
		struct mtk_camsys_sensor_ctrl *sensor_ctrl)
{
	unsigned long flags;
	struct mtk_cam_request_stream_data *req_stream_data;

	req_stream_data =
		&current_req->stream_data[sensor_ctrl->ctx->stream_id];
	/* EnQ this request's state element to state_list (STATE:READY) */
	spin_lock_irqsave(&sensor_ctrl->camsys_state_lock, flags);
	list_add_tail(&req_stream_data->state.state_element,
		      &sensor_ctrl->camsys_state_list);
	sensor_ctrl->sensor_request_seq_no = req_stream_data->frame_seq_no;
	spin_unlock_irqrestore(&sensor_ctrl->camsys_state_lock, flags);

	req_stream_data->sensor_work.req = current_req;
	req_stream_data->sensor_work.ctx = sensor_ctrl->ctx;
	if (req_stream_data->feature.switch_feature_type) {
		dev_info(sensor_ctrl->ctx->cam->dev,
		"[TimerIRQ] switch type:%d request:%d - pass sensor\n",
		req_stream_data->feature.switch_feature_type,
		req_stream_data->frame_seq_no);
		return;
	}
	if (!sensor_ctrl->sensorsetting_wq) {
		pr_info("[set_sensor] return:workqueue null\n");
	} else {
		INIT_WORK(&req_stream_data->sensor_work.work,
		  mtk_cam_sensor_worker);
		queue_work(sensor_ctrl->sensorsetting_wq, &req_stream_data->sensor_work.work);
	}
}

static enum hrtimer_restart sensor_set_handler(struct hrtimer *t)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl =
		container_of(t, struct mtk_camsys_sensor_ctrl,
			     sensor_deadline_timer);
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request *current_req = NULL;
	struct mtk_cam_request *state_req = NULL;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_camsys_ctrl_state *state_entry;
	unsigned long flags;
	int sensor_seq_no_next = sensor_ctrl->sensor_request_seq_no + 1;
	int time_after_sof = ktime_get_boottime_ns() / 1000000 -
			   ctx->sensor_ctrl.sof_time;

	spin_lock_irqsave(&sensor_ctrl->camsys_state_lock, flags);
	/* Check if previous state was without cq done */
	list_for_each_entry(state_entry, &sensor_ctrl->camsys_state_list,
				state_element) {
		state_req = state_entry->req;
		req_stream_data = &state_req->stream_data[ctx->stream_id];
		if (req_stream_data->frame_seq_no == sensor_ctrl->sensor_request_seq_no) {
			if (state_entry->estate == E_STATE_CQ && USINGSCQ &&
			    req_stream_data->frame_seq_no > INITIAL_DROP_FRAME_CNT) {
				state_entry->estate = E_STATE_CQ_SCQ_DELAY;
				spin_unlock_irqrestore(&sensor_ctrl->camsys_state_lock, flags);
				dev_info(ctx->cam->dev,
					"[TimerIRQ] SCQ DELAY STATE at SOF+%dms\n", time_after_sof);
				return HRTIMER_NORESTART;
			} else if (state_entry->estate == E_STATE_CAMMUX_OUTER_CFG) {
				state_entry->estate = E_STATE_CAMMUX_OUTER_CFG_DELAY;
				dev_dbg(ctx->cam->dev,
					"[TimerIRQ] CAMMUX OUTTER CFG DELAY STATE\n");
				spin_unlock(&sensor_ctrl->camsys_state_lock);
				return HRTIMER_NORESTART;

			} else if (state_entry->estate <= E_STATE_SENSOR) {
				spin_unlock_irqrestore(&sensor_ctrl->camsys_state_lock, flags);
				dev_info(ctx->cam->dev,
					"[TimerIRQ] wrong state:%d (sensor workqueue delay)\n",
					state_entry->estate);
				return HRTIMER_NORESTART;
			}
		} else if (req_stream_data->frame_seq_no ==
			sensor_ctrl->sensor_request_seq_no - 1) {
			if (state_entry->estate < E_STATE_INNER) {
				spin_unlock_irqrestore(&sensor_ctrl->camsys_state_lock, flags);
				dev_info(ctx->cam->dev,
					"[TimerIRQ] req:%d isn't arrive inner at SOF+%dms\n",
					req_stream_data->frame_seq_no, time_after_sof);
				return HRTIMER_NORESTART;
			}
		}
	}
	spin_unlock_irqrestore(&sensor_ctrl->camsys_state_lock, flags);

	current_req = mtk_cam_dev_get_req(cam, ctx, sensor_seq_no_next);
	req_stream_data = &current_req->stream_data[ctx->stream_id];

	if (current_req) {
		req_stream_data->state.time_swirq_timer = ktime_get_boottime_ns() / 1000;
		mtk_cam_set_sensor(current_req, &ctx->sensor_ctrl);
		dev_dbg(cam->dev,
			"[TimerIRQ [SOF+%dms]:] ctx:%d, sensor_req_seq_no:%d\n",
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
	unsigned int i;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl =
		container_of(t, struct mtk_camsys_sensor_ctrl,
			     sensor_deadline_timer);
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_camsv_device *camsv_dev;
	ktime_t m_kt;
	int time_after_sof = ktime_get_boottime_ns() / 1000000 -
			   sensor_ctrl->sof_time;
	bool drained_res = false;

	sensor_ctrl->sensor_deadline_timer.function = sensor_set_handler;
	m_kt = ktime_set(0, sensor_ctrl->timer_req_sensor * 1000000);

	if (ctx->used_raw_num) {
		/* handle V4L2_EVENT_REQUEST_DRAINED event */
		drained_res = mtk_cam_request_drained(sensor_ctrl);
	}
	for (i = 0; i < ctx->used_sv_num; i++) {
		camsv_dev = cam->camsys_ctrl.camsv_dev[ctx->sv_pipe[i]->id -
			MTKCAM_SUBDEV_CAMSV_START];
		dev_dbg(camsv_dev->dev, "[SOF+%dms]\n", time_after_sof);
		/* handle V4L2_EVENT_REQUEST_DRAINED event */
		mtk_cam_sv_request_drained(camsv_dev, sensor_ctrl);
	}
	dev_dbg(cam->dev,
			"[TimerIRQ [SOF+%dms]] ctx:%d, sensor_req_seq_no:%d\n",
			time_after_sof, ctx->stream_id,
			sensor_ctrl->sensor_request_seq_no);
	if (mtk_cam_is_subsample(ctx)) {
		if (!drained_res)
			mtk_cam_subspl_req_prepare(sensor_ctrl);
		return HRTIMER_NORESTART;
	}
	hrtimer_forward_now(&sensor_ctrl->sensor_deadline_timer, m_kt);

	return HRTIMER_RESTART;
}

static void mtk_cam_sof_timer_setup(struct mtk_cam_ctx *ctx)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	ktime_t m_kt;
	struct mtk_seninf_sof_notify_param param;

	/*notify sof to sensor*/
	param.sd = ctx->seninf;
	param.sof_cnt = sensor_ctrl->sensor_request_seq_no;
	mtk_cam_seninf_sof_notify(&param);

	sensor_ctrl->sof_time = ktime_get_boottime_ns() / 1000000;
	sensor_ctrl->sensor_deadline_timer.function =
		sensor_deadline_timer_handler;
	sensor_ctrl->ctx = ctx;
	m_kt = ktime_set(0, sensor_ctrl->timer_req_event * 1000000);
	hrtimer_start(&sensor_ctrl->sensor_deadline_timer, m_kt,
		      HRTIMER_MODE_REL);
}

static void
mtk_cam_set_timestamp(struct mtk_cam_request_stream_data *stream_data,
		      u64 time_boot,
		      u64 time_mono)
{
	stream_data->timestamp = time_boot;
	stream_data->timestamp_mono = time_mono;
}

int mtk_camsys_raw_subspl_state_handle(struct mtk_raw_device *raw_dev,
				   struct mtk_camsys_sensor_ctrl *sensor_ctrl,
		struct mtk_camsys_ctrl_state **current_state,
		int frame_inner_idx)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_camsys_ctrl_state *state_temp, *state_outer = NULL;
	struct mtk_camsys_ctrl_state *state_rec[STATE_NUM_AT_SOF];
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	int stateidx;
	int que_cnt = 0;
	unsigned long flags;
	u64 time_boot = ktime_get_boottime_ns();
	u64 time_mono = ktime_get_ns();

	/* List state-queue status*/
	spin_lock_irqsave(&sensor_ctrl->camsys_state_lock, flags);
	list_for_each_entry(state_temp, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req = state_temp->req;
		req_stream_data = &req->stream_data[ctx->stream_id];
		stateidx = (sensor_ctrl->sensor_request_seq_no + 1) -
			   req_stream_data->frame_seq_no;
		if (stateidx < STATE_NUM_AT_SOF && stateidx > -1) {
			state_rec[stateidx] = state_temp;
			/* Find outer state element */
			if (state_temp->estate == E_STATE_SUBSPL_SENSOR ||
				state_temp->estate == E_STATE_SUBSPL_OUTER) {
				state_outer = state_temp;
				mtk_cam_set_timestamp(req_stream_data,
						      time_boot, time_mono);
				req_stream_data->state.time_irq_sof2 =
							ktime_get_boottime_ns() / 1000;
			}
			if (state_temp->estate == E_STATE_SUBSPL_READY)
				req_stream_data->state.time_irq_sof1 =
							ktime_get_boottime_ns() / 1000;
			dev_dbg(raw_dev->dev,
			"[SOF-subsample] STATE_CHECK [N-%d] Req:%d / State:0x%x\n",
			stateidx, req_stream_data->frame_seq_no,
			state_rec[stateidx]->estate);
		}
		/* counter for state queue*/
		que_cnt++;
	}
	spin_unlock_irqrestore(&sensor_ctrl->camsys_state_lock, flags);
	/* HW imcomplete case */
	if (que_cnt >= STATE_NUM_AT_SOF)
		dev_dbg(raw_dev->dev, "[SOF-subsample] HW_DELAY state\n");
	/* Trigger high resolution timer to try sensor setting */
	mtk_cam_sof_timer_setup(ctx);
	/* Transit outer state to inner state */
	if (state_outer && sensor_ctrl->sensorsetting_wq) {
		req = state_outer->req;
		req_stream_data = &req->stream_data[ctx->stream_id];
		if (req_stream_data->frame_seq_no == frame_inner_idx) {
			if (frame_inner_idx > sensor_ctrl->isp_request_seq_no) {
				if (state_outer->estate == E_STATE_SUBSPL_OUTER) {
					mtk_cam_set_sub_sample_sensor(raw_dev, ctx);
					state_transition(state_outer, E_STATE_SUBSPL_OUTER,
						 E_STATE_SUBSPL_INNER);
					dev_dbg(raw_dev->dev, "sensor delay to SOF\n");
				}
				state_transition(state_outer, E_STATE_SUBSPL_SENSOR,
						 E_STATE_SUBSPL_INNER);
				sensor_ctrl->isp_request_seq_no =
					frame_inner_idx;
				dev_dbg(raw_dev->dev,
					"[SOF-subsample] frame_seq_no:%d, SENSOR/OUTER->INNER state:0x%x\n",
					req_stream_data->frame_seq_no, state_outer->estate);
			}
		}
	}
	/* Initial request case */
	if (sensor_ctrl->sensor_request_seq_no <
		INITIAL_DROP_FRAME_CNT) {
		sensor_ctrl->isp_request_seq_no = frame_inner_idx;
		dev_dbg(raw_dev->dev, "[SOF-subsample] INIT STATE cnt:%d\n", que_cnt);
		if (que_cnt > 0)
			state_transition(state_rec[0], E_STATE_SUBSPL_READY,
					 E_STATE_SUBSPL_SCQ);
		return STATE_RESULT_PASS_CQ_INIT;
	}
	if (que_cnt > 0) {
		/* CQ triggering judgment*/
		if (state_rec[0]->estate == E_STATE_SUBSPL_READY) {
			*current_state = state_rec[0];
			return STATE_RESULT_TRIGGER_CQ;
		}
		/* last SCQ triggering delay judgment*/
		if (state_rec[0]->estate == E_STATE_SUBSPL_SCQ_DELAY) {
			dev_dbg(raw_dev->dev, "[SOF-subsample] SCQ_DELAY state:0x%x\n",
				state_rec[0]->estate);
			return STATE_RESULT_PASS_CQ_SCQ_DELAY;
		}
	}

	return STATE_RESULT_PASS_CQ_SW_DELAY;
}

static int mtk_camsys_raw_state_handle(struct mtk_raw_device *raw_dev,
				   struct mtk_camsys_sensor_ctrl *sensor_ctrl,
		struct mtk_camsys_ctrl_state **current_state,
		int frame_inner_idx)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_camsys_ctrl_state *state_temp, *state_outer = NULL;
	struct mtk_camsys_ctrl_state *state_sensor = NULL;
	struct mtk_camsys_ctrl_state *state_inner = NULL;
	struct mtk_camsys_ctrl_state *state_rec[STATE_NUM_AT_SOF];
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	int stateidx;
	int que_cnt = 0;
	int raw_num = 0;
	int write_cnt;
	unsigned long flags;
	u64 time_boot = ktime_get_boottime_ns();
	u64 time_mono = ktime_get_ns();

	/* List state-queue status*/
	spin_lock_irqsave(&sensor_ctrl->camsys_state_lock, flags);
	list_for_each_entry(state_temp, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req = state_temp->req;
		req_stream_data = &req->stream_data[ctx->stream_id];
		stateidx = sensor_ctrl->sensor_request_seq_no -
			   req_stream_data->frame_seq_no;
		if (stateidx < STATE_NUM_AT_SOF && stateidx > -1) {
			state_rec[stateidx] = state_temp;
			/* Find outer state element */
			if (state_temp->estate == E_STATE_OUTER ||
			    state_temp->estate == E_STATE_OUTER_HW_DELAY) {
				state_outer = state_temp;
				mtk_cam_set_timestamp(req_stream_data,
						      time_boot, time_mono);
				req_stream_data->state.time_irq_sof2 =
							ktime_get_boottime_ns() / 1000;
			}
			/* Find inner state element request*/
			if (state_temp->estate == E_STATE_INNER ||
			    state_temp->estate == E_STATE_INNER_HW_DELAY) {
				state_inner = state_temp;
			}
			/* Find sensor state element request*/
			if (state_temp->estate <= E_STATE_SENSOR) {
				state_sensor = state_temp;
				req_stream_data->state.time_irq_sof1 =
							ktime_get_boottime_ns() / 1000;
			}
			dev_dbg(raw_dev->dev,
			"[SOF] STATE_CHECK [N-%d] Req:%d / State:%d\n",
			stateidx, req_stream_data->frame_seq_no,
			state_rec[stateidx]->estate);
		}
		/* counter for state queue*/
		que_cnt++;
	}
	spin_unlock_irqrestore(&sensor_ctrl->camsys_state_lock, flags);
	/* HW imcomplete case */
	if (state_inner) {
		req = state_inner->req;
		req_stream_data = &req->stream_data[ctx->stream_id];
		write_cnt = (sensor_ctrl->isp_request_seq_no / 256) * 256 + raw_dev->write_cnt;
		if (frame_inner_idx > sensor_ctrl->isp_request_seq_no ||
			req_stream_data->frame_done_queue_work == 1) {
			dev_dbg(raw_dev->dev, "[SOF] frame done work too late\n");
		} else if (write_cnt >= req_stream_data->frame_seq_no) {
			dev_info_ratelimited(raw_dev->dev, "[SOF] frame done reading lost %d frames\n",
				write_cnt - req_stream_data->frame_seq_no + 1);
			mtk_cam_set_timestamp(req_stream_data,
						      time_boot - 1000, time_mono - 1000);
			mtk_camsys_frame_done(ctx, write_cnt, ctx->stream_id);
		} else {
			state_transition(state_inner, E_STATE_INNER,
				 E_STATE_INNER_HW_DELAY);
			if (state_outer) {
				state_transition(state_outer, E_STATE_OUTER,
				 E_STATE_OUTER_HW_DELAY);
				state_transition(state_outer, E_STATE_CAMMUX_OUTER,
				 E_STATE_OUTER_HW_DELAY);
			}
			dev_info_ratelimited(raw_dev->dev, "[SOF] HW_IMCOMPLETE state\n");

			return STATE_RESULT_PASS_CQ_HW_DELAY;
		}
	}
	/* Trigger high resolution timer to try sensor setting */
	mtk_cam_sof_timer_setup(ctx);
	/* Transit outer state to inner state */
	if (state_outer && sensor_ctrl->sensorsetting_wq) {
		req = state_outer->req;
		req_stream_data = &req->stream_data[ctx->stream_id];
		if (sensor_ctrl->initial_drop_frame_cnt == 0 &&
			req_stream_data->frame_seq_no == frame_inner_idx) {
			if (frame_inner_idx > sensor_ctrl->isp_request_seq_no) {
				state_transition(state_outer,
						 E_STATE_OUTER_HW_DELAY,
						 E_STATE_INNER_HW_DELAY);
				state_transition(state_outer, E_STATE_OUTER,
						 E_STATE_INNER);
				state_transition(state_outer, E_STATE_CAMMUX_OUTER,
						 E_STATE_INNER);
				sensor_ctrl->isp_request_seq_no =
					frame_inner_idx;
				dev_dbg(raw_dev->dev,
					"[SOF-DBLOAD] frame_seq_no:%d, OUTER->INNER state:%d\n",
					req_stream_data->frame_seq_no, state_outer->estate);
			}
		}
	}
	/* Initial request case - 1st sensor wasn't set yet or initial drop wasn't finished*/
	if (sensor_ctrl->sensor_request_seq_no <= INITIAL_DROP_FRAME_CNT ||
		sensor_ctrl->initial_drop_frame_cnt) {
		dev_dbg(raw_dev->dev, "[SOF] INIT STATE cnt:%d\n", que_cnt);
		if (que_cnt > 0) {
			state_temp = state_rec[0];
			req = state_temp->req;
			req_stream_data = &req->stream_data[ctx->stream_id];
			if (req_stream_data->frame_seq_no == 1)
				state_transition(state_temp, E_STATE_SENSOR,
					 E_STATE_OUTER);
			/* Initial request readout will be delayed 1 frame*/
			if (raw_dev->sof_count == 1 &&
				(ctx->pipe->res_config.raw_feature == 0)) {
				write_readcount(raw_dev);
				raw_num = raw_dev->pipeline->res_config.raw_num_used;
				if (raw_num != 1) {
					struct mtk_raw_device *raw_dev_slave =
						get_slave_raw_dev(ctx->cam, raw_dev->pipeline);
					write_readcount(raw_dev_slave);
					if (raw_num == 3) {
						struct mtk_raw_device *raw_dev_slave2 =
							get_slave2_raw_dev(ctx->cam,
							raw_dev->pipeline);
						write_readcount(raw_dev_slave2);
					}
				}
			}
		}
		return STATE_RESULT_PASS_CQ_INIT;
	}
	if (que_cnt > 0) {
		/*handle exposure switch at frame start*/
		if (state_sensor) {
			req = state_sensor->req;
			req_stream_data = &req->stream_data[ctx->stream_id];
			if (req_stream_data->feature.switch_feature_type) {
				mtk_cam_exp_sensor_switch(ctx, req_stream_data);
				state_transition(state_sensor, E_STATE_READY,
						 E_STATE_SENSOR);
				*current_state = state_sensor;
				return STATE_RESULT_TRIGGER_CQ;
			}
		}
		if (state_rec[0]->estate == E_STATE_READY)
			dev_info(raw_dev->dev, "[SOF] sensor delay\n");
		/* CQ triggering judgment*/
		if (state_rec[0]->estate == E_STATE_SENSOR) {
			*current_state = state_rec[0];
			return STATE_RESULT_TRIGGER_CQ;
		}
		/* last SCQ triggering delay judgment*/
		if (state_rec[0]->estate == E_STATE_CQ_SCQ_DELAY) {
			state_transition(state_rec[0], E_STATE_CQ_SCQ_DELAY,
					 E_STATE_OUTER);
			dev_info(raw_dev->dev, "[SOF] SCQ_DELAY state:%d\n",
				state_rec[0]->estate);
			return STATE_RESULT_PASS_CQ_SCQ_DELAY;
		}
	}

	return STATE_RESULT_PASS_CQ_SW_DELAY;
}

static void mtk_camsys_ts_sv_done(struct mtk_cam_ctx *ctx,
				       unsigned int dequeued_frame_seq_no)
{
	struct mtk_cam_request *req = NULL;
	struct mtk_cam_request_stream_data *req_stream_data;

	/* Find request of this sv dequeued frame */
	req = mtk_cam_dev_get_req(ctx->cam, ctx, dequeued_frame_seq_no);
	if (req != NULL) {
		req_stream_data = &req->stream_data[ctx->stream_id];
		state_transition(&req_stream_data->state, E_STATE_TS_SV,
					E_STATE_TS_MEM);
		dev_info(ctx->cam->dev,
		"TS-SVD[ctx:%d-#%d], SV done state:0x%x\n",
		ctx->stream_id, dequeued_frame_seq_no, req_stream_data->state.estate);
	}

}


static void mtk_camsys_ts_raw_try_set(struct mtk_raw_device *raw_dev,
				       struct mtk_cam_ctx *ctx,
				       unsigned int dequeued_frame_seq_no)
{
	struct mtk_cam_request *req = NULL;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_working_buf_entry *buf_entry;
	dma_addr_t base_addr;

	/* Find request of this dequeued frame */
	req = mtk_cam_dev_get_req(ctx->cam, ctx, dequeued_frame_seq_no);
	if (!req) {
		dev_info(raw_dev->dev,
			"TS-CQ[ctx:%d-#%d], request drained\n",
			ctx->stream_id, dequeued_frame_seq_no);
		return;
	}
	req_stream_data = &req->stream_data[ctx->stream_id];
	if (raw_dev->time_shared_busy ||
		req_stream_data->state.estate != E_STATE_TS_MEM) {
		dev_info(raw_dev->dev,
			"TS-CQ[ctx:%d-#%d], CQ isn't updated [busy:%d/state:0x%x]\n",
			ctx->stream_id, dequeued_frame_seq_no,
			raw_dev->time_shared_busy, req_stream_data->state.estate);
		return;
	}
	raw_dev->time_shared_busy = true;
	/* Update CQ base address if needed */
	if (ctx->composed_frame_seq_no < dequeued_frame_seq_no) {
		dev_info(raw_dev->dev,
			"TS-CQ[ctx:%d-#%d], CQ isn't updated [composed_frame_deq (%d) ]\n",
			ctx->stream_id, dequeued_frame_seq_no,
			ctx->composed_frame_seq_no);
		return;
	}
	/* apply next composed buffer */
	spin_lock(&ctx->composed_buffer_list.lock);
	if (list_empty(&ctx->composed_buffer_list.list)) {
		dev_info(raw_dev->dev,
			"TS-CQ, no buffer update, cq_num:%d, frame_seq:%d\n",
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
		apply_cq(raw_dev,
			base_addr,
			buf_entry->cq_desc_size, buf_entry->cq_desc_offset, 0,
			buf_entry->sub_cq_desc_size, buf_entry->sub_cq_desc_offset);
		state_transition(&req_stream_data->state, E_STATE_TS_MEM,
						E_STATE_TS_CQ);
		raw_dev->time_shared_busy_ctx_id = ctx->stream_id;
		dev_info(raw_dev->dev,
			"TS-CQ[ctx:%d-#%d], update CQ state:0x%x [composed_req(%d)]\n",
			ctx->stream_id, dequeued_frame_seq_no, req_stream_data->state.estate,
			ctx->composed_frame_seq_no);
	}
}
static int mtk_camsys_ts_state_handle(
		struct mtk_camsys_sensor_ctrl *sensor_ctrl,
		struct mtk_camsys_ctrl_state **current_state,
		int frame_inner_idx)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_camsys_ctrl_state *state_temp = NULL;
	struct mtk_camsys_ctrl_state *state_rec[STATE_NUM_AT_SOF];
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	int stateidx;
	int que_cnt = 0;
	unsigned long flags;
	u64 time_boot = ktime_get_boottime_ns();
	u64 time_mono = ktime_get_ns();

	/* List state-queue status*/
	spin_lock_irqsave(&sensor_ctrl->camsys_state_lock, flags);
	list_for_each_entry(state_temp, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req = state_temp->req;
		req_stream_data = &req->stream_data[ctx->stream_id];
		stateidx = sensor_ctrl->sensor_request_seq_no -
			   req_stream_data->frame_seq_no;
		if (stateidx < STATE_NUM_AT_SOF && stateidx > -1) {
			state_rec[stateidx] = state_temp;
			if (state_temp->estate <= E_STATE_TS_SENSOR)
				req_stream_data->state.time_irq_sof1 =
					ktime_get_boottime_ns() / 1000;
			if (state_temp->estate == E_STATE_TS_SV) {
				req_stream_data->timestamp = time_boot;
				req_stream_data->timestamp_mono = time_mono;
			}
			dev_info(ctx->cam->dev,
			"[TS-SOF] ctx:%d STATE_CHECK [N-%d] Req:%d / State:0x%x\n",
			ctx->stream_id, stateidx, req_stream_data->frame_seq_no,
			state_rec[stateidx]->estate);
		}
		/* counter for state queue*/
		que_cnt++;
	}
	spin_unlock_irqrestore(&sensor_ctrl->camsys_state_lock, flags);
	if (que_cnt > 0) {
		if (state_rec[0]->estate == E_STATE_TS_READY) {
			dev_info(ctx->cam->dev, "[TS-SOF] sensor delay\n");
			return STATE_RESULT_PASS_CQ_SW_DELAY;
		}
	}
	/* Trigger high resolution timer to try sensor setting */
	mtk_cam_sof_timer_setup(ctx);
	if (que_cnt > 0) {
		/* camsv enque judgment*/
		if (state_rec[0]->estate == E_STATE_TS_SENSOR) {
			*current_state = state_rec[0];
			return STATE_RESULT_TRIGGER_CQ;
		}
	}

	return STATE_RESULT_PASS_CQ_SW_DELAY;
}
static void mtk_camsys_ts_frame_start(struct mtk_cam_ctx *ctx,
				       unsigned int dequeued_frame_seq_no)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request *req_cq = NULL;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_camsys_ctrl_state *current_state;
	enum MTK_CAMSYS_STATE_RESULT state_handle_ret;
	struct mtk_camsv_device *camsv_dev;
	struct mtk_camsv_device *camsv_top_dev;
	struct device *dev_sv;
	int sv_i;

	/* Send V4L2_EVENT_FRAME_SYNC event */
	mtk_cam_event_frame_sync(ctx->pipe, dequeued_frame_seq_no);

	if (ctx->sensor) {
		state_handle_ret =
			mtk_camsys_ts_state_handle(sensor_ctrl,
						&current_state,
						dequeued_frame_seq_no);
		if (state_handle_ret != STATE_RESULT_TRIGGER_CQ) {
			dev_info(cam->dev, "[TS-SOF] SV-ENQUE drop s:%d deq:%d\n",
				state_handle_ret, dequeued_frame_seq_no);
			return;
		}
	}
	/* Transit state from Sensor -> CQ */
	if (ctx->sensor) {
		state_transition(current_state,
			E_STATE_TS_SENSOR, E_STATE_TS_SV);
		req_cq = current_state->req;
		req_stream_data = &req_cq->stream_data[ctx->stream_id];
		/* time sharing sv wdma flow - stream on at 1st request*/
		for (sv_i = MTKCAM_SUBDEV_CAMSV_END - 1;
			sv_i >= MTKCAM_SUBDEV_CAMSV_START; sv_i--) {
			if (ctx->pipe->enabled_raw & (1 << sv_i)) {
				dev_sv = cam->sv.devs[sv_i - MTKCAM_SUBDEV_CAMSV_START];
				camsv_dev = dev_get_drvdata(dev_sv);
				camsv_top_dev =
					dev_get_drvdata(ctx->cam->sv.devs[camsv_dev->id / 2 * 2]);
				mtk_cam_sv_enquehwbuf(camsv_top_dev, camsv_dev,
				req_stream_data->frame_params.img_ins[0].buf[0].iova,
				req_stream_data->frame_seq_no);
			}
		}
		req_stream_data->timestamp = ktime_get_boottime_ns();
		req_stream_data->state.time_cqset = ktime_get_boottime_ns() / 1000;
		dev_info(cam->dev,
		"TS-SOF[ctx:%d-#%d], SV-ENQ req:%d is update, composed:%d, iova:0x%x, time:%lld\n",
		ctx->stream_id, dequeued_frame_seq_no, req_stream_data->frame_seq_no,
		ctx->composed_frame_seq_no, req_stream_data->frame_params.img_ins[0].buf[0].iova,
		req_stream_data->timestamp);
	}
}

static void mtk_camsys_raw_m2m_frame_done(struct mtk_raw_device *raw_dev,
				       struct mtk_cam_ctx *ctx,
				       unsigned int dequeued_frame_seq_no)
{
	struct mtk_camsys_ctrl_state *state_temp, *state_inner = NULL;
	struct mtk_camsys_ctrl_state *state_sensor = NULL;
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_working_buf_entry *buf_entry;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	dma_addr_t base_addr;
	int que_cnt = 0;
	unsigned long flags;
	u64 time_boot = ktime_get_boottime_ns();
	u64 time_mono = ktime_get_ns();

	/* Send V4L2_EVENT_FRAME_SYNC event */
	mtk_cam_event_frame_sync(ctx->pipe, dequeued_frame_seq_no);

	ctx->dequeued_frame_seq_no = dequeued_frame_seq_no;

	/* List state-queue status*/
	spin_lock_irqsave(&sensor_ctrl->camsys_state_lock, flags);
	list_for_each_entry(state_temp, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req = state_temp->req;
		req_stream_data = &req->stream_data[ctx->stream_id];

		if (state_temp->estate == E_STATE_INNER && state_inner == NULL)
			state_inner = state_temp;
		else if (state_temp->estate == E_STATE_SENSOR && state_sensor == NULL)
			state_sensor = state_temp;
		/* counter for state queue*/
		que_cnt++;
	}
	spin_unlock_irqrestore(&sensor_ctrl->camsys_state_lock, flags);

	/* Transit inner state to done state */
	if (state_inner) {
		req = state_inner->req;
		req_stream_data = &req->stream_data[ctx->stream_id];

		dev_dbg(raw_dev->dev,
			"[M2M P1 Don] req_stream_data->frame_seq_no:%d dequeued_frame_seq_no:%d\n",
			req_stream_data->frame_seq_no, dequeued_frame_seq_no);

		if (req_stream_data->frame_seq_no == dequeued_frame_seq_no) {
			state_transition(state_inner, E_STATE_INNER,
			      E_STATE_DONE_NORMAL);
			sensor_ctrl->isp_request_seq_no =
			      dequeued_frame_seq_no;
			dev_dbg(raw_dev->dev,
			      "[Frame done] frame_seq_no:%d, INNER->DONE_NORMAL state:%d\n",
			      req_stream_data->frame_seq_no, state_inner->estate);
		}
	}

	/* apply next composed buffer */
	spin_lock(&ctx->composed_buffer_list.lock);
	dev_dbg(raw_dev->dev,
		"[M2M check next action] que_cnt:%d composed_buffer_list.cnt:%d\n",
		que_cnt, ctx->composed_buffer_list.cnt);

	if (list_empty(&ctx->composed_buffer_list.list)) {
		dev_info(raw_dev->dev,
			"[M2M] no buffer, cq_num:%d, frame_seq:%d, composed_buffer_list.cnt :%d\n",
			ctx->composed_frame_seq_no, dequeued_frame_seq_no,
			ctx->composed_buffer_list.cnt);
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

		dev_dbg(raw_dev->dev,
			"[M2M P1 Don] ctx->processing_buffer_list.cnt:%d\n",
			ctx->processing_buffer_list.cnt);

		spin_unlock(&ctx->processing_buffer_list.lock);
		base_addr = buf_entry->buffer.iova;

		req = state_sensor->req;
		req_stream_data = &req->stream_data[ctx->stream_id];
		req_stream_data->timestamp = time_boot;
		req_stream_data->timestamp_mono = time_mono;

		apply_cq(raw_dev,
			base_addr,
			buf_entry->cq_desc_size, buf_entry->cq_desc_offset, 0,
			buf_entry->sub_cq_desc_size, buf_entry->sub_cq_desc_offset);
		/* Transit state from Sensor -> CQ */
		if (ctx->sensor) {
			state_transition(state_sensor, E_STATE_SENSOR, E_STATE_CQ);

			dev_dbg(raw_dev->dev,
			"M2M apply_cq [ctx:%d-#%d], CQ-%d, composed:%d, cq_addr:0x%x\n",
			ctx->stream_id, dequeued_frame_seq_no, req_stream_data->frame_seq_no,
			ctx->composed_frame_seq_no, base_addr);

			dev_dbg(raw_dev->dev,
			"M2M apply_cq: composed_buffer_list.cnt:%d time:%lld, monotime:%lld\n",
			ctx->composed_buffer_list.cnt, req_stream_data->timestamp,
			req_stream_data->timestamp_mono);
		}
	}

	if (mtk_cam_dequeue_req_frame(ctx, dequeued_frame_seq_no, ctx->stream_id))
		mtk_cam_dev_req_try_queue(ctx->cam);
}

static void mtk_camsys_raw_frame_start(struct mtk_raw_device *raw_dev,
				       struct mtk_cam_ctx *ctx,
				       unsigned int dequeued_frame_seq_no)
{
	struct mtk_cam_device *cam = raw_dev->cam;
	struct mtk_cam_request *req = NULL, *req_cq = NULL;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_cam_working_buf_entry *buf_entry;
	struct mtk_camsys_ctrl_state *current_state;
	dma_addr_t base_addr;
	enum MTK_CAMSYS_STATE_RESULT state_handle_ret;

	/* inner register dequeue number */
	if (!mtk_cam_is_stagger(ctx))
		ctx->dequeued_frame_seq_no = dequeued_frame_seq_no;
	/* Send V4L2_EVENT_FRAME_SYNC event */
	mtk_cam_event_frame_sync(ctx->pipe, dequeued_frame_seq_no);
	/* Find request of this dequeued frame */
	req = mtk_cam_dev_get_req(cam, ctx, dequeued_frame_seq_no);
	/* If continuous 8 frame dequeue failed, we trigger the debug dump */
	mtk_cam_debug_detect_dequeue_failed(ctx, req, 8);

	if (ctx->sensor) {
		if (mtk_cam_is_subsample(ctx))
			state_handle_ret =
			mtk_camsys_raw_subspl_state_handle(raw_dev, sensor_ctrl,
						&current_state,
						dequeued_frame_seq_no);
		else
			state_handle_ret =
			mtk_camsys_raw_state_handle(raw_dev, sensor_ctrl,
						&current_state,
						dequeued_frame_seq_no);
		if (state_handle_ret != STATE_RESULT_TRIGGER_CQ) {
			dev_dbg(raw_dev->dev, "[SOF] CQ drop s:%d deq:%d\n",
				state_handle_ret, dequeued_frame_seq_no);
			return;
		}
	}
	/* Update CQ base address if needed */
	if (ctx->composed_frame_seq_no <= dequeued_frame_seq_no) {
		dev_info(raw_dev->dev,
			"SOF[ctx:%d-#%d], CQ isn't updated [composed_frame_deq (%d) ]\n",
			ctx->stream_id, dequeued_frame_seq_no,
			ctx->composed_frame_seq_no);
		return;
	}
	/* apply next composed buffer */
	spin_lock(&ctx->composed_buffer_list.lock);
	if (list_empty(&ctx->composed_buffer_list.list)) {
		dev_info(raw_dev->dev,
			"SOF_INT_ST, no buffer update, cq_num:%d, frame_seq:%d\n",
			ctx->composed_frame_seq_no, dequeued_frame_seq_no);
		spin_unlock(&ctx->composed_buffer_list.lock);
	} else {
		if (ctx->used_sv_num)
			mtk_cam_sv_apply_next_buffer(ctx);

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
		apply_cq(raw_dev,
			base_addr,
			buf_entry->cq_desc_size, buf_entry->cq_desc_offset, 0,
			buf_entry->sub_cq_desc_size, buf_entry->sub_cq_desc_offset);
		/* Transit state from Sensor -> CQ */
		if (ctx->sensor) {
			if (mtk_cam_is_subsample(ctx))
				state_transition(current_state,
				E_STATE_SUBSPL_READY, E_STATE_SUBSPL_SCQ);
			else
				state_transition(current_state,
				E_STATE_SENSOR, E_STATE_CQ);
			req_cq = current_state->req;
			req_stream_data = &req_cq->stream_data[ctx->stream_id];
			req_stream_data->state.time_cqset = ktime_get_boottime_ns() / 1000;
			dev_dbg(raw_dev->dev,
			"SOF[ctx:%d-#%d], CQ-%d is update, composed:%d, cq_addr:0x%x, time:%lld, monotime:%lld\n",
			ctx->stream_id, dequeued_frame_seq_no, req_stream_data->frame_seq_no,
			ctx->composed_frame_seq_no, base_addr, req_stream_data->timestamp,
			req_stream_data->timestamp_mono);
		}
	}
}

int mtk_cam_hdr_last_frame_start(struct mtk_raw_device *raw_dev,
			struct mtk_cam_ctx *ctx, int deque_frame_no)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_camsys_ctrl_state *state_temp;
	struct mtk_camsys_ctrl_state *state_switch = NULL;
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	int stateidx;
	int que_cnt = 0;
	unsigned long flags;

	sensor_ctrl->ctx->dequeued_frame_seq_no = deque_frame_no;
	/*1-exp - as normal mode*/
	if (!raw_dev->stagger_en) {
		mtk_camsys_raw_frame_start(raw_dev, ctx, deque_frame_no);
		return 0;
	}
	/* List state-queue status*/
	spin_lock_irqsave(&sensor_ctrl->camsys_state_lock, flags);
	list_for_each_entry(state_temp, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req = state_temp->req;
		req_stream_data = &req->stream_data[ctx->stream_id];
		stateidx = sensor_ctrl->sensor_request_seq_no -
			   req_stream_data->frame_seq_no;
		if (stateidx < STATE_NUM_AT_SOF && stateidx > -1) {
			/* Find switch element for switch request*/
			if (state_temp->estate > E_STATE_SENSOR &&
			    state_temp->estate < E_STATE_INNER) {
				state_switch = state_temp;
			}
			dev_dbg(ctx->cam->dev,
			"[%s] STATE_CHECK [N-%d] Req:%d / State:%d\n",
			__func__, stateidx, req_stream_data->frame_seq_no, state_temp->estate);
		}
		/* counter for state queue*/
		que_cnt++;
	}
	spin_unlock_irqrestore(&sensor_ctrl->camsys_state_lock, flags);
	/*HDR to Normal cam mux switch case timing will be at last sof*/
	if (state_switch) {
		req = state_switch->req;
		req_stream_data = &req->stream_data[ctx->stream_id];
		if (req_stream_data->feature.switch_feature_type) {
			mtk_camsys_exp_switch_cam_mux(raw_dev, ctx, req_stream_data);
			dev_info(ctx->cam->dev,
			"[%s] switch Req:%d / State:%d\n",
			__func__, req_stream_data->frame_seq_no, state_switch->estate);
		} else {
			dev_info(ctx->cam->dev,
			"[%s] non-switch Req:%d / State:%d\n",
			__func__, req_stream_data->frame_seq_no, state_switch->estate);
		}
	}

	return 0;
}

static void mtk_cam_handle_mux_switch(struct mtk_raw_device *raw_dev,
				   struct mtk_cam_ctx *ctx,
				   struct mtk_cam_request *req)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request_stream_data *req_stream_data;
	int cnt_need_change_mux_cq_done = 0;
	int cnt_need_change_mux = 0;
	struct mtk_cam_request_stream_data *stream_data_change[MTKCAM_SUBDEV_MAX];
	struct mtk_cam_seninf_mux_setting mux_settings[
			MTKCAM_SUBDEV_RAW_END - MTKCAM_SUBDEV_RAW_START];
	int i;

	if (!(req->ctx_used & cam->streaming_ctx & req->ctx_link_update))
		return;

	/* Check if all ctx is ready to change mux though double buffer */
	for (i = 0; i < cam->max_stream_num; i++) {
		if (1 << i & (req->ctx_used & cam->streaming_ctx & req->ctx_link_update)) {
			cnt_need_change_mux++;
			req_stream_data = &req->stream_data[i];
			if (req->stream_data[i].state.estate == E_STATE_OUTER) {
				stream_data_change[cnt_need_change_mux_cq_done] = req_stream_data;
				mux_settings[cnt_need_change_mux_cq_done].seninf =
					req_stream_data->seninf_new;
				mux_settings[cnt_need_change_mux_cq_done].source =
					i -  MTKCAM_SUBDEV_RAW_0 +	PAD_SRC_RAW0;
				mux_settings[cnt_need_change_mux_cq_done].camtg =
					i;
				cnt_need_change_mux_cq_done++;

			}
		}
	}

	if (!cnt_need_change_mux ||
		cnt_need_change_mux != cnt_need_change_mux_cq_done) {
		dev_dbg(raw_dev->dev,
				"%s:%s: No cam mux change,ctx_used(0x%x),link_update(0x%x),streaming_ctx(0x%x)\n",
				__func__, req->req.debug_str, req->ctx_used, req->ctx_link_update,
				cam->streaming_ctx);
		return;
	}

	for (i = 0 ; i < cnt_need_change_mux_cq_done; i++) {
		state_transition(&(stream_data_change[i]->state), E_STATE_OUTER,
			E_STATE_CAMMUX_OUTER_CFG);
	}

}

static void mtk_camsys_raw_m2m_cq_done(struct mtk_raw_device *raw_dev,
				   struct mtk_cam_ctx *ctx,
				   unsigned int frame_seq_no_outer)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_camsys_ctrl_state *state_entry;
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;

	if (frame_seq_no_outer == 1)
		stream_on(raw_dev, 1);

	dev_info(raw_dev->dev,
		"[M2M CQD] frame_seq_no_outer:%d composed_buffer_list.cnt:%d\n",
		frame_seq_no_outer, ctx->composed_buffer_list.cnt);

	spin_lock(&sensor_ctrl->camsys_state_lock);
	list_for_each_entry(state_entry, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req = state_entry->req;
		req_stream_data = &req->stream_data[ctx->stream_id];

		if (req_stream_data->frame_seq_no == frame_seq_no_outer) {
			if (frame_seq_no_outer > sensor_ctrl->isp_request_seq_no) {
				/**
				 * outer number is 1 more from last SOF's
				 * inner number
				 */
				state_transition(state_entry, E_STATE_CQ,
						E_STATE_OUTER);

				req_stream_data->state.time_irq_outer =
					ktime_get_boottime_ns() / 1000;
				dev_dbg(raw_dev->dev,
					"[M2M CQD] req:%d, CQ->OUTER state:%d\n",
					req_stream_data->frame_seq_no, state_entry->estate);
			}
		}
	}
	spin_unlock(&sensor_ctrl->camsys_state_lock);
}

static void mtk_camsys_raw_cq_done(struct mtk_raw_device *raw_dev,
				   struct mtk_cam_ctx *ctx,
				   unsigned int frame_seq_no_outer)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_camsys_ctrl_state *state_entry;
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	int i, type;

	/* initial CQ done */
	if (raw_dev->sof_count == 0) {
		req = mtk_cam_dev_get_req(ctx->cam, ctx, 1);
		req_stream_data = &req->stream_data[ctx->stream_id];
		sensor_ctrl->initial_cq_done = 1;
		if (req_stream_data->state.estate >= E_STATE_SENSOR) {
			unsigned int hw_scen =
				(1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER);

			spin_lock(&ctx->streaming_lock);
			if (ctx->streaming) {
				stream_on(raw_dev, 1);
				for (i = 0; i < ctx->used_sv_num; i++)
					mtk_cam_sv_dev_stream_on(ctx, i, 1, 1);

				for (i = MTKCAM_SUBDEV_CAMSV_END - 1;
					i >= MTKCAM_SUBDEV_CAMSV_START; i--) {
					if (mtk_cam_is_stagger(ctx) &&
						ctx->pipe->enabled_raw & (1 << i))
						mtk_cam_sv_dev_stream_on(
							ctx, i - MTKCAM_SUBDEV_CAMSV_START,
							1, hw_scen);
				}

			}
			spin_unlock(&ctx->streaming_lock);
		} else {
			dev_dbg(raw_dev->dev,
				"[CQD] 1st sensor not set yet, req:%d, state:%d\n",
				req_stream_data->frame_seq_no, req_stream_data->state.estate);
		}
	}
	/* Legacy CQ done will be always happened at frame done */
	spin_lock(&sensor_ctrl->camsys_state_lock);
	list_for_each_entry(state_entry, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req = state_entry->req;
		req_stream_data = &req->stream_data[ctx->stream_id];
		if (mtk_cam_is_subsample(ctx)) {
			state_transition(state_entry, E_STATE_SUBSPL_SCQ,
						E_STATE_SUBSPL_OUTER);
			state_transition(state_entry, E_STATE_SUBSPL_SCQ_DELAY,
						E_STATE_SUBSPL_OUTER);
			if (raw_dev->sof_count == 0)
				state_transition(state_entry, E_STATE_SUBSPL_READY,
						E_STATE_SUBSPL_OUTER);
			req_stream_data->state.time_irq_outer = ktime_get_boottime_ns() / 1000;
			dev_dbg(raw_dev->dev,
					"[CQD-subsample] req:%d, CQ->OUTER state:0x%x\n",
					req_stream_data->frame_seq_no, state_entry->estate);
		} else if (mtk_cam_is_time_shared(ctx)) {
			if (req_stream_data->frame_seq_no == frame_seq_no_outer &&
				frame_seq_no_outer > sensor_ctrl->isp_request_seq_no) {
				state_transition(state_entry, E_STATE_TS_CQ,
						E_STATE_TS_INNER);
				dev_info(raw_dev->dev, "[TS-SOF] ctx:%d sw trigger rawi_r2 req:%d->%d, state:0x%x\n",
						ctx->stream_id, ctx->dequeued_frame_seq_no,
						req_stream_data->frame_seq_no, state_entry->estate);
				ctx->dequeued_frame_seq_no = frame_seq_no_outer;
				writel_relaxed(RAWI_R2_TRIG, raw_dev->base + REG_CTL_RAWI_TRIG);
				raw_dev->sof_count++;
				wmb(); /* TBC */
			}
		} else if (req_stream_data->frame_seq_no == frame_seq_no_outer) {
			if (frame_seq_no_outer > sensor_ctrl->isp_request_seq_no) {
				/**
				 * outer number is 1 more from last SOF's
				 * inner number
				 */
				state_transition(state_entry, E_STATE_CQ,
						E_STATE_OUTER);
				state_transition(state_entry,
						E_STATE_CQ_SCQ_DELAY,
						E_STATE_OUTER);
				req_stream_data->state.time_irq_outer =
						ktime_get_boottime_ns() / 1000;
				type = req_stream_data->feature.switch_feature_type;
				if (type != 0) {
					if (type == EXPOSURE_CHANGE_3_to_1 ||
						type == EXPOSURE_CHANGE_2_to_1)
						stagger_disable(raw_dev);
					else if (type == EXPOSURE_CHANGE_1_to_2 ||
						type == EXPOSURE_CHANGE_1_to_3)
						stagger_enable(raw_dev);
					dev_dbg(raw_dev->dev,
						"[CQD-switch] req:%d type:%d\n",
						req_stream_data->frame_seq_no, type);
				}
				dev_dbg(raw_dev->dev,
					"[CQD] req:%d, CQ->OUTER state:%d\n",
					req_stream_data->frame_seq_no, state_entry->estate);
				mtk_cam_handle_mux_switch(raw_dev, ctx, req);
			}
		}
	}
	spin_unlock(&sensor_ctrl->camsys_state_lock);
}

static void mtk_camsys_raw_m2m_trigger(struct mtk_raw_device *raw_dev,
				   struct mtk_cam_ctx *ctx,
				   unsigned int frame_seq_no_outer)
{

	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_camsys_ctrl_state *state_entry;
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;

	if (!(raw_dev->pipeline->res_config.raw_feature & MTK_CAM_FEATURE_STAGGER_M2M_MASK))
		return;

	trigger_rawi(raw_dev);

	spin_lock(&sensor_ctrl->camsys_state_lock);
	list_for_each_entry(state_entry, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req = state_entry->req;
		req_stream_data = &req->stream_data[ctx->stream_id];
		if (req_stream_data->frame_seq_no == frame_seq_no_outer) {
			/**
			 * outer number is 1 more from last SOF's
			 * inner number
			 */
			sensor_ctrl->isp_request_seq_no = frame_seq_no_outer;
			state_transition(state_entry, E_STATE_OUTER,
					E_STATE_INNER);
			req_stream_data->state.time_irq_sof1 = ktime_get_boottime_ns() / 1000;
			dev_dbg(raw_dev->dev,
				"[SW Trigger] req:%d, M2M CQ->INNER state:%d frame_seq_no:%d\n",
				req_stream_data->frame_seq_no, state_entry->estate,
				frame_seq_no_outer);
		}
	}
	spin_unlock(&sensor_ctrl->camsys_state_lock);

}

static bool
mtk_camsys_raw_prepare_frame_done(struct mtk_raw_device *raw_dev,
				  struct mtk_cam_ctx *ctx,
				  unsigned int dequeued_frame_seq_no)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_camsys_sensor_ctrl *camsys_sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_camsys_ctrl_state *state_entry;
	struct mtk_cam_request *state_req;
	struct mtk_cam_request_stream_data *req_stream_data;
	unsigned long flags;

	if (ctx->sensor) {
		spin_lock_irqsave(&camsys_sensor_ctrl->camsys_state_lock, flags);
		/**
		 * Find inner register number's request and transit to
		 * STATE_DONE_xxx
		 */
		list_for_each_entry(state_entry,
				    &camsys_sensor_ctrl->camsys_state_list,
				    state_element) {
			state_req = state_entry->req;
			req_stream_data = &state_req->stream_data[ctx->stream_id];
			if (req_stream_data->frame_seq_no == dequeued_frame_seq_no) {
				if (mtk_cam_is_subsample(ctx)) {
					state_transition(state_entry,
						E_STATE_SUBSPL_INNER,
						E_STATE_SUBSPL_DONE_NORMAL);
					if (state_entry->estate == E_STATE_SUBSPL_DONE_NORMAL)
						req_stream_data->state.time_irq_done =
							ktime_get_boottime_ns() / 1000;
					dev_dbg(cam->dev, "[SWD-subspl] req:%d/state:0x%x/time:%lld\n",
						req_stream_data->frame_seq_no, state_entry->estate,
						req_stream_data->timestamp);
				} else if (mtk_cam_is_time_shared(ctx)) {
					state_transition(state_entry,
						E_STATE_TS_INNER,
						E_STATE_TS_DONE_NORMAL);
					if (state_entry->estate == E_STATE_TS_DONE_NORMAL)
						req_stream_data->state.time_irq_done =
							ktime_get_boottime_ns() / 1000;
					dev_dbg(cam->dev, "[TS-SWD] ctx:%d req:%d/state:0x%x/time:%lld\n",
						ctx->stream_id, req_stream_data->frame_seq_no,
						state_entry->estate, req_stream_data->timestamp);
				} else {
					state_transition(state_entry,
							 E_STATE_INNER_HW_DELAY,
							 E_STATE_DONE_MISMATCH);
					state_transition(state_entry, E_STATE_INNER,
							 E_STATE_DONE_NORMAL);
					if (state_entry->estate == E_STATE_DONE_NORMAL)
						req_stream_data->state.time_irq_done =
							ktime_get_boottime_ns() / 1000;
					if (camsys_sensor_ctrl->isp_request_seq_no == 0)
						state_transition(state_entry,
						 E_STATE_CQ,
						 E_STATE_OUTER);
					dev_dbg(cam->dev, "[SWD] req:%d/state:%d/time:%lld/sync_id:%lld\n",
					req_stream_data->frame_seq_no,
					state_entry->estate,
					req_stream_data->timestamp,
					state_req->sync_id);
					if (state_req->sync_id != -1)
						imgsys_cmdq_setevent(state_req->sync_id);
					}
			}
		}
		spin_unlock_irqrestore(&camsys_sensor_ctrl->camsys_state_lock, flags);
	} else {
		dev_info(cam->dev, "%s: no sensor found in ctx:%d, req:%d",
			 __func__, ctx->stream_id, dequeued_frame_seq_no);
	}

	return true;
}

static void
mtk_camsys_raw_change_pipeline(struct mtk_raw_device *raw_dev,
			       struct mtk_cam_ctx *ctx,
			       struct mtk_camsys_sensor_ctrl *sensor_ctrl,
			       unsigned int dequeued_frame_seq_no)
{
	int i;
	struct mtk_cam_device *cam = raw_dev->cam;
	struct mtk_cam_request *req;
	int frame_seq = dequeued_frame_seq_no + 1;

	req = mtk_cam_dev_get_req(cam, ctx, frame_seq);

	if (!req) {
		dev_dbg(raw_dev->dev, "%s next req (%d) not queued\n", __func__, frame_seq);
		return;
	}

	if (!req->ctx_link_update) {
		dev_dbg(raw_dev->dev, "%s next req (%d) no link stup\n", __func__, frame_seq);
		return;
	}

	dev_dbg(raw_dev->dev, "%s:req(%d) check: req->ctx_used:0x%x, req->ctx_link_update0x%x\n",
		__func__, frame_seq, req->ctx_used, req->ctx_link_update);

	/* Check if all ctx is ready to change link */
	for (i = 0; i < cam->max_stream_num; i++) {
		if ((req->ctx_used & 1 << i) && (req->ctx_link_update & (1 << i))) {
			/**
			 * Switch cammux double buffer write delay, we have to disable the
			 * mux (mask the data and sof to raw) and than switch it.
			 */
			if (req->stream_data[i].state.estate == E_STATE_CAMMUX_OUTER_CFG_DELAY) {
				/**
				 * To be move to the start of frame done hanlding
				 * INIT_WORK(&req->link_work, mtk_cam_link_change_worker);
				 * queue_work(cam->link_change_wq, &req->link_work);
				 */
				dev_info(raw_dev->dev, "Exchange streams at req(%d), update link ctx (0x%x)\n",
				frame_seq, ctx->stream_id, req->ctx_link_update);
				mtk_cam_link_change_worker(&req->link_work);
				return;
			}
		}
	}
	dev_info(raw_dev->dev, "%s:req(%d) no link update data found!\n",
		__func__, frame_seq);

}

static void mtk_cam_handle_frame_done(struct mtk_cam_ctx *ctx,
				      unsigned int frame_seq_no,
				      unsigned int pipe_id)
{
	struct mtk_raw_device *raw_dev = NULL;
	bool need_dequeue;

	/**
	 * If ctx is already off, just return; mtk_cam_dev_req_cleanup()
	 * triggered by mtk_cam_vb2_stop_streaming() puts the all media
	 * requests back.
	 */
	spin_lock(&ctx->streaming_lock);
	if (!ctx->streaming) {
		dev_dbg(ctx->cam->dev,
			 "%s: skip frame done for stream off ctx:%d\n",
			 __func__, ctx->stream_id);
		spin_unlock(&ctx->streaming_lock);
		return;
	}
	spin_unlock(&ctx->streaming_lock);

	if (mtk_camsv_is_sv_pipe(pipe_id)) {
		need_dequeue = true;
	} else {
		raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
		need_dequeue =
			mtk_camsys_raw_prepare_frame_done(raw_dev, ctx,
							  frame_seq_no);
	}

	if (!need_dequeue)
		return;

	dev_info(ctx->cam->dev, "[%s] job done ctx-%d:pipe-%d:req(%d)\n",
			 __func__, ctx->stream_id, pipe_id, frame_seq_no);
	if (mtk_cam_dequeue_req_frame(ctx, frame_seq_no, pipe_id)) {
		mtk_cam_dev_req_try_queue(ctx->cam);
		mtk_cam_debug_wakeup(&ctx->cam->debug_exception_waitq);
		mtk_camsys_raw_change_pipeline(raw_dev, ctx,
							&ctx->sensor_ctrl,
							frame_seq_no);
	}
}

void mtk_cam_frame_done_work(struct work_struct *work)
{
	struct mtk_cam_request *req = mtk_cam_req_work_to_req(work);
	struct mtk_cam_req_work *frame_done_work = (struct mtk_cam_req_work *)work;
	struct mtk_cam_request_stream_data *req_stream_data;

	req_stream_data = &req->stream_data[frame_done_work->pipe_id];
	mtk_cam_handle_frame_done(frame_done_work->ctx,
				  req_stream_data->frame_seq_no,
				  frame_done_work->pipe_id);
}

void mtk_camsys_frame_done(struct mtk_cam_ctx *ctx,
				  unsigned int frame_seq_no,
				  unsigned int pipe_id)
{
	struct mtk_cam_request *req;
	struct mtk_cam_req_work *frame_done_work;
	struct mtk_camsys_sensor_ctrl *camsys_sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_raw_device *raw_dev;
	struct device *dev_sv;
	struct mtk_camsv_device *camsv_dev;
	int type, sv_main_id, i;

	if (mtk_cam_is_stagger(ctx)) {
		req = mtk_cam_dev_get_req(ctx->cam, ctx, frame_seq_no + 1);
		if (req) {
			req_stream_data = &req->stream_data[pipe_id];
			type = req_stream_data->feature.switch_feature_type;
			if (type != 0) {
				raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
				sv_main_id = get_main_sv_pipe_id(ctx->cam, ctx->pipe->enabled_raw);
				dev_sv = ctx->cam->sv.devs[sv_main_id - MTKCAM_PIPE_CAMSV_0];
				camsv_dev = dev_get_drvdata(dev_sv);
				enable_tg_db(raw_dev, 0);
				mtk_cam_sv_toggle_tg_db(camsv_dev);
				enable_tg_db(raw_dev, 1);
				toggle_db(raw_dev);
				dev_dbg(ctx->cam->dev,
					"[SWD-switch req+1 check] req:%d type:%d\n",
					req_stream_data->frame_seq_no, type);
			}
		}
	}
	req = mtk_cam_dev_get_req(ctx->cam, ctx, frame_seq_no);
	if (!req) {
		dev_info(ctx->cam->dev, "%s:ctx-%d:pipe-%d:req(%d) not found!\n",
			 __func__, ctx->stream_id, pipe_id, frame_seq_no);
		return;
	}
	/* Initial request readout will be delayed 1 frame*/
	if (ctx->sensor) {
		if (camsys_sensor_ctrl->isp_request_seq_no == 0 &&
			(ctx->pipe->res_config.raw_feature == 0)) {
			dev_info(ctx->cam->dev,
					"1st SWD passed for initial request setting\n");
			camsys_sensor_ctrl->initial_drop_frame_cnt--;
			return;
		}
	}
	req_stream_data = &req->stream_data[ctx->stream_id];
	if (req_stream_data->frame_done_queue_work) {
		dev_info(ctx->cam->dev,
			"already queue done work %d\n", req_stream_data->frame_seq_no);
		return;
	}
	req_stream_data->frame_done_queue_work = 1;
	frame_done_work = &req->stream_data[pipe_id].frame_done_work;
	/* To be removed after passing camsv UT */
	frame_done_work->pipe_id = pipe_id;
	frame_done_work->ctx = ctx;
	queue_work(ctx->frame_done_wq, &frame_done_work->work);

	if (mtk_cam_is_time_shared(ctx)) {
		raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
		raw_dev->time_shared_busy = false;
		/*try set other ctx in one request first*/
		if (req->pipe_used != (1 << ctx->stream_id)) {
			struct mtk_cam_ctx *ctx_2;
			int pipe_used_remain = req->pipe_used & (~(1 << ctx->stream_id));

			for (i = 0;  i < ctx->cam->max_stream_num; i++)
				if (pipe_used_remain == (1 << i)) {
					ctx_2 = &ctx->cam->ctxs[i];
					break;
				}
			dev_dbg(raw_dev->dev, "%s: time sharing ctx-%d deq_no(%d)\n",
			 __func__, ctx_2->stream_id, ctx_2->dequeued_frame_seq_no+1);
			mtk_camsys_ts_raw_try_set(raw_dev, ctx_2, ctx_2->dequeued_frame_seq_no + 1);
		}
		mtk_camsys_ts_raw_try_set(raw_dev, ctx, ctx->dequeued_frame_seq_no + 1);
	}

}


void mtk_camsys_state_delete(struct mtk_cam_ctx *ctx,
				struct mtk_camsys_sensor_ctrl *sensor_ctrl,
				struct mtk_cam_request *req)
{
	struct mtk_camsys_ctrl_state *state_entry, *state_entry_prev;
	int state_found = 0;

	if (ctx->sensor) {
		spin_lock(&sensor_ctrl->camsys_state_lock);
		list_for_each_entry_safe(state_entry, state_entry_prev,
				&sensor_ctrl->camsys_state_list,
				state_element) {
			struct mtk_camsys_ctrl_state *req_state =
				&req->stream_data[ctx->stream_id].state;
			if (state_entry == req_state) {
				list_del(&state_entry->state_element);
				state_found = 1;
			}
		}
		spin_unlock(&sensor_ctrl->camsys_state_lock);
		if (state_found == 0)
			dev_dbg(ctx->cam->dev, "state not found\n");
	}
}

static int mtk_camsys_camsv_state_handle(
		struct mtk_camsv_device *camsv_dev,
		struct mtk_camsys_sensor_ctrl *sensor_ctrl,
		struct mtk_camsys_ctrl_state **current_state,
		int frame_inner_idx)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_camsys_ctrl_state *state_temp, *state_outer = NULL;
	struct mtk_camsys_ctrl_state *state_rec[STATE_NUM_AT_SOF];
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	int stateidx;
	int que_cnt = 0;
	unsigned long flags;

	/* List state-queue status*/
	spin_lock_irqsave(&sensor_ctrl->camsys_state_lock, flags);
	list_for_each_entry(state_temp,
	&sensor_ctrl->camsys_state_list, state_element) {
		req = state_temp->req;
		req_stream_data = &req->stream_data[ctx->stream_id];
		stateidx = sensor_ctrl->sensor_request_seq_no -
			   req_stream_data->frame_seq_no;

		if (stateidx < STATE_NUM_AT_SOF && stateidx > -1) {
			state_rec[stateidx] = state_temp;
			/* Find outer state element */
			if (state_temp->estate == E_STATE_OUTER ||
				state_temp->estate == E_STATE_OUTER_HW_DELAY)
				state_outer = state_temp;
			dev_dbg(camsv_dev->dev,
				"[SOF] STATE_CHECK [N-%d] Req:%d / State:%d\n",
				stateidx, req_stream_data->frame_seq_no,
				state_rec[stateidx]->estate);
		}
		/* counter for state queue*/
		que_cnt++;
	}
	spin_unlock_irqrestore(&sensor_ctrl->camsys_state_lock, flags);

	/* HW imcomplete case */
	if (que_cnt >= STATE_NUM_AT_SOF) {
		state_transition(state_rec[2], E_STATE_INNER, E_STATE_INNER_HW_DELAY);
		state_transition(state_rec[1], E_STATE_OUTER, E_STATE_OUTER_HW_DELAY);
		dev_dbg(camsv_dev->dev, "[SOF] HW_DELAY state\n");
		return STATE_RESULT_PASS_CQ_HW_DELAY;
	}

	/* Trigger high resolution timer to try sensor setting */
	mtk_cam_sof_timer_setup(ctx);

	/* Transit outer state to inner state */
	if (state_outer != NULL) {
		req = state_outer->req;
		req_stream_data = &req->stream_data[ctx->stream_id];
		if (req_stream_data->frame_seq_no == frame_inner_idx) {
			if (frame_inner_idx == (sensor_ctrl->isp_request_seq_no + 1)) {
				state_transition(state_outer,
					E_STATE_OUTER_HW_DELAY, E_STATE_INNER_HW_DELAY);
				state_transition(state_outer, E_STATE_OUTER, E_STATE_INNER);
				sensor_ctrl->isp_request_seq_no = frame_inner_idx;
				dev_dbg(camsv_dev->dev, "[SOF-DBLOAD] req:%d, OUTER->INNER state:%d\n",
						req_stream_data->frame_seq_no, state_outer->estate);
			}
		}
	}

	if (que_cnt > 0) {
		/* CQ triggering judgment*/
		if (state_rec[0]->estate == E_STATE_SENSOR) {
			*current_state = state_rec[0];
			return STATE_RESULT_TRIGGER_CQ;
		}
	}

	return STATE_RESULT_PASS_CQ_SW_DELAY;
}

static void mtk_camsys_camsv_frame_start(struct mtk_camsv_device *camsv_dev,
	struct mtk_cam_ctx *ctx, unsigned int dequeued_frame_seq_no)
{
	struct mtk_cam_device *cam = camsv_dev->cam;
	struct mtk_cam_request *req;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_camsys_ctrl_state *current_state;
	enum MTK_CAMSYS_STATE_RESULT state_handle_ret;
	int sv_dev_index;

	/* inner register dequeue number */
	sv_dev_index = mtk_cam_find_sv_dev_index(ctx, camsv_dev->id);
	if (sv_dev_index == -1) {
		dev_dbg(camsv_dev->dev, "cannot find sv_dev_index(%d)", camsv_dev->id);
		return;
	}
	ctx->sv_dequeued_frame_seq_no[sv_dev_index] = dequeued_frame_seq_no;
	/* Send V4L2_EVENT_FRAME_SYNC event */
	mtk_cam_sv_event_frame_sync(camsv_dev, dequeued_frame_seq_no);

	if (ctx->sensor &&
		(ctx->stream_id >= MTKCAM_SUBDEV_CAMSV_START &&
		ctx->stream_id < MTKCAM_SUBDEV_CAMSV_END)) {
		state_handle_ret = mtk_camsys_camsv_state_handle(camsv_dev, sensor_ctrl,
				&current_state, dequeued_frame_seq_no);
		if (state_handle_ret != STATE_RESULT_TRIGGER_CQ)
			return;
	}

	/* Find request of this dequeued frame */
	if (ctx->stream_id >= MTKCAM_SUBDEV_CAMSV_START &&
		ctx->stream_id < MTKCAM_SUBDEV_CAMSV_END) {
		req = mtk_cam_dev_get_req(cam, ctx, dequeued_frame_seq_no);
		if (req) {
			req->stream_data[ctx->stream_id].timestamp = ktime_get_boottime_ns();
			req->stream_data[ctx->stream_id].timestamp_mono = ktime_get_ns();
		}
	}

	/* apply next buffer */
	if (ctx->stream_id >= MTKCAM_SUBDEV_CAMSV_START &&
		ctx->stream_id < MTKCAM_SUBDEV_CAMSV_END) {
		if (mtk_cam_sv_apply_next_buffer(ctx)) {
			/* Transit state from Sensor -> Outer */
			if (ctx->sensor)
				state_transition(current_state, E_STATE_SENSOR, E_STATE_OUTER);
		}
	}
}

int mtk_camsys_isr_event(struct mtk_cam_device *cam,
			 struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_raw_device *raw_dev;
	struct mtk_camsv_device *camsv_dev;
	struct mtk_cam_ctx *ctx;
	int sub_engine_type = irq_info->engine_id & MTK_CAMSYS_ENGINE_IDXMASK;
	int ret = 0;
	int sv_dev_index;
	unsigned int stream_id;
	unsigned int seq;

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
		if (raw_dev->pipeline->res_config.raw_feature & MTK_CAM_FEATURE_TIMESHARE_MASK)
			ctx = &cam->ctxs[raw_dev->time_shared_busy_ctx_id];
		else
			ctx = mtk_cam_find_ctx(cam, &raw_dev->pipeline->subdev.entity);
		if (!ctx) {
			dev_dbg(raw_dev->dev, "cannot find ctx\n");
			ret = -EINVAL;
			break;
		}
		/* Twin only handle cq done case, sw done and sof will not be handled */
		if (raw_dev->pipeline->res_config.raw_num_used == 2) {
			/* twin - master/slave's CQ done */
			if (irq_info->irq_type & (1 << CAMSYS_IRQ_SETTING_DONE)) {
				struct mtk_raw_device *raw_dev_master =
					get_master_raw_dev(cam, raw_dev->pipeline);
				struct mtk_raw_device *raw_dev_slave =
					get_slave_raw_dev(cam, raw_dev->pipeline);
				dev_dbg(raw_dev->dev, "[twin-cq] cnt m=%d/s=%d\n",
					raw_dev_master->setting_count,
					raw_dev_slave->setting_count);
				if (raw_dev_master->setting_count == raw_dev_slave->setting_count)
					raw_dev = raw_dev_master;
				else
					return ret;
			}
			/* twin - slave's SOF and SW done */
			if (irq_info->slave_engine) {
				if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_DONE))
					return ret;
				if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_START))
					return ret;
			}
		} else if (raw_dev->pipeline->res_config.raw_num_used == 3) {
			/* triplet - master/slave/slave2's CQ done */
			if (irq_info->irq_type & (1 << CAMSYS_IRQ_SETTING_DONE)) {
				struct mtk_raw_device *raw_dev_master =
					get_master_raw_dev(cam, raw_dev->pipeline);
				struct mtk_raw_device *raw_dev_slave =
					get_slave_raw_dev(cam, raw_dev->pipeline);
				struct mtk_raw_device *raw_dev_slave2 =
					get_slave2_raw_dev(cam, raw_dev->pipeline);
				dev_dbg(raw_dev->dev, "[triplet-cq] cnt m=%d/s=%d/s2=%d\n",
					raw_dev_master->setting_count,
					raw_dev_slave->setting_count,
					raw_dev_slave2->setting_count);
				if ((raw_dev_master->setting_count ==
					raw_dev_slave->setting_count)
					&& (raw_dev_master->setting_count ==
					raw_dev_slave2->setting_count))
					raw_dev = raw_dev_master;
				else
					return ret;
			}
			/* triplet - slave's SOF and SW done */
			if (irq_info->slave_engine) {
				if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_DONE))
					return ret;
				if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_START))
					return ret;
			}
		}
		/* raw's CQ done */
		if (irq_info->irq_type & (1 << CAMSYS_IRQ_SETTING_DONE)) {
			if (ctx->pipe->res_config.raw_feature & MTK_CAM_FEATURE_STAGGER_M2M_MASK) {
				mtk_camsys_raw_m2m_cq_done(raw_dev, ctx, irq_info->frame_idx);
				mtk_camsys_raw_m2m_trigger(raw_dev, ctx, irq_info->frame_idx);
			} else
				mtk_camsys_raw_cq_done(raw_dev, ctx, irq_info->frame_idx);
		}
		/* raw's subsample sensor setting */
		if (irq_info->irq_type & (1 << CAMSYS_IRQ_SUBSAMPLE_SENSOR_SET))
			mtk_cam_set_sub_sample_sensor(raw_dev, ctx);

		/* raw's SW done */
		if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_DONE)) {
			if (ctx->pipe->res_config.raw_feature & MTK_CAM_FEATURE_STAGGER_M2M_MASK) {
				mtk_camsys_raw_m2m_frame_done(raw_dev, ctx,
						   irq_info->frame_inner_idx);
			} else
				mtk_camsys_frame_done(ctx, ctx->dequeued_frame_seq_no,
					ctx->stream_id);
		}
		/* raw's SOF */
		if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_START)) {
			if (mtk_cam_is_stagger(ctx)) {
				dev_dbg(raw_dev->dev, "[stagger] last frame_start\n");
				mtk_cam_hdr_last_frame_start(raw_dev, ctx,
					irq_info->frame_inner_idx);
			} else {
				mtk_camsys_raw_frame_start(raw_dev, ctx,
						  irq_info->frame_inner_idx);
			}
		}
		break;
	case MTK_CAMSYS_ENGINE_MRAW_TAG:
		/* struct mtk_mraw_device *mraw_dev; */
		break;
	case MTK_CAMSYS_ENGINE_CAMSV_TAG:
		camsv_dev = cam->camsys_ctrl.camsv_dev[irq_info->engine_id -
			CAMSYS_ENGINE_CAMSV_BEGIN];
		if (camsv_dev->pipeline->hw_scen &
			MTK_CAMSV_SUPPORTED_SPECIAL_HW_SCENARIO) {
			// first exposure camsv's SOF
			if (camsv_dev->pipeline->is_first_expo) {
				if (irq_info->irq_type & (1<<CAMSYS_IRQ_FRAME_START)) {
					struct mtk_raw_pipeline *pipeline = &cam->raw
					.pipelines[camsv_dev->pipeline->master_pipe_id];
					struct mtk_raw_device *raw_dev =
						get_master_raw_dev(cam, pipeline);
					struct mtk_cam_ctx *ctx =
						mtk_cam_find_ctx(cam, &pipeline->subdev.entity);

					dev_dbg(camsv_dev->dev, "SOF+raw_frame_start %d/%d/%d\n",
						camsv_dev->pipeline->master_pipe_id,
						raw_dev->id, ctx->stream_id);
					mtk_camsys_raw_frame_start(raw_dev, ctx,
						   irq_info->frame_inner_idx);
				}
			}
			// time sharing - camsv write DRAM mode
			if (camsv_dev->pipeline->hw_scen &
				(1 << MTKCAM_IPI_HW_PATH_OFFLINE_M2M)) {
				if (irq_info->irq_type & (1<<CAMSYS_IRQ_FRAME_DONE)) {
					struct mtk_raw_pipeline *pipeline = &cam->raw
					.pipelines[camsv_dev->pipeline->master_pipe_id];
					struct mtk_cam_ctx *ctx =
						mtk_cam_find_ctx(cam, &pipeline->subdev.entity);
					struct mtk_raw_device *raw_dev =
						get_master_raw_dev(ctx->cam, ctx->pipe);
					dev_info(camsv_dev->dev, "[TS-SV-SWD] ctx:%d, req:%d\n",
						ctx->stream_id, irq_info->frame_inner_idx);
					mtk_camsys_ts_sv_done(ctx, irq_info->frame_inner_idx);
					mtk_camsys_ts_raw_try_set(
						raw_dev, ctx, ctx->dequeued_frame_seq_no + 1);
				}
				if (irq_info->irq_type & (1<<CAMSYS_IRQ_FRAME_START)) {
					struct mtk_raw_pipeline *pipeline = &cam->raw
					.pipelines[camsv_dev->pipeline->master_pipe_id];
					struct mtk_cam_ctx *ctx =
						mtk_cam_find_ctx(cam, &pipeline->subdev.entity);
					mtk_camsys_ts_frame_start(ctx, irq_info->frame_inner_idx);
				}
			}
		} else {
			ctx = mtk_cam_find_ctx(cam, &camsv_dev->pipeline->subdev.entity);
			if (!ctx) {
				dev_dbg(camsv_dev->dev, "cannot find ctx\n");
				ret = -EINVAL;
				break;
			}
			stream_id = irq_info->engine_id - CAMSYS_ENGINE_CAMSV_BEGIN +
				MTKCAM_SUBDEV_CAMSV_START;
			/* camsv's SW done */
			if (irq_info->irq_type & (1<<CAMSYS_IRQ_FRAME_DONE)) {
				sv_dev_index = mtk_cam_find_sv_dev_index(ctx, camsv_dev->id);
				if (sv_dev_index == -1) {
					dev_dbg(camsv_dev->dev,
						"cannot find sv_dev_index(%d)", camsv_dev->id);
					ret = -EINVAL;
					break;
				}
				seq = ctx->sv_dequeued_frame_seq_no[sv_dev_index];
				mtk_camsys_frame_done(ctx, seq, stream_id);
			}
			/* camsv's SOF */
			if (irq_info->irq_type & (1<<CAMSYS_IRQ_FRAME_START))
				mtk_camsys_camsv_frame_start(camsv_dev, ctx,
					irq_info->frame_inner_idx);
		}
		break;
	case MTK_CAMSYS_ENGINE_SENINF_TAG:
		/* ToDo - cam mux setting delay handling */
		if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_DROP))
			dev_info(cam->dev, "MTK_CAMSYS_ENGINE_SENINF_TAG engine:%d type:0x%x\n",
				irq_info->engine_id, irq_info->irq_type);
		break;
	default:
		break;
	}

	return ret;
}

void mtk_cam_initial_sensor_setup(struct mtk_cam_request *initial_req,
				  struct mtk_cam_ctx *ctx)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;

	sensor_ctrl->ctx = ctx;
	initial_req->stream_data[ctx->stream_id].state.time_swirq_timer =
		ktime_get_boottime_ns() / 1000;
	mtk_cam_set_sensor(initial_req, sensor_ctrl);
	if (mtk_cam_is_subsample(ctx))
		state_transition(&initial_req->stream_data[ctx->stream_id].state,
			E_STATE_READY, E_STATE_SUBSPL_READY);
	dev_info(ctx->cam->dev, "Initial sensor timer setup\n");
}
void mtk_cam_req_ctrl_setup(struct mtk_cam_ctx *ctx,
					struct mtk_cam_request *req)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct media_request_object *obj;
	struct v4l2_ctrl_handler *parent_hdl;

	req_stream_data = &req->stream_data[ctx->stream_id];

	/* request setup*/
	list_for_each_entry(obj, &req->req.objects, list) {
		if (likely(obj))
			parent_hdl = obj->priv;
		else
			continue;
		if (parent_hdl == &ctx->pipe->ctrl_handler) {
			dev_dbg(cam->dev, "%s:%s:ctx(%d) raw ctrl set start (req:%d)\n",
				__func__, req->req.debug_str, ctx->stream_id,
				req_stream_data->frame_seq_no);
			v4l2_ctrl_request_setup(&req->req, parent_hdl);
		}
	}
}

static int timer_reqdrained_chk(int fps_ratio, int sub_sample)
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
		timer_ms = timer_ms > 8 ? 8 : timer_ms;

	return timer_ms;
}
static int timer_setsensor(int fps_ratio, int sub_sample)
{
	int timer_ms = 0;

	if (sub_sample > 0) {
		if (fps_ratio > 1)
			timer_ms = SENSOR_SET_RESERVED_MS;
		else
			timer_ms = SENSOR_SET_RESERVED_MS * fps_ratio;
	} else {
		if (fps_ratio > 1)
			timer_ms = SENSOR_SET_RESERVED_MS / fps_ratio;
		else
			timer_ms = SENSOR_SET_RESERVED_MS;
	}
	/* faster sensor setting*/
	if (sub_sample == 0 && fps_ratio > 1)
		timer_ms = timer_ms > 3 ? 3 : timer_ms;

	return timer_ms;
}

int mtk_camsys_ctrl_start(struct mtk_cam_ctx *ctx)
{
	struct mtk_camsys_ctrl *camsys_ctrl = &ctx->cam->camsys_ctrl;
	struct mtk_camsys_sensor_ctrl *camsys_sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_raw_device *raw_dev, *raw_dev_slave, *raw_dev_slave2;
	struct v4l2_subdev_frame_interval fi;
	unsigned int i;
	int fps_factor = 1, sub_ratio = 0;

	if (ctx->used_raw_num) {
		fi.pad = 1;
		fi.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		v4l2_subdev_call(ctx->sensor, video, g_frame_interval, &fi);
		fps_factor = fi.interval.denominator / fi.interval.numerator / 30;
		raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
		camsys_ctrl->raw_dev[raw_dev->id] = raw_dev;
		if (ctx->pipe->res_config.raw_num_used != 1) {
			raw_dev_slave = get_slave_raw_dev(ctx->cam, ctx->pipe);
			camsys_ctrl->raw_dev[raw_dev_slave->id] = raw_dev_slave;
			if (ctx->pipe->res_config.raw_num_used == 3) {
				raw_dev_slave2 = get_slave2_raw_dev(ctx->cam, ctx->pipe);
				camsys_ctrl->raw_dev[raw_dev_slave2->id] = raw_dev_slave2;
			}
		}
		sub_ratio =
			mtk_cam_get_subsample_ratio(ctx->pipe->res_config.raw_feature);
	}
	for (i = 0; i < ctx->used_sv_num; i++) {
		camsys_ctrl->camsv_dev[ctx->sv_pipe[i]->id -
			MTKCAM_SUBDEV_CAMSV_START] =
			dev_get_drvdata(ctx->cam->sv.devs[ctx->sv_pipe[i]->id -
			MTKCAM_SUBDEV_CAMSV_START]);
	}
	camsys_sensor_ctrl->ctx = ctx;
	camsys_sensor_ctrl->sensor_request_seq_no = 0;
	camsys_sensor_ctrl->isp_request_seq_no = 0;
	camsys_sensor_ctrl->initial_cq_done = 0;
	camsys_sensor_ctrl->sof_time = 0;
	if (ctx->pipe->res_config.raw_feature == 0)
		camsys_sensor_ctrl->initial_drop_frame_cnt = INITIAL_DROP_FRAME_CNT;
	else
		camsys_sensor_ctrl->initial_drop_frame_cnt = 0;
	camsys_sensor_ctrl->timer_req_event =
		timer_reqdrained_chk(fps_factor, sub_ratio);
	camsys_sensor_ctrl->timer_req_sensor =
		timer_setsensor(fps_factor, sub_ratio);
	INIT_LIST_HEAD(&camsys_sensor_ctrl->camsys_state_list);
	spin_lock_init(&camsys_sensor_ctrl->camsys_state_lock);
	if (ctx->sensor) {
		hrtimer_init(&camsys_sensor_ctrl->sensor_deadline_timer,
			     CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		camsys_sensor_ctrl->sensor_deadline_timer.function =
			sensor_deadline_timer_handler;
		camsys_sensor_ctrl->sensorsetting_wq =
			alloc_ordered_workqueue(dev_name(ctx->cam->dev),
						WQ_HIGHPRI | WQ_FREEZABLE);
		if (!camsys_sensor_ctrl->sensorsetting_wq) {
			dev_dbg(ctx->cam->dev,
				"failed to alloc sensor setting workqueue\n");
			return -ENOMEM;
		}
	}

	dev_info(ctx->cam->dev, "[%s] ctx:%d/raw_dev:0x%x drained/sensor (%d)%d/%d\n",
		__func__, ctx->stream_id, ctx->used_raw_dev, fps_factor,
		camsys_sensor_ctrl->timer_req_event, camsys_sensor_ctrl->timer_req_sensor);

	return 0;
}

void mtk_camsys_ctrl_stop(struct mtk_cam_ctx *ctx)
{
	struct mtk_camsys_sensor_ctrl *camsys_sensor_ctrl = &ctx->sensor_ctrl;
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
	if (ctx->used_raw_num)
		mtk_cam_event_eos(ctx->pipe);
	dev_info(ctx->cam->dev, "[%s] ctx:%d/raw_dev:0x%x\n",
		__func__, ctx->stream_id, ctx->used_raw_dev);
}

void mtk_cam_m2m_enter_cq_state(struct mtk_camsys_ctrl_state *ctrl_state)
{
	state_transition(ctrl_state, E_STATE_SENSOR, E_STATE_CQ);
}
