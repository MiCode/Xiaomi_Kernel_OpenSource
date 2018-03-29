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
#include "syscommon.h"
#include "rgxheapconfig.h"
#include "rgxpower.h"

#include "rgxinit.h"
#if defined(SUPPORT_PVRSRV_GPUVIRT)
#include "rgxinit_vz.h"
#endif

#include "pdump_km.h"
#include "handle.h"
#include "allocmem.h"
#include "devicemem.h"
#include "devicemem_pdump.h"
#include "rgxmem.h"
#include "sync_internal.h"

#include "rgxutils.h"
#include "rgxfwutils.h"
#include "rgx_fwif_km.h"

#include "rgxmmuinit.h"
#if defined(RGX_FEATURE_MIPS)
#include "rgxmipsmmuinit.h"
#endif
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

#if defined(PDUMP)
#include "rgxstartstop.h"
#endif

#if defined(SUPPORT_KERNEL_SRVINIT)
#include "rgx_fwif_alignchecks.h"
#endif

#if defined(SUPPORT_PDVFS)
#include "rgxpdvfs.h"
#endif

static PVRSRV_ERROR RGXDevInitCompatCheck(PVRSRV_DEVICE_NODE *psDeviceNode, IMG_UINT32 ui32ClientBuildOptions);
static PVRSRV_ERROR RGXDevVersionString(PVRSRV_DEVICE_NODE *psDeviceNode, IMG_CHAR **ppszVersionString);
static PVRSRV_ERROR RGXDevClockSpeed(PVRSRV_DEVICE_NODE *psDeviceNode, IMG_PUINT32  pui32RGXClockSpeed);
static PVRSRV_ERROR RGXSoftReset(PVRSRV_DEVICE_NODE *psDeviceNode, IMG_UINT64  ui64ResetValue1, IMG_UINT64  ui64ResetValue2);

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
	/* time of the last LISR invocation */
	IMG_UINT64 ui64Clockns;
} LISR_EXECUTION_INFO;

/* information about the last execution of the LISR */
static LISR_EXECUTION_INFO g_sLISRExecutionInfo;

#endif

#if !defined(NO_HARDWARE)
#if defined(PVRSRV_GPUVIRT_GUESTDRV)
void RGX_WaitForInterruptsTimeout(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	OSScheduleMISR(psDevInfo->pvMISRData);
}

/*
	Guest Driver RGX LISR Handler
*/
static IMG_BOOL RGX_LISRHandler (void *pvData)
{
	PVRSRV_DEVICE_NODE *psDeviceNode = pvData;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

	if (psDevInfo->bRGXPowered == IMG_FALSE)
	{
		return IMG_FALSE;
	}

	OSScheduleMISR(psDevInfo->pvMISRData);
	return IMG_TRUE;
}
#else

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

void RGX_WaitForInterruptsTimeout(PVRSRV_RGXDEV_INFO *psDevInfo)
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
				"RGX FW thread %u: InterruptCountSnapshot: 0x%X", \
				ui32TID, g_sLISRExecutionInfo.aui32InterruptCountSnapshot[ui32TID]));
	}
#else
	PVR_DPF((PVR_DBG_ERROR, "No further information available. Please enable PVRSRV_DEBUG_LISR_EXECUTION"));
