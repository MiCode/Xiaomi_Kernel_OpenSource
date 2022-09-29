// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2020 MediaTek Inc.

#include <linux/pm_runtime.h>
#include <linux/thermal.h>

#include "kd_imgsensor_define_v4l2.h"
#include "adaptor.h"
#include "adaptor-i2c.h"
#include "adaptor-ctrls.h"
#include "adaptor-common-ctrl.h"
#include "adaptor-fsync-ctrls.h"
#include "adaptor-hw.h"
#include "adaptor-trace.h"

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

static void dump_perframe_info(struct adaptor_ctx *ctx, struct mtk_hdr_ae *ae_ctrl)
{
	dev_info(ctx->dev,
		"[%s][%s] sensor_idx %d, req id %d, sof_cnt:%u, exposure[LLLE->SSSE] %d %d %d %d %d ana_gain[LLLE->SSSE] %d %d %d %d %d, w(%d/%d/%d/%d/%d,%d/%d/%d/%d/%d) sub_tag:%u, fl:%u, min_fl:%u, flick_en:%u, mode:(line_time:%u, margin:%u, scen:%u; STG:(readout_l:%u, read_margin:%u, ext_fl:%u, fast_mode:%u))\n",
		ctx->sd.name,
		(ctx->subdrv) ? (ctx->subdrv->name) : "null",
		ctx->idx,
		ae_ctrl->req_id,
		ctx->sof_cnt,
		ae_ctrl->exposure.le_exposure,
		ae_ctrl->exposure.me_exposure,
		ae_ctrl->exposure.se_exposure,
		ae_ctrl->exposure.sse_exposure,
		ae_ctrl->exposure.ssse_exposure,
		ae_ctrl->gain.le_gain,
		ae_ctrl->gain.me_gain,
		ae_ctrl->gain.se_gain,
		ae_ctrl->gain.sse_gain,
		ae_ctrl->gain.ssse_gain,
		ae_ctrl->w_exposure.le_exposure,
		ae_ctrl->w_exposure.me_exposure,
		ae_ctrl->w_exposure.se_exposure,
		ae_ctrl->w_exposure.sse_exposure,
		ae_ctrl->w_exposure.ssse_exposure,
		ae_ctrl->w_gain.le_gain,
		ae_ctrl->w_gain.me_gain,
		ae_ctrl->w_gain.se_gain,
		ae_ctrl->w_gain.sse_gain,
		ae_ctrl->w_gain.ssse_gain,
		ae_ctrl->subsample_tags,
		ctx->subctx.frame_length,
		ctx->subctx.min_frame_length,
		ctx->subctx.autoflicker_en,
		CALC_LINE_TIME_IN_NS(ctx->subctx.pclk, ctx->subctx.line_length),
		ctx->subctx.margin,
		ctx->subctx.current_scenario_id,
		ctx->subctx.readout_length,
		ctx->subctx.read_margin,
		ctx->subctx.extend_frame_length_en,
		ctx->subctx.fast_mode_on);
}

static int set_hdr_exposure_tri(struct adaptor_ctx *ctx, struct mtk_hdr_exposure *info)
{
	union feature_para para;
	u32 len = 0;
	int ret = 0;

	para.u64[0] = info->le_exposure;
	para.u64[1] = info->me_exposure;
	para.u64[2] = info->se_exposure;

	ret = chk_s_exp_with_fl_by_fsync_mgr(ctx, info->arr, 3);
	if (!ret) {
		/* NOT enable frame-sync || using HW sync solution */
		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_SET_HDR_TRI_SHUTTER,
			para.u8, &len);
	}
	notify_fsync_mgr_set_shutter(ctx, info->arr, 3, ret);

	return 0;
}

static void get_dispatch_gain(struct adaptor_ctx *ctx, u32 tgain, u32 *again, u32 *dgain)
{
	int i;
	u32 ag = tgain;
	u32 dg = 0;
	u32 dig_gain_step = ctx->subctx.s_ctx.dig_gain_step;
	u32 *ana_gain_table = ctx->subctx.s_ctx.ana_gain_table;
	u32 ana_gain_table_size = ctx->subctx.s_ctx.ana_gain_table_size;
	u32 ana_gain_table_cnt = 0;

	if (dig_gain_step && ana_gain_table && (tgain > ana_gain_table[0])) {
		ana_gain_table_cnt = (ana_gain_table_size / sizeof(ana_gain_table[0]));
		for (i = 1; i < ana_gain_table_cnt; i++) {
			if (ana_gain_table[i] > tgain) {
				ag = ana_gain_table[i - 1];
				dg = (u32) ((u64)tgain * BASE_DGAIN / ag);
				break;
			}
		}
		if (i == ana_gain_table_cnt) {
			ag = ana_gain_table[i - 1];
			dg = (u32) ((u64)tgain * BASE_DGAIN / ag);
		}
	}

	if (again)
		*again = ag;
	if (dgain)
		*dgain = dg;

#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "again tlb cnt = %u sz(%u), gain(t/a/d) = %u / %u / %u\n",
		 ana_gain_table_cnt, ana_gain_table_size, tgain, ag, dg);
#endif
}

static int set_hdr_gain_tri(struct adaptor_ctx *ctx, struct mtk_hdr_gain *info)
{
	union feature_para para;
	u32 len = 0;
	u32 again_exp[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u32 dgain_exp[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};

	get_dispatch_gain(ctx, info->le_gain, again_exp, dgain_exp);
	get_dispatch_gain(ctx, info->me_gain, again_exp + 1, dgain_exp + 1);
	get_dispatch_gain(ctx, info->se_gain, again_exp + 2, dgain_exp + 2);

	// Set dig gain
	para.u64[0] = (u64)dgain_exp;
	para.u64[1] = 3;
	para.u64[2] = 0;
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_SET_MULTI_DIG_GAIN,
		para.u8, &len);

	// Set ana gain
	para.u64[0] = again_exp[0];
	para.u64[1] = again_exp[1];
	para.u64[2] = again_exp[2];
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_SET_HDR_TRI_GAIN,
		para.u8, &len);

	return 0;
}

static int set_hdr_exposure_dual(struct adaptor_ctx *ctx, struct mtk_hdr_exposure *info)
{
	union feature_para para;
	u32 len = 0;
	int ret = 0;

	para.u64[0] = info->le_exposure;
	para.u64[1] = info->me_exposure; // temporailly workaround, 2 exp should be NE/SE

	ret = chk_s_exp_with_fl_by_fsync_mgr(ctx, info->arr, 2);
	if (!ret) {
		/* NOT enable frame-sync || using HW sync solution */
		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_SET_HDR_SHUTTER,
			para.u8, &len);
	}
	notify_fsync_mgr_set_shutter(ctx, info->arr, 2, ret);

	return 0;
}

