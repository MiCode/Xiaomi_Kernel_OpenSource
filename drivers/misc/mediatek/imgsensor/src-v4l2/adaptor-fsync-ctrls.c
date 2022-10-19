// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/atomic.h>

#include "kd_imgsensor_define_v4l2.h"
#include "imgsensor-user.h"

#include "adaptor.h"
#include "adaptor-def.h"
#include "adaptor-common-ctrl.h"
#include "adaptor-fsync-ctrls.h"


#define REDUCE_FSYNC_CTRLS_LOG





/*******************************************************************************
 * fsync mgr variables
 ******************************************************************************/
/* for checking if any sensor enter long exposure mode */
static atomic_t long_exp_mode_bits = ATOMIC_INIT(0);





/*******************************************************************************
 * sensor driver feature ctrls
 ******************************************************************************/
static u32 fsync_mgr_g_sensor_hw_sync_mode(struct adaptor_ctx *ctx)
{
	union feature_para para;
	u32 sync_mode = 0;
	u32 len;

	para.u32[0] = 0;

	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_SENSOR_SYNC_MODE,
		para.u8, &len);

	sync_mode = para.u32[0];


#if !defined(REDUCE_FSYNC_CTRLS_LOG)
	dev_info(ctx->dev,
		"%s: sidx:%d, get hw sync mode:%u(N:0/M:1/S:2)\n",
		__func__,
		ctx->idx,
		sync_mode);
#endif


	return sync_mode;
}

static void fsync_mgr_s_frame_length(struct adaptor_ctx *ctx)
{
	union feature_para para;
	u32 len;

	// para.u64[0] = ctx->subctx.frame_length;
	para.u64[0] = ctx->fsync_out_fl;

	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_SET_FRAMELENGTH,
		para.u8, &len);
}

#if defined(TWO_STAGE_FS)
static void fsync_mgr_s_multi_shutter_frame_length(
	struct adaptor_ctx *ctx,
	u32 *ae_exp_arr, u32 ae_exp_cnt)
{
	union feature_para para;
	u32 fsync_exp[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u32 len = 0;
	int i;

	if (likely(ae_exp_arr != NULL)) {
		for (i = 0;
			(i < ae_exp_cnt) && (i < IMGSENSOR_STAGGER_EXPOSURE_CNT);
			++i)
			fsync_exp[i] = (u32)(*(ae_exp_arr + i));
	}

	para.u64[0] = (u64)fsync_exp;
	para.u64[1] = min_t(u32, ae_exp_cnt, (u32)IMGSENSOR_STAGGER_EXPOSURE_CNT);
	para.u64[2] = ctx->fsync_out_fl;

	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME,
		para.u8, &len);
}
#endif // TWO_STAGE_FS


/*******************************************************************************
 * fsync mgr static functions
 ******************************************************************************/
static void fsync_mgr_chk_long_exposure(struct adaptor_ctx *ctx,
	const u32 *ae_exp_arr, const u32 ae_exp_cnt)
{
	unsigned int i = 0;
	int has_long_exp = 0;
	u32 fine_integ_line = 0;

	fine_integ_line =
		g_sensor_fine_integ_line(ctx, ctx->subctx.current_scenario_id);

	for (i = 0; i < ae_exp_cnt; ++i) {
		u32 exp_lc =
			FINE_INTEG_CONVERT(ae_exp_arr[i], fine_integ_line);

		/* check if any exp will enter long exposure mode */
		if ((exp_lc + ctx->subctx.margin) >=
				ctx->subctx.max_frame_length) {
			has_long_exp = 1;
			break;
		}
	}

	/* has_long_exp > 0 => set bits ; has_long_exp == 0 => clear bits */
	if (has_long_exp != 0)
		atomic_fetch_or((1UL << ctx->idx), &long_exp_mode_bits);
	else
		atomic_fetch_and((~(1UL << ctx->idx)), &long_exp_mode_bits);

#if !defined(REDUCE_FSYNC_CTRLS_LOG)
	dev_info(ctx->dev,
		"%s: sidx:%d, NOTICE: detected long exp:%d, long_exp_mode_bits:%#x\n",
		__func__, ctx->idx,
		has_long_exp, atomic_read(&long_exp_mode_bits));
#endif
}