#endif


	if(psRGXFWIfTraceBuf->ePowState != RGXFWIF_POW_OFF)
	{
		PVR_DPF((PVR_DBG_ERROR, "_WaitForInterruptsTimeout: FW pow state is not OFF (is %u)",
						(unsigned int) psRGXFWIfTraceBuf->ePowState));
	}

	bScheduleMISR = SampleIRQCount(psRGXFWIfTraceBuf->aui32InterruptCount, 
								   psDevInfo->aui32SampleIRQCount);

	if (bScheduleMISR)
	{
		OSScheduleMISR(psDevInfo->pvMISRData);
		
		if(psDevInfo->pvAPMISRData != NULL)
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
	IMG_BOOL bInterruptProcessed = IMG_FALSE;
	IMG_UINT32 ui32IRQStatus;
	RGXFWIF_TRACEBUF *psRGXFWIfTraceBuf = psDevInfo->psRGXFWIfTraceBuf;

#if defined(PVRSRV_DEBUG_LISR_EXECUTION)
	IMG_UINT32 ui32TID;

	for (ui32TID = 0; ui32TID < RGXFW_THREAD_NUM; ui32TID++)
	{
		g_sLISRExecutionInfo.aui32InterruptCountSnapshot[ui32TID ] = 
			psRGXFWIfTraceBuf->aui32InterruptCount[ui32TID];
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

	ui32IRQStatus = OSReadHWReg32(psDevInfo->pvRegsBaseKM, RGXFW_CR_IRQ_STATUS);
	if (ui32IRQStatus & RGXFW_CR_IRQ_STATUS_EVENT_EN)
	{
#if defined(PVRSRV_DEBUG_LISR_EXECUTION)
		g_sLISRExecutionInfo.ui32State |= RGX_LISR_EVENT_EN;
#endif

		OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGXFW_CR_IRQ_CLEAR, RGXFW_CR_IRQ_CLEAR_MASK);

#if defined(RGX_FEATURE_OCPBUS)
		OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_OCP_IRQSTATUS_2, RGX_CR_OCP_IRQSTATUS_2_RGX_IRQ_STATUS_EN);
#endif

		bInterruptProcessed = SampleIRQCount(psRGXFWIfTraceBuf->aui32InterruptCount, 
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

static void RGXCheckFWActivePowerState(void *psDevice)
{
	PVRSRV_DEVICE_NODE	*psDeviceNode = psDevice;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	RGXFWIF_TRACEBUF *psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;
	PVRSRV_ERROR eError = PVRSRV_OK;

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
	IMG_UINT64 aui64TmpCounters[RGXFWIF_GPU_UTIL_STATE_NUM] = {0};
	IMG_UINT64 ui64TimeNow;
	IMG_UINT64 ui64LastPeriod;
	IMG_UINT64 ui64LastWord = 0, ui64LastState = 0, ui64LastTime = 0;
	IMG_UINT32 i = 0;


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


	/***** (2) Get latest data from shared area *****/

	OSLockAcquire(psDevInfo->hGPUUtilLock);

	/* Read the timer before reading the latest stats from the shared
	 * area, discard it later in case of state updates after this point.
	 */
	ui64TimeNow = RGXFWIF_GPU_UTIL_GET_TIME(OSClockns64());
	OSMemoryBarrier();

	/* Keep reading the counters until the values stabilise as the FW
	 * might be updating them at the same time.
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

	OSLockRelease(psDevInfo->hGPUUtilLock);

	if (i == MAX_ITERATIONS)
	{
		PVR_DPF((PVR_DBG_WARNING, "RGXGetGpuUtilStats could not get reliable data within a short time."));
		return PVRSRV_ERROR_TIMEOUT;
	}


	/***** (3) Compute return stats and update aggregate stats *****/

	/* Update temp counters to account for the time since the last update to the shared ones */
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

	/* Update aggregate stats for the current user */
	psAggregateStats->ui64GpuStatActiveLow  += psReturnStats->ui64GpuStatActiveLow;
	psAggregateStats->ui64GpuStatIdle       += psReturnStats->ui64GpuStatIdle;
	psAggregateStats->ui64GpuStatActiveHigh += psReturnStats->ui64GpuStatActiveHigh;
	psAggregateStats->ui64GpuStatBlocked    += psReturnStats->ui64GpuStatBlocked;


	/***** (4) Convert return stats to microseconds *****/

	psReturnStats->ui64GpuStatActiveLow  = OSDivide64(psReturnStats->ui64GpuStatActiveLow, 1000, &i);
	psReturnStats->ui64GpuStatIdle       = OSDivide64(psReturnStats->ui64GpuStatIdle, 1000, &i);
	psReturnStats->ui64GpuStatActiveHigh = OSDivide64(psReturnStats->ui64GpuStatActiveHigh, 1000, &i);
	psReturnStats->ui64GpuStatBlocked    = OSDivide64(psReturnStats->ui64GpuStatBlocked, 1000, &i);
	psReturnStats->ui64GpuStatCumulative = OSDivide64(psReturnStats->ui64GpuStatCumulative, 1000, &i);

	/* Check that the return stats make sense */
	if(psReturnStats->ui64GpuStatCumulative == 0)
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
#endif /* defined(PVRSRV_GPUVIRT_GUESTDRV) */

PVRSRV_ERROR RGXRegisterGpuUtilStats(IMG_HANDLE *phGpuUtilUser)
{
	RGXFWIF_GPU_UTIL_STATS *psAggregateStats;

	psAggregateStats = OSAllocMem(sizeof(RGXFWIF_GPU_UTIL_STATS));
	if(psAggregateStats == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psAggregateStats->ui64GpuStatActiveLow  = 0;
	psAggregateStats->ui64GpuStatIdle       = 0;
	psAggregateStats->ui64GpuStatActiveHigh = 0;
	psAggregateStats->ui64GpuStatBlocked    = 0;

	/* Not used */
	psAggregateStats->bValid = IMG_FALSE;
	psAggregateStats->ui64GpuStatCumulative = 0;

	*phGpuUtilUser = psAggregateStats;

	return PVRSRV_OK;
}

PVRSRV_ERROR RGXUnregisterGpuUtilStats(IMG_HANDLE hGpuUtilUser)
{
	RGXFWIF_GPU_UTIL_STATS *psAggregateStats;

	if(hGpuUtilUser == NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psAggregateStats = hGpuUtilUser;
	OSFreeMem(psAggregateStats);

	return PVRSRV_OK;
}

/*
	RGX MISR Handler
*/
static void RGX_MISRHandler (void *pvData)
{
	PVRSRV_DEVICE_NODE *psDeviceNode = pvData;

	/* Give the HWPerf service a chance to transfer some data from the FW
	 * buffer to the host driver transport layer buffer.
	 */
	RGXHWPerfDataStoreCB(psDeviceNode);

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

#if !defined(PVRSRV_GPUVIRT_GUESTDRV)
	/* Calibrate the GPU frequency and recorrelate Host and FW timers (done every few seconds) */
	RGXGPUFreqCalibrateCorrelatePeriodic(psDeviceNode);
#endif
}
#endif /* !defined(NO_HARDWARE) */

#if defined(RGX_FEATURE_MIPS)
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


	/* Rogue Registers physical address */
	PhysHeapCpuPAddrToDevPAddr(psDevInfo->psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL],
							   1, &sPhyAddr, &(psDeviceNode->psDevConfig->sRegsCpuPBase));
	pui64BootConfig[RGXMIPSFW_ROGUE_REGS_BASE_PHYADDR_OFFSET] = sPhyAddr.uiAddr;

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
	PMR *psFWDataPMR = (PMR *)(psDevInfo->psRGXFWDataMemDesc->psImport->hPMR);
	IMG_DEV_PHYADDR sTmpAddr;
	IMG_UINT32 ui32BootConfOffset, ui32ParamOffset;
	PVRSRV_ERROR eError;

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

	eError = PDumpPTBaseObjectToMem64(psDeviceNode->pszMMUPxPDumpMemSpaceName,
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
#endif /* RGX_FEATURE_MIPS */

PVRSRV_ERROR PVRSRVGPUVIRTPopulateLMASubArenasKM(CONNECTION_DATA    * psConnection,
                                                 PVRSRV_DEVICE_NODE	* psDeviceNode,
                                                 IMG_UINT32         ui32NumElements,
                                                 IMG_UINT32         aui32Elements[],
                                                 IMG_BOOL         bEnableTrustedDeviceAceConfig)
{
#if defined(SUPPORT_GPUVIRT_VALIDATION)
{
	IMG_UINT32	ui32OS, ui32Region, ui32Counter=0;
	IMG_UINT32	aui32OSidMin[GPUVIRT_VALIDATION_NUM_OS][GPUVIRT_VALIDATION_NUM_REGIONS];
	IMG_UINT32	aui32OSidMax[GPUVIRT_VALIDATION_NUM_OS][GPUVIRT_VALIDATION_NUM_REGIONS];

	PVR_UNREFERENCED_PARAMETER(ui32NumElements);

	for (ui32OS = 0; ui32OS < GPUVIRT_VALIDATION_NUM_OS; ui32OS++)
	{
		for (ui32Region = 0; ui32Region < GPUVIRT_VALIDATION_NUM_REGIONS; ui32Region++)
		{
			aui32OSidMin[ui32OS][ui32Region] = aui32Elements[ui32Counter++];
			aui32OSidMax[ui32OS][ui32Region] = aui32Elements[ui32Counter++];

			PVR_DPF((PVR_DBG_MESSAGE,"OS=%u, Region=%u, Min=%u, Max=%u", ui32OS, ui32Region, aui32OSidMin[ui32OS][ui32Region], aui32OSidMax[ui32OS][ui32Region]));
		}
	}

	PopulateLMASubArenas(psDeviceNode, aui32OSidMin, aui32OSidMax);

    #if defined(RGX_FEATURE_AXI_ACELITE) && defined(EMULATOR)
    if (bEnableTrustedDeviceAceConfig)
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
	PVR_UNREFERENCED_PARAMETER(ui32NumElements);
	PVR_UNREFERENCED_PARAMETER(aui32Elements);
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
	psDevInfo->sPowerParams.psDevInfo = psDevInfo;
	psDevInfo->sPowerParams.psDevConfig = psDevConfig;
#if defined(PDUMP)
	psDevInfo->sPowerParams.ui32PdumpFlags = PDUMP_FLAGS_CONTINUOUS;
#endif

#if defined(RGX_FEATURE_META)
	{
		IMG_DEV_PHYADDR sKernelMMUCtxPCAddr;

		eError = MMU_AcquireBaseAddr(psDevInfo->psKernelMMUCtx,
		                             &sKernelMMUCtxPCAddr);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "RGXSetPowerParams: Failed to acquire Kernel MMU Ctx page catalog"));
			return eError;
		}

		psDevInfo->sPowerParams.sPCAddr = sKernelMMUCtxPCAddr;
	}
#else
	{
		PMR *psFWCodePMR = (PMR *)(psDevInfo->psRGXFWCodeMemDesc->psImport->hPMR);
		PMR *psFWDataPMR = (PMR *)(psDevInfo->psRGXFWDataMemDesc->psImport->hPMR);
		IMG_DEV_PHYADDR sPhyAddr;
		IMG_BOOL bValid;

		/* The physical address of the GPU registers needs to be translated
		 * in case we are in a LMA scenario
		 */
		PhysHeapCpuPAddrToDevPAddr(psDevInfo->psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL],
		                           1,
		                           &sPhyAddr,
		                           &(psDevConfig->sRegsCpuPBase));

		psDevInfo->sPowerParams.sGPURegAddr = sPhyAddr;

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

		psDevInfo->sPowerParams.sBootRemapAddr = sPhyAddr;

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

		psDevInfo->sPowerParams.sDataRemapAddr = sPhyAddr;

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

		psDevInfo->sPowerParams.sCodeRemapAddr = sPhyAddr;
	}
#endif

#if defined(SUPPORT_TRUSTED_DEVICE) && !defined(NO_HARDWARE)
	/* Send information used on power transitions to the trusted device as
	 * in this setup the driver cannot start/stop the GPU and perform resets
	 */
	if (psDevConfig->pfnTDSetPowerParams)
	{
		PVRSRV_TD_POWER_PARAMS sTDPowerParams;

#if defined(RGX_FEATURE_META)
		sTDPowerParams.sPCAddr = psDevInfo->sPowerParams.sPCAddr;
#else
		sTDPowerParams.sGPURegAddr    = psDevInfo->sPowerParams.sGPURegAddr;
		sTDPowerParams.sBootRemapAddr = psDevInfo->sPowerParams.sBootRemapAddr;
		sTDPowerParams.sCodeRemapAddr = psDevInfo->sPowerParams.sCodeRemapAddr;
		sTDPowerParams.sDataRemapAddr = psDevInfo->sPowerParams.sDataRemapAddr;
#endif
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
 * PVRSRVRGXInitDevPart2KM
 */ 
IMG_EXPORT
PVRSRV_ERROR PVRSRVRGXInitDevPart2KM (CONNECTION_DATA       *psConnection,
                                      PVRSRV_DEVICE_NODE	*psDeviceNode,
									  RGX_INIT_COMMAND		*psDbgScript,
									  IMG_UINT32			ui32DeviceFlags,
									  IMG_UINT32			ui32HWPerfHostBufSizeKB,
									  IMG_UINT32			ui32HWPerfHostFilter,
									  RGX_ACTIVEPM_CONF		eActivePMConf,
									  PMR					*psFWCodePMR,
									  PMR					*psFWDataPMR,
									  PMR					*psFWCorePMR,
									  PMR					*psHWPerfPMR)
{
	PVRSRV_ERROR			eError;
	PVRSRV_RGXDEV_INFO		*psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_DEV_POWER_STATE	eDefaultPowerState = PVRSRV_DEV_POWER_STATE_ON;
	PVRSRV_DEVICE_CONFIG	*psDevConfig = psDeviceNode->psDevConfig;

#if !defined(PVRSRV_GPUVIRT_GUESTDRV)
#if defined(PDUMP) && defined (RGX_FEATURE_MIPS)
	RGXPDumpBootldrData(psDeviceNode, psDevInfo);
#endif

#if defined(TIMING) || defined(DEBUG)
	OSUserModeAccessToPerfCountersEn();
#endif
#endif

	/* Passing down the PMRs to destroy their handles */
	PVR_UNREFERENCED_PARAMETER(psFWCodePMR);
	PVR_UNREFERENCED_PARAMETER(psFWDataPMR);
	PVR_UNREFERENCED_PARAMETER(psFWCorePMR);
	PVR_UNREFERENCED_PARAMETER(psHWPerfPMR);

	PDUMPCOMMENT("RGX Initialisation Part 2");

	/*
	 * Map RGX Registers
	 */
#if !defined(NO_HARDWARE)
	psDevInfo->pvRegsBaseKM = OSMapPhysToLin(psDevConfig->sRegsCpuPBase,
										     psDevConfig->ui32RegsSize,
										     PVRSRV_MEMALLOCFLAG_CPU_UNCACHED);

	if (psDevInfo->pvRegsBaseKM == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXInitDevPart2KM: Failed to create RGX register mapping"));
		return PVRSRV_ERROR_BAD_MAPPING;
	}
#else
	psDevInfo->pvRegsBaseKM = NULL;
#endif /* !NO_HARDWARE */

	psDevInfo->ui32RegSize = psDevConfig->ui32RegsSize;
	psDevInfo->sRegsPhysBase = psDevConfig->sRegsCpuPBase;

	/* Initialise Device Flags */
	psDevInfo->ui32DeviceFlags = 0;
	RGXSetDeviceFlags(psDevInfo, ui32DeviceFlags, IMG_TRUE);

	/* Initialise HWPerfHost buffer. */
	if (RGXHWPerfHostInit(ui32HWPerfHostBufSizeKB) == PVRSRV_OK)
	{
		/* If HWPerf enabled allocate all resources for the host side buffer. */
		if (ui32DeviceFlags & RGXKMIF_DEVICE_STATE_HWPERF_HOST_EN)
		{
			if (RGXHWPerfHostInitOnDemandResources() == PVRSRV_OK)
			{
				RGXHWPerfHostSetEventFilter(ui32HWPerfHostFilter);
			}
			else
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

#if defined(SUPPORT_GPUTRACE_EVENTS)
	{
		/* The tracing might have already been enabled by pvr/gpu_tracing_on
		 * but if SUPPORT_KERNEL_SRVINIT == 1 the HWPerf has just been
		 * allocated so the initialisation wasn't full.
		 * RGXHWPerfFTraceGPUEventsEnabledSet() will perform full
		 * initialisation in such case. */
		IMG_BOOL bInit = IMG_FALSE;

		/* This can happen if SUPPORT_KERNEL_SRVINIT == 1. */
		if (PVRGpuTracePreEnabled())
		{
			bInit = IMG_TRUE;
		}
		else
		{
			bInit = ui32DeviceFlags & RGXKMIF_DEVICE_STATE_FTRACE_EN ?
				IMG_TRUE : IMG_FALSE;
		}
		RGXHWPerfFTraceGPUEventsEnabledSet(bInit);
	}
#endif

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

#if 1//defined(SUPPORT_RAY_TRACING)
	eError = OSLockCreate(&psDevInfo->hLockRPMFreeList,LOCK_TYPE_PASSIVE);
	PVR_ASSERT(eError == PVRSRV_OK);
	dllist_init(&psDevInfo->sRPMFreeListHead);
	psDevInfo->ui32RPMFreelistCurrID = 1;
	eError = OSLockCreate(&psDevInfo->hLockRPMContext,LOCK_TYPE_PASSIVE);
	PVR_ASSERT(eError == PVRSRV_OK);
#endif

#if defined(SUPPORT_PAGE_FAULT_DEBUG)
	eError = OSLockCreate(&psDevInfo->hDebugFaultInfoLock, LOCK_TYPE_PASSIVE);

	if(eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = OSLockCreate(&psDevInfo->hMMUCtxUnregLock, LOCK_TYPE_PASSIVE);

	if(eError != PVRSRV_OK)
	{
		return eError;
	}
#endif
#if defined(RGX_FEATURE_MIPS)
	eError = OSLockCreate(&psDevInfo->hNMILock, LOCK_TYPE_DISPATCH);

	if(eError != PVRSRV_OK)
	{
		return eError;
	}
#endif

#if !defined(PVRSRV_GPUVIRT_GUESTDRV)
	/* Allocate DVFS Table */
	psDevInfo->psGpuDVFSTable = OSAllocZMem(sizeof(*(psDevInfo->psGpuDVFSTable)));
	if (psDevInfo->psGpuDVFSTable == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXInitDevPart2KM: failed to allocate gpu dvfs table storage"));
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	/* Reset DVFS Table */
	psDevInfo->psGpuDVFSTable->ui32CurrentDVFSId = 0;
	psDevInfo->psGpuDVFSTable->aui32DVFSClock[0] = 0;

	/* Setup GPU utilisation stats update callback */
#if !defined(NO_HARDWARE)
	psDevInfo->pfnGetGpuUtilStats = RGXGetGpuUtilStats;
#endif

	eError = OSLockCreate(&psDevInfo->hGPUUtilLock, LOCK_TYPE_PASSIVE);
	PVR_ASSERT(eError == PVRSRV_OK);

	eDefaultPowerState = PVRSRV_DEV_POWER_STATE_ON;

	/* set-up the Active Power Mgmt callback */
#if !defined(NO_HARDWARE)
	{
		RGX_DATA *psRGXData = (RGX_DATA*) psDeviceNode->psDevConfig->hDevData;
		IMG_BOOL bSysEnableAPM = psRGXData->psRGXTimingInfo->bEnableActivePM;
		IMG_BOOL bEnableAPM = ((eActivePMConf == RGX_ACTIVEPM_DEFAULT) && bSysEnableAPM) ||
							   (eActivePMConf == RGX_ACTIVEPM_FORCE_ON);
#if defined(SUPPORT_PVRSRV_GPUVIRT)
		/* Disable APM for now */
		bEnableAPM = IMG_FALSE;
#endif
		if (bEnableAPM)
		{
			eError = OSInstallMISR(&psDevInfo->pvAPMISRData, RGXCheckFWActivePowerState, psDeviceNode);
			if (eError != PVRSRV_OK)
			{
				return eError;
			}

			/* Prevent the device being woken up before there is something to do. */
			eDefaultPowerState = PVRSRV_DEV_POWER_STATE_OFF;
		}
	}
#endif
#endif

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

#if defined(PVRSRV_GPUVIRT_GUESTDRV)
	/* 
	 * Guest drivers do not perform on-chip firmware
	 *  - Loading, Initialization and Management
	 */
	PVR_UNREFERENCED_PARAMETER(psDbgScript);
	PVR_UNREFERENCED_PARAMETER(eActivePMConf);
#else
	/*
	 * Copy scripts
	 */
	OSMemCopy(psDevInfo->psScripts->asDbgCommands, psDbgScript,
			  RGX_MAX_DEBUG_COMMANDS * sizeof(*psDbgScript));

#if defined(PDUMP)
	/* Run RGXStop with the correct PDump flags to feed the last-frame deinit buffer */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_DEINIT, "RGX deinitialisation commands");

	psDevInfo->sPowerParams.ui32PdumpFlags |= PDUMP_FLAGS_DEINIT | PDUMP_FLAGS_NOHW;

	eError = RGXStop(&psDevInfo->sPowerParams);
	if (eError != PVRSRV_OK) return eError;

	psDevInfo->sPowerParams.ui32PdumpFlags &= ~(PDUMP_FLAGS_DEINIT | PDUMP_FLAGS_NOHW);
#endif
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
	eError = OSInstallMISR(&psDevInfo->pvMISRData,
									RGX_MISRHandler, psDeviceNode);
	if (eError != PVRSRV_OK)
	{
		if (psDevInfo->pvAPMISRData != NULL)
		{
			(void) OSUninstallMISR(psDevInfo->pvAPMISRData);
		}
		(void) OSUninstallMISR(psDevInfo->hProcessQueuesMISR);
		return eError;
	}
PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXInitDevPart2KM: Attempt to SysInstallDeviceLISR IRQ:(%d)",psDevConfig->ui32IRQ));	
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
PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXInitDevPart2KM: SysInstallDeviceLISR (%d)",eError));    

#endif

#if defined(SUPPORT_PDVFS) && !defined(RGXFW_META_SUPPORT_2ND_THREAD)
	psDeviceNode->psDevConfig->sDVFS.sPDVFSData.hReactiveTimer =
		OSAddTimer((PFN_TIMER_FUNC)PDVFSRequestReactiveUpdate,
		           psDevInfo,
		           PDVFS_REACTIVE_INTERVAL_MS);

	OSEnableTimer(psDeviceNode->psDevConfig->sDVFS.sPDVFSData.hReactiveTimer);
#endif

#if defined(PDUMP) && !defined(RGX_FEATURE_S7_CACHE_HIERARCHY)
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
#endif

	psDevInfo->bDevInit2Done = IMG_TRUE;

	return PVRSRV_OK;
}
 
IMG_EXPORT
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
											IMG_TRUE);

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

	eError = DevmemFindHeapByName(psDevInfo->psKernelDevmemCtx,
								  "Firmware", /* FIXME: We need to create an IDENT macro for this string.
								                 Make sure the IDENT macro is not accessible to userland */
								  &psDevInfo->psFirmwareHeap);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXInitCreateFWKernelMemoryContext: Failed DevmemFindHeapByName (%u)", eError));
		goto failed_to_find_heap;
	}

#if defined(SUPPORT_PVRSRV_GPUVIRT)
	eError = RGXVzInitCreateFWKernelMemoryContext(psDeviceNode);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "RGXInitCreateFWKernelMemoryContext: Failed RGXVzInitCreateFWKernelMemoryContext (%u)",
				 eError));
		goto failed_to_find_heap;
	}
#endif

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

#if defined(SUPPORT_PVRSRV_GPUVIRT)
	RGXVzDeInitDestroyFWKernelMemoryContext(psDeviceNode);
#endif

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

#if defined(SUPPORT_KERNEL_SRVINIT) && defined(RGXFW_ALIGNCHECKS)
static PVRSRV_ERROR RGXAlignmentCheck(PVRSRV_DEVICE_NODE *psDevNode,
                                      IMG_UINT32 ui32AlignChecksSize,
                                      IMG_UINT32 aui32AlignChecks[])
{
	static IMG_UINT32 aui32AlignChecksKM[] = {RGXFW_ALIGN_CHECKS_INIT_KM};
	PVRSRV_RGXDEV_INFO *psDevInfo = psDevNode->pvDevice;
	IMG_UINT32 i, *paui32FWAlignChecks;
	PVRSRV_ERROR eError = PVRSRV_OK;

	eError = DevmemAcquireCpuVirtAddr(psDevInfo->psRGXFWAlignChecksMemDesc,
	                                  (void **) &paui32FWAlignChecks);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVAlignmentCheckKM: Failed to acquire"
		        " kernel address for alignment checks (%u)", eError));
		return eError;
	}

	paui32FWAlignChecks += IMG_ARR_NUM_ELEMS(aui32AlignChecksKM) + 1;
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
#endif /* defined(SUPPORT_KERNEL_SRVINIT) */

