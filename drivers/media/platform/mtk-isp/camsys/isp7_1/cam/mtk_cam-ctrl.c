// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/of.h>

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
#include "mtk_cam-raw_debug.h"
#include "mtk_cam-sv-regs.h"
#include "mtk_cam-mraw-regs.h"
#include "mtk_cam-tg-flash.h"
#include "mtk_camera-v4l2-controls.h"
#include "mtk_camera-videodev2.h"
#include "mtk_cam-trace.h"

#include "imgsys/mtk_imgsys-cmdq-ext.h"

#include "frame_sync_camsys.h"

#define SENSOR_SET_DEADLINE_MS  18
#define SENSOR_SET_RESERVED_MS  7
#define SENSOR_SET_DEADLINE_MS_60FPS  6
#define SENSOR_SET_RESERVED_MS_60FPS  6
#define SENSOR_SET_STAGGER_DEADLINE_MS  23
#define SENSOR_SET_STAGGER_RESERVED_MS  6


#define STATE_NUM_AT_SOF 3
#define INITIAL_DROP_FRAME_CNT 1
#define STAGGER_SEAMLESS_DBLOAD_FORCE 1

enum MTK_CAMSYS_STATE_RESULT {
	STATE_RESULT_TRIGGER_CQ = 0,
	STATE_RESULT_PASS_CQ_INIT,
	STATE_RESULT_PASS_CQ_SW_DELAY,
	STATE_RESULT_PASS_CQ_SCQ_DELAY,
	STATE_RESULT_PASS_CQ_HW_DELAY,
};


#define v4l2_set_frame_interval_which(x, y) (x.reserved[0] = y)

void state_transition(struct mtk_camsys_ctrl_state *state_entry,
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
	MTK_CAM_TRACE(BASIC, "raw drained event id:%d",
		pipeline->id);
	v4l2_event_queue(pipeline->subdev.devnode, &event);
}

static void mtk_cam_sv_event_request_drained(struct mtk_camsv_device *camsv_dev)
{
	struct v4l2_event event = {
		.type = V4L2_EVENT_REQUEST_DRAINED,
	};
	MTK_CAM_TRACE(BASIC, "sv drained event id:%d",
		camsv_dev->id);
	v4l2_event_queue(camsv_dev->pipeline->subdev.devnode, &event);
}

static bool mtk_cam_request_drained(struct mtk_camsys_sensor_ctrl *sensor_ctrl)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	int sensor_seq_no_next =
			atomic_read(&sensor_ctrl->sensor_request_seq_no) + 1;
	int res = 0;

	if (mtk_cam_is_subsample(ctx))
		sensor_seq_no_next = atomic_read(&ctx->sensor_ctrl.isp_enq_seq_no) + 1;

	if (mtk_cam_is_mstream(ctx))
		sensor_seq_no_next = atomic_read(&ctx->sensor_ctrl.isp_enq_seq_no) + 1;
	if (sensor_seq_no_next <= atomic_read(&ctx->enqueued_frame_seq_no))
		res = 1;
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
	int sensor_seq_no_next =
			atomic_read(&sensor_ctrl->sensor_request_seq_no) + 1;
	int res = 0;

	if (mtk_cam_is_subsample(ctx))
		sensor_seq_no_next = atomic_read(&ctx->sensor_ctrl.isp_enq_seq_no) + 1;

	if (sensor_seq_no_next <= atomic_read(&ctx->enqueued_frame_seq_no))
		res = 1;
	/* Send V4L2_EVENT_REQUEST_DRAINED event */
	if (res == 0) {
		mtk_cam_sv_event_request_drained(camsv_dev);
		dev_dbg(camsv_dev->dev, "request_drained:(%d)\n", sensor_seq_no_next);
	}
}

static bool mtk_cam_req_frame_sync_set(struct mtk_cam_request *req,
				       unsigned int pipe_id, int value)
{
	struct mtk_cam_device *cam =
		container_of(req->req.mdev, struct mtk_cam_device, media_dev);
	struct mtk_cam_request_stream_data *s_data;
	struct v4l2_ctrl *ctrl;
	struct v4l2_ctrl_handler *hdl;

	s_data = mtk_cam_req_get_s_data(req, pipe_id, 0);
	if (!s_data) {
		dev_info(cam->dev,
			 "%s:%s: get s_data(%d) failed\n",
			 __func__, req->req.debug_str, pipe_id);
		return false;
	}

	/* Use V4L2_CID_FRAME_SYNC to group sensors to be frame sync. */
	if (!s_data->sensor_hdl_obj) {
		dev_info(cam->dev,
			 "%s:%s: pipe(%d): get sensor_hdl_obj failed\n",
			 __func__, req->req.debug_str, pipe_id);
		return false;
	}
	hdl = (struct v4l2_ctrl_handler *)s_data->sensor_hdl_obj->priv;

	ctrl = v4l2_ctrl_find(hdl, V4L2_CID_FRAME_SYNC);
	if (ctrl) {
		v4l2_ctrl_s_ctrl(ctrl, value);
		dev_info(cam->dev,
			 "%s:%s: pipe(%d): apply V4L2_CID_FRAME_SYNC(%d)\n",
			 __func__, req->req.debug_str, pipe_id, value);
		return true;
	}
	dev_info(cam->dev,
		 "%s:%s: pipe(%d): failed to find V4L2_CID_FRAME_SYNC\n",
		 __func__, req->req.debug_str, pipe_id);

	return false;
}

static bool mtk_cam_req_frame_sync_start(struct mtk_cam_request *req)
{
	/* All ctx with sensor is in ready state */
	struct mtk_cam_device *cam =
		container_of(req->req.mdev, struct mtk_cam_device, media_dev);
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_ctx *sync_ctx[MTKCAM_SUBDEV_MAX];
	unsigned int pipe_id;
	int i, ctx_cnt = 0, synced_cnt = 0;
	bool ret = false;

	/* pick out the used ctxs */
	for (i = 0; i < cam->max_stream_num; i++) {
		if (!(1 << i & req->ctx_used))
			continue;

		sync_ctx[ctx_cnt] = &cam->ctxs[i];
		ctx_cnt++;
	}

	mutex_lock(&req->fs.op_lock);
	if (ctx_cnt > 1) {  /* multi sensor case */
		req->fs.on_cnt++;
		if (req->fs.on_cnt != 1)  /* not first time */
			goto EXIT;

		for (i = 0; i < ctx_cnt; i++) {
			ctx = sync_ctx[i];
			spin_lock(&ctx->streaming_lock);
			if (!ctx->streaming) {
				spin_unlock(&ctx->streaming_lock);
				dev_info(cam->dev,
					 "%s: ctx(%d): is streamed off\n",
					 __func__, ctx->stream_id);
				continue;
			}
			pipe_id = ctx->stream_id;
			spin_unlock(&ctx->streaming_lock);

			/* update sensor frame sync */
			if (!ctx->synced) {
				if (mtk_cam_req_frame_sync_set(req, pipe_id, 1))
					ctx->synced = 1;
			}
			/* TODO: user fs */

			if (ctx->synced)
				synced_cnt++;
		}

		/* the prepared sensor is no enough, skip */
		/* frame sync set failed or stream off */
		if (synced_cnt < 2) {
			mtk_cam_fs_reset(&req->fs);
			dev_info(cam->dev, "%s:%s: sensor is not ready\n",
				 __func__, req->req.debug_str);
			goto EXIT;
		}

		dev_dbg(cam->dev, "%s:%s:fs_sync_frame(1): ctxs: 0x%x\n",
			__func__, req->req.debug_str, req->ctx_used);

		fs_sync_frame(1);

		ret = true;
		goto EXIT;

	} else if (ctx_cnt == 1) {  /* single sensor case */
		ctx = sync_ctx[0];
		spin_lock(&ctx->streaming_lock);
		if (!ctx->streaming) {
			spin_unlock(&ctx->streaming_lock);
			dev_info(cam->dev,
				 "%s: ctx(%d): is streamed off\n",
				 __func__, ctx->stream_id);
			goto EXIT;
		}
		pipe_id = ctx->stream_id;
		spin_unlock(&ctx->streaming_lock);

		if (ctx->synced) {
			if (mtk_cam_req_frame_sync_set(req, pipe_id, 0))
				ctx->synced = 0;
		}
	}
EXIT:
	dev_dbg(cam->dev, "%s:%s:target/on/off(%d/%d/%d)\n", __func__,
		req->req.debug_str, req->fs.target, req->fs.on_cnt,
		req->fs.off_cnt);
	mutex_unlock(&req->fs.op_lock);
	return ret;
}

static bool mtk_cam_req_frame_sync_end(struct mtk_cam_request *req)
{
	/* All ctx with sensor is not in ready state */
	struct mtk_cam_device *cam =
		container_of(req->req.mdev, struct mtk_cam_device, media_dev);
	bool ret = false;

	mutex_lock(&req->fs.op_lock);
	if (req->fs.target && req->fs.on_cnt) { /* check fs on */
		req->fs.off_cnt++;
		if (req->fs.on_cnt != req->fs.target ||
		    req->fs.off_cnt != req->fs.target) { /* not the last */
			goto EXIT;
		}
		dev_dbg(cam->dev,
			 "%s:%s:fs_sync_frame(0): ctxs: 0x%x\n",
			 __func__, req->req.debug_str, req->ctx_used);

		fs_sync_frame(0);

		ret = true;
		goto EXIT;
	}
EXIT:
	dev_dbg(cam->dev, "%s:%s:target/on/off(%d/%d/%d)\n", __func__,
		req->req.debug_str, req->fs.target, req->fs.on_cnt,
		req->fs.off_cnt);
	mutex_unlock(&req->fs.op_lock);
	return ret;
}

struct mtk_camsv_device *get_hdr_sv_dev(struct mtk_cam_ctx *ctx, int exp_order)
{
	struct mtk_camsv_device *camsv_dev;
	struct device *dev_sv;
	int i;

	for (i = MTKCAM_SUBDEV_CAMSV_END - 1; i >= MTKCAM_SUBDEV_CAMSV_START; i--) {
		if (ctx->pipe->enabled_raw & (1 << i)) {
			dev_sv = ctx->cam->sv.devs[i - MTKCAM_SUBDEV_CAMSV_START];
			if (dev_sv == NULL)
				dev_info(ctx->cam->dev, "[%s] camsv device not found\n", __func__);
			camsv_dev = dev_get_drvdata(dev_sv);
			if (camsv_dev->pipeline->exp_order == exp_order)
				return camsv_dev;
		}
	}
	return NULL;
}

static void mtk_cam_stream_on(struct mtk_raw_device *raw_dev,
			struct mtk_cam_ctx *ctx)
{
	struct mtk_camsv_device *camsv_dev;
	struct device *dev_sv;
	unsigned int hw_scen = 0;
	int i;

	if (mtk_cam_is_stagger(ctx))
		hw_scen = mtk_raw_get_hdr_scen_id(ctx);
	else if (mtk_cam_is_with_w_channel(ctx))
		hw_scen = (1 << MTKCAM_SV_SPECIAL_SCENARIO_ADDITIONAL_RAW);

	spin_lock(&ctx->streaming_lock);
	if (ctx->streaming) {
		for (i = 0; i < ctx->used_sv_num; i++)
			mtk_cam_sv_dev_stream_on(ctx, i, 1, 1);
		for (i = 0; i < ctx->used_mraw_num; i++)
			mtk_cam_mraw_dev_stream_on(ctx, i, 1);
		for (i = MTKCAM_SUBDEV_CAMSV_END - 1;
				i >= MTKCAM_SUBDEV_CAMSV_START; i--) {
			if (hw_scen &&
				(ctx->pipe->enabled_raw & (1 << i))) {
				if (ctx->pipe->stagger_path == STAGGER_DCIF) {
					dev_sv = ctx->cam->sv.devs[i - MTKCAM_SUBDEV_CAMSV_START];
					if (dev_sv == NULL)
						dev_info(ctx->cam->dev,
						"[%s] camsv device not found\n", __func__);
					camsv_dev = dev_get_drvdata(dev_sv);
					if (camsv_dev->pipeline->exp_order == 0)
						mtk_cam_sv_dev_stream_on(
						ctx, i - MTKCAM_SUBDEV_CAMSV_START, 1, hw_scen);
				} else {
					mtk_cam_sv_dev_stream_on(
						ctx, i - MTKCAM_SUBDEV_CAMSV_START, 1, hw_scen);
				}
			}
		}
		stream_on(raw_dev, 1);
	}
	spin_unlock(&ctx->streaming_lock);
}

bool is_first_request_sync(struct mtk_cam_ctx *ctx)
{
	if (ctx->used_raw_num != 0) {
		if (ctx->pipe->feature_active == 0 &&
			MTK_CAM_INITIAL_REQ_SYNC && ctx->sensor)
			return true;
	} else { // only for sv running case
		if (MTK_CAM_INITIAL_REQ_SYNC && ctx->sensor)
			return true;
	}

	return false;
}

int mtk_cam_sensor_switch_start_hw(struct mtk_cam_ctx *ctx,
				   struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_req_raw_pipe_data *s_raw_pipe_data;
	struct mtk_cam_resource_config *res_config;
	struct mtk_raw_device *raw_dev;
	struct device *dev;
	int i, j, ret;
	int tgo_pxl_mode;
	int feature_first_req;
	int exp_no;

	s_raw_pipe_data = mtk_cam_s_data_get_raw_pipe_data(s_data);
	if (!s_raw_pipe_data) {
		dev_info(ctx->cam->dev, "%s: failed to get raw_pipe_data (pipe:%d, seq:%d)\n",
			 __func__, s_data->pipe_id, s_data->frame_seq_no);
		return -EINVAL;
	}

	res_config = &s_raw_pipe_data->res_config;
	if (ctx->used_raw_num) {
		tgo_pxl_mode = res_config->tgo_pxl_mode;

	dev = mtk_cam_find_raw_dev(cam, ctx->used_raw_dev);
	if (!dev) {
		dev_info(cam->dev, "streamon raw device not found\n");
		ret = -EINVAL;
		goto fail_switch_stop;
	}

	mtk_camsys_ctrl_update(ctx, 0);

	raw_dev = dev_get_drvdata(dev);

	atomic_set(&raw_dev->vf_en, 1);

	if (mtk_cam_is_mstream(ctx)) {
		struct mtk_cam_request *req = mtk_cam_s_data_get_req(s_data);
		struct mtk_cam_request_stream_data *mstream_s_data;

		mstream_s_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 1);
		state_transition(&mstream_s_data->state,
				E_STATE_SENINF, E_STATE_OUTER);
	} else {
		state_transition(&s_data->state, E_STATE_SENINF, E_STATE_OUTER);
	}

	feature_first_req = s_data->feature.raw_feature;
	/* stagger mode - use sv to output data to DRAM - online mode */
	if (mtk_cam_is_stagger(ctx)) {
		int used_pipes, src_pad_idx;

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
				mtk_cam_call_seninf_set_pixelmode(ctx, s_data->seninf_new,
								  src_pad_idx,
								  tgo_pxl_mode);
				mtk_cam_seninf_set_camtg(s_data->seninf_old,
							 src_pad_idx,
							 0xFF);
				mtk_cam_seninf_set_camtg(s_data->seninf_new, src_pad_idx,
							 cam->sv.pipelines[
							 i - MTKCAM_SUBDEV_CAMSV_START].cammux_id);
				dev_info(cam->dev, "seninf_set_camtg(src_pad:%d/i:%d/camtg:%d)",
					 src_pad_idx, i,
					 cam->sv.pipelines[
						i - MTKCAM_SUBDEV_CAMSV_START].cammux_id);
			}
		}
	} else if (mtk_cam_is_time_shared(ctx)) {
		int used_pipes, src_pad_idx, hw_scen;

		hw_scen = (1 << MTKCAM_IPI_HW_PATH_OFFLINE_M2M);
		src_pad_idx = PAD_SRC_RAW0;
		used_pipes = ctx->pipe->enabled_raw;
		for (i = MTKCAM_SUBDEV_CAMSV_START ; i < MTKCAM_SUBDEV_CAMSV_END ; i++) {
			if (used_pipes & (1 << i)) {
				//HSF control
				if (mtk_cam_is_hsf(ctx)) {
					dev_info(cam->dev, "error: un-support hsf stagger mode\n");
					goto fail_switch_stop;
				}

				mtk_cam_call_seninf_set_pixelmode(ctx,
								  s_data->seninf_new,
								  src_pad_idx,
								  tgo_pxl_mode);
				mtk_cam_seninf_set_camtg(s_data->seninf_old, src_pad_idx, 0xFF);
				mtk_cam_seninf_set_camtg(s_data->seninf_new, src_pad_idx,
							 cam->sv.pipelines[
							 i - MTKCAM_SUBDEV_CAMSV_START].cammux_id);
				ret = mtk_cam_sv_dev_config(ctx,
							    i - MTKCAM_SUBDEV_CAMSV_START, hw_scen,
							    0);
				if (ret)
					goto fail_switch_stop;

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
				mtk_cam_call_seninf_set_pixelmode(ctx, s_data->seninf_new,
								  PAD_SRC_RAW_W0,
								  tgo_pxl_mode);
				mtk_cam_seninf_set_camtg(s_data->seninf_old,
							 PAD_SRC_RAW_W0, 0xFF);
				mtk_cam_seninf_set_camtg(s_data->seninf_new, PAD_SRC_RAW_W0,
							 cam->sv.pipelines[
							 i - MTKCAM_SUBDEV_CAMSV_START].cammux_id);
				dev_info(cam->dev,
					 "seninf_set_camtg(src_pad:%d/i:%d/camtg:%d)",
					 PAD_SRC_RAW_W0, i,
					 cam->sv.pipelines[
						i - MTKCAM_SUBDEV_CAMSV_START].cammux_id);
				ret = mtk_cam_sv_dev_config
					(ctx, i - MTKCAM_SUBDEV_CAMSV_START, hw_scen, 0);
				if (ret)
					goto fail_switch_stop;
				break;
			}
		}
	}

	/*set cam mux camtg and pixel mode*/
	if (mtk_cam_is_stagger(ctx)) {
		if (ctx->pipe->stagger_path == STAGGER_ON_THE_FLY) {
			int seninf_pad;

			if (mtk_cam_feature_is_2_exposure(feature_first_req)) {
				seninf_pad = PAD_SRC_RAW1;
				exp_no = 2;
			} else if (mtk_cam_feature_is_3_exposure(feature_first_req)) {
				seninf_pad = PAD_SRC_RAW2;
				exp_no = 3;
			} else {
				seninf_pad = PAD_SRC_RAW0;
				exp_no = 1;
			}

			/* todo: backend support one pixel mode only */
			mtk_cam_call_seninf_set_pixelmode(ctx,
							  s_data->seninf_new,
							  seninf_pad,
							  tgo_pxl_mode);
//			mtk_cam_seninf_set_camtg(s_data->seninf_old, seninf_pad, 0xFF);
			dev_info(cam->dev,
				 "%s: change camtg(src_pad:%d/camtg:%d) for stagger, exp number:%d\n",
				 __func__, seninf_pad, PipeIDtoTGIDX(raw_dev->id), exp_no);

			mtk_cam_seninf_set_camtg(s_data->seninf_new, seninf_pad,
						 PipeIDtoTGIDX(raw_dev->id));
		}
	} else if (!mtk_cam_is_m2m(ctx) &&
				!mtk_cam_is_time_shared(ctx)) {
		if (mtk_cam_is_hsf(ctx)) {
			//HSF control
			dev_info(cam->dev, "enabled_hsf_raw =%d\n",
				res_config->enable_hsf_raw);
				ret = mtk_cam_hsf_config(ctx, raw_dev->id);
				if (ret != 0) {
					dev_info(cam->dev, "Error:enabled_hsf fail\n");
					goto fail_switch_stop;
				}
			}
	#ifdef USING_HSF_SENSOR
			if (mtk_cam_is_hsf(ctx))
				mtk_cam_seninf_set_secure
					(s_data->seninf_new, 1,
					 ctx->hsf->share_buf->chunk_hsfhandle);
			else
				mtk_cam_seninf_set_secure(s_data->seninf_new, 0, 0);
	#endif
			mtk_cam_call_seninf_set_pixelmode(ctx, s_data->seninf_new,
							  PAD_SRC_RAW0,
							  tgo_pxl_mode);
//			mtk_cam_seninf_set_camtg(s_data->seninf_old,
//						 PAD_SRC_RAW0, 0xFF);

			mtk_cam_seninf_set_camtg(s_data->seninf_new, PAD_SRC_RAW0,
						 PipeIDtoTGIDX(raw_dev->id));
		}
	}

	if (!mtk_cam_is_m2m(ctx)) {
		for (i = 0 ; i < ctx->used_sv_num ; i++) {
			/* use 8-pixel mode as default */
			mtk_cam_call_seninf_set_pixelmode(ctx,
							s_data->seninf_new,
							ctx->sv_pipe[i]->seninf_padidx, 3);
			mtk_cam_seninf_set_camtg(s_data->seninf_old,
						 ctx->sv_pipe[i]->seninf_padidx,
						 0xFF);
			mtk_cam_seninf_set_camtg(s_data->seninf_new,
						 ctx->sv_pipe[i]->seninf_padidx,
						 ctx->sv_pipe[i]->cammux_id);
		}

		for (i = 0 ; i < ctx->used_mraw_num ; i++) {
			/* use 1-pixel mode as default */
			mtk_cam_call_seninf_set_pixelmode(ctx,
							s_data->seninf_new,
							ctx->mraw_pipe[i]->seninf_padidx, 0);
			mtk_cam_seninf_set_camtg(s_data->seninf_old,
						 ctx->mraw_pipe[i]->seninf_padidx,
						 0xFF);
			mtk_cam_seninf_set_camtg(s_data->seninf_new,
						 ctx->mraw_pipe[i]->seninf_padidx,
						 ctx->mraw_pipe[i]->cammux_id);
		}
	} else {
		ctx->processing_buffer_list.cnt = 0;
		ctx->composed_buffer_list.cnt = 0;
		dev_dbg(cam->dev, "[M2M] reset processing_buffer_list.cnt & composed_buffer_list.cnt\n");
	}

	return 0;

