// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2020 MediaTek Inc.

#include <linux/pm_runtime.h>

#include "kd_imgsensor_define_v4l2.h"
#include "adaptor.h"
#include "adaptor-i2c.h"
#include "adaptor-ctrls.h"

#define ctrl_to_ctx(ctrl) \
	container_of(ctrl->handler, struct adaptor_ctx, ctrls)

#define sizeof_u32(__struct_name__) (sizeof(__struct_name__) / sizeof(u32))
#define sizeof_u16(__struct_name__) (sizeof(__struct_name__) / sizeof(u16))

#define USER_DESC_TO_IMGSENSOR_ENUM(DESC) \
				(DESC - VC_STAGGER_NE + \
				IMGSENSOR_STAGGER_EXPOSURE_LE)
#define IS_HDR_STAGGER(DESC) \
				((DESC >= VC_STAGGER_NE) && \
				 (DESC <= VC_STAGGER_SE))

#ifdef V4L2_CID_PD_PIXEL_REGION
static int g_pd_pixel_region(struct adaptor_ctx *ctx, struct v4l2_ctrl *ctrl)
{
	struct SET_PD_BLOCK_INFO_T pd;
	struct v4l2_ctrl_image_pd_pixel_region region;
	union feature_para para;
	u32 i, len = 0;

	para.u64[0] = ctx->cur_mode->id;
	para.u64[1] = (u64)&pd;

	memset(&pd, 0, sizeof(pd));

	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_PDAF_INFO,
		para.u8, &len);

	if (!pd.i4BlockNumX || !pd.i4BlockNumY)
		return -EINVAL;

	region.offset_x = pd.i4OffsetX;
	region.offset_y = pd.i4OffsetY;
	region.pitch_x = pd.i4PitchX;
	region.pitch_y = pd.i4PitchY;
	region.pair_num = pd.i4PairNum;
	region.subblk_w = pd.i4SubBlkW;
	region.subblk_h = pd.i4SubBlkH;
	for (i = 0; i < ARRAY_SIZE(region.posL); i++) {
		region.posL[i][0] = pd.i4PosL[i][0];
		region.posL[i][1] = pd.i4PosL[i][1];
		region.posR[i][0] = pd.i4PosR[i][0];
		region.posR[i][1] = pd.i4PosR[i][1];
	}
	region.blk_num_x = pd.i4BlockNumX;
	region.blk_num_y = pd.i4BlockNumY;
	region.mirror_flip = pd.iMirrorFlip;
	region.crop_x = pd.i4Crop[ctx->cur_mode->id][0];
	region.crop_y = pd.i4Crop[ctx->cur_mode->id][1];

	memcpy(ctrl->p_new.p_image_pd_pixel_region, &region, sizeof(region));

	return 0;
}
#endif

/* callback function for frame-sync set framelength using */
/*     return: 0 => No-Error ; non-0 => Error */
int cb_fsync_mgr_set_framelength(void *p_ctx,
				unsigned int cmd_id,
				unsigned int framelength)
{
	struct adaptor_ctx *ctx = NULL;
	enum ACDK_SENSOR_FEATURE_ENUM cmd = 0;
	union feature_para para;
	int ret = 0;
	u32 len;

	if (p_ctx == NULL) {
		ret = 1;
		dev_info(ctx->dev, "p_ctx is a NULL pointer\n");
		return ret;
	}

	ctx = (struct adaptor_ctx *)p_ctx;
	cmd = (enum ACDK_SENSOR_FEATURE_ENUM)cmd_id;

	ctx->subctx.frame_length = framelength;

	switch (cmd) {
	/* for set_shutter_frame_length() */
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		para.u64[0] = ctx->subctx.shutter;
		para.u64[1] = ctx->subctx.frame_length;
		para.u64[2] = 1; // auto_extend ON
		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME,
			para.u8, &len);
		break;

	default:
		ret = 2;
		dev_info(ctx->dev, "unknown CMD type, do nothing\n");
		break;
	}

	return ret;
}

/* notify frame-sync set sensor for doing frame sync */
static void notify_fsync_mgr_set_sync(struct adaptor_ctx *ctx, u64 en)
{
	/* call frame-sync fs_set_sync() */
	if (ctx->fsync_mgr != NULL) {
		ctx->fsync_mgr->fs_update_tg(ctx->idx,
					ctx->fsync_map_id->val + 1);
		ctx->fsync_mgr->fs_set_sync(ctx->idx, en);
	} else
		dev_info(ctx->dev, "frame-sync is not init!\n");
}

/* notify frame-sync update anti-flicker-en status */
static void notify_fsync_mgr_update_auto_flicker_mode(struct adaptor_ctx *ctx,
							u64 en)
{
	/* call frame-sync fs_set_sync() */
	if (ctx->fsync_mgr != NULL)
		ctx->fsync_mgr->fs_update_auto_flicker_mode(ctx->idx, en);
	else
		dev_info(ctx->dev, "frame-sync is not init!\n");
}

/* notify frame-sync update min_fl_lc value */
static void notify_fsync_mgr_update_min_fl(struct adaptor_ctx *ctx)
{
	/* call frame-sync fs_set_sync() */
	if (ctx->fsync_mgr != NULL) {
		ctx->fsync_mgr->fs_update_min_framelength_lc(ctx->idx,
					ctx->subctx.min_frame_length);
	} else
		dev_info(ctx->dev, "frame-sync is not init!\n");
}

