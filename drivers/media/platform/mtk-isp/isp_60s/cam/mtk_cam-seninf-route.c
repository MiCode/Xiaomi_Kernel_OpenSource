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

void mtk_cam_seninf_init_res(struct seninf_core *core)
{
	int i;

	INIT_LIST_HEAD(&core->list_mux);
	for (i = 0; i < ARRAY_SIZE(core->mux); i++) {
		core->mux[i].idx = i;
		list_add_tail(&core->mux[i].list, &core->list_mux);
	}

#ifdef SENINF_DEBUG
	INIT_LIST_HEAD(&core->list_cam_mux);
	for (i = 0; i < ARRAY_SIZE(core->cam_mux); i++) {
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

	vc = &vcinfo->vc[vcinfo->cnt++];
	vc->vc = 0;
	vc->dt = 0x2b;
	vc->feature = VC_RAW_DATA;
	vc->out_pad = PAD_SRC_RAW0;
	vc->group = 0;
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

static int get_vcinfo_by_pad_fmt(struct seninf_ctx *ctx)
{
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;

	vcinfo->cnt = 0;

	switch (ctx->fmt[PAD_SINK].format.code) {
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
	default:
		return -1;
	}

	return 0;
}

#ifdef SENINF_VC_ROUTING
int mtk_cam_seninf_get_vcinfo(struct seninf_ctx *ctx)
{
	int ret, i, grp, grp_metadata, raw_cnt;
	struct v4l2_mbus_frame_desc fd;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;

	if (!ctx->sensor_sd)
		return -EINVAL;

	ret = v4l2_subdev_call(ctx->sensor_sd, pad, get_frame_desc,
			       ctx->sensor_pad_idx, &fd);
	if (ret || fd.type != V4L2_MBUS_FRAME_DESC_TYPE_CSI2)
		return get_vcinfo_by_pad_fmt(ctx);

	vcinfo->cnt = 0;
	grp = 0;
	raw_cnt = 0;
	grp_metadata = -1;

	for (i = 0; i < fd.num_entries; i++) {
		vc = &vcinfo->vc[vcinfo->cnt];
		vc->vc = fd.entry[i].bus.csi2.channel;
		vc->dt = fd.entry[i].bus.csi2.data_type;
		if (vc->dt == 0x2b) {
			if (raw_cnt >= 3) {
				dev_warn(ctx->dev, "too much raw data\n");
				continue;
			}
			vc->feature = VC_RAW_DATA;
			vc->out_pad = PAD_SRC_RAW0 + raw_cnt++;
			vc->group = grp++;
		} else if (V4L2_MBUS_CSI2_IS_USER_DEFINED_DATA(vc->dt)) {
			int desc = fd.entry[i].bus.csi2.user_data_desc;

			switch (desc) {
			case V4L2_MBUS_CSI2_USER_DEFINED_DATA_DESC_Y_HIST:
				vc->feature = VC_3HDR_Y;
				vc->out_pad = PAD_SRC_HDR0;
				break;
			case V4L2_MBUS_CSI2_USER_DEFINED_DATA_DESC_AE_HIST:
				vc->feature = VC_3HDR_AE;
				vc->out_pad = PAD_SRC_HDR1;
				break;
			case V4L2_MBUS_CSI2_USER_DEFINED_DATA_DESC_FLICKER:
				vc->feature = VC_3HDR_FLICKER;
				vc->out_pad = PAD_SRC_HDR2;
				break;
			case V4L2_MBUS_CSI2_USER_DEFINED_DATA_DESC_PDAF_PIXEL:
				vc->out_pad = PAD_SRC_PDAF0;
				break;
			default:
				dev_warn(ctx->dev, "unknown desc %d\n", desc);
				continue;
			}

			if (grp_metadata < 0)
				grp_metadata = grp++;

			vc->group = grp_metadata;
		} else {
			dev_warn(ctx->dev, "unknown data_type %d\n", vc->dt);
			continue;
		}

		vc->exp_hsize = fd.entry[i].bus.csi2.hsize;
		vc->exp_vsize = fd.entry[i].bus.csi2.vsize;
		vcinfo->cnt++;

		/* update pad fotmat */
		if (vc->exp_hsize && vc->exp_vsize) {
			ctx->fmt[vc->out_pad].format.width = vc->exp_hsize;
			ctx->fmt[vc->out_pad].format.height = vc->exp_vsize;
		}

		dev_info(ctx->dev, "vc[%d] vc 0x%x dt 0x%x pad %d exp %dx%d\n",
			 vcinfo->cnt, vc->vc, vc->dt, vc->out_pad,
			vc->exp_hsize, vc->exp_vsize);
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
#ifdef SENINF_DEBUG
	if (ctx->is_test_streamon)
		return 1;
#endif

	if (media_entity_remote_pad(&ctx->pads[vc->out_pad]))
		return 1;

	return 0;
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
		pr_err("%s: invalid pad=%d\n", __func__, pad_id);
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
		pr_err("%s: invalid pad=%d\n", __func__, pad_id);
		return -1;
	}

	vc->pixel_mode = pixelMode;

	return 0;
}

int mtk_cam_seninf_set_camtg(struct v4l2_subdev *sd, int pad_id, int camtg)
{
	int vc_en, old_camtg;
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_vc *vc;

	if (pad_id < PAD_SRC_RAW0 || pad_id >= PAD_MAXCNT)
		return -EINVAL;

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, pad_id);
	if (!vc)
		return -EINVAL;

	ctx->pad2cam[pad_id] = camtg;

	vc_en = mtk_cam_seninf_is_vc_enabled(ctx, vc);

	/* change cam-mux while streaming */
	if (ctx->streaming && vc_en && vc->cam != camtg) {
		/* disable old */
		old_camtg = vc->cam;
		mtk_cam_seninf_disable_cammux(ctx, vc->cam);

		/* enable new */
		vc->cam = camtg;
		mtk_cam_seninf_set_cammux_vc(ctx, vc->cam,
					     vc->vc, vc->dt,
					     !!vc->dt, !!vc->dt);
		mtk_cam_seninf_set_cammux_src(ctx, vc->mux, vc->cam,
					      vc->exp_hsize,
					      vc->exp_vsize);
		mtk_cam_seninf_set_cammux_chk_pixel_mode(ctx,
							 vc->cam,
							 vc->pixel_mode);
		mtk_cam_seninf_cammux(ctx, vc->cam);

		dev_info(ctx->dev, "%s: pad %d mux %d cam %d -> %d\n",
			 __func__, vc->out_pad, vc->mux, old_camtg, vc->cam);
	}

	return 0;
}
