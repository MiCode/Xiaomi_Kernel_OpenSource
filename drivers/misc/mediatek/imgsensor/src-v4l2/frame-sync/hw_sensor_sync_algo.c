// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifdef FS_UT
#include <string.h>
#include <stdlib.h>         /* Needed by memory allocate */
#else
/* INSTEAD of using stdio.h, you have to use the following include */
#include <linux/slab.h>     /* Needed by memory allocate */
#include <linux/string.h>
#endif // FS_UT

#include "frame_sync_log.h"
#include "frame_sync_util.h"
#include "frame_sync_algo.h"
#include "frame_monitor.h"
#include "hw_sensor_sync_algo.h"
#include "custom/custom_hw_sync.h"

#if !defined(FS_UT)
#include "kd_imgsensor_define_v4l2.h"
#endif // FS_UT

#define PFX "HwSensorSyncAlgo"

#undef EN_DBG_LOG

struct HwSyncSensorInfo {
	unsigned int sensor_id;       // imx586 -> 0x0586; s5k3m5sx -> 0x30D5
	unsigned int sensor_idx;      // main1 -> 0; sub1 -> 1;
	unsigned int fl_active_delay; // SONY/auto_ext:(3, 1); others:(2, 0);
	unsigned int sync_mode;       // sync operate mode. none/master/slave
	unsigned int sync_group_id;
	unsigned int line_time_in_ns; // ~= 10^9 * (linelength/pclk)
	unsigned int min_fl_lc;       // dynamic FPS using
	unsigned int shutter_lc;
	unsigned int margin_lc;
	unsigned int flicker_en;
	unsigned int out_fl_lc;

	unsigned int magic_num;       // for debug using
	unsigned int act_cnt;         // for debug using
};

static struct HwSyncSensorInfo sensor_infos[SENSOR_MAX_NUM];

void
hw_fs_dump_dynamic_para(unsigned int idx)
{
	unsigned int tg = 0, fl_lc = 0, shut_lc = 0;
	unsigned int fmeas_idx = 0, pr_fl_us = 0, pr_fl_lc = 0, act_fl_us = 0;
	unsigned int last_vts = 0, time_after_sof = 0, cur_tick = 0, vsyncs = 0;
	unsigned int fmeas_ts[VSYNCS_MAX] = {0}, ts_arr[VSYNCS_MAX] = {0};

	fs_alg_get_cur_frec_data(idx, &fl_lc, &shut_lc);

	fs_alg_get_fs_inst_ts_data(idx,
		 &tg, ts_arr, &last_vts, &time_after_sof, &cur_tick, &vsyncs);

	frm_get_curr_frame_mesurement_and_ts_data(idx,
		&fmeas_idx, &pr_fl_us, &pr_fl_lc, &act_fl_us, fmeas_ts);

	LOG_MUST(
		"ID:%#x(sidx:%u), #%u(act:%u:%u), out_fl:%u(%u), (%u/%u/%u(%u), %u), flk_en:%u, hw_sync(%u(N:0/M:1/S:2), groupID:%u), fdelay:%u [frec(0:%u/%u)(fl_lc/shut_lc), tg:%u, fmeas:%u(pr:%u(%u)/act:%u), fmeas_ts(%u/%u/%u/%u), fs_inst_ts(%u/%u/%u/%u, %u/+%u(%u)/%u)]\n",
		sensor_infos[idx].sensor_id,
		sensor_infos[idx].sensor_idx,
		sensor_infos[idx].magic_num,
		sensor_infos[idx].sync_group_id,
		sensor_infos[idx].act_cnt,
		convert2TotalTime(
			sensor_infos[idx].line_time_in_ns,
			sensor_infos[idx].out_fl_lc),
		sensor_infos[idx].out_fl_lc,
		sensor_infos[idx].shutter_lc,
		sensor_infos[idx].margin_lc,
		sensor_infos[idx].min_fl_lc,
		convert2TotalTime(
			sensor_infos[idx].line_time_in_ns,
			sensor_infos[idx].min_fl_lc),
		sensor_infos[idx].line_time_in_ns,
		sensor_infos[idx].flicker_en,
		sensor_infos[idx].sync_mode,
		sensor_infos[idx].sync_group_id,
		sensor_infos[idx].fl_active_delay,
		fl_lc,
		shut_lc,
		tg,
		fmeas_idx,
		pr_fl_us,
		pr_fl_lc,
		act_fl_us,
		fmeas_ts[0],
		fmeas_ts[1],
		fmeas_ts[2],
		fmeas_ts[3],
		ts_arr[0],
		ts_arr[1],
		ts_arr[2],
		ts_arr[3],
		last_vts,
		time_after_sof,
		cur_tick,
		vsyncs);
}