/* notify frame-sync set_shutter(), bind all SENSOR_FEATURE_SET_ESHUTTER CMD */
static void notify_fsync_mgr_set_shutter(struct adaptor_ctx *ctx,
					enum ACDK_SENSOR_FEATURE_ENUM cmd,
					u64 shutter)
{
	struct fs_perframe_st pf_ctrl = {0};

	pf_ctrl.sensor_id = ctx->subdrv->id;
	pf_ctrl.sensor_idx = ctx->idx;

	pf_ctrl.min_fl_lc = ctx->subctx.min_frame_length;
	pf_ctrl.shutter_lc = shutter;
	pf_ctrl.margin_lc = ctx->subctx.margin;
	pf_ctrl.flicker_en = ctx->subctx.autoflicker_en;
	pf_ctrl.out_fl_lc = ctx->subctx.frame_length; // sensor current fl_lc

	pf_ctrl.pclk = ctx->cur_mode->pclk;
	pf_ctrl.linelength = ctx->cur_mode->llp;
	pf_ctrl.lineTimeInNs = ctx->cur_mode->linetime_in_ns;

	pf_ctrl.cmd_id = (unsigned int)cmd;

	/* call frame-sync fs_set_shutter() */
	if (ctx->fsync_mgr != NULL)
		ctx->fsync_mgr->fs_set_shutter(&pf_ctrl);
	else
		dev_info(ctx->dev, "frame-sync is not init!\n");
}

static int set_hdr_exposure_tri(struct adaptor_ctx *ctx, struct mtk_hdr_exposure *info)
{
	union feature_para para;
	u32 len = 0;

	para.u64[0] = info->le_exposure;
	para.u64[1] = info->me_exposure;
	para.u64[2] = info->se_exposure;
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_SET_HDR_TRI_SHUTTER,
		para.u8, &len);

	return 0;
}

static int set_hdr_gain_tri(struct adaptor_ctx *ctx, struct mtk_hdr_gain *info)
{
	union feature_para para;
	u32 len = 0;

	para.u64[0] = info->le_gain;
	para.u64[1] = info->me_gain;
	para.u64[2] = info->se_gain;
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_SET_HDR_TRI_GAIN,
		para.u8, &len);

	return 0;
}

static int set_hdr_exposure_dual(struct adaptor_ctx *ctx, struct mtk_hdr_exposure *info)
{
	union feature_para para;
	u32 len = 0;

	para.u64[0] = info->le_exposure;
	para.u64[1] = info->me_exposure; // temporailly workaround, 2 exp should be NE/SE
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_SET_HDR_SHUTTER,
		para.u8, &len);

	return 0;
}

static int set_hdr_gain_dual(struct adaptor_ctx *ctx, struct mtk_hdr_gain *info)
{
	union feature_para para;
	u32 len = 0;

	para.u64[0] = info->le_gain;
	para.u64[1] = info->me_gain; // temporailly workaround, 2 exp should be NE/SE
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_SET_DUAL_GAIN,
		para.u8, &len);

	return 0;
}

static int g_stagger_info(struct adaptor_ctx *ctx,
						  int origin_scenario,
						  struct mtk_stagger_info *info)
{
	int ret = 0;
	struct mtk_mbus_frame_desc fd;
	int hdr_cnt = 0;
	unsigned int i = 0;

	if (!info)
		return 0;

	ret = subdrv_call(ctx, get_frame_desc, origin_scenario, &fd);

	if (!ret) {
		for (i = 0; i < fd.num_entries; ++i) {
			u16 udd =
				fd.entry[i].bus.csi2.user_data_desc;

			if (IS_HDR_STAGGER(udd)) {
				hdr_cnt++;
				info->order[i] = USER_DESC_TO_IMGSENSOR_ENUM(udd);
			}
		}
	}

	info->count = hdr_cnt;

	return ret;
}

static int g_stagger_scenario(struct adaptor_ctx *ctx,
							  int origin_scenario,
							  struct mtk_stagger_target_scenario *info)
{
	int ret = 0;
	union feature_para para;
	u32 len;

	if (!ctx || !info)
		return 0;

	para.u64[0] = origin_scenario;
	para.u64[1] = (u64)info->exposure_num;
	para.u64[2] = SENSOR_SCENARIO_ID_NONE;

	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_STAGGER_TARGET_SCENARIO,
		para.u8, &len);

	info->target_scenario_id = (u32)para.u64[2];

	return ret;
}

static int do_set_ae_ctrl(struct adaptor_ctx *ctx,
						  struct mtk_hdr_ae *ae_ctrl)
{
	union feature_para para;
	int ret = 0;
	u32 len = 0;
	struct mtk_stagger_info info;

	ret = g_stagger_info(ctx, ctx->cur_mode->id, &info);

	dev_info(ctx->dev, "exposure[LLLE->SSSE] %d %d %d %d %d\n",
			 ae_ctrl->exposure.le_exposure,
			 ae_ctrl->exposure.me_exposure,
			 ae_ctrl->exposure.se_exposure,
			 ae_ctrl->exposure.sse_exposure,
			 ae_ctrl->exposure.ssse_exposure);

	dev_info(ctx->dev, "ana_gain[LLLE->SSSE] %d %d %d %d %d\n",
			 ae_ctrl->gain.le_gain,
			 ae_ctrl->gain.me_gain,
			 ae_ctrl->gain.se_gain,
			 ae_ctrl->gain.sse_gain,
			 ae_ctrl->gain.ssse_gain);
	ae_ctrl->gain.le_gain /= 16;
	ae_ctrl->gain.me_gain /= 16;
	ae_ctrl->gain.se_gain /= 16;
	ae_ctrl->gain.sse_gain /= 16;
	ae_ctrl->gain.ssse_gain /= 16;