#if defined(PVRSRV_GPUVIRT_GUESTDRV)
static PVRSRV_ERROR RGXDevInitCompatCheck(PVRSRV_DEVICE_NODE *psDeviceNode, IMG_UINT32 ui32ClientBuildOptions)
{
	/* FW compatibility checks are ignored in guest drivers */
	PVR_UNREFERENCED_PARAMETER(psDeviceNode);
	PVR_UNREFERENCED_PARAMETER(ui32ClientBuildOptions);
	return PVRSRV_OK;
}

static PVRSRV_ERROR RGXSoftReset(PVRSRV_DEVICE_NODE *psDeviceNode,
                                 IMG_UINT64  ui64ResetValue1,
                                 IMG_UINT64  ui64ResetValue2)
{
	PVR_UNREFERENCED_PARAMETER(psDeviceNode);
	PVR_UNREFERENCED_PARAMETER(ui64ResetValue1);
	PVR_UNREFERENCED_PARAMETER(ui64ResetValue2);
	return PVRSRV_OK;
}

static void RGXDebugRequestNotify(PVRSRV_DBGREQ_HANDLE hDbgReqestHandle,
					IMG_UINT32 ui32VerbLevel,
					DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
					void *pvDumpDebugFile)
{
	PVR_UNREFERENCED_PARAMETER(hDbgReqestHandle);
	PVR_UNREFERENCED_PARAMETER(ui32VerbLevel);
	PVR_UNREFERENCED_PARAMETER(pfnDumpDebugPrintf);
	PVR_UNREFERENCED_PARAMETER(pvDumpDebugFile);
}

