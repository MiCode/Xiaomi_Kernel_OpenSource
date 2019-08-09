/*************************************************************************/ /*!
@File           rgxkicksync.c
@Title          Server side of the sync only kick API
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    
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
#include "rgxkicksync.h"

#include "rgxdevice.h"
#include "rgxmem.h"
#include "rgxfwutils.h"
#include "allocmem.h"
#include "sync.h"
#include "rgxhwperf.h"

#include "sync_checkpoint.h"
#include "sync_checkpoint_internal.h"

/* Enable this to dump the compiled list of UFOs prior to kick call */
#define ENABLE_KICKSYNC_UFO_DUMP	0

//#define KICKSYNC_CHECKPOINT_DEBUG 1

#if defined(KICKSYNC_CHECKPOINT_DEBUG)
#define CHKPT_DBG(X) PVR_DPF(X)
#else
#define CHKPT_DBG(X)
#endif

struct _RGX_SERVER_KICKSYNC_CONTEXT_
{
	PVRSRV_DEVICE_NODE        * psDeviceNode;
	RGX_SERVER_COMMON_CONTEXT * psServerCommonContext;
	PVRSRV_CLIENT_SYNC_PRIM   * psSync;
	DLLIST_NODE                 sListNode;
	SYNC_ADDR_LIST              sSyncAddrListFence;
	SYNC_ADDR_LIST              sSyncAddrListUpdate;
	ATOMIC_T                    hIntJobRef;
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	POS_LOCK                     hLock;
#endif
};


PVRSRV_ERROR PVRSRVRGXCreateKickSyncContextKM(CONNECTION_DATA             * psConnection,
                                              PVRSRV_DEVICE_NODE          * psDeviceNode,
                                              IMG_HANDLE					hMemCtxPrivData,
                                              RGX_SERVER_KICKSYNC_CONTEXT ** ppsKickSyncContext)
{
	PVRSRV_RGXDEV_INFO          * psDevInfo = psDeviceNode->pvDevice;
	DEVMEM_MEMDESC              * psFWMemContextMemDesc = RGXGetFWMemDescFromMemoryContextHandle(hMemCtxPrivData);
	RGX_SERVER_KICKSYNC_CONTEXT * psKickSyncContext;
	RGX_COMMON_CONTEXT_INFO      sInfo;
	PVRSRV_ERROR                 eError = PVRSRV_OK;

	/* Prepare cleanup struct */
	* ppsKickSyncContext = NULL;
	psKickSyncContext = OSAllocZMem(sizeof(*psKickSyncContext));
	if (psKickSyncContext == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	eError = OSLockCreate(&psKickSyncContext->hLock, LOCK_TYPE_NONE);

	if(eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create lock (%s)",
									__func__,
									PVRSRVGetErrorStringKM(eError)));
		goto err_lockcreate;
	}
#endif

	psKickSyncContext->psDeviceNode = psDeviceNode;

	/* Allocate cleanup sync */
	eError = SyncPrimAlloc(psDeviceNode->hSyncPrimContext,
	                       & psKickSyncContext->psSync,
	                       "kick sync cleanup");
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "PVRSRVRGXCreateKickSyncContextKM: Failed to allocate cleanup sync (0x%x)",
		         eError));
		goto fail_syncalloc;
	}

	sInfo.psFWFrameworkMemDesc = NULL;

	eError = FWCommonContextAllocate(psConnection,
									 psDeviceNode,
									 REQ_TYPE_KICKSYNC,
									 RGXFWIF_DM_GP,
									 NULL,
									 0,
									 psFWMemContextMemDesc,
									 NULL,
									 RGX_KICKSYNC_CCB_SIZE_LOG2,
	                                 0, /* priority */
									 & sInfo,
									 & psKickSyncContext->psServerCommonContext);
	if (eError != PVRSRV_OK)
	{
		goto fail_contextalloc;
	}

	OSWRLockAcquireWrite(psDevInfo->hKickSyncCtxListLock);
	dllist_add_to_tail(&(psDevInfo->sKickSyncCtxtListHead), &(psKickSyncContext->sListNode));
	OSWRLockReleaseWrite(psDevInfo->hKickSyncCtxListLock);

	SyncAddrListInit(&psKickSyncContext->sSyncAddrListFence);
	SyncAddrListInit(&psKickSyncContext->sSyncAddrListUpdate);

	* ppsKickSyncContext = psKickSyncContext;
	return PVRSRV_OK;