	switch (info.count) {
	case 3:
	{
		set_hdr_exposure_tri(ctx, &ae_ctrl->exposure);
		set_hdr_gain_tri(ctx, &ae_ctrl->gain);
	}
		break;
	case 2:
	{
		set_hdr_exposure_dual(ctx, &ae_ctrl->exposure);
		set_hdr_gain_dual(ctx, &ae_ctrl->gain);
	}
		break;
	case 1:
	default:
	{
		para.u64[0] = ae_ctrl->exposure.le_exposure;
		subdrv_call(ctx, feature_control,
					SENSOR_FEATURE_SET_ESHUTTER,
					para.u8, &len);

		notify_fsync_mgr_set_shutter(ctx,
			SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME, para.u64[0]);

		para.u64[0] = ae_ctrl->gain.le_gain;
		subdrv_call(ctx, feature_control,
					SENSOR_FEATURE_SET_GAIN,
					para.u8, &len);
	}
		break;
	}

	if (ae_ctrl->actions & IMGSENSOR_EXTEND_FRAME_LENGTH_TO_DOL) {
		para.u64[0] = 0;
		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_SET_SEAMLESS_EXTEND_FRAME_LENGTH,
			para.u8, &len);
	}

	ctx->exposure->val = ae_ctrl->exposure.le_exposure;
	ctx->analogue_gain->val = ae_ctrl->gain.le_gain;

	return 0;
}

static int s_ae_ctrl(struct v4l2_ctrl *ctrl)
{
	struct adaptor_ctx *ctx = ctrl_to_ctx(ctrl);
	struct mtk_hdr_ae *ae_ctrl = ctrl->p_new.p;

	if (!ctx->is_streaming) {
		memcpy(&ctx->ae_memento, ae_ctrl,
			   sizeof(ctx->ae_memento));
		dev_info(ctx->dev, "streaming off, store ae_ctrl\n");
		return 0;
	}

	return do_set_ae_ctrl(ctx, ae_ctrl);
}

static int g_volatile_temperature(struct adaptor_ctx *ctx,
		struct v4l2_ctrl *ctrl)
{
	union feature_para para;
	u32 len = 0;

	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_TEMPERATURE_VALUE,
		para.u8, &len);

	if (len)
		*ctrl->p_new.p_s32 = para.u32[0];

	return 0;
}
static int _get_frame_desc(struct adaptor_ctx *ctx, unsigned int pad,
		struct mtk_mbus_frame_desc *fd)
{
	int ret;
//	struct adaptor_ctx *ctx = to_ctx(sd);
	u64 desc_visited = 0x0;
	int write_to = 0, i = -1, j = 0;

	while (i < SENSOR_SCENARIO_ID_MAX) {
		struct mtk_mbus_frame_desc fd_tmp;
		u32 scenario_id = (-1 == i) ? ctx->cur_mode->id : ctx->seamless_scenarios[i];

		if (scenario_id == SENSOR_SCENARIO_ID_NONE)
			break;

		ret = subdrv_call(ctx, get_frame_desc, scenario_id, &fd_tmp);

		if (!ret) {
			for (j = 0; write_to < V4L2_FRAME_DESC_ENTRY_MAX && j < fd_tmp.num_entries;
				++j) {
				if (desc_visited & (0x1 << fd_tmp.entry[j].bus.csi2.user_data_desc))
					continue;

				dev_info(ctx->dev, "[%s] scenario %u desc %d\n", __func__,
						scenario_id,
						fd_tmp.entry[j].bus.csi2.user_data_desc);
				memcpy(&fd->entry[write_to++], &fd_tmp.entry[j],
					   sizeof(struct mtk_mbus_frame_desc_entry));

				desc_visited |= (0x1 << fd_tmp.entry[j].bus.csi2.user_data_desc);
			}
		}

		++i;
	}

	fd->num_entries = write_to;
	fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;

	return ret;
}


static int ext_ctrl(struct adaptor_ctx *ctx, struct v4l2_ctrl *ctrl, struct sensor_mode *mode)
{
	int ret = 0;

	if (mode == NULL)
		return -EINVAL;

	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		ctrl->val = mode->fll - mode->height;
		break;
	case V4L2_CID_HBLANK:
		ctrl->val = mode->llp - mode->width;
		break;
	case V4L2_CID_MTK_SENSOR_PIXEL_RATE:
		ctrl->val = mode->mipi_pixel_rate;
		break;
	case V4L2_CID_MTK_STAGGER_INFO:
	{
		struct mtk_stagger_info *info = ctrl->p_new.p;
		int scenario_id = (info->scenario_id >= SENSOR_SCENARIO_ID_MAX) ?
			info->scenario_id - SENSOR_SCENARIO_ID_MAX : mode->id;

		g_stagger_info(ctx, scenario_id, info);
	}
		break;
	case V4L2_CID_MTK_FRAME_DESC:
	{
		struct mtk_mbus_frame_desc *fd = ctrl->p_new.p;

		_get_frame_desc(ctx, 0, fd);
	}
		break;
	default:
		break;
	}

	return ret;
}

static int imgsensor_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct adaptor_ctx *ctx = ctrl_to_ctx(ctrl);

	switch (ctrl->id) {
#ifdef V4L2_CID_PD_PIXEL_REGION
	case V4L2_CID_PD_PIXEL_REGION:
		ret = g_pd_pixel_region(ctx, ctrl);
		break;
#endif
	case V4L2_CID_MTK_TEMPERATURE:
		ret = g_volatile_temperature(ctx, ctrl);
		break;
	default:
		ret = ext_ctrl(ctx, ctrl, ctx->cur_mode);
		break;
	}

	return ret;
}

