/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __GED_TYPE_H__
#define __GED_TYPE_H__

typedef enum GED_ERROR_TAG {
	GED_OK,
	GED_ERROR_FAIL,
	GED_ERROR_OOM,
	GED_ERROR_OUT_OF_FD,
	GED_ERROR_FAIL_WITH_LIMIT,
	GED_ERROR_TIMEOUT,
	GED_ERROR_CMD_NOT_PROCESSED,
	GED_ERROR_INVALID_PARAMS,
	GED_ERROR_INTENTIONAL_BLOCK,
} GED_ERROR;

typedef void *GED_HANDLE;

typedef void *GED_FRR_HANDLE;

typedef void *GED_GLES_HANDLE;

typedef void *GED_SWD_HANDLE;

typedef void *GED_LOG_HANDLE;

typedef void *GED_KPI_HANDLE;

typedef unsigned int GED_LOG_BUF_HANDLE;

typedef enum GED_BOOL_TAG {
	GED_FALSE,
	GED_TRUE
} GED_BOOL;

typedef enum GED_INFO_TAG {
	GED_LOADING = 0,
	GED_IDLE = 1,
	GED_BLOCKING = 2,
	GED_PRE_FREQ = 3,
	GED_PRE_FREQ_IDX = 4,
	GED_CUR_FREQ = 5,
	GED_CUR_FREQ_IDX = 6,
	GED_MAX_FREQ_IDX = 7,
	GED_MAX_FREQ_IDX_FREQ = 8,
	GED_MIN_FREQ_IDX = 9,
	GED_MIN_FREQ_IDX_FREQ = 10,
	GED_3D_FENCE_DONE_TIME = 11,
	GED_VSYNC_OFFSET = 12,
	GED_EVENT_STATUS = 13,
	GED_EVENT_DEBUG_STATUS = 14,
	GED_EVENT_GAS_MODE = 15,
	GED_SRV_SUICIDE = 16,
	GED_PRE_HALF_PERIOD = 17,
	GED_LATEST_START = 18,
	GED_FPS = 19,

	GED_INFO_SIZE
} GED_INFO;

typedef enum {
	GED_DVFS_VSYNC_OFFSET_DEBUG_CLEAR_EVENT = 0,
	GED_DVFS_VSYNC_OFFSET_FORCE_ON = 1,
	GED_DVFS_VSYNC_OFFSET_FORCE_OFF = 2,
	GED_DVFS_VSYNC_OFFSET_TOUCH_EVENT = 3,
	GED_DVFS_VSYNC_OFFSET_THERMAL_EVENT = 4,
	GED_DVFS_VSYNC_OFFSET_WFD_EVENT = 5,
	GED_DVFS_VSYNC_OFFSET_MHL_EVENT = 6,
	GED_DVFS_VSYNC_OFFSET_GAS_EVENT = 7,
	GED_DVFS_VSYNC_OFFSET_LOW_POWER_MODE_EVENT = 8,
	GED_DVFS_VSYNC_OFFSET_MHL4K_VID_EVENT = 9,
	GED_DVFS_VSYNC_OFFSET_VR_EVENT = 10,
	GED_DVFS_BOOST_HOST_EVENT = 11,
	GED_DVFS_VSYNC_OFFSET_VILTE_VID_EVENT = 12,
} GED_DVFS_VSYNC_OFFSET_SWITCH_CMD;

typedef enum {
	GED_VSYNC_SW_EVENT,
	GED_VSYNC_HW_EVENT
} GED_VSYNC_TYPE;

enum {
	GAS_CATEGORY_GAME,
	GAS_CATEGORY_OTHERS,
};

typedef enum {
	GED_BOOST_GPU_FREQ_LEVEL_MAX = 100
} GED_BOOST_GPU_FREQ_LEVEL;

#define GED_BRIDGE_IN_LOGBUF_SIZE 320
enum {
	/* bit 0~7 reserved for internal used */
	GED_RESVERED                = 0xFF,

	/* log with a prefix kernel time */
	GED_LOG_ATTR_TIME           = 0x100,

	/* log with a prefix user time, pid, tid */
	GED_LOG_ATTR_TIME_TPT       = 0x200,
};

typedef struct GED_DVFS_UM_QUERY_PACK_TAG {
	char bFirstBorn;
	unsigned int ui32GPULoading;
	unsigned int ui32GPUFreqID;
	unsigned int gpu_cur_freq;
	unsigned int gpu_pre_freq;
	long long usT;
	long long nsOffset;
	unsigned long long ul3DFenceDoneTime;
	unsigned long long ulPreCalResetTS_us;
	unsigned long long ulWorkingPeriod_us;
	unsigned int ui32TargetPeriod_us;
	unsigned int ui32BoostValue;
} GED_DVFS_UM_QUERY_PACK;

#endif
