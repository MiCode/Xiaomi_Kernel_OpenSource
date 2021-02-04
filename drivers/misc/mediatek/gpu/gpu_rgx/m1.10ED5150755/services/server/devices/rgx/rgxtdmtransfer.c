/*************************************************************************/ /*!
@File           rgxtdmtransfer.c
@Title          Device specific TDM transfer queue routines
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

#include "pdump_km.h"
#include "rgxdevice.h"
#include "rgxccb.h"
#include "rgxutils.h"
#include "rgxfwutils.h"
#include "rgxtdmtransfer.h"
#include "rgx_tq_shared.h"
#include "rgxmem.h"
#include "allocmem.h"
#include "devicemem.h"
#include "devicemem_pdump.h"
#include "osfunc.h"
#include "pvr_debug.h"
#include "pvrsrv.h"
#include "rgx_fwif_resetframework.h"
#include "rgx_memallocflags.h"
#include "rgxtimerquery.h"
#include "rgxhwperf.h"
#include "htbuffer.h"

#include "pdump_km.h"

#include "sync_server.h"
#include "sync_internal.h"
#include "sync.h"

#if defined(SUPPORT_BUFFER_SYNC)
#include "pvr_buffer_sync.h"
#endif

/* Enable this to dump the compiled list of UFOs prior to kick call */
#define ENABLE_TDM_UFO_DUMP	0

//#define TDM_CHECKPOINT_DEBUG 1

#if defined(TDM_CHECKPOINT_DEBUG)
#define CHKPT_DBG(X) PVR_DPF(X)
#else
#define CHKPT_DBG(X)
#endif

typedef struct {
	RGX_SERVER_COMMON_CONTEXT * psServerCommonContext;
	IMG_UINT32                  ui32Priority;
#if defined(SUPPORT_BUFFER_SYNC)
	struct pvr_buffer_sync_context *psBufferSyncContext;
#endif
} RGX_SERVER_TQ_TDM_DATA;


struct _RGX_SERVER_TQ_TDM_CONTEXT_ {
	PVRSRV_DEVICE_NODE      *psDeviceNode;
	DEVMEM_MEMDESC          *psFWFrameworkMemDesc;
	IMG_UINT32              ui32Flags;
	RGX_SERVER_TQ_TDM_DATA  sTDMData;
	PVRSRV_CLIENT_SYNC_PRIM *psCleanupSync;
	DLLIST_NODE             sListNode;
	SYNC_ADDR_LIST          sSyncAddrListFence;
	SYNC_ADDR_LIST          sSyncAddrListUpdate;
	ATOMIC_T                hIntJobRef;
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	POS_LOCK		hLock;
#endif
};

static PVRSRV_ERROR _CreateTDMTransferContext(
		CONNECTION_DATA         * psConnection,
		PVRSRV_DEVICE_NODE      * psDeviceNode,
		DEVMEM_MEMDESC          * psFWMemContextMemDesc,
		IMG_UINT32                ui32Priority,
		RGX_COMMON_CONTEXT_INFO * psInfo,
		RGX_SERVER_TQ_TDM_DATA  * psTDMData)
{
	PVRSRV_ERROR eError;

#if defined(SUPPORT_BUFFER_SYNC)
	psTDMData->psBufferSyncContext =
			pvr_buffer_sync_context_create(psDeviceNode->psDevConfig->pvOSDevice,
			                               "rogue-tdm");
	if (IS_ERR(psTDMData->psBufferSyncContext))
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: failed to create buffer_sync context (err=%ld)",
				__func__, PTR_ERR(psTDMData->psBufferSyncContext)));

		eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto fail_buffer_sync_context_create;
	}
#endif

	eError = FWCommonContextAllocate(
			psConnection,
			psDeviceNode,
			REQ_TYPE_TQ_TDM,
			RGXFWIF_DM_TDM,
			NULL,
			0,
			psFWMemContextMemDesc,
			NULL,
			RGX_TQ2D_CCB_SIZE_LOG2,
			ui32Priority,
			psInfo,
			&psTDMData->psServerCommonContext);
	if (eError != PVRSRV_OK)
	{
		goto fail_contextalloc;
	}

	psTDMData->ui32Priority = ui32Priority;
	return PVRSRV_OK;

	fail_contextalloc:
#if defined(SUPPORT_BUFFER_SYNC)
	pvr_buffer_sync_context_destroy(psTDMData->psBufferSyncContext);
	psTDMData->psBufferSyncContext = NULL;
	fail_buffer_sync_context_create:
#endif
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}


static PVRSRV_ERROR _DestroyTDMTransferContext(
		RGX_SERVER_TQ_TDM_DATA  * psTDMData,
		PVRSRV_DEVICE_NODE      * psDeviceNode,
		PVRSRV_CLIENT_SYNC_PRIM * psCleanupSync)
{
	PVRSRV_ERROR eError;

	/* Check if the FW has finished with this resource ... */
	eError = RGXFWRequestCommonContextCleanUp(
			psDeviceNode,
			psTDMData->psServerCommonContext,
			psCleanupSync,
			RGXFWIF_DM_TDM,
			PDUMP_FLAGS_NONE);
	if (eError == PVRSRV_ERROR_RETRY)
	{
		return eError;
	}
	else if (eError != PVRSRV_OK)
	{
		PVR_LOG(("%s: Unexpected error from RGXFWRequestCommonContextCleanUp (%s)",
				__func__,
				PVRSRVGetErrorStringKM(eError)));
		return eError;
	}

	/* ... it has so we can free it's resources */
	FWCommonContextFree(psTDMData->psServerCommonContext);

#if defined(SUPPORT_BUFFER_SYNC)
	pvr_buffer_sync_context_destroy(psTDMData->psBufferSyncContext);
	psTDMData->psBufferSyncContext = NULL;
#endif

	return PVRSRV_OK;
}

/*
 * PVRSRVCreateTransferContextKM
 */
