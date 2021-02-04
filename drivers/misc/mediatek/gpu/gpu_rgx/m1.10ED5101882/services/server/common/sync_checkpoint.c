/*************************************************************************/ /*!
@File
@Title          Services synchronisation checkpoint interface
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements server side code for services synchronisation
	            interface
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

#include "img_types.h"
#include "allocmem.h"
#include "devicemem.h"
#include "devicemem_pdump.h"
#include "pvr_debug.h"
#include "pvr_notifier.h"
#include "osfunc.h"
#include "dllist.h"
#include "sync.h"
#include "sync_checkpoint_external.h"
#include "sync_checkpoint.h"
#include "sync_checkpoint_internal.h"
#include "sync_checkpoint_internal_fw.h"
#include "sync_checkpoint_init.h"
#include "lock.h"
#include "log2.h"
#include "pvrsrv.h"
#include "pdump_km.h"

#include "pvrsrv_sync_km.h"
#include "rgxhwperf.h"

#if defined(PVRSRV_NEED_PVR_DPF)

/* Enable this to turn on debug relating to the creation and
   resolution of contexts */
#define ENABLE_SYNC_CHECKPOINT_CONTEXT_DEBUG 0

/* Enable this to turn on debug relating to the creation and
   resolution of fences */
#define ENABLE_SYNC_CHECKPOINT_FENCE_DEBUG 0

/* Enable this to turn on debug relating to the sync checkpoint
   allocation and freeing */
#define ENABLE_SYNC_CHECKPOINT_ALLOC_AND_FREE_DEBUG 0

/* Enable this to turn on debug relating to the sync checkpoint
   enqueuing and signalling */
#define ENABLE_SYNC_CHECKPOINT_ENQ_AND_SIGNAL_DEBUG 0

/* Enable this to turn on debug relating to the sync checkpoint pool */
#define ENABLE_SYNC_CHECKPOINT_POOL_DEBUG 0

/* Enable this to turn on debug relating to sync checkpoint UFO
   lookup */
#define ENABLE_SYNC_CHECKPOINT_UFO_DEBUG 0

/* Enable this to turn on sync checkpoint deferred cleanup debug
 * (for syncs we have been told to free but which have some
 * outstanding FW operations remaining (enqueued in CCBs)
 */
#define ENABLE_SYNC_CHECKPOINT_DEFERRED_CLEANUP_DEBUG 0

#else

#define ENABLE_SYNC_CHECKPOINT_CONTEXT_DEBUG 0
#define ENABLE_SYNC_CHECKPOINT_FENCE_DEBUG 0
#define ENABLE_SYNC_CHECKPOINT_ALLOC_AND_FREE_DEBUG 0
#define ENABLE_SYNC_CHECKPOINT_ENQ_AND_SIGNAL_DEBUG 0
#define ENABLE_SYNC_CHECKPOINT_POOL_DEBUG 0
#define ENABLE_SYNC_CHECKPOINT_UFO_DEBUG 0
#define ENABLE_SYNC_CHECKPOINT_DEFERRED_CLEANUP_DEBUG 0

#endif

/* Set the size of the sync checkpoint pool (not used if 0).
 * A pool will be maintained for each sync checkpoint context.
 */
#define SYNC_CHECKPOINT_POOL_SIZE	128

#define SYNC_CHECKPOINT_BLOCK_LIST_CHUNK_SIZE  10
#define LOCAL_SYNC_CHECKPOINT_RESET_VALUE      PVRSRV_SYNC_CHECKPOINT_NOT_SIGNALLED

/*
	This defines the maximum amount of synchronisation memory
	that can be allocated per sync checkpoint context.
	In reality this number is meaningless as we would run out
	of synchronisation memory before we reach this limit, but
	we need to provide a size to the span RA.
*/
#define MAX_SYNC_CHECKPOINT_MEM  (4 * 1024 * 1024)

typedef struct _SYNC_CHECKPOINT_BLOCK_LIST_
{
	IMG_UINT32            ui32BlockCount;            /*!< Number of contexts in the list */
	IMG_UINT32            ui32BlockListSize;         /*!< Size of the array contexts */
	SYNC_CHECKPOINT_BLOCK **papsSyncCheckpointBlock; /*!< Array of sync checkpoint blocks */
} SYNC_CHECKPOINT_BLOCK_LIST;

typedef struct _SYNC_CHECKPOINT_CONTEXT_CTL_
{
	SHARED_DEV_CONNECTION					psDeviceNode;
	PFN_SYNC_CHECKPOINT_FENCE_RESOLVE_FN	pfnFenceResolve;
	PFN_SYNC_CHECKPOINT_FENCE_CREATE_FN		pfnFenceCreate;
	/*
	 *  Used as head of linked-list of sync checkpoints for which
	 *  SyncCheckpointFree() has been called, but have outstanding
	 *  FW operations (enqueued in CCBs)
	 *  This list will be check whenever a SyncCheckpointFree() is
	 *  called, and when SyncCheckpointContextDestroy() is called.
	 */
	DLLIST_NODE								sDeferredCleanupListHead;
	/* Lock to protect the deferred cleanup list */
	POS_LOCK								hDeferredCleanupListLock;

#if (SYNC_CHECKPOINT_POOL_SIZE > 0)
	_SYNC_CHECKPOINT						*psSyncCheckpointPool[SYNC_CHECKPOINT_POOL_SIZE];
	IMG_BOOL								bSyncCheckpointPoolFull;
	IMG_BOOL								bSyncCheckpointPoolValid;
	IMG_UINT32								ui32SyncCheckpointPoolCount;
	IMG_UINT32								ui32SyncCheckpointPoolWp;
	IMG_UINT32								ui32SyncCheckpointPoolRp;
	POS_LOCK								hSyncCheckpointPoolLock;
#endif
} _SYNC_CHECKPOINT_CONTEXT_CTL;

#if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)

/* this is the max number of sync checkpoint records we will search or dump
 * at any time.
 */
#define SYNC_CHECKPOINT_RECORD_LIMIT 20000

#define DECREMENT_WITH_WRAP(value, sz) ((value) ? ((value) - 1) : ((sz) - 1))

struct SYNC_CHECKPOINT_RECORD
{
	PVRSRV_DEVICE_NODE		*psDevNode;
	SYNC_CHECKPOINT_BLOCK	*psSyncCheckpointBlock;	/*!< handle to SYNC_CHECKPOINT_BLOCK */
	IMG_UINT32				ui32SyncOffset; 		/*!< offset to sync in block */
	IMG_UINT32				ui32FwBlockAddr;
	IMG_PID					uiPID;
	IMG_UINT32				ui32UID;
	IMG_UINT64				ui64OSTime;
	DLLIST_NODE				sNode;
	IMG_CHAR				szClassName[SYNC_MAX_CLASS_NAME_LEN];
};
#endif /* defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING) */

static IMG_BOOL gbSyncCheckpointInit = IMG_FALSE;
static PFN_SYNC_CHECKPOINT_FENCE_RESOLVE_FN g_pfnFenceResolve;
static PFN_SYNC_CHECKPOINT_FENCE_CREATE_FN g_pfnFenceCreate;
static PFN_SYNC_CHECKPOINT_FENCE_ROLLBACK_DATA_FN g_pfnFenceDataRollback;
static PFN_SYNC_CHECKPOINT_FENCE_FINALISE_FN g_pfnFenceFinalise;
static PFN_SYNC_CHECKPOINT_NOHW_UPDATE_TIMELINES_FN g_pfnNoHWUpdateTimelines;
static PFN_SYNC_CHECKPOINT_FREE_CHECKPOINT_LIST_MEM_FN g_pfnFreeChkptListMem;

#if (SYNC_CHECKPOINT_POOL_SIZE > 0)
static _SYNC_CHECKPOINT *_GetCheckpointFromPool(_SYNC_CHECKPOINT_CONTEXT *psContext);
static IMG_BOOL _PutCheckpointInPool(_SYNC_CHECKPOINT *psSyncCheckpoint);
static IMG_UINT32 _CleanCheckpointPool(_SYNC_CHECKPOINT_CONTEXT *psContext);
#endif

/* Defined values to indicate status of sync checkpoint, which is
 * stored in the memory of the structure */
#define SYNC_CHECKPOINT_PATTERN_IN_USE 0x1a1aa
#define SYNC_CHECKPOINT_PATTERN_IN_POOL 0x2b2bb
#define SYNC_CHECKPOINT_PATTERN_FREED 0x3c3cc

#if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)
static PVRSRV_ERROR
_SyncCheckpointRecordAdd(PSYNC_CHECKPOINT_RECORD_HANDLE *phRecord,
	                    SYNC_CHECKPOINT_BLOCK *hSyncCheckpointBlock,
	                    IMG_UINT32 ui32FwBlockAddr,
	                    IMG_UINT32 ui32SyncOffset,
	                    IMG_UINT32 ui32UID,
	                    IMG_UINT32 ui32ClassNameSize,
	                    const IMG_CHAR *pszClassName);
static PVRSRV_ERROR
_SyncCheckpointRecordRemove(PSYNC_CHECKPOINT_RECORD_HANDLE hRecord);
static void _SyncCheckpointState(PDLLIST_NODE psNode,
					DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
					void *pvDumpDebugFile);
static void _SyncCheckpointDebugRequest(PVRSRV_DBGREQ_HANDLE hDebugRequestHandle,
					IMG_UINT32 ui32VerbLevel,
					DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
					void *pvDumpDebugFile);
static PVRSRV_ERROR _SyncCheckpointRecordListInit(PVRSRV_DEVICE_NODE *psDevNode);
static void _SyncCheckpointRecordListDeinit(PVRSRV_DEVICE_NODE *psDevNode);
#endif

#if defined(PDUMP)
static PVRSRV_ERROR _SyncCheckpointSignalPDump(_SYNC_CHECKPOINT *psSyncCheckpoint);
static PVRSRV_ERROR _SyncCheckpointErrorPDump(_SYNC_CHECKPOINT *psSyncCheckpoint);
#endif

/* Unique incremental ID assigned to sync checkpoints when allocated */
static IMG_UINT32 g_SyncCheckpointUID;

static void _CheckDeferredCleanupList(_SYNC_CHECKPOINT_CONTEXT *psContext);

/*
	Internal interfaces for management of _SYNC_CHECKPOINT_CONTEXT
*/
static void
_SyncCheckpointContextUnref(_SYNC_CHECKPOINT_CONTEXT *psContext)
{
	if (!OSAtomicRead(&psContext->hRefCount))
	{
		PVR_LOG_ERROR(PVRSRV_ERROR_INVALID_CONTEXT,
		              "_SyncCheckpointContextUnref context already freed");
	}
	else if (0 == OSAtomicDecrement(&psContext->hRefCount))
	{
		/* SyncCheckpointContextDestroy only when no longer referenced */
		OSLockDestroy(psContext->psContextCtl->hDeferredCleanupListLock);
		psContext->psContextCtl->hDeferredCleanupListLock = NULL;
#if (SYNC_CHECKPOINT_POOL_SIZE > 0)
		if (psContext->psContextCtl->ui32SyncCheckpointPoolCount)
		{
			PVR_DPF((PVR_DBG_WARNING,
					"%s called for context<%p> with %d sync checkpoints still in the pool",
					__FUNCTION__,
					(void*)psContext,
					psContext->psContextCtl->ui32SyncCheckpointPoolCount));
		}
		psContext->psContextCtl->bSyncCheckpointPoolValid = IMG_FALSE;
		OSLockDestroy(psContext->psContextCtl->hSyncCheckpointPoolLock);
		psContext->psContextCtl->hSyncCheckpointPoolLock = NULL;
#endif
		OSFreeMem(psContext->psContextCtl);
		RA_Delete(psContext->psSpanRA);
		RA_Delete(psContext->psSubAllocRA);
		OSLockDestroy(psContext->hLock);
		psContext->hLock = NULL;
		OSFreeMem(psContext);
	}
}

static void
_SyncCheckpointContextRef(_SYNC_CHECKPOINT_CONTEXT *psContext)
{
	if (!OSAtomicRead(&psContext->hRefCount))
	{
		PVR_LOG_ERROR(PVRSRV_ERROR_INVALID_CONTEXT,
		              "_SyncCheckpointContextRef context use after free");
	}
	else
	{
		OSAtomicIncrement(&psContext->hRefCount);
	}
}

/*
	Internal interfaces for management of synchronisation block memory
*/
static PVRSRV_ERROR
_AllocSyncCheckpointBlock(_SYNC_CHECKPOINT_CONTEXT *psContext,
						  SYNC_CHECKPOINT_BLOCK    **ppsSyncBlock)
{
	PVRSRV_DEVICE_NODE *psDevNode;
	SYNC_CHECKPOINT_BLOCK *psSyncBlk;
	PVRSRV_ERROR eError;

	psSyncBlk = OSAllocMem(sizeof(*psSyncBlk));
	PVR_LOGG_IF_NOMEM(psSyncBlk, "OSAllocMem", eError, fail_alloc);

	psSyncBlk->psContext = psContext;

	/* Allocate sync checkpoint block */
	psDevNode = psContext->psDevNode;
	if (!psDevNode)
	{
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		PVR_LOG_ERROR(eError, "context device node invalid");
		goto fail_alloc_ufo_block;
	}
	psSyncBlk->psDevNode = psDevNode;

	eError = psDevNode->pfnAllocUFOBlock(psDevNode,
											 &psSyncBlk->hMemDesc,
											 &psSyncBlk->ui32FirmwareAddr,
											 &psSyncBlk->ui32SyncBlockSize);
	if (eError != PVRSRV_OK)
	{
		PVR_LOG_ERROR(eError, "failed to allocate ufo block");
		goto fail_alloc_ufo_block;
	}

	eError = DevmemAcquireCpuVirtAddr(psSyncBlk->hMemDesc,
									  (void **) &psSyncBlk->pui32LinAddr);
	if (eError != PVRSRV_OK)
	{
		PVR_LOG_ERROR(eError, "DevmemAcquireCpuVirtAddr");
		goto fail_devmem_acquire;
	}

	OSAtomicWrite(&psSyncBlk->hRefCount, 1);

	OSLockCreate(&psSyncBlk->hLock, LOCK_TYPE_NONE);

	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS,
						  "Allocated Sync Checkpoint UFO block (FirmwareVAddr = 0x%08x)",
						  psSyncBlk->ui32FirmwareAddr);

	*ppsSyncBlock = psSyncBlk;
	return PVRSRV_OK;

fail_devmem_acquire:
	psDevNode->pfnFreeUFOBlock(psDevNode, psSyncBlk->hMemDesc);
fail_alloc_ufo_block:
	OSFreeMem(psSyncBlk);
fail_alloc:
	return eError;
}

