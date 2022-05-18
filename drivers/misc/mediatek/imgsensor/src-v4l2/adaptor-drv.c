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

#include "kd_imgsensor_define_v4l2.h"

#include "adaptor.h"
#include "adaptor-hw.h"
#include "adaptor-i2c.h"
#include "adaptor-ctrls.h"
#include "adaptor-ioctl.h"
#include "imgsensor-glue/imgsensor-glue.h"
#include "virt-sensor/virt-sensor-entry.h"

#undef E
#define E(__x__) (__x__##_entry)
#define EXTERN_IMGSENSOR_SUBDRVS extern struct subdrv_entry \
	IMGSENSOR_SUBDRVS
#ifdef IMGSENSOR_SUBDRVS
EXTERN_IMGSENSOR_SUBDRVS;
#endif

#undef E
#define E(__x__) (&__x__##_entry)
static struct subdrv_entry *imgsensor_subdrvs[] = {
#ifdef IMGSENSOR_SUBDRVS
	IMGSENSOR_SUBDRVS
#endif
};

module_param(sensor_debug, uint, 0644);
MODULE_PARM_DESC(sensor_debug, "imgsensor_debug");

static int get_outfmt_code(struct adaptor_ctx *ctx)
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

	case SENSOR_OUTPUT_FORMAT_RAW8_MONO:
	case SENSOR_OUTPUT_FORMAT_RAW8_B:
		return MEDIA_BUS_FMT_SBGGR8_1X8;
	case SENSOR_OUTPUT_FORMAT_RAW8_Gb:
		return MEDIA_BUS_FMT_SGBRG8_1X8;
	case SENSOR_OUTPUT_FORMAT_RAW8_Gr:
		return MEDIA_BUS_FMT_SGRBG8_1X8;
	case SENSOR_OUTPUT_FORMAT_RAW8_R:
		return MEDIA_BUS_FMT_SRGGB8_1X8;

	case SENSOR_OUTPUT_FORMAT_RAW12_B:
		return MEDIA_BUS_FMT_SBGGR12_1X12;
	case SENSOR_OUTPUT_FORMAT_RAW12_Gb:
		return MEDIA_BUS_FMT_SGBRG12_1X12;
	case SENSOR_OUTPUT_FORMAT_RAW12_Gr:
		return MEDIA_BUS_FMT_SGRBG12_1X12;
	case SENSOR_OUTPUT_FORMAT_RAW12_R:
		return MEDIA_BUS_FMT_SRGGB12_1X12;
	}

	pr_warn("unknown output format %d\n", outfmt);

	return MEDIA_BUS_FMT_SBGGR10_1X10;
}

static void add_sensor_mode(struct adaptor_ctx *ctx,
		int id, int width, int height)
{
	union feature_para para;
	u32 idx, val, len, llp_readout;
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

	para.u64[2] = SENSOR_GET_LINELENGTH_FOR_READOUT;
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO,
		para.u8, &len);
	llp_readout = val & 0xffff;
	para.u64[2] = 0;

	val = 0;
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO,
		para.u8, &len);

	mode->llp = val & 0xffff;
	mode->fll = val >> 16;

	if (!mode->llp || !mode->fll)
		return;

	val = 0;
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_MIPI_PIXEL_RATE,
		para.u8, &len);

	mode->mipi_pixel_rate = val;

	val = 0;
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_CUST_PIXEL_RATE,
		para.u8, &len);

	mode->cust_pixel_rate = val;

	val = 0;
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO,
		para.u8, &len);

	mode->max_framerate = val;

	val = 0;
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO,
		para.u8, &len);

	mode->pclk = val;

	val = 0;

	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_FINE_INTEG_LINE_BY_SCENARIO,
		para.u8, &len);
	mode->fine_intg_line = val;

	val = 0;
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_ESD_RESET_BY_USER,
		para.u8, &len);

	mode->esd_reset_by_user = val;


	if (!mode->mipi_pixel_rate || !mode->max_framerate || !mode->pclk)
		return;

	subdrv_call(ctx, get_csi_param, mode->id, &mode->csi_param);

	/* update linetime_in_ns */
	mode->linetime_in_ns = (u64)mode->llp * 1000000 +
		(mode->pclk / 1000 - 1);
	do_div(mode->linetime_in_ns, mode->pclk / 1000);


	mode->linetime_in_ns_readout = (u64)llp_readout * 1000000 +
		(mode->pclk / 1000 - 1);
	do_div(mode->linetime_in_ns_readout, mode->pclk / 1000);


	dev_dbg(ctx->dev, "%s [%d] id %d %dx%d %dx%d px %d fps %d tLine %lld|%lld fintl %u\n",
		__func__,
		idx, id, width, height,
		mode->llp, mode->fll,
		mode->mipi_pixel_rate, mode->max_framerate,
		mode->linetime_in_ns,
		mode->linetime_in_ns_readout,
		mode->fine_intg_line);

	ctx->mode_cnt++;
}

