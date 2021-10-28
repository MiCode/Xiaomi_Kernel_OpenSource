// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "frame_sync_log.h"
#include "frame_sync_util.h"
#include "hw_sensor_sync_algo.h"
#include "custom/custom_hw_sync.h"
#include "kd_imgsensor_define_v4l2.h"

#define PFX "HwSensorSyncAlgo"

#undef EN_DBG_LOG

struct HwSyncSensorInfo {
	unsigned int sensor_id;       // imx586 -> 0x0586; s5k3m5sx -> 0x30D5
	unsigned int sensor_idx;      // main1 -> 0; sub1 -> 1;
	unsigned int fl_active_delay; // SONY/auto_ext:(3, 1); others:(2, 0);
	unsigned int sync_mode;       // sync operate mode. none/master/slave
	unsigned int line_time_in_ns; // ~= 10^9 * (linelength/pclk)
	unsigned int min_fl_lc;       // dynamic FPS using
	unsigned int shutter_lc;
	unsigned int margin_lc;
};

static struct HwSyncSensorInfo sensor_infos[SENSOR_MAX_NUM];

void
hw_fs_alg_set_streaming_st_data(unsigned int idx, struct fs_streaming_st (*pData))
{
	sensor_infos[idx].sensor_id = pData->sensor_id;
	sensor_infos[idx].sensor_idx = pData->sensor_idx;
	sensor_infos[idx].fl_active_delay = pData->fl_active_delay;
	sensor_infos[idx].sync_mode = pData->sync_mode;

	if (sensor_infos[idx].fl_active_delay < 2
		|| sensor_infos[idx].fl_active_delay > 3) {

		LOG_INF(
			"ERROR: [%u] ID:%#x(sidx:%u), get non valid frame_time_delay_frame:%u (must be 2 or 3)\n",
			idx,
			sensor_infos[idx].sensor_id,
			sensor_infos[idx].sensor_idx,
			sensor_infos[idx].fl_active_delay);
	}

#ifdef EN_DBG_LOG
	LOG_INF("streaming data idx(%u),id(0x%x),fl_dalay(%u),sync_mode(%u)\n",
		sensor_infos[idx].sensor_idx,
		sensor_infos[idx].sensor_id,
		sensor_infos[idx].fl_active_delay,
		sensor_infos[idx].sync_mode);
#endif
}

void
hw_fs_alg_update_min_fl_lc(unsigned int idx, unsigned int min_fl_lc)
{
	if (sensor_infos[idx].min_fl_lc != min_fl_lc) {
		/* min_fl_lc was changed after set shutter, so update it */
		sensor_infos[idx].min_fl_lc = min_fl_lc;


		LOG_INF("[%u] ID:%#x(sidx:%u), updated min_fl:%u(%u)\n",
			idx,
			sensor_infos[idx].sensor_id,
			sensor_infos[idx].sensor_idx,
			convert2TotalTime(
				sensor_infos[idx].line_time_in_ns,
				sensor_infos[idx].min_fl_lc),
			sensor_infos[idx].min_fl_lc);
	}
}

void
hw_fs_alg_set_perframe_st_data(unsigned int idx, struct fs_perframe_st (*pData))
{
	sensor_infos[idx].min_fl_lc = pData->min_fl_lc;
	sensor_infos[idx].shutter_lc = pData->shutter_lc;
	sensor_infos[idx].margin_lc = pData->margin_lc;
	sensor_infos[idx].line_time_in_ns = pData->lineTimeInNs;

	if (sensor_infos[idx].margin_lc == 0) {
		LOG_INF("WARNING: [%u] ID:%#x(sidx:%u), margin_lc:%u\n",
			idx,
			sensor_infos[idx].sensor_id,
			sensor_infos[idx].sensor_idx,
			sensor_infos[idx].margin_lc);
	}

#ifdef EN_DBG_LOG
	LOG_INF("streaming data idx(%u),id(0x%x),min_fl(%u),shutter(%u),margin(%u),line_time(%u)\n",
		sensor_infos[idx].sensor_idx,
		sensor_infos[idx].sensor_id,
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
	int i;
	unsigned int idx;
	struct SyncSensorPara para[SENSOR_MAX_NUM];

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

		para[i].line_time_in_ns = sensor_infos[idx].line_time_in_ns;
		para[i].shutter_lc = sensor_infos[idx].shutter_lc;
		para[i].min_fl_lc = sensor_infos[idx].min_fl_lc;
		para[i].sensor_margin_lc = sensor_infos[idx].margin_lc;

		para[i].out_fl_lc = 0;
	}

	/* call custom hw sensor sync frame length calculator */
	custom_frame_time_calculator(para, len);

	/* copy results */
	for (i = 0; i < len; ++i) {
		framelength_lc[i] = para[i].out_fl_lc;

#ifdef EN_DBG_LOG
		LOG_INF("sensor_idx(%u), out_fl_lc = %u\n",
			para[i].sensor_idx, para[i].out_fl_lc);
#endif
	}

	return 0;
}

