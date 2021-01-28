/*************************************************************************/ /*!
@File
@Title          Device specific initialisation routines
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

#include "img_defs.h"
#include "pvr_notifier.h"
#include "pvrsrv.h"
#include "pvrsrv_bridge_init.h"
#include "syscommon.h"
#include "rgx_heaps.h"
#include "rgxheapconfig.h"
#include "rgxdefs_km.h"
#include "rgxpower.h"
#include "tlstream.h"
#include "pvrsrv_tlstreams.h"

#include "rgxinit.h"
#include "rgxbvnc.h"

#include "pdump_km.h"
#include "handle.h"
#include "allocmem.h"
#include "devicemem.h"
#include "devicemem_pdump.h"
#include "rgxmem.h"
#include "sync_internal.h"
#include "pvrsrv_apphint.h"
#include "oskm_apphint.h"
#include "rgxfwdbg.h"
#include "info_page.h"

#include "rgxutils.h"
#include "rgxfwutils.h"
#include "rgx_fwif_km.h"

#include "rgxmmuinit.h"
#include "devicemem_utils.h"
#include "devicemem_server.h"
#include "physmem_osmem.h"

#include "rgxdebug.h"
#include "rgxhwperf.h"
#include "htbserver.h"

#include "rgx_options.h"
#include "pvrversion.h"

#include "rgx_compat_bvnc.h"

#include "rgx_heaps.h"

#include "rgxta3d.h"
#include "rgxtimecorr.h"

#if defined(PDUMP)
#include "rgxstartstop.h"
#endif

#include "rgx_fwif_alignchecks.h"

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
#include "rgxworkest.h"
#endif
#if defined(SUPPORT_PDVFS)
#include "rgxpdvfs.h"
#endif

#if defined(PVR_SUPPORT_HMMU_VALIDATION)
#include "hmmu_interface.h"
#endif

#if defined(SUPPORT_VALIDATION) && defined(SUPPORT_SOC_TIMER) && defined(PDUMP) && defined(NO_HARDWARE)
#include "validation_soc.h"
#endif

static PVRSRV_ERROR RGXDevInitCompatCheck(PVRSRV_DEVICE_NODE *psDeviceNode);
static PVRSRV_ERROR RGXDevVersionString(PVRSRV_DEVICE_NODE *psDeviceNode, IMG_CHAR **ppszVersionString);
static PVRSRV_ERROR RGXDevClockSpeed(PVRSRV_DEVICE_NODE *psDeviceNode, IMG_PUINT32 pui32RGXClockSpeed);
static PVRSRV_ERROR RGXSoftReset(PVRSRV_DEVICE_NODE *psDeviceNode, IMG_UINT64 ui64ResetValue, IMG_UINT64 ui64SPUResetValue);

#if (RGX_NUM_OS_SUPPORTED > 1)
static PVRSRV_ERROR RGXInitGuestFwHeap(DEVMEM_HEAP_BLUEPRINT *psDevMemHeap, IMG_UINT32 ui32OSid);
static void RGXDeInitGuestFwHeap(DEVMEM_HEAP_BLUEPRINT *psDevMemHeap);
#endif

#define RGX_MMU_LOG2_PAGE_SIZE_4KB   (12)
#define RGX_MMU_LOG2_PAGE_SIZE_16KB  (14)
#define RGX_MMU_LOG2_PAGE_SIZE_64KB  (16)
#define RGX_MMU_LOG2_PAGE_SIZE_256KB (18)
#define RGX_MMU_LOG2_PAGE_SIZE_1MB   (20)
#define RGX_MMU_LOG2_PAGE_SIZE_2MB   (21)

#define RGX_MMU_PAGE_SIZE_4KB   (   4 * 1024)
#define RGX_MMU_PAGE_SIZE_16KB  (  16 * 1024)
#define RGX_MMU_PAGE_SIZE_64KB  (  64 * 1024)
#define RGX_MMU_PAGE_SIZE_256KB ( 256 * 1024)
#define RGX_MMU_PAGE_SIZE_1MB   (1024 * 1024)
#define RGX_MMU_PAGE_SIZE_2MB   (2048 * 1024)
#define RGX_MMU_PAGE_SIZE_MIN RGX_MMU_PAGE_SIZE_4KB
#define RGX_MMU_PAGE_SIZE_MAX RGX_MMU_PAGE_SIZE_2MB

#define VAR(x) #x

#define MAX_BVNC_LEN (12)
#define RGXBVNC_BUFFER_SIZE (((PVRSRV_MAX_DEVICES)*(MAX_BVNC_LEN))+1)

static void RGXDeInitHeaps(DEVICE_MEMORY_INFO *psDevMemoryInfo);

#if defined(PVRSRV_DEBUG_LISR_EXECUTION)

/* bits used by the LISR to provide a trace of its last execution */
#define RGX_LISR_DEVICE_NOT_POWERED	(1 << 0)
#define RGX_LISR_FWIF_POW_OFF		(1 << 1)
#define RGX_LISR_EVENT_EN		(1 << 2)
#define RGX_LISR_COUNTS_EQUAL		(1 << 3)
#define RGX_LISR_PROCESSED		(1 << 4)

typedef struct _LISR_EXECUTION_INFO_
{
	/* bit mask showing execution flow of last LISR invocation */
	IMG_UINT32 ui32State;
	/* snapshot from the last LISR invocation, regardless of
	 * whether an interrupt was handled
	 */
	IMG_UINT32 aui32InterruptCountSnapshot[RGXFW_THREAD_NUM];
	/* time of the last LISR invocation */
	IMG_UINT64 ui64Clockns;
} LISR_EXECUTION_INFO;

/* information about the last execution of the LISR */
static LISR_EXECUTION_INFO g_sLISRExecutionInfo;

#endif

#if !defined(NO_HARDWARE)
/*************************************************************************/ /*!
@Function       SampleIRQCount
@Description    Utility function taking snapshots of RGX FW interrupt count.
@Input          paui32Input  A pointer to RGX FW IRQ count array.
                             Size of the array should be equal to RGX FW thread
                             count.
@Input          paui32Output A pointer to array containing sampled RGX FW
                             IRQ counts
@Return         IMG_BOOL     Returns IMG_TRUE, if RGX FW IRQ is not equal to
                             sampled RGX FW IRQ count for any RGX FW thread.
*/ /**************************************************************************/
static INLINE IMG_BOOL SampleIRQCount(volatile IMG_UINT32 *paui32Input,
									  volatile IMG_UINT32 *paui32Output)
{
	IMG_UINT32 ui32TID;
	IMG_BOOL bReturnVal = IMG_FALSE;

	for (ui32TID = 0; ui32TID < RGXFW_THREAD_NUM; ui32TID++)
	{
		if (paui32Output[ui32TID] != paui32Input[ui32TID])
		{
			/**
			 * we are handling any unhandled interrupts here so align the host
			 * count with the FW count
			 */

			/* Sample the current count from the FW _after_ we've cleared the interrupt. */
			paui32Output[ui32TID] = paui32Input[ui32TID];
			bReturnVal = IMG_TRUE;
		}
	}

	return bReturnVal;
}

static IMG_BOOL _WaitForInterruptsTimeoutCheck(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	RGXFWIF_SYSDATA *psFwSysData = psDevInfo->psRGXFWIfFwSysData;
	IMG_BOOL bScheduleMISR = IMG_FALSE;
#if defined(PVRSRV_DEBUG_LISR_EXECUTION)
	IMG_UINT32 ui32TID;
#endif

	RGXDEBUG_PRINT_IRQ_COUNT(psDevInfo);

#if defined(PVRSRV_DEBUG_LISR_EXECUTION)
	PVR_DPF((PVR_DBG_ERROR, "Last RGX_LISRHandler State: 0x%08X Clock: %llu",
							g_sLISRExecutionInfo.ui32State,
							g_sLISRExecutionInfo.ui64Clockns));

	for (ui32TID = 0; ui32TID < RGXFW_THREAD_NUM; ui32TID++)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"RGX FW thread %u: InterruptCountSnapshot: 0x%X",
				ui32TID, g_sLISRExecutionInfo.aui32InterruptCountSnapshot[ui32TID]));
	}
#else
	PVR_DPF((PVR_DBG_ERROR, "No further information available. Please enable PVRSRV_DEBUG_LISR_EXECUTION"));
#endif


	if (psFwSysData->ePowState != RGXFWIF_POW_OFF)
	{
		PVR_DPF((PVR_DBG_ERROR, "_WaitForInterruptsTimeout: FW pow state is not OFF (is %u)",
				(unsigned int) psFwSysData->ePowState));
	}

	bScheduleMISR = SampleIRQCount(psFwSysData->aui32InterruptCount,
								   psDevInfo->aui32SampleIRQCount);
	return bScheduleMISR;
}

void RGX_WaitForInterruptsTimeout(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	IMG_BOOL bScheduleMISR;

	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		bScheduleMISR = IMG_TRUE;
	}
	else
	{
		bScheduleMISR = _WaitForInterruptsTimeoutCheck(psDevInfo);
	}

	if (bScheduleMISR)
	{
		OSScheduleMISR(psDevInfo->pvMISRData);

		if (psDevInfo->pvAPMISRData != NULL)
		{
			OSScheduleMISR(psDevInfo->pvAPMISRData);
		}
	}
}

/*
	RGX LISR Handler
*/
static IMG_BOOL RGX_LISRHandler (void *pvData)
{
	PVRSRV_DEVICE_NODE *psDeviceNode = pvData;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	IMG_BOOL bInterruptProcessed;
	RGXFWIF_SYSDATA *psFwSysData;
	IMG_UINT32 ui32IRQStatus;
	IMG_UINT32 ui32IRQThreadMask;

#if defined(PVR_SUPPORT_HMMU_VALIDATION)
	/*
	 * Trigger an HMMU LISR handler even if this interrupt might not be due to
	 * the HMMU. A spurious interrupt will have no negative effect other than
	 * wasted time and since we are running in HMMU validation mode this is not
	 * a big issue.
	 */
	PVR_HMMUHandleLISR(psDevInfo);

	if (psDevInfo->pvHMMUMISRData != NULL)
	{
		OSScheduleMISR(psDevInfo->pvHMMUMISRData);
	}
#endif

	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		if (! psDevInfo->bRGXPowered)
		{
			return IMG_FALSE;
		}

		/* guest OS should also clear interrupt register */
		ui32IRQStatus = OSReadHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_IRQ_OS0_EVENT_STATUS);
		ui32IRQThreadMask = (ui32IRQStatus & ~RGX_CR_IRQ_OS0_EVENT_STATUS_SOURCE_CLRMSK);
		OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_IRQ_OS0_EVENT_CLEAR, ui32IRQThreadMask);
		OSScheduleMISR(psDevInfo->pvMISRData);
		return IMG_TRUE;
	}
	else
	{
		bInterruptProcessed = IMG_FALSE;
		psFwSysData = psDevInfo->psRGXFWIfFwSysData;
	}

#if defined(PVRSRV_DEBUG_LISR_EXECUTION)
	{
		IMG_UINT32 ui32TID;

		for (ui32TID = 0; ui32TID < RGXFW_THREAD_NUM; ui32TID++)
		{
			g_sLISRExecutionInfo.aui32InterruptCountSnapshot[ui32TID] = psFwSysData->aui32InterruptCount[ui32TID];
		}

		g_sLISRExecutionInfo.ui32State = 0;
		g_sLISRExecutionInfo.ui64Clockns = OSClockns64();
	}
#endif

	if (psDevInfo->bRGXPowered == IMG_FALSE)
	{
#if defined(PVRSRV_DEBUG_LISR_EXECUTION)
		g_sLISRExecutionInfo.ui32State |= RGX_LISR_DEVICE_NOT_POWERED;
#endif
		if (psFwSysData->ePowState == RGXFWIF_POW_OFF)
		{
#if defined(PVRSRV_DEBUG_LISR_EXECUTION)
			g_sLISRExecutionInfo.ui32State |= RGX_LISR_FWIF_POW_OFF;
#endif
			return bInterruptProcessed;
		}
	}

	/*
	 * Due to the remappings done by the hypervisor we
	 * will always "see" our registers in OS0s regbank
	 */
	ui32IRQStatus = OSReadHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_IRQ_OS0_EVENT_STATUS);
	ui32IRQThreadMask = (ui32IRQStatus & ~RGX_CR_IRQ_OS0_EVENT_STATUS_SOURCE_CLRMSK);

	/* Check for an interrupt from _any_ FW thread */
	if (ui32IRQThreadMask != 0)
	{
#if defined(PVRSRV_DEBUG_LISR_EXECUTION)
		g_sLISRExecutionInfo.ui32State |= RGX_LISR_EVENT_EN;
#endif

		OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_IRQ_OS0_EVENT_CLEAR, ui32IRQThreadMask);

		bInterruptProcessed = SampleIRQCount(psFwSysData->aui32InterruptCount,
											 psDevInfo->aui32SampleIRQCount);

		if (!bInterruptProcessed)
		{
#if defined(PVRSRV_DEBUG_LISR_EXECUTION)
			g_sLISRExecutionInfo.ui32State |= RGX_LISR_COUNTS_EQUAL;
#endif
			return bInterruptProcessed;
		}

		bInterruptProcessed = IMG_TRUE;
#if defined(PVRSRV_DEBUG_LISR_EXECUTION)
		g_sLISRExecutionInfo.ui32State |= RGX_LISR_PROCESSED;
#endif

		OSScheduleMISR(psDevInfo->pvMISRData);

		if (psDevInfo->pvAPMISRData != NULL)
		{
			OSScheduleMISR(psDevInfo->pvAPMISRData);
		}
	}

	return bInterruptProcessed;
}

static void RGX_MISR_ProcessKCCBDeferredList(PVRSRV_DEVICE_NODE	*psDeviceNode)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

	/* First check whether there are pending commands in Deferred KCCB List */
	OSLockAcquire(psDevInfo->hLockKCCBDeferredCommandsList);
	if (dllist_is_empty(&psDevInfo->sKCCBDeferredCommandsListHead))
	{
		OSLockRelease(psDevInfo->hLockKCCBDeferredCommandsList);
		return;
	}
	OSLockRelease(psDevInfo->hLockKCCBDeferredCommandsList);


	/* Powerlock to avoid further Power transition requests
	   while KCCB deferred list is being processed */
	eError = PVRSRVPowerLock(psDeviceNode);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Failed to acquire PowerLock (device: %p, error: %s)",
				 __func__, psDeviceNode, PVRSRVGetErrorString(eError)));
		goto _RGX_MISR_ProcessKCCBDeferredList_PowerLock_failed;
	}

	/* Try to send deferred KCCB commands Do not Poll from here*/
	eError = RGXSendCommandsFromDeferredList(psDevInfo, IMG_FALSE);

	PVRSRVPowerUnlock(psDeviceNode);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_MESSAGE,
				 "%s could not flush Deferred KCCB list, KCCB is full.",
				 __func__));
	}

_RGX_MISR_ProcessKCCBDeferredList_PowerLock_failed:

	return;
}

static void RGX_MISRHandler_CheckFWActivePowerState(void *psDevice)
{
	PVRSRV_DEVICE_NODE	*psDeviceNode = psDevice;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	RGXFWIF_SYSDATA *psFwSysData = psDevInfo->psRGXFWIfFwSysData;
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (psFwSysData->ePowState == RGXFWIF_POW_ON || psFwSysData->ePowState == RGXFWIF_POW_IDLE)
	{
		RGX_MISR_ProcessKCCBDeferredList(psDeviceNode);
	}

	if (psFwSysData->ePowState == RGXFWIF_POW_IDLE)
	{
		/* The FW is IDLE and therefore could be shut down */
		eError = RGXActivePowerRequest(psDeviceNode);

		if ((eError != PVRSRV_OK) && (eError != PVRSRV_ERROR_DEVICE_POWER_CHANGE_DENIED))
		{
			if (eError != PVRSRV_ERROR_RETRY)
			{
				PVR_DPF((PVR_DBG_WARNING,
					"%s: Failed RGXActivePowerRequest call (device: %p) with %s",
					__func__, psDeviceNode, PVRSRVGetErrorString(eError)));
				PVRSRVDebugRequest(psDeviceNode, DEBUG_REQUEST_VERBOSITY_MAX, NULL, NULL);
			}
			else
			{
				/* Re-schedule the power down request as it was deferred. */
				OSScheduleMISR(psDevInfo->pvAPMISRData);
			}
		}
	}

}

#if defined(PVR_SUPPORT_HMMU_VALIDATION)
/* Simple wrapper called when HMMU MISR is scheduled. */
static void RGXHandleHMMUInterrupt(void *pvDevice)
{
	PVRSRV_DEVICE_NODE	*psDeviceNode = pvDevice;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

	PVR_HMMUHandleMISR(psDevInfo);
}
#endif

/* Shorter defines to keep the code a bit shorter */
#define GPU_IDLE       RGXFWIF_GPU_UTIL_STATE_IDLE
#define GPU_ACTIVE     RGXFWIF_GPU_UTIL_STATE_ACTIVE
#define GPU_BLOCKED    RGXFWIF_GPU_UTIL_STATE_BLOCKED
#define MAX_ITERATIONS 64

static PVRSRV_ERROR RGXGetGpuUtilStats(PVRSRV_DEVICE_NODE *psDeviceNode,
                                       IMG_HANDLE hGpuUtilUser,
                                       RGXFWIF_GPU_UTIL_STATS *psReturnStats)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	volatile RGXFWIF_GPU_UTIL_FWCB *psUtilFWCb = psDevInfo->psRGXFWIfGpuUtilFWCb;
	RGXFWIF_GPU_UTIL_STATS *psAggregateStats;
	IMG_UINT64 ui64TimeNow;
	IMG_UINT32 ui32Attempts;
	IMG_UINT32 ui32Remainder;


	/***** (1) Initialise return stats *****/

	psReturnStats->bValid = IMG_FALSE;
	psReturnStats->ui64GpuStatIdle       = 0;
	psReturnStats->ui64GpuStatActive     = 0;
	psReturnStats->ui64GpuStatBlocked    = 0;
	psReturnStats->ui64GpuStatCumulative = 0;

	if (hGpuUtilUser == NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	psAggregateStats = hGpuUtilUser;


	/* Try to acquire GPU utilisation counters and repeat if the FW is in the middle of an update */
	for (ui32Attempts = 0; ui32Attempts < 4; ui32Attempts++)
	{
		IMG_UINT64 aui64TmpCounters[RGXFWIF_GPU_UTIL_STATE_NUM] = {0};
		IMG_UINT64 ui64LastPeriod = 0, ui64LastWord = 0, ui64LastState = 0, ui64LastTime = 0;
		IMG_UINT32 i = 0;


		/***** (2) Get latest data from shared area *****/

		OSLockAcquire(psDevInfo->hGPUUtilLock);

		/*
		 * First attempt at detecting if the FW is in the middle of an update.
		 * This should also help if the FW is in the middle of a 64 bit variable update.
		 */
		while (((ui64LastWord != psUtilFWCb->ui64LastWord) ||
				(aui64TmpCounters[ui64LastState] !=
				 psUtilFWCb->aui64StatsCounters[ui64LastState])) &&
			   (i < MAX_ITERATIONS))
		{
			ui64LastWord  = psUtilFWCb->ui64LastWord;
			ui64LastState = RGXFWIF_GPU_UTIL_GET_STATE(ui64LastWord);
			aui64TmpCounters[GPU_IDLE]    = psUtilFWCb->aui64StatsCounters[GPU_IDLE];
			aui64TmpCounters[GPU_ACTIVE]  = psUtilFWCb->aui64StatsCounters[GPU_ACTIVE];
			aui64TmpCounters[GPU_BLOCKED] = psUtilFWCb->aui64StatsCounters[GPU_BLOCKED];
			i++;
		}

		OSLockRelease(psDevInfo->hGPUUtilLock);

		if (i == MAX_ITERATIONS)
		{
			PVR_DPF((PVR_DBG_WARNING,
			         "RGXGetGpuUtilStats could not get reliable data after trying %u times", i));
			return PVRSRV_ERROR_TIMEOUT;
		}


		/***** (3) Compute return stats *****/

		/* Update temp counters to account for the time since the last update to the shared ones */
		OSMemoryBarrier(); /* Ensure the current time is read after the loop above */
		ui64TimeNow    = RGXFWIF_GPU_UTIL_GET_TIME(RGXTimeCorrGetClockns64());
		ui64LastTime   = RGXFWIF_GPU_UTIL_GET_TIME(ui64LastWord);
		ui64LastPeriod = RGXFWIF_GPU_UTIL_GET_PERIOD(ui64TimeNow, ui64LastTime);
		aui64TmpCounters[ui64LastState] += ui64LastPeriod;

		/* Get statistics for a user since its last request */
		psReturnStats->ui64GpuStatIdle = RGXFWIF_GPU_UTIL_GET_PERIOD(aui64TmpCounters[GPU_IDLE],
		                                                             psAggregateStats->ui64GpuStatIdle);
		psReturnStats->ui64GpuStatActive = RGXFWIF_GPU_UTIL_GET_PERIOD(aui64TmpCounters[GPU_ACTIVE],
		                                                               psAggregateStats->ui64GpuStatActive);
		psReturnStats->ui64GpuStatBlocked = RGXFWIF_GPU_UTIL_GET_PERIOD(aui64TmpCounters[GPU_BLOCKED],
		                                                                psAggregateStats->ui64GpuStatBlocked);
		psReturnStats->ui64GpuStatCumulative = psReturnStats->ui64GpuStatIdle +
		                                       psReturnStats->ui64GpuStatActive +
		                                       psReturnStats->ui64GpuStatBlocked;

		if (psAggregateStats->ui64TimeStamp != 0)
		{
			IMG_UINT64 ui64TimeSinceLastCall = ui64TimeNow - psAggregateStats->ui64TimeStamp;
			/* We expect to return at least 75% of the time since the last call in GPU stats */
			IMG_UINT64 ui64MinReturnedStats = ui64TimeSinceLastCall - (ui64TimeSinceLastCall / 4);

			/*
			 * If the returned stats are substantially lower than the time since
			 * the last call, then the Host might have read a partial update from the FW.
			 * If this happens, try sampling the shared counters again.
			 */
			if (psReturnStats->ui64GpuStatCumulative < ui64MinReturnedStats)
			{
				PVR_DPF((PVR_DBG_MESSAGE,
				         "%s: Return stats (%" IMG_UINT64_FMTSPEC ") too low "
				         "(call period %" IMG_UINT64_FMTSPEC ")",
				         __func__, psReturnStats->ui64GpuStatCumulative, ui64TimeSinceLastCall));
				PVR_DPF((PVR_DBG_MESSAGE, "%s: Attempt #%u has failed, trying again",
				         __func__, ui32Attempts));
				continue;
			}
		}

		break;
	}


	/***** (4) Update aggregate stats for the current user *****/

	psAggregateStats->ui64GpuStatIdle    += psReturnStats->ui64GpuStatIdle;
	psAggregateStats->ui64GpuStatActive  += psReturnStats->ui64GpuStatActive;
	psAggregateStats->ui64GpuStatBlocked += psReturnStats->ui64GpuStatBlocked;
	psAggregateStats->ui64TimeStamp       = ui64TimeNow;


	/***** (5) Convert return stats to microseconds *****/

	psReturnStats->ui64GpuStatIdle       = OSDivide64(psReturnStats->ui64GpuStatIdle, 1000, &ui32Remainder);
	psReturnStats->ui64GpuStatActive     = OSDivide64(psReturnStats->ui64GpuStatActive, 1000, &ui32Remainder);
	psReturnStats->ui64GpuStatBlocked    = OSDivide64(psReturnStats->ui64GpuStatBlocked, 1000, &ui32Remainder);
	psReturnStats->ui64GpuStatCumulative = OSDivide64(psReturnStats->ui64GpuStatCumulative, 1000, &ui32Remainder);

	/* Check that the return stats make sense */
	if (psReturnStats->ui64GpuStatCumulative == 0)
	{
		/* We can enter here only if all the RGXFWIF_GPU_UTIL_GET_PERIOD
		 * returned 0. This could happen if the GPU frequency value
		 * is not well calibrated and the FW is updating the GPU state
		 * while the Host is reading it.
		 * When such an event happens frequently, timers or the aggregate
		 * stats might not be accurate...
		 */
		PVR_DPF((PVR_DBG_WARNING, "RGXGetGpuUtilStats could not get reliable data."));
		return PVRSRV_ERROR_RESOURCE_UNAVAILABLE;
	}

	psReturnStats->bValid = IMG_TRUE;

	return PVRSRV_OK;
}