static int init_sensor_mode(struct adaptor_ctx *ctx)
{
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT res;
	unsigned int i = 0;

	memset(&res, 0, sizeof(res));
	subdrv_call(ctx, get_resolution, &res);
	for (i = SENSOR_SCENARIO_ID_MIN; i < SENSOR_SCENARIO_ID_MAX; i++) {
		ctx->seamless_scenarios[i] = SENSOR_SCENARIO_ID_NONE;

		if (res.SensorWidth[i] > 0 && res.SensorHeight[i] > 0)
			add_sensor_mode(ctx,
			i,
			res.SensorWidth[i],
			res.SensorHeight[i]);
	}
	return 0;
}

static void control_sensor(struct adaptor_ctx *ctx)
{
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT image_window;
	MSDK_SENSOR_CONFIG_STRUCT sensor_config_data;

	if (!ctx->is_sensor_scenario_inited && !ctx->is_streaming) {
		subdrv_call(ctx, control,
				ctx->cur_mode->id,
				&image_window,
				&sensor_config_data);
		ctx->is_sensor_scenario_inited = 1;
	}
	restore_ae_ctrl(ctx);
}

static int set_sensor_mode(struct adaptor_ctx *ctx,
		struct sensor_mode *mode, char update_ctrl_defs)
{
	s64 min, max, def;

	if (ctx->cur_mode == mode) {
		if (update_ctrl_defs)
			control_sensor(ctx);
		return 0;
	}

	ctx->cur_mode = mode;
	ctx->is_sensor_scenario_inited = 0;

	subdrv_call(ctx, get_info,
			mode->id,
			&ctx->sensor_info,
			&ctx->sensor_cfg);

	if (update_ctrl_defs) {
		/* pixel rate */
		min = max = def = mode->mipi_pixel_rate;
		__v4l2_ctrl_modify_range(ctx->pixel_rate, min, max, 1, def);

		/* hblank */
		min = max = def = mode->llp - mode->width;
		__v4l2_ctrl_modify_range(ctx->hblank, min, max, 1, def);

		/* vblank */
		min = def = mode->fll - mode->height;
		max = ctx->subctx.max_frame_length - mode->height;
		__v4l2_ctrl_modify_range(ctx->vblank, min, max, 1, def);

		/* max fps */
		max = def = mode->max_framerate;
		__v4l2_ctrl_modify_range(ctx->max_fps, 1, max, 1, def);

		/* init sensor scenario setting */
		control_sensor(ctx);
	}

	dev_dbg(ctx->dev, "select %dx%d@%d %dx%d px %d\n",
		mode->width, mode->height, mode->max_framerate,
		mode->llp, mode->fll, mode->mipi_pixel_rate);

	return 0;
}

static int init_sensor_info(struct adaptor_ctx *ctx)
{
	init_sensor_mode(ctx);
	set_sensor_mode(ctx, &ctx->mode[0], 0);
	ctx->try_format_mode = &ctx->mode[0];
	ctx->fmt_code = get_outfmt_code(ctx);
	return 0;
}

