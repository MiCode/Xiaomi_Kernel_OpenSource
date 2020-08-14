// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/thermal.h>

#include "kd_imgsensor_define.h"

#include "adaptor.h"
#include "adaptor-hw.h"
#include "adaptor-i2c.h"
#include "adaptor-ctrls.h"
#include "adaptor-ioctl.h"

#define to_ctx(__sd) container_of(__sd, struct adaptor_ctx, sd)

#undef E
#define E(__x__) (__x__##_entry)
#define EXTERN_IMGSENSOR_SUBDRVS extern struct subdrv_entry \
	IMGSENSOR_SUBDRVS
EXTERN_IMGSENSOR_SUBDRVS;

#undef E
#define E(__x__) (&__x__##_entry)
static struct subdrv_entry *imgsensor_subdrvs[] = {
	IMGSENSOR_SUBDRVS
};

static get_outfmt_code(struct adaptor_ctx *ctx)
{
	int outfmt = ctx->sensor_info.SensorOutputDataFormat;

	switch (outfmt) {
	case SENSOR_OUTPUT_FORMAT_RAW_B:
		return MEDIA_BUS_FMT_SBGGR10_1X10;
	case SENSOR_OUTPUT_FORMAT_RAW_Gb:
		return MEDIA_BUS_FMT_SGBRG10_1X10;
	case SENSOR_OUTPUT_FORMAT_RAW_Gr:
		return MEDIA_BUS_FMT_SGRBG10_1X10;
	case SENSOR_OUTPUT_FORMAT_RAW_R:
		return MEDIA_BUS_FMT_SRGGB10_1X10;

	case SENSOR_OUTPUT_FORMAT_RAW_4CELL_B:
	case SENSOR_OUTPUT_FORMAT_RAW_4CELL_BAYER_B:
	case SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B:
		pr_warn("unsupported 4cell output_format %d\n", outfmt);
		return MEDIA_BUS_FMT_SBGGR10_1X10;

	case SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gb:
	case SENSOR_OUTPUT_FORMAT_RAW_4CELL_BAYER_Gb:
	case SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gb:
		pr_warn("unsupported 4cell output_format %d\n", outfmt);
		return MEDIA_BUS_FMT_SGBRG10_1X10;

	case SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gr:
	case SENSOR_OUTPUT_FORMAT_RAW_4CELL_BAYER_Gr:
	case SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gr:
		pr_warn("unsupported 4cell output_format %d\n", outfmt);
		return MEDIA_BUS_FMT_SGRBG10_1X10;

	case SENSOR_OUTPUT_FORMAT_RAW_4CELL_R:
	case SENSOR_OUTPUT_FORMAT_RAW_4CELL_BAYER_R:
	case SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R:
		pr_warn("unsupported 4cell output_format %d\n", outfmt);
		return MEDIA_BUS_FMT_SRGGB10_1X10;
	}

	pr_warn("unknown output format %d\n", outfmt);

	return MEDIA_BUS_FMT_SBGGR10_1X10;
}

static void add_sensor_mode(struct adaptor_ctx *ctx,
		int id, int width, int height)
{
	union feature_para para;
	u32 idx, val, len;
	struct sensor_mode *mode;

	idx = ctx->mode_cnt;

	if (idx >= MODE_MAXCNT) {
		dev_warn(ctx->dev, "invalid mode idx %d\n", idx);
		return;
	}

	mode = &ctx->mode[idx];
	mode->id = id;
	mode->width = width;
	mode->height = height;

	para.u64[0] = id;
	para.u64[1] = (u64)&val;

	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO,
		para.u8, &len);

	mode->llp = val & 0xffff;
	mode->fll = val >> 16;

	if (!mode->llp || !mode->fll)
		return;

	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_MIPI_PIXEL_RATE,
		para.u8, &len);

	mode->mipi_pixel_rate = val;

	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO,
		para.u8, &len);

	mode->max_framerate = val;

	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO,
		para.u8, &len);

	mode->pclk = val;

	if (!mode->mipi_pixel_rate || !mode->max_framerate || !mode->pclk)
		return;

	/* update linetime_in_ns */
	mode->linetime_in_ns = (u64)mode->llp * 1000000 +
		(mode->pclk / 1000 - 1);
	do_div(mode->linetime_in_ns, mode->pclk / 1000);

	dev_info(ctx->dev, "[%d] id %d %dx%d %dx%d px %d fps %d\n",
		idx, id, width, height,
		mode->llp, mode->fll,
		mode->mipi_pixel_rate, mode->max_framerate);

	ctx->mode_cnt++;
}

