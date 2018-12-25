/*************************************************************************/ /*!
@File           cache_km.c
@Title          CPU d-cache maintenance operations framework
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements server side code for CPU d-cache maintenance taking
                into account the idiosyncrasies of the various types of CPU
                d-cache instruction-set architecture (ISA) maintenance
                mechanisms.
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
#include <asm/uaccess.h>
#include <asm/current.h>
#include <linux/sched.h>
#include <linux/mm.h>
#endif

#include "pmr.h"
#include "log2.h"
#include "device.h"
#include "pvrsrv.h"
#include "osfunc.h"
#include "cache_km.h"
#include "pvr_debug.h"
#include "lock_types.h"
#include "allocmem.h"
#include "process_stats.h"
#if defined(PVR_RI_DEBUG) && defined(DEBUG)
#include "ri_server.h"
#endif
#include "devicemem.h"
#include "pvrsrv_apphint.h"
#include "pvrsrv_sync_server.h"

/* Top-level file-local build definitions */
#if defined(PVRSRV_ENABLE_CACHEOP_STATS) && defined(LINUX)
#define CACHEOP_DEBUG
#define CACHEOP_STATS_ITEMS_MAX 			32
#define INCR_WRAP(x)						((x+1) >= CACHEOP_STATS_ITEMS_MAX ? 0 : (x+1))
#define DECR_WRAP(x)						((x-1) < 0 ? (CACHEOP_STATS_ITEMS_MAX-1) : (x-1))
#if defined(PVR_RI_DEBUG) && defined(DEBUG)
/* Refer to CacheOpStatsExecLogHeader() for header item names */
#define CACHEOP_RI_PRINTF_HEADER			"%-8s %-10s %-10s %-5s %-16s %-16s %-10s %-10s %-18s %-18s %-12s"
#define CACHEOP_RI_PRINTF					"%-8d %-10s %-10s %-5s 0x%-14llx 0x%-14llx 0x%-8llx 0x%-8llx %-18llu %-18llu 0x%-10x\n"
#else
#define CACHEOP_PRINTF_HEADER				"%-8s %-10s %-10s %-5s %-10s %-10s %-18s %-18s %-12s"
#define CACHEOP_PRINTF						"%-8d %-10s %-10s %-5s 0x%-8llx 0x%-8llx %-18llu %-18llu 0x%-10x\n"
#endif
#endif

//#define CACHEOP_NO_CACHE_LINE_ALIGNED_ROUNDING		/* Force OS page (not cache line) flush granularity */
#define CACHEOP_THREAD_WAIT_TIMEOUT			500000ULL	/* Wait 500ms between wait unless woken-up on demand */
#define CACHEOP_FENCE_WAIT_TIMEOUT			1000ULL		/* Wait 1ms between wait events unless woken-up */
#define CACHEOP_FENCE_RETRY_ABORT			1000ULL		/* Fence retries that aborts fence operation */
#define CACHEOP_SEQ_MIDPOINT (IMG_UINT32)	0x7FFFFFFF	/* Where seqNum(s) are rebase, compared at */
#define CACHEOP_ABORT_FENCE_ERROR_STRING	"detected stalled client, retrying cacheop fence"
#define CACHEOP_NO_GFLUSH_ERROR_STRING		"global flush requested on CPU without support"
#define CACHEOP_DEVMEM_OOR_ERROR_STRING		"cacheop device memory request is out of range"
#define CACHEOP_MAX_DEBUG_MESSAGE_LEN		160

typedef struct _CACHEOP_WORK_ITEM_
{
	PMR *psPMR;
	IMG_UINT32 ui32GFSeqNum;
	IMG_UINT32 ui32OpSeqNum;
	IMG_DEVMEM_SIZE_T uiSize;
	PVRSRV_CACHE_OP uiCacheOp;
	IMG_DEVMEM_OFFSET_T uiOffset;
	PVRSRV_TIMELINE iTimeline;
	SYNC_TIMELINE_OBJ sSWTimelineObj;
#if defined(CACHEOP_DEBUG)
	IMG_UINT64 ui64EnqueuedTime;
	IMG_UINT64 ui64DequeuedTime;
	IMG_UINT64 ui64ExecuteTime;
	IMG_BOOL bDeferred;
	IMG_BOOL bKMReq;
	IMG_BOOL bRBF;
	IMG_BOOL bUMF;
	IMG_PID pid;
#if defined(PVR_RI_DEBUG) && defined(DEBUG)
	RGXFWIF_DM eFenceOpType;
#endif
#endif
} CACHEOP_WORK_ITEM;

typedef struct _CACHEOP_STATS_EXEC_ITEM_
{
	IMG_PID pid;
	IMG_UINT32 ui32OpSeqNum;
	PVRSRV_CACHE_OP uiCacheOp;
	IMG_DEVMEM_SIZE_T uiOffset;
	IMG_DEVMEM_SIZE_T uiSize;
	IMG_UINT64 ui64EnqueuedTime;
	IMG_UINT64 ui64DequeuedTime;
	IMG_UINT64 ui64ExecuteTime;
	IMG_BOOL bIsFence;
	IMG_BOOL bKMReq;
	IMG_BOOL bRBF;
	IMG_BOOL bUMF;
	IMG_BOOL bDeferred;
#if defined(PVR_RI_DEBUG) && defined(DEBUG)
	IMG_DEV_VIRTADDR sDevVAddr;
	IMG_DEV_PHYADDR sDevPAddr;
	RGXFWIF_DM eFenceOpType;
#endif
} CACHEOP_STATS_EXEC_ITEM;

typedef enum _CACHEOP_CONFIG_
{
	CACHEOP_CONFIG_DEFAULT = 0,
	/* cache flush mechanism types */
	CACHEOP_CONFIG_KRBF    = 1,
	CACHEOP_CONFIG_KGF     = 2,
	CACHEOP_CONFIG_URBF    = 4,
	/* sw-emulated deferred flush mechanism */
	CACHEOP_CONFIG_KDF     = 8,
	/* pseudo configuration items */
	CACHEOP_CONFIG_LAST    = 16,
	CACHEOP_CONFIG_KLOG    = 16,
	CACHEOP_CONFIG_ALL     = 31
} CACHEOP_CONFIG;

typedef struct _CACHEOP_WORK_QUEUE_
{
/*
 * Init. state & primary device node framework
 * is anchored on.
 */
	IMG_BOOL bInit;
/*
  MMU page size/shift & d-cache line size
 */
	size_t uiPageSize;
	IMG_UINT32 uiLineSize;
	IMG_UINT32 uiLineShift;
	IMG_UINT32 uiPageShift;
	PVRSRV_CACHE_OP_ADDR_TYPE uiCacheOpAddrType;
/*
  CacheOp deferred queueing protocol
  + Implementation geared for performance, atomic counter based
	- Value Space is 0 -> 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 -> 8 -> n.
	- Index Space is 0 -> 1 -> 2 -> 3 -> 0 -> 1 -> 2 -> 3 -> 0 -> m.
		- Index = Value modulo CACHEOP_INDICES_LOG2_SIZE.
  + Write counter never collides with read counter in index space
	- Unless at start of day when both are initialised to zero.
	- This means we sacrifice one entry when the queue is full.
	- Incremented by producer
		- Value space tracks total number of CacheOps queued.
		- Index space identifies CacheOp CCB queue index.
  + Read counter increments towards write counter in value space
	- Empty queue occurs when read equals write counter.
	- Wrap-round logic handled by consumer as/when needed.
	- Incremented by consumer
		- Value space tracks total # of CacheOps executed.
		- Index space identifies CacheOp CCB queue index.
  + Total queued size adjusted up/down during write/read activity
	- Counter might overflow but does not compromise framework.
 */
	ATOMIC_T hReadCounter;
	ATOMIC_T hWriteCounter;
/*
  CacheOp sequence numbers
  + hCommonSeqNum:
	- Common sequence, numbers every CacheOp operation in both UM/KM.
	- In KM
		- Every deferred CacheOp (on behalf of UM) gets a unique seqNum.
		- Last executed deferred CacheOp updates gsCwq.hCompletedSeqNum.
		- Every GF operation (if supported) also gets a unique seqNum.
		- Last executed GF operation updates CACHEOP_INFO_GFSEQNUM0.
		- Under debug, all CacheOp gets a unique seqNum for tracking.
		- This includes all UM/KM synchronous non-deferred CacheOp(s)
	- In UM
		- If the processor architecture supports GF maintenance (in KM)
		- All UM CacheOp samples CACHEOP_INFO_GFSEQNUM0 via info. page.
		- CacheOp(s) discarded if another GF occurs before execution.
		- CacheOp(s) discarding happens in both UM and KM space.
  + hCompletedSeqNum:
	- Tracks last executed KM/deferred RBF/Global<timeline> CacheOp(s)
  + hDeferredSize:
	- Running total of size of currently deferred CacheOp in queue.
 */
	ATOMIC_T hDeferredSize;
	ATOMIC_T hCommonSeqNum;
	ATOMIC_T hCompletedSeqNum;
/*
  CacheOp information page
  + psInfoPageMemDesc:
	- Single system-wide OS page that is multi-mapped in UM/KM.
	- Mapped into clients using read-only memory protection.
	- Mapped into server using read/write memory protection.
	- Contains information pertaining to cache framework.
  + pui32InfoPage:
	- Server linear address pointer to said information page.
	- Each info-page entry currently of sizeof(IMG_UINT32).
 */
	PMR *psInfoPagePMR;
	IMG_UINT32 *pui32InfoPage;
	DEVMEM_MEMDESC *psInfoPageMemDesc;
/*
  CacheOp deferred work-item queue
  + CACHEOP_INDICES_LOG2_SIZE
	- Sized using GF/RBF ratio
 */
#define CACHEOP_INDICES_LOG2_SIZE	(4)
#define CACHEOP_INDICES_MAX			(1 << CACHEOP_INDICES_LOG2_SIZE)
#define CACHEOP_INDICES_MASK		(CACHEOP_INDICES_MAX-1)
	CACHEOP_WORK_ITEM asWorkItems[CACHEOP_INDICES_MAX];
#if defined(CACHEOP_DEBUG)
/*
  CacheOp statistics
 */
	void *pvStatsEntry;
	IMG_HANDLE hStatsExecLock;
	IMG_UINT32 ui32ServerASync;
	IMG_UINT32 ui32ServerSyncVA;
	IMG_UINT32 ui32ServerSync;
	IMG_UINT32 ui32ServerRBF;
	IMG_UINT32 ui32ServerGF;
	IMG_UINT32 ui32ServerDGF;
	IMG_UINT32 ui32ServerDTL;
	IMG_UINT32 ui32ClientSync;
	IMG_UINT32 ui32ClientRBF;
	IMG_UINT32 ui32KMDiscards;
	IMG_UINT32 ui32UMDiscards;
	IMG_UINT32 ui32TotalFenceOps;
	IMG_UINT32 ui32TotalExecOps;
	IMG_UINT32 ui32AvgExecTime;
	IMG_UINT32 ui32AvgFenceTime;
	IMG_INT32 i32StatsExecWriteIdx;
	CACHEOP_STATS_EXEC_ITEM asStatsExecuted[CACHEOP_STATS_ITEMS_MAX];
#endif
/*
  CacheOp (re)configuration
 */
	void *pvConfigTune;
	IMG_HANDLE hConfigLock;
/*
  CacheOp deferred worker thread
  + eConfig
	- Runtime configuration
  + hWorkerThread
	- CacheOp thread handler
  + hThreadWakeUpEvtObj
	- Event object to drive CacheOp worker thread sleep/wake-ups.
  + hClientWakeUpEvtObj
	- Event object to unblock stalled clients waiting on queue.
  +  uiWorkerThreadPid
	- CacheOp thread process id
 */
	CACHEOP_CONFIG	eConfig;
	IMG_UINT32		ui32Config;
	IMG_BOOL		bConfigTuning;
	IMG_HANDLE		hWorkerThread;
	IMG_HANDLE 		hDeferredLock;
	IMG_HANDLE 		hGlobalFlushLock;
	IMG_PID			uiWorkerThreadPid;
	IMG_HANDLE		hThreadWakeUpEvtObj;
	IMG_HANDLE		hClientWakeUpEvtObj;
	IMG_UINT32		ui32FenceWaitTimeUs;
	IMG_UINT32		ui32FenceRetryAbort;
	IMG_BOOL		bNoGlobalFlushImpl;
	IMG_BOOL		bSupportsUMFlush;
} CACHEOP_WORK_QUEUE;

/* Top-level CacheOp framework object */
static CACHEOP_WORK_QUEUE gsCwq = {0};

#define CacheOpConfigSupports(e) ((gsCwq.eConfig & (e)) ? IMG_TRUE : IMG_FALSE)

static INLINE IMG_UINT32 CacheOpIdxRead(ATOMIC_T *phCounter)
{
	IMG_UINT32 ui32Idx = OSAtomicRead(phCounter);
	return ui32Idx & CACHEOP_INDICES_MASK;
}

static INLINE IMG_UINT32 CacheOpIdxIncrement(ATOMIC_T *phCounter)
{
	IMG_UINT32 ui32Idx = OSAtomicIncrement(phCounter);
	return ui32Idx & CACHEOP_INDICES_MASK;
}

static INLINE IMG_UINT32 CacheOpIdxNext(ATOMIC_T *phCounter)
{
	IMG_UINT32 ui32Idx = OSAtomicRead(phCounter);
	return ++ui32Idx & CACHEOP_INDICES_MASK;
}

static INLINE IMG_UINT32 CacheOpIdxSpan(ATOMIC_T *phLhs, ATOMIC_T *phRhs)
{
	return OSAtomicRead(phLhs) - OSAtomicRead(phRhs);
}

static INLINE IMG_UINT64 DivBy10(IMG_UINT64 uiNum)
{
	IMG_UINT64 uiQuot;
	IMG_UINT64 uiRem;

	uiQuot = (uiNum >> 1) + (uiNum >> 2);
	uiQuot = uiQuot + (uiQuot >> 4);
	uiQuot = uiQuot + (uiQuot >> 8);
	uiQuot = uiQuot + (uiQuot >> 16);
	uiQuot = uiQuot >> 3;
	uiRem  = uiNum - (((uiQuot << 2) + uiQuot) << 1);

	return uiQuot + (uiRem > 9);
}

#if defined(CACHEOP_DEBUG)
static INLINE void CacheOpStatsExecLogHeader(IMG_CHAR szBuffer[CACHEOP_MAX_DEBUG_MESSAGE_LEN])
{
	OSSNPrintf(szBuffer, CACHEOP_MAX_DEBUG_MESSAGE_LEN,
#if defined(PVR_RI_DEBUG) && defined(DEBUG)
				CACHEOP_RI_PRINTF_HEADER,
#else
				CACHEOP_PRINTF_HEADER,
#endif
				"Pid",
				"CacheOp",
				"  Type",
				"Mode",
#if defined(PVR_RI_DEBUG) && defined(DEBUG)
				"DevVAddr",
				"DevPAddr",
#endif
				"Offset",
				"Size",
				"xTime (us)",
				"qTime (us)",
				"SeqNum");
}

static INLINE void CacheOpStatsExecLogWrite(CACHEOP_WORK_ITEM *psCacheOpWorkItem)
{
	IMG_UINT64 ui64ExecuteTime;
	IMG_UINT64 ui64EnqueuedTime;
	IMG_INT32 i32WriteOffset;

	if (!psCacheOpWorkItem->ui32OpSeqNum && !psCacheOpWorkItem->uiCacheOp)
	{
		/* This breaks the logic of read-out, so we do not queue items
		   with zero sequence number and no CacheOp */
		return;
	}
	else if (psCacheOpWorkItem->bKMReq && !CacheOpConfigSupports(CACHEOP_CONFIG_KLOG))
	{
		/* KM logs spams the history due to frequency, this remove its completely */
		return;
	}

	OSLockAcquire(gsCwq.hStatsExecLock);

	i32WriteOffset = gsCwq.i32StatsExecWriteIdx;
	gsCwq.asStatsExecuted[i32WriteOffset].pid = psCacheOpWorkItem->pid;
	gsCwq.i32StatsExecWriteIdx = INCR_WRAP(gsCwq.i32StatsExecWriteIdx);
	gsCwq.asStatsExecuted[i32WriteOffset].bRBF = psCacheOpWorkItem->bRBF;
	gsCwq.asStatsExecuted[i32WriteOffset].bUMF = psCacheOpWorkItem->bUMF;
	gsCwq.asStatsExecuted[i32WriteOffset].uiSize = psCacheOpWorkItem->uiSize;
	gsCwq.asStatsExecuted[i32WriteOffset].bKMReq = psCacheOpWorkItem->bKMReq;
	gsCwq.asStatsExecuted[i32WriteOffset].uiOffset	= psCacheOpWorkItem->uiOffset;
	gsCwq.asStatsExecuted[i32WriteOffset].uiCacheOp = psCacheOpWorkItem->uiCacheOp;
	gsCwq.asStatsExecuted[i32WriteOffset].bDeferred = psCacheOpWorkItem->bDeferred;
	gsCwq.asStatsExecuted[i32WriteOffset].ui32OpSeqNum	= psCacheOpWorkItem->ui32OpSeqNum;
	gsCwq.asStatsExecuted[i32WriteOffset].ui64ExecuteTime = psCacheOpWorkItem->ui64ExecuteTime;
	gsCwq.asStatsExecuted[i32WriteOffset].ui64EnqueuedTime = psCacheOpWorkItem->ui64EnqueuedTime;
	gsCwq.asStatsExecuted[i32WriteOffset].ui64DequeuedTime = psCacheOpWorkItem->ui64DequeuedTime;
	/* During early system initialisation, only non-fence & non-PMR CacheOps are processed */
	gsCwq.asStatsExecuted[i32WriteOffset].bIsFence = gsCwq.bInit && !psCacheOpWorkItem->psPMR;
	PVR_ASSERT(gsCwq.asStatsExecuted[i32WriteOffset].pid);
#if defined(PVR_RI_DEBUG) && defined(DEBUG)
	if (gsCwq.bInit && psCacheOpWorkItem->psPMR)
	{
		IMG_CPU_PHYADDR sDevPAddr;
		PVRSRV_ERROR eError;
		IMG_BOOL bValid;

		/* Get more detailed information regarding the sub allocations that
		   PMR has from RI manager for process that requested the CacheOp */
		eError = RIDumpProcessListKM(psCacheOpWorkItem->psPMR,
									 gsCwq.asStatsExecuted[i32WriteOffset].pid,
									 gsCwq.asStatsExecuted[i32WriteOffset].uiOffset,
									 &gsCwq.asStatsExecuted[i32WriteOffset].sDevVAddr);
		if (eError != PVRSRV_OK)
		{
			return;
		}

		/* (Re)lock here as some PMR might have not been locked */
		eError = PMRLockSysPhysAddresses(psCacheOpWorkItem->psPMR);
		if (eError != PVRSRV_OK)
		{
			return;
		}

		eError = PMR_CpuPhysAddr(psCacheOpWorkItem->psPMR,
								 gsCwq.uiPageShift,
								 1,
								 gsCwq.asStatsExecuted[i32WriteOffset].uiOffset,
								 &sDevPAddr,
								 &bValid);
		if (eError != PVRSRV_OK)
		{
			eError = PMRUnlockSysPhysAddresses(psCacheOpWorkItem->psPMR);
			PVR_LOG_IF_ERROR(eError, "PMRUnlockSysPhysAddresses");
			return;
		}

		eError = PMRUnlockSysPhysAddresses(psCacheOpWorkItem->psPMR);
		PVR_LOG_IF_ERROR(eError, "PMRUnlockSysPhysAddresses");

		gsCwq.asStatsExecuted[i32WriteOffset].sDevPAddr.uiAddr = sDevPAddr.uiAddr;
	}

	if (gsCwq.asStatsExecuted[i32WriteOffset].bIsFence)
	{
		gsCwq.asStatsExecuted[i32WriteOffset].eFenceOpType = psCacheOpWorkItem->eFenceOpType;
	}
#endif

	/* Convert timing from nano-seconds to micro-seconds */
	ui64ExecuteTime = gsCwq.asStatsExecuted[i32WriteOffset].ui64ExecuteTime;
	ui64EnqueuedTime = gsCwq.asStatsExecuted[i32WriteOffset].ui64EnqueuedTime;
	ui64ExecuteTime = DivBy10(DivBy10(DivBy10(ui64ExecuteTime)));
	ui64EnqueuedTime = DivBy10(DivBy10(DivBy10(ui64EnqueuedTime)));

	/* Coalesced (to global) deferred CacheOps do not contribute to statistics,
	   as both enqueue/execute time is identical for these CacheOps */
	if (!gsCwq.asStatsExecuted[i32WriteOffset].bIsFence)
	{
		/* Calculate the rolling approximate average execution time */
		IMG_UINT32 ui32Time = ui64EnqueuedTime < ui64ExecuteTime ?
									ui64ExecuteTime - ui64EnqueuedTime :
									ui64EnqueuedTime - ui64ExecuteTime;
		if (gsCwq.ui32TotalExecOps > 2 && ui32Time)
		{
			gsCwq.ui32AvgExecTime -= (gsCwq.ui32AvgExecTime / gsCwq.ui32TotalExecOps);
			gsCwq.ui32AvgExecTime += (ui32Time / gsCwq.ui32TotalExecOps);
		}
		else if (ui32Time)
		{
			gsCwq.ui32AvgExecTime = (IMG_UINT32)ui32Time;
		}
	}

	if (! gsCwq.asStatsExecuted[i32WriteOffset].bKMReq)
	{
		/* This operation queues only UM CacheOp in per-PID process statistics database */
		PVRSRVStatsUpdateCacheOpStats(gsCwq.asStatsExecuted[i32WriteOffset].uiCacheOp,
						gsCwq.asStatsExecuted[i32WriteOffset].ui32OpSeqNum,
#if defined(PVR_RI_DEBUG) && defined(DEBUG)
						gsCwq.asStatsExecuted[i32WriteOffset].sDevVAddr,
						gsCwq.asStatsExecuted[i32WriteOffset].sDevPAddr,
						gsCwq.asStatsExecuted[i32WriteOffset].eFenceOpType,
#endif
						gsCwq.asStatsExecuted[i32WriteOffset].uiOffset,
						gsCwq.asStatsExecuted[i32WriteOffset].uiSize,
						ui64EnqueuedTime < ui64ExecuteTime ?
							ui64ExecuteTime - ui64EnqueuedTime:
							ui64EnqueuedTime - ui64ExecuteTime,
						gsCwq.asStatsExecuted[i32WriteOffset].bRBF,
						gsCwq.asStatsExecuted[i32WriteOffset].bUMF,
						gsCwq.asStatsExecuted[i32WriteOffset].bIsFence,
						psCacheOpWorkItem->pid);
	}

	OSLockRelease(gsCwq.hStatsExecLock);
}

