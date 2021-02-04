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

#include <stddef.h>

#include "img_defs.h"
#include "pvr_notifier.h"
#include "pvrsrv.h"
#include "pvrsrv_bridge_init.h"
#include "syscommon.h"
#include "rgx_heaps.h"
#include "rgxheapconfig.h"
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
#include "debugmisc_server.h"

#include "rgxutils.h"
#include "rgxfwutils.h"
#include "rgx_fwif_km.h"

#include "rgxmmuinit.h"
#include "rgxmipsmmuinit.h"
#include "physmem.h"
#include "devicemem_utils.h"
#include "devicemem_server.h"
#include "physmem_osmem.h"

#include "rgxdebug.h"
#include "rgxhwperf.h"
#if defined(SUPPORT_GPUTRACE_EVENTS)
#include "pvr_gputrace.h"
#endif
#include "htbserver.h"

#include "rgx_options.h"
#include "pvrversion.h"

#include "rgx_compat_bvnc.h"

#include "rgx_heaps.h"

#include "rgxta3d.h"
#include "rgxtimecorr.h"

#include "rgx_bvnc_defs_km.h"
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

static PVRSRV_ERROR RGXDevInitCompatCheck(PVRSRV_DEVICE_NODE *psDeviceNode);
static PVRSRV_ERROR RGXDevVersionString(PVRSRV_DEVICE_NODE *psDeviceNode, IMG_CHAR **ppszVersionString);
static PVRSRV_ERROR RGXDevClockSpeed(PVRSRV_DEVICE_NODE *psDeviceNode, IMG_PUINT32  pui32RGXClockSpeed);
static PVRSRV_ERROR RGXSoftReset(PVRSRV_DEVICE_NODE *psDeviceNode, IMG_UINT64  ui64ResetValue1, IMG_UINT64  ui64ResetValue2);
static PVRSRV_ERROR RGXVzInitCreateFWKernelMemoryContext(PVRSRV_DEVICE_NODE *psDeviceNode);
static void RGXVzDeInitDestroyFWKernelMemoryContext(PVRSRV_DEVICE_NODE *psDeviceNode);
static PVRSRV_ERROR RGXVzInitHeaps(DEVICE_MEMORY_INFO *psNewMemoryInfo,
							DEVMEM_HEAP_BLUEPRINT *psDeviceMemoryHeapCursor);
static void RGXVzDeInitHeaps(DEVICE_MEMORY_INFO *psDevMemoryInfo);

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
	IMG_UINT32 aui32InterruptTimeSnapshot[RGXFW_THREAD_NUM];
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
									  volatile IMG_UINT32 *paui32InputTime,
									  volatile IMG_UINT32 *paui32Output,
									  volatile IMG_UINT32 *paui32OutputTime)
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
			paui32OutputTime[ui32TID] = paui32InputTime[ui32TID];
			bReturnVal = IMG_TRUE;
		}
	}

	return bReturnVal;
}

static IMG_BOOL _WaitForInterruptsTimeoutCheck(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	RGXFWIF_TRACEBUF *psRGXFWIfTraceBuf = psDevInfo->psRGXFWIfTraceBuf;
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
		PVR_DPF((PVR_DBG_ERROR, \
				"RGX FW thread %u: InterruptCountSnapshot: 0x%X, InterruptTimeSnapshot: %u", \
				ui32TID, g_sLISRExecutionInfo.aui32InterruptCountSnapshot[ui32TID],
				g_sLISRExecutionInfo.aui32InterruptTimeSnapshot[ui32TID]));
	}
#else
	PVR_DPF((PVR_DBG_ERROR, "No further information available. Please enable PVRSRV_DEBUG_LISR_EXECUTION"));
#endif


	if (psRGXFWIfTraceBuf->ePowState != RGXFWIF_POW_OFF)
	{
		PVR_DPF((PVR_DBG_ERROR, "_WaitForInterruptsTimeout: FW pow state is not OFF (is %u)",
						(unsigned int) psRGXFWIfTraceBuf->ePowState));
	}

	bScheduleMISR = SampleIRQCount(psRGXFWIfTraceBuf->aui32InterruptCount,
								   psRGXFWIfTraceBuf->aui32InterruptTime,
								   psDevInfo->aui32SampleIRQCount,
								   psDevInfo->aui32SampleIRQTime);
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
	RGXFWIF_TRACEBUF *psRGXFWIfTraceBuf;
	IMG_UINT32 ui32IRQStatus, ui32IRQStatusReg, ui32IRQStatusEventMsk, ui32IRQClearReg, ui32IRQClearMask;

	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		if (! psDevInfo->bRGXPowered)
		{
			return IMG_FALSE;
		}

		OSScheduleMISR(psDevInfo->pvMISRData);
		return IMG_TRUE;
	}
	else
	{
		bInterruptProcessed = IMG_FALSE;
		psRGXFWIfTraceBuf = psDevInfo->psRGXFWIfTraceBuf;
	}

	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, MIPS))
	{
		ui32IRQStatusReg = RGX_CR_MIPS_WRAPPER_IRQ_STATUS;
		ui32IRQStatusEventMsk = RGX_CR_MIPS_WRAPPER_IRQ_STATUS_EVENT_EN;
		ui32IRQClearReg = RGX_CR_MIPS_WRAPPER_IRQ_CLEAR;
		ui32IRQClearMask = RGX_CR_MIPS_WRAPPER_IRQ_CLEAR_EVENT_EN;
	}else
	{
		ui32IRQStatusReg = RGX_CR_META_SP_MSLVIRQSTATUS;
		ui32IRQStatusEventMsk = RGX_CR_META_SP_MSLVIRQSTATUS_TRIGVECT2_EN;
		ui32IRQClearReg = RGX_CR_META_SP_MSLVIRQSTATUS;
		ui32IRQClearMask = RGX_CR_META_SP_MSLVIRQSTATUS_TRIGVECT2_CLRMSK;
	}

#if defined(PVRSRV_DEBUG_LISR_EXECUTION)
	IMG_UINT32 ui32TID;

	for (ui32TID = 0; ui32TID < RGXFW_THREAD_NUM; ui32TID++)
	{
		g_sLISRExecutionInfo.aui32InterruptCountSnapshot[ui32TID ] =
			psRGXFWIfTraceBuf->aui32InterruptCount[ui32TID];

		g_sLISRExecutionInfo.aui32InterruptTimeSnapshot[ui32TID] =
			psRGXFWIfTraceBuf->aui32InterruptTime[ui32TID];
	}
	g_sLISRExecutionInfo.ui32State = 0;
	g_sLISRExecutionInfo.ui64Clockns = OSClockns64();
#endif

	if (psDevInfo->bRGXPowered == IMG_FALSE)
	{
#if defined(PVRSRV_DEBUG_LISR_EXECUTION)
		g_sLISRExecutionInfo.ui32State |= RGX_LISR_DEVICE_NOT_POWERED;
#endif
		if (psRGXFWIfTraceBuf->ePowState == RGXFWIF_POW_OFF)
		{
#if defined(PVRSRV_DEBUG_LISR_EXECUTION)
			g_sLISRExecutionInfo.ui32State |= RGX_LISR_FWIF_POW_OFF;
#endif
			return bInterruptProcessed;
		}
	}

	ui32IRQStatus = OSReadHWReg32(psDevInfo->pvRegsBaseKM, ui32IRQStatusReg);
	if (ui32IRQStatus & ui32IRQStatusEventMsk)
	{
#if defined(PVRSRV_DEBUG_LISR_EXECUTION)
		g_sLISRExecutionInfo.ui32State |= RGX_LISR_EVENT_EN;
#endif

		OSWriteHWReg32(psDevInfo->pvRegsBaseKM, ui32IRQClearReg, ui32IRQClearMask);

#if defined(RGX_FEATURE_OCPBUS)
		OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_OCP_IRQSTATUS_2, RGX_CR_OCP_IRQSTATUS_2_RGX_IRQ_STATUS_EN);
#endif

		bInterruptProcessed = SampleIRQCount(psRGXFWIfTraceBuf->aui32InterruptCount,
											 psRGXFWIfTraceBuf->aui32InterruptTime,
											 psDevInfo->aui32SampleIRQCount,
											 psDevInfo->aui32SampleIRQTime);

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

#if defined(PVRSRV_USE_BRIDGE_LOCK)
	OSAcquireBridgeLock();
#endif

	/* Powerlock to avoid further Power transition requests
	   while KCCB deferred list is being processed */
	eError = PVRSRVPowerLock(psDeviceNode);
	if(eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Failed to acquire PowerLock (device: %p, error: %s)",
				 __func__, psDeviceNode, PVRSRVGetErrorStringKM(eError)));
		goto _RGX_MISR_ProcessKCCBDeferredList_PowerLock_failed;
	}

	/* Try to send deferred KCCB commands Do not Poll from here*/
	eError = RGXSendCommandsFromDeferredList(psDevInfo, IMG_FALSE);
	if(eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_WARNING, "RGX_MISR_ProcessKCCBDeferredList  \
								could not flush Deferred KCCB list, KCCB is full.\n"));
	}

	PVRSRVPowerUnlock(psDeviceNode);

_RGX_MISR_ProcessKCCBDeferredList_PowerLock_failed:

#if defined(PVRSRV_USE_BRIDGE_LOCK)
	OSReleaseBridgeLock();
#endif
	return;
}

static void RGX_MISRHandler_CheckFWActivePowerState(void *psDevice)
{
	PVRSRV_DEVICE_NODE	*psDeviceNode = psDevice;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	RGXFWIF_TRACEBUF *psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (psFWTraceBuf->ePowState == RGXFWIF_POW_ON || psFWTraceBuf->ePowState == RGXFWIF_POW_IDLE)
	{
		RGX_MISR_ProcessKCCBDeferredList(psDeviceNode);
	}

	if (psFWTraceBuf->ePowState == RGXFWIF_POW_IDLE)
	{
		/* The FW is IDLE and therefore could be shut down */
		eError = RGXActivePowerRequest(psDeviceNode);

		if ((eError != PVRSRV_OK) && (eError != PVRSRV_ERROR_DEVICE_POWER_CHANGE_DENIED))
		{
			PVR_DPF((PVR_DBG_WARNING,
					 "%s: Failed RGXActivePowerRequest call (device: %p) with %s",
					 __func__, psDeviceNode, PVRSRVGetErrorStringKM(eError)));

			PVRSRVDebugRequest(psDeviceNode, DEBUG_REQUEST_VERBOSITY_MAX, NULL, NULL);
		}
	}

}

/* Shorter defines to keep the code a bit shorter */
#define GPU_ACTIVE_LOW   RGXFWIF_GPU_UTIL_STATE_ACTIVE_LOW
#define GPU_IDLE         RGXFWIF_GPU_UTIL_STATE_IDLE
#define GPU_ACTIVE_HIGH  RGXFWIF_GPU_UTIL_STATE_ACTIVE_HIGH
#define GPU_BLOCKED      RGXFWIF_GPU_UTIL_STATE_BLOCKED
#define MAX_ITERATIONS   64

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

#ifdef ENABLE_COMMON_DVFS
	unsigned long uLockFlags;
#endif

	/***** (1) Initialise return stats *****/

	psReturnStats->bValid = IMG_FALSE;
	psReturnStats->ui64GpuStatActiveLow  = 0;
	psReturnStats->ui64GpuStatIdle       = 0;
	psReturnStats->ui64GpuStatActiveHigh = 0;
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

#ifndef ENABLE_COMMON_DVFS
		OSLockAcquire(psDevInfo->hGPUUtilLock);
#else
	spin_lock_irqsave(&psDevInfo->sGPUUtilLock, uLockFlags);
#endif

		/*
		 * First attempt at detecting if the FW is in the middle of an update.
		 * This should also help if the FW is in the middle of a 64 bit variable update.
		 */
		while(((ui64LastWord != psUtilFWCb->ui64LastWord) ||
		       (aui64TmpCounters[ui64LastState] !=
		        psUtilFWCb->aui64StatsCounters[ui64LastState])) &&
		      (i < MAX_ITERATIONS))
		{
			ui64LastWord  = psUtilFWCb->ui64LastWord;
			ui64LastState = RGXFWIF_GPU_UTIL_GET_STATE(ui64LastWord);
			aui64TmpCounters[GPU_ACTIVE_LOW]  = psUtilFWCb->aui64StatsCounters[GPU_ACTIVE_LOW];
			aui64TmpCounters[GPU_IDLE]        = psUtilFWCb->aui64StatsCounters[GPU_IDLE];
			aui64TmpCounters[GPU_ACTIVE_HIGH] = psUtilFWCb->aui64StatsCounters[GPU_ACTIVE_HIGH];
			aui64TmpCounters[GPU_BLOCKED]     = psUtilFWCb->aui64StatsCounters[GPU_BLOCKED];
			i++;
		}

#ifndef ENABLE_COMMON_DVFS
		OSLockRelease(psDevInfo->hGPUUtilLock);
#else
	spin_unlock_irqrestore(&psDevInfo->sGPUUtilLock, uLockFlags);
#endif

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
		psReturnStats->ui64GpuStatActiveLow = RGXFWIF_GPU_UTIL_GET_PERIOD(aui64TmpCounters[GPU_ACTIVE_LOW],
		                                                                  psAggregateStats->ui64GpuStatActiveLow);
		psReturnStats->ui64GpuStatIdle = RGXFWIF_GPU_UTIL_GET_PERIOD(aui64TmpCounters[GPU_IDLE],
		                                                             psAggregateStats->ui64GpuStatIdle);
		psReturnStats->ui64GpuStatActiveHigh = RGXFWIF_GPU_UTIL_GET_PERIOD(aui64TmpCounters[GPU_ACTIVE_HIGH],
		                                                                   psAggregateStats->ui64GpuStatActiveHigh);
		psReturnStats->ui64GpuStatBlocked = RGXFWIF_GPU_UTIL_GET_PERIOD(aui64TmpCounters[GPU_BLOCKED],
		                                                                psAggregateStats->ui64GpuStatBlocked);
		psReturnStats->ui64GpuStatCumulative = psReturnStats->ui64GpuStatActiveLow + psReturnStats->ui64GpuStatIdle +
		                                       psReturnStats->ui64GpuStatActiveHigh + psReturnStats->ui64GpuStatBlocked;

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

	psAggregateStats->ui64GpuStatActiveLow  += psReturnStats->ui64GpuStatActiveLow;
	psAggregateStats->ui64GpuStatIdle       += psReturnStats->ui64GpuStatIdle;
	psAggregateStats->ui64GpuStatActiveHigh += psReturnStats->ui64GpuStatActiveHigh;
	psAggregateStats->ui64GpuStatBlocked    += psReturnStats->ui64GpuStatBlocked;
	psAggregateStats->ui64TimeStamp          = ui64TimeNow;


	/***** (5) Convert return stats to microseconds *****/

	psReturnStats->ui64GpuStatActiveLow  = OSDivide64(psReturnStats->ui64GpuStatActiveLow, 1000, &ui32Remainder);
	psReturnStats->ui64GpuStatIdle       = OSDivide64(psReturnStats->ui64GpuStatIdle, 1000, &ui32Remainder);
	psReturnStats->ui64GpuStatActiveHigh = OSDivide64(psReturnStats->ui64GpuStatActiveHigh, 1000, &ui32Remainder);
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

	psAggregateStats->ui64GpuStatActiveLow  = 0;
	psAggregateStats->ui64GpuStatIdle       = 0;
	psAggregateStats->ui64GpuStatActiveHigh = 0;
	psAggregateStats->ui64GpuStatBlocked    = 0;
	psAggregateStats->ui64TimeStamp         = 0;

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

	/* Inform other services devices that we have finished an operation */
	PVRSRVCheckStatus(psDeviceNode);

#if defined(SUPPORT_PDVFS) && defined(RGXFW_META_SUPPORT_2ND_THREAD)
	/* Normally, firmware CCB only exists for the primary FW thread unless PDVFS
	   is running on the second[ary] FW thread, here we process said CCB */
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


/* This function puts into the firmware image some parameters for the initial boot */
static PVRSRV_ERROR RGXBootldrDataInit(PVRSRV_DEVICE_NODE *psDeviceNode,
                                       void *pvFWImage)
{
	PVRSRV_ERROR eError;
	PVRSRV_RGXDEV_INFO *psDevInfo = (PVRSRV_RGXDEV_INFO*) psDeviceNode->pvDevice;
	IMG_UINT64 *pui64BootConfig;
	IMG_DEV_PHYADDR sPhyAddr;
	IMG_BOOL bValid;

	/* To get a pointer to the bootloader configuration data start from a pointer to the FW image... */
	pui64BootConfig =  (IMG_UINT64 *) pvFWImage;

	/* ... jump to the boot/NMI data page... */
	pui64BootConfig += RGXMIPSFW_GET_OFFSET_IN_QWORDS(RGXMIPSFW_BOOT_NMI_DATA_BASE_PAGE * RGXMIPSFW_PAGE_SIZE);

	/* ... and then jump to the bootloader data offset within the page */
	pui64BootConfig += RGXMIPSFW_GET_OFFSET_IN_QWORDS(RGXMIPSFW_BOOTLDR_CONF_OFFSET);

#if defined(SUPPORT_ALT_REGBASE)
	pui64BootConfig[RGXMIPSFW_ROGUE_REGS_BASE_PHYADDR_OFFSET] = psDeviceNode->psDevConfig->sAltRegsCpuPBase.uiAddr;
#else
	/* Rogue Registers physical address */
	PhysHeapCpuPAddrToDevPAddr(psDevInfo->psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL],
							   1, &sPhyAddr, &(psDeviceNode->psDevConfig->sRegsCpuPBase));
	pui64BootConfig[RGXMIPSFW_ROGUE_REGS_BASE_PHYADDR_OFFSET] = sPhyAddr.uiAddr;
