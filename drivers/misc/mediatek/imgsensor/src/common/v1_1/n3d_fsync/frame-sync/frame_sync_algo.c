// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifdef FS_UT
#include <string.h>
#include <stdlib.h>         /* Needed by memory allocate */
#else
/* INSTEAD of using stdio.h, you have to use the following include */
#include <linux/module.h>   /* Needed by all modules */
#include <linux/kernel.h>   /* Needed for KERN_INFO */
#include <linux/slab.h>     /* Needed by memory allocate */

#include <linux/string.h>
#endif // FS_UT

#include "frame_sync_algo.h"
#include "frame_monitor.h"


/******************************************************************************/
// Log message => see frame_sync_def.h
/******************************************************************************/
#define REDUCE_FS_ALGO_LOG
#define PFX "FrameSyncAlgo"
/******************************************************************************/





/******************************************************************************/
// Frame Sync Instance Structure (private structure)
/******************************************************************************/
struct FrameSyncInst {
	/* register sensor info */
	unsigned int sensor_id;         // imx586 -> 0x0586; s5k3m5sx -> 0x30D5
	unsigned int sensor_idx;        // main1 -> 0; sub1 -> 1;
					// main2 -> 2; sub2 -> 3; main3 -> 4;

//---------------------------- fs_streaming_st -------------------------------//
	/* know TG when streaming */
	unsigned int tg;                // Not used if ISP7 uses sensor_idx

	unsigned int fl_active_delay;   // SONY/auto_ext:(3, 1); others:(2, 0);
	unsigned int def_min_fl_lc;
	unsigned int max_fl_lc;         // for frame length boundary check

	unsigned int def_shutter_lc;    // default shutter_lc in driver

//----------------------------------------------------------------------------//

	/* IOCTRL CID ANTI_FLICKER */
	unsigned int flicker_en:1;      // move to perframe_st

//---------------------------- fs_perframe_st --------------------------------//
	/* IOCTRL CID SHUTTER_FRAME_LENGTH */
	unsigned int min_fl_lc;         // dynamic FPS using
	unsigned int shutter_lc;

	/* on-the-fly sensor mode change */
	unsigned int margin_lc;
	unsigned int pclk;
	unsigned int linelength;
	unsigned int lineTimeInNs;      // ~= 10^9 * (linelength/pclk)

	/* output frame length */
	unsigned int output_fl_lc;
	unsigned int output_fl_us;      // microsecond for algo using
//----------------------------------------------------------------------------//

//---------------------------- private member --------------------------------//
	/* predicted frame length, current:0, next:1 */
	/* must be updated when getting new frame record data / vsync data */
	unsigned int predicted_fl_us[2];
	unsigned int predicted_fl_lc[2];

	unsigned int vsyncs_updated:1;

	/* frame_record_st (record shutter and framelength settings) */
	struct frame_record_st recs[RECORDER_DEPTH];

	/* frame monitor data */
	unsigned int vsyncs;
	unsigned int last_vts;
	unsigned int vdiff;
};
static struct FrameSyncInst fs_inst[SENSOR_MAX_NUM];


/* fps sync result */
static unsigned int target_min_fl_us;


/* frame monitor data */
static unsigned int cur_tick;
static unsigned int tick_factor;