static void CacheOpStatsExecLogRead(void *pvFilePtr, void *pvData,
								OS_STATS_PRINTF_FUNC* pfnOSStatsPrintf)
{
	IMG_CHAR *pszFlushype;
	IMG_CHAR *pszCacheOpType;
	IMG_CHAR *pszFlushSource;
	IMG_INT32 i32ReadOffset;
	IMG_INT32 i32WriteOffset;
	IMG_UINT64 ui64EnqueuedTime;
	IMG_UINT64 ui64DequeuedTime;
	IMG_UINT64 ui64ExecuteTime;
	IMG_CHAR szBuffer[CACHEOP_MAX_DEBUG_MESSAGE_LEN] = {0};
	PVR_UNREFERENCED_PARAMETER(pvData);

	OSLockAcquire(gsCwq.hStatsExecLock);

	pfnOSStatsPrintf(pvFilePtr,
			"Primary CPU d-cache architecture: LSZ: 0x%d, URBF: %s, KGF: %s, KRBF: %s\n",
			gsCwq.uiLineSize,
			gsCwq.bSupportsUMFlush ? "Yes" : "No",
			!gsCwq.bNoGlobalFlushImpl ? "Yes" : "No",
			"Yes" /* KRBF mechanism always available */
		);

	pfnOSStatsPrintf(pvFilePtr,
			"Framework runtime configuration: QSZ: %d, UKT: %d, KDFT: %d, KGFT: %d, KDF: %s, URBF: %s, KGF: %s, KRBF: %s\n",
			CACHEOP_INDICES_MAX,
			gsCwq.pui32InfoPage[CACHEOP_INFO_UMKMTHRESHLD],
			gsCwq.pui32InfoPage[CACHEOP_INFO_KMDFTHRESHLD],
			gsCwq.pui32InfoPage[CACHEOP_INFO_KMGFTHRESHLD],
			gsCwq.eConfig & CACHEOP_CONFIG_KDF  ? "Yes" : "No",
			gsCwq.eConfig & CACHEOP_CONFIG_URBF ? "Yes" : "No",
			gsCwq.eConfig & CACHEOP_CONFIG_KGF  ? "Yes" : "No",
			gsCwq.eConfig & CACHEOP_CONFIG_KRBF ? "Yes" : "No"
		);

	pfnOSStatsPrintf(pvFilePtr,
			"Summary: OP[F][TL] (tot.avg): %d.%d/%d.%d/%d, [KM][UM][A]SYNC: %d.%d/%d/%d, RBF (um/km): %d/%d, [D]GF (km): %d/%d, DSC (um/km): %d/%d\n",
			gsCwq.ui32TotalExecOps, gsCwq.ui32AvgExecTime, gsCwq.ui32TotalFenceOps, gsCwq.ui32AvgFenceTime, gsCwq.ui32ServerDTL,
			gsCwq.ui32ServerSync, gsCwq.ui32ServerSyncVA, gsCwq.ui32ClientSync,	gsCwq.ui32ServerASync,
			gsCwq.ui32ClientRBF,   gsCwq.ui32ServerRBF,
			gsCwq.ui32ServerDGF,   gsCwq.ui32ServerGF,
			gsCwq.ui32UMDiscards,  gsCwq.ui32KMDiscards
		);

	CacheOpStatsExecLogHeader(szBuffer);
	pfnOSStatsPrintf(pvFilePtr, "%s\n", szBuffer);

	i32WriteOffset = gsCwq.i32StatsExecWriteIdx;
	for (i32ReadOffset = DECR_WRAP(i32WriteOffset);
		 i32ReadOffset != i32WriteOffset;
		 i32ReadOffset = DECR_WRAP(i32ReadOffset))
	{
		if (!gsCwq.asStatsExecuted[i32ReadOffset].ui32OpSeqNum &&
			!gsCwq.asStatsExecuted[i32ReadOffset].uiCacheOp)
		{
			break;
		}

		/* Convert from nano-seconds to micro-seconds */
		ui64ExecuteTime = gsCwq.asStatsExecuted[i32ReadOffset].ui64ExecuteTime;
		ui64EnqueuedTime = gsCwq.asStatsExecuted[i32ReadOffset].ui64EnqueuedTime;
		ui64DequeuedTime = gsCwq.asStatsExecuted[i32ReadOffset].ui64DequeuedTime;
		ui64ExecuteTime = DivBy10(DivBy10(DivBy10(ui64ExecuteTime)));
		ui64EnqueuedTime = DivBy10(DivBy10(DivBy10(ui64EnqueuedTime)));
		ui64DequeuedTime = ui64DequeuedTime ? DivBy10(DivBy10(DivBy10(ui64DequeuedTime))) : 0;

		if (gsCwq.asStatsExecuted[i32ReadOffset].bIsFence)
		{
			IMG_CHAR *pszMode = "";
			IMG_CHAR *pszFenceType = "";
			pszCacheOpType = "Fence";

#if defined(PVR_RI_DEBUG) && defined(DEBUG)
			pszMode = gsCwq.asStatsExecuted[i32ReadOffset].uiCacheOp != PVRSRV_CACHE_OP_GLOBAL ? "" : "  GF  ";
			switch (gsCwq.asStatsExecuted[i32ReadOffset].eFenceOpType)
			{
				case RGXFWIF_DM_GP:
					pszFenceType = " GP/GF";
					break;

				case RGXFWIF_DM_TDM:
					pszFenceType = "  TDM ";
					break;

				case RGXFWIF_DM_TA:
					pszFenceType = "  TA ";
					break;

				case RGXFWIF_DM_3D:
					pszFenceType = "  PDM ";
					break;

				case RGXFWIF_DM_CDM:
					pszFenceType = "  CDM ";
					break;

				case RGXFWIF_DM_RTU:
					pszFenceType = "  RTU ";
					break;

				case RGXFWIF_DM_SHG:
					pszFenceType = "  SHG ";
					break;

				default:
					PVR_ASSERT(0);
					break;
			}
#else
			/* The CacheOp fence operation also triggered a global cache flush operation */
			pszFenceType =
				gsCwq.asStatsExecuted[i32ReadOffset].uiCacheOp != PVRSRV_CACHE_OP_GLOBAL ? "" : "   GF ";
#endif
			pfnOSStatsPrintf(pvFilePtr,
#if defined(PVR_RI_DEBUG) && defined(DEBUG)
							CACHEOP_RI_PRINTF,
#else
							CACHEOP_PRINTF,
#endif
							gsCwq.asStatsExecuted[i32ReadOffset].pid,
							pszCacheOpType,
							pszFenceType,
							pszMode,
#if defined(PVR_RI_DEBUG) && defined(DEBUG)
							"",
							"",
#endif
							gsCwq.asStatsExecuted[i32ReadOffset].uiOffset,
							gsCwq.asStatsExecuted[i32ReadOffset].uiSize,
							ui64EnqueuedTime < ui64ExecuteTime ?
									ui64ExecuteTime - ui64EnqueuedTime
										:
									ui64EnqueuedTime - ui64ExecuteTime,
							ui64EnqueuedTime < ui64DequeuedTime ?
									ui64DequeuedTime - ui64EnqueuedTime
										:
									!ui64DequeuedTime ? 0 : ui64EnqueuedTime - ui64DequeuedTime,
							gsCwq.asStatsExecuted[i32ReadOffset].ui32OpSeqNum);
		}
		else
		{
			if (gsCwq.asStatsExecuted[i32ReadOffset].bRBF)
			{
				IMG_DEVMEM_SIZE_T ui64NumOfPages;

				ui64NumOfPages = gsCwq.asStatsExecuted[i32ReadOffset].uiSize >> gsCwq.uiPageShift;
				if (ui64NumOfPages <= PMR_MAX_TRANSLATION_STACK_ALLOC)
				{
					pszFlushype = "RBF.Fast";
				}
				else
				{
					pszFlushype = "RBF.Slow";
				}
			}
			else
			{
				pszFlushype = "   GF ";
			}

			if (gsCwq.asStatsExecuted[i32ReadOffset].bUMF)
			{
				pszFlushSource = " UM";
			}
			else
			{
				/*
				   - Request originates directly from a KM thread or in KM (KM<), or
				   - Request originates from a UM thread and is KM deferred (KM+), or
				   - Request is/was discarded due to an 'else-[when,where]' GFlush
				     - i.e. GF occurs either (a)sync to current UM/KM thread
				*/
				pszFlushSource =
					gsCwq.asStatsExecuted[i32ReadOffset].bKMReq ? " KM<" :
					gsCwq.asStatsExecuted[i32ReadOffset].bDeferred && gsCwq.asStatsExecuted[i32ReadOffset].ui64ExecuteTime ? " KM+" :
					!gsCwq.asStatsExecuted[i32ReadOffset].ui64ExecuteTime ? " KM-" : " KM";
			}

			switch (gsCwq.asStatsExecuted[i32ReadOffset].uiCacheOp)
			{
				case PVRSRV_CACHE_OP_NONE:
					pszCacheOpType = "None";
					break;
				case PVRSRV_CACHE_OP_CLEAN:
					pszCacheOpType = "Clean";
					break;
				case PVRSRV_CACHE_OP_INVALIDATE:
					pszCacheOpType = "Invalidate";
					break;
				case PVRSRV_CACHE_OP_FLUSH:
					pszCacheOpType = "Flush";
					break;
				case PVRSRV_CACHE_OP_GLOBAL:
					pszCacheOpType = "GFlush";
					break;
				case PVRSRV_CACHE_OP_TIMELINE:
					pszCacheOpType = "Timeline";
					pszFlushype = "      ";
					break;
				default:
					if ((IMG_UINT32)gsCwq.asStatsExecuted[i32ReadOffset].uiCacheOp == (IMG_UINT32)(PVRSRV_CACHE_OP_GLOBAL|PVRSRV_CACHE_OP_TIMELINE))
					{
						pszCacheOpType = "Timeline";
					}
					else
					{
						pszCacheOpType = "Unknown";
						gsCwq.asStatsExecuted[i32ReadOffset].ui32OpSeqNum =
								(IMG_UINT32) gsCwq.asStatsExecuted[i32ReadOffset].uiCacheOp;
					}
					break;
			}

			pfnOSStatsPrintf(pvFilePtr,
#if defined(PVR_RI_DEBUG) && defined(DEBUG)
							CACHEOP_RI_PRINTF,
#else
							CACHEOP_PRINTF,
#endif
							gsCwq.asStatsExecuted[i32ReadOffset].pid,
							pszCacheOpType,
							pszFlushype,
							pszFlushSource,
#if defined(PVR_RI_DEBUG) && defined(DEBUG)
							gsCwq.asStatsExecuted[i32ReadOffset].sDevVAddr.uiAddr,
							gsCwq.asStatsExecuted[i32ReadOffset].sDevPAddr.uiAddr,
#endif
							gsCwq.asStatsExecuted[i32ReadOffset].uiOffset,
							gsCwq.asStatsExecuted[i32ReadOffset].uiSize,
							ui64EnqueuedTime < ui64ExecuteTime ?
										ui64ExecuteTime - ui64EnqueuedTime
											:
										ui64EnqueuedTime - ui64ExecuteTime,
							ui64EnqueuedTime < ui64DequeuedTime ?
									ui64DequeuedTime - ui64EnqueuedTime
										:
									!ui64DequeuedTime ? 0 : ui64EnqueuedTime - ui64DequeuedTime,
							gsCwq.asStatsExecuted[i32ReadOffset].ui32OpSeqNum);
		}
	}

	OSLockRelease(gsCwq.hStatsExecLock);
}
#endif /* defined(CACHEOP_DEBUG) */

static void CacheOpStatsReset(void)
{
#if defined(CACHEOP_DEBUG)
	gsCwq.ui32KMDiscards    = 0;
	gsCwq.ui32UMDiscards    = 0;
	gsCwq.ui32TotalExecOps  = 0;
	gsCwq.ui32TotalFenceOps = 0;
	gsCwq.ui32AvgExecTime   = 0;
	gsCwq.ui32AvgFenceTime  = 0;
	gsCwq.ui32ClientRBF     = 0;
	gsCwq.ui32ClientSync    = 0;
	gsCwq.ui32ServerRBF     = 0;
	gsCwq.ui32ServerASync   = 0;
	gsCwq.ui32ServerSyncVA   = 0;
	gsCwq.ui32ServerSync    = 0;
	gsCwq.ui32ServerGF      = 0;
	gsCwq.ui32ServerDGF     = 0;
	gsCwq.ui32ServerDTL     = 0;
	gsCwq.i32StatsExecWriteIdx = 0;
	OSCachedMemSet(gsCwq.asStatsExecuted, 0, sizeof(gsCwq.asStatsExecuted));
#endif
}