#endif

	/* MIPS Page Table physical address. There are 16 pages for a firmware heap of 32 MB */
	MMU_AcquireBaseAddr(psDevInfo->psKernelMMUCtx, &sPhyAddr);
	pui64BootConfig[RGXMIPSFW_PAGE_TABLE_BASE_PHYADDR_OFFSET] = sPhyAddr.uiAddr;

	/* MIPS Stack Pointer Physical Address */
	eError = RGXGetPhyAddr(psDevInfo->psRGXFWDataMemDesc->psImport->hPMR,
						   &sPhyAddr,
						   RGXMIPSFW_STACK_OFFSET,
						   RGXMIPSFW_LOG2_PAGE_SIZE,
						   1,
						   &bValid);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXBootldrDataInit: RGXGetPhyAddr failed (%u)",
				eError));
		return eError;
	}
	pui64BootConfig[RGXMIPSFW_STACKPOINTER_PHYADDR_OFFSET] = sPhyAddr.uiAddr;

	/* Reserved for future use */
	pui64BootConfig[RGXMIPSFW_RESERVED_FUTURE_OFFSET] = 0;

	/* FW Init Data Structure Virtual Address */
	pui64BootConfig[RGXMIPSFW_FWINIT_VIRTADDR_OFFSET] = psDevInfo->psRGXFWIfInitMemDesc->sDeviceMemDesc.sDevVAddr.uiAddr;

	return PVRSRV_OK;
}

#if defined(PDUMP)
static PVRSRV_ERROR RGXPDumpBootldrData(PVRSRV_DEVICE_NODE *psDeviceNode,
                                        PVRSRV_RGXDEV_INFO *psDevInfo)
{
	PMR *psFWDataPMR;
	IMG_DEV_PHYADDR sTmpAddr;
	IMG_UINT32 ui32BootConfOffset, ui32ParamOffset;
	PVRSRV_ERROR eError;
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_GUEST, PVRSRV_OK);

	psFWDataPMR = (PMR *)(psDevInfo->psRGXFWDataMemDesc->psImport->hPMR);
	ui32BootConfOffset = (RGXMIPSFW_BOOT_NMI_DATA_BASE_PAGE * RGXMIPSFW_PAGE_SIZE);
	ui32BootConfOffset += RGXMIPSFW_BOOTLDR_CONF_OFFSET;

	/* The physical addresses used by a pdump player will be different
	 * than the ones we have put in the MIPS bootloader configuration data.
	 * We have to tell the pdump player to replace the original values with the real ones.
	 */
	PDUMPCOMMENT("Pass new boot parameters to the FW");

	/* Rogue Registers physical address */
	ui32ParamOffset = ui32BootConfOffset + (RGXMIPSFW_ROGUE_REGS_BASE_PHYADDR_OFFSET * sizeof(IMG_UINT64));

	eError = PDumpRegLabelToMem64(RGX_PDUMPREG_NAME,
	                              0x0,
	                              psFWDataPMR,
	                              ui32ParamOffset,
	                              PDUMP_FLAGS_CONTINUOUS);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXPDumpBootldrData: Dump of Rogue registers phy address failed (%u)", eError));
		return eError;
	}

	/* Page Table physical Address */
	ui32ParamOffset = ui32BootConfOffset + (RGXMIPSFW_PAGE_TABLE_BASE_PHYADDR_OFFSET * sizeof(IMG_UINT64));

	MMU_AcquireBaseAddr(psDevInfo->psKernelMMUCtx, &sTmpAddr);

	eError = PDumpPTBaseObjectToMem64(psDeviceNode->psFirmwareMMUDevAttrs->pszMMUPxPDumpMemSpaceName,
	                                  psFWDataPMR,
	                                  0,
	                                  ui32ParamOffset,
	                                  PDUMP_FLAGS_CONTINUOUS,
	                                  MMU_LEVEL_1,
	                                  sTmpAddr.uiAddr);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXPDumpBootldrData: Dump of page tables phy address failed (%u)", eError));
		return eError;
	}

	/* Stack physical address */
	ui32ParamOffset = ui32BootConfOffset + (RGXMIPSFW_STACKPOINTER_PHYADDR_OFFSET * sizeof(IMG_UINT64));

	eError = PDumpMemLabelToMem64(psFWDataPMR,
	                              psFWDataPMR,
	                              RGXMIPSFW_STACK_OFFSET,
	                              ui32ParamOffset,
	                              PDUMP_FLAGS_CONTINUOUS);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXPDumpBootldrData: Dump of stack phy address failed (%u)", eError));
		return eError;
	}

	return eError;
}
#endif /* PDUMP */


PVRSRV_ERROR PVRSRVGPUVIRTPopulateLMASubArenasKM(PVRSRV_DEVICE_NODE	*psDeviceNode,
												 IMG_UINT32          aui32OSidMin[GPUVIRT_VALIDATION_NUM_REGIONS][GPUVIRT_VALIDATION_NUM_OS],
												 IMG_UINT32          aui32OSidMax[GPUVIRT_VALIDATION_NUM_REGIONS][GPUVIRT_VALIDATION_NUM_OS],
                                                 IMG_BOOL            bEnableTrustedDeviceAceConfig)
{
	PVRSRV_RGXDEV_INFO *psDevInfo;
	psDevInfo = (PVRSRV_RGXDEV_INFO *) psDeviceNode->pvDevice;

#if defined(SUPPORT_GPUVIRT_VALIDATION)
{
	IMG_UINT32	ui32OS, ui32Region;

	for (ui32OS = 0; ui32OS < GPUVIRT_VALIDATION_NUM_OS; ui32OS++)
	{
		for (ui32Region = 0; ui32Region < GPUVIRT_VALIDATION_NUM_REGIONS; ui32Region++)
		{
			PVR_DPF((PVR_DBG_MESSAGE,"OS=%u, Region=%u, Min=%u, Max=%u", ui32OS, ui32Region, aui32OSidMin[ui32OS][ui32Region], aui32OSidMax[ui32OS][ui32Region]));
		}
	}

	PopulateLMASubArenas(psDeviceNode, aui32OSidMin, aui32OSidMax);

    #if defined(EMULATOR)
    if ((bEnableTrustedDeviceAceConfig) && (RGX_IS_FEATURE_SUPPORTED(psDevInfo, AXI_ACELITE)))
    {
        SetTrustedDeviceAceEnabled();
    }
    #else
    {
        PVR_UNREFERENCED_PARAMETER(bEnableTrustedDeviceAceConfig);
    }
    #endif
    }
#else
{
	PVR_UNREFERENCED_PARAMETER(psDeviceNode);
	PVR_UNREFERENCED_PARAMETER(aui32OSidMin);
	PVR_UNREFERENCED_PARAMETER(aui32OSidMax);
    PVR_UNREFERENCED_PARAMETER(bEnableTrustedDeviceAceConfig);
}
#endif

	return PVRSRV_OK;
}

static PVRSRV_ERROR RGXSetPowerParams(PVRSRV_RGXDEV_INFO   *psDevInfo,
                                      PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_ERROR eError;

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
	}else
	{
		PMR *psFWCodePMR = (PMR *)(psDevInfo->psRGXFWCodeMemDesc->psImport->hPMR);
		PMR *psFWDataPMR = (PMR *)(psDevInfo->psRGXFWDataMemDesc->psImport->hPMR);
		IMG_DEV_PHYADDR sPhyAddr;
		IMG_BOOL bValid;

#if defined(SUPPORT_ALT_REGBASE)
		psDevInfo->sLayerParams.sGPURegAddr.uiAddr = psDevConfig->sAltRegsCpuPBase.uiAddr;
#else
		/* The physical address of the GPU registers needs to be translated
		 * in case we are in a LMA scenario
		 */
		PhysHeapCpuPAddrToDevPAddr(psDevInfo->psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL],
		                           1,
		                           &sPhyAddr,
		                           &(psDevConfig->sRegsCpuPBase));

		psDevInfo->sLayerParams.sGPURegAddr = sPhyAddr;
#endif

		eError = RGXGetPhyAddr(psFWCodePMR,
		                       &sPhyAddr,
		                       RGXMIPSFW_BOOT_NMI_CODE_BASE_PAGE * RGXMIPSFW_PAGE_SIZE,
		                       RGXMIPSFW_LOG2_PAGE_SIZE,
		                       1,
		                       &bValid);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "RGXSetPowerParams: Failed to acquire FW boot/NMI code address"));
			return eError;
		}

		psDevInfo->sLayerParams.sBootRemapAddr = sPhyAddr;

		eError = RGXGetPhyAddr(psFWDataPMR,
		                       &sPhyAddr,
		                       RGXMIPSFW_BOOT_NMI_DATA_BASE_PAGE * RGXMIPSFW_PAGE_SIZE,
		                       RGXMIPSFW_LOG2_PAGE_SIZE,
		                       1,
		                       &bValid);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "RGXSetPowerParams: Failed to acquire FW boot/NMI data address"));
			return eError;
		}

		psDevInfo->sLayerParams.sDataRemapAddr = sPhyAddr;

		eError = RGXGetPhyAddr(psFWCodePMR,
		                       &sPhyAddr,
		                       RGXMIPSFW_EXCEPTIONSVECTORS_BASE_PAGE * RGXMIPSFW_PAGE_SIZE,
		                       RGXMIPSFW_LOG2_PAGE_SIZE,
		                       1,
		                       &bValid);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "RGXSetPowerParams: Failed to acquire FW exceptions address"));
			return eError;
		}

		psDevInfo->sLayerParams.sCodeRemapAddr = sPhyAddr;

		psDevInfo->sLayerParams.sTrampolineRemapAddr.uiAddr = psDevInfo->sTrampoline.sPhysAddr.uiAddr;

#if defined(SUPPORT_DEVICE_PA0_AS_VALID)
		psDevInfo->sLayerParams.bDevicePA0IsValid = psDevConfig->bDevicePA0IsValid;
#else
#if defined(LMA) || defined(TC_MEMORY_CONFIG)
		/*
		 * On LMA system, there is a high chance that address 0x0 is used by the GPU, e.g. TC.
		 * In that case we don't need to protect the spurious MIPS accesses to address 0x0,
		 * since that's a valid address to access.
		 * The TC is usually built with HYBRID memory, but even in UMA we do not need
		 * to apply the WA code on that system, so disable it to simplify.
		 */
		psDevInfo->sLayerParams.bDevicePA0IsValid = IMG_TRUE;
#else
		psDevInfo->sLayerParams.bDevicePA0IsValid = IMG_FALSE;
#endif
#endif


	}

#if defined(SUPPORT_TRUSTED_DEVICE) && !defined(NO_HARDWARE)
	/* Send information used on power transitions to the trusted device as
	 * in this setup the driver cannot start/stop the GPU and perform resets
	 */
	if (psDevConfig->pfnTDSetPowerParams)
	{
		PVRSRV_TD_POWER_PARAMS sTDPowerParams;

		if (RGX_IS_FEATURE_VALUE_SUPPORTED(psDevInfo, META))
		{
			sTDPowerParams.sPCAddr = psDevInfo->sLayerParams.sPCAddr;
		}else
		{
			sTDPowerParams.sGPURegAddr    = psDevInfo->sLayerParams.sGPURegAddr;
			sTDPowerParams.sBootRemapAddr = psDevInfo->sLayerParams.sBootRemapAddr;
			sTDPowerParams.sCodeRemapAddr = psDevInfo->sLayerParams.sCodeRemapAddr;
			sTDPowerParams.sDataRemapAddr = psDevInfo->sLayerParams.sDataRemapAddr;
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

PVRSRV_ERROR PVRSRVRGXInitReleaseFWInitResourcesKM(PVRSRV_DEVICE_NODE *psDeviceNode,
												   PMR *psFWCodePMR,
												   PMR *psFWDataPMR,
												   PMR *psFWCorePMR,
												   PMR *psHWPerfPMR)
{
	/* provide a stub interface for the direct bridge */
	PVR_UNREFERENCED_PARAMETER(psDeviceNode);
	PVR_UNREFERENCED_PARAMETER(psFWCodePMR);
	PVR_UNREFERENCED_PARAMETER(psFWDataPMR);
	PVR_UNREFERENCED_PARAMETER(psFWCorePMR);
	PVR_UNREFERENCED_PARAMETER(psHWPerfPMR);

	return PVRSRV_OK;
}

/*
 * PVRSRVRGXInitDevPart2KM
 */
PVRSRV_ERROR PVRSRVRGXInitDevPart2KM (PVRSRV_DEVICE_NODE	*psDeviceNode,
									  IMG_UINT32			ui32DeviceFlags,
									  IMG_UINT32			ui32HWPerfHostBufSizeKB,
									  IMG_UINT32			ui32HWPerfHostFilter,
									  RGX_ACTIVEPM_CONF		eActivePMConf)
{
	PVRSRV_ERROR			eError;
	PVRSRV_RGXDEV_INFO		*psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_DEV_POWER_STATE	eDefaultPowerState = PVRSRV_DEV_POWER_STATE_ON;
	PVRSRV_DEVICE_CONFIG	*psDevConfig = psDeviceNode->psDevConfig;

#if defined(PDUMP)
	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, MIPS))
	{
		RGXPDumpBootldrData(psDeviceNode, psDevInfo);
	}
#endif
#if defined(TIMING) || defined(DEBUG)
	OSUserModeAccessToPerfCountersEn();
#endif

	PDUMPCOMMENT("RGX Initialisation Part 2");

	psDevInfo->ui32RegSize = psDevConfig->ui32RegsSize;
	psDevInfo->sRegsPhysBase = psDevConfig->sRegsCpuPBase;

	/* Initialise Device Flags */
	psDevInfo->ui32DeviceFlags = 0;
	RGXSetDeviceFlags(psDevInfo, ui32DeviceFlags, IMG_TRUE);

	/* Allocate DVFS Table (needs to be allocated before SUPPORT_GPUTRACE_EVENTS
	 * is initialised because there is a dependency between them) */
	psDevInfo->psGpuDVFSTable = OSAllocZMem(sizeof(*(psDevInfo->psGpuDVFSTable)));
	if (psDevInfo->psGpuDVFSTable == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXInitDevPart2KM: failed to allocate gpu dvfs table storage"));
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

	/* Initialise lists of ZSBuffers */
	eError = OSLockCreate(&psDevInfo->hLockZSBuffer,LOCK_TYPE_PASSIVE);
	PVR_ASSERT(eError == PVRSRV_OK);
	dllist_init(&psDevInfo->sZSBufferHead);
	psDevInfo->ui32ZSBufferCurrID = 1;

	/* Initialise lists of growable Freelists */
	eError = OSLockCreate(&psDevInfo->hLockFreeList,LOCK_TYPE_PASSIVE);
	PVR_ASSERT(eError == PVRSRV_OK);
	dllist_init(&psDevInfo->sFreeListHead);
	psDevInfo->ui32FreelistCurrID = 1;

#if defined(SUPPORT_RAY_TRACING)
	eError = OSLockCreate(&psDevInfo->hLockRPMFreeList,LOCK_TYPE_PASSIVE);
	PVR_ASSERT(eError == PVRSRV_OK);
	dllist_init(&psDevInfo->sRPMFreeListHead);
	psDevInfo->ui32RPMFreelistCurrID = 1;
	eError = OSLockCreate(&psDevInfo->hLockRPMContext,LOCK_TYPE_PASSIVE);
	PVR_ASSERT(eError == PVRSRV_OK);
#endif

#if defined(SUPPORT_PAGE_FAULT_DEBUG)
	eError = OSLockCreate(&psDevInfo->hDebugFaultInfoLock, LOCK_TYPE_PASSIVE);

	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = OSLockCreate(&psDevInfo->hMMUCtxUnregLock, LOCK_TYPE_PASSIVE);

	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif

	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, MIPS))
	{
		eError = OSLockCreate(&psDevInfo->hNMILock, LOCK_TYPE_DISPATCH);

		if (eError != PVRSRV_OK)
		{
			return eError;
		}
	}

	/* Setup GPU utilisation stats update callback */
#if !defined(NO_HARDWARE)
	psDevInfo->pfnGetGpuUtilStats = RGXGetGpuUtilStats;
#endif

#ifndef ENABLE_COMMON_DVFS
	eError = OSLockCreate(&psDevInfo->hGPUUtilLock, LOCK_TYPE_PASSIVE);
	PVR_ASSERT(eError == PVRSRV_OK);
#else
	spin_lock_init(&psDevInfo->sGPUUtilLock);
#endif

	eDefaultPowerState = PVRSRV_DEV_POWER_STATE_ON;
	psDevInfo->eActivePMConf = eActivePMConf;

	/* set-up the Active Power Mgmt callback */
#if !defined(NO_HARDWARE)
	{
		RGX_DATA *psRGXData = (RGX_DATA*) psDeviceNode->psDevConfig->hDevData;
		IMG_BOOL bSysEnableAPM = psRGXData->psRGXTimingInfo->bEnableActivePM;
		IMG_BOOL bEnableAPM = ((eActivePMConf == RGX_ACTIVEPM_DEFAULT) && bSysEnableAPM) ||
							   (eActivePMConf == RGX_ACTIVEPM_FORCE_ON);
		/* Disable APM if in VZ mode */
		bEnableAPM = bEnableAPM && PVRSRV_VZ_MODE_IS(DRIVER_MODE_NATIVE);

		if (bEnableAPM)
		{
			eError = OSInstallMISR(&psDevInfo->pvAPMISRData, RGX_MISRHandler_CheckFWActivePowerState, psDeviceNode);
			if (eError != PVRSRV_OK)
			{
				return eError;
			}

			/* Prevent the device being woken up before there is something to do. */
			eDefaultPowerState = PVRSRV_DEV_POWER_STATE_OFF;
		}
	}
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
									   &RGXDustCountChange,
									   (IMG_HANDLE)psDeviceNode,
									   PVRSRV_DEV_POWER_STATE_OFF,
									   eDefaultPowerState);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXInitDevPart2KM: failed to register device with power manager"));
		return eError;
	}

	eError = RGXSetPowerParams(psDevInfo, psDevConfig);
	if (eError != PVRSRV_OK) return eError;

#if defined(PDUMP)
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
		return eError;
	}

	/* Register the interrupt handlers */
	eError = OSInstallMISR(&psDevInfo->pvMISRData, RGX_MISRHandler_Main, psDeviceNode);
	if (eError != PVRSRV_OK)
	{
		if (psDevInfo->pvAPMISRData != NULL)
		{
			(void) OSUninstallMISR(psDevInfo->pvAPMISRData);
		}
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
		(void) OSUninstallMISR(psDevInfo->hProcessQueuesMISR);
		(void) OSUninstallMISR(psDevInfo->pvMISRData);
		return eError;
	}
#endif