static int set_hdr_gain_dual(struct adaptor_ctx *ctx, struct mtk_hdr_gain *info)
{
	union feature_para para;
	u32 len = 0;
	u32 again_exp[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u32 dgain_exp[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};

	get_dispatch_gain(ctx, info->le_gain, again_exp, dgain_exp);
	// temporailly workaround, 2 exp should be NE/SE
	get_dispatch_gain(ctx, info->me_gain, again_exp + 1, dgain_exp + 1);

	// Set dig gain
	para.u64[0] = (u64)dgain_exp;
	para.u64[1] = 2;
	para.u64[2] = 0;
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_SET_MULTI_DIG_GAIN,
		para.u8, &len);

	// Set ana gain
	para.u64[0] = again_exp[0];
	para.u64[1] = again_exp[1];
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_SET_DUAL_GAIN,
		para.u8, &len);

	return 0;
}

static int do_set_ae_ctrl(struct adaptor_ctx *ctx,
						  struct mtk_hdr_ae *ae_ctrl)
{
	union feature_para para;
	u32 len = 0, exp_count = 0;
	struct mtk_stagger_info info = {0};
	int ret = 0;

#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s]+\n", __func__);
#endif

	info.scenario_id = SENSOR_SCENARIO_ID_NONE;

	/* update ctx req id */
	ctx->req_id = ae_ctrl->req_id;

	ctx->subctx.ae_ctrl_gph_en = 1;
	while (exp_count < IMGSENSOR_STAGGER_EXPOSURE_CNT &&
		ae_ctrl->exposure.arr[exp_count] != 0)
		exp_count++;

	/* get exp_cnt */
	ret = g_stagger_info(ctx, ctx->cur_mode->id, &info);
	if (!ret) {
		/* non-stagger mode, the info count would be 0, it's same as 1 */
		if (info.count == 0)
			info.count = 1;
		if (info.count != exp_count) {
			dev_info(ctx->dev, "warn: scenario_exp_cnt=%u, but ae_exp_count=%u\n",
				 info.count, exp_count);
			exp_count = info.count;
		}
	}
	switch (exp_count) {
	case 3:
	{
		ADAPTOR_SYSTRACE_BEGIN("imgsensor::set_exposure_tri");
		set_hdr_exposure_tri(ctx, &ae_ctrl->exposure);
		ADAPTOR_SYSTRACE_END();

		ADAPTOR_SYSTRACE_BEGIN("imgsensor::set_gain_tri");
		set_hdr_gain_tri(ctx, &ae_ctrl->gain);
		ADAPTOR_SYSTRACE_END();
	}
		break;
	case 2:
	{
		ADAPTOR_SYSTRACE_BEGIN("imgsensor::set_exposure_dual");
		set_hdr_exposure_dual(ctx, &ae_ctrl->exposure);
		ADAPTOR_SYSTRACE_END();

		ADAPTOR_SYSTRACE_BEGIN("imgsensor::set_gain_dual");
		set_hdr_gain_dual(ctx, &ae_ctrl->gain);
		ADAPTOR_SYSTRACE_END();
	}
		break;
	case 1:
	default:
	{
		u32 fsync_exp[1] = {0}; /* needed by fsync set_shutter */
		int ret = 0;
		u32 again_exp[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
		u32 dgain_exp[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};

		/* notify subsample tags if set */
		if (ae_ctrl->subsample_tags) {
			notify_fsync_mgr_subsample_tag(ctx,
						ae_ctrl->subsample_tags);
		}

		ADAPTOR_SYSTRACE_BEGIN("imgsensor::set_exposure");
		fsync_exp[0] = ae_ctrl->exposure.le_exposure;
		ret = chk_s_exp_with_fl_by_fsync_mgr(ctx, fsync_exp, 1);
		if (!ret) {
			/* NOT enable frame-sync || using HW sync solution */
			para.u64[0] = ae_ctrl->exposure.le_exposure;
			subdrv_call(ctx, feature_control,
						SENSOR_FEATURE_SET_ESHUTTER,
						para.u8, &len);
		}
		notify_fsync_mgr_set_shutter(ctx, fsync_exp, 1, ret);
		ADAPTOR_SYSTRACE_END();

		get_dispatch_gain(ctx, ae_ctrl->gain.le_gain, again_exp, dgain_exp);

		// Set dig gain
		para.u64[0] = (u64)dgain_exp;
		para.u64[1] = 1;
		para.u64[2] = 0;
		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_SET_MULTI_DIG_GAIN,
			para.u8, &len);

		// Set ana gain
		para.u64[0] = again_exp[0];
		para.u64[1] = 0;
		para.u64[2] = 0;

		ADAPTOR_SYSTRACE_BEGIN("imgsensor::set_gain");
		subdrv_call(ctx, feature_control,
					SENSOR_FEATURE_SET_GAIN,
					para.u8, &len);

		ADAPTOR_SYSTRACE_END();
	}
		break;
	}

	if (ae_ctrl->actions & IMGSENSOR_EXTEND_FRAME_LENGTH_TO_DOL) {
		para.u64[0] = 0;
		ADAPTOR_SYSTRACE_BEGIN("imgsensor::set_extend_frame_length");
		subdrv_call(ctx, feature_control,
					SENSOR_FEATURE_SET_SEAMLESS_EXTEND_FRAME_LENGTH,
					para.u8, &len);
		ADAPTOR_SYSTRACE_END();

		notify_fsync_mgr_set_extend_framelength(ctx, para.u64[0]);
	}

	ctx->exposure->val = ae_ctrl->exposure.le_exposure;
	ctx->analogue_gain->val = ae_ctrl->gain.le_gain;
	ctx->subctx.ae_ctrl_gph_en = 0;
	dump_perframe_info(ctx, ae_ctrl);
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s]-\n", __func__);
#endif
	return 0;
}

