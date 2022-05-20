/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _HW_SENSOR_SYNC_ALGO_H
#define _HW_SENSOR_SYNC_ALGO_H

#include "frame_sync.h"

#define ENO_HW_FS_UNHANDLEED 1

#if defined(FS_UT)
/* Sync from kd_imgsensor_define_v4l2.h */
/* Define for sensor sync mode (bitwise) */
#define SENSOR_NO_SYNC_MODE 0
#define SENSOR_MASTER_SYNC_MODE 1
#define SENSOR_SLAVE_SYNC_MODE 2

/* Only for FS_UT using */
/* Default is using linux/minmax.h (in linux/kernel.h) */
#define max(a, b) \
({ \
	__typeof__(a) _a = (a); \
	__typeof__(b) _b = (b); \
	_a > _b ? _a : _b; \
})

#define min(a, b) \
({ \
	__typeof__(a) _a = (a); \
	__typeof__(b) _b = (b); \
	_a < _b ? _a : _b; \
})
#endif // FS_UT


/* dump info */
void hw_fs_dump_dynamic_para(unsigned int idx);

/* Set data when sensor streaming */
void hw_fs_alg_set_streaming_st_data(
	unsigned int idx, struct fs_streaming_st *pData);

void hw_fs_alg_update_min_fl_lc(unsigned int idx,
	unsigned int min_fl_lc);

void hw_fs_alg_set_perframe_st_data(
	unsigned int idx, struct fs_perframe_st *pData);

int handle_by_hw_sensor_sync(unsigned int solveIdxs[], unsigned int len);

unsigned int hw_fs_alg_solve_frame_length(
	unsigned int solveIdxs[],
	unsigned int framelength_lc[], unsigned int len);

#endif