static int search_sensor(struct adaptor_ctx *ctx)
{
	int ret, i, j, of_sensor_names_cnt, subdrvs_cnt;
	struct subdrv_entry **subdrvs, *subdrv;
	struct subdrv_entry *of_subdrvs[OF_SENSOR_NAMES_MAXCNT];
	const char *of_sensor_names[OF_SENSOR_NAMES_MAXCNT];
	int subdrv_name_ret;
	const char *of_subdrv_name;

	of_sensor_names_cnt = of_property_read_string_array(ctx->dev->of_node,
		"sensor-names", of_sensor_names, ARRAY_SIZE(of_sensor_names));

	subdrv_name_ret = of_property_read_string(ctx->dev->of_node,
						  "subdrv-name", &of_subdrv_name);

	dev_info(ctx->dev, "subdrv_name_ret %d\n", subdrv_name_ret);

	if (!subdrv_name_ret)
		dev_info(ctx->dev, "subdrv name %s found\n", of_subdrv_name);

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
	} else if (!subdrv_name_ret) {
		subdrvs_cnt = 0;
		of_subdrvs[0] = vs_query_subdrv_entry(of_subdrv_name);

		if (of_subdrvs[0]) {
			subdrvs_cnt = 1;
			subdrvs = of_subdrvs;
			dev_info(ctx->dev, "subdrv name %s found\n", of_subdrv_name);
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

	ctx->open_refcnt++;

	/* Initialize try_fmt */
	try_fmt->width = ctx->cur_mode->width;
	try_fmt->height = ctx->cur_mode->height;
	try_fmt->code = ctx->fmt_code;
	try_fmt->field = V4L2_FIELD_NONE;

#ifdef POWERON_ONCE_OPENED
#ifdef IMGSENSOR_USE_PM_FRAMEWORK
	dev_info(ctx->dev, "%s use IMGSENSOR_USE_PM_FRAMEWORK\n", __func__);
	pm_runtime_get_sync(ctx->dev);
#else
	dev_info(ctx->dev, "%s use self ref cnt\n", __func__);
	adaptor_hw_power_on(ctx);
#endif

	adaptor_sensor_init(ctx);
#endif

	mutex_unlock(&ctx->mutex);

	return 0;
}

static int imgsensor_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct adaptor_ctx *ctx = to_ctx(sd);
	int i;

	mutex_lock(&ctx->mutex);

#ifdef POWERON_ONCE_OPENED
#ifdef IMGSENSOR_USE_PM_FRAMEWORK
	dev_info(ctx->dev, "%s use IMGSENSOR_USE_PM_FRAMEWORK\n", __func__);
	pm_runtime_put(ctx->dev);
#else
	dev_info(ctx->dev, "%s use self ref cnt\n", __func__);
	adaptor_hw_power_off(ctx);
#endif
#endif

	ctx->open_refcnt--;
	if (ctx->open_refcnt <= 0) {
		for (i = 0; ctx->power_refcnt; i++)
			adaptor_hw_power_off(ctx);
		ctx->open_refcnt = 0;
	}

	mutex_unlock(&ctx->mutex);

	return 0;
}

static int imgsensor_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	struct adaptor_ctx *ctx = to_ctx(sd);

	if (code->index >= ctx->mode_cnt)
		return -EINVAL;

	code->code = to_mtk_ext_fmt_code(ctx->fmt_code, ctx->mode[code->index].id);

	return 0;
}

static int imgsensor_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	struct adaptor_ctx *ctx = to_ctx(sd);

	if (fse->index >= ctx->mode_cnt)
		return -EINVAL;

	fse->min_width = ctx->mode[fse->index].width;
	fse->max_width = fse->min_width;
	fse->min_height = ctx->mode[fse->index].height;
	fse->max_height = fse->min_height;

	return 0;
}