static int init_sensor_mode(struct adaptor_ctx *ctx)
{
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT res;

	subdrv_call(ctx, get_resolution, &res);

	// preview
	if (res.SensorPreviewWidth && res.SensorPreviewHeight) {
		add_sensor_mode(ctx,
			MSDK_SCENARIO_ID_CAMERA_PREVIEW,
			res.SensorPreviewWidth,
			res.SensorPreviewHeight);
	}

	// capture
	if (res.SensorFullWidth && res.SensorFullHeight) {
		add_sensor_mode(ctx,
			MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG,
			res.SensorFullWidth,
			res.SensorFullHeight);
	}

	// video
	if (res.SensorVideoWidth && res.SensorVideoHeight) {
		add_sensor_mode(ctx,
			MSDK_SCENARIO_ID_VIDEO_PREVIEW,
			res.SensorVideoWidth,
			res.SensorVideoHeight);
	}

	// high-speed video
	if (res.SensorHighSpeedVideoWidth && res.SensorHighSpeedVideoHeight) {
		add_sensor_mode(ctx,
			MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO,
			res.SensorHighSpeedVideoWidth,
			res.SensorHighSpeedVideoHeight);
	}

	// slim video
	if (res.SensorSlimVideoWidth && res.SensorSlimVideoHeight) {
		add_sensor_mode(ctx,
			MSDK_SCENARIO_ID_SLIM_VIDEO,
			res.SensorSlimVideoWidth,
			res.SensorSlimVideoHeight);
	}

	// custom1
	if (res.SensorCustom1Width && res.SensorCustom1Height) {
		add_sensor_mode(ctx,
			MSDK_SCENARIO_ID_CUSTOM1,
			res.SensorCustom1Width,
			res.SensorCustom1Height);
	}

	// custom2
	if (res.SensorCustom2Width && res.SensorCustom2Height) {
		add_sensor_mode(ctx,
			MSDK_SCENARIO_ID_CUSTOM2,
			res.SensorCustom2Width,
			res.SensorCustom2Height);
	}

	// custom3
	if (res.SensorCustom3Width && res.SensorCustom3Height) {
		add_sensor_mode(ctx,
			MSDK_SCENARIO_ID_CUSTOM3,
			res.SensorCustom3Width,
			res.SensorCustom3Height);
	}

	// custom4
	if (res.SensorCustom4Width && res.SensorCustom4Height) {
		add_sensor_mode(ctx,
			MSDK_SCENARIO_ID_CUSTOM4,
			res.SensorCustom4Width,
			res.SensorCustom4Height);
	}

	// custom5
	if (res.SensorCustom5Width && res.SensorCustom5Height) {
		add_sensor_mode(ctx,
			MSDK_SCENARIO_ID_CUSTOM5,
			res.SensorCustom5Width,
			res.SensorCustom5Height);
	}

	return 0;
}

static int set_sensor_mode(struct adaptor_ctx *ctx, struct sensor_mode *mode)
{
	if (ctx->cur_mode == mode)
		return 0;

	ctx->cur_mode = mode;

	subdrv_call(ctx, get_info,
			mode->id,
			&ctx->sensor_info,
			&ctx->sensor_cfg);

	return 0;
}

static int init_sensor_info(struct adaptor_ctx *ctx)
{
	init_sensor_mode(ctx);
	set_sensor_mode(ctx, &ctx->mode[0]);
	ctx->fmt_code = get_outfmt_code(ctx);
	return 0;
}

