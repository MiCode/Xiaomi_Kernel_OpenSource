// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/module.h>
#include <linux/delay.h>
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

struct seninf_mux *mtk_cam_seninf_mux_get(struct seninf_ctx *ctx)
{
	struct seninf_core *core = ctx->core;
	struct seninf_mux *ent = NULL;

	mutex_lock(&core->mutex);

	if (!list_empty(&core->list_mux)) {
		ent = list_first_entry(&core->list_mux,
				       struct seninf_mux, list);
		list_move_tail(&ent->list, &ctx->list_mux);
	}

	mutex_unlock(&core->mutex);

	return ent;
}

struct seninf_mux *mtk_cam_seninf_mux_get_pref(struct seninf_ctx *ctx,
					       int *pref_idx, int pref_cnt)
{
	int i;
	struct seninf_core *core = ctx->core;
	struct seninf_mux *ent = NULL;

	mutex_lock(&core->mutex);

	list_for_each_entry(ent, &core->list_mux, list) {
		for (i = 0; i < pref_cnt; i++) {
			if (ent->idx == pref_idx[i]) {
				list_move_tail(&ent->list,
					       &ctx->list_mux);
				mutex_unlock(&core->mutex);
				return ent;
			}
		}
	}

	mutex_unlock(&core->mutex);

	return mtk_cam_seninf_mux_get(ctx);
}

void mtk_cam_seninf_mux_put(struct seninf_ctx *ctx, struct seninf_mux *mux)
{
	struct seninf_core *core = ctx->core;

	mutex_lock(&core->mutex);
	list_move_tail(&mux->list, &core->list_mux);
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

unsigned int mtk_cam_seninf_get_vc_feature(struct v4l2_subdev *sd, unsigned int pad)
{
	struct seninf_vc *pvc = NULL;
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);

	pvc = mtk_cam_seninf_get_vc_by_pad(ctx, pad);
	if (pvc)
		return pvc->feature;

	return VC_NONE;
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
		for (i = 0; i < master->ncontrols; i++) {
			cur_to_new(master->cluster[i]);
		}
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

	if (!ctx->sensor_sd)
		return -EINVAL;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler, V4L2_CID_MTK_CSI_PARAM);
	if (!ctrl) {
		dev_info(ctx->dev, "%s, no V4L2_CID_MTK_CSI_PARAM %s\n",
			__func__, sensor_sd->name);
		return -EINVAL;
	}
	memset(csi_param, 0, sizeof(struct mtk_csi_param));

	ctrl->p_new.p = csi_param;

	ret = get_ctrl(ctrl);
	dev_info(ctx->dev, "%s get_ctrl ret:%d 0x%x|0x%x|0x%x|0x%x\n", __func__,
		ret, csi_param->cphy_settle,
		csi_param->dphy_clk_settle,
		csi_param->dphy_data_settle,
		csi_param->dphy_trail);

	return 0;
}

