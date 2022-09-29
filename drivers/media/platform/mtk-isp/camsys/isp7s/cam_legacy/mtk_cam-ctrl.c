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
#ifdef MTK_CAM_HSF_SUPPORT
#include "mtk_cam-hsf.h"
#endif
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

static int debug_cam_ctrl;
module_param(debug_cam_ctrl, int, 0644);

#undef dev_dbg
#define dev_dbg(dev, fmt, arg...)		\
	do {					\
		if (debug_cam_ctrl >= 1)	\
			dev_info(dev, fmt,	\
				## arg);	\
	} while (0)

#define SENSOR_SET_DEADLINE_MS  18
#define SENSOR_SET_RESERVED_MS  7
#define SENSOR_SET_DEADLINE_MS_60FPS  6
#define SENSOR_SET_RESERVED_MS_60FPS  4
#define SENSOR_DELAY_GUARD_TIME_60FPS 16
#define SENSOR_SET_STAGGER_DEADLINE_MS  20
#define SENSOR_SET_STAGGER_RESERVED_MS  7
#define SENSOR_SET_EXTISP_DEADLINE_MS  18
#define SENSOR_SET_EXTISP_RESERVED_MS  7


#define STATE_NUM_AT_SOF 5
#define INITIAL_DROP_FRAME_CNT 1
#define STAGGER_SEAMLESS_DBLOAD_FORCE 1

enum MTK_CAMSYS_STATE_RESULT {
	STATE_RESULT_TRIGGER_CQ = 0,
	STATE_RESULT_PASS_CQ_INIT,
	STATE_RESULT_PASS_CQ_SW_DELAY,
	STATE_RESULT_PASS_CQ_SCQ_DELAY,
	STATE_RESULT_PASS_CQ_HW_DELAY,
};

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
void mtk_cam_event_esd_recovery(struct mtk_raw_pipeline *pipeline,
				     unsigned int frame_seq_no)
{
	struct v4l2_event event = {
		.type = V4L2_EVENT_ESD_RECOVERY,
		.u.frame_sync.frame_sequence = frame_seq_no,
	};
	v4l2_event_queue(pipeline->subdev.devnode, &event);
}
void mtk_cam_event_sensor_trigger(struct mtk_raw_pipeline *pipeline,
				     unsigned int frame_seq_no, unsigned int tg_cnt)
{
	struct mtk_cam_event_sensor_trigger data = {
		.tg_cnt = tg_cnt,
		.sensor_seq = frame_seq_no,
	};
	struct v4l2_event event = {
		.type = V4L2_EVENT_REQUEST_SENSOR_TRIGGER,
		.u.frame_sync.frame_sequence = frame_seq_no,
	};
	memcpy(event.u.data, &data, 8);
	// pr_info("preisp sensor event:(%d/%d)\n", (__u32)event.u.data[0], (__u32)event.u.data[1]);
	v4l2_event_queue(pipeline->subdev.devnode, &event);
}

void mtk_cam_event_error(struct mtk_raw_pipeline *pipeline, char *msg)
{
	struct v4l2_event event = {
		.type = V4L2_EVENT_ERROR,
	};
	if (strlen(msg) < 64) {
		memcpy(event.u.data, msg, strlen(msg));
	} else {
		memcpy(event.u.data, msg, 63);
		event.u.data[63] = '\0';
	}

	if (pipeline)
		v4l2_event_queue(pipeline->subdev.devnode, &event);
	else
		pr_info("%s: get raw_pipeline failed", __func__);
}

static void mtk_cam_sv_event_eos(struct mtk_camsv_pipeline *pipeline)
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

static void mtk_cam_sv_event_frame_sync(struct mtk_camsv_pipeline *pipeline,
				unsigned int frame_seq_no)
{
	struct v4l2_event event = {
		.type = V4L2_EVENT_FRAME_SYNC,
		.u.frame_sync.frame_sequence = frame_seq_no,
	};
	v4l2_event_queue(pipeline->subdev.devnode, &event);
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

static void mtk_cam_sv_event_request_drained(struct mtk_camsv_pipeline *pipeline)
{
	struct v4l2_event event = {
		.type = V4L2_EVENT_REQUEST_DRAINED,
	};
	MTK_CAM_TRACE(BASIC, "sv drained event id:%d",
		pipeline->id);
	v4l2_event_queue(pipeline->subdev.devnode, &event);
}

static bool mtk_cam_request_drained(struct mtk_camsys_sensor_ctrl *sensor_ctrl)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	int sensor_seq_no_next =
			atomic_read(&sensor_ctrl->sensor_request_seq_no) + 1;
	int res = 0;

	if (mtk_cam_ctx_has_raw(ctx) &&
	    mtk_cam_scen_is_subsample(&ctx->pipe->scen_active))
		sensor_seq_no_next = atomic_read(&ctx->sensor_ctrl.isp_enq_seq_no) + 1;
	spin_lock(&sensor_ctrl->drained_check_lock);
	if (mtk_cam_ctx_has_raw(ctx) &&
	    mtk_cam_scen_is_mstream_2exp_types(&ctx->pipe->scen_active)) {
		if (sensor_seq_no_next <= atomic_read(&ctx->sensor_ctrl.sensor_enq_seq_no) ||
				!ctx->trigger_next_drain)
			res = 1;
	} else {
		if (sensor_seq_no_next <= atomic_read(&ctx->sensor_ctrl.sensor_enq_seq_no))
			res = 1;
	}

	/* Send V4L2_EVENT_REQUEST_DRAINED event */
	if (res == 0) {
		atomic_set(&sensor_ctrl->last_drained_seq_no, sensor_seq_no_next);
		mtk_cam_event_request_drained(ctx->pipe);
		dev_dbg(ctx->cam->dev, "request_drained:(%d)\n",
			sensor_seq_no_next);
	}
	spin_unlock(&sensor_ctrl->drained_check_lock);
	return (res == 0);
}

static void mtk_cam_sv_request_drained(
	struct mtk_camsys_sensor_ctrl *sensor_ctrl,
	struct mtk_camsv_pipeline *pipeline)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	int sensor_seq_no_next =
			atomic_read(&sensor_ctrl->sensor_request_seq_no) + 1;
	int res = 0;

	if (mtk_cam_ctx_has_raw(ctx) &&
	    mtk_cam_scen_is_subsample(&ctx->pipe->scen_active))
		sensor_seq_no_next = atomic_read(&ctx->sensor_ctrl.isp_enq_seq_no) + 1;

	if (sensor_seq_no_next <= atomic_read(&ctx->enqueued_frame_seq_no))
		res = 1;
	/* Send V4L2_EVENT_REQUEST_DRAINED event */
	if (res == 0) {
		mtk_cam_sv_event_request_drained(pipeline);
		dev_dbg(ctx->sv_dev->dev, "request_drained:(%d)\n", sensor_seq_no_next);
	}
}

static void mtk_cam_fs_sync_frame(struct mtk_cam_ctx *ctx,
				  struct mtk_cam_request *req, int state)
{
	struct mtk_cam_device *cam;

	if (!ctx)
		return;

	if (!req)
		return;

	cam = ctx->cam;
	if (!cam)
		return;

