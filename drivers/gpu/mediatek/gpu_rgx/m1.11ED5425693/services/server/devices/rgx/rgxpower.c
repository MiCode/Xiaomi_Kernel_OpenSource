/*************************************************************************/ /*!
@File
@Title          Device specific power routines
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Device specific functions
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */ /**************************************************************************/

#if defined(LINUX)
#include <linux/stddef.h>
#else
#include <stddef.h>
#endif

#include "rgxpower.h"
#include "rgxinit.h"
#include "rgx_fwif_km.h"
#include "rgxfwutils.h"
#include "pdump_km.h"
#include "pvr_debug.h"
#include "osfunc.h"
#include "rgxdebug.h"
#include "devicemem.h"
#include "devicemem_pdump.h"
#include "rgxtimecorr.h"
#include "devicemem_utils.h"
#include "htbserver.h"
#include "rgxstartstop.h"
#include "rgxfwimageutils.h"
#include "sync.h"

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#include "process_stats.h"
#endif
#if defined(PVR_DVFS)
#include "pvr_dvfs_device.h"
#endif

static PVRSRV_ERROR RGXFWNotifyHostTimeout(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	RGXFWIF_KCCB_CMD sCmd;
	PVRSRV_ERROR eError;

	/* Send the Timeout notification to the FW */
	sCmd.eCmdType = RGXFWIF_KCCB_CMD_POW;
	sCmd.uCmdData.sPowData.ePowType = RGXFWIF_POW_FORCED_IDLE_REQ;
	sCmd.uCmdData.sPowData.uPoweReqData.ePowRequestType = RGXFWIF_POWER_HOST_TIMEOUT;

	eError = RGXSendCommand(psDevInfo,
	                        RGXFWIF_DM_GP,
	                        &sCmd,
	                        PDUMP_FLAGS_NONE);

	return eError;
}

static void _RGXUpdateGPUUtilStats(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	RGXFWIF_GPU_UTIL_FWCB *psUtilFWCb;
	IMG_UINT64 *paui64StatsCounters;
	IMG_UINT64 ui64LastPeriod;
	IMG_UINT64 ui64LastState;
	IMG_UINT64 ui64LastTime;
	IMG_UINT64 ui64TimeNow;

#ifdef ENABLE_COMMON_DVFS
	unsigned long uLockFlags;
#endif

	psUtilFWCb = psDevInfo->psRGXFWIfGpuUtilFWCb;
	paui64StatsCounters = &psUtilFWCb->aui64StatsCounters[0];

#ifndef ENABLE_COMMON_DVFS
	OSLockAcquire(psDevInfo->hGPUUtilLock);
#else
	spin_lock_irqsave(&psDevInfo->sGPUUtilLock, uLockFlags);
#endif

	ui64TimeNow = RGXFWIF_GPU_UTIL_GET_TIME(RGXTimeCorrGetClockns64());

	/* Update counters to account for the time since the last update */
	ui64LastState  = RGXFWIF_GPU_UTIL_GET_STATE(psUtilFWCb->ui64LastWord);
	ui64LastTime   = RGXFWIF_GPU_UTIL_GET_TIME(psUtilFWCb->ui64LastWord);
	ui64LastPeriod = RGXFWIF_GPU_UTIL_GET_PERIOD(ui64TimeNow, ui64LastTime);
	paui64StatsCounters[ui64LastState] += ui64LastPeriod;

	/* Update state and time of the latest update */
	psUtilFWCb->ui64LastWord = RGXFWIF_GPU_UTIL_MAKE_WORD(ui64TimeNow, ui64LastState);

#ifndef ENABLE_COMMON_DVFS
	OSLockRelease(psDevInfo->hGPUUtilLock);
#else
	spin_unlock_irqrestore(&psDevInfo->sGPUUtilLock, uLockFlags);
#endif
}

static INLINE PVRSRV_ERROR RGXDoStop(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_ERROR eError;

#if defined(SUPPORT_TRUSTED_DEVICE) && !defined(NO_HARDWARE)
	PVRSRV_DEVICE_CONFIG *psDevConfig = psDeviceNode->psDevConfig;
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_GUEST, PVRSRV_OK);

	if (psDevConfig->pfnTDRGXStop == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXPrePowerState: TDRGXStop not implemented!"));
		return PVRSRV_ERROR_NOT_IMPLEMENTED;
	}

	eError = psDevConfig->pfnTDRGXStop(psDevConfig->hSysData);
#else
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_GUEST, PVRSRV_OK);

	eError = RGXStop(&psDevInfo->sLayerParams);
#endif

	return eError;
}

/*
	RGXPrePowerState
 */