static int imgsensor_enum_frame_interval(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_frame_interval_enum *fie)
{
	struct adaptor_ctx *ctx = to_ctx(sd);
	struct sensor_mode *mode;
	u32 i, index = fie->index;

	for (i = 0; i < ctx->mode_cnt; i++) {
		mode = &ctx->mode[i];
		if (fie->width != mode->width ||
			fie->height != mode->height)
			continue;
		if (index-- == 0) {
			fie->interval.numerator = 10;
			fie->interval.denominator = mode->max_framerate;
			return 0;
		}
	}

	return -EINVAL;
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
	fmt->format.code = to_mtk_ext_fmt_code(ctx->fmt_code, mode->id);
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
	int sensor_mode_id = 0;

	mutex_lock(&ctx->mutex);

	/* Only one raw bayer order is supported */
	set_std_parts_fmt_code(fmt->format.code, ctx->fmt_code);


	/* Returns the best match or NULL if the Length of the array is zero */
	mode = v4l2_find_nearest_size(ctx->mode,
		ctx->mode_cnt, width, height,
		fmt->format.width, fmt->format.height);

	/* Override mode when fmt code specified sensor mode */
	if (is_mtk_ext_fmt_code(fmt->format.code)) {
		sensor_mode_id = get_sensor_mode_from_fmt_code(fmt->format.code);
		if (sensor_mode_id >= 0 && sensor_mode_id < ctx->mode_cnt)
			mode = &ctx->mode[sensor_mode_id];
	}

	if (mode == NULL) {
		dev_info(ctx->dev,
			"set fmt code = 0x%x, which %d ctx->mode_cnt = %d\n",
			fmt->format.code, fmt->which, ctx->mode_cnt);

		mutex_unlock(&ctx->mutex);
		return -EINVAL;
	}

	dev_info(ctx->dev,
		"set fmt code = 0x%x, which %d sensor_mode_id = %u\n",
		fmt->format.code, fmt->which, mode->id);


	update_pad_format(ctx, mode, fmt);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		framefmt = v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
		*framefmt = fmt->format;

		ctx->try_format_mode = mode;
	} else {
#ifndef POWERON_ONCE_OPENED
		adaptor_sensor_init(ctx);
#endif
		set_sensor_mode(ctx, mode, 1);
	}
	mutex_unlock(&ctx->mutex);

	return 0;
}

static u32 get_sensor_sync_mode(struct adaptor_ctx *ctx)
{
	u32 sync_mode = 0;
	union feature_para para;
	u32 len;

	para.u32[0] = 0;

	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_SENSOR_SYNC_MODE,
		para.u8, &len);

	sync_mode = para.u32[0];

	dev_info(ctx->dev, "sync mode = %u\n", sync_mode);

	return sync_mode;
}

/* notify frame-sync streaming ON/OFF and set sensor info to frame-sync */
static void notify_fsync_mgr_streaming(struct adaptor_ctx *ctx,
				unsigned int flag)
{
	unsigned int ret = 0;
	struct fs_streaming_st streaming_sensor_info = {0};

	streaming_sensor_info.sensor_id = ctx->subdrv->id;
	streaming_sensor_info.sensor_idx = ctx->idx;

	/* fsync_map_id is cam_mux no */
	streaming_sensor_info.tg = ctx->fsync_map_id->val + 1;
	streaming_sensor_info.fl_active_delay =
				ctx->subctx.frame_time_delay_frame;


	/* using ctx->subctx.frame_length instead of ctx->cur_mode->fll */
	/*     for any settings before streaming on */
	streaming_sensor_info.def_fl_lc = ctx->subctx.frame_length;
	streaming_sensor_info.max_fl_lc = ctx->subctx.max_frame_length;

	/* frame sync sensor operate mode. none/master/slave */
	streaming_sensor_info.sync_mode = get_sensor_sync_mode(ctx);


	/* using ctx->subctx.shutter instead of ctx->subctx.exposure_def */
	/*     for any settings before streaming on */
	streaming_sensor_info.def_shutter_lc = ctx->subctx.shutter;


	/* callback data */
	streaming_sensor_info.func_ptr = cb_fsync_mgr_set_framelength;
	streaming_sensor_info.p_ctx = ctx;


	/* call frame-sync streaming ON/OFF */
	if (ctx->fsync_mgr != NULL) {
		ret = ctx->fsync_mgr->fs_streaming(flag, &streaming_sensor_info);

		if (ret != 0)
			dev_info(ctx->dev, "frame-sync streaming ERROR!\n");
	} else
		dev_info(ctx->dev, "frame-sync is not init!\n");
}