PVRSRV_ERROR PVRSRVRGXTDMCreateTransferContextKM(
		CONNECTION_DATA            * psConnection,
		PVRSRV_DEVICE_NODE         * psDeviceNode,
		IMG_UINT32                   ui32Priority,
		IMG_UINT32                   ui32FrameworkCommandSize,
		IMG_PBYTE                    pabyFrameworkCommand,
		IMG_HANDLE                   hMemCtxPrivData,
		RGX_SERVER_TQ_TDM_CONTEXT ** ppsTransferContext)
{
	RGX_SERVER_TQ_TDM_CONTEXT * psTransferContext;

	DEVMEM_MEMDESC          * psFWMemContextMemDesc = RGXGetFWMemDescFromMemoryContextHandle(hMemCtxPrivData);
	RGX_COMMON_CONTEXT_INFO   sInfo;
	PVRSRV_ERROR              eError = PVRSRV_OK;
	PVRSRV_RGXDEV_INFO        *psDevInfo = psDeviceNode->pvDevice;

	/* Allocate the server side structure */
	*ppsTransferContext = NULL;
	psTransferContext = OSAllocZMem(sizeof(*psTransferContext));
	if (psTransferContext == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	eError = OSLockCreate(&psTransferContext->hLock, LOCK_TYPE_NONE);

	if(eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create lock (%s)",
				__func__,
				PVRSRVGetErrorStringKM(eError)));
		goto fail_lockcreate;
	}
#endif

	psTransferContext->psDeviceNode = psDeviceNode;

	/* Allocate cleanup sync */
	eError = SyncPrimAlloc(psDeviceNode->hSyncPrimContext,
	                       &psTransferContext->psCleanupSync,
	                       "transfer context cleanup");
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVCreateTransferContextKM: Failed to allocate cleanup sync (0x%x)",
				eError));
		goto fail_syncalloc;
	}

	/* 
	 * Create the FW framework buffer
	 */
	eError = PVRSRVRGXFrameworkCreateKM(psDeviceNode,
	                                    &psTransferContext->psFWFrameworkMemDesc,
	                                    ui32FrameworkCommandSize);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVCreateTransferContextKM: Failed to allocate firmware GPU framework state (%u)",
				eError));
		goto fail_frameworkcreate;
	}

	/* Copy the Framework client data into the framework buffer */
	eError = PVRSRVRGXFrameworkCopyCommand(psTransferContext->psFWFrameworkMemDesc,
	                                       pabyFrameworkCommand,
	                                       ui32FrameworkCommandSize);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVCreateTransferContextKM: Failed to populate the framework buffer (%u)",
				eError));
		goto fail_frameworkcopy;
	}

	sInfo.psFWFrameworkMemDesc = psTransferContext->psFWFrameworkMemDesc;

	eError = _CreateTDMTransferContext(psConnection,
	                                   psDeviceNode,
	                                   psFWMemContextMemDesc,
	                                   ui32Priority,
	                                   &sInfo,
	                                   &psTransferContext->sTDMData);
	if (eError != PVRSRV_OK)
	{
		goto fail_tdmtransfercontext;
	}

	SyncAddrListInit(&psTransferContext->sSyncAddrListFence);
	SyncAddrListInit(&psTransferContext->sSyncAddrListUpdate);

	{
		OSWRLockAcquireWrite(psDevInfo->hTDMCtxListLock);
		dllist_add_to_tail(&(psDevInfo->sTDMCtxtListHead), &(psTransferContext->sListNode));
		OSWRLockReleaseWrite(psDevInfo->hTDMCtxListLock);
		*ppsTransferContext = psTransferContext;
	}

	*ppsTransferContext = psTransferContext;

	return PVRSRV_OK;

	fail_tdmtransfercontext:
	fail_frameworkcopy:
	DevmemFwFree(psDevInfo, psTransferContext->psFWFrameworkMemDesc);
	fail_frameworkcreate:
	SyncPrimFree(psTransferContext->psCleanupSync);
	fail_syncalloc:
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockDestroy(psTransferContext->hLock);
	fail_lockcreate:
#endif
	OSFreeMem(psTransferContext);
	PVR_ASSERT(eError != PVRSRV_OK);
	*ppsTransferContext = NULL;
	return eError;
}

PVRSRV_ERROR PVRSRVRGXTDMDestroyTransferContextKM(RGX_SERVER_TQ_TDM_CONTEXT *psTransferContext)
{
	PVRSRV_ERROR eError;
	PVRSRV_RGXDEV_INFO *psDevInfo = psTransferContext->psDeviceNode->pvDevice;

	/* remove node from list before calling destroy - as destroy, if successful
	 * will invalidate the node
	 * must be re-added if destroy fails
	 */
	OSWRLockAcquireWrite(psDevInfo->hTDMCtxListLock);
	dllist_remove_node(&(psTransferContext->sListNode));
	OSWRLockReleaseWrite(psDevInfo->hTDMCtxListLock);


	eError = _DestroyTDMTransferContext(&psTransferContext->sTDMData,
	                                    psTransferContext->psDeviceNode,
	                                    psTransferContext->psCleanupSync);
	if (eError != PVRSRV_OK)
	{
		goto fail_destroyTDM;
	}

	DevmemFwFree(psDevInfo, psTransferContext->psFWFrameworkMemDesc);
	SyncPrimFree(psTransferContext->psCleanupSync);

	SyncAddrListDeinit(&psTransferContext->sSyncAddrListFence);
	SyncAddrListDeinit(&psTransferContext->sSyncAddrListUpdate);

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockDestroy(psTransferContext->hLock);
#endif

	OSFreeMem(psTransferContext);

	return PVRSRV_OK;

	fail_destroyTDM:

	OSWRLockAcquireWrite(psDevInfo->hTDMCtxListLock);
	dllist_add_to_tail(&(psDevInfo->sTDMCtxtListHead), &(psTransferContext->sListNode));
	OSWRLockReleaseWrite(psDevInfo->hTDMCtxListLock);
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}


/*
 * PVRSRVSubmitTQ3DKickKM
 */
