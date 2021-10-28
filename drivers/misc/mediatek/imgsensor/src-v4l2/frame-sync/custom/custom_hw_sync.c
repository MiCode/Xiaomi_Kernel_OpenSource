// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>

#include "custom_hw_sync.h"
#include "frame_sync_log.h"
#include "frame_sync_util.h"

#define PFX "CustomHwSync"

/*
 * Sample code for normal single exposure sensor
 * Set all sensor's frame time to the max frame time of sensors
 *
 * This function is custom-able if need
 */
int custom_frame_time_calculator(
	struct SyncSensorPara sensor_paras[], unsigned int len)
{
	int i;
	unsigned int tmp;
	unsigned int min_fl;
	unsigned int max_frame_time = 0;
	struct SyncSensorPara *para;

	/* Test parameter */
	if (!sensor_paras) {
		LOG_PR_WARN("The parameter sensor_pars is invalid\n");
		return 1;
	}

	/* calculate min frame time for all */
	for (i = 0; i < len; ++i) {
		para = &sensor_paras[i];

		LOG_INF(
			"sensor idx(%u), type(%u), sync_mode(%u), line_time_ns(%u), shutter_lc(%u), min_fl_lc(%u), margin_lc(%u)\n",
			para->sensor_idx,
			para->sensor_type,
			para->sync_mode,
			para->line_time_in_ns,
			para->shutter_lc,
			para->min_fl_lc,
			para->sensor_margin_lc);

		min_fl = max((para->shutter_lc + para->sensor_margin_lc),
			     para->min_fl_lc);
		tmp = convert2TotalTime(para->line_time_in_ns, min_fl);

		if (tmp > max_frame_time)
			max_frame_time = tmp;
	}

	/* calculate min frame time for all */
	for (i = 0; i < len; ++i) {
		para = &sensor_paras[i];

		para->out_fl_lc = convert2LineCount(para->line_time_in_ns, max_frame_time);

		/* make master sensor slightly larger than slave */
		if (para->sync_mode == SENSOR_SYNC_MASTER)
			++(para->out_fl_lc);

		LOG_INF("sensor_idx(%u), out_fl_lc = %u\n",
			para->sensor_idx, para->out_fl_lc);
	}

	return 0;
}