PVRSRV_ERROR RGXPrePowerState(IMG_HANDLE				hDevHandle,
                              PVRSRV_DEV_POWER_STATE	eNewPowerState,
                              PVRSRV_DEV_POWER_STATE	eCurrentPowerState,
                              IMG_BOOL					bForced)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_GUEST, PVRSRV_OK);
	if ((eNewPowerState != eCurrentPowerState) &&
	    (eNewPowerState != PVRSRV_DEV_POWER_STATE_ON))
	{
		PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
		PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
		RGXFWIF_KCCB_CMD	sPowCmd;
		RGXFWIF_TRACEBUF	*psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;

		/* Send the Power off request to the FW */
		sPowCmd.eCmdType = RGXFWIF_KCCB_CMD_POW;
		sPowCmd.uCmdData.sPowData.ePowType = RGXFWIF_POW_OFF_REQ;
		sPowCmd.uCmdData.sPowData.uPoweReqData.bForced = bForced;

		eError = SyncPrimSet(psDevInfo->psPowSyncPrim, 0);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed to set Power sync prim",
					__func__));
			return eError;
		}

		eError = RGXSendCommand(psDevInfo,
		                        RGXFWIF_DM_GP,
		                        &sPowCmd,
		                        PDUMP_FLAGS_NONE);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed to send Power off request",
					__func__));
			return eError;
		}

		/* Wait for the firmware to complete processing. It cannot use PVRSRVWaitForValueKM as it relies
		   on the EventObject which is signalled in this MISR */
		eError = RGXPollForGPCommandCompletion(psDeviceNode,
								  psDevInfo->psPowSyncPrim->pui32LinAddr,
								  0x1, 0xFFFFFFFF);

		/* Check the Power state after the answer */
		if (eError == PVRSRV_OK)
		{
			/* Finally, de-initialise some registers. */
			if (psFWTraceBuf->ePowState == RGXFWIF_POW_OFF)
			{
#if !defined(NO_HARDWARE)
#if defined(RGX_FW_IRQ_OS_COUNTERS)
				IMG_UINT32 ui32idx = RGXFW_HYPERVISOR_OS;
#else
				IMG_UINT32 ui32idx;
				for_each_irq_cnt(ui32idx)
#endif /* RGX_FW_IRQ_OS_COUNTERS */
				{
					IMG_UINT32 ui32IrqCnt;

					get_irq_cnt_val(ui32IrqCnt, ui32idx, psDevInfo);

					/* Wait for the pending META/MIPS to host interrupts to come back. */
#ifdef MTK_POWER_OFF_TIMING
					eError = PVRSRVPollForGEValueKM(&g_ui32HostSampleIRQCount,
                                                psDevInfo->psRGXFWIfTraceBuf->ui32InterruptCount);
#else
					eError = PVRSRVPollForValueKM((IMG_UINT32 __iomem *)&psDevInfo->aui32SampleIRQCount[ui32idx],
					                              ui32IrqCnt,
					                              0xffffffff);
#endif

					if (eError != PVRSRV_OK)
					{
						PVR_DPF((PVR_DBG_ERROR,
								"%s: Wait for pending interrupts failed." MSG_IRQ_CNT_TYPE " %u Host: %u, FW: %u",
								__func__,
								ui32idx,
								psDevInfo->aui32SampleIRQCount[ui32idx],
								ui32IrqCnt));

						RGX_WaitForInterruptsTimeout(psDevInfo);
					}
				}
#endif /* NO_HARDWARE */

				/* Update GPU frequency and timer correlation related data */
				RGXTimeCorrEnd(psDeviceNode, RGXTIMECORR_EVENT_POWER);

				/* Update GPU state counters */
				_RGXUpdateGPUUtilStats(psDevInfo);

#if defined(PVR_DVFS)
				eError = SuspendDVFS();
				if (eError != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_ERROR,"%s: Failed to suspend DVFS", __func__));
					return eError;
				}
#endif

				psDevInfo->bRGXPowered = IMG_FALSE;

				eError = RGXDoStop(psDeviceNode);
				if (eError != PVRSRV_OK)
				{
					/* Power down failures are treated as successful since the power was removed but logged. */
					PVR_DPF((PVR_DBG_WARNING, "%s: RGXDoStop failed (%s)",
							__func__, PVRSRVGetErrorString(eError)));
					psDevInfo->ui32ActivePMReqNonIdle++;
					eError = PVRSRV_OK;
				}
			}
			else
			{
				/* the sync was updated but the pow state isn't off -> the FW denied the transition */
				eError = PVRSRV_ERROR_DEVICE_POWER_CHANGE_DENIED;

				if (bForced)
				{	/* It is an error for a forced request to be denied */
					PVR_DPF((PVR_DBG_ERROR,
							 "%s: Failure to power off during a forced power off. FW: %d",
							 __func__, psFWTraceBuf->ePowState));
				}
			}
		}
		else if (eError == PVRSRV_ERROR_TIMEOUT)
		{
			/* timeout waiting for the FW to ack the request: return timeout */
			PVR_DPF((PVR_DBG_WARNING,
					 "%s: Timeout waiting for powoff ack from the FW",
					 __func__));
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: Error waiting for powoff ack from the FW (%s)",
					 __func__, PVRSRVGetErrorString(eError)));
			eError = PVRSRV_ERROR_DEVICE_POWER_CHANGE_FAILURE;
		}
	}

	return eError;
}

#if defined(TRACK_FW_BOOT)
static INLINE void RGXGetFWBootStage(PVRSRV_RGXDEV_INFO *psDevInfo,
                                     FW_BOOT_STAGE *peStage,
                                     FW_BOOT_STAGE *peStageMax)
{
	*peStageMax = FW_BOOT_INIT_DONE;

	if (RGX_IS_FEATURE_VALUE_SUPPORTED(psDevInfo, META))
	{
		/* Boot stage temporarily stored to the register below */
		*peStage = OSReadHWReg32(psDevInfo->pvRegsBaseKM,
		                         RGX_CR_POWER_ESTIMATE_RESULT);
	}
	else
	{
		IMG_UINT32 *pui32BootStage;

		if (PVRSRV_OK != DevmemAcquireCpuVirtAddr(psDevInfo->psRGXFWDataMemDesc,
		                                          (void**)&pui32BootStage))
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Could not acquire pointer to FW boot stage", __func__));
			*peStage = FW_BOOT_STAGE_NOT_AVAILABLE;
			return;
		}

		pui32BootStage += RGXMIPSFW_GET_OFFSET_IN_DWORDS(RGXGetFWImageSectionOffset(NULL, MIPS_BOOT_DATA));
		pui32BootStage += RGXMIPSFW_GET_OFFSET_IN_DWORDS(RGXMIPSFW_BOOT_STAGE_OFFSET);

		*peStage = *(FW_BOOT_STAGE*)pui32BootStage;

		DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWDataMemDesc);
	}
}
#endif

