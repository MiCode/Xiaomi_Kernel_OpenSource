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

#include "ged_error.h"

typedef void* GED_HANDLE;

typedef unsigned int GED_LOG_BUF_HANDLE;

typedef	enum GED_BOOL_TAG
{
	GED_FALSE,
	GED_TRUE
} GED_BOOL;

typedef enum GED_INFO_TAG
{
    GED_LOADING,
    GED_IDLE,
    GED_BLOCKING,
    GED_PRE_FREQ,
    GED_PRE_FREQ_IDX,
    GED_CUR_FREQ,
    GED_CUR_FREQ_IDX,
    GED_MAX_FREQ_IDX,
    GED_MAX_FREQ_IDX_FREQ,
    GED_MIN_FREQ_IDX,
    GED_MIN_FREQ_IDX_FREQ,    
    GED_3D_FENCE_DONE_TIME,
    GED_VSYNC_OFFSET,
    GED_EVENT_STATUS,
    GED_EVENT_DEBUG_STATUS,
    GED_EVENT_GAS_MODE,
    GED_SRV_SUICIDE,
    GED_PRE_HALF_PERIOD,
    GED_LATEST_START,
    GED_FPS,
    GED_UNDEFINED
} GED_INFO;

typedef enum GED_VSYNC_TYPE_TAG
{
    GED_VSYNC_SW_EVENT,
    GED_VSYNC_HW_EVENT
} GED_VSYNC_TYPE;

typedef struct GED_DVFS_UM_QUERY_PACK_TAG
{
    	char bFirstBorn;
    unsigned int ui32GPULoading;
    unsigned int ui32GPUFreqID;
    unsigned long gpu_cur_freq;
    unsigned long gpu_pre_freq;
    long long usT;
    long long nsOffset;
    unsigned long ul3DFenceDoneTime;    
    unsigned long ulPreCalResetTS_us;
    unsigned long ulWorkingPeriod_us;
}GED_DVFS_UM_QUERY_PACK;

enum {
	GAS_CATEGORY_GAME,
	GAS_CATEGORY_OTHERS,
};

typedef enum GED_DVFS_VSYNC_OFFSET_SWITCH_CMD_TAG
{
	GED_DVFS_VSYNC_OFFSET_DEBUG_CLEAR_EVENT,
	GED_DVFS_VSYNC_OFFSET_FORCE_ON,
	GED_DVFS_VSYNC_OFFSET_FORCE_OFF,
	GED_DVFS_VSYNC_OFFSET_TOUCH_EVENT,
	GED_DVFS_VSYNC_OFFSET_THERMAL_EVENT,
	GED_DVFS_VSYNC_OFFSET_WFD_EVENT,
	GED_DVFS_VSYNC_OFFSET_MHL_EVENT,
	GED_DVFS_VSYNC_OFFSET_GAS_EVENT,
	GED_DVFS_VSYNC_OFFSET_LOW_POWER_MODE_EVENT,
	GED_DVFS_VSYNC_OFFSET_MHL4K_VID_EVENT,
	GED_DVFS_VSYNC_OFFSET_VR_EVENT,
	GED_DVFS_VSYNC_OFFSET_VILTE_VID_EVENT,
	GED_DVFS_BOOST_HOST_EVENT,
} GED_DVFS_VSYNC_OFFSET_SWITCH_CMD;

typedef struct {
	void (*free_func)(void *);
} GED_FILE_PRIVATE_BASE;

#endif
