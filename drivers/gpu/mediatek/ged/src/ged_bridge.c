// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <mt-plat/mtk_gpu_utility.h>
/* To-Do: FPSGO*/
/* #include <mt-plat/fpsgo_common.h>*/

#include "ged_base.h"
#include "ged_bridge.h"
#include "ged_log.h"
#include "ged_monitor_3D_fence.h"
#include "ged_notify_sw_vsync.h"
#include "ged_dvfs.h"
#include <linux/module.h>
#include "ged_kpi.h"
#include "ged.h"

static unsigned int ged_boost_enable = 1;
//-----------------------------------------------------------------------------
int ged_bridge_log_buf_get(
	struct GED_BRIDGE_IN_LOGBUFGET *psLogBufGetIN,
	struct GED_BRIDGE_OUT_LOGBUFGET *psLogBufGetOUT)
{
	psLogBufGetOUT->hLogBuf = ged_log_buf_get(psLogBufGetIN->acName);
	psLogBufGetOUT->eError =
		psLogBufGetOUT->hLogBuf ? GED_OK : GED_ERROR_FAIL;
	return 0;
}
//-----------------------------------------------------------------------------
int ged_bridge_log_buf_write(
	struct GED_BRIDGE_IN_LOGBUFWRITE *psLogBufWriteIN,
	struct GED_BRIDGE_OUT_LOGBUFWRITE *psLogBufWriteOUT)
{
	psLogBufWriteOUT->eError =
		ged_log_buf_print2(psLogBufWriteIN->hLogBuf,
		psLogBufWriteIN->attrs, "%s", psLogBufWriteIN->acLogBuf);
	return 0;
}
//-----------------------------------------------------------------------------
int ged_bridge_log_buf_reset(
	struct GED_BRIDGE_IN_LOGBUFRESET *psLogBufResetIn,
	struct GED_BRIDGE_OUT_LOGBUFRESET *psLogBufResetOUT)
{
	psLogBufResetOUT->eError = ged_log_buf_reset(psLogBufResetIn->hLogBuf);
	return 0;
}
//-----------------------------------------------------------------------------
int ged_bridge_boost_gpu_freq(
	struct GED_BRIDGE_IN_BOOSTGPUFREQ *psBoostGpuFreqIN,
	struct GED_BRIDGE_OUT_BOOSTGPUFREQ *psBoostGpuFreqOUT)
{
	psBoostGpuFreqOUT->eError = (mtk_set_bottom_gpu_freq(
		psBoostGpuFreqIN->eGPUFreqLevel) == true) ?
		GED_OK : GED_ERROR_FAIL;
	return 0;
}
//-----------------------------------------------------------------------------
int ged_bridge_monitor_3D_fence(
	struct GED_BRIDGE_IN_MONITOR3DFENCE *psMonitor3DFenceINT,
	struct GED_BRIDGE_OUT_MONITOR3DFENCE *psMonitor3DFenceOUT)
{
	psMonitor3DFenceOUT->eError =
		ged_monitor_3D_fence_add(psMonitor3DFenceINT->fd);
	return 0;
}
//-----------------------------------------------------------------------------
int ged_bridge_query_info(
	struct GED_BRIDGE_IN_QUERY_INFO *psQueryInfoINT,
	struct GED_BRIDGE_OUT_QUERY_INFO *psQueryInfoOUT)
{
	psQueryInfoOUT->retrieve = ged_query_info(psQueryInfoINT->eType);
	return 0;
}
//-----------------------------------------------------------------------------
int ged_bridge_notify_vsync(
	struct GED_BRIDGE_IN_NOTIFY_VSYNC *psNotifyVsyncINT,
	struct GED_BRIDGE_OUT_NOTIFY_VSYNC *psNotifyVsyncOUT)
{
	psNotifyVsyncOUT->eError =
		ged_notify_sw_vsync(psNotifyVsyncINT->eType,
			&psNotifyVsyncOUT->sQueryData);

	return 0;
}
//-----------------------------------------------------------------------------
int ged_bridge_dvfs_probe(
	struct GED_BRIDGE_IN_DVFS_PROBE *psDVFSProbeINT,
	struct GED_BRIDGE_OUT_DVFS_PROBE *psDVFSProbeOUT)
{
	psDVFSProbeOUT->eError = ged_dvfs_probe(psDVFSProbeINT->pid);
	return 0;
}