/* frame sync flicker table */
#define FLICKER_TABLE_SIZE 8
static unsigned int fs_flicker_table[FLICKER_TABLE_SIZE][2] = {

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
/******************************************************************************/





/******************************************************************************/
// utility functions
/******************************************************************************/
inline unsigned int
calcLineTimeInNs(unsigned int pclk, unsigned int linelength)
{
	unsigned int val = 0;


	val = ((unsigned long long)linelength * 1000000 + ((pclk / 1000) - 1))
		/ (pclk / 1000);

	LOG_INF("lineTime(us):%u\n", val);


	return val;
}


inline unsigned int
convert2TotalTime(unsigned int lineTimeInNs, unsigned int lc)
{
	if (lineTimeInNs == 0)
		return 0;

	return (unsigned int)((unsigned long long)(lc)
				* lineTimeInNs / 1000);
}


inline unsigned int
convert2LineCount(unsigned int lineTimeInNs, unsigned int val)
{
	return ((1000 * (unsigned long long)val) / lineTimeInNs) +
		((1000 * (unsigned long long)val) % lineTimeInNs ? 1 : 0);
}


static unsigned int get_anti_flicker_fl(unsigned int framelength)
{
	unsigned int i = 0;


	for (i = 0; i < FLICKER_TABLE_SIZE; ++i) {
		if (fs_flicker_table[i][0] == 0)
			break;

		if ((fs_flicker_table[i][0] > framelength) &&
			(framelength >= fs_flicker_table[i][1])) {

			framelength = fs_flicker_table[i][0];
			break;
		}
	}


	return framelength;
}


static void check_fl_boundary(unsigned int idx)
{
	unsigned int buf_fl_lc = 0, buf_fl_us = 0;


	/* check framelength boundary */
	if (fs_inst[idx].output_fl_lc > fs_inst[idx].max_fl_lc) {
		buf_fl_lc = fs_inst[idx].output_fl_lc;
		buf_fl_us = fs_inst[idx].output_fl_us;


		/* reset fl_lc and fl_us to maximum value */
		fs_inst[idx].output_fl_lc = fs_inst[idx].max_fl_lc;
		fs_inst[idx].output_fl_us = convert2TotalTime(
					fs_inst[idx].lineTimeInNs,
					fs_inst[idx].output_fl_lc);


		/* log warning (this case will break sync) */
		LOG_PR_WARN(
			"WARNING: [%u] ID:%#x(sidx:%u), set fl:%u(%u), but reaches max_fl:%u(%u)\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			buf_fl_us,
			buf_fl_lc,
			fs_inst[idx].output_fl_us,
			fs_inst[idx].output_fl_lc);
	}
}


static unsigned int check_vsync_valid(unsigned int solveIdxs[], unsigned int len)
{
	unsigned int i = 0;
	unsigned int ret = 1; // valid -> 1 / non-valid -> 0


	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];


		/* valid -> ret = 1 (not be changed) */
		/* non-valid -> ret = 0, and keep it being 0 */
		if (fs_inst[idx].last_vts == 0) {
			ret = 0;

			LOG_PR_ERR(
				"ERROR: [%u] ID:%#x(sidx:%u), last vts:%u\n",
				idx,
				fs_inst[idx].sensor_id,
				fs_inst[idx].sensor_idx,
				fs_inst[idx].last_vts);
		}
	}


	return ret;
}


/*******************************************************************************
 * all framelength operation must use this API
 ******************************************************************************/
static void set_fl_us(unsigned int idx, unsigned int us)
{
	/* 1. set / update framelength value */
	fs_inst[idx].output_fl_us = us;
	fs_inst[idx].output_fl_lc = convert2LineCount(
					fs_inst[idx].lineTimeInNs,
					fs_inst[idx].output_fl_us);


#ifndef REDUCE_FS_ALGO_LOG
	LOG_INF(
		"[%u] ID:%#x(sidx:%u), set fl:%u(%u)\n",
		idx,
		fs_inst[idx].sensor_id,
		fs_inst[idx].sensor_idx,
		fs_inst[idx].output_fl_us,
		fs_inst[idx].output_fl_lc);
#endif


	/* 2. check framelength boundary */
	check_fl_boundary(idx);


	/* 3. update result to frame recorder for predicted fl used */
	*fs_inst[idx].recs[0].framelength_lc = fs_inst[idx].output_fl_lc;


#ifdef FS_UT
	/* N. set frame monitor sensor_curr_fl_us for FS_UT timestamp */
	frm_set_sensor_curr_fl_us(idx, us);
#endif
}


/*******************************************************************************
 * calculate a appropriate min framelength base on shutter with boundary check
 *
 * Input: idx, min_fl_lc
 * Reference: shutter, margin, max_fl_lc
 *
 * "min_fl_lc":
 *      could be "def_min_fl_lc" ( like sensor driver write_shutter() function )
 *      or "min_fl_lc" ( for frame sync dynamic FPS ).
 ******************************************************************************/
static inline unsigned int
calc_min_fl_lc(unsigned int idx, unsigned int min_fl_lc)
{
	unsigned int output_fl_lc = 0;
	unsigned int shutter_margin =
			fs_inst[idx].shutter_lc + fs_inst[idx].margin_lc;


	/* calculate appropriate min framelength */
	if (shutter_margin > min_fl_lc)
		output_fl_lc = shutter_margin;
	else
		output_fl_lc = min_fl_lc;


	/* framelength boundary check */
	if (output_fl_lc > fs_inst[idx].max_fl_lc)
		output_fl_lc = fs_inst[idx].max_fl_lc;


	return output_fl_lc;
}


#ifdef FS_UT
/*******************************************************************************
 * like sensor driver set_max_framerate() function
 ******************************************************************************/
static inline void
set_max_framerate(unsigned int idx, unsigned int framerate)
{
	fs_inst[idx].output_fl_lc =
		fs_inst[idx].pclk / framerate * 10 / fs_inst[idx].linelength;
}