static int s_ae_ctrl(struct v4l2_ctrl *ctrl)
{
	struct adaptor_ctx *ctx = ctrl_to_ctx(ctrl);
	struct mtk_hdr_ae *ae_ctrl = ctrl->p_new.p;

	memcpy(&ctx->ae_memento, ae_ctrl,
		   sizeof(ctx->ae_memento));
	if (!ctx->is_streaming) {
		/* update timeout value upon streaming off */
		ctx->shutter_for_timeout = ctx->ae_memento.exposure.le_exposure;
		if (ctx->cur_mode->fine_intg_line)
			ctx->shutter_for_timeout /= 1000;
		dev_info(ctx->dev, "%s streaming off, set restore ae_ctrl later\n", __func__);
		return 0;
	}

	return do_set_ae_ctrl(ctx, ae_ctrl);
}

static int _sensor_reset_s_stream(struct v4l2_ctrl *ctrl)
{
	struct adaptor_ctx *ctx = ctrl_to_ctx(ctrl);
	u64 data[4];
	u32 len;

	//dev_info(ctx->dev, "%s val: %d, stream_off_state: %d\n",
	//	 __func__, ctrl->val,
	//	 ctx->is_sensor_reset_stream_off);

	if (ctrl->val && ctx->is_sensor_reset_stream_off) {
		subdrv_call(ctx, feature_control,
			    SENSOR_FEATURE_SET_STREAMING_RESUME,
			    (u8 *)data, &len);
		ctx->is_sensor_reset_stream_off = 0;
	} else if (!ctrl->val) {
		ctx->is_sensor_reset_stream_off = 1;
		subdrv_call(ctx, feature_control,
			    SENSOR_FEATURE_SET_STREAMING_SUSPEND,
			    (u8 *)data, &len);
	}

	return 0;
}

static int g_volatile_temperature(struct adaptor_ctx *ctx,
		struct v4l2_ctrl *ctrl)
{
	union feature_para para;
	u32 len = 0;

	if (ctx->is_streaming) {
		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_GET_TEMPERATURE_VALUE,
			para.u8, &len);

		if (len)
			*ctrl->p_new.p_s32 = para.u32[0];
	} else
		*ctrl->p_new.p_s32 = THERMAL_TEMP_INVALID;

	return 0;
}

static int _get_frame_desc(struct adaptor_ctx *ctx, unsigned int pad,
		struct mtk_mbus_frame_desc *fd)
{
	/* default -1 is the same as subdrv_call get_frame_desc */
	int ret = -1;
//	struct adaptor_ctx *ctx = to_ctx(sd);
	u64 desc_visited = 0x0;
	int write_to = 0, i = -1, j = 0;

	while (i < SENSOR_SCENARIO_ID_MAX) {
		struct mtk_mbus_frame_desc fd_tmp = {0};
		u32 scenario_id = (-1 == i) ? ctx->cur_mode->id : ctx->seamless_scenarios[i];

		if (scenario_id == SENSOR_SCENARIO_ID_NONE)
			break;

		ret = subdrv_call(ctx, get_frame_desc, scenario_id, &fd_tmp);

		if (!ret) {
			for (j = 0; write_to < MTK_FRAME_DESC_ENTRY_MAX && j < fd_tmp.num_entries;
				++j) {
				if (desc_visited
					& ((u64)(0x1) << fd_tmp.entry[j].bus.csi2.user_data_desc))
					continue;

				dev_info(ctx->dev, "[%s] scenario %u desc %d/%d/%d/%d\n", __func__,
						scenario_id,
						fd_tmp.entry[j].bus.csi2.user_data_desc,
						i, j, fd_tmp.num_entries);
				memcpy(&fd->entry[write_to++], &fd_tmp.entry[j],
					   sizeof(struct mtk_mbus_frame_desc_entry));

				desc_visited |=
					((u64)(0x1) << fd_tmp.entry[j].bus.csi2.user_data_desc);
			}
		}

		++i;
	}

	fd->num_entries = write_to;
	fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;

	return ret;
}

static const char * const state_names[] = {
	ADAPTOR_STATE_NAMES
};

static int _aov_switch_i2c_bus_scl_aux(struct v4l2_ctrl *ctrl)
{
	struct adaptor_ctx *ctx = ctrl_to_ctx(ctrl);
	int ret = 0;
	enum mtk_cam_sensor_i2c_bus_scl aux =
		(enum mtk_cam_sensor_i2c_bus_scl)ctrl->val;

	// aux as aux function number corresponding to gpio table.
	switch (aux) {
	case SDA4:
		ret = pinctrl_select_state(ctx->pinctrl, ctx->state[STATE_SCL_AP]);
		if (ret < 0) {
			dev_info(ctx->dev,
				"[%s] select(%s)(fail)\n",
				__func__, state_names[STATE_SCL_AP]);
			return ret;
		}
		dev_info(ctx->dev,
			"[%s] select(%s)(correct)\n",
			__func__, state_names[STATE_SCL_AP]);
		break;
	case SCL7:
		ret = pinctrl_select_state(ctx->pinctrl, ctx->state[STATE_SCL_SCP]);
		if (ret < 0) {
			dev_info(ctx->dev,
				"[%s] select(%s)(fail)\n",
				__func__, state_names[STATE_SCL_SCP]);
			return ret;
		}
		dev_info(ctx->dev,
			"[%s] select(%s)(correct)\n",
			__func__, state_names[STATE_SCL_SCP]);
		break;
	default:
		dev_info(ctx->dev,
			"[%s] i2c bus aux function not support(%d)\n",
			__func__, ctrl->val);
		return -EINVAL;
	}

	return 0;
}

static int _aov_switch_i2c_bus_sda_aux(struct v4l2_ctrl *ctrl)
{
	struct adaptor_ctx *ctx = ctrl_to_ctx(ctrl);
	int ret = 0;
	enum mtk_cam_sensor_i2c_bus_sda aux =
		(enum mtk_cam_sensor_i2c_bus_sda)ctrl->val;

	// aux as aux function number corresponding to gpio table.
	switch (aux) {
	case SDA4:
		ret = pinctrl_select_state(ctx->pinctrl, ctx->state[STATE_SDA_AP]);
		if (ret < 0) {
			dev_info(ctx->dev,
				"[%s] select(%s)(fail)\n",
				__func__, state_names[STATE_SDA_AP]);
			return ret;
		} else
			dev_info(ctx->dev,
				"[%s] select(%s)(correct)\n",
				__func__, state_names[STATE_SDA_AP]);
		break;
	case SDA7:
		ret = pinctrl_select_state(ctx->pinctrl, ctx->state[STATE_SDA_SCP]);
		if (ret < 0) {
			dev_info(ctx->dev,
				"[%s] select(%s)(fail)\n",
				__func__, state_names[STATE_SDA_SCP]);
			return ret;
		} else
			dev_info(ctx->dev,
				"[%s] select(%s)(correct)\n",
				__func__, state_names[STATE_SDA_SCP]);
		break;
	default:
		dev_info(ctx->dev,
			"[%s] i2c bus aux function not support(%d)\n",
			__func__, ctrl->val);
		return -EINVAL;
	}

	return 0;
}

