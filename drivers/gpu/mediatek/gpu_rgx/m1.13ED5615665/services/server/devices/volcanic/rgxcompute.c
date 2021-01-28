/*************************************************************************/ /*!
@File
@Title          RGX Compute routines
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    RGX Compute routines
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

#include "img_defs.h"
#include "srvkm.h"
#include "pdump_km.h"
#include "pvr_debug.h"
#include "rgxutils.h"
#include "rgxfwutils.h"
#include "rgxcompute.h"
#include "rgxmem.h"
#include "allocmem.h"
#include "devicemem.h"
#include "devicemem_pdump.h"
#include "osfunc.h"
#include "rgxccb.h"
#include "rgxhwperf.h"
#include "ospvr_gputrace.h"
#include "htbuffer.h"

#include "sync_server.h"
#include "sync_internal.h"
#include "sync.h"
#include "rgx_memallocflags.h"

#include "sync_checkpoint.h"
#include "sync_checkpoint_internal.h"

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
#include "rgxworkest.h"

#define HASH_CLEAN_LIMIT 6
#endif

/* Enable this to dump the compiled list of UFOs prior to kick call */
#define ENABLE_CMP_UFO_DUMP	0

//#define CMP_CHECKPOINT_DEBUG 1

#if defined(CMP_CHECKPOINT_DEBUG)
#define CHKPT_DBG(X) PVR_DPF(X)
#else
#define CHKPT_DBG(X)
#endif

typedef struct {
	DEVMEM_MEMDESC				*psContextStateMemDesc;
	RGX_SERVER_COMMON_CONTEXT	*psServerCommonContext;
	IMG_UINT32					ui32Priority;
} RGX_SERVER_CC_CMP_DATA;

struct _RGX_SERVER_COMPUTE_CONTEXT_ {
	PVRSRV_DEVICE_NODE			*psDeviceNode;
	//RGX_SERVER_COMMON_CONTEXT	*psServerCommonContext;
	//DEVMEM_MEMDESC				*psFWComputeContextStateMemDesc;
	DEVMEM_MEMDESC				*psFWComputeContextMemDesc;
	RGX_SERVER_CC_CMP_DATA		sComputeData;
	DLLIST_NODE					sListNode;
	SYNC_ADDR_LIST				sSyncAddrListFence;
	SYNC_ADDR_LIST				sSyncAddrListUpdate;
	POS_LOCK					hLock;
#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	WORKEST_HOST_DATA			sWorkEstData;
#endif
};

static
PVRSRV_ERROR _CreateComputeContext(CONNECTION_DATA *psConnection,
								   PVRSRV_DEVICE_NODE *psDeviceNode,
								   DEVMEM_MEMDESC *psAllocatedMemDesc,
								   IMG_UINT32 ui32AllocatedOffset,
								   SERVER_MMU_CONTEXT *psServerMMUContext,
								   DEVMEM_MEMDESC *psFWMemContextMemDesc,
								   IMG_UINT32 ui32PackedCCBSizeU88,
								   IMG_UINT32 ui32ContextFlags,
								   IMG_UINT32 ui32Priority,
								   IMG_UINT64 ui64RobustnessAddress,
								   IMG_UINT32 ui32MaxDeadlineMS,
								   RGX_COMMON_CONTEXT_INFO *psInfo,
								   RGX_SERVER_CC_CMP_DATA *psComputeData)
{
	IMG_UINT32 ui32CCBAllocSizeLog2, ui32CCBMaxAllocSizeLog2;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_ERROR eError;

	/*
		Allocate device memory for the firmware GPU context suspend state.
		Note: the FW reads/writes the state to memory by accessing the GPU register interface.
	 */
	PDUMPCOMMENT("Allocate RGX firmware compute context suspend state");

	eError = DevmemFwAllocate(psDevInfo,
							  sizeof(RGXFWIF_COMPUTECTX_STATE),
							  RGX_FWCOMCTX_ALLOCFLAGS,
							  "FwComputeContextState",
							  &psComputeData->psContextStateMemDesc);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Failed to allocate firmware GPU context suspend state (%d)",
				 __func__,
				 eError));
		goto fail_contextsuspendalloc;
	}

	ui32CCBAllocSizeLog2 = U32toU8_Unpack1(ui32PackedCCBSizeU88);
	ui32CCBMaxAllocSizeLog2 = U32toU8_Unpack2(ui32PackedCCBSizeU88);
	eError = FWCommonContextAllocate(psConnection,
									 psDeviceNode,
									 REQ_TYPE_CDM,
									 RGXFWIF_DM_CDM,
									 psServerMMUContext,
									 psAllocatedMemDesc,
									 ui32AllocatedOffset,
									 psFWMemContextMemDesc,
									 psComputeData->psContextStateMemDesc,
									 ui32CCBAllocSizeLog2 ? ui32CCBAllocSizeLog2 : RGX_CDM_CCB_SIZE_LOG2,
									 ui32CCBMaxAllocSizeLog2 ? ui32CCBMaxAllocSizeLog2 : RGX_CDM_CCB_MAX_SIZE_LOG2,
									 ui32ContextFlags,
									 ui32Priority,
									 ui32MaxDeadlineMS,
									 ui64RobustnessAddress,
									 psInfo,
									 &psComputeData->psServerCommonContext);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: Failed to init Compute fw common context (%s)",
				 __func__,
				 PVRSRVGetErrorString(eError)));
		goto fail_computecommoncontext;
	}

	/*
	 * Dump the FW compute context suspend state buffer
	*/
	PDUMPCOMMENT("Dump the compute context suspend state buffer");
	DevmemPDumpLoadMem(psComputeData->psContextStateMemDesc,
					   0,
					   sizeof(RGXFWIF_COMPUTECTX_STATE),
					   PDUMP_FLAGS_CONTINUOUS);

	psComputeData->ui32Priority = ui32Priority;
	return PVRSRV_OK;

fail_computecommoncontext:
	DevmemFree(psComputeData->psContextStateMemDesc);
fail_contextsuspendalloc:
	PVR_ASSERT(eError != PVRSRV_OK);

	return eError;
}