/* TODO: output value in fs_inst[].output_fl_c or in stack? */
unsigned int fs_alg_write_shutter(unsigned int idx)
{
	unsigned int realtime_fps = 0;
	unsigned int is_adjust_fps = 0;


	/* get appropriate min framelength base on shutter */
	fs_inst[idx].output_fl_lc =
		calc_min_fl_lc(idx, fs_inst[idx].def_min_fl_lc);

	/* shutter boundary check, pass for UT */

	/* for anti flicker */
	realtime_fps = fs_inst[idx].pclk / fs_inst[idx].linelength * 10
					/ fs_inst[idx].output_fl_lc;

	if (fs_inst[idx].flicker_en) {
		if (realtime_fps >= 297 && realtime_fps <= 305) {
			set_max_framerate(idx, 296);
			realtime_fps = 296;
			is_adjust_fps = 1;
		} else if (realtime_fps >= 147 && realtime_fps <= 150) {
			set_max_framerate(idx, 146);
			realtime_fps = 146;
			is_adjust_fps = 1;
		}
	}


	LOG_INF(
		"[%u] ID:%#x(sidx:%u), fl:%u(%u), x10fps(%u):%u, flicker(%u), shutter:%u(%u)\n",
		idx,
		fs_inst[idx].sensor_id,
		fs_inst[idx].sensor_idx,
		convert2TotalTime(fs_inst[idx].lineTimeInNs,
				fs_inst[idx].output_fl_lc),
		fs_inst[idx].output_fl_lc,
		is_adjust_fps,
		realtime_fps,
		fs_inst[idx].flicker_en,
		convert2TotalTime(fs_inst[idx].lineTimeInNs,
				fs_inst[idx].shutter_lc),
		fs_inst[idx].shutter_lc);


	return fs_inst[idx].output_fl_lc;
}
#endif


static inline unsigned int
check_valid_for_using_vsyncs_recs_data(unsigned int idx)
{
	return (fs_inst[idx].vsyncs_updated) ? 1 : 0;
}


static inline void
set_valid_for_using_vsyncs_recs_data(unsigned int idx, unsigned int flag)
{
	fs_inst[idx].vsyncs_updated = flag;
}