PVRSRV_ERROR PVRSRVRGXTDMSubmitTransferKM(
	RGX_SERVER_TQ_TDM_CONTEXT * psTransferContext,
	IMG_UINT32                  ui32PDumpFlags,
	IMG_UINT32                  ui32ClientCacheOpSeqNum,
	IMG_UINT32                  ui32ClientFenceCount,
	SYNC_PRIMITIVE_BLOCK     ** pauiClientFenceUFOSyncPrimBlock,
	IMG_UINT32                * paui32ClientFenceSyncOffset,
	IMG_UINT32                * paui32ClientFenceValue,
	IMG_UINT32                  ui32ClientUpdateCount,
	SYNC_PRIMITIVE_BLOCK     ** pauiClientUpdateUFOSyncPrimBlock,
	IMG_UINT32                * paui32ClientUpdateSyncOffset,
	IMG_UINT32                * paui32ClientUpdateValue,
	IMG_UINT32                  ui32ServerSyncCount,
	IMG_UINT32                * paui32ServerSyncFlags,
	SERVER_SYNC_PRIMITIVE    ** papsServerSyncs,
	PVRSRV_FENCE                iCheckFence,
	PVRSRV_TIMELINE             iUpdateTimeline,
	PVRSRV_FENCE              * piUpdateFence,
	IMG_CHAR                    szUpdateFenceName[32],
	IMG_UINT32                  ui32FWCommandSize,
	IMG_UINT8                 * pui8FWCommand,
	IMG_UINT32                  ui32ExtJobRef,
	IMG_UINT32                  ui32SyncPMRCount,
	IMG_UINT32                * paui32SyncPMRFlags,
	PMR                      ** ppsSyncPMRs)
{
	PVRSRV_DEVICE_NODE *psDeviceNode = psTransferContext->psDeviceNode;
	RGX_CCB_CMD_HELPER_DATA *psCmdHelper;
	PRGXFWIF_UFO_ADDR * pauiIntFenceUFOAddress   = NULL;
	PRGXFWIF_UFO_ADDR * pauiIntUpdateUFOAddress  = NULL;
	IMG_UINT32        * paui32IntFenceValue      = paui32ClientFenceValue;
	IMG_UINT32          ui32IntClientFenceCount  = ui32ClientFenceCount;
	IMG_UINT32        * paui32IntUpdateValue     = paui32ClientUpdateValue;
	IMG_UINT32          ui32IntClientUpdateCount = ui32ClientUpdateCount;
	PVRSRV_ERROR eError;
	PVRSRV_ERROR eError2;
	PVRSRV_FENCE iUpdateFence = PVRSRV_NO_FENCE;
	IMG_UINT32 ui32IntJobRef;

	IMG_UINT32 ui32CmdOffset = 0;
	IMG_BOOL bCCBStateOpen;

	PRGXFWIF_TIMESTAMP_ADDR pPreAddr;
	PRGXFWIF_TIMESTAMP_ADDR pPostAddr;
	PRGXFWIF_UFO_ADDR       pRMWUFOAddr;
	IMG_UINT64               uiCheckFenceUID = 0;
	IMG_UINT64               uiUpdateFenceUID = 0;
#if defined(SUPPORT_BUFFER_SYNC)
	struct pvr_buffer_sync_append_data *psBufferSyncData = NULL;
	PSYNC_CHECKPOINT *apsBufferFenceSyncCheckpoints = NULL;
	IMG_UINT32 ui32BufferFenceSyncCheckpointCount = 0;
	PSYNC_CHECKPOINT psBufferUpdateSyncCheckpoint = NULL;
#endif

#if defined(PVR_USE_FENCE_SYNC_MODEL)
	PSYNC_CHECKPOINT psUpdateSyncCheckpoint = NULL;
	PSYNC_CHECKPOINT *apsFenceSyncCheckpoints = NULL;
	IMG_UINT32 ui32FenceSyncCheckpointCount = 0;
	IMG_UINT32 *pui32IntAllocatedUpdateValues = NULL;
	PVRSRV_CLIENT_SYNC_PRIM *psFenceTimelineUpdateSync = NULL;
	IMG_UINT32 ui32FenceTimelineUpdateValue = 0;
	void *pvUpdateFenceFinaliseData = NULL;
#endif /* defined(PVR_USE_FENCE_SYNC_MODEL) */
	IMG_DEV_VIRTADDR sRobustnessResetReason = {0};

#if defined(PVR_USE_FENCE_SYNC_MODEL)
	if (iUpdateTimeline >= 0 && !piUpdateFence)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
#else /* defined(PVR_USE_FENCE_SYNC_MODEL) */
	if (iUpdateTimeline >= 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Providing update timeline (%d) in non-supporting driver",
				__func__, iUpdateTimeline));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	if (iCheckFence >= 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Providing check fence (%d) in non-supporting driver",
				__func__, iCheckFence));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
