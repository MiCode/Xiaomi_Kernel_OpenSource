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

#include <linux/spinlock.h>
#include <linux/spinlock_types.h>

#include "n3d_util.h"
#include "vsync_recorder.h"

#define CLK_TO_TIME_IN_US(x) ((x) / N3D_CLK_FREQ)
#define ONE_MIN_SEC (1000 * N3D_CLK_FREQ)
#define THRESH_TS_REC_AMEND 500 /* us */
#define THRESH_TRUST_PERIOD_DIFF 100 /* us */

#define THRESH_CNT_TS_REC_AMEND (THRESH_TS_REC_AMEND * N3D_CLK_FREQ)
#define THRESH_CNT_TRUST_PERIOD_DIFF (THRESH_TRUST_PERIOD_DIFF * N3D_CLK_FREQ)

#define AUTO_CORRECTION_TS 1

struct Vtimestamp {
	int cammux_id;
	unsigned int timestamps[MAX_RECORD_NUM];
	int w_cur_pos;
	int unread_cnt;
};

static DEFINE_SPINLOCK(global_lock);
static unsigned int global_time;
static unsigned int latest_vs1_period;
static unsigned int latest_vs2_period;
static unsigned int vs2_diff_cnt_correction;
static struct Vtimestamp records[MAX_RECORD_SENSOR];

int reset_recorder(int cammux_id1, int cammux_id2)
{
	int i, j;
	unsigned long flags = 0;

	spin_lock_irqsave(&global_lock, flags);

	global_time = 0;
	latest_vs1_period = 0;
	latest_vs2_period = 0;
	vs2_diff_cnt_correction = 0;

	for (i = 0; i < ARRAY_SIZE(records); i++) {
		records[i].cammux_id = -1;
		records[i].w_cur_pos = -1;
		records[i].unread_cnt = 0;
		for (j = 0; j < MAX_RECORD_NUM; j++)
			records[i].timestamps[j] = 0;
	}
	records[0].cammux_id = cammux_id1;
	records[1].cammux_id = cammux_id2;

	spin_unlock_irqrestore(&global_lock, flags);

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

static int is_period_closely(unsigned int period_vs1, unsigned int period_vs2)
{
	unsigned int diff_period = 0;

	if ((period_vs1 == 0) || (period_vs2 == 0))
		return 0;

	if (period_vs1 > period_vs2)
		diff_period = period_vs1 - period_vs2;
	else
		diff_period = period_vs2 - period_vs1;

	return (diff_period <= THRESH_CNT_TRUST_PERIOD_DIFF);
}

#if AUTO_CORRECTION_TS
static int update_vs1_rec(unsigned int d_tclk)
{
	int vs1_pos, vs2_pos;
	int vs1_idx = 0;
	int vs2_idx = 1;
	unsigned int diff_ts = CLK_TO_TIME_IN_US(d_tclk);
	unsigned int vs2_latest_ts;

	if (global_time != 0) {
		vs1_pos = records[vs1_idx].w_cur_pos;
		vs2_pos = records[vs2_idx].w_cur_pos;

		vs2_latest_ts = records[vs2_idx].timestamps[vs2_pos];

		if (records[vs1_idx].timestamps[vs1_pos] > vs2_latest_ts) {
			records[vs1_idx].timestamps[vs1_pos] =
				vs2_latest_ts + diff_ts;
			/*
			 * LOG_D("amend vs1 ts to %u\n",
			 *   records[vs1_idx].timestamps[vs1_pos]);
			 */
		}
	}

	return 0;
}

static int update_vs2_rec(unsigned int d_tclk)
{
	int vs1_pos, vs2_pos;
	int vs1_idx = 0;
	int vs2_idx = 1;
	unsigned int diff_ts = CLK_TO_TIME_IN_US(d_tclk);
	unsigned int vs1_latest_ts;

	if (global_time != 0) {
		vs1_pos = records[vs1_idx].w_cur_pos;
		vs2_pos = records[vs2_idx].w_cur_pos;

		vs1_latest_ts = records[vs1_idx].timestamps[vs1_pos];

		if (records[vs2_idx].timestamps[vs2_pos] > vs1_latest_ts) {
			records[vs2_idx].timestamps[vs2_pos] =
				vs1_latest_ts + diff_ts;
			/*
			 * LOG_D("amend vs2 ts to %u\n",
			 *    records[vs2_idx].timestamps[vs2_pos]);
			 */
		} else {
			/* Need to correction vs2 when vs2 recorded */
			vs2_diff_cnt_correction = d_tclk;
		}
	}

	return 0;
}

static int try_update_vs2_rec(unsigned int d_tclk)
{
	/* prevent out of date ts diff info */
	if (is_period_closely(latest_vs1_period, latest_vs2_period))
		update_vs2_rec(d_tclk);

	return 0;
}
#endif

/* ISR */
int record_vs_diff(int vflag, unsigned int diff_cnt)
{
	spin_lock(&global_lock);

	if (!is_period_closely(latest_vs1_period, latest_vs2_period)) {
		/* skip recording untrust ts */
	} else if (global_time == 0) {
		/*LOG_D("start recording, diff_cnt = %u\n", diff_cnt);*/
		/* default global time starts from 1 ms */
		global_time = ONE_MIN_SEC;
		if (vflag) {
			/* vs2 after vs1 */
			record_vs(0, 0);
			global_time += CLK_TO_TIME_IN_US(diff_cnt);
		} else {
			/* vs1 after vs2 */
			record_vs(1, 0);
			global_time += CLK_TO_TIME_IN_US(diff_cnt);
			record_vs(0, 0);
		}
#if AUTO_CORRECTION_TS
	} else if (diff_cnt > THRESH_CNT_TS_REC_AMEND) {
		/* diff correction */
		if (vflag) {
			/* vs2 after vs1 */
			update_vs2_rec(diff_cnt);
		} else {
			/* vs1 after vs2 */
			update_vs1_rec(diff_cnt);
		}
#endif
	}

	spin_unlock(&global_lock);

	return 0;
}

/* ISR */
int record_vs1(unsigned int tclk)
{
	spin_lock(&global_lock);

	latest_vs1_period = tclk;

	if (records[0].w_cur_pos == -1)
		tclk = 0;

	record_vs(0, tclk);

	spin_unlock(&global_lock);

	return 0;
}

/* ISR */
int record_vs2(unsigned int tclk)
{
	spin_lock(&global_lock);

	latest_vs2_period = tclk;

	if (records[1].w_cur_pos == -1)
		tclk = 0;

	record_vs(1, tclk);

#if AUTO_CORRECTION_TS
	if (vs2_diff_cnt_correction) {
		/* try amend vs2 ts */
		try_update_vs2_rec(vs2_diff_cnt_correction);
		vs2_diff_cnt_correction = 0;
	}
#endif

	spin_unlock(&global_lock);

	return 0;
}

/* ISR */
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
	unsigned long flags = 0;

	for (i = 0; i < pData->ids; i++) {
		rec = get_record_entry(pData->recs[i].id);
		if (rec != NULL) {
			spin_lock_irqsave(&global_lock, flags);

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

			spin_unlock_irqrestore(&global_lock, flags);
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

