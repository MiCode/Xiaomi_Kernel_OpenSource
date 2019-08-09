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
#include "rgx_bvnc_defs_km.h"
#include "rgxmem.h"
#include "allocmem.h"
#include "devicemem.h"
#include "devicemem_pdump.h"
#include "osfunc.h"
#include "rgxccb.h"
#include "rgxhwperf.h"
#include "rgxtimerquery.h"
#include "htbuffer.h"

#include "sync_server.h"
#include "sync_internal.h"
#include "sync.h"
#include "rgx_memallocflags.h"

#include "sync_checkpoint.h"
#include "sync_checkpoint_internal.h"

/* Enable this to dump the compiled list of UFOs prior to kick call */
#define ENABLE_CMP_UFO_DUMP	0

//#define CMP_CHECKPOINT_DEBUG 1

#if defined(CMP_CHECKPOINT_DEBUG)
#define CHKPT_DBG(X) PVR_DPF(X)
#else
#define CHKPT_DBG(X)
#endif

struct _RGX_SERVER_COMPUTE_CONTEXT_ {
	PVRSRV_DEVICE_NODE			*psDeviceNode;
	RGX_SERVER_COMMON_CONTEXT	*psServerCommonContext;
	DEVMEM_MEMDESC				*psFWFrameworkMemDesc;
	DEVMEM_MEMDESC				*psFWComputeContextStateMemDesc;
	PVRSRV_CLIENT_SYNC_PRIM		*psSync;
	DLLIST_NODE					sListNode;
	SYNC_ADDR_LIST				sSyncAddrListFence;
	SYNC_ADDR_LIST				sSyncAddrListUpdate;
	ATOMIC_T					hIntJobRef;
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	POS_LOCK                    		 hLock;
#endif
};

PVRSRV_ERROR PVRSRVRGXCreateComputeContextKM(CONNECTION_DATA			*psConnection,
											 PVRSRV_DEVICE_NODE			*psDeviceNode,
											 IMG_UINT32					ui32Priority,
											 IMG_UINT32					ui32FrameworkCommandSize,
											 IMG_PBYTE					pbyFrameworkCommand,
											 IMG_HANDLE					hMemCtxPrivData,
											 IMG_DEV_VIRTADDR			sServicesSignalAddr,
											 RGX_SERVER_COMPUTE_CONTEXT	**ppsComputeContext)
{
	PVRSRV_RGXDEV_INFO 			*psDevInfo = psDeviceNode->pvDevice;
	DEVMEM_MEMDESC				*psFWMemContextMemDesc = RGXGetFWMemDescFromMemoryContextHandle(hMemCtxPrivData);
	RGX_SERVER_COMPUTE_CONTEXT	*psComputeContext;
	RGX_COMMON_CONTEXT_INFO		sInfo;
	PVRSRV_ERROR				eError = PVRSRV_OK;

	/* Prepare cleanup struct */
	*ppsComputeContext = NULL;
	psComputeContext = OSAllocZMem(sizeof(*psComputeContext));
	if (psComputeContext == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	eError = OSLockCreate(&psComputeContext->hLock, LOCK_TYPE_NONE);

	if(eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create lock (%s)",
									__func__,
									PVRSRVGetErrorStringKM(eError)));
		goto fail_createlock;
	}