static void CacheOpConfigUpdate(IMG_UINT32 ui32Config)
{
	OSLockAcquire(gsCwq.hConfigLock);

	/* Step 0, set the gsCwq.eConfig bits */
	if (!(ui32Config & (CACHEOP_CONFIG_LAST - 1)))
	{
		gsCwq.bConfigTuning = IMG_FALSE;
		gsCwq.eConfig = CACHEOP_CONFIG_KRBF | CACHEOP_CONFIG_KDF;
		if (! gsCwq.bNoGlobalFlushImpl)
		{
			gsCwq.eConfig |= CACHEOP_CONFIG_KGF;
		}
		if (gsCwq.bSupportsUMFlush)
		{
			gsCwq.eConfig |= CACHEOP_CONFIG_URBF;
		}
	}
	else
	{
		gsCwq.bConfigTuning = IMG_TRUE;

		if (ui32Config & CACHEOP_CONFIG_KRBF)
		{
			gsCwq.eConfig |= CACHEOP_CONFIG_KRBF;
		}
		else
		{
			gsCwq.eConfig &= ~CACHEOP_CONFIG_KRBF;
		}

		if (ui32Config & CACHEOP_CONFIG_KDF)
		{
			gsCwq.eConfig |= CACHEOP_CONFIG_KDF;
		}
		else
		{
			gsCwq.eConfig &= ~CACHEOP_CONFIG_KDF;
		}

		if (!gsCwq.bNoGlobalFlushImpl && (ui32Config & CACHEOP_CONFIG_KGF))
		{
			gsCwq.eConfig |= CACHEOP_CONFIG_KGF;
		}
		else
		{
			gsCwq.eConfig &= ~CACHEOP_CONFIG_KGF;
		}

		if (gsCwq.bSupportsUMFlush && (ui32Config & CACHEOP_CONFIG_URBF))
		{
			gsCwq.eConfig |= CACHEOP_CONFIG_URBF;
		}
		else
		{
			gsCwq.eConfig &= ~CACHEOP_CONFIG_URBF;
		}
	}

	if (ui32Config & CACHEOP_CONFIG_KLOG)
	{
		/* Suppress logs from KM caller */
		gsCwq.eConfig |= CACHEOP_CONFIG_KLOG;
	}
	else
	{
		gsCwq.eConfig &= ~CACHEOP_CONFIG_KLOG;
	}

	/* Step 1, set gsCwq.ui32Config based on gsCwq.eConfig */
	ui32Config = 0;
	if (gsCwq.eConfig & CACHEOP_CONFIG_KRBF)
	{
		ui32Config |= CACHEOP_CONFIG_KRBF;
	}
	if (gsCwq.eConfig & CACHEOP_CONFIG_KDF)
	{
		ui32Config |= CACHEOP_CONFIG_KDF;
	}
	if (gsCwq.eConfig & CACHEOP_CONFIG_KGF)
	{
		ui32Config |= CACHEOP_CONFIG_KGF;
	}
	if (gsCwq.eConfig & CACHEOP_CONFIG_URBF)
	{
		ui32Config |= CACHEOP_CONFIG_URBF;
	}
	if (gsCwq.eConfig & CACHEOP_CONFIG_KLOG)
	{
		ui32Config |= CACHEOP_CONFIG_KLOG;
	}
	gsCwq.ui32Config = ui32Config;

	/* Step 2, Bar RBF promotion to GF, unless a GF is implemented */
	gsCwq.pui32InfoPage[CACHEOP_INFO_KMGFTHRESHLD] = (IMG_UINT32)~0;
	if (! gsCwq.bNoGlobalFlushImpl)
	{
		gsCwq.pui32InfoPage[CACHEOP_INFO_KMGFTHRESHLD] = (IMG_UINT32)PVR_DIRTY_BYTES_FLUSH_THRESHOLD;
	}

	/* Step 3, in certain cases where a CacheOp/VA is provided, this threshold determines at what point
	   the optimisation due to the presence of said VA (i.e. us not having to remap the PMR pages in KM)
	   is clawed-back because of the overhead of maintaining such large request which might stalls the
	   user thread; so to hide this latency have these CacheOps executed on deferred CacheOp thread */
	gsCwq.pui32InfoPage[CACHEOP_INFO_KMDFTHRESHLD] = (IMG_UINT32)(PVR_DIRTY_BYTES_FLUSH_THRESHOLD >> 2);

	/* Step 4, if no UM support, all requests are done in KM so zero these forcing all client requests
	   to come down into the KM for maintenance */
	gsCwq.pui32InfoPage[CACHEOP_INFO_UMKMTHRESHLD] = (IMG_UINT32)0;
	gsCwq.pui32InfoPage[CACHEOP_INFO_UMRBFONLY] = 0;
	if (gsCwq.bSupportsUMFlush)
	{
		/* If URBF has been selected exclusively OR selected but there is no GF implementation */
		if ((gsCwq.eConfig & CACHEOP_CONFIG_URBF) &&
			(gsCwq.bNoGlobalFlushImpl || !((gsCwq.ui32Config & (CACHEOP_CONFIG_LAST-1)) & ~CACHEOP_CONFIG_URBF)))
		{
			/* If only URBF has been selected, simulate without GF support OR no GF support means all client
			   requests should be done in UM. In both cases, set this threshold to the highest value to
			   prevent any client requests coming down to the server for maintenance. */
			gsCwq.pui32InfoPage[CACHEOP_INFO_UMKMTHRESHLD] = (IMG_UINT32)~0;
			gsCwq.pui32InfoPage[CACHEOP_INFO_UMRBFONLY] = 1;
		}
		/* This is the default entry for setting the UM info. page entries */
		else if ((gsCwq.eConfig & CACHEOP_CONFIG_URBF) && !gsCwq.bNoGlobalFlushImpl)
		{
			/* Set UM/KM threshold, all request sizes above this goes to server for GF maintenance _only_
			   because UM flushes already have VA acquired, no cost is incurred in per-page (re)mapping
			   of the to-be maintained PMR/page(s) as it the case with KM flushing so disallow KDF */
#if defined(ARM64) || defined(__aarch64__) || defined(__arm64__)
			/* This value is set to be higher for ARM64 due to a very optimised UM flush implementation */
			gsCwq.pui32InfoPage[CACHEOP_INFO_UMKMTHRESHLD] = gsCwq.pui32InfoPage[CACHEOP_INFO_KMGFTHRESHLD] << 5;
#else
			/* For others, assume an average UM flush performance, anything above should be promoted to GF.
			   For x86 UMA/LMA, we avoid KDF because remapping PMR/pages in KM might fail due to exhausted
			   or fragmented VMALLOC kernel VA space */
			gsCwq.pui32InfoPage[CACHEOP_INFO_UMKMTHRESHLD] = gsCwq.pui32InfoPage[CACHEOP_INFO_KMGFTHRESHLD];
#endif
			gsCwq.pui32InfoPage[CACHEOP_INFO_UMRBFONLY] = 0;
		}
	}

	/* Step 5, reset stats. */
	CacheOpStatsReset();

	OSLockRelease(gsCwq.hConfigLock);
}

static void CacheOpConfigRead(void *pvFilePtr, void *pvData,
							OS_STATS_PRINTF_FUNC* pfnOSStatsPrintf)
{
	PVR_UNREFERENCED_PARAMETER(pvData);
	pfnOSStatsPrintf(pvFilePtr,
			"KDF: %s, URBF: %s, KGF: %s, KRBF: %s\n",
			gsCwq.eConfig & CACHEOP_CONFIG_KDF  ? "Yes" : "No",
			gsCwq.eConfig & CACHEOP_CONFIG_URBF ? "Yes" : "No",
			gsCwq.eConfig & CACHEOP_CONFIG_KGF  ? "Yes" : "No",
			gsCwq.eConfig & CACHEOP_CONFIG_KRBF ? "Yes" : "No"
		);
}

static PVRSRV_ERROR CacheOpConfigQuery(const PVRSRV_DEVICE_NODE *psDevNode,
									   const void *psPrivate,
									   IMG_UINT32 *pui32Value)
{
	PVR_UNREFERENCED_PARAMETER(psDevNode);
	PVR_UNREFERENCED_PARAMETER(psPrivate);
	*pui32Value = gsCwq.ui32Config;
	return PVRSRV_OK;
}

static PVRSRV_ERROR CacheOpConfigSet(const PVRSRV_DEVICE_NODE *psDevNode,
									const void *psPrivate,
									IMG_UINT32 ui32Value)
{
	PVR_UNREFERENCED_PARAMETER(psDevNode);
	PVR_UNREFERENCED_PARAMETER(psPrivate);
	CacheOpConfigUpdate(ui32Value & CACHEOP_CONFIG_ALL);
	return PVRSRV_OK;
}

static INLINE void CacheOpQItemRecycle(CACHEOP_WORK_ITEM *psCacheOpWorkItem)
{
	PVRSRV_ERROR eError;
	eError = PMRUnlockSysPhysAddresses(psCacheOpWorkItem->psPMR);
	PVR_LOG_IF_ERROR(eError, "PMRUnlockSysPhysAddresses");
	/* Set to max as precaution should recycling this CacheOp index fail
	   to reset it, this is purely to safe-guard having to discard such
	   subsequent deferred CacheOps or signal the sw sync timeline */
	psCacheOpWorkItem->iTimeline = PVRSRV_NO_UPDATE_TIMELINE_REQUIRED;
	psCacheOpWorkItem->ui32GFSeqNum = (IMG_UINT32)~0;
	psCacheOpWorkItem->ui32OpSeqNum = (IMG_UINT32)~0;
#if defined(CACHEOP_DEBUG)
	psCacheOpWorkItem->psPMR = (void *)(uintptr_t)~0;
#endif
}

static INLINE void CacheOpQItemReadCheck(CACHEOP_WORK_ITEM *psCacheOpWorkItem)
{
#if defined(CACHEOP_DEBUG)
	PVR_ASSERT(psCacheOpWorkItem->psPMR);
	PVR_ASSERT(psCacheOpWorkItem->psPMR != (void *)(uintptr_t)~0);
	PVR_ASSERT(psCacheOpWorkItem->ui32OpSeqNum != (IMG_UINT32)~0);
	if (CacheOpConfigSupports(CACHEOP_CONFIG_KGF))
	{
		PVR_ASSERT(psCacheOpWorkItem->ui32GFSeqNum != (IMG_UINT32)~0);
	}
#else
	PVR_UNREFERENCED_PARAMETER(psCacheOpWorkItem);
#endif
}

static INLINE void CacheOpQItemWriteCheck(CACHEOP_WORK_ITEM *psCacheOpWorkItem)
{
#if defined(CACHEOP_DEBUG)
	PVR_ASSERT(psCacheOpWorkItem->psPMR == (void *)(uintptr_t)~0);
	PVR_ASSERT(psCacheOpWorkItem->ui32OpSeqNum == (IMG_UINT32)~0);
	PVR_ASSERT(psCacheOpWorkItem->ui32GFSeqNum == (IMG_UINT32)~0);
	PVR_ASSERT(psCacheOpWorkItem->iTimeline == PVRSRV_NO_UPDATE_TIMELINE_REQUIRED);
#else
	PVR_UNREFERENCED_PARAMETER(psCacheOpWorkItem);
#endif
}

static INLINE IMG_UINT32 CacheOpGetNextCommonSeqNum(void)
{
	IMG_UINT32 ui32SeqNum = OSAtomicIncrement(&gsCwq.hCommonSeqNum);
	if (! ui32SeqNum)
	{
		/* Zero is _not_ a valid sequence value, doing so simplifies _all_
		   subsequent fence checking when no cache maintenance operation
		   is outstanding as in this case a fence value of zero is supplied. */
		if (CacheOpConfigSupports(CACHEOP_CONFIG_KGF))
		{
			/* Also when seqNum wraps around/crosses zero, this requires us to
			   ensure that GFSEQNUM is not erroneously higher than any/all client
			   seqNum(s) in the system during this wrap-around transition so we
			   disable both momentarily until the next GF comes along. This has
			   the effect that all subsequent in-flight discards using ">" is
			   never true seeing zero is _not_ greater than anything and all
			   "<=" comparison are always true seeing zero is always less than
			   all non-zero integers. The additional GF here is done mostly to
			   account for race condition(s) during this transition for all
			   pending seqNum(s) that are still behind zero. */
			gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0] = 0;
			gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM1] = 0;
			ui32SeqNum = OSAtomicIncrement(&gsCwq.hCommonSeqNum);
			(void) OSCPUOperation(PVRSRV_CACHE_OP_FLUSH);
		}
		else
		{
			ui32SeqNum = OSAtomicIncrement(&gsCwq.hCommonSeqNum);
		}
	}
	return ui32SeqNum;
}

static INLINE PVRSRV_ERROR CacheOpGlobalFlush(void)
{
#if !defined(CACHEFLUSH_ISA_SUPPORTS_GLOBAL_FLUSH)
	return PVRSRV_ERROR_NOT_IMPLEMENTED;
#else
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32OpSeqNum = gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0];

	if (! CacheOpConfigSupports(CACHEOP_CONFIG_KGF))
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	OSLockAcquire(gsCwq.hGlobalFlushLock);
	if (ui32OpSeqNum < gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0])
	{
#if defined(CACHEOP_DEBUG)
		gsCwq.ui32KMDiscards += 1;
#endif
		goto exit;
	}

	/* User space sampling the information-page seqNumbers after this point
	   and before the corresponding GFSEQNUM0 update leads to an invalid
	   sampling which must be discarded by UM. This implements a lockless
	   critical region for a single KM(writer) & multiple UM/KM(readers) */
	ui32OpSeqNum = CacheOpGetNextCommonSeqNum();
	gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM1] = ui32OpSeqNum;

	eError = OSCPUOperation(PVRSRV_CACHE_OP_FLUSH);
	PVR_LOGR_IF_ERROR(eError, "OSCPUOperation(PVRSRV_CACHE_OP_FLUSH)");

	gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0] = ui32OpSeqNum;
	OSAtomicWrite(&gsCwq.hDeferredSize, 0);
#if defined(CACHEOP_DEBUG)
	gsCwq.ui32ServerGF += 1;
#endif

exit:
	OSLockRelease(gsCwq.hGlobalFlushLock);
	return PVRSRV_OK;
#endif
}

static INLINE void CacheOpExecRangeBased(PVRSRV_DEVICE_NODE *psDevNode,
										PVRSRV_CACHE_OP uiCacheOp,
										IMG_BYTE *pbCpuVirtAddr,
										IMG_CPU_PHYADDR sCpuPhyAddr,
										IMG_DEVMEM_OFFSET_T uiPgAlignedOffset,
										IMG_DEVMEM_OFFSET_T uiCLAlignedStartOffset,
										IMG_DEVMEM_OFFSET_T uiCLAlignedEndOffset)
{
	IMG_BYTE *pbCpuVirtAddrEnd;
	IMG_BYTE *pbCpuVirtAddrStart;
	IMG_CPU_PHYADDR sCpuPhyAddrEnd;
	IMG_CPU_PHYADDR sCpuPhyAddrStart;
	IMG_DEVMEM_SIZE_T uiRelFlushSize;
	IMG_DEVMEM_OFFSET_T uiRelFlushOffset;
	IMG_DEVMEM_SIZE_T uiNextPgAlignedOffset;

	/* These quantities allows us to perform cache operations
	   at cache-line granularity thereby ensuring we do not
	   perform more than is necessary */
	PVR_ASSERT(uiPgAlignedOffset < uiCLAlignedEndOffset);
	uiRelFlushSize = (IMG_DEVMEM_SIZE_T)gsCwq.uiPageSize;
	uiRelFlushOffset = 0;

	if (uiCLAlignedStartOffset > uiPgAlignedOffset)
	{
		/* Zero unless initially starting at an in-page offset */
		uiRelFlushOffset = uiCLAlignedStartOffset - uiPgAlignedOffset;
		uiRelFlushSize -= uiRelFlushOffset;
	}

	/* uiRelFlushSize is gsCwq.uiPageSize unless current outstanding CacheOp
	   size is smaller. The 1st case handles in-page CacheOp range and
	   the 2nd case handles multiple-page CacheOp range with a last
	   CacheOp size that is less than gsCwq.uiPageSize */
	uiNextPgAlignedOffset = uiPgAlignedOffset + (IMG_DEVMEM_SIZE_T)gsCwq.uiPageSize;
	if (uiNextPgAlignedOffset < uiPgAlignedOffset)
	{
		/* uiNextPgAlignedOffset is greater than uiCLAlignedEndOffset
		   by implication of this wrap-round; this only happens when
		   uiPgAlignedOffset is the last page aligned offset */
		uiRelFlushSize = uiRelFlushOffset ?
				uiCLAlignedEndOffset - uiCLAlignedStartOffset :
				uiCLAlignedEndOffset - uiPgAlignedOffset;
	}
	else
	{
		if (uiNextPgAlignedOffset > uiCLAlignedEndOffset)
		{
			uiRelFlushSize = uiRelFlushOffset ?
					uiCLAlignedEndOffset - uiCLAlignedStartOffset :
					uiCLAlignedEndOffset - uiPgAlignedOffset;
		}
	}

	/* More efficient to request cache maintenance operation for full
	   relative range as opposed to multiple cache-aligned ranges */
	sCpuPhyAddrStart.uiAddr = sCpuPhyAddr.uiAddr + uiRelFlushOffset;
	sCpuPhyAddrEnd.uiAddr = sCpuPhyAddrStart.uiAddr + uiRelFlushSize;
	if (pbCpuVirtAddr)
	{
		pbCpuVirtAddrStart = pbCpuVirtAddr + uiRelFlushOffset;
		pbCpuVirtAddrEnd = pbCpuVirtAddrStart + uiRelFlushSize;
	}
	else
	{
		/* Some OS/Env layer support functions expect NULL(s) */
		pbCpuVirtAddrStart = NULL;
		pbCpuVirtAddrEnd = NULL;
	}

	/* Perform requested CacheOp on the CPU data cache for successive cache
	   line worth of bytes up to page or in-page cache-line boundary */
	switch (uiCacheOp)
	{
		case PVRSRV_CACHE_OP_CLEAN:
			OSCPUCacheCleanRangeKM(psDevNode, pbCpuVirtAddrStart, pbCpuVirtAddrEnd,
									sCpuPhyAddrStart, sCpuPhyAddrEnd);
			break;
		case PVRSRV_CACHE_OP_INVALIDATE:
			OSCPUCacheInvalidateRangeKM(psDevNode, pbCpuVirtAddrStart, pbCpuVirtAddrEnd,
									sCpuPhyAddrStart, sCpuPhyAddrEnd);
			break;
		case PVRSRV_CACHE_OP_FLUSH:
			OSCPUCacheFlushRangeKM(psDevNode, pbCpuVirtAddrStart, pbCpuVirtAddrEnd,
									sCpuPhyAddrStart, sCpuPhyAddrEnd);
			break;
		default:
			PVR_DPF((PVR_DBG_ERROR,	"%s: Invalid cache operation type %d",
					__FUNCTION__, uiCacheOp));
			break;
	}

#if defined(CACHEOP_DEBUG)
	/* Tracks the number of kernel-mode cacheline maintenance instructions */
	gsCwq.ui32ServerRBF += (uiRelFlushSize & ((IMG_DEVMEM_SIZE_T)~(gsCwq.uiLineSize - 1))) >> gsCwq.uiLineShift;
#endif
}

static INLINE void CacheOpExecRangeBasedVA(PVRSRV_DEVICE_NODE *psDevNode,
										 IMG_CPU_VIRTADDR pvAddress,
										 IMG_DEVMEM_SIZE_T uiSize,
										 PVRSRV_CACHE_OP uiCacheOp)
{
	IMG_CPU_PHYADDR sCpuPhyAddrUnused = {(uintptr_t)0xCAFEF00DDEADBEEF};
	IMG_BYTE *pbEnd = (IMG_BYTE*)((uintptr_t)pvAddress + (uintptr_t)uiSize);
	IMG_BYTE *pbStart = (IMG_BYTE*)((uintptr_t)pvAddress & ~((uintptr_t)gsCwq.uiLineSize-1));

	/*
	  If the start/end address isn't aligned to cache line size, round it up to the
	  nearest multiple; this ensures that we flush all the cache lines affected by
	  unaligned start/end addresses.
	 */
	pbEnd = (IMG_BYTE *) PVR_ALIGN((uintptr_t)pbEnd, (uintptr_t)gsCwq.uiLineSize);
	switch (uiCacheOp)
	{
		case PVRSRV_CACHE_OP_CLEAN:
			OSCPUCacheCleanRangeKM(psDevNode, pbStart, pbEnd, sCpuPhyAddrUnused, sCpuPhyAddrUnused);
			break;
		case PVRSRV_CACHE_OP_INVALIDATE:
			OSCPUCacheInvalidateRangeKM(psDevNode, pbStart, pbEnd, sCpuPhyAddrUnused, sCpuPhyAddrUnused);
			break;
		case PVRSRV_CACHE_OP_FLUSH:
			OSCPUCacheFlushRangeKM(psDevNode, pbStart, pbEnd, sCpuPhyAddrUnused, sCpuPhyAddrUnused);
			break;
		default:
			PVR_DPF((PVR_DBG_ERROR,	"%s: Invalid cache operation type %d", __FUNCTION__, uiCacheOp));
			break;
	}

#if defined(CACHEOP_DEBUG)
	/* Tracks the number of kernel-mode cacheline maintenance instructions */
	gsCwq.ui32ServerRBF += (uiSize & ((IMG_DEVMEM_SIZE_T)~(gsCwq.uiLineSize - 1))) >> gsCwq.uiLineShift;
#endif
}