static
PVRSRV_ERROR _DestroyComputeContext(RGX_SERVER_CC_CMP_DATA *psComputeData,
									PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_ERROR eError;

	/* Check if the FW has finished with this resource ... */
	eError = RGXFWRequestCommonContextCleanUp(psDeviceNode,
											  psComputeData->psServerCommonContext,
											  RGXFWIF_DM_CDM,
											  PDUMP_FLAGS_NONE);
	if (eError == PVRSRV_ERROR_RETRY)
	{
		return eError;
	}
	else if (eError != PVRSRV_OK)
	{
		PVR_LOG(("%s: Unexpected error from RGXFWRequestCommonContextCleanUp (%s)",
				 __func__,
				PVRSRVGetErrorString(eError)));
		return eError;
	}

	/* ... it has so we can free its resources */
	FWCommonContextFree(psComputeData->psServerCommonContext);
	DevmemFwUnmapAndFree(psDeviceNode->pvDevice, psComputeData->psContextStateMemDesc);
	psComputeData->psServerCommonContext = NULL;
	return PVRSRV_OK;
	}

PVRSRV_ERROR PVRSRVRGXCreateComputeContextKM(CONNECTION_DATA			*psConnection,
											 PVRSRV_DEVICE_NODE			*psDeviceNode,
											 IMG_UINT32					ui32Priority,
											 IMG_HANDLE					hMemCtxPrivData,
											 IMG_UINT32					ui32StaticComputeContextStateSize,
											 IMG_PBYTE					pStaticComputeContextState,
											 IMG_UINT32					ui32PackedCCBSizeU88,
											 IMG_UINT32					ui32ContextFlags,
											 IMG_UINT64					ui64RobustnessAddress,
											 IMG_UINT32					ui32MaxDeadlineMS,
											 RGX_SERVER_COMPUTE_CONTEXT	**ppsComputeContext)
{
	DEVMEM_MEMDESC				*psFWMemContextMemDesc = RGXGetFWMemDescFromMemoryContextHandle(hMemCtxPrivData);
	PVRSRV_RGXDEV_INFO			*psDevInfo = psDeviceNode->pvDevice;
	RGX_SERVER_COMPUTE_CONTEXT	*psComputeContext;
	RGX_COMMON_CONTEXT_INFO		sInfo;
	PVRSRV_ERROR				eError = PVRSRV_OK;
	RGXFWIF_FWCOMPUTECONTEXT	*psFWComputeContext;

	/* Prepare cleanup struct */
	*ppsComputeContext = NULL;

	if (ui32StaticComputeContextStateSize > RGXFWIF_STATIC_COMPUTECONTEXT_SIZE)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psComputeContext = OSAllocZMem(sizeof(*psComputeContext));
	if (psComputeContext == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	/*
		Create the FW compute context, this has the CDM common
		context embedded within it
	 */
	eError = DevmemFwAllocate(psDevInfo,
			sizeof(RGXFWIF_FWCOMPUTECONTEXT),
			RGX_FWCOMCTX_ALLOCFLAGS,
			"FwComputeContext",
			&psComputeContext->psFWComputeContextMemDesc);
	if (eError != PVRSRV_OK)
	{
		goto fail_fwcomputecontext;
	}

	eError = OSLockCreate(&psComputeContext->hLock);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Failed to create lock (%s)",
				 __func__,
				 PVRSRVGetErrorString(eError)));
		goto fail_createlock;
	}

	psComputeContext->psDeviceNode = psDeviceNode;

	eError = _CreateComputeContext(psConnection,
									 psDeviceNode,
									 psComputeContext->psFWComputeContextMemDesc,
									 offsetof(RGXFWIF_FWCOMPUTECONTEXT, sCDMContext),
									 hMemCtxPrivData,
									 psFWMemContextMemDesc,
									 ui32PackedCCBSizeU88,
									 ui32ContextFlags,
									 ui32Priority,
									 ui64RobustnessAddress,
									 ui32MaxDeadlineMS,
									 &sInfo,
									 &psComputeContext->sComputeData);
	if (eError != PVRSRV_OK)
	{
		goto fail_computecontext;
	}

	eError = DevmemAcquireCpuVirtAddr(psComputeContext->psFWComputeContextMemDesc,
			(void **)&psFWComputeContext);
	if (eError != PVRSRV_OK)
	{
		goto fail_acquire_cpu_mapping;
	}

	OSDeviceMemCopy(&psFWComputeContext->sStaticComputeContextState, pStaticComputeContextState, ui32StaticComputeContextStateSize);
	DevmemPDumpLoadMem(psComputeContext->psFWComputeContextMemDesc, 0, sizeof(RGXFWIF_FWCOMPUTECONTEXT), PDUMP_FLAGS_CONTINUOUS);
	DevmemReleaseCpuVirtAddr(psComputeContext->psFWComputeContextMemDesc);

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	WorkEstInitCompute(psDevInfo, &psComputeContext->sWorkEstData);
#endif

	SyncAddrListInit(&psComputeContext->sSyncAddrListFence);
	SyncAddrListInit(&psComputeContext->sSyncAddrListUpdate);

	{
		PVRSRV_RGXDEV_INFO			*psDevInfo = psDeviceNode->pvDevice;

		OSWRLockAcquireWrite(psDevInfo->hComputeCtxListLock);
		dllist_add_to_tail(&(psDevInfo->sComputeCtxtListHead), &(psComputeContext->sListNode));
		OSWRLockReleaseWrite(psDevInfo->hComputeCtxListLock);
	}

	*ppsComputeContext = psComputeContext;
	return PVRSRV_OK;

fail_acquire_cpu_mapping:
	FWCommonContextFree(psComputeContext->sComputeData.psServerCommonContext);
fail_computecontext:
	OSLockDestroy(psComputeContext->hLock);
fail_createlock:
	DevmemFwUnmapAndFree(psDevInfo, psComputeContext->psFWComputeContextMemDesc);
fail_fwcomputecontext:
	OSFreeMem(psComputeContext);
	return eError;
}

PVRSRV_ERROR PVRSRVRGXDestroyComputeContextKM(RGX_SERVER_COMPUTE_CONTEXT *psComputeContext)
{
	PVRSRV_ERROR				eError = PVRSRV_OK;
	PVRSRV_RGXDEV_INFO *psDevInfo = psComputeContext->psDeviceNode->pvDevice;
#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	RGXFWIF_FWCOMPUTECONTEXT	*psFWComputeContext;
	IMG_UINT32 ui32WorkEstCCBSubmitted;

	eError = DevmemAcquireCpuVirtAddr(psComputeContext->psFWComputeContextMemDesc,
			(void **)&psFWComputeContext);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: Failed to map firmware compute context (%s)",
				__func__,
				PVRSRVGetErrorString(eError)));
		return eError;
	}

	ui32WorkEstCCBSubmitted = psFWComputeContext->ui32WorkEstCCBSubmitted;

	DevmemReleaseCpuVirtAddr(psComputeContext->psFWComputeContextMemDesc);

	/* Check if all of the workload estimation CCB commands for this workload are read */
	if (ui32WorkEstCCBSubmitted != psComputeContext->sWorkEstData.ui32WorkEstCCBReceived)
	{
        PVR_DPF((PVR_DBG_WARNING,
                "%s: WorkEst # cmds submitted (%u) and received (%u) mismatch",
                __func__, ui32WorkEstCCBSubmitted,
                psComputeContext->sWorkEstData.ui32WorkEstCCBReceived));

		return PVRSRV_ERROR_RETRY;
	}