	if (ctx->sensor &&
	    ctx->sensor->ops &&
	    ctx->sensor->ops->core &&
	    ctx->sensor->ops->core->command) {
		ctx->sensor->ops->core->command(ctx->sensor,
						V4L2_CMD_FSYNC_SYNC_FRAME_START_END,
						&state);
		dev_info(cam->dev, "%s:%s:fs_sync_frame(%d): ctxs:0x%x\n",
			__func__, req->req.debug_str, state, req->ctx_used);
	} else {
		dev_info(cam->dev,
			"%s:%s: find sensor command failed, state(%d)\n",
			__func__, req->req.debug_str, state);
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

	/* TODO: sensor_subdev->ops->core->s_ctrl */

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
	unsigned int pipe_id, ctx_cnt = 0, synced_cnt = 0;
	int i;
	bool ret = false;

	/* pick out the used ctxs */
	for (i = 0; i < cam->max_stream_num; i++) {
		if (!(1 << i & req->ctx_used))
			continue;

		sync_ctx[ctx_cnt] = &cam->ctxs[i];
		ctx_cnt++;
	}

	mutex_lock(&req->fs.op_lock);
	/* multi sensor case or sync state change */
	if (req->fs.target || req->fs.update_ctx) {
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

			if (req->fs.update_ctx & (1 << ctx->stream_id)) {
				int sync_cfg;

				sync_cfg = req->fs.update_value &
					   (1 << ctx->stream_id);
				if (ctx->synced != sync_cfg) {
					if (mtk_cam_req_frame_sync_set(req, pipe_id, !!sync_cfg))
						ctx->synced = sync_cfg;
				} else {
					dev_info(cam->dev,
						 "%s: ctx(%d): fs no need change(%d)\n",
						 __func__, ctx->stream_id,
						 sync_cfg);
				}
				req->fs.update_ctx &= ~(1 << ctx->stream_id);
			}

			if (ctx->synced)
				synced_cnt++;
		}

		/* just change frame sync state */
		if (!req->fs.target) {
			dev_info(cam->dev, "%s:%s: update ctx only\n",
				 __func__, req->req.debug_str);
			goto EXIT;
		}

		/* the prepared sensor is no enough, skip */
		/* frame sync set failed or stream off */
		if (synced_cnt < 2) {
			mtk_cam_fs_reset(&req->fs);
			dev_info(cam->dev, "%s:%s: sensor is not ready\n",
				 __func__, req->req.debug_str);
			goto EXIT;
		}

		if (ctx_cnt)
			mtk_cam_fs_sync_frame(sync_ctx[0], req, 1);

		ret = true;
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
	struct mtk_cam_ctx *sync_ctx[MTKCAM_SUBDEV_MAX];
	int i;
	unsigned int ctx_cnt = 0;
	bool ret = false;

	/* pick out the used ctxs */
	for (i = 0; i < cam->max_stream_num; i++) {
		if (!(1 << i & req->ctx_used))
			continue;

		sync_ctx[ctx_cnt] = &cam->ctxs[i];
		ctx_cnt++;
	}

	mutex_lock(&req->fs.op_lock);
	if (req->fs.target && req->fs.on_cnt) { /* check fs on */
		req->fs.off_cnt++;
		if (req->fs.on_cnt != req->fs.target ||
		    req->fs.off_cnt != req->fs.target) { /* not the last */
			goto EXIT;
		}

		if (ctx_cnt)
			mtk_cam_fs_sync_frame(sync_ctx[0], req, 0);

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

static void mtk_cam_stream_on(struct mtk_raw_device *raw_dev,
			struct mtk_cam_ctx *ctx)
{
	int i;

	spin_lock(&ctx->streaming_lock);
	if (ctx->streaming) {
		mtk_cam_sv_dev_stream_on(ctx, 1);
		for (i = 0; i < ctx->used_mraw_num; i++)
			mtk_cam_mraw_dev_stream_on(ctx,
				get_mraw_dev(ctx->cam, ctx->mraw_pipe[i]), 1);
		stream_on(raw_dev, 1);
	}
	spin_unlock(&ctx->streaming_lock);
}

bool is_first_request_sync(struct mtk_cam_ctx *ctx)
{
	if (ctx->used_raw_num != 0) {
		if (mtk_cam_scen_is_sensor_normal(&ctx->pipe->scen_active) &&
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
	int i, ret = 0;
	int tgo_pxl_mode;
	struct mtk_cam_scen scen_first_req;
	int exp_no = 1;
	struct mtk_cam_request *req = mtk_cam_s_data_get_req(s_data);
	int first_tag_idx, second_tag_idx, last_tag_idx;
	unsigned int sv_cammux_id;

	s_raw_pipe_data = mtk_cam_s_data_get_raw_pipe_data(s_data);
	if (!s_raw_pipe_data) {
		dev_info(ctx->cam->dev, "%s: failed to get raw_pipe_data (pipe:%d, seq:%d)\n",
			 __func__, s_data->pipe_id, s_data->frame_seq_no);
		return -EINVAL;
	}

	res_config = &s_raw_pipe_data->res_config;
	if (ctx->used_raw_num) {
		tgo_pxl_mode = res_config->tgo_pxl_mode_before_raw;

	dev = mtk_cam_find_raw_dev(cam, ctx->used_raw_dev);
	if (!dev) {
		dev_info(cam->dev, "streamon raw device not found\n");
		ret = -EINVAL;
		goto fail_switch_stop;
	}

	mtk_camsys_ctrl_update(ctx, 0);

	raw_dev = dev_get_drvdata(dev);

	atomic_set(&raw_dev->vf_en, 1);

	scen_first_req = *s_data->feature.scen;
	if (mtk_cam_scen_is_mstream_2exp_types(&scen_first_req)) {
		struct mtk_cam_request_stream_data *mstream_s_data;

		mstream_s_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 1);
		state_transition(&mstream_s_data->state,
				E_STATE_SENINF, E_STATE_OUTER);
	} else {
		state_transition(&s_data->state, E_STATE_SENINF, E_STATE_OUTER);
	}

	if (mtk_cam_scen_is_2_exp(&scen_first_req))
		exp_no = 2;
	else if (mtk_cam_scen_is_3_exp(&scen_first_req))
		exp_no = 3;
	else
		exp_no = 1;

	/*set cam mux camtg and pixel mode*/
	if (mtk_cam_ctx_has_raw(ctx) && mtk_cam_scen_is_sensor_stagger(&ctx->pipe->scen_active)) {
		if (mtk_cam_hw_mode_is_otf(ctx->pipe->hw_mode)) {
			int seninf_pad;

			if (mtk_cam_scen_is_2_exp(&scen_first_req))
				seninf_pad = PAD_SRC_RAW1;
			else if (mtk_cam_scen_is_3_exp(&scen_first_req))
				seninf_pad = PAD_SRC_RAW2;
			else
				seninf_pad = PAD_SRC_RAW0;

//			mtk_cam_seninf_set_camtg(s_data->seninf_old, seninf_pad, 0xFF);
			dev_info(cam->dev,
				 "%s: change camtg(src_pad:%d/camtg:%d) for stagger, exp number:%d\n",
				 __func__, seninf_pad, PipeIDtoTGIDX(raw_dev->id), exp_no);

			mtk_cam_seninf_set_camtg(s_data->seninf_new, seninf_pad,
						 PipeIDtoTGIDX(raw_dev->id));
		}
	} else if (!mtk_cam_scen_is_m2m(&scen_first_req) &&
		   !mtk_cam_scen_is_time_shared(&scen_first_req)) {
#ifdef MTK_CAM_HSF_SUPPORT
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
#endif

//			mtk_cam_seninf_set_camtg(s_data->seninf_old,
//						 PAD_SRC_RAW0, 0xFF);

			if (!(exp_no == 1 && mtk_cam_hw_is_dc(ctx))) {
				mtk_cam_seninf_set_camtg(s_data->seninf_new, PAD_SRC_RAW0,
						 PipeIDtoTGIDX(raw_dev->id));
			}
		}
	}

	if (mtk_cam_ctx_has_raw(ctx) && mtk_cam_scen_is_sensor_stagger(&ctx->pipe->scen_active)) {
		if (ctx->sv_dev) {
			if (exp_no == 2) {
				first_tag_idx = get_first_sv_tag_idx(ctx, exp_no, false);
				last_tag_idx = get_last_sv_tag_idx(ctx, exp_no, false);

				sv_cammux_id =
					mtk_cam_get_sv_cammux_id(ctx->sv_dev, first_tag_idx);
				mtk_cam_seninf_set_camtg_camsv(s_data->seninf_new,
					PAD_SRC_RAW0, sv_cammux_id, first_tag_idx);

				if (mtk_cam_hw_is_dc(ctx)) {
					sv_cammux_id =
						mtk_cam_get_sv_cammux_id(ctx->sv_dev, last_tag_idx);
					mtk_cam_seninf_set_camtg_camsv(s_data->seninf_new,
						PAD_SRC_RAW1, sv_cammux_id, last_tag_idx);
				} else if (mtk_cam_ctx_support_pure_raw_with_sv(ctx)) {
					sv_cammux_id =
						mtk_cam_get_sv_cammux_id(ctx->sv_dev, SVTAG_2);
					mtk_cam_seninf_set_camtg_camsv(s_data->seninf_new,
						PAD_SRC_RAW1, sv_cammux_id, SVTAG_2);
				}
			} else if (exp_no == 3) {
				first_tag_idx = get_first_sv_tag_idx(ctx, exp_no, false);
				second_tag_idx = get_second_sv_tag_idx(ctx, exp_no, false);
				last_tag_idx = get_last_sv_tag_idx(ctx, exp_no, false);

				sv_cammux_id =
					mtk_cam_get_sv_cammux_id(ctx->sv_dev, first_tag_idx);
				mtk_cam_seninf_set_camtg_camsv(s_data->seninf_new,
					PAD_SRC_RAW0, sv_cammux_id, first_tag_idx);

				sv_cammux_id =
					mtk_cam_get_sv_cammux_id(ctx->sv_dev, second_tag_idx);
				mtk_cam_seninf_set_camtg_camsv(s_data->seninf_new,
					PAD_SRC_RAW1, sv_cammux_id, second_tag_idx);

				sv_cammux_id =
					mtk_cam_get_sv_cammux_id(ctx->sv_dev, last_tag_idx);
				mtk_cam_seninf_set_camtg_camsv(s_data->seninf_new,
					PAD_SRC_RAW2, sv_cammux_id, last_tag_idx);

				if (mtk_cam_ctx_support_pure_raw_with_sv(ctx)) {
					sv_cammux_id =
						mtk_cam_get_sv_cammux_id(ctx->sv_dev, SVTAG_2);
					mtk_cam_seninf_set_camtg_camsv(s_data->seninf_new,
						PAD_SRC_RAW2, sv_cammux_id, SVTAG_2);
				}
			} else {
				first_tag_idx = get_first_sv_tag_idx(ctx, exp_no, false);

				if (mtk_cam_hw_is_dc(ctx)) {
					sv_cammux_id =
						mtk_cam_get_sv_cammux_id(
							ctx->sv_dev, first_tag_idx);
					mtk_cam_seninf_set_camtg_camsv(s_data->seninf_new,
						PAD_SRC_RAW0, sv_cammux_id, first_tag_idx);
				} else if (mtk_cam_ctx_support_pure_raw_with_sv(ctx)) {
					sv_cammux_id =
						mtk_cam_get_sv_cammux_id(ctx->sv_dev, SVTAG_2);
					mtk_cam_seninf_set_camtg_camsv(s_data->seninf_new,
						PAD_SRC_RAW0, sv_cammux_id, SVTAG_2);
				}
			}
		}
	} else {
		if (ctx->sv_dev) {
			for (i = SVTAG_IMG_START; i < SVTAG_IMG_END; i++) {
				if (ctx->sv_dev->enabled_tags & (1 << i)) {
					sv_cammux_id =
						mtk_cam_get_sv_cammux_id(ctx->sv_dev, i);
					mtk_cam_seninf_set_camtg_camsv(s_data->seninf_new,
						ctx->sv_dev->tag_info[i].seninf_padidx,
						sv_cammux_id, i);
				}
			}
		}
	}

	if (!(mtk_cam_ctx_has_raw(ctx) &&
	      mtk_cam_scen_is_m2m(&scen_first_req))) {
		if (ctx->sv_dev) {
			for (i = SVTAG_META_START; i < SVTAG_META_END; i++) {
				if (ctx->sv_dev->enabled_tags & (1 << i)) {
					sv_cammux_id =
						mtk_cam_get_sv_cammux_id(ctx->sv_dev, i);
					mtk_cam_seninf_set_camtg_camsv(s_data->seninf_new,
						ctx->sv_dev->tag_info[i].seninf_padidx,
						sv_cammux_id, i);
				}
			}
		}

		for (i = 0; i < ctx->used_mraw_num; i++) {
			mtk_cam_seninf_set_camtg(s_data->seninf_new,
						 ctx->mraw_pipe[i]->seninf_padidx,
						 ctx->mraw_pipe[i]->cammux_id);
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

	dev_info(ctx->cam->dev, "%s: stream on seninf:%s\n",
		 __func__, s_data->seninf_new->name);
	v4l2_subdev_call(s_data->seninf_new, video, s_stream, 1);

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
				 __func__, stream_id, req_stream_data->seninf_new->name,
				 PAD_SRC_RAW0, PipeIDtoTGIDX(raw_dev->id));

			mtk_cam_sensor_switch_start_hw(ctx, req_stream_data);

			dev_info(cam->dev, "%s: pipe(%d): update BW for %s\n",
				 __func__, stream_id, req_stream_data->seninf_new->name);

			mtk_cam_qos_bw_calc(ctx, req_stream_data->raw_dmas, false);
		}
	}

	dev_dbg(cam->dev, "%s: update DVFS\n",	 __func__);
	mutex_lock(&cam->dvfs_op_lock);
	mtk_cam_dvfs_update_clk(cam, true);
	mutex_unlock(&cam->dvfs_op_lock);

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
			mtk_ctx_watchdog_start(ctx, 4, raw_dev->id + MTKCAM_SUBDEV_RAW_START);
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
		int req_id;

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
		req_id = req_stream_data->req_id;

		dev_dbg(ctx->cam->dev,
			"%s req_id:%d exposure:%d gain:%d\n", __func__, req_id, shutter, gain);

		if (shutter > 0 && gain > 0) {
			ae.exposure.le_exposure = shutter;
			ae.gain.le_gain = gain;
			ae.req_id = req_id;

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
	struct mtk_cam_seninf_mux_setting settings[4];
	int type = req_stream_data->feature.switch_feature_type;
	int first_tag_idx, second_tag_idx, last_tag_idx;
	int first_tag_idx_w, last_tag_idx_w;
	int config_exposure_num;
	struct mtk_cam_scen *scen_active;
	int is_dc = mtk_cam_hw_mode_is_dc(ctx->pipe->hw_mode);
	bool is_rgbw;

	if (req_stream_data->feature.switch_done == 1)
		return 0;
	scen_active = &ctx->pipe->scen_active;
	config_exposure_num = mtk_cam_scen_get_max_exp_num(scen_active);
	is_rgbw = mtk_cam_scen_is_rgbw_enabled(scen_active);

	if (type != EXPOSURE_CHANGE_NONE && config_exposure_num == 3) {
		switch (type) {
		case EXPOSURE_CHANGE_3_to_2:
		case EXPOSURE_CHANGE_1_to_2:
			first_tag_idx = get_first_sv_tag_idx(ctx, 2, false);
			last_tag_idx = get_last_sv_tag_idx(ctx, 2, false);
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  =
				mtk_cam_get_sv_cammux_id(ctx->sv_dev, first_tag_idx);
			settings[0].tag_id = first_tag_idx;
			settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  = (is_dc) ?
				mtk_cam_get_sv_cammux_id(ctx->sv_dev, last_tag_idx) :
				PipeIDtoTGIDX(raw_dev->id);
			settings[1].tag_id = (is_dc) ? last_tag_idx : -1;
			settings[1].enable = 1;

			settings[2].seninf = ctx->seninf;
			settings[2].source = PAD_SRC_RAW2;
			settings[2].camtg  = -1;
			settings[2].tag_id = -1;
			settings[2].enable = 0;

			if (!is_dc && mtk_cam_ctx_support_pure_raw_with_sv(ctx)) {
				settings[3].seninf = ctx->seninf;
				settings[3].source = PAD_SRC_RAW1;
				settings[3].camtg  =
					mtk_cam_get_sv_cammux_id(ctx->sv_dev, SVTAG_2);
				settings[3].tag_id = SVTAG_2;
				settings[3].enable = 1;
			}
			break;
		case EXPOSURE_CHANGE_3_to_1:
		case EXPOSURE_CHANGE_2_to_1:
			first_tag_idx = get_first_sv_tag_idx(ctx, 1, false);
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  = (is_dc) ?
				mtk_cam_get_sv_cammux_id(ctx->sv_dev, first_tag_idx) :
				PipeIDtoTGIDX(raw_dev->id);
			settings[0].tag_id = (is_dc) ? first_tag_idx : -1;
			settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  = -1;
			settings[1].tag_id = -1;
			settings[1].enable = 0;

			settings[2].seninf = ctx->seninf;
			settings[2].source = PAD_SRC_RAW2;
			settings[2].camtg  = -1;
			settings[2].tag_id = -1;
			settings[2].enable = 0;

			if (!is_dc && mtk_cam_ctx_support_pure_raw_with_sv(ctx)) {
				settings[3].seninf = ctx->seninf;
				settings[3].source = PAD_SRC_RAW0;
				settings[3].camtg  =
					mtk_cam_get_sv_cammux_id(ctx->sv_dev, SVTAG_2);
				settings[3].tag_id = SVTAG_2;
				settings[3].enable = 1;
			}
			break;
		case EXPOSURE_CHANGE_2_to_3:
		case EXPOSURE_CHANGE_1_to_3:
			first_tag_idx = get_first_sv_tag_idx(ctx, 3, false);
			second_tag_idx = get_second_sv_tag_idx(ctx, 3, false);
			last_tag_idx = get_last_sv_tag_idx(ctx, 3, false);
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  =
				mtk_cam_get_sv_cammux_id(ctx->sv_dev, first_tag_idx);
			settings[0].tag_id = first_tag_idx;
			settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  =
				mtk_cam_get_sv_cammux_id(ctx->sv_dev, second_tag_idx);
			settings[1].tag_id = second_tag_idx;
			settings[1].enable = 1;

			settings[2].seninf = ctx->seninf;
			settings[2].source = PAD_SRC_RAW2;
			settings[2].camtg  = (is_dc) ?
				mtk_cam_get_sv_cammux_id(ctx->sv_dev, last_tag_idx) :
				PipeIDtoTGIDX(raw_dev->id);
			settings[2].tag_id = (is_dc) ? last_tag_idx : -1;
			settings[2].enable = 1;

			if (!is_dc && mtk_cam_ctx_support_pure_raw_with_sv(ctx)) {
				settings[3].seninf = ctx->seninf;
				settings[3].source = PAD_SRC_RAW2;
				settings[3].camtg  =
					mtk_cam_get_sv_cammux_id(ctx->sv_dev, SVTAG_2);
				settings[3].tag_id = SVTAG_2;
				settings[3].enable = 1;
			}
			break;
		default:
			break;
		}
		param.settings = &settings[0];
		param.num = (!is_dc && mtk_cam_ctx_support_pure_raw_with_sv(ctx)) ? 4 : 3;
		mtk_cam_seninf_streaming_mux_change(&param);
		req_stream_data->feature.switch_done = 1;
		if (!is_dc && mtk_cam_ctx_support_pure_raw_with_sv(ctx))
			dev_info(ctx->cam->dev,
				"[%s] switch Req:%d, type:%d, cam_mux[0][1][2][3]:[%d/%d/%d][%d/%d/%d][%d/%d/%d][%d/%d/%d] ts:%lu\n",
				__func__, req_stream_data->frame_seq_no, type,
				settings[0].source, settings[0].camtg, settings[0].enable,
				settings[1].source, settings[1].camtg, settings[1].enable,
				settings[2].source, settings[2].camtg, settings[2].enable,
				settings[3].source, settings[3].camtg, settings[3].enable,
				ktime_get_boottime_ns() / 1000);
		else
			dev_info(ctx->cam->dev,
				"[%s] switch Req:%d, type:%d, cam_mux[0][1][2]:[%d/%d/%d][%d/%d/%d][%d/%d/%d]\n",
				__func__, req_stream_data->frame_seq_no, type,
				settings[0].source, settings[0].camtg, settings[0].enable,
				settings[1].source, settings[1].camtg, settings[1].enable,
				settings[2].source, settings[2].camtg, settings[2].enable);
	} else if (type != EXPOSURE_CHANGE_NONE && config_exposure_num == 2) {
		switch (type) {
		case EXPOSURE_CHANGE_2_to_1:
			first_tag_idx = get_first_sv_tag_idx(ctx, 1, false);
			first_tag_idx_w = get_first_sv_tag_idx(ctx, 1, true);
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  = (is_dc) ?
				mtk_cam_get_sv_cammux_id(ctx->sv_dev, first_tag_idx) :
				PipeIDtoTGIDX(raw_dev->id);
			settings[0].tag_id = (is_dc) ? first_tag_idx : -1;
			settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  = -1;
			settings[1].tag_id = -1;
			settings[1].enable = 0;

			if (is_rgbw) {
				settings[2].seninf = ctx->seninf;
				settings[2].source = PAD_SRC_RAW_W0;
				settings[2].camtg  = (is_dc) ?
					mtk_cam_get_sv_cammux_id(ctx->sv_dev, first_tag_idx_w) :
					PipeIDtoTGIDX(raw_dev->id);
				settings[2].tag_id = (is_dc) ? first_tag_idx_w : -1;
				settings[2].enable = 1;

				settings[3].seninf = ctx->seninf;
				settings[3].source = PAD_SRC_RAW_W1;
				settings[3].camtg  = -1;
				settings[3].tag_id = -1;
				settings[3].enable = 0;
			} else if (!is_dc && mtk_cam_ctx_support_pure_raw_with_sv(ctx)) {
				settings[2].seninf = ctx->seninf;
				settings[2].source = PAD_SRC_RAW0;
				settings[2].camtg  =
					mtk_cam_get_sv_cammux_id(ctx->sv_dev, SVTAG_2);
				settings[2].tag_id = SVTAG_2;
				settings[2].enable = 1;
			}
			break;
		case EXPOSURE_CHANGE_1_to_2:
			first_tag_idx = get_first_sv_tag_idx(ctx, 2, false);
			last_tag_idx = get_last_sv_tag_idx(ctx, 2, false);
			first_tag_idx_w = get_first_sv_tag_idx(ctx, 2, true);
			last_tag_idx_w = get_last_sv_tag_idx(ctx, 2, true);
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  =
				mtk_cam_get_sv_cammux_id(ctx->sv_dev, first_tag_idx);
			settings[0].tag_id = first_tag_idx;
			settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  = (is_dc) ?
				mtk_cam_get_sv_cammux_id(ctx->sv_dev, last_tag_idx) :
				PipeIDtoTGIDX(raw_dev->id);
			settings[1].tag_id = (is_dc) ? last_tag_idx : -1;
			settings[1].enable = 1;

			if (is_rgbw) {
				settings[2].seninf = ctx->seninf;
				settings[2].source = PAD_SRC_RAW_W0;
				settings[2].camtg  =
					mtk_cam_get_sv_cammux_id(ctx->sv_dev, first_tag_idx_w);
				settings[2].tag_id = first_tag_idx_w;
				settings[2].enable = 1;

				settings[3].seninf = ctx->seninf;
				settings[3].source = PAD_SRC_RAW_W1;
				settings[3].camtg  = (is_dc) ?
					mtk_cam_get_sv_cammux_id(ctx->sv_dev, last_tag_idx_w) :
					PipeIDtoTGIDX(raw_dev->id);
				settings[3].tag_id = (is_dc) ? last_tag_idx_w : -1;
				settings[3].enable = 1;
			} else if (!is_dc && mtk_cam_ctx_support_pure_raw_with_sv(ctx)) {
				settings[2].seninf = ctx->seninf;
				settings[2].source = PAD_SRC_RAW1;
				settings[2].camtg  =
					mtk_cam_get_sv_cammux_id(ctx->sv_dev, SVTAG_2);
				settings[2].tag_id = SVTAG_2;
				settings[2].enable = 1;
			}
			break;
		default:
			break;
		}
		param.settings = &settings[0];
		if (is_rgbw)
			param.num = 4;
		else if (!is_dc && mtk_cam_ctx_support_pure_raw_with_sv(ctx))
			param.num = 3;
		else
			param.num = 2;
		mtk_cam_seninf_streaming_mux_change(&param);
		req_stream_data->feature.switch_done = 1;
		if (is_rgbw)
			dev_info(ctx->cam->dev,
				"[%s] switch Req:%d, type:%d, cam_mux[0][1][2][3]:[%d/%d/%d][%d/%d/%d][%d/%d/%d][%d/%d/%d] ts:%lu\n",
				__func__, req_stream_data->frame_seq_no, type,
				settings[0].source, settings[0].camtg, settings[0].enable,
				settings[1].source, settings[1].camtg, settings[1].enable,
				settings[2].source, settings[2].camtg, settings[2].enable,
				settings[3].source, settings[3].camtg, settings[3].enable,
				ktime_get_boottime_ns() / 1000);
		else if (!is_dc && mtk_cam_ctx_support_pure_raw_with_sv(ctx))
			dev_info(ctx->cam->dev,
				"[%s] switch Req:%d, type:%d, cam_mux[0][1][2]:[%d/%d/%d][%d/%d/%d][%d/%d/%d] ts:%lu\n",
				__func__, req_stream_data->frame_seq_no, type,
				settings[0].source, settings[0].camtg, settings[0].enable,
				settings[1].source, settings[1].camtg, settings[1].enable,
				settings[2].source, settings[2].camtg, settings[2].enable,
				ktime_get_boottime_ns() / 1000);
		else
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
	dev_info(cam->dev,
		"[%s] [SOF+%dms]] ctx:%d, req:%d\n",
		__func__, time_after_sof, ctx->stream_id, req_stream_data->frame_seq_no);

	return 0;
}

static int mtk_cam_hdr_switch_toggle(struct mtk_cam_ctx *ctx, struct mtk_cam_scen *scen)
{
	struct mtk_raw_device *raw_dev;
	struct mtk_cam_video_device *node;

	if (STAGGER_SEAMLESS_DBLOAD_FORCE)
		return 0;
	node = &ctx->pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
	raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
	enable_tg_db(raw_dev, 0);
	mtk_cam_sv_toggle_tg_db(ctx->sv_dev);
	mtk_cam_sv_toggle_db(ctx->sv_dev);
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
	req_stream_data = mtk_cam_get_req_s_data(ctx, ctx->stream_id,
		sensor_seq_no_next - 1);
	if (req_stream_data) {
		/*initial request handling*/
		if (sensor_seq_no_next == 2)
			req_stream_data->state.estate = E_STATE_SUBSPL_SENSOR;
		if (req_stream_data->state.estate < E_STATE_SUBSPL_SENSOR) {
			dev_info(cam->dev, "[%s:pass] sensor_no:%d state:0x%x\n", __func__,
					sensor_seq_no_next - 1, req_stream_data->state.estate);
			return;
		}
	}
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
	unsigned int sof_count_before_set = 0;
	unsigned int sof_count_after_set = 0;
	int sv_i;
	int is_mstream_last_exposure = 0;
	struct mtk_cam_scen scen;

	mtk_cam_scen_init(&scen);
	if (!mtk_cam_s_data_get_scen(&scen, s_data)) {
		dev_dbg(sensor_ctrl->ctx->cam->dev,
			 "%s:%d: can't get scen from s_data(%d):%p\n",
			 __func__, s_data->pipe_id,
			 s_data->frame_seq_no, s_data->feature.scen);
	}

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

	if (mtk_cam_scen_is_m2m(&scen)) {
		mtk_cam_m2m_sensor_skip(s_data);
		return;
	}
	/*sensor_worker task*/
	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	req = mtk_cam_s_data_get_req(s_data);

	if (ctx->used_raw_num) {
		raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);

		/* read sof count before sensor set */
		if (raw_dev)
			sof_count_before_set = raw_dev->sof_count;
	}

	/* Update ctx->sensor for switch sensor cases */
	if (s_data->seninf_new)
		mtk_cam_update_sensor(ctx, s_data->sensor);

	dev_dbg(cam->dev, "%s:%s:ctx(%d) req(%d):sensor try set start sof_cnt(%d)\n",
		__func__, req->req.debug_str, ctx->stream_id, s_data->frame_seq_no,
		sof_count_before_set);

	if (ctx->used_raw_num) {
		MTK_CAM_TRACE_BEGIN(BASIC, "frame_sync_start");
		if (mtk_cam_req_frame_sync_start(req))
			dev_dbg(cam->dev, "%s:%s:ctx(%d): sensor ctrl with frame sync - start\n",
				__func__, req->req.debug_str, ctx->stream_id);
		MTK_CAM_TRACE_END(BASIC); /* frame_sync_start */
	}

	if (mtk_cam_ctx_has_raw(ctx) &&
	    mtk_cam_scen_is_mstream_is_2_exp(&scen)) {
		is_mstream_last_exposure =
			mtk_cam_set_sensor_mstream_exposure(ctx, s_data);
		if (raw_dev)
			sof_count_after_set = raw_dev->sof_count;
		if (is_mstream_last_exposure && ctx->sensor_ctrl.sof_time > 0)
			time_after_sof =
				ktime_get_boottime_ns() / 1000000 - ctx->sensor_ctrl.sof_time;
	}

	/* request setup*/
	/* 1st frame sensor setting in mstream is treated like normal frame and is set with
	 * other settings like max fps.
	 * 2nd is special, only expsure is set.
	 */
	if (!(mtk_cam_ctx_has_raw(ctx) && mtk_cam_scen_is_m2m(&scen)) &&
	    !is_mstream_last_exposure) {
		if (s_data->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_EN) {
			/* event only at preisp enable */
			if (mtk_cam_scen_is_ext_isp(&scen))
				mtk_cam_event_sensor_trigger(ctx->pipe,
					s_data->frame_seq_no, ctx->sv_dev->tg_cnt);
			v4l2_ctrl_request_setup(&req->req,
						s_data->sensor->ctrl_handler);
			if (raw_dev)
				sof_count_after_set = raw_dev->sof_count;
			if (ctx->sensor_ctrl.sof_time > 0)
				time_after_sof =
					ktime_get_boottime_ns() / 1000000 -
					ctx->sensor_ctrl.sof_time;

			dev_dbg(cam->dev,
				"[SOF+%dms] Sensor request:%d[ctx:%d] setup sof_cnt(%d)\n",
				time_after_sof, s_data->frame_seq_no,
				ctx->stream_id, sof_count_after_set);

			/* update request scheduler timer while sensor fps changes */
			if (s_data->frame_seq_no ==
					atomic_read(&sensor_ctrl->isp_update_timer_seq_no)) {
				int fps_factor = s_data->req->p_data[ctx->stream_id].s_data_num;

				mtk_camsys_ctrl_update(ctx, fps_factor);
				atomic_set(&sensor_ctrl->isp_update_timer_seq_no, 0);
			}
		}
	}

	if (mtk_cam_ctx_has_raw(ctx) && mtk_cam_scen_is_subsample(&scen))
		state_transition(&s_data->state,
		E_STATE_SUBSPL_OUTER, E_STATE_SUBSPL_SENSOR);
	else if (mtk_cam_ctx_has_raw(ctx) && mtk_cam_scen_is_time_shared(&scen))
		state_transition(&s_data->state,
		E_STATE_TS_READY, E_STATE_TS_SENSOR);
	else if (mtk_cam_ctx_has_raw(ctx) && mtk_cam_scen_is_ext_isp(&scen))
		state_transition(&s_data->state,
		E_STATE_EXTISP_READY, E_STATE_EXTISP_SENSOR);
	else
		state_transition(&s_data->state,
		E_STATE_READY, E_STATE_SENSOR);

	if (mtk_cam_ctx_has_raw(ctx) && mtk_cam_scen_is_mstream_is_2_exp(&scen)) {
		if (time_after_sof >= SENSOR_DELAY_GUARD_TIME_60FPS ||
				sof_count_after_set != sof_count_before_set) {
			s_data->flags |=
				MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_DELAYED;
			pr_debug("[SOF+%dms] sensor delay req:%d[ctx:%d]\n",
				time_after_sof, s_data->frame_seq_no,
				ctx->stream_id);
		}
	}

	if (ctx->used_raw_num) {
		MTK_CAM_TRACE_BEGIN(BASIC, "frame_sync_end");
		if (mtk_cam_req_frame_sync_end(req))
			dev_dbg(cam->dev, "%s:ctx(%d): sensor ctrl with frame sync - stop\n",
				__func__, ctx->stream_id);
		MTK_CAM_TRACE_END(BASIC); /* frame_sync_end */
	}

	if (ctx->used_raw_num && raw_dev) {
		if (atomic_read(&raw_dev->vf_en) == 0 &&
			ctx->sensor_ctrl.initial_cq_done == 1 &&
			s_data->frame_seq_no == 1) {
			mtk_cam_stream_on(raw_dev, ctx);
		}
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
	if (mtk_cam_ctx_has_raw(ctx) && mtk_cam_scen_is_time_shared(&scen) &&
	    s_data->frame_seq_no == 1) {
		for (sv_i = SVTAG_IMG_START; sv_i < SVTAG_IMG_END; sv_i++) {
			if (ctx->pipe->enabled_sv_tags & (1 << sv_i))
				mtk_cam_sv_dev_pertag_stream_on(ctx, sv_i, 1);
		}
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
		if (MTK_CAM_INITIAL_REQ_SYNC == 0 && sensor_seq_no_next <= 2 &&
		    (mtk_cam_scen_is_sensor_normal(&ctx->pipe->scen_active) ||
			 mtk_cam_scen_is_sensor_stagger(&ctx->pipe->scen_active))) {
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
			    !(mtk_cam_ctx_has_raw(ctx) &&
			    mtk_cam_scen_is_sensor_stagger(&ctx->pipe->scen_active))) {
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
			} else if (mtk_cam_ctx_has_raw(ctx) &&
				mtk_cam_scen_is_ext_isp(&ctx->pipe->scen_active) &&
				state_entry->estate <= E_STATE_EXTISP_SENSOR) {
				spin_unlock(&sensor_ctrl->camsys_state_lock);
				dev_info(ctx->cam->dev,
					 "[%s] ext-isp state:0x%x (sensor/enque delay)\n",
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
		if (req->fs.target) {
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
	}

	req_stream_data =  mtk_cam_get_req_s_data(ctx, ctx->stream_id, sensor_seq_no_next);
	if (req_stream_data) {
		req = mtk_cam_s_data_get_req(req_stream_data);
		if (req->ctx_used & (1 << req_stream_data->pipe_id) &&
		    req->ctx_link_update & (1 << req_stream_data->pipe_id)) {
			if (mtk_cam_ctx_has_raw(ctx) &&
			    mtk_cam_scen_is_mstream_2exp_types(req_stream_data->feature.scen)) {
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
	if (mtk_cam_ctx_has_raw(ctx) && mtk_cam_scen_is_subsample(&ctx->pipe->scen_active))
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
	ktime_t m_kt;
	int time_after_sof = ktime_get_boottime_ns() / 1000000 -
			   sensor_ctrl->sof_time;
	bool drained_res = false;
	unsigned int tag_idx;

	sensor_ctrl->sensor_deadline_timer.function = sensor_set_handler;

	m_kt = ktime_set(0, sensor_ctrl->timer_req_sensor * 1000000);

	if (ctx->used_raw_num) {
		/* handle V4L2_EVENT_REQUEST_DRAINED event */
		drained_res = mtk_cam_request_drained(sensor_ctrl);
	}
	for (i = 0; i < ctx->used_sv_num; i++) {
		dev_dbg(ctx->sv_dev->dev, "[SOF+%dms]\n", time_after_sof);
		tag_idx = mtk_cam_get_sv_tag_index(ctx, ctx->sv_pipe[i]->id);
		/* handle V4L2_EVENT_REQUEST_DRAINED event */
		mtk_cam_sv_request_drained(sensor_ctrl,
			ctx->sv_dev->tag_info[tag_idx].sv_pipe);
	}
	if (mtk_cam_ctx_has_raw(ctx) && mtk_cam_scen_is_subsample(&ctx->pipe->scen_active)) {
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
		if (enq_no >= sen_no) {
			mtk_cam_submit_kwork_in_sensorctrl(
			sensor_ctrl->sensorsetting_wq, sensor_ctrl);
			return HRTIMER_NORESTART;
		}
		dev_dbg(cam->dev,
			"[TimerIRQ [SOF+%dms]] ctx:%d, enq:%d/sensor_enq:%d\n",
			time_after_sof, ctx->stream_id, enq_no, sen_no);
	}
	/* while drianed, using next enque timing for sensor setting*/
	if (ctx->used_raw_num) {
		if (mtk_cam_scen_is_sensor_normal(&ctx->pipe->scen_active) ||
		    mtk_cam_scen_is_mstream(&ctx->pipe->scen_active) ||
		    mtk_cam_scen_is_ext_isp(&ctx->pipe->scen_active)) {
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

static void
mtk_cam_set_hdr_timestamp_first(struct mtk_cam_request_stream_data *stream_data,
	u64 time_boot, u64 time_mono)
{
	if (!stream_data) {
		pr_info("%s sdata is null\n", __func__);
		return;
	}

	if (mtk_cam_scen_is_mstream_types(stream_data->feature.scen)) {
		switch (stream_data->feature.scen->scen.mstream.type) {
		case MTK_CAM_MSTREAM_SE_NE:
			stream_data->hdr_timestamp_cache.se = time_boot;
			stream_data->hdr_timestamp_cache.se_mono = time_mono;
			break;
		case MTK_CAM_MSTREAM_NE_SE:
		default:
			stream_data->hdr_timestamp_cache.ne = time_boot;
			stream_data->hdr_timestamp_cache.ne_mono = time_mono;
			break;
		}
	} else if (mtk_cam_scen_is_stagger_types(stream_data->feature.scen)) {
		switch (stream_data->feature.scen->scen.normal.exp_order) {
		case MTK_CAM_EXP_SE_LE:
			stream_data->hdr_timestamp_cache.se = time_boot;
			stream_data->hdr_timestamp_cache.se_mono = time_mono;
			break;
		case MTK_CAM_EXP_LE_SE:
		default:
			stream_data->hdr_timestamp_cache.ne = time_boot;
			stream_data->hdr_timestamp_cache.ne_mono = time_mono;
			break;
		}
	} else {
		dev_info(stream_data->ctx->cam->dev,
			"[%s] req:%d unsupport scenario\n",
			__func__, stream_data->frame_seq_no);
	}
	dev_dbg(stream_data->ctx->cam->dev,
			"[%s] req:%d le/ne/se:%lld/%lld/%lld\n", __func__,
			stream_data->frame_seq_no,
			stream_data->hdr_timestamp_cache.le,
			stream_data->hdr_timestamp_cache.ne,
			stream_data->hdr_timestamp_cache.se);
}

static void
mtk_cam_set_hdr_timestamp_last(struct mtk_cam_request_stream_data *stream_data,
	u64 time_boot, u64 time_mono)
{
	if (!stream_data) {
		pr_info("%s sdata is null\n", __func__);
		return;
	}

	if (mtk_cam_scen_is_mstream_types(stream_data->feature.scen)) {
		switch (stream_data->feature.scen->scen.mstream.type) {
		case MTK_CAM_MSTREAM_SE_NE:
			stream_data->hdr_timestamp_cache.ne = time_boot;
			stream_data->hdr_timestamp_cache.ne_mono = time_mono;
			break;
		case MTK_CAM_MSTREAM_NE_SE:
		default:
			stream_data->hdr_timestamp_cache.se = time_boot;
			stream_data->hdr_timestamp_cache.se_mono = time_mono;
			break;
		}
	} else if (mtk_cam_scen_is_stagger_types(stream_data->feature.scen)) {
		switch (stream_data->feature.scen->scen.normal.exp_order) {
		case MTK_CAM_EXP_SE_LE:
			stream_data->hdr_timestamp_cache.ne = time_boot;
			stream_data->hdr_timestamp_cache.ne_mono = time_mono;
			break;
		case MTK_CAM_EXP_LE_SE:
		default:
			stream_data->hdr_timestamp_cache.se = time_boot;
			stream_data->hdr_timestamp_cache.se_mono = time_mono;
			break;
		}
	} else {
		dev_info(stream_data->ctx->cam->dev,
			"[%s] req:%d unsupport scenario\n",
			__func__, stream_data->frame_seq_no);
	}
	dev_dbg(stream_data->ctx->cam->dev,
			"[%s] req:%d le/ne/se:%lld/%lld/%lld\n", __func__,
			stream_data->frame_seq_no,
			stream_data->hdr_timestamp_cache.le,
			stream_data->hdr_timestamp_cache.ne,
			stream_data->hdr_timestamp_cache.se);
}

static void
mtk_cam_set_mstream_hdr_timestamp(struct mtk_cam_ctx *ctx,
	struct mtk_cam_request_stream_data *stream_data, bool is_first,
	u64 time_boot, u64 time_mono)
{
	struct mtk_cam_request *req = NULL;
	struct mtk_cam_request_stream_data *mstream_sdata;

	if (!stream_data) {
		pr_info("%s sdata is null\n", __func__);
		return;
	}

	req = mtk_cam_get_req(ctx, stream_data->frame_seq_no);
	mstream_sdata = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);

	if (mtk_cam_scen_is_mstream_2exp_types(&ctx->pipe->scen_active)) {
		if (is_first)
			mtk_cam_set_hdr_timestamp_first(mstream_sdata,
				time_boot, time_mono);
		else {
			mtk_cam_set_hdr_timestamp_last(mstream_sdata,
				time_boot, time_mono);
		}
	} else {
		mtk_cam_set_hdr_timestamp_first(stream_data,
			time_boot, time_mono);
	}
}

static void
mtk_cam_read_hdr_timestamp(struct mtk_cam_ctx *ctx,
	struct mtk_cam_request_stream_data *stream_data)
{
	if (!stream_data) {
		pr_info("%s sdata is null\n", __func__);
		return;
	}

	if (mtk_cam_ctx_has_raw(ctx)) {
		ctx->pipe->hdr_timestamp.le =
			stream_data->hdr_timestamp_cache.le;
		ctx->pipe->hdr_timestamp.le_mono =
			stream_data->hdr_timestamp_cache.le_mono;
		ctx->pipe->hdr_timestamp.ne =
			stream_data->hdr_timestamp_cache.ne;
		ctx->pipe->hdr_timestamp.ne_mono =
			stream_data->hdr_timestamp_cache.ne_mono;
		ctx->pipe->hdr_timestamp.se =
			stream_data->hdr_timestamp_cache.se;
		ctx->pipe->hdr_timestamp.se_mono =
			stream_data->hdr_timestamp_cache.se_mono;
		dev_dbg(ctx->cam->dev,
			"[hdr timestamp to subdev pipe] req:%d le/ne/se:%lld/%lld/%lld\n",
			stream_data->frame_seq_no,
			stream_data->hdr_timestamp_cache.le,
			stream_data->hdr_timestamp_cache.ne,
			stream_data->hdr_timestamp_cache.se);
	}
}

int mtk_camsys_raw_subspl_state_handle(struct mtk_raw_device *raw_dev,
				   struct mtk_camsys_sensor_ctrl *sensor_ctrl,
		struct mtk_camsys_ctrl_state **current_state,
		struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_camsys_ctrl_state *state_temp, *state_outer = NULL;
	struct mtk_camsys_ctrl_state *state_ready = NULL;
	struct mtk_camsys_ctrl_state *state_cq = NULL;
	struct mtk_camsys_ctrl_state *state_rec[STATE_NUM_AT_SOF] = {};
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	int frame_idx_inner = irq_info->frame_idx_inner;
	int sensor_seq_no =
		atomic_read(&sensor_ctrl->sensor_request_seq_no);
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
			if (state_temp->estate == E_STATE_SUBSPL_SCQ)
				state_cq = state_temp;
			dev_dbg(raw_dev->dev,
			"[SOF-subsample] STATE_CHECK [N-%d] Req:%d / State:0x%x\n",
			stateidx, req_stream_data->frame_seq_no,
			state_rec[stateidx]->estate);
		}
		/* counter for state queue*/
		que_cnt++;
	}
	spin_unlock(&sensor_ctrl->camsys_state_lock);
	/* check if last sensor setting triggered */
	if (sensor_seq_no < frame_idx_inner) {
		/* check if cq done signal missing */
		if (que_cnt > 0 && state_cq &&
			state_cq->estate == E_STATE_SUBSPL_SCQ &&
			atomic_read(&sensor_ctrl->initial_drop_frame_cnt) == 0 &&
			irq_info->frame_idx == frame_idx_inner) {
			req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_cq);
			state_transition(state_cq, E_STATE_SUBSPL_SCQ,
					E_STATE_SUBSPL_OUTER);
			atomic_set(&sensor_ctrl->isp_enq_seq_no,
					req_stream_data->frame_seq_no);
			dev_info(raw_dev->dev, "[SOF-subsample] cq done missing (inner:%d)\n",
				frame_idx_inner);
		} else {
			dev_info(raw_dev->dev, "[%s:pass] sen_no:%d inner:%d\n",
				__func__, sensor_seq_no, frame_idx_inner);
			return STATE_RESULT_PASS_CQ_SW_DELAY;
		}
	}
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
					dev_info(raw_dev->dev, "sensor delay to SOF, pass next CQ (in:%d)\n",
						frame_idx_inner);
					return STATE_RESULT_PASS_CQ_SW_DELAY;
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

/*
 * When mtk_camsys_raw_state_handle return E_STATE_OUTER_HW_DELAY, we put
 * the inner state 's mtk_camsys_ctrl_state in current_state. In other cases,
 * we save the lastest mtk_camsys_ctrl_state already executed sensor work in
 * current_state.
 */
static int mtk_camsys_raw_state_handle(struct mtk_raw_device *raw_dev,
		struct mtk_camsys_sensor_ctrl *sensor_ctrl,
		struct mtk_camsys_ctrl_state **current_state,
		struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_camsys_ctrl_state *state_temp, *state_outer = NULL;
	struct mtk_camsys_ctrl_state *state_sensor = NULL;
	struct mtk_camsys_ctrl_state *state_inner = NULL;
	struct mtk_camsys_ctrl_state *state_rec[STATE_NUM_AT_SOF] = {};
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_request_stream_data *prev_stream_data = NULL;
	int frame_idx_inner = irq_info->frame_idx_inner;
	int stateidx;
	int que_cnt = 0;
	int write_cnt;
	int write_cnt_offset;
	u64 time_boot = ktime_get_boottime_ns();
	u64 time_mono = ktime_get_ns();
	int working_req_found = 0;
	int switch_type, i;
	unsigned int sensor_request_seq_no = atomic_read(&sensor_ctrl->sensor_request_seq_no);
	struct mtk_cam_scen scen, scen_prev;
	int skip_cq_state =
		(mtk_cam_scen_get_stagger_exp_num(&ctx->pipe->scen_active) > 1) ? true:false;

	/* List state-queue status*/
	spin_lock(&sensor_ctrl->camsys_state_lock);
	list_for_each_entry(state_temp, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_temp);

		if (req_stream_data->frame_seq_no == sensor_request_seq_no - 1)
			prev_stream_data = req_stream_data;

		stateidx = sensor_request_seq_no - req_stream_data->frame_seq_no;
		if (stateidx < STATE_NUM_AT_SOF && stateidx > -1) {
			state_rec[stateidx] = state_temp;
			if (stateidx == 0)
				working_req_found = 1;
			/* Find outer state element */
			if ((!skip_cq_state && state_temp->estate == E_STATE_CQ) ||
			    state_temp->estate == E_STATE_OUTER ||
			    state_temp->estate == E_STATE_CAMMUX_OUTER ||
			    state_temp->estate == E_STATE_OUTER_HW_DELAY) {
				if (state_outer == NULL) {
					state_outer = state_temp;
					mtk_cam_set_timestamp(req_stream_data,
						time_boot, time_mono);
					if (mtk_cam_scen_is_sensor_stagger(
						&ctx->pipe->scen_active)) {
						mtk_cam_set_hdr_timestamp_first(req_stream_data,
							time_boot, time_mono);
					}
				}
			}
			/* Find inner state element request*/
			if (state_temp->estate == E_STATE_INNER ||
			    state_temp->estate == E_STATE_INNER_HW_DELAY) {
				state_inner = state_temp;
			}
			/* Find sensor state element request*/
			if (state_temp->estate <= E_STATE_SENSOR)
				state_sensor = state_temp;

			dev_dbg(raw_dev->dev,
			"[SOF] STATE_CHECK [N-%d] Req:%d / State:%d\n",
			stateidx, req_stream_data->frame_seq_no,
			state_rec[stateidx]->estate);
		}
		/* counter for state queue*/
		que_cnt++;
	}

	if (que_cnt > 1 && (prev_stream_data == NULL || state_outer == NULL)) {
		if (que_cnt > STATE_NUM_AT_SOF)
			dev_info(raw_dev->dev,
			"[SOF] STATE_CHECK_DBG que_cnt:%d\n", que_cnt);
		for (i = 0; i < STATE_NUM_AT_SOF; i++) {
			if (state_rec[i]) {
				req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_rec[i]);
				dev_info(raw_dev->dev,
				"[SOF] STATE_CHECK_DBG [N-%d] Req:%d / State:%d\n",
				i, req_stream_data->frame_seq_no,
				state_rec[i]->estate);
			}
		}
	}
	spin_unlock(&sensor_ctrl->camsys_state_lock);
	/* HW imcomplete case */
	if (state_inner) {
		req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_inner);
		write_cnt_offset = atomic_read(&sensor_ctrl->reset_seq_no) - 1;
		write_cnt = ((atomic_read(&sensor_ctrl->isp_request_seq_no)-
					  write_cnt_offset) / 256)
					* 256 + irq_info->write_cnt;
		if (frame_idx_inner > atomic_read(&sensor_ctrl->isp_request_seq_no) ||
			atomic_read(&req_stream_data->frame_done_work.is_queued) == 1) {
			dev_info_ratelimited(raw_dev->dev, "[SOF] frame done work too late frames. req(%d),ts(%lu)\n",
				req_stream_data->frame_seq_no, irq_info->ts_ns / 1000);
		} else if (mtk_cam_ctx_has_raw(ctx) &&
			   mtk_cam_scen_is_stagger_2_exp(&ctx->pipe->scen_active)) {
			dev_dbg(raw_dev->dev, "[SOF:%d] HDR SWD over SOF case\n", frame_idx_inner);
		} else if (write_cnt >= req_stream_data->frame_seq_no - write_cnt_offset) {
			dev_info_ratelimited(raw_dev->dev, "[SOF] frame done reading lost %d frames. req(%d),ts(%lu)\n",
				write_cnt - (req_stream_data->frame_seq_no - write_cnt_offset) + 1,
				req_stream_data->frame_seq_no, irq_info->ts_ns / 1000);
			mtk_cam_set_timestamp(req_stream_data,
						      time_boot - 1000, time_mono - 1000);
			mtk_camsys_frame_done(ctx, write_cnt + write_cnt_offset, ctx->stream_id);
		} else if ((write_cnt >= req_stream_data->frame_seq_no - write_cnt_offset - 1)
			&& irq_info->fbc_cnt == 0) {
			dev_info_ratelimited(raw_dev->dev, "[SOF] frame done reading lost frames. req(%d),ts(%lu)\n",
				req_stream_data->frame_seq_no, irq_info->ts_ns / 1000);
			mtk_cam_set_timestamp(req_stream_data,
						      time_boot - 1000, time_mono - 1000);
			mtk_camsys_frame_done(ctx, write_cnt + write_cnt_offset + 1,
					      ctx->stream_id);
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
			*current_state = state_inner;
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
				state_transition(state_outer, E_STATE_CQ,
						 E_STATE_INNER);
				state_transition(state_outer, E_STATE_OUTER,
						 E_STATE_INNER);
				state_transition(state_outer, E_STATE_CAMMUX_OUTER,
						 E_STATE_INNER);
				atomic_set(&sensor_ctrl->isp_request_seq_no, frame_idx_inner);
				/* scen debug information */
				mtk_cam_scen_init(&scen);
				mtk_cam_scen_init(&scen_prev);
				if (ctx->used_raw_num) {
					mtk_cam_s_data_get_scen(&scen, req_stream_data);
					scen_prev = ctx->pipe->scen_active;
					ctx->pipe->scen_active = scen;
				}
				dev_dbg(raw_dev->dev,
					"[SOF-DBLOAD] frame_seq_no:%d, OUTER->INNER state:%d,ts:%lu, scen_active(%s), scen_active_prev(%s)\n",
					req_stream_data->frame_seq_no, state_outer->estate,
					irq_info->ts_ns / 1000,
					scen.dbg_str, scen_prev.dbg_str);
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
				req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_rec[0]);
				req_stream_data->flags |=
					MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_DELAYED;
				dev_info(raw_dev->dev, "[SOF] sensor delay(seq_no %d) ts:%lu\n",
					req_stream_data->frame_seq_no, irq_info->ts_ns / 1000);
			}

			if (state_rec[0]->estate == E_STATE_SENINF)
				dev_info(raw_dev->dev, "[SOF] sensor switch delay\n");

			/* CQ triggering judgment*/
			if (state_rec[0]->estate == E_STATE_SENSOR) {
				*current_state = state_rec[0];
				if (prev_stream_data) {
					if (prev_stream_data->state.estate < E_STATE_INNER) {
						dev_info(raw_dev->dev,
							"[SOF] previous req (state:%d) doesn't DB load\n",
							prev_stream_data->state.estate);
						return STATE_RESULT_PASS_CQ_SW_DELAY;
					} else if (prev_stream_data->state.estate != E_STATE_INNER)
						dev_info(raw_dev->dev, "[SOF] previous req frame no %d (state:%d)\n",
							prev_stream_data->frame_seq_no,
							prev_stream_data->state.estate);
				}
				return STATE_RESULT_TRIGGER_CQ;
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
	struct mtk_camsys_ctrl_state *state_rec[STATE_NUM_AT_SOF] = {};
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
	int sv_i, tag_idx = 0;

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
		/* camsv todo: need tag_idx */

		for (sv_i = MTKCAM_SUBDEV_CAMSV_END - 1;
			sv_i >= MTKCAM_SUBDEV_CAMSV_START; sv_i--) {
			if (ctx->pipe->enabled_raw & (1 << sv_i)) {
				dev_sv = cam->sv.devs[0];
				camsv_dev = dev_get_drvdata(dev_sv);
				mtk_cam_sv_enquehwbuf(camsv_dev,
				req_stream_data->frame_params.img_ins[0].buf[0].iova,
				req_stream_data->frame_seq_no, tag_idx);
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
	if (mtk_cam_scen_is_mstream_2exp_types(s_data->feature.scen)) {
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
					unsigned int dequeued_frame_seq_no,
					bool *is_first_frame)
{
	struct mtk_cam_request *req = NULL;
	struct mtk_cam_request_stream_data *s_data =
				mtk_cam_get_req_s_data(ctx, ctx->stream_id,
				dequeued_frame_seq_no);

	if (s_data)
		req = s_data->req;

	if (s_data && req) {
		ctx->trigger_next_drain = false;

		/* whether 1exp or 2exp, s_data[0] always holds scenario  */
		if (mtk_cam_scen_is_mstream_2exp_types(s_data->feature.scen)) {
			if (dequeued_frame_seq_no == ctx->next_sof_frame_seq_no) {
				dev_dbg(raw_dev->dev,
					"%s mstream [SOF] with-req frame:%d sof:%d enque_req_cnt:%d next sof frame: %d\n",
					__func__, dequeued_frame_seq_no,
					req->p_data[ctx->stream_id].req_seq,
					ctx->enqueued_request_cnt,
					ctx->next_sof_frame_seq_no);
				/* mask out next sof for supsampling */
				ctx->next_sof_mask_frame_seq_no =
					dequeued_frame_seq_no + 1;
				ctx->next_sof_frame_seq_no =
					ctx->next_sof_mask_frame_seq_no + 1;
				ctx->working_request_seq =
					req->p_data[ctx->stream_id].req_seq;
				mtk_cam_event_frame_sync(ctx->pipe,
					req->p_data[ctx->stream_id].req_seq);
				ctx->trigger_next_drain = true;
				*is_first_frame = true;
			} else if (dequeued_frame_seq_no ==
					ctx->next_sof_mask_frame_seq_no) {
				dev_dbg(raw_dev->dev, "mstream [SOF-mask] with-req frame:%d working_seq:%d, sof_cnt:%d\n",
					dequeued_frame_seq_no, ctx->working_request_seq,
					raw_dev->sof_count);
				/* after mask out, reset mask frame seq */
				ctx->next_sof_mask_frame_seq_no = 1;
			} else if (s_data->no_frame_done_cnt) {
				/* bypass if sof just sent */
				if (s_data->frame_seq_no != ctx->next_sof_mask_frame_seq_no - 1) {
					dev_dbg(raw_dev->dev, "mstream [SOF] p1done delay frame idx:%d with-req frame:%d\n",
							dequeued_frame_seq_no,
					ctx->working_request_seq);
					ctx->next_sof_mask_frame_seq_no =
							dequeued_frame_seq_no + 1;
					ctx->next_sof_frame_seq_no =
							ctx->next_sof_mask_frame_seq_no + 1;
					ctx->working_request_seq =
							req->p_data[ctx->stream_id].req_seq;
					mtk_cam_event_frame_sync(ctx->pipe,
							ctx->working_request_seq);
					ctx->trigger_next_drain = true;
					*is_first_frame = true;
				} else {
					ctx->next_sof_frame_seq_no =
							ctx->next_sof_mask_frame_seq_no;
					ctx->next_sof_mask_frame_seq_no =
							ctx->next_sof_frame_seq_no + 1;
					ctx->working_request_seq =
							req->p_data[ctx->stream_id].req_seq;
				}
			} else {
				dev_dbg(raw_dev->dev, "mstream [SOF] dup deq frame idx:%d with-req frame:%d\n",
					dequeued_frame_seq_no,
					ctx->working_request_seq);
				if (mtk_cam_ctx_has_raw(ctx) &&
				    mtk_cam_scen_is_mstream_m2m(s_data->feature.scen))
					mtk_cam_event_frame_sync(ctx->pipe,
						dequeued_frame_seq_no);
				else
					mtk_cam_event_frame_sync(ctx->pipe,
						ctx->working_request_seq);

				ctx->trigger_next_drain = true;
				*is_first_frame = true;
			}
		} else {
			/* mstream 1exp case */
			ctx->working_request_seq =
				req->p_data[ctx->stream_id].req_seq;
			mtk_cam_event_frame_sync(ctx->pipe,
				req->p_data[ctx->stream_id].req_seq);
			ctx->trigger_next_drain = true;
			ctx->next_sof_frame_seq_no =
					dequeued_frame_seq_no + 1;
			*is_first_frame = true;
			dev_dbg(raw_dev->dev,
					"%s mstream 1-exp [SOF] with-req frame:%d sof:%d enque_req_cnt:%d next sof frame:%d\n",
					__func__, dequeued_frame_seq_no,
					req->p_data[ctx->stream_id].req_seq,
					ctx->enqueued_request_cnt,
					ctx->next_sof_frame_seq_no);
		}
	} else if (dequeued_frame_seq_no ==
			ctx->next_sof_mask_frame_seq_no) {
		/* when frame request is already remove sof_done block or laggy enque case */
		dev_dbg(raw_dev->dev, "mstream [SOF-mask] req-gone frame:%d sof:%d sof_cnt:%d\n",
			ctx->next_sof_mask_frame_seq_no, ctx->working_request_seq,
			raw_dev->sof_count);
		ctx->next_sof_mask_frame_seq_no = 1;
	} else {
		/**
		 * TBC: Ryan: May we check the scen_active (last applied cq's scen here)
		 * The legacy flow to handle the case that we can't find the request from
		 * the running list is to check the feature pending
		 * (last enequeue s_data's scenario). Could we read the scen_active
		 * (scen of the previous frame)
		 */
		if (mtk_cam_ctx_has_raw(ctx) &&
		    mtk_cam_scen_is_mstream_2exp_types(&ctx->pipe->user_res.raw_res.scen)) {
			/* bypass if sof just sent */
			if (dequeued_frame_seq_no != ctx->next_sof_frame_seq_no - 2) {
				dev_dbg(raw_dev->dev, "mstream [SOF] req-gone frame:%d\n",
						ctx->working_request_seq);
				mtk_cam_event_frame_sync(ctx->pipe,
						ctx->working_request_seq);
				ctx->next_sof_mask_frame_seq_no =
						dequeued_frame_seq_no + 1;
				ctx->next_sof_frame_seq_no =
						ctx->next_sof_mask_frame_seq_no + 1;
				ctx->trigger_next_drain = true;
				*is_first_frame = true;
			} else {
				ctx->next_sof_frame_seq_no =
						dequeued_frame_seq_no + 1;
				ctx->next_sof_mask_frame_seq_no =
						ctx->next_sof_frame_seq_no + 1;
			}
		} else {
			/* except: keep report current working request sequence */
			dev_dbg(raw_dev->dev, "mstream [SOF] req-gone frame:%d\n",
					ctx->working_request_seq);
			mtk_cam_event_frame_sync(ctx->pipe,
					ctx->working_request_seq);
			ctx->trigger_next_drain = true;
			*is_first_frame = true;
		}
	}
}

void mtk_cam_m2m_try_apply_cq(struct mtk_cam_ctx *ctx)
{
	struct mtk_raw_device *raw_dev;
	struct mtk_cam_working_buf_entry *buf_entry;
	struct mtk_cam_request_stream_data *s_data;
	unsigned int next_cq_seq;

	if (!ctx)
		return;

	raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
	next_cq_seq = ctx->dequeued_frame_seq_no + 1;

	spin_lock(&ctx->using_buffer_list.lock);
	/* check composed cq */
	spin_lock(&ctx->composed_buffer_list.lock);
	if (list_empty(&ctx->composed_buffer_list.list)) {
		dev_info(raw_dev->dev,
			"[M2M] no buffer for next_cq_seq:%d, composed_frame_seq_no:%d, composed_buffer_list.cnt:%d\n",
			next_cq_seq,
			ctx->composed_frame_seq_no,
			ctx->composed_buffer_list.cnt);
	} else {
		buf_entry = list_first_entry(&ctx->composed_buffer_list.list,
					     struct mtk_cam_working_buf_entry,
					     list_entry);

		s_data = buf_entry->s_data;
		if (!s_data) {
			dev_info(raw_dev->dev,
				"[M2M] get s_data failed at next_cq_seq:%d\n",
				next_cq_seq);
			goto EXIT;
		}

		dev_dbg(raw_dev->dev,
			"[M2M] %s: next_cq_seq:%d, s_data->frame_seq_no:%d\n",
			__func__, next_cq_seq, s_data->frame_seq_no);

		if (s_data->frame_seq_no != next_cq_seq)
			goto EXIT;

		list_del(&buf_entry->list_entry);
		ctx->composed_buffer_list.cnt--;

		/* transit to processing */
		spin_lock(&ctx->processing_buffer_list.lock);
		list_add_tail(&buf_entry->list_entry,
			      &ctx->processing_buffer_list.list);
		ctx->processing_buffer_list.cnt++;
		spin_unlock(&ctx->processing_buffer_list.lock);

		apply_cq(raw_dev, 0,
			buf_entry->buffer.iova,
			buf_entry->cq_desc_size,
			buf_entry->cq_desc_offset,
			buf_entry->sub_cq_desc_size,
			buf_entry->sub_cq_desc_offset);

		/* all (raw/sv/mraw) buffer entry should be transited */
		/* from composed to processing */
		if (mtk_cam_sv_apply_all_buffers(ctx) == 0)
			dev_info(raw_dev->dev, "sv apply all buffers failed");

		if (mtk_cam_mraw_apply_all_buffers(ctx) == 0)
			dev_info(raw_dev->dev, "mraw apply all buffers failed");

		s_data->timestamp = ktime_get_boottime_ns();
		s_data->timestamp_mono = ktime_get_ns();
		mtk_cam_m2m_enter_cq_state(&s_data->state);
		dev_info(raw_dev->dev,
			"[M2M] apply cq, s_data->frame_seq_no:%d\n",
			s_data->frame_seq_no);
	}
EXIT:
	spin_unlock(&ctx->composed_buffer_list.lock);
	spin_unlock(&ctx->using_buffer_list.lock);
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
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	int que_cnt = 0;
	int dequeue_cnt;
	bool is_mstream_first = false;

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
	if (mtk_cam_ctx_has_raw(ctx) &&
	    (mtk_cam_scen_is_mstream_m2m(&ctx->pipe->scen_active) ||
	     ctx->next_sof_mask_frame_seq_no != 0)) {
		/**
		 * TBC: Ryan, In the legacy flow, it reads feature pending so it runs into
		 * mtk_cam_mstream_frame_sync when feature is switched from
		 * mstream 1 exp to 2 exp. Is it the expected flow the original design?
		 */
		mtk_cam_mstream_frame_sync(raw_dev, ctx,
				dequeued_frame_seq_no, &is_mstream_first);
	} else {
		/* normal */
		mtk_cam_event_frame_sync(ctx->pipe, dequeued_frame_seq_no);
	}

	/* assign at SOF */
	ctx->dequeued_frame_seq_no = dequeued_frame_seq_no;

	/* List state-queue status*/
	spin_lock(&sensor_ctrl->camsys_state_lock);
	list_for_each_entry(state_temp, &sensor_ctrl->camsys_state_list,
			    state_element) {
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
	/*
	 * TBC: Ryan, Can we skip to check if it is 2 exp?
	 * The legacy code checks if it is 2 exp odt mstream, however
	 * it only always run use 2 exp.
	 */
	if (mtk_cam_ctx_has_raw(ctx) && req_stream_data &&
	    mtk_cam_scen_is_mstream_m2m(req_stream_data->feature.scen) &&
	    mtk_cam_scen_is_2_exp(req_stream_data->feature.scen)) {
		if (mtk_cam_raw_prepare_mstream_frame_done(ctx, req_stream_data)) {
			dequeue_cnt = mtk_cam_dequeue_req_frame(ctx,
					dequeued_frame_seq_no, ctx->stream_id);
		} else {
			dequeue_cnt = 0;
		}
	} else {
		dequeue_cnt = mtk_cam_dequeue_req_frame(ctx,
				dequeued_frame_seq_no, ctx->stream_id);
	}

	/* apply next composed buffer */
	if (req_stream_data &&
	    mtk_cam_scen_is_m2m(req_stream_data->feature.scen))
		mtk_cam_m2m_try_apply_cq(ctx);

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
	struct mtk_camsys_ctrl_state *current_state = NULL;
	dma_addr_t base_addr;
	enum MTK_CAMSYS_STATE_RESULT state_handle_ret;
	bool is_apply = false;
	bool is_mstream_first = false;
	u64 time_boot = ktime_get_boottime_ns();
	u64 time_mono = ktime_get_ns();

	/*touch watchdog*/
	if (watchdog_scenario(ctx))
		mtk_ctx_watchdog_kick(ctx, raw_dev->id + MTKCAM_SUBDEV_RAW_START);
	/* inner register dequeue number */
	if (!(mtk_cam_ctx_has_raw(ctx) && mtk_cam_scen_is_sensor_stagger(&ctx->pipe->scen_active)))
		ctx->dequeued_frame_seq_no = dequeued_frame_seq_no;
	/* Send V4L2_EVENT_FRAME_SYNC event */
	if (mtk_cam_ctx_has_raw(ctx) &&
	    (mtk_cam_scen_is_mstream(&ctx->pipe->scen_active) ||
	     ctx->next_sof_mask_frame_seq_no != 0) &&
	    !mtk_cam_scen_is_mstream_m2m(&ctx->pipe->scen_active)) {
		/**
		 * TBC: Ryan, In the legacy flow, it reads feature pending so it runs into
		 * mtk_cam_mstream_frame_sync when feature is switched from
		 * mstream 1 exp to 2 exp. Is it the expected flow the original design?
		 */
		mtk_cam_mstream_frame_sync(raw_dev, ctx, dequeued_frame_seq_no, &is_mstream_first);
	} else {
		/* normal */
		mtk_cam_event_frame_sync(ctx->pipe, dequeued_frame_seq_no);
	}

	/* Find request of this dequeued frame */
	req_stream_data = mtk_cam_get_req_s_data(ctx, ctx->stream_id, dequeued_frame_seq_no);

	/* mstream time stamp */
	if (mtk_cam_scen_is_mstream(&ctx->pipe->scen_active)) {
		mtk_cam_set_mstream_hdr_timestamp(ctx,
			req_stream_data, is_mstream_first,
			time_boot, time_mono);
	}

	/* Detect no frame done and trigger camsys dump for debugging */
	mtk_cam_debug_detect_dequeue_failed(req_stream_data, 30, irq_info, raw_dev);
	if (ctx->sensor) {
		if (mtk_cam_ctx_has_raw(ctx) &&
		    mtk_cam_scen_is_subsample(&ctx->pipe->scen_active))
			state_handle_ret =
			mtk_camsys_raw_subspl_state_handle(raw_dev, sensor_ctrl,
						&current_state, irq_info);
		else
			state_handle_ret =
			mtk_camsys_raw_state_handle(raw_dev, sensor_ctrl,
						&current_state, irq_info);

		if (state_handle_ret == STATE_RESULT_PASS_CQ_HW_DELAY && current_state) {
			int frame_seq_next;
			struct mtk_cam_request *req_next;
			bool trigger_raw_switch = false;
			struct mtk_cam_request_stream_data *s_data_next;

			req_stream_data = mtk_cam_ctrl_state_to_req_s_data(current_state);
			/* check if we need to start raw switch */
			frame_seq_next = req_stream_data->frame_seq_no + 1;
			req_next = mtk_cam_get_req(ctx, frame_seq_next);
			if (!req_next) {
				dev_dbg(ctx->cam->dev, "%s next req (%d) not queued\n",
					__func__, frame_seq_next);
			} else {
				dev_dbg(ctx->cam->dev,
					"%s:req(%d) check: req->ctx_used:0x%x, req->ctx_link_update0x%x\n",
					__func__, frame_seq_next, req_next->ctx_used,
					req_next->ctx_link_update);
				mutex_lock(&ctx->cam->queue_lock);
				if ((req_next->ctx_used & (1 << ctx->stream_id))
				    && mtk_cam_is_nonimmediate_switch_req(req_next, ctx->stream_id))
					trigger_raw_switch = true;
				else
					dev_dbg(ctx->cam->dev, "%s next req (%d) no link stup\n",
						__func__, frame_seq_next);
				/**
				 * release the lock once we know
				 * if raw switch needs to be triggered or not here
				 */
				mutex_unlock(&ctx->cam->queue_lock);
			}

			if (trigger_raw_switch) {
				mtk_cam_req_dump(req_stream_data,
						 MTK_CAM_REQ_DUMP_DEQUEUE_FAILED,
						 "Camsys: No P1 done before raw switch",
						 false);
				s_data_next = mtk_cam_req_get_s_data(req_next, ctx->stream_id, 0);
				mtk_camsys_raw_change_pipeline(ctx, &ctx->sensor_ctrl, s_data_next);
			}
		}

		if (state_handle_ret != STATE_RESULT_TRIGGER_CQ) {
			dev_dbg(raw_dev->dev, "[SOF] CQ drop s:%d deq:%d\n",
				state_handle_ret, dequeued_frame_seq_no);
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
		req_stream_data = mtk_cam_ctrl_state_to_req_s_data(current_state);
		atomic_set(&ctx->composed_delay_seq_no, req_stream_data->frame_seq_no);
		ctx->composed_delay_sof_tsns = irq_info->ts_ns;
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
		req_stream_data = mtk_cam_ctrl_state_to_req_s_data(current_state);
		atomic_set(&ctx->composed_delay_seq_no, req_stream_data->frame_seq_no);
		ctx->composed_delay_sof_tsns = irq_info->ts_ns;
	} else {
		spin_lock(&ctx->processing_buffer_list.lock);
		is_apply = true;
		buf_entry = list_first_entry(&ctx->composed_buffer_list.list,
					     struct mtk_cam_working_buf_entry,
					     list_entry);
		list_del(&buf_entry->list_entry);
		ctx->composed_buffer_list.cnt--;
		list_add_tail(&buf_entry->list_entry,
			      &ctx->processing_buffer_list.list);
		ctx->processing_buffer_list.cnt++;
		spin_unlock(&ctx->processing_buffer_list.lock);
		spin_unlock(&ctx->composed_buffer_list.lock);
		base_addr = buf_entry->buffer.iova;
		apply_cq(raw_dev, 0, base_addr,
			buf_entry->cq_desc_size,
			buf_entry->cq_desc_offset,
			buf_entry->sub_cq_desc_size,
			buf_entry->sub_cq_desc_offset);

		/* Transit state from Sensor -> CQ */
		if (ctx->sensor) {
			/* req_stream_data of req_cq*/
			req_stream_data = mtk_cam_ctrl_state_to_req_s_data(current_state);
			/* update qos bw */
			mtk_cam_qos_bw_calc(ctx, req_stream_data->raw_dmas, false);
			if (mtk_cam_ctx_has_raw(ctx) &&
			    mtk_cam_scen_is_subsample(req_stream_data->feature.scen))
				state_transition(current_state,
				E_STATE_SUBSPL_READY, E_STATE_SUBSPL_SCQ);
			else {
				state_transition(current_state,
				E_STATE_SENSOR, E_STATE_CQ);

				if (current_state->estate != E_STATE_CQ)
					dev_info(raw_dev->dev,
						"SOF_INT_ST, state transition failed, frame_seq:%d, state:%d\n",
						req_stream_data->frame_seq_no,
						current_state->estate);
			}
			dev_dbg(raw_dev->dev,
			"SOF[ctx:%d-#%d], CQ-%d is update, composed:%d, cq_addr:0x%x, time:%lld, monotime:%lld\n",
			ctx->stream_id, dequeued_frame_seq_no, req_stream_data->frame_seq_no,
			ctx->composed_frame_seq_no, base_addr, req_stream_data->timestamp,
			req_stream_data->timestamp_mono);
		}
	}

	if (is_apply) {
		if (mtk_cam_sv_apply_all_buffers(ctx) == 0)
			dev_info(raw_dev->dev, "sv apply all buffers failed");
	}

	if (ctx->used_mraw_num && is_apply) {
		if (mtk_cam_mraw_apply_all_buffers(ctx) == 0)
			dev_info(raw_dev->dev, "mraw apply all buffers failed");
	}
}

void mtk_camsys_composed_delay_enque(struct mtk_raw_device *raw_dev,
				       struct mtk_cam_ctx *ctx,
				       struct mtk_cam_request_stream_data *req_stream_data)
{
	struct mtk_cam_working_buf_entry *buf_entry;
	dma_addr_t base_addr;
	bool is_apply = false;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	int time_after_sof = ktime_get_boottime_ns() / 1000000 -
			   ctx->composed_delay_sof_tsns / 1000000;

	if (time_after_sof > SCQ_DEADLINE_MS - 4) {
		dev_info(raw_dev->dev, "[%s] wrong timing:sof+%d(ms)\n",
				__func__, time_after_sof);
		return;
	}
	if (mtk_cam_ctx_has_raw(ctx) &&
	    mtk_cam_scen_is_subsample(req_stream_data->feature.scen)) {
		if (req_stream_data->state.estate != E_STATE_SUBSPL_READY) {
			dev_info(raw_dev->dev, "[%s] wrong state 0x%x",
				__func__, req_stream_data->state.estate);
			return;
		}
	} else {
		if (req_stream_data->state.estate != E_STATE_SENSOR) {
			dev_info(raw_dev->dev, "[%s] wrong state 0x%x",
				__func__, req_stream_data->state.estate);
			return;
		}
	}
	/* apply next composed buffer */
	spin_lock(&ctx->composed_buffer_list.lock);
	/* in case sof comes and runs out of composed buffer before */
	if (list_empty(&ctx->composed_buffer_list.list)) {
		dev_info(raw_dev->dev,
			"[%s], no buffer update, cq_num:%d, s_data:%d\n",
			__func__, ctx->composed_frame_seq_no,
			req_stream_data->frame_seq_no);
		spin_unlock(&ctx->composed_buffer_list.lock);
	} else {
		spin_lock(&ctx->processing_buffer_list.lock);
		is_apply = true;
		buf_entry = list_first_entry(&ctx->composed_buffer_list.list,
					struct mtk_cam_working_buf_entry,
					list_entry);
		list_del(&buf_entry->list_entry);
		ctx->composed_buffer_list.cnt--;
		list_add_tail(&buf_entry->list_entry,
			&ctx->processing_buffer_list.list);
		ctx->processing_buffer_list.cnt++;
		base_addr = buf_entry->buffer.iova;
		spin_lock(&sensor_ctrl->camsys_state_lock);
		apply_cq(raw_dev, 0, base_addr,
			buf_entry->cq_desc_size,
			buf_entry->cq_desc_offset,
			buf_entry->sub_cq_desc_size,
			buf_entry->sub_cq_desc_offset);

		/* Transit state from Sensor -> CQ */
		if (ctx->sensor) {
			if (mtk_cam_ctx_has_raw(ctx) &&
			    mtk_cam_scen_is_subsample(req_stream_data->feature.scen))
				state_transition(&req_stream_data->state,
				E_STATE_SUBSPL_READY, E_STATE_SUBSPL_SCQ);
			else
				state_transition(&req_stream_data->state,
				E_STATE_SENSOR, E_STATE_CQ);

			dev_info(raw_dev->dev,
			"[%s:SOF+%dms][ctx:%d], CQ-%d is update, composed:%d, cq_addr:0x%x, time:%lld, monotime:%lld\n",
			__func__, time_after_sof, ctx->stream_id, req_stream_data->frame_seq_no,
			ctx->composed_frame_seq_no, base_addr, req_stream_data->timestamp,
			req_stream_data->timestamp_mono);
		}
		spin_unlock(&sensor_ctrl->camsys_state_lock);
		spin_unlock(&ctx->processing_buffer_list.lock);
		spin_unlock(&ctx->composed_buffer_list.lock);
		if (ctx->sensor)
			/* update qos bw */
			mtk_cam_qos_bw_calc(ctx, req_stream_data->raw_dmas, false);

	}

	if (is_apply) {
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
			if (req_bad)
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
	struct mtk_cam_req_raw_pipe_data *s_raw_pipe_data;
	struct mtk_camsv_device *camsv_dev;
	dma_addr_t base_addr;
	bool is_apply = false;
	unsigned int dequeued_frame_seq_no = irq_info->frame_idx_inner;
	int i;
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

			if (ctx->component_dequeued_frame_seq_no > irq_info->frame_idx_inner) {
				s_data = mtk_cam_get_req_s_data(ctx, ctx->stream_id,
					ctx->component_dequeued_frame_seq_no);
				if (!s_data)
					return 0;
				s_raw_pipe_data = mtk_cam_s_data_get_raw_pipe_data(s_data);
				if (!s_raw_pipe_data)
					return 0;
				camsv_dev = mtk_cam_get_used_sv_dev(ctx);
				if (!camsv_dev)
					return 0;
				for (i = SVTAG_IMG_START; i < SVTAG_IMG_END; i++) {
					if (s_raw_pipe_data->enabled_sv_tags & (1 << i)) {
						mtk_cam_sv_dev_pertag_write_rcnt(camsv_dev, i);
						dev_info(raw_dev->dev,
							"[SOF-noDBLOAD] camsv_id/tag_idx:%d/%d rcnt++ (sv/raw inner: %d/%d)\n",
							camsv_dev->id, i,
							ctx->component_dequeued_frame_seq_no,
							irq_info->frame_idx_inner);
					}
				}
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

	if (is_apply) {
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
	struct mtk_camsys_ctrl_state *state_switch = NULL, *state_sensor = NULL;
	struct mtk_camsys_ctrl_state *state_cq = NULL, *state_outer = NULL;
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	int stateidx;
	int que_cnt = 0;
	u64 time_boot = ktime_get_boottime_ns();
	u64 time_mono = ktime_get_ns();

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
			/* Find outer state element */
			if (state_temp->estate == E_STATE_INNER ||
				state_temp->estate == E_STATE_INNER_HW_DELAY) {
				mtk_cam_set_timestamp(req_stream_data,
						      time_boot, time_mono);
				if (mtk_cam_scen_is_sensor_stagger(&ctx->pipe->scen_active)) {
					mtk_cam_set_hdr_timestamp_last(req_stream_data,
						time_boot, time_mono);
				}
			}
			/*Find CQ element for DCIF stagger*/
			if (state_temp->estate == E_STATE_CQ)
				state_cq = state_temp;

			/*Find CQ element for DCIF stagger*/
			if (state_temp->estate == E_STATE_OUTER)
				state_outer = state_temp;

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

	if (mtk_cam_hw_mode_is_dc(ctx->pipe->hw_mode) &&
	    mtk_cam_ctx_has_raw(ctx) &&
	    mtk_cam_scen_is_sensor_stagger(&ctx->pipe->scen_active)) {
		if (state_cq) {
			req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_cq);
			if (req_stream_data->frame_seq_no == irq_info->frame_idx_inner) {
				state_transition(state_cq, E_STATE_CQ, E_STATE_INNER);
				dev_info(ctx->cam->dev,
					"[DC Stagger] check db load frame#%d CQ->inner\n",
					req_stream_data->frame_seq_no);
			}
		}

		if (state_outer) {
			req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_outer);
			if (req_stream_data->frame_seq_no == irq_info->frame_idx_inner) {
				state_transition(state_outer, E_STATE_OUTER, E_STATE_INNER);
				dev_info(ctx->cam->dev,
					"[DC Stagger] check db load frame#%d outer->inner\n",
					req_stream_data->frame_seq_no);
			}
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

static bool mtk_cam_handle_seamless_switch(struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_ctx *ctx = mtk_cam_s_data_get_ctx(s_data);
	struct mtk_cam_device *cam = ctx->cam;
	bool ret = false;

	if (s_data->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_MODE_UPDATE_T1) {
		ret = mtk_cam_hw_mode_is_dc(ctx->pipe->hw_mode) ? true : false;
		state_transition(&s_data->state, E_STATE_OUTER, E_STATE_CAMMUX_OUTER_CFG);
		INIT_WORK(&s_data->seninf_s_fmt_work.work, mtk_cam_seamless_switch_work);
		queue_work(cam->link_change_wq, &s_data->seninf_s_fmt_work.work);
	}

	return ret;
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
	int i, j, stream_id;

	if (!(req->ctx_used & cam->streaming_ctx & req->ctx_link_update))
		return;

	dev_info(cam->dev, "%s, req->ctx_used:0x%x, req->ctx_link_update:0x%x\n",
		 __func__, req->ctx_used, req->ctx_link_update);

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

			if (ctx->sv_dev) {
				mtk_cam_sv_toggle_tg_db(ctx->sv_dev);
				mtk_cam_sv_toggle_db(ctx->sv_dev);
			}
			for (j = 0; j < ctx->used_mraw_num; j++) {
				mraw_dev = get_mraw_dev(cam, ctx->mraw_pipe[j]);
				mtk_cam_mraw_toggle_tg_db(mraw_dev);
				mtk_cam_mraw_toggle_db(mraw_dev);
			}

			INIT_WORK(&req->link_work, mtk_cam_link_change_worker);
			queue_work(cam->link_change_wq, &req->link_work);
		}
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
	struct mtk_cam_scen *scen;
	bool fixed_clklv = false;
	int toggle_db_check = false;
	int type;

	/* Legacy CQ done will be always happened at frame done */
	spin_lock(&sensor_ctrl->camsys_state_lock);
	list_for_each_entry(state_entry, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_entry);
		req = req_stream_data->req;
		if (mtk_cam_ctx_has_raw(ctx) &&
		    mtk_cam_scen_is_subsample(req_stream_data->feature.scen)) {
			if (state_entry->estate == E_STATE_SUBSPL_READY &&
				req_stream_data->frame_seq_no == 1)
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
		} else if (mtk_cam_ctx_has_raw(ctx) &&
			   mtk_cam_scen_is_time_shared(req_stream_data->feature.scen)) {
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
		} else if (mtk_cam_ctx_has_raw(ctx) &&
			   mtk_cam_scen_is_ext_isp(req_stream_data->feature.scen)) {
			if (req_stream_data->frame_seq_no ==
				frame_seq_no_outer &&
				frame_seq_no_outer >
				atomic_read(&sensor_ctrl->isp_request_seq_no)) {
				state_transition(state_entry, E_STATE_EXTISP_CQ,
						E_STATE_EXTISP_OUTER);
				dev_info(raw_dev->dev, "[CQD-EXT-ISP] ctx:%d req:%d, state:0x%x\n",
					ctx->stream_id,
					req_stream_data->frame_seq_no, state_entry->estate);
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
				state_transition(state_entry, E_STATE_SENINF,
						 E_STATE_OUTER);

				type = req_stream_data->feature.switch_feature_type;
				/**
				 * TBC: Rayn, Yu-ming: May we just check if it is stagger scenario?
				 * I added some utility to check if it is in stagger
				 * scenarios or not
				 * (We can know it is in stagger /odt stagger/ VSE stagger
				 * regardless 1 exp or 2 exp)
				 */
				if (type != 0 && mtk_cam_ctx_has_raw(ctx) &&
				    (mtk_cam_scen_is_stagger_types(&ctx->pipe->scen_active) &&
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
					scen = req_stream_data->feature.scen;
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
						mtk_cam_hdr_switch_toggle(ctx, scen);
					dev_dbg(raw_dev->dev,
						"[CQD-switch] req:%d type:%d\n",
						req_stream_data->frame_seq_no, type);
				}
				dev_dbg(raw_dev->dev,
					"[CQD] req:%d, CQ->OUTER state:%d\n",
					req_stream_data->frame_seq_no, state_entry->estate);
				fixed_clklv = mtk_cam_handle_seamless_switch(req_stream_data);
				scen = req_stream_data->feature.scen;
				if (mtk_cam_scen_is_mstream_is_2_exp(scen)) {
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

	/* force clk level for DC mode */
	if (fixed_clklv)
		mtk_cam_dvfs_force_clk(ctx->cam, true);

	/* initial CQ done */
	if (raw_dev->sof_count == 0) {
		bool stream_on_camsv_last_exp = false;

		sensor_ctrl->initial_cq_done = 1;
		req_stream_data = mtk_cam_get_req_s_data(ctx, ctx->stream_id, 1);
		if (!req_stream_data) {
			dev_info(raw_dev->dev, "%s: req_stream_data of seq 1 not found\n",
				 __func__);
			return;
		}

		req = mtk_cam_s_data_get_req(req_stream_data);
		type = req_stream_data->feature.switch_feature_type;
		if (mtk_cam_hw_is_dc(ctx) && req && !req->ctx_link_update) {
			int ctx_exp_num =
				mtk_cam_scen_get_max_exp_num(req_stream_data->feature.scen);
			int req_exp_num =
				mtk_cam_scen_get_exp_num(req_stream_data->feature.scen);
			/* TBC: should ODT/M2M stagger run into here? */
			if (ctx_exp_num > req_exp_num && req_exp_num == 1) {
				dev_info(raw_dev->dev, "exp num mismatched: ctx exp (%d) request exp(%d)",
						ctx_exp_num, req_exp_num);
				stream_on_camsv_last_exp = true;
				switch (ctx_exp_num) {
				case 3:
					type = EXPOSURE_CHANGE_3_to_1;
					req_stream_data->feature.switch_feature_type =
						EXPOSURE_CHANGE_3_to_1;
					req_stream_data->feature.switch_done = 0;
					break;
				case 2:
					type = EXPOSURE_CHANGE_2_to_1;
					req_stream_data->feature.switch_feature_type =
						EXPOSURE_CHANGE_2_to_1;
					req_stream_data->feature.switch_done = 0;
					break;
				default:
					break;
				}
			}
		}

		if (req && req->ctx_link_update & (1 << ctx->stream_id)) {
			dev_info(raw_dev->dev, "%s: Skip frist CQ done's mtk_cam_stream_on\n",
				 __func__);
			return;
		}

		if (type == EXPOSURE_CHANGE_2_to_1 || type == EXPOSURE_CHANGE_3_to_1)
			mtk_camsys_exp_switch_cam_mux(raw_dev, ctx, req_stream_data);

		if (req_stream_data->state.estate >= E_STATE_SENSOR ||
			!ctx->sensor) {
			if (mtk_cam_scen_is_ext_isp(req_stream_data->feature.scen)) {
				spin_lock(&ctx->streaming_lock);
				if (ctx->streaming)
					stream_on(raw_dev, 1);
				spin_unlock(&ctx->streaming_lock);
			} else
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
	struct mtk_cam_scen *scen, *scen_prev;

	if (!mtk_cam_scen_is_m2m(&raw_dev->pipeline->scen_active))
		return;

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
				scen = req_stream_data->feature.scen;
				scen_prev = &req_stream_data->feature.prev_scen;
				if (mtk_cam_scen_is_mstream_m2m(scen) &&
				    (req_stream_data->feature.scen->scen.mstream.type !=
				     MTK_CAM_MSTREAM_1_EXPOSURE)) {
					if (s_data_idx == 0) {
						toggle_db(raw_dev);
						trigger_rawi(raw_dev, ctx,
						MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER);
					} else if (s_data_idx == 1) {
						toggle_db(raw_dev);
						trigger_rawi(raw_dev, ctx,
						MTKCAM_IPI_HW_PATH_OFFLINE);
					}
				} else {
					struct mtk_cam_apu_info *apu_info;

					apu_info = &req_stream_data->apu_info;
					if (mtk_cam_scen_get_exp_num(scen) !=
					    mtk_cam_scen_get_exp_num(scen_prev)) {
						dev_dbg(raw_dev->dev, "toggle_db, frame_seq_no %d",
							req_stream_data->frame_seq_no);
						toggle_db(raw_dev);
					}
					if ((mtk_cam_scen_is_stagger_m2m(scen) &&
					    scen->scen.normal.exp_num != 1) ||
						mtk_cam_scen_is_rgbw_enabled(scen)) {
						trigger_rawi(raw_dev, ctx,
							MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER);
					} else if (apu_info->is_update &&
						   apu_info->apu_path == APU_DC_RAW) {
						trigger_apu_start(raw_dev, ctx);
					} else if (apu_info->is_update &&
						   apu_info->apu_path == APU_FRAME_MODE) {
						trigger_vpui(raw_dev, ctx);
					} else {
						trigger_rawi(raw_dev, ctx,
							MTKCAM_IPI_HW_PATH_OFFLINE);
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
	struct mtk_cam_request *state_req_done;
	struct mtk_cam_request_stream_data *s_data;
	struct mtk_cam_request_stream_data *s_data_done = NULL;

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
			state_req_done = state_req;
			s_data_done = s_data;
			if (mtk_cam_ctx_has_raw(ctx) &&
			    mtk_cam_scen_is_subsample(s_data->feature.scen)) {
				state_transition(state_entry,
						 E_STATE_SUBSPL_INNER,
						 E_STATE_SUBSPL_DONE_NORMAL);

				dev_dbg(cam->dev, "[SWD-subspl] req:%d/state:0x%x/time:%lld\n",
					s_data->frame_seq_no, state_entry->estate,
					s_data->timestamp);
			} else if (mtk_cam_ctx_has_raw(ctx) &&
				   mtk_cam_scen_is_time_shared(s_data->feature.scen)) {
				state_transition(state_entry,
						 E_STATE_TS_INNER,
						 E_STATE_TS_DONE_NORMAL);
				dev_dbg(cam->dev, "[TS-SWD] ctx:%d req:%d/state:0x%x/time:%lld\n",
					ctx->stream_id, s_data->frame_seq_no,
					state_entry->estate, s_data->timestamp);
			} else if (mtk_cam_ctx_has_raw(ctx) &&
				   mtk_cam_scen_is_ext_isp(s_data->feature.scen)) {
				state_transition(state_entry,
						E_STATE_EXTISP_INNER,
						E_STATE_EXTISP_DONE_NORMAL);
				dev_dbg(cam->dev, "[SWD-extISP] ctx:%d req:%d/state:0x%x/time:%lld\n",
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

				/**
				 * TBC: Ryan
				 * The legacy mstream 1 exp dones't enter
				 * mtk_cam_raw_prepare_mstream_frame_done
				 * (conflict with the follwoing comment)
				 */
				/* mstream 2 and 1 exposure */
				if (mtk_cam_scen_is_mstream_2exp_types(s_data->feature.scen) ||
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

	/**
	 * update qos bw for mstream 2exp -> 1 exp
	 * because qos bandwidth update tries to get frame interval from
	 * sensor and there is a mutex lock in it, update it after spin
	 * unlock.
	 */
	if (s_data_done && mtk_cam_scen_is_mstream_types(s_data_done->feature.scen)) {
		struct mtk_cam_request_stream_data *req_s_data;

		req_s_data = mtk_cam_req_get_s_data(state_req_done, ctx->stream_id, 0);
		if (!mtk_cam_scen_is_mstream_2exp_types(req_s_data->feature.scen) &&
				req_s_data->feature.switch_feature_type ==
				(EXPOSURE_CHANGE_2_to_1 | MSTREAM_EXPOSURE_CHANGE)) {
			mtk_cam_qos_bw_calc(ctx, req_s_data->raw_dmas, true);
		}
	}

	return true;
}

void
mtk_camsys_raw_change_pipeline(struct mtk_cam_ctx *ctx,
			       struct mtk_camsys_sensor_ctrl *sensor_ctrl,
			       struct mtk_cam_request_stream_data *req_stream_data)
{
	struct mtk_cam_request *req = mtk_cam_s_data_get_req(req_stream_data);
	int frame_seq = req_stream_data->frame_seq_no;
	struct mtk_cam_working_buf_entry *buf_entry;
	struct mtk_camsv_working_buf_entry *sv_buf_entry;
	dma_addr_t base_addr;
	u64 ts_ns;
	struct mtk_raw_device *raw_dev;
	struct mtk_mraw_device *mraw_dev;
	int i;

	mutex_lock(&ctx->sensor_switch_op_lock);

	dev_info(ctx->cam->dev, "Exchange streams at seq(%d), update link ctx (0x%x)\n",
		 req_stream_data->frame_seq_no, req->ctx_link_update);

	mtk_cam_sensor_switch_stop_reinit_hw(ctx, req_stream_data, ctx->stream_id);

	spin_lock(&ctx->composed_buffer_list.lock);
	if (list_empty(&ctx->composed_buffer_list.list)) {
		req_stream_data->flags |= MTK_CAM_REQ_S_DATA_FLAG_SENSOR_SWITCH_BACKEND_DELAYED;
		dev_info(ctx->cam->dev,
			 "RAW SWITCH delay, no buffer update, cq_num:%d, frame_seq:%d\n",
			 ctx->composed_frame_seq_no, frame_seq);
		spin_unlock(&ctx->composed_buffer_list.lock);
		mutex_unlock(&ctx->sensor_switch_op_lock);
		return;
	}

	spin_lock(&ctx->first_cq_lock);
	ctx->is_first_cq_done = 0;
	ctx->cq_done_status = 0;
	spin_unlock(&ctx->first_cq_lock);

	if (ctx->sv_dev) {
		sv_buf_entry = list_first_entry(&ctx->sv_composed_buffer_list.list,
							struct mtk_camsv_working_buf_entry,
							list_entry);
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
		if (req->pipe_used &
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

	raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
	apply_cq(raw_dev, 1, base_addr,
		 buf_entry->cq_desc_size,
		 buf_entry->cq_desc_offset,
		 buf_entry->sub_cq_desc_size,
		 buf_entry->sub_cq_desc_offset);
	ts_ns = ktime_get_boottime_ns();

	if (mtk_cam_sv_apply_all_buffers(ctx) == 0)
		dev_info(raw_dev->dev, "sv apply all buffers failed");

	if (mtk_cam_mraw_apply_all_buffers(ctx) == 0)
		dev_info(raw_dev->dev, "mraw apply all buffers failed");

	mutex_unlock(&ctx->sensor_switch_op_lock);
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

	if (mtk_cam_scen_is_mstream_2exp_types(s_data_ctx->feature.scen)) {
		s_data_mstream = mtk_cam_req_get_s_data(req, ctx->stream_id, 1);

		unreliable |= (s_data->flags &
			(MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_DELAYED |
			MTK_CAM_REQ_S_DATA_FLAG_INCOMPLETE));

		if (s_data_mstream) {
			unreliable |= (s_data_mstream->flags &
				(MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_DELAYED |
				MTK_CAM_REQ_S_DATA_FLAG_INCOMPLETE));
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

	/* Update the timestamp for the buffer*/
	mtk_cam_s_data_update_timestamp(buf, s_data_ctx);

	/* clean the stream data for req reinit case */
	mtk_cam_s_data_reset_vbuf(s_data, MTK_RAW_META_OUT_1);

	/* Let user get the buffer */
	buf->final_state = unreliable ? VB2_BUF_STATE_ERROR : VB2_BUF_STATE_DONE;
	mtk_cam_mark_vbuf_done(req, buf);

	dev_dbg(ctx->cam->dev, "%s:%s: req(%d) done\n",
		 __func__, req->req.debug_str, s_data->frame_seq_no);
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
	mtk_cam_read_hdr_timestamp(ctx, req_stream_data);
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
	struct mtk_cam_request_stream_data *req_stream_data, *s_data_ctx;
	struct mtk_cam_request *req;
	struct mtk_cam_ctx *ctx;

	MTK_CAM_TRACE_BEGIN(BASIC, "frame_done");

	req_stream_data = mtk_cam_req_work_get_s_data(frame_done_work);
	ctx = mtk_cam_s_data_get_ctx(req_stream_data);
	req = mtk_cam_s_data_get_req(req_stream_data);
	s_data_ctx = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);

	if (mtk_cam_ctx_has_raw(ctx) &&
	    mtk_cam_scen_is_m2m(s_data_ctx->feature.scen))
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
	struct mtk_cam_scen scen;

	mtk_cam_scen_init(&scen);

	if (mtk_cam_ctx_has_raw(ctx) &&
	    mtk_cam_scen_is_sensor_stagger(&ctx->pipe->scen_active) &&
	    is_raw_subdev(pipe_id)) {
		/*check if switch request 's previous frame done may trigger tg db toggle */
		req = mtk_cam_get_req(ctx, frame_seq_no + 1);
		if (req) {
			req_stream_data = mtk_cam_req_get_s_data(req, pipe_id, 0);
			switch_type = req_stream_data->feature.switch_feature_type;
			mtk_cam_s_data_get_scen(&scen, req_stream_data);
			if (switch_type &&
				!mtk_cam_feature_change_is_mstream(switch_type)) {
				req_stream_data->feature.switch_prev_frame_done = 1;
				if (req_stream_data->feature.switch_prev_frame_done &&
					req_stream_data->feature.switch_curr_setting_done)
					mtk_cam_hdr_switch_toggle(ctx, &scen);
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

		/* cancel force clk level for DC mode */
		if (req_stream_data->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_MODE_UPDATE_T1)
			if (mtk_cam_hw_mode_is_dc(ctx->pipe->hw_mode))
				mtk_cam_dvfs_force_clk(ctx->cam, false);

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

	if (!(req_stream_data->flags & MTK_CAM_REQ_S_DATA_FLAG_META1_INDEPENDENT))
		mtk_cam_read_hdr_timestamp(ctx, req_stream_data);

	atomic_set(&req_stream_data->seninf_dump_state, MTK_CAM_REQ_DBGWORK_S_FINISHED);
	atomic_set(&req_stream_data->frame_done_work.is_queued, 1);
	frame_done_work = &req_stream_data->frame_done_work;
	queue_work(ctx->frame_done_wq, &frame_done_work->work);

	/**
	 * TBC: Yu-ming,
	 * Do we need to enter the mtk_camsys_ts_raw_try_set if the
	 * frame done is coming form mraw or sv but the scenrio is times share?
	 * (or the time share can't be coupled with mraw and camsv)
	 */
	if (mtk_cam_ctx_has_raw(ctx) &&
	    mtk_cam_scen_is_time_shared(&ctx->pipe->scen_active)) {
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

			if (ctx_2) {
				dev_dbg(raw_dev->dev,
					"%s: time sharing ctx-%d deq_no(%d)\n",
					__func__, ctx_2->stream_id,
					ctx_2->dequeued_frame_seq_no + 1);
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

	if (ctx->sensor ||
	    (!ctx->sensor && mtk_cam_ctx_has_raw(ctx) &&
	     mtk_cam_scen_is_pure_m2m(&ctx->pipe->scen_active))) {
		if (mtk_cam_ctx_has_raw(ctx) &&
		    mtk_cam_scen_is_subsample(&ctx->pipe->scen_active)) {
			s_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
			if (s_data->state.estate <= E_STATE_SUBSPL_SENSOR &&
				s_data->state.estate > E_STATE_SUBSPL_SCQ &&
				s_data->frame_seq_no > 1) {
				atomic_set(&sensor_ctrl->isp_request_seq_no,
					s_data->frame_seq_no);
				atomic_set(&sensor_ctrl->sensor_request_seq_no,
					s_data->frame_seq_no);
				media_request_object_complete(s_data->sensor_hdl_obj);
				dev_info(ctx->cam->dev,
					"[%s:subsample] frame_seq_no:%d, state:0x%x\n", __func__,
					s_data->frame_seq_no, s_data->state.estate);
			}
		}
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

			if (mtk_cam_scen_is_mstream_2exp_types(s_data->feature.scen)) {
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
	} else if (mtk_cam_ctx_has_raw(ctx) &&
		   mtk_cam_scen_is_ext_isp(&ctx->pipe->scen_active)) {
		mtk_cam_state_del_wo_sensor(ctx, req);
	}
}

static int mtk_camsys_camsv_state_handle(
		struct mtk_camsv_device *camsv_dev,
		struct mtk_camsys_sensor_ctrl *sensor_ctrl,
		struct mtk_camsys_ctrl_state **current_state,
		int frame_idx_inner)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_camsys_ctrl_state *state_temp;
	struct mtk_camsys_ctrl_state *state_outer = NULL, *state_inner = NULL;
	struct mtk_camsys_ctrl_state *state_rec[STATE_NUM_AT_SOF] = {};
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	int stateidx;
	int que_cnt = 0;
	u64 time_boot = ktime_get_boottime_ns();
	u64 time_mono = ktime_get_ns();

	/* list state-queue status */
	spin_lock(&sensor_ctrl->camsys_state_lock);
	list_for_each_entry(state_temp,
	&sensor_ctrl->camsys_state_list, state_element) {
		req = mtk_cam_ctrl_state_get_req(state_temp);
		req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		stateidx = atomic_read(&sensor_ctrl->sensor_request_seq_no) -
			   req_stream_data->frame_seq_no;

		if (stateidx < STATE_NUM_AT_SOF && stateidx > -1) {
			state_rec[stateidx] = state_temp;
			/* find outer state element */
			if (state_temp->estate == E_STATE_OUTER ||
				state_temp->estate == E_STATE_OUTER_HW_DELAY) {
				state_outer = state_temp;
				mtk_cam_set_timestamp(req_stream_data, time_boot, time_mono);
			}
			/* find inner state element */
			if (state_temp->estate == E_STATE_INNER ||
			    state_temp->estate == E_STATE_INNER_HW_DELAY) {
				state_inner = state_temp;
			}
			dev_dbg(camsv_dev->dev,
				"[SOF] STATE_CHECK [N-%d] Req:%d / State:%d\n",
				stateidx, req_stream_data->frame_seq_no,
				state_rec[stateidx]->estate);
		}
		/* counter for state queue */
		que_cnt++;
	}
	spin_unlock(&sensor_ctrl->camsys_state_lock);

	/* hw imcomplete case */
	if (state_inner) {
		req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_inner);
		if (frame_idx_inner > atomic_read(&sensor_ctrl->isp_request_seq_no) ||
			atomic_read(&req_stream_data->frame_done_work.is_queued) == 1) {
			dev_info_ratelimited(camsv_dev->dev, "[SOF] frame done work too late frames. req(%d)\n",
				req_stream_data->frame_seq_no);
		} else {
			if (state_outer) {
				state_transition(state_outer,
					E_STATE_INNER, E_STATE_INNER_HW_DELAY);
				state_transition(state_outer,
					E_STATE_OUTER, E_STATE_OUTER_HW_DELAY);
			}
			return STATE_RESULT_PASS_CQ_HW_DELAY;
		}
	}

	/* transit outer state to inner state */
	if (state_outer != NULL) {
		req = mtk_cam_ctrl_state_get_req(state_outer);
		req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		if (req_stream_data->frame_seq_no == frame_idx_inner) {
			if (frame_idx_inner >
				atomic_read(&sensor_ctrl->isp_request_seq_no)) {
				state_transition(state_outer,
					E_STATE_OUTER_HW_DELAY, E_STATE_INNER_HW_DELAY);
				state_transition(state_outer, E_STATE_OUTER, E_STATE_INNER);
				atomic_set(&sensor_ctrl->isp_request_seq_no, frame_idx_inner);
				dev_dbg(camsv_dev->dev, "[SOF-DBLOAD] req:%d, OUTER->INNER state:%d\n",
						req_stream_data->frame_seq_no, state_outer->estate);
			}
		}
	}

	/* trigger high resolution timer to try sensor setting */
	sensor_ctrl->sof_time = ktime_get_boottime_ns() / 1000000;
	mtk_cam_sof_timer_setup(ctx);

	if (que_cnt > 0 && state_rec[0]) {
		/* CQ triggering judgment*/
		if (state_rec[0]->estate == E_STATE_SENSOR) {
			*current_state = state_rec[0];
			return STATE_RESULT_TRIGGER_CQ;
		}
	}

	return STATE_RESULT_PASS_CQ_SW_DELAY;
}

static void mtk_camsys_camsv_check_pure_raw_done(struct mtk_cam_ctx *ctx,
	unsigned int dequeued_frame_seq_no)
{
#define CHECK_STATE_DEPTH 3
	struct mtk_cam_request *req, *req_prev;
	struct mtk_cam_request_stream_data *s_data_ctx;
	unsigned int seqList[CHECK_STATE_DEPTH];
	unsigned int i, cnt = 0;

	spin_lock(&ctx->cam->running_job_lock);
	list_for_each_entry_safe(req, req_prev, &ctx->cam->running_job_list, list) {
		if (!(req->pipe_used & ctx->streaming_pipe))
			continue;

		s_data_ctx = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		if (!s_data_ctx)
			continue;

		if (s_data_ctx->frame_seq_no < dequeued_frame_seq_no)
			seqList[cnt++] = s_data_ctx->frame_seq_no;
		else if (mtk_cam_sv_is_zero_fbc_cnt(ctx, SVTAG_2) &&
			(s_data_ctx->frame_seq_no == dequeued_frame_seq_no))
			seqList[cnt++] = s_data_ctx->frame_seq_no;

		if (cnt == CHECK_STATE_DEPTH)
			break;
	}
	spin_unlock(&ctx->cam->running_job_lock);

	for (i = 0; i < cnt; i++)
		mtk_camsv_pure_raw_scenario_handler(ctx, seqList[i], SVTAG_2);
}

static void mtk_camsys_mraw_check_frame_done(struct mtk_cam_ctx *ctx,
	unsigned int dequeued_frame_seq_no, struct mtk_mraw_device *mraw_dev)
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
			if (req_stream_data->frame_seq_no < dequeued_frame_seq_no)
				seqList[cnt++] = req_stream_data->frame_seq_no;
			else if (mtk_cam_mraw_is_zero_fbc_cnt(ctx, mraw_dev) &&
				(req_stream_data->frame_seq_no == dequeued_frame_seq_no))
				seqList[cnt++] = req_stream_data->frame_seq_no;

			if (cnt == CHECK_STATE_DEPTH)
				break;
		}
		spin_unlock_irqrestore(&sensor_ctrl->camsys_state_lock, flags);
		for (i = 0; i < cnt; i++)
			mtk_camsys_frame_done(ctx, seqList[i], mraw_dev->pipeline->id);
	}
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
	unsigned int cnt = 0, tag_idx = mtk_cam_get_sv_tag_index(ctx, pipe_id);
	int i;

	if (ctx->sensor) {
		spin_lock_irqsave(&sensor_ctrl->camsys_state_lock, flags);
		list_for_each_entry(state_temp, &sensor_ctrl->camsys_state_list,
						state_element) {
			req_stream_data = mtk_cam_ctrl_state_to_req_s_data(state_temp);
			if (req_stream_data->frame_seq_no < dequeued_frame_seq_no)
				seqList[cnt++] = req_stream_data->frame_seq_no;
			else if (mtk_cam_sv_is_zero_fbc_cnt(ctx, tag_idx) &&
				(req_stream_data->frame_seq_no == dequeued_frame_seq_no))
				seqList[cnt++] = req_stream_data->frame_seq_no;

			if (cnt == CHECK_STATE_DEPTH)
				break;
		}
		spin_unlock_irqrestore(&sensor_ctrl->camsys_state_lock, flags);
		for (i = 0; i < cnt; i++)
			mtk_camsys_frame_done(ctx, seqList[i], pipe_id);
	}
}

static void mtk_camsys_camsv_frame_start(struct mtk_camsv_device *camsv_dev,
	struct mtk_cam_ctx *ctx, unsigned int dequeued_frame_seq_no,
	unsigned int tag_idx)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_camsys_ctrl_state *current_state;
	enum MTK_CAMSYS_STATE_RESULT state_handle_ret;
	struct mtk_camsv_pipeline *sv_pipe = camsv_dev->tag_info[tag_idx].sv_pipe;

	/* touch watchdog */
	if (watchdog_scenario(ctx) && sv_pipe != NULL)
		mtk_ctx_watchdog_kick(ctx, sv_pipe->id);

	ctx->sv_dequeued_frame_seq_no[tag_idx] = dequeued_frame_seq_no;

	/* send V4L2_EVENT_FRAME_SYNC event */
	if (camsv_dev->tag_info[tag_idx].sv_pipe != NULL) {
		mtk_cam_sv_event_frame_sync(camsv_dev->tag_info[tag_idx].sv_pipe,
			dequeued_frame_seq_no);
	}

#if PURE_RAW_WITH_SV
	mtk_camsys_camsv_check_pure_raw_done(ctx, dequeued_frame_seq_no);
#endif

	/* check frame done */
	if (camsv_dev->tag_info[tag_idx].sv_pipe != NULL) {
		mtk_camsys_camsv_check_frame_done(ctx, dequeued_frame_seq_no,
			camsv_dev->tag_info[tag_idx].sv_pipe->id);
	}

	if (ctx->sensor &&
		(ctx->stream_id >= MTKCAM_SUBDEV_CAMSV_START &&
		ctx->stream_id < MTKCAM_SUBDEV_CAMSV_END)) {
		state_handle_ret = mtk_camsys_camsv_state_handle(camsv_dev, sensor_ctrl,
				&current_state, dequeued_frame_seq_no);
		if (state_handle_ret != STATE_RESULT_TRIGGER_CQ)
			return;
	}

	/* apply next buffer */
	if (ctx->stream_id >= MTKCAM_SUBDEV_CAMSV_START &&
		ctx->stream_id < MTKCAM_SUBDEV_CAMSV_END) {
		if (mtk_cam_sv_apply_all_buffers(ctx)) {
			/* transit state from sensor -> outer */
			if (ctx->sensor)
				state_transition(current_state, E_STATE_SENSOR, E_STATE_OUTER);
		} else {
			dev_dbg(camsv_dev->dev, "sv apply all buffers failed");
		}
	}
}

static void mtk_camsys_mraw_frame_start(struct mtk_mraw_device *mraw_dev,
	struct mtk_cam_ctx *ctx, struct mtk_camsys_irq_info *irq_info)
{
	int mraw_dev_index;
	unsigned int dequeued_frame_seq_no = irq_info->frame_idx_inner;

#ifdef CHECK_MRAW_NODEQ
	int write_cnt = irq_info->write_cnt;
	int fbc_cnt = irq_info->fbc_cnt;

	mraw_check_fbc_no_deque(ctx, mraw_dev, fbc_cnt, write_cnt, dequeued_frame_seq_no);
#endif

	/* touch watchdog */
	if (watchdog_scenario(ctx))
		mtk_ctx_watchdog_kick(ctx, mraw_dev->id + MTKCAM_SUBDEV_MRAW_START);

	/* inner register dequeue number */
	mraw_dev_index = mtk_cam_find_mraw_dev_index(ctx, mraw_dev->id);
	if (mraw_dev_index == -1) {
		dev_dbg(mraw_dev->dev, "cannot find mraw_dev_index(%d)", mraw_dev->id);
		return;
	}
	ctx->mraw_dequeued_frame_seq_no[mraw_dev_index] = dequeued_frame_seq_no;

	/* check frame done */
	mtk_camsys_mraw_check_frame_done(ctx, dequeued_frame_seq_no, mraw_dev);
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
	if (ctx->used_raw_num && ctx->pipe)
		all_subdevs |= (1 << ctx->pipe->id);
	if (ctx->sv_dev)
		all_subdevs |= (1 << (ctx->sv_dev->id + MTKCAM_SUBDEV_CAMSV_START));
	for (i = 0; i < ctx->used_mraw_num; i++)
		all_subdevs |= (1 << ctx->mraw_pipe[i]->id);
	if ((ctx->cq_done_status & all_subdevs) == all_subdevs) {
		ctx->is_first_cq_done = 1;
		ret = true;
	}
	spin_unlock(&ctx->first_cq_lock);
	dev_info(ctx->cam->dev, "[1st-CQD] all done:%d, pipe_id:%d (using raw/mraw/sv:%d/%d/%d) sv_tag_cnt:%d\n",
		ctx->is_first_cq_done, pipe_id, ctx->used_raw_num, ctx->used_mraw_num,
		(ctx->sv_dev) ? ctx->sv_dev->id + MTKCAM_SUBDEV_CAMSV_START : 0,
		(ctx->sv_dev) ? ctx->sv_dev->used_tag_cnt : 0);
EXIT:
	return ret;
}

static int mtk_camsys_event_handle_raw(struct mtk_cam_device *cam,
				       unsigned int engine_id,
				       struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_raw_device *raw_dev;
	struct mtk_cam_ctx *ctx;
	int tag_idx, tag_idx_w;
	bool is_rgbw;

	raw_dev = dev_get_drvdata(cam->raw.devs[engine_id]);
	if (mtk_cam_scen_is_time_shared(&raw_dev->pipeline->scen_active))
		ctx = &cam->ctxs[raw_dev->time_shared_busy_ctx_id];
	else
		ctx = mtk_cam_find_ctx(cam, &raw_dev->pipeline->subdev.entity);
	if (!ctx) {
		dev_dbg(raw_dev->dev, "cannot find ctx\n");
		return -EINVAL;
	}

	is_rgbw = mtk_cam_scen_is_rgbw_enabled(&raw_dev->pipeline->scen_active);

	if (mtk_cam_scen_is_ext_isp(&ctx->pipe->scen_active)) {
		dev_info(raw_dev->dev, "ts=%lu irq_type %d, req:%d/%d, cnt:%d/%d\n",
		irq_info->ts_ns / 1000,
		irq_info->irq_type,
		irq_info->frame_idx_inner,
		irq_info->frame_idx,
		raw_dev->tg_count, raw_dev->sof_count);
	}

	/* trace for FPS tool */
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_START)) {
		MTK_CAM_TRACE_ASYNC_BEGIN(FPS_TOOL, irq_info->frame_idx_inner);
	} else if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_DONE)) {
		MTK_CAM_TRACE_ASYNC_END(FPS_TOOL, irq_info->frame_idx_inner);
	}

	/* raw's CQ done */
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_SETTING_DONE)) {

		if (mtk_cam_scen_is_m2m(&raw_dev->pipeline->scen_active)) {
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
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_AFO_DONE)) {
		if (mtk_cam_ctx_has_raw(ctx) &&
		    mtk_cam_scen_is_m2m(&ctx->pipe->scen_active)) {
			mtk_cam_meta1_done(ctx, irq_info->frame_idx_inner,
					   ctx->stream_id);
		} else {
			mtk_cam_meta1_done(ctx, ctx->dequeued_frame_seq_no,
					   ctx->stream_id);
		}
	}

	/* raw's SW done */
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_DONE)) {

		if (mtk_cam_ctx_has_raw(ctx) &&
		    mtk_cam_scen_is_m2m(&ctx->pipe->scen_active)) {
			mtk_camsys_m2m_frame_done(ctx, irq_info->frame_idx_inner,
						  ctx->stream_id);
		} else if (mtk_cam_ctx_has_raw(ctx) &&
			   mtk_cam_scen_is_mstream_2exp_types(&ctx->pipe->scen_active)) {
			/**
			 * TBC:
			 * It reads feature_pending now,
			 * which may get the wrong mstream scen type when 2exp --> 1 exp
			 */
			mtk_camsys_frame_done(ctx, irq_info->frame_idx_inner,
					      ctx->stream_id);
		} else {
			mtk_camsys_frame_done(ctx, ctx->dequeued_frame_seq_no,
					      ctx->stream_id);
		}
	}

	/* raw's DCIF stagger main SOF(first exposure) */
	if ((irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_START_DCIF_MAIN)) &&
		raw_dev->stagger_en) {
		int frame_no_inner = irq_info->frame_idx_inner;

		irq_info->frame_idx_inner =
			mtk_cam_sv_frame_no_inner(ctx->sv_dev);

		mtk_camsys_raw_frame_start(raw_dev, ctx, irq_info);
		irq_info->frame_idx_inner = frame_no_inner;

		tag_idx = get_first_sv_tag_idx(ctx,
			mtk_cam_scen_get_stagger_exp_num(&raw_dev->pipeline->scen_active),
			false);
		tag_idx_w = get_first_sv_tag_idx(ctx,
			mtk_cam_scen_get_stagger_exp_num(&raw_dev->pipeline->scen_active),
			true);
		if (tag_idx != -1) {
			mtk_cam_sv_check_fbc_cnt(ctx->sv_dev, tag_idx);
		} else {
			dev_info(raw_dev->dev, "illegal first tag_idx: exp_num:%d\n",
				mtk_cam_scen_get_stagger_exp_num(&raw_dev->pipeline->scen_active));
		}
		if (is_rgbw) {
			if (tag_idx_w != -1) {
				mtk_cam_sv_check_fbc_cnt(ctx->sv_dev, tag_idx_w);
			} else {
				dev_info(raw_dev->dev, "illegal first tag_idx_w: exp_num:%d\n",
					mtk_cam_scen_get_stagger_exp_num(
					&raw_dev->pipeline->scen_active));
			}
		}
	}

	if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_START)) {
		/* raw's SOF */
		if (atomic_read(&raw_dev->vf_en) == 0) {
			dev_info(raw_dev->dev, "skip sof event when vf off\n");
			return 0;
		}

		if (mtk_cam_ctx_has_raw(ctx) &&
		    mtk_cam_scen_is_sensor_stagger(&ctx->pipe->scen_active))
			mtk_cam_hdr_last_frame_start(raw_dev, ctx, irq_info);

		else if (mtk_cam_ctx_has_raw(ctx) &&
			 mtk_cam_scen_is_ext_isp(&ctx->pipe->scen_active))
			mtk_camsys_extisp_raw_frame_start(raw_dev, ctx, irq_info);
		else
			mtk_camsys_raw_frame_start(raw_dev, ctx, irq_info);

		if (mtk_cam_hw_mode_is_dc(raw_dev->pipeline->hw_mode)) {
			if (mtk_cam_scen_is_sensor_stagger(&raw_dev->pipeline->scen_active)) {
				if (mtk_cam_scen_get_stagger_exp_num(
					&raw_dev->pipeline->scen_active) == 1) {
					tag_idx = get_first_sv_tag_idx(ctx,
						mtk_cam_scen_get_stagger_exp_num(
						&raw_dev->pipeline->scen_active), false);
					tag_idx_w = get_first_sv_tag_idx(ctx,
						mtk_cam_scen_get_stagger_exp_num(
						&raw_dev->pipeline->scen_active), true);
				} else {
					tag_idx = get_last_sv_tag_idx(ctx,
						mtk_cam_scen_get_stagger_exp_num(
						&raw_dev->pipeline->scen_active), false);
					tag_idx_w = get_last_sv_tag_idx(ctx,
						mtk_cam_scen_get_stagger_exp_num(
						&raw_dev->pipeline->scen_active), true);
				}

				if (tag_idx != -1) {
					mtk_cam_sv_check_fbc_cnt(ctx->sv_dev, tag_idx);
				} else {
					dev_info(raw_dev->dev, "illegal tag_idx: exp_num:%d\n",
						mtk_cam_scen_get_stagger_exp_num(
						&raw_dev->pipeline->scen_active));
				}
				if (is_rgbw) {
					if (tag_idx_w != -1) {
						mtk_cam_sv_check_fbc_cnt(ctx->sv_dev, tag_idx_w);
					} else {
						dev_info(raw_dev->dev, "illegal tag_idx_w: exp_num:%d\n",
							mtk_cam_scen_get_stagger_exp_num(
							&raw_dev->pipeline->scen_active));
					}
				}
			} else {
				tag_idx = get_first_sv_tag_idx(ctx,
					mtk_cam_scen_get_stagger_exp_num(
					&raw_dev->pipeline->scen_active), false);
				tag_idx_w = get_first_sv_tag_idx(ctx,
					mtk_cam_scen_get_stagger_exp_num(
					&raw_dev->pipeline->scen_active), true);
				if (tag_idx != -1) {
					mtk_cam_sv_check_fbc_cnt(ctx->sv_dev, tag_idx);
				} else {
					dev_info(raw_dev->dev, "illegal first tag_idx: exp_num:%d\n",
						mtk_cam_scen_get_stagger_exp_num(
						&raw_dev->pipeline->scen_active));
				}
				if (is_rgbw) {
					if (tag_idx_w != -1) {
						mtk_cam_sv_check_fbc_cnt(ctx->sv_dev, tag_idx_w);
					} else {
						dev_info(raw_dev->dev, "illegal first tag_idx_w: exp_num:%d\n",
							mtk_cam_scen_get_stagger_exp_num(
							&raw_dev->pipeline->scen_active));
					}
				}
			}
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
	unsigned int seq, i;

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
		mtk_camsys_mraw_frame_start(mraw_dev, ctx, irq_info);
	/* mraw's CQ done */
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_SETTING_DONE)) {
		if (mtk_camsys_is_all_cq_done(ctx, stream_id)) {
			/* stream on after all pipes' cq done */
			if (atomic_read(&mraw_dev->is_first_frame)) {
				atomic_set(&mraw_dev->is_first_frame, 0);
				if (mtk_cam_scen_is_ext_isp(&ctx->pipe->scen_active)) {
					if (ctx->sv_dev && ctx->sv_dev->used_tag_cnt)
						mtk_cam_sv_dev_stream_on(ctx, 1);
					for (i = 0; i < ctx->used_mraw_num; i++)
						mtk_cam_mraw_dev_stream_on(ctx,
							get_mraw_dev(ctx->cam,
								ctx->mraw_pipe[i]), 1);
				} else {
					raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
					mtk_camsys_raw_cq_done(raw_dev, ctx, irq_info->frame_idx);
				}
			} else {
				mtk_cam_mraw_vf_on(mraw_dev, 1);
				mtk_ctx_watchdog_start(ctx, 4, stream_id);
			}
		} else
			atomic_set(&mraw_dev->is_first_frame, 0);
	}

	return 0;
}

static int mtk_camsys_event_handle_camsv(struct mtk_cam_device *cam,
					 unsigned int engine_id,
					 struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_camsv_device *camsv_dev;
	struct mtk_raw_device *raw_dev;
	struct mtk_cam_ctx *ctx;
	unsigned int hw_scen;
	int group_idx, tag_idx, i;

	camsv_dev = dev_get_drvdata(cam->sv.devs[engine_id]);
	if (camsv_dev->ctx_stream_id >= MTKCAM_SUBDEV_CAMSV_END) {
		dev_info(camsv_dev->dev, "stream id out of range : %d",
				camsv_dev->ctx_stream_id);
		return -1;
	}
	ctx = &cam->ctxs[camsv_dev->ctx_stream_id];
	if (ctx->pipe && (mtk_cam_scen_is_ext_isp(&ctx->pipe->scen_active))) {
		dev_info(camsv_dev->dev, "ts=%lu irq_type %d, req:%d/%d, cnt:%d/%d, done_group:0x%x\n",
		irq_info->ts_ns / 1000,
		irq_info->irq_type,
		irq_info->frame_idx_inner,
		irq_info->frame_idx,
		camsv_dev->tg_cnt, camsv_dev->sof_count,
		irq_info->done_groups);
	}
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_DONE)) {
		for (group_idx = 0; group_idx < MAX_SV_HW_GROUPS; group_idx++) {
			if (!(irq_info->done_groups & (1 << group_idx)))
				continue;
			for (tag_idx = 0; tag_idx < MAX_SV_HW_TAGS; tag_idx++) {
				if (camsv_dev->active_group_info[group_idx] & (1 << tag_idx)) {
					hw_scen = camsv_dev->tag_info[tag_idx].hw_scen;
					if (hw_scen & MTK_CAMSV_SUPPORTED_SPECIAL_HW_SCENARIO)
						mtk_camsv_special_hw_scenario_handler(cam,
						camsv_dev, irq_info, hw_scen, tag_idx);
					else
						mtk_camsv_normal_scenario_handler(cam, camsv_dev,
							irq_info, tag_idx);
				}
#if PURE_RAW_WITH_SV_DONE_CHECK
				if (tag_idx == SVTAG_2)
					mtk_camsv_pure_raw_scenario_handler(ctx,
						ctx->sv_dequeued_frame_seq_no[tag_idx], tag_idx);
#endif
			}
		}
	}
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_START)) {
		mtk_cam_sv_print_fbc_status(camsv_dev);
		for (tag_idx = 0; tag_idx < MAX_SV_HW_TAGS; tag_idx++) {
			if (!(irq_info->sof_tags & (1 << tag_idx)))
				continue;
			hw_scen = camsv_dev->tag_info[tag_idx].hw_scen;
			if (hw_scen & MTK_CAMSV_SUPPORTED_SPECIAL_HW_SCENARIO)
				mtk_camsv_special_hw_scenario_handler(cam, camsv_dev, irq_info,
					hw_scen, tag_idx);
			else
				mtk_camsv_normal_scenario_handler(cam, camsv_dev, irq_info,
					tag_idx);
		}
		/* nothing to do for pure raw dump */
	}
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_SETTING_DONE)) {
		if (ctx->pipe && mtk_cam_scen_is_ext_isp(&ctx->pipe->scen_active) &&
			irq_info->frame_idx == 1) {
			struct mtk_cam_request_stream_data *s_data =
				mtk_cam_get_req_s_data(ctx, ctx->stream_id, 1);

			if (s_data)
				s_data->state.estate = E_STATE_EXTISP_SV_OUTER;
		}
		if (mtk_camsys_is_all_cq_done(ctx, camsv_dev->id +
			MTKCAM_SUBDEV_CAMSV_START) == false) {
			atomic_set(&camsv_dev->is_first_frame, 0);
			return 0;
		}
		/* stream on after all pipes' cq done */
		if (atomic_read(&camsv_dev->is_first_frame)) {
			atomic_set(&camsv_dev->is_first_frame, 0);
			if (ctx->used_raw_num) {
				if (!ctx->pipe) {
					dev_dbg(camsv_dev->dev, "no ctx->pipe\n");
					return -EINVAL;
				}
				if (mtk_cam_scen_is_ext_isp(&ctx->pipe->scen_active)) {
					if (ctx->sv_dev && ctx->sv_dev->used_tag_cnt)
						mtk_cam_sv_dev_stream_on(ctx, 1);
					for (i = 0; i < ctx->used_mraw_num; i++)
						mtk_cam_mraw_dev_stream_on(ctx,
							get_mraw_dev(ctx->cam,
								ctx->mraw_pipe[i]), 1);
				} else {
					raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);
					mtk_camsys_raw_cq_done(raw_dev, ctx,
						irq_info->frame_idx);
				}
			} else if (ctx->sv_dev && ctx->sv_dev->used_tag_cnt) {
				/* stream on */
				mtk_cam_sv_dev_stream_on(ctx, 1);
			}
		}
		/* nothing to do for pure raw dump */
	}

	return 0;
}

static bool mtk_cam_before_sensor_margin(struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &s_data->ctx->sensor_ctrl;
	int time_after_sof = ktime_get_boottime_ns() / 1000000 -
		s_data->ctx->sensor_ctrl.sof_time;
	int sensor_margin_ms = sensor_ctrl->timer_req_event +
		sensor_ctrl->timer_req_sensor;

	dev_dbg(sensor_ctrl->ctx->cam->dev, "[%s] %d + %d <= %d ? ret : %d",
		__func__, sensor_ctrl->timer_req_event, sensor_ctrl->timer_req_sensor,
		time_after_sof, time_after_sof <= sensor_margin_ms);

	return time_after_sof <= sensor_margin_ms;
}
void mtk_cam_try_set_sensor_at_enque(struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &s_data->ctx->sensor_ctrl;
	int time_after_sof = ktime_get_boottime_ns() / 1000000 -
							s_data->ctx->sensor_ctrl.sof_time;

	if (!s_data->sensor_hdl_obj)
		mtk_cam_submit_kwork_in_sensorctrl(
			sensor_ctrl->sensorsetting_wq,
			sensor_ctrl);
	else if (mtk_cam_before_sensor_margin(s_data))
		mtk_cam_submit_kwork_in_sensorctrl(
			sensor_ctrl->sensorsetting_wq,
			sensor_ctrl);
	else
		dev_info(s_data->ctx->cam->dev, "over-margin req:%d (+%dms)\n",
		s_data->frame_seq_no, time_after_sof);
}
int mtk_camsv_special_hw_scenario_handler(struct mtk_cam_device *cam,
	struct mtk_camsv_device *camsv_dev, struct mtk_camsys_irq_info *irq_info,
	unsigned int hw_scen, unsigned int tag_idx)
{
	struct mtk_raw_pipeline *pipeline;
	struct mtk_raw_device *raw_dev;
	struct mtk_cam_ctx *ctx;
	bool bDcif = false;

	if (camsv_dev->ctx_stream_id >= MTKCAM_SUBDEV_RAW_END) {
		dev_info(camsv_dev->dev, "stream id out of raw range : %d",
				camsv_dev->ctx_stream_id);
		return -1;
	}
	pipeline = &cam->raw.pipelines[camsv_dev->ctx_stream_id];
	raw_dev = get_master_raw_dev(cam, pipeline);
	bDcif = hw_scen & (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_DC_STAGGER));
	ctx = cam->ctxs + camsv_dev->ctx_stream_id;
	if (hw_scen & (1 << MTKCAM_SV_SPECIAL_SCENARIO_EXT_ISP)) {
		int seninf_padidx = camsv_dev->tag_info[tag_idx].seninf_padidx;

		/* raw/yuv pipeline frame start from camsv engine */
		if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_START)) {
			/* preisp frame start */
			if (mtk_cam_ctx_has_raw(ctx) &&
			    !mtk_cam_scen_is_ext_isp_yuv(&ctx->pipe->scen_active) &&
			    seninf_padidx == PAD_SRC_GENERAL0)
				mtk_cam_extisp_sv_frame_start(ctx, irq_info);
			/* yuv pipeline processed raw frame start */
			if (mtk_cam_ctx_has_raw(ctx) &&
			    mtk_cam_scen_is_ext_isp_yuv(&ctx->pipe->scen_active) &&
			    seninf_padidx == PAD_SRC_RAW_EXT0)
				mtk_camsys_extisp_yuv_frame_start(camsv_dev, ctx, irq_info);
		}
		/* yuv pipeline frame done from camsv engine */
		if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_DONE)) {
			if (mtk_cam_ctx_has_raw(ctx) &&
			    mtk_cam_scen_is_ext_isp_yuv(&ctx->pipe->scen_active) &&
			    seninf_padidx == PAD_SRC_RAW_EXT0)
				mtk_camsys_frame_done(ctx, ctx->dequeued_frame_seq_no,
					ctx->stream_id);
			/* TBR - moved meta deque earlier to meta done */
			if (mtk_cam_scen_is_ext_isp(&ctx->pipe->scen_active) &&
				seninf_padidx == PAD_SRC_GENERAL0) {
				struct mtk_cam_buffer *buf;
				struct mtk_cam_request *req;
				struct mtk_cam_request_stream_data *s_data;

				s_data = mtk_cam_get_req_s_data(ctx, ctx->stream_id,
					ctx->component_dequeued_frame_seq_no);
				if (!s_data) {
					dev_info(ctx->cam->dev, "ctx(%d): extisp:s_data(%d) is NULL\n",
					ctx->stream_id, ctx->component_dequeued_frame_seq_no);
					return 0;
				}
				req = mtk_cam_s_data_get_req(s_data);
				if (!req) {
					dev_info(ctx->cam->dev, "ctx(%d): extisp:req(%d) is NULL\n",
					ctx->stream_id, ctx->component_dequeued_frame_seq_no);
					return 0;
				}
				buf = mtk_cam_s_data_get_vbuf(s_data, MTK_RAW_META_SV_OUT_0);
				if (!buf) {
					dev_info(ctx->cam->dev,
						 "ctx(%d): can't get META_SV_OUT_0 buf from req(%d)\n",
						 ctx->stream_id, s_data->frame_seq_no);
					return 0;
				}
				mtk_cam_s_data_update_timestamp(buf, s_data);
				mtk_cam_s_data_reset_vbuf(s_data, MTK_RAW_META_SV_OUT_0);
				/* Let user get the buffer */
				buf->final_state = VB2_BUF_STATE_DONE;
				mtk_cam_mark_vbuf_done(req, buf);
				dev_info(ctx->cam->dev, "%s: ctx:%d seq:%d, custom 3a done ts:%lld\n",
					__func__, ctx->stream_id, s_data->frame_seq_no,
					s_data->preisp_meta_ts[0]);
			}
		}
		return 0;
	}
	/* first exposure sof */
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_START)) {
#if PURE_RAW_WITH_SV
		if (tag_idx == SVTAG_2)
			mtk_camsys_camsv_frame_start(camsv_dev, ctx,
				irq_info->frame_idx_inner, tag_idx);
#endif
		if (!bDcif && (camsv_dev->first_tag == (1 << tag_idx)))
			mtk_camsys_raw_frame_start(raw_dev, ctx, irq_info);
	}
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_DONE)) {
		if (camsv_dev->first_tag == (1 << tag_idx)) {
			ctx->component_dequeued_frame_seq_no =
				irq_info->frame_idx_inner;
		}
	}
	/* time sharing - camsv write dram mode */
	if (hw_scen & (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_OFFLINE))) {
		if (irq_info->irq_type & (1<<CAMSYS_IRQ_FRAME_DONE)) {
			mtk_camsys_ts_sv_done(ctx, irq_info->frame_idx_inner);
			mtk_camsys_ts_raw_try_set(raw_dev, ctx,
				ctx->dequeued_frame_seq_no + 1);
		}
		if (irq_info->irq_type & (1<<CAMSYS_IRQ_FRAME_START))
			mtk_camsys_ts_frame_start(ctx, irq_info->frame_idx_inner);
	}
	return 0;
}
int mtk_camsv_normal_scenario_handler(struct mtk_cam_device *cam,
	struct mtk_camsv_device *camsv_dev, struct mtk_camsys_irq_info *irq_info,
	unsigned int tag_idx)
{
	unsigned int pipe_id;
	unsigned int seq_no;
	struct mtk_camsv_tag_info *tag_info = &camsv_dev->tag_info[tag_idx];
	struct mtk_cam_ctx *ctx;

	ctx = cam->ctxs + camsv_dev->ctx_stream_id;
	if (!ctx) {
		dev_dbg(camsv_dev->dev, "cannot find ctx\n");
		return -EINVAL;
	}
	/* camsv's sw group done */
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_DONE)) {
		if (mtk_cam_is_display_ic(ctx) &&
			camsv_dev->first_tag != (1 << tag_idx))
			return 0;
		if (!tag_info->sv_pipe) {
			dev_dbg(camsv_dev->dev,
				"tag_idx:%d is not controlled by user", tag_idx);
			return -EINVAL;
		}
		pipe_id = tag_info->sv_pipe->id;
		seq_no = ctx->sv_dequeued_frame_seq_no[tag_idx];
		mtk_camsys_frame_done(ctx, seq_no, pipe_id);
	}
	/* camsv's tag sof */
	if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_START)) {
		if (mtk_cam_is_display_ic(ctx) &&
			camsv_dev->first_tag != (1 << tag_idx))
			return 0;
		mtk_camsys_camsv_frame_start(camsv_dev, ctx,
			irq_info->frame_idx_inner, tag_idx);
	}

	return 0;
}