static IMG_CPU_VIRTADDR CacheOpValidateVAOffset(PMR *psPMR,
												IMG_CPU_VIRTADDR pvAddress,
												IMG_DEVMEM_OFFSET_T uiOffset,
												IMG_DEVMEM_SIZE_T uiSize)
{
#if defined(LINUX)
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
#endif

	if (! pvAddress)
	{
		return NULL;
	}

#if !defined(LINUX)
	pvAddress = NULL;
#else
	/* Offset VA, validate UM/VA, skip all KM/VA as it's pre-validated */
	pvAddress = (void*)(uintptr_t)((uintptr_t)pvAddress + uiOffset);
	if (access_ok(VERIFY_READ, pvAddress, uiSize))
	{
		down_read(&mm->mmap_sem);
		vma = find_vma(mm, (unsigned long)(uintptr_t)pvAddress);
		if (!vma ||
			vma->vm_start > (unsigned long)(uintptr_t)pvAddress ||
			vma->vm_end - vma->vm_start > (unsigned long)(uintptr_t)uiSize)
		{
			/* Out of range mm_struct->vm_area VA */
			pvAddress = NULL;
		}
		else if (vma->vm_private_data != psPMR)
		{
			/*
			   Unknown mm_struct->vm_area VA, can't risk dcache maintenance using
			   this VA as the client user space mapping could be removed without
			   us knowing which might induce CPU fault during cache maintenance.
			*/
			pvAddress = NULL;
		}
		up_read(&mm->mmap_sem);
	}
	/* Fail if access is not OK and the supplied address is a client user space VA */
	else if ((IMG_UINT64)(uintptr_t)pvAddress <= OSGetCurrentProcessVASpaceSize())
	{
		pvAddress = NULL;
	}
#endif

	return pvAddress;
}

static PVRSRV_ERROR CacheOpPMRExec (PMR *psPMR,
									IMG_CPU_VIRTADDR pvAddress,
									IMG_DEVMEM_OFFSET_T uiOffset,
									IMG_DEVMEM_SIZE_T uiSize,
									PVRSRV_CACHE_OP uiCacheOp,
									IMG_UINT32 ui32GFlushSeqNum,
									IMG_BOOL bIsRequestValidated,
									IMG_BOOL *pbUsedGlobalFlush)
{
	IMG_HANDLE hPrivOut;
	IMG_BOOL bPMRIsSparse;
	IMG_UINT32 ui32PageIndex;
	IMG_UINT32 ui32NumOfPages;
	IMG_DEVMEM_SIZE_T uiOutSize;
	PVRSRV_DEVICE_NODE *psDevNode;
	IMG_DEVMEM_SIZE_T uiPgAlignedSize;
	IMG_DEVMEM_OFFSET_T uiPgAlignedOffset;
	IMG_DEVMEM_OFFSET_T uiCLAlignedEndOffset;
	IMG_DEVMEM_OFFSET_T uiPgAlignedEndOffset;
	IMG_DEVMEM_OFFSET_T uiCLAlignedStartOffset;
	IMG_DEVMEM_OFFSET_T uiPgAlignedStartOffset;
	IMG_BOOL abValid[PMR_MAX_TRANSLATION_STACK_ALLOC];
	IMG_CPU_PHYADDR asCpuPhyAddr[PMR_MAX_TRANSLATION_STACK_ALLOC];
	IMG_CPU_PHYADDR *psCpuPhyAddr = asCpuPhyAddr;
	IMG_BOOL bIsPMRInfoValid = IMG_FALSE;
	PVRSRV_ERROR eError = PVRSRV_OK;
	IMG_BYTE *pbCpuVirtAddr = NULL;
	IMG_BOOL *pbValid = abValid;

	if (uiCacheOp == PVRSRV_CACHE_OP_NONE || uiCacheOp == PVRSRV_CACHE_OP_TIMELINE)
	{
		return PVRSRV_OK;
	}
	/* Some cache ISA(s) supports privilege kernel-mode global flush, if it fails then fall-back to KRBF */
	else if (uiCacheOp == PVRSRV_CACHE_OP_GLOBAL || uiSize >= gsCwq.pui32InfoPage[CACHEOP_INFO_KMGFTHRESHLD])
	{
		if (gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0] > ui32GFlushSeqNum)
		{
			*pbUsedGlobalFlush = IMG_FALSE;
#if defined(CACHEOP_DEBUG)
			gsCwq.ui32KMDiscards += 1;
#endif
			return PVRSRV_OK;
		}
		else
		{
			eError = CacheOpGlobalFlush();
			if (eError == PVRSRV_OK)
			{
				*pbUsedGlobalFlush = IMG_TRUE;
				return PVRSRV_OK;
			}
			else if (uiCacheOp == PVRSRV_CACHE_OP_GLOBAL)
			{
				/* Cannot fall-back to KRBF as an explicit KGF was erroneously requested for */
				*pbUsedGlobalFlush = IMG_FALSE;
				PVR_ASSERT(eError == PVRSRV_OK);
				PVR_LOGR_IF_ERROR(eError, CACHEOP_NO_GFLUSH_ERROR_STRING);
			}
		}
	}
	else if (! uiSize)
	{
		/* GF are queued with !size, so check for GF first before !size */
		return PVRSRV_OK;
	}

	if (! bIsRequestValidated)
	{
		IMG_DEVMEM_SIZE_T uiLogicalSize;

		/* Need to validate parameters before proceeding */
		eError = PMR_LogicalSize(psPMR, &uiLogicalSize);
		PVR_LOGR_IF_ERROR(eError, "PMR_LogicalSize");

		PVR_LOGR_IF_FALSE(((uiOffset+uiSize) <= uiLogicalSize), CACHEOP_DEVMEM_OOR_ERROR_STRING, PVRSRV_ERROR_DEVICEMEM_OUT_OF_RANGE);

		eError = PMRLockSysPhysAddresses(psPMR);
		PVR_LOGR_IF_ERROR(eError, "PMRLockSysPhysAddresses");
	}

	/* Note we're using KRBF Flush */
	*pbUsedGlobalFlush = IMG_FALSE;

	/* Fast track if VA is provided and CPU ISA supports VA only maintenance */
	pvAddress = CacheOpValidateVAOffset(psPMR, pvAddress, uiOffset, uiSize);
	if (pvAddress && gsCwq.uiCacheOpAddrType == PVRSRV_CACHE_OP_ADDR_TYPE_VIRTUAL)
	{
		CacheOpExecRangeBasedVA(PMR_DeviceNode(psPMR), pvAddress, uiSize, uiCacheOp);
		if (! bIsRequestValidated)
		{
			eError = PMRUnlockSysPhysAddresses(psPMR);
			PVR_LOG_IF_ERROR(eError, "PMRUnlockSysPhysAddresses");
		}
#if defined(CACHEOP_DEBUG)
		gsCwq.ui32ServerSyncVA += 1;
#endif
		return PVRSRV_OK;
	}
	else if (pvAddress)
	{
		/* Round down the incoming VA (if any) down to the nearest page aligned VA */
		 pvAddress = (void*)((uintptr_t)pvAddress & ~((uintptr_t)gsCwq.uiPageSize-1));
#if defined(CACHEOP_DEBUG)
		gsCwq.ui32ServerSyncVA += 1;
#endif
	}

	/* Need this for kernel mapping */
	bPMRIsSparse = PMR_IsSparse(psPMR);
	psDevNode = PMR_DeviceNode(psPMR);

	/* Round the incoming offset down to the nearest cache-line / page aligned-address */
	uiCLAlignedEndOffset = uiOffset + uiSize;
	uiCLAlignedEndOffset = PVR_ALIGN(uiCLAlignedEndOffset, (IMG_DEVMEM_SIZE_T)gsCwq.uiLineSize);
	uiCLAlignedStartOffset = (uiOffset & ~((IMG_DEVMEM_OFFSET_T)gsCwq.uiLineSize-1));

	uiPgAlignedEndOffset = uiCLAlignedEndOffset;
	uiPgAlignedEndOffset = PVR_ALIGN(uiPgAlignedEndOffset, (IMG_DEVMEM_SIZE_T)gsCwq.uiPageSize);
	uiPgAlignedStartOffset = (uiOffset & ~((IMG_DEVMEM_OFFSET_T)gsCwq.uiPageSize-1));
	uiPgAlignedSize = uiPgAlignedEndOffset - uiPgAlignedStartOffset;

#if defined(CACHEOP_NO_CACHE_LINE_ALIGNED_ROUNDING)
	/* For internal debug if cache-line optimised
	   flushing is suspected of causing data corruption */
	uiCLAlignedStartOffset = uiPgAlignedStartOffset;
	uiCLAlignedEndOffset = uiPgAlignedEndOffset;
#endif

	/* Type of allocation backing the PMR data */
	ui32NumOfPages = uiPgAlignedSize >> gsCwq.uiPageShift;
	if (ui32NumOfPages > PMR_MAX_TRANSLATION_STACK_ALLOC)
	{
		/* The pbValid array is allocated first as it is needed in
		   both physical/virtual cache maintenance methods */
		pbValid = OSAllocZMem(ui32NumOfPages * sizeof(IMG_BOOL));
		if (! pbValid)
		{
			pbValid = abValid;
		}
		else if (gsCwq.uiCacheOpAddrType != PVRSRV_CACHE_OP_ADDR_TYPE_VIRTUAL)
		{
			psCpuPhyAddr = OSAllocZMem(ui32NumOfPages * sizeof(IMG_CPU_PHYADDR));
			if (! psCpuPhyAddr)
			{
				psCpuPhyAddr = asCpuPhyAddr;
				OSFreeMem(pbValid);
				pbValid = abValid;
			}
		}
	}

	/* We always retrieve PMR data in bulk, up-front if number of pages is within
	   PMR_MAX_TRANSLATION_STACK_ALLOC limits else we check to ensure that a
	   dynamic buffer has been allocated to satisfy requests outside limits */
	if (ui32NumOfPages <= PMR_MAX_TRANSLATION_STACK_ALLOC || pbValid != abValid)
	{
		if (gsCwq.uiCacheOpAddrType != PVRSRV_CACHE_OP_ADDR_TYPE_VIRTUAL)
		{
			/* Look-up PMR CpuPhyAddr once, if possible */
			eError = PMR_CpuPhysAddr(psPMR,
									 gsCwq.uiPageShift,
									 ui32NumOfPages,
									 uiPgAlignedStartOffset,
									 psCpuPhyAddr,
									 pbValid);
			if (eError == PVRSRV_OK)
			{
				bIsPMRInfoValid = IMG_TRUE;
			}
		}
		else
		{
			/* Look-up PMR per-page validity once, if possible */
			eError = PMR_IsOffsetValid(psPMR,
									   gsCwq.uiPageShift,
									   ui32NumOfPages,
									   uiPgAlignedStartOffset,
									   pbValid);
			bIsPMRInfoValid = (eError == PVRSRV_OK) ? IMG_TRUE : IMG_FALSE;
		}
	}

	/* For each device page, carry out the requested cache maintenance operation */
	for (uiPgAlignedOffset = uiPgAlignedStartOffset, ui32PageIndex = 0;
		 uiPgAlignedOffset < uiPgAlignedEndOffset;
		 uiPgAlignedOffset += (IMG_DEVMEM_OFFSET_T) gsCwq.uiPageSize, ui32PageIndex += 1)
	{
		/* Just before issuing the CacheOp RBF, check if it can be discarded */
		if (gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0] > ui32GFlushSeqNum)
		{
#if defined(CACHEOP_DEBUG)
			gsCwq.ui32KMDiscards += 1;
#endif
			break;
		}

		if (! bIsPMRInfoValid)
		{
			/* Never cross page boundary without looking up corresponding PMR page physical
			   address and/or page validity if these were not looked-up, in bulk, up-front */
			ui32PageIndex = 0;
			if (gsCwq.uiCacheOpAddrType != PVRSRV_CACHE_OP_ADDR_TYPE_VIRTUAL)
			{
				eError = PMR_CpuPhysAddr(psPMR,
										 gsCwq.uiPageShift,
										 1,
										 uiPgAlignedOffset,
										 psCpuPhyAddr,
										 pbValid);
				PVR_LOGG_IF_ERROR(eError, "PMR_CpuPhysAddr", e0);
			}
			else
			{
				eError = PMR_IsOffsetValid(psPMR,
										  gsCwq.uiPageShift,
										  1,
										  uiPgAlignedOffset,
										  pbValid);
				PVR_LOGG_IF_ERROR(eError, "PMR_IsOffsetValid", e0);
			}
		}

		/* Skip invalid PMR pages (i.e. sparse) */
		if (pbValid[ui32PageIndex] == IMG_FALSE)
		{
			PVR_ASSERT(bPMRIsSparse);
			continue;
		}

		if (pvAddress)
		{
			/* The caller has supplied either a KM/UM CpuVA, so use it unconditionally */
			pbCpuVirtAddr =
				(void *)(uintptr_t)((uintptr_t)pvAddress + (uintptr_t)(uiPgAlignedOffset-uiPgAlignedStartOffset));
		}
		/* Skip CpuVA acquire if CacheOp can be maintained entirely using CpuPA */
		else if (gsCwq.uiCacheOpAddrType != PVRSRV_CACHE_OP_ADDR_TYPE_PHYSICAL)
		{
			if (bPMRIsSparse)
			{
				eError =
					PMRAcquireSparseKernelMappingData(psPMR,
													  uiPgAlignedOffset,
													  gsCwq.uiPageSize,
													  (void **)&pbCpuVirtAddr,
													  (size_t*)&uiOutSize,
													  &hPrivOut);
				PVR_LOGG_IF_ERROR(eError, "PMRAcquireSparseKernelMappingData", e0);
			}
			else
			{
				eError =
					PMRAcquireKernelMappingData(psPMR,
												uiPgAlignedOffset,
												gsCwq.uiPageSize,
												(void **)&pbCpuVirtAddr,
												(size_t*)&uiOutSize,
												&hPrivOut);
				PVR_LOGG_IF_ERROR(eError, "PMRAcquireKernelMappingData", e0);
			}
		}

		/* Issue actual cache maintenance for PMR */
		CacheOpExecRangeBased(psDevNode,
							uiCacheOp,
							pbCpuVirtAddr,
							(gsCwq.uiCacheOpAddrType != PVRSRV_CACHE_OP_ADDR_TYPE_VIRTUAL) ?
								psCpuPhyAddr[ui32PageIndex] : psCpuPhyAddr[0],
							uiPgAlignedOffset,
							uiCLAlignedStartOffset,
							uiCLAlignedEndOffset);

		if (! pvAddress)
		{
			/* The caller has not supplied either a KM/UM CpuVA, release mapping */
			if (gsCwq.uiCacheOpAddrType != PVRSRV_CACHE_OP_ADDR_TYPE_PHYSICAL)
			{
				eError = PMRReleaseKernelMappingData(psPMR, hPrivOut);
				PVR_LOG_IF_ERROR(eError, "PMRReleaseKernelMappingData");
			}
		}
	}

e0:
	if (psCpuPhyAddr != asCpuPhyAddr)
	{
		OSFreeMem(psCpuPhyAddr);
	}

	if (pbValid != abValid)
	{
		OSFreeMem(pbValid);
	}

	if (! bIsRequestValidated)
	{
		eError = PMRUnlockSysPhysAddresses(psPMR);
		PVR_LOG_IF_ERROR(eError, "PMRUnlockSysPhysAddresses");
	}

	return eError;
}

static INLINE IMG_BOOL CacheOpFenceCheck(IMG_UINT32 ui32CompletedSeqNum,
										 IMG_UINT32 ui32FenceSeqNum)
{
	IMG_UINT32 ui32RebasedCompletedNum;
	IMG_UINT32 ui32RebasedFenceNum;
	IMG_UINT32 ui32Rebase;

	if (ui32FenceSeqNum == 0)
	{
		return IMG_TRUE;
	}

	/*
	   The problem statement is how to compare two values
	   on a numerical sequentially incrementing timeline in
	   the presence of wrap around arithmetic semantics using
	   a single ui32 counter & atomic (increment) operations.

	   The rationale for the solution here is to rebase the
	   incoming values to the sequence midpoint and perform
	   comparisons there; this allows us to handle overflow
	   or underflow wrap-round using only a single integer.

	   NOTE: We assume that the absolute value of the
	   difference between the two incoming values in _not_
	   greater than CACHEOP_SEQ_MIDPOINT. This assumption
	   holds as it implies that it is very _unlikely_ that 2
	   billion CacheOp requests could have been made between
	   a single client's CacheOp request & the corresponding
	   fence check. This code sequence is hopefully a _more_
	   hand optimised (branchless) version of this:

		   x = ui32CompletedOpSeqNum
		   y = ui32FenceOpSeqNum

		   if (|x - y| < CACHEOP_SEQ_MIDPOINT)
			   return (x - y) >= 0 ? true : false
		   else
			   return (y - x) >= 0 ? true : false
	 */
	ui32Rebase = CACHEOP_SEQ_MIDPOINT - ui32CompletedSeqNum;

	/* ui32Rebase could be either positive/negative, in
	   any case we still perform operation using unsigned
	   semantics as 2's complement notation always means
	   we end up with the correct result */
	ui32RebasedCompletedNum = ui32Rebase + ui32CompletedSeqNum;
	ui32RebasedFenceNum = ui32Rebase + ui32FenceSeqNum;

	return (ui32RebasedCompletedNum >= ui32RebasedFenceNum);
}

static INLINE PVRSRV_ERROR CacheOpTimelineBind(CACHEOP_WORK_ITEM *psCacheOpWorkItem,
											   PVRSRV_TIMELINE iTimeline)
{
	PVRSRV_ERROR eError;

	/* Always default the incoming CacheOp work-item to safe values */
	psCacheOpWorkItem->sSWTimelineObj = (SYNC_TIMELINE_OBJ)(uintptr_t)NULL;
	psCacheOpWorkItem->iTimeline = PVRSRV_NO_UPDATE_TIMELINE_REQUIRED;
	if (iTimeline == PVRSRV_NO_UPDATE_TIMELINE_REQUIRED)
	{
		return PVRSRV_OK;
	}

	psCacheOpWorkItem->iTimeline = iTimeline;
#if defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC)
	if (! PVRSRVIsTimelineValidKM(iTimeline))
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	eError = SyncSWGetTimelineObj(iTimeline, &psCacheOpWorkItem->sSWTimelineObj);
	PVR_LOG_IF_ERROR(eError, "SyncSWGetTimelineObj");
#else
	eError = PVRSRV_ERROR_NOT_IMPLEMENTED;
#endif

	return eError;
}

static INLINE PVRSRV_ERROR CacheOpTimelineExec(CACHEOP_WORK_ITEM *psCacheOpWorkItem)
{
	PVRSRV_ERROR eError;

	if (psCacheOpWorkItem->iTimeline == PVRSRV_NO_UPDATE_TIMELINE_REQUIRED)
	{
		return PVRSRV_OK;
	}
	PVR_ASSERT(psCacheOpWorkItem->sSWTimelineObj);

#if defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC)
	eError = SyncSWTimelineAdvanceKM(psCacheOpWorkItem->sSWTimelineObj);
	(void) SyncSWTimelineReleaseKM(psCacheOpWorkItem->sSWTimelineObj);
#else
	eError = PVRSRV_ERROR_NOT_IMPLEMENTED;
#endif

	return eError;
}