static INLINE PVRSRV_ERROR RGXDoStart(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_ERROR eError;

#if defined(SUPPORT_TRUSTED_DEVICE) && !defined(NO_HARDWARE)
	PVRSRV_DEVICE_CONFIG *psDevConfig = psDeviceNode->psDevConfig;

	if (psDevConfig->pfnTDRGXStart == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXPostPowerState: TDRGXStart not implemented!"));
		return PVRSRV_ERROR_NOT_IMPLEMENTED;
	}

	eError = psDevConfig->pfnTDRGXStart(psDevConfig->hSysData);
#else
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

	eError = RGXStart(&psDevInfo->sLayerParams);
#endif

	return eError;
}


#if defined(NO_HARDWARE) && defined(PDUMP)

#if 0
#include "rgxtbdefs.h"
#else

/*
    Register RGX_TB_SYSTEM_STATUS
*/
#define RGX_TB_SYSTEM_STATUS                              (0x00E0U)
#define RGX_TB_SYSTEM_STATUS_MASKFULL                     (IMG_UINT64_C(0x00000000030100FF))
/*
directly indicates the status of power_abort flag from the power management controller (RGX_PRCM)
*/
#define RGX_TB_SYSTEM_STATUS_HOST_POWER_EVENT_ABORT_SHIFT (25U)
#define RGX_TB_SYSTEM_STATUS_HOST_POWER_EVENT_ABORT_CLRMSK (IMG_UINT64_C(0XFFFFFFFFFDFFFFFF))
#define RGX_TB_SYSTEM_STATUS_HOST_POWER_EVENT_ABORT_EN    (IMG_UINT64_C(0X0000000002000000))
/*
directly indicates the status of power_complete flag from the power management controller (RGX_PRCM)
*/
#define RGX_TB_SYSTEM_STATUS_HOST_POWER_EVENT_COMPLETE_SHIFT (24U)
#define RGX_TB_SYSTEM_STATUS_HOST_POWER_EVENT_COMPLETE_CLRMSK (IMG_UINT64_C(0XFFFFFFFFFEFFFFFF))
#define RGX_TB_SYSTEM_STATUS_HOST_POWER_EVENT_COMPLETE_EN (IMG_UINT64_C(0X0000000001000000))
/*
directly indicates the status of GPU's hmmu_irq
*/
#define RGX_TB_SYSTEM_STATUS_HMMU_IRQ_SHIFT               (16U)
#define RGX_TB_SYSTEM_STATUS_HMMU_IRQ_CLRMSK              (IMG_UINT64_C(0XFFFFFFFFFFFEFFFF))
#define RGX_TB_SYSTEM_STATUS_HMMU_IRQ_EN                  (IMG_UINT64_C(0X0000000000010000))
/*
directly indicates the status of GPU's irq per OS_ID
*/
#define RGX_TB_SYSTEM_STATUS_IRQ_SHIFT                    (1U)
#define RGX_TB_SYSTEM_STATUS_IRQ_CLRMSK                   (IMG_UINT64_C(0XFFFFFFFFFFFFFE01))
/*
old deprecated single irq
*/
#define RGX_TB_SYSTEM_STATUS_OLD_IRQ_SHIFT                    (0U)
#define RGX_TB_SYSTEM_STATUS_OLD_IRQ_CLRMSK                   (IMG_UINT64_C(0XFFFFFFFFFFFFFFFE))
#endif

#define RGX_FEATURE_NUM_OSIDS 8

static PVRSRV_ERROR
_ValidateIrqs(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	IMG_UINT32 ui32OSid;
	IMG_UINT32 ui32ConfigFlagsExt;
	PDUMP_FLAGS_T ui32PDumpFlags = PDUMP_FLAGS_CONTINUOUS;

	{
		PVRSRV_ERROR eError;
		RGXFWIF_OS_CONFIG *psOSConfig;

		eError = DevmemAcquireCpuVirtAddr(psDevInfo->psRGXFWIfOSConfigDesc,
							  (void **)&psOSConfig);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					"%s: Failed to acquire OS Config (%u)",
					__func__,
					eError));
			return eError;
		}

		ui32ConfigFlagsExt = psOSConfig->ui32ConfigFlagsExt;

		DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfOSConfigDesc);
	}

	/* Check if the Validation IRQ flag is set */
	if ((ui32ConfigFlagsExt & RGXFWIF_INICFG_EXT_VALIDATE_IRQ) == 0)
	{
		return PVRSRV_OK;
	}

	PDUMPIF("IMG_PVR_TESTBENCH", ui32PDumpFlags);
	PDUMPCOMMENTWITHFLAGS(ui32PDumpFlags, "Poll for TB irq status to be set (irqs signalled)...");
	PDUMPREGPOL(RGX_TB_PDUMPREG_NAME,
	            RGX_TB_SYSTEM_STATUS,
				~RGX_TB_SYSTEM_STATUS_IRQ_CLRMSK,
				~RGX_TB_SYSTEM_STATUS_IRQ_CLRMSK,
				ui32PDumpFlags,
				PDUMP_POLL_OPERATOR_EQUAL);

	PDUMPCOMMENTWITHFLAGS(ui32PDumpFlags, "... and then clear them");
	for (ui32OSid = 0; ui32OSid < RGX_FEATURE_NUM_OSIDS; ui32OSid++)
	{
		PDUMPREG32(RGX_PDUMPREG_NAME,
		           RGX_CR_IRQ_OS0_EVENT_CLEAR + ui32OSid * 0x10000,
		           RGX_CR_IRQ_OS0_EVENT_CLEAR_MASKFULL,
		           ui32PDumpFlags);
	}

	PDUMPFI("IMG_PVR_TESTBENCH", ui32PDumpFlags);

	/* Poll on all the interrupt status registers for all OSes */
	PDUMPCOMMENTWITHFLAGS(ui32PDumpFlags, "Validate Interrupt lines.");

	for (ui32OSid = 0; ui32OSid < RGX_FEATURE_NUM_OSIDS; ui32OSid++)
	{
		PDUMPREGPOL(RGX_PDUMPREG_NAME,
		            RGX_CR_IRQ_OS0_EVENT_STATUS + ui32OSid * 0x10000,
		            0x0,
		            0xFFFFFFFF,
		            ui32PDumpFlags,
		            PDUMP_POLL_OPERATOR_EQUAL);
	}

	return PVRSRV_OK;
}
#endif

