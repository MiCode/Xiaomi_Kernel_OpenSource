/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _FRAME_SYNC_H
#define _FRAME_SYNC_H

#include "frame_sync_def.h"


/*******************************************************************************
 * The Method for FrameSync Register Sensor (default pls using sensor_idx).
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
	FS_STATUS_UNKNOWN
};


/*******************************************************************************
 * The Method for FrameSync sync type.
 ******************************************************************************/
enum FS_SYNC_TYPE {
	FS_SYNC_TYPE_NONE = 0,
	FS_SYNC_TYPE_VSYNC = 1 << 1,
	FS_SYNC_TYPE_READOUT_CENTER = 1 << 2,
	FS_SYNC_TYPE_LE = 1 << 3,
	FS_SYNC_TYPE_SE = 1 << 4,
};
/******************************************************************************/


/*******************************************************************************
 * The Feature mode for FrameSync.
 ******************************************************************************/
enum FS_FEATURE_MODE {
	FS_FT_MODE_NORMAL = 0,
	FS_FT_MODE_STG_HDR = 1,

	/* N:1 / M-Stream */
	FS_FT_MODE_FRAME_TAG = 1 << 1, /* (N:1) Not one-to-one sync */
	FS_FT_MODE_ASSIGN_FRAME_TAG = 1 << 2, /* (M-Stream) Not one-to-one sync */

	FS_FT_MODE_N_1_ON = 1 << 3,
	FS_FT_MODE_N_1_KEEP = 1 << 4,
	FS_FT_MODE_N_1_OFF = 1 << 5,
};
/******************************************************************************/


/*******************************************************************************
 * The Method for FrameSync standalone (SA) algorithm.
 ******************************************************************************/
enum FS_SA_METHOD {
	FS_SA_ADAPTIVE_MASTER = 0,
};
/******************************************************************************/


/* callback function pointer for setting framelength */
/* see adaptor-ctrl.h */
typedef int (*callback_set_framelength)(void *p_ctx,
				unsigned int cmd_id,
				unsigned int framelength);


/*******************************************************************************
 * FrameSync HDR structure & variables
 ******************************************************************************/
enum FS_HDR_EXP {
	FS_HDR_NONE = -1,
	FS_HDR_LE = 0,
	FS_HDR_ME = 1,
	FS_HDR_SE = 2,
	FS_HDR_SSE = 3,
	FS_HDR_SSSE = 4,
	FS_HDR_MAX
};


/* get exp location by hdr_exp_idx_map[exp_cnt][exp] */
/* -1: error handle for mapping to a non valid idx and */
/*     a info/hint for fs_alg_setup_multi_exp_value() */
const static int hdr_exp_idx_map[][FS_HDR_MAX] = {
	/* order => LE:0 / ME:1 / SE:2 / SSE:3 / SSSE:4, (MAX:5) */
	{FS_HDR_NONE, FS_HDR_NONE, FS_HDR_NONE, FS_HDR_NONE, FS_HDR_NONE}, // exp cnt:0
	{FS_HDR_LE, FS_HDR_NONE, FS_HDR_NONE, FS_HDR_NONE, FS_HDR_NONE}, // exp cnt:1
	{FS_HDR_LE, FS_HDR_SE, FS_HDR_NONE, FS_HDR_NONE, FS_HDR_NONE}, // exp cnt:2
	{FS_HDR_LE, FS_HDR_ME, FS_HDR_SE, FS_HDR_NONE, FS_HDR_NONE}, // exp cnt:3

	/* T.B.D. */
	{FS_HDR_LE, FS_HDR_ME, FS_HDR_SE, FS_HDR_SSE, FS_HDR_NONE}, // exp cnt:4
	{FS_HDR_LE, FS_HDR_ME, FS_HDR_SE, FS_HDR_SSE, FS_HDR_SSSE}  // exp cnt:5
};


struct fs_hdr_exp_st {
	unsigned int mode_exp_cnt;       // exp cnt from HDR mode
	unsigned int ae_exp_cnt;         // exp cnt from ae set ctrl

	unsigned int exp_lc[FS_HDR_MAX];

	/* stagger read offset change */
	unsigned int readout_len_lc;
	unsigned int read_margin_lc;
};
/******************************************************************************/


struct fs_streaming_st {
	unsigned int sensor_id;
	unsigned int sensor_idx;

	unsigned int tg;

	unsigned int fl_active_delay;
	unsigned int def_fl_lc;          // default framelength_lc
	unsigned int max_fl_lc;          // for framelength boundary check
	unsigned int def_shutter_lc;     // default shutter_lc
	unsigned int sync_mode;          // sync operate mode. none/master/slave

	struct fs_hdr_exp_st hdr_exp;    // hdr exposure settings

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

	struct fs_hdr_exp_st hdr_exp;    // hdr exposure settings

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
	void (*fs_update_shutter)(struct fs_perframe_st *frameCtrl);


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


	/* set extend framelength for STG sensor doing seamless switch */
	/*     parameter: set lc => ext_fl_lc != 0 && ext_fl_us == 0 */
	/*                set us => ext_fl_lc == 0 && ext_fl_us != 0 */
	/*                clear  => ext_fl_lc == 0 && ext_fl_us == 0 */
	void (*fs_set_extend_framelength)(
		unsigned int ident,
		unsigned int ext_fl_lc, unsigned int ext_fl_us);


	/* for notify FrameSync sensor doing seamless switch using */
	void (*fs_seamless_switch)(unsigned int ident);


	/* for choosing FrameSync StandAlone algorithm */
	void (*fs_set_using_sa_mode)(unsigned int en);


	/* for cam-sys assign taget vsync at subsample */
	/* e.g: SE:0/LE:1, target vsync:0 (in this case is SE) */
	/* => f_tag:0, 1, 0, 1, ... */
	void (*fs_set_frame_tag)(unsigned int ident, unsigned int f_tag);


	void (*fs_n_1_en)(unsigned int ident, unsigned int n, unsigned int en);


	void (*fs_mstream_en)(unsigned int ident, unsigned int en);


	void (*fs_notify_vsync)(unsigned int ident);


	/**********************************************************************/
	/* get frame sync status for this sensor_id */
	/* return: (0 / 1) => (disable / enable) */
	/**********************************************************************/
	unsigned int (*fs_is_set_sync)(unsigned int sensor_id);
};


#if defined(SUPPORT_FS_NEW_METHOD)
void fs_sa_request_switch_master(unsigned int idx);
#endif


/*
 * Frame Sync init function.
 *
 *    init FrameSync object.
 *    get FrameSync function for operation.
 *
 *    return: (0 / 1) => (no error / error)
 */
#if !defined(FS_UT)
unsigned int FrameSyncInit(struct FrameSync **pframeSync, struct device *dev);
void FrameSyncUnInit(struct device *dev);
#else // FS_UT
unsigned int FrameSyncInit(struct FrameSync **framesync);
#endif // FS_UT




#endif