PVRSRV_ERROR SORgxGpuUtilStatsRegister(IMG_HANDLE *phGpuUtilUser)
{
	RGXFWIF_GPU_UTIL_STATS *psAggregateStats;

	/* NoStats used since this may be called outside of the register/de-register
	 * process calls which track memory use. */
	psAggregateStats = OSAllocMemNoStats(sizeof(RGXFWIF_GPU_UTIL_STATS));
	if (psAggregateStats == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psAggregateStats->ui64GpuStatIdle    = 0;
	psAggregateStats->ui64GpuStatActive  = 0;
	psAggregateStats->ui64GpuStatBlocked = 0;
	psAggregateStats->ui64TimeStamp      = 0;

	/* Not used */
	psAggregateStats->bValid = IMG_FALSE;
	psAggregateStats->ui64GpuStatCumulative = 0;

	*phGpuUtilUser = psAggregateStats;

	return PVRSRV_OK;
}

PVRSRV_ERROR SORgxGpuUtilStatsUnregister(IMG_HANDLE hGpuUtilUser)
{
	RGXFWIF_GPU_UTIL_STATS *psAggregateStats;

	if (hGpuUtilUser == NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psAggregateStats = hGpuUtilUser;
	OSFreeMemNoStats(psAggregateStats);

	return PVRSRV_OK;
}

/*
	RGX MISR Handler
*/
static void RGX_MISRHandler_Main (void *pvData)
{
	PVRSRV_DEVICE_NODE *psDeviceNode = pvData;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

	/* Give the HWPerf service a chance to transfer some data from the FW
	 * buffer to the host driver transport layer buffer.
	 */
	RGXHWPerfDataStoreCB(psDeviceNode);

#if defined(PVRSRV_SYNC_CHECKPOINT_CCB)
	/* Process the signalled checkpoints in the checkpoint CCB, before
	 * handling all other notifiers. */
	RGXCheckCheckpointCCB(psDeviceNode);
#endif /* defined(PVRSRV_SYNC_CHECKPOINT_CCB) */

	/* Inform other services devices that we have finished an operation */
	PVRSRVCheckStatus(psDeviceNode);

#if defined(SUPPORT_PDVFS) && defined(RGXFW_META_SUPPORT_2ND_THREAD)
	/*
	 * Firmware CCB only exists for primary FW thread. Only requirement for
	 * non primary FW thread(s) to communicate with host driver is in the case
	 * of PDVFS running on non primary FW thread.
	 * This requirement is directly handled by the below
	 */
	RGXPDVFSCheckCoreClkRateChange(psDeviceNode->pvDevice);
#endif

	/* Process the Firmware CCB for pending commands */
	RGXCheckFirmwareCCB(psDeviceNode->pvDevice);

	/* Calibrate the GPU frequency and recorrelate Host and GPU timers (done every few seconds) */
	RGXTimeCorrRestartPeriodic(psDeviceNode);

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	/* Process Workload Estimation Specific commands from the FW */
	WorkEstCheckFirmwareCCB(psDeviceNode->pvDevice);
#endif

	if (psDevInfo->pvAPMISRData == NULL)
	{
		RGX_MISR_ProcessKCCBDeferredList(psDeviceNode);
	}
}
#endif /* !defined(NO_HARDWARE) */

#if defined(SUPPORT_GPUVIRT_VALIDATION)

PVRSRV_ERROR PVRSRVGPUVIRTPopulateLMASubArenasKM(PVRSRV_DEVICE_NODE	*psDeviceNode,
                                                 IMG_UINT32          aui32OSidMin[GPUVIRT_VALIDATION_NUM_REGIONS][GPUVIRT_VALIDATION_NUM_OS],
                                                 IMG_UINT32          aui32OSidMax[GPUVIRT_VALIDATION_NUM_REGIONS][GPUVIRT_VALIDATION_NUM_OS],
                                                 IMG_BOOL            bEnableTrustedDeviceAceConfig)
{
	IMG_UINT32 ui32OS, ui32Region;

	for (ui32OS = 0; ui32OS < GPUVIRT_VALIDATION_NUM_OS; ui32OS++)
	{
		for (ui32Region = 0; ui32Region < GPUVIRT_VALIDATION_NUM_REGIONS; ui32Region++)
		{
			PVR_DPF((PVR_DBG_MESSAGE,
			         "OS=%u, Region=%u, Min=%u, Max=%u",
			         ui32OS,
			         ui32Region,
			         aui32OSidMin[ui32Region][ui32OS],
			         aui32OSidMax[ui32Region][ui32OS]));
		}
	}

	PopulateLMASubArenas(psDeviceNode, aui32OSidMin, aui32OSidMax);

	PVR_UNREFERENCED_PARAMETER(bEnableTrustedDeviceAceConfig);

	return PVRSRV_OK;
}
#endif /* defined(SUPPORT_GPUVIRT_VALIDATION) */

static PVRSRV_ERROR RGXSetPowerParams(PVRSRV_RGXDEV_INFO   *psDevInfo,
                                      PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	/* Save information used on power transitions for later
	 * (when RGXStart and RGXStop are executed)
	 */
	psDevInfo->sLayerParams.psDevInfo = psDevInfo;
	psDevInfo->sLayerParams.psDevConfig = psDevConfig;
#if defined(PDUMP)
	psDevInfo->sLayerParams.ui32PdumpFlags = PDUMP_FLAGS_CONTINUOUS;
#endif
	if (RGX_IS_FEATURE_VALUE_SUPPORTED(psDevInfo, META))
	{
		IMG_DEV_PHYADDR sKernelMMUCtxPCAddr;

		eError = MMU_AcquireBaseAddr(psDevInfo->psKernelMMUCtx,
		                             &sKernelMMUCtxPCAddr);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "RGXSetPowerParams: Failed to acquire Kernel MMU Ctx page catalog"));
			return eError;
		}

		psDevInfo->sLayerParams.sPCAddr = sKernelMMUCtxPCAddr;
	}

#if defined(SUPPORT_TRUSTED_DEVICE) && !defined(NO_HARDWARE) && !defined(SUPPORT_SECURITY_VALIDATION)
	/* Send information used on power transitions to the trusted device as
	 * in this setup the driver cannot start/stop the GPU and perform resets
	 */
	if (psDevConfig->pfnTDSetPowerParams)
	{
		PVRSRV_TD_POWER_PARAMS sTDPowerParams;

		if (RGX_IS_FEATURE_VALUE_SUPPORTED(psDevInfo, META))
		{
			sTDPowerParams.sPCAddr = psDevInfo->sLayerParams.sPCAddr;
		}
		eError = psDevConfig->pfnTDSetPowerParams(psDevConfig->hSysData,
												  &sTDPowerParams);
	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXSetPowerParams: TDSetPowerParams not implemented!"));
		eError = PVRSRV_ERROR_NOT_IMPLEMENTED;
	}
#endif

	return eError;
}

/*
	RGXSystemGetFabricCoherency
*/
PVRSRV_ERROR RGXSystemGetFabricCoherency(IMG_CPU_PHYADDR sRegsCpuPBase,
										 IMG_UINT32 ui32RegsSize,
										 PVRSRV_DEVICE_FABRIC_TYPE *peDevFabricType,
										 PVRSRV_DEVICE_SNOOP_MODE *peCacheSnoopingMode)
{
	IMG_CHAR *aszLabels[] = {"none", "acelite", "fullace", "unknown"};
	PVRSRV_DEVICE_SNOOP_MODE eAppHintCacheSnoopingMode;
	PVRSRV_DEVICE_SNOOP_MODE eDeviceCacheSnoopingMode;
	IMG_UINT32 ui32AppHintFabricCoherency;
	IMG_UINT32 ui32DeviceFabricCoherency;
#if !defined(NO_HARDWARE)
	void *pvRegsBaseKM;
#endif

	if (!sRegsCpuPBase.uiAddr || !ui32RegsSize)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "RGXSystemGetFabricCoherency: Invalid RGX register base/size parameters"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

#if !defined(NO_HARDWARE)
	pvRegsBaseKM = OSMapPhysToLin(sRegsCpuPBase, ui32RegsSize, PVRSRV_MEMALLOCFLAG_CPU_UNCACHED);
	if (! pvRegsBaseKM)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "RGXSystemGetFabricCoherency: Failed to create RGX register mapping"));
		return PVRSRV_ERROR_BAD_MAPPING;
	}

	/* AXI support within the SoC, bitfield COHERENCY_SUPPORT [1 .. 0]
		value NO_COHERENCY        0x0 {SoC does not support any form of Coherency}
		value ACE_LITE_COHERENCY  0x1 {SoC supports ACE-Lite or I/O Coherency}
		value FULL_ACE_COHERENCY  0x2 {SoC supports full ACE or 2-Way Coherency} */
	ui32DeviceFabricCoherency = OSReadHWReg32(pvRegsBaseKM, RGX_CR_SOC_AXI);
	PVR_LOG(("AXI fabric coherency (RGX_CR_SOC_AXI): 0x%x", ui32DeviceFabricCoherency));
#if defined(DEBUG)
	if (ui32DeviceFabricCoherency & ~((IMG_UINT32)RGX_CR_SOC_AXI_MASKFULL))
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Invalid RGX_CR_SOC_AXI value.", __func__));
		return PVRSRV_ERROR_INVALID_DEVICE;
	}
#endif
	ui32DeviceFabricCoherency &= ~((IMG_UINT32)RGX_CR_SOC_AXI_COHERENCY_SUPPORT_CLRMSK);
	ui32DeviceFabricCoherency >>= RGX_CR_SOC_AXI_COHERENCY_SUPPORT_SHIFT;

	/* UnMap Regs */
	OSUnMapPhysToLin(pvRegsBaseKM, ui32RegsSize, PVRSRV_MEMALLOCFLAG_CPU_UNCACHED);

	switch (ui32DeviceFabricCoherency)
	{
	case RGX_CR_SOC_AXI_COHERENCY_SUPPORT_FULL_ACE_COHERENCY:
		eDeviceCacheSnoopingMode = PVRSRV_DEVICE_SNOOP_CROSS;
		*peDevFabricType = PVRSRV_DEVICE_FABRIC_FULLACE;
		break;

	case RGX_CR_SOC_AXI_COHERENCY_SUPPORT_ACE_LITE_COHERENCY:
		eDeviceCacheSnoopingMode = PVRSRV_DEVICE_SNOOP_CPU_ONLY;
		*peDevFabricType = PVRSRV_DEVICE_FABRIC_ACELITE;
		break;

	case RGX_CR_SOC_AXI_COHERENCY_SUPPORT_NO_COHERENCY:
	default:
		eDeviceCacheSnoopingMode = PVRSRV_DEVICE_SNOOP_NONE;
		*peDevFabricType = PVRSRV_DEVICE_FABRIC_NONE;
		break;
	}
#else
#if defined(RGX_FEATURE_GPU_CPU_COHERENCY)
	*peDevFabricType = PVRSRV_DEVICE_FABRIC_FULLACE;
	eDeviceCacheSnoopingMode = PVRSRV_DEVICE_SNOOP_CROSS;
	ui32DeviceFabricCoherency = RGX_CR_SOC_AXI_COHERENCY_SUPPORT_FULL_ACE_COHERENCY;
#else
	*peDevFabricType = PVRSRV_DEVICE_FABRIC_ACELITE;
	eDeviceCacheSnoopingMode = PVRSRV_DEVICE_SNOOP_CPU_ONLY;
	ui32DeviceFabricCoherency = RGX_CR_SOC_AXI_COHERENCY_SUPPORT_ACE_LITE_COHERENCY;
#endif
#endif

	pvr_apphint_get_uint32(APPHINT_ID_FabricCoherencyOverride, &ui32AppHintFabricCoherency);

#if defined(SUPPORT_SECURITY_VALIDATION)
	/* Temporarily disable coherency */
	ui32AppHintFabricCoherency = RGX_CR_SOC_AXI_COHERENCY_SUPPORT_NO_COHERENCY;
#endif

	/* Suppress invalid AppHint value */
	switch (ui32AppHintFabricCoherency)
	{
	case RGX_CR_SOC_AXI_COHERENCY_SUPPORT_NO_COHERENCY:
		eAppHintCacheSnoopingMode = PVRSRV_DEVICE_SNOOP_NONE;
		break;

	case RGX_CR_SOC_AXI_COHERENCY_SUPPORT_ACE_LITE_COHERENCY:
		eAppHintCacheSnoopingMode = PVRSRV_DEVICE_SNOOP_CPU_ONLY;
		break;

	case RGX_CR_SOC_AXI_COHERENCY_SUPPORT_FULL_ACE_COHERENCY:
		eAppHintCacheSnoopingMode = PVRSRV_DEVICE_SNOOP_CROSS;
		break;

	default:
		PVR_DPF((PVR_DBG_ERROR,
				"Invalid FabricCoherencyOverride AppHint %d, ignoring",
				ui32AppHintFabricCoherency));
		eAppHintCacheSnoopingMode = PVRSRV_DEVICE_SNOOP_CROSS;
		ui32AppHintFabricCoherency = RGX_CR_SOC_AXI_COHERENCY_SUPPORT_FULL_ACE_COHERENCY;
		break;
	}

	if (ui32AppHintFabricCoherency < ui32DeviceFabricCoherency)
	{
		PVR_LOG(("Downgrading device fabric coherency from %s to %s",
				aszLabels[ui32DeviceFabricCoherency],
				aszLabels[ui32AppHintFabricCoherency]));
		eDeviceCacheSnoopingMode = eAppHintCacheSnoopingMode;
	}
	else if (ui32AppHintFabricCoherency > ui32DeviceFabricCoherency)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"Cannot upgrade device fabric coherency from %s to %s, ignoring",
				aszLabels[ui32DeviceFabricCoherency],
				aszLabels[ui32AppHintFabricCoherency]));

		/* Override requested-for app-hint with actual app-hint value being used */
		pvr_apphint_set_uint32(APPHINT_ID_FabricCoherencyOverride, ui32DeviceFabricCoherency);
	}

	*peCacheSnoopingMode = eDeviceCacheSnoopingMode;
	return PVRSRV_OK;
}

/*
	RGXSystemHasCacheSnooping
*/
static IMG_BOOL RGXSystemHasCacheSnooping(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	if (psDeviceNode->eCacheSnoopingMode != PVRSRV_DEVICE_SNOOP_NONE)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

/*
	RGXSystemSnoopingOfCPUCache
*/
static IMG_BOOL RGXSystemSnoopingOfCPUCache(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	if ((psDeviceNode->eCacheSnoopingMode == PVRSRV_DEVICE_SNOOP_CPU_ONLY) ||
		(psDeviceNode->eCacheSnoopingMode == PVRSRV_DEVICE_SNOOP_CROSS))
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

/*
	RGXSystemSnoopingOfDeviceCache
*/
static IMG_BOOL RGXSystemSnoopingOfDeviceCache(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	if ((psDeviceNode->eCacheSnoopingMode == PVRSRV_DEVICE_SNOOP_DEVICE_ONLY) ||
		(psDeviceNode->eCacheSnoopingMode == PVRSRV_DEVICE_SNOOP_CROSS))
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

/*
	RGXSystemHasNonMappableLocalMemory
*/
static IMG_BOOL RGXSystemHasNonMappableLocalMemory(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	return psDeviceNode->psDevConfig->bHasNonMappableLocalMemory;
}

/*
	RGXSystemHasFBCDCVersion31
*/
static IMG_BOOL RGXSystemHasFBCDCVersion31(PVRSRV_DEVICE_NODE *psDeviceNode)
{
#if defined(SUPPORT_VALIDATION)
	IMG_UINT32 ui32FBCDCVersionOverride = 0;
#endif

	{

#if defined(SUPPORT_VALIDATION)
		if (ui32FBCDCVersionOverride == 2)
		{
			PVR_DPF((PVR_DBG_WARNING,
			         "%s: FBCDCVersionOverride forces FBC3.1 but this core doesn't support it!",
			         __func__));
		}
#endif

#if !defined(NO_HARDWARE)
		if (psDeviceNode->psDevConfig->bHasFBCDCVersion31)
		{
			PVR_DPF((PVR_DBG_ERROR,
			         "%s: System uses FBCDC3.1 but GPU doesn't support it!",
			         __func__));
		}
#endif
	}

	return IMG_FALSE;
}

/*
 * RGXInitDevPart2
 */
PVRSRV_ERROR RGXInitDevPart2 (PVRSRV_DEVICE_NODE	*psDeviceNode,
							  IMG_UINT32			ui32DeviceFlags,
							  IMG_UINT32			ui32HWPerfHostBufSizeKB,
							  IMG_UINT32			ui32HWPerfHostFilter,
							  RGX_ACTIVEPM_CONF		eActivePMConf,
							  IMG_UINT32			ui32AvailableSPUMask)
{
	PVRSRV_ERROR			eError;
	PVRSRV_RGXDEV_INFO		*psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_DEV_POWER_STATE	eDefaultPowerState = PVRSRV_DEV_POWER_STATE_ON;
	PVRSRV_DEVICE_CONFIG	*psDevConfig = psDeviceNode->psDevConfig;
	IMG_UINT32			ui32AllSPUMask = (1 << psDevInfo->sDevFeatureCfg.ui32MAXDustCount) - 1;

#if defined(TIMING) || defined(DEBUG)
	OSUserModeAccessToPerfCountersEn();
#endif

	PDUMPCOMMENT("RGX Initialisation Part 2");

	psDevInfo->ui32RegSize = psDevConfig->ui32RegsSize;
	psDevInfo->sRegsPhysBase = psDevConfig->sRegsCpuPBase;

	/* Initialise Device Flags */
	psDevInfo->ui32DeviceFlags = 0;
	RGXSetDeviceFlags(psDevInfo, ui32DeviceFlags, IMG_TRUE);

	/* Allocate DVFS Table (needs to be allocated before GPU trace events
	 *  component is initialised because there is a dependency between them) */
	psDevInfo->psGpuDVFSTable = OSAllocZMem(sizeof(*(psDevInfo->psGpuDVFSTable)));
	if (psDevInfo->psGpuDVFSTable == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "PVRSRVRGXInitDevPart2KM: failed to allocate gpu dvfs table storage"));
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	/* Initialise HWPerfHost buffer. */
	if (RGXHWPerfHostInit(psDevInfo, ui32HWPerfHostBufSizeKB) == PVRSRV_OK)
	{
		if (psDevInfo->ui32HWPerfHostFilter == 0)
		{
			RGXHWPerfHostSetEventFilter(psDevInfo, ui32HWPerfHostFilter);
		}

		/* If HWPerf enabled allocate all resources for the host side buffer. */
		if (psDevInfo->ui32HWPerfHostFilter != 0)
		{
			if (RGXHWPerfHostInitOnDemandResources(psDevInfo) != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_WARNING, "HWPerfHost buffer on demand"
				        " initialisation failed."));
			}
		}
	}
	else
	{
		PVR_DPF((PVR_DBG_WARNING, "HWPerfHost buffer initialisation failed."));
	}

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	/* Initialise work estimation lock */
	eError = OSLockCreate(&psDevInfo->hWorkEstLock);
	PVR_ASSERT(eError == PVRSRV_OK);
#endif

	/* Initialise lists of ZSBuffers */
	eError = OSLockCreate(&psDevInfo->hLockZSBuffer);
	PVR_ASSERT(eError == PVRSRV_OK);
	dllist_init(&psDevInfo->sZSBufferHead);
	psDevInfo->ui32ZSBufferCurrID = 1;

	/* Initialise lists of growable Freelists */
	eError = OSLockCreate(&psDevInfo->hLockFreeList);
	PVR_ASSERT(eError == PVRSRV_OK);
	dllist_init(&psDevInfo->sFreeListHead);
	psDevInfo->ui32FreelistCurrID = 1;

	eError = OSLockCreate(&psDevInfo->hDebugFaultInfoLock);

	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	if (GetInfoPageDebugFlagsKM() & DEBUG_FEATURE_PAGE_FAULT_DEBUG_ENABLED)
	{

		eError = OSLockCreate(&psDevInfo->hMMUCtxUnregLock);

		if (eError != PVRSRV_OK)
		{
			return eError;
		}
	}

	/* Setup GPU utilisation stats update callback */
	eError = OSLockCreate(&psDevInfo->hGPUUtilLock);
	PVR_ASSERT(eError == PVRSRV_OK);
#if !defined(NO_HARDWARE)
	psDevInfo->pfnGetGpuUtilStats = RGXGetGpuUtilStats;
#endif

	eDefaultPowerState = PVRSRV_DEV_POWER_STATE_ON;
	psDevInfo->eActivePMConf = eActivePMConf;

	/* Validate the SPU mask and initialize to number of SPUs to power up */
	if ((ui32AvailableSPUMask & ui32AllSPUMask) == 0)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s:Invalid SPU mask (All=0x%X, Non Fused=0x%X). At-least one SPU must to be powered up.",
		         __func__,
		         ui32AllSPUMask,
		         ui32AvailableSPUMask));
		return PVRSRV_ERROR_INVALID_SPU_MASK;
	}

	psDevInfo->ui32AvailableSPUMask = ui32AvailableSPUMask & ui32AllSPUMask;