#if defined(SUPPORT_PDVFS) && !defined(RGXFW_META_SUPPORT_2ND_THREAD)
	psDeviceNode->psDevConfig->sDVFS.sPDVFSData.hReactiveTimer =
						OSAddTimer((PFN_TIMER_FUNC)PDVFSRequestReactiveUpdate,
								   psDevInfo,
								   PDVFS_REACTIVE_INTERVAL_MS);

	OSEnableTimer(psDeviceNode->psDevConfig->sDVFS.sPDVFSData.hReactiveTimer);
#endif

#if defined(PDUMP)
	if(!(RGX_IS_FEATURE_SUPPORTED(psDevInfo, S7_CACHE_HIERARCHY)))
	{
		if (!PVRSRVSystemSnoopingOfCPUCache(psDevConfig) &&
			!PVRSRVSystemSnoopingOfDeviceCache(psDevConfig))
		{
			PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "System has NO cache snooping");
		}
		else
		{
			if (PVRSRVSystemSnoopingOfCPUCache(psDevConfig))
			{
				PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "System has CPU cache snooping");
			}
			if (PVRSRVSystemSnoopingOfDeviceCache(psDevConfig))
			{
				PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "System has DEVICE cache snooping");
			}
		}
	}
#endif

	psDevInfo->bDevInit2Done = IMG_TRUE;

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVRGXInitHWPerfCountersKM(PVRSRV_DEVICE_NODE	*psDeviceNode)
{

	PVRSRV_ERROR			eError;
	RGXFWIF_KCCB_CMD		sKccbCmd;

	/* Fill in the command structure with the parameters needed
	 */
	sKccbCmd.eCmdType = RGXFWIF_KCCB_CMD_HWPERF_CONFIG_ENABLE_BLKS_DIRECT;

	eError = RGXSendCommandWithPowLock(psDeviceNode->pvDevice,
											RGXFWIF_DM_GP,
											&sKccbCmd,
											sizeof(sKccbCmd),
											PDUMP_FLAGS_CONTINUOUS);

	return PVRSRV_OK;

}

static PVRSRV_ERROR RGXInitCreateFWKernelMemoryContext(PVRSRV_DEVICE_NODE	*psDeviceNode)
{
	/* set up fw memory contexts */
	PVRSRV_RGXDEV_INFO 	*psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_ERROR        eError;

	/* Register callbacks for creation of device memory contexts */
	psDeviceNode->pfnRegisterMemoryContext = RGXRegisterMemoryContext;
	psDeviceNode->pfnUnregisterMemoryContext = RGXUnregisterMemoryContext;

	/* Create the memory context for the firmware. */
	eError = DevmemCreateContext(psDeviceNode, DEVMEM_HEAPCFG_META,
								 &psDevInfo->psKernelDevmemCtx);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXInitCreateFWKernelMemoryContext: Failed DevmemCreateContext (%u)", eError));
		goto failed_to_create_ctx;
	}

	eError = DevmemFindHeapByName(psDevInfo->psKernelDevmemCtx, RGX_FIRMWARE_MAIN_HEAP_IDENT,
					  &psDevInfo->psFirmwareMainHeap);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXInitCreateFWKernelMemoryContext: Failed DevmemFindHeapByName (%u)", eError));
		goto failed_to_find_heap;
	}

	eError = DevmemFindHeapByName(psDevInfo->psKernelDevmemCtx, RGX_FIRMWARE_CONFIG_HEAP_IDENT,
					  &psDevInfo->psFirmwareConfigHeap);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXInitCreateFWKernelMemoryContext: Failed DevmemFindHeapByName (%u)", eError));
		goto failed_to_find_heap;
	}

	/* Perform additional vz specific initialization */
	eError = RGXVzInitCreateFWKernelMemoryContext(psDeviceNode);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "RGXInitCreateFWKernelMemoryContext: Failed RGXVzInitCreateFWKernelMemoryContext (%u)",
				 eError));
		goto failed_to_find_heap;
	}

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

static void RGXDeInitDestroyFWKernelMemoryContext(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_RGXDEV_INFO 	*psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_ERROR        eError;

	RGXVzDeInitDestroyFWKernelMemoryContext(psDeviceNode);

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

#if defined(RGXFW_ALIGNCHECKS)
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
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVAlignmentCheckKM: FW Alignment Check"
		        " Mem Descriptor is NULL"));
		return PVRSRV_ERROR_ALIGNMENT_ARRAY_NOT_AVAILABLE;
	}

	eError = DevmemAcquireCpuVirtAddr(psDevInfo->psRGXFWAlignChecksMemDesc,
	                                  (void **) &paui32FWAlignChecks);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVAlignmentCheckKM: Failed to acquire"
		        " kernel address for alignment checks (%u)", eError));
		return eError;
	}

	paui32FWAlignChecks += ARRAY_SIZE(aui32AlignChecksKM) + 1;
	if (*paui32FWAlignChecks++ != ui32AlignChecksSize)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVAlignmentCheckKM: Mismatch"
					" in number of structures to check."));
		eError = PVRSRV_ERROR_INVALID_ALIGNMENT;
		goto return_;
	}

	for (i = 0; i < ui32AlignChecksSize; i++)
	{
		if (aui32AlignChecks[i] != paui32FWAlignChecks[i])
		{
			PVR_DPF((PVR_DBG_ERROR, "PVRSRVAlignmentCheckKM: Check for"
					" structured alignment failed."));
			eError = PVRSRV_ERROR_INVALID_ALIGNMENT;
			goto return_;
		}
	}

return_:

	DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWAlignChecksMemDesc);

	return eError;
}
#endif

static
PVRSRV_ERROR RGXAllocateFWCodeRegion(PVRSRV_DEVICE_NODE *psDeviceNode,
                                     IMG_DEVMEM_SIZE_T ui32FWCodeAllocSize,
                                     IMG_UINT32 uiMemAllocFlags,
                                     IMG_BOOL bFWCorememCode,
                                     const IMG_PCHAR pszText,
                                     DEVMEM_MEMDESC **ppsMemDescPtr)
{
	PVRSRV_ERROR eError;
	IMG_DEVMEM_LOG2ALIGN_T uiLog2Align = OSGetPageShift();

#if defined(SUPPORT_TRUSTED_DEVICE)
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, MIPS))
	{
		uiLog2Align = RGXMIPSFW_LOG2_PAGE_SIZE_64K;
	}
#endif

#if !defined(SUPPORT_TRUSTED_DEVICE)
	uiMemAllocFlags |= PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE |
	                   PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC;

	PVR_UNREFERENCED_PARAMETER(bFWCorememCode);

	PDUMPCOMMENT("Allocate and export FW %s memory",
	             bFWCorememCode? "coremem code" : "code");

	eError = DevmemFwAllocateExportable(psDeviceNode,
	                                    ui32FWCodeAllocSize,
	                                    1 << uiLog2Align,
	                                    uiMemAllocFlags,
	                                    pszText,
	                                    ppsMemDescPtr);
	return eError;
#else
	PDUMPCOMMENT("Import secure FW %s memory",
	             bFWCorememCode? "coremem code" : "code");

	eError = DevmemImportTDFWCode(psDeviceNode,
	                              ui32FWCodeAllocSize,
	                              uiLog2Align,
	                              uiMemAllocFlags,
	                              bFWCorememCode,
	                              ppsMemDescPtr);
	return eError;
#endif
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

 @Input psRGXFWInit - FW init data

 @Return   PVRSRV_ERROR - depending on mismatch found

******************************************************************************/
static PVRSRV_ERROR RGXDevInitCompatCheck_KMBuildOptions_FWAgainstDriver(RGXFWIF_INIT *psRGXFWInit)
{
#if !defined(NO_HARDWARE)
	IMG_UINT32			ui32BuildOptions, ui32BuildOptionsFWKMPart, ui32BuildOptionsMismatch;

	if (psRGXFWInit == NULL)
		return PVRSRV_ERROR_INVALID_PARAMS;

	ui32BuildOptions = (RGX_BUILD_OPTIONS_KM);

	ui32BuildOptionsFWKMPart = psRGXFWInit->sRGXCompChecks.ui32BuildOptions & RGX_BUILD_OPTIONS_MASK_KM;

	if (ui32BuildOptions != ui32BuildOptionsFWKMPart)
	{
		ui32BuildOptionsMismatch = ui32BuildOptions ^ ui32BuildOptionsFWKMPart;
#if !defined(PVRSRV_STRICT_COMPAT_CHECK)
		/*Mask the debug flag option out as we do support combinations of debug vs release in um & km*/
		ui32BuildOptionsMismatch &= OPTIONS_STRICT;
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
 @Input psRGXFWInit - FW init data

 @Return   PVRSRV_ERROR - depending on mismatch found

******************************************************************************/
static PVRSRV_ERROR RGXDevInitCompatCheck_DDKVersion_FWAgainstDriver(PVRSRV_RGXDEV_INFO *psDevInfo,
																			RGXFWIF_INIT *psRGXFWInit)
{
#if defined(PDUMP)||(!defined(NO_HARDWARE))
	IMG_UINT32			ui32DDKVersion;
	PVRSRV_ERROR		eError;

	ui32DDKVersion = PVRVERSION_PACK(PVRVERSION_MAJ, PVRVERSION_MIN);
#endif

#if defined(PDUMP)
	PDUMPCOMMENT("Compatibility check: KM driver and FW DDK version");
	eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfInitMemDesc,
												offsetof(RGXFWIF_INIT, sRGXCompChecks) +
												offsetof(RGXFWIF_COMPCHECKS, ui32DDKVersion),
												ui32DDKVersion,
												0xffffffff,
												PDUMP_POLL_OPERATOR_EQUAL,
												PDUMP_FLAGS_CONTINUOUS);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfInitMemDesc (%d)", eError));
		return eError;
	}
#endif

#if !defined(NO_HARDWARE)
	if (psRGXFWInit == NULL)
		return PVRSRV_ERROR_INVALID_PARAMS;

	if (psRGXFWInit->sRGXCompChecks.ui32DDKVersion != ui32DDKVersion)
	{
		PVR_LOG(("(FAIL) RGXDevInitCompatCheck: Incompatible driver DDK version (%u.%u) / Firmware version (%u.%u).",
				PVRVERSION_MAJ, PVRVERSION_MIN,
				PVRVERSION_UNPACK_MAJ(psRGXFWInit->sRGXCompChecks.ui32DDKVersion),
				PVRVERSION_UNPACK_MIN(psRGXFWInit->sRGXCompChecks.ui32DDKVersion)));
		eError = PVRSRV_ERROR_DDK_VERSION_MISMATCH;
		PVR_DBG_BREAK;
		return eError;
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE, "RGXDevInitCompatCheck: driver DDK version (%u.%u) and Firmware version (%u.%u) match. [ OK ]",
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
 @Input psRGXFWInit - FW init data

 @Return   PVRSRV_ERROR - depending on mismatch found

******************************************************************************/
static PVRSRV_ERROR RGXDevInitCompatCheck_DDKBuild_FWAgainstDriver(PVRSRV_RGXDEV_INFO *psDevInfo,
																			RGXFWIF_INIT *psRGXFWInit)
{
	PVRSRV_ERROR		eError=PVRSRV_OK;
#if defined(PDUMP)||(!defined(NO_HARDWARE))
	IMG_UINT32			ui32DDKBuild;

	ui32DDKBuild = PVRVERSION_BUILD;
#endif

#if defined(PDUMP) && defined(PVRSRV_STRICT_COMPAT_CHECK)
	PDUMPCOMMENT("Compatibility check: KM driver and FW DDK build");
	eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfInitMemDesc,
												offsetof(RGXFWIF_INIT, sRGXCompChecks) +
												offsetof(RGXFWIF_COMPCHECKS, ui32DDKBuild),
												ui32DDKBuild,
												0xffffffff,
												PDUMP_POLL_OPERATOR_EQUAL,
												PDUMP_FLAGS_CONTINUOUS);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfInitMemDesc (%d)", eError));
		return eError;
	}
#endif

#if !defined(NO_HARDWARE)
	if (psRGXFWInit == NULL)
		return PVRSRV_ERROR_INVALID_PARAMS;

	if (psRGXFWInit->sRGXCompChecks.ui32DDKBuild != ui32DDKBuild)
	{
		PVR_LOG(("(WARN) RGXDevInitCompatCheck: Different driver DDK build version (%d) / Firmware DDK build version (%d).",
				ui32DDKBuild, psRGXFWInit->sRGXCompChecks.ui32DDKBuild));
#if defined(PVRSRV_STRICT_COMPAT_CHECK)
		eError = PVRSRV_ERROR_DDK_BUILD_MISMATCH;
		PVR_DBG_BREAK;
		return eError;
#endif
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE, "RGXDevInitCompatCheck: driver DDK build version (%d) and Firmware DDK build version (%d) match. [ OK ]",
				ui32DDKBuild, psRGXFWInit->sRGXCompChecks.ui32DDKBuild));
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
 @Input psRGXFWInit - FW init data

 @Return   PVRSRV_ERROR - depending on mismatch found

******************************************************************************/
static PVRSRV_ERROR RGXDevInitCompatCheck_BVNC_FWAgainstDriver(PVRSRV_RGXDEV_INFO *psDevInfo,
																			RGXFWIF_INIT *psRGXFWInit)
{
#if defined(PDUMP)
	IMG_UINT32					i;
#endif
#if !defined(NO_HARDWARE)
	IMG_BOOL bCompatibleAll, bCompatibleVersion, bCompatibleLenMax, bCompatibleBNC, bCompatibleV;
#endif
#if defined(PDUMP)||(!defined(NO_HARDWARE))
	IMG_UINT32					ui32B, ui32V, ui32N, ui32C;
	RGXFWIF_COMPCHECKS_BVNC_DECLARE_AND_INIT(sBVNC);
	PVRSRV_ERROR				eError;
	IMG_CHAR	szV[8];

	ui32B = psDevInfo->sDevFeatureCfg.ui32B;
	ui32V = psDevInfo->sDevFeatureCfg.ui32V;
	ui32N = psDevInfo->sDevFeatureCfg.ui32N;
	ui32C = psDevInfo->sDevFeatureCfg.ui32C;

	OSSNPrintf(szV, sizeof(szV),"%u",ui32V);

	rgx_bvnc_packed(&sBVNC.ui64BNC, sBVNC.aszV, sBVNC.ui32VLenMax, ui32B, szV, ui32N, ui32C);
#endif

#if defined(PDUMP)
	PDUMPCOMMENT("Compatibility check: KM driver and FW BVNC (struct version)");
	eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfInitMemDesc,
											offsetof(RGXFWIF_INIT, sRGXCompChecks) +
											offsetof(RGXFWIF_COMPCHECKS, sFWBVNC) +
											offsetof(RGXFWIF_COMPCHECKS_BVNC, ui32LayoutVersion),
											sBVNC.ui32LayoutVersion,
											0xffffffff,
											PDUMP_POLL_OPERATOR_EQUAL,
											PDUMP_FLAGS_CONTINUOUS);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfInitMemDesc (%d)", eError));
	}

	PDUMPCOMMENT("Compatibility check: KM driver and FW BVNC (maxlen)");
	eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfInitMemDesc,
											offsetof(RGXFWIF_INIT, sRGXCompChecks) +
											offsetof(RGXFWIF_COMPCHECKS, sFWBVNC) +
											offsetof(RGXFWIF_COMPCHECKS_BVNC, ui32VLenMax),
											sBVNC.ui32VLenMax,
											0xffffffff,
											PDUMP_POLL_OPERATOR_EQUAL,
											PDUMP_FLAGS_CONTINUOUS);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfInitMemDesc (%d)", eError));
	}

	PDUMPCOMMENT("Compatibility check: KM driver and FW BVNC (BNC part - lower 32 bits)");
	eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfInitMemDesc,
											offsetof(RGXFWIF_INIT, sRGXCompChecks) +
											offsetof(RGXFWIF_COMPCHECKS, sFWBVNC) +
											offsetof(RGXFWIF_COMPCHECKS_BVNC, ui64BNC),
											(IMG_UINT32)sBVNC.ui64BNC,
											0xffffffff,
											PDUMP_POLL_OPERATOR_EQUAL,
											PDUMP_FLAGS_CONTINUOUS);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfInitMemDesc (%d)", eError));
	}

	PDUMPCOMMENT("Compatibility check: KM driver and FW BVNC (BNC part - Higher 32 bits)");
	eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfInitMemDesc,
											offsetof(RGXFWIF_INIT, sRGXCompChecks) +
											offsetof(RGXFWIF_COMPCHECKS, sFWBVNC) +
											offsetof(RGXFWIF_COMPCHECKS_BVNC, ui64BNC) +
											sizeof(IMG_UINT32),
											(IMG_UINT32)(sBVNC.ui64BNC >> 32),
											0xffffffff,
											PDUMP_POLL_OPERATOR_EQUAL,
											PDUMP_FLAGS_CONTINUOUS);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfInitMemDesc (%d)", eError));
	}

	for (i = 0; i < sBVNC.ui32VLenMax; i += sizeof(IMG_UINT32))
	{
		PDUMPCOMMENT("Compatibility check: KM driver and FW BVNC (V part)");
		eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfInitMemDesc,
												offsetof(RGXFWIF_INIT, sRGXCompChecks) +
												offsetof(RGXFWIF_COMPCHECKS, sFWBVNC) +
												offsetof(RGXFWIF_COMPCHECKS_BVNC, aszV) +
												i,
												*((IMG_UINT32 *)(sBVNC.aszV + i)),
												0xffffffff,
												PDUMP_POLL_OPERATOR_EQUAL,
												PDUMP_FLAGS_CONTINUOUS);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfInitMemDesc (%d)", eError));
		}
	}
#endif