#if PURE_RAW_WITH_SV_DONE_CHECK
static void mtk_camsv_pure_raw_done_work(struct work_struct *work)
{
	struct mtk_cam_req_work *frame_done_work = (struct mtk_cam_req_work *)work;
	struct mtk_cam_request_stream_data *s_data_raw;
	struct mtk_cam_request_stream_data *s_data_ctx;
	struct mtk_cam_request_stream_data *s_data_temp;
	struct mtk_cam_request_stream_data *deq_s_data[18];
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_request *req, *req_prev;
	struct mtk_cam_buffer *buf;
	unsigned int s_data_cnt, handled_cnt;

	/**
	 * 1. find s_data_raw.
	 * 2. find the imgo buf
	 * 3. mark done
	 */

	/* current s_data */
	s_data_raw = mtk_cam_req_work_get_s_data(frame_done_work);

	ctx = mtk_cam_s_data_get_ctx(s_data_raw);
	if (!ctx) {
		pr_info("%s: get ctx failed, seq(%d)",
			__func__, s_data_raw->frame_seq_no);
		return;
	}

	/* find all previous imgo and finish them */
	s_data_cnt = 0;
	mutex_lock(&ctx->cleanup_lock);
	spin_lock(&ctx->cam->running_job_lock);
	list_for_each_entry_safe(req, req_prev, &ctx->cam->running_job_list, list) {
		if (!(req->pipe_used & (1 << ctx->pipe->id)))
			continue;

		s_data_temp = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		if (!s_data_temp)
			continue;

		if (s_data_temp->frame_seq_no > s_data_raw->frame_seq_no)
			goto STOP_SCAN;

		deq_s_data[s_data_cnt++] = s_data_temp;
		if (s_data_cnt >= 18)
			goto STOP_SCAN;
	}
STOP_SCAN:
	spin_unlock(&ctx->cam->running_job_lock);

	for (handled_cnt = 0; handled_cnt < s_data_cnt; handled_cnt++) {
		s_data_temp = deq_s_data[handled_cnt];
		req = mtk_cam_get_req(ctx, s_data_temp->frame_seq_no);
		if (!req) {
			dev_info(ctx->cam->dev, "%s: get req failed, seq(%d)",
				__func__, s_data_temp->frame_seq_no);
			continue;
		}

		s_data_ctx = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		if (!s_data_ctx) {
			dev_info(ctx->cam->dev,
				"%s:%s:ctx-%d:s_data(%d) not found!\n",
				__func__, req->req.debug_str,
				ctx->stream_id, s_data_temp->frame_seq_no);
			continue;
		}

		/* mark imgo done and release buf */
		buf = mtk_cam_s_data_get_vbuf(s_data_temp, MTK_RAW_MAIN_STREAM_OUT);
		if (!buf) {
			dev_info(ctx->cam->dev,
				"%s:%s:ctx-%d: can't get MTK_RAW_MAIN_STREAM_OUT, seq(%d)\n",
				__func__, req->req.debug_str, ctx->stream_id,
				s_data_temp->frame_seq_no);
			continue;
		}

		/* clean the stream data for req reinit case */
		mtk_cam_s_data_reset_vbuf(s_data_temp, MTK_RAW_MAIN_STREAM_OUT);

		/* Update the timestamp for the buffer*/
		mtk_cam_s_data_update_timestamp(buf, s_data_ctx);

		/* TODO: mstream, unreliable */
		if (s_data_temp->frame_seq_no < s_data_raw->frame_seq_no)
			buf->final_state = VB2_BUF_STATE_ERROR;
		else
			buf->final_state = VB2_BUF_STATE_DONE;

		/* access request before done */
		dev_info(ctx->cam->dev,
			"%s:%s:ctx-%d: seq(%d) done\n",
			__func__, req->req.debug_str, ctx->stream_id,
			s_data_temp->frame_seq_no);

		/* Let user get the buffer */
		mtk_cam_mark_vbuf_done(req, buf);
	}
	mutex_unlock(&ctx->cleanup_lock);
}