fail_switch_stop:
	return ret;
}

void mtk_cam_req_seninf_change(struct mtk_cam_request *req)
{
	struct media_pipeline *m_pipe;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam =
		container_of(req->req.mdev, struct mtk_cam_device, media_dev);
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_raw_device *raw_dev;
	struct mtk_camsv_device *camsv_dev;
	struct mtk_mraw_device *mraw_dev;
	int i, j, stream_id;
	u32 val;

	dev_info(cam->dev, "%s, req->ctx_used:0x%x, req->ctx_link_update:0x%x\n",
		__func__, req->ctx_used, req->ctx_link_update);

	for (i = 0; i < cam->max_stream_num; i++) {
		if (req->ctx_used & (1 << i) && (req->ctx_link_update & (1 << i))) {
			stream_id = i;
			ctx = &cam->ctxs[stream_id];
			raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
			req_stream_data = mtk_cam_req_get_s_data(req, stream_id, 0);

			dev_info(cam->dev, "%s: pipe(%d): switch seninf: %s--> %s\n",
				 __func__, stream_id, req_stream_data->seninf_old->name,
				 req_stream_data->seninf_new->name);

			for (j = 0; j < ctx->used_sv_num; j++) {
				camsv_dev = get_camsv_dev(ctx->cam, ctx->sv_pipe[j]);
				dev_info(cam->dev, "%s: pipe(%d): switch seninf: %s--> %s\n",
					 __func__, ctx->sv_pipe[j]->id,
					 req_stream_data->seninf_old->name,
					 req_stream_data->seninf_new->name);
			}

			for (j = 0; j < ctx->used_mraw_num; j++) {
				mraw_dev = get_mraw_dev(ctx->cam, ctx->mraw_pipe[j]);
				dev_info(cam->dev, "%s: pipe(%d): switch seninf: %s--> %s\n",
					 __func__, ctx->mraw_pipe[j]->id,
					 req_stream_data->seninf_old->name,
					 req_stream_data->seninf_new->name);
			}

			mtk_cam_apply_pending_dev_config(req_stream_data);
			m_pipe = req_stream_data->seninf_new->entity.pipe;
			req_stream_data->seninf_new->entity.stream_count++;
			req_stream_data->seninf_new->entity.pipe =
				req_stream_data->seninf_old->entity.pipe;
			req_stream_data->seninf_old->entity.stream_count--;
			req_stream_data->seninf_old->entity.pipe = m_pipe;

			dev_info(cam->dev,
				 "%s: pipe(%d):seninf(%s):seninf_set_camtg, pad(%d) camtg(%d)",
				 __func__, stream_id, req_stream_data->seninf_old->name,
				 PAD_SRC_RAW0, PipeIDtoTGIDX(raw_dev->id));

			mtk_cam_sensor_switch_start_hw(ctx, req_stream_data);

			dev_info(cam->dev, "%s: pipe(%d): update BW for %s\n",
				 __func__, stream_id, req_stream_data->seninf_new->name);

			mtk_cam_qos_bw_calc(ctx, req_stream_data->raw_dmas, false);
		}
	}

	dev_dbg(cam->dev, "%s: update DVFS\n",	 __func__);
	mtk_cam_dvfs_update_clk(cam);

	for (i = 0; i < cam->max_stream_num; i++) {
		if (req->ctx_used & (1 << i) && req->ctx_link_update & (1 << i)) {
			stream_id = i;
			ctx = &cam->ctxs[stream_id];
			req_stream_data = mtk_cam_req_get_s_data(req, stream_id, 0);
			raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);

			// reset for mstream
			ctx->next_sof_mask_frame_seq_no = 0;

			dev_info(cam->dev, "%s: pipe(%d): Enable VF of raw:%d\n",
				 __func__, stream_id, raw_dev->id);

			/* Enable CMOS */
			dev_info(raw_dev->dev, "enable CMOS\n");
			val = readl(raw_dev->base + REG_TG_SEN_MODE);
			writel(val | TG_SEN_MODE_CMOS_EN, raw_dev->base + REG_TG_SEN_MODE);

			mtk_cam_stream_on(raw_dev, ctx);

			dev_info(raw_dev->dev, "%s: stream off seninf:%s\n",
				 __func__, req_stream_data->seninf_old->name);
			v4l2_subdev_call(req_stream_data->seninf_old, video, s_stream, 0);

			if (ctx->prev_sensor || ctx->prev_seninf) {
				ctx->prev_sensor = NULL;
				ctx->prev_seninf = NULL;
			}
		}
	}
	dev_dbg(cam->dev, "%s: cam mux switch done\n", __func__);
}

static void mtk_cam_m2m_sensor_skip(struct mtk_cam_request_stream_data *req_stream_data)
{
	struct mtk_cam_request *req;
	struct mtk_cam_device *cam;
	struct mtk_cam_ctx *ctx;

	ctx = mtk_cam_s_data_get_ctx(req_stream_data);
	cam = ctx->cam;
	req = mtk_cam_s_data_get_req(req_stream_data);
	dev_dbg(ctx->cam->dev,
		"%s:%s:ctx(%d):sensor ctrl skip frame_seq_no %d\n",
		__func__, req->req.debug_str,
		ctx->stream_id, req_stream_data->frame_seq_no);

	state_transition(&req_stream_data->state,
	E_STATE_READY, E_STATE_SENSOR);

	mtk_cam_complete_sensor_hdl(req_stream_data);

}

static void mtk_cam_set_sensor_mstream_mode(struct mtk_cam_ctx *ctx, bool on)
{
	struct v4l2_ctrl *mstream_mode_ctrl;

	mstream_mode_ctrl = v4l2_ctrl_find(ctx->sensor->ctrl_handler,
			V4L2_CID_MTK_MSTREAM_MODE);

	if (!mstream_mode_ctrl) {
		dev_info(ctx->cam->dev,
			"%s: ctx(%d): no sensor mstream mode control found\n",
			__func__, ctx->stream_id);
		return;
	}

	if (on)
		v4l2_ctrl_s_ctrl(mstream_mode_ctrl, 1);
	else
		v4l2_ctrl_s_ctrl(mstream_mode_ctrl, 0);

	dev_dbg(ctx->cam->dev,
		"%s mstream mode:%d\n", __func__, on);
}

static int mtk_cam_set_sensor_mstream_exposure(struct mtk_cam_ctx *ctx,
		struct mtk_cam_request_stream_data *req_stream_data)
{
	struct mtk_cam_device *cam = ctx->cam;
	int is_mstream_last_exposure = 0;
	struct mtk_cam_request *req = mtk_cam_s_data_get_req(req_stream_data);
	struct mtk_cam_request_stream_data *last_req_stream_data;

	last_req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
	if (req_stream_data->frame_seq_no == last_req_stream_data->frame_seq_no)
		is_mstream_last_exposure = 1;

	if (!ctx->sensor) {
		dev_info(cam->dev, "%s: ctx(%d): no sensor found\n",
			__func__, ctx->stream_id);
	} else {
		struct v4l2_ctrl *ae_ctrl;
		struct mtk_hdr_ae ae;
		u32 shutter, gain;

		/* mstream mode on */
		if (last_req_stream_data->frame_seq_no == 2 || mtk_cam_feature_change_is_mstream(
				last_req_stream_data->feature.switch_feature_type))
			mtk_cam_set_sensor_mstream_mode(ctx, 1);

		ae_ctrl = v4l2_ctrl_find(ctx->sensor->ctrl_handler,
				V4L2_CID_MTK_STAGGER_AE_CTRL);
		if (!ae_ctrl) {
			dev_info(ctx->cam->dev,
				"no stagger ae ctrl id in %s\n",
				ctx->sensor->name);
			return is_mstream_last_exposure;
		}

		shutter = req_stream_data->mtk_cam_exposure.shutter;
		gain = req_stream_data->mtk_cam_exposure.gain;

		dev_dbg(ctx->cam->dev,
			"%s exposure:%d gain:%d\n", __func__, shutter, gain);

		if (shutter > 0 && gain > 0) {
			ae.exposure.le_exposure = shutter;
			ae.gain.le_gain = gain;

			if (!is_mstream_last_exposure)
				ae.subsample_tags = 1;
			else
				ae.subsample_tags = 2;
			v4l2_ctrl_s_ctrl_compound(ae_ctrl, V4L2_CTRL_TYPE_U32,
						&ae);
			dev_dbg(ctx->cam->dev, "mstream sensor ae ctrl done\n");
		}
	}

	return is_mstream_last_exposure;
}

static bool mtk_cam_submit_kwork(struct kthread_worker *worker,
				 struct mtk_cam_sensor_work *sensor_work,
				 kthread_work_func_t func)
{
	if (!worker) {
		pr_info("%s: not queue work since kthread_worker is null\n",
			__func__);

		return false;
	}

	if (atomic_read(&sensor_work->is_queued)) {
		pr_info("%s: not queue work since sensor_work is already queued\n",
			__func__);

		return false;
	}

	/**
	 * TODO: init the work function during request enqueue since
	 * mtk_cam_submit_kwork() is called in interrupt context
	 * now.
	 */
	kthread_init_work(&sensor_work->work, func);

	atomic_set(&sensor_work->is_queued, 1);
	return kthread_queue_work(worker, &sensor_work->work);
}

static void mtk_cam_exp_switch_sensor(
			struct mtk_cam_request_stream_data *req_stream_data)
{
	struct mtk_cam_request *req;
	struct mtk_cam_device *cam;
	struct mtk_cam_ctx *ctx;
	unsigned int time_after_sof = 0;

	ctx = mtk_cam_s_data_get_ctx(req_stream_data);
	cam = ctx->cam;
	req = mtk_cam_s_data_get_req(req_stream_data);
	dev_dbg(cam->dev, "%s:%s:ctx(%d):sensor try set start\n",
		__func__, req->req.debug_str, ctx->stream_id);

	if (mtk_cam_req_frame_sync_start(req))
		dev_dbg(cam->dev, "%s:%s:ctx(%d): sensor ctrl with frame sync - start\n",
			__func__, req->req.debug_str, ctx->stream_id);
	/* request setup*/
	if (req_stream_data->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_EN) {
		v4l2_ctrl_request_setup(&req->req, req_stream_data->sensor->ctrl_handler);
		time_after_sof = ktime_get_boottime_ns() / 1000000 -
					ctx->sensor_ctrl.sof_time;
		dev_dbg(cam->dev, "[SOF+%dms] Sensor request:%d[ctx:%d] setup\n",
			time_after_sof, req_stream_data->frame_seq_no,
			ctx->stream_id);
	}

	state_transition(&req_stream_data->state,
		E_STATE_READY, E_STATE_SENSOR);
	if (mtk_cam_req_frame_sync_end(req))
		dev_dbg(cam->dev, "%s:ctx(%d): sensor ctrl with frame sync - stop\n",
			__func__, ctx->stream_id);

	dev_info(cam->dev, "%s:%s:ctx(%d)req(%d):sensor done at SOF+%dms\n",
		 __func__, req->req.debug_str, ctx->stream_id,
		 req_stream_data->frame_seq_no, time_after_sof);

	mtk_cam_complete_sensor_hdl(req_stream_data);
}


static void mtk_cam_exp_switch_sensor_worker(struct kthread_work *work)
{
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_device *cam;
	struct mtk_cam_ctx *ctx;
	unsigned int time_after_sof = 0;

	req_stream_data = mtk_cam_sensor_work_to_s_data(work);
	ctx = mtk_cam_s_data_get_ctx(req_stream_data);
	cam = ctx->cam;
	req = mtk_cam_s_data_get_req(req_stream_data);
	dev_dbg(cam->dev, "%s:%s:ctx(%d):sensor try set start\n",
		__func__, req->req.debug_str, ctx->stream_id);

	if (mtk_cam_req_frame_sync_start(req))
		dev_dbg(cam->dev, "%s:%s:ctx(%d): sensor ctrl with frame sync - start\n",
			__func__, req->req.debug_str, ctx->stream_id);
	/* request setup*/
	if (req_stream_data->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_EN) {
		v4l2_ctrl_request_setup(&req->req, req_stream_data->sensor->ctrl_handler);
		time_after_sof = ktime_get_boottime_ns() / 1000000 -
					ctx->sensor_ctrl.sof_time;
		dev_dbg(cam->dev, "[SOF+%dms] Sensor request:%d[ctx:%d] setup\n",
			time_after_sof, req_stream_data->frame_seq_no,
			ctx->stream_id);
	}

	state_transition(&req_stream_data->state,
		E_STATE_READY, E_STATE_SENSOR);
	if (mtk_cam_req_frame_sync_end(req))
		dev_dbg(cam->dev, "%s:ctx(%d): sensor ctrl with frame sync - stop\n",
			__func__, ctx->stream_id);

	dev_info(cam->dev, "%s:%s:ctx(%d)req(%d):sensor done at SOF+%dms\n",
		 __func__, req->req.debug_str, ctx->stream_id,
		 req_stream_data->frame_seq_no, time_after_sof);

	mtk_cam_complete_sensor_hdl(req_stream_data);
}

static int mtk_camsys_exp_switch_cam_mux(struct mtk_raw_device *raw_dev,
		struct mtk_cam_ctx *ctx, struct mtk_cam_request_stream_data *req_stream_data)
{
	struct mtk_cam_seninf_mux_param param;
	struct mtk_cam_seninf_mux_setting settings[3];
	int type = req_stream_data->feature.switch_feature_type;
	int sv_main_id, sv_sub_id;
	int config_exposure_num = 3;
	int feature_active;

	/**
	 * To identify the "max" exposure_num, we use
	 * feature_active, not req_stream_data->feature.raw_feature
	 * since the latter one stores the exposure_num information,
	 * not the max one.
	 */
	if (req_stream_data->feature.switch_done == 1)
		return 0;
	feature_active = ctx->pipe->feature_active;
	if (feature_active == STAGGER_2_EXPOSURE_LE_SE ||
	    feature_active == STAGGER_2_EXPOSURE_SE_LE)
		config_exposure_num = 2;

	if (type != EXPOSURE_CHANGE_NONE && config_exposure_num == 3) {
		sv_main_id = get_main_sv_pipe_id(ctx->cam, ctx->pipe->enabled_raw);
		sv_sub_id = get_sub_sv_pipe_id(ctx->cam, ctx->pipe->enabled_raw);
		switch (type) {
		case EXPOSURE_CHANGE_3_to_2:
		case EXPOSURE_CHANGE_1_to_2:
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  =
				ctx->cam->sv.pipelines[
					sv_main_id - MTKCAM_SUBDEV_CAMSV_START].cammux_id;
			settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  = PipeIDtoTGIDX(raw_dev->id);
			settings[1].enable = 1;

			settings[2].seninf = ctx->seninf;
			settings[2].source = PAD_SRC_RAW2;
			settings[2].camtg  =
				ctx->cam->sv.pipelines[
					sv_sub_id - MTKCAM_SUBDEV_CAMSV_START].cammux_id;
			settings[2].enable = 0;
			break;
		case EXPOSURE_CHANGE_3_to_1:
		case EXPOSURE_CHANGE_2_to_1:
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  = PipeIDtoTGIDX(raw_dev->id);
			settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  =
				ctx->cam->sv.pipelines[
					sv_sub_id - MTKCAM_SUBDEV_CAMSV_START].cammux_id;
			settings[1].enable = 0;

			settings[2].seninf = ctx->seninf;
			settings[2].source = PAD_SRC_RAW2;
			settings[2].camtg  =
				ctx->cam->sv.pipelines[
					sv_main_id - MTKCAM_SUBDEV_CAMSV_START].cammux_id;
			settings[2].enable = 0;
			break;
		case EXPOSURE_CHANGE_2_to_3:
		case EXPOSURE_CHANGE_1_to_3:
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  =
				ctx->cam->sv.pipelines[
					sv_main_id - MTKCAM_SUBDEV_CAMSV_START].cammux_id;
			settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  =
				ctx->cam->sv.pipelines[
					sv_sub_id - MTKCAM_SUBDEV_CAMSV_START].cammux_id;
			settings[1].enable = 1;

			settings[2].seninf = ctx->seninf;
			settings[2].source = PAD_SRC_RAW2;
			settings[2].camtg  = PipeIDtoTGIDX(raw_dev->id);
			settings[2].enable = 1;
			break;
		default:
			break;
		}
		param.settings = &settings[0];
		param.num = 3;
		mtk_cam_seninf_streaming_mux_change(&param);
		req_stream_data->feature.switch_done = 1;
		dev_info(ctx->cam->dev,
			"[%s] switch Req:%d, type:%d, cam_mux[0][1][2]:[%d/%d/%d][%d/%d/%d][%d/%d/%d]\n",
			__func__, req_stream_data->frame_seq_no, type,
			settings[0].source, settings[0].camtg, settings[0].enable,
			settings[1].source, settings[1].camtg, settings[1].enable,
			settings[2].source, settings[2].camtg, settings[2].enable);
	} else if (type != EXPOSURE_CHANGE_NONE && config_exposure_num == 2) {
		sv_main_id = get_main_sv_pipe_id(ctx->cam, ctx->pipe->enabled_raw);
		switch (type) {
		case EXPOSURE_CHANGE_2_to_1:
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  = PipeIDtoTGIDX(raw_dev->id);
			settings[0].enable = 1;
			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  =
				ctx->cam->sv.pipelines[
					sv_main_id - MTKCAM_SUBDEV_CAMSV_START].cammux_id;
			settings[1].enable = 0;
			break;
		case EXPOSURE_CHANGE_1_to_2:
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  =
				ctx->cam->sv.pipelines[
					sv_main_id - MTKCAM_SUBDEV_CAMSV_START].cammux_id;
			settings[0].enable = 1;
			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  = PipeIDtoTGIDX(raw_dev->id);
			settings[1].enable = 1;
			break;
		default:
			break;
		}
		param.settings = &settings[0];
		param.num = 2;
		mtk_cam_seninf_streaming_mux_change(&param);
		req_stream_data->feature.switch_done = 1;
		dev_info(ctx->cam->dev,
			"[%s] switch Req:%d, type:%d, cam_mux[0][1]:[%d/%d/%d][%d/%d/%d] ts:%lu\n",
			__func__, req_stream_data->frame_seq_no, type,
			settings[0].source, settings[0].camtg, settings[0].enable,
			settings[1].source, settings[1].camtg, settings[1].enable,
			ktime_get_boottime_ns() / 1000);
	}
	/*switch state*/
	if (type == EXPOSURE_CHANGE_3_to_1 ||
		type == EXPOSURE_CHANGE_2_to_1) {
		state_transition(&req_stream_data->state,
				E_STATE_CQ, E_STATE_CAMMUX_OUTER);
		state_transition(&req_stream_data->state,
				E_STATE_OUTER, E_STATE_CAMMUX_OUTER);
	}

	return 0;
}

static int mtk_cam_exp_sensor_switch(struct mtk_cam_ctx *ctx,
		struct mtk_cam_request_stream_data *req_stream_data)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_raw_device *raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
	int time_after_sof;
	int type = req_stream_data->feature.switch_feature_type;
	if (type == EXPOSURE_CHANGE_2_to_1 || type == EXPOSURE_CHANGE_3_to_1) {
		if (STAGGER_CQ_LAST_SOF == 0)
			mtk_cam_submit_kwork(ctx->sensor_ctrl.sensorsetting_wq,
			     &req_stream_data->sensor_work,
			     mtk_cam_exp_switch_sensor_worker);
	} else if (type == EXPOSURE_CHANGE_1_to_2 || type == EXPOSURE_CHANGE_1_to_3) {
		mtk_cam_exp_switch_sensor(req_stream_data);
		mtk_camsys_exp_switch_cam_mux(raw_dev, ctx, req_stream_data);
	}
	time_after_sof = ktime_get_boottime_ns() / 1000000 -
			   ctx->sensor_ctrl.sof_time;
	dev_dbg(cam->dev,
		"[%s] [SOF+%dms]] ctx:%d, req:%d\n",
		__func__, time_after_sof, ctx->stream_id, req_stream_data->frame_seq_no);

	return 0;
}

static int mtk_cam_hdr_switch_toggle(struct mtk_cam_ctx *ctx, int raw_feature)
{
	struct mtk_raw_device *raw_dev;
	struct mtk_camsv_device *camsv_dev;
	struct mtk_cam_video_device *node;
	struct device *dev_sv;
	int sv_main_id, sv_sub_id;

	if (STAGGER_SEAMLESS_DBLOAD_FORCE)
		return 0;
	node = &ctx->pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
	raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
	sv_main_id = get_main_sv_pipe_id(ctx->cam, ctx->pipe->enabled_raw);
	dev_sv = ctx->cam->sv.devs[sv_main_id - MTKCAM_PIPE_CAMSV_0];
	camsv_dev = dev_get_drvdata(dev_sv);
	enable_tg_db(raw_dev, 0);
	mtk_cam_sv_toggle_tg_db(camsv_dev);
	if (raw_feature == STAGGER_3_EXPOSURE_LE_NE_SE ||
	    raw_feature == STAGGER_3_EXPOSURE_SE_NE_LE) {
		sv_sub_id = get_sub_sv_pipe_id(ctx->cam, ctx->pipe->enabled_raw);
		dev_sv = ctx->cam->sv.devs[sv_sub_id - MTKCAM_PIPE_CAMSV_0];
		camsv_dev = dev_get_drvdata(dev_sv);
		mtk_cam_sv_toggle_tg_db(camsv_dev);
	}
	enable_tg_db(raw_dev, 1);
	toggle_db(raw_dev);

	return 0;
}