static int imgsensor_try_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct adaptor_ctx *ctx = ctrl_to_ctx(ctrl);

	switch (ctrl->id) {
	case V4L2_CID_MAX_EXP_TIME:
	{
		u32 len = 0;
		union feature_para para;
		struct mtk_stagger_max_exp_time *info = ctrl->p_new.p;
		int scenario_id = (info->scenario_id >= SENSOR_SCENARIO_ID_MAX) ?
			info->scenario_id - SENSOR_SCENARIO_ID_MAX : ctx->try_format_mode->id;

		para.u64[0] = scenario_id;
		para.u64[1] = (u64)info->exposure;
		para.u64[2] = 0;

		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_GET_STAGGER_MAX_EXP_TIME,
			para.u8, &len);

		info->max_exp_time = (u32)para.u64[2];
	}
		break;
	case V4L2_CID_STAGGER_TARGET_SCENARIO:
	{
		struct mtk_stagger_target_scenario *info = ctrl->p_new.p;
		int scenario_id = (info->scenario_id >= SENSOR_SCENARIO_ID_MAX) ?
			info->scenario_id - SENSOR_SCENARIO_ID_MAX : ctx->try_format_mode->id;

		g_stagger_scenario(ctx, scenario_id, info);
	}
		break;
	default:
		ret = ext_ctrl(ctx, ctrl, ctx->try_format_mode);
		break;
	}

	return ret;
}

#ifdef IMGSENSOR_DEBUG
static void proc_debug_cmd(struct adaptor_ctx *ctx, char *text)
{
	dev_info(ctx->dev, "%s\n", text);
	if (!strcmp(text, "unregister_subdev"))
		v4l2_async_unregister_subdev(&ctx->sd);
}
#endif


