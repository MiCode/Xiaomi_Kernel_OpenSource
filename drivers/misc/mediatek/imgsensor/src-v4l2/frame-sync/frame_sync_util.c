// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "frame_sync_util.h"
#include "frame_sync_def.h"


/******************************************************************************/
// Log message
/******************************************************************************/
#include "frame_sync_log.h"

#define REDUCE_FS_UTIL_LOG
#define PFX "FrameSyncUtil"
/******************************************************************************/





/******************************************************************************/
// Frame Sync Utilities function
/******************************************************************************/
inline unsigned int calcLineTimeInNs(
	const unsigned int pclk, const unsigned int linelength)
{
	unsigned int val = 0;

	if ((pclk / 1000) == 0)
		return 0;

	val = ((unsigned long long)linelength * 1000000 + ((pclk / 1000) - 1))
		/ (pclk / 1000);

#if !defined(REDUCE_FS_UTIL_LOG)
	LOG_INF(
		"lineTime(ns):%u (linelength:%u, pclk:%u)\n",
		val, linelength, pclk);
#endif

	return val;
}


inline unsigned int convert2TotalTime(
	const unsigned int lineTimeInNs, const unsigned int lc)
{
	if (lineTimeInNs == 0)
		return 0;

	return (unsigned int)((unsigned long long)(lc)
				* lineTimeInNs / 1000);
}


inline unsigned int convert2LineCount(
	const unsigned int lineTimeInNs, const unsigned int val)
{
	if (lineTimeInNs == 0)
		return 0;

	return ((1000 * (unsigned long long)val) / lineTimeInNs) +
		((1000 * (unsigned long long)val) % lineTimeInNs ? 1 : 0);
}


inline unsigned int convert_timestamp_2_tick(
	const unsigned int timestamp, const unsigned int tick_factor)
{
	return (tick_factor) ? (timestamp * tick_factor) : timestamp;
}


inline unsigned int convert_tick_2_timestamp(
	const unsigned int tick, const unsigned int tick_factor)
{
	return (tick_factor) ? (tick / tick_factor) : tick;
}


inline unsigned int calc_time_after_sof(
	const unsigned int timestamp,
	const unsigned int tick, const unsigned int tick_factor)
{
	return (tick_factor != 0)
		? ((tick - convert_timestamp_2_tick(timestamp, tick_factor))
			/ tick_factor) : 0;
}


/**
 * return:
 * @1: tick_b is after tick_a (assuming the interval is a short period)
 * @0: tick_a is after tick_b (assuming the interval is a short period)
 */
inline unsigned int check_tick_b_after_a(
	const unsigned int tick_a, const unsigned int tick_b)
{
	unsigned int tick_diff = (tick_b - tick_a);
	unsigned int tick_half = 0, tick_max = (0 - 1); // 0xFFFFFFFF

	if (tick_a == tick_b)
		return 0;


	tick_half = tick_max >> 1;

#if !defined(REDUCE_FS_UTIL_LOG)
	LOG_INF(
		"[Tick] (b-a)=%u (a:%u/b:%u), max:%u, half:%u, ret:%u\n",
		(tick_b-tick_a), tick_a, tick_b,
		tick_max, tick_half,
		(tick_diff < tick_half) ? 1 : 0
	);
#endif

	return (tick_diff < tick_half) ? 1 : 0;
}


inline unsigned int check_timestamp_b_after_a(
	const unsigned int ts_a, const unsigned int ts_b,
	const unsigned int tick_factor)
{
	unsigned int tick_a, tick_b;

	tick_a = convert_timestamp_2_tick(ts_a, tick_factor);
	tick_b = convert_timestamp_2_tick(ts_b, tick_factor);

	return check_tick_b_after_a(tick_a, tick_b);
}