IMG_EXPORT
PVRSRV_ERROR PVRSRVRGXInitAllocFWImgMemKM(CONNECTION_DATA      *psConnection,
                                          PVRSRV_DEVICE_NODE   *psDeviceNode,
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
	DEVMEM_FLAGS_T uiMemAllocFlags;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_ERROR eError;

	/* Guest driver do not perform actual on-chip FW loading/initialization */
	PVR_UNREFERENCED_PARAMETER(psConnection);
	PVR_UNREFERENCED_PARAMETER(psDevInfo);
	PVR_UNREFERENCED_PARAMETER(uiMemAllocFlags);

	eError = RGXInitCreateFWKernelMemoryContext(psDeviceNode);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXInitAllocFWImgMemKM: Failed RGXInitCreateFWKernelMemoryContext (%u)", eError));
		goto failFWMemoryContextAlloc;
	}

failFWMemoryContextAlloc:
	return eError;
}
#else
static
PVRSRV_ERROR RGXAllocateFWCodeRegion(PVRSRV_DEVICE_NODE *psDeviceNode,
                                     IMG_DEVMEM_SIZE_T ui32FWCodeAllocSize,
                                     IMG_UINT32 uiMemAllocFlags)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_ERROR eError;
#if defined(RGX_FEATURE_MIPS) && defined(SUPPORT_TRUSTED_DEVICE)
	IMG_DEVMEM_LOG2ALIGN_T uiLog2Align = RGXMIPSFW_LOG2_PAGE_SIZE_64K;
#else
	IMG_DEVMEM_LOG2ALIGN_T uiLog2Align = OSGetPageShift();
#endif

#if !defined(SUPPORT_TRUSTED_DEVICE)
	uiMemAllocFlags |= PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE |
	                   PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC;

	PDUMPCOMMENT("Allocate and export code memory for fw");

	eError = DevmemFwAllocateExportable(psDeviceNode,
	                                    ui32FWCodeAllocSize,
	                                    1 << uiLog2Align,
	                                    uiMemAllocFlags,
	                                    "FwExCodeRegion",
	                                    &psDevInfo->psRGXFWCodeMemDesc);
	return eError;
#else
	PDUMPCOMMENT("Import secure code memory for fw");

	eError = DevmemImportTDFWCode(psDeviceNode,
	                              ui32FWCodeAllocSize,
	                              uiLog2Align,
	                              uiMemAllocFlags,
	                              &psDevInfo->psRGXFWCodeMemDesc);
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

 @Function	RGXDevInitCompatCheck_BuildOptions_FWAgainstClient

 @Description

 Validate the FW build options against client build options (KM and non-KM)

 @Input psDevInfo - device info
 @Input psRGXFWInit - FW init data
 @Input ui32ClientBuildOptions - client build options flags

 @Return   PVRSRV_ERROR - depending on mismatch found