#endif

	psComputeContext->psDeviceNode = psDeviceNode;

	/* Allocate cleanup sync */
	eError = SyncPrimAlloc(psDeviceNode->hSyncPrimContext,
						   &psComputeContext->psSync,
						   "compute cleanup");
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateComputeContextKM: Failed to allocate cleanup sync (0x%x)",
				eError));
		goto fail_syncalloc;
	}

	/*
		Allocate device memory for the firmware GPU context suspend state.
		Note: the FW reads/writes the state to memory by accessing the GPU register interface.
	*/
	PDUMPCOMMENT("Allocate RGX firmware compute context suspend state");

	eError = DevmemFwAllocate(psDevInfo,
							  sizeof(RGXFWIF_COMPUTECTX_STATE),
							  RGX_FWCOMCTX_ALLOCFLAGS,
							  "FwComputeContextState",
							  &psComputeContext->psFWComputeContextStateMemDesc);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateComputeContextKM: Failed to allocate firmware GPU context suspend state (%u)",
				eError));
		goto fail_contextsuspendalloc;
	}

	/*
	 * Create the FW framework buffer
	 */
	eError = PVRSRVRGXFrameworkCreateKM(psDeviceNode,
										&psComputeContext->psFWFrameworkMemDesc,
										ui32FrameworkCommandSize);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateComputeContextKM: Failed to allocate firmware GPU framework state (%u)",
				eError));
		goto fail_frameworkcreate;
	}

	/* Copy the Framework client data into the framework buffer */
	eError = PVRSRVRGXFrameworkCopyCommand(psComputeContext->psFWFrameworkMemDesc,
										   pbyFrameworkCommand,
										   ui32FrameworkCommandSize);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateComputeContextKM: Failed to populate the framework buffer (%u)",
				eError));
		goto fail_frameworkcopy;
	}
	
	sInfo.psFWFrameworkMemDesc = psComputeContext->psFWFrameworkMemDesc;

	if(RGX_IS_FEATURE_VALUE_SUPPORTED(psDevInfo, CDM_CONTROL_STREAM_FORMAT) &&
			RGX_GET_FEATURE_VALUE(psDevInfo, CDM_CONTROL_STREAM_FORMAT) == 2 &&
			RGX_IS_FEATURE_SUPPORTED(psDevInfo, SIGNAL_SNOOPING))
	{
		sInfo.psResumeSignalAddr = &sServicesSignalAddr;
	}else
	{
		PVR_UNREFERENCED_PARAMETER(sServicesSignalAddr);
	}

	eError = FWCommonContextAllocate(psConnection,
									 psDeviceNode,
									 REQ_TYPE_CDM,
									 RGXFWIF_DM_CDM,
									 NULL,
									 0,
									 psFWMemContextMemDesc,
									 psComputeContext->psFWComputeContextStateMemDesc,
									 RGX_CDM_CCB_SIZE_LOG2,
									 ui32Priority,
									 &sInfo,
									 &psComputeContext->psServerCommonContext);
	if (eError != PVRSRV_OK)
	{
		goto fail_contextalloc;
	}

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

fail_contextalloc:
fail_frameworkcopy:
	DevmemFwFree(psDevInfo, psComputeContext->psFWFrameworkMemDesc);
fail_frameworkcreate:
	DevmemFwFree(psDevInfo, psComputeContext->psFWComputeContextStateMemDesc);
fail_contextsuspendalloc:
	SyncPrimFree(psComputeContext->psSync);
fail_syncalloc:
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockDestroy(psComputeContext->hLock);
fail_createlock:
#endif
	OSFreeMem(psComputeContext);
	return eError;
}