static void
_FreeSyncCheckpointBlock(SYNC_CHECKPOINT_BLOCK *psSyncBlk)
{
	OSLockAcquire(psSyncBlk->hLock);
	if (0 == OSAtomicDecrement(&psSyncBlk->hRefCount))
	{
		PVRSRV_DEVICE_NODE *psDevNode = psSyncBlk->psDevNode;

		DevmemReleaseCpuVirtAddr(psSyncBlk->hMemDesc);
		psDevNode->pfnFreeUFOBlock(psDevNode, psSyncBlk->hMemDesc);
		OSLockRelease(psSyncBlk->hLock);
		OSLockDestroy(psSyncBlk->hLock);
		psSyncBlk->hLock = NULL;
		OSFreeMem(psSyncBlk);
	}
	else
	{
		OSLockRelease(psSyncBlk->hLock);
	}
}

static PVRSRV_ERROR
_SyncCheckpointBlockImport(RA_PERARENA_HANDLE hArena,
                           RA_LENGTH_T uSize,
                           RA_FLAGS_T uFlags,
                           const IMG_CHAR *pszAnnotation,
                           RA_BASE_T *puiBase,
                           RA_LENGTH_T *puiActualSize,
                           RA_PERISPAN_HANDLE *phImport)
{
	_SYNC_CHECKPOINT_CONTEXT *psContext = hArena;
	SYNC_CHECKPOINT_BLOCK *psSyncBlock = NULL;
	RA_LENGTH_T uiSpanSize;
	PVRSRV_ERROR eError;
	PVR_UNREFERENCED_PARAMETER(uFlags);

	PVR_LOG_IF_FALSE((hArena != NULL), "hArena is NULL");

	/* Check we've not be called with an unexpected size */
	PVR_LOG_IF_FALSE((uSize == sizeof(_SYNC_CHECKPOINT_FW_OBJ)),
	                 "uiSize is not the size of _SYNC_CHECKPOINT_FW_OBJ");

	/*
		Ensure the sync checkpoint context doesn't go away while we have sync blocks
		attached to it
	*/
	_SyncCheckpointContextRef(psContext);

	/* Allocate the block of memory */
	eError = _AllocSyncCheckpointBlock(psContext, &psSyncBlock);
	if (eError != PVRSRV_OK)
	{
		goto fail_syncblockalloc;
	}

	/* Allocate a span for it */
	eError = RA_Alloc(psContext->psSpanRA,
					psSyncBlock->ui32SyncBlockSize,
					RA_NO_IMPORT_MULTIPLIER,
					0,
					psSyncBlock->ui32SyncBlockSize,
					pszAnnotation,
					&psSyncBlock->uiSpanBase,
					&uiSpanSize,
					NULL);
	if (eError != PVRSRV_OK)
	{
		goto fail_spanalloc;
	}

	/*
		There is no reason the span RA should return an allocation larger
		then we request
	*/
	PVR_LOG_IF_FALSE((uiSpanSize == psSyncBlock->ui32SyncBlockSize),
	                 "uiSpanSize invalid");

	*puiBase = psSyncBlock->uiSpanBase;
	*puiActualSize = psSyncBlock->ui32SyncBlockSize;
	*phImport = psSyncBlock;
	return PVRSRV_OK;

fail_spanalloc:
	_FreeSyncCheckpointBlock(psSyncBlock);
fail_syncblockalloc:
	_SyncCheckpointContextUnref(psContext);

	return eError;
}

static void
_SyncCheckpointBlockUnimport(RA_PERARENA_HANDLE hArena,
                             RA_BASE_T uiBase,
                             RA_PERISPAN_HANDLE hImport)
{
	_SYNC_CHECKPOINT_CONTEXT *psContext = hArena;
	SYNC_CHECKPOINT_BLOCK   *psSyncBlock = hImport;

	PVR_LOG_IF_FALSE((psContext != NULL), "hArena invalid");
	PVR_LOG_IF_FALSE((psSyncBlock != NULL), "hImport invalid");
	PVR_LOG_IF_FALSE((uiBase == psSyncBlock->uiSpanBase), "uiBase invalid");

	/* Free the span this import is using */
	RA_Free(psContext->psSpanRA, uiBase);

	/* Free the sync checkpoint block */
	_FreeSyncCheckpointBlock(psSyncBlock);

	/*	Drop our reference to the sync checkpoint context */
	_SyncCheckpointContextUnref(psContext);
}

static INLINE IMG_UINT32 _SyncCheckpointGetOffset(_SYNC_CHECKPOINT *psSyncInt)
{
	IMG_UINT64 ui64Temp;

	ui64Temp =  psSyncInt->uiSpanAddr - psSyncInt->psSyncCheckpointBlock->uiSpanBase;
	PVR_ASSERT(ui64Temp<IMG_UINT32_MAX);
	return (IMG_UINT32)ui64Temp;
}

/* Used by SyncCheckpointContextCreate() below */
static INLINE IMG_UINT32 _Log2(IMG_UINT32 ui32Align)
{
	PVR_ASSERT(IsPower2(ui32Align));
	return ExactLog2(ui32Align);
}

/*
	External interfaces
*/

PVRSRV_ERROR
SyncCheckpointRegisterFunctions(PFN_SYNC_CHECKPOINT_FENCE_RESOLVE_FN pfnFenceResolve,
                                PFN_SYNC_CHECKPOINT_FENCE_CREATE_FN pfnFenceCreate,
                                PFN_SYNC_CHECKPOINT_FENCE_ROLLBACK_DATA_FN pfnFenceDataRollback,
                                PFN_SYNC_CHECKPOINT_FENCE_FINALISE_FN pfnFenceFinalise,
                                PFN_SYNC_CHECKPOINT_NOHW_UPDATE_TIMELINES_FN pfnNoHWUpdateTimelines,
                                PFN_SYNC_CHECKPOINT_FREE_CHECKPOINT_LIST_MEM_FN pfnFreeCheckpointListMem)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	g_pfnFenceResolve = pfnFenceResolve;
	g_pfnFenceCreate = pfnFenceCreate;
	g_pfnFenceDataRollback = pfnFenceDataRollback;
	g_pfnFenceFinalise = pfnFenceFinalise;
	g_pfnNoHWUpdateTimelines = pfnNoHWUpdateTimelines;
	g_pfnFreeChkptListMem = pfnFreeCheckpointListMem;

	return eError;
}
PVRSRV_ERROR
SyncCheckpointResolveFence(PSYNC_CHECKPOINT_CONTEXT psSyncCheckpointContext,
                           PVRSRV_FENCE hFence, IMG_UINT32 *pui32NumSyncCheckpoints,
                           PSYNC_CHECKPOINT **papsSyncCheckpoints,
                           IMG_UINT64 *pui64FenceUID)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (!g_pfnFenceResolve)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: ERROR (eError=PVRSRV_ERROR_SYNC_NATIVESYNC_NOT_REGISTERED)",
				__FUNCTION__));
		eError = PVRSRV_ERROR_SYNC_NATIVESYNC_NOT_REGISTERED;
		PVR_LOG_ERROR(eError, "g_pfnFenceResolve is NULL");
		return eError;
	}

	if (papsSyncCheckpoints)
	{
		eError = g_pfnFenceResolve(psSyncCheckpointContext,
		                           hFence,
		                           pui32NumSyncCheckpoints,
		                           papsSyncCheckpoints,
		                           pui64FenceUID);
	}
	else
	{
		eError = PVRSRV_ERROR_INVALID_PARAMS;
	}

	PVR_LOGR_IF_ERROR(eError, "g_pfnFenceResolve");

	if (*pui32NumSyncCheckpoints > MAX_SYNC_CHECKPOINTS_PER_FENCE)
	{
		IMG_UINT32 i;
		PVR_DPF((PVR_DBG_ERROR, "%s: g_pfnFenceResolve() returned too many checkpoints (%u > MAX_SYNC_CHECKPOINTS_PER_FENCE=%u)",
				__func__, *pui32NumSyncCheckpoints, MAX_SYNC_CHECKPOINTS_PER_FENCE));

		/* Free resources after error */
		for (i = 0; i < *pui32NumSyncCheckpoints; i++)
		{
			SyncCheckpointDropRef((*papsSyncCheckpoints)[i]);
		}
		if (*papsSyncCheckpoints)
		{
			SyncCheckpointFreeCheckpointListMem(*papsSyncCheckpoints);
		}

		return PVRSRV_ERROR_INVALID_PARAMS;
	}

#if (ENABLE_SYNC_CHECKPOINT_FENCE_DEBUG == 1)
	{
		IMG_UINT32 ii;

		PVR_DPF((PVR_DBG_WARNING,
				"%s: g_pfnFenceResolve() for fence %d returned the following %d checkpoints:",
				__FUNCTION__,
				hFence,
				*pui32NumSyncCheckpoints));

		for (ii=0; ii<*pui32NumSyncCheckpoints; ii++)
		{
			PSYNC_CHECKPOINT psNextCheckpoint = *(*papsSyncCheckpoints +  ii);
			PVR_DPF((PVR_DBG_WARNING,
					"%s:   *papsSyncCheckpoints[%d]:<%p>",
					__FUNCTION__,
					ii,
					(void*)psNextCheckpoint));
		}
	}
#endif

	return eError;
}

PVRSRV_ERROR
SyncCheckpointCreateFence(PVRSRV_DEVICE_NODE *psDevNode,
                          const IMG_CHAR *pszFenceName,
                          PVRSRV_TIMELINE hTimeline,
                          PSYNC_CHECKPOINT_CONTEXT psSyncCheckpointContext,
                          PVRSRV_FENCE *phNewFence,
                          IMG_UINT64 *puiUpdateFenceUID,
                          void **ppvFenceFinaliseData,
                          PSYNC_CHECKPOINT *psNewSyncCheckpoint,
                          void **ppvTimelineUpdateSyncPrim,
                          IMG_UINT32 *pui32TimelineUpdateValue)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	PVR_UNREFERENCED_PARAMETER(psDevNode);

	if (!g_pfnFenceCreate)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: ERROR (eError=PVRSRV_ERROR_SYNC_NATIVESYNC_NOT_REGISTERED)",
				__FUNCTION__));
		eError = PVRSRV_ERROR_SYNC_NATIVESYNC_NOT_REGISTERED;
		PVR_LOG_ERROR(eError, "g_pfnFenceCreate is NULL");
	}
	else
	{
		eError = g_pfnFenceCreate(pszFenceName,
		                          hTimeline,
		                          psSyncCheckpointContext,
		                          phNewFence,
		                          puiUpdateFenceUID,
		                          ppvFenceFinaliseData,
		                          psNewSyncCheckpoint,
		                          ppvTimelineUpdateSyncPrim,
		                          pui32TimelineUpdateValue);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					"%s failed to create new fence<%p> for timeline<%d> using "
					"sync checkpoint context<%p>, psNewSyncCheckpoint=<%p>, eError=%s",
					 __FUNCTION__,
					 (void*)phNewFence,
					 hTimeline,
					 (void*)psSyncCheckpointContext,
					 (void*)psNewSyncCheckpoint,
					 PVRSRVGetErrorStringKM(eError)));
		}
#if (ENABLE_SYNC_CHECKPOINT_FENCE_DEBUG == 1)
		else
		{
			PVR_DPF((PVR_DBG_WARNING,
					"%s created new fence<%d> for timeline<%d> using "
					"sync checkpoint context<%p>, new sync_checkpoint=<%p>",
					 __FUNCTION__,
					 *phNewFence,
					 hTimeline,
					 (void*)psSyncCheckpointContext,
					 (void*)*psNewSyncCheckpoint));
		}
#endif
	}
	return eError;
}

PVRSRV_ERROR
SyncCheckpointRollbackFenceData(PVRSRV_FENCE hFence, void *pvFinaliseData)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (!g_pfnFenceDataRollback)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: ERROR (eError=PVRSRV_ERROR_SYNC_NATIVESYNC_NOT_REGISTERED)",
				__FUNCTION__));
		eError = PVRSRV_ERROR_SYNC_NATIVESYNC_NOT_REGISTERED;
		PVR_LOG_ERROR(eError, "g_pfnFenceDataRollback is NULL");
	}
	else
	{
#if (ENABLE_SYNC_CHECKPOINT_FENCE_DEBUG == 1)
		PVR_DPF((PVR_DBG_WARNING,
				"%s: called to rollback fence data <%p>",
				__FUNCTION__,
				pvFinaliseData));
#endif
		eError = g_pfnFenceDataRollback(hFence, pvFinaliseData);
		PVR_LOG_IF_ERROR(eError, "g_pfnFenceDataRollback returned error");
	}
	return eError;
}

PVRSRV_ERROR
SyncCheckpointFinaliseFence(PVRSRV_FENCE hFence, void *pvFinaliseData)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (!g_pfnFenceFinalise)
	{
		PVR_DPF((PVR_DBG_WARNING,
				"%s: Warning (eError=PVRSRV_ERROR_SYNC_NATIVESYNC_NOT_REGISTERED) (this is permitted)",
				__FUNCTION__));
		eError = PVRSRV_ERROR_SYNC_NATIVESYNC_NOT_REGISTERED;
	}
	else
	{
#if (ENABLE_SYNC_CHECKPOINT_FENCE_DEBUG == 1)
		PVR_DPF((PVR_DBG_WARNING,
				"%s: called to finalise fence <%d>",
				__FUNCTION__,
				hFence));
#endif
		eError = g_pfnFenceFinalise(hFence, pvFinaliseData);
		PVR_LOG_IF_ERROR(eError, "g_pfnFenceFinalise returned error");
	}
	return eError;
}

void
SyncCheckpointFreeCheckpointListMem(void *pvCheckpointListMem)
{
	if (g_pfnFreeChkptListMem)
	{
		g_pfnFreeChkptListMem(pvCheckpointListMem);
	}
}

PVRSRV_ERROR
SyncCheckpointNoHWUpdateTimelines(void *pvPrivateData)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (!g_pfnNoHWUpdateTimelines)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: ERROR (eError=PVRSRV_ERROR_SYNC_NATIVESYNC_NOT_REGISTERED)",
				__FUNCTION__));
		eError = PVRSRV_ERROR_SYNC_NATIVESYNC_NOT_REGISTERED;
		PVR_LOG_ERROR(eError, "g_pfnNoHWUpdateTimelines is NULL");
	}
	else
	{
		g_pfnNoHWUpdateTimelines(pvPrivateData);
	}
	return eError;

}