******************************************************************************/
static PVRSRV_ERROR RGXDevInitCompatCheck_BuildOptions_FWAgainstClient(PVRSRV_RGXDEV_INFO 	*psDevInfo,
																			RGXFWIF_INIT *psRGXFWInit,
																			IMG_UINT32 ui32ClientBuildOptions)
{
#if !defined(NO_HARDWARE)
	IMG_UINT32			ui32BuildOptionsMismatch;
	IMG_UINT32			ui32BuildOptionsFW;
#endif
#if defined(PDUMP)
	PVRSRV_ERROR		eError;
#endif

#if defined(PDUMP)
	PDUMPCOMMENT("Compatibility check: client and FW build options");
	eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfInitMemDesc,
												offsetof(RGXFWIF_INIT, sRGXCompChecks) + 
												offsetof(RGXFWIF_COMPCHECKS, ui32BuildOptions),
												ui32ClientBuildOptions,
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
	
	ui32BuildOptionsFW = psRGXFWInit->sRGXCompChecks.ui32BuildOptions;
	
	if (ui32ClientBuildOptions != ui32BuildOptionsFW)
	{
		ui32BuildOptionsMismatch = ui32ClientBuildOptions ^ ui32BuildOptionsFW;
		if ( (ui32ClientBuildOptions & ui32BuildOptionsMismatch) != 0)
		{
			PVR_LOG(("(FAIL) RGXDevInitCompatCheck: Mismatch in Firmware and client build options; "
				"extra options present in client: (0x%x). Please check rgx_options.h",
				ui32ClientBuildOptions & ui32BuildOptionsMismatch ));
		}

		if ( (ui32BuildOptionsFW & ui32BuildOptionsMismatch) != 0)
		{
			PVR_LOG(("(FAIL) RGXDevInitCompatCheck: Mismatch in Firmware and client build options; "
				"extra options present in Firmware: (0x%x). Please check rgx_options.h",
				ui32BuildOptionsFW & ui32BuildOptionsMismatch ));
		}
		return PVRSRV_ERROR_BUILD_OPTIONS_MISMATCH;
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE, "RGXDevInitCompatCheck: Firmware and client build options match. [ OK ]"));
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
		PVR_LOG(("(FAIL) RGXDevInitCompatCheck: Incompatible driver DDK version (%u.%u) / Firmware DDK revision (%u.%u).",
				PVRVERSION_MAJ, PVRVERSION_MIN, 
				PVRVERSION_UNPACK_MAJ(psRGXFWInit->sRGXCompChecks.ui32DDKVersion),
				PVRVERSION_UNPACK_MIN(psRGXFWInit->sRGXCompChecks.ui32DDKVersion)));
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
		PVR_LOG(("(WARN) RGXDevInitCompatCheck: Incompatible driver DDK build version (%d) / Firmware DDK build version (%d).",
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
	RGXFWIF_COMPCHECKS_BVNC_DECLARE_AND_INIT(sBVNC);
	PVRSRV_ERROR				eError;
	
	rgx_bvnc_packed(&sBVNC.ui64BNC, sBVNC.aszV, sBVNC.ui32VLenMax, RGX_BVNC_KM_B, RGX_BVNC_KM_V_ST, RGX_BVNC_KM_N, RGX_BVNC_KM_C);
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
			PVR_LOG(("(FAIL) %s: Incompatible compatibility struct version of driver (%d) and firmware (%d).",
					__FUNCTION__, 
					sBVNC.ui32LayoutVersion, 
					psRGXFWInit->sRGXCompChecks.sFWBVNC.ui32LayoutVersion));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			return eError;
		}

		if (!bCompatibleLenMax)
		{
			PVR_LOG(("(FAIL) %s: Incompatible V maxlen of driver (%d) and firmware (%d).",
					__FUNCTION__, 
					sBVNC.ui32VLenMax, 
					psRGXFWInit->sRGXCompChecks.sFWBVNC.ui32VLenMax));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			return eError;
		}

		if (!bCompatibleBNC)
		{
			PVR_LOG(("(FAIL) RGXDevInitCompatCheck: Mismatch in KM driver BNC (%d._.%d.%d) and Firmware BNC (%d._.%d.%d)",
					RGX_BVNC_PACKED_EXTR_B(sBVNC), 
					RGX_BVNC_PACKED_EXTR_N(sBVNC), 
					RGX_BVNC_PACKED_EXTR_C(sBVNC), 
					RGX_BVNC_PACKED_EXTR_B(psRGXFWInit->sRGXCompChecks.sFWBVNC), 
					RGX_BVNC_PACKED_EXTR_N(psRGXFWInit->sRGXCompChecks.sFWBVNC), 
					RGX_BVNC_PACKED_EXTR_C(psRGXFWInit->sRGXCompChecks.sFWBVNC)));
			eError = PVRSRV_ERROR_BVNC_MISMATCH;
			return eError;
		}

		bCompatibleV = true;
		
		if (!bCompatibleV)
		{
			PVR_LOG(("(FAIL) RGXDevInitCompatCheck: Mismatch in KM driver BVNC (%d.%s.%d.%d) and Firmware BVNC (%d.%s.%d.%d)",
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

#if defined(FIX_HW_BRN_38835)
#define COMPAT_BVNC_MASK_B
#define COMPAT_BVNC_MASK_V
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

#if defined(COMPAT_BVNC_MASK_B)
	ui64MaskBNC &= ~RGX_BVNC_PACK_MASK_B;
#endif
#if defined(COMPAT_BVNC_MASK_V)
	bMaskV = IMG_TRUE;
#endif
#if defined(COMPAT_BVNC_MASK_N)
	ui64MaskBNC &= ~RGX_BVNC_PACK_MASK_N;
#endif
#if defined(COMPAT_BVNC_MASK_C)
	ui64MaskBNC &= ~RGX_BVNC_PACK_MASK_C;
#endif
	
	rgx_bvnc_packed(&sSWBVNC.ui64BNC, sSWBVNC.aszV, sSWBVNC.ui32VLenMax, RGX_BVNC_KM_B, RGX_BVNC_KM_V_ST, RGX_BVNC_KM_N, RGX_BVNC_KM_C);

#if defined(FIX_HW_BRN_38344)
	if (RGX_BVNC_KM_C >= 10)
	{
		ui64MaskBNC &= ~RGX_BVNC_PACK_MASK_C;
	}
#endif

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

#if defined(FIX_HW_BRN_42480)
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
#endif

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
		bCompatibleV = true;
		
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

#if defined(PDUMP)
	PDUMPIF("DISABLE_HWMETA_CHECK");
	PDUMPELSE("DISABLE_HWMETA_CHECK");
	PDUMPCOMMENT("Compatibility check: KM driver and HW FW Processor version");
	eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfInitMemDesc,
					offsetof(RGXFWIF_INIT, sRGXCompChecks) +
					offsetof(RGXFWIF_COMPCHECKS, ui32FWProcessorVersion),
					FW_CORE_ID_VALUE,
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

	if (psRGXFWInit->sRGXCompChecks.ui32FWProcessorVersion != FW_CORE_ID_VALUE)
	{
		PVR_LOG(("RGXDevInitCompatCheck: Incompatible driver %s version (%d) / HW %s version (%d).",
				 RGXFW_PROCESSOR,
				 FW_CORE_ID_VALUE,
				 RGXFW_PROCESSOR,
				 psRGXFWInit->sRGXCompChecks.ui32FWProcessorVersion));
		eError = PVRSRV_ERROR_FWPROCESSOR_MISMATCH;
		PVR_DBG_BREAK;
		return eError;
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE, "RGXDevInitCompatCheck: Compatible driver %s version (%d) / HW %s version (%d) [OK].",
				 RGXFW_PROCESSOR,
				 FW_CORE_ID_VALUE,
				 RGXFW_PROCESSOR,
				 psRGXFWInit->sRGXCompChecks.ui32FWProcessorVersion));
	}
#endif
	return PVRSRV_OK;
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
static PVRSRV_ERROR RGXDevInitCompatCheck(PVRSRV_DEVICE_NODE *psDeviceNode, IMG_UINT32 ui32ClientBuildOptions)
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
		if(*((volatile IMG_BOOL *)&psRGXFWInit->sRGXCompChecks.bUpdated))
		{
			/* No need to wait if the FW has already updated the values */
			break;
		}
		OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
	} END_LOOP_UNTIL_TIMEOUT();

	ui32RegValue = 0;

#if defined(RGX_FEATURE_META)
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
#endif
	
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

	eError = RGXDevInitCompatCheck_BuildOptions_FWAgainstClient(psDevInfo, psRGXFWInit, ui32ClientBuildOptions);
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
	eError = RGXDevInitCompatCheck_FWProcessorVersion_AgainstDriver(psDevInfo, psRGXFWInit);
	if (eError != PVRSRV_OK)
	{
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

	PVR_ASSERT(psDeviceNode != NULL);
	PVR_ASSERT(psDeviceNode->pvDevice != NULL);

	if ((ui64ResetValue1 & RGX_CR_SOFT_RESET_MASKFULL) != ui64ResetValue1
#if defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
		|| (ui64ResetValue2 & RGX_CR_SOFT_RESET2_MASKFULL) != ui64ResetValue2
#endif
		)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/* the device info */
	psDevInfo = psDeviceNode->pvDevice;

	/* Set in soft-reset */
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, ui64ResetValue1);
#if defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET2, ui64ResetValue2);
#endif

	/* Read soft-reset to fence previous write in order to clear the SOCIF pipeline */
	(void) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET);