#if !defined(NO_HARDWARE)
	if (psRGXFWInit == NULL)
		return PVRSRV_ERROR_INVALID_PARAMS;

	RGX_BVNC_EQUAL(sBVNC, psRGXFWInit->sRGXCompChecks.sFWBVNC, bCompatibleAll, bCompatibleVersion, bCompatibleLenMax, bCompatibleBNC, bCompatibleV);

	if (!bCompatibleAll)
	{
		if (!bCompatibleVersion)
		{
			PVR_LOG(("(FAIL) %s: Incompatible compatibility struct version of driver (%u) and firmware (%u).",
					__FUNCTION__,
					sBVNC.ui32LayoutVersion,
					psRGXFWInit->sRGXCompChecks.sFWBVNC.ui32LayoutVersion));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			return eError;
		}

		if (!bCompatibleLenMax)
		{
			PVR_LOG(("(FAIL) %s: Incompatible V maxlen of driver (%u) and firmware (%u).",
					__FUNCTION__,
					sBVNC.ui32VLenMax,
					psRGXFWInit->sRGXCompChecks.sFWBVNC.ui32VLenMax));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			return eError;
		}

		if (!bCompatibleBNC)
		{
			PVR_LOG(("(FAIL) RGXDevInitCompatCheck: Mismatch in KM driver BNC (%u._.%u.%u) and Firmware BNC (%u._.%u.%u)",
					RGX_BVNC_PACKED_EXTR_B(sBVNC),
					RGX_BVNC_PACKED_EXTR_N(sBVNC),
					RGX_BVNC_PACKED_EXTR_C(sBVNC),
					RGX_BVNC_PACKED_EXTR_B(psRGXFWInit->sRGXCompChecks.sFWBVNC),
					RGX_BVNC_PACKED_EXTR_N(psRGXFWInit->sRGXCompChecks.sFWBVNC),
					RGX_BVNC_PACKED_EXTR_C(psRGXFWInit->sRGXCompChecks.sFWBVNC)));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			return eError;
		}

		if (!bCompatibleV)
		{
			PVR_LOG(("(FAIL) RGXDevInitCompatCheck: Mismatch in KM driver BVNC (%u.%s.%u.%u) and Firmware BVNC (%u.%s.%u.%u)",
					RGX_BVNC_PACKED_EXTR_B(sBVNC),
					RGX_BVNC_PACKED_EXTR_V(sBVNC),
					RGX_BVNC_PACKED_EXTR_N(sBVNC),
					RGX_BVNC_PACKED_EXTR_C(sBVNC),
					RGX_BVNC_PACKED_EXTR_B(psRGXFWInit->sRGXCompChecks.sFWBVNC),
					RGX_BVNC_PACKED_EXTR_V(psRGXFWInit->sRGXCompChecks.sFWBVNC),
					RGX_BVNC_PACKED_EXTR_N(psRGXFWInit->sRGXCompChecks.sFWBVNC),
					RGX_BVNC_PACKED_EXTR_C(psRGXFWInit->sRGXCompChecks.sFWBVNC)));
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
 @Input psRGXFWInit - FW init data

 @Return   PVRSRV_ERROR - depending on mismatch found

******************************************************************************/
#if ((!defined(NO_HARDWARE))&&(!defined(EMULATOR)))
#define TARGET_SILICON  /* definition for everything that is not emu and not nohw configuration */
#endif

static PVRSRV_ERROR RGXDevInitCompatCheck_BVNC_HWAgainstDriver(PVRSRV_RGXDEV_INFO *psDevInfo,
																	RGXFWIF_INIT *psRGXFWInit)
{
#if defined(PDUMP) || defined(TARGET_SILICON)
	IMG_UINT64 ui64MaskBNC = RGX_BVNC_PACK_MASK_B |
								RGX_BVNC_PACK_MASK_N |
								RGX_BVNC_PACK_MASK_C;

	IMG_UINT32 bMaskV = IMG_FALSE;

	PVRSRV_ERROR				eError;
	RGXFWIF_COMPCHECKS_BVNC_DECLARE_AND_INIT(sSWBVNC);
#endif

#if defined(TARGET_SILICON)
	RGXFWIF_COMPCHECKS_BVNC_DECLARE_AND_INIT(sHWBVNC);
	IMG_BOOL bCompatibleAll, bCompatibleVersion, bCompatibleLenMax, bCompatibleBNC, bCompatibleV;
#endif

#if defined(PDUMP) || defined(TARGET_SILICON)
	IMG_UINT32 ui32B, ui32V, ui32N, ui32C;
	IMG_CHAR szV[8];

	/*if(RGX_DEVICE_HAS_BRN(psDevInfo, 38835))
	{
		ui64MaskBNC &= ~RGX_BVNC_PACK_MASK_B;
		bMaskV = IMG_TRUE;
	}*/
#if defined(COMPAT_BVNC_MASK_N)
	ui64MaskBNC &= ~RGX_BVNC_PACK_MASK_N;
#endif
#if defined(COMPAT_BVNC_MASK_C)
	ui64MaskBNC &= ~RGX_BVNC_PACK_MASK_C;
#endif
	ui32B = psDevInfo->sDevFeatureCfg.ui32B;
	ui32V = psDevInfo->sDevFeatureCfg.ui32V;
	ui32N = psDevInfo->sDevFeatureCfg.ui32N;
	ui32C = psDevInfo->sDevFeatureCfg.ui32C;

	OSSNPrintf(szV, sizeof(szV),"%d",ui32V);
	rgx_bvnc_packed(&sSWBVNC.ui64BNC, sSWBVNC.aszV, sSWBVNC.ui32VLenMax,  ui32B, szV, ui32N, ui32C);


	if (RGX_IS_BRN_SUPPORTED(psDevInfo, 38344) && (ui32C >= 10))
	{
		ui64MaskBNC &= ~RGX_BVNC_PACK_MASK_C;
	}

	if ((ui64MaskBNC != (RGX_BVNC_PACK_MASK_B | RGX_BVNC_PACK_MASK_N | RGX_BVNC_PACK_MASK_C)) || bMaskV)
	{
		PVR_LOG(("Compatibility checks: Ignoring fields: '%s%s%s%s' of HW BVNC.",
				((!(ui64MaskBNC & RGX_BVNC_PACK_MASK_B))?("B"):("")),
				((bMaskV)?("V"):("")),
				((!(ui64MaskBNC & RGX_BVNC_PACK_MASK_N))?("N"):("")),
				((!(ui64MaskBNC & RGX_BVNC_PACK_MASK_C))?("C"):(""))));
	}
#endif

#if defined(EMULATOR)
	PVR_LOG(("Compatibility checks for emu target: Ignoring HW BVNC checks."));
#endif

#if defined(PDUMP)
	PDUMPCOMMENT("Compatibility check: Layout version of compchecks struct");
	eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfInitMemDesc,
											offsetof(RGXFWIF_INIT, sRGXCompChecks) +
											offsetof(RGXFWIF_COMPCHECKS, sHWBVNC) +
											offsetof(RGXFWIF_COMPCHECKS_BVNC, ui32LayoutVersion),
											sSWBVNC.ui32LayoutVersion,
											0xffffffff,
											PDUMP_POLL_OPERATOR_EQUAL,
											PDUMP_FLAGS_CONTINUOUS);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfInitMemDesc (%d)", eError));
		return eError;
	}

	PDUMPCOMMENT("Compatibility check: HW V max len and FW V max len");
	eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfInitMemDesc,
											offsetof(RGXFWIF_INIT, sRGXCompChecks) +
											offsetof(RGXFWIF_COMPCHECKS, sHWBVNC) +
											offsetof(RGXFWIF_COMPCHECKS_BVNC, ui32VLenMax),
											sSWBVNC.ui32VLenMax,
											0xffffffff,
											PDUMP_POLL_OPERATOR_EQUAL,
											PDUMP_FLAGS_CONTINUOUS);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfInitMemDesc (%d)", eError));
		return eError;
	}

	if (ui64MaskBNC != 0)
	{
		PDUMPIF("DISABLE_HWBNC_CHECK");
		PDUMPELSE("DISABLE_HWBNC_CHECK");
		PDUMPCOMMENT("Compatibility check: HW BNC and FW BNC (Lower 32 bits)");
		eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfInitMemDesc,
												offsetof(RGXFWIF_INIT, sRGXCompChecks) +
												offsetof(RGXFWIF_COMPCHECKS, sHWBVNC) +
												offsetof(RGXFWIF_COMPCHECKS_BVNC, ui64BNC),
												(IMG_UINT32)sSWBVNC.ui64BNC ,
												(IMG_UINT32)ui64MaskBNC,
												PDUMP_POLL_OPERATOR_EQUAL,
												PDUMP_FLAGS_CONTINUOUS);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfInitMemDesc (%d)", eError));
			return eError;
		}

		PDUMPCOMMENT("Compatibility check: HW BNC and FW BNC (Higher 32 bits)");
		eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfInitMemDesc,
												offsetof(RGXFWIF_INIT, sRGXCompChecks) +
												offsetof(RGXFWIF_COMPCHECKS, sHWBVNC) +
												offsetof(RGXFWIF_COMPCHECKS_BVNC, ui64BNC) +
												sizeof(IMG_UINT32),
												(IMG_UINT32)(sSWBVNC.ui64BNC >> 32),
												(IMG_UINT32)(ui64MaskBNC >> 32),
												PDUMP_POLL_OPERATOR_EQUAL,
												PDUMP_FLAGS_CONTINUOUS);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfInitMemDesc (%d)", eError));
			return eError;
		}

		PDUMPFI("DISABLE_HWBNC_CHECK");
	}
	if (!bMaskV)
	{
		IMG_UINT32 i;
		PDUMPIF("DISABLE_HWV_CHECK");
		PDUMPELSE("DISABLE_HWV_CHECK");
		for (i = 0; i < sSWBVNC.ui32VLenMax; i += sizeof(IMG_UINT32))
		{
			PDUMPCOMMENT("Compatibility check: HW V and FW V");
			eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfInitMemDesc,
												offsetof(RGXFWIF_INIT, sRGXCompChecks) +
												offsetof(RGXFWIF_COMPCHECKS, sHWBVNC) +
												offsetof(RGXFWIF_COMPCHECKS_BVNC, aszV) +
												i,
												*((IMG_UINT32 *)(sSWBVNC.aszV + i)),
												0xffffffff,
												PDUMP_POLL_OPERATOR_EQUAL,
												PDUMP_FLAGS_CONTINUOUS);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfInitMemDesc (%d)", eError));
				return eError;
			}
		}
		PDUMPFI("DISABLE_HWV_CHECK");
	}
#endif

#if defined(TARGET_SILICON)
	if (psRGXFWInit == NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	sHWBVNC = psRGXFWInit->sRGXCompChecks.sHWBVNC;

	sHWBVNC.ui64BNC &= ui64MaskBNC;
	sSWBVNC.ui64BNC &= ui64MaskBNC;

	if (bMaskV)
	{
		sHWBVNC.aszV[0] = '\0';
		sSWBVNC.aszV[0] = '\0';
	}

	RGX_BVNC_EQUAL(sSWBVNC, sHWBVNC, bCompatibleAll, bCompatibleVersion, bCompatibleLenMax, bCompatibleBNC, bCompatibleV);

	if (RGX_IS_BRN_SUPPORTED(psDevInfo, 42480))
	{
		if (!bCompatibleAll && bCompatibleVersion)
		{
			if ((RGX_BVNC_PACKED_EXTR_B(sSWBVNC) == 1) &&
				!(OSStringCompare(RGX_BVNC_PACKED_EXTR_V(sSWBVNC),"76")) &&
				(RGX_BVNC_PACKED_EXTR_N(sSWBVNC) == 4) &&
				(RGX_BVNC_PACKED_EXTR_C(sSWBVNC) == 6))
			{
				if ((RGX_BVNC_PACKED_EXTR_B(sHWBVNC) == 1) &&
					!(OSStringCompare(RGX_BVNC_PACKED_EXTR_V(sHWBVNC),"69")) &&
					(RGX_BVNC_PACKED_EXTR_N(sHWBVNC) == 4) &&
					(RGX_BVNC_PACKED_EXTR_C(sHWBVNC) == 4))
				{
					bCompatibleBNC = IMG_TRUE;
					bCompatibleLenMax = IMG_TRUE;
					bCompatibleV = IMG_TRUE;
					bCompatibleAll = IMG_TRUE;
				}
			}
		}
	}

	if (!bCompatibleAll)
	{
		if (!bCompatibleVersion)
		{
			PVR_LOG(("(FAIL) %s: Incompatible compatibility struct version of HW (%d) and FW (%d).",
					__FUNCTION__,
					sHWBVNC.ui32LayoutVersion,
					sSWBVNC.ui32LayoutVersion));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			return eError;
		}

		if (!bCompatibleLenMax)
		{
			PVR_LOG(("(FAIL) %s: Incompatible V maxlen of HW (%d) and FW (%d).",
					__FUNCTION__,
					sHWBVNC.ui32VLenMax,
					sSWBVNC.ui32VLenMax));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			return eError;
		}

		if (!bCompatibleBNC)
		{
			PVR_LOG(("(FAIL) RGXDevInitCompatCheck: Incompatible HW BNC (%d._.%d.%d) and FW BNC (%d._.%d.%d).",
					RGX_BVNC_PACKED_EXTR_B(sHWBVNC),
					RGX_BVNC_PACKED_EXTR_N(sHWBVNC),
					RGX_BVNC_PACKED_EXTR_C(sHWBVNC),
					RGX_BVNC_PACKED_EXTR_B(sSWBVNC),
					RGX_BVNC_PACKED_EXTR_N(sSWBVNC),
					RGX_BVNC_PACKED_EXTR_C(sSWBVNC)));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			return eError;
		}

		if (!bCompatibleV)
		{
			PVR_LOG(("(FAIL) RGXDevInitCompatCheck: Incompatible HW BVNC (%d.%s.%d.%d) and FW BVNC (%d.%s.%d.%d).",
					RGX_BVNC_PACKED_EXTR_B(sHWBVNC),
					RGX_BVNC_PACKED_EXTR_V(sHWBVNC),
					RGX_BVNC_PACKED_EXTR_N(sHWBVNC),
					RGX_BVNC_PACKED_EXTR_C(sHWBVNC),
					RGX_BVNC_PACKED_EXTR_B(sSWBVNC),
					RGX_BVNC_PACKED_EXTR_V(sSWBVNC),
					RGX_BVNC_PACKED_EXTR_N(sSWBVNC),
					RGX_BVNC_PACKED_EXTR_C(sSWBVNC)));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			return eError;
		}
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE, "RGXDevInitCompatCheck: HW BVNC (%d.%s.%d.%d) and FW BVNC (%d.%s.%d.%d) match. [ OK ]",
				RGX_BVNC_PACKED_EXTR_B(sHWBVNC),
				RGX_BVNC_PACKED_EXTR_V(sHWBVNC),
				RGX_BVNC_PACKED_EXTR_N(sHWBVNC),
				RGX_BVNC_PACKED_EXTR_C(sHWBVNC),
				RGX_BVNC_PACKED_EXTR_B(sSWBVNC),
				RGX_BVNC_PACKED_EXTR_V(sSWBVNC),
				RGX_BVNC_PACKED_EXTR_N(sSWBVNC),
				RGX_BVNC_PACKED_EXTR_C(sSWBVNC)));
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
 @Input psRGXFWInit - FW init data

 @Return   PVRSRV_ERROR - depending on mismatch found

******************************************************************************/
static PVRSRV_ERROR RGXDevInitCompatCheck_FWProcessorVersion_AgainstDriver(PVRSRV_RGXDEV_INFO *psDevInfo,
									RGXFWIF_INIT *psRGXFWInit)
{
#if defined(PDUMP)||(!defined(NO_HARDWARE))
	PVRSRV_ERROR		eError;
#endif
	IMG_UINT32	ui32FWCoreIDValue = 0;
	IMG_CHAR *pcRGXFW_PROCESSOR = NULL;

	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, MIPS))
	{
		ui32FWCoreIDValue = RGXMIPSFW_CORE_ID_VALUE;
		pcRGXFW_PROCESSOR = RGXFW_PROCESSOR_MIPS;
	}
	else if (RGX_IS_FEATURE_VALUE_SUPPORTED(psDevInfo, META))
	{
		switch(RGX_GET_FEATURE_VALUE(psDevInfo, META))
		{
		case MTP218: ui32FWCoreIDValue = RGX_CR_META_MTP218_CORE_ID_VALUE; break;
		case MTP219: ui32FWCoreIDValue = RGX_CR_META_MTP219_CORE_ID_VALUE; break;
		case LTP218: ui32FWCoreIDValue = RGX_CR_META_LTP218_CORE_ID_VALUE; break;
		case LTP217: ui32FWCoreIDValue = RGX_CR_META_LTP217_CORE_ID_VALUE; break;
		default:
			PVR_DPF((PVR_DBG_ERROR,"%s: Undefined FW_CORE_ID_VALUE", __func__));
			PVR_ASSERT(0);
		}
		pcRGXFW_PROCESSOR = RGXFW_PROCESSOR_META;
	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR,"%s: Undefined FW_CORE_ID_VALUE", __func__));
		PVR_ASSERT(0);
	}

#if defined(PDUMP)
	PDUMPIF("DISABLE_HWMETA_CHECK");
	PDUMPELSE("DISABLE_HWMETA_CHECK");
	PDUMPCOMMENT("Compatibility check: KM driver and HW FW Processor version");
	eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfInitMemDesc,
					offsetof(RGXFWIF_INIT, sRGXCompChecks) +
					offsetof(RGXFWIF_COMPCHECKS, ui32FWProcessorVersion),
					ui32FWCoreIDValue,
					0xffffffff,
					PDUMP_POLL_OPERATOR_EQUAL,
					PDUMP_FLAGS_CONTINUOUS);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXDevInitCompatCheck: problem pdumping POL for psRGXFWIfInitMemDesc (%d)", eError));
		return eError;
	}
	PDUMPFI("DISABLE_HWMETA_CHECK");
#endif

#if !defined(NO_HARDWARE)
	if (psRGXFWInit == NULL)
		return PVRSRV_ERROR_INVALID_PARAMS;

	if (psRGXFWInit->sRGXCompChecks.ui32FWProcessorVersion != ui32FWCoreIDValue)
	{
		PVR_LOG(("RGXDevInitCompatCheck: Incompatible driver %s version (%d) / HW %s version (%d).",
				pcRGXFW_PROCESSOR,
				 ui32FWCoreIDValue,
				 pcRGXFW_PROCESSOR,
				 psRGXFWInit->sRGXCompChecks.ui32FWProcessorVersion));
		eError = PVRSRV_ERROR_FWPROCESSOR_MISMATCH;
		PVR_DBG_BREAK;
		return eError;
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE, "RGXDevInitCompatCheck: Compatible driver %s version (%d) / HW %s version (%d) [OK].",
				 pcRGXFW_PROCESSOR,
				 ui32FWCoreIDValue,
				 pcRGXFW_PROCESSOR,
				 psRGXFWInit->sRGXCompChecks.ui32FWProcessorVersion));
	}
#endif
	return PVRSRV_OK;
}