PVRSRV_ERROR
SyncCheckpointContextCreate(PPVRSRV_DEVICE_NODE psDevNode,
							PSYNC_CHECKPOINT_CONTEXT *ppsSyncCheckpointContext)
{
	_SYNC_CHECKPOINT_CONTEXT *psContext = NULL;
	_SYNC_CHECKPOINT_CONTEXT_CTL *psContextCtl = NULL;
	PVRSRV_ERROR eError = PVRSRV_OK;

	PVR_LOGR_IF_FALSE((ppsSyncCheckpointContext != NULL),
	                  "ppsSyncCheckpointContext invalid",
	                  PVRSRV_ERROR_INVALID_PARAMS);

	psContext = OSAllocMem(sizeof(*psContext));
	PVR_LOGG_IF_NOMEM(psContext, "OSAllocMem", eError, fail_alloc); /* Sets OOM error code */

	psContextCtl = OSAllocMem(sizeof(*psContextCtl));
	PVR_LOGG_IF_NOMEM(psContextCtl, "OSAllocMem", eError, fail_alloc2); /* Sets OOM error code */

	eError = OSLockCreate(&psContext->hLock, LOCK_TYPE_NONE);
	if (eError != PVRSRV_OK)
	{
		PVR_LOG_ERROR(eError, "SyncCheckpointContextCreate call "
				"to OSLockCreate(context lock) failed");
		goto fail_create_context_lock;
	}

	eError = OSLockCreate(&psContextCtl->hDeferredCleanupListLock, LOCK_TYPE_NONE);
	if (eError != PVRSRV_OK)
	{
		PVR_LOG_ERROR(eError, "SyncCheckpointContextCreate call "
				"to OSLockCreate(deferred cleanup list lock) failed");
		goto fail_create_deferred_cleanup_lock;
	}

#if (SYNC_CHECKPOINT_POOL_SIZE > 0)
	eError = OSLockCreate(&psContextCtl->hSyncCheckpointPoolLock, LOCK_TYPE_NONE);
	if (eError != PVRSRV_OK)
	{
		PVR_LOG_ERROR(eError, "SyncCheckpointContextCreate call "
				"to OSLockCreate(sync checkpoint pool lock) failed");
		goto fail_create_pool_lock;
	}
#endif

	dllist_init(&psContextCtl->sDeferredCleanupListHead);
#if (SYNC_CHECKPOINT_POOL_SIZE > 0)
	psContextCtl->ui32SyncCheckpointPoolCount = 0;
	psContextCtl->ui32SyncCheckpointPoolWp = 0;
	psContextCtl->ui32SyncCheckpointPoolRp = 0;
	psContextCtl->bSyncCheckpointPoolFull = IMG_FALSE;
	psContextCtl->bSyncCheckpointPoolValid = IMG_TRUE;
#endif
	psContext->psDevNode = psDevNode;

	OSSNPrintf(psContext->azName, PVRSRV_SYNC_NAME_LENGTH, "Sync Prim RA-%p", psContext);
	OSSNPrintf(psContext->azSpanName, PVRSRV_SYNC_NAME_LENGTH, "Sync Prim span RA-%p", psContext);

	/*
		Create the RA for sub-allocations of the sync checkpoints

		Note:
		The import size doesn't matter here as the server will pass
		back the blocksize when it does the import which overrides
		what we specify here.
	*/
	psContext->psSubAllocRA = RA_Create(psContext->azName,
										/* Params for imports */
										_Log2(sizeof(IMG_UINT32)),
										RA_LOCKCLASS_2,
										_SyncCheckpointBlockImport,
										_SyncCheckpointBlockUnimport,
										psContext,
										IMG_FALSE);
	if (psContext->psSubAllocRA == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		PVR_LOG_ERROR(eError, "SyncCheckpointContextCreate call to RA_Create(subAlloc) failed");
		goto fail_suballoc;
	}

	/*
		Create the span-management RA

		The RA requires that we work with linear spans. For our use
		here we don't require this behaviour as we're always working
		within offsets of blocks (imports). However, we need to keep
		the RA happy so we create the "span" management RA which
		ensures that all are imports are added to the RA in a linear
		fashion
	*/
	psContext->psSpanRA = RA_Create(psContext->azSpanName,
									/* Params for imports */
									0,
									RA_LOCKCLASS_1,
									NULL,
									NULL,
									NULL,
									IMG_FALSE);
	if (psContext->psSpanRA == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		PVR_LOG_ERROR(eError, "SyncCheckpointContextCreate call to RA_Create(span) failed");
		goto fail_span;
	}

	if (!RA_Add(psContext->psSpanRA, 0, MAX_SYNC_CHECKPOINT_MEM, 0, NULL))
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		PVR_LOG_ERROR(eError, "SyncCheckpointContextCreate call to RA_Add(span) failed");
		goto fail_span_add;
	}

	OSAtomicWrite(&psContext->hRefCount, 1);
	OSAtomicWrite(&psContext->hCheckpointCount, 0);

	psContext->psContextCtl = psContextCtl;

	*ppsSyncCheckpointContext = (PSYNC_CHECKPOINT_CONTEXT)psContext;
#if (ENABLE_SYNC_CHECKPOINT_CONTEXT_DEBUG == 1)
	PVR_DPF((PVR_DBG_WARNING,
			"%s: created psSyncCheckpointContext=<%p>",
			__FUNCTION__,
			(void*)*ppsSyncCheckpointContext));
#endif
	return PVRSRV_OK;

fail_span_add:
	RA_Delete(psContext->psSpanRA);
fail_span:
	RA_Delete(psContext->psSubAllocRA);
fail_suballoc:
#if (SYNC_CHECKPOINT_POOL_SIZE > 0)
	OSLockDestroy(psContextCtl->hSyncCheckpointPoolLock);
	psContextCtl->hSyncCheckpointPoolLock = NULL;
fail_create_pool_lock:
#endif
	OSLockDestroy(psContextCtl->hDeferredCleanupListLock);
	psContextCtl->hDeferredCleanupListLock = NULL;
fail_create_deferred_cleanup_lock:
	OSLockDestroy(psContext->hLock);
	psContext->hLock = NULL;
fail_create_context_lock:
	OSFreeMem(psContextCtl);
fail_alloc2:
	OSFreeMem(psContext);
fail_alloc:
	return eError;
}

/* Poisons and frees the checkpoint and lock.
 * Decrements context refcount. */
static void _FreeSyncCheckpoint(_SYNC_CHECKPOINT *psSyncCheckpoint)
{
	_SYNC_CHECKPOINT_CONTEXT *psContext = psSyncCheckpoint->psSyncCheckpointBlock->psContext;

	psSyncCheckpoint->sCheckpointUFOAddr.ui32Addr = 0;
	psSyncCheckpoint->psSyncCheckpointFwObj = NULL;
	psSyncCheckpoint->ui32ValidationCheck = SYNC_CHECKPOINT_PATTERN_FREED;

	RA_Free(psSyncCheckpoint->psSyncCheckpointBlock->psContext->psSubAllocRA,
			psSyncCheckpoint->uiSpanAddr);
	psSyncCheckpoint->psSyncCheckpointBlock = NULL;

	OSLockDestroy(psSyncCheckpoint->hLock);
	OSFreeMem(psSyncCheckpoint);

	OSAtomicDecrement(&psContext->hCheckpointCount);
}

PVRSRV_ERROR SyncCheckpointContextDestroy(PSYNC_CHECKPOINT_CONTEXT psSyncCheckpointContext)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	_SYNC_CHECKPOINT_CONTEXT *psContext = (_SYNC_CHECKPOINT_CONTEXT*)psSyncCheckpointContext;
	PVRSRV_DEVICE_NODE *psDevNode = (PVRSRV_DEVICE_NODE *)psContext->psDevNode;
	IMG_INT iRf = 0;

	PVR_LOG_IF_FALSE((psSyncCheckpointContext != NULL), "psSyncCheckpointContext invalid");

#if (ENABLE_SYNC_CHECKPOINT_CONTEXT_DEBUG == 1)
	PVR_DPF((PVR_DBG_WARNING,
			"%s: destroying psSyncCheckpointContext=<%p>",
			__FUNCTION__,
			(void*)psSyncCheckpointContext));
#endif

	_CheckDeferredCleanupList(psContext);

#if (SYNC_CHECKPOINT_POOL_SIZE > 0)
	if (psContext->psContextCtl->ui32SyncCheckpointPoolCount > 0)
	{
		IMG_UINT32 ui32NumFreedFromPool = _CleanCheckpointPool(psContext);

#if (ENABLE_SYNC_CHECKPOINT_POOL_DEBUG == 1)
		PVR_DPF((PVR_DBG_WARNING,
				"%s freed %d sync checkpoints that were still in the pool for context<%p>",
				__FUNCTION__,
				ui32NumFreedFromPool,
				(void*)psContext));
#else
		PVR_UNREFERENCED_PARAMETER(ui32NumFreedFromPool);
#endif
	}
#endif

	iRf = OSAtomicRead(&psContext->hCheckpointCount);

	if (iRf != 0)
	{
		/* Note, this is not a permanent error as the caller may retry later */
		PVR_DPF((PVR_DBG_WARNING,
				"%s <%p> attempted with active references (iRf=%d), "
				"may be the result of a race",
				__FUNCTION__,
				(void*)psContext,
				iRf));

		OSLockAcquire(psDevNode->hSyncCheckpointListLock);
		{
			DLLIST_NODE *psNode, *psNext;

			dllist_foreach_node(&psDevNode->sSyncCheckpointSyncsList, psNode, psNext)
			{
				_SYNC_CHECKPOINT *psSyncCheckpoint = IMG_CONTAINER_OF(psNode, _SYNC_CHECKPOINT, sListNode);
				IMG_BOOL bDeferredFree = dllist_node_is_in_list(&psSyncCheckpoint->sDeferredFreeListNode);

				/* Line below avoids build error in release builds (where PVR_DPF is not defined) */
				PVR_UNREFERENCED_PARAMETER(bDeferredFree);
				PVR_DPF((PVR_DBG_WARNING,
						"%s syncCheckpoint<%p> ID=%d, %s, refs=%d, state=%s, enqCount:%d, FWCount:%d %s",
						__FUNCTION__,
						(void*)psSyncCheckpoint,
						psSyncCheckpoint->ui32UID,
						psSyncCheckpoint->azName,
						OSAtomicRead(&psSyncCheckpoint->hRefCount),
						 psSyncCheckpoint->psSyncCheckpointFwObj->ui32State == PVRSRV_SYNC_CHECKPOINT_SIGNALLED ?
								 "PVRSRV_SYNC_CHECKPOINT_SIGNALLED" :
									 psSyncCheckpoint->psSyncCheckpointFwObj->ui32State == PVRSRV_SYNC_CHECKPOINT_NOT_SIGNALLED ?
											 "PVRSRV_SYNC_CHECKPOINT_NOT_SIGNALLED" : "PVRSRV_SYNC_CHECKPOINT_ERRORED",
						 OSAtomicRead(&psSyncCheckpoint->hEnqueuedCCBCount),
						 psSyncCheckpoint->psSyncCheckpointFwObj->ui32FwRefCount,
						 bDeferredFree ? "(deferred free)" : ""));

				eError = PVRSRV_ERROR_UNABLE_TO_DESTROY_CONTEXT;
			}
		}
		OSLockRelease(psDevNode->hSyncCheckpointListLock);
	}
	else
	{
		IMG_INT iRf2 = 0;

		iRf2 = OSAtomicRead(&psContext->hRefCount);
		_SyncCheckpointContextUnref(psContext);
	}

	PVR_LOG_IF_ERROR(eError, "SyncCheckpointContextDestroy returning error");
	return eError;
}

PVRSRV_ERROR
SyncCheckpointAlloc(PSYNC_CHECKPOINT_CONTEXT psSyncContext,
	                PVRSRV_TIMELINE hTimeline,
	                const IMG_CHAR *pszCheckpointName,
	                PSYNC_CHECKPOINT *ppsSyncCheckpoint)
{
	_SYNC_CHECKPOINT *psNewSyncCheckpoint = NULL;
	_SYNC_CHECKPOINT_CONTEXT *psSyncContextInt = (_SYNC_CHECKPOINT_CONTEXT*)psSyncContext;
	PVRSRV_DEVICE_NODE *psDevNode;
	PVRSRV_ERROR eError;

	PVR_LOGR_IF_FALSE((psSyncContext != NULL), "psSyncContext invalid", PVRSRV_ERROR_INVALID_PARAMS);
	PVR_LOGR_IF_FALSE((ppsSyncCheckpoint != NULL), "ppsSyncCheckpoint invalid", PVRSRV_ERROR_INVALID_PARAMS);

	psDevNode = (PVRSRV_DEVICE_NODE *)psSyncContextInt->psDevNode;

#if (SYNC_CHECKPOINT_POOL_SIZE > 0)
#if ((ENABLE_SYNC_CHECKPOINT_POOL_DEBUG == 1) || (ENABLE_SYNC_CHECKPOINT_ALLOC_AND_FREE_DEBUG == 1))
	PVR_DPF((PVR_DBG_WARNING, "%s Entry, Getting checkpoint from pool", __FUNCTION__));
#endif
	psNewSyncCheckpoint = _GetCheckpointFromPool(psSyncContextInt);
	if (!psNewSyncCheckpoint)
	{
#if ((ENABLE_SYNC_CHECKPOINT_POOL_DEBUG == 1) || (ENABLE_SYNC_CHECKPOINT_ALLOC_AND_FREE_DEBUG == 1))
		PVR_DPF((PVR_DBG_WARNING, "%s     checkpoint pool empty - will have to allocate", __FUNCTION__));
#endif
	}
#endif
	/* If pool is empty (or not defined) alloc the new sync checkpoint */
	if (!psNewSyncCheckpoint)
	{
		psNewSyncCheckpoint = OSAllocMem(sizeof(*psNewSyncCheckpoint));
		PVR_LOGG_IF_NOMEM(psNewSyncCheckpoint, "OSAllocMem", eError, fail_alloc); /* Sets OOM error code */

		eError = OSLockCreate(&psNewSyncCheckpoint->hLock, LOCK_TYPE_NONE);

		PVR_LOGG_IF_ERROR(eError, "OSLockCreate", fail_create_checkpoint_lock);

		eError = RA_Alloc(psSyncContextInt->psSubAllocRA,
		                  sizeof(*psNewSyncCheckpoint->psSyncCheckpointFwObj),
		                  RA_NO_IMPORT_MULTIPLIER,
		                  0,
		                  sizeof(IMG_UINT32),
		                  (IMG_CHAR*)pszCheckpointName,
		                  &psNewSyncCheckpoint->uiSpanAddr,
		                  NULL,
		                  (RA_PERISPAN_HANDLE *) &psNewSyncCheckpoint->psSyncCheckpointBlock);
		PVR_LOGG_IF_ERROR(eError, "RA_Alloc", fail_raalloc);

#if (ENABLE_SYNC_CHECKPOINT_ALLOC_AND_FREE_DEBUG == 1)
		PVR_DPF((PVR_DBG_WARNING,
				"%s CALLED RA_Alloc(), psSubAllocRA=<%p>, ui32SpanAddr=0x%llx",
				__FUNCTION__,
				(void*)psSyncContextInt->psSubAllocRA,
				psNewSyncCheckpoint->uiSpanAddr));
#endif
		psNewSyncCheckpoint->hTimeline = hTimeline;
		psNewSyncCheckpoint->psSyncCheckpointFwObj =
				(volatile _SYNC_CHECKPOINT_FW_OBJ*)(psNewSyncCheckpoint->psSyncCheckpointBlock->pui32LinAddr +
						(_SyncCheckpointGetOffset(psNewSyncCheckpoint)/sizeof(IMG_UINT32)));
		OSAtomicIncrement(&psNewSyncCheckpoint->psSyncCheckpointBlock->psContext->hCheckpointCount);
		psNewSyncCheckpoint->ui32ValidationCheck = SYNC_CHECKPOINT_PATTERN_IN_USE;
#if (ENABLE_SYNC_CHECKPOINT_ALLOC_AND_FREE_DEBUG == 1)
		PVR_DPF((PVR_DBG_WARNING, "%s called to allocate new sync checkpoint<%p> for context<%p>", __FUNCTION__, (void*)psNewSyncCheckpoint, (void*)psSyncContext));
		PVR_DPF((PVR_DBG_WARNING, "%s                    psSyncCheckpointFwObj<%p>", __FUNCTION__, (void*)psNewSyncCheckpoint->psSyncCheckpointFwObj));
		PVR_DPF((PVR_DBG_WARNING, "%s                    psSyncCheckpoint FwAddr=0x%x", __FUNCTION__, SyncCheckpointGetFirmwareAddr((PSYNC_CHECKPOINT)psNewSyncCheckpoint)));
		PVR_DPF((PVR_DBG_WARNING, "%s                    pszCheckpointName = %s", __FUNCTION__, pszCheckpointName));
		PVR_DPF((PVR_DBG_WARNING, "%s                    psSyncCheckpoint Timeline=%d", __FUNCTION__, psNewSyncCheckpoint->hTimeline));
#endif
	}

	OSAtomicWrite(&psNewSyncCheckpoint->hRefCount, 1);
	OSAtomicWrite(&psNewSyncCheckpoint->hEnqueuedCCBCount, 0);
	psNewSyncCheckpoint->psSyncCheckpointFwObj->ui32FwRefCount = 0;
	psNewSyncCheckpoint->psSyncCheckpointFwObj->ui32State = PVRSRV_SYNC_CHECKPOINT_NOT_SIGNALLED;
	OSCachedMemSet(&psNewSyncCheckpoint->sDeferredFreeListNode, 0, sizeof(psNewSyncCheckpoint->sDeferredFreeListNode));

	if(pszCheckpointName)
	{
		/* Copy over the checkpoint name annotation */
		OSStringLCopy(psNewSyncCheckpoint->azName, pszCheckpointName, PVRSRV_SYNC_NAME_LENGTH);
	}
	else
	{
		/* No sync checkpoint name annotation */
		psNewSyncCheckpoint->azName[0] = '\0';
	}

	/* Store sync checkpoint FW address in PRGXFWIF_UFO_ADDR struct */
	psNewSyncCheckpoint->sCheckpointUFOAddr.ui32Addr = SyncCheckpointGetFirmwareAddr((PSYNC_CHECKPOINT)psNewSyncCheckpoint);

	/* Assign unique ID to this sync checkpoint */
	psNewSyncCheckpoint->ui32UID = g_SyncCheckpointUID++;

	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS,
						  "Allocated Sync Checkpoint %s (ID:%d, TL:%d, FirmwareVAddr = 0x%08x)",
						  psNewSyncCheckpoint->azName,
						  psNewSyncCheckpoint->ui32UID, psNewSyncCheckpoint->hTimeline,
						  psNewSyncCheckpoint->sCheckpointUFOAddr.ui32Addr);