static INLINE PVRSRV_ERROR CacheOpQListExecGlobal(void)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	IMG_UINT32 ui32NumOfEntries;
	CACHEOP_WORK_ITEM *psCacheOpWorkItem;
#if defined(CACHEOP_DEBUG)
	IMG_UINT64 uiTimeNow = 0;
	IMG_UINT64 ui64DequeuedTime;
	CACHEOP_WORK_ITEM sCacheOpWorkItem = {0};
#endif
	PVR_ASSERT(!gsCwq.bNoGlobalFlushImpl);

	/* Take the current snapshot of queued CacheOps before we issue a global cache
	   flush operation so that we retire the right amount of CacheOps that has
	   been affected by the to-be executed global CacheOp */
	ui32NumOfEntries = CacheOpIdxSpan(&gsCwq.hWriteCounter, &gsCwq.hReadCounter);
	if (OSAtomicRead(&gsCwq.hWriteCounter) < OSAtomicRead(&gsCwq.hReadCounter))
	{
		/* Branch handles when the write-counter has wrapped-around in value space.
		   The logic here works seeing the read-counter does not change value for
		   the duration of this function so we don't run the risk of it too wrapping
		   round whilst the number of entries is being determined here, that is to
		   say the consumer process in this framework is single threaded and this
		   function is that thread */
		ui32NumOfEntries = CacheOpIdxSpan(&gsCwq.hReadCounter, &gsCwq.hWriteCounter);

		/* Two's complement arithmetic gives the number of entries */
		ui32NumOfEntries = CACHEOP_INDICES_MAX - ui32NumOfEntries;
	}
#if defined(CACHEOP_DEBUG)
	PVR_ASSERT(ui32NumOfEntries < CACHEOP_INDICES_MAX);
#endif

	/* Use the current/latest queue-tail work-item's GF/SeqNum to predicate GF */
	psCacheOpWorkItem = &gsCwq.asWorkItems[CacheOpIdxRead(&gsCwq.hWriteCounter)];
	CacheOpQItemReadCheck(psCacheOpWorkItem);
#if defined(CACHEOP_DEBUG)
	/* The time waiting in the queue to be serviced */
	ui64DequeuedTime = OSClockns64();
#endif

	/* Check if items between hRead/WriteCounter can be discarded before issuing GF */
	if (gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0] > psCacheOpWorkItem->ui32GFSeqNum)
	{
		/* The currently discarded CacheOp item updates gsCwq.hCompletedSeqNum */
		OSAtomicWrite(&gsCwq.hCompletedSeqNum, psCacheOpWorkItem->ui32OpSeqNum);
#if defined(CACHEOP_DEBUG)
		gsCwq.ui32KMDiscards += ui32NumOfEntries;
#endif
	}
	else
	{
		eError = CacheOpGlobalFlush();
		PVR_LOGR_IF_ERROR(eError, "CacheOpGlobalFlush");
#if defined(CACHEOP_DEBUG)
		uiTimeNow = OSClockns64();
		sCacheOpWorkItem.bDeferred = IMG_TRUE;
		sCacheOpWorkItem.ui64ExecuteTime = uiTimeNow;
		sCacheOpWorkItem.psPMR = gsCwq.psInfoPagePMR;
		sCacheOpWorkItem.pid = OSGetCurrentProcessID();
		sCacheOpWorkItem.uiCacheOp = PVRSRV_CACHE_OP_GLOBAL;
		sCacheOpWorkItem.ui64DequeuedTime = ui64DequeuedTime;
		sCacheOpWorkItem.ui64EnqueuedTime = psCacheOpWorkItem->ui64EnqueuedTime;
		sCacheOpWorkItem.ui32OpSeqNum = gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0];
#endif
	}

	while (ui32NumOfEntries)
	{
		psCacheOpWorkItem = &gsCwq.asWorkItems[CacheOpIdxNext(&gsCwq.hReadCounter)];
		CacheOpQItemReadCheck(psCacheOpWorkItem);

#if defined(CACHEOP_DEBUG)
		if (psCacheOpWorkItem->uiCacheOp != PVRSRV_CACHE_OP_GLOBAL)
		{
			psCacheOpWorkItem->bRBF = IMG_FALSE;
			if (! uiTimeNow)
			{
				/* Measure deferred queueing overhead only */
				uiTimeNow = OSClockns64();
				psCacheOpWorkItem->ui64ExecuteTime = uiTimeNow;
			}
			else
			{
				psCacheOpWorkItem->ui64ExecuteTime = uiTimeNow;
			}
			psCacheOpWorkItem->ui64DequeuedTime = ui64DequeuedTime;
			CacheOpStatsExecLogWrite(psCacheOpWorkItem);
		}
		/* Something's gone horribly wrong if these 2 counters are identical at this point */
		PVR_ASSERT(OSAtomicRead(&gsCwq.hWriteCounter) != OSAtomicRead(&gsCwq.hReadCounter));
#endif

		/* If CacheOp is timeline(d), notify timeline waiters */
		eError = CacheOpTimelineExec(psCacheOpWorkItem);
		PVR_LOG_IF_ERROR(eError, "CacheOpTimelineExec");

		/* Mark index as ready for recycling for next CacheOp */
		CacheOpQItemRecycle(psCacheOpWorkItem);
		(void) CacheOpIdxIncrement(&gsCwq.hReadCounter);
		ui32NumOfEntries = ui32NumOfEntries - 1;
	}

#if defined(CACHEOP_DEBUG)
	if (uiTimeNow)
	{
		/* Only log GF that was actually executed */
		CacheOpStatsExecLogWrite(&sCacheOpWorkItem);
	}
#endif

	return eError;
}

static PVRSRV_ERROR CacheOpQListExecRangeBased(void)
{
	IMG_UINT32 ui32NumOfEntries;
	PVRSRV_ERROR eError = PVRSRV_OK;
	IMG_UINT32 ui32WriteCounter = ~0;
	IMG_BOOL bUsedGlobalFlush = IMG_FALSE;
	CACHEOP_WORK_ITEM *psCacheOpWorkItem = NULL;
#if defined(CACHEOP_DEBUG)
	IMG_UINT64 uiTimeNow = 0;
#endif

	/* Take a snapshot of the current count of deferred entries at this junction */
	ui32NumOfEntries = CacheOpIdxSpan(&gsCwq.hWriteCounter, &gsCwq.hReadCounter);
	while (ui32NumOfEntries)
	{
		if (! OSAtomicRead(&gsCwq.hReadCounter))
		{
			/* Normally, the read-counter will trail the write counter until the write
			   counter wraps-round to zero. Under this condition we (re)calculate as the
			   read-counter too is wrapping around at this point */
			ui32NumOfEntries = CacheOpIdxSpan(&gsCwq.hWriteCounter, &gsCwq.hReadCounter);
		}
#if defined(CACHEOP_DEBUG)
		/* Something's gone horribly wrong if these 2 counters are identical at this point */
		PVR_ASSERT(OSAtomicRead(&gsCwq.hWriteCounter) != OSAtomicRead(&gsCwq.hReadCounter));
#endif

		/* Select the next pending deferred work-item for RBF cache maintenance */
		psCacheOpWorkItem = &gsCwq.asWorkItems[CacheOpIdxNext(&gsCwq.hReadCounter)];
		CacheOpQItemReadCheck(psCacheOpWorkItem);
#if defined(CACHEOP_DEBUG)
		/* The time waiting in the queue to be serviced */
		psCacheOpWorkItem->ui64DequeuedTime = OSClockns64();
#endif

		/* The following CacheOpPMRExec() could trigger a GF, so we (re)read this
		   counter just in case so that we know all such pending CacheOp(s) that will
		   benefit from this soon-to-be-executed GF */
		ui32WriteCounter = CacheOpConfigSupports(CACHEOP_CONFIG_KGF) ?
								OSAtomicRead(&gsCwq.hWriteCounter) : ui32WriteCounter;

		eError = CacheOpPMRExec(psCacheOpWorkItem->psPMR,
								NULL, /* No UM virtual address */
								psCacheOpWorkItem->uiOffset,
								psCacheOpWorkItem->uiSize,
								psCacheOpWorkItem->uiCacheOp,
								psCacheOpWorkItem->ui32GFSeqNum,
								IMG_TRUE, /* PMR is pre-validated */
								&bUsedGlobalFlush);
		if (eError != PVRSRV_OK)
		{
#if defined(CACHEOP_DEBUG)
			PVR_LOG(("Deferred CacheOpPMRExec failed: PID:%d PMR:%p Offset:%" IMG_UINT64_FMTSPECX " Size:%" IMG_UINT64_FMTSPECX " CacheOp:%d, error: %d",
					(IMG_UINT32)psCacheOpWorkItem->pid,
#else
			PVR_LOG(("Deferred CacheOpPMRExec failed: PMR:%p Offset: %" IMG_UINT64_FMTSPECX "Size:%" IMG_UINT64_FMTSPECX " CacheOp:%d, error: %d",
#endif
					psCacheOpWorkItem->psPMR,
					psCacheOpWorkItem->uiOffset,
					psCacheOpWorkItem->uiSize,
					psCacheOpWorkItem->uiCacheOp,
					eError));
		}
		else if (bUsedGlobalFlush)
		{
#if defined(CACHEOP_DEBUG)
			psCacheOpWorkItem->ui32OpSeqNum = gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0];
#endif
			break;
		}

#if defined(CACHEOP_DEBUG)
		if (psCacheOpWorkItem->uiCacheOp != PVRSRV_CACHE_OP_GLOBAL)
		{
			psCacheOpWorkItem->bRBF = IMG_TRUE;
			psCacheOpWorkItem->ui64ExecuteTime = OSClockns64();
			CacheOpStatsExecLogWrite(psCacheOpWorkItem);
		}
		else
		{
			PVR_ASSERT(!gsCwq.bNoGlobalFlushImpl);
		}
#endif

		/* The currently executed CacheOp item updates gsCwq.hCompletedSeqNum.
		   NOTE: This CacheOp item might be a discard item, if so its seqNum
		   still updates the gsCwq.hCompletedSeqNum */
		OSAtomicWrite(&gsCwq.hCompletedSeqNum, psCacheOpWorkItem->ui32OpSeqNum);
		OSAtomicSubtract(&gsCwq.hDeferredSize, psCacheOpWorkItem->uiSize);

		/* If CacheOp is timeline(d), notify timeline waiters */
		eError = CacheOpTimelineExec(psCacheOpWorkItem);
		PVR_LOG_IF_ERROR(eError, "CacheOpTimelineExec");

		/* Indicate that this CCB work-item slot is now free for (re)use */
		CacheOpQItemRecycle(psCacheOpWorkItem);
		(void) CacheOpIdxIncrement(&gsCwq.hReadCounter);
		ui32NumOfEntries = ui32NumOfEntries - 1;
	}

	if (bUsedGlobalFlush)
	{
#if defined(CACHEOP_DEBUG)
		uiTimeNow = OSClockns64();
		PVR_ASSERT(OSAtomicRead(&gsCwq.hWriteCounter) != OSAtomicRead(&gsCwq.hReadCounter));
#endif

		/* Snapshot of queued CacheOps before the global cache flush was issued */
		ui32NumOfEntries = ui32WriteCounter - OSAtomicRead(&gsCwq.hReadCounter);
		if (ui32WriteCounter < OSAtomicRead(&gsCwq.hReadCounter))
		{
			/* Branch handles when the write-counter has wrapped-around in value space */
			ui32NumOfEntries = OSAtomicRead(&gsCwq.hReadCounter) - ui32WriteCounter;
			ui32NumOfEntries = CACHEOP_INDICES_MAX - ui32NumOfEntries;
		}

		while (ui32NumOfEntries)
		{
			CacheOpQItemReadCheck(psCacheOpWorkItem);

#if defined(CACHEOP_DEBUG)
			psCacheOpWorkItem->bRBF = IMG_FALSE;
			psCacheOpWorkItem->ui64ExecuteTime = uiTimeNow;
			if (psCacheOpWorkItem->uiCacheOp == PVRSRV_CACHE_OP_GLOBAL)
			{
				PVR_ASSERT(!gsCwq.bNoGlobalFlushImpl);
				psCacheOpWorkItem->pid = OSGetCurrentProcessID();
			}
			CacheOpStatsExecLogWrite(psCacheOpWorkItem);
#endif

			eError = CacheOpTimelineExec(psCacheOpWorkItem);
			PVR_LOG_IF_ERROR(eError, "CacheOpTimelineExec");

			/* Mark index as ready for recycling for next CacheOp */
			CacheOpQItemRecycle(psCacheOpWorkItem);
			(void) CacheOpIdxIncrement(&gsCwq.hReadCounter);
			ui32NumOfEntries = ui32NumOfEntries - 1;
			psCacheOpWorkItem = &gsCwq.asWorkItems[CacheOpIdxNext(&gsCwq.hReadCounter)];
		}
	}

	return eError;
}

static INLINE PVRSRV_ERROR CacheOpQListExec(void)
{
	PVRSRV_ERROR eError;

	if (CacheOpConfigSupports(CACHEOP_CONFIG_KGF) &&
		(!CacheOpConfigSupports(CACHEOP_CONFIG_KRBF)
		 || OSAtomicRead(&gsCwq.hDeferredSize) >= gsCwq.pui32InfoPage[CACHEOP_INFO_KMGFTHRESHLD]))
	{
		eError = CacheOpQListExecGlobal();
		PVR_LOG_IF_ERROR(eError, "CacheOpQListExecGlobal");
	}
	else
	{
		eError = CacheOpQListExecRangeBased();
		PVR_LOG_IF_ERROR(eError, "CacheOpQListExecRangeBased");
	}

	/* Signal any waiting threads blocked on CacheOp fence checks update
	   completed sequence number to last queue work item */
	eError = OSEventObjectSignal(gsCwq.hClientWakeUpEvtObj);
	PVR_LOG_IF_ERROR(eError, "OSEventObjectSignal");

	return eError;
}

static void CacheOpThread(void *pvData)
{
	PVRSRV_DATA *psPVRSRVData = pvData;
	IMG_HANDLE hOSEvent;
	PVRSRV_ERROR eError;

	/* Store the process id (pid) of the CacheOp-up thread */
	gsCwq.uiWorkerThreadPid = OSGetCurrentProcessID();

	/* Open CacheOp thread event object, abort driver if event object open fails */
	eError = OSEventObjectOpen(gsCwq.hThreadWakeUpEvtObj, &hOSEvent);
	PVR_ASSERT(eError == PVRSRV_OK);

	/* While driver is in good state & loaded, perform pending cache maintenance */
	while ((psPVRSRVData->eServicesState == PVRSRV_SERVICES_STATE_OK) && gsCwq.bInit)
	{
		/* Sleep-wait here until when signalled for new queued CacheOp work items */
		eError = OSEventObjectWaitTimeout(hOSEvent, CACHEOP_THREAD_WAIT_TIMEOUT);
		if (! CacheOpIdxSpan(&gsCwq.hWriteCounter, &gsCwq.hReadCounter))
		{
			if (! gsCwq.bInit)
			{
				break;
			}
		}
		else if (! gsCwq.bInit)
		{
			/* Last maintenance before shutdown */
			(void) CacheOpQListExec();
			break;
		}

		/* Drain deferred queue completely before the next event-wait */
		while (CacheOpIdxSpan(&gsCwq.hWriteCounter, &gsCwq.hReadCounter))
		{
			eError = CacheOpQListExec();
			PVR_LOG_IF_ERROR(eError, "CacheOpQListExec");
		}
	}

	eError = OSEventObjectClose(hOSEvent);
	PVR_LOG_IF_ERROR(eError, "OSEventObjectClose");
}

static PVRSRV_ERROR CacheOpBatchExecTimeline(PVRSRV_TIMELINE iTimeline,
											 IMG_BOOL bUsedGlobalFlush,
											 IMG_UINT32 ui32CurrentFenceSeqNum,
											 IMG_UINT32 *pui32NextFenceSeqNum)
{
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32NextIdx;
	CACHEOP_WORK_ITEM sCacheOpWorkItem = {0};
	CACHEOP_WORK_ITEM *psCacheOpWorkItem = NULL;

	eError = CacheOpTimelineBind(&sCacheOpWorkItem, iTimeline);
	PVR_LOGR_IF_ERROR(eError, "CacheOpTimelineBind");

	OSLockAcquire(gsCwq.hDeferredLock);

	/*
	   Check if there is any deferred queueing space available and that nothing is
	   currently queued. This second check is required as Android where timelines
	   are used sets a timeline signalling deadline of 1000ms to signal timelines
	   else complains. So seeing we cannot be sure how long the CacheOp presently
	   in the queue would take we should not send this timeline down the queue as
	   well.
	 */
	ui32NextIdx = CacheOpIdxNext(&gsCwq.hWriteCounter);
	if (!CacheOpIdxSpan(&gsCwq.hWriteCounter, &gsCwq.hReadCounter) &&
		CacheOpIdxRead(&gsCwq.hReadCounter) != ui32NextIdx)
	{
		psCacheOpWorkItem = &gsCwq.asWorkItems[ui32NextIdx];
		CacheOpQItemWriteCheck(psCacheOpWorkItem);

		psCacheOpWorkItem->sSWTimelineObj = sCacheOpWorkItem.sSWTimelineObj;
		psCacheOpWorkItem->iTimeline = sCacheOpWorkItem.iTimeline;
		psCacheOpWorkItem->ui32OpSeqNum = CacheOpGetNextCommonSeqNum();
		psCacheOpWorkItem->uiCacheOp = PVRSRV_CACHE_OP_TIMELINE;
		psCacheOpWorkItem->uiOffset = (IMG_DEVMEM_OFFSET_T)0;
		psCacheOpWorkItem->uiSize = (IMG_DEVMEM_SIZE_T)0;
		psCacheOpWorkItem->ui32GFSeqNum = 0;
		/* Defer timeline using information page PMR */
		psCacheOpWorkItem->psPMR = gsCwq.psInfoPagePMR;
		eError = PMRLockSysPhysAddresses(psCacheOpWorkItem->psPMR);
		PVR_LOGG_IF_ERROR(eError, "PMRLockSysPhysAddresses", e0);
#if defined(CACHEOP_DEBUG)
		psCacheOpWorkItem->pid = OSGetCurrentClientProcessIDKM();
		psCacheOpWorkItem->ui64EnqueuedTime = OSClockns64();
		gsCwq.ui32ServerASync += 1;
		gsCwq.ui32ServerDTL += 1;
#endif
		OSLockRelease(gsCwq.hDeferredLock);
		(void) CacheOpIdxIncrement(&gsCwq.hWriteCounter);

		/* Signal the CacheOp thread to ensure this GF get processed */
		eError = OSEventObjectSignal(gsCwq.hThreadWakeUpEvtObj);
		PVR_LOG_IF_ERROR(eError, "OSEventObjectSignal");
	}
	else
	{
		IMG_BOOL bExecTimeline = IMG_TRUE;
		IMG_UINT32 ui32CompletedOpSeqNum = OSAtomicRead(&gsCwq.hCompletedSeqNum);

		OSLockRelease(gsCwq.hDeferredLock);

		/*
		   This pathway requires careful handling here as the client CacheOp(s) predicated on this
		   timeline might have been broken-up (i.e. batched) into several server requests by client:
		   1 - In the first case, a CacheOp from an earlier batch is still in-flight, so we check if
		   this is the case because even though we might have executed all the CacheOps in this batch
		   synchronously, we cannot be sure that any in-flight CacheOp pending on this client is not
		   predicated on this timeline hence we need to synchronise here for safety by fencing until
		   all in-flight CacheOps are completed. NOTE: On Android, this might cause issues due to
		   timelines notification deadlines so we do not fence (i.e. cannot sleep or wait) here to
		   synchronise, instead nudge services client to retry the request if there is no GF support.
		   2 - In the second case, there is no in-flight CacheOp for this client in which case just
		   continue processing as normal.
		 */
		if (!bUsedGlobalFlush && !CacheOpFenceCheck(ui32CompletedOpSeqNum, ui32CurrentFenceSeqNum))
		{
#if defined(ANDROID)
			bExecTimeline = IMG_TRUE;
			if (CacheOpGlobalFlush() != PVRSRV_OK)
			{
				bExecTimeline = IMG_FALSE;
				eError = PVRSRV_ERROR_RETRY;
			}
#else
			eError = CacheOpFence ((RGXFWIF_DM)0, ui32CurrentFenceSeqNum);
			PVR_LOG_IF_ERROR(eError, "CacheOpFence");

			/* CacheOpFence() might have triggered a GF so we take advantage of it */
			if (gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0] > ui32CurrentFenceSeqNum)
			{
				*pui32NextFenceSeqNum = 0;
			}
#endif
		}

		if (bExecTimeline)
		{
			/* CacheOp fence requirement met, signal timeline */
			eError = CacheOpTimelineExec(&sCacheOpWorkItem);
			PVR_LOG_IF_ERROR(eError, "CacheOpTimelineExec");
		}
	}

	return eError;
e0:
	if (psCacheOpWorkItem)
	{
		/* Need to ensure we leave this CacheOp QItem in the proper recycled state */
		CacheOpQItemRecycle(psCacheOpWorkItem);
		OSLockRelease(gsCwq.hDeferredLock);
	}

	return eError;
}