#endif

	eError = _DestroyComputeContext(&psComputeContext->sComputeData,
									psComputeContext->psDeviceNode);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	OSWRLockAcquireWrite(psDevInfo->hComputeCtxListLock);
	dllist_remove_node(&(psComputeContext->sListNode));
	OSWRLockReleaseWrite(psDevInfo->hComputeCtxListLock);

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	WorkEstDeInitCompute(psDevInfo, &psComputeContext->sWorkEstData);
#endif

	DevmemFwUnmapAndFree(psDevInfo, psComputeContext->psFWComputeContextMemDesc);

	OSLockDestroy(psComputeContext->hLock);
	OSFreeMem(psComputeContext);

	return PVRSRV_OK;
}


PVRSRV_ERROR PVRSRVRGXKickCDMKM(RGX_SERVER_COMPUTE_CONTEXT	*psComputeContext,
								IMG_UINT32					ui32ClientCacheOpSeqNum,
								IMG_UINT32					ui32ClientFenceCount,
								SYNC_PRIMITIVE_BLOCK		**pauiClientFenceUFOSyncPrimBlock,
								IMG_UINT32					*paui32ClientFenceSyncOffset,
								IMG_UINT32					*paui32ClientFenceValue,
								IMG_UINT32					ui32ClientUpdateCount,
								SYNC_PRIMITIVE_BLOCK		**pauiClientUpdateUFOSyncPrimBlock,
								IMG_UINT32					*paui32ClientUpdateSyncOffset,
								IMG_UINT32					*paui32ClientUpdateValue,
#if defined(SUPPORT_SERVER_SYNC_IMPL)
								IMG_UINT32					ui32ServerSyncPrims,
								IMG_UINT32					*paui32ServerSyncFlags,
								SERVER_SYNC_PRIMITIVE		**pasServerSyncs,
#endif
								PVRSRV_FENCE				iCheckFence,
								PVRSRV_TIMELINE				iUpdateTimeline,
								PVRSRV_FENCE				*piUpdateFence,
								IMG_CHAR					pszUpdateFenceName[PVRSRV_SYNC_NAME_LENGTH],
								IMG_UINT32					ui32CmdSize,
								IMG_PBYTE					pui8DMCmd,
								IMG_UINT32					ui32PDumpFlags,
								IMG_UINT32					ui32ExtJobRef,
								IMG_UINT32					ui32NumWorkgroups,
								IMG_UINT32					ui32NumWorkitems,
								IMG_UINT64					ui64DeadlineInus)
{
	RGXFWIF_KCCB_CMD		sCmpKCCBCmd;
	RGX_CCB_CMD_HELPER_DATA	asCmdHelperData[1];
	PVRSRV_ERROR			eError;
	PVRSRV_ERROR			eError2;
#if defined(SUPPORT_SERVER_SYNC_IMPL)
	IMG_UINT32				i;
#endif
	IMG_UINT32				ui32CDMCmdOffset = 0;
	PVRSRV_RGXDEV_INFO      *psDevInfo = FWCommonContextGetRGXDevInfo(psComputeContext->sComputeData.psServerCommonContext);
	RGX_CLIENT_CCB          *psClientCCB = FWCommonContextGetClientCCB(psComputeContext->sComputeData.psServerCommonContext);
	IMG_UINT32              ui32IntJobRef = OSAtomicIncrement(&psDevInfo->iCCBSubmissionOrdinal);
	IMG_UINT32				ui32FWCtx;
	IMG_BOOL				bCCBStateOpen = IMG_FALSE;

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	RGXFWIF_WORKEST_KICK_DATA sWorkloadKickDataCompute = {0};
	IMG_UINT32 ui32CDMWorkloadDataRO = 0;
	IMG_UINT32 ui32CDMCmdHeaderOffset = 0;
	IMG_UINT32 ui32CDMCmdOffsetWrapCheck = 0;
	RGX_WORKLOAD sWorkloadCharacteristics = {0};
#endif

	IMG_UINT64				ui64FBSCEntryMask;
	IMG_UINT32 ui32IntClientFenceCount = 0;
	PRGXFWIF_UFO_ADDR *pauiIntFenceUFOAddress = NULL;
	IMG_UINT32 *paui32IntFenceValue = NULL;
	IMG_UINT32 ui32IntClientUpdateCount = 0;
	PRGXFWIF_UFO_ADDR *pauiIntUpdateUFOAddress = NULL;
	IMG_UINT32 *paui32IntUpdateValue = NULL;
	PVRSRV_FENCE  iUpdateFence = PVRSRV_NO_FENCE;
	IMG_UINT64               uiCheckFenceUID = 0;
	IMG_UINT64               uiUpdateFenceUID = 0;
#if defined(PVR_USE_FENCE_SYNC_MODEL)
	PSYNC_CHECKPOINT psUpdateSyncCheckpoint = NULL;
	PSYNC_CHECKPOINT *apsFenceSyncCheckpoints = NULL;
	IMG_UINT32 ui32FenceSyncCheckpointCount = 0;
	IMG_UINT32 *pui32IntAllocatedUpdateValues = NULL;
	PVRSRV_CLIENT_SYNC_PRIM *psFenceTimelineUpdateSync = NULL;
	IMG_UINT32 ui32FenceTimelineUpdateValue = 0;
	void *pvUpdateFenceFinaliseData = NULL;

	if (iUpdateTimeline >= 0 && !piUpdateFence)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
#else /* defined(PVR_USE_FENCE_SYNC_MODEL) */
	if (iUpdateTimeline >= 0)
	{
		PVR_DPF((PVR_DBG_WARNING, "%s: Providing update timeline (%d) in non-supporting driver",
			__func__, iUpdateTimeline));
	}
	if (iCheckFence >= 0)
	{
		PVR_DPF((PVR_DBG_WARNING, "%s: Providing check fence (%d) in non-supporting driver",
			__func__, iCheckFence));
	}
#endif /* defined(PVR_USE_FENCE_SYNC_MODEL) */

	/* Ensure the string is null-terminated (Required for safety) */
	pszUpdateFenceName[31] = '\0';

	OSLockAcquire(psComputeContext->hLock);
	ui32IntClientFenceCount = ui32ClientFenceCount;

	eError = SyncAddrListPopulate(&psComputeContext->sSyncAddrListFence,
									ui32ClientFenceCount,
									pauiClientFenceUFOSyncPrimBlock,
									paui32ClientFenceSyncOffset);
	if (eError != PVRSRV_OK)
	{
		goto err_populate_sync_addr_list;
	}
	if (ui32IntClientFenceCount && !pauiIntFenceUFOAddress)
	{
		pauiIntFenceUFOAddress = psComputeContext->sSyncAddrListFence.pasFWAddrs;
	}
	paui32IntFenceValue = paui32ClientFenceValue;

	ui32IntClientUpdateCount = ui32ClientUpdateCount;

	eError = SyncAddrListPopulate(&psComputeContext->sSyncAddrListUpdate,
									ui32ClientUpdateCount,
									pauiClientUpdateUFOSyncPrimBlock,
									paui32ClientUpdateSyncOffset);
	if (eError != PVRSRV_OK)
	{
		goto err_populate_sync_addr_list;
	}
	if (ui32IntClientUpdateCount && !pauiIntUpdateUFOAddress)
	{
		pauiIntUpdateUFOAddress = psComputeContext->sSyncAddrListUpdate.pasFWAddrs;
	}
	paui32IntUpdateValue = paui32ClientUpdateValue;

#if defined(SUPPORT_SERVER_SYNC_IMPL)
	/* Sanity check the server fences */
	for (i=0;i<ui32ServerSyncPrims;i++)
	{
		if (!(paui32ServerSyncFlags[i] & PVRSRV_CLIENT_SYNC_PRIM_OP_CHECK))
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Server fence (on CDM) must fence", __func__));
			eError = PVRSRV_ERROR_INVALID_SYNC_PRIM_OP;
			goto err_populate_sync_addr_list;
		}
	}
