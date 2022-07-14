// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include "mtk_cam-feature.h"

/**
 * TODO: phase out mtk_cam_is_[feature] since it can't be
 * used when feature per-frame chages except mtk_cam_is_stagger(),
 * which is using the raw_feature which is not changed
 * during streaming.
 */

bool mtk_cam_is_ext_isp(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	if (ctx->pipe)
		return mtk_cam_feature_is_ext_isp(ctx->pipe->feature_active);
	else
		return false;
}

bool mtk_cam_is_ext_isp_yuv(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	if (ctx->pipe)
		return mtk_cam_feature_is_ext_isp_yuv(ctx->pipe->feature_active);
	else
		return false;
}

bool mtk_cam_is_time_shared(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	if (ctx->pipe)
		return mtk_cam_feature_is_time_shared(ctx->pipe->feature_pending);
	else
		return false;
}

bool mtk_cam_is_hsf(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	if (ctx->pipe && ctx->pipe->res_config.enable_hsf_raw)
		return true;
	else
		return false;
}

bool mtk_cam_is_pure_m2m(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	if ((ctx->pipe) &&
		(ctx->pipe->feature_pending & MTK_CAM_FEATURE_PURE_OFFLINE_M2M_MASK))
		return true;
	else
		return false;
}

bool mtk_cam_is_m2m(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;
	if (!ctx->pipe) {
		dev_dbg(ctx->cam->dev, "[%s] ctx->pipe is null\n",
			__func__);
		return false;
	}

	return mtk_cam_feature_is_m2m(ctx->pipe->feature_pending);
}

bool mtk_cam_is_stagger_m2m(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	if (ctx->pipe)
		return mtk_cam_feature_is_stagger_m2m(ctx->pipe->feature_pending);
	else
		return false;
}

bool mtk_cam_is_stagger(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	if (ctx->pipe)
		return mtk_cam_feature_is_stagger(ctx->pipe->feature_active);
	else
		return false;
}

bool mtk_cam_is_mstream_m2m(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	if (ctx->pipe)
		return mtk_cam_feature_is_mstream_m2m(ctx->pipe->feature_pending);
	else
		return false;
}

bool mtk_cam_is_mstream(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num) {
		pr_debug("not mstream because of used_raw_num=0 !!!\n");
		return false;
	}

	if (ctx->pipe)
		return mtk_cam_feature_is_mstream(ctx->pipe->feature_pending);
	else
		return false;
}

bool mtk_cam_is_subsample(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	if (ctx->pipe)
		return mtk_cam_feature_is_subsample(ctx->pipe->feature_pending);
	else
		return false;
}

bool mtk_cam_is_2_exposure(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	if (ctx->pipe)
		return mtk_cam_feature_is_2_exposure(ctx->pipe->feature_pending);
	else
		return false;
}

bool mtk_cam_is_3_exposure(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	if (ctx->pipe)
		return mtk_cam_feature_is_3_exposure(ctx->pipe->feature_pending);
	else
		return false;
}

bool mtk_cam_is_with_w_channel(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	if (ctx->pipe)
		return (ctx->pipe->feature_pending & WITH_W_CHANNEL) != 0;
	else
		return false;
}

int mtk_cam_get_sensor_exposure_num(u32 raw_feature)
{
	int result = 1;
	raw_feature &= MTK_CAM_FEATURE_HDR_MASK;

	switch (raw_feature) {
	case STAGGER_3_EXPOSURE_LE_NE_SE:
	case STAGGER_3_EXPOSURE_SE_NE_LE:
		result = 3;
		break;
	case STAGGER_2_EXPOSURE_LE_SE:
	case STAGGER_2_EXPOSURE_SE_LE:
		result = 2;
		break;
	default:
		result = 1;
		break;
	}
	return result;
}

int mtk_cam_get_feature_switch(struct mtk_raw_pipeline *raw_pipe,
			       int prev)
{
	int cur = raw_pipe->feature_pending;
	int res = EXPOSURE_CHANGE_NONE;

	if (cur == prev)
		return EXPOSURE_CHANGE_NONE;
	if (cur & MTK_CAM_FEATURE_HDR_MASK || prev & MTK_CAM_FEATURE_HDR_MASK) {
		if (mtk_cam_feature_is_mstream(cur) ||
				mtk_cam_feature_is_mstream(prev) ||
				mtk_cam_feature_is_mstream_m2m(cur) ||
				mtk_cam_feature_is_mstream_m2m(prev)) {
			/* mask out m2m before comparison */
			cur &= MTK_CAM_FEATURE_HDR_MASK;
			prev &= MTK_CAM_FEATURE_HDR_MASK;

			if (prev == 0  && mtk_cam_feature_is_mstream(cur))
				res = EXPOSURE_CHANGE_1_to_2 |
						MSTREAM_EXPOSURE_CHANGE;
			else if (cur == 0 && mtk_cam_feature_is_mstream(prev))
				res = EXPOSURE_CHANGE_2_to_1 |
						MSTREAM_EXPOSURE_CHANGE;
		} else {
			cur &= MTK_CAM_FEATURE_HDR_MASK;
			prev &= MTK_CAM_FEATURE_HDR_MASK;
			if ((cur == STAGGER_2_EXPOSURE_LE_SE || cur == STAGGER_2_EXPOSURE_SE_LE) &&
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
	}
	dev_dbg(raw_pipe->subdev.dev, "[%s] res:%d cur:0x%x prev:0x%x\n",
			__func__, res, cur, prev);

	return res;
}

bool mtk_cam_hw_is_otf(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	if (ctx->pipe)
		return mtk_cam_hw_mode_is_otf(ctx->pipe->hw_mode_pending);
	else
		return false;
}

bool mtk_cam_hw_is_dc(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	if (ctx->pipe)
		return mtk_cam_hw_mode_is_dc(ctx->pipe->hw_mode_pending);
	else
		return false;
}

bool mtk_cam_hw_is_offline(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	if (ctx->pipe)
		return mtk_cam_hw_mode_is_offline(ctx->pipe->hw_mode_pending);
	else
		return false;
}

bool mtk_cam_hw_is_m2m(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	if (ctx->pipe)
		return mtk_cam_hw_mode_is_m2m(ctx->pipe->hw_mode_pending);
	else
		return false;
}

bool mtk_cam_is_srt(int hw_mode)
{
	return hw_mode == HW_MODE_DIRECT_COUPLED;
}