#endif /* defined(PVR_USE_FENCE_SYNC_MODEL) */

	/* Ensure the string is null-terminated (Required for safety) */
	szUpdateFenceName[31] = '\0';

	if (ui32SyncPMRCount != 0)
	{
		if (!ppsSyncPMRs)
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockAcquire(psTransferContext->hLock);
#endif

	ui32IntJobRef = OSAtomicIncrement(&psTransferContext->hIntJobRef);

	/* We can't allocate the required amount of stack space on all consumer architectures */
	psCmdHelper = OSAllocMem(sizeof(RGX_CCB_CMD_HELPER_DATA));
	if (psCmdHelper == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto fail_allochelper;
	}


	/*
		Init the command helper commands for all the prepares
	 */
	{
		RGX_CLIENT_CCB *psClientCCB;
		RGX_SERVER_COMMON_CONTEXT *psServerCommonCtx;
		IMG_CHAR *pszCommandName;
		RGXFWIF_CCB_CMD_TYPE eType;
#if defined(SUPPORT_BUFFER_SYNC)
		struct pvr_buffer_sync_context *psBufferSyncContext;
#endif

		psServerCommonCtx = psTransferContext->sTDMData.psServerCommonContext;
		psClientCCB = FWCommonContextGetClientCCB(psServerCommonCtx);
		pszCommandName = "TQ-TDM";

		if (ui32FWCommandSize == 0)
		{
			/* A NULL CMD for TDM is used to append updates to a non finished
			 * FW command. bCCBStateOpen is used in case capture range is 
			 * entered on this command, to not drain CCB up to the Roff for this 
			 * command, but the finished command prior to this.
			 */
			bCCBStateOpen = IMG_TRUE;
			eType = RGXFWIF_CCB_CMD_TYPE_NULL;
		}
		else
		{
			bCCBStateOpen = IMG_FALSE;
			eType = RGXFWIF_CCB_CMD_TYPE_TQ_TDM;
		}
#if defined(SUPPORT_BUFFER_SYNC)
		psBufferSyncContext = psTransferContext->sTDMData.psBufferSyncContext;
#endif

		eError = SyncAddrListPopulate(&psTransferContext->sSyncAddrListFence,
		                              ui32ClientFenceCount,
		                              pauiClientFenceUFOSyncPrimBlock,
		                              paui32ClientFenceSyncOffset);
		if(eError != PVRSRV_OK)
		{
			goto fail_populate_sync_addr_list;
		}
		paui32IntFenceValue      = paui32ClientFenceValue;
		pauiIntFenceUFOAddress = psTransferContext->sSyncAddrListFence.pasFWAddrs;

		eError = SyncAddrListPopulate(&psTransferContext->sSyncAddrListUpdate,
		                              ui32ClientUpdateCount,
		                              pauiClientUpdateUFOSyncPrimBlock,
		                              paui32ClientUpdateSyncOffset);
		if(eError != PVRSRV_OK)
		{
			goto fail_populate_sync_addr_list;
		}
		paui32IntUpdateValue     = paui32ClientUpdateValue;
		pauiIntUpdateUFOAddress = psTransferContext->sSyncAddrListUpdate.pasFWAddrs;


		if (ui32SyncPMRCount)
		{
#if defined(SUPPORT_BUFFER_SYNC)
			int err;

			CHKPT_DBG((PVR_DBG_ERROR, "%s:   Calling pvr_buffer_sync_resolve_and_create_fences", __func__));
			err = pvr_buffer_sync_resolve_and_create_fences(psBufferSyncContext,
			                                                ui32SyncPMRCount,
			                                                ppsSyncPMRs,
			                                                paui32SyncPMRFlags,
			                                                &ui32BufferFenceSyncCheckpointCount,
			                                                &apsBufferFenceSyncCheckpoints,
			                                                &psBufferUpdateSyncCheckpoint,
			                                                &psBufferSyncData);
			if (err)
			{
				eError = (err == -ENOMEM) ? PVRSRV_ERROR_OUT_OF_MEMORY : PVRSRV_ERROR_INVALID_PARAMS;
				PVR_DPF((PVR_DBG_ERROR, "%s:   pvr_buffer_sync_resolve_and_create_fences failed (%d)", __func__, eError));
				goto fail_resolve_input_fence;
			}

			/* Append buffer sync fences */
			if (ui32BufferFenceSyncCheckpointCount > 0)
			{
				CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append %d buffer sync checkpoints to TQ Fence (&psTransferContext->sSyncAddrListFence=<%p>, pauiIntFenceUFOAddress=<%p>)...", __func__, ui32BufferFenceSyncCheckpointCount, (void*)&psTransferContext->sSyncAddrListFence , (void*)pauiIntFenceUFOAddress));
				SyncAddrListAppendAndDeRefCheckpoints(&psTransferContext->sSyncAddrListFence,
				                                      ui32BufferFenceSyncCheckpointCount,
				                                      apsBufferFenceSyncCheckpoints);
				if (!pauiIntFenceUFOAddress)
				{
					pauiIntFenceUFOAddress = psTransferContext->sSyncAddrListFence.pasFWAddrs;
				}
				ui32IntClientFenceCount += ui32BufferFenceSyncCheckpointCount;
			}

			if (psBufferUpdateSyncCheckpoint)
			{
				/* Append the update (from output fence) */
				SyncAddrListAppendCheckpoints(&psTransferContext->sSyncAddrListUpdate,
				                              1,
				                              &psBufferUpdateSyncCheckpoint);
				if (!pauiIntUpdateUFOAddress)
				{
					pauiIntUpdateUFOAddress = psTransferContext->sSyncAddrListUpdate.pasFWAddrs;
				}
				ui32IntClientUpdateCount++;
			}
#else /* defined(SUPPORT_BUFFER_SYNC) */
			PVR_DPF((PVR_DBG_ERROR, "%s: Buffer sync not supported but got %u buffers", __func__, ui32SyncPMRCount));
			eError = PVRSRV_ERROR_INVALID_PARAMS;
			goto fail_populate_sync_addr_list;
#endif /* defined(SUPPORT_BUFFER_SYNC) */
		}

#if defined(PVR_USE_FENCE_SYNC_MODEL)
		/* Resolve the sync checkpoints that make up the input fence */
		eError = SyncCheckpointResolveFence(psTransferContext->psDeviceNode->hSyncCheckpointContext,
		                                    iCheckFence,
		                                    &ui32FenceSyncCheckpointCount,
		                                    &apsFenceSyncCheckpoints,
		                                    &uiCheckFenceUID);
		if (eError != PVRSRV_OK)
		{
			goto fail_resolve_input_fence;
		}
#if defined(TDM_CHECKPOINT_DEBUG)
		{
			IMG_UINT32 ii;
			for (ii=0; ii<32; ii++)
			{
				PSYNC_CHECKPOINT psNextCheckpoint = *(apsFenceSyncCheckpoints +  ii);
				CHKPT_DBG((PVR_DBG_ERROR, "%s:    apsFenceSyncCheckpoints[%d]=<%p>", __func__, ii, (void*)psNextCheckpoint)); //psFenceSyncCheckpoints[ii]));
			}
		}
#endif
		/* Create the output fence (if required) */
		if (iUpdateTimeline != PVRSRV_NO_TIMELINE)
		{
			eError = SyncCheckpointCreateFence(psTransferContext->psDeviceNode,
			                                   szUpdateFenceName,
			                                   iUpdateTimeline,
			                                   psTransferContext->psDeviceNode->hSyncCheckpointContext,
			                                   &iUpdateFence,
			                                   &uiUpdateFenceUID,
			                                   &pvUpdateFenceFinaliseData,
			                                   &psUpdateSyncCheckpoint,
			                                   (void*)&psFenceTimelineUpdateSync,
			                                   &ui32FenceTimelineUpdateValue);
			if (eError != PVRSRV_OK)
			{
				goto fail_create_output_fence;
			}

			/* Append the sync prim update for the timeline (if required) */
			if (psFenceTimelineUpdateSync)
			{
				IMG_UINT32 *pui32TimelineUpdateWp = NULL;

				/* Allocate memory to hold the list of update values (including our timeline update) */
				pui32IntAllocatedUpdateValues = OSAllocMem(sizeof(*pui32IntAllocatedUpdateValues) * (ui32IntClientUpdateCount+1));
				if (!pui32IntAllocatedUpdateValues)
				{
					/* Failed to allocate memory */
					eError = PVRSRV_ERROR_OUT_OF_MEMORY;
					goto fail_alloc_update_values_mem;
				}
				OSCachedMemSet(pui32IntAllocatedUpdateValues, 0xbb, sizeof(*pui32IntAllocatedUpdateValues) * (ui32IntClientUpdateCount+1));
				/* Copy the update values into the new memory, then append our timeline update value */
				OSCachedMemCopy(pui32IntAllocatedUpdateValues, paui32IntUpdateValue, sizeof(*pui32IntAllocatedUpdateValues) * ui32IntClientUpdateCount);
				/* Now set the additional update value */
				pui32TimelineUpdateWp = pui32IntAllocatedUpdateValues + ui32IntClientUpdateCount;
				*pui32TimelineUpdateWp = ui32FenceTimelineUpdateValue;
				ui32IntClientUpdateCount++;
#if defined(TDM_CHECKPOINT_DEBUG)
				{
					IMG_UINT32 iii;
					IMG_UINT32 *pui32Tmp = (IMG_UINT32*)pui32IntAllocatedUpdateValues;

					for (iii=0; iii<ui32IntClientUpdateCount; iii++)
					{
						CHKPT_DBG((PVR_DBG_ERROR, "%s: pui32IntAllocatedUpdateValues[%d](<%p>) = 0x%x", __func__, iii, (void*)pui32Tmp, *pui32Tmp));
						pui32Tmp++;
					}
				}
#endif
				/* Now append the timeline sync prim addr to the transfer context update list */
				SyncAddrListAppendSyncPrim(&psTransferContext->sSyncAddrListUpdate,
				                           psFenceTimelineUpdateSync);
#if defined(TDM_CHECKPOINT_DEBUG)
				{
					IMG_UINT32 iii;
					IMG_UINT32 *pui32Tmp = (IMG_UINT32*)pui32IntAllocatedUpdateValues;

					for (iii=0; iii<ui32IntClientUpdateCount; iii++)
					{
						CHKPT_DBG((PVR_DBG_ERROR, "%s: pui32IntAllocatedUpdateValues[%d](<%p>) = 0x%x", __func__, iii, (void*)pui32Tmp, *pui32Tmp));
						pui32Tmp++;
					}
				}
#endif
				/* Ensure paui32IntUpdateValue is now pointing to our new array of update values */
				paui32IntUpdateValue = pui32IntAllocatedUpdateValues;
			}
		}

		if (ui32FenceSyncCheckpointCount)
		{
			/* Append the checks (from input fence) */
			if (ui32FenceSyncCheckpointCount > 0)
			{
				CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append %d sync checkpoints to TQ Fence (&psTransferContext->sSyncAddrListFence=<%p>)...", __func__, ui32FenceSyncCheckpointCount, (void*)&psTransferContext->sSyncAddrListFence));
#if defined(TDM_CHECKPOINT_DEBUG)
				{
					IMG_UINT32 iii;
					IMG_UINT32 *pui32Tmp = (IMG_UINT32*)pauiIntFenceUFOAddress;

					for (iii=0; iii<ui32IntClientUpdateCount; iii++)
					{
						CHKPT_DBG((PVR_DBG_ERROR, "%s: pui32IntAllocatedUpdateValues[%d](<%p>) = 0x%x", __func__, iii, (void*)pui32Tmp, *pui32Tmp));
						pui32Tmp++;
					}
				}
#endif
				SyncAddrListAppendCheckpoints(&psTransferContext->sSyncAddrListFence,
				                              ui32FenceSyncCheckpointCount,
				                              apsFenceSyncCheckpoints);
				if (!pauiIntFenceUFOAddress)
				{
					pauiIntFenceUFOAddress = psTransferContext->sSyncAddrListFence.pasFWAddrs;
				}
				ui32IntClientFenceCount += ui32FenceSyncCheckpointCount;
			}
#if defined(TDM_CHECKPOINT_DEBUG)
			{
				IMG_UINT32 iii;
				IMG_UINT32 *pui32Tmp = (IMG_UINT32*)pui32IntAllocatedUpdateValues;

				for (iii=0; iii<ui32IntClientUpdateCount; iii++)
				{
					CHKPT_DBG((PVR_DBG_ERROR, "%s: pui32IntAllocatedUpdateValues[%d](<%p>) = 0x%x", __func__, iii, (void*)pui32Tmp, *pui32Tmp));
					pui32Tmp++;
				}
			}
#endif
		}
		if (psUpdateSyncCheckpoint)
		{
			/* Append the update (from output fence) */
			CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append 1 sync checkpoint to TQ Update (&psTransferContext->sSyncAddrListUpdate=<%p>, pauiIntUpdateUFOAddress=<%p>)...", __func__, (void*)&psTransferContext->sSyncAddrListUpdate , (void*)pauiIntUpdateUFOAddress));
			SyncAddrListAppendCheckpoints(&psTransferContext->sSyncAddrListUpdate,
			                              1,
			                              &psUpdateSyncCheckpoint);
			if (!pauiIntUpdateUFOAddress)
			{
				pauiIntUpdateUFOAddress = psTransferContext->sSyncAddrListUpdate.pasFWAddrs;
			}
			ui32IntClientUpdateCount++;
#if defined(TDM_CHECKPOINT_DEBUG)
			{
				IMG_UINT32 iii;
				IMG_UINT32 *pui32Tmp = (IMG_UINT32*)pui32IntAllocatedUpdateValues;

				for (iii=0; iii<ui32IntClientUpdateCount; iii++)
				{
					CHKPT_DBG((PVR_DBG_ERROR, "%s: pui32IntAllocatedUpdateValues[%d](<%p>) = 0x%x", __func__, iii, (void*)pui32Tmp, *pui32Tmp));
					pui32Tmp++;
				}
			}
#endif
		}
#endif /* defined(PVR_USE_FENCE_SYNC_MODEL) */

#if (ENABLE_TDM_UFO_DUMP == 1)
		PVR_DPF((PVR_DBG_ERROR, "%s: dumping TDM fence/updates syncs...", __func__));
		{
			IMG_UINT32 ii;
			PRGXFWIF_UFO_ADDR *psTmpIntFenceUFOAddress = pauiIntFenceUFOAddress;
			IMG_UINT32 *pui32TmpIntFenceValue = paui32IntFenceValue;
			PRGXFWIF_UFO_ADDR *psTmpIntUpdateUFOAddress = pauiIntUpdateUFOAddress;
			IMG_UINT32 *pui32TmpIntUpdateValue = paui32IntUpdateValue;

			/* Dump Fence syncs and Update syncs */
			PVR_DPF((PVR_DBG_ERROR, "%s: Prepared %d TDM fence syncs (&psTransferContext->sSyncAddrListFence=<%p>, pauiIntFenceUFOAddress=<%p>):", __func__, ui32IntClientFenceCount, (void*)&psTransferContext->sSyncAddrListFence, (void*)pauiIntFenceUFOAddress));
			for (ii=0; ii<ui32IntClientFenceCount; ii++)
			{
				if (psTmpIntFenceUFOAddress->ui32Addr & 0x1)
				{
					PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, CheckValue=PVRSRV_SYNC_CHECKPOINT_SIGNALLED", __func__, ii+1, ui32IntClientFenceCount, (void*)psTmpIntFenceUFOAddress, psTmpIntFenceUFOAddress->ui32Addr));
				}
				else
				{
					PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, CheckValue=%d(0x%x)", __func__, ii+1, ui32IntClientFenceCount, (void*)psTmpIntFenceUFOAddress, psTmpIntFenceUFOAddress->ui32Addr, *pui32TmpIntFenceValue, *pui32TmpIntFenceValue));
					pui32TmpIntFenceValue++;
				}
				psTmpIntFenceUFOAddress++;
			}
			PVR_DPF((PVR_DBG_ERROR, "%s: Prepared %d TDM update syncs (&psTransferContext->sSyncAddrListUpdate=<%p>, pauiIntUpdateUFOAddress=<%p>):", __func__, ui32IntClientUpdateCount, (void*)&psTransferContext->sSyncAddrListUpdate, (void*)pauiIntUpdateUFOAddress));
			for (ii=0; ii<ui32IntClientUpdateCount; ii++)
			{
				if (psTmpIntUpdateUFOAddress->ui32Addr & 0x1)
				{
					PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, UpdateValue=PVRSRV_SYNC_CHECKPOINT_SIGNALLED", __func__, ii+1, ui32IntClientUpdateCount, (void*)psTmpIntUpdateUFOAddress, psTmpIntUpdateUFOAddress->ui32Addr));
				}
				else
				{
					PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, UpdateValue=%d", __func__, ii+1, ui32IntClientUpdateCount, (void*)psTmpIntUpdateUFOAddress, psTmpIntUpdateUFOAddress->ui32Addr, *pui32TmpIntUpdateValue));
					pui32TmpIntUpdateValue++;
				}
				psTmpIntUpdateUFOAddress++;
			}
		}
#endif

		RGX_GetTimestampCmdHelper((PVRSRV_RGXDEV_INFO*) psTransferContext->psDeviceNode->pvDevice,
		                          & pPreAddr,
		                          & pPostAddr,
		                          & pRMWUFOAddr);

		/*
			Create the command helper data for this command
		 */
		eError = RGXCmdHelperInitCmdCCB(psClientCCB,
		                                ui32IntClientFenceCount,
		                                pauiIntFenceUFOAddress,
		                                paui32IntFenceValue,
		                                ui32IntClientUpdateCount,
		                                pauiIntUpdateUFOAddress,
		                                paui32IntUpdateValue,
		                                ui32ServerSyncCount,
		                                paui32ServerSyncFlags,
		                                SYNC_FLAG_MASK_ALL,
		                                papsServerSyncs,
		                                ui32FWCommandSize,
		                                pui8FWCommand,
		                                & pPreAddr,
		                                & pPostAddr,
		                                & pRMWUFOAddr,
		                                eType,
		                                ui32ExtJobRef,
		                                ui32IntJobRef,
		                                ui32PDumpFlags,
		                                NULL,
		                                pszCommandName,
		                                bCCBStateOpen,
		                                psCmdHelper,
		                                sRobustnessResetReason);
		if (eError != PVRSRV_OK)
		{
			goto fail_initcmd;
		}
	}

	/*
		Acquire space for all the commands in one go
	 */

	eError = RGXCmdHelperAcquireCmdCCB(1, psCmdHelper);
	if (eError != PVRSRV_OK)
	{
		goto fail_3dcmdacquire;
	}


	/*
		We should acquire the kernel CCB(s) space here as the schedule could fail
		and we would have to roll back all the syncs
	 */

	/*
		Only do the command helper release (which takes the server sync
		operations if the acquire succeeded
	 */
	ui32CmdOffset = RGXGetHostWriteOffsetCCB(FWCommonContextGetClientCCB(psTransferContext->sTDMData.psServerCommonContext));
	RGXCmdHelperReleaseCmdCCB(1,
	                          psCmdHelper,
	                          "TQ_TDM",
	                          FWCommonContextGetFWAddress(psTransferContext->sTDMData.psServerCommonContext).ui32Addr);


	/*
		Even if we failed to acquire the client CCB space we might still need
		to kick the HW to process a padding packet to release space for us next
		time round
	 */
	{
		RGXFWIF_KCCB_CMD sTDMKCCBCmd;

		/* Construct the kernel 3D CCB command. */
		sTDMKCCBCmd.eCmdType = RGXFWIF_KCCB_CMD_KICK;
		sTDMKCCBCmd.uCmdData.sCmdKickData.psContext = FWCommonContextGetFWAddress(psTransferContext->sTDMData.psServerCommonContext);
		sTDMKCCBCmd.uCmdData.sCmdKickData.ui32CWoffUpdate = RGXGetHostWriteOffsetCCB(FWCommonContextGetClientCCB(psTransferContext->sTDMData.psServerCommonContext));
		sTDMKCCBCmd.uCmdData.sCmdKickData.ui32NumCleanupCtl = 0;

		/* HTBLOGK(HTB_SF_MAIN_KICK_TDM, */
		/* 		s3DKCCBCmd.uCmdData.sCmdKickData.psContext, */
		/* 		ui323DCmdOffset); */
		RGX_HWPERF_HOST_ENQ(psTransferContext,
	                        OSGetCurrentClientProcessIDKM(),
		                    FWCommonContextGetFWAddress(psTransferContext->sTDMData.psServerCommonContext).ui32Addr,
		                    ui32ExtJobRef,
		                    ui32IntJobRef,
	                        RGX_HWPERF_KICK_TYPE_TQTDM,
		                    uiCheckFenceUID,
		                    uiUpdateFenceUID,
		                    NO_DEADLINE,
		                    NO_CYCEST);

		LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
		{
			eError2 = RGXScheduleCommand(psDeviceNode->pvDevice,
			                             RGXFWIF_DM_TDM,
			                             & sTDMKCCBCmd,
			                             sizeof(sTDMKCCBCmd),
			                             ui32ClientCacheOpSeqNum,
			                             ui32PDumpFlags);
			if (eError2 != PVRSRV_ERROR_RETRY)
			{
				break;
			}
			OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
		} END_LOOP_UNTIL_TIMEOUT();

#if defined(SUPPORT_GPUTRACE_EVENTS)
		RGXHWPerfFTraceGPUEnqueueEvent(psDeviceNode->pvDevice,
 			FWCommonContextGetFWAddress(psTransferContext->
 				sTDMData.psServerCommonContext).ui32Addr,
			ui32IntJobRef, RGX_HWPERF_KICK_TYPE_TQTDM);
#endif
	}

	/*
	 * Now check eError (which may have returned an error from our earlier calls
	 * to RGXCmdHelperAcquireCmdCCB) - we needed to process any flush command first
	 * so we check it now...
	 */
	if (eError != PVRSRV_OK )
	{
		goto fail_2dcmdacquire;
	}

#if defined(PVR_USE_FENCE_SYNC_MODEL)
#if defined(NO_HARDWARE)
	/* If NO_HARDWARE, signal the output fence's sync checkpoint and sync prim */
	if (psUpdateSyncCheckpoint)
	{
		SyncCheckpointSignalNoHW(psUpdateSyncCheckpoint);
	}
	if (psFenceTimelineUpdateSync)
	{
		SyncPrimNoHwUpdate(psFenceTimelineUpdateSync, ui32FenceTimelineUpdateValue);
	}
	SyncCheckpointNoHWUpdateTimelines(NULL);
#endif /* defined (NO_HARDWARE) */
#endif /* defined(PVR_USE_FENCE_SYNC_MODEL) */

#if defined(SUPPORT_BUFFER_SYNC)
	if (psBufferSyncData)
	{
		pvr_buffer_sync_kick_succeeded(psBufferSyncData);
	}
	if (apsBufferFenceSyncCheckpoints)
	{
		kfree(apsBufferFenceSyncCheckpoints);
	}
#endif /* defined(SUPPORT_BUFFER_SYNC) */

	* piUpdateFence = iUpdateFence;
#if defined(PVR_USE_FENCE_SYNC_MODEL)
	if (pvUpdateFenceFinaliseData && (iUpdateFence != PVRSRV_NO_FENCE))
	{
		SyncCheckpointFinaliseFence(iUpdateFence, pvUpdateFenceFinaliseData);
	}
#endif /* defined(PVR_USE_FENCE_SYNC_MODEL) */

	OSFreeMem(psCmdHelper);

#if defined(PVR_USE_FENCE_SYNC_MODEL)
	/* Drop the references taken on the sync checkpoints in the
	 * resolved input fence */
	SyncAddrListDeRefCheckpoints(ui32FenceSyncCheckpointCount,
	                             apsFenceSyncCheckpoints);
	/* Free the memory that was allocated for the sync checkpoint list returned by ResolveFence() */
	if (apsFenceSyncCheckpoints)
	{
		SyncCheckpointFreeCheckpointListMem(apsFenceSyncCheckpoints);
	}
	/* Free memory allocated to hold the internal list of update values */
	if (pui32IntAllocatedUpdateValues)
	{
		OSFreeMem(pui32IntAllocatedUpdateValues);
		pui32IntAllocatedUpdateValues = NULL;
	}
#endif /* defined(PVR_USE_FENCE_SYNC_MODEL) */

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockRelease(psTransferContext->hLock);
#endif
	return PVRSRV_OK;

	/*
	No resources are created in this function so there is nothing to free
	unless we had to merge syncs.
	If we fail after the client CCB acquire there is still nothing to do
	as only the client CCB release will modify the client CCB
	 */
	fail_2dcmdacquire:
	fail_3dcmdacquire:

	fail_initcmd:
#if defined(PVR_USE_FENCE_SYNC_MODEL)
	SyncAddrListRollbackCheckpoints(psTransferContext->psDeviceNode, &psTransferContext->sSyncAddrListFence);
	SyncAddrListRollbackCheckpoints(psTransferContext->psDeviceNode, &psTransferContext->sSyncAddrListUpdate);
	fail_alloc_update_values_mem:
#endif /* defined(PVR_USE_FENCE_SYNC_MODEL) */

	/* fail_pdumpcheck: */
	/* fail_cmdtype: */

#if defined(PVR_USE_FENCE_SYNC_MODEL)
	if(iUpdateFence != PVRSRV_NO_FENCE)
	{
		SyncCheckpointRollbackFenceData(iUpdateFence, pvUpdateFenceFinaliseData);
	}
	fail_create_output_fence:
	/* Drop the references taken on the sync checkpoints in the
	 * resolved input fence */
	SyncAddrListDeRefCheckpoints(ui32FenceSyncCheckpointCount,
	                             apsFenceSyncCheckpoints);
#endif /* defined(PVR_USE_FENCE_SYNC_MODEL) */

#if defined(PVR_USE_FENCE_SYNC_MODEL) || defined(SUPPORT_BUFFER_SYNC)
	fail_resolve_input_fence:
#endif

#if defined(SUPPORT_BUFFER_SYNC)
	if (psBufferSyncData)
	{
		pvr_buffer_sync_kick_failed(psBufferSyncData);
	}
	if (apsBufferFenceSyncCheckpoints)
	{
		kfree(apsBufferFenceSyncCheckpoints);
	}
#endif /* defined(SUPPORT_BUFFER_SYNC) */

	fail_populate_sync_addr_list:
	PVR_ASSERT(eError != PVRSRV_OK);
	OSFreeMem(psCmdHelper);
	fail_allochelper:

#if defined(PVR_USE_FENCE_SYNC_MODEL)
	if (apsFenceSyncCheckpoints)
	{
		SyncCheckpointFreeCheckpointListMem(apsFenceSyncCheckpoints);
	}
#endif
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockRelease(psTransferContext->hLock);
#endif
	return eError;
}