#if !defined(NO_HARDWARE)
	/* set-up the Active Power Mgmt callback */
	{
		RGX_DATA *psRGXData = (RGX_DATA*) psDeviceNode->psDevConfig->hDevData;
		IMG_BOOL bSysEnableAPM = psRGXData->psRGXTimingInfo->bEnableActivePM;
		IMG_BOOL bEnableAPM = ((eActivePMConf == RGX_ACTIVEPM_DEFAULT) && bSysEnableAPM) ||
							   (eActivePMConf == RGX_ACTIVEPM_FORCE_ON);

		if (bEnableAPM && (!PVRSRV_VZ_MODE_IS(DRIVER_MODE_NATIVE)))
		{
			PVR_DPF((PVR_DBG_WARNING, "%s: Active Power Management disabled in virtualization mode", __func__));
			bEnableAPM = false;
		}

		if (bEnableAPM)
		{
			eError = OSInstallMISR(&psDevInfo->pvAPMISRData,
					RGX_MISRHandler_CheckFWActivePowerState,
					psDeviceNode,
					"RGX_CheckFWActivePower");
			if (eError != PVRSRV_OK)
			{
				return eError;
			}

			/* Prevent the device being woken up before there is something to do. */
			eDefaultPowerState = PVRSRV_DEV_POWER_STATE_OFF;
		}
	}

	/* Set up HMMU callback. */
#if defined(PVR_SUPPORT_HMMU_VALIDATION)
	eError = OSInstallMISR(&psDevInfo->pvHMMUMISRData, RGXHandleHMMUInterrupt, psDeviceNode, "RGX Handle HMMU Interrupt");
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif
#endif

	PVRSRVAppHintRegisterHandlersUINT32(APPHINT_ID_EnableAPM,
	                                    RGXQueryAPMState,
	                                    RGXSetAPMState,
	                                    psDeviceNode,
	                                    NULL);

	RGXTimeCorrInitAppHintCallbacks(psDeviceNode);

	/*
		Register the device with the power manager.
			Normal/Hyperv Drivers: Supports power management
			Guest Drivers: Do not currently support power management
	*/
	eError = PVRSRVRegisterPowerDevice(psDeviceNode,
									   &RGXPrePowerState, &RGXPostPowerState,
									   psDevConfig->pfnPrePowerState, psDevConfig->pfnPostPowerState,
									   &RGXPreClockSpeedChange, &RGXPostClockSpeedChange,
									   &RGXForcedIdleRequest, &RGXCancelForcedIdleRequest,
									   &RGXSPUPowerStateMaskChange,
									   (IMG_HANDLE)psDeviceNode,
									   PVRSRV_DEV_POWER_STATE_OFF,
									   eDefaultPowerState);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "PVRSRVRGXInitDevPart2KM: failed to register device with power manager"));
		return eError;
	}

	eError = RGXSetPowerParams(psDevInfo, psDevConfig);
	if (eError != PVRSRV_OK) return eError;

#if defined(SUPPORT_VALIDATION)
	{
		void *pvAppHintState = NULL;

		IMG_UINT32 ui32AppHintDefault;

		OSCreateKMAppHintState(&pvAppHintState);
		ui32AppHintDefault = PVRSRV_APPHINT_TESTSLRINTERVAL;
		OSGetKMAppHintUINT32(pvAppHintState, TestSLRInterval,
		                     &ui32AppHintDefault, &psDevInfo->ui32TestSLRInterval);
		PVR_LOG(("OSGetKMAppHintUINT32(TestSLRInterval) ui32AppHintDefault=%d, psDevInfo->ui32TestSLRInterval=%d",
		        ui32AppHintDefault, psDevInfo->ui32TestSLRInterval));
		OSFreeKMAppHintState(pvAppHintState);
		psDevInfo->ui32TestSLRCount = psDevInfo->ui32TestSLRInterval;
		psDevInfo->ui32SLRSkipFWAddr = 0;
	}
#endif

#if defined(PDUMP)
#if defined(NO_HARDWARE)
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_DEINIT, "Wait for the FW to signal idle");

	/* Kick the FW once, in case it still needs to detect and set the idle state */
	PDUMPREG32(RGX_PDUMPREG_NAME,
			   RGX_CR_MTS_SCHEDULE,
			   RGXFWIF_DM_GP & ~RGX_CR_MTS_SCHEDULE_DM_CLRMSK,
			   PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_DEINIT);

	eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfFwSysDataMemDesc,
	                                offsetof(RGXFWIF_SYSDATA, ePowState),
	                                RGXFWIF_POW_IDLE,
	                                0xFFFFFFFFU,
	                                PDUMP_POLL_OPERATOR_EQUAL,
	                                PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_DEINIT);
	if (eError != PVRSRV_OK) return eError;
#endif

	/* Run RGXStop with the correct PDump flags to feed the last-frame deinit buffer */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_DEINIT, "RGX deinitialisation commands");

	psDevInfo->sLayerParams.ui32PdumpFlags |= PDUMP_FLAGS_DEINIT | PDUMP_FLAGS_NOHW;

	if (! PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		eError = RGXStop(&psDevInfo->sLayerParams);
		if (eError != PVRSRV_OK) return eError;
	}

	psDevInfo->sLayerParams.ui32PdumpFlags &= ~(PDUMP_FLAGS_DEINIT | PDUMP_FLAGS_NOHW);
#endif

#if !defined(NO_HARDWARE)
	eError = RGXInstallProcessQueuesMISR(&psDevInfo->hProcessQueuesMISR, psDeviceNode);
	if (eError != PVRSRV_OK)
	{
		if (psDevInfo->pvAPMISRData != NULL)
		{
			(void) OSUninstallMISR(psDevInfo->pvAPMISRData);
		}
#if defined(PVR_SUPPORT_HMMU_VALIDATION)
		if (psDevInfo->pvHMMUMISRData != NULL)
		{
			(void) OSUninstallMISR(psDevInfo->pvHMMUMISRData);
		}
#endif

		return eError;
	}

	/* Register the interrupt handlers */
	eError = OSInstallMISR(&psDevInfo->pvMISRData,
			RGX_MISRHandler_Main,
			psDeviceNode,
			"RGX_Main");
	if (eError != PVRSRV_OK)
	{
		if (psDevInfo->pvAPMISRData != NULL)
		{
			(void) OSUninstallMISR(psDevInfo->pvAPMISRData);
		}
#if defined(PVR_SUPPORT_HMMU_VALIDATION)
		if (psDevInfo->pvHMMUMISRData != NULL)
		{
			(void) OSUninstallMISR(psDevInfo->pvHMMUMISRData);
		}
#endif
		(void) OSUninstallMISR(psDevInfo->hProcessQueuesMISR);
		return eError;
	}

	eError = SysInstallDeviceLISR(psDevConfig->hSysData,
								  psDevConfig->ui32IRQ,
								  PVRSRV_MODNAME,
								  RGX_LISRHandler,
								  psDeviceNode,
								  &psDevInfo->pvLISRData);
	if (eError != PVRSRV_OK)
	{
		if (psDevInfo->pvAPMISRData != NULL)
		{
			(void) OSUninstallMISR(psDevInfo->pvAPMISRData);
		}
#if defined(PVR_SUPPORT_HMMU_VALIDATION)
		if (psDevInfo->pvHMMUMISRData != NULL)
		{
			(void) OSUninstallMISR(psDevInfo->pvHMMUMISRData);
		}
#endif
		(void) OSUninstallMISR(psDevInfo->hProcessQueuesMISR);
		(void) OSUninstallMISR(psDevInfo->pvMISRData);
		return eError;
	}

#endif

#if defined(PDUMP)
	if (!RGXSystemSnoopingOfCPUCache(psDeviceNode) &&
		!RGXSystemSnoopingOfDeviceCache(psDeviceNode))
	{
		PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "System has NO cache snooping");
	}
	else
	{
		if (RGXSystemSnoopingOfCPUCache(psDeviceNode))
		{
			PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "System has CPU cache snooping");
		}
		if (RGXSystemSnoopingOfDeviceCache(psDeviceNode))
		{
			PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "System has DEVICE cache snooping");
		}
	}
#endif

	psDevInfo->bDevInit2Done = IMG_TRUE;

	return PVRSRV_OK;
}

#define VZ_RGX_FW_FILENAME_SUFFIX ".vz"
#define RGX_FW_FILENAME_MAX_SIZE   ((sizeof(RGX_FW_FILENAME)+ \
			RGX_BVNC_STR_SIZE_MAX+sizeof(VZ_RGX_FW_FILENAME_SUFFIX)))

static void _GetFWFileName(PVRSRV_DEVICE_NODE *psDeviceNode,
		IMG_CHAR *pszFWFilenameStr,
		IMG_CHAR *pszFWpFilenameStr)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	const IMG_CHAR * const pszFWFilenameSuffix =
			PVRSRV_VZ_MODE_IS(DRIVER_MODE_NATIVE) ? "" : VZ_RGX_FW_FILENAME_SUFFIX;

	OSSNPrintf(pszFWFilenameStr, RGX_FW_FILENAME_MAX_SIZE,
			"%s." RGX_BVNC_STR_FMTSPEC "%s",
			RGX_FW_FILENAME,
			psDevInfo->sDevFeatureCfg.ui32B, psDevInfo->sDevFeatureCfg.ui32V,
			psDevInfo->sDevFeatureCfg.ui32N, psDevInfo->sDevFeatureCfg.ui32C,
			pszFWFilenameSuffix);

	OSSNPrintf(pszFWpFilenameStr, RGX_FW_FILENAME_MAX_SIZE,
			"%s." RGX_BVNC_STRP_FMTSPEC "%s",
			RGX_FW_FILENAME,
			psDevInfo->sDevFeatureCfg.ui32B, psDevInfo->sDevFeatureCfg.ui32V,
			psDevInfo->sDevFeatureCfg.ui32N, psDevInfo->sDevFeatureCfg.ui32C,
			pszFWFilenameSuffix);
}

const void * RGXLoadAndGetFWData(PVRSRV_DEVICE_NODE *psDeviceNode,
		OS_FW_IMAGE **ppsRGXFW)
{
	IMG_CHAR aszFWFilenameStr[RGX_FW_FILENAME_MAX_SIZE];
	IMG_CHAR aszFWpFilenameStr[RGX_FW_FILENAME_MAX_SIZE];
	IMG_CHAR *pszLoadedFwStr;

	/* Prepare the image filenames to use in the following code */
	_GetFWFileName(psDeviceNode, aszFWFilenameStr, aszFWpFilenameStr);

	/* Get pointer to Firmware image */
	pszLoadedFwStr = aszFWFilenameStr;
	*ppsRGXFW = OSLoadFirmware(psDeviceNode, pszLoadedFwStr, OS_FW_VERIFY_FUNCTION);
	if (*ppsRGXFW == NULL)
	{
		pszLoadedFwStr = aszFWpFilenameStr;
		*ppsRGXFW = OSLoadFirmware(psDeviceNode, pszLoadedFwStr, OS_FW_VERIFY_FUNCTION);
		if (*ppsRGXFW == NULL)
		{
			pszLoadedFwStr = RGX_FW_FILENAME;
			*ppsRGXFW = OSLoadFirmware(psDeviceNode, pszLoadedFwStr, OS_FW_VERIFY_FUNCTION);
			if (*ppsRGXFW == NULL)
			{
				PVR_DPF((PVR_DBG_FATAL, "All RGX Firmware image loads failed for '%s'",
						aszFWFilenameStr));
				return NULL;
			}
		}
	}

	PVR_LOG(("RGX Firmware image '%s' loaded", pszLoadedFwStr));

	return OSFirmwareData(*ppsRGXFW);
}

PVRSRV_ERROR RGXInitCreateFWKernelMemoryContext(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	/* set up fw memory contexts */
	PVRSRV_RGXDEV_INFO   *psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_DEVICE_CONFIG *psDevConfig = psDeviceNode->psDevConfig;
	PVRSRV_ERROR eError;

	/* Set the device fabric coherency before FW context creation */
	eError = RGXSystemGetFabricCoherency(psDevConfig->sRegsCpuPBase,
										 psDevConfig->ui32RegsSize,
										 &psDeviceNode->eDevFabricType,
										 &psDeviceNode->eCacheSnoopingMode);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Failed RGXSystemGetFabricCoherency (%u)",
		         __func__,
		         eError));
		goto failed_to_create_ctx;
	}

	/* Register callbacks for creation of device memory contexts */
	psDeviceNode->pfnRegisterMemoryContext = RGXRegisterMemoryContext;
	psDeviceNode->pfnUnregisterMemoryContext = RGXUnregisterMemoryContext;

	/* Create the memory context for the firmware. */
	eError = DevmemCreateContext(psDeviceNode, DEVMEM_HEAPCFG_META,
								 &psDevInfo->psKernelDevmemCtx);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Failed DevmemCreateContext (%u)",
		         __func__,
		         eError));
		goto failed_to_create_ctx;
	}

	eError = DevmemFindHeapByName(psDevInfo->psKernelDevmemCtx, RGX_FIRMWARE_MAIN_HEAP_IDENT,
					  &psDevInfo->psFirmwareMainHeap);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Failed DevmemFindHeapByName (%u)",
		         __func__,
		         eError));
		goto failed_to_find_heap;
	}

	eError = DevmemFindHeapByName(psDevInfo->psKernelDevmemCtx, RGX_FIRMWARE_CONFIG_HEAP_IDENT,
					  &psDevInfo->psFirmwareConfigHeap);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Failed DevmemFindHeapByName (%u)",
		         __func__,
		         eError));
		goto failed_to_find_heap;
	}

#if (RGX_NUM_OS_SUPPORTED > 1) && !defined(RGX_VZ_STATIC_CARVEOUT_FW_HEAPS)
	/* Perform additional vz specific initialization */
	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		eError = SysVzRegisterFwPhysHeap(psDeviceNode->psDevConfig);

		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
			         "%s: Failed SysVzRegisterFwPhysHeap (%u)",
			         __func__,
			         eError));
			goto failed_to_find_heap;
		}
	}
#endif /* (RGX_NUM_OS_SUPPORTED > 1) && !defined(RGX_VZ_STATIC_CARVEOUT_FW_HEAPS) */

	return eError;

failed_to_find_heap:
	/*
	 * Clear the mem context create callbacks before destroying the RGX firmware
	 * context to avoid a spurious callback.
	 */
	psDeviceNode->pfnRegisterMemoryContext = NULL;
	psDeviceNode->pfnUnregisterMemoryContext = NULL;
	DevmemDestroyContext(psDevInfo->psKernelDevmemCtx);
	psDevInfo->psKernelDevmemCtx = NULL;
failed_to_create_ctx:
	return eError;
}

void RGXDeInitDestroyFWKernelMemoryContext(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_ERROR       eError;

#if (RGX_NUM_OS_SUPPORTED > 1) && !defined(RGX_VZ_STATIC_CARVEOUT_FW_HEAPS)
	/* Perform additional vz specific initialization */
	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		SysVzUnregisterFwPhysHeap(psDeviceNode->psDevConfig);
	}
#endif /* (RGX_NUM_OS_SUPPORTED > 1) && !defined(RGX_VZ_STATIC_CARVEOUT_FW_HEAPS) */

	/*
	 * Clear the mem context create callbacks before destroying the RGX firmware
	 * context to avoid a spurious callback.
	 */
	psDeviceNode->pfnRegisterMemoryContext = NULL;
	psDeviceNode->pfnUnregisterMemoryContext = NULL;

	if (psDevInfo->psKernelDevmemCtx)
	{
		eError = DevmemDestroyContext(psDevInfo->psKernelDevmemCtx);
		/* FIXME - this should return void */
		PVR_ASSERT(eError == PVRSRV_OK);
	}
}

static PVRSRV_ERROR RGXAlignmentCheck(PVRSRV_DEVICE_NODE *psDevNode,
                                      IMG_UINT32 ui32AlignChecksSize,
                                      IMG_UINT32 aui32AlignChecks[])
{
	static IMG_UINT32 aui32AlignChecksKM[] = {RGXFW_ALIGN_CHECKS_INIT_KM};
	PVRSRV_RGXDEV_INFO *psDevInfo = psDevNode->pvDevice;
	IMG_UINT32 i, *paui32FWAlignChecks;
	PVRSRV_ERROR eError = PVRSRV_OK;

	/* Skip the alignment check if the driver is guest
	   since there is no firmware to check against */
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_GUEST, eError);

	if (psDevInfo->psRGXFWAlignChecksMemDesc == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: FW Alignment Check Mem Descriptor is NULL",
		         __func__));
		return PVRSRV_ERROR_ALIGNMENT_ARRAY_NOT_AVAILABLE;
	}

	eError = DevmemAcquireCpuVirtAddr(psDevInfo->psRGXFWAlignChecksMemDesc,
	                                  (void **) &paui32FWAlignChecks);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Failed to acquire kernel address for alignment checks (%u)",
		         __func__,
		         eError));
		return eError;
	}

	paui32FWAlignChecks += ARRAY_SIZE(aui32AlignChecksKM) + 1;
	if (*paui32FWAlignChecks++ != ui32AlignChecksSize)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Mismatch in number of structures to check.",
		         __func__));
		eError = PVRSRV_ERROR_INVALID_ALIGNMENT;
		goto return_;
	}

	for (i = 0; i < ui32AlignChecksSize; i++)
	{
		if (aui32AlignChecks[i] != paui32FWAlignChecks[i])
		{
			PVR_DPF((PVR_DBG_ERROR,
			         "%s: Check for structured alignment failed.",
			         __func__));
			eError = PVRSRV_ERROR_INVALID_ALIGNMENT;
			goto return_;
		}
	}

return_:

	DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWAlignChecksMemDesc);

	return eError;
}

static
PVRSRV_ERROR RGXAllocateFWMemoryRegion(PVRSRV_DEVICE_NODE *psDeviceNode,
                                       IMG_DEVMEM_SIZE_T ui32Size,
                                       IMG_UINT32 uiMemAllocFlags,
                                       PVRSRV_TD_FW_MEM_REGION eRegion,
                                       const IMG_PCHAR pszText,
                                       DEVMEM_MEMDESC **ppsMemDescPtr)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	IMG_DEVMEM_LOG2ALIGN_T uiLog2Align = OSGetPageShift();

#if defined(SUPPORT_DEDICATED_FW_MEMORY)
	PVR_UNREFERENCED_PARAMETER(eRegion);

	PDUMPCOMMENT("Allocate dedicated FW %s memory", pszText);

	eError = DevmemAllocateDedicatedFWMem(psDeviceNode,
			ui32Size,
			uiLog2Align,
			uiMemAllocFlags,
			pszText,
			ppsMemDescPtr);