#if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)
	{
		IMG_CHAR szChkptName[PVRSRV_SYNC_NAME_LENGTH];

		if(pszCheckpointName)
		{
			/* Copy the checkpoint name annotation into a fixed-size array */
			OSStringLCopy(szChkptName, pszCheckpointName, PVRSRV_SYNC_NAME_LENGTH);
		}
		else
		{
			/* No checkpoint name annotation */
			szChkptName[0] = 0;
		}
		/* record this sync */
		eError = _SyncCheckpointRecordAdd(&psNewSyncCheckpoint->hRecord,
		                                 psNewSyncCheckpoint->psSyncCheckpointBlock,
		                                 psNewSyncCheckpoint->psSyncCheckpointBlock->ui32FirmwareAddr,
		                                 _SyncCheckpointGetOffset(psNewSyncCheckpoint),
		                                 psNewSyncCheckpoint->ui32UID,
		                                 OSStringNLength(szChkptName, PVRSRV_SYNC_NAME_LENGTH),
		                                 szChkptName);
		if(eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed to add sync checkpoint record \"%s\" (%s)",
												__func__,
												szChkptName,
												PVRSRVGetErrorStringKM(eError)));
			psNewSyncCheckpoint->hRecord = NULL;
			/* note the error but continue without affecting driver operation */
		}
	}
#else
	PVR_UNREFERENCED_PARAMETER(pszCheckpointName);
#endif /* if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING) */

	/* Add the sync checkpoint to the device list */
	OSLockAcquire(psDevNode->hSyncCheckpointListLock);
	dllist_add_to_head(&psDevNode->sSyncCheckpointSyncsList,
	                   &psNewSyncCheckpoint->sListNode);
	OSLockRelease(psDevNode->hSyncCheckpointListLock);

	*ppsSyncCheckpoint = (PSYNC_CHECKPOINT)psNewSyncCheckpoint;

#if (ENABLE_SYNC_CHECKPOINT_ALLOC_AND_FREE_DEBUG == 1)
	PVR_DPF((PVR_DBG_WARNING,
			"%s Exit(Ok), psNewSyncCheckpoint->ui32UID=%d <%p>",
			__FUNCTION__,
			psNewSyncCheckpoint->ui32UID,
			(void*)psNewSyncCheckpoint));
#endif
	return PVRSRV_OK;

fail_raalloc:
	OSLockDestroy(psNewSyncCheckpoint->hLock);
	psNewSyncCheckpoint->hLock = NULL;
fail_create_checkpoint_lock:
	OSFreeMem(psNewSyncCheckpoint);
fail_alloc:
	return eError;
}

static void SyncCheckpointUnref(_SYNC_CHECKPOINT *psSyncCheckpointInt)
{
	_SYNC_CHECKPOINT_CONTEXT *psContext;
	PVRSRV_DEVICE_NODE *psDevNode;

	psContext = psSyncCheckpointInt->psSyncCheckpointBlock->psContext;
	psDevNode = (PVRSRV_DEVICE_NODE *)psContext->psDevNode;

	/*
	 * Without this reference, the context may be destroyed as soon
	 * as _FreeSyncCheckpoint is called, but the context is still
	 * needed when _CheckDeferredCleanupList is called at the end
	 * of this function.
	 */
	_SyncCheckpointContextRef(psContext);

	PVR_ASSERT(psSyncCheckpointInt->ui32ValidationCheck == SYNC_CHECKPOINT_PATTERN_IN_USE);
	if (!OSAtomicRead(&psSyncCheckpointInt->hRefCount))
	{
		PVR_DPF((PVR_DBG_ERROR, "SyncCheckpointUnref sync checkpoint already freed"));
	}
	else if (0 == OSAtomicDecrement(&psSyncCheckpointInt->hRefCount))
	{
		/* If the firmware has serviced all enqueued references to the sync checkpoint, free it */
		if (psSyncCheckpointInt->psSyncCheckpointFwObj->ui32FwRefCount ==
				(IMG_UINT32)(OSAtomicRead(&psSyncCheckpointInt->hEnqueuedCCBCount)))
		{
#if (ENABLE_SYNC_CHECKPOINT_ALLOC_AND_FREE_DEBUG == 1)
			PVR_DPF((PVR_DBG_WARNING,
					"%s No outstanding FW ops and hRef is zero, deleting SyncCheckpoint..",
					__FUNCTION__));
#endif
#if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)
			if(psSyncCheckpointInt->hRecord)
			{
				PVRSRV_ERROR eError;
				/* remove this sync record */
				eError = _SyncCheckpointRecordRemove(psSyncCheckpointInt->hRecord);
				PVR_LOG_IF_ERROR(eError, "_SyncCheckpointRecordRemove");
			}
#endif
			/* Remove the sync checkpoint from the global list */
			OSLockAcquire(psDevNode->hSyncCheckpointListLock);
			dllist_remove_node(&psSyncCheckpointInt->sListNode);
			OSLockRelease(psDevNode->hSyncCheckpointListLock);

#if (SYNC_CHECKPOINT_POOL_SIZE > 0)
#if ((ENABLE_SYNC_CHECKPOINT_POOL_DEBUG == 1) || (ENABLE_SYNC_CHECKPOINT_ALLOC_AND_FREE_DEBUG == 1))
			PVR_DPF((PVR_DBG_WARNING,
					"%s attempting to return sync checkpoint to the pool",
					__FUNCTION__));
#endif
			if (!_PutCheckpointInPool(psSyncCheckpointInt))
#endif
			{
#if (SYNC_CHECKPOINT_POOL_SIZE > 0)
#if ((ENABLE_SYNC_CHECKPOINT_POOL_DEBUG == 1) || (ENABLE_SYNC_CHECKPOINT_ALLOC_AND_FREE_DEBUG == 1))
				PVR_DPF((PVR_DBG_WARNING,
						"%s pool is full, so just free it",
						__FUNCTION__));
#endif
#endif
#if (ENABLE_SYNC_CHECKPOINT_ALLOC_AND_FREE_DEBUG == 1)
				PVR_DPF((PVR_DBG_WARNING,
						"%s CALLING RA_Free(psSyncCheckpoint(ID:%d)<%p>), psSubAllocRA=<%p>, ui32SpanAddr=0x%llx",
						__FUNCTION__,
						psSyncCheckpointInt->ui32UID,
						(void*)psSyncCheckpointInt,
						(void*)psSyncCheckpointInt->psSyncCheckpointBlock->psContext->psSubAllocRA,
						psSyncCheckpointInt->uiSpanAddr));
#endif
				_FreeSyncCheckpoint(psSyncCheckpointInt);
			}
		}
		else
		{
#if ((ENABLE_SYNC_CHECKPOINT_DEFERRED_CLEANUP_DEBUG == 1) || (ENABLE_SYNC_CHECKPOINT_ALLOC_AND_FREE_DEBUG == 1))
			PVR_DPF((PVR_DBG_WARNING,
					"%s Outstanding FW ops hEnqueuedCCBCount=%d != FwObj->ui32FwRefCount=%d "
					"- DEFERRING CLEANUP psSyncCheckpoint(ID:%d)<%p>",
					__FUNCTION__,
					OSAtomicRead(&psSyncCheckpointInt->hEnqueuedCCBCount),
					psSyncCheckpointInt->psSyncCheckpointFwObj->ui32FwRefCount,
					psSyncCheckpointInt->ui32UID,
					(void*)psSyncCheckpointInt));
#endif
			/* Add the sync checkpoint to the deferred free list */
			OSLockAcquire(psContext->psContextCtl->hDeferredCleanupListLock);
			dllist_add_to_tail(&psContext->psContextCtl->sDeferredCleanupListHead,
			                   &psSyncCheckpointInt->sDeferredFreeListNode);
			OSLockRelease(psContext->psContextCtl->hDeferredCleanupListLock);
		}
	}
	else
	{
#if (ENABLE_SYNC_CHECKPOINT_ALLOC_AND_FREE_DEBUG == 1)
		PVR_DPF((PVR_DBG_WARNING,
				"%s psSyncCheckpoint(ID:%d)<%p>, hRefCount decremented to %d",
				__FUNCTION__,
				psSyncCheckpointInt->ui32UID,
				(void*)psSyncCheckpointInt,
				(IMG_UINT32)(OSAtomicRead(&psSyncCheckpointInt->hRefCount))));
#endif
	}

	/* See if any sync checkpoints in the deferred cleanup list can be freed */
	_CheckDeferredCleanupList(psContext);

	_SyncCheckpointContextUnref(psContext);
}

void SyncCheckpointFree(PSYNC_CHECKPOINT psSyncCheckpoint)
{
	_SYNC_CHECKPOINT *psSyncCheckpointInt = (_SYNC_CHECKPOINT*)psSyncCheckpoint;

	PVR_LOG_IF_FALSE((psSyncCheckpoint != NULL), "psSyncCheckpoint invalid");

#if (ENABLE_SYNC_CHECKPOINT_ALLOC_AND_FREE_DEBUG == 1)
	PVR_DPF((PVR_DBG_WARNING,
			"%s Entry,  psSyncCheckpoint(ID:%d)<%p>, hRefCount=%d, psSyncCheckpoint->ui32ValidationCheck=0x%x",
			__FUNCTION__,
			psSyncCheckpointInt->ui32UID,
			(void*)psSyncCheckpoint,
			(IMG_UINT32)(OSAtomicRead(&psSyncCheckpointInt->hRefCount)),
			psSyncCheckpointInt->ui32ValidationCheck));
#endif
	SyncCheckpointUnref(psSyncCheckpointInt);
}

void
SyncCheckpointSignal(PSYNC_CHECKPOINT psSyncCheckpoint, IMG_BOOL bSleepAllowed)
{
	_SYNC_CHECKPOINT *psSyncCheckpointInt = (_SYNC_CHECKPOINT*)psSyncCheckpoint;

	PVR_LOG_IF_FALSE((psSyncCheckpoint != NULL), "psSyncCheckpoint invalid");

	if(psSyncCheckpointInt)
	{
		PVR_LOG_IF_FALSE((psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State == PVRSRV_SYNC_CHECKPOINT_NOT_SIGNALLED),
		                 "psSyncCheckpoint already signalled");

		if (psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State == PVRSRV_SYNC_CHECKPOINT_NOT_SIGNALLED)
		{
			RGX_HWPERF_UFO_DATA_ELEMENT sSyncData;
			PVRSRV_RGXDEV_INFO *psDevInfo = psSyncCheckpointInt->psSyncCheckpointBlock->psDevNode->pvDevice;

			sSyncData.sUpdate.ui32FWAddr = SyncCheckpointGetFirmwareAddr(psSyncCheckpoint);
			sSyncData.sUpdate.ui32OldValue = psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State;
			sSyncData.sUpdate.ui32NewValue = PVRSRV_SYNC_CHECKPOINT_SIGNALLED;

			RGX_HWPERF_HOST_UFO(psDevInfo, RGX_HWPERF_UFO_EV_UPDATE, &sSyncData, bSleepAllowed);

			psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State = PVRSRV_SYNC_CHECKPOINT_SIGNALLED;

#if defined(PDUMP)
			/* We may need to temporarily disable the posting of PDump events here, as the caller can be
			 * in interrupt context and PDUMPCOMMENTWITHFLAGS takes the PDUMP_LOCK mutex
			 */
			PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS,
								  "Signalled Sync Checkpoint %s (ID:%d, TL:%d, FirmwareVAddr = 0x%08x)",
								  psSyncCheckpointInt->azName,
								  psSyncCheckpointInt->ui32UID, psSyncCheckpointInt->hTimeline,
								  (psSyncCheckpointInt->psSyncCheckpointBlock->ui32FirmwareAddr +
										  _SyncCheckpointGetOffset(psSyncCheckpointInt)));
			_SyncCheckpointSignalPDump(psSyncCheckpointInt);
#endif
		}
		else
		{
#if (ENABLE_SYNC_CHECKPOINT_ENQ_AND_SIGNAL_DEBUG == 1)
			PVR_DPF((PVR_DBG_WARNING,
					"%s asked to set PVRSRV_SYNC_CHECKPOINT_SIGNALLED(%d) for (psSyncCheckpointInt->ui32UID=%d), "
					"when value is already %d",
					__FUNCTION__,
					PVRSRV_SYNC_CHECKPOINT_SIGNALLED,
					psSyncCheckpointInt->ui32UID,
					psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State));
#endif
		}
	}
}

