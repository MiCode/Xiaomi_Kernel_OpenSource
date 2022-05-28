// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/rpmsg/mtk_ccd_rpmsg.h>
#include <linux/pm_runtime.h>

#include "mtk_cam-fmt_utils.h"
#include "mtk_cam.h"
#include "mtk_cam-ipi_7_1.h"
#include "mtk_cam-job.h"
#include "mtk_cam-ufbc-def.h"
#include "mtk_cam-plat.h"
#include "mtk_cam-debug.h"
#include "mtk_cam-timesync.h"


#include "frame_sync_camsys.h"

static int debug_job;
module_param(debug_job, int, 0644);

#define _dprintk(bit, fmt, arg...)				\
	do {							\
		if (debug_job & (1 << bit))			\
			pr_info("%s: " fmt, __func__, ##arg);	\
	} while (0)

#define buf_printk(fmt, arg...)		_dprintk(0, fmt, ##arg)

#define SENSOR_SET_MARGIN_MS  25
#define SENSOR_SET_MARGIN_MS_STAGGER  27

/* flags of mtk_cam_request */
#define MTK_CAM_REQ_FLAG_SENINF_CHANGED			BIT(0)
#define MTK_CAM_REQ_FLAG_SENINF_IMMEDIATE_UPDATE	BIT(1)
/* flags of mtk_cam_job */
#define MTK_CAM_REQ_S_DATA_FLAG_TG_FLASH		BIT(0)
#define MTK_CAM_REQ_S_DATA_FLAG_META1_INDEPENDENT	BIT(1)
#define MTK_CAM_REQ_S_DATA_FLAG_SINK_FMT_UPDATE		BIT(2)
/* Apply sensor mode and the timing is 1 vsync before */
#define MTK_CAM_REQ_S_DATA_FLAG_SENSOR_MODE_UPDATE_T1	BIT(3)
#define MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_EN		BIT(4)
#define MTK_CAM_REQ_S_DATA_FLAG_RAW_HDL_EN		BIT(5)
#define MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_COMPLETE	BIT(6)
#define MTK_CAM_REQ_S_DATA_FLAG_RAW_HDL_COMPLETE	BIT(7)
#define MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_DELAYED	BIT(8)
#define MTK_CAM_REQ_S_DATA_FLAG_INCOMPLETE BIT(9)

/*For state analysis and controlling for request*/
enum MTK_CAMSYS_STATE_IDX {
	E_STATE_READY = 0x0,
	E_STATE_SENINF,
	E_STATE_SENSOR,
	E_STATE_CQ,
	E_STATE_OUTER,
	E_STATE_CAMMUX_OUTER_CFG,
	E_STATE_CAMMUX_OUTER,
	E_STATE_INNER,
	E_STATE_DONE_NORMAL,
	E_STATE_CQ_SCQ_DELAY,
	E_STATE_CAMMUX_OUTER_CFG_DELAY,
	E_STATE_OUTER_HW_DELAY,
	E_STATE_INNER_HW_DELAY,
	E_STATE_DONE_MISMATCH,
	E_STATE_SUBSPL_READY = 0x10,
	E_STATE_SUBSPL_SCQ,
	E_STATE_SUBSPL_OUTER,
	E_STATE_SUBSPL_SENSOR,
	E_STATE_SUBSPL_INNER,
	E_STATE_SUBSPL_DONE_NORMAL,
	E_STATE_SUBSPL_SCQ_DELAY,
	E_STATE_TS_READY = 0x20,
	E_STATE_TS_SENSOR,
	E_STATE_TS_SV,
	E_STATE_TS_MEM,
	E_STATE_TS_CQ,
	E_STATE_TS_INNER,
	E_STATE_TS_DONE_NORMAL,
	E_STATE_EXTISP_READY = 0x30,
	E_STATE_EXTISP_SENSOR,
	E_STATE_EXTISP_SV_OUTER,
	E_STATE_EXTISP_SV_INNER,
	E_STATE_EXTISP_CQ,
	E_STATE_EXTISP_OUTER,
	E_STATE_EXTISP_INNER,
	E_STATE_EXTISP_DONE_NORMAL,
};
enum MTK_CAMSYS_JOB_TYPE {
	RAW_JOB_ON_THE_FLY = 0x0,
	RAW_JOB_DC,
	RAW_JOB_OFFLINE,
	RAW_JOB_MSTREAM,
	RAW_JOB_DC_MSTREAM,
	RAW_JOB_STAGGER,
	RAW_JOB_DC_STAGGER,
	RAW_JOB_OFFLINE_STAGGER,
	RAW_JOB_OTF_RGBW,
	RAW_JOB_DC_RGBW,
	RAW_JOB_OFFLINE_RGBW,
	RAW_JOB_HW_TIMESHARED,
	RAW_JOB_HW_SUBSAMPLE,
	RAW_JOB_HW_PREISP,
	RAW_JOB_ONLY_SV = 0x100,
	RAW_JOB_ONLY_MRAW = 0x200,
};
static bool
is_feature_mstream(int feature)
{
	int raw_feature;

	if (feature & MTK_CAM_FEATURE_OFFLINE_M2M_MASK)
		return false;

	raw_feature = feature & MTK_CAM_FEATURE_HDR_MASK;
	if (raw_feature == MSTREAM_NE_SE || raw_feature == MSTREAM_SE_NE)
		return true;

	return false;
}

static bool
is_feature_3_exposure(int feature)
{
	u32 raw_feature = 0;

	raw_feature = feature & 0x0000000F;
	if (raw_feature == STAGGER_3_EXPOSURE_LE_NE_SE ||
	    raw_feature == STAGGER_3_EXPOSURE_SE_NE_LE)
		return true;

	return false;
}
static bool
is_feature_2_exposure(int feature)
{
	u32 raw_feature = 0;

	raw_feature = feature & 0x0000000F;
	if (raw_feature == STAGGER_2_EXPOSURE_LE_SE ||
		raw_feature == STAGGER_2_EXPOSURE_SE_LE ||
		is_feature_mstream(feature))
		return true;

	return false;
}

static bool
is_feature_stagger(int feature)
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

static bool
is_job_stagger(struct mtk_cam_job *job)
{
	bool ret = false;

	if (job->job_type == RAW_JOB_STAGGER ||
		job->job_type == RAW_JOB_DC_STAGGER ||
		job->job_type == RAW_JOB_OFFLINE_STAGGER)
		ret = true;

	return ret;
}
static bool
is_job_dc(struct mtk_cam_job *job)
{
	bool ret = false;

	if (job->job_type == RAW_JOB_DC_MSTREAM ||
		job->job_type == RAW_JOB_DC ||
		job->job_type == RAW_JOB_DC_STAGGER ||
		job->job_type == RAW_JOB_DC_RGBW)
		ret = true;

	return ret;
}

static bool
is_job_mstream(struct mtk_cam_job *job)
{
	bool ret = false;

	if (job->job_type == RAW_JOB_MSTREAM ||
		job->job_type == RAW_JOB_DC_MSTREAM)
		ret = true;

	return ret;
}
static bool
is_job_mstream_change(int feature_change)
{
	if (feature_change & MSTREAM_EXPOSURE_CHANGE)
		return true;

	return false;
}

static bool
is_job_subsample(struct mtk_cam_job *job)
{
	bool ret = false;

	if (job->job_type == RAW_JOB_HW_SUBSAMPLE)
		ret = true;

	return ret;
}
static bool
is_job_preisp(struct mtk_cam_job *job)
{
	bool ret = false;

	if (job->job_type == RAW_JOB_HW_PREISP)
		ret = true;

	return ret;
}
static bool
is_job_offline_timeshared(struct mtk_cam_job *job)
{
	bool ret = false;

	if (job->job_type == RAW_JOB_OFFLINE)
		ret = true;

	return ret;
}

static int mtk_cam_job_fill_ipi_config(struct mtk_cam_job *job,
				       struct mtkcam_ipi_config_param *config);
static int mtk_cam_job_fill_ipi_frame(struct mtk_cam_job *job);

static int mtk_cam_get_job_type(struct mtk_cam_ctx *ctx,
				 struct mtk_cam_request *req)
{
	unsigned long feature = req->raw_data[ctx->raw_subdev_idx].ctrl.feature;
	int job_type;

	switch (feature & 0xFF) {
	case 0:
		job_type = RAW_JOB_ON_THE_FLY;
		break;
	case STAGGER_2_EXPOSURE_LE_SE:
	case STAGGER_2_EXPOSURE_SE_LE:
	case STAGGER_3_EXPOSURE_LE_NE_SE:
	case STAGGER_3_EXPOSURE_SE_NE_LE:
		job_type = RAW_JOB_STAGGER;
		break;
	case MSTREAM_NE_SE:
	case MSTREAM_SE_NE:
		job_type = RAW_JOB_MSTREAM;
		break;
	case HIGHFPS_2_SUBSAMPLE:
	case HIGHFPS_4_SUBSAMPLE:
	case HIGHFPS_8_SUBSAMPLE:
	case HIGHFPS_16_SUBSAMPLE:
	case HIGHFPS_32_SUBSAMPLE:
		job_type = RAW_JOB_HW_SUBSAMPLE;
		break;
	default:
		job_type = RAW_JOB_ON_THE_FLY;
		break;
	}
	return job_type;
}
static int
get_hdr_scen_id(struct mtk_cam_job *job)
{
	int hw_scen =
		(1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER);

	if (is_job_dc(job))
		hw_scen = (1 << MTKCAM_IPI_HW_PATH_OFFLINE_SRT_DCIF_STAGGER);
	/* TBC mstream / offline stagger*/

	pr_info("%s hw_scen:%d", __func__, hw_scen);
	return hw_scen;
}
static int
get_exp_order(struct mtk_cam_job *job, int sv_index)
{
	struct mtk_camsv_pipeline *sv_pipe =
		&job->src_ctx->cam->pipelines.camsv[sv_index];
	int feature = job->req->raw_data[job->src_ctx->raw_subdev_idx].ctrl.feature;
	int isDC = is_job_dc(job);
	int exp_no;
	int exp_order = -1;

	if (is_feature_stagger(feature) || isDC) {
		if (is_feature_2_exposure(feature))
			exp_no = 2;
		else if (is_feature_3_exposure(feature))
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
	}
	pr_info("[%s] exp_order:%d", __func__, exp_order);

	return exp_order;
}

static int mtk_cam_job_pack_init(struct mtk_cam_job *job,
				 struct mtk_cam_ctx *ctx,
				 struct mtk_cam_request *req)
{
	struct device *dev = ctx->cam->dev;
	int ret;

	job->job_type = mtk_cam_get_job_type(ctx, req);
	job->req = req;
	job->src_ctx = ctx;

	ret = mtk_cam_buffer_pool_fetch(&ctx->cq_pool, &job->cq);
	if (ret) {
		dev_info(dev, "ctx %d failed to fetch cq buffer\n",
			 ctx->stream_id);
		return ret;
	}

	ret = mtk_cam_buffer_pool_fetch(&ctx->ipi_pool, &job->ipi);
	if (ret) {
		dev_info(dev, "ctx %d failed to fetch ipi buffer\n",
			 ctx->stream_id);
		mtk_cam_buffer_pool_return(&job->cq);
		return ret;
	}

	return ret;
}
static int mtk_cam_dc_last_camsv(int raw)
{
	if (raw == 2)
		return 2;

	return 1;
}

static int select_camsv_engine(struct mtk_cam_device *cam,
		int hw_scen, int req_amount, int master, int is_trial)
{
	unsigned int i, j, k, group, exp_order;
	unsigned int idle_pipes = 0, match_cnt = 0, extisp_cnt = 0;
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
					idle_pipes |= (1 << cam->pipelines.camsv[j].id);
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
						idle_pipes |= (1 << cam->pipelines.camsv[j].id);
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
	} else {
		for (i = 0; i < cam->engines.num_camsv_devices; i++) {
			if ((cam->pipelines.camsv[i].is_occupied == 0) &&
				(cam->pipelines.camsv[i].hw_cap & hw_scen)) {
				match_cnt++;
				idle_pipes |= (1 << cam->pipelines.camsv[i].id);
			}
			if (match_cnt == req_amount)
				break;
		}
		if (match_cnt < req_amount) {
			idle_pipes = 0;
			match_cnt = 0;
			goto EXIT;
		}
	}

	if (is_trial == 0) {
		for (i = 0; i < cam->engines.num_camsv_devices; i++) {
			if (idle_pipes & (1 << cam->pipelines.camsv[i].id) &&
				hw_scen == (1 << MTKCAM_SV_SPECIAL_SCENARIO_EXT_ISP)) {
				cam->pipelines.camsv[i].is_occupied = 1;
				/* TODO: use other argument rather than req_amount */
				/*       to determine which ext isp case is        */
				if (req_amount == 2) {
					if (extisp_cnt == 0) {
						cam->pipelines.camsv[i].raw_vdevidx =
							MTK_RAW_MAIN_STREAM_SV_1_OUT;
						cam->pipelines.camsv[i].seninf_padidx =
							PAD_SRC_RAW0;
					} else if (extisp_cnt == 1) {
						cam->pipelines.camsv[i].raw_vdevidx =
							MTK_RAW_META_SV_OUT_0;
						cam->pipelines.camsv[i].seninf_padidx =
							PAD_SRC_GENERAL0;
					}
				} else if (req_amount == 3) {
					if (extisp_cnt == 0) {
						cam->pipelines.camsv[i].raw_vdevidx =
							MTK_RAW_MAIN_STREAM_SV_1_OUT;
						cam->pipelines.camsv[i].seninf_padidx =
							PAD_SRC_RAW0;
					} else if (extisp_cnt == 2) {
						cam->pipelines.camsv[i].raw_vdevidx =
							MTK_RAW_MAIN_STREAM_OUT;
						cam->pipelines.camsv[i].seninf_padidx =
							PAD_SRC_RAW_EXT0;
					} else if (extisp_cnt == 1) {
						cam->pipelines.camsv[i].raw_vdevidx =
							MTK_RAW_META_SV_OUT_0;
						cam->pipelines.camsv[i].seninf_padidx =
							PAD_SRC_GENERAL0;
					}
				}
				extisp_cnt++;
			}
		}
	}

EXIT:
	dev_info(cam->dev, "%s idle_pipes 0x%08x", __func__, idle_pipes);
	return idle_pipes;
}