/*!
*******************************************************************************

 @Function	RGXDevInitCompatCheck_StoreBVNCInUMSharedMem

 @Description

Store BVNC of the core being handled in memory shared with UM for compatibility
check performed by the UM part of the driver.

 @Input psDevInfo - device info
 @Input psRGXFWInit - FW init data

 @Return   PVRSRV_ERROR - PVRSRV_OK on success or appropriate error code

******************************************************************************/
static PVRSRV_ERROR RGXDevInitCompatCheck_StoreBVNCInUMSharedMem(PVRSRV_RGXDEV_INFO *psDevInfo, 
																RGXFWIF_INIT *psRGXFWInit)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	IMG_UINT32 *pui32InfoPage = PVRSRVGetPVRSRVData()->pui32InfoPage;
	PVR_ASSERT(pui32InfoPage);

#if !defined(NO_HARDWARE)
	PVR_UNREFERENCED_PARAMETER(psDevInfo);
	PVR_ASSERT(psRGXFWInit);

	pui32InfoPage[CORE_ID_BRANCH] = RGX_BVNC_PACKED_EXTR_B(psRGXFWInit->sRGXCompChecks.sFWBVNC);
	eError = OSStringToUINT32(RGX_BVNC_PACKED_EXTR_V(psRGXFWInit->sRGXCompChecks.sFWBVNC), 10, &pui32InfoPage[CORE_ID_VERSION]);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"%s: Failed to obtain core version (%u)",
				__FUNCTION__, eError));
	}
	pui32InfoPage[CORE_ID_NUMBER_OF_SCALABLE_UNITS] = RGX_BVNC_PACKED_EXTR_N(psRGXFWInit->sRGXCompChecks.sFWBVNC);
	pui32InfoPage[CORE_ID_CONFIG] = RGX_BVNC_PACKED_EXTR_C(psRGXFWInit->sRGXCompChecks.sFWBVNC);
#else
	PVR_UNREFERENCED_PARAMETER(psRGXFWInit);
	PVR_ASSERT(psDevInfo);

	pui32InfoPage[CORE_ID_BRANCH] = psDevInfo->sDevFeatureCfg.ui32B;
	pui32InfoPage[CORE_ID_VERSION] = psDevInfo->sDevFeatureCfg.ui32V;
	pui32InfoPage[CORE_ID_NUMBER_OF_SCALABLE_UNITS] = psDevInfo->sDevFeatureCfg.ui32N;
	pui32InfoPage[CORE_ID_CONFIG] = psDevInfo->sDevFeatureCfg.ui32C;
#endif /* !defined(NO_HARDWARE) */

	return eError;
}

/*!
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
	PVRSRV_RGXDEV_INFO 	*psDevInfo = psDeviceNode->pvDevice;
	RGXFWIF_INIT		*psRGXFWInit = NULL;
#if !defined(NO_HARDWARE)
	IMG_UINT32			ui32RegValue;

	/* Retrieve the FW information */
	eError = DevmemAcquireCpuVirtAddr(psDevInfo->psRGXFWIfInitMemDesc,
												(void **)&psRGXFWInit);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"%s: Failed to acquire kernel fw compatibility check info (%u)",
				__FUNCTION__, eError));
		return eError;
	}

	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{
		if (*((volatile IMG_BOOL *)&psRGXFWInit->sRGXCompChecks.bUpdated))
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
					__FUNCTION__, eError));
			goto chk_exit;
		}

		if (!(ui32RegValue & META_CR_TXENABLE_ENABLE_BIT))
		{
			eError = PVRSRV_ERROR_META_THREAD0_NOT_ENABLED;
			PVR_DPF((PVR_DBG_ERROR,"%s: RGX META is not running. Is the GPU correctly powered up? %d (%u)",
					__FUNCTION__, psRGXFWInit->sRGXCompChecks.bUpdated, eError));
			goto chk_exit;
		}
	}

	if (!*((volatile IMG_BOOL *)&psRGXFWInit->sRGXCompChecks.bUpdated))
	{
		eError = PVRSRV_ERROR_TIMEOUT;
		PVR_DPF((PVR_DBG_ERROR,"%s: Missing compatibility info from FW (%u)",
				__FUNCTION__, eError));
		goto chk_exit;
	}
#endif /* defined(NO_HARDWARE) */

	eError = RGXDevInitCompatCheck_KMBuildOptions_FWAgainstDriver(psRGXFWInit);
	if (eError != PVRSRV_OK)
	{
		goto chk_exit;
	}

	eError = RGXDevInitCompatCheck_DDKVersion_FWAgainstDriver(psDevInfo, psRGXFWInit);
	if (eError != PVRSRV_OK)
	{
		goto chk_exit;
	}

	eError = RGXDevInitCompatCheck_DDKBuild_FWAgainstDriver(psDevInfo, psRGXFWInit);
	if (eError != PVRSRV_OK)
	{
		goto chk_exit;
	}

	if (!PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		eError = RGXDevInitCompatCheck_BVNC_FWAgainstDriver(psDevInfo, psRGXFWInit);
		if (eError != PVRSRV_OK)
		{
			goto chk_exit;
		}

		eError = RGXDevInitCompatCheck_BVNC_HWAgainstDriver(psDevInfo, psRGXFWInit);
		if (eError != PVRSRV_OK)
		{
			goto chk_exit;
		}
	}
	eError = RGXDevInitCompatCheck_FWProcessorVersion_AgainstDriver(psDevInfo, psRGXFWInit);
	if (eError != PVRSRV_OK)
	{
		goto chk_exit;
	}

	eError = RGXDevInitCompatCheck_StoreBVNCInUMSharedMem(psDevInfo, psRGXFWInit);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"%s: Failed to store compatibility info for UM consumption (%u)",
				__FUNCTION__, eError));
		goto chk_exit;
	}

	eError = PVRSRV_OK;
chk_exit:
#if !defined(NO_HARDWARE)
	DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfInitMemDesc);
#endif
	return eError;
}

/**************************************************************************/ /*!
@Function       RGXSoftReset
@Description    Resets some modules of the RGX device
@Input          psDeviceNode		Device node
@Input          ui64ResetValue1 A mask for which each bit set corresponds
                                to a module to reset (via the SOFT_RESET
                                register).
@Input          ui64ResetValue2 A mask for which each bit set corresponds
                                to a module to reset (via the SOFT_RESET2
                                register).
@Return         PVRSRV_ERROR
*/ /***************************************************************************/
static PVRSRV_ERROR RGXSoftReset(PVRSRV_DEVICE_NODE *psDeviceNode,
                                 IMG_UINT64  ui64ResetValue1,
                                 IMG_UINT64  ui64ResetValue2)
{
	PVRSRV_RGXDEV_INFO        *psDevInfo;
	IMG_BOOL	bSoftReset = IMG_FALSE;
	IMG_UINT64	ui64SoftResetMask = 0;

	PVR_ASSERT(psDeviceNode != NULL);
	PVR_ASSERT(psDeviceNode->pvDevice != NULL);
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_GUEST, PVRSRV_OK);

	/* the device info */
	psDevInfo = psDeviceNode->pvDevice;
	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, PBE2_IN_XE))
	{
		ui64SoftResetMask = RGX_CR_SOFT_RESET__PBE2_XE__MASKFULL;
	}else
	{
		ui64SoftResetMask = RGX_CR_SOFT_RESET_MASKFULL;
	}

	if ((RGX_IS_FEATURE_SUPPORTED(psDevInfo, S7_TOP_INFRASTRUCTURE)) && \
		((ui64ResetValue2 & RGX_CR_SOFT_RESET2_MASKFULL) != ui64ResetValue2))
	{
		bSoftReset = IMG_TRUE;
	}

	if (((ui64ResetValue1 & ui64SoftResetMask) != ui64ResetValue1) || bSoftReset)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/* Set in soft-reset */
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, ui64ResetValue1);

	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, S7_TOP_INFRASTRUCTURE))
	{
		OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET2, ui64ResetValue2);
	}


	/* Read soft-reset to fence previous write in order to clear the SOCIF pipeline */
	(void) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET);
	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, S7_TOP_INFRASTRUCTURE))
	{
		(void) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET2);
	}

	/* Take the modules out of reset... */
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, 0);
	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, S7_TOP_INFRASTRUCTURE))
	{
		OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET2, 0);
	}

	/* ...and fence again */
	(void) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET);
	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, S7_TOP_INFRASTRUCTURE))
	{
		(void) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET2);
	}

	return PVRSRV_OK;
}

/*!
******************************************************************************

 @Function	RGXDebugRequestNotify

 @Description Dump the debug data for RGX

******************************************************************************/
static void RGXDebugRequestNotify(PVRSRV_DBGREQ_HANDLE hDbgReqestHandle,
					IMG_UINT32 ui32VerbLevel,
					DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
					void *pvDumpDebugFile)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = hDbgReqestHandle;

	/* Only action the request if we've fully init'ed */
	if (psDevInfo->bDevInit2Done)
	{
		RGXDebugRequestProcess(pfnDumpDebugPrintf, pvDumpDebugFile, psDevInfo, ui32VerbLevel);
	}
}

static const RGX_MIPS_ADDRESS_TRAMPOLINE sNullTrampoline;

static void RGXFreeTrampoline(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

	DevPhysMemFree(psDeviceNode,
#if defined(PDUMP)
	               psDevInfo->sTrampoline.hPdumpPages,
#endif
	               &psDevInfo->sTrampoline.sPages);
	psDevInfo->sTrampoline = sNullTrampoline;
}

static PVRSRV_ERROR RGXAllocTrampoline(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_ERROR eError;
	IMG_INT32 i, j;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	RGX_MIPS_ADDRESS_TRAMPOLINE asTrampoline[RGXMIPSFW_TRAMPOLINE_NUMPAGES];

	PDUMPCOMMENT("Allocate pages for trampoline");

	/* Retry the allocation of the trampoline, retaining any allocations
	 * overlapping with the target range until we get an allocation that
	 * doesn't overlap with the target range. Any allocation like this
	 * will require a maximum of 3 tries.
	 * Free the unused allocations only after the desired range is obtained
	 * to prevent the alloc function from returning the same bad range
	 * repeatedly.
	 */
	#define RANGES_OVERLAP(x,y,size) (x < (y+size) && y < (x+size))
	for (i = 0; i < 3; i++)
	{
		eError = DevPhysMemAlloc(psDeviceNode,
					 RGXMIPSFW_TRAMPOLINE_SIZE,
					 RGXMIPSFW_TRAMPOLINE_LOG2_SEGMENT_SIZE,
					 0,         // (init) u8Value
					 IMG_FALSE, // bInitPage,
#if defined(PDUMP)
					 psDeviceNode->psFirmwareMMUDevAttrs->pszMMUPxPDumpMemSpaceName,
					 "TrampolineRegion",
					 &asTrampoline[i].hPdumpPages,
#endif
					 &asTrampoline[i].sPages,
					 &asTrampoline[i].sPhysAddr);
		if (PVRSRV_OK != eError)
		{
			PVR_DPF((PVR_DBG_ERROR,"%s failed (%u)",
				 __func__, eError));
			goto fail;
		}

		if (!RANGES_OVERLAP(asTrampoline[i].sPhysAddr.uiAddr,
		                    RGXMIPSFW_TRAMPOLINE_TARGET_PHYS_ADDR,
		                    RGXMIPSFW_TRAMPOLINE_SIZE))
		{
			break;
		}
	}
	if (RGXMIPSFW_TRAMPOLINE_NUMPAGES == i)
	{
		eError = PVRSRV_ERROR_FAILED_TO_ALLOC_PAGES;
		PVR_DPF((PVR_DBG_ERROR,
		         "%s failed to allocate non-overlapping pages (%u)",
			 __func__, eError));
		goto fail;
	}
	#undef RANGES_OVERLAP

	psDevInfo->sTrampoline = asTrampoline[i];

fail:
	/* free all unused allocations */
	for (j = 0; j < i; j++)
	{
		DevPhysMemFree(psDeviceNode,
#if defined(PDUMP)
		               asTrampoline[j].hPdumpPages,
#endif
		               &asTrampoline[j].sPages);
	}

	return eError;
}

PVRSRV_ERROR PVRSRVRGXInitAllocFWImgMemKM(PVRSRV_DEVICE_NODE   *psDeviceNode,
                                          IMG_DEVMEM_SIZE_T    uiFWCodeLen,
                                          IMG_DEVMEM_SIZE_T    uiFWDataLen,
                                          IMG_DEVMEM_SIZE_T    uiFWCorememLen,
                                          PMR                  **ppsFWCodePMR,
                                          IMG_DEV_VIRTADDR     *psFWCodeDevVAddrBase,
                                          PMR                  **ppsFWDataPMR,
                                          IMG_DEV_VIRTADDR     *psFWDataDevVAddrBase,
                                          PMR                  **ppsFWCorememPMR,
                                          IMG_DEV_VIRTADDR     *psFWCorememDevVAddrBase,
                                          RGXFWIF_DEV_VIRTADDR *psFWCorememMetaVAddrBase)
{
	DEVMEM_FLAGS_T		uiMemAllocFlags;
	PVRSRV_RGXDEV_INFO 	*psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_ERROR        eError;

	eError = RGXInitCreateFWKernelMemoryContext(psDeviceNode);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXInitAllocFWImgMemKM: Failed RGXInitCreateFWKernelMemoryContext (%u)", eError));
		goto failFWMemoryContextAlloc;
	}

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


	eError = RGXAllocateFWCodeRegion(psDeviceNode,
	                                 uiFWCodeLen,
	                                 uiMemAllocFlags,
	                                 IMG_FALSE,
	                                 "FwExCodeRegion",
	                                 &psDevInfo->psRGXFWCodeMemDesc);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"Failed to allocate fw code mem (%u)",
				eError));
		goto failFWCodeMemDescAlloc;
	}

	eError = DevmemLocalGetImportHandle(psDevInfo->psRGXFWCodeMemDesc, (void**) ppsFWCodePMR);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"DevmemLocalGetImportHandle failed (%u)", eError));
		goto failFWCodeMemDescAqDevVirt;
	}

	eError = DevmemAcquireDevVirtAddr(psDevInfo->psRGXFWCodeMemDesc,
	                                  &psDevInfo->sFWCodeDevVAddrBase);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"Failed to acquire devVAddr for fw code mem (%u)",
				eError));
		goto failFWCodeMemDescAqDevVirt;
	}
	*psFWCodeDevVAddrBase = psDevInfo->sFWCodeDevVAddrBase;

	if (!(RGX_IS_FEATURE_SUPPORTED(psDevInfo, MIPS) || (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))))
	{
		/*
		* The FW code must be the first allocation in the firmware heap, otherwise
		* the bootloader will not work (META will not be able to find the bootloader).
		*/
		PVR_ASSERT(psFWCodeDevVAddrBase->uiAddr == RGX_FIRMWARE_RAW_HEAP_BASE);
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

	PDUMPCOMMENT("Allocate and export data memory for fw");

	eError = DevmemFwAllocateExportable(psDeviceNode,
										uiFWDataLen,
										OSGetPageSize(),
										uiMemAllocFlags,
										"FwExDataRegion",
	                                    &psDevInfo->psRGXFWDataMemDesc);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"Failed to allocate fw data mem (%u)",
				eError));
		goto failFWDataMemDescAlloc;
	}

	eError = DevmemLocalGetImportHandle(psDevInfo->psRGXFWDataMemDesc, (void **) ppsFWDataPMR);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"DevmemLocalGetImportHandle failed (%u)", eError));
		goto failFWDataMemDescAqDevVirt;
	}

	eError = DevmemAcquireDevVirtAddr(psDevInfo->psRGXFWDataMemDesc,
	                                  &psDevInfo->sFWDataDevVAddrBase);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"Failed to acquire devVAddr for fw data mem (%u)",
				eError));
		goto failFWDataMemDescAqDevVirt;
	}
	*psFWDataDevVAddrBase = psDevInfo->sFWDataDevVAddrBase;

	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, MIPS))
	{
		eError = RGXAllocTrampoline(psDeviceNode);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "Failed to allocate trampoline region (%u)",
					 eError));
			goto failTrampolineMemDescAlloc;
		}
	}

	if (uiFWCorememLen != 0)
	{
		/*
		 * Set up Allocation for FW coremem section
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

		eError = RGXAllocateFWCodeRegion(psDeviceNode,
		                                 uiFWCorememLen,
		                                 uiMemAllocFlags,
		                                 IMG_TRUE,
		                                 "FwExCorememRegion",
		                                 &psDevInfo->psRGXFWCorememMemDesc);

		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"Failed to allocate fw coremem mem, size: %"  IMG_INT64_FMTSPECd ", flags: %x (%u)",
						uiFWCorememLen, uiMemAllocFlags, eError));
			goto failFWCorememMemDescAlloc;
		}

		eError = DevmemLocalGetImportHandle(psDevInfo->psRGXFWCorememMemDesc, (void**) ppsFWCorememPMR);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"DevmemLocalGetImportHandle failed (%u)", eError));
			goto failFWCorememMemDescAqDevVirt;
		}

		eError = DevmemAcquireDevVirtAddr(psDevInfo->psRGXFWCorememMemDesc,
		                                  &psDevInfo->sFWCorememCodeDevVAddrBase);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"Failed to acquire devVAddr for fw coremem mem (%u)",
						eError));
			goto failFWCorememMemDescAqDevVirt;
		}

		RGXSetFirmwareAddress(&psDevInfo->sFWCorememCodeFWAddr,
		                      psDevInfo->psRGXFWCorememMemDesc,
		                      0, RFW_FWADDR_NOREF_FLAG);
	}
	else
	{
		*ppsFWCorememPMR = NULL;
		psDevInfo->sFWCorememCodeDevVAddrBase.uiAddr = 0;
		psDevInfo->sFWCorememCodeFWAddr.ui32Addr = 0;
	}

	*psFWCorememDevVAddrBase = psDevInfo->sFWCorememCodeDevVAddrBase;
	*psFWCorememMetaVAddrBase = psDevInfo->sFWCorememCodeFWAddr;

	return PVRSRV_OK;

failFWCorememMemDescAqDevVirt:
	if (uiFWCorememLen != 0)
	{
		DevmemFwFree(psDevInfo, psDevInfo->psRGXFWCorememMemDesc);
		psDevInfo->psRGXFWCorememMemDesc = NULL;
	}
failFWCorememMemDescAlloc:
	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, MIPS))
	{
		RGXFreeTrampoline(psDeviceNode);
	}
failTrampolineMemDescAlloc:
	DevmemReleaseDevVirtAddr(psDevInfo->psRGXFWDataMemDesc);
failFWDataMemDescAqDevVirt:
	DevmemFwFree(psDevInfo, psDevInfo->psRGXFWDataMemDesc);
	psDevInfo->psRGXFWDataMemDesc = NULL;
failFWDataMemDescAlloc:
	DevmemReleaseDevVirtAddr(psDevInfo->psRGXFWCodeMemDesc);
failFWCodeMemDescAqDevVirt:
	DevmemFwFree(psDevInfo, psDevInfo->psRGXFWCodeMemDesc);
	psDevInfo->psRGXFWCodeMemDesc = NULL;
failFWCodeMemDescAlloc:
failFWMemoryContextAlloc:
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

	eResult = PVRSRVRGXDebugMiscQueryFWLogKM(NULL, psDeviceNode, pui32Value);
	*pui32Value &= RGXFWIF_LOG_TYPE_GROUP_MASK;
	return eResult;
}

static
PVRSRV_ERROR RGXFWTraceQueryLogType(const PVRSRV_DEVICE_NODE *psDeviceNode,
                                   const void *psPrivate,
                                   IMG_UINT32 *pui32Value)
{
	PVRSRV_ERROR eResult;

	eResult = PVRSRVRGXDebugMiscQueryFWLogKM(NULL, psDeviceNode, pui32Value);
	if (PVRSRV_OK == eResult)
	{
		if (*pui32Value & RGXFWIF_LOG_TYPE_TRACE)
		{
			*pui32Value = 2; /* Trace */
		}
		else if (*pui32Value & RGXFWIF_LOG_TYPE_GROUP_MASK)
		{
			*pui32Value = 1; /* TBI */
		}
		else
		{
			*pui32Value = 0; /* None */
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
		if (ui32Value && 1 != ui32RGXFWLogType)
		{
			ui32Value |= RGXFWIF_LOG_TYPE_TRACE;
		}
		eResult = PVRSRVRGXDebugMiscSetFWLogKM(NULL, psDeviceNode, ui32Value);
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

	/* 0 - none, 1 - tbi, 2 - trace */
	if (ui32Value)
	{
		eResult = RGXFWTraceQueryFilter(psDeviceNode, NULL, &ui32RGXFWLogType);
		if (PVRSRV_OK != eResult)
		{
			return eResult;
		}
		if (!ui32RGXFWLogType)
		{
			ui32RGXFWLogType = RGXFWIF_LOG_TYPE_GROUP_MAIN;
		}
		if (2 == ui32Value)
		{
			ui32RGXFWLogType |= RGXFWIF_LOG_TYPE_TRACE;
		}
	}

	eResult = PVRSRVRGXDebugMiscSetFWLogKM(NULL, psDeviceNode, ui32RGXFWLogType);
	return eResult;
}

