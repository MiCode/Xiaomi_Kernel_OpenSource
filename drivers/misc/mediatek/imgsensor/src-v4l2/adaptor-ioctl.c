// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2020 MediaTek Inc.

#include <linux/pm_runtime.h>

#include "kd_imgsensor_define.h"
#include "imgsensor-user.h"
#include "adaptor.h"
#include "adaptor-ioctl.h"
#include "adaptor-i2c.h"

#define GAIN_TBL_SIZE 4096
#define sd_to_ctx(__sd) container_of(__sd, struct adaptor_ctx, sd)

#define F_READ 1
#define F_ZERO 2
#define F_WRITE 4

struct workbuf {
	void *kbuf;
	void __user *ubuf;
	int size;
	int flags;
};

static inline struct sensor_mode *
find_sensor_mode_by_scenario(struct adaptor_ctx *ctx, int scenario_id)
{
	int i;

	for (i = 0; i < ctx->mode_cnt; i++) {
		if (ctx->mode[i].id == scenario_id)
			return &ctx->mode[i];
	}

	return NULL;
}

static int workbuf_get(struct workbuf *workbuf, void *ubuf, int size, int flags)
{
	void *kbuf;

	if (!ubuf || !size)
		return -EINVAL;

	kbuf = kmalloc(size, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	if (flags & F_READ) {
		if (copy_from_user(kbuf, ubuf, size)) {
			pr_err("copy from user fail\n");
			kfree(kbuf);
			return -EFAULT;
		}
	}

	else if (flags & F_ZERO) {
		memset(kbuf, 0, size);
	}

	workbuf->kbuf = kbuf;
	workbuf->ubuf = ubuf;
	workbuf->size = size;
	workbuf->flags = flags;

	return 0;
}

static int workbuf_put(struct workbuf *workbuf)
{
	if (workbuf->flags & F_WRITE) {
		if (copy_to_user(workbuf->ubuf, workbuf->kbuf, workbuf->size)) {
			kfree(workbuf->kbuf);
			return -EFAULT;
		}
	}

	kfree(workbuf->kbuf);

	return 0;
}

static void vcinfo_to_vcinfo2(
		struct SENSOR_VC_INFO_STRUCT *vcinfo,
		struct SENSOR_VC_INFO2_STRUCT *vcinfo2)
{
	struct SINGLE_VC_INFO2 *vc = vcinfo2->vc_info;

	vcinfo2->VC_Num = vcinfo->VC_Num;
	vcinfo2->VC_PixelNum = vcinfo->VC_PixelNum;
	vcinfo2->ModeSelect = vcinfo->ModeSelect;
	vcinfo2->EXPO_Ratio = vcinfo->EXPO_Ratio;
	vcinfo2->ODValue = vcinfo->ODValue;

	if (vcinfo->VC4_DataType && vcinfo->VC4_SIZEH && vcinfo->VC4_SIZEV) {
		vc[0].VC_DataType = vcinfo->VC0_DataType;
		vc[0].VC_ID = vcinfo->VC0_ID;
		vc[0].VC_FEATURE = VC_RAW_DATA;
		vc[0].VC_SIZEH_PIXEL = vcinfo->VC0_SIZEH;
		vc[0].VC_SIZEV = vcinfo->VC0_SIZEV;

		vc[1].VC_DataType = vcinfo->VC1_DataType;
		vc[1].VC_ID = vcinfo->VC1_ID;
		vc[1].VC_FEATURE = VC_3HDR_EMBEDDED;
		vc[1].VC_SIZEH_PIXEL = vcinfo->VC1_SIZEH;
		vc[1].VC_SIZEV = vcinfo->VC1_SIZEV;
		vc[1].VC_SIZEH_BYTE = vcinfo->VC1_DataType != 0x2b ?
			vcinfo->VC1_SIZEH : vcinfo->VC1_SIZEH * 10 / 8;

		vc[2].VC_DataType = vcinfo->VC2_DataType;
		vc[2].VC_ID = vcinfo->VC2_ID;
		vc[2].VC_FEATURE = VC_3HDR_Y;
		vc[2].VC_SIZEH_PIXEL = vcinfo->VC2_SIZEH;
		vc[2].VC_SIZEV = vcinfo->VC2_SIZEV;
		vc[2].VC_SIZEH_BYTE = vcinfo->VC2_DataType != 0x2b ?
			vcinfo->VC2_SIZEH : vcinfo->VC2_SIZEH * 10 / 8;

		vc[3].VC_DataType = vcinfo->VC3_DataType;
		vc[3].VC_ID = vcinfo->VC3_ID;
		vc[3].VC_FEATURE = VC_3HDR_AE;
		vc[3].VC_SIZEH_PIXEL = vcinfo->VC3_SIZEH;
		vc[3].VC_SIZEV = vcinfo->VC3_SIZEV;
		vc[3].VC_SIZEH_BYTE = vcinfo->VC3_DataType != 0x2b ?
			vcinfo->VC3_SIZEH : vcinfo->VC3_SIZEH * 10 / 8;

		vc[4].VC_DataType = vcinfo->VC4_DataType;
		vc[4].VC_ID = vcinfo->VC4_ID;
		vc[4].VC_FEATURE = VC_3HDR_FLICKER;
		vc[4].VC_SIZEH_PIXEL = vcinfo->VC4_SIZEH;
		vc[4].VC_SIZEV = vcinfo->VC4_SIZEV;
		vc[4].VC_SIZEH_BYTE = vcinfo->VC4_DataType != 0x2b ?
			vcinfo->VC4_SIZEH : vcinfo->VC4_SIZEH * 10 / 8;

	} else {
		vc[0].VC_DataType = vcinfo->VC0_DataType;
		vc[0].VC_ID = vcinfo->VC0_ID;
		vc[0].VC_FEATURE = VC_RAW_DATA;
		vc[0].VC_SIZEH_PIXEL = vcinfo->VC0_SIZEH;
		vc[0].VC_SIZEV = vcinfo->VC0_SIZEV;

		vc[1].VC_DataType = vcinfo->VC1_DataType;
		vc[1].VC_ID = vcinfo->VC1_ID;
		vc[1].VC_FEATURE = VC_HDR_MVHDR;
		vc[1].VC_SIZEH_PIXEL = vcinfo->VC1_SIZEH;
		vc[1].VC_SIZEV = vcinfo->VC1_SIZEV;
		vc[1].VC_SIZEH_BYTE = vcinfo->VC1_DataType != 0x2b ?
			vcinfo->VC1_SIZEH : vcinfo->VC1_SIZEH * 10 / 8;

		vc[2].VC_DataType = vcinfo->VC2_DataType;
		vc[2].VC_ID = vcinfo->VC2_ID;
		vc[2].VC_FEATURE = VC_PDAF_STATS;
		vc[2].VC_SIZEH_PIXEL = vcinfo->VC2_SIZEH;
		vc[2].VC_SIZEV = vcinfo->VC2_SIZEV;
		vc[2].VC_SIZEH_BYTE = vcinfo->VC2_DataType != 0x2b ?
			vcinfo->VC2_SIZEH : vcinfo->VC2_SIZEH * 10 / 8;
	}
}

static int g_def_fps_by_scenario(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_fps_by_scenario *info = arg;
	union feature_para para;
	u32 len;

	para.u64[0] = info->scenario_id;
	para.u64[1] = (u64)&info->fps;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO,
		para.u8, &len);

	return 0;
}