void mtk_cam_subspl_req_prepare(struct mtk_camsys_sensor_ctrl *sensor_ctrl)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request_stream_data *req_stream_data;
	int sensor_seq_no_next =
			atomic_read(&ctx->sensor_ctrl.isp_enq_seq_no) + 1;

	req_stream_data = mtk_cam_get_req_s_data(ctx, ctx->stream_id, sensor_seq_no_next);
	if (req_stream_data) {
		if (req_stream_data->state.estate == E_STATE_READY) {
			dev_dbg(cam->dev, "[%s] sensor_no:%d stream_no:%d\n", __func__,
					sensor_seq_no_next, req_stream_data->frame_seq_no);

			/* EnQ this request's state element to state_list (STATE:READY) */
			spin_lock(&sensor_ctrl->camsys_state_lock);
			list_add_tail(&req_stream_data->state.state_element,
					  &sensor_ctrl->camsys_state_list);
			state_transition(&req_stream_data->state,
				E_STATE_READY, E_STATE_SUBSPL_READY);
			spin_unlock(&sensor_ctrl->camsys_state_lock);
		}
	}
}

static inline bool is_sensor_switch(struct mtk_cam_request_stream_data *s_data)
{
	if (s_data->feature.switch_feature_type &&
	    !mtk_cam_feature_change_is_mstream(s_data->feature.switch_feature_type))
		return true;

	if (s_data->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_MODE_UPDATE_T1) {
		state_transition(&s_data->state, E_STATE_READY, E_STATE_SENSOR);
		return true;
	}

	return false;
}

static void
mtk_cam_set_sensor_subspl(struct mtk_cam_request_stream_data *s_data,
		   struct mtk_camsys_sensor_ctrl *sensor_ctrl)
{
	struct mtk_cam_device *cam;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_request *req;
	struct mtk_raw_device *raw_dev = NULL;
	unsigned int time_after_sof = 0;

	/*sensor_worker task*/
	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	req = mtk_cam_s_data_get_req(s_data);

	dev_dbg(cam->dev, "%s:%s:ctx(%d) req(%d):sensor try set start\n",
		__func__, req->req.debug_str, ctx->stream_id, s_data->frame_seq_no);
	atomic_set(&sensor_ctrl->sensor_request_seq_no,
					s_data->frame_seq_no);
	/* request setup*/
	/* 1st frame sensor setting in mstream is treated like normal frame and is set with
	 * other settings like max fps.
	 * 2nd is special, only expsure is set.
	 */
	if (s_data->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_EN) {
		v4l2_ctrl_request_setup(&req->req,
					s_data->sensor->ctrl_handler);
		time_after_sof =
			ktime_get_boottime_ns() / 1000000 - ctx->sensor_ctrl.sof_time;
		dev_dbg(cam->dev,
			"[SOF+%dms] Sensor request:%d[ctx:%d] setup\n",
			time_after_sof, s_data->frame_seq_no,
			ctx->stream_id);
	}
	state_transition(&s_data->state,
		E_STATE_SUBSPL_OUTER, E_STATE_SUBSPL_SENSOR);

	if (ctx->used_raw_num) {
		raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
		if (atomic_read(&raw_dev->vf_en) == 0 &&
			ctx->sensor_ctrl.initial_cq_done == 1 &&
			s_data->frame_seq_no == 1)
			mtk_cam_stream_on(raw_dev, ctx);
	}

	dev_dbg(cam->dev, "%s:%s:ctx(%d)req(%d):sensor done at SOF+%dms\n",
		__func__, req->req.debug_str, ctx->stream_id,
		s_data->frame_seq_no, time_after_sof);

	mtk_cam_tg_flash_req_setup(ctx, s_data);

	mtk_cam_complete_sensor_hdl(s_data);
}

void
mtk_cam_set_sensor_full(struct mtk_cam_request_stream_data *s_data,
		   struct mtk_camsys_sensor_ctrl *sensor_ctrl)
{
	struct mtk_cam_device *cam;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_request *req;
	struct mtk_raw_device *raw_dev = NULL;
	unsigned int time_after_sof = 0;
	int sv_i;
	int is_mstream_last_exposure = 0;

	/* EnQ this request's state element to state_list (STATE:READY) */
	spin_lock(&sensor_ctrl->camsys_state_lock);
	list_add_tail(&s_data->state.state_element,
		      &sensor_ctrl->camsys_state_list);
	atomic_set(&sensor_ctrl->sensor_request_seq_no, s_data->frame_seq_no);
	spin_unlock(&sensor_ctrl->camsys_state_lock);

	if (is_sensor_switch(s_data) && s_data->frame_seq_no > 1) {
		dev_info(sensor_ctrl->ctx->cam->dev,
			 "[TimerIRQ] switch type:%d request:%d - pass sensor\n",
			 s_data->feature.switch_feature_type,
			 s_data->frame_seq_no);
		return;
	}

	if (mtk_cam_is_m2m(sensor_ctrl->ctx)) {
		mtk_cam_m2m_sensor_skip(s_data);
		return;
	}
	/*sensor_worker task*/
	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	req = mtk_cam_s_data_get_req(s_data);

	/* Update ctx->sensor for switch sensor cases */
	if (s_data->seninf_new)
		mtk_cam_update_sensor(ctx, s_data->sensor);

	dev_dbg(cam->dev, "%s:%s:ctx(%d) req(%d):sensor try set start\n",
		__func__, req->req.debug_str, ctx->stream_id, s_data->frame_seq_no);

	if (ctx->used_raw_num) {
		MTK_CAM_TRACE_BEGIN(BASIC, "frame_sync_start");
		if (mtk_cam_req_frame_sync_start(req))
			dev_dbg(cam->dev, "%s:%s:ctx(%d): sensor ctrl with frame sync - start\n",
				__func__, req->req.debug_str, ctx->stream_id);
		MTK_CAM_TRACE_END(BASIC); /* frame_sync_start */
	}

	if (mtk_cam_is_mstream(ctx))
		is_mstream_last_exposure =
			mtk_cam_set_sensor_mstream_exposure(ctx, s_data);

	/* request setup*/
	/* 1st frame sensor setting in mstream is treated like normal frame and is set with
	 * other settings like max fps.
	 * 2nd is special, only expsure is set.
	 */
	if (!mtk_cam_is_m2m(ctx) && !is_mstream_last_exposure) {
		if (s_data->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_EN) {
			v4l2_ctrl_request_setup(&req->req,
						s_data->sensor->ctrl_handler);
			time_after_sof =
				ktime_get_boottime_ns() / 1000000 - ctx->sensor_ctrl.sof_time;
			dev_dbg(cam->dev,
				"[SOF+%dms] Sensor request:%d[ctx:%d] setup\n",
				time_after_sof, s_data->frame_seq_no,
				ctx->stream_id);

			/* update request scheduler timer while sensor fps changes */
			if (s_data->frame_seq_no ==
					atomic_read(&sensor_ctrl->isp_update_timer_seq_no)) {
				int fps_factor = s_data->req->p_data[ctx->stream_id].s_data_num;

				mtk_camsys_ctrl_update(ctx, fps_factor);
				atomic_set(&sensor_ctrl->isp_update_timer_seq_no, 0);
			}
		}
	}

	if (mtk_cam_is_subsample(ctx))
		state_transition(&s_data->state,
		E_STATE_SUBSPL_OUTER, E_STATE_SUBSPL_SENSOR);
	else if (mtk_cam_is_time_shared(ctx))
		state_transition(&s_data->state,
		E_STATE_TS_READY, E_STATE_TS_SENSOR);
	else
		state_transition(&s_data->state,
		E_STATE_READY, E_STATE_SENSOR);

	if (ctx->used_raw_num) {
		MTK_CAM_TRACE_BEGIN(BASIC, "frame_sync_end");
		if (mtk_cam_req_frame_sync_end(req))
			dev_dbg(cam->dev, "%s:ctx(%d): sensor ctrl with frame sync - stop\n",
				__func__, ctx->stream_id);
		MTK_CAM_TRACE_END(BASIC); /* frame_sync_end */
	}

	if (ctx->used_raw_num) {
		raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
		if (atomic_read(&raw_dev->vf_en) == 0 &&
			ctx->sensor_ctrl.initial_cq_done == 1 &&
			s_data->frame_seq_no == 1)
			mtk_cam_stream_on(raw_dev, ctx);
	}

	if (mtk_cam_feature_change_is_mstream(
				s_data->feature.switch_feature_type)) {
		/* mstream mode off */
		if (s_data->feature.switch_feature_type ==
				(EXPOSURE_CHANGE_2_to_1 | MSTREAM_EXPOSURE_CHANGE))
			mtk_cam_set_sensor_mstream_mode(ctx, 0);
	}

	dev_dbg(cam->dev, "%s:%s:ctx(%d)req(%d):sensor done at SOF+%dms\n",
		__func__, req->req.debug_str, ctx->stream_id,
		s_data->frame_seq_no, time_after_sof);

	mtk_cam_tg_flash_req_setup(ctx, s_data);

	mtk_cam_complete_sensor_hdl(s_data);

	/* time sharing sv wdma flow - stream on at 1st request*/
	if (mtk_cam_is_time_shared(ctx) &&
		s_data->frame_seq_no == 1) {
		unsigned int hw_scen =
			(1 << MTKCAM_IPI_HW_PATH_OFFLINE_M2M);
		for (sv_i = MTKCAM_SUBDEV_CAMSV_END - 1;
			sv_i >= MTKCAM_SUBDEV_CAMSV_START; sv_i--)
			if (ctx->pipe->enabled_raw & (1 << sv_i))
				mtk_cam_sv_dev_stream_on(ctx, sv_i - MTKCAM_SUBDEV_CAMSV_START,
							 1, hw_scen);
	}
}

static void mtk_cam_try_set_sensor_subspl(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	int sensor_seq_no_next =
		atomic_read(&sensor_ctrl->sensor_request_seq_no) + 1;

	dev_dbg(ctx->cam->dev, "[%s:check] sensor_no:%d isp_no:%d\n", __func__,
			sensor_seq_no_next, atomic_read(&sensor_ctrl->isp_request_seq_no));
	req_stream_data = mtk_cam_get_req_s_data(ctx,
			ctx->stream_id, sensor_seq_no_next);

	if (req_stream_data && (sensor_seq_no_next > 1)) {
		if (req_stream_data->state.estate == E_STATE_SUBSPL_OUTER) {
			dev_dbg(ctx->cam->dev, "[%s:setup] sensor_no:%d stream_no:%d\n", __func__,
				sensor_seq_no_next, req_stream_data->frame_seq_no);
			mtk_cam_set_sensor_subspl(req_stream_data, &ctx->sensor_ctrl);
		} else if (req_stream_data->state.estate == E_STATE_SUBSPL_SCQ) {
			dev_dbg(ctx->cam->dev, "[%s:setup:SCQ] sensor_no:%d stream_no:%d\n",
				__func__, sensor_seq_no_next, req_stream_data->frame_seq_no);
		}
	}
}

static void mtk_cam_try_set_sensor(struct mtk_cam_ctx *ctx)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_camsys_ctrl_state *state_entry;
	struct mtk_cam_request *req;
	int sensor_seq_no_next =
		atomic_read(&sensor_ctrl->sensor_request_seq_no) + 1;
	int time_after_sof = ktime_get_boottime_ns() / 1000000 -
			   ctx->sensor_ctrl.sof_time;
	/*for 1st unsync, sensor setting will be set at enque thread*/
	if (ctx->used_raw_num) {
		if (MTK_CAM_INITIAL_REQ_SYNC == 0 &&
				ctx->pipe->feature_active == 0 &&
				sensor_seq_no_next <= 2) {
			return;
		}
	} else {
		if (MTK_CAM_INITIAL_REQ_SYNC == 0 &&
				sensor_seq_no_next <= 2) {
			return;
		}
	}
	spin_lock(&sensor_ctrl->camsys_state_lock);
	/* Check if previous state was without cq done */
	list_for_each_entry(state_entry, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_entry);
		if (req_stream_data->frame_seq_no ==
			atomic_read(&sensor_ctrl->sensor_request_seq_no)) {
			if (state_entry->estate == E_STATE_CQ && USINGSCQ &&
			    req_stream_data->frame_seq_no > INITIAL_DROP_FRAME_CNT &&
			    !mtk_cam_is_stagger(ctx)) {
				state_entry->estate = E_STATE_CQ_SCQ_DELAY;
				spin_unlock(&sensor_ctrl->camsys_state_lock);
				dev_dbg(ctx->cam->dev,
					 "[%s] SCQ DELAY STATE at SOF+%dms\n", __func__,
					 time_after_sof);
				return;
			} else if (state_entry->estate == E_STATE_CAMMUX_OUTER_CFG) {
				state_entry->estate = E_STATE_CAMMUX_OUTER_CFG_DELAY;
				dev_dbg(ctx->cam->dev,
					"[%s] CAMMUX OUTTER CFG DELAY STATE\n", __func__);
				spin_unlock(&sensor_ctrl->camsys_state_lock);
				return;

			} else if (state_entry->estate <= E_STATE_SENSOR) {
				spin_unlock(&sensor_ctrl->camsys_state_lock);
				dev_dbg(ctx->cam->dev,
					 "[%s] wrong state:%d (sensor workqueue delay)\n",
					 __func__, state_entry->estate);
				return;
			}
		} else if (req_stream_data->frame_seq_no ==
			atomic_read(&sensor_ctrl->sensor_request_seq_no) - 1) {
			if (state_entry->estate < E_STATE_INNER) {
				spin_unlock(&sensor_ctrl->camsys_state_lock);
				dev_dbg(ctx->cam->dev,
					 "[%s] req:%d isn't arrive inner at SOF+%dms\n",
					 __func__, req_stream_data->frame_seq_no, time_after_sof);
				return;
			}
		}
	}
	spin_unlock(&sensor_ctrl->camsys_state_lock);

	/** Frame sync:
	 * make sure the all ctxs of previous request are triggered
	 */
	req_stream_data = mtk_cam_get_req_s_data(ctx, ctx->stream_id,
						 sensor_seq_no_next - 1);
	if (req_stream_data) {
		req = mtk_cam_s_data_get_req(req_stream_data);
		/* fs complete: fs.target <= off and on == off */
		if (!(req->fs.target <= req->fs.off_cnt &&
		      req->fs.off_cnt == req->fs.on_cnt)) {
			dev_info(ctx->cam->dev,
				 "[TimerIRQ] ctx:%d the fs of req(%s/%d) is not completed, target/on/off(%d/%d/%d)\n",
				 ctx->stream_id, req->req.debug_str,
				 sensor_seq_no_next - 1, req->fs.target,
				 req->fs.on_cnt, req->fs.off_cnt);
			return;
		}
	}

	req_stream_data =  mtk_cam_get_req_s_data(ctx, ctx->stream_id, sensor_seq_no_next);
	if (req_stream_data) {
		req = mtk_cam_s_data_get_req(req_stream_data);
		if (req->ctx_used & (1 << req_stream_data->pipe_id) &&
		    req->ctx_link_update & (1 << req_stream_data->pipe_id)) {
			if (mtk_cam_is_mstream(ctx)) {
				struct mtk_cam_request_stream_data *req_stream_data;

				req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 1);
				if (sensor_seq_no_next == req_stream_data->frame_seq_no) {
					dev_info(ctx->cam->dev,
						"[TimerIRQ] state:%d, mstream sensor switch s_data(1st exp), mtk_cam_set_sensor_full is not triggered by timer\n",
						state_entry->estate);
					return;
				}
			} else {
				dev_info(ctx->cam->dev,
					"[TimerIRQ] state:%d, sensor switch s_data, mtk_cam_set_sensor_full is not triggered by timer\n",
					state_entry->estate);
				return;
			}
		}

		mtk_cam_set_sensor_full(req_stream_data, &ctx->sensor_ctrl);
		dev_dbg(cam->dev,
			"%s:[TimerIRQ [SOF+%dms]:] ctx:%d, sensor_req_seq_no:%d\n",
			__func__, time_after_sof, ctx->stream_id, sensor_seq_no_next);
	} else {
		dev_dbg(cam->dev,
			"%s:[TimerIRQ [SOF+%dms]] ctx:%d, empty req_queue, sensor_req_seq_no:%d\n",
			__func__, time_after_sof, ctx->stream_id,
			atomic_read(&sensor_ctrl->sensor_request_seq_no));
	}
}

static void mtk_cam_sensor_worker_in_sensorctrl(struct kthread_work *work)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl =
			container_of(work, struct mtk_camsys_sensor_ctrl, work);
	struct mtk_cam_ctx *ctx;

	ctx = sensor_ctrl->ctx;
	if (mtk_cam_is_subsample(ctx))
		mtk_cam_try_set_sensor_subspl(ctx);
	else
		mtk_cam_try_set_sensor(ctx);
}

bool mtk_cam_submit_kwork_in_sensorctrl(struct kthread_worker *worker,
				 struct mtk_camsys_sensor_ctrl *sensor_ctrl)
{
	if (!worker) {
		pr_info("%s: not queue work since kthread_worker is null\n",
			__func__);

		return false;
	}

	return kthread_queue_work(worker, &sensor_ctrl->work);
}

static enum hrtimer_restart sensor_set_handler(struct hrtimer *t)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl =
		container_of(t, struct mtk_camsys_sensor_ctrl,
			     sensor_deadline_timer);

	mtk_cam_submit_kwork_in_sensorctrl(sensor_ctrl->sensorsetting_wq,
				     sensor_ctrl);

	return HRTIMER_NORESTART;
}
static enum hrtimer_restart sensor_deadline_timer_handler(struct hrtimer *t)
{
	unsigned int i;
	unsigned int enq_no, sen_no;
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
		camsv_dev = dev_get_drvdata(cam->sv.devs[ctx->sv_pipe[i]->id -
					    MTKCAM_SUBDEV_CAMSV_START]);
		dev_dbg(camsv_dev->dev, "[SOF+%dms]\n", time_after_sof);
		/* handle V4L2_EVENT_REQUEST_DRAINED event */
		mtk_cam_sv_request_drained(camsv_dev, sensor_ctrl);
	}
	if (mtk_cam_is_subsample(ctx)) {
		dev_dbg(cam->dev,
			"[TimerIRQ [SOF+%dms]] ctx:%d, isp_enq_seq_no:%d\n",
			time_after_sof, ctx->stream_id,
			atomic_read(&sensor_ctrl->isp_enq_seq_no));
		return HRTIMER_NORESTART;
	}
	dev_dbg(cam->dev,
			"[TimerIRQ [SOF+%dms]] ctx:%d, sensor_req_seq_no:%d\n",
			time_after_sof, ctx->stream_id,
			atomic_read(&sensor_ctrl->sensor_request_seq_no));
	if (drained_res == 0) {
		sen_no = atomic_read(&sensor_ctrl->sensor_enq_seq_no);
		enq_no = atomic_read(&ctx->enqueued_frame_seq_no);
		if (enq_no == sen_no) {
			mtk_cam_submit_kwork_in_sensorctrl(
			sensor_ctrl->sensorsetting_wq, sensor_ctrl);
			return HRTIMER_NORESTART;
		}
		dev_dbg(cam->dev,
			"[TimerIRQ [SOF+%dms]] ctx:%d, enq:%d/sensor_enq:%d\n",
			time_after_sof, ctx->stream_id, enq_no, sen_no);
	}
	/*using enque timing for sensor setting*/
	if (ctx->used_raw_num) {
		if (ctx->pipe->feature_active == 0) {
			int drained_seq_no =
				atomic_read(&sensor_ctrl->sensor_request_seq_no) + 1;
			atomic_set(&sensor_ctrl->last_drained_seq_no, drained_seq_no);
			return HRTIMER_NORESTART;
		}
	}
	hrtimer_forward_now(&sensor_ctrl->sensor_deadline_timer, m_kt);

	return HRTIMER_RESTART;

}