static void fsync_mgr_set_hdr_exp_data(struct adaptor_ctx *ctx,
	struct fs_hdr_exp_st *p_hdr_exp,
	u32 *ae_exp_arr, u32 ae_exp_cnt,
	u32 fine_integ_line, const u32 mode_id)
{
	struct mtk_stagger_info info = {0};
	unsigned int i = 0;
	int ret = 0;

	/* error handle */
	if (unlikely(ae_exp_cnt == 0
		/*|| ae_exp_cnt > MAX_NUM_OF_EXPOSURE*/
		|| ae_exp_arr == NULL)) {

		dev_info(ctx->dev,
			"%s: sidx:%d, ERROR: get ae_exp_cnt:%u (min:1, max:), *ae_exp_arr:%p, return\n",
			__func__, ctx->idx,
			ae_exp_cnt/*, MAX_NUM_OF_EXPOSURE*/,
			ae_exp_arr);

		return;
	}

	if (unlikely(p_hdr_exp == NULL)) {
		dev_info(ctx->dev,
			"%s: sidx:%d, ERROR: get p_hdr_exp is NULL, return\n",
			__func__, ctx->idx);

		return;
	}


	info.scenario_id = SENSOR_SCENARIO_ID_NONE;

	/* for hdr-exp settings, e.g. STG sensor */
	// ret = g_stagger_info(ctx, ctx->cur_mode->id, &info);
	ret = g_stagger_info(ctx, mode_id, &info);
	if (!ret) {
		p_hdr_exp->mode_exp_cnt = info.count;
		p_hdr_exp->ae_exp_cnt = ae_exp_cnt;
		p_hdr_exp->readout_len_lc = ctx->subctx.readout_length;
		p_hdr_exp->read_margin_lc = ctx->subctx.read_margin;

		for (i = 0; i < ae_exp_cnt; ++i) {
			int idx = hdr_exp_idx_map[ae_exp_cnt][i];

			if (idx >= 0) {
				p_hdr_exp->exp_lc[idx] = ae_exp_arr[i];
				if (fine_integ_line) {
					p_hdr_exp->exp_lc[idx] =
						FINE_INTEG_CONVERT(p_hdr_exp->exp_lc[idx],
							fine_integ_line);
				}

#ifndef REDUCE_FSYNC_CTRLS_LOG
				adaptor_logi(ctx,
					"ae_exp_arr[%u]:%u, fine_integ_line:%u, p_hdr_exp->exp_lc[%d]:%u\n",
					i, ae_exp_arr[i], fine_integ_line,
					idx, p_hdr_exp->exp_lc[idx]);
#endif // !REDUCE_FSYNC_CTRLS_LOG

			} else {
				adaptor_logi(ctx,
					"ERROR: idx:%d (< 0) = hdr_exp_idx_map[%u][%u]\n",
					idx, ae_exp_cnt, i);
			}
		}
	}
}

static void fsync_mgr_set_exp_data(struct adaptor_ctx *ctx,
	struct fs_perframe_st *p_pf_ctrl,
	u32 *ae_exp_arr, u32 ae_exp_cnt, const u32 mode_id)
{
	u32 fine_integ_line = 0;

	/* error handle */
	if (unlikely(ae_exp_arr == NULL || ae_exp_cnt == 0)) {
		dev_info(ctx->dev,
			"%s: sidx:%d, ERROR: get ae_exp_arr is NULL, ae_exp_cnt:%u, return\n",
			__func__, ctx->idx,
			ae_exp_cnt);

		return;
	}

	if (unlikely(p_pf_ctrl == NULL)) {
		dev_info(ctx->dev,
			"%s: sidx:%d, ERROR: get p_pf_ctrl is NULL, return\n",
			__func__, ctx->idx);

		return;
	}


	fine_integ_line = g_sensor_fine_integ_line(ctx, mode_id);
	p_pf_ctrl->shutter_lc = (ae_exp_cnt == 1) ? *(ae_exp_arr + 0) : 0;
	if (fine_integ_line) {
		p_pf_ctrl->shutter_lc =
			FINE_INTEG_CONVERT(p_pf_ctrl->shutter_lc, fine_integ_line);
	}

	fsync_mgr_set_hdr_exp_data(ctx,
		&p_pf_ctrl->hdr_exp,
		ae_exp_arr, ae_exp_cnt,
		fine_integ_line, mode_id);
}


/*******************************************************************************
 * call back function for Frame-Sync set frame length using
 ******************************************************************************/