static int g_pclk_by_scenario(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_pclk_by_scenario *info = arg;
	union feature_para para;
	u32 len;

	para.u64[0] = info->scenario_id;
	para.u64[1] = (u64)&info->pclk;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO,
		para.u8, &len);

	return 0;
}

static int g_llp_fll_by_scenario(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_llp_fll_by_scenario *info = arg;
	union feature_para para;
	u32 len, tmp = 0;

	para.u64[0] = info->scenario_id;
	para.u64[1] = (u64)&tmp;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO,
		para.u8, &len);

	info->llp = tmp & 0xffff;
	info->fll = tmp >> 16;

	return 0;
}

static int g_gain_range_by_scenario(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_gain_range_by_scenario *info = arg;
	union feature_para para;
	u32 len;

	para.u64[0] = info->scenario_id;
	para.u64[1] = 0;
	para.u64[2] = 0;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO,
		para.u8, &len);

	info->min_gain = para.u64[1];
	info->max_gain = para.u64[2];

	return 0;
}

static int g_min_shutter_by_scenario(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_min_shutter_by_scenario *info = arg;
	union feature_para para;
	u32 len;

	para.u64[0] = info->scenario_id;
	para.u64[1] = 0;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO,
		para.u8, &len);

	info->min_shutter = para.u64[1];

	return 0;
}