static int _aov_switch_rx_param(struct v4l2_ctrl *ctrl)
{
	struct adaptor_ctx *ctx = ctrl_to_ctx(ctrl);
	enum mtk_cam_seninf_csi_clk_for_param csi_clk =
		(enum mtk_cam_seninf_csi_clk_for_param)ctrl->val;
	union feature_para para;
	u32 len;
	struct sensor_mode *mode;
	u32 i = 0;

	switch (csi_clk) {
	case CSI_CLK_130:
		para.u32[0] = 130;
		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_SET_AOV_CSI_CLK, para.u8, &len);
		dev_info(ctx->dev,
			"[%s] csi clk select(%u)\n",
			__func__, csi_clk);
		/* update mode csi_param */
		for (i = SENSOR_SCENARIO_ID_MIN; i < SENSOR_SCENARIO_ID_MAX; i++) {
			mode = &ctx->mode[i];
			mode->id = i;
			subdrv_call(ctx, get_csi_param, mode->id, &mode->csi_param);
		}
		dev_info(ctx->dev,
			"[%s] update mode csi_param(done)\n", __func__);
		break;
	case CSI_CLK_242:
		para.u32[0] = 242;
		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_SET_AOV_CSI_CLK, para.u8, &len);
		dev_info(ctx->dev,
			"[%s] csi clk select(%u)\n", __func__, csi_clk);
		/* update mode csi_param */
		for (i = SENSOR_SCENARIO_ID_MIN; i < SENSOR_SCENARIO_ID_MAX; i++) {
			mode = &ctx->mode[i];
			mode->id = i;
			subdrv_call(ctx, get_csi_param, mode->id, &mode->csi_param);
		}
		dev_info(ctx->dev,
			"[%s] update mode csi_param(done)\n", __func__);
		break;
	default:
		dev_info(ctx->dev,
			"[%s] csi clk not support(%d)\n",
			__func__, ctrl->val);
		return -EINVAL;
	}

	return 0;
}

static int _aov_switch_pm_ops(struct v4l2_ctrl *ctrl)
{
	struct adaptor_ctx *ctx = ctrl_to_ctx(ctrl);
	enum mtk_cam_sensor_pm_ops pm_ops =
		(enum mtk_cam_sensor_pm_ops)ctrl->val;

	switch (pm_ops) {
	case AOV_PM_RELAX:
		if (ctx->aov_pm_ops_flag == 1) {
			ctx->aov_pm_ops_flag = 0;
			__pm_relax(ctx->sensor_ws);
			dev_info(ctx->dev,
				"[%s] switch to __pm_relax(correct),pm_ops_flag(%d)\n",
				__func__, ctx->aov_pm_ops_flag);
		} else
			dev_info(ctx->dev,
				"[%s] switch to __pm_relax(no need),pm_ops_flag(%d)\n",
				__func__, ctx->aov_pm_ops_flag);
		break;
	case AOV_PM_STAY_AWAKE:
		if (ctx->aov_pm_ops_flag == 0) {
			ctx->aov_pm_ops_flag = 1;
			__pm_stay_awake(ctx->sensor_ws);
			dev_info(ctx->dev,
				"[%s] switch to __pm_stay_awake(correct),pm_ops_flag(%d)\n",
				__func__, ctx->aov_pm_ops_flag);
		} else
			dev_info(ctx->dev,
				"[%s] switch to __pm_stay_awake(no need),pm_ops_flag(%d)\n",
				__func__, ctx->aov_pm_ops_flag);
		break;
	case AOV_ABNORMAL_FORCE_SENSOR_PWR_OFF:
		adaptor_hw_power_off(ctx);
		dev_info(ctx->dev, "[%s] adaptor_hw_power_off(done)", __func__);
		break;
	case AOV_ABNORMAL_FORCE_SENSOR_PWR_ON:
		adaptor_hw_power_on(ctx);
		dev_info(ctx->dev, "[%s] adaptor_hw_power_on(done)", __func__);
		break;
	default:
		dev_info(ctx->dev,
			"[%s] function not support(%d)\n",
			__func__, ctrl->val);
		return -EINVAL;
	}

	return 0;
}