#endif

#if defined(PVR_USE_FENCE_SYNC_MODEL)
	CHKPT_DBG((PVR_DBG_ERROR, "%s: calling SyncCheckpointResolveFence (iCheckFence=%d), psComputeContext->psDeviceNode->hSyncCheckpointContext=<%p>...", __func__, iCheckFence, (void*)psComputeContext->psDeviceNode->hSyncCheckpointContext));
	/* Resolve the sync checkpoints that make up the input fence */
	eError = SyncCheckpointResolveFence(psComputeContext->psDeviceNode->hSyncCheckpointContext,
										iCheckFence,
										&ui32FenceSyncCheckpointCount,
										&apsFenceSyncCheckpoints,
	                                    &uiCheckFenceUID, ui32PDumpFlags);
	if (eError != PVRSRV_OK)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s: ...done, returned ERROR (eError=%d)", __func__, eError));
		goto fail_resolve_input_fence;
	}
	CHKPT_DBG((PVR_DBG_ERROR, "%s: ...done, fence %d contained %d checkpoints (apsFenceSyncCheckpoints=<%p>)", __func__, iCheckFence, ui32FenceSyncCheckpointCount, (void*)apsFenceSyncCheckpoints));
#if defined(CMP_CHECKPOINT_DEBUG)
	if (ui32FenceSyncCheckpointCount > 0)
	{
		IMG_UINT32 ii;
		for (ii=0; ii<ui32FenceSyncCheckpointCount; ii++)
		{
			PSYNC_CHECKPOINT psNextCheckpoint = *(apsFenceSyncCheckpoints + ii);
			CHKPT_DBG((PVR_DBG_ERROR, "%s:    apsFenceSyncCheckpoints[%d]=<%p>", __func__, ii, (void*)psNextCheckpoint));
		}
	}
#endif
	/* Create the output fence (if required) */
	if (iUpdateTimeline != PVRSRV_NO_TIMELINE)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s: calling SyncCheckpointCreateFence (iUpdateFence=%d, iUpdateTimeline=%d,  psComputeContext->psDeviceNode->hSyncCheckpointContext=<%p>)...", __func__, iUpdateFence, iUpdateTimeline, (void*)psComputeContext->psDeviceNode->hSyncCheckpointContext));
		eError = SyncCheckpointCreateFence(psComputeContext->psDeviceNode,
		                                   pszUpdateFenceName,
										   iUpdateTimeline,
										   psComputeContext->psDeviceNode->hSyncCheckpointContext,
										   &iUpdateFence,
										   &uiUpdateFenceUID,
										   &pvUpdateFenceFinaliseData,
										   &psUpdateSyncCheckpoint,
										   (void*)&psFenceTimelineUpdateSync,
										   &ui32FenceTimelineUpdateValue,
										   ui32PDumpFlags);
		if (eError != PVRSRV_OK)
		{
			CHKPT_DBG((PVR_DBG_ERROR, "%s: ...returned error (%d)", __func__, eError));
			goto fail_create_output_fence;
		}

		CHKPT_DBG((PVR_DBG_ERROR, "%s: ...returned from SyncCheckpointCreateFence (iUpdateFence=%d, psFenceTimelineUpdateSync=<%p>, ui32FenceTimelineUpdateValue=%u)", __func__, iUpdateFence, psFenceTimelineUpdateSync, ui32FenceTimelineUpdateValue));

		CHKPT_DBG((PVR_DBG_ERROR, "%s: ui32IntClientUpdateCount=%u, psFenceTimelineUpdateSync=<%p>", __func__, ui32IntClientUpdateCount, (void*)psFenceTimelineUpdateSync));
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
#if defined(CMP_CHECKPOINT_DEBUG)
			if (ui32IntClientUpdateCount > 0)
			{
				IMG_UINT32 iii;
				IMG_UINT32 *pui32Tmp = (IMG_UINT32*)pui32IntAllocatedUpdateValues;

				CHKPT_DBG((PVR_DBG_ERROR, "%s: ui32IntClientUpdateCount=%u:", __func__, ui32IntClientUpdateCount));
				for (iii=0; iii<ui32IntClientUpdateCount; iii++)
				{
					CHKPT_DBG((PVR_DBG_ERROR, "%s: pui32IntAllocatedUpdateValues[%d](<%p>) = 0x%x", __func__, iii, (void*)pui32Tmp, *pui32Tmp));
					pui32Tmp++;
				}
			}