inline unsigned int get_two_timestamp_diff(
	const unsigned int ts_a, const unsigned int ts_b,
	const unsigned int tick_factor)
{
	unsigned int tick_a, tick_b;

	if (tick_factor == 0) {
		LOG_MUST(
			"WARNING: tick_factor:%u, abort after calc, return 0\n",
			tick_factor);
		return 0;
	}

	tick_a = convert_timestamp_2_tick(ts_a, tick_factor);
	tick_b = convert_timestamp_2_tick(ts_b, tick_factor);

	return (check_tick_b_after_a(tick_a, tick_b))
		? ((tick_b - tick_a) / tick_factor)
		: ((tick_a - tick_b) / tick_factor);
}


void get_array_data_from_new_to_old(
	const unsigned int *in_data,
	const unsigned int newest_idx, const unsigned int len,
	unsigned int *res_data)
{
	unsigned int i = 0, idx = 0;

	for (i = 0; i < len; ++i) {
		idx = (newest_idx + (len - i)) % len;
		res_data[i] = in_data[idx];
	}
}


/*
 * return:
 *     positive: ts diff table idx
 *     negative: idx_a == idx_b, same sensor
 */
int get_ts_diff_table_idx(const unsigned int idx_a, const unsigned int idx_b)
{
	if (idx_a == idx_b) {
		LOG_MUST(
			"WARNING: non-valid input, idx:(%u/%u) same idx, abort return -1\n",
			idx_a, idx_b);
		return -1;
	}

	if (idx_a < idx_b)
		return (idx_a*SENSOR_MAX_NUM)+idx_b-((idx_a+1)*(idx_a+2))/2;
	else
		return (idx_b*SENSOR_MAX_NUM)+idx_a-((idx_b+1)*(idx_b+2))/2;
}


static void find_latest_n_timestamps(
	const unsigned int *ts_a_arr, const unsigned int *ts_b_arr,
	const unsigned int ts_arr_len, const unsigned int ts_ordered_cnt,
	const unsigned int tick_factor,
	unsigned int *ts_ordered_arr, unsigned int *ts_label_arr)
{
	unsigned int i = 0, j = 0, k = 0;


	/* non-valid input checking */
	if (ts_arr_len < 3 || ts_ordered_cnt > ts_arr_len || tick_factor == 0) {
		LOG_MUST(
			"WARNING: non-valid input, ts_arr_len:%u (<3), ts_ordered_cnt:%u (>ts_arr_len), tick_factor:%u (==0), skip for finding out latest TS, return\n",
			ts_arr_len, ts_ordered_cnt, tick_factor);
		return;
	}

	/* merge two sorted arrays into result array */
	while (i < ts_ordered_cnt && j < ts_ordered_cnt && k < ts_ordered_cnt) {
		/* index => (i: ts_a_arr / j: ts_b_arr / k: ts_ordered_arr) */
		if (check_timestamp_b_after_a(
				ts_a_arr[i], ts_b_arr[j], tick_factor)) {

			/* pick ts b => mark 'b' to '2' */
			ts_ordered_arr[k] = ts_b_arr[j];
			ts_label_arr[k] = 2;
			k++;
			j++;

		} else {
			/* pick ts a => mark 'a' to '1' */
			ts_ordered_arr[k] = ts_a_arr[i];
			ts_label_arr[k] = 1;
			k++;
			i++;
		}
	}
}


/*
 * input:
 *     ts_a_arr & ts_b_arr: MUST be sorted (in ordered)
 *
 * function:
 *     find out timestamp diff of two target.
 *     ONLY bellow ordered type can find out valid result
 *         |a|b|a| or |b|a|b|
 *
 * return:
 *     positive: find out two target timestamp diff.
 *     negative: can NOT find out target timestamp diff.
 */