static int ext_ctrl(struct adaptor_ctx *ctx, struct v4l2_ctrl *ctrl, struct sensor_mode *mode)
{
	int ret = 0;

	if (mode == NULL)
		return -EINVAL;

	switch (ctrl->id) {
	case V4L2_CID_MTK_SENSOR_IDX:
		ctrl->val = ctx->idx;
		break;
	case V4L2_CID_MTK_SOF_TIMEOUT_VALUE:
		if (ctx->shutter_for_timeout != 0) {
			u64 tmp = mode->linetime_in_ns * ctx->shutter_for_timeout;

			ctrl->val = tmp / 1000;
		}
		dev_info(ctx->dev, "[%s][%s] sof timeout value in us %d|%llu|%d|%d\n",
			__func__,
			(ctx->subdrv) ? (ctx->subdrv->name) : "null",
			ctx->shutter_for_timeout,
			mode->linetime_in_ns,
			ctrl->val,
			10000000 / mode->max_framerate);

		if (ctrl->val < (10000000 / mode->max_framerate))
			ctrl->val = 10000000 / mode->max_framerate;
		break;
	case V4L2_CID_VBLANK:
		if (mode->linetime_in_ns_readout > mode->linetime_in_ns) {
			ctrl->val = mode->fll - (mode->height *
				((mode->linetime_in_ns_readout / mode->linetime_in_ns) +
				(mode->linetime_in_ns_readout % mode->linetime_in_ns > 0 ? 1 : 0)));
			dev_info(ctx->dev, "[%s] V4L2_CID_VBLANK %d|%d|%d|%d|%d\n",
				__func__,
				ctrl->val,
				mode->linetime_in_ns_readout,
				mode->linetime_in_ns,
				mode->fll,
				(mode->height *
				((mode->linetime_in_ns_readout / mode->linetime_in_ns) +
			(mode->linetime_in_ns_readout % mode->linetime_in_ns > 0 ? 1 : 0))));
		} else {
			ctrl->val = mode->fll - mode->height;
		}
		break;
	case V4L2_CID_HBLANK:
		ctrl->val =
			(((mode->linetime_in_ns_readout *
				mode->mipi_pixel_rate)/1000000000) - mode->width);

		if (ctrl->val < 1)
			ctrl->val = 1;
		break;
	case V4L2_CID_MTK_SENSOR_PIXEL_RATE:
		ctrl->val = mode->mipi_pixel_rate;
		break;
	case V4L2_CID_MTK_CUST_SENSOR_PIXEL_RATE:
		ctrl->val = mode->cust_pixel_rate;
		break;
	case V4L2_CID_MTK_STAGGER_INFO:
	{
		struct mtk_stagger_info *info = ctrl->p_new.p;

		g_stagger_info(ctx, mode->id, info);
	}
		break;
	case V4L2_CID_MTK_FRAME_DESC:
	{
		struct mtk_mbus_frame_desc *fd = ctrl->p_new.p;

		_get_frame_desc(ctx, 0, fd);
	}
		break;

	case V4L2_CID_MTK_CSI_PARAM:
	{
		struct mtk_csi_param *csi_param = ctrl->p_new.p;

		if (csi_param) {
			csi_param->cphy_settle = mode->csi_param.cphy_settle;
			csi_param->dphy_clk_settle = mode->csi_param.dphy_clk_settle;
			csi_param->dphy_data_settle = mode->csi_param.dphy_data_settle;
			csi_param->dphy_trail = mode->csi_param.dphy_trail;
			csi_param->legacy_phy = mode->csi_param.legacy_phy;
			csi_param->not_fixed_trail_settle =
				mode->csi_param.not_fixed_trail_settle;
			csi_param->dphy_csi2_resync_dmy_cycle =
				mode->csi_param.dphy_csi2_resync_dmy_cycle;
		}
	}
		break;
	case V4L2_CID_MTK_SENSOR_RESET_BY_USER:
		ctrl->val = mode->esd_reset_by_user;
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
		struct mtk_stagger_max_exp_time *info = ctrl->p_new.p;

		g_max_exposure(ctx, ctx->try_format_mode->id, info);
	}
		break;
	case V4L2_CID_STAGGER_TARGET_SCENARIO:
	{
		struct mtk_stagger_target_scenario *info = ctrl->p_new.p;

		g_stagger_scenario(ctx, ctx->try_format_mode->id, info);
	}
		break;
	case V4L2_CID_MTK_SENSOR_STATIC_PARAM:
	{
		struct mtk_sensor_static_param *info = ctrl->p_new.p;
		u32 val, len;
		union feature_para para;

		if (info->scenario_id < ctx->mode_cnt) {
			struct sensor_mode *mode = &ctx->mode[info->scenario_id];

			para.u64[0] = info->scenario_id;
			para.u64[1] = (u64)&val;

			subdrv_call(ctx, feature_control,
						SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO,
						para.u8, &len);

			info->fps = val / 10;

			if (mode->linetime_in_ns_readout > mode->linetime_in_ns) {
				info->vblank = mode->fll - mode->height *
				((mode->linetime_in_ns_readout / mode->linetime_in_ns) +
				(mode->linetime_in_ns_readout % mode->linetime_in_ns) ? 1 : 0);
			} else {
				info->vblank = mode->fll - mode->height;
			}

			info->hblank =
				(((mode->linetime_in_ns_readout *
					mode->mipi_pixel_rate)/1000000000) - mode->width);

			if (info->hblank < 1)
				info->hblank = 1;

			info->pixelrate = mode->mipi_pixel_rate;
			info->cust_pixelrate = mode->cust_pixel_rate;
			info->grab_h = mode->height;
			info->grab_w = mode->width;
		}

#if IMGSENSOR_LOG_MORE
		dev_dbg(ctx->dev,
				"%s [scenario %d]:fps: %d vb: %d hb: %d pixelrate: %d cust_pixel_rate: %d, w %d, h %d\n",
				__func__, info->scenario_id, info->fps, info->vblank,
				info->hblank, info->pixelrate, info->cust_pixelrate,
				info->grab_w, info->grab_h);
#endif
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
	int i;

	/*
	 * Applying V4L2 control value only happens
	 * when power is up for streaming
	 */
	if (pm_runtime_get_if_in_use(dev) == 0)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_UPDATE_SOF_CNT:
		subdrv_call(ctx, update_sof_cnt, (u64)ctrl->val);
		break;
	case V4L2_CID_VSYNC_NOTIFY:
		/* update ctx sof cnt */
		ctx->sof_cnt = ctrl->val;

		subdrv_call(ctx, vsync_notify, (u64)ctrl->val);
		notify_fsync_mgr_vsync(ctx);

		/* update timeout value upon vsync*/
		ctx->shutter_for_timeout = ctx->exposure->val;
		if (ctx->cur_mode->fine_intg_line)
			ctx->shutter_for_timeout /= 1000;
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		para.u64[0] = ctrl->val;
		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_SET_GAIN,
			para.u8, &len);
		break;
	case V4L2_CID_EXPOSURE:
		{
			u32 fsync_exp[1] = {0}; /* needed by fsync set_shutter */

			para.u64[0] = ctrl->val;

			fsync_exp[0] = (u32)para.u64[0];
			ret = chk_s_exp_with_fl_by_fsync_mgr(ctx, fsync_exp, 1);
			if (!ret) {
				/* NOT enable frame-sync || using HW sync solution */
				subdrv_call(ctx, feature_control,
					SENSOR_FEATURE_SET_ESHUTTER,
					para.u8, &len);
			}
			notify_fsync_mgr_set_shutter(ctx, fsync_exp, 1, ret);
		}
		break;
	case V4L2_CID_MTK_STAGGER_AE_CTRL:
		ADAPTOR_SYSTRACE_BEGIN("SensorWorker::s_ae_ctrl");
		s_ae_ctrl(ctrl);
		ADAPTOR_SYSTRACE_END();
		break;
	case V4L2_CID_EXPOSURE_ABSOLUTE:
		{
			u32 fsync_exp[1] = {0}; /* needed by fsync set_shutter */
			__u32 fine_integ_time = 0;

			para.u64[0] = (u64)(ctrl->val) * 100000;
			do_div(para.u64[0], ctx->cur_mode->linetime_in_ns);

			/* read fine integ time*/
			fine_integ_time = g_sensor_fine_integ_line(ctx);

			if (fine_integ_time > 0)
				para.u64[0] = para.u64[0] * 1000;

			fsync_exp[0] = (u32)para.u64[0];
			ret = chk_s_exp_with_fl_by_fsync_mgr(ctx, fsync_exp, 1);
			if (!ret) {
				/* NOT enable frame-sync || using HW sync solution */
				subdrv_call(ctx, feature_control,
					SENSOR_FEATURE_SET_ESHUTTER,
					para.u8, &len);
			}
			notify_fsync_mgr_set_shutter(ctx, fsync_exp, 1, ret);
		}
		break;
	case V4L2_CID_VBLANK:
		para.u64[0] = ctx->exposure->val;
		para.u64[1] = ctx->cur_mode->height + ctrl->val;
		para.u64[2] = 0;
		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_SET_FRAMELENGTH,
			para.u8, &len);
		break;
	case V4L2_CID_TEST_PATTERN:
		// dev_dbg(dev, "V4L2_SET_TEST_PATTERN (mode:%d)", ctrl->val);
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
			"V4L2_CID_FRAME_SYNC (set_sync), idx:%d, value:%d(%#x)\n",
			ctx->idx, ctrl->val, ctrl->val);

		notify_fsync_mgr_set_sync(ctx, (u64)ctrl->val);
		break;

	case V4L2_CID_FSYNC_ASYNC_MASTER:
		dev_info(dev,
			"V4L2_CID_FSYNC_ASYNC_MASTER, idx:%d, value:%d\n",
			ctx->idx, ctrl->val);
		notify_fsync_mgr_set_async_master(ctx, ctrl->val);
		break;

	case V4L2_CID_FSYNC_MAP_ID:
		notify_fsync_mgr_update_tg(ctx, (u64)ctrl->val);
		break;

	case V4L2_CID_FSYNC_LISTEN_TARGET:
		notify_fsync_mgr_update_target_tg(ctx, (u64)ctrl->val);
		dev_info(dev,
			"V4L2_CID_FSYNC_LISTEN_TARGET (update_tg), idx:%d, value:%d\n",
			ctx->idx, ctrl->val);
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
			u32 fsync_exp[1] = {0}; /* needed by fsync set_shutter */

			para.u64[0] = info->shutter;

			fsync_exp[0] = (u32)para.u64[0];
			ret = chk_s_exp_with_fl_by_fsync_mgr(ctx, fsync_exp, 1);
			if (!ret) {
				/* NOT enable frame-sync || using HW sync solution */
				subdrv_call(ctx, feature_control,
					SENSOR_FEATURE_SET_ESHUTTER,
					para.u8, &len);
			}
			notify_fsync_mgr_set_shutter(ctx, fsync_exp, 1, ret);

			para.u64[0] = info->gain;
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

			// TODO: remove CID
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

			/* reset seamless_scenarios */
			for (i = SENSOR_SCENARIO_ID_MIN; i < SENSOR_SCENARIO_ID_MAX; i++)
				ctx->seamless_scenarios[i] = SENSOR_SCENARIO_ID_NONE;

			ret = copy_from_user(
				&ctx->seamless_scenarios, info->target_scenario_ids,
				min(sizeof(ctx->seamless_scenarios),
					info->count * sizeof(*info->target_scenario_ids)));

			/* returns number of bytes that could not be copied */
			/* On success, this will be zero */
			if (ret != 0) {
				dev_info(dev,
					"[V4L2_CID_SEAMLESS_SCENARIOS] copy_from_user has some error, ret:%d\n",
					ret);
			}
		}
		break;
	case V4L2_CID_START_SEAMLESS_SWITCH:
		{
			struct mtk_seamless_switch_param *info = ctrl->p_new.p;
			u64 time_boot = ktime_get_boottime_ns();
			u64 time_mono = ktime_get_ns();

			para.u64[0] = info->target_scenario_id;
			para.u64[1] = (uintptr_t)&info->ae_ctrl[0];
			para.u64[2] = (uintptr_t)&info->ae_ctrl[1];

			dev_info(dev,
				    "seamless %u s[%u %u %u %u %u] g[%u %u %u %u %u] s1[%u %u %u %u %u] g1[%u %u %u %u %u] %llu|%llu\n",
					info->target_scenario_id,
					info->ae_ctrl[0].exposure.arr[0],
					info->ae_ctrl[0].exposure.arr[1],
					info->ae_ctrl[0].exposure.arr[2],
					info->ae_ctrl[0].exposure.arr[3],
					info->ae_ctrl[0].exposure.arr[4],
					info->ae_ctrl[0].gain.arr[0],
					info->ae_ctrl[0].gain.arr[1],
					info->ae_ctrl[0].gain.arr[2],
					info->ae_ctrl[0].gain.arr[3],
					info->ae_ctrl[0].gain.arr[4],
					info->ae_ctrl[1].exposure.arr[0],
					info->ae_ctrl[1].exposure.arr[1],
					info->ae_ctrl[1].exposure.arr[2],
					info->ae_ctrl[1].exposure.arr[3],
					info->ae_ctrl[1].exposure.arr[4],
					info->ae_ctrl[1].gain.arr[0],
					info->ae_ctrl[1].gain.arr[1],
					info->ae_ctrl[1].gain.arr[2],
					info->ae_ctrl[1].gain.arr[3],
					info->ae_ctrl[1].gain.arr[4],
					time_boot,
					time_mono);

			if (info->target_scenario_id == 0 &&
				info->ae_ctrl[0].exposure.arr[0] == 0 &&
				info->ae_ctrl[0].gain.arr[0] == 0 &&
				info->ae_ctrl[1].exposure.arr[0] == 0 &&
				info->ae_ctrl[1].gain.arr[0] == 0) {
				dev_info(dev, "V4L2_CID_START_SEAMLESS_SWITCH %u invalid value\n",
					info->target_scenario_id);
				break;
			}
			subdrv_call(ctx, feature_control,
				SENSOR_FEATURE_SEAMLESS_SWITCH,
				para.u8, &len);

			notify_fsync_mgr_seamless_switch(ctx);

			/*store ae ctrl for ESD reset*/
			memset(&ctx->ae_memento, 0, sizeof(ctx->ae_memento));
			memcpy(&ctx->ae_memento, &info->ae_ctrl[0],  sizeof(ctx->ae_memento));

			/* update timeout value upon seamless switch*/
			ctx->exposure->val = info->ae_ctrl[0].exposure.arr[0];
			ctx->shutter_for_timeout = info->ae_ctrl[0].exposure.arr[0];
			if (ctx->cur_mode->fine_intg_line)
				ctx->shutter_for_timeout /= 1000;

			if (info->target_scenario_id < MODE_MAXCNT)
				ctx->cur_mode = &ctx->mode[info->target_scenario_id];
			else {
				dev_info(dev, "[%s] err info->target_scenario_id %d >= MODE_MAXCNT\n",
					__func__,
					info->target_scenario_id);
			}

		}
		break;