static int imgsensor_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct adaptor_ctx *ctx = ctrl_to_ctx(ctrl);
	struct device *dev = ctx->dev;
	union feature_para para;
	int ret = 0;
	u32 len;

	/*
	 * Applying V4L2 control value only happens
	 * when power is up for streaming
	 */
	if (pm_runtime_get_if_in_use(dev) == 0)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_VSYNC_NOTIFY:
		subdrv_call(ctx, vsync_notify, (u64)ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		para.u64[0] = ctrl->val / 16;
		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_SET_GAIN,
			para.u8, &len);
		break;
	case V4L2_CID_EXPOSURE:
		para.u64[0] = ctrl->val;
		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_SET_ESHUTTER,
			para.u8, &len);
		notify_fsync_mgr_set_shutter(ctx,
			SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME, (u64)ctrl->val);
		break;
	case V4L2_CID_MTK_STAGGER_AE_CTRL:
		s_ae_ctrl(ctrl);
		break;
	case V4L2_CID_EXPOSURE_ABSOLUTE:
		para.u64[0] = ctrl->val * 100000;
		do_div(para.u64[0], ctx->cur_mode->linetime_in_ns);
		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_SET_ESHUTTER,
			para.u8, &len);
		notify_fsync_mgr_set_shutter(ctx,
			SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME, (u64)ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		para.u64[0] = ctx->exposure->val;
		para.u64[1] = ctx->cur_mode->height + ctrl->val;
		para.u64[2] = 0;
		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME,
			para.u8, &len);
		break;
	case V4L2_CID_TEST_PATTERN:
		para.u8[0] = ctrl->val;
		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_SET_TEST_PATTERN,
			para.u8, &len);
		break;
	case V4L2_CID_MTK_ANTI_FLICKER:
		para.u16[0] = ctrl->val;
		para.u16[1] = 0;
		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_SET_AUTO_FLICKER_MODE,
			para.u8, &len);
		notify_fsync_mgr_update_auto_flicker_mode(ctx, (u64)ctrl->val);
		break;

	case V4L2_CID_FRAME_SYNC:
		dev_info(dev,
			"V4L2_CID_FRAME_SYNC (set_sync), idx:%u, value:%d\n",
			ctx->idx, ctrl->val);

		notify_fsync_mgr_set_sync(ctx, (u64)ctrl->val);
		break;

	case V4L2_CID_MTK_AWB_GAIN:
		{
			struct mtk_awb_gain *info = ctrl->p_new.p;
			struct SET_SENSOR_AWB_GAIN awb_gain = {
				.ABS_GAIN_GR = info->abs_gain_gr,
				.ABS_GAIN_R = info->abs_gain_r,
				.ABS_GAIN_B = info->abs_gain_b,
				.ABS_GAIN_GB = info->abs_gain_gb,
			};

			subdrv_call(ctx, feature_control,
				SENSOR_FEATURE_SET_AWB_GAIN,
				(u8 *)&awb_gain, &len);
		}
		break;
	case V4L2_CID_MTK_SHUTTER_GAIN_SYNC:
		{
			struct mtk_shutter_gain_sync *info = ctrl->p_new.p;

			para.u64[0] = info->shutter;
			subdrv_call(ctx, feature_control,
				SENSOR_FEATURE_SET_ESHUTTER,
				para.u8, &len);
			notify_fsync_mgr_set_shutter(ctx,
				SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME,
				(u64)ctrl->val);
			para.u64[0] = info->gain / 16;
			subdrv_call(ctx, feature_control,
				SENSOR_FEATURE_SET_GAIN,
				para.u8, &len);
		}
		break;
	case V4L2_CID_MTK_DUAL_GAIN:
		{
			struct mtk_dual_gain *info = ctrl->p_new.p;
			struct mtk_hdr_gain dual_gain;

			dual_gain.le_gain = info->le_gain;
			dual_gain.se_gain = info->se_gain;

			set_hdr_gain_dual(ctx, &dual_gain);
		}
		break;
	case V4L2_CID_MTK_IHDR_SHUTTER_GAIN:
		{
			struct mtk_ihdr_shutter_gain *info = ctrl->p_new.p;

			para.u64[0] = info->le_shutter;
			para.u64[1] = info->se_shutter;
			para.u64[2] = info->gain;
			subdrv_call(ctx, feature_control,
				SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN,
				para.u8, &len);
		}
		break;
	case V4L2_CID_MTK_HDR_SHUTTER:
		{
			struct mtk_hdr_exposure *info = ctrl->p_new.p;

			set_hdr_exposure_dual(ctx, info);
		}
		break;
	case V4L2_CID_MTK_SHUTTER_FRAME_LENGTH:
		{
			struct mtk_shutter_frame_length *info = ctrl->p_new.p;

			para.u64[0] = info->shutter;
			para.u64[1] = info->frame_length;
			para.u64[2] = info->auto_extend_en;
			subdrv_call(ctx, feature_control,
				SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME,
				para.u8, &len);
		}
		break;
	case V4L2_CID_MTK_PDFOCUS_AREA:
		{
			struct mtk_pdfocus_area *info = ctrl->p_new.p;

			para.u64[0] = info->pos;
			para.u64[1] = info->size;
			subdrv_call(ctx, feature_control,
				SENSOR_FEATURE_SET_PDFOCUS_AREA,
				para.u8, &len);
		}
		break;
	case V4L2_CID_MTK_HDR_ATR:
		{
			struct mtk_hdr_atr *info = ctrl->p_new.p;

			para.u64[0] = info->limit_gain;
			para.u64[1] = info->ltc_rate;
			para.u64[2] = info->post_gain;
			subdrv_call(ctx, feature_control,
				SENSOR_FEATURE_SET_HDR_ATR,
				para.u8, &len);
		}
		break;
	case V4L2_CID_MTK_HDR_TRI_SHUTTER:
		{
			struct mtk_hdr_exposure *info = ctrl->p_new.p;

			set_hdr_exposure_tri(ctx, info);
		}
		break;
	case V4L2_CID_MTK_HDR_TRI_GAIN:
		{
			struct mtk_hdr_gain *info = ctrl->p_new.p;

			set_hdr_gain_tri(ctx, info);
		}
		break;
	case V4L2_CID_MTK_MAX_FPS:
		para.u64[0] = ctx->cur_mode->id;
		para.u64[1] = ctrl->val;
		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO,
			para.u8, &len);
		notify_fsync_mgr_update_min_fl(ctx);
		break;
	case V4L2_CID_SEAMLESS_SCENARIOS:
		{
			struct mtk_seamless_target_scenarios *info = ctrl->p_new.p;

			copy_from_user(&ctx->seamless_scenarios, info->target_scenario_ids,
					min(sizeof(ctx->seamless_scenarios),
					info->count * sizeof(*info->target_scenario_ids)));
		}
		break;
	case V4L2_CID_START_SEAMLESS_SWITCH:
		{
			struct mtk_seamless_switch_param *info = ctrl->p_new.p;

			para.u64[0] = info->target_scenario_id;
			para.u64[1] = (uintptr_t)&info->ae_ctrl[0];
			para.u64[2] = (uintptr_t)&info->ae_ctrl[1];

			dev_info(dev, "V4L2_CID_START_SEAMLESS_SWITCH %u\n",
					info->target_scenario_id);

			dev_info(dev, "shutter[%u %u %u] gain[%u %u %u]\n",
					info->ae_ctrl[0].exposure.arr[0],
					info->ae_ctrl[0].exposure.arr[1],
					info->ae_ctrl[0].exposure.arr[2],
					info->ae_ctrl[0].gain.arr[0],
					info->ae_ctrl[0].gain.arr[1],
					info->ae_ctrl[0].gain.arr[2]);
			dev_info(dev, "shutter[%u %u %u] gain[%u %u %u]\n",
					info->ae_ctrl[1].exposure.arr[0],
					info->ae_ctrl[1].exposure.arr[1],
					info->ae_ctrl[1].exposure.arr[2],
					info->ae_ctrl[1].gain.arr[0],
					info->ae_ctrl[1].gain.arr[1],
					info->ae_ctrl[1].gain.arr[2]);

			subdrv_call(ctx, feature_control,
				SENSOR_FEATURE_SEAMLESS_SWITCH,
				para.u8, &len);
		}
		break;
#ifdef IMGSENSOR_DEBUG
	case V4L2_CID_MTK_DEBUG_CMD:
		proc_debug_cmd(ctx, ctrl->p_new.p_char);
		break;
#endif
	}

	pm_runtime_put(dev);

	return ret;
}

static const struct v4l2_ctrl_ops ctrl_ops = {
	.g_volatile_ctrl = imgsensor_g_volatile_ctrl,
	.try_ctrl = imgsensor_try_ctrl,
	.s_ctrl = imgsensor_set_ctrl,
};

static const char * const test_pattern_menu[] = {
	"Disabled",
	"Colour Bars",
};