#endif
			/* Now set the additional update value */
			pui32TimelineUpdateWp = pui32IntAllocatedUpdateValues + ui32IntClientUpdateCount;
			*pui32TimelineUpdateWp = ui32FenceTimelineUpdateValue;
			ui32IntClientUpdateCount++;
			/* Now make sure paui32ClientUpdateValue points to pui32IntAllocatedUpdateValues */
			paui32ClientUpdateValue = pui32IntAllocatedUpdateValues;

			CHKPT_DBG((PVR_DBG_ERROR, "%s: append the timeline sync prim addr <%p> to the compute context update list", __func__,  (void*)psFenceTimelineUpdateSync));
			/* Now append the timeline sync prim addr to the compute context update list */
			SyncAddrListAppendSyncPrim(&psComputeContext->sSyncAddrListUpdate,
			                           psFenceTimelineUpdateSync);
#if defined(CMP_CHECKPOINT_DEBUG)
			if (ui32IntClientUpdateCount > 0)
			{
				IMG_UINT32 iii;
				IMG_UINT32 *pui32Tmp = (IMG_UINT32*)pui32IntAllocatedUpdateValues;

				CHKPT_DBG((PVR_DBG_ERROR, "%s: ui32IntClientUpdateCount=%u:", __func__, ui32IntClientUpdateCount));
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

	/* Append the checks (from input fence) */
	if (ui32FenceSyncCheckpointCount > 0)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append %d sync checkpoints to Compute CDM Fence (&psComputeContext->sSyncAddrListFence=<%p>)...", __func__, ui32FenceSyncCheckpointCount, (void*)&psComputeContext->sSyncAddrListFence));
#if defined(CMP_CHECKPOINT_DEBUG)
		if (ui32IntClientUpdateCount > 0)
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
		SyncAddrListAppendCheckpoints(&psComputeContext->sSyncAddrListFence,
									  ui32FenceSyncCheckpointCount,
									  apsFenceSyncCheckpoints);
		if (!pauiIntFenceUFOAddress)
		{
			pauiIntFenceUFOAddress = psComputeContext->sSyncAddrListFence.pasFWAddrs;
		}
		ui32IntClientFenceCount += ui32FenceSyncCheckpointCount;
	}
#if defined(CMP_CHECKPOINT_DEBUG)
	if (ui32IntClientUpdateCount > 0)
	{
		IMG_UINT32 iii;
		IMG_UINT32 *pui32Tmp = (IMG_UINT32*)paui32IntUpdateValue;

		CHKPT_DBG((PVR_DBG_ERROR, "%s:   Dumping %d update values (paui32IntUpdateValue=<%p>)...", __func__, ui32IntClientUpdateCount, (void*)paui32IntUpdateValue));
		for (iii=0; iii<ui32IntClientUpdateCount; iii++)
		{
			CHKPT_DBG((PVR_DBG_ERROR, "%s: paui32IntUpdateValue[%d] = <%p>", __func__, iii, (void*)pui32Tmp));
			CHKPT_DBG((PVR_DBG_ERROR, "%s: *paui32IntUpdateValue[%d] = 0x%x", __func__, iii, *pui32Tmp));
			pui32Tmp++;
		}
	}
#endif

	if (psUpdateSyncCheckpoint)
	{
		/* Append the update (from output fence) */
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append 1 sync checkpoint to Compute CDM Update (&psComputeContext->sSyncAddrListUpdate=<%p>, psUpdateSyncCheckpoint=<%p>)...", __func__, (void*)&psComputeContext->sSyncAddrListUpdate , (void*)psUpdateSyncCheckpoint));
		SyncAddrListAppendCheckpoints(&psComputeContext->sSyncAddrListUpdate,
									  1,
									  &psUpdateSyncCheckpoint);
		if (!pauiIntUpdateUFOAddress)
		{
			pauiIntUpdateUFOAddress = psComputeContext->sSyncAddrListUpdate.pasFWAddrs;
		}
		ui32IntClientUpdateCount++;
#if defined(CMP_CHECKPOINT_DEBUG)
		if (ui32IntClientUpdateCount > 0)
		{
			IMG_UINT32 iii;
			IMG_UINT32 *pui32Tmp = (IMG_UINT32*)pauiIntUpdateUFOAddress;

			CHKPT_DBG((PVR_DBG_ERROR, "%s: pauiIntUpdateUFOAddress=<%p>, pui32Tmp=<%p>, ui32IntClientUpdateCount=%u", __func__, (void*)pauiIntUpdateUFOAddress, (void*)pui32Tmp, ui32IntClientUpdateCount));
			for (iii=0; iii<ui32IntClientUpdateCount; iii++)
			{
				CHKPT_DBG((PVR_DBG_ERROR, "%s: pauiIntUpdateUFOAddress[%d](<%p>) = 0x%x", __func__, iii, (void*)pui32Tmp, *pui32Tmp));
				pui32Tmp++;
			}
		}
#endif
	}
	CHKPT_DBG((PVR_DBG_ERROR, "%s:   (after pvr_sync) ui32IntClientFenceCount=%d, ui32IntClientUpdateCount=%d", __func__, ui32IntClientFenceCount, ui32IntClientUpdateCount));
#endif /* defined(PVR_USE_FENCE_SYNC_MODEL) */

#if (ENABLE_CMP_UFO_DUMP == 1)
		PVR_DPF((PVR_DBG_ERROR, "%s: dumping Compute (CDM) fence/updates syncs...", __func__));
		{
			IMG_UINT32 ii;
			PRGXFWIF_UFO_ADDR *psTmpIntFenceUFOAddress = pauiIntFenceUFOAddress;
			IMG_UINT32 *pui32TmpIntFenceValue = paui32IntFenceValue;
			PRGXFWIF_UFO_ADDR *psTmpIntUpdateUFOAddress = pauiIntUpdateUFOAddress;
			IMG_UINT32 *pui32TmpIntUpdateValue = paui32IntUpdateValue;

			/* Dump Fence syncs and Update syncs */
			PVR_DPF((PVR_DBG_ERROR, "%s: Prepared %d Compute (CDM) fence syncs (&psComputeContext->sSyncAddrListFence=<%p>, pauiIntFenceUFOAddress=<%p>):", __func__, ui32IntClientFenceCount, (void*)&psComputeContext->sSyncAddrListFence, (void*)pauiIntFenceUFOAddress));
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
			PVR_DPF((PVR_DBG_ERROR, "%s: Prepared %d Compute (CDM) update syncs (&psComputeContext->sSyncAddrListUpdate=<%p>, pauiIntUpdateUFOAddress=<%p>):", __func__, ui32IntClientUpdateCount, (void*)&psComputeContext->sSyncAddrListUpdate, (void*)pauiIntUpdateUFOAddress));
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
	/*
	 * Extract the FBSC entries from MMU Context for the deferred FBSC invalidate command,
	 * in other words, take the value and set it to zero afterwards.
	 * FBSC Entry Mask must be extracted from MMU ctx and updated just before the kick starts
	 * as it must be ready at the time of context activation.
	 */
	{
		eError = RGXExtractFBSCEntryMaskFromMMUContext(psComputeContext->psDeviceNode,
													   FWCommonContextGetServerMMUCtx(psComputeContext->sComputeData.psServerCommonContext),
													   &ui64FBSCEntryMask);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "Failed to extract FBSC Entry Mask (%d)", eError));
			goto fail_cmdinvalfbsc;
		}
	}

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	sWorkloadCharacteristics.sCompute.ui32NumberOfWorkgroups = ui32NumWorkgroups;
	sWorkloadCharacteristics.sCompute.ui32NumberOfWorkitems  = ui32NumWorkitems;

	/* Prepare workload estimation */
	WorkEstPrepare(psComputeContext->psDeviceNode->pvDevice,
			&psComputeContext->sWorkEstData,
			&psComputeContext->sWorkEstData.uWorkloadMatchingData.sCompute.sDataCDM,
			RGXFWIF_CCB_CMD_TYPE_CDM,
			&sWorkloadCharacteristics,
			ui64DeadlineInus,
			&sWorkloadKickDataCompute);