int mtk_camsv_pure_raw_scenario_handler(struct mtk_cam_ctx *ctx,
	unsigned int frame_seq_no, int tag_idx)
{
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *s_data_raw;
	struct mtk_cam_req_work *frame_done_work;

	/**
	 * 1. find the master raw s_data (the request is valid until all
	 *    buffers being done, so the s_data_raw exist while camsv still
	 *    occupy imgo buffer)
	 * 2. queue the pure_raw_done_work (to track by cleanup flow)
	 */

	if (!ctx->sv_dev) {
		dev_info(ctx->cam->dev, "%s: sv_dev shall not be null", __func__);
		return -1;
	}

	req = mtk_cam_get_req(ctx, frame_seq_no);
	if (!req) {
		dev_dbg(ctx->cam->dev, "%s: get req failed, seq(%d)",
			__func__, frame_seq_no);
		return -1;
	}

	if (ctx->pipe)
		s_data_raw = mtk_cam_req_get_s_data(req, ctx->pipe->id, 0);
	else
		s_data_raw = NULL;
	if (!s_data_raw) {
		dev_info(ctx->cam->dev, "%s:%s: get s_data_raw failed, seq(%d)",
			__func__, req->req.debug_str, frame_seq_no);
		return -1;
	}

	if (!mtk_cam_s_data_is_pure_raw_with_sv(s_data_raw)) {
		dev_dbg(ctx->cam->dev, "%s: req w/o pure raw dump\n", __func__);
		return 0;
	}

	if (s_data_raw->pure_raw_sv_tag_idx != tag_idx) {
		dev_dbg(ctx->cam->dev, "%s: tag index mismatch(seq_no:%d/tag_idx:%d)",
			__func__, frame_seq_no, tag_idx);
		return 0;
	}

	if (frame_seq_no != s_data_raw->frame_seq_no)
		dev_info(ctx->cam->dev, "%s:%s: sequence mismatch(raw/sv:%d/%d)",
			__func__, req->req.debug_str, s_data_raw->frame_seq_no,
			frame_seq_no);

	if (atomic_read(&s_data_raw->pure_raw_done_work.is_queued)) {
		dev_dbg(ctx->cam->dev,
			"%s:%s: already queue pure raw done work, seq(%d)\n",
			__func__, req->req.debug_str, frame_seq_no);
		return -1;
	}

	dev_dbg(ctx->cam->dev, "%s: queue pure raw done work(seq_no:%d)\n",
		__func__, frame_seq_no);

	atomic_set(&s_data_raw->pure_raw_done_work.is_queued, 1);
	INIT_WORK(&s_data_raw->pure_raw_done_work.work, mtk_camsv_pure_raw_done_work);
	frame_done_work = &s_data_raw->pure_raw_done_work;
	queue_work(ctx->frame_done_wq, &frame_done_work->work);

	return 0;
}
#endif
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
	if (mtk_cam_ctx_has_raw(ctx) &&
	    mtk_cam_scen_is_subsample(req_stream_data->feature.scen))
		state_transition(&req_stream_data->state,
			E_STATE_READY, E_STATE_SUBSPL_READY);
	dev_info(ctx->cam->dev, "Directly setup sensor req:%d\n",
		req_stream_data->frame_seq_no);
}