#ifdef V4L2_CID_PD_PIXEL_REGION
static const struct v4l2_ctrl_config cfg_pd_pixel_region = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_PD_PIXEL_REGION,
	.type = V4L2_CTRL_TYPE_IMAGE_PD_PIXEL_REGION,
};
#endif

#ifdef V4L2_CID_CAMERA_SENSOR_LOCATION
static const struct v4l2_ctrl_config cfg_location = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_CAMERA_SENSOR_LOCATION,
	.name = "location",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_READ_ONLY,
	.max = 2,
	.step = 1,
};
#endif

#ifdef V4L2_CID_CAMERA_SENSOR_ROTATION
static const struct v4l2_ctrl_config cfg_rotation = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_CAMERA_SENSOR_ROTATION,
	.name = "rotation",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_READ_ONLY,
	.max = 360,
	.step = 1,
};
#endif

static const struct v4l2_ctrl_config cust_vol_cfgs[] = {
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_MTK_TEMPERATURE,
		.name = "temperature",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
		.max = 0x7fffffff,
		.step = 1,
	}
};

static const struct v4l2_ctrl_config cfg_anti_flicker = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_ANTI_FLICKER,
	.name = "anti_flicker",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.max = 1,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_frame_sync = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_FRAME_SYNC,
	.name = "frame_sync",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.max = 1,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_fsync_map_id = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_FSYNC_MAP_ID,
	.name = "fsync_map_id",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.max = 0xffff,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_vsync_notify = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_VSYNC_NOTIFY,
	.name = "vsync_notify",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0xffff,
	.step = 1,
};


static struct v4l2_ctrl_config cfg_ae_ctrl = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_STAGGER_AE_CTRL,
	.name = "ae_ctrl",
	.type = V4L2_CTRL_TYPE_U32,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0xffffffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_hdr_ae)},
};

static struct v4l2_ctrl_config cfg_fd_ctrl = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_FRAME_DESC,
	.name = "frame_des_ctrl",
	.type = V4L2_CTRL_TYPE_U32,
	//.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
	.max = 0xffffffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_mbus_frame_desc)},
};


static const struct v4l2_ctrl_config cfg_awb_gain = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_AWB_GAIN,
	.name = "awb_gain",
	.type = V4L2_CTRL_TYPE_U32,
	.max = 0xffffffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_awb_gain)},
};

/* force to apply repeated settings for frame-sync mode */
static const struct v4l2_ctrl_config cfg_shutter_gain_sync = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_SHUTTER_GAIN_SYNC,
	.name = "shutter_gain_sync",
	.type = V4L2_CTRL_TYPE_U32,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0xffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_shutter_gain_sync)},
};

static const struct v4l2_ctrl_config cfg_dual_gain = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_DUAL_GAIN,
	.name = "dual_gain",
	.type = V4L2_CTRL_TYPE_U32,
	.max = 0xffffffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_dual_gain)},
};

static const struct v4l2_ctrl_config cfg_ihdr_shutter_gain = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_IHDR_SHUTTER_GAIN,
	.name = "ihdr_shutter_gain",
	.type = V4L2_CTRL_TYPE_U32,
	.max = 0xffffffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_ihdr_shutter_gain)},
};

static const struct v4l2_ctrl_config cfg_hdr_shutter = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_HDR_SHUTTER,
	.name = "hdr_shutter",
	.type = V4L2_CTRL_TYPE_U32,
	.max = 0xffffffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_hdr_exposure)},
};

static const struct v4l2_ctrl_config cfg_shutter_frame_length = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_SHUTTER_FRAME_LENGTH,
	.name = "shutter_frame_length",
	.type = V4L2_CTRL_TYPE_U32,
	.max = 0xffffffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_shutter_frame_length)},
};

static const struct v4l2_ctrl_config cfg_pdfocus_area = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_PDFOCUS_AREA,
	.name = "pdfocus_area",
	.type = V4L2_CTRL_TYPE_U32,
	.max = 0xffffffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_pdfocus_area)},
};

static const struct v4l2_ctrl_config cfg_hdr_atr = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_HDR_ATR,
	.name = "hdr_atr",
	.type = V4L2_CTRL_TYPE_U32,
	.max = 0xffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_hdr_atr)},
};

static const struct v4l2_ctrl_config cfg_hdr_tri_shutter = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_HDR_TRI_SHUTTER,
	.name = "hdr_tri_shutter",
	.type = V4L2_CTRL_TYPE_U32,
	.max = 0xffffffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_hdr_exposure)},
};

static const struct v4l2_ctrl_config cfg_hdr_tri_gain = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_HDR_TRI_GAIN,
	.name = "hdr_tri_gain",
	.type = V4L2_CTRL_TYPE_U32,
	.max = 0xffffffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_hdr_gain)},
};

static const struct v4l2_ctrl_config cfg_max_fps = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_MAX_FPS,
	.name = "max_fps",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0xffff,
	.step = 1,
};

static const struct v4l2_ctrl_config cust_mtkcam_pixel_rate = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_SENSOR_PIXEL_RATE,
	.name = "pixel rate",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_VOLATILE,
	.max = 0x7fffffff,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_seamless_scenario = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_SEAMLESS_SCENARIOS,
	.name = "seamless scenario",
	.type = V4L2_CTRL_TYPE_U32,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0xffffffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_seamless_target_scenarios)},
};

static const struct v4l2_ctrl_config cfg_stagger_info = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_STAGGER_INFO,
	.name = "stagger info",
	.type = V4L2_CTRL_TYPE_U32,
	.flags = V4L2_CTRL_FLAG_VOLATILE,
	.max = 0xffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_stagger_info)},
};