static int search_sensor(struct adaptor_ctx *ctx)
{
	int ret, i, j, of_sensor_names_cnt, subdrvs_cnt;
	struct subdrv_entry **subdrvs, *subdrv;
	struct subdrv_entry *of_subdrvs[OF_SENSOR_NAMES_MAXCNT];
	const char *of_sensor_names[OF_SENSOR_NAMES_MAXCNT];

	of_sensor_names_cnt = of_property_read_string_array(ctx->dev->of_node,
		"sensor-names", of_sensor_names, ARRAY_SIZE(of_sensor_names));

	/* try to load custom list from DT */
	if (of_sensor_names_cnt > 0) {
		subdrvs = of_subdrvs;
		subdrvs_cnt = 0;
		for (i = 0; i < of_sensor_names_cnt; i++) {
			for (j = 0; j < ARRAY_SIZE(imgsensor_subdrvs); j++) {
				subdrv = imgsensor_subdrvs[j];
				if (!strcmp(subdrv->name,
					of_sensor_names[i])) {
					of_subdrvs[subdrvs_cnt++] = subdrv;
					break;
				}
			}
			if (j == ARRAY_SIZE(imgsensor_subdrvs)) {
				dev_warn(ctx->dev, "%s not found\n",
					of_sensor_names[i]);
			}
		}
	} else {
		subdrvs = imgsensor_subdrvs;
		subdrvs_cnt = ARRAY_SIZE(imgsensor_subdrvs);
	}

	for (i = 0; i < subdrvs_cnt; i++) {
		u32 sensor_id;

		ctx->subdrv = subdrvs[i];
		ctx->subctx.i2c_client = ctx->i2c_client;
		adaptor_hw_power_on(ctx);
		ret = subdrv_call(ctx, get_id, &sensor_id);
		adaptor_hw_power_off(ctx);
		if (!ret) {
			dev_info(ctx->dev, "sensor %s found\n",
				ctx->subdrv->name);
			subdrv_call(ctx, init_ctx, ctx->i2c_client,
				ctx->subctx.i2c_write_id);
			return 0;
		}
	}

	return -EIO;
}

static int imgsensor_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct adaptor_ctx *ctx = to_ctx(sd);
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(sd, fh->pad, 0);

	mutex_lock(&ctx->mutex);

	/* Initialize try_fmt */
	try_fmt->width = ctx->cur_mode->width;
	try_fmt->height = ctx->cur_mode->height;
	try_fmt->code = ctx->fmt_code;
	try_fmt->field = V4L2_FIELD_NONE;

#ifdef POWERON_ONCE_OPENED
	pm_runtime_get_sync(ctx->dev);

	if (!ctx->is_sensor_init) {
		subdrv_call(ctx, open);
		ctx->is_sensor_init = 1;
	}
#endif

	mutex_unlock(&ctx->mutex);

	return 0;
}

static int imgsensor_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct adaptor_ctx *ctx = to_ctx(sd);

	mutex_lock(&ctx->mutex);

#ifdef POWERON_ONCE_OPENED
	pm_runtime_put(ctx->dev);
#endif

	mutex_unlock(&ctx->mutex);

	return 0;
}

static int imgsensor_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	struct adaptor_ctx *ctx = to_ctx(sd);

	/* Only one bayer order(GRBG) is supported */
	if (code->index > 0)
		return -EINVAL;

	code->code = ctx->fmt_code;

	return 0;
}

static int imgsensor_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	struct adaptor_ctx *ctx = to_ctx(sd);

	if (fse->index >= ctx->mode_cnt)
		return -EINVAL;

	if (fse->code != ctx->fmt_code)
		return -EINVAL;

	fse->min_width = ctx->mode[fse->index].width;
	fse->max_width = fse->min_width;
	fse->min_height = ctx->mode[fse->index].height;
	fse->max_height = fse->min_height;

	return 0;
}

static int imgsensor_get_selection(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_selection *sel)
{
	struct adaptor_ctx *ctx = to_ctx(sd);

	if (sel->which != V4L2_SUBDEV_FORMAT_ACTIVE)
		return -EINVAL;

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP:
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = ctx->cur_mode->width;
		sel->r.height = ctx->cur_mode->height;
		return 0;
	default:
		return -EINVAL;
	}
}

static void update_pad_format(struct adaptor_ctx *ctx,
		const struct sensor_mode *mode,
		struct v4l2_subdev_format *fmt)
{
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.code = ctx->fmt_code;
	fmt->format.field = V4L2_FIELD_NONE;
}

