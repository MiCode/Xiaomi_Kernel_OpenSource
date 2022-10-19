/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __ADAPTOR_FSYNC_CTRLS_H__
#define __ADAPTOR_FSYNC_CTRLS_H__


/* !!! ONLY for testing or bypass fsync mgr !!! */
// #define FORCE_DISABLE_FSYNC_MGR


/*******************************************************************************
 * call back function for Frame-Sync set frame length using
 ******************************************************************************/
/* return: 0 => No-Error ; non-0 => Error */
int cb_fsync_mgr_set_framelength(void *p_ctx,
	unsigned int cmd_id, unsigned int framelength);


/*******************************************************************************
 * streaming ctrls
 ******************************************************************************/
void notify_fsync_mgr_streaming(struct adaptor_ctx *ctx, unsigned int flag);


/*******************************************************************************
 * per-frame ctrl
 ******************************************************************************/
/*
 * return:
 *     1 => fsync_mgr will use set multi shutter frame length
 *          to set exposure and frame length simultaneously.
 *     0 => sensor adaptor directly set this ctrls to driver.
 *          #ifndef (TWO_STAGE_FS) => must return 0
 *          long exposure => must return 0
 */
int chk_s_exp_with_fl_by_fsync_mgr(struct adaptor_ctx *ctx,
	u32 *ae_exp_arr, u32 ae_exp_cnt);

void notify_fsync_mgr_update_tg(struct adaptor_ctx *ctx, u64 val);
void notify_fsync_mgr_update_target_tg(struct adaptor_ctx *ctx, u64 val);

void notify_fsync_mgr_set_sync(struct adaptor_ctx *ctx, u64 en);

void notify_fsync_mgr_set_async_master(struct adaptor_ctx *ctx, const u64 en);

void notify_fsync_mgr_update_auto_flicker_mode(struct adaptor_ctx *ctx, u64 en);

void notify_fsync_mgr_update_min_fl(struct adaptor_ctx *ctx);

void notify_fsync_mgr_set_extend_framelength(
	struct adaptor_ctx *ctx, u64 ext_fl);

void notify_fsync_mgr_seamless_switch(struct adaptor_ctx *ctx,
	u32 *ae_exp_arr, u32 ae_exp_max_cnt,
	u32 orig_readout_time_us, u32 target_scenario_id);

void notify_fsync_mgr_n_1_en(struct adaptor_ctx *ctx, u64 n, u64 en);

void notify_fsync_mgr_mstream_en(struct adaptor_ctx *ctx, u64 en);

void notify_fsync_mgr_subsample_tag(struct adaptor_ctx *ctx, u64 sub_tag);

void notify_fsync_mgr_set_shutter(struct adaptor_ctx *ctx,
	u32 *ae_exp_arr, u32 ae_exp_cnt,
	int do_set_exp_with_fl);


/* this API only be used (NOT directly return) on HW senor sync pin */
void notify_fsync_mgr_sync_frame(struct adaptor_ctx *ctx,
	const unsigned int flag);


/*******************************************************************************
 * ext ctrl
 ******************************************************************************/
void notify_fsync_mgr_vsync(struct adaptor_ctx *ctx);


void notify_fsync_mgr_g_fl_record_info(struct adaptor_ctx *ctx,
	struct mtk_fs_frame_length_info *p_fl_info);


/*******************************************************************************
 * init Frame-Sync Mgr / get all function calls
 ******************************************************************************/
int notify_fsync_mgr(struct adaptor_ctx *ctx, int on);

#endif