void
SyncCheckpointSignalNoHW(PSYNC_CHECKPOINT psSyncCheckpoint)
{
	_SYNC_CHECKPOINT *psSyncCheckpointInt = (_SYNC_CHECKPOINT*)psSyncCheckpoint;

	PVR_LOG_IF_FALSE((psSyncCheckpoint != NULL), "psSyncCheckpoint invalid");

	if(psSyncCheckpointInt)
	{
		PVR_LOG_IF_FALSE((psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State == PVRSRV_SYNC_CHECKPOINT_NOT_SIGNALLED),
		                 "psSyncCheckpoint already signalled");

		if (psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State == PVRSRV_SYNC_CHECKPOINT_NOT_SIGNALLED)
		{
			RGX_HWPERF_UFO_DATA_ELEMENT sSyncData;
			PVRSRV_RGXDEV_INFO *psDevInfo = psSyncCheckpointInt->psSyncCheckpointBlock->psDevNode->pvDevice;

			sSyncData.sUpdate.ui32FWAddr = SyncCheckpointGetFirmwareAddr(psSyncCheckpoint);
			sSyncData.sUpdate.ui32OldValue = psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State;
			sSyncData.sUpdate.ui32NewValue = PVRSRV_SYNC_CHECKPOINT_SIGNALLED;

			RGX_HWPERF_HOST_UFO(psDevInfo, RGX_HWPERF_UFO_EV_UPDATE, &sSyncData, IMG_TRUE);

			psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State = PVRSRV_SYNC_CHECKPOINT_SIGNALLED;
		}
		else
		{
#if (ENABLE_SYNC_CHECKPOINT_ENQ_AND_SIGNAL_DEBUG == 1)
			PVR_DPF((PVR_DBG_WARNING,
					"%s asked to set PVRSRV_SYNC_CHECKPOINT_SIGNALLED(%d) for (psSyncCheckpointInt->ui32UID=%d), "
					"when value is already %d",
					__FUNCTION__,
					PVRSRV_SYNC_CHECKPOINT_SIGNALLED,
					psSyncCheckpointInt->ui32UID,
					psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State));
#endif
		}
	}
}

void
SyncCheckpointError(PSYNC_CHECKPOINT psSyncCheckpoint, IMG_BOOL bSleepAllowed)
{
	_SYNC_CHECKPOINT *psSyncCheckpointInt = (_SYNC_CHECKPOINT*)psSyncCheckpoint;

	PVR_LOG_IF_FALSE((psSyncCheckpoint != NULL), "psSyncCheckpoint invalid");

	if(psSyncCheckpointInt)
	{
		PVR_LOG_IF_FALSE((psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State == PVRSRV_SYNC_CHECKPOINT_NOT_SIGNALLED),
		                 "psSyncCheckpoint already signalled");

		if (psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State == PVRSRV_SYNC_CHECKPOINT_NOT_SIGNALLED)
		{
			RGX_HWPERF_UFO_DATA_ELEMENT sSyncData;
			PVRSRV_RGXDEV_INFO *psDevInfo = psSyncCheckpointInt->psSyncCheckpointBlock->psDevNode->pvDevice;

			sSyncData.sUpdate.ui32FWAddr = SyncCheckpointGetFirmwareAddr(psSyncCheckpoint);
			sSyncData.sUpdate.ui32OldValue = psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State;
			sSyncData.sUpdate.ui32NewValue = PVRSRV_SYNC_CHECKPOINT_ERRORED;

			RGX_HWPERF_HOST_UFO(psDevInfo, RGX_HWPERF_UFO_EV_UPDATE, &sSyncData, bSleepAllowed);

			psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State = PVRSRV_SYNC_CHECKPOINT_ERRORED;

#if defined(PDUMP)
			/* We may need to temporarily disable the posting of PDump events here, as the caller can be
			 * in interrupt context and PDUMPCOMMENTWITHFLAGS takes the PDUMP_LOCK mutex
			 */
			PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS,
								  "Errored Sync Checkpoint %s (ID:%d, TL:%d, FirmwareVAddr = 0x%08x)",
								  psSyncCheckpointInt->azName,
								  psSyncCheckpointInt->ui32UID, psSyncCheckpointInt->hTimeline,
								  (psSyncCheckpointInt->psSyncCheckpointBlock->ui32FirmwareAddr +
										  _SyncCheckpointGetOffset(psSyncCheckpointInt)));
			_SyncCheckpointErrorPDump(psSyncCheckpointInt);
#endif
		}
	}
}

IMG_BOOL SyncCheckpointIsSignalled(PSYNC_CHECKPOINT psSyncCheckpoint, IMG_BOOL bSleepAllowed)
{
	IMG_BOOL bRet = IMG_FALSE;
	_SYNC_CHECKPOINT *psSyncCheckpointInt = (_SYNC_CHECKPOINT*)psSyncCheckpoint;

	PVR_LOG_IF_FALSE((psSyncCheckpoint != NULL), "psSyncCheckpoint invalid");

	if (psSyncCheckpointInt)
	{
		RGX_HWPERF_UFO_DATA_ELEMENT sSyncData;
		RGX_HWPERF_UFO_EV eEV;
		PVRSRV_RGXDEV_INFO *psDevInfo = psSyncCheckpointInt->psSyncCheckpointBlock->psDevNode->pvDevice;

		bRet = ((psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State == PVRSRV_SYNC_CHECKPOINT_SIGNALLED) ||
		        (psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State == PVRSRV_SYNC_CHECKPOINT_ERRORED));

		if (bRet)
		{
			sSyncData.sCheckSuccess.ui32FWAddr = SyncCheckpointGetFirmwareAddr(psSyncCheckpoint);
			sSyncData.sCheckSuccess.ui32Value = psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State;

			eEV = RGX_HWPERF_UFO_EV_CHECK_SUCCESS;
		}
		else
		{
			sSyncData.sCheckFail.ui32FWAddr = SyncCheckpointGetFirmwareAddr(psSyncCheckpoint);
			sSyncData.sCheckFail.ui32Value = psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State;
			sSyncData.sCheckFail.ui32Required = PVRSRV_SYNC_CHECKPOINT_SIGNALLED;

			eEV = RGX_HWPERF_UFO_EV_CHECK_FAIL;
		}

		RGX_HWPERF_HOST_UFO(psDevInfo, eEV, &sSyncData, bSleepAllowed);

#if (ENABLE_SYNC_CHECKPOINT_ENQ_AND_SIGNAL_DEBUG == 1)
		PVR_DPF((PVR_DBG_WARNING,
				"%s called for psSyncCheckpoint<%p>, returning %d",
				__FUNCTION__,
				(void*)psSyncCheckpoint,
				bRet));
#endif
	}
	return bRet;
}

IMG_BOOL
SyncCheckpointIsErrored(PSYNC_CHECKPOINT psSyncCheckpoint, IMG_BOOL bSleepAllowed)
{
	IMG_BOOL bRet = IMG_FALSE;
	_SYNC_CHECKPOINT *psSyncCheckpointInt = (_SYNC_CHECKPOINT*)psSyncCheckpoint;

	PVR_LOG_IF_FALSE((psSyncCheckpoint != NULL), "psSyncCheckpoint invalid");

	if (psSyncCheckpointInt)
	{
		RGX_HWPERF_UFO_DATA_ELEMENT sSyncData;
		RGX_HWPERF_UFO_EV eEV;
		PVRSRV_RGXDEV_INFO *psDevInfo = psSyncCheckpointInt->psSyncCheckpointBlock->psDevNode->pvDevice;

		bRet = (psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State == PVRSRV_SYNC_CHECKPOINT_ERRORED);

		if (bRet)
		{
			sSyncData.sCheckSuccess.ui32FWAddr = SyncCheckpointGetFirmwareAddr(psSyncCheckpoint);
			sSyncData.sCheckSuccess.ui32Value = PVRSRV_SYNC_CHECKPOINT_ERRORED;

			eEV = RGX_HWPERF_UFO_EV_CHECK_SUCCESS;
		}
		else
		{
			sSyncData.sCheckFail.ui32FWAddr = SyncCheckpointGetFirmwareAddr(psSyncCheckpoint);
			sSyncData.sCheckFail.ui32Value = psSyncCheckpointInt->psSyncCheckpointFwObj->ui32State;
			sSyncData.sCheckFail.ui32Required = PVRSRV_SYNC_CHECKPOINT_ERRORED;

			eEV = RGX_HWPERF_UFO_EV_CHECK_FAIL;
		}

		RGX_HWPERF_HOST_UFO(psDevInfo, eEV, &sSyncData, bSleepAllowed);

#if (ENABLE_SYNC_CHECKPOINT_ENQ_AND_SIGNAL_DEBUG == 1)
		PVR_DPF((PVR_DBG_WARNING,
				"%s called for psSyncCheckpoint<%p>, returning %d",
				__FUNCTION__,
				(void*)psSyncCheckpoint,
				bRet));
#endif
	}
	return bRet;
}

PVRSRV_ERROR
SyncCheckpointTakeRef(PSYNC_CHECKPOINT psSyncCheckpoint)
{
	PVRSRV_ERROR eRet = PVRSRV_OK;
	_SYNC_CHECKPOINT *psSyncCheckpointInt = (_SYNC_CHECKPOINT*)psSyncCheckpoint;

	PVR_LOGR_IF_FALSE((psSyncCheckpoint != NULL),
	                  "psSyncCheckpoint invalid",
	                  PVRSRV_ERROR_INVALID_PARAMS);
#if (ENABLE_SYNC_CHECKPOINT_ALLOC_AND_FREE_DEBUG == 1)
	PVR_DPF((PVR_DBG_WARNING, "%s called for psSyncCheckpoint<%p> %d->%d (FWRef %u)",
			__func__,
			psSyncCheckpointInt,
			OSAtomicRead(&psSyncCheckpointInt->hRefCount),
			OSAtomicRead(&psSyncCheckpointInt->hRefCount)+1,
			psSyncCheckpointInt->psSyncCheckpointFwObj->ui32FwRefCount));
#endif
	OSAtomicIncrement(&psSyncCheckpointInt->hRefCount);

	return eRet;
}

PVRSRV_ERROR
SyncCheckpointDropRef(PSYNC_CHECKPOINT psSyncCheckpoint)
{
	PVRSRV_ERROR eRet = PVRSRV_OK;
	_SYNC_CHECKPOINT *psSyncCheckpointInt = (_SYNC_CHECKPOINT*)psSyncCheckpoint;

	PVR_LOGR_IF_FALSE((psSyncCheckpoint != NULL),
	                  "psSyncCheckpoint invalid",
	                  PVRSRV_ERROR_INVALID_PARAMS);
#if (ENABLE_SYNC_CHECKPOINT_ALLOC_AND_FREE_DEBUG == 1)
	PVR_DPF((PVR_DBG_WARNING, "%s called for psSyncCheckpoint<%p> %d->%d (FWRef %u)",
			__func__,
			psSyncCheckpointInt,
			OSAtomicRead(&psSyncCheckpointInt->hRefCount),
			OSAtomicRead(&psSyncCheckpointInt->hRefCount)-1,
			psSyncCheckpointInt->psSyncCheckpointFwObj->ui32FwRefCount));
#endif
	SyncCheckpointUnref(psSyncCheckpointInt);

	return eRet;
}

void
SyncCheckpointCCBEnqueued(PSYNC_CHECKPOINT psSyncCheckpoint)
{
	_SYNC_CHECKPOINT *psSyncCheckpointInt = (_SYNC_CHECKPOINT*)psSyncCheckpoint;

	PVR_LOG_IF_FALSE((psSyncCheckpoint != NULL), "psSyncCheckpoint invalid");

	if (psSyncCheckpointInt)
	{
#if !defined(NO_HARDWARE)
#if (ENABLE_SYNC_CHECKPOINT_ENQ_AND_SIGNAL_DEBUG == 1)
		PVR_DPF((PVR_DBG_WARNING, "%s called for psSyncCheckpoint<%p> %d->%d (FWRef %u)",
				__FUNCTION__,
				 (void*)psSyncCheckpoint,
				 OSAtomicRead(&psSyncCheckpointInt->hEnqueuedCCBCount),
				 OSAtomicRead(&psSyncCheckpointInt->hEnqueuedCCBCount)+1,
				 psSyncCheckpointInt->psSyncCheckpointFwObj->ui32FwRefCount));
#endif
		OSAtomicIncrement(&psSyncCheckpointInt->hEnqueuedCCBCount);
#endif
	}
}

PRGXFWIF_UFO_ADDR*
SyncCheckpointGetRGXFWIFUFOAddr(PSYNC_CHECKPOINT psSyncCheckpoint)
{
	_SYNC_CHECKPOINT *psSyncCheckpointInt = (_SYNC_CHECKPOINT*)psSyncCheckpoint;

	PVR_LOGG_IF_FALSE((psSyncCheckpoint != NULL), "psSyncCheckpoint invalid", invalid_chkpt);

	if (psSyncCheckpointInt)
	{
		if (psSyncCheckpointInt->ui32ValidationCheck == SYNC_CHECKPOINT_PATTERN_IN_USE)
		{
			return &psSyncCheckpointInt->sCheckpointUFOAddr;
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR,
					"%s called for psSyncCheckpoint<%p>, but ui32ValidationCheck=0x%x",
					__FUNCTION__,
					 (void*)psSyncCheckpoint,
					 psSyncCheckpointInt->ui32ValidationCheck));
		}
	}

invalid_chkpt:
	return NULL;
}

IMG_UINT32
SyncCheckpointGetFirmwareAddr(PSYNC_CHECKPOINT psSyncCheckpoint)
{
	_SYNC_CHECKPOINT *psSyncCheckpointInt = (_SYNC_CHECKPOINT*)psSyncCheckpoint;
	SYNC_CHECKPOINT_BLOCK *psSyncBlock;
	IMG_UINT32 ui32Ret = 0;

	PVR_LOGG_IF_FALSE((psSyncCheckpoint != NULL), "psSyncCheckpoint invalid", invalid_chkpt);

	if (psSyncCheckpointInt)
	{
		if (psSyncCheckpointInt->ui32ValidationCheck == SYNC_CHECKPOINT_PATTERN_IN_USE)
		{
			psSyncBlock = psSyncCheckpointInt->psSyncCheckpointBlock;
			/* add 1 to addr to indicate this FW addr is a sync checkpoint (not a sync prim) */
			ui32Ret = psSyncBlock->ui32FirmwareAddr + _SyncCheckpointGetOffset(psSyncCheckpointInt) + 1;
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR,
					"%s called for psSyncCheckpoint<%p>, but ui32ValidationCheck=0x%x",
					__FUNCTION__,
					 (void*)psSyncCheckpoint,
					 psSyncCheckpointInt->ui32ValidationCheck));
		}
	}
	return ui32Ret;