/* return: 0 => No-Error ; non-0 => Error */
int cb_fsync_mgr_set_framelength(void *p_ctx,
	unsigned int cmd_id, unsigned int framelength)
{
	struct adaptor_ctx *ctx = NULL;
	struct fs_perframe_st pf_ctrl = {0};
	enum ACDK_SENSOR_FEATURE_ENUM cmd = 0;
	int ret = 0;

	/* error handle */
	if (unlikely(p_ctx == NULL)) {
		ret = 1;
		pr_info(
			"%s: ERROR: p_ctx is NULL (cmd_id:%u, fl:%u), return:%d\n",
			__func__,
			cmd_id, framelength, ret);

		return ret;
	}


	ctx = (struct adaptor_ctx *)p_ctx;
	cmd = (enum ACDK_SENSOR_FEATURE_ENUM)cmd_id;


	/* not expected case */
	if (unlikely(ctx->fsync_mgr == NULL)) {
		ret = 2;

#if !defined(FORCE_DISABLE_FSYNC_MGR)
		dev_info(ctx->dev,
			"%s: sidx:%d, NOTICE: ctx->fsync_mgr is NULL (cmd_id:%u, fl:%u), return:%d\n",
			__func__, ctx->idx,
			cmd_id, framelength, ret);
#endif

		return ret;
	}


	// ctx->subctx.frame_length = framelength;
	ctx->fsync_out_fl = framelength;


	switch (cmd) {
#if defined(TWO_STAGE_FS)
	case SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME:

#if !defined(REDUCE_FSYNC_CTRLS_LOG)
		dev_info(ctx->dev,
			"%s: sidx:%d, CMD:SET_MULTI_SHUTTER_FRAME_TIME, update ctx->fsync_out_fl:%u, return to notify set shutter\n",
			__func__, ctx->idx,
			ctx->fsync_out_fl);
#endif

		break;
#endif
	case SENSOR_FEATURE_SET_FRAMELENGTH:

#if !defined(REDUCE_FSYNC_CTRLS_LOG)
		dev_info(ctx->dev,
			"%s: sidx:%d, CMD:SET_FRAMELENGTH, set ctx->fsync_out_fl:%u to sensor drv\n",
			__func__, ctx->idx,
			ctx->fsync_out_fl);
#endif

		/* set frame length */
		fsync_mgr_s_frame_length(ctx);

		/* update sensor current fl_lc to Frame-Sync */
		pf_ctrl.sensor_id = ctx->subdrv->id;
		pf_ctrl.sensor_idx = ctx->idx;
		pf_ctrl.out_fl_lc = ctx->subctx.frame_length;
		ctx->fsync_mgr->fs_update_shutter(&pf_ctrl);

		break;

	default:
		dev_info(ctx->dev, "unknown CMD type, do nothing\n");
		break;
	}

	return ret;
}


/*******************************************************************************
 * streaming ctrls
 ******************************************************************************/
void notify_fsync_mgr_streaming(struct adaptor_ctx *ctx, unsigned int flag)
{
	struct fs_streaming_st s_info = {0};
	unsigned int ret = 0;

	/* not expected case */
	if (unlikely(ctx->fsync_mgr == NULL)) {

#if !defined(FORCE_DISABLE_FSYNC_MGR)
		dev_info(ctx->dev,
			"%s: sidx:%d, NOTICE: ctx->fsync_mgr is NULL, return\n",
			__func__, ctx->idx);
#endif

		return;
	}

	s_info.sensor_id = ctx->subdrv->id;
	s_info.sensor_idx = ctx->idx;

	/* fsync_map_id is cam_mux no */
	s_info.cammux_id = (ctx->fsync_map_id->val > 0)
		? (ctx->fsync_map_id->val + 1) : 0;
	/* 7s use fsync_listen_target to update ccu tg id, so init from this */
	s_info.target_tg = ctx->fsync_listen_target->val;

	s_info.fl_active_delay = ctx->subctx.frame_time_delay_frame;

	/* using ctx->subctx.frame_length instead of ctx->cur_mode->fll */
	/* for any settings before streaming on */
	s_info.def_fl_lc = ctx->subctx.frame_length;
	s_info.max_fl_lc = ctx->subctx.max_frame_length;

	/* frame sync sensor operate mode. none/master/slave */
	s_info.sync_mode = fsync_mgr_g_sensor_hw_sync_mode(ctx);


	/* using ctx->subctx.shutter instead of ctx->subctx.exposure_def */
	/* for any settings before streaming on */
	s_info.def_shutter_lc = ctx->subctx.shutter;
	s_info.margin_lc = g_sensor_margin(ctx, ctx->subctx.current_scenario_id);


	/* sensor mode info */
	s_info.pclk = ctx->subctx.pclk;
	s_info.linelength = ctx->subctx.line_length;
	s_info.lineTimeInNs =
		CALC_LINE_TIME_IN_NS(s_info.pclk, s_info.linelength);


	/* callback data */
	s_info.func_ptr = cb_fsync_mgr_set_framelength;
	s_info.p_ctx = ctx;


	/* call frame-sync streaming ON/OFF */
	ret = ctx->fsync_mgr->fs_streaming(flag, &s_info);
	if (unlikely(ret != 0)) {
		dev_info(ctx->dev,
			"%s: sidx:%d, NOTICE: frame-sync streaming ERROR!\n",
			__func__, ctx->idx);
	}


#if !defined(REDUCE_FSYNC_CTRLS_LOG)
	dev_info(ctx->dev,
		"%s: sidx:%d, cammux_id:%u/target_tg:%u, fl_delay:%u(must be 3 or 2), fl(def/max):%u/%u, def_exp:%u, lineTime:%u(pclk:%u/linelength:%u), hw_sync_mode:%u(N:0/M:1/S:2)\n",
		__func__, ctx->idx,
		s_info.cammux_id,
		s_info.target_tg,
		s_info.fl_active_delay,
		s_info.def_fl_lc,
		s_info.max_fl_lc,
		s_info.def_shutter_lc,
		s_info.lineTimeInNs,
		s_info.pclk,
		s_info.linelength,
		s_info.sync_mode);
#endif
}