/*
	RGXPostPowerState
 */
PVRSRV_ERROR RGXPostPowerState(IMG_HANDLE				hDevHandle,
                               PVRSRV_DEV_POWER_STATE	eNewPowerState,
                               PVRSRV_DEV_POWER_STATE	eCurrentPowerState,
                               IMG_BOOL					bForced)
{
	if ((eNewPowerState != eCurrentPowerState) &&
	    (eCurrentPowerState != PVRSRV_DEV_POWER_STATE_ON))
	{
		PVRSRV_ERROR		 eError;
		PVRSRV_DEVICE_NODE	 *psDeviceNode = hDevHandle;
		PVRSRV_RGXDEV_INFO	 *psDevInfo = psDeviceNode->pvDevice;
		RGXFWIF_INIT *psRGXFWInit;

		if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
		{
			psDevInfo->bRGXPowered = IMG_TRUE;
			return PVRSRV_OK;
		}

		if (eCurrentPowerState == PVRSRV_DEV_POWER_STATE_OFF)
		{
			/* Update timer correlation related data */
			RGXTimeCorrBegin(psDeviceNode, RGXTIMECORR_EVENT_POWER);

			/* Update GPU state counters */
			_RGXUpdateGPUUtilStats(psDevInfo);

			eError = RGXDoStart(psDeviceNode);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"RGXPostPowerState: RGXDoStart failed"));
				return eError;
			}

			OSMemoryBarrier();

			eError = DevmemAcquireCpuVirtAddr(psDevInfo->psRGXFWIfInitMemDesc,
			                                  (void **)&psRGXFWInit);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,
						"RGXPostPowerState: Failed to acquire kernel fw if ctl (%u)",
						eError));
				return eError;
			}

			/*
			 * Check whether the FW has started by polling on bFirmwareStarted flag
			 */
			if (PVRSRVPollForValueKM((IMG_UINT32 __iomem *)&psRGXFWInit->bFirmwareStarted,
			                         IMG_TRUE,
			                         0xFFFFFFFF) != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR, "RGXPostPowerState: Polling for 'FW started' flag failed."));
				eError = PVRSRV_ERROR_TIMEOUT;

#if defined(TRACK_FW_BOOT)
				{
					FW_BOOT_STAGE eStage, eStageMax;

					RGXGetFWBootStage(psDevInfo, &eStage, &eStageMax);

					PVR_LOG(("%s: FW reached boot stage %i/%i.",
					         __func__, eStage, eStageMax));
				}
#endif

				/*
				 * When bFirmwareStarted fails some info may be gained by doing the following
				 * debug dump but unfortunately it could be potentially dangerous if the reason
				 * for not booting is the GPU power is not ON. However, if we have reached this
				 * point the System Layer has returned without errors, we assume the GPU power
				 * is indeed ON.
				 */
				RGXDumpRGXDebugSummary(NULL, NULL, psDeviceNode->pvDevice, IMG_TRUE);
				RGXDumpRGXRegisters(NULL, NULL, psDeviceNode->pvDevice);

				DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfInitMemDesc);
				return eError;
			}

#if defined(PDUMP)
			PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "Wait for the Firmware to start.");
			eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfInitMemDesc,
			                                offsetof(RGXFWIF_INIT, bFirmwareStarted),
			                                IMG_TRUE,
			                                0xFFFFFFFFU,
			                                PDUMP_POLL_OPERATOR_EQUAL,
			                                PDUMP_FLAGS_CONTINUOUS);

			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,
						"RGXPostPowerState: problem pdumping POL for psRGXFWIfInitMemDesc (%d)",
						eError));
				DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfInitMemDesc);
				return eError;
			}

#if defined(NO_HARDWARE) && defined(PDUMP)
			eError = _ValidateIrqs(psDevInfo);
			if (eError != PVRSRV_OK)
			{
				return eError;
			}
#endif
#endif

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
			SetFirmwareStartTime(psRGXFWInit->ui32FirmwareStartedTimeStamp);
#endif

			HTBSyncPartitionMarker(psRGXFWInit->ui32MarkerVal);

			DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfInitMemDesc);

			psDevInfo->bRGXPowered = IMG_TRUE;