invalid_chkpt:
	return 0;
}

IMG_UINT32
SyncCheckpointGetFirmwareAddrFromList(PSYNC_CHECKPOINT psSyncCheckpoint)
{
	_SYNC_CHECKPOINT *psSyncCheckpointInt = (_SYNC_CHECKPOINT*) psSyncCheckpoint;
	SYNC_CHECKPOINT_BLOCK *psSyncBlock;
	IMG_UINT32 ui32Ret = 0;

	if (psSyncCheckpointInt)
	{
		if (psSyncCheckpointInt->ui32ValidationCheck == SYNC_CHECKPOINT_PATTERN_IN_USE)
		{
			psSyncBlock = psSyncCheckpointInt->psSyncCheckpointBlock;
			/* add 1 to addr to indicate this FW addr is a sync checkpoint (not a sync prim) */
			ui32Ret = psSyncBlock->ui32FirmwareAddr + _SyncCheckpointGetOffset(psSyncCheckpointInt) + 1;
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR,
					"%s called for psSyncCheckpointInt<%p>, but ui32ValidationCheck=0x%x",
					__FUNCTION__,
					 (void*)psSyncCheckpointInt,
					 psSyncCheckpointInt->ui32ValidationCheck));
		}
	}

#if (ENABLE_SYNC_CHECKPOINT_UFO_DEBUG == 1)
	PVR_DPF((PVR_DBG_WARNING,
			"%s returning %u (0x%x) as UFO addr for psSyncCheckpointInt<%p>",
			__FUNCTION__,
			ui32Ret,
			ui32Ret,
			(void*)psSyncCheckpointInt));
#endif
	return ui32Ret;
}

IMG_UINT32
SyncCheckpointGetId(PSYNC_CHECKPOINT psSyncCheckpoint)
{
	_SYNC_CHECKPOINT *psSyncCheckpointInt = (_SYNC_CHECKPOINT*)psSyncCheckpoint;
	IMG_UINT32 ui32Ret = 0;

	PVR_LOGG_IF_FALSE((psSyncCheckpoint != NULL), "psSyncCheckpoint invalid", invalid_chkpt);

	if (psSyncCheckpointInt)
	{
#if (ENABLE_SYNC_CHECKPOINT_UFO_DEBUG == 1)
		PVR_DPF((PVR_DBG_WARNING,
				"%s returning ID for sync checkpoint<%p>",
				__FUNCTION__,
				(void*)psSyncCheckpointInt));
		PVR_DPF((PVR_DBG_WARNING,
				"%s (validationCheck=0x%x)",
				__FUNCTION__,
				psSyncCheckpointInt->ui32ValidationCheck));
#endif
		ui32Ret = psSyncCheckpointInt->ui32UID;
#if (ENABLE_SYNC_CHECKPOINT_UFO_DEBUG == 1)
		PVR_DPF((PVR_DBG_WARNING,
				"%s (ui32UID=0x%x)",
				__FUNCTION__,
				psSyncCheckpointInt->ui32UID));
#endif
	}
	return ui32Ret;

invalid_chkpt:
	return 0;
}

PVRSRV_TIMELINE
SyncCheckpointGetTimeline(PSYNC_CHECKPOINT psSyncCheckpoint)
{
	_SYNC_CHECKPOINT *psSyncCheckpointInt = (_SYNC_CHECKPOINT*)psSyncCheckpoint;
	PVRSRV_TIMELINE i32Ret = PVRSRV_NO_TIMELINE;

	PVR_LOGG_IF_FALSE((psSyncCheckpoint != NULL),
	                  "psSyncCheckpoint invalid",
	                  invalid_chkpt);

	if (psSyncCheckpointInt)
	{
		i32Ret = psSyncCheckpointInt->hTimeline;
	}
	return i32Ret;

invalid_chkpt:
	return 0;
}


IMG_UINT32
SyncCheckpointGetEnqueuedCount(PSYNC_CHECKPOINT psSyncCheckpoint)
{
	_SYNC_CHECKPOINT *psSyncCheckpointInt = (_SYNC_CHECKPOINT*)psSyncCheckpoint;
	IMG_UINT32 ui32Ret = 0;

	PVR_LOGG_IF_FALSE((psSyncCheckpoint != NULL),
	                  "psSyncCheckpoint invalid",
	                  invalid_chkpt);

	if (psSyncCheckpointInt)
	{
		ui32Ret = OSAtomicRead(&psSyncCheckpointInt->hEnqueuedCCBCount);
	}
	return ui32Ret;

invalid_chkpt:
	return 0;
}

void SyncCheckpointErrorFromUFO(PPVRSRV_DEVICE_NODE psDevNode,
								IMG_UINT32 ui32FwAddr)
{
	_SYNC_CHECKPOINT *psSyncCheckpointInt;
	PDLLIST_NODE psNode, psNext;

#if (ENABLE_SYNC_CHECKPOINT_UFO_DEBUG == 1)
	PVR_DPF((PVR_DBG_WARNING,
			"%s called to error UFO with ui32FWAddr=%d",
			__FUNCTION__,
			ui32FwAddr));
#endif

	OSLockAcquire(psDevNode->hSyncCheckpointListLock);
	dllist_foreach_node(&psDevNode->sSyncCheckpointSyncsList, psNode, psNext)
	{
		psSyncCheckpointInt = IMG_CONTAINER_OF(psNode, _SYNC_CHECKPOINT, sListNode);
		if (ui32FwAddr == SyncCheckpointGetFirmwareAddr((PSYNC_CHECKPOINT)psSyncCheckpointInt))
		{
#if (ENABLE_SYNC_CHECKPOINT_UFO_DEBUG == 1)
			PVR_DPF((PVR_DBG_WARNING,
					"%s calling SyncCheckpointError for sync checkpoint <%p>",
					__FUNCTION__,
					(void*)psSyncCheckpointInt));
#endif
			/* Mark as errored */
			SyncCheckpointError((PSYNC_CHECKPOINT)psSyncCheckpointInt, IMG_TRUE);
			break;
		}
	}
	OSLockRelease(psDevNode->hSyncCheckpointListLock);
}

void SyncCheckpointRollbackFromUFO(PPVRSRV_DEVICE_NODE psDevNode, IMG_UINT32 ui32FwAddr)
{
	_SYNC_CHECKPOINT *psSyncCheckpointInt;
	PDLLIST_NODE psNode, psNext;

#if (ENABLE_SYNC_CHECKPOINT_UFO_DEBUG == 1)
	PVR_DPF((PVR_DBG_WARNING,
			"%s called to rollback UFO with ui32FWAddr=0x%x",
			__FUNCTION__,
			ui32FwAddr));
#endif
#if !defined(NO_HARDWARE)
	OSLockAcquire(psDevNode->hSyncCheckpointListLock);
	dllist_foreach_node(&psDevNode->sSyncCheckpointSyncsList, psNode, psNext)
	{
		psSyncCheckpointInt = IMG_CONTAINER_OF(psNode, _SYNC_CHECKPOINT, sListNode);
		if (ui32FwAddr == SyncCheckpointGetFirmwareAddr((PSYNC_CHECKPOINT)psSyncCheckpointInt))
		{
#if ((ENABLE_SYNC_CHECKPOINT_UFO_DEBUG == 1)) || (ENABLE_SYNC_CHECKPOINT_ENQ_AND_SIGNAL_DEBUG == 1)
			PVR_DPF((PVR_DBG_WARNING,
					"%s called for psSyncCheckpointInt<%p> %d->%d",
					__FUNCTION__,
					(void*)psSyncCheckpointInt,
					OSAtomicRead(&psSyncCheckpointInt->hEnqueuedCCBCount),
					OSAtomicRead(&psSyncCheckpointInt->hEnqueuedCCBCount)-1));
#endif
			OSAtomicDecrement(&psSyncCheckpointInt->hEnqueuedCCBCount);
			break;
		}
	}
	OSLockRelease(psDevNode->hSyncCheckpointListLock);
#else
	PVR_UNREFERENCED_PARAMETER(psNode);
	PVR_UNREFERENCED_PARAMETER(psNext);
	PVR_UNREFERENCED_PARAMETER(psSyncCheckpointInt);
#endif
}

PVRSRV_ERROR
SyncCheckpointInit(PPVRSRV_DEVICE_NODE psDevNode)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (!gbSyncCheckpointInit)
	{
		eError = OSLockCreate(&psDevNode->hSyncCheckpointListLock, LOCK_TYPE_NONE);
		if (eError == PVRSRV_OK)
		{
			dllist_init(&psDevNode->sSyncCheckpointSyncsList);

#if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)
			eError = PVRSRVRegisterDbgRequestNotify(&psDevNode->hSyncCheckpointNotify,
													psDevNode,
													_SyncCheckpointDebugRequest,
													DEBUG_REQUEST_SYNCCHECKPOINT,
													(PVRSRV_DBGREQ_HANDLE)psDevNode);
#endif
			if (eError == PVRSRV_OK)
			{
#if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)
				_SyncCheckpointRecordListInit(psDevNode);
#endif
				gbSyncCheckpointInit = IMG_TRUE;
			}
			else
			{
				/* free the created lock */
				OSLockDestroy(psDevNode->hSyncCheckpointListLock);
				psDevNode->hSyncCheckpointListLock = NULL;
			}
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR,
					"%s FAILED to create psDevNode->hSyncCheckpointListLock",
					__FUNCTION__));
		}
	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR, "%s called but already initialised", __FUNCTION__));
	}
	return eError;
}

void SyncCheckpointDeinit(PPVRSRV_DEVICE_NODE psDevNode)
{
#if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)
	PVRSRVUnregisterDbgRequestNotify(psDevNode->hSyncCheckpointNotify);
#endif
	psDevNode->hSyncCheckpointNotify = NULL;
	OSLockDestroy(psDevNode->hSyncCheckpointListLock);
	psDevNode->hSyncCheckpointListLock = NULL;
#if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)
	_SyncCheckpointRecordListDeinit(psDevNode);
#endif
	gbSyncCheckpointInit = IMG_FALSE;
}

#if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)
void SyncCheckpointRecordLookup(PPVRSRV_DEVICE_NODE psDevNode, IMG_UINT32 ui32FwAddr,
								IMG_CHAR * pszSyncInfo, size_t len)
{
	DLLIST_NODE *psNode, *psNext;
	IMG_BOOL bFound = IMG_FALSE;

	if (!pszSyncInfo)
	{
		return;
	}

	pszSyncInfo[0] = '\0';

	OSLockAcquire(psDevNode->hSyncCheckpointRecordLock);
		dllist_foreach_node(&psDevNode->sSyncCheckpointRecordList, psNode, psNext)
		{
			struct SYNC_CHECKPOINT_RECORD *psSyncCheckpointRec =
				IMG_CONTAINER_OF(psNode, struct SYNC_CHECKPOINT_RECORD, sNode);
			if ((psSyncCheckpointRec->ui32FwBlockAddr + psSyncCheckpointRec->ui32SyncOffset + 1) == ui32FwAddr)
			{
				SYNC_CHECKPOINT_BLOCK *psSyncCheckpointBlock = psSyncCheckpointRec->psSyncCheckpointBlock;
				if (psSyncCheckpointBlock && psSyncCheckpointBlock->pui32LinAddr)
				{
					void *pSyncCheckpointAddr = (void*)( ((IMG_BYTE*)
							psSyncCheckpointBlock->pui32LinAddr) + psSyncCheckpointRec->ui32SyncOffset);
					OSSNPrintf(pszSyncInfo, len, "%s Checkpoint:%05u (%s)",
							   (*(IMG_UINT32*)pSyncCheckpointAddr == PVRSRV_SYNC_CHECKPOINT_SIGNALLED) ?
							   "SIGNALLED" :
							   ((*(IMG_UINT32*)pSyncCheckpointAddr == PVRSRV_SYNC_CHECKPOINT_ERRORED) ?
								"ERRORED" : "NOT_SIGNALLED"),
							   psSyncCheckpointRec->uiPID,
							   psSyncCheckpointRec->szClassName);
				}
				else
				{
					OSSNPrintf(pszSyncInfo, len, "Checkpoint:%05u (%s)",
							   psSyncCheckpointRec->uiPID,
							   psSyncCheckpointRec->szClassName);
				}

				bFound = IMG_TRUE;
				break;
			}
		}
	OSLockRelease(psDevNode->hSyncCheckpointRecordLock);

	if(!bFound && (psDevNode->ui32SyncCheckpointRecordCountHighWatermark == SYNC_CHECKPOINT_RECORD_LIMIT))
	{
		OSSNPrintf(pszSyncInfo, len, "(Record may be lost)");
	}
}

static void _SyncCheckpointState(PDLLIST_NODE psNode,
					DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
					void *pvDumpDebugFile)
{
	_SYNC_CHECKPOINT *psSyncCheckpoint = IMG_CONTAINER_OF(psNode, _SYNC_CHECKPOINT, sListNode);

	if (psSyncCheckpoint->psSyncCheckpointFwObj->ui32State == PVRSRV_SYNC_CHECKPOINT_NOT_SIGNALLED)
	{
		PVR_DUMPDEBUG_LOG("\tPending sync checkpoint(ID = %d, FWAddr = 0x%08x): (%s)",
						   psSyncCheckpoint->ui32UID,
						   psSyncCheckpoint->psSyncCheckpointBlock->ui32FirmwareAddr +
							   _SyncCheckpointGetOffset(psSyncCheckpoint),
						   psSyncCheckpoint->azName);
	}
}

static void _SyncCheckpointDebugRequest(PVRSRV_DBGREQ_HANDLE hDebugRequestHandle,
					IMG_UINT32 ui32VerbLevel,
					DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
					void *pvDumpDebugFile)
{
	PVRSRV_DEVICE_NODE *psDevNode = (PVRSRV_DEVICE_NODE *)hDebugRequestHandle;
	DLLIST_NODE *psNode, *psNext;

	if (ui32VerbLevel == DEBUG_REQUEST_VERBOSITY_MEDIUM)
	{
		PVR_DUMPDEBUG_LOG("Dumping all pending sync checkpoints");
		OSLockAcquire(psDevNode->hSyncCheckpointListLock);
		dllist_foreach_node(&psDevNode->sSyncCheckpointSyncsList, psNode, psNext)
		{
			_SyncCheckpointState(psNode, pfnDumpDebugPrintf, pvDumpDebugFile);
		}
		OSLockRelease(psDevNode->hSyncCheckpointListLock);
	}
}