#else  /* defined(SUPPORT_DEDICATED_FW_MEMORY) */

#if defined(SUPPORT_TRUSTED_DEVICE)
	PDUMPCOMMENT("Import secure FW %s memory", pszText);

	eError = DevmemImportTDFWMem(psDeviceNode,
	                             ui32Size,
	                             uiLog2Align,
	                             uiMemAllocFlags,
	                             eRegion,
	                             ppsMemDescPtr);
#else
	uiMemAllocFlags |= PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE |
	                   PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC;

	PVR_UNREFERENCED_PARAMETER(uiLog2Align);
	PVR_UNREFERENCED_PARAMETER(eRegion);

	PDUMPCOMMENT("Allocate FW %s memory", pszText);

	eError = DevmemFwAllocate(psDeviceNode->pvDevice,
	                          ui32Size,
	                          uiMemAllocFlags,
	                          pszText,
	                          ppsMemDescPtr);
#endif

#endif /* defined(SUPPORT_DEDICATED_FW_MEMORY) */

	return eError;
}

/*!
*******************************************************************************

 @Function	RGXDevInitCompatCheck_KMBuildOptions_FWAgainstDriver

 @Description

 Validate the FW build options against KM driver build options (KM build options only)

 Following check is redundant, because next check checks the same bits.
 Redundancy occurs because if client-server are build-compatible and client-firmware are
 build-compatible then server-firmware are build-compatible as well.

 This check is left for clarity in error messages if any incompatibility occurs.

 @Input psFwOsInit - FW init data

 @Return   PVRSRV_ERROR - depending on mismatch found

******************************************************************************/
static PVRSRV_ERROR RGXDevInitCompatCheck_KMBuildOptions_FWAgainstDriver(RGXFWIF_OSINIT *psFwOsInit)
{
#if !defined(NO_HARDWARE)
	IMG_UINT32			ui32BuildOptions, ui32BuildOptionsFWKMPart, ui32BuildOptionsMismatch;

	if (psFwOsInit == NULL)
		return PVRSRV_ERROR_INVALID_PARAMS;

	ui32BuildOptions = (RGX_BUILD_OPTIONS_KM);

	ui32BuildOptionsFWKMPart = psFwOsInit->sRGXCompChecks.ui32BuildOptions & RGX_BUILD_OPTIONS_MASK_KM;

	if (ui32BuildOptions != ui32BuildOptionsFWKMPart)
	{
		ui32BuildOptionsMismatch = ui32BuildOptions ^ ui32BuildOptionsFWKMPart;
#if !defined(PVRSRV_STRICT_COMPAT_CHECK)
		/*Mask the debug flag option out as we do support combinations of debug vs release in um & km*/
		ui32BuildOptionsMismatch &= ~OPTIONS_DEBUG_MASK;
#endif
		if ( (ui32BuildOptions & ui32BuildOptionsMismatch) != 0)
		{
			PVR_LOG(("(FAIL) RGXDevInitCompatCheck: Mismatch in Firmware and KM driver build options; "
				"extra options present in the KM driver: (0x%x). Please check rgx_options.h",
				ui32BuildOptions & ui32BuildOptionsMismatch ));
			return PVRSRV_ERROR_BUILD_OPTIONS_MISMATCH;
		}

		if ( (ui32BuildOptionsFWKMPart & ui32BuildOptionsMismatch) != 0)
		{
			PVR_LOG(("(FAIL) RGXDevInitCompatCheck: Mismatch in Firmware-side and KM driver build options; "
				"extra options present in Firmware: (0x%x). Please check rgx_options.h",
				ui32BuildOptionsFWKMPart & ui32BuildOptionsMismatch ));
			return PVRSRV_ERROR_BUILD_OPTIONS_MISMATCH;
		}
		PVR_DPF((PVR_DBG_WARNING, "RGXDevInitCompatCheck: Firmware and KM driver build options differ."));
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE, "RGXDevInitCompatCheck: Firmware and KM driver build options match. [ OK ]"));
	}
#endif

	return PVRSRV_OK;
}

/*!
*******************************************************************************

 @Function	RGXDevInitCompatCheck_DDKVersion_FWAgainstDriver

 @Description

 Validate FW DDK version against driver DDK version

 @Input psDevInfo - device info
 @Input psFwOsInit - FW init data

 @Return   PVRSRV_ERROR - depending on mismatch found

******************************************************************************/
static PVRSRV_ERROR RGXDevInitCompatCheck_DDKVersion_FWAgainstDriver(PVRSRV_RGXDEV_INFO *psDevInfo,
																			RGXFWIF_OSINIT *psFwOsInit)
{
#if defined(PDUMP)||(!defined(NO_HARDWARE))
	IMG_UINT32			ui32DDKVersion;
	PVRSRV_ERROR		eError;

	ui32DDKVersion = PVRVERSION_PACK(PVRVERSION_MAJ, PVRVERSION_MIN);
#endif

#if defined(PDUMP)
	PDUMPCOMMENT("Compatibility check: KM driver and FW DDK version");
	eError = DevmemPDumpDevmemCheck32(psDevInfo->psRGXFWIfOsInitMemDesc,
												offsetof(RGXFWIF_OSINIT, sRGXCompChecks) +
												offsetof(RGXFWIF_COMPCHECKS, ui32DDKVersion),
												ui32DDKVersion,
												0xffffffff,
												PDUMP_POLL_OPERATOR_EQUAL,
												PDUMP_FLAGS_CONTINUOUS);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfOsInitMemDesc (%d)", eError));
		return eError;
	}
#endif

#if !defined(NO_HARDWARE)
	if (psFwOsInit == NULL)
		return PVRSRV_ERROR_INVALID_PARAMS;

	if (psFwOsInit->sRGXCompChecks.ui32DDKVersion != ui32DDKVersion)
	{
		PVR_LOG(("(FAIL) RGXDevInitCompatCheck: Incompatible driver DDK version (%u.%u) / Firmware DDK revision (%u.%u).",
				PVRVERSION_MAJ, PVRVERSION_MIN,
				PVRVERSION_UNPACK_MAJ(psFwOsInit->sRGXCompChecks.ui32DDKVersion),
				PVRVERSION_UNPACK_MIN(psFwOsInit->sRGXCompChecks.ui32DDKVersion)));
		eError = PVRSRV_ERROR_DDK_VERSION_MISMATCH;
		PVR_DBG_BREAK;
		return eError;
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE, "RGXDevInitCompatCheck: driver DDK version (%u.%u) and Firmware DDK revision (%u.%u) match. [ OK ]",
				PVRVERSION_MAJ, PVRVERSION_MIN,
				PVRVERSION_MAJ, PVRVERSION_MIN));
	}
#endif

	return PVRSRV_OK;
}

/*!
*******************************************************************************

 @Function	RGXDevInitCompatCheck_DDKBuild_FWAgainstDriver

 @Description

 Validate FW DDK build against driver DDK build

 @Input psDevInfo - device info
 @Input psFwOsInit - FW init data

 @Return   PVRSRV_ERROR - depending on mismatch found

******************************************************************************/
static PVRSRV_ERROR RGXDevInitCompatCheck_DDKBuild_FWAgainstDriver(PVRSRV_RGXDEV_INFO *psDevInfo,
																			RGXFWIF_OSINIT *psFwOsInit)
{
	PVRSRV_ERROR		eError=PVRSRV_OK;
#if defined(PDUMP)||(!defined(NO_HARDWARE))
	IMG_UINT32			ui32DDKBuild;

	ui32DDKBuild = PVRVERSION_BUILD;
#endif

#if defined(PDUMP) && defined(PVRSRV_STRICT_COMPAT_CHECK)
	PDUMPCOMMENT("Compatibility check: KM driver and FW DDK build");
	eError = DevmemPDumpDevmemCheck32(psDevInfo->psRGXFWIfOsInitMemDesc,
												offsetof(RGXFWIF_OSINIT, sRGXCompChecks) +
												offsetof(RGXFWIF_COMPCHECKS, ui32DDKBuild),
												ui32DDKBuild,
												0xffffffff,
												PDUMP_POLL_OPERATOR_EQUAL,
												PDUMP_FLAGS_CONTINUOUS);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfOsInitMemDesc (%d)", eError));
		return eError;
	}
#endif

#if !defined(NO_HARDWARE)
	if (psFwOsInit == NULL)
		return PVRSRV_ERROR_INVALID_PARAMS;

	if (psFwOsInit->sRGXCompChecks.ui32DDKBuild != ui32DDKBuild)
	{
		PVR_LOG(("(WARN) RGXDevInitCompatCheck: Different driver DDK build version (%d) / Firmware DDK build version (%d).",
				ui32DDKBuild, psFwOsInit->sRGXCompChecks.ui32DDKBuild));
#if defined(PVRSRV_STRICT_COMPAT_CHECK)
		eError = PVRSRV_ERROR_DDK_BUILD_MISMATCH;
		PVR_DBG_BREAK;
		return eError;
#endif
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE, "RGXDevInitCompatCheck: driver DDK build version (%d) and Firmware DDK build version (%d) match. [ OK ]",
				ui32DDKBuild, psFwOsInit->sRGXCompChecks.ui32DDKBuild));
	}
#endif
	return eError;
}

/*!
*******************************************************************************

 @Function	RGXDevInitCompatCheck_BVNC_FWAgainstDriver

 @Description

 Validate FW BVNC against driver BVNC

 @Input psDevInfo - device info
 @Input psFwOsInit - FW init data

 @Return   PVRSRV_ERROR - depending on mismatch found

******************************************************************************/
static PVRSRV_ERROR RGXDevInitCompatCheck_BVNC_FWAgainstDriver(PVRSRV_RGXDEV_INFO *psDevInfo,
																			RGXFWIF_OSINIT *psFwOsInit)
{
#if !defined(NO_HARDWARE)
	IMG_BOOL bCompatibleAll, bCompatibleVersion, bCompatibleBVNC;
#endif
#if defined(PDUMP)||(!defined(NO_HARDWARE))
	RGXFWIF_COMPCHECKS_BVNC_DECLARE_AND_INIT(sBVNC);
	PVRSRV_ERROR				eError;

	sBVNC.ui64BVNC = rgx_bvnc_pack(psDevInfo->sDevFeatureCfg.ui32B,
					psDevInfo->sDevFeatureCfg.ui32V,
					psDevInfo->sDevFeatureCfg.ui32N,
					psDevInfo->sDevFeatureCfg.ui32C);
#endif

#if defined(PDUMP)
	PDUMPCOMMENT("Compatibility check: KM driver and FW BVNC (struct version)");
	eError = DevmemPDumpDevmemCheck32(psDevInfo->psRGXFWIfOsInitMemDesc,
											offsetof(RGXFWIF_OSINIT, sRGXCompChecks) +
											offsetof(RGXFWIF_COMPCHECKS, sFWBVNC) +
											offsetof(RGXFWIF_COMPCHECKS_BVNC, ui32LayoutVersion),
											sBVNC.ui32LayoutVersion,
											0xffffffff,
											PDUMP_POLL_OPERATOR_EQUAL,
											PDUMP_FLAGS_CONTINUOUS);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfOsInitMemDesc (%d)", eError));
	}


	PDUMPCOMMENT("Compatibility check: KM driver and FW BVNC (BNC part - lower 32 bits)");
	eError = DevmemPDumpDevmemCheck32(psDevInfo->psRGXFWIfOsInitMemDesc,
											offsetof(RGXFWIF_OSINIT, sRGXCompChecks) +
											offsetof(RGXFWIF_COMPCHECKS, sFWBVNC) +
											offsetof(RGXFWIF_COMPCHECKS_BVNC, ui64BVNC),
											(IMG_UINT32)sBVNC.ui64BVNC,
											0xffffffff,
											PDUMP_POLL_OPERATOR_EQUAL,
											PDUMP_FLAGS_CONTINUOUS);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfOsInitMemDesc (%d)", eError));
	}

	PDUMPCOMMENT("Compatibility check: KM driver and FW BVNC (BNC part - Higher 32 bits)");
	eError = DevmemPDumpDevmemCheck32(psDevInfo->psRGXFWIfOsInitMemDesc,
											offsetof(RGXFWIF_OSINIT, sRGXCompChecks) +
											offsetof(RGXFWIF_COMPCHECKS, sFWBVNC) +
											offsetof(RGXFWIF_COMPCHECKS_BVNC, ui64BVNC) +
											sizeof(IMG_UINT32),
											(IMG_UINT32)(sBVNC.ui64BVNC >> 32),
											0xffffffff,
											PDUMP_POLL_OPERATOR_EQUAL,
											PDUMP_FLAGS_CONTINUOUS);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfOsInitMemDesc (%d)", eError));
	}
#endif

#if !defined(NO_HARDWARE)
	if (psFwOsInit == NULL)
		return PVRSRV_ERROR_INVALID_PARAMS;

	RGX_BVNC_EQUAL(sBVNC, psFwOsInit->sRGXCompChecks.sFWBVNC, bCompatibleAll, bCompatibleVersion, bCompatibleBVNC);

	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		bCompatibleAll = IMG_TRUE;
	}

	if (!bCompatibleAll)
	{
		if (!bCompatibleVersion)
		{
			PVR_LOG(("(FAIL) %s: Incompatible compatibility struct version of driver (%u) and firmware (%u).",
					__func__,
					sBVNC.ui32LayoutVersion,
					psFwOsInit->sRGXCompChecks.sFWBVNC.ui32LayoutVersion));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			return eError;
		}

		if (!bCompatibleBVNC)
		{
			PVR_LOG(("(FAIL) RGXDevInitCompatCheck: Mismatch in KM driver BVNC (%u.%u.%u.%u) and Firmware BVNC (%u.%u.%u.%u)",
					RGX_BVNC_PACKED_EXTR_B(sBVNC.ui64BVNC),
					RGX_BVNC_PACKED_EXTR_V(sBVNC.ui64BVNC),
					RGX_BVNC_PACKED_EXTR_N(sBVNC.ui64BVNC),
					RGX_BVNC_PACKED_EXTR_C(sBVNC.ui64BVNC),
					RGX_BVNC_PACKED_EXTR_B(psFwOsInit->sRGXCompChecks.sFWBVNC.ui64BVNC),
					RGX_BVNC_PACKED_EXTR_V(psFwOsInit->sRGXCompChecks.sFWBVNC.ui64BVNC),
					RGX_BVNC_PACKED_EXTR_N(psFwOsInit->sRGXCompChecks.sFWBVNC.ui64BVNC),
					RGX_BVNC_PACKED_EXTR_C(psFwOsInit->sRGXCompChecks.sFWBVNC.ui64BVNC)));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			return eError;
		}
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE, "RGXDevInitCompatCheck: Firmware BVNC and KM driver BNVC match. [ OK ]"));
	}
#endif
	return PVRSRV_OK;
}

/*!
*******************************************************************************

 @Function	RGXDevInitCompatCheck_BVNC_HWAgainstDriver

 @Description

 Validate HW BVNC against driver BVNC

 @Input psDevInfo - device info
 @Input psFwOsInit - FW init data

 @Return   PVRSRV_ERROR - depending on mismatch found

******************************************************************************/
static PVRSRV_ERROR RGXDevInitCompatCheck_BVNC_HWAgainstDriver(PVRSRV_RGXDEV_INFO *psDevInfo,
																	RGXFWIF_OSINIT *psFwOsInit)
{
#if defined(PDUMP) || !defined(NO_HARDWARE)
	IMG_UINT64 ui64MaskBVNC = RGX_BVNC_PACK_MASK_B |
								RGX_BVNC_PACK_MASK_V |
								RGX_BVNC_PACK_MASK_N |
								RGX_BVNC_PACK_MASK_C;

	PVRSRV_ERROR				eError;
	RGXFWIF_COMPCHECKS_BVNC_DECLARE_AND_INIT(sSWBVNC);
#endif

#if !defined(NO_HARDWARE)
	RGXFWIF_COMPCHECKS_BVNC_DECLARE_AND_INIT(sHWBVNC);
	IMG_BOOL bCompatibleAll, bCompatibleVersion, bCompatibleBVNC;
#endif

#if defined(PDUMP)
	PDUMP_FLAGS_T ui32PDumpFlags = PDUMP_FLAGS_CONTINUOUS;
#endif

	if (psDevInfo->bIgnoreHWReportedBVNC)
	{
		PVR_LOG(("BVNC compatibility checks between driver and HW are disabled (AppHint override)"));
		return PVRSRV_OK;
	}

#if defined(PDUMP) || !defined(NO_HARDWARE)
#if defined(COMPAT_BVNC_MASK_B)
	ui64MaskBNC &= ~RGX_BVNC_PACK_MASK_B;
#endif
#if defined(COMPAT_BVNC_MASK_V)
	ui64MaskBVNC &= ~RGX_BVNC_PACK_MASK_V;
#endif
#if defined(COMPAT_BVNC_MASK_N)
	ui64MaskBVNC &= ~RGX_BVNC_PACK_MASK_N;
#endif
#if defined(COMPAT_BVNC_MASK_C)
	ui64MaskBVNC &= ~RGX_BVNC_PACK_MASK_C;
#endif

	sSWBVNC.ui64BVNC = rgx_bvnc_pack(psDevInfo->sDevFeatureCfg.ui32B,
									psDevInfo->sDevFeatureCfg.ui32V,
									psDevInfo->sDevFeatureCfg.ui32N,
									psDevInfo->sDevFeatureCfg.ui32C);



	if (ui64MaskBVNC != (RGX_BVNC_PACK_MASK_B | RGX_BVNC_PACK_MASK_V | RGX_BVNC_PACK_MASK_N | RGX_BVNC_PACK_MASK_C))
	{
		PVR_LOG(("Compatibility checks: Ignoring fields: '%s%s%s%s' of HW BVNC.",
				((!(ui64MaskBVNC & RGX_BVNC_PACK_MASK_B))?("B"):("")),
				((!(ui64MaskBVNC & RGX_BVNC_PACK_MASK_V))?("V"):("")),
				((!(ui64MaskBVNC & RGX_BVNC_PACK_MASK_N))?("N"):("")),
				((!(ui64MaskBVNC & RGX_BVNC_PACK_MASK_C))?("C"):(""))));
	}
#endif

#if defined(PDUMP)
	PDUMPCOMMENTWITHFLAGS(ui32PDumpFlags, "Compatibility check: Layout version of compchecks struct");
	eError = DevmemPDumpDevmemCheck32(psDevInfo->psRGXFWIfOsInitMemDesc,
											offsetof(RGXFWIF_OSINIT, sRGXCompChecks) +
											offsetof(RGXFWIF_COMPCHECKS, sHWBVNC) +
											offsetof(RGXFWIF_COMPCHECKS_BVNC, ui32LayoutVersion),
											sSWBVNC.ui32LayoutVersion,
											0xffffffff,
											PDUMP_POLL_OPERATOR_EQUAL,
											ui32PDumpFlags);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfOsInitMemDesc (%d)", eError));
		return eError;
	}

	PDUMPCOM(ui32PDumpFlags, "BVNC compatibility check started");
	if (ui64MaskBVNC & (RGX_BVNC_PACK_MASK_B | RGX_BVNC_PACK_MASK_N | RGX_BVNC_PACK_MASK_C))
	{
		PDUMPIF("DISABLE_HWBNC_CHECK", ui32PDumpFlags);
		PDUMPELSE("DISABLE_HWBNC_CHECK", ui32PDumpFlags);
		PDUMPCOMMENTWITHFLAGS(ui32PDumpFlags, "Compatibility check: HW BNC and FW BNC (Lower 32 bits)");
		eError = DevmemPDumpDevmemCheck32(psDevInfo->psRGXFWIfOsInitMemDesc,
												offsetof(RGXFWIF_OSINIT, sRGXCompChecks) +
												offsetof(RGXFWIF_COMPCHECKS, sHWBVNC) +
												offsetof(RGXFWIF_COMPCHECKS_BVNC, ui64BVNC),
												(IMG_UINT32)sSWBVNC.ui64BVNC ,
												(IMG_UINT32)(ui64MaskBVNC & ~RGX_BVNC_PACK_MASK_V),
												PDUMP_POLL_OPERATOR_EQUAL,
												ui32PDumpFlags);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfOsInitMemDesc (%d)", eError));
			return eError;
		}

		PDUMPCOMMENTWITHFLAGS(ui32PDumpFlags, "Compatibility check: HW BNC and FW BNC (Higher 32 bits)");
		eError = DevmemPDumpDevmemCheck32(psDevInfo->psRGXFWIfOsInitMemDesc,
												offsetof(RGXFWIF_OSINIT, sRGXCompChecks) +
												offsetof(RGXFWIF_COMPCHECKS, sHWBVNC) +
												offsetof(RGXFWIF_COMPCHECKS_BVNC, ui64BVNC) +
												sizeof(IMG_UINT32),
												(IMG_UINT32)(sSWBVNC.ui64BVNC >> 32),
												(IMG_UINT32)((ui64MaskBVNC & ~RGX_BVNC_PACK_MASK_V) >> 32),
												PDUMP_POLL_OPERATOR_EQUAL,
												ui32PDumpFlags);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfOsInitMemDesc (%d)", eError));
			return eError;
		}

		PDUMPFI("DISABLE_HWBNC_CHECK", ui32PDumpFlags);
	}
	if (ui64MaskBVNC & RGX_BVNC_PACK_MASK_V)
	{
		PDUMPIF("DISABLE_HWV_CHECK", ui32PDumpFlags);
		PDUMPELSE("DISABLE_HWV_CHECK", ui32PDumpFlags);

		PDUMPCOMMENTWITHFLAGS(ui32PDumpFlags, "Compatibility check: HW V and FW V");
		eError = DevmemPDumpDevmemCheck32(psDevInfo->psRGXFWIfOsInitMemDesc,
					offsetof(RGXFWIF_OSINIT, sRGXCompChecks) +
					offsetof(RGXFWIF_COMPCHECKS, sHWBVNC) +
					offsetof(RGXFWIF_COMPCHECKS_BVNC, ui64BVNC) +
					((RGX_BVNC_PACK_SHIFT_V >= 32) ? sizeof(IMG_UINT32) : 0),
					(IMG_UINT32)(sSWBVNC.ui64BVNC >> ((RGX_BVNC_PACK_SHIFT_V >= 32) ? 32 : 0)),
					RGX_BVNC_PACK_MASK_V >> ((RGX_BVNC_PACK_SHIFT_V >= 32) ? 32 : 0),
					PDUMP_POLL_OPERATOR_EQUAL,
					ui32PDumpFlags);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfOsInitMemDesc (%d)", eError));
			return eError;
		}
		PDUMPFI("DISABLE_HWV_CHECK", ui32PDumpFlags);
	}
	PDUMPCOM(ui32PDumpFlags, "BVNC compatibility check finished");
