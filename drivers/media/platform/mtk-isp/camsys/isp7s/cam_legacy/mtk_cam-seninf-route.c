// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of_graph.h>
#include <linux/of_device.h>

#include <linux/videodev2.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>

#include "mtk_cam-seninf.h"
#include "mtk_cam-seninf-route.h"
#include "mtk_cam-seninf-if.h"
#include "mtk_cam-seninf-hw.h"
#include "imgsensor-user.h"
#include "mtk_cam-seninf-ca.h"

#include "mtk_cam-defs.h"

#define to_std_fmt_code(code) \
	((code) & 0xFFFF)

void mtk_cam_seninf_init_res(struct seninf_core *core)
{
	int i;

	INIT_LIST_HEAD(&core->list_mux);
	for (i = 0; i < g_seninf_ops->mux_num; i++) {
		core->mux[i].idx = i;
		list_add_tail(&core->mux[i].list, &core->list_mux);
	}

#ifdef SENINF_DEBUG
	INIT_LIST_HEAD(&core->list_cam_mux);
	for (i = 0; i < g_seninf_ops->cam_mux_num; i++) {
		core->cam_mux[i].idx = i;
		list_add_tail(&core->cam_mux[i].list, &core->list_cam_mux);
	}
#endif
}

static enum CAM_TYPE_ENUM mtk_cam_seninf_get_vc_type(u8 out_pad)
{
	switch (out_pad) {
	case PAD_SRC_RAW0:
	case PAD_SRC_RAW1:
	case PAD_SRC_RAW2:
	case PAD_SRC_RAW_EXT0:
		return TYPE_RAW;
	default:
		return TYPE_CAMSV_SAT;
	}
}

void mtk_cam_seninf_alloc_cammux(struct seninf_ctx *ctx)
{
	int i;
	struct seninf_core *core = ctx->core;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;
	struct seninf_cam_mux *ent;
	enum CAM_TYPE_ENUM cam_type;
	bool auto_alloc;

	pr_info("[%s]+\n", __func__);

	mutex_lock(&core->mutex);

	/* allocate cam muxs if assigned */
	for (i = 0; i < vcinfo->cnt; i++) {
		auto_alloc = true;
		vc = &vcinfo->vc[i];

		/* cam is assigned */
		if (ctx->pad2cam[vc->out_pad][0] != 0xff) {
			list_for_each_entry(ent, &core->list_cam_mux, list) {
				if (ent->idx == ctx->pad2cam[vc->out_pad][0]) {
					list_move_tail(&ent->list,
						       &ctx->list_cam_mux);
					dev_info(ctx->dev, "pad%d -> cam%d\n",
						 vc->out_pad, ent->idx);
					auto_alloc = false;
					break;
				}
			}
			if (auto_alloc) {
				dev_info(ctx->dev, "cam%d had been occupied\n",
					 ctx->pad2cam[vc->out_pad][0]);
				ctx->pad2cam[vc->out_pad][0] = 0xff;
			}
		}
	}

	/* auto allocate cam muxs */
	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];
		if (ctx->pad2cam[vc->out_pad][0] == 0xff) {
			cam_type = mtk_cam_seninf_get_vc_type(vc->out_pad);
			list_for_each_entry(ent, &core->list_cam_mux, list) {
				if (ent->idx >= core->cammux_range[(unsigned int)cam_type].first &&
				    ent->idx <= core->cammux_range[(unsigned int)cam_type].second) {
					list_move_tail(&ent->list,
						       &ctx->list_cam_mux);
					ctx->pad2cam[vc->out_pad][0] = ent->idx;
					ctx->pad_tag_id[vc->out_pad][0] = ent->idx % 8;
					vc->dest_cnt = 1;
					dev_info(ctx->dev, "pad%d -> cam%d\n",
						 vc->out_pad, ent->idx);
					break;
				}
			}
		}
	}

	mutex_unlock(&core->mutex);

	pr_info("[%s]-\n", __func__);
}

struct seninf_mux *mtk_cam_seninf_mux_get_by_type(struct seninf_ctx *ctx,
						enum CAM_TYPE_ENUM cam_type)
{
	struct seninf_core *core = ctx->core;
	struct seninf_mux *ent = NULL;

	mutex_lock(&core->mutex);

	list_for_each_entry(ent, &core->list_mux, list) {
		if (ent->idx >= core->mux_range[cam_type].first
		    && ent->idx <= core->mux_range[cam_type].second) {
			list_move_tail(&ent->list,
				       &ctx->list_mux);
			mutex_unlock(&core->mutex);
			return ent;
		}
	}

	mutex_unlock(&core->mutex);

	return NULL;
}

#define SAT_MUX_FACTOR 8

int mux2mux_vr(struct seninf_ctx *ctx, int mux, int cammux)
{
	int sat_mux_factor = SAT_MUX_FACTOR;
	struct seninf_core *core = ctx->core;
	int mux_vr = mux;
	int sat_mux_first = core->mux_range[TYPE_CAMSV_SAT].first;
	int sat_mux_second = core->mux_range[TYPE_CAMSV_SAT].second;
	int sat_cammux_first = core->cammux_range[TYPE_CAMSV_SAT].first;
	int sat_cammux_second = core->cammux_range[TYPE_CAMSV_SAT].second;
	int num_sat_mux = sat_mux_second - sat_mux_first + 1;

	if (mux < sat_mux_first)
		mux_vr = mux;
	else if (mux >= sat_mux_first && mux <= sat_mux_second) {
		mux_vr = sat_mux_first + ((mux - sat_mux_first) * sat_mux_factor);
		if (cammux >= sat_cammux_first && cammux <= sat_cammux_second)
			mux_vr += ((cammux - sat_cammux_first) % sat_mux_factor);
	} else
		mux_vr = (mux - sat_mux_second) + (num_sat_mux * sat_mux_factor) - 1;

	return mux_vr;
}

int mux_vr2mux(struct seninf_ctx *ctx, int mux_vr)
{
	int sat_mux_factor = SAT_MUX_FACTOR;
	struct seninf_core *core = ctx->core;
	int mux = mux_vr;
	int sat_mux_first = core->mux_range[TYPE_CAMSV_SAT].first;
	int sat_mux_last = core->mux_range[TYPE_CAMSV_SAT].second;
	int num_sat_mux = sat_mux_last - sat_mux_first + 1;
	int sat_mux_vr_first = sat_mux_first;
	int sat_mux_vr_last = sat_mux_first + (sat_mux_factor * num_sat_mux) - 1;

	if (mux_vr < sat_mux_vr_first)
		mux = mux_vr;
	else if ((mux_vr >= sat_mux_vr_first) && (mux_vr <= sat_mux_vr_last))
		mux = sat_mux_first + ((mux_vr - sat_mux_vr_first) / sat_mux_factor);
	else
		mux = sat_mux_last + (mux_vr - sat_mux_vr_last);

	return mux;
}

enum CAM_TYPE_ENUM cammux2camtype(struct seninf_ctx *ctx, int cammux)
{
	struct seninf_core *core = ctx->core;
	enum CAM_TYPE_ENUM type = TYPE_CAMSV_SAT;
	int i;

	for (i = 0; i < TYPE_MAX_NUM; i++) {
		if (cammux >= core->cammux_range[i].first
		    && cammux <= core->cammux_range[i].second) {
			type = (enum CAM_TYPE_ENUM) i;
			break;
		}
	}

	return type;
}

static int cammux_tag_2_fsync_target_id(struct seninf_ctx *ctx, int cammux, int tag)
{
	int cammux_factor = 8;
	int fsync_camsv_start_id = 5;
	struct seninf_core *core = ctx->core;
	enum CAM_TYPE_ENUM type = cammux2camtype(ctx, cammux);
	int ret = 0xff;

	if (cammux < 0 || cammux >= 0xff) {
		ret = 0xff;
	} else if (type == TYPE_CAMSV_SAT) {
		ret = fsync_camsv_start_id
			+ (cammux - core->cammux_range[TYPE_CAMSV_SAT].first);
	} else if (type == TYPE_CAMSV_NORMAL) {
		ret = ((cammux - core->cammux_range[TYPE_CAMSV_NORMAL].first) * cammux_factor)
			+ core->cammux_range[TYPE_CAMSV_NORMAL].first
			+ fsync_camsv_start_id + tag;
	} else if (type == TYPE_RAW) {
		ret = 1 + (cammux - core->cammux_range[TYPE_RAW].first);
	}

	dev_dbg(ctx->dev, "[%s] cammux = %d, tag = %d, target_id = %d\n",
		 __func__, cammux, tag, ret);

	return ret;
}