fail_contextalloc:
fail_syncalloc:
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockDestroy(psKickSyncContext->hLock);
err_lockcreate:
#endif
	OSFreeMem(psKickSyncContext);
	return eError;
}


PVRSRV_ERROR PVRSRVRGXDestroyKickSyncContextKM(RGX_SERVER_KICKSYNC_CONTEXT * psKickSyncContext)
{
	PVRSRV_ERROR         eError    = PVRSRV_OK;
	PVRSRV_RGXDEV_INFO * psDevInfo = psKickSyncContext->psDeviceNode->pvDevice;

	/* Check if the FW has finished with this resource ... */
	eError = RGXFWRequestCommonContextCleanUp(psKickSyncContext->psDeviceNode,
	                                          psKickSyncContext->psServerCommonContext,
	                                          psKickSyncContext->psSync,
	                                          RGXFWIF_DM_3D,
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

	OSWRLockAcquireWrite(psDevInfo->hKickSyncCtxListLock);
	dllist_remove_node(&(psKickSyncContext->sListNode));
	OSWRLockReleaseWrite(psDevInfo->hKickSyncCtxListLock);

	FWCommonContextFree(psKickSyncContext->psServerCommonContext);
	SyncPrimFree(psKickSyncContext->psSync);

	SyncAddrListDeinit(&psKickSyncContext->sSyncAddrListFence);
	SyncAddrListDeinit(&psKickSyncContext->sSyncAddrListUpdate);

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockDestroy(psKickSyncContext->hLock);
#endif

	OSFreeMem(psKickSyncContext);

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVRGXKickSyncKM(RGX_SERVER_KICKSYNC_CONTEXT * psKickSyncContext,

                                 IMG_UINT32                    ui32ClientCacheOpSeqNum,

                                 IMG_UINT32                    ui32ClientFenceCount,
                                 SYNC_PRIMITIVE_BLOCK           ** pauiClientFenceUFOSyncPrimBlock,
                                 IMG_UINT32                  * paui32ClientFenceOffset,
                                 IMG_UINT32                  * paui32ClientFenceValue,

                                 IMG_UINT32                    ui32ClientUpdateCount,
                                 SYNC_PRIMITIVE_BLOCK           ** pauiClientUpdateUFOSyncPrimBlock,
                                 IMG_UINT32                  * paui32ClientUpdateOffset,
                                 IMG_UINT32                  * paui32ClientUpdateValue,

                                 IMG_UINT32                    ui32ServerSyncPrims,
                                 IMG_UINT32                  * paui32ServerSyncFlags,
                                 SERVER_SYNC_PRIMITIVE      ** pasServerSyncs,

                                 PVRSRV_FENCE                  iCheckFence,
                                 PVRSRV_TIMELINE               iUpdateTimeline,
                                 PVRSRV_FENCE                * piUpdateFence,
                                 IMG_CHAR                      szUpdateFenceName[32],

                                 IMG_UINT32                    ui32ExtJobRef)
{
	RGXFWIF_KCCB_CMD         sKickSyncKCCBCmd;
	RGX_CCB_CMD_HELPER_DATA  asCmdHelperData[1];
	PVRSRV_ERROR             eError;
	PVRSRV_ERROR             eError2;
	IMG_UINT32               i;
	IMG_BOOL                 bCCBStateOpen = IMG_FALSE;
	PRGXFWIF_UFO_ADDR        *pauiClientFenceUFOAddress = NULL;
	PRGXFWIF_UFO_ADDR        *pauiClientUpdateUFOAddress = NULL;
	PVRSRV_FENCE             iUpdateFence = PVRSRV_NO_FENCE;
	IMG_UINT32               ui32IntJobRef;
	IMG_UINT32               ui32FWCtx = FWCommonContextGetFWAddress(psKickSyncContext->psServerCommonContext).ui32Addr;
	IMG_UINT64               uiCheckFenceUID = 0;
	IMG_UINT64               uiUpdateFenceUID = 0;
#if defined(PVR_USE_FENCE_SYNC_MODEL)
	PSYNC_CHECKPOINT psUpdateSyncCheckpoint = NULL;
	PSYNC_CHECKPOINT *apsFenceSyncCheckpoints = NULL;
	IMG_UINT32 ui32FenceSyncCheckpointCount = 0;
	IMG_UINT32 ui32FenceTimelineUpdateValue = 0;
	IMG_UINT32 *pui32IntAllocatedUpdateValues = NULL;
	PVRSRV_CLIENT_SYNC_PRIM *psFenceTimelineUpdateSync = NULL;
	void *pvUpdateFenceFinaliseData = NULL;
#endif /* defined(PVR_USE_FENCE_SYNC_MODEL) */
	IMG_DEV_VIRTADDR sRobustnessResetReason = {0};

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockAcquire(psKickSyncContext->hLock);
#endif

	ui32IntJobRef = OSAtomicIncrement(&psKickSyncContext->hIntJobRef);

	eError = SyncAddrListPopulate(&psKickSyncContext->sSyncAddrListFence,
							ui32ClientFenceCount,
							pauiClientFenceUFOSyncPrimBlock,
							paui32ClientFenceOffset);

	if(eError != PVRSRV_OK)
	{
		goto fail_syncaddrlist;
	}

	if (ui32ClientFenceCount > 0)
	{
		pauiClientFenceUFOAddress = psKickSyncContext->sSyncAddrListFence.pasFWAddrs;
	}

	eError = SyncAddrListPopulate(&psKickSyncContext->sSyncAddrListUpdate,
							ui32ClientUpdateCount,
							pauiClientUpdateUFOSyncPrimBlock,
							paui32ClientUpdateOffset);

	if(eError != PVRSRV_OK)
	{
		goto fail_syncaddrlist;
	}

	if (ui32ClientUpdateCount > 0)
	{
		pauiClientUpdateUFOAddress = psKickSyncContext->sSyncAddrListUpdate.pasFWAddrs;
	}

	/* Sanity check the server fences */
	for (i = 0; i < ui32ServerSyncPrims; i++)
	{
		if (0 == (paui32ServerSyncFlags[i] & PVRSRV_CLIENT_SYNC_PRIM_OP_CHECK))
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Server fence (on Kick Sync) must fence", __FUNCTION__));
			eError = PVRSRV_ERROR_INVALID_SYNC_PRIM_OP;
			goto out_unlock;
		}
	}

	/* Ensure the string is null-terminated (Required for safety) */
	szUpdateFenceName[31] = '\0';

#if defined(PVR_USE_FENCE_SYNC_MODEL)
	/* This will never be true if called from the bridge since piUpdateFence will always be valid */
	if (iUpdateTimeline >= 0 && !piUpdateFence)
	{
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto out_unlock;
	}

	CHKPT_DBG((PVR_DBG_ERROR, "%s: calling SyncCheckpointResolveFence (iCheckFence=%d), psKickSyncContext->psDeviceNode->hSyncCheckpointContext=<%p>...", __FUNCTION__, iCheckFence, (void*)psKickSyncContext->psDeviceNode->hSyncCheckpointContext));
	/* Resolve the sync checkpoints that make up the input fence */
	eError = SyncCheckpointResolveFence(psKickSyncContext->psDeviceNode->hSyncCheckpointContext,
	                                    iCheckFence,
	                                    &ui32FenceSyncCheckpointCount,
	                                    &apsFenceSyncCheckpoints,
	                                    &uiCheckFenceUID);
	if (eError != PVRSRV_OK)
	{
		goto fail_resolve_fence;
	}

	/* Create the output fence (if required) */
	if (iUpdateTimeline != PVRSRV_NO_TIMELINE)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s: calling SyncCheckpointCreateFence (iUpdateTimeline=%d)...", __FUNCTION__, iUpdateTimeline));
		eError = SyncCheckpointCreateFence(psKickSyncContext->psDeviceNode,
		                                   szUpdateFenceName,
		                                   iUpdateTimeline,
		                                   psKickSyncContext->psDeviceNode->hSyncCheckpointContext,
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

		/* Append the sync prim update for the timeline (if required) */
		if (psFenceTimelineUpdateSync)
		{
			IMG_UINT32 *pui32TimelineUpdateWp = NULL;

			/* Allocate memory to hold the list of update values (including our timeline update) */
			pui32IntAllocatedUpdateValues = OSAllocMem(sizeof(*paui32ClientUpdateValue) * (ui32ClientUpdateCount+1));
			if (!pui32IntAllocatedUpdateValues)
			{
				/* Failed to allocate memory */
				eError = PVRSRV_ERROR_OUT_OF_MEMORY;
				goto fail_alloc_update_values_mem;
			}
			OSCachedMemSet(pui32IntAllocatedUpdateValues, 0xbb, sizeof(*pui32IntAllocatedUpdateValues) * (ui32ClientUpdateCount+1));
			/* Copy the update values into the new memory, then append our timeline update value */
			OSCachedMemCopy(pui32IntAllocatedUpdateValues, paui32ClientUpdateValue, sizeof(*pui32IntAllocatedUpdateValues) * ui32ClientUpdateCount);
			/* Now set the additional update value */
			pui32TimelineUpdateWp = pui32IntAllocatedUpdateValues + ui32ClientUpdateCount;
			*pui32TimelineUpdateWp = ui32FenceTimelineUpdateValue;
			ui32ClientUpdateCount++;
			/* Now make sure paui32ClientUpdateValue points to pui32IntAllocatedUpdateValues */
			paui32ClientUpdateValue = pui32IntAllocatedUpdateValues;
#if defined(KICKSYNC_CHECKPOINT_DEBUG)
			{
				IMG_UINT32 iii;
				IMG_UINT32 *pui32Tmp = (IMG_UINT32*)pui32IntAllocatedUpdateValues;

				for (iii=0; iii<ui32ClientUpdateCount; iii++)
				{
					CHKPT_DBG((PVR_DBG_ERROR, "%s: pui32IntAllocatedUpdateValues[%d](<%p>) = 0x%x", __FUNCTION__, iii, (void*)pui32Tmp, *pui32Tmp));
					pui32Tmp++;
				}
			}
#endif
			/* Now append the timeline sync prim addr to the kicksync context update list */
			SyncAddrListAppendSyncPrim(&psKickSyncContext->sSyncAddrListUpdate,
			                           psFenceTimelineUpdateSync);
		}
	}

	if (ui32FenceSyncCheckpointCount > 0)
	{
		/* Append the checks (from input fence) */
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append %d sync checkpoints to KickSync Fence (&psKickSyncContext->sSyncAddrListFence=<%p>)...", __FUNCTION__, ui32FenceSyncCheckpointCount, (void*)&psKickSyncContext->sSyncAddrListFence));
		SyncAddrListAppendCheckpoints(&psKickSyncContext->sSyncAddrListFence,
									  ui32FenceSyncCheckpointCount,
									  apsFenceSyncCheckpoints);
		if (!pauiClientFenceUFOAddress)
		{
			pauiClientFenceUFOAddress = psKickSyncContext->sSyncAddrListFence.pasFWAddrs;
		}
		ui32ClientFenceCount += ui32FenceSyncCheckpointCount;
#if defined(KICKSYNC_CHECKPOINT_DEBUG)
			{
				IMG_UINT32 iii;
				IMG_UINT32 *pui32Tmp = (IMG_UINT32*)pauiClientFenceUFOAddress;

				for (iii=0; iii<ui32ClientFenceCount; iii++)
				{
					CHKPT_DBG((PVR_DBG_ERROR, "%s: pauiClientFenceUFOAddress[%d](<%p>) = 0x%x", __FUNCTION__, iii, (void*)pui32Tmp, *pui32Tmp));
					pui32Tmp++;
				}
			}
#endif
	}

	if (psUpdateSyncCheckpoint)
	{
		PVRSRV_ERROR eErr;

		/* Append the update (from output fence) */
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append 1 sync checkpoint to KickSync Update (&psKickSyncContext->sSyncAddrListUpdate=<%p>)...", __FUNCTION__, (void*)&psKickSyncContext->sSyncAddrListUpdate));
		eErr = SyncAddrListAppendCheckpoints(&psKickSyncContext->sSyncAddrListUpdate,
											 1,
											 &psUpdateSyncCheckpoint);
		if (eErr != PVRSRV_OK)
		{
			CHKPT_DBG((PVR_DBG_ERROR, "%s:  ...done. SyncAddrListAppendCheckpoints() returned error (%d)", __FUNCTION__, eErr));
		}
		else
		{
			CHKPT_DBG((PVR_DBG_ERROR, "%s:  ...done.", __FUNCTION__));
		}
		if (!pauiClientUpdateUFOAddress)
		{
			pauiClientUpdateUFOAddress = psKickSyncContext->sSyncAddrListUpdate.pasFWAddrs;
		}
		ui32ClientUpdateCount++;
#if defined(KICKSYNC_CHECKPOINT_DEBUG)
		{
			IMG_UINT32 iii;
			IMG_UINT32 *pui32Tmp = (IMG_UINT32*)pauiClientUpdateUFOAddress;

			for (iii=0; iii<ui32ClientUpdateCount; iii++)
			{
				CHKPT_DBG((PVR_DBG_ERROR, "%s: pauiClientUpdateUFOAddress[%d](<%p>) = 0x%x", __FUNCTION__, iii, (void*)pui32Tmp, *pui32Tmp));
				pui32Tmp++;
			}
		}
#endif
	}
#endif /* defined(PVR_USE_FENCE_SYNC_MODEL) */

#if (ENABLE_KICKSYNC_UFO_DUMP == 1)
		PVR_DPF((PVR_DBG_ERROR, "%s: dumping KICKSYNC fence/updates syncs...", __FUNCTION__));
		{
			IMG_UINT32 ii;
			PRGXFWIF_UFO_ADDR *psTmpIntFenceUFOAddress = pauiClientFenceUFOAddress;
			IMG_UINT32 *pui32TmpIntFenceValue = paui32ClientFenceValue;
			PRGXFWIF_UFO_ADDR *psTmpIntUpdateUFOAddress = pauiClientUpdateUFOAddress;
			IMG_UINT32 *pui32TmpIntUpdateValue = paui32ClientUpdateValue;

			/* Dump Fence syncs and Update syncs */
			PVR_DPF((PVR_DBG_ERROR, "%s: Prepared %d KickSync fence syncs (&psKickSyncContext->sSyncAddrListFence=<%p>, pauiClientFenceUFOAddress=<%p>):", __FUNCTION__, ui32ClientFenceCount, (void*)&psKickSyncContext->sSyncAddrListFence, (void*)pauiClientFenceUFOAddress));
			for (ii=0; ii<ui32ClientFenceCount; ii++)
			{
				if (psTmpIntFenceUFOAddress->ui32Addr & 0x1)
				{
					PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, CheckValue=PVRSRV_SYNC_CHECKPOINT_SIGNALLED", __FUNCTION__, ii+1, ui32ClientFenceCount, (void*)psTmpIntFenceUFOAddress, psTmpIntFenceUFOAddress->ui32Addr));
				}
				else
				{
					PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, CheckValue=%d(0x%x)", __FUNCTION__, ii+1, ui32ClientFenceCount, (void*)psTmpIntFenceUFOAddress, psTmpIntFenceUFOAddress->ui32Addr, *pui32TmpIntFenceValue, *pui32TmpIntFenceValue));
					pui32TmpIntFenceValue++;
				}
				psTmpIntFenceUFOAddress++;
			}
			PVR_DPF((PVR_DBG_ERROR, "%s: Prepared %d KickSync update syncs (&psKickSyncContext->sSyncAddrListUpdate=<%p>, pauiClientUpdateUFOAddress=<%p>):", __FUNCTION__, ui32ClientUpdateCount, (void*)&psKickSyncContext->sSyncAddrListUpdate, (void*)pauiClientUpdateUFOAddress));
			for (ii=0; ii<ui32ClientUpdateCount; ii++)
			{
				CHKPT_DBG((PVR_DBG_ERROR, "%s:  Line %d, psTmpIntUpdateUFOAddress=<%p>", __FUNCTION__, __LINE__, (void*)psTmpIntUpdateUFOAddress));
				CHKPT_DBG((PVR_DBG_ERROR, "%s:  Line %d, pui32TmpIntUpdateValue=<%p>", __FUNCTION__, __LINE__, (void*)pui32TmpIntUpdateValue));
				if (psTmpIntUpdateUFOAddress->ui32Addr & 0x1)
				{
					PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, UpdateValue=PVRSRV_SYNC_CHECKPOINT_SIGNALLED", __FUNCTION__, ii+1, ui32ClientUpdateCount, (void*)psTmpIntUpdateUFOAddress, psTmpIntUpdateUFOAddress->ui32Addr));
				}
				else
				{
					PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, UpdateValue=%d", __FUNCTION__, ii+1, ui32ClientUpdateCount, (void*)psTmpIntUpdateUFOAddress, psTmpIntUpdateUFOAddress->ui32Addr, *pui32TmpIntUpdateValue));
					pui32TmpIntUpdateValue++;
				}
				psTmpIntUpdateUFOAddress++;
			}
		}
#endif

	eError = RGXCmdHelperInitCmdCCB(FWCommonContextGetClientCCB(psKickSyncContext->psServerCommonContext),
	                                ui32ClientFenceCount,
	                                pauiClientFenceUFOAddress,
	                                paui32ClientFenceValue,
	                                ui32ClientUpdateCount,
	                                pauiClientUpdateUFOAddress,
	                                paui32ClientUpdateValue,
	                                ui32ServerSyncPrims,
	                                paui32ServerSyncFlags,
	                                SYNC_FLAG_MASK_ALL,
	                                pasServerSyncs,
	                                0,
	                                NULL,
	                                NULL,
	                                NULL,
	                                NULL,
	                                RGXFWIF_CCB_CMD_TYPE_NULL,
	                                ui32ExtJobRef,
	                                ui32IntJobRef,
	                                PDUMP_FLAGS_NONE,
	                                NULL,
	                                "KickSync",
	                                bCCBStateOpen,
	                                asCmdHelperData,
									sRobustnessResetReason);
	if (eError != PVRSRV_OK)
	{
		goto fail_cmdinit;
	}

	eError = RGXCmdHelperAcquireCmdCCB(ARRAY_SIZE(asCmdHelperData), asCmdHelperData);
	if (eError != PVRSRV_OK)
	{
		goto fail_cmdaquire;
	}

	/*
	 *  We should reserve space in the kernel CCB here and fill in the command
	 *  directly.
	 *  This is so if there isn't space in the kernel CCB we can return with
	 *  retry back to services client before we take any operations
	 */

	/*
	 * We might only be kicking for flush out a padding packet so only submit
	 * the command if the create was successful
	 */
	if (eError == PVRSRV_OK)
	{
		/*
		 * All the required resources are ready at this point, we can't fail so
		 * take the required server sync operations and commit all the resources
		 */
		RGXCmdHelperReleaseCmdCCB(1,
		                          asCmdHelperData,
		                          "KickSync",
		                          FWCommonContextGetFWAddress(psKickSyncContext->psServerCommonContext).ui32Addr);
	}

	/* Construct the kernel kicksync CCB command. */
	sKickSyncKCCBCmd.eCmdType = RGXFWIF_KCCB_CMD_KICK;
	sKickSyncKCCBCmd.uCmdData.sCmdKickData.psContext = FWCommonContextGetFWAddress(psKickSyncContext->psServerCommonContext);
	sKickSyncKCCBCmd.uCmdData.sCmdKickData.ui32CWoffUpdate = RGXGetHostWriteOffsetCCB(FWCommonContextGetClientCCB(psKickSyncContext->psServerCommonContext));
	sKickSyncKCCBCmd.uCmdData.sCmdKickData.ui32NumCleanupCtl = 0;
	sKickSyncKCCBCmd.uCmdData.sCmdKickData.ui32WorkEstCmdHeaderOffset = 0;
	
	/*
	 * Submit the kicksync command to the firmware.
	 */
	RGX_HWPERF_HOST_ENQ(psKickSyncContext,
	                    OSGetCurrentClientProcessIDKM(),
	                    ui32FWCtx,
	                    ui32ExtJobRef,
	                    ui32IntJobRef,
	                    RGX_HWPERF_KICK_TYPE_SYNC,
	                    uiCheckFenceUID,
	                    uiUpdateFenceUID,
	                    NO_DEADLINE,
	                    NO_CYCEST);

	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{
		eError2 = RGXScheduleCommand(psKickSyncContext->psDeviceNode->pvDevice,
		                             RGXFWIF_DM_GP,
		                             & sKickSyncKCCBCmd,
		                             sizeof(sKickSyncKCCBCmd),
		                             ui32ClientCacheOpSeqNum,
		                             PDUMP_FLAGS_NONE);
		if (eError2 != PVRSRV_ERROR_RETRY)
		{
			break;
		}
		OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
	} END_LOOP_UNTIL_TIMEOUT();

#if defined(SUPPORT_GPUTRACE_EVENTS)
		RGXHWPerfFTraceGPUEnqueueEvent(psKickSyncContext->psDeviceNode->pvDevice,
					ui32FWCtx, ui32IntJobRef, RGX_HWPERF_KICK_TYPE_SYNC);
#endif

	if (eError2 != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "PVRSRVRGXKickSync failed to schedule kernel CCB command. (0x%x)",
		         eError));
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
#endif
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

	*piUpdateFence = iUpdateFence;
#if defined(PVR_USE_FENCE_SYNC_MODEL)
	if (pvUpdateFenceFinaliseData && (iUpdateFence != PVRSRV_NO_FENCE))
	{
		SyncCheckpointFinaliseFence(iUpdateFence, pvUpdateFenceFinaliseData);
	}
#endif /* defined(PVR_USE_FENCE_SYNC_MODEL) */

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockRelease(psKickSyncContext->hLock);
#endif
	return PVRSRV_OK;

fail_cmdaquire:
fail_cmdinit:
#if defined(PVR_USE_FENCE_SYNC_MODEL)
	SyncAddrListRollbackCheckpoints(psKickSyncContext->psDeviceNode, &psKickSyncContext->sSyncAddrListFence);
	SyncAddrListRollbackCheckpoints(psKickSyncContext->psDeviceNode, &psKickSyncContext->sSyncAddrListUpdate);
	if(iUpdateFence != PVRSRV_NO_FENCE)
	{
		SyncCheckpointRollbackFenceData(iUpdateFence, pvUpdateFenceFinaliseData);
	}

	/* Free memory allocated to hold update values */
	if (pui32IntAllocatedUpdateValues)
	{
		OSFreeMem(pui32IntAllocatedUpdateValues);
	}
fail_alloc_update_values_mem:
fail_create_output_fence:
	/* Drop the references taken on the sync checkpoints in the
	 * resolved input fence */
	SyncAddrListDeRefCheckpoints(ui32FenceSyncCheckpointCount,
								 apsFenceSyncCheckpoints);
	/* Free memory allocated to hold the resolved fence's checkpoints */
	if (apsFenceSyncCheckpoints)
	{
		SyncCheckpointFreeCheckpointListMem(apsFenceSyncCheckpoints);
	}
fail_resolve_fence:
#endif /* defined(PVR_USE_FENCE_SYNC_MODEL) */
fail_syncaddrlist:
out_unlock:
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockRelease(psKickSyncContext->hLock);
#endif
	return eError;
}	


/**************************************************************************//**
 End of file (rgxkicksync.c)
******************************************************************************/