#endif

#if !defined(NO_HARDWARE)
	if (psFwOsInit == NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	sHWBVNC = psFwOsInit->sRGXCompChecks.sHWBVNC;

	sHWBVNC.ui64BVNC &= ui64MaskBVNC;
	sSWBVNC.ui64BVNC &= ui64MaskBVNC;

	RGX_BVNC_EQUAL(sSWBVNC, sHWBVNC, bCompatibleAll, bCompatibleVersion, bCompatibleBVNC);

	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		bCompatibleAll = IMG_TRUE;
	}

	if (!bCompatibleAll)
	{
		if (!bCompatibleVersion)
		{
			PVR_LOG(("(FAIL) %s: Incompatible compatibility struct version of HW (%d) and FW (%d).",
					__func__,
					sHWBVNC.ui32LayoutVersion,
					sSWBVNC.ui32LayoutVersion));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			return eError;
		}

		if (!bCompatibleBVNC)
		{
			PVR_LOG(("(FAIL) RGXDevInitCompatCheck: Incompatible HW BVNC (%d.%d.%d.%d) and FW BVNC (%d.%d.%d.%d).",
					RGX_BVNC_PACKED_EXTR_B(sHWBVNC.ui64BVNC),
					RGX_BVNC_PACKED_EXTR_V(sHWBVNC.ui64BVNC),
					RGX_BVNC_PACKED_EXTR_N(sHWBVNC.ui64BVNC),
					RGX_BVNC_PACKED_EXTR_C(sHWBVNC.ui64BVNC),
					RGX_BVNC_PACKED_EXTR_B(sSWBVNC.ui64BVNC),
					RGX_BVNC_PACKED_EXTR_V(sSWBVNC.ui64BVNC),
					RGX_BVNC_PACKED_EXTR_N(sSWBVNC.ui64BVNC),
					RGX_BVNC_PACKED_EXTR_C(sSWBVNC.ui64BVNC)));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			return eError;
		}
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE, "RGXDevInitCompatCheck: HW BVNC (%d.%d.%d.%d) and FW BVNC (%d.%d.%d.%d) match. [ OK ]",
				RGX_BVNC_PACKED_EXTR_B(sHWBVNC.ui64BVNC),
				RGX_BVNC_PACKED_EXTR_V(sHWBVNC.ui64BVNC),
				RGX_BVNC_PACKED_EXTR_N(sHWBVNC.ui64BVNC),
				RGX_BVNC_PACKED_EXTR_C(sHWBVNC.ui64BVNC),
				RGX_BVNC_PACKED_EXTR_B(sSWBVNC.ui64BVNC),
				RGX_BVNC_PACKED_EXTR_V(sSWBVNC.ui64BVNC),
				RGX_BVNC_PACKED_EXTR_N(sSWBVNC.ui64BVNC),
				RGX_BVNC_PACKED_EXTR_C(sSWBVNC.ui64BVNC)));
	}
#endif

	return PVRSRV_OK;
}

/*!
*******************************************************************************

 @Function	RGXDevInitCompatCheck_METACoreVersion_AgainstDriver

 @Description

 Validate HW META version against driver META version

 @Input psDevInfo - device info
 @Input psFwOsInit - FW init data

 @Return   PVRSRV_ERROR - depending on mismatch found

******************************************************************************/
static PVRSRV_ERROR RGXDevInitCompatCheck_FWProcessorVersion_AgainstDriver(PVRSRV_RGXDEV_INFO *psDevInfo,
									RGXFWIF_OSINIT *psFwOsInit)
{
#if defined(PDUMP)||(!defined(NO_HARDWARE))
	PVRSRV_ERROR		eError;
#endif

#if defined(PDUMP)
	PDUMP_FLAGS_T ui32PDumpFlags = PDUMP_FLAGS_CONTINUOUS;
#endif

	IMG_UINT32	ui32FWCoreIDValue = 0;
	IMG_CHAR *pcRGXFW_PROCESSOR = NULL;
	if (RGX_IS_FEATURE_VALUE_SUPPORTED(psDevInfo, META))
	{
		switch (RGX_GET_FEATURE_VALUE(psDevInfo, META))
		{
		case MTP218: ui32FWCoreIDValue = RGX_CR_META_MTP218_CORE_ID_VALUE; break;
		case MTP219: ui32FWCoreIDValue = RGX_CR_META_MTP219_CORE_ID_VALUE; break;
		case LTP218: ui32FWCoreIDValue = RGX_CR_META_LTP218_CORE_ID_VALUE; break;
		case LTP217: ui32FWCoreIDValue = RGX_CR_META_LTP217_CORE_ID_VALUE; break;
		default:
			PVR_DPF((PVR_DBG_ERROR, "%s: Undefined FW_CORE_ID_VALUE", __func__));
			PVR_ASSERT(0);
		}
		pcRGXFW_PROCESSOR = RGXFW_PROCESSOR_META;
	}else
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Undefined FW_CORE_ID_VALUE", __func__));
		PVR_ASSERT(0);
	}

#if defined(PDUMP)
	PDUMPIF("DISABLE_HWMETA_CHECK", ui32PDumpFlags);
	PDUMPELSE("DISABLE_HWMETA_CHECK", ui32PDumpFlags);
	PDUMPCOMMENTWITHFLAGS(ui32PDumpFlags, "Compatibility check: KM driver and HW FW Processor version");
	eError = DevmemPDumpDevmemCheck32(psDevInfo->psRGXFWIfOsInitMemDesc,
					offsetof(RGXFWIF_OSINIT, sRGXCompChecks) +
					offsetof(RGXFWIF_COMPCHECKS, ui32FWProcessorVersion),
					ui32FWCoreIDValue,
					0xffffffff,
					PDUMP_POLL_OPERATOR_EQUAL,
					ui32PDumpFlags);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfOsInitMemDesc (%d)", eError));
		return eError;
	}
	PDUMPFI("DISABLE_HWMETA_CHECK", ui32PDumpFlags);
#endif

#if !defined(NO_HARDWARE)
	if (psFwOsInit == NULL)
		return PVRSRV_ERROR_INVALID_PARAMS;

	if (psFwOsInit->sRGXCompChecks.ui32FWProcessorVersion != ui32FWCoreIDValue)
	{
		PVR_LOG(("RGXDevInitCompatCheck: Incompatible driver %s version (%d) / HW %s version (%d).",
				 RGXFW_PROCESSOR_META,
				 ui32FWCoreIDValue,
				 RGXFW_PROCESSOR_META,
				 psFwOsInit->sRGXCompChecks.ui32FWProcessorVersion));
		eError = PVRSRV_ERROR_FWPROCESSOR_MISMATCH;
		PVR_DBG_BREAK;
		return eError;
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE, "RGXDevInitCompatCheck: Compatible driver %s version (%d) / HW %s version (%d) [OK].",
				 RGXFW_PROCESSOR_META,
				 ui32FWCoreIDValue,
				 RGXFW_PROCESSOR_META,
				 psFwOsInit->sRGXCompChecks.ui32FWProcessorVersion));
	}
#endif
	return PVRSRV_OK;
}

/*!
*******************************************************************************
*******************************************************************************

 @Function	RGXDevInitCompatCheck

 @Description

 Check compatibility of host driver and firmware (DDK and build options)
 for RGX devices at services/device initialisation

 @Input psDeviceNode - device node

 @Return   PVRSRV_ERROR - depending on mismatch found

******************************************************************************/
static PVRSRV_ERROR RGXDevInitCompatCheck(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_ERROR		eError;
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
#if !defined(NO_HARDWARE)
	IMG_UINT32			ui32RegValue;
	IMG_UINT8			ui8FwOsCount;

	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{
		if (*((volatile IMG_BOOL *)&psDevInfo->psRGXFWIfOsInit->sRGXCompChecks.bUpdated))
		{
			/* No need to wait if the FW has already updated the values */
			break;
		}
		OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
	} END_LOOP_UNTIL_TIMEOUT();

	ui32RegValue = 0;

	if ((!PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST)) &&
		RGX_IS_FEATURE_VALUE_SUPPORTED(psDevInfo, META))
	{
		eError = RGXReadMETAAddr(psDevInfo, META_CR_T0ENABLE_OFFSET, &ui32RegValue);

		if (eError != PVRSRV_OK)
		{
			PVR_LOG(("%s: Reading RGX META register failed. Is the GPU correctly powered up? (%u)",
					__func__, eError));
			goto chk_exit;
		}

		if (!(ui32RegValue & META_CR_TXENABLE_ENABLE_BIT))
		{
			eError = PVRSRV_ERROR_META_THREAD0_NOT_ENABLED;
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: RGX META is not running. Is the GPU correctly powered up? %d (%u)",
					__func__, psDevInfo->psRGXFWIfOsInit->sRGXCompChecks.bUpdated, eError));
			goto chk_exit;
		}
	}

	if (!*((volatile IMG_BOOL *)&psDevInfo->psRGXFWIfOsInit->sRGXCompChecks.bUpdated))
	{
		eError = PVRSRV_ERROR_TIMEOUT;
		PVR_DPF((PVR_DBG_ERROR, "%s: GPU Firmware not responding: failed to supply compatibility info (%u)",
				__func__, eError));
		goto chk_exit;
	}

	ui8FwOsCount = psDevInfo->psRGXFWIfOsInit->sRGXCompChecks.sInitOptions.ui8OsCountSupport;
	if ((PVRSRV_VZ_MODE_IS(DRIVER_MODE_NATIVE) && (ui8FwOsCount > 1)) ||
		(PVRSRV_VZ_MODE_IS(DRIVER_MODE_HOST) && (ui8FwOsCount != RGX_NUM_OS_SUPPORTED)))
	{
		PVR_DPF((PVR_DBG_WARNING, "%s: Mismatch between the number of Operating Systems supported by KM driver (%d) and FW (%d)",
				__func__, (PVRSRV_VZ_MODE_IS(DRIVER_MODE_NATIVE)) ? (1) : (RGX_NUM_OS_SUPPORTED), ui8FwOsCount));
	}
#endif /* defined(NO_HARDWARE) */

	eError = RGXDevInitCompatCheck_KMBuildOptions_FWAgainstDriver(psDevInfo->psRGXFWIfOsInit);
	if (eError != PVRSRV_OK)
	{
		goto chk_exit;
	}

	eError = RGXDevInitCompatCheck_DDKVersion_FWAgainstDriver(psDevInfo, psDevInfo->psRGXFWIfOsInit);
	if (eError != PVRSRV_OK)
	{
		goto chk_exit;
	}

	eError = RGXDevInitCompatCheck_DDKBuild_FWAgainstDriver(psDevInfo, psDevInfo->psRGXFWIfOsInit);
	if (eError != PVRSRV_OK)
	{
		goto chk_exit;
	}

	if (!PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		eError = RGXDevInitCompatCheck_BVNC_FWAgainstDriver(psDevInfo, psDevInfo->psRGXFWIfOsInit);
		if (eError != PVRSRV_OK)
		{
			goto chk_exit;
		}

		eError = RGXDevInitCompatCheck_BVNC_HWAgainstDriver(psDevInfo, psDevInfo->psRGXFWIfOsInit);
		if (eError != PVRSRV_OK)
		{
			goto chk_exit;
		}
	}

	eError = RGXDevInitCompatCheck_FWProcessorVersion_AgainstDriver(psDevInfo, psDevInfo->psRGXFWIfOsInit);
	if (eError != PVRSRV_OK)
	{
		goto chk_exit;
	}

	eError = PVRSRV_OK;
chk_exit:

	return eError;
}

/**************************************************************************/ /*!
@Function       RGXSoftReset
@Description    Resets some modules of the RGX device
@Input          psDeviceNode		Device node
@Input          ui64ResetValue  A mask for which each bit set corresponds
                                to a module to reset (via the SOFT_RESET
                                register).
@Input          ui64SPUResetValue A mask for which each bit set corresponds
                                to a module to reset (via the SOFT_RESET_SPU
                                register).
@Return         PVRSRV_ERROR
*/ /***************************************************************************/
static PVRSRV_ERROR RGXSoftReset(PVRSRV_DEVICE_NODE *psDeviceNode,
                                 IMG_UINT64  ui64ResetValue,
                                 IMG_UINT64  ui64SPUResetValue)
{
	PVRSRV_RGXDEV_INFO        *psDevInfo;

	PVR_ASSERT(psDeviceNode != NULL);
	PVR_ASSERT(psDeviceNode->pvDevice != NULL);
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_GUEST, PVRSRV_OK);

	if (((ui64ResetValue & RGX_CR_SOFT_RESET_MASKFULL) != ui64ResetValue)
		|| (ui64SPUResetValue & RGX_CR_SOFT_RESET_SPU_MASKFULL) != ui64SPUResetValue)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/* The device info */
	psDevInfo = psDeviceNode->pvDevice;

	/* Set in soft-reset */
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, ui64ResetValue);
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET_SPU, ui64SPUResetValue);

	/* Read soft-reset to fence previous write in order to clear the SOCIF pipeline */
	(void) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET);
	(void) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET_SPU);

	/* Take the modules out of reset... */
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, 0);
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET_SPU, 0);

	/* ...and fence again */
	(void) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET);
	(void) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET_SPU);

	return PVRSRV_OK;
}

PVRSRV_ERROR RGXInitAllocFWImgMem(PVRSRV_DEVICE_NODE   *psDeviceNode,
                                  IMG_DEVMEM_SIZE_T    uiFWCodeLen,
                                  IMG_DEVMEM_SIZE_T    uiFWDataLen,
                                  IMG_DEVMEM_SIZE_T    uiFWCorememCodeLen,
                                  IMG_DEVMEM_SIZE_T    uiFWCorememDataLen)
{
	DEVMEM_FLAGS_T		uiMemAllocFlags;
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_ERROR        eError;

	/*
	 * Set up Allocation for FW code section
	 */
	uiMemAllocFlags = PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(PMMETA_PROTECT) |
	                  PVRSRV_MEMALLOCFLAG_GPU_READABLE |
	                  PVRSRV_MEMALLOCFLAG_GPU_WRITEABLE |
	                  PVRSRV_MEMALLOCFLAG_CPU_READABLE |
	                  PVRSRV_MEMALLOCFLAG_CPU_WRITEABLE |
	                  PVRSRV_MEMALLOCFLAG_GPU_CACHE_INCOHERENT |
	                  PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(FIRMWARE_CACHED) |
	                  PVRSRV_MEMALLOCFLAG_KERNEL_CPU_MAPPABLE;

	eError = RGXAllocateFWMemoryRegion(psDeviceNode,
	                                   uiFWCodeLen,
	                                   uiMemAllocFlags,
	                                   PVRSRV_DEVICE_FW_CODE_REGION,
	                                   "FwCodeRegion",
	                                   &psDevInfo->psRGXFWCodeMemDesc);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "Failed to allocate fw code mem (%u)",
		         eError));
		goto failFWCodeMemDescAlloc;
	}

	eError = DevmemAcquireDevVirtAddr(psDevInfo->psRGXFWCodeMemDesc,
	                                  &psDevInfo->sFWCodeDevVAddrBase);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "Failed to acquire devVAddr for fw code mem (%u)",
		         eError));
		goto failFWCodeMemDescAqDevVirt;
	}

	if (!RGX_IS_FEATURE_VALUE_SUPPORTED(psDevInfo, META))
	{
		/*
		* The FW code must be the first allocation in the firmware heap, otherwise
		* the bootloader will not work (META will not be able to find the bootloader).
		*/
		PVR_ASSERT(psDevInfo->sFWCodeDevVAddrBase.uiAddr == RGX_FIRMWARE_HOST_MAIN_HEAP_BASE);
	}

	/*
	 * Set up Allocation for FW data section
	 */
	uiMemAllocFlags = PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(PMMETA_PROTECT) |
	                  PVRSRV_MEMALLOCFLAG_GPU_READABLE |
	                  PVRSRV_MEMALLOCFLAG_GPU_WRITEABLE |
	                  PVRSRV_MEMALLOCFLAG_CPU_READABLE |
	                  PVRSRV_MEMALLOCFLAG_CPU_WRITEABLE |
	                  PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(FIRMWARE_CACHED) |
	                  PVRSRV_MEMALLOCFLAG_GPU_CACHE_INCOHERENT |
	                  PVRSRV_MEMALLOCFLAG_KERNEL_CPU_MAPPABLE |
	                  PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE |
	                  PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC;

	eError = RGXAllocateFWMemoryRegion(psDeviceNode,
	                                   uiFWDataLen,
	                                   uiMemAllocFlags,
	                                   PVRSRV_DEVICE_FW_PRIVATE_DATA_REGION,
	                                   "FwDataRegion",
	                                   &psDevInfo->psRGXFWDataMemDesc);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "Failed to allocate fw data mem (%u)",
		         eError));
		goto failFWDataMemDescAlloc;
	}

	eError = DevmemAcquireDevVirtAddr(psDevInfo->psRGXFWDataMemDesc,
	                                  &psDevInfo->sFWDataDevVAddrBase);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "Failed to acquire devVAddr for fw data mem (%u)",
		         eError));
		goto failFWDataMemDescAqDevVirt;
	}

	if (uiFWCorememCodeLen != 0)
	{
		/*
		 * Set up Allocation for FW coremem code section
		 */
		uiMemAllocFlags = PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(PMMETA_PROTECT) |
			PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(FIRMWARE_CACHED) |
			PVRSRV_MEMALLOCFLAG_GPU_READABLE |
			PVRSRV_MEMALLOCFLAG_CPU_READABLE |
			PVRSRV_MEMALLOCFLAG_CPU_WRITEABLE |
			PVRSRV_MEMALLOCFLAG_GPU_CACHE_INCOHERENT |
			PVRSRV_MEMALLOCFLAG_KERNEL_CPU_MAPPABLE |
			PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE |
			PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC;

		eError = RGXAllocateFWMemoryRegion(psDeviceNode,
		                                   uiFWCorememCodeLen,
		                                   uiMemAllocFlags,
		                                   PVRSRV_DEVICE_FW_COREMEM_CODE_REGION,
		                                   "FwCorememCodeRegion",
		                                   &psDevInfo->psRGXFWCorememCodeMemDesc);

		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
			         "Failed to allocate fw coremem code mem, size: %"  IMG_INT64_FMTSPECd ", flags: %" PVRSRV_MEMALLOCFLAGS_FMTSPEC " (%u)",
			         uiFWCorememCodeLen, uiMemAllocFlags, eError));
			goto failFWCorememMemDescAlloc;
		}

		eError = DevmemAcquireDevVirtAddr(psDevInfo->psRGXFWCorememCodeMemDesc,
		                                  &psDevInfo->sFWCorememCodeDevVAddrBase);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
			         "Failed to acquire devVAddr for fw coremem mem code (%u)",
			         eError));
			goto failFWCorememCodeMemDescAqDevVirt;
		}

		RGXSetFirmwareAddress(&psDevInfo->sFWCorememCodeFWAddr,
		                      psDevInfo->psRGXFWCorememCodeMemDesc,
		                      0, RFW_FWADDR_NOREF_FLAG);
	}
	else
	{
		psDevInfo->sFWCorememCodeDevVAddrBase.uiAddr = 0;
		psDevInfo->sFWCorememCodeFWAddr.ui32Addr = 0;
	}

	if (uiFWCorememDataLen != 0)
	{
		/*
		 * Set up Allocation for FW coremem data section
		 */
		uiMemAllocFlags = PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(PMMETA_PROTECT) |
				PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(FIRMWARE_CACHED) |
				PVRSRV_MEMALLOCFLAG_GPU_READABLE  |
				PVRSRV_MEMALLOCFLAG_GPU_WRITEABLE |
				PVRSRV_MEMALLOCFLAG_CPU_READABLE  |
				PVRSRV_MEMALLOCFLAG_CPU_WRITEABLE |
				PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC |
				PVRSRV_MEMALLOCFLAG_GPU_CACHE_INCOHERENT;

		eError = RGXAllocateFWMemoryRegion(psDeviceNode,
				uiFWCorememDataLen,
				uiMemAllocFlags,
				PVRSRV_DEVICE_FW_COREMEM_DATA_REGION,
				"FwCorememDataRegion",
				&psDevInfo->psRGXFWIfCorememDataStoreMemDesc);

		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
			         "Failed to allocate fw coremem data mem, "
			         "size: %"  IMG_INT64_FMTSPECd ", flags: %" PVRSRV_MEMALLOCFLAGS_FMTSPEC " (%u)",
			         uiFWCorememDataLen,
			         uiMemAllocFlags,
			         eError));
			goto failFWCorememDataMemDescAlloc;
		}

		eError = DevmemAcquireDevVirtAddr(psDevInfo->psRGXFWIfCorememDataStoreMemDesc,
				&psDevInfo->sFWCorememDataStoreDevVAddrBase);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
			         "Failed to acquire devVAddr for fw coremem mem data (%u)",
			         eError));
			goto failFWCorememDataMemDescAqDevVirt;
		}

		RGXSetFirmwareAddress(&psDevInfo->sFWCorememDataStoreFWAddr,
				psDevInfo->psRGXFWIfCorememDataStoreMemDesc,
				0, RFW_FWADDR_NOREF_FLAG);
	}
	else
	{
		psDevInfo->sFWCorememDataStoreDevVAddrBase.uiAddr = 0;
		psDevInfo->sFWCorememDataStoreFWAddr.ui32Addr = 0;
	}

	return PVRSRV_OK;