static int __imgsensor_get_pad_format(struct adaptor_ctx *ctx,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_format *fmt)
{
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		fmt->format = *v4l2_subdev_get_try_format(&ctx->sd, cfg,
							  fmt->pad);
	else
		update_pad_format(ctx, ctx->cur_mode, fmt);

	return 0;
}

static int imgsensor_get_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_format *fmt)
{
	struct adaptor_ctx *ctx = to_ctx(sd);
	int ret;

	mutex_lock(&ctx->mutex);
	ret = __imgsensor_get_pad_format(ctx, cfg, fmt);
	mutex_unlock(&ctx->mutex);

	return ret;
}

static int imgsensor_set_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_format *fmt)
{
	struct adaptor_ctx *ctx = to_ctx(sd);
	struct sensor_mode *mode;
	struct v4l2_mbus_framefmt *framefmt;
	s64 min, max, def;

	mutex_lock(&ctx->mutex);

	/* Only one raw bayer order is supported */
	fmt->format.code = ctx->fmt_code;

	mode = v4l2_find_nearest_size(ctx->mode,
		ctx->mode_cnt, width, height,
		fmt->format.width, fmt->format.height);
	update_pad_format(ctx, mode, fmt);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		framefmt = v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
		*framefmt = fmt->format;
	} else {
		set_sensor_mode(ctx, mode);

		/* pixel rate */
		min = max = def = mode->mipi_pixel_rate;
		__v4l2_ctrl_modify_range(ctx->pixel_rate, min, max, 1, def);

		/* hblank */
		min = max = def = mode->llp - mode->width;
		__v4l2_ctrl_modify_range(ctx->hblank, min, max, 1, def);

		/* vblank */
		min = def = mode->fll - mode->height;
		max = ctx->subdrv->max_frame_length - mode->height;
		__v4l2_ctrl_modify_range(ctx->vblank, min, max, 1, def);
		__v4l2_ctrl_s_ctrl(ctx->vblank, def);
	}

	mutex_unlock(&ctx->mutex);

	return 0;
}

static int imgsensor_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct adaptor_ctx *ctx = to_ctx(sd);

	return adaptor_hw_power_on(ctx);
}

static int imgsensor_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct adaptor_ctx *ctx = to_ctx(sd);

	/* clear flags once power-off */
	ctx->is_sensor_init = 0;

	return adaptor_hw_power_off(ctx);
}

static int imgsensor_set_power(struct v4l2_subdev *sd, int on)
{
	struct adaptor_ctx *ctx = to_ctx(sd);
	int ret;

	mutex_lock(&ctx->mutex);
	if (on)
		ret = pm_runtime_get_sync(ctx->dev);
	else
		ret = pm_runtime_put(ctx->dev);
	mutex_unlock(&ctx->mutex);

	return ret;
}

/* Start streaming */
static int imgsensor_start_streaming(struct adaptor_ctx *ctx)
{
//	int ret;
	u64 data[4];
	u32 len;
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT image_window;
	MSDK_SENSOR_CONFIG_STRUCT sensor_config_data;

	if (!ctx->is_sensor_init) {
		subdrv_call(ctx, open);
		ctx->is_sensor_init = 1;
	}

	subdrv_call(ctx, control,
			ctx->cur_mode->id,
			&image_window,
			&sensor_config_data);

#ifdef APPLY_CUSTOMIZED_VALUES_FROM_USER
	/* Apply customized values from user */
	ret =  __v4l2_ctrl_handler_setup(ctx->sd.ctrl_handler);
	if (ret)
		dev_info(ctx->dev, "failed to apply customized values\n");
#endif

	data[0] = 0; // shutter
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_SET_STREAMING_RESUME,
		(u8 *)data, &len);

	return 0;
}

/* Stop streaming */
static int imgsensor_stop_streaming(struct adaptor_ctx *ctx)
{
	u64 data[4];
	u32 len;

	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_SET_STREAMING_SUSPEND,
		(u8 *)data, &len);

	return 0;
}

static int imgsensor_get_frame_interval(struct v4l2_subdev *sd,
		struct v4l2_subdev_frame_interval *fi)
{
	struct adaptor_ctx *ctx = to_ctx(sd);

