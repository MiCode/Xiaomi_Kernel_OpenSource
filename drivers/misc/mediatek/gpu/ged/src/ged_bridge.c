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

#include <linux/kernel.h>
#include <mt-plat/mtk_gpu_utility.h>
#include <mt-plat/fpsgo_common.h>

#include "ged_base.h"
#include "ged_bridge.h"
#include "ged_log.h"
#include "ged_profile_dvfs.h"
#include "ged_monitor_3D_fence.h"
#include "ged_notify_sw_vsync.h"
#include "ged_dvfs.h"
#include <linux/module.h>
#include "ged_kpi.h"
#include "ged.h"
#include "ged_frr.h"

static unsigned int ged_boost_enable = 1;
//-----------------------------------------------------------------------------
int ged_bridge_log_buf_get(
		GED_BRIDGE_IN_LOGBUFGET *psLogBufGetIN,
		GED_BRIDGE_OUT_LOGBUFGET *psLogBufGetOUT)
{
	psLogBufGetOUT->hLogBuf = ged_log_buf_get(psLogBufGetIN->acName);
	psLogBufGetOUT->eError = psLogBufGetOUT->hLogBuf ? GED_OK : GED_ERROR_FAIL;
	return 0;
}
//-----------------------------------------------------------------------------
int ged_bridge_log_buf_write(
		GED_BRIDGE_IN_LOGBUFWRITE *psLogBufWriteIN,
		GED_BRIDGE_OUT_LOGBUFWRITE *psLogBufWriteOUT)
{
	psLogBufWriteOUT->eError = 
		ged_log_buf_print2(psLogBufWriteIN->hLogBuf, psLogBufWriteIN->attrs, "%s", psLogBufWriteIN->acLogBuf);
	return 0;
}
//-----------------------------------------------------------------------------
int ged_bridge_log_buf_reset(
		GED_BRIDGE_IN_LOGBUFRESET *psLogBufResetIn,
		GED_BRIDGE_OUT_LOGBUFRESET *psLogBufResetOUT)
{
	psLogBufResetOUT->eError = ged_log_buf_reset(psLogBufResetIn->hLogBuf);
	return 0;
}
//-----------------------------------------------------------------------------
int ged_bridge_boost_gpu_freq(
		GED_BRIDGE_IN_BOOSTGPUFREQ *psBoostGpuFreqIN,
		GED_BRIDGE_OUT_BOOSTGPUFREQ *psBoostGpuFreqOUT)
{
#if 1
	psBoostGpuFreqOUT->eError = (mtk_set_bottom_gpu_freq(
		psBoostGpuFreqIN->eGPUFreqLevel) == true) ?
		GED_OK : GED_ERROR_FAIL;
#else
	unsigned int ui32Count;
	if (mtk_custom_get_gpu_freq_level_count(&ui32Count))
	{
		int i32Level = (ui32Count - 1) - GED_BOOST_GPU_FREQ_LEVEL_MAX - psBoostGpuFreqIN->eGPUFreqLevel;
		mtk_boost_gpu_freq(i32Level);
		psBoostGpuFreqOUT->eError = GED_OK;
	}
	else
	{
		psBoostGpuFreqOUT->eError = GED_ERROR_FAIL;
	}
#endif
	return 0;
}
//-----------------------------------------------------------------------------
int ged_bridge_monitor_3D_fence(
		GED_BRIDGE_IN_MONITOR3DFENCE *psMonitor3DFenceINT,
		GED_BRIDGE_OUT_MONITOR3DFENCE *psMonitor3DFenceOUT)
{
	psMonitor3DFenceOUT->eError =
		ged_monitor_3D_fence_add(psMonitor3DFenceINT->fd);
	return 0;
}
//-----------------------------------------------------------------------------
int ged_bridge_query_info(
		GED_BRIDGE_IN_QUERY_INFO *psQueryInfoINT,
		GED_BRIDGE_OUT_QUERY_INFO *psQueryInfoOUT)
{
	psQueryInfoOUT->retrieve = ged_query_info( psQueryInfoINT->eType);
	return 0;
}
//-----------------------------------------------------------------------------
int ged_bridge_notify_vsync(
		GED_BRIDGE_IN_NOTIFY_VSYNC *psNotifyVsyncINT,
		GED_BRIDGE_OUT_NOTIFY_VSYNC *psNotifyVsyncOUT)
{
	psNotifyVsyncOUT->eError = 
		//ged_notify_vsync(psNotifyVsyncINT->eType, &psNotifyVsyncOUT->t);
		ged_notify_sw_vsync(psNotifyVsyncINT->eType, &psNotifyVsyncOUT->sQueryData);

	return 0;
}
//-----------------------------------------------------------------------------
int ged_bridge_dvfs_probe(
		GED_BRIDGE_IN_DVFS_PROBE *psDVFSProbeINT, 
		GED_BRIDGE_OUT_DVFS_PROBE *psDVFSProbeOUT)
{
	psDVFSProbeOUT->eError = ged_dvfs_probe(psDVFSProbeINT->pid);
	return 0;
}