failFWCorememDataMemDescAqDevVirt:
	if (uiFWCorememDataLen != 0)
	{
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWIfCorememDataStoreMemDesc);
		psDevInfo->psRGXFWIfCorememDataStoreMemDesc = NULL;
	}
failFWCorememDataMemDescAlloc:
	if (uiFWCorememCodeLen != 0)
	{
		DevmemReleaseDevVirtAddr(psDevInfo->psRGXFWCorememCodeMemDesc);
	}
failFWCorememCodeMemDescAqDevVirt:
	if (uiFWCorememCodeLen != 0)
	{
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWCorememCodeMemDesc);
		psDevInfo->psRGXFWCorememCodeMemDesc = NULL;
	}
failFWCorememMemDescAlloc:
	DevmemReleaseDevVirtAddr(psDevInfo->psRGXFWDataMemDesc);
failFWDataMemDescAqDevVirt:
	DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWDataMemDesc);
	psDevInfo->psRGXFWDataMemDesc = NULL;
failFWDataMemDescAlloc:
	DevmemReleaseDevVirtAddr(psDevInfo->psRGXFWCodeMemDesc);
failFWCodeMemDescAqDevVirt:
	DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWCodeMemDesc);
	psDevInfo->psRGXFWCodeMemDesc = NULL;
failFWCodeMemDescAlloc:
	return eError;
}

/*
	AppHint parameter interface
*/
static
PVRSRV_ERROR RGXFWTraceQueryFilter(const PVRSRV_DEVICE_NODE *psDeviceNode,
                                   const void *psPrivate,
                                   IMG_UINT32 *pui32Value)
{
	PVRSRV_ERROR eResult;

	eResult = PVRSRVRGXFWDebugQueryFWLogKM(NULL, psDeviceNode, pui32Value);
	*pui32Value &= RGXFWIF_LOG_TYPE_GROUP_MASK;
	return eResult;
}

static
PVRSRV_ERROR RGXFWTraceQueryLogType(const PVRSRV_DEVICE_NODE *psDeviceNode,
                                   const void *psPrivate,
                                   IMG_UINT32 *pui32Value)
{
	PVRSRV_ERROR eResult;

	eResult = PVRSRVRGXFWDebugQueryFWLogKM(NULL, psDeviceNode, pui32Value);
	if (PVRSRV_OK == eResult)
	{
		if (*pui32Value & RGXFWIF_LOG_TYPE_TRACE)
		{
			*pui32Value = 0; /* Trace */
		}
		else
		{
			*pui32Value = 1; /* TBI */
		}
	}
	return eResult;
}

static
PVRSRV_ERROR RGXFWTraceSetFilter(const PVRSRV_DEVICE_NODE *psDeviceNode,
                                  const void *psPrivate,
                                  IMG_UINT32 ui32Value)
{
	PVRSRV_ERROR eResult;
	IMG_UINT32 ui32RGXFWLogType;

	eResult = RGXFWTraceQueryLogType(psDeviceNode, NULL, &ui32RGXFWLogType);
	if (PVRSRV_OK == eResult)
	{
		if (0 == ui32RGXFWLogType)
		{
			BITMASK_SET(ui32Value, RGXFWIF_LOG_TYPE_TRACE);
		}
		eResult = PVRSRVRGXFWDebugSetFWLogKM(NULL, psDeviceNode, ui32Value);
	}
	return eResult;
}

static
PVRSRV_ERROR RGXFWTraceSetLogType(const PVRSRV_DEVICE_NODE *psDeviceNode,
                                    const void *psPrivate,
                                    IMG_UINT32 ui32Value)
{
	PVRSRV_ERROR eResult;
	IMG_UINT32 ui32RGXFWLogType = ui32Value;

	eResult = RGXFWTraceQueryFilter(psDeviceNode, NULL, &ui32RGXFWLogType);
	if (PVRSRV_OK != eResult)
	{
		return eResult;
	}

	/* 0 - trace, 1 - tbi */
	if (0 == ui32Value)
	{
		BITMASK_SET(ui32RGXFWLogType, RGXFWIF_LOG_TYPE_TRACE);
	}
#if defined(SUPPORT_TBI_INTERFACE)
	else if (1 == ui32Value)
	{
		BITMASK_UNSET(ui32RGXFWLogType, RGXFWIF_LOG_TYPE_TRACE);
	}
#endif
	else
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Invalid parameter %u specified to set FW log type AppHint.",
		         __func__, ui32Value));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	eResult = PVRSRVRGXFWDebugSetFWLogKM(NULL, psDeviceNode, ui32RGXFWLogType);
	return eResult;
}

static
PVRSRV_ERROR RGXQueryFWPoisonOnFree(const PVRSRV_DEVICE_NODE *psDeviceNode,
									const void *psPrivate,
									IMG_BOOL *pbValue)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = (PVRSRV_RGXDEV_INFO *) psDeviceNode->pvDevice;

	*pbValue = (PVRSRV_MEMALLOCFLAG_POISON_ON_FREE == psDevInfo->ui32FWPoisonOnFreeFlag)
		? IMG_TRUE
		: IMG_FALSE;
	return PVRSRV_OK;
}

static
PVRSRV_ERROR RGXSetFWPoisonOnFree(const PVRSRV_DEVICE_NODE *psDeviceNode,
									const void *psPrivate,
									IMG_BOOL bValue)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = (PVRSRV_RGXDEV_INFO *) psDeviceNode->pvDevice;
	psDevInfo->ui32FWPoisonOnFreeFlag = bValue
			? PVRSRV_MEMALLOCFLAG_POISON_ON_FREE
			: 0UL;

	return PVRSRV_OK;
}

/*
 * RGXInitFirmware
 */
PVRSRV_ERROR
RGXInitFirmware(PVRSRV_DEVICE_NODE       *psDeviceNode,
                IMG_BOOL                 bEnableSignatureChecks,
                IMG_UINT32               ui32SignatureChecksBufSize,
                IMG_UINT32               ui32HWPerfFWBufSizeKB,
                IMG_UINT64               ui64HWPerfFilter,
                IMG_UINT32               ui32RGXFWAlignChecksArrLength,
                IMG_UINT32               *pui32RGXFWAlignChecks,
                IMG_UINT32               ui32ConfigFlags,
                IMG_UINT32               ui32LogType,
                IMG_UINT32               ui32FilterFlags,
                IMG_UINT32               ui32JonesDisableMask,
                IMG_UINT32               ui32HWRDebugDumpLimit,
                IMG_UINT32				 ui32KillingCtl,
                IMG_UINT32				 *pui32TPUTrilinearFracMask,
                IMG_UINT32				 *pui32USRMNumRegions,
                IMG_UINT64				 *pui64UVBRMNumRegions,
                IMG_UINT32               ui32HWPerfCountersDataSize,
                RGX_RD_POWER_ISLAND_CONF eRGXRDPowerIslandingConf,
                FW_PERF_CONF             eFirmwarePerf,
                IMG_UINT32               ui32ConfigFlagsExt,
                IMG_UINT32               ui32AvailableSPUMask,
				IMG_UINT32               ui32FwOsCfgFlags)
{
	PVRSRV_ERROR eError;
	PVRSRV_RGXDEV_INFO *psDevInfo = (PVRSRV_RGXDEV_INFO *)psDeviceNode->pvDevice;
	IMG_BOOL bEnableFWPoisonOnFree = IMG_FALSE;

	eError = RGXSetupFirmware(psDeviceNode,
	                          bEnableSignatureChecks,
	                          ui32SignatureChecksBufSize,
	                          ui32HWPerfFWBufSizeKB,
	                          ui64HWPerfFilter,
	                          ui32RGXFWAlignChecksArrLength,
	                          pui32RGXFWAlignChecks,
	                          ui32ConfigFlags,
	                          ui32ConfigFlagsExt,
	                          ui32FwOsCfgFlags,
	                          ui32LogType,
	                          ui32FilterFlags,
	                          ui32JonesDisableMask,
	                          ui32HWRDebugDumpLimit,
	                          ui32HWPerfCountersDataSize,
	                          ui32KillingCtl,
	                          pui32TPUTrilinearFracMask,
	                          pui32USRMNumRegions,
	                          pui64UVBRMNumRegions,
	                          eRGXRDPowerIslandingConf,
	                          eFirmwarePerf,
	                          ui32AvailableSPUMask);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "PVRSRVRGXInitFirmwareKM: RGXSetupFirmware failed (%u)",
		         eError));
		goto failed_init_firmware;
	}
	PVRSRVAppHintRegisterHandlersUINT32(APPHINT_ID_EnableLogGroup,
	                                    RGXFWTraceQueryFilter,
	                                    RGXFWTraceSetFilter,
	                                    psDeviceNode,
	                                    NULL);
	PVRSRVAppHintRegisterHandlersUINT32(APPHINT_ID_FirmwareLogType,
	                                    RGXFWTraceQueryLogType,
	                                    RGXFWTraceSetLogType,
	                                    psDeviceNode,
	                                    NULL);

	bEnableFWPoisonOnFree = PVRSRV_APPHINT_ENABLEFWPOISONONFREE;

	PVRSRVAppHintRegisterHandlersBOOL(APPHINT_ID_EnableFWPoisonOnFree,
	                                   RGXQueryFWPoisonOnFree,
	                                   RGXSetFWPoisonOnFree,
	                                   psDeviceNode,
	                                   NULL);

	psDevInfo->ui32FWPoisonOnFreeFlag = bEnableFWPoisonOnFree
			? PVRSRV_MEMALLOCFLAG_POISON_ON_FREE
			: 0UL;

	return PVRSRV_OK;

failed_init_firmware:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

/* See device.h for function declaration */
static PVRSRV_ERROR RGXAllocUFOBlock(PVRSRV_DEVICE_NODE *psDeviceNode,
									 DEVMEM_MEMDESC **psMemDesc,
									 IMG_UINT32 *puiSyncPrimVAddr,
									 IMG_UINT32 *puiSyncPrimBlockSize)
{
	PVRSRV_RGXDEV_INFO *psDevInfo;
	PVRSRV_ERROR eError;
	RGXFWIF_DEV_VIRTADDR pFirmwareAddr;
	IMG_DEVMEM_SIZE_T uiUFOBlockSize = sizeof(IMG_UINT32);
	IMG_DEVMEM_ALIGN_T ui32UFOBlockAlign = sizeof(IMG_UINT32);
	IMG_UINT32 ui32CoherencyFlag = 0;

	psDevInfo = psDeviceNode->pvDevice;

	/* Size and align are 'expanded' because we request an Exportalign allocation */
	eError = DevmemExportalignAdjustSizeAndAlign(DevmemGetHeapLog2PageSize(psDevInfo->psFirmwareMainHeap),
										&uiUFOBlockSize,
										&ui32UFOBlockAlign);

	if (eError != PVRSRV_OK)
	{
		goto e0;
	}

	if (psDeviceNode->pfnHasSnoopingOfDeviceCache(psDeviceNode) &&
		psDeviceNode->pfnHasSnoopingOfCPUCache(psDeviceNode))
	{
		ui32CoherencyFlag = PVRSRV_MEMALLOCFLAG_CACHE_COHERENT;
	}
	else
	{
		ui32CoherencyFlag = PVRSRV_MEMALLOCFLAG_UNCACHED;
	}

	eError = DevmemFwAllocateExportable(psDeviceNode,
										uiUFOBlockSize,
										ui32UFOBlockAlign,
										PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(PMMETA_PROTECT) |
										PVRSRV_MEMALLOCFLAG_KERNEL_CPU_MAPPABLE |
										PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC |
										PVRSRV_MEMALLOCFLAG_GPU_READABLE |
										PVRSRV_MEMALLOCFLAG_GPU_WRITEABLE |
										PVRSRV_MEMALLOCFLAG_CPU_READABLE |
										PVRSRV_MEMALLOCFLAG_CPU_WRITEABLE |
										ui32CoherencyFlag,
										"FwExUFOBlock",
										psMemDesc);
	if (eError != PVRSRV_OK)
	{
		goto e0;
	}

	RGXSetFirmwareAddress(&pFirmwareAddr, *psMemDesc, 0, RFW_FWADDR_FLAG_NONE);
	*puiSyncPrimVAddr = pFirmwareAddr.ui32Addr;
	*puiSyncPrimBlockSize = TRUNCATE_64BITS_TO_32BITS(uiUFOBlockSize);

	return PVRSRV_OK;

e0:
	return eError;
}

/* See device.h for function declaration */
static void RGXFreeUFOBlock(PVRSRV_DEVICE_NODE *psDeviceNode,
							DEVMEM_MEMDESC *psMemDesc)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

	/*
		If the system has snooping of the device cache then the UFO block
		might be in the cache so we need to flush it out before freeing
		the memory

		When the device is being shutdown/destroyed we don't care anymore.
		Several necessary data structures to issue a flush were destroyed
		already.
	*/
	if (RGXSystemSnoopingOfDeviceCache(psDeviceNode) &&
		psDeviceNode->eDevState != PVRSRV_DEVICE_STATE_DEINIT)
	{
		RGXFWIF_KCCB_CMD sFlushInvalCmd;
		PVRSRV_ERROR eError;
		IMG_UINT32 ui32kCCBCommandSlot;

		/* Schedule the SLC flush command ... */
#if defined(PDUMP)
		PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "Submit SLC flush and invalidate");
#endif
		sFlushInvalCmd.eCmdType = RGXFWIF_KCCB_CMD_SLCFLUSHINVAL;
		sFlushInvalCmd.uCmdData.sSLCFlushInvalData.ui64Size = 0;
		sFlushInvalCmd.uCmdData.sSLCFlushInvalData.ui64Address = 0;
		sFlushInvalCmd.uCmdData.sSLCFlushInvalData.bInval = IMG_TRUE;
		sFlushInvalCmd.uCmdData.sSLCFlushInvalData.bDMContext = IMG_FALSE;
		sFlushInvalCmd.uCmdData.sSLCFlushInvalData.psContext.ui32Addr = 0;

		eError = RGXSendCommandWithPowLockAndGetKCCBSlot(psDevInfo,
														 &sFlushInvalCmd,
														 PDUMP_FLAGS_CONTINUOUS,
														 &ui32kCCBCommandSlot);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
			         "%s: Failed to schedule SLC flush command with error (%u)",
			         __func__,
			         eError));
		}
		else
		{
			/* Wait for the SLC flush to complete */
			eError = RGXWaitForKCCBSlotUpdate(psDevInfo, ui32kCCBCommandSlot, PDUMP_FLAGS_CONTINUOUS);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,
				         "%s: SLC flush and invalidate aborted with error (%u)",
				         __func__,
				         eError));
			}
			else if (unlikely(psDevInfo->pui32KernelCCBRtnSlots[ui32kCCBCommandSlot] &
							  RGXFWIF_KCCB_RTN_SLOT_POLL_FAILURE))
			{
				PVR_DPF((PVR_DBG_WARNING, "%s: FW poll on a HW operation failed", __func__));
			}
		}
	}

	RGXUnsetFirmwareAddress(psMemDesc);
	DevmemFwUnmapAndFree(psDevInfo, psMemDesc);
}

/*
	DevDeInitRGX
*/
PVRSRV_ERROR DevDeInitRGX(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_RGXDEV_INFO		*psDevInfo = (PVRSRV_RGXDEV_INFO*)psDeviceNode->pvDevice;
	PVRSRV_ERROR			eError;
	DEVICE_MEMORY_INFO		*psDevMemoryInfo;
	IMG_UINT32		ui32Temp=0;

	if (!psDevInfo)
	{
		/* Can happen if DevInitRGX failed */
		PVR_DPF((PVR_DBG_ERROR, "DevDeInitRGX: Null DevInfo"));
		return PVRSRV_OK;
	}

	eError = DeviceDepBridgeDeInit(psDevInfo->sDevFeatureCfg.ui64Features);
	PVR_LOG_IF_ERROR(eError, "DeviceDepBridgeDeInit");

#if defined(PVRSRV_FORCE_UNLOAD_IF_BAD_STATE)
	if (PVRSRVGetPVRSRVData()->eServicesState != PVRSRV_SERVICES_STATE_OK)
	{
		OSAtomicWrite(&psDeviceNode->sDummyPage.atRefCounter, 0);
		PVR_UNREFERENCED_PARAMETER(ui32Temp);
	}
	else
#else
	{
		/*Delete the Dummy page related info */
		ui32Temp = (IMG_UINT32)OSAtomicRead(&psDeviceNode->sDummyPage.atRefCounter);
		if (0 != ui32Temp)
		{
			PVR_DPF((PVR_DBG_ERROR,
			         "%s: Dummy page reference counter is non zero (%u)",
			         __func__,
			         ui32Temp));
			PVR_ASSERT(0);
		}
	}
#endif

	/*Delete the Dummy page related info */
	ui32Temp = (IMG_UINT32)OSAtomicRead(&psDeviceNode->sDevZeroPage.atRefCounter);
	if (0 != ui32Temp)
	{
		PVR_DPF((PVR_DBG_ERROR,
			     "%s: Zero page reference counter is non zero (%u)",
			     __func__,
			     ui32Temp));
	}

#if defined(PDUMP)
	if (NULL != psDeviceNode->sDummyPage.hPdumpPg)
	{
		PDUMPCOMMENT("Error dummy page handle is still active");
	}

	if (NULL != psDeviceNode->sDevZeroPage.hPdumpPg)
	{
		PDUMPCOMMENT("Error Zero page handle is still active");
	}
#endif

	/*The lock type need to be dispatch type here because it can be acquired from MISR (Z-buffer) path */
	OSLockDestroy(psDeviceNode->sDummyPage.psPgLock);

	/* Destroy the zero page lock */
	OSLockDestroy(psDeviceNode->sDevZeroPage.psPgLock);

	/* Unregister debug request notifiers first as they could depend on anything. */

	RGXDebugDeinit(psDevInfo);


	/* Cancel notifications to this device */
	PVRSRVUnregisterCmdCompleteNotify(psDeviceNode->hCmdCompNotify);
	psDeviceNode->hCmdCompNotify = NULL;

	/*
	 * De-initialise in reverse order, so stage 2 init is undone first.
	 */
	if (psDevInfo->bDevInit2Done)
	{
		psDevInfo->bDevInit2Done = IMG_FALSE;

#if !defined(NO_HARDWARE)
		(void) SysUninstallDeviceLISR(psDevInfo->pvLISRData);
		(void) OSUninstallMISR(psDevInfo->pvMISRData);
		(void) OSUninstallMISR(psDevInfo->hProcessQueuesMISR);
		if (psDevInfo->pvAPMISRData != NULL)
		{
			(void) OSUninstallMISR(psDevInfo->pvAPMISRData);
		}
#if defined(PVR_SUPPORT_HMMU_VALIDATION)
		if (psDevInfo->pvHMMUMISRData != NULL)
		{
			(void) OSUninstallMISR(psDevInfo->pvHMMUMISRData);
		}
#endif

#endif /* !NO_HARDWARE */

		/* Remove the device from the power manager */
		eError = PVRSRVRemovePowerDevice(psDeviceNode);
		if (eError != PVRSRV_OK)
		{
			return eError;
		}

		psDevInfo->pfnGetGpuUtilStats = NULL;
		OSLockDestroy(psDevInfo->hGPUUtilLock);

		/* Free DVFS Table */
		if (psDevInfo->psGpuDVFSTable != NULL)
		{
			OSFreeMem(psDevInfo->psGpuDVFSTable);
			psDevInfo->psGpuDVFSTable = NULL;
		}

		/* De-init Freelists/ZBuffers... */
		OSLockDestroy(psDevInfo->hLockFreeList);
		OSLockDestroy(psDevInfo->hLockZSBuffer);

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
		/* De-init work estimation lock */
		OSLockDestroy(psDevInfo->hWorkEstLock);
#endif

		/* Unregister MMU related stuff */
		eError = RGXMMUInit_Unregister(psDeviceNode);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
			         "DevDeInitRGX: Failed RGXMMUInit_Unregister (0x%x)",
			         eError));
			return eError;
		}
	}

#if !defined(PVR_SUPPORT_HMMU_VALIDATION)
	/* UnMap Regs */
	if (psDevInfo->pvRegsBaseKM != NULL)
	{
#if !defined(NO_HARDWARE)
		OSUnMapPhysToLin((void __force *) psDevInfo->pvRegsBaseKM,
						 psDevInfo->ui32RegSize,
						 PVRSRV_MEMALLOCFLAG_CPU_UNCACHED);
#endif /* !NO_HARDWARE */
		psDevInfo->pvRegsBaseKM = NULL;
	}