PVRSRV_ERROR PVRSRVRGXDestroyComputeContextKM(RGX_SERVER_COMPUTE_CONTEXT *psComputeContext)
{
	PVRSRV_ERROR				eError = PVRSRV_OK;
	PVRSRV_RGXDEV_INFO *psDevInfo = psComputeContext->psDeviceNode->pvDevice;

	/* Check if the FW has finished with this resource ... */
	eError = RGXFWRequestCommonContextCleanUp(psComputeContext->psDeviceNode,
											  psComputeContext->psServerCommonContext,
											  psComputeContext->psSync,
											  RGXFWIF_DM_CDM,
											  PDUMP_FLAGS_NONE);

	if (eError == PVRSRV_ERROR_RETRY)
	{
		return eError;
	}
	else if (eError != PVRSRV_OK)
	{
		PVR_LOG(("%s: Unexpected error from RGXFWRequestCommonContextCleanUp (%s)",
				__FUNCTION__,
				PVRSRVGetErrorStringKM(eError)));
		return eError;
	}

	/* ... it has so we can free its resources */

	OSWRLockAcquireWrite(psDevInfo->hComputeCtxListLock);
	dllist_remove_node(&(psComputeContext->sListNode));
	OSWRLockReleaseWrite(psDevInfo->hComputeCtxListLock);

	FWCommonContextFree(psComputeContext->psServerCommonContext);
	DevmemFwFree(psDevInfo, psComputeContext->psFWFrameworkMemDesc);
	DevmemFwFree(psDevInfo, psComputeContext->psFWComputeContextStateMemDesc);
	SyncPrimFree(psComputeContext->psSync);

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockDestroy(psComputeContext->hLock);
#endif
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
								IMG_UINT32					ui32ServerSyncPrims,
								IMG_UINT32					*paui32ServerSyncFlags,
								SERVER_SYNC_PRIMITIVE		**pasServerSyncs,
								PVRSRV_FENCE				iCheckFence,
								PVRSRV_TIMELINE				iUpdateTimeline,
								PVRSRV_FENCE				*piUpdateFence,
								IMG_CHAR					pszUpdateFenceName[32],
								IMG_UINT32					ui32CmdSize,
								IMG_PBYTE					pui8DMCmd,
								IMG_UINT32					ui32PDumpFlags,
								IMG_UINT32					ui32ExtJobRef,
								IMG_DEV_VIRTADDR			sRobustnessResetReason)
{
	RGXFWIF_KCCB_CMD		sCmpKCCBCmd;
	RGX_CCB_CMD_HELPER_DATA	asCmdHelperData[1];
	PVRSRV_ERROR			eError;
	PVRSRV_ERROR			eError2;
	IMG_UINT32				i;
	IMG_UINT32				ui32CDMCmdOffset = 0;
	IMG_UINT32				ui32IntJobRef;
	IMG_UINT32				ui32FWCtx;
	IMG_BOOL				bCCBStateOpen = IMG_FALSE;

	PRGXFWIF_TIMESTAMP_ADDR pPreAddr;
	PRGXFWIF_TIMESTAMP_ADDR pPostAddr;
	PRGXFWIF_UFO_ADDR       pRMWUFOAddr;

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

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockAcquire(psComputeContext->hLock);
#endif

	ui32IntJobRef = OSAtomicIncrement(&psComputeContext->hIntJobRef);

	ui32IntClientFenceCount = ui32ClientFenceCount;
	eError = SyncAddrListPopulate(&psComputeContext->sSyncAddrListFence,
									ui32ClientFenceCount,
									pauiClientFenceUFOSyncPrimBlock,
									paui32ClientFenceSyncOffset);
	if(eError != PVRSRV_OK)
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
	if(eError != PVRSRV_OK)
	{
		goto err_populate_sync_addr_list;
	}
	if (ui32IntClientUpdateCount && !pauiIntUpdateUFOAddress)
	{
		pauiIntUpdateUFOAddress = psComputeContext->sSyncAddrListUpdate.pasFWAddrs;
	}
	paui32IntUpdateValue = paui32ClientUpdateValue;

	/* Sanity check the server fences */
	for (i=0;i<ui32ServerSyncPrims;i++)
	{
		if (!(paui32ServerSyncFlags[i] & PVRSRV_CLIENT_SYNC_PRIM_OP_CHECK))
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Server fence (on CDM) must fence", __FUNCTION__));
			eError = PVRSRV_ERROR_INVALID_SYNC_PRIM_OP;
			goto err_populate_sync_addr_list;
		}
	}

#if defined(PVR_USE_FENCE_SYNC_MODEL)
	CHKPT_DBG((PVR_DBG_ERROR, "%s: calling SyncCheckpointResolveFence (iCheckFence=%d), psComputeContext->psDeviceNode->hSyncCheckpointContext=<%p>...", __FUNCTION__, iCheckFence, (void*)psComputeContext->psDeviceNode->hSyncCheckpointContext));
	/* Resolve the sync checkpoints that make up the input fence */
	eError = SyncCheckpointResolveFence(psComputeContext->psDeviceNode->hSyncCheckpointContext,
										iCheckFence,
										&ui32FenceSyncCheckpointCount,
										&apsFenceSyncCheckpoints,
	                                    &uiCheckFenceUID);
	if (eError != PVRSRV_OK)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s: ...done, returned ERROR (eError=%d)", __FUNCTION__, eError));
		goto fail_resolve_input_fence;
	}
	CHKPT_DBG((PVR_DBG_ERROR, "%s: ...done, fence %d contained %d checkpoints (apsFenceSyncCheckpoints=<%p>)", __FUNCTION__, iCheckFence, ui32FenceSyncCheckpointCount, (void*)apsFenceSyncCheckpoints));
#if defined(CMP_CHECKPOINT_DEBUG)
	if (ui32FenceSyncCheckpointCount > 0)
	{
		IMG_UINT32 ii;
		for (ii=0; ii<ui32FenceSyncCheckpointCount; ii++)
		{
			PSYNC_CHECKPOINT psNextCheckpoint = *(apsFenceSyncCheckpoints +  ii);
			CHKPT_DBG((PVR_DBG_ERROR, "%s:    apsFenceSyncCheckpoints[%d]=<%p>", __FUNCTION__, ii, (void*)psNextCheckpoint));
		}
	}