static void mtk_cam_sof_timer_setup(struct mtk_cam_ctx *ctx)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	ktime_t m_kt;
	struct mtk_seninf_sof_notify_param param;
	int after_sof_ms = ktime_get_boottime_ns() / 1000000
			- sensor_ctrl->sof_time;

	/*notify sof to sensor*/
	param.sd = ctx->seninf;
	param.sof_cnt = atomic_read(&sensor_ctrl->sensor_request_seq_no);
	mtk_cam_seninf_sof_notify(&param);

	//sensor_ctrl->sof_time = ktime_get_boottime_ns() / 1000000;
	sensor_ctrl->sensor_deadline_timer.function =
		sensor_deadline_timer_handler;
	sensor_ctrl->ctx = ctx;
	if (after_sof_ms < 0)
		after_sof_ms = 0;
	else if (after_sof_ms > sensor_ctrl->timer_req_event)
		after_sof_ms = sensor_ctrl->timer_req_event - 1;
	m_kt = ktime_set(0, sensor_ctrl->timer_req_event * 1000000
			- after_sof_ms * 1000000);
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
		struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_camsys_ctrl_state *state_temp, *state_outer = NULL;
	struct mtk_camsys_ctrl_state *state_ready = NULL;
	struct mtk_camsys_ctrl_state *state_rec[STATE_NUM_AT_SOF];
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	int frame_idx_inner = irq_info->frame_idx_inner;
	int stateidx;
	int que_cnt = 0;
	u64 time_boot = ktime_get_boottime_ns();
	u64 time_mono = ktime_get_ns();
	/* add apply sub sample state list*/
	mtk_cam_subspl_req_prepare(sensor_ctrl);
	/* List state-queue status*/
	spin_lock(&sensor_ctrl->camsys_state_lock);
	list_for_each_entry(state_temp, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req = mtk_cam_ctrl_state_get_req(state_temp);
		req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		stateidx = atomic_read(&sensor_ctrl->isp_enq_seq_no) + 1 -
			   req_stream_data->frame_seq_no;
		if (stateidx < STATE_NUM_AT_SOF && stateidx > -1) {
			state_rec[stateidx] = state_temp;
			/* Find outer state element */
			if (state_temp->estate == E_STATE_SUBSPL_SENSOR ||
				state_temp->estate == E_STATE_SUBSPL_OUTER) {
				state_outer = state_temp;
				mtk_cam_set_timestamp(req_stream_data,
						      time_boot, time_mono);
			}
			if (state_temp->estate == E_STATE_SUBSPL_READY ||
				state_temp->estate == E_STATE_SUBSPL_SCQ_DELAY) {
				state_ready = state_temp;
			}
			dev_dbg(raw_dev->dev,
			"[SOF-subsample] STATE_CHECK [N-%d] Req:%d / State:0x%x\n",
			stateidx, req_stream_data->frame_seq_no,
			state_rec[stateidx]->estate);
		}
		/* counter for state queue*/
		que_cnt++;
	}
	spin_unlock(&sensor_ctrl->camsys_state_lock);
	/* HW imcomplete case */
	if (que_cnt >= STATE_NUM_AT_SOF)
		dev_dbg(raw_dev->dev, "[SOF-subsample] HW_DELAY state\n");
	/* Trigger high resolution timer to try sensor setting */
	sensor_ctrl->sof_time = ktime_get_boottime_ns() / 1000000;
	/*check if no dbload happended*/
	if (state_ready && sensor_ctrl->sensorsetting_wq) {
		req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_ready);
		if (atomic_read(&sensor_ctrl->initial_drop_frame_cnt) == 0 &&
			irq_info->frame_idx > frame_idx_inner) {
			dev_info(raw_dev->dev,
				"[SOF-noDBLOAD] HW delay outer_no:%d, inner_idx:%d <= processing_idx:%d,ts:%lu\n",
				req_stream_data->frame_seq_no, frame_idx_inner,
				atomic_read(&sensor_ctrl->isp_request_seq_no),
				irq_info->ts_ns / 1000);
			return STATE_RESULT_PASS_CQ_HW_DELAY;
		}
	}
	mtk_cam_sof_timer_setup(ctx);
	/* Transit outer state to inner state */
	if (state_outer && sensor_ctrl->sensorsetting_wq) {
		req = mtk_cam_ctrl_state_get_req(state_outer);
		req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		if (req_stream_data->frame_seq_no == frame_idx_inner) {
			if (frame_idx_inner > atomic_read(&sensor_ctrl->isp_request_seq_no)) {
				if (state_outer->estate == E_STATE_SUBSPL_OUTER) {
					mtk_cam_submit_kwork_in_sensorctrl(
						sensor_ctrl->sensorsetting_wq, sensor_ctrl);
					dev_dbg(raw_dev->dev, "sensor delay to SOF\n");
				}
				state_transition(state_outer, E_STATE_SUBSPL_SENSOR,
						 E_STATE_SUBSPL_INNER);
				atomic_set(&sensor_ctrl->isp_request_seq_no, frame_idx_inner);
				dev_dbg(raw_dev->dev,
					"[SOF-subsample] frame_seq_no:%d, SENSOR/OUTER->INNER state:0x%x\n",
					req_stream_data->frame_seq_no, state_outer->estate);
			}
		}
	}
	/* Initial request case */
	if (atomic_read(&sensor_ctrl->sensor_request_seq_no) <
		INITIAL_DROP_FRAME_CNT) {
		atomic_set(&sensor_ctrl->isp_request_seq_no, frame_idx_inner);
		dev_dbg(raw_dev->dev, "[SOF-subsample] INIT STATE cnt:%d\n", que_cnt);
		if (que_cnt > 0 && state_ready)
			state_transition(state_ready, E_STATE_SUBSPL_READY,
					 E_STATE_SUBSPL_SCQ);
		return STATE_RESULT_PASS_CQ_INIT;
	}
	if (que_cnt > 0 && state_ready) {
		/* CQ triggering judgment*/
		if (state_ready->estate == E_STATE_SUBSPL_READY) {
			*current_state = state_ready;
			return STATE_RESULT_TRIGGER_CQ;
		}
		/* last SCQ triggering delay judgment*/
		if (state_ready->estate == E_STATE_SUBSPL_SCQ_DELAY) {
			dev_dbg(raw_dev->dev, "[SOF-subsample] SCQ_DELAY state:0x%x\n",
				state_ready->estate);
			return STATE_RESULT_PASS_CQ_SCQ_DELAY;
		}
	}

	return STATE_RESULT_PASS_CQ_SW_DELAY;
}

static int mtk_camsys_raw_state_handle(struct mtk_raw_device *raw_dev,
		struct mtk_camsys_sensor_ctrl *sensor_ctrl,
		struct mtk_camsys_ctrl_state **current_state,
		struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_camsys_ctrl_state *state_temp, *state_outer = NULL;
	struct mtk_camsys_ctrl_state *state_sensor = NULL;
	struct mtk_camsys_ctrl_state *state_inner = NULL;
	struct mtk_camsys_ctrl_state *state_rec[STATE_NUM_AT_SOF];
	struct mtk_cam_request_stream_data *req_stream_data;
	int frame_idx_inner = irq_info->frame_idx_inner;
	int stateidx;
	int que_cnt = 0;
	int write_cnt;
	u64 time_boot = ktime_get_boottime_ns();
	u64 time_mono = ktime_get_ns();
	int working_req_found = 0;
	int switch_type;

	/* List state-queue status*/
	spin_lock(&sensor_ctrl->camsys_state_lock);
	list_for_each_entry(state_temp, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_temp);
		stateidx = atomic_read(&sensor_ctrl->sensor_request_seq_no) -
			   req_stream_data->frame_seq_no;
		if (stateidx < STATE_NUM_AT_SOF && stateidx > -1) {
			state_rec[stateidx] = state_temp;
			if (stateidx == 0)
				working_req_found = 1;
			/* Find outer state element */
			if (state_temp->estate == E_STATE_OUTER ||
				state_temp->estate == E_STATE_CAMMUX_OUTER ||
			    state_temp->estate == E_STATE_OUTER_HW_DELAY) {
				state_outer = state_temp;
				mtk_cam_set_timestamp(req_stream_data,
						      time_boot, time_mono);
			}
			/* Find inner state element request*/
			if (state_temp->estate == E_STATE_INNER ||
			    state_temp->estate == E_STATE_INNER_HW_DELAY) {
				state_inner = state_temp;
			}
			/* Find sensor state element request*/
			if (state_temp->estate <= E_STATE_SENSOR) {
				state_sensor = state_temp;
			}
			dev_dbg(raw_dev->dev,
			"[SOF] STATE_CHECK [N-%d] Req:%d / State:%d\n",
			stateidx, req_stream_data->frame_seq_no,
			state_rec[stateidx]->estate);
		}
		/* counter for state queue*/
		que_cnt++;
	}
	spin_unlock(&sensor_ctrl->camsys_state_lock);
	/* HW imcomplete case */
	if (state_inner) {
		req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_inner);
		write_cnt = (atomic_read(&sensor_ctrl->isp_request_seq_no) / 256)
					* 256 + irq_info->write_cnt;
		if (frame_idx_inner > atomic_read(&sensor_ctrl->isp_request_seq_no) ||
			atomic_read(&req_stream_data->frame_done_work.is_queued) == 1) {
			dev_info_ratelimited(raw_dev->dev, "[SOF] frame done work too late frames. req(%d),ts(%lu)\n",
				req_stream_data->frame_seq_no, irq_info->ts_ns / 1000);
		} else if (mtk_cam_is_stagger(ctx)) {
			dev_dbg(raw_dev->dev, "[SOF:%d] HDR SWD over SOF case\n", frame_idx_inner);
		} else if (write_cnt >= req_stream_data->frame_seq_no) {
			dev_info_ratelimited(raw_dev->dev, "[SOF] frame done reading lost %d frames. req(%d),ts(%lu)\n",
				write_cnt - req_stream_data->frame_seq_no + 1,
				req_stream_data->frame_seq_no, irq_info->ts_ns / 1000);
			mtk_cam_set_timestamp(req_stream_data,
						      time_boot - 1000, time_mono - 1000);
			mtk_camsys_frame_done(ctx, write_cnt, ctx->stream_id);
		} else if ((write_cnt >= req_stream_data->frame_seq_no - 1)
			&& irq_info->fbc_cnt == 0) {
			dev_info_ratelimited(raw_dev->dev, "[SOF] frame done reading lost frames. req(%d),ts(%lu)\n",
				req_stream_data->frame_seq_no, irq_info->ts_ns / 1000);
			mtk_cam_set_timestamp(req_stream_data,
						      time_boot - 1000, time_mono - 1000);
			mtk_camsys_frame_done(ctx, write_cnt + 1, ctx->stream_id);
		} else {
			state_transition(state_inner, E_STATE_INNER,
				 E_STATE_INNER_HW_DELAY);
			if (state_outer) {
				state_transition(state_outer, E_STATE_OUTER,
				 E_STATE_OUTER_HW_DELAY);
				state_transition(state_outer, E_STATE_CAMMUX_OUTER,
				 E_STATE_OUTER_HW_DELAY);
			}
			dev_info_ratelimited(raw_dev->dev,
				"[SOF] HW_IMCOMPLETE state cnt(%d,%d),req(%d),ts(%lu)\n",
				write_cnt, irq_info->write_cnt, req_stream_data->frame_seq_no,
				irq_info->ts_ns / 1000);
			return STATE_RESULT_PASS_CQ_HW_DELAY;
		}
	}
	/* Transit outer state to inner state */
	if (state_outer && sensor_ctrl->sensorsetting_wq) {
		req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_outer);
		if (atomic_read(&sensor_ctrl->initial_drop_frame_cnt) == 0 &&
			req_stream_data->frame_seq_no > frame_idx_inner) {
			dev_info(raw_dev->dev,
				"[SOF-noDBLOAD] HW delay outer_no:%d, inner_idx:%d <= processing_idx:%d,ts:%lu\n",
				req_stream_data->frame_seq_no, frame_idx_inner,
				atomic_read(&sensor_ctrl->isp_request_seq_no),
				irq_info->ts_ns / 1000);
			return STATE_RESULT_PASS_CQ_HW_DELAY;
		}

		if (atomic_read(&sensor_ctrl->initial_drop_frame_cnt) == 0 &&
			req_stream_data->frame_seq_no == frame_idx_inner) {
			if (frame_idx_inner > atomic_read(&sensor_ctrl->isp_request_seq_no)) {
				state_transition(state_outer,
						 E_STATE_OUTER_HW_DELAY,
						 E_STATE_INNER_HW_DELAY);
				state_transition(state_outer, E_STATE_OUTER,
						 E_STATE_INNER);
				state_transition(state_outer, E_STATE_CAMMUX_OUTER,
						 E_STATE_INNER);
				atomic_set(&sensor_ctrl->isp_request_seq_no, frame_idx_inner);
				dev_dbg(raw_dev->dev,
					"[SOF-DBLOAD] frame_seq_no:%d, OUTER->INNER state:%d,ts:%lu\n",
					req_stream_data->frame_seq_no, state_outer->estate,
					irq_info->ts_ns / 1000);
			}
		}
	}
	/* Trigger high resolution timer to try sensor setting */
	sensor_ctrl->sof_time = irq_info->ts_ns / 1000000;
	mtk_cam_sof_timer_setup(ctx);
	/* Initial request case - 1st sensor wasn't set yet or initial drop wasn't finished*/
	if (MTK_CAM_INITIAL_REQ_SYNC) {
		if (atomic_read(&sensor_ctrl->sensor_request_seq_no)
			<= INITIAL_DROP_FRAME_CNT ||
			atomic_read(&sensor_ctrl->initial_drop_frame_cnt)) {
			dev_dbg(raw_dev->dev, "[SOF] INIT STATE cnt:%d\n", que_cnt);
			if (que_cnt > 0 && state_rec[0]) {
				state_temp = state_rec[0];
				req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_temp);
				if (req_stream_data->frame_seq_no == 1)
					state_transition(state_temp, E_STATE_SENSOR,
						 E_STATE_OUTER);
			}
			return STATE_RESULT_PASS_CQ_INIT;
		}
	} else {
		if (que_cnt > 1 && state_rec[1]) {
			state_temp = state_rec[1];
			req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_temp);
			if (req_stream_data->frame_seq_no == 1)
				state_transition(state_temp, E_STATE_SENSOR,
					 E_STATE_INNER);
		}
	}
	if (que_cnt > 0) {
		/*handle exposure switch at frame start*/
		if (working_req_found && state_rec[0]) {
			req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_rec[0]);
			switch_type = req_stream_data->feature.switch_feature_type;
			if (switch_type && raw_dev->sof_count > 1 &&
				req_stream_data->feature.switch_done == 0 &&
				!mtk_cam_feature_change_is_mstream(switch_type)) {
				mtk_cam_exp_sensor_switch(ctx, req_stream_data);
					state_transition(state_rec[0], E_STATE_READY,
							E_STATE_SENSOR);
				*current_state = state_rec[0];
				return STATE_RESULT_TRIGGER_CQ;
			}
		}
		if (working_req_found && state_rec[0]) {
			if (state_rec[0]->estate == E_STATE_READY) {
				dev_info(raw_dev->dev, "[SOF] sensor delay ts:%lu\n",
					irq_info->ts_ns / 1000);
				req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_rec[0]);
				req_stream_data->flags |=
					MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_DELAYED;
			}

			if (state_rec[0]->estate == E_STATE_SENINF)
				dev_info(raw_dev->dev, "[SOF] sensor switch delay\n");

			/* CQ triggering judgment*/
			if (state_rec[0]->estate == E_STATE_SENSOR) {
				*current_state = state_rec[0];
				return STATE_RESULT_TRIGGER_CQ;
			}
			/* last SCQ triggering delay judgment*/
			if (state_rec[0]->estate == E_STATE_CQ_SCQ_DELAY) {
				state_transition(state_rec[0], E_STATE_CQ_SCQ_DELAY,
						E_STATE_OUTER);
				dev_info(raw_dev->dev, "[SOF] SCQ_DELAY state:%d ts:%lu\n",
					state_rec[0]->estate, irq_info->ts_ns / 1000);
				return STATE_RESULT_PASS_CQ_SCQ_DELAY;
			}
		} else {
			dev_dbg(raw_dev->dev, "[SOF] working request not found\n");
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
	req = mtk_cam_get_req(ctx, dequeued_frame_seq_no);
	if (req != NULL) {
		req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
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
	req = mtk_cam_get_req(ctx, dequeued_frame_seq_no);
	if (!req) {
		dev_info(raw_dev->dev,
			"TS-CQ[ctx:%d-#%d], request drained\n",
			ctx->stream_id, dequeued_frame_seq_no);
		return;
	}
	req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
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
		apply_cq(raw_dev, 0, base_addr,
			buf_entry->cq_desc_size,
			buf_entry->cq_desc_offset,
			buf_entry->sub_cq_desc_size,
			buf_entry->sub_cq_desc_offset);
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
		int frame_idx_inner)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_camsys_ctrl_state *state_temp = NULL;
	struct mtk_camsys_ctrl_state *state_rec[STATE_NUM_AT_SOF];
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	int stateidx;
	int que_cnt = 0;
	u64 time_boot = ktime_get_boottime_ns();
	u64 time_mono = ktime_get_ns();

	/* List state-queue status*/
	spin_lock(&sensor_ctrl->camsys_state_lock);
	list_for_each_entry(state_temp, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req = mtk_cam_ctrl_state_get_req(state_temp);
		req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		stateidx = atomic_read(&sensor_ctrl->sensor_request_seq_no) -
			   req_stream_data->frame_seq_no;
		if (stateidx < STATE_NUM_AT_SOF && stateidx > -1) {
			state_rec[stateidx] = state_temp;
			if (state_temp->estate == E_STATE_TS_SV) {
				req_stream_data->timestamp = time_boot;
				req_stream_data->timestamp_mono = time_mono;
			}
			dev_dbg(ctx->cam->dev,
			"[TS-SOF] ctx:%d STATE_CHECK [N-%d] Req:%d / State:0x%x\n",
			ctx->stream_id, stateidx, req_stream_data->frame_seq_no,
			state_rec[stateidx]->estate);
		}
		/* counter for state queue*/
		que_cnt++;
	}
	spin_unlock(&sensor_ctrl->camsys_state_lock);
	if (que_cnt > 0 && state_rec[0]) {
		if (state_rec[0]->estate == E_STATE_TS_READY) {
			dev_info(ctx->cam->dev, "[TS-SOF] sensor delay\n");
			return STATE_RESULT_PASS_CQ_SW_DELAY;
		}
	}
	/* Trigger high resolution timer to try sensor setting */
	sensor_ctrl->sof_time = ktime_get_boottime_ns() / 1000000;
	mtk_cam_sof_timer_setup(ctx);
	if (que_cnt > 0 && state_rec[0]) {
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
		req_cq = mtk_cam_ctrl_state_get_req(current_state);
		req_stream_data = mtk_cam_req_get_s_data(req_cq, ctx->stream_id, 0);
		/* time sharing sv wdma flow - stream on at 1st request*/
		for (sv_i = MTKCAM_SUBDEV_CAMSV_END - 1;
			sv_i >= MTKCAM_SUBDEV_CAMSV_START; sv_i--) {
			if (ctx->pipe->enabled_raw & (1 << sv_i)) {
				dev_sv = cam->sv.devs[sv_i - MTKCAM_SUBDEV_CAMSV_START];
				camsv_dev = dev_get_drvdata(dev_sv);
				mtk_cam_sv_enquehwbuf(camsv_dev,
				req_stream_data->frame_params.img_ins[0].buf[0].iova,
				req_stream_data->frame_seq_no);
			}
		}
		req_stream_data->timestamp = ktime_get_boottime_ns();
		dev_dbg(cam->dev,
		"TS-SOF[ctx:%d-#%d], SV-ENQ req:%d is update, composed:%d, iova:0x%x, time:%lld\n",
		ctx->stream_id, dequeued_frame_seq_no, req_stream_data->frame_seq_no,
		ctx->composed_frame_seq_no, req_stream_data->frame_params.img_ins[0].buf[0].iova,
		req_stream_data->timestamp);
	}
}

static bool
mtk_cam_raw_prepare_mstream_frame_done(struct mtk_cam_ctx *ctx,
				       struct mtk_cam_request_stream_data *req_stream_data)
{
	unsigned int frame_undone;
	struct mtk_cam_device *cam;
	struct mtk_cam_request_stream_data *s_data;
	struct mtk_cam_request *state_req;
	struct mtk_camsys_ctrl_state *state_entry;

	cam = ctx->cam;
	state_req = mtk_cam_s_data_get_req(req_stream_data);
	state_entry = &req_stream_data->state;
	s_data = mtk_cam_req_get_s_data(state_req, ctx->stream_id, 0);
	if (mtk_cam_feature_is_mstream(s_data->feature.raw_feature) ||
			mtk_cam_feature_is_mstream_m2m(s_data->feature.raw_feature)) {
		if (req_stream_data->frame_seq_no == s_data->frame_seq_no)
			frame_undone = 0;
		else
			frame_undone = 1;

		dev_dbg(cam->dev,
			"[mstream][SWD] req:%d/state:%d/time:%lld/sync_id:%lld/frame_undone:%d\n",
			req_stream_data->frame_seq_no, state_entry->estate,
			req_stream_data->timestamp, state_req->sync_id,
			frame_undone);

		if (frame_undone)
			return false;
	} else {
		dev_dbg(cam->dev, "[mstream][SWD] req:%d/state:%d/time:%lld/sync_id:%lld\n",
			req_stream_data->frame_seq_no, state_entry->estate,
			req_stream_data->timestamp,
			state_req->sync_id);
	}

	return true;
}