#if defined(PVR_DVFS)
			eError = ResumeDVFS();
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"RGXPostPowerState: Failed to resume DVFS"));
				return eError;
			}
#endif
		}
	}

	PDUMPCOMMENT("RGXPostPowerState: Current state: %d, New state: %d", eCurrentPowerState, eNewPowerState);

	return PVRSRV_OK;
}

/*
	RGXPreClockSpeedChange
 */
PVRSRV_ERROR RGXPreClockSpeedChange(IMG_HANDLE				hDevHandle,
                                    PVRSRV_DEV_POWER_STATE	eCurrentPowerState)
{
	PVRSRV_ERROR		eError = PVRSRV_OK;
	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	RGX_DATA			*psRGXData = (RGX_DATA*)psDeviceNode->psDevConfig->hDevData;
	RGXFWIF_TRACEBUF	*psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_GUEST, PVRSRV_OK);
	PVR_UNREFERENCED_PARAMETER(psRGXData);

	PVR_DPF((PVR_DBG_MESSAGE,"RGXPreClockSpeedChange: RGX clock speed was %uHz",
			psRGXData->psRGXTimingInfo->ui32CoreClockSpeed));

	if ((eCurrentPowerState != PVRSRV_DEV_POWER_STATE_OFF) &&
	    (psFWTraceBuf->ePowState != RGXFWIF_POW_OFF))
	{
		/* Update GPU frequency and timer correlation related data */
		RGXTimeCorrEnd(psDeviceNode, RGXTIMECORR_EVENT_DVFS);
	}

	return eError;
}

/*
	RGXPostClockSpeedChange
 */
PVRSRV_ERROR RGXPostClockSpeedChange(IMG_HANDLE				hDevHandle,
                                     PVRSRV_DEV_POWER_STATE	eCurrentPowerState)
{
	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	RGX_DATA			*psRGXData = (RGX_DATA*)psDeviceNode->psDevConfig->hDevData;
	PVRSRV_ERROR		eError = PVRSRV_OK;
	RGXFWIF_TRACEBUF	*psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;
	IMG_UINT32 		ui32NewClockSpeed = psRGXData->psRGXTimingInfo->ui32CoreClockSpeed;
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_GUEST, PVRSRV_OK);

	/* Update runtime configuration with the new value */
	psDevInfo->psRGXFWIfRuntimeCfg->ui32CoreClockSpeed = ui32NewClockSpeed;

	if ((eCurrentPowerState != PVRSRV_DEV_POWER_STATE_OFF) &&
	    (psFWTraceBuf->ePowState != RGXFWIF_POW_OFF))
	{
		RGXFWIF_KCCB_CMD sCOREClkSpeedChangeCmd;

		RGXTimeCorrBegin(psDeviceNode, RGXTIMECORR_EVENT_DVFS);

		sCOREClkSpeedChangeCmd.eCmdType = RGXFWIF_KCCB_CMD_CORECLKSPEEDCHANGE;
		sCOREClkSpeedChangeCmd.uCmdData.sCoreClkSpeedChangeData.ui32NewClockSpeed = ui32NewClockSpeed;

		/* Ensure the new clock speed is written to memory before requesting the FW to read it */
		OSMemoryBarrier();

		PDUMPCOMMENT("Scheduling CORE clock speed change command");

		PDUMPPOWCMDSTART();
		eError = RGXSendCommand(psDeviceNode->pvDevice,
		                        RGXFWIF_DM_GP,
		                        &sCOREClkSpeedChangeCmd,
		                        PDUMP_FLAGS_NONE);
		PDUMPPOWCMDEND();

		if (eError != PVRSRV_OK)
		{
			PDUMPCOMMENT("Scheduling CORE clock speed change command failed");
			PVR_DPF((PVR_DBG_ERROR, "RGXPostClockSpeedChange: Scheduling KCCB command failed. Error:%u", eError));
			return eError;
		}

		PVR_DPF((PVR_DBG_MESSAGE,"RGXPostClockSpeedChange: RGX clock speed changed to %uHz",
				psRGXData->psRGXTimingInfo->ui32CoreClockSpeed));
	}

	return eError;
}

/*!
 ******************************************************************************

 @Function	RGXDustCountChange

 @Description

	Does change of number of DUSTs

 @Input	   hDevHandle : RGX Device Node
 @Input	   ui32NumberOfDusts : Number of DUSTs to make transition to

 @Return   PVRSRV_ERROR :

 ******************************************************************************/
PVRSRV_ERROR RGXDustCountChange(IMG_HANDLE		hDevHandle,
                                IMG_UINT32		ui32NumberOfDusts)
{

	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_ERROR		eError;
	RGXFWIF_KCCB_CMD 	sDustCountChange;
	IMG_UINT32			ui32MaxAvailableDusts = psDevInfo->sDevFeatureCfg.ui32MAXDustCount;
	RGXFWIF_RUNTIME_CFG *psRuntimeCfg = psDevInfo->psRGXFWIfRuntimeCfg;
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_GUEST, PVRSRV_OK);

	if (ui32NumberOfDusts > ui32MaxAvailableDusts)
	{
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		PVR_DPF((PVR_DBG_ERROR,
				"%s: Invalid number of DUSTs (%u) while expecting value within <0,%u>. Error:%u",
				__func__,
				ui32NumberOfDusts,
				ui32MaxAvailableDusts,
				eError));
		return eError;
	}

