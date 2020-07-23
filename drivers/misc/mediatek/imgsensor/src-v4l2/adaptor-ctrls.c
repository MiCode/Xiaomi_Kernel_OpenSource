// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2020 MediaTek Inc.

#include <linux/pm_runtime.h>

#include "kd_imgsensor_define.h"
#include "imgsensor-user.h"
#include "adaptor.h"
#include "adaptor-i2c.h"
#include "adaptor-ctrls.h"

#define ctrl_to_ctx(ctrl) \
	container_of(ctrl->handler, struct adaptor_ctx, ctrls)

#define sizeof_u32(__struct_name__) (sizeof(__struct_name__) / sizeof(u32))
#define sizeof_u16(__struct_name__) (sizeof(__struct_name__) / sizeof(u16))

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

	ctx->sensor->ops->SensorFeatureControl(
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

static int g_volatile_temperature(struct adaptor_ctx *ctx, struct v4l2_ctrl *ctrl)
{
	union feature_para para;
	u32 len = 0;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_TEMPERATURE_VALUE,
		para.u8, &len);

	if (len)
		*ctrl->p_new.p_s32 = para.u32[0];

	return 0;
}

static int imgsensor_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct adaptor_ctx *ctx = ctrl_to_ctx(ctrl);

	adaptor_legacy_lock();
	adaptor_legacy_set_i2c_client(ctx->i2c_client);

	switch (ctrl->id) {
#ifdef V4L2_CID_PD_PIXEL_REGION
	case V4L2_CID_PD_PIXEL_REGION:
		ret = g_pd_pixel_region(ctx, ctrl);
		break;
#endif
	case V4L2_CID_MTK_TEMPERATURE:
		ret = g_volatile_temperature(ctx, ctrl);
		break;
	}

	adaptor_legacy_unlock();

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

	adaptor_legacy_lock();
	adaptor_legacy_set_i2c_client(ctx->i2c_client);

	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		para.u64[0] = ctrl->val;
		ctx->sensor->ops->SensorFeatureControl(
			SENSOR_FEATURE_SET_GAIN,
			para.u8, &len);
		break;
	case V4L2_CID_EXPOSURE:
		para.u64[0] = ctrl->val;
		ctx->sensor->ops->SensorFeatureControl(
			SENSOR_FEATURE_SET_ESHUTTER,
			para.u8, &len);
		break;
	case V4L2_CID_EXPOSURE_ABSOLUTE:
		para.u64[0] = ctrl->val * 100000;
		do_div(para.u64[0], ctx->cur_mode->linetime_in_ns);
		ctx->sensor->ops->SensorFeatureControl(
			SENSOR_FEATURE_SET_ESHUTTER,
			para.u8, &len);
		break;
	case V4L2_CID_VBLANK:
		para.u64[0] = ctx->exposure->val;
		para.u64[1] = ctx->cur_mode->height + ctrl->val;
		para.u64[0] = 0;
		ctx->sensor->ops->SensorFeatureControl(
			SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME,
			para.u8, &len);
		break;
	case V4L2_CID_TEST_PATTERN:
		para.u8[0] = ctrl->val;
		ctx->sensor->ops->SensorFeatureControl(
			SENSOR_FEATURE_SET_TEST_PATTERN,
			para.u8, &len);
		break;
	case V4L2_CID_MTK_ANTI_FLICKER:
		para.u16[0] = ctrl->val;
		para.u16[1] = 0;
		ctx->sensor->ops->SensorFeatureControl(
			SENSOR_FEATURE_SET_AUTO_FLICKER_MODE,
			para.u8, &len);
		break;

	case V4L2_CID_MTK_FRAME_SYNC:
		/* TODO: notify frame-sync module of sync status */
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

			ctx->sensor->ops->SensorFeatureControl(
				SENSOR_FEATURE_SET_AWB_GAIN,
				(u8 *)&awb_gain, &len);
		}
		break;
	case V4L2_CID_MTK_SHUTTER_GAIN_SYNC:
		{
			struct mtk_shutter_gain_sync *info = ctrl->p_new.p;

			para.u64[0] = info->shutter;
			ctx->sensor->ops->SensorFeatureControl(
				SENSOR_FEATURE_SET_ESHUTTER,
				para.u8, &len);
			para.u64[0] = info->gain;
			ctx->sensor->ops->SensorFeatureControl(
				SENSOR_FEATURE_SET_GAIN,
				para.u8, &len);
		}
		break;
	case V4L2_CID_MTK_DUAL_GAIN:
		{
			struct mtk_dual_gain *info = ctrl->p_new.p;

			para.u64[0] = info->le_gain;
			para.u64[1] = info->se_gain;
			ctx->sensor->ops->SensorFeatureControl(
				SENSOR_FEATURE_SET_DUAL_GAIN,
				para.u8, &len);
		}
		break;
	case V4L2_CID_MTK_IHDR_SHUTTER_GAIN:
		{
			struct mtk_ihdr_shutter_gain *info = ctrl->p_new.p;

			para.u64[0] = info->le_shutter;
			para.u64[1] = info->se_shutter;
			para.u64[2] = info->gain;
			ctx->sensor->ops->SensorFeatureControl(
				SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN,
				para.u8, &len);
		}
		break;
	case V4L2_CID_MTK_HDR_SHUTTER:
		{
			struct mtk_hdr_shutter *info = ctrl->p_new.p;

			para.u64[0] = info->le_shutter;
			para.u64[1] = info->se_shutter;
			ctx->sensor->ops->SensorFeatureControl(
				SENSOR_FEATURE_SET_HDR_SHUTTER,
				para.u8, &len);
		}
		break;
	case V4L2_CID_MTK_SHUTTER_FRAME_LENGTH:
		{
			struct mtk_shutter_frame_length *info = ctrl->p_new.p;

			para.u64[0] = info->shutter;
			para.u64[1] = info->frame_length;
			para.u64[2] = info->auto_extend_en;
			ctx->sensor->ops->SensorFeatureControl(
				SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME,
				para.u8, &len);
		}
		break;
	case V4L2_CID_MTK_PDFOCUS_AREA:
		{
			struct mtk_pdfocus_area *info = ctrl->p_new.p;

			para.u64[0] = info->pos;
			para.u64[1] = info->size;
			ctx->sensor->ops->SensorFeatureControl(
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
			ctx->sensor->ops->SensorFeatureControl(
				SENSOR_FEATURE_SET_HDR_ATR,
				para.u8, &len);
		}
		break;
	case V4L2_CID_MTK_HDR_TRI_SHUTTER:
		{
			struct mtk_hdr_tri_shutter *info = ctrl->p_new.p;

			para.u64[0] = info->le_shutter;
			para.u64[1] = info->me_shutter;
			para.u64[2] = info->se_shutter;
			ctx->sensor->ops->SensorFeatureControl(
				SENSOR_FEATURE_SET_HDR_TRI_SHUTTER,
				para.u8, &len);
		}
		break;
	case V4L2_CID_MTK_HDR_TRI_GAIN:
		{
			struct mtk_hdr_tri_gain *info = ctrl->p_new.p;

			para.u64[0] = info->le_gain;
			para.u64[1] = info->me_gain;
			para.u64[2] = info->se_gain;
			ctx->sensor->ops->SensorFeatureControl(
				SENSOR_FEATURE_SET_HDR_TRI_GAIN,
				para.u8, &len);
		}
		break;
#ifdef IMGSENSOR_DEBUG
	case V4L2_CID_MTK_DEBUG_CMD:
		proc_debug_cmd(ctx, ctrl->p_new.p_char);
		break;
#endif
	default:
		dev_info(dev, "ctrl(id:0x%x,val:0x%x) is not handled\n",
			 ctrl->id, ctrl->val);
		ret = -EINVAL;
		break;
	}

	adaptor_legacy_unlock();

	pm_runtime_put(dev);

	return ret;
}