	mutex_lock(&ctx->mutex);
	fi->interval.numerator = ctx->cur_mode->max_framerate;
	fi->interval.denominator = 10;
	mutex_unlock(&ctx->mutex);

	return 0;
}

static int imgsensor_set_stream(struct v4l2_subdev *sd, int enable)
{
	int ret;
	struct adaptor_ctx *ctx = to_ctx(sd);

	mutex_lock(&ctx->mutex);
	if (ctx->streaming == enable) {
		mutex_unlock(&ctx->mutex);
		return 0;
	}

	if (enable) {
		ret = pm_runtime_get_sync(ctx->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(ctx->dev);
			goto err_unlock;
		}

		/*
		 * Apply default & customized values
		 * and then start streaming.
		 */
		ret = imgsensor_start_streaming(ctx);
		if (ret)
			goto err_rpm_put;
	} else {
		imgsensor_stop_streaming(ctx);
		pm_runtime_put(ctx->dev);
	}

	ctx->streaming = enable;
	mutex_unlock(&ctx->mutex);

	dev_info(ctx->dev, "%s: en %d\n", __func__, enable);

	return 0;

err_rpm_put:
	pm_runtime_put(ctx->dev);
err_unlock:
	mutex_unlock(&ctx->mutex);

	return ret;
}

static int imgsensor_g_mbus_config(struct v4l2_subdev *sd,
		struct v4l2_mbus_config *cfg)
{
	struct adaptor_ctx *ctx = to_ctx(sd);

	cfg->type = ctx->sensor_info.MIPIsensorType == MIPI_CPHY ?
		V4L2_MBUS_CSI2_CPHY : V4L2_MBUS_CSI2_DPHY;

	switch (ctx->sensor_info.SensorMIPILaneNumber) {
	case SENSOR_MIPI_1_LANE:
		cfg->flags = V4L2_MBUS_CSI2_1_LANE;
		break;
	case SENSOR_MIPI_2_LANE:
		cfg->flags = V4L2_MBUS_CSI2_2_LANE;
		break;
	case SENSOR_MIPI_3_LANE:
		cfg->flags = V4L2_MBUS_CSI2_3_LANE;
		break;
	case SENSOR_MIPI_4_LANE:
		cfg->flags = V4L2_MBUS_CSI2_4_LANE;
		break;
	}

	return 0;
}

#ifdef IMGSENSOR_VC_ROUTING
static int imgsensor_get_frame_desc(struct v4l2_subdev *sd, unsigned int pad,
		struct v4l2_mbus_frame_desc *fd)
{
	int ret;
	struct adaptor_ctx *ctx = to_ctx(sd);

	ret = subdrv_call(ctx, get_frame_desc,
			ctx->cur_mode->id,
			fd);

	if (ret) {
		dev_warn(ctx->dev, "get_frame_desc ret %d (scenario_id %d)\n",
			ret, ctx->cur_mode->id);
	}

	return ret;
}

#define IS_3HDR(udesc) (udesc == V4L2_MBUS_CSI2_USER_DEFINED_DATA_DESC_Y_HIST \
		|| udesc == V4L2_MBUS_CSI2_USER_DEFINED_DATA_DESC_AE_HIST \
		|| udesc == V4L2_MBUS_CSI2_USER_DEFINED_DATA_DESC_FLICKER)

static int imgsensor_set_frame_desc(struct v4l2_subdev *sd, unsigned int pad,
		struct v4l2_mbus_frame_desc *fd)
{
	int i, b3HDR = 0;
	struct adaptor_ctx *ctx = to_ctx(sd);
	struct v4l2_mbus_frame_desc_entry_csi2 *desc;
	union feature_para para;
	u32 len;

	for (i = 0; i < fd->num_entries; i++) {
		desc = &fd->entry[i].bus.csi2;
		if (!desc->enable)
			continue;
		if (V4L2_MBUS_CSI2_IS_USER_DEFINED_DATA(desc->data_type)) {
			if (IS_3HDR(desc->user_data_desc))
				b3HDR = 1;
		}
	}

	if (b3HDR) {
		para.u32[0] = 1;
		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_SET_HDR,
			para.u8, &len);
	}

	return 0;
}
#endif