#if defined(FIX_HW_BRN_59042)
	if (ui32NumberOfDusts < ui32MaxAvailableDusts && (ui32NumberOfDusts & 0x1))
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: Invalid number of DUSTs (%u) due to HW restriction. Allowed values are :-",
				__func__,
				ui32NumberOfDusts));
		switch (ui32MaxAvailableDusts)
		{
			case 2:	PVR_DPF((PVR_DBG_ERROR, "0, 2")); break;
			case 3:	PVR_DPF((PVR_DBG_ERROR, "0, 2, 3")); break;
			case 4:	PVR_DPF((PVR_DBG_ERROR, "0, 2, 4")); break;
			case 5:	PVR_DPF((PVR_DBG_ERROR, "0, 2, 4, 5")); break;
			case 6:	PVR_DPF((PVR_DBG_ERROR, "0, 2, 4, 6")); break;
			default: break;
		}
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
#endif

	psRuntimeCfg->ui32DefaultDustsNumInit = ui32NumberOfDusts;

#if !defined(NO_HARDWARE)
	{
		RGXFWIF_TRACEBUF *psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;

		if (psFWTraceBuf->ePowState == RGXFWIF_POW_OFF)
		{
			return PVRSRV_OK;
		}

		if (psFWTraceBuf->ePowState != RGXFWIF_POW_FORCED_IDLE)
		{
			eError = PVRSRV_ERROR_DEVICE_POWER_CHANGE_DENIED;
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: Attempt to change dust count when not IDLE",
					 __func__));
			return eError;
		}
	}
#endif

	eError = SyncPrimSet(psDevInfo->psPowSyncPrim, 0);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to set Power sync prim",
				__func__));
		return eError;
	}

	sDustCountChange.eCmdType = RGXFWIF_KCCB_CMD_POW;
	sDustCountChange.uCmdData.sPowData.ePowType = RGXFWIF_POW_NUMDUST_CHANGE;
	sDustCountChange.uCmdData.sPowData.uPoweReqData.ui32NumOfDusts = ui32NumberOfDusts;

	PDUMPCOMMENT("Scheduling command to change Dust Count to %u", ui32NumberOfDusts);
	eError = RGXSendCommand(psDeviceNode->pvDevice,
	                        RGXFWIF_DM_GP,
	                        &sDustCountChange,
	                        PDUMP_FLAGS_NONE);

	if (eError != PVRSRV_OK)
	{
		PDUMPCOMMENT("Scheduling command to change Dust Count failed. Error:%u", eError);
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Scheduling KCCB to change Dust Count failed. Error:%u",
				 __func__, eError));
		return eError;
	}

	/* Wait for the firmware to answer. */
	eError = RGXPollForGPCommandCompletion(psDeviceNode,
	                              psDevInfo->psPowSyncPrim->pui32LinAddr,
								  0x1, 0xFFFFFFFF);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"%s: Timeout waiting for idle request", __func__));
		return eError;
	}

#if defined(PDUMP)
	PDUMPCOMMENT("RGXDustCountChange: Poll for Kernel SyncPrim [0x%p] on DM %d", psDevInfo->psPowSyncPrim->pui32LinAddr, RGXFWIF_DM_GP);

	SyncPrimPDumpPol(psDevInfo->psPowSyncPrim,
	                 1,
	                 0xffffffff,
	                 PDUMP_POLL_OPERATOR_EQUAL,
	                 0);
#endif

	return PVRSRV_OK;
}

/*
 @Function	RGXAPMLatencyChange
 */
PVRSRV_ERROR RGXAPMLatencyChange(IMG_HANDLE		hDevHandle,
                                 IMG_UINT32		ui32ActivePMLatencyms,
                                 IMG_BOOL		bActivePMLatencyPersistant)
{

	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_ERROR		eError;
	RGXFWIF_RUNTIME_CFG	*psRuntimeCfg = psDevInfo->psRGXFWIfRuntimeCfg;
	PVRSRV_DEV_POWER_STATE	ePowerState;
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_GUEST, PVRSRV_OK);

	eError = PVRSRVPowerLock(psDeviceNode);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXAPMLatencyChange: Failed to acquire power lock"));
		return eError;
	}

	/* Update runtime configuration with the new values and ensure the
	 * new APM latency is written to memory before requesting the FW to
	 * read it
	 */
	psRuntimeCfg->ui32ActivePMLatencyms = ui32ActivePMLatencyms;
	psRuntimeCfg->bActivePMLatencyPersistant = bActivePMLatencyPersistant;
	OSMemoryBarrier();

	eError = PVRSRVGetDevicePowerState(psDeviceNode, &ePowerState);

	if ((eError == PVRSRV_OK) && (ePowerState != PVRSRV_DEV_POWER_STATE_OFF))
	{
		RGXFWIF_KCCB_CMD	sActivePMLatencyChange;
		sActivePMLatencyChange.eCmdType = RGXFWIF_KCCB_CMD_POW;
		sActivePMLatencyChange.uCmdData.sPowData.ePowType = RGXFWIF_POW_APM_LATENCY_CHANGE;
		sActivePMLatencyChange.uCmdData.sPowData.uPoweReqData.ui32ActivePMLatencyms = ui32ActivePMLatencyms;

		PDUMPCOMMENT("Scheduling command to change APM latency to %u", ui32ActivePMLatencyms);
		eError = RGXSendCommand(psDeviceNode->pvDevice,
		                        RGXFWIF_DM_GP,
		                        &sActivePMLatencyChange,
		                        PDUMP_FLAGS_NONE);

		if (eError != PVRSRV_OK)
		{
			PDUMPCOMMENT("Scheduling command to change APM latency failed. Error:%u", eError);
			PVR_DPF((PVR_DBG_ERROR, "RGXAPMLatencyChange: Scheduling KCCB to change APM latency failed. Error:%u", eError));
			goto ErrorExit;
		}
	}

	ErrorExit:
	PVRSRVPowerUnlock(psDeviceNode);

	return eError;
}

