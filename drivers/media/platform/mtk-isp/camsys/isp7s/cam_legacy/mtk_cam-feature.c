// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include "mtk_cam-feature.h"

static int debug_cam_feature;
module_param(debug_cam_feature, int, 0644);

#undef dev_dbg
#define dev_dbg(dev, fmt, arg...)		\
	do {					\
		if (debug_cam_feature >= 1)	\
			dev_info(dev, fmt,	\
				## arg);	\
	} while (0)


bool mtk_cam_is_hsf(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	if (ctx->pipe->res_config.enable_hsf_raw)
		return true;
	else
		return false;
}

int mtk_cam_get_feature_switch(struct mtk_raw_pipeline *raw_pipe,
			       struct mtk_cam_scen *prev)
{
	struct mtk_cam_scen *cur = &raw_pipe->user_res.raw_res.scen;
	int res = EXPOSURE_CHANGE_NONE;
	int exp = 1, exp_prev = 1;

	dev_dbg(raw_pipe->subdev.dev, "%s scen: cur(%s) prev(%s)\n",
			__func__, cur->dbg_str, prev->dbg_str);

	exp = mtk_cam_scen_get_exp_num(cur);
	exp_prev = mtk_cam_scen_get_exp_num(prev);

	if ((mtk_cam_scen_is_subsample_1st_frame_only(cur) &&
			!mtk_cam_scen_is_subsample_1st_frame_only(prev)) ||
			(mtk_cam_scen_is_subsample_1st_frame_only(prev) &&
			!mtk_cam_scen_is_subsample_1st_frame_only(cur))) {
		dev_dbg(raw_pipe->subdev.dev, "subspl mode change\n");
		return SUBSPL_MODE_CHANGE;
	}

	if (exp == exp_prev)
		return EXPOSURE_CHANGE_NONE;

	if ((mtk_cam_scen_is_mstream(cur) && mtk_cam_scen_is_mstream(prev)) ||
	    (mtk_cam_scen_is_mstream_m2m(cur) && mtk_cam_scen_is_mstream_m2m(prev))) {
		if (exp_prev == 1)
			res = EXPOSURE_CHANGE_1_to_2 |
						MSTREAM_EXPOSURE_CHANGE;
		else
			res = EXPOSURE_CHANGE_2_to_1 |
						MSTREAM_EXPOSURE_CHANGE;
	}

	if ((mtk_cam_scen_is_sensor_stagger(cur) && mtk_cam_scen_is_sensor_stagger(prev)) ||
	    (mtk_cam_scen_is_stagger_m2m(cur) && mtk_cam_scen_is_stagger_m2m(prev)) ||
	    (mtk_cam_scen_is_stagger_pure_m2m(cur) && mtk_cam_scen_is_stagger_pure_m2m(prev))) {
		if (exp_prev == 3) {
			if (exp == 1)
				res = EXPOSURE_CHANGE_3_to_1;
			else if (exp == 2)
				res = EXPOSURE_CHANGE_3_to_2;
		} else if (exp_prev == 2) {
			if (exp == 1)
				res = EXPOSURE_CHANGE_2_to_1;
			else if (exp == 3)
				res = EXPOSURE_CHANGE_2_to_3;
		} else if (exp_prev == 1)  {
			if (exp == 2)
				res = EXPOSURE_CHANGE_1_to_2;
			else if (exp == 3)
				res = EXPOSURE_CHANGE_1_to_3;
		}
	}

	dev_dbg(raw_pipe->subdev.dev, "[%s] scen:%d res:%d cur_exp:%d prev_exp:%d\n",
			__func__, cur->id, res, exp, exp_prev);

	return res;
}

bool mtk_cam_hw_is_otf(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	return mtk_cam_hw_mode_is_otf(ctx->pipe->hw_mode_pending);
}

bool mtk_cam_hw_is_dc(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	return mtk_cam_hw_mode_is_dc(ctx->pipe->hw_mode_pending);
}

bool mtk_cam_hw_is_offline(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	return mtk_cam_hw_mode_is_offline(ctx->pipe->hw_mode_pending);
}

bool mtk_cam_hw_is_m2m(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	return mtk_cam_hw_mode_is_m2m(ctx->pipe->hw_mode_pending);
}

bool mtk_cam_is_srt(int hw_mode)
{
	return hw_mode == HW_MODE_DIRECT_COUPLED;
}

