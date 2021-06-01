/*
 * Copyright (C) 2021 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/mutex.h>

#include "n3d_util.h"
#include "vsync_recorder.h"

#define CLK_TO_TIME_IN_US(x) ((x) / N3D_CLK_FREQ)
#define ONE_MIN_SEC (1000 * N3D_CLK_FREQ)

struct Vtimestamp {
	int cammux_id;
	unsigned int timestamps[MAX_RECORD_NUM];
	int w_cur_pos;
	int unread_cnt;
};

static DEFINE_MUTEX(global_mutex);
static unsigned int global_time;
static int first_time_diff;
static struct Vtimestamp records[MAX_RECORD_SENSOR];

int reset_recorder(int cammux_id1, int cammux_id2)
{
	int i, j;

	mutex_lock(&global_mutex);

	global_time = 0;
	first_time_diff = 2;

	for (i = 0; i < ARRAY_SIZE(records); i++) {
		records[i].cammux_id = -1;
		records[i].w_cur_pos = -1;
		records[i].unread_cnt = 0;
		for (j = 0; j < MAX_RECORD_NUM; j++)
			records[i].timestamps[j] = 0;
	}
	records[0].cammux_id = cammux_id1;
	records[1].cammux_id = cammux_id2;

	mutex_unlock(&global_mutex);

	return 0;
}

static int record_vs(unsigned int idx, unsigned int tclk)
{
	int w_pos;
	unsigned int pre_time;
	unsigned int ts = CLK_TO_TIME_IN_US(tclk);

	if (global_time != 0) {
		w_pos = records[idx].w_cur_pos;
		if (w_pos == -1)
			pre_time = global_time;
		else
			pre_time = records[idx].timestamps[w_pos];
		w_pos = (w_pos + 1) % MAX_RECORD_NUM;
		records[idx].timestamps[w_pos] = pre_time + ts;
		records[idx].w_cur_pos = w_pos;
		records[idx].unread_cnt++;
	}

	return 0;
}

int record_vs_diff(int vflag, unsigned int diff)
{
	mutex_lock(&global_mutex);

	if (first_time_diff) {
		first_time_diff--;
		// skip first time diff record
		LOG_D("skip first time diff\n");
	} else if (global_time == 0) {
		/* default global time starts from 1 ms */
		global_time = ONE_MIN_SEC;
		if (vflag) {
			/* vs2 after vs1 */
			record_vs(0, 0);
			global_time += CLK_TO_TIME_IN_US(diff);
		} else {
			/* vs1 after vs2 */
			record_vs(1, 0);
			global_time += CLK_TO_TIME_IN_US(diff);
			record_vs(0, 0);
		}
	}

	mutex_unlock(&global_mutex);

	return 0;
}

int record_vs1(unsigned int tclk)
{
	mutex_lock(&global_mutex);

	if (records[0].w_cur_pos == -1)
		tclk = 0;

	record_vs(0, tclk);

	mutex_unlock(&global_mutex);

	return 0;
}

int record_vs2(unsigned int tclk)
{
	mutex_lock(&global_mutex);

	if (records[1].w_cur_pos == -1)
		tclk = 0;

	record_vs(1, tclk);

	mutex_unlock(&global_mutex);

	return 0;
}

int show_records(unsigned int idx)
{
	LOG_D("show-%d [%u, %u, %u, %u]\n", idx,
		 records[idx].timestamps[0],
		 records[idx].timestamps[1],
		 records[idx].timestamps[2],
		 records[idx].timestamps[3]);

	return 0;
}

static struct Vtimestamp *get_record_entry(unsigned int tg)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(records); i++) {
		if ((tg - 1) == records[i].cammux_id)
			return &records[i];
	}

	return NULL;
}

int query_n3d_vsync_data(struct vsync_rec *pData)
{
	int i, j, rec_pos;
	unsigned int max_tick = 0;
	struct Vtimestamp *rec = NULL;

	for (i = 0; i < pData->ids; i++) {
		rec = get_record_entry(pData->recs[i].id);
		if (rec != NULL) {
			mutex_lock(&global_mutex);

			for (j = 0; j < MAX_RECORD_NUM; j++) {
				rec_pos = rec->w_cur_pos - j;
				if (rec_pos < 0)
					rec_pos = MAX_RECORD_NUM + rec_pos;

				pData->recs[i].timestamps[j] =
					rec->timestamps[rec_pos];
			}

			if (pData->recs[i].timestamps[0] > max_tick)
				max_tick = pData->recs[i].timestamps[0];

			pData->recs[i].vsyncs = rec->unread_cnt;
			rec->unread_cnt = 0;

			mutex_unlock(&global_mutex);
		}
	}

	pData->cur_tick = max_tick + 1;
	pData->tick_factor = 1;

	LOG_D("idx 1 time[0] = %u, time[1] = %u, time[2] = %u, time[3] = %u\n",
	       pData->recs[0].timestamps[0],
	       pData->recs[0].timestamps[1],
	       pData->recs[0].timestamps[2],
	       pData->recs[0].timestamps[3]);

	LOG_D("idx 2 time[0] = %u, time[1] = %u, time[2] = %u, time[3] = %u\n",
	       pData->recs[1].timestamps[0],
	       pData->recs[1].timestamps[1],
	       pData->recs[1].timestamps[2],
	       pData->recs[1].timestamps[3]);

	return 0;
}