/*
	RGXActivePowerRequest
 */
PVRSRV_ERROR RGXActivePowerRequest(IMG_HANDLE hDevHandle)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	RGXFWIF_TRACEBUF *psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_GUEST, PVRSRV_OK);

#if defined(PVRSRV_USE_BRIDGE_LOCK)
	OSAcquireBridgeLock();
	/* NOTE: If this function were to wait for event object attempt should be
	   made to prevent releasing bridge lock during sleep. Bridge lock should
	   be held during sleep. */
#endif

	psDevInfo->ui32ActivePMReqTotal++;

	/* Powerlock to avoid further requests from racing with the FW hand-shake
	 * from now on (previous kicks to this point are detected by the FW)
	 * PVRSRVPowerLock is replaced with PVRSRVPowerTryLock to avoid
	 * potential dead lock between PDumpWriteLock and PowerLock
	 * during 'DriverLive + PDUMP=1 + EnableAPM=1'.
	 */
	eError = PVRSRVPowerTryLock(psDeviceNode);
	if (eError != PVRSRV_OK)
	{
		if (eError != PVRSRV_ERROR_RETRY)
		{
			PVR_LOG_ERROR(eError, "PVRSRVPowerTryLock");
		}
		else
		{
			psDevInfo->ui32ActivePMReqRetry++;
		}
		goto _RGXActivePowerRequest_PowerLock_failed;
	}

	/* Check again for IDLE once we have the power lock */
	if (psFWTraceBuf->ePowState == RGXFWIF_POW_IDLE)
	{
#if defined(PVRSRV_ENABLE_PROCESS_STATS)
		SetFirmwareHandshakeIdleTime(RGXReadHWTimerReg(psDevInfo)-psFWTraceBuf->ui64StartIdleTime);
#endif

		PDUMPPOWCMDSTART();
		eError = PVRSRVSetDevicePowerStateKM(psDeviceNode,
		                                     PVRSRV_DEV_POWER_STATE_OFF,
		                                     IMG_FALSE); /* forced */
		PDUMPPOWCMDEND();

		if (eError == PVRSRV_OK)
		{
			psDevInfo->ui32ActivePMReqOk++;
		}
		else if (eError == PVRSRV_ERROR_DEVICE_POWER_CHANGE_DENIED)
		{
			psDevInfo->ui32ActivePMReqDenied++;
		}
	}
	else
	{
		psDevInfo->ui32ActivePMReqNonIdle++;
	}

	PVRSRVPowerUnlock(psDeviceNode);

_RGXActivePowerRequest_PowerLock_failed:
#if defined(PVRSRV_USE_BRIDGE_LOCK)
	OSReleaseBridgeLock();
#endif

	return eError;

}
/*
	RGXForcedIdleRequest
 */

#define RGX_FORCED_IDLE_RETRY_COUNT 10

PVRSRV_ERROR RGXForcedIdleRequest(IMG_HANDLE hDevHandle, IMG_BOOL bDeviceOffPermitted)
{
	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	RGXFWIF_KCCB_CMD	sPowCmd;
	PVRSRV_ERROR		eError;
	IMG_UINT32			ui32RetryCount = 0;
#if !defined(NO_HARDWARE)
	RGXFWIF_TRACEBUF	*psFWTraceBuf;
#endif
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_GUEST, PVRSRV_OK);

#if !defined(NO_HARDWARE)
	psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;

	/* Firmware already forced idle */
	if (psFWTraceBuf->ePowState == RGXFWIF_POW_FORCED_IDLE)
	{
		return PVRSRV_OK;
	}

	/* Firmware is not powered. Sometimes this is permitted, for instance we were forcing idle to power down. */
	if (psFWTraceBuf->ePowState == RGXFWIF_POW_OFF)
	{
		return (bDeviceOffPermitted) ? PVRSRV_OK : PVRSRV_ERROR_DEVICE_IDLE_REQUEST_DENIED;
	}
#endif

	eError = SyncPrimSet(psDevInfo->psPowSyncPrim, 0);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to set Power sync prim",
				__func__));
		return eError;
	}
	sPowCmd.eCmdType = RGXFWIF_KCCB_CMD_POW;
	sPowCmd.uCmdData.sPowData.ePowType = RGXFWIF_POW_FORCED_IDLE_REQ;
	sPowCmd.uCmdData.sPowData.uPoweReqData.ePowRequestType = RGXFWIF_POWER_FORCE_IDLE;

	PDUMPCOMMENT("RGXForcedIdleRequest: Sending forced idle command");

	/* Send one forced IDLE command to GP */
	eError = RGXSendCommand(psDevInfo,
	                        RGXFWIF_DM_GP,
	                        &sPowCmd,
	                        PDUMP_FLAGS_NONE);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"%s: Failed to send idle request", __func__));
		return eError;
	}

	/* Wait for GPU to finish current workload */
	do {
		eError = RGXPollForGPCommandCompletion(psDeviceNode,
		                              psDevInfo->psPowSyncPrim->pui32LinAddr,
									  0x1, 0xFFFFFFFF);
		if ((eError == PVRSRV_OK) || (ui32RetryCount == RGX_FORCED_IDLE_RETRY_COUNT))
		{
			break;
		}
		ui32RetryCount++;
		PVR_DPF((PVR_DBG_WARNING,
				"%s: Request timeout. Retry %d of %d",
				 __func__, ui32RetryCount, RGX_FORCED_IDLE_RETRY_COUNT));
	} while (IMG_TRUE);

	if (eError != PVRSRV_OK)
	{
		RGXFWNotifyHostTimeout(psDevInfo);
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Idle request failed. Firmware potentially left in forced idle state",
				 __func__));
		return eError;
	}