static PVRSRV_ERROR CacheOpBatchExecRangeBased(PMR **ppsPMR,
											IMG_CPU_VIRTADDR *pvAddress,
											IMG_DEVMEM_OFFSET_T *puiOffset,
											IMG_DEVMEM_SIZE_T *puiSize,
											PVRSRV_CACHE_OP *puiCacheOp,
											IMG_UINT32 ui32NumCacheOps,
											PVRSRV_TIMELINE uiTimeline,
											IMG_UINT32 ui32GlobalFlushSeqNum,
											IMG_UINT32 uiCurrentFenceSeqNum,
											IMG_UINT32 *pui32NextFenceSeqNum)
{
	IMG_UINT32 ui32Idx;
	IMG_UINT32 ui32NextIdx;
	IMG_BOOL bBatchHasTimeline;
	IMG_BOOL bCacheOpConfigKDF;
	IMG_BOOL bCacheOpConfigKRBF;
	IMG_DEVMEM_SIZE_T uiLogicalSize;
	PVRSRV_ERROR eError = PVRSRV_OK;
	IMG_BOOL bUseGlobalFlush = IMG_FALSE;
	CACHEOP_WORK_ITEM *psCacheOpWorkItem = NULL;
#if defined(CACHEOP_DEBUG)
	CACHEOP_WORK_ITEM sCacheOpWorkItem = {0};
	IMG_UINT32 ui32OpSeqNum = CacheOpGetNextCommonSeqNum();
	sCacheOpWorkItem.pid = OSGetCurrentClientProcessIDKM();
#endif

	/* Check if batch has an associated timeline update */
	bBatchHasTimeline = puiCacheOp[ui32NumCacheOps-1] & PVRSRV_CACHE_OP_TIMELINE;
	puiCacheOp[ui32NumCacheOps-1] &= ~(PVRSRV_CACHE_OP_GLOBAL | PVRSRV_CACHE_OP_TIMELINE);

	/* Check if config. supports kernel deferring of cacheops */
	bCacheOpConfigKDF = CacheOpConfigSupports(CACHEOP_CONFIG_KDF);
	bCacheOpConfigKRBF = CacheOpConfigSupports(CACHEOP_CONFIG_KRBF);

	/*
	   Client expects the next fence seqNum to be zero unless the server has deferred
	   at least one CacheOp in the submitted queue in which case the server informs
	   the client of the last CacheOp seqNum deferred in this batch.
	*/
	for (*pui32NextFenceSeqNum = 0, ui32Idx = 0; ui32Idx < ui32NumCacheOps; ui32Idx++)
	{
		if (bCacheOpConfigKDF)
		{
			/* Check if there is deferred queueing space available */
			ui32NextIdx = CacheOpIdxNext(&gsCwq.hWriteCounter);
			if (ui32NextIdx != CacheOpIdxRead(&gsCwq.hReadCounter))
			{
				psCacheOpWorkItem = &gsCwq.asWorkItems[ui32NextIdx];
			}
		}

		/*
		   Normally, we would like to defer client CacheOp(s) but we may not always be in a
		   position or is necessary to do so based on the following reasons:
		   0 - There is currently no queueing space left to enqueue this CacheOp, this might
		       imply the system is queueing more requests than can be consumed by the CacheOp
		       thread in time.
		   1 - Batch has timeline, action this now due to Android timeline signaling deadlines.
		   2 - Configuration does not support deferring of cache maintenance operations so we
		       execute the batch synchronously/immediately.
		   3 - CacheOp has an INVALIDATE, as this is used to transfer device memory buffer
		       ownership back to the processor, we cannot defer it so action it immediately.
		   4 - CacheOp size too small (single OS page size) to warrant overhead of deferment,
		       this will not be considered if KRBF is not present, as it implies defer all.
		   5 - CacheOp size OK for deferment, but a client virtual address is supplied so we
		       might has well just take advantage of said VA & flush immediately in UM context.
		   6 - Prevent DoS attack if a malicious client queues something very large, say 1GiB
		       and the processor cache ISA does not have a global flush implementation. Here
			   we upper bound this threshold to PVR_DIRTY_BYTES_FLUSH_THRESHOLD.
		   7 - Ensure QoS (load balancing) by not over-loading queue with too much requests,
		       here the (pseudo) alternate queue is the user context so we execute directly
		       on it if the processor cache ISA does not have a global flush implementation.
		*/
		if (!psCacheOpWorkItem  ||
			bBatchHasTimeline   ||
			!bCacheOpConfigKDF  ||
			puiCacheOp[ui32Idx] & PVRSRV_CACHE_OP_INVALIDATE ||
			(bCacheOpConfigKRBF && puiSize[ui32Idx] <= (IMG_DEVMEM_SIZE_T)gsCwq.uiPageSize) ||
			(pvAddress[ui32Idx] && puiSize[ui32Idx] < (IMG_DEVMEM_SIZE_T)gsCwq.pui32InfoPage[CACHEOP_INFO_KMDFTHRESHLD]) ||
			(gsCwq.bNoGlobalFlushImpl && puiSize[ui32Idx] >= (IMG_DEVMEM_SIZE_T)(gsCwq.pui32InfoPage[CACHEOP_INFO_KMDFTHRESHLD] << 2)) ||
			(gsCwq.bNoGlobalFlushImpl && OSAtomicRead(&gsCwq.hDeferredSize) >= gsCwq.pui32InfoPage[CACHEOP_INFO_KMDFTHRESHLD] << CACHEOP_INDICES_LOG2_SIZE))
		{
			/* When the CacheOp thread not keeping up, trash d-cache */
			bUseGlobalFlush = !psCacheOpWorkItem && bCacheOpConfigKDF ? IMG_TRUE : IMG_FALSE;
#if defined(CACHEOP_DEBUG)
			sCacheOpWorkItem.ui64EnqueuedTime = OSClockns64();
			gsCwq.ui32ServerSync += 1;
#endif
			psCacheOpWorkItem = NULL;

			eError = CacheOpPMRExec(ppsPMR[ui32Idx],
									pvAddress[ui32Idx],
									puiOffset[ui32Idx],
									puiSize[ui32Idx],
									puiCacheOp[ui32Idx],
									ui32GlobalFlushSeqNum,
									IMG_FALSE,
									&bUseGlobalFlush);
			PVR_LOGG_IF_ERROR(eError, "CacheOpExecPMR", e0);

#if defined(CACHEOP_DEBUG)
			sCacheOpWorkItem.ui64ExecuteTime = OSClockns64();
			sCacheOpWorkItem.bRBF = !bUseGlobalFlush;
			sCacheOpWorkItem.ui32OpSeqNum = bUseGlobalFlush ?
				gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0] : ui32OpSeqNum;
			sCacheOpWorkItem.psPMR = ppsPMR[ui32Idx];
			sCacheOpWorkItem.uiSize = puiSize[ui32Idx];
			sCacheOpWorkItem.uiOffset = puiOffset[ui32Idx];
			sCacheOpWorkItem.uiCacheOp = puiCacheOp[ui32Idx];
			CacheOpStatsExecLogWrite(&sCacheOpWorkItem);
#endif

			if (bUseGlobalFlush) break;
			continue;
		}

		/* Need to validate request parameters here before enqueing */
		eError = PMR_LogicalSize(ppsPMR[ui32Idx], &uiLogicalSize);
		PVR_LOGG_IF_ERROR(eError, "PMR_LogicalSize", e0);
		eError = PVRSRV_ERROR_DEVICEMEM_OUT_OF_RANGE;
		PVR_LOGG_IF_FALSE(((puiOffset[ui32Idx]+puiSize[ui32Idx]) <= uiLogicalSize), CACHEOP_DEVMEM_OOR_ERROR_STRING, e0);
		eError = PVRSRV_OK;

		/* For safety, take reference here in user context */
		eError = PMRLockSysPhysAddresses(ppsPMR[ui32Idx]);
		PVR_LOGG_IF_ERROR(eError, "PMRLockSysPhysAddresses", e0);

		OSLockAcquire(gsCwq.hDeferredLock);

		/* Select next item off the queue to defer with */
		ui32NextIdx = CacheOpIdxNext(&gsCwq.hWriteCounter);
		if (ui32NextIdx != CacheOpIdxRead(&gsCwq.hReadCounter))
		{
			psCacheOpWorkItem = &gsCwq.asWorkItems[ui32NextIdx];
			CacheOpQItemWriteCheck(psCacheOpWorkItem);
		}
		else
		{
			/* Retry, disable KDF for this batch */
			OSLockRelease(gsCwq.hDeferredLock);
			bCacheOpConfigKDF = IMG_FALSE;
			psCacheOpWorkItem = NULL;
			ui32Idx = ui32Idx - 1;
			continue;
		}

		/* Timeline need to be looked-up (i.e. bind) in the user context
		   before deferring into the CacheOp thread kernel context */
		eError = CacheOpTimelineBind(psCacheOpWorkItem, PVRSRV_NO_UPDATE_TIMELINE_REQUIRED);
		PVR_LOGG_IF_ERROR(eError, "CacheOpTimelineBind", e1);

		/* Prepare & enqueue next deferred work item for CacheOp thread */
		psCacheOpWorkItem->ui32OpSeqNum = CacheOpGetNextCommonSeqNum();
		*pui32NextFenceSeqNum = psCacheOpWorkItem->ui32OpSeqNum;
		psCacheOpWorkItem->ui32GFSeqNum = ui32GlobalFlushSeqNum;
		psCacheOpWorkItem->uiCacheOp = puiCacheOp[ui32Idx];
		psCacheOpWorkItem->uiOffset = puiOffset[ui32Idx];
		psCacheOpWorkItem->uiSize = puiSize[ui32Idx];
		psCacheOpWorkItem->psPMR = ppsPMR[ui32Idx];
#if defined(CACHEOP_DEBUG)
		psCacheOpWorkItem->ui64EnqueuedTime = OSClockns64();
		psCacheOpWorkItem->pid = sCacheOpWorkItem.pid;
		psCacheOpWorkItem->bDeferred = IMG_TRUE;
		psCacheOpWorkItem->bKMReq = IMG_FALSE;
		psCacheOpWorkItem->bUMF = IMG_FALSE;
		gsCwq.ui32ServerASync += 1;
#endif

		OSLockRelease(gsCwq.hDeferredLock);

		/* Increment deferred size & mark index ready for cache maintenance */
		OSAtomicAdd(&gsCwq.hDeferredSize, (IMG_UINT32)puiSize[ui32Idx]);
		(void) CacheOpIdxIncrement(&gsCwq.hWriteCounter);
		psCacheOpWorkItem = NULL;
	}

	/* Signal the CacheOp thread to ensure these items get processed */
	eError = OSEventObjectSignal(gsCwq.hThreadWakeUpEvtObj);
	PVR_LOG_IF_ERROR(eError, "OSEventObjectSignal");

	if (bUseGlobalFlush)
	{
#if defined(CACHEOP_DEBUG)
		/* GF was logged already in the loop above, so rest if any are discards */
		sCacheOpWorkItem.ui64ExecuteTime = sCacheOpWorkItem.ui64EnqueuedTime;
		sCacheOpWorkItem.pid = OSGetCurrentClientProcessIDKM();
		while (++ui32Idx < ui32NumCacheOps)
		{
			sCacheOpWorkItem.psPMR = ppsPMR[ui32Idx];
			sCacheOpWorkItem.uiSize = puiSize[ui32Idx];
			sCacheOpWorkItem.uiOffset = puiOffset[ui32Idx];
			sCacheOpWorkItem.uiCacheOp = puiCacheOp[ui32Idx];
			CacheOpStatsExecLogWrite(&sCacheOpWorkItem);
			gsCwq.ui32KMDiscards += 1;
		}
#endif

		/* No next UM fence seqNum */
		*pui32NextFenceSeqNum = 0;
	}

e1:
	if (psCacheOpWorkItem)
	{
		/* Need to ensure we leave this CacheOp QItem in the proper recycled state */
		CacheOpQItemRecycle(psCacheOpWorkItem);
		OSLockRelease(gsCwq.hDeferredLock);
	}
e0:
	if (bBatchHasTimeline)
	{
		PVRSRV_ERROR eError2;
		eError2 = CacheOpBatchExecTimeline(uiTimeline, bUseGlobalFlush, uiCurrentFenceSeqNum, pui32NextFenceSeqNum);
		eError = (eError2 == PVRSRV_ERROR_RETRY) ? eError2 : eError;
	}

	return eError;
}

static PVRSRV_ERROR CacheOpBatchExecGlobal(PMR **ppsPMR,
									IMG_CPU_VIRTADDR *pvAddress,
									IMG_DEVMEM_OFFSET_T *puiOffset,
									IMG_DEVMEM_SIZE_T *puiSize,
									PVRSRV_CACHE_OP *puiCacheOp,
									IMG_UINT32 ui32NumCacheOps,
									PVRSRV_TIMELINE uiTimeline,
									IMG_UINT32 ui32GlobalFlushSeqNum,
									IMG_UINT32 uiCurrentFenceSeqNum,
									IMG_UINT32 *pui32NextFenceSeqNum)
{
	IMG_UINT32 ui32Idx;
	IMG_BOOL bBatchHasTimeline;
	PVRSRV_ERROR eError = PVRSRV_OK;
	IMG_BOOL bUseGlobalFlush = IMG_FALSE;
	CACHEOP_WORK_ITEM *psCacheOpWorkItem = NULL;
#if	defined(CACHEOP_DEBUG)
	IMG_DEVMEM_SIZE_T uiTotalSize = 0;
	CACHEOP_WORK_ITEM sCacheOpWorkItem = {0};
	sCacheOpWorkItem.pid = OSGetCurrentClientProcessIDKM();
#endif
#if !defined(CACHEFLUSH_ISA_SUPPORTS_GLOBAL_FLUSH)
	PVR_LOGR_IF_ERROR(PVRSRV_ERROR_NOT_SUPPORTED, CACHEOP_NO_GFLUSH_ERROR_STRING);
#endif
	PVR_UNREFERENCED_PARAMETER(pvAddress);

	/* Check if batch has an associated timeline update request */
	bBatchHasTimeline = puiCacheOp[ui32NumCacheOps-1] & PVRSRV_CACHE_OP_TIMELINE;
	puiCacheOp[ui32NumCacheOps-1] &= ~(PVRSRV_CACHE_OP_GLOBAL | PVRSRV_CACHE_OP_TIMELINE);

	/* Skip operation if an else-when GF has occurred in the interim time */
	if (gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0] > ui32GlobalFlushSeqNum)
	{
#if	defined(CACHEOP_DEBUG)
		sCacheOpWorkItem.ui32OpSeqNum = ui32GlobalFlushSeqNum;
#endif
		bUseGlobalFlush = IMG_TRUE;
		*pui32NextFenceSeqNum = 0;
		goto exec_timeline;
	}

	/* Here we need to check that client batch does not contain an INVALIDATE CacheOp */
	for (*pui32NextFenceSeqNum = 0, ui32Idx = 0; ui32Idx < ui32NumCacheOps; ui32Idx++)
	{
#if	defined(CACHEOP_DEBUG)
		IMG_DEVMEM_SIZE_T uiLogicalSize;
		uiTotalSize += puiSize[ui32Idx];
		/* There is no need to validate request parameters as we are about
		   to issue a GF but this might lead to issues being reproducible
		   in one config but not the other, so valid under debug */
		eError = PMR_LogicalSize(ppsPMR[ui32Idx], &uiLogicalSize);
		PVR_LOGG_IF_ERROR(eError, "PMR_LogicalSize", e0);
		eError = PVRSRV_ERROR_DEVICEMEM_OUT_OF_RANGE;
		PVR_LOGG_IF_FALSE(((puiOffset[ui32Idx]+puiSize[ui32Idx]) <= uiLogicalSize), CACHEOP_DEVMEM_OOR_ERROR_STRING, e0);
		eError = PVRSRV_OK;
#endif
		if (puiCacheOp[ui32Idx] & PVRSRV_CACHE_OP_INVALIDATE)
		{
			/* Invalidates cannot be deferred */
			bUseGlobalFlush = IMG_TRUE;
#if	!defined(CACHEOP_DEBUG)
			break;
#endif
		}
	}

	OSLockAcquire(gsCwq.hDeferredLock);

	/*
	   Normally, we would like to defer client CacheOp(s) but we may not always be in a
	   position to do so based on the following reasons:
	   0 - Batch has an INVALIDATE, as this is used to transfer device memory buffer
	       ownership back to the processor, we cannot defer it so action it immediately.
	   1 - Configuration does not support deferring of cache maintenance operations so
		   we execute synchronously/immediately.
	   2 - There is currently no queueing space left to enqueue this CacheOp, this might
	       imply the system is queueing more requests that can be consumed by the CacheOp
	       thread in time.
	   3 - Batch has a timeline and there is currently something queued, we cannot defer
	       because currently queued operation(s) might take quite a while to action which
	       might cause a timeline deadline timeout.
	*/
	if (bUseGlobalFlush ||
		!CacheOpConfigSupports(CACHEOP_CONFIG_KDF) ||
		CacheOpIdxNext(&gsCwq.hWriteCounter) == CacheOpIdxRead(&gsCwq.hReadCounter) ||
		(bBatchHasTimeline && CacheOpIdxSpan(&gsCwq.hWriteCounter, &gsCwq.hReadCounter)))

	{
		OSLockRelease(gsCwq.hDeferredLock);
#if	defined(CACHEOP_DEBUG)
		sCacheOpWorkItem.ui32OpSeqNum =	CacheOpGetNextCommonSeqNum();
		sCacheOpWorkItem.ui64EnqueuedTime = OSClockns64();
#endif
		eError = CacheOpGlobalFlush();
		PVR_LOGG_IF_ERROR(eError, "CacheOpGlobalFlush", e0);
		bUseGlobalFlush = IMG_TRUE;
#if	defined(CACHEOP_DEBUG)
		sCacheOpWorkItem.ui64ExecuteTime = OSClockns64();
		gsCwq.ui32ServerSync += 1;
#endif
		goto exec_timeline;
	}

	/* Select next item off queue to defer this GF and possibly timeline with */
	psCacheOpWorkItem = &gsCwq.asWorkItems[CacheOpIdxNext(&gsCwq.hWriteCounter)];
	CacheOpQItemWriteCheck(psCacheOpWorkItem);

	/* Defer the GF using information page PMR */
	psCacheOpWorkItem->psPMR = gsCwq.psInfoPagePMR;
	eError = PMRLockSysPhysAddresses(psCacheOpWorkItem->psPMR);
	PVR_LOGG_IF_ERROR(eError, "PMRLockSysPhysAddresses", e0);

	/* Timeline object has to be looked-up here in user context */
	eError = CacheOpTimelineBind(psCacheOpWorkItem, uiTimeline);
	PVR_LOGG_IF_ERROR(eError, "CacheOpTimelineBind", e0);

	/* Prepare & enqueue next deferred work item for CacheOp thread */
	*pui32NextFenceSeqNum = CacheOpGetNextCommonSeqNum();
	psCacheOpWorkItem->ui32OpSeqNum = *pui32NextFenceSeqNum;
	psCacheOpWorkItem->ui32GFSeqNum = ui32GlobalFlushSeqNum;
	psCacheOpWorkItem->uiCacheOp = PVRSRV_CACHE_OP_GLOBAL;
	psCacheOpWorkItem->uiOffset = (IMG_DEVMEM_OFFSET_T)0;
	psCacheOpWorkItem->uiSize = (IMG_DEVMEM_SIZE_T)0;