void mtk_cam_mstream_mark_incomplete_frame(struct mtk_cam_ctx *ctx,
					struct mtk_cam_request_stream_data *incomplete_s_data)
{
	int i;
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *s_data_mstream;

	if (!mtk_cam_scen_is_mstream_2exp_types(incomplete_s_data->feature.scen))
		return;

	req = mtk_cam_s_data_get_req(incomplete_s_data);
	s_data_mstream = mtk_cam_req_get_s_data(req, ctx->stream_id, 1);

	s_data_mstream->flags |=
			MTK_CAM_REQ_S_DATA_FLAG_INCOMPLETE;

	/**
	 * mark all no_frame_done requests as unreliable
	 * (current frame_seq_no + (no_frame_done_cnt - 1))
	 */
	for (i = 1; i < incomplete_s_data->no_frame_done_cnt; i++) {
		struct mtk_cam_request_stream_data *req_stream_data;

		req_stream_data = mtk_cam_get_req_s_data(ctx, ctx->stream_id,
				incomplete_s_data->frame_seq_no + i);
		if (!req_stream_data) {
			pr_debug("%s null s_data:%d bypass\n", __func__,
				incomplete_s_data->frame_seq_no + i);
			continue;
		}
		req = mtk_cam_s_data_get_req(req_stream_data);

		if (mtk_cam_scen_is_mstream_2exp_types(req_stream_data->feature.scen)) {
			s_data_mstream = mtk_cam_req_get_s_data(req, ctx->stream_id, 1);
			s_data_mstream->flags |=
					MTK_CAM_REQ_S_DATA_FLAG_INCOMPLETE;
		} else {
			// seamless switch to 1-exp
			req_stream_data->flags |=
					MTK_CAM_REQ_S_DATA_FLAG_INCOMPLETE;
		}
	}
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
			timer_ms = (fps_ratio / (sub_sample + 1) == 2) ?
						SENSOR_SET_DEADLINE_MS_60FPS :
						SENSOR_SET_DEADLINE_MS;
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
			timer_ms = (fps_ratio / (sub_sample + 1) == 2) ?
						SENSOR_SET_RESERVED_MS_60FPS :
						SENSOR_SET_RESERVED_MS;
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
	int fps_factor, sub_ratio = 0;