int mtk_cam_seninf_get_vcinfo(struct seninf_ctx *ctx)
{
	int ret = 0;
	int i, grp, grp_metadata, raw_cnt;
	struct mtk_mbus_frame_desc fd;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;
	int desc;
	struct v4l2_subdev_format raw_fmt;
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;

	if (!ctx->sensor_sd)
		return -EINVAL;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler, V4L2_CID_MTK_FRAME_DESC);
	if (!ctrl) {
		dev_info(ctx->dev, "%s, no V4L2_CID_MTK_FRAME_DESC %s\n",
			__func__, sensor_sd->name);
	}

	ctrl->p_new.p = &fd;

	ret = get_ctrl(ctrl);

	if (ret || fd.type != MTK_MBUS_FRAME_DESC_TYPE_CSI2 || !fd.num_entries) {
		dev_info(ctx->dev, "%s get_ctrl ret:%d num_entries:%d type:%d\n", __func__,
			ret, fd.num_entries, fd.type);
		return get_vcinfo_by_pad_fmt(ctx);
	}

	vcinfo->cnt = 0;
	grp = 0;
	raw_cnt = 0;
	grp_metadata = -1;

	for (i = 0; i < fd.num_entries; i++) {
		vc = &vcinfo->vc[vcinfo->cnt];
		vc->vc = fd.entry[i].bus.csi2.channel;
		vc->dt = fd.entry[i].bus.csi2.data_type;
		desc = fd.entry[i].bus.csi2.user_data_desc;

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
			vc->feature = VC_RAW_DATA;
			vc->out_pad = PAD_SRC_RAW0;
			break;
		case VC_YUV_UV:
			vc->feature = VC_RAW_DATA;
			vc->out_pad = PAD_SRC_RAW1;
			break;
		case VC_GENERAL_EMBEDDED:
			vc->feature = VC_GENERAL_EMBEDDED;
			vc->out_pad = PAD_SRC_GENERAL0;
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
					break;
				case VC_STAGGER_ME:
					vc->out_pad = PAD_SRC_RAW1;
					break;
				case VC_STAGGER_SE:
					vc->out_pad = PAD_SRC_RAW2;
					break;
				case VC_RAW_W_DATA:
					vc->out_pad = PAD_SRC_RAW_W0;
					break;
				case VC_RAW_PROCESSED_DATA:
					vc->out_pad = PAD_SRC_RAW_EXT0;
					break;
				default:
					vc->out_pad = PAD_SRC_RAW0 + raw_cnt;
					break;
				}
				++raw_cnt;
				vc->feature = VC_RAW_DATA;
				vc->group = grp++;
			} else {
				dev_info(ctx->dev, "unknown desc %d, dt 0x%x\n",
					desc, vc->dt);
				continue;
			}
			break;
		}

		if (vc->feature != VC_RAW_DATA) {
			switch (vc->feature) {
			case VC_PDAF_STATS_PIX_1:
			case VC_PDAF_STATS_PIX_2:
			case VC_PDAF_STATS_ME_PIX_1:
			case VC_PDAF_STATS_ME_PIX_2:
			case VC_PDAF_STATS_SE_PIX_1:
			case VC_PDAF_STATS_SE_PIX_2:
				vc->group = grp++;
				break;
			default:
				if (grp_metadata < 0)
					grp_metadata = grp++;
				vc->group = grp_metadata;
				break;
			}
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

		dev_info(ctx->dev, "%s vc[%d] vc 0x%x dt 0x%x pad %d exp %dx%d grp 0x%x code 0x%x\n",
			__func__,
			vcinfo->cnt, vc->vc, vc->dt, vc->out_pad,
			vc->exp_hsize, vc->exp_vsize, vc->group,
			ctx->fmt[vc->out_pad].format.code);
		vcinfo->cnt++;
	}

	return 0;
}
#endif

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

	mutex_lock(&core->mutex);

	/* release all cam muxs */
	list_for_each_entry_safe(ent, tmp, &ctx->list_cam_mux, list) {
		list_move_tail(&ent->list, &core->list_cam_mux);
	}

	mutex_unlock(&core->mutex);
}