#if defined(CACHEOP_DEBUG)
	/* Note client pid & queueing time of deferred GF CacheOp */
	psCacheOpWorkItem->ui64EnqueuedTime = OSClockns64();
	psCacheOpWorkItem->pid = sCacheOpWorkItem.pid;
	OSAtomicAdd(&gsCwq.hDeferredSize, uiTotalSize);
	psCacheOpWorkItem->uiSize = uiTotalSize;
	psCacheOpWorkItem->bDeferred = IMG_TRUE;
	psCacheOpWorkItem->bKMReq = IMG_FALSE;
	psCacheOpWorkItem->bUMF = IMG_FALSE;
	/* Client CacheOp is logged using the deferred seqNum */
	sCacheOpWorkItem.ui32OpSeqNum =	*pui32NextFenceSeqNum;
	sCacheOpWorkItem.ui64EnqueuedTime = psCacheOpWorkItem->ui64EnqueuedTime;
	sCacheOpWorkItem.ui64ExecuteTime = psCacheOpWorkItem->ui64EnqueuedTime;
	/* Update the CacheOp statistics */
	gsCwq.ui32ServerASync += 1;
	gsCwq.ui32ServerDGF += 1;
#endif

	OSLockRelease(gsCwq.hDeferredLock);

	/* Mark index ready for cache maintenance */
	(void) CacheOpIdxIncrement(&gsCwq.hWriteCounter);

	/* Signal CacheOp thread to ensure this GF get processed */
	eError = OSEventObjectSignal(gsCwq.hThreadWakeUpEvtObj);
	PVR_LOG_IF_ERROR(eError, "OSEventObjectSignal");

exec_timeline:
	if (bUseGlobalFlush && bBatchHasTimeline)
	{
		eError = CacheOpBatchExecTimeline(uiTimeline, bUseGlobalFlush, uiCurrentFenceSeqNum, pui32NextFenceSeqNum);
	}

#if	defined(CACHEOP_DEBUG)
	for (ui32Idx = 0; ui32Idx < ui32NumCacheOps; ui32Idx++)
	{
		sCacheOpWorkItem.psPMR = ppsPMR[ui32Idx];
		sCacheOpWorkItem.uiSize = puiSize[ui32Idx];
		sCacheOpWorkItem.uiOffset = puiOffset[ui32Idx];
		sCacheOpWorkItem.uiCacheOp = puiCacheOp[ui32Idx];
		if (bUseGlobalFlush)
		{
			if (sCacheOpWorkItem.ui64ExecuteTime && ui32Idx)
			{
				/* Only first item carries the real execution time, rest are discards */
				sCacheOpWorkItem.ui64EnqueuedTime = sCacheOpWorkItem.ui64ExecuteTime;
			}
			gsCwq.ui32KMDiscards += !sCacheOpWorkItem.ui64ExecuteTime ? 1 : ui32Idx ? 1 : 0;
		}
		CacheOpStatsExecLogWrite(&sCacheOpWorkItem);
	}
#endif

	return eError;
e0:
	if (psCacheOpWorkItem)
	{
		/* Need to ensure we leave this CacheOp QItem in the proper recycled state */
		CacheOpQItemRecycle(psCacheOpWorkItem);
		OSLockRelease(gsCwq.hDeferredLock);
	}

	if (bBatchHasTimeline)
	{
		PVRSRV_ERROR eError2;
		eError2 = CacheOpBatchExecTimeline(uiTimeline, IMG_FALSE, uiCurrentFenceSeqNum, pui32NextFenceSeqNum);
		eError = (eError2 == PVRSRV_ERROR_RETRY) ? eError2 : eError;
	}

	return eError;
}

PVRSRV_ERROR CacheOpExecKM (PPVRSRV_DEVICE_NODE psDevNode,
							void *pvVirtStart,
							void *pvVirtEnd,
							IMG_CPU_PHYADDR sCPUPhysStart,
							IMG_CPU_PHYADDR sCPUPhysEnd,
							PVRSRV_CACHE_OP uiCacheOp)
{
	PVRSRV_ERROR eError = PVRSRV_ERROR_RETRY;
#if	defined(CACHEOP_DEBUG)
	IMG_BOOL bUsedGlobalFlush = IMG_FALSE;
	CACHEOP_WORK_ITEM sCacheOpWorkItem = {0};
	sCacheOpWorkItem.ui64EnqueuedTime = OSClockns64();
#endif

	if (gsCwq.bInit)
	{
		IMG_DEVMEM_SIZE_T uiSize = sCPUPhysEnd.uiAddr - sCPUPhysStart.uiAddr;
		if (uiSize >= (IMG_DEVMEM_SIZE_T)gsCwq.pui32InfoPage[CACHEOP_INFO_KMGFTHRESHLD])
		{
			eError = CacheOpGlobalFlush();
		}
	}

	if (eError == PVRSRV_OK)
	{
#if	defined(CACHEOP_DEBUG)
		bUsedGlobalFlush = IMG_TRUE;
#endif
	}
	else
	{
		switch (uiCacheOp)
		{
			case PVRSRV_CACHE_OP_CLEAN:
				OSCPUCacheCleanRangeKM(psDevNode, pvVirtStart, pvVirtEnd, sCPUPhysStart, sCPUPhysEnd);
				break;
			case PVRSRV_CACHE_OP_INVALIDATE:
				OSCPUCacheInvalidateRangeKM(psDevNode, pvVirtStart, pvVirtEnd, sCPUPhysStart, sCPUPhysEnd);
				break;
			case PVRSRV_CACHE_OP_FLUSH:
				OSCPUCacheFlushRangeKM(psDevNode, pvVirtStart, pvVirtEnd, sCPUPhysStart, sCPUPhysEnd);
				break;
			default:
				PVR_DPF((PVR_DBG_ERROR,	"%s: Invalid cache operation type %d", __FUNCTION__, uiCacheOp));
				break;
		}
		eError = PVRSRV_OK;
	}

#if	defined(CACHEOP_DEBUG)
	if (! CacheOpConfigSupports(CACHEOP_CONFIG_KLOG))
	{
		if (bUsedGlobalFlush)
		{
			/* Undo the accounting for server GF done in CacheOpGlobalFlush() */
			gsCwq.ui32ServerGF -= 1;
		}
	}
	else
	{
		gsCwq.ui32TotalExecOps += 1;
		if (! bUsedGlobalFlush)
		{
			gsCwq.ui32ServerSync += 1;
			gsCwq.ui32ServerRBF +=
				((sCPUPhysEnd.uiAddr - sCPUPhysStart.uiAddr) & ((IMG_DEVMEM_SIZE_T)~(gsCwq.uiLineSize - 1))) >> gsCwq.uiLineShift;
		}
		sCacheOpWorkItem.uiOffset = 0;
		sCacheOpWorkItem.bKMReq = IMG_TRUE;
		sCacheOpWorkItem.uiCacheOp = uiCacheOp;
		sCacheOpWorkItem.bRBF = !bUsedGlobalFlush;
		/* Use information page PMR for logging KM request */
		sCacheOpWorkItem.psPMR = gsCwq.psInfoPagePMR;
		sCacheOpWorkItem.ui64ExecuteTime = OSClockns64();
		sCacheOpWorkItem.pid = OSGetCurrentClientProcessIDKM();
		sCacheOpWorkItem.ui32OpSeqNum = CacheOpGetNextCommonSeqNum();
		sCacheOpWorkItem.uiSize = (sCPUPhysEnd.uiAddr - sCPUPhysStart.uiAddr);
		CacheOpStatsExecLogWrite(&sCacheOpWorkItem);
	}
#endif

	return eError;
}

PVRSRV_ERROR CacheOpExec(PMR *psPMR,
						 IMG_UINT64 uiAddress,
						 IMG_DEVMEM_OFFSET_T uiOffset,
						 IMG_DEVMEM_SIZE_T uiSize,
						 PVRSRV_CACHE_OP uiCacheOp)
{
	PVRSRV_ERROR eError;
	IMG_CPU_VIRTADDR pvAddress = (IMG_CPU_VIRTADDR)(uintptr_t)uiAddress;
	IMG_BOOL bUseGlobalFlush = uiSize >= gsCwq.pui32InfoPage[CACHEOP_INFO_KMGFTHRESHLD];
#if	defined(CACHEOP_DEBUG)
	CACHEOP_WORK_ITEM sCacheOpWorkItem = {0};
	gsCwq.ui32TotalExecOps += 1;
	gsCwq.ui32ServerSync += 1;
	sCacheOpWorkItem.psPMR = psPMR;
	sCacheOpWorkItem.uiSize = uiSize;
	sCacheOpWorkItem.uiOffset = uiOffset;
	sCacheOpWorkItem.uiCacheOp = uiCacheOp;
	sCacheOpWorkItem.pid = OSGetCurrentClientProcessIDKM();
	sCacheOpWorkItem.ui32OpSeqNum = CacheOpGetNextCommonSeqNum();
	sCacheOpWorkItem.ui64EnqueuedTime = OSClockns64();
#endif

	eError = CacheOpPMRExec(psPMR,
							pvAddress,
							uiOffset,
							uiSize,
							uiCacheOp,
							gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0],
							IMG_FALSE,
							&bUseGlobalFlush);
	PVR_LOGG_IF_ERROR(eError, "CacheOpPMRExec", e0);

#if	defined(CACHEOP_DEBUG)
	sCacheOpWorkItem.bRBF = !bUseGlobalFlush;
	sCacheOpWorkItem.ui64ExecuteTime = OSClockns64();
	CacheOpStatsExecLogWrite(&sCacheOpWorkItem);
#endif

e0:
	return eError;
}

PVRSRV_ERROR CacheOpQueue (IMG_UINT32 ui32NumCacheOps,
						   PMR **ppsPMR,
						   IMG_UINT64 *puiAddress,
						   IMG_DEVMEM_OFFSET_T *puiOffset,
						   IMG_DEVMEM_SIZE_T *puiSize,
						   PVRSRV_CACHE_OP *puiCacheOp,
						   IMG_UINT32 ui32OpTimeline,
						   IMG_UINT32 ui32ClientGFSeqNum,
						   IMG_UINT32 uiCurrentFenceSeqNum,
						   IMG_UINT32 *pui32NextFenceSeqNum)
{
	PVRSRV_ERROR eError;
	PVRSRV_TIMELINE uiTimeline = (PVRSRV_TIMELINE)ui32OpTimeline;
	IMG_CPU_VIRTADDR *pvAddress = (IMG_CPU_VIRTADDR*)(uintptr_t)puiAddress;
#if !defined(CACHEFLUSH_ISA_SUPPORTS_GLOBAL_FLUSH)
	PVR_ASSERT(ui32ClientGFSeqNum == 0);
#endif
#if defined(CACHEOP_DEBUG)
	gsCwq.ui32TotalExecOps += ui32NumCacheOps;
#endif

	if (! gsCwq.bInit)
	{
		PVR_LOG(("CacheOp framework not initialised, failing request"));
		return PVRSRV_ERROR_NOT_INITIALISED;
	}
	else if (! ui32NumCacheOps)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	/* Ensure any single timeline CacheOp request is processed immediately */
	else if (ui32NumCacheOps == 1 && puiCacheOp[0] == PVRSRV_CACHE_OP_TIMELINE)
	{
		eError = CacheOpBatchExecTimeline(uiTimeline, IMG_TRUE, uiCurrentFenceSeqNum, pui32NextFenceSeqNum);
	}
	/* Services client explicitly requested a GF or config is GF only (i.e. no KRBF support), this takes priority */
	else if (CacheOpConfigSupports(CACHEOP_CONFIG_KGF) &&
			 ((puiCacheOp[ui32NumCacheOps-1] & PVRSRV_CACHE_OP_GLOBAL) || !CacheOpConfigSupports(CACHEOP_CONFIG_KRBF)))
	{
		eError =
			CacheOpBatchExecGlobal(ppsPMR,
								   pvAddress,
								   puiOffset,
								   puiSize,
								   puiCacheOp,
								   ui32NumCacheOps,
								   uiTimeline,
								   ui32ClientGFSeqNum,
								   uiCurrentFenceSeqNum,
								   pui32NextFenceSeqNum);
	}
	/* This is the default entry for all client requests */
	else
	{
		if (!(gsCwq.eConfig & (CACHEOP_CONFIG_LAST-1)))
		{
			/* default the configuration before execution */
			CacheOpConfigUpdate(CACHEOP_CONFIG_DEFAULT);
		}

		eError =
			CacheOpBatchExecRangeBased(ppsPMR,
									   pvAddress,
									   puiOffset,
									   puiSize,
									   puiCacheOp,
									   ui32NumCacheOps,
									   uiTimeline,
									   ui32ClientGFSeqNum,
									   uiCurrentFenceSeqNum,
									   pui32NextFenceSeqNum);
	}

	return eError;
}

PVRSRV_ERROR CacheOpFence (RGXFWIF_DM eFenceOpType, IMG_UINT32 ui32FenceOpSeqNum)
{
	IMG_HANDLE hOSEvent;
	PVRSRV_ERROR eError2;
	IMG_UINT32 ui32RetryAbort;
	IMG_UINT32 ui32CompletedOpSeqNum;
	PVRSRV_ERROR eError = PVRSRV_OK;
#if defined(CACHEOP_DEBUG)
	IMG_UINT64 uiTimeNow;
	CACHEOP_WORK_ITEM sCacheOpWorkItem = {0};
	sCacheOpWorkItem.pid = OSGetCurrentClientProcessIDKM();
	sCacheOpWorkItem.ui32OpSeqNum = ui32FenceOpSeqNum;
	sCacheOpWorkItem.ui64EnqueuedTime = OSClockns64();
	uiTimeNow = sCacheOpWorkItem.ui64EnqueuedTime;
#if defined(PVR_RI_DEBUG) && defined(DEBUG)
	sCacheOpWorkItem.eFenceOpType = eFenceOpType;
#endif
	sCacheOpWorkItem.uiSize = (uintptr_t) OSAtomicRead(&gsCwq.hCompletedSeqNum);
	sCacheOpWorkItem.uiOffset = (uintptr_t) gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0];
#endif
	PVR_UNREFERENCED_PARAMETER(eFenceOpType);

	/* CacheOp(s) this thread is fencing for has already been satisfied by an
	   else-when GF. Another way of looking at this, if last else-when GF is
	   logically behind or momentarily disabled (zero) then we have to flush
	   the cache */
	if (gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0] > ui32FenceOpSeqNum)
	{
#if defined(CACHEOP_DEBUG)
		sCacheOpWorkItem.uiOffset = (uintptr_t) gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0];
#endif
		goto e0;
	}

	/* If initial fence check fails, then wait-and-retry in loop */
	ui32CompletedOpSeqNum = OSAtomicRead(&gsCwq.hCompletedSeqNum);
	if (CacheOpFenceCheck(ui32CompletedOpSeqNum, ui32FenceOpSeqNum))
	{
#if defined(CACHEOP_DEBUG)
		sCacheOpWorkItem.uiSize = (uintptr_t) ui32CompletedOpSeqNum;
#endif
		goto e0;
	}

	/* Open CacheOp update event object, if event open fails return error */
	eError2 = OSEventObjectOpen(gsCwq.hClientWakeUpEvtObj, &hOSEvent);
	PVR_LOGG_IF_ERROR(eError2, "OSEventObjectOpen", e0);

	/* Linear (i.e. use exponential?) back-off, upper bounds user wait */
	for (ui32RetryAbort = gsCwq.ui32FenceRetryAbort; ;--ui32RetryAbort)
	{
		/* (Re)read completed CacheOp sequence number before waiting */
		ui32CompletedOpSeqNum = OSAtomicRead(&gsCwq.hCompletedSeqNum);
		if (CacheOpFenceCheck(ui32CompletedOpSeqNum, ui32FenceOpSeqNum))
		{
#if defined(CACHEOP_DEBUG)
			sCacheOpWorkItem.uiSize = (uintptr_t) ui32CompletedOpSeqNum;
#endif
			break;
		}

		/*
		   For cache ISA with GF support, the wait(ms) must be set to be around
		   25% GF overhead and as such there is no point waiting longer, we just
		   perform a GF as it means the CacheOp thread is really lagging behind.
		   Lastly, we cannot (or should not) hang the client thread indefinitely
		   so after a certain duration, we just give up. What this duration is,
		   is hard to state but for now we set it to be 1 seconds, which is the
		   product of CACHEOP_FENCE_[WAIT_TIMEOUT * RETRY_ABORT]. We ask the
		   client to retry the operation by exiting with PVRSRV_ERROR_RETRY.
		*/
		(void) OSEventObjectWaitTimeout(hOSEvent, gsCwq.ui32FenceWaitTimeUs);
		if (gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0] > ui32FenceOpSeqNum)
		{
#if defined(CACHEOP_DEBUG)
			sCacheOpWorkItem.uiOffset = (uintptr_t) gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0];
			uiTimeNow = OSClockns64();