static void setup_fsync_vsync_src_pad(struct seninf_ctx *ctx,
	const u64 fsync_ext_vsync_pad_code)
{
	const unsigned int has_processed_data = (unsigned int)
		((fsync_ext_vsync_pad_code >> PAD_SRC_RAW_EXT0) & (u64)1);
	const unsigned int has_general_embedded = (unsigned int)
		((fsync_ext_vsync_pad_code >> PAD_SRC_GENERAL0) & (u64)1);

	/* default using raw0 vsync signal */
	ctx->fsync_vsync_src_pad = PAD_SRC_RAW0;

	/* check case to overwrite */
	/* --- if pre-isp case */
	if (has_processed_data) {
		if (has_general_embedded) {
			ctx->fsync_vsync_src_pad = PAD_SRC_GENERAL0;

			dev_info(ctx->dev,
				"[%s] NOTICE: set fsync_vsync_src_pad:%d(%d:RAW0/%d:GENERAL0), fsync_ext_vsync_pad_code:%#llx(processed_data:%u/general_embedded:%u)\n",
				__func__,
				ctx->fsync_vsync_src_pad,
				PAD_SRC_RAW0, PAD_SRC_GENERAL0,
				fsync_ext_vsync_pad_code,
				has_processed_data, has_general_embedded);
		} else {
			ctx->fsync_vsync_src_pad = PAD_SRC_RAW0;

			dev_info(ctx->dev,
				"[%s] WARNING: fsync_ext_vsync_pad_code:%#llx, has processed_data:%u, but general_embedded:%u, force set fsync_vsync_src_pad:%d(%d:RAW0/%d:GENERAL0)\n",
				__func__,
				fsync_ext_vsync_pad_code,
				has_processed_data, has_general_embedded,
				ctx->fsync_vsync_src_pad,
				PAD_SRC_RAW0, PAD_SRC_GENERAL0);
		}
	}
}

static void chk_is_fsync_vsync_src(struct seninf_ctx *ctx, const int pad_id)
{
	const int vsync_src_pad = ctx->fsync_vsync_src_pad;

	if (vsync_src_pad != pad_id)
		return;

	if (vsync_src_pad == PAD_SRC_RAW0) {
		// notify vc->cam
		notify_fsync_listen_target_with_kthread(ctx, 0);
	} else if (vsync_src_pad == PAD_SRC_GENERAL0) {
		dev_info(ctx->dev,
			"[%s] NOTICE: pad_id:%d, fsync_vsync_src_pad:%d(%d:RAW0/%d:GENERAL0), fsync listen 3A-meta(general-embedded) vsync signal\n",
			__func__,
			pad_id,
			vsync_src_pad,
			PAD_SRC_RAW0,
			PAD_SRC_GENERAL0);

		notify_fsync_listen_target_with_kthread(ctx, 0);
	} else {
		/* unexpected case */
		dev_info(ctx->dev,
			"[%s] ERROR: unknown fsync_vsync_src_pad:%d(%d:RAW0/%d:GENERAL0) type, pad_id:%d\n",
			__func__,
			vsync_src_pad,
			PAD_SRC_RAW0,
			PAD_SRC_GENERAL0,
			pad_id);
	}
}

void mtk_cam_seninf_mux_put(struct seninf_ctx *ctx, struct seninf_mux *mux)
{
	struct seninf_core *core = ctx->core;
	int i, j;

	mutex_lock(&core->mutex);
	list_move_tail(&mux->list, &core->list_mux);
	for (i = 0; i < VC_CH_GROUP_MAX_NUM; i++) {
		for (j = 0; j < TYPE_MAX_NUM; j++) {
			if (ctx->mux_by[i][j] == mux)
				ctx->mux_by[i][j] = NULL;
		}
	}
	mutex_unlock(&core->mutex);
}

void mtk_cam_seninf_get_vcinfo_test(struct seninf_ctx *ctx)
{
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;

	vcinfo->cnt = 0;

	if (ctx->is_test_model == 1) {
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;
	} else if (ctx->is_test_model == 2) {
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_STAGGER_NE;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_STAGGER_ME;
		vc->out_pad = PAD_SRC_RAW1;
		vc->group = 0;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_STAGGER_SE;
		vc->out_pad = PAD_SRC_RAW2;
		vc->group = 0;
	} else if (ctx->is_test_model == 3) {
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x30;
		vc->feature = VC_PDAF_STATS;
		vc->out_pad = PAD_SRC_PDAF0;
		vc->group = 0;
	} else if (ctx->is_test_model == 4) {
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW1;
		vc->group = 0;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW2;
		vc->group = 0;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_PDAF0;
		vc->group = 0;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_PDAF1;
		vc->group = 0;
	} else if (ctx->is_test_model == 5) {
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_W_DATA;
		vc->out_pad = PAD_SRC_RAW_W0;
		vc->group = 0;
	}
}

struct seninf_vc *mtk_cam_seninf_get_vc_by_pad(struct seninf_ctx *ctx, int idx)
{
	int i;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;

	for (i = 0; i < vcinfo->cnt; i++) {
		if (vcinfo->vc[i].out_pad == idx)
			return &vcinfo->vc[i];
	}

	return NULL;
}

int mtk_cam_seninf_get_pad_data_info(struct v4l2_subdev *sd,
				unsigned int pad,
				struct mtk_seninf_pad_data_info *result)
{
	struct seninf_vc *pvc = NULL;
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);

	if (!result)
		return -1;

	memset(result, 0, sizeof(*result));
	pvc = mtk_cam_seninf_get_vc_by_pad(ctx, pad);
	if (pvc) {
		result->feature = pvc->feature;
		result->mux = pvc->dest[0].mux;
		result->exp_hsize = pvc->exp_hsize;
		result->exp_vsize = pvc->exp_vsize;

		return 0;
	}

	return -1;
}

static int get_mbus_format_by_dt(int dt)
{
	switch (dt) {
	case 0x2a:
		return MEDIA_BUS_FMT_SBGGR8_1X8;
	case 0x2b:
		return MEDIA_BUS_FMT_SBGGR10_1X10;
	case 0x2c:
		return MEDIA_BUS_FMT_SBGGR12_1X12;
	default:
		/* default raw8 for other data types */
		return MEDIA_BUS_FMT_SBGGR8_1X8;
	}
}

static int get_vcinfo_by_pad_fmt(struct seninf_ctx *ctx)
{
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;

	vcinfo->cnt = 0;

	switch (to_std_fmt_code(ctx->fmt[PAD_SINK].format.code)) {
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;
		break;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2c;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;
		break;
	default:
		return -1;
	}

	return 0;
}

#ifdef SENINF_VC_ROUTING
#define has_op(master, op) \
	(master->ops && master->ops->op)
#define call_op(master, op) \
	(has_op(master, op) ? master->ops->op(master) : 0)

/* Copy the one value to another. */
static void ptr_to_ptr(struct v4l2_ctrl *ctrl,
		       union v4l2_ctrl_ptr from, union v4l2_ctrl_ptr to)
{
	if (ctrl == NULL) {
		pr_info("%s ctrl == NULL\n", __func__);
		return;
	}
	memcpy(to.p, from.p, ctrl->elems * ctrl->elem_size);
}

/* Copy the current value to the new value */
static void cur_to_new(struct v4l2_ctrl *ctrl)
{
	if (ctrl == NULL) {
		pr_info("%s ctrl == NULL\n", __func__);
		return;
	}
	ptr_to_ptr(ctrl, ctrl->p_cur, ctrl->p_new);
}

/* Helper function to get a single control */
static int get_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ctrl *master = ctrl->cluster[0];
	int ret = 0;
	int i;

	if (ctrl->flags & V4L2_CTRL_FLAG_WRITE_ONLY) {
		pr_info("%s ctrl->flags&V4L2_CTRL_FLAG_WRITE_ONLY\n",
			__func__);
		return -EACCES;
	}

	v4l2_ctrl_lock(master);
	if (ctrl->flags & V4L2_CTRL_FLAG_VOLATILE) {
		pr_info("%s master->ncontrols:%d",
			__func__, master->ncontrols);
		for (i = 0; i < master->ncontrols; i++)
			cur_to_new(master->cluster[i]);
		ret = call_op(master, g_volatile_ctrl);
	}
	v4l2_ctrl_unlock(master);

	return ret;
}

