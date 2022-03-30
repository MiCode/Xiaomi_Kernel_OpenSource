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
	unsigned int pclk, unsigned int linelength)
{
	unsigned int val = 0;

	if ((pclk / 1000) == 0 || (pclk / 1000) == 1)
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
	unsigned int lineTimeInNs, unsigned int lc)
{
	if (lineTimeInNs == 0)
		return 0;

	return (unsigned int)((unsigned long long)(lc)
				* lineTimeInNs / 1000);
}


inline unsigned int convert2LineCount(
	unsigned int lineTimeInNs, unsigned int val)
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
	unsigned int tick_a, unsigned int tick_b)
{
	unsigned int tick_diff = (tick_b - tick_a);
	unsigned int tick_half = 0, tick_max = (0 - 1); // 0xFFFFFFFF

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
/******************************************************************************/
