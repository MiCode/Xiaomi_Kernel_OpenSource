/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _CUSTOM_HW_SYNC_H
#define _CUSTOM_HW_SYNC_H

#include "hw_sensor_sync_algo.h"

enum SENSOR_SYNC_MODE {
	SENSOR_SYNC_NONE,
	SENSOR_SYNC_MASTER,
	SENSOR_SYNC_SLAVE,
};

enum SENSOR_FL_EFFECT_TYPE {
	SENSOR_N_1,
	SENSOR_N_2,
};

struct SyncSensorPara {
	unsigned int sensor_idx;      // main1 -> 0; sub1 -> 1;
	enum SENSOR_FL_EFFECT_TYPE sensor_type;
	enum SENSOR_SYNC_MODE sync_mode;
	unsigned int line_time_in_ns; // ~= 10^9 * (linelength/pclk)
	unsigned int shutter_lc;
	unsigned int min_fl_lc;
	unsigned int sensor_margin_lc;
	unsigned int out_fl_lc;
};

int custom_frame_time_calculator(
	struct SyncSensorPara sensor_paras[], unsigned int len);

#endif
