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

#define GED_OK                        0
#define GED_ERROR_FAIL                1
#define GED_ERROR_OOM                 2
#define GED_ERROR_OUT_OF_FD           3
#define GED_ERROR_FAIL_WITH_LIMIT     4
#define GED_ERROR_TIMEOUT             5
#define GED_ERROR_CMD_NOT_PROCESSED	  6
#define GED_ERROR_INVALID_PARAMS      7
#define GED_ERROR_INTENTIONAL_BLOCK   8
#define GED_ERROR                     int

#define GED_HANDLE                    void*
#define GED_FRR_HANDLE                void*
#define GED_GLES_HANDLE               void*
#define GED_SWD_HANDLE                void*
#define GED_LOG_HANDLE                void*
#define GED_KPI_HANDLE                void*
#define GED_LOG_BUF_HANDLE unsigned   int

#define	GED_FALSE   0
#define GED_TRUE    1
#define GED_BOOL    int

#define GED_LOADING              0
#define GED_IDLE                 1
#define GED_BLOCKING             2
#define GED_PRE_FREQ             3
#define GED_PRE_FREQ_IDX         4
#define GED_CUR_FREQ             5
#define GED_CUR_FREQ_IDX         6
#define GED_MAX_FREQ_IDX         7
#define GED_MAX_FREQ_IDX_FREQ	 8
#define GED_MIN_FREQ_IDX         9
#define GED_MIN_FREQ_IDX_FREQ    10
#define GED_3D_FENCE_DONE_TIME   11
#define GED_VSYNC_OFFSET         12
#define GED_EVENT_STATUS         13
#define GED_EVENT_DEBUG_STATUS   14
#define GED_EVENT_GAS_MODE       15
#define GED_SRV_SUICIDE	         16
#define GED_PRE_HALF_PERIOD	     17
#define GED_LATEST_START         18
#define GED_FPS	                 19
#define GED_INFO_SIZE            20
#define GED_INFO                 int


#define GED_DVFS_VSYNC_OFFSET_DEBUG_CLEAR_EVENT	         0
#define GED_DVFS_VSYNC_OFFSET_FORCE_ON                   1
#define GED_DVFS_VSYNC_OFFSET_FORCE_OFF                  2
#define GED_DVFS_VSYNC_OFFSET_TOUCH_EVENT                3
#define GED_DVFS_VSYNC_OFFSET_THERMAL_EVENT              4
#define GED_DVFS_VSYNC_OFFSET_WFD_EVENT                  5
#define GED_DVFS_VSYNC_OFFSET_MHL_EVENT                  6
#define GED_DVFS_VSYNC_OFFSET_GAS_EVENT                  7
#define GED_DVFS_VSYNC_OFFSET_LOW_POWER_MODE_EVENT       8
#define GED_DVFS_VSYNC_OFFSET_MHL4K_VID_EVENT            9
#define GED_DVFS_VSYNC_OFFSET_VR_EVENT                   10
#define GED_DVFS_BOOST_HOST_EVENT                        11
#define GED_DVFS_VSYNC_OFFSET_VILTE_VID_EVENT            12
#define GED_DVFS_VSYNC_OFFSET_LOW_LATENCY_MODE_EVENT     13
#define GED_DVFS_VSYNC_OFFSET_DHWC_EVENT                 14
#define GED_DVFS_VSYNC_OFFSET_SWITCH_CMD                 int


#define GED_VSYNC_SW_EVENT	0
#define GED_VSYNC_HW_EVENT	1
#define GED_VSYNC_TYPE      int

enum {
	GAS_CATEGORY_GAME,
	GAS_CATEGORY_OTHERS,
};


#define	GED_BOOST_GPU_FREQ_LEVEL_MAX  100
#define GED_BOOST_GPU_FREQ_LEVEL      int

#define GED_BRIDGE_IN_LOGBUF_SIZE     320

	/* bit 0~7 reserved for internal used */
#define GED_RESVERED                  0xFF
	/* log with a prefix kernel time */
#define GED_LOG_ATTR_TIME             0x100
	/* log with a prefix user time, pid, tid */
#define GED_LOG_ATTR_TIME_TPT         0x200

struct GED_DVFS_UM_QUERY_PACK {
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
};

#endif