static int mtk_cam_select_hw(struct mtk_cam_ctx *ctx, struct mtk_cam_job *job)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *raw = NULL;
	int available, raw_available;
	int selected;
	int i, j, sv_cnt = 0;
	int sv_available_subidx, selected_sv;
	int master, hw_scen, req_amount;
	int feature = job->req->raw_data[ctx->raw_subdev_idx].ctrl.feature;

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
	job->feature.raw_feature = feature;
	/* otf stagger case */
	if (job->job_type == RAW_JOB_STAGGER) {
		sv_available_subidx = 0;
		selected_sv = 0;
		dev_info(cam->dev, "[%s++] otf stagger case (feature:0x%x), select:0x%x\n",
			__func__, feature, selected);
		master = (1 << MTKCAM_SUBDEV_RAW_0);
		hw_scen = (1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER);
		req_amount = (is_feature_3_exposure(feature)) ? 2 : 1;
		sv_available_subidx = select_camsv_engine(cam, hw_scen, req_amount, master, 0);
		for (i = 0; i < cam->engines.num_camsv_devices; i++) {
			j = i + MTKCAM_SUBDEV_CAMSV_START;
			if (sv_available_subidx & BIT(j)) {
				USED_MASK_SET(&selected_sv, camsv, i);
				ctx->camsv_subdev_idx[sv_cnt++] = i;
			}
		}
		dev_info(cam->dev, "[%s--] otf stagger case, sv_subdevidx/select/select_all:0x%x/0x%x/0x%x\n",
			__func__, sv_available_subidx, selected_sv, selected);
		selected |= selected_sv;
	}
	return selected;
}

static int
get_raw_subdev_idx(int used_pipe)
{
	int used_raw = USED_MASK_GET_SUBMASK(&used_pipe, raw);
	int i;

	for (i = 0; used_raw; i++)
		if (SUBMASK_HAS(&used_raw, raw, i))
			return i;

	return -1;
}


static unsigned int
_get_master_raw_id(unsigned int num_raw, unsigned int enabled_raw)
{
	unsigned int i;

	for (i = 0; i < num_raw; i++) {
		if (enabled_raw & (1 << i))
			break;
	}

	if (i == num_raw)
		pr_info("no raw id found, enabled_raw 0x%x", enabled_raw);

	return i;
}

static void
_state_trans(struct mtk_cam_job *job,
		      enum MTK_CAMSYS_STATE_IDX from,
		      enum MTK_CAMSYS_STATE_IDX to)
{
	if (job->state == from)
		job->state = to;
}

static void
_set_timestamp(struct mtk_cam_job *job,
		      u64 time_boot,
		      u64 time_mono)
{
	job->timestamp = time_boot;
	job->timestamp_mono = time_mono;
}
#define FH_SEQ_BIT_MASK 0x00FFFFFF
#define FH_CTX_ID_SHIFT_BIT_NUM 24

unsigned int
decode_fh_reserved_data_to_ctx(u32 data_in)
{
	return (data_in & ~FH_SEQ_BIT_MASK) >> FH_CTX_ID_SHIFT_BIT_NUM;
}

unsigned int
encode_fh_reserved_data(u32 ctx_id_in, u32 seq_no_in)
{
	u32 ctx_id_data = ctx_id_in << FH_CTX_ID_SHIFT_BIT_NUM;
	u32 seq_no_data = seq_no_in & FH_SEQ_BIT_MASK;

	return ctx_id_data | seq_no_data;
}

unsigned int
decode_fh_reserved_data_to_seq(u32 ref_near_by, u32 data_in)
{
	u32 ctx_id_data = decode_fh_reserved_data_to_ctx(data_in);
	u32 seq_no_data = data_in & FH_SEQ_BIT_MASK;
	u32 seq_no_nearby = ref_near_by;
	u32 seq_no_candidate = seq_no_data + (seq_no_nearby & ~FH_SEQ_BIT_MASK);
	bool dbg = false;

	if (seq_no_nearby > 10) {
		if (seq_no_candidate > seq_no_nearby + 10)
			seq_no_candidate = seq_no_candidate - BIT(FH_CTX_ID_SHIFT_BIT_NUM);
		else if (seq_no_candidate < seq_no_nearby - 10)
			seq_no_candidate = seq_no_candidate + BIT(FH_CTX_ID_SHIFT_BIT_NUM);
	}
	if (dbg)
		pr_info("[%s]: %d/%d <= %d",
			__func__, ctx_id_data, seq_no_candidate, data_in);

	return seq_no_candidate;
}

static void
_complete_hdl(struct mtk_cam_job *job,
				 struct media_request_object *hdl_obj,
				 char *name)
{
	char *debug_str;
	u64 start, cost;

	debug_str = job->req->req.debug_str;

	start = ktime_get_boottime_ns();
	if (hdl_obj->ops)
		hdl_obj->ops->unbind(hdl_obj);	/* mutex used */
	else
		pr_info("%s:%s:seq(%d): cannot unbind %s hd\n",
			__func__, debug_str, job->frame_seq_no, name);

	cost = ktime_get_boottime_ns() - start;
	if (cost > 1000000)
		pr_info("%s:%s:seq(%d): complete hdl:%s, cost:%llu ns\n",
			__func__, debug_str, job->frame_seq_no, name, cost);
	else
		pr_debug("%s:%s:seq(%d): complete hdl:%s, cost:%llu ns\n",
			 __func__, debug_str, job->frame_seq_no, name, cost);

	media_request_object_complete(hdl_obj);
}

void _complete_sensor_hdl(struct mtk_cam_job *job)
{
	if (job->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_EN &&
	    !(job->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_COMPLETE) &&
	    job->sensor_hdl_obj) {
		_complete_hdl(job, job->sensor_hdl_obj, "sensor");
		job->flags |= MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_COMPLETE;
	}
}

static bool _frame_sync_start(struct mtk_cam_request *req)
{
#ifdef NOT_READY

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
#endif
	return 0;
}