#if defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
	(void) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET2);
#endif

	/* Take the modules out of reset... */
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, 0);
#if defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET2, 0);
#endif

	/* ...and fence again */
	(void) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET);
#if defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
	(void) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET2);
#endif
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

IMG_EXPORT
PVRSRV_ERROR PVRSRVRGXInitAllocFWImgMemKM(CONNECTION_DATA      *psConnection,
                                          PVRSRV_DEVICE_NODE   *psDeviceNode,
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

	PVR_UNREFERENCED_PARAMETER(psConnection);

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
	                                 uiMemAllocFlags);

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

#if defined(RGX_FEATURE_META)
	/*
	* The FW code must be the first allocation in the firmware heap, otherwise
	* the bootloader will not work (META will not be able to find the bootloader).
	*/
	PVR_ASSERT(psFWCodeDevVAddrBase->uiAddr == RGX_FIRMWARE_HEAP_BASE);
#endif

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

		PDUMPCOMMENT("Allocate and export coremem memory for fw");

		eError = DevmemFwAllocateExportable(psDeviceNode,
				uiFWCorememLen,
				OSGetPageSize(),
				uiMemAllocFlags,
				"FwExCorememRegion",
				&psDevInfo->psRGXFWCorememMemDesc);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"Failed to allocate fw coremem mem, size: %lld, flags: %x (%u)",
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
		psDevInfo->sFWCorememCodeDevVAddrBase.uiAddr = 0;
		psDevInfo->sFWCorememCodeFWAddr.ui32Addr = 0;
	}

	*psFWCorememDevVAddrBase = psDevInfo->sFWCorememCodeDevVAddrBase;
	*psFWCorememMetaVAddrBase = psDevInfo->sFWCorememCodeFWAddr;

	return PVRSRV_OK;

failFWCorememMemDescAqDevVirt:
	if (uiFWCorememLen != 0)
	{
		DevmemFwFree(psDevInfo->psRGXFWCorememMemDesc);
		psDevInfo->psRGXFWCorememMemDesc = NULL;
	}
failFWCorememMemDescAlloc:
	DevmemReleaseDevVirtAddr(psDevInfo->psRGXFWDataMemDesc);
failFWDataMemDescAqDevVirt:
	DevmemFwFree(psDevInfo->psRGXFWDataMemDesc);
	psDevInfo->psRGXFWDataMemDesc = NULL;
failFWDataMemDescAlloc:
	DevmemReleaseDevVirtAddr(psDevInfo->psRGXFWCodeMemDesc);
failFWCodeMemDescAqDevVirt:
	DevmemFwFree(psDevInfo->psRGXFWCodeMemDesc);
	psDevInfo->psRGXFWCodeMemDesc = NULL;
failFWCodeMemDescAlloc:
failFWMemoryContextAlloc:
	return eError;
}
#endif /* defined(PVRSRV_GPUVIRT_GUESTDRV) */

/*
 * PVRSRVRGXInitFirmwareKM
 */ 
IMG_EXPORT PVRSRV_ERROR
PVRSRVRGXInitFirmwareKM(CONNECTION_DATA          *psConnection,
                        PVRSRV_DEVICE_NODE       *psDeviceNode,
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
                        IMG_UINT32               ui32HWPerfCountersDataSize,
                        PMR                      **ppsHWPerfPMR,
                        RGX_RD_POWER_ISLAND_CONF eRGXRDPowerIslandingConf,
                        FW_PERF_CONF             eFirmwarePerf)
{
	PVRSRV_ERROR eError;
	RGXFWIF_COMPCHECKS_BVNC_DECLARE_AND_INIT(sBVNC);
	IMG_BOOL bCompatibleAll, bCompatibleVersion, bCompatibleLenMax, bCompatibleBNC, bCompatibleV;
	RGXFWIF_BIFTILINGMODE eBIFTilingMode;
	IMG_UINT32 ui32NumBIFTilingConfigs, *pui32BIFTilingXStrides, i;


	/* Check if BVNC numbers of client and driver are compatible */
	rgx_bvnc_packed(&sBVNC.ui64BNC, sBVNC.aszV, sBVNC.ui32VLenMax, RGX_BVNC_KM_B, RGX_BVNC_KM_V_ST, RGX_BVNC_KM_N, RGX_BVNC_KM_C);

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

		bCompatibleV = true;
		
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
	if(pui32BIFTilingXStrides == NULL)
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
		if(eError != PVRSRV_OK)
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
	                          eFirmwarePerf);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXInitFirmwareKM: RGXSetupFirmware failed (%u)", eError));
		goto failed_init_firmware;
	}
	
	OSFreeMem(pui32BIFTilingXStrides);

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
	DevmemExportalignAdjustSizeAndAlign(psDevInfo->psFirmwareHeap,
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
											IMG_TRUE);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"RGXFreeUFOBlock: Failed to schedule SLC flush command with error (%u)", eError));
		}
		else
		{
			/* Wait for the SLC flush to complete */
			eError = RGXWaitForFWOp(psDeviceNode->pvDevice, RGXFWIF_DM_GP, psDeviceNode->psSyncPrim, IMG_TRUE);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"RGXFreeUFOBlock: SLC flush and invalidate aborted with error (%u)", eError));
			}
		}
	}

	RGXUnsetFirmwareAddress(psMemDesc);
	DevmemFwFree(psMemDesc);
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
		if(0 != ui32Temp)
		{
			PVR_DPF((PVR_DBG_ERROR,"%s: Dummy page reference counter is non zero", __func__));
			PVR_ASSERT(0);
		}
	}
#endif
#if defined(PDUMP)
		if(NULL != psDeviceNode->sDummyPage.hPdumpDummyPg)
		{
			PDUMPCOMMENT("Error dummy page handle is still active");
		}
#endif

#if defined(SUPPORT_PDVFS) && !defined(RGXFW_META_SUPPORT_2ND_THREAD)
	OSDisableTimer(psDeviceNode->psDevConfig->sDVFS.sPDVFSData.hReactiveTimer);
	OSRemoveTimer(psDeviceNode->psDevConfig->sDVFS.sPDVFSData.hReactiveTimer);
#endif

	/*The lock type need to be dispatch type here because it can be acquired from MISR (Z-buffer) path */
	OSLockDestroy(psDeviceNode->sDummyPage.psDummyPgLock);

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

		OSLockDestroy(psDevInfo->hGPUUtilLock);

#if !defined(PVRSRV_GPUVIRT_GUESTDRV)
		/* Free DVFS Table */
		if (psDevInfo->psGpuDVFSTable != NULL)
		{
			OSFreeMem(psDevInfo->psGpuDVFSTable);
			psDevInfo->psGpuDVFSTable = NULL;
		}
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

#if defined(RGX_FEATURE_MIPS)
		/* Unregister MMU related stuff */
		eError = RGXMipsMMUInit_Unregister(psDeviceNode);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"DevDeInitRGX: Failed RGXMipsMMUInit_Unregister (0x%x)", eError));
			return eError;
		}
#endif


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

#if !defined(PVRSRV_GPUVIRT_GUESTDRV)
	if (psDevInfo->psRGXFWCodeMemDesc)
	{
		/* Free fw code */
		PDUMPCOMMENT("Freeing FW code memory");
		DevmemReleaseDevVirtAddr(psDevInfo->psRGXFWCodeMemDesc);
		DevmemFwFree(psDevInfo->psRGXFWCodeMemDesc);
		psDevInfo->psRGXFWCodeMemDesc = NULL;
	}
	if (psDevInfo->psRGXFWDataMemDesc)
	{
		/* Free fw data */
		PDUMPCOMMENT("Freeing FW data memory");
		DevmemReleaseDevVirtAddr(psDevInfo->psRGXFWDataMemDesc);
		DevmemFwFree(psDevInfo->psRGXFWDataMemDesc);
		psDevInfo->psRGXFWDataMemDesc = NULL;
	}
	if (psDevInfo->psRGXFWCorememMemDesc)
	{
		/* Free fw data */
		PDUMPCOMMENT("Freeing FW coremem memory");
		DevmemReleaseDevVirtAddr(psDevInfo->psRGXFWCorememMemDesc);
		DevmemFwFree(psDevInfo->psRGXFWCorememMemDesc);
		psDevInfo->psRGXFWCorememMemDesc = NULL;
	}