static
PVRSRV_ERROR RGXQueryFWPoisonOnFree(const PVRSRV_DEVICE_NODE *psDeviceNode,
									const void *psPrivate,
									IMG_BOOL *pbValue)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = (PVRSRV_RGXDEV_INFO *) psDeviceNode->pvDevice;

	*pbValue = psDevInfo->bEnableFWPoisonOnFree;
	return PVRSRV_OK;
}

static
PVRSRV_ERROR RGXSetFWPoisonOnFree(const PVRSRV_DEVICE_NODE *psDeviceNode,
									const void *psPrivate,
									IMG_BOOL bValue)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = (PVRSRV_RGXDEV_INFO *) psDeviceNode->pvDevice;

	psDevInfo->bEnableFWPoisonOnFree = bValue;
	return PVRSRV_OK;
}

static
PVRSRV_ERROR RGXQueryFWPoisonOnFreeValue(const PVRSRV_DEVICE_NODE *psDeviceNode,
									const void *psPrivate,
									IMG_UINT32 *pui32Value)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = (PVRSRV_RGXDEV_INFO *) psDeviceNode->pvDevice;
	*pui32Value = psDevInfo->ubFWPoisonOnFreeValue;
	return PVRSRV_OK;
}

static
PVRSRV_ERROR RGXSetFWPoisonOnFreeValue(const PVRSRV_DEVICE_NODE *psDeviceNode,
									const void *psPrivate,
									IMG_UINT32 ui32Value)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = (PVRSRV_RGXDEV_INFO *) psDeviceNode->pvDevice;
	psDevInfo->ubFWPoisonOnFreeValue = (IMG_BYTE) ui32Value;
	return PVRSRV_OK;
}

/*
 * PVRSRVRGXInitFirmwareKM
 */
PVRSRV_ERROR
PVRSRVRGXInitFirmwareKM(PVRSRV_DEVICE_NODE       *psDeviceNode,
                        RGXFWIF_DEV_VIRTADDR     *psRGXFwInit,
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
                        RGXFWIF_COMPCHECKS_BVNC  *psClientBVNC,
                        RGXFWIF_COMPCHECKS_BVNC  *psFirmwareBVNC,
                        IMG_UINT32               ui32HWPerfCountersDataSize,
                        PMR                      **ppsHWPerfPMR,
                        RGX_RD_POWER_ISLAND_CONF eRGXRDPowerIslandingConf,
                        FW_PERF_CONF             eFirmwarePerf,
                        IMG_UINT32               ui32ConfigFlagsExt)
{
	PVRSRV_ERROR eError;
	void *pvAppHintState = NULL;
	IMG_UINT32 ui32AppHintDefault;
	RGXFWIF_COMPCHECKS_BVNC_DECLARE_AND_INIT(sBVNC);
	IMG_BOOL bCompatibleAll=IMG_TRUE, bCompatibleVersion=IMG_TRUE, bCompatibleLenMax=IMG_TRUE, bCompatibleBNC=IMG_TRUE, bCompatibleV=IMG_TRUE;
	IMG_UINT32 ui32NumBIFTilingConfigs, *pui32BIFTilingXStrides, i, ui32B, ui32V, ui32N, ui32C;
	RGXFWIF_BIFTILINGMODE eBIFTilingMode;
	IMG_CHAR szV[8];
	PVRSRV_RGXDEV_INFO *psDevInfo = (PVRSRV_RGXDEV_INFO *)psDeviceNode->pvDevice;

	OSSNPrintf(szV, sizeof(szV),"%d",psDevInfo->sDevFeatureCfg.ui32V);
	rgx_bvnc_packed(&sBVNC.ui64BNC, sBVNC.aszV, sBVNC.ui32VLenMax, psDevInfo->sDevFeatureCfg.ui32B, szV, psDevInfo->sDevFeatureCfg.ui32N, psDevInfo->sDevFeatureCfg.ui32C);

	/* Check if BVNC numbers of firmware and driver are compatible */
	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		bCompatibleAll = IMG_TRUE;
	}
	else
	{
		RGX_BVNC_EQUAL(sBVNC, *psFirmwareBVNC, bCompatibleAll, bCompatibleVersion, bCompatibleLenMax, bCompatibleBNC, bCompatibleV);
	}

	if (!bCompatibleAll)
	{
		if (!bCompatibleVersion)
		{
			PVR_LOG(("(FAIL) %s: Incompatible compatibility struct version of driver (%d) and firmware (%d).",
			         __FUNCTION__,
			         sBVNC.ui32LayoutVersion,
			         psFirmwareBVNC->ui32LayoutVersion));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			PVR_DBG_BREAK;
			goto failed_to_pass_compatibility_check;
		}

		if (!bCompatibleLenMax)
		{
			PVR_LOG(("(FAIL) %s: Incompatible V maxlen of driver (%d) and firmware (%d).",
			         __FUNCTION__,
			         sBVNC.ui32VLenMax,
			         psFirmwareBVNC->ui32VLenMax));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			PVR_DBG_BREAK;
			goto failed_to_pass_compatibility_check;
		}

		if (!bCompatibleBNC)
		{
			PVR_LOG(("(FAIL) %s: Incompatible driver BNC (%d._.%d.%d) / firmware BNC (%d._.%d.%d).",
			         __FUNCTION__,
			         RGX_BVNC_PACKED_EXTR_B(sBVNC),
			         RGX_BVNC_PACKED_EXTR_N(sBVNC),
			         RGX_BVNC_PACKED_EXTR_C(sBVNC),
			         RGX_BVNC_PACKED_EXTR_B(*psFirmwareBVNC),
			         RGX_BVNC_PACKED_EXTR_N(*psFirmwareBVNC),
			         RGX_BVNC_PACKED_EXTR_C(*psFirmwareBVNC)));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			PVR_DBG_BREAK;
			goto failed_to_pass_compatibility_check;
		}

		if (!bCompatibleV)
		{
			PVR_LOG(("(FAIL) %s: Incompatible driver BVNC (%d.%s.%d.%d) / firmware BVNC (%d.%s.%d.%d).",
			         __FUNCTION__,
			         RGX_BVNC_PACKED_EXTR_B(sBVNC),
			         RGX_BVNC_PACKED_EXTR_V(sBVNC),
			         RGX_BVNC_PACKED_EXTR_N(sBVNC),
			         RGX_BVNC_PACKED_EXTR_C(sBVNC),
			         RGX_BVNC_PACKED_EXTR_B(*psFirmwareBVNC),
			         RGX_BVNC_PACKED_EXTR_V(*psFirmwareBVNC),
			         RGX_BVNC_PACKED_EXTR_N(*psFirmwareBVNC),
			         RGX_BVNC_PACKED_EXTR_C(*psFirmwareBVNC)));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			PVR_DBG_BREAK;
			goto failed_to_pass_compatibility_check;
		}
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE, "%s: COMPAT_TEST: driver BVNC (%d.%s.%d.%d) and firmware BVNC (%d.%s.%d.%d) match. [ OK ]",
		         __FUNCTION__,
		         RGX_BVNC_PACKED_EXTR_B(sBVNC),
		         RGX_BVNC_PACKED_EXTR_V(sBVNC),
		         RGX_BVNC_PACKED_EXTR_N(sBVNC),
		         RGX_BVNC_PACKED_EXTR_C(sBVNC),
		         RGX_BVNC_PACKED_EXTR_B(*psFirmwareBVNC),
		         RGX_BVNC_PACKED_EXTR_V(*psFirmwareBVNC),
		         RGX_BVNC_PACKED_EXTR_N(*psFirmwareBVNC),
		         RGX_BVNC_PACKED_EXTR_C(*psFirmwareBVNC)));
	}

	ui32B = psDevInfo->sDevFeatureCfg.ui32B;
	ui32V = psDevInfo->sDevFeatureCfg.ui32V;
	ui32N = psDevInfo->sDevFeatureCfg.ui32N;
	ui32C = psDevInfo->sDevFeatureCfg.ui32C;

	OSSNPrintf(szV, sizeof(szV),"%d",ui32V);

	/* Check if BVNC numbers of client and driver are compatible */
	rgx_bvnc_packed(&sBVNC.ui64BNC, sBVNC.aszV, sBVNC.ui32VLenMax,  ui32B, szV, ui32N, ui32C);

	RGX_BVNC_EQUAL(sBVNC, *psClientBVNC, bCompatibleAll, bCompatibleVersion, bCompatibleLenMax, bCompatibleBNC, bCompatibleV);

	if (!bCompatibleAll)
	{
		if (!bCompatibleVersion)
		{
			PVR_LOG(("(FAIL) %s: Incompatible compatibility struct version of driver (%d) and client (%d).",
					__FUNCTION__,
					sBVNC.ui32LayoutVersion,
					psClientBVNC->ui32LayoutVersion));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			PVR_DBG_BREAK;
			goto failed_to_pass_compatibility_check;
		}

		if (!bCompatibleLenMax)
		{
			PVR_LOG(("(FAIL) %s: Incompatible V maxlen of driver (%d) and client (%d).",
					__FUNCTION__,
					sBVNC.ui32VLenMax,
					psClientBVNC->ui32VLenMax));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			PVR_DBG_BREAK;
			goto failed_to_pass_compatibility_check;
		}

		if (!bCompatibleBNC)
		{
			PVR_LOG(("(FAIL) %s: Incompatible driver BNC (%d._.%d.%d) / client BNC (%d._.%d.%d).",
					__FUNCTION__,
					RGX_BVNC_PACKED_EXTR_B(sBVNC),
					RGX_BVNC_PACKED_EXTR_N(sBVNC),
					RGX_BVNC_PACKED_EXTR_C(sBVNC),
					RGX_BVNC_PACKED_EXTR_B(*psClientBVNC),
					RGX_BVNC_PACKED_EXTR_N(*psClientBVNC),
					RGX_BVNC_PACKED_EXTR_C(*psClientBVNC)));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			PVR_DBG_BREAK;
			goto failed_to_pass_compatibility_check;
		}

		if (!bCompatibleV)
		{
			PVR_LOG(("(FAIL) %s: Incompatible driver BVNC (%d.%s.%d.%d) / client BVNC (%d.%s.%d.%d).",
					__FUNCTION__,
					RGX_BVNC_PACKED_EXTR_B(sBVNC),
					RGX_BVNC_PACKED_EXTR_V(sBVNC),
					RGX_BVNC_PACKED_EXTR_N(sBVNC),
					RGX_BVNC_PACKED_EXTR_C(sBVNC),
					RGX_BVNC_PACKED_EXTR_B(*psClientBVNC),
					RGX_BVNC_PACKED_EXTR_V(*psClientBVNC),
					RGX_BVNC_PACKED_EXTR_N(*psClientBVNC),
					RGX_BVNC_PACKED_EXTR_C(*psClientBVNC)));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			PVR_DBG_BREAK;
			goto failed_to_pass_compatibility_check;
		}
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE, "%s: COMPAT_TEST: driver BVNC (%d.%s.%d.%d) and client BVNC (%d.%s.%d.%d) match. [ OK ]",
				__FUNCTION__,
				RGX_BVNC_PACKED_EXTR_B(sBVNC),
				RGX_BVNC_PACKED_EXTR_V(sBVNC),
				RGX_BVNC_PACKED_EXTR_N(sBVNC),
				RGX_BVNC_PACKED_EXTR_C(sBVNC),
				RGX_BVNC_PACKED_EXTR_B(*psClientBVNC),
				RGX_BVNC_PACKED_EXTR_V(*psClientBVNC),
				RGX_BVNC_PACKED_EXTR_N(*psClientBVNC),
				RGX_BVNC_PACKED_EXTR_C(*psClientBVNC)));
	}

	PVRSRVSystemBIFTilingGetConfig(psDeviceNode->psDevConfig,
	                               &eBIFTilingMode,
	                               &ui32NumBIFTilingConfigs);
	pui32BIFTilingXStrides = OSAllocMem(sizeof(IMG_UINT32) * ui32NumBIFTilingConfigs);
	if (pui32BIFTilingXStrides == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXInitFirmwareKM: OSAllocMem failed (%u)", eError));
		goto failed_BIF_tiling_alloc;
	}
	for(i = 0; i < ui32NumBIFTilingConfigs; i++)
	{
		eError = PVRSRVSystemBIFTilingHeapGetXStride(psDeviceNode->psDevConfig,
		                                             i+1,
		                                             &pui32BIFTilingXStrides[i]);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
			         "%s: Failed to get BIF tiling X stride for heap %u (%u)",
			          __func__, i + 1, eError));
			goto failed_BIF_heap_init;
		}
	}

	eError = RGXSetupFirmware(psDeviceNode,
	                          bEnableSignatureChecks,
	                          ui32SignatureChecksBufSize,
	                          ui32HWPerfFWBufSizeKB,
	                          ui64HWPerfFilter,
	                          ui32RGXFWAlignChecksArrLength,
	                          pui32RGXFWAlignChecks,
	                          ui32ConfigFlags,
	                          ui32LogType,
	                          eBIFTilingMode,
	                          ui32NumBIFTilingConfigs,
	                          pui32BIFTilingXStrides,
	                          ui32FilterFlags,
	                          ui32JonesDisableMask,
	                          ui32HWRDebugDumpLimit,
	                          ui32HWPerfCountersDataSize,
	                          ppsHWPerfPMR,
	                          psRGXFwInit,
	                          eRGXRDPowerIslandingConf,
	                          eFirmwarePerf,
	                          ui32ConfigFlagsExt);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXInitFirmwareKM: RGXSetupFirmware failed (%u)", eError));
		goto failed_init_firmware;
	}

	OSFreeMem(pui32BIFTilingXStrides);

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

	/* FW Poison values are not passed through from the init code
	 * so grab them here */
	OSCreateKMAppHintState(&pvAppHintState);

	ui32AppHintDefault = PVRSRV_APPHINT_ENABLEFWPOISONONFREE;
	OSGetKMAppHintBOOL(pvAppHintState,
	                   EnableFWPoisonOnFree,
	                   &ui32AppHintDefault,
	                   &psDevInfo->bEnableFWPoisonOnFree);

	ui32AppHintDefault = PVRSRV_APPHINT_FWPOISONONFREEVALUE;
	OSGetKMAppHintUINT32(pvAppHintState,
	                     FWPoisonOnFreeValue,
	                     &ui32AppHintDefault,
	                     (IMG_UINT32*)&psDevInfo->ubFWPoisonOnFreeValue);

	OSFreeKMAppHintState(pvAppHintState);

	PVRSRVAppHintRegisterHandlersBOOL(APPHINT_ID_EnableFWPoisonOnFree,
	                                  RGXQueryFWPoisonOnFree,
	                                  RGXSetFWPoisonOnFree,
	                                  psDeviceNode,
	                                  NULL);

	PVRSRVAppHintRegisterHandlersUINT32(APPHINT_ID_FWPoisonOnFreeValue,
	                                    RGXQueryFWPoisonOnFreeValue,
	                                    RGXSetFWPoisonOnFreeValue,
	                                    psDeviceNode,
	                                    NULL);

	return PVRSRV_OK;

failed_init_firmware:
failed_BIF_heap_init:
	OSFreeMem(pui32BIFTilingXStrides);