#endif

#if 0 /* not required at this time */
	if (psDevInfo->hTimer)
	{
		eError = OSRemoveTimer(psDevInfo->hTimer);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
			         "DevDeInitRGX: Failed to remove timer"));
			return eError;
		}
		psDevInfo->hTimer = NULL;
	}
#endif

	psDevMemoryInfo = &psDeviceNode->sDevMemoryInfo;

	RGXDeInitHeaps(psDevMemoryInfo);

	if (psDevInfo->psRGXFWCodeMemDesc)
	{
		/* Free fw code */
		PDUMPCOMMENT("Freeing FW code memory");
		DevmemReleaseDevVirtAddr(psDevInfo->psRGXFWCodeMemDesc);
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWCodeMemDesc);
		psDevInfo->psRGXFWCodeMemDesc = NULL;
	}
	if (psDevInfo->psRGXFWDataMemDesc)
	{
		/* Free fw data */
		PDUMPCOMMENT("Freeing FW data memory");
		DevmemReleaseDevVirtAddr(psDevInfo->psRGXFWDataMemDesc);
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWDataMemDesc);
		psDevInfo->psRGXFWDataMemDesc = NULL;
	}
	if (psDevInfo->psRGXFWCorememCodeMemDesc)
	{
		/* Free fw core mem code */
		PDUMPCOMMENT("Freeing FW coremem code memory");
		DevmemReleaseDevVirtAddr(psDevInfo->psRGXFWCorememCodeMemDesc);
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWCorememCodeMemDesc);
		psDevInfo->psRGXFWCorememCodeMemDesc = NULL;
	}

	if (psDevInfo->psRGXFWIfCorememDataStoreMemDesc)
	{
		/* Free fw core mem data */
		PDUMPCOMMENT("Freeing FW coremem data store memory");
		DevmemReleaseDevVirtAddr(psDevInfo->psRGXFWIfCorememDataStoreMemDesc);
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWIfCorememDataStoreMemDesc);
		psDevInfo->psRGXFWIfCorememDataStoreMemDesc = NULL;
	}

	/*
	   Free the firmware allocations.
	 */
	RGXFreeFirmware(psDevInfo);
	RGXDeInitDestroyFWKernelMemoryContext(psDeviceNode);

#if defined(SUPPORT_VALIDATION)
	RGXSPUPowerDomainDeInitState(&psDevInfo->sSPUPowerDomainState);
#endif

	/* De-initialise non-device specific (TL) users of RGX device memory */
	RGXHWPerfHostDeInit(psDevInfo);
	eError = HTBDeInit();
	PVR_LOG_IF_ERROR(eError, "HTBDeInit");

	/* destroy the TDM shared mem lock */
	OSLockDestroy(psDevInfo->hTQSharedMemLock);

	/* destroy the stalled CCB locks */
	OSLockDestroy(psDevInfo->hCCBRecoveryLock);
	OSLockDestroy(psDevInfo->hCCBStallCheckLock);

#if defined(PVR_SUPPORT_HMMU_VALIDATION)
	/* Deinit HMMU interface */
	if (psDevInfo->pvHmmuIfPriv && PVR_HMMU_Deinit(psDeviceNode)) {
		PVR_DPF((PVR_HMMU_ERROR, "HMMU interface De-init failed"));
	}

	/* UnMap Regs */
	if (psDevInfo->pvRegsBaseKM != NULL)
	{
#if !defined(NO_HARDWARE)
		OSUnMapPhysToLin(psDevInfo->pvRegsBaseKM,
				 psDevInfo->ui32RegSize,
				 PVRSRV_MEMALLOCFLAG_CPU_UNCACHED);
#endif /* !NO_HARDWARE */
		psDevInfo->pvRegsBaseKM = NULL;
	}
#endif

	/* destroy the context list locks */
	OSLockDestroy(psDevInfo->sRegCongfig.hLock);
	OSLockDestroy(psDevInfo->hBPLock);
	OSLockDestroy(psDevInfo->hRGXFWIfBufInitLock);
	OSWRLockDestroy(psDevInfo->hRenderCtxListLock);
	OSWRLockDestroy(psDevInfo->hComputeCtxListLock);
	OSWRLockDestroy(psDevInfo->hTransferCtxListLock);
	OSWRLockDestroy(psDevInfo->hTDMCtxListLock);
	OSWRLockDestroy(psDevInfo->hKickSyncCtxListLock);
	OSWRLockDestroy(psDevInfo->hMemoryCtxListLock);
	OSLockDestroy(psDevInfo->hLockKCCBDeferredCommandsList);
	OSWRLockDestroy(psDevInfo->hCommonCtxtListLock);

	if (psDevInfo->hDebugFaultInfoLock != NULL)
	{
		OSLockDestroy(psDevInfo->hDebugFaultInfoLock);
	}

	if (GetInfoPageDebugFlagsKM() & DEBUG_FEATURE_PAGE_FAULT_DEBUG_ENABLED)
	{
		if (psDevInfo->hMMUCtxUnregLock != NULL)
		{
			OSLockDestroy(psDevInfo->hMMUCtxUnregLock);
		}
	}

	/* Free device BVNC string */
	if (NULL != psDevInfo->sDevFeatureCfg.pszBVNCString)
	{
		OSFreeMem(psDevInfo->sDevFeatureCfg.pszBVNCString);
	}

	/* DeAllocate devinfo */
	OSFreeMem(psDevInfo);

	psDeviceNode->pvDevice = NULL;

	return PVRSRV_OK;
}

#if defined(PDUMP)
static
PVRSRV_ERROR RGXResetPDump(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = (PVRSRV_RGXDEV_INFO *)(psDeviceNode->pvDevice);

	psDevInfo->bDumpedKCCBCtlAlready = IMG_FALSE;

	return PVRSRV_OK;
}
#endif /* PDUMP */

static INLINE DEVMEM_HEAP_BLUEPRINT _blueprint_init(IMG_CHAR *name,
		IMG_UINT64 heap_base,
		IMG_DEVMEM_SIZE_T heap_length,
		IMG_DEVMEM_SIZE_T heap_reserved_region_length,
		IMG_UINT32 log2_import_alignment)
{
	void *pvAppHintState = NULL;
	IMG_UINT32 ui32AppHintDefault = PVRSRV_APPHINT_GENERALNON4KHEAPPAGESIZE;
	IMG_UINT32 ui32GeneralNon4KHeapPageSize;
	IMG_UINT32	ui32OSLog2PageShift = OSGetPageShift();
	IMG_UINT32	ui32OSPageSize;

	DEVMEM_HEAP_BLUEPRINT b = {
		.pszName = name,
		.sHeapBaseAddr.uiAddr = heap_base,
		.uiHeapLength = heap_length,
		.uiReservedRegionLength = heap_reserved_region_length,
		.uiLog2DataPageSize = RGXHeapDerivePageSize(ui32OSLog2PageShift),
		.uiLog2ImportAlignment = log2_import_alignment,
	};

	ui32OSPageSize = (1 << ui32OSLog2PageShift);

	/* Any heap length should at least match OS page size at the minimum or
	 * a multiple of OS page size */
	if ((b.uiHeapLength == 0) || (b.uiHeapLength & (ui32OSPageSize - 1)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			     "%s: Invalid Heap \"%s\" Size: %llu (0x%llx)",
			     __func__,
			     b.pszName, b.uiHeapLength, b.uiHeapLength));
		PVR_DPF((PVR_DBG_ERROR,
			     "Heap Size should always be a non-zero value and a "
			     "multiple of OS Page Size:%u(0x%x)",
			     ui32OSPageSize, ui32OSPageSize));
		PVR_ASSERT(b.uiHeapLength >= ui32OSPageSize);
	}


	PVR_ASSERT(b.uiReservedRegionLength % RGX_HEAP_RESERVED_SIZE_GRANULARITY == 0);

	if (!OSStringNCompare(name, RGX_GENERAL_NON4K_HEAP_IDENT, sizeof(RGX_GENERAL_NON4K_HEAP_IDENT)))
	{
		OSCreateKMAppHintState(&pvAppHintState);
		OSGetKMAppHintUINT32(pvAppHintState, GeneralNon4KHeapPageSize,
				&ui32AppHintDefault, &ui32GeneralNon4KHeapPageSize);
		switch (ui32GeneralNon4KHeapPageSize)
		{
			case (1<<RGX_HEAP_4KB_PAGE_SHIFT):
				b.uiLog2DataPageSize = RGX_HEAP_4KB_PAGE_SHIFT;
				break;
			case (1<<RGX_HEAP_16KB_PAGE_SHIFT):
				b.uiLog2DataPageSize = RGX_HEAP_16KB_PAGE_SHIFT;
				break;
			case (1<<RGX_HEAP_64KB_PAGE_SHIFT):
				b.uiLog2DataPageSize = RGX_HEAP_64KB_PAGE_SHIFT;
				break;
			case (1<<RGX_HEAP_256KB_PAGE_SHIFT):
				b.uiLog2DataPageSize = RGX_HEAP_256KB_PAGE_SHIFT;
				break;
			case (1<<RGX_HEAP_1MB_PAGE_SHIFT):
				b.uiLog2DataPageSize = RGX_HEAP_1MB_PAGE_SHIFT;
				break;
			case (1<<RGX_HEAP_2MB_PAGE_SHIFT):
				b.uiLog2DataPageSize = RGX_HEAP_2MB_PAGE_SHIFT;
				break;
			default:
				b.uiLog2DataPageSize = RGX_HEAP_16KB_PAGE_SHIFT;

				PVR_DPF((PVR_DBG_ERROR,
						 "Invalid AppHint GeneralAltHeapPageSize [%d] value, using 16KB",
						 ui32AppHintDefault));
				break;
		}
		OSFreeKMAppHintState(pvAppHintState);
	}

	return b;
}

#define INIT_HEAP(NAME) \
do { \
	*psDeviceMemoryHeapCursor = _blueprint_init( \
			RGX_ ## NAME ## _HEAP_IDENT, \
			RGX_ ## NAME ## _HEAP_BASE, \
			RGX_ ## NAME ## _HEAP_SIZE, \
			RGX_ ## NAME ## _HEAP_RESERVED_SIZE, \
			0); \
	psDeviceMemoryHeapCursor++; \
} while (0)

#define INIT_FW_HEAP(TYPE, MODE) \
do { \
	*psDeviceMemoryHeapCursor = _blueprint_init( \
			RGX_FIRMWARE_ ## TYPE ## _HEAP_IDENT, \
			RGX_FIRMWARE_ ## MODE ## _ ## TYPE ## _HEAP_BASE, \
			RGX_FIRMWARE_ ## TYPE ## _HEAP_SIZE, \
			0, /* No reserved space in any FW heaps */ \
			0); \
	psDeviceMemoryHeapCursor++; \
} while (0)

#define INIT_HEAP_NAME(STR, NAME) \
do { \
	*psDeviceMemoryHeapCursor = _blueprint_init( \
			RGX_ ## STR ## _HEAP_IDENT, \
			RGX_ ## NAME ## _HEAP_BASE, \
			RGX_ ## NAME ## _HEAP_SIZE, \
			RGX_ ## STR ## _HEAP_RESERVED_SIZE, \
			0); \
	psDeviceMemoryHeapCursor++; \
} while (0)


static PVRSRV_ERROR RGXInitHeaps(PVRSRV_RGXDEV_INFO *psDevInfo,
								 DEVICE_MEMORY_INFO *psNewMemoryInfo,
								 IMG_UINT32 *pui32Log2DummyPgSize)
{
	DEVMEM_HEAP_BLUEPRINT *psDeviceMemoryHeapCursor;
	void *pvAppHintState = NULL;
	IMG_UINT32 ui32AppHintDefault = PVRSRV_APPHINT_GENERALNON4KHEAPPAGESIZE;
	IMG_UINT32 ui32GeneralNon4KHeapPageSize;

	/* FIXME - consider whether this ought not to be on the device node itself */
	psNewMemoryInfo->psDeviceMemoryHeap = OSAllocMem(sizeof(DEVMEM_HEAP_BLUEPRINT) * RGX_MAX_HEAP_ID);
	if (psNewMemoryInfo->psDeviceMemoryHeap == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "RGXRegisterDevice : Failed to alloc memory for DEVMEM_HEAP_BLUEPRINT"));
		goto e0;
	}

	/* Get the page size for the dummy page from the NON4K heap apphint */
	OSCreateKMAppHintState(&pvAppHintState);
	OSGetKMAppHintUINT32(pvAppHintState, GeneralNon4KHeapPageSize,
			&ui32AppHintDefault, &ui32GeneralNon4KHeapPageSize);
	*pui32Log2DummyPgSize = ExactLog2(ui32GeneralNon4KHeapPageSize);
	OSFreeKMAppHintState(pvAppHintState);

	/* Initialise the heaps */
	psDeviceMemoryHeapCursor = psNewMemoryInfo->psDeviceMemoryHeap;

	INIT_HEAP(GENERAL_SVM);
	INIT_HEAP(GENERAL);
	INIT_HEAP(GENERAL_NON4K);
	INIT_HEAP(PDSCODEDATA);
	INIT_HEAP(USCCODE);
	INIT_HEAP(TQ3DPARAMETERS);
	INIT_HEAP(SIGNALS);
	INIT_HEAP(COMPONENT_CTRL);
	INIT_HEAP(FBCDC);
	INIT_HEAP(PDS_INDIRECT_STATE);
	INIT_HEAP(TEXTURE_STATE);
	INIT_HEAP(TDM_TPU_YUV_COEFFS);
	INIT_HEAP(VISIBILITY_TEST);

	/* vulkan capture replay buffer heap */
	INIT_HEAP_NAME(VK_CAPT_REPLAY_BUF, VK_CAPT_REPLAY_BUF);

	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		INIT_FW_HEAP(CONFIG, GUEST);
		INIT_FW_HEAP(MAIN, GUEST);
	}
	else
	{
		INIT_FW_HEAP(MAIN, HOST);
		INIT_FW_HEAP(CONFIG, HOST);
	}

	/* set the heap count */
	psNewMemoryInfo->ui32HeapCount = (IMG_UINT32)(psDeviceMemoryHeapCursor - psNewMemoryInfo->psDeviceMemoryHeap);

	PVR_ASSERT(psNewMemoryInfo->ui32HeapCount <= RGX_MAX_HEAP_ID);

	/*
	   In the new heap setup, we initialise 2 configurations:
		1 - One will be for the firmware only (index 1 in array)
			a. This primarily has the firmware heap in it.
			b. It also has additional guest OSID firmware heap(s)
				- Only if the number of support firmware OSID > 1
		2 - Others shall be for clients only (index 0 in array)
			a. This has all the other client heaps in it.
	*/
	psNewMemoryInfo->uiNumHeapConfigs = 2;
	psNewMemoryInfo->psDeviceMemoryHeapConfigArray = OSAllocMem(sizeof(DEVMEM_HEAP_CONFIG) * psNewMemoryInfo->uiNumHeapConfigs);
	if (psNewMemoryInfo->psDeviceMemoryHeapConfigArray == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "RGXRegisterDevice : Failed to alloc memory for DEVMEM_HEAP_CONFIG"));
		goto e1;
	}

	psNewMemoryInfo->psDeviceMemoryHeapConfigArray[0].pszName = "Default Heap Configuration";
	psNewMemoryInfo->psDeviceMemoryHeapConfigArray[0].uiNumHeaps = psNewMemoryInfo->ui32HeapCount - RGX_FIRMWARE_NUMBER_OF_FW_HEAPS;
	psNewMemoryInfo->psDeviceMemoryHeapConfigArray[0].psHeapBlueprintArray = psNewMemoryInfo->psDeviceMemoryHeap;

	psNewMemoryInfo->psDeviceMemoryHeapConfigArray[1].pszName = "Firmware Heap Configuration";
	psNewMemoryInfo->psDeviceMemoryHeapConfigArray[1].uiNumHeaps = RGX_FIRMWARE_NUMBER_OF_FW_HEAPS;
	psNewMemoryInfo->psDeviceMemoryHeapConfigArray[1].psHeapBlueprintArray = psDeviceMemoryHeapCursor-2;

	if (RGX_GET_FEATURE_VALUE(psDevInfo, MMU_VERSION) >= 4)
	{
		IMG_UINT32 i;

		/*
		 * Zero all MMU Page Size Range Config registers so they won't participate in
		 * address decode. Size field = 0 also means 4K page size which is default
		 * for all addresses not within the ranges defined by these registers.
		 */
		for (i = 0; i < ARRAY_SIZE(psDevInfo->aui64MMUPageSizeRangeValue); ++i)
		{
			psDevInfo->aui64MMUPageSizeRangeValue[i] = 0;
		}

		/*
		 * Set up the first range only to reflect the Non4K general heap.
		 * In future, we could allow multiple simultaneous non4K page sizes.
		 */
		if (ui32GeneralNon4KHeapPageSize != 4*1024)
		{
				psDevInfo->aui64MMUPageSizeRangeValue[0] = RGXMMUInit_GetConfigRangeValue(ui32GeneralNon4KHeapPageSize,
																						  RGX_GENERAL_NON4K_HEAP_BASE,
																						  RGX_GENERAL_NON4K_HEAP_SIZE);
		}
	}

#if (RGX_NUM_OS_SUPPORTED > 1)
	/* Perform additional virtualization initialization */
	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_HOST))
	{
		IMG_UINT32 ui32OSid;

		/* Create additional guest OSID firmware heaps */
		for (ui32OSid = 1; ui32OSid < RGX_NUM_OS_SUPPORTED; ui32OSid++)
		{
			if (RGXInitGuestFwHeap(psDeviceMemoryHeapCursor, ui32OSid) != PVRSRV_OK)
			{
				IMG_UINT32 ui32DeInitOs;

				/* if any allocation fails, free previously allocated heaps and abandon initialisation */
				for (ui32DeInitOs = ui32OSid - 1; ui32DeInitOs > 0; ui32DeInitOs--)
				{
					RGXDeInitGuestFwHeap(psDeviceMemoryHeapCursor);
					psDeviceMemoryHeapCursor--;
				}
				goto e1;
			}

			/* Append additional guest(s) firmware heap to host driver firmware context heap configuration */
			psNewMemoryInfo->psDeviceMemoryHeapConfigArray[1].uiNumHeaps += 1;

			/* advance to the next heap */
			psDeviceMemoryHeapCursor++;
		}
	}
#endif /* (RGX_NUM_OS_SUPPORTED > 1) */

	return PVRSRV_OK;
e1:
	OSFreeMem(psNewMemoryInfo->psDeviceMemoryHeap);
e0:
	return PVRSRV_ERROR_OUT_OF_MEMORY;
}

#undef INIT_HEAP
#undef INIT_HEAP_NAME


static void RGXDeInitHeaps(DEVICE_MEMORY_INFO *psDevMemoryInfo)
{
#if (RGX_NUM_OS_SUPPORTED > 1)
	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_HOST))
	{
		IMG_UINT32 ui32OSid;
		DEVMEM_HEAP_BLUEPRINT *psDeviceMemoryHeapCursor = psDevMemoryInfo->psDeviceMemoryHeap;

		/* Create additional guest OSID firmware heaps */
		for (ui32OSid = 1; ui32OSid < RGX_NUM_OS_SUPPORTED; ui32OSid++)
		{
			RGXDeInitGuestFwHeap(psDeviceMemoryHeapCursor);
			psDeviceMemoryHeapCursor++;
		}
	}
#endif /* (RGX_NUM_OS_SUPPORTED > 1) */

	OSFreeMem(psDevMemoryInfo->psDeviceMemoryHeapConfigArray);
	OSFreeMem(psDevMemoryInfo->psDeviceMemoryHeap);
}

/*
	RGXRegisterDevice
*/
PVRSRV_ERROR RGXRegisterDevice (PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_ERROR eError;
	DEVICE_MEMORY_INFO *psDevMemoryInfo;
	PVRSRV_RGXDEV_INFO	*psDevInfo;

	PDUMPCOMMENT("Device Name: %s", psDeviceNode->psDevConfig->pszName);

	if (psDeviceNode->psDevConfig->pszVersion)
	{
		PDUMPCOMMENT("Device Version: %s", psDeviceNode->psDevConfig->pszVersion);
	}

	PDUMPCOMMENT("RGX Initialisation (Part 1)");

	/*********************
	 * Device node setup *
	 *********************/
	/* Setup static data and callbacks on the device agnostic device node */
#if defined(PDUMP)
	psDeviceNode->sDevId.pszPDumpRegName	= RGX_PDUMPREG_NAME;
	/*
		FIXME: This should not be required as PMR's should give the memspace
		name. However, due to limitations within PDump we need a memspace name
		when pdumping with MMU context with virtual address in which case we
		don't have a PMR to get the name from.

		There is also the issue obtaining a namespace name for the catbase which
		is required when we PDump the write of the physical catbase into the FW
		structure
	*/
	psDeviceNode->sDevId.pszPDumpDevName	= PhysHeapPDumpMemspaceName(psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL]);
	psDeviceNode->pfnPDumpInitDevice = &RGXResetPDump;
	psDeviceNode->ui64FBCClearColour = RGX_FBC_CC_DEFAULT;