int mtk_cam_seninf_get_csi_param(struct seninf_ctx *ctx)
{
	int ret = 0;

	struct mtk_csi_param *csi_param = &ctx->csi_param;
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;
#if AOV_GET_PARAM
	struct seninf_core *core = ctx->core;
#endif

	if (!ctx->sensor_sd)
		return -EINVAL;

	if (ctx->is_aov_real_sensor) {
		switch (core->aov_csi_clk_switch_flag) {
		case CSI_CLK_52:
		case CSI_CLK_65:
		case CSI_CLK_104:
		case CSI_CLK_130:
		case CSI_CLK_242:
		case CSI_CLK_260:
		case CSI_CLK_312:
		case CSI_CLK_416:
		case CSI_CLK_499:
			ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
				V4L2_CID_MTK_AOV_SWITCH_RX_PARAM);
			if (!ctrl) {
				dev_info(ctx->dev,
					"no(%s) in subdev(%s)\n",
					__func__, sensor_sd->name);
				return -EINVAL;
			}
			dev_info(ctx->dev,
				"[%s] aov csi clk switch to (%u)\n",
				__func__, core->aov_csi_clk_switch_flag);
			v4l2_ctrl_s_ctrl(ctrl, (unsigned int)core->aov_csi_clk_switch_flag);
			break;
		default:
			dev_info(ctx->dev,
				"[%s] csi clk not support (%u)\n",
				__func__, core->aov_csi_clk_switch_flag);
			return -EINVAL;
		}
	}

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler, V4L2_CID_MTK_CSI_PARAM);
	if (!ctrl) {
		dev_info(ctx->dev, "%s, no V4L2_CID_MTK_CSI_PARAM %s\n",
			__func__, sensor_sd->name);
		return -EINVAL;
	}
	memset(csi_param, 0, sizeof(struct mtk_csi_param));

	ctrl->p_new.p = csi_param;

	ret = get_ctrl(ctrl);
	dev_info(ctx->dev, "%s get_ctrl ret:%d %d|%d|%d|%d|%d|%d|%d\n", __func__,
		ret, csi_param->cphy_settle,
		csi_param->dphy_clk_settle,
		csi_param->dphy_data_settle,
		csi_param->dphy_trail,
		csi_param->not_fixed_trail_settle,
		csi_param->legacy_phy,
		csi_param->dphy_csi2_resync_dmy_cycle);

#if AOV_GET_PARAM
	if (!(core->aov_sensor_id < 0) &&
		!(core->current_sensor_id < 0) &&
		(core->current_sensor_id == core->aov_sensor_id)) {
		g_aov_param.cphy_settle = csi_param->cphy_settle;
		g_aov_param.dphy_clk_settle = csi_param->dphy_clk_settle;
		g_aov_param.dphy_data_settle = csi_param->dphy_data_settle;
		g_aov_param.dphy_trail = csi_param->dphy_trail;
		g_aov_param.legacy_phy = csi_param->legacy_phy;
		g_aov_param.not_fixed_trail_settle = csi_param->not_fixed_trail_settle;
		g_aov_param.dphy_csi2_resync_dmy_cycle = csi_param->dphy_csi2_resync_dmy_cycle;
	}
#endif

	return 0;
}

int mtk_cam_seninf_get_vcinfo(struct seninf_ctx *ctx)
{
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;
	struct v4l2_subdev_format raw_fmt;
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct mtk_mbus_frame_desc fd;
	struct v4l2_ctrl *ctrl;
	u64 fsync_ext_vsync_pad_code = 0;
	int i, raw_cnt;
	int desc;
	int ret = 0;

	if (!ctx->sensor_sd)
		return -EINVAL;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler, V4L2_CID_MTK_FRAME_DESC);
	if (!ctrl) {
		dev_info(ctx->dev, "%s, no V4L2_CID_MTK_FRAME_DESC %s\n",
			__func__, sensor_sd->name);
		return -EINVAL;
	}
	memset(&fd, 0, sizeof(struct mtk_mbus_frame_desc));
	ctrl->p_new.p = &fd;

	ret = get_ctrl(ctrl);

	if (ret || fd.type != MTK_MBUS_FRAME_DESC_TYPE_CSI2 || !fd.num_entries) {
		dev_info(ctx->dev, "%s get_ctrl ret:%d num_entries:%d type:%d\n", __func__,
			ret, fd.num_entries, fd.type);
		return get_vcinfo_by_pad_fmt(ctx);
	}

	vcinfo->cnt = 0;
	raw_cnt = 0;

	for (i = 0; i < fd.num_entries; i++) {
		vc = &vcinfo->vc[vcinfo->cnt];
		vc->vc = fd.entry[i].bus.csi2.channel;
		vc->dt = fd.entry[i].bus.csi2.data_type;
		desc = fd.entry[i].bus.csi2.user_data_desc;
		vc->dt_remap_to_type = fd.entry[i].bus.csi2.dt_remap_to_type;

		switch (desc) {
		case VC_3HDR_Y:
			vc->feature = VC_3HDR_Y;
			vc->out_pad = PAD_SRC_HDR0;
			break;
		case VC_3HDR_AE:
			vc->feature = VC_3HDR_AE;
			vc->out_pad = PAD_SRC_HDR1;
			break;
		case VC_3HDR_FLICKER:
			vc->feature = VC_3HDR_FLICKER;
			vc->out_pad = PAD_SRC_HDR2;
			break;
		case VC_PDAF_STATS:
			vc->feature = VC_PDAF_STATS;
			vc->out_pad = PAD_SRC_PDAF0;
			break;
		case VC_PDAF_STATS_PIX_1:
			vc->feature = VC_PDAF_STATS_PIX_1;
			vc->out_pad = PAD_SRC_PDAF1;
			break;
		case VC_PDAF_STATS_PIX_2:
			vc->feature = VC_PDAF_STATS_PIX_2;
			vc->out_pad = PAD_SRC_PDAF2;
			break;
		case VC_PDAF_STATS_ME_PIX_1:
			vc->feature = VC_PDAF_STATS_ME_PIX_1;
			vc->out_pad = PAD_SRC_PDAF3;
			break;
		case VC_PDAF_STATS_ME_PIX_2:
			vc->feature = VC_PDAF_STATS_ME_PIX_2;
			vc->out_pad = PAD_SRC_PDAF4;
			break;
		case VC_PDAF_STATS_SE_PIX_1:
			vc->feature = VC_PDAF_STATS_SE_PIX_1;
			vc->out_pad = PAD_SRC_PDAF5;
			break;
		case VC_PDAF_STATS_SE_PIX_2:
			vc->feature = VC_PDAF_STATS_SE_PIX_2;
			vc->out_pad = PAD_SRC_PDAF6;
			break;
		case VC_YUV_Y:
			if (raw_cnt >= 3) {
				dev_info(ctx->dev,
					 "too much raw data\n");
				continue;
			}
			vc->feature = VC_RAW_DATA;
			vc->out_pad = PAD_SRC_RAW0;

			++raw_cnt;
			vc->group = VC_CH_GROUP_RAW1;
			break;
		case VC_YUV_UV:
			if (raw_cnt >= 3) {
				dev_info(ctx->dev,
					 "too much raw data\n");
				continue;
			}
			vc->feature = VC_RAW_DATA;
			vc->out_pad = PAD_SRC_RAW1;

			++raw_cnt;
			vc->group = VC_CH_GROUP_RAW2;
			break;
		case VC_GENERAL_EMBEDDED:
			vc->feature = VC_GENERAL_EMBEDDED;
			vc->out_pad = PAD_SRC_GENERAL0;

			/* for determin fsync vsync signal src (pre-isp) */
			fsync_ext_vsync_pad_code |=
				((u64)1 << PAD_SRC_GENERAL0);
			break;
		case VC_RAW_PROCESSED_DATA:
			vc->feature = VC_RAW_DATA;
			vc->out_pad = PAD_SRC_RAW_EXT0;

			vc->group = VC_CH_GROUP_RAW1;

			/* for determin fsync vsync signal src (pre-isp) */
			fsync_ext_vsync_pad_code |=
				((u64)1 << PAD_SRC_RAW_EXT0);
			break;
		case VC_RAW_W_DATA:
			vc->feature = VC_RAW_DATA;
			vc->out_pad = PAD_SRC_RAW_W0;
			break;
		case VC_RAW_ME_W_DATA:
			vc->feature = VC_RAW_ME_W_DATA;
			vc->out_pad = PAD_SRC_RAW_W1;
			break;
		case VC_RAW_SE_W_DATA:
			vc->feature = VC_RAW_SE_W_DATA;
			vc->out_pad = PAD_SRC_RAW_W2;
			break;
		default:
			if (vc->dt == 0x2a || vc->dt == 0x2b ||
			    vc->dt == 0x2c) {
				if (raw_cnt >= 3) {
					dev_info(ctx->dev,
						"too much raw data\n");
					continue;
				}

				switch (desc) {
				case VC_STAGGER_NE:
					vc->out_pad = PAD_SRC_RAW0;
					vc->group = VC_CH_GROUP_RAW1;
					break;
				case VC_STAGGER_ME:
					vc->out_pad = PAD_SRC_RAW1;
					vc->group = VC_CH_GROUP_RAW2;
					break;
				case VC_STAGGER_SE:
					vc->out_pad = PAD_SRC_RAW2;
					vc->group = VC_CH_GROUP_RAW3;
					break;
				default:
					vc->out_pad = PAD_SRC_RAW0 + raw_cnt;
					vc->group = VC_CH_GROUP_RAW1 + raw_cnt;
					break;
				}
				++raw_cnt;
				vc->feature = VC_RAW_DATA;
			} else {
				dev_info(ctx->dev, "unknown desc %d, dt 0x%x\n",
					desc, vc->dt);
				continue;
			}
			break;
		}

		vc->exp_hsize = fd.entry[i].bus.csi2.hsize;
		vc->exp_vsize = fd.entry[i].bus.csi2.vsize;

		switch (vc->dt) {
		case 0x28:
			vc->bit_depth = 6;
			break;
		case 0x29:
			vc->bit_depth = 7;
			break;
		case 0x2A:
		case 0x1E:
		case 0x1C:
		case 0x1A:
		case 0x18:
			vc->bit_depth = 8;
			break;
		case 0x2B:
		case 0x1F:
		case 0x19:
		case 0x1D:
			vc->bit_depth = 10;
			break;
		case 0x2C:
			vc->bit_depth = 12;
			break;
		case 0x2D:
			vc->bit_depth = 14;
			break;
		case 0x2E:
			vc->bit_depth = 16;
			break;
		case 0x2F:
			vc->bit_depth = 20;
			break;
		default:
			vc->bit_depth = 8;
			break;
		}


		/* update pad fotmat */
		if (vc->exp_hsize && vc->exp_vsize) {
			ctx->fmt[vc->out_pad].format.width = vc->exp_hsize;
			ctx->fmt[vc->out_pad].format.height = vc->exp_vsize;
		}

		if (vc->feature == VC_RAW_DATA) {
			raw_fmt.pad = ctx->sensor_pad_idx;
			raw_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
			ret = v4l2_subdev_call(ctx->sensor_sd, pad, get_fmt,
					       NULL, &raw_fmt);
			if (ret) {
				dev_info(ctx->dev, "no get_fmt in %s\n",
					ctx->sensor_sd);
				ctx->fmt[vc->out_pad].format.code =
					get_mbus_format_by_dt(vc->dt);
			} else {
				ctx->fmt[vc->out_pad].format.code =
					to_std_fmt_code(raw_fmt.format.code);
			}
		} else {
			ctx->fmt[vc->out_pad].format.code =
				get_mbus_format_by_dt(vc->dt);
		}

		dev_info(ctx->dev,
			"%s vc[%d] vc 0x%x dt 0x%x pad %d exp %dx%d grp 0x%x code 0x%x, fsync_ext_vsync_pad_code:%#llx\n",
			__func__,
			vcinfo->cnt, vc->vc, vc->dt, vc->out_pad,
			vc->exp_hsize, vc->exp_vsize, vc->group,
			ctx->fmt[vc->out_pad].format.code,
			fsync_ext_vsync_pad_code);

		vcinfo->cnt++;
	}

	setup_fsync_vsync_src_pad(ctx, fsync_ext_vsync_pad_code);

	return 0;
}
#endif // SENINF_VC_ROUTING