#ifdef IMGSENSOR_USE_PM_FRAMEWORK
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
	ctx->is_sensor_inited = 0;
	ctx->is_sensor_scenario_inited = 0;

	return adaptor_hw_power_off(ctx);
}
#endif

static int imgsensor_set_power(struct v4l2_subdev *sd, int on)
{
	struct adaptor_ctx *ctx = to_ctx(sd);
	int ret;

	mutex_lock(&ctx->mutex);
	if (on)
#ifdef IMGSENSOR_USE_PM_FRAMEWORK
		ret = pm_runtime_get_sync(ctx->dev);
#else
	ret = adaptor_hw_power_on(ctx);
#endif
	else
#ifdef IMGSENSOR_USE_PM_FRAMEWORK
		ret = pm_runtime_put(ctx->dev);
#else
	ret = adaptor_hw_power_off(ctx);
#endif
	mutex_unlock(&ctx->mutex);

	return ret;
}

/* Start streaming */
static int imgsensor_start_streaming(struct adaptor_ctx *ctx)
{
//	int ret;
	u64 data[4];
	u32 len;
	union feature_para para;

	adaptor_sensor_init(ctx);

	control_sensor(ctx);

#ifdef APPLY_CUSTOMIZED_VALUES_FROM_USER
	/* Apply customized values from user */
	ret =  __v4l2_ctrl_handler_setup(ctx->sd.ctrl_handler);
	if (ret)
		dev_info(ctx->dev, "failed to apply customized values\n");
#endif

	data[0] = 0; // shutter
	para.u8[0] = 0;
	//Make sure close test pattern
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_SET_TEST_PATTERN,
		para.u8, &len);
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_SET_STREAMING_RESUME,
		(u8 *)data, &len);

	/* notify frame-sync streaming ON */
	notify_fsync_mgr_streaming(ctx, 1);

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

	/* notify frame-sync streaming OFF */
	notify_fsync_mgr_streaming(ctx, 0);

	memset(&ctx->ae_memento, 0, sizeof(ctx->ae_memento));

	return 0;
}

static int imgsensor_get_frame_interval(struct v4l2_subdev *sd,
		struct v4l2_subdev_frame_interval *fi)
{
	struct adaptor_ctx *ctx = to_ctx(sd);

	mutex_lock(&ctx->mutex);
	fi->interval.numerator = 10;

	if (fi->reserved[0] == V4L2_SUBDEV_FORMAT_TRY)
		fi->interval.denominator = ctx->try_format_mode->max_framerate;
	else
		fi->interval.denominator = ctx->cur_mode->max_framerate;

	mutex_unlock(&ctx->mutex);

	return 0;
}

static int imgsensor_set_frame_interval(struct v4l2_subdev *sd,
		struct v4l2_subdev_frame_interval *fi)
{
	struct adaptor_ctx *ctx = to_ctx(sd);

	dev_info(ctx->dev, "set_frame_interval = %u\n", fi->interval.denominator);

	return imgsensor_get_frame_interval(sd, fi);
}

static int imgsensor_set_stream(struct v4l2_subdev *sd, int enable)
{
	int ret;
	struct adaptor_ctx *ctx = to_ctx(sd);

	mutex_lock(&ctx->mutex);
	if (ctx->is_streaming == enable) {
		mutex_unlock(&ctx->mutex);
		return 0;
	}

	if (enable) {
#ifdef IMGSENSOR_USE_PM_FRAMEWORK
		ret = pm_runtime_get_sync(ctx->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(ctx->dev);
			goto err_unlock;
		}
#endif

		/*
		 * Apply default & customized values
		 * and then start streaming.
		 */
		ret = imgsensor_start_streaming(ctx);
		if (ret)
			goto err_rpm_put;
	} else {
		imgsensor_stop_streaming(ctx);
#ifdef IMGSENSOR_USE_PM_FRAMEWORK
		pm_runtime_put(ctx->dev);
#endif
	}

	ctx->is_streaming = enable;
	mutex_unlock(&ctx->mutex);

	dev_info(ctx->dev, "%s: en %d, cammux usage %d\n",
		__func__, enable, ctx->fsync_map_id->val);

	return 0;

err_rpm_put:
#ifdef IMGSENSOR_USE_PM_FRAMEWORK
	pm_runtime_put(ctx->dev);
err_unlock:
#endif
	mutex_unlock(&ctx->mutex);

	return ret;
}