	memset(&fi, 0, sizeof(fi));

	fi.pad = 0;
	v4l2_subdev_call(ctx->sensor, video, g_frame_interval, &fi);
	fps_factor = (fi.interval.numerator > 0) ?
			(fi.interval.denominator / fi.interval.numerator / 30) : 1;
	if (mtk_cam_ctx_has_raw(ctx) &&
		mtk_cam_scen_is_ext_isp(&ctx->pipe->scen_active)) {
		ctx->pipe->res_config.interval.denominator = fi.interval.denominator;
		ctx->pipe->res_config.interval.numerator = fi.interval.numerator;
		dev_info(ctx->cam->dev, "[%s:preisp] f interval:%d/%d\n",
			__func__, ctx->pipe->res_config.interval.denominator,
				ctx->pipe->res_config.interval.numerator);
	}
	sub_ratio = (ctx->used_raw_num) ?
		mtk_cam_raw_get_subsample_ratio(&ctx->pipe->res_config.scen) : 0;

	camsys_sensor_ctrl->ctx = ctx;
	atomic_set(&camsys_sensor_ctrl->reset_seq_no, 1);
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
	if (mtk_cam_ctx_has_raw(ctx) && mtk_cam_scen_is_sensor_stagger(&ctx->pipe->scen_active) &&
		fps_factor == 1 && sub_ratio == 0) {
		camsys_sensor_ctrl->timer_req_event =
			SENSOR_SET_STAGGER_DEADLINE_MS;
		camsys_sensor_ctrl->timer_req_sensor =
			SENSOR_SET_STAGGER_RESERVED_MS;
	}
	if (mtk_cam_ctx_has_raw(ctx) &&
		mtk_cam_scen_is_ext_isp(&ctx->pipe->scen_active) &&
		fps_factor == 1 && sub_ratio == 0) {
		camsys_sensor_ctrl->timer_req_event =
			SENSOR_SET_EXTISP_DEADLINE_MS;
		camsys_sensor_ctrl->timer_req_sensor =
			SENSOR_SET_EXTISP_RESERVED_MS;
	}
	INIT_LIST_HEAD(&camsys_sensor_ctrl->camsys_state_list);
	spin_lock_init(&camsys_sensor_ctrl->camsys_state_lock);
	spin_lock_init(&camsys_sensor_ctrl->drained_check_lock);
	if (ctx->sensor) {
		hrtimer_init(&camsys_sensor_ctrl->sensor_deadline_timer,
			     CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		camsys_sensor_ctrl->sensor_deadline_timer.function =
			sensor_deadline_timer_handler;
		camsys_sensor_ctrl->sensorsetting_wq = &ctx->sensor_worker;
	}
	kthread_init_work(&camsys_sensor_ctrl->work, mtk_cam_sensor_worker_in_sensorctrl);

	dev_info(ctx->cam->dev, "[%s] ctx:%d/raw_dev:0x%x fps_ratio/sub_ratio:%d/%d drained/sensor:%d/%d\n",
		__func__, ctx->stream_id, ctx->used_raw_dev, fps_factor, sub_ratio,
		camsys_sensor_ctrl->timer_req_event, camsys_sensor_ctrl->timer_req_sensor);

	return 0;
}

void mtk_camsys_ctrl_update(struct mtk_cam_ctx *ctx, int sensor_ctrl_factor)
{
	struct mtk_camsys_sensor_ctrl *camsys_sensor_ctrl = &ctx->sensor_ctrl;
	struct v4l2_subdev_frame_interval fi;
	int fps_factor = 1, sub_ratio = 0;

	memset(&fi, 0, sizeof(fi));

	if (ctx->used_raw_num) {
		fi.pad = 0;
		if (sensor_ctrl_factor > 0) {
			fps_factor = sensor_ctrl_factor;
		} else {
			/* TBC: v4l2_set_frame_interval_which(fi, V4L2_SUBDEV_FORMAT_ACTIVE); */
			v4l2_subdev_call(ctx->sensor, video, g_frame_interval, &fi);
			fps_factor = (fi.interval.numerator > 0) ?
					(fi.interval.denominator / fi.interval.numerator / 30) : 1;
		}
		sub_ratio =
			mtk_cam_raw_get_subsample_ratio(&ctx->pipe->res_config.scen);
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
	else
		mtk_cam_sv_event_eos(
			&ctx->cam->sv.pipelines[ctx->stream_id - MTKCAM_SUBDEV_CAMSV_START]);
	dev_info(ctx->cam->dev, "[%s] ctx:%d/raw_dev:0x%x\n",
		__func__, ctx->stream_id, ctx->used_raw_dev);
}

void mtk_cam_m2m_enter_cq_state(struct mtk_camsys_ctrl_state *ctrl_state)
{
	state_transition(ctrl_state, E_STATE_SENSOR, E_STATE_CQ);
}

void mtk_cam_extisp_sv_frame_start(struct mtk_cam_ctx *ctx,
				       struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_cam_request_stream_data *stream_data;
	struct mtk_camsys_ctrl_state *state_temp, *state_sensor = NULL, *state_out = NULL;
	struct mtk_cam_request *req;
	struct mtk_raw_device *raw_dev;
	unsigned long flags;
	int stateidx;
	unsigned int dequeued_frame_seq_no = irq_info->frame_idx_inner;
	u64 time_boot = irq_info->ts_ns;
	u64 time_mono = ktime_get_ns();

	raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);

	mtk_cam_event_frame_sync(ctx->pipe, dequeued_frame_seq_no);
	/* touch watchdog */
	mtk_ctx_watchdog_kick(ctx, raw_dev->id + MTKCAM_SUBDEV_RAW_START);
	/*update sof time for sensor*/
	sensor_ctrl->sof_time = irq_info->ts_ns / 1000000;
	/* Trigger high resolution timer to try sensor setting */
	if (ctx->sensor)
		mtk_cam_sof_timer_setup(ctx);
	else
		mtk_cam_state_add_wo_sensor(ctx);

	/* List state-queue status*/
	spin_lock_irqsave(&sensor_ctrl->camsys_state_lock, flags);
	list_for_each_entry(state_temp, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req = mtk_cam_ctrl_state_get_req(state_temp);
		stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		stateidx = atomic_read(&sensor_ctrl->sensor_request_seq_no) -
			   stream_data->frame_seq_no;
		/* Find inner element*/
		if (state_temp->estate >= E_STATE_EXTISP_SV_OUTER &&
			state_temp->estate < E_STATE_EXTISP_INNER &&
		    stream_data->frame_seq_no == dequeued_frame_seq_no) {
			state_out = state_temp;
			mtk_cam_set_timestamp(stream_data,
						      time_boot, time_mono);
			ctx->component_dequeued_frame_seq_no =
				dequeued_frame_seq_no;
			/* handle sv timestamp in stream_data */
			mtk_cam_extisp_handle_sv_tstamp(ctx, stream_data, irq_info);
		}
		/* Find to-be-set element*/
		if (state_temp->estate == E_STATE_EXTISP_SENSOR)
			state_sensor = state_temp;
		dev_dbg(ctx->cam->dev,
		"[%s] STATE_CHECK [N-%d] Req:%d / State:0x%x\n",
		__func__, stateidx, stream_data->frame_seq_no, state_temp->estate);
	}
	spin_unlock_irqrestore(&sensor_ctrl->camsys_state_lock, flags);

	if (state_sensor) {
		req = mtk_cam_ctrl_state_get_req(state_sensor);
		stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		/* apply sv buffer */
		if (mtk_cam_sv_apply_all_buffers(ctx) == 0)
			dev_info(cam->dev, "sv apply all buffers failed");
		/* apply mraw buffer */
		if (mtk_cam_mraw_apply_all_buffers(ctx) == 0)
			dev_info(cam->dev, "mraw apply all buffers failed");
		state_transition(state_sensor, E_STATE_EXTISP_SENSOR,
				 E_STATE_EXTISP_SV_OUTER);
		stream_data->state.sof_cnt_key = ctx->sv_dev->tg_cnt;
		dev_info(cam->dev, "[%s-ENQ-SV] ctx:%d/req:%d s:0x%x, cnt:%d/%d, key:%d\n",
		__func__, ctx->stream_id, stream_data->frame_seq_no, state_sensor->estate,
		ctx->sv_dev->tg_cnt, ctx->sv_dev->sof_count, stream_data->state.sof_cnt_key);
	}
	if (state_out) {
		req = mtk_cam_ctrl_state_get_req(state_out);
		stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		if (mtk_cam_ctx_has_raw(ctx) &&
		    mtk_cam_scen_is_ext_isp_yuv(stream_data->feature.scen) &&
			stream_data->frame_seq_no == 1) {
			/* for hw timing reason, camsv cannot support ext isp yuv */
		} else if (stream_data->frame_seq_no == 1) {
			struct mtk_raw_device *raw_dev =
						get_master_raw_dev(cam, ctx->pipe);
			struct mtk_cam_working_buf_entry *buf_entry;
			dma_addr_t base_addr;
			if (list_empty(&ctx->composed_buffer_list.list) ||
				state_out->estate >= E_STATE_EXTISP_CQ) {
				dev_info_ratelimited(raw_dev->dev,
					"no buffer update, state:0x%x\n",
					state_out->estate);
				return;
			}
			spin_lock(&ctx->composed_buffer_list.lock);
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
			state_transition(state_out, E_STATE_EXTISP_SV_OUTER,
				 E_STATE_EXTISP_CQ);
		}
		dev_info(cam->dev, "[%s] ctx:%d/req:%d state:0x%x\n", __func__,
			ctx->stream_id, stream_data->frame_seq_no, state_out->estate);
	}
}
int mtk_camsys_extisp_state_handle(struct mtk_raw_device *raw_dev,
				   struct mtk_camsys_sensor_ctrl *sensor_ctrl,
		struct mtk_camsys_ctrl_state **current_state,
		struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_cam_ctx *ctx = sensor_ctrl->ctx;
	struct mtk_camsys_ctrl_state *state_temp, *state_out = NULL, *state_sv = NULL;
	struct mtk_camsys_ctrl_state *state_in = NULL;
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *stream_data;
	int stateidx;
	unsigned long flags;
	int frame_inner_idx = irq_info->frame_idx_inner;
	int write_cnt;
	int write_cnt_offset;
	u64 time_boot = ktime_get_boottime_ns();
	u64 time_mono = ktime_get_ns();

	/* List state-queue status*/
	spin_lock_irqsave(&sensor_ctrl->camsys_state_lock, flags);
	list_for_each_entry(state_temp, &sensor_ctrl->camsys_state_list,
			    state_element) {
		req = mtk_cam_ctrl_state_get_req(state_temp);
		stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		stateidx = atomic_read(&sensor_ctrl->sensor_request_seq_no) -
				stream_data->frame_seq_no;
		if (stateidx < STATE_NUM_AT_SOF && stateidx > -1) {
			/* Find outer state element */
			if (state_temp->estate == E_STATE_EXTISP_INNER)
				state_in = state_temp;
			if (state_temp->estate == E_STATE_EXTISP_OUTER) {
				state_out = state_temp;
			}
			if (state_temp->estate == E_STATE_EXTISP_SV_OUTER &&
				stream_data->state.sof_cnt_key == raw_dev->tg_count)
				if (!state_sv)
					state_sv = state_temp;
			dev_dbg(ctx->cam->dev,
			"[%s] STATE_CHECK [N-%d] Req:%d / State:0x%x\n", __func__,
			stateidx, stream_data->frame_seq_no, state_temp->estate);
		}
	}
	spin_unlock_irqrestore(&sensor_ctrl->camsys_state_lock, flags);
	/* HW imcomplete case */
	if (state_in) {
		stream_data = mtk_cam_ctrl_state_to_req_s_data(state_in);
		write_cnt_offset = atomic_read(&sensor_ctrl->reset_seq_no) - 1;
		write_cnt = ((atomic_read(&sensor_ctrl->isp_request_seq_no)-
					  write_cnt_offset) / 256)
					* 256 + irq_info->write_cnt;
		if (frame_inner_idx > atomic_read(&sensor_ctrl->isp_request_seq_no) ||
			atomic_read(&stream_data->frame_done_work.is_queued) == 1) {
			dev_info_ratelimited(raw_dev->dev, "[SOF] frame done work too late frames. req(%d),ts(%lu)\n",
				stream_data->frame_seq_no, irq_info->ts_ns / 1000);
		} else if (write_cnt >= stream_data->frame_seq_no - write_cnt_offset) {
			dev_info_ratelimited(raw_dev->dev, "[SOF] frame done reading lost %d frames. req(%d),ts(%lu)\n",
				write_cnt - (stream_data->frame_seq_no - write_cnt_offset) + 1,
				stream_data->frame_seq_no, irq_info->ts_ns / 1000);
			mtk_cam_set_timestamp(stream_data,
							time_boot - 1000, time_mono - 1000);
			mtk_camsys_frame_done(ctx, write_cnt + write_cnt_offset,
							ctx->stream_id);
		} else if ((write_cnt >= stream_data->frame_seq_no - write_cnt_offset - 1)
			&& irq_info->fbc_cnt == 0) {
			dev_info_ratelimited(raw_dev->dev, "[SOF] frame done reading lost frames. req(%d),ts(%lu)\n",
				stream_data->frame_seq_no, irq_info->ts_ns / 1000);
			mtk_cam_set_timestamp(stream_data,
							time_boot - 1000, time_mono - 1000);
			mtk_camsys_frame_done(ctx, write_cnt + write_cnt_offset + 1,
							ctx->stream_id);
		}
	}
	/* Transit outer state to inner state */
	if (state_out) {
		req = mtk_cam_ctrl_state_get_req(state_out);
		stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		if (stream_data->frame_seq_no == frame_inner_idx) {
			if (frame_inner_idx >
				atomic_read(&sensor_ctrl->isp_request_seq_no)) {
				state_transition(state_out, E_STATE_EXTISP_OUTER,
						 E_STATE_EXTISP_INNER);
				atomic_set(&sensor_ctrl->isp_request_seq_no,
					frame_inner_idx);
				mtk_cam_extisp_handle_raw_tstamp(ctx, stream_data, irq_info);
				dev_info(ctx->cam->dev,
					"[%s] frame_seq_no:%d, OUTER->INNER state:0x%x\n",
					__func__, stream_data->frame_seq_no, state_out->estate);
			}
		}
	}
	/* CQ triggering judgment*/
	if (state_sv) {
		*current_state = state_sv;
		return STATE_RESULT_TRIGGER_CQ;
	}
	return STATE_RESULT_PASS_CQ_SW_DELAY;
}

void mtk_camsys_extisp_yuv_frame_start(struct mtk_camsv_device *camsv,
				       struct mtk_cam_ctx *ctx,
				       struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_camsys_ctrl_state *current_state;
	enum MTK_CAMSYS_STATE_RESULT state_handle_ret;
	struct mtk_raw_device *raw_dev = NULL;
	unsigned int dequeued_frame_seq_no = irq_info->frame_idx_inner;

	raw_dev = get_master_raw_dev(ctx->cam, ctx->pipe);

	if (ctx->ext_isp_meta_off && ctx->ext_isp_pureraw_off) {
		mtk_cam_event_frame_sync(ctx->pipe, dequeued_frame_seq_no);
		/* touch watchdog */
		if (watchdog_scenario(ctx))
			mtk_ctx_watchdog_kick(ctx, raw_dev->id + MTKCAM_SUBDEV_RAW_START);
		/* Trigger high resolution timer to try sensor setting */
		if (ctx->sensor)
			mtk_cam_sof_timer_setup(ctx);
		else
			mtk_cam_state_add_wo_sensor(ctx);
	}
	/* inner register dequeue number */
	if (!(mtk_cam_ctx_has_raw(ctx) && mtk_cam_scen_is_sensor_stagger(&ctx->pipe->scen_active)))
		ctx->dequeued_frame_seq_no = dequeued_frame_seq_no;
	/* Find request of this dequeued frame */
	req_stream_data = mtk_cam_get_req_s_data(ctx, ctx->stream_id, dequeued_frame_seq_no);
	/* If continuous 8 frame dequeue failed, we trigger the debug dump */
	// mtk_cam_debug_detect_dequeue_failed(req_stream_data, 8, irq_info);

	state_handle_ret = mtk_camsys_extisp_state_handle(raw_dev, sensor_ctrl,
					&current_state,
					irq_info);

	if (state_handle_ret != STATE_RESULT_TRIGGER_CQ) {
		dev_dbg(camsv->dev, "[YUVSOF] CQ drop s:%d deq:%d\n",
			state_handle_ret, dequeued_frame_seq_no);
		return;
	}

	/* Update CQ base address if needed */
	if (ctx->composed_frame_seq_no <= dequeued_frame_seq_no) {
		dev_info(camsv->dev,
			"YUVSOF[ctx:%d-#%d], CQ isn't updated [composed_frame_deq (%d) ]\n",
			ctx->stream_id, dequeued_frame_seq_no,
			ctx->composed_frame_seq_no);
		return;
	}
	/* apply next composed buffer */
	spin_lock(&ctx->composed_buffer_list.lock);
	if (list_empty(&ctx->composed_buffer_list.list)) {
		dev_info(camsv->dev,
			"YUV_SOF_INT_ST, no buffer update, cq_num:%d, frame_seq:%d\n",
			ctx->composed_frame_seq_no, dequeued_frame_seq_no);
		spin_unlock(&ctx->composed_buffer_list.lock);
	} else {
		spin_unlock(&ctx->composed_buffer_list.lock);
		/* for hw timing reason, camsv cannot support ext isp yuv */
		state_transition(current_state,
			E_STATE_EXTISP_SV_OUTER, E_STATE_EXTISP_OUTER);
		state_transition(current_state,
			E_STATE_EXTISP_SV_INNER, E_STATE_EXTISP_OUTER);
		if (req_stream_data)
			dev_info(camsv->dev,
			"YUVSOF[ctx:%d-#%d], CQ-%d is update, composed:%d, time:%lld, monotime:%lld\n",
			ctx->stream_id, dequeued_frame_seq_no, req_stream_data->frame_seq_no,
			ctx->composed_frame_seq_no, req_stream_data->timestamp,
			req_stream_data->timestamp_mono);
		else
			dev_info(camsv->dev,
			"YUVSOF[ctx:%d-#%d], CQ is update, composed:%d\n",
			ctx->stream_id, dequeued_frame_seq_no,
			ctx->composed_frame_seq_no);
	}
}


void mtk_camsys_extisp_raw_frame_start(struct mtk_raw_device *raw_dev,
				       struct mtk_cam_ctx *ctx,
				       struct mtk_camsys_irq_info *irq_info)
{
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_cam_working_buf_entry *buf_entry;
	struct mtk_camsys_ctrl_state *current_state;
	dma_addr_t base_addr;
	enum MTK_CAMSYS_STATE_RESULT state_handle_ret;
	int dequeued_frame_seq_no = irq_info->frame_idx_inner;