static const struct v4l2_ctrl_ops ctrl_ops = {
	.g_volatile_ctrl = imgsensor_g_volatile_ctrl,
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
	.id = V4L2_CID_MTK_FRAME_SYNC,
	.name = "frame_sync",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.max = 1,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_awb_gain = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_AWB_GAIN,
	.name = "awb_gain",
	.type = V4L2_CTRL_TYPE_U32,
	.max = 0xffff,
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
	.max = 0xffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_dual_gain)},
};

static const struct v4l2_ctrl_config cfg_ihdr_shutter_gain = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_IHDR_SHUTTER_GAIN,
	.name = "ihdr_shutter_gain",
	.type = V4L2_CTRL_TYPE_U32,
	.max = 0xffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_ihdr_shutter_gain)},
};

static const struct v4l2_ctrl_config cfg_hdr_shutter = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_HDR_SHUTTER,
	.name = "hdr_shutter",
	.type = V4L2_CTRL_TYPE_U32,
	.max = 0xffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_hdr_shutter)},
};

static const struct v4l2_ctrl_config cfg_shutter_frame_length = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_SHUTTER_FRAME_LENGTH,
	.name = "shutter_frame_length",
	.type = V4L2_CTRL_TYPE_U32,
	.max = 0xffff,
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
	.max = 0xffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_hdr_tri_shutter)},
};