#endif

	/*
	   Free the firmware allocations.
	 */
	RGXFreeFirmware(psDevInfo);
	RGXDeInitDestroyFWKernelMemoryContext(psDeviceNode);

	/* De-initialise non-device specific (TL) users of RGX device memory */
	RGXHWPerfHostDeInit();
	eError = HTBDeInit();
	PVR_LOG_IF_ERROR(eError, "HTBDeInit");

	/* destroy the context list locks */
	OSWRLockDestroy(psDevInfo->hRenderCtxListLock);
	OSWRLockDestroy(psDevInfo->hComputeCtxListLock);
	OSWRLockDestroy(psDevInfo->hTransferCtxListLock);
	OSWRLockDestroy(psDevInfo->hRaytraceCtxListLock);
	OSWRLockDestroy(psDevInfo->hKickSyncCtxListLock);
	OSWRLockDestroy(psDevInfo->hMemoryCtxListLock);

#if defined(RGX_FEATURE_MIPS)
	if (psDevInfo->hNMILock != NULL)
	{
		OSLockDestroy(psDevInfo->hNMILock);
	}
#endif

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

#if !defined(PVRSRV_GPUVIRT_GUESTDRV)
	/* Free the init scripts. */
	OSFreeMem(psDevInfo->psScripts);
#endif

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
	IMG_UINT32 log2_import_alignment)
{
	DEVMEM_HEAP_BLUEPRINT b = {
		.pszName = name,
		.sHeapBaseAddr.uiAddr = heap_base,
		.uiHeapLength = heap_length,
		.uiLog2DataPageSize = GET_LOG2_PAGESIZE(),
		.uiLog2ImportAlignment = log2_import_alignment
	};

	return b;
}

#define INIT_HEAP(NAME) \
do { \
	*psDeviceMemoryHeapCursor = _blueprint_init( \
			RGX_ ## NAME ## _HEAP_IDENT, \
			RGX_ ## NAME ## _HEAP_BASE, \
			RGX_ ## NAME ## _HEAP_SIZE, \
			0); \
	psDeviceMemoryHeapCursor++; \
} while (0)

#define INIT_HEAP_NAME(STR, NAME) \
do { \
	*psDeviceMemoryHeapCursor = _blueprint_init( \
			STR, \
			RGX_ ## NAME ## _HEAP_BASE, \
			RGX_ ## NAME ## _HEAP_SIZE, \
			0); \
	psDeviceMemoryHeapCursor++; \
} while (0)

#define INIT_TILING_HEAP(D, N)		\
do { \
	IMG_UINT32 xstride; \
	PVRSRVSystemBIFTilingHeapGetXStride((D)->psDeviceNode->psDevConfig, N, &xstride); \
	*psDeviceMemoryHeapCursor = _blueprint_init( \
			RGX_BIF_TILING_HEAP_ ## N ## _IDENT, \
			RGX_BIF_TILING_HEAP_ ## N ## _BASE, \
			RGX_BIF_TILING_HEAP_SIZE, \
			RGX_BIF_TILING_HEAP_ALIGN_LOG2_FROM_XSTRIDE(xstride)); \
	psDeviceMemoryHeapCursor++; \
} while (0)

static PVRSRV_ERROR RGXInitHeaps(PVRSRV_RGXDEV_INFO *psDevInfo,
								 DEVICE_MEMORY_INFO *psNewMemoryInfo,
								 IMG_UINT32 *pui32DummyPgSize)
{
    DEVMEM_HEAP_BLUEPRINT *psDeviceMemoryHeapCursor;

    /* FIXME - consider whether this ought not to be on the device node itself */
	psNewMemoryInfo->psDeviceMemoryHeap = OSAllocMem(sizeof(DEVMEM_HEAP_BLUEPRINT) * RGX_MAX_HEAP_ID);
    if(psNewMemoryInfo->psDeviceMemoryHeap == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXRegisterDevice : Failed to alloc memory for DEVMEM_HEAP_BLUEPRINT"));
		goto e0;
	}

	/* Calculate the dummy page size which is the maximum page size supported
	 * by heaps which can have sparse allocations
	 *
	 * The heaps that can have sparse allocations are general and Doppler for now.
	 * As it was suggested the doppler allocations doesn't have to be backed by dummy
	 * and taking into account its massive 2MB page size supported in future, we take
	 * general heap page size as reference for now */
	*pui32DummyPgSize =  GET_LOG2_PAGESIZE();

	/* Initialise the heaps */
	psDeviceMemoryHeapCursor = psNewMemoryInfo->psDeviceMemoryHeap;

	INIT_HEAP(GENERAL_SVM);
	INIT_HEAP(GENERAL);
	INIT_HEAP(VISTEST);
	INIT_HEAP(PDSCODEDATA);
	INIT_HEAP(USCCODE);
	INIT_HEAP(TQ3DPARAMETERS);
	INIT_TILING_HEAP(psDevInfo, 1);
	INIT_TILING_HEAP(psDevInfo, 2);
	INIT_TILING_HEAP(psDevInfo, 3);
	INIT_TILING_HEAP(psDevInfo, 4);
	INIT_HEAP(DOPPLER);
	INIT_HEAP(DOPPLER_OVERFLOW);
#if defined (RGX_FEATURE_SIGNAL_SNOOPING)
	INIT_HEAP(SERVICES_SIGNALS);
	INIT_HEAP(SIGNALS);
#endif
#if defined(FIX_HW_BRN_37200)
	INIT_HEAP_NAME("HWBRN37200", HWBRN37200);
#endif
	INIT_HEAP(TDM_TPU_YUV_COEFFS);
	INIT_HEAP_NAME("Firmware", FIRMWARE);

	/* set the heap count */
	psNewMemoryInfo->ui32HeapCount = (IMG_UINT32)(psDeviceMemoryHeapCursor - psNewMemoryInfo->psDeviceMemoryHeap);

	PVR_ASSERT(psNewMemoryInfo->ui32HeapCount <= RGX_MAX_HEAP_ID);

    /* the new way: we'll set up 2 heap configs: one will be for Meta
       only, and has only the firmware heap in it. 
       The remaining one shall be for clients only, and shall have all
       the other heaps in it */

    psNewMemoryInfo->uiNumHeapConfigs = 2;
	psNewMemoryInfo->psDeviceMemoryHeapConfigArray = OSAllocMem(sizeof(DEVMEM_HEAP_CONFIG) * psNewMemoryInfo->uiNumHeapConfigs);
    if (psNewMemoryInfo->psDeviceMemoryHeapConfigArray == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXRegisterDevice : Failed to alloc memory for DEVMEM_HEAP_CONFIG"));
		goto e1;
	}
    
    psNewMemoryInfo->psDeviceMemoryHeapConfigArray[0].pszName = "Default Heap Configuration";
    psNewMemoryInfo->psDeviceMemoryHeapConfigArray[0].uiNumHeaps = psNewMemoryInfo->ui32HeapCount-1;
    psNewMemoryInfo->psDeviceMemoryHeapConfigArray[0].psHeapBlueprintArray = psNewMemoryInfo->psDeviceMemoryHeap;

    psNewMemoryInfo->psDeviceMemoryHeapConfigArray[1].pszName = "Firmware Heap Configuration";
#if defined(FIX_HW_BRN_37200)
    psNewMemoryInfo->psDeviceMemoryHeapConfigArray[1].uiNumHeaps = 2;
    psNewMemoryInfo->psDeviceMemoryHeapConfigArray[1].psHeapBlueprintArray = psDeviceMemoryHeapCursor-2;
#else
    psNewMemoryInfo->psDeviceMemoryHeapConfigArray[1].uiNumHeaps = 1;
    psNewMemoryInfo->psDeviceMemoryHeapConfigArray[1].psHeapBlueprintArray = psDeviceMemoryHeapCursor-1;
#endif

#if defined(SUPPORT_PVRSRV_GPUVIRT)
	if (RGXVzInitHeaps(psNewMemoryInfo, psDeviceMemoryHeapCursor) != PVRSRV_OK)
	{
		goto e1;
	}
#endif

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
#if defined(SUPPORT_PVRSRV_GPUVIRT)
	RGXVzDeInitHeaps(psDevMemoryInfo);
#endif
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

	/* pdump info about the core */
	PDUMPCOMMENT("RGX Version Information (KM): %s", RGX_BVNC_KM);
	
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

#if defined(RGX_FEATURE_MIPS)
	RGXMipsMMUInit_Register(psDeviceNode);
#endif

	psDeviceNode->pfnMMUCacheInvalidate = RGXMMUCacheInvalidate;

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

#if defined(SUPPORT_KERNEL_SRVINIT) && defined(RGXFW_ALIGNCHECKS)
	/* Register callback for checking alignment of UM structures */
	psDeviceNode->pfnAlignmentCheck = RGXAlignmentCheck;
#endif

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
	if(PVRSRV_OK != eError)
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

	eError = OSWRLockCreate(&(psDevInfo->hRaytraceCtxListLock));
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create raytrace context list lock", __func__));
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

	dllist_init(&(psDevInfo->sKCCBDeferredCommandsListHead));

	dllist_init(&(psDevInfo->sRenderCtxtListHead));
	dllist_init(&(psDevInfo->sComputeCtxtListHead));
	dllist_init(&(psDevInfo->sTransferCtxtListHead));
	dllist_init(&(psDevInfo->sRaytraceCtxtListHead));
	dllist_init(&(psDevInfo->sKickSyncCtxtListHead));

	dllist_init(&(psDevInfo->sCommonCtxtListHead));
	psDevInfo->ui32CommonCtxtCurrentID = 1;

	dllist_init(&psDevInfo->sMemoryContextList);

#if !defined(PVRSRV_GPUVIRT_GUESTDRV)
	/* Allocate space for scripts. */
	psDevInfo->psScripts = OSAllocMem(sizeof(*psDevInfo->psScripts));
	if (!psDevInfo->psScripts)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to allocate memory for scripts", __func__));
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto e6;
	}