#ifndef SENINF_VC_ROUTING
int mtk_cam_seninf_get_vcinfo(struct seninf_ctx *ctx)
{
	return get_vcinfo_by_pad_fmt(ctx);
}
#endif

void mtk_cam_seninf_release_mux(struct seninf_ctx *ctx)
{
	struct seninf_mux *ent, *tmp;

	list_for_each_entry_safe(ent, tmp, &ctx->list_mux, list) {
		mtk_cam_seninf_mux_put(ctx, ent);
	}
}

int mtk_cam_seninf_is_vc_enabled(struct seninf_ctx *ctx, struct seninf_vc *vc)
{
#ifdef SENINF_VC_ROUTING
	return 1;
#else
	int i;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;

#ifdef SENINF_DEBUG
	if (ctx->is_test_streamon)
		return 1;
#endif

	if (vc->out_pad != PAD_SRC_RAW0 &&
		vc->out_pad != PAD_SRC_RAW1 &&
		vc->out_pad != PAD_SRC_RAW2) {
		if (media_entity_remote_pad(&ctx->pads[vc->out_pad]))
			return 1;
		else
			return 0;
	}

	for (i = 0; i < vcinfo->cnt; i++) {
		u8 out_pad = vcinfo->vc[i].out_pad;

		if ((out_pad == PAD_SRC_RAW0 ||
			 out_pad == PAD_SRC_RAW1 ||
			 out_pad == PAD_SRC_RAW2) &&
			media_entity_remote_pad(&ctx->pads[out_pad]))
			return 1;
	}

	return 0;

#endif
}

int mtk_cam_seninf_is_di_enabled(struct seninf_ctx *ctx, u8 ch, u8 dt)
{
	int i;
	struct seninf_vc *vc;

	for (i = 0; i < ctx->vcinfo.cnt; i++) {
		vc = &ctx->vcinfo.vc[i];
		if (vc->vc == ch && vc->dt == dt) {
#ifdef SENINF_DEBUG
			if (ctx->is_test_streamon)
				return 1;
#endif
			if (media_entity_remote_pad(&ctx->pads[vc->out_pad]))
				return 1;
			return 0;
		}
	}

	return 0;
}

/* Debug Only */
#ifdef SENINF_DEBUG
void mtk_cam_seninf_release_cam_mux(struct seninf_ctx *ctx)
{
	struct seninf_core *core = ctx->core;
	struct seninf_cam_mux *ent, *tmp;

	pr_info("[%s]+\n", __func__);

	mutex_lock(&core->mutex);

	/* release all cam muxs */
	list_for_each_entry_safe(ent, tmp, &ctx->list_cam_mux, list) {
		list_move_tail(&ent->list, &core->list_cam_mux);
	}

	mutex_unlock(&core->mutex);

	pr_info("[%s]-\n", __func__);
}
#endif

int mtk_cam_seninf_get_pixelmode(struct v4l2_subdev *sd,
				 int pad_id, int *pixelMode)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_vc *vc;

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, pad_id);
	if (!vc) {
		pr_info("%s: invalid pad=%d\n", __func__, pad_id);
		return -1;
	}

	*pixelMode = vc->pixel_mode;

	return 0;
}

int mtk_cam_seninf_set_pixelmode(struct v4l2_subdev *sd,
				 int pad_id, int pixelMode)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_vc *vc;

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, pad_id);
	if (!vc) {
		pr_info("%s: invalid pad=%d\n", __func__, pad_id);
		return -1;
	}

	vc->pixel_mode = pixelMode;
	if (ctx->streaming) {
		update_isp_clk(ctx);
		g_seninf_ops->_update_mux_pixel_mode(ctx, vc->dest[0].mux, pixelMode);
	}
	// if streaming, update ispclk and update pixle mode seninf mux and reset

	return 0;
}

static struct seninf_mux *get_mux(struct seninf_ctx *ctx, struct seninf_vc *vc,
				  u8 dest_cam_type, int intf)
{
	int skip_mux_ctrl;
	u32 group_src = VC_CH_GROUP_ALL;
	struct seninf_mux *mux = NULL;
	int hsPol, vsPol;

	// TODO
	hsPol = 0;
	vsPol = 0;

	switch (dest_cam_type) {
	case TYPE_RAW:
	case TYPE_UISP:
		group_src = vc->group;
		break;
	default:
		break;
	}

	/* alloc mux by group */
	if (ctx->mux_by[group_src][dest_cam_type]) {
		mux = ctx->mux_by[group_src][dest_cam_type];
		skip_mux_ctrl = 1;
	} else {
		mux = mtk_cam_seninf_mux_get_by_type(ctx, dest_cam_type);
		ctx->mux_by[group_src][dest_cam_type] = mux;
		skip_mux_ctrl = 0;
	}

	if (!mux) {
		mtk_cam_seninf_release_mux(ctx);
		return NULL;
	}

	if (!skip_mux_ctrl) {
		g_seninf_ops->_mux(ctx, mux->idx);
		g_seninf_ops->_set_mux_ctrl(ctx, mux->idx,
					    hsPol, vsPol,
					    group_src + MIPI_SENSOR,
					    vc->pixel_mode);

		g_seninf_ops->_set_top_mux_ctrl(ctx, mux->idx, intf);

		//TODO
		//mtk_cam_seninf_set_mux_crop(ctx, mux->idx, 0, 2327, 0);
	}

	return mux;
}

