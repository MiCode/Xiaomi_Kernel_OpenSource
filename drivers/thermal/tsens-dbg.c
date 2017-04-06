/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/arch_timer.h>
#include "tsens.h"

/* debug defines */
#define	TSENS_DBG_BUS_ID_0			0
#define	TSENS_DBG_BUS_ID_1			1
#define	TSENS_DBG_BUS_ID_2			2
#define	TSENS_DBG_BUS_ID_15			15
#define	TSENS_DEBUG_LOOP_COUNT_ID_0		2
#define	TSENS_DEBUG_LOOP_COUNT			5
#define	TSENS_DEBUG_STATUS_REG_START		10
#define	TSENS_DEBUG_OFFSET_RANGE		16
#define	TSENS_DEBUG_OFFSET_WORD1		0x4
#define	TSENS_DEBUG_OFFSET_WORD2		0x8
#define	TSENS_DEBUG_OFFSET_WORD3		0xc
#define	TSENS_DEBUG_OFFSET_ROW			0x10
#define	TSENS_DEBUG_DECIDEGC			-950
#define	TSENS_DEBUG_CYCLE_MS			64
#define	TSENS_DEBUG_POLL_MS			200
#define	TSENS_DEBUG_BUS_ID2_MIN_CYCLE		50
#define	TSENS_DEBUG_BUS_ID2_MAX_CYCLE		51
#define	TSENS_DEBUG_ID_MASK_1_4			0xffffffe1
#define	DEBUG_SIZE				10

#define TSENS_DEBUG_CONTROL(n)			((n) + 0x1130)
#define TSENS_DEBUG_DATA(n)			((n) + 0x1134)

struct tsens_dbg_func {
	int (*dbg_func)(struct tsens_device *, u32, u32, int *);
};

static int tsens_dbg_log_temp_reads(struct tsens_device *data, u32 id,
					u32 dbg_type, int *temp)
{
	struct tsens_sensor *sensor;
	struct tsens_device *tmdev = NULL;
	u32 idx = 0;

	if (!data)
		return -EINVAL;

	pr_debug("%d %d\n", id, dbg_type);
	tmdev = data;
	sensor = &tmdev->sensor[id];
	idx = tmdev->tsens_dbg.sensor_dbg_info[sensor->hw_id].idx;
	tmdev->tsens_dbg.sensor_dbg_info[sensor->hw_id].temp[idx%10] = *temp;
	tmdev->tsens_dbg.sensor_dbg_info[sensor->hw_id].time_stmp[idx%10] =
					sched_clock();
	idx++;
	tmdev->tsens_dbg.sensor_dbg_info[sensor->hw_id].idx = idx;

	return 0;
}

static int tsens_dbg_log_interrupt_timestamp(struct tsens_device *data,
						u32 id, u32 dbg_type, int *val)
{
	struct tsens_device *tmdev = NULL;
	u32 idx = 0;

	if (!data)
		return -EINVAL;

	pr_debug("%d %d\n", id, dbg_type);
	tmdev = data;
	/* debug */
	idx = tmdev->tsens_dbg.tsens_thread_iq_dbg.idx;
	tmdev->tsens_dbg.tsens_thread_iq_dbg.dbg_count[idx%10]++;
	tmdev->tsens_dbg.tsens_thread_iq_dbg.time_stmp[idx%10] =
							sched_clock();
	tmdev->tsens_dbg.tsens_thread_iq_dbg.idx++;

	return 0;
}

static struct tsens_dbg_func dbg_arr[] = {
	[TSENS_DBG_LOG_TEMP_READS] = {tsens_dbg_log_temp_reads},
	[TSENS_DBG_LOG_INTERRUPT_TIMESTAMP] = {
			tsens_dbg_log_interrupt_timestamp},
};

int tsens2xxx_dbg(struct tsens_device *data, u32 id, u32 dbg_type, int *val)
{
	if (dbg_type >= TSENS_DBG_LOG_MAX)
		return -EINVAL;

	dbg_arr[dbg_type].dbg_func(data, id, dbg_type, val);

	return 0;
}
EXPORT_SYMBOL(tsens2xxx_dbg);