static void calibrate_recs_data_by_vsyncs(unsigned int idx)
{
	if (!check_valid_for_using_vsyncs_recs_data(idx)) {
		LOG_PR_WARN(
			"[%u] ID:%#x(sidx:%u), calibrate recs failed, update vsyncs data first\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx);

		return;
	}


	/* Check how many vsyncs have passed so far, */
	/* for syncing correct frame settings. */
	/* EX: [0, 1, 2] -> [1, 2, temp] or [2, 2, temp] */
	if (fs_inst[idx].vsyncs > 1) {
		// fs_inst[idx].recs[2] = fs_inst[idx].recs[1];

		*fs_inst[idx].recs[2].framelength_lc =
				*fs_inst[idx].recs[1].framelength_lc;
		*fs_inst[idx].recs[2].shutter_lc =
				*fs_inst[idx].recs[1].shutter_lc;


#ifndef REDUCE_FS_ALGO_LOG
		LOG_INF(
			"[%u] ID:%#x(sidx:%u), passed vsyncs:%u, calibrate frame records data\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			fs_inst[idx].vsyncs);
#endif
	}
}


/*******************************************************************************
 * predicted frame length ( current:0, next:1 )
 *
 * must ONLY be updated when
 *    setting new frame recs data and after getting vsyncs data
 *    otherwise the results will be not correct
 ******************************************************************************/
static void calc_predicted_frame_length(unsigned int idx)
{
	unsigned int i = 0;

	unsigned int fdelay = fs_inst[idx].fl_active_delay;
	unsigned int *p_fl_lc = fs_inst[idx].predicted_fl_lc;
	unsigned int *p_fl_us = fs_inst[idx].predicted_fl_us;


	/* for error handle, check sensor fl_active_delay value */
	if ((fdelay < 2) || (fdelay > 3)) {
		LOG_INF(
			"ERROR: [%u] ID:%#x(sidx:%u), frame_time_delay_frame:%u is not valid (must be 2 or 3)\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			fs_inst[idx].fl_active_delay);

		return;
	}

	/* calculate "current predicted" and "next predicted" framelength */
	/* P.S. current:0, next:1 */
	for (i = 0; i < 2; ++i) {
		if (fdelay == 3) {
			/* SONY sensor with auto-extend on */
			unsigned int sm =
				*fs_inst[idx].recs[1-i].shutter_lc +
				fs_inst[idx].margin_lc;

			p_fl_lc[i] =
				(*fs_inst[idx].recs[2-i].framelength_lc > sm)
				? *fs_inst[idx].recs[2-i].framelength_lc
				: sm;


			p_fl_us[i] = convert2TotalTime(
						fs_inst[idx].lineTimeInNs,
						p_fl_lc[i]);

		} else { // fdelay == 2
			/* non-SONY sensor case */
			p_fl_lc[i] = *fs_inst[idx].recs[1-i].framelength_lc;


			p_fl_us[i] = convert2TotalTime(
						fs_inst[idx].lineTimeInNs,
						p_fl_lc[i]);
		}
	}
}
/******************************************************************************/





/******************************************************************************/
// Dump & Debug function
/******************************************************************************/
static inline void fs_alg_dump_streaming_data(unsigned int idx)
{
	LOG_INF(
		"[%u] ID:%#x(sidx:%u), tg:%u, fl_delay:%u, fl_lc(def/max):%u/%u, def_shut_lc:%u\n",
		idx,
		fs_inst[idx].sensor_id,
		fs_inst[idx].sensor_idx,
		fs_inst[idx].tg,
		fs_inst[idx].fl_active_delay,
		fs_inst[idx].def_min_fl_lc,
		fs_inst[idx].max_fl_lc,
		fs_inst[idx].def_shutter_lc);
}


static inline void fs_alg_dump_perframe_data(unsigned int idx)
{
	LOG_INF(
		"[%u] ID:%#x(sidx:%u), flk_en:%u, min_fl:%u(%u), shutter:%u(%u), margin:%u(%u), lineTime(ns):%u(%u/%u)\n",
		idx,
		fs_inst[idx].sensor_id,
		fs_inst[idx].sensor_idx,
		fs_inst[idx].flicker_en,
		convert2TotalTime(
			fs_inst[idx].lineTimeInNs,
			fs_inst[idx].min_fl_lc),
		fs_inst[idx].min_fl_lc,
		convert2TotalTime(
			fs_inst[idx].lineTimeInNs,
			fs_inst[idx].shutter_lc),
		fs_inst[idx].shutter_lc,
		convert2TotalTime(
			fs_inst[idx].lineTimeInNs,
			fs_inst[idx].margin_lc),
		fs_inst[idx].margin_lc,
		fs_inst[idx].lineTimeInNs,
		fs_inst[idx].linelength,
		fs_inst[idx].pclk);
}


/* for debug using, dump all data in instance */
void fs_alg_dump_fs_inst_data(unsigned int idx)
{
	fs_alg_dump_streaming_data(idx);
	fs_alg_dump_perframe_data(idx);
}


/* for debug using, dump all data in all instance */
void fs_alg_dump_all_fs_inst_data(void)
{
	unsigned int i = 0;

	for (i = 0; i < SENSOR_MAX_NUM; ++i)
		fs_alg_dump_fs_inst_data(i);
}
/******************************************************************************/




/******************************************************************************/
// fs algo operation functions (set information data)
/******************************************************************************/
void fs_alg_set_anti_flicker(unsigned int idx, unsigned int flag)
{
	fs_inst[idx].flicker_en = flag;


#ifndef REDUCE_FS_ALGO_LOG
	LOG_INF("[%u] ID:%#x(sidx:%u), flk_en:%u\n",
		idx,
		fs_inst[idx].sensor_id,
		fs_inst[idx].sensor_idx,
		fs_inst[idx].flicker_en);
#endif
}


void fs_alg_update_tg(unsigned int idx, unsigned int tg)
{
	fs_inst[idx].tg = tg;


	LOG_INF("[%u] ID:%#x(sidx:%u), updated tg:%u\n",
		idx,
		fs_inst[idx].sensor_id,
		fs_inst[idx].sensor_idx,
		fs_inst[idx].tg);
}


void fs_alg_update_min_fl_lc(unsigned int idx, unsigned int min_fl_lc)
{
	if (fs_inst[idx].min_fl_lc != min_fl_lc) {
		/* min_fl_lc was changed after set shutter, so update it */
		fs_inst[idx].min_fl_lc = min_fl_lc;


		LOG_INF("[%u] ID:%#x(sidx:%u), updated min_fl:%u(%u)\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			convert2TotalTime(
				fs_inst[idx].lineTimeInNs,
				fs_inst[idx].min_fl_lc),
			fs_inst[idx].min_fl_lc);
	}
}


void
fs_alg_set_streaming_st_data(unsigned int idx, struct fs_streaming_st (*pData))
{
	fs_inst[idx].sensor_id = pData->sensor_id;
	fs_inst[idx].sensor_idx = pData->sensor_idx;
	fs_inst[idx].tg = pData->tg;
	fs_inst[idx].fl_active_delay = pData->fl_active_delay;
	fs_inst[idx].def_min_fl_lc = pData->def_fl_lc;
	fs_inst[idx].max_fl_lc = pData->max_fl_lc;
	fs_inst[idx].def_shutter_lc = pData->def_shutter_lc;


	fs_alg_dump_streaming_data(idx);
}


void
fs_alg_set_perframe_st_data(unsigned int idx, struct fs_perframe_st (*pData))
{
	//fs_inst[idx].sensor_id = pData->sensor_id;
	//fs_inst[idx].sensor_idx = pData->sensor_idx;
	fs_inst[idx].min_fl_lc = pData->min_fl_lc;
	fs_inst[idx].shutter_lc = pData->shutter_lc;
	fs_inst[idx].margin_lc = pData->margin_lc;
	fs_inst[idx].flicker_en = pData->flicker_en;
	fs_inst[idx].pclk = pData->pclk;
	fs_inst[idx].linelength = pData->linelength;
	fs_inst[idx].lineTimeInNs = pData->lineTimeInNs;


#ifndef REDUCE_FS_ALGO_LOG
	fs_alg_dump_perframe_data(idx);
#endif
}


void fs_alg_reset_fs_inst(unsigned int idx)
{
	struct FrameSyncInst clear_fs_inst_st = {0};

	fs_inst[idx] = clear_fs_inst_st;


#ifndef REDUCE_FS_ALGO_LOG
	LOG_INF("clear idx:%u data. (all to zero)\n", idx);
#endif
}


/*******************************************************************************
 * receive frame record data from fs_drv
 *
 * fs algo will use these information to predict current and
 *     next framelength when calculating vsync diff.
 ******************************************************************************/
void
fs_alg_set_frame_record_st_data(unsigned int idx, struct frame_record_st data[])
{
	unsigned int i = 0;


	/* 1. set/update frame recoder data */
	for (i = 0; i < RECORDER_DEPTH; ++i)
		fs_inst[idx].recs[i] = data[i];


	/* 2. update predicted frame length by new frame recorder data */
	calc_predicted_frame_length(idx);


	/* 3. set frame measurement predicted frame length */
	frm_set_frame_measurement(
		idx, fs_inst[idx].vsyncs,
		fs_inst[idx].predicted_fl_us[0],
		fs_inst[idx].predicted_fl_lc[0],
		fs_inst[idx].predicted_fl_us[1],
		fs_inst[idx].predicted_fl_lc[1]);


#ifndef REDUCE_FS_ALGO_LOG
	LOG_INF(
		"[%u] ID:%#x(sidx:%u), tg:%u, frecs: (0:%u/%u), (1:%u/%u), (2:%u/%u) (fl_lc/shut_lc), pred_fl(curr:%u(%u), next:%u(%u))(%u), margin_lc:%u, fdelay:%u\n",
		idx,
		fs_inst[idx].sensor_id,
		fs_inst[idx].sensor_idx,
		fs_inst[idx].tg,
		*fs_inst[idx].recs[0].framelength_lc,
		*fs_inst[idx].recs[0].shutter_lc,
		*fs_inst[idx].recs[1].framelength_lc,
		*fs_inst[idx].recs[1].shutter_lc,
		*fs_inst[idx].recs[2].framelength_lc,
		*fs_inst[idx].recs[2].shutter_lc,
		fs_inst[idx].predicted_fl_us[0],
		fs_inst[idx].predicted_fl_lc[0],
		fs_inst[idx].predicted_fl_us[1],
		fs_inst[idx].predicted_fl_lc[1],
		fs_inst[idx].lineTimeInNs,
		fs_inst[idx].margin_lc,
		fs_inst[idx].fl_active_delay);
#endif
}
/******************************************************************************/





/******************************************************************************/
// Frame Sync Algorithm function
/******************************************************************************/

/* return "0" -> done; "non 0" -> error ? */
unsigned int
fs_alg_get_vsync_data(unsigned int solveIdxs[], unsigned int len)
{
	unsigned int i = 0;

	unsigned int monitorSensor[TG_MAX_NUM];
	struct vsync_rec vsync_recs = {0};


	/* according to "solve Idx", get correct "TG / sensor_idx" */
	for (i = 0; i < len; ++i)
		monitorSensor[i] = fs_inst[solveIdxs[i]].tg;


	/* call Frame Monitor API to get vsync data from CCU */
	if (frm_query_vsync_data(monitorSensor, len)) {
		LOG_PR_WARN("ERROR: query vsync data from CCU error\n");
		return 1;
	}
	frm_get_vsync_data(&vsync_recs);


	/* keep cur_tick and tick_factor value */
	cur_tick = vsync_recs.cur_tick;
	tick_factor = vsync_recs.tick_factor;


#ifndef REDUCE_FS_ALGO_LOG
	LOG_INF("cur_tick:%u, tick_factor:%u\n",
		cur_tick,
		tick_factor);
#endif


	/* keep vsync and last_vts data */
	for (i = 0; i < len; ++i) {
		if (fs_inst[solveIdxs[i]].tg != vsync_recs.recs[i].id) {
			LOG_PR_WARN(
				"ERROR: [%u].tg:%u not sync to v_recs[%u].tg:%u\n",
				solveIdxs[i],
				fs_inst[solveIdxs[i]].tg,
				i,
				vsync_recs.recs[i].id);

			return 1;
		}

		fs_inst[solveIdxs[i]].vsyncs =
					vsync_recs.recs[i].vsyncs;
		fs_inst[solveIdxs[i]].last_vts =
					vsync_recs.recs[i].timestamps[0];


#ifndef REDUCE_FS_ALGO_LOG
		LOG_INF(
			"[%u] ID:%#x(sidx:%u), tg:%u, vsyncs:%u, last_vts:%u\n",
			solveIdxs[i],
			fs_inst[solveIdxs[i]].sensor_id,
			fs_inst[solveIdxs[i]].sensor_idx,
			fs_inst[solveIdxs[i]].tg,
			fs_inst[solveIdxs[i]].vsyncs,
			fs_inst[solveIdxs[i]].last_vts);
#endif


		set_valid_for_using_vsyncs_recs_data(solveIdxs[i], 1);


		/* using frame monitor to get new vsync information */
		/* and make frame recorder data be correct */
		calibrate_recs_data_by_vsyncs(solveIdxs[i]);
	}


	return 0;
}


static void do_fps_sync(unsigned int solveIdxs[], unsigned int len)
{
	unsigned int i = 0;
	unsigned int target_fl_us = 0;

	int ret = 0;
	char *log_buf = NULL;

#ifdef FS_UT
	log_buf = calloc(LOG_BUF_STR_LEN, sizeof(char));
#else
	log_buf = kcalloc(LOG_BUF_STR_LEN, sizeof(char), GFP_KERNEL);
#endif // FS_UT

	if (log_buf == NULL) {
		LOG_PR_ERR("ERROR: log_buf allocate memory failed\n");

		return;
	}

	log_buf[0] = '\0';


	/* 1. calc each fl_us for each sensor, take max fl_us as target_fl_us */
	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];
		unsigned int fl_us = 0, fl_lc = 0;

		fl_lc = calc_min_fl_lc(idx, fs_inst[idx].min_fl_lc);
		fl_us = convert2TotalTime(fs_inst[idx].lineTimeInNs, fl_lc);


		ret = snprintf(log_buf + strlen(log_buf),
			LOG_BUF_STR_LEN - strlen(log_buf),
			"[%u] ID:%#x(sidx:%u), fl:%u(%u) (%u/%u/%u/%u, %u); ",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			fl_us,
			fl_lc,
			fs_inst[idx].shutter_lc,
			fs_inst[idx].margin_lc,
			fs_inst[idx].min_fl_lc,
			*fs_inst[idx].recs[0].framelength_lc,
			fs_inst[idx].lineTimeInNs);

		if (ret < 0)
			LOG_INF("ERROR: LOG encoding error, ret:%d\n", ret);


		if (fl_us > target_fl_us)
			target_fl_us = fl_us;
	}


	/* 2. use target_fl_us as output_fl_us for each sensor */
	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];

		/* set to min framelength */
		/* all framelength operation must use this API */
		set_fl_us(idx, target_fl_us);
	}

	target_min_fl_us = target_fl_us;


	ret = snprintf(log_buf + strlen(log_buf),
		LOG_BUF_STR_LEN - strlen(log_buf),
		"FL sync to %u (us)",
		target_fl_us);

	if (ret < 0)
		LOG_INF("ERROR: LOG encoding error, ret:%d\n", ret);


	LOG_INF("%s\n", log_buf);