//-----------------------------------------------------------------------------
int ged_bridge_dvfs_um_retrun(
		GED_BRIDGE_IN_DVFS_UM_RETURN *psDVFS_UM_returnINT, 
		GED_BRIDGE_OUT_DVFS_UM_RETURN *psDVFS_UM_returnOUT)
{
	psDVFS_UM_returnOUT->eError = 
		ged_dvfs_um_commit( psDVFS_UM_returnINT->gpu_tar_freq,
				psDVFS_UM_returnINT->bFallback);
	return 0;
}

//-----------------------------------------------------------------------------
int ged_bridge_event_notify(
		GED_BRIDGE_IN_EVENT_NOTIFY *psEVENT_NOTIFYINT, 
		GED_BRIDGE_OUT_EVENT_NOTIFY *psEVENT_NOTIFYOUT)
{
    if (ged_boost_enable)
    {
        psEVENT_NOTIFYOUT->eError = 
            ged_dvfs_vsync_offset_event_switch(psEVENT_NOTIFYINT->eEvent,
                                                psEVENT_NOTIFYINT->bSwitch);
    }
    else
    {
        psEVENT_NOTIFYOUT->eError = GED_OK;
    }

	return 0;
}

/* ----------------------------------------------------------------------------- */
int ged_bridge_gpu_timestamp(
	GED_BRIDGE_IN_GPU_TIMESTAMP * psGpuBeginINT,
	GED_BRIDGE_OUT_GPU_TIMESTAMP *psGpuBeginOUT)
{
	if (ged_kpi_enabled() == 1) {
		if (psGpuBeginINT->QedBuffer_length == -2)
			psGpuBeginOUT->eError =
				ged_kpi_dequeue_buffer_ts(psGpuBeginINT->pid,
									psGpuBeginINT->ullWnd,
									psGpuBeginINT->i32FrameID,
									psGpuBeginINT->fence_fd,
									psGpuBeginINT->isSF);
		else if (psGpuBeginINT->QedBuffer_length != -1)
			psGpuBeginOUT->eError =
				ged_kpi_queue_buffer_ts(psGpuBeginINT->pid,
										psGpuBeginINT->ullWnd,
										psGpuBeginINT->i32FrameID,
										psGpuBeginINT->fence_fd,
										psGpuBeginINT->QedBuffer_length);
		else
			psGpuBeginOUT->eError =
				ged_kpi_acquire_buffer_ts(psGpuBeginINT->pid,
										psGpuBeginINT->ullWnd,
										psGpuBeginINT->i32FrameID);
		psGpuBeginOUT->is_ged_kpi_enabled = 1;
	} else {
		psGpuBeginOUT->is_ged_kpi_enabled = 0;
		psGpuBeginOUT->eError = GED_OK;
	}
	return 0;
}

/* ----------------------------------------------------------------------------- */
#ifdef MTK_FRR20
int ged_bridge_wait_hw_vsync(void)
{
	ged_frr_wait_hw_vsync();
	return 0;
}

/* ----------------------------------------------------------------------------- */
int ged_bridge_query_target_fps(
	GED_BRIDGE_IN_QUERY_TARGET_FPS * in,
	GED_BRIDGE_OUT_QUERY_TARGET_FPS *out)
{
	ged_frr_fence2context_table_update(in->pid, in->cid, in->fenceFd);
	out->fps = ged_frr_get_fps(in->pid, in->cid);
	return 0;
}
#endif

//-----------------------------------------------------------------------------
int ged_bridge_gpu_hint_to_cpu(
		GED_BRIDGE_IN_GPU_HINT_TO_CPU * in,
		GED_BRIDGE_OUT_GPU_HINT_TO_CPU *out)
{
	int ret = 0;

	ret = fpsgo_notify_gpu_block(in->tid, in->i32BridgeFD, in->hint);

	out->eError = GED_OK;
	out->boost_flag = ret;
	out->boost_value = ged_dvfs_boost_value();
	return 0;
}

module_param(ged_boost_enable, uint, 0644);