#endif

	RGXCmdHelperInitCmdCCB(psClientCCB,
	                       ui64FBSCEntryMask,
	                       ui32IntClientFenceCount,
	                       pauiIntFenceUFOAddress,
	                       paui32IntFenceValue,
	                       ui32IntClientUpdateCount,
	                       pauiIntUpdateUFOAddress,
	                       paui32IntUpdateValue,
#if defined(SUPPORT_SERVER_SYNC_IMPL)
	                       ui32ServerSyncPrims,
	                       paui32ServerSyncFlags,
	                       SYNC_FLAG_MASK_ALL,
	                       pasServerSyncs,
#endif /* SUPPORT_SERVER_SYNC_IMPL */
	                       ui32CmdSize,
	                       pui8DMCmd,
	                       RGXFWIF_CCB_CMD_TYPE_CDM,
	                       ui32ExtJobRef,
	                       ui32IntJobRef,
	                       ui32PDumpFlags,
#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	                       &sWorkloadKickDataCompute,
#else
	                       NULL,
#endif
	                       "Compute",
	                       bCCBStateOpen,
	                       asCmdHelperData);

	eError = RGXCmdHelperAcquireCmdCCB(ARRAY_SIZE(asCmdHelperData), asCmdHelperData);
	if (eError != PVRSRV_OK)
	{
		goto fail_cmdaquire;
	}


	/*
		We should reserve space in the kernel CCB here and fill in the command
		directly.
		This is so if there isn't space in the kernel CCB we can return with
		retry back to services client before we take any operations
	*/

	/*
		We might only be kicking for flush out a padding packet so only submit
		the command if the create was successful
	*/
	if (eError == PVRSRV_OK)
	{
		/*
			All the required resources are ready at this point, we can't fail so
			take the required server sync operations and commit all the resources
		*/

		ui32CDMCmdOffset = RGXGetHostWriteOffsetCCB(psClientCCB);
		RGXCmdHelperReleaseCmdCCB(1, asCmdHelperData, "CDM", FWCommonContextGetFWAddress(psComputeContext->sComputeData.psServerCommonContext).ui32Addr);
	}

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	/* The following is used to determine the offset of the command header containing
	   the workload estimation data so that can be accessed when the KCCB is read */
	ui32CDMCmdHeaderOffset = RGXCmdHelperGetDMCommandHeaderOffset(asCmdHelperData);

	ui32CDMCmdOffsetWrapCheck = RGXGetHostWriteOffsetCCB(FWCommonContextGetClientCCB(psComputeContext->sComputeData.psServerCommonContext));

	/* This checks if the command would wrap around at the end of the CCB and
	 * therefore would start at an offset of 0 rather than the current command
	 * offset */
	if (ui32CDMCmdOffset < ui32CDMCmdOffsetWrapCheck)
	{
		ui32CDMWorkloadDataRO = ui32CDMCmdOffset;
	}
	else
	{
		ui32CDMWorkloadDataRO = 0;
	}
#endif

	/* Construct the kernel compute CCB command. */
	sCmpKCCBCmd.eCmdType = RGXFWIF_KCCB_CMD_KICK;
	sCmpKCCBCmd.uCmdData.sCmdKickData.psContext = FWCommonContextGetFWAddress(psComputeContext->sComputeData.psServerCommonContext);
	sCmpKCCBCmd.uCmdData.sCmdKickData.ui32CWoffUpdate = RGXGetHostWriteOffsetCCB(psClientCCB);
	sCmpKCCBCmd.uCmdData.sCmdKickData.ui32CWrapMaskUpdate = RGXGetWrapMaskCCB(psClientCCB);
	sCmpKCCBCmd.uCmdData.sCmdKickData.ui32NumCleanupCtl = 0;

	/* Add the Workload data into the KCCB kick */
#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	/* Store the offset to the CCCB command header so that it can be referenced
	 * when the KCCB command reaches the FW */
	sCmpKCCBCmd.uCmdData.sCmdKickData.ui32WorkEstCmdHeaderOffset = ui32CDMWorkloadDataRO + ui32CDMCmdHeaderOffset;
#else
	sCmpKCCBCmd.uCmdData.sCmdKickData.ui32WorkEstCmdHeaderOffset = 0;
