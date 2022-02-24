/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _FRAME_SYNC_ALG_H
#define _FRAME_SYNC_ALG_H

#include "frame_sync.h"


/* utility functions */
unsigned int calcLineTimeInus(unsigned int pclk, unsigned int linelength);
unsigned int convert2TotalTime(unsigned int lineTimeInus, unsigned int time);
unsigned int convert2LineCount(unsigned int lineTimeInNs, unsigned int val);
unsigned int fs_alg_get_vsync_data(unsigned int solveIdxs[], unsigned int len);

#ifdef FS_UT
unsigned int fs_alg_write_shutter(unsigned int idx);
#endif


/* Dump & Debug function */
void fs_alg_dump_fs_inst_data(unsigned int idx);
void fs_alg_dump_all_fs_inst_data(void);


/*******************************************************************************
 * fs algo operation functions (set information data)
 ******************************************************************************/
void fs_alg_set_anti_flicker(unsigned int idx, unsigned int flag);

void fs_alg_update_tg(unsigned int idx, unsigned int tg);

void fs_alg_update_min_fl_lc(unsigned int idx, unsigned int min_fl_lc);

void fs_alg_set_streaming_st_data(
	unsigned int idx, struct fs_streaming_st *pData);

void fs_alg_set_perframe_st_data(
	unsigned int idx, struct fs_perframe_st *pData);

void fs_alg_reset_fs_inst(unsigned int idx);

void fs_alg_set_frame_record_st_data(
	unsigned int idx, struct frame_record_st data[]);


/*******************************************************************************
 * Frame Sync Algorithm function
 ******************************************************************************/

/* return ("0" -> done; "non 0" -> error ?) */
unsigned int fs_alg_solve_frame_length(
	unsigned int solveIdxs[],
	unsigned int framelength[], unsigned int len);


#endif
