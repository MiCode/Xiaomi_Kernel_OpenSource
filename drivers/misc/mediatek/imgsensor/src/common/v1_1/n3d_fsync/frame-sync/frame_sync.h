/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _FRAME_SYNC_H
#define _FRAME_SYNC_H

#include "frame_sync_def.h"


/*******************************************************************************
 * The Method for Frame Sync Register Sensor (default pls using sensor_idx).
 ******************************************************************************/
enum CHECK_SENSOR_INFO_METHOD {
	BY_NONE = 0,
	BY_SENSOR_ID = 1,
	BY_SENSOR_IDX = 2,
};

/* set your choose */
#define REGISTER_METHOD BY_SENSOR_IDX
#define REG_INFO "SENSOR_IDX" // for log showing info
/******************************************************************************/


enum FS_STATUS {
	FS_NONE = 0,
	FS_INITIALIZED = 1,
	FS_WAIT_FOR_SYNCFRAME_START = 2,
	FS_START_TO_GET_PERFRAME_CTRL = 3,
};


/* callback function pointer for setting framelength */
/* see adaptor-ctrl.h */
typedef int (*callback_set_framelength)(void *p_ctx,
				unsigned int cmd_id,
				unsigned int framelength);


struct fs_streaming_st {
	unsigned int sensor_id;
	unsigned int sensor_idx;

	unsigned int tg;

	unsigned int fl_active_delay;
	unsigned int def_fl_lc;          // default framelength_lc
	unsigned int max_fl_lc;          // for framelength boundary check
	unsigned int def_shutter_lc;     // default shutter_lc

	/* callback function */
	callback_set_framelength func_ptr;
	void *p_ctx;
};


struct fs_perframe_st {
	unsigned int sensor_id;
	unsigned int sensor_idx;

	/* bellow items can be query from "subdrv_ctx" */
	unsigned int min_fl_lc;          // also means max frame rate
	unsigned int shutter_lc;
	unsigned int margin_lc;
	unsigned int flicker_en;
	unsigned int out_fl_lc;

	/* for on-the-fly mode change */
	unsigned int pclk;               // write_shutter(), set_max_framerate()
	unsigned int linelength;         // write_shutter(), set_max_framerate()
	/* lineTimeInNs ~= 10^9 * (linelength/pclk) */
	unsigned int lineTimeInNs;

	/* callback function using */
	unsigned int cmd_id;
};





/*******************************************************************************
 * Frame Sync member functions.
 ******************************************************************************/
struct FrameSync {
	/* according to sensor idx, register image sensor info */
	unsigned int (*fs_streaming)(
				unsigned int flag,
				struct fs_streaming_st *streaming_st);


	/* enable / disable frame sync processing for this sensor ident */
	void (*fs_set_sync)(unsigned int sensor_ident, unsigned int flag);


	/**********************************************************************/
	// API is defined in frame_sync_camsys.h
	//
	// flag = 1 => Start
	//     return a integer represents how many sensors that
	//     are valid for sync.
	//
	// flag = 0 => End
	//     return a integer represents how many Perframe_Ctrl is achieve.
	/**********************************************************************/
	unsigned int (*fs_sync_frame)(unsigned int flag);


	/* frame sync set shutter */
	void (*fs_set_shutter)(struct fs_perframe_st *perframe_st);


	/* for cam mux switch and sensor streaming on before setup cam mux */
	void (*fs_update_tg)(unsigned int ident, unsigned int tg);


	/* update fs_perframe_st data */
	/*     (for v4l2_ctrl_request_setup order) */
	/*     (set_framelength is called after set_anti_flicker) */
	void (*fs_update_auto_flicker_mode)(
				unsigned int ident, unsigned int en);


	/* update fs_perframe_st data */
	/*     (for v4l2_ctrl_request_setup order) */
	/*     (set_max_framerate_by_scenario is called after set_shutter) */
	void (*fs_update_min_framelength_lc)(
				unsigned int ident, unsigned int min_fl_lc);


	/**********************************************************************/
	/* get frame sync status for this sensor_id */
	/* return: (0 / 1) => (disable / enable) */
	/**********************************************************************/
	unsigned int (*fs_is_set_sync)(unsigned int sensor_id);
};


/*******************************************************************************
 * Frame Sync init function.
 *
 *    init FrameSync object.
 *    get FrameSync function for operation.
 *
 *    return: (0 / 1) => (no error / error)
 ******************************************************************************/
unsigned int FrameSyncInit(struct FrameSync **framesync);


#endif