failed_BIF_tiling_alloc:
failed_to_pass_compatibility_check:
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

	psDevInfo = psDeviceNode->pvDevice;

	/* Size and align are 'expanded' because we request an Exportalign allocation */
	DevmemExportalignAdjustSizeAndAlign(DevmemGetHeapLog2PageSize(psDevInfo->psFirmwareMainHeap),
										&uiUFOBlockSize,
										&ui32UFOBlockAlign);

	eError = DevmemFwAllocateExportable(psDeviceNode,
										uiUFOBlockSize,
										ui32UFOBlockAlign,
										PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(PMMETA_PROTECT) |
										PVRSRV_MEMALLOCFLAG_KERNEL_CPU_MAPPABLE |
										PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC |
										PVRSRV_MEMALLOCFLAG_CACHE_COHERENT |
										PVRSRV_MEMALLOCFLAG_GPU_READABLE |
										PVRSRV_MEMALLOCFLAG_GPU_WRITEABLE |
										PVRSRV_MEMALLOCFLAG_CPU_READABLE |
										PVRSRV_MEMALLOCFLAG_CPU_WRITEABLE,
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
	/*
		If the system has snooping of the device cache then the UFO block
		might be in the cache so we need to flush it out before freeing
		the memory

		When the device is being shutdown/destroyed we don't care anymore.
		Several necessary data structures to issue a flush were destroyed
		already.
	*/
	if (PVRSRVSystemSnoopingOfDeviceCache(psDeviceNode->psDevConfig) &&
		psDeviceNode->eDevState != PVRSRV_DEVICE_STATE_DEINIT)
	{
		RGXFWIF_KCCB_CMD sFlushInvalCmd;
		PVRSRV_ERROR eError;

		/* Schedule the SLC flush command ... */
#if defined(PDUMP)
		PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "Submit SLC flush and invalidate");
#endif
		sFlushInvalCmd.eCmdType = RGXFWIF_KCCB_CMD_SLCFLUSHINVAL;
		sFlushInvalCmd.uCmdData.sSLCFlushInvalData.bInval = IMG_TRUE;
		sFlushInvalCmd.uCmdData.sSLCFlushInvalData.bDMContext = IMG_FALSE;
		sFlushInvalCmd.uCmdData.sSLCFlushInvalData.eDM = 0;
		sFlushInvalCmd.uCmdData.sSLCFlushInvalData.psContext.ui32Addr = 0;

		eError = RGXSendCommandWithPowLock(psDeviceNode->pvDevice,
											RGXFWIF_DM_GP,
											&sFlushInvalCmd,
											sizeof(sFlushInvalCmd),
											PDUMP_FLAGS_CONTINUOUS);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"RGXFreeUFOBlock: Failed to schedule SLC flush command with error (%u)", eError));
		}
		else
		{
			/* Wait for the SLC flush to complete */
			eError = RGXWaitForFWOp(psDeviceNode->pvDevice, RGXFWIF_DM_GP, psDeviceNode->psSyncPrim, PDUMP_FLAGS_CONTINUOUS);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"RGXFreeUFOBlock: SLC flush and invalidate aborted with error (%u)", eError));
			}
		}
	}

	RGXUnsetFirmwareAddress(psMemDesc);
	DevmemFwFree(psDeviceNode->pvDevice, psMemDesc);
}

/*
	DevDeInitRGX
*/
PVRSRV_ERROR DevDeInitRGX (PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_RGXDEV_INFO			*psDevInfo = (PVRSRV_RGXDEV_INFO*)psDeviceNode->pvDevice;
	PVRSRV_ERROR				eError;
	DEVICE_MEMORY_INFO		    *psDevMemoryInfo;
	IMG_UINT32		ui32Temp=0;

	if (!psDevInfo)
	{
		/* Can happen if DevInitRGX failed */
		PVR_DPF((PVR_DBG_ERROR,"DevDeInitRGX: Null DevInfo"));
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
			PVR_DPF((PVR_DBG_ERROR,"%s: Dummy page reference counter is non zero (%u)",
					__func__,
					ui32Temp));
			PVR_ASSERT(0);
		}
	}
#endif
#if defined(PDUMP)
		if (NULL != psDeviceNode->sDummyPage.hPdumpDummyPg)
		{
			PDUMPCOMMENT("Error dummy page handle is still active");
		}
#endif

#if defined(SUPPORT_PDVFS) && !defined(RGXFW_META_SUPPORT_2ND_THREAD)
	if (psDeviceNode->psDevConfig->sDVFS.sPDVFSData.hReactiveTimer)
	{
		OSDisableTimer(psDeviceNode->psDevConfig->sDVFS.sPDVFSData.hReactiveTimer);
		OSRemoveTimer(psDeviceNode->psDevConfig->sDVFS.sPDVFSData.hReactiveTimer);
	}
#endif

	/*The lock type need to be dispatch type here because it can be acquired from MISR (Z-buffer) path */
	OSLockDestroy(psDeviceNode->sDummyPage.psDummyPgLock);

#if defined(SUPPORT_POWER_SAMPLING_VIA_DEBUGFS)
	OSLockDestroy(psDevInfo->hCounterDumpingLock);
#endif

	/* Unregister debug request notifiers first as they could depend on anything. */
	if (psDevInfo->hDbgReqNotify)
	{
		PVRSRVUnregisterDbgRequestNotify(psDevInfo->hDbgReqNotify);
	}

	/* Cancel notifications to this device */
	PVRSRVUnregisterCmdCompleteNotify(psDeviceNode->hCmdCompNotify);
	psDeviceNode->hCmdCompNotify = NULL;

	/*
	 *  De-initialise in reverse order, so stage 2 init is undone first.
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
#endif /* !NO_HARDWARE */

		/* Remove the device from the power manager */
		eError = PVRSRVRemovePowerDevice(psDeviceNode);
		if (eError != PVRSRV_OK)
		{
			return eError;
		}

#ifndef ENABLE_COMMON_DVFS
		OSLockDestroy(psDevInfo->hGPUUtilLock);
#endif

		/* Free DVFS Table */
		if (psDevInfo->psGpuDVFSTable != NULL)
		{
			OSFreeMem(psDevInfo->psGpuDVFSTable);
			psDevInfo->psGpuDVFSTable = NULL;
		}

#if defined(SUPPORT_RAY_TRACING)
		OSLockDestroy(psDevInfo->hLockRPMContext);
		OSLockDestroy(psDevInfo->hLockRPMFreeList);
#endif

		/* De-init Freelists/ZBuffers... */
		OSLockDestroy(psDevInfo->hLockFreeList);
		OSLockDestroy(psDevInfo->hLockZSBuffer);

		/* Unregister MMU related stuff */
		eError = RGXMMUInit_Unregister(psDeviceNode);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"DevDeInitRGX: Failed RGXMMUInit_Unregister (0x%x)", eError));
			return eError;
		}


		if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, MIPS))
		{
			/* Unregister MMU related stuff */
			eError = RGXMipsMMUInit_Unregister(psDeviceNode);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"DevDeInitRGX: Failed RGXMipsMMUInit_Unregister (0x%x)", eError));
				return eError;
			}
		}
	}

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

#if 0 /* not required at this time */
	if (psDevInfo->hTimer)
	{
		eError = OSRemoveTimer(psDevInfo->hTimer);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"DevDeInitRGX: Failed to remove timer"));
			return 	eError;
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
		DevmemFwFree(psDevInfo, psDevInfo->psRGXFWCodeMemDesc);
		psDevInfo->psRGXFWCodeMemDesc = NULL;
	}
	else
	{
		PVR_DPF((PVR_DBG_WARNING,"No firmware code memory to free"));
	}

	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, MIPS))
	{
		if (psDevInfo->sTrampoline.sPages.u.pvHandle)
		{
			/* Free trampoline region */
			PDUMPCOMMENT("Freeing trampoline memory");
			RGXFreeTrampoline(psDeviceNode);
		}
	}

	if (psDevInfo->psRGXFWDataMemDesc)
	{
		/* Free fw data */
		PDUMPCOMMENT("Freeing FW data memory");
		DevmemReleaseDevVirtAddr(psDevInfo->psRGXFWDataMemDesc);
		DevmemFwFree(psDevInfo, psDevInfo->psRGXFWDataMemDesc);
		psDevInfo->psRGXFWDataMemDesc = NULL;
	}
	else
	{
		PVR_DPF((PVR_DBG_WARNING,"No firmware data memory to free"));
	}

	if (psDevInfo->psRGXFWCorememMemDesc)
	{
		/* Free fw data */
		PDUMPCOMMENT("Freeing FW coremem memory");
		DevmemReleaseDevVirtAddr(psDevInfo->psRGXFWCorememMemDesc);
		DevmemFwFree(psDevInfo, psDevInfo->psRGXFWCorememMemDesc);
		psDevInfo->psRGXFWCorememMemDesc = NULL;
	}

	/*
	   Free the firmware allocations.
	 */
	RGXFreeFirmware(psDevInfo);
	RGXDeInitDestroyFWKernelMemoryContext(psDeviceNode);

	/* De-initialise non-device specific (TL) users of RGX device memory */
	RGXHWPerfHostDeInit(psDevInfo);
	eError = HTBDeInit();
	PVR_LOG_IF_ERROR(eError, "HTBDeInit");

	/* destroy the context list locks */
	OSWRLockDestroy(psDevInfo->hRenderCtxListLock);
	OSWRLockDestroy(psDevInfo->hComputeCtxListLock);
	OSWRLockDestroy(psDevInfo->hTransferCtxListLock);
	OSWRLockDestroy(psDevInfo->hTDMCtxListLock);
	OSWRLockDestroy(psDevInfo->hRaytraceCtxListLock);
	OSWRLockDestroy(psDevInfo->hKickSyncCtxListLock);
	OSWRLockDestroy(psDevInfo->hMemoryCtxListLock);
	OSLockDestroy(psDevInfo->hLockKCCBDeferredCommandsList);
	OSWRLockDestroy(psDevInfo->hCommonCtxtListLock);


	if ((psDevInfo->hNMILock != NULL) && (RGX_IS_FEATURE_SUPPORTED(psDevInfo, MIPS)))
	{
		OSLockDestroy(psDevInfo->hNMILock);
	}

#if defined(SUPPORT_PAGE_FAULT_DEBUG)
	if (psDevInfo->hDebugFaultInfoLock != NULL)
	{
		OSLockDestroy(psDevInfo->hDebugFaultInfoLock);
	}
	if (psDevInfo->hMMUCtxUnregLock != NULL)
	{
		OSLockDestroy(psDevInfo->hMMUCtxUnregLock);
	}
#endif

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
	IMG_UINT32 log2_import_alignment,
	IMG_UINT32 tiling_mode)
{
	DEVMEM_HEAP_BLUEPRINT b = {
		.pszName = name,
		.sHeapBaseAddr.uiAddr = heap_base,
		.uiHeapLength = heap_length,
		.uiLog2DataPageSize = RGXHeapDerivePageSize(OSGetPageShift()),
		.uiLog2ImportAlignment = log2_import_alignment,
		.uiLog2TilingStrideFactor = (RGX_BIF_TILING_HEAP_LOG2_ALIGN_TO_STRIDE_BASE - tiling_mode)
	};
	void *pvAppHintState = NULL;
	IMG_UINT32 ui32AppHintDefault = PVRSRV_APPHINT_GENERAL_NON4K_HEAP_PAGE_SIZE;
	IMG_UINT32 ui32GeneralNon4KHeapPageSize;

	if (!OSStringCompare(name, RGX_GENERAL_NON4K_HEAP_IDENT))
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

				PVR_DPF((PVR_DBG_ERROR,"Invalid AppHint GeneralAltHeapPageSize [%d] value, using 16KB",
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
			0, 0); \
	psDeviceMemoryHeapCursor++; \
} while (0)

#define INIT_FW_HEAP(TYPE, MODE) \
do { \
	*psDeviceMemoryHeapCursor = _blueprint_init( \
			RGX_FIRMWARE_ ## TYPE ## _HEAP_IDENT, \
			RGX_FIRMWARE_ ## MODE ## _ ## TYPE ## _HEAP_BASE, \
			RGX_FIRMWARE_ ## TYPE ## _HEAP_SIZE, \
			0, 0); \
	psDeviceMemoryHeapCursor++; \
} while (0)

#define INIT_HEAP_NAME(STR, NAME) \
do { \
	*psDeviceMemoryHeapCursor = _blueprint_init( \
			STR, \
			RGX_ ## NAME ## _HEAP_BASE, \
			RGX_ ## NAME ## _HEAP_SIZE, \
			0, 0); \
	psDeviceMemoryHeapCursor++; \
} while (0)

#define INIT_TILING_HEAP(D, N, M)		\
do { \
	IMG_UINT32 xstride; \
	PVRSRVSystemBIFTilingHeapGetXStride((D)->psDeviceNode->psDevConfig, N, &xstride); \
	*psDeviceMemoryHeapCursor = _blueprint_init( \
			RGX_BIF_TILING_HEAP_ ## N ## _IDENT, \
			RGX_BIF_TILING_HEAP_ ## N ## _BASE, \
			RGX_BIF_TILING_HEAP_SIZE, \
			RGX_BIF_TILING_HEAP_ALIGN_LOG2_FROM_XSTRIDE(xstride), \
			(IMG_UINT32)M); \
	psDeviceMemoryHeapCursor++; \
} while (0)

static PVRSRV_ERROR RGXInitHeaps(PVRSRV_RGXDEV_INFO *psDevInfo,
								 DEVICE_MEMORY_INFO *psNewMemoryInfo,
								 IMG_UINT32 *pui32Log2DummyPgSize)
{
	DEVMEM_HEAP_BLUEPRINT *psDeviceMemoryHeapCursor;
	RGXFWIF_BIFTILINGMODE eBIFTilingMode;
	IMG_UINT32 uiNumHeaps;
	void *pvAppHintState = NULL;
	IMG_UINT32 ui32AppHintDefault = PVRSRV_APPHINT_GENERAL_NON4K_HEAP_PAGE_SIZE;
	IMG_UINT32 ui32GeneralNon4KHeapPageSize;

#if defined(SUPPORT_VALIDATION)
	IMG_UINT32 ui32BIFTilingMode, ui32AppHintDefaultTilingMode = RGXFWIF_BIFTILINGMODE_MAX;

	OSCreateKMAppHintState(&pvAppHintState);
	OSGetKMAppHintUINT32(pvAppHintState, BIFTilingMode,
			&ui32AppHintDefaultTilingMode, &ui32BIFTilingMode);
	OSFreeKMAppHintState(pvAppHintState);
	if (ui32BIFTilingMode == RGXFWIF_BIFTILINGMODE_256x16 || ui32BIFTilingMode == RGXFWIF_BIFTILINGMODE_512x8)
	{
		psDevInfo->psDeviceNode->psDevConfig->eBIFTilingMode = ui32BIFTilingMode;
	}
	else if (ui32BIFTilingMode != RGXFWIF_BIFTILINGMODE_MAX)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXInitHeaps: BIF Tiling mode apphint is invalid"));
	}
#endif

	/* FIXME - consider whether this ought not to be on the device node itself */
	psNewMemoryInfo->psDeviceMemoryHeap = OSAllocMem(sizeof(DEVMEM_HEAP_BLUEPRINT) * RGX_MAX_HEAP_ID);
	if (psNewMemoryInfo->psDeviceMemoryHeap == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXRegisterDevice : Failed to alloc memory for DEVMEM_HEAP_BLUEPRINT"));
		goto e0;
	}

	PVRSRVSystemBIFTilingGetConfig(psDevInfo->psDeviceNode->psDevConfig, &eBIFTilingMode, &uiNumHeaps);

	/* Get the page size for the dummy page from the NON4K heap apphint */
	OSCreateKMAppHintState(&pvAppHintState);
	OSGetKMAppHintUINT32(pvAppHintState, GeneralNon4KHeapPageSize,
			&ui32AppHintDefault, &ui32GeneralNon4KHeapPageSize);
	*pui32Log2DummyPgSize = ExactLog2(ui32GeneralNon4KHeapPageSize);
	OSFreeKMAppHintState(pvAppHintState);

	/* Initialise the heaps */
	psDeviceMemoryHeapCursor = psNewMemoryInfo->psDeviceMemoryHeap;

	INIT_HEAP(GENERAL_SVM);

	if (RGX_IS_BRN_SUPPORTED(psDevInfo, 65273))
	{
		INIT_HEAP_NAME(RGX_GENERAL_HEAP_IDENT, GENERAL_BRN_65273);
	}
	else
	{
		INIT_HEAP(GENERAL);
	}

	if (RGX_IS_BRN_SUPPORTED(psDevInfo, 63142))
	{
		/* BRN63142 heap must be at the top of an aligned 16GB range. */
		INIT_HEAP(RGNHDR_BRN_63142);
		PVR_ASSERT((RGX_RGNHDR_BRN_63142_HEAP_BASE & IMG_UINT64_C(0x3FFFFFFFF)) +
		           RGX_RGNHDR_BRN_63142_HEAP_SIZE == IMG_UINT64_C(0x400000000));
	}

	if (RGX_IS_BRN_SUPPORTED(psDevInfo, 65273))
	{
		INIT_HEAP_NAME(RGX_GENERAL_NON4K_HEAP_IDENT, GENERAL_NON4K_BRN_65273);
		INIT_HEAP_NAME(RGX_VISTEST_HEAP_IDENT, VISTEST_BRN_65273);

		/* HWBRN65273 workaround also requires two Region Header buffers 4GB apart. */
		INIT_HEAP(MMU_INIA_BRN_65273);
		INIT_HEAP(MMU_INIB_BRN_65273);
	}
	else
	{
		INIT_HEAP(GENERAL_NON4K);
		INIT_HEAP(VISTEST);
	}

	if (RGX_IS_BRN_SUPPORTED(psDevInfo, 65273))
	{
		INIT_HEAP_NAME(RGX_PDSCODEDATA_HEAP_IDENT, PDSCODEDATA_BRN_65273);
		INIT_HEAP_NAME(RGX_USCCODE_HEAP_IDENT, USCCODE_BRN_65273);
	}
	else
	{
		INIT_HEAP(PDSCODEDATA);
		INIT_HEAP(USCCODE);
	}

	if (RGX_IS_BRN_SUPPORTED(psDevInfo, 65273))
	{
		INIT_HEAP_NAME(RGX_TQ3DPARAMETERS_HEAP_IDENT, TQ3DPARAMETERS_BRN_65273);
	}
	else
	{
		INIT_HEAP(TQ3DPARAMETERS);
	}

	INIT_TILING_HEAP(psDevInfo, 1, eBIFTilingMode);
	INIT_TILING_HEAP(psDevInfo, 2, eBIFTilingMode);
	INIT_TILING_HEAP(psDevInfo, 3, eBIFTilingMode);
	INIT_TILING_HEAP(psDevInfo, 4, eBIFTilingMode);
	INIT_HEAP(DOPPLER);
	INIT_HEAP(DOPPLER_OVERFLOW);
	INIT_HEAP(TDM_TPU_YUV_COEFFS);
	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, SIGNAL_SNOOPING))
	{
		INIT_HEAP(SERVICES_SIGNALS);
		INIT_HEAP(SIGNALS);
	}

	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		INIT_FW_HEAP(CONFIG, GUEST);
		INIT_FW_HEAP(MAIN, GUEST);
	}
	else
	{
		INIT_FW_HEAP(MAIN, HYPERV);
		INIT_FW_HEAP(CONFIG, HYPERV);
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
		PVR_DPF((PVR_DBG_ERROR,"RGXRegisterDevice : Failed to alloc memory for DEVMEM_HEAP_CONFIG"));
		goto e1;
	}

	psNewMemoryInfo->psDeviceMemoryHeapConfigArray[0].pszName = "Default Heap Configuration";
	psNewMemoryInfo->psDeviceMemoryHeapConfigArray[0].uiNumHeaps = psNewMemoryInfo->ui32HeapCount-2;
	psNewMemoryInfo->psDeviceMemoryHeapConfigArray[0].psHeapBlueprintArray = psNewMemoryInfo->psDeviceMemoryHeap;

	psNewMemoryInfo->psDeviceMemoryHeapConfigArray[1].pszName = "Firmware Heap Configuration";
	psNewMemoryInfo->psDeviceMemoryHeapConfigArray[1].uiNumHeaps = 2;
	psNewMemoryInfo->psDeviceMemoryHeapConfigArray[1].psHeapBlueprintArray = psDeviceMemoryHeapCursor-2;

	/* Perform additional virtualization initialization */
	if (RGXVzInitHeaps(psNewMemoryInfo, psDeviceMemoryHeapCursor) != PVRSRV_OK)
	{
		goto e1;
	}

	return PVRSRV_OK;