#ifdef IMGSENSOR_DEBUG
	case V4L2_CID_MTK_DEBUG_CMD:
		proc_debug_cmd(ctx, ctrl->p_new.p_char);
		break;
#endif
	case V4L2_CID_MTK_SENSOR_POWER:
		dev_dbg(dev, "V4L2_CID_MTK_SENSOR_POWER val = %d\n", ctrl->val);
		if (ctrl->val)
			adaptor_hw_power_on(ctx);
		else
			adaptor_hw_power_off(ctx);
		break;
	case V4L2_CID_MTK_MSTREAM_MODE:
		dev_info(dev,
			"V4L2_CID_MTK_MSTREAM_MODE, idx:%d, value:%d\n",
			ctx->idx, ctrl->val);
		notify_fsync_mgr_mstream_en(ctx, ctrl->val);
		break;
	case V4L2_CID_MTK_N_1_MODE:
		{
			struct mtk_n_1_mode *info = ctrl->p_new.p;

			dev_info(dev,
				"V4L2_CID_MTK_N_1_MODE, idx:%d, n:%u, en:%u\n",
				ctx->idx, info->n, info->en);
			notify_fsync_mgr_n_1_en(ctx, info->n, info->en);
		}
		break;

	case V4L2_CID_MTK_SENSOR_TEST_PATTERN_DATA:
		//struct mtk_test_pattern_data *info = ctrl->p_new.p;

		// dev_dbg(dev, "V4L2_SET_TEST_PATTERN_DATA R(%x),Gr(%x),Gb(%x),B(%x)",
			// info->Channel_R,
			// info->Channel_Gr,
			// info->Channel_Gb,
			// info->Channel_B);
		subdrv_call(ctx, feature_control,
			SENSOR_FEATURE_SET_TEST_PATTERN_DATA,
			ctrl->p_new.p, &len);
		break;
	case V4L2_CID_MTK_SENSOR_RESET:
		{
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT image_window;
			MSDK_SENSOR_CONFIG_STRUCT sensor_config_data;

			//dev_info(dev, "V4L2_CID_MTK_SENSOR_RESET\n");
			if (adaptor_hw_sensor_reset(ctx) < 0)
				break;

			ctx->is_sensor_reset_stream_off = 1;
			subdrv_call(ctx, open);
			subdrv_call(ctx, control,
					ctx->cur_mode->id,
					&image_window,
					&sensor_config_data);

			restore_ae_ctrl(ctx);
			_sensor_reset_s_stream(ctrl);
			//dev_info(dev, "exit V4L2_CID_MTK_SENSOR_RESET\n");
		}
		break;
	case V4L2_CID_MTK_SENSOR_INIT:
		//dev_info(dev, "V4L2_CID_MTK_SENSOR_INIT val = %d\n", ctrl->val);
		if (ctrl->val)
			adaptor_sensor_init(ctx);
		break;
	case V4L2_CID_MTK_SENSOR_RESET_S_STREAM:
		_sensor_reset_s_stream(ctrl);
		break;
	case V4L2_CID_MTK_AOV_SWITCH_I2C_BUS_SCL_AUX:
		ret = _aov_switch_i2c_bus_scl_aux(ctrl);
		if (ret < 0)
			dev_info(dev,
				"[%s] _aov_switch_i2c_bus_scl_aux(fail),ret(%d)\n",
				__func__, ret);
		else
			dev_info(dev,
				"[%s] _aov_switch_i2c_bus_scl_aux(correct),ret(%d)\n",
				__func__, ret);
		break;
	case V4L2_CID_MTK_AOV_SWITCH_I2C_BUS_SDA_AUX:
		ret = _aov_switch_i2c_bus_sda_aux(ctrl);
		if (ret < 0)
			dev_info(dev,
				"[%s] _aov_switch_i2c_bus_sda_aux(fail),ret(%d)\n",
				__func__, ret);
		else
			dev_info(dev,
				"[%s] _aov_switch_i2c_bus_sda_aux(correct),ret(%d)\n",
				__func__, ret);
		break;
	case V4L2_CID_MTK_AOV_SWITCH_RX_PARAM:
		ret = _aov_switch_rx_param(ctrl);
		dev_info(dev,
			"[%s] _aov_switch_rx_param(correct),ret(%d)\n",
			__func__, ret);
		break;
	case V4L2_CID_MTK_AOV_SWITCH_PM_OPS:
		ret = _aov_switch_pm_ops(ctrl);
		dev_info(dev,
			"[%s] _aov_switch_pm_ops(correct),ret(%d)\n",
			__func__, ret);
		break;
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
	"Solid Color",
	"COLOR_BARS",
	"COLOR_BARS_FADE_TO_GRAY",
	"PN9",
	"BLACK",
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
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0x7fffffff,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_fsync_async_master = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_FSYNC_ASYNC_MASTER,
	.name = "fsync_async_master",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.def = 255, // FrameSync::MASTER_IDX_NONE
	.max = 0x7fffffff,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_fsync_map_id = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_FSYNC_MAP_ID,
	.name = "fsync_map_id",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0xffff,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_fsync_listen_target = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_FSYNC_LISTEN_TARGET,
	.name = "fsync_listen_target",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0xffff,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_vsync_notify = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_VSYNC_NOTIFY,
	.name = "vsync_notify",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0x7fffffff,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_update_sof_cnt = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_UPDATE_SOF_CNT,
	.name = "update_sof_cnt",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0x7fffffff,
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