static int imgsensor_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad,
		struct v4l2_mbus_config *config)
{
	struct adaptor_ctx *ctx = to_ctx(sd);

	config->type = ctx->sensor_info.MIPIsensorType == MIPI_CPHY ?
		V4L2_MBUS_CSI2_CPHY : V4L2_MBUS_CSI2_DPHY;

	switch (ctx->sensor_info.SensorMIPILaneNumber) {
	case SENSOR_MIPI_1_LANE:
		config->flags = V4L2_MBUS_CSI2_1_LANE;
		break;
	case SENSOR_MIPI_2_LANE:
		config->flags = V4L2_MBUS_CSI2_2_LANE;
		break;
	case SENSOR_MIPI_3_LANE:
		config->flags = V4L2_MBUS_CSI2_3_LANE;
		break;
	case SENSOR_MIPI_4_LANE:
		config->flags = V4L2_MBUS_CSI2_4_LANE;
		break;
	}

	return 0;
}

//def IMGSENSOR_VC_ROUTING
// static int imgsensor_get_frame_desc(struct v4l2_subdev *sd, unsigned int pad,
		// struct mtk_mbus_frame_desc *fd)
// {
	// int ret;
	// struct adaptor_ctx *ctx = to_ctx(sd);
	// u64 desc_visited = 0x0;
	// int write_to = 0, i = -1, j = 0;

	// while (i < SENSOR_SCENARIO_ID_MAX) {
		// struct mtk_mbus_frame_desc fd_tmp;
		// u32 scenario_id = (-1 == i) ? ctx->cur_mode->id : ctx->seamless_scenarios[i];

		// if (scenario_id == SENSOR_SCENARIO_ID_NONE)
			// break;

		// ret = subdrv_call(ctx, get_frame_desc, scenario_id, &fd_tmp);

		// if (!ret) {
			// for (j = 0;
				// write_to < V4L2_FRAME_DESC_ENTRY_MAX && j < fd_tmp.num_entries;
				// ++j) {
				// if (desc_visited &
					// (0x1 << fd_tmp.entry[j].bus.csi2.user_data_desc))
					// continue;

				// dev_info(ctx->dev, "[%s] scenario %u desc %d\n", __func__,
						// scenario_id,
						// fd_tmp.entry[j].bus.csi2.user_data_desc);
				// memcpy(&fd->entry[write_to++], &fd_tmp.entry[j],
					//    sizeof(struct mtk_mbus_frame_desc_entry));

				// desc_visited |= (0x1 << fd_tmp.entry[j].bus.csi2.user_data_desc);
			// }
		// }

		// ++i;
	// }

	// fd->num_entries = write_to;
	// fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;

	// return ret;
// }

// #define IS_3HDR(udesc) (udesc == VC_3HDR_Y \
		// || udesc == VC_3HDR_AE \
		// || udesc == VC_3HDR_FLICKER)

// static int imgsensor_set_frame_desc(struct v4l2_subdev *sd, unsigned int pad,
		// struct mtk_mbus_frame_desc *fd)
// {
	// return 0;

// }


#ifdef IMGSENSOR_USE_PM_FRAMEWORK
static int __maybe_unused imgsensor_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct adaptor_ctx *ctx = to_ctx(sd);

	if (pm_runtime_suspended(dev))
		return 0;

	if (ctx->is_streaming)
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

	if (ctx->is_streaming) {
		ret = imgsensor_start_streaming(ctx);
		if (ret)
			goto error;
	}

	return 0;

error:
	imgsensor_stop_streaming(ctx);
	ctx->is_streaming = 0;
	return ret;
}
#endif

static const struct v4l2_subdev_core_ops imgsensor_core_ops = {
	.s_power = imgsensor_set_power,
	.ioctl = adaptor_ioctl,
};