static const struct v4l2_ctrl_config cfg_stagger_scenario = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_STAGGER_TARGET_SCENARIO,
	.name = "stagger scenario",
	.type = V4L2_CTRL_TYPE_U32,
	.flags = V4L2_CTRL_FLAG_VOLATILE,
	.max = 0xffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_stagger_target_scenario)},
};

static const struct v4l2_ctrl_config cfg_stagger_max_exp_time = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MAX_EXP_TIME,
	.name = "maximum exposure time",
	.type = V4L2_CTRL_TYPE_U32,
	.flags = V4L2_CTRL_FLAG_VOLATILE,
	.max = 0xffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_stagger_max_exp_time)},
};

static const struct v4l2_ctrl_config cfg_start_seamless_switch = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_START_SEAMLESS_SWITCH,
	.name = "start_seamless_switch",
	.type = V4L2_CTRL_TYPE_U32,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0xffffffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_seamless_switch_param)},
};

#ifdef IMGSENSOR_DEBUG
static const struct v4l2_ctrl_config cfg_debug_cmd = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_DEBUG_CMD,
	.name = "debug_cmd",
	.type = V4L2_CTRL_TYPE_STRING,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 64,
	.step = 1,
};
#endif

void restore_ae_ctrl(struct adaptor_ctx *ctx)
{
	if (!ctx->ae_memento.exposure.le_exposure ||
		!ctx->ae_memento.gain.le_gain) {
		return;
	}

	dev_info(ctx->dev, "%s\n", __func__);

	do_set_ae_ctrl(ctx, &ctx->ae_memento);
	memset(&ctx->ae_memento, 0, sizeof(ctx->ae_memento));
}

int adaptor_init_ctrls(struct adaptor_ctx *ctx)
{
	int i, ret;
	s64 min, max, step, def;
	const struct sensor_mode *cur_mode;
	struct v4l2_ctrl_handler *ctrl_hdlr;
	struct v4l2_ctrl_config cfg;

	ctrl_hdlr = &ctx->ctrls;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 8);
	if (ret)
		return ret;

	ctrl_hdlr->lock = &ctx->mutex;
	cur_mode = ctx->cur_mode;

	/* pixel rate */
	min = max = def = cur_mode->mipi_pixel_rate;
	ctx->pixel_rate = v4l2_ctrl_new_std(ctrl_hdlr, &ctrl_ops,
				V4L2_CID_PIXEL_RATE, min, max, 1, def);

	/* pixel rate for try control*/
	v4l2_ctrl_new_custom(&ctx->ctrls, &cust_mtkcam_pixel_rate, NULL);

	/* hblank */
	min = max = def = cur_mode->llp - cur_mode->width;
	ctx->hblank = v4l2_ctrl_new_std(ctrl_hdlr, &ctrl_ops,
				V4L2_CID_HBLANK, min, max, 1, def);
	if (ctx->hblank)
		ctx->hblank->flags |= V4L2_CTRL_FLAG_VOLATILE;

	/* vblank */
	min = def = cur_mode->fll - cur_mode->height;
	max = ctx->subctx.max_frame_length - cur_mode->height;
	ctx->vblank = v4l2_ctrl_new_std(ctrl_hdlr, &ctrl_ops,
				V4L2_CID_VBLANK, min, max, 1, def);
	if (ctx->vblank)
		ctx->vblank->flags |= V4L2_CTRL_FLAG_VOLATILE;

	/* ae ctrl */
	ctx->hdr_ae_ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &cfg_ae_ctrl, NULL);

	if (ctx->hdr_ae_ctrl)
		ctx->hdr_ae_ctrl->flags |= V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;

	/* exposure */
	min = ctx->subctx.exposure_min;
	max = ctx->subctx.exposure_max;
	step = ctx->subctx.exposure_step;
	def = ctx->subctx.exposure_def;

	ctx->exposure = v4l2_ctrl_new_std(ctrl_hdlr, &ctrl_ops,
			V4L2_CID_EXPOSURE, min, max, step, def);

	if (ctx->exposure)
		ctx->exposure->flags |= V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;

	min = ctx->subctx.ana_gain_min * 16;
	max = ctx->subctx.ana_gain_max * 16;
	step = ctx->subctx.ana_gain_step * 16;
	def = ctx->subctx.ana_gain_def * 16;

	ctx->analogue_gain = v4l2_ctrl_new_std(ctrl_hdlr, &ctrl_ops,
			V4L2_CID_ANALOGUE_GAIN, min, max, step, def);

	if (ctx->analogue_gain)
		ctx->analogue_gain->flags |= V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;

	/* exposure_absolute: in 100 us */
	min = min * cur_mode->linetime_in_ns;
	do_div(min, 100000);
	max = max * cur_mode->linetime_in_ns;
	do_div(max, 100000);
	def = def * cur_mode->linetime_in_ns;
	do_div(def, 100000);
	ctx->exposure_abs = v4l2_ctrl_new_std(ctrl_hdlr, &ctrl_ops,
			V4L2_CID_EXPOSURE_ABSOLUTE, min, max, 1, def);
	if (ctx->exposure_abs)
		ctx->exposure_abs->flags |= V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;

	/* test pattern */
	v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &ctrl_ops,
			V4L2_CID_TEST_PATTERN,
			ARRAY_SIZE(test_pattern_menu) - 1,
			0, 0, test_pattern_menu);

	/* hflip */
	def = ctx->subctx.is_hflip;
	ctx->hflip = v4l2_ctrl_new_std(ctrl_hdlr, &ctrl_ops,
			V4L2_CID_HFLIP, 0, 1, 1, def);
	if (ctx->hflip)
		ctx->hflip->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	/* vflip */
	def = ctx->subctx.is_vflip;
	ctx->vflip = v4l2_ctrl_new_std(ctrl_hdlr, &ctrl_ops,
			V4L2_CID_VFLIP, 0, 1, 1, def);
	if (ctx->vflip)
		ctx->vflip->flags |= V4L2_CTRL_FLAG_READ_ONLY;