#ifdef FS_UT
	free(log_buf);
#else
	kfree(log_buf);
#endif // FS_UT
}


static void adjust_vsync_diff(unsigned int solveIdxs[], unsigned int len)
{
	unsigned int i = 0;

	unsigned int min_vtick_diff = (0 - 1);    // 0xffffffff
	unsigned int target_vts = 0;
	unsigned int pf_ctrl_timing_error[SENSOR_MAX_NUM] = {0};

	unsigned int anti_flicker_en = 0;
	unsigned int flicker_vdiff[SENSOR_MAX_NUM] = {0};
	unsigned int max_flicker_vdiff = 0;


	/* for snprintf and memory allocate */
	int ret = 0;
	char *log_buf[SENSOR_MAX_NUM] = {NULL};

	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];

#ifdef FS_UT
		log_buf[idx] = calloc(LOG_BUF_STR_LEN, sizeof(char));
#else
		log_buf[idx] = kcalloc(LOG_BUF_STR_LEN, sizeof(char),
					GFP_KERNEL);
#endif // FS_UT

		if (log_buf[idx] == NULL) {
			LOG_PR_ERR(
				"ERROR: [%u] log_buf allocate memory failed\n",
				idx);

			/* return, and free alloc memory */
			goto free_alloc_mem;
		} else
			log_buf[idx][0] = '\0';
	}


	/* 0. check vsync timestamp (preventing last vts is "0") */
	if (check_vsync_valid(solveIdxs, len) == 0) {
		LOG_PR_WARN(
			"ERROR: Incorrect vsync timestamp detected, not adjust vsync diff\n"
			);

		/* return, and free alloc memory */
		goto free_alloc_mem;
	}


	/* 1. calculate vsync diff for all */
	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];


		fs_inst[idx].vdiff =
			fs_inst[idx].last_vts * tick_factor - cur_tick;


		if (fs_inst[idx].vdiff < min_vtick_diff)
			min_vtick_diff = fs_inst[idx].vdiff;
	}
	// LOG_INF("min vtick diff:%u\n", min_vtick_diff);
	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];


		fs_inst[idx].vdiff -= min_vtick_diff;


		/* prevent for floating point exception */
		if (tick_factor != 0)
			fs_inst[idx].vdiff /= tick_factor;

		// fs_inst[idx].predicted_vts = fs_inst[idx].vdiff;


		ret = snprintf(log_buf[idx] + strlen(log_buf[idx]),
			LOG_BUF_STR_LEN - strlen(log_buf[idx]),
			"[%u] ID:%#x(sidx:%u), tg:%u, vsyncs:%u, vdiff:%u, ",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			fs_inst[idx].tg,
			fs_inst[idx].vsyncs,
			fs_inst[idx].vdiff);

		if (ret < 0)
			LOG_INF("ERROR: LOG encoding error, ret:%d\n", ret);
	}


	/* 2. predict current and next vsync timestamp */
	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];
		unsigned int *predicted_fl_us = fs_inst[idx].predicted_fl_us;

		calc_predicted_frame_length(idx);

		fs_inst[idx].vdiff += predicted_fl_us[0] + predicted_fl_us[1];


		ret = snprintf(log_buf[idx] + strlen(log_buf[idx]),
			LOG_BUF_STR_LEN - strlen(log_buf[idx]),
			"pred_fl(curr:%u(%u), next:%u(%u)), fdelay:%u, ",
			predicted_fl_us[0],
			fs_inst[idx].predicted_fl_lc[0],
			predicted_fl_us[1],
			fs_inst[idx].predicted_fl_lc[1],
			fs_inst[idx].fl_active_delay);

		if (ret < 0)
			LOG_INF("ERROR: LOG encoding error, ret:%d\n", ret);