#endif
	/* Create the output fence (if required) */
	if (iUpdateTimeline != PVRSRV_NO_TIMELINE)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s: calling SyncCheckpointCreateFence (iUpdateFence=%d, iUpdateTimeline=%d,  psComputeContext->psDeviceNode->hSyncCheckpointContext=<%p>)...", __FUNCTION__, iUpdateFence, iUpdateTimeline, (void*)psComputeContext->psDeviceNode->hSyncCheckpointContext));
		eError = SyncCheckpointCreateFence(psComputeContext->psDeviceNode,
		                                   pszUpdateFenceName,
										   iUpdateTimeline,
										   psComputeContext->psDeviceNode->hSyncCheckpointContext,
										   &iUpdateFence,
										   &uiUpdateFenceUID,
										   &pvUpdateFenceFinaliseData,
										   &psUpdateSyncCheckpoint,
										   (void*)&psFenceTimelineUpdateSync,
										   &ui32FenceTimelineUpdateValue);
		if (eError != PVRSRV_OK)
		{
			CHKPT_DBG((PVR_DBG_ERROR, "%s: ...returned error (%d)", __FUNCTION__, eError));
			goto fail_create_output_fence;
		}

		CHKPT_DBG((PVR_DBG_ERROR, "%s: ...returned from SyncCheckpointCreateFence (iUpdateFence=%d, psFenceTimelineUpdateSync=<%p>, ui32FenceTimelineUpdateValue=%u)", __FUNCTION__, iUpdateFence, psFenceTimelineUpdateSync, ui32FenceTimelineUpdateValue));

		CHKPT_DBG((PVR_DBG_ERROR, "%s: ui32IntClientUpdateCount=%u, psFenceTimelineUpdateSync=<%p>", __FUNCTION__, ui32IntClientUpdateCount, (void*)psFenceTimelineUpdateSync));
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

				CHKPT_DBG((PVR_DBG_ERROR, "%s: ui32IntClientUpdateCount=%u:", __FUNCTION__, ui32IntClientUpdateCount));
				for (iii=0; iii<ui32IntClientUpdateCount; iii++)
				{
					CHKPT_DBG((PVR_DBG_ERROR, "%s: pui32IntAllocatedUpdateValues[%d](<%p>) = 0x%x", __FUNCTION__, iii, (void*)pui32Tmp, *pui32Tmp));
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

			CHKPT_DBG((PVR_DBG_ERROR, "%s: append the timeline sync prim addr <%p> to the compute context update list", __FUNCTION__,  (void*)psFenceTimelineUpdateSync));
			/* Now append the timeline sync prim addr to the compute context update list */
			SyncAddrListAppendSyncPrim(&psComputeContext->sSyncAddrListUpdate,
			                           psFenceTimelineUpdateSync);
#if defined(CMP_CHECKPOINT_DEBUG)
			if (ui32IntClientUpdateCount > 0)
			{
				IMG_UINT32 iii;
				IMG_UINT32 *pui32Tmp = (IMG_UINT32*)pui32IntAllocatedUpdateValues;

				CHKPT_DBG((PVR_DBG_ERROR, "%s: ui32IntClientUpdateCount=%u:", __FUNCTION__, ui32IntClientUpdateCount));
				for (iii=0; iii<ui32IntClientUpdateCount; iii++)
				{
					CHKPT_DBG((PVR_DBG_ERROR, "%s: pui32IntAllocatedUpdateValues[%d](<%p>) = 0x%x", __FUNCTION__, iii, (void*)pui32Tmp, *pui32Tmp));
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
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append %d sync checkpoints to Compute CDM Fence (&psComputeContext->sSyncAddrListFence=<%p>)...", __FUNCTION__, ui32FenceSyncCheckpointCount, (void*)&psComputeContext->sSyncAddrListFence));
#if defined(CMP_CHECKPOINT_DEBUG)
		if (ui32IntClientUpdateCount > 0)
		{
			IMG_UINT32 iii;
			IMG_UINT32 *pui32Tmp = (IMG_UINT32*)pauiIntFenceUFOAddress;

			for (iii=0; iii<ui32IntClientUpdateCount; iii++)
			{
				CHKPT_DBG((PVR_DBG_ERROR, "%s: pui32IntAllocatedUpdateValues[%d](<%p>) = 0x%x", __FUNCTION__, iii, (void*)pui32Tmp, *pui32Tmp));
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

		CHKPT_DBG((PVR_DBG_ERROR, "%s:   Dumping %d update values (paui32IntUpdateValue=<%p>)...", __FUNCTION__, ui32IntClientUpdateCount, (void*)paui32IntUpdateValue));
		for (iii=0; iii<ui32IntClientUpdateCount; iii++)
		{
			CHKPT_DBG((PVR_DBG_ERROR, "%s: paui32IntUpdateValue[%d] = <%p>", __FUNCTION__, iii, (void*)pui32Tmp));
			CHKPT_DBG((PVR_DBG_ERROR, "%s: *paui32IntUpdateValue[%d] = 0x%x", __FUNCTION__, iii, *pui32Tmp));
			pui32Tmp++;
		}
	}
#endif

	if (psUpdateSyncCheckpoint)
	{
		/* Append the update (from output fence) */
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append 1 sync checkpoint to Compute CDM Update (&psComputeContext->sSyncAddrListUpdate=<%p>, psUpdateSyncCheckpoint=<%p>)...", __FUNCTION__, (void*)&psComputeContext->sSyncAddrListUpdate , (void*)psUpdateSyncCheckpoint));
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

			CHKPT_DBG((PVR_DBG_ERROR, "%s: pauiIntUpdateUFOAddress=<%p>, pui32Tmp=<%p>, ui32IntClientUpdateCount=%u", __FUNCTION__, (void*)pauiIntUpdateUFOAddress, (void*)pui32Tmp, ui32IntClientUpdateCount));
			for (iii=0; iii<ui32IntClientUpdateCount; iii++)
			{
				CHKPT_DBG((PVR_DBG_ERROR, "%s: pauiIntUpdateUFOAddress[%d](<%p>) = 0x%x", __FUNCTION__, iii, (void*)pui32Tmp, *pui32Tmp));
				pui32Tmp++;
			}
		}
#endif
	}
	CHKPT_DBG((PVR_DBG_ERROR, "%s:   (after pvr_sync) ui32IntClientFenceCount=%d, ui32IntClientUpdateCount=%d", __FUNCTION__, ui32IntClientFenceCount, ui32IntClientUpdateCount));
#endif /* defined(PVR_USE_FENCE_SYNC_MODEL) */

#if (ENABLE_CMP_UFO_DUMP == 1)
		PVR_DPF((PVR_DBG_ERROR, "%s: dumping Compute (CDM) fence/updates syncs...", __FUNCTION__));
		{
			IMG_UINT32 ii;
			PRGXFWIF_UFO_ADDR *psTmpIntFenceUFOAddress = pauiIntFenceUFOAddress;
			IMG_UINT32 *pui32TmpIntFenceValue = paui32IntFenceValue;
			PRGXFWIF_UFO_ADDR *psTmpIntUpdateUFOAddress = pauiIntUpdateUFOAddress;
			IMG_UINT32 *pui32TmpIntUpdateValue = paui32IntUpdateValue;

			/* Dump Fence syncs and Update syncs */
			PVR_DPF((PVR_DBG_ERROR, "%s: Prepared %d Compute (CDM) fence syncs (&psComputeContext->sSyncAddrListFence=<%p>, pauiIntFenceUFOAddress=<%p>):", __FUNCTION__, ui32IntClientFenceCount, (void*)&psComputeContext->sSyncAddrListFence, (void*)pauiIntFenceUFOAddress));
			for (ii=0; ii<ui32IntClientFenceCount; ii++)
			{
				if (psTmpIntFenceUFOAddress->ui32Addr & 0x1)
				{
					PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, CheckValue=PVRSRV_SYNC_CHECKPOINT_SIGNALLED", __FUNCTION__, ii+1, ui32IntClientFenceCount, (void*)psTmpIntFenceUFOAddress, psTmpIntFenceUFOAddress->ui32Addr));
				}
				else
				{
					PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, CheckValue=%d(0x%x)", __FUNCTION__, ii+1, ui32IntClientFenceCount, (void*)psTmpIntFenceUFOAddress, psTmpIntFenceUFOAddress->ui32Addr, *pui32TmpIntFenceValue, *pui32TmpIntFenceValue));
					pui32TmpIntFenceValue++;
				}
				psTmpIntFenceUFOAddress++;
			}
			PVR_DPF((PVR_DBG_ERROR, "%s: Prepared %d Compute (CDM) update syncs (&psComputeContext->sSyncAddrListUpdate=<%p>, pauiIntUpdateUFOAddress=<%p>):", __FUNCTION__, ui32IntClientUpdateCount, (void*)&psComputeContext->sSyncAddrListUpdate, (void*)pauiIntUpdateUFOAddress));
			for (ii=0; ii<ui32IntClientUpdateCount; ii++)
			{
				if (psTmpIntUpdateUFOAddress->ui32Addr & 0x1)
				{
					PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, UpdateValue=PVRSRV_SYNC_CHECKPOINT_SIGNALLED", __FUNCTION__, ii+1, ui32IntClientUpdateCount, (void*)psTmpIntUpdateUFOAddress, psTmpIntUpdateUFOAddress->ui32Addr));
				}
				else
				{
					PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, UpdateValue=%d", __FUNCTION__, ii+1, ui32IntClientUpdateCount, (void*)psTmpIntUpdateUFOAddress, psTmpIntUpdateUFOAddress->ui32Addr, *pui32TmpIntUpdateValue));
					pui32TmpIntUpdateValue++;
				}
				psTmpIntUpdateUFOAddress++;
			}
		}
#endif

	RGX_GetTimestampCmdHelper((PVRSRV_RGXDEV_INFO*) psComputeContext->psDeviceNode->pvDevice,
	                          & pPreAddr,
	                          & pPostAddr,
	                          & pRMWUFOAddr);

	eError = RGXCmdHelperInitCmdCCB(FWCommonContextGetClientCCB(psComputeContext->psServerCommonContext),
	                                ui32IntClientFenceCount,
	                                pauiIntFenceUFOAddress,
	                                paui32IntFenceValue,
	                                ui32IntClientUpdateCount,
	                                pauiIntUpdateUFOAddress,
	                                paui32IntUpdateValue,
	                                ui32ServerSyncPrims,
	                                paui32ServerSyncFlags,
	                                SYNC_FLAG_MASK_ALL,
	                                pasServerSyncs,
	                                ui32CmdSize,
	                                pui8DMCmd,
	                                & pPreAddr,
	                                & pPostAddr,
	                                & pRMWUFOAddr,
	                                RGXFWIF_CCB_CMD_TYPE_CDM,
	                                ui32ExtJobRef,
	                                ui32IntJobRef,
	                                ui32PDumpFlags,
	                                NULL,
	                                "Compute",
	                                bCCBStateOpen,
	                                asCmdHelperData,
									sRobustnessResetReason);
	if (eError != PVRSRV_OK)
	{
		goto fail_cmdinit;
	}

	eError = RGXCmdHelperAcquireCmdCCB(ARRAY_SIZE(asCmdHelperData),
	                                   asCmdHelperData);
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

		ui32CDMCmdOffset = RGXGetHostWriteOffsetCCB(FWCommonContextGetClientCCB(psComputeContext->psServerCommonContext));
		RGXCmdHelperReleaseCmdCCB(1, asCmdHelperData, "CDM", FWCommonContextGetFWAddress(psComputeContext->psServerCommonContext).ui32Addr);
	}

	/* Construct the kernel compute CCB command. */
	sCmpKCCBCmd.eCmdType = RGXFWIF_KCCB_CMD_KICK;
	sCmpKCCBCmd.uCmdData.sCmdKickData.psContext = FWCommonContextGetFWAddress(psComputeContext->psServerCommonContext);
	sCmpKCCBCmd.uCmdData.sCmdKickData.ui32CWoffUpdate = RGXGetHostWriteOffsetCCB(FWCommonContextGetClientCCB(psComputeContext->psServerCommonContext));
	sCmpKCCBCmd.uCmdData.sCmdKickData.ui32NumCleanupCtl = 0;

	ui32FWCtx = FWCommonContextGetFWAddress(psComputeContext->psServerCommonContext).ui32Addr;

	HTBLOGK(HTB_SF_MAIN_KICK_CDM,
			sCmpKCCBCmd.uCmdData.sCmdKickData.psContext,
			ui32CDMCmdOffset
			);
	RGX_HWPERF_HOST_ENQ(psComputeContext,
	                    OSGetCurrentClientProcessIDKM(),
	                    ui32FWCtx,
	                    ui32ExtJobRef,
	                    ui32IntJobRef,
	                    RGX_HWPERF_KICK_TYPE_CDM,
	                    uiCheckFenceUID,
	                    uiUpdateFenceUID,
	                    NO_DEADLINE,
	                    NO_CYCEST);

	/*
	 * Submit the compute command to the firmware.
	 */
	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{
		eError2 = RGXScheduleCommand(psComputeContext->psDeviceNode->pvDevice,
									RGXFWIF_DM_CDM,
									&sCmpKCCBCmd,
									sizeof(sCmpKCCBCmd),
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
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVRGXKickCDMKM failed to schedule kernel CCB command. (0x%x)", eError));
	}
	else
	{
#if defined(SUPPORT_GPUTRACE_EVENTS)
		RGXHWPerfFTraceGPUEnqueueEvent(psComputeContext->psDeviceNode->pvDevice,
				ui32FWCtx, ui32IntJobRef, RGX_HWPERF_KICK_TYPE_CDM);
#endif
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
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   Signalling NOHW sync checkpoint<%p>, ID:%d, FwAddr=0x%x", __FUNCTION__, (void*)psUpdateSyncCheckpoint, SyncCheckpointGetId(psUpdateSyncCheckpoint), SyncCheckpointGetFirmwareAddr(psUpdateSyncCheckpoint)));
		SyncCheckpointSignalNoHW(psUpdateSyncCheckpoint);
	}
	if (psFenceTimelineUpdateSync)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   Updating NOHW sync prim<%p> to %d", __FUNCTION__, (void*)psFenceTimelineUpdateSync, ui32FenceTimelineUpdateValue));
		SyncPrimNoHwUpdate(psFenceTimelineUpdateSync, ui32FenceTimelineUpdateValue);
	}
	SyncCheckpointNoHWUpdateTimelines(NULL);
#endif /* defined (NO_HARDWARE) */
#endif /* defined(PVR_USE_FENCE_SYNC_MODEL) */

	*piUpdateFence = iUpdateFence;

#if defined(PVR_USE_FENCE_SYNC_MODEL)
	if (pvUpdateFenceFinaliseData && (iUpdateFence != PVRSRV_NO_FENCE))
	{
		SyncCheckpointFinaliseFence(iUpdateFence, pvUpdateFenceFinaliseData);
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

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockRelease(psComputeContext->hLock);
#endif

	return PVRSRV_OK;

fail_cmdinit:
fail_cmdaquire:
#if defined(PVR_USE_FENCE_SYNC_MODEL)
	SyncAddrListRollbackCheckpoints(psComputeContext->psDeviceNode, &psComputeContext->sSyncAddrListFence);
	SyncAddrListRollbackCheckpoints(psComputeContext->psDeviceNode, &psComputeContext->sSyncAddrListUpdate);
fail_alloc_update_values_mem:
	if(iUpdateFence != PVRSRV_NO_FENCE)
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
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockRelease(psComputeContext->hLock);
#endif
	return eError;
}

PVRSRV_ERROR PVRSRVRGXFlushComputeDataKM(RGX_SERVER_COMPUTE_CONTEXT *psComputeContext)
{
	RGXFWIF_KCCB_CMD sFlushCmd;
	PVRSRV_ERROR eError = PVRSRV_OK;

#if defined(PDUMP)
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "Submit Compute flush");
#endif
	sFlushCmd.eCmdType = RGXFWIF_KCCB_CMD_SLCFLUSHINVAL;
	sFlushCmd.uCmdData.sSLCFlushInvalData.bInval = IMG_FALSE;
	sFlushCmd.uCmdData.sSLCFlushInvalData.bDMContext = IMG_TRUE;
	sFlushCmd.uCmdData.sSLCFlushInvalData.eDM = RGXFWIF_DM_CDM;
	sFlushCmd.uCmdData.sSLCFlushInvalData.psContext = FWCommonContextGetFWAddress(psComputeContext->psServerCommonContext);

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockAcquire(psComputeContext->hLock);
#endif

	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{
		eError = RGXScheduleCommand(psComputeContext->psDeviceNode->pvDevice,
									RGXFWIF_DM_CDM,
									&sFlushCmd,
									sizeof(sFlushCmd),
									0,
									PDUMP_FLAGS_CONTINUOUS);
		if (eError != PVRSRV_ERROR_RETRY)
		{
			break;
		}
		OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
	} END_LOOP_UNTIL_TIMEOUT();

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXFlushComputeDataKM: Failed to schedule SLC flush command with error (%u)", eError));
	}
	else
	{
		/* Wait for the SLC flush to complete */
		eError = RGXWaitForFWOp(psComputeContext->psDeviceNode->pvDevice,
								RGXFWIF_DM_CDM,
								psComputeContext->psSync,
								PDUMP_FLAGS_CONTINUOUS);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXFlushComputeDataKM: Compute flush aborted with error (%u)", eError));
		}
	}

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockRelease(psComputeContext->hLock);
#endif
	return eError;
}