int _mtk_cam_seninf_set_camtg_with_dest_idx(struct v4l2_subdev *sd, int pad_id,
				int camtg, int tag_id, u8 dest_set,
				bool from_set_camtg)
{
	int vc_en, old_camtg, old_mux, old_mux_vr;
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_vc *vc;
	struct seninf_vc_out_dest *dest;
	bool disable_last = from_set_camtg;
	int en_tag = ((tag_id >= 0) && (tag_id <= 31));
	struct seninf_mux *mux = NULL;

	if (pad_id < PAD_SRC_RAW0 || pad_id >= PAD_MAXCNT)
		return -EINVAL;

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, pad_id);
	if (!vc)
		return -EINVAL;

	if (!from_set_camtg && !ctx->streaming) {
		dev_info(ctx->dev, "%s !from_set_camtg && !ctx->streaming\n", __func__);
		return -EINVAL;
	}

	if (dest_set >= MAX_DEST_NUM) {
		dev_info(ctx->dev, "%s reach max dest_set %d, vc->dest_cnt = %u\n",
			 __func__, dest_set, vc->dest_cnt);
		return -EINVAL;
	}

	dest = &vc->dest[dest_set];

	ctx->pad2cam[pad_id][dest_set] = camtg;
	ctx->pad_tag_id[pad_id][dest_set] = tag_id;

	vc_en = mtk_cam_seninf_is_vc_enabled(ctx, vc);

	/* change cam-mux while streaming */
	if (ctx->streaming && vc_en) {
		g_seninf_ops->_set_cam_mux_dyn_en(ctx, true, camtg, 0/*index*/);
#ifdef SENSOR_SECURE_MTEE_SUPPORT
		if (ctx->is_secure == 1) {
			dest->cam = camtg;
			dest->mux_vr = mux2mux_vr(ctx, dest->mux, dest->cam);

			dev_info(ctx->dev, "Sensor Secure CA");
			g_seninf_ops->_set_cammux_vc(ctx, dest->cam,
							vc->vc, vc->dt,
							!!vc->dt, !!vc->dt);
			g_seninf_ops->_set_cammux_src(ctx, dest->mux_vr, dest->cam,
							vc->exp_hsize,
							vc->exp_vsize,
							vc->dt);
			g_seninf_ops->_set_cammux_chk_pixel_mode(ctx,
							dest->cam,
							vc->pixel_mode);
			g_seninf_ops->_cammux(ctx, dest->cam);

			chk_is_fsync_vsync_src(ctx, pad_id);

			if (!seninf_ca_open_session())
				dev_info(ctx->dev, "seninf_ca_open_session fail");

			dev_info(ctx->dev, "Sensor kernel ca_checkpipe");
			seninf_ca_checkpipe(ctx->SecInfo_addr);
		} else {
#endif // SENSOR_SECURE_MTEE_SUPPORT

			/* disable old */
			old_camtg = dest->cam;
			old_mux = dest->mux;
			old_mux_vr = dest->mux_vr;

			if (camtg == 0xff) {
				dest->cam = 0xff;
				if (disable_last) {
					g_seninf_ops->_switch_to_cammux_inner_page(ctx, true);
					g_seninf_ops->_set_cammux_next_ctrl(ctx, 0x3f, old_camtg);
					g_seninf_ops->_disable_cammux(ctx, old_camtg);
				}
			} else {
				/* enable new */
				dest->cam = camtg;
				dest->tag = tag_id;

				dest->cam_type = cammux2camtype(ctx, dest->cam);
				mux = get_mux(ctx, vc, dest->cam_type, ctx->seninfIdx);
				if (!mux) {
					dev_info(ctx->dev, "mux is null\n");
					return -EBUSY;
				}
				// set vc split
				g_seninf_ops->_set_mux_vc_split(ctx, mux->idx,
								dest->tag, vc->vc);

				dest->mux = mux->idx;
				dest->mux_vr = mux2mux_vr(ctx, dest->mux, dest->cam);

				g_seninf_ops->_switch_to_cammux_inner_page(ctx, true);
				g_seninf_ops->_set_cammux_next_ctrl(ctx, 0x3f, dest->cam);

				g_seninf_ops->_switch_to_cammux_inner_page(ctx, false);

				g_seninf_ops->_set_cammux_vc(ctx, dest->cam,
								vc->vc, vc->dt,
								!!vc->dt, !!vc->dt);
				g_seninf_ops->_set_cammux_tag(ctx, dest->cam,
							vc->vc, vc->dt, dest->tag, en_tag);
				g_seninf_ops->_set_cammux_src(ctx, dest->mux_vr, dest->cam,
								vc->exp_hsize,
								vc->exp_vsize,
								vc->dt);
				g_seninf_ops->_set_cammux_chk_pixel_mode(ctx,
								dest->cam,
								vc->pixel_mode);
				if (old_camtg != 0xff && disable_last) {
					//disable old in next sof
					g_seninf_ops->_disable_cammux(ctx, old_camtg);
				}
				g_seninf_ops->_cammux(ctx, dest->cam); //enable in next sof
				g_seninf_ops->_switch_to_cammux_inner_page(ctx, true);
				g_seninf_ops->_set_cammux_next_ctrl(ctx, dest->mux_vr, dest->cam);
				if (old_camtg != 0xff && disable_last)
					g_seninf_ops->_set_cammux_next_ctrl(ctx,
									dest->mux_vr, old_camtg);

				chk_is_fsync_vsync_src(ctx, pad_id);
			}
			seninf_logi(ctx,
				"pad %d dest %u mux %d -> %d mux_vr %d -> %d cam %d -> %d, tag %d\n",
				vc->out_pad, dest_set, old_mux, dest->mux,
				old_mux_vr, dest->mux_vr, old_camtg, dest->cam, dest->tag);

#ifdef SENSOR_SECURE_MTEE_SUPPORT
		}
#endif
	} else {
		seninf_logi(ctx,
			"pad_id %d, dest %u camtg %d, ctx->streaming %d, vc_en %d, tag %d\n",
			pad_id, dest_set, camtg, ctx->streaming, vc_en, tag_id);
	}

	return 0;
}

static int _mtk_cam_seninf_reset_cammux(struct seninf_ctx *ctx, int pad_id)
{
	struct seninf_vc *vc;
	int old_camtg;
	u8 j;

	if (pad_id < PAD_SRC_RAW0 || pad_id >= PAD_MAXCNT)
		return -EINVAL;

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, pad_id);
	if (!vc)
		return -EINVAL;

	if (!ctx->streaming) {
		dev_info(ctx->dev, "%s !ctx->streaming\n", __func__);
		return -EINVAL;
	}

	for (j = 0; j < vc->dest_cnt; j++) {
		old_camtg = vc->dest[j].cam;

		if (old_camtg != 0xff)
			g_seninf_ops->_set_cam_mux_dyn_en(ctx, true, old_camtg, 0/*index*/);

		g_seninf_ops->_switch_to_cammux_inner_page(ctx, false);
		if (old_camtg != 0xff) {
			//disable old in next sof
			g_seninf_ops->_disable_cammux(ctx, old_camtg);
		}
		g_seninf_ops->_switch_to_cammux_inner_page(ctx, true);

		dev_info(ctx->dev, "disable outer of pad_id(%d) old camtg(%d)\n",
			 pad_id, old_camtg);
	}

	vc->dest_cnt = 0;

	return 0;
}

int _mtk_cam_seninf_set_camtg(struct v4l2_subdev *sd, int pad_id, int camtg, int tag_id,
			      bool from_set_camtg)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_vc *vc;
	int set;

	if (pad_id < PAD_SRC_RAW0 || pad_id >= PAD_MAXCNT)
		return -EINVAL;

	if (camtg < 0 || camtg == 0xff) {
		/* disable all dest */
		return _mtk_cam_seninf_reset_cammux(ctx, pad_id);
	}

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, pad_id);
	if (!vc)
		return -EINVAL;

	set = vc->dest_cnt;

	if (set < MAX_DEST_NUM) {
		vc->dest_cnt += 1;
		return _mtk_cam_seninf_set_camtg_with_dest_idx(sd, pad_id,
						camtg, tag_id, set, from_set_camtg);
	}

	return -EINVAL;
}

int mtk_cam_seninf_set_camtg_camsv(struct v4l2_subdev *sd, int pad_id, int camtg, int tag_id)
{
	return _mtk_cam_seninf_set_camtg(sd, pad_id, camtg, tag_id, true);
}