#endif

	ui32FWCtx = FWCommonContextGetFWAddress(psComputeContext->sComputeData.psServerCommonContext).ui32Addr;

	HTBLOGK(HTB_SF_MAIN_KICK_CDM,
			sCmpKCCBCmd.uCmdData.sCmdKickData.psContext,
			ui32CDMCmdOffset
			);
	RGXSRV_HWPERF_ENQ(psComputeContext, OSGetCurrentClientProcessIDKM(),
	                  ui32FWCtx, ui32ExtJobRef, ui32IntJobRef,
	                  RGX_HWPERF_KICK_TYPE_CDM,
	                  iCheckFence,
	                  iUpdateFence,
	                  iUpdateTimeline,
	                  uiCheckFenceUID,
	                  uiUpdateFenceUID);

	/*
	 * Submit the compute command to the firmware.
	 */
	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{
		eError2 = RGXScheduleCommand(psComputeContext->psDeviceNode->pvDevice,
									 FWCommonContextGetServerMMUCtx(psComputeContext->sComputeData.psServerCommonContext),
									RGXFWIF_DM_CDM,
									&sCmpKCCBCmd,
									ui32ClientCacheOpSeqNum,
									ui32PDumpFlags);
		if (eError2 != PVRSRV_ERROR_RETRY)
		{
			break;
		}
		OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
	} END_LOOP_UNTIL_TIMEOUT();

	if (eError2 != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s failed to schedule kernel CCB command (%s)",
				 __func__,
				 PVRSRVGetErrorString(eError2)));
	}
	else
	{
		PVRGpuTraceEnqueueEvent(psComputeContext->psDeviceNode->pvDevice,
		                        ui32FWCtx, ui32ExtJobRef, ui32IntJobRef,
		                        RGX_HWPERF_KICK_TYPE_CDM);
	}
	/*
	 * Now check eError (which may have returned an error from our earlier call
	 * to RGXCmdHelperAcquireCmdCCB) - we needed to process any flush command first
	 * so we check it now...
	 */
	if (eError != PVRSRV_OK )
	{
		goto fail_cmdaquire;
	}

#if defined(PVR_USE_FENCE_SYNC_MODEL)
#if defined(NO_HARDWARE)
	/* If NO_HARDWARE, signal the output fence's sync checkpoint and sync prim */
	if (psUpdateSyncCheckpoint)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   Signalling NOHW sync checkpoint<%p>, ID:%d, FwAddr=0x%x", __func__, (void*)psUpdateSyncCheckpoint, SyncCheckpointGetId(psUpdateSyncCheckpoint), SyncCheckpointGetFirmwareAddr(psUpdateSyncCheckpoint)));
		SyncCheckpointSignalNoHW(psUpdateSyncCheckpoint);
	}
	if (psFenceTimelineUpdateSync)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   Updating NOHW sync prim<%p> to %d", __func__, (void*)psFenceTimelineUpdateSync, ui32FenceTimelineUpdateValue));
		SyncPrimNoHwUpdate(psFenceTimelineUpdateSync, ui32FenceTimelineUpdateValue);
	}
	SyncCheckpointNoHWUpdateTimelines(NULL);
#endif /* defined(NO_HARDWARE) */
#endif /* defined(PVR_USE_FENCE_SYNC_MODEL) */

	*piUpdateFence = iUpdateFence;

#if defined(PVR_USE_FENCE_SYNC_MODEL)
	if (pvUpdateFenceFinaliseData && (iUpdateFence != PVRSRV_NO_FENCE))
	{
		SyncCheckpointFinaliseFence(psComputeContext->psDeviceNode, iUpdateFence,
		                            pvUpdateFenceFinaliseData,
									psUpdateSyncCheckpoint, pszUpdateFenceName);
	}
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

	OSLockRelease(psComputeContext->hLock);

	return PVRSRV_OK;

fail_cmdaquire:
fail_cmdinvalfbsc:
#if defined(PVR_USE_FENCE_SYNC_MODEL)
	SyncAddrListRollbackCheckpoints(psComputeContext->psDeviceNode, &psComputeContext->sSyncAddrListFence);
	SyncAddrListRollbackCheckpoints(psComputeContext->psDeviceNode, &psComputeContext->sSyncAddrListUpdate);
fail_alloc_update_values_mem:
	if (iUpdateFence != PVRSRV_NO_FENCE)
	{
		SyncCheckpointRollbackFenceData(iUpdateFence, pvUpdateFenceFinaliseData);
	}
fail_create_output_fence:
	/* Drop the references taken on the sync checkpoints in the
	 * resolved input fence */
	SyncAddrListDeRefCheckpoints(ui32FenceSyncCheckpointCount,
								 apsFenceSyncCheckpoints);
fail_resolve_input_fence:
#endif /* defined(PVR_USE_FENCE_SYNC_MODEL) */

err_populate_sync_addr_list:
#if defined(PVR_USE_FENCE_SYNC_MODEL)
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
	OSLockRelease(psComputeContext->hLock);
	return eError;
}

PVRSRV_ERROR PVRSRVRGXFlushComputeDataKM(RGX_SERVER_COMPUTE_CONTEXT *psComputeContext)
{
	RGXFWIF_KCCB_CMD sFlushCmd;
	PVRSRV_ERROR eError = PVRSRV_OK;
	IMG_UINT32 ui32kCCBCommandSlot;
	PVRSRV_RGXDEV_INFO *psDevInfo = psComputeContext->psDeviceNode->pvDevice;

#if defined(PDUMP)
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "Submit Compute flush");
#endif
	sFlushCmd.eCmdType = RGXFWIF_KCCB_CMD_SLCFLUSHINVAL;
	sFlushCmd.uCmdData.sSLCFlushInvalData.ui64Size = 0;
	sFlushCmd.uCmdData.sSLCFlushInvalData.ui64Address = 0;
	sFlushCmd.uCmdData.sSLCFlushInvalData.bInval = IMG_FALSE;
	sFlushCmd.uCmdData.sSLCFlushInvalData.bDMContext = IMG_TRUE;
	sFlushCmd.uCmdData.sSLCFlushInvalData.psContext = FWCommonContextGetFWAddress(psComputeContext->sComputeData.psServerCommonContext);

	OSLockAcquire(psComputeContext->hLock);

	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{
		eError = RGXScheduleCommandAndGetKCCBSlot(psDevInfo,
									FWCommonContextGetServerMMUCtx(psComputeContext->sComputeData.psServerCommonContext),
									RGXFWIF_DM_CDM,
									&sFlushCmd,
									0,
									PDUMP_FLAGS_CONTINUOUS,
									&ui32kCCBCommandSlot);
		if (eError != PVRSRV_ERROR_RETRY)
		{
			break;
		}
		OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
	} END_LOOP_UNTIL_TIMEOUT();

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Failed to schedule SLC flush command (%s)",
				 __func__,
				 PVRSRVGetErrorString(eError)));
	}
	else
	{
		/* Wait for the SLC flush to complete */
		eError = RGXWaitForKCCBSlotUpdate(psDevInfo, ui32kCCBCommandSlot, PDUMP_FLAGS_CONTINUOUS);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: Compute flush aborted (%s)",
					 __func__,
					 PVRSRVGetErrorString(eError)));
		}
		else if (unlikely(psDevInfo->pui32KernelCCBRtnSlots[ui32kCCBCommandSlot] &
		                  RGXFWIF_KCCB_RTN_SLOT_POLL_FAILURE))
		{
			PVR_DPF((PVR_DBG_WARNING, "%s: FW poll on a HW operation failed", __func__));
		}
	}

	OSLockRelease(psComputeContext->hLock);
	return eError;
}