/*******************************************************************************
 * per-frame ctrls
 ******************************************************************************/
int chk_s_exp_with_fl_by_fsync_mgr(struct adaptor_ctx *ctx,
	u32 *ae_exp_arr, u32 ae_exp_cnt)
{
	int ret = 0;
	int en_fsync = 0;

#if defined(TWO_STAGE_FS)

	/* not expected case */
	if (unlikely(ctx->fsync_mgr == NULL)) {

#if !defined(FORCE_DISABLE_FSYNC_MGR)
		dev_info(ctx->dev,
			"%s: sidx:%d, NOTICE: ctx->fsync_mgr is NULL, return 0\n",
			__func__, ctx->idx);
#endif

		return 0;
	}


	/* check situation */
	en_fsync = (ctx->fsync_mgr->fs_is_set_sync(ctx->idx));

	if (en_fsync) {
		fsync_mgr_chk_long_exposure(ctx, ae_exp_arr, ae_exp_cnt);
		if (atomic_read(&long_exp_mode_bits) != 0) {

#if !defined(REDUCE_FSYNC_CTRLS_LOG)
			dev_info(ctx->dev,
				"%s: sidx:%d, NOTICE: detected any en_sync sensor in long exp mode, long_exp_mode_bits:%#x => CTRL by sensor drv, return:0\n",
				__func__, ctx->idx,
				atomic_read(&long_exp_mode_bits));
#endif

			return 0;
		}
	}

#endif


	/* get result from above situation check */
	ret = en_fsync;

#if !defined(FORCE_DISABLE_FSYNC_MGR) && !defined(REDUCE_FSYNC_CTRLS_LOG)
	dev_info(ctx->dev, "%s: sidx:%d, ret:%d(en_fsync:%u)\n",
		__func__, ctx->idx,
		ret, en_fsync);
#endif

	return ret;
}

void notify_fsync_mgr_update_tg(struct adaptor_ctx *ctx, u64 val)
{
	/* not expected case */
	if (unlikely(ctx->fsync_mgr == NULL)) {

#if !defined(FORCE_DISABLE_FSYNC_MGR)
		dev_info(ctx->dev,
			"%s: sidx:%d, NOTICE: set update tg:%llu, but ctx->fsync_mgr is NULL, return\n",
			__func__, ctx->idx,
			val);
#endif

		return;
	}

	ctx->fsync_mgr->fs_update_tg(ctx->idx, val + 1);
}

/* ISP7S new add */
void notify_fsync_mgr_update_target_tg(struct adaptor_ctx *ctx, u64 val)
{
	/* not expected case */
	if (unlikely(ctx->fsync_mgr == NULL)) {

#if !defined(FORCE_DISABLE_FSYNC_MGR)
		dev_info(ctx->dev,
			"%s: sidx:%d, NOTICE: set update tg:%llu, but ctx->fsync_mgr is NULL, return\n",
			__func__, ctx->idx,
			val);
#endif

		return;
	}

	ctx->fsync_mgr->fs_update_target_tg(ctx->idx, val);
}

void notify_fsync_mgr_set_sync(struct adaptor_ctx *ctx, u64 en)
{
	/* not expected case */
	if (unlikely(ctx->fsync_mgr == NULL)) {

#if !defined(FORCE_DISABLE_FSYNC_MGR)
		dev_info(ctx->dev,
			"%s: sidx:%d, NOTICE: set sync:%llu, but ctx->fsync_mgr is NULL, return\n",
			__func__, ctx->idx,
			en);
#endif

		return;
	}

	ctx->fsync_mgr->fs_set_sync(ctx->idx, en);

	if (en == 0) {
		ctx->fsync_out_fl = 0;
		atomic_fetch_and((~(1UL << ctx->idx)), &long_exp_mode_bits);
	}
}

void notify_fsync_mgr_set_async_master(struct adaptor_ctx *ctx, const u64 en)
{
	/* not expected case */
	if (unlikely(ctx->fsync_mgr == NULL)) {

#if !defined(FORCE_DISABLE_FSYNC_MGR)
		dev_info(ctx->dev,
			"%s: sidx:%d, NOTICE: set async master:%llu, but ctx->fsync_mgr is NULL, return\n",
			__func__, ctx->idx,
			en);
#endif

		return;
	}

	ctx->fsync_mgr->fs_sa_set_user_async_master(ctx->idx, en);
}