static struct v4l2_ctrl_config cfg_csi_param_ctrl = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_CSI_PARAM,
	.name = "mtk_csi_param",
	.type = V4L2_CTRL_TYPE_U32,
	//.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
	.max = 0xffffffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_csi_param)},
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
	.max = 0xffffffff,
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

static const struct v4l2_ctrl_config cfg_mtkcam_pixel_rate = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_SENSOR_PIXEL_RATE,
	.name = "pixel rate",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_VOLATILE,
	.max = 0x7fffffff,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_mtkcam_cust_pixel_rate = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_CUST_SENSOR_PIXEL_RATE,
	.name = "customized pixel rate",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_VOLATILE,
	.max = 0x7fffffff,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_mtkcam_sof_timeout_value = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_SOF_TIMEOUT_VALUE,
	.name = "sof timeout value",
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

static const struct v4l2_ctrl_config cfg_static_param = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_SENSOR_STATIC_PARAM,
	.name = "static param by scenario",
	.type = V4L2_CTRL_TYPE_U32,
	.flags = V4L2_CTRL_FLAG_VOLATILE,
	.max = 0xffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_sensor_static_param)},
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

static const struct v4l2_ctrl_config cfg_test_pattern_data = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_SENSOR_TEST_PATTERN_DATA,
	.name = "test_pattern_data",
	.type = V4L2_CTRL_TYPE_U32,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0xffffffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_test_pattern_data)},
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