	/*touch watchdog*/
	mtk_ctx_watchdog_kick(ctx, raw_dev->id + MTKCAM_SUBDEV_RAW_START);
	/* inner register dequeue number */
	if (!(mtk_cam_ctx_has_raw(ctx) && mtk_cam_scen_is_sensor_stagger(&ctx->pipe->scen_active)))
		ctx->dequeued_frame_seq_no = dequeued_frame_seq_no;
	/* Find request of this dequeued frame */
	req_stream_data = mtk_cam_get_req_s_data(ctx, ctx->stream_id, dequeued_frame_seq_no);
	/* If continuous 8 frame dequeue failed, we trigger the debug dump */
	mtk_cam_debug_detect_dequeue_failed(req_stream_data, 30, irq_info, raw_dev);
	state_handle_ret = mtk_camsys_extisp_state_handle(raw_dev, sensor_ctrl,
					&current_state,
					irq_info);

	if (state_handle_ret != STATE_RESULT_TRIGGER_CQ) {
		dev_dbg(raw_dev->dev, "[SOF] CQ drop s:%d deq:%d\n",
			state_handle_ret, dequeued_frame_seq_no);
		return;
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
		buf_entry = list_first_entry(&ctx->composed_buffer_list.list,
					     struct mtk_cam_working_buf_entry,
					     list_entry);
		list_del(&buf_entry->list_entry);
		ctx->composed_buffer_list.cnt--;
		spin_unlock(&ctx->composed_buffer_list.lock);
		/* check streaming status - avoid racing between stream off */
		if (!ctx->streaming) {
			dev_info(ctx->cam->dev, "%s: stream off\n",
			__func__);
			return;
		}
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
		/* Transit state from svinner -> CQ */
		state_transition(current_state,
			E_STATE_EXTISP_SV_OUTER, E_STATE_EXTISP_CQ);

		/* req_stream_data of req_cq*/
		req_stream_data = mtk_cam_ctrl_state_to_req_s_data(current_state);
		dev_info(raw_dev->dev,
		"SOF[ctx:%d-#%d], CQ-%d is update, composed:%d, cq_addr:0x%x, cnt:%d/%d, key:%d\n",
		ctx->stream_id, dequeued_frame_seq_no, req_stream_data->frame_seq_no,
		ctx->composed_frame_seq_no, base_addr,
		raw_dev->tg_count, raw_dev->sof_count, req_stream_data->state.sof_cnt_key);

	}
}

int mtk_cam_extisp_prepare_meta(struct mtk_cam_ctx *ctx,
				int pad_src)
{
	struct v4l2_format *img_fmt;
	struct mtk_seninf_pad_data_info result;

	if (ctx->seninf) {
		mtk_cam_seninf_get_pad_data_info(ctx->seninf,
			pad_src, &result);
		dev_info(ctx->cam->dev, "[%s] hsize/vsize:%d/%d\n",
			__func__, result.exp_hsize, result.exp_vsize);
		if (pad_src == PAD_SRC_GENERAL0) {
			img_fmt = &ctx->pipe->vdev_nodes[
				MTK_RAW_META_SV_OUT_0 - MTK_RAW_SINK_NUM]
				.active_fmt;
			img_fmt->fmt.pix_mp.width = result.exp_hsize;
			img_fmt->fmt.pix_mp.height = result.exp_vsize;
			dev_info(ctx->cam->dev, "[%s] vdev_nodes:%d, w/h/size:%d/%d/%d\n",
				__func__, MTK_RAW_META_SV_OUT_0 - MTK_RAW_SINK_NUM,
				img_fmt->fmt.pix_mp.width,
				img_fmt->fmt.pix_mp.height,
				img_fmt->fmt.pix_mp.width * img_fmt->fmt.pix_mp.height);
		}
		if (pad_src == PAD_SRC_RAW0) {
			img_fmt = &ctx->pipe->vdev_nodes[
				MTK_RAW_MAIN_STREAM_SV_1_OUT - MTK_RAW_SINK_NUM]
				.active_fmt;
			if (img_fmt->fmt.pix_mp.width != result.exp_hsize ||
				img_fmt->fmt.pix_mp.height != result.exp_vsize) {
				result.exp_hsize = 0;
				result.exp_vsize = 0;
			}
			dev_info(ctx->cam->dev, "[%s] vdev_nodes:%d, w/h/size:%d/%d/%d\n",
				__func__, MTK_RAW_META_SV_OUT_0 - MTK_RAW_SINK_NUM,
				img_fmt->fmt.pix_mp.width,
				img_fmt->fmt.pix_mp.height,
				img_fmt->fmt.pix_mp.width * img_fmt->fmt.pix_mp.height);
		}
		return result.exp_hsize * result.exp_vsize;
	}
	return CAMSV_EXT_META_0_WIDTH * CAMSV_EXT_META_0_HEIGHT;
}


void mtk_cam_extisp_handle_sv_tstamp(struct mtk_cam_ctx *ctx,
			struct mtk_cam_request_stream_data *stream_data,
			struct mtk_camsys_irq_info *irq_info)
{
	int i;
	u32 seninf_padidx;