static PVRSRV_ERROR
_SyncCheckpointRecordAdd(
			PSYNC_CHECKPOINT_RECORD_HANDLE * phRecord,
			SYNC_CHECKPOINT_BLOCK *hSyncCheckpointBlock,
			IMG_UINT32 ui32FwBlockAddr,
			IMG_UINT32 ui32SyncOffset,
			IMG_UINT32 ui32UID,
			IMG_UINT32 ui32ClassNameSize,
			const IMG_CHAR *pszClassName)
{
	struct SYNC_CHECKPOINT_RECORD * psSyncRec;
	_SYNC_CHECKPOINT_CONTEXT *psContext = hSyncCheckpointBlock->psContext;
	PVRSRV_DEVICE_NODE *psDevNode = psContext->psDevNode;
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (!phRecord)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	*phRecord = NULL;

	psSyncRec = OSAllocMem(sizeof(*psSyncRec));
	PVR_LOGG_IF_NOMEM(psSyncRec, "OSAllocMem", eError, fail_alloc); /* Sets OOM error code */

	psSyncRec->psDevNode = psDevNode;
	psSyncRec->psSyncCheckpointBlock = hSyncCheckpointBlock;
	psSyncRec->ui32SyncOffset = ui32SyncOffset;
	psSyncRec->ui32FwBlockAddr = ui32FwBlockAddr;
	psSyncRec->ui64OSTime = OSClockns64();
	psSyncRec->uiPID = OSGetCurrentProcessID();
	psSyncRec->ui32UID = ui32UID;
	if(pszClassName)
	{
		if (ui32ClassNameSize >= SYNC_MAX_CLASS_NAME_LEN)
			ui32ClassNameSize = SYNC_MAX_CLASS_NAME_LEN;
		/* Copy over the class name annotation */
		OSStringLCopy(psSyncRec->szClassName, pszClassName, ui32ClassNameSize);
	}
	else
	{
		/* No class name annotation */
		psSyncRec->szClassName[0] = 0;
	}

	OSLockAcquire(psDevNode->hSyncCheckpointRecordLock);
	if(psDevNode->ui32SyncCheckpointRecordCount < SYNC_CHECKPOINT_RECORD_LIMIT)
	{
		dllist_add_to_head(&psDevNode->sSyncCheckpointRecordList, &psSyncRec->sNode);
		psDevNode->ui32SyncCheckpointRecordCount++;

		if(psDevNode->ui32SyncCheckpointRecordCount > psDevNode->ui32SyncCheckpointRecordCountHighWatermark)
		{
			psDevNode->ui32SyncCheckpointRecordCountHighWatermark = psDevNode->ui32SyncCheckpointRecordCount;
		}
	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to add sync checkpoint record \"%s\". %u records already exist.",
											__func__,
											pszClassName,
											psDevNode->ui32SyncCheckpointRecordCount));
		OSFreeMem(psSyncRec);
		psSyncRec = NULL;
		eError = PVRSRV_ERROR_TOOMANYBUFFERS;
	}
	OSLockRelease(psDevNode->hSyncCheckpointRecordLock);

	*phRecord = (PSYNC_CHECKPOINT_RECORD_HANDLE)psSyncRec;

fail_alloc:
	return eError;
}

static PVRSRV_ERROR
_SyncCheckpointRecordRemove(PSYNC_CHECKPOINT_RECORD_HANDLE hRecord)
{
	struct SYNC_CHECKPOINT_RECORD **ppFreedSync;
	struct SYNC_CHECKPOINT_RECORD *pSync = (struct SYNC_CHECKPOINT_RECORD*)hRecord;
	PVRSRV_DEVICE_NODE *psDevNode;

	if (!hRecord)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDevNode = pSync->psDevNode;

	OSLockAcquire(psDevNode->hSyncCheckpointRecordLock);

	dllist_remove_node(&pSync->sNode);

	if (psDevNode->uiSyncCheckpointRecordFreeIdx >= PVRSRV_FULL_SYNC_TRACKING_HISTORY_LEN)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: psDevNode->uiSyncCheckpointRecordFreeIdx out of range",
				__FUNCTION__));
		psDevNode->uiSyncCheckpointRecordFreeIdx = 0;
	}
	ppFreedSync = &psDevNode->apsSyncCheckpointRecordsFreed[psDevNode->uiSyncCheckpointRecordFreeIdx];
	psDevNode->uiSyncCheckpointRecordFreeIdx =
			(psDevNode->uiSyncCheckpointRecordFreeIdx + 1) % PVRSRV_FULL_SYNC_TRACKING_HISTORY_LEN;

	if (*ppFreedSync)
	{
		OSFreeMem(*ppFreedSync);
	}
	pSync->psSyncCheckpointBlock = NULL;
	pSync->ui64OSTime = OSClockns64();
	*ppFreedSync = pSync;

	psDevNode->ui32SyncCheckpointRecordCount--;

	OSLockRelease(psDevNode->hSyncCheckpointRecordLock);

	return PVRSRV_OK;
}

#define NS_IN_S (1000000000UL)
static void _SyncCheckpointRecordPrint(struct SYNC_CHECKPOINT_RECORD *psSyncCheckpointRec,
					IMG_UINT64 ui64TimeNow,
					DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
					void *pvDumpDebugFile)
{
	SYNC_CHECKPOINT_BLOCK *psSyncCheckpointBlock = psSyncCheckpointRec->psSyncCheckpointBlock;
	IMG_UINT64 ui64DeltaS;
	IMG_UINT32 ui32DeltaF;
	IMG_UINT64 ui64Delta = ui64TimeNow - psSyncCheckpointRec->ui64OSTime;
	ui64DeltaS = OSDivide64(ui64Delta, NS_IN_S, &ui32DeltaF);

	if (psSyncCheckpointBlock && psSyncCheckpointBlock->pui32LinAddr)
	{
		void *pSyncCheckpointAddr;
		pSyncCheckpointAddr = (void*)( ((IMG_BYTE*) psSyncCheckpointBlock->pui32LinAddr) + psSyncCheckpointRec->ui32SyncOffset);

		PVR_DUMPDEBUG_LOG("\t%05u %05llu.%09u %010u FWAddr=0x%08x State=%s (%s)",
		                  psSyncCheckpointRec->uiPID,
		                  ui64DeltaS, ui32DeltaF,psSyncCheckpointRec->ui32UID,
		                  (psSyncCheckpointRec->ui32FwBlockAddr+psSyncCheckpointRec->ui32SyncOffset),
		                  (*(IMG_UINT32*)pSyncCheckpointAddr == PVRSRV_SYNC_CHECKPOINT_SIGNALLED) ?
		                      "SIGNALLED" :
		                      ((*(IMG_UINT32*)pSyncCheckpointAddr == PVRSRV_SYNC_CHECKPOINT_ERRORED) ?
		                          "ERRORED" : "NOT_SIGNALLED"),
		                  psSyncCheckpointRec->szClassName);
	}
	else
	{
		PVR_DUMPDEBUG_LOG("\t%05u %05llu.%09u %010u FWAddr=0x%08x State=<null_ptr> (%s)",
			psSyncCheckpointRec->uiPID,
			ui64DeltaS, ui32DeltaF, psSyncCheckpointRec->ui32UID,
			(psSyncCheckpointRec->ui32FwBlockAddr+psSyncCheckpointRec->ui32SyncOffset),
			psSyncCheckpointRec->szClassName
			);
	}
}

static void _SyncCheckpointRecordRequest(PVRSRV_DBGREQ_HANDLE hDebugRequestHandle,
						IMG_UINT32 ui32VerbLevel,
						DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
						void *pvDumpDebugFile)
{
	PVRSRV_DEVICE_NODE *psDevNode = (PVRSRV_DEVICE_NODE *)hDebugRequestHandle;
	IMG_UINT64 ui64TimeNowS;
	IMG_UINT32 ui32TimeNowF;
	IMG_UINT64 ui64TimeNow = OSClockns64();
	DLLIST_NODE *psNode, *psNext;

	ui64TimeNowS = OSDivide64(ui64TimeNow, NS_IN_S, &ui32TimeNowF);

	if (ui32VerbLevel == DEBUG_REQUEST_VERBOSITY_MEDIUM)
	{
		IMG_UINT32 i;

		OSLockAcquire(psDevNode->hSyncCheckpointRecordLock);

		PVR_DUMPDEBUG_LOG("Dumping allocated sync checkpoints. Allocated: %u High watermark: %u (time ref %05llu.%09u)",
		                  psDevNode->ui32SyncCheckpointRecordCount,
		                  psDevNode->ui32SyncCheckpointRecordCountHighWatermark,
		                  ui64TimeNowS,
		                  ui32TimeNowF);
		if(psDevNode->ui32SyncCheckpointRecordCountHighWatermark == SYNC_CHECKPOINT_RECORD_LIMIT)
		{
			PVR_DUMPDEBUG_LOG("Warning: Record limit (%u) was reached. Some sync checkpoints may not have been recorded in the debug information.",
																SYNC_CHECKPOINT_RECORD_LIMIT);
		}
		PVR_DUMPDEBUG_LOG("\t%-5s %-15s %-10s %-17s %-14s (%s)",
					"PID", "Time Delta (s)", "UID", "Address", "State", "Annotation");

		dllist_foreach_node(&psDevNode->sSyncCheckpointRecordList, psNode, psNext)
		{
			struct SYNC_CHECKPOINT_RECORD *psSyncCheckpointRec =
				IMG_CONTAINER_OF(psNode, struct SYNC_CHECKPOINT_RECORD, sNode);
			_SyncCheckpointRecordPrint(psSyncCheckpointRec, ui64TimeNow,
							pfnDumpDebugPrintf, pvDumpDebugFile);
		}

		PVR_DUMPDEBUG_LOG("Dumping all recently freed sync checkpoints @ %05llu.%09u",
		                  ui64TimeNowS,
		                  ui32TimeNowF);
		PVR_DUMPDEBUG_LOG("\t%-5s %-15s %-10s %-17s %-14s (%s)",
					"PID", "Time Delta (s)", "UID", "Address", "State", "Annotation");
		for(i = DECREMENT_WITH_WRAP(psDevNode->uiSyncCheckpointRecordFreeIdx, PVRSRV_FULL_SYNC_TRACKING_HISTORY_LEN);
				i != psDevNode->uiSyncCheckpointRecordFreeIdx;
				i = DECREMENT_WITH_WRAP(i, PVRSRV_FULL_SYNC_TRACKING_HISTORY_LEN))
		{
			if (psDevNode->apsSyncCheckpointRecordsFreed[i])
			{
				_SyncCheckpointRecordPrint(psDevNode->apsSyncCheckpointRecordsFreed[i],
										   ui64TimeNow, pfnDumpDebugPrintf, pvDumpDebugFile);
			}
			else
			{
				break;
			}
		}
		OSLockRelease(psDevNode->hSyncCheckpointRecordLock);
	}
}
#undef NS_IN_S
static PVRSRV_ERROR _SyncCheckpointRecordListInit(PVRSRV_DEVICE_NODE *psDevNode)
{
	PVRSRV_ERROR eError;

	eError = OSLockCreate(&psDevNode->hSyncCheckpointRecordLock, LOCK_TYPE_NONE);
	if (eError != PVRSRV_OK)
	{
		goto fail_lock_create;
	}
	dllist_init(&psDevNode->sSyncCheckpointRecordList);

	psDevNode->ui32SyncCheckpointRecordCount = 0;
	psDevNode->ui32SyncCheckpointRecordCountHighWatermark = 0;

	eError = PVRSRVRegisterDbgRequestNotify(&psDevNode->hSyncCheckpointRecordNotify,
											psDevNode,
											_SyncCheckpointRecordRequest,
											DEBUG_REQUEST_SYNCCHECKPOINT,
											(PVRSRV_DBGREQ_HANDLE)psDevNode);

	if (eError != PVRSRV_OK)
	{
		goto fail_dbg_register;
	}

	return PVRSRV_OK;

fail_dbg_register:
	OSLockDestroy(psDevNode->hSyncCheckpointRecordLock);
fail_lock_create:
	return eError;
}

static void _SyncCheckpointRecordListDeinit(PVRSRV_DEVICE_NODE *psDevNode)
{
	DLLIST_NODE *psNode, *psNext;
	int i;

	OSLockAcquire(psDevNode->hSyncCheckpointRecordLock);
	dllist_foreach_node(&psDevNode->sSyncCheckpointRecordList, psNode, psNext)
	{
		struct SYNC_CHECKPOINT_RECORD *pSyncCheckpointRec =
			IMG_CONTAINER_OF(psNode, struct SYNC_CHECKPOINT_RECORD, sNode);

		dllist_remove_node(psNode);
		OSFreeMem(pSyncCheckpointRec);
	}

	for (i = 0; i < PVRSRV_FULL_SYNC_TRACKING_HISTORY_LEN; i++)
	{
		if (psDevNode->apsSyncCheckpointRecordsFreed[i])
		{
			OSFreeMem(psDevNode->apsSyncCheckpointRecordsFreed[i]);
			psDevNode->apsSyncCheckpointRecordsFreed[i] = NULL;
		}
	}
	OSLockRelease(psDevNode->hSyncCheckpointRecordLock);

	if (psDevNode->hSyncCheckpointRecordNotify)
	{
		PVRSRVUnregisterDbgRequestNotify(psDevNode->hSyncCheckpointRecordNotify);
	}
	OSLockDestroy(psDevNode->hSyncCheckpointRecordLock);
}
#endif /* defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING) */

PVRSRV_ERROR
SyncCheckpointPDumpPol(PSYNC_CHECKPOINT psSyncCheckpoint, PDUMP_FLAGS_T ui32PDumpFlags)
{
	_SYNC_CHECKPOINT *psSyncCheckpointInt = (_SYNC_CHECKPOINT*)psSyncCheckpoint;

	DevmemPDumpDevmemPol32(psSyncCheckpointInt->psSyncCheckpointBlock->hMemDesc,
						   _SyncCheckpointGetOffset(psSyncCheckpointInt),
						   PVRSRV_SYNC_CHECKPOINT_SIGNALLED,
						   0xFFFFFFFF,
						   PDUMP_POLL_OPERATOR_EQUAL,
						   ui32PDumpFlags);
	return PVRSRV_OK;
}

#if defined(PDUMP)
static PVRSRV_ERROR
_SyncCheckpointSignalPDump(_SYNC_CHECKPOINT *psSyncCheckpoint)
{
	/*
		We might be ask to PDump sync state outside of capture range
		(e.g. texture uploads) so make this continuous.
	*/
	DevmemPDumpLoadMemValue32(psSyncCheckpoint->psSyncCheckpointBlock->hMemDesc,
					   _SyncCheckpointGetOffset(psSyncCheckpoint),
					   PVRSRV_SYNC_CHECKPOINT_SIGNALLED,
					   PDUMP_FLAGS_CONTINUOUS);

	return PVRSRV_OK;
}

static PVRSRV_ERROR
_SyncCheckpointErrorPDump(_SYNC_CHECKPOINT *psSyncCheckpoint)
{
	/*
		We might be ask to PDump sync state outside of capture range
		(e.g. texture uploads) so make this continuous.
	*/
	DevmemPDumpLoadMemValue32(psSyncCheckpoint->psSyncCheckpointBlock->hMemDesc,
					   _SyncCheckpointGetOffset(psSyncCheckpoint),
					   PVRSRV_SYNC_CHECKPOINT_ERRORED,
					   PDUMP_FLAGS_CONTINUOUS);

	return PVRSRV_OK;
}
#endif