static int __maybe_unused imgsensor_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct adaptor_ctx *ctx = to_ctx(sd);

	if (pm_runtime_suspended(dev))
		return 0;

	if (ctx->streaming)
		imgsensor_stop_streaming(ctx);

	return imgsensor_runtime_suspend(dev);
}

static int __maybe_unused imgsensor_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct adaptor_ctx *ctx = to_ctx(sd);
	int ret;

	if (pm_runtime_suspended(dev))
		return 0;

	ret = imgsensor_runtime_resume(dev);
	if (ret)
		return ret;

	if (ctx->streaming) {
		ret = imgsensor_start_streaming(ctx);
		if (ret)
			goto error;
	}

	return 0;

error:
	imgsensor_stop_streaming(ctx);
	ctx->streaming = 0;
	return ret;
}

static const struct v4l2_subdev_core_ops imgsensor_core_ops = {
	.s_power = imgsensor_set_power,
	.ioctl = adaptor_ioctl,
};

static const struct v4l2_subdev_video_ops imgsensor_video_ops = {
	.g_frame_interval = imgsensor_get_frame_interval,
	.s_stream = imgsensor_set_stream,
	.g_mbus_config = imgsensor_g_mbus_config,
};

static const struct v4l2_subdev_pad_ops imgsensor_pad_ops = {
	.enum_mbus_code = imgsensor_enum_mbus_code,
	.get_fmt = imgsensor_get_pad_format,
	.set_fmt = imgsensor_set_pad_format,
	.enum_frame_size = imgsensor_enum_frame_size,
	.get_selection = imgsensor_get_selection,
#ifdef IMGSENSOR_VC_ROUTING
	.get_frame_desc = imgsensor_get_frame_desc,
	.set_frame_desc = imgsensor_set_frame_desc,
#endif
};

static const struct v4l2_subdev_ops imgsensor_subdev_ops = {
	.core = &imgsensor_core_ops,
	.video = &imgsensor_video_ops,
	.pad = &imgsensor_pad_ops,
};

static const struct v4l2_subdev_internal_ops imgsensor_internal_ops = {
	.open = imgsensor_open,
	.close = imgsensor_close,
};

static int imgsensor_get_temp(void *data, int *temperature)
{
	struct adaptor_ctx *ctx = data;

	if (pm_runtime_get_if_in_use(ctx->dev) == 0)
		return THERMAL_TEMP_INVALID;

	subdrv_call(ctx, get_temp, temperature);

	pm_runtime_put(ctx->dev);

	return 0;
}

static const struct thermal_zone_of_device_ops imgsensor_tz_ops = {
	.get_temp = imgsensor_get_temp,
};

static int notify_fsync_mgr(struct adaptor_ctx *ctx)
{
	int ret, seninf_idx = 0;
	const char *seninf_port = NULL;
	char c_ab;
	struct device_node *seninf_np;
	struct device *dev = ctx->dev;

	seninf_np = of_graph_get_remote_node(dev->of_node, 0, 0);
	if (!seninf_np) {
		dev_info(dev, "no remote device node\n");
		return -EINVAL;
	}

	ret = of_property_read_string(seninf_np, "csi-port", &seninf_port);

	of_node_put(seninf_np);

	if (ret || !seninf_port) {
		dev_info(dev, "no seninf csi-port\n");
		return -EINVAL;
	}

	/* convert seninf-port to seninf-idx */
	ret = sscanf(seninf_port, "%d%c", &seninf_idx, &c_ab);
	seninf_idx <<= 1;
	seninf_idx += (ret == 2 && (c_ab == 'b' || c_ab == 'B'));

	dev_info(dev, "sensor_idx %d seninf_port \'%s\' seninf_idx %d\n",
		ctx->idx, seninf_port, seninf_idx);

	/* notify frame-sync mgr of sensor-idx and seninf-idx */
	//TODO

	return 0;
}