static int g_crop_by_scenario(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_crop_by_scenario *info = arg;
	union feature_para para;
	u32 len;
	struct SENSOR_WINSIZE_INFO_STRUCT winsize;

	para.u64[0] = info->scenario_id;
	para.u64[1] = (u64)&winsize;

	memset(&winsize, 0, sizeof(winsize));

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_CROP_INFO,
		para.u8, &len);

	if (copy_to_user((void *)info->p_winsize, &winsize, sizeof(winsize)))
		return -EFAULT;

	return 0;
}

static int g_vcinfo_by_scenario(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_vcinfo_by_scenario *info = arg;
	union feature_para para;
	u32 len;
	struct SENSOR_VC_INFO2_STRUCT vcinfo2;

	para.u64[0] = info->scenario_id;
	para.u64[1] = (u64)&vcinfo2;

	memset(&vcinfo2, 0, sizeof(vcinfo2));

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_VC_INFO2,
		para.u8, &len);

	if (!vcinfo2.updated) {
		struct SENSOR_VC_INFO_STRUCT vcinfo;

		para.u64[1] = (u64)&vcinfo;
		memset(&vcinfo, 0, sizeof(vcinfo));
		ctx->sensor->ops->SensorFeatureControl(
			SENSOR_FEATURE_GET_VC_INFO,
			para.u8, &len);
		vcinfo_to_vcinfo2(&vcinfo, &vcinfo2);
	}

	if (copy_to_user((void *)info->p_vcinfo, &vcinfo2, sizeof(vcinfo2)))
		return -EFAULT;

	return 0;
}

static int g_pdaf_info_by_scenario(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_pdaf_info_by_scenario *info = arg;
	union feature_para para;
	u32 len;
	struct SET_PD_BLOCK_INFO_T pd;

	para.u64[0] = info->scenario_id;
	para.u64[1] = (u64)&pd;

	memset(&pd, 0, sizeof(pd));

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_PDAF_INFO,
		para.u8, &len);

	if (copy_to_user((void *)info->p_pd, &pd, sizeof(pd)))
		return -EFAULT;

	return 0;
}

static int g_llp_fll(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_llp_fll *info = arg;
	union feature_para para;
	u32 len;

	para.u16[0] = 0;
	para.u16[1] = 0;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_PERIOD,
		para.u8, &len);

	info->llp = para.u16[0];
	info->fll = para.u16[1];

	return 0;
}

static int g_pclk(struct adaptor_ctx *ctx, void *arg)
{
	u32 *info = arg;
	union feature_para para;
	u32 len;

	para.u32[0] = 0;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ,
		para.u8, &len);

	*info = para.u32[0];

	return 0;
}

static int g_binning_type(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_binning_type *info = arg;
	union feature_para para;
	u32 len;

	para.u64[0] = 0;
	para.u64[1] = info->scenario_id;
	para.u64[2] = info->HDRMode;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_BINNING_TYPE,
		para.u8, &len);

	info->binning_type = para.u32[0];

	return 0;
}

static int g_test_pattern_checksum(struct adaptor_ctx *ctx, void *arg)
{
	u32 *info = arg;
	union feature_para para;
	u32 len;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE,
		para.u8, &len);

	*info = para.u32[0];

	return 0;
}

static int g_base_gain_iso_n_step(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_base_gain_iso_n_step *info = arg;
	union feature_para para;
	u32 len;

	para.u64[0] =
	para.u64[1] =
	para.u64[2] = 0;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP,
		para.u8, &len);

	info->min_gain_iso = para.u64[0];
	info->gain_step = para.u64[1];
	info->gain_type = para.u64[2];

	return 0;
}