PVRSRV_ERROR PVRSRVRGXNotifyComputeWriteOffsetUpdateKM(RGX_SERVER_COMPUTE_CONTEXT  *psComputeContext)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = psComputeContext->psDeviceNode->pvDevice;
	if (RGX_IS_FEATURE_VALUE_SUPPORTED(psDevInfo, CDM_CONTROL_STREAM_FORMAT) &&
		2 == RGX_GET_FEATURE_VALUE(psDevInfo, CDM_CONTROL_STREAM_FORMAT))
	{

		RGXFWIF_KCCB_CMD  sKCCBCmd;
		PVRSRV_ERROR      eError;

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
		OSLockAcquire(psComputeContext->hLock);
#endif

		/* Schedule the firmware command */
		sKCCBCmd.eCmdType = RGXFWIF_KCCB_CMD_NOTIFY_WRITE_OFFSET_UPDATE;
		sKCCBCmd.uCmdData.sWriteOffsetUpdateData.psContext = FWCommonContextGetFWAddress(psComputeContext->psServerCommonContext);

		LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
		{
			eError = RGXScheduleCommand(psComputeContext->psDeviceNode->pvDevice,
										RGXFWIF_DM_CDM,
										&sKCCBCmd,
										sizeof(sKCCBCmd),
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
			PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXNotifyWriteOffsetUpdateKM: Failed to schedule the FW command %d (%s)",
					eError, PVRSRVGETERRORSTRING(eError)));
		}

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
		OSLockRelease(psComputeContext->hLock);