#if defined(PDUMP)
	PDUMPCOMMENT("RGXForcedIdleRequest: Poll for Kernel SyncPrim [0x%p] on DM %d", psDevInfo->psPowSyncPrim->pui32LinAddr, RGXFWIF_DM_GP);

	SyncPrimPDumpPol(psDevInfo->psPowSyncPrim,
	                 1,
	                 0xffffffff,
	                 PDUMP_POLL_OPERATOR_EQUAL,
	                 0);
#endif

#if !defined(NO_HARDWARE)
	/* Check the firmware state for idleness */
	if (psFWTraceBuf->ePowState != RGXFWIF_POW_FORCED_IDLE)
	{
		return PVRSRV_ERROR_DEVICE_IDLE_REQUEST_DENIED;
	}
#endif

	return PVRSRV_OK;
}

/*
	RGXCancelForcedIdleRequest
 */
PVRSRV_ERROR RGXCancelForcedIdleRequest(IMG_HANDLE hDevHandle)
{
	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	RGXFWIF_KCCB_CMD	sPowCmd;
	PVRSRV_ERROR		eError = PVRSRV_OK;
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_GUEST, PVRSRV_OK);

	eError = SyncPrimSet(psDevInfo->psPowSyncPrim, 0);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to set Power sync prim",
				__func__));
		goto ErrorExit;
	}

	/* Send the IDLE request to the FW */
	sPowCmd.eCmdType = RGXFWIF_KCCB_CMD_POW;
	sPowCmd.uCmdData.sPowData.ePowType = RGXFWIF_POW_FORCED_IDLE_REQ;
	sPowCmd.uCmdData.sPowData.uPoweReqData.ePowRequestType = RGXFWIF_POWER_CANCEL_FORCED_IDLE;

	PDUMPCOMMENT("RGXForcedIdleRequest: Sending cancel forced idle command");

	/* Send cancel forced IDLE command to GP */
	eError = RGXSendCommand(psDevInfo,
	                        RGXFWIF_DM_GP,
	                        &sPowCmd,
	                        PDUMP_FLAGS_NONE);

	if (eError != PVRSRV_OK)
	{
		PDUMPCOMMENT("RGXCancelForcedIdleRequest: Failed to send cancel IDLE request for DM%d", RGXFWIF_DM_GP);
		goto ErrorExit;
	}

	/* Wait for the firmware to answer. */
	eError = RGXPollForGPCommandCompletion(psDeviceNode,
	                              psDevInfo->psPowSyncPrim->pui32LinAddr,
								  1, 0xFFFFFFFF);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"%s: Timeout waiting for cancel idle request", __func__));
		goto ErrorExit;
	}

#if defined(PDUMP)
	PDUMPCOMMENT("RGXCancelForcedIdleRequest: Poll for Kernel SyncPrim [0x%p] on DM %d", psDevInfo->psPowSyncPrim->pui32LinAddr, RGXFWIF_DM_GP);

	SyncPrimPDumpPol(psDevInfo->psPowSyncPrim,
	                 1,
	                 0xffffffff,
	                 PDUMP_POLL_OPERATOR_EQUAL,
	                 0);
#endif

	return eError;

ErrorExit:
	PVR_DPF((PVR_DBG_ERROR,"%s: Firmware potentially left in forced idle state", __func__));
	return eError;
}

/*!
 ******************************************************************************

 @Function	PVRSRVGetNextDustCount

 @Description

	Calculate a sequence of dust counts to achieve full transition coverage.
	We increment two counts of dusts and switch up and down between them.
	It does	contain a few redundant transitions. If two dust exist, the
	output transitions should be as follows.

	0->1, 0<-1, 0->2, 0<-2, (0->1)
	1->1, 1->2, 1<-2, (1->2)
	2->2, (2->0),
	0->0. Repeat.

	Redundant transitions in brackets.

 @Input		psDustReqState : Counter state used to calculate next dust count
 @Input		ui32DustCount : Number of dusts in the core

 @Return	PVRSRV_ERROR

 ******************************************************************************/
IMG_UINT32 RGXGetNextDustCount(RGX_DUST_STATE *psDustReqState, IMG_UINT32 ui32DustCount)
{
	if (psDustReqState->bToggle)
	{
		psDustReqState->ui32DustCount2++;
	}

	if (psDustReqState->ui32DustCount2 > ui32DustCount)
	{
		psDustReqState->ui32DustCount1++;
		psDustReqState->ui32DustCount2 = psDustReqState->ui32DustCount1;
	}

	if (psDustReqState->ui32DustCount1 > ui32DustCount)
	{
		psDustReqState->ui32DustCount1 = 0;
		psDustReqState->ui32DustCount2 = 0;
	}

	psDustReqState->bToggle = !psDustReqState->bToggle;

	return (psDustReqState->bToggle) ? psDustReqState->ui32DustCount1 : psDustReqState->ui32DustCount2;
}

/******************************************************************************
 End of file (rgxpower.c)
 ******************************************************************************/