static int g_offset_to_start_of_exposure(struct adaptor_ctx *ctx, void *arg)
{
	u32 *info = arg;
	union feature_para para;
	u32 len, tmp = 0;

	para.u64[0] = 0;
	para.u64[1] = (u64)&tmp;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE,
		para.u8, &len);

	*info = tmp;

	return 0;
}

static int g_ana_gain_table_size(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_ana_gain_table *info = arg;
	union feature_para para;
	u32 len;

	para.u64[0] = 0;
	para.u64[1] = 0;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_ANA_GAIN_TABLE,
		para.u8, &len);

	info->size = para.u64[0];

	return 0;
}

static int g_ana_gain_table(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_ana_gain_table *info = arg;
	union feature_para para;
	u32 len;
	struct workbuf workbuf;
	int ret;

	if (!info->size || !info->p_buf)
		return -EINVAL;

	if (info->size > GAIN_TBL_SIZE)
		return -E2BIG;

	ret = workbuf_get(&workbuf, info->p_buf, info->size, F_ZERO | F_WRITE);
	if (ret)
		return ret;

	para.u64[0] = info->size;
	para.u64[1] = (u64)workbuf.kbuf;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_ANA_GAIN_TABLE,
		para.u8, &len);

	ret = workbuf_put(&workbuf);
	if (ret)
		return ret;

	return 0;
}

static int g_pdaf_data(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_pdaf_data *info = arg;
	union feature_para para;
	u32 len;
	struct workbuf workbuf;
	int ret;

	ret = workbuf_get(&workbuf, info->p_buf, info->size, F_ZERO | F_WRITE);
	if (ret)
		return ret;

	para.u64[0] = info->offset;
	para.u64[1] = (u64)workbuf.kbuf;
	para.u64[2] = info->size;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_PDAF_DATA,
		para.u8, &len);

	ret = workbuf_put(&workbuf);
	if (ret)
		return ret;

	return 0;
}

static int g_pdaf_cap(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_cap *info = arg;
	union feature_para para;
	u32 len, tmp = 0;

	para.u64[0] = info->scenario_id;
	para.u64[1] = (u64)&tmp;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY,
		para.u8, &len);

	info->cap = tmp;

	return 0;
}

static int g_pdaf_regs(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_regs *info = arg;
	struct workbuf workbuf;
	int ret;

	ret = workbuf_get(&workbuf, info->p_buf, info->size, F_READ | F_WRITE);
	if (ret)
		return ret;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_PDAF_REG_SETTING,
		workbuf.kbuf, &info->size);

	ret = workbuf_put(&workbuf);
	if (ret)
		return ret;

	return 0;
}

static int g_mipi_pixel_rate(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_mipi_pixel_rate *info = arg;
	union feature_para para;
	u32 len, tmp = 0;

	para.u64[0] = info->scenario_id;
	para.u64[1] = (u64)&tmp;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_MIPI_PIXEL_RATE,
		para.u8, &len);

	info->mipi_pixel_rate = tmp;

	return 0;
}

static int g_hdr_cap(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_cap *info = arg;
	union feature_para para;
	u32 len, tmp = 0;

	para.u64[0] = info->scenario_id;
	para.u64[1] = (u64)&tmp;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY,
		para.u8, &len);

	info->cap = tmp;

	return 0;
}

static int g_delay_info(struct adaptor_ctx *ctx, void *arg)
{
	struct SENSOR_DELAY_INFO_STRUCT *info = arg;
	union feature_para para;
	u32 len;

	para.u64[0] = (u64)info;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_DELAY_INFO,
		para.u8, &len);

	return 0;
}

static int g_resolution(struct adaptor_ctx *ctx, void *arg)
{
	struct ACDK_SENSOR_RESOLUTION_INFO_STRUCT *info = arg;
	struct workbuf workbuf;
	int ret;

	ret = workbuf_get(&workbuf, info,
			sizeof(*info), F_WRITE);
	if (ret)
		return ret;

	ctx->sensor->ops->SensorGetResolution(workbuf.kbuf);

	ret = workbuf_put(&workbuf);
	if (ret)
		return ret;

	return 0;
}

