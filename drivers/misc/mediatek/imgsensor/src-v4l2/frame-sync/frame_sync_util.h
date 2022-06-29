/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _FRAME_SYNC_UTIL_H
#define _FRAME_SYNC_UTIL_H


/*******************************************************************************
 * Frame Sync Util functions.
 ******************************************************************************/

/* sensor related APIs */
unsigned int calcLineTimeInNs(
	const unsigned int pclk, const unsigned int linelength);

unsigned int convert2TotalTime(
	const unsigned int lineTimeInus, const unsigned int time);
unsigned int convert2LineCount(
	const unsigned int lineTimeInNs, const unsigned int val);
/* *** */


/* timestamp/tick related APIs */
unsigned int convert_timestamp_2_tick(
	const unsigned int timestamp, const unsigned int tick_factor);
unsigned int convert_tick_2_timestamp(
	const unsigned int tick, const unsigned int tick_factor);

unsigned int calc_time_after_sof(
	const unsigned int timestamp,
	const unsigned int tick, const unsigned int tick_factor);

unsigned int check_tick_b_after_a(
	const unsigned int tick_a, const unsigned int tick_b);

unsigned int check_timestamp_b_after_a(
	const unsigned int ts_a, const unsigned int ts_b,
	const unsigned int tick_factor);

unsigned int get_two_timestamp_diff(
	const unsigned int ts_a, const unsigned int ts_b,
	const unsigned int tick_factor);

void get_array_data_from_new_to_old(
	const unsigned int *in_arr,
	const unsigned int newest_idx, const unsigned int len,
	unsigned int *res_data);

int get_ts_diff_table_idx(const unsigned int idx_a, const unsigned int idx_b);

int find_two_sensor_timestamp_diff(
	const unsigned int *ts_a_arr, const unsigned int *ts_b_arr,
	const unsigned int ts_arr_len, const unsigned int tick_factor);

int check_sync_result(
	const int *ts_diff_table_arr, const unsigned int mask,
	const unsigned int len, const unsigned int threshold);
/* *** */


/******************************************************************************/

#endif