#endif

	/* Setup static data and callbacks on the device specific device info */
	psDevInfo->psDeviceNode		= psDeviceNode;

	psDevMemoryInfo = &psDeviceNode->sDevMemoryInfo;
	psDevInfo->pvDeviceMemoryHeap = psDevMemoryInfo->psDeviceMemoryHeap;

	eError = RGXInitHeaps(psDevInfo, psDevMemoryInfo,
						  &psDeviceNode->sDummyPage.ui32Log2DummyPgSize);
	if (eError != PVRSRV_OK)
	{
		goto e7;
	}

	psDeviceNode->pvDevice = psDevInfo;

	eError = RGXHWPerfInit(psDeviceNode);
	PVR_LOGG_IF_ERROR(eError, "RGXHWPerfInit", e8);

	/* Register callback for dumping debug info */
	eError = PVRSRVRegisterDbgRequestNotify(&psDevInfo->hDbgReqNotify,
											psDeviceNode,
											RGXDebugRequestNotify,
											DEBUG_REQUEST_SYS,
											psDevInfo);
	PVR_LOG_IF_ERROR(eError, "PVRSRVRegisterDbgRequestNotify");

	return PVRSRV_OK;

e8:
	psDeviceNode->pvDevice = NULL;
	RGXDeInitHeaps(psDevMemoryInfo);
e7:
#if !defined(PVRSRV_GPUVIRT_GUESTDRV)
	OSFreeMem(psDevInfo->psScripts);
e6:
#endif
	OSWRLockDestroy(psDevInfo->hMemoryCtxListLock);
e5:
	OSWRLockDestroy(psDevInfo->hKickSyncCtxListLock);
e4:
	OSWRLockDestroy(psDevInfo->hRaytraceCtxListLock);
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

IMG_EXPORT
PVRSRV_ERROR PVRSRVRGXInitGuestKM(CONNECTION_DATA		*psConnection,
								PVRSRV_DEVICE_NODE		*psDeviceNode,
								IMG_BOOL				bEnableSignatureChecks,
								IMG_UINT32				ui32SignatureChecksBufSize,
								IMG_UINT32				ui32DeviceFlags,
								RGXFWIF_COMPCHECKS_BVNC	*psClientBVNC)
{
	PVRSRV_ERROR	eError;

#if defined(PVRSRV_GPUVIRT_GUESTDRV)
	/*
	 * Guest drivers do not support the following functionality:
	 *  - Perform actual on-chip firmware loading & initialization
	 *  - Perform actual on-chip firmware configuration
	 *  - Perform actual on-chip fw RDPowIsland(ing)
	 *  - Perform actual on-chip fw tracing, HWPerf
	 *  - Configure FW perf counters
	 */
	eError =  PVRSRVRGXInitAllocFWImgMemKM(psConnection,
											psDeviceNode,
											0,		/* uiFWCodeLen */
											0,		/* uiFWDataLen */
											0,		/* uiFWCorememLen */
											NULL,	/* ppsFWCodePMR */
											NULL,	/* psFWCodeDevVAddrBase */
											NULL,	/* ppsFWDataPMR */
											NULL,	/* psFWDataDevVAddrBase */
											NULL,	/* ppsFWCorememPMR */
											NULL,	/* psFWCorememDevVAddrBase */
											NULL	/* psFWCorememMetaVAddrBase */);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXInitGuest: PVRSRVRGXInitAllocFWImgMemKM failed (%u)", eError));
		goto e0;
	}

	eError =  PVRSRVRGXInitFirmwareKM(psConnection,
									  psDeviceNode,
									  NULL,	/* psRGXFwInit */
									  bEnableSignatureChecks,
									  ui32SignatureChecksBufSize,
									  0,	/* ui32HWPerfFWBufSizeKB */
									  0,	/* ui64HWPerfFilter */
									  0,	/* ui32RGXFWAlignChecksSize */
									  0,	/* pui32RGXFWAlignChecks */
									  0,	/* ui32ConfigFlags */
									  0,	/* ui32LogType */
									  0,	/* ui32FilterFlags */
									  0,	/* ui32JonesDisableMask */
									  0,	/* ui32HWRDebugDumpLimit */
									  psClientBVNC,
									  0,	/* ui32HWPerfCountersDataSize */
									  NULL,	/* ppsHWPerfPMR */
									  0,	/* eRGXRDPowerIslandingConf */
									  0		/* eFirmwarePerf */);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXInitGuest: PVRSRVRGXInitFirmwareKM failed (%u)", eError));
		goto e0;
	}

	eError =   PVRSRVRGXInitDevPart2KM(psConnection,
									   psDeviceNode,
									   NULL,	/* psDbgScript */
									   ui32DeviceFlags,
									   0,		/* ui32HWPerfHostBufSizeKB */
									   0,		/* ui32HWPerfHostFilter */
									   0,		/* eActivePMConf */
									   NULL,	/* psFWCodePMR */
									   NULL,	/* psFWDataPMR */
									   NULL,	/* psFWCorePMR */
									   NULL		/* psHWPerfPMR */);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXInitGuest: PVRSRVRGXInitDevPart2KM failed (%u)", eError));
		goto e0;
	}

e0:
#else
	eError = PVRSRV_ERROR_NOT_SUPPORTED;
#endif

	return eError;
}

IMG_EXPORT PVRSRV_ERROR
PVRSRVRGXInitFinaliseFWImageKM(CONNECTION_DATA *psConnection,
                               PVRSRV_DEVICE_NODE *psDeviceNode)
{
#if defined(RGX_FEATURE_MIPS)
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
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

#endif /* defined(RGX_FEATURE_MIPS) */

	PVR_UNREFERENCED_PARAMETER(psConnection);
	PVR_UNREFERENCED_PARAMETER(psDeviceNode);

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
#if defined(COMPAT_BVNC_MASK_B) || defined(COMPAT_BVNC_MASK_V) || defined(COMPAT_BVNC_MASK_N) || defined(COMPAT_BVNC_MASK_C) || defined(NO_HARDWARE) || defined(EMULATOR)
	IMG_CHAR pszFormatString[] = "Rogue Version: %d.%s.%d.%d (SW)";
#else
	IMG_CHAR pszFormatString[] = "Rogue Version: %d.%s.%d.%d (HW)";
#endif
	size_t uiStringLength;

	if (psDeviceNode == NULL || ppszVersionString == NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	uiStringLength = OSStringLength(pszFormatString);
	uiStringLength += OSStringLength(IMG_STRINGIFY(RGX_BVNC_KM_B));
	uiStringLength += OSStringLength(IMG_STRINGIFY(RGX_BVNC_KM_V));
	uiStringLength += OSStringLength(IMG_STRINGIFY(RGX_BVNC_KM_N));
	uiStringLength += OSStringLength(IMG_STRINGIFY(RGX_BVNC_KM_C));

	*ppszVersionString = OSAllocZMem(uiStringLength * sizeof(IMG_CHAR));
	if (*ppszVersionString == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	OSSNPrintf(*ppszVersionString, uiStringLength, pszFormatString, 
		   RGX_BVNC_KM_B, IMG_STRINGIFY(RGX_BVNC_KM_V), RGX_BVNC_KM_N, RGX_BVNC_KM_C);

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

/******************************************************************************
 End of file (rgxinit.c)
******************************************************************************/