static int g_feature_info(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_feature_info *info = arg;
	struct workbuf workbuf1, workbuf2;
	int ret;

	ret = workbuf_get(&workbuf1, info->p_info,
			sizeof(*info->p_info),
			F_ZERO | F_WRITE);
	if (ret)
		return ret;

	ret = workbuf_get(&workbuf2, info->p_config,
			sizeof(*info->p_config),
			F_ZERO | F_WRITE);
	if (ret) {
		workbuf_put(&workbuf1);
		return ret;
	}

	ctx->sensor->ops->SensorGetInfo(
		info->scenario_id, workbuf1.kbuf, workbuf2.kbuf);

	ret = workbuf_put(&workbuf1);

	ret = workbuf_put(&workbuf2);

	if (ret)
		return ret;

	if (info->p_resolution)
		ret = g_resolution(ctx, info->p_resolution);

	return ret;
}

static int g_4cell_data(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_4cell_data *info = arg;
	struct workbuf workbuf;
	union feature_para para;
	u32 len;
	int ret;

	ret = workbuf_get(&workbuf, info->p_buf, info->size, F_ZERO | F_WRITE);
	if (ret)
		return ret;

	para.u64[0] = info->type;
	para.u64[1] = (u64)workbuf.kbuf;
	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_4CELL_DATA,
		para.u8, &len);

	ret = workbuf_put(&workbuf);
	if (ret)
		return ret;

	return 0;
}

static int g_ae_frame_mode_for_le(struct adaptor_ctx *ctx, void *arg)
{
	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE,
		0, arg);

	return 0;
}

static int g_ae_effective_frame_for_le(struct adaptor_ctx *ctx, void *arg)
{
	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE,
		0, arg);

	return 0;
}


static int g_scenario_combo_info(struct adaptor_ctx *ctx, void *arg)
{
	int ret;
	struct mtk_scenario_combo_info *info = arg;

	if (info->p_timing) {
		struct sensor_mode *mode;
		struct mtk_scenario_timing timing;

		mode = find_sensor_mode_by_scenario(ctx, info->scenario_id);
		if (!mode)
			return -EINVAL;
		timing.llp = mode->llp;
		timing.fll = mode->fll;
		timing.width = mode->width;
		timing.height = mode->height;
		timing.mipi_pixel_rate = mode->mipi_pixel_rate;
		timing.max_framerate = mode->max_framerate;
		timing.pclk = mode->pclk;
		timing.linetime_in_ns = mode->linetime_in_ns;
		if (copy_to_user(info->p_timing, &timing, sizeof(timing)))
			return -EFAULT;
	}

	if (info->p_vcinfo) {
		struct mtk_vcinfo_by_scenario vcinfo = {
			.scenario_id = info->scenario_id,
			.p_vcinfo = info->p_vcinfo,
		};
		ret = g_vcinfo_by_scenario(ctx, &vcinfo);
		if (ret)
			return ret;
	}

	if (info->p_winsize) {
		struct mtk_crop_by_scenario crop = {
			.scenario_id = info->scenario_id,
			.p_winsize = info->p_winsize,
		};
		ret = g_crop_by_scenario(ctx, &crop);
		if (ret)
			return ret;
	}

	if (info->p_pd) {
		struct mtk_pdaf_info_by_scenario pdaf = {
			.scenario_id = info->scenario_id,
			.p_pd = info->p_pd,
		};
		ret = g_pdaf_info_by_scenario(ctx, &pdaf);
		if (ret)
			return ret;
	}

	return 0;
}

static int g_sensor_info(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_sensor_info *info = arg;

	snprintf(info->name, sizeof(info->name), "%s", ctx->sensor->name);
	info->id = ctx->sensor->id;

	/* read property */
	of_property_read_u32(ctx->dev->of_node, "dir", &info->dir);
	of_property_read_u32(ctx->dev->of_node, "orientation", &info->orientation);
	of_property_read_u32(ctx->dev->of_node, "h_fov", &info->horizontalFov);
	of_property_read_u32(ctx->dev->of_node, "v_fov", &info->verticalFov);

	return 0;
}