void
hw_fs_alg_set_streaming_st_data(unsigned int idx, struct fs_streaming_st (*pData))
{
	sensor_infos[idx].sensor_id = pData->sensor_id;
	sensor_infos[idx].sensor_idx = pData->sensor_idx;
	sensor_infos[idx].fl_active_delay = pData->fl_active_delay;
	sensor_infos[idx].sync_mode = pData->sync_mode;
	sensor_infos[idx].sync_group_id = pData->hw_sync_group_id;

	/* clear/reset magic num & act cnt */
	sensor_infos[idx].magic_num = 0;
	sensor_infos[idx].act_cnt = 0;

	if (sensor_infos[idx].fl_active_delay < 2
		|| sensor_infos[idx].fl_active_delay > 3) {

		LOG_MUST(
			"ERROR: [%u] ID:%#x(sidx:%u), get non valid frame_time_delay_frame:%u (must be 2 or 3), plz check sensor driver\n",
			idx,
			sensor_infos[idx].sensor_id,
			sensor_infos[idx].sensor_idx,
			sensor_infos[idx].fl_active_delay);
	}

#ifdef EN_DBG_LOG
	LOG_MUST(
		"s_idx(%u),id(0x%x),#%u(act:%u)(reset),fl_dalay(%u),sync_mode(%u),sync_group_ID(%u)\n",
		sensor_infos[idx].sensor_idx,
		sensor_infos[idx].sensor_id,
		sensor_infos[idx].magic_num,
		sensor_infos[idx].act_cnt,
		sensor_infos[idx].fl_active_delay,
		sensor_infos[idx].sync_mode,
		sensor_infos[idx].sync_group_id);
#endif
}

void
hw_fs_alg_update_min_fl_lc(unsigned int idx, unsigned int min_fl_lc)
{
	if (sensor_infos[idx].min_fl_lc != min_fl_lc) {
		/* min_fl_lc was changed after set shutter, so update it */
		sensor_infos[idx].min_fl_lc = min_fl_lc;

#ifdef EN_DBG_LOG
		LOG_MUST("[%u] ID:%#x(sidx:%u), updated min_fl:%u(%u)\n",
			idx,
			sensor_infos[idx].sensor_id,
			sensor_infos[idx].sensor_idx,
			convert2TotalTime(
				sensor_infos[idx].line_time_in_ns,
				sensor_infos[idx].min_fl_lc),
			sensor_infos[idx].min_fl_lc);
#endif // EN_DBG_LOG
	}
}

void
hw_fs_alg_set_perframe_st_data(unsigned int idx, struct fs_perframe_st (*pData))
{
	sensor_infos[idx].min_fl_lc = pData->min_fl_lc;
	sensor_infos[idx].shutter_lc = pData->shutter_lc;
	sensor_infos[idx].margin_lc = pData->margin_lc;
	sensor_infos[idx].line_time_in_ns = pData->lineTimeInNs;
	sensor_infos[idx].flicker_en = pData->flicker_en;

	/* increase magic num */
	sensor_infos[idx].magic_num++;

	if (sensor_infos[idx].margin_lc == 0) {
		LOG_MUST(
			"WARNING: [%u] ID:%#x(sidx:%u), get non valid margin_lc:%u, plz check sensor driver\n",
			idx,
			sensor_infos[idx].sensor_id,
			sensor_infos[idx].sensor_idx,
			sensor_infos[idx].margin_lc);
	}

#ifdef EN_DBG_LOG
	LOG_MUST(
		"s_idx(%u),id(0x%x),#%u,min_fl(%u),shutter(%u),margin(%u),line_time(%u),flicker_en(%u)\n",
		sensor_infos[idx].sensor_idx,
		sensor_infos[idx].sensor_id,
		sensor_infos[idx].magic_num,
		sensor_infos[idx].min_fl_lc,
		sensor_infos[idx].shutter_lc,
		sensor_infos[idx].margin_lc,
		sensor_infos[idx].line_time_in_ns);
#endif
}