static void mtk_cam_mstream_frame_sync(struct mtk_raw_device *raw_dev,
					struct mtk_cam_ctx *ctx,
					unsigned int dequeued_frame_seq_no)
{
	struct mtk_cam_request *req;

	req = mtk_cam_get_req(ctx, dequeued_frame_seq_no);
	if (req) {
		struct mtk_cam_request_stream_data *s_data =
			mtk_cam_req_get_s_data(req, ctx->stream_id, 0);

		if (mtk_cam_feature_is_mstream(s_data->feature.raw_feature) ||
				mtk_cam_feature_is_mstream_m2m(s_data->feature.raw_feature)) {
			/* report on first exp */
			s_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 1);
			if (dequeued_frame_seq_no == s_data->frame_seq_no) {
				dev_dbg(raw_dev->dev,
					"%s mstream [SOF] with-req frame:%d sof:%d enque_req_cnt:%d\n",
					__func__, dequeued_frame_seq_no,
					req->p_data[ctx->stream_id].req_seq,
					ctx->enqueued_request_cnt);
				ctx->next_sof_mask_frame_seq_no =
					dequeued_frame_seq_no + 1;
				ctx->working_request_seq =
					req->p_data[ctx->stream_id].req_seq;
				mtk_cam_event_frame_sync(ctx->pipe,
					req->p_data[ctx->stream_id].req_seq);
			} else if (dequeued_frame_seq_no ==
				ctx->next_sof_mask_frame_seq_no) {
				dev_dbg(raw_dev->dev, "mstream [SOF-mask] with-req frame:%d working_seq:%d, sof_cnt:%d\n",
					dequeued_frame_seq_no, ctx->working_request_seq,
					raw_dev->sof_count);
				ctx->next_sof_mask_frame_seq_no = 1;
			} else {
				dev_dbg(raw_dev->dev, "mstream [SOF] with-req frame:%d\n",
					ctx->working_request_seq);
				mtk_cam_event_frame_sync(ctx->pipe,
					ctx->working_request_seq);
			}
		} else {
			/* mstream 1exp case */
			dev_dbg(raw_dev->dev,
					"%s mstream 1-exp [SOF] with-req frame:%d sof:%d enque_req_cnt:%d\n",
					__func__, dequeued_frame_seq_no,
					req->p_data[ctx->stream_id].req_seq,
					ctx->enqueued_request_cnt);
			ctx->working_request_seq =
				req->p_data[ctx->stream_id].req_seq;
			mtk_cam_event_frame_sync(ctx->pipe,
				req->p_data[ctx->stream_id].req_seq);
		}
	} else if (dequeued_frame_seq_no ==
			ctx->next_sof_mask_frame_seq_no) {
		/* when frame request is already remove sof_done block or laggy enque case */
		dev_dbg(raw_dev->dev, "mstream [SOF-mask] req-gone frame:%d sof:%d sof_cnt:%d\n",
			ctx->next_sof_mask_frame_seq_no, ctx->working_request_seq,
			raw_dev->sof_count);
		ctx->next_sof_mask_frame_seq_no = 1;
	} else {
		/* except: keep report current working request sequence */
		dev_dbg(raw_dev->dev, "mstream [SOF] req-gone frame:%d\n",
			ctx->working_request_seq);
		mtk_cam_event_frame_sync(ctx->pipe,
				ctx->working_request_seq);
	}
}

static void mtk_cam_handle_m2m_frame_done(struct mtk_cam_ctx *ctx,
			      unsigned int dequeued_frame_seq_no,
			      unsigned int pipe_id)
{
	struct mtk_raw_device *raw_dev = NULL;
	struct mtk_camsys_ctrl_state *state_temp, *state_inner = NULL;
	struct mtk_camsys_ctrl_state *state_sensor = NULL;
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_working_buf_entry *buf_entry;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	dma_addr_t base_addr;
	int que_cnt = 0;
	u64 time_boot = ktime_get_boottime_ns();
	u64 time_mono = ktime_get_ns();
	int dequeue_cnt;

	spin_lock(&ctx->streaming_lock);
	if (!ctx->streaming) {
		dev_dbg(ctx->cam->dev,
			 "%s: skip frame done for stream off ctx:%d\n",
			 __func__, ctx->stream_id);
		spin_unlock(&ctx->streaming_lock);
		return;
	}
	spin_unlock(&ctx->streaming_lock);

	raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);

	/* Send V4L2_EVENT_FRAME_SYNC event */
	if (mtk_cam_is_mstream_m2m(ctx) || ctx->next_sof_mask_frame_seq_no != 0) {
		mtk_cam_mstream_frame_sync(raw_dev, ctx, dequeued_frame_seq_no);
	} else {
		/* normal */
		mtk_cam_event_frame_sync(ctx->pipe, dequeued_frame_seq_no);
	}

	ctx->dequeued_frame_seq_no = dequeued_frame_seq_no;

	/* List state-queue status*/
	spin_lock(&sensor_ctrl->camsys_state_lock);
	list_for_each_entry(state_temp, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req = mtk_cam_ctrl_state_get_req(state_temp);
		req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);

		if (state_temp->estate == E_STATE_INNER && state_inner == NULL)
			state_inner = state_temp;
		else if (state_temp->estate == E_STATE_SENSOR && state_sensor == NULL)
			state_sensor = state_temp;
		/* counter for state queue*/
		que_cnt++;
	}
	spin_unlock(&sensor_ctrl->camsys_state_lock);

	/* Transit inner state to done state */
	if (state_inner) {
		req = mtk_cam_ctrl_state_get_req(state_inner);
		req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);

		dev_dbg(raw_dev->dev,
			"[M2M P1 Don] req_stream_data->frame_seq_no:%d dequeued_frame_seq_no:%d\n",
			req_stream_data->frame_seq_no, dequeued_frame_seq_no);

		if (req_stream_data->frame_seq_no == dequeued_frame_seq_no) {
			state_transition(state_inner, E_STATE_INNER,
			      E_STATE_DONE_NORMAL);
			atomic_set(&sensor_ctrl->isp_request_seq_no, dequeued_frame_seq_no);
			dev_dbg(raw_dev->dev,
			      "[Frame done] frame_seq_no:%d, INNER->DONE_NORMAL state:%d\n",
			      req_stream_data->frame_seq_no, state_inner->estate);
		}
	}

	req_stream_data = mtk_cam_get_req_s_data(ctx, ctx->stream_id,
						dequeued_frame_seq_no);
	if (mtk_cam_is_mstream_m2m(ctx)) {
		if (mtk_cam_raw_prepare_mstream_frame_done(ctx, req_stream_data)) {
			dequeue_cnt = mtk_cam_dequeue_req_frame(ctx,
					dequeued_frame_seq_no, ctx->stream_id);
		}
	} else {
		dequeue_cnt = mtk_cam_dequeue_req_frame(ctx,
				dequeued_frame_seq_no, ctx->stream_id);
	}
	complete(&ctx->m2m_complete);

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

		if (state_sensor == NULL) {
			dev_info(raw_dev->dev, "[M2M P1 Don] Invalid state_sensor\n");
			return;
		}

		req = mtk_cam_ctrl_state_get_req(state_sensor);
		req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		req_stream_data->timestamp = time_boot;
		req_stream_data->timestamp_mono = time_mono;

		apply_cq(raw_dev, 0, base_addr,
			buf_entry->cq_desc_size,
			buf_entry->cq_desc_offset,
			buf_entry->sub_cq_desc_size,
			buf_entry->sub_cq_desc_offset);
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

	if (dequeue_cnt) {
		mutex_lock(&ctx->cam->queue_lock);
		mtk_cam_dev_req_try_queue(ctx->cam);
		mutex_unlock(&ctx->cam->queue_lock);
	}
}
static bool hdr_apply_cq_at_first_sof(struct mtk_raw_device *raw_dev,
				       struct mtk_camsys_ctrl_state *cur_state,
				       int state_ret)
{
	struct mtk_cam_request_stream_data *s_data;

	/*hd last sof trigger setting enable*/
	if (!cur_state)
		return true;
	s_data = mtk_cam_ctrl_state_to_req_s_data(cur_state);
	if (!s_data)
		return true;
	if (raw_dev->stagger_en) {
		atomic_set(&s_data->first_setting_check, 1);
		if (STAGGER_CQ_LAST_SOF == 0)
			return true;
		else
			return false;
	}

	return true;
}

static void mtk_camsys_raw_frame_start(struct mtk_raw_device *raw_dev,
				       struct mtk_cam_ctx *ctx,
				       struct mtk_camsys_irq_info *irq_info)
{
	unsigned int dequeued_frame_seq_no = irq_info->frame_idx_inner;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_cam_working_buf_entry *buf_entry;
	struct mtk_camsys_ctrl_state *current_state;
	dma_addr_t base_addr;
	enum MTK_CAMSYS_STATE_RESULT state_handle_ret;
	bool is_apply = false;

	/* update sv/mraw's ts */
	if (mtk_cam_sv_update_all_buffer_ts(ctx, irq_info->ts_ns) == 0)
		dev_dbg(raw_dev->dev, "sv update all buffer ts failed");
	if (mtk_cam_mraw_update_all_buffer_ts(ctx, irq_info->ts_ns) == 0)
		dev_dbg(raw_dev->dev, "mraw update all buffer ts failed");

	/*touch watchdog*/
	if (watchdog_scenario(ctx))
		mtk_ctx_watchdog_kick(ctx);
	/* inner register dequeue number */
	if (!mtk_cam_is_stagger(ctx))
		ctx->dequeued_frame_seq_no = dequeued_frame_seq_no;
	/* Send V4L2_EVENT_FRAME_SYNC event */
	if (mtk_cam_is_mstream(ctx) || ctx->next_sof_mask_frame_seq_no != 0) {
		mtk_cam_mstream_frame_sync(raw_dev, ctx, dequeued_frame_seq_no);
	} else {
		/* normal */
		mtk_cam_event_frame_sync(ctx->pipe, dequeued_frame_seq_no);
	}

	/* Find request of this dequeued frame */
	req_stream_data = mtk_cam_get_req_s_data(ctx, ctx->stream_id, dequeued_frame_seq_no);

	/* Detect no frame done and trigger camsys dump for debugging */
	mtk_cam_debug_detect_dequeue_failed(req_stream_data, 30, irq_info, raw_dev);
	if (ctx->sensor) {
		if (mtk_cam_is_subsample(ctx))
			state_handle_ret =
			mtk_camsys_raw_subspl_state_handle(raw_dev, sensor_ctrl,
						&current_state, irq_info);
		else
			state_handle_ret =
			mtk_camsys_raw_state_handle(raw_dev, sensor_ctrl,
						&current_state, irq_info);
		if (state_handle_ret != STATE_RESULT_TRIGGER_CQ) {
			dev_dbg(raw_dev->dev, "[SOF] CQ drop s:%d deq:%d\n",
				state_handle_ret, dequeued_frame_seq_no);
			if (ctx->pipe->stagger_path == STAGGER_DCIF) {
				//mtk_camsys_hdr_dcif_enquesv();
				struct mtk_cam_img_working_buf_entry *buf_entry;
				struct mtk_camsv_device *camsv_dev;
				struct device *dev_sv;
				int sv_i;

				for (sv_i = MTKCAM_SUBDEV_CAMSV_START;
					sv_i < MTKCAM_SUBDEV_CAMSV_END ; sv_i++) {
					if (ctx->pipe->enabled_raw & (1 << sv_i)) {
						dev_sv = ctx->cam->sv.devs[sv_i
							- MTKCAM_SUBDEV_CAMSV_START];
						camsv_dev = dev_get_drvdata(dev_sv);
						/* prepare working buffer */
						buf_entry = mtk_cam_img_working_buf_get(ctx);
						mtk_cam_sv_enquehwbuf(camsv_dev,
							buf_entry->img_buffer.iova,
							dequeued_frame_seq_no);
						dev_info(camsv_dev->dev,
						"[%s] dcif stagger workaround camsv id:%d/iova:0x%x\n",
						__func__, camsv_dev->id,
						buf_entry->img_buffer.iova);
						mtk_cam_img_working_buf_put(buf_entry);
					}
				}
			}
			return;
		}
		if (!hdr_apply_cq_at_first_sof(raw_dev, current_state, state_handle_ret))
			return;
	}
	/* Update CQ base address if needed */
	if (ctx->composed_frame_seq_no <= dequeued_frame_seq_no) {
		dev_info_ratelimited(raw_dev->dev,
			"SOF[ctx:%d-#%d], CQ isn't updated [composed_frame_deq (%d) ts:%lu]\n",
			ctx->stream_id, dequeued_frame_seq_no,
			ctx->composed_frame_seq_no, irq_info->ts_ns / 1000);
		return;
	}
	/* apply next composed buffer */
	spin_lock(&ctx->composed_buffer_list.lock);
	if (list_empty(&ctx->composed_buffer_list.list)) {
		dev_info_ratelimited(raw_dev->dev,
			"SOF_INT_ST, no buffer update, cq_num:%d, frame_seq:%d, ts:%lu\n",
			ctx->composed_frame_seq_no, dequeued_frame_seq_no,
			irq_info->ts_ns / 1000);
		spin_unlock(&ctx->composed_buffer_list.lock);
	} else {
		is_apply = true;
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
		apply_cq(raw_dev, 0, base_addr,
			buf_entry->cq_desc_size,
			buf_entry->cq_desc_offset,
			buf_entry->sub_cq_desc_size,
			buf_entry->sub_cq_desc_offset);

		/* req_stream_data of req_cq*/
		req_stream_data = mtk_cam_ctrl_state_to_req_s_data(current_state);
		/* update qos bw */
		mtk_cam_qos_bw_calc(ctx, req_stream_data->raw_dmas, false);

		/* Transit state from Sensor -> CQ */
		if (ctx->sensor) {
			if (mtk_cam_is_subsample(ctx))
				state_transition(current_state,
				E_STATE_SUBSPL_READY, E_STATE_SUBSPL_SCQ);
			else
				state_transition(current_state,
				E_STATE_SENSOR, E_STATE_CQ);

			dev_dbg(raw_dev->dev,
			"SOF[ctx:%d-#%d], CQ-%d is update, composed:%d, cq_addr:0x%x, time:%lld, monotime:%lld\n",
			ctx->stream_id, dequeued_frame_seq_no, req_stream_data->frame_seq_no,
			ctx->composed_frame_seq_no, base_addr, req_stream_data->timestamp,
			req_stream_data->timestamp_mono);
		}
	}

	if (mtk_cam_is_with_w_channel(ctx) && is_apply) {
		if (mtk_cam_sv_rgbw_apply_next_buffer(buf_entry->s_data) == 0)
			dev_info(raw_dev->dev, "rgbw: sv apply next buffer failed");
	}
	if (ctx->used_sv_num && is_apply) {
		if (mtk_cam_sv_apply_all_buffers(ctx) == 0)
			dev_info(raw_dev->dev, "sv apply all buffers failed");
	}
	if (ctx->used_mraw_num && is_apply) {
		if (mtk_cam_mraw_apply_all_buffers(ctx) == 0)
			dev_info(raw_dev->dev, "mraw apply all buffers failed");
	}
}
static void seamless_switch_check_bad_frame(
		struct mtk_cam_ctx *ctx, unsigned int frame_seq_no)

{
	struct mtk_cam_request *req, *req_bad, *req_cq;
	struct mtk_cam_request_stream_data *s_data, *s_data_bad, *s_data_cq;
	struct mtk_raw_device *raw_dev;
	struct mtk_cam_working_buf_entry *buf_entry;
	int switch_type;
	dma_addr_t base_addr;
	int raw_pipe_done = 0;
	/*check if switch request 's previous frame was done, */
	/*if yes , fine.*/
	/*if no, switch bad frame handling : may trigger frame done also */
	req = mtk_cam_get_req(ctx, frame_seq_no);
	if (req) {
		s_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		switch_type = s_data->feature.switch_feature_type;
		/* update qos bw */
		if (switch_type == EXPOSURE_CHANGE_1_to_2 ||
			switch_type == EXPOSURE_CHANGE_1_to_3)
			mtk_cam_qos_bw_calc(ctx, s_data->raw_dmas, true);

		if (switch_type &&
			!mtk_cam_feature_change_is_mstream(switch_type)) {
			req_bad = mtk_cam_get_req(ctx, frame_seq_no - 1);
			raw_pipe_done = req_bad->done_status & (1 << ctx->pipe->id);
			if (req_bad && raw_pipe_done == 0) {
				raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
				s_data_bad = mtk_cam_req_get_s_data(req_bad, ctx->stream_id, 0);
				dev_info(ctx->cam->dev,
				"[SWD-error] switch req:%d type:%d, bad req:%d,done_status:0x%x,used:0x%x\n",
				s_data->frame_seq_no, switch_type,
				s_data_bad->frame_seq_no, req_bad->done_status, req_bad->pipe_used);
				reset(raw_dev);
				dev_info(ctx->cam->dev,
				"[SWD-error] reset (FBC accumulated due to force dbload)\n");
				req_cq = mtk_cam_get_req(ctx, frame_seq_no + 1);
				if (req_cq) {
					s_data_cq = mtk_cam_req_get_s_data(req_cq,
							ctx->stream_id, 0);
					if (s_data_cq->state.estate >= E_STATE_OUTER) {
						buf_entry = s_data_cq->working_buf;
						base_addr = buf_entry->buffer.iova;
						apply_cq(raw_dev, 0, base_addr,
								buf_entry->cq_desc_size,
								buf_entry->cq_desc_offset,
								buf_entry->sub_cq_desc_size,
								buf_entry->sub_cq_desc_offset);
					} else {
						dev_info(ctx->cam->dev,
						"[SWD-error] w/o cq, req:%d state:0x%x\n",
						s_data_cq->frame_seq_no, s_data_cq->state.estate);
					}
				}
				debug_dma_fbc(raw_dev->dev, raw_dev->base, raw_dev->yuv_base);
			}
		}
	}
}

static int mtk_cam_hdr_last_frame_switch_check(
		struct mtk_camsys_ctrl_state *state,
		struct mtk_cam_request_stream_data *req_stream_data)
{
	int type = req_stream_data->feature.switch_feature_type;

	if ((type == EXPOSURE_CHANGE_2_to_1 ||
		type == EXPOSURE_CHANGE_3_to_1) &&
			req_stream_data->feature.switch_done == 0) {
		return 1;
	} else if ((type == EXPOSURE_CHANGE_1_to_2 ||
				type == EXPOSURE_CHANGE_1_to_3) &&
				state->estate > E_STATE_SENSOR &&
				state->estate < E_STATE_CAMMUX_OUTER) {
		return 1;
	}

	return 0;
}

int hdr_apply_cq_at_last_sof(struct mtk_raw_device *raw_dev,
			struct mtk_cam_ctx *ctx,
			struct mtk_camsys_ctrl_state *state_sensor,
			struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_cam_working_buf_entry *buf_entry;
	struct mtk_cam_request_stream_data *s_data;
	struct mtk_camsv_device *camsv_dev;
	struct device *dev_sv;
	dma_addr_t base_addr;
	bool is_apply = false;
	unsigned int dequeued_frame_seq_no = irq_info->frame_idx_inner;
	int sv_main_id;
	int switch_type;

	/*hd last sof trigger setting enable*/
	if (STAGGER_CQ_LAST_SOF == 0)
		return 0;
	/*if db load judgment*/
	if (ctx->sensor_ctrl.sensorsetting_wq) {
		if (atomic_read(&ctx->sensor_ctrl.initial_drop_frame_cnt) == 0 &&
			irq_info->frame_idx > dequeued_frame_seq_no) {
			dev_info(raw_dev->dev,
				"[SOF-noDBLOAD] HW delay outer_no:%d, inner_idx:%d <= processing_idx:%d\n",
				irq_info->frame_idx, dequeued_frame_seq_no,
				atomic_read(&ctx->sensor_ctrl.isp_request_seq_no));
			sv_main_id = get_main_sv_pipe_id(ctx->cam, ctx->pipe->enabled_raw);
			dev_sv = ctx->cam->sv.devs[sv_main_id - MTKCAM_PIPE_CAMSV_0];
			camsv_dev = dev_get_drvdata(dev_sv);
			if (ctx->component_dequeued_frame_seq_no > irq_info->frame_idx_inner) {
				mtk_cam_sv_write_rcnt(ctx, sv_main_id);
				dev_info(raw_dev->dev,
				"[SOF-noDBLOAD] camsv_id:%d rcnt++ (sv/raw inner: %d/%d)\n",
				sv_main_id, ctx->component_dequeued_frame_seq_no,
				irq_info->frame_idx_inner);
			}
			return 0;
		}
	}
	/* Update CQ base address if needed */
	if (ctx->composed_frame_seq_no <= dequeued_frame_seq_no) {
		dev_info_ratelimited(raw_dev->dev,
			"SOF[ctx:%d-#%d], CQ isn't updated [composed_frame_deq (%d) ]\n",
			ctx->stream_id, dequeued_frame_seq_no,
			ctx->composed_frame_seq_no);
		return 0;
	}
	if (!state_sensor)
		return 0;
	s_data = mtk_cam_ctrl_state_to_req_s_data(state_sensor);
	/* apply next composed buffer */
	spin_lock(&ctx->composed_buffer_list.lock);
	if (list_empty(&ctx->composed_buffer_list.list)) {
		dev_info(raw_dev->dev,
			"SOF_INT_ST, no buffer update, cq_num:%d, frame_seq:%d\n",
			ctx->composed_frame_seq_no, dequeued_frame_seq_no);
		spin_unlock(&ctx->composed_buffer_list.lock);
	} else if (!atomic_read(&s_data->first_setting_check) &&
			s_data->feature.switch_feature_type == 0) {
		dev_info(raw_dev->dev,
			"SOF_INT_ST, 1st cq check failed, cq_num:%d, frame_seq:%d\n",
			ctx->composed_frame_seq_no, dequeued_frame_seq_no);
			spin_unlock(&ctx->composed_buffer_list.lock);
	} else {
		is_apply = true;
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
		apply_cq(raw_dev, 0, base_addr,
			buf_entry->cq_desc_size,
			buf_entry->cq_desc_offset,
			buf_entry->sub_cq_desc_size,
			buf_entry->sub_cq_desc_offset);

		/* req_stream_data of req_cq*/

		/* update qos bw */
		switch_type = s_data->feature.switch_feature_type;
		if (switch_type == EXPOSURE_CHANGE_2_to_1 ||
			switch_type == EXPOSURE_CHANGE_3_to_1)
			mtk_cam_qos_bw_calc(ctx, s_data->raw_dmas, true);

		/* Transit state from Sensor -> CQ */
		if (ctx->sensor) {
			state_transition(state_sensor,
				E_STATE_SENSOR, E_STATE_CQ);
		}
	}

	if (mtk_cam_is_with_w_channel(ctx) && is_apply) {
		if (mtk_cam_sv_rgbw_apply_next_buffer(buf_entry->s_data) == 0)
			dev_info(raw_dev->dev, "rgbw: sv apply next buffer failed");
	}
	if (ctx->used_sv_num && is_apply) {
		if (mtk_cam_sv_apply_all_buffers(ctx) == 0)
			dev_info(raw_dev->dev, "sv apply all buffers failed");
	}
	if (ctx->used_mraw_num && is_apply) {
		if (mtk_cam_mraw_apply_all_buffers(ctx) == 0)
			dev_info(raw_dev->dev, "mraw apply all buffers failed");
	}

	return 0;
}