#endif
		return eError;
	}else
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}
}


PVRSRV_ERROR PVRSRVRGXSetComputeContextPriorityKM(CONNECTION_DATA *psConnection,
                                                  PVRSRV_DEVICE_NODE * psDeviceNode,
												  RGX_SERVER_COMPUTE_CONTEXT *psComputeContext,
												  IMG_UINT32 ui32Priority)
{
	PVRSRV_ERROR eError;

	PVR_UNREFERENCED_PARAMETER(psDeviceNode);

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockAcquire(psComputeContext->hLock);
#endif

	eError = ContextSetPriority(psComputeContext->psServerCommonContext,
								psConnection,
								psComputeContext->psDeviceNode->pvDevice,
								ui32Priority,
								RGXFWIF_DM_CDM);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to set the priority of the compute context (%s)", __FUNCTION__, PVRSRVGetErrorStringKM(eError)));
	}

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockRelease(psComputeContext->hLock);
#endif
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
	
	*peLastResetReason = FWCommonContextGetLastResetReason(psComputeContext->psServerCommonContext,
	                                                       pui32LastResetJobRef);

	return PVRSRV_OK;
}

void CheckForStalledComputeCtxt(PVRSRV_RGXDEV_INFO *psDevInfo,
				DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
				void *pvDumpDebugFile)
{
	DLLIST_NODE *psNode, *psNext;
	OSWRLockAcquireRead(psDevInfo->hComputeCtxListLock);
	dllist_foreach_node(&psDevInfo->sComputeCtxtListHead, psNode, psNext)
	{
		RGX_SERVER_COMPUTE_CONTEXT *psCurrentServerComputeCtx =
			IMG_CONTAINER_OF(psNode, RGX_SERVER_COMPUTE_CONTEXT, sListNode);
		DumpStalledFWCommonContext(psCurrentServerComputeCtx->psServerCommonContext,
								   pfnDumpDebugPrintf, pvDumpDebugFile);
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

		if (CheckStalledClientCommonContext(psCurrentServerComputeCtx->psServerCommonContext, RGX_KICK_TYPE_DM_CDM)
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