static const struct v4l2_ctrl_config cfg_hdr_tri_gain = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_HDR_TRI_GAIN,
	.name = "hdr_tri_gain",
	.type = V4L2_CTRL_TYPE_U32,
	.max = 0xffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_hdr_tri_gain)},
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
	if (ctx->pixel_rate)
		ctx->pixel_rate->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	/* hblank */
	min = max = def = cur_mode->llp - cur_mode->width;
	ctx->hblank = v4l2_ctrl_new_std(ctrl_hdlr, &ctrl_ops,
				V4L2_CID_HBLANK, min, max, 1, def);
	if (ctx->hblank)
		ctx->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	/* vblank */
	min = def = cur_mode->fll - cur_mode->height;
	max = ctx->sensor->max_frame_length - cur_mode->height;
	ctx->vblank = v4l2_ctrl_new_std(ctrl_hdlr, &ctrl_ops,
				V4L2_CID_VBLANK, min, max, 1, def);

	/* exposure */
	min = ctx->sensor->exposure_min;
	max = ctx->sensor->exposure_max;
	step = ctx->sensor->exposure_step;
	def = ctx->sensor->exposure_def;
	ctx->exposure = v4l2_ctrl_new_std(ctrl_hdlr, &ctrl_ops,
			V4L2_CID_EXPOSURE, min, max, step, def);

	/* exposure_absolute: in 100 us */
	min = min * cur_mode->linetime_in_ns;
	do_div(min, 100000);
	max = max * cur_mode->linetime_in_ns;
	do_div(max, 100000);
	def = def * cur_mode->linetime_in_ns;
	do_div(def, 100000);
	ctx->exposure_abs = v4l2_ctrl_new_std(ctrl_hdlr, &ctrl_ops,
			V4L2_CID_EXPOSURE_ABSOLUTE, min, max, 1, def);

	/* analog gain */
	v4l2_ctrl_new_std(ctrl_hdlr, &ctrl_ops,
			V4L2_CID_ANALOGUE_GAIN,
			ctx->sensor->ana_gain_min,
			ctx->sensor->ana_gain_max,
			ctx->sensor->ana_gain_step,
			ctx->sensor->ana_gain_def);

	/* test pattern */
	v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &ctrl_ops,
			V4L2_CID_TEST_PATTERN,
			ARRAY_SIZE(test_pattern_menu) - 1,
			0, 0, test_pattern_menu);

	/* hflip */
	def = ctx->sensor->is_hflip;
	ctx->hflip = v4l2_ctrl_new_std(ctrl_hdlr, &ctrl_ops,
			V4L2_CID_HFLIP, 0, 1, 1, def);
	if (ctx->hflip)
		ctx->hflip->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	/* vflip */
	def = ctx->sensor->is_vflip;
	ctx->vflip = v4l2_ctrl_new_std(ctrl_hdlr, &ctrl_ops,
			V4L2_CID_VFLIP, 0, 1, 1, def);
	if (ctx->vflip)
		ctx->vflip->flags |= V4L2_CTRL_FLAG_READ_ONLY;

#ifdef V4L2_CID_PD_PIXEL_REGION
	ctx->pd_pixel_region = v4l2_ctrl_new_custom(ctrl_hdlr, &cfg_pd_pixel_region, NULL);
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
	if (ctx->sensor->hdr_cap & (HDR_CAP_MVHDR | HDR_CAP_ZHDR)) {
		ctx->dual_gain = v4l2_ctrl_new_custom(&ctx->ctrls,
			&cfg_dual_gain, NULL);
	}

	/* custom ihdr-shutter-gain */
	if (ctx->sensor->hdr_cap & HDR_CAP_IHDR) {
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
	if (ctx->sensor->pdaf_cap & PDAF_CAP_PDFOCUS_AREA) {
		ctx->pdfocus_area = v4l2_ctrl_new_custom(&ctx->ctrls,
			&cfg_pdfocus_area, NULL);
	}

	/* custom hdr-atr */
	if (ctx->sensor->hdr_cap & HDR_CAP_ATR) {
		ctx->hdr_atr = v4l2_ctrl_new_custom(
			&ctx->ctrls, &cfg_hdr_atr, NULL);
	}

	/* custom hdr-tri-shutter/gain */
	if (ctx->sensor->hdr_cap & HDR_CAP_3HDR) {
		ctx->hdr_tri_shutter = v4l2_ctrl_new_custom(&ctx->ctrls,
			&cfg_hdr_tri_shutter, NULL);
		ctx->hdr_tri_gain = v4l2_ctrl_new_custom(&ctx->ctrls,
			&cfg_hdr_tri_gain, NULL);
	}

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