int mtk_cam_hdr_last_frame_start(struct mtk_raw_device *raw_dev,
			struct mtk_cam_ctx *ctx,
			struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_camsys_ctrl_state *state_temp;
	struct mtk_camsys_ctrl_state *state_switch = NULL, *state_cq = NULL;
	struct mtk_camsys_ctrl_state *state_sensor = NULL;
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	int stateidx;
	int que_cnt = 0;

	sensor_ctrl->ctx->dequeued_frame_seq_no = irq_info->frame_idx_inner;

	/* List state-queue status*/
	spin_lock(&sensor_ctrl->camsys_state_lock);
	list_for_each_entry(state_temp, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req = mtk_cam_ctrl_state_get_req(state_temp);
		req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);

		stateidx = atomic_read(&sensor_ctrl->sensor_request_seq_no) -
			   req_stream_data->frame_seq_no;
		if (stateidx < STATE_NUM_AT_SOF && stateidx > -1) {
			/* Find sensor element for last sof cq*/
			if (state_temp->estate == E_STATE_SENSOR)
				state_sensor = state_temp;
			/*Find switch element*/
			if (mtk_cam_hdr_last_frame_switch_check(state_temp,
				req_stream_data)) {
				state_switch = state_temp;
				state_sensor = state_temp;
			}
			/*Find CQ element for DCIF stagger*/
			if (state_temp->estate == E_STATE_CQ)
				state_cq = state_temp;
			dev_dbg(ctx->cam->dev,
				"[%s] STATE_CHECK [N-%d] Req:%d / State:%d\n",
				__func__, stateidx,
				req_stream_data->frame_seq_no, state_temp->estate);
		}
		/* counter for state queue*/
		que_cnt++;
	}
	spin_unlock(&sensor_ctrl->camsys_state_lock);
	/*1-exp - as normal mode*/
	if (!raw_dev->stagger_en && !state_switch) {
		mtk_camsys_raw_frame_start(raw_dev, ctx, irq_info);
		return 0;
	}
	/*HDR to Normal cam mux switch case timing will be at last sof*/
	if (state_switch) {
		req = mtk_cam_ctrl_state_get_req(state_switch);
		req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		if (STAGGER_CQ_LAST_SOF == 1 &&
			(req_stream_data->feature.switch_feature_type == EXPOSURE_CHANGE_2_to_1 ||
			req_stream_data->feature.switch_feature_type == EXPOSURE_CHANGE_3_to_1)) {
			/*if db load judgment*/
			if (atomic_read(&ctx->sensor_ctrl.initial_drop_frame_cnt) == 0 &&
				irq_info->frame_idx <= irq_info->frame_idx_inner) {
				mtk_cam_exp_switch_sensor(req_stream_data);
				mtk_camsys_exp_switch_cam_mux(raw_dev, ctx, req_stream_data);
				dev_info(ctx->cam->dev,
				"[%s] switch Req:%d / State:%d\n",
				__func__, req_stream_data->frame_seq_no, state_switch->estate);
			} else {
				dev_info(ctx->cam->dev,
				"[%s] HW delay outer_no:%d, inner_idx:%d <= processing_idx:%d\n",
				__func__, irq_info->frame_idx, irq_info->frame_idx_inner,
				atomic_read(&ctx->sensor_ctrl.isp_request_seq_no));
			}
		}
	}

	/*dcif stagger*/
	if (ctx->pipe->stagger_path == STAGGER_DCIF && state_cq) {
		req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_cq);
		if (req_stream_data->frame_seq_no == irq_info->frame_idx_inner) {
			state_transition(state_cq, E_STATE_CQ, E_STATE_INNER);
			dev_info(ctx->cam->dev,
			"[%s] dcif stagger Req:%d / State:%d\n",
			__func__, req_stream_data->frame_seq_no, state_cq->estate);
		}
	}
	/*if need apply cq at last sof*/
	hdr_apply_cq_at_last_sof(raw_dev, ctx,
		state_sensor, irq_info);

	return 0;
}

static void mtk_cam_seamless_switch_work(struct work_struct *work)
{
	struct mtk_cam_req_work *req_work = (struct mtk_cam_req_work *)work;
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *s_data;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	char *debug_str;
	struct v4l2_subdev_format fmt;
	unsigned int time_after_sof = 0;
	int ret = 0;

	s_data = mtk_cam_req_work_get_s_data(req_work);
	if (!s_data) {
		pr_info("%s mtk_cam_req_work(%p), req_stream_data(%p), dropped\n",
			__func__, req_work, s_data);
		return;
	}

	req = mtk_cam_s_data_get_req(s_data);
	if (!req) {
		pr_info("%s s_data(%p), req(%p), dropped\n",
			__func__, s_data, req);
		return;
	}
	debug_str = req->req.debug_str;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	if (!ctx) {
		pr_info("%s s_data(%p), ctx(%p), dropped\n",
			__func__, s_data, ctx);
		return;
	}
	cam = ctx->cam;
	fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	fmt.pad = 0;
	fmt.format = s_data->seninf_fmt.format;
	ret = v4l2_subdev_call(ctx->sensor, pad, set_fmt, NULL, &fmt);
	dev_dbg(cam->dev,
		"%s:ctx(%d):sd(%s):%s:seq(%d): apply sensor fmt, pad:%d set format w/h/code %d/%d/0x%x\n",
		__func__, ctx->stream_id, ctx->sensor->name, debug_str, s_data->frame_seq_no,
		fmt.pad, s_data->seninf_fmt.format.width,
		s_data->seninf_fmt.format.height,
		s_data->seninf_fmt.format.code);

	if (mtk_cam_req_frame_sync_start(req))
		dev_dbg(cam->dev, "%s:%s:ctx(%d): sensor ctrl with frame sync - start\n",
			__func__, req->req.debug_str, ctx->stream_id);

	if (s_data->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_EN) {
		v4l2_ctrl_request_setup(&req->req, s_data->sensor->ctrl_handler);
		time_after_sof =
			ktime_get_boottime_ns() / 1000000 - ctx->sensor_ctrl.sof_time;
		dev_dbg(cam->dev,
			"%s:ctx(%d):sd(%s):%s:seq(%d):[SOF+%dms] Sensor request setup\n",
			__func__, ctx->stream_id, ctx->sensor->name, debug_str,
			s_data->frame_seq_no, time_after_sof);

	}

	if (mtk_cam_req_frame_sync_end(req))
		dev_dbg(cam->dev, "%s:ctx(%d): sensor ctrl with frame sync - stop\n",
			__func__, ctx->stream_id);

	fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	fmt.pad = PAD_SINK;
	fmt.format = s_data->seninf_fmt.format;
	ret = v4l2_subdev_call(ctx->pipe->res_config.seninf,
			       pad, set_fmt, NULL, &fmt);
	dev_dbg(cam->dev,
		"%s:ctx(%d):sd(%s):%s:seq(%d): apply seninf fmt, pad:%d set format w/h/code %d/%d/0x%x\n",
		__func__, ctx->stream_id, ctx->pipe->res_config.seninf->name,
		debug_str, s_data->frame_seq_no,
		fmt.pad,
		s_data->seninf_fmt.format.width,
		s_data->seninf_fmt.format.height,
		s_data->seninf_fmt.format.code);

	fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	fmt.pad = PAD_SRC_RAW0;
	fmt.format = s_data->seninf_fmt.format;
	ret = v4l2_subdev_call(ctx->pipe->res_config.seninf,
			       pad, set_fmt, NULL, &fmt);
	dev_dbg(cam->dev,
		"%s:ctx(%d):sd(%s):%s:seq(%d): apply seninf fmt, pad:%d set format w/h/code %d/%d/0x%x\n",
		__func__, ctx->stream_id, ctx->pipe->res_config.seninf->name,
		debug_str, s_data->frame_seq_no,
		fmt.pad,
		s_data->seninf_fmt.format.width,
		s_data->seninf_fmt.format.height,
		s_data->seninf_fmt.format.code);

	dev_info(cam->dev,
		 "%s:ctx(%d):sd(%s):%s:seq(%d): sensor done\n", __func__,
		 ctx->stream_id, ctx->sensor->name, debug_str,
		 s_data->frame_seq_no);

	mtk_cam_complete_sensor_hdl(s_data);

	state_transition(&s_data->state, E_STATE_CAMMUX_OUTER_CFG, E_STATE_CAMMUX_OUTER);
	state_transition(&s_data->state, E_STATE_CAMMUX_OUTER_CFG_DELAY, E_STATE_INNER);
}

static void mtk_cam_handle_seamless_switch(struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_ctx *ctx = mtk_cam_s_data_get_ctx(s_data);
	struct mtk_cam_device *cam = ctx->cam;

	if (s_data->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_MODE_UPDATE_T1) {
		state_transition(&s_data->state, E_STATE_OUTER, E_STATE_CAMMUX_OUTER_CFG);
		INIT_WORK(&s_data->seninf_s_fmt_work.work, mtk_cam_seamless_switch_work);
		queue_work(cam->link_change_wq, &s_data->seninf_s_fmt_work.work);
	}
}

static void mtk_cam_link_change_worker(struct work_struct *work)
{
	struct mtk_cam_request *req =
		container_of(work, struct mtk_cam_request, link_work);
	mtk_cam_req_seninf_change(req);
	req->flags |= MTK_CAM_REQ_FLAG_SENINF_CHANGED;
}

// TODO(mstream): check mux switch case
static void mtk_cam_handle_mux_switch(struct mtk_raw_device *raw_src,
				      struct mtk_cam_ctx *ctx,
				      struct mtk_cam_request *req)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_raw_device *raw_dev;
	struct mtk_mraw_device *mraw_dev;
	struct mtk_camsv_device *sv_dev;
	int i, j, stream_id;


	if (!(req->ctx_used & cam->streaming_ctx & req->ctx_link_update))
		return;

	dev_info(cam->dev, "%s, req->ctx_used:0x%x, req->ctx_link_update:0x%x\n",
		 __func__, req->ctx_used, req->ctx_link_update);

	if (req->flags & MTK_CAM_REQ_FLAG_SENINF_IMMEDIATE_UPDATE) {
		for (i = 0; i < cam->max_stream_num; i++) {
			if ((req->ctx_used & 1 << i) && (req->ctx_link_update & 1 << i)) {
				stream_id = i;
				ctx = &cam->ctxs[stream_id];
				raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
				req_stream_data = mtk_cam_req_get_s_data(req, stream_id, 0);
				dev_info(cam->dev, "%s: toggle for rawi\n", __func__);

				enable_tg_db(raw_dev, 0);
				enable_tg_db(raw_dev, 1);
				toggle_db(raw_dev);

				for (j = 0; j < ctx->used_sv_num; j++) {
					sv_dev = get_camsv_dev(cam, ctx->sv_pipe[j]);
					mtk_cam_sv_toggle_tg_db(sv_dev);
					mtk_cam_sv_toggle_db(sv_dev);
				}

				for (j = 0; j < ctx->used_mraw_num; j++) {
					mraw_dev = get_mraw_dev(cam, ctx->mraw_pipe[j]);
					mtk_cam_mraw_toggle_tg_db(mraw_dev);
					mtk_cam_mraw_toggle_db(mraw_dev);
				}
			}
		}

		INIT_WORK(&req->link_work, mtk_cam_link_change_worker);
		queue_work(cam->link_change_wq, &req->link_work);
	}
}

static void mtk_cam_handle_mstream_mux_switch(struct mtk_raw_device *raw_dev,
				      struct mtk_cam_ctx *ctx,
				      struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_request *req = mtk_cam_s_data_get_req(s_data);
	struct mtk_cam_request_stream_data *mstream_s_data;
	struct mtk_cam_device *cam = ctx->cam;

	if (!(req->ctx_used & cam->streaming_ctx & req->ctx_link_update))
		return;

	mstream_s_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 1);
	if (s_data->frame_seq_no != mstream_s_data->frame_seq_no)
		return;

	mtk_cam_handle_mux_switch(raw_dev, ctx, req);
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

	dev_dbg(raw_dev->dev,
		"[M2M CQD] frame_seq_no_outer:%d composed_buffer_list.cnt:%d\n",
		frame_seq_no_outer, ctx->composed_buffer_list.cnt);

	spin_lock(&sensor_ctrl->camsys_state_lock);
	list_for_each_entry(state_entry, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req = mtk_cam_ctrl_state_get_req(state_entry);
		req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);

		if (req_stream_data->frame_seq_no == frame_seq_no_outer) {
			if (frame_seq_no_outer > atomic_read(&sensor_ctrl->isp_request_seq_no)) {
				/**
				 * outer number is 1 more from last SOF's
				 * inner number
				 */
				state_transition(state_entry, E_STATE_CQ,
						E_STATE_OUTER);

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
	int feature;
	int toggle_db_check = false;
	int type;

	/* Legacy CQ done will be always happened at frame done */
	spin_lock(&sensor_ctrl->camsys_state_lock);
	list_for_each_entry(state_entry, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_entry);
		req = req_stream_data->req;
		if (mtk_cam_is_subsample(ctx)) {
			if (raw_dev->sof_count == 0)
				state_transition(state_entry, E_STATE_SUBSPL_READY,
						E_STATE_SUBSPL_OUTER);
			if (state_entry->estate >= E_STATE_SUBSPL_SCQ &&
				state_entry->estate < E_STATE_SUBSPL_INNER) {
				state_transition(state_entry, E_STATE_SUBSPL_SCQ,
							E_STATE_SUBSPL_OUTER);
				state_transition(state_entry, E_STATE_SUBSPL_SCQ_DELAY,
							E_STATE_SUBSPL_OUTER);
				atomic_set(&sensor_ctrl->isp_enq_seq_no,
							req_stream_data->frame_seq_no);
				dev_dbg(raw_dev->dev,
						"[CQD-subsample] req:%d, CQ->OUTER state:0x%x\n",
						req_stream_data->frame_seq_no, state_entry->estate);
			}
		} else if (mtk_cam_is_time_shared(ctx)) {
			if (req_stream_data->frame_seq_no == frame_seq_no_outer &&
				frame_seq_no_outer >
				atomic_read(&sensor_ctrl->isp_request_seq_no)) {
				state_transition(state_entry, E_STATE_TS_CQ,
						E_STATE_TS_INNER);
				dev_dbg(raw_dev->dev, "[TS-SOF] ctx:%d sw trigger rawi_r2 req:%d->%d, state:0x%x\n",
						ctx->stream_id, ctx->dequeued_frame_seq_no,
						req_stream_data->frame_seq_no, state_entry->estate);
				ctx->dequeued_frame_seq_no = frame_seq_no_outer;
				writel_relaxed(RAWI_R2_TRIG, raw_dev->base + REG_CTL_RAWI_TRIG);
				raw_dev->sof_count++;
				wmb(); /* TBC */
			}
		} else if (req_stream_data->frame_seq_no == frame_seq_no_outer) {
			if (frame_seq_no_outer > atomic_read(&sensor_ctrl->isp_request_seq_no)) {
				/**
				 * outer number is 1 more from last SOF's
				 * inner number
				 */
				if (frame_seq_no_outer == 1)
					state_entry->estate = E_STATE_OUTER;
				state_transition(state_entry, E_STATE_CQ,
						 E_STATE_OUTER);
				state_transition(state_entry, E_STATE_CQ_SCQ_DELAY,
						 E_STATE_OUTER);
				state_transition(state_entry, E_STATE_SENINF,
						 E_STATE_OUTER);

				type = req_stream_data->feature.switch_feature_type;
				if (type != 0 && (!mtk_cam_is_mstream(ctx) &&
						!mtk_cam_feature_change_is_mstream(type))) {
					// check if need to tg db
					req_stream_data->feature.switch_curr_setting_done = 1;
					if (!mtk_cam_get_req_s_data(ctx, ctx->stream_id,
						req_stream_data->frame_seq_no - 1)) {
						req_stream_data->feature.switch_prev_frame_done = 1;
						dev_info(raw_dev->dev,
						"[CQD-switch] req:%d, prev frame is done\n",
						req_stream_data->frame_seq_no);
					}
					feature = req_stream_data->feature.raw_feature;
					toggle_db_check =
						req_stream_data->feature.switch_prev_frame_done &&
						req_stream_data->feature.switch_curr_setting_done;
					if (type == EXPOSURE_CHANGE_3_to_1 ||
						type == EXPOSURE_CHANGE_2_to_1)
						stagger_disable(raw_dev);
					else if (type == EXPOSURE_CHANGE_1_to_2 ||
						type == EXPOSURE_CHANGE_1_to_3)
						stagger_enable(raw_dev);
					if (STAGGER_SEAMLESS_DBLOAD_FORCE)
						dbload_force(raw_dev);
					if (toggle_db_check)
						mtk_cam_hdr_switch_toggle(ctx, feature);
					dev_dbg(raw_dev->dev,
						"[CQD-switch] req:%d type:%d\n",
						req_stream_data->frame_seq_no, type);
				}
				dev_dbg(raw_dev->dev,
					"[CQD] req:%d, CQ->OUTER state:%d\n",
					req_stream_data->frame_seq_no, state_entry->estate);
				mtk_cam_handle_seamless_switch(req_stream_data);
				feature = req_stream_data->feature.raw_feature;
				if (mtk_cam_feature_is_mstream(feature)) {
					atomic_set(&sensor_ctrl->isp_enq_seq_no,
							req_stream_data->frame_seq_no);
					mtk_cam_handle_mstream_mux_switch(raw_dev,
									  ctx,
									  req_stream_data);
				} else {
					mtk_cam_handle_mux_switch(raw_dev, ctx, req);
				}
			}
		}
	}
	spin_unlock(&sensor_ctrl->camsys_state_lock);

	/* initial CQ done */
	if (raw_dev->sof_count == 0) {
		sensor_ctrl->initial_cq_done = 1;
		req_stream_data = mtk_cam_get_req_s_data(ctx, ctx->stream_id, 1);
		type = req_stream_data->feature.switch_feature_type;
		if (type == EXPOSURE_CHANGE_2_to_1 || type == EXPOSURE_CHANGE_3_to_1
			|| type == EXPOSURE_CHANGE_1_to_2 || type == EXPOSURE_CHANGE_1_to_3) {
			mtk_camsys_exp_switch_cam_mux(raw_dev, ctx, req_stream_data);
		}

		if (req_stream_data->state.estate >= E_STATE_SENSOR ||
			!ctx->sensor) {
			mtk_cam_stream_on(raw_dev, ctx);
		} else {
			dev_dbg(raw_dev->dev,
				"[CQD] 1st sensor not set yet, req:%d, state:%d\n",
				req_stream_data->frame_seq_no, req_stream_data->state.estate);
		}
	}
}