void notify_fsync_mgr_update_auto_flicker_mode(struct adaptor_ctx *ctx, u64 en)
{
	/* not expected case */
	if (unlikely(ctx->fsync_mgr == NULL)) {

#if !defined(FORCE_DISABLE_FSYNC_MGR)
		dev_info(ctx->dev,
			"%s: sidx:%d, NOTICE: set flicker en:%llu, but ctx->fsync_mgr is NULL, return\n",
			__func__, ctx->idx,
			en);
#endif

		return;
	}

	ctx->fsync_mgr->fs_update_auto_flicker_mode(ctx->idx, en);
}

void notify_fsync_mgr_update_min_fl(struct adaptor_ctx *ctx)
{
	/* not expected case */
	if (unlikely(ctx->fsync_mgr == NULL)) {

#if !defined(FORCE_DISABLE_FSYNC_MGR)
		dev_info(ctx->dev,
			"%s: sidx:%d, NOTICE: set min fl:%u, but ctx->fsync_mgr is NULL, return\n",
			__func__, ctx->idx,
			ctx->subctx.min_frame_length);
#endif

		return;
	}

	ctx->fsync_mgr->fs_update_min_framelength_lc(ctx->idx,
					ctx->subctx.min_frame_length);
}

void notify_fsync_mgr_set_extend_framelength(
	struct adaptor_ctx *ctx, u64 ext_fl)
{
	unsigned int ext_fl_us = 0;

	/* not expected case */
	if (unlikely(ctx->fsync_mgr == NULL)) {

#if !defined(FORCE_DISABLE_FSYNC_MGR)
		dev_info(ctx->dev,
			"%s: sidx:%d, NOTICE: set extend FL:%u(ns), but ctx->fsync_mgr is NULL, return\n",
			__func__, ctx->idx,
			ext_fl);
#endif

		return;
	}

	/* ext_fl (input) is ns */
	ext_fl_us = ext_fl / 1000;

	/* args:(ident / ext_fl_lc / ext_fl_us) */
	ctx->fsync_mgr->fs_set_extend_framelength(ctx->idx, 0, ext_fl_us);
}