int mtk_cam_seninf_get_tag_order(struct v4l2_subdev *sd, int pad_id)
{
	/* seninf todo: tag order */
	/* 0: first exposure 1: second exposure 2: last exposure */
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;
	int ret = 0;
	int i = 0;
	int exposure_num = 0;

	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];
		if (vc->out_pad == PAD_SRC_PDAF1 ||
			vc->out_pad == PAD_SRC_PDAF3 ||
			vc->out_pad == PAD_SRC_PDAF5) {
			exposure_num++;
		}
	}

	switch (pad_id) {
	case PAD_SRC_PDAF3:
		switch (exposure_num) {
		case 3:
			ret = 1;
			break;
		case 2:
			ret = 2;
			break;
		default:
			ret = 0;
			break;
		}
		break;
	case PAD_SRC_PDAF5:
		ret = (exposure_num == 3) ? 2 : 0;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

int mtk_cam_seninf_get_vsync_order(struct v4l2_subdev *sd)
{
	/* todo: 0: bayer first 1: w first */
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;
	int i = 0;

	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];

		switch (vc->out_pad) {
		case PAD_SRC_RAW0:
		case PAD_SRC_RAW1:
		case PAD_SRC_RAW2:
			return MTKCAM_IPI_ORDER_BAYER_FIRST;

		case PAD_SRC_RAW_W0:
		case PAD_SRC_RAW_W1:
		case PAD_SRC_RAW_W2:
			return MTKCAM_IPI_ORDER_W_FIRST;

		default:
			break;
		}
	}

	return MTKCAM_IPI_ORDER_BAYER_FIRST;
}

int mtk_cam_seninf_set_camtg(struct v4l2_subdev *sd, int pad_id, int camtg)
{
	return mtk_cam_seninf_set_camtg_camsv(sd, pad_id, camtg, -1);
}

int mtk_cam_seninf_s_stream_mux(struct seninf_ctx *ctx)
{
	int i;
	u8 j;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;
	struct seninf_vc_out_dest *dest;
	int vc_sel, dt_sel, dt_en;
	int intf = ctx->seninfIdx;
	struct seninf_mux *mux;
	int en_tag = 0;
	struct seninf_core *core = ctx->core;

	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];

		vc->enable = mtk_cam_seninf_is_vc_enabled(ctx, vc);
		if (!vc->enable) {
			dev_info(ctx->dev, "vc[%d] pad %d. skip\n",
				 i, vc->feature, vc->out_pad);
			continue;
		}

		if (ctx->is_aov_real_sensor) {
			if (!(core->aov_sensor_id < 0) &&
				!(core->current_sensor_id < 0) &&
				(core->current_sensor_id == core->aov_sensor_id)) {
				dev_info(ctx->dev,
					"[%s] aov streaming mux & cammux workaround on scp\n",
					__func__);
				break;
			}
		}

		if (!vc->dest_cnt) {
			dev_info(ctx->dev, "not set camtg yet, vc[%d] pad %d intf %d dest_cnt %u\n",
				 i, vc->out_pad, intf, vc->dest_cnt);
			continue;
		}

		for (j = 0; j < vc->dest_cnt; j++) {
			dest = &vc->dest[j];

			dest->cam = ctx->pad2cam[vc->out_pad][j];

			dest->cam_type = cammux2camtype(ctx, dest->cam);

			mux = get_mux(ctx, vc, dest->cam_type, intf);
			if (!mux)
				return -EBUSY;

			dest->mux = mux->idx;

			if (dest->cam != 0xff) {
				dest->mux_vr = mux2mux_vr(ctx, dest->mux, dest->cam);
				dest->tag = ctx->pad_tag_id[vc->out_pad][j];
				// set vc split
				g_seninf_ops->_set_mux_vc_split(ctx, dest->mux,
								dest->tag, vc->vc);

				vc_sel = vc->vc;
				dt_sel = vc->dt;
				dt_en = !!dt_sel;

				if (dest->tag <= 31)
					en_tag = 1;
				else
					en_tag = 0;

				// set outter
				g_seninf_ops->_switch_to_cammux_inner_page(ctx, false);
				g_seninf_ops->_set_cammux_vc(ctx, dest->cam,
							     vc_sel, dt_sel, dt_en, dt_en);
				g_seninf_ops->_set_cammux_tag(ctx, dest->cam,
							      vc_sel, dt_sel, dest->tag, en_tag);

				g_seninf_ops->_set_cammux_src(ctx, dest->mux_vr, dest->cam,
							      vc->exp_hsize, vc->exp_vsize, vc->dt);

				g_seninf_ops->_set_cammux_chk_pixel_mode(ctx,
									 dest->cam,
									 vc->pixel_mode);
				g_seninf_ops->_cammux(ctx, dest->cam);
				g_seninf_ops->_set_cammux_next_ctrl(ctx, dest->mux_vr, dest->cam);
				g_seninf_ops->_switch_to_cammux_inner_page(ctx, true);


				/* CMD_SENINF_FINALIZE_CAM_MUX */
				g_seninf_ops->_set_cammux_vc(ctx, dest->cam,
							     vc_sel, dt_sel, dt_en, dt_en);
				g_seninf_ops->_set_cammux_tag(ctx, dest->cam,
							      vc_sel, dt_sel, dest->tag, en_tag);

				g_seninf_ops->_set_cammux_src(ctx, dest->mux_vr, dest->cam,
							      vc->exp_hsize, vc->exp_vsize, vc->dt);

				g_seninf_ops->_set_cammux_chk_pixel_mode(ctx,
									 dest->cam,
									 vc->pixel_mode);
				g_seninf_ops->_cammux(ctx, dest->cam);

				// inner next
				g_seninf_ops->_set_cammux_next_ctrl(ctx, dest->mux_vr, dest->cam);

				seninf_logi(ctx,
					"vc[%d] dest[%u] pad %d intf %d mux %d next/src mux_vr %d cam %d tag %d vc 0x%x dt 0x%x first %d\n",
					i, j, vc->out_pad, intf, dest->mux, dest->mux_vr,
					dest->cam, dest->tag, vc_sel, dt_sel, en_tag);

#ifdef SENSOR_SECURE_MTEE_SUPPORT
				if (ctx->is_secure == 1) {
					dev_info(ctx->dev, "Sensor kernel init seninf_ca");
					if (!seninf_ca_open_session())
						dev_info(ctx->dev, "seninf_ca_open_session fail");

					dev_info(ctx->dev, "Sensor kernel ca_checkpipe");
					seninf_ca_checkpipe(ctx->SecInfo_addr);
				}
#endif
			} else {
				dest->mux_vr = 0xFF;
				seninf_logi(ctx, "invalid camtg, vc[%d] pad %d intf %d cam %d\n",
					 i, vc->out_pad, intf, dest->cam);
			}
		}
	}

	return 0;
}

static int mtk_cam_seninf_get_fsync_vsync_src_cam_info(struct seninf_ctx *ctx)
{
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;
	int i;
	int target_id = -1;

	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];

		if (vc->out_pad == ctx->fsync_vsync_src_pad) {
			/* vsync_src_pad must be first-raw or general-embedded */
			target_id = cammux_tag_2_fsync_target_id(ctx,
					vc->dest[0].cam, vc->dest[0].tag);

			dev_info(ctx->dev,
				"[%s] fsync_vsync_src_pad:%d(%d:RAW0/%d:GENERAL0) => vc->cam:%d, vc->tag:%d => target_id:%d\n",
				__func__,
				ctx->fsync_vsync_src_pad,
				PAD_SRC_RAW0,
				PAD_SRC_GENERAL0,
				vc->dest[0].cam,
				vc->dest[0].tag,
				target_id);

			return target_id;
		}
	}

	dev_info(ctx->dev, "%s: no raw data in vc channel\n", __func__);
	return -1;
}

bool
mtk_cam_seninf_streaming_mux_change(struct mtk_cam_seninf_mux_param *param)
{
	struct v4l2_subdev *sd = NULL;
	int pad_id = -1;
	int camtg = -1;
	int tag_id = -1;
	struct seninf_ctx *ctx;
	int i;
	char *buf = NULL;
	char *strptr = NULL;
	size_t buf_sz = 0;
	size_t remind = 0;
	int num = 0;

	if (!param)
		return false;

	remind = buf_sz = (param->num) * 50;
	strptr = buf = kzalloc(buf_sz + 1, GFP_KERNEL);
	if (!buf)
		return false;

	// disable all camtg changing first
	for (i = 0; i < param->num; i++) {
		sd = param->settings[i].seninf;
		pad_id = param->settings[i].source;
		camtg = param->settings[i].camtg;
		ctx = container_of(sd, struct seninf_ctx, subdev);

		_mtk_cam_seninf_reset_cammux(ctx, pad_id);
	}

	// set new camtg
	for (i = 0; i < param->num; i++) {
		sd = param->settings[i].seninf;
		pad_id = param->settings[i].source;
		camtg = param->settings[i].camtg;
		tag_id = param->settings[i].tag_id;
		ctx = container_of(sd, struct seninf_ctx, subdev);

		_mtk_cam_seninf_set_camtg(sd, pad_id, camtg, tag_id, false);

		// log
		num = snprintf(strptr, remind, "pad_id[%d] %d, ctx->camtg[%d] %d, ",
			       i, param->settings[i].source,
			       i, param->settings[i].camtg);
		if (num < 0) {
			dev_info(ctx->dev, "snprintf retuns error ret = %d\n", num);
			break;
		}

		remind -= num;
		strptr += num;

	}

	dev_info(ctx->dev,
		 "%s: param->num %d, %s %llu|%llu\n",
		 __func__, param->num,
		 buf,
		 ktime_get_boottime_ns(),
		 ktime_get_ns());

	kfree(buf);

	return true;
}