static const struct v4l2_subdev_video_ops imgsensor_video_ops = {
	.g_frame_interval = imgsensor_get_frame_interval,
	.s_frame_interval = imgsensor_set_frame_interval,
	.s_stream = imgsensor_set_stream,
};

static const struct v4l2_subdev_pad_ops imgsensor_pad_ops = {
	.enum_mbus_code = imgsensor_enum_mbus_code,
	.get_fmt = imgsensor_get_pad_format,
	.set_fmt = imgsensor_set_pad_format,
	.enum_frame_size = imgsensor_enum_frame_size,
	.enum_frame_interval = imgsensor_enum_frame_interval,
	.get_selection = imgsensor_get_selection,
	.get_mbus_config = imgsensor_g_mbus_config,
#ifdef IMGSENSOR_VC_ROUTING
	//.get_frame_desc = imgsensor_get_frame_desc,
	//.set_frame_desc = imgsensor_set_frame_desc,
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

#ifdef IMGSENSOR_USE_PM_FRAMEWORK
	if (pm_runtime_get_if_in_use(ctx->dev) == 0) {
		*temperature = THERMAL_TEMP_INVALID;
		return 0;
	}
#else
	*temperature = 0;
#endif
	if (ctx->is_streaming)
		subdrv_call(ctx, get_temp, temperature);
	else
		*temperature = THERMAL_TEMP_INVALID;

#ifdef IMGSENSOR_USE_PM_FRAMEWORK
	pm_runtime_put(ctx->dev);
#endif

	return 0;
}

static const struct thermal_zone_of_device_ops imgsensor_tz_ops = {
	.get_temp = imgsensor_get_temp,
};

static int notify_fsync_mgr(struct adaptor_ctx *ctx, int on)
{
	int ret, seninf_idx = 0;
	const char *seninf_port = NULL;
	char c_ab;
	struct device_node *seninf_np;
	struct device *dev = ctx->dev;

	if (!on) {
		/* imgsensor remove => for remove sysfs file */
		FrameSyncUnInit(ctx->dev);
		return 0;
	}

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

	dev_info(dev, "sensor_idx %d seninf_port %s seninf_idx %d\n",
		ctx->idx, seninf_port, seninf_idx);

	/* frame-sync init */
	ret = FrameSyncInit(&ctx->fsync_mgr, ctx->dev);
	if (ret != 0) {
		dev_info(ctx->dev, "frame-sync init failed !\n");
		ctx->fsync_mgr = NULL;
	} else
		dev_info(dev, "frame-sync init done, ret:%d", ret);

	/* notify frame-sync mgr of sensor-idx and seninf-idx */
	//TODO

	return 0;
}


#define SHOW(buf, len, fmt, ...) { \
	len += snprintf(buf + len, PAGE_SIZE - len, fmt, ##__VA_ARGS__); \
}

static ssize_t debug_i2c_ops_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct adaptor_ctx *ctx = to_ctx(dev_get_drvdata(dev));


	SHOW(buf, len, "%s i2c read 0x%08x = 0x%08x\n",
			ctx->subdrv->name,
			ctx->sensorReg.RegAddr,
			ctx->sensorReg.RegData);
	return len;
}

enum DBG_ARG_IDX {
	DBG_ARG_IDX_I2C_ADDR,
	DBG_ARG_IDX_I2C_DATA,
	DBG_ARG_IDX_MAX_NUM,
};