PVRSRV_ERROR PVRSRVRGXTDMNotifyWriteOffsetUpdateKM(
		RGX_SERVER_TQ_TDM_CONTEXT *psTransferContext,
		IMG_UINT32                 ui32PDumpFlags)
{
	RGXFWIF_KCCB_CMD  sKCCBCmd;
	PVRSRV_ERROR      eError;

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockAcquire(psTransferContext->hLock);
#endif

	/* Schedule the firmware command */
	sKCCBCmd.eCmdType = RGXFWIF_KCCB_CMD_NOTIFY_WRITE_OFFSET_UPDATE;
	sKCCBCmd.uCmdData.sWriteOffsetUpdateData.psContext = FWCommonContextGetFWAddress(psTransferContext->sTDMData.psServerCommonContext);

	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{
		eError = RGXScheduleCommand(psTransferContext->psDeviceNode->pvDevice,
		                            RGXFWIF_DM_TDM,
		                            &sKCCBCmd,
		                            sizeof(sKCCBCmd),
		                            0,
		                            ui32PDumpFlags);
		if (eError != PVRSRV_ERROR_RETRY)
		{
			break;
		}
		OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
	} END_LOOP_UNTIL_TIMEOUT();

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXTDMNotifyWriteOffsetUpdateKM: Failed to schedule the FW command %d (%s)",
				eError, PVRSRVGETERRORSTRING(eError)));
	}

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockRelease(psTransferContext->hLock);
#endif
	return eError;
}