static void mtk_notify_vsync_fn(struct kthread_work *work)
{
	struct mtk_seninf_work *seninf_work =
		container_of(work, struct mtk_seninf_work, work);
	struct seninf_ctx *ctx = seninf_work->ctx;
	struct v4l2_ctrl *ctrl;
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	unsigned int sof_cnt = seninf_work->data.sof;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
				V4L2_CID_VSYNC_NOTIFY);
	if (!ctrl) {
		dev_info(ctx->dev, "%s, no V4L2_CID_VSYNC_NOTIFY %s\n",
			__func__,
			sensor_sd->name);
		return;
	}

//	dev_info(ctx->dev, "%s sof %s cnt %d\n",
//		__func__,
//		sensor_sd->name,
//		sof_cnt);
	v4l2_ctrl_s_ctrl(ctrl, sof_cnt);

	kfree(seninf_work);
}


void
mtk_cam_seninf_sof_notify(struct mtk_seninf_sof_notify_param *param)
{
	struct v4l2_subdev *sd = param->sd;
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct mtk_seninf_work *seninf_work = NULL;
	struct v4l2_ctrl *ctrl;
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
				V4L2_CID_UPDATE_SOF_CNT);
	if (!ctrl) {
		dev_info(ctx->dev, "%s, no V4L2_CID_UPDATE_SOF_CNT %s\n",
			__func__,
			sensor_sd->name);
		return;
	}

//	dev_info(ctx->dev, "%s sof %s cnt %d\n",
//		__func__,
//		sensor_sd->name,
//		param->sof_cnt);
	v4l2_ctrl_s_ctrl(ctrl, param->sof_cnt);

	if (ctx->streaming) {
		seninf_work = kmalloc(sizeof(struct mtk_seninf_work),
				GFP_ATOMIC);
		if (seninf_work) {
			kthread_init_work(&seninf_work->work,
					mtk_notify_vsync_fn);
			seninf_work->ctx = ctx;
			seninf_work->data.sof = param->sof_cnt;
			kthread_queue_work(&ctx->core->seninf_worker,
					&seninf_work->work);
		}
	}
}

u8 is_reset_by_user(struct seninf_ctx *ctx)
{
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
			V4L2_CID_MTK_SENSOR_RESET_BY_USER);

	return (ctrl) ? v4l2_ctrl_g_ctrl(ctrl) : 0;
}

int reset_sensor(struct seninf_ctx *ctx)
{
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
			V4L2_CID_MTK_SENSOR_RESET);
	if (!ctrl) {
		dev_info(ctx->dev, "V4L2_CID_MTK_SENSOR_RESET %s\n",
			sensor_sd->name);
		return -EINVAL;
	}

	v4l2_ctrl_s_ctrl(ctrl, 1);

	return 0;
}


int notify_fsync_listen_target(struct seninf_ctx *ctx)
{
	int cam_idx = mtk_cam_seninf_get_fsync_vsync_src_cam_info(ctx);
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;

	if (cam_idx < 0 || cam_idx >= 0xff)
		return -EINVAL;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
			V4L2_CID_FSYNC_LISTEN_TARGET);
	if (!ctrl) {
		dev_info(ctx->dev, "no fsync listen target in %s\n",
			sensor_sd->name);
		return -EINVAL;
	}

	dev_info(ctx->dev, "raw cammux usage = %d\n", cam_idx);

	v4l2_ctrl_s_ctrl(ctrl, cam_idx);

	return 0;
}

static void mtk_notify_listen_target_fn(struct kthread_work *work)
{
	struct mtk_seninf_work *seninf_work = NULL;
	struct seninf_ctx *ctx = NULL;

	// --- change to use kthread_delayed_work.
	// seninf_work = container_of(work, struct mtk_seninf_work, work);
	seninf_work = container_of(work, struct mtk_seninf_work, dwork.work);

	if (seninf_work) {
		ctx = seninf_work->ctx;
		if (ctx)
			notify_fsync_listen_target(ctx);

		kfree(seninf_work);
	}
}

void notify_fsync_listen_target_with_kthread(struct seninf_ctx *ctx,
	const unsigned int mdelay)
{
	struct mtk_seninf_work *seninf_work = NULL;

	if (ctx->streaming) {
		seninf_work = kmalloc(sizeof(struct mtk_seninf_work),
					GFP_ATOMIC);
		if (seninf_work) {
			// --- change to use kthread_delayed_work.
			// kthread_init_work(&seninf_work->work,
			//		mtk_notify_listen_target_fn);
			kthread_init_delayed_work(&seninf_work->dwork,
					mtk_notify_listen_target_fn);

			seninf_work->ctx = ctx;

			// --- change to use kthread_delayed_work.
			// kthread_queue_work(&ctx->core->seninf_worker,
			//		&seninf_work->work);
			kthread_queue_delayed_work(&ctx->core->seninf_worker,
					&seninf_work->dwork,
					msecs_to_jiffies(mdelay));
		}
	}
}

#if AOV_GET_PARAM
#ifdef SENSING_MODE_READY
/**
 * @brief: switch i2c bus scl aux function.
 *
 * GPIO 183 for R_CAM3_SCL4, its aux function on apmcu side
 * is 1 (default). So, we need to switch its aux function to 3 for
 * aov use on scp side.
 *
 */
int aov_switch_i2c_bus_scl_aux(struct seninf_ctx *ctx,
	enum mtk_cam_sensor_i2c_bus_scl aux)
{
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
		V4L2_CID_MTK_AOV_SWITCH_I2C_BUS_SCL_AUX);
	if (!ctrl) {
		dev_info(ctx->dev,
			"no(%s) in subdev(%s)\n",
			__func__, sensor_sd->name);
		return -EINVAL;
	}
	dev_info(ctx->dev,
		"[%s] find ctrl (V4L2_CID_MTK_AOV_SWITCH_I2C_BUS_SCL_AUX)\n",
		__func__);
	v4l2_ctrl_s_ctrl(ctrl, (unsigned int)aux);

	return 0;
}

/**
 * @brief: switch i2c bus sda aux function.
 *
 * GPIO 184 for R_CAM3_SDA4, its aux function on apmcu side
 * is 1 (default). So, we need to switch its aux function to 3 for
 * aov use on scp side.
 *
 */
int aov_switch_i2c_bus_sda_aux(struct seninf_ctx *ctx,
	enum mtk_cam_sensor_i2c_bus_sda aux)
{
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
		V4L2_CID_MTK_AOV_SWITCH_I2C_BUS_SDA_AUX);
	if (!ctrl) {
		dev_info(ctx->dev,
			"no(%s) in subdev(%s)\n",
			__func__, sensor_sd->name);
		return -EINVAL;
	}
	dev_info(ctx->dev,
		"[%s] find ctrl (V4L2_CID_MTK_AOV_SWITCH_I2C_BUS_SDA_AUX)\n",
		__func__);
	v4l2_ctrl_s_ctrl(ctrl, (unsigned int)aux);

	return 0;
}
#endif

/**
 * @brief: switch aov pm ops.
 *
 * switch __pm_relax/__pm_stay_awake.
 *
 */
int aov_switch_pm_ops(struct seninf_ctx *ctx,
	enum mtk_cam_sensor_pm_ops pm_ops)
{
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
		V4L2_CID_MTK_AOV_SWITCH_PM_OPS);
	if (!ctrl) {
		dev_info(ctx->dev,
			"no(%s) in subdev(%s)\n",
			__func__, sensor_sd->name);
		return -EINVAL;
	}
	dev_info(ctx->dev,
		"[%s] find ctrl (V4L2_CID_MTK_AOV_SWITCH_PM_OPS)\n",
		__func__);
	v4l2_ctrl_s_ctrl(ctrl, (unsigned int)pm_ops);

	return 0;
}

/**
 * @brief: send apmcu param to scp.
 *
 * As a callee, For sending value/address to caller: scp.
 *
 */
int mtk_cam_seninf_s_aov_param(unsigned int sensor_id,
	struct mtk_seninf_aov_param *aov_seninf_param,
	enum AOV_INIT_TYPE aov_seninf_init_type)
{
	unsigned int real_sensor_id = 0;
	struct seninf_ctx *ctx = NULL;
	struct seninf_vc *vc;
	struct seninf_core *core = NULL;