#endif /* PDUMP */

	OSAtomicWrite(&psDeviceNode->eHealthStatus, PVRSRV_DEVICE_HEALTH_STATUS_OK);
	OSAtomicWrite(&psDeviceNode->eHealthReason, PVRSRV_DEVICE_HEALTH_REASON_NONE);

	psDeviceNode->pfnDevSLCFlushRange = RGXSLCFlushRange;
	psDeviceNode->pfnInvalFBSCTable = RGXInvalidateFBSCTable;

	psDeviceNode->pfnMMUCacheInvalidate = RGXMMUCacheInvalidate;

	psDeviceNode->pfnMMUCacheInvalidateKick = RGXMMUCacheInvalidateKick;

	/* Register RGX to receive notifies when other devices complete some work */
	PVRSRVRegisterCmdCompleteNotify(&psDeviceNode->hCmdCompNotify, &RGXScheduleProcessQueuesKM, psDeviceNode);

	psDeviceNode->pfnInitDeviceCompatCheck	= &RGXDevInitCompatCheck;

	/* Register callbacks for creation of device memory contexts */
	psDeviceNode->pfnRegisterMemoryContext = RGXRegisterMemoryContext;
	psDeviceNode->pfnUnregisterMemoryContext = RGXUnregisterMemoryContext;

	/* Register callbacks for Unified Fence Objects */
	psDeviceNode->pfnAllocUFOBlock = RGXAllocUFOBlock;
	psDeviceNode->pfnFreeUFOBlock = RGXFreeUFOBlock;

	/* Register callback for checking the device's health */
	psDeviceNode->pfnUpdateHealthStatus = RGXUpdateHealthStatus;

	/* Register method to service the FW HWPerf buffer */
	psDeviceNode->pfnServiceHWPerf = RGXHWPerfDataStoreCB;

	/* Register callback for getting the device version information string */
	psDeviceNode->pfnDeviceVersionString = RGXDevVersionString;

	/* Register callback for getting the device clock speed */
	psDeviceNode->pfnDeviceClockSpeed = RGXDevClockSpeed;

	/* Register callback for soft resetting some device modules */
	psDeviceNode->pfnSoftReset = RGXSoftReset;

	/* Register callback for resetting the HWR logs */
	psDeviceNode->pfnResetHWRLogs = RGXResetHWRLogs;

	/* Register callback for resetting the HWR logs */
	psDeviceNode->pfnVerifyBVNC = RGXVerifyBVNC;

	/* Register callback for checking alignment of UM structures */
	psDeviceNode->pfnAlignmentCheck = RGXAlignmentCheck;

	/*Register callback for checking the supported features and getting the
	 * corresponding values */
	psDeviceNode->pfnCheckDeviceFeature = RGXBvncCheckFeatureSupported;
	psDeviceNode->pfnGetDeviceFeatureValue = RGXBvncGetSupportedFeatureValue;

	/* Set up required support for dummy page */
	OSAtomicWrite(&(psDeviceNode->sDummyPage.atRefCounter), 0);
	OSAtomicWrite(&(psDeviceNode->sDevZeroPage.atRefCounter), 0);

	/* Set the order to 0 */
	psDeviceNode->sDummyPage.sPageHandle.uiOrder = 0;
	psDeviceNode->sDevZeroPage.sPageHandle.uiOrder = 0;

	/* Set the size of the Dummy page to zero */
	psDeviceNode->sDummyPage.ui32Log2PgSize = 0;

	/* Set the size of the Zero page to zero */
	psDeviceNode->sDevZeroPage.ui32Log2PgSize = 0;

	/* Set the Dummy page phys addr */
	psDeviceNode->sDummyPage.ui64PgPhysAddr = MMU_BAD_PHYS_ADDR;

	/* Set the Zero page phys addr */
	psDeviceNode->sDevZeroPage.ui64PgPhysAddr = MMU_BAD_PHYS_ADDR;

	/* The lock can be acquired from MISR (Z-buffer) path */
	eError = OSLockCreate(&psDeviceNode->sDummyPage.psPgLock);
	if (PVRSRV_OK != eError)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create dummy page lock", __func__));
		return eError;
	}

	/* Create the lock for zero page */
	eError = OSLockCreate(&psDeviceNode->sDevZeroPage.psPgLock);
	if (PVRSRV_OK != eError)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create Zero page lock", __func__));
		goto free_dummy_page;
	}
#if defined(PDUMP)
	psDeviceNode->sDummyPage.hPdumpPg = NULL;
	psDeviceNode->sDevZeroPage.hPdumpPg = NULL;
#endif

	/* Register callbacks for fabric coherency & non-mappable state */
	psDeviceNode->pfnHasCacheSnooping = RGXSystemHasCacheSnooping;
	psDeviceNode->pfnHasSnoopingOfCPUCache = RGXSystemSnoopingOfCPUCache;
	psDeviceNode->pfnHasSnoopingOfDeviceCache = RGXSystemSnoopingOfDeviceCache;
	psDeviceNode->pfnHasNonMappableLocalMemory = RGXSystemHasNonMappableLocalMemory;
	psDeviceNode->pfnHasFBCDCVersion31 = RGXSystemHasFBCDCVersion31;

	/* The device shared-virtual-memory heap address-space size is stored here for faster
	   look-up without having to walk the device heap configuration structures during
	   client device connection  (i.e. this size is relative to a zero-based offset) */
	psDeviceNode->ui64GeneralSVMHeapTopVA = RGX_GENERAL_SVM_HEAP_BASE + RGX_GENERAL_SVM_HEAP_SIZE;

	/*********************
	 * Device info setup *
	 *********************/
	/* Allocate device control block */
	psDevInfo = OSAllocZMem(sizeof(*psDevInfo));
	if (psDevInfo == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "DevInitRGXPart1 : Failed to alloc memory for DevInfo"));
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	/* create locks for the context lists stored in the DevInfo structure.
	 * these lists are modified on context create/destroy and read by the
	 * watchdog thread
	 */

	eError = OSWRLockCreate(&(psDevInfo->hRenderCtxListLock));
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create render context list lock", __func__));
		goto e0;
	}

	eError = OSWRLockCreate(&(psDevInfo->hComputeCtxListLock));
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create compute context list lock", __func__));
		goto e1;
	}

	eError = OSWRLockCreate(&(psDevInfo->hTransferCtxListLock));
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create transfer context list lock", __func__));
		goto e2;
	}

	eError = OSWRLockCreate(&(psDevInfo->hTDMCtxListLock));
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create TDM context list lock", __func__));
		goto e3;
	}

	eError = OSWRLockCreate(&(psDevInfo->hKickSyncCtxListLock));
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create kick sync context list lock", __func__));
		goto e4;
	}

	eError = OSWRLockCreate(&(psDevInfo->hMemoryCtxListLock));
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create memory context list lock", __func__));
		goto e5;
	}

	eError = OSLockCreate(&psDevInfo->hLockKCCBDeferredCommandsList);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to KCCB deferred commands list lock", __func__));
		goto e6;
	}
	dllist_init(&(psDevInfo->sKCCBDeferredCommandsListHead));

	dllist_init(&(psDevInfo->sRenderCtxtListHead));
	dllist_init(&(psDevInfo->sComputeCtxtListHead));
	dllist_init(&(psDevInfo->sTDMCtxtListHead));
	dllist_init(&(psDevInfo->sKickSyncCtxtListHead));

	dllist_init(&(psDevInfo->sCommonCtxtListHead));
	psDevInfo->ui32CommonCtxtCurrentID = 1;


	eError = OSWRLockCreate(&psDevInfo->hCommonCtxtListLock);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create common context list lock", __func__));
		goto e7;
	}

	eError = OSLockCreate(&psDevInfo->sRegCongfig.hLock);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create register configuration lock", __func__));
		goto e8;
	}

	eError = OSLockCreate(&psDevInfo->hBPLock);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create lock for break points", __func__));
		goto e9;
	}

	eError = OSLockCreate(&psDevInfo->hRGXFWIfBufInitLock);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create lock for trace buffers", __func__));
		goto e10;
	}

	eError = OSLockCreate(&psDevInfo->hCCBStallCheckLock);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create stalled CCB checking lock", __func__));
		goto e11;
	}
	eError = OSLockCreate(&psDevInfo->hCCBRecoveryLock);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create stalled CCB recovery lock", __func__));
		goto e12;
	}

	eError = OSLockCreate(&psDevInfo->hTQSharedMemLock);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create TDM shared mem lock", __func__));
		goto e13;
	}

	dllist_init(&psDevInfo->sMemoryContextList);

	/* initialise ui32SLRHoldoffCounter */
	if (RGX_INITIAL_SLR_HOLDOFF_PERIOD_MS > DEVICES_WATCHDOG_POWER_ON_SLEEP_TIMEOUT)
	{
		psDevInfo->ui32SLRHoldoffCounter = RGX_INITIAL_SLR_HOLDOFF_PERIOD_MS / DEVICES_WATCHDOG_POWER_ON_SLEEP_TIMEOUT;
	}
	else
	{
		psDevInfo->ui32SLRHoldoffCounter = 0;
	}

	/* Setup static data and callbacks on the device specific device info */
	psDevInfo->psDeviceNode		= psDeviceNode;

	psDevMemoryInfo = &psDeviceNode->sDevMemoryInfo;
	psDevInfo->pvDeviceMemoryHeap = psDevMemoryInfo->psDeviceMemoryHeap;

	/*
	 * Map RGX Registers
	 */
#if !defined(NO_HARDWARE)
	psDevInfo->pvRegsBaseKM = (void __iomem *) OSMapPhysToLin(psDeviceNode->psDevConfig->sRegsCpuPBase,
												psDeviceNode->psDevConfig->ui32RegsSize,
										     PVRSRV_MEMALLOCFLAG_CPU_UNCACHED);

	if (psDevInfo->pvRegsBaseKM == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "PVRSRVRGXInitDevPart2KM: Failed to create RGX register mapping"));
		eError = PVRSRV_ERROR_BAD_MAPPING;
		goto e14;
	}
#else
	psDevInfo->pvRegsBaseKM = NULL;
#endif /* !NO_HARDWARE */

	psDeviceNode->pvDevice = psDevInfo;


	eError = RGXBvncInitialiseConfiguration(psDeviceNode);
	if (PVRSRV_OK != eError)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Unsupported HW device detected by driver",
		         __func__));
		goto e15;
	}

	/* pdump info about the core */
	PDUMPCOMMENT("RGX Version Information (KM): %d.%d.%d.%d",
	             psDevInfo->sDevFeatureCfg.ui32B,
	             psDevInfo->sDevFeatureCfg.ui32V,
	             psDevInfo->sDevFeatureCfg.ui32N,
	             psDevInfo->sDevFeatureCfg.ui32C);

	/* Configure MMU specific stuff */
	RGXMMUInit_Register(psDeviceNode);

	eError = RGXInitHeaps(psDevInfo, psDevMemoryInfo,
			&psDeviceNode->sDummyPage.ui32Log2PgSize);
	if (eError != PVRSRV_OK)
	{
		goto e15;
	}

	/*Set the zero page size as needed for the heap with biggest page size */
	psDeviceNode->sDevZeroPage.ui32Log2PgSize = psDeviceNode->sDummyPage.ui32Log2PgSize;

	eError = RGXHWPerfInit(psDevInfo);
	PVR_LOG_GOTO_IF_ERROR(eError, "RGXHWPerfInit", e15);

#if defined(SUPPORT_VALIDATION)
	eError = RGXSPUPowerDomainInitState(&psDevInfo->sSPUPowerDomainState,
										psDevInfo->sDevFeatureCfg.ui32MAXDustCount);
	if (eError != PVRSRV_OK)
	{
		goto e16;
	}
#if defined(SUPPORT_SOC_TIMER) && defined(PDUMP) && defined(NO_HARDWARE)
	{
		IMG_BOOL ui32AppHintDefault = IMG_FALSE;
		IMG_BOOL bInitSocTimer;
		void *pvAppHintState = NULL;

		OSCreateKMAppHintState(&pvAppHintState);
		OSGetKMAppHintBOOL(pvAppHintState, ValidateSOCUSCTimer, &ui32AppHintDefault, &bInitSocTimer);
		OSFreeKMAppHintState(pvAppHintState);

		if (bInitSocTimer)
		{
			PVRSRVPdumpInitSOCUSCTimer();
		}
	}
#endif
#endif

	/* Register callback for dumping debug info */
	eError = RGXDebugInit(psDevInfo);
	PVR_LOG_GOTO_IF_ERROR(eError, "RGXDebugInit", e17);



	/* Initialise the device dependent bridges */
	eError = DeviceDepBridgeInit(psDevInfo->sDevFeatureCfg.ui64Features);
	PVR_LOG_IF_ERROR(eError, "DeviceDepBridgeInit");

	return PVRSRV_OK;

e17:
#if defined(SUPPORT_VALIDATION)
	RGXSPUPowerDomainDeInitState(&psDevInfo->sSPUPowerDomainState);
e16:
#endif
	RGXHWPerfDeinit(psDevInfo);
e15:
#if !defined(NO_HARDWARE)
	OSUnMapPhysToLin((void __force *) psDevInfo->pvRegsBaseKM,
							 psDevInfo->ui32RegSize,
							 PVRSRV_MEMALLOCFLAG_CPU_UNCACHED);

e14:
#endif /* !NO_HARDWARE */
	OSLockDestroy(psDevInfo->hTQSharedMemLock);
e13:
	OSLockDestroy(psDevInfo->hCCBRecoveryLock);
e12:
	OSLockDestroy(psDevInfo->hCCBStallCheckLock);
e11:
	OSLockDestroy(psDevInfo->hRGXFWIfBufInitLock);
e10:
	OSLockDestroy(psDevInfo->hBPLock);
e9:
	OSLockDestroy(psDevInfo->sRegCongfig.hLock);
e8:
	OSWRLockDestroy(psDevInfo->hCommonCtxtListLock);
e7:
	OSLockDestroy(psDevInfo->hLockKCCBDeferredCommandsList);
e6:
	OSWRLockDestroy(psDevInfo->hMemoryCtxListLock);
e5:
	OSWRLockDestroy(psDevInfo->hKickSyncCtxListLock);
e4:
	OSWRLockDestroy(psDevInfo->hTDMCtxListLock);
e3:
	OSWRLockDestroy(psDevInfo->hTransferCtxListLock);
e2:
	OSWRLockDestroy(psDevInfo->hComputeCtxListLock);
e1:
	OSWRLockDestroy(psDevInfo->hRenderCtxListLock);
e0:
	OSFreeMem(psDevInfo);

	/* Destroy the zero page lock created above */
	OSLockDestroy(psDeviceNode->sDevZeroPage.psPgLock);

free_dummy_page:
	/* Destroy the dummy page lock created above */
	OSLockDestroy(psDeviceNode->sDummyPage.psPgLock);

	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

IMG_PCHAR RGXDevBVNCString(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	IMG_PCHAR psz = psDevInfo->sDevFeatureCfg.pszBVNCString;
	if (NULL == psz)
	{
		IMG_CHAR pszBVNCInfo[RGX_HWPERF_MAX_BVNC_LEN];
		size_t uiBVNCStringSize;
		size_t uiStringLength;

		uiStringLength = OSSNPrintf(pszBVNCInfo, RGX_HWPERF_MAX_BVNC_LEN, "%d.%d.%d.%d",
				psDevInfo->sDevFeatureCfg.ui32B,
				psDevInfo->sDevFeatureCfg.ui32V,
				psDevInfo->sDevFeatureCfg.ui32N,
				psDevInfo->sDevFeatureCfg.ui32C);
		PVR_ASSERT(uiStringLength < RGX_HWPERF_MAX_BVNC_LEN);

		uiBVNCStringSize = (uiStringLength + 1) * sizeof(IMG_CHAR);
		psz = OSAllocMem(uiBVNCStringSize);
		if (NULL != psz)
		{
			OSCachedMemCopy(psz, pszBVNCInfo, uiBVNCStringSize);
			psDevInfo->sDevFeatureCfg.pszBVNCString = psz;
		}
		else
		{
			PVR_DPF((PVR_DBG_MESSAGE,
					"%s: Allocating memory for BVNC Info string failed",
					__func__));
		}
	}

	return psz;
}

/*************************************************************************/ /*!
@Function       RGXDevVersionString
@Description    Gets the version string for the given device node and returns
                a pointer to it in ppszVersionString. It is then the
                responsibility of the caller to free this memory.
@Input          psDeviceNode            Device node from which to obtain the
                                        version string
@Output	        ppszVersionString	Contains the version string upon return
@Return         PVRSRV_ERROR
*/ /**************************************************************************/
static PVRSRV_ERROR RGXDevVersionString(PVRSRV_DEVICE_NODE *psDeviceNode,
					IMG_CHAR **ppszVersionString)
{
#if defined(COMPAT_BVNC_MASK_B) || defined(COMPAT_BVNC_MASK_V) || defined(COMPAT_BVNC_MASK_N) || defined(COMPAT_BVNC_MASK_C) || defined(NO_HARDWARE) || defined(EMULATOR)
	const IMG_CHAR szFormatString[] = "Rogue Version: %s (SW)";
#else
	const IMG_CHAR szFormatString[] = "Rogue Version: %s (HW)";
#endif
	PVRSRV_RGXDEV_INFO *psDevInfo;
	IMG_PCHAR pszBVNC;
	size_t uiStringLength;

	if (psDeviceNode == NULL || ppszVersionString == NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDevInfo = (PVRSRV_RGXDEV_INFO *)psDeviceNode->pvDevice;
	pszBVNC = RGXDevBVNCString(psDevInfo);

	if (NULL == pszBVNC)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	uiStringLength = OSStringLength(pszBVNC);
	uiStringLength += (sizeof(szFormatString) - 2); /* sizeof includes the null, -2 for "%s" */
	*ppszVersionString = OSAllocMem(uiStringLength * sizeof(IMG_CHAR));
	if (*ppszVersionString == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	OSSNPrintf(*ppszVersionString, uiStringLength, szFormatString,
		pszBVNC);

	return PVRSRV_OK;
}

/**************************************************************************/ /*!
@Function       RGXDevClockSpeed
@Description    Gets the clock speed for the given device node and returns
                it in pui32RGXClockSpeed.
@Input          psDeviceNode		Device node
@Output         pui32RGXClockSpeed  Variable for storing the clock speed
@Return         PVRSRV_ERROR
*/ /***************************************************************************/
static PVRSRV_ERROR RGXDevClockSpeed(PVRSRV_DEVICE_NODE *psDeviceNode,
					IMG_PUINT32  pui32RGXClockSpeed)
{
	RGX_DATA *psRGXData = (RGX_DATA*) psDeviceNode->psDevConfig->hDevData;

	/* get clock speed */
	*pui32RGXClockSpeed = psRGXData->psRGXTimingInfo->ui32CoreClockSpeed;

	return PVRSRV_OK;
}

#if (RGX_NUM_OS_SUPPORTED > 1)
/*!
 *******************************************************************************

 @Function		RGXInitGuestFwHeap

 @Description	Called to perform additional initialisation
 ******************************************************************************/
static PVRSRV_ERROR RGXInitGuestFwHeap(DEVMEM_HEAP_BLUEPRINT *psDevMemHeap, IMG_UINT32 ui32OSid)
{
	IMG_UINT32 uiStringLength;
	IMG_UINT32 uiStringLengthMax = 32;

	uiStringLength = MIN(sizeof(RGX_FIRMWARE_GUEST_RAW_HEAP_IDENT), uiStringLengthMax + 1);

	/* Start by allocating memory for this guest OSID heap identification string */
	psDevMemHeap->pszName = OSAllocMem(uiStringLength * sizeof(IMG_CHAR));
	if (psDevMemHeap->pszName == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	/* Append the guest OSID number to the RGX_FIRMWARE_GUEST_RAW_HEAP_IDENT string */
	OSSNPrintf((IMG_CHAR *)psDevMemHeap->pszName, uiStringLength, RGX_FIRMWARE_GUEST_RAW_HEAP_IDENT, ui32OSid);

	/* Use the common blueprint template support function to initialise the heap */
	*psDevMemHeap = _blueprint_init((IMG_CHAR *)psDevMemHeap->pszName,
			RGX_FIRMWARE_RAW_HEAP_BASE + (ui32OSid * RGX_FIRMWARE_RAW_HEAP_SIZE),
			RGX_FIRMWARE_RAW_HEAP_SIZE,
			0,
			0);

	return PVRSRV_OK;
}

/*!
 *******************************************************************************

 @Function		RGXDeInitGuestFwHeap

 @Description	Called to perform additional deinitialisation
 ******************************************************************************/
static void RGXDeInitGuestFwHeap(DEVMEM_HEAP_BLUEPRINT *psDevMemHeap)
{
	IMG_UINT64 uiBase = RGX_FIRMWARE_RAW_HEAP_BASE + RGX_FIRMWARE_RAW_HEAP_SIZE;
	IMG_UINT64 uiSpan = uiBase + ((RGX_NUM_OS_SUPPORTED - 1) * RGX_FIRMWARE_RAW_HEAP_SIZE);

	/* Safe to do as the guest firmware heaps are last in the list */
	if (psDevMemHeap->sHeapBaseAddr.uiAddr >= uiBase &&
	    psDevMemHeap->sHeapBaseAddr.uiAddr < uiSpan)
	{
		void *pszName = (void*)psDevMemHeap->pszName;
		OSFreeMem(pszName);
	}
}
#endif /* (RGX_NUM_OS_SUPPORTED > 1) */

/******************************************************************************
 End of file (rgxinit.c)
******************************************************************************/