static void mtk_camsys_raw_m2m_trigger(struct mtk_raw_device *raw_dev,
				   struct mtk_cam_ctx *ctx,
				   unsigned int frame_seq_no_outer)
{

	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_camsys_ctrl_state *state_entry;
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	bool triggered;

	if (!mtk_cam_feature_is_m2m(raw_dev->pipeline->feature_active))
		return;

	if (!mtk_cam_is_mstream_m2m(ctx))
		trigger_rawi(raw_dev, ctx, -1);

	spin_lock(&sensor_ctrl->camsys_state_lock);
	triggered = false;

	list_for_each_entry(state_entry, &sensor_ctrl->camsys_state_list,
			    state_element) {
		int s_data_idx, s_data_num;

		req = mtk_cam_ctrl_state_get_req(state_entry);
		s_data_num = req->p_data[ctx->stream_id].s_data_num;

		for (s_data_idx = 0; s_data_idx < s_data_num; s_data_idx++) {
			req_stream_data = mtk_cam_req_get_s_data(req,
								ctx->stream_id,
								s_data_idx);
			dev_dbg(raw_dev->dev,
					"s_data_idx/s_data_num:%d/%d, req_stream_data->frame_seq_no:%d",
					s_data_idx, s_data_num,
					req_stream_data->frame_seq_no);
			if (req_stream_data->frame_seq_no == frame_seq_no_outer) {
				if (mtk_cam_is_mstream_m2m(ctx)) {
					if (s_data_idx == 0) {
						toggle_db(raw_dev);
						trigger_rawi(raw_dev, ctx,
						MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER);
					} else if (s_data_idx == 1) {
						toggle_db(raw_dev);
						trigger_rawi(raw_dev, ctx,
						MTKCAM_IPI_HW_PATH_OFFLINE_M2M);
					}
				}
				/**
				 * outer number is 1 more from last SOF's
				 * inner number
				 */
				atomic_set(&sensor_ctrl->isp_request_seq_no, frame_seq_no_outer);
				state_transition(state_entry, E_STATE_OUTER,
						E_STATE_INNER);
				dev_dbg(raw_dev->dev,
					"[SW Trigger] req:%d, M2M CQ->INNER state:%d frame_seq_no:%d\n",
					req_stream_data->frame_seq_no,
					state_entry->estate,
					frame_seq_no_outer);
				triggered = true;
				break;
			}
		}
		if (triggered)
			break;
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
	struct mtk_cam_request_stream_data *s_data;

	if (!ctx->sensor) {
		dev_info(cam->dev, "%s: no sensor found in ctx:%d, req:%d",
			 __func__, ctx->stream_id, dequeued_frame_seq_no);

		return true;
	}

	spin_lock(&camsys_sensor_ctrl->camsys_state_lock);
	/**
	 * Find inner register number's request and transit to
	 * STATE_DONE_xxx
	 */
	list_for_each_entry(state_entry,
			    &camsys_sensor_ctrl->camsys_state_list,
			    state_element) {
		state_req = mtk_cam_ctrl_state_get_req(state_entry);
		s_data = mtk_cam_ctrl_state_to_req_s_data(state_entry);
		if (s_data->frame_seq_no == dequeued_frame_seq_no) {
			if (mtk_cam_is_subsample(ctx)) {
				state_transition(state_entry,
						 E_STATE_SUBSPL_INNER,
						 E_STATE_SUBSPL_DONE_NORMAL);

				dev_dbg(cam->dev, "[SWD-subspl] req:%d/state:0x%x/time:%lld\n",
					s_data->frame_seq_no, state_entry->estate,
					s_data->timestamp);
			} else if (mtk_cam_is_time_shared(ctx)) {
				state_transition(state_entry,
						 E_STATE_TS_INNER,
						 E_STATE_TS_DONE_NORMAL);

				dev_dbg(cam->dev, "[TS-SWD] ctx:%d req:%d/state:0x%x/time:%lld\n",
					ctx->stream_id, s_data->frame_seq_no,
					state_entry->estate, s_data->timestamp);
			} else {
				state_transition(state_entry,
						 E_STATE_INNER_HW_DELAY,
						 E_STATE_DONE_MISMATCH);
				state_transition(state_entry, E_STATE_INNER,
						 E_STATE_DONE_NORMAL);

				if (atomic_read(&camsys_sensor_ctrl->isp_request_seq_no) == 0)
					state_transition(state_entry,
							 E_STATE_CQ,
							 E_STATE_OUTER);

				/* mstream 2 and 1 exposure */
				if ((mtk_cam_feature_is_mstream(
					s_data->feature.raw_feature) ||
					mtk_cam_feature_is_mstream_m2m(
					s_data->feature.raw_feature)) ||
				    ctx->next_sof_mask_frame_seq_no != 0) {
					if (!mtk_cam_raw_prepare_mstream_frame_done
						(ctx, s_data)) {
						spin_unlock(&camsys_sensor_ctrl->camsys_state_lock);
						return false;
					}
				} else {
					dev_dbg(cam->dev,
						"[SWD] req:%d/state:%d/time:%lld/sync_id:%lld\n",
						s_data->frame_seq_no,
						state_entry->estate,
						s_data->timestamp,
						state_req->sync_id);
				}

				// if (state_req->sync_id != -1)
				//	imgsys_cmdq_setevent(state_req->sync_id);
			}
		}
	}
	spin_unlock(&camsys_sensor_ctrl->camsys_state_lock);

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
	struct mtk_cam_request_stream_data *req_stream_data;
	int frame_seq = dequeued_frame_seq_no + 1;

	req = mtk_cam_get_req(ctx, frame_seq);

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
			req_stream_data = mtk_cam_req_get_s_data(req, i, 0);
			if (req_stream_data->state.estate == E_STATE_CAMMUX_OUTER_CFG_DELAY) {
				/**
				 * To be move to the start of frame done hanlding
				 * INIT_WORK(&req->link_work, mtk_cam_link_change_worker);
				 * queue_work(cam->link_change_wq, &req->link_work);
				 */
				dev_info(raw_dev->dev, "Exchange streams at req(%d), update link ctx (0x%x)\n",
				frame_seq, ctx->stream_id, req->ctx_link_update);
				mtk_cam_req_seninf_change(req);
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
		dev_info(ctx->cam->dev,
			 "%s: skip frame done for stream off ctx:%d\n",
			 __func__, ctx->stream_id);
		spin_unlock(&ctx->streaming_lock);
		return;
	}
	spin_unlock(&ctx->streaming_lock);

	if (is_camsv_subdev(pipe_id) || is_mraw_subdev(pipe_id)) {
		need_dequeue = true;
	} else {
		raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
		need_dequeue =
			mtk_camsys_raw_prepare_frame_done(raw_dev, ctx,
							  frame_seq_no);
	}

	if (!need_dequeue)
		return;

	dev_dbg(ctx->cam->dev, "[%s] job done ctx-%d:pipe-%d:req(%d)\n",
		 __func__, ctx->stream_id, pipe_id, frame_seq_no);
	if (mtk_cam_dequeue_req_frame(ctx, frame_seq_no, pipe_id)) {
		mutex_lock(&ctx->cam->queue_lock);
		mtk_cam_dev_req_try_queue(ctx->cam);
		mutex_unlock(&ctx->cam->queue_lock);
		if (is_raw_subdev(pipe_id))
			mtk_camsys_raw_change_pipeline(raw_dev, ctx,
						       &ctx->sensor_ctrl,
						       frame_seq_no);
	}
}

void mtk_cam_meta1_done_work(struct work_struct *work)
{
	struct mtk_cam_req_work *meta1_done_work = (struct mtk_cam_req_work *)work;
	struct mtk_cam_request_stream_data *s_data, *s_data_ctx, *s_data_mstream;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_request *req;
	struct mtk_cam_buffer *buf;
	struct vb2_buffer *vb;
	struct mtk_cam_video_device *node;
	void *vaddr;
	bool unreliable = false;

	s_data = mtk_cam_req_work_get_s_data(meta1_done_work);
	ctx = mtk_cam_s_data_get_ctx(s_data);
	req = mtk_cam_s_data_get_req(s_data);
	s_data_ctx = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);

	dev_dbg(ctx->cam->dev, "%s: ctx:%d\n", __func__, ctx->stream_id);

	spin_lock(&ctx->streaming_lock);
	if (!ctx->streaming) {
		spin_unlock(&ctx->streaming_lock);
		dev_info(ctx->cam->dev, "%s: skip for stream off ctx:%d\n",
			 __func__, ctx->stream_id);
		return;
	}
	spin_unlock(&ctx->streaming_lock);

	if (!s_data) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d): can't get s_data\n",
			 __func__, ctx->stream_id);
		return;
	}

	if (mtk_cam_is_mstream(ctx)) {
		s_data_mstream = mtk_cam_req_get_s_data(req, ctx->stream_id, 1);

		unreliable |= (s_data->flags &
			MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_DELAYED);

		if (s_data_mstream) {
			unreliable |= (s_data_mstream->flags &
				MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_DELAYED);
		}
	}

	/* Copy the meta1 output content to user buffer */
	buf = mtk_cam_s_data_get_vbuf(s_data, MTK_RAW_META_OUT_1);
	if (!buf) {
		dev_info(ctx->cam->dev,
			 "ctx(%d): can't get MTK_RAW_META_OUT_1 buf from req(%d)\n",
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

	node = mtk_cam_vbq_to_vdev(vb->vb2_queue);

	vaddr = vb2_plane_vaddr(&buf->vbb.vb2_buf, 0);
	if (!vaddr) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d): can't get plane_vadd\n",
			 __func__, ctx->stream_id);
		return;
	}

	if (!s_data->working_buf->meta_buffer.size) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d): can't get s_data working buf\n",
			 __func__, ctx->stream_id);
		return;
	}

	MTK_CAM_TRACE_BEGIN(BASIC, "meta_copy");
	memcpy(vaddr, s_data->working_buf->meta_buffer.va,
	       s_data->working_buf->meta_buffer.size);
	MTK_CAM_TRACE_END(BASIC);

	/* Update the timestamp for the buffer*/
	mtk_cam_s_data_update_timestamp(buf, s_data_ctx);

	/* clean the stream data for req reinit case */
	mtk_cam_s_data_reset_vbuf(s_data, MTK_RAW_META_OUT_1);

	/* Let user get the buffer */
	if (unreliable)
		vb2_buffer_done(&buf->vbb.vb2_buf, VB2_BUF_STATE_ERROR);
	else
		vb2_buffer_done(&buf->vbb.vb2_buf, VB2_BUF_STATE_DONE);

	dev_dbg(ctx->cam->dev, "%s:%s: req(%d) done\n",
		 __func__, req->req.debug_str, s_data->frame_seq_no);
}

void mtk_cam_sv_work(struct work_struct *work)
{
	struct mtk_cam_req_work *sv_work = (struct mtk_cam_req_work *)work;
	struct mtk_cam_request_stream_data *s_data;
	struct mtk_cam_ctx *ctx;
	struct device *dev_sv;
	struct mtk_camsv_device *camsv_dev;
	unsigned int seq_no;
	dma_addr_t base_addr;

	s_data = mtk_cam_req_work_get_s_data(sv_work);
	ctx = mtk_cam_s_data_get_ctx(s_data);
	dev_sv = ctx->cam->sv.devs[s_data->pipe_id - MTKCAM_SUBDEV_CAMSV_START];
	camsv_dev = dev_get_drvdata(dev_sv);

	if (s_data->req->pipe_used & (1 << s_data->pipe_id)) {
		seq_no = s_data->frame_seq_no;
		base_addr = s_data->sv_frame_params.img_out.buf[0][0].iova;
		mtk_cam_sv_setup_cfg_info(camsv_dev, s_data);
		mtk_cam_sv_enquehwbuf(camsv_dev, base_addr, seq_no);
		mtk_cam_sv_vf_on(camsv_dev, 1);
	} else {
		mtk_cam_sv_vf_on(camsv_dev, 0);
	}
}

static void mtk_cam_meta1_done(struct mtk_cam_ctx *ctx,
			       unsigned int frame_seq_no,
			       unsigned int pipe_id)
{
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_req_work *meta1_done_work;
	struct mtk_camsys_sensor_ctrl *camsys_sensor_ctrl = &ctx->sensor_ctrl;

	req = mtk_cam_get_req(ctx, frame_seq_no);
	if (!req) {
		dev_info(ctx->cam->dev, "%s:ctx-%d:pipe-%d:req(%d) not found!\n",
			 __func__, ctx->stream_id, pipe_id, frame_seq_no);
		return;
	}

	req_stream_data = mtk_cam_req_get_s_data(req, pipe_id, 0);
	if (!req_stream_data) {
		dev_info(ctx->cam->dev, "%s:ctx-%d:pipe-%d:s_data not found!\n",
			 __func__, ctx->stream_id, pipe_id);
		return;
	}

	if (!(req_stream_data->flags & MTK_CAM_REQ_S_DATA_FLAG_META1_INDEPENDENT))
		return;

	/* Initial request readout will be delayed 1 frame*/
	if (ctx->sensor) {
		if (atomic_read(&camsys_sensor_ctrl->isp_request_seq_no) == 0 &&
			is_first_request_sync(ctx)) {
			dev_info(ctx->cam->dev,
				 "1st META1 done passed for initial request setting\n");
			return;
		}
	}

	meta1_done_work = &req_stream_data->meta1_done_work;
	atomic_set(&meta1_done_work->is_queued, 1);
	queue_work(ctx->frame_done_wq, &meta1_done_work->work);
}

void mtk_camsys_m2m_frame_done(struct mtk_cam_ctx *ctx,
							unsigned int frame_seq_no,
							unsigned int pipe_id)
{
	struct mtk_cam_request *req;
	struct mtk_cam_req_work *frame_done_work;
	struct mtk_cam_request_stream_data *req_stream_data;

	req_stream_data = mtk_cam_get_req_s_data(ctx, pipe_id, frame_seq_no);
	if (req_stream_data) {
		req = mtk_cam_s_data_get_req(req_stream_data);
	} else {
		dev_dbg(ctx->cam->dev, "%s:ctx-%d:pipe-%d:req(%d) not found!\n",
				__func__, ctx->stream_id, pipe_id, frame_seq_no);
		return;
	}

	if (atomic_read(&req_stream_data->frame_done_work.is_queued)) {
		dev_info(ctx->cam->dev,
			"already queue done work %d\n", req_stream_data->frame_seq_no);
		return;
	}

	atomic_set(&req_stream_data->seninf_dump_state, MTK_CAM_REQ_DBGWORK_S_FINISHED);
	atomic_set(&req_stream_data->frame_done_work.is_queued, 1);
	frame_done_work = &req_stream_data->frame_done_work;
	queue_work(ctx->frame_done_wq, &frame_done_work->work);
}

void mtk_cam_frame_done_work(struct work_struct *work)
{
	struct mtk_cam_req_work *frame_done_work = (struct mtk_cam_req_work *)work;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_ctx *ctx;

	MTK_CAM_TRACE_BEGIN(BASIC, "frame_done");

	req_stream_data = mtk_cam_req_work_get_s_data(frame_done_work);
	ctx = mtk_cam_s_data_get_ctx(req_stream_data);

	if (mtk_cam_is_m2m(ctx))
		mtk_cam_handle_m2m_frame_done(ctx,
				  req_stream_data->frame_seq_no,
				  req_stream_data->pipe_id);
	else
		mtk_cam_handle_frame_done(ctx,
				  req_stream_data->frame_seq_no,
				  req_stream_data->pipe_id);

	MTK_CAM_TRACE_END(BASIC);
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
	int i;
	int switch_type;
	int feature;

	if (mtk_cam_is_stagger(ctx) && is_raw_subdev(pipe_id)) {
		/*check if switch request 's previous frame done may trigger tg db toggle */
		req = mtk_cam_get_req(ctx, frame_seq_no + 1);
		if (req) {
			req_stream_data = mtk_cam_req_get_s_data(req, pipe_id, 0);
			switch_type = req_stream_data->feature.switch_feature_type;
			feature = req_stream_data->feature.raw_feature;
			if (switch_type &&
				!mtk_cam_feature_change_is_mstream(switch_type)) {
				req_stream_data->feature.switch_prev_frame_done = 1;
				if (req_stream_data->feature.switch_prev_frame_done &&
					req_stream_data->feature.switch_curr_setting_done)
					mtk_cam_hdr_switch_toggle(ctx, feature);
				dev_dbg(ctx->cam->dev,
					"[SWD] switch req toggle check req:%d type:%d\n",
					req_stream_data->frame_seq_no,
					switch_type);
			}
		}
		seamless_switch_check_bad_frame(ctx, frame_seq_no);
	}

	/* Initial request readout will be delayed 1 frame*/
	if (ctx->sensor) {
		if (atomic_read(&camsys_sensor_ctrl->isp_request_seq_no) == 0 &&
			is_first_request_sync(ctx) &&
			atomic_read(&camsys_sensor_ctrl->initial_drop_frame_cnt)) {
			dev_info(ctx->cam->dev,
					"1st SWD passed for initial request setting\n");
			atomic_dec(&camsys_sensor_ctrl->initial_drop_frame_cnt);
			return;
		}
	}

	req_stream_data = mtk_cam_get_req_s_data(ctx, pipe_id, frame_seq_no);
	if (req_stream_data) {
		req = mtk_cam_s_data_get_req(req_stream_data);
	} else {
		dev_info(ctx->cam->dev, "%s:ctx-%d:pipe-%d:req(%d) not found!\n",
			 __func__, ctx->stream_id, pipe_id, frame_seq_no);
		return;
	}

	if (atomic_read(&req_stream_data->frame_done_work.is_queued)) {
		dev_info(ctx->cam->dev,
			"already queue done work req:%d seq:%d pipe_id:%d\n",
			req_stream_data->frame_seq_no, frame_seq_no, pipe_id);
		return;
	}

	atomic_set(&req_stream_data->seninf_dump_state, MTK_CAM_REQ_DBGWORK_S_FINISHED);
	atomic_set(&req_stream_data->frame_done_work.is_queued, 1);
	frame_done_work = &req_stream_data->frame_done_work;
	queue_work(ctx->frame_done_wq, &frame_done_work->work);
	if (mtk_cam_is_time_shared(ctx)) {
		raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
		raw_dev->time_shared_busy = false;
		/*try set other ctx in one request first*/
		if (req->pipe_used != (1 << ctx->stream_id)) {
			struct mtk_cam_ctx *ctx_2 = NULL;
			int pipe_used_remain = req->pipe_used & (~(1 << ctx->stream_id));

			for (i = 0;  i < ctx->cam->max_stream_num; i++)
				if (pipe_used_remain == (1 << i)) {
					ctx_2 = &ctx->cam->ctxs[i];
					break;
				}

			if (!ctx_2) {
				dev_dbg(raw_dev->dev, "%s: time sharing ctx-%d deq_no(%d)\n",
				 __func__, ctx_2->stream_id, ctx_2->dequeued_frame_seq_no+1);
				mtk_camsys_ts_raw_try_set(raw_dev, ctx_2,
								ctx_2->dequeued_frame_seq_no + 1);
			}
		}
		mtk_camsys_ts_raw_try_set(raw_dev, ctx, ctx->dequeued_frame_seq_no + 1);
	}
}


void mtk_camsys_state_delete(struct mtk_cam_ctx *ctx,
				struct mtk_camsys_sensor_ctrl *sensor_ctrl,
				struct mtk_cam_request *req)
{
	struct mtk_camsys_ctrl_state *state_entry, *state_entry_prev;
	struct mtk_cam_request_stream_data *s_data;
	struct mtk_camsys_ctrl_state *req_state;
	int state_found = 0;