#ifdef FS_UT
		/* update frame monitor current predicted framelength data */
		frm_update_predicted_curr_fl_us(idx, predicted_fl_us[0]);
#endif
	}


	/* 3. calculate diff of predicted_vts */
	/* 3.1 find target timestamp to sync */
	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];

		if (fs_inst[idx].vdiff > target_vts)
			target_vts = fs_inst[idx].vdiff;
	}
	/* 3.2 extend frame length to align target timestamp */
	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];
		unsigned int pred_vdiff = 0;
		unsigned int fl_lc = 0;


		/* detect pf ctrl timing error */
		pred_vdiff = target_vts - fs_inst[idx].vdiff;
		if (pred_vdiff > target_min_fl_us) {
			/* pred_vdiff > FPS sync result */
			/* => Do not adjust vdiff, only use fps sync result */

			pf_ctrl_timing_error[idx] = 1;
			pred_vdiff = 0;
		}


		/* add diff and set framelength */
		/* all framelength operation must use this API */
		fl_lc = fs_inst[idx].output_fl_us + pred_vdiff;

		set_fl_us(idx, fl_lc);


		ret = snprintf(log_buf[idx] + strlen(log_buf[idx]),
			LOG_BUF_STR_LEN - strlen(log_buf[idx]),
			"pred_vdiff:%u(timing_err:%u), flk_en:%u, out_fl:%u(%u)",
			target_vts - fs_inst[idx].vdiff,
			pf_ctrl_timing_error[idx],
			fs_inst[idx].flicker_en,
			fs_inst[idx].output_fl_us,
			fs_inst[idx].output_fl_lc);

		if (ret < 0)
			LOG_INF("ERROR: LOG encoding error, ret:%d\n", ret);
	}


	/* TODO: add check perframe ctrl timing error */


	/* 4. anti-flicker */

	/* 4.1 check anti-flicker enable, */
	/*     and find out max flicker vdiff simultaneously */
	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];


		/* check anti-flicker enable */
		if (fs_inst[idx].flicker_en == 0)
			continue;

		/* for anti-flicker enable case */
		anti_flicker_en |= fs_inst[idx].flicker_en;


		/* calculate anti-flicker vdiff */
		/*      flk vdiff = 0 => not flk fl */
		/*      flk vdiff > 0 => flk fl, adjust fl */
		flicker_vdiff[idx] =
			get_anti_flicker_fl(fs_inst[idx].output_fl_us) -
			fs_inst[idx].output_fl_us;

		if (flicker_vdiff[idx] == 0)
			continue;


		/* get maximum flicker vdiff */
		if (flicker_vdiff[idx] > max_flicker_vdiff)
			max_flicker_vdiff = flicker_vdiff[idx];
	}


	if (anti_flicker_en == 0) {
		for (i = 0; i < len; ++i) {
			unsigned int idx = solveIdxs[i];

			LOG_INF("%s\n", log_buf[idx]);
		}

		/* return, and free alloc memory */
		goto free_alloc_mem;
	}


	/* 4.2 add max anti-flicker vdiff to (all) sony sensor */
	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];
		unsigned int fl_lc = 0;


		/* add max flk vdiff and set framelength */
		/* all framelength operation must use this API */
		fl_lc = fs_inst[idx].output_fl_us + max_flicker_vdiff;
		set_fl_us(idx, fl_lc);


		ret = snprintf(log_buf[idx] + strlen(log_buf[idx]),
			LOG_BUF_STR_LEN - strlen(log_buf[idx]),
			", +flk:%u, 10xFPS:%u, out_fl:%u(%u)",
			max_flicker_vdiff,
			(unsigned int)(10000000/fs_inst[idx].output_fl_us),
			fs_inst[idx].output_fl_us,
			fs_inst[idx].output_fl_lc);

		if (ret < 0)
			LOG_INF("ERROR: LOG encoding error, ret:%d\n", ret);
	}


	for (i = 0; i < len; ++i)
		LOG_INF("%s\n", log_buf[solveIdxs[i]]);