static ssize_t debug_i2c_ops_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	char delim[] = " ";
	char *token = NULL;
	char *sbuf = kzalloc(sizeof(char) * (count + 1), GFP_KERNEL);
	char *s = sbuf;
	int ret;
	unsigned int num_para = 0;
	char *arg[DBG_ARG_IDX_MAX_NUM];
	struct adaptor_ctx *ctx = to_ctx(dev_get_drvdata(dev));
	u32 val;
	u32 reg;

	ctx->sensorReg.RegAddr = 0;
	ctx->sensorReg.RegData = 0;

	if (!sbuf)
		goto ERR_DEBUG_OPS_STORE;

	memcpy(sbuf, buf, count);

	token = strsep(&s, delim);
	while (token != NULL && num_para < DBG_ARG_IDX_MAX_NUM) {
		if (strlen(token)) {
			arg[num_para] = token;
			num_para++;
		}

		token = strsep(&s, delim);
	}

	if (num_para > DBG_ARG_IDX_MAX_NUM) {
		dev_info(dev, "Wrong command parameter number %u\n", num_para);
		goto ERR_DEBUG_OPS_STORE;
	}
	ret = kstrtouint(arg[DBG_ARG_IDX_I2C_ADDR], 0, &reg);
	if (ret)
		goto ERR_DEBUG_OPS_STORE;
	ctx->sensorReg.RegAddr = reg;

	if (num_para == DBG_ARG_IDX_MAX_NUM) {
		ret = kstrtouint(arg[DBG_ARG_IDX_I2C_DATA], 0, &val);
		if (ret)
			goto ERR_DEBUG_OPS_STORE;
		ctx->sensorReg.RegData = val;

		ret = subdrv_call(ctx, feature_control, SENSOR_FEATURE_SET_REGISTER,
					(MUINT8 *) &ctx->sensorReg,
					(MUINT32 *) sizeof(MSDK_SENSOR_REG_INFO_STRUCT));
		dev_info(dev, "%s i2c write 0x%08x = 0x%08x ret = %d\n",
			__func__,
			ctx->sensorReg.RegAddr, ctx->sensorReg.RegData, ret);
	}

	ret = subdrv_call(ctx, feature_control, SENSOR_FEATURE_GET_REGISTER,
				(MUINT8 *) &ctx->sensorReg,
				(MUINT32 *) sizeof(MSDK_SENSOR_REG_INFO_STRUCT));
		dev_info(dev, "%s i2c read 0x%08x = 0x%08x  ret = %d\n",
			__func__,
			ctx->sensorReg.RegAddr, ctx->sensorReg.RegData, ret);


ERR_DEBUG_OPS_STORE:

	kfree(sbuf);
	dev_dbg(dev, "exit %s\n", __func__);

	return count;
}


static DEVICE_ATTR_RW(debug_i2c_ops);

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
	ctx->open_refcnt = 0;
	ctx->power_refcnt = 0;

	ctx->i2c_client = client;
	ctx->dev = dev;
	ctx->sensor_debug_flag = &sensor_debug;

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

#ifdef IMGSENSOR_USE_PM_FRAMEWORK
	pm_runtime_enable(dev);
#else
	// TODO
#endif

	/* register thermal device */
	if (ctx->subdrv->ops->get_temp) {
		struct thermal_zone_device *tzdev;

		tzdev = devm_thermal_zone_of_sensor_register(
			       dev, 0, ctx, &imgsensor_tz_ops);
		if (IS_ERR(tzdev))
			dev_info(dev, "failed to register thermal zone\n");
	}

	notify_fsync_mgr(ctx, 1);

	ret = device_create_file(dev, &dev_attr_debug_i2c_ops);
	if (ret)
		dev_info(dev, "failed to create sysfs debug_i2c_ops\n");

	ctx->sensor_ws = wakeup_source_register(dev, ctx->sd.name);

	if (!ctx->sensor_ws)
		dev_info(dev, "failed to wakeup_source_register\n");

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

	notify_fsync_mgr(ctx, 0);

	if (!ctx->sensor_ws)
		wakeup_source_unregister(ctx->sensor_ws);
	ctx->sensor_ws = NULL;

#ifdef IMGSENSOR_USE_PM_FRAMEWORK
	pm_runtime_disable(&client->dev);
#else
	// TODO
#endif
	device_remove_file(ctx->dev, &dev_attr_debug_i2c_ops);

	mutex_destroy(&ctx->mutex);

	return 0;
}

#ifdef IMGSENSOR_USE_PM_FRAMEWORK
static const struct dev_pm_ops imgsensor_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(imgsensor_suspend, imgsensor_resume)
	SET_RUNTIME_PM_OPS(imgsensor_runtime_suspend,
			imgsensor_runtime_resume, NULL)
};
#endif

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
#ifdef IMGSENSOR_USE_PM_FRAMEWORK
		.pm = &imgsensor_pm_ops,
#endif
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
