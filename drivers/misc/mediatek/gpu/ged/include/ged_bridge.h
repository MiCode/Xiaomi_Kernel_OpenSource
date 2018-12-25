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

#ifndef __GED_BRIDGE_H__
#define __GED_BRIDGE_H__

#include "ged_base.h"
#include "ged_log.h"
#include "ged_type.h"

#include "ged_bridge_id.h"


/*****************************************************************************
 *  BRIDGE FUNCTIONS
 *****************************************************************************/

int ged_bridge_log_buf_get(
    GED_BRIDGE_IN_LOGBUFGET *psLogBufGetIN,
    GED_BRIDGE_OUT_LOGBUFGET *psLogBufGetOUT);

int ged_bridge_log_buf_write(
    GED_BRIDGE_IN_LOGBUFWRITE *psLogBufWriteIN,
    GED_BRIDGE_OUT_LOGBUFWRITE *psLogBufWriteOUT);

int ged_bridge_log_buf_reset(
    GED_BRIDGE_IN_LOGBUFRESET *psLogBufResetIn,
    GED_BRIDGE_OUT_LOGBUFRESET *psLogBufResetOUT);

int ged_bridge_boost_gpu_freq(
    GED_BRIDGE_IN_BOOSTGPUFREQ *psBoostGpuFreqIN,
    GED_BRIDGE_OUT_BOOSTGPUFREQ *psBoostGpuFreqOUT);

int ged_bridge_monitor_3D_fence(
    GED_BRIDGE_IN_MONITOR3DFENCE *psMonitor3DFenceINT,
    GED_BRIDGE_OUT_MONITOR3DFENCE *psMonitor3DFenceOUT);

int ged_bridge_query_info(
    GED_BRIDGE_IN_QUERY_INFO *psQueryInfoINT,
    GED_BRIDGE_OUT_QUERY_INFO *psQueryInfoOUT);

int ged_bridge_notify_vsync(
    GED_BRIDGE_IN_NOTIFY_VSYNC *psNotifyVsyncINT,
    GED_BRIDGE_OUT_NOTIFY_VSYNC *psNotifyVsyncOUT);

int ged_bridge_dvfs_probe(
    GED_BRIDGE_IN_DVFS_PROBE *psDVFSProbeINT, 
    GED_BRIDGE_OUT_DVFS_PROBE *psDVFSProbeOUT);

int ged_bridge_dvfs_um_retrun(
    GED_BRIDGE_IN_DVFS_UM_RETURN *psDVFS_UM_returnINT, 
    GED_BRIDGE_OUT_DVFS_UM_RETURN *psDVFS_UM_returnOUT);

int ged_bridge_event_notify(
		GED_BRIDGE_IN_EVENT_NOTIFY *psEVENT_NOTIFYINT, 
		GED_BRIDGE_OUT_EVENT_NOTIFY *psEVENT_NOTIFYOUT);

int ged_bridge_gpu_timestamp(
	GED_BRIDGE_IN_GPU_TIMESTAMP * psGpuBeginINT,
	GED_BRIDGE_OUT_GPU_TIMESTAMP *psGpuBeginOUT);

int ged_bridge_ge_alloc(
		struct GED_BRIDGE_IN_GE_ALLOC_TAG  *psALLOC_IN,
		struct GED_BRIDGE_OUT_GE_ALLOC_TAG *psALLOC_OUT);

int ged_bridge_ge_get(
		struct GED_BRIDGE_IN_GE_GET_TAG  *psGET_IN,
		struct GED_BRIDGE_OUT_GE_GET_TAG *psGET_OUT);

int ged_bridge_ge_set(
		struct GED_BRIDGE_IN_GE_SET_TAG  *psSET_IN,
		struct GED_BRIDGE_OUT_GE_SET_TAG *psSET_OUT);

int ged_bridge_ge_info(
		struct GED_BRIDGE_IN_GE_INFO_TAG  *psINFO_IN,
		struct GED_BRIDGE_OUT_GE_INFO_TAG *psINFO_OUT);

#ifdef ENABLE_FRR_FOR_MT6XXX_PLATFORM
int ged_bridge_vsync_wait(void *IN, void *OUT);
#endif

#endif