int
handle_by_hw_sensor_sync(unsigned int solveIdxs[], unsigned int len)
{
	int i;
	unsigned int idx;
	int has_none = 0;
	int has_master = 0;
	int has_slave = 0;

	/* Check if master & slave sync mode */
	for (i = 0; i < len; ++i) {
		idx = solveIdxs[i];
		if (sensor_infos[idx].sync_mode == SENSOR_MASTER_SYNC_MODE)
			has_master = 1;
		else if (sensor_infos[idx].sync_mode == SENSOR_SLAVE_SYNC_MODE)
			has_slave = 1;
		else
			has_none = 1;
	}

	if (has_none || !(has_master && has_slave)) {
#ifdef EN_DBG_LOG
		LOG_INF("No master/slave mode paring. Unhandle by hw sensor sync");
#endif
		return 0;
	}

	return 1;
}

unsigned int
hw_fs_alg_solve_frame_length(
	unsigned int solveIdxs[],
	unsigned int framelength_lc[], unsigned int len)
{
	int i, ret = 0;
	unsigned int idx;
	struct SyncSensorPara para[SENSOR_MAX_NUM];
	char *log_buf = NULL;

#ifdef FS_UT
	log_buf = calloc(LOG_BUF_STR_LEN, sizeof(char));
#else
	log_buf = kcalloc(LOG_BUF_STR_LEN, sizeof(char), GFP_KERNEL);
#endif // FS_UT

	if (log_buf == NULL) {
		LOG_MUST(
			"ERROR: log_buf allocate memory failed\n");

		return 1;
	}

	log_buf[0] = '\0';


	/* Handle by hw sensor sync */
	for (i = 0; i < len; ++i) {
		idx = solveIdxs[i];
		para[i].sensor_idx = sensor_infos[idx].sensor_idx;

		switch (sensor_infos[idx].sync_mode) {
		case SENSOR_MASTER_SYNC_MODE:
			para[i].sync_mode = SENSOR_SYNC_MASTER;
			break;
		case SENSOR_SLAVE_SYNC_MODE:
			para[i].sync_mode = SENSOR_SYNC_SLAVE;
			break;
		default:
			para[i].sync_mode = SENSOR_SYNC_NONE;
			break;
		}

		if (sensor_infos[idx].fl_active_delay == 3) {
			/* frame length N+2 take effect */
			para[i].sensor_type = SENSOR_N_2;
		} else {
			/* frame length N+1 take effect */
			para[i].sensor_type = SENSOR_N_1;
		}

		para[i].sync_group_id = sensor_infos[idx].sync_group_id;

		para[i].line_time_in_ns = sensor_infos[idx].line_time_in_ns;
		para[i].shutter_lc = sensor_infos[idx].shutter_lc;
		para[i].min_fl_lc = sensor_infos[idx].min_fl_lc;
		para[i].sensor_margin_lc = sensor_infos[idx].margin_lc;
		para[i].flicker_en = sensor_infos[idx].flicker_en;

		para[i].out_fl_lc = 0;

		para[i].magic_num = sensor_infos[idx].magic_num;
	}

	/* call custom hw sensor sync frame length calculator */
	custom_frame_time_calculator(para, len);

	/* copy results */
	for (i = 0; i < len; ++i) {
		idx = solveIdxs[i];

		framelength_lc[i] = para[i].out_fl_lc;
		sensor_infos[idx].out_fl_lc = para[i].out_fl_lc;
		sensor_infos[idx].act_cnt++;

		ret = snprintf(log_buf + strlen(log_buf),
			LOG_BUF_STR_LEN - strlen(log_buf),
			"s_idx:%u(#%u(act:%u:%u), out_fl:%u(%u), hw_sync(%u(N:0/M:1/S:2), groupID:%u), (%u/%u/%u(%u), %u), flk_en:%u, s_type:%u); ",
			para[i].sensor_idx,
			para[i].magic_num,
			para[i].sync_group_id,
			sensor_infos[idx].act_cnt,
			convert2TotalTime(
				para[i].line_time_in_ns,
				para[i].out_fl_lc),
			para[i].out_fl_lc,
			para[i].sync_mode,
			para[i].sync_group_id,
			para[i].shutter_lc,
			para[i].sensor_margin_lc,
			para[i].min_fl_lc,
			convert2TotalTime(
				para[i].line_time_in_ns,
				para[i].min_fl_lc),
			para[i].line_time_in_ns,
			para[i].flicker_en,
			para[i].sensor_type);

		if (ret < 0)
			LOG_MUST("ERROR: LOG encoding error, ret:%d\n", ret);

	}

// #ifdef EN_DBG_LOG
	LOG_MUST("%s\n", log_buf);
// #endif


#ifdef FS_UT
	free(log_buf);
#else
	kfree(log_buf);
#endif // FS_UT


	return 0;
}