static void _CheckDeferredCleanupList(_SYNC_CHECKPOINT_CONTEXT *psContext)
{
	DLLIST_NODE *psNode, *psNext;
	PVRSRV_DEVICE_NODE *psDevNode = (PVRSRV_DEVICE_NODE*)psContext->psDevNode;

	/* Check the deferred cleanup list and free any sync checkpoints we can */
	OSLockAcquire(psContext->psContextCtl->hDeferredCleanupListLock);
#if (ENABLE_SYNC_CHECKPOINT_DEFERRED_CLEANUP_DEBUG == 1)
	PVR_DPF((PVR_DBG_WARNING, "%s called", __FUNCTION__));
#endif

	if (dllist_is_empty(&psContext->psContextCtl->sDeferredCleanupListHead))
	{
#if (ENABLE_SYNC_CHECKPOINT_DEFERRED_CLEANUP_DEBUG == 1)
		PVR_DPF((PVR_DBG_WARNING, "%s: Defer free list is empty", __FUNCTION__));
#endif
	}

	dllist_foreach_node(&psContext->psContextCtl->sDeferredCleanupListHead, psNode, psNext)
	{
		_SYNC_CHECKPOINT *psSyncCheckpointInt =
			IMG_CONTAINER_OF(psNode, _SYNC_CHECKPOINT, sDeferredFreeListNode);

		if (psSyncCheckpointInt->psSyncCheckpointFwObj->ui32FwRefCount ==
				(IMG_UINT32)(OSAtomicRead(&psSyncCheckpointInt->hEnqueuedCCBCount)))
		{
#if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)
			if(psSyncCheckpointInt->hRecord)
			{
				PVRSRV_ERROR eError;
				/* remove this sync record */
				eError = _SyncCheckpointRecordRemove(psSyncCheckpointInt->hRecord);
				PVR_LOG_IF_ERROR(eError, "_SyncCheckpointRecordRemove");
			}
#endif

			/* Remove the sync checkpoint from the deferred free list */
			dllist_remove_node(&psSyncCheckpointInt->sDeferredFreeListNode);

			/* Remove the sync checkpoint from the global list */
			OSLockAcquire(psDevNode->hSyncCheckpointListLock);
			dllist_remove_node(&psSyncCheckpointInt->sListNode);
			OSLockRelease(psDevNode->hSyncCheckpointListLock);

#if (SYNC_CHECKPOINT_POOL_SIZE > 0)
#if (ENABLE_SYNC_CHECKPOINT_DEFERRED_CLEANUP_DEBUG == 1)
			PVR_DPF((PVR_DBG_WARNING,
					"%s attempting to return sync(ID:%d),%p> to pool",
					__FUNCTION__,
					psSyncCheckpointInt->ui32UID,
					(void*)psSyncCheckpointInt));
#endif
			if (!_PutCheckpointInPool(psSyncCheckpointInt))
#endif
			{
#if (SYNC_CHECKPOINT_POOL_SIZE > 0)
#if (ENABLE_SYNC_CHECKPOINT_DEFERRED_CLEANUP_DEBUG == 1)
				PVR_DPF((PVR_DBG_WARNING, "%s pool is full, so just free it", __FUNCTION__));
#endif
#endif
#if (ENABLE_SYNC_CHECKPOINT_DEFERRED_CLEANUP_DEBUG == 1)
				PVR_DPF((PVR_DBG_WARNING,
						"%s CALLING RA_Free(psSyncCheckpoint(ID:%d)<%p>), "
						"psSubAllocRA=<%p>, ui32SpanAddr=0x%llx",
						__FUNCTION__,
						psSyncCheckpointInt->ui32UID,
						(void*)psSyncCheckpointInt,
						(void*)psSyncCheckpointInt->psSyncCheckpointBlock->psContext->psSubAllocRA,
						psSyncCheckpointInt->uiSpanAddr));
#endif
				_FreeSyncCheckpoint(psSyncCheckpointInt);
			}
		}
#if (ENABLE_SYNC_CHECKPOINT_DEFERRED_CLEANUP_DEBUG == 1)
		else
		{
			PVR_DPF((PVR_DBG_WARNING,
					"%s psSyncCheckpoint '%s'' (ID:%d)<%p>), still pending (enq=%d,FWRef=%d)",
					__FUNCTION__,
					psSyncCheckpointInt->azName,
					psSyncCheckpointInt->ui32UID,
					(void*)psSyncCheckpointInt,
					(IMG_UINT32)(OSAtomicRead(&psSyncCheckpointInt->hEnqueuedCCBCount)),
					psSyncCheckpointInt->psSyncCheckpointFwObj->ui32FwRefCount));
		}
#endif
	}
	OSLockRelease(psContext->psContextCtl->hDeferredCleanupListLock);
}

#if (SYNC_CHECKPOINT_POOL_SIZE > 0)
static _SYNC_CHECKPOINT *_GetCheckpointFromPool(_SYNC_CHECKPOINT_CONTEXT *psContext)
{
	_SYNC_CHECKPOINT *psSyncCheckpoint = NULL;

	/* Acquire sync checkpoint pool lock */
	OSLockAcquire(psContext->psContextCtl->hSyncCheckpointPoolLock);

	/* Check if pool has anything in it */
	if (psContext->psContextCtl->bSyncCheckpointPoolValid && (psContext->psContextCtl->ui32SyncCheckpointPoolCount > 0) &&
			(psContext->psContextCtl->ui32SyncCheckpointPoolWp != psContext->psContextCtl->ui32SyncCheckpointPoolRp))
	{
		/* Get the next sync checkpoint from the pool */
		psSyncCheckpoint = psContext->psContextCtl->psSyncCheckpointPool[psContext->psContextCtl->ui32SyncCheckpointPoolRp++];
		if (psContext->psContextCtl->ui32SyncCheckpointPoolRp == SYNC_CHECKPOINT_POOL_SIZE)
		{
			psContext->psContextCtl->ui32SyncCheckpointPoolRp = 0;
		}
		psContext->psContextCtl->ui32SyncCheckpointPoolCount--;
		psContext->psContextCtl->bSyncCheckpointPoolFull = IMG_FALSE;
		psSyncCheckpoint->ui32ValidationCheck = SYNC_CHECKPOINT_PATTERN_IN_USE;
#if (ENABLE_SYNC_CHECKPOINT_POOL_DEBUG == 1)
		PVR_DPF((PVR_DBG_WARNING,
				"%s checkpoint(old ID:%d)<-POOL(%d/%d), psContext=<%p>, poolRp=%d, poolWp=%d",
				__FUNCTION__,
				psSyncCheckpoint->ui32UID,
				psContext->psContextCtl->ui32SyncCheckpointPoolCount,
				SYNC_CHECKPOINT_POOL_SIZE,
				(void*)psContext, psContext->psContextCtl->ui32SyncCheckpointPoolRp, psContext->psContextCtl->ui32SyncCheckpointPoolWp));
#endif
	}
	/* Release sync checkpoint pool lock */
	OSLockRelease(psContext->psContextCtl->hSyncCheckpointPoolLock);

	return psSyncCheckpoint;
}

static IMG_BOOL _PutCheckpointInPool(_SYNC_CHECKPOINT *psSyncCheckpoint)
{
	IMG_BOOL bReturnedToPool = IMG_FALSE;
	_SYNC_CHECKPOINT_CONTEXT *psContext = psSyncCheckpoint->psSyncCheckpointBlock->psContext;

	/* Acquire sync checkpoint pool lock */
	OSLockAcquire(psContext->psContextCtl->hSyncCheckpointPoolLock);

	/* Check if pool has space */
	if (psContext->psContextCtl->bSyncCheckpointPoolValid &&
			!psContext->psContextCtl->bSyncCheckpointPoolFull)
	{
		/* Put the sync checkpoint into the next write slot in the pool */
		psContext->psContextCtl->psSyncCheckpointPool[psContext->psContextCtl->ui32SyncCheckpointPoolWp++] = psSyncCheckpoint;
		if (psContext->psContextCtl->ui32SyncCheckpointPoolWp == SYNC_CHECKPOINT_POOL_SIZE)
		{
			psContext->psContextCtl->ui32SyncCheckpointPoolWp = 0;
		}
		psContext->psContextCtl->ui32SyncCheckpointPoolCount++;
		psContext->psContextCtl->bSyncCheckpointPoolFull =
		        ((psContext->psContextCtl->ui32SyncCheckpointPoolCount > 0) &&
		         (psContext->psContextCtl->ui32SyncCheckpointPoolWp == psContext->psContextCtl->ui32SyncCheckpointPoolRp));
		bReturnedToPool = IMG_TRUE;
		psSyncCheckpoint->ui32ValidationCheck = SYNC_CHECKPOINT_PATTERN_IN_POOL;
#if (ENABLE_SYNC_CHECKPOINT_POOL_DEBUG == 1)
		PVR_DPF((PVR_DBG_WARNING,
				"%s checkpoint(ID:%d)->POOL(%d/%d), poolRp=%d, poolWp=%d",
				__FUNCTION__,
				psSyncCheckpoint->ui32UID,
				psContext->psContextCtl->ui32SyncCheckpointPoolCount,
				SYNC_CHECKPOINT_POOL_SIZE, psContext->psContextCtl->ui32SyncCheckpointPoolRp, psContext->psContextCtl->ui32SyncCheckpointPoolWp));
#endif
	}
	/* Release sync checkpoint pool lock */
	OSLockRelease(psContext->psContextCtl->hSyncCheckpointPoolLock);

	return bReturnedToPool;
}

static IMG_UINT32 _CleanCheckpointPool(_SYNC_CHECKPOINT_CONTEXT *psContext)
{
	_SYNC_CHECKPOINT *psSyncCheckpointInt = NULL;
	IMG_UINT32 ui32ItemsFreed = 0;

	/* Acquire sync checkpoint pool lock */
	OSLockAcquire(psContext->psContextCtl->hSyncCheckpointPoolLock);

#if (ENABLE_SYNC_CHECKPOINT_POOL_DEBUG == 1)
	PVR_DPF((PVR_DBG_WARNING,
			"%s psContext=<%p>, bSyncCheckpointPoolValid=%d, uiSyncCheckpointPoolCount=%d",
			__FUNCTION__,
			(void*)psContext,
			psContext->psContextCtl->bSyncCheckpointPoolValid,
			psContext->psContextCtl->ui32SyncCheckpointPoolCount));
#endif
	/* While the pool still contains sync checkpoints, free them */
	while (psContext->psContextCtl->bSyncCheckpointPoolValid &&
			(psContext->psContextCtl->ui32SyncCheckpointPoolCount > 0))
	{
		/* Get the sync checkpoint from the next read slot in the pool */
		psSyncCheckpointInt = psContext->psContextCtl->psSyncCheckpointPool[psContext->psContextCtl->ui32SyncCheckpointPoolRp++];
		if (psContext->psContextCtl->ui32SyncCheckpointPoolRp == SYNC_CHECKPOINT_POOL_SIZE)
		{
			psContext->psContextCtl->ui32SyncCheckpointPoolRp = 0;
		}
		psContext->psContextCtl->ui32SyncCheckpointPoolCount--;
		psContext->psContextCtl->bSyncCheckpointPoolFull =
		         ((psContext->psContextCtl->ui32SyncCheckpointPoolCount > 0) &&
		         (psContext->psContextCtl->ui32SyncCheckpointPoolWp == psContext->psContextCtl->ui32SyncCheckpointPoolRp));

		if (psSyncCheckpointInt)
		{
			if (psSyncCheckpointInt->ui32ValidationCheck != SYNC_CHECKPOINT_PATTERN_IN_POOL)
			{
#if (ENABLE_SYNC_CHECKPOINT_POOL_DEBUG == 1)
				PVR_DPF((PVR_DBG_WARNING,
						"%s pool contains invalid entry (ui32ValidationCheck=0x%x)",
						__FUNCTION__,
						psSyncCheckpointInt->ui32ValidationCheck));
#endif
			}

#if (ENABLE_SYNC_CHECKPOINT_POOL_DEBUG == 1)
			PVR_DPF((PVR_DBG_WARNING, "%s psSyncCheckpoint(ID:%d)", __FUNCTION__, psSyncCheckpointInt->ui32UID));
			PVR_DPF((PVR_DBG_WARNING, "%s psSyncCheckpoint->ui32ValidationCheck=0x%x", __FUNCTION__, psSyncCheckpointInt->ui32ValidationCheck));
			PVR_DPF((PVR_DBG_WARNING, "%s psSyncCheckpoint->uiSpanAddr=0x%llx", __FUNCTION__, psSyncCheckpointInt->uiSpanAddr));
			PVR_DPF((PVR_DBG_WARNING, "%s psSyncCheckpoint->psSyncCheckpointBlock=<%p>", __FUNCTION__, (void*)psSyncCheckpointInt->psSyncCheckpointBlock));
			PVR_DPF((PVR_DBG_WARNING, "%s psSyncCheckpoint->psSyncCheckpointBlock->psContext=<%p>", __FUNCTION__, (void*)psSyncCheckpointInt->psSyncCheckpointBlock->psContext));
			PVR_DPF((PVR_DBG_WARNING, "%s psSyncCheckpoint->psSyncCheckpointBlock->psContext->psSubAllocRA=<%p>", __FUNCTION__, (void*)psSyncCheckpointInt->psSyncCheckpointBlock->psContext->psSubAllocRA));
#endif
			OSAtomicDecrement(&psContext->hCheckpointCount);
			psSyncCheckpointInt->ui32ValidationCheck = SYNC_CHECKPOINT_PATTERN_FREED;
#if (ENABLE_SYNC_CHECKPOINT_POOL_DEBUG == 1)
			PVR_DPF((PVR_DBG_WARNING,
					"%s CALLING RA_Free(psSyncCheckpoint(ID:%d)<%p>), psSubAllocRA=<%p>, ui32SpanAddr=0x%llx",
					__FUNCTION__,
					psSyncCheckpointInt->ui32UID,
					(void*)psSyncCheckpointInt,
					(void*)psSyncCheckpointInt->psSyncCheckpointBlock->psContext->psSubAllocRA,
					psSyncCheckpointInt->uiSpanAddr));
#endif
			RA_Free(psSyncCheckpointInt->psSyncCheckpointBlock->psContext->psSubAllocRA,
			        psSyncCheckpointInt->uiSpanAddr);
			ui32ItemsFreed++;
		}
		else
		{
#if (ENABLE_SYNC_CHECKPOINT_POOL_DEBUG == 1)
			PVR_DPF((PVR_DBG_WARNING, "%s pool contains NULL entry", __FUNCTION__));
#endif
		}
	}
	/* Release sync checkpoint pool lock */
	OSLockRelease(psContext->psContextCtl->hSyncCheckpointPoolLock);

	return ui32ItemsFreed;
}
#endif /* (SYNC_CHECKPOINT_POOL_SIZE > 0) */