void mtk_cam_seninf_alloc_cam_mux(struct seninf_ctx *ctx)
{
	int i;
	struct seninf_core *core = ctx->core;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;
	struct seninf_cam_mux *ent;

	mutex_lock(&core->mutex);

	/* allocate all cam muxs */
	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];
		ent = list_first_entry_or_null(&core->list_cam_mux,
					       struct seninf_cam_mux, list);
		if (ent) {
			list_move_tail(&ent->list, &ctx->list_cam_mux);
			ctx->pad2cam[vc->out_pad] = ent->idx;
			dev_info(ctx->dev, "pad%d -> cam%d\n",
				 vc->out_pad, ent->idx);
		}
	}

	mutex_unlock(&core->mutex);
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
		g_seninf_ops->_update_mux_pixel_mode(ctx, vc->mux, pixelMode);
	}
	// if streaming, update ispclk and update pixle mode seninf mux and reset

	return 0;
}
int _mtk_cam_seninf_set_camtg(struct v4l2_subdev *sd,
					int pad_id, int camtg, bool from_set_camtg)
{
	int vc_en, old_camtg;
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_vc *vc;
	bool disable_last = from_set_camtg;

	if (pad_id < PAD_SRC_RAW0 || pad_id >= PAD_MAXCNT)
		return -EINVAL;

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, pad_id);
	if (!vc)
		return -EINVAL;

	if (!from_set_camtg && !ctx->streaming) {
		dev_info(ctx->dev, "%s !from_set_camtg && !ctx->streaming\n", __func__);
		return -EINVAL;
	}

	ctx->pad2cam[pad_id] = camtg;

	vc_en = mtk_cam_seninf_is_vc_enabled(ctx, vc);

	/* change cam-mux while streaming */
	if (ctx->streaming && vc_en && vc->cam != camtg) {
#ifdef SENSOR_SECURE_MTEE_SUPPORT
		if (ctx->is_secure == 1) {
			vc->cam = camtg;

			dev_info(ctx->dev, "Sensor Secure CA");
			g_seninf_ops->_set_cammux_vc(ctx, vc->cam,
								vc->vc, vc->dt,
								!!vc->dt, !!vc->dt);
			g_seninf_ops->_set_cammux_src(ctx, vc->mux, vc->cam,
								vc->exp_hsize,
								vc->exp_vsize);
			g_seninf_ops->_set_cammux_chk_pixel_mode(ctx,
								vc->cam,
								vc->pixel_mode);
			g_seninf_ops->_cammux(ctx, vc->cam);
			if (pad_id == PAD_SRC_RAW0) {
				// notify vc->cam
				notify_fsync_cammux_usage_with_kthread(ctx);
			}

			if (!seninf_ca_open_session())
				dev_info(ctx->dev, "seninf_ca_open_session fail");

			dev_info(ctx->dev, "Sensor kernel ca_checkpipe");
			seninf_ca_checkpipe(ctx->SecInfo_addr);
		} else {
#endif
			if (camtg == 0xff) {
				old_camtg = vc->cam;
				vc->cam = 0xff;
				g_seninf_ops->_switch_to_cammux_inner_page(ctx, true);
				g_seninf_ops->_set_cammux_next_ctrl(ctx, 0x1f, old_camtg);
				g_seninf_ops->_disable_cammux(ctx, old_camtg);
			} else {
				/* disable old */
				old_camtg = vc->cam;
				/* enable new */
				vc->cam = camtg;
				g_seninf_ops->_switch_to_cammux_inner_page(ctx, true);
				g_seninf_ops->_set_cammux_next_ctrl(ctx, 0x1f, vc->cam);

				g_seninf_ops->_switch_to_cammux_inner_page(ctx, false);

				g_seninf_ops->_set_cammux_vc(ctx, vc->cam,
									vc->vc, vc->dt,
									!!vc->dt, !!vc->dt);
				g_seninf_ops->_set_cammux_src(ctx, vc->mux, vc->cam,
									vc->exp_hsize,
									vc->exp_vsize);
				g_seninf_ops->_set_cammux_chk_pixel_mode(ctx,
									vc->cam,
									vc->pixel_mode);
				if (old_camtg != 0xff && disable_last) {
					//disable old in next sof
					g_seninf_ops->_disable_cammux(ctx, old_camtg);
				}
				g_seninf_ops->_cammux(ctx, vc->cam); //enable in next sof
				g_seninf_ops->_switch_to_cammux_inner_page(ctx, true);
				g_seninf_ops->_set_cammux_next_ctrl(ctx, vc->mux, vc->cam);
				if (old_camtg != 0xff && disable_last)
					g_seninf_ops->_set_cammux_next_ctrl(ctx,
									vc->mux, old_camtg);

				if (pad_id == PAD_SRC_RAW0) {
					// notify vc->cam
					notify_fsync_cammux_usage_with_kthread(ctx);
				}
			}
			dev_info(ctx->dev, "%s: pad %d mux %d cam %d -> %d\n",
				 __func__, vc->out_pad, vc->mux, old_camtg, vc->cam);

#ifdef SENSOR_SECURE_MTEE_SUPPORT
		}
#endif
	} else {
		dev_info(ctx->dev, "%s: pad_id %d, camtg %d, ctx->streaming %d, vc_en %d, vc->cam %d\n",
			 __func__, pad_id, camtg, ctx->streaming, vc_en, vc->cam);
	}

	return 0;
}


int mtk_cam_seninf_set_camtg(struct v4l2_subdev *sd, int pad_id, int camtg)
{
	return _mtk_cam_seninf_set_camtg(sd, pad_id, camtg, true);
}