void notify_fsync_mgr_seamless_switch(struct adaptor_ctx *ctx,
	u32 *ae_exp_arr, u32 ae_exp_max_cnt,
	u32 orig_readout_time_us, u32 target_scenario_id)
{
	struct fs_seamless_st seamless_info = {0};
	unsigned int ae_exp_cnt = 0;
	unsigned int mode_crop_height = 0;
	unsigned int mode_linetime_readout_ns = 0;
	unsigned int i = 0;

	/* not expected case */
	if (unlikely(ctx->fsync_mgr == NULL)) {

#if !defined(FORCE_DISABLE_FSYNC_MGR)
		adaptor_logi(ctx,
			"sidx:%d, NOTICE: seamless switch, but ctx->fsync_mgr is NULL, return\n",
			ctx->idx);
#endif

		return;
	}

	if (unlikely(ae_exp_arr == NULL || ae_exp_max_cnt == 0)) {
		adaptor_logi(ctx,
			"sidx:%d, ERROR: get ae_exp_arr:%p, ae_exp_cnt:%u, return\n",
			ctx->idx,
			ae_exp_arr,
			ae_exp_max_cnt);
		return;
	}

	if (unlikely(target_scenario_id >= MODE_MAXCNT)) {
		/* not expected case */
		mode_crop_height = 0;
		mode_linetime_readout_ns = 0;

		adaptor_logi(ctx,
			"sidx:%d, ERROR target_scenario_id:%u >= MODE_MAXCNT:%u, auto set mode_crop_height:%u, mode_linetime_readout_ns:%u\n",
			target_scenario_id,
			MODE_MAXCNT,
			mode_crop_height,
			mode_linetime_readout_ns);
	} else {
		mode_crop_height = ctx->mode[target_scenario_id].height;
		mode_linetime_readout_ns =
			ctx->mode[target_scenario_id].linetime_in_ns_readout;
	}


	seamless_info.seamless_pf_ctrl.req_id = ctx->req_id;

	seamless_info.seamless_pf_ctrl.sensor_id = ctx->subdrv->id;
	seamless_info.seamless_pf_ctrl.sensor_idx = ctx->idx;

	seamless_info.seamless_pf_ctrl.min_fl_lc = ctx->subctx.min_frame_length;
	seamless_info.seamless_pf_ctrl.margin_lc =
		g_sensor_margin(ctx, target_scenario_id);
	seamless_info.seamless_pf_ctrl.flicker_en = ctx->subctx.autoflicker_en;
	seamless_info.seamless_pf_ctrl.out_fl_lc = ctx->subctx.frame_length;

	/* preventing issue (seamless switch not update ctx->cur_mode data) */
	seamless_info.seamless_pf_ctrl.pclk = ctx->subctx.pclk;
	seamless_info.seamless_pf_ctrl.linelength = ctx->subctx.line_length;
	seamless_info.seamless_pf_ctrl.lineTimeInNs =
		CALC_LINE_TIME_IN_NS(
			seamless_info.seamless_pf_ctrl.pclk,
			seamless_info.seamless_pf_ctrl.linelength);
	seamless_info.seamless_pf_ctrl.readout_time_us =
		(mode_crop_height * mode_linetime_readout_ns / 1000);

	/* calculate ae exp cnt manually */
	for (i = 0; i < ae_exp_max_cnt; ++i) {
		/* check how many non zero exp setting */
		if (*(ae_exp_arr + i) != 0)
			ae_exp_cnt++;
	}

	/* set exposure data */
	fsync_mgr_set_exp_data(ctx, &seamless_info.seamless_pf_ctrl,
		ae_exp_arr, ae_exp_cnt, target_scenario_id);

	/* set orig readout time */
	seamless_info.orig_readout_time_us = orig_readout_time_us;


	ctx->fsync_mgr->fs_seamless_switch(ctx->idx, &seamless_info, ctx->sof_cnt);


	adaptor_logd(ctx,
		"sidx:%d, exp(%u, %u/%u/%u/%u/%u, cnt(mode:%u/ae:%u), max:%u, readout_len:%u, read_margin:%u), margin:%u, min_fl:%u, flk:%u, line_time:%u(ns), readout_time_us:%u(height:%u/linetime_readout_ns:%u), orig_readout_time_us:%u, fl(fsync:%u/subctx:%u), req_id:%d, sof_cnt:%u\n",
		ctx->idx,
		seamless_info.seamless_pf_ctrl.shutter_lc,
		seamless_info.seamless_pf_ctrl.hdr_exp.exp_lc[0],
		seamless_info.seamless_pf_ctrl.hdr_exp.exp_lc[1],
		seamless_info.seamless_pf_ctrl.hdr_exp.exp_lc[2],
		seamless_info.seamless_pf_ctrl.hdr_exp.exp_lc[3],
		seamless_info.seamless_pf_ctrl.hdr_exp.exp_lc[4],
		seamless_info.seamless_pf_ctrl.hdr_exp.mode_exp_cnt,
		seamless_info.seamless_pf_ctrl.hdr_exp.ae_exp_cnt,
		ae_exp_max_cnt,
		seamless_info.seamless_pf_ctrl.hdr_exp.readout_len_lc,
		seamless_info.seamless_pf_ctrl.hdr_exp.read_margin_lc,
		seamless_info.seamless_pf_ctrl.margin_lc,
		seamless_info.seamless_pf_ctrl.min_fl_lc,
		seamless_info.seamless_pf_ctrl.flicker_en,
		seamless_info.seamless_pf_ctrl.lineTimeInNs,
		seamless_info.seamless_pf_ctrl.readout_time_us,
		mode_crop_height,
		mode_linetime_readout_ns,
		seamless_info.orig_readout_time_us,
		ctx->fsync_out_fl,
		ctx->subctx.frame_length,
		ctx->req_id,
		ctx->sof_cnt);
}

void notify_fsync_mgr_n_1_en(struct adaptor_ctx *ctx, u64 n, u64 en)
{
	/* not expected case */
	if (unlikely(ctx->fsync_mgr == NULL)) {

#if !defined(FORCE_DISABLE_FSYNC_MGR)
		dev_info(ctx->dev,
			"%s: sidx:%d, NOTICE: set N(%llu):1 en(%llu), but ctx->fsync_mgr is NULL, return\n",
			__func__, ctx->idx,
			n, en);
#endif

		return;
	}

	ctx->fsync_mgr->fs_n_1_en(ctx->idx, n, en);
}

void notify_fsync_mgr_mstream_en(struct adaptor_ctx *ctx, u64 en)
{
	/* not expected case */
	if (unlikely(ctx->fsync_mgr == NULL)) {

#if !defined(FORCE_DISABLE_FSYNC_MGR)
		dev_info(ctx->dev,
			"%s: sidx:%d, NOTICE: set mstream en:%llu, but ctx->fsync_mgr is NULL, return\n",
			__func__, ctx->idx,
			en);
#endif

		return;
	}

	ctx->fsync_mgr->fs_mstream_en(ctx->idx, en);
}