	pr_info("[%s]+ sensor_id(%d),aov_seninf_init_type(%u)\n",
		__func__, sensor_id, aov_seninf_init_type);

	if (g_aov_param.is_test_model) {
		real_sensor_id = 5;
	} else {
		if (sensor_id == g_aov_param.sensor_idx) {
			real_sensor_id = g_aov_param.sensor_idx;
			pr_info("[%s] input sensor id(%u)(success)\n",
				__func__, real_sensor_id);
		} else {
			real_sensor_id = sensor_id;
			pr_info("input sensor id(%u)(fail)\n", real_sensor_id);
			seninf_aee_print(
				"[AEE] [%s] input sensor id(%u)(fail)",
				__func__, real_sensor_id);
			return -ENODEV;
		}
	}

	if (aov_ctx[real_sensor_id] != NULL) {
		pr_info("[%s] sensor idx(%u)\n", __func__, real_sensor_id);
		ctx = aov_ctx[real_sensor_id];
		core = ctx->core;
#ifdef SENSING_MODE_READY
		switch (aov_seninf_init_type) {
		case INIT_ABNORMAL_SCP_READY:
			dev_info(ctx->dev,
				"[%s] init type is abnormal(%u)!\n",
				__func__, aov_seninf_init_type);
			core->aov_abnormal_init_flag = 1;
			/* seninf/sensor streaming on */
			v4l2_subdev_call(&ctx->subdev, video, s_stream, 1);
			break;
		case INIT_NORMAL:
		default:
			dev_info(ctx->dev,
				"[%s] init type is normal(%u)!\n",
				__func__, aov_seninf_init_type);
			break;
		}
		if (!g_aov_param.is_test_model) {
			/* switch i2c bus scl from apmcu to scp */
			aov_switch_i2c_bus_scl_aux(ctx, SCL7);
			/* switch i2c bus sda from apmcu to scp */
			aov_switch_i2c_bus_sda_aux(ctx, SDA7);
			/* switch aov pm ops: pm_relax */
			aov_switch_pm_ops(ctx, AOV_PM_RELAX);
		}
#endif
		vc = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW0);
	} else {
		pr_info("[%s] Can't find ctx from input sensor id!\n", __func__);
		return -ENODEV;
	}
	if (!vc) {
		pr_info("[%s] vc should not be NULL!\n", __func__);
		return -ENODEV;
	}

	g_aov_param.vc = *vc;
	/* workaround */
	if (!g_aov_param.is_test_model) {
		g_aov_param.vc.dest_cnt = 1;
		g_aov_param.vc.dest[0].mux = 5;
		g_aov_param.vc.dest[0].mux_vr = 33;
		g_aov_param.vc.dest[0].cam = 33;
		g_aov_param.vc.pixel_mode = 3;
		g_aov_param.camtg = 33;
	}

	if (aov_seninf_param != NULL) {
		pr_info("[%s] memcpy aov_seninf_param\n", __func__);
		memcpy((void *)aov_seninf_param, (void *)&g_aov_param,
			sizeof(struct mtk_seninf_aov_param));
		// debug use
		pr_debug(
			"[%s] port(%d)\n", __func__, aov_seninf_param->port);
		pr_debug(
			"[%s] portA(%d)\n", __func__, aov_seninf_param->portA);
		pr_debug(
			"[%s] portB(%d)\n", __func__, aov_seninf_param->portB);
		pr_debug(
			"[%s] is_4d1c(%u)\n", __func__, aov_seninf_param->is_4d1c);
		pr_debug(
			"[%s] seninfIdx(%d)\n", __func__, aov_seninf_param->seninfIdx);
		pr_debug(
			"[%s] vcinfo_cnt(%d)\n", __func__, aov_seninf_param->cnt);
		pr_debug(
			"[%s] seninf_dphy_settle_delay_dt(%d)\n",
			__func__, aov_seninf_param->seninf_dphy_settle_delay_dt);
		pr_debug(
			"[%s] cphy_settle_delay_dt(%d)\n",
			__func__, aov_seninf_param->cphy_settle_delay_dt);
		pr_debug(
			"[%s] dphy_settle_delay_dt(%d)\n",
			__func__, aov_seninf_param->dphy_settle_delay_dt);
		pr_debug(
			"[%s] settle_delay_ck(%d)\n",
			__func__, aov_seninf_param->settle_delay_ck);
		pr_debug(
			"[%s] hs_trail_parameter(%d)\n",
			__func__, aov_seninf_param->hs_trail_parameter);
		pr_debug(
			"[%s] width(%lld)\n", __func__, aov_seninf_param->width);
		pr_debug(
			"[%s] height(%lld)\n", __func__, aov_seninf_param->height);
		pr_debug(
			"[%s] hblank(%lld)\n", __func__, aov_seninf_param->hblank);
		pr_debug(
			"[%s] vblank(%lld)\n", __func__, aov_seninf_param->vblank);
		pr_debug(
			"[%s] fps_n(%d)\n", __func__, aov_seninf_param->fps_n);
		pr_debug(
			"[%s] fps_d(%d)\n", __func__, aov_seninf_param->fps_d);
		pr_debug(
			"[%s] customized_pixel_rate(%lld)\n",
			__func__, aov_seninf_param->customized_pixel_rate);
		pr_debug(
			"[%s] mipi_pixel_rate(%lld)\n",
			__func__, aov_seninf_param->mipi_pixel_rate);
		pr_debug(
			"[%s] is_cphy(%u)\n",
			__func__, aov_seninf_param->is_cphy);
		pr_debug(
			"[%s] num_data_lanes(%d)\n",
			__func__, aov_seninf_param->num_data_lanes);
		pr_debug(
			"[%s] isp_freq(%d)\n",
			__func__, aov_seninf_param->isp_freq);
		pr_debug(
			"[%s] cphy_settle(%u)\n",
			__func__, aov_seninf_param->cphy_settle);
		pr_debug(
			"[%s] dphy_clk_settle(%u)\n",
			__func__, aov_seninf_param->dphy_clk_settle);
		pr_debug(
			"[%s] dphy_data_settle(%u)\n",
			__func__, aov_seninf_param->dphy_data_settle);
		pr_debug(
			"[%s] dphy_trail(%d)\n",
			__func__, aov_seninf_param->dphy_trail);
		pr_debug(
			"[%s] legacy_phy(%d)\n",
			__func__, aov_seninf_param->legacy_phy);
		pr_debug(
			"[%s] not_fixed_trail_settle(%d)\n",
			__func__, aov_seninf_param->not_fixed_trail_settle);
		pr_debug(
			"[%s] dphy_csi2_resync_dmy_cycle(%u)\n",
			__func__, aov_seninf_param->dphy_csi2_resync_dmy_cycle);
		pr_debug(
			"[%s] vc(%d)\n", __func__, aov_seninf_param->vc.vc);
		pr_debug(
			"[%s] dt(%d)\n", __func__, aov_seninf_param->vc.dt);
		pr_debug(
			"[%s] feature(%d)\n", __func__, aov_seninf_param->vc.feature);
		pr_debug(
			"[%s] out_pad(%d)\n", __func__, aov_seninf_param->vc.out_pad);
		pr_debug(
			"[%s] pixel_mode(%d)\n", __func__, aov_seninf_param->vc.pixel_mode);
		pr_debug(
			"[%s] group(%d)\n", __func__, aov_seninf_param->vc.group);
		pr_debug(
			"[%s] mux(%d)\n", __func__, aov_seninf_param->vc.dest[0].mux);
		pr_debug(
			"[%s] mux_vr(%d)\n", __func__, aov_seninf_param->vc.dest[0].mux_vr);
		pr_debug(
			"[%s] cam(%d)\n", __func__, aov_seninf_param->vc.dest[0].cam);
		pr_debug(
			"[%s] tag(%d)\n", __func__, aov_seninf_param->vc.dest[0].tag);
		pr_debug(
			"[%s] cam_type(%d)\n", __func__, aov_seninf_param->vc.dest[0].cam_type);
		pr_debug(
			"[%s] enable(%d)\n", __func__, aov_seninf_param->vc.enable);
		pr_debug(
			"[%s] exp_hsize(%d)\n", __func__, aov_seninf_param->vc.exp_hsize);
		pr_debug(
			"[%s] exp_vsize(%d)\n", __func__, aov_seninf_param->vc.exp_vsize);
		pr_debug(
			"[%s] bit_depth(%d)\n", __func__, aov_seninf_param->vc.bit_depth);
		pr_debug(
			"[%s] dt_remap_to_type(%d)\n",
			__func__, aov_seninf_param->vc.dt_remap_to_type);
	} else {
		pr_info("[%s] Must allocate buffer first!\n", __func__);
		return -ENOMEM;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_cam_seninf_s_aov_param);
#endif