e1:
	OSFreeMem(psNewMemoryInfo->psDeviceMemoryHeap);
e0:
	return PVRSRV_ERROR_OUT_OF_MEMORY;
}

#undef INIT_HEAP
#undef INIT_HEAP_NAME
#undef INIT_TILING_HEAP

static void RGXDeInitHeaps(DEVICE_MEMORY_INFO *psDevMemoryInfo)
{
	RGXVzDeInitHeaps(psDevMemoryInfo);
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

	#if defined(RGX_FEATURE_SYSTEM_CACHE)
	PDUMPCOMMENT("RGX System Level Cache is present");
	#endif /* RGX_FEATURE_SYSTEM_CACHE */

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
#endif /* PDUMP */

	OSAtomicWrite(&psDeviceNode->eHealthStatus, PVRSRV_DEVICE_HEALTH_STATUS_OK);
	OSAtomicWrite(&psDeviceNode->eHealthReason, PVRSRV_DEVICE_HEALTH_REASON_NONE);

	/* Configure MMU specific stuff */
	RGXMMUInit_Register(psDeviceNode);

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

#if defined(RGXFW_ALIGNCHECKS)
	/* Register callback for checking alignment of UM structures */
	psDeviceNode->pfnAlignmentCheck = RGXAlignmentCheck;
#endif

	/*Register callback for checking the supported features and getting the
	 * corresponding values */
	psDeviceNode->pfnCheckDeviceFeature = RGXBvncCheckFeatureSupported;
	psDeviceNode->pfnGetDeviceFeatureValue = RGXBvncGetSupportedFeatureValue;

	/*Set up required support for dummy page */
	OSAtomicWrite(&(psDeviceNode->sDummyPage.atRefCounter), 0);

	/*Set the order to 0 */
	psDeviceNode->sDummyPage.sDummyPageHandle.ui32Order = 0;

	/*Set the size of the Dummy page to zero */
	psDeviceNode->sDummyPage.ui32Log2DummyPgSize = 0;

	/*Set the Dummy page phys addr */
	psDeviceNode->sDummyPage.ui64DummyPgPhysAddr = MMU_BAD_PHYS_ADDR;

	/*The lock type need to be dispatch type here because it can be acquired from MISR (Z-buffer) path */
	eError = OSLockCreate(&psDeviceNode->sDummyPage.psDummyPgLock ,LOCK_TYPE_DISPATCH);
	if (PVRSRV_OK != eError)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create dummy page lock", __func__));
		return eError;
	}
#if defined(PDUMP)
	psDeviceNode->sDummyPage.hPdumpDummyPg = NULL;
#endif

	/*********************
	 * Device info setup *
	 *********************/
	/* Allocate device control block */
	psDevInfo = OSAllocZMem(sizeof(*psDevInfo));
	if (psDevInfo == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"DevInitRGXPart1 : Failed to alloc memory for DevInfo"));
		return (PVRSRV_ERROR_OUT_OF_MEMORY);
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

	eError = OSWRLockCreate(&(psDevInfo->hRaytraceCtxListLock));
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create raytrace context list lock", __func__));
		goto e4;
	}

	eError = OSWRLockCreate(&(psDevInfo->hKickSyncCtxListLock));
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create kick sync context list lock", __func__));
		goto e5;
	}

	eError = OSWRLockCreate(&(psDevInfo->hMemoryCtxListLock));
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create memory context list lock", __func__));
		goto e6;
	}

	eError = OSLockCreate(&psDevInfo->hLockKCCBDeferredCommandsList,LOCK_TYPE_PASSIVE);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to KCCB deferred commands list lock", __func__));
		goto e7;
	}
	dllist_init(&(psDevInfo->sKCCBDeferredCommandsListHead));

	dllist_init(&(psDevInfo->sRenderCtxtListHead));
	dllist_init(&(psDevInfo->sComputeCtxtListHead));
	dllist_init(&(psDevInfo->sTransferCtxtListHead));
	dllist_init(&(psDevInfo->sTDMCtxtListHead));
	dllist_init(&(psDevInfo->sRaytraceCtxtListHead));
	dllist_init(&(psDevInfo->sKickSyncCtxtListHead));

	dllist_init(&(psDevInfo->sCommonCtxtListHead));
	psDevInfo->ui32CommonCtxtCurrentID = 1;


	eError = OSWRLockCreate(&psDevInfo->hCommonCtxtListLock);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create common context list lock", __func__));
		goto e8;
	}

	dllist_init(&psDevInfo->sMemoryContextList);

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
		PVR_DPF((PVR_DBG_ERROR,"%s: Failed to create RGX register mapping", __func__));
		eError = PVRSRV_ERROR_BAD_MAPPING;
		goto e9;
	}
#endif

	psDeviceNode->pvDevice = psDevInfo;

	eError = RGXBvncInitialiseConfiguration(psDeviceNode);
	if (PVRSRV_OK != eError)
	{
		PVR_DPF((PVR_DBG_ERROR,"%s: Unsupported HW device detected by driver", __func__));
		goto e10;
	}

	/* pdump info about the core */
	PDUMPCOMMENT("RGX Version Information (KM): %d.%d.%d.%d",
	             psDevInfo->sDevFeatureCfg.ui32B,
	             psDevInfo->sDevFeatureCfg.ui32V,
	             psDevInfo->sDevFeatureCfg.ui32N,
	             psDevInfo->sDevFeatureCfg.ui32C);

	eError = RGXInitHeaps(psDevInfo, psDevMemoryInfo,
						  &psDeviceNode->sDummyPage.ui32Log2DummyPgSize);
	if (eError != PVRSRV_OK)
	{
		goto e10;
	}

	eError = RGXHWPerfInit(psDevInfo);
	PVR_LOGG_IF_ERROR(eError, "RGXHWPerfInit", e10);

	/* Register callback for dumping debug info */
	eError = PVRSRVRegisterDbgRequestNotify(&psDevInfo->hDbgReqNotify,
											psDeviceNode,
											RGXDebugRequestNotify,
											DEBUG_REQUEST_SYS,
											psDevInfo);
	PVR_LOG_IF_ERROR(eError, "PVRSRVRegisterDbgRequestNotify");

	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, MIPS))
	{
		RGXMipsMMUInit_Register(psDeviceNode);
	}

	/* The device shared-virtual-memory heap address-space size is stored here for faster
	   look-up without having to walk the device heap configuration structures during
	   client device connection  (i.e. this size is relative to a zero-based offset) */
	if (RGX_IS_BRN_SUPPORTED(psDevInfo, 65273))
	{
		psDeviceNode->ui64GeneralSVMHeapTopVA = 0;
	}else
	{
		psDeviceNode->ui64GeneralSVMHeapTopVA = RGX_GENERAL_SVM_HEAP_BASE + RGX_GENERAL_SVM_HEAP_SIZE;
	}

	if (NULL != psDeviceNode->psDevConfig->pfnSysDevFeatureDepInit)
	{
		psDeviceNode->psDevConfig->pfnSysDevFeatureDepInit(psDeviceNode->psDevConfig, \
				psDevInfo->sDevFeatureCfg.ui64Features);
	}

	/* Initialise the device dependent bridges */
	eError = DeviceDepBridgeInit(psDevInfo->sDevFeatureCfg.ui64Features);
	PVR_LOG_IF_ERROR(eError, "DeviceDepBridgeInit");

#if defined(SUPPORT_POWER_SAMPLING_VIA_DEBUGFS)
	eError = OSLockCreate(&psDevInfo->hCounterDumpingLock, LOCK_TYPE_PASSIVE);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create lock for counter sampling.", __func__));
		goto e10;
	}
#endif

	return PVRSRV_OK;

e10:
#if !defined(NO_HARDWARE)
	OSUnMapPhysToLin((void __force *) psDevInfo->pvRegsBaseKM,
							 psDevInfo->ui32RegSize,
							 PVRSRV_MEMALLOCFLAG_CPU_UNCACHED);
e9:
#endif /* !NO_HARDWARE */
	OSWRLockDestroy(psDevInfo->hCommonCtxtListLock);
e8:
	OSWRLockDestroy(psDevInfo->hMemoryCtxListLock);
e7:
	OSLockDestroy(psDevInfo->hLockKCCBDeferredCommandsList);
e6:
	OSWRLockDestroy(psDevInfo->hKickSyncCtxListLock);
e5:
	OSWRLockDestroy(psDevInfo->hRaytraceCtxListLock);
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

	/*Destroy the dummy page lock created above */
	OSLockDestroy(psDeviceNode->sDummyPage.psDummyPgLock);
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

PVRSRV_ERROR
PVRSRVRGXInitFinaliseFWImageKM(PVRSRV_DEVICE_NODE *psDeviceNode)
{

	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, MIPS))
	{
		void *pvFWImage;
		PVRSRV_ERROR eError;

		eError = DevmemAcquireCpuVirtAddr(psDevInfo->psRGXFWDataMemDesc, &pvFWImage);

		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "PVRSRVRGXInitFinaliseFWImageKM: Acquire mapping for FW data failed (%u)",
					 eError));
			return eError;
		}

		eError = RGXBootldrDataInit(psDeviceNode, pvFWImage);

		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "PVRSRVRGXInitLoadFWImageKM: ELF parameters injection failed (%u)",
					 eError));
			return eError;
		}

		DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWDataMemDesc);

	}
	return PVRSRV_OK;
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
#if defined(NO_HARDWARE) || defined(EMULATOR)
	IMG_PCHAR pszFormatString = "Rogue Version: %s (SW)";
#else
	IMG_PCHAR pszFormatString = "Rogue Version: %s (HW)";
#endif
	PVRSRV_RGXDEV_INFO *psDevInfo;
	size_t uiStringLength;

	if (psDeviceNode == NULL || ppszVersionString == NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDevInfo = (PVRSRV_RGXDEV_INFO *)psDeviceNode->pvDevice;

	if (NULL == psDevInfo->sDevFeatureCfg.pszBVNCString)
	{
		IMG_CHAR pszBVNCInfo[MAX_BVNC_STRING_LEN];
		size_t uiBVNCStringSize;

		OSSNPrintf(pszBVNCInfo, MAX_BVNC_STRING_LEN, "%d.%d.%d.%d", \
				psDevInfo->sDevFeatureCfg.ui32B, \
				psDevInfo->sDevFeatureCfg.ui32V, \
				psDevInfo->sDevFeatureCfg.ui32N, \
				psDevInfo->sDevFeatureCfg.ui32C);

		uiBVNCStringSize = (OSStringLength(pszBVNCInfo) + 1) * sizeof(IMG_CHAR);
		psDevInfo->sDevFeatureCfg.pszBVNCString = OSAllocMem(uiBVNCStringSize);
		if (NULL == psDevInfo->sDevFeatureCfg.pszBVNCString)
		{
			PVR_DPF((PVR_DBG_MESSAGE,
					 "%s: Allocating memory for BVNC Info string failed",
					 __func__));
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}

		OSCachedMemCopy(psDevInfo->sDevFeatureCfg.pszBVNCString,pszBVNCInfo,uiBVNCStringSize);
	}

	uiStringLength = OSStringLength(psDevInfo->sDevFeatureCfg.pszBVNCString) +
	                 OSStringLength(pszFormatString);
	*ppszVersionString = OSAllocMem(uiStringLength);
	if (*ppszVersionString == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	OSSNPrintf(*ppszVersionString, uiStringLength, pszFormatString,
			psDevInfo->sDevFeatureCfg.pszBVNCString);

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
/*!
*******************************************************************************

 @Function		RGXVzInitCreateFWKernelMemoryContext

 @Description	Called to perform additional initialisation during firmware
 	 	 	 	kernel context creation.
******************************************************************************/

static PVRSRV_ERROR RGXVzInitCreateFWKernelMemoryContext(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_NATIVE, PVRSRV_OK);
	return RGXVzCreateFWKernelMemoryContext(psDeviceNode);
}
/*!
*******************************************************************************

 @Function		RGXVzDeInitDestroyFWKernelMemoryContext

 @Description	Called to perform additional deinitialisation during firmware
 	 	 	 	kernel context destruction.
******************************************************************************/

static void RGXVzDeInitDestroyFWKernelMemoryContext(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_VZ_RETN_IF_MODE(DRIVER_MODE_NATIVE);
	RGXVzDestroyFWKernelMemoryContext(psDeviceNode);
}

/*!
*******************************************************************************

 @Function		RGXVzInitHeaps

 @Description	Called to perform additional initialisation
******************************************************************************/
static PVRSRV_ERROR RGXVzInitHeaps(DEVICE_MEMORY_INFO *psNewMemoryInfo,
								   DEVMEM_HEAP_BLUEPRINT *psDeviceMemoryHeapCursor)
{
	IMG_UINT32 uiIdx;
	IMG_UINT32 uiStringLength;
	IMG_UINT32 uiStringLengthMax = 32;
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_GUEST, PVRSRV_OK);
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_NATIVE, PVRSRV_OK);

	uiStringLength = MIN(sizeof(RGX_FIRMWARE_GUEST_RAW_HEAP_IDENT), uiStringLengthMax + 1);

	/* Create additional guest OSID firmware heaps */
	for (uiIdx = 1; uiIdx < RGXFW_NUM_OS; uiIdx++)
	{
		/* Start by allocating memory for this guest OSID heap identification string */
		psDeviceMemoryHeapCursor->pszName = OSAllocMem(uiStringLength * sizeof(IMG_CHAR));
		if (psDeviceMemoryHeapCursor->pszName == NULL)
		{
			for (uiIdx = uiIdx - 1; uiIdx > 0; uiIdx--)
			{
				void *pzsName = (void *)psDeviceMemoryHeapCursor->pszName;
				psDeviceMemoryHeapCursor--;
				OSFreeMem(pzsName);
			}

			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}

		/* Append the guest OSID number to the RGX_FIRMWARE_GUEST_RAW_HEAP_IDENT string */
		OSSNPrintf((IMG_CHAR *)psDeviceMemoryHeapCursor->pszName, uiStringLength, RGX_FIRMWARE_GUEST_RAW_HEAP_IDENT, uiIdx);

		/* Use the common blueprint template support function to initialise the heap */
		*psDeviceMemoryHeapCursor = _blueprint_init((IMG_CHAR *)psDeviceMemoryHeapCursor->pszName,
												    RGX_FIRMWARE_RAW_HEAP_BASE + (uiIdx * RGX_FIRMWARE_RAW_HEAP_SIZE),
												    RGX_FIRMWARE_RAW_HEAP_SIZE,
												    0,
												    0);

		/* Append additional guest(s) firmware heap to host driver firmware context heap configuration */
		psNewMemoryInfo->psDeviceMemoryHeapConfigArray[1].uiNumHeaps += 1;

		/* advance to the next heap */
		psDeviceMemoryHeapCursor++;
	}

	return PVRSRV_OK;
}

/*!
*******************************************************************************

 @Function		RGXVzDeInitHeaps

 @Description	Called to perform additional deinitialisation
******************************************************************************/
static void RGXVzDeInitHeaps(DEVICE_MEMORY_INFO *psDevMemoryInfo)
{
	IMG_UINT32 uiIdx;
	IMG_UINT64 uiBase, uiSpan;
	DEVMEM_HEAP_BLUEPRINT *psDeviceMemoryHeapCursor;
	PVRSRV_VZ_RETN_IF_MODE(DRIVER_MODE_NATIVE);
	PVRSRV_VZ_RETN_IF_MODE(DRIVER_MODE_GUEST);

	psDeviceMemoryHeapCursor = psDevMemoryInfo->psDeviceMemoryHeap;
	uiBase = RGX_FIRMWARE_RAW_HEAP_BASE + RGX_FIRMWARE_RAW_HEAP_SIZE;
	uiSpan = uiBase + ((RGXFW_NUM_OS - 1) * RGX_FIRMWARE_RAW_HEAP_SIZE);

	for (uiIdx = 1; uiIdx < RGXFW_NUM_OS; uiIdx++)
	{
		/* Safe to do as the guest firmware heaps are last in the list */
		if (psDeviceMemoryHeapCursor->sHeapBaseAddr.uiAddr >= uiBase &&
			psDeviceMemoryHeapCursor->sHeapBaseAddr.uiAddr <  uiSpan)
		{
			void *pszName = (void*)psDeviceMemoryHeapCursor->pszName;
			OSFreeMem(pszName);
			uiIdx += 1;
		}

		psDeviceMemoryHeapCursor++;
	}
}

/******************************************************************************
 End of file (rgxinit.c)
******************************************************************************/