free_alloc_mem:
	for (i = 0; i < len; ++i) {
		if (log_buf[solveIdxs[i]] == NULL)
			continue;
#ifdef FS_UT
		free(log_buf[solveIdxs[i]]);
#else
		kfree(log_buf[solveIdxs[i]]);
#endif // FS_UT
	}
}


/* return "0" -> done; "non 0" -> error ? */
unsigned int fs_alg_solve_frame_length(
	unsigned int solveIdxs[],
	unsigned int framelength_lc[], unsigned int len)
{
	unsigned int i = 0;


#ifndef REDUCE_FS_ALGO_LOG
	LOG_INF("%u sensors do frame sync\n", len);
#endif


	/* boundary checking for how many sensor sync */
	// if (len > SENSOR_MAX_NUM)
	if (len > TG_MAX_NUM)
		return 1;


#ifndef REDUCE_FS_ALGO_LOG
	/* dump information */
	for (i = 0; i < len; ++i)
		dump_fs_inst_data(solveIdxs[i]);

	// dump_all_fs_inst_data();
#endif


	/* 1. get Vsync data by Frame Monitor */
	if (fs_alg_get_vsync_data(solveIdxs, len)) {
		LOG_PR_WARN("Get Vsync data ERROR\n");
		return 1;
	}


	/* 2. FPS Sync */
	do_fps_sync(solveIdxs, len);


	/* 3. adjust Vsync diff */
	adjust_vsync_diff(solveIdxs, len);


	/* 4. copy framelength_lc results */
	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];

		framelength_lc[idx] = fs_inst[idx].output_fl_lc;


		/* 5. set NOT valid for using vsyncs / recs data for */
		/*    preventing calculation error */
		set_valid_for_using_vsyncs_recs_data(idx, 0);
	}


	return 0;
}
/******************************************************************************/
