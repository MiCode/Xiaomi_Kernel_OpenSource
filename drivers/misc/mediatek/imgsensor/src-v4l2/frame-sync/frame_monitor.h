/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _FRAME_MONITOR_H
#define _FRAME_MONITOR_H

#include "frame_sync_def.h"


/*******************************************************************************
 * CCU rpmsg data structure
 ******************************************************************************/
/* Sync To: "ccu_control_extif.h" */
#define CCU_CAM_TG_MIN 1
#define CCU_CAM_TG_MAX 4

/* Sync To: "ccu_platform.h" */
#define CAMSV_MAX       (10)
#define CAMSV_TG_MIN    (5)
#define CAMSV_TG_MAX    (CAMSV_TG_MIN+CAMSV_MAX)
#define FM_TG_CNT       (CAMSV_TG_MAX)


/* for per sensor */
#define VSYNCS_MAX 4
struct vsync_time {
	unsigned int id;        // tg (min is 1, start from 1)
	unsigned int vsyncs;    // how many vsyncs have been passed
				// since last call to CCU ?
	unsigned int timestamps[VSYNCS_MAX];
};

/* for per Rproc IPC send */
/* TODO : add a general param for array size, and sync this for fs, algo, fm */
#define TG_MAX_NUM (CCU_CAM_TG_MAX - CCU_CAM_TG_MIN)
struct vsync_rec {
	unsigned int ids;
	unsigned int cur_tick;
	unsigned int tick_factor;
	struct vsync_time recs[TG_MAX_NUM];
};
/******************************************************************************/
#define MSG_TO_CCU_RESET_VSYNC_TIMESTAMP 0
#define MSG_TO_CCU_CLEAR_VSYNC_TIMESTAMP 1
#define MSG_TO_CCU_GET_VSYNC_TIMESTAMP 2
/******************************************************************************/





/*******************************************************************************
 * frame monitor function
 ******************************************************************************/
#ifdef USING_CCU
void frm_power_on_ccu(unsigned int flag);

void frm_reset_ccu_vsync_timestamp(unsigned int idx, unsigned int en);

unsigned int frm_get_ccu_pwn_cnt(void);
#endif


void frm_init_frame_info_st_data(
	unsigned int idx,
	unsigned int sensor_id, unsigned int sensor_idx, unsigned int tg);

void frm_reset_frame_info(unsigned int idx);

unsigned int frm_convert_cammux_tg_to_ccu_tg(unsigned int tg);

void frm_update_tg(unsigned int idx, unsigned int tg);


/*
 * return: (0/non 0) for (done/error)
 *
 * input:
 *     tgs -> all TG you want to get vsync from CCU;
 *     len -> array length;
 */
unsigned int frm_query_vsync_data(
	unsigned int tgs[], unsigned int len, struct vsync_rec *pData);


void frm_set_frame_measurement(
	unsigned int idx, unsigned int passed_vsyncs,
	unsigned int curr_fl_us, unsigned int curr_fl_lc,
	unsigned int next_fl_us, unsigned int next_fl_lc);


void frm_get_curr_frame_mesurement_and_ts_data(
	const unsigned int idx, unsigned int *p_fmeas_idx,
	unsigned int *p_pr_fl_us, unsigned int *p_pr_fl_lc,
	unsigned int *p_act_fl_us, unsigned int *p_ts_arr);


int frm_timestamp_checker(unsigned int m_tg, unsigned int s_tg);


#ifdef FS_UT
/*******************************************************************************
 * !!! please only use bellow function on software debug or ut_test !!!
 ******************************************************************************/
/* only for FrameSync Driver and ut test used */
int frm_get_instance_idx_by_tg(unsigned int tg);

void frm_update_predicted_curr_fl_us(unsigned int idx, unsigned int fl_us);

void frm_update_next_vts_bias_us(unsigned int idx, unsigned int vts_bias);

void frm_set_sensor_curr_fl_us(unsigned int idx, unsigned int fl_us);

unsigned int frm_get_predicted_curr_fl_us(unsigned int idx);

void frm_get_predicted_fl_us(
	unsigned int idx,
	unsigned int fl_us[], unsigned int *sensor_curr_fl_us);

void frm_get_next_vts_bias_us(unsigned int idx, unsigned int *vts_bias);

void frm_debug_set_last_vsync_data(struct vsync_rec *v_rec);
#endif // FS_UT


#endif