	if (ctx->sensor) {
		spin_lock(&sensor_ctrl->camsys_state_lock);
		list_for_each_entry_safe(state_entry, state_entry_prev,
				&sensor_ctrl->camsys_state_list,
				state_element) {
			s_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
			req_state = &s_data->state;

			if (state_entry == req_state) {
				list_del(&state_entry->state_element);
				state_found = 1;
			}

			if (mtk_cam_feature_is_mstream(
					s_data->feature.raw_feature) ||
					mtk_cam_feature_is_mstream_m2m(
					s_data->feature.raw_feature)) {
				s_data = mtk_cam_req_get_s_data(req,
								ctx->stream_id, 1);
				req_state = &s_data->state;
				if (state_entry == req_state) {
					list_del(&state_entry->state_element);
					state_found = 1;
				}
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
		int frame_idx_inner)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_camsys_ctrl_state *state_temp, *state_outer = NULL;
	struct mtk_camsys_ctrl_state *state_rec[STATE_NUM_AT_SOF];
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	int stateidx;
	int que_cnt = 0;

	/* List state-queue status*/
	spin_lock(&sensor_ctrl->camsys_state_lock);
	list_for_each_entry(state_temp,
	&sensor_ctrl->camsys_state_list, state_element) {
		req = mtk_cam_ctrl_state_get_req(state_temp);
		req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		stateidx = atomic_read(&sensor_ctrl->sensor_request_seq_no) -
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
	spin_unlock(&sensor_ctrl->camsys_state_lock);

	/* HW imcomplete case */
	if (que_cnt >= STATE_NUM_AT_SOF && state_rec[1] && state_rec[2]) {
		state_transition(state_rec[2], E_STATE_INNER, E_STATE_INNER_HW_DELAY);
		state_transition(state_rec[1], E_STATE_OUTER, E_STATE_OUTER_HW_DELAY);
		dev_dbg(camsv_dev->dev, "[SOF] HW_DELAY state\n");
		return STATE_RESULT_PASS_CQ_HW_DELAY;
	}

	/* Trigger high resolution timer to try sensor setting */
	sensor_ctrl->sof_time = ktime_get_boottime_ns() / 1000000;
	mtk_cam_sof_timer_setup(ctx);

	/* Transit outer state to inner state */
	if (state_outer != NULL) {
		req = mtk_cam_ctrl_state_get_req(state_outer);
		req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		if (req_stream_data->frame_seq_no == frame_idx_inner) {
			if (frame_idx_inner ==
				(atomic_read(&sensor_ctrl->isp_request_seq_no) + 1)) {
				state_transition(state_outer,
					E_STATE_OUTER_HW_DELAY, E_STATE_INNER_HW_DELAY);
				state_transition(state_outer, E_STATE_OUTER, E_STATE_INNER);
				atomic_set(&sensor_ctrl->isp_request_seq_no, frame_idx_inner);
				dev_dbg(camsv_dev->dev, "[SOF-DBLOAD] req:%d, OUTER->INNER state:%d\n",
						req_stream_data->frame_seq_no, state_outer->estate);
			}
		}
	}

	if (que_cnt > 0 && state_rec[0]) {
		/* CQ triggering judgment*/
		if (state_rec[0]->estate == E_STATE_SENSOR) {
			*current_state = state_rec[0];
			return STATE_RESULT_TRIGGER_CQ;
		}
	}

	return STATE_RESULT_PASS_CQ_SW_DELAY;
}

static void mtk_camsys_camsv_check_frame_done(struct mtk_cam_ctx *ctx,
	unsigned int dequeued_frame_seq_no, unsigned int pipe_id)
{
#define CHECK_STATE_DEPTH 3
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_camsys_ctrl_state *state_temp;
	struct mtk_cam_request_stream_data *req_stream_data;
	unsigned long flags;
	unsigned int seqList[CHECK_STATE_DEPTH];
	unsigned int cnt = 0;
	int i;

	if (ctx->sensor) {
		spin_lock_irqsave(&sensor_ctrl->camsys_state_lock, flags);
		list_for_each_entry(state_temp, &sensor_ctrl->camsys_state_list,
						state_element) {
			req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_temp);
			if (req_stream_data->frame_seq_no < dequeued_frame_seq_no) {
				seqList[cnt++] = req_stream_data->frame_seq_no;
				if (cnt == CHECK_STATE_DEPTH)
					break;
			}
		}
		spin_unlock_irqrestore(&sensor_ctrl->camsys_state_lock, flags);
		for (i = 0; i < cnt; i++)
			mtk_camsys_frame_done(ctx, seqList[i], pipe_id);
	}
}

static void mtk_camsys_camsv_frame_start(struct mtk_camsv_device *camsv_dev,
	struct mtk_cam_ctx *ctx, unsigned int dequeued_frame_seq_no, u64 ts_ns)
{
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
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

	/* check frame done */
	if (ctx->stream_id >= MTKCAM_SUBDEV_CAMSV_START &&
		ctx->stream_id < MTKCAM_SUBDEV_CAMSV_END) {
		mtk_camsys_camsv_check_frame_done(ctx, dequeued_frame_seq_no,
			ctx->stream_id);
	}

	if (ctx->sensor &&
		(ctx->stream_id >= MTKCAM_SUBDEV_CAMSV_START &&
		ctx->stream_id < MTKCAM_SUBDEV_CAMSV_END)) {
		state_handle_ret = mtk_camsys_camsv_state_handle(camsv_dev, sensor_ctrl,
				&current_state, dequeued_frame_seq_no);
		if (state_handle_ret != STATE_RESULT_TRIGGER_CQ)
			return;
	}

	/* Find request of this dequeued frame and check frame done */
	if (ctx->stream_id >= MTKCAM_SUBDEV_CAMSV_START &&
		ctx->stream_id < MTKCAM_SUBDEV_CAMSV_END) {
		req = mtk_cam_get_req(ctx, dequeued_frame_seq_no);
		if (req) {
			req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
			req_stream_data->timestamp = ktime_get_boottime_ns();
			req_stream_data->timestamp_mono = ktime_get_ns();
		}
	}

	/* apply next buffer */
	if (ctx->stream_id >= MTKCAM_SUBDEV_CAMSV_START &&
		ctx->stream_id < MTKCAM_SUBDEV_CAMSV_END) {
		if (mtk_cam_sv_apply_next_buffer(ctx,
			camsv_dev->id + MTKCAM_SUBDEV_CAMSV_START, ts_ns)) {
			/* Transit state from Sensor -> Outer */
			if (ctx->sensor)
				state_transition(current_state, E_STATE_SENSOR, E_STATE_OUTER);
		} else {
			dev_info(camsv_dev->dev, "sv apply next buffer failed");
		}
	} else {
		mtk_cam_sv_apply_next_buffer(ctx,
			camsv_dev->id + MTKCAM_SUBDEV_CAMSV_START, ts_ns);
	}
}

static void mtk_camsys_mraw_frame_start(struct mtk_mraw_device *mraw_dev,
	struct mtk_cam_ctx *ctx, unsigned int dequeued_frame_seq_no, u64 ts_ns)
{
	int mraw_dev_index;

	/* inner register dequeue number */
	mraw_dev_index = mtk_cam_find_mraw_dev_index(ctx, mraw_dev->id);
	if (mraw_dev_index == -1) {
		dev_dbg(mraw_dev->dev, "cannot find mraw_dev_index(%d)", mraw_dev->id);
		return;
	}
	ctx->mraw_dequeued_frame_seq_no[mraw_dev_index] = dequeued_frame_seq_no;

	mtk_cam_mraw_apply_next_buffer(ctx,
		mraw_dev->id + MTKCAM_SUBDEV_MRAW_START, ts_ns);
}

static bool mtk_camsys_is_all_cq_done(struct mtk_cam_ctx *ctx,
	unsigned int pipe_id)
{
	unsigned int all_subdevs = 0;
	bool ret = false;
	int i;

	spin_lock(&ctx->first_cq_lock);
	if (ctx->is_first_cq_done) {
		ret = true;
		spin_unlock(&ctx->first_cq_lock);
		goto EXIT;
	}

	// update cq done status
	ctx->cq_done_status |= (1 << pipe_id);

	// check cq done status
	if (ctx->used_raw_num)
		all_subdevs |= (1 << ctx->pipe->id);
	for (i = 0; i < ctx->used_mraw_num; i++)
		all_subdevs |= (1 << ctx->mraw_pipe[i]->id);
	if ((ctx->cq_done_status & all_subdevs) == all_subdevs) {
		ctx->is_first_cq_done = 1;
		ret = true;
	}
	spin_unlock(&ctx->first_cq_lock);
	dev_info(ctx->cam->dev, "[1st-CQD] all done:%d, pipe_id:%d (using raw/mraw:%d/%d)\n",
		ctx->is_first_cq_done, pipe_id, ctx->used_raw_num, ctx->used_mraw_num);
EXIT:
	return ret;
}

static int mtk_camsys_event_handle_raw(struct mtk_cam_device *cam,
				       unsigned int engine_id,
				       struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_raw_device *raw_dev;
	struct mtk_cam_ctx *ctx;

	raw_dev = dev_get_drvdata(cam->raw.devs[engine_id]);
	if (raw_dev->pipeline->feature_active & MTK_CAM_FEATURE_TIMESHARE_MASK)
		ctx = &cam->ctxs[raw_dev->time_shared_busy_ctx_id];
	else
		ctx = mtk_cam_find_ctx(cam, &raw_dev->pipeline->subdev.entity);
	if (!ctx) {
		dev_dbg(raw_dev->dev, "cannot find ctx\n");
		return -EINVAL;
	}

	/* raw's CQ done */
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_SETTING_DONE)) {

		if (mtk_cam_is_m2m(ctx)) {
			mtk_camsys_raw_m2m_cq_done(raw_dev, ctx, irq_info->frame_idx);
			mtk_camsys_raw_m2m_trigger(raw_dev, ctx, irq_info->frame_idx);
		} else {
			if (mtk_camsys_is_all_cq_done(ctx, ctx->pipe->id))
				mtk_camsys_raw_cq_done(raw_dev, ctx, irq_info->frame_idx);
		}
	}
	/* raw's subsample sensor setting */
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_SUBSAMPLE_SENSOR_SET))
		mtk_cam_submit_kwork_in_sensorctrl(ctx->sensor_ctrl.sensorsetting_wq,
				     &ctx->sensor_ctrl);

	/* raw's DMA done, we only allow AFO done here */
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_AFO_DONE))
		mtk_cam_meta1_done(ctx, ctx->dequeued_frame_seq_no, ctx->stream_id);

	/* raw's SW done */
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_DONE)) {

		if (mtk_cam_is_m2m(ctx)) {
			mtk_camsys_m2m_frame_done(ctx, irq_info->frame_idx_inner,
						  ctx->stream_id);
		} else
			mtk_camsys_frame_done(ctx, ctx->dequeued_frame_seq_no,
					      ctx->stream_id);
	}
	/* raw's SOF */
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_START)) {
		if (atomic_read(&raw_dev->vf_en) == 0) {
			dev_info(raw_dev->dev, "skip sof event when vf off\n");
			return 0;
		}

		if (mtk_cam_is_stagger(ctx)) {
			dev_dbg(raw_dev->dev, "[stagger] last frame_start\n");
			mtk_cam_hdr_last_frame_start(raw_dev, ctx, irq_info);
		} else {
			mtk_camsys_raw_frame_start(raw_dev, ctx, irq_info);
		}
	}

	return 0;
}

static int mtk_camsys_event_handle_mraw(struct mtk_cam_device *cam,
					unsigned int engine_id,
					struct mtk_camsys_irq_info *irq_info)
{
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

	return 0;
}

static int mtk_camsys_event_handle_camsv(struct mtk_cam_device *cam,
					 unsigned int engine_id,
					 struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_camsv_device *camsv_dev;
	struct mtk_cam_ctx *ctx;
	int sv_dev_index;
	unsigned int stream_id;
	unsigned int seq;

	camsv_dev = dev_get_drvdata(cam->sv.devs[engine_id]);
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

		// first exposure camsv's SOF
		if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_START)) {
			if (camsv_dev->pipeline->exp_order == 0) {
				if (ctx->pipe->stagger_path == STAGGER_DCIF) {
					raw_dev->sof_count = irq_info->frame_idx_inner;
					dev_dbg(camsv_dev->dev, "dcif/offline stagger raw sof:%d\n",
						raw_dev->sof_count);
					if (raw_dev->sof_count == 1) {
						struct mtk_camsv_device *camsv_dev_s;
						int hw_scen = mtk_raw_get_hdr_scen_id(ctx);

						camsv_dev_s = get_hdr_sv_dev(ctx, 2);
						mtk_cam_sv_dev_stream_on(ctx,
							camsv_dev_s->id, 1, hw_scen);
					}
				}
				mtk_camsys_raw_frame_start(raw_dev, ctx, irq_info);
			} else if (camsv_dev->pipeline->exp_order == 2) {
				dev_dbg(camsv_dev->dev, "dcif/offline stagger raw last sof:%d\n",
						raw_dev->sof_count);
				mtk_cam_hdr_last_frame_start(raw_dev, ctx, irq_info);
			}
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

	return 0;
}

int mtk_camsys_isr_event(struct mtk_cam_device *cam,
			 enum MTK_CAMSYS_ENGINE_TYPE engine_type,
			 unsigned int engine_id,
			 struct mtk_camsys_irq_info *irq_info)
{
	int ret = 0;

	MTK_CAM_TRACE_BEGIN(BASIC, "irq_type %d, inner %d",
			    irq_info->irq_type, irq_info->frame_idx_inner);
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

		ret = mtk_camsys_event_handle_raw(cam, engine_id, irq_info);

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

	MTK_CAM_TRACE_END(BASIC);

	return ret;
}

void mtk_cam_mstream_initial_sensor_setup(struct mtk_cam_request *initial_req,
				  struct mtk_cam_ctx *ctx)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_cam_request_stream_data *req_stream_data =
		mtk_cam_req_get_s_data(initial_req, ctx->stream_id, 1);
	sensor_ctrl->ctx = ctx;
	req_stream_data->ctx = ctx;
	mtk_cam_set_sensor_full(req_stream_data, &ctx->sensor_ctrl);
	dev_info(ctx->cam->dev, "[mstream] Initial sensor timer setup, seq_no(%d)\n",
				req_stream_data->frame_seq_no);
	if (mtk_cam_is_mstream_m2m(ctx))
		mtk_cam_initial_sensor_setup(initial_req, ctx);
}

void mtk_cam_initial_sensor_setup(struct mtk_cam_request *initial_req,
				  struct mtk_cam_ctx *ctx)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_cam_request_stream_data *req_stream_data;

	sensor_ctrl->ctx = ctx;
	req_stream_data = mtk_cam_req_get_s_data(initial_req, ctx->stream_id, 0);
	req_stream_data->ctx = ctx;
	mtk_cam_set_sensor_full(req_stream_data, &ctx->sensor_ctrl);
	if (mtk_cam_is_subsample(ctx))
		state_transition(&req_stream_data->state,
			E_STATE_READY, E_STATE_SUBSPL_READY);
	dev_info(ctx->cam->dev, "Directly setup sensor req:%d\n",
		req_stream_data->frame_seq_no);
}

static void mtk_cam_complete_hdl(struct mtk_cam_request_stream_data *s_data,
				 struct media_request_object *hdl_obj,
				 char *name)
{
	char *debug_str;
	u64 start, cost;

	debug_str = mtk_cam_s_data_get_dbg_str(s_data);

	start = ktime_get_boottime_ns();
	if (hdl_obj->ops)
		hdl_obj->ops->unbind(hdl_obj);	/* mutex used */
	else
		pr_info("%s:%s:pipe(%d):seq(%d): cannot unbind %s hd\n",
			__func__, debug_str, s_data->pipe_id,
			s_data->frame_seq_no, name);

	cost = ktime_get_boottime_ns() - start;
	if (cost > 1000000)
		pr_info("%s:%s:pipe(%d):seq(%d): complete hdl:%s, cost:%llu ns\n",
			__func__, debug_str, s_data->pipe_id,
			s_data->frame_seq_no, name, cost);
	else
		pr_debug("%s:%s:pipe(%d):seq(%d): complete hdl:%s, cost:%llu ns\n",
			 __func__, debug_str, s_data->pipe_id,
			 s_data->frame_seq_no, name, cost);

	media_request_object_complete(hdl_obj);
}

void mtk_cam_complete_sensor_hdl(struct mtk_cam_request_stream_data *s_data)
{
	if (s_data->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_EN &&
	    !(s_data->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_COMPLETE) &&
	    s_data->sensor_hdl_obj) {
		mtk_cam_complete_hdl(s_data, s_data->sensor_hdl_obj, "sensor");
		s_data->flags |= MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_COMPLETE;
	}
}

void mtk_cam_complete_raw_hdl(struct mtk_cam_request_stream_data *s_data)
{
	if ((s_data->flags & MTK_CAM_REQ_S_DATA_FLAG_RAW_HDL_EN) &&
	    !(s_data->flags & MTK_CAM_REQ_S_DATA_FLAG_RAW_HDL_COMPLETE) &&
	    s_data->raw_hdl_obj) {
		mtk_cam_complete_hdl(s_data, s_data->raw_hdl_obj, "raw");
		s_data->flags |= MTK_CAM_REQ_S_DATA_FLAG_RAW_HDL_COMPLETE;
	}
}

void mtk_cam_req_ctrl_setup(struct mtk_raw_pipeline *raw_pipe,
			    struct mtk_cam_request *req)
{
	struct mtk_cam_request_stream_data *req_stream_data;

	req_stream_data = mtk_cam_req_get_s_data(req, raw_pipe->id, 0);

	/* Setup raw pipeline's ctrls */
	if (req_stream_data->flags & MTK_CAM_REQ_S_DATA_FLAG_RAW_HDL_EN &&
	    !(req_stream_data->flags & MTK_CAM_REQ_S_DATA_FLAG_RAW_HDL_COMPLETE) &&
	    req_stream_data->raw_hdl_obj) {
		dev_dbg(raw_pipe->subdev.v4l2_dev->dev,
			"%s:%s:%s:raw ctrl set start (seq:%d)\n",
			__func__, raw_pipe->subdev.name, req->req.debug_str,
			req_stream_data->frame_seq_no);
		v4l2_ctrl_request_setup(&req->req, &raw_pipe->ctrl_handler);

		mtk_cam_complete_raw_hdl(req_stream_data);
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
		timer_ms = SENSOR_SET_DEADLINE_MS_60FPS;

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
		timer_ms = SENSOR_SET_RESERVED_MS_60FPS;

	return timer_ms;
}

int mtk_camsys_ctrl_start(struct mtk_cam_ctx *ctx)
{
	struct mtk_camsys_sensor_ctrl *camsys_sensor_ctrl = &ctx->sensor_ctrl;
	struct v4l2_subdev_frame_interval fi;
	int fps_factor = 1, sub_ratio = 0;

	if (ctx->used_raw_num) {
		fi.pad = 0;
		v4l2_set_frame_interval_which(fi, V4L2_SUBDEV_FORMAT_ACTIVE);
		v4l2_subdev_call(ctx->sensor, video, g_frame_interval, &fi);
		fps_factor = (fi.interval.numerator > 0) ?
				(fi.interval.denominator / fi.interval.numerator / 30) : 1;
		sub_ratio =
			mtk_cam_get_subsample_ratio(ctx->pipe->res_config.raw_feature);
	}

	camsys_sensor_ctrl->ctx = ctx;
	atomic_set(&camsys_sensor_ctrl->sensor_enq_seq_no, 0);
	atomic_set(&camsys_sensor_ctrl->sensor_request_seq_no, 0);
	atomic_set(&camsys_sensor_ctrl->isp_request_seq_no, 0);
	atomic_set(&camsys_sensor_ctrl->isp_enq_seq_no, 0);
	atomic_set(&camsys_sensor_ctrl->isp_update_timer_seq_no, 0);
	atomic_set(&camsys_sensor_ctrl->last_drained_seq_no, 0);
	camsys_sensor_ctrl->initial_cq_done = 0;
	camsys_sensor_ctrl->sof_time = 0;
	if (ctx->used_raw_num) {
		if (is_first_request_sync(ctx))
			atomic_set(&camsys_sensor_ctrl->initial_drop_frame_cnt,
				INITIAL_DROP_FRAME_CNT);
		else
			atomic_set(&camsys_sensor_ctrl->initial_drop_frame_cnt, 0);
	}

	camsys_sensor_ctrl->timer_req_event =
		timer_reqdrained_chk(fps_factor, sub_ratio);
	camsys_sensor_ctrl->timer_req_sensor =
		timer_setsensor(fps_factor, sub_ratio);
	if (mtk_cam_is_stagger(ctx) &&
		fps_factor == 1 && sub_ratio == 0) {
		camsys_sensor_ctrl->timer_req_event =
			SENSOR_SET_STAGGER_DEADLINE_MS;
		camsys_sensor_ctrl->timer_req_sensor =
			SENSOR_SET_STAGGER_RESERVED_MS;
	}
	INIT_LIST_HEAD(&camsys_sensor_ctrl->camsys_state_list);
	spin_lock_init(&camsys_sensor_ctrl->camsys_state_lock);
	if (ctx->sensor) {
		hrtimer_init(&camsys_sensor_ctrl->sensor_deadline_timer,
			     CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		camsys_sensor_ctrl->sensor_deadline_timer.function =
			sensor_deadline_timer_handler;
		camsys_sensor_ctrl->sensorsetting_wq = &ctx->sensor_worker;
	}
	kthread_init_work(&camsys_sensor_ctrl->work, mtk_cam_sensor_worker_in_sensorctrl);

	dev_info(ctx->cam->dev, "[%s] ctx:%d/raw_dev:0x%x drained/sensor (%d)%d/%d\n",
		__func__, ctx->stream_id, ctx->used_raw_dev, fps_factor,
		camsys_sensor_ctrl->timer_req_event, camsys_sensor_ctrl->timer_req_sensor);

	return 0;
}

void mtk_camsys_ctrl_update(struct mtk_cam_ctx *ctx, int sensor_ctrl_factor)
{
	struct mtk_camsys_sensor_ctrl *camsys_sensor_ctrl = &ctx->sensor_ctrl;
	struct v4l2_subdev_frame_interval fi;
	int fps_factor = 1, sub_ratio = 0;

	if (ctx->used_raw_num) {
		fi.pad = 0;
		if (sensor_ctrl_factor > 0) {
			fps_factor = sensor_ctrl_factor;
		} else {
			v4l2_set_frame_interval_which(fi, V4L2_SUBDEV_FORMAT_ACTIVE);
			v4l2_subdev_call(ctx->sensor, video, g_frame_interval, &fi);
			fps_factor = (fi.interval.numerator > 0) ?
					(fi.interval.denominator / fi.interval.numerator / 30) : 1;
		}
		sub_ratio =
			mtk_cam_get_subsample_ratio(ctx->pipe->res_config.raw_feature);
	}

	camsys_sensor_ctrl->timer_req_event =
		timer_reqdrained_chk(fps_factor, sub_ratio);
	camsys_sensor_ctrl->timer_req_sensor =
		timer_setsensor(fps_factor, sub_ratio);

	dev_info(ctx->cam->dev, "[%s] ctx:%d/raw_dev:0x%x drained/sensor (%d)%d/%d\n",
		__func__, ctx->stream_id, ctx->used_raw_dev, fps_factor,
		camsys_sensor_ctrl->timer_req_event, camsys_sensor_ctrl->timer_req_sensor);
}

void mtk_camsys_ctrl_stop(struct mtk_cam_ctx *ctx)
{
	struct mtk_camsys_sensor_ctrl *camsys_sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_camsys_ctrl_state *state_entry, *state_entry_prev;

	spin_lock(&camsys_sensor_ctrl->camsys_state_lock);
	list_for_each_entry_safe(state_entry, state_entry_prev,
				 &camsys_sensor_ctrl->camsys_state_list,
				 state_element) {
		list_del(&state_entry->state_element);
	}
	spin_unlock(&camsys_sensor_ctrl->camsys_state_lock);
	if (ctx->sensor) {
		hrtimer_cancel(&camsys_sensor_ctrl->sensor_deadline_timer);
		camsys_sensor_ctrl->sensorsetting_wq = NULL;
	}
	kthread_flush_work(&camsys_sensor_ctrl->work);
	if (ctx->used_raw_num)
		mtk_cam_event_eos(ctx->pipe);
	dev_info(ctx->cam->dev, "[%s] ctx:%d/raw_dev:0x%x\n",
		__func__, ctx->stream_id, ctx->used_raw_dev);
}

void mtk_cam_m2m_enter_cq_state(struct mtk_camsys_ctrl_state *ctrl_state)
{
	state_transition(ctrl_state, E_STATE_SENSOR, E_STATE_CQ);
}