int find_two_sensor_timestamp_diff(
	const unsigned int *ts_a_arr, const unsigned int *ts_b_arr,
	const unsigned int ts_arr_len, const unsigned int tick_factor)
{
	int i = 0, diff[3] = {-1}, out_diff = 0, min_diff = 2147483647;
	unsigned int ts_ordered[4] = {0}, ts_label[4] = {0};
	unsigned int valid = 1;


	/* non-valid input checking */
	if (ts_arr_len < 3 || tick_factor == 0) {
		LOG_MUST(
			"WARNING: non-valid input, ts_arr_len:%u, tick_factor:%u, skip for updating TS diff, return -1\n",
			ts_arr_len, tick_factor);
		return -1;
	}


	/* find out latest N timestamp from two timestamp array */
	find_latest_n_timestamps(
		ts_a_arr, ts_b_arr,
		ts_arr_len, 4, tick_factor,
		ts_ordered, ts_label);

	/* calculate timestamp diff */
	for (i = 0; i < 3; ++i) {
		if (check_timestamp_b_after_a(
				ts_ordered[i], ts_ordered[i+1], tick_factor)) {

			LOG_MUST(
				"WARNING: ts_arr data seems not from new to old, ts_ordered:(%u/%u/%u/%u), ts_label:(%u/%u/%u/%u), skip for updating TS diff, return -1 [ts(a:(%u/%u/%u/%u)/(b:(%u/%u/%u/%u)), a:1/b:2]\n",
				ts_ordered[0], ts_ordered[1], ts_ordered[2], ts_ordered[3],
				ts_label[0], ts_label[1], ts_label[2], ts_label[3],
				ts_a_arr[0], ts_a_arr[1], ts_a_arr[2], ts_a_arr[3],
				ts_b_arr[0], ts_b_arr[1], ts_b_arr[2], ts_b_arr[3]);

			return -1;
		}

		diff[i] = get_two_timestamp_diff(
			ts_ordered[i], ts_ordered[i+1], tick_factor);

		if (diff[i] < min_diff)
			min_diff = diff[i];
	}

	/* check result is valid or not by using ts label */
	valid = ((ts_label[0] == ts_label[1])
		&& (ts_label[1] == ts_label[2]))
		? 0 : 1;

	out_diff = (valid) ? min_diff : (-1);

	LOG_INF(
		"diff:%d(%u/%u/%u, min_diff:%u(valid:%u)), ts_ordered:(%u/%u/%u/%u), ts_label:(%u/%u/%u/%u) [ts(a:(%u/%u/%u/%u)/(b:(%u/%u/%u/%u)), a:1/b:2, len/factor:(%u/%u)]\n",
		out_diff, diff[0], diff[1], diff[2], min_diff, valid,
		ts_ordered[0], ts_ordered[1], ts_ordered[2], ts_ordered[3],
		ts_label[0], ts_label[1], ts_label[2], ts_label[3],
		ts_a_arr[0], ts_a_arr[1], ts_a_arr[2], ts_a_arr[3],
		ts_b_arr[0], ts_b_arr[1], ts_b_arr[2], ts_b_arr[3],
		ts_arr_len, tick_factor);

	return out_diff;
}


/*
 * return:
 *     1: all results of sensor are synced.
 *     0: one of results of sensor is un-synced.
 *     -1: situation not defined, user should check it manually.
 *
 * input:
 *     mask: bits value for notifying which data will be check.
 *     len: default it should be equal to TS_DIFF_TABLE_LEN.
 */
int check_sync_result(
	const int *ts_diff_table_arr, const unsigned int mask,
	const unsigned int len, const unsigned int threshold)
{
	int ret = -1;
	unsigned int i = 0;

	/* for error input checking */
	if (len > TS_DIFF_TABLE_LEN) {
		LOG_MUST(
			"WARNING: non-valid input, arr_len:%u > TS_DIFF_TABLE_LEN:%u, abort return -1\n",
			len, TS_DIFF_TABLE_LEN);
		return -1;
	}

	for (i = 0; i < len; ++i) {
		if ((mask >> i) == 0 || ts_diff_table_arr[i] < 0)
			continue;

		ret = (ts_diff_table_arr[i] <= threshold)
			? 1 : 0;
	}

	return ret;
}


/******************************************************************************/