#endif
			break;
		}
		else if (CacheOpConfigSupports(CACHEOP_CONFIG_KGF))
		{
			eError2 = CacheOpGlobalFlush();
			PVR_LOG_IF_ERROR(eError2, "CacheOpGlobalFlush");
#if defined(CACHEOP_DEBUG)
			sCacheOpWorkItem.uiCacheOp = PVRSRV_CACHE_OP_GLOBAL;
			sCacheOpWorkItem.uiOffset = (uintptr_t) gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0];
			uiTimeNow = OSClockns64();
#endif
			break;
		}
		else if (! ui32RetryAbort)
		{
#if defined(CACHEOP_DEBUG)
			sCacheOpWorkItem.uiSize = (uintptr_t) OSAtomicRead(&gsCwq.hCompletedSeqNum);
			sCacheOpWorkItem.uiOffset = (uintptr_t) gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0];
			uiTimeNow = OSClockns64();
#endif
			PVR_LOG(("CacheOpFence() event: "CACHEOP_ABORT_FENCE_ERROR_STRING));
			eError = PVRSRV_ERROR_RETRY;
			break;
		}
		else
		{
#if defined(CACHEOP_DEBUG)
			uiTimeNow = OSClockns64();
#endif
		}
	}

	eError2 = OSEventObjectClose(hOSEvent);
	PVR_LOG_IF_ERROR(eError2, "OSEventObjectOpen");

e0:
#if defined(CACHEOP_DEBUG)
	sCacheOpWorkItem.ui64ExecuteTime = uiTimeNow;
	if (ui32FenceOpSeqNum)
	{
		/* Only fence(s) pending on CacheOp(s) contribute towards statistics,
		   here we calculate the rolling approximate average waiting time
		   for these fence(s) */
		IMG_UINT32 ui64EnqueuedTime = sCacheOpWorkItem.ui64EnqueuedTime;
		IMG_UINT32 ui64ExecuteTime = sCacheOpWorkItem.ui64ExecuteTime;
		IMG_UINT32 ui32Time = ui64EnqueuedTime < ui64ExecuteTime ?
									ui64ExecuteTime - ui64EnqueuedTime :
									ui64EnqueuedTime - ui64ExecuteTime;
		ui32Time = DivBy10(DivBy10(DivBy10(ui32Time)));
		gsCwq.ui32TotalFenceOps += 1;
		if (gsCwq.ui32TotalFenceOps > 2)
		{
			gsCwq.ui32AvgFenceTime -= (gsCwq.ui32AvgFenceTime / gsCwq.ui32TotalFenceOps);
			gsCwq.ui32AvgFenceTime += (ui32Time / gsCwq.ui32TotalFenceOps);
		}
		else if (ui32Time)
		{
			gsCwq.ui32AvgFenceTime = (IMG_UINT32)ui32Time;
		}
	}
	CacheOpStatsExecLogWrite(&sCacheOpWorkItem);
#endif

	return eError;
}

PVRSRV_ERROR CacheOpLog (PMR *psPMR,
						 IMG_UINT64 puiAddress,
						 IMG_DEVMEM_OFFSET_T uiOffset,
						 IMG_DEVMEM_SIZE_T uiSize,
						 IMG_UINT64 ui64EnqueuedTimeUs,
						 IMG_UINT64 ui64ExecuteTimeUs,
						 IMG_UINT32 ui32NumRBF,
						 IMG_BOOL bIsDiscard,
						 PVRSRV_CACHE_OP uiCacheOp)
{
#if defined(CACHEOP_DEBUG)
	CACHEOP_WORK_ITEM sCacheOpWorkItem = {0};
	PVR_UNREFERENCED_PARAMETER(puiAddress);

	sCacheOpWorkItem.psPMR = psPMR;
	sCacheOpWorkItem.uiSize = uiSize;
	sCacheOpWorkItem.uiOffset = uiOffset;
	sCacheOpWorkItem.uiCacheOp = uiCacheOp;
	sCacheOpWorkItem.pid = OSGetCurrentClientProcessIDKM();
	sCacheOpWorkItem.ui32OpSeqNum = CacheOpGetNextCommonSeqNum();

	sCacheOpWorkItem.ui64EnqueuedTime = ui64EnqueuedTimeUs;
	sCacheOpWorkItem.ui64ExecuteTime = ui64ExecuteTimeUs;
	sCacheOpWorkItem.bUMF = IMG_TRUE;
	sCacheOpWorkItem.bRBF = bIsDiscard ? IMG_FALSE : IMG_TRUE;
	gsCwq.ui32UMDiscards += bIsDiscard ? 1 : 0;
	gsCwq.ui32ClientRBF += bIsDiscard ? 0 : ui32NumRBF;
	gsCwq.ui32ClientSync += 1;
	gsCwq.ui32TotalExecOps += 1;

	CacheOpStatsExecLogWrite(&sCacheOpWorkItem);
#else
	PVR_UNREFERENCED_PARAMETER(psPMR);
	PVR_UNREFERENCED_PARAMETER(uiSize);
	PVR_UNREFERENCED_PARAMETER(uiOffset);
	PVR_UNREFERENCED_PARAMETER(uiCacheOp);
	PVR_UNREFERENCED_PARAMETER(ui32NumRBF);
	PVR_UNREFERENCED_PARAMETER(puiAddress);
	PVR_UNREFERENCED_PARAMETER(ui64ExecuteTimeUs);
	PVR_UNREFERENCED_PARAMETER(ui64EnqueuedTimeUs);
#endif
	return PVRSRV_OK;
}

PVRSRV_ERROR CacheOpInit2 (void)
{
	PVRSRV_ERROR eError;
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();

	/* Create an event object for pending CacheOp work items */
	eError = OSEventObjectCreate("PVRSRV_CACHEOP_EVENTOBJECT", &gsCwq.hThreadWakeUpEvtObj);
	PVR_LOGG_IF_ERROR(eError, "OSEventObjectCreate", e0);

	/* Create an event object for updating pending fence checks on CacheOp */
	eError = OSEventObjectCreate("PVRSRV_CACHEOP_EVENTOBJECT", &gsCwq.hClientWakeUpEvtObj);
	PVR_LOGG_IF_ERROR(eError, "OSEventObjectCreate", e0);

	/* Appending work-items is not concurrent, lock protects against this */
	eError = OSLockCreate((POS_LOCK*)&gsCwq.hDeferredLock, LOCK_TYPE_PASSIVE);
	PVR_LOGG_IF_ERROR(eError, "OSLockCreate", e0);

	/* Apphint read/write is not concurrent, so lock protects against this */
	eError = OSLockCreate((POS_LOCK*)&gsCwq.hConfigLock, LOCK_TYPE_PASSIVE);
	PVR_LOGG_IF_ERROR(eError, "OSLockCreate", e0);

	/* Determine CPU cache ISA maintenance mechanism available, GF and UMF */
	gsCwq.bNoGlobalFlushImpl = (IMG_BOOL)OSCPUOperation(PVRSRV_CACHE_OP_FLUSH);
	if (! gsCwq.bNoGlobalFlushImpl)
	{
		IMG_UINT64 uiIdx;
		IMG_UINT64 uiTime = 0;
		IMG_UINT64 uiTimeAfter;
		IMG_UINT64 uiTimeBefore;

		for (uiIdx = 0; uiIdx < 4; uiIdx++)
		{
			/* Take average of four GF */
			uiTimeBefore = OSClockns64();
			(void) OSCPUOperation(PVRSRV_CACHE_OP_FLUSH);
			uiTimeAfter = OSClockns64();

			uiTimeBefore = DivBy10(DivBy10(DivBy10(uiTimeBefore)));
			uiTimeAfter = DivBy10(DivBy10(DivBy10(uiTimeAfter)));
			uiTime += uiTimeBefore < uiTimeAfter ?
								uiTimeAfter  - uiTimeBefore :
								uiTimeBefore - uiTimeAfter;
		}

		gsCwq.ui32FenceWaitTimeUs = (IMG_UINT32)(uiTime >> 2);
		gsCwq.ui32FenceRetryAbort = ~0;
	}
	else
	{
		gsCwq.ui32FenceWaitTimeUs = CACHEOP_FENCE_WAIT_TIMEOUT;
		gsCwq.ui32FenceRetryAbort = CACHEOP_FENCE_RETRY_ABORT;
	}
#if defined(CACHEFLUSH_ISA_SUPPORTS_UM_FLUSH)
	gsCwq.bSupportsUMFlush = IMG_TRUE;
#else
	gsCwq.bSupportsUMFlush = IMG_FALSE;
#endif

	gsCwq.psInfoPageMemDesc = psPVRSRVData->psInfoPageMemDesc;
	gsCwq.pui32InfoPage = psPVRSRVData->pui32InfoPage;
	gsCwq.psInfoPagePMR = psPVRSRVData->psInfoPagePMR;

	/* Normally, platforms should use their default configurations, put exceptions here */
#if defined(__i386__) || defined(__x86_64__)
#if !defined(TC_MEMORY_CONFIG)
	CacheOpConfigUpdate(CACHEOP_CONFIG_URBF | CACHEOP_CONFIG_KGF | CACHEOP_CONFIG_KDF);
#else
	CacheOpConfigUpdate(CACHEOP_CONFIG_KGF | CACHEOP_CONFIG_KDF);
#endif
#else /* defined(__x86__) */
	CacheOpConfigUpdate(CACHEOP_CONFIG_DEFAULT);
#endif

	/* Initialise the remaining occupants of the CacheOp information page */
	gsCwq.pui32InfoPage[CACHEOP_INFO_PGSIZE]    = (IMG_UINT32)gsCwq.uiPageSize;
	gsCwq.pui32InfoPage[CACHEOP_INFO_LINESIZE]  = (IMG_UINT32)gsCwq.uiLineSize;
	gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM0] = (IMG_UINT32)0;
	gsCwq.pui32InfoPage[CACHEOP_INFO_GFSEQNUM1] = (IMG_UINT32)0;

	/* Set before spawning thread */
	gsCwq.bInit = IMG_TRUE;

	/* Create a thread which is used to execute the deferred CacheOp(s),
	   these are CacheOp(s) executed by the server on behalf of clients
	   asynchronously. All clients synchronise with the server before
	   submitting any HW operation (i.e. device kicks) to ensure that
	   client device work-load memory is coherent */
	eError = OSThreadCreatePriority(&gsCwq.hWorkerThread,
									"pvr_cacheop",
									CacheOpThread,
									psPVRSRVData,
									OS_THREAD_HIGHEST_PRIORITY);
	PVR_LOGG_IF_ERROR(eError, "OSThreadCreatePriority", e0);

	/* Writing the unsigned integer binary encoding of CACHEOP_CONFIG
	   into this file cycles through avail. configuration(s) */
	gsCwq.pvConfigTune = OSCreateStatisticEntry("cacheop_config",
											NULL,
											CacheOpConfigRead,
											NULL,
											NULL,
											NULL);
	PVR_LOGG_IF_FALSE(gsCwq.pvConfigTune, "OSCreateStatisticEntry", e0);

	/* Register the CacheOp framework (re)configuration handlers */
	PVRSRVAppHintRegisterHandlersUINT32(APPHINT_ID_CacheOpConfig,
										CacheOpConfigQuery,
										CacheOpConfigSet,
										APPHINT_OF_DRIVER_NO_DEVICE,
										NULL);

	return PVRSRV_OK;
e0:
	CacheOpDeInit2();
	return eError;
}

PVRSRV_ERROR CacheOpAcquireInfoPage(PMR **ppsPMR)
{
    PVRSRV_DATA *psData = PVRSRVGetPVRSRVData();

    PVR_LOGR_IF_FALSE(psData->psInfoPageMemDesc != NULL, "invalid MEMDESC"
                      " handle", PVRSRV_ERROR_INVALID_PARAMS);
    PVR_LOGR_IF_FALSE(psData->psInfoPagePMR != NULL, "invalid PMR handle",
                      PVRSRV_ERROR_INVALID_PARAMS);

    /* Copy the PMR import handle back */
    *ppsPMR = psData->psInfoPagePMR;

    return PVRSRV_OK;
}

PVRSRV_ERROR CacheOpReleaseInfoPage(PMR *ppsPMR)
{
    /* Nothing to do here as PMR is singleton */
    PVR_UNREFERENCED_PARAMETER(ppsPMR);
    return PVRSRV_OK;
}

void CacheOpDeInit2 (void)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	gsCwq.bInit = IMG_FALSE;

	if (gsCwq.hThreadWakeUpEvtObj)
	{
		eError = OSEventObjectSignal(gsCwq.hThreadWakeUpEvtObj);
		PVR_LOG_IF_ERROR(eError, "OSEventObjectSignal");
	}

	if (gsCwq.hClientWakeUpEvtObj)
	{
		eError = OSEventObjectSignal(gsCwq.hClientWakeUpEvtObj);
		PVR_LOG_IF_ERROR(eError, "OSEventObjectSignal");
	}

	if (gsCwq.hWorkerThread)
	{
		LOOP_UNTIL_TIMEOUT(OS_THREAD_DESTROY_TIMEOUT_US)
		{
			eError = OSThreadDestroy(gsCwq.hWorkerThread);
			if (PVRSRV_OK == eError)
			{
				gsCwq.hWorkerThread = NULL;
				break;
			}
			OSWaitus(OS_THREAD_DESTROY_TIMEOUT_US/OS_THREAD_DESTROY_RETRY_COUNT);
		} END_LOOP_UNTIL_TIMEOUT();
		PVR_LOG_IF_ERROR(eError, "OSThreadDestroy");
		gsCwq.hWorkerThread = NULL;
	}

	if (gsCwq.hClientWakeUpEvtObj)
	{
		eError = OSEventObjectDestroy(gsCwq.hClientWakeUpEvtObj);
		PVR_LOG_IF_ERROR(eError, "OSEventObjectDestroy");
		gsCwq.hClientWakeUpEvtObj = NULL;
	}

	if (gsCwq.hThreadWakeUpEvtObj)
	{
		eError = OSEventObjectDestroy(gsCwq.hThreadWakeUpEvtObj);
		PVR_LOG_IF_ERROR(eError, "OSEventObjectDestroy");
		gsCwq.hThreadWakeUpEvtObj = NULL;
	}

	if (gsCwq.hConfigLock)
	{
		eError = OSLockDestroy(gsCwq.hConfigLock);
		PVR_LOG_IF_ERROR(eError, "OSLockDestroy");
		gsCwq.hConfigLock = NULL;
	}

	if (gsCwq.hDeferredLock)
	{
		eError = OSLockDestroy(gsCwq.hDeferredLock);
		PVR_LOG_IF_ERROR(eError, "OSLockDestroy");
		gsCwq.hDeferredLock = NULL;
	}

	if (gsCwq.pvConfigTune)
	{
		OSRemoveStatisticEntry(gsCwq.pvConfigTune);
		gsCwq.pvConfigTune = NULL;
	}

	gsCwq.psInfoPageMemDesc = NULL;
	gsCwq.pui32InfoPage = NULL;
	gsCwq.psInfoPagePMR = NULL;
}

PVRSRV_ERROR CacheOpInit (void)
{
	IMG_UINT32 idx;
	PVRSRV_ERROR eError = PVRSRV_OK;

	/* DDK initialisation is anticipated to be performed on the boot
	   processor (little core in big/little systems) though this may
	   not always be the case. If so, the value cached here is the
	   system wide safe (i.e. smallest) L1 d-cache line size value
	   on any/such platforms with mismatched d-cache line sizes */
	gsCwq.uiPageSize = OSGetPageSize();
	gsCwq.uiPageShift = OSGetPageShift();
	gsCwq.uiLineSize = OSCPUCacheAttributeSize(PVR_DCACHE_LINE_SIZE);
	gsCwq.uiLineShift = ExactLog2(gsCwq.uiLineSize);
	PVR_LOGR_IF_FALSE((gsCwq.uiLineSize && gsCwq.uiPageSize && gsCwq.uiPageShift), "", PVRSRV_ERROR_INIT_FAILURE);
	gsCwq.uiCacheOpAddrType = OSCPUCacheOpAddressType();

	/* More information regarding these atomic counters can be found
	   in the CACHEOP_WORK_QUEUE type definition at top of file  */
	OSAtomicWrite(&gsCwq.hCompletedSeqNum, 0);
	OSAtomicWrite(&gsCwq.hCommonSeqNum, 0);
	OSAtomicWrite(&gsCwq.hDeferredSize, 0);
	OSAtomicWrite(&gsCwq.hWriteCounter, 0);
	OSAtomicWrite(&gsCwq.hReadCounter, 0);

	for (idx = 0; idx < CACHEOP_INDICES_MAX; idx++)
	{
		gsCwq.asWorkItems[idx].iTimeline = PVRSRV_NO_UPDATE_TIMELINE_REQUIRED;
		gsCwq.asWorkItems[idx].psPMR = (void *)(uintptr_t)~0;
		gsCwq.asWorkItems[idx].ui32OpSeqNum = (IMG_UINT32)~0;
		gsCwq.asWorkItems[idx].ui32GFSeqNum = (IMG_UINT32)~0;
	}

	/* Lock prevents multiple threads from issuing surplus to requirement GF */
	eError = OSLockCreate((POS_LOCK*)&gsCwq.hGlobalFlushLock, LOCK_TYPE_PASSIVE);
	PVR_LOGG_IF_ERROR(eError, "OSLockCreate", e0);

#if defined(CACHEOP_DEBUG)
	/* debugfs file read-out is not concurrent, so lock protects against this */
	eError = OSLockCreate((POS_LOCK*)&gsCwq.hStatsExecLock, LOCK_TYPE_PASSIVE);
	PVR_LOGG_IF_ERROR(eError, "OSLockCreate", e0);

	gsCwq.i32StatsExecWriteIdx = 0;
	OSCachedMemSet(gsCwq.asStatsExecuted, 0, sizeof(gsCwq.asStatsExecuted));

	/* File captures the most recent subset of CacheOp(s) executed */
	gsCwq.pvStatsEntry = OSCreateStatisticEntry("cacheop_history",
												NULL,
												CacheOpStatsExecLogRead,
												NULL,
												NULL,
												NULL);
	PVR_LOGG_IF_ERROR(eError, "OSCreateStatisticEntry", e0);
#endif

e0:
	return eError;
}

void CacheOpDeInit (void)
{
#if defined(CACHEOP_DEBUG)
	if (gsCwq.hStatsExecLock)
	{
		(void) OSLockDestroy(gsCwq.hStatsExecLock);
		gsCwq.hStatsExecLock = NULL;
	}

	if (gsCwq.pvStatsEntry)
	{
		OSRemoveStatisticEntry(gsCwq.pvStatsEntry);
		gsCwq.pvStatsEntry = NULL;
	}
#endif
	if (gsCwq.hGlobalFlushLock)
	{
		(void) OSLockDestroy(gsCwq.hGlobalFlushLock);
		gsCwq.hGlobalFlushLock = NULL;
	}
}
