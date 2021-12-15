/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _UT_PERFRAME_CTRL_SAMPLE_DEF_H
#define _UT_PERFRAME_CTRL_SAMPLE_DEF_H


/* utility marco */
/**
 *  x : input time in us.
 *  y : put sensor lineTimeInNs value.
 */
#define US_TO_LC(x, y) ((x)*1000/(y)+(((x)*1000%(y))?1:0))


/* UT perframe ctrl sample structure */
struct ut_perframe_ctrl_sample_st {
	struct fs_perframe_st *pf_settings;

	unsigned int vsyncs;
	unsigned int last_vsync_timestamp;
	unsigned int cur_tick;
	unsigned int cur_time;
};


#endif