void notify_fsync_mgr_subsample_tag(struct adaptor_ctx *ctx, u64 sub_tag)
{
	/* not expected case */
	if (unlikely(ctx->fsync_mgr == NULL)) {

#if !defined(FORCE_DISABLE_FSYNC_MGR)
		dev_info(ctx->dev,
			"%s: sidx:%d, NOTICE: set subsample tag:%u, but ctx->fsync_mgr is NULL, return\n",
			__func__, ctx->idx,
			sub_tag);
#endif

		return;
	}

	if (unlikely(sub_tag < 1)) {
		dev_info(ctx->dev,
			"%s: sidx:%d, NOTICE: sub_tag:%llu should larger than 1, return\n",
			__func__, ctx->idx,
			sub_tag);

		return;
	}


#if !defined(REDUCE_FSYNC_CTRLS_LOG)
	dev_info(ctx->dev,
		"%s: sidx:%d, sub_tag %u\n",
		__func__, ctx->idx,
		sub_tag);
#endif


	ctx->fsync_mgr->fs_set_frame_tag(ctx->idx, sub_tag - 1);
}

void notify_fsync_mgr_set_shutter(struct adaptor_ctx *ctx,
	u32 *ae_exp_arr, u32 ae_exp_cnt,
	int do_set_exp_with_fl)
{
	struct fs_perframe_st pf_ctrl = {0};
	const unsigned int mode_id = ctx->subctx.current_scenario_id;

	/* not expected case */
	if (unlikely(ctx->fsync_mgr == NULL)) {

#if !defined(FORCE_DISABLE_FSYNC_MGR)
		dev_info(ctx->dev,
			"%s: sidx:%d, NOTICE: notify set shutter, but ctx->fsync_mgr is NULL, return\n",
			__func__, ctx->idx);
#endif

		return;
	}

	if (atomic_read(&long_exp_mode_bits) != 0) {
		dev_info(ctx->dev,
			"%s: sidx:%d, NOTICE: detected any sensor in long exp mode, long_exp_mode_bits:%#x => return [do_set_exp_with_fl:%d]\n",
			__func__, ctx->idx,
			atomic_read(&long_exp_mode_bits), do_set_exp_with_fl);
		return;
	}


	pf_ctrl.req_id = ctx->req_id;

	pf_ctrl.sensor_id = ctx->subdrv->id;
	pf_ctrl.sensor_idx = ctx->idx;

	pf_ctrl.min_fl_lc = ctx->subctx.min_frame_length;
	pf_ctrl.margin_lc = g_sensor_margin(ctx, mode_id);
	pf_ctrl.flicker_en = ctx->subctx.autoflicker_en;
	pf_ctrl.out_fl_lc = ctx->subctx.frame_length; // sensor current fl_lc

	/* preventing issue (seamless switch not update ctx->cur_mode data) */
	pf_ctrl.pclk = ctx->subctx.pclk;
	pf_ctrl.linelength = ctx->subctx.line_length;
	pf_ctrl.lineTimeInNs =
		CALC_LINE_TIME_IN_NS(pf_ctrl.pclk, pf_ctrl.linelength);
	pf_ctrl.readout_time_us =
		(ctx->mode[mode_id].height * ctx->mode[mode_id].linetime_in_ns_readout / 1000);

	/* set exposure data */
	fsync_mgr_set_exp_data(ctx, &pf_ctrl, ae_exp_arr, ae_exp_cnt, mode_id);


#if defined(TWO_STAGE_FS)
	pf_ctrl.cmd_id = (do_set_exp_with_fl)
		? (unsigned int)SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME
		: (unsigned int)SENSOR_FEATURE_SET_FRAMELENGTH;
#else
	pf_ctrl.cmd_id = (unsigned int)SENSOR_FEATURE_SET_FRAMELENGTH;
#endif


	/* call frame-sync fs set shutter */
	ctx->fsync_mgr->fs_set_shutter(&pf_ctrl);


#if defined(TWO_STAGE_FS)
	if (do_set_exp_with_fl) {
		/* Enable frame-sync && using SW sync (SA algo) solution */
		/* set exp with fl (ctx->fsync_out_fl) */
		fsync_mgr_s_multi_shutter_frame_length(ctx,
			ae_exp_arr, ae_exp_cnt);

		/* update sensor current fl_lc */
		pf_ctrl.out_fl_lc = ctx->subctx.frame_length;
	}


	/* update sensor current fl_lc to Frame-Sync */
	ctx->fsync_mgr->fs_update_shutter(&pf_ctrl);
#endif


	adaptor_logd(ctx,
		"sidx:%d, exp(%u, %u/%u/%u/%u/%u, cnt(mode:%u/ae:%u), readout_len:%u, read_margin:%u), margin:%u, min_fl:%u, flk:%u, line_time:%u(ns), readout_time_us:%u(mode_id:%u/height:%u/linetime_readout_ns:%u), set_exp_with_fl(%u, %u/%u), req_id:%d, sof_cnt:%u\n",
		ctx->idx,
		pf_ctrl.shutter_lc,
		pf_ctrl.hdr_exp.exp_lc[0],
		pf_ctrl.hdr_exp.exp_lc[1],
		pf_ctrl.hdr_exp.exp_lc[2],
		pf_ctrl.hdr_exp.exp_lc[3],
		pf_ctrl.hdr_exp.exp_lc[4],
		pf_ctrl.hdr_exp.mode_exp_cnt,
		pf_ctrl.hdr_exp.ae_exp_cnt,
		pf_ctrl.hdr_exp.readout_len_lc,
		pf_ctrl.hdr_exp.read_margin_lc,
		pf_ctrl.margin_lc,
		pf_ctrl.min_fl_lc,
		pf_ctrl.flicker_en,
		pf_ctrl.lineTimeInNs,
		pf_ctrl.readout_time_us,
		mode_id,
		ctx->mode[mode_id].height,
		ctx->mode[mode_id].linetime_in_ns_readout,
		do_set_exp_with_fl,
		ctx->fsync_out_fl,
		ctx->subctx.frame_length,
		ctx->req_id,
		ctx->sof_cnt);
}