PVRSRV_ERROR PVRSRVRGXNotifyComputeWriteOffsetUpdateKM(RGX_SERVER_COMPUTE_CONTEXT  *psComputeContext)
{
	RGXFWIF_KCCB_CMD  sKCCBCmd;
	PVRSRV_ERROR      eError;

	OSLockAcquire(psComputeContext->hLock);

	/* Schedule the firmware command */
	sKCCBCmd.eCmdType = RGXFWIF_KCCB_CMD_NOTIFY_WRITE_OFFSET_UPDATE;
	sKCCBCmd.uCmdData.sWriteOffsetUpdateData.psContext = FWCommonContextGetFWAddress(psComputeContext->sComputeData.psServerCommonContext);

	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{
		eError = RGXScheduleCommand(psComputeContext->psDeviceNode->pvDevice,
								FWCommonContextGetServerMMUCtx(psComputeContext->sComputeData.psServerCommonContext),
									RGXFWIF_DM_CDM,
									&sKCCBCmd,
									0,
									PDUMP_FLAGS_NONE);
		if (eError != PVRSRV_ERROR_RETRY)
		{
			break;
		}
		OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
	} END_LOOP_UNTIL_TIMEOUT();

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: Failed to schedule the FW command %d (%s)",
				__func__,
				eError,
				PVRSRVGETERRORSTRING(eError)));
	}

	OSLockRelease(psComputeContext->hLock);

	return eError;
}


PVRSRV_ERROR PVRSRVRGXSetComputeContextPriorityKM(CONNECTION_DATA *psConnection,
                                                  PVRSRV_DEVICE_NODE * psDeviceNode,
												  RGX_SERVER_COMPUTE_CONTEXT *psComputeContext,
												  IMG_UINT32 ui32Priority)
{
	PVRSRV_ERROR eError;

	PVR_UNREFERENCED_PARAMETER(psDeviceNode);

	OSLockAcquire(psComputeContext->hLock);

	eError = ContextSetPriority(psComputeContext->sComputeData.psServerCommonContext,
								psConnection,
								psComputeContext->psDeviceNode->pvDevice,
								ui32Priority,
								RGXFWIF_DM_CDM);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to set the priority of the compute context (%s)", __func__, PVRSRVGetErrorString(eError)));
	}

	OSLockRelease(psComputeContext->hLock);
	return eError;
}

/*
 * PVRSRVRGXGetLastComputeContextResetReasonKM
 */
PVRSRV_ERROR PVRSRVRGXGetLastComputeContextResetReasonKM(RGX_SERVER_COMPUTE_CONTEXT *psComputeContext,
                                                         IMG_UINT32 *peLastResetReason,
														 IMG_UINT32 *pui32LastResetJobRef)
{
	PVR_ASSERT(psComputeContext != NULL);
	PVR_ASSERT(peLastResetReason != NULL);
	PVR_ASSERT(pui32LastResetJobRef != NULL);

	*peLastResetReason = FWCommonContextGetLastResetReason(psComputeContext->sComputeData.psServerCommonContext,
	                                                       pui32LastResetJobRef);

	return PVRSRV_OK;
}

/*
 * PVRSRVRGXSetComputeContextPropertyKM
 */
PVRSRV_ERROR PVRSRVRGXSetComputeContextPropertyKM(RGX_SERVER_COMPUTE_CONTEXT *psComputeContext,
                                                  RGX_CONTEXT_PROPERTY eContextProperty,
                                                  IMG_UINT64 ui64Input,
                                                  IMG_UINT64 *pui64Output)
{
	PVRSRV_ERROR eError;

	switch (eContextProperty)
	{
		case RGX_CONTEXT_PROPERTY_FLAGS:
		{
			OSLockAcquire(psComputeContext->hLock);
			eError = FWCommonContextSetFlags(psComputeContext->sComputeData.psServerCommonContext,
			                                 (IMG_UINT32)ui64Input);
			OSLockRelease(psComputeContext->hLock);
			PVR_LOG_IF_ERROR(eError, "FWCommonContextSetFlags");
			break;
		}

		default:
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: PVRSRV_ERROR_NOT_SUPPORTED - asked to set unknown property (%d)", __func__, eContextProperty));
			eError = PVRSRV_ERROR_NOT_SUPPORTED;
		}
	}

	return eError;
}

void DumpComputeCtxtsInfo(PVRSRV_RGXDEV_INFO *psDevInfo,
                          DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                          void *pvDumpDebugFile,
                          IMG_UINT32 ui32VerbLevel)
{
	DLLIST_NODE *psNode, *psNext;
	OSWRLockAcquireRead(psDevInfo->hComputeCtxListLock);
	dllist_foreach_node(&psDevInfo->sComputeCtxtListHead, psNode, psNext)
	{
		RGX_SERVER_COMPUTE_CONTEXT *psCurrentServerComputeCtx =
			IMG_CONTAINER_OF(psNode, RGX_SERVER_COMPUTE_CONTEXT, sListNode);
		DumpFWCommonContextInfo(psCurrentServerComputeCtx->sComputeData.psServerCommonContext,
		                        pfnDumpDebugPrintf, pvDumpDebugFile, ui32VerbLevel);
	}
	OSWRLockReleaseRead(psDevInfo->hComputeCtxListLock);
}

IMG_UINT32 CheckForStalledClientComputeCtxt(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	IMG_UINT32 ui32ContextBitMask = 0;
	DLLIST_NODE *psNode, *psNext;
	OSWRLockAcquireRead(psDevInfo->hComputeCtxListLock);
	dllist_foreach_node(&psDevInfo->sComputeCtxtListHead, psNode, psNext)
	{
		RGX_SERVER_COMPUTE_CONTEXT *psCurrentServerComputeCtx =
			IMG_CONTAINER_OF(psNode, RGX_SERVER_COMPUTE_CONTEXT, sListNode);

		if (CheckStalledClientCommonContext(psCurrentServerComputeCtx->sComputeData.psServerCommonContext, RGX_KICK_TYPE_DM_CDM)
			== PVRSRV_ERROR_CCCB_STALLED)
		{
			ui32ContextBitMask |= RGX_KICK_TYPE_DM_CDM;
		}
	}
	OSWRLockReleaseRead(psDevInfo->hComputeCtxListLock);
	return ui32ContextBitMask;
}

/******************************************************************************
 End of file (rgxcompute.c)
******************************************************************************/