static int s_video_framerate(struct adaptor_ctx *ctx, void *arg)
{
	u32 *info = arg;
	union feature_para para;
	u32 len;

	para.u64[0] = *info;
	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_SET_VIDEO_MODE,
		para.u8, &len);

	return 0;
}

static int s_max_fps_by_scenario(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_fps_by_scenario *info = arg;
	union feature_para para;
	u32 len;

	para.u64[0] = info->scenario_id;
	para.u64[1] = info->fps;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO,
		para.u8, &len);

	return 0;
}

static int s_framerate(struct adaptor_ctx *ctx, void *arg)
{
	u32 *info = arg;
	union feature_para para;
	u32 len;

	para.u32[0] = *info;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_SET_FRAMERATE,
		para.u8, &len);

	return 0;
}

static int s_hdr(struct adaptor_ctx *ctx, void *arg)
{
	int *info = arg;
	union feature_para para;
	u32 len;

	para.u32[0] = *info;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_SET_HDR,
		para.u8, &len);

	return 0;
}

static int s_pdaf_regs(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_regs *info = arg;
	struct workbuf workbuf;
	int ret;

	ret = workbuf_get(&workbuf, info->p_buf, info->size, F_READ);
	if (ret)
		return ret;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_SET_PDAF_REG_SETTING,
		workbuf.kbuf, &info->size);

	ret = workbuf_put(&workbuf);
	if (ret)
		return ret;

	return 0;
}

static int s_pdaf(struct adaptor_ctx *ctx, void *arg)
{
	int *info = arg;
	union feature_para para;
	u32 len;

	para.u32[0] = *info;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_SET_PDAF,
		para.u8, &len);

	return 0;
}

static int s_min_max_fps(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_min_max_fps *info = arg;
	union feature_para para;
	u32 len;

	para.u64[0] = info->min_fps;
	para.u64[1] = info->max_fps;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_SET_MIN_MAX_FPS,
		para.u8, &len);

	return 0;
}

static int s_lsc_tbl(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_lsc_tbl *info = arg;
	struct workbuf workbuf;
	int ret;

	ret = workbuf_get(&workbuf, info->p_buf, info->size + 1, F_READ);
	if (ret)
		return ret;

	/* store index in last byte */
	*(((u8 *)workbuf.kbuf) + info->size) = info->index;

	ctx->sensor->ops->SensorFeatureControl(
		SENSOR_FEATURE_SET_LSC_TBL,
		workbuf.kbuf, &info->size);

	ret = workbuf_put(&workbuf);
	if (ret)
		return ret;

	return 0;
}

static int s_control(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_sensor_control *info = arg;
	struct workbuf workbuf1, workbuf2;
	int ret;

	ret = workbuf_get(&workbuf1, info->p_window,
		sizeof(*info->p_window), F_READ);
	if (ret)
		return ret;

	ret = workbuf_get(&workbuf2, info->p_config,
		sizeof(*info->p_config), F_READ);
	if (ret)
		return ret;

	ctx->sensor->ops->SensorControl(
		info->scenario_id, workbuf1.kbuf, workbuf2.kbuf);

	ret = workbuf_put(&workbuf1);
	ret = workbuf_put(&workbuf2);
	if (ret)
		return ret;

	return 0;
}

static int s_tg(struct adaptor_ctx *ctx, void *arg)
{
	int *p_tg = arg;

	dev_info(ctx->dev, "notify frame-sync module of TG\n", *p_tg);

	return 0;
}

struct ioctl_entry {
	unsigned int cmd;
	int (*func)(struct adaptor_ctx *ctx, void *arg);
};