PVRSRV_ERROR PVRSRVRGXTDMSetTransferContextPriorityKM(CONNECTION_DATA *psConnection,
                                                      PVRSRV_DEVICE_NODE * psDeviceNode,
                                                      RGX_SERVER_TQ_TDM_CONTEXT *psTransferContext,
                                                      IMG_UINT32 ui32Priority)
{
	PVRSRV_ERROR eError;

	PVR_UNREFERENCED_PARAMETER(psDeviceNode);

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockAcquire(psTransferContext->hLock);
#endif

	if (psTransferContext->sTDMData.ui32Priority != ui32Priority)
	{
		eError = ContextSetPriority(psTransferContext->sTDMData.psServerCommonContext,
		                            psConnection,
		                            psTransferContext->psDeviceNode->pvDevice,
		                            ui32Priority,
		                            RGXFWIF_DM_TDM);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed to set the priority (%s)", __func__, PVRSRVGetErrorStringKM(eError)));

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
			OSLockRelease(psTransferContext->hLock);
#endif
			return eError;
		}
	}

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockRelease(psTransferContext->hLock);
#endif
	return PVRSRV_OK;
}

void CheckForStalledTDMTransferCtxt(PVRSRV_RGXDEV_INFO *psDevInfo,
                                    DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                                    void *pvDumpDebugFile)
{
	DLLIST_NODE *psNode, *psNext;

	OSWRLockAcquireRead(psDevInfo->hTDMCtxListLock);

	dllist_foreach_node(&psDevInfo->sTDMCtxtListHead, psNode, psNext)
	{
		RGX_SERVER_TQ_TDM_CONTEXT *psCurrentServerTransferCtx =
				IMG_CONTAINER_OF(psNode, RGX_SERVER_TQ_TDM_CONTEXT, sListNode);

		DumpStalledFWCommonContext(psCurrentServerTransferCtx->sTDMData.psServerCommonContext,
		                           pfnDumpDebugPrintf, pvDumpDebugFile);
	}

	OSWRLockReleaseRead(psDevInfo->hTDMCtxListLock);
}


IMG_UINT32 CheckForStalledClientTDMTransferCtxt(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	DLLIST_NODE *psNode, *psNext;
	IMG_UINT32 ui32ContextBitMask = 0;

	OSWRLockAcquireRead(psDevInfo->hTDMCtxListLock);

	dllist_foreach_node(&psDevInfo->sTDMCtxtListHead, psNode, psNext)
	{
		RGX_SERVER_TQ_TDM_CONTEXT *psCurrentServerTransferCtx =
				IMG_CONTAINER_OF(psNode, RGX_SERVER_TQ_TDM_CONTEXT, sListNode);

		if (CheckStalledClientCommonContext(
				psCurrentServerTransferCtx->sTDMData.psServerCommonContext, RGX_KICK_TYPE_DM_TDM_2D)
				== PVRSRV_ERROR_CCCB_STALLED) {
			ui32ContextBitMask = RGX_KICK_TYPE_DM_TDM_2D;
		}
	}

	OSWRLockReleaseRead(psDevInfo->hTDMCtxListLock);
	return ui32ContextBitMask;
}

/**************************************************************************//**
 End of file (rgxtdmtransfer.c)
 ******************************************************************************/
