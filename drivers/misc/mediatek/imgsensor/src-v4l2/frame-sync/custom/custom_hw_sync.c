// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>

#include "custom_hw_sync.h"
#include "frame_sync_log.h"
#include "frame_sync_util.h"

#define PFX "CustomHwSync"

#undef EN_CUSTOM_DBG_LOG


/* flicker table */
#define CUSTOM_FLK_TABLE_SIZE 8
static unsigned int custom_fs_flicker_table[CUSTOM_FLK_TABLE_SIZE][2] = {
	/* 14.6 ~ 15.3 */
	{68493, 65359},

	/* 24.6 ~ 25.3 */
	{40650, 39525},

	/* 29.6 ~ 30.5 */
	{33783, 32786},

	/* 59.2 ~ 60.7 */
	{16891, 16474},

	/* END */
	{0, 0}
};

static unsigned int get_anti_flicker_fl(unsigned int framelength)
{
	unsigned int i = 0;


	for (i = 0; i < CUSTOM_FLK_TABLE_SIZE; ++i) {
		if (custom_fs_flicker_table[i][0] == 0)
			break;

		if ((custom_fs_flicker_table[i][0] > framelength) &&
			(framelength >= custom_fs_flicker_table[i][1])) {

			framelength = custom_fs_flicker_table[i][0];
			break;
		}
	}


	return framelength;
}

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

#ifdef EN_CUSTOM_DBG_LOG
		LOG_MUST(
			"sensor_idx(%u), #%u, sensor_type(%u), hw_sync:%u(N:0/M:1/S:2), line_time_ns(%u), shutter_lc(%u), min_fl_lc(%u), margin_lc(%u)\n",
			para->sensor_idx,
			para->magic_num,
			para->sensor_type,
			para->sync_mode,
			para->line_time_in_ns,
			para->shutter_lc,
			para->min_fl_lc,
			para->sensor_margin_lc);
#endif // EN_CUSTOM_DBG_LOG

		min_fl = max((para->shutter_lc + para->sensor_margin_lc),
			     para->min_fl_lc);
		tmp = convert2TotalTime(para->line_time_in_ns, min_fl);

		if (para->flicker_en)
			tmp = get_anti_flicker_fl(tmp);

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

#ifdef EN_CUSTOM_DBG_LOG
		LOG_MUST("sensor_idx(%u), #%u, out_fl:%u(%u), hw_sync:%u(N:0/M:1/S:2)\n",
			para->sensor_idx,
			para->magic_num,
			convert2TotalTime(
				para->line_time_in_ns,
				para->out_fl_lc),
			para->out_fl_lc,
			para->sync_mode);
#endif // EN_CUSTOM_DBG_LOG

	}

	return 0;
}