#ifdef V4L2_CID_PD_PIXEL_REGION
	ctx->pd_pixel_region =
		v4l2_ctrl_new_custom(ctrl_hdlr, &cfg_pd_pixel_region, NULL);
	if (ctx->pd_pixel_region) {
		ctx->pd_pixel_region->flags |=
			V4L2_CTRL_FLAG_READ_ONLY |
			V4L2_CTRL_FLAG_VOLATILE;
	}
#endif

	memset(&cfg, 0, sizeof(cfg));

#ifdef V4L2_CID_CAMERA_SENSOR_LOCATION
	memcpy(&cfg, &cfg_location, sizeof(cfg));
	cfg.def = ctx->location;
	v4l2_ctrl_new_custom(ctrl_hdlr, &cfg_location, NULL);
#endif

#ifdef V4L2_CID_CAMERA_SENSOR_ROTATION
	memcpy(&cfg, &cfg_rotation, sizeof(cfg));
	cfg.def = ctx->rotation;
	v4l2_ctrl_new_custom(ctrl_hdlr, &cfg, NULL);
#endif

	/* custom volatile configs */
	for (i = 0; i < ARRAY_SIZE(cust_vol_cfgs); i++)
		v4l2_ctrl_new_custom(&ctx->ctrls, &cust_vol_cfgs[i], NULL);

	/* custom anti-flicker */
	ctx->anti_flicker = v4l2_ctrl_new_custom(&ctx->ctrls,
		&cfg_anti_flicker, NULL);

	/* custom frame-sync */
	ctx->frame_sync = v4l2_ctrl_new_custom(&ctx->ctrls,
		&cfg_frame_sync, NULL);

	/* custom awb gain */
	ctx->awb_gain = v4l2_ctrl_new_custom(&ctx->ctrls,
		&cfg_awb_gain, NULL);

	/* custom shutter-gain-sync */
	ctx->shutter_gain_sync = v4l2_ctrl_new_custom(&ctx->ctrls,
		&cfg_shutter_gain_sync, NULL);

	/* custom dual-gain */
	if (ctx->subctx.hdr_cap & (HDR_CAP_MVHDR | HDR_CAP_ZHDR)) {
		ctx->dual_gain = v4l2_ctrl_new_custom(&ctx->ctrls,
			&cfg_dual_gain, NULL);
	}

	/* custom ihdr-shutter-gain */
	if (ctx->subctx.hdr_cap & HDR_CAP_IHDR) {
		ctx->ihdr_shutter_gain = v4l2_ctrl_new_custom(&ctx->ctrls,
			&cfg_ihdr_shutter_gain, NULL);
	}

	/* custom hdr-shutter */
	ctx->hdr_shutter = v4l2_ctrl_new_custom(&ctx->ctrls,
		&cfg_hdr_shutter, NULL);

	/* custom shutter-frame-length */
	ctx->shutter_frame_length = v4l2_ctrl_new_custom(&ctx->ctrls,
		&cfg_shutter_frame_length, NULL);

	/* custom pdfocus-area */
	if (ctx->subctx.pdaf_cap & PDAF_CAP_PDFOCUS_AREA) {
		ctx->pdfocus_area = v4l2_ctrl_new_custom(&ctx->ctrls,
			&cfg_pdfocus_area, NULL);
	}

	/* custom hdr-atr */
	if (ctx->subctx.hdr_cap & HDR_CAP_ATR) {
		ctx->hdr_atr = v4l2_ctrl_new_custom(
			&ctx->ctrls, &cfg_hdr_atr, NULL);
	}

	/* custom hdr-tri-shutter/gain */
	if (ctx->subctx.hdr_cap & HDR_CAP_3HDR) {
		ctx->hdr_tri_shutter = v4l2_ctrl_new_custom(&ctx->ctrls,
			&cfg_hdr_tri_shutter, NULL);
		ctx->hdr_tri_gain = v4l2_ctrl_new_custom(&ctx->ctrls,
			&cfg_hdr_tri_gain, NULL);
	}

	/* max fps */
	max = def = cur_mode->max_framerate;
	memcpy(&cfg, &cfg_max_fps, sizeof(cfg));
	cfg.min = 1;
	cfg.max = max;
	cfg.def = def;
	ctx->max_fps = v4l2_ctrl_new_custom(&ctx->ctrls, &cfg, NULL);

	/* update fsync map id (cammux idx) */
	ctx->fsync_map_id = v4l2_ctrl_new_custom(&ctx->ctrls,
		&cfg_fsync_map_id, NULL);

	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_vsync_notify, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_stagger_info, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_stagger_scenario, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_stagger_max_exp_time, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_start_seamless_switch, NULL);

	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_seamless_scenario, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_fd_ctrl, NULL);

#ifdef IMGSENSOR_DEBUG
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_debug_cmd, NULL);
#endif

	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(ctx->dev, "control init failed: %d\n", ret);
		goto error;
	}

	ctx->sd.ctrl_handler = ctrl_hdlr;

	return 0;

error:
	v4l2_ctrl_handler_free(ctrl_hdlr);

	return ret;
}