void notify_fsync_mgr_sync_frame(struct adaptor_ctx *ctx,
	const unsigned int flag)
{
	/* not expected case */
	if (unlikely(ctx->fsync_mgr == NULL)) {

#if !defined(FORCE_DISABLE_FSYNC_MGR)
		adaptor_logi(ctx,
			"sidx:%d, NOTICE: notify fsync sync frame, but ctx->fsync_mgr is NULL, return\n",
			ctx->idx);
#endif

		return;
	}

	adaptor_logd(ctx, "sidx:%d, flag:%u\n", ctx->idx, flag);

	ctx->fsync_mgr->fs_sync_frame(flag);
}


/*******************************************************************************
 * ext ctrls
 ******************************************************************************/
void notify_fsync_mgr_vsync(struct adaptor_ctx *ctx)
{
	/* not expected case */
	if (unlikely(ctx->fsync_mgr == NULL)) {

#if !defined(FORCE_DISABLE_FSYNC_MGR) && !defined(REDUCE_FSYNC_CTRLS_LOG)
		dev_info(ctx->dev,
			"%s: sidx:%d, NOTICE: notify vsync, but ctx->fsync_mgr is NULL, return\n",
			__func__, ctx->idx);
#endif

		return;
	}

	ctx->fsync_mgr->fs_notify_vsync(ctx->idx, ctx->sof_cnt);
}


void notify_fsync_mgr_g_fl_record_info(struct adaptor_ctx *ctx,
	struct mtk_fs_frame_length_info *p_fl_info)
{
	/* not expected case */
	if (unlikely(ctx->fsync_mgr == NULL)) {

#if !defined(FORCE_DISABLE_FSYNC_MGR)
		adaptor_logi(ctx,
			"sidx:%d, NOTICE: notify fsync sync frame, but ctx->fsync_mgr is NULL, return\n",
			ctx->idx);
#endif

		return;
	}

	ctx->fsync_mgr->fs_get_fl_record_info(ctx->idx,
		&p_fl_info->target_min_fl_us, &p_fl_info->out_fl_us);

	adaptor_logd(ctx,
		"sidx:%d, p_fl_info(target_min_fl_us:%u, out_fl_us:%u)\n",
		ctx->idx,
		p_fl_info->target_min_fl_us,
		p_fl_info->out_fl_us);
}


/*******************************************************************************
 * init Frame-Sync Mgr / get all function calls
 ******************************************************************************/
int notify_fsync_mgr(struct adaptor_ctx *ctx, int on)
{
	int ret, seninf_idx = 0;
	const char *seninf_port = NULL;
	char c_ab;
	struct device_node *seninf_np;
	struct device *dev = ctx->dev;

	if (!on) {

#if !defined(FORCE_DISABLE_FSYNC_MGR)
		/* imgsensor remove => for remove sysfs file */
		FrameSyncUnInit(ctx->dev);
#endif

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

	/* notify frame-sync mgr of sensor-idx and seninf-idx */
#if !defined(FORCE_DISABLE_FSYNC_MGR)
	/* frame-sync init */
	ret = FrameSyncInit(&ctx->fsync_mgr, ctx->dev);
	if (ret != 0) {
		dev_info(ctx->dev,
			"%s: sidx:%d, WARNING: ctx->fsync_mgr init failed!\n",
			__func__, ctx->idx);

		ctx->fsync_mgr = NULL;
	} else {
		dev_info(dev,
			"%s: sidx:%d, ctx->fsync_mgr init done, ret:%d",
			__func__, ctx->idx,
			ret);
	}
#else
	ctx->fsync_mgr = NULL;
	dev_info(ctx->dev,
		"%s: sidx:%d, WARNING: ctx->fsync_mgr is NULL(set FORCE_DISABLE_FSYNC_MGR compile flag)\n",
		__func__, ctx->idx);
#endif

	return 0;
}