static bool _frame_sync_end(struct mtk_cam_request *req)
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
			 "%s:%s:fs_sync_frame(0)\n",
			 __func__, req->req.debug_str);

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
#ifdef NOT_READY
static void mtk_cam_seamless_switch_work(struct work_struct *work)
{
	struct mtk_cam_req_work *req_work = (struct mtk_cam_req_work *)work;
	struct mtk_cam_request *req;
	struct mtk_cam_job *s_data;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	char *debug_str;
	struct v4l2_subdev_format fmt;
	unsigned int time_after_sof = 0;
	int ret = 0;

	s_data = mtk_cam_req_work_get_s_data(req_work);
	if (!s_data) {
		pr_info("%s mtk_cam_req_work(%p), job(%p), dropped\n",
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
			ktime_get_boottime_ns() / 1000000 - ctx->cam_ctrl.sof_time;
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

static void mtk_cam_handle_seamless_switch(struct mtk_cam_job *s_data)
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
	struct mtk_cam_job *job;
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
				job = mtk_cam_req_get_s_data(req, stream_id, 0);
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
#endif
static void _switch_check_bad_frame(
		struct mtk_cam_ctx *ctx, unsigned int frame_seq_no)

{
#ifdef NOT_READY
	struct mtk_cam_request *req, *req_bad, *req_cq;
	struct mtk_cam_job *s_data, *s_data_bad, *s_data_cq;
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
#endif
}

static void _exp_switch_sensor(struct mtk_cam_job *job)
{
	struct mtk_cam_request *req = job->req;
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	unsigned int time_after_sof = ktime_get_boottime_ns() / 1000000 -
					ctx->cam_ctrl.sof_time;

	dev_dbg(cam->dev, "%s:%s:ctx(%d):sensor try set start\n",
		__func__, req->req.debug_str, ctx->stream_id);

	if (_frame_sync_start(req))
		dev_dbg(cam->dev, "%s:%s:ctx(%d): sensor ctrl with frame sync - start\n",
			__func__, req->req.debug_str, ctx->stream_id);
	/* request setup*/
	if (job->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_EN) {
		v4l2_ctrl_request_setup(&req->req, job->src_ctx->sensor->ctrl_handler);
		dev_dbg(cam->dev, "[SOF+%dms] Sensor request:%d[ctx:%d] setup\n",
			time_after_sof, job->frame_seq_no,
			ctx->stream_id);
	}

	_state_trans(job, E_STATE_READY, E_STATE_SENSOR);
	if (_frame_sync_end(req))
		dev_dbg(cam->dev, "%s:ctx(%d): sensor ctrl with frame sync - stop\n",
			__func__, ctx->stream_id);

	dev_info(cam->dev, "%s:%s:ctx(%d)req(%d):sensor done at SOF+%dms\n",
		 __func__, req->req.debug_str, ctx->stream_id,
		 job->frame_seq_no, time_after_sof);

	_complete_sensor_hdl(job);
}
static int _exp_switch_cam_mux(struct mtk_cam_ctx *ctx,
		struct mtk_cam_job *job)
{
	struct mtk_cam_seninf_mux_param param;
	struct mtk_cam_seninf_mux_setting settings[3];
	int type = job->feature.switch_feature_type;
	int sv_main_id, sv_sub_id, sv_last_id;
	int sv_main_tg, sv_last_tg, raw_tg;
	int config_exposure_num = 3;
	int feature_active;
	int is_dc = 0; /* mtk_cam_hw_mode_is_dc(ctx->pipe->hw_mode); */

	/**
	 * To identify the "max" exposure_num, we use
	 * feature_active, not job->feature.raw_feature
	 * since the latter one stores the exposure_num information,
	 * not the max one.
	 */
	if (job->feature.switch_done == 1)
		return 0;
	feature_active = job->feature.feature_config;
	if (feature_active == STAGGER_2_EXPOSURE_LE_SE ||
	    feature_active == STAGGER_2_EXPOSURE_SE_LE)
		config_exposure_num = 2;

	if (type != EXPOSURE_CHANGE_NONE && config_exposure_num == 3) {
		sv_main_id = 0; /* get_main_sv_pipe_id(ctx->cam, ctx->pipe->enabled_raw); - TBC */
		sv_sub_id = 1; /* get_sub_sv_pipe_id(ctx->cam, ctx->pipe->enabled_raw); - TBC */
		switch (type) {
		case EXPOSURE_CHANGE_3_to_2:
		case EXPOSURE_CHANGE_1_to_2:
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			/* settings[0].camtg  = */
				/* ctx->cam->sv.pipelines[ */
					/* sv_main_id - MTKCAM_SUBDEV_CAMSV_START].cammux_id; */
			settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			/* settings[1].camtg  = PipeIDtoTGIDX(raw_dev->id); */
			settings[1].enable = 1;

			settings[2].seninf = ctx->seninf;
			settings[2].source = PAD_SRC_RAW2;
			/* settings[2].camtg  =*/
				/* ctx->cam->sv.pipelines[*/
					/* sv_sub_id - MTKCAM_SUBDEV_CAMSV_START].cammux_id; */
			settings[2].enable = 0;
			break;
		case EXPOSURE_CHANGE_3_to_1:
		case EXPOSURE_CHANGE_2_to_1:
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			/* settings[0].camtg  = PipeIDtoTGIDX(raw_dev->id); */
			settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			/* settings[1].camtg  =*/
				/* ctx->cam->sv.pipelines[*/
					/* sv_sub_id - MTKCAM_SUBDEV_CAMSV_START].cammux_id; */
			settings[1].enable = 0;

			settings[2].seninf = ctx->seninf;
			settings[2].source = PAD_SRC_RAW2;
			/* settings[2].camtg  = */
				/* ctx->cam->sv.pipelines[*/
					/* sv_main_id - MTKCAM_SUBDEV_CAMSV_START].cammux_id; */
			settings[2].enable = 0;
			break;
		case EXPOSURE_CHANGE_2_to_3:
		case EXPOSURE_CHANGE_1_to_3:
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			/* settings[0].camtg  = */
				/* ctx->cam->sv.pipelines[ */
					/* sv_main_id - MTKCAM_SUBDEV_CAMSV_START].cammux_id; */
			settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			/* settings[1].camtg  = */
				/* ctx->cam->sv.pipelines[ */
					/* sv_sub_id - MTKCAM_SUBDEV_CAMSV_START].cammux_id; */
			settings[1].enable = 1;

			settings[2].seninf = ctx->seninf;
			settings[2].source = PAD_SRC_RAW2;
			/* settings[2].camtg  = PipeIDtoTGIDX(raw_dev->id); */
			settings[2].enable = 1;
			break;
		default:
			break;
		}
		param.settings = &settings[0];
		param.num = 3;
		mtk_cam_seninf_streaming_mux_change(&param);
		job->feature.switch_done = 1;
		dev_info(ctx->cam->dev,
			"[%s] switch Req:%d, type:%d, cam_mux[0][1][2]:[%d/%d/%d][%d/%d/%d][%d/%d/%d]\n",
			__func__, job->frame_seq_no, type,
			settings[0].source, settings[0].camtg, settings[0].enable,
			settings[1].source, settings[1].camtg, settings[1].enable,
			settings[2].source, settings[2].camtg, settings[2].enable);
	} else if (type != EXPOSURE_CHANGE_NONE && config_exposure_num == 2) {
		sv_main_id = 0; /* get_main_sv_pipe_id(ctx->cam, ctx->pipe->enabled_raw); */
		sv_last_id = 1; /* get_last_sv_pipe_id(ctx->cam, ctx->pipe->enabled_raw); */
		raw_tg = 0; /* PipeIDtoTGIDX(raw_dev->id); */

		sv_main_tg = 0; /* ctx->cam->sv.pipelines[ */
			/* sv_main_id - MTKCAM_SUBDEV_CAMSV_START].cammux_id; */

		if (is_dc) {
			if (sv_last_id == -1) {
				dev_info(ctx->cam->dev, "dc mode without exp_order 2 sv");
				sv_last_tg = raw_tg;
			} else {
				sv_last_tg = 0; /* ctx->cam->sv.pipelines[ */
					/* sv_last_id - MTKCAM_SUBDEV_CAMSV_START].cammux_id; */
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
		job->feature.switch_done = 1;
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

static int _exp_sensor_switch(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int time_after_sof;
	int type = job->feature.switch_feature_type;

	if (type == EXPOSURE_CHANGE_1_to_2 || type == EXPOSURE_CHANGE_1_to_3) {
		_exp_switch_sensor(job);
		_exp_switch_cam_mux(ctx, job);
	}
	time_after_sof = ktime_get_boottime_ns() / 1000000 -
			   ctx->cam_ctrl.sof_time;
	dev_info(cam->dev,
		"[%s] [SOF+%dms]] ctx:%d, req:%d\n",
		__func__, time_after_sof, ctx->stream_id, job->frame_seq_no);

	return 0;
}
static bool
_is_sensor_switch(struct mtk_cam_job *job)
{
	if (job->feature.switch_feature_type)
		return true;

	if (job->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_MODE_UPDATE_T1) {
		_state_trans(job, E_STATE_READY, E_STATE_SENSOR);
		return true;
	}

	return false;
}
/* workqueue context */
static int
_meta1_done(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int pipe_id = get_raw_subdev_idx(job->req->used_pipe);

	if (pipe_id < 0)
		return 0;
	dev_dbg(cam->dev, "%s:%s:ctx(%d): seq_no:%d, state:0x%x\n",
			__func__, job->req->req.debug_str, job->src_ctx->stream_id,
			job->frame_seq_no, job->state);

	mtk_cam_req_buffer_done(job->req, pipe_id, MTKCAM_IPI_RAW_META_STATS_1,
			VB2_BUF_STATE_DONE, job->timestamp);

	return 0;
}

/* workqueue context */
static int
_frame_done(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int pipe_id = get_raw_subdev_idx(job->req->used_pipe);
	bool is_normal = job->state == E_STATE_DONE_NORMAL;
	u32 *fho_va;
	int subsample = 0;
	int i;

	if (pipe_id < 0)
		return 0;
	// using cpu's timestamp
	// (*job->timestamp_buf)[0] = job->timestamp_mono;
	// (*job->timestamp_buf)[1] = job->timestamp;

	fho_va = (u32 *)(job->cq.vaddr + job->cq.size - 64 * (subsample + 1));
	for (i = 0; i < (subsample + 1); i++) {
		/* timstamp_LSB + timestamp_MSB << 32 */
		(*job->timestamp_buf)[i*2] = mtk_cam_timesync_to_monotonic
		((u64) (*(fho_va + i*16)) + ((u64)(*(fho_va + i*16 + 1)) << 32))
		/1000;
		(*job->timestamp_buf)[i*2 + 1] = mtk_cam_timesync_to_boot
		((u64) (*(fho_va + i*16)) + ((u64)(*(fho_va + i*16 + 1)) << 32))
		/1000;
		dev_dbg(ctx->cam->dev,
			"timestamp TS:momo %ld us boot %ld us, LSB:%d MSB:%d\n",
			(*job->timestamp_buf)[i*2], (*job->timestamp_buf)[i*2 + 1],
			*(fho_va + i*16), *(fho_va + i*16 + 1));
	}
	if (is_normal)
		mtk_cam_req_buffer_done(job->req, pipe_id, -1,
			VB2_BUF_STATE_DONE, job->timestamp);
	else
		mtk_cam_req_buffer_done(job->req, pipe_id, -1,
			VB2_BUF_STATE_ERROR, job->timestamp);

	dev_info(cam->dev, "%s:%s:ctx(%d): seq_no:%d, state:0x%x, is_normal:%d, B/M ts:%lld/%lld\n",
		__func__, job->req->req.debug_str, job->src_ctx->stream_id,
		job->frame_seq_no, job->state, is_normal, job->timestamp, job->timestamp_mono);

	return 0;
}
static int
_stream_on_otf_stagger(struct mtk_cam_job *job, bool on)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int raw_id = _get_master_raw_id(cam->engines.num_raw_devices,
			job->used_engine);
	struct mtk_raw_device *raw_dev =
		dev_get_drvdata(cam->engines.raw_devs[raw_id]);
	int feature = job->feature.raw_feature;
	int hw_scen = BIT(MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER);

	if (is_feature_2_exposure(feature)) {
		mtk_cam_sv_dev_stream_on(
			ctx, job->src_ctx->camsv_subdev_idx[0], 1, hw_scen);
		stream_on(raw_dev, on);
		if (job->stream_on_seninf)
			ctx_stream_on_seninf_sensor_hdr(job->src_ctx, 1,
				PAD_SRC_RAW1, 1, raw_dev->id);
	} else if (is_feature_3_exposure(feature)) {
		mtk_cam_sv_dev_stream_on(
			ctx, job->src_ctx->camsv_subdev_idx[0], 1, hw_scen);
		mtk_cam_sv_dev_stream_on(
			ctx, job->src_ctx->camsv_subdev_idx[1], 1, hw_scen);
		stream_on(raw_dev, on);
		if (job->stream_on_seninf)
			ctx_stream_on_seninf_sensor_hdr(job->src_ctx, 1,
				PAD_SRC_RAW2, 1, raw_dev->id);
	} else {
		dev_dbg(cam->dev, "[%s] ctx:%d, job:%d, weird feature:%d\n",
			__func__, ctx->stream_id, job->frame_seq_no, feature);
	}

	return 0;
}


static int
_stream_on(struct mtk_cam_job *job, bool on)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int raw_id = _get_master_raw_id(cam->engines.num_raw_devices,
			job->used_engine);
	struct mtk_raw_device *raw_dev =
		dev_get_drvdata(cam->engines.raw_devs[raw_id]);

	stream_on(raw_dev, on);
	if (job->stream_on_seninf)
		ctx_stream_on_seninf_sensor(job->src_ctx, 1);

	return 0;
}

/* kthread context */
static int
_apply_sensor(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request *req = job->req;

	//MTK_CAM_TRACE_BEGIN(BASIC, "frame_sync_start");
	if (_frame_sync_start(req))
		dev_dbg(cam->dev, "%s:%s:ctx(%d): sensor ctrl with frame sync - start\n",
			__func__, req->req.debug_str, ctx->stream_id);
	//MTK_CAM_TRACE_END(BASIC); /* frame_sync_start */
	if (job->flags & MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_EN) {
		v4l2_ctrl_request_setup(&req->req,
					job->sensor->ctrl_handler);
		dev_dbg(cam->dev,
			"[%s] ctx:%d, job:%d\n",
			__func__, ctx->stream_id, job->frame_seq_no);
	}


	if (is_job_subsample(job))
		_state_trans(job, E_STATE_SUBSPL_OUTER, E_STATE_SUBSPL_SENSOR);
	else if (is_job_offline_timeshared(job))
		_state_trans(job, E_STATE_TS_READY, E_STATE_TS_SENSOR);
	else if (is_job_preisp(job))
		_state_trans(job, E_STATE_EXTISP_READY, E_STATE_EXTISP_SENSOR);
	else
		_state_trans(job, E_STATE_READY, E_STATE_SENSOR);


	//MTK_CAM_TRACE_BEGIN(BASIC, "frame_sync_end");
	if (_frame_sync_end(req))
		dev_dbg(cam->dev, "%s:ctx(%d): sensor ctrl with frame sync - stop\n",
				__func__, ctx->stream_id);
	//MTK_CAM_TRACE_END(BASIC); /* frame_sync_end */

	/* TBC */
	/* mtk_cam_tg_flash_req_setup(ctx, s_data); */
	_complete_sensor_hdl(job);
	dev_dbg(cam->dev, "%s:%s:ctx(%d)req(%d):sensor done\n",
		__func__, req->req.debug_str, ctx->stream_id, job->frame_seq_no);
	return 0;
}

static int ipi_config(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_config_param *config = &event.config_data;
	struct mtkcam_ipi_config_param *src_config = &job->ipi_config;

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_CONFIG;
	session->session_id = ctx->stream_id;
	memcpy(config, src_config, sizeof(*src_config));

	rpmsg_send(ctx->rpmsg_dev->rpdev.ept, &event, sizeof(event));

	dev_info(job->src_ctx->cam->dev, "%s: rpmsg_send id: %d\n",
		 __func__, event.cmd_id);
	return 0;
}

static int _compose(struct mtk_cam_job *job)
{
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_frame_info *frame_info = &event.frame_data;
	struct mtk_cam_pool_buffer *ipi = &job->ipi;
	int ret;

	if (job->do_ipi_config) {
		ret = ipi_config(job);
		if (ret)
			return ret;
	}

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_FRAME;
	session->session_id = job->src_ctx->stream_id;
	session->frame_no =
		encode_fh_reserved_data(job->src_ctx->stream_id, job->frame_seq_no);
	frame_info->cur_msgbuf_offset = ipi->size * ipi->priv.index;
	frame_info->cur_msgbuf_size = ipi->size;

	if (WARN_ON(!job->src_ctx->rpmsg_dev))
		return -1;

	//MTK_CAM_TRACE_BEGIN(BASIC, "ipi_cmd_frame:%d",
	//req_stream_data->frame_seq_no);

	rpmsg_send(job->src_ctx->rpmsg_dev->rpdev.ept, &event, sizeof(event));

	//MTK_CAM_TRACE_END(BASIC);

	dev_info(job->src_ctx->cam->dev,
		 "%s: req:%s: rpmsg_send id: %d, ctx:%d, seq:%d\n",
		 __func__, job->req->req.debug_str,
		 event.cmd_id, session->session_id,
		 job->frame_seq_no);

	return 0;
}

static int _apply_cq(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int raw_id = _get_master_raw_id(cam->engines.num_raw_devices,
			job->used_engine);
	struct mtk_raw_device *raw_dev =
		dev_get_drvdata(cam->engines.raw_devs[raw_id]);
	dma_addr_t base_addr = job->cq.daddr;
	int ret = 0;

	if (!job->composed) {
		dev_info_ratelimited(raw_dev->dev,
			"SOF_INT_ST, no buffer update, frame_seq:%d\n",
			job->frame_seq_no);
		ret = -1;
	} else {
		apply_cq(raw_dev, 0, base_addr,
			job->cq_rst.cq_desc_size,
			job->cq_rst.cq_desc_offset,
			job->cq_rst.sub_cq_desc_size,
			job->cq_rst.sub_cq_desc_offset);
		/* Transit state from Sensor -> CQ */
		if (job->sensor) {
			/* update qos bw */
			//mtk_cam_qos_bw_calc(ctx, job->raw_dmas, false);
		}
		_state_trans(job, E_STATE_SENSOR, E_STATE_CQ);
		dev_info(raw_dev->dev,
			"SOF[ctx:%d], CQ-%d triggered, cq_addr:0x%x\n",
			ctx->stream_id, job->frame_seq_no, base_addr);
	}
#ifdef NOT_READY
	/* update sv/mraw's ts */
	if (mtk_cam_sv_update_all_buffer_ts(ctx, event_info->ts_ns) == 0)
		dev_dbg(raw_dev->dev, "sv update all buffer ts failed");
	if (mtk_cam_mraw_update_all_buffer_ts(ctx, event_info->ts_ns) == 0)
		dev_dbg(raw_dev->dev, "mraw update all buffer ts failed");

	if (mtk_cam_is_with_w_channel(ctx) && is_apply) {
		if (mtk_cam_sv_rgbw_apply_next_buffer(buf_entry->job) == 0)
			dev_info(raw_dev->dev, "rgbw: sv apply next buffer failed");
	}
	if (ctx->used_sv_num && is_apply) {
		if (mtk_cam_sv_apply_all_buffers(ctx, true) == 0)
			dev_info(raw_dev->dev, "sv apply all buffers failed");
	}
	if (ctx->used_mraw_num && is_apply) {
		if (mtk_cam_mraw_apply_all_buffers(ctx, true) == 0)
			dev_info(raw_dev->dev, "mraw apply all buffers failed");
	}
#endif
	return ret;
}

static int _compose_done(struct mtk_cam_job *job,
	struct mtkcam_ipi_frame_ack_result *cq_ret)
{
	job->composed = true;
	job->cq_rst.cq_desc_offset = cq_ret->cq_desc_offset;
	job->cq_rst.cq_desc_size = cq_ret->cq_desc_size;
	job->cq_rst.sub_cq_desc_size = cq_ret->sub_cq_desc_size;
	job->cq_rst.sub_cq_desc_offset = cq_ret->sub_cq_desc_offset;
	if (job->frame_seq_no == 1)
		_apply_cq(job);
	return 0;
}

static void
_update_event_sensor_try_set(struct mtk_cam_job *job,
		struct mtk_cam_job_event_info *event_info, int *action)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
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
		if (job->state == E_STATE_CQ &&
		    job->frame_seq_no > 1 && !is_job_stagger(job)) {
			/**
			 * FIXME: sw scq delay judgement, may need hw signal to confirm.
			 * because CQ_MAIN_TRIG_DLY_ST is coming
			 * in the next sof, a bit too late, can't depend on it.
			 */
			job->state = E_STATE_CQ_SCQ_DELAY;
			dev_info(ctx->cam->dev,
				 "[%s] SCQ DELAY STATE\n", __func__);
			*action = BIT(CAM_JOB_CQ_DELAY);
			return;
		} else if (job->state == E_STATE_CAMMUX_OUTER_CFG) {
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
		if (_is_sensor_switch(job) && job->frame_seq_no > 1) {
			dev_info(ctx->cam->dev,
				 "[%s] switch type:%d request:%d - pass sensor\n",
				 __func__, job->feature.switch_feature_type,
				 job->frame_seq_no);
			*action = 0;
			return;
		}
		if (*action & BIT(CAM_JOB_HW_DELAY) ||
			*action & BIT(CAM_JOB_CQ_DELAY) ||
			*action & BIT(CAM_JOB_SENSOR_DELAY))
			return;

		*action |= BIT(CAM_JOB_APPLY_SENSOR);
	}
	if (job->frame_seq_no > cur_sen_seq_no + 1)
		*action = 0;
}

static void
_update_event_meta1_done(struct mtk_cam_job *job,
		struct mtk_cam_job_event_info *event_info, int *action)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	unsigned int frame_seq_no_inner = event_info->frame_idx_inner;

	if (job->frame_seq_no <= frame_seq_no_inner) {
		if (!(job->flags & MTK_CAM_REQ_S_DATA_FLAG_META1_INDEPENDENT))
			return;
		if (atomic_read(&job->meta1_done_work.is_queued)) {
			pr_info("already queue done work req:%d ctx_id:%d\n",
				job->frame_seq_no, job->ctx_id);
			return;
		}
		atomic_set(&job->seninf_dump_state, MTK_CAM_REQ_DBGWORK_S_FINISHED);
		atomic_set(&job->meta1_done_work.is_queued, 1);
		*action |= BIT(CAM_JOB_DEQUE_META1);
		if (job->frame_seq_no == frame_seq_no_inner) {
			// mark buf normal
			dev_dbg(cam->dev, "[%s] ctx_id:%d, mark job:%d NORMAL\n",
				__func__, ctx->stream_id, job->frame_seq_no);
		} else {
			// mark buf error
			dev_dbg(cam->dev, "[%s] ctx_id:%d, mark job:%d ERROR\n",
				__func__, ctx->stream_id, job->frame_seq_no);
		}
	} else {
		*action = 0;
	}
}

static void
_update_event_frame_done(struct mtk_cam_job *job,
		struct mtk_cam_job_event_info *event_info, int *action)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	unsigned int frame_seq_no_inner = event_info->frame_idx_inner;

	if (is_job_stagger(job)) {
		/*check if switch request 's previous frame done may trigger tg db toggle */
		_switch_check_bad_frame(job->src_ctx, job->frame_seq_no);
	}

	if (job->frame_seq_no <= frame_seq_no_inner) {
		if (atomic_read(&job->frame_done_work.is_queued)) {
			pr_info("already queue done work req:%d ctx_id:%d\n",
				job->frame_seq_no, job->ctx_id);
			return;
		}
		atomic_set(&job->seninf_dump_state, MTK_CAM_REQ_DBGWORK_S_FINISHED);
		atomic_set(&job->frame_done_work.is_queued, 1);
		*action |= BIT(CAM_JOB_DEQUE_ALL);
		if (job->frame_seq_no == frame_seq_no_inner) {
			// mark buf normal
			_state_trans(job, E_STATE_INNER, E_STATE_DONE_NORMAL);
			dev_dbg(cam->dev, "[%s] ctx_id:%d, mark job:%d NORMAL\n",
				__func__, ctx->stream_id, job->frame_seq_no);
		} else {
			// mark buf error
			_state_trans(job, E_STATE_INNER_HW_DELAY, E_STATE_DONE_MISMATCH);
			dev_dbg(cam->dev, "[%s] ctx_id:%d, mark job:%d ERROR\n",
				__func__, ctx->stream_id, job->frame_seq_no);
		}
	} else {
		*action = 0;
	}
}


static void
_update_event_setting_done(struct mtk_cam_job *job,
		struct mtk_cam_job_event_info *event_info, int *action)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_raw_device *raw_dev =
		dev_get_drvdata(cam->engines.raw_devs[job->proc_engine & 0xF]);
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
		type = job->feature.switch_feature_type;
		if (type != 0 && (!is_job_mstream(job) &&
				!is_job_mstream_change(type))) {
			if (type == EXPOSURE_CHANGE_3_to_1 ||
				type == EXPOSURE_CHANGE_2_to_1)
				stagger_disable(raw_dev);
			else if (type == EXPOSURE_CHANGE_1_to_2 ||
				type == EXPOSURE_CHANGE_1_to_3)
				stagger_enable(raw_dev);
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

static void _update_event_sensor_vsync_normal(struct mtk_cam_job *job,
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

static void _update_event_frame_start_normal(struct mtk_cam_job *job,
		struct mtk_cam_job_event_info *event_info, int *action)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	int frame_idx_inner = event_info->frame_idx_inner;
	int write_cnt_offset, write_cnt;
	u64 time_boot = event_info->ts_ns;
	u64 time_mono = ktime_get_ns();

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
		int switch_type = job->feature.switch_feature_type;

		if (*action & BIT(CAM_JOB_HW_DELAY) ||
			*action & BIT(CAM_JOB_CQ_DELAY))
			return;
		/* job - to be set */
		if (switch_type && job->frame_seq_no > 1 &&
			job->feature.switch_done == 0) {
			// _exp_sensor_switch(ctx, job);
			*action |= BIT(CAM_JOB_SENSOR_SWITCH);
			_state_trans(job, E_STATE_READY, E_STATE_SENSOR);
		} else if (job->state == E_STATE_SENINF) {
			dev_info(ctx->cam->dev, "[SOF] sensor switch delay\n");
			*action |= BIT(CAM_JOB_SENSOR_DELAY);
		} else if (job->state == E_STATE_SENSOR) {
			*action |= BIT(CAM_JOB_APPLY_CQ);
		}

	} else {
		dev_info(ctx->cam->dev,
		"[%s] need check, req:%d, state:%d\n", __func__,
		job->frame_seq_no, job->state);
		*action = 0;
	}

}

static int
_update_event(struct mtk_cam_job *job,
		struct mtk_cam_job_event_info *event_info, int *action)
{
	struct mtk_cam_device *cam = job->src_ctx->cam;
	int engine_type = (event_info->engine >> 8) & 0xFF;

	/* handle frame start */
	if (event_info->irq_type & BIT(CAMSYS_IRQ_FRAME_START)) {
		if (is_job_stagger(job) || is_job_dc(job)) {
			if (engine_type == CAMSYS_ENGINE_CAMSV)
				_update_event_sensor_vsync_normal(
					job, event_info, action);
			else if (engine_type == CAMSYS_ENGINE_RAW)
				_update_event_frame_start_normal(
					job, event_info, action);
		}
		if (job->job_type == RAW_JOB_ON_THE_FLY) {
			if (engine_type == CAMSYS_ENGINE_RAW) {
				_update_event_frame_start_normal(
					job, event_info, action);
				_update_event_sensor_vsync_normal(
					job, event_info, action);
			}
		}
	}
	/* handle try set sensor */
	if (event_info->irq_type & BIT(CAMSYS_IRQ_TRY_SENSOR_SET))
		_update_event_sensor_try_set(job, event_info, action);

	/* handle setting done */
	if (event_info->irq_type & BIT(CAMSYS_IRQ_SETTING_DONE))
		_update_event_setting_done(job, event_info, action);

	/* handle frame done */
	if (event_info->irq_type & BIT(CAMSYS_IRQ_FRAME_DONE))
		_update_event_frame_done(job, event_info, action);

	/* handle meta1 done */
	if (event_info->irq_type & BIT(CAMSYS_IRQ_AFO_DONE))
		_update_event_meta1_done(job, event_info, action);

	dev_dbg(cam->dev,
		"[%s] engine_type:%d, job:%d irq: type:0x%x, out/in:%d/%d, ts:%lld, action:0x%x\n",
		__func__, engine_type, job->frame_seq_no, event_info->irq_type,
		event_info->frame_idx, event_info->frame_idx_inner, event_info->ts_ns, *action);

	return 0;
}

static void job_cancel(struct mtk_cam_job *job)
{
	int pipe_id = get_raw_subdev_idx(job->req->used_pipe);

	if (pipe_id >= 0 && job->req)
		mtk_cam_req_buffer_done(job->req, pipe_id, -1,
			VB2_BUF_STATE_ERROR, job->timestamp);
	cancel_work_sync(&job->frame_done_work.work);
	cancel_work_sync(&job->meta1_done_work.work);
}

static void job_finalize(struct mtk_cam_job *job)
{
	mtk_cam_buffer_pool_return(&job->cq);
	mtk_cam_buffer_pool_return(&job->ipi);
}

static void
_config_job(struct mtk_cam_job *job, struct mtk_cam_ctx *ctx,
		     struct mtk_cam_request *req)
{
	struct media_request_object *obj;
	struct v4l2_ctrl_handler *hdl;
	/* only job used data */
	job->src_ctx = ctx;
	job->req = req;
	job->ctx_id = ctx->stream_id;
	job->sensor = ctx->sensor;
	job->flags = 0;
	if (job->sensor)
		job->flags |= MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_EN;
	job->flags |= MTK_CAM_REQ_S_DATA_FLAG_META1_INDEPENDENT;
	list_for_each_entry(obj, &req->req.objects, list) {
		if (vb2_request_object_is_buffer(obj))
			continue;
		hdl = (struct v4l2_ctrl_handler *)obj->priv;
		if (hdl == ctx->sensor->ctrl_handler)
			job->sensor_hdl_obj = obj;
	}
	atomic_set(&job->frame_done_work.is_queued, 0);
	atomic_set(&job->meta1_done_work.is_queued, 0);
	job->composed = false;

	/* common */
	job->ops.finalize = job_finalize;
	job->ops.cancel = job_cancel;
	job->ops.compose = _compose;
	job->ops.compose_done = _compose_done;
	/* job type dependent */
	switch (job->job_type) {
	case RAW_JOB_ON_THE_FLY:
		job->ops.stream_on = _stream_on;
		job->ops.update_event = _update_event;
		job->ops.apply_isp = _apply_cq;
		job->ops.apply_exp_switch = _exp_sensor_switch;
		job->ops.apply_sensor = _apply_sensor;
		job->ops.frame_done = _frame_done;
		job->ops.afo_done = _meta1_done;
		job->sensor_set_margin = SENSOR_SET_MARGIN_MS;
		job->state = E_STATE_READY;
		break;
	case RAW_JOB_STAGGER:
		job->ops.stream_on = _stream_on_otf_stagger;
		job->ops.update_event = _update_event;
		job->ops.apply_isp = _apply_cq;
		job->ops.apply_exp_switch = _exp_sensor_switch;
		job->ops.apply_sensor = _apply_sensor;
		job->ops.frame_done = _frame_done;
		job->ops.afo_done = _meta1_done;
		job->sensor_set_margin = SENSOR_SET_MARGIN_MS_STAGGER;
		job->state = E_STATE_READY;
		break;
	case RAW_JOB_HW_SUBSAMPLE:
#ifdef NOT_READY
		job->ops.stream_on = _stream_on;
		job->ops.update_event = _update_event_subspl;
		job->ops.apply_isp = _apply_cq;
		job->ops.apply_exp_switch = NULL;
		job->ops.apply_sensor = _apply_sensor_subspl;
		job->ops.frame_done = _frame_done_subspl;
		job->ops.afo_done = _meta1_done;
		job->sensor_set_margin = SENSOR_SET_MARGIN_MS;
		job->state = E_STATE_SUBSPL_READY;
#endif
		break;
	case RAW_JOB_DC:
	case RAW_JOB_DC_STAGGER:
		break;
	case RAW_JOB_OFFLINE:
		break;
	case RAW_JOB_MSTREAM:
	case RAW_JOB_DC_MSTREAM:
		break;
	case RAW_JOB_OFFLINE_STAGGER:
		break;
	case RAW_JOB_OTF_RGBW:
	case RAW_JOB_DC_RGBW:
		break;
	case RAW_JOB_OFFLINE_RGBW:
		break;
	case RAW_JOB_HW_TIMESHARED:
		break;
	case RAW_JOB_ONLY_SV:
		break;
	case RAW_JOB_ONLY_MRAW:
		break;
	default:
		break;
	}
}

static int alloc_image_work_buffer(struct mtk_cam_device_buf *buf, int size,
				   struct device *dev)
{
	struct dma_buf *dbuf;
	int ret;

	WARN_ON(!dev);

	dbuf = mtk_cam_noncached_buffer_alloc(size);

	ret = mtk_cam_device_buf_init(buf, dbuf, dev, size);
	dma_heap_buffer_free(dbuf);
	return  ret;
}

static int alloc_hdr_buffer(struct mtk_cam_ctx *ctx,
			    struct mtk_cam_request *req)
{
	struct mtk_cam_driver_buf_desc *desc = &ctx->hdr_buf_desc;
	struct mtk_cam_device_buf *buf = &ctx->hdr_buffer;
	struct device *dev;
	struct mtk_raw_request_data *d;
	int ret;

	/* FIXME */
	d = &req->raw_data[ctx->raw_subdev_idx];

	/* desc */
	desc->ipi_fmt = sensor_mbus_to_ipi_fmt(d->sink.mbus_code);
	if (WARN_ON_ONCE(desc->ipi_fmt == MTKCAM_IPI_BAYER_PXL_ID_UNKNOWN))
		return -1;

	desc->width = d->sink.width;
	desc->height = d->sink.height;
	desc->stride[0] = mtk_cam_dmao_xsize(d->sink.width, desc->ipi_fmt, 4);
	desc->stride[1] = 0;
	desc->stride[2] = 0;
	desc->size = desc->stride[0] * desc->height;

	/* FIXME: */
	dev = ctx->hw_raw;

	ret = alloc_image_work_buffer(buf, desc->size, dev);
	if (ret)
		return ret;

	desc->daddr = buf->daddr;
	desc->fd = 0; /* TODO: for UFO */

	dev_info(ctx->cam->dev, "%s: fmt %d %dx%d str %d size %zu da 0x%x\n",
		 __func__, desc->ipi_fmt, desc->width, desc->height,
		 desc->stride[0], desc->size, desc->daddr);
	return 0;
}

static int apply_raw_target_clk(struct mtk_cam_ctx *ctx,
				struct mtk_cam_request *req)
{
	struct mtk_raw_request_data *raw_data;
	struct mtk_cam_resource_driver *res;

	raw_data = &req->raw_data[ctx->raw_subdev_idx];
	res = &raw_data->ctrl.resource;

	return mtk_cam_dvfs_update(&ctx->cam->dvfs, ctx->stream_id,
				   res->clk_target);
}

int mtk_cam_job_pack(struct mtk_cam_job *job, struct mtk_cam_ctx *ctx,
		     struct mtk_cam_request *req)
{
	int ret;

	ret = mtk_cam_job_pack_init(job, ctx, req);
	if (ret)
		return ret;

	job->stream_on_seninf = false;
	if (!ctx->used_engine) {
		int selected;

		selected = mtk_cam_select_hw(ctx, job);
		if (!selected)
			return -1;

		if (mtk_cam_occupy_engine(ctx->cam, selected))
			return -1;

		mtk_cam_pm_runtime_engines(&ctx->cam->engines, selected, 1);
		ctx->used_engine = selected;

		if (ctx->hw_raw) {
			struct mtk_raw_device *raw = dev_get_drvdata(ctx->hw_raw);

			initialize(raw, 0);
			/* stagger case */
			if (is_job_stagger(job)) {
				int src_pad_idx = PAD_SRC_RAW0;
				int tgo_pxl_mode = 1;
				int sv_ppl_idx = job->src_ctx->camsv_subdev_idx[0];

				mtk_cam_seninf_set_pixelmode(ctx->seninf,
					src_pad_idx, tgo_pxl_mode);
				mtk_cam_seninf_set_camtg(ctx->seninf, src_pad_idx,
					ctx->cam->pipelines.camsv[sv_ppl_idx].cammux_id);
				mtk_cam_sv_dev_config(ctx, sv_ppl_idx, get_hdr_scen_id(job),
						get_exp_order(job, sv_ppl_idx), tgo_pxl_mode);
				stagger_enable(raw);
			}
			/* subsample case */
			if (is_job_subsample(job))
				subsample_enable(raw);
			/* twin case - TBD */
		}

		job->stream_on_seninf = true;
	}
	job->used_engine = ctx->used_engine;
	job->hw_raw = ctx->hw_raw;

	job->do_ipi_config = false;
	if (!ctx->configured) {
		/* if has raw */
		if (USED_MASK_GET_SUBMASK(&ctx->used_engine, raw)) {
			/* ipi_config_param */
			ret = mtk_cam_job_fill_ipi_config(job, &ctx->ipi_config);
			if (ret)
				return ret;
		}

		job->do_ipi_config = true;
		ctx->configured = true;
	}

	/* clone into job for debug dump */
	job->ipi_config = ctx->ipi_config;

	if (!ctx->not_first_job) {

		if (is_job_stagger(job)) {
			ret = alloc_hdr_buffer(ctx, req);
			if (ret)
				return ret;
		}

		ctx->not_first_job = true;

		apply_raw_target_clk(ctx, req);
	}

	ret = mtk_cam_job_fill_ipi_frame(job);
	if (ret)
		return ret;

	_config_job(job, ctx, req);

	return 0;
}

static void ipi_add_hw_map(struct mtkcam_ipi_config_param *config,
				   int pipe_id, int dev_mask)
{
	int n_maps = config->n_maps;

	WARN_ON(n_maps >= ARRAY_SIZE(config->maps));
	WARN_ON(!dev_mask);

	config->maps[n_maps] = (struct mtkcam_ipi_hw_mapping) {
		.pipe_id = pipe_id,
		.dev_mask = dev_mask,
		.exp_order = 0
	};
	config->n_maps++;
}

static void ipi_add_hw_map_sv(struct mtkcam_ipi_config_param *config,
				   int pipe_id, int dev_mask, int exp_order)
{
	int n_maps = config->n_maps;

	WARN_ON(n_maps >= ARRAY_SIZE(config->maps));
	WARN_ON(!dev_mask);
	pr_info("[%s] pipe_id/mask/exp_order:%d/%d/%d",
		__func__, pipe_id, dev_mask, exp_order);
	config->maps[n_maps] = (struct mtkcam_ipi_hw_mapping) {
		.pipe_id = pipe_id,
		.dev_mask = dev_mask,
		.exp_order = exp_order
	};
	config->n_maps++;
}

static inline struct mtkcam_ipi_crop
v4l2_rect_to_ipi_crop(const struct v4l2_rect *r)
{
	return (struct mtkcam_ipi_crop) {
		.p = (struct mtkcam_ipi_point) {
			.x = r->left,
			.y = r->top,
		},
		.s = (struct mtkcam_ipi_size) {
			.w = r->width,
			.h = r->height,
		},
	};
}

static int raw_set_ipi_input_param(struct mtkcam_ipi_input_param *input,
				   struct mtk_raw_sink_data *sink,
				   int pixel_mode, int dc_sv_pixel_mode,
				   int subsample)
{
	input->fmt = sensor_mbus_to_ipi_fmt(sink->mbus_code);
	input->raw_pixel_id = sensor_mbus_to_ipi_pixel_id(sink->mbus_code);
	input->data_pattern = MTKCAM_IPI_SENSOR_PATTERN_NORMAL;
	input->pixel_mode = pixel_mode;
	input->pixel_mode_before_raw = dc_sv_pixel_mode;
	input->subsample = subsample;
	input->in_crop = v4l2_rect_to_ipi_crop(&sink->crop);

	return 0;
}
static int mtk_cam_job_fill_ipi_config(struct mtk_cam_job *job,
				       struct mtkcam_ipi_config_param *config)
{
	struct mtk_cam_request *req = job->req;
	int used_engine = job->src_ctx->used_engine;
	struct mtkcam_ipi_input_param *input = &config->input;
	int raw_pipe_idx;

	memset(config, 0, sizeof(*config));

	/* assume: at most one raw-subdev is used */
	raw_pipe_idx = get_raw_subdev_idx(req->used_pipe);
	if (raw_pipe_idx != -1) {
		struct mtk_raw_sink_data *sink =
			&req->raw_data[raw_pipe_idx].sink;
		int raw_dev, sv_dev;
		int i;

		config->flags = MTK_CAM_IPI_CONFIG_TYPE_INIT;
		if (is_job_stagger(job))
			config->sw_feature = MTKCAM_IPI_SW_FEATURE_VHDR_STAGGER;
		else
			config->sw_feature = MTKCAM_IPI_SW_FEATURE_NORMAL;

		raw_set_ipi_input_param(input, sink, 1, 0, 0); /* TODO */

		raw_dev = USED_MASK_GET_SUBMASK(&used_engine, raw);
		ipi_add_hw_map(config, MTKCAM_SUBDEV_RAW_0, raw_dev);
		/* dc/stagger case */
		sv_dev = USED_MASK_GET_SUBMASK(&used_engine, camsv);
		for (i = 0; i < job->src_ctx->cam->engines.num_camsv_devices; i++) {
			if (sv_dev & BIT(i)) {
				ipi_add_hw_map_sv(config, MTKCAM_SUBDEV_CAMSV_0 + i,
					BIT(MTKCAM_SUBDEV_CAMSV_0 + i),
					get_exp_order(job, i));
			}
		}
	}

	return 0;
}

static int update_job_cq_buffer_to_ipi_frame(struct mtk_cam_job *job,
					     struct mtkcam_ipi_frame_param *fp)
{
	struct mtk_cam_pool_buffer *cq = &job->cq;

	/* cq offset */
	fp->cur_workbuf_offset = cq->size * cq->priv.index;
	fp->cur_workbuf_size = cq->size;
	return 0;
}

static int map_ipi_imgo_path(int v4l2_raw_path)
{
	switch (v4l2_raw_path) {
	case V4L2_MTK_CAM_RAW_PATH_SELECT_BPC: return MTKCAM_IPI_IMGO_AFTER_BPC;
	case V4L2_MTK_CAM_RAW_PATH_SELECT_FUS: return MTKCAM_IPI_IMGO_AFTER_FUS;
	case V4L2_MTK_CAM_RAW_PATH_SELECT_DGN: return MTKCAM_IPI_IMGO_AFTER_DGN;
	case V4L2_MTK_CAM_RAW_PATH_SELECT_LSC: return MTKCAM_IPI_IMGO_AFTER_LSC;
	case V4L2_MTK_CAM_RAW_PATH_SELECT_LTM: return MTKCAM_IPI_IMGO_AFTER_LTM;
	default:
		break;
	}
	/* un-processed raw frame */
	return MTKCAM_IPI_IMGO_UNPROCESSED;
}

static int update_job_raw_param_to_ipi_frame(struct mtk_cam_job *job,
					     struct mtkcam_ipi_frame_param *fp)
{
	struct mtkcam_ipi_raw_frame_param *p = &fp->raw_param;
	struct mtk_cam_request *req = job->req;
	struct mtk_raw_ctrl_data *ctrl;
	int raw_pipe_idx;

	/* assume: at most one raw-subdev is used */
	raw_pipe_idx = get_raw_subdev_idx(req->used_pipe);
	if (raw_pipe_idx == -1)
		return 0;

	ctrl = &req->raw_data[raw_pipe_idx].ctrl;

	p->imgo_path_sel = map_ipi_imgo_path(ctrl->raw_path);
	p->hardware_scenario = MTKCAM_IPI_HW_PATH_ON_THE_FLY;
	p->bin_flag = BIN_OFF;
	p->exposure_num = 1;
	p->previous_exposure_num = 1;
	dev_info(job->src_ctx->cam->dev, "[%s] job_type:%d feature:0x%x exp:%d/%d", __func__,
			job->job_type, ctrl->feature, p->exposure_num, p->previous_exposure_num);
	if (is_job_stagger(job)) {
		p->hardware_scenario = MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER;
		if (is_feature_2_exposure(ctrl->feature)) {
			p->exposure_num = 2;
			p->previous_exposure_num = 2;
		} else if (is_feature_3_exposure(ctrl->feature)) {
			p->exposure_num = 3;
			p->previous_exposure_num = 3;
		}
		dev_info(job->src_ctx->cam->dev, "[%s] stagger feature:0x%x exp:%d/%d", __func__,
			ctrl->feature, p->exposure_num, p->previous_exposure_num);
	}

	return 0;
}

struct req_buffer_helper {
	struct mtk_cam_job *job;
	struct mtkcam_ipi_frame_param *fp;

	int ii_idx; /* image in */
	int io_idx; /* imgae out */
	int mi_idx; /* meta in */
	int mo_idx; /* meta out */

	/* for stagger case */
	bool filled_hdr_buffer;
};

/* TODO: refine this function... */
static int mtk_cam_fill_img_buf(struct mtkcam_ipi_img_output *img_out,
				struct mtk_cam_buffer *buf)
{
	struct mtk_cam_cached_image_info *info = &buf->image_info;

	u32 pixelformat = info->v4l2_pixelformat;
	u32 width = info->width;
	u32 height = info->height;
	u32 stride = info->bytesperline[0];
	dma_addr_t daddr = buf->daddr;

	u32 aligned_width;
	unsigned int addr_offset = 0;
	int i;

	img_out->buf[0][0].ccd_fd = buf->vbb.vb2_buf.planes[0].m.fd;

	if (is_mtk_format(pixelformat)) {
		const struct mtk_format_info *info;

		info = mtk_format_info(pixelformat);
		if (WARN_ON(!info))
			return -EINVAL;

		if (info->mem_planes != 1) {
			pr_info("do not support non-contiguous mplane\n");
			return -EINVAL;
		}

		aligned_width = stride / info->bpp[0];
		if (is_yuv_ufo(pixelformat)) {
			aligned_width = ALIGN(width, 64);
			img_out->buf[0][0].iova = daddr;
			img_out->fmt.stride[0] = aligned_width * info->bit_r_num
				/ info->bit_r_den;
			img_out->buf[0][0].size = img_out->fmt.stride[0] * height;
			img_out->buf[0][0].size += img_out->fmt.stride[0] * height / 2;
			img_out->buf[0][0].size += ALIGN((aligned_width / 64), 8) * height;
			img_out->buf[0][0].size += ALIGN((aligned_width / 64), 8) * height
				/ 2;
			img_out->buf[0][0].size += sizeof(struct UfbcBufferHeader);

			buf_printk("plane:%d stride:%d plane_size:%d addr:0x%x\n",
				   0,
				   img_out->fmt.stride[0],
				   img_out->buf[0][0].size,
				   img_out->buf[0][0].iova);
		} else if (is_raw_ufo(pixelformat)) {
			aligned_width = ALIGN(width, 64);
			img_out->buf[0][0].iova = daddr;
			img_out->fmt.stride[0] = aligned_width * info->bit_r_num /
				info->bit_r_den;
			img_out->buf[0][0].size = img_out->fmt.stride[0] * height;
			img_out->buf[0][0].size += ALIGN((aligned_width / 64), 8) * height;
			img_out->buf[0][0].size += sizeof(struct UfbcBufferHeader);

			buf_printk("plane:%d stride:%d plane_size:%d addr:0x%x\n",
				   0, img_out->fmt.stride[0], img_out->buf[0][0].size,
				   img_out->buf[0][0].iova);
		} else {
			for (i = 0; i < info->comp_planes; i++) {
				unsigned int hdiv = (i == 0) ? 1 : info->hdiv;
				unsigned int vdiv = (i == 0) ? 1 : info->vdiv;

				img_out->buf[0][i].iova = daddr + addr_offset;
				img_out->fmt.stride[i] = info->bpp[i] *
					DIV_ROUND_UP(aligned_width, hdiv);
				img_out->buf[0][i].size = img_out->fmt.stride[i]
					* DIV_ROUND_UP(height, vdiv);
				addr_offset += img_out->buf[0][i].size;

				buf_printk("plane:%d stride:%d plane_size:%d addr:0x%x\n",
					   i,
					   img_out->fmt.stride[i],
					   img_out->buf[0][i].size,
					   img_out->buf[0][i].iova);
			}
		}
	} else {
		const struct v4l2_format_info *info;

		info = v4l2_format_info(pixelformat);
		if (WARN_ON(!info))
			return -EINVAL;

		if (info->mem_planes != 1) {
			pr_info("do not support non contiguous mplane\n");
			return -EINVAL;
		}

		aligned_width = stride / info->bpp[0];
		for (i = 0; i < info->comp_planes; i++) {
			unsigned int hdiv = (i == 0) ? 1 : info->hdiv;
			unsigned int vdiv = (i == 0) ? 1 : info->vdiv;

			img_out->buf[0][i].iova = daddr + addr_offset;
			img_out->fmt.stride[i] = info->bpp[i] *
				DIV_ROUND_UP(aligned_width, hdiv);
			img_out->buf[0][i].size = img_out->fmt.stride[i]
				* DIV_ROUND_UP(height, vdiv);
			addr_offset += img_out->buf[0][i].size;

			buf_printk("stride:%d plane_size:%d addr:0x%x\n",
				   img_out->fmt.stride[i],
				   img_out->buf[0][i].size,
				   img_out->buf[0][i].iova);
		}
	}

	return 0;
}

static int fill_img_fmt(struct mtkcam_ipi_pix_fmt *ipi_pfmt,
			struct mtk_cam_buffer *buf)
{
	struct mtk_cam_cached_image_info *info = &buf->image_info;
	int i;

	ipi_pfmt->format = mtk_cam_get_img_fmt(info->v4l2_pixelformat);
	ipi_pfmt->s = (struct mtkcam_ipi_size) {
		.w = info->width,
		.h = info->height,
	};

	for (i = 0; i < ARRAY_SIZE(ipi_pfmt->stride); i++)
		ipi_pfmt->stride[i] = i < ARRAY_SIZE(info->bytesperline) ?
			info->bytesperline[i] : 0;
	return 0;
}
static int fill_img_in_hdr(struct mtkcam_ipi_img_input *ii,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node)
{
	int ret;

	/* uid */
	ii->uid = node->uid;
	ii->uid.id = MTKCAM_IPI_RAW_RAWI_2;
	/* fmt */
	ret = fill_img_fmt(&ii->fmt, buf);


	/* FIXME: porting workaround */
	ii->buf[0].size = buf->image_info.bytesperline[0] * buf->image_info.height;
	ii->buf[0].iova = buf->daddr;
	ii->buf[0].ccd_fd = buf->vbb.vb2_buf.planes[0].m.fd;

	pr_debug("[%s] buf->daddr:0x%x, io->buf[0][0].iova:0x%x, size%d", __func__,
		buf->daddr, ii->buf[0].iova, ii->buf[0].size);

	return ret;
}

static int fill_img_out_hdr(struct mtkcam_ipi_img_output *io,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node)
{
	int ret;

	/* uid */
	io->uid = node->uid;

	/* fmt */
	ret = fill_img_fmt(&io->fmt, buf);


	/* FIXME: porting workaround */
	io->buf[0][0].size = buf->image_info.bytesperline[0] * buf->image_info.height;
	io->buf[0][0].iova = buf->daddr + io->buf[0][0].size;
	io->buf[0][0].ccd_fd = buf->vbb.vb2_buf.planes[0].m.fd;

	pr_info("[%s] buf->daddr:0x%x, io->buf[0][0].iova:0x%x, size:%d", __func__,
		buf->daddr, io->buf[0][0].iova, io->buf[0][0].size);
	/* crop */
	io->crop = v4l2_rect_to_ipi_crop(&buf->image_info.crop);

	/* FIXME: porting workaround */
	if (WARN_ON_ONCE(!io->crop.s.w || !io->crop.s.h)) {
		io->crop = (struct mtkcam_ipi_crop) {
			.p = (struct mtkcam_ipi_point) {
				.x = 0,
				.y = 0,
			},
			.s = (struct mtkcam_ipi_size) {
				.w = io->fmt.s.w,
				.h = io->fmt.s.h,
			},
		};
	}

	buf_printk("%s: %s %dx%d @%d,%d-%dx%d\n",
		   __func__,
		   node->desc.name,
		   io->fmt.s.w, io->fmt.s.h,
		   io->crop.p.x, io->crop.p.y, io->crop.s.w, io->crop.s.h);
	return ret;
}

static int fill_img_in_driver_buf(struct mtkcam_ipi_img_input *ii,
				  struct mtkcam_ipi_uid uid,
				  struct mtk_cam_driver_buf_desc *desc)
{
	int i;

	/* uid */
	ii->uid = uid;

	/* fmt */
	ii->fmt.format = desc->ipi_fmt;
	ii->fmt.s = (struct mtkcam_ipi_size) {
		.w = desc->width,
		.h = desc->height,
	};

	for (i = 0; i < ARRAY_SIZE(ii->fmt.stride); i++)
		ii->fmt.stride[i] = i < ARRAY_SIZE(desc->stride) ?
			desc->stride[i] : 0;

	/* buf */
	ii->buf[0].size = desc->size;
	ii->buf[0].iova = desc->daddr;
	ii->buf[0].ccd_fd = 0; /* TODO: ufo : desc->fd; */

	buf_printk("%s: %dx%d sz %zu\n",
		   __func__, desc->width, desc->height, desc->size);
	return 0;
}

static int fill_img_out(struct mtkcam_ipi_img_output *io,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node)
{
	int ret;

	/* uid */
	io->uid = node->uid;

	/* fmt */
	ret = fill_img_fmt(&io->fmt, buf);


	/* FIXME: porting workaround */
	if (node->desc.dma_port == MTKCAM_IPI_RAW_IMGO) {
		io->buf[0][0].iova = buf->daddr;
		io->buf[0][0].ccd_fd = buf->vbb.vb2_buf.planes[0].m.fd;
	} else {
		/* buf */
		ret = ret || mtk_cam_fill_img_buf(io, buf);
	}

	/* crop */
	io->crop = v4l2_rect_to_ipi_crop(&buf->image_info.crop);

	/* FIXME: porting workaround */
	if (WARN_ON_ONCE(!io->crop.s.w || !io->crop.s.h)) {
		io->crop = (struct mtkcam_ipi_crop) {
			.p = (struct mtkcam_ipi_point) {
				.x = 0,
				.y = 0,
			},
			.s = (struct mtkcam_ipi_size) {
				.w = io->fmt.s.w,
				.h = io->fmt.s.h,
			},
		};
	}

	buf_printk("%s: %s %dx%d @%d,%d-%dx%d\n",
		   __func__,
		   node->desc.name,
		   io->fmt.s.w, io->fmt.s.h,
		   io->crop.p.x, io->crop.p.y, io->crop.s.w, io->crop.s.h);
	return ret;
}

static int update_raw_image_buf_to_ipi_frame(struct req_buffer_helper *helper,
					     struct mtk_cam_buffer *buf,
					     struct mtk_cam_video_device *node)
{
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	int ret = -1;

	switch (node->desc.dma_port) {
	case MTKCAM_IPI_RAW_RAWI_2:
		/* TODO */
		pr_info("%s:%d not implemented yet\n", __func__, __LINE__);
		break;
	case MTKCAM_IPI_RAW_IMGO:
		{
			struct mtkcam_ipi_img_output *out;
			struct mtkcam_ipi_img_input *in;

			out = &fp->img_outs[helper->io_idx];
			++helper->io_idx;
			if (is_job_stagger(helper->job)) {
				ret = fill_img_out_hdr(out, buf, node);
				in = &fp->img_ins[helper->ii_idx];
				++helper->ii_idx;
				ret = fill_img_in_hdr(in, buf, node);

				helper->filled_hdr_buffer = true;
			} else {
				ret = fill_img_out(out, buf, node);
			}
		}
		break;
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
		{
			struct mtkcam_ipi_img_output *out;

			out = &fp->img_outs[helper->io_idx];
			++helper->io_idx;

			ret = fill_img_out(out, buf, node);
		}
		break;
	default:
		pr_info("%s %s: not supported port: %d\n",
			__FILE__, __func__, node->desc.dma_port);
	}

	return ret;
}

#define FILL_META_IN_OUT(_ipi_meta, _cam_buf, _id)		\
{								\
	typeof(_ipi_meta) _m = (_ipi_meta);			\
	typeof(_cam_buf) _b = (_cam_buf);			\
								\
	_m->buf.ccd_fd = _b->vbb.vb2_buf.planes[0].m.fd;	\
	_m->buf.size = _b->meta_info.buffersize;		\
	_m->buf.iova = _b->daddr;				\
	_m->uid.id = _id;					\
}

static int update_raw_meta_buf_to_ipi_frame(struct req_buffer_helper *helper,
					    struct mtk_cam_buffer *buf,
					    struct mtk_cam_video_device *node)
{
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	int ret = 0;

	switch (node->desc.dma_port) {
	case MTKCAM_IPI_RAW_META_STATS_CFG:
		{
			struct mtkcam_ipi_meta_input *in;

			in = &fp->meta_inputs[helper->mi_idx];
			++helper->mi_idx;

			FILL_META_IN_OUT(in, buf, node->desc.dma_port);
		}
		break;
	case MTKCAM_IPI_RAW_META_STATS_0:
	case MTKCAM_IPI_RAW_META_STATS_1:
		{
			struct mtkcam_ipi_meta_output *out;
			void *vaddr;

			out = &fp->meta_outputs[helper->mo_idx];
			++helper->mo_idx;

			FILL_META_IN_OUT(out, buf, node->desc.dma_port);

			vaddr = vb2_plane_vaddr(&buf->vbb.vb2_buf, 0);
			ret = CALL_PLAT_V4L2(set_meta_stats_info,
					     node->desc.dma_port,
					     vaddr);

			if (node->desc.dma_port == MTKCAM_IPI_RAW_META_STATS_0) {
				struct mtk_cam_job *job = helper->job;

				job->timestamp_buf = vaddr +
					GET_PLAT_V4L2(timestamp_buffer_ofst);
			}
		}
		break;
	default:
		pr_info("%s %s: not supported port: %d\n",
			__FILE__, __func__, node->desc.dma_port);
		ret = -1;
	}

	WARN_ON(ret);
	return ret;
}

static bool belong_to_current_ctx(struct mtk_cam_job *job, int ipi_pipe_id)
{
	int ctx_used_pipe;
	int idx;
	bool ret = false;

	WARN_ON(!job->src_ctx);

	ctx_used_pipe = job->src_ctx->used_pipe;

	/* TODO: update for 7s */
	if (is_raw_subdev(ipi_pipe_id)) {
		idx = ipi_pipe_id;
		ret = USED_MASK_HAS(&ctx_used_pipe, raw, idx);
	} else if (is_camsv_subdev(ipi_pipe_id)) {
		idx = ipi_pipe_id - MTKCAM_SUBDEV_CAMSV_START;
		ret = USED_MASK_HAS(&ctx_used_pipe, camsv, idx);
	} else if (is_mraw_subdev(ipi_pipe_id)) {
		idx = ipi_pipe_id - MTKCAM_SUBDEV_MRAW_START;
		ret = USED_MASK_HAS(&ctx_used_pipe, mraw, idx);
	} else {
		WARN_ON(1);
	}

	return ret;
}

static int update_cam_buf_to_ipi_frame(struct req_buffer_helper *helper,
				       struct mtk_cam_buffer *buf)
{
	struct mtk_cam_video_device *node;
	int pipe_id;
	int ret = -1;

	node = mtk_cam_buf_to_vdev(buf);
	pipe_id = node->uid.pipe_id;

	/* skip if it does not belong to current ctx */
	if (!belong_to_current_ctx(helper->job, pipe_id))
		return 0;

	if (is_raw_subdev(pipe_id)) {
		if (node->desc.image)
			ret = update_raw_image_buf_to_ipi_frame(helper,
								buf, node);
		else
			ret = update_raw_meta_buf_to_ipi_frame(helper,
							       buf, node);
	}

	/* TODO: mraw/camsv */

	if (ret)
		pr_info("failed to update pipe %x buf %s\n",
			pipe_id, node->desc.name);

	return ret;
}

static void reset_unused_io_of_ipi_frame(struct req_buffer_helper *helper)
{
	struct mtkcam_ipi_frame_param *fp;
	int i;

	fp = helper->fp;

	for (i = helper->ii_idx; i < ARRAY_SIZE(fp->img_ins); i++) {
		struct mtkcam_ipi_img_input *io = &fp->img_ins[i];

		io->uid = (struct mtkcam_ipi_uid) {0, 0};
	}

	for (i = helper->io_idx; i < ARRAY_SIZE(fp->img_outs); i++) {
		struct mtkcam_ipi_img_output *io = &fp->img_outs[i];

		io->uid = (struct mtkcam_ipi_uid) {0, 0};
	}

	for (i = helper->mi_idx; i < ARRAY_SIZE(fp->meta_inputs); i++) {
		struct mtkcam_ipi_meta_input *io = &fp->meta_inputs[i];

		io->uid = (struct mtkcam_ipi_uid) {0, 0};
	}

	for (i = helper->mo_idx; i < ARRAY_SIZE(fp->meta_outputs); i++) {
		struct mtkcam_ipi_meta_output *io = &fp->meta_outputs[i];

		io->uid = (struct mtkcam_ipi_uid) {0, 0};
	}
}

static int update_work_buffer_to_ipi_frame(struct req_buffer_helper *helper)
{
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	struct mtk_cam_job *job = helper->job;
	struct mtk_cam_ctx *ctx = job->src_ctx;
	int ret = 0;

	if (is_job_stagger(job)) {
		struct mtkcam_ipi_img_input *ii;
		struct mtkcam_ipi_uid uid;

		if (helper->filled_hdr_buffer)
			return 0;

		uid.pipe_id = get_raw_subdev_idx(ctx->used_pipe);
		uid.id = MTKCAM_IPI_RAW_RAWI_2;

		ii = &fp->img_ins[helper->ii_idx];
		++helper->ii_idx;

		ret = fill_img_in_driver_buf(ii, uid, &ctx->hdr_buf_desc);
	}

	return ret;
}

static int update_job_buffer_to_ipi_frame(struct mtk_cam_job *job,
					  struct mtkcam_ipi_frame_param *fp)
{
	struct req_buffer_helper helper;
	struct mtk_cam_request *req = job->req;
	struct mtk_cam_buffer *buf;
	int ret;

	memset(&helper, 0, sizeof(helper));
	helper.job = job;
	helper.fp = fp;

	list_for_each_entry(buf, &req->buf_list, list) {
		ret = ret || update_cam_buf_to_ipi_frame(&helper, buf);
	}

	/* update necessary working buffer */
	ret = ret || update_work_buffer_to_ipi_frame(&helper);

	reset_unused_io_of_ipi_frame(&helper);

	return ret;
}

static int mtk_cam_job_fill_ipi_frame(struct mtk_cam_job *job)
{
	struct mtkcam_ipi_frame_param *fp;
	int ret;

	fp = (struct mtkcam_ipi_frame_param *)job->ipi.vaddr;

	ret = update_job_cq_buffer_to_ipi_frame(job, fp)
		|| update_job_raw_param_to_ipi_frame(job, fp)
		|| update_job_buffer_to_ipi_frame(job, fp);

	if (ret)
		pr_info("%s: failed.", __func__);

	return ret;
}
