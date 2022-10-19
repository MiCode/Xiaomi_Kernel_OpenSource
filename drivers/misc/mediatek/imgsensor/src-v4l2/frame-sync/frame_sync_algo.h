/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _FRAME_SYNC_ALG_H
#define _FRAME_SYNC_ALG_H

#include "frame_sync.h"
#include "frame_sync_util.h"


/* utility functions */
unsigned int fs_alg_get_vsync_data(unsigned int solveIdxs[], unsigned int len);


/*
 * be careful:
 *    In each frame this API should only be called at once,
 *    otherwise will cause wrong frame monitor data.
 *
 *    So calling this API at/before next vsync coming maybe a good choise.
 */
void fs_alg_setup_frame_monitor_fmeas_data(unsigned int idx);


#ifdef FS_UT
unsigned int fs_alg_write_shutter(unsigned int idx);
#endif


/* Dump & Debug function */
void fs_alg_get_cur_frec_data(unsigned int idx,
	unsigned int *p_fl_lc, unsigned int *p_shut_lc);

void fs_alg_get_fs_inst_ts_data(unsigned int idx,
	unsigned int *p_tg, unsigned int ts_arr[],
	unsigned int *p_last_vts, unsigned int *p_time_after_sof,
	unsigned int *p_cur_tick, unsigned int *p_vsyncs);

void fs_alg_dump_fs_inst_data(const unsigned int idx);
void fs_alg_dump_all_fs_inst_data(void);


void fs_alg_get_fl_rec_st_info(const unsigned int idx,
	unsigned int *p_target_min_fl_us, unsigned int *p_out_fl_us);


/*******************************************************************************
 * fs algo operation functions (set information data)
 ******************************************************************************/
void fs_alg_set_sync_type(unsigned int idx, unsigned int type);

void fs_alg_set_anti_flicker(unsigned int idx, unsigned int flag);

void fs_alg_set_extend_framelength(unsigned int idx,
	unsigned int ext_fl_lc, unsigned int ext_fl_us);

void fs_alg_seamless_switch(const unsigned int idx,
	struct fs_seamless_st *p_seamless_info,
	const struct fs_sa_cfg *p_sa_cfg);

void fs_alg_update_tg(unsigned int idx, unsigned int tg);

void fs_alg_update_min_fl_lc(unsigned int idx, unsigned int min_fl_lc);

void fs_alg_set_sync_with_diff(unsigned int idx, unsigned int diff_us);

void fs_alg_set_streaming_st_data(
	unsigned int idx, struct fs_streaming_st *pData);

void fs_alg_set_perframe_st_data(
	unsigned int idx, struct fs_perframe_st *pData);

void fs_alg_set_preset_perframe_streaming_st_data(const unsigned int idx,
	struct fs_streaming_st *p_stream_data,
	struct fs_perframe_st *p_pf_ctrl_data);

void fs_alg_set_debug_info_sof_cnt(const unsigned int idx,
	const unsigned int sof_cnt);

void fs_alg_reset_vsync_data(const unsigned int idx);

void fs_alg_reset_fs_inst(unsigned int idx);

void fs_alg_set_frame_record_st_data(
	unsigned int idx, struct frame_record_st data[]);

void fs_alg_set_frame_cell_size(unsigned int idx, unsigned int size);

void fs_alg_set_frame_tag(unsigned int idx, unsigned int count);

void fs_alg_set_n_1_on_off_flag(unsigned int idx, unsigned int flag);


void fs_alg_sa_notify_setup_all_frame_info(unsigned int idx);

void fs_alg_sa_notify_vsync(unsigned int idx);


/*******************************************************************************
 * Frame Sync Algorithm function
 ******************************************************************************/

/* return ("0" -> done; "non 0" -> error ?) */
unsigned int fs_alg_solve_frame_length(
	unsigned int solveIdxs[],
	unsigned int framelength[], unsigned int len);


#ifdef SUPPORT_FS_NEW_METHOD
/*
 * return: (0/1) for (no error/some error happened)
 *
 * input:
 *     struct fs_sa_cfg
 *         idx: standalone instance idx
 *         m_idx: master instance idx
 *         valid_sync_bits: all valid for doing frame-sync instance idxs
 *         sa_method: 0 => adaptive switch master; 1 => fix master
 *
 * output:
 *     *fl_lc: pointer for output frame length
 */
unsigned int fs_alg_solve_frame_length_sa(
	const struct fs_sa_cfg *p_sa_cfg, unsigned int *fl_lc);
#endif // SUPPORT_FS_NEW_METHOD


#endif