//-----------------------------------------------------------------------------
int ged_bridge_dvfs_um_retrun(
	struct GED_BRIDGE_IN_DVFS_UM_RETURN *psDVFS_UM_returnINT,
	struct GED_BRIDGE_OUT_DVFS_UM_RETURN *psDVFS_UM_returnOUT)
{
	psDVFS_UM_returnOUT->eError =
		ged_dvfs_um_commit(psDVFS_UM_returnINT->gpu_tar_freq,
			psDVFS_UM_returnINT->bFallback);
	return 0;
}

//-----------------------------------------------------------------------------
int ged_bridge_event_notify(
	struct GED_BRIDGE_IN_EVENT_NOTIFY *psEVENT_NOTIFYINT,
	struct GED_BRIDGE_OUT_EVENT_NOTIFY *psEVENT_NOTIFYOUT)
{
	if (ged_boost_enable) {
		psEVENT_NOTIFYOUT->eError =
			ged_dvfs_vsync_offset_event_switch(
			psEVENT_NOTIFYINT->eEvent, psEVENT_NOTIFYINT->bSwitch);
	} else {
		psEVENT_NOTIFYOUT->eError = GED_OK;
	}

	return 0;
}

/* -------------------------------------------------------------------------- */
int ged_bridge_gpu_timestamp(
	struct GED_BRIDGE_IN_GPU_TIMESTAMP *psGpuBeginINT,
	struct GED_BRIDGE_OUT_GPU_TIMESTAMP *psGpuBeginOUT)
{
	if (ged_kpi_enabled() == 1) {
		if (psGpuBeginINT->QedBuffer_length == -2) {
			psGpuBeginOUT->eError =
				ged_kpi_dequeue_buffer_ts(psGpuBeginINT->pid,
					psGpuBeginINT->ullWnd,
					psGpuBeginINT->i32FrameID,
					psGpuBeginINT->fence_fd,
					psGpuBeginINT->isSF);
		} else if (psGpuBeginINT->QedBuffer_length != -1) {
			psGpuBeginOUT->eError =
				ged_kpi_queue_buffer_ts(psGpuBeginINT->pid,
					psGpuBeginINT->ullWnd,
					psGpuBeginINT->i32FrameID,
					psGpuBeginINT->fence_fd,
					psGpuBeginINT->QedBuffer_length);
		} else {
			psGpuBeginOUT->eError =
				ged_kpi_acquire_buffer_ts(psGpuBeginINT->pid,
					psGpuBeginINT->ullWnd,
					psGpuBeginINT->i32FrameID);
		}
		psGpuBeginOUT->is_ged_kpi_enabled = 1;
	} else {
		psGpuBeginOUT->is_ged_kpi_enabled = 0;
		psGpuBeginOUT->eError = GED_OK;
	}
	return 0;
}

/* -------------------------------------------------------------------------- */


//-----------------------------------------------------------------------------
int ged_bridge_gpu_hint_to_cpu(
		struct GED_BRIDGE_IN_GPU_HINT_TO_CPU *in,
		struct GED_BRIDGE_OUT_GPU_HINT_TO_CPU *out)
{
	int ret = 0;
	/* To-Do: FPSGo*/
	/*ret = fpsgo_notify_gpu_block(in->tid, in->i32BridgeFD, in->hint);*/

	out->eError = GED_OK;
	out->boost_flag = ret;
	out->boost_value = ged_dvfs_boost_value();
	return 0;
}

//-----------------------------------------------------------------------------
static int ged_force_mdp_enable;
int ged_bridge_hint_force_mdp(
	struct GED_BRIDGE_IN_HINT_FORCE_MDP *psHintForceMdpIn,
	struct GED_BRIDGE_OUT_HINT_FORCE_MDP *psHintForceMdpOut)
{
	/* Set flag */
	if (psHintForceMdpIn->hint != -1) {
		ged_force_mdp_enable = psHintForceMdpIn->hint;
		psHintForceMdpOut->eError =
			(ged_force_mdp_enable == psHintForceMdpIn->hint) ?
			GED_OK : GED_ERROR_FAIL;
	}
	/* Get flag */
	else {
		psHintForceMdpOut->mdp_flag = ged_force_mdp_enable;
		psHintForceMdpOut->eError =
			(ged_force_mdp_enable == psHintForceMdpOut->mdp_flag) ?
			GED_OK : GED_ERROR_FAIL;
	}

	return 0;
}
//-----------------------------------------------------------------------------

module_param(ged_boost_enable, uint, 0644);
module_param(ged_force_mdp_enable, int, 0644);