static int mtk_cam_seninf_get_raw_cam_info(struct seninf_ctx *ctx)
{
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;
	int i;

	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];
		if (vc->out_pad == PAD_SRC_RAW0) /* first raw */
			return vc->cam;
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
	struct seninf_ctx *ctx;
	int index = 0;


	if (param != NULL && param->num == 1) {
		sd = param->settings[0].seninf;
		pad_id = param->settings[0].source;
		camtg = param->settings[0].camtg;
		ctx = container_of(sd, struct seninf_ctx, subdev);
		//_mtk_cam_seninf_set_camtg(sd, pad_id, camtg, true);
		dev_info(ctx->dev,
			"%s error, should use mtk_cam_seninf_set_camtg directly!!!\n"
			, __func__);

	} else if (param != NULL && param->num > 1) {
		sd = param->settings[0].seninf;
		pad_id = param->settings[0].source;
		camtg = param->settings[0].camtg;

		ctx = container_of(sd, struct seninf_ctx, subdev);

		//mtk_cam_seninf_enable_global_drop_irq(ctx, true, 0);
		g_seninf_ops->_reset_cam_mux_dyn_en(ctx, index);
		//k_cam_seninf_set_sw_cfg_busy(ctx, true, index);

		_mtk_cam_seninf_set_camtg(sd, pad_id, camtg, false);
		//g_seninf_ops->_enable_cam_mux_vsync_irq(ctx, true, camtg);
		//k_cam_seninf_set_cam_mux_dyn_en(ctx, true, camtg, index);



		sd = param->settings[1].seninf;
		pad_id = param->settings[1].source;
		camtg = param->settings[1].camtg;
		_mtk_cam_seninf_set_camtg(sd, pad_id, camtg, false);
		//g_seninf_ops->_enable_cam_mux_vsync_irq(ctx, true, camtg);
		//mtk_cam_seninf_set_cam_mux_dyn_en(ctx, true, camtg, index);



		if (param->num > 2) {
			sd = param->settings[2].seninf;
			pad_id = param->settings[2].source;
			camtg = param->settings[2].camtg;

			_mtk_cam_seninf_set_camtg(sd, pad_id, camtg, false);

			//g_seninf_ops->_enable_cam_mux_vsync_irq(ctx, true, camtg);
			//k_cam_seninf_set_cam_mux_dyn_en(ctx, true, camtg, index);

			dev_info(ctx->dev,
				"%s: param->num %d, pad_id[0] %d, ctx->camtg[0] %d, pad_id[1] %d, ctx->camtg[1] %d pad_id[2] %d, ctx->camtg[2] %d %llu|%llu\n",
				__func__, param->num,
				param->settings[0].source, param->settings[0].camtg,
				param->settings[1].source, param->settings[1].camtg,
				param->settings[2].source, param->settings[2].camtg,
				ktime_get_boottime_ns(),
				ktime_get_ns());
		} else
			dev_info(ctx->dev,
				"%s: param->num %d, pad_id[0] %d, ctx->camtg[0] %d, pad_id[1] %d, ctx->camtg[1] %d %llu|%llu\n",
				__func__, param->num, pad_id, camtg,
				param->settings[1].source, param->settings[1].camtg,
				ktime_get_boottime_ns(),
				ktime_get_ns());

		//mtk_cam_seninf_set_sw_cfg_busy(ctx, false, index);
		//mtk_cam_seninf_enable_global_drop_irq(ctx, false, 0);
		//g_seninf_ops->_disable_all_cam_mux_vsync_irq(ctx);

	}
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
		dev_err(ctx->dev, "%s, no V4L2_CID_VSYNC_NOTIFY %s\n",
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


int notify_fsync_cammux_usage(struct seninf_ctx *ctx)
{
	int cam_idx = mtk_cam_seninf_get_raw_cam_info(ctx);
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;

	if (cam_idx < 0)
		return -EINVAL;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
			V4L2_CID_FSYNC_MAP_ID);
	if (!ctrl) {
		dev_info(ctx->dev, "no fsync map id in %s\n",
			sensor_sd->name);
		return -EINVAL;
	}

	dev_info(ctx->dev, "raw cammux usage = %d\n", cam_idx);

	v4l2_ctrl_s_ctrl(ctrl, cam_idx);

	return 0;
}

static void mtk_notify_cammux_usage_fn(struct kthread_work *work)
{
	struct mtk_seninf_work *seninf_work = NULL;
	struct seninf_ctx *ctx = NULL;

	seninf_work = container_of(work, struct mtk_seninf_work, work);

	if (seninf_work) {
		ctx = seninf_work->ctx;
		if (ctx)
			notify_fsync_cammux_usage(ctx);

		kfree(seninf_work);
	}
}

void notify_fsync_cammux_usage_with_kthread(struct seninf_ctx *ctx)
{
	struct mtk_seninf_work *seninf_work = NULL;

	if (ctx->streaming) {
		seninf_work = kmalloc(sizeof(struct mtk_seninf_work),
					GFP_ATOMIC);
		if (seninf_work) {
			kthread_init_work(&seninf_work->work,
					mtk_notify_cammux_usage_fn);
			seninf_work->ctx = ctx;
			kthread_queue_work(&ctx->core->seninf_worker,
					&seninf_work->work);
		}
	}
}