	for (i = SVTAG_IMG_START; i < SVTAG_IMG_END; i++) {
		if (ctx->sv_dev->enabled_tags & (1 << i)) {
			seninf_padidx = ctx->sv_dev->tag_info[i].seninf_padidx;
			if (seninf_padidx == PAD_SRC_RAW0)
				stream_data->preisp_img_ts[0] = ctx->sv_dev->sof_timestamp;
			else if (seninf_padidx == PAD_SRC_RAW1)
				stream_data->preisp_img_ts[1] = ctx->sv_dev->sof_timestamp;
			else if (seninf_padidx == PAD_SRC_GENERAL0)
				stream_data->preisp_meta_ts[0] = ctx->sv_dev->sof_timestamp;
		}
	}
}

void mtk_cam_extisp_handle_raw_tstamp(struct mtk_cam_ctx *ctx,
				struct mtk_cam_request_stream_data *stream_data,
				struct mtk_camsys_irq_info *irq_info)
{
	// proc raw timestamp
	stream_data->preisp_img_ts[1] = irq_info->ts_ns;
	/* s_data->ts assigned in sv frame start , hw incompl. case will miss it */
	if (stream_data->timestamp == 0 &&
		stream_data->preisp_meta_ts[0] > stream_data->timestamp) {
		dev_info(ctx->cam->dev, "[%s] fix ts:ctx:%d req:%d(ns) s_data:%lld < meta:%lld\n",
		__func__, ctx->stream_id, stream_data->frame_seq_no,
		stream_data->timestamp, stream_data->preisp_meta_ts[0]);
		stream_data->timestamp = stream_data->preisp_meta_ts[0];
	}
	dev_dbg(ctx->cam->dev, "[%s] req:%d(ns)0/1:%lld/%lld,0/1/2:%lld/%lld/%lld\n",
		__func__, ctx->stream_id,
		stream_data->frame_seq_no,
		stream_data->preisp_img_ts[0],
		stream_data->preisp_img_ts[1],
		stream_data->preisp_meta_ts[0],
		stream_data->preisp_meta_ts[1],
		stream_data->preisp_meta_ts[2]);
}
void mtk_cam_extisp_vf_reset(struct mtk_raw_pipeline *pipe)
{
	struct mtk_raw_device *raw_dev = NULL;
	struct mtk_camsv_device *camsv_dev = NULL;
	struct mtk_cam_ctx *ctx = NULL;
	struct mtk_cam_device *cam = NULL;
	int i;

	for (i = MTKCAM_SUBDEV_RAW_0; i < ARRAY_SIZE(pipe->raw->devs); i++)
		if (pipe->enabled_raw & (1 << i)) {
			raw_dev = dev_get_drvdata(pipe->raw->devs[i]);
			break;
		}
	if (raw_dev) {
		ctx = mtk_cam_find_ctx(raw_dev->cam, &pipe->subdev.entity);
		if (ctx) {
			cam = ctx->cam;
			mtk_cam_raw_vf_reset(ctx, raw_dev);
			raw_dev->sof_count = 0;
			raw_dev->tg_count = 0;
			camsv_dev = ctx->sv_dev;
			mtk_cam_sv_vf_reset(ctx, camsv_dev);
			camsv_dev->sof_count = 0;
			camsv_dev->tg_cnt = 0;
		}
	}
}

void mtk_cam_state_add_wo_sensor(struct mtk_cam_ctx *ctx)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_cam_request_stream_data *req_stream_data;
	int sensor_seq_no_next =
		atomic_read(&sensor_ctrl->sensor_request_seq_no) + 1;
	unsigned long flags;

	sensor_ctrl->sof_time = ktime_get_boottime_ns() / 1000000;
	sensor_ctrl->ctx = ctx;
	req_stream_data = mtk_cam_get_req_s_data(ctx, ctx->stream_id, sensor_seq_no_next);
	if (req_stream_data) {
		/* EnQ this request's state element to state_list (STATE:READY) */
		spin_lock_irqsave(&sensor_ctrl->camsys_state_lock, flags);
		list_add_tail(&req_stream_data->state.state_element,
			      &sensor_ctrl->camsys_state_list);
		atomic_set(&sensor_ctrl->sensor_request_seq_no,
			req_stream_data->frame_seq_no);
		spin_unlock_irqrestore(&sensor_ctrl->camsys_state_lock, flags);
		state_transition(&req_stream_data->state,
			E_STATE_EXTISP_READY, E_STATE_EXTISP_SENSOR);
		dev_dbg(ctx->cam->dev, "[%s] sensor set:%d state:0x%x\n",
				__func__, atomic_read(&sensor_ctrl->sensor_request_seq_no),
				req_stream_data->state.estate);
	}
}

void mtk_cam_state_del_wo_sensor(struct mtk_cam_ctx *ctx,
							struct mtk_cam_request *req)
{
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_camsys_ctrl_state *state_entry, *state_entry_prev;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_camsys_ctrl_state *req_state;
	struct mtk_cam_request *req_tmp;

	req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
	req_state = &req_stream_data->state;
	spin_lock(&sensor_ctrl->camsys_state_lock);
	list_for_each_entry_safe(state_entry, state_entry_prev,
			&sensor_ctrl->camsys_state_list,
			state_element) {
		if (state_entry == req_state) {
			list_del(&state_entry->state_element);
			req_tmp = mtk_cam_ctrl_state_get_req(state_entry);
			media_request_put(&req_tmp->req);
		}
	}
	spin_unlock(&sensor_ctrl->camsys_state_lock);
}