static const struct v4l2_ctrl_config cfg_sensor_power = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_SENSOR_POWER,
	.name = "sensor_power",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 1,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_sensor_reset = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_SENSOR_RESET,
	.name = "sensor_reset",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 1,
	.step = 1,
};


static const struct v4l2_ctrl_config cfg_mstream_mode = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_MSTREAM_MODE,
	.name = "mstream_mode",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.def = 0,
	.max = 1,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_n_1_mode = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_N_1_MODE,
	.name = "n_1_mode",
	.type = V4L2_CTRL_TYPE_U32,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0xffff,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_n_1_mode)},
};

static const struct v4l2_ctrl_config cfg_sensor_init = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_SENSOR_INIT,
	.name = "sensor_init",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 1,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_sensor_reset_s_stream = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_SENSOR_RESET_S_STREAM,
	.name = "sensor_reset_s_stream",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 1,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_sensor_reset_by_user = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_SENSOR_RESET_BY_USER,
	.name = "sensor_reset_by_user",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.flags = V4L2_CTRL_FLAG_VOLATILE,
	.max = 1,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_mtkcam_sensor_idx = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_SENSOR_IDX,
	.name = "sensor idx",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_VOLATILE,
	.max = 0xffff,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_mtkcam_aov_switch_i2c_bus_scl_aux = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_AOV_SWITCH_I2C_BUS_SCL_AUX,
	.name = "aov_switch_i2c_bus_scl_aux",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0xffff,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_mtkcam_aov_switch_i2c_bus_sda_aux = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_AOV_SWITCH_I2C_BUS_SDA_AUX,
	.name = "aov_switch_i2c_bus_sda_aux",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0xffff,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_mtkcam_aov_switch_rx_param = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_AOV_SWITCH_RX_PARAM,
	.name = "aov_switch_rx_param",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0xffff,
	.step = 1,
};

static const struct v4l2_ctrl_config cfg_mtkcam_aov_switch_pm_ops = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_MTK_AOV_SWITCH_PM_OPS,
	.name = "aov_pm_ops",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0xffff,
	.step = 1,
};

void adaptor_sensor_init(struct adaptor_ctx *ctx)
{
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s][%s]+\n",
		__func__, (ctx->subdrv) ? (ctx->subdrv->name) : "null");
#endif
	if (ctx && !ctx->is_sensor_inited) {
		subdrv_call(ctx, open);
		ctx->is_sensor_inited = 1;
	}
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s][%s]-\n",
		__func__, (ctx->subdrv) ? (ctx->subdrv->name) : "null");
#endif
}

void restore_ae_ctrl(struct adaptor_ctx *ctx)
{
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s][%s]+\n",
		__func__, (ctx->subdrv) ? (ctx->subdrv->name) : "null");
#endif
	if (!ctx->ae_memento.exposure.le_exposure ||
		!ctx->ae_memento.gain.le_gain) {
		return;
	}

	// dev_dbg(ctx->dev, "%s\n", __func__);

	do_set_ae_ctrl(ctx, &ctx->ae_memento);
#if IMGSENSOR_LOG_MORE
	dev_info(ctx->dev, "[%s][%s]-\n",
		__func__, (ctx->subdrv) ? (ctx->subdrv->name) : "null");
#endif
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
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_mtkcam_pixel_rate, NULL);


	/* pixel rate for special output timing*/
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_mtkcam_cust_pixel_rate, NULL);

	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_mtkcam_sof_timeout_value, NULL);


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

	min = ctx->subctx.ana_gain_min;
	max = ctx->subctx.ana_gain_max;
	step = ctx->subctx.ana_gain_step;
	def = ctx->subctx.ana_gain_def;

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
	ctx->test_pattern = v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &ctrl_ops,
			V4L2_CID_TEST_PATTERN,
			ARRAY_SIZE(test_pattern_menu) - 1,
			0, 0, test_pattern_menu);
	if (ctx->test_pattern)
		ctx->test_pattern->flags |= V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;

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

	/* custom frame-sync - async master */
	ctx->fsync_async_master = v4l2_ctrl_new_custom(&ctx->ctrls,
		&cfg_fsync_async_master, NULL);

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

	/*
	 * update fsync listen target (the id vsync listen to), this is
	 * replacment of fsync_map_id
	 */
	ctx->fsync_listen_target = v4l2_ctrl_new_custom(&ctx->ctrls,
		&cfg_fsync_listen_target, NULL);

	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_vsync_notify, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_update_sof_cnt, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_stagger_info, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_stagger_scenario, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_stagger_max_exp_time, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_start_seamless_switch, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_static_param, NULL);

	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_seamless_scenario, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_fd_ctrl, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_csi_param_ctrl, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_test_pattern_data, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_mtkcam_sensor_idx, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_mtkcam_aov_switch_i2c_bus_scl_aux, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_mtkcam_aov_switch_i2c_bus_sda_aux, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_mtkcam_aov_switch_rx_param, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_mtkcam_aov_switch_pm_ops, NULL);

#ifdef IMGSENSOR_DEBUG
	v4l2_ctrl_new_custom(&ctx->ctrls, &cfg_debug_cmd, NULL);
#endif

	v4l2_ctrl_new_custom(ctrl_hdlr, &cfg_sensor_power, NULL);
	v4l2_ctrl_new_custom(ctrl_hdlr, &cfg_mstream_mode, NULL);
	v4l2_ctrl_new_custom(ctrl_hdlr, &cfg_n_1_mode, NULL);
	v4l2_ctrl_new_custom(ctrl_hdlr, &cfg_sensor_reset, NULL);
	v4l2_ctrl_new_custom(ctrl_hdlr, &cfg_sensor_init, NULL);
	v4l2_ctrl_new_custom(ctrl_hdlr, &cfg_sensor_reset_s_stream, NULL);
	v4l2_ctrl_new_custom(ctrl_hdlr, &cfg_sensor_reset_by_user, NULL);

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