static const struct ioctl_entry ioctl_list[] = {
	/* GET */
	{VIDIOC_MTK_G_DEF_FPS_BY_SCENARIO, g_def_fps_by_scenario},
	{VIDIOC_MTK_G_PCLK_BY_SCENARIO, g_pclk_by_scenario},
	{VIDIOC_MTK_G_LLP_FLL_BY_SCENARIO, g_llp_fll_by_scenario},
	{VIDIOC_MTK_G_GAIN_RANGE_BY_SCENARIO, g_gain_range_by_scenario},
	{VIDIOC_MTK_G_MIN_SHUTTER_BY_SCENARIO, g_min_shutter_by_scenario},
	{VIDIOC_MTK_G_CROP_BY_SCENARIO, g_crop_by_scenario},
	{VIDIOC_MTK_G_VCINFO_BY_SCENARIO, g_vcinfo_by_scenario},
	{VIDIOC_MTK_G_PDAF_INFO_BY_SCENARIO, g_pdaf_info_by_scenario},
	{VIDIOC_MTK_G_LLP_FLL, g_llp_fll},
	{VIDIOC_MTK_G_PCLK, g_pclk},
	{VIDIOC_MTK_G_BINNING_TYPE, g_binning_type},
	{VIDIOC_MTK_G_TEST_PATTERN_CHECKSUM, g_test_pattern_checksum},
	{VIDIOC_MTK_G_BASE_GAIN_ISO_N_STEP, g_base_gain_iso_n_step},
	{VIDIOC_MTK_G_OFFSET_TO_START_OF_EXPOSURE, g_offset_to_start_of_exposure},
	{VIDIOC_MTK_G_ANA_GAIN_TABLE_SIZE, g_ana_gain_table_size},
	{VIDIOC_MTK_G_ANA_GAIN_TABLE, g_ana_gain_table},
	{VIDIOC_MTK_G_PDAF_DATA, g_pdaf_data},
	{VIDIOC_MTK_G_PDAF_CAP, g_pdaf_cap},
	{VIDIOC_MTK_G_PDAF_REGS, g_pdaf_regs},
	{VIDIOC_MTK_G_MIPI_PIXEL_RATE, g_mipi_pixel_rate},
	{VIDIOC_MTK_G_HDR_CAP, g_hdr_cap},
	{VIDIOC_MTK_G_DELAY_INFO, g_delay_info},
	{VIDIOC_MTK_G_FEATURE_INFO, g_feature_info},
	{VIDIOC_MTK_G_4CELL_DATA, g_4cell_data},
	{VIDIOC_MTK_G_AE_FRAME_MODE_FOR_LE, g_ae_frame_mode_for_le},
	{VIDIOC_MTK_G_AE_EFFECTIVE_FRAME_FOR_LE, g_ae_effective_frame_for_le},
	{VIDIOC_MTK_G_SCENARIO_COMBO_INFO, g_scenario_combo_info},
	{VIDIOC_MTK_G_SENSOR_INFO, g_sensor_info},
	/* SET */
	{VIDIOC_MTK_S_VIDEO_FRAMERATE, s_video_framerate},
	{VIDIOC_MTK_S_MAX_FPS_BY_SCENARIO, s_max_fps_by_scenario},
	{VIDIOC_MTK_S_FRAMERATE, s_framerate},
	{VIDIOC_MTK_S_HDR, s_hdr},
	{VIDIOC_MTK_S_PDAF_REGS, s_pdaf_regs},
	{VIDIOC_MTK_S_PDAF, s_pdaf},
	{VIDIOC_MTK_S_MIN_MAX_FPS, s_min_max_fps},
	{VIDIOC_MTK_S_LSC_TBL, s_lsc_tbl},
	{VIDIOC_MTK_S_CONTROL, s_control},
	{VIDIOC_MTK_S_TG, s_tg},
};

long adaptor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int i, ret = -ENOIOCTLCMD;
	struct adaptor_ctx *ctx = sd_to_ctx(sd);

	/* dispatch ioctl request */
	for (i = 0; i < ARRAY_SIZE(ioctl_list); i++) {
		if (ioctl_list[i].cmd == cmd) {
			adaptor_legacy_lock();
			adaptor_legacy_set_i2c_client(ctx->i2c_client);
			ret = ioctl_list[i].func(ctx, arg);
			adaptor_legacy_unlock();
			break;
		}
	}

	return ret;
}