static int imgsensor_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct device_node *endpoint;
	struct adaptor_ctx *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mutex_init(&ctx->mutex);

	ctx->i2c_client = client;
	ctx->dev = dev;

	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!endpoint) {
		dev_err(dev, "endpoint node not found\n");
		return -EINVAL;
	}

	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(endpoint),
					 &ctx->ep);

	of_node_put(endpoint);

	if (ret < 0) {
		dev_err(dev, "parsing endpoint node failed\n");
		return ret;
	}

	ret = adaptor_hw_init(ctx);
	if (ret) {
		dev_err(dev, "failed to init hw handles\n");
		return ret;
	}

	ret = search_sensor(ctx);
	if (ret) {
		dev_err(dev, "no sensor found\n");
		return ret;
	}

	/* read property */
	of_property_read_u32(dev->of_node, "location", &ctx->location);
	of_property_read_u32(dev->of_node, "rotation", &ctx->rotation);

	/* init sensor info */
	init_sensor_info(ctx);

	/* init subdev */
	v4l2_i2c_subdev_init(&ctx->sd, client, &imgsensor_subdev_ops);
	ctx->sd.internal_ops = &imgsensor_internal_ops;
	ctx->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ctx->pad.flags = MEDIA_PAD_FL_SOURCE;
	ctx->sd.dev = &client->dev;
	ctx->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	/* init subdev name */
	snprintf(ctx->sd.name, V4L2_SUBDEV_NAME_SIZE, "%s",
		dev->of_node->name);

	ret = sscanf(dev->of_node->name, OF_SENSOR_NAME_PREFIX"%d", &ctx->idx);
	if (ret != 1)
		dev_warn(dev, "failed to parse %s\n", dev->of_node->name);

	/* init controls */
	ret = adaptor_init_ctrls(ctx);
	if (ret) {
		dev_err(dev, "failed to init controls\n");
		return ret;
	}

	ret = media_entity_pads_init(&ctx->sd.entity, 1, &ctx->pad);
	if (ret < 0) {
		dev_err(dev, "failed to init entity pads: %d", ret);
		goto free_ctrl;
	}

	ret = v4l2_async_register_subdev(&ctx->sd);
	if (ret < 0) {
		dev_err(dev, "could not register v4l2 device\n");
		goto free_entity;
	}

	pm_runtime_enable(dev);

	/* register thermal device */
	if (ctx->subdrv->ops->get_temp) {
		struct thermal_zone_device *tzdev;

		tzdev = devm_thermal_zone_of_sensor_register(
			       dev, 0, ctx, &imgsensor_tz_ops);
		if (IS_ERR(tzdev))
			dev_info(dev, "failed to register thermal zone\n");
	}

	notify_fsync_mgr(ctx);

	return 0;

free_entity:
	media_entity_cleanup(&ctx->sd.entity);

free_ctrl:
	v4l2_ctrl_handler_free(&ctx->ctrls);
	mutex_destroy(&ctx->mutex);

	return ret;
}

static int imgsensor_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct adaptor_ctx *ctx = to_ctx(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);

	pm_runtime_disable(&client->dev);

	mutex_destroy(&ctx->mutex);

	return 0;
}

static const struct dev_pm_ops imgsensor_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(imgsensor_suspend, imgsensor_resume)
	SET_RUNTIME_PM_OPS(imgsensor_runtime_suspend,
			imgsensor_runtime_resume, NULL)
};

static const struct i2c_device_id imgsensor_id[] = {
	{"imgsensor", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, imgsensor_id);

static const struct of_device_id imgsensor_of_match[] = {
	{.compatible = "mediatek,imgsensor"},
	{}
};
MODULE_DEVICE_TABLE(of, imgsensor_of_match);

static struct i2c_driver imgsensor_i2c_driver = {
	.driver = {
		.of_match_table = of_match_ptr(imgsensor_of_match),
		.name = "imgsensor",
		.pm = &imgsensor_pm_ops,
	},
	.probe_new = imgsensor_probe,
	.remove = imgsensor_remove,
	.id_table = imgsensor_id,
};

static int __init adaptor_drv_init(void)
{
	i2c_add_driver(&imgsensor_i2c_driver);
	return 0;
}

static void __exit adaptor_drv_exit(void)
{
	i2c_del_driver(&imgsensor_i2c_driver);
}

late_initcall(adaptor_drv_init);
module_exit(adaptor_drv_exit);

MODULE_LICENSE("GPL v2");
